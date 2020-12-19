#include "ggwave-common-sdl2.h"

#include "ggwave-common.h"

#include "ggwave/ggwave.h"

#include <SDL.h>
#include <SDL_opengl.h>

#include <chrono>

#ifdef __EMSCRIPTEN__
#include "emscripten/emscripten.h"
#else
#define EMSCRIPTEN_KEEPALIVE
#endif

namespace {

std::string g_defaultCaptureDeviceName = "";

SDL_AudioDeviceID g_devIdIn = 0;
SDL_AudioDeviceID g_devIdOut = 0;

SDL_AudioSpec g_obtainedSpecIn;
SDL_AudioSpec g_obtainedSpecOut;

GGWave *g_ggWave = nullptr;

}

// JS interface

extern "C" {
    EMSCRIPTEN_KEEPALIVE
        int sendData(int textLength, const char * text, int protocolId, int volume) {
            g_ggWave->init(textLength, text, g_ggWave->getTxProtocols()[protocolId], volume);
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
        int hasDeviceCapture()          { return g_devIdIn; }

    EMSCRIPTEN_KEEPALIVE
        int doInit()                    {
            return GGWave_init(-1, -1);
        }
}

void GGWave_setDefaultCaptureDeviceName(std::string name) {
    g_defaultCaptureDeviceName = std::move(name);
}

bool GGWave_init(
        const int playbackId,
        const int captureId) {

    if (g_devIdIn && g_devIdOut) {
        return false;
    }

    if (g_devIdIn == 0 && g_devIdOut == 0) {
        SDL_LogSetPriority(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_INFO);

        if (SDL_Init(SDL_INIT_AUDIO) < 0) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't initialize SDL: %s\n", SDL_GetError());
            return (1);
        }

        SDL_SetHintWithPriority(SDL_HINT_AUDIO_RESAMPLING_MODE, "medium", SDL_HINT_OVERRIDE);

        {
            int nDevices = SDL_GetNumAudioDevices(SDL_FALSE);
            printf("Found %d playback devices:\n", nDevices);
            for (int i = 0; i < nDevices; i++) {
                printf("    - Playback device #%d: '%s'\n", i, SDL_GetAudioDeviceName(i, SDL_FALSE));
            }
        }
        {
            int nDevices = SDL_GetNumAudioDevices(SDL_TRUE);
            printf("Found %d capture devices:\n", nDevices);
            for (int i = 0; i < nDevices; i++) {
                printf("    - Capture device #%d: '%s'\n", i, SDL_GetAudioDeviceName(i, SDL_TRUE));
            }
        }
    }

    bool reinit = false;

    if (g_devIdOut == 0) {
        printf("Initializing playback ...\n");

        SDL_AudioSpec playbackSpec;
        SDL_zero(playbackSpec);

        playbackSpec.freq = GGWave::kBaseSampleRate;
        playbackSpec.format = AUDIO_S16SYS;
        playbackSpec.channels = 1;
        playbackSpec.samples = 16*1024;
        playbackSpec.callback = NULL;

        SDL_zero(g_obtainedSpecOut);

        if (playbackId >= 0) {
            printf("Attempt to open playback device %d : '%s' ...\n", playbackId, SDL_GetAudioDeviceName(playbackId, SDL_FALSE));
            g_devIdOut = SDL_OpenAudioDevice(SDL_GetAudioDeviceName(playbackId, SDL_FALSE), SDL_FALSE, &playbackSpec, &g_obtainedSpecOut, 0);
        } else {
            printf("Attempt to open default playback device ...\n");
            g_devIdOut = SDL_OpenAudioDevice(NULL, SDL_FALSE, &playbackSpec, &g_obtainedSpecOut, 0);
        }

        if (!g_devIdOut) {
            printf("Couldn't open an audio device for playback: %s!\n", SDL_GetError());
            g_devIdOut = 0;
        } else {
            printf("Obtained spec for output device (SDL Id = %d):\n", g_devIdOut);
            printf("    - Sample rate:       %d (required: %d)\n", g_obtainedSpecOut.freq, playbackSpec.freq);
            printf("    - Format:            %d (required: %d)\n", g_obtainedSpecOut.format, playbackSpec.format);
            printf("    - Channels:          %d (required: %d)\n", g_obtainedSpecOut.channels, playbackSpec.channels);
            printf("    - Samples per frame: %d (required: %d)\n", g_obtainedSpecOut.samples, playbackSpec.samples);

            if (g_obtainedSpecOut.format != playbackSpec.format ||
                g_obtainedSpecOut.channels != playbackSpec.channels ||
                g_obtainedSpecOut.samples != playbackSpec.samples) {
                g_devIdOut = 0;
                SDL_CloseAudio();
                fprintf(stderr, "Failed to initialize playback SDL_OpenAudio!");

                return false;
            }

            reinit = true;
        }
    }

    if (g_devIdIn == 0) {
        SDL_AudioSpec captureSpec;
        captureSpec = g_obtainedSpecOut;
        captureSpec.freq = GGWave::kBaseSampleRate;
        captureSpec.format = AUDIO_F32SYS;
        captureSpec.samples = 4096;

        SDL_zero(g_obtainedSpecIn);

        if (captureId >= 0) {
            printf("Attempt to open capture device %d : '%s' ...\n", captureId, SDL_GetAudioDeviceName(captureId, SDL_FALSE));
            g_devIdIn = SDL_OpenAudioDevice(SDL_GetAudioDeviceName(captureId, SDL_TRUE), SDL_TRUE, &captureSpec, &g_obtainedSpecIn, 0);
        } else {
            printf("Attempt to open default capture device ...\n");
            g_devIdIn = SDL_OpenAudioDevice(g_defaultCaptureDeviceName.empty() ? nullptr : g_defaultCaptureDeviceName.c_str(),
                                            SDL_TRUE, &captureSpec, &g_obtainedSpecIn, 0);
        }
        if (!g_devIdIn) {
            printf("Couldn't open an audio device for capture: %s!\n", SDL_GetError());
            g_devIdIn = 0;
        } else {
            printf("Obtained spec for input device (SDL Id = %d):\n", g_devIdIn);
            printf("    - Sample rate:       %d\n", g_obtainedSpecIn.freq);
            printf("    - Format:            %d (required: %d)\n", g_obtainedSpecIn.format, captureSpec.format);
            printf("    - Channels:          %d (required: %d)\n", g_obtainedSpecIn.channels, captureSpec.channels);
            printf("    - Samples per frame: %d\n", g_obtainedSpecIn.samples);

            reinit = true;
        }
    }

    int sampleSizeBytesIn = 4;
    int sampleSizeBytesOut = 2;

    switch (g_obtainedSpecIn.format) {
        case AUDIO_U8:
        case AUDIO_S8:
            sampleSizeBytesIn = 1;
            break;
        case AUDIO_U16SYS:
        case AUDIO_S16SYS:
            sampleSizeBytesIn = 2;
            break;
        case AUDIO_S32SYS:
        case AUDIO_F32SYS:
            sampleSizeBytesIn = 4;
            break;
    }

    switch (g_obtainedSpecOut.format) {
        case AUDIO_U8:
        case AUDIO_S8:
            sampleSizeBytesOut = 1;
            break;
        case AUDIO_U16SYS:
        case AUDIO_S16SYS:
            sampleSizeBytesOut = 2;
            break;
        case AUDIO_S32SYS:
        case AUDIO_F32SYS:
            sampleSizeBytesOut = 4;
            break;
    }

    if (reinit) {
        if (g_ggWave) delete g_ggWave;

        g_ggWave = new GGWave(
                g_obtainedSpecIn.freq,
                g_obtainedSpecOut.freq,
                1024,
                sampleSizeBytesIn,
                sampleSizeBytesOut);
    }

    return true;
}

GGWave * GGWave_instance() { return g_ggWave; }

bool GGWave_mainLoop() {
    if (g_devIdIn == 0 && g_devIdOut == 0) {
        return false;
    }

    static GGWave::CBQueueAudio cbQueueAudio = [&](const void * data, uint32_t nBytes) {
        SDL_QueueAudio(g_devIdOut, data, nBytes);
    };

    static GGWave::CBDequeueAudio CBDequeueAudio = [&](void * data, uint32_t nMaxBytes) {
        return SDL_DequeueAudio(g_devIdIn, data, nMaxBytes);
    };

    if (g_ggWave->hasTxData() == false) {
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
        }
    } else {
        SDL_PauseAudioDevice(g_devIdOut, SDL_TRUE);
        SDL_PauseAudioDevice(g_devIdIn, SDL_TRUE);

        g_ggWave->send(cbQueueAudio);
    }

    return true;
}

bool GGWave_deinit() {
    if (g_devIdIn == 0 && g_devIdOut == 0) {
        return false;
    }

    delete g_ggWave;
    g_ggWave = nullptr;

    SDL_PauseAudioDevice(g_devIdIn, 1);
    SDL_CloseAudioDevice(g_devIdIn);
    SDL_PauseAudioDevice(g_devIdOut, 1);
    SDL_CloseAudioDevice(g_devIdOut);
    SDL_CloseAudio();
    SDL_Quit();

    return true;
}
