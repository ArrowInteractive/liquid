#include "window.hpp"
#include "loadframes.hpp"
#include <iostream>
using namespace std;

int main(int argc, const char** argv)
{
    int frame_width, frame_height;
    unsigned char* frame_data;
    window* gl_window = new window(1280, 720, "Liquid Media Player");
    gl_window->initWindow();

    if(!load_frames("E:\\Projects\\liquid\\build\\sample.mp4", &frame_width, &frame_height, &frame_data))
    {
        cout<<"Couldn't load media frames!"<<endl;
        return 1;
    }

    while(gl_window->isWindowNotClosed())
    {
        gl_window->updateWindow();
    }
    gl_window->destroyWindow();
    delete(gl_window);

    return 0;
}
