#include <loaddata.hpp>

// Variables
AVFormatContext* av_format_ctx;
AVCodecParameters* av_codec_params;
AVCodec* av_codec;

bool init_ctx()
{
    if(!(av_format_ctx = avformat_alloc_context()))
    {
        return false;
    }
    return true;
}

bool open_input(char* _filename)
{
    if(avformat_open_input(&av_format_ctx, _filename, NULL, NULL) != 0)
    {
        return false;
    }

    for(int i=0; i < av_format_ctx->nb_streams; i++)
    {
        av_codec_params = av_format_ctx->streams[i]->codecpar;
        av_codec = avcodec_find_decoder(av_codec_params->codec_id);
        cout<<"Stream ID : "<<i<<endl;
        cout<<"CODEC     : "<<av_codec->AVCodec::long_name<<endl;
    }

    return true;
}