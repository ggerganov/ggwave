#include "interface.h"

#include "pfd/pfd.h"

#include <fstream>

void interface_addFile(
        const char * ,
        const char * ,
        const char * ,
        size_t ) {
}

void interface_loadAllFiles() {
}

void interface_shareFile(
        const char * ,
        const char * filename,
        const char * dataBuffer,
        size_t dataSize) {
    auto f = pfd::save_file("Save file", filename, { "All Files", "*" }, pfd::opt::none);

    if (f.result().empty() == false) {
        printf("Saving to: %s\n", f.result().c_str());
        std::ofstream fout(f.result(), std::ios::binary);
        if (fout.is_open() && fout.good()) {
            fout.write(dataBuffer, dataSize);
            fout.close();
        }
    }
}

void interface_openFile(
        const char * ,
        const char * ,
        const char * ,
        size_t ) {
}

void interface_deleteFile(
        const char * ,
        const char * ) {
}

void interface_receiveFile(
        const char * uri,
        const char * filename,
        const char * dataBuffer,
        size_t dataSize) {
    addFile(uri, filename, dataBuffer, dataSize, false);
}

bool interface_needReloadFiles() {
    return false;
}
