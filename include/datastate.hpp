#pragma once

/*
**  Includes
*/

#include "ui.hpp"
#include "packetqueue.hpp"
#include "framequeue.hpp"

/*
**  Macros
*/

#define FF_QUIT_EVENT    (SDL_USEREVENT + 2)
#define MAX_QUEUE_SIZE (15 * 1024 * 1024)
#define MIN_FRAMES 25
#define EXTERNAL_CLOCK_MIN_FRAMES 2
#define EXTERNAL_CLOCK_MAX_FRAMES 10

#define SDL_AUDIO_MIN_BUFFER_SIZE 512
#define SDL_AUDIO_MAX_CALLBACKS_PER_SEC 30

#define SDL_VOLUME_STEP (0.75)
#define AV_SYNC_THRESHOLD_MIN 0.04
#define AV_SYNC_THRESHOLD_MAX 0.1
#define AV_SYNC_FRAMEDUP_THRESHOLD 0.1
#define AV_NOSYNC_THRESHOLD 10.0

#define SAMPLE_CORRECTION_PERCENT_MAX 10

#define EXTERNAL_CLOCK_SPEED_MIN  0.900
#define EXTERNAL_CLOCK_SPEED_MAX  1.010
#define EXTERNAL_CLOCK_SPEED_STEP 0.001

#define AUDIO_DIFF_AVG_NB   20

#define REFRESH_RATE 0.01

/* NOTE: the size must be big enough to compensate the hardware audio buffersize size */
/* Assuming that the decoded and resampled frames fit here */
#define SAMPLE_ARRAY_SIZE (8 * 65536)

#define CURSOR_HIDE_DELAY 1000000

#define USE_ONEPASS_SUBTITLE_RENDER 1

/* Scaling method */
static unsigned sws_flags = SWS_LANCZOS;

#define FFSWAP(type,a,b) do{type SWAP_tmp= b; b= a; a= SWAP_tmp;}while(0)
#define FF_ARRAY_ELEMS(a) (sizeof(a) / sizeof((a)[0]))

/*
**  Structs
*/

struct AudioParams{
    int freq;
    int channels;
    int64_t channel_layout;
    enum AVSampleFormat fmt;
    int frame_size;
    int bytes_per_sec;
};

struct ClockBase{
    double pts;           /* clock base */
    double pts_drift;     /* clock base minus time at which we updated the clock */
    double last_updated;
    double speed;
    int serial;           /* clock is based on a packet with this serial */
    int paused;
    int *queue_serial;    /* pointer to the current packet queue serial, used for obsolete clock detection */
};

enum{
    AV_SYNC_AUDIO_MASTER,   /* default choice */
    AV_SYNC_VIDEO_MASTER,
    AV_SYNC_EXTERNAL_CLOCK, /* synchronize to an external clock */
};

enum ShowMode{
        SHOW_MODE_NONE = -1, 
        SHOW_MODE_VIDEO = 0, 
        SHOW_MODE_WAVES, 
        SHOW_MODE_RDFT, 
        SHOW_MODE_NB
};

struct DecoderBase{
    AVPacket *pkt;
    PacketQueue *queue;
    AVCodecContext *avctx;
    int pkt_serial;
    int finished;
    int packet_pending;
    SDL_cond *empty_queue_cond;
    int64_t start_pts;
    AVRational start_pts_tb;
    int64_t next_pts;
    AVRational next_pts_tb;
    SDL_Thread *decoder_tid;
};

struct VideoState {
    SDL_Thread *read_tid;
    SDL_Thread *ui_tid;
    const AVInputFormat *iformat;
    int abort_request;
    int force_refresh;
    int paused;
    int last_paused;
    int queue_attachments_req;
    int seek_req;
    int seek_flags;
    int64_t seek_pos;
    int64_t seek_rel;
    int read_pause_return;
    AVFormatContext *ic;
    AVFilterContext *buffersink_ctx;
    AVFilterContext *buffersrc_ctx;
    AVFilterGraph *filter_graph;
    int realtime;

    ClockBase audclk;
    ClockBase vidclk;
    ClockBase extclk;

    FrameQueue pictq;
    FrameQueue subpq;
    FrameQueue sampq;

    DecoderBase auddec;
    DecoderBase viddec;
    DecoderBase subdec;

    int audio_stream;

    int av_sync_type;

    double audio_clock;
    int audio_clock_serial;
    double audio_diff_cum; /* used for AV difference average computation */
    double audio_diff_avg_coef;
    double audio_diff_threshold;
    int audio_diff_avg_count;
    AVStream *audio_st;
    PacketQueue audioq;
    int audio_hw_buf_size;
    uint8_t *audio_buf;
    uint8_t *audio_buf1;
    unsigned int audio_buf_size;    /* in bytes */
    unsigned int audio_buf1_size;
    int audio_buf_index;            /* in bytes */
    int audio_write_buf_size;
    int audio_volume;
    int muted;
    struct AudioParams audio_src;

    struct AudioParams audio_tgt;
    struct SwrContext *swr_ctx;
    int frame_drops_early;
    int frame_drops_late;

    enum ShowMode show_mode;

    int16_t sample_array[SAMPLE_ARRAY_SIZE];
    int sample_array_index;
    int last_i_start;
    RDFTContext *rdft;
    int rdft_bits;
    FFTSample *rdft_data;
    int xpos;
    double last_vis_time;
    SDL_Texture *sub_texture;
    SDL_Texture *vid_texture;

    int subtitle_stream;
    AVStream *subtitle_st;
    PacketQueue subtitleq;

    double frame_timer;
    double frame_last_returned_time;
    double frame_last_filter_delay;
    int video_stream;
    AVStream *video_st;
    PacketQueue videoq;
    double max_frame_duration;      /* Maximum duration of a frame - above this, we consider the jump a timestamp discontinuity */
    struct SwsContext *img_convert_ctx;
    struct SwsContext *sub_convert_ctx;
    int eof;

    char *filename;
    int width, height, xleft, ytop;
    int step;

    int last_video_stream, last_audio_stream, last_subtitle_stream;

    SDL_cond *continue_read_thread;
};

/*
**  Globals
*/

extern int startup_volume;
extern int av_sync_type;
extern SDL_AudioDeviceID audio_dev;
extern int genpts;
extern int find_stream_info;
extern int seek_by_bytes;
extern char *window_title;
extern char *input_filename;
extern int64_t start_time;
extern int64_t duration;
extern int show_status;
extern char* wanted_stream_spec[AVMEDIA_TYPE_NB];
extern enum ShowMode show_mode;
extern int screen_width;
extern int screen_height;
extern int default_width;
extern int default_height;
extern int lowres;
extern const char *audio_codec_name;
extern const char *subtitle_codec_name;
extern const char *video_codec_name;
extern int64_t audio_callback_time;
extern int fast;
extern int infinite_buffer;
extern int loop;
extern int autoexit;
extern int framedrop;
extern int decoder_reorder_pts;
extern int exit_on_keydown;
extern int is_full_screen;
extern float seek_interval;
extern int exit_on_mousedown;
extern int cursor_hidden;
extern int64_t cursor_last_shown;
extern int display_disable;
extern int screen_left;
extern int screen_top;
extern int is_ui_init;
extern double pos;
extern double incr;
extern double frac;
extern AVFormatContext* avformat_ctx;
extern SDL_RendererFlip need_flip;

extern SDL_Window *window;
extern SDL_Renderer *renderer;
extern SDL_GLContext context;

static const struct TextureFormatEntry {
    enum AVPixelFormat format;
    int texture_fmt;
} sdl_texture_format_map[] = {
    { AV_PIX_FMT_RGB8,           SDL_PIXELFORMAT_RGB332 },
    { AV_PIX_FMT_RGB444,         SDL_PIXELFORMAT_RGB444 },
    { AV_PIX_FMT_RGB555,         SDL_PIXELFORMAT_RGB555 },
    { AV_PIX_FMT_BGR555,         SDL_PIXELFORMAT_BGR555 },
    { AV_PIX_FMT_RGB565,         SDL_PIXELFORMAT_RGB565 },
    { AV_PIX_FMT_BGR565,         SDL_PIXELFORMAT_BGR565 },
    { AV_PIX_FMT_RGB24,          SDL_PIXELFORMAT_RGB24 },
    { AV_PIX_FMT_BGR24,          SDL_PIXELFORMAT_BGR24 },
    { AV_PIX_FMT_0RGB32,         SDL_PIXELFORMAT_RGB888 },
    { AV_PIX_FMT_0BGR32,         SDL_PIXELFORMAT_BGR888 },
    { AV_PIX_FMT_NE(RGB0, 0BGR), SDL_PIXELFORMAT_RGBX8888 },
    { AV_PIX_FMT_NE(BGR0, 0RGB), SDL_PIXELFORMAT_BGRX8888 },
    { AV_PIX_FMT_RGB32,          SDL_PIXELFORMAT_ARGB8888 },
    { AV_PIX_FMT_RGB32_1,        SDL_PIXELFORMAT_RGBA8888 },
    { AV_PIX_FMT_BGR32,          SDL_PIXELFORMAT_ABGR8888 },
    { AV_PIX_FMT_BGR32_1,        SDL_PIXELFORMAT_BGRA8888 },
    { AV_PIX_FMT_YUV420P,        SDL_PIXELFORMAT_IYUV },
    { AV_PIX_FMT_YUYV422,        SDL_PIXELFORMAT_YUY2 },
    { AV_PIX_FMT_UYVY422,        SDL_PIXELFORMAT_UYVY },
    { AV_PIX_FMT_NONE,           SDL_PIXELFORMAT_UNKNOWN },
};

/*
**  Functions
*/



// Seek & Pause class


// Event class 


// Filtering functions
int init_filters(const char *filters_descr);

// Exit functions
void do_exit(VideoState *videostate);