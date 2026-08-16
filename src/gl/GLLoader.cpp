#include "gl/GLLoader.hpp"
#include "SDL.h"
#include "utils/Log.hpp"

namespace gl {

PFNGLCREATESHADERPROC CreateShader = nullptr;
PFNGLSHADERSOURCEPROC ShaderSource = nullptr;
PFNGLCOMPILESHADERPROC CompileShader = nullptr;
PFNGLGETSHADERIVPROC GetShaderiv = nullptr;
PFNGLGETSHADERINFOLOGPROC GetShaderInfoLog = nullptr;
PFNGLDELETESHADERPROC DeleteShader = nullptr;
PFNGLCREATEPROGRAMPROC CreateProgram = nullptr;
PFNGLATTACHSHADERPROC AttachShader = nullptr;
PFNGLLINKPROGRAMPROC LinkProgram = nullptr;
PFNGLGETPROGRAMIVPROC GetProgramiv = nullptr;
PFNGLGETPROGRAMINFOLOGPROC GetProgramInfoLog = nullptr;
PFNGLDELETEPROGRAMPROC DeleteProgram = nullptr;
PFNGLUSEPROGRAMPROC UseProgram = nullptr;
PFNGLGETUNIFORMLOCATIONPROC GetUniformLocation = nullptr;
PFNGLUNIFORM1IPROC Uniform1i = nullptr;
PFNGLUNIFORM2IPROC Uniform2i = nullptr;
PFNGLUNIFORM1FPROC Uniform1f = nullptr;
PFNGLUNIFORM2FPROC Uniform2f = nullptr;
PFNGLUNIFORM3FPROC Uniform3f = nullptr;
PFNGLUNIFORM4FPROC Uniform4f = nullptr;

PFNGLGENVERTEXARRAYSPROC GenVertexArrays = nullptr;
PFNGLBINDVERTEXARRAYPROC BindVertexArray = nullptr;
PFNGLDELETEVERTEXARRAYSPROC DeleteVertexArrays = nullptr;
PFNGLGENBUFFERSPROC GenBuffers = nullptr;
PFNGLBINDBUFFERPROC BindBuffer = nullptr;
PFNGLBUFFERDATAPROC BufferData = nullptr;
PFNGLBUFFERSUBDATAPROC BufferSubData = nullptr;
PFNGLDELETEBUFFERSPROC DeleteBuffers = nullptr;
PFNGLVERTEXATTRIBPOINTERPROC VertexAttribPointer = nullptr;
PFNGLENABLEVERTEXATTRIBARRAYPROC EnableVertexAttribArray = nullptr;

PFNGLACTIVETEXTUREPROC ActiveTexture = nullptr;

PFNGLGENFRAMEBUFFERSPROC GenFramebuffers = nullptr;
PFNGLBINDFRAMEBUFFERPROC BindFramebuffer = nullptr;
PFNGLFRAMEBUFFERTEXTURE2DPROC FramebufferTexture2D = nullptr;
PFNGLCHECKFRAMEBUFFERSTATUSPROC CheckFramebufferStatus = nullptr;
PFNGLDELETEFRAMEBUFFERSPROC DeleteFramebuffers = nullptr;

namespace {

template <typename T>
bool load_proc(T &fn, const char *name)
{
    fn = reinterpret_cast<T>(SDL_GL_GetProcAddress(name));
    if (!fn)
        Log::error() << "OpenGL driver is missing required entry point: " << name;
    return fn != nullptr;
}

} // namespace

bool load()
{
    bool ok = true;
    ok &= load_proc(CreateShader, "glCreateShader");
    ok &= load_proc(ShaderSource, "glShaderSource");
    ok &= load_proc(CompileShader, "glCompileShader");
    ok &= load_proc(GetShaderiv, "glGetShaderiv");
    ok &= load_proc(GetShaderInfoLog, "glGetShaderInfoLog");
    ok &= load_proc(DeleteShader, "glDeleteShader");
    ok &= load_proc(CreateProgram, "glCreateProgram");
    ok &= load_proc(AttachShader, "glAttachShader");
    ok &= load_proc(LinkProgram, "glLinkProgram");
    ok &= load_proc(GetProgramiv, "glGetProgramiv");
    ok &= load_proc(GetProgramInfoLog, "glGetProgramInfoLog");
    ok &= load_proc(DeleteProgram, "glDeleteProgram");
    ok &= load_proc(UseProgram, "glUseProgram");
    ok &= load_proc(GetUniformLocation, "glGetUniformLocation");
    ok &= load_proc(Uniform1i, "glUniform1i");
    ok &= load_proc(Uniform2i, "glUniform2i");
    ok &= load_proc(Uniform1f, "glUniform1f");
    ok &= load_proc(Uniform2f, "glUniform2f");
    ok &= load_proc(Uniform3f, "glUniform3f");
    ok &= load_proc(Uniform4f, "glUniform4f");

    ok &= load_proc(GenVertexArrays, "glGenVertexArrays");
    ok &= load_proc(BindVertexArray, "glBindVertexArray");
    ok &= load_proc(DeleteVertexArrays, "glDeleteVertexArrays");
    ok &= load_proc(GenBuffers, "glGenBuffers");
    ok &= load_proc(BindBuffer, "glBindBuffer");
    ok &= load_proc(BufferData, "glBufferData");
    ok &= load_proc(BufferSubData, "glBufferSubData");
    ok &= load_proc(DeleteBuffers, "glDeleteBuffers");
    ok &= load_proc(VertexAttribPointer, "glVertexAttribPointer");
    ok &= load_proc(EnableVertexAttribArray, "glEnableVertexAttribArray");

    ok &= load_proc(ActiveTexture, "glActiveTexture");

    ok &= load_proc(GenFramebuffers, "glGenFramebuffers");
    ok &= load_proc(BindFramebuffer, "glBindFramebuffer");
    ok &= load_proc(FramebufferTexture2D, "glFramebufferTexture2D");
    ok &= load_proc(CheckFramebufferStatus, "glCheckFramebufferStatus");
    ok &= load_proc(DeleteFramebuffers, "glDeleteFramebuffers");

    return ok;
}

} // namespace gl
