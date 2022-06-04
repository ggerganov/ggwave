#pragma once

#include <string>
#include <memory>

class GGWave;

// GGWave helpers

void GGWave_setDefaultCaptureDeviceName(std::string name);
bool GGWave_init(const int playbackId, const int captureId, const int payloadLength = -1, const float sampleRateOffset = 0, const bool useDSS = false);
std::shared_ptr<GGWave> GGWave_instance();
void GGWave_reset(void * parameters);
bool GGWave_mainLoop();
bool GGWave_deinit();
