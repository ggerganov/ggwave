#include "common.h"

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

    SDL_GL_DeleteContext(g_gl_context);
    SDL_DestroyWindow(g_window);
    SDL_Quit();

    return 0;
}
