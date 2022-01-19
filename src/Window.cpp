#include "Window.hpp"

Window::Window(const char *title, int width, int height) : 
    m_title(title),
    m_width(width),
    m_height(height)
{
    m_title = title;
    m_width = width;
    m_height = height;

    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER);
    SDL_GetDesktopDisplayMode(0, &m_displaymode);

    m_windowptr = SDL_CreateWindow(
        m_title,
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        m_width,
        m_height,
        SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN);
    if (m_windowptr == NULL)
    {
        std::cout<<"ERROR::Could not initialize window!"<<std::endl;
    }
    std::cout<<"LOG::Initialized window"<<std::endl;
}
SDL_Window *Window::get_window_ptr()
{
    return m_windowptr;
}

int Window::get_window_height()
{
    return m_height;
}

int Window::get_window_width()
{
    return m_width;
}

void Window::close_window()
{
    SDL_DestroyWindow(m_windowptr);
    std::cout<<"LOG::Destroyed window."<<std::endl;
}