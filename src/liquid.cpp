#include "liquid.hpp"

int main(int argc, char **argv)
{
    Window *window;
    Renderer *renderer;
    Texture *texture;
    VideoData *videodata;
    Input input;
    bool is_file_open = false;
    framedata_struct state;

    if (argc < 2)
    {
        window = new Window("Liquid Video Player", 960, 480);
        renderer = new Renderer(window);
        texture = new Texture(renderer, 960, 480);
    }
    else
    {
        if (!exists(argv[1]))
        {
            cout << "File doesn't exist." << endl;
            return -1;
        }

        if (!is_regular_file(argv[1]))
        {
            /*
                Setup liquid to load media files from the folder and play
                them sequentially.

                For now, display some error.
            */
            cout << "Provided input is a folder." << endl;
            return -1;
        }

        /* Using previosly written functions */
        if (!(videodata->load_data(argv[1], &state)))
        {
            return -1;
        }
        is_file_open = true;

        window = new Window(argv[1],
                            state.av_codec_ctx->width,
                            state.av_codec_ctx->height);

        /* Set target width and height */
        state.t_width = window->m_displaymode.w;
        state.t_height = window->m_displaymode.h;

        renderer = new Renderer(window);

        texture = new Texture(renderer,
                              state.av_codec_ctx->width,
                              state.av_codec_ctx->height);
    }

    while (input.is_window_running())
    {
        input.update_events();
        if (is_file_open)
        {
            if (!(videodata->load_frames(&state)))
            {
                return -1;
            }
            renderer->update_texture(texture->get_texture(),&state);
        }

        renderer->clear_renderer();
        renderer->render_texture(texture->get_texture());
        renderer->render_present();
        SDL_Delay(5);
    }
    window->close_window();
    renderer->destroy_renderer();

    return 0;
}