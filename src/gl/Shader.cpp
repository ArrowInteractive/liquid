#include "gl/Shader.hpp"
#include "utils/Log.hpp"
#include <vector>

GLuint Shader::compile_stage(GLenum stage, const char *src)
{
    GLuint id = gl::CreateShader(stage);
    gl::ShaderSource(id, 1, &src, NULL);
    gl::CompileShader(id);

    GLint success = GL_FALSE;
    gl::GetShaderiv(id, GL_COMPILE_STATUS, &success);
    if (!success) {
        GLint log_len = 0;
        gl::GetShaderiv(id, GL_INFO_LOG_LENGTH, &log_len);
        std::vector<char> log(log_len > 0 ? log_len : 1);
        gl::GetShaderInfoLog(id, (GLsizei)log.size(), NULL, log.data());
        Log::error() << "Shader compilation failed: " << log.data();
        gl::DeleteShader(id);
        return 0;
    }
    return id;
}

GLuint Shader::compile_program(const char *vertex_src, const char *fragment_src)
{
    GLuint vs = compile_stage(GL_VERTEX_SHADER, vertex_src);
    if (!vs)
        return 0;
    GLuint fs = compile_stage(GL_FRAGMENT_SHADER, fragment_src);
    if (!fs) {
        gl::DeleteShader(vs);
        return 0;
    }

    GLuint program = gl::CreateProgram();
    gl::AttachShader(program, vs);
    gl::AttachShader(program, fs);
    gl::LinkProgram(program);

    gl::DeleteShader(vs);
    gl::DeleteShader(fs);

    GLint success = GL_FALSE;
    gl::GetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        GLint log_len = 0;
        gl::GetProgramiv(program, GL_INFO_LOG_LENGTH, &log_len);
        std::vector<char> log(log_len > 0 ? log_len : 1);
        gl::GetProgramInfoLog(program, (GLsizei)log.size(), NULL, log.data());
        Log::error() << "Shader program linking failed: " << log.data();
        gl::DeleteProgram(program);
        return 0;
    }
    return program;
}
