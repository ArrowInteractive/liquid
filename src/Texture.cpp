#include "Texture.hpp"

Texture::Texture(Renderer *renderer)
{
    if (m_texture ==NULL)
        delete_texture();
    m_renderer = renderer;
    m_texture = SDL_CreateTexture(
        m_renderer->get_renderer(),
        SDL_PIXELFORMAT_YV12,
        SDL_TEXTUREACCESS_STREAMING,
        1280,
        720);
}

Texture::Texture(Renderer *renderer, int width, int height)
{
    if (m_texture ==NULL)
        delete_texture();
    m_renderer = renderer;
    m_texture = SDL_CreateTexture(
        m_renderer->get_renderer(),
        SDL_PIXELFORMAT_YV12,
        SDL_TEXTUREACCESS_STREAMING,
        m_width,
        m_height);
}


void Texture::set_texture_data()
{
}

void Texture::delete_texture()
{
    SDL_DestroyTexture(m_texture);
}