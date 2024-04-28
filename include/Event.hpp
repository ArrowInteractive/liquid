#pragma once
#include "state/datastate.hpp"

class Event
{
    public:
        static void event_loop(VideoState *videostate);
        static void refresh_loop_wait_event(VideoState *videostate, SDL_Event *event);
        static void toggle_full_screen(VideoState *videostate);
        static void toggle_pause(VideoState *videostate);
        static void toggle_mute(VideoState *videostate);
        static void update_volume(VideoState *videostate);
        static void stream_cycle_channel(VideoState *videostate, int codec_type);
        static void seek_chapter(VideoState *videostate, int incr);
        static Uint32 hide_ui(Uint32 interval,void* param);
};