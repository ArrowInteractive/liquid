#include "Renderer.hpp"

Renderer::Renderer(Window *window) : m_window(window)
{
    m_window = window;
}

Renderer::~Renderer()
{
    closeRenderer();
}

void Renderer::InitRenderer()
{
    m_renderer = SDL_CreateRenderer(
        m_window->getWindowptr(),
        -1,
        SDL_RENDERER_ACCELERATED);
}

void Renderer::RenderImage()
{
    closeRenderer();
}

void Renderer::clearRenderer()
{
    SDL_RenderClear(m_renderer);
}

void Renderer::render()
{
    SDL_RenderPresent(m_renderer);
}

void Renderer::closeRenderer()
{
    SDL_DestroyRenderer(m_renderer);
    std::cout<<"LOG::DESTROY RENDERER"<<std::endl;
}
