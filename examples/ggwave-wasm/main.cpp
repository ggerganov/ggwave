/*! \file main.cpp
 *  \brief Send/Receive - WebAssembly port
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

#include "build_timestamp.h"
#include "emscripten/emscripten.h"

static char *g_defaultCaptureDeviceName = nullptr;

static int g_captureId = -1;
static int g_playbackId = -1;

static bool g_isInitialized = false;

static SDL_AudioDeviceID g_devIdIn = 0;
static SDL_AudioDeviceID g_devIdOut = 0;

static GGWave *g_ggWave = nullptr;

// JS interface
extern "C" {
    EMSCRIPTEN_KEEPALIVE
        int setText(int textLength, const char * text) {
            g_ggWave->init(textLength, text);
            return 0;
        }

    EMSCRIPTEN_KEEPALIVE
    int getText(char * text) {
        std::copy(g_ggWave->getRxData().begin(), g_ggWave->getRxData().end(), text);
        return 0;
    }

    EMSCRIPTEN_KEEPALIVE
    int getSampleRate()             { return g_ggWave->getSampleRateIn(); }

    EMSCRIPTEN_KEEPALIVE
    float getAverageRxTime_ms()     { return g_ggWave->getAverageRxTime_ms(); }

    EMSCRIPTEN_KEEPALIVE
    int getFramesToRecord()         { return g_ggWave->getFramesToRecord(); }

    EMSCRIPTEN_KEEPALIVE
    int getFramesLeftToRecord()     { return g_ggWave->getFramesLeftToRecord(); }

    EMSCRIPTEN_KEEPALIVE
    int getFramesToAnalyze()        { return g_ggWave->getFramesToAnalyze(); }

    EMSCRIPTEN_KEEPALIVE
    int getFramesLeftToAnalyze()    { return g_ggWave->getFramesLeftToAnalyze(); }

    EMSCRIPTEN_KEEPALIVE
    int hasDeviceOutput()           { return g_devIdOut; }

    EMSCRIPTEN_KEEPALIVE
    int hasDeviceCapture()          { return (g_ggWave->getTotalBytesCaptured() > 0) ? g_devIdIn : 0; }

    EMSCRIPTEN_KEEPALIVE
    int doInit()                    {
        return initSDL2ForGGWave(
                g_isInitialized,
                g_playbackId,
                g_devIdIn,
                g_captureId,
                g_devIdOut,
                g_ggWave,
                g_defaultCaptureDeviceName);
    }

    EMSCRIPTEN_KEEPALIVE
    int setTxMode(int txMode) {
        g_ggWave->setTxMode((GGWave::TxMode)(txMode));
        g_ggWave->init(0, "");
        return 0;
    }

    EMSCRIPTEN_KEEPALIVE
    void setParameters(
            int paramFreqDelta,
            int paramFreqStart,
            int paramFramesPerTx,
            int paramBytesPerTx,
            int /*paramECCBytesPerTx*/,
            int paramVolume) {
        if (g_ggWave == nullptr) return;

        g_ggWave->setParameters(
                paramFreqDelta,
                paramFreqStart,
                paramFramesPerTx,
                paramBytesPerTx,
                paramVolume);

        g_ggWave->init(0, "");
    }
}

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
        #ifdef __EMSCRIPTEN__
        emscripten_cancel_main_loop();
        #endif
    }
}

int main(int , char** argv) {
    printf("Build time: %s\n", BUILD_TIMESTAMP);
    printf("Press the Init button to start\n");

    g_defaultCaptureDeviceName = argv[1];

    emscripten_set_main_loop(update, 60, 1);

    return 0;
}
