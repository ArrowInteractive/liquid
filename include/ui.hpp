#pragma once

/*
**  Includes
*/

#include <imgui.h>
#include <imgui_impl_sdl.h>
#include "imgui_impl_sdlrenderer.h"
#include "SDL.h"
#include "SDL_opengl.h"
#include <iostream>

/*
**  Globals
*/

extern bool req_pause;

/*
**  Functions
*/

void init_imgui(SDL_Window* window, SDL_Renderer* renderer);
void update_imgui(SDL_Renderer* renderer, int width, int height);
void destroy_imgui_data();
void imgui_event_handler(SDL_Event& event);
bool want_capture_mouse();
bool want_capture_keyboard();
void change_imgui_win_size();
void end_win_size_change();