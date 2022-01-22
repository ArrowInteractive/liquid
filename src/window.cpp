#include "window.hpp"

bool get_display_mode(SDL_DisplayMode* displaymode)
{
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER);

    if (SDL_GetDesktopDisplayMode(0, displaymode)){
        cout<<"Error getting display mode!"<<endl;
        return false;
    }
    return true;
}

SDL_Window* window_create(SDL_Window* window, const char* title, int width, int height)
{   
    window = SDL_CreateWindow(
        title,
        SDL_WINDOWPOS_UNDEFINED,
        SDL_WINDOWPOS_UNDEFINED,
        width,
        height,
        SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN
    );
    return window;
}

void window_destroy(SDL_Window* window)
{
    SDL_DestroyWindow(window);
    cout<<"Destroyed window."<<endl;
}