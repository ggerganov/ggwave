#include "ggwave/ggwave.h"

#include "ggwave-common.h"
#include "ggwave-common-sdl2.h"

#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>
#include <imgui-extra/imgui_impl.h>

#include <SDL.h>

#include <cstdio>
#include <string>

#include <mutex>
#include <thread>

static SDL_Window * g_window;
static void * g_gl_context;

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
    printf("    -tN - transmission protocol:\n");
    printf("          -t0 : Normal\n");
    printf("          -t1 : Fast (default)\n");
    printf("          -t2 : Fastest\n");
    printf("          -t3 : Ultrasonic\n");
    printf("\n");

    auto argm = parseCmdArguments(argc, argv);
    int captureId = argm["c"].empty() ? 0 : std::stoi(argm["c"]);
    int playbackId = argm["p"].empty() ? 0 : std::stoi(argm["p"]);
    int txProtocol = argm["t"].empty() ? 1 : std::stoi(argm["t"]);

    if (GGWave_init(playbackId, captureId) == false) {
        fprintf(stderr, "Failed to initialize GGWave\n");
        return -1;
    }

    auto ggWave = GGWave_instance();

    ggWave->setTxMode(GGWave::TxMode::VariableLength);

    printf("Selecting Tx protocol %d\n", txProtocol);
    switch (txProtocol) {
        case 0:
            {
                printf("Using 'Normal' Tx Protocol\n");
                ggWave->setParameters(1, 40, 9, 3, 50);
            }
            break;
        case 1:
            {
                printf("Using 'Fast' Tx Protocol\n");
                ggWave->setParameters(1, 40, 6, 3, 50);
            }
            break;
        case 2:
            {
                printf("Using 'Fastest' Tx Protocol\n");
                ggWave->setParameters(1, 40, 3, 3, 50);
            }
            break;
        case 3:
            {
                printf("Using 'Ultrasonic' Tx Protocol\n");
                ggWave->setParameters(1, 320, 9, 3, 50);
            }
            break;
        default:
            {
                printf("Using 'Fast' Tx Protocol\n");
                ggWave->setParameters(1, 40, 6, 3, 50);
            }
    };
    printf("\n");

    ggWave->init(0, "");

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

    while (true) {
        GGWave_mainLoop();

        if (ImGui_beginFrame(g_window) == false) {
            break;
        }

        static char inputBuf[256];
        static bool isTextInput = false;
        static double tStartInput = 0.0f;
        static double tEndInput = -100.0f;

        const double tShowKeyboard = 0.2f;

        const auto& displaySize = ImGui::GetIO().DisplaySize;
        auto& style = ImGui::GetStyle();

        const float statusBarHeight = 44.0f + 2.0f*style.ItemSpacing.y;
        const float menuButtonHeight = 40.0f + 2.0f*style.ItemSpacing.y;

        ImGui::SetNextWindowPos({ 0, 0, });
        ImGui::SetNextWindowSize(displaySize);
        ImGui::Begin("Main", nullptr,
                     ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_NoTitleBar |
                     ImGuiWindowFlags_NoScrollbar |
                     ImGuiWindowFlags_NoSavedSettings);

        ImGui::InvisibleButton("StatusBar", { ImGui::GetContentRegionAvailWidth(), statusBarHeight });

        if (ImGui::Button("Messages", { 0.5f*ImGui::GetContentRegionAvailWidth(), menuButtonHeight })) {
        }
        ImGui::SameLine();

        if (ImGui::Button("Commands", { 1.0f*ImGui::GetContentRegionAvailWidth(), menuButtonHeight })) {
        }

        const float messagesInputHeight = ImGui::GetTextLineHeightWithSpacing();
        const float messagesHistoryHeigthMax = ImGui::GetContentRegionAvail().y - messagesInputHeight - 2.0f*style.FramePadding.y;
        float messagesHistoryHeigth = messagesHistoryHeigthMax;

        if (isTextInput) {
          messagesHistoryHeigth -= 0.5f*messagesHistoryHeigthMax*std::min(tShowKeyboard, ImGui::GetTime() - tStartInput) / tShowKeyboard;
        } else {
          messagesHistoryHeigth -= 0.5f*messagesHistoryHeigthMax - 0.5f*messagesHistoryHeigthMax*std::min(tShowKeyboard, ImGui::GetTime() - tEndInput) / tShowKeyboard;
        }

        ImGui::BeginChild("Messages:history", { ImGui::GetContentRegionAvailWidth(), messagesHistoryHeigth }, true);

        for (int i = 0; i < 100; ++i) {
            ImGui::Text("SAA        sadfa line %d\n", i);
        }


        ImVec2 mouse_delta = ImGui::GetIO().MouseDelta;
        ScrollWhenDraggingOnVoid(ImVec2(0.0f, -mouse_delta.y), ImGuiMouseButton_Left);
        ImGui::EndChild();

        ImGui::PushItemWidth(ImGui::GetContentRegionAvailWidth() - 2*ImGui::CalcTextSize("Send").x);
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
        if (ImGui::Button("Send")) {
            std::string input(inputBuf);
            ggWave->init(input.size(), input.data());
            inputBuf[0] = 0;
        }
        if (!ImGui::IsItemHovered() && requestStopTextInput) {
            SDL_StopTextInput();
            isTextInput = false;
            tEndInput = ImGui::GetTime();
        }

        ImGui::End();

        ImGui_endFrame(g_window);
    }

    GGWave_deinit();

    return 0;
}
