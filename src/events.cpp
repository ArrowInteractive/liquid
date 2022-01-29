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
                if(videostate->is_fullscreen){
                    // Make it windowed mode
                    std::cout<<"Changing into windowed mode."<<std::endl;

                    // Lock mutexes
                    SDL_LockMutex(videostate->window_mutex);

                    // Change Window size & Scaling
                    SDL_SetWindowFullscreen(videostate->window, 0);
                    change_scaling(videostate, videostate->video_ctx->width, videostate->video_ctx->height);

                    // Unlock mutexes
                    SDL_UnlockMutex(videostate->window_mutex);
                }
                else{
                    // Make it windowed mode
                    std::cout<<"Changing into fullscreen mode."<<std::endl;

                    // Lock mutexes
                    SDL_LockMutex(videostate->window_mutex);

                    // Change Window size & Scaling
                    // Fix tearing & Aspect ratio
                    SDL_SetWindowFullscreen(videostate->window, 1 | SDL_WINDOW_FULLSCREEN_DESKTOP);
                    change_scaling(videostate, videostate->video_ctx->width, videostate->video_ctx->height);

                    // Unlock mutexes
                    SDL_UnlockMutex(videostate->window_mutex);
                }
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