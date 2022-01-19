#include <SDL2/SDL.h>
#include "Window.hpp"
#include "Renderer.hpp"

int main()
{
    Window* window;
    Renderer* renderer;
    SDL_Event event;
    window = new Window("Liquid Video Player", 1280, 720);
    renderer = new Renderer(window);

    window->InitWindow();
    renderer->InitRenderer();

    while (true)
    {
        SDL_PollEvent(&event);
        if (event.type == SDL_QUIT)
        {
            break;
        }
        if (event.type == SDL_KEYDOWN)
        {
            if(event.key.keysym.sym == SDLK_q)
            {
                break;
            }
        }

        renderer->clearRenderer();
        renderer->render();
        SDL_Delay(5);
    }
}