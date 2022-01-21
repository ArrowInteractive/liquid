#ifndef TEXTURE
#define TEXTURE

#include <iostream>
#include <SDL2/SDL.h>
using namespace std;

bool texture_create(SDL_Texture* texture);
bool texture_update(SDL_Texture* texture);
bool texture_destroy(SDL_Texture* texture);

#endif