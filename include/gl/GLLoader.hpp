#pragma once

#include "SDL_opengl.h"

/*
** Modern (post-1.1) GL entry points are not statically exported by every
** platform's GL library (notably opengl32.dll on Windows), so they must be
** resolved at runtime via SDL_GL_GetProcAddress(). Everything below is kept
** behind the gl:: namespace instead of the real glFoo names so this loader
** never collides with a platform GL header that happens to already declare
** the real symbol (macOS does, for example).
*/
namespace gl {

extern PFNGLCREATESHADERPROC CreateShader;
extern PFNGLSHADERSOURCEPROC ShaderSource;
extern PFNGLCOMPILESHADERPROC CompileShader;
extern PFNGLGETSHADERIVPROC GetShaderiv;
extern PFNGLGETSHADERINFOLOGPROC GetShaderInfoLog;
extern PFNGLDELETESHADERPROC DeleteShader;
extern PFNGLCREATEPROGRAMPROC CreateProgram;
extern PFNGLATTACHSHADERPROC AttachShader;
extern PFNGLLINKPROGRAMPROC LinkProgram;
extern PFNGLGETPROGRAMIVPROC GetProgramiv;
extern PFNGLGETPROGRAMINFOLOGPROC GetProgramInfoLog;
extern PFNGLDELETEPROGRAMPROC DeleteProgram;
extern PFNGLUSEPROGRAMPROC UseProgram;
extern PFNGLGETUNIFORMLOCATIONPROC GetUniformLocation;
extern PFNGLUNIFORM1IPROC Uniform1i;
extern PFNGLUNIFORM2IPROC Uniform2i;
extern PFNGLUNIFORM1FPROC Uniform1f;
extern PFNGLUNIFORM2FPROC Uniform2f;
extern PFNGLUNIFORM3FPROC Uniform3f;
extern PFNGLUNIFORM4FPROC Uniform4f;

extern PFNGLGENVERTEXARRAYSPROC GenVertexArrays;
extern PFNGLBINDVERTEXARRAYPROC BindVertexArray;
extern PFNGLDELETEVERTEXARRAYSPROC DeleteVertexArrays;
extern PFNGLGENBUFFERSPROC GenBuffers;
extern PFNGLBINDBUFFERPROC BindBuffer;
extern PFNGLBUFFERDATAPROC BufferData;
extern PFNGLBUFFERSUBDATAPROC BufferSubData;
extern PFNGLDELETEBUFFERSPROC DeleteBuffers;
extern PFNGLVERTEXATTRIBPOINTERPROC VertexAttribPointer;
extern PFNGLENABLEVERTEXATTRIBARRAYPROC EnableVertexAttribArray;

extern PFNGLACTIVETEXTUREPROC ActiveTexture;

extern PFNGLGENFRAMEBUFFERSPROC GenFramebuffers;
extern PFNGLBINDFRAMEBUFFERPROC BindFramebuffer;
extern PFNGLFRAMEBUFFERTEXTURE2DPROC FramebufferTexture2D;
extern PFNGLCHECKFRAMEBUFFERSTATUSPROC CheckFramebufferStatus;
extern PFNGLDELETEFRAMEBUFFERSPROC DeleteFramebuffers;

// Loads every entry point above via SDL_GL_GetProcAddress(). Must be called
// once, after an SDL_GLContext is current. Returns false if any of them are
// missing (i.e. the driver can't actually give us a GL 3.3 core context).
bool load();

}
