/*! \file main.cpp
 *  \brief Send/Receive data through sound in the terminal
 *  \author Georgi Gerganov
 */

#include "ggwave/ggwave.h"

#include "ggwave-common.h"
#include "ggwave-common-sdl2.h"

#include <SDL.h>
#include <SDL_audio.h>

#include <cstdio>
#include <string>
#include <chrono>

#include <mutex>
#include <thread>
#include <iostream>

static char *g_defaultCaptureDeviceName = nullptr;

static int g_captureId = -1;
static int g_playbackId = -1;

static bool g_isInitialized = false;

static SDL_AudioDeviceID g_devIdIn = 0;
static SDL_AudioDeviceID g_devIdOut = 0;

static GGWave *g_ggWave = nullptr;

// main loop
void update() {
    if (g_isInitialized == false) return;

    SDL_Event e;
    SDL_bool shouldTerminate = SDL_FALSE;
    while (SDL_PollEvent(&e)) {
        if (e.type == SDL_QUIT) {
            shouldTerminate = SDL_TRUE;
        }
    }

    static GGWave::CBQueueAudio cbQueueAudio = [&](const void * data, uint32_t nBytes) {
        SDL_QueueAudio(g_devIdOut, data, nBytes);
    };

    static GGWave::CBDequeueAudio CBDequeueAudio = [&](void * data, uint32_t nMaxBytes) {
        return SDL_DequeueAudio(g_devIdIn, data, nMaxBytes);
    };

    if (g_ggWave->getHasData() == false) {
        SDL_PauseAudioDevice(g_devIdOut, SDL_FALSE);

        static auto tLastNoData = std::chrono::high_resolution_clock::now();
        auto tNow = std::chrono::high_resolution_clock::now();

        if ((int) SDL_GetQueuedAudioSize(g_devIdOut) < g_ggWave->getSamplesPerFrame()*g_ggWave->getSampleSizeBytesOut()) {
            SDL_PauseAudioDevice(g_devIdIn, SDL_FALSE);
            if (::getTime_ms(tLastNoData, tNow) > 500.0f) {
                g_ggWave->receive(CBDequeueAudio);
                if ((int) SDL_GetQueuedAudioSize(g_devIdIn) > 32*g_ggWave->getSamplesPerFrame()*g_ggWave->getSampleSizeBytesIn()) {
                    SDL_ClearQueuedAudio(g_devIdIn);
                }
            } else {
                SDL_ClearQueuedAudio(g_devIdIn);
            }
        } else {
            tLastNoData = tNow;
            //SDL_ClearQueuedAudio(g_devIdIn);
            //SDL_Delay(10);
        }
    } else {
        SDL_PauseAudioDevice(g_devIdOut, SDL_TRUE);
        SDL_PauseAudioDevice(g_devIdIn, SDL_TRUE);

        g_ggWave->send(cbQueueAudio);
    }

    if (shouldTerminate) {
        SDL_PauseAudioDevice(g_devIdIn, 1);
        SDL_CloseAudioDevice(g_devIdIn);
        SDL_PauseAudioDevice(g_devIdOut, 1);
        SDL_CloseAudioDevice(g_devIdOut);
        SDL_CloseAudio();
        SDL_Quit();
    }
}

int main(int argc, char** argv) {
    printf("Usage: %s [-cN] [-pN] [-tN]\n", argv[0]);
    printf("    -cN - select capture device N\n");
    printf("    -pN - select playback device N\n");
    printf("    -tN - transmission protocol:\n");
    printf("          -t0 : Normal\n");
    printf("          -t1 : Fast (default)\n");
    printf("          -t2 : Fastest\n");
    printf("          -t3 : Ultrasonic\n");
    printf("\n");

    g_defaultCaptureDeviceName = nullptr;

    auto argm = parseCmdArguments(argc, argv);
    g_captureId = argm["c"].empty() ? 0 : std::stoi(argm["c"]);
    g_playbackId = argm["p"].empty() ? 0 : std::stoi(argm["p"]);
    int txProtocol = argm["t"].empty() ? 1 : std::stoi(argm["t"]);

    initSDL2ForGGWave(
            g_isInitialized,
            g_playbackId,
            g_devIdIn,
            g_captureId,
            g_devIdOut,
            g_ggWave);

    g_ggWave->setTxMode(GGWave::TxMode::VariableLength);

    printf("Selecting Tx protocol %d\n", txProtocol);
    switch (txProtocol) {
        case 0:
            {
                printf("Using 'Normal' Tx Protocol\n");
                g_ggWave->setParameters(1, 40, 9, 3, 50);
            }
            break;
        case 1:
            {
                printf("Using 'Fast' Tx Protocol\n");
                g_ggWave->setParameters(1, 40, 6, 3, 50);
            }
            break;
        case 2:
            {
                printf("Using 'Fastest' Tx Protocol\n");
                g_ggWave->setParameters(1, 40, 3, 3, 50);
            }
            break;
        case 3:
            {
                printf("Using 'Ultrasonic' Tx Protocol\n");
                g_ggWave->setParameters(1, 320, 9, 3, 50);
            }
            break;
        default:
            {
                printf("Using 'Fast' Tx Protocol\n");
                g_ggWave->setParameters(1, 40, 6, 3, 50);
            }
    };
    printf("\n");

    g_ggWave->init(0, "");

    std::mutex mutex;
    std::thread inputThread([&mutex]() {
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
                g_ggWave->init(input.size(), input.data());
            }
            inputOld = input;
        }
    });

    while (true) {
        SDL_Delay(1);
        {
            std::lock_guard<std::mutex> lock(mutex);
            update();
        }
    }

    inputThread.join();

    delete g_ggWave;

    SDL_PauseAudioDevice(g_devIdIn, 1);
    SDL_CloseAudioDevice(g_devIdIn);
    SDL_PauseAudioDevice(g_devIdOut, 1);
    SDL_CloseAudioDevice(g_devIdOut);
    SDL_CloseAudio();
    SDL_Quit();

    return 0;
}
