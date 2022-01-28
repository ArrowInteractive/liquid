#include "events.hpp"

void handle_events(SDL_Event* event, VideoState* videostate){
    SDL_PollEvent(event);

    // SDL_Quit
    if(event->type == SDL_QUIT)
    {
        videostate->quit = 1;
        SDL_CondSignal(videostate->audioq.cond);
        SDL_CondSignal(videostate->videoq.cond);
        SDL_Quit();
    }

    // Keydowns
    if(event->type == SDL_KEYDOWN)
    {
        switch(event->key.keysym.sym)
        {
            case SDLK_q:
            {
                videostate->quit = 1;
                SDL_CondSignal(videostate->audioq.cond);
                SDL_CondSignal(videostate->videoq.cond);
                SDL_Quit();
            }
            break;

            case SDLK_f:
            {
                std::cout<<"is_fullscreen status: "<<videostate->is_fullscreen<<std::endl;
                videostate->is_fullscreen = !videostate->is_fullscreen;
            }
            break;

            default:
            {
                // Do nothing
            }
            break;
        }
    }

    if(event->type == FF_REFRESH_EVENT)
    {
        video_refresh_timer(event->user.data1);
    }
}