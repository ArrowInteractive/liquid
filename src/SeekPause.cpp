#include "SeekPause.hpp"
#include "Clock.hpp"

void SeekPause::step_to_next_frame(VideoState *videostate)
{
    /* If the stream is paused unpause it, then step */
    if (videostate->paused)
        SeekPause::stream_toggle_pause(videostate);
    videostate->step = 1;
}

/* Pause or resume the video */
void SeekPause::stream_toggle_pause(VideoState *videostate)
{
    if (videostate->paused) {
        videostate->frame_timer += av_gettime_relative() / 1000000.0 - videostate->vidclk.last_updated;
        if (videostate->read_pause_return != AVERROR(ENOSYS)) {
            videostate->vidclk.paused = 0;
        }
        Clock::set_clock(&videostate->vidclk, Clock::get_clock(&videostate->vidclk), videostate->vidclk.serial);
    }
    Clock::set_clock(&videostate->extclk, Clock::get_clock(&videostate->extclk), videostate->extclk.serial);
    videostate->paused = videostate->audclk.paused = videostate->vidclk.paused = videostate->extclk.paused = !videostate->paused;
}

void SeekPause::stream_seek(VideoState *videostate, int64_t pos, int64_t rel, int seek_by_bytes)
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

void SeekPause::execute_seek(VideoState *videostate, double incr)
{
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
    } 
    else {
        pos = Clock::get_master_clock(videostate);
        if (isnan(pos))
            pos = (double)videostate->seek_pos / AV_TIME_BASE;
        pos += incr;
        if (videostate->ic->start_time != AV_NOPTS_VALUE && pos < videostate->ic->start_time / (double)AV_TIME_BASE)
            pos = videostate->ic->start_time / (double)AV_TIME_BASE;
        stream_seek(videostate, (int64_t)(pos * AV_TIME_BASE), (int64_t)(incr * AV_TIME_BASE), 0);
    }
}
