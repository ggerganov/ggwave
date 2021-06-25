#include "interface.h"

#include "ggwave-common.h"

#ifdef __EMSCRIPTEN__
#include "build_timestamp.h"
#include "emscripten/emscripten.h"
#else
#define EMSCRIPTEN_KEEPALIVE
#endif

#include <imgui-extra/imgui_impl.h>

#include <SDL.h>
#include <SDL_opengl.h>

#include <fstream>
#include <vector>
#include <functional>

const float kGlobalImGuiScale = 1.25f;

// ImGui helpers

bool ImGui_tryLoadFont(const std::string & filename, float size = 14.0f, bool merge = false) {
    printf("Trying to load font from '%s' ..\n", filename.c_str());
    std::ifstream f(filename);
    if (f.good() == false) {
        printf(" - failed\n");
        return false;
    }
    printf(" - success\n");
    if (merge) {
        // todo : ugly static !!!
        static ImWchar ranges[] = { 0xf000, 0xf3ff, 0 };
        static ImFontConfig config;

        config.MergeMode = true;
        config.GlyphOffset = { 0.0f, 0.0f };

        ImGui::GetIO().Fonts->AddFontFromFileTTF(filename.c_str(), size, &config, ranges);
    } else {
        ImGui::GetIO().Fonts->AddFontFromFileTTF(filename.c_str(), size);
    }
    return true;
}

bool ImGui_BeginFrame(SDL_Window * window) {
    SDL_Event event;
    while (SDL_PollEvent(&event))
    {
        ImGui_ProcessEvent(&event);
        if (event.type == SDL_QUIT) return false;
        if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_CLOSE && event.window.windowID == SDL_GetWindowID(window)) return false;
        if (event.type == SDL_DROPFILE) {
            printf("Dropped file: '%s'\n", event.drop.file);
            auto data = readFile(event.drop.file);
            if (data.empty()) {
                fprintf(stderr, "Unable to access file. Probably missing permissions\n");
                continue;
            }

            std::string uri = event.drop.file;
            std::string filename = event.drop.file;
            if (uri.find("/") || uri.find("\\")) {
                filename = uri.substr(uri.find_last_of("/\\") + 1);
            }
            addFile(uri.c_str(), filename.c_str(), std::move(data), true);
        }
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

    style.AntiAliasedFill = true;
    style.AntiAliasedLines = true;
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
    style.Colors[ImGuiCol_WindowBg]              = ImVec4(0.11f, 0.15f, 0.20f, 0.60f);
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

    style.ScaleAllSizes(kGlobalImGuiScale);

    return true;
}

static std::function<bool()> g_doInit;
static std::function<void(int, int)> g_setWindowSize;
static std::function<bool()> g_mainUpdate;

void mainUpdate(void *) {
    g_mainUpdate();
}

// JS interface

extern "C" {
    EMSCRIPTEN_KEEPALIVE
        int do_init() {
            return g_doInit();
        }

    EMSCRIPTEN_KEEPALIVE
        void set_window_size(int sizeX, int sizeY) {
            g_setWindowSize(sizeX, sizeY);
        }
}

int main(int argc, char** argv) {
#ifdef __EMSCRIPTEN__
    printf("Build time: %s\n", BUILD_TIMESTAMP);
    printf("Press the Init button to start\n");

    if (argv[1]) {
        GGWave_setDefaultCaptureDeviceName(argv[1]);
    }
#endif

    auto argm = parseCmdArguments(argc, argv);
    int captureId = argm["c"].empty() ? 0 : std::stoi(argm["c"]);
    int playbackId = argm["p"].empty() ? 0 : std::stoi(argm["p"]);

    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "Error: %s\n", SDL_GetError());
        return -1;
    }

    ImGui_PreInit();

    int windowX = 400;
    int windowY = 600;

    // Waver video settings
    //float scale = 0.65;

    //int windowX = scale*570;
    //int windowY = scale*917;

    const char * windowTitle = "Waver";

#ifdef __EMSCRIPTEN__
    SDL_Renderer * renderer;
    SDL_Window * window;
    SDL_CreateWindowAndRenderer(windowX, windowY, SDL_WINDOW_OPENGL, &window, &renderer);
#else
    SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    SDL_Window * window = SDL_CreateWindow(windowTitle, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, windowX, windowY, window_flags);
#endif

    void * gl_context = SDL_GL_CreateContext(window);

    SDL_GL_MakeCurrent(window, gl_context);
    SDL_GL_SetSwapInterval(1); // Enable vsync

    ImGui_Init(window, gl_context);
    ImGui::GetIO().IniFilename = nullptr;

    {
        bool isNotLoaded = true;
        isNotLoaded = isNotLoaded && !ImGui_tryLoadFont(getBinaryPath() + "DroidSans.ttf", kGlobalImGuiScale*14.0f, false);
        isNotLoaded = isNotLoaded && !ImGui_tryLoadFont(getBinaryPath() + "../bin/DroidSans.ttf", kGlobalImGuiScale*14.0f, false);
        isNotLoaded = isNotLoaded && !ImGui_tryLoadFont(getBinaryPath() + "../examples/assets/fonts/DroidSans.ttf", kGlobalImGuiScale*14.0f, false);
        isNotLoaded = isNotLoaded && !ImGui_tryLoadFont(getBinaryPath() + "../../examples/assets/fonts/DroidSans.ttf", kGlobalImGuiScale*14.0f, false);
    }

    {
        bool isNotLoaded = true;
        isNotLoaded = isNotLoaded && !ImGui_tryLoadFont(getBinaryPath() + "fontawesome-webfont.ttf", kGlobalImGuiScale*14.0f, true);
        isNotLoaded = isNotLoaded && !ImGui_tryLoadFont(getBinaryPath() + "../bin/fontawesome-webfont.ttf", kGlobalImGuiScale*14.0f, true);
        isNotLoaded = isNotLoaded && !ImGui_tryLoadFont(getBinaryPath() + "../examples/assets/fonts/fontawesome-webfont.ttf", kGlobalImGuiScale*14.0f, true);
        isNotLoaded = isNotLoaded && !ImGui_tryLoadFont(getBinaryPath() + "../../examples/assets/fonts/fontawesome-webfont.ttf", kGlobalImGuiScale*14.0f, true);
    }

    ImGui_SetStyle();

    SDL_EventState(SDL_DROPFILE, SDL_ENABLE);

    ImGui_NewFrame(window);
    ImGui::Render();

    // tmp
    //addFile("test0.raw", "test0.raw", std::vector<char>(1024));
    //addFile("test1.jpg", "test0.jpg", std::vector<char>(1024*1024 + 624));
    //addFile("test2.mpv", "test0.mov", std::vector<char>(1024*1024*234 + 53827));

    bool isInitialized = false;
    std::thread worker;

    g_doInit = [&]() {
        if (GGWave_init(playbackId, captureId) == false) {
            fprintf(stderr, "Failed to initialize GGWave\n");
            return false;
        }

#ifdef __EMSCRIPTEN__
        initMain();
#else
        worker = initMainAndRunCore();
#endif

        isInitialized = true;

        return true;
    };

    g_setWindowSize = [&](int sizeX, int sizeY) {
        SDL_SetWindowSize(window, sizeX, sizeY);
    };

    g_mainUpdate = [&]() {
        if (isInitialized == false) {
            return true;
        }

        if (ImGui_BeginFrame(window) == false) {
            return false;
        }

        renderMain();
        updateMain();

        ImGui_EndFrame(window);

#ifdef __EMSCRIPTEN__
        updateCore();
#endif

        return true;
    };

#ifdef __EMSCRIPTEN__
    emscripten_set_main_loop_arg(mainUpdate, NULL, 60, true);
#else
    if (g_doInit() == false) {
        printf("Error: failed to initialize audio\n");
        return -2;
    }

    while (true) {
        if (g_mainUpdate() == false) break;
    }

    deinitMain();
    worker.join();

    GGWave_deinit();

    // Cleanup
    ImGui_Shutdown();
    ImGui::DestroyContext();

    SDL_GL_DeleteContext(gl_context);
    SDL_DestroyWindow(window);
    SDL_CloseAudio();
    SDL_Quit();
#endif

    return 0;
}
