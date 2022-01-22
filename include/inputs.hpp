#ifndef INPUTS
#define INPUTS

#include <SDL2/SDL.h>

struct eventstruct
{
    bool is_running = true;
    bool is_fullscreen = false;
};

void handle_inputs(SDL_Event* event, eventstruct* eventstate);

#endif