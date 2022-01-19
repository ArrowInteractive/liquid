#ifndef VIDEODATA
#define VIDEODATA
#include <iostream>
using namespace std;

extern "C"
{
    #include <libavcodec/avcodec.h>
    #include <libavformat/avformat.h>
    #include <libswscale/swscale.h>
    #include <libavutil/imgutils.h>
}

struct framedata_struct
{
    int t_width=0, t_height=0, video_stream_index = -1, audio_stream_index = -1, p_response, f_response, num_bytes;
    AVFormatContext* av_format_ctx;
    AVCodecParameters* av_codec_params;
    AVCodec* av_codec;
    AVCodecContext* av_codec_ctx;
    AVPacket* av_packet;
    AVFrame* av_frame;
    AVFrame * decoded_frame;
    uint8_t * buffer = NULL;
    SwsContext* sws_ctx = NULL;
};

class VideoData
{
private:


public:
    bool load_data(char* filename, framedata_struct* state);
    bool load_frames(framedata_struct* state);
    void close_data(framedata_struct* state);
};

#endif