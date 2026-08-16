#pragma once

#include "gl/GLLoader.hpp"

class Shader
{
    public:
        // Compiles and links a vertex+fragment program. Returns 0 on failure
        // (errors are logged to stdout).
        static GLuint compile_program(const char *vertex_src, const char *fragment_src);
    private:
        static GLuint compile_stage(GLenum stage, const char *src);
};
