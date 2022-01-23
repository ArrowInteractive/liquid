#ifndef DATASTRUCT
#define DATASTRUCT

extern "C"
{
    #include <libavcodec/avcodec.h>
    #include <libavformat/avformat.h>
    #include <libswscale/swscale.h>
    #include <libavutil/imgutils.h>
}

struct datastruct
{
    int d_width, d_height,
        t_width, t_height,
        video_stream_index = -1, 
        audio_stream_index = -1, 
        response, num_bytes,
        max_frames_to_decode;
    double fps, sleep_time;
    AVFormatContext* av_format_ctx = NULL;
    AVCodecParameters* av_codec_params = NULL;
    AVCodec* av_codec = NULL;
    AVCodecContext* av_codec_ctx = NULL;
    AVPacket* av_packet = NULL;
    AVFrame* av_frame = NULL;
    AVFrame * decoded_frame = NULL;
    uint8_t * buffer = NULL;
    SwsContext* sws_ctx = NULL;
};

#endif