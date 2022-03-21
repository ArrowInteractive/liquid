#pragma once

/*
**  Includes
*/

#include "imgui.h"
#include "imgui_impl_sdl.h"
#include "imgui_impl_sdlrenderer.h"
#include <iostream>

/*
**  Globals
*/

extern bool req_pause;
extern bool req_seek;
extern bool req_mute;
extern bool req_seek_progress;
extern bool req_audio_track_change;
extern bool req_sub_track_change;
extern bool draw_ui;
extern double ui_incr;
extern int sound_var;
extern bool vol_change;
extern double cur_tim;
extern int cur_sec;
extern int cur_min;
extern int cur_hur;
extern float progress_var;
extern std::string max_video_duration;
extern std::string current_time;

/*
**  Functions
*/

void init_imgui(SDL_Window* window, SDL_Renderer* renderer);
void update_imgui(SDL_Renderer* renderer, int width, int height);
void destroy_imgui_data();
void imgui_event_handler(SDL_Event& event);
bool want_capture_mouse();
bool want_capture_keyboard();
void change_imgui_win_size();
void end_win_size_change();