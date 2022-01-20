#ifndef RENDERER
#define RENDERER

#include <SDL2/SDL.h>
#include "Window.hpp"
#include "VideoData.hpp"

class Renderer
{
private:
    Window *m_window;
    SDL_Renderer *m_renderer;

public:
    Renderer(Window *window);
    void clear_renderer();
    void render_copy(SDL_Texture *texture);
    void update_texture(SDL_Texture *texture,framedata_struct* state);
    void render_present();
    void destroy_renderer();
    SDL_Renderer* get_renderer();
};

#endif