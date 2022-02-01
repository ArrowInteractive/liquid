#include <glad/glad.h>
#include <SDL2/SDL.h>
#include <iostream>
#include <fstream>

void init_glad();

void init_shader(std::string vertexpath,std::string fragmentpath);

void use_shader();