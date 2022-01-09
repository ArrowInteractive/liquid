#define SDL_MAIN_HANDLED

#include <iostream>
#include <SDL2/SDL.h>
#include <filesystem>

#include "loaddata.hpp"
using namespace std;
using std::filesystem::exists;
using std::filesystem::is_directory;
using std::filesystem::is_regular_file;

int main(int argc, char** argv)
{
    // Variables
    framedata_struct state;
    SDL_DisplayMode dm;
    SDL_Window* window;
    SDL_Event event;
    SDL_Renderer* renderer;
    SDL_Texture* texture;
    bool is_fullscreen = false;

    SDL_Init(SDL_INIT_VIDEO);

    if (SDL_GetDesktopDisplayMode(0, &dm))
    {
        cout<<"Error getting display mode!"<<endl;
        return -1;
    }

    if(argc < 2)
    {
        window = SDL_CreateWindow(
                                    "Liquid Media Player",
                                    SDL_WINDOWPOS_UNDEFINED,
                                    SDL_WINDOWPOS_UNDEFINED,
                                    1280,
                                    720,
                                    SDL_WINDOW_OPENGL
                                );

        if(window == NULL)
        {
            cout<<"Couldn't create window!"<<endl;
            return -1;
        }

        renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    }
    else
    {
        if(!exists(argv[1]))
        {
            cout<<"File doesn't exist."<<endl;
            return -1;
        }

        if(!is_regular_file(argv[1]))
        {
            // Write some code to load files from that folder
            // For now just display some error.
            cout<<"Provided input is a folder."<<endl;
            return -1;
        }

        if(!(load_data(argv[1], &state)))
        {
            return -1;
        }

        cout<<"Frame width  : "<<state.f_width<<endl;
        cout<<"Frame height : "<<state.f_height<<endl;
        window = SDL_CreateWindow(
                                    argv[1],
                                    SDL_WINDOWPOS_UNDEFINED,
                                    SDL_WINDOWPOS_UNDEFINED,
                                    state.f_width,
                                    state.f_height,
                                    SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN
                                );

        if(window == NULL)
        {
            cout<<"Couldn't create window!"<<endl;
            return -1;
        }

        renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    }

    while(true)
    {
        SDL_PollEvent(&event);

        if(event.type == SDL_QUIT)
        {
            break;
        }
        else
        {
            if(event.type == SDL_KEYDOWN)
            {
                if(event.key.keysym.sym == SDLK_q)
                {
                    break;
                }
                else if(event.key.keysym.sym == SDLK_f)
                {
                    if(!is_fullscreen)
                    {
                        SDL_SetWindowSize(window, dm.w, dm.h + 10);
                        SDL_SetWindowPosition(window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
                    }
                    else
                    {
                        if(state.f_width == 0 && state.f_height == 0)
                        {
                            SDL_SetWindowSize(window, 1280, 720);
                            SDL_SetWindowPosition(window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
                        }
                        else
                        {
                            SDL_SetWindowSize(window, state.f_width, state.f_height);
                            SDL_SetWindowPosition(window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
                        }
                    }
                    is_fullscreen = !is_fullscreen;
                }
            }
        }

        SDL_RenderClear(renderer);
        SDL_RenderPresent(renderer);
        SDL_Delay(5);
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    close_data(&state);
    return 0;
}