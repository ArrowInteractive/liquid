#include "liquid.hpp"

int main(int argc, char **argv)
{
    Window *window;
    Renderer *renderer;
    Texture* texture;
    VideoData* videodata;
    Input input;
    framedata_struct state;

    if(argc < 2)
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
        
        if (!(videodata->load_data(argv[1], &state)))
        {
            return -1;
        }
        
        window = new Window(    argv[1], 
                                state.av_codec_ctx->width, 
                                state.av_codec_ctx->height
                            );
                            
        /* Set target width and height */
        state.t_width = window->m_displaymode.w;
        state.t_height = window->m_displaymode.h;

        renderer = new Renderer(window);
        
        texture = new Texture(  renderer, 
                                state.av_codec_ctx->width, 
                                state.av_codec_ctx->height
                            );
    }

    while (input.is_window_running())
    {
        input.update_events();
        renderer->clear_renderer();
        renderer->render();
        SDL_Delay(5);
    }
    window->close_window();
    renderer->close_renderer();
    
    return 0;
}