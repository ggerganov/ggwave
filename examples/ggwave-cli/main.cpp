#include "ggwave/ggwave.h"

#include "ggwave-common.h"
#include "ggwave-common-sdl2.h"

#include <SDL.h>

#include <cstdio>
#include <string>

#include <mutex>
#include <thread>
#include <iostream>

int main(int argc, char** argv) {
    printf("Usage: %s [-cN] [-pN] [-tN] [-lN]\n", argv[0]);
    printf("    -cN - select capture device N\n");
    printf("    -pN - select playback device N\n");
    printf("    -tN - transmission protocol\n");
    printf("    -lN - fixed payload length of size N, N in [1, %d]\n", GGWave::kMaxLengthFixed);
    printf("    -d  - use Direct Sequence Spread (DSS)\n");
    printf("    -v  - print generated tones on resend\n");
    printf("\n");

    const auto argm          = parseCmdArguments(argc, argv);
    const int  captureId     = argm.count("c") == 0 ?  0 : std::stoi(argm.at("c"));
    const int  playbackId    = argm.count("p") == 0 ?  0 : std::stoi(argm.at("p"));
    const int  txProtocolId  = argm.count("t") == 0 ?  1 : std::stoi(argm.at("t"));
    const int  payloadLength = argm.count("l") == 0 ? -1 : std::stoi(argm.at("l"));
    const bool useDSS        = argm.count("d") >  0;
    const bool printTones    = argm.count("v") >  0;

    if (GGWave_init(playbackId, captureId, payloadLength, 0.0f, useDSS) == false) {
        fprintf(stderr, "Failed to initialize GGWave\n");
        return -1;
    }

    auto ggWave = GGWave_instance();

    printf("Available Tx protocols:\n");
    const auto & protocols = GGWave::Protocols::kDefault();
    for (int i = 0; i < (int) protocols.size(); ++i) {
        const auto & protocol = protocols[i];
        if (protocol.enabled == false) {
            continue;
        }
        printf("      %d - %s\n", i, protocol.name);
    }

    if (txProtocolId < 0) {
        fprintf(stderr, "Unknown Tx protocol %d\n", txProtocolId);
        return -3;
    }

    printf("Selecting Tx protocol %d\n", txProtocolId);

    std::mutex mutex;
    std::thread inputThread([&]() {
        std::string inputOld = "";
        while (true) {
            std::string input;
            printf("Enter text: ");
            fflush(stdout);
            getline(std::cin, input);
            if (input.empty()) {
                printf("Re-sending ...\n");
                input = inputOld;

                if (printTones) {
                    printf("Printing generated waveform tones (Hz):\n");
                    const auto & protocol = protocols[txProtocolId];
                    const auto tones = ggWave->txTones();
                    for (int i = 0; i < (int) tones.size(); ++i) {
                        if (tones[i] < 0) {
                            printf(" - end tx\n");
                            continue;
                        }
                        const auto freq_hz = (protocol.freqStart + tones[i])*ggWave->hzPerSample();
                        printf(" - tone %3d: %f\n", i, freq_hz);
                    }
                }
            } else {
                printf("Sending ...\n");
            }
            {
                std::lock_guard<std::mutex> lock(mutex);
                ggWave->init(input.size(), input.data(), GGWave::TxProtocolId(txProtocolId), 10);
            }
            inputOld = input;
        }
    });

    while (true) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        {
            std::lock_guard<std::mutex> lock(mutex);
            GGWave_mainLoop();
        }
    }

    inputThread.join();

    GGWave_deinit();

    SDL_CloseAudio();
    SDL_Quit();

    return 0;
}
