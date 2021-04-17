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
    int payloadLength = argm["l"].empty() ? 16 : std::stoi(argm["l"]);

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

    int fd = 1;
    if (ioctl(fd, KDMKTONE, 0)) {
        fd = open(CONSOLE, O_RDONLY);
    }
    if (fd < 0) {
        perror(CONSOLE);
        fprintf(stderr, "This program must be run as root\n");
        return 1;
    }

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

    ggWave.init(message.size(), message.data(), protocols.at(GGWave::TxProtocolId(txProtocolId)), 10);

    GGWave::CBWaveformOut tmp = [](const void * , uint32_t ){};
    ggWave.encode(tmp);

    int nFrames = 0;
    float lastF = -1.0f;

    auto tones = ggWave.getWaveformTones();
    for (auto & tonesCur : tones) {
        if (tonesCur.size() == 0) continue;
        const auto & tone = tonesCur.front();
        if (tone.freq_hz != lastF) {
            if (nFrames > 0) {
                long pitch = std::round(1193180.0/lastF);
                long ms = std::round(nFrames*tone.duration_ms);
                ioctl(fd, KDMKTONE, (ms<<16)|pitch);
                usleep(ms*1000);
            }
            nFrames = 0;
            lastF = tone.freq_hz;
        }
        ++nFrames;
    }

    if (nFrames > 0) {
        const auto & tone = tones.front().front();
        long pitch = std::round(1193180.0/lastF);
        long ms = std::round(nFrames*tone.duration_ms);
        ioctl(fd, KDMKTONE, (ms<<16)|pitch);
        usleep(ms*1000);
    }

    return 0;
}
