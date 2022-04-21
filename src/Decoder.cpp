#include "Decoder.hpp"

int Decoder::decoder_init(DecoderBase *d, AVCodecContext *avctx, PacketQueue *queue, SDL_cond *empty_queue_cond)
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

int Decoder::decoder_start(DecoderBase *d, int (*fn)(void *), const char *thread_name, void* arg)
{
    packet_queue_start(d->queue);
    d->decoder_tid = SDL_CreateThread(fn, thread_name, arg);
    if (!d->decoder_tid) {
        std::cout<<"FATAL ERROR: SDL_CreateThread() failed!"<<SDL_GetError()<<std::endl;
        return AVERROR(ENOMEM);
    }
    return 0;
}

void Decoder::decoder_abort(DecoderBase *d, FrameQueue *fq)
{
    packet_queue_abort(d->queue);
    frame_queue_signal(fq);
    SDL_WaitThread(d->decoder_tid, NULL);
    d->decoder_tid = NULL;
    packet_queue_flush(d->queue);
}

void Decoder::decoder_destroy(DecoderBase *d) 
{
    av_packet_free(&d->pkt);
    avcodec_free_context(&d->avctx);
}