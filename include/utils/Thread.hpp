#pragma once
#include "state/datastate.hpp"

class Thread
{
    public:
        static int read_thread(void *arg);
        static int video_thread(void *arg);
        static int audio_thread(void *arg);
        static int subtitle_thread(void *arg);
};