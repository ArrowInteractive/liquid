#pragma once

#include <SDL2/SDL.h>

void init_imgui(SDL_Window* window, SDL_Renderer* renderer);

void update_imgui(SDL_Renderer* renderer);

void destroy_imgui_data();

void imgui_event_handler(SDL_Event& event);

bool want_capture_mouse();

bool want_capture_keyboard();