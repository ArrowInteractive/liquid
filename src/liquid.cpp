#include "liquid.hpp"

int main(int argc, char * argv[])
{
    if(argc < 2){
        return -1;
    }

    int ret = SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER);
    if (ret != 0){
        std::cout<<"ERROR: Could not initialize SDL!"<<std::endl;
        return -1;
    }

    VideoState* videostate = NULL;
    videostate = (VideoState*)av_mallocz(sizeof(VideoState));

    videostate->flush_pkt = av_packet_alloc();

    videostate->filename = argv[1];

    // Initialize locks for the display buffer (pictq)
    videostate->pictq_mutex = SDL_CreateMutex();
    videostate->pictq_cond = SDL_CreateCond();

    // launch our threads by pushing an SDL_event of type FF_REFRESH_EVENT
    schedule_refresh(videostate, 100);
    videostate->av_sync_type = DEFAULT_AV_SYNC_TYPE;

    // Decode info about media
    decode_info(videostate);

    // start the decoding thread to read data from the AVFormatContext
    videostate->decode_thd = SDL_CreateThread(decode_thread, "Decoding Thread", videostate);

    // check the decode thread was correctly started
    if(!videostate->decode_thd)
    {
        std::cout<<"ERROR: Could not start decoding thread!"<<std::endl;

        // free allocated memory before exiting
        av_free(videostate);

        return -1;
    }

    av_init_packet(videostate->flush_pkt);
    videostate->flush_pkt->data = 0;

    SDL_Event event;
    while(true)
    {
        SDL_PollEvent(&event);
        if(event.type == SDL_KEYDOWN)
        {
            if(event.key.keysym.sym == SDLK_q)
            {
                videostate->quit = 1;
                SDL_CondSignal(videostate->audioq.cond);
                SDL_CondSignal(videostate->videoq.cond);
                SDL_Quit();
            }   
        }

        if(event.type == FF_REFRESH_EVENT)
        {
            video_refresh_timer(event.user.data1);
        }

        if (videostate->quit)
        {
            // exit for loop
            break;
        }
    }

    return 0;
}