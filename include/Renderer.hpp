#ifndef RENDERER
#define RENDERER

#include <iostream>
#include <SDL2/SDL.h>
using namespace std;

SDL_Renderer* renderer_create(SDL_Renderer* renderer, SDL_Window* window);
void renderer_clear(SDL_Renderer* renderer);
void renderer_copy(SDL_Renderer* renderer, SDL_Texture* texture);
void render_present(SDL_Renderer* renderer);
void renderer_destroy(SDL_Renderer* renderer);

#endif