#pragma once
#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>

static SDL_bool shaders_supported;

enum {
    SHADER_COLOR,
    SHADER_TEXTURE,
    SHADER_TEXCOORDS,
    NUM_SHADERS
};

typedef struct {
    GLhandleARB program;
    GLhandleARB vert_shader;
    GLhandleARB frag_shader;
    const char *vert_source;
    const char *frag_source;
} ShaderData;

static SDL_bool CompileShader(GLhandleARB shader, const char *source);

static SDL_bool CompileShaderProgram(ShaderData *data);

static void DestroyShaderProgram(ShaderData *data);

static SDL_bool InitShaders();

static void QuitShaders();

GLuint SDL_GL_LoadTexture(SDL_Surface * surface, GLfloat * texcoord);

void InitGL(int Width, int Height);