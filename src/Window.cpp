#include "Window.hpp"
#include "gl/GLLoader.hpp"
#include "gl/VideoRenderer.hpp"
#include "utils/Log.hpp"

int Window::create_window(){
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 0);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 0);

    window = SDL_CreateWindow(
        "Liquid Media Player",
        SDL_WINDOWPOS_UNDEFINED,
        SDL_WINDOWPOS_UNDEFINED,
        default_width, default_height,
         SDL_WINDOW_OPENGL |
         SDL_WINDOW_ALLOW_HIGHDPI |
         SDL_WINDOW_RESIZABLE
    );
    if (!window)
        return -1;

    context = SDL_GL_CreateContext(window);
    if (!context){
        Log::error() << "Could not create an OpenGL 3.3 core context: " << SDL_GetError();
        return -1;
    }

    if (!gl::load())
        return -1;

    SDL_GL_SetSwapInterval(1);

    if (!VideoRenderer::init())
        return -1;

    return 0;
}

void Window::set_default_window_size(int width, int height, AVRational sar)
{
    SDL_Rect rect;
    int max_width  = screen_width  ? screen_width  : INT_MAX;
    int max_height = screen_height ? screen_height : INT_MAX;
    if (max_width == INT_MAX && max_height == INT_MAX)
        max_height = height;
    calculate_display_rect(&rect, 0, 0, max_width, max_height, width, height, sar);
    default_width  = rect.w;
    default_height = rect.h;
}

void Window::calculate_display_rect(
    SDL_Rect *rect,
    int scr_xleft, int scr_ytop, int scr_width, int scr_height,
    int pic_width, int pic_height, AVRational pic_sar
)
{
    AVRational aspect_ratio = pic_sar;
    int64_t width, height, x, y;

    if (av_cmp_q(aspect_ratio, av_make_q(0, 1)) <= 0)
        aspect_ratio = av_make_q(1, 1);

    aspect_ratio = av_mul_q(aspect_ratio, av_make_q(pic_width, pic_height));

    /* XXX: we suppose the screen has a 1.0 pixel ratio */
    height = scr_height;
    width = av_rescale(height, aspect_ratio.num, aspect_ratio.den) & ~1;
    if (width > scr_width) {
        width = scr_width;
        height = av_rescale(width, aspect_ratio.den, aspect_ratio.num) & ~1;
    }
    x = (scr_width - width) / 2;
    y = (scr_height - height) / 2;
    rect->x = scr_xleft + x;
    rect->y = scr_ytop  + y;
    rect->w = FFMAX((int)width,  1);
    rect->h = FFMAX((int)height, 1);
}

void Window::update_sample_display(VideoState *videostate, short *samples, int samples_size)
{
    int size, len;

    size = samples_size / sizeof(short);
    while (size > 0) {
        len = SAMPLE_ARRAY_SIZE - videostate->sample_array_index;
        if (len > size)
            len = size;
        memcpy(videostate->sample_array + videostate->sample_array_index, samples, len * sizeof(short));
        samples += len;
        videostate->sample_array_index += len;
        if (videostate->sample_array_index >= SAMPLE_ARRAY_SIZE)
            videostate->sample_array_index = 0;
        size -= len;
    }
}