#pragma once
#include "datastate.hpp"

class SeekPause
{
    public:
        static void step_to_next_frame(VideoState *videostate);
        static void stream_toggle_pause(VideoState *videostate);
        static void stream_seek(VideoState *videostate, int64_t pos, int64_t rel, int seek_by_bytes);
        static void execute_seek(VideoState *videostate, double incr);
};