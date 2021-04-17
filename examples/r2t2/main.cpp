#include "ggwave/ggwave.h"

#include "ggwave-common.h"

#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <linux/kd.h>

#define CONSOLE "/dev/tty0"

#include <cmath>
#include <cstdio>
#include <string>
#include <iostream>

int main(int argc, char** argv) {
    printf("Usage: %s [-tN] [-lN]\n", argv[0]);
    printf("    -tN - transmission protocol\n");
    printf("    -lN - fixed payload length of size N, N in [1, %d]\n", GGWave::kMaxLengthFixed);
    printf("\n");

    const GGWave::TxProtocols protocols = {
        { GGWAVE_TX_PROTOCOL_CUSTOM_0, { "[R2T2] Normal",  64,  9, 1, } },
        { GGWAVE_TX_PROTOCOL_CUSTOM_1, { "[R2T2] Fast",    64,  6, 1, } },
        { GGWAVE_TX_PROTOCOL_CUSTOM_2, { "[R2T2] Fastest", 64,  3, 1, } },
    };

    auto argm = parseCmdArguments(argc, argv);
    int txProtocolId = argm["t"].empty() ? GGWAVE_TX_PROTOCOL_CUSTOM_0 : std::stoi(argm["t"]);
    int payloadLength = argm["l"].empty() ? 4 : std::stoi(argm["l"]);

    GGWave ggWave({
        payloadLength,
        GGWave::kBaseSampleRate,
        GGWave::kBaseSampleRate,
        GGWave::kDefaultSamplesPerFrame,
        GGWave::kDefaultSoundMarkerThreshold,
        GGWAVE_SAMPLE_FORMAT_F32,
        GGWAVE_SAMPLE_FORMAT_F32});

    printf("Available Tx protocols:\n");
    for (const auto & protocol : protocols) {
        printf("    -t%d : %s\n", protocol.first, protocol.second.name);
    }

    if (protocols.find(GGWave::TxProtocolId(txProtocolId)) == protocols.end()) {
        fprintf(stderr, "Unknown Tx protocol %d\n", txProtocolId);
        return -3;
    }

    printf("Selecting Tx protocol %d\n", txProtocolId);

    fprintf(stderr, "Enter a text message:\n");

    std::string message;
    std::getline(std::cin, message);

    if (message.size() == 0) {
        fprintf(stderr, "Invalid message: size = 0\n");
        return -2;
    }

    if ((int) message.size() > payloadLength) {
        fprintf(stderr, "Invalid message: size > %d\n", payloadLength);
        return -3;
    }

    int fd = 1;
    if (ioctl(fd, KDMKTONE, 0)) {
        fd = open(CONSOLE, O_RDONLY);
    }
    if (fd < 0) {
        perror(CONSOLE);
        return 1;
    }

    ggWave.init(message.size(), message.data(), protocols.at(GGWave::TxProtocolId(txProtocolId)), 10);

    GGWave::CBWaveformOut tmp = [](const void * , uint32_t ){};
    ggWave.encode(tmp);

    auto tones = ggWave.getWaveformTones();
    for (auto & tonesCur : tones) {
        for (auto & tone : tonesCur) {
            long pitch   = std::round(1193180.0/tone.freq_hz);
            long ms = std::round(tone.duration_ms);
            ioctl(fd, KDMKTONE, (ms<<16)|pitch);
            usleep(ms*1000);
        }
    }

    return 0;
}
