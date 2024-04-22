/*
**  Includes
*/

#include <memory>

#include "Liquid.hpp"
#include "UI.hpp"
#include "Stream.hpp"
#include "Window.hpp"
#include "Event.hpp"


Liquid::Liquid(int argc, char *argv[])
{
    if(argc < 2){
        std::cout<<"ERROR: Please provide an input file."<<std::endl;
        exit(-1);
    }

    if (!std::filesystem::exists(argv[1])){
        std::cout<<"The input file is not valid!"<<std::endl;
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
        std::cout<<"ERROR: Could not initialize SDL!"<<std::endl;
        std::cout<<SDL_GetError()<<std::endl;
        exit(-1);
    }

    SDL_EventState(SDL_SYSWMEVENT, SDL_IGNORE);
    SDL_EventState(SDL_USEREVENT, SDL_IGNORE);

    #ifdef  SDL_HINT_VIDEO_X11_NET_WM_BYPASS_COMPOSITOR
            SDL_SetHint(SDL_HINT_VIDEO_X11_NET_WM_BYPASS_COMPOSITOR, "0");
    #endif

    if(Window::create_window() != 0){
        std::cout<<"ERROR: Could not setup a window or renderer!"<<std::endl;
        exit(-1);
    }  

    videostate = Stream::stream_open(input_filename);
    if(!videostate){
        std::cout<<"ERROR: Failed to initialize VideoState!"<<std::endl;
        exit(-1);
    }

    // FIXME : Window sizing workaround
    Event::toggle_full_screen(videostate);
    videostate->force_refresh = 1;

    Event::event_loop(videostate);
    return;
}

int main(int argc, char *argv[])
{
  std::unique_ptr<Liquid> liquid(new Liquid(argc, argv));
  liquid->run();
}
