#ifndef WINDOW
#define WINDOW

#include <iostream>
#include <SDL2/SDL.h>
using namespace std;

bool get_display_mode(SDL_DisplayMode* displaymode);
SDL_Window* window_create(SDL_Window* window, const char* title, int width, int height);
void window_resize(SDL_Window* window, int width, int height);
void window_destroy(SDL_Window* window);

#endif