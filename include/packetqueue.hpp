#pragma once

// Includes
extern "C"{
    #include "libavutil/avstring.h"
    #include "libavutil/channel_layout.h"
    #include "libavutil/eval.h"
    #include "libavutil/mathematics.h"
    #include "libavutil/pixdesc.h"
    #include "libavutil/imgutils.h"
    #include "libavutil/dict.h"
    #include "libavutil/fifo.h"
    #include "libavutil/parseutils.h"
    #include "libavutil/samplefmt.h"
    #include "libavutil/time.h"
    #include "libavutil/bprint.h"
    #include "libavformat/avformat.h"
    #include "libavdevice/avdevice.h"
    #include "libswscale/swscale.h"
    #include "libavutil/opt.h"
    #include "libavcodec/avfft.h"
    #include "libswresample/swresample.h"
}

#include <iostream>
#include <SDL2/SDL.h>

// Structs
struct LiquidAVPacketList {
    AVPacket *pkt;
    int serial;
};

struct PacketQueue{
    AVFifoBuffer *pkt_list;
    int nb_packets;
    int size;
    int64_t duration;
    int abort_request;
    int serial;
    SDL_mutex *mutex;
    SDL_cond *cond;
};

// Functions
int packet_queue_init(PacketQueue *q);
void packet_queue_start(PacketQueue *q);
int packet_queue_put(PacketQueue *q, AVPacket *pkt);
int packet_queue_put_private(PacketQueue *q, AVPacket *pkt);
int packet_queue_put_nullpacket(PacketQueue *q, AVPacket *pkt, int stream_index);
int packet_queue_get(PacketQueue *q, AVPacket *pkt, int block, int *serial);
void packet_queue_abort(PacketQueue *q);
void packet_queue_destroy(PacketQueue *q);
void packet_queue_flush(PacketQueue *q);