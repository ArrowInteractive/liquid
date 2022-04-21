/*
**  Includes
*/

#include "datastate.hpp"
#include "Stream.hpp"

/*
**  Globals
*/

int startup_volume = 100;

int av_sync_type = AV_SYNC_AUDIO_MASTER;
SDL_AudioDeviceID audio_dev;
int genpts = 0;
int find_stream_info = 1;
int seek_by_bytes = -1;
char *window_title;
char *input_filename;
int64_t start_time = AV_NOPTS_VALUE;
int64_t duration = AV_NOPTS_VALUE;
int show_status = -1;
char* wanted_stream_spec[AVMEDIA_TYPE_NB] = {0};
enum ShowMode show_mode = SHOW_MODE_NONE;
int screen_width  = 0;
int screen_height = 0;
int default_width  = 640;
int default_height = 480;
int lowres = 0;
const char *audio_codec_name;
const char *subtitle_codec_name;
const char *video_codec_name;
int64_t audio_callback_time;
int fast = 0;
int infinite_buffer = -1;
int loop = 1;
int autoexit;
int framedrop = -1;
int decoder_reorder_pts = -1;
int exit_on_keydown;
int is_full_screen;
float seek_interval = 10;
int exit_on_mousedown;
int cursor_hidden = 0;
int64_t cursor_last_shown;
int display_disable;
int screen_left = SDL_WINDOWPOS_CENTERED;
int screen_top = SDL_WINDOWPOS_CENTERED;

AVFormatContext* avformat_ctx;
std::string current_time;
std::string max_video_duration;
SDL_RendererFlip need_flip;

SDL_Window *window;
SDL_Renderer *renderer;
SDL_GLContext context;


/*
**  Filtering functions
*/

int init_filters(const char *filters_descr){
    // Dummy
    return 0;
}

/*
**  Exit functions
*/

void do_exit(VideoState *videostate)
{
    if (videostate) {
        Stream::stream_close(videostate);
    }
    if (renderer)
        SDL_DestroyRenderer(renderer);
    if (window)
        SDL_DestroyWindow(window);

    destroy_imgui_data();
    SDL_Quit();
    exit(0);
}