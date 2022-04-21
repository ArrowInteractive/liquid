#pragma once
#include "datastate.hpp"

class Decode
{
    public:
        static int decode_interrupt_cb(void *ctx);
        static int decoder_decode_frame(DecoderBase *d, AVFrame *frame, AVSubtitle *sub);
};