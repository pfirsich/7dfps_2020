#pragma once

#include <functional>

#include <SDL2/SDL.h>
#include <imgui.h>
#include <imgui_impl_opengl3.h>
#include <imgui_impl_sdl.h>

void initImgui(SDL_Window* window, SDL_GLContext glContext);
void deinitImgui();

// Only one drawImgui call per frame!
// Also make sure it's called, even if we don't draw anything, so input is handled correctly
// (otherwise widgets that are not drawn keep focus).
void drawImgui(SDL_Window* window, std::function<void(void)> func);
void drawImgui(size_t width, size_t height, std::function<void(void)> func);
