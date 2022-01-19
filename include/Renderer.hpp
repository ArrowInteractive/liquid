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
    ~Renderer();
    void InitRenderer();
    void RenderImage();
    void clearRenderer();
    void render();
    void closeRenderer();

};

#endif