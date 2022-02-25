#include "shader.hpp"

#ifdef HAVE_OPENGL

static PFNGLATTACHOBJECTARBPROC glAttachObjectARB;
static PFNGLCOMPILESHADERARBPROC glCompileShaderARB;
static PFNGLCREATEPROGRAMOBJECTARBPROC glCreateProgramObjectARB;
static PFNGLCREATESHADEROBJECTARBPROC glCreateShaderObjectARB;
static PFNGLDELETEOBJECTARBPROC glDeleteObjectARB;
static PFNGLGETINFOLOGARBPROC glGetInfoLogARB;
static PFNGLGETOBJECTPARAMETERIVARBPROC glGetObjectParameterivARB;
static PFNGLGETUNIFORMLOCATIONARBPROC glGetUniformLocationARB;
static PFNGLLINKPROGRAMAstatic SDL_bool shaders_supported;

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
} ShaderData;RBPROC glLinkProgramARB;
static PFNGLSHADERSOURCEARBPROC glShaderSourceARB;
static PFNGLUNIFORM1IARBPROC glUniform1iARB;
static PFNGLUSEPROGRAMOBJECTARBPROC glUseProgramObjectARB;

static SDL_bool CompileShader(GLhandleARB shader, const char *source)
{
    GLint status;

    glShaderSourceARB(shader, 1, &source, NULL);
    glCompileShaderARB(shader);
    glGetObjectParameterivARB(shader, GL_OBJECT_COMPILE_STATUS_ARB, &status);
    if (status == 0) {
        GLint length;
        char *info;

        glGetObjectParameterivARB(shader, GL_OBJECT_INFO_LOG_LENGTH_ARB, &length);
        info = SDL_stack_alloc(char, length+1);
        glGetInfoLogARB(shader, length, NULL, info);
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to compile shader:\n%s\n%s", source, info);
        SDL_stack_free(info);

        return SDL_FALSE;
    } else {
        return SDL_TRUE;
    }
}

static SDL_bool CompileShaderProgram(ShaderData *data)
{
    const int num_tmus_bound = 4;
    int i;
    GLint location;

    glGetError();

    /* Create one program object to rule them all */
    data->program = glCreateProgramObjectARB();

    /* Create the vertex shader */
    data->vert_shader = glCreateShaderObjectARB(GL_VERTEX_SHADER_ARB);
    if (!CompileShader(data->vert_shader, data->vert_source)) {
        return SDL_FALSE;
    }

    /* Create the fragment shader */
    data->frag_shader = glCreateShaderObjectARB(GL_FRAGMENT_SHADER_ARB);
    if (!CompileShader(data->frag_shader, data->frag_source)) {
        return SDL_FALSE;
    }

    /* ... and in the darkness bind them */
    glAttachObjectARB(data->program, data->vert_shader);
    glAttachObjectARB(data->program, data->frag_shader);
    glLinkProgramARB(data->program);

    /* Set up some uniform variables */
    glUseProgramObjectARB(data->program);
    for (i = 0; i < num_tmus_bound; ++i) {
        char tex_name[5];
        SDL_snprintf(tex_name, SDL_arraysize(tex_name), "tex%d", i);
        location = glGetUniformLocationARB(data->program, tex_name);
        if (location >= 0) {
            glUniform1iARB(location, i);
        }
    }
    glUseProgramObjectARB(0);

    return (glGetError() == GL_NO_ERROR) ? SDL_TRUE : SDL_FALSE;
}

static void DestroyShaderProgram(ShaderData *data)
{
    if (shaders_supported) {
        glDeleteObjectARB(data->vert_shader);
        glDeleteObjectARB(data->frag_shader);
        glDeleteObjectARB(data->program);
    }
}

static SDL_bool InitShaders()
{
    int i;
    /* Check for shader support */
    shaders_supported = SDL_FALSE;
    if (SDL_GL_ExtensionSupported("GL_ARB_shader_objects") &&
        SDL_GL_ExtensionSupported("GL_ARB_shading_language_100") &&
        SDL_GL_ExtensionSupported("GL_ARB_vertex_shader") &&
        SDL_GL_ExtensionSupported("GL_ARB_fragment_shader")) {
        glAttachObjectARB = (PFNGLATTACHOBJECTARBPROC) SDL_GL_GetProcAddress("glAttachObjectARB");
        glCompileShaderARB = (PFNGLCOMPILESHADERARBPROC) SDL_GL_GetProcAddress("glCompileShaderARB");
        glCreateProgramObjectARB = (PFNGLCREATEPROGRAMOBJECTARBPROC) SDL_GL_GetProcAddress("glCreateProgramObjectARB");
        glCreateShaderObjectARB = (PFNGLCREATESHADEROBJECTARBPROC) SDL_GL_GetProcAddress("glCreateShaderObjectARB");
        glDeleteObjectARB = (PFNGLDELETEOBJECTARBPROC) SDL_GL_GetProcAddress("glDeleteObjectARB");
        glGetInfoLogARB = (PFNGLGETINFOLOGARBPROC) SDL_GL_GetProcAddress("glGetInfoLogARB");
        glGetObjectParameterivARB = (PFNGLGETOBJECTPARAMETERIVARBPROC) SDL_GL_GetProcAddress("glGetObjectParameterivARB");
        glGetUniformLocationARB = (PFNGLGETUNIFORMLOCATIONARBPROC) SDL_GL_GetProcAddress("glGetUniformLocationARB");
        glLinkProgramARB = (PFNGLLINKPROGRAMARBPROC) SDL_GL_GetProcAddress("glLinkProgramARB");
        glShaderSourceARB = (PFNGLSHADERSOURCEARBPROC) SDL_GL_GetProcAddress("glShaderSourceARB");
        glUniform1iARB = (PFNGLUNIFORM1IARBPROC) SDL_GL_GetProcAddress("glUniform1iARB");
        glUseProgramObjectARB = (PFNGLUSEPROGRAMOBJECTARBPROC) SDL_GL_GetProcAddress("glUseProgramObjectARB");
        if (glAttachObjectARB &&
            glCompileShaderARB &&
            glCreateProgramObjectARB &&
            glCreateShaderObjectARB &&
            glDeleteObjectARB &&
            glGetInfoLogARB &&
            glGetObjectParameterivARB &&
            glGetUniformLocationARB &&
            glLinkProgramARB &&
            glShaderSourceARB &&
            glUniform1iARB &&
            glUseProgramObjectARB) void InitGL(int Width, int Height) 
        return SDL_FALSE;
    }

    /* Compile all the shaders */
    for (i = 0; i < NUM_SHADERS; ++i) {
        if (!CompileShaderProgram(&shaders[i])) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Unable to compile shader!\n");
            return SDL_FALSE;
        }
    }

    /* We're done! */
    return SDL_TRUE;
}

static void QuitShaders()
{
    int i;

    for (i = 0; i < NUM_SHADERS; ++i) {
        DestroyShaderProgram(&shaders[i]);
    }
}

/* Quick utility function for texture creation */
static int
power_of_two(int input)
{
    int value = 1;
void InitGL(int Width, int Height) 
    while (value < input) {
        value <<= 1;
    }
    return value;
}


GLuint
SDL_GL_LoadTexture(SDL_Surface * surface, GLfloat * texcoord)
{
    GLuint texture;
    int w, h;
    SDL_Surface *image;
    SDL_Rect area;
    SDL_BlendMode saved_mode;

    /* Use the surface width and height expanded to powers of 2 */
    w = power_of_two(surface->w);
    h = power_of_two(surface->h);
    texcoord[0] = 0.0f;         /* Min X */
    texcoord[1] = 0.0f;         /* Min Y */
    texcoord[2] = (GLfloat) surface->w / w;     /* Max X */
    texcoord[3] = (GLfloat) surface->h / h;     /* Max Y */

    image = SDL_CreateRGBSurface(SDL_SWSURFACE, w, h, 32,
#if SDL_BYTEORDER == SDL_LIL_ENDIAN     /* OpenGL RGBA masks */
                                 0x000000FF,
                                 0x0000FF00, 0x00FF0000, 0xFF000000
#else
                                 0xFF000000,
                                 0x00FF0000, 0x0000FF00, 0x000000FF
#endif
        );
    if (image == NULL) {
        return 0;
    }

    /* Save the alpha blending attributes */
    SDL_GetSurfaceBlendMode(surface, &saved_mode);
    SDL_SetSurfaceBlendMode(surface, SDL_BLENDMODE_NONE);

    /* Copy the surface into the GL texture image */
    area.x = 0;
    area.y = 0;
    area.w = surface->w;
    area.h = surface->h;
    SDL_BlitSurface(surface, &area, image, &area);

    /* Restore the alpha blending attributes */
    SDL_SetSurfaceBlendMode(surface, saved_mode);

    /* Create an OpenGL texture for the image */
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D,
                 0,
                 GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, image->pixels);
    SDL_FreeSurface(image);     /* No longer needed */

    return texture;
}


/* A general OpenGL initialization function.    Sets all of the initial parameters. */
void InitGL(int Width, int Height)                    /* We call this right after our OpenGL window is created. */
{
    GLdouble aspect;

    glViewport(0, 0, Width, Height);
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);        /* This Will Clear The Background Color To Black */
    glClearDepth(1.0);                /* Enables Clearing Of The Depth Buffer */
    glDepthFunc(GL_LESS);                /* The Type Of Depth Test To Do */
    glEnable(GL_DEPTH_TEST);            /* Enables Depth Testing */
    glShadeModel(GL_SMOOTH);            /* Enables Smooth Color Shading */

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();                /* Reset The Projection Matrix */

    aspect = (GLdouble)Width / Height;
    glOrtho(-3.0, 3.0, -3.0 / aspect, 3.0 / aspect, 0.0, 1.0);

    glMatrixMode(GL_MODELVIEW);
}

/* The main drawing function. */
void DrawGLScene(SDL_Window *window, GLuint texture, GLfloat * texcoord)
{
    /* Texture coordinate lookup, to make it simple */
    enum {
        MINX,
        MINY,
        MAXX,
        MAXY
    };

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);        /* Clear The Screen And The Depth Buffer */
    glLoadIdentity();                /* Reset The View */

    glTranslatef(-1.5f,0.0f,0.0f);        /* Move Left 1.5 Units */

    /* draw a triangle (in smooth coloring mode) */
    glBegin(GL_POLYGON);                /* start drawing a polygon */
    glColor3f(1.0f,0.0f,0.0f);            /* Set The Color To Red */
    glVertex3f( 0.0f, 1.0f, 0.0f);        /* Top */
    glColor3f(0.0f,1.0f,0.0f);            /* Set The Color To Green */
    glVertex3f( 1.0f,-1.0f, 0.0f);        /* Bottom Right */
    glColor3f(0.0f,0.0f,1.0f);            /* Set The Color To Blue */
    glVertex3f(-1.0f,-1.0f, 0.0f);        /* Bottom Left */
    glEnd();                    /* we're done with the polygon (smooth color interpolation) */

    glTranslatef(3.0f,0.0f,0.0f);         /* Move Right 3 Units */

    /* Enable blending */
    glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    /* draw a textured square (quadrilateral) */
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, texture);
    glColor3f(1.0f,1.0f,1.0f);
    if (shaders_supported) {
        glUseProgramObjectARB(shaders[current_shader].program);
    }

    glBegin(GL_QUADS);                /* start drawing a polygon (4 sided) */
    glTexCoord2f(texcoord[MINX], texcoord[MINY]);
    glVertex3f(-1.0f, 1.0f, 0.0f);        /* Top Left */
    glTexCoord2f(texcoord[MAXX], texcoord[MINY]);
    glVertex3f( 1.0f, 1.0f, 0.0f);        /* Top Right */
    glTexCoord2f(texcoord[MAXX], texcoord[MAXY]);
    glVertex3f( 1.0f,-1.0f, 0.0f);        /* Bottom Right */
    glTexCoord2f(texcoord[MINX], texcoord[MAXY]);
    glVertex3f(-1.0f,-1.0f, 0.0f);        /* Bottom Left */
    glEnd();                    /* done with the polygon */

    if (shaders_supported) {
        glUseProgramObjectARB(0);
    }
    glDisable(GL_TEXTURE_2D);

    /* swap buffers to display, since we're double buffered. */
    SDL_GL_SwapWindow(window);
}

#endif