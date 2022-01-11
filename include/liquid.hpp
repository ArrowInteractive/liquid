#define SDL_MAIN_HANDLED

#include <iostream>
#include <SDL2/SDL.h>
#include <filesystem>
#include <loaddata.hpp>
using namespace std;
using std::filesystem::exists;
using std::filesystem::is_directory;
using std::filesystem::is_regular_file;

void cleanup(SDL_Window* window, SDL_Renderer* renderer, SDL_Texture* texture, bool is_file_open, framedata_struct state);