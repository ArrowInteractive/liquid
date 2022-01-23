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
    AVFormatContext* video_format_ctx = NULL;
    AVCodecParameters* video_codec_params = NULL;
    AVCodec* video_codec = NULL;
    AVCodecContext* video_codec_ctx = NULL;
    AVPacket* video_packet = NULL;
    AVFrame* video_frame = NULL;
    AVFrame * decoded_video_frame = NULL;
    uint8_t * video_buffer = NULL;
    SwsContext* sws_ctx = NULL;
};

#endif