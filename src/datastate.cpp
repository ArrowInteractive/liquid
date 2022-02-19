/*
**  Includes
*/

#include "datastate.hpp"

/*
**  Stream functions
*/

VideoState *stream_open(char *filename)
{
    VideoState *videostate;

    videostate = (VideoState *)av_mallocz(sizeof(VideoState));
    if (!videostate)
        return NULL;
    
    AVFormatContext* avformat_ctx = avformat_alloc_context();
    avformat_open_input(&avformat_ctx, filename, NULL, NULL);

    videostate->last_video_stream = videostate->video_stream = -1;
    videostate->last_audio_stream = videostate->audio_stream = -1;
    videostate->last_subtitle_stream = videostate->subtitle_stream = -1;
    videostate->filename = av_strdup(filename);
    if (!videostate->filename)
        goto fail;
    
    videostate->iformat = av_find_input_format(avformat_ctx->iformat->name);
    videostate->ytop    = 0;
    videostate->xleft   = 0;

    /* start video display */
    if (frame_queue_init(&videostate->pictq, &videostate->videoq, VIDEO_PICTURE_QUEUE_SIZE, 1) < 0)
        goto fail;
    if (frame_queue_init(&videostate->subpq, &videostate->subtitleq, SUBPICTURE_QUEUE_SIZE, 0) < 0)
        goto fail;
    if (frame_queue_init(&videostate->sampq, &videostate->audioq, SAMPLE_QUEUE_SIZE, 1) < 0)
        goto fail;

    if (packet_queue_init(&videostate->videoq) < 0 ||
        packet_queue_init(&videostate->audioq) < 0 ||
        packet_queue_init(&videostate->subtitleq) < 0)
        goto fail;

    if (!(videostate->continue_read_thread = SDL_CreateCond())) {
        std::cout<<"FATAL ERROR: SDL_CreateCond() failed!"<<SDL_GetError()<<std::endl;
        goto fail;
    }

    init_clock(&videostate->vidclk, &videostate->videoq.serial);
    init_clock(&videostate->audclk, &videostate->audioq.serial);
    init_clock(&videostate->extclk, &videostate->extclk.serial);
    videostate->audio_clock_serial = -1;
    if (startup_volume < 0)
        std::cout<<"Setting volume to 0."<<std::endl;
    if (startup_volume > 100)
        std::cout<<"Setting volume to 100."<<std::endl;
    startup_volume = av_clip(startup_volume, 0, 100);
    startup_volume = av_clip(SDL_MIX_MAXVOLUME * startup_volume / 100, 0, SDL_MIX_MAXVOLUME);
    videostate->audio_volume = startup_volume;
    videostate->muted = 0;
    videostate->av_sync_type = av_sync_type;
    videostate->read_tid     = SDL_CreateThread(read_thread, "read_thread", videostate);
    if (!videostate->read_tid) {
        std::cout<<"FATAL ERROR: SDL_CreateThread() failed!"<<SDL_GetError()<<std::endl;
fail:
        stream_close(videostate);
        return NULL;
    }
    return videostate;
}

void stream_close(VideoState *videostate)
{
    /* XXX: use a special url_shutdown call to abort parse cleanly */
    videostate->abort_request = 1;
    SDL_WaitThread(videostate->read_tid, NULL);

    /* close each stream */
    if (videostate->audio_stream >= 0)
        stream_component_close(videostate, videostate->audio_stream);
    if (videostate->video_stream >= 0)
        stream_component_close(videostate, videostate->video_stream);
    if (videostate->subtitle_stream >= 0)
        stream_component_close(videostate, videostate->subtitle_stream);

    avformat_close_input(&videostate->ic);

    packet_queue_destroy(&videostate->videoq);
    packet_queue_destroy(&videostate->audioq);
    packet_queue_destroy(&videostate->subtitleq);

    /* free all pictures */
    frame_queue_destroy(&videostate->pictq);
    frame_queue_destroy(&videostate->sampq);
    frame_queue_destroy(&videostate->subpq);
    SDL_DestroyCond(videostate->continue_read_thread);
    sws_freeContext(videostate->img_convert_ctx);
    sws_freeContext(videostate->sub_convert_ctx);
    av_free(videostate->filename);
    if (videostate->vis_texture)
        SDL_DestroyTexture(videostate->vis_texture);
    if (videostate->vid_texture)
        SDL_DestroyTexture(videostate->vid_texture);
    if (videostate->sub_texture)
        SDL_DestroyTexture(videostate->sub_texture);
    av_free(videostate);
}

int stream_component_open(VideoState *videostate, int stream_index)
{
    AVFormatContext *ic = (AVFormatContext *)videostate->ic;
    AVCodecContext *avctx;
    const AVCodec *codec;
    const char *forced_codec_name = NULL;
    AVDictionary *opts = NULL;
    const AVDictionaryEntry *t = NULL;
    int sample_rate, nb_channels;
    int64_t channel_layout;
    int ret = 0;
    int stream_lowres = lowres;

    if (stream_index < 0 || stream_index >= ic->nb_streams)
        return -1;

    avctx = avcodec_alloc_context3(NULL);
    if (!avctx)
        return AVERROR(ENOMEM);

    ret = avcodec_parameters_to_context(avctx, ic->streams[stream_index]->codecpar);
    if (ret < 0)
        goto fail;
    avctx->pkt_timebase = ic->streams[stream_index]->time_base;

    codec = avcodec_find_decoder(avctx->codec_id);

    switch(avctx->codec_type){
        case AVMEDIA_TYPE_AUDIO   : videostate->last_audio_stream    = stream_index; forced_codec_name =    audio_codec_name; break;
        case AVMEDIA_TYPE_SUBTITLE: videostate->last_subtitle_stream = stream_index; forced_codec_name = subtitle_codec_name; break;
        case AVMEDIA_TYPE_VIDEO   : videostate->last_video_stream    = stream_index; forced_codec_name =    video_codec_name; break;
    }
    if (forced_codec_name)
        codec = avcodec_find_decoder_by_name(forced_codec_name);
    if (!codec) {
        if (forced_codec_name)
            std::cout<<"ERROR: No codec found: "<<forced_codec_name<<std::endl;
        else                   
            std::cout<<"ERROR: No decoder could be found for codec: "<<avcodec_get_name(avctx->codec_id)<<std::endl;
        ret = AVERROR(EINVAL);
        goto fail;
    }

    avctx->codec_id = codec->id;
    if (stream_lowres > codec->max_lowres) {
        std::cout<<"ERROR: The maximum value for lowres supported by the decoder is "<<codec->max_lowres<<std::endl;
        stream_lowres = codec->max_lowres;
    }
    avctx->lowres = stream_lowres;

    if (fast)
        avctx->flags2 |= AV_CODEC_FLAG2_FAST;

    if (!av_dict_get(opts, "threads", NULL, 0))
        av_dict_set(&opts, "threads", "auto", 0);
    if (stream_lowres)
        av_dict_set_int(&opts, "lowres", stream_lowres, 0);
    if ((ret = avcodec_open2(avctx, codec, &opts)) < 0) {
        goto fail;
    }
    if ((t = av_dict_get(opts, "", NULL, AV_DICT_IGNORE_SUFFIX))) {
        std::cout<<"ERROR: Option for found "<<t->key<<std::endl;
        ret =  AVERROR_OPTION_NOT_FOUND;
        goto fail;
    }

    videostate->eof = 0;
    ic->streams[stream_index]->discard = AVDISCARD_DEFAULT;
    switch (avctx->codec_type) {
    case AVMEDIA_TYPE_AUDIO:
        sample_rate    = avctx->sample_rate;
        nb_channels    = avctx->channels;
        channel_layout = avctx->channel_layout;
        /* prepare audio output */
        if ((ret = audio_open(videostate, channel_layout, nb_channels, sample_rate, &videostate->audio_tgt)) < 0)
            goto fail;
        videostate->audio_hw_buf_size = ret;
        videostate->audio_src = videostate->audio_tgt;
        videostate->audio_buf_size  = 0;
        videostate->audio_buf_index = 0;

        /* init averaging filter */
        videostate->audio_diff_avg_coef  = exp(log(0.01) / AUDIO_DIFF_AVG_NB);
        videostate->audio_diff_avg_count = 0;
        /* since we do not have a precise anough audio FIFO fullness,
           we correct audio sync only if larger than this threshold */
        videostate->audio_diff_threshold = (double)(videostate->audio_hw_buf_size) / videostate->audio_tgt.bytes_per_sec;

        videostate->audio_stream = stream_index;
        videostate->audio_st = ic->streams[stream_index];

        if ((ret = decoder_init(&videostate->auddec, avctx, &videostate->audioq, videostate->continue_read_thread)) < 0)
            goto fail;
        if ((videostate->ic->iformat->flags & (AVFMT_NOBINSEARCH | AVFMT_NOGENSEARCH | AVFMT_NO_BYTE_SEEK)) && !videostate->ic->iformat->read_seek) {
            videostate->auddec.start_pts = videostate->audio_st->start_time;
            videostate->auddec.start_pts_tb = videostate->audio_st->time_base;
        }
        if ((ret = decoder_start(&videostate->auddec, audio_thread, "audio_decoder", videostate)) < 0)
            goto out;
        SDL_PauseAudioDevice(audio_dev, 0);
        break;
    case AVMEDIA_TYPE_VIDEO:
        videostate->video_stream = stream_index;
        videostate->video_st = ic->streams[stream_index];

        if ((ret = decoder_init(&videostate->viddec, avctx, &videostate->videoq, videostate->continue_read_thread)) < 0)
            goto fail;
        if ((ret = decoder_start(&videostate->viddec, video_thread, "video_decoder", videostate)) < 0)
            goto out;
        videostate->queue_attachments_req = 1;
        break;
    case AVMEDIA_TYPE_SUBTITLE:
        videostate->subtitle_stream = stream_index;
        videostate->subtitle_st = ic->streams[stream_index];

        if ((ret = decoder_init(&videostate->subdec, avctx, &videostate->subtitleq, videostate->continue_read_thread)) < 0)
            goto fail;
        
        if ((ret = decoder_start(&videostate->subdec, subtitle_thread, "subtitle_decoder", videostate)) < 0)
            goto out;

        break;
    default:
        break;
    }
    goto out;

fail:
    avcodec_free_context(&avctx);
out:
    av_dict_free(&opts);

    return ret;
}

void stream_component_close(VideoState *videostate, int stream_index)
{
    AVFormatContext *ic = videostate->ic;
    AVCodecParameters *codecpar;

    if (stream_index < 0 || stream_index >= ic->nb_streams)
        return;
    codecpar = ic->streams[stream_index]->codecpar;

    switch (codecpar->codec_type) {
    case AVMEDIA_TYPE_AUDIO:
        decoder_abort(&videostate->auddec, &videostate->sampq);
        SDL_CloseAudioDevice(audio_dev);
        decoder_destroy(&videostate->auddec);
        swr_free(&videostate->swr_ctx);
        av_freep(&videostate->audio_buf1);
        videostate->audio_buf1_size = 0;
        videostate->audio_buf = NULL;

        if (videostate->rdft) {
            av_rdft_end(videostate->rdft);
            av_freep(&videostate->rdft_data);
            videostate->rdft = NULL;
            videostate->rdft_bits = 0;
        }
        break;
    case AVMEDIA_TYPE_VIDEO:
        decoder_abort(&videostate->viddec, &videostate->pictq);
        decoder_destroy(&videostate->viddec);
        break;
    case AVMEDIA_TYPE_SUBTITLE:
        decoder_abort(&videostate->subdec, &videostate->subpq);
        decoder_destroy(&videostate->subdec);
        break;
    default:
        break;
    }

    ic->streams[stream_index]->discard = AVDISCARD_ALL;
    switch (codecpar->codec_type) {
    case AVMEDIA_TYPE_AUDIO:
        videostate->audio_st = NULL;
        videostate->audio_stream = -1;
        break;
    case AVMEDIA_TYPE_VIDEO:
        videostate->video_st = NULL;
        videostate->video_stream = -1;
        break;
    case AVMEDIA_TYPE_SUBTITLE:
        videostate->subtitle_st = NULL;
        videostate->subtitle_stream = -1;
        break;
    default:
        break;
    }
}

int stream_has_enough_packets(AVStream *st, int stream_id, PacketQueue *queue) {
    return stream_id < 0 ||
           queue->abort_request ||
           (st->disposition & AV_DISPOSITION_ATTACHED_PIC) ||
           queue->nb_packets > MIN_FRAMES && (!queue->duration || av_q2d(st->time_base) * queue->duration > 1.0);
}

/*
**  Clock functions
*/

void init_clock(Clock *c, int *queue_serial)
{
    c->speed = 1.0;
    c->paused = 0;
    c->queue_serial = queue_serial;
    set_clock(c, NAN, -1);
}

void set_clock(Clock *c, double pts, int serial)
{
    double time = av_gettime_relative() / 1000000.0;
    set_clock_at(c, pts, serial, time);
}

void set_clock_at(Clock *c, double pts, int serial, double time)
{
    c->pts = pts;
    c->last_updated = time;
    c->pts_drift = c->pts - time;
    c->serial = serial;
}

double get_clock(Clock *c)
{
    if (*c->queue_serial != c->serial)
        return NAN;
    if (c->paused) {
        return c->pts;
    } else {
        double time = av_gettime_relative() / 1000000.0;
        return c->pts_drift + time - (time - c->last_updated) * (1.0 - c->speed);
    }
}

int is_realtime(AVFormatContext *s)
{
    if(   !strcmp(s->iformat->name, "rtp")
       || !strcmp(s->iformat->name, "rtsp")
       || !strcmp(s->iformat->name, "sdp")
    )
        return 1;

    if(s->pb && (   !strncmp(s->url, "rtp:", 4)
                 || !strncmp(s->url, "udp:", 4)
                )
    )
        return 1;
    return 0;
}

void sync_clock_to_slave(Clock *c, Clock *slave)
{
    double clock = get_clock(c);
    double slave_clock = get_clock(slave);
    if (!isnan(slave_clock) && (isnan(clock) || fabs(clock - slave_clock) > AV_NOSYNC_THRESHOLD))
        set_clock(c, slave_clock, slave->serial);
}

int get_master_sync_type(VideoState *videostate) 
{
    if (videostate->av_sync_type == AV_SYNC_VIDEO_MASTER){
        if (videostate->video_st)
            return AV_SYNC_VIDEO_MASTER;
        else
            return AV_SYNC_AUDIO_MASTER;
    } else if (videostate->av_sync_type == AV_SYNC_AUDIO_MASTER){
        if (videostate->audio_st)
            return AV_SYNC_AUDIO_MASTER;
        else
            return AV_SYNC_EXTERNAL_CLOCK;
    } else {
        return AV_SYNC_EXTERNAL_CLOCK;
    }
}

double get_master_clock(VideoState *videostate)
{
    double val;

    switch (get_master_sync_type(videostate)){
        case AV_SYNC_VIDEO_MASTER:
            val = get_clock(&videostate->vidclk);
            break;
        case AV_SYNC_AUDIO_MASTER:
            val = get_clock(&videostate->audclk);
            break;
        default:
            val = get_clock(&videostate->extclk);
            break;
    }
    return val;
}

void check_external_clock_speed(VideoState *videostate) 
{
   if (videostate->video_stream >= 0 && videostate->videoq.nb_packets <= EXTERNAL_CLOCK_MIN_FRAMES ||
       videostate->audio_stream >= 0 && videostate->audioq.nb_packets <= EXTERNAL_CLOCK_MIN_FRAMES) {
       set_clock_speed(&videostate->extclk, FFMAX(EXTERNAL_CLOCK_SPEED_MIN, videostate->extclk.speed - EXTERNAL_CLOCK_SPEED_STEP));
   } else if ((videostate->video_stream < 0 || videostate->videoq.nb_packets > EXTERNAL_CLOCK_MAX_FRAMES) &&
              (videostate->audio_stream < 0 || videostate->audioq.nb_packets > EXTERNAL_CLOCK_MAX_FRAMES)) {
       set_clock_speed(&videostate->extclk, FFMIN(EXTERNAL_CLOCK_SPEED_MAX, videostate->extclk.speed + EXTERNAL_CLOCK_SPEED_STEP));
   } else {
       double speed = videostate->extclk.speed;
       if (speed != 1.0)
           set_clock_speed(&videostate->extclk, speed + EXTERNAL_CLOCK_SPEED_STEP * (1.0 - speed) / fabs(1.0 - speed));
   }
}

void set_clock_speed(Clock *c, double speed)
{
    set_clock(c, get_clock(c), c->serial);
    c->speed = speed;
}

double vp_duration(VideoState *videostate, Frame *vp, Frame *nextvp) {
    if (vp->serial == nextvp->serial) {
        double duration = nextvp->pts - vp->pts;
        if (isnan(duration) || duration <= 0 || duration > videostate->max_frame_duration)
            return vp->duration;
        else
            return duration;
    } else {
        return 0.0;
    }
}

double compute_target_delay(double delay, VideoState *videostate)
{
    double sync_threshold, diff = 0;

    /* update delay to follow master synchronisation source */
    if (get_master_sync_type(videostate) != AV_SYNC_VIDEO_MASTER) {
        /* if video is slave, we try to correct big delays by
           duplicating or deleting a frame */
        diff = get_clock(&videostate->vidclk) - get_master_clock(videostate);

        /* skip or repeat frame. We take into account the
           delay to compute the threshold. I still don't know
           if it is the best guess */
        sync_threshold = FFMAX(AV_SYNC_THRESHOLD_MIN, FFMIN(AV_SYNC_THRESHOLD_MAX, delay));
        if (!isnan(diff) && fabs(diff) < videostate->max_frame_duration) {
            if (diff <= -sync_threshold)
                delay = FFMAX(0, delay + diff);
            else if (diff >= sync_threshold && delay > AV_SYNC_FRAMEDUP_THRESHOLD)
                delay = delay + diff;
            else if (diff >= sync_threshold)
                delay = 2 * delay;
        }
    }
    return delay;
}

/*
**  Decoder functions
*/

int decoder_init(Decoder *d, AVCodecContext *avctx, PacketQueue *queue, SDL_cond *empty_queue_cond)
{
    memset(d, 0, sizeof(Decoder));
    d->pkt = av_packet_alloc();
    if (!d->pkt)
        return AVERROR(ENOMEM);
    d->avctx = avctx;
    d->queue = queue;
    d->empty_queue_cond = empty_queue_cond;
    d->start_pts = AV_NOPTS_VALUE;
    d->pkt_serial = -1;
    return 0;
}

int decoder_start(Decoder *d, int (*fn)(void *), const char *thread_name, void* arg)
{
    packet_queue_start(d->queue);
    d->decoder_tid = SDL_CreateThread(fn, thread_name, arg);
    if (!d->decoder_tid) {
        std::cout<<"FATAL ERROR: SDL_CreateThread() failed!"<<SDL_GetError()<<std::endl;
        return AVERROR(ENOMEM);
    }
    return 0;
}

void decoder_abort(Decoder *d, FrameQueue *fq)
{
    packet_queue_abort(d->queue);
    frame_queue_signal(fq);
    SDL_WaitThread(d->decoder_tid, NULL);
    d->decoder_tid = NULL;
    packet_queue_flush(d->queue);
}

void decoder_destroy(Decoder *d) 
{
    av_packet_free(&d->pkt);
    avcodec_free_context(&d->avctx);
}

/*
**  Thread functions
*/

int read_thread(void *arg)
{
    VideoState *videostate = (VideoState *)arg;
    AVFormatContext *ic = NULL;
    int err, i, ret;
    int st_index[AVMEDIA_TYPE_NB];
    AVPacket *pkt = NULL;
    int64_t stream_start_time;
    int pkt_in_play_range = 0;
    const AVDictionaryEntry *t;
    SDL_mutex *wait_mutex = SDL_CreateMutex();
    int scan_all_pmts_set = 0;
    int64_t pkt_ts;

    if (!wait_mutex) {
        std::cout<<"FATAL ERROR: SDL_CreateMutex() failed!"<<SDL_GetError()<<std::endl;
        ret = AVERROR(ENOMEM);
        goto fail;
    }

    memset(st_index, -1, sizeof(st_index));
    videostate->eof = 0;

    pkt = av_packet_alloc();
    if (!pkt) {
        std::cout<<"FATAL ERROR: Could not allocate packet!"<<std::endl;
        ret = AVERROR(ENOMEM);
        goto fail;
    }
    ic = avformat_alloc_context();
    if (!ic) {
        std::cout<<"FATAL ERROR: Could not allocate context!"<<std::endl;
        ret = AVERROR(ENOMEM);
        goto fail;
    }
    ic->interrupt_callback.callback = decode_interrupt_cb;
    ic->interrupt_callback.opaque = videostate;
    // Compilation fails if FFmpeg < 5 here if we dont do casting
    err = avformat_open_input(&ic, videostate->filename, (AVInputFormat *)videostate->iformat, NULL);
    if (err < 0) {
        ret = -1;
        goto fail;
    }

    videostate->ic = ic;

    if (genpts)
        ic->flags |= AVFMT_FLAG_GENPTS;

    av_format_inject_global_side_data(ic);

    if (find_stream_info){
        int orig_nb_streams = ic->nb_streams;

        err = avformat_find_stream_info(ic, NULL);

        if (err < 0) {
            std::cout<<"FATAL ERROR: Could not find codec parameters: "<<videostate->filename<<std::endl;
            ret = -1;
            goto fail;
        }
    }

    if (ic->pb)
        ic->pb->eof_reached = 0; // FIXME hack, ffplay maybe should not use avio_feof() to test for the end

    if (seek_by_bytes < 0)
        seek_by_bytes = !!(ic->iformat->flags & AVFMT_TS_DISCONT) && strcmp("ogg", ic->iformat->name);

    videostate->max_frame_duration = (ic->iformat->flags & AVFMT_TS_DISCONT) ? 10.0 : 3600.0;

    if (!window_title && (t = av_dict_get(ic->metadata, "title", NULL, 0)))
        window_title = av_asprintf("%s - %s", t->value, input_filename);

    /* if seeking requested, we execute it */
    if (start_time != AV_NOPTS_VALUE) {
        int64_t timestamp;

        timestamp = start_time;
        /* add the stream start time */
        if (ic->start_time != AV_NOPTS_VALUE)
            timestamp += ic->start_time;
        ret = avformat_seek_file(ic, -1, INT64_MIN, timestamp, INT64_MAX, 0);
        if (ret < 0) {
            std::cout<<"ERROR: Could not seek to position!"<<std::endl;
        }
    }

    videostate->realtime = is_realtime(ic);

    if (show_status)
        av_dump_format(ic, 0, videostate->filename, 0);

    for (i = 0; i < ic->nb_streams; i++) {
        AVStream *st = ic->streams[i];
        enum AVMediaType type = st->codecpar->codec_type;
        st->discard = AVDISCARD_ALL;
        if (type >= 0 && wanted_stream_spec[type] && st_index[type] == -1)
            if (avformat_match_stream_specifier(ic, st, wanted_stream_spec[type]) > 0)
                st_index[type] = i;
    }
    for (i = 0; i < AVMEDIA_TYPE_NB; i++) {
        if (wanted_stream_spec[i] && st_index[i] == -1) {
            std::cout<<"ERROR: Stream specifier does not match any stream!"<<std::endl;
            st_index[i] = INT_MAX;
        }
    }

    if (!video_disable)
        st_index[AVMEDIA_TYPE_VIDEO] =
            av_find_best_stream(ic, AVMEDIA_TYPE_VIDEO,
                                st_index[AVMEDIA_TYPE_VIDEO], -1, NULL, 0);
    if (!audio_disable)
        st_index[AVMEDIA_TYPE_AUDIO] =
            av_find_best_stream(ic, AVMEDIA_TYPE_AUDIO,
                                st_index[AVMEDIA_TYPE_AUDIO],
                                st_index[AVMEDIA_TYPE_VIDEO],
                                NULL, 0);
    if (!video_disable && !subtitle_disable)
        st_index[AVMEDIA_TYPE_SUBTITLE] =
            av_find_best_stream(ic, AVMEDIA_TYPE_SUBTITLE,
                                st_index[AVMEDIA_TYPE_SUBTITLE],
                                (st_index[AVMEDIA_TYPE_AUDIO] >= 0 ?
                                 st_index[AVMEDIA_TYPE_AUDIO] :
                                 st_index[AVMEDIA_TYPE_VIDEO]),
                                NULL, 0);

    videostate->show_mode = show_mode;
    if (st_index[AVMEDIA_TYPE_VIDEO] >= 0) {
        AVStream *st = ic->streams[st_index[AVMEDIA_TYPE_VIDEO]];
        AVCodecParameters *codecpar = st->codecpar;
        AVRational sar = av_guess_sample_aspect_ratio(ic, st, NULL);
        if (codecpar->width)
            set_default_window_size(codecpar->width, codecpar->height, sar);
    }

    /* open the streams */
    if (st_index[AVMEDIA_TYPE_AUDIO] >= 0) {
        stream_component_open(videostate, st_index[AVMEDIA_TYPE_AUDIO]);
    }

    ret = -1;
    if (st_index[AVMEDIA_TYPE_VIDEO] >= 0) {
        ret = stream_component_open(videostate, st_index[AVMEDIA_TYPE_VIDEO]);
    }
    if (videostate->show_mode == SHOW_MODE_NONE)
        videostate->show_mode = ret >= 0 ? SHOW_MODE_VIDEO : SHOW_MODE_RDFT;

    if (st_index[AVMEDIA_TYPE_SUBTITLE] >= 0) {
        stream_component_open(videostate, st_index[AVMEDIA_TYPE_SUBTITLE]);
    }

    if (videostate->video_stream < 0 && videostate->audio_stream < 0) {
        std::cout<<"FATAL ERROR: Failed to open file: "<<videostate->filename<<std::endl;
        ret = -1;
        goto fail;
    }

    if (infinite_buffer < 0 && videostate->realtime)
        infinite_buffer = 1;

    for (;;) {
        if (videostate->abort_request)
            break;
        if (videostate->paused != videostate->last_paused) {
            videostate->last_paused = videostate->paused;
            if (videostate->paused)
                videostate->read_pause_return = av_read_pause(ic);
            else
                av_read_play(ic);
        }

        if (videostate->seek_req) {
            int64_t seek_target = videostate->seek_pos;
            int64_t seek_min    = videostate->seek_rel > 0 ? seek_target - videostate->seek_rel + 2: INT64_MIN;
            int64_t seek_max    = videostate->seek_rel < 0 ? seek_target - videostate->seek_rel - 2: INT64_MAX;
            // FIXME the +-2 videostate due to rounding being not done in the correct direction in generation
            // of the seek_pos/seek_rel variables

            ret = avformat_seek_file(videostate->ic, -1, seek_min, seek_target, seek_max, videostate->seek_flags);
            if (ret < 0) {
                std::cout<<"ERROR: Could not seek: "<<videostate->ic->url<<std::endl;
            } else {
                if (videostate->audio_stream >= 0)
                    packet_queue_flush(&videostate->audioq);
                if (videostate->subtitle_stream >= 0)
                    packet_queue_flush(&videostate->subtitleq);
                if (videostate->video_stream >= 0)
                    packet_queue_flush(&videostate->videoq);
                if (videostate->seek_flags & AVSEEK_FLAG_BYTE) {
                   set_clock(&videostate->extclk, NAN, 0);
                } else {
                   set_clock(&videostate->extclk, seek_target / (double)AV_TIME_BASE, 0);
                }
            }
            videostate->seek_req = 0;
            videostate->queue_attachments_req = 1;
            videostate->eof = 0;
            if (videostate->paused)
                step_to_next_frame(videostate);
        }
        if (videostate->queue_attachments_req) {
            if (videostate->video_st && videostate->video_st->disposition & AV_DISPOSITION_ATTACHED_PIC) {
                if ((ret = av_packet_ref(pkt, &videostate->video_st->attached_pic)) < 0)
                    goto fail;
                packet_queue_put(&videostate->videoq, pkt);
                packet_queue_put_nullpacket(&videostate->videoq, pkt, videostate->video_stream);
            }
            videostate->queue_attachments_req = 0;
        }

        /* if the queue are full, no need to read more */
        if (infinite_buffer<1 &&
              (videostate->audioq.size + videostate->videoq.size + videostate->subtitleq.size > MAX_QUEUE_SIZE
            || (stream_has_enough_packets(videostate->audio_st, videostate->audio_stream, &videostate->audioq) &&
                stream_has_enough_packets(videostate->video_st, videostate->video_stream, &videostate->videoq) &&
                stream_has_enough_packets(videostate->subtitle_st, videostate->subtitle_stream, &videostate->subtitleq)))) {
            /* wait 10 ms */
            SDL_LockMutex(wait_mutex);
            SDL_CondWaitTimeout(videostate->continue_read_thread, wait_mutex, 10);
            SDL_UnlockMutex(wait_mutex);
            continue;
        }
        if (!videostate->paused &&
            (!videostate->audio_st || (videostate->auddec.finished == videostate->audioq.serial && frame_queue_nb_remaining(&videostate->sampq) == 0)) &&
            (!videostate->video_st || (videostate->viddec.finished == videostate->videoq.serial && frame_queue_nb_remaining(&videostate->pictq) == 0))) {
            if (loop != 1 && (!loop || --loop)) {
                stream_seek(videostate, start_time != AV_NOPTS_VALUE ? start_time : 0, 0, 0);
            } else if (autoexit) {
                ret = AVERROR_EOF;
                goto fail;
            }
        }
        ret = av_read_frame(ic, pkt);
        if (ret < 0) {
            if ((ret == AVERROR_EOF || avio_feof(ic->pb)) && !videostate->eof) {
                if (videostate->video_stream >= 0)
                    packet_queue_put_nullpacket(&videostate->videoq, pkt, videostate->video_stream);
                if (videostate->audio_stream >= 0)
                    packet_queue_put_nullpacket(&videostate->audioq, pkt, videostate->audio_stream);
                if (videostate->subtitle_stream >= 0)
                    packet_queue_put_nullpacket(&videostate->subtitleq, pkt, videostate->subtitle_stream);
                videostate->eof = 1;
            }
            if (ic->pb && ic->pb->error) {
                if (autoexit)
                    goto fail;
                else
                    break;
            }
            SDL_LockMutex(wait_mutex);
            SDL_CondWaitTimeout(videostate->continue_read_thread, wait_mutex, 10);
            SDL_UnlockMutex(wait_mutex);
            continue;
        } else {
            videostate->eof = 0;
        }
        /* check if packet videostate in play range specified by user, then queue, otherwise discard */
        stream_start_time = ic->streams[pkt->stream_index]->start_time;
        pkt_ts = pkt->pts == AV_NOPTS_VALUE ? pkt->dts : pkt->pts;
        pkt_in_play_range = duration == AV_NOPTS_VALUE ||
                (pkt_ts - (stream_start_time != AV_NOPTS_VALUE ? stream_start_time : 0)) *
                av_q2d(ic->streams[pkt->stream_index]->time_base) -
                (double)(start_time != AV_NOPTS_VALUE ? start_time : 0) / 1000000
                <= ((double)duration / 1000000);
        if (pkt->stream_index == videostate->audio_stream && pkt_in_play_range) {
            packet_queue_put(&videostate->audioq, pkt);
        } else if (pkt->stream_index == videostate->video_stream && pkt_in_play_range
                   && !(videostate->video_st->disposition & AV_DISPOSITION_ATTACHED_PIC)) {
            packet_queue_put(&videostate->videoq, pkt);
        } else if (pkt->stream_index == videostate->subtitle_stream && pkt_in_play_range) {
            packet_queue_put(&videostate->subtitleq, pkt);
        } else {
            av_packet_unref(pkt);
        }
    }

    ret = 0;
 fail:
    if (ic && !videostate->ic)
        avformat_close_input(&ic);

    av_packet_free(&pkt);
    if (ret != 0) {
        SDL_Event event;

        event.type = FF_QUIT_EVENT;
        event.user.data1 = videostate;
        SDL_PushEvent(&event);
    }
    SDL_DestroyMutex(wait_mutex);
    return 0;
}

int video_thread(void *arg)
{
    VideoState *videostate = (VideoState *)arg;
    AVFrame *frame = av_frame_alloc();
    double pts;
    double duration;
    int ret;
    AVRational tb = videostate->video_st->time_base;
    AVRational frame_rate = av_guess_frame_rate(videostate->ic, videostate->video_st, NULL);

    if (!frame)
        return AVERROR(ENOMEM);

    for (;;) {
        ret = get_video_frame(videostate, frame);
        if (ret < 0)
            goto the_end;
        if (!ret)
            continue;

        duration = (frame_rate.num && frame_rate.den ? av_q2d((AVRational){frame_rate.den, frame_rate.num}) : 0);
        pts = (frame->pts == AV_NOPTS_VALUE) ? NAN : frame->pts * av_q2d(tb);
        ret = queue_picture(videostate, frame, pts, duration, frame->pkt_pos, videostate->viddec.pkt_serial);
        av_frame_unref(frame);

        if (ret < 0)
            goto the_end;
    }
 the_end:
    av_frame_free(&frame);
    return 0;
}

int audio_thread(void *arg)
{
    VideoState *videostate = (VideoState *)arg;
    AVFrame *frame = av_frame_alloc();
    Frame *af;
    int got_frame = 0;
    AVRational tb;
    int ret = 0;

    if (!frame)
        return AVERROR(ENOMEM);

    do{
        if ((got_frame = decoder_decode_frame(&videostate->auddec, frame, NULL)) < 0)
            goto the_end;

        if (got_frame) {
            tb = (AVRational){1, frame->sample_rate};
            
            if (!(af = frame_queue_peek_writable(&videostate->sampq)))
                goto the_end;

            af->pts = (frame->pts == AV_NOPTS_VALUE) ? NAN : frame->pts * av_q2d(tb);
            af->pos = frame->pkt_pos;
            af->serial = videostate->auddec.pkt_serial;
            af->duration = av_q2d((AVRational){frame->nb_samples, frame->sample_rate});

            av_frame_move_ref(af->frame, frame);
            frame_queue_push(&videostate->sampq);
        }
    }while (ret >= 0 || ret == AVERROR(EAGAIN) || ret == AVERROR_EOF);
    
the_end:
    av_frame_free(&frame);
    return ret;
}

int subtitle_thread(void *arg)
{
    VideoState *videostate = (VideoState *)arg;
    Frame *sp;
    int got_subtitle;
    double pts;

    for (;;) {
        if (!(sp = frame_queue_peek_writable(&videostate->subpq)))
            return 0;

        if ((got_subtitle = decoder_decode_frame(&videostate->subdec, NULL, &sp->sub)) < 0)
            break;

        pts = 0;

        if (got_subtitle && sp->sub.format == 0) {
            if (sp->sub.pts != AV_NOPTS_VALUE)
                pts = sp->sub.pts / (double)AV_TIME_BASE;
            sp->pts = pts;
            sp->serial = videostate->subdec.pkt_serial;
            sp->width = videostate->subdec.avctx->width;
            sp->height = videostate->subdec.avctx->height;
            sp->uploaded = 0;

            /* now we can update the picture count */
            frame_queue_push(&videostate->subpq);
        } else if (got_subtitle) {
            avsubtitle_free(&sp->sub);
        }
    }
    return 0;
}

/*
**  Decode functions
*/

int decode_interrupt_cb(void *ctx)
{
    VideoState *videostate = (VideoState *)ctx;
    return videostate->abort_request;
}

int decoder_decode_frame(Decoder *d, AVFrame *frame, AVSubtitle *sub) {
    int ret = AVERROR(EAGAIN);

    for (;;) {
        if (d->queue->serial == d->pkt_serial) {
            do {
                if (d->queue->abort_request)
                    return -1;

                switch (d->avctx->codec_type) {
                    case AVMEDIA_TYPE_VIDEO:
                        ret = avcodec_receive_frame(d->avctx, frame);
                        if (ret >= 0) {
                            if (decoder_reorder_pts == -1) {
                                frame->pts = frame->best_effort_timestamp;
                            } else if (!decoder_reorder_pts) {
                                frame->pts = frame->pkt_dts;
                            }
                        }
                        break;
                    case AVMEDIA_TYPE_AUDIO:
                        ret = avcodec_receive_frame(d->avctx, frame);
                        if (ret >= 0) {
                            AVRational tb = (AVRational){1, frame->sample_rate};
                            if (frame->pts != AV_NOPTS_VALUE)
                                frame->pts = av_rescale_q(frame->pts, d->avctx->pkt_timebase, tb);
                            else if (d->next_pts != AV_NOPTS_VALUE)
                                frame->pts = av_rescale_q(d->next_pts, d->next_pts_tb, tb);
                            if (frame->pts != AV_NOPTS_VALUE) {
                                d->next_pts = frame->pts + frame->nb_samples;
                                d->next_pts_tb = tb;
                            }
                        }
                        break;
                }
                if (ret == AVERROR_EOF) {
                    d->finished = d->pkt_serial;
                    avcodec_flush_buffers(d->avctx);
                    return 0;
                }
                if (ret >= 0)
                    return 1;
            } while (ret != AVERROR(EAGAIN));
        }

        do {
            if (d->queue->nb_packets == 0)
                SDL_CondSignal(d->empty_queue_cond);
            if (d->packet_pending) {
                d->packet_pending = 0;
            } else {
                int old_serial = d->pkt_serial;
                if (packet_queue_get(d->queue, d->pkt, 1, &d->pkt_serial) < 0)
                    return -1;
                if (old_serial != d->pkt_serial) {
                    avcodec_flush_buffers(d->avctx);
                    d->finished = 0;
                    d->next_pts = d->start_pts;
                    d->next_pts_tb = d->start_pts_tb;
                }
            }
            if (d->queue->serial == d->pkt_serial)
                break;
            av_packet_unref(d->pkt);
        } while (1);

        if (d->avctx->codec_type == AVMEDIA_TYPE_SUBTITLE) {
            int got_frame = 0;
            ret = avcodec_decode_subtitle2(d->avctx, sub, &got_frame, d->pkt);
            if (ret < 0) {
                ret = AVERROR(EAGAIN);
            } else {
                if (got_frame && !d->pkt->data) {
                    d->packet_pending = 1;
                }
                ret = got_frame ? 0 : (d->pkt->data ? AVERROR(EAGAIN) : AVERROR_EOF);
            }
            av_packet_unref(d->pkt);
        } else {
            if (avcodec_send_packet(d->avctx, d->pkt) == AVERROR(EAGAIN)) {
                std::cout<<"ERROR: Receive_frame and send_packet both returned EAGAIN, which is an API violation."<<std::endl;
                d->packet_pending = 1;
            } else {
                av_packet_unref(d->pkt);
            }
        }
    }
}

/*
**  Window functions
*/

int create_window(){
    window = SDL_CreateWindow(
        "Liquid Media Player", 
        SDL_WINDOWPOS_UNDEFINED, 
        SDL_WINDOWPOS_UNDEFINED, 
        default_width, default_height,
         SDL_WINDOW_OPENGL | 
         SDL_WINDOW_ALLOW_HIGHDPI |
         SDL_WINDOW_RESIZABLE
    );
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");
    renderer = SDL_CreateRenderer(
        window, 
        -1, 
        SDL_RENDERER_ACCELERATED 
        | SDL_RENDERER_PRESENTVSYNC
    );
    if (!window || !renderer){
        return -1; 
    }
    init_imgui(window,renderer);
    
    return 0;
}

void set_default_window_size(int width, int height, AVRational sar)
{
    SDL_Rect rect;
    int max_width  = screen_width  ? screen_width  : INT_MAX;
    int max_height = screen_height ? screen_height : INT_MAX;
    if (max_width == INT_MAX && max_height == INT_MAX)
        max_height = height;
    calculate_display_rect(&rect, 0, 0, max_width, max_height, width, height, sar);
    default_width  = rect.w;
    default_height = rect.h;
}

void calculate_display_rect(
    SDL_Rect *rect,
    int scr_xleft, int scr_ytop, int scr_width, int scr_height,
    int pic_width, int pic_height, AVRational pic_sar
)
{
    AVRational aspect_ratio = pic_sar;
    int64_t width, height, x, y;

    if (av_cmp_q(aspect_ratio, av_make_q(0, 1)) <= 0)
        aspect_ratio = av_make_q(1, 1);

    aspect_ratio = av_mul_q(aspect_ratio, av_make_q(pic_width, pic_height));

    /* XXX: we suppose the screen has a 1.0 pixel ratio */
    height = scr_height;
    width = av_rescale(height, aspect_ratio.num, aspect_ratio.den) & ~1;
    if (width > scr_width) {
        width = scr_width;
        height = av_rescale(width, aspect_ratio.den, aspect_ratio.num) & ~1;
    }
    x = (scr_width - width) / 2;
    y = (scr_height - height) / 2;
    rect->x = scr_xleft + x;
    rect->y = scr_ytop  + y;
    rect->w = FFMAX((int)width,  1);
    rect->h = FFMAX((int)height, 1);
}

void update_sample_display(VideoState *videostate, short *samples, int samples_size)
{
    int size, len;

    size = samples_size / sizeof(short);
    while (size > 0) {
        len = SAMPLE_ARRAY_SIZE - videostate->sample_array_index;
        if (len > size)
            len = size;
        memcpy(videostate->sample_array + videostate->sample_array_index, samples, len * sizeof(short));
        samples += len;
        videostate->sample_array_index += len;
        if (videostate->sample_array_index >= SAMPLE_ARRAY_SIZE)
            videostate->sample_array_index = 0;
        size -= len;
    }
}

/*
**  Video functions
*/

int get_video_frame(VideoState *videostate, AVFrame *frame)
{
    int got_picture;

    if ((got_picture = decoder_decode_frame(&videostate->viddec, frame, NULL)) < 0)
        return -1;

    if (got_picture) {
        double dpts = NAN;

        if (frame->pts != AV_NOPTS_VALUE)
            dpts = av_q2d(videostate->video_st->time_base) * frame->pts;

        frame->sample_aspect_ratio = av_guess_sample_aspect_ratio(videostate->ic, videostate->video_st, frame);

        if (framedrop>0 || (framedrop && get_master_sync_type(videostate) != AV_SYNC_VIDEO_MASTER)) {
            if (frame->pts != AV_NOPTS_VALUE) {
                double diff = dpts - get_master_clock(videostate);
                if (!isnan(diff) && fabs(diff) < AV_NOSYNC_THRESHOLD &&
                    diff - videostate->frame_last_filter_delay < 0 &&
                    videostate->viddec.pkt_serial == videostate->vidclk.serial &&
                    videostate->videoq.nb_packets) {
                    videostate->frame_drops_early++;
                    av_frame_unref(frame);
                    got_picture = 0;
                }
            }
        }
    }
    
    return got_picture;
}

int queue_picture(VideoState *videostate, AVFrame *src_frame, double pts, double duration, int64_t pos, int serial)
{
    Frame *vp;
    if (!(vp = frame_queue_peek_writable(&videostate->pictq)))
        return -1;

    vp->sar = src_frame->sample_aspect_ratio;
    vp->uploaded = 0;

    vp->width = src_frame->width;
    vp->height = src_frame->height;
    vp->format = src_frame->format;

    vp->pts = pts;
    vp->duration = duration;
    vp->pos = pos;
    vp->serial = serial;

    set_default_window_size(vp->width, vp->height, vp->sar);

    av_frame_move_ref(vp->frame, src_frame);
    frame_queue_push(&videostate->pictq);
    return 0;
}

void video_refresh(void *arg, double *remaining_time)
{
    VideoState *videostate = (VideoState *)arg;
    double time;

    Frame *sp, *sp2;

    if (!videostate->paused && get_master_sync_type(videostate) == AV_SYNC_EXTERNAL_CLOCK && videostate->realtime)
        check_external_clock_speed(videostate);

    if (!display_disable && videostate->show_mode != SHOW_MODE_VIDEO && videostate->audio_st) {
        time = av_gettime_relative() / 1000000.0;
        if (videostate->force_refresh || videostate->last_vis_time + rdftspeed < time) {
            video_display(videostate);
            videostate->last_vis_time = time;
        }
        *remaining_time = FFMIN(*remaining_time, videostate->last_vis_time + rdftspeed - time);
    }

    if (videostate->video_st) {
retry:
        if (frame_queue_nb_remaining(&videostate->pictq) == 0) {
            // nothing to do, no picture to display in the queue
        } else {
            double last_duration, duration, delay;
            Frame *vp, *lastvp;

            /* dequeue the picture */
            lastvp = frame_queue_peek_last(&videostate->pictq);
            vp = frame_queue_peek(&videostate->pictq);

            if (vp->serial != videostate->videoq.serial) {
                frame_queue_next(&videostate->pictq);
                goto retry;
            }

            if (lastvp->serial != vp->serial)
                videostate->frame_timer = av_gettime_relative() / 1000000.0;

            if (videostate->paused)
                goto display;

            /* compute nominal last_duration */
            last_duration = vp_duration(videostate, lastvp, vp);
            delay = compute_target_delay(last_duration, videostate);

            time= av_gettime_relative()/1000000.0;
            if (time < videostate->frame_timer + delay) {
                *remaining_time = FFMIN(videostate->frame_timer + delay - time, *remaining_time);
                goto display;
            }

            videostate->frame_timer += delay;
            if (delay > 0 && time - videostate->frame_timer > AV_SYNC_THRESHOLD_MAX)
                videostate->frame_timer = time;

            SDL_LockMutex(videostate->pictq.mutex);
            if (!isnan(vp->pts))
                update_video_pts(videostate, vp->pts, vp->pos, vp->serial);
            SDL_UnlockMutex(videostate->pictq.mutex);

            if (frame_queue_nb_remaining(&videostate->pictq) > 1) {
                Frame *nextvp = frame_queue_peek_next(&videostate->pictq);
                duration = vp_duration(videostate, vp, nextvp);
                if(!videostate->step && (framedrop>0 || (framedrop && get_master_sync_type(videostate) != AV_SYNC_VIDEO_MASTER)) && time > videostate->frame_timer + duration){
                    videostate->frame_drops_late++;
                    frame_queue_next(&videostate->pictq);
                    goto retry;
                }
            }

            if (videostate->subtitle_st) {
                while (frame_queue_nb_remaining(&videostate->subpq) > 0) {
                    sp = frame_queue_peek(&videostate->subpq);

                    if (frame_queue_nb_remaining(&videostate->subpq) > 1)
                        sp2 = frame_queue_peek_next(&videostate->subpq);
                    else
                        sp2 = NULL;

                    if (sp->serial != videostate->subtitleq.serial
                            || (videostate->vidclk.pts > (sp->pts + ((float) sp->sub.end_display_time / 1000)))
                            || (sp2 && videostate->vidclk.pts > (sp2->pts + ((float) sp2->sub.start_display_time / 1000))))
                    {
                        if (sp->uploaded) {
                            int i;
                            for (i = 0; i < sp->sub.num_rects; i++) {
                                AVSubtitleRect *sub_rect = sp->sub.rects[i];
                                uint8_t *pixels;
                                int pitch, j;

                                if (!SDL_LockTexture(videostate->sub_texture, (SDL_Rect *)sub_rect, (void **)&pixels, &pitch)) {
                                    for (j = 0; j < sub_rect->h; j++, pixels += pitch)
                                        memset(pixels, 0, sub_rect->w << 2);
                                    SDL_UnlockTexture(videostate->sub_texture);
                                }
                            }
                        }
                        frame_queue_next(&videostate->subpq);
                    } else {
                        break;
                    }
                }
            }

            frame_queue_next(&videostate->pictq);
            videostate->force_refresh = 1;

            if (videostate->step && !videostate->paused)
                stream_toggle_pause(videostate);
        }
display:
        /* display picture */
        if (!display_disable && videostate->force_refresh && videostate->show_mode == SHOW_MODE_VIDEO && videostate->pictq.rindex_shown)
        {
            SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
            SDL_RenderClear(renderer);
            video_display(videostate);
        }
    }
    videostate->force_refresh = 0;
    if (show_status) {
        AVBPrint buf;
        static int64_t last_time;
        int64_t cur_time;
        int aqsize, vqsize, sqsize;
        double av_diff;

        cur_time = av_gettime_relative();
        if (!last_time || (cur_time - last_time) >= 30000) {
            aqsize = 0;
            vqsize = 0;
            sqsize = 0;
            if (videostate->audio_st)
                aqsize = videostate->audioq.size;
            if (videostate->video_st)
                vqsize = videostate->videoq.size;
            if (videostate->subtitle_st)
                sqsize = videostate->subtitleq.size;
            av_diff = 0;
            if (videostate->audio_st && videostate->video_st)
                av_diff = get_clock(&videostate->audclk) - get_clock(&videostate->vidclk);
            else if (videostate->video_st)
                av_diff = get_master_clock(videostate) - get_clock(&videostate->vidclk);
            else if (videostate->audio_st)
                av_diff = get_master_clock(videostate) - get_clock(&videostate->audclk);
            
            av_bprint_init(&buf, 0, AV_BPRINT_SIZE_AUTOMATIC);
            fflush(stderr);
            av_bprint_finalize(&buf, NULL);

            last_time = cur_time;
        }
    }
}

void update_video_pts(VideoState *videostate, double pts, int64_t pos, int serial) {
    /* update current video pts */
    set_clock(&videostate->vidclk, pts, serial);
    sync_clock_to_slave(&videostate->extclk, &videostate->vidclk);
}

void video_display(VideoState *videostate)
{
    if (!videostate->width)
        video_open(videostate);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);
    if (videostate->audio_st && videostate->show_mode != SHOW_MODE_VIDEO)
        video_audio_display(videostate);
    else if (videostate->video_st)
        video_image_display(videostate);
    update_imgui(renderer);
    SDL_RenderPresent(renderer);
}

int video_open(VideoState *videostate)
{
    int w,h;

    w = screen_width ? screen_width : default_width;
    h = screen_height ? screen_height : default_height;

    if (!window_title)
        window_title = input_filename;
    SDL_SetWindowTitle(window, window_title);

    SDL_SetWindowSize(window, w, h);
    SDL_SetWindowPosition(window, screen_left, screen_top);
    if (is_full_screen)
        SDL_SetWindowFullscreen(window, SDL_WINDOW_FULLSCREEN_DESKTOP);
    SDL_ShowWindow(window);

    videostate->width  = w;
    videostate->height = h;

    return 0;
}

void video_audio_display(VideoState *videostate)
{
    int i, i_start, x, y1, y, ys, delay, n, nb_display_channels;
    int ch, channels, h, h2;
    int64_t time_diff;
    int rdft_bits, nb_freq;

    for (rdft_bits = 1; (1 << rdft_bits) < 2 * videostate->height; rdft_bits++)
        ;
    nb_freq = 1 << (rdft_bits - 1);

    /* compute display index : center on currently output samples */
    channels = videostate->audio_tgt.channels;
    nb_display_channels = channels;
    if (!videostate->paused) {
        int data_used= videostate->show_mode == SHOW_MODE_WAVES ? videostate->width : (2*nb_freq);
        n = 2 * channels;
        delay = videostate->audio_write_buf_size;
        delay /= n;

        /* to be more precise, we take into account the time spent since
           the last buffer computation */
        if (audio_callback_time) {
            time_diff = av_gettime_relative() - audio_callback_time;
            delay -= (time_diff * videostate->audio_tgt.freq) / 1000000;
        }

        delay += 2 * data_used;
        if (delay < data_used)
            delay = data_used;

        i_start= x = compute_mod(videostate->sample_array_index - delay * channels, SAMPLE_ARRAY_SIZE);
        if (videostate->show_mode == SHOW_MODE_WAVES) {
            h = INT_MIN;
            for (i = 0; i < 1000; i += channels) {
                int idx = (SAMPLE_ARRAY_SIZE + x - i) % SAMPLE_ARRAY_SIZE;
                int a = videostate->sample_array[idx];
                int b = videostate->sample_array[(idx + 4 * channels) % SAMPLE_ARRAY_SIZE];
                int c = videostate->sample_array[(idx + 5 * channels) % SAMPLE_ARRAY_SIZE];
                int d = videostate->sample_array[(idx + 9 * channels) % SAMPLE_ARRAY_SIZE];
                int score = a - d;
                if (h < score && (b ^ c) < 0) {
                    h = score;
                    i_start = idx;
                }
            }
        }

        videostate->last_i_start = i_start;
    } else {
        i_start = videostate->last_i_start;
    }

    if (videostate->show_mode == SHOW_MODE_WAVES) {
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);

        /* total height for one channel */
        h = videostate->height / nb_display_channels;
        /* graph height / 2 */
        h2 = (h * 9) / 20;
        for (ch = 0; ch < nb_display_channels; ch++) {
            i = i_start + ch;
            y1 = videostate->ytop + ch * h + (h / 2); /* position of center line */
            for (x = 0; x < videostate->width; x++) {
                y = (videostate->sample_array[i] * h2) >> 15;
                if (y < 0) {
                    y = -y;
                    ys = y1 - y;
                } else {
                    ys = y1;
                }
                fill_rectangle(videostate->xleft + x, ys, 1, y);
                i += channels;
                if (i >= SAMPLE_ARRAY_SIZE)
                    i -= SAMPLE_ARRAY_SIZE;
            }
        }

        SDL_SetRenderDrawColor(renderer, 0, 0, 255, 255);

        for (ch = 1; ch < nb_display_channels; ch++) {
            y = videostate->ytop + ch * h;
            fill_rectangle(videostate->xleft, y, videostate->width, 1);
        }
    } else {
        if (realloc_texture(&videostate->vis_texture, SDL_PIXELFORMAT_ARGB8888, videostate->width, videostate->height, SDL_BLENDMODE_NONE, 1) < 0)
            return;

        if (videostate->xpos >= videostate->width)
            videostate->xpos = 0;
        nb_display_channels= FFMIN(nb_display_channels, 2);
        if (rdft_bits != videostate->rdft_bits) {
            av_rdft_end(videostate->rdft);
            av_free(videostate->rdft_data);
            videostate->rdft = av_rdft_init(rdft_bits, DFT_R2C);
            videostate->rdft_bits = rdft_bits;
            videostate->rdft_data = (FFTSample *)av_malloc_array(nb_freq, 4 *sizeof(*videostate->rdft_data));
        }
        if (!videostate->rdft || !videostate->rdft_data){
            std::cout<<"ERROR: Failed to allocate buffers for RDFT, switching to waves display."<<std::endl;
            videostate->show_mode = SHOW_MODE_WAVES;
        } else {
            FFTSample *data[2];
            SDL_Rect rect = {.x = videostate->xpos, .y = 0, .w = 1, .h = videostate->height};
            uint32_t *pixels;
            int pitch;
            for (ch = 0; ch < nb_display_channels; ch++) {
                data[ch] = videostate->rdft_data + 2 * nb_freq * ch;
                i = i_start + ch;
                for (x = 0; x < 2 * nb_freq; x++) {
                    double w = (x-nb_freq) * (1.0 / nb_freq);
                    data[ch][x] = videostate->sample_array[i] * (1.0 - w * w);
                    i += channels;
                    if (i >= SAMPLE_ARRAY_SIZE)
                        i -= SAMPLE_ARRAY_SIZE;
                }
                av_rdft_calc(videostate->rdft, data[ch]);
            }
            /* Least efficient way to do this, we should of course
             * directly access it but it is more than fast enough. */
            if (!SDL_LockTexture(videostate->vis_texture, &rect, (void **)&pixels, &pitch)) {
                pitch >>= 2;
                pixels += pitch * videostate->height;
                for (y = 0; y < videostate->height; y++) {
                    double w = 1 / sqrt(nb_freq);
                    int a = sqrt(w * sqrt(data[0][2 * y + 0] * data[0][2 * y + 0] + data[0][2 * y + 1] * data[0][2 * y + 1]));
                    int b = (nb_display_channels == 2 ) ? sqrt(w * hypot(data[1][2 * y + 0], data[1][2 * y + 1]))
                                                        : a;
                    a = FFMIN(a, 255);
                    b = FFMIN(b, 255);
                    pixels -= pitch;
                    *pixels = (a << 16) + (b << 8) + ((a+b) >> 1);
                }
                SDL_UnlockTexture(videostate->vis_texture);
            }
            SDL_RenderCopy(renderer, videostate->vis_texture, NULL, NULL);
        }
        if (!videostate->paused)
            videostate->xpos++;
    }
}

void fill_rectangle(int x, int y, int w, int h)
{
    SDL_Rect rect;
    rect.x = x;
    rect.y = y;
    rect.w = w;
    rect.h = h;
    if (w && h)
        SDL_RenderFillRect(renderer, &rect);
}

int compute_mod(int a, int b)
{
    return a < 0 ? a%b + b : a%b;
}

int realloc_texture(SDL_Texture **texture, Uint32 new_format, int new_width, int new_height, SDL_BlendMode blendmode, int init_texture)
{
    Uint32 format;
    int access, w, h;
    if (!*texture || SDL_QueryTexture(*texture, &format, &access, &w, &h) < 0 || new_width != w || new_height != h || new_format != format) {
        void *pixels;
        int pitch;
        if (*texture)
            SDL_DestroyTexture(*texture);
        if (!(*texture = SDL_CreateTexture(renderer, new_format, SDL_TEXTUREACCESS_STREAMING, new_width, new_height)))
            return -1;
        if (SDL_SetTextureBlendMode(*texture, blendmode) < 0)
            return -1;
        if (init_texture) {
            if (SDL_LockTexture(*texture, NULL, &pixels, &pitch) < 0)
                return -1;
            memset(pixels, 0, pitch * new_height);
            SDL_UnlockTexture(*texture);
        }
    }
    return 0;
}

void video_image_display(VideoState *videostate)
{
    Frame *vp;
    Frame *sp = NULL;
    SDL_Rect rect;

    vp = frame_queue_peek_last(&videostate->pictq);
    if (videostate->subtitle_st) {
        if (frame_queue_nb_remaining(&videostate->subpq) > 0) {
            sp = frame_queue_peek(&videostate->subpq);

            if (vp->pts >= sp->pts + ((float) sp->sub.start_display_time / 1000)) {
                if (!sp->uploaded) {
                    uint8_t* pixels[4];
                    int pitch[4];
                    int i;
                    if (!sp->width || !sp->height) {
                        sp->width = vp->width;
                        sp->height = vp->height;
                    }
                    if (realloc_texture(&videostate->sub_texture, SDL_PIXELFORMAT_ARGB8888, sp->width, sp->height, SDL_BLENDMODE_BLEND, 1) < 0)
                        return;

                    for (i = 0; i < sp->sub.num_rects; i++) {
                        AVSubtitleRect *sub_rect = sp->sub.rects[i];

                        sub_rect->x = av_clip(sub_rect->x, 0, sp->width );
                        sub_rect->y = av_clip(sub_rect->y, 0, sp->height);
                        sub_rect->w = av_clip(sub_rect->w, 0, sp->width  - sub_rect->x);
                        sub_rect->h = av_clip(sub_rect->h, 0, sp->height - sub_rect->y);

                        videostate->sub_convert_ctx = sws_getCachedContext(videostate->sub_convert_ctx,
                            sub_rect->w, sub_rect->h, AV_PIX_FMT_PAL8,
                            sub_rect->w, sub_rect->h, AV_PIX_FMT_BGRA,
                            0, NULL, NULL, NULL);
                        if (!videostate->sub_convert_ctx) {
                            std::cout<<"FATAL ERROR: Could not initialize convertion context!"<<std::endl;
                            return;
                        }
                        if (!SDL_LockTexture(videostate->sub_texture, (SDL_Rect *)sub_rect, (void **)pixels, pitch)) {
                            sws_scale(videostate->sub_convert_ctx, (const uint8_t * const *)sub_rect->data, sub_rect->linesize,
                                      0, sub_rect->h, pixels, pitch);
                            SDL_UnlockTexture(videostate->sub_texture);
                        }
                    }
                    sp->uploaded = 1;
                }
            } else
                sp = NULL;
        }
    }

    calculate_display_rect(&rect, videostate->xleft, videostate->ytop, videostate->width, videostate->height, vp->width, vp->height, vp->sar);

    if (!vp->uploaded) {
        if (upload_texture(&videostate->vid_texture, vp->frame, &videostate->img_convert_ctx) < 0)
            return;
        vp->uploaded = 1;
        vp->flip_v = vp->frame->linesize[0] < 0;
    }

    set_sdl_yuv_conversion_mode(vp->frame);

    need_flip = static_cast<SDL_RendererFlip>((vp->flip_v ? SDL_FLIP_VERTICAL : 0));

    SDL_RenderCopyEx(renderer, videostate->vid_texture, NULL, &rect, 0, NULL, need_flip);
    set_sdl_yuv_conversion_mode(NULL);
    if (sp) {
        SDL_RenderCopy(renderer, videostate->sub_texture, NULL, &rect);
    }
}

int upload_texture(SDL_Texture **tex, AVFrame *frame, struct SwsContext **img_convert_ctx) {
    int ret = 0;
    Uint32 sdl_pix_fmt;
    SDL_BlendMode sdl_blendmode;
    get_sdl_pix_fmt_and_blendmode(frame->format, &sdl_pix_fmt, &sdl_blendmode);
    if (realloc_texture(tex, sdl_pix_fmt == SDL_PIXELFORMAT_UNKNOWN ? SDL_PIXELFORMAT_ARGB8888 : sdl_pix_fmt, frame->width, frame->height, sdl_blendmode, 0) < 0)
        return -1;
    switch (sdl_pix_fmt) {
        case SDL_PIXELFORMAT_UNKNOWN:
            /* This should only happen if we are not using avfilter... */
            *img_convert_ctx = sws_getCachedContext(*img_convert_ctx,
                frame->width, frame->height, static_cast<AVPixelFormat>(frame->format), frame->width, frame->height,
                AV_PIX_FMT_BGRA, sws_flags, NULL, NULL, NULL);
            if (*img_convert_ctx != NULL) {
                uint8_t *pixels[4];
                int pitch[4];
                if (!SDL_LockTexture(*tex, NULL, (void **)pixels, pitch)) {
                    sws_scale(*img_convert_ctx, (const uint8_t * const *)frame->data, frame->linesize,
                              0, frame->height, pixels, pitch);
                    SDL_UnlockTexture(*tex);
                }
            } else {
                std::cout<<"FATAL ERROR: Could not initialize convertion context!"<<std::endl;
                ret = -1;
            }
            break;
        case SDL_PIXELFORMAT_IYUV:
            if (frame->linesize[0] > 0 && frame->linesize[1] > 0 && frame->linesize[2] > 0) {
                ret = SDL_UpdateYUVTexture(*tex, NULL, frame->data[0], frame->linesize[0],
                                                       frame->data[1], frame->linesize[1],
                                                       frame->data[2], frame->linesize[2]);
            } else if (frame->linesize[0] < 0 && frame->linesize[1] < 0 && frame->linesize[2] < 0) {
                ret = SDL_UpdateYUVTexture(*tex, NULL, frame->data[0] + frame->linesize[0] * (frame->height                    - 1), -frame->linesize[0],
                                                       frame->data[1] + frame->linesize[1] * (AV_CEIL_RSHIFT(frame->height, 1) - 1), -frame->linesize[1],
                                                       frame->data[2] + frame->linesize[2] * (AV_CEIL_RSHIFT(frame->height, 1) - 1), -frame->linesize[2]);
            } else {
                std::cout<<"FATAL ERROR: Mixed negative and positive linesizes are not supported."<<std::endl;
                return -1;
            }
            break;
        default:
            if (frame->linesize[0] < 0) {
                ret = SDL_UpdateTexture(*tex, NULL, frame->data[0] + frame->linesize[0] * (frame->height - 1), -frame->linesize[0]);
            } else {
                ret = SDL_UpdateTexture(*tex, NULL, frame->data[0], frame->linesize[0]);
            }
            break;
    }
    return ret;
}

void set_sdl_yuv_conversion_mode(AVFrame *frame)
{
#if SDL_VERSION_ATLEAST(2,0,8)
    SDL_YUV_CONVERSION_MODE mode = SDL_YUV_CONVERSION_AUTOMATIC;
    if (frame && (frame->format == AV_PIX_FMT_YUV420P || frame->format == AV_PIX_FMT_YUYV422 || frame->format == AV_PIX_FMT_UYVY422)) {
        if (frame->color_range == AVCOL_RANGE_JPEG)
            mode = SDL_YUV_CONVERSION_JPEG;
        else if (frame->colorspace == AVCOL_SPC_BT709)
            mode = SDL_YUV_CONVERSION_BT709;
        else if (frame->colorspace == AVCOL_SPC_BT470BG || frame->colorspace == AVCOL_SPC_SMPTE170M)
            mode = SDL_YUV_CONVERSION_BT601;
    }
    SDL_SetYUVConversionMode(mode); /* FIXME: no support for linear transfer */
#endif
}

void get_sdl_pix_fmt_and_blendmode(int format, Uint32 *sdl_pix_fmt, SDL_BlendMode *sdl_blendmode)
{
    int i;
    *sdl_blendmode = SDL_BLENDMODE_NONE;
    *sdl_pix_fmt = SDL_PIXELFORMAT_UNKNOWN;
    if (format == AV_PIX_FMT_RGB32   ||
        format == AV_PIX_FMT_RGB32_1 ||
        format == AV_PIX_FMT_BGR32   ||
        format == AV_PIX_FMT_BGR32_1)
        *sdl_blendmode = SDL_BLENDMODE_BLEND;
    for (i = 0; i < FF_ARRAY_ELEMS(sdl_texture_format_map) - 1; i++){
        if (format == sdl_texture_format_map[i].format) {
            *sdl_pix_fmt = sdl_texture_format_map[i].texture_fmt;
            return;
        }
    }
}

/*
**  Audio functions
*/

int audio_open(void *opaque, int64_t wanted_channel_layout, int wanted_nb_channels, int wanted_sample_rate, struct AudioParams *audio_hw_params)
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
    wanted_spec.callback = sdl_audio_callback;
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

void sdl_audio_callback(void *opaque, Uint8 *stream, int len)
{
    VideoState *videostate = (VideoState *)opaque;
    int audio_size, len1;

    audio_callback_time = av_gettime_relative();

    while (len > 0) {
        if (videostate->audio_buf_index >= videostate->audio_buf_size) {
           audio_size = audio_decode_frame(videostate);
           if (audio_size < 0) {
                /* if error, just output silence */
               videostate->audio_buf = NULL;
               videostate->audio_buf_size = SDL_AUDIO_MIN_BUFFER_SIZE / videostate->audio_tgt.frame_size * videostate->audio_tgt.frame_size;
           } else {
               if (videostate->show_mode != SHOW_MODE_VIDEO)
                   update_sample_display(videostate, (int16_t *)videostate->audio_buf, audio_size);
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
        set_clock_at(&videostate->audclk, videostate->audio_clock - (double)(2 * videostate->audio_hw_buf_size + videostate->audio_write_buf_size) / videostate->audio_tgt.bytes_per_sec, videostate->audio_clock_serial, audio_callback_time / 1000000.0);
        sync_clock_to_slave(&videostate->extclk, &videostate->audclk);
    }
}

int audio_decode_frame(VideoState *videostate)
{
    int data_size, resampled_data_size;
    int64_t dec_channel_layout;
    av_unused double audio_clock0;
    int wanted_nb_samples;
    Frame *af;

    if (videostate->paused)
        return -1;

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
    wanted_nb_samples = synchronize_audio(videostate, af->frame->nb_samples);

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

int synchronize_audio(VideoState *videostate, int nb_samples)
{
    int wanted_nb_samples = nb_samples;

    /* if not master, then we try to remove or add samples to correct the clock */
    if (get_master_sync_type(videostate) != AV_SYNC_AUDIO_MASTER) {
        double diff, avg_diff;
        int min_nb_samples, max_nb_samples;

        diff = get_clock(&videostate->audclk) - get_master_clock(videostate);

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

/*
**  Seek functions
*/

void step_to_next_frame(VideoState *videostate)
{
    /* If the stream is paused unpause it, then step */
    if (videostate->paused)
        stream_toggle_pause(videostate);
    videostate->step = 1;
}

/* Pause or resume the video */
void stream_toggle_pause(VideoState *videostate)
{
    if (videostate->paused) {
        videostate->frame_timer += av_gettime_relative() / 1000000.0 - videostate->vidclk.last_updated;
        if (videostate->read_pause_return != AVERROR(ENOSYS)) {
            videostate->vidclk.paused = 0;
        }
        set_clock(&videostate->vidclk, get_clock(&videostate->vidclk), videostate->vidclk.serial);
    }
    set_clock(&videostate->extclk, get_clock(&videostate->extclk), videostate->extclk.serial);
    videostate->paused = videostate->audclk.paused = videostate->vidclk.paused = videostate->extclk.paused = !videostate->paused;
}

void stream_seek(VideoState *videostate, int64_t pos, int64_t rel, int seek_by_bytes)
{
    if (!videostate->seek_req) {
        videostate->seek_pos = pos;
        videostate->seek_rel = rel;
        videostate->seek_flags &= ~AVSEEK_FLAG_BYTE;
        if (seek_by_bytes)
            videostate->seek_flags |= AVSEEK_FLAG_BYTE;
        videostate->seek_req = 1;
        SDL_CondSignal(videostate->continue_read_thread);
    }
}

/*
**  Event functions
*/

void event_loop(VideoState *videostate)
{
    SDL_Event event;
    double incr, pos, frac;
    for (;;) {
        double x;
        refresh_loop_wait_event(videostate, &event);

        switch (event.type) {
        case SDL_KEYDOWN:
            if (exit_on_keydown || event.key.keysym.sym == SDLK_ESCAPE || event.key.keysym.sym == SDLK_q) {
                do_exit(videostate);
                break;
            }
            // If we don't yet have a window, skip all key events, because read_thread might still be initializing...
            if (!videostate->width)
                continue;
            switch (event.key.keysym.sym) {
            case SDLK_f:
                toggle_full_screen(videostate);
                videostate->force_refresh = 1;
                break;
            case SDLK_p:
            case SDLK_SPACE:
                toggle_pause(videostate);
                break;
            case SDLK_m:
                toggle_mute(videostate);
                break;
            case SDLK_KP_MULTIPLY:
            case SDLK_0:
                update_volume(videostate, 1, SDL_VOLUME_STEP);
                break;
            case SDLK_KP_DIVIDE:
            case SDLK_9:
                update_volume(videostate, -1, SDL_VOLUME_STEP);
                break;
            case SDLK_s: // S: Step to next frame
                step_to_next_frame(videostate);
                break;
            case SDLK_a:
                stream_cycle_channel(videostate, AVMEDIA_TYPE_AUDIO);
                break;
            case SDLK_v:
                stream_cycle_channel(videostate, AVMEDIA_TYPE_VIDEO);
                break;
            case SDLK_c:
                stream_cycle_channel(videostate, AVMEDIA_TYPE_VIDEO);
                stream_cycle_channel(videostate, AVMEDIA_TYPE_AUDIO);
                stream_cycle_channel(videostate, AVMEDIA_TYPE_SUBTITLE);

                /*
                **  Work around for channel switch distortion 
                */
                incr = seek_interval ? -seek_interval : -1.0;
                goto do_seek;

                break;
            case SDLK_t:
                stream_cycle_channel(videostate, AVMEDIA_TYPE_SUBTITLE);
                break;
            case SDLK_w:
                toggle_audio_display(videostate);
                break;
            case SDLK_PAGEUP:
                if (videostate->ic->nb_chapters <= 1) {
                    incr = 600.0;
                    goto do_seek;
                }
                seek_chapter(videostate, 1);
                break;
            case SDLK_PAGEDOWN:
                if (videostate->ic->nb_chapters <= 1) {
                    incr = -600.0;
                    goto do_seek;
                }
                seek_chapter(videostate, -1);
                break;
            case SDLK_LEFT:
                incr = seek_interval ? -seek_interval : -10.0;
                goto do_seek;
            case SDLK_RIGHT:
                incr = seek_interval ? seek_interval : 10.0;
                goto do_seek;
            case SDLK_UP:
                incr = 60.0;
                goto do_seek;
            case SDLK_DOWN:
                incr = -60.0;
            do_seek:
                    if (seek_by_bytes) {
                        pos = -1;
                        if (pos < 0 && videostate->video_stream >= 0)
                            pos = frame_queue_last_pos(&videostate->pictq);
                        if (pos < 0 && videostate->audio_stream >= 0)
                            pos = frame_queue_last_pos(&videostate->sampq);
                        if (pos < 0)
                            pos = avio_tell(videostate->ic->pb);
                        if (videostate->ic->bit_rate)
                            incr *= videostate->ic->bit_rate / 8.0;
                        else
                            incr *= 180000.0;
                        pos += incr;
                        stream_seek(videostate, pos, incr, 1);
                    } else {
                        pos = get_master_clock(videostate);
                        if (isnan(pos))
                            pos = (double)videostate->seek_pos / AV_TIME_BASE;
                        pos += incr;
                        if (videostate->ic->start_time != AV_NOPTS_VALUE && pos < videostate->ic->start_time / (double)AV_TIME_BASE)
                            pos = videostate->ic->start_time / (double)AV_TIME_BASE;
                        stream_seek(videostate, (int64_t)(pos * AV_TIME_BASE), (int64_t)(incr * AV_TIME_BASE), 0);
                    }
                break;
            default:
                break;
            }
            break;
        case SDL_MOUSEBUTTONDOWN:
            if(!want_capture_mouse())
            {
                if (exit_on_mousedown) {
                    do_exit(videostate);
                    break;
                }
                if (event.button.button == SDL_BUTTON_LEFT) {
                    static int64_t last_mouse_left_click = 0;
                    if (av_gettime_relative() - last_mouse_left_click <= 500000) {
                        toggle_full_screen(videostate);
                        videostate->force_refresh = 1;
                        last_mouse_left_click = 0;
                    } else {
                        last_mouse_left_click = av_gettime_relative();
                    }
                }
            }
        case SDL_MOUSEMOTION:
            if (cursor_hidden) {
                SDL_ShowCursor(1);
                cursor_hidden = 0;
            }
            cursor_last_shown = av_gettime_relative();
            if (event.type == SDL_MOUSEBUTTONDOWN) {
                if (event.button.button != SDL_BUTTON_RIGHT)
                    break;
                x = event.button.x;
            } else {
                if (!(event.motion.state & SDL_BUTTON_RMASK))
                    break;
                x = event.motion.x;
            }
                if (seek_by_bytes || videostate->ic->duration <= 0) {
                    uint64_t size =  avio_size(videostate->ic->pb);
                    stream_seek(videostate, size*x/videostate->width, 0, 1);
                } else {
                    int64_t ts;
                    int ns, hh, mm, ss;
                    int tns, thh, tmm, tss;
                    tns  = videostate->ic->duration / 1000000LL;
                    thh  = tns / 3600;
                    tmm  = (tns % 3600) / 60;
                    tss  = (tns % 60);
                    frac = x / videostate->width;
                    ns   = frac * tns;
                    hh   = ns / 3600;
                    mm   = (ns % 3600) / 60;
                    ss   = (ns % 60);
                    std::cout<<"Seeking to "<<frac*100<<":"<<hh<<":"<<mm<<":"<<ss<<":"<<thh<<":"<<tmm<<":"<<tss<<std::endl;
                    ts = frac * videostate->ic->duration;
                    if (videostate->ic->start_time != AV_NOPTS_VALUE)
                        ts += videostate->ic->start_time;
                    stream_seek(videostate, ts, 0, 0);
                }
            break;
        case SDL_WINDOWEVENT:
            switch (event.window.event) {
                case SDL_WINDOWEVENT_SIZE_CHANGED:
                    screen_width  = videostate->width  = event.window.data1;
                    screen_height = videostate->height = event.window.data2;
                    if (videostate->vis_texture) {
                        SDL_DestroyTexture(videostate->vis_texture);
                        videostate->vis_texture = NULL;
                    }
                case SDL_WINDOWEVENT_EXPOSED:
                    videostate->force_refresh = 1;
            }
            break;
        case SDL_QUIT:
        case FF_QUIT_EVENT:
            do_exit(videostate);
            break;
        default:
            break;
        }
        imgui_event_handler(event);
    }
}

void refresh_loop_wait_event(VideoState *videostate, SDL_Event *event) 
{
    double remaining_time = 0.0;
    SDL_PumpEvents();
    while (!SDL_PeepEvents(event, 1, SDL_GETEVENT, SDL_FIRSTEVENT, SDL_LASTEVENT)) {
        if (!cursor_hidden && av_gettime_relative() - cursor_last_shown > CURSOR_HIDE_DELAY) {
            SDL_ShowCursor(0);
            cursor_hidden = 1;
        }
        if (remaining_time > 0.0)
            av_usleep((int64_t)(remaining_time * 1000000.0));
        remaining_time = REFRESH_RATE;
        if (videostate->show_mode != SHOW_MODE_NONE && (!videostate->paused || videostate->force_refresh))
            video_refresh(videostate, &remaining_time);
        
        // Display last received frame here
        if(videostate->paused){
            SDL_RenderClear(renderer);
            if (videostate->audio_st && videostate->show_mode != SHOW_MODE_VIDEO)
                video_audio_display(videostate);
            else if (videostate->video_st)
                video_image_display(videostate);
            
            update_imgui(renderer);
            SDL_RenderPresent(renderer);
        }
        SDL_PumpEvents();
    }
}

void toggle_full_screen(VideoState *videostate)
{
    is_full_screen = !is_full_screen;
    SDL_SetWindowFullscreen(window, is_full_screen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
}

void toggle_pause(VideoState *videostate)
{
    stream_toggle_pause(videostate);
    videostate->step = 0;
}

void toggle_mute(VideoState *videostate)
{
    videostate->muted = !videostate->muted;
}

void update_volume(VideoState *videostate, int sign, double step)
{
    double volume_level = videostate->audio_volume ? (20 * log(videostate->audio_volume / (double)SDL_MIX_MAXVOLUME) / log(10)) : -1000.0;
    int new_volume = lrint(SDL_MIX_MAXVOLUME * pow(10.0, (volume_level + sign * step) / 20.0));
    videostate->audio_volume = av_clip(videostate->audio_volume == new_volume ? (videostate->audio_volume + sign) : new_volume, 0, SDL_MIX_MAXVOLUME);
}

void stream_cycle_channel(VideoState *videostate, int codec_type)
{
    AVFormatContext *ic = videostate->ic;
    int start_index, stream_index;
    int old_index;
    AVStream *st;
    AVProgram *p = NULL;
    int nb_streams = videostate->ic->nb_streams;

    if (codec_type == AVMEDIA_TYPE_VIDEO) {
        start_index = videostate->last_video_stream;
        old_index = videostate->video_stream;
    } else if (codec_type == AVMEDIA_TYPE_AUDIO) {
        start_index = videostate->last_audio_stream;
        old_index = videostate->audio_stream;
    } else {
        start_index = videostate->last_subtitle_stream;
        old_index = videostate->subtitle_stream;
    }
    stream_index = start_index;

    if (codec_type != AVMEDIA_TYPE_VIDEO && videostate->video_stream != -1) {
        p = av_find_program_from_stream(ic, NULL, videostate->video_stream);
        if (p) {
            nb_streams = p->nb_stream_indexes;
            for (start_index = 0; start_index < nb_streams; start_index++)
                if (p->stream_index[start_index] == stream_index)
                    break;
            if (start_index == nb_streams)
                start_index = -1;
            stream_index = start_index;
        }
    }

    for (;;) {
        if (++stream_index >= nb_streams)
        {
            if (codec_type == AVMEDIA_TYPE_SUBTITLE)
            {
                stream_index = -1;
                videostate->last_subtitle_stream = -1;
                goto the_end;
            }
            if (start_index == -1)
                return;
            stream_index = 0;
        }
        if (stream_index == start_index)
            return;
        st = videostate->ic->streams[p ? p->stream_index[stream_index] : stream_index];
        if (st->codecpar->codec_type == codec_type) {
            /* check that parameters are OK */
            switch (codec_type) {
            case AVMEDIA_TYPE_AUDIO:
                if (st->codecpar->sample_rate != 0 &&
                    st->codecpar->channels != 0)
                    goto the_end;
                break;
            case AVMEDIA_TYPE_VIDEO:
            case AVMEDIA_TYPE_SUBTITLE:
                goto the_end;
            default:
                break;
            }
        }
    }
 the_end:
    if (p && stream_index != -1)
        stream_index = p->stream_index[stream_index];
    std::cout<<"LOG: Switching stream from "<<old_index<<" to "<<stream_index<<std::endl;
    stream_component_close(videostate, old_index);
    stream_component_open(videostate, stream_index);
}

void toggle_audio_display(VideoState *videostate)
{
    int next = videostate->show_mode;
    do {
        next = (next + 1) % SHOW_MODE_NB;
    } while (next != videostate->show_mode && (next == SHOW_MODE_VIDEO && !videostate->video_st || next != SHOW_MODE_VIDEO && !videostate->audio_st));
    if (videostate->show_mode != next) {
        videostate->force_refresh = 1;
        videostate->show_mode = static_cast<ShowMode>(next);
    }
}

void seek_chapter(VideoState *videostate, int incr)
{
    int64_t pos = get_master_clock(videostate) * AV_TIME_BASE;
    int i;

    if (!videostate->ic->nb_chapters)
        return;

    /* find the current chapter */
    for (i = 0; i < videostate->ic->nb_chapters; i++) {
        AVChapter *ch = videostate->ic->chapters[i];
        if (av_compare_ts(pos, AV_TIME_BASE_Q, ch->start, ch->time_base) < 0) {
            i--;
            break;
        }
    }

    i += incr;
    i = FFMAX(i, 0);
    if (i >= videostate->ic->nb_chapters)
        return;

    std::cout<<"LOG: Seeking to chapter "<<i<<std::endl;
    stream_seek(videostate, av_rescale_q(videostate->ic->chapters[i]->start, videostate->ic->chapters[i]->time_base,
                                 AV_TIME_BASE_Q), 0, 0);
}

/*
**  Exit functions
*/

void do_exit(VideoState *videostate)
{
    if (videostate) {
        stream_close(videostate);
    }
    if (renderer)
        SDL_DestroyRenderer(renderer);
    if (window)
        SDL_DestroyWindow(window);

    destroy_imgui_data();
    SDL_Quit();
    exit(0);
}