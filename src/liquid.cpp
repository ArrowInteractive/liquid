#define SDL_MAIN_HANDLED
#include "Window.hpp"
#include "Renderer.hpp"
#include "Input.hpp"
#include <SDL2/SDL.h>

int main()
{
    Window* window;
    Renderer* renderer;
    Input input;
    window = new Window("Liquid Video Player", 1280, 720);
    renderer = new Renderer(window);

    window->init_window();
    renderer->init_renderer();

    while (input.is_window_running())
    {
        input.update_events();
        renderer->clear_renderer();
        renderer->render();
        SDL_Delay(5);
    }
    window->close_window();
    renderer->close_renderer();
}