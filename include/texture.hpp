#ifndef TEXTURE
#define TEXTURE

#include <iostream>
#include <SDL2/SDL.h>
#include "load_data.hpp"
using namespace std;

SDL_Texture* texture_create(SDL_Renderer* renderer, SDL_Texture* texture, int width, int height);
void texture_update(SDL_Texture* texture, datastruct* datastate);
void texture_destroy(SDL_Texture* texture);

#endif