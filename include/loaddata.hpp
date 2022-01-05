#ifndef LOADDATA
#define LOADDATA

#include <iostream>
using namespace std;

extern "C"
{
    #include <libavcodec/avcodec.h>
    #include <libavformat/avformat.h>
    #include <inttypes.h>
}

// Functions
bool load_data( char* filename, 
                AVFormatContext* av_format_ctx, 
                AVCodecParameters* av_codec_params, 
                AVCodec* av_codec, 
                AVCodecContext* av_codec_ctx,
                AVPacket* av_packet,
                AVFrame* av_frame
            );
#endif