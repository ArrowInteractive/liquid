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
    ~Window();
    SDL_Window* getWindowptr();
    void InitWindow();
    void closeWindow();
};

#endif