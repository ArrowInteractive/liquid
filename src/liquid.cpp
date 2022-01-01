#include "window.hpp"
#include <iostream>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
using namespace std;

int main(int argc, const char** argv)
{
    window* gl_window = new window(1280, 720, "Liquid Media Player");
    gl_window->initWindow();

    while(gl_window->isWindowNotClosed())
    {
        gl_window->updateWindow();
    }
    gl_window->destroyWindow();
    delete(gl_window);

    return 0;
}
