#include "Event.hpp"
#include "SeekPause.hpp"
#include "Video.hpp"
#include "utils/Clock.hpp"
#include "Stream.hpp"
#include "gl/VideoRenderer.hpp"
#include "utils/Log.hpp"
#include <string>
#include <cstdio>

int step;
double pos;
double incr;
double frac;

namespace {

// Logs and shows an on-screen toast for the same message - the pattern
// every state-changing key handler below wants, so player actions (pause,
// volume, seek, stream switches, upscaler/scale changes) get visible
// feedback instead of only a console line nobody's looking at.
void show_toast(const std::string &msg)
{
    Log::info() << msg;
    VideoRenderer::show_overlay(msg);
}

std::string fmt_seek(double seconds)
{
    char buf[32];
    snprintf(buf, sizeof buf, "Seek: %+.0fs", seconds);
    return buf;
}

} // namespace

void Event::event_loop(VideoState *videostate)
{
    SDL_Event event;
    double incr, frac;
    for (;;) {
        double x;
        Event::refresh_loop_wait_event(videostate, &event);

        switch (event.type) {
        case SDL_KEYDOWN:
            if (exit_on_keydown || event.key.keysym.sym == SDLK_q) {
                do_exit(videostate);
                break;
            }
            // If we don't yet have a window, skip all key events, because read_thread might still be initializing...
            if (!videostate->width)
                continue;
            switch (event.key.keysym.sym) {
            case SDLK_ESCAPE:
                // Only exits fullscreen, doesn't quit - use Q to quit.
                if (is_full_screen) {
                    Event::toggle_full_screen(videostate);
                    videostate->force_refresh = 1;
                }
                break;
            case SDLK_f:
                Event::toggle_full_screen(videostate);
                videostate->force_refresh = 1;
                break;
            case SDLK_p:
            case SDLK_SPACE:
                Event::toggle_pause(videostate);
                show_toast(videostate->paused ? "Paused" : "Playing");
                break;
            case SDLK_m:
                Event::toggle_mute(videostate);
                show_toast(videostate->muted ? "Muted" : "Unmuted");
                break;
            case SDLK_KP_MULTIPLY:
            case SDLK_0:
                step = 1;
                Event::update_volume(videostate);
                show_toast("Volume: " + std::to_string(videostate->audio_volume * 100 / SDL_MIX_MAXVOLUME) + "%");
                break;
            case SDLK_KP_DIVIDE:
            case SDLK_9:
                step = -1;
                Event::update_volume(videostate);
                show_toast("Volume: " + std::to_string(videostate->audio_volume * 100 / SDL_MIX_MAXVOLUME) + "%");
                break;
            case SDLK_s: // S: Step to next frame
                SeekPause::step_to_next_frame(videostate);
                break;
            case SDLK_a:
                Event::stream_cycle_channel(videostate, AVMEDIA_TYPE_AUDIO);
                break;
            case SDLK_v:
                Event::stream_cycle_channel(videostate, AVMEDIA_TYPE_VIDEO);
                break;
            case SDLK_c:
                Event::stream_cycle_channel(videostate, AVMEDIA_TYPE_VIDEO);
                Event::stream_cycle_channel(videostate, AVMEDIA_TYPE_AUDIO);
                Event::stream_cycle_channel(videostate, AVMEDIA_TYPE_SUBTITLE);

                /*
                **  Work around for channel switch distortion 
                */
                incr = seek_interval ? -seek_interval : -1.0;
                SeekPause::execute_seek(videostate, incr);
                break;
            case SDLK_t:
                Event::stream_cycle_channel(videostate, AVMEDIA_TYPE_SUBTITLE);
                break;
            case SDLK_u:
                // Deliberately NOT setting force_refresh here: while paused
                // that would make refresh_loop_wait_event() call
                // Video::video_refresh(), which doesn't check pause state
                // before comparing wall-clock time against the current
                // frame's schedule - since real time kept moving while
                // paused, it looks "overdue" and advances the frame queue.
                // The paused branch already redraws the current frame every
                // ~10ms regardless, so the toggle shows up on its own.
                VideoRenderer::cycle_upscaler();
                show_toast(std::string("Upscaler: ") + VideoRenderer::upscaler_name());
                break;
            case SDLK_r:
                // Same reasoning as SDLK_u above: no force_refresh, the
                // paused branch's unconditional redraw picks it up on its own.
                VideoRenderer::cycle_render_scale();
                show_toast(std::string("Scale preset: ") + VideoRenderer::render_scale_name());
                break;
            case SDLK_h:
                // No force_refresh needed here either - same reasoning as
                // SDLK_u/SDLK_r.
                VideoRenderer::toggle_help();
                break;
            case SDLK_PAGEUP:
                if (videostate->ic->nb_chapters <= 1) {
                    incr = 600.0;
                    SeekPause::execute_seek(videostate, incr);
                    show_toast(fmt_seek(incr));
                }
                Event::seek_chapter(videostate, 1);
                break;
            case SDLK_PAGEDOWN:
                if (videostate->ic->nb_chapters <= 1) {
                    incr = -600.0;
                    SeekPause::execute_seek(videostate, incr);
                    show_toast(fmt_seek(incr));
                }
                Event::seek_chapter(videostate, -1);
                break;
            case SDLK_LEFT:
                incr = seek_interval ? -seek_interval : -10.0;
                SeekPause::execute_seek(videostate, incr);
                show_toast(fmt_seek(incr));
                break;
            case SDLK_RIGHT:
                incr = seek_interval ? seek_interval : 10.0;
                SeekPause::execute_seek(videostate, incr);
                show_toast(fmt_seek(incr));
                break;
            case SDLK_UP:
                incr = 60.0;
                SeekPause::execute_seek(videostate, incr);
                show_toast(fmt_seek(incr));
                break;
            case SDLK_DOWN:
                incr = -60.0;
                SeekPause::execute_seek(videostate, incr);
                show_toast(fmt_seek(incr));
                break;
            default:
                break;
            }
            break;
        case SDL_MOUSEBUTTONDOWN:
            if (exit_on_mousedown) {
                do_exit(videostate);
                break;
            }
            if (event.button.button == SDL_BUTTON_LEFT) {
                static int64_t last_mouse_left_click = 0;
                if (av_gettime_relative() - last_mouse_left_click <= 500000) {
                    Event::toggle_full_screen(videostate);
                    videostate->force_refresh = 1;
                    last_mouse_left_click = 0;
                } else {
                    last_mouse_left_click = av_gettime_relative();
                }
            }
        case SDL_MOUSEMOTION:
            cursor_hidden = 1;
            if (cursor_hidden) {
                SDL_ShowCursor(SDL_ENABLE);
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
                    SeekPause::stream_seek(videostate, size*x/videostate->width, 0, 1);
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
                    Log::info() << "Seeking to " << frac*100 << ":" << hh << ":" << mm << ":" << ss << ":" << thh << ":" << tmm << ":" << tss;
                    ts = frac * videostate->ic->duration;
                    if (videostate->ic->start_time != AV_NOPTS_VALUE)
                        ts += videostate->ic->start_time;
                    SeekPause::stream_seek(videostate, ts, 0, 0);
                }
            break;
        case SDL_WINDOWEVENT:
            switch (event.window.event) {
                case SDL_WINDOWEVENT_SIZE_CHANGED:
                    screen_width  = videostate->width  = event.window.data1;
                    screen_height = videostate->height = event.window.data2;
                case SDL_WINDOWEVENT_EXPOSED:
                    videostate->force_refresh = 1;
            }
            break;
        case SDL_QUIT:
            do_exit(videostate);
            break;
        case FF_QUIT_EVENT:
            do_exit(videostate);
            break;
        default:
            break;
        }
    }
}

void Event::refresh_loop_wait_event(VideoState *videostate, SDL_Event *event) 
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
            Video::video_refresh(videostate, &remaining_time);
        
        // Display last received frame here. Goes through video_display()
        // (not a direct video_image_display()+present()) so the frame gets
        // cleared first - otherwise repeated redraws while paused never
        // erase anything outside the letterboxed video rect (e.g. the
        // overlay toast, or the black bars for non-matching aspect ratios),
        // since the composite pass only repaints the rect itself.
        if(videostate->paused)
            Video::video_display(videostate);

        SDL_PumpEvents();
    }
}

void Event::toggle_full_screen(VideoState *videostate)
{
    is_full_screen = !is_full_screen;
    SDL_SetWindowFullscreen(window, is_full_screen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
}

void Event::toggle_pause(VideoState *videostate)
{
    SeekPause::stream_toggle_pause(videostate);
    videostate->step = 0;
}

void Event::toggle_mute(VideoState *videostate)
{
    videostate->muted = !videostate->muted;
}

void Event::update_volume(VideoState *videostate)
{
    if(step == -1 && videostate->audio_volume > 0){
        // Decrease the volume
        videostate->audio_volume--;
    }
    else if(step == 1 && videostate->audio_volume < SDL_MIX_MAXVOLUME){
        // Increase volume
        videostate->audio_volume++;
    }

    step = 0;
}

void Event::stream_cycle_channel(VideoState *videostate, int codec_type)
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
                    st->codecpar->ch_layout.nb_channels != 0)
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
    Stream::stream_component_close(videostate, old_index);
    Stream::stream_component_open(videostate, stream_index);

    const char *type_name = (codec_type == AVMEDIA_TYPE_AUDIO) ? "Audio" :
                             (codec_type == AVMEDIA_TYPE_VIDEO) ? "Video" : "Subtitle";
    if (stream_index == -1)
        show_toast(std::string(type_name) + ": off");
    else
        show_toast(std::string(type_name) + " stream: " + std::to_string(stream_index));
}

void Event::seek_chapter(VideoState *videostate, int incr)
{
    int64_t pos = Clock::get_master_clock(videostate) * AV_TIME_BASE;
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

    show_toast("Chapter " + std::to_string(i));
    SeekPause::stream_seek(videostate, av_rescale_q(videostate->ic->chapters[i]->start, videostate->ic->chapters[i]->time_base,
                                 AV_TIME_BASE_Q), 0, 0);
}