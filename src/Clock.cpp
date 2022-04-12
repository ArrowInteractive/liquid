#include "Clock.hpp"

void Clock::init_clock(ClockBase *c, int *queue_serial)
{
    c->speed = 1.0;
    c->paused = 0;
    c->queue_serial = queue_serial;
    set_clock(c, NAN, -1);
}

void Clock::set_clock(ClockBase *c, double pts, int serial)
{
    double time = av_gettime_relative() / 1000000.0;
    set_clock_at(c, pts, serial, time);
}

void Clock::set_clock_at(ClockBase *c, double pts, int serial, double time)
{
    c->pts = pts;
    c->last_updated = time;
    c->pts_drift = c->pts - time;
    c->serial = serial;
}

double Clock::get_clock(ClockBase *c)
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

int Clock::is_realtime(AVFormatContext *s)
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

void Clock::sync_clock_to_slave(ClockBase *c, ClockBase *slave)
{
    double clock = get_clock(c);
    double slave_clock = get_clock(slave);
    if (!isnan(slave_clock) && (isnan(clock) || fabs(clock - slave_clock) > AV_NOSYNC_THRESHOLD))
        set_clock(c, slave_clock, slave->serial);
}

int Clock::get_master_sync_type(VideoState *videostate) 
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

double Clock::get_master_clock(VideoState *videostate)
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

void Clock::check_external_clock_speed(VideoState *videostate) 
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

void Clock::set_clock_speed(ClockBase *c, double speed)
{
    set_clock(c, get_clock(c), c->serial);
    c->speed = speed;
}

double Clock::vp_duration(VideoState *videostate, Frame *vp, Frame *nextvp) {
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

double Clock::compute_target_delay(double delay, VideoState *videostate)
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