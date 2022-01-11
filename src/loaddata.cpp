#include <loaddata.hpp>

bool load_data( char* filename, framedata_struct* state)
{
    // Variables
    auto& av_format_ctx = state->av_format_ctx;
    auto& av_codec_params = state->av_codec_params;
    auto& av_codec = state->av_codec;
    auto& av_codec_ctx = state->av_codec_ctx;
    auto& av_packet = state->av_packet;
    auto& av_frame = state->av_frame;
    auto& decoded_frame = state->decoded_frame;
    auto& video_stream_index = state->video_stream_index;
    auto& audio_stream_index = state->audio_stream_index;
    auto& f_response = state->f_response;
    auto& p_response = state->p_response;
    auto& f_width = state->f_width;
    auto& f_height = state->f_height;
    auto& num_bytes = state->num_bytes;
    auto& buffer = state->buffer;
    auto& sws_ctx = state->sws_ctx;
    
    if(!(av_packet = av_packet_alloc()))
    {
        cout<<"Couldn't allocate AVPacket!"<<endl;
        return false;
    }

    if(!(av_frame = av_frame_alloc()))
    {
        cout<<"Couldn't allocate AVFrame!"<<endl;
        return false;
    }

    if(!(av_format_ctx = avformat_alloc_context()))
    {
        cout<<"Couldn't initialize AVFormatContext!"<<endl;
        return false;
    }

    if(avformat_open_input(&av_format_ctx, filename, NULL, NULL) != 0)
    {
        cout<<"Couldn't open file!"<<endl;
        return false;
    }

    for(int i=0; i < av_format_ctx->nb_streams; i++)
    {
        av_codec_params = av_format_ctx->streams[i]->codecpar;
        av_codec = avcodec_find_decoder(av_codec_params->codec_id);
        cout<<"Stream ID : "<<i<<endl;
        cout<<"CODEC     : "<<av_codec->AVCodec::long_name<<endl;

        if(av_codec_params->codec_type == AVMEDIA_TYPE_VIDEO)
        {
            video_stream_index = i;
        }

        if(av_codec_params->codec_type == AVMEDIA_TYPE_AUDIO)
        {
            // For now just find the first audio stream
            if(audio_stream_index == -1)
            {
                audio_stream_index = i;
            }
            else
            {
                continue;
            }
        }
    }

    if(video_stream_index == -1 && audio_stream_index == -1)
    {
        cout<<"Couldn't find any streams in the file!"<<endl;
        return false;
    }

    if(!(av_codec_ctx = avcodec_alloc_context3(av_codec)))
    {
        cout<<"Couldn't setup AVCodecContext!"<<endl;
        return false;
    }

    if(avcodec_parameters_to_context(av_codec_ctx, av_codec_params) < 0)
    {
        cout<<"Couldn't pass the parameters to AVCodecContext!"<<endl;
        return false;
    }

    if(avcodec_open2(av_codec_ctx, av_codec, NULL) < 0)
    {
        cout<<"Couldn't open codec!"<<endl;
        return false;
    }

    while(av_read_frame(av_format_ctx, av_packet) >= 0)
    {
        if(av_packet->stream_index != video_stream_index)
        {
            continue;
        }

        p_response = avcodec_send_packet(av_codec_ctx, av_packet);
        if(p_response < 0)
        {
            cout<<"Failed to decode packet!"<<endl;
            return false;
        }

        f_response = avcodec_receive_frame(av_codec_ctx, av_frame);
        if(f_response == AVERROR(EAGAIN) || f_response == AVERROR_EOF)
        {
            continue;
        }
        else if(f_response < 0)
        {
            cout<<"Failed to receive frame!"<<endl;
            return false;
        }

        // Receive the first frame and break
        f_width = av_frame->width;
        f_height = av_frame->height;
        
        break;
    }

    /* Setup SWSContext here and send the decoded frame to renderer using decoded_frame */
    sws_ctx = sws_getContext(   av_frame->width, av_frame->height, av_codec_ctx->pix_fmt, 
                                av_frame->width, av_frame->height, AV_PIX_FMT_YUV420P, 
                                SWS_BICUBIC, NULL, NULL, NULL);
    
    
    num_bytes = av_image_get_buffer_size(
                AV_PIX_FMT_YUV420P,
                av_frame->width,
                av_frame->height,
                32
            );
    buffer = (uint8_t *) av_malloc(num_bytes * sizeof(uint8_t));

    decoded_frame = av_frame_alloc();

    av_image_fill_arrays(
        decoded_frame->data,
        decoded_frame->linesize,
        buffer,
        AV_PIX_FMT_YUV420P,
        av_frame->width,
        av_frame->height,
        32
    );

    sws_scale(
                    sws_ctx,
                    (uint8_t const * const *)av_frame->data,
                    av_frame->linesize,
                    0,
                    av_codec_ctx->height,
                    decoded_frame->data,
                    decoded_frame->linesize
                );
    return true;
}

void close_data(framedata_struct* state)
{
    av_packet_free(&state->av_packet);
    av_frame_free(&state->av_frame);
    avformat_close_input(&state->av_format_ctx);
    avformat_free_context(state->av_format_ctx);
    avcodec_parameters_free(&state->av_codec_params);
    avcodec_free_context(&state->av_codec_ctx);
    av_free(&state->buffer);
    sws_freeContext(state->sws_ctx);
}