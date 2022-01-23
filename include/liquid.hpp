#ifdef __MINGW32__
#define SDL_MAIN_HANDLED
#endif

#include <iostream>
#include <SDL2/SDL.h>
#include <filesystem>

#include "window.hpp"
#include "texture.hpp"
#include "renderer.hpp"
#include "load_data.hpp"
#include "video.hpp"
#include "inputs.hpp"

using namespace std;
using std::filesystem::exists;
using std::filesystem::is_directory;
using std::filesystem::is_regular_file;