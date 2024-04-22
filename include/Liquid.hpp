#pragma once

/*
*   Macros
*/

#ifdef __MINGW32__
#define SDL_MAIN_HANDLED
#endif

/*
*   Includes
*/

#include "datastate.hpp"
#include <filesystem>


class Liquid
{
    public:
        Liquid(int argc, char *argv[]);
        void run();
        
    private:
        int flags;
        VideoState *videostate;
        char* input_filename;

};