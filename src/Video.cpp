#include "Video.hpp"
#include "Decode.hpp"
#include "utils/Clock.hpp"
#include "Window.hpp"
#include "SeekPause.hpp"


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
            update_imgui(renderer, videostate->width, videostate->height);
            SDL_RenderPresent(renderer);
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
    if (!videostate->width)
        video_open(videostate);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);
    if (videostate->video_st)
        video_image_display(videostate);
    update_imgui(renderer, videostate->width, videostate->height);
    SDL_RenderPresent(renderer);
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

    return 0;
}

void Video::fill_rectangle(int x, int y, int w, int h)
{
    SDL_Rect rect;
    rect.x = x;
    rect.y = y;
    rect.w = w;
    rect.h = h;
    if (w && h)
        SDL_RenderFillRect(renderer, &rect);
}

int Video::compute_mod(int a, int b)
{
    return a < 0 ? a%b + b : a%b;
}

int Video::realloc_texture(SDL_Texture **texture, Uint32 new_format, int new_width, int new_height, SDL_BlendMode blendmode, int init_texture)
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

void Video::video_image_display(VideoState *videostate)
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

    Window::calculate_display_rect(&rect, videostate->xleft, videostate->ytop, videostate->width, videostate->height, vp->width, vp->height, vp->sar);

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

int Video::upload_texture(SDL_Texture **tex, AVFrame *frame, struct SwsContext **img_convert_ctx) {
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

void Video::set_sdl_yuv_conversion_mode(AVFrame *frame)
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

void Video::get_sdl_pix_fmt_and_blendmode(int format, Uint32 *sdl_pix_fmt, SDL_BlendMode *sdl_blendmode)
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