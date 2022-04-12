#pragma once
#include "datastate.hpp"

class Video
{
    public:
        static int get_video_frame(VideoState *videostate, AVFrame *frame);
        static int queue_picture(VideoState *videostate, AVFrame *src_frame, double pts, double duration, int64_t pos, int serial);
        static void video_refresh(void *arg, double *remaining_time);
        static void update_video_pts(VideoState *videostate, double pts, int64_t pos, int serial);
        static void video_display(VideoState *videostate);
        static int video_open(VideoState *videostate);
        static void fill_rectangle(int x, int y, int w, int h);
        static int compute_mod(int a, int b);
        static int realloc_texture(SDL_Texture **texture, Uint32 new_format, int new_width, int new_height, SDL_BlendMode blendmode, int init_texture);
        static void video_image_display(VideoState *videostate);
        static int upload_texture(SDL_Texture **tex, AVFrame *frame, struct SwsContext **img_convert_ctx);
        static void set_sdl_yuv_conversion_mode(AVFrame *frame);
        static void get_sdl_pix_fmt_and_blendmode(int format, Uint32 *sdl_pix_fmt, SDL_BlendMode *sdl_blendmode);
};