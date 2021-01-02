#ifndef interface_h
#define interface_h

#ifdef __cplusplus
#include "common.h"

extern "C" {
#endif

void interface_addFile(
        const char * uri,
        const char * filename,
        const char * dataBuffer,
        size_t dataSize);

void interface_loadAllFiles();

void interface_shareFile(
        const char * uri,
        const char * filename,
        const char * dataBuffer,
        size_t dataSize);

void interface_openFile(
        const char * uri,
        const char * filename,
        const char * dataBuffer,
        size_t dataSize);

void interface_deleteFile(
        const char * uri,
        const char * filename);

void interface_receiveFile(
        const char * uri,
        const char * filename,
        const char * dataBuffer,
        size_t dataSize);

bool interface_needReloadFiles();

void updateMain();

#ifdef __cplusplus
}
#endif

#endif /* interface_h */
