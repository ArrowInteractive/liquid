#include "VideoData.hpp"

bool VideoData::load_data(char* filename, framedata_struct* state)
{
    auto& av_format_ctx = state->av_format_ctx;
    auto& av_codec_params = state->av_codec_params;
    auto& av_codec = state->av_codec;
    auto& av_codec_ctx = state->av_codec_ctx;
    auto& video_stream_index = state->video_stream_index;
    auto& audio_stream_index = state->audio_stream_index;

    auto& buffer = state->buffer;
    auto& num_bytes = state->num_bytes;
    auto& av_packet = state->av_packet;
    auto& av_frame = state->av_frame;
    auto& decoded_frame = state->decoded_frame;
    auto& t_width = state->t_width;
    auto& t_height = state->t_height;


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
        if(av_format_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
        {
            av_codec_params = av_format_ctx->streams[i]->codecpar;
            av_codec = avcodec_find_decoder(av_codec_params->codec_id);
            video_stream_index = i;
            break;
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

    if(!(decoded_frame = av_frame_alloc()))
    {
        cout<<"Couldn't allocate AVFrame!"<<endl;
        return false;
    }

    /* Check resolution - Experimental */
    if(t_width <= av_codec_ctx->width && t_height <= av_codec_ctx->height)
    {
        t_width = av_codec_ctx->width;
        t_height = av_codec_ctx->height;
    }

    num_bytes = av_image_get_buffer_size(   AV_PIX_FMT_YUV420P,
                                            t_width,
                                            t_height,
                                            32
                                        );
    buffer = (uint8_t *)av_malloc(num_bytes * sizeof(uint8_t));

    av_image_fill_arrays(   decoded_frame->data,
                            decoded_frame->linesize,
                            buffer,
                            AV_PIX_FMT_YUV420P,
                            t_width,
                            t_height,
                            32
                        );

    return true;
}

bool VideoData::load_frames(framedata_struct* state)
{
    /* Unpacking vars */
    auto& av_format_ctx = state->av_format_ctx;
    auto& av_codec_ctx = state->av_codec_ctx;
    auto& av_packet = state->av_packet;
    auto& av_frame = state->av_frame;
    auto& decoded_frame = state->decoded_frame;
    auto& video_stream_index = state->video_stream_index;
    auto& audio_stream_index = state->audio_stream_index;
    auto& t_width = state->t_width;
    auto& t_height = state->t_height;
    auto& sws_ctx = state->sws_ctx;

    auto& buffer = state->buffer;
    auto& num_bytes = state->num_bytes;
    auto& p_response = state->p_response;
    auto& f_response = state->f_response;

    while(av_read_frame(av_format_ctx, av_packet) >= 0)
    {
        /* An exception gets raised randomly here */
        if(av_packet->stream_index == video_stream_index)
        {
            p_response = avcodec_send_packet(av_codec_ctx, av_packet);
            if(p_response < 0)
            {
                cout<<"Failed to decode packet!"<<endl;
                return false;
            }

            if(sws_ctx == NULL)
            {
                /*
                    If sws_ctx is not set to NULL, it causes a seg fault on Linux based systems
                    when running sws_scale()

                    Setup sws_context here and send the decoded frame to renderer using decoded_frame
                    May cause crashes
                */
                sws_ctx = sws_getContext(   av_codec_ctx->width, av_codec_ctx->height, av_codec_ctx->pix_fmt,
                                            t_width, t_height, AV_PIX_FMT_YUV420P,
                                            SWS_LANCZOS, NULL, NULL, NULL
                                        );
            }

            f_response = avcodec_receive_frame(av_codec_ctx, av_frame);
            if(f_response == AVERROR(EAGAIN))
            {
                av_packet_unref(state->av_packet);
                continue;
            }
            else if(f_response == AVERROR_EOF)
            {
                av_packet_unref(state->av_packet);
                break;
            }
            else if(f_response < 0)
            {
                cout<<"Failed to receive frame!"<<endl;
                av_packet_unref(state->av_packet);
                av_frame_unref(state->av_frame);
                return false;
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
        if(f_response != AVERROR_EOF)
        {
            /* Scale the image and send it via decoded_frame */
            sws_scale(  sws_ctx,
                        (uint8_t const * const *)av_frame->data,
                        av_frame->linesize,
                        0,
                        av_frame->height,
                        decoded_frame->data,
                        decoded_frame->linesize
                    );
        }
        av_frame_unref(state->av_frame);
        break;
    }
    return true;
}

void VideoData::close_data(framedata_struct* state)
{
    av_packet_free(&state->av_packet);
    av_frame_free(&state->av_frame);
    av_frame_free(&state->decoded_frame);
    avformat_close_input(&state->av_format_ctx);
    avcodec_free_context(&state->av_codec_ctx);
    avcodec_close(state->av_codec_ctx);
    av_freep(&state->buffer);
    sws_freeContext(state->sws_ctx);
    cout<<"Cleanup complete!"<<endl;
}