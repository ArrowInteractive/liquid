#pragma once

/*
**  Includes
*/

#include "SDL.h"
#include "SDL_opengl.h"

/*
**  Enums
*/

enum{
    SHADER_COLOR,
    SHADER_TEXTURE,
    SHADER_TEXCOORDS,
    NUM_SHADERS
};

/*
**  Structs
*/

struct ShaderData{
    GLhandleARB program;
    GLhandleARB vert_shader;
    GLhandleARB frag_shader;
    const char *vert_source;
    const char *frag_source;
};

/*
**  Gloabls
*/

extern PFNGLATTACHOBJECTARBPROC glAttachObjectARB;
extern PFNGLCOMPILESHADERARBPROC glCompileShaderARB;
extern PFNGLCREATEPROGRAMOBJECTARBPROC glCreateProgramObjectARB;
extern PFNGLCREATESHADEROBJECTARBPROC glCreateShaderObjectARB;
extern PFNGLDELETEOBJECTARBPROC glDeleteObjectARB;
extern PFNGLGETINFOLOGARBPROC glGetInfoLogARB;
extern PFNGLGETOBJECTPARAMETERIVARBPROC glGetObjectParameterivARB;
extern PFNGLGETUNIFORMLOCATIONARBPROC glGetUniformLocationARB;
extern PFNGLLINKPROGRAMARBPROC glLinkProgramARB;
extern PFNGLSHADERSOURCEARBPROC glShaderSourceARB;
extern PFNGLUNIFORM1IARBPROC glUniform1iARB;
extern PFNGLUSEPROGRAMOBJECTARBPROC glUseProgramObjectARB;
extern ShaderData shaders[NUM_SHADERS];

/*
**  Functions
*/

bool compile_shader(GLhandleARB shader, const char *source);
bool compile_shader_program(ShaderData *data);
void destroy_shader_program(ShaderData *data);
bool init_shaders();
int power_of_two(int input);
void quit_shaders();
GLuint SDL_GL_LoadTexture(SDL_Surface *surface, GLfloat *texcoord);
void init_gl(int width, int height);
void draw_gl_scene(SDL_Window *window, GLuint texture, GLfloat * texcoord);



