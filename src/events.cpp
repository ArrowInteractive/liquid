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
                    SDL_SetWindowSize(videostate->window, videostate->video_ctx->width, videostate->video_ctx->height);
                    SDL_SetWindowPosition(videostate->window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
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
                    // Use fullscreen function instead, fix tearing and returning back to windowed mode
                    SDL_SetWindowSize(videostate->window, videostate->display_mode.w, videostate->display_mode.h + 10);
                    SDL_SetWindowPosition(videostate->window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
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