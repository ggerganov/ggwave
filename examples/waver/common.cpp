#include "common.h"

#include "ggwave-common.h"

#include "ggwave/ggwave.h"

#include "ggsock/communicator.h"
#include "ggsock/file-server.h"
#include "ggsock/serialization.h"

#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>

#include <SDL.h>

#include <array>
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

bool ScrollWhenDraggingOnVoid(const ImVec2& delta, ImGuiMouseButton mouse_button) {
    ImGuiContext& g = *ImGui::GetCurrentContext();
    ImGuiWindow* window = g.CurrentWindow;
    bool hovered = false;
    bool held = false;
    bool dragging = false;
    ImGuiButtonFlags button_flags = (mouse_button == 0) ? ImGuiButtonFlags_MouseButtonLeft : (mouse_button == 1) ? ImGuiButtonFlags_MouseButtonRight : ImGuiButtonFlags_MouseButtonMiddle;
    if (g.HoveredId == 0) // If nothing hovered so far in the frame (not same as IsAnyItemHovered()!)
        ImGui::ButtonBehavior(window->Rect(), window->GetID("##scrolldraggingoverlay"), &hovered, &held, button_flags);
    if (held && delta.x != 0.0f) {
        ImGui::SetScrollX(window, window->Scroll.x + delta.x);
    }
    if (held && delta.y != 0.0f) {
        ImGui::SetScrollY(window, window->Scroll.y + delta.y);
        dragging = true;
    }
    return dragging;
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
    bool dss;
    float volume;
    Type type;
};

struct GGWaveStats {
    bool receiving;
    bool analyzing;
    int framesToRecord;
    int framesLeftToRecord;
    int framesToAnalyze;
    int framesLeftToAnalyze;
    int samplesPerFrame;
    float sampleRateInp;
    float sampleRateOut;
    float sampleRate;
    int sampleSizeInp;
    int sampleSizeOut;
};

struct State {
    bool update = false;

    struct Flags {
        bool newMessage = false;
        bool newSpectrum = false;
        bool newTxAmplitude = false;
        bool newStats = false;
        bool newTxProtocols = false;

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
            dst.rxSpectrum = std::move(this->rxSpectrum);
        }

        if (this->flags.newTxAmplitude) {
            dst.update = true;
            dst.flags.newTxAmplitude = true;
            dst.txAmplitude.assign(this->txAmplitude);
        }

        if (this->flags.newStats) {
            dst.update = true;
            dst.flags.newStats = true;
            dst.stats = std::move(this->stats);
        }

        if (this->flags.newTxProtocols) {
            dst.update = true;
            dst.flags.newTxProtocols = true;
            dst.txProtocols = std::move(this->txProtocols);
        }

        flags.clear();
        update = false;
    }

    Message message;

    std::vector<float>   rxSpectrum;
    GGWave::Amplitude    rxAmplitude;
    GGWave::AmplitudeI16 txAmplitude;

    GGWaveStats stats;
    GGWave::TxProtocols txProtocols;
};

struct Input {
    bool update = true;

    struct Flags {
        bool newMessage = false;
        bool needReinit = false;
        bool changeNeedSpectrum = false;
        bool stopReceiving = false;
        bool changeRxProtocols = false;

        void clear() { memset(this, 0, sizeof(Flags)); }
    } flags;

    void apply(Input & dst) {
        if (update == false) return;

        if (this->flags.newMessage) {
            dst.update = true;
            dst.flags.newMessage = true;
            dst.message = std::move(this->message);
        }

        if (this->flags.needReinit) {
            dst.update = true;
            dst.flags.needReinit = true;
            dst.sampleRateOffset = std::move(this->sampleRateOffset);
            dst.freqStartShift = std::move(this->freqStartShift);
            dst.payloadLength = std::move(this->payloadLength);
            dst.directSequenceSpread = std::move(this->directSequenceSpread);
        }

        if (this->flags.changeNeedSpectrum) {
            dst.update = true;
            dst.flags.changeNeedSpectrum = true;
            dst.needSpectrum = std::move(this->needSpectrum);
        }

        if (this->flags.stopReceiving) {
            dst.update = true;
            dst.flags.stopReceiving = true;
        }

        if (this->flags.changeRxProtocols) {
            dst.update = true;
            dst.flags.changeRxProtocols = true;
            dst.rxProtocols = std::move(this->rxProtocols);
        }

        flags.clear();
        update = false;
    }

    // message
    Message message;

    // reinit
    float sampleRateOffset = 0;
    int freqStartShift = 0;
    int payloadLength = -1;

    // spectrum
    bool needSpectrum = false;

    // other
    bool directSequenceSpread = false;

    // rx protocols
    GGWave::RxProtocols rxProtocols;
};

struct Buffer {
    std::mutex mutex;

    State stateCore;
    Input inputCore;

    State stateUI;
    Input inputUI;
};

std::atomic<bool> g_isRunning;
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
    bool receiving = false;
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

    static bool isFirstCall = true;
    static bool needSpectrum = false;
    static int rxDataLengthLast = 0;
    static float rxTimestampLast = 0.0f;
    static GGWave::TxRxData rxDataLast;

    auto ggWave = GGWave_instance();
    if (ggWave == nullptr) {
        return;
    }

    {
        std::lock_guard<std::mutex> lock(g_buffer.mutex);
        g_buffer.inputCore.apply(inputCurrent);
    }

    if (inputCurrent.update) {
        if (inputCurrent.flags.newMessage) {
            int n = (int) inputCurrent.message.data.size();

            if (ggWave->init(
                    n, inputCurrent.message.data.data(),
                    GGWave::TxProtocolId(inputCurrent.message.protocolId),
                    100*inputCurrent.message.volume) == false) {
                g_buffer.stateCore.update = true;
                g_buffer.stateCore.flags.newMessage = true;
                g_buffer.stateCore.message = {
                    false,
                    std::chrono::system_clock::now(),
                    (GGWave::Protocols::tx()[inputCurrent.message.protocolId].extra == 2 && inputCurrent.payloadLength <= 0) ?
                        "MT protocols require fixed-length mode" : "Failed to transmit",
                    ggWave->rxProtocolId(),
                    ggWave->isDSSEnabled(),
                    0,
                    Message::Error,
                };
            }
        }

        if (inputCurrent.flags.needReinit) {
            static auto sampleRateInpOld = ggWave->sampleRateInp();
            static auto sampleRateOutOld = ggWave->sampleRateOut();
            static auto freqStartShiftOld = 0;

            auto sampleFormatInpOld = ggWave->sampleFormatInp();
            auto sampleFormatOutOld = ggWave->sampleFormatOut();
            auto rxProtocolsOld = ggWave->rxProtocols();

            for (int i = 0; i < GGWAVE_PROTOCOL_COUNT; ++i) {
                GGWave::Protocols::tx()[i].freqStart = std::max(1, GGWave::Protocols::tx()[i].freqStart + inputCurrent.freqStartShift - freqStartShiftOld);
                rxProtocolsOld[i].freqStart = std::max(1, rxProtocolsOld[i].freqStart + inputCurrent.freqStartShift - freqStartShiftOld);
            }

            freqStartShiftOld = inputCurrent.freqStartShift;

            GGWave::OperatingMode mode = GGWAVE_OPERATING_MODE_RX_AND_TX;
            if (inputCurrent.directSequenceSpread) mode |= GGWAVE_OPERATING_MODE_USE_DSS;

            GGWave::Parameters parameters {
                inputCurrent.payloadLength,
                sampleRateInpOld,
                sampleRateOutOld + inputCurrent.sampleRateOffset,
                GGWave::kDefaultSampleRate,
                GGWave::kDefaultSamplesPerFrame,
                GGWave::kDefaultSoundMarkerThreshold,
                sampleFormatInpOld,
                sampleFormatOutOld,
                mode,
            };

            GGWave_reset(&parameters);
            ggWave = GGWave_instance();

            ggWave->rxProtocols() = rxProtocolsOld;
        }

        if (inputCurrent.flags.changeNeedSpectrum) {
            needSpectrum = inputCurrent.needSpectrum;
        }

        if (inputCurrent.flags.stopReceiving) {
            ggWave->rxStopReceiving();
        }

        if (inputCurrent.flags.changeRxProtocols) {
            ggWave->rxProtocols() = inputCurrent.rxProtocols;
        }

        inputCurrent.flags.clear();
        inputCurrent.update = false;
    }

    GGWave_mainLoop();

    rxDataLengthLast = ggWave->rxTakeData(rxDataLast);
    if (rxDataLengthLast == -1) {
        g_buffer.stateCore.update = true;
        g_buffer.stateCore.flags.newMessage = true;
        g_buffer.stateCore.message = {
            true,
            std::chrono::system_clock::now(),
            "Failed to decode",
            ggWave->rxProtocolId(),
            ggWave->isDSSEnabled(),
            0,
            Message::Error,
        };
    } else if (rxDataLengthLast > 0 && ImGui::GetTime() - rxTimestampLast > 0.5f) {
        auto message = std::string((char *) rxDataLast.data(), rxDataLengthLast);

        const Message::Type type = isFileBroadcastMessage(message) ? Message::FileBroadcast : Message::Text;
        g_buffer.stateCore.update = true;
        g_buffer.stateCore.flags.newMessage = true;
        g_buffer.stateCore.message = {
            true,
            std::chrono::system_clock::now(),
            std::move(message),
            ggWave->rxProtocolId(),
            ggWave->isDSSEnabled(),
            0,
            type,
        };

        rxTimestampLast = ImGui::GetTime();
    }

    if (needSpectrum) {
        if (ggWave->rxTakeAmplitude(g_buffer.stateCore.rxAmplitude)) {
            static const int NMax = GGWave::kMaxSamplesPerFrame;
            static float tmp[2*NMax];

            const int N = ggWave->samplesPerFrame();
            ggWave->computeFFTR(g_buffer.stateCore.rxAmplitude.data(), tmp, N);

            g_buffer.stateCore.rxSpectrum.resize(N);
            for (int i = 0; i < N; ++i) {
                g_buffer.stateCore.rxSpectrum[i] = (tmp[2*i + 0]*tmp[2*i + 0] + tmp[2*i + 1]*tmp[2*i + 1]);
            }
            for (int i = 1; i < N/2; ++i) {
                g_buffer.stateCore.rxSpectrum[i] += g_buffer.stateCore.rxSpectrum[N - i];
            }

            g_buffer.stateCore.update = true;
            g_buffer.stateCore.flags.newSpectrum = true;
        }
    }

    if (ggWave->txTakeAmplitudeI16(g_buffer.stateCore.txAmplitude)) {
        g_buffer.stateCore.update = true;
        g_buffer.stateCore.flags.newTxAmplitude = true;
    }

    if (true) {
        g_buffer.stateCore.update = true;
        g_buffer.stateCore.flags.newStats = true;
        g_buffer.stateCore.stats.receiving           = ggWave->rxReceiving();
        g_buffer.stateCore.stats.analyzing           = ggWave->rxAnalyzing();
        g_buffer.stateCore.stats.framesToRecord      = ggWave->rxFramesToRecord();
        g_buffer.stateCore.stats.framesLeftToRecord  = ggWave->rxFramesLeftToRecord();
        g_buffer.stateCore.stats.framesToAnalyze     = ggWave->rxFramesToAnalyze();
        g_buffer.stateCore.stats.framesLeftToAnalyze = ggWave->rxFramesLeftToAnalyze();
        g_buffer.stateCore.stats.samplesPerFrame     = ggWave->samplesPerFrame();
        g_buffer.stateCore.stats.sampleRateInp       = ggWave->sampleRateInp();
        g_buffer.stateCore.stats.sampleRateOut       = ggWave->sampleRateOut();
        g_buffer.stateCore.stats.sampleRate          = GGWave::kDefaultSampleRate;
        g_buffer.stateCore.stats.sampleSizeInp       = ggWave->sampleSizeInp();
        g_buffer.stateCore.stats.sampleSizeOut       = ggWave->sampleSizeOut();
    }

    if (isFirstCall) {
        g_buffer.stateCore.update = true;
        g_buffer.stateCore.flags.newTxProtocols = true;
        g_buffer.stateCore.txProtocols = GGWave::Protocols::tx();

        isFirstCall = false;
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

                if (fileInfoExtended.receiving == false) {
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

    enum class SubWindowIdFiles {
        Send,
        Receive,
    };

    enum class SubWindowIdSpectrum {
        Spectrum,
        Spectrogram,
    };

    struct Settings {
        int protocolId = GGWAVE_PROTOCOL_AUDIBLE_FAST;
        bool isSampleRateOffset = false;
        float sampleRateOffset = -512.0f;
        bool isFreqStartShift = false;
        int freqStartShift = 48;
        bool isFixedLength = false;
        bool directSequenceSpread = false;
        int payloadLength = 16;
        float volume = 0.10f;

        GGWave::TxProtocols txProtocols;
        GGWave::RxProtocols rxProtocols;
    };

    static WindowId windowId = WindowId::Messages;
    static WindowId windowIdLast = windowId;
    static SubWindowIdFiles subWindowIdFiles = SubWindowIdFiles::Send;
    static SubWindowIdSpectrum subWindowIdSpectrum = SubWindowIdSpectrum::Spectrum;

    static Settings settings;

    const double tHoldContextPopup = 0.2f;

    const int kMaxInputSize = 140;
    static char inputBuf[kMaxInputSize];

    static bool doInputFocus = false;
    static bool doSendMessage = false;
    static bool mouseButtonLeftLast = 0;
    static bool isTextInput = false;
    static bool scrollMessagesToBottom = true;
    static bool hasAudioCaptureData = false;
    static bool hasNewMessages = false;
    static bool hasNewSpectrum = false;
    static bool showSpectrumSettings = true;
#ifdef __EMSCRIPTEN__
    static bool hasFileSharingSupport = false;
#else
    static bool hasFileSharingSupport = true;
#endif

#if defined(IOS) || defined(ANDROID)
    static double tStartInput = 0.0f;
    static double tEndInput = -100.0f;
#endif
    static double tStartTx = 0.0f;
    static double tLengthTx = 0.0f;

    static GGWaveStats statsCurrent;
    static std::vector<float> spectrumCurrent;
    static std::vector<int16_t> txAmplitudeCurrent;
    static std::vector<Message> messageHistory;
    static std::string inputLast = "";

    // keyboard shortcuts:
    if (ImGui::IsKeyPressed(62)) {
        printf("F5 pressed : clear message history\n");
        messageHistory.clear();
    }

    if (ImGui::IsKeyPressed(63)) {
        if (messageHistory.size() > 0) {
            printf("F6 pressed : delete last message\n");
            messageHistory.erase(messageHistory.end() - 1);
        }
    }

    if (stateCurrent.update) {
        if (stateCurrent.flags.newMessage) {
            scrollMessagesToBottom = true;
            messageHistory.push_back(std::move(stateCurrent.message));
            hasNewMessages = true;
        }
        if (stateCurrent.flags.newSpectrum) {
            spectrumCurrent = std::move(stateCurrent.rxSpectrum);
            hasNewSpectrum = true;
            hasAudioCaptureData = !spectrumCurrent.empty();
        }
        if (stateCurrent.flags.newTxAmplitude) {
            txAmplitudeCurrent.resize(stateCurrent.txAmplitude.size());
            std::copy(stateCurrent.txAmplitude.begin(), stateCurrent.txAmplitude.end(), txAmplitudeCurrent.begin());

            tStartTx = ImGui::GetTime() + (16.0f*1024.0f)/statsCurrent.sampleRateOut;
            tLengthTx = txAmplitudeCurrent.size()/statsCurrent.sampleRateOut;
            {
                auto & ampl = txAmplitudeCurrent;
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
        if (stateCurrent.flags.newTxProtocols) {
            settings.txProtocols = std::move(stateCurrent.txProtocols);
            settings.rxProtocols = settings.txProtocols;
        }
        stateCurrent.flags.clear();
        stateCurrent.update = false;
    }

    if (settings.txProtocols.empty()) {
        printf("No Tx Protocols available\n");
        return;
    }

    if (g_focusFileSend) {
        windowId = WindowId::Files;
        subWindowIdFiles = SubWindowIdFiles::Send;
        g_focusFileSend = false;
    }

    if (mouseButtonLeftLast == 0 && ImGui::GetIO().MouseDown[0] == 1) {
        ImGui::GetIO().MouseDelta = { 0.0, 0.0 };
    }
    mouseButtonLeftLast = ImGui::GetIO().MouseDown[0];

    const auto& displaySize = ImGui::GetIO().DisplaySize;
    auto& style = ImGui::GetStyle();

    const auto sendButtonText = ICON_FA_PLAY_CIRCLE " Send";
#if defined(IOS) || defined(ANDROID)
    const double tShowKeyboard = 0.2f;
#endif
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
            if (windowId == WindowId::Spectrum) {
                showSpectrumSettings = !showSpectrumSettings;
            }
            windowId = WindowId::Spectrum;
        }
        auto radius = 0.3f*ImGui::GetTextLineHeight();
        posSave.x += 2.0f*radius;
        posSave.y += 2.0f*radius;
        ImGui::GetWindowDrawList()->AddCircleFilled(posSave, radius, hasAudioCaptureData ? ImGui::ColorConvertFloat4ToU32({ 0.0f, 1.0f, 0.0f, 1.0f }) : ImGui::ColorConvertFloat4ToU32({ 1.0f, 0.0f, 0.0f, 1.0f }), 16);
    }

    if ((windowIdLast != windowId) || (hasAudioCaptureData == false)) {
        g_buffer.inputUI.update = true;
        g_buffer.inputUI.flags.changeNeedSpectrum = true;
        g_buffer.inputUI.needSpectrum = (windowId == WindowId::Spectrum) || (hasAudioCaptureData == false);

        windowIdLast = windowId;
    }

    if (windowId == WindowId::Settings) {
        ImGui::BeginChild("Settings:main", ImGui::GetContentRegionAvail(), true, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
        ImGui::Text("Waver v1.5.3");
        ImGui::Separator();

        ImGui::Text("%s", "");
        ImGui::Text("Sample rate (capture):  %g, %d B/sample", statsCurrent.sampleRateInp, statsCurrent.sampleSizeInp);
        ImGui::Text("Sample rate (playback): %g, %d B/sample", statsCurrent.sampleRateOut, statsCurrent.sampleSizeOut);

        const float kLabelWidth = ImGui::CalcTextSize("Inp. SR Offset:  ").x;

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

        // tx protocol
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
            ImGui::Text("%s", "");
            ImGui::SetCursorScreenPos({ posSave.x + kLabelWidth, posSave.y });
            ImGui::TextDisabled("[MT] = mono-tone");
        }
        {
            auto posSave = ImGui::GetCursorScreenPos();
            ImGui::Text("Tx Protocol: ");
            ImGui::SetCursorScreenPos({ posSave.x + kLabelWidth, posSave.y });
        }
        if (ImGui::BeginCombo("##txProtocol", settings.txProtocols[settings.protocolId].name)) {
            for (int i = 0; i < (int) settings.txProtocols.size(); ++i) {
                const auto & txProtocol = settings.txProtocols[i];
                if (txProtocol.name == nullptr) continue;
                const bool isSelected = (settings.protocolId == i);
                if (ImGui::Selectable(txProtocol.name, isSelected)) {
                    settings.protocolId = i;
                }

                if (isSelected) {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }

        {
            auto posSave = ImGui::GetCursorScreenPos();
            ImGui::Text("Bandwidth: ");
            ImGui::SetCursorScreenPos({ posSave.x + kLabelWidth, posSave.y });
        }
        {
            const auto & protocol = settings.txProtocols[settings.protocolId];
            const float bandwidth = ((float(0.715f*protocol.bytesPerTx)/(protocol.framesPerTx*statsCurrent.samplesPerFrame))*statsCurrent.sampleRate)/protocol.extra;
            ImGui::Text("%4.2f B/s", bandwidth);
        }

        {
            auto posSave = ImGui::GetCursorScreenPos();
            ImGui::Text("Frequencies: ");
            ImGui::SetCursorScreenPos({ posSave.x + kLabelWidth, posSave.y });
        }
        {
            const float df = statsCurrent.sampleRate/statsCurrent.samplesPerFrame;
            const auto & protocol = settings.txProtocols[settings.protocolId];
            const auto freqStart = std::max(1, protocol.freqStart + (settings.isFreqStartShift ? settings.freqStartShift : 0));
            const float f0 = df*freqStart;
            const float f1 = df*(freqStart + float(2*16*protocol.bytesPerTx)/protocol.extra);
            ImGui::Text("%6.2f Hz - %6.2f Hz", f0, f1);
        }

        // fixed-length
        ImGui::Text("%s", "");
        {
            auto posSave = ImGui::GetCursorScreenPos();
            ImGui::Text("%s", "");
            ImGui::SetCursorScreenPos({ posSave.x + kLabelWidth, posSave.y });
            ImGui::PushTextWrapPos();
            ImGui::TextDisabled("Fixed-length Tx/Rx does not use sound-markers");
            ImGui::PopTextWrapPos();
        }
        {
            auto posSave = ImGui::GetCursorScreenPos();
            ImGui::Text("Fixed-length: ");
            ImGui::SetCursorScreenPos({ posSave.x + kLabelWidth, posSave.y });
        }
        if (ImGui::Checkbox("##fixed-length", &settings.isFixedLength)) {
            g_buffer.inputUI.update = true;
            g_buffer.inputUI.flags.needReinit = true;
            g_buffer.inputUI.payloadLength = settings.isFixedLength ? settings.payloadLength : -1;
        }

        if (settings.isFixedLength) {
            ImGui::SameLine();
            ImGui::PushItemWidth(0.5*ImGui::GetContentRegionAvailWidth());
            if (ImGui::DragInt("Bytes", &settings.payloadLength, 1, 1, GGWave::kMaxLengthFixed)) {
                g_buffer.inputUI.update = true;
                g_buffer.inputUI.flags.needReinit = true;
                g_buffer.inputUI.payloadLength = settings.isFixedLength ? settings.payloadLength : -1;
            }
            ImGui::PopItemWidth();
        }

        // Direct-sequence spread
        //ImGui::Text("%s", "");
        {
            auto posSave = ImGui::GetCursorScreenPos();
            ImGui::Text("%s", "");
            ImGui::SetCursorScreenPos({ posSave.x + kLabelWidth, posSave.y });
            ImGui::PushTextWrapPos();
            ImGui::TextDisabled("Direct-sequence spread");
            ImGui::PopTextWrapPos();
        }
        {
            auto posSave = ImGui::GetCursorScreenPos();
            ImGui::Text("Use DSS: ");
            ImGui::SetCursorScreenPos({ posSave.x + kLabelWidth, posSave.y });
        }
        if (ImGui::Checkbox("##direct-sequence-spread", &settings.directSequenceSpread)) {
            g_buffer.inputUI.update = true;
            g_buffer.inputUI.flags.needReinit = true;
            g_buffer.inputUI.directSequenceSpread = settings.directSequenceSpread;
        }

        // FreqStart offset
        //ImGui::Text("%s", "");
        {
            auto posSave = ImGui::GetCursorScreenPos();
            ImGui::Text("%s", "");
            ImGui::SetCursorScreenPos({ posSave.x + kLabelWidth, posSave.y });
            ImGui::PushTextWrapPos();
            ImGui::TextDisabled("Apply tx/rx frequency shift");
            ImGui::PopTextWrapPos();
        }
        {
            auto posSave = ImGui::GetCursorScreenPos();
            ImGui::Text("Freq shift: ");
            ImGui::SetCursorScreenPos({ posSave.x + kLabelWidth, posSave.y });
        }
        if (ImGui::Checkbox("##freq-start-offset", &settings.isFreqStartShift)) {
            g_buffer.inputUI.update = true;
            g_buffer.inputUI.flags.needReinit = true;
            g_buffer.inputUI.freqStartShift = settings.isFreqStartShift ? settings.freqStartShift : 0;
        }

        if (settings.isFreqStartShift) {
            ImGui::SameLine();
            ImGui::PushItemWidth(0.5*ImGui::GetContentRegionAvailWidth());
            if (ImGui::DragInt("bins", &settings.freqStartShift, 1, -64, 64, "%d")) {
                g_buffer.inputUI.update = true;
                g_buffer.inputUI.flags.needReinit = true;
                g_buffer.inputUI.freqStartShift = settings.isFreqStartShift ? settings.freqStartShift : 0;
            }
            ImGui::PopItemWidth();

            {
                auto posSave = ImGui::GetCursorScreenPos();
                ImGui::Text("");
                ImGui::SetCursorScreenPos({ posSave.x + kLabelWidth, posSave.y });
            }
            {
                const float df = statsCurrent.sampleRate/statsCurrent.samplesPerFrame;
                ImGui::Text("%6.2f Hz", df*settings.freqStartShift);
            }
        }

        // Output sample-rate offset
        //ImGui::Text("%s", "");
        {
            auto posSave = ImGui::GetCursorScreenPos();
            ImGui::Text("%s", "");
            ImGui::SetCursorScreenPos({ posSave.x + kLabelWidth, posSave.y });
            ImGui::PushTextWrapPos();
            ImGui::TextDisabled("Modify the output Sampling Rate");
            ImGui::PopTextWrapPos();
        }
        {
            auto posSave = ImGui::GetCursorScreenPos();
            ImGui::Text("Pitch shift: ");
            ImGui::SetCursorScreenPos({ posSave.x + kLabelWidth, posSave.y });
        }
        if (ImGui::Checkbox("##output-sample-rate-offset", &settings.isSampleRateOffset)) {
            g_buffer.inputUI.update = true;
            g_buffer.inputUI.flags.needReinit = true;
            g_buffer.inputUI.sampleRateOffset = settings.isSampleRateOffset ? settings.sampleRateOffset : 0;
        }

        if (settings.isSampleRateOffset) {
            ImGui::SameLine();
            ImGui::PushItemWidth(0.5*ImGui::GetContentRegionAvailWidth());
            if (ImGui::SliderFloat("Samples", &settings.sampleRateOffset, -1000, 1000, "%.0f")) {
                g_buffer.inputUI.update = true;
                g_buffer.inputUI.flags.needReinit = true;
                g_buffer.inputUI.sampleRateOffset = settings.isSampleRateOffset ? settings.sampleRateOffset : 0;
            }
            ImGui::PopItemWidth();
        }

        // rx protocols
        bool updateRxProtocols = false;
        ImGui::Text("%s", "");
        {
            auto posSave = ImGui::GetCursorScreenPos();
            ImGui::Text("%s", "");
            ImGui::SetCursorScreenPos({ posSave.x + kLabelWidth, posSave.y });
            ImGui::PushTextWrapPos();
            ImGui::TextDisabled("Waver will receive only the selected protocols:");
            ImGui::PopTextWrapPos();
        }
        {
            auto posSave = ImGui::GetCursorScreenPos();
            ImGui::Text("Rx Protocols: ");
            ImGui::SetCursorScreenPos(posSave);
        }
        {
            ImGui::PushID("RxProtocols");
            for (int i = 0; i < settings.rxProtocols.size(); ++i) {
                auto & rxProtocol = settings.rxProtocols[i];
                if (rxProtocol.name == nullptr) continue;
                auto posSave = ImGui::GetCursorScreenPos();
                ImGui::Text("%s", "");
                ImGui::SetCursorScreenPos({ posSave.x + kLabelWidth, posSave.y });
                if (ImGui::Checkbox(rxProtocol.name, &rxProtocol.enabled)) {
                    updateRxProtocols = true;
                }
            }
            ImGui::PopID();
        }

        if (updateRxProtocols) {
            g_buffer.inputUI.update = true;
            g_buffer.inputUI.flags.changeRxProtocols = true;
            g_buffer.inputUI.rxProtocols = settings.rxProtocols;
        }

        ScrollWhenDraggingOnVoid(ImVec2(0.0f, -mouse_delta.y), ImGuiMouseButton_Left);

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

        static bool isDragging = false;
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
            ImGui::TextColored({ 0.0f, 0.6f, 0.4f, interp }, "%s", settings.txProtocols[message.protocolId].name);
            ImGui::SameLine();
            if (message.dss) {
                ImGui::TextColored({ 0.4f, 0.6f, 0.4f, interp }, "DSS");
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("Direct Sequence Spread");
                }
                ImGui::SameLine();
            }
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
                            ImGui::TextColored(col, "Error: %s", message.data.c_str());
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

        if (ImGui::IsMouseReleased(0) && isHoldingMessage && isDragging == false) {
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
                g_buffer.inputUI.flags.newMessage = true;
                g_buffer.inputUI.message = { false, std::chrono::system_clock::now(), messageSelected.data, messageSelected.protocolId, messageSelected.dss, settings.volume, Message::Text };

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
                    subWindowIdFiles = SubWindowIdFiles::Receive;

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

        {
            bool isDraggingCur = ScrollWhenDraggingOnVoid(ImVec2(0.0f, -mouse_delta.y), ImGuiMouseButton_Left);
            if (isDraggingCur && ImGui::IsMouseDown(0)) {
                isDragging = true;
            } else if (isDraggingCur == false && ImGui::IsMouseDown(0) == false) {
                isDragging = false;
            }
        }
        ImGui::EndChild();

        if (statsCurrent.receiving) {
            if (statsCurrent.analyzing) {
                ImGui::TextColored({ 0.0f, 1.0f, 0.0f, 1.0f }, "Analyzing ...");
                ImGui::SameLine();
                ImGui::ProgressBar(1.0f - float(statsCurrent.framesLeftToAnalyze)/statsCurrent.framesToAnalyze,
                                   { ImGui::GetContentRegionAvailWidth() - ImGui::CalcTextSize(sendButtonText).x - 2*style.ItemSpacing.x, ImGui::GetTextLineHeight() });
            } else {
                ImGui::TextColored({ 0.0f, 1.0f, 0.0f, 1.0f }, "Receiving ...");
                ImGui::SameLine();
                if (ImGui::SmallButton("Stop")) {
                    g_buffer.inputUI.update = true;
                    g_buffer.inputUI.flags.stopReceiving = true;
                }
                ImGui::SameLine();
                ImGui::ProgressBar(1.0f - float(statsCurrent.framesLeftToRecord)/statsCurrent.framesToRecord,
                                   { ImGui::GetContentRegionAvailWidth() - ImGui::CalcTextSize(sendButtonText).x - 2*style.ItemSpacing.x, ImGui::GetTextLineHeight() });
            }
        } else {
            static float amax = 0.0f;
            static float frac = 0.0f;

            amax = 0.0f;
            frac = (ImGui::GetTime() - tStartTx)/tLengthTx;

            if (txAmplitudeCurrent.size() && frac <= 1.0f) {
                struct Funcs {
                    static float Sample(void * data, int i) {
                        auto res = std::fabs(((int16_t *)(data))[i]) ;
                        if (res > amax) amax = res;
                        return res;
                    }

                    static float SampleFrac(void * data, int i) {
                        if (i > frac*txAmplitudeCurrent.size()) {
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
                                     txAmplitudeCurrent.data(),
                                     (int) txAmplitudeCurrent.size(), 0,
                                     (std::string("")).c_str(),
                                     0.0f, FLT_MAX, wSize);
                ImGui::PopStyleColor(2);

                ImGui::SetCursorScreenPos(posSave);

                ImGui::PushStyleColor(ImGuiCol_FrameBg, { 0.0f, 0.0f, 0.0f, 0.0f });
                ImGui::PushStyleColor(ImGuiCol_PlotHistogram, { 0.0f, 1.0f, 0.0f, 1.0f });
                ImGui::PlotHistogram("##plotSpectrumCurrent",
                                     Funcs::SampleFrac,
                                     txAmplitudeCurrent.data(),
                                     (int) txAmplitudeCurrent.size(), 0,
                                     (std::string("")).c_str(),
                                     0.0f, amax, wSize);
                ImGui::PopStyleColor(2);
            } else {
                if (settings.isFixedLength) {
                    ImGui::TextDisabled("Listening for waves (fixed-length %d bytes)", settings.payloadLength);
                } else {
                    ImGui::TextDisabled("Listening for waves (variable-length)");
                }
            }
        }

        if (doInputFocus) {
            ImGui::SetKeyboardFocusHere();
            doInputFocus = false;
        }

        doSendMessage = false;
        {
            auto pos0 = ImGui::GetCursorScreenPos();
            ImGui::PushItemWidth(ImGui::GetContentRegionAvailWidth() - ImGui::CalcTextSize(sendButtonText).x - 2*style.ItemSpacing.x);
            if (ImGui::InputText("##Messages:Input", inputBuf, kMaxInputSize, ImGuiInputTextFlags_EnterReturnsTrue)) {
                doSendMessage = true;
            }
            ImGui::PopItemWidth();

            if (isTextInput == false && inputBuf[0] == 0) {
                auto drawList = ImGui::GetWindowDrawList();
                pos0.x += style.ItemInnerSpacing.x;
                pos0.y += 0.5*style.ItemInnerSpacing.y;
                static char tmp[128];
                snprintf(tmp, 128, "Send message using '%s'", settings.txProtocols[settings.protocolId].name);
                drawList->AddText(pos0, ImGui::ColorConvertFloat4ToU32({0.0f, 0.6f, 0.4f, 1.0f}), tmp);
            }
        }

        if (ImGui::IsItemActive() && isTextInput == false) {
            SDL_StartTextInput();
            isTextInput = true;
#if defined(IOS) || defined(ANDROID)
            tStartInput = ImGui::GetTime();
#endif
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
                strncpy(inputBuf, inputLast.data(), kMaxInputSize - 1);
            }
            if (inputBuf[0] != 0) {
                inputLast = std::string(inputBuf);
                g_buffer.inputUI.update = true;
                g_buffer.inputUI.flags.newMessage = true;
                g_buffer.inputUI.message = { false, std::chrono::system_clock::now(), std::string(inputBuf), settings.protocolId, settings.directSequenceSpread, settings.volume, Message::Text };

                messageHistory.push_back(g_buffer.inputUI.message);

                inputBuf[0] = 0;
                doInputFocus = true;
                scrollMessagesToBottom = true;
            }
        }
        if (!ImGui::IsItemHovered() && requestStopTextInput) {
            SDL_StopTextInput();
            isTextInput = false;
#if defined(IOS) || defined(ANDROID)
            tEndInput = ImGui::GetTime();
#endif
        }
    }

    if (windowId == WindowId::Files) {
        const float subWindowButtonHeight = menuButtonHeight;

        if (ImGui::ButtonSelectable("Send", { 0.50f*ImGui::GetContentRegionAvailWidth(), subWindowButtonHeight }, subWindowIdFiles == SubWindowIdFiles::Send)) {
            subWindowIdFiles = SubWindowIdFiles::Send;
        }
        ImGui::SameLine();

        if (ImGui::ButtonSelectable("Receive", { 1.0f*ImGui::GetContentRegionAvailWidth(), subWindowButtonHeight }, subWindowIdFiles == SubWindowIdFiles::Receive)) {
            subWindowIdFiles = SubWindowIdFiles::Receive;
        }

        switch (subWindowIdFiles) {
            case SubWindowIdFiles::Send:
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
                            ImGui::TextColored({ 1.0f, 0.6f, 0.0f, 1.0f }, "File sharing works only between peers in the same local network!");
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
                            g_buffer.inputUI.flags.newMessage = true;
                            g_buffer.inputUI.message = {
                                false,
                                std::chrono::system_clock::now(),
                                ::generateFileBroadcastMessage(),
                                settings.protocolId,
                                settings.directSequenceSpread,
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
            case SubWindowIdFiles::Receive:
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
                            } else if (g_fileClient.isConnected() && (fileInfoExtended.receiving || fileInfoExtended.nReceivedChunks > 0)) {
                                if (fileInfoExtended.receiving) {
                                    if (ImGui::Button(ICON_FA_PAUSE "  Pause")) {
                                        fileInfoExtended.receiving = false;
                                    }
                                } else {
                                    if (ImGui::Button(ICON_FA_PLAY "  Resume")) {
                                        fileInfoExtended.receiving = true;
                                    }
                                }

                                ImGui::SameLine();
                                ImGui::ProgressBar(float(fileInfoExtended.nReceivedChunks)/fileInfo.second.nChunks);
                            } else if (g_fileClient.isConnected()) {
                                if (ImGui::Button(ICON_FA_DOWNLOAD "  Receive")) {
                                    fileInfoExtended.receiving = true;
                                    fileInfoExtended.isChunkReceived.resize(fileInfo.second.nChunks);
                                    fileInfoExtended.isChunkRequested.resize(fileInfo.second.nChunks);
                                }
                            } else {
                                ImGui::Text("%s", "");
                            }

                            if ((fileInfoExtended.receiving == false || isReceived) && fileInfoExtended.requestToShare == false) {
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
        if (hasAudioCaptureData == false) {
            ImGui::Text("%s", "");
            ImGui::TextColored({ 1.0f, 0.0f, 0.0f, 1.0f }, "No capture data available!");
            ImGui::TextColored({ 1.0f, 0.0f, 0.0f, 1.0f }, "Please make sure you have allowed microphone access for this app.");
        } else {
            const int nBins = statsCurrent.samplesPerFrame/2;

            static int binMin = 20;
            static int binMax = 100;

            static float scale = 30.0f;

            static bool showFPS = false;
            static bool showRx = true;
            static bool showSpectrogram = false;

            static const float kSpectrogramTime_s = 3.0f;

            // 3 seconds data
            static int freqDataSize = (kSpectrogramTime_s*statsCurrent.sampleRateInp)/statsCurrent.samplesPerFrame;
            static int freqDataHead = 0;

            struct FreqData {
                float freq;

                std::vector<float> mag;
            };

            static std::vector<FreqData> freqData;
            if (freqData.empty()) {
                float df = statsCurrent.sampleRateInp/statsCurrent.samplesPerFrame;
                freqData.resize(nBins);
                for (int i = 0; i < nBins; ++i) {
                    freqData[i].freq = df*i;
                    freqData[i].mag.resize(freqDataSize);
                }
            }

            if (hasNewSpectrum) {
                for (int i = 0; i < (int) freqData.size(); ++i) {
                    freqData[i].mag[freqDataHead] = spectrumCurrent[i];
                }
                if (++freqDataHead == freqDataSize) {
                    freqDataHead = 0;
                }

                hasNewSpectrum = false;
            }

            if (showSpectrumSettings) {
                auto width = ImGui::GetContentRegionAvailWidth();
                ImGui::PushItemWidth(0.5*width);
                static char buf[64];
                snprintf(buf, 64, "Bin: %3d, Freq: %5.2f Hz", binMin, 0.5*binMin*statsCurrent.sampleRateInp/nBins);
                ImGui::DragInt("##binMin", &binMin, 1, 0, binMax - 2, buf);
                ImGui::SameLine();
                ImGui::Checkbox("FPS", &showFPS);
                ImGui::SameLine();
                ImGui::Checkbox("Spectrogram", &showSpectrogram);
                snprintf(buf, 64, "Bin: %3d, Freq: %5.2f Hz", binMax, 0.5*binMax*statsCurrent.sampleRateInp/nBins);
                ImGui::DragInt("##binMax", &binMax, 1, binMin + 1, nBins, buf);
                ImGui::SameLine();
                ImGui::Checkbox("Rx", &showRx);
                ImGui::SameLine();
                ImGui::DragFloat("##scale", &scale, 1.0f, 1.0f, 1000.0f);
                ImGui::PopItemWidth();
            }

            if (showSpectrogram) {
                subWindowIdSpectrum = SubWindowIdSpectrum::Spectrogram;
            } else {
                subWindowIdSpectrum = SubWindowIdSpectrum::Spectrum;
            }

            auto p0 = ImGui::GetCursorScreenPos();
            auto p1 = ImGui::GetContentRegionAvail();
            p1.x += p0.x;
            p1.y += p0.y;
            if (ImGui::IsMouseHoveringRect(p0, p1) && ImGui::IsMouseClicked(0)) {
                g_buffer.inputUI.update = true;
                g_buffer.inputUI.flags.changeNeedSpectrum = true;
                g_buffer.inputUI.needSpectrum = !g_buffer.inputUI.needSpectrum;
            }

            auto itemSpacingSave = style.ItemSpacing;
            style.ItemSpacing.x = 0.0f;
            style.ItemSpacing.y = 0.0f;

            auto windowPaddingSave = style.WindowPadding;
            style.WindowPadding.x = 0.0f;
            style.WindowPadding.y = 0.0f;

            auto childBorderSizeSave = style.ChildBorderSize;
            style.ChildBorderSize = 0.0f;

            ImGui::BeginChild("Spectrum:main", ImGui::GetContentRegionAvail(), true, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
            switch (subWindowIdSpectrum) {
                case SubWindowIdSpectrum::Spectrum:
                    {
                        ImGui::PushTextWrapPos();
                        if (showFPS) {
                            auto posSave = ImGui::GetCursorScreenPos();
                            ImGui::Text("FPS: %4.2f\n", ImGui::GetIO().Framerate);
                            ImGui::SetCursorScreenPos(posSave);
                        }
                        auto wSize = ImGui::GetContentRegionAvail();
                        ImGui::PushStyleColor(ImGuiCol_FrameBg, { 0.3f, 0.3f, 0.3f, 0.3f });
                        if (statsCurrent.receiving) {
                            ImGui::PushStyleColor(ImGuiCol_PlotHistogram, { 1.0f, 0.0f, 0.0f, 1.0f });
                        } else {
                            ImGui::PushStyleColor(ImGuiCol_PlotHistogram, { 0.0f, 1.0f, 0.0f, 1.0f });
                        }
                        ImGui::PlotHistogram("##plotSpectrumCurrent",
                                             spectrumCurrent.data() + binMin, binMax - binMin, 0,
                                             (std::string("Current Spectrum")).c_str(),
                                             0.0f, FLT_MAX, wSize);
                        ImGui::PopStyleColor(2);
                        ImGui::PopTextWrapPos();
                    }
                    break;
                case SubWindowIdSpectrum::Spectrogram:
                    {
                        if (showFPS) {
                            auto posSave = ImGui::GetCursorScreenPos();
                            ImGui::Text("FPS: %4.2f\n", ImGui::GetIO().Framerate);
                            ImGui::SetCursorScreenPos(posSave);
                        }

                        float sum = 0.0;
                        for (int i = binMin; i < binMax; ++i) {
                            for (int j = 0; j < freqDataSize; ++j) {
                                sum += freqData[i].mag[j];
                            }
                        }

                        int nf = binMax - binMin;
                        sum /= (nf*freqDataSize);

                        const auto wSize = ImGui::GetContentRegionAvail();

                        const float dx = wSize.x/nf;
                        const float dy = wSize.y/freqDataSize;

                        auto p0 = ImGui::GetCursorScreenPos();

                        int nChildWindows = 0;
                        int nFreqPerChild = 32;
                        ImGui::PushID(nChildWindows++);
                        ImGui::BeginChild("Spectrogram", { wSize.x, (nFreqPerChild + 1)*dy }, true);
                        auto drawList = ImGui::GetWindowDrawList();

                        for (int j = 0; j < freqDataSize; ++j) {
                            if (j > 0 && j % nFreqPerChild == 0) {
                                ImGui::EndChild();
                                ImGui::PopID();

                                ImGui::PushID(nChildWindows++);
                                ImGui::SetCursorScreenPos({ p0.x, p0.y + nFreqPerChild*int(j/nFreqPerChild)*dy });
                                ImGui::BeginChild("Spectrogram", { wSize.x, (nFreqPerChild + 1)*dy }, true);
                                drawList = ImGui::GetWindowDrawList();
                            }
                            for (int i = 0; i < nf; ++i) {
                                int k = freqDataHead + j;
                                if (k >= freqDataSize) k -= freqDataSize;
                                auto v = freqData[binMin + i].mag[k];
                                ImVec4 c = { 0.0f, 1.0f, 0.0, 0.0f };
                                c.w = v/(scale*sum);
                                drawList->AddRectFilled({ p0.x + i*dx, p0.y + j*dy }, { p0.x + i*dx + dx, p0.y + j*dy + dy }, ImGui::ColorConvertFloat4ToU32(c));
                            }
                        }

                        ImGui::EndChild();
                        ImGui::PopID();

                        while (showRx && messageHistory.size() > 0) {
                            const auto& msg = messageHistory.back();
                            static float tRecv = 0.0;
                            if (g_buffer.inputUI.needSpectrum) {
                                tRecv = 0.001f*std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - msg.timestamp).count();
                            }

                            if (tRecv > 2.0f*kSpectrogramTime_s || msg.received == false) {
                                break;
                            }

                            const auto & protocol = settings.txProtocols[msg.protocolId];
                            const int msgLength_bytes = settings.isFixedLength ? 1.4f*settings.payloadLength : 1.4f*msg.data.size() + GGWave::kDefaultEncodedDataOffset;
                            const int msgLength_frames = settings.isFixedLength ?
                                ((msgLength_bytes + protocol.bytesPerTx - 1)/protocol.bytesPerTx)*protocol.framesPerTx :
                                ((msgLength_bytes + protocol.bytesPerTx - 1)/protocol.bytesPerTx)*protocol.framesPerTx + 2*GGWave::kDefaultMarkerFrames;
                            const float frameLength_s = (float(statsCurrent.samplesPerFrame)/statsCurrent.sampleRateInp);

                            const float x0 = protocol.freqStart - binMin;
                            const float x1 = x0 + 32*protocol.bytesPerTx;
                            const float y1 = freqDataSize - tRecv/frameLength_s + (settings.isFixedLength ? 0.0f : 0.5*GGWave::kDefaultMarkerFrames);
                            const float y0 = y1 - msgLength_frames;

                            ImVec4 c = { 1.0f, 0.0f, 0.0, 1.0f };
                            drawList->AddRect({ p0.x + x0*dx, p0.y + y0*dy }, { p0.x + x1*dx + dx, p0.y + y1*dy }, ImGui::ColorConvertFloat4ToU32(c));

                            break;
                        }

                    }
                    break;
            };
            ImGui::EndChild();

            style.ItemSpacing = itemSpacingSave;
            style.WindowPadding = windowPaddingSave;
            style.ChildBorderSize = childBorderSizeSave;
        }
    }

    ImGui::End();

    ImGui::GetIO().KeysDown[ImGui::GetIO().KeyMap[ImGuiKey_Backspace]] = false;
    ImGui::GetIO().KeysDown[ImGui::GetIO().KeyMap[ImGuiKey_Enter]] = false;

    {
        std::lock_guard<std::mutex> lock(g_buffer.mutex);
        g_buffer.inputUI.apply(g_buffer.inputCore);
    }
}

void deinitMain() {
    g_isRunning = false;
}
