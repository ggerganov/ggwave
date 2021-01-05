#include "interface.h"

int g_lastShareId = 0;
int g_lastOpenId = 0;
int g_lastDeleteId = 0;
int g_lastReceivedId = 0;
int g_frameCount = 0;

void updateMain() {
    auto curShareId = getShareId();
    if (curShareId != g_lastShareId) {
        auto shareInfo = getShareInfo();
        interface_shareFile(
                shareInfo.uri.data(),
                shareInfo.filename.data(),
                shareInfo.dataBuffer,
                shareInfo.dataSize);

        g_lastShareId = curShareId;
    }

    auto curOpenId = getOpenId();
    if (curOpenId != g_lastOpenId) {
        auto openInfo = getOpenInfo();
        interface_openFile(
                openInfo.uri.data(),
                openInfo.filename.data(),
                openInfo.dataBuffer,
                openInfo.dataSize);

        g_lastOpenId = curOpenId;
    }

    auto curDeleteId = getDeleteId();
    if (curDeleteId != g_lastDeleteId) {
        auto deleteInfo = getDeleteInfo();
        interface_deleteFile(deleteInfo.uri.c_str(), deleteInfo.filename.c_str());

        bool isRemoveAll = std::string(deleteInfo.uri) == "###ALL-FILES###";

        if (interface_needReloadFiles() || isRemoveAll) {
            clearAllFiles();
            interface_loadAllFiles();
        } else {
            clearFile(deleteInfo.uri.c_str());
        }

        g_lastDeleteId = curDeleteId;
    }

    auto curReceivedId = getReceivedId();
    if (curReceivedId != g_lastReceivedId) {
        auto receiveInfos = getReceiveInfos();

        int n = (int) receiveInfos.size();

        for (int i = 0; i < n; ++i) {
            interface_receiveFile(
                    receiveInfos[i].uri,
                    receiveInfos[i].filename,
                    receiveInfos[i].dataBuffer,
                    receiveInfos[i].dataSize);
            confirmReceive(receiveInfos[i].uri);
        }

        if (interface_needReloadFiles()) {
            clearAllFiles();
            interface_loadAllFiles();
        }

        g_lastReceivedId = curReceivedId;
    }
}
