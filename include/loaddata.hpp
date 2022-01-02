#ifndef LOADDATA
#define LOADDATA

#include <iostream>
using namespace std;

extern "C"
{
    #include <libavcodec/avcodec.h>
    #include <libavformat/avformat.h>
    #include <inttypes.h>
}

// Functions
bool init_ctx();
bool open_input(char* _filename);

#endif