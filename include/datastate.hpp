#pragma once

/*
**  Includes
*/

#include "ui.hpp"
#include "packetqueue.hpp"
#include "framequeue.hpp"
#include "shader.hpp"

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

struct Clock{
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

struct Decoder{
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
    int realtime;

    Clock audclk;
    Clock vidclk;
    Clock extclk;

    FrameQueue pictq;
    FrameQueue subpq;
    FrameQueue sampq;

    Decoder auddec;
    Decoder viddec;
    Decoder subdec;

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
extern int video_disable;
extern int audio_disable;
extern int subtitle_disable;
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
extern double rdftspeed;
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

// Stream functions
VideoState *stream_open(char *filename);
void stream_close(VideoState *videostate);
int stream_component_open(VideoState *videostate, int stream_index);
void stream_component_close(VideoState *videostate, int stream_index);
int stream_has_enough_packets(AVStream *st, int stream_id, PacketQueue *queue);

// Clock functions
void init_clock(Clock *c, int *queue_serial);
void set_clock(Clock *c, double pts, int serial);
void set_clock_at(Clock *c, double pts, int serial, double time);
double get_clock(Clock *c);
int is_realtime(AVFormatContext *s);
void sync_clock_to_slave(Clock *c, Clock *slave);
int get_master_sync_type(VideoState *videostate);
double get_master_clock(VideoState *videostate);
void check_external_clock_speed(VideoState *videostate);
void set_clock_speed(Clock *c, double speed);
double vp_duration(VideoState *videostate, Frame *vp, Frame *nextvp);
double compute_target_delay(double delay, VideoState *videostate);

// Decoder functions
int decoder_init(Decoder *d, AVCodecContext *avctx, PacketQueue *queue, SDL_cond *empty_queue_cond);
int decoder_start(Decoder *d, int (*fn)(void *), const char *thread_name, void* arg);
void decoder_abort(Decoder *d, FrameQueue *fq);
void decoder_destroy(Decoder *d);

// Thread functions
int read_thread(void *arg);
int video_thread(void *arg);
int audio_thread(void *arg);
int subtitle_thread(void *arg);

// Decode functions
int decode_interrupt_cb(void *ctx);
int decoder_decode_frame(Decoder *d, AVFrame *frame, AVSubtitle *sub);

// Window functions
int create_window();
void set_default_window_size(int width, int height, AVRational sar);
void calculate_display_rect(
    SDL_Rect *rect, int scr_xleft, 
    int scr_ytop, int scr_width, int scr_height,
    int pic_width, int pic_height, AVRational pic_sar
);
void update_sample_display(VideoState *videostate, short *samples, int samples_size);

// Video functions
int get_video_frame(VideoState *videostate, AVFrame *frame);
int queue_picture(VideoState *videostate, AVFrame *src_frame, double pts, double duration, int64_t pos, int serial);
void video_refresh(void *arg, double *remaining_time);
void update_video_pts(VideoState *videostate, double pts, int64_t pos, int serial);
void video_display(VideoState *videostate);
int video_open(VideoState *videostate);
void fill_rectangle(int x, int y, int w, int h);
int compute_mod(int a, int b);
int realloc_texture(SDL_Texture **texture, Uint32 new_format, int new_width, int new_height, SDL_BlendMode blendmode, int init_texture);
void video_image_display(VideoState *videostate);
int upload_texture(SDL_Texture **tex, AVFrame *frame, struct SwsContext **img_convert_ctx);
void set_sdl_yuv_conversion_mode(AVFrame *frame);
void get_sdl_pix_fmt_and_blendmode(int format, Uint32 *sdl_pix_fmt, SDL_BlendMode *sdl_blendmode);

// Audio functions
int audio_open(void *opaque, int64_t wanted_channel_layout, int wanted_nb_channels, int wanted_sample_rate, struct AudioParams *audio_hw_params);
void sdl_audio_callback(void *opaque, Uint8 *stream, int len);
int audio_decode_frame(VideoState *videostate);
int synchronize_audio(VideoState *videostate, int nb_samples);

// Seek & Pause functions
void step_to_next_frame(VideoState *videostate);
void stream_toggle_pause(VideoState *videostate);
void stream_seek(VideoState *videostate, int64_t pos, int64_t rel, int seek_by_bytes);
void execute_seek(VideoState *videostate, double incr);

// Event functions
void event_loop(VideoState *videostate);
void refresh_loop_wait_event(VideoState *videostate, SDL_Event *event);
void toggle_full_screen(VideoState *videostate);
void toggle_pause(VideoState *videostate);
void toggle_mute(VideoState *videostate);
void update_volume(VideoState *videostate, int sign, double step);
void stream_cycle_channel(VideoState *videostate, int codec_type);
void seek_chapter(VideoState *videostate, int incr);
Uint32 hide_ui(Uint32 interval,void* param);

// Exit functions
void do_exit(VideoState *videostate);