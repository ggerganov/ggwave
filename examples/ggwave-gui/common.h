#pragma once

#include "ggwave-common-sdl2.h"

#include <thread>

std::thread initMain();
void renderMain();
void deinitMain(std::thread & worker);
