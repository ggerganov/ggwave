#include "common.h"

#include "ggwave/ggwave.h"

#include "ggwave-common.h"

#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>

#include <SDL.h>

#include <atomic>
#include <cmath>
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
#define ICON_FA_ARROW_CIRCLE_DOWN "V"
#define ICON_FA_PASTE "P"
#endif

struct Message {
    bool received;
    std::chrono::system_clock::time_point timestamp;
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
    GGWave::AmplitudeData16 txAmplitudeData;
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

char * toTimeString(const std::chrono::system_clock::time_point & tp) {
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

GGWave * g_ggWave;
Buffer g_buffer;
std::atomic<bool> g_isRunning;

std::thread initMain() {
    g_isRunning = true;
    g_ggWave = GGWave_instance();

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
                        (int) inputCurrent.message.data.size(),
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
                    std::chrono::system_clock::now(),
                    std::string((char *) lastRxData.data(), lastRxDataLength),
                    g_ggWave->getRxProtocolId(),
                    0,
                };
            }

            if (g_ggWave->takeSpectrum(g_buffer.stateCore.spectrum)) {
                g_buffer.stateCore.update = true;
                g_buffer.stateCore.flags.newSpectrum = true;
            }

            if (g_ggWave->takeTxAmplitudeData16(g_buffer.stateCore.txAmplitudeData)) {
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

            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    });
}

void renderMain() {
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

    const double tHoldContextPopup = 0.5f;

    const int kMaxInputSize = 140;
    static char inputBuf[kMaxInputSize];

    static bool doInputFocus = false;
    static bool lastMouseButtonLeft = 0;
    static bool isTextInput = false;
    static bool scrollMessagesToBottom = true;

    static double tStartInput = 0.0f;
    static double tEndInput = -100.0f;
    static double tStartTx = 0.0f;
    static double tLengthTx = 0.0f;

    static GGWaveStats statsCurrent;
    static GGWave::SpectrumData spectrumCurrent;
    static GGWave::AmplitudeData16 txAmplitudeDataCurrent;
    static std::vector<Message> messageHistory;

    if (stateCurrent.update) {
        if (stateCurrent.flags.newMessage) {
            scrollMessagesToBottom = true;
            messageHistory.push_back(std::move(stateCurrent.message));
        }
        if (stateCurrent.flags.newSpectrum) {
            spectrumCurrent = std::move(stateCurrent.spectrum);
        }
        if (stateCurrent.flags.newTxAmplitudeData) {
            txAmplitudeDataCurrent = std::move(stateCurrent.txAmplitudeData);

            tStartTx = ImGui::GetTime() + (16.0f*1024.0f)/g_ggWave->getSampleRateOut();
            tLengthTx = txAmplitudeDataCurrent.size()/g_ggWave->getSampleRateOut();
            {
                auto & ampl = txAmplitudeDataCurrent;
                int nBins = 512;
                int nspb = ampl.size()/nBins;
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
        ImGui::Text("Waver v1.2.0");
        ImGui::Separator();

        ImGui::Text("%s", "");
        ImGui::Text("Sample rate (capture):  %g, %d B/sample", g_ggWave->getSampleRateIn(),  g_ggWave->getSampleSizeBytesIn());
        ImGui::Text("Sample rate (playback): %g, %d B/sample", g_ggWave->getSampleRateOut(), g_ggWave->getSampleSizeBytesOut());

        const float kLabelWidth = ImGui::CalcTextSize("Tx Protocol:  ").x;

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

        bool showScrollToBottom = false;
        const auto wPos0 = ImGui::GetCursorScreenPos();
        const auto wSize = ImVec2 { ImGui::GetContentRegionAvailWidth(), messagesHistoryHeigth };

        ImGui::BeginChild("Messages:history", wSize, true, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

        static bool isHoldingMessage = false;
        static bool isHoldingInput = false;
        static int messageIdHolding = 0;

        const auto & mouse_delta = ImGui::GetIO().MouseDelta;
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

            ImGui::TextColored(msgColor, "[%s] %s (%s):", ::toTimeString(message.timestamp), msgStatus, g_ggWave->getTxProtocols()[message.protocolId].name);

            {
                auto p0 = ImGui::GetCursorScreenPos();
                auto p00 = p0;
                p00.y -= ImGui::GetTextLineHeightWithSpacing();
                p0.x -= style.ItemSpacing.x;
                p0.y -= 0.5f*style.ItemSpacing.y;

                auto col = style.Colors[ImGuiCol_Text];
                col.w = interp;
                ImGui::TextColored(col, "%s", message.data.c_str());

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

            if (ImGui::Button("Resend")) {
                g_buffer.inputUI.update = true;
                g_buffer.inputUI.message = { false, std::chrono::system_clock::now(), messageSelected.data, messageSelected.protocolId, settings.volume };

                messageHistory.push_back(g_buffer.inputUI.message);
                ImGui::CloseCurrentPopup();
            }

            ImGui::SameLine();
            ImGui::TextDisabled("|");

            ImGui::SameLine();
            if (ImGui::Button("Copy")) {
                SDL_SetClipboardText(messageSelected.data.c_str());
                ImGui::CloseCurrentPopup();
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
                wSize.y = ImGui::GetTextLineHeight();

                ImGui::PushStyleColor(ImGuiCol_FrameBg, { 0.3f, 0.3f, 0.3f, 0.3f });
                ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
                ImGui::PlotHistogram("##plotSpectrumCurrent",
                                     Funcs::Sample,
                                     txAmplitudeDataCurrent.data(),
                                     txAmplitudeDataCurrent.size(), 0,
                                     (std::string("")).c_str(),
                                     0.0f, FLT_MAX, wSize);
                ImGui::PopStyleColor(2);

                ImGui::SetCursorScreenPos(posSave);

                ImGui::PushStyleColor(ImGuiCol_FrameBg, { 0.0f, 0.0f, 0.0f, 0.0f });
                ImGui::PushStyleColor(ImGuiCol_PlotHistogram, { 0.0f, 1.0f, 0.0f, 1.0f });
                ImGui::PlotHistogram("##plotSpectrumCurrent",
                                     Funcs::SampleFrac,
                                     txAmplitudeDataCurrent.data(),
                                     txAmplitudeDataCurrent.size(), 0,
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

        ImGui::PushItemWidth(ImGui::GetContentRegionAvailWidth() - ImGui::CalcTextSize(sendButtonText).x - 2*style.ItemSpacing.x);
        ImGui::InputText("##Messages:Input", inputBuf, kMaxInputSize, ImGuiInputTextFlags_EnterReturnsTrue);
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
        if (ImGui::Button(sendButtonText) && inputBuf[0] != 0) {
            g_buffer.inputUI.update = true;
            g_buffer.inputUI.message = { false, std::chrono::system_clock::now(), std::string(inputBuf), settings.protocolId, settings.volume };

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
}
