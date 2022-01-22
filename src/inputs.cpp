#include "inputs.hpp"

void handle_inputs(SDL_Event* event, eventstruct* eventstate, SDL_Window* window, datastruct* datastate)
{
    SDL_PollEvent(event);
    if(event->type == SDL_KEYDOWN)
    {
        // Handle keyboard keydowns
        if(event->key.keysym.sym == SDLK_q){
            eventstate->is_running = false;
        }
        else if(event->key.keysym.sym == SDLK_f){
            if(!eventstate->is_fullscreen){
                window_resize(window, datastate->d_width, datastate->d_height + 10);
            }
            else{
                window_resize(window, datastate->av_codec_ctx->width, datastate->av_codec_ctx->height);
            }
            eventstate->is_fullscreen = !eventstate->is_fullscreen;
        }
    }
}