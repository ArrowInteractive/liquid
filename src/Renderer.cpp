#include "Renderer.hpp"

Renderer::Renderer(Window *window) : m_window(window)
{
    m_window = window;
    m_renderer = SDL_CreateRenderer(
                                        m_window->get_window_ptr(),
                                        -1,
                                        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC | SDL_RENDERER_TARGETTEXTURE
                                    );
    std::cout<<"LOG::INITIALISED RENDERER"<<std::endl;
}

void Renderer::render_copy()
{

}

void Renderer::clear_renderer()
{
    SDL_RenderClear(m_renderer);
}

void Renderer::render_present()
{
    SDL_RenderPresent(m_renderer);
}

void Renderer::destroy_renderer()
{
    SDL_DestroyRenderer(m_renderer);
    std::cout<<"LOG::DESTROY RENDERER"<<std::endl;
}

SDL_Renderer *Renderer::get_renderer()
{
    return m_renderer;
}