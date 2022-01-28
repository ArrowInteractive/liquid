#include "events.hpp"

void handle_events(SDL_Event* event, VideoState* videostate){
    SDL_PollEvent(event);

    // Keydowns
    if(event->type == SDL_KEYDOWN)
    {
        if(event->key.keysym.sym == SDLK_q){
            videostate->quit = 1;
            SDL_CondSignal(videostate->audioq.cond);
            SDL_CondSignal(videostate->videoq.cond);
            SDL_Quit();
        }
        else if(event->key.keysym.sym == SDLK_f){
            std::cout<<"Fullscreen: "<<videostate->is_fullscreen<<std::endl;
            videostate->is_fullscreen = !videostate->is_fullscreen;
        }
    }

    if(event->type == FF_REFRESH_EVENT)
    {
        video_refresh_timer(event->user.data1);
    }
}