#include "Thread.hpp"
#include "Stream.hpp"
#include "Clock.hpp"
#include "Decode.hpp"
#include "Window.hpp"
#include "Video.hpp"
#include "SeekPause.hpp"

int Thread::read_thread(void *arg)
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
    ic->interrupt_callback.callback = Decode::decode_interrupt_cb;
    ic->interrupt_callback.opaque = videostate;
    // Casting here to make it work with both FFmpeg 5 and 4
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

    videostate->realtime = Clock::is_realtime(ic);

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

    st_index[AVMEDIA_TYPE_VIDEO] = av_find_best_stream( ic, AVMEDIA_TYPE_VIDEO, 
                                                        st_index[AVMEDIA_TYPE_VIDEO], 
                                                        -1, NULL, 0
                                                    );
    st_index[AVMEDIA_TYPE_AUDIO] = av_find_best_stream( ic, AVMEDIA_TYPE_AUDIO,
                                                        st_index[AVMEDIA_TYPE_AUDIO],
                                                        st_index[AVMEDIA_TYPE_VIDEO],
                                                        NULL, 0
                                                    );
    st_index[AVMEDIA_TYPE_SUBTITLE] = av_find_best_stream(  ic, AVMEDIA_TYPE_SUBTITLE,
                                                            st_index[AVMEDIA_TYPE_SUBTITLE],
                                                            (st_index[AVMEDIA_TYPE_AUDIO] >= 0 ?
                                                            st_index[AVMEDIA_TYPE_AUDIO] :
                                                            st_index[AVMEDIA_TYPE_VIDEO]),
                                                            NULL, 0
                                                        );

    videostate->show_mode = show_mode;
    if (st_index[AVMEDIA_TYPE_VIDEO] >= 0) {
        AVStream *st = ic->streams[st_index[AVMEDIA_TYPE_VIDEO]];
        AVCodecParameters *codecpar = st->codecpar;
        AVRational sar = av_guess_sample_aspect_ratio(ic, st, NULL);
        if (codecpar->width)
            Window::set_default_window_size(codecpar->width, codecpar->height, sar);
    }

    /* open the streams */
    if (st_index[AVMEDIA_TYPE_AUDIO] >= 0) {
        Stream::stream_component_open(videostate, st_index[AVMEDIA_TYPE_AUDIO]);
    }

    ret = -1;
    if (st_index[AVMEDIA_TYPE_VIDEO] >= 0) {
        ret = Stream::stream_component_open(videostate, st_index[AVMEDIA_TYPE_VIDEO]);
    }
    if (videostate->show_mode == SHOW_MODE_NONE)
        videostate->show_mode = ret >= 0 ? SHOW_MODE_VIDEO : SHOW_MODE_RDFT;

    if (st_index[AVMEDIA_TYPE_SUBTITLE] >= 0) {
        Stream::stream_component_open(videostate, st_index[AVMEDIA_TYPE_SUBTITLE]);
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
            if (ret < 0){
                std::cout<<"ERROR: Could not seek: "<<videostate->ic->url<<std::endl;
            } 
            else{
                if (videostate->audio_stream >= 0)
                    packet_queue_flush(&videostate->audioq);
                if (videostate->subtitle_stream >= 0)
                    packet_queue_flush(&videostate->subtitleq);
                if (videostate->video_stream >= 0)
                    packet_queue_flush(&videostate->videoq);
                if (videostate->seek_flags & AVSEEK_FLAG_BYTE) {
                   Clock::set_clock(&videostate->extclk, NAN, 0);
                } else {
                   Clock::set_clock(&videostate->extclk, seek_target / (double)AV_TIME_BASE, 0);
                }
            }
            videostate->seek_req = 0;
            videostate->queue_attachments_req = 1;
            videostate->eof = 0;
            if (videostate->paused)
                SeekPause::step_to_next_frame(videostate);
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
            || (Stream::stream_has_enough_packets(videostate->audio_st, videostate->audio_stream, &videostate->audioq) &&
                Stream::stream_has_enough_packets(videostate->video_st, videostate->video_stream, &videostate->videoq) &&
                Stream::stream_has_enough_packets(videostate->subtitle_st, videostate->subtitle_stream, &videostate->subtitleq)))) {
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
                SeekPause::stream_seek(videostate, start_time != AV_NOPTS_VALUE ? start_time : 0, 0, 0);
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

int Thread::video_thread(void *arg)
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
        ret = Video::get_video_frame(videostate, frame);
        if (ret < 0)
            goto the_end;
        if (!ret)
            continue;

        duration = (frame_rate.num && frame_rate.den ? av_q2d((AVRational){frame_rate.den, frame_rate.num}) : 0);
        pts = (frame->pts == AV_NOPTS_VALUE) ? NAN : frame->pts * av_q2d(tb);
        ret = Video::queue_picture(videostate, frame, pts, duration, frame->pkt_pos, videostate->viddec.pkt_serial);
        av_frame_unref(frame);

        if (ret < 0)
            goto the_end;
    }
 the_end:
    av_frame_free(&frame);
    return 0;
}

int Thread::audio_thread(void *arg)
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
        if ((got_frame = Decode::decoder_decode_frame(&videostate->auddec, frame, NULL)) < 0)
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

int Thread::subtitle_thread(void *arg)
{
    VideoState *videostate = (VideoState *)arg;
    Frame *sp;
    int got_subtitle;
    double pts;

    for (;;) {
        if (!(sp = frame_queue_peek_writable(&videostate->subpq)))
            return 0;

        if ((got_subtitle = Decode::decoder_decode_frame(&videostate->subdec, NULL, &sp->sub)) < 0)
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