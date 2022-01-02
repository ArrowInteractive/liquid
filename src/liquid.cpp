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
            cout<<"File doesn't exist."<<endl;
            //Display some error
        }

        //Check if it is a file or a folder
        if(!is_regular_file(argv[1]))
        {
            // Write some code to load files from that folder
            // For now just display some error.
            cout<<"Unable to open file!"<<endl;
            return -1;
        }

        // Initializing AVFormatContext
        if(!init_ctx())
        {
            cout<<"Couldn't allocate AVFormatContext!"<<endl;
            return -1;
        }

        // Open Input
        if(!open_input(argv[1]))
        {
            cout<<"Couldn't open source media!"<<endl;
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
