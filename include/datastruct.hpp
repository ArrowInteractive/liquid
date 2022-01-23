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

    AVFormatContext* av_format_ctx = nullptr;

    // Video vars
    AVCodecParameters* video_codec_params = nullptr;
    AVCodec* video_codec = nullptr;
    AVCodecContext* video_codec_ctx = nullptr;
    AVPacket* video_packet = nullptr;
    AVFrame* video_frame = nullptr;
    AVFrame * decoded_video_frame = nullptr;
    uint8_t * video_buffer = nullptr;
    SwsContext* sws_ctx = nullptr;

    // Audio vars
    AVCodec* audio_codec = nullptr;
};

#endif