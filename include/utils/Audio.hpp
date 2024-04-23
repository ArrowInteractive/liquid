#pragma once
#include "state/datastate.hpp"

class Audio
{
    public:
        static int audio_open(void *opaque, int64_t wanted_channel_layout, int wanted_nb_channels, int wanted_sample_rate, struct AudioParams *audio_hw_params);
        static void sdl_audio_callback(void *opaque, Uint8 *stream, int len);
        static int audio_decode_frame(VideoState *videostate);
        static int synchronize_audio(VideoState *videostate, int nb_samples);
};