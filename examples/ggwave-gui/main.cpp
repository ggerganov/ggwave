#include "common.h"

#include <imgui-extra/imgui_impl.h>

#include <SDL_opengl.h>

// ImGui helpers

bool ImGui_beginFrame(SDL_Window * window);
bool ImGui_endFrame(SDL_Window * window);

bool ImGui_beginFrame(SDL_Window * window) {
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

bool ImGui_endFrame(SDL_Window * window) {
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

    ImGui_NewFrame(g_window);
    ImGui::Render();

    initMain();
    auto worker = initWorker();

    while (true) {
        if (ImGui_beginFrame(g_window) == false) {
            break;
        }

        mainRender();

        ImGui_endFrame(g_window);
    }

    deinitMain(worker);

    GGWave_deinit();

    SDL_GL_DeleteContext(g_gl_context);
    SDL_DestroyWindow(g_window);
    SDL_Quit();

    return 0;
}
