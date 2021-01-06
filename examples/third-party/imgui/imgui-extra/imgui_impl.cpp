#include "imgui-extra/imgui_impl.h"

#include "imgui-extra/imgui_impl_sdl.h"
#include "imgui-extra/imgui_impl_opengl3.h"

#if defined(IMGUI_IMPL_OPENGL_LOADER_GL3W)
#include <GL/gl3w.h>    // Initialize with gl3wInit()
#elif defined(IMGUI_IMPL_OPENGL_LOADER_GLEW)
#include <GL/glew.h>    // Initialize with glewInit()
#elif defined(IMGUI_IMPL_OPENGL_LOADER_GLAD)
#include <glad/glad.h>  // Initialize with gladLoadGL()
#else
#include IMGUI_IMPL_OPENGL_LOADER_CUSTOM
#endif

#include <SDL.h>

#include <cstdio>

bool ImGui_PreInit() {
    // Decide GL+GLSL versions
#if __APPLE__
    // GL 3.2 Core + GLSL 150
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG); // Always required on Mac
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
#elif __EMSCRIPTEN__
    const char* glsl_version = "#version 100";
    //const char* glsl_version = "#version 300 es";
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
#else
    // GL 3.0 + GLSL 130
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
#endif

    // Create window with graphics context
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);

    return true;
}

ImGuiContext* ImGui_Init(SDL_Window* window, SDL_GLContext gl_context) {
    // Decide GL+GLSL versions
#if __APPLE__
    // GL 3.2 Core + GLSL 150
    const char* glsl_version = "#version 150";
#elif __EMSCRIPTEN__
    const char* glsl_version = "#version 100";
#else
    // GL 3.0 + GLSL 130
    const char* glsl_version = "#version 130";
#endif

    static bool isInitialized = false;
    if (!isInitialized) {
        // Initialize OpenGL loader
#if defined(IMGUI_IMPL_OPENGL_LOADER_GL3W)
        bool err = gl3wInit() != 0;
#elif defined(IMGUI_IMPL_OPENGL_LOADER_GLEW)
        bool err = glewInit() != GLEW_OK;
#elif defined(IMGUI_IMPL_OPENGL_LOADER_GLAD)
        bool err = gladLoadGL() == 0;
#else
        bool err = false; // If you use IMGUI_IMPL_OPENGL_LOADER_CUSTOM, your loader is likely to requires some form of initialization.
#endif

        if (err) {
            fprintf(stderr, "Failed to initialize OpenGL loader!\n");
            return nullptr;
        }
        isInitialized = true;
    }

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    auto ctx = ImGui::CreateContext();
    ImGui::SetCurrentContext(ctx);

    ImGuiIO& io = ImGui::GetIO(); (void)io;
    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;  // Enable Keyboard Controls
    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;   // Enable Gamepad Controls

    // Setup Platform/Renderer bindings
    bool res = true;
    res &= ImGui_ImplSDL2_InitForOpenGL(window, gl_context);
    res &= ImGui_ImplOpenGL3_Init(glsl_version);

    return res ? ctx : nullptr;
}

void ImGui_Shutdown() { ImGui_ImplOpenGL3_Shutdown(); ImGui_ImplSDL2_Shutdown(); }
void ImGui_NewFrame(SDL_Window* window) { ImGui_ImplOpenGL3_NewFrame(); ImGui_ImplSDL2_NewFrame(window); ImGui::NewFrame(); }
bool ImGui_ProcessEvent(const SDL_Event* event) { return ImGui_ImplSDL2_ProcessEvent(event); }

void ImGui_RenderDrawData(ImDrawData* draw_data)    { ImGui_ImplOpenGL3_RenderDrawData(draw_data); }

bool ImGui_CreateFontsTexture()     { return ImGui_ImplOpenGL3_CreateFontsTexture(); }
void ImGui_DestroyFontsTexture()    { ImGui_ImplOpenGL3_DestroyFontsTexture(); }
bool ImGui_CreateDeviceObjects()    { return ImGui_ImplOpenGL3_CreateDeviceObjects(); }
void ImGui_DestroyDeviceObjects()   { ImGui_ImplOpenGL3_DestroyDeviceObjects(); }

void ImGui_SaveState(int id) { ImGui_ImplSDL2_SaveState(id); ImGui_ImplOpenGL3_SaveState(id); }
void ImGui_LoadState(int id) { ImGui_ImplSDL2_LoadState(id); ImGui_ImplOpenGL3_LoadState(id); }
