#include "ggwave/ggwave.h"

#include "ggwave-common.h"
#include "ggwave-common-sdl2.h"

#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>
#include <imgui-extra/imgui_impl.h>

#include <SDL.h>

#include <cstdio>
#include <string>
#include <ctime>
#include <atomic>
#include <mutex>
#include <thread>
#include <vector>

static SDL_Window * g_window;
static void * g_gl_context;

#define ICON_FA_COGS "#"
#define ICON_FA_COMMENT_ALT ""
#define ICON_FA_LIST_UL ""
#define ICON_FA_PLAY_CIRCLE ""

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

int main(int argc, char** argv) {
    printf("Usage: %s [-cN] [-pN] [-tN]\n", argv[0]);
    printf("    -cN - select capture device N\n");
    printf("    -pN - select playback device N\n");
    printf("\n");

    auto argm = parseCmdArguments(argc, argv);
    int captureId = argm["c"].empty() ? 0 : std::stoi(argm["c"]);
    int playbackId = argm["p"].empty() ? 0 : std::stoi(argm["p"]);

    if (GGWave_init(playbackId, captureId) == false) {
        fprintf(stderr, "Failed to initialize GGWave\n");
        return -1;
    }

    auto ggWave = GGWave_instance();

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_GAMECONTROLLER) != 0) {
        fprintf(stderr, "Error: %s\n", SDL_GetError());
        return -1;
    }

    int windowX = 400;
    int windowY = 600;
    const char * windowTitle = "ggwave-gui";

    SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    g_window = SDL_CreateWindow(windowTitle, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, windowX, windowY, window_flags);

    g_gl_context = SDL_GL_CreateContext(g_window);
    SDL_GL_MakeCurrent(g_window, g_gl_context);
    SDL_GL_SetSwapInterval(1); // Enable vsync

    ImGui_Init(g_window, g_gl_context);

    ImGui_NewFrame(g_window);
    ImGui::Render();

    std::atomic<bool> isRunning { true };

    struct Message {
        bool received;
        std::time_t timestamp;
        std::string data;
        int protocolId;
        float volume;
    };

    struct State {
        bool update = false;
        Message message;
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

    Buffer buffer;

    auto worker = std::thread([&]() {
        Input inputCurrent;

        int lastRxDataLength = 0;
        GGWave::TxRxData lastRxData;

        while (isRunning) {
            {
                std::lock_guard<std::mutex> lock(buffer.mutex);
                if (buffer.inputCore.update) {
                    inputCurrent = std::move(buffer.inputCore);
                    buffer.inputCore.update = false;
                }
            }

            if (inputCurrent.update) {
                ggWave->init(
                        inputCurrent.message.data.size(),
                        inputCurrent.message.data.data(),
                        ggWave->getTxProtocols()[inputCurrent.message.protocolId],
                        100*inputCurrent.message.volume);

                inputCurrent.update = false;
            }

            GGWave_mainLoop();

            lastRxDataLength = ggWave->takeRxData(lastRxData);
            if (lastRxDataLength > 0) {
                buffer.stateCore.update = true;
                buffer.stateCore.message = {
                    true,
                    std::time(nullptr),
                    std::string((char *) lastRxData.data(), lastRxDataLength),
                    ggWave->getRxProtocolId(),
                    0,
                };
            }

            {
                std::lock_guard<std::mutex> lock(buffer.mutex);
                if (buffer.stateCore.update) {
                    buffer.stateUI = std::move(buffer.stateCore);
                    buffer.stateCore.update = false;
                }
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
    });

    while (true) {
        if (ImGui_beginFrame(g_window) == false) {
            break;
        }

        static State stateCurrent;

        {
            std::lock_guard<std::mutex> lock(buffer.mutex);
            if (buffer.stateUI.update) {
                stateCurrent = std::move(buffer.stateUI);
                buffer.stateUI.update = false;
            }
        }

        enum class WindowId {
            Settings,
            Messages,
            Commands,
        };

        struct Settings {
            int protocolId = 1;
            float volume = 0.25f;
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

        static std::vector<Message> messageHistory;

        if (stateCurrent.update) {
            messageHistory.push_back(std::move(stateCurrent.message));
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
        const float statusBarHeight = displaySize.x < displaySize.y ? 20.0f + 2.0f*style.ItemSpacing.y : 0.1f;
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

        if (ImGui::Button(ICON_FA_LIST_UL "  Commands", { 1.0f*ImGui::GetContentRegionAvailWidth(), menuButtonHeight })) {
            windowId = WindowId::Commands;
        }

        if (windowId == WindowId::Settings) {
            ImGui::BeginChild("Settings:main", ImGui::GetContentRegionAvail(), true);
            ImGui::Text("Waver v0.1");
            ImGui::Separator();

            ImGui::Text("%s", "");
            ImGui::Text("Sample rate (capture):  %g, %d B/sample", ggWave->getSampleRateIn(),  ggWave->getSampleSizeBytesIn());
            ImGui::Text("Sample rate (playback): %g, %d B/sample", ggWave->getSampleRateOut(), ggWave->getSampleSizeBytesOut());

            static float kLabelWidth = 100.0f;

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
            ImGui::SameLine();
            if (ImGui::BeginCombo("##protocol", ggWave->getTxProtocols()[settings.protocolId].name)) {
                for (int i = 0; i < (int) ggWave->getTxProtocols().size(); ++i) {
                    const bool isSelected = (settings.protocolId == i);
                    if (ImGui::Selectable(ggWave->getTxProtocols()[i].name, isSelected)) {
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
            const float messagesInputHeight = ImGui::GetTextLineHeightWithSpacing();
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

            for (int i = 0; i < (int) messageHistory.size(); ++i) {
                ImGui::PushID(i);
                const auto & message = messageHistory[i];
                if (message.received) {
                    ImGui::TextColored({ 0.0f, 1.0f, 0.0f, 1.0f }, "[%s] Recv (%s):", ::toTimeString(message.timestamp), ggWave->getTxProtocols()[message.protocolId].name);
                    ImGui::SameLine();
                    if (ImGui::SmallButton("Resend")) {
                        buffer.inputUI.update = true;
                        buffer.inputUI.message = { false, std::time(nullptr), message.data, message.protocolId, settings.volume };

                        messageHistory.push_back(buffer.inputUI.message);
                    }
                    ImGui::Text("%s", message.data.c_str());
                } else {
                    ImGui::TextColored({ 1.0f, 1.0f, 0.0f, 1.0f }, "[%s] Sent (%s):", ::toTimeString(message.timestamp), ggWave->getTxProtocols()[message.protocolId].name);
                    ImGui::SameLine();
                    if (ImGui::SmallButton("Resend")) {
                        buffer.inputUI.update = true;
                        buffer.inputUI.message = { false, std::time(nullptr), message.data, message.protocolId, settings.volume };

                        messageHistory.push_back(buffer.inputUI.message);
                    }
                    ImGui::Text("%s", message.data.c_str());
                }
                ImGui::Text("%s", "");
                ImGui::PopID();
            }

            if (scrollMessagesToBottom) {
                ImGui::SetScrollHereY();
                scrollMessagesToBottom = false;
            }

            ImVec2 mouse_delta = ImGui::GetIO().MouseDelta;
            ScrollWhenDraggingOnVoid(ImVec2(0.0f, -mouse_delta.y), ImGuiMouseButton_Left);
            ImGui::EndChild();

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
                buffer.inputUI.update = true;
                buffer.inputUI.message = { false, std::time(nullptr), std::string(inputBuf), settings.protocolId, settings.volume };

                messageHistory.push_back(buffer.inputUI.message);

                inputBuf[0] = 0;
                doInputFocus = true;
            }
            if (!ImGui::IsItemHovered() && requestStopTextInput) {
                SDL_StopTextInput();
                isTextInput = false;
                tEndInput = ImGui::GetTime();
            }
        }

        if (windowId == WindowId::Commands) {
            ImGui::BeginChild("Commands:main", ImGui::GetContentRegionAvail(), true);
            ImGui::Text("Todo");
            ImGui::EndChild();
        }

        ImGui::End();

        ImGui::GetIO().KeysDown[ImGui::GetIO().KeyMap[ImGuiKey_Backspace]] = false;
        ImGui::GetIO().KeysDown[ImGui::GetIO().KeyMap[ImGuiKey_Enter]] = false;

        ImGui_endFrame(g_window);

        {
            std::lock_guard<std::mutex> lock(buffer.mutex);
            if (buffer.inputUI.update) {
                buffer.inputCore = std::move(buffer.inputUI);
                buffer.inputUI.update = false;
            }
        }
    }

    isRunning = false;
    worker.join();

    GGWave_deinit();

    // Cleanup
    ImGui_Shutdown();
    ImGui::DestroyContext();

    SDL_GL_DeleteContext(g_gl_context);
    SDL_DestroyWindow(g_window);
    SDL_Quit();

    return 0;
}
