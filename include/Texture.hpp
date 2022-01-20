#ifndef TEXTURE
#define TEXTURE

#include <SDL2/SDL.h>
#include "Renderer.hpp"

class Texture
{
private:
    SDL_Texture *m_texture;
    Renderer *m_renderer;
    int m_width, m_height;

public:
    Texture(Renderer *renderer);
    Texture(Renderer *renderer, int width, int height);
    void destroy_texture();
    SDL_Texture *get_texture();
};

#endif