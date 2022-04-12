#pragma once
#include "datastate.hpp"

class Decoder
{
    public:
        static int decoder_init(DecoderBase *d, AVCodecContext *avctx, PacketQueue *queue, SDL_cond *empty_queue_cond);
        static int decoder_start(DecoderBase *d, int (*fn)(void *), const char *thread_name, void* arg);
        static void decoder_abort(DecoderBase *d, FrameQueue *fq);
        static void decoder_destroy(DecoderBase *d);
};