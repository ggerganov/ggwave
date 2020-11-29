#include "ggwave/ggwave.h"

#include "ggwave-common.h"
#include "ggwave-common-sdl2.h"

#include <imgui/imgui.h>
#include <imgui-extra/imgui_impl.h>

#include <SDL.h>
#include <SDL_audio.h>
#include <SDL_opengl.h>

#include <cstdio>
#include <string>

#include <mutex>
#include <thread>
#include <iostream>

static SDL_Window * g_window;
static void * g_gl_context;

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

    std::mutex mutex;
    std::thread inputThread([&]() {
        std::string inputOld = "";
        while (true) {
            std::string input;
            std::cout << "Enter text: ";
            getline(std::cin, input);
            if (input.empty()) {
                std::cout << "Re-sending ... " << std::endl;
                input = inputOld;
            } else {
                std::cout << "Sending ... " << std::endl;
            }
            {
                std::lock_guard<std::mutex> lock(mutex);
                ggWave->init(input.size(), input.data());
            }
            inputOld = input;
        }
    });

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
    SDL_GL_SetSwapInterval(0); // Enable vsync

    ImGui_Init(g_window, g_gl_context);

    ImGui_NewFrame(g_window);
    ImGui::Render();

    while (true) {
        {
            std::lock_guard<std::mutex> lock(mutex);
            GGWave_mainLoop();
        }

        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            ImGui_ProcessEvent(&event);
            if (event.type == SDL_QUIT) {}
            if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_CLOSE && event.window.windowID == SDL_GetWindowID(g_window)) {}
        }

        ImGui_NewFrame(g_window);

        ImGui::ShowDemoWindow();

        // Rendering
        int display_w, display_h;
        SDL_GetWindowSize(g_window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.0f, 0.0f, 0.0f, 0.4f);
        glClear(GL_COLOR_BUFFER_BIT);

        ImGui::Render();
        ImGui_RenderDrawData(ImGui::GetDrawData());

        SDL_GL_SwapWindow(g_window);
    }

    inputThread.join();

    GGWave_deinit();

    return 0;
}
