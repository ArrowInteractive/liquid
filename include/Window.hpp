#ifndef WINDOW
#define WINDOW

#include <SDL2/SDL.h>
#include <iostream>
using namespace std;

class Window
{
private:
    SDL_Window* m_windowptr;
    const char* m_title;
    int m_width;
    int m_height;

public:
    SDL_DisplayMode m_displaymode;
    Window(const char *title, int width, int height);
    SDL_Window* get_window_ptr();
    void close_window();
    int get_window_height();
    int get_window_width();
};

#endif