#include "Video.hpp"
#include "Decode.hpp"
#include "utils/Clock.hpp"
#include "Window.hpp"
#include "SeekPause.hpp"
#include "gl/VideoRenderer.hpp"


int Video::get_video_frame(VideoState *videostate, AVFrame *frame)
{
    int got_picture;

    if ((got_picture = Decode::decoder_decode_frame(&videostate->viddec, frame, NULL)) < 0)
        return -1;

    if (got_picture) {
        double dpts = NAN;

        if (frame->pts != AV_NOPTS_VALUE)
            dpts = av_q2d(videostate->video_st->time_base) * frame->pts;

        frame->sample_aspect_ratio = av_guess_sample_aspect_ratio(videostate->ic, videostate->video_st, frame);

        if (framedrop>0 || (framedrop && Clock::get_master_sync_type(videostate) != AV_SYNC_VIDEO_MASTER)) {
            if (frame->pts != AV_NOPTS_VALUE) {
                double diff = dpts - Clock::get_master_clock(videostate);
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

int Video::queue_picture(VideoState *videostate, AVFrame *src_frame, double pts, double duration, int64_t pos, int serial)
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

    Window::set_default_window_size(vp->width, vp->height, vp->sar);

    av_frame_move_ref(vp->frame, src_frame);
    frame_queue_push(&videostate->pictq);
    return 0;
}

void Video::video_refresh(void *arg, double *remaining_time)
{
    VideoState *videostate = (VideoState *)arg;
    double time;

    Frame *sp, *sp2;

    if (!videostate->paused && Clock::get_master_sync_type(videostate) == AV_SYNC_EXTERNAL_CLOCK && videostate->realtime)
        Clock::check_external_clock_speed(videostate);

    if (videostate->video_st) {
retry:
        if (frame_queue_nb_remaining(&videostate->pictq) == 0) {
            // nothing to do, no picture to display in the queue
            Video::present();
        }
        else {
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

            /* compute nominal last_duration */
            last_duration = Clock::vp_duration(videostate, lastvp, vp);
            delay = Clock::compute_target_delay(last_duration, videostate);

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
                duration = Clock::vp_duration(videostate, vp, nextvp);
                if(!videostate->step && (framedrop>0 || (framedrop && Clock::get_master_sync_type(videostate) != AV_SYNC_VIDEO_MASTER)) && time > videostate->frame_timer + duration){
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
                        frame_queue_next(&videostate->subpq);
                    } else {
                        break;
                    }
                }
            }
            
            frame_queue_next(&videostate->pictq);
            videostate->force_refresh = 1;

            if (videostate->step && !videostate->paused)
                SeekPause::stream_toggle_pause(videostate);
        }
display:
        /* display picture */
        if (!display_disable && videostate->force_refresh)
            video_display(videostate);
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
                av_diff = Clock::get_clock(&videostate->audclk) - Clock::get_clock(&videostate->vidclk);
            else if (videostate->video_st)
                av_diff = Clock::get_master_clock(videostate) - Clock::get_clock(&videostate->vidclk);
            else if (videostate->audio_st)
                av_diff = Clock::get_master_clock(videostate) - Clock::get_clock(&videostate->audclk);
            
            av_bprint_init(&buf, 0, AV_BPRINT_SIZE_AUTOMATIC);
            fflush(stderr);
            av_bprint_finalize(&buf, NULL);

            last_time = cur_time;
        }
    }
}

void Video::update_video_pts(VideoState *videostate, double pts, int64_t pos, int serial) {
    /* update current video pts */
    Clock::set_clock(&videostate->vidclk, pts, serial);
    Clock::sync_clock_to_slave(&videostate->extclk, &videostate->vidclk);
}

void Video::video_display(VideoState *videostate)
{
    if (!videostate->window_opened)
        video_open(videostate);
    VideoRenderer::clear();
    if (videostate->video_st)
        video_image_display(videostate);
    present();
}

void Video::present()
{
    VideoRenderer::present();
}

int Video::video_open(VideoState *videostate)
{
    int w,h;

    w = screen_width ? screen_width : default_width;
    if(w < 645)
        w = 645;
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
    videostate->window_opened = 1;

    return 0;
}

int Video::compute_mod(int a, int b)
{
    return a < 0 ? a%b + b : a%b;
}

void Video::video_image_display(VideoState *videostate)
{
    Frame *vp;
    SDL_Rect rect;

    // Subtitle compositing is not wired into the GL path yet - it needs its
    // own (non-upscaled) pass so FSR doesn't run over subtitle text.
    vp = frame_queue_peek_last(&videostate->pictq);

    int out_w, out_h;
    SDL_GL_GetDrawableSize(window, &out_w, &out_h);
    Window::calculate_display_rect(&rect, videostate->xleft, videostate->ytop, out_w, out_h, vp->width, vp->height, vp->sar);

    if (!vp->uploaded) {
        if (!VideoRenderer::upload_frame(vp->frame, &videostate->img_convert_ctx))
            return;
        vp->uploaded = 1;
        vp->flip_v = vp->frame->linesize[0] < 0;
    }

    VideoRenderer::draw(rect, out_w, out_h, vp->flip_v != 0);
}