#pragma once
#include "datastate.hpp"

class Clock
{
    public:
        static void init_clock(ClockBase *c, int *queue_serial);
        static void set_clock(ClockBase *c, double pts, int serial);
        static void set_clock_at(ClockBase *c, double pts, int serial, double time);
        static double get_clock(ClockBase *c);
        static int is_realtime(AVFormatContext *s);
        static void sync_clock_to_slave(ClockBase *c, ClockBase *slave);
        static int get_master_sync_type(VideoState *videostate);
        static double get_master_clock(VideoState *videostate);
        static void check_external_clock_speed(VideoState *videostate);
        static void set_clock_speed(ClockBase *c, double speed);
        static double vp_duration(VideoState *videostate, Frame *vp, Frame *nextvp);
        static double compute_target_delay(double delay, VideoState *videostate);
};