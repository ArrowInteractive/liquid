#pragma once
#include "state/datastate.hpp"

class Video
{
    public:
        static int get_video_frame(VideoState *videostate, AVFrame *frame);
        static int queue_picture(VideoState *videostate, AVFrame *src_frame, double pts, double duration, int64_t pos, int serial);
        static void video_refresh(void *arg, double *remaining_time);
        static void update_video_pts(VideoState *videostate, double pts, int64_t pos, int serial);
        static void video_display(VideoState *videostate);
        static int video_open(VideoState *videostate);
        static int compute_mod(int a, int b);
        static void video_image_display(VideoState *videostate);
        static void present();
};