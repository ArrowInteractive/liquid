#include <loaddata.hpp>

bool load_data(char* filename, AVFormatContext* av_format_ctx, AVCodecParameters* av_codec_params, AVCodec* av_codec)
{
    // Variables
    int video_stream_index = -1;
    int audio_stream_index = -1;

    // Initializing AVFormatContext
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

    if(video_stream_index == -1 && audio_stream_index == -1)
    {
        cout<<"Couldn't find any streams in the file!"<<endl;
        return false;
    }

    return true;
}