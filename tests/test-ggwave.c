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
    ggwave_Parameters parameters = ggwave_getDefaultParameters();
    parameters.sampleFormatInp = GGWAVE_SAMPLE_FORMAT_I16;
    parameters.sampleFormatOut = GGWAVE_SAMPLE_FORMAT_I16;

    ggwave_Instance instance = ggwave_init(parameters);

    int ret;
    const char * payload = "test";
    char decoded[256];

    int n = ggwave_encode(instance, payload, 4, GGWAVE_TX_PROTOCOL_AUDIBLE_FAST, 50, NULL, 1);
    char waveform[n];

    ret = ggwave_encode(instance, payload, 4, GGWAVE_TX_PROTOCOL_AUDIBLE_FAST, 50, waveform, 0);
    CHECK(ret > 0);

    ret = ggwave_decode(instance, waveform, sizeof(signed short)*ret, decoded);
    CHECK(ret == 4);

    CHECK(strcmp(decoded, payload) == 0);

    ggwave_free(instance);

    return 0;
}
