#pragma once
#include "datastate.hpp"

class Stream{

    public:
        static VideoState *stream_open(char *filename);
        static void stream_close(VideoState *videostate);
        static int stream_component_open(VideoState *videostate, int stream_index);
        static void stream_component_close(VideoState *videostate, int stream_index);
        static int stream_has_enough_packets(AVStream *st, int stream_id, PacketQueue *queue);
};