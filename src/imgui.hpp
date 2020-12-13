#pragma once

#include <functional>

#include <SDL2/SDL.h>
#include <imgui.h>
#include <imgui_impl_opengl3.h>
#include <imgui_impl_sdl.h>

void initImgui(SDL_Window* window, SDL_GLContext glContext);
void deinitImgui();
void drawImgui(SDL_Window* window, std::function<void(void)> func);
