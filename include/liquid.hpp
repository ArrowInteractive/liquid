#define SDL_MAIN_HANDLED

#include <iostream>
#include <SDL2/SDL.h>
#include <filesystem>
#include <loaddata.hpp>
using namespace std;
using std::filesystem::exists;
using std::filesystem::is_directory;
using std::filesystem::is_regular_file;

/* Functions */
Uint32 hide_ui(Uint32 interval,void* param);
void cleanup(SDL_Window* window, SDL_Renderer* renderer, SDL_Texture* texture, SDL_TimerID ui_draw_timer, bool is_file_open, framedata_struct state);