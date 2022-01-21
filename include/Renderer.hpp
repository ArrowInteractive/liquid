#ifndef RENDERER
#define RENDERER

#include <iostream>
#include <SDL2/SDL.h>
using namespace std;

bool renderer_create(SDL_Renderer* renderer);
bool renderer_clear(SDL_Renderer* renderer);
bool renderer_copy(SDL_Renderer* renderer);
bool renderer_destroy(SDL_Renderer* renderer);

#endif