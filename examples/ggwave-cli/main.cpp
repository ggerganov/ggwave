#include "ggwave/ggwave.h"

#include "ggwave-common.h"
#include "ggwave-common-sdl2.h"

#include <cstdio>
#include <string>

#include <mutex>
#include <thread>
#include <iostream>

int main(int argc, char** argv) {
    printf("Usage: %s [-cN] [-pN] [-tN]\n", argv[0]);
    printf("    -cN - select capture device N\n");
    printf("    -pN - select playback device N\n");
    printf("    -tN - transmission protocol\n");
    printf("\n");

    auto argm = parseCmdArguments(argc, argv);
    int captureId = argm["c"].empty() ? 0 : std::stoi(argm["c"]);
    int playbackId = argm["p"].empty() ? 0 : std::stoi(argm["p"]);
    int txProtocol = argm["t"].empty() ? 1 : std::stoi(argm["t"]);

    if (GGWave_init(playbackId, captureId) == false) {
        fprintf(stderr, "Failed to initialize GGWave\n");
        return -1;
    }

    auto ggWave = GGWave_instance();

    printf("Available Tx protocols:\n");
    for (int i = 0; i < (int) ggWave->getTxProtocols().size(); ++i) {
        printf("    -t%d : %s\n", i, ggWave->getTxProtocols()[i].name);
    }

    if (txProtocol < 0 || txProtocol > (int) ggWave->getTxProtocols().size()) {
        fprintf(stderr, "Unknown Tx protocol %d\n", txProtocol);
        return -3;
    }

    printf("Selecting Tx protocol %d\n", txProtocol);

    std::mutex mutex;
    std::thread inputThread([&]() {
        std::string inputOld = "";
        while (true) {
            std::string input;
            std::cout << "Enter text: ";
            getline(std::cin, input);
            if (input.empty()) {
                std::cout << "Re-sending ... " << std::endl;
                input = inputOld;
            } else {
                std::cout << "Sending ... " << std::endl;
            }
            {
                std::lock_guard<std::mutex> lock(mutex);
                ggWave->init(input.size(), input.data(), ggWave->getTxProtocols()[txProtocol], 50);
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

    return 0;
}
