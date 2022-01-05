#include <loaddata.hpp>

bool load_data(char* filename, AVFormatContext* av_format_ctx, AVCodecParameters* av_codec_params, AVCodec* av_codec)
{
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
    }

    return true;
}