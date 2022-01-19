#ifndef RENDERER
#define RENDERER

#include <SDL2/SDL.h>
#include "Window.hpp"

class Renderer
{
private:
    Window *m_window;
    SDL_Renderer *m_renderer;

public:
    Renderer(Window *window);
    void clear_renderer();
    void render_copy();
    void render_present();
    void destroy_renderer();
    SDL_Renderer* get_renderer();
};

#endif