#pragma once
#include "state/datastate.hpp"

class Window
{
    public:
        static int create_window();
        static void set_default_window_size(int width, int height, AVRational sar);
        static void calculate_display_rect(
            SDL_Rect *rect, int scr_xleft, 
            int scr_ytop, int scr_width, int scr_height,
            int pic_width, int pic_height, AVRational pic_sar
        );
        static void update_sample_display(VideoState *videostate, short *samples, int samples_size);
};