#include "gl/FSRCNNRenderer.hpp"
#include "gl/FSRCNNWeights.hpp"
#include "gl/Shader.hpp"
#include "utils/Log.hpp"
#include <sstream>
#include <vector>
#include <cstdio>

namespace {

// Shared by every pass in this module: a fullscreen quad. Only the two
// colorspace-prep passes (Y/CbCr from RGB) actually read the UV attribute
// (via texture(), for hardware bilinear filtering isn't even needed there,
// but it's the simplest way to sample the single input texel-for-texel).
// Every other pass here (all 44 conv passes, depth-to-space, chroma
// resize, recombine) addresses texels directly via gl_FragCoord +
// texelFetch - which reads and writes raw memory rows with no UV
// indirection, so those passes are immune to the "render-to-texture
// inverts row order" issue that the video decode/EASU passes have to
// compensate for (see VideoRenderer.cpp's static_verts comment). Only the
// two texture()-based prep passes need that compensation, so this quad
// uses the same v-flipped convention as VideoRenderer's g_vao_static:
// with that, sampling the (already correctly-oriented) input RGB texture
// straight through produces a correctly-oriented output, and everything
// downstream preserves that via texelFetch.
const char *vertex_src =
    "#version 330 core\n"
    "layout(location = 0) in vec2 aPos;\n"
    "layout(location = 1) in vec2 aTexCoord;\n"
    "out vec2 vTexCoord;\n"
    "void main() {\n"
    "    gl_Position = vec4(aPos, 0.0, 1.0);\n"
    "    vTexCoord = aTexCoord;\n"
    "}\n";

const char *fragment_src_y_prep =
    "#version 330 core\n"
    "in vec2 vTexCoord;\n"
    "out vec4 FragColor;\n"
    "uniform sampler2D uTex;\n"
    "void main() {\n"
    "    vec3 c = texture(uTex, vTexCoord).rgb;\n"
    "    float y = 0.299 * c.r + 0.587 * c.g + 0.114 * c.b;\n"
    "    FragColor = vec4(y, 0.0, 0.0, 1.0);\n"
    "}\n";

const char *fragment_src_cbcr_prep =
    "#version 330 core\n"
    "in vec2 vTexCoord;\n"
    "out vec4 FragColor;\n"
    "uniform sampler2D uTex;\n"
    "void main() {\n"
    "    vec3 c = texture(uTex, vTexCoord).rgb;\n"
    "    float cb = -0.168736 * c.r - 0.331264 * c.g + 0.5 * c.b + 0.5;\n"
    "    float cr =  0.5 * c.r - 0.418688 * c.g - 0.081312 * c.b + 0.5;\n"
    "    FragColor = vec4(cb, cr, 0.0, 1.0);\n"
    "}\n";

// DepthToSpace(block=2) + bias, matching TF's NHWC channel-to-block mapping:
// output[i,j] = input[i/2, j/2, (i%2)*2 + (j%2)]. gl_FragCoord/texelFetch
// throughout means "i"/"j" here are the same row/col space conv8's output
// was computed in - no reorientation needed.
const char *fragment_src_depth_to_space =
    "#version 330 core\n"
    "out vec4 FragColor;\n"
    "uniform sampler2D uTex;\n"
    "uniform float uBias;\n"
    "void main() {\n"
    "    ivec2 pix = ivec2(gl_FragCoord.xy - vec2(0.5));\n"
    "    ivec2 inPix = pix / 2;\n"
    "    ivec2 sub = pix - inPix * 2;\n"
    "    vec4 v = texelFetch(uTex, inPix, 0);\n"
    "    int idx = sub.y * 2 + sub.x;\n"
    "    float y = (idx == 0) ? v.x : (idx == 1) ? v.y : (idx == 2) ? v.z : v.w;\n"
    "    y += uBias;\n"
    "    FragColor = vec4(clamp(y, 0.0, 1.0), 0.0, 0.0, 1.0);\n"
    "}\n";

// Bilinear resize, texelFetch-based (not texture()) so it stays orientation-
// neutral like the rest of this module instead of needing its own flip
// compensation.
const char *fragment_src_chroma_resize =
    "#version 330 core\n"
    "out vec4 FragColor;\n"
    "uniform sampler2D uTex;\n"
    "uniform ivec2 uSrcSize;\n"
    "vec4 fetchClamped(ivec2 p) {\n"
    "    p = clamp(p, ivec2(0), uSrcSize - ivec2(1));\n"
    "    return texelFetch(uTex, p, 0);\n"
    "}\n"
    "void main() {\n"
    "    ivec2 pix = ivec2(gl_FragCoord.xy - vec2(0.5));\n"
    "    vec2 srcPos = (vec2(pix) + 0.5) * 0.5 - 0.5;\n"
    "    vec2 base = floor(srcPos);\n"
    "    vec2 f = srcPos - base;\n"
    "    ivec2 p00 = ivec2(base);\n"
    "    vec4 c00 = fetchClamped(p00);\n"
    "    vec4 c10 = fetchClamped(p00 + ivec2(1, 0));\n"
    "    vec4 c01 = fetchClamped(p00 + ivec2(0, 1));\n"
    "    vec4 c11 = fetchClamped(p00 + ivec2(1, 1));\n"
    "    FragColor = mix(mix(c00, c10, f.x), mix(c01, c11, f.x), f.y);\n"
    "}\n";

const char *fragment_src_recombine =
    "#version 330 core\n"
    "out vec4 FragColor;\n"
    "uniform sampler2D uY;\n"
    "uniform sampler2D uCbCr;\n"
    "void main() {\n"
    "    ivec2 pix = ivec2(gl_FragCoord.xy - vec2(0.5));\n"
    "    float y = texelFetch(uY, pix, 0).r;\n"
    "    vec2 cbcr = texelFetch(uCbCr, pix, 0).rg;\n"
    "    float cb = cbcr.x - 0.5;\n"
    "    float cr = cbcr.y - 0.5;\n"
    "    vec3 rgb = vec3(\n"
    "        y + 1.402 * cr,\n"
    "        y - 0.344136 * cb - 0.714136 * cr,\n"
    "        y + 1.772 * cb\n"
    "    );\n"
    "    FragColor = vec4(clamp(rgb, 0.0, 1.0), 1.0);\n"
    "}\n";

// Formats a valid GLSL float literal. "%.9g" alone isn't safe to suffix
// with "f" directly - e.g. 0.0 formats as "0", and "0f" is not a legal
// GLSL float literal (needs a decimal point or exponent).
std::string fmt_float(float v)
{
    char tmp[64];
    snprintf(tmp, sizeof tmp, "%.9g", v);
    std::string s(tmp);
    if (s.find_first_of(".eEnN") == std::string::npos)
        s += ".0";
    s += "f";
    return s;
}

// Builds one conv-layer output-group's fragment shader: reads `inGroups`
// RGBA32F input textures (or one single-channel texture when
// singleChannelInput, for conv1's luma input), accumulates a K x K
// zero-padded (TF SAME-padding-with-zeros, not edge clamp) convolution into
// up to 4 output channels, applies PReLU if `alpha` is given. `weights` must
// already be sliced/reordered to this output group's layout:
// [kh][kw][cin (0..inGroups*4 or 1)][local out 0..3].
std::string build_conv_shader(int kernelSize, int inGroups, bool singleChannelInput,
                               const std::vector<float> &weights, const float bias[4],
                               const float *alpha)
{
    std::ostringstream ss;
    ss << "#version 330 core\n";
    ss << "out vec4 FragColor;\n";
    int nSamplers = singleChannelInput ? 1 : inGroups;
    for (int i = 0; i < nSamplers; i++)
        ss << "uniform sampler2D uIn" << i << ";\n";
    ss << "uniform ivec2 uSize;\n";
    ss << "vec4 fetchZP(sampler2D tex, ivec2 p) {\n"
          "    if (p.x < 0 || p.y < 0 || p.x >= uSize.x || p.y >= uSize.y) return vec4(0.0);\n"
          "    return texelFetch(tex, p, 0);\n"
          "}\n";
    ss << "void main() {\n";
    ss << "    ivec2 pix = ivec2(gl_FragCoord.xy - vec2(0.5));\n";

    char buf[256];
    ss << "    vec4 acc = vec4(" << fmt_float(bias[0]) << ", " << fmt_float(bias[1]) << ", "
       << fmt_float(bias[2]) << ", " << fmt_float(bias[3]) << ");\n";

    int half = kernelSize / 2;
    int totalCin = singleChannelInput ? 1 : inGroups * 4;

    for (int kh = 0; kh < kernelSize; kh++) {
        for (int kw = 0; kw < kernelSize; kw++) {
            int dy = kh - half, dx = kw - half;
            ss << "    {\n";
            if (dx == 0 && dy == 0) {
                ss << "        ivec2 p = pix;\n";
            } else {
                snprintf(buf, sizeof buf, "        ivec2 p = pix + ivec2(%d, %d);\n", dx, dy);
                ss << buf;
            }
            if (singleChannelInput) {
                ss << "        vec4 v0 = fetchZP(uIn0, p);\n";
                int base = (kh * kernelSize + kw) * 1 * 4; // cin=0 only
                ss << "        acc += v0.r * vec4(" << fmt_float(weights[base + 0]) << ", "
                   << fmt_float(weights[base + 1]) << ", " << fmt_float(weights[base + 2]) << ", "
                   << fmt_float(weights[base + 3]) << ");\n";
            } else {
                for (int g = 0; g < inGroups; g++) {
                    snprintf(buf, sizeof buf, "        vec4 v%d = fetchZP(uIn%d, p);\n", g, g);
                    ss << buf;
                }
                for (int co = 0; co < 4; co++) {
                    ss << "        acc[" << co << "] += ";
                    for (int g = 0; g < inGroups; g++) {
                        int base = (kh * kernelSize + kw) * totalCin + 4 * g;
                        float w0 = weights[(base + 0) * 4 + co];
                        float w1 = weights[(base + 1) * 4 + co];
                        float w2 = weights[(base + 2) * 4 + co];
                        float w3 = weights[(base + 3) * 4 + co];
                        ss << "dot(v" << g << ", vec4(" << fmt_float(w0) << ", " << fmt_float(w1) << ", "
                           << fmt_float(w2) << ", " << fmt_float(w3) << "))" << ((g + 1 < inGroups) ? " + " : "");
                    }
                    ss << ";\n";
                }
            }
            ss << "    }\n";
        }
    }

    if (alpha) {
        ss << "    vec4 a = vec4(" << fmt_float(alpha[0]) << ", " << fmt_float(alpha[1]) << ", "
           << fmt_float(alpha[2]) << ", " << fmt_float(alpha[3]) << ");\n";
        ss << "    acc = max(acc, vec4(0.0)) + a * min(acc, vec4(0.0));\n";
    }
    ss << "    FragColor = acc;\n";
    ss << "}\n";
    return ss.str();
}

// Reslices a TF conv2d filter [kh][kw][cin][cout] (row-major) down to one
// output group's [kh][kw][cin][4] (cout in [group*4, group*4+3]).
std::vector<float> slice_output_group(const float *full, int K, int cin, int cout, int group)
{
    std::vector<float> out((size_t)K * K * cin * 4, 0.0f);
    for (int kh = 0; kh < K; kh++)
        for (int kw = 0; kw < K; kw++)
            for (int c = 0; c < cin; c++)
                for (int co = 0; co < 4; co++) {
                    int coutIdx = group * 4 + co;
                    if (coutIdx < cout)
                        out[((size_t)(kh * K + kw) * cin + c) * 4 + co] =
                            full[((size_t)(kh * K + kw) * cin + c) * cout + coutIdx];
                }
    return out;
}

void slice4(const float *full, int count, int group, float out[4])
{
    for (int i = 0; i < 4; i++) {
        int idx = group * 4 + i;
        out[i] = (idx < count) ? full[idx] : 0.0f;
    }
}

// -- Layer topology: d=56, s=12, m=4 mapping layers, scale=2 (PS=4) --
const int kD = 56, kS = 12, kPS = 4;
const int kGroupsD = kD / 4;  // 14
const int kGroupsS = kS / 4;  // 3
const int kGroupsPS = kPS / 4; // 1

GLuint g_prog_conv1[14];
GLuint g_prog_conv2[3];
GLuint g_prog_mapping[4][3];
GLuint g_prog_conv7[14];
GLuint g_prog_conv8[1];
GLuint g_prog_y_prep = 0, g_prog_cbcr_prep = 0, g_prog_depth_to_space = 0;
GLuint g_prog_chroma_resize = 0, g_prog_recombine = 0;

GLint g_loc_dts_tex = -1, g_loc_dts_bias = -1;
GLint g_loc_resize_tex = -1, g_loc_resize_size = -1;
GLint g_loc_recombine_y = -1, g_loc_recombine_cbcr = -1;

GLuint g_vao = 0, g_vbo = 0;

// Ping-pong channel-group texture pool, up to 14 groups per set (56ch max).
GLuint g_group_tex[2][14] = {{0}};
GLuint g_group_fbo[2][14] = {{0}};
int g_group_w = 0, g_group_h = 0;

GLuint g_tex_y_in = 0, g_fbo_y_in = 0;
GLuint g_tex_cbcr_in = 0, g_fbo_cbcr_in = 0;
GLuint g_tex_y_out = 0, g_fbo_y_out = 0;
GLuint g_tex_cbcr_out = 0, g_fbo_cbcr_out = 0;
GLuint g_tex_final = 0, g_fbo_final = 0;
int g_res_w = 0, g_res_h = 0; // sizes of the *_in/group textures
int g_out_w = 0, g_out_h = 0; // sizes of the *_out/final textures (2x)

GLuint make_r32f_texture()
{
    GLuint tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    return tex;
}

bool make_target(GLuint &tex, GLuint &fbo, GLenum internalFmt, GLenum format, int w, int h, const char *name)
{
    if (!tex) tex = make_r32f_texture();
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, internalFmt, w, h, 0, format, GL_FLOAT, NULL);

    if (!fbo) gl::GenFramebuffers(1, &fbo);
    gl::BindFramebuffer(GL_FRAMEBUFFER, fbo);
    gl::FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);
    GLenum status = gl::CheckFramebufferStatus(GL_FRAMEBUFFER);
    gl::BindFramebuffer(GL_FRAMEBUFFER, 0);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        Log::error() << "FSRCNN " << name << " framebuffer is incomplete (status 0x"
                     << std::hex << status << std::dec << ").";
        return false;
    }
    return true;
}

bool ensure_sizes(int w, int h)
{
    bool resWChanged = (g_res_w != w || g_res_h != h);
    if (!resWChanged && g_tex_final)
        return true;

    int ow = w * 2, oh = h * 2;

    for (int set = 0; set < 2; set++)
        for (int i = 0; i < 14; i++)
            if (!make_target(g_group_tex[set][i], g_group_fbo[set][i], GL_RGBA32F, GL_RGBA, w, h, "group"))
                return false;

    if (!make_target(g_tex_y_in, g_fbo_y_in, GL_R32F, GL_RED, w, h, "y_in")) return false;
    if (!make_target(g_tex_cbcr_in, g_fbo_cbcr_in, GL_RG32F, GL_RG, w, h, "cbcr_in")) return false;
    if (!make_target(g_tex_y_out, g_fbo_y_out, GL_R32F, GL_RED, ow, oh, "y_out")) return false;
    if (!make_target(g_tex_cbcr_out, g_fbo_cbcr_out, GL_RG32F, GL_RG, ow, oh, "cbcr_out")) return false;
    if (!make_target(g_tex_final, g_fbo_final, GL_RGBA32F, GL_RGBA, ow, oh, "final")) return false;

    g_group_w = w; g_group_h = h;
    g_res_w = w; g_res_h = h;
    g_out_w = ow; g_out_h = oh;
    return true;
}

void draw_quad()
{
    gl::BindVertexArray(g_vao);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}

// Runs one conv layer: `programs[g]` for output group g reads all of
// `inTex[0..inCount-1]` and writes `outFbo[g]`.
void run_conv_layer(const GLuint *programs, int outGroups, const GLuint *inTex, int inCount,
                     const GLuint *outFbo, int w, int h, bool singleChannelInput)
{
    glViewport(0, 0, w, h);
    for (int g = 0; g < outGroups; g++) {
        gl::BindFramebuffer(GL_FRAMEBUFFER, outFbo[g]);
        gl::UseProgram(programs[g]);
        int nSamplers = singleChannelInput ? 1 : inCount;
        for (int i = 0; i < nSamplers; i++) {
            gl::ActiveTexture(GL_TEXTURE0 + i);
            glBindTexture(GL_TEXTURE_2D, inTex[i]);
            GLint loc = gl::GetUniformLocation(programs[g], (std::string("uIn") + std::to_string(i)).c_str());
            gl::Uniform1i(loc, i);
        }
        GLint sizeLoc = gl::GetUniformLocation(programs[g], "uSize");
        gl::Uniform2i(sizeLoc, w, h);
        draw_quad();
    }
}

} // namespace

bool FSRCNNRenderer::init()
{
    gl::GenVertexArrays(1, &g_vao);
    gl::GenBuffers(1, &g_vbo);
    gl::BindVertexArray(g_vao);
    gl::BindBuffer(GL_ARRAY_BUFFER, g_vbo);
    // Same v-flip convention as VideoRenderer's g_vao_static (see comment
    // at the top of this file / there): needed for the two texture()-based
    // prep passes, harmless/unused everywhere else here.
    static const float verts[16] = {
        -1.0f,  1.0f, 0.0f, 1.0f,
         1.0f,  1.0f, 1.0f, 1.0f,
        -1.0f, -1.0f, 0.0f, 0.0f,
         1.0f, -1.0f, 1.0f, 0.0f,
    };
    gl::BufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);
    gl::VertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void *)0);
    gl::EnableVertexAttribArray(0);
    gl::VertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void *)(2 * sizeof(float)));
    gl::EnableVertexAttribArray(1);
    gl::BindVertexArray(0);

    g_prog_y_prep = Shader::compile_program(vertex_src, fragment_src_y_prep);
    g_prog_cbcr_prep = Shader::compile_program(vertex_src, fragment_src_cbcr_prep);
    g_prog_depth_to_space = Shader::compile_program(vertex_src, fragment_src_depth_to_space);
    g_prog_chroma_resize = Shader::compile_program(vertex_src, fragment_src_chroma_resize);
    g_prog_recombine = Shader::compile_program(vertex_src, fragment_src_recombine);
    if (!g_prog_y_prep || !g_prog_cbcr_prep || !g_prog_depth_to_space || !g_prog_chroma_resize || !g_prog_recombine)
        return false;

    g_loc_dts_tex = gl::GetUniformLocation(g_prog_depth_to_space, "uTex");
    g_loc_dts_bias = gl::GetUniformLocation(g_prog_depth_to_space, "uBias");
    g_loc_resize_tex = gl::GetUniformLocation(g_prog_chroma_resize, "uTex");
    g_loc_resize_size = gl::GetUniformLocation(g_prog_chroma_resize, "uSrcSize");
    g_loc_recombine_y = gl::GetUniformLocation(g_prog_recombine, "uY");
    g_loc_recombine_cbcr = gl::GetUniformLocation(g_prog_recombine, "uCbCr");

    // conv1: 1 -> 56, 5x5, single-channel input, 14 output groups.
    for (int g = 0; g < kGroupsD; g++) {
        float bias[4], alpha[4];
        slice4(fsrcnn_b1, kD, g, bias);
        slice4(fsrcnn_alpha1, kD, g, alpha);
        auto w = slice_output_group(fsrcnn_f1, 5, 1, kD, g);
        std::string src = build_conv_shader(5, 1, true, w, bias, alpha);
        g_prog_conv1[g] = Shader::compile_program(vertex_src, src.c_str());
        if (!g_prog_conv1[g]) return false;
    }

    // conv2: 56 -> 12, 1x1, 3 output groups.
    for (int g = 0; g < kGroupsS; g++) {
        float bias[4], alpha[4];
        slice4(fsrcnn_b2, kS, g, bias);
        slice4(fsrcnn_alpha2, kS, g, alpha);
        auto w = slice_output_group(fsrcnn_f2, 1, kD, kS, g);
        std::string src = build_conv_shader(1, kGroupsD, false, w, bias, alpha);
        g_prog_conv2[g] = Shader::compile_program(vertex_src, src.c_str());
        if (!g_prog_conv2[g]) return false;
    }

    // conv3..6: 12 -> 12, 3x3, 4 mapping layers x 3 output groups.
    const float *mapF[4] = { fsrcnn_f3, fsrcnn_f4, fsrcnn_f5, fsrcnn_f6 };
    const float *mapB[4] = { fsrcnn_b3, fsrcnn_b4, fsrcnn_b5, fsrcnn_b6 };
    const float *mapA[4] = { fsrcnn_alpha3, fsrcnn_alpha4, fsrcnn_alpha5, fsrcnn_alpha6 };
    for (int layer = 0; layer < 4; layer++) {
        for (int g = 0; g < kGroupsS; g++) {
            float bias[4], alpha[4];
            slice4(mapB[layer], kS, g, bias);
            slice4(mapA[layer], kS, g, alpha);
            auto w = slice_output_group(mapF[layer], 3, kS, kS, g);
            std::string src = build_conv_shader(3, kGroupsS, false, w, bias, alpha);
            g_prog_mapping[layer][g] = Shader::compile_program(vertex_src, src.c_str());
            if (!g_prog_mapping[layer][g]) return false;
        }
    }

    // conv7: 12 -> 56, 1x1, 14 output groups.
    for (int g = 0; g < kGroupsD; g++) {
        float bias[4], alpha[4];
        slice4(fsrcnn_b7, kD, g, bias);
        slice4(fsrcnn_alpha7, kD, g, alpha);
        auto w = slice_output_group(fsrcnn_f7, 1, kS, kD, g);
        std::string src = build_conv_shader(1, kGroupsS, false, w, bias, alpha);
        g_prog_conv7[g] = Shader::compile_program(vertex_src, src.c_str());
        if (!g_prog_conv7[g]) return false;
    }

    // conv8: 56 -> 4, 1x1, no activation, 1 output group. conv8 itself has
    // NO bias in the source graph - b8 is added once, after depth-to-space
    // (see fsrcnn.py: bias_add happens on depth_to_space's output, not
    // conv8's), so conv8's own bias here must be all zero.
    {
        float bias[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
        auto w = slice_output_group(fsrcnn_f8, 1, kD, kPS, 0);
        std::string src = build_conv_shader(1, kGroupsD, false, w, bias, nullptr);
        g_prog_conv8[0] = Shader::compile_program(vertex_src, src.c_str());
        if (!g_prog_conv8[0]) return false;
    }

    return true;
}

void FSRCNNRenderer::destroy()
{
    for (int set = 0; set < 2; set++)
        for (int i = 0; i < 14; i++) {
            if (g_group_tex[set][i]) glDeleteTextures(1, &g_group_tex[set][i]);
            if (g_group_fbo[set][i]) gl::DeleteFramebuffers(1, &g_group_fbo[set][i]);
            g_group_tex[set][i] = g_group_fbo[set][i] = 0;
        }
    GLuint *texes[] = { &g_tex_y_in, &g_tex_cbcr_in, &g_tex_y_out, &g_tex_cbcr_out, &g_tex_final };
    GLuint *fbos[] = { &g_fbo_y_in, &g_fbo_cbcr_in, &g_fbo_y_out, &g_fbo_cbcr_out, &g_fbo_final };
    for (int i = 0; i < 5; i++) {
        if (*texes[i]) glDeleteTextures(1, texes[i]);
        if (*fbos[i]) gl::DeleteFramebuffers(1, fbos[i]);
        *texes[i] = *fbos[i] = 0;
    }
    if (g_vbo) gl::DeleteBuffers(1, &g_vbo);
    if (g_vao) gl::DeleteVertexArrays(1, &g_vao);
    for (int g = 0; g < kGroupsD; g++) { if (g_prog_conv1[g]) gl::DeleteProgram(g_prog_conv1[g]); if (g_prog_conv7[g]) gl::DeleteProgram(g_prog_conv7[g]); }
    for (int g = 0; g < kGroupsS; g++) if (g_prog_conv2[g]) gl::DeleteProgram(g_prog_conv2[g]);
    for (int l = 0; l < 4; l++) for (int g = 0; g < kGroupsS; g++) if (g_prog_mapping[l][g]) gl::DeleteProgram(g_prog_mapping[l][g]);
    if (g_prog_conv8[0]) gl::DeleteProgram(g_prog_conv8[0]);
    if (g_prog_y_prep) gl::DeleteProgram(g_prog_y_prep);
    if (g_prog_cbcr_prep) gl::DeleteProgram(g_prog_cbcr_prep);
    if (g_prog_depth_to_space) gl::DeleteProgram(g_prog_depth_to_space);
    if (g_prog_chroma_resize) gl::DeleteProgram(g_prog_chroma_resize);
    if (g_prog_recombine) gl::DeleteProgram(g_prog_recombine);
    g_res_w = g_res_h = g_out_w = g_out_h = g_group_w = g_group_h = 0;
}

GLuint FSRCNNRenderer::run(GLuint rgb_tex, int src_w, int src_h, int &out_w, int &out_h)
{
    if (!ensure_sizes(src_w, src_h)) {
        out_w = out_h = 0;
        return 0;
    }

    gl::BindVertexArray(g_vao);

    // Colorspace prep: RGB -> Y (native res) and Cb/Cr (native res).
    glViewport(0, 0, src_w, src_h);
    gl::BindFramebuffer(GL_FRAMEBUFFER, g_fbo_y_in);
    gl::UseProgram(g_prog_y_prep);
    gl::ActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, rgb_tex);
    gl::Uniform1i(gl::GetUniformLocation(g_prog_y_prep, "uTex"), 0);
    draw_quad();

    gl::BindFramebuffer(GL_FRAMEBUFFER, g_fbo_cbcr_in);
    gl::UseProgram(g_prog_cbcr_prep);
    gl::ActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, rgb_tex);
    gl::Uniform1i(gl::GetUniformLocation(g_prog_cbcr_prep, "uTex"), 0);
    draw_quad();

    // conv1 (1 -> 56): set 0 gets the 14 groups.
    GLuint yin[1] = { g_tex_y_in };
    run_conv_layer(g_prog_conv1, kGroupsD, yin, 1, g_group_fbo[0], src_w, src_h, true);

    // conv2 (56 -> 12): set 0 -> set 1 (3 groups).
    run_conv_layer(g_prog_conv2, kGroupsS, g_group_tex[0], kGroupsD, g_group_fbo[1], src_w, src_h, false);

    // conv3..6 (12 -> 12) x4: ping-pong set1 <-> set0 (3 groups each).
    int cur = 1;
    for (int layer = 0; layer < 4; layer++) {
        int next = 1 - cur;
        run_conv_layer(g_prog_mapping[layer], kGroupsS, g_group_tex[cur], kGroupsS, g_group_fbo[next], src_w, src_h, false);
        cur = next;
    }
    // cur now holds the last mapping layer's output (3 groups).

    // conv7 (12 -> 56): cur -> the OTHER set's 14 groups.
    int expandSet = 1 - cur;
    run_conv_layer(g_prog_conv7, kGroupsD, g_group_tex[cur], kGroupsS, g_group_fbo[expandSet], src_w, src_h, false);

    // conv8 (56 -> 4): expandSet -> cur's group[0] (1 group, reused slot).
    run_conv_layer(g_prog_conv8, kGroupsPS, g_group_tex[expandSet], kGroupsD, g_group_fbo[cur], src_w, src_h, false);

    // Depth-to-space + bias: group[cur][0] (4ch, native res) -> y_out (1ch, 2x res).
    glViewport(0, 0, g_out_w, g_out_h);
    gl::BindFramebuffer(GL_FRAMEBUFFER, g_fbo_y_out);
    gl::UseProgram(g_prog_depth_to_space);
    gl::ActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, g_group_tex[cur][0]);
    gl::Uniform1i(g_loc_dts_tex, 0);
    gl::Uniform1f(g_loc_dts_bias, fsrcnn_b8[0]);
    draw_quad();

    // Chroma resize: cbcr_in (native res) -> cbcr_out (2x res).
    gl::BindFramebuffer(GL_FRAMEBUFFER, g_fbo_cbcr_out);
    gl::UseProgram(g_prog_chroma_resize);
    gl::ActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, g_tex_cbcr_in);
    gl::Uniform1i(g_loc_resize_tex, 0);
    gl::Uniform2i(g_loc_resize_size, src_w, src_h);
    draw_quad();

    // Recombine: y_out + cbcr_out -> final RGBA (2x res).
    gl::BindFramebuffer(GL_FRAMEBUFFER, g_fbo_final);
    gl::UseProgram(g_prog_recombine);
    gl::ActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, g_tex_y_out);
    gl::ActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, g_tex_cbcr_out);
    gl::Uniform1i(g_loc_recombine_y, 0);
    gl::Uniform1i(g_loc_recombine_cbcr, 1);
    draw_quad();

    gl::BindVertexArray(0);

    out_w = g_out_w;
    out_h = g_out_h;
    return g_tex_final;
}
