#include <liquid.hpp>

/* Globals */
bool draw_ui = true;

int main(int argc, char** argv)
{
    // Variables
    framedata_struct state;
    SDL_DisplayMode dm;
    SDL_Window* window;
    SDL_Event event;
    SDL_Renderer* renderer;
    SDL_Texture* texture;
    SDL_Rect top_bar;
    SDL_Rect topbar;
    SDL_Rect xbutton;
    SDL_TimerID ui_draw_timer;
    int window_pos_x,window_pos_y;
    bool is_fullscreen = false;
    bool is_file_open = false;

    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER);

    if (SDL_GetDesktopDisplayMode(0, &dm))
    {
        cout<<"Error getting display mode!"<<endl;
        return -1;
    }

    if(argc < 2)
    {
        window = SDL_CreateWindow(
                                    "Liquid Media Player",
                                    SDL_WINDOWPOS_UNDEFINED,
                                    SDL_WINDOWPOS_UNDEFINED,
                                    1280,
                                    720,
                                    SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN
                                );

        if(window == NULL)
        {
            cout<<"Couldn't create window!"<<endl;
            SDL_DestroyWindow(window);
            return -1;
        }

        renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
        if(renderer == NULL)
        {
            cout<<"Couldn't create renderer!"<<endl;
            SDL_DestroyRenderer(renderer);
            return -1;
        }
    }
    else
    {
        if(!exists(argv[1]))
        {
            cout<<"File doesn't exist."<<endl;
            return -1;
        }

        if(!is_regular_file(argv[1]))
        {
            // Write some code to load files from that folder
            // For now just display some error.
            cout<<"Provided input is a folder."<<endl;
            return -1;
        }

        /* Set target width and height */
        state.t_width = dm.w;
        state.t_height = dm.h;
        if(!(load_data(argv[1], &state)))
        {
            return -1;
        }

        if(!(load_frames(&state)))
        {
            return -1;
        }

        is_file_open = true;
        cout<<"Frame width  : "<<state.av_frame->width<<endl;
        cout<<"Frame height : "<<state.av_frame->height<<endl;
        window = SDL_CreateWindow(
                                    argv[1],
                                    SDL_WINDOWPOS_UNDEFINED,
                                    SDL_WINDOWPOS_UNDEFINED,
                                    state.av_frame->width,
                                    state.av_frame->height,
                                    SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN
                                );



        if(window == NULL)
        {
            cout<<"Couldn't create window!"<<endl;
            return -1;
        }

        renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC | SDL_RENDERER_TARGETTEXTURE);
        if(renderer == NULL)
        {
            cout<<"Couldn't create renderer!"<<endl;
            SDL_DestroyRenderer(renderer);
            return -1;
        }

        texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_YV12, SDL_TEXTUREACCESS_STREAMING, state.t_width, state.t_height);
        if(texture == NULL)
        {
            cout<<"Couldn't create texture!"<<endl;
            SDL_DestroyTexture(texture);
            return -1;
        }
    }

    SDL_SetWindowBordered(window, SDL_FALSE);

    while(true)
    {
        SDL_PollEvent(&event);

        /* Draw UI */
        if(!is_fullscreen)
        {
            /*
                Not fullscreen
                Drawing UI
            */
            topbar.x = 0;
            topbar.y = 0;
            topbar.w = state.av_frame->width;
            topbar.h = (int)(state.av_frame->height*5)/100;

            xbutton.x = (int)(state.av_frame->width*97)/100;
            xbutton.y = 0;
            xbutton.w = (int)(state.av_frame->width*6)/100;
            xbutton.h = (int)(state.av_frame->height*5)/100;
        }
        else
        {
            /*
                Is fullscreen
                Drawing UI
            */
            topbar.x = 0;
            topbar.y = 0;
            topbar.w = dm.w;
            topbar.h = (int)(dm.h*5)/100;

            xbutton.x = (int)(dm.w*97)/100;
            xbutton.y = 0;
            xbutton.w = (int)(dm.h*6)/100;
            xbutton.h = (int)(dm.h*5)/100;
        }
        /* Draw the UI based on mouse activity */
        if(event.type == SDL_MOUSEMOTION)
        {
            SDL_RemoveTimer(ui_draw_timer);
            draw_ui = true;
            ui_draw_timer = SDL_AddTimer(1000, hide_ui, (void *)false);
        }
        else
        {
            if(event.type == SDL_KEYDOWN)
            {
                if(event.key.keysym.sym == SDLK_q )
                {
                    break;
                }
                else if(event.key.keysym.sym == SDLK_f)
                {
                    /*
                        Check if fullscreen, if not then make it
                        If it is fullscreen, then make it the initial size
                    */
                    if(!is_fullscreen)
                    {
                        SDL_SetWindowSize(window, dm.w, dm.h + 10);
                        SDL_SetWindowPosition(window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
                    }
                    else
                    {
                        /* Temporary fix */
                        if(is_file_open)
                        {
                            if(state.av_frame->width == 0 && state.av_frame->height == 0)
                            {
                                SDL_SetWindowSize(window, 1280, 720);
                                SDL_SetWindowPosition(window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
                            }
                            else
                            {
                                SDL_SetWindowSize(window, state.av_frame->width, state.av_frame->height);
                                SDL_SetWindowPosition(window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
                            }
                        }
                        else
                        {
                            SDL_SetWindowSize(window, 1280, 720);
                            SDL_SetWindowPosition(window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
                        }
                    }
                    is_fullscreen = !is_fullscreen;
                }
            }
            else if(event.type == SDL_MOUSEBUTTONDOWN)
            {
                if(     event.button.button == SDL_BUTTON_LEFT &&
                        ((event.motion.x > xbutton.x) &&
                        (event.motion.x < xbutton.x + xbutton.w) &&
                        (event.motion.y > xbutton.y) &&
                        (event.motion.y < xbutton.y + xbutton.h)))
                {
                    break;
                }

                if(     event.button.button == SDL_BUTTON_LEFT &&
                        ((event.motion.x > topbar.x) &&
                        (event.motion.x < topbar.x + topbar.w) &&
                        (event.motion.y > topbar.y) &&
                        (event.motion.y < topbar.y + topbar.h)))
                {
                    /*
                        Work in progress
                        Dragging the window
                    */
                    SDL_GetWindowPosition(window, &window_pos_x, &window_pos_y);
                    window_pos_x = event.motion.x + window_pos_x;
                    window_pos_y = event.motion.y + window_pos_y;
                    cout<<"position : "<<window_pos_x<<window_pos_y<<endl;
                    SDL_SetWindowPosition(window,window_pos_x,window_pos_y);
                }

            }
        }

        if(is_file_open)
        {
            /* Load the texture from decoded_frame */
            if(!(load_frames(&state)))
            {
                return -1;
            }
            SDL_UpdateYUVTexture(
                        texture,            // the texture to update
                        NULL,              // a pointer to the topbarangle of pixels to update, or NULL to update the entire texture
                        state.decoded_frame->data[0],      // the raw pixel data for the Y plane
                        state.decoded_frame->linesize[0],  // the number of bytes between rows of pixel data for the Y plane
                        state.decoded_frame->data[1],      // the raw pixel data for the U plane
                        state.decoded_frame->linesize[1],  // the number of bytes between rows of pixel data for the U plane
                        state.decoded_frame->data[2],      // the raw pixel data for the V plane
                        state.decoded_frame->linesize[2]   // the number of bytes between rows of pixel data for the V plane
                    );
        }

        SDL_RenderClear(renderer);
        SDL_RenderCopy(renderer, texture, NULL, NULL);

        /* Draw the ui */
        if(draw_ui)
        {
            SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0); /* Set render color to black */
            SDL_RenderDrawRect(renderer, &topbar);
            SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255); /* Set render color to red */
            SDL_RenderFillRect(renderer, &xbutton);
            SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
        }

        SDL_RenderPresent(renderer);
        SDL_Delay(5);
    }

    cleanup(window, renderer, texture, ui_draw_timer, is_file_open, state);
    return 0;
}

Uint32 hide_ui(Uint32 interval,void* param)
{
    draw_ui = false;
    return 0;
}

void cleanup(SDL_Window* window, SDL_Renderer* renderer, SDL_Texture* texture, SDL_TimerID ui_draw_timer, bool is_file_open, framedata_struct state)
{
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_RemoveTimer(ui_draw_timer);
    SDL_Quit();
    if(is_file_open)
    {
        close_data(&state);
    }
}