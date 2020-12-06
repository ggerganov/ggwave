#pragma once

#include "ggwave/ggwave.h"

#include "ggwave-common.h"
#include "ggwave-common-sdl2.h"

#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>
#include <imgui-extra/imgui_impl.h>

#include <SDL.h>

#include <atomic>
#include <cstdio>
#include <string>
#include <ctime>
#include <mutex>
#include <thread>
#include <vector>

#ifndef ICON_FA_COGS
#define ICON_FA_COGS "#"
#define ICON_FA_COMMENT_ALT ""
#define ICON_FA_SIGNAL ""
#define ICON_FA_PLAY_CIRCLE ""
#endif

struct Message {
    bool received;
    std::time_t timestamp;
    std::string data;
    int protocolId;
    float volume;
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
    GGWaveStats stats;
};

struct Input {
    bool update = false;
    Message message;
};

struct Buffer {
    std::mutex mutex;

    State stateCore;
    Input inputCore;

    State stateUI;
    Input inputUI;
};

char * toTimeString(const std::time_t t) {
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

GGWave * g_ggWave;
Buffer g_buffer;
std::atomic<bool> g_isRunning;

void initMain() {
    ImGuiStyle & style = ImGui::GetStyle();

    style.AntiAliasedFill = false;
    style.AntiAliasedLines = false;
    style.WindowRounding = 0.0f;

    style.WindowPadding = ImVec2(8, 8);
    style.WindowRounding = 0.0f;
    style.FramePadding = ImVec2(4, 3);
    style.FrameRounding = 0.0f;
    style.ItemSpacing = ImVec2(8, 4);
    style.ItemInnerSpacing = ImVec2(4, 4);
    style.IndentSpacing = 21.0f;
    style.ScrollbarSize = 16.0f;
    style.ScrollbarRounding = 9.0f;
    style.GrabMinSize = 10.0f;
    style.GrabRounding = 3.0f;

    style.Colors[ImGuiCol_Text]                  = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
    style.Colors[ImGuiCol_TextDisabled]          = ImVec4(0.24f, 0.41f, 0.41f, 1.00f);
    style.Colors[ImGuiCol_WindowBg]              = ImVec4(0.08f, 0.08f, 0.08f, 0.94f);
    //style.Colors[ImGuiCol_ChildWindowBg]         = ImVec4(0.07f, 0.07f, 0.09f, 1.00f);
    style.Colors[ImGuiCol_PopupBg]               = ImVec4(0.07f, 0.07f, 0.09f, 1.00f);
    style.Colors[ImGuiCol_Border]                = ImVec4(0.31f, 0.31f, 0.31f, 0.71f);
    style.Colors[ImGuiCol_BorderShadow]          = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    style.Colors[ImGuiCol_FrameBg]               = ImVec4(0.00f, 0.39f, 0.39f, 0.39f);
    style.Colors[ImGuiCol_FrameBgHovered]        = ImVec4(0.26f, 1.00f, 1.00f, 0.39f);
    style.Colors[ImGuiCol_FrameBgActive]         = ImVec4(0.00f, 0.78f, 0.00f, 1.00f);
    style.Colors[ImGuiCol_TitleBg]               = ImVec4(0.00f, 0.50f, 0.50f, 0.70f);
    style.Colors[ImGuiCol_TitleBgCollapsed]      = ImVec4(0.00f, 0.50f, 0.50f, 1.00f);
    style.Colors[ImGuiCol_TitleBgActive]         = ImVec4(0.00f, 0.70f, 0.70f, 1.00f);
    style.Colors[ImGuiCol_MenuBarBg]             = ImVec4(0.00f, 0.70f, 0.70f, 1.00f);
    style.Colors[ImGuiCol_ScrollbarBg]           = ImVec4(0.10f, 0.27f, 0.27f, 1.00f);
    style.Colors[ImGuiCol_ScrollbarGrab]         = ImVec4(0.80f, 0.80f, 0.83f, 0.31f);
    style.Colors[ImGuiCol_ScrollbarGrabHovered]  = ImVec4(0.26f, 1.00f, 1.00f, 0.39f);
    style.Colors[ImGuiCol_ScrollbarGrabActive]   = ImVec4(0.00f, 0.78f, 0.00f, 1.00f);
    //style.Colors[ImGuiCol_ComboBg]               = ImVec4(0.00f, 0.39f, 0.39f, 1.00f);
    style.Colors[ImGuiCol_CheckMark]             = ImVec4(0.80f, 0.80f, 0.83f, 0.39f);
    style.Colors[ImGuiCol_SliderGrab]            = ImVec4(0.80f, 0.80f, 0.83f, 0.39f);
    style.Colors[ImGuiCol_SliderGrabActive]      = ImVec4(0.00f, 0.78f, 0.00f, 1.00f);
    style.Colors[ImGuiCol_Button]                = ImVec4(0.13f, 0.55f, 0.55f, 1.00f);
    style.Colors[ImGuiCol_ButtonHovered]         = ImVec4(0.61f, 1.00f, 0.00f, 0.51f);
    style.Colors[ImGuiCol_ButtonActive]          = ImVec4(0.00f, 0.78f, 0.00f, 1.00f);
    style.Colors[ImGuiCol_Header]                = ImVec4(0.79f, 0.51f, 0.00f, 0.51f);
    style.Colors[ImGuiCol_HeaderHovered]         = ImVec4(0.79f, 0.51f, 0.00f, 0.67f);
    style.Colors[ImGuiCol_HeaderActive]          = ImVec4(0.79f, 0.51f, 0.00f, 0.67f);
    //style.Colors[ImGuiCol_Column]                = ImVec4(0.79f, 0.51f, 0.00f, 0.67f);
    //style.Colors[ImGuiCol_ColumnHovered]         = ImVec4(0.25f, 1.00f, 0.00f, 1.00f);
    //style.Colors[ImGuiCol_ColumnActive]          = ImVec4(0.79f, 0.51f, 0.00f, 0.67f);
    style.Colors[ImGuiCol_ResizeGrip]            = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    style.Colors[ImGuiCol_ResizeGripHovered]     = ImVec4(0.26f, 1.00f, 1.00f, 0.39f);
    style.Colors[ImGuiCol_ResizeGripActive]      = ImVec4(0.00f, 0.78f, 0.00f, 1.00f);
    //style.Colors[ImGuiCol_CloseButton]           = ImVec4(0.40f, 0.39f, 0.38f, 0.16f);
    //style.Colors[ImGuiCol_CloseButtonHovered]    = ImVec4(0.26f, 1.00f, 1.00f, 0.39f);
    //style.Colors[ImGuiCol_CloseButtonActive]     = ImVec4(0.79f, 0.51f, 0.00f, 0.67f);
    style.Colors[ImGuiCol_PlotLines]             = ImVec4(1.00f, 0.65f, 0.38f, 0.67f);
    style.Colors[ImGuiCol_PlotLinesHovered]      = ImVec4(0.25f, 1.00f, 0.00f, 1.00f);
    style.Colors[ImGuiCol_PlotHistogram]         = ImVec4(1.00f, 0.65f, 0.38f, 0.67f);
    style.Colors[ImGuiCol_PlotHistogramHovered]  = ImVec4(0.25f, 1.00f, 0.00f, 1.00f);
    style.Colors[ImGuiCol_TextSelectedBg]        = ImVec4(0.25f, 1.00f, 0.00f, 0.43f);
    style.Colors[ImGuiCol_ModalWindowDarkening]  = ImVec4(1.00f, 0.98f, 0.95f, 0.78f);

    g_ggWave = GGWave_instance();
    g_isRunning = true;
}

std::thread initWorker() {
    return std::thread([&]() {
        Input inputCurrent;

        int lastRxDataLength = 0;
        GGWave::TxRxData lastRxData;

        while (g_isRunning) {
            {
                std::lock_guard<std::mutex> lock(g_buffer.mutex);
                if (g_buffer.inputCore.update) {
                    inputCurrent = std::move(g_buffer.inputCore);
                    g_buffer.inputCore.update = false;
                }
            }

            if (inputCurrent.update) {
                g_ggWave->init(
                        inputCurrent.message.data.size(),
                        inputCurrent.message.data.data(),
                        g_ggWave->getTxProtocols()[inputCurrent.message.protocolId],
                        100*inputCurrent.message.volume);

                inputCurrent.update = false;
            }

            GGWave_mainLoop();

            lastRxDataLength = g_ggWave->takeRxData(lastRxData);
            if (lastRxDataLength > 0) {
                g_buffer.stateCore.update = true;
                g_buffer.stateCore.flags.newMessage = true;
                g_buffer.stateCore.message = {
                    true,
                    std::time(nullptr),
                    std::string((char *) lastRxData.data(), lastRxDataLength),
                    g_ggWave->getRxProtocolId(),
                    0,
                };
            }

            if (g_ggWave->takeSpectrum(g_buffer.stateCore.spectrum)) {
                g_buffer.stateCore.update = true;
                g_buffer.stateCore.flags.newSpectrum = true;
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

            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    });
}

void mainRender() {
    static State stateCurrent;

    {
        std::lock_guard<std::mutex> lock(g_buffer.mutex);
        g_buffer.stateUI.apply(stateCurrent);
    }

    enum class WindowId {
        Settings,
        Messages,
        Spectrum,
    };

    struct Settings {
        int protocolId = 1;
        float volume = 0.10f;
    };

    static WindowId windowId = WindowId::Messages;
    static Settings settings;

    static char inputBuf[256];

    static bool doInputFocus = false;
    static bool lastMouseButtonLeft = 0;
    static bool isTextInput = false;
    static bool scrollMessagesToBottom = true;

    static double tStartInput = 0.0f;
    static double tEndInput = -100.0f;

    static GGWaveStats statsCurrent;
    static GGWave::SpectrumData spectrumCurrent;
    static std::vector<Message> messageHistory;

    if (stateCurrent.update) {
        if (stateCurrent.flags.newMessage) {
            scrollMessagesToBottom = true;
            messageHistory.push_back(std::move(stateCurrent.message));
        }
        if (stateCurrent.flags.newSpectrum) {
            spectrumCurrent = std::move(stateCurrent.spectrum);
        }
        if (stateCurrent.flags.newStats) {
            statsCurrent = std::move(stateCurrent.stats);
        }
        stateCurrent.flags.clear();
        stateCurrent.update = false;
    }

    if (lastMouseButtonLeft == 0 && ImGui::GetIO().MouseDown[0] == 1) {
        ImGui::GetIO().MouseDelta = { 0.0, 0.0 };
    }
    lastMouseButtonLeft = ImGui::GetIO().MouseDown[0];

    const auto& displaySize = ImGui::GetIO().DisplaySize;
    auto& style = ImGui::GetStyle();

    const auto sendButtonText = ICON_FA_PLAY_CIRCLE " Send";
    const double tShowKeyboard = 0.2f;
#ifdef IOS
    const float statusBarHeight = displaySize.x < displaySize.y ? 10.0f + 2.0f*style.ItemSpacing.y : 0.1f;
#else
    const float statusBarHeight = 0.1f;
#endif
    const float menuButtonHeight = 24.0f + 2.0f*style.ItemSpacing.y;

    ImGui::SetNextWindowPos({ 0, 0, });
    ImGui::SetNextWindowSize(displaySize);
    ImGui::Begin("Main", nullptr,
                 ImGuiWindowFlags_NoMove |
                 ImGuiWindowFlags_NoTitleBar |
                 ImGuiWindowFlags_NoScrollbar |
                 ImGuiWindowFlags_NoResize |
                 ImGuiWindowFlags_NoSavedSettings);

    ImGui::InvisibleButton("StatusBar", { ImGui::GetContentRegionAvailWidth(), statusBarHeight });

    if (ImGui::Button(ICON_FA_COGS, { menuButtonHeight, menuButtonHeight } )) {
        windowId = WindowId::Settings;
    }
    ImGui::SameLine();

    if (ImGui::Button(ICON_FA_COMMENT_ALT "  Messages", { 0.5f*ImGui::GetContentRegionAvailWidth(), menuButtonHeight })) {
        windowId = WindowId::Messages;
    }
    ImGui::SameLine();

    if (ImGui::Button(ICON_FA_SIGNAL "  Spectrum", { 1.0f*ImGui::GetContentRegionAvailWidth(), menuButtonHeight })) {
        windowId = WindowId::Spectrum;
    }

    if (windowId == WindowId::Settings) {
        ImGui::BeginChild("Settings:main", ImGui::GetContentRegionAvail(), true);
        ImGui::Text("%s", "");
        ImGui::Text("%s", "");
        ImGui::Text("Waver v0.1");
        ImGui::Separator();

        ImGui::Text("%s", "");
        ImGui::Text("Sample rate (capture):  %g, %d B/sample", g_ggWave->getSampleRateIn(),  g_ggWave->getSampleSizeBytesIn());
        ImGui::Text("Sample rate (playback): %g, %d B/sample", g_ggWave->getSampleRateOut(), g_ggWave->getSampleSizeBytesOut());

        const float kLabelWidth = ImGui::CalcTextSize("Tx Protocol:  ").x;

        // volume
        ImGui::Text("%s", "");
        {
            auto posSave = ImGui::GetCursorScreenPos();
            ImGui::Text("Volume: ");
            ImGui::SetCursorScreenPos({ posSave.x + kLabelWidth, posSave.y });
        }
        ImGui::SliderFloat("##volume", &settings.volume, 0.0f, 1.0f);

        // protocol
        ImGui::Text("%s", "");
        {
            auto posSave = ImGui::GetCursorScreenPos();
            ImGui::Text("Tx Protocol: ");
            ImGui::SetCursorScreenPos({ posSave.x + kLabelWidth, posSave.y });
        }
        if (ImGui::BeginCombo("##protocol", g_ggWave->getTxProtocols()[settings.protocolId].name)) {
            for (int i = 0; i < (int) g_ggWave->getTxProtocols().size(); ++i) {
                const bool isSelected = (settings.protocolId == i);
                if (ImGui::Selectable(g_ggWave->getTxProtocols()[i].name, isSelected)) {
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

        // no automatic screen resize support for iOS
#ifdef IOS
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

        ImGui::BeginChild("Messages:history", { ImGui::GetContentRegionAvailWidth(), messagesHistoryHeigth }, true);

        ImGui::PushTextWrapPos();
        for (int i = 0; i < (int) messageHistory.size(); ++i) {
            ImGui::PushID(i);
            const auto & message = messageHistory[i];
            if (message.received) {
                ImGui::TextColored({ 0.0f, 1.0f, 0.0f, 1.0f }, "[%s] Recv (%s):", ::toTimeString(message.timestamp), g_ggWave->getTxProtocols()[message.protocolId].name);
                ImGui::SameLine();
                if (ImGui::SmallButton("Resend")) {
                    g_buffer.inputUI.update = true;
                    g_buffer.inputUI.message = { false, std::time(nullptr), message.data, message.protocolId, settings.volume };

                    messageHistory.push_back(g_buffer.inputUI.message);
                }
                ImGui::Text("%s", message.data.c_str());
            } else {
                ImGui::TextColored({ 1.0f, 1.0f, 0.0f, 1.0f }, "[%s] Sent (%s):", ::toTimeString(message.timestamp), g_ggWave->getTxProtocols()[message.protocolId].name);
                ImGui::SameLine();
                if (ImGui::SmallButton("Resend")) {
                    g_buffer.inputUI.update = true;
                    g_buffer.inputUI.message = { false, std::time(nullptr), message.data, message.protocolId, settings.volume };

                    messageHistory.push_back(g_buffer.inputUI.message);
                }
                ImGui::Text("%s", message.data.c_str());
            }
            ImGui::Text("%s", "");
            ImGui::PopID();
        }
        ImGui::PopTextWrapPos();

        if (scrollMessagesToBottom) {
            ImGui::SetScrollHereY();
            scrollMessagesToBottom = false;
        }

        ImVec2 mouse_delta = ImGui::GetIO().MouseDelta;
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
            ImGui::TextDisabled("Listening for waves ...\n");
        }

        if (doInputFocus) {
            ImGui::SetKeyboardFocusHere();
            doInputFocus = false;
        }
        ImGui::PushItemWidth(ImGui::GetContentRegionAvailWidth() - ImGui::CalcTextSize(sendButtonText).x - 2*style.ItemSpacing.x);
        ImGui::InputText("##Messages:Input", inputBuf, 256, ImGuiInputTextFlags_EnterReturnsTrue);
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
        ImGui::SameLine();
        if (ImGui::Button(sendButtonText) && inputBuf[0] != 0) {
            g_buffer.inputUI.update = true;
            g_buffer.inputUI.message = { false, std::time(nullptr), std::string(inputBuf), settings.protocolId, settings.volume };

            messageHistory.push_back(g_buffer.inputUI.message);

            inputBuf[0] = 0;
            doInputFocus = true;
            scrollMessagesToBottom = true;
        }
        if (!ImGui::IsItemHovered() && requestStopTextInput) {
            SDL_StopTextInput();
            isTextInput = false;
            tEndInput = ImGui::GetTime();
        }
    }

    if (windowId == WindowId::Spectrum) {
        ImGui::BeginChild("Spectrum:main", ImGui::GetContentRegionAvail(), true);
        ImGui::PushTextWrapPos();
        {
            auto posSave = ImGui::GetCursorScreenPos();
            ImGui::Text("FPS: %4.2f\n", ImGui::GetIO().Framerate);
            ImGui::SetCursorScreenPos(posSave);
        }
        if (spectrumCurrent.empty() == false) {
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

void deinitMain(std::thread & worker) {
    g_isRunning = false;
    worker.join();

    GGWave_deinit();

    // Cleanup
    ImGui_Shutdown();
    ImGui::DestroyContext();
}
