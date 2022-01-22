#ifndef INPUTS
#define INPUTS

#include <SDL2/SDL.h>
#include "load_data.hpp"
#include "window.hpp"

struct eventstruct
{
    bool is_running = true;
    bool is_fullscreen = false;
};

void handle_inputs(SDL_Event* event, eventstruct* eventstate, SDL_Window* window, datastruct* datastate);

#endif