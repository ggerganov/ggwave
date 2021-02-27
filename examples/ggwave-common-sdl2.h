#pragma once

#include <string>

class GGWave;

// GGWave helpers

void GGWave_setDefaultCaptureDeviceName(std::string name);
bool GGWave_init(const int playbackId, const int captureId, const int payloadLength = -1, const float sampleRateOffset = 0);
GGWave *& GGWave_instance();
bool GGWave_mainLoop();
bool GGWave_deinit();
