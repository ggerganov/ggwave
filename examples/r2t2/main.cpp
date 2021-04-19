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

void processTone(int fd, double freq_hz, long duration_ms, bool useBeep, bool printTones) {
    if (printTones) {
        printf("TONE %8.2f Hz %5ld ms\n", freq_hz, duration_ms);
        return;
    }

    if (useBeep) {
        static char cmd[128];
        snprintf(cmd, 128, "beep -f %g -l %ld", freq_hz, duration_ms);
        system(cmd);
        return;
    }

    long pitch = std::round(1193180.0/freq_hz);
    long ms = std::round(duration_ms);
    ioctl(fd, KDMKTONE, (ms<<16)|pitch);
    usleep(ms*1000);
}

int main(int argc, char** argv) {
    printf("Usage: %s [-tN] [-lN]\n", argv[0]);
    printf("    -p  - print tones, no playback\n");
    //printf("    -b  - use 'beep' command\n");
    printf("    -tN - transmission protocol\n");
    printf("    -lN - fixed payload length of size N, N in [1, %d]\n", GGWave::kMaxLengthFixed);
    printf("\n");

    const GGWave::TxProtocols protocols = {
        { GGWAVE_TX_PROTOCOL_CUSTOM_0, { "[R2T2] Normal",  64,  9, 1, } },
        { GGWAVE_TX_PROTOCOL_CUSTOM_1, { "[R2T2] Fast",    64,  6, 1, } },
        { GGWAVE_TX_PROTOCOL_CUSTOM_2, { "[R2T2] Fastest", 64,  3, 1, } },
    };

    auto argm = parseCmdArguments(argc, argv);
    bool printTones = argm.find("p") != argm.end();
    bool useBeep = false; //argm.find("b") != argm.end();
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
        printf("    -t%-2d : %-16s", protocol.first, protocol.second.name);
        if (protocol.first == GGWAVE_TX_PROTOCOL_CUSTOM_0) {
            printf(" (8.5 sec)\n");
        } else if (protocol.first == GGWAVE_TX_PROTOCOL_CUSTOM_1) {
            printf(" (5.7 sec)\n");
        } else if (protocol.first == GGWAVE_TX_PROTOCOL_CUSTOM_2) {
            printf(" (2.9 sec)\n");
        } else {
            printf("\n");
        }
    }
    printf("\n");

    if (protocols.find(GGWave::TxProtocolId(txProtocolId)) == protocols.end()) {
        fprintf(stderr, "Unknown Tx protocol %d\n", txProtocolId);
        return -3;
    }

    printf("Selecting Tx protocol %d\n", txProtocolId);

    int fd = 1;
    if (useBeep == false && printTones == false) {
        if (ioctl(fd, KDMKTONE, 0)) {
            fd = open(CONSOLE, O_RDONLY);
        }
        if (fd < 0) {
            perror(CONSOLE);
            fprintf(stderr, "This program must be run as root\n");
            return 1;
        }
    }

    fprintf(stderr, "Enter a text message:\n");

    std::string message;
    std::getline(std::cin, message);

    printf("\n");

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
    double lastF = -1.0f;

    auto tones = ggWave.getWaveformTones();
    for (auto & tonesCur : tones) {
        if (tonesCur.size() == 0) continue;
        const auto & tone = tonesCur.front();
        if (tone.freq_hz != lastF) {
            if (nFrames > 0) {
                processTone(fd, lastF, nFrames*tone.duration_ms, useBeep, printTones);
            }
            nFrames = 0;
            lastF = tone.freq_hz;
        }
        ++nFrames;
    }

    if (nFrames > 0) {
        const auto & tone = tones.front().front();
        processTone(fd, lastF, nFrames*tone.duration_ms, useBeep, printTones);
    }

    return 0;
}
