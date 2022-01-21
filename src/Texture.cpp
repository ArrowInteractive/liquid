#include "texture.hpp"

SDL_Texture* texture_create(SDL_Renderer* renderer, SDL_Texture* texture, int width, int height)
{
    texture = SDL_CreateTexture(
        renderer,
        SDL_PIXELFORMAT_YV12,
        SDL_TEXTUREACCESS_STREAMING,
        width,
        height
    );
    return texture;
}

void texture_update(SDL_Texture* texture, datastruct* state)
{
    SDL_UpdateYUVTexture(
        texture,                            // the texture to update
        NULL,                               // a pointer to the topbarangle of pixels to update, or NULL to update the entire texture
        state->decoded_frame->data[0],      // the raw pixel data for the Y plane
        state->decoded_frame->linesize[0],  // the number of bytes between rows of pixel data for the Y plane
        state->decoded_frame->data[1],      // the raw pixel data for the U plane
        state->decoded_frame->linesize[1],  // the number of bytes between rows of pixel data for the U plane
        state->decoded_frame->data[2],      // the raw pixel data for the V plane
        state->decoded_frame->linesize[2]   // the number of bytes between rows of pixel data for the V plane
    );
}

void texture_destroy(SDL_Texture* texture)
{
    SDL_DestroyTexture(texture);
    cout<<"Destoyed texture."<<endl;
}