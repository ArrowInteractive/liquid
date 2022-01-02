#ifndef LOADFRAME
#define LOADFRAME

#include <iostream>
using namespace std;

extern "C"
{
    #include <libavcodec/avcodec.h>
    #include <libavformat/avformat.h>
    #include <inttypes.h>
}

bool load_frames(const char* _filename, int* width, int* height, unsigned char** data);

#endif