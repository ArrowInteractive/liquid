#include "liquid.hpp"

int main(int argc, char * argv[])
{
    if(argc < 2){
        return -1;
    }

    VideoState* videostate = NULL;
    videostate = (VideoState*)av_mallocz(sizeof(VideoState));
    videostate->flush_pkt = av_packet_alloc();
    videostate->filename = argv[1];

    // Initialize locks for the display buffer (pictq)
    videostate->pictq_mutex = SDL_CreateMutex();
    videostate->pictq_cond = SDL_CreateCond();

    // Launch threads by pushing an SDL_event of type FF_REFRESH_EVENT
    schedule_refresh(videostate, 100);
    videostate->av_sync_type = DEFAULT_AV_SYNC_TYPE;

    // Decode info about media
    decode_info(videostate);

    // Start the decoding thread to read data from the AVFormatContext
    videostate->decode_thd = SDL_CreateThread(decode_thread, "Decoding Thread", videostate);

    // Check the decode thread was correctly started
    if(!videostate->decode_thd)
    {
        std::cout<<"ERROR: Could not start decoding thread!"<<std::endl;

        // Free allocated memory before exiting
        av_free(videostate);

        return -1;
    }

    av_init_packet(videostate->flush_pkt);
    videostate->flush_pkt->data = 0;
    
    // Main loop
    while(videostate->quit == 0){
        handle_events(&videostate->event, videostate);
    }
    
    return 0;
}