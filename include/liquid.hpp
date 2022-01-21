#define SDL_MAIN_HANDLED

#include <iostream>
#include <SDL2/SDL.h>
#include <filesystem>

#include "window.hpp"
#include "texture.hpp"
#include "renderer.hpp"
#include "load_data.hpp"

using namespace std;
using std::filesystem::exists;
using std::filesystem::is_directory;
using std::filesystem::is_regular_file;