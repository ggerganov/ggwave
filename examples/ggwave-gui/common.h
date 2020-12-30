#pragma once

#include "ggwave-common-sdl2.h"

#include <thread>
#include <vector>

std::thread initMain();
void renderMain();
void deinitMain(std::thread & worker);

int getShareId();
const char * getShareFilename();

int getDeleteId();
const char * getDeleteFilename();

int getReceivedId();
std::vector<const char *> getReceivedFilename();
std::vector<const char *> getReceivedDataBuffer();
std::vector<size_t>       getReceivedDataSize();

void clearFiles();

void addFile(
        const char * uri,
        const char * filename,
        const char * dataBuffer,
        size_t dataSize);

void addFile(
        const char * uri,
        const char * filename,
        std::vector<char> && data);
