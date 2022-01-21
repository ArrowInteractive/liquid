#include "load_data.hpp"

bool load_data(char* filename, datastruct* state)
{
    if(!(state->av_format_ctx = avformat_alloc_context())){
        cout<<"ERROR: Could not setup AVFormatContext!"<<endl;
        return false;
    }

    if(avformat_open_input(&state->av_format_ctx, filename, NULL, NULL) != 0){
        cout<<"ERROR: Could not open file!"<<endl;
        return false;
    }

    for(int i=0; i < state->av_format_ctx->nb_streams; i++){
        if(state->av_format_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
        {
            state->av_codec_params = state->av_format_ctx->streams[i]->codecpar;
            state->av_codec = avcodec_find_decoder(state->av_codec_params->codec_id);
            state->video_stream_index = i;
            break;
        }
    }

    if(state->video_stream_index == -1 && state->audio_stream_index == -1){
        cout<<"ERROR: Could not find any streams in the file!"<<endl;
        return false;
    }

    if(!(state->av_codec_ctx = avcodec_alloc_context3(state->av_codec))){
        cout<<"ERROR: Could not setup AVCodecContext!"<<endl;
        return false;
    }

    if(avcodec_parameters_to_context(state->av_codec_ctx, state->av_codec_params) < 0){
        cout<<"ERROR: Could not pass the parameters to AVCodecContext!"<<endl;
        return false;
    }

    if(avcodec_open2(state->av_codec_ctx, state->av_codec, NULL) < 0){
        cout<<"ERROR: Could not open codec!"<<endl;
        return false;
    }

    if(!(state->av_packet = av_packet_alloc())){
        cout<<"ERROR: Couldn't allocate AVPacket!"<<endl;
        return false;
    }

    if(!(state->av_frame = av_frame_alloc())){
        cout<<"ERROR: Could not allocate AVFrame!"<<endl;
        return false;
    }

    if(!(state->decoded_frame = av_frame_alloc())){
        cout<<"ERROR: Could not allocate AVFrame!"<<endl;
        return false;
    }

    /* Work in progress : Check resolution for scaling */
    if(state->t_width <= state->av_codec_ctx->width && state->t_height <= state->av_codec_ctx->height)
    {
        state->t_width = state->av_codec_ctx->width;
        state->t_height = state->av_codec_ctx->height;
    }

    state->num_bytes = av_image_get_buffer_size(   
        AV_PIX_FMT_YUV420P,
        state->t_width,
        state->t_height,
        32
    );

    state->buffer = (uint8_t *)av_malloc(state->num_bytes * sizeof(uint8_t));

    av_image_fill_arrays(   
        state->decoded_frame->data,
        state->decoded_frame->linesize,
        state->buffer,
        AV_PIX_FMT_YUV420P,
        state->t_width,
        state->t_height,
        32
    );

    return true;
}

void load_frame(datastruct* state)
{
    while(av_read_frame(state->av_format_ctx, state->av_packet) >= 0)
    {
        if(state->av_packet->stream_index == state->video_stream_index)
        {
            state->response = avcodec_send_packet(state->av_codec_ctx, state->av_packet);
            if(state->response < 0){
                cout<<"Failed to decode packet!"<<endl;
                break;
            }

            if(state->sws_ctx == NULL)
            {
                /*
                    If sws_ctx is not set to NULL, it causes a seg fault on Linux based systems
                    when running sws_scale()
                    Setup sws_context here and send the decoded frame to renderer using decoded_frame
                    May cause crashes
                */
                state->sws_ctx = sws_getContext(   
                    state->av_codec_ctx->width, state->av_codec_ctx->height, state->av_codec_ctx->pix_fmt,
                    state->t_width, state->t_height, AV_PIX_FMT_YUV420P,
                    SWS_LANCZOS, NULL, NULL, NULL
                );
            }

            state->response = avcodec_receive_frame(state->av_codec_ctx, state->av_frame);
            if(state->response == AVERROR(EAGAIN))
            {
                av_packet_unref(state->av_packet);
                continue;
            }
            else if(state->response == AVERROR_EOF)
            {
                av_packet_unref(state->av_packet);
                break;
            }
            else if(state->response < 0)
            {
                cout<<"Failed to receive frame!"<<endl;
                av_packet_unref(state->av_packet);
                av_frame_unref(state->av_frame);
                break;
            }
            av_packet_unref(state->av_packet);
        }
        else
        {
            av_packet_unref(state->av_packet);
            av_frame_unref(state->av_frame);
            continue;
        }

        /* If End of File, then stop processing frames */
        if(state->response != AVERROR_EOF)
        {
            /* Scale the image and send it via decoded_frame */
            sws_scale(  
                state->sws_ctx,
                (uint8_t const * const *)state->av_frame->data,
                state->av_frame->linesize,
                0,
                state->av_frame->height,
                state->decoded_frame->data,
                state->decoded_frame->linesize
            );
        }
        av_frame_unref(state->av_frame);
        break;
    }
}

void scale_frame(datastruct* state)
{
    if(state->sws_ctx == NULL){
        /*
            If sws_ctx is not set to NULL, it causes a seg fault on Linux based systems
            when running sws_scale()
            Setup sws_context here and send the decoded frame to renderer using decoded_frame
            May cause crashes
        */
        state->sws_ctx = sws_getContext(
            state->av_codec_ctx->width, state->av_codec_ctx->height, state->av_codec_ctx->pix_fmt,
            state->t_width, state->t_height, AV_PIX_FMT_YUV420P,
            SWS_LANCZOS, NULL, NULL, NULL
        );
    }

    // Scale the frame
    sws_scale(  
        state->sws_ctx,
        (uint8_t const * const *)state->av_frame->data,
        state->av_frame->linesize,
        0,
        state->av_frame->height,
        state->decoded_frame->data,
        state->decoded_frame->linesize
    );
}

void close_data(datastruct* state)
{
    av_packet_free(&state->av_packet);
    av_frame_free(&state->av_frame);
    av_frame_free(&state->decoded_frame);
    avformat_close_input(&state->av_format_ctx);
    avcodec_free_context(&state->av_codec_ctx);
    avcodec_close(state->av_codec_ctx);
    av_freep(&state->buffer);
    sws_freeContext(state->sws_ctx);
    cout<<"Cleanup complete."<<endl;
}