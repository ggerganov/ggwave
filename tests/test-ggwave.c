#include "ggwave/ggwave.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(cond) \
    if (!(cond)) { \
        fprintf(stderr, "[%s:%d] Check failed: %s\n", __FILE__, __LINE__, #cond); \
        exit(1); \
    }

#define CHECK_T(cond) CHECK(cond)
#define CHECK_F(cond) CHECK(!(cond))

int main() {
    //ggwave_setLogFile(NULL); // disable logging
    ggwave_setLogFile(stdout);

    ggwave_Parameters parameters = ggwave_getDefaultParameters();
    parameters.sampleFormatInp = GGWAVE_SAMPLE_FORMAT_I16;
    parameters.sampleFormatOut = GGWAVE_SAMPLE_FORMAT_I16;

    ggwave_Instance instance = ggwave_init(parameters);

    int ret;
    const char * payload = "test";
    char decoded[16];

    int n = ggwave_encode(instance, payload, 4, GGWAVE_PROTOCOL_AUDIBLE_FASTEST, 50, NULL, 1);
    char *waveform = malloc(n);
    CHECK(waveform != NULL);

    int ne = ggwave_encode(instance, payload, 4, GGWAVE_PROTOCOL_AUDIBLE_FASTEST, 50, waveform, 0);
    CHECK(ne > 0);

    // not enough output buffer size to store the decoded message
    ret = ggwave_ndecode(instance, waveform, ne, decoded, 3);
    CHECK(ret == -2); // fail

    // just enough size to store it
    ret = ggwave_ndecode(instance, waveform, ne, decoded, 4);
    CHECK(ret == 4); // success

    // unsafe method - will write the decoded output to the output buffer regardless of the size
    ret = ggwave_decode(instance, waveform, ne, decoded);
    CHECK(ret == 4);

    // disable Rx protocol
    {
        ggwave_rxToggleProtocol(GGWAVE_PROTOCOL_AUDIBLE_FASTEST, 0);
        ggwave_Instance instanceTmp = ggwave_init(parameters);

        ret = ggwave_ndecode(instanceTmp, waveform, ne, decoded, 4);
        CHECK(ret == -1); // fail

        ggwave_free(instanceTmp);
    }

    // enable Rx protocol
    {
        ggwave_rxToggleProtocol(GGWAVE_PROTOCOL_AUDIBLE_FASTEST, 1);
        ggwave_Instance instanceTmp = ggwave_init(parameters);

        ret = ggwave_ndecode(instanceTmp, waveform, ne, decoded, 4);
        CHECK(ret == 4); // success

        ggwave_free(instanceTmp);
    }

    decoded[ret] = 0; // null-terminate the received data
    CHECK(strcmp(decoded, payload) == 0);

    ggwave_free(instance);
    free(waveform);

    return 0;
}
