/*
**  Includes
*/

#include "liquid.hpp"
#include <imgui.h>
#include <imgui_impl_sdlrenderer.h>

/*
**  Functions
*/

int main(int argc, char *argv[])
{
    if(argc < 2){
        std::cout<<"ERROR: Please provide an input file."<<std::endl;
        return -1;
    }

    int flags;
    VideoState *videostate;
    input_filename = argv[1];

    flags = SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER;

    if (display_disable) {
        video_disable = 1;
        flags &= ~SDL_INIT_VIDEO;
    }
    if (audio_disable)
        flags &= ~SDL_INIT_AUDIO;
    else {
        /* Try to work around an occasional ALSA buffer underflow issue when the
         * period size is NPOT due to ALSA resampling by forcing the buffer size. */
        if (!SDL_getenv("SDL_AUDIO_ALSA_SET_BUFFER_SIZE"))
            SDL_setenv("SDL_AUDIO_ALSA_SET_BUFFER_SIZE","1", 1);
    }

    if (SDL_Init (flags)) {
        std::cout<<"ERROR: Could not initialize SDL!"<<std::endl;
        return -1;
    }

    SDL_EventState(SDL_SYSWMEVENT, SDL_IGNORE);
    SDL_EventState(SDL_USEREVENT, SDL_IGNORE);

#ifdef  SDL_HINT_VIDEO_X11_NET_WM_BYPASS_COMPOSITOR
        SDL_SetHint(SDL_HINT_VIDEO_X11_NET_WM_BYPASS_COMPOSITOR, "0");
#endif

    if(create_window() != 0){
        std::cout<<"ERROR: Could not setup a window or renderer!"<<std::endl;
        return -1;
    }    
    videostate = stream_open(input_filename);
    if(!videostate){
        std::cout<<"ERROR: Failed to initialize VideoState!"<<std::endl;
        return -1;
    }

    event_loop(videostate);

    return 0;
}