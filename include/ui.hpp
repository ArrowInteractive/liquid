#pragma once

#include "SDL.h"
#include "ui.hpp"
#include "imgui.h"
#include "imgui_impl_sdl.h"
#include "imgui_impl_sdlrenderer.h"
#include "SDL_opengl.h"

void init_imgui(SDL_Window* window, SDL_Renderer* renderer);

void update_imgui(SDL_Renderer* renderer);

void destroy_imgui_data();

void imgui_event_handler(SDL_Event& event);

bool want_capture_mouse();

bool want_capture_keyboard();