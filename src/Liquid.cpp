/*
**  Includes
*/

#include <memory>

#include "Liquid.hpp"
#include "Stream.hpp"
#include "Window.hpp"
#include "Event.hpp"
#include "utils/Log.hpp"


Liquid::Liquid(int argc, char *argv[])
{
    if(argc < 2){
        Log::error() << "Please provide an input file.";
        exit(-1);
    }

    if (!std::filesystem::exists(argv[1])){
        Log::error() << "The input file is not valid!";
        exit(-1);
    }
    input_filename = argv[1];
    flags = SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER;
}

void Liquid::run()
{
    if (!SDL_getenv("SDL_AUDIO_ALSA_SET_BUFFER_SIZE"))
    SDL_setenv("SDL_AUDIO_ALSA_SET_BUFFER_SIZE","1", 1);

    if (SDL_Init (flags)) {
        Log::error() << "Could not initialize SDL! " << SDL_GetError();
        exit(-1);
    }

    SDL_EventState(SDL_SYSWMEVENT, SDL_IGNORE);
    SDL_EventState(SDL_USEREVENT, SDL_IGNORE);

    #ifdef  SDL_HINT_VIDEO_X11_NET_WM_BYPASS_COMPOSITOR
            SDL_SetHint(SDL_HINT_VIDEO_X11_NET_WM_BYPASS_COMPOSITOR, "0");
    #endif

    if(Window::create_window() != 0){
        Log::error() << "Could not set up a window or OpenGL context!";
        exit(-1);
    }

    videostate = Stream::stream_open(input_filename);
    if(!videostate){
        Log::error() << "Failed to initialize VideoState!";
        exit(-1);
    }

    /* Start in fullscreen by setting the flag only: calling
       Event::toggle_full_screen() here would call SDL_SetWindowFullscreen()
       before the window has been sized/positioned by Video::video_open(),
       firing a premature SDL_WINDOWEVENT_SIZE_CHANGED that sets
       videostate->width early and permanently skips video_open() (which is
       gated on window_opened, set inside video_open() itself). Leaving the
       actual SDL_SetWindowFullscreen() call to video_open() keeps the
       ordering correct. */
    is_full_screen = 1;
    videostate->force_refresh = 1;

    Event::event_loop(videostate);
    return;
}

