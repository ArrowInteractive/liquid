#define SDL_MAIN_HANDLED

#include <iostream>
#include <SDL2/SDL.h>
#include <filesystem>
#include "Window.hpp"
#include "Renderer.hpp"
#include "Input.hpp"
#include "Texture.hpp"
#include "VideoData.hpp"

using namespace std;
using std::filesystem::exists;
using std::filesystem::is_directory;
using std::filesystem::is_regular_file;