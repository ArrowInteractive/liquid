#include "Event.hpp"
#include "SeekPause.hpp"
#include "Video.hpp"
#include "utils/Clock.hpp"
#include "Stream.hpp"

int step;
int is_ui_init = 0;
double pos;
double incr;
double frac;
double seek_time;
double master_clock;
SDL_TimerID ui_draw_timer;

std::string current_hour;
std::string current_min;
std::string current_sec;

void Event::event_loop(VideoState *videostate)
{
    SDL_Event event;
    double incr, frac;
    for (;;) {
        double x;
        Event::refresh_loop_wait_event(videostate, &event);

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
                Event::toggle_full_screen(videostate);
                videostate->force_refresh = 1;
                break;
            case SDLK_p:
            case SDLK_SPACE:
                Event::toggle_pause(videostate);
                break;
            case SDLK_m:
                Event::toggle_mute(videostate);
                break;
            case SDLK_KP_MULTIPLY:
            case SDLK_0:
                step = 1;
                Event::update_volume(videostate);
                break;
            case SDLK_KP_DIVIDE:
            case SDLK_9:
                step = -1;
                Event::update_volume(videostate);
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
            case SDLK_PAGEUP:
                if (videostate->ic->nb_chapters <= 1) {
                    incr = 600.0;
                    SeekPause::execute_seek(videostate, incr);
                }
                Event::seek_chapter(videostate, 1);
                break;
            case SDLK_PAGEDOWN:
                if (videostate->ic->nb_chapters <= 1) {
                    incr = -600.0;
                    SeekPause::execute_seek(videostate, incr);
                }
                Event::seek_chapter(videostate, -1);
                break;
            case SDLK_LEFT:
                incr = seek_interval ? -seek_interval : -10.0;
                SeekPause::execute_seek(videostate, incr);
                break;
            case SDLK_RIGHT:
                incr = seek_interval ? seek_interval : 10.0;
                SeekPause::execute_seek(videostate, incr);
                break;
            case SDLK_UP:
                incr = 60.0;
                SeekPause::execute_seek(videostate, incr);
                break;
            case SDLK_DOWN:
                incr = -60.0;
                SeekPause::execute_seek(videostate, incr);
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
                        Event::toggle_full_screen(videostate);
                        videostate->force_refresh = 1;
                        last_mouse_left_click = 0;
                    } else {
                        last_mouse_left_click = av_gettime_relative();
                    }
                }
            }
        case SDL_MOUSEMOTION:
            SDL_RemoveTimer(ui_draw_timer);
            draw_ui = true;
            cursor_hidden = 1;
            ui_draw_timer = SDL_AddTimer(3000, Event::hide_ui, (void *)false);
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
                    std::cout<<"Seeking to "<<frac*100<<":"<<hh<<":"<<mm<<":"<<ss<<":"<<thh<<":"<<tmm<<":"<<tss<<std::endl;
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
        imgui_event_handler(event);
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
        
        // Display last received frame here
        if(videostate->paused){
            if (videostate->video_st)
                Video::video_image_display(videostate);
            
            update_imgui(renderer, videostate->width, videostate->height);
            SDL_RenderPresent(renderer);
        }

        // Handle UI events
        if(req_pause){
            Event::toggle_pause(videostate);
            req_pause = false;
        }

        if(req_seek){
            SeekPause::execute_seek(videostate, ui_incr);
            req_seek = false;
        }

        if(req_mute){
            Event::toggle_mute(videostate);
            req_mute = false;
        }

        if(req_audio_track_change){
            Event::stream_cycle_channel(videostate, AVMEDIA_TYPE_AUDIO);
            req_audio_track_change = false;
        }

        if(req_sub_track_change){
            Event::stream_cycle_channel(videostate, AVMEDIA_TYPE_SUBTITLE);
            req_sub_track_change = false;
        }

        if(req_seek_progress){
            seek_time = ((progress_var * avformat_ctx->duration/1000000)/100);
            master_clock = Clock::get_master_clock(videostate);
            if(seek_time < master_clock){
                seek_time = master_clock - seek_time;
                SeekPause::execute_seek(videostate, -seek_time);
            }   
            else if(seek_time > master_clock){
                seek_time = seek_time - master_clock;
                SeekPause::execute_seek(videostate, seek_time);
            }
            req_seek_progress = !req_seek_progress;
        }

        if(vol_change){
            videostate->audio_volume = sound_var;
            vol_change = false;
        }

        // Calculate clock values
        if(Clock::get_master_clock(videostate) <= (double)avformat_ctx->duration/1000000){
            cur_hur = Clock::get_master_clock(videostate)/3600;
            cur_tim = ((int)Clock::get_master_clock(videostate))%3600;
            cur_min = cur_tim/60;
            cur_tim = ((int)cur_tim)%60;
            cur_sec = cur_tim;
        }
        
        if (cur_hur < 10)
            current_hour = "0" + std::to_string(cur_hur);
        else
            current_hour = std::to_string(cur_hur);

        if(cur_min < 10)
            current_min = "0" + std::to_string(cur_min);
        else
            current_min = std::to_string(cur_min);

        if(cur_sec < 10)
            current_sec = "0" + std::to_string(cur_sec);
        else
            current_sec = std::to_string(cur_sec);

        current_time = current_hour + ":" + current_min + ":" + current_sec;
        progress_var = (float)(Clock::get_master_clock(videostate) * 100 ) / ((double)avformat_ctx->duration/1000000);
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
    
    // Update UI sound var
    sound_var = videostate->audio_volume;

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
    Stream::stream_component_close(videostate, old_index);
    Stream::stream_component_open(videostate, stream_index);
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

    std::cout<<"LOG: Seeking to chapter "<<i<<std::endl;
    SeekPause::stream_seek(videostate, av_rescale_q(videostate->ic->chapters[i]->start, videostate->ic->chapters[i]->time_base,
                                 AV_TIME_BASE_Q), 0, 0);
}

Uint32 Event::hide_ui(Uint32 interval,void* param)
{
    draw_ui = false;
    SDL_ShowCursor(SDL_DISABLE);
    return 0;
}