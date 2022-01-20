#ifndef INPUT
#define INPUT

#include <SDL2/SDL.h>

class Input
{
private:
    bool m_isrunning = true;
    SDL_Event event;

public:
    bool is_window_running();
    void update_events();
};

#endif