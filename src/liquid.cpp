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
    bool is_file_open = false;

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
                                    SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN
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

        is_file_open = true;
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

        renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC | SDL_RENDERER_TARGETTEXTURE);
        texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_YV12, SDL_TEXTUREACCESS_STREAMING, state.f_width, state.f_height);
    }

    SDL_SetWindowBordered(window, SDL_FALSE);
    
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
        
        if(is_file_open)
        {
            /* Load the texture from decoded_frame */
            SDL_UpdateYUVTexture(
                        texture,            // the texture to update
                        NULL,              // a pointer to the rectangle of pixels to update, or NULL to update the entire texture
                        state.decoded_frame->data[0],      // the raw pixel data for the Y plane
                        state.decoded_frame->linesize[0],  // the number of bytes between rows of pixel data for the Y plane
                        state.decoded_frame->data[1],      // the raw pixel data for the U plane
                        state.decoded_frame->linesize[1],  // the number of bytes between rows of pixel data for the U plane
                        state.decoded_frame->data[2],      // the raw pixel data for the V plane
                        state.decoded_frame->linesize[2]   // the number of bytes between rows of pixel data for the V plane
                    );
        }
        SDL_RenderClear(renderer);
        SDL_RenderCopy(renderer, texture, NULL, NULL);
        SDL_RenderPresent(renderer);
        SDL_Delay(5);
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    if(is_file_open)
    {
        close_data(&state);
    }
    
    return 0;
}