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
    void render_image();
    void clear_renderer();
    void render();
    void close_renderer();
    SDL_Renderer* get_renderer();
};

#endif