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

            /*
                Video thread
            */
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

int video_thread(void * arg){
    VideoState* videostate = (VideoState *)arg;

    AVPacket* packet = av_packet_alloc();
    if (packet == NULL)
    {
        std::cout<<"ERROR: Could not allocate packet!"<<std::endl;
        return -1;
    }

    // Set this when we are done decoding an entire frame
    int is_frame_finished = 0;

    static AVFrame* d_frame = NULL;
    d_frame = av_frame_alloc();
    if (!d_frame)
    {
        std::cout<<"ERROR: Could not allocate frame!"<<std::endl;
        return -1;
    }

    // Every decoded frame carries its PTS in the VideoPicture queue
    double pts;

    for (;;)
    {
        int ret = packet_queue_get(videostate, &videostate->videoq, packet, 1);
        if (ret < 0)
        {
            // Stop getting packets
            break;
        }

        if (packet->data == videostate->flush_pkt->data)
        {
            avcodec_flush_buffers(videostate->video_ctx);
            continue;
        }

        // Send the decoder raw compressed data in an AVPacket
        ret = avcodec_send_packet(videostate->video_ctx, packet);
        if (ret < 0)
        {
            std::cout<<"ERROR: Failed to send packet!"<<std::endl;
            return -1;
        }

        while (ret >= 0)
        {
            // Get data from the decoder
            ret = avcodec_receive_frame(videostate->video_ctx, d_frame);

            // Check if an entire frame was decoded
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
            {
                break;
            }
            else if(ret < 0)
            {
                std::cout<<"ERROR: Failed to decode frame!"<<std::endl;
                return -1;
            }
            else
            {
                is_frame_finished = 1;
            }

            // Attempt to guess the proper monotonic timestamp for decoded video frame
            pts = guess_correct_pts(videostate->video_ctx, d_frame->pts, d_frame->pkt_dts);

            // In case we receive an undefined timestamp value
            if (pts == AV_NOPTS_VALUE)
            {
                // Set the PTS to the default value of 0
                pts = 0;
            }

            pts *= av_q2d(videostate->video_stream->time_base);

            // Did we get an entire video frame?
            if(is_frame_finished)
            {
                pts = synchronize_video(videostate, d_frame, pts);

                if(queue_picture(videostate, d_frame, pts) < 0)
                {
                    break;
                }
            }
        }

        // Wipe the packet
        av_packet_unref(packet);
    }
    // Wipe the frame
    av_frame_free(&d_frame);
    av_free(d_frame);

    return 0;
}

int64_t guess_correct_pts(AVCodecContext* ctx, int64_t reordered_pts, int64_t dts){
    int64_t pts;

    if (dts != AV_NOPTS_VALUE)
    {
        ctx->pts_correction_num_faulty_dts += dts <= ctx->pts_correction_last_dts;
        ctx->pts_correction_last_dts = dts;
    }
    else if (reordered_pts != AV_NOPTS_VALUE)
    {
        ctx->pts_correction_last_dts = reordered_pts;
    }

    if (reordered_pts != AV_NOPTS_VALUE)
    {
        ctx->pts_correction_num_faulty_pts += reordered_pts <= ctx->pts_correction_last_pts;
        ctx->pts_correction_last_pts = reordered_pts;
    }
    else if (dts != AV_NOPTS_VALUE)
    {
        ctx->pts_correction_last_pts = dts;
    }

    if ((ctx->pts_correction_num_faulty_pts <= ctx->pts_correction_num_faulty_dts || dts == AV_NOPTS_VALUE) && reordered_pts != AV_NOPTS_VALUE)
    {
        pts = reordered_pts;
    }
    else
    {
        pts = dts;
    }

    return pts;
}

double synchronize_video(VideoState* videostate, AVFrame* d_frame, double pts){
    double frame_delay;

    if (pts != 0)
    {
        // If we have PTS, set the clock to it
        videostate->video_clock = pts;
    }
    else
    {
        // If the PTS is not provided, set it as clock
        pts = videostate->video_clock;
    }

    // Update the video clock
    frame_delay = av_q2d(videostate->video_ctx->time_base);

    // If we are repeating a frame, adjust clock accordingly
    frame_delay += d_frame->repeat_pict * (frame_delay * 0.5);

    // Increase video clock to match the delay required for repeaing frames
    videostate->video_clock += frame_delay;

    return pts;
}

int queue_picture(VideoState* videostate, AVFrame* d_frame, double pts){
    // Lock VideoState->pictq mutex
    SDL_LockMutex(videostate->pictq_mutex);

    // Wait until we have space for a new pic in VideoState->pictq
    while (videostate->pictq_size >= VIDEO_PICTURE_QUEUE_SIZE && !videostate->quit)
    {
        SDL_CondWait(videostate->pictq_cond, videostate->pictq_mutex);
    }

    // Unlock VideoState->pictq mutex
    SDL_UnlockMutex(videostate->pictq_mutex);

    // Check quit flag
    if (videostate->quit)
    {
        return -1;
    }

    // Retrieve video picture using the queue write index
    VideoPicture * video_picture;
    video_picture = &videostate->pictq[videostate->pictq_windex];

    // If the VideoPicture SDL_Overlay is not allocated or has a different width/height
    if (!video_picture->frame ||
        video_picture->width != videostate->video_ctx->width ||
        video_picture->height != videostate->video_ctx->height)
    {
        // Set SDL_Overlay not allocated
        video_picture->allocated = 0;

        // allocate a new SDL_Overlay for the VideoPicture struct
        alloc_picture(videostate);

        // Check quit flag
        if(videostate->quit)
        {
            return -1;
        }
    }

    // Check the new SDL_Overlay was correctly allocated
    if (video_picture->frame)
    {
        // Set pts value for the last decode frame in the VideoPicture queu (pctq)
        video_picture->pts = pts;

        // Set VideoPicture AVFrame info using the last decoded frame
        video_picture->frame->pict_type = d_frame->pict_type;
        video_picture->frame->pts = d_frame->pts;
        video_picture->frame->pkt_dts = d_frame->pkt_dts;
        video_picture->frame->key_frame = d_frame->key_frame;
        video_picture->frame->coded_picture_number = d_frame->coded_picture_number;
        video_picture->frame->display_picture_number = d_frame->display_picture_number;
        video_picture->frame->width = d_frame->width;
        video_picture->frame->height = d_frame->height;

        // Scale the image in pFrame->data and put the resulting scaled image in pict->data
        sws_scale(
            videostate->sws_ctx,
            (uint8_t const * const *)d_frame->data,
            d_frame->linesize,
            0,
            videostate->video_ctx->height,
            video_picture->frame->data,
            video_picture->frame->linesize
        );

        // Update VideoPicture queue write index
        ++videostate->pictq_windex;

        // If the write index has reached the VideoPicture queue size
        if(videostate->pictq_windex == VIDEO_PICTURE_QUEUE_SIZE)
        {
            // Set it to 0
            videostate->pictq_windex = 0;
        }

        // lock VideoPicture queue
        SDL_LockMutex(videostate->pictq_mutex);

        // Increase VideoPicture queue size
        videostate->pictq_size++;

        // Unlock VideoPicture queue
        SDL_UnlockMutex(videostate->pictq_mutex);
    }

    return 0;
}

void alloc_picture(void * arg){
    // Retrieve global VideoState reference.
    VideoState * videostate = (VideoState *)arg;

    // Retrieve the VideoPicture pointed by the queue write index
    VideoPicture * video_picture;
    video_picture = &videostate->pictq[videostate->pictq_windex];

    // Check if the SDL_Overlay is allocated
    if (video_picture->frame)
    {
        // we already have an AVFrame allocated, free memory
        av_frame_free(&video_picture->frame);
        av_free(video_picture->frame);
    }

    // lock global screen mutex
    SDL_LockMutex(videostate->window_mutex);

    // Get the size in bytes required to store an image with the given parameters
    int numbytes;
    numbytes = av_image_get_buffer_size(
        AV_PIX_FMT_YUV420P,
        videostate->video_ctx->width,
        videostate->video_ctx->height,
        32
    );

    // Allocate image data buffer
    uint8_t * buffer = NULL;
    buffer = (uint8_t *) av_malloc(numbytes * sizeof(uint8_t));

    // Alloc the AVFrame later used to contain the scaled frame
    video_picture->frame = av_frame_alloc();
    if (video_picture->frame == NULL)
    {
        std::cout<<"ERROR: Could not allocate frame!"<<std::endl;
        return;
    }

    // The fields of the given image are filled in by using the buffer which points to the image data buffer.
    av_image_fill_arrays(
            video_picture->frame->data,
            video_picture->frame->linesize,
            buffer,
            AV_PIX_FMT_YUV420P,
            videostate->video_ctx->width,
            videostate->video_ctx->height,
            32
    );

    // Unlock global screen mutex
    SDL_UnlockMutex(videostate->window_mutex);

    // Update VideoPicture struct fields
    video_picture->width = videostate->video_ctx->width;
    video_picture->height = videostate->video_ctx->height;
    video_picture->allocated = 1;
}




