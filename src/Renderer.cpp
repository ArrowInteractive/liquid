#include "Renderer.hpp"

Renderer::Renderer(Window *window) : m_window(window)
{
    m_window = window;
}

void Renderer::init_renderer()
{
    m_renderer = SDL_CreateRenderer(
        m_window->get_window_ptr(),
        -1,
        SDL_RENDERER_ACCELERATED);

    std::cout<<"LOG::INITIALISED RENDERER"<<std::endl;
}

void Renderer::render_image()
{}

void Renderer::clear_renderer()
{
    SDL_RenderClear(m_renderer);
}

void Renderer::render()
{
    SDL_RenderPresent(m_renderer);
}

void Renderer::close_renderer()
{
    SDL_DestroyRenderer(m_renderer);
    std::cout<<"LOG::DESTROY RENDERER"<<std::endl;
}
