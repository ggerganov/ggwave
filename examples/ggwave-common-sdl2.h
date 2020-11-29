#pragma once

#include <SDL.h>

class GGWave;

bool initSDL2ForGGWave(
        bool & isInitialized,
        const int playbackId,
        SDL_AudioDeviceID & devIdIn,
        const int captureId,
        SDL_AudioDeviceID & devIdOut,
        GGWave *& ggWave,
        const char * defaultCaptureDeviceName = nullptr);
