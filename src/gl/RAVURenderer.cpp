#include "gl/RAVURenderer.hpp"
#include "gl/RAVUWeights.hpp"
#include "gl/Shader.hpp"
#include "utils/Log.hpp"

namespace {

// Same v-flip convention as FSRCNNRenderer/VideoRenderer's static quads:
// only the two texture()-based prep passes need it (see those files'
// comments for why). Everything else here uses gl_FragCoord + texelFetch,
// which is immune to it.
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

// Faithful port of RAVU-Lite's core algorithm (extract_key +
// apply_convolution_kernel in ravu-lite.py): per source luma pixel, build a
// 3x3 local gradient structure tensor (Gaussian-weighted), eigen-decompose
// it into (angle, strength, coherence), quantize into one of 288 buckets,
// and apply that bucket's trained 3x3 filter kernel to produce all 4 of
// this pixel's 2x-upscale sub-pixel outputs (packed into one RGBA texel).
// The kernel is stored using its 180-degree rotational symmetry (only 5 of
// 9 taps kept; the other 4 are recovered via the w.wzyx swizzle below,
// exactly like the reference does) - see RAVUWeights.hpp.
const char *fragment_src_step1 =
    "#version 330 core\n"
    "out vec4 FragColor;\n"
    "uniform sampler2D uTex;\n"
    "uniform sampler2D uLUT;\n"
    "uniform ivec2 uSize;\n"
    "const float PI = 3.14159265358979323846;\n"
    "const float EPS = 1.192092896e-7;\n"
    "const float kGaussian[9] = float[9](\n"
    "    0.1018680644198163, 0.11543163961422666, 0.1018680644198163,\n"
    "    0.11543163961422666, 0.13080118386382833, 0.11543163961422666,\n"
    "    0.1018680644198163, 0.11543163961422666, 0.1018680644198163\n"
    ");\n"
    "const float kMinStrength[3] = float[3](0.004, 0.016, 0.05);\n"
    "const float kMinCoherence[2] = float[2](0.25, 0.5);\n"
    "float fetchClampedR(ivec2 p) {\n"
    "    p = clamp(p, ivec2(0), uSize - ivec2(1));\n"
    "    return texelFetch(uTex, p, 0).r;\n"
    "}\n"
    "void main() {\n"
    "    ivec2 pix = ivec2(gl_FragCoord.xy - vec2(0.5));\n"
    "    float s[9];\n"
    "    for (int i = 0; i < 3; i++)\n"
    "        for (int j = 0; j < 3; j++)\n"
    "            s[i * 3 + j] = fetchClampedR(pix + ivec2(i - 1, j - 1));\n"
    "\n"
    "    vec3 abd = vec3(0.0);\n"
    "    for (int i = 0; i < 3; i++) {\n"
    "        for (int j = 0; j < 3; j++) {\n"
    "            float gx, gy;\n"
    "            if (i == 0) gx = s[1 * 3 + j] - s[0 * 3 + j];\n"
    "            else if (i == 2) gx = s[2 * 3 + j] - s[1 * 3 + j];\n"
    "            else gx = (s[2 * 3 + j] - s[0 * 3 + j]) / 2.0;\n"
    "            if (j == 0) gy = s[i * 3 + 1] - s[i * 3 + 0];\n"
    "            else if (j == 2) gy = s[i * 3 + 2] - s[i * 3 + 1];\n"
    "            else gy = (s[i * 3 + 2] - s[i * 3 + 0]) / 2.0;\n"
    "            float gw = kGaussian[i * 3 + j];\n"
    "            abd += vec3(gx * gx, gx * gy, gy * gy) * gw;\n"
    "        }\n"
    "    }\n"
    "\n"
    "    float a = abd.x, b = abd.y, dd = abd.z;\n"
    "    float T = a + dd, D = a * dd - b * b;\n"
    "    float delta = sqrt(max(T * T / 4.0 - D, 0.0));\n"
    "    float L1 = T / 2.0 + delta, L2 = T / 2.0 - delta;\n"
    "    float sqrtL1 = sqrt(L1), sqrtL2 = sqrt(L2);\n"
    "    float theta = (abs(b) < EPS) ? 0.0 : mod(atan(L1 - a, b) + PI, PI);\n"
    "    float lambda = sqrtL1;\n"
    "    float mu = (sqrtL1 + sqrtL2 < EPS) ? 0.0 : (sqrtL1 - sqrtL2) / (sqrtL1 + sqrtL2);\n"
    "\n"
    "    int angleB = clamp(int(floor(theta * 24.0 / PI)), 0, 23);\n"
    "    int strengthB = 0;\n"
    "    if (lambda >= kMinStrength[0]) strengthB = 1;\n"
    "    if (lambda >= kMinStrength[1]) strengthB = 2;\n"
    "    if (lambda >= kMinStrength[2]) strengthB = 3;\n"
    "    int coherenceB = 0;\n"
    "    if (mu >= kMinCoherence[0]) coherenceB = 1;\n"
    "    if (mu >= kMinCoherence[1]) coherenceB = 2;\n"
    "\n"
    "    int bucketRow = (angleB * 4 + strengthB) * 3 + coherenceB;\n"
    "\n"
    "    vec4 res = vec4(0.0);\n"
    "    for (int i = 0; i < 5; i++) {\n"
    "        vec4 w = texelFetch(uLUT, ivec2(i, bucketRow), 0);\n"
    "        int j = 8 - i;\n"
    "        if (i < j) res += s[i] * w + s[j] * w.wzyx;\n"
    "        else res += s[i] * w;\n"
    "    }\n"
    "    FragColor = clamp(res, 0.0, 1.0);\n"
    "}\n";

// Unpack: unlike FSRCNN's TF depth_to_space (whose channel order is
// documented), this idx formula is reverse-engineered from RAVU-Lite's own
// mpv step2 shader (`fract(HOOKED_pos*HOOKED_size)-0.5` / `dir.x>0`/`dir.y>0`
// tracing out to col_offset*2+row_offset against the *bound* (native-res)
// intermediate texture) - verified numerically against a from-scratch
// Python port of the full algorithm on synthetic input, the same way the
// FSRCNN depth-to-space bias bug was caught, rather than trusted blind.
const char *fragment_src_step2 =
    "#version 330 core\n"
    "out vec4 FragColor;\n"
    "uniform sampler2D uTex;\n"
    "void main() {\n"
    "    ivec2 pix = ivec2(gl_FragCoord.xy - vec2(0.5));\n"
    "    ivec2 inPix = pix / 2;\n"
    "    ivec2 sub = pix - inPix * 2;\n"
    "    vec4 v = texelFetch(uTex, inPix, 0);\n"
    "    int idx = sub.x * 2 + sub.y;\n"
    "    float y = (idx == 0) ? v.x : (idx == 1) ? v.y : (idx == 2) ? v.z : v.w;\n"
    "    FragColor = vec4(clamp(y, 0.0, 1.0), 0.0, 0.0, 1.0);\n"
    "}\n";

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

GLuint g_prog_y_prep = 0, g_prog_cbcr_prep = 0, g_prog_step1 = 0, g_prog_step2 = 0;
GLuint g_prog_chroma_resize = 0, g_prog_recombine = 0;

GLint g_loc_step1_tex = -1, g_loc_step1_lut = -1, g_loc_step1_size = -1;
GLint g_loc_step2_tex = -1;
GLint g_loc_resize_tex = -1, g_loc_resize_size = -1;
GLint g_loc_recombine_y = -1, g_loc_recombine_cbcr = -1;

GLuint g_vao = 0, g_vbo = 0;
GLuint g_tex_lut = 0;

GLuint g_tex_y_in = 0, g_fbo_y_in = 0;
GLuint g_tex_cbcr_in = 0, g_fbo_cbcr_in = 0;
GLuint g_tex_step1 = 0, g_fbo_step1 = 0;
GLuint g_tex_y_out = 0, g_fbo_y_out = 0;
GLuint g_tex_cbcr_out = 0, g_fbo_cbcr_out = 0;
GLuint g_tex_final = 0, g_fbo_final = 0;
int g_res_w = 0, g_res_h = 0;
int g_out_w = 0, g_out_h = 0;

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
        Log::error() << "RAVU " << name << " framebuffer is incomplete (status 0x"
                     << std::hex << status << std::dec << ").";
        return false;
    }
    return true;
}

bool ensure_sizes(int w, int h)
{
    if (g_res_w == w && g_res_h == h && g_tex_final)
        return true;

    int ow = w * 2, oh = h * 2;
    if (!make_target(g_tex_y_in, g_fbo_y_in, GL_R32F, GL_RED, w, h, "y_in")) return false;
    if (!make_target(g_tex_cbcr_in, g_fbo_cbcr_in, GL_RG32F, GL_RG, w, h, "cbcr_in")) return false;
    if (!make_target(g_tex_step1, g_fbo_step1, GL_RGBA32F, GL_RGBA, w, h, "step1")) return false;
    if (!make_target(g_tex_y_out, g_fbo_y_out, GL_R32F, GL_RED, ow, oh, "y_out")) return false;
    if (!make_target(g_tex_cbcr_out, g_fbo_cbcr_out, GL_RG32F, GL_RG, ow, oh, "cbcr_out")) return false;
    if (!make_target(g_tex_final, g_fbo_final, GL_RGBA32F, GL_RGBA, ow, oh, "final")) return false;

    g_res_w = w; g_res_h = h;
    g_out_w = ow; g_out_h = oh;
    return true;
}

void draw_quad()
{
    gl::BindVertexArray(g_vao);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}

} // namespace

bool RAVURenderer::init()
{
    gl::GenVertexArrays(1, &g_vao);
    gl::GenBuffers(1, &g_vbo);
    gl::BindVertexArray(g_vao);
    gl::BindBuffer(GL_ARRAY_BUFFER, g_vbo);
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
    g_prog_step1 = Shader::compile_program(vertex_src, fragment_src_step1);
    g_prog_step2 = Shader::compile_program(vertex_src, fragment_src_step2);
    g_prog_chroma_resize = Shader::compile_program(vertex_src, fragment_src_chroma_resize);
    g_prog_recombine = Shader::compile_program(vertex_src, fragment_src_recombine);
    if (!g_prog_y_prep || !g_prog_cbcr_prep || !g_prog_step1 || !g_prog_step2 || !g_prog_chroma_resize || !g_prog_recombine)
        return false;

    g_loc_step1_tex = gl::GetUniformLocation(g_prog_step1, "uTex");
    g_loc_step1_lut = gl::GetUniformLocation(g_prog_step1, "uLUT");
    g_loc_step1_size = gl::GetUniformLocation(g_prog_step1, "uSize");
    g_loc_step2_tex = gl::GetUniformLocation(g_prog_step2, "uTex");
    g_loc_resize_tex = gl::GetUniformLocation(g_prog_chroma_resize, "uTex");
    g_loc_resize_size = gl::GetUniformLocation(g_prog_chroma_resize, "uSrcSize");
    g_loc_recombine_y = gl::GetUniformLocation(g_prog_recombine, "uY");
    g_loc_recombine_cbcr = gl::GetUniformLocation(g_prog_recombine, "uCbCr");

    glGenTextures(1, &g_tex_lut);
    glBindTexture(GL_TEXTURE_2D, g_tex_lut);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, 5, 288, 0, GL_RGBA, GL_FLOAT, ravu_lite_r2_lut);

    return true;
}

void RAVURenderer::destroy()
{
    if (g_tex_lut) glDeleteTextures(1, &g_tex_lut);
    GLuint *texes[] = { &g_tex_y_in, &g_tex_cbcr_in, &g_tex_step1, &g_tex_y_out, &g_tex_cbcr_out, &g_tex_final };
    GLuint *fbos[] = { &g_fbo_y_in, &g_fbo_cbcr_in, &g_fbo_step1, &g_fbo_y_out, &g_fbo_cbcr_out, &g_fbo_final };
    for (int i = 0; i < 6; i++) {
        if (*texes[i]) glDeleteTextures(1, texes[i]);
        if (*fbos[i]) gl::DeleteFramebuffers(1, fbos[i]);
        *texes[i] = *fbos[i] = 0;
    }
    if (g_vbo) gl::DeleteBuffers(1, &g_vbo);
    if (g_vao) gl::DeleteVertexArrays(1, &g_vao);
    if (g_prog_y_prep) gl::DeleteProgram(g_prog_y_prep);
    if (g_prog_cbcr_prep) gl::DeleteProgram(g_prog_cbcr_prep);
    if (g_prog_step1) gl::DeleteProgram(g_prog_step1);
    if (g_prog_step2) gl::DeleteProgram(g_prog_step2);
    if (g_prog_chroma_resize) gl::DeleteProgram(g_prog_chroma_resize);
    if (g_prog_recombine) gl::DeleteProgram(g_prog_recombine);
    g_prog_y_prep = g_prog_cbcr_prep = g_prog_step1 = g_prog_step2 = g_prog_chroma_resize = g_prog_recombine = 0;
    g_tex_lut = g_vao = g_vbo = 0;
    g_res_w = g_res_h = g_out_w = g_out_h = 0;
}

GLuint RAVURenderer::run(GLuint rgb_tex, int src_w, int src_h, int &out_w, int &out_h)
{
    if (!ensure_sizes(src_w, src_h)) {
        out_w = out_h = 0;
        return 0;
    }

    gl::BindVertexArray(g_vao);

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

    // Step 1: gradient analysis + kernel lookup, packs all 4 sub-pixel
    // outputs for each source pixel into one RGBA texel (native res).
    gl::BindFramebuffer(GL_FRAMEBUFFER, g_fbo_step1);
    gl::UseProgram(g_prog_step1);
    gl::ActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, g_tex_y_in);
    gl::ActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, g_tex_lut);
    gl::Uniform1i(g_loc_step1_tex, 0);
    gl::Uniform1i(g_loc_step1_lut, 1);
    gl::Uniform2i(g_loc_step1_size, src_w, src_h);
    draw_quad();

    // Step 2: unpack to full 2x resolution.
    glViewport(0, 0, g_out_w, g_out_h);
    gl::BindFramebuffer(GL_FRAMEBUFFER, g_fbo_y_out);
    gl::UseProgram(g_prog_step2);
    gl::ActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, g_tex_step1);
    gl::Uniform1i(g_loc_step2_tex, 0);
    draw_quad();

    gl::BindFramebuffer(GL_FRAMEBUFFER, g_fbo_cbcr_out);
    gl::UseProgram(g_prog_chroma_resize);
    gl::ActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, g_tex_cbcr_in);
    gl::Uniform1i(g_loc_resize_tex, 0);
    gl::Uniform2i(g_loc_resize_size, src_w, src_h);
    draw_quad();

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
