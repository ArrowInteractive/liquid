#include "inputs.hpp"

void handle_inputs(SDL_Event* event, eventstruct* eventstate)
{
    SDL_PollEvent(event);
    if(event->type == SDL_KEYDOWN)
    {
        // Handle keyboard keydowns
        if(event->key.keysym.sym == SDLK_q){
            eventstate->is_running = false;
        }
        else if(event->key.keysym.sym == SDLK_f){
            eventstate->is_fullscreen = !eventstate->is_fullscreen;
        }
    }
}