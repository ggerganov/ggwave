#pragma once

#include "ggwave-common-sdl2.h"

#include <thread>
#include <vector>

std::thread initMainAndRunCore();

void initMain();
void updateCore();
void renderMain();
void deinitMain();

// share info

struct ShareInfo {
    std::string uri;
    std::string filename;
    const char * dataBuffer;
    size_t dataSize;
};

int getShareId();
ShareInfo getShareInfo();

// open info

struct OpenInfo {
    std::string uri;
    std::string filename;
    const char * dataBuffer;
    size_t dataSize;
};

int getOpenId();
OpenInfo getOpenInfo();

// delete file

struct DeleteInfo {
    std::string uri;
    std::string filename;
};

int getDeleteId();
DeleteInfo getDeleteInfo();

// receive

struct ReceiveInfo {
    const char * uri;
    const char * filename;
    const char * dataBuffer;
    size_t dataSize;
};

int getReceivedId();
std::vector<ReceiveInfo> getReceiveInfos();
bool confirmReceive(const char * uri);

// input

void clearAllFiles();
void clearFile(const char * uri);

void addFile(
        const char * uri,
        const char * filename,
        const char * dataBuffer,
        size_t dataSize,
        bool focus);

void addFile(
        const char * uri,
        const char * filename,
        std::vector<char> && data,
        bool focus);
