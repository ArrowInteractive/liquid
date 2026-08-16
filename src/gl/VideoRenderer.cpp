#include "gl/VideoRenderer.hpp"
#include "gl/Shader.hpp"
#include "gl/NISCoefficients.hpp"
#include "gl/Font8x8.hpp"
#include "gl/FSRCNNRenderer.hpp"
#include "gl/RAVURenderer.hpp"
#include "utils/Log.hpp"
#include <ios>
#include <vector>
#include <string>
#include <cstring>

namespace {

const char *vertex_src =
    "#version 330 core\n"
    "layout(location = 0) in vec2 aPos;\n"
    "layout(location = 1) in vec2 aTexCoord;\n"
    "out vec2 vTexCoord;\n"
    "void main() {\n"
    "    gl_Position = vec4(aPos, 0.0, 1.0);\n"
    "    vTexCoord = aTexCoord;\n"
    "}\n";

// Decode pass: YUV420P -> RGB. uYuvMode: 0 = BT.601 limited range
// (default), 1 = BT.601 full range (JPEG), 2 = BT.709 limited range. uFlipV
// resolves bottom-up (negative linesize) frames here, once, so every later
// pass can assume a normal top-down image.
const char *fragment_src_decode_yuv =
    "#version 330 core\n"
    "in vec2 vTexCoord;\n"
    "out vec4 FragColor;\n"
    "uniform sampler2D uTexY;\n"
    "uniform sampler2D uTexU;\n"
    "uniform sampler2D uTexV;\n"
    "uniform int uYuvMode;\n"
    "uniform bool uFlipV;\n"
    "void main() {\n"
    "    vec2 uv = uFlipV ? vec2(vTexCoord.x, 1.0 - vTexCoord.y) : vTexCoord;\n"
    "    float y = texture(uTexY, uv).r;\n"
    "    float u = texture(uTexU, uv).r;\n"
    "    float v = texture(uTexV, uv).r;\n"
    "    vec3 rgb;\n"
    "    if (uYuvMode == 1) {\n"
    "        float d = u - 0.50196078;\n"
    "        float e = v - 0.50196078;\n"
    "        rgb = vec3(y + e * 1.402,\n"
    "                   y - d * 0.344136 - e * 0.714136,\n"
    "                   y + d * 1.772);\n"
    "    } else {\n"
    "        float c = (y - 0.06274510) * 1.164;\n"
    "        float d = u - 0.50196078;\n"
    "        float e = v - 0.50196078;\n"
    "        if (uYuvMode == 2) {\n"
    "            rgb = vec3(c + e * 1.793, c - d * 0.213 - e * 0.533, c + d * 2.112);\n"
    "        } else {\n"
    "            rgb = vec3(c + e * 1.596, c - d * 0.392 - e * 0.813, c + d * 2.017);\n"
    "        }\n"
    "    }\n"
    "    FragColor = vec4(clamp(rgb, 0.0, 1.0), 1.0);\n"
    "}\n";

const char *fragment_src_decode_rgba =
    "#version 330 core\n"
    "in vec2 vTexCoord;\n"
    "out vec4 FragColor;\n"
    "uniform sampler2D uTex;\n"
    "uniform bool uFlipV;\n"
    "void main() {\n"
    "    vec2 uv = uFlipV ? vec2(vTexCoord.x, 1.0 - vTexCoord.y) : vTexCoord;\n"
    "    FragColor = texture(uTex, uv);\n"
    "}\n";

// Faithful port of AMD's ffx_fsr1.h FsrEasuF (non-packed 32-bit path),
// MIT licensed: https://github.com/GPUOpen-Effects/FidelityFX-FSR/blob/master/ffx-fsr/ffx_fsr1.h
// The only real change from AMD's source is mechanical: their version reads
// taps via Gather4 callbacks split across R/G/B channels (an optimization
// for their hardware gather trick); this reads each of the 12 taps with a
// plain texture() fetch instead. The direction/length estimation
// (FsrEasuSetF), the tap weighting (FsrEasuTapF, including its negative
// lobe - this is what gives it real sharpening character, unlike a plain
// clamped Lanczos), and the min4/max4 dering clamp are all unchanged.
const char *fragment_src_easu =
    "#version 330 core\n"
    "in vec2 vTexCoord;\n"
    "out vec4 FragColor;\n"
    "uniform sampler2D uTex;\n"
    "uniform vec2 uSrcSize;\n"
    "uniform vec2 uSrcTexelSize;\n"
    "\n"
    "float luma2(vec3 c) { return c.b * 0.5 + (c.r * 0.5 + c.g); }\n"
    "\n"
    "vec3 fetchTap(vec2 base, vec2 offset) {\n"
    "    return texture(uTex, (base + offset + 0.5) * uSrcTexelSize).rgb;\n"
    "}\n"
    "\n"
    "void easuSet(inout vec2 dir, inout float len, vec2 pp, bool biS, bool biT, bool biU, bool biV,\n"
    "             float lA, float lB, float lC, float lD, float lE) {\n"
    "    float w = 0.0;\n"
    "    if (biS) w = (1.0 - pp.x) * (1.0 - pp.y);\n"
    "    if (biT) w = pp.x * (1.0 - pp.y);\n"
    "    if (biU) w = (1.0 - pp.x) * pp.y;\n"
    "    if (biV) w = pp.x * pp.y;\n"
    "    float dc = lD - lC;\n"
    "    float cb = lC - lB;\n"
    "    float lenX = 1.0 / max(abs(dc), abs(cb));\n"
    "    float dirX = lD - lB;\n"
    "    dir.x += dirX * w;\n"
    "    lenX = clamp(abs(dirX) * lenX, 0.0, 1.0);\n"
    "    lenX *= lenX;\n"
    "    len += lenX * w;\n"
    "    float ec = lE - lC;\n"
    "    float ca = lC - lA;\n"
    "    float lenY = 1.0 / max(abs(ec), abs(ca));\n"
    "    float dirY = lE - lA;\n"
    "    dir.y += dirY * w;\n"
    "    lenY = clamp(abs(dirY) * lenY, 0.0, 1.0);\n"
    "    lenY *= lenY;\n"
    "    len += lenY * w;\n"
    "}\n"
    "\n"
    "void easuTap(inout vec3 aC, inout float aW, vec2 offset, vec2 dir, vec2 len, float lob, float clp, vec3 c) {\n"
    "    vec2 v;\n"
    "    v.x = offset.x * dir.x + offset.y * dir.y;\n"
    "    v.y = offset.x * -dir.y + offset.y * dir.x;\n"
    "    v *= len;\n"
    "    float d2 = min(v.x * v.x + v.y * v.y, clp);\n"
    "    float wB = (2.0 / 5.0) * d2 - 1.0;\n"
    "    float wA = lob * d2 - 1.0;\n"
    "    wB *= wB;\n"
    "    wA *= wA;\n"
    "    wB = (25.0 / 16.0) * wB - (25.0 / 16.0 - 1.0);\n"
    "    float w = wB * wA;\n"
    "    aC += c * w;\n"
    "    aW += w;\n"
    "}\n"
    "\n"
    "void main() {\n"
    "    vec2 pp = vTexCoord * uSrcSize - 0.5;\n"
    "    vec2 base = floor(pp);\n"
    "    pp -= base;\n"
    "\n"
    "    vec3 b = fetchTap(base, vec2( 0.0, -1.0));\n"
    "    vec3 c = fetchTap(base, vec2( 1.0, -1.0));\n"
    "    vec3 e = fetchTap(base, vec2(-1.0,  0.0));\n"
    "    vec3 f = fetchTap(base, vec2( 0.0,  0.0));\n"
    "    vec3 g = fetchTap(base, vec2( 1.0,  0.0));\n"
    "    vec3 h = fetchTap(base, vec2( 2.0,  0.0));\n"
    "    vec3 i = fetchTap(base, vec2(-1.0,  1.0));\n"
    "    vec3 j = fetchTap(base, vec2( 0.0,  1.0));\n"
    "    vec3 k = fetchTap(base, vec2( 1.0,  1.0));\n"
    "    vec3 l = fetchTap(base, vec2( 2.0,  1.0));\n"
    "    vec3 n = fetchTap(base, vec2( 0.0,  2.0));\n"
    "    vec3 o = fetchTap(base, vec2( 1.0,  2.0));\n"
    "\n"
    "    float bL = luma2(b), cL = luma2(c), eL = luma2(e), fL = luma2(f);\n"
    "    float gL = luma2(g), hL = luma2(h), iL = luma2(i), jL = luma2(j);\n"
    "    float kL = luma2(k), lL = luma2(l), nL = luma2(n), oL = luma2(o);\n"
    "\n"
    "    vec2 dir = vec2(0.0);\n"
    "    float len = 0.0;\n"
    "    easuSet(dir, len, pp, true,  false, false, false, bL, eL, fL, gL, jL);\n"
    "    easuSet(dir, len, pp, false, true,  false, false, cL, fL, gL, hL, kL);\n"
    "    easuSet(dir, len, pp, false, false, true,  false, fL, iL, jL, kL, nL);\n"
    "    easuSet(dir, len, pp, false, false, false, true,  gL, jL, kL, lL, oL);\n"
    "\n"
    "    vec2 dir2 = dir * dir;\n"
    "    float dirR = dir2.x + dir2.y;\n"
    "    bool zro = dirR < (1.0 / 32768.0);\n"
    "    dirR = zro ? 1.0 : inversesqrt(dirR);\n"
    "    dir.x = zro ? 1.0 : dir.x;\n"
    "    dir *= dirR;\n"
    "\n"
    "    len = len * 0.5;\n"
    "    len *= len;\n"
    "\n"
    "    float stretch = (dir.x * dir.x + dir.y * dir.y) / max(abs(dir.x), abs(dir.y));\n"
    "    vec2 len2 = vec2(1.0 + (stretch - 1.0) * len, 1.0 - 0.5 * len);\n"
    "\n"
    "    float lob = 0.5 + (0.25 - 0.04 - 0.5) * len;\n"
    "    float clp = 1.0 / lob;\n"
    "\n"
    "    vec3 min4 = min(min(f, g), min(j, k));\n"
    "    vec3 max4 = max(max(f, g), max(j, k));\n"
    "\n"
    "    vec3 aC = vec3(0.0);\n"
    "    float aW = 0.0;\n"
    "    easuTap(aC, aW, vec2( 0.0, -1.0) - pp, dir, len2, lob, clp, b);\n"
    "    easuTap(aC, aW, vec2( 1.0, -1.0) - pp, dir, len2, lob, clp, c);\n"
    "    easuTap(aC, aW, vec2(-1.0,  1.0) - pp, dir, len2, lob, clp, i);\n"
    "    easuTap(aC, aW, vec2( 0.0,  1.0) - pp, dir, len2, lob, clp, j);\n"
    "    easuTap(aC, aW, vec2( 0.0,  0.0) - pp, dir, len2, lob, clp, f);\n"
    "    easuTap(aC, aW, vec2(-1.0,  0.0) - pp, dir, len2, lob, clp, e);\n"
    "    easuTap(aC, aW, vec2( 1.0,  1.0) - pp, dir, len2, lob, clp, k);\n"
    "    easuTap(aC, aW, vec2( 2.0,  1.0) - pp, dir, len2, lob, clp, l);\n"
    "    easuTap(aC, aW, vec2( 2.0,  0.0) - pp, dir, len2, lob, clp, h);\n"
    "    easuTap(aC, aW, vec2( 1.0,  0.0) - pp, dir, len2, lob, clp, g);\n"
    "    easuTap(aC, aW, vec2( 1.0,  2.0) - pp, dir, len2, lob, clp, o);\n"
    "    easuTap(aC, aW, vec2( 0.0,  2.0) - pp, dir, len2, lob, clp, n);\n"
    "\n"
    "    vec3 result = min(max4, max(min4, aC / max(aW, 1e-6)));\n"
    "    FragColor = vec4(result, 1.0);\n"
    "}\n";

// Faithful port of AMD's ffx_fsr1.h FsrRcasF (non-packed 32-bit path), WITH
// FSR_RCAS_DENOISE enabled (always on, not behind a define here): it derives
// a per-pixel noise estimate from the same 5-tap neighborhood and multiplies
// the sharpen lobe by it, so compression/grain noise in near-flat regions
// doesn't get amplified into visible dithering the way plain sharpening
// would - real edges (where the 5 taps disagree in a structured, not
// noise-like, way) still get sharpened normally. uSharpness uses AMD's own
// convention from FsrRcasCon: "stops" where 0.0 = maximum sharpness and each
// +1.0 halves it (con.x = exp2(-sharpness), applied here directly).
const char *fragment_src_rcas =
    "#version 330 core\n"
    "in vec2 vTexCoord;\n"
    "out vec4 FragColor;\n"
    "uniform sampler2D uTex;\n"
    "uniform vec2 uTexelSize;\n"
    "uniform float uSharpness;\n"
    "float luma2(vec3 c) { return c.b * 0.5 + (c.r * 0.5 + c.g); }\n"
    "void main() {\n"
    "    vec3 b = texture(uTex, vTexCoord + vec2(0.0, -1.0) * uTexelSize).rgb;\n"
    "    vec3 d = texture(uTex, vTexCoord + vec2(-1.0, 0.0) * uTexelSize).rgb;\n"
    "    vec3 e = texture(uTex, vTexCoord).rgb;\n"
    "    vec3 f = texture(uTex, vTexCoord + vec2(1.0, 0.0) * uTexelSize).rgb;\n"
    "    vec3 h = texture(uTex, vTexCoord + vec2(0.0, 1.0) * uTexelSize).rgb;\n"
    "\n"
    "    vec3 mn4 = min(min(b, d), min(f, h));\n"
    "    vec3 mx4 = max(max(b, d), max(f, h));\n"
    "\n"
    "    float bL = luma2(b), dL = luma2(d), eL = luma2(e), fL = luma2(f), hL = luma2(h);\n"
    "    float nz = 0.25 * bL + 0.25 * dL + 0.25 * fL + 0.25 * hL - eL;\n"
    "    float nzRange = max(max(max(bL, dL), max(eL, fL)), hL) - min(min(min(bL, dL), min(eL, fL)), hL);\n"
    "    nz = clamp(abs(nz) / max(nzRange, 1e-6), 0.0, 1.0);\n"
    "    nz = -0.5 * nz + 1.0;\n"
    "\n"
    "    float peakCx = 1.0;\n"
    "    float peakCy = -4.0;\n"
    "    vec3 hitMin = min(mn4, e) / (4.0 * mx4 + 1e-6);\n"
    "    vec3 hitMax = (peakCx - max(mx4, e)) / (4.0 * mn4 + peakCy - 1e-6);\n"
    "    vec3 lobeRGB = max(-hitMin, hitMax);\n"
    "    float lobe = max(-0.1875, min(max(lobeRGB.r, max(lobeRGB.g, lobeRGB.b)), 0.0)) * exp2(-uSharpness) * nz;\n"
    "\n"
    "    float rcpL = 1.0 / (4.0 * lobe + 1.0);\n"
    "    vec3 result = (lobe * (b + d + f + h) + e) * rcpL;\n"
    "\n"
    "    FragColor = vec4(clamp(result, 0.0, 1.0), 1.0);\n"
    "}\n";

// Faithful port of NVIDIA Image Scaling's NVScaler (NIS_Scaler.h, MIT
// licensed: https://github.com/NVIDIAGameWorks/NVIDIAImageScaling). Their
// reference is written as a compute shader that caches a luma tile and a
// derived edge map in shared memory across a whole thread-group, since
// that's a real perf win at 4K/60. We don't have compute shaders (GL 3.3
// core, for macOS compatibility - same reasoning as EASU), and at video
// resolutions the redundant work is cheap anyway, so every fragment just
// independently re-fetches and re-derives its own local edge map / 6x6 luma
// window straight from the source texture. The filter math itself (edge
// detection, the coefficient-LUT-driven 6-tap normal + 4 directional
// filters, the LTI ringing guard, the luma-only USM sharpen) is unchanged.
// GLSL 330 has no 2D arrays, so the reference's p[6][6] window is a flat
// float[36] indexed as p[i*6+j] throughout.
const char *fragment_src_nis =
    "#version 330 core\n"
    "in vec2 vTexCoord;\n"
    "out vec4 FragColor;\n"
    "uniform sampler2D uTex;\n"
    "uniform sampler2D uCoefScaler;\n"
    "uniform sampler2D uCoefUSM;\n"
    "uniform vec2 uSrcSize;\n"
    "uniform vec2 uSrcTexelSize;\n"
    "uniform float uDetectRatio;\n"
    "uniform float uDetectThres;\n"
    "uniform float uMinContrastRatio;\n"
    "uniform float uRatioNorm;\n"
    "uniform float uSharpStartY;\n"
    "uniform float uSharpScaleY;\n"
    "uniform float uSharpStrengthMin;\n"
    "uniform float uSharpStrengthScale;\n"
    "uniform float uSharpLimitMin;\n"
    "uniform float uSharpLimitScale;\n"
    "const float kContrastBoost = 1.0;\n"
    "const float kEps = 1.0 / 255.0;\n"
    "const int kPhaseCount = 64;\n"
    "\n"
    "float getY(vec3 rgb) { return 0.2126 * rgb.r + 0.7152 * rgb.g + 0.0722 * rgb.b; }\n"
    "\n"
    "float lumaAt(vec2 texel) {\n"
    "    return getY(texture(uTex, (texel + 0.5) * uSrcTexelSize).rgb);\n"
    "}\n"
    "\n"
    // Edge weights (0/90/45/135 deg) centered at integer texel `c`, from its
    // own 3x3 luma neighborhood - this is GetEdgeMap() evaluated at a single
    // fixed offset, since every call site in the original only ever needed
    // one specific 3x3 window per invocation once the tile-relative indices
    // are resolved back to world texel coordinates.
    "vec4 edgeMapAt(vec2 c) {\n"
    "    float p00 = lumaAt(c + vec2(-1.0, -1.0));\n"
    "    float p01 = lumaAt(c + vec2( 0.0, -1.0));\n"
    "    float p02 = lumaAt(c + vec2( 1.0, -1.0));\n"
    "    float p10 = lumaAt(c + vec2(-1.0,  0.0));\n"
    "    float p12 = lumaAt(c + vec2( 1.0,  0.0));\n"
    "    float p20 = lumaAt(c + vec2(-1.0,  1.0));\n"
    "    float p21 = lumaAt(c + vec2( 0.0,  1.0));\n"
    "    float p22 = lumaAt(c + vec2( 1.0,  1.0));\n"
    "\n"
    "    float g0 = abs(p00 + p01 + p02 - p20 - p21 - p22);\n"
    "    float g45 = abs(p10 + p00 + p01 - p21 - p22 - p12);\n"
    "    float g90 = abs(p00 + p10 + p20 - p02 - p12 - p22);\n"
    "    float g135 = abs(p10 + p20 + p21 - p01 - p02 - p12);\n"
    "\n"
    "    float g0_90_max = max(g0, g90);\n"
    "    float g0_90_min = min(g0, g90);\n"
    "    float g45_135_max = max(g45, g135);\n"
    "    float g45_135_min = min(g45, g135);\n"
    "\n"
    "    if (g0_90_max + g45_135_max == 0.0)\n"
    "        return vec4(0.0);\n"
    "\n"
    "    float e_0_90 = min(g0_90_max / (g0_90_max + g45_135_max), 1.0);\n"
    "    float e_45_135 = 1.0 - e_0_90;\n"
    "\n"
    "    bool c_0_90 = (g0_90_max > g0_90_min * uDetectRatio) && (g0_90_max > uDetectThres) && (g0_90_max > g45_135_min);\n"
    "    bool c_45_135 = (g45_135_max > g45_135_min * uDetectRatio) && (g45_135_max > uDetectThres) && (g45_135_max > g0_90_min);\n"
    "    bool c_g_0_90 = (g0_90_max == g0);\n"
    "    bool c_g_45_135 = (g45_135_max == g45);\n"
    "\n"
    "    float f_e_0_90 = (c_0_90 && c_45_135) ? e_0_90 : 1.0;\n"
    "    float f_e_45_135 = (c_0_90 && c_45_135) ? e_45_135 : 1.0;\n"
    "\n"
    "    float w0 = (c_0_90 && c_g_0_90) ? f_e_0_90 : 0.0;\n"
    "    float w90 = (c_0_90 && !c_g_0_90) ? f_e_0_90 : 0.0;\n"
    "    float w45 = (c_45_135 && c_g_45_135) ? f_e_45_135 : 0.0;\n"
    "    float w135 = (c_45_135 && !c_g_45_135) ? f_e_45_135 : 0.0;\n"
    "\n"
    "    return vec4(w0, w90, w45, w135);\n"
    "}\n"
    "\n"
    "float calcLTI(float p0, float p1, float p2, float p3, float p4, float p5, int phaseIndex) {\n"
    "    bool selector = (phaseIndex <= kPhaseCount / 2);\n"
    "    float sel = selector ? p0 : p3;\n"
    "    float aMin = min(min(p1, p2), sel);\n"
    "    float aMax = max(max(p1, p2), sel);\n"
    "    sel = selector ? p2 : p5;\n"
    "    float bMin = min(min(p3, p4), sel);\n"
    "    float bMax = max(max(p3, p4), sel);\n"
    "\n"
    "    float aCont = aMax - aMin;\n"
    "    float bCont = bMax - bMin;\n"
    "\n"
    "    float contRatio = max(aCont, bCont) / (min(aCont, bCont) + kEps);\n"
    "    return (1.0 - clamp((contRatio - uMinContrastRatio) * uRatioNorm, 0.0, 1.0)) * kContrastBoost;\n"
    "}\n"
    "\n"
    "float evalPoly6(float pxl[6], int phaseInt) {\n"
    "    vec4 lo = texelFetch(uCoefScaler, ivec2(0, phaseInt), 0);\n"
    "    vec4 hi = texelFetch(uCoefScaler, ivec2(1, phaseInt), 0);\n"
    "    float y = pxl[0]*lo.x + pxl[1]*lo.y + pxl[2]*lo.z + pxl[3]*lo.w + pxl[4]*hi.x + pxl[5]*hi.y;\n"
    "\n"
    "    vec4 uLo = texelFetch(uCoefUSM, ivec2(0, phaseInt), 0);\n"
    "    vec4 uHi = texelFetch(uCoefUSM, ivec2(1, phaseInt), 0);\n"
    "    float yUsm = pxl[0]*uLo.x + pxl[1]*uLo.y + pxl[2]*uLo.z + pxl[3]*uLo.w + pxl[4]*uHi.x + pxl[5]*uHi.y;\n"
    "\n"
    "    float yScale = 1.0 - clamp((y - uSharpStartY) * uSharpScaleY, 0.0, 1.0);\n"
    "    float ySharpness = yScale * uSharpStrengthScale + uSharpStrengthMin;\n"
    "    yUsm *= ySharpness;\n"
    "\n"
    "    float ySharpnessLimit = (yScale * uSharpLimitScale + uSharpLimitMin) * y;\n"
    "    yUsm = min(ySharpnessLimit, max(-ySharpnessLimit, yUsm));\n"
    "    yUsm *= calcLTI(pxl[0], pxl[1], pxl[2], pxl[3], pxl[4], pxl[5], phaseInt);\n"
    "\n"
    "    return y + yUsm;\n"
    "}\n"
    "\n"
    "float filterNormal(float p[36], int phaseXInt, int phaseYInt) {\n"
    "    vec4 loY = texelFetch(uCoefScaler, ivec2(0, phaseYInt), 0);\n"
    "    vec4 hiY = texelFetch(uCoefScaler, ivec2(1, phaseYInt), 0);\n"
    "    float cY[6] = float[6](loY.x, loY.y, loY.z, loY.w, hiY.x, hiY.y);\n"
    "\n"
    "    vec4 loX = texelFetch(uCoefScaler, ivec2(0, phaseXInt), 0);\n"
    "    vec4 hiX = texelFetch(uCoefScaler, ivec2(1, phaseXInt), 0);\n"
    "    float cX[6] = float[6](loX.x, loX.y, loX.z, loX.w, hiX.x, hiX.y);\n"
    "\n"
    "    float hAcc = 0.0;\n"
    "    for (int j = 0; j < 6; j++) {\n"
    "        float vAcc = 0.0;\n"
    "        for (int i = 0; i < 6; i++)\n"
    "            vAcc += p[i*6+j] * cY[i];\n"
    "        hAcc += vAcc * cX[j];\n"
    "    }\n"
    "    return hAcc;\n"
    "}\n"
    "\n"
    "float addDirFilters(float p[36], float phaseXFrac, float phaseYFrac, int phaseXInt, int phaseYInt, vec4 w) {\n"
    "    float f = 0.0;\n"
    "    if (w.x > 0.0) {\n"
    "        float interp0Deg[6];\n"
    "        for (int i = 0; i < 6; i++)\n"
    "            interp0Deg[i] = mix(p[i*6+2], p[i*6+3], phaseXFrac);\n"
    "        f += evalPoly6(interp0Deg, phaseYInt) * w.x;\n"
    "    }\n"
    "    if (w.y > 0.0) {\n"
    "        float interp90Deg[6];\n"
    "        for (int i = 0; i < 6; i++)\n"
    "            interp90Deg[i] = mix(p[2*6+i], p[3*6+i], phaseYFrac);\n"
    "        f += evalPoly6(interp90Deg, phaseXInt) * w.y;\n"
    "    }\n"
    "    if (w.z > 0.0) {\n"
    "        float pphaseB45 = 0.5 + 0.5 * (phaseXFrac - phaseYFrac);\n"
    "        float tempInterp45Deg[7];\n"
    "        tempInterp45Deg[1] = mix(p[2*6+1], p[1*6+2], pphaseB45);\n"
    "        tempInterp45Deg[3] = mix(p[3*6+2], p[2*6+3], pphaseB45);\n"
    "        tempInterp45Deg[5] = mix(p[4*6+3], p[3*6+4], pphaseB45);\n"
    "        {\n"
    "            float pb = pphaseB45 - 0.5;\n"
    "            float a = (pb >= 0.0) ? p[0*6+2] : p[2*6+0];\n"
    "            float b = (pb >= 0.0) ? p[1*6+3] : p[3*6+1];\n"
    "            float c = (pb >= 0.0) ? p[2*6+4] : p[4*6+2];\n"
    "            float d = (pb >= 0.0) ? p[3*6+5] : p[5*6+3];\n"
    "            tempInterp45Deg[0] = mix(p[1*6+1], a, abs(pb));\n"
    "            tempInterp45Deg[2] = mix(p[2*6+2], b, abs(pb));\n"
    "            tempInterp45Deg[4] = mix(p[3*6+3], c, abs(pb));\n"
    "            tempInterp45Deg[6] = mix(p[4*6+4], d, abs(pb));\n"
    "        }\n"
    "        float interp45Deg[6];\n"
    "        float pphaseP45 = phaseXFrac + phaseYFrac;\n"
    "        if (pphaseP45 >= 1.0) {\n"
    "            for (int i = 0; i < 6; i++)\n"
    "                interp45Deg[i] = tempInterp45Deg[i+1];\n"
    "            pphaseP45 -= 1.0;\n"
    "        } else {\n"
    "            for (int i = 0; i < 6; i++)\n"
    "                interp45Deg[i] = tempInterp45Deg[i];\n"
    "        }\n"
    "        f += evalPoly6(interp45Deg, int(pphaseP45 * 64.0)) * w.z;\n"
    "    }\n"
    "    if (w.w > 0.0) {\n"
    "        float pphaseB135 = 0.5 * (phaseXFrac + phaseYFrac);\n"
    "        float tempInterp135Deg[7];\n"
    "        tempInterp135Deg[1] = mix(p[3*6+1], p[4*6+2], pphaseB135);\n"
    "        tempInterp135Deg[3] = mix(p[2*6+2], p[3*6+3], pphaseB135);\n"
    "        tempInterp135Deg[5] = mix(p[1*6+3], p[2*6+4], pphaseB135);\n"
    "        {\n"
    "            float pb = pphaseB135 - 0.5;\n"
    "            float a = (pb >= 0.0) ? p[5*6+2] : p[3*6+0];\n"
    "            float b = (pb >= 0.0) ? p[4*6+3] : p[2*6+1];\n"
    "            float c = (pb >= 0.0) ? p[3*6+4] : p[1*6+2];\n"
    "            float d = (pb >= 0.0) ? p[2*6+5] : p[0*6+3];\n"
    "            tempInterp135Deg[0] = mix(p[4*6+1], a, abs(pb));\n"
    "            tempInterp135Deg[2] = mix(p[3*6+2], b, abs(pb));\n"
    "            tempInterp135Deg[4] = mix(p[2*6+3], c, abs(pb));\n"
    "            tempInterp135Deg[6] = mix(p[1*6+4], d, abs(pb));\n"
    "        }\n"
    "        float interp135Deg[6];\n"
    "        float pphaseP135 = 1.0 + (phaseXFrac - phaseYFrac);\n"
    "        if (pphaseP135 >= 1.0) {\n"
    "            for (int i = 0; i < 6; i++)\n"
    "                interp135Deg[i] = tempInterp135Deg[i+1];\n"
    "            pphaseP135 -= 1.0;\n"
    "        } else {\n"
    "            for (int i = 0; i < 6; i++)\n"
    "                interp135Deg[i] = tempInterp135Deg[i];\n"
    "        }\n"
    "        f += evalPoly6(interp135Deg, int(pphaseP135 * 64.0)) * w.w;\n"
    "    }\n"
    "    return f;\n"
    "}\n"
    "\n"
    "void main() {\n"
    "    vec2 srcPos = vTexCoord * uSrcSize - 0.5;\n"
    "    vec2 ip = floor(srcPos);\n"
    "    vec2 frac = srcPos - ip;\n"
    "    ivec2 phaseInt = ivec2(frac * 64.0);\n"
    "\n"
    "    vec4 edge00 = edgeMapAt(ip);\n"
    "    vec4 edge01 = edgeMapAt(ip + vec2(1.0, 0.0));\n"
    "    vec4 edge10 = edgeMapAt(ip + vec2(0.0, 1.0));\n"
    "    vec4 edge11 = edgeMapAt(ip + vec2(1.0, 1.0));\n"
    "    vec4 h0 = mix(edge00, edge01, frac.x);\n"
    "    vec4 h1 = mix(edge10, edge11, frac.x);\n"
    "    vec4 w = mix(h0, h1, frac.y);\n"
    "\n"
    "    float p[36];\n"
    "    for (int i = 0; i < 6; i++)\n"
    "        for (int j = 0; j < 6; j++)\n"
    "            p[i*6+j] = lumaAt(ip + vec2(float(j - 2), float(i - 2)));\n"
    "\n"
    "    float baseWeight = 1.0 - w.x - w.y - w.z - w.w;\n"
    "    float opY = filterNormal(p, phaseInt.x, phaseInt.y) * baseWeight;\n"
    "    opY += addDirFilters(p, frac.x, frac.y, phaseInt.x, phaseInt.y, w);\n"
    "\n"
    "    vec4 op = texture(uTex, vTexCoord);\n"
    "    float y = getY(op.rgb);\n"
    "    float corr = opY - y;\n"
    "    op.rgb += corr;\n"
    "\n"
    "    FragColor = vec4(clamp(op.rgb, 0.0, 1.0), 1.0);\n"
    "}\n";

// Overlay glyph shader: samples the font atlas's red channel as alpha over a
// flat color, for the "which upscaler is active" toast.
const char *fragment_src_text =
    "#version 330 core\n"
    "in vec2 vTexCoord;\n"
    "out vec4 FragColor;\n"
    "uniform sampler2D uAtlas;\n"
    "uniform vec3 uColor;\n"
    "uniform float uAlpha;\n"
    "void main() {\n"
    "    float a = texture(uAtlas, vTexCoord).r * uAlpha;\n"
    "    FragColor = vec4(uColor, a);\n"
    "}\n";

// Flat translucent box behind the overlay text, for legibility over
// arbitrary video content.
const char *fragment_src_flat =
    "#version 330 core\n"
    "out vec4 FragColor;\n"
    "uniform vec4 uColor;\n"
    "void main() {\n"
    "    FragColor = uColor;\n"
    "}\n";

GLuint g_program_decode_yuv = 0;
GLuint g_program_decode_rgba = 0;
GLuint g_program_easu = 0;
GLuint g_program_rcas = 0;
GLuint g_program_nis = 0;
GLuint g_program_text = 0;
GLuint g_program_flat = 0;

GLint g_loc_decode_yuv_tex_y = -1, g_loc_decode_yuv_tex_u = -1, g_loc_decode_yuv_tex_v = -1;
GLint g_loc_decode_yuv_mode = -1, g_loc_decode_yuv_flip = -1;
GLint g_loc_decode_rgba_tex = -1, g_loc_decode_rgba_flip = -1;
GLint g_loc_easu_tex = -1, g_loc_easu_src_size = -1, g_loc_easu_texel_size = -1;
GLint g_loc_rcas_tex = -1, g_loc_rcas_texel_size = -1, g_loc_rcas_sharpness = -1;
GLint g_loc_nis_tex = -1, g_loc_nis_coef_scaler = -1, g_loc_nis_coef_usm = -1;
GLint g_loc_nis_src_size = -1, g_loc_nis_texel_size = -1;
GLint g_loc_nis_detect_ratio = -1, g_loc_nis_detect_thres = -1, g_loc_nis_min_contrast_ratio = -1, g_loc_nis_ratio_norm = -1;
GLint g_loc_nis_sharp_start_y = -1, g_loc_nis_sharp_scale_y = -1;
GLint g_loc_nis_sharp_strength_min = -1, g_loc_nis_sharp_strength_scale = -1;
GLint g_loc_nis_sharp_limit_min = -1, g_loc_nis_sharp_limit_scale = -1;
GLint g_loc_text_atlas = -1, g_loc_text_color = -1, g_loc_text_alpha = -1;
GLint g_loc_flat_color = -1;

GLuint g_vao_static = 0, g_vbo_static = 0;   // fixed fullscreen quad: decode + EASU passes
GLuint g_vao_composite = 0, g_vbo_composite = 0; // positioned into `rect`: final RCAS pass
GLuint g_vao_text = 0, g_vbo_text = 0;       // dynamic glyph quads for the overlay toast
GLuint g_vao_box = 0, g_vbo_box = 0;         // background box behind the overlay text
GLuint g_vao_help_text = 0, g_vbo_help_text = 0; // glyph quads for the help panel
GLuint g_vao_help_box = 0, g_vbo_help_box = 0;   // background box behind the help panel

GLuint g_tex_y = 0, g_tex_u = 0, g_tex_v = 0;
int g_y_w = 0, g_y_h = 0;
int g_uv_w = 0, g_uv_h = 0;

GLuint g_tex_rgba = 0;
int g_rgba_w = 0, g_rgba_h = 0;

bool g_using_yuv = true;
int g_yuv_mode = 0;

GLuint g_fbo_decode = 0, g_tex_decode = 0;
int g_decode_w = 0, g_decode_h = 0;

GLuint g_fbo_upscale = 0, g_tex_upscale = 0;
int g_upscale_w = 0, g_upscale_h = 0;

GLuint g_fbo_downscale = 0, g_tex_downscale = 0;
int g_downscale_w = 0, g_downscale_h = 0;

GLuint g_tex_nis_coef_scale = 0, g_tex_nis_coef_usm = 0;

GLuint g_tex_font_atlas = 0;
const int kFontCols = 16, kFontRows = 8; // 128 glyphs, 16x8 grid, 8x8 px each
const int kGlyphScale = 3;               // on-screen glyph size = 8*3 = 24px
const int kGlyphCell = 8 * kGlyphScale;
const int kOverlayMarginPx = 16;
const int kOverlayPaddingPx = 8;
const int kMaxOverlayChars = 64;
const double kOverlayHoldSeconds = 1.0;
const double kOverlayFadeSeconds = 0.4;

std::string g_overlay_text;
int g_overlay_char_count = 0;
int g_overlay_box_w = 0, g_overlay_box_h = 0;
int64_t g_overlay_start_us = 0;
bool g_overlay_active = false;

// Help panel: same font/box shaders as the toast, but persistent (toggled,
// not timed) and multi-line, so it needs its own geometry buffers rather
// than sharing the toast's - both can be on screen at once (e.g. pressing a
// shortcut while the help panel is open).
const char *kHelpLines[] = {
    "KEYBOARD SHORTCUTS   (H to close)",
    "",
    "Q             Quit",
    "Esc           Exit fullscreen",
    "F             Toggle fullscreen",
    "P / Space     Play / pause",
    "M             Mute",
    "S             Step one frame",
    "* / 0         Volume up",
    "/ / 9         Volume down",
    "A             Cycle audio track",
    "V             Cycle video track",
    "T             Cycle subtitle track",
    "C             Cycle all tracks",
    "U             Cycle upscaler",
    "R             Cycle scale preset",
    "Left / Right  Seek -10s / +10s",
    "Up / Down     Seek +60s / -60s",
    "PageUp/Down   Next / prev chapter",
};
const int kHelpLineCount = sizeof(kHelpLines) / sizeof(kHelpLines[0]);
const int kMaxHelpChars = 600; // real content is ~510 chars, some headroom

bool g_help_active = false;
int g_help_char_count = 0;
int g_help_box_w = 0, g_help_box_h = 0;

float g_sharpness = 0.2f; // AMD's "stops" convention: 0.0 = max sharpness
float g_nis_sharpness = 0.5f; // NIS's own [0,1] convention: 0.5 is its documented neutral default

enum class UpscalerMode { Bilinear, FSR1, NIS, FSRCNN, RAVU };
const int kUpscalerModeCount = 5;
const char *kUpscalerNames[] = { "Bilinear (off)", "FSR1 (EASU+RCAS)", "NIS", "FSRCNN", "RAVU-Lite" };
UpscalerMode g_upscaler_mode = UpscalerMode::FSR1;

// AMD's published FSR1 quality presets: the per-dimension ratio between the
// image EASU actually upscales from and the final display size. "Native"
// (1.0x, not an official FSR preset) feeds EASU the full decoded frame - the
// other four match AMD's real recommended ratios, deliberately throwing away
// resolution first so you can judge how much EASU can recover at each one,
// same as you'd compare them in a game's FSR quality setting.
struct RenderScalePreset { const char *name; float ratio; };
const RenderScalePreset kRenderScalePresets[] = {
    { "Native",        1.0f },
    { "Ultra Quality",  1.3f },
    { "Quality",        1.5f },
    { "Balanced",       1.7f },
    { "Performance",    2.0f },
};
const int kRenderScalePresetCount = sizeof(kRenderScalePresets) / sizeof(kRenderScalePresets[0]);
int g_scale_index = 0;

GLuint make_clamped_linear_texture()
{
    GLuint tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    return tex;
}

// Faithful port of NIS_Config.h's NVScalerUpdateConfig() tuning-constant
// derivation (SDR/None HDR mode branch only - we don't support HDR). Cheap
// enough to just recompute from the current sharpness slider every frame
// rather than caching.
struct NISTuning {
    float detectRatio, detectThres, minContrastRatio, ratioNorm;
    float sharpStartY, sharpScaleY;
    float sharpStrengthMin, sharpStrengthScale;
    float sharpLimitMin, sharpLimitScale;
};

NISTuning compute_nis_tuning(float sharpness)
{
    sharpness = sharpness < 0.0f ? 0.0f : (sharpness > 1.0f ? 1.0f : sharpness);
    float sharpen_slider = sharpness - 0.5f;

    float maxScale = (sharpen_slider >= 0.0f) ? 1.25f : 1.75f;
    float minScale = (sharpen_slider >= 0.0f) ? 1.25f : 1.0f;
    float limitScale = (sharpen_slider >= 0.0f) ? 1.25f : 1.0f;

    NISTuning t;
    t.detectRatio = 2.0f * 1127.0f / 1024.0f;
    t.detectThres = 64.0f / 1024.0f;
    t.minContrastRatio = 2.0f;
    float maxContrastRatio = 10.0f;

    t.sharpStartY = 0.45f;
    float sharpEndY = 0.9f;
    t.sharpStrengthMin = (0.0f > 0.4f + sharpen_slider * minScale * 1.2f) ? 0.0f : 0.4f + sharpen_slider * minScale * 1.2f;
    float sharpStrengthMax = 1.6f + sharpen_slider * maxScale * 1.8f;
    t.sharpLimitMin = (0.1f > 0.14f + sharpen_slider * limitScale * 0.32f) ? 0.1f : 0.14f + sharpen_slider * limitScale * 0.32f;
    float sharpLimitMax = 0.5f + sharpen_slider * limitScale * 0.6f;

    t.ratioNorm = 1.0f / (maxContrastRatio - t.minContrastRatio);
    t.sharpScaleY = 1.0f / (sharpEndY - t.sharpStartY);
    t.sharpStrengthScale = sharpStrengthMax - t.sharpStrengthMin;
    t.sharpLimitScale = sharpLimitMax - t.sharpLimitMin;
    return t;
}

} // namespace

bool VideoRenderer::init()
{
    g_program_decode_yuv = Shader::compile_program(vertex_src, fragment_src_decode_yuv);
    g_program_decode_rgba = Shader::compile_program(vertex_src, fragment_src_decode_rgba);
    g_program_easu = Shader::compile_program(vertex_src, fragment_src_easu);
    g_program_rcas = Shader::compile_program(vertex_src, fragment_src_rcas);
    g_program_nis = Shader::compile_program(vertex_src, fragment_src_nis);
    g_program_text = Shader::compile_program(vertex_src, fragment_src_text);
    g_program_flat = Shader::compile_program(vertex_src, fragment_src_flat);
    if (!g_program_decode_yuv || !g_program_decode_rgba || !g_program_easu || !g_program_rcas || !g_program_nis
        || !g_program_text || !g_program_flat)
        return false;

    g_loc_decode_yuv_tex_y = gl::GetUniformLocation(g_program_decode_yuv, "uTexY");
    g_loc_decode_yuv_tex_u = gl::GetUniformLocation(g_program_decode_yuv, "uTexU");
    g_loc_decode_yuv_tex_v = gl::GetUniformLocation(g_program_decode_yuv, "uTexV");
    g_loc_decode_yuv_mode = gl::GetUniformLocation(g_program_decode_yuv, "uYuvMode");
    g_loc_decode_yuv_flip = gl::GetUniformLocation(g_program_decode_yuv, "uFlipV");
    g_loc_decode_rgba_tex = gl::GetUniformLocation(g_program_decode_rgba, "uTex");
    g_loc_decode_rgba_flip = gl::GetUniformLocation(g_program_decode_rgba, "uFlipV");
    g_loc_easu_tex = gl::GetUniformLocation(g_program_easu, "uTex");
    g_loc_easu_src_size = gl::GetUniformLocation(g_program_easu, "uSrcSize");
    g_loc_easu_texel_size = gl::GetUniformLocation(g_program_easu, "uSrcTexelSize");
    g_loc_rcas_tex = gl::GetUniformLocation(g_program_rcas, "uTex");
    g_loc_rcas_texel_size = gl::GetUniformLocation(g_program_rcas, "uTexelSize");
    g_loc_rcas_sharpness = gl::GetUniformLocation(g_program_rcas, "uSharpness");
    g_loc_nis_tex = gl::GetUniformLocation(g_program_nis, "uTex");
    g_loc_nis_coef_scaler = gl::GetUniformLocation(g_program_nis, "uCoefScaler");
    g_loc_nis_coef_usm = gl::GetUniformLocation(g_program_nis, "uCoefUSM");
    g_loc_nis_src_size = gl::GetUniformLocation(g_program_nis, "uSrcSize");
    g_loc_nis_texel_size = gl::GetUniformLocation(g_program_nis, "uSrcTexelSize");
    g_loc_nis_detect_ratio = gl::GetUniformLocation(g_program_nis, "uDetectRatio");
    g_loc_nis_detect_thres = gl::GetUniformLocation(g_program_nis, "uDetectThres");
    g_loc_nis_min_contrast_ratio = gl::GetUniformLocation(g_program_nis, "uMinContrastRatio");
    g_loc_nis_ratio_norm = gl::GetUniformLocation(g_program_nis, "uRatioNorm");
    g_loc_nis_sharp_start_y = gl::GetUniformLocation(g_program_nis, "uSharpStartY");
    g_loc_nis_sharp_scale_y = gl::GetUniformLocation(g_program_nis, "uSharpScaleY");
    g_loc_nis_sharp_strength_min = gl::GetUniformLocation(g_program_nis, "uSharpStrengthMin");
    g_loc_nis_sharp_strength_scale = gl::GetUniformLocation(g_program_nis, "uSharpStrengthScale");
    g_loc_nis_sharp_limit_min = gl::GetUniformLocation(g_program_nis, "uSharpLimitMin");
    g_loc_nis_sharp_limit_scale = gl::GetUniformLocation(g_program_nis, "uSharpLimitScale");
    g_loc_text_atlas = gl::GetUniformLocation(g_program_text, "uAtlas");
    g_loc_text_color = gl::GetUniformLocation(g_program_text, "uColor");
    g_loc_text_alpha = gl::GetUniformLocation(g_program_text, "uAlpha");
    g_loc_flat_color = gl::GetUniformLocation(g_program_flat, "uColor");

    gl::GenVertexArrays(1, &g_vao_static);
    gl::GenBuffers(1, &g_vbo_static);
    gl::BindVertexArray(g_vao_static);
    gl::BindBuffer(GL_ARRAY_BUFFER, g_vbo_static);
    // Texcoords are v-flipped relative to what you'd naively expect (top
    // vertex -> v=1) because this quad is used to render INTO an FBO
    // texture, not straight to the screen. NDC y=+1 rasterizes to the
    // framebuffer's last memory row, and a later texture(tex, v) call reads
    // v=0 from the FIRST memory row - so a plain top->v=0 quad comes out
    // upside down every time its output texture gets sampled again. Flipping
    // it here cancels that inversion so both the decode and EASU passes each
    // produce a right-side-up texture on their own, instead of only
    // "working" when both happen to run back to back and cancel out.
    static const float static_verts[16] = {
        -1.0f,  1.0f, 0.0f, 1.0f,
         1.0f,  1.0f, 1.0f, 1.0f,
        -1.0f, -1.0f, 0.0f, 0.0f,
         1.0f, -1.0f, 1.0f, 0.0f,
    };
    gl::BufferData(GL_ARRAY_BUFFER, sizeof(static_verts), static_verts, GL_STATIC_DRAW);
    gl::VertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void *)0);
    gl::EnableVertexAttribArray(0);
    gl::VertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void *)(2 * sizeof(float)));
    gl::EnableVertexAttribArray(1);

    gl::GenVertexArrays(1, &g_vao_composite);
    gl::GenBuffers(1, &g_vbo_composite);
    gl::BindVertexArray(g_vao_composite);
    gl::BindBuffer(GL_ARRAY_BUFFER, g_vbo_composite);
    gl::BufferData(GL_ARRAY_BUFFER, sizeof(float) * 16, NULL, GL_DYNAMIC_DRAW);
    gl::VertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void *)0);
    gl::EnableVertexAttribArray(0);
    gl::VertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void *)(2 * sizeof(float)));
    gl::EnableVertexAttribArray(1);
    gl::BindVertexArray(0);

    gl::GenVertexArrays(1, &g_vao_box);
    gl::GenBuffers(1, &g_vbo_box);
    gl::BindVertexArray(g_vao_box);
    gl::BindBuffer(GL_ARRAY_BUFFER, g_vbo_box);
    gl::BufferData(GL_ARRAY_BUFFER, sizeof(float) * 16, NULL, GL_DYNAMIC_DRAW);
    gl::VertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void *)0);
    gl::EnableVertexAttribArray(0);
    gl::VertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void *)(2 * sizeof(float)));
    gl::EnableVertexAttribArray(1);
    gl::BindVertexArray(0);

    // Text glyph quads: 6 verts (2 triangles, no index buffer) x 4 floats
    // (pos.xy, uv.xy) per character, capacity for kMaxOverlayChars, rebuilt
    // whenever show_overlay() is called (not every frame - only the fade
    // alpha changes per frame, and that's a uniform, not per-vertex data).
    gl::GenVertexArrays(1, &g_vao_text);
    gl::GenBuffers(1, &g_vbo_text);
    gl::BindVertexArray(g_vao_text);
    gl::BindBuffer(GL_ARRAY_BUFFER, g_vbo_text);
    gl::BufferData(GL_ARRAY_BUFFER, sizeof(float) * 4 * 6 * kMaxOverlayChars, NULL, GL_DYNAMIC_DRAW);
    gl::VertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void *)0);
    gl::EnableVertexAttribArray(0);
    gl::VertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void *)(2 * sizeof(float)));
    gl::EnableVertexAttribArray(1);
    gl::BindVertexArray(0);

    // Help panel's own box + glyph buffers (see kHelpLines) - separate from
    // the toast's so both can be visible at once without clobbering each
    // other.
    gl::GenVertexArrays(1, &g_vao_help_box);
    gl::GenBuffers(1, &g_vbo_help_box);
    gl::BindVertexArray(g_vao_help_box);
    gl::BindBuffer(GL_ARRAY_BUFFER, g_vbo_help_box);
    gl::BufferData(GL_ARRAY_BUFFER, sizeof(float) * 16, NULL, GL_DYNAMIC_DRAW);
    gl::VertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void *)0);
    gl::EnableVertexAttribArray(0);
    gl::VertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void *)(2 * sizeof(float)));
    gl::EnableVertexAttribArray(1);
    gl::BindVertexArray(0);

    gl::GenVertexArrays(1, &g_vao_help_text);
    gl::GenBuffers(1, &g_vbo_help_text);
    gl::BindVertexArray(g_vao_help_text);
    gl::BindBuffer(GL_ARRAY_BUFFER, g_vbo_help_text);
    gl::BufferData(GL_ARRAY_BUFFER, sizeof(float) * 4 * 6 * kMaxHelpChars, NULL, GL_DYNAMIC_DRAW);
    gl::VertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void *)0);
    gl::EnableVertexAttribArray(0);
    gl::VertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void *)(2 * sizeof(float)));
    gl::EnableVertexAttribArray(1);
    gl::BindVertexArray(0);

    gl::GenFramebuffers(1, &g_fbo_decode);
    gl::GenFramebuffers(1, &g_fbo_upscale);
    gl::GenFramebuffers(1, &g_fbo_downscale);

    // NIS coefficient LUTs: 2 texels wide x 64 phases tall, RGBA32F, sampled
    // with texelFetch (exact, filtering mode doesn't matter) - same texture
    // layout NVIDIA's own reference uses, so the shader's texel-packing math
    // (c0..c3 in texel 0, c4,c5 in texel 1) matches directly.
    glGenTextures(1, &g_tex_nis_coef_scale);
    glBindTexture(GL_TEXTURE_2D, g_tex_nis_coef_scale);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, 2, 64, 0, GL_RGBA, GL_FLOAT, nis_coef_scale);

    glGenTextures(1, &g_tex_nis_coef_usm);
    glBindTexture(GL_TEXTURE_2D, g_tex_nis_coef_usm);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, 2, 64, 0, GL_RGBA, GL_FLOAT, nis_coef_usm);

    // Font atlas: unpack the bit-packed 8x8 glyphs into a single-channel
    // 128x64 texture (16x8 grid of glyphs), glyph index N at cell
    // (N % 16, N / 16). Row 0 of each glyph is its top row, bit 0 (LSB) of
    // each row byte is its leftmost pixel - matches font8x8_basic's layout
    // directly, no repacking needed beyond bit -> byte expansion.
    {
        const int atlasW = kFontCols * 8, atlasH = kFontRows * 8;
        std::vector<unsigned char> atlas(atlasW * atlasH, 0);
        for (int glyph = 0; glyph < 128; glyph++) {
            int cellX = (glyph % kFontCols) * 8;
            int cellY = (glyph / kFontCols) * 8;
            for (int row = 0; row < 8; row++) {
                unsigned char bits = font8x8_basic[glyph][row];
                for (int col = 0; col < 8; col++) {
                    if ((bits >> col) & 1)
                        atlas[(cellY + row) * atlasW + (cellX + col)] = 255;
                }
            }
        }
        glGenTextures(1, &g_tex_font_atlas);
        glBindTexture(GL_TEXTURE_2D, g_tex_font_atlas);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, atlasW, atlasH, 0, GL_RED, GL_UNSIGNED_BYTE, atlas.data());
    }

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);

    if (!FSRCNNRenderer::init())
        return false;
    if (!RAVURenderer::init())
        return false;

    return true;
}

void VideoRenderer::destroy()
{
    FSRCNNRenderer::destroy();
    RAVURenderer::destroy();

    if (g_tex_y) glDeleteTextures(1, &g_tex_y);
    if (g_tex_u) glDeleteTextures(1, &g_tex_u);
    if (g_tex_v) glDeleteTextures(1, &g_tex_v);
    if (g_tex_rgba) glDeleteTextures(1, &g_tex_rgba);
    if (g_tex_decode) glDeleteTextures(1, &g_tex_decode);
    if (g_tex_upscale) glDeleteTextures(1, &g_tex_upscale);
    if (g_tex_downscale) glDeleteTextures(1, &g_tex_downscale);
    if (g_tex_nis_coef_scale) glDeleteTextures(1, &g_tex_nis_coef_scale);
    if (g_tex_nis_coef_usm) glDeleteTextures(1, &g_tex_nis_coef_usm);
    if (g_tex_font_atlas) glDeleteTextures(1, &g_tex_font_atlas);
    if (g_fbo_decode) gl::DeleteFramebuffers(1, &g_fbo_decode);
    if (g_fbo_upscale) gl::DeleteFramebuffers(1, &g_fbo_upscale);
    if (g_fbo_downscale) gl::DeleteFramebuffers(1, &g_fbo_downscale);
    if (g_vbo_static) gl::DeleteBuffers(1, &g_vbo_static);
    if (g_vao_static) gl::DeleteVertexArrays(1, &g_vao_static);
    if (g_vbo_composite) gl::DeleteBuffers(1, &g_vbo_composite);
    if (g_vao_composite) gl::DeleteVertexArrays(1, &g_vao_composite);
    if (g_vbo_text) gl::DeleteBuffers(1, &g_vbo_text);
    if (g_vao_text) gl::DeleteVertexArrays(1, &g_vao_text);
    if (g_vbo_box) gl::DeleteBuffers(1, &g_vbo_box);
    if (g_vao_box) gl::DeleteVertexArrays(1, &g_vao_box);
    if (g_vbo_help_text) gl::DeleteBuffers(1, &g_vbo_help_text);
    if (g_vao_help_text) gl::DeleteVertexArrays(1, &g_vao_help_text);
    if (g_vbo_help_box) gl::DeleteBuffers(1, &g_vbo_help_box);
    if (g_vao_help_box) gl::DeleteVertexArrays(1, &g_vao_help_box);
    if (g_program_decode_yuv) gl::DeleteProgram(g_program_decode_yuv);
    if (g_program_decode_rgba) gl::DeleteProgram(g_program_decode_rgba);
    if (g_program_easu) gl::DeleteProgram(g_program_easu);
    if (g_program_rcas) gl::DeleteProgram(g_program_rcas);
    if (g_program_nis) gl::DeleteProgram(g_program_nis);
    if (g_program_text) gl::DeleteProgram(g_program_text);
    if (g_program_flat) gl::DeleteProgram(g_program_flat);

    g_tex_y = g_tex_u = g_tex_v = g_tex_rgba = 0;
    g_tex_decode = g_tex_upscale = g_tex_downscale = 0;
    g_tex_nis_coef_scale = g_tex_nis_coef_usm = g_tex_font_atlas = 0;
    g_fbo_decode = g_fbo_upscale = g_fbo_downscale = 0;
    g_vbo_static = g_vao_static = g_vbo_composite = g_vao_composite = 0;
    g_vbo_text = g_vao_text = g_vbo_box = g_vao_box = 0;
    g_vbo_help_text = g_vao_help_text = g_vbo_help_box = g_vao_help_box = 0;
    g_program_decode_yuv = g_program_decode_rgba = g_program_easu = g_program_rcas = g_program_nis = 0;
    g_program_text = g_program_flat = 0;
    g_y_w = g_y_h = g_uv_w = g_uv_h = g_rgba_w = g_rgba_h = 0;
    g_decode_w = g_decode_h = g_upscale_w = g_upscale_h = g_downscale_w = g_downscale_h = 0;
    g_help_active = false;
    g_overlay_active = false;
}

bool VideoRenderer::ensure_yuv_textures(int width, int height)
{
    int uv_w = AV_CEIL_RSHIFT(width, 1);
    int uv_h = AV_CEIL_RSHIFT(height, 1);
    if (g_tex_y && g_y_w == width && g_y_h == height && g_uv_w == uv_w && g_uv_h == uv_h)
        return true;

    if (!g_tex_y) g_tex_y = make_clamped_linear_texture();
    if (!g_tex_u) g_tex_u = make_clamped_linear_texture();
    if (!g_tex_v) g_tex_v = make_clamped_linear_texture();

    glBindTexture(GL_TEXTURE_2D, g_tex_y);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, width, height, 0, GL_RED, GL_UNSIGNED_BYTE, NULL);
    glBindTexture(GL_TEXTURE_2D, g_tex_u);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, uv_w, uv_h, 0, GL_RED, GL_UNSIGNED_BYTE, NULL);
    glBindTexture(GL_TEXTURE_2D, g_tex_v);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, uv_w, uv_h, 0, GL_RED, GL_UNSIGNED_BYTE, NULL);

    g_y_w = width;
    g_y_h = height;
    g_uv_w = uv_w;
    g_uv_h = uv_h;
    return true;
}

bool VideoRenderer::ensure_rgba_texture(int width, int height)
{
    if (g_tex_rgba && g_rgba_w == width && g_rgba_h == height)
        return true;

    if (!g_tex_rgba)
        g_tex_rgba = make_clamped_linear_texture();

    glBindTexture(GL_TEXTURE_2D, g_tex_rgba);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_BGRA, GL_UNSIGNED_BYTE, NULL);

    g_rgba_w = width;
    g_rgba_h = height;
    return true;
}

namespace {

bool ensure_render_target(GLuint fbo, GLuint &tex, int &cur_w, int &cur_h, int width, int height, const char *name)
{
    if (tex && cur_w == width && cur_h == height)
        return true;

    if (!tex)
        tex = make_clamped_linear_texture();

    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);

    gl::BindFramebuffer(GL_FRAMEBUFFER, fbo);
    gl::FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);
    GLenum status = gl::CheckFramebufferStatus(GL_FRAMEBUFFER);
    gl::BindFramebuffer(GL_FRAMEBUFFER, 0);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        Log::error() << name << " framebuffer is incomplete (status 0x"
                     << std::hex << status << std::dec << ").";
        return false;
    }

    cur_w = width;
    cur_h = height;
    return true;
}

} // namespace

bool VideoRenderer::ensure_decode_target(int width, int height)
{
    return ensure_render_target(g_fbo_decode, g_tex_decode, g_decode_w, g_decode_h, width, height, "decode");
}

bool VideoRenderer::ensure_upscale_target(int width, int height)
{
    return ensure_render_target(g_fbo_upscale, g_tex_upscale, g_upscale_w, g_upscale_h, width, height, "upscale");
}

bool VideoRenderer::ensure_downscale_target(int width, int height)
{
    return ensure_render_target(g_fbo_downscale, g_tex_downscale, g_downscale_w, g_downscale_h, width, height, "downscale");
}

bool VideoRenderer::upload_frame(AVFrame *frame, struct SwsContext **img_convert_ctx)
{
    if (frame->format == AV_PIX_FMT_YUV420P) {
        if (!ensure_yuv_textures(frame->width, frame->height))
            return false;

        uint8_t *y_data = frame->data[0];
        uint8_t *u_data = frame->data[1];
        uint8_t *v_data = frame->data[2];
        int y_stride = frame->linesize[0];
        int u_stride = frame->linesize[1];
        int v_stride = frame->linesize[2];

        // Bottom-up frames (negative linesize) are re-pointed to their last
        // row with a positive stride, so uploads always walk top-to-bottom -
        // same normalization ffplay's upload_texture() did for SDL.
        if (y_stride < 0 && u_stride < 0 && v_stride < 0) {
            y_data += y_stride * (frame->height - 1);
            u_data += u_stride * (g_uv_h - 1);
            v_data += v_stride * (g_uv_h - 1);
            y_stride = -y_stride;
            u_stride = -u_stride;
            v_stride = -v_stride;
        } else if (y_stride < 0 || u_stride < 0 || v_stride < 0) {
            Log::error() << "Mixed negative and positive linesizes are not supported.";
            return false;
        }

        auto upload_plane = [](GLuint tex, uint8_t *data, int stride, int w, int h) {
            glBindTexture(GL_TEXTURE_2D, tex);
            glPixelStorei(GL_UNPACK_ROW_LENGTH, stride);
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, w, h, GL_RED, GL_UNSIGNED_BYTE, data);
            glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
        };

        upload_plane(g_tex_y, y_data, y_stride, frame->width, frame->height);
        upload_plane(g_tex_u, u_data, u_stride, g_uv_w, g_uv_h);
        upload_plane(g_tex_v, v_data, v_stride, g_uv_w, g_uv_h);

        g_yuv_mode = 0;
        if (frame->color_range == AVCOL_RANGE_JPEG)
            g_yuv_mode = 1;
        else if (frame->colorspace == AVCOL_SPC_BT709)
            g_yuv_mode = 2;

        g_using_yuv = true;
        return true;
    }

    *img_convert_ctx = sws_getCachedContext(*img_convert_ctx,
        frame->width, frame->height, static_cast<AVPixelFormat>(frame->format),
        frame->width, frame->height, AV_PIX_FMT_BGRA,
        sws_flags, NULL, NULL, NULL);
    if (!*img_convert_ctx) {
        Log::error() << "Could not initialize conversion context!";
        return false;
    }

    if (!ensure_rgba_texture(frame->width, frame->height))
        return false;

    std::vector<uint8_t> buf((size_t)frame->width * frame->height * 4);
    uint8_t *dst_data[4] = { buf.data(), NULL, NULL, NULL };
    int dst_linesize[4] = { frame->width * 4, 0, 0, 0 };
    sws_scale(*img_convert_ctx, frame->data, frame->linesize, 0, frame->height, dst_data, dst_linesize);

    glBindTexture(GL_TEXTURE_2D, g_tex_rgba);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, frame->width, frame->height, GL_BGRA, GL_UNSIGNED_BYTE, buf.data());

    g_using_yuv = false;
    return true;
}

void VideoRenderer::set_composite_quad(const SDL_Rect &rect, int drawable_w, int drawable_h)
{
    float x0 = (float(rect.x) / drawable_w) * 2.0f - 1.0f;
    float x1 = (float(rect.x + rect.w) / drawable_w) * 2.0f - 1.0f;
    float y_top = 1.0f - (float(rect.y) / drawable_h) * 2.0f;
    float y_bot = 1.0f - (float(rect.y + rect.h) / drawable_h) * 2.0f;

    float verts[16] = {
        x0, y_top, 0.0f, 0.0f,
        x1, y_top, 1.0f, 0.0f,
        x0, y_bot, 0.0f, 1.0f,
        x1, y_bot, 1.0f, 1.0f,
    };

    gl::BindBuffer(GL_ARRAY_BUFFER, g_vbo_composite);
    gl::BufferSubData(GL_ARRAY_BUFFER, 0, sizeof(verts), verts);
}

void VideoRenderer::draw(const SDL_Rect &rect, int drawable_w, int drawable_h, bool flip_v)
{
    if (rect.w <= 0 || rect.h <= 0)
        return;

    int src_w = g_using_yuv ? g_y_w : g_rgba_w;
    int src_h = g_using_yuv ? g_y_h : g_rgba_h;
    if (src_w <= 0 || src_h <= 0)
        return;

    if (!ensure_decode_target(src_w, src_h))
        return;
    if (g_upscaler_mode != UpscalerMode::Bilinear && !ensure_upscale_target(rect.w, rect.h))
        return;

    // Pass A: decode (YUV/BGRA -> RGB) into a native-resolution FBO.
    gl::BindFramebuffer(GL_FRAMEBUFFER, g_fbo_decode);
    glViewport(0, 0, src_w, src_h);
    gl::BindVertexArray(g_vao_static);
    if (g_using_yuv) {
        gl::UseProgram(g_program_decode_yuv);
        gl::ActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, g_tex_y);
        gl::ActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, g_tex_u);
        gl::ActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, g_tex_v);
        gl::Uniform1i(g_loc_decode_yuv_tex_y, 0);
        gl::Uniform1i(g_loc_decode_yuv_tex_u, 1);
        gl::Uniform1i(g_loc_decode_yuv_tex_v, 2);
        gl::Uniform1i(g_loc_decode_yuv_mode, g_yuv_mode);
        gl::Uniform1i(g_loc_decode_yuv_flip, flip_v ? 1 : 0);
    } else {
        gl::UseProgram(g_program_decode_rgba);
        gl::ActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, g_tex_rgba);
        gl::Uniform1i(g_loc_decode_rgba_tex, 0);
        gl::Uniform1i(g_loc_decode_rgba_flip, flip_v ? 1 : 0);
    }
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    // Pass B: upscale into a destination-resolution FBO - skipped entirely
    // in Bilinear mode, so Pass C falls back to sampling the decoded frame
    // directly (plain GL_LINEAR stretch, same quality the old SDL_Renderer
    // path had) for a clean A/B comparison.
    GLuint composite_source = g_tex_decode;
    float composite_sharpness = 100.0f; // exp2(-100) ~= 0: lobe forced to 0, RCAS becomes an identity passthrough
    if (g_upscaler_mode != UpscalerMode::Bilinear) {
        GLuint upscale_input_tex = g_tex_decode;
        int upscale_src_w = src_w, upscale_src_h = src_h;

        // Optional quality-preset pre-pass: deliberately throw away
        // resolution (a plain GL_LINEAR downsize, reusing the RCAS program
        // with sharpness forced off as a passthrough blit) before upscaling,
        // so the upscaler has to reconstruct detail the same way it would
        // for a game rendered at less than native resolution. Native (ratio
        // 1.0) skips this and feeds the upscaler the full decoded frame.
        float scale_ratio = kRenderScalePresets[g_scale_index].ratio;
        if (scale_ratio > 1.0f) {
            int ds_w = (int)(src_w / scale_ratio + 0.5f);
            int ds_h = (int)(src_h / scale_ratio + 0.5f);
            if (ds_w < 1) ds_w = 1;
            if (ds_h < 1) ds_h = 1;
            if (!ensure_downscale_target(ds_w, ds_h))
                return;

            gl::BindFramebuffer(GL_FRAMEBUFFER, g_fbo_downscale);
            glViewport(0, 0, ds_w, ds_h);
            gl::UseProgram(g_program_rcas);
            gl::ActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, g_tex_decode);
            gl::Uniform1i(g_loc_rcas_tex, 0);
            gl::Uniform2f(g_loc_rcas_texel_size, 1.0f / src_w, 1.0f / src_h);
            gl::Uniform1f(g_loc_rcas_sharpness, 100.0f);
            glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

            upscale_input_tex = g_tex_downscale;
            upscale_src_w = ds_w;
            upscale_src_h = ds_h;
        }

        if (g_upscaler_mode == UpscalerMode::FSRCNN || g_upscaler_mode == UpscalerMode::RAVU) {
            // Both own their own FBOs/textures at their own fixed-2x-scale
            // resolution (not rect.w x rect.h) - the composite pass below
            // samples whatever they return via normalized UV regardless, the
            // same way it already fits FSR1/NIS's rect-sized output, so no
            // extra resize step is needed here.
            int out_w = 0, out_h = 0;
            GLuint out_tex = (g_upscaler_mode == UpscalerMode::FSRCNN)
                ? FSRCNNRenderer::run(upscale_input_tex, upscale_src_w, upscale_src_h, out_w, out_h)
                : RAVURenderer::run(upscale_input_tex, upscale_src_w, upscale_src_h, out_w, out_h);
            if (!out_tex)
                return;
            composite_source = out_tex;
            composite_sharpness = 100.0f; // both are trained SR nets/filters, not paired with a separate sharpen pass
        } else {
            gl::BindFramebuffer(GL_FRAMEBUFFER, g_fbo_upscale);
            glViewport(0, 0, rect.w, rect.h);
            gl::ActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, upscale_input_tex);

            if (g_upscaler_mode == UpscalerMode::FSR1) {
                gl::UseProgram(g_program_easu);
                gl::Uniform1i(g_loc_easu_tex, 0);
                gl::Uniform2f(g_loc_easu_src_size, (float)upscale_src_w, (float)upscale_src_h);
                gl::Uniform2f(g_loc_easu_texel_size, 1.0f / upscale_src_w, 1.0f / upscale_src_h);
                glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
                composite_source = g_tex_upscale;
                composite_sharpness = g_sharpness;
            } else { // NIS
                NISTuning nt = compute_nis_tuning(g_nis_sharpness);
                gl::UseProgram(g_program_nis);
                gl::ActiveTexture(GL_TEXTURE1);
                glBindTexture(GL_TEXTURE_2D, g_tex_nis_coef_scale);
                gl::ActiveTexture(GL_TEXTURE2);
                glBindTexture(GL_TEXTURE_2D, g_tex_nis_coef_usm);
                gl::Uniform1i(g_loc_nis_tex, 0);
                gl::Uniform1i(g_loc_nis_coef_scaler, 1);
                gl::Uniform1i(g_loc_nis_coef_usm, 2);
                gl::Uniform2f(g_loc_nis_src_size, (float)upscale_src_w, (float)upscale_src_h);
                gl::Uniform2f(g_loc_nis_texel_size, 1.0f / upscale_src_w, 1.0f / upscale_src_h);
                gl::Uniform1f(g_loc_nis_detect_ratio, nt.detectRatio);
                gl::Uniform1f(g_loc_nis_detect_thres, nt.detectThres);
                gl::Uniform1f(g_loc_nis_min_contrast_ratio, nt.minContrastRatio);
                gl::Uniform1f(g_loc_nis_ratio_norm, nt.ratioNorm);
                gl::Uniform1f(g_loc_nis_sharp_start_y, nt.sharpStartY);
                gl::Uniform1f(g_loc_nis_sharp_scale_y, nt.sharpScaleY);
                gl::Uniform1f(g_loc_nis_sharp_strength_min, nt.sharpStrengthMin);
                gl::Uniform1f(g_loc_nis_sharp_strength_scale, nt.sharpStrengthScale);
                gl::Uniform1f(g_loc_nis_sharp_limit_min, nt.sharpLimitMin);
                gl::Uniform1f(g_loc_nis_sharp_limit_scale, nt.sharpLimitScale);
                glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
                composite_source = g_tex_upscale;
                composite_sharpness = 100.0f; // NIS bakes its own USM sharpen in; RCAS stays a passthrough
            }
        }
    }

    // Pass C: RCAS sharpen (or passthrough), drawn straight to the backbuffer at `rect`.
    gl::BindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, drawable_w, drawable_h);
    set_composite_quad(rect, drawable_w, drawable_h);
    gl::UseProgram(g_program_rcas);
    gl::ActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, composite_source);
    gl::Uniform1i(g_loc_rcas_tex, 0);
    gl::Uniform2f(g_loc_rcas_texel_size, 1.0f / rect.w, 1.0f / rect.h);
    gl::Uniform1f(g_loc_rcas_sharpness, composite_sharpness);
    gl::BindVertexArray(g_vao_composite);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    gl::BindVertexArray(0);

    draw_overlay(drawable_w, drawable_h);
    draw_help(drawable_w, drawable_h);

    GLenum err;
    while ((err = glGetError()) != GL_NO_ERROR)
        Log::error() << "GL error 0x" << std::hex << err << std::dec << " during VideoRenderer::draw().";
}

void VideoRenderer::set_sharpness(float sharpness)
{
    // AMD's "stops" convention (FsrRcasCon): 0.0 = maximum sharpness, each
    // +1.0 halves it. Only clamped from below - negative stops would push
    // the lobe past the stability limit ffx_fsr1.h derives it from.
    g_sharpness = sharpness < 0.0f ? 0.0f : sharpness;
}

void VideoRenderer::show_overlay(const std::string &text)
{
    g_overlay_text = text.substr(0, kMaxOverlayChars);
    g_overlay_start_us = av_gettime_relative();
    g_overlay_active = !g_overlay_text.empty();
}

// Rebuilds the toast's glyph quads from g_overlay_text every time it's
// about to be drawn (not just when show_overlay() was called) - the NDC
// positions bake in the drawable size at build time, so if the window
// resizes (or fullscreen is toggled) while a toast happens to still be
// fading, stale geometry sized for the old drawable would render at the
// wrong scale in the new one. Rebuilding is cheap (a handful of quads), so
// it's simplest to just always do it fresh.
void VideoRenderer::build_overlay_geometry()
{
    int drawable_w = 0, drawable_h = 0;
    SDL_GL_GetDrawableSize(window, &drawable_w, &drawable_h);
    if (drawable_w <= 0 || drawable_h <= 0) {
        g_overlay_char_count = 0;
        return;
    }

    int count = (int)g_overlay_text.size();

    if (count > 0) {
        std::vector<float> verts;
        verts.reserve(count * 6 * 4);

        int textX0 = kOverlayMarginPx + kOverlayPaddingPx;
        int textY0 = kOverlayMarginPx + kOverlayPaddingPx;

        for (int i = 0; i < count; i++) {
            unsigned char c = (unsigned char)g_overlay_text[i];
            int glyph = (c < 128) ? c : 32; // anything outside ASCII falls back to space
            int col = glyph % kFontCols;
            int row = glyph / kFontCols;
            float u0 = (float)col / kFontCols;
            float v0 = (float)row / kFontRows;
            float u1 = (float)(col + 1) / kFontCols;
            float v1 = (float)(row + 1) / kFontRows;

            float px0 = (float)(textX0 + i * kGlyphCell);
            float py0 = (float)textY0;
            float px1 = px0 + kGlyphCell;
            float py1 = py0 + kGlyphCell;

            // Direct-to-screen quad (not rendered into an FBO for later
            // resampling), so this is the plain top->NDC+1 mapping - no
            // v-flip needed, unlike the video decode/upscale passes.
            float x0 = (px0 / drawable_w) * 2.0f - 1.0f;
            float x1 = (px1 / drawable_w) * 2.0f - 1.0f;
            float y0 = 1.0f - (py0 / drawable_h) * 2.0f;
            float y1 = 1.0f - (py1 / drawable_h) * 2.0f;

            float quad[24] = {
                x0, y0, u0, v0,
                x1, y0, u1, v0,
                x0, y1, u0, v1,

                x1, y0, u1, v0,
                x1, y1, u1, v1,
                x0, y1, u0, v1,
            };
            verts.insert(verts.end(), quad, quad + 24);
        }

        gl::BindBuffer(GL_ARRAY_BUFFER, g_vbo_text);
        gl::BufferSubData(GL_ARRAY_BUFFER, 0, verts.size() * sizeof(float), verts.data());
    }

    g_overlay_char_count = count;
    g_overlay_box_w = kOverlayPaddingPx * 2 + count * kGlyphCell;
    g_overlay_box_h = kOverlayPaddingPx * 2 + kGlyphCell;
}

void VideoRenderer::draw_overlay(int drawable_w, int drawable_h)
{
    if (!g_overlay_active)
        return;

    double elapsed = (av_gettime_relative() - g_overlay_start_us) / 1000000.0;
    if (elapsed >= kOverlayHoldSeconds + kOverlayFadeSeconds) {
        g_overlay_active = false;
        return;
    }

    build_overlay_geometry();

    float alpha = 1.0f;
    if (elapsed > kOverlayHoldSeconds)
        alpha = 1.0f - (float)((elapsed - kOverlayHoldSeconds) / kOverlayFadeSeconds);
    alpha = alpha < 0.0f ? 0.0f : (alpha > 1.0f ? 1.0f : alpha);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Background box, for legibility over arbitrary video content.
    {
        float bx0 = (float)kOverlayMarginPx;
        float by0 = (float)kOverlayMarginPx;
        float bx1 = bx0 + g_overlay_box_w;
        float by1 = by0 + g_overlay_box_h;

        float x0 = (bx0 / drawable_w) * 2.0f - 1.0f;
        float x1 = (bx1 / drawable_w) * 2.0f - 1.0f;
        float y0 = 1.0f - (by0 / drawable_h) * 2.0f;
        float y1 = 1.0f - (by1 / drawable_h) * 2.0f;

        float verts[16] = {
            x0, y0, 0.0f, 0.0f,
            x1, y0, 1.0f, 0.0f,
            x0, y1, 0.0f, 1.0f,
            x1, y1, 1.0f, 1.0f,
        };
        gl::BindBuffer(GL_ARRAY_BUFFER, g_vbo_box);
        gl::BufferSubData(GL_ARRAY_BUFFER, 0, sizeof(verts), verts);

        gl::UseProgram(g_program_flat);
        gl::Uniform4f(g_loc_flat_color, 0.0f, 0.0f, 0.0f, 0.55f * alpha);
        gl::BindVertexArray(g_vao_box);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    }

    if (g_overlay_char_count > 0) {
        gl::UseProgram(g_program_text);
        gl::ActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, g_tex_font_atlas);
        gl::Uniform1i(g_loc_text_atlas, 0);
        gl::Uniform3f(g_loc_text_color, 1.0f, 1.0f, 1.0f);
        gl::Uniform1f(g_loc_text_alpha, alpha);
        gl::BindVertexArray(g_vao_text);
        glDrawArrays(GL_TRIANGLES, 0, g_overlay_char_count * 6);
    }

    gl::BindVertexArray(0);
    glDisable(GL_BLEND);
}

void VideoRenderer::build_help_geometry()
{
    int drawable_w = 0, drawable_h = 0;
    SDL_GL_GetDrawableSize(window, &drawable_w, &drawable_h);
    if (drawable_w <= 0 || drawable_h <= 0)
        return;

    std::vector<float> verts;
    int maxLineLen = 0;
    int totalChars = 0;

    int textX0 = kOverlayMarginPx + kOverlayPaddingPx;
    int textY0 = kOverlayMarginPx + kOverlayPaddingPx;

    for (int line = 0; line < kHelpLineCount; line++) {
        const char *text = kHelpLines[line];
        int len = (int)strlen(text);
        if (len > maxLineLen)
            maxLineLen = len;

        for (int i = 0; i < len && totalChars < kMaxHelpChars; i++) {
            unsigned char c = (unsigned char)text[i];
            int glyph = (c < 128) ? c : 32;
            int col = glyph % kFontCols;
            int row = glyph / kFontCols;
            float u0 = (float)col / kFontCols;
            float v0 = (float)row / kFontRows;
            float u1 = (float)(col + 1) / kFontCols;
            float v1 = (float)(row + 1) / kFontRows;

            float px0 = (float)(textX0 + i * kGlyphCell);
            float py0 = (float)(textY0 + line * kGlyphCell);
            float px1 = px0 + kGlyphCell;
            float py1 = py0 + kGlyphCell;

            float x0 = (px0 / drawable_w) * 2.0f - 1.0f;
            float x1 = (px1 / drawable_w) * 2.0f - 1.0f;
            float y0 = 1.0f - (py0 / drawable_h) * 2.0f;
            float y1 = 1.0f - (py1 / drawable_h) * 2.0f;

            float quad[24] = {
                x0, y0, u0, v0,
                x1, y0, u1, v0,
                x0, y1, u0, v1,

                x1, y0, u1, v0,
                x1, y1, u1, v1,
                x0, y1, u0, v1,
            };
            verts.insert(verts.end(), quad, quad + 24);
            totalChars++;
        }
    }

    if (!verts.empty()) {
        gl::BindBuffer(GL_ARRAY_BUFFER, g_vbo_help_text);
        gl::BufferSubData(GL_ARRAY_BUFFER, 0, verts.size() * sizeof(float), verts.data());
    }

    g_help_char_count = totalChars;
    g_help_box_w = kOverlayPaddingPx * 2 + maxLineLen * kGlyphCell;
    g_help_box_h = kOverlayPaddingPx * 2 + kHelpLineCount * kGlyphCell;
}

void VideoRenderer::toggle_help()
{
    g_help_active = !g_help_active;
}

bool VideoRenderer::help_visible()
{
    return g_help_active;
}

void VideoRenderer::draw_help(int drawable_w, int drawable_h)
{
    if (!g_help_active)
        return;

    // Rebuilt every frame while visible (not just on toggle-on) - see
    // build_overlay_geometry()'s comment for why: otherwise stale geometry
    // sized for a since-resized drawable renders at the wrong scale, which
    // is exactly what toggling fullscreen while the panel is open used to do.
    build_help_geometry();

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    {
        float bx0 = (float)kOverlayMarginPx;
        float by0 = (float)kOverlayMarginPx;
        float bx1 = bx0 + g_help_box_w;
        float by1 = by0 + g_help_box_h;

        float x0 = (bx0 / drawable_w) * 2.0f - 1.0f;
        float x1 = (bx1 / drawable_w) * 2.0f - 1.0f;
        float y0 = 1.0f - (by0 / drawable_h) * 2.0f;
        float y1 = 1.0f - (by1 / drawable_h) * 2.0f;

        float verts[16] = {
            x0, y0, 0.0f, 0.0f,
            x1, y0, 1.0f, 0.0f,
            x0, y1, 0.0f, 1.0f,
            x1, y1, 1.0f, 1.0f,
        };
        gl::BindBuffer(GL_ARRAY_BUFFER, g_vbo_help_box);
        gl::BufferSubData(GL_ARRAY_BUFFER, 0, sizeof(verts), verts);

        gl::UseProgram(g_program_flat);
        gl::Uniform4f(g_loc_flat_color, 0.0f, 0.0f, 0.0f, 0.75f); // a bit more opaque than the toast - this one stays up
        gl::BindVertexArray(g_vao_help_box);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    }

    if (g_help_char_count > 0) {
        gl::UseProgram(g_program_text);
        gl::ActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, g_tex_font_atlas);
        gl::Uniform1i(g_loc_text_atlas, 0);
        gl::Uniform3f(g_loc_text_color, 1.0f, 1.0f, 1.0f);
        gl::Uniform1f(g_loc_text_alpha, 1.0f);
        gl::BindVertexArray(g_vao_help_text);
        glDrawArrays(GL_TRIANGLES, 0, g_help_char_count * 6);
    }

    gl::BindVertexArray(0);
    glDisable(GL_BLEND);
}

void VideoRenderer::cycle_upscaler()
{
    g_upscaler_mode = static_cast<UpscalerMode>((static_cast<int>(g_upscaler_mode) + 1) % kUpscalerModeCount);
}

const char *VideoRenderer::upscaler_name()
{
    return kUpscalerNames[static_cast<int>(g_upscaler_mode)];
}

void VideoRenderer::cycle_render_scale()
{
    g_scale_index = (g_scale_index + 1) % kRenderScalePresetCount;
}

const char *VideoRenderer::render_scale_name()
{
    return kRenderScalePresets[g_scale_index].name;
}

void VideoRenderer::clear()
{
    gl::BindFramebuffer(GL_FRAMEBUFFER, 0);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
}

void VideoRenderer::present()
{
    SDL_GL_SwapWindow(window);
}
