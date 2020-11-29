#include "ggwave-common-sdl2.h"

#include "ggwave/ggwave.h"

constexpr double kBaseSampleRate = 48000.0;

bool initSDL2ForGGWave(
        bool & isInitialized,
        const int playbackId,
        SDL_AudioDeviceID & devIdIn,
        const int captureId,
        SDL_AudioDeviceID & devIdOut,
        GGWave *& ggWave,
        const char * defaultCaptureDeviceName) {
    if (isInitialized) return 0;

    printf("Initializing ...\n");

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

    SDL_AudioSpec playbackSpec;
    SDL_zero(playbackSpec);

    playbackSpec.freq = ::kBaseSampleRate;
    playbackSpec.format = AUDIO_S16SYS;
    playbackSpec.channels = 1;
    playbackSpec.samples = 16*1024;
    playbackSpec.callback = NULL;

    SDL_AudioSpec obtainedSpecIn;
    SDL_AudioSpec obtainedSpecOut;

    int sampleSizeBytesIn = 4;
    int sampleSizeBytesOut = 2;

    SDL_zero(obtainedSpecIn);
    SDL_zero(obtainedSpecOut);

    if (playbackId >= 0) {
        printf("Attempt to open playback device %d : '%s' ...\n", playbackId, SDL_GetAudioDeviceName(playbackId, SDL_FALSE));
        devIdOut = SDL_OpenAudioDevice(SDL_GetAudioDeviceName(playbackId, SDL_FALSE), SDL_FALSE, &playbackSpec, &obtainedSpecOut, 0);
    } else {
        printf("Attempt to open default playback device ...\n");
        devIdOut = SDL_OpenAudioDevice(NULL, SDL_FALSE, &playbackSpec, &obtainedSpecOut, 0);
    }

    if (!devIdOut) {
        printf("Couldn't open an audio device for playback: %s!\n", SDL_GetError());
        devIdOut = 0;
    } else {
        printf("Obtained spec for output device (SDL Id = %d):\n", devIdOut);
        printf("    - Sample rate:       %d (required: %d)\n", obtainedSpecOut.freq, playbackSpec.freq);
        printf("    - Format:            %d (required: %d)\n", obtainedSpecOut.format, playbackSpec.format);
        printf("    - Channels:          %d (required: %d)\n", obtainedSpecOut.channels, playbackSpec.channels);
        printf("    - Samples per frame: %d (required: %d)\n", obtainedSpecOut.samples, playbackSpec.samples);

        if (obtainedSpecOut.format != playbackSpec.format ||
            obtainedSpecOut.channels != playbackSpec.channels ||
            obtainedSpecOut.samples != playbackSpec.samples) {
            SDL_CloseAudio();
            fprintf(stderr, "Failed to initialize playback SDL_OpenAudio!");
            return false;
        }
    }

    switch (obtainedSpecOut.format) {
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

    SDL_AudioSpec captureSpec;
    captureSpec = obtainedSpecOut;
    captureSpec.freq = ::kBaseSampleRate;
    captureSpec.format = AUDIO_F32SYS;
    captureSpec.samples = 4096;

    if (captureId >= 0) {
        printf("Attempt to open capture device %d : '%s' ...\n", captureId, SDL_GetAudioDeviceName(captureId, SDL_FALSE));
        devIdIn = SDL_OpenAudioDevice(SDL_GetAudioDeviceName(captureId, SDL_TRUE), SDL_TRUE, &captureSpec, &obtainedSpecIn, 0);
    } else {
        printf("Attempt to open default capture device ...\n");
        devIdIn = SDL_OpenAudioDevice(defaultCaptureDeviceName, SDL_TRUE, &captureSpec, &obtainedSpecIn, 0);
    }
    if (!devIdIn) {
        printf("Couldn't open an audio device for capture: %s!\n", SDL_GetError());
        devIdIn = 0;
    } else {
        printf("Obtained spec for input device (SDL Id = %d):\n", devIdIn);
        printf("    - Sample rate:       %d\n", obtainedSpecIn.freq);
        printf("    - Format:            %d (required: %d)\n", obtainedSpecIn.format, captureSpec.format);
        printf("    - Channels:          %d (required: %d)\n", obtainedSpecIn.channels, captureSpec.channels);
        printf("    - Samples per frame: %d\n", obtainedSpecIn.samples);
    }

    switch (obtainedSpecIn.format) {
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

    ggWave = new GGWave(
            obtainedSpecIn.freq,
            obtainedSpecOut.freq,
            1024,
            sampleSizeBytesIn,
            sampleSizeBytesOut);

    isInitialized = true;
    return 0;
}

