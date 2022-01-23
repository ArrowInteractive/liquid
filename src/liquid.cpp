#include "liquid.hpp"

int main(int argc, char **argv)
{   
    SDL_DisplayMode displaymode;
    SDL_Window* window;
    SDL_Renderer* renderer;
    SDL_Texture* texture;
    SDL_Event event;
    datastruct datastate;
    eventstruct eventstate;
    bool is_file_open = false;
    
    if(!(get_display_mode(&displaymode))){
        cout<<"ERROR: Could not get display mode!"<<endl;
        return -1;
    }
    datastate.d_width = displaymode.w;
    datastate.d_height = displaymode.h;
    datastate.t_width = displaymode.w;
    datastate.t_height = displaymode.h;

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

        /*
            Use FFmpeg to decode basic info about the input
            Need to have separate functions for file and folder
        */
        if(!(load_data(argv[1], &datastate))){
            cout<<"ERROR: Failed to load data!"<<endl;
            return -1;
        }
        is_file_open = true;

        if((window = window_create(window, argv[1], datastate.video_codec_ctx->width, datastate.video_codec_ctx->height)) == NULL)
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
        if((texture = texture_create(renderer, texture, datastate.t_width, datastate.t_height)) == NULL)
        {
            cout<<"ERROR: Could not create texture!"<<endl;
        }
    }


    // Main loop
    while(eventstate.is_running)
    {
        // Process inputs
        handle_inputs(&event, &eventstate, window, &datastate);
        
        if(is_file_open){
            if(eventstate.change_scaling){
                // Need to change the scaling
                if(eventstate.is_fullscreen){
                    /* Free previously used items */
                    SDL_DestroyTexture(texture);
                    sws_freeContext(datastate.sws_ctx);
                    texture = SDL_CreateTexture(    
                        renderer, SDL_PIXELFORMAT_YV12,
                        SDL_TEXTUREACCESS_STREAMING,
                        datastate.video_codec_ctx->width,
                        datastate.video_codec_ctx->height
                    );

                    if(texture == NULL){
                        cout<<"Couldn't create texture!"<<endl;
                        SDL_DestroyTexture(texture);
                        return -1;
                    }

                    datastate.num_bytes = av_image_get_buffer_size(     
                        AV_PIX_FMT_YUV420P,
                        datastate.video_codec_ctx->width,
                        datastate.video_codec_ctx->height,
                        32
                    );

                    datastate.video_buffer = (uint8_t *)av_malloc(datastate.num_bytes * sizeof(uint8_t));
                    datastate.sws_ctx = sws_getContext(     
                        datastate.video_codec_ctx->width, datastate.video_codec_ctx->height, datastate.video_codec_ctx->pix_fmt,
                        datastate.video_codec_ctx->width, datastate.video_codec_ctx->height, AV_PIX_FMT_YUV420P,
                        SWS_BILINEAR, NULL, NULL, NULL
                    );
                }
                else{
                    /* Free previously used items */
                    SDL_DestroyTexture(texture);
                    sws_freeContext(datastate.sws_ctx);
                    texture = SDL_CreateTexture(    
                        renderer, SDL_PIXELFORMAT_YV12,
                        SDL_TEXTUREACCESS_STREAMING,
                        datastate.d_width,
                        datastate.d_height
                    );

                    if(texture == NULL){
                        cout<<"Couldn't create texture!"<<endl;
                        SDL_DestroyTexture(texture);
                        return -1;
                    }

                    datastate.num_bytes = av_image_get_buffer_size(     
                        AV_PIX_FMT_YUV420P,
                        datastate.d_width,
                        datastate.d_height,
                        32
                    );

                    datastate.video_buffer = (uint8_t *)av_malloc(datastate.num_bytes * sizeof(uint8_t));

                    datastate.sws_ctx = sws_getContext(     
                        datastate.video_codec_ctx->width, datastate.video_codec_ctx->height, datastate.video_codec_ctx->pix_fmt,
                        datastate.d_width, datastate.d_height, AV_PIX_FMT_YUV420P,
                        SWS_BILINEAR, NULL, NULL, NULL
                    );
                }
                eventstate.change_scaling = false;
                eventstate.is_fullscreen = !eventstate.is_fullscreen;
            }
            load_video_frame(&datastate);
            texture_update(texture, &datastate);
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
        close_data(&datastate);
    }
    SDL_Quit();
    
    return 0;
}