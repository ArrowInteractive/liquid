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

    // Printing metadata
    cout<<"File Name : "<<av_format_ctx->AVFormatContext::filename<<endl;
    cout<<"Duration  : "<<av_format_ctx->AVFormatContext::duration<<endl;
    return false;
}
