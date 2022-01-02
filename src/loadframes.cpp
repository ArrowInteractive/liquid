#include <loadframes.hpp>

bool load_frames(const char* _filename, int* width, int* height, unsigned char** data)
{
    AVFormatContext* av_format_ctx = avformat_alloc_context();
    if(!av_format_ctx)
    {
        cout<<"Couldn't create AVFormatContext!"<<endl;
        return false;
    }

    if(avformat_open_input(&av_format_ctx, _filename, NULL, NULL) != 0)
    {
        cout<<"Couldn't open source media!"<<endl;
        return false;
    }

    // Parsing data
    AVCodecParameters* av_codec_params;
    AVCodec* av_codec;
    cout<<"File Name : "<<av_format_ctx->AVFormatContext::filename<<endl;
    for(int i=0; av_format_ctx->nb_streams; i++)
    {
        auto stream = av_format_ctx->streams[i];
        av_codec_params = av_format_ctx->streams[i]->codecpar;
        av_codec = avcodec_find_decoder(av_codec_params->codec_id);
        cout<<"Stream ID : "<<i<<endl;
        cout<<"CODEC     : "<<av_codec->AVCodec::long_name<<endl;
    }

    return true;
}
