#ifndef VIDEOSTATE
#define VIDEOSTATE

/*
    Macros
*/

// Audio macros
#define SDL_AUDIO_BUFFER_SIZE 1024
#define MAX_AUDIO_FRAME_SIZE 192000
#define MAX_AUDIOQ_SIZE (5 * 16 * 1024)
#define AUDIO_DIFF_AVG_NB 20

// Video macros
#define VIDEO_PICTURE_QUEUE_SIZE 1
#define MAX_VIDEOQ_SIZE (5 * 256 * 1024)

// Event macros
#define FF_REFRESH_EVENT (SDL_USEREVENT)

// Sync macros
#define AV_NOSYNC_THRESHOLD 1.0
#define SAMPLE_CORRECTION_PERCENT_MAX 10
#define AV_SYNC_THRESHOLD 0.01


/*
    Audio Video Sync Types.
*/

enum
{
    /*
        Sync to audio clock.
    */
    AV_SYNC_AUDIO_MASTER,

    /*
        Sync to video clock.
    */
    AV_SYNC_VIDEO_MASTER,

    /*
        Sync to external clock: the computer clock
    */
    AV_SYNC_EXTERNAL_MASTER,
};
#define DEFAULT_AV_SYNC_TYPE AV_SYNC_AUDIO_MASTER

/*
    Includes
*/

#include <iostream>
#include <assert.h>
#include <SDL2/SDL.h>
extern "C"
{
    #include <libavcodec/avcodec.h>
    #include <libavformat/avformat.h>
    #include <libswscale/swscale.h>
    #include <libswresample/swresample.h>
    #include <libavutil/imgutils.h>
    #include <libavutil/avutil.h>
    #include <libavutil/time.h>
    #include <libavutil/macros.h>
    #include <libavutil/opt.h>
}

/*
    Structs
*/

struct PacketQueue
{
    AVPacketList*       first_pkt;
    AVPacketList*       last_pkt;
    int                 nb_packets;
    int                 size;
    SDL_mutex*          mutex;
    SDL_cond*           cond;
};

struct VideoPicture
{
    AVFrame*            frame;
    int                 width;
    int                 height;
    int                 allocated;
    double              pts;
};

struct AudioResamplingState
{
    SwrContext * swr_ctx;
    int64_t in_channel_layout;
    uint64_t out_channel_layout;
    int out_nb_channels;
    int out_linesize;
    int in_nb_samples;
    int64_t out_nb_samples;
    int64_t max_out_nb_samples;
    uint8_t ** resampled_data;
    int resampled_data_size;

};

struct VideoState
{
    // File format context
    AVFormatContext*    pformatctx;


    // Audio stream
    int                 audio_stream_index;
    AVStream*           audio_stream;
    AVCodecContext*     audio_ctx;
    PacketQueue         audioq;
    uint8_t             audio_buf[(MAX_AUDIO_FRAME_SIZE * 3) /2];
    unsigned int        audio_buf_size;
    unsigned int        audio_buf_index;
    AVFrame             audio_frame;
    AVPacket            audio_pkt;
    uint8_t*            audio_pkt_data;
    int                 audio_pkt_size;
    double              audio_clock;

    // Video stream
    int                 video_stream_index;
    AVStream*           video_stream;
    AVCodecContext*     video_ctx;
    PacketQueue         videoq;
    SwsContext*         sws_ctx;
    double              frame_timer;
    double              frame_last_pts;
    double              frame_last_delay;
    double              video_clock;
    double              video_current_pts;
    int64_t             video_current_pts_time;
    double              audio_diff_cum;
    double              audio_diff_avg_coef;
    double              audio_diff_threshold;
    int                 audio_diff_avg_count;

    // Window, Display Mode, Renderer, Texture, and Events
    SDL_Window*         window;
    SDL_mutex*          window_mutex;
    SDL_Texture*        texture;
    SDL_Renderer*       renderer;
    SDL_Event           event;
    SDL_DisplayMode     display_mode;

    // Video picture queue
    VideoPicture        pictq[VIDEO_PICTURE_QUEUE_SIZE];
    int                 pictq_size;
    int                 pictq_rindex;
    int                 pictq_windex;
    SDL_mutex*          pictq_mutex;
    SDL_cond*           pictq_cond;

    // AV Sync
    int                 av_sync_type;
    double              external_clock;
    int64_t             external_clock_time;

    // Seeking
    int                 seek_req;
    int                 seek_flags;
    int64_t             seek_pos;

    // Threads
    SDL_Thread*         decode_thd;
    SDL_Thread*         video_thd;
    SDL_Thread*         render_thd;

    // Input filename
    const char*         filename;

    // Flags
    int                 ret;
    int                 quit;
    bool                is_fullscreen = false;
    bool                change_scaling = false;

    // Flush packet
    AVPacket*           flush_pkt;
};

/*
    Functions
*/

// Packet queue
void packet_queue_init(PacketQueue* queue);
int packet_queue_put(PacketQueue* queue, AVPacket* packet);
int packet_queue_get(VideoState* videostate, PacketQueue* queue, AVPacket* packet, int blocking);
void packet_queue_flush(PacketQueue* queue);

// Schedule
void schedule_refresh(VideoState* videostate, Uint32 delay);
Uint32 sdl_refresh_timer_cb(Uint32 interval, void * param);

// Decode
int decode_info(VideoState* videostate);
int decode_thread(void * arg);
int stream_component_open(VideoState* videostate, int stream_index);

// Window, renderer, texture
int render_thread(void * arg);

// Video
int video_thread(void * arg);
int64_t guess_correct_pts(AVCodecContext* ctx, int64_t reordered_pts, int64_t dts);
double synchronize_video(VideoState* videostate, AVFrame* d_frame, double pts);
int queue_picture(VideoState* videostate, AVFrame* d_frame, double pts);
void alloc_picture(void * arg);

// Audio
void audio_callback(void * arg, Uint8* stream, int len);
int audio_decode_frame(VideoState* videostate, uint8_t* audio_buf, int buf_size, double* pts_ptr);
int audio_resampling(VideoState* videostate, AVFrame* decoded_audio_frame, enum AVSampleFormat out_sample_fmt, uint8_t* out_buf);
AudioResamplingState* get_audio_resampling(uint64_t channel_layout);
int synchronize_audio(VideoState* videostate, short* samples, int samples_size);

// Clocks
double get_master_clock(VideoState* videostate);
double get_video_clock(VideoState* videostate);
double get_audio_clock(VideoState* videostate);
double get_external_clock(VideoState* videostate);

// Refresh
void video_refresh_timer(void * arg);
void video_display(VideoState* videostate);

#endif
