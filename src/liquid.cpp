#include <SDL2/SDL.h>
#include "Window.hpp"
#include "Renderer.hpp"
#include "Input.hpp"
#include "Texture.hpp"

int main()
{
    Window *window;
    Renderer *renderer;
    Texture* texture;
    Input input;
    window = new Window("Liquid Video Player", 1280, 720);
    renderer = new Renderer(window);
    texture = new Texture(renderer,1280,720);

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