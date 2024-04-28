
#include "Stream.hpp"
#include "utils/Clock.hpp"
#include "utils/Thread.hpp"
#include "utils/Audio.hpp"
#include "Decoder.hpp"



VideoState *Stream::stream_open(char *filename)
{
    std::string hour;
    std::string min;
    std::string sec;

    VideoState *videostate;

    videostate = (VideoState *)av_mallocz(sizeof(VideoState));
    if (!videostate)
        return NULL;
    
    avformat_ctx = avformat_alloc_context();
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

    int hours, mins, secs, us;

    if (avformat_ctx->duration != AV_NOPTS_VALUE) {
        
        int64_t duration = avformat_ctx->duration + 5000;
        secs  = duration / AV_TIME_BASE;
        us    = duration % AV_TIME_BASE;
        mins  = secs / 60;   
        secs %= 60;          
        hours = mins / 60;   
        mins %= 60;
    }


    if(hours < 10)
        hour = "0"+std::to_string(hours);
    else
        hour = std::to_string(hours);

    if(mins < 10)
        min = "0"+std::to_string(mins);
    else
        min = std::to_string(mins);

    if(secs < 10)
        sec = "0"+std::to_string(secs);
    else
        sec = std::to_string(secs);

    max_video_duration = hour+":"+min+":"+sec;


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

    Clock::init_clock(&videostate->vidclk, &videostate->videoq.serial);
    Clock::init_clock(&videostate->audclk, &videostate->audioq.serial);
    Clock::init_clock(&videostate->extclk, &videostate->extclk.serial);
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
    videostate->read_tid     = SDL_CreateThread(Thread::read_thread, "read_thread", videostate);
    if (!videostate->read_tid) {
        std::cout<<"FATAL ERROR: SDL_CreateThread() failed!"<<SDL_GetError()<<std::endl;
fail:
        Stream::stream_close(videostate);
        return NULL;
    }
    return videostate;
}

void Stream::stream_close(VideoState *videostate)
{
    /* XXX: use a special url_shutdown call to abort parse cleanly */
    videostate->abort_request = 1;
    SDL_WaitThread(videostate->read_tid, NULL);

    /* close each stream */
    if (videostate->audio_stream >= 0)
        Stream::stream_component_close(videostate, videostate->audio_stream);
    if (videostate->video_stream >= 0)
        Stream::stream_component_close(videostate, videostate->video_stream);
    if (videostate->subtitle_stream >= 0)
        Stream::stream_component_close(videostate, videostate->subtitle_stream);

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
    if (videostate->vid_texture)
        SDL_DestroyTexture(videostate->vid_texture);
    if (videostate->sub_texture)
        SDL_DestroyTexture(videostate->sub_texture);
    av_free(videostate);
}

int Stream::stream_component_open(VideoState *videostate, int stream_index)
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
        if ((ret = Audio::audio_open(videostate, channel_layout, nb_channels, sample_rate, &videostate->audio_tgt)) < 0)
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

        if ((ret = Decoder::decoder_init(&videostate->auddec, avctx, &videostate->audioq, videostate->continue_read_thread)) < 0)
            goto fail;
        if ((videostate->ic->iformat->flags & (AVFMT_NOBINSEARCH | AVFMT_NOGENSEARCH | AVFMT_NO_BYTE_SEEK)) && !videostate->ic->iformat->read_seek) {
            videostate->auddec.start_pts = videostate->audio_st->start_time;
            videostate->auddec.start_pts_tb = videostate->audio_st->time_base;
        }
        if ((ret = Decoder::decoder_start(&videostate->auddec, Thread::audio_thread, "audio_decoder", videostate)) < 0)
            goto out;
        SDL_PauseAudioDevice(audio_dev, 0);
        break;
    case AVMEDIA_TYPE_VIDEO:
        videostate->video_stream = stream_index;
        videostate->video_st = ic->streams[stream_index];

        if ((ret = Decoder::decoder_init(&videostate->viddec, avctx, &videostate->videoq, videostate->continue_read_thread)) < 0)
            goto fail;
        if ((ret = Decoder::decoder_start(&videostate->viddec, Thread::video_thread, "video_decoder", videostate)) < 0)
            goto out;
        videostate->queue_attachments_req = 1;
        break;
    case AVMEDIA_TYPE_SUBTITLE:
        videostate->subtitle_stream = stream_index;
        videostate->subtitle_st = ic->streams[stream_index];

        if ((ret = Decoder::decoder_init(&videostate->subdec, avctx, &videostate->subtitleq, videostate->continue_read_thread)) < 0)
            goto fail;
        
        if ((ret = Decoder::decoder_start(&videostate->subdec, Thread::subtitle_thread, "subtitle_decoder", videostate)) < 0)
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

void Stream::stream_component_close(VideoState *videostate, int stream_index)
{
    AVFormatContext *ic = videostate->ic;
    AVCodecParameters *codecpar;

    if (stream_index < 0 || stream_index >= ic->nb_streams)
        return;
    codecpar = ic->streams[stream_index]->codecpar;

    switch (codecpar->codec_type) {
    case AVMEDIA_TYPE_AUDIO:
        Decoder::decoder_abort(&videostate->auddec, &videostate->sampq);
        SDL_CloseAudioDevice(audio_dev);
        Decoder::decoder_destroy(&videostate->auddec);
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
        Decoder::decoder_abort(&videostate->viddec, &videostate->pictq);
        Decoder::decoder_destroy(&videostate->viddec);
        break;
    case AVMEDIA_TYPE_SUBTITLE:
        Decoder::decoder_abort(&videostate->subdec, &videostate->subpq);
        Decoder::decoder_destroy(&videostate->subdec);
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

int Stream::stream_has_enough_packets(AVStream *st, int stream_id, PacketQueue *queue) {
    return stream_id < 0 ||
           queue->abort_request ||
           (st->disposition & AV_DISPOSITION_ATTACHED_PIC) ||
           queue->nb_packets > MIN_FRAMES && (!queue->duration || av_q2d(st->time_base) * queue->duration > 1.0);
}