#include "Input.hpp"

bool Input::is_window_running()
{
    return m_isrunning;
}

void Input::update_events()
{
    SDL_PollEvent(&event);
    if(event.type == SDL_QUIT)
    {
        m_isrunning = false;
    }
    if(event.type == SDL_KEYDOWN)
    {
        if(event.key.keysym.sym == SDLK_q)
        {
            m_isrunning = false;
        }
    }
}