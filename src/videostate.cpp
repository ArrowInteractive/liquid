#include "videostate.hpp"

/*
    Packet Queue
*/

void packet_queue_init(PacketQueue* queue){
    // alloc memory for the audio queue
    memset(queue, 0,sizeof(PacketQueue));

    // Returns the initialized and unlocked mutex or NULL on failure
    queue->mutex = SDL_CreateMutex();
    if (!queue->mutex)
    {
        // could not create mutex
        std::cout<<"ERROR: Could not create mutex!"<<std::endl;
        return;
    }

    // Returns a new condition variable or NULL on failure
    queue->cond = SDL_CreateCond();
    if (!queue->cond)
    {
        // could not create condition variable
        std::cout<<"ERROR: Could not create condition!"<<std::endl;
        return;
    }
}

int packet_queue_put(PacketQueue* queue, AVPacket* packet){
    // alloc the new AVPacketList to be inserted in the audio PacketQueue
    AVPacketList* av_packet_list;
    av_packet_list = (AVPacketList*)av_malloc(sizeof(AVPacketList));

    // check the AVPacketList was allocated
    if (!av_packet_list)
    {
        return -1;
    }

    // add reference to the given AVPacket
    av_packet_list->pkt = * packet;

    // the new AVPacketList will be inserted at the end of the queue
    av_packet_list->next = NULL;

    // lock mutex
    SDL_LockMutex(queue->mutex);

    // check the queue is empty
    if (!queue->last_pkt)
    {
        // if it is, insert as first
        queue->first_pkt = av_packet_list;
    }
    else
    {
        // if not, insert as last
        queue->last_pkt->next = av_packet_list;
    }

    // point the last AVPacketList in the queue to the newly created AVPacketList
    queue->last_pkt = av_packet_list;

    // increase by 1 the number of AVPackets in the queue
    queue->nb_packets++;

    // increase queue size by adding the size of the newly inserted AVPacket
    queue->size += av_packet_list->pkt.size;

    // notify packet_queue_get which is waiting that a new packet is available
    SDL_CondSignal(queue->cond);

    // unlock mutex
    SDL_UnlockMutex(queue->mutex);

    return 0;
}

int packet_queue_get(VideoState* videostate, PacketQueue* queue, AVPacket* packet, int blocking){
    int ret;

    AVPacketList* av_packet_list;

    // lock mutex
    SDL_LockMutex(queue->mutex);

    for (;;)
    {
        // check quit flag
        if (videostate->quit)
        {
            ret = -1;
            break;
        }

        // point to the first AVPacketList in the queue
        av_packet_list = queue->first_pkt;

        // if the first packet is not NULL, the queue is not empty
        if (av_packet_list)
        {
            // place the second packet in the queue at first position
            queue->first_pkt = av_packet_list->next;

            // check if queue is empty after removal
            if (!queue->first_pkt)
            {
                // first_pkt = last_pkt = NULL = empty queue
                queue->last_pkt = NULL;
            }

            // decrease the number of packets in the queue
            queue->nb_packets--;

            // decrease the size of the packets in the queue
            queue->size -= av_packet_list->pkt.size;

            // point packet to the extracted packet, this will return to the calling function
            *packet = av_packet_list->pkt;

            // free memory
            av_free(av_packet_list);

            ret = 1;
            break;
        }
        else if (!blocking)
        {
            ret = 0;
            break;
        }
        else
        {
            // unlock mutex and wait for cond signal, then lock mutex again
            SDL_CondWait(queue->cond, queue->mutex);
        }
    }

    // unlock mutex
    SDL_UnlockMutex(queue->mutex);

    return ret;
}

void packet_queue_flush(PacketQueue* queue){
    AVPacketList *pkt, *pkt_new;

    SDL_LockMutex(queue->mutex);

    for (pkt = queue->first_pkt; pkt != NULL; pkt = pkt_new)
    {
        pkt_new = pkt->next;
        av_free_packet(&pkt->pkt);
        av_freep(&pkt);
    }

    queue->last_pkt = NULL;
    queue->first_pkt = NULL;
    queue->nb_packets = 0;
    queue->size = 0;

    SDL_UnlockMutex(queue->mutex);
}

/*
    Schedule 
*/

void schedule_refresh(VideoState* videostate, Uint32 delay){
    // schedule an SDL timer
    int ret = SDL_AddTimer(delay, sdl_refresh_timer_cb, videostate);

    // check the timer was correctly scheduled
    if (ret == 0)
    {
        std::cout<<"ERROR: Could not schedule refresh callback!"<<std::endl;
    }
}

Uint32 sdl_refresh_timer_cb(Uint32 interval, void * param){
    // create an SDL_Event of type FF_REFRESH_EVENT
    SDL_Event event;
    event.type = FF_REFRESH_EVENT;
    event.user.data1 = param;

    // push the event to the events queue
    SDL_PushEvent(&event);

    // return 0 to cancel the timer
    return 0;
}

/*
    Decode
*/

int decode_thread(void * arg){
    // retrieve VideoState reference
    VideoState * videostate = (VideoState *)arg;

    videostate->pformatctx = NULL;

    int ret = avformat_open_input(&videostate->pformatctx, videostate->filename, NULL, NULL);
    if (ret < 0)
    {
        std::cout<<"ERROR: Could not open file!"<<std::endl;
        return -1;
    }

    videostate->audio_stream_index = -1;
    videostate->video_stream_index = -1;

    ret = avformat_find_stream_info(videostate->pformatctx, NULL);
    if (ret < 0)
    {
        std::cout<<"ERROR: Could not find any stream information!"<<std::endl;
        return -1;
    }

    // Loop through available streams to find audio and video stream indexes
    for (int i = 0; i < videostate->pformatctx->nb_streams; i++)
    {
        // look for the video stream
        if (videostate->pformatctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO && videostate->video_stream_index == -1)
        {
            videostate->video_stream_index = i;
        }

        // look for the audio stream
        if (videostate->pformatctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO && videostate->audio_stream_index == -1)
        {
            videostate->audio_stream_index = i;
        }
    }

    /*
        NOTE: Modify the code to read from muted video files
    */
    if(videostate->video_stream_index == -1){
        std::cout<<"ERROR: Could not find any video streams!"<<std::endl;
        return -1;
    }
    
    if(videostate->audio_stream_index == -1){
        std::cout<<"ERROR: Could not find any audio streams!"<<std::endl;
        return -1;
    }

    // Open stream components here

    return 0;
}

int stream_component_open(VideoState* videostate, int stream_index){
    if(stream_index < 0 || stream_index >= videostate->pformatctx->nb_streams){
        std::cout<<"ERROR: Invalid stream index!"<<std::endl;
        return -1;
    }

    // Retreive codec for the stream index provided
    AVCodec* codec = NULL;
    codec = avcodec_find_decoder(videostate->pformatctx->streams[stream_index]->codecpar->codec_id);
    if(codec == NULL){
        std::cout<<"ERROR: Could not find a decoder for the media!"<<std::endl;
        return -1;
    }

    // Retreive codec context
    AVCodecContext* codec_ctx = NULL;
    codec_ctx = avcodec_alloc_context3(codec);
    int ret = avcodec_parameters_to_context(codec_ctx, videostate->pformatctx->streams[stream_index]->codecpar);
    if(ret != 0){
        std::cout<<"ERROR: Could not copy parameters to context!"<<std::endl;
        return -1;
    }

    if(avcodec_open2(codec_ctx, codec, NULL) < 0){
        std::cout<<"ERROR: Unsupported codec!"<<std::endl;
        return -1;
    }

    // Setup videostate based on the type of stream being processed
    switch (codec_ctx->codec_type){
        case AVMEDIA_TYPE_AUDIO:
        {
            // desired and obtained audio specs references
            SDL_AudioSpec wanted_specs;
            SDL_AudioSpec specs;

            // Set audio settings from codec info
            wanted_specs.freq = codec_ctx->sample_rate;
            wanted_specs.format = AUDIO_S16SYS;
            wanted_specs.channels = codec_ctx->channels;
            wanted_specs.silence = 0;
            wanted_specs.samples = SDL_AUDIO_BUFFER_SIZE;
            wanted_specs.callback = audio_callback;
            wanted_specs.userdata = videostate;

            /*
                NOTE: Deprecated, SDL_OpenAudioDevice() should be used insted
            */
            ret = SDL_OpenAudio(&wanted_specs, &specs);

            // check audio device was correctly opened
            if (ret < 0)
            {
                std::cout<<"ERROR: Could not open audio device!"<<std::endl;
                return -1;
            }

            videostate->audio_stream_index = stream_index;
            videostate->audio_stream = videostate->pformatctx->streams[stream_index];
            videostate->audio_ctx = codec_ctx;
            videostate->audio_buf_size = 0;
            videostate->audio_buf_index = 0;

            // zero out the block of memory pointed by videoState->audio_pkt
            memset(&videostate->audio_pkt, 0, sizeof(videostate->audio_pkt));

            // init audio packet queue
            packet_queue_init(&videostate->audioq);

            // start playing audio on the first audio device
            SDL_PauseAudio(0);
        }
        break;

        case AVMEDIA_TYPE_VIDEO:
        {
            // Set VideoState video related fields
            videostate->video_stream_index = stream_index;
            videostate->video_stream = videostate->pformatctx->streams[stream_index];
            videostate->video_ctx = codec_ctx;

            // Initialize the frame timer and the initial
            // previous frame delay: 1ms = 1e-6s
            videostate->frame_timer = (double)av_gettime() / 1000000.0;
            videostate->frame_last_delay = 40e-3;
            videostate->video_current_pts_time = av_gettime();

            // Snit video packet queue
            packet_queue_init(&videostate->videoq);

            // Start video decoding thread
            videostate->video_thd = SDL_CreateThread(video_thread, "Video Thread", videostate);

            // set up the VideoState SWSContext to convert the image data to YUV420
            videostate->sws_ctx = sws_getContext(
                videostate->video_ctx->width,
                videostate->video_ctx->height,
                videostate->video_ctx->pix_fmt,
                videostate->video_ctx->width,
                videostate->video_ctx->height,
                AV_PIX_FMT_YUV420P,
                SWS_LANCZOS,
                NULL,
                NULL,
                NULL
            );

            /*
                NOTE: Seperate into functions
            */
            videostate->window = SDL_CreateWindow(
                "Liquid Media Player",
                SDL_WINDOWPOS_UNDEFINED,
                SDL_WINDOWPOS_UNDEFINED,
                codec_ctx->width,
                codec_ctx->height,
                SDL_WINDOW_OPENGL | SDL_WINDOW_ALLOW_HIGHDPI
            );

            // check window was correctly created
            if (!videostate->window)
            {
                std::cout<<"ERROR: Could not create window!"<<std::endl;
                return -1;
            }

            //
            SDL_GL_SetSwapInterval(1);

            // initialize global SDL_Surface mutex reference
            videostate->window_mutex = SDL_CreateMutex();

            // create a 2D rendering context for the SDL_Window
            videostate->renderer = SDL_CreateRenderer(
                videostate->window, -1, 
                SDL_RENDERER_ACCELERATED |
                SDL_RENDERER_PRESENTVSYNC | 
                SDL_RENDERER_TARGETTEXTURE
            );

            // create a texture for a rendering context
            videostate->texture = SDL_CreateTexture(
                videostate->renderer,
                SDL_PIXELFORMAT_YV12,
                SDL_TEXTUREACCESS_STREAMING,
                videostate->video_ctx->width,
                videostate->video_ctx->height
            );
        }
        break;

        default:
        {
            // nothing to do
        }
        break;
    }
}


