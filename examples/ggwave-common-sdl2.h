#pragma once

#include <SDL.h>

#include <string>

class GGWave;

// GGWave helpers

void GGWave_setDefaultCaptureDeviceName(std::string name);
bool GGWave_init(const int playbackId, const int captureId);
GGWave * GGWave_instance();
bool GGWave_mainLoop();
bool GGWave_deinit();

// ImGui helpers

bool ImGui_beginFrame(SDL_Window * window);
bool ImGui_endFrame(SDL_Window * window);
