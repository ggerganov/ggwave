/*! \file imgui_impl.h
 *  \brief Enter description here.
 */

#pragma once

#if (defined(__EMSCRIPTEN__))
#define IMGUI_IMPL_OPENGL_LOADER_GLEW
#endif

#include "imgui/imgui.h"

struct SDL_Window;
typedef void * SDL_GLContext;
typedef union SDL_Event SDL_Event;

IMGUI_API bool ImGui_PreInit();
IMGUI_API ImGuiContext* ImGui_Init(SDL_Window* window, SDL_GLContext gl_context);

void IMGUI_API ImGui_Shutdown();
void IMGUI_API ImGui_NewFrame(SDL_Window* window);
bool IMGUI_API ImGui_ProcessEvent(const SDL_Event* event);

void IMGUI_API ImGui_RenderDrawData(ImDrawData* draw_data);

bool IMGUI_API ImGui_CreateFontsTexture();
void IMGUI_API ImGui_DestroyFontsTexture();
bool IMGUI_API ImGui_CreateDeviceObjects();
void IMGUI_API ImGui_DestroyDeviceObjects();

void IMGUI_API ImGui_SaveState(int id);
void IMGUI_API ImGui_LoadState(int id);
