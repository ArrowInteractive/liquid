#include "liquid.hpp"

int main(int argc, char **argv)
{   
    SDL_DisplayMode displaymode;
    SDL_Window* window;
    SDL_Renderer* renderer;
    SDL_Texture* texture;
    SDL_Event event;
    datastruct state;
    eventstruct eventstate;
    bool is_file_open = false;
    
    if(!(get_display_mode(&displaymode))){
        cout<<"ERROR: Could not get display mode!"<<endl;
        return -1;
    }

    // Check arguments
    if(argc < 2){
       if((window = window_create(window, "Liquid Media Player", 600, 400)) == NULL)
       {
           cout<<"ERROR: Could not create window!"<<endl;
       }
    }
    else{

        // Check file properties
        if(!exists(argv[1])){
            cout<<"The file doesn't exist!"<<endl;
            return -1;
        }

        if(!is_regular_file(argv[1])){
            /*
                Setup liquid to load media files from the folder and play
                them sequentially.

                For now, display some error.
            */
            cout<<"Provided input is a folder!"<<endl;
            return -1;
        }

        // Set target width and height
        state.t_width = displaymode.w;
        state.t_height = displaymode.h;

        /*
            Use FFmpeg to decode basic info about the input
            Need to have separate functions for file and folder
        */

        if(!(load_data(argv[1], &state))){
            cout<<"ERROR: Failed to load data!"<<endl;
            return -1;
        }
        is_file_open = true;

        if((window = window_create(window, argv[1], state.av_codec_ctx->width, state.av_codec_ctx->height)) == NULL)
        {
            cout<<"ERROR: Could not create window!"<<endl;
            return -1;
        }
    }
    /*
        Setup renderer and texture
    */
    if((renderer = renderer_create(renderer, window)) == NULL)
    {
        cout<<"ERROR: Could not create renderer!"<<endl;
        return -1;
    }
    
    if(is_file_open){
        if((texture = texture_create(renderer, texture, state.t_width, state.t_height)) == NULL)
        {
            cout<<"ERROR: Could not create texture!"<<endl;
        }
    }


    // Main loop
    while(eventstate.is_running)
    {
        // Process inputs
        handle_inputs(&event, &eventstate);
        
        if(is_file_open){
            load_frame(&state);
            texture_update(texture, &state);
        }
        renderer_clear(renderer);

        if(is_file_open){
            renderer_copy(renderer, texture);
        }
        render_present(renderer);

        SDL_Delay(5);
    }


    // Cleanup
    window_destroy(window);
    renderer_destroy(renderer);
    if(is_file_open){
        texture_destroy(texture);
        close_data(&state);
    }
    SDL_Quit();
    
    return 0;
}