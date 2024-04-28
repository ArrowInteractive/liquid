#include "utils/Audio.hpp"
#include "utils/Clock.hpp"
#include "Window.hpp"

int Audio::audio_open(void *opaque, int64_t wanted_channel_layout, int wanted_nb_channels, int wanted_sample_rate, struct AudioParams *audio_hw_params)
{
    SDL_AudioSpec wanted_spec, spec;
    const char *env;
    static const int next_nb_channels[] = {0, 0, 1, 6, 2, 6, 4, 6};
    static const int next_sample_rates[] = {0, 44100, 48000, 96000, 192000};
    int next_sample_rate_idx = FF_ARRAY_ELEMS(next_sample_rates) - 1;

    env = SDL_getenv("SDL_AUDIO_CHANNELS");
    if (env) {
        wanted_nb_channels = atoi(env);
        wanted_channel_layout = av_get_default_channel_layout(wanted_nb_channels);
    }
    if (!wanted_channel_layout || wanted_nb_channels != av_get_channel_layout_nb_channels(wanted_channel_layout)) {
        wanted_channel_layout = av_get_default_channel_layout(wanted_nb_channels);
        wanted_channel_layout &= ~AV_CH_LAYOUT_STEREO_DOWNMIX;
    }
    wanted_nb_channels = av_get_channel_layout_nb_channels(wanted_channel_layout);
    wanted_spec.channels = wanted_nb_channels;
    wanted_spec.freq = wanted_sample_rate;
    if (wanted_spec.freq <= 0 || wanted_spec.channels <= 0) {
        std::cout<<"FATAL ERROR: Invalid sample rate or channel count!"<<std::endl;
        return -1;
    }
    while (next_sample_rate_idx && next_sample_rates[next_sample_rate_idx] >= wanted_spec.freq)
        next_sample_rate_idx--;
    wanted_spec.format = AUDIO_S16SYS;
    wanted_spec.silence = 0;
    wanted_spec.samples = FFMAX(SDL_AUDIO_MIN_BUFFER_SIZE, 2 << wanted_spec.freq / SDL_AUDIO_MAX_CALLBACKS_PER_SEC);
    wanted_spec.callback = Audio::sdl_audio_callback;
    wanted_spec.userdata = opaque;
    while (!(audio_dev = SDL_OpenAudioDevice(NULL, 0, &wanted_spec, &spec, SDL_AUDIO_ALLOW_FREQUENCY_CHANGE | SDL_AUDIO_ALLOW_CHANNELS_CHANGE))) {
        wanted_spec.channels = next_nb_channels[FFMIN(7, wanted_spec.channels)];
        if (!wanted_spec.channels) {
            wanted_spec.freq = next_sample_rates[next_sample_rate_idx--];
            wanted_spec.channels = wanted_nb_channels;
            if (!wanted_spec.freq) {
                std::cout<<"FATAL ERROR: Could not open audio device!"<<std::endl;
                return -1;
            }
        }
        wanted_channel_layout = av_get_default_channel_layout(wanted_spec.channels);
    }
    if (spec.format != AUDIO_S16SYS) {
        std::cout<<"SDL adviced audio format is not supported: "<<spec.format<<std::endl;
        return -1;
    }
    if (spec.channels != wanted_spec.channels) {
        wanted_channel_layout = av_get_default_channel_layout(spec.channels);
        if (!wanted_channel_layout) {
            std::cout<<"SDL adviced channel layout is not supported: "<<spec.channels<<std::endl;
            return -1;
        }
    }

    audio_hw_params->fmt = AV_SAMPLE_FMT_S16;
    audio_hw_params->freq = spec.freq;
    audio_hw_params->channel_layout = wanted_channel_layout;
    audio_hw_params->channels =  spec.channels;
    audio_hw_params->frame_size = av_samples_get_buffer_size(NULL, audio_hw_params->channels, 1, audio_hw_params->fmt, 1);
    audio_hw_params->bytes_per_sec = av_samples_get_buffer_size(NULL, audio_hw_params->channels, audio_hw_params->freq, audio_hw_params->fmt, 1);
    if (audio_hw_params->bytes_per_sec <= 0 || audio_hw_params->frame_size <= 0) {
        std::cout<<"FATAL ERROR: av_samples_get_buffer_size() failed!"<<std::endl;
        return -1;
    }
    return spec.size;
}

void Audio::sdl_audio_callback(void *opaque, Uint8 *stream, int len)
{
    VideoState *videostate = (VideoState *)opaque;
    int audio_size, len1;

    audio_callback_time = av_gettime_relative();

    while (len > 0) {
        if (videostate->audio_buf_index >= videostate->audio_buf_size) {
           audio_size = Audio::audio_decode_frame(videostate);
           if (audio_size < 0) {
                /* if error, just output silence */
               videostate->audio_buf = NULL;
               videostate->audio_buf_size = SDL_AUDIO_MIN_BUFFER_SIZE / videostate->audio_tgt.frame_size * videostate->audio_tgt.frame_size;
           } else {
               if (videostate->show_mode != SHOW_MODE_VIDEO)
                   Window::update_sample_display(videostate, (int16_t *)videostate->audio_buf, audio_size);
               videostate->audio_buf_size = audio_size;
           }
           videostate->audio_buf_index = 0;
        }
        len1 = videostate->audio_buf_size - videostate->audio_buf_index;
        if (len1 > len)
            len1 = len;
        if (!videostate->muted && videostate->audio_buf && videostate->audio_volume == SDL_MIX_MAXVOLUME)
            memcpy(stream, (uint8_t *)videostate->audio_buf + videostate->audio_buf_index, len1);
        else {
            memset(stream, 0, len1);
            if (!videostate->muted && videostate->audio_buf)
                SDL_MixAudioFormat(stream, (uint8_t *)videostate->audio_buf + videostate->audio_buf_index, AUDIO_S16SYS, len1, videostate->audio_volume);
        }
        len -= len1;
        stream += len1;
        videostate->audio_buf_index += len1;
    }
    videostate->audio_write_buf_size = videostate->audio_buf_size - videostate->audio_buf_index;
    /* Let's assume the audio driver that is used by SDL has two periods. */
    if (!isnan(videostate->audio_clock)) {
        Clock::set_clock_at(&videostate->audclk, videostate->audio_clock - (double)(2 * videostate->audio_hw_buf_size + videostate->audio_write_buf_size) / videostate->audio_tgt.bytes_per_sec, videostate->audio_clock_serial, audio_callback_time / 1000000.0);
        Clock::sync_clock_to_slave(&videostate->extclk, &videostate->audclk);
    }
}

int Audio::audio_decode_frame(VideoState *videostate)
{
    int data_size, resampled_data_size;
    int64_t dec_channel_layout;
    av_unused double audio_clock0;
    int wanted_nb_samples;
    Frame *af;

    if (videostate->paused)
    {
        return -1;
    }

    do {
#if defined(_WIN32)
        while (frame_queue_nb_remaining(&videostate->sampq) == 0) {
            if ((av_gettime_relative() - audio_callback_time) > 1000000LL * videostate->audio_hw_buf_size / videostate->audio_tgt.bytes_per_sec / 2)
                return -1;
            av_usleep (1000);
        }
#endif
        if (!(af = frame_queue_peek_readable(&videostate->sampq)))
            return -1;
        frame_queue_next(&videostate->sampq);
    } while (af->serial != videostate->audioq.serial);

    data_size = av_samples_get_buffer_size(
        NULL, 
        af->frame->channels,
        af->frame->nb_samples,
        static_cast<AVSampleFormat>(af->frame->format),
        1
    );

    dec_channel_layout =
        (af->frame->channel_layout && af->frame->channels == av_get_channel_layout_nb_channels(af->frame->channel_layout)) ?
        af->frame->channel_layout : av_get_default_channel_layout(af->frame->channels);
    wanted_nb_samples = Audio::synchronize_audio(videostate, af->frame->nb_samples);

    if (af->frame->format        != videostate->audio_src.fmt            ||
        dec_channel_layout       != videostate->audio_src.channel_layout ||
        af->frame->sample_rate   != videostate->audio_src.freq           ||
        (wanted_nb_samples       != af->frame->nb_samples && !videostate->swr_ctx)) {
        swr_free(&videostate->swr_ctx);
        videostate->swr_ctx = swr_alloc_set_opts(
            NULL,
            videostate->audio_tgt.channel_layout, 
            videostate->audio_tgt.fmt,
            videostate->audio_tgt.freq,
            dec_channel_layout,         
            static_cast<AVSampleFormat>(af->frame->format),
            af->frame->sample_rate,
            0, 
            NULL
        );
        if (!videostate->swr_ctx || swr_init(videostate->swr_ctx) < 0)
        {
            std::cout<<"ERROR: Could not create audio resampler context!"<<std::endl;
            swr_free(&videostate->swr_ctx);
            return -1;
        }
        videostate->audio_src.channel_layout = dec_channel_layout;
        videostate->audio_src.channels       = af->frame->channels;
        videostate->audio_src.freq = af->frame->sample_rate;
        videostate->audio_src.fmt = static_cast<AVSampleFormat>(af->frame->format);
    }

    if (videostate->swr_ctx) {
        const uint8_t **in = (const uint8_t **)af->frame->extended_data;
        uint8_t **out = &videostate->audio_buf1;
        int out_count = (int64_t)wanted_nb_samples * videostate->audio_tgt.freq / af->frame->sample_rate + 256;
        int out_size  = av_samples_get_buffer_size(NULL, videostate->audio_tgt.channels, out_count, videostate->audio_tgt.fmt, 0);
        int len2;
        if (out_size < 0) {
            std::cout<<"FATAL ERROR: av_samples_get_buffer_size() failed!"<<std::endl;
            return -1;
        }
        if (wanted_nb_samples != af->frame->nb_samples) {
            if (swr_set_compensation(videostate->swr_ctx, (wanted_nb_samples - af->frame->nb_samples) * videostate->audio_tgt.freq / af->frame->sample_rate,
                                        wanted_nb_samples * videostate->audio_tgt.freq / af->frame->sample_rate) < 0) {
                std::cout<<"FATAL ERROR: swr_set_compensation() failed!"<<std::endl;
                return -1;
            }
        }
        av_fast_malloc(&videostate->audio_buf1, &videostate->audio_buf1_size, out_size);
        if (!videostate->audio_buf1)
            return AVERROR(ENOMEM);
        len2 = swr_convert(videostate->swr_ctx, out, out_count, in, af->frame->nb_samples);
        if (len2 < 0) {
            std::cout<<"FATAL ERROR: swr_convert() failed!"<<std::endl;
            return -1;
        }
        if (len2 == out_count) {
            std::cout<<"ERROR: Audio buffer probably too small."<<std::endl;
            if (swr_init(videostate->swr_ctx) < 0)
                swr_free(&videostate->swr_ctx);
        }
        videostate->audio_buf = videostate->audio_buf1;
        resampled_data_size = len2 * videostate->audio_tgt.channels * av_get_bytes_per_sample(videostate->audio_tgt.fmt);
    } else {
        videostate->audio_buf = af->frame->data[0];
        resampled_data_size = data_size;
    }

    audio_clock0 = videostate->audio_clock;
    /* update the audio clock with the pts */
    if (!isnan(af->pts))
        videostate->audio_clock = af->pts + (double) af->frame->nb_samples / af->frame->sample_rate;
    else
        videostate->audio_clock = NAN;
    videostate->audio_clock_serial = af->serial;
    
    return resampled_data_size;
}

int Audio::synchronize_audio(VideoState *videostate, int nb_samples)
{
    int wanted_nb_samples = nb_samples;

    /* if not master, then we try to remove or add samples to correct the clock */
    if (Clock::get_master_sync_type(videostate) != AV_SYNC_AUDIO_MASTER) {
        double diff, avg_diff;
        int min_nb_samples, max_nb_samples;

        diff = Clock::get_clock(&videostate->audclk) - Clock::get_master_clock(videostate);

        if (!isnan(diff) && fabs(diff) < AV_NOSYNC_THRESHOLD) {
            videostate->audio_diff_cum = diff + videostate->audio_diff_avg_coef * videostate->audio_diff_cum;
            if (videostate->audio_diff_avg_count < AUDIO_DIFF_AVG_NB) {
                /* not enough measures to have a correct estimate */
                videostate->audio_diff_avg_count++;
            } else {
                /* estimate the A-V difference */
                avg_diff = videostate->audio_diff_cum * (1.0 - videostate->audio_diff_avg_coef);

                if (fabs(avg_diff) >= videostate->audio_diff_threshold) {
                    wanted_nb_samples = nb_samples + (int)(diff * videostate->audio_src.freq);
                    min_nb_samples = ((nb_samples * (100 - SAMPLE_CORRECTION_PERCENT_MAX) / 100));
                    max_nb_samples = ((nb_samples * (100 + SAMPLE_CORRECTION_PERCENT_MAX) / 100));
                    wanted_nb_samples = av_clip(wanted_nb_samples, min_nb_samples, max_nb_samples);
                }
            }
        } else {
            /* too big difference : may be initial PTS errors, so
               reset A-V filter */
            videostate->audio_diff_avg_count = 0;
            videostate->audio_diff_cum       = 0;
        }
    }

    return wanted_nb_samples;
}