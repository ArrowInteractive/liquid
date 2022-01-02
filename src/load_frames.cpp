#include <loadframes.hpp>

bool load_frames(const char* filename, int* width, int* height, unsigned char** data)
{
    AVFormatContext* av_format_ctx = avformat_alloc_context();
    if(!av_format_ctx)
    {
        cout<<"Couldn't create AVFormatContext!"<<endl;
        return false;
    }

    if(avformat_open_input(&av_format_ctx, filename, NULL, NULL) != 0)
    {
        cout<<"Couldn't open source media!"<<endl;
        return false;
    }

    return false;
}