#include "common.h"

#include "ggwave-common.h"

#include "ggwave/ggwave.h"

#include "ggsock/communicator.h"
#include "ggsock/file-server.h"
#include "ggsock/serialization.h"

#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>

#include <SDL.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#if defined(IOS) || defined(ANDROID)
#include "imgui-wrapper/icons_font_awesome.h"
#endif

#ifndef ICON_FA_COGS
#include "icons_font_awesome.h"
#endif

namespace {
std::mutex g_mutex;
char * toTimeString(const std::chrono::system_clock::time_point & tp) {
    std::lock_guard<std::mutex> lock(g_mutex);

    time_t t = std::chrono::system_clock::to_time_t(tp);
    std::tm * ptm = std::localtime(&t);
    static char buffer[32];
    std::strftime(buffer, 32, "%H:%M:%S", ptm);
    return buffer;
}

void ScrollWhenDraggingOnVoid(const ImVec2& delta, ImGuiMouseButton mouse_button) {
    ImGuiContext& g = *ImGui::GetCurrentContext();
    ImGuiWindow* window = g.CurrentWindow;
    bool hovered = false;
    bool held = false;
    ImGuiButtonFlags button_flags = (mouse_button == 0) ? ImGuiButtonFlags_MouseButtonLeft : (mouse_button == 1) ? ImGuiButtonFlags_MouseButtonRight : ImGuiButtonFlags_MouseButtonMiddle;
    if (g.HoveredId == 0) // If nothing hovered so far in the frame (not same as IsAnyItemHovered()!)
        ImGui::ButtonBehavior(window->Rect(), window->GetID("##scrolldraggingoverlay"), &hovered, &held, button_flags);
    if (held && delta.x != 0.0f)
        ImGui::SetScrollX(window, window->Scroll.x + delta.x);
    if (held && delta.y != 0.0f)
        ImGui::SetScrollY(window, window->Scroll.y + delta.y);
}

}

namespace ImGui {
bool ButtonDisabled(const char* label, const ImVec2& size = ImVec2(0, 0)) {
    {
        auto col = ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled);
        col.x *= 0.8;
        col.y *= 0.8;
        col.z *= 0.8;
        PushStyleColor(ImGuiCol_Button, col);
        PushStyleColor(ImGuiCol_ButtonHovered, col);
        PushStyleColor(ImGuiCol_ButtonActive, col);
    }
    {
        auto col = ImGui::GetStyleColorVec4(ImGuiCol_Text);
        col.x *= 0.75;
        col.y *= 0.75;
        col.z *= 0.75;
        PushStyleColor(ImGuiCol_Text, col);
    }
    bool result = Button(label, size);
    PopStyleColor(4);
    return result;
}

bool ButtonDisablable(const char* label, const ImVec2& size = ImVec2(0, 0), bool isDisabled = false) {
    if (isDisabled) {
        ButtonDisabled(label, size);
        return false;
    }
    return Button(label, size);
}

bool ButtonSelected(const char* label, const ImVec2& size = ImVec2(0, 0)) {
    auto col = ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive);
    PushStyleColor(ImGuiCol_Button, col);
    PushStyleColor(ImGuiCol_ButtonHovered, col);
    bool result = Button(label, size);
    PopStyleColor(2);
    return result;
}

bool ButtonSelectable(const char* label, const ImVec2& size = ImVec2(0, 0), bool isSelected = false) {
    if (isSelected) return ButtonSelected(label, size);
    return Button(label, size);
}

}

static const char * kFileBroadcastPrefix = "\xba\xbc\xbb";
static const int kMaxSimultaneousChunkRequests = 4;
static const float kBroadcastTime_sec = 60.0f;

struct Message {
    enum Type {
        Error,
        Text,
        FileBroadcast,
    };

    bool received;
    std::chrono::system_clock::time_point timestamp;
    std::string data;
    int protocolId;
    float volume;
    Type type;
};

struct GGWaveStats {
    bool isReceiving;
    bool isAnalyzing;
    int framesToRecord;
    int framesLeftToRecord;
    int framesToAnalyze;
    int framesLeftToAnalyze;
};

struct State {
    bool update = false;

    struct Flags {
        bool newMessage = false;
        bool newSpectrum = false;
        bool newTxAmplitudeData = false;
        bool newStats = false;

        void clear() { memset(this, 0, sizeof(Flags)); }
    } flags;

    void apply(State & dst) {
        if (update == false) return;

        if (this->flags.newMessage) {
            dst.update = true;
            dst.flags.newMessage = true;
            dst.message = std::move(this->message);
        }

        if (this->flags.newSpectrum) {
            dst.update = true;
            dst.flags.newSpectrum = true;
            dst.spectrum = std::move(this->spectrum);
        }

        if (this->flags.newTxAmplitudeData) {
            dst.update = true;
            dst.flags.newTxAmplitudeData = true;
            dst.txAmplitudeData = std::move(this->txAmplitudeData);
        }

        if (this->flags.newStats) {
            dst.update = true;
            dst.flags.newStats = true;
            dst.stats = std::move(this->stats);
        }

        flags.clear();
        update = false;
    }

    Message message;
    GGWave::SpectrumData spectrum;
    GGWave::AmplitudeDataI16 txAmplitudeData;
    GGWaveStats stats;
};

struct Input {
    bool update = false;
    Message message;

    bool reinit = false;
    bool isSampleRateOffset = false;
    int payloadLength = -1;
};

struct Buffer {
    std::mutex mutex;

    State stateCore;
    Input inputCore;

    State stateUI;
    Input inputUI;
};

std::atomic<bool> g_isRunning;
GGWave * g_ggWave;
Buffer g_buffer;

// file send data
struct BroadcastInfo {
    std::string ip;
    int port;
    int key;
};

bool g_focusFileSend = false;
float g_tLastBroadcast = -100.0f;
GGSock::FileServer g_fileServer;

// file received data
struct FileInfoExtended {
    bool isReceiving = false;
    bool readyToShare = false;
    bool requestToShare = false;
    int nReceivedChunks = 0;
    int nRequestedChunks = 0;
    std::vector<bool> isChunkRequested;
    std::vector<bool> isChunkReceived;
};

bool g_hasRemoteInfo = false;
int g_remotePort = 23045;
std::string g_remoteIP = "127.0.0.1";

bool g_hasReceivedFileInfos = false;
bool g_hasRequestedFileInfos = false;
bool g_hasReceivedFiles = false;
GGSock::FileServer::TFileInfos g_receivedFileInfos;
std::map<GGSock::FileServer::TURI, GGSock::FileServer::FileData> g_receivedFiles;
std::map<GGSock::FileServer::TURI, FileInfoExtended> g_receivedFileInfosExtended;

GGSock::Communicator g_fileClient(false);

// external api

int g_shareId = 0;
ShareInfo g_shareInfo;

int g_openId = 0;
OpenInfo g_openInfo;

int g_deleteId = 0;
DeleteInfo g_deleteInfo;

int g_receivedId = 0;

int getShareId() {
    return g_shareId;
}

ShareInfo getShareInfo() {
    return g_shareInfo;
}

int getOpenId() {
    return g_openId;
}

OpenInfo getOpenInfo() {
    return g_openInfo;
}

int getDeleteId() {
    return g_deleteId;
}

DeleteInfo getDeleteInfo() {
    return g_deleteInfo;
}

int getReceivedId() {
    return g_receivedId;
}

std::vector<ReceiveInfo> getReceiveInfos() {
    std::vector<ReceiveInfo> result;

    for (const auto & file : g_receivedFiles) {
        if (g_receivedFileInfosExtended[file.second.info.uri].requestToShare == false ||
            g_receivedFileInfosExtended[file.second.info.uri].readyToShare == true) {
            continue;
        }
        result.push_back({
            file.second.info.uri.c_str(),
            file.second.info.filename.c_str(),
            file.second.data.data(),
            file.second.data.size(),
        });
    }

    return result;
}

bool confirmReceive(const char * uri) {
    if (g_receivedFiles.find(uri) == g_receivedFiles.end()) {
        return false;
    }

    g_receivedFileInfosExtended[uri].readyToShare = true;
    g_receivedFiles.erase(uri);

    return true;
}

void clearAllFiles() {
    g_fileServer.clearAllFiles();
}

void clearFile(const char * uri) {
    g_fileServer.clearFile(uri);
}

void addFile(
        const char * uri,
        const char * filename,
        const char * dataBuffer,
        size_t dataSize,
        bool focus) {
    GGSock::FileServer::FileData file;
    file.info.uri = uri;
    file.info.filename = filename;
    file.data.resize(dataSize);
    std::memcpy(file.data.data(), dataBuffer, dataSize);

    g_fileServer.addFile(std::move(file));

    if (focus) {
        g_focusFileSend = true;
    }
}

void addFile(
        const char * uri,
        const char * filename,
        std::vector<char> && data,
        bool focus) {
    GGSock::FileServer::FileData file;
    file.info.uri = uri;
    file.info.filename = filename;
    file.data = std::move(data);

    g_fileServer.addFile(std::move(file));

    if (focus) {
        g_focusFileSend = true;
    }
}

std::string generateFileBroadcastMessage() {
    // todo : to binary
    std::string result;

    int plen = (int) strlen(kFileBroadcastPrefix);
    result.resize(plen + 4 + 2 + 2);

    char *p = &result[0];
    for (int i = 0; i < (int) plen; ++i) {
        *p++ = kFileBroadcastPrefix[i];
    }

    {
        auto ip = GGSock::Communicator::getLocalAddress();
        std::replace(ip.begin(), ip.end(), '.', ' ');
        std::stringstream ss(ip);

        { int b; ss >> b; *p++ = b; }
        { int b; ss >> b; *p++ = b; }
        { int b; ss >> b; *p++ = b; }
        { int b; ss >> b; *p++ = b; }
    }

    {
        uint16_t port = g_fileServer.getParameters().listenPort;

        { int b = port/256; *p++ = b; }
        { int b = port%256; *p++ = b; }
    }

    {
        uint16_t key = rand()%65536;

        { int b = key/256; *p++ = b; }
        { int b = key%256; *p++ = b; }
    }

    return result;
}

BroadcastInfo parseBroadcastInfo(const std::string & message) {
    BroadcastInfo result;

    const uint8_t *p = (uint8_t *) message.data();
    p += strlen(kFileBroadcastPrefix);

    result.ip += std::to_string((uint8_t)(*p++));
    result.ip += '.';
    result.ip += std::to_string((uint8_t)(*p++));
    result.ip += '.';
    result.ip += std::to_string((uint8_t)(*p++));
    result.ip += '.';
    result.ip += std::to_string((uint8_t)(*p++));

    result.port  = 256*((int)(*p++));
    result.port +=     ((int)(*p++));

    result.key  = 256*((int)(*p++));
    result.key +=     ((int)(*p++));

    return result;
}

bool isFileBroadcastMessage(const std::string & message) {
    if (message.size() != strlen(kFileBroadcastPrefix) + 4 + 2 + 2) {
        return false;
    }

    bool result = true;

    auto pSrc = kFileBroadcastPrefix;
    auto pDst = message.data();

    while (*pSrc != 0) {
        if (*pDst == 0 || *pSrc++ != *pDst++) {
            result = false;
            break;
        }
    }

    return result;
}

std::thread initMainAndRunCore() {
    initMain();

    return std::thread([&]() {
        while (g_isRunning) {
            updateCore();

            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    });
}

void initMain() {
    g_isRunning = true;
    g_ggWave = GGWave_instance();

    GGSock::FileServer::Parameters p;
#ifdef __EMSCRIPTEN__
    p.nWorkerThreads = 0;
#else
    p.nWorkerThreads = 2;
#endif
    g_fileServer.init(p);

    g_fileClient.setErrorCallback([](GGSock::Communicator::TErrorCode code) {
        printf("Disconnected with code = %d\n", code);

        g_hasReceivedFileInfos = false;
        g_hasRequestedFileInfos = false;
        g_hasReceivedFiles = false;
    });

    g_fileClient.setMessageCallback(GGSock::FileServer::MsgFileInfosResponse, [&](const char * dataBuffer, size_t dataSize) {
        printf("Received message %d, size = %d\n", GGSock::FileServer::MsgFileInfosResponse, (int) dataSize);

        size_t offset = 0;
        GGSock::Unserialize()(g_receivedFileInfos, dataBuffer, dataSize, offset);

        for (const auto & info : g_receivedFileInfos) {
            printf("    - %s : %s (size = %d, chunks = %d)\n", info.second.uri.c_str(), info.second.filename.c_str(), (int) info.second.filesize, (int) info.second.nChunks);
            g_receivedFiles[info.second.uri].info = info.second;
            g_receivedFiles[info.second.uri].data.resize(info.second.filesize);

            g_receivedFileInfosExtended[info.second.uri] = {};
        }

        g_hasReceivedFileInfos = true;

        return 0;
    });

    g_fileClient.setMessageCallback(GGSock::FileServer::MsgFileChunkResponse, [&](const char * dataBuffer, size_t dataSize) {
        GGSock::FileServer::FileChunkResponseData data;

        size_t offset = 0;
        GGSock::Unserialize()(data, dataBuffer, dataSize, offset);

        //printf("Received chunk %d for file '%s', size = %d\n", data.chunkId, data.uri.c_str(), (int) data.data.size());
        std::memcpy(g_receivedFiles[data.uri].data.data() + data.pStart, data.data.data(), data.pLen);

        g_receivedFileInfosExtended[data.uri].nReceivedChunks++;
        g_receivedFileInfosExtended[data.uri].nRequestedChunks--;
        g_receivedFileInfosExtended[data.uri].isChunkReceived[data.chunkId] = true;

        return 0;
    });
}

void updateCore() {
    static Input inputCurrent;

    static int lastRxDataLength = 0;
    static float lastRxTimestamp = 0.0f;
    static GGWave::TxRxData lastRxData;

    {
        std::lock_guard<std::mutex> lock(g_buffer.mutex);
        if (g_buffer.inputCore.update) {
            inputCurrent = std::move(g_buffer.inputCore);
            g_buffer.inputCore.update = false;
        }
    }

    if (inputCurrent.reinit) {
        GGWave_deinit();
        // todo : use the provided cli arguments for playback and capture device
        GGWave_init(0, 0, inputCurrent.payloadLength, inputCurrent.isSampleRateOffset ? -512 : 0);
        g_ggWave = GGWave_instance();

        inputCurrent.reinit = false;
    }

    if (inputCurrent.update) {
        g_ggWave->init(
                (int) inputCurrent.message.data.size(),
                inputCurrent.message.data.data(),
                g_ggWave->getTxProtocol(inputCurrent.message.protocolId),
                100*inputCurrent.message.volume);

        inputCurrent.update = false;
    }

    GGWave_mainLoop();

    lastRxDataLength = g_ggWave->takeRxData(lastRxData);
    if (lastRxDataLength == -1) {
        g_buffer.stateCore.update = true;
        g_buffer.stateCore.flags.newMessage = true;
        g_buffer.stateCore.message = {
            true,
            std::chrono::system_clock::now(),
            "",
            g_ggWave->getRxProtocolId(),
            0,
            Message::Error,
        };
    } else if (lastRxDataLength > 0 && ImGui::GetTime() - lastRxTimestamp > 0.5f) {
        auto message = std::string((char *) lastRxData.data(), lastRxDataLength);
        const Message::Type type = isFileBroadcastMessage(message) ? Message::FileBroadcast : Message::Text;
        g_buffer.stateCore.update = true;
        g_buffer.stateCore.flags.newMessage = true;
        g_buffer.stateCore.message = {
            true,
            std::chrono::system_clock::now(),
            std::move(message),
            g_ggWave->getRxProtocolId(),
            0,
            type,
        };
        lastRxTimestamp = ImGui::GetTime();
    }

    if (g_ggWave->takeSpectrum(g_buffer.stateCore.spectrum)) {
        g_buffer.stateCore.update = true;
        g_buffer.stateCore.flags.newSpectrum = true;
    }

    if (g_ggWave->takeTxAmplitudeDataI16(g_buffer.stateCore.txAmplitudeData)) {
        g_buffer.stateCore.update = true;
        g_buffer.stateCore.flags.newTxAmplitudeData = true;
    }

    if (true) {
        g_buffer.stateCore.update = true;
        g_buffer.stateCore.flags.newStats = true;
        g_buffer.stateCore.stats.isReceiving = g_ggWave->isReceiving();
        g_buffer.stateCore.stats.isAnalyzing = g_ggWave->isAnalyzing();
        g_buffer.stateCore.stats.framesToRecord = g_ggWave->getFramesToRecord();
        g_buffer.stateCore.stats.framesLeftToRecord = g_ggWave->getFramesLeftToRecord();
        g_buffer.stateCore.stats.framesToAnalyze = g_ggWave->getFramesToAnalyze();
        g_buffer.stateCore.stats.framesLeftToAnalyze = g_ggWave->getFramesLeftToAnalyze();
    }

    {
        std::lock_guard<std::mutex> lock(g_buffer.mutex);
        g_buffer.stateCore.apply(g_buffer.stateUI);
    }
}

void renderMain() {
    g_fileServer.update();

    if (ImGui::GetTime() - g_tLastBroadcast > kBroadcastTime_sec && g_fileServer.isListening()) {
        g_fileServer.stopListening();
    }

    if (g_fileClient.isConnected()) {
        if (!g_hasRequestedFileInfos) {
            g_receivedFileInfos.clear();
            g_receivedFiles.clear();
            g_receivedFileInfosExtended.clear();

            g_fileClient.send(GGSock::FileServer::MsgFileInfosRequest);
            g_hasRequestedFileInfos = true;
        } else {
            for (const auto & fileInfo : g_receivedFileInfos) {
                const auto & uri = fileInfo.second.uri;
                auto & fileInfoExtended = g_receivedFileInfosExtended[uri];

                if (fileInfoExtended.isReceiving == false) {
                    continue;
                }

                if (fileInfoExtended.nReceivedChunks == fileInfo.second.nChunks) {
                    continue;
                }

                int nextChunkId = 0;
                while (fileInfoExtended.nRequestedChunks < kMaxSimultaneousChunkRequests) {
                    if (fileInfoExtended.nReceivedChunks + fileInfoExtended.nRequestedChunks == fileInfo.second.nChunks) {
                        break;
                    }

                    while (fileInfoExtended.isChunkRequested[nextChunkId] == true) {
                        ++nextChunkId;
                    }
                    fileInfoExtended.isChunkRequested[nextChunkId] = true;

                    GGSock::FileServer::FileChunkRequestData data;
                    data.uri = fileInfo.second.uri;
                    data.chunkId = nextChunkId;
                    data.nChunksHave = 0;
                    data.nChunksExpected = fileInfo.second.nChunks;

                    GGSock::SerializationBuffer buffer;
                    GGSock::Serialize()(data, buffer);
                    g_fileClient.send(GGSock::FileServer::MsgFileChunkRequest, buffer.data(), (int32_t) buffer.size());

                    ++fileInfoExtended.nRequestedChunks;
                }
            }
        }
    }

    g_fileClient.update();

    static State stateCurrent;

    {
        std::lock_guard<std::mutex> lock(g_buffer.mutex);
        g_buffer.stateUI.apply(stateCurrent);
    }

    enum class WindowId {
        Settings,
        Messages,
        Files,
        Spectrum,
    };

    enum SubWindowId {
        Send,
        Receive,
    };

    struct Settings {
        int protocolId = GGWAVE_TX_PROTOCOL_DT_FASTEST;
        bool isFixedLength = false;
        int payloadLength = 8;
        bool isSampleRateOffset = false;
        float volume = 0.10f;
    };

    static WindowId windowId = WindowId::Messages;
    static SubWindowId subWindowId = SubWindowId::Send;

    static Settings settings;

    const double tHoldContextPopup = 0.5f;

    const int kMaxInputSize = 140;
    static char inputBuf[kMaxInputSize];

    static bool doInputFocus = false;
    static bool doSendMessage = false;
    static bool lastMouseButtonLeft = 0;
    static bool isTextInput = false;
    static bool scrollMessagesToBottom = true;
    static bool hasAudioCaptureData = false;
    static bool hasNewMessages = false;
#ifdef __EMSCRIPTEN__
    static bool hasFileSharingSupport = false;
#else
    static bool hasFileSharingSupport = true;
#endif

    static double tStartInput = 0.0f;
    static double tEndInput = -100.0f;
    static double tStartTx = 0.0f;
    static double tLengthTx = 0.0f;

    static GGWaveStats statsCurrent;
    static GGWave::SpectrumData spectrumCurrent;
    static GGWave::AmplitudeDataI16 txAmplitudeDataCurrent;
    static std::vector<Message> messageHistory;
    static std::string lastInput = "";

    if (stateCurrent.update) {
        if (stateCurrent.flags.newMessage) {
            scrollMessagesToBottom = true;
            messageHistory.push_back(std::move(stateCurrent.message));
            hasNewMessages = true;
        }
        if (stateCurrent.flags.newSpectrum) {
            spectrumCurrent = std::move(stateCurrent.spectrum);
            hasAudioCaptureData = !spectrumCurrent.empty();
        }
        if (stateCurrent.flags.newTxAmplitudeData) {
            txAmplitudeDataCurrent = std::move(stateCurrent.txAmplitudeData);

            tStartTx = ImGui::GetTime() + (16.0f*1024.0f)/g_ggWave->getSampleRateOut();
            tLengthTx = txAmplitudeDataCurrent.size()/g_ggWave->getSampleRateOut();
            {
                auto & ampl = txAmplitudeDataCurrent;
                int nBins = 512;
                int nspb = (int) ampl.size()/nBins;
                for (int i = 0; i < nBins; ++i) {
                    double sum = 0.0;
                    for (int j = 0; j < nspb; ++j) {
                        sum += std::abs(ampl[i*nspb + j]);
                    }
                    ampl[i] = sum/nspb;
                }
                ampl.resize(nBins);
            }
        }
        if (stateCurrent.flags.newStats) {
            statsCurrent = std::move(stateCurrent.stats);
        }
        stateCurrent.flags.clear();
        stateCurrent.update = false;
    }

    if (g_focusFileSend) {
        windowId = WindowId::Files;
        subWindowId = SubWindowId::Send;
        g_focusFileSend = false;
    }

    if (lastMouseButtonLeft == 0 && ImGui::GetIO().MouseDown[0] == 1) {
        ImGui::GetIO().MouseDelta = { 0.0, 0.0 };
    }
    lastMouseButtonLeft = ImGui::GetIO().MouseDown[0];

    const auto& displaySize = ImGui::GetIO().DisplaySize;
    auto& style = ImGui::GetStyle();

    const auto sendButtonText = ICON_FA_PLAY_CIRCLE " Send";
    const double tShowKeyboard = 0.2f;
#if defined(IOS)
    const float statusBarHeight = displaySize.x < displaySize.y ? 2.0f*style.ItemSpacing.y : 0.1f;
#else
    const float statusBarHeight = 0.1f;
#endif
    const float menuButtonHeight = 24.0f + 2.0f*style.ItemSpacing.y;

    const auto & mouse_delta = ImGui::GetIO().MouseDelta;

    ImGui::SetNextWindowPos({ 0, 0, });
    ImGui::SetNextWindowSize(displaySize);
    ImGui::Begin("Main", nullptr,
                 ImGuiWindowFlags_NoMove |
                 ImGuiWindowFlags_NoTitleBar |
                 ImGuiWindowFlags_NoScrollbar |
                 ImGuiWindowFlags_NoResize |
                 ImGuiWindowFlags_NoSavedSettings);

    ImGui::InvisibleButton("StatusBar", { ImGui::GetContentRegionAvailWidth(), statusBarHeight });

    if (ImGui::ButtonSelectable(ICON_FA_COGS, { menuButtonHeight, menuButtonHeight }, windowId == WindowId::Settings )) {
        windowId = WindowId::Settings;
    }
    ImGui::SameLine();

    {
        auto posSave = ImGui::GetCursorScreenPos();
        if (ImGui::ButtonSelectable(ICON_FA_COMMENT_ALT "  Messages", { 0.35f*ImGui::GetContentRegionAvailWidth(), menuButtonHeight }, windowId == WindowId::Messages)) {
            windowId = WindowId::Messages;
        }
        auto radius = 0.3f*ImGui::GetTextLineHeight();
        posSave.x += 2.0f*radius;
        posSave.y += 2.0f*radius;
        if (hasNewMessages) {
            ImGui::GetWindowDrawList()->AddCircleFilled(posSave, radius, ImGui::ColorConvertFloat4ToU32({ 1.0f, 0.0f, 0.0f, 1.0f }), 16);
        }
    }
    ImGui::SameLine();

    if (!hasFileSharingSupport) {
        ImGui::ButtonDisabled(ICON_FA_FILE "  Files", { 0.40f*ImGui::GetContentRegionAvailWidth(), menuButtonHeight });
        if (ImGui::IsItemHovered()) {
            ImGui::BeginTooltip();
            ImGui::Text("File sharing is not supported on this platform!");
            ImGui::EndTooltip();
        }
    } else if (ImGui::ButtonSelectable(ICON_FA_FILE "  Files", { 0.40f*ImGui::GetContentRegionAvailWidth(), menuButtonHeight }, windowId == WindowId::Files)) {
        windowId = WindowId::Files;
    }
    ImGui::SameLine();

    {
        auto posSave = ImGui::GetCursorScreenPos();
        if (ImGui::ButtonSelectable(ICON_FA_SIGNAL "  Spectrum", { 1.0f*ImGui::GetContentRegionAvailWidth(), menuButtonHeight }, windowId == WindowId::Spectrum)) {
            windowId = WindowId::Spectrum;
        }
        auto radius = 0.3f*ImGui::GetTextLineHeight();
        posSave.x += 2.0f*radius;
        posSave.y += 2.0f*radius;
        ImGui::GetWindowDrawList()->AddCircleFilled(posSave, radius, hasAudioCaptureData ? ImGui::ColorConvertFloat4ToU32({ 0.0f, 1.0f, 0.0f, 1.0f }) : ImGui::ColorConvertFloat4ToU32({ 1.0f, 0.0f, 0.0f, 1.0f }), 16);
    }

    if (windowId == WindowId::Settings) {
        ImGui::BeginChild("Settings:main", ImGui::GetContentRegionAvail(), true);
        ImGui::Text("%s", "");
        ImGui::Text("%s", "");
        ImGui::Text("Waver v1.4.0");
        ImGui::Separator();

        ImGui::Text("%s", "");
        ImGui::Text("Sample rate (capture):  %g, %d B/sample", g_ggWave->getSampleRateInp(), g_ggWave->getSampleSizeBytesInp());
        ImGui::Text("Sample rate (playback): %g, %d B/sample", g_ggWave->getSampleRateOut(), g_ggWave->getSampleSizeBytesOut());

        const float kLabelWidth = ImGui::CalcTextSize("Fixed-length:  ").x;

        // volume
        ImGui::Text("%s", "");
        {
            auto posSave = ImGui::GetCursorScreenPos();
            ImGui::Text("%s", "");
            ImGui::SetCursorScreenPos({ posSave.x + kLabelWidth, posSave.y });
            if (settings.volume < 0.2f) {
                ImGui::TextColored({ 0.0f, 1.0f, 0.0f, 0.5f }, "Normal volume");
            } else if (settings.volume < 0.5f) {
                ImGui::TextColored({ 1.0f, 1.0f, 0.0f, 0.5f }, "Intermediate volume");
            } else {
                ImGui::TextColored({ 1.0f, 0.0f, 0.0f, 0.5f }, "Warning: high volume!");
            }
        }
        {
            auto posSave = ImGui::GetCursorScreenPos();
            ImGui::Text("Volume: ");
            ImGui::SetCursorScreenPos({ posSave.x + kLabelWidth, posSave.y });
        }
        {
            auto p0 = ImGui::GetCursorScreenPos();

            {
                auto & cols = ImGui::GetStyle().Colors;
                ImGui::PushStyleColor(ImGuiCol_FrameBg, cols[ImGuiCol_WindowBg]);
                ImGui::PushStyleColor(ImGuiCol_FrameBgActive, cols[ImGuiCol_WindowBg]);
                ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, cols[ImGuiCol_WindowBg]);
                ImGui::SliderFloat("##volume", &settings.volume, 0.0f, 1.0f);
                ImGui::PopStyleColor(3);
            }

            auto posSave = ImGui::GetCursorScreenPos();
            ImGui::SameLine();
            auto p1 = ImGui::GetCursorScreenPos();
            p1.x -= ImGui::CalcTextSize(" ").x;
            p1.y += ImGui::GetTextLineHeightWithSpacing() + 0.5f*style.ItemInnerSpacing.y;
            ImGui::GetWindowDrawList()->AddRectFilledMultiColor(
                    p0, { 0.35f*(p0.x + p1.x), p1.y },
                    ImGui::ColorConvertFloat4ToU32({0.0f, 1.0f, 0.0f, 0.5f}),
                    ImGui::ColorConvertFloat4ToU32({1.0f, 1.0f, 0.0f, 0.3f}),
                    ImGui::ColorConvertFloat4ToU32({1.0f, 1.0f, 0.0f, 0.3f}),
                    ImGui::ColorConvertFloat4ToU32({0.0f, 1.0f, 0.0f, 0.5f})
                    );
            ImGui::GetWindowDrawList()->AddRectFilledMultiColor(
                    { 0.35f*(p0.x + p1.x), p0.y }, p1,
                    ImGui::ColorConvertFloat4ToU32({1.0f, 1.0f, 0.0f, 0.3f}),
                    ImGui::ColorConvertFloat4ToU32({1.0f, 0.0f, 0.0f, 0.5f}),
                    ImGui::ColorConvertFloat4ToU32({1.0f, 0.0f, 0.0f, 0.5f}),
                    ImGui::ColorConvertFloat4ToU32({1.0f, 1.0f, 0.0f, 0.3f})
                    );
            ImGui::SetCursorScreenPos(posSave);
        }

        // fixed-length
        ImGui::Text("%s", "");
        {
            auto posSave = ImGui::GetCursorScreenPos();
            ImGui::Text("Fixed-length: ");
            ImGui::SetCursorScreenPos({ posSave.x + kLabelWidth, posSave.y });
        }
        if (ImGui::Checkbox("##fixed-length", &settings.isFixedLength)) {
            g_buffer.inputUI.update = true;
            g_buffer.inputUI.reinit = true;
            g_buffer.inputUI.isSampleRateOffset = settings.isSampleRateOffset;
            g_buffer.inputUI.payloadLength = settings.isFixedLength ? settings.payloadLength : -1;
        } else {
            g_buffer.inputUI.reinit = false;
            g_buffer.inputUI.isSampleRateOffset = false;
        }

        if (settings.isFixedLength) {
            ImGui::SameLine();
            ImGui::PushItemWidth(0.5*ImGui::GetContentRegionAvailWidth());
            ImGui::SliderInt("Bytes", &settings.payloadLength, 1, 16);
            ImGui::PopItemWidth();
            ImGui::SameLine();
            if (ImGui::Checkbox("Offset", &settings.isSampleRateOffset)) {
                g_buffer.inputUI.update = true;
                g_buffer.inputUI.reinit = true;
                g_buffer.inputUI.isSampleRateOffset = settings.isSampleRateOffset;
                g_buffer.inputUI.payloadLength = settings.isFixedLength ? settings.payloadLength : -1;
            }
        }

        // protocol
        ImGui::Text("%s", "");
        {
            auto posSave = ImGui::GetCursorScreenPos();
            ImGui::Text("%s", "");
            ImGui::SetCursorScreenPos({ posSave.x + kLabelWidth, posSave.y });
            ImGui::TextDisabled("[U] = ultrasound");
        }
        {
            auto posSave = ImGui::GetCursorScreenPos();
            ImGui::Text("%s", "");
            ImGui::SetCursorScreenPos({ posSave.x + kLabelWidth, posSave.y });
            ImGui::TextDisabled("[DT] = dual-tone");
        }
        {
            auto posSave = ImGui::GetCursorScreenPos();
            ImGui::Text("Tx Protocol: ");
            ImGui::SetCursorScreenPos({ posSave.x + kLabelWidth, posSave.y });
        }
        if (ImGui::BeginCombo("##protocol", g_ggWave->getTxProtocol(settings.protocolId).name)) {
            for (int i = 0; i < (int) g_ggWave->getTxProtocols().size(); ++i) {
                const bool isSelected = (settings.protocolId == i);
                if (ImGui::Selectable(g_ggWave->getTxProtocol(i).name, isSelected)) {
                    settings.protocolId = i;
                }

                if (isSelected) {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }

        ImGui::EndChild();
    }

    if (windowId == WindowId::Messages) {
        const float messagesInputHeight = 2*ImGui::GetTextLineHeightWithSpacing();
        const float messagesHistoryHeigthMax = ImGui::GetContentRegionAvail().y - messagesInputHeight - 2.0f*style.ItemSpacing.x;
        float messagesHistoryHeigth = messagesHistoryHeigthMax;

        hasNewMessages = false;

        // no automatic screen resize support for iOS
#if defined(IOS) || defined(ANDROID)
        if (displaySize.x < displaySize.y) {
            if (isTextInput) {
                messagesHistoryHeigth -= 0.5f*messagesHistoryHeigthMax*std::min(tShowKeyboard, ImGui::GetTime() - tStartInput) / tShowKeyboard;
            } else {
                messagesHistoryHeigth -= 0.5f*messagesHistoryHeigthMax - 0.5f*messagesHistoryHeigthMax*std::min(tShowKeyboard, ImGui::GetTime() - tEndInput) / tShowKeyboard;
            }
        } else {
            if (isTextInput) {
                messagesHistoryHeigth -= 0.5f*displaySize.y*std::min(tShowKeyboard, ImGui::GetTime() - tStartInput) / tShowKeyboard;
            } else {
                messagesHistoryHeigth -= 0.5f*displaySize.y - 0.5f*displaySize.y*std::min(tShowKeyboard, ImGui::GetTime() - tEndInput) / tShowKeyboard;
            }
        }
#endif

        bool showScrollToBottom = false;
        const auto wPos0 = ImGui::GetCursorScreenPos();
        const auto wSize = ImVec2 { ImGui::GetContentRegionAvailWidth(), messagesHistoryHeigth };

        ImGui::BeginChild("Messages:history", wSize, true, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

        static bool isHoldingMessage = false;
        static bool isHoldingInput = false;
        static int messageIdHolding = 0;

        const float tMessageFlyIn = 0.3f;

        // we need this because we push messages in the next loop
        if (messageHistory.capacity() == messageHistory.size()) {
            messageHistory.reserve(messageHistory.size() + 16);
        }

        for (int i = 0; i < (int) messageHistory.size(); ++i) {
            ImGui::PushID(i);
            const auto & message = messageHistory[i];
            const float tRecv = 0.001f*std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - message.timestamp).count();
            const float interp = std::min(tRecv, tMessageFlyIn)/tMessageFlyIn;
            const float xoffset = std::max(0.0f, (1.0f - interp)*ImGui::GetContentRegionAvailWidth());

            if (xoffset > 0.0f) {
                ImGui::Indent(xoffset);
            } else {
                ImGui::PushTextWrapPos();
            }

            const auto msgStatus = message.received ? "Recv" : "Send";
            const auto msgColor = message.received ? ImVec4 { 0.0f, 1.0f, 0.0f, interp } : ImVec4 { 1.0f, 1.0f, 0.0f, interp };

            ImGui::TextDisabled("%s |", ::toTimeString(message.timestamp));
            ImGui::SameLine();
            ImGui::TextColored(msgColor, "%s", msgStatus);
            ImGui::SameLine();
            ImGui::TextDisabled("|");
            ImGui::SameLine();
            ImGui::TextColored({ 1.0f, 0.2f, 0.9f, interp }, "%s", g_ggWave->getTxProtocol(message.protocolId).name);
            ImGui::SameLine();
            ImGui::TextDisabled("|");

            {
                auto p0 = ImGui::GetCursorScreenPos();
                auto p00 = p0;
                p00.y -= ImGui::GetTextLineHeightWithSpacing();
                p0.x -= style.ItemSpacing.x;
                p0.y -= 0.5f*style.ItemSpacing.y;

                switch (message.type) {
                    case Message::Error:
                        {
                            auto col = ImVec4 { 1.0f, 0.0f, 0.0f, 1.0f };
                            col.w = interp;
                            ImGui::TextColored(col, "Failed to receive");
                        }
                        break;
                    case Message::Text:
                        {
                            auto col = style.Colors[ImGuiCol_Text];
                            col.w = interp;
                            ImGui::TextColored(col, "%s", message.data.c_str());
                        }
                        break;
                    case Message::FileBroadcast:
                        {
                            auto col = ImVec4 { 0.0f, 1.0f, 1.0f, 1.0f };
                            col.w = interp;
                            auto broadcastInfo = parseBroadcastInfo(message.data);
                            ImGui::TextColored(col, "-=[ File Broadcast from %s:%d ]=-", broadcastInfo.ip.c_str(), broadcastInfo.port);
                        }
                        break;
                }

                auto p1 = ImGui::GetCursorScreenPos();
                p1.x = p00.x + ImGui::GetContentRegionAvailWidth();

                if (xoffset == 0.0f) {
                    if (ImGui::IsMouseHoveringRect(p00, p1, true)) {
                        auto col = ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled);
                        col.w *= ImGui::GetIO().MouseDownDuration[0] > tHoldContextPopup ? 0.25f : 0.10f;
                        ImGui::GetWindowDrawList()->AddRectFilled(p0, p1, ImGui::ColorConvertFloat4ToU32(col), 12.0f);

                        if (ImGui::GetIO().MouseDownDuration[0] > tHoldContextPopup) {
                            isHoldingMessage = true;
                            messageIdHolding = i;
                        }
                    }
                }
            }

            if (xoffset == 0.0f) {
                ImGui::PopTextWrapPos();
            }
            ImGui::Text("%s", "");
            ImGui::PopID();
        }

        if (ImGui::IsMouseReleased(0) && isHoldingMessage) {
            auto pos = ImGui::GetMousePos();
            pos.x -= 1.0f*ImGui::CalcTextSize("Resend | Copy").x;
            pos.y -= 1.0f*ImGui::GetTextLineHeightWithSpacing();
            ImGui::SetNextWindowPos(pos);

            ImGui::OpenPopup("Message options");
            isHoldingMessage = false;
        }

        if (ImGui::BeginPopup("Message options")) {
            const auto & messageSelected = messageHistory[messageIdHolding];

            if (ImGui::ButtonDisablable("Resend", {}, messageSelected.type != Message::Text)) {
                g_buffer.inputUI.update = true;
                g_buffer.inputUI.message = { false, std::chrono::system_clock::now(), messageSelected.data, messageSelected.protocolId, settings.volume, Message::Text };

                messageHistory.push_back(g_buffer.inputUI.message);
                ImGui::CloseCurrentPopup();
            }

            ImGui::SameLine();
            ImGui::TextDisabled("|");

            ImGui::SameLine();
            if (ImGui::ButtonDisablable("Copy", {}, messageSelected.type != Message::Text)) {
                SDL_SetClipboardText(messageSelected.data.c_str());
                ImGui::CloseCurrentPopup();
            }

            if (messageSelected.type == Message::FileBroadcast) {
                ImGui::SameLine();
                ImGui::TextDisabled("|");

                ImGui::SameLine();
                if (ImGui::ButtonDisablable("Receive", {}, !messageSelected.received || messageSelected.type != Message::FileBroadcast || !hasFileSharingSupport)) {
                    auto broadcastInfo = parseBroadcastInfo(messageSelected.data);

                    g_remoteIP = broadcastInfo.ip;
                    g_remotePort = broadcastInfo.port;
                    g_hasRemoteInfo = true;

                    g_fileClient.disconnect();
                    g_hasReceivedFileInfos = false;
                    g_hasRequestedFileInfos = false;
                    g_hasReceivedFiles = false;

                    windowId = WindowId::Files;
                    subWindowId = SubWindowId::Receive;

                    ImGui::CloseCurrentPopup();
                }
            }

            ImGui::EndPopup();
        }

        if (scrollMessagesToBottom) {
            ImGui::SetScrollHereY();
            scrollMessagesToBottom = false;
        }

        if (ImGui::GetScrollY() < ImGui::GetScrollMaxY() - 10) {
            showScrollToBottom = true;
        }

        if (showScrollToBottom) {
            auto posSave = ImGui::GetCursorScreenPos();
            auto butSize = ImGui::CalcTextSize(ICON_FA_ARROW_CIRCLE_DOWN);
            ImGui::SetCursorScreenPos({ wPos0.x + wSize.x - 2.0f*butSize.x - 2*style.ItemSpacing.x, wPos0.y + wSize.y - 2.0f*butSize.y - 2*style.ItemSpacing.y });
            if (ImGui::Button(ICON_FA_ARROW_CIRCLE_DOWN)) {
                scrollMessagesToBottom = true;
            }
            ImGui::SetCursorScreenPos(posSave);
        }

        ScrollWhenDraggingOnVoid(ImVec2(0.0f, -mouse_delta.y), ImGuiMouseButton_Left);
        ImGui::EndChild();

        if (statsCurrent.isReceiving) {
            if (statsCurrent.isAnalyzing) {
                ImGui::TextColored({ 0.0f, 1.0f, 0.0f, 1.0f }, "Analyzing ...");
                ImGui::SameLine();
                ImGui::ProgressBar(1.0f - float(statsCurrent.framesLeftToAnalyze)/statsCurrent.framesToAnalyze,
                                   { ImGui::GetContentRegionAvailWidth(), ImGui::GetTextLineHeight() });
            } else {
                ImGui::TextColored({ 0.0f, 1.0f, 0.0f, 1.0f }, "Receiving ...");
                ImGui::SameLine();
                ImGui::ProgressBar(1.0f - float(statsCurrent.framesLeftToRecord)/statsCurrent.framesToRecord,
                                   { ImGui::GetContentRegionAvailWidth(), ImGui::GetTextLineHeight() });
            }
        } else {
            static float amax = 0.0f;
            static float frac = 0.0f;

            amax = 0.0f;
            frac = (ImGui::GetTime() - tStartTx)/tLengthTx;

            if (txAmplitudeDataCurrent.size() && frac <= 1.0f) {
                struct Funcs {
                    static float Sample(void * data, int i) {
                        auto res = std::fabs(((int16_t *)(data))[i]) ;
                        if (res > amax) amax = res;
                        return res;
                    }

                    static float SampleFrac(void * data, int i) {
                        if (i > frac*txAmplitudeDataCurrent.size()) {
                            return 0.0f;
                        }
                        return std::fabs(((int16_t *)(data))[i]);
                    }
                };

                auto posSave = ImGui::GetCursorScreenPos();
                auto wSize = ImGui::GetContentRegionAvail();
                wSize.x = ImGui::GetContentRegionAvailWidth() - ImGui::CalcTextSize(sendButtonText).x - 2*style.ItemSpacing.x;
                wSize.y = ImGui::GetTextLineHeight();

                ImGui::PushStyleColor(ImGuiCol_FrameBg, { 0.3f, 0.3f, 0.3f, 0.3f });
                ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
                ImGui::PlotHistogram("##plotSpectrumCurrent",
                                     Funcs::Sample,
                                     txAmplitudeDataCurrent.data(),
                                     (int) txAmplitudeDataCurrent.size(), 0,
                                     (std::string("")).c_str(),
                                     0.0f, FLT_MAX, wSize);
                ImGui::PopStyleColor(2);

                ImGui::SetCursorScreenPos(posSave);

                ImGui::PushStyleColor(ImGuiCol_FrameBg, { 0.0f, 0.0f, 0.0f, 0.0f });
                ImGui::PushStyleColor(ImGuiCol_PlotHistogram, { 0.0f, 1.0f, 0.0f, 1.0f });
                ImGui::PlotHistogram("##plotSpectrumCurrent",
                                     Funcs::SampleFrac,
                                     txAmplitudeDataCurrent.data(),
                                     (int) txAmplitudeDataCurrent.size(), 0,
                                     (std::string("")).c_str(),
                                     0.0f, amax, wSize);
                ImGui::PopStyleColor(2);
            } else {
                ImGui::TextDisabled("Listening for waves ...\n");
            }
        }

        if (doInputFocus) {
            ImGui::SetKeyboardFocusHere();
            doInputFocus = false;
        }

        doSendMessage = false;
        ImGui::PushItemWidth(ImGui::GetContentRegionAvailWidth() - ImGui::CalcTextSize(sendButtonText).x - 2*style.ItemSpacing.x);
        if (ImGui::InputText("##Messages:Input", inputBuf, kMaxInputSize, ImGuiInputTextFlags_EnterReturnsTrue)) {
            doSendMessage = true;
        }

        ImGui::PopItemWidth();
        if (ImGui::IsItemActive() && isTextInput == false) {
            SDL_StartTextInput();
            isTextInput = true;
            tStartInput = ImGui::GetTime();
        }
        bool requestStopTextInput = false;
        if (ImGui::IsItemDeactivated()) {
            requestStopTextInput = true;
        }

        if (isTextInput) {
            if (ImGui::IsItemHovered() && ImGui::GetIO().MouseDownDuration[0] > tHoldContextPopup) {
                isHoldingInput = true;
            }
        }

        if (ImGui::IsMouseReleased(0) && isHoldingInput) {
            auto pos = ImGui::GetMousePos();
            pos.x -= 2.0f*ImGui::CalcTextSize("Paste").x;
            pos.y -= 1.0f*ImGui::GetTextLineHeightWithSpacing();
            ImGui::SetNextWindowPos(pos);

            ImGui::OpenPopup("Input options");
            isHoldingInput = false;
        }

        if (ImGui::BeginPopup("Input options")) {
            if (ImGui::Button("Paste")) {
                for (int i = 0; i < kMaxInputSize; ++i) inputBuf[i] = 0;
                strncpy(inputBuf, SDL_GetClipboardText(), kMaxInputSize - 1);
                ImGui::CloseCurrentPopup();
            }

            ImGui::EndPopup();
        }

        ImGui::SameLine();
        {
            auto posCur = ImGui::GetCursorScreenPos();
            posCur.y -= ImGui::GetTextLineHeightWithSpacing();
            ImGui::SetCursorScreenPos(posCur);
        }
        if ((ImGui::Button(sendButtonText, { 0, 2*ImGui::GetTextLineHeightWithSpacing() }) || doSendMessage)) {
            if (inputBuf[0] == 0) {
                strncpy(inputBuf, lastInput.data(), kMaxInputSize - 1);
            }
            if (inputBuf[0] != 0) {
                lastInput = std::string(inputBuf);
                g_buffer.inputUI.update = true;
                g_buffer.inputUI.message = { false, std::chrono::system_clock::now(), std::string(inputBuf), settings.protocolId, settings.volume, Message::Text };

                messageHistory.push_back(g_buffer.inputUI.message);

                inputBuf[0] = 0;
                doInputFocus = true;
                scrollMessagesToBottom = true;
            }
        }
        if (!ImGui::IsItemHovered() && requestStopTextInput) {
            SDL_StopTextInput();
            isTextInput = false;
            tEndInput = ImGui::GetTime();
        }
    }

    if (windowId == WindowId::Files) {
        const float subWindowButtonHeight = menuButtonHeight;

        if (ImGui::ButtonSelectable("Send", { 0.50f*ImGui::GetContentRegionAvailWidth(), subWindowButtonHeight }, subWindowId == SubWindowId::Send)) {
            subWindowId = SubWindowId::Send;
        }
        ImGui::SameLine();

        if (ImGui::ButtonSelectable("Receive", { 1.0f*ImGui::GetContentRegionAvailWidth(), subWindowButtonHeight }, subWindowId == SubWindowId::Receive)) {
            subWindowId = SubWindowId::Receive;
        }

        switch (subWindowId) {
            case SubWindowId::Send:
                {
                    const float statusWindowHeight = 2*style.ItemInnerSpacing.y + 4*ImGui::GetTextLineHeightWithSpacing();

                    bool hasAtLeastOneFile = false;
                    {
                        const auto wSize = ImVec2 { ImGui::GetContentRegionAvailWidth(), ImGui::GetContentRegionAvail().y - subWindowButtonHeight - statusWindowHeight - 2*style.ItemInnerSpacing.y };

                        ImGui::BeginChild("Files:Send:fileInfos", wSize, true, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

                        auto fileInfos = g_fileServer.getFileInfos();
                        for (const auto & fileInfo : fileInfos) {
                            hasAtLeastOneFile = true;
                            ImGui::PushID(fileInfo.first);
                            ImGui::Text("File: '%s' (%4.2f MB)\n", fileInfo.second.filename.c_str(), float(fileInfo.second.filesize)/1024.0f/1024.0f);
                            if (ImGui::Button(ICON_FA_SHARE_ALT "  Share")) {
                                g_shareInfo.uri = fileInfo.second.uri;
                                g_shareInfo.filename = fileInfo.second.filename;
                                const auto & fileData = g_fileServer.getFileData(g_shareInfo.uri);
                                g_shareInfo.dataBuffer = fileData.data.data();
                                g_shareInfo.dataSize = fileData.data.size();
                                g_shareId++;
                            }
                            ImGui::SameLine();

#ifdef ANDROID
                            if (ImGui::Button(ICON_FA_EYE "  VIEW")) {
                                g_openInfo.uri = fileInfo.second.uri;
                                g_openInfo.filename = fileInfo.second.filename;
                                const auto & fileData = g_fileServer.getFileData(g_openInfo.uri);
                                g_openInfo.dataBuffer = fileData.data.data();
                                g_openInfo.dataSize = fileData.data.size();
                                g_openId++;
                            }
                            ImGui::SameLine();
#endif

                            if (ImGui::Button(ICON_FA_TRASH "  Clear")) {
                                g_deleteInfo.uri = fileInfo.second.uri.data();
                                g_deleteInfo.filename = fileInfo.second.filename.data();
                                g_deleteId++;
                            }

                            ImGui::PopID();
                        }

                        ImGui::PushTextWrapPos();
                        if (hasAtLeastOneFile == false) {
                            ImGui::TextColored({ 1.0f, 1.0f, 0.0f, 1.0f }, "There are currently no files availble to share.");
#if defined(IOS) || defined(ANDROID)
                            ImGui::TextColored({ 1.0f, 1.0f, 0.0f, 1.0f }, "Share some files with this app to be able to broadcast them to nearby devices through sound.");
#else
                            ImGui::TextColored({ 1.0f, 1.0f, 0.0f, 1.0f }, "Drag and drop some files on this window to be able to broadcast them to nearby devices through sound.");
#endif
                        }
                        ImGui::PopTextWrapPos();

                        ScrollWhenDraggingOnVoid(ImVec2(-mouse_delta.x, -mouse_delta.y), ImGuiMouseButton_Left);
                        ImGui::EndChild();
                    }

                    {
                        const auto wSize = ImVec2 { ImGui::GetContentRegionAvailWidth(), ImGui::GetContentRegionAvail().y - subWindowButtonHeight - style.ItemInnerSpacing.y };

                        ImGui::BeginChild("Files:Send:clientInfos", wSize, true, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

                        if (g_fileServer.isListening() == false) {
                            ImGui::TextColored({ 1.0f, 1.0f, 0.0f, 1.0f }, "Not accepting new connections.");
                        } else {
                            ImGui::TextColored({ 0.0f, 1.0f, 0.0f, 1.0f }, "Accepting new connections at: %s:%d (%4.1f sec)",
                                               GGSock::Communicator::getLocalAddress().c_str(), g_fileServer.getParameters().listenPort, kBroadcastTime_sec - ImGui::GetTime() + g_tLastBroadcast);
                        }

                        auto clientInfos = g_fileServer.getClientInfos();
                        if (clientInfos.empty()) {
                            ImGui::Text("No peers currently connected ..");
                        } else {
                            for (const auto & clientInfo : clientInfos) {
                                ImGui::Text("Peer: %s\n", clientInfo.second.address.c_str());
                            }
                        }

                        ScrollWhenDraggingOnVoid(ImVec2(-mouse_delta.x, -mouse_delta.y), ImGuiMouseButton_Left);
                        ImGui::EndChild();
                    }

                    {
                        if (ImGui::Button("Broadcast", { 0.40f*ImGui::GetContentRegionAvailWidth(), subWindowButtonHeight })) {
                            g_buffer.inputUI.update = true;
                            g_buffer.inputUI.message = {
                                false,
                                std::chrono::system_clock::now(),
                                ::generateFileBroadcastMessage(),
                                settings.protocolId,
                                settings.volume,
                                Message::FileBroadcast
                            };

                            messageHistory.push_back(g_buffer.inputUI.message);

                            g_tLastBroadcast = ImGui::GetTime();
                            g_fileServer.startListening();
                        }
                        ImGui::SameLine();

                        if (ImGui::ButtonDisablable("Stop", { 0.50f*ImGui::GetContentRegionAvailWidth(), subWindowButtonHeight }, !g_fileServer.isListening())) {
                            g_fileServer.stopListening();
                        }
                        ImGui::SameLine();

                        if (ImGui::ButtonDisablable("Clear", { 1.0f*ImGui::GetContentRegionAvailWidth(), subWindowButtonHeight }, !hasAtLeastOneFile)) {
                            g_deleteInfo.uri = "###ALL-FILES###";
                            g_deleteInfo.filename = "";
                            g_deleteId++;
                        }
                    }
                }
                break;
            case SubWindowId::Receive:
                {
                    const float statusWindowHeight = 2*style.ItemInnerSpacing.y + 4*ImGui::GetTextLineHeightWithSpacing();
                    {
                        const auto wSize = ImVec2 { ImGui::GetContentRegionAvailWidth(), ImGui::GetContentRegionAvail().y - subWindowButtonHeight - statusWindowHeight - 2*style.ItemInnerSpacing.y };

                        ImGui::BeginChild("Files:Receive:fileInfos", wSize, true, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

                        int toErase = -1;

                        auto fileInfos = g_receivedFileInfos;
                        for (const auto & fileInfo : fileInfos) {
                            ImGui::PushID(fileInfo.first);
                            ImGui::Text("File: '%s' (%4.2f MB)\n", fileInfo.second.filename.c_str(), float(fileInfo.second.filesize)/1024.0f/1024.0f);

                            const auto & uri = fileInfo.second.uri;
                            auto & fileInfoExtended = g_receivedFileInfosExtended[uri];

                            const bool isReceived = fileInfo.second.nChunks == fileInfoExtended.nReceivedChunks;

                            if (isReceived) {
                                if (fileInfoExtended.requestToShare == false) {
                                    if (ImGui::Button(ICON_FA_FOLDER "  To Send")) {
                                        fileInfoExtended.requestToShare = true;
                                        g_receivedId++;
                                    }
                                }

                                if (fileInfoExtended.readyToShare) {
                                    ImGui::TextColored({ 0.0f, 1.0f, 0.0f, 1.0f }, "Ready to share!");
                                }
                            } else if (g_fileClient.isConnected() && (fileInfoExtended.isReceiving || fileInfoExtended.nReceivedChunks > 0)) {
                                if (fileInfoExtended.isReceiving) {
                                    if (ImGui::Button(ICON_FA_PAUSE "  Pause")) {
                                        fileInfoExtended.isReceiving = false;
                                    }
                                } else {
                                    if (ImGui::Button(ICON_FA_PLAY "  Resume")) {
                                        fileInfoExtended.isReceiving = true;
                                    }
                                }

                                ImGui::SameLine();
                                ImGui::ProgressBar(float(fileInfoExtended.nReceivedChunks)/fileInfo.second.nChunks);
                            } else if (g_fileClient.isConnected()) {
                                if (ImGui::Button(ICON_FA_DOWNLOAD "  Receive")) {
                                    fileInfoExtended.isReceiving = true;
                                    fileInfoExtended.isChunkReceived.resize(fileInfo.second.nChunks);
                                    fileInfoExtended.isChunkRequested.resize(fileInfo.second.nChunks);
                                }
                            } else {
                                ImGui::Text("%s", "");
                            }

                            if ((fileInfoExtended.isReceiving == false || isReceived) && fileInfoExtended.requestToShare == false) {
                                ImGui::SameLine();
                                if (ImGui::Button(ICON_FA_TRASH "  Clear")) {
                                    toErase = fileInfo.first;
                                }
                            }

                            ImGui::PopID();
                        }

                        if (toErase != -1) {
                            g_receivedFiles.erase(g_receivedFileInfos[toErase].uri);
                            g_receivedFileInfosExtended.erase(g_receivedFileInfos[toErase].uri);
                            g_receivedFileInfos.erase(toErase);
                        }

                        ImGui::EndChild();
                    }

                    {
                        const auto wSize = ImVec2 { ImGui::GetContentRegionAvailWidth(), ImGui::GetContentRegionAvail().y - subWindowButtonHeight - style.ItemInnerSpacing.y };

                        ImGui::BeginChild("Files:Receive:status", wSize, true, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

                        ImGui::PushTextWrapPos();
                        if (g_hasRemoteInfo == false) {
                            ImGui::TextColored({ 1.0f, 0.0f, 0.0f, 1.0f }, "There is no broadcast offer selected yet.");
                            ImGui::TextColored({ 1.0f, 0.0f, 0.0f, 1.0f }, "Wait for a broadcast message to be received first.");
                        } else {
                            if (g_fileClient.isConnected()) {
                                ImGui::TextColored({ 0.0f, 1.0f, 0.0f, 1.0f }, "Successfully connected to peer:");
                            } else {
                                ImGui::TextColored({ 1.0f, 1.0f, 0.0f, 1.0f }, "Broadcast offer from the following peer:");
                            }
                            ImGui::Text("Remote IP:   %s", g_remoteIP.c_str());
                            ImGui::Text("Remote Port: %d", g_remotePort);
                            if (g_fileClient.isConnecting()) {
                                ImGui::TextColored({ 1.0f, 1.0f, 0.0f, 1.0f }, "Attempting to connect ...");
                            }
                        }

                        ImGui::PopTextWrapPos();

                        ScrollWhenDraggingOnVoid(ImVec2(-mouse_delta.x, -mouse_delta.y), ImGuiMouseButton_Left);
                        ImGui::EndChild();
                    }

                    {
                        if (g_fileClient.isConnecting() == false && g_fileClient.isConnected() == false) {
                            if (ImGui::ButtonDisablable("Connect", { 1.00f*ImGui::GetContentRegionAvailWidth(), subWindowButtonHeight }, !g_hasRemoteInfo)) {
                                g_fileClient.connect(g_remoteIP, g_remotePort, 0);
                            }
                        }

                        if (g_fileClient.isConnecting() || g_fileClient.isConnected()) {
                            if (ImGui::Button("Disconnect", { 1.00f*ImGui::GetContentRegionAvailWidth(), subWindowButtonHeight })) {
                                g_fileClient.disconnect();
                                g_hasReceivedFileInfos = false;
                                g_hasRequestedFileInfos = false;
                                g_hasReceivedFiles = false;
                            }
                        }
                    }
                }
                break;
        };
    }

    if (windowId == WindowId::Spectrum) {
        ImGui::BeginChild("Spectrum:main", ImGui::GetContentRegionAvail(), true);
        ImGui::PushTextWrapPos();
        {
            auto posSave = ImGui::GetCursorScreenPos();
            ImGui::Text("FPS: %4.2f\n", ImGui::GetIO().Framerate);
            ImGui::SetCursorScreenPos(posSave);
        }
        if (hasAudioCaptureData) {
            auto wSize = ImGui::GetContentRegionAvail();
            ImGui::PushStyleColor(ImGuiCol_FrameBg, { 0.3f, 0.3f, 0.3f, 0.3f });
            if (statsCurrent.isReceiving) {
                ImGui::PushStyleColor(ImGuiCol_PlotHistogram, { 1.0f, 0.0f, 0.0f, 1.0f });
            } else {
                ImGui::PushStyleColor(ImGuiCol_PlotHistogram, { 0.0f, 1.0f, 0.0f, 1.0f });
            }
            ImGui::PlotHistogram("##plotSpectrumCurrent",
                                 spectrumCurrent.data() + 30,
                                 g_ggWave->getSamplesPerFrame()/2 - 30, 0,
                                 (std::string("Current Spectrum")).c_str(),
                                 0.0f, FLT_MAX, wSize);
            ImGui::PopStyleColor(2);
        } else {
            ImGui::Text("%s", "");
            ImGui::TextColored({ 1.0f, 0.0f, 0.0f, 1.0f }, "No capture data available!");
            ImGui::TextColored({ 1.0f, 0.0f, 0.0f, 1.0f }, "Please make sure you have allowed microphone access for this app.");
        }
        ImGui::PopTextWrapPos();
        ImGui::EndChild();
    }

    ImGui::End();

    ImGui::GetIO().KeysDown[ImGui::GetIO().KeyMap[ImGuiKey_Backspace]] = false;
    ImGui::GetIO().KeysDown[ImGui::GetIO().KeyMap[ImGuiKey_Enter]] = false;

    {
        std::lock_guard<std::mutex> lock(g_buffer.mutex);
        if (g_buffer.inputUI.update) {
            g_buffer.inputCore = std::move(g_buffer.inputUI);
            g_buffer.inputUI.update = false;
        }
    }
}

void deinitMain() {
    g_isRunning = false;
}
