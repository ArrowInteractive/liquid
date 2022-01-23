#ifndef INPUTS
#define INPUTS

#include <SDL2/SDL.h>
#include "datastruct.hpp"
#include "window.hpp"

struct eventstruct
{
    bool is_running = true;
    bool is_fullscreen = false;
    bool change_scaling = false;
};

void handle_inputs(SDL_Event* event, eventstruct* eventstate, SDL_Window* window, datastruct* datastate);

#endif