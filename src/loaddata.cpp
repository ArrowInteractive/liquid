#include <loaddata.hpp>

bool load_data( char* filename, 
                AVFormatContext* av_format_ctx, 
                AVCodecParameters* av_codec_params, 
                AVCodec* av_codec, 
                AVCodecContext* av_codec_ctx,
                AVPacket* av_packet,
                AVFrame* av_frame,
                framedata_struct* state
            )
{
    // Variables
    int video_stream_index = -1, audio_stream_index = -1, p_response, f_response;
    auto& width = state->f_width;
    
    // Allocate AVPacket
    if(!(av_packet = av_packet_alloc()))
    {
        cout<<"Couldn't allocate AVPacket!"<<endl;
        return false;
    }

    // Allocate AVFrame
    if(!(av_frame = av_frame_alloc()))
    {
        cout<<"Couldn't allocate AVFrame!"<<endl;
        return false;
    }

    // Allocating AVFormatContext
    if(!(av_format_ctx = avformat_alloc_context()))
    {
        cout<<"Couldn't initialize AVFormatContext!"<<endl;
        return false;
    }

    // Opening file
    if(avformat_open_input(&av_format_ctx, filename, NULL, NULL) != 0)
    {
        cout<<"Couldn't open file!"<<endl;
        return false;
    }

    // Finding streams
    for(int i=0; i < av_format_ctx->nb_streams; i++)
    {
        av_codec_params = av_format_ctx->streams[i]->codecpar;
        av_codec = avcodec_find_decoder(av_codec_params->codec_id);
        cout<<"Stream ID : "<<i<<endl;
        cout<<"CODEC     : "<<av_codec->AVCodec::long_name<<endl;

        // Finding stream ids
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

    // If no stream was found, exit
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
        cout<<"Frame width  : "<<av_frame->width<<endl;
        cout<<"Frame height : "<<av_frame->height<<endl;
        // Setup a break point here

        
        break;
    }

    return true;
}