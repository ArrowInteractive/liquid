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
    auto& video_stream_index = state->video_stream_index;
    auto& audio_stream_index = state->audio_stream_index;
    auto& f_response = state->f_response;
    auto& p_response = state->p_response;
    auto& f_width = state->f_width;
    auto& f_height = state->f_height;
    auto& frame_data = state->frame_data;
    
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

        // Temporary code here
        // Send the frame width and height to init window
        // Receive the first frame and break
        f_width = av_frame->width;
        f_height = av_frame->height;
        
        break;
    }

    return true;
}

void close_data(framedata_struct* state)
{
    avformat_close_input(&state->av_format_ctx);
    avformat_free_context(state->av_format_ctx);
    av_packet_free(&state->av_packet);
    av_frame_free(&state->av_frame);
    avcodec_free_context(&state->av_codec_ctx);
    cout<<"Successfully freed resources."<<endl;
}