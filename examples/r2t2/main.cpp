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

void processTone(int fd, double freq_hz, long duration_ms, bool useBeep, bool printTones, bool printArduino) {
    if (printTones) {
        printf("TONE %8.2f Hz %5ld ms\n", freq_hz, duration_ms);
        return;
    }

    if (printArduino) {
        printf("tone(kPinSpeaker, %8.2f); delay(%4ld);\n", freq_hz, duration_ms);
        return;
    }

    if (useBeep) {
        static char cmd[128];
        snprintf(cmd, 128, "beep -f %g -l %ld", freq_hz, duration_ms);
        int ret = system(cmd);
        if (ret != 0) {
            printf("system(\"%s\") failed with %d\n", cmd, ret);
        }
        return;
    }

    long pitch = std::round(1193180.0/freq_hz);
    long ms = std::round(duration_ms);
    ioctl(fd, KDMKTONE, (ms<<16)|pitch);
    usleep(ms*1000);
}

int main(int argc, char** argv) {
    printf("Usage: %s [-p] [-b] [-tN] [-lN]\n", argv[0]);
    printf("    -p  - print tones, no playback\n");
    printf("    -A  - print Arduino code\n");
    printf("    -b  - use 'beep' command\n");
    printf("    -s  - use Direct Sequence Spread (DSS)\n");
    printf("    -tN - transmission protocol\n");
    printf("    -lN - fixed payload length of size N, N in [1, %d]\n", GGWave::kMaxLengthFixed);
    printf("\n");

    auto & protocols = GGWave::Protocols::tx();
    protocols = { {
        { "[R2T2] Normal",      64,  9, 1, 2, true, },
        { "[R2T2] Fast",        64,  6, 1, 2, true, },
        { "[R2T2] Fastest",     64,  3, 1, 2, true, },
        { "[R2T2] Low Normal",  16,  9, 1, 2, true, },
        { "[R2T2] Low Fast",    16,  6, 1, 2, true, },
        { "[R2T2] Low Fastest", 16,  3, 1, 2, true, },
    } };

    const auto argm         = parseCmdArguments(argc, argv);
    const bool printTones   = argm.count("p") > 0;
    const bool printArduino = argm.count("A") > 0;
    const bool useBeep      = argm.count("b") > 0;
    const bool useDSS       = argm.count("s") > 0;
    const int txProtocolId  = argm.count("t") == 0 ? 0 : std::stoi(argm.at("t"));
    const int payloadLength = argm.count("l") == 0 ? 16 : std::stoi(argm.at("l"));

    GGWave::OperatingMode mode = GGWAVE_OPERATING_MODE_TX | GGWAVE_OPERATING_MODE_TX_ONLY_TONES;
    if (useDSS) mode |= GGWAVE_OPERATING_MODE_USE_DSS;

    GGWave ggWave({
        payloadLength,
        GGWave::kDefaultSampleRate,
        GGWave::kDefaultSampleRate,
        GGWave::kDefaultSampleRate,
        GGWave::kDefaultSamplesPerFrame,
        GGWave::kDefaultSoundMarkerThreshold,
        GGWAVE_SAMPLE_FORMAT_F32,
        GGWAVE_SAMPLE_FORMAT_F32,
        mode,
    });

    printf("Available Tx protocols:\n");
    for (int i = 0; i < (int) protocols.size(); ++i) {
        const auto & protocol = protocols[i];
        if (protocol.enabled && protocol.name) {
            printf("    -t%-2d : %-16s\n", i, protocol.name);
        }
    }
    printf("\n");

    if (txProtocolId < 0 || txProtocolId >= (int) protocols.size()) {
        fprintf(stderr, "Unknown Tx protocol %d\n", txProtocolId);
        return -3;
    }

    printf("Selecting Tx protocol %d\n", txProtocolId);

    int fd = 1;
    if (useBeep == false && printTones == false && printArduino == false) {
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

    ggWave.init(message.size(), message.data(), GGWave::TxProtocolId(txProtocolId), 10);
    ggWave.encode();

    const auto & protocol = protocols[txProtocolId];
    const auto tones = ggWave.txTones();
    const auto duration_ms = protocol.txDuration_ms(ggWave.samplesPerFrame(), ggWave.sampleRateOut());
    for (auto & tone : tones) {
        const auto freq_hz = (protocol.freqStart + tone)*ggWave.hzPerSample();
        processTone(fd, freq_hz, duration_ms, useBeep, printTones, printArduino);
    }

    return 0;
}
