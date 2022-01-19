#ifndef WINDOW
#define WINDOW

#include <SDL2/SDL.h>
#include <iostream>

class Window
{
private:
    SDL_Window *m_windowptr;
    const char *m_title;
    int m_width;
    int m_height;

public:
    Window(const char *title, int width, int height);
    SDL_Window* get_window_ptr();
    void init_window();
    void close_window();
    int get_window_height();
    int get_window_width();
};

#endif