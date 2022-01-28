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

int decode_info(VideoState* videostate){
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
    // Return with error in case no video stream was found
    if (videostate->video_stream_index == -1)
    {
        std::cout<<"ERROR: Could not find any video stream!"<<std::endl;
        return -1;
    }
    else
    {
        // Open video stream component codec
        ret = stream_component_open(videostate, videostate->video_stream_index);

        // Check video codec was opened correctly
        if (ret < 0)
        {
            std::cout<<"ERROR: Could not open video codec!"<<std::endl;
            return -1;
        }
    }

    // Return with error in case no audio stream was found
    if (videostate->audio_stream_index == -1)
    {
        std::cout<<"ERROR: Could not find any audio stream!"<<std::endl;
        return -1;
    }
    else
    {
        // open audio stream component codec
        ret = stream_component_open(videostate, videostate->audio_stream_index);

        // check audio codec was opened correctly
        if (ret < 0)
        {
            std::cout<<"ERROR: Could not open audio codec!"<<std::endl;
            return -1;
        }
    }

    // Check if both audio and video codecs were correctly retrieved
    if (videostate->video_stream_index < 0 || videostate->audio_stream_index < 0)
    {
        std::cout<<"ERROR: Could not open the file!"<<std::endl;
        return -1;
    }

    return 0;
}

int decode_thread(void * arg){
    // retrieve VideoState reference
    VideoState * videostate = (VideoState *)arg;
    int ret = 0;

    // Alloc the AVPacket used to read the media file
    AVPacket * packet = av_packet_alloc();
    if (packet == NULL)
    {
        std::cout<<"ERROR: Could not allocate AVPacket!"<<std::endl;
        return -1;
    }

    for (;;)
    {
        // Check quit flag
        if (videostate->quit)
        {
            break;
        }

        // Seek setup goes here

        // Check audio and video packets queues size
        if (videostate->audioq.size > MAX_AUDIOQ_SIZE || videostate->videoq.size > MAX_VIDEOQ_SIZE)
        {
            // Wait for audio and video queues to decrease size
            SDL_Delay(10);
            continue;
        }

        // Read data from the AVFormatContext by repeatedly calling av_read_frame()
        ret = av_read_frame(videostate->pformatctx, packet);
        if (ret < 0)
        {
            if (ret == AVERROR_EOF)
            {
                // Media EOF reached, quit
                videostate->quit = 1;
                break;
            }
            else if (videostate->pformatctx->pb->error == 0)
            {
                // No read error; wait for user input
                SDL_Delay(10);

                continue;
            }
            else
            {
                // Exit loop in case of error
                break;
            }
        }

        // Put the packet in the appropriate queue
        if (packet->stream_index == videostate->video_stream_index)
        {
            packet_queue_put(&videostate->videoq, packet);
        }
        else if (packet->stream_index == videostate->audio_stream_index)
        {
            packet_queue_put(&videostate->audioq, packet);
        }
        else
        {
            // Otherwise free the memory
            av_packet_unref(packet);
        }
    }

    // Wait for the rest of the program to end
    while (!videostate->quit)
    {
        SDL_Delay(100);
    }

    // Close the opened input AVFormatContext
    avformat_close_input(&videostate->pformatctx);

    // Fail removed
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
            // Window
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
    return 0;
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

void audio_callback(void * arg, Uint8* stream, int len){
    // Retrieve the VideoState
    VideoState * videostate = (VideoState *)arg;

    double pts;

    // While the length of the audio data buffer is > 0
    while (len > 0)
    {
        // Check quit flag
        if (videostate->quit)
        {
            return;
        }

        // Check how much audio is left to writes
        if (videostate->audio_buf_index >= videostate->audio_buf_size)
        {
            // we have already sent all avaialble data; get more
            int audio_size = audio_decode_frame(
                                videostate,
                                videostate->audio_buf,
                                sizeof(videostate->audio_buf),
                                &pts
                            );

            // if error
            if (audio_size < 0)
            {
                // output silence
                videostate->audio_buf_size = 1024;

                // clear memory
                memset(videostate->audio_buf, 0, videostate->audio_buf_size);

                std::cout<<"ERROR: Audio decode failed!"<<std::endl;
            }
            else
            {
                audio_size = synchronize_audio(videostate, (int16_t *)videostate->audio_buf, audio_size);

                // cast to usigned just to get rid of annoying warning messages
                videostate->audio_buf_size = (unsigned)audio_size;
            }

            videostate->audio_buf_index = 0;
        }

        int len1 = videostate->audio_buf_size - videostate->audio_buf_index;

        if (len1 > len)
        {
            len1 = len;
        }

        // Copy data from audio buffer to the SDL stream
        memcpy(stream, (uint8_t *)videostate->audio_buf + videostate->audio_buf_index, len1);

        len -= len1;
        stream += len1;

        // Update global VideoState audio buffer index
        videostate->audio_buf_index += len1;
    }
}

int audio_decode_frame(VideoState* videostate, uint8_t* audio_buf, int buf_size, double* pts_ptr){
    // Allocate AVPacket to read from the audio PacketQueue (audioq)
    AVPacket * packet = av_packet_alloc();
    if (packet == NULL)
    {
        std::cout<<"ERROR: Could not allocate packet!"<<std::endl;
        return -1;
    }

    static uint8_t * audio_pkt_data = NULL;
    static int audio_pkt_size = 0;

    double pts;
    int n;

    // Allocate a new frame, used to decode audio packets
    static AVFrame * d_frame = NULL;
    d_frame = av_frame_alloc();
    if (!d_frame)
    {
        std::cout<<"ERROR: Could not allocate frame!"<<std::endl;
        return -1;
    }

    int len1 = 0;
    int data_size = 0;

    // Infinite loop: read AVPackets from the audio PacketQueue, decode them into
    // Audio frames, resample the obtained frame and update the audio buffer
    for (;;)
    {
        // Check quit flag
        if (videostate->quit)
        {
            av_frame_free(&d_frame);
            return -1;
        }

        // Check if we obtained an AVPacket from the audio PacketQueue
        while (audio_pkt_size > 0)
        {
            int got_frame = 0;

            // Get decoded output data from decoder
            int ret = avcodec_receive_frame(videostate->audio_ctx, d_frame);

            // check and entire audio frame was decoded
            if (ret == 0)
            {
                got_frame = 1;
            }

            // Check the decoder needs more AVPackets to be sent
            if (ret == AVERROR(EAGAIN))
            {
                ret = 0;
            }

            if (ret == 0)
            {
                // Give the decoder raw compressed data in an AVPacket
                ret = avcodec_send_packet(videostate->audio_ctx, packet);
            }

            // Check the decoder needs more AVPackets to be sent
            if (ret == AVERROR(EAGAIN))
            {
                ret = 0;
            }
            else if (ret < 0)
            {
                std::cout<<"ERROR: Decoding failed!"<<std::endl;
                av_frame_free(&d_frame);
                return -1;
            }
            else
            {
                len1 = packet->size;
            }

            if (len1 < 0)
            {
                // If error, skip frame
                audio_pkt_size = 0;
                break;
            }

            audio_pkt_data += len1;
            audio_pkt_size -= len1;
            data_size = 0;

            // If we decoded an entire audio frame
            if (got_frame)
            {
                // Apply audio resampling to the decoded frame
                data_size = audio_resampling(
                    videostate,
                    d_frame,
                    AV_SAMPLE_FMT_S16,
                    audio_buf
                );

                assert(data_size <= buf_size);
            }

            if (data_size <= 0)
            {
                // No data yet, get more frames
                continue;
            }

            // Keep audio_clock up-to-date
            pts = videostate->audio_clock;
            *pts_ptr = pts;
            n = 2 * videostate->audio_ctx->channels;
            videostate->audio_clock += (double)data_size / (double)(n * videostate->audio_ctx->sample_rate);

            if (packet->data)
            {
                // Wipe the packet
                av_packet_unref(packet);
            }
            av_frame_free(&d_frame);

            // We have the data, return it and come back for more later
            return data_size;
        }

        if (packet->data)
        {
            // wipe the packet
            av_packet_unref(packet);
        }

        // Get more audio AVPacket
        int ret = packet_queue_get(videostate, &videostate->audioq, packet, 1);

        // If packet_queue_get returns < 0, the global quit flag was set
        if (ret < 0)
        {
            return -1;
        }

        if (packet->data == videostate->flush_pkt->data)
        {
            avcodec_flush_buffers(videostate->audio_ctx);

            continue;
        }

        audio_pkt_data = packet->data;
        audio_pkt_size = packet->size;

        // Keep audio_clock up-to-date
        if (packet->pts != AV_NOPTS_VALUE)
        {
            videostate->audio_clock = av_q2d(videostate->audio_stream->time_base)*packet->pts;
        }
    }

    av_frame_free(&d_frame);
    return 0;
}

int audio_resampling(VideoState * videostate, AVFrame * decoded_audio_frame, enum AVSampleFormat out_sample_fmt, uint8_t * out_buf){
    // get an instance of the AudioResamplingState struct
    AudioResamplingState * ar_state = get_audio_resampling(videostate->audio_ctx->channel_layout);

    if (!ar_state->swr_ctx)
    {
        std::cout<<"ERROR: swr_alloc failed!"<<std::endl;
        return -1;
    }

    // get input audio channels
    ar_state->in_channel_layout = (videostate->audio_ctx->channels ==
                                  av_get_channel_layout_nb_channels(videostate->audio_ctx->channel_layout)) ?
                                 videostate->audio_ctx->channel_layout :
                                 av_get_default_channel_layout(videostate->audio_ctx->channels);

    // check input audio channels correctly retrieved
    if (ar_state->in_channel_layout <= 0)
    {
        std::cout<<"ERROR: in_channel_layout"<<std::endl;
        return -1;
    }

    // set output audio channels based on the input audio channels
    if (videostate->audio_ctx->channels == 1)
    {
        ar_state->out_channel_layout = AV_CH_LAYOUT_MONO;
    }
    else if (videostate->audio_ctx->channels == 2)
    {
        ar_state->out_channel_layout = AV_CH_LAYOUT_STEREO;
    }
    else
    {
        ar_state->out_channel_layout = AV_CH_LAYOUT_SURROUND;
    }

    // retrieve number of audio samples (per channel)
    ar_state->in_nb_samples = decoded_audio_frame->nb_samples;
    if (ar_state->in_nb_samples <= 0)
    {
        std::cout<<"ERROR: in_nb_samples"<<std::endl;
        return -1;
    }

    // Set SwrContext parameters for resampling
    av_opt_set_int(
            ar_state->swr_ctx,
            "in_channel_layout",
            ar_state->in_channel_layout,
            0
    );

    // Set SwrContext parameters for resampling
    av_opt_set_int(
            ar_state->swr_ctx,
            "in_sample_rate",
            videostate->audio_ctx->sample_rate,
            0
    );

    // Set SwrContext parameters for resampling
    av_opt_set_sample_fmt(
            ar_state->swr_ctx,
            "in_sample_fmt",
            videostate->audio_ctx->sample_fmt,
            0
    );

    // Set SwrContext parameters for resampling
    av_opt_set_int(
            ar_state->swr_ctx,
            "out_channel_layout",
            ar_state->out_channel_layout,
            0
    );

    // Set SwrContext parameters for resampling
    av_opt_set_int(
            ar_state->swr_ctx,
            "out_sample_rate",
            videostate->audio_ctx->sample_rate,
            0
    );

    // Set SwrContext parameters for resampling
    av_opt_set_sample_fmt(
            ar_state->swr_ctx,
            "out_sample_fmt",
            out_sample_fmt,
            0
    );

    // Initialize SWR context after user parameters have been set
    int ret = swr_init(ar_state->swr_ctx);;
    if (ret < 0)
    {
        std::cout<<"ERROR: Could not initialize resampling context!"<<std::endl;
        return -1;
    }

    ar_state->max_out_nb_samples = ar_state->out_nb_samples = av_rescale_rnd(
            ar_state->in_nb_samples,
            videostate->audio_ctx->sample_rate,
            videostate->audio_ctx->sample_rate,
            AV_ROUND_UP
    );

    // Check rescaling was successful
    if (ar_state->max_out_nb_samples <= 0)
    {
        std::cout<<"ERROR: av_rescale_rnd failed!"<<std::endl;
        return -1;
    }

    // Get number of output audio channels
    ar_state->out_nb_channels = av_get_channel_layout_nb_channels(ar_state->out_channel_layout);

    // Allocate data pointers array for arState->resampled_data and fill data
    // pointers and linesize accordingly
    ret = av_samples_alloc_array_and_samples(
            &ar_state->resampled_data,
            &ar_state->out_linesize,
            ar_state->out_nb_channels,
            ar_state->out_nb_samples,
            out_sample_fmt,
            0
    );

    // Check memory allocation for the resampled data was successful
    if (ret < 0)
    {
        std::cout<<"ERROR: Could not allocate destination samples!"<<std::endl;
        return -1;
    }

    // Retrieve output samples number taking into account the progressive delay
    ar_state->out_nb_samples = av_rescale_rnd(
            swr_get_delay(ar_state->swr_ctx, videostate->audio_ctx->sample_rate) + ar_state->in_nb_samples,
            videostate->audio_ctx->sample_rate,
            videostate->audio_ctx->sample_rate,
            AV_ROUND_UP
    );

    // Check output samples number was correctly rescaled
    if (ar_state->out_nb_samples <= 0)
    {
        std::cout<<"ERROR: av_rescale_rnd failed!"<<std::endl;
        return -1;
    }

    if (ar_state->out_nb_samples > ar_state->max_out_nb_samples)
    {
        // Free memory block and set pointer to NULL
        av_free(ar_state->resampled_data[0]);

        // Allocate a samples buffer for out_nb_samples samples
        ret = av_samples_alloc(
                ar_state->resampled_data,
                &ar_state->out_linesize,
                ar_state->out_nb_channels,
                ar_state->out_nb_samples,
                out_sample_fmt,
                1
        );

        // Check samples buffer correctly allocated
        if (ret < 0)
        {
            std::cout<<"ERROR: av_alloc_frames failed!"<<std::endl;
            return -1;
        }

        ar_state->max_out_nb_samples = ar_state->out_nb_samples;
    }

    if (ar_state->swr_ctx)
    {
        // Do the actual audio data resampling
        ret = swr_convert(
                ar_state->swr_ctx,
                ar_state->resampled_data,
                ar_state->out_nb_samples,
                (const uint8_t **) decoded_audio_frame->data,
                decoded_audio_frame->nb_samples
        );

        // Check audio conversion was successful
        if (ret < 0)
        {
            std::cout<<"ERROR: sws_convert failed!"<<std::endl;
            return -1;
        }

        // Get the required buffer size for the given audio parameters
        ar_state->resampled_data_size = av_samples_get_buffer_size(
                &ar_state->out_linesize,
                ar_state->out_nb_channels,
                ret,
                out_sample_fmt,
                1
        );

        // Check audio buffer size
        if (ar_state->resampled_data_size < 0)
        {
            std::cout<<"ERROR: av_samples_get_buffersize failed!"<<std::endl;
            return -1;
        }
    }
    else
    {
        std::cout<<"ERROR: sws_ctx is null!"<<std::endl;
        return -1;
    }

    // Copy the resampled data to the output buffer
    memcpy(out_buf, ar_state->resampled_data[0], ar_state->resampled_data_size);

    /*
        Memory Cleanup.
    */
    if (ar_state->resampled_data)
    {
        // Free memory block and set pointer to NULL
        av_freep(&ar_state->resampled_data[0]);
    }

    av_freep(&ar_state->resampled_data);
    ar_state->resampled_data = NULL;

    if (ar_state->swr_ctx)
    {
        // Free the allocated SwrContext and set the pointer to NULL
        swr_free(&ar_state->swr_ctx);
    }

    return ar_state->resampled_data_size;
}

AudioResamplingState* get_audio_resampling(uint64_t channel_layout){
    AudioResamplingState * audio_resampling = (AudioResamplingState*)av_mallocz(sizeof(AudioResamplingState));

    audio_resampling->swr_ctx = swr_alloc();
    audio_resampling->in_channel_layout = channel_layout;
    audio_resampling->out_channel_layout = AV_CH_LAYOUT_STEREO;
    audio_resampling->out_nb_channels = 0;
    audio_resampling->out_linesize = 0;
    audio_resampling->in_nb_samples = 0;
    audio_resampling->out_nb_samples = 0;
    audio_resampling->max_out_nb_samples = 0;
    audio_resampling->resampled_data = NULL;
    audio_resampling->resampled_data_size = 0;

    return audio_resampling;
}

int synchronize_audio(VideoState* videostate, short * samples, int samples_size){
    int n;
    double ref_clock;

    n = 2 * videostate->audio_ctx->channels;

    // check if
    if (videostate->av_sync_type != AV_SYNC_AUDIO_MASTER)
    {
        double diff, avg_diff;
        int wanted_size, min_size, max_size;

        ref_clock = get_master_clock(videostate);
        diff = get_audio_clock(videostate) - ref_clock;

        if (diff < AV_NOSYNC_THRESHOLD)
        {
            // Accumulate the diffs
            videostate->audio_diff_cum = diff + videostate->audio_diff_avg_coef * videostate->audio_diff_cum;

            if (videostate->audio_diff_avg_count < AUDIO_DIFF_AVG_NB)
            {
                videostate->audio_diff_avg_count++;
            }
            else
            {
                avg_diff = videostate->audio_diff_cum * (1.0 - videostate->audio_diff_avg_coef);

                /*
                    So we're doing pretty well; we know approximately how off the audio
                    is from the video or whatever we're using for a clock. So let's now
                    calculate how many samples we need to add or lop off by putting this
                    code where the "Shrinking/expanding buffer code" section is:
                */
                if (fabs(avg_diff) >= videostate->audio_diff_threshold)
                {
                    wanted_size = samples_size + ((int)(diff * videostate->audio_ctx->sample_rate) * n);
                    min_size = samples_size * ((100 - SAMPLE_CORRECTION_PERCENT_MAX) / 100);
                    max_size = samples_size * ((100 + SAMPLE_CORRECTION_PERCENT_MAX) / 100);

                    if(wanted_size < min_size)
                    {
                        wanted_size = min_size;
                    }
                    else if (wanted_size > max_size)
                    {
                        wanted_size = max_size;
                    }

                    /*
                        Now we have to actually correct the audio. You may have noticed that our
                        synchronize_audio function returns a sample size, which will then tell us
                        how many bytes to send to the stream. So we just have to adjust the sample
                        size to the wanted_size. This works for making the sample size smaller.
                        But if we want to make it bigger, we can't just make the sample size larger
                        because there's no more data in the buffer! So we have to add it. But what
                        should we add? It would be foolish to try and extrapolate audio, so let's
                        just use the audio we already have by padding out the buffer with the
                        value of the last sample.
                    */
                    if(wanted_size < samples_size)
                    {
                        /* Remove samples */
                        samples_size = wanted_size;
                    }
                    else if(wanted_size > samples_size)
                    {
                        uint8_t *samples_end, *q;
                        int nb;

                        /* add samples by copying final sample*/
                        nb = (samples_size - wanted_size);
                        samples_end = (uint8_t *)samples + samples_size - n;
                        q = samples_end + n;

                        while(nb > 0)
                        {
                            memcpy(q, samples_end, n);
                            q += n;
                            nb -= n;
                        }

                        samples_size = wanted_size;
                    }
                }
            }
        }
        else
        {
            /* Difference is TOO big; reset diff stuff */
            videostate->audio_diff_avg_count = 0;
            videostate->audio_diff_cum = 0;
        }
    }

    return samples_size;
}

double get_master_clock(VideoState* videostate){
    if (videostate->av_sync_type == AV_SYNC_VIDEO_MASTER)
    {
        return get_video_clock(videostate);
    }
    else if (videostate->av_sync_type == AV_SYNC_AUDIO_MASTER)
    {
        return get_audio_clock(videostate);
    }
    else if (videostate->av_sync_type == AV_SYNC_EXTERNAL_MASTER)
    {
        return get_external_clock(videostate);
    }
    else
    {
        std::cout<<"ERROR: Undefined AVSyncType!"<<std::endl;
        return -1;
    }
}

double get_video_clock(VideoState* videostate){
    double delta = (av_gettime() - videostate->video_current_pts_time) / 1000000.0;

    return videostate->video_current_pts + delta;
}

double get_audio_clock(VideoState* videostate){
    double pts = videostate->audio_clock;

    int hw_buf_size = videostate->audio_buf_size - videostate->audio_buf_index;

    int bytes_per_sec = 0;

    int n = 2 * videostate->audio_ctx->channels;

    if (videostate->audio_stream)
    {
        bytes_per_sec = videostate->audio_ctx->sample_rate * n;
    }

    if (bytes_per_sec)
    {
        pts -= (double) hw_buf_size / bytes_per_sec;
    }

    return pts;
}

double get_external_clock(VideoState* videostate){
    videostate->external_clock_time = av_gettime();
    videostate->external_clock = videostate->external_clock_time / 1000000.0;

    return videostate->external_clock;
}

void video_refresh_timer(void * arg){
    // Refreshing state

    // Retrieve VideoState reference
    VideoState * videostate = (VideoState *)arg;

    // VideoPicture read index reference
    VideoPicture * videopicture;

    // Used for video frames display delay and audio video sync
    double pts_delay;
    double audio_ref_clock;
    double sync_threshold;
    double real_delay;
    double audio_video_delay;

    // Check if video_stream was opened properly
    if(videostate->video_stream){
        // Check if the VideoPicture queue contains decoded frames
        if (videostate->pictq_size == 0)
        {
            schedule_refresh(videostate, 1);
        }
        else
        {
            // Get VideoPicture reference using the queue read index
            videopicture = &videostate->pictq[videostate->pictq_rindex];

            // Get last frame pts
            pts_delay = videopicture->pts - videostate->frame_last_pts;

            // If the obtained delay is incorrect
            if (pts_delay <= 0 || pts_delay >= 1.0)
            {
                // Use the previously calculated delay
                pts_delay = videostate->frame_last_delay;
            }

            // Save delay information for the next time
            videostate->frame_last_delay = pts_delay;
            videostate->frame_last_pts = videopicture->pts;

            // In case the external clock is not used
            if(videostate->av_sync_type != AV_SYNC_VIDEO_MASTER)
            {
                // Update delay to stay in sync with the master clock: audio or video
                audio_ref_clock = get_master_clock(videostate);


                // Calculate audio video delay accordingly to the master clock
                audio_video_delay = videopicture->pts - audio_ref_clock;

                // Skip or repeat the frame taking into account the delay
                sync_threshold = (pts_delay > AV_SYNC_THRESHOLD) ? pts_delay : AV_SYNC_THRESHOLD;

                // Check audio video delay absolute value is below sync threshold
                if(fabs(audio_video_delay) < AV_NOSYNC_THRESHOLD)
                {
                    if(audio_video_delay <= -sync_threshold)
                    {
                        pts_delay = 0;
                    }
                    else if (audio_video_delay >= sync_threshold)
                    {
                        pts_delay = 2 * pts_delay;
                    }
                }
            }
            videostate->frame_timer += pts_delay;

            // Compute the real delay
            real_delay = videostate->frame_timer - (av_gettime() / 1000000.0);
            if (real_delay < 0.010)
            {
                real_delay = 0.010;
            }

            schedule_refresh(videostate, (Uint32)(real_delay * 1000 + 0.5));

            // Show the frame on the SDL_Surface (the screen)
            video_display(videostate);

            // Update read index for the next frame
            if(++videostate->pictq_rindex == VIDEO_PICTURE_QUEUE_SIZE)
            {
                videostate->pictq_rindex = 0;
            }

            // Lock VideoPicture queue mutex
            SDL_LockMutex(videostate->pictq_mutex);

            // Decrease VideoPicture queue size
            videostate->pictq_size--;

            // Notify other threads waiting for the VideoPicture queue
            SDL_CondSignal(videostate->pictq_cond);

            // Unlock VideoPicture queue mutex
            SDL_UnlockMutex(videostate->pictq_mutex);
        }
    }
    else
    {
        schedule_refresh(videostate, 100);
    }
}

void video_display(VideoState* videostate){
    // Reference for the next VideoPicture to be displayed
    VideoPicture * videoPicture;

    double aspect_ratio;

    int w, h, x, y;

    // Get next VideoPicture to be displayed from the VideoPicture queue
    videoPicture = &videostate->pictq[videostate->pictq_rindex];

    if (videoPicture->frame)
    {
        if (videostate->video_ctx->sample_aspect_ratio.num == 0)
        {
            aspect_ratio = 0;
        }
        else
        {
            aspect_ratio = av_q2d(videostate->video_ctx->sample_aspect_ratio) * videostate->video_ctx->width / videostate->video_ctx->height;
        }

        if (aspect_ratio <= 0.0)
        {
            aspect_ratio = (float)videostate->video_ctx->width /
                           (float)videostate->video_ctx->height;
        }

        // Get the size of a window's client area
        int screen_width;
        int screen_height;
        SDL_GetWindowSize(videostate->window, &screen_width, &screen_height);

        // Global SDL_Surface height
        h = screen_height;

        // Retrieve width using the calculated aspect ratio and the screen height
        w = ((int) rint(h * aspect_ratio)) & -3;

        // If the new width is bigger than the screen width
        if (w > screen_width)
        {
            // Set the width to the screen width
            w = screen_width;

            // Recalculate height using the calculated aspect ratio and the screen width
            h = ((int) rint(w / aspect_ratio)) & -3;
        }

        x = (screen_width - w);
        y = (screen_height - h);

        // Lock screen mutex
        SDL_LockMutex(videostate->window_mutex);

        // Update the texture with the new pixel data
        SDL_UpdateYUVTexture(
                videostate->texture,
                NULL,
                videoPicture->frame->data[0],
                videoPicture->frame->linesize[0],
                videoPicture->frame->data[1],
                videoPicture->frame->linesize[1],
                videoPicture->frame->data[2],
                videoPicture->frame->linesize[2]
        );

        // Clear the current rendering target with the drawing color
        SDL_RenderClear(videostate->renderer);

        // Copy the texture to the current rendering target
        SDL_RenderCopy(videostate->renderer, videostate->texture, NULL, NULL);

        // Update the screen with any rendering performed since the previous call
        SDL_RenderPresent(videostate->renderer);

        // Unlock screen mutex
        SDL_UnlockMutex(videostate->window_mutex);
    }
}

