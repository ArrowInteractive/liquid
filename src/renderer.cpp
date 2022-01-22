#include "renderer.hpp"

SDL_Renderer* renderer_create(SDL_Renderer* renderer, SDL_Window* window)
{
    renderer = SDL_CreateRenderer(
        window,
        -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC | SDL_RENDERER_TARGETTEXTURE
    );
    return renderer;
}

void renderer_clear(SDL_Renderer* renderer)
{
    SDL_RenderClear(renderer);
}

void renderer_copy(SDL_Renderer* renderer, SDL_Texture* texture)
{
    SDL_RenderCopy(renderer, texture, NULL, NULL);
}

void render_present(SDL_Renderer* renderer)
{
    SDL_RenderPresent(renderer);
}

void renderer_destroy(SDL_Renderer* renderer)
{
    SDL_DestroyRenderer(renderer);
    cout<<"Destroyed renderer."<<endl;
}