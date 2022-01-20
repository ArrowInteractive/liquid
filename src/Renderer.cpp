#include "Renderer.hpp"

Renderer::Renderer(Window *window) : m_window(window)
{
    m_window = window;
    m_renderer = SDL_CreateRenderer(
                                        m_window->get_window_ptr(),
                                        -1,
                                        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC | SDL_RENDERER_TARGETTEXTURE
                                    );
    std::cout<<"LOG::Initialized renderer."<<std::endl;
}

void Renderer::render_texture(SDL_Texture *texture)
{
    SDL_RenderCopy(m_renderer, texture, NULL, NULL);
}

void Renderer::clear_renderer()
{
    SDL_RenderClear(m_renderer);
}

void Renderer::update_texture(SDL_Texture *texture,framedata_struct* state)
{
    SDL_UpdateYUVTexture(
        texture,                          // the texture to update
        NULL,                             // a pointer to the topbarangle of pixels to update, or NULL to update the entire texture
        state->decoded_frame->data[0],     // the raw pixel data for the Y plane
        state->decoded_frame->linesize[0], // the number of bytes between rows of pixel data for the Y plane
        state->decoded_frame->data[1],     // the raw pixel data for the U plane
        state->decoded_frame->linesize[1], // the number of bytes between rows of pixel data for the U plane
        state->decoded_frame->data[2],     // the raw pixel data for the V plane
        state->decoded_frame->linesize[2]  // the number of bytes between rows of pixel data for the V plane
    );
}

void Renderer::render_present()
{
    SDL_RenderPresent(m_renderer);
}

void Renderer::destroy_renderer()
{
    SDL_DestroyRenderer(m_renderer);
    std::cout<<"LOG::Destroyed renderer."<<std::endl;
}

SDL_Renderer *Renderer::get_renderer()
{
    return m_renderer;
}