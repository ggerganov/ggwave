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
    printf("    -v  - print generated tones on resend\n");
    printf("\n");

    auto argm = parseCmdArguments(argc, argv);
    int captureId = argm["c"].empty() ? 0 : std::stoi(argm["c"]);
    int playbackId = argm["p"].empty() ? 0 : std::stoi(argm["p"]);
    int txProtocolId = argm["t"].empty() ? 1 : std::stoi(argm["t"]);
    int payloadLength = argm["l"].empty() ? -1 : std::stoi(argm["l"]);
    bool printTones = argm.find("v") == argm.end() ? false : true;

    if (GGWave_init(playbackId, captureId, payloadLength) == false) {
        fprintf(stderr, "Failed to initialize GGWave\n");
        return -1;
    }

    auto ggWave = GGWave_instance();

    printf("Available Tx protocols:\n");
    const auto & protocols = GGWave::getTxProtocols();
    for (const auto & protocol : protocols) {
        printf("    -t%d : %s\n", protocol.first, protocol.second.name);
    }

    if (txProtocolId < 0 || txProtocolId > (int) ggWave->getTxProtocols().size()) {
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
                    auto waveformTones = ggWave->getWaveformTones();
                    for (int i = 0; i < (int) waveformTones.size(); ++i) {
                        printf(" - frame %3d: ", i);
                        for (int j = 0; j < (int) waveformTones[i].size(); ++j) {
                            printf("%8.2f ", waveformTones[i][j].freq_hz);
                        }
                        printf("\n");
                    }
                }
            } else {
                printf("Sending ...\n");
            }
            {
                std::lock_guard<std::mutex> lock(mutex);
                ggWave->init(input.size(), input.data(), ggWave->getTxProtocol(txProtocolId), 10);
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
