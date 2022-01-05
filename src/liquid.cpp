#include "window.hpp"
#include "loaddata.hpp"
#include <iostream>
#include <filesystem>
using namespace std;
using std::filesystem::exists;
using std::filesystem::is_directory;
using std::filesystem::is_regular_file;

int main(int argc, char** argv)
{
    // Variables
    AVCodec* av_codec;
    AVCodecContext* av_codec_ctx;
    AVFormatContext* av_format_ctx;
    AVCodecParameters* av_codec_params;
    
    int _frame_width, _frame_height;
    unsigned char* _frame_data;
    window* gl_window;

    // Check for args
    if(argc < 2)
    {
        gl_window = new window(1280, 720, "Liquid Media Player");
    }
    else
    {
        if(!exists(argv[1]))
        {
            //Display some error
            cout<<"File doesn't exist."<<endl;
            return -1;
        }

        //Check if it is a file or a folder
        if(!is_regular_file(argv[1]))
        {
            // Write some code to load files from that folder
            // For now just display some error.
            cout<<"Provided input is a folder."<<endl;
            return -1;
        }

        // Loading data
        if(!(load_data(argv[1], av_format_ctx, av_codec_params, av_codec)))
        {
            return -1;
        }

        // New window
        gl_window = new window(1280, 720, argv[1]);
    }

    //Initialize the window
    gl_window->initWindow();
    
    // Main loop
    while(gl_window->isWindowNotClosed())
    {
        gl_window->updateWindow();
    }

    // Deallocate
    gl_window->destroyWindow();
    delete(gl_window);

    return 0;
}
