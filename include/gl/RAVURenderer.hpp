#pragma once

#include "state/datastate.hpp"
#include "gl/GLLoader.hpp"

// Runs RAVU-Lite (radius=2), a RAISR-style trained upscaler, faithfully
// ported from bjin/mpv-prescalers (LGPLv3) - see RAVUWeights.hpp for the
// algorithm/weights. Fixed 2x scale, luma-only (same Y/CbCr split as
// FSRCNNRenderer, and the same two-stage "compute all 4 sub-pixel outputs
// packed into one RGBA texel per source pixel, then unpack" shape, since
// RAVU-Lite's own reference shader works the same way for the same reason -
// a fragment shader can only write 4 channels at once).
class RAVURenderer
{
    public:
        static bool init();
        static void destroy();

        // Runs the full pipeline on `rgb_tex` (RGBA, src_w x src_h - the
        // same post-render-scale-preset-downsample source the other
        // upscalers use). Fixed 2x scale: always produces
        // (src_w*2, src_h*2), written into `out_w`/`out_h`. Returns a
        // GL_RGBA32F texture owned internally (valid until the next call).
        static GLuint run(GLuint rgb_tex, int src_w, int src_h, int &out_w, int &out_h);
};
