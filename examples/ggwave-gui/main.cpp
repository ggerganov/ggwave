#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>
#include <imgui-extra/imgui_impl.h>

#include "common.h"

#include <SDL_opengl.h>

// ImGui helpers

bool ImGui_BeginFrame(SDL_Window * window);
bool ImGui_EndFrame(SDL_Window * window);

bool ImGui_BeginFrame(SDL_Window * window) {
    SDL_Event event;
    while (SDL_PollEvent(&event))
    {
        ImGui_ProcessEvent(&event);
        if (event.type == SDL_QUIT) return false;
        if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_CLOSE && event.window.windowID == SDL_GetWindowID(window)) return false;
    }

    ImGui_NewFrame(window);

    return true;
}

bool ImGui_EndFrame(SDL_Window * window) {
    // Rendering
    int display_w, display_h;
    SDL_GetWindowSize(window, &display_w, &display_h);
    glViewport(0, 0, display_w, display_h);
    glClearColor(0.0f, 0.0f, 0.0f, 0.4f);
    glClear(GL_COLOR_BUFFER_BIT);

    ImGui::Render();
    ImGui_RenderDrawData(ImGui::GetDrawData());

    SDL_GL_SwapWindow(window);

    return true;
}

bool ImGui_SetStyle() {
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

    return true;
}

static SDL_Window * g_window;
static void * g_gl_context;

int main(int argc, char** argv) {
    auto argm = parseCmdArguments(argc, argv);
    int captureId = argm["c"].empty() ? 0 : std::stoi(argm["c"]);
    int playbackId = argm["p"].empty() ? 0 : std::stoi(argm["p"]);

    if (GGWave_init(playbackId, captureId) == false) {
        fprintf(stderr, "Failed to initialize GGWave\n");
        return -1;
    }

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
    ImGui_SetStyle();

    ImGui_NewFrame(g_window);
    ImGui::Render();

    auto worker = initMain();

    while (true) {
        if (ImGui_BeginFrame(g_window) == false) {
            break;
        }

        renderMain();

        ImGui_EndFrame(g_window);
    }

    deinitMain(worker);
    GGWave_deinit();

    // Cleanup
    ImGui_Shutdown();
    ImGui::DestroyContext();

    SDL_GL_DeleteContext(g_gl_context);
    SDL_DestroyWindow(g_window);
    SDL_Quit();

    return 0;
}
