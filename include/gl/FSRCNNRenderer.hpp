#pragma once

#include "state/datastate.hpp"
#include "gl/GLLoader.hpp"

// Runs a pretrained FSRCNN(d=56, s=12, m=4) 2x super-resolution network
// (see FSRCNNWeights.hpp for the weights/architecture) as a chain of GL 3.3
// core fragment-shader passes. GL 3.3 core has no compute shaders, and a
// fragment shader only writes 4 channels (RGBA) at a time, so the network's
// 56/12-channel feature maps are packed across multiple RGBA32F textures
// (4 channels each), and each conv layer runs as several passes - one per
// group of up to 4 output channels, each sampling every input-channel
// group. For this architecture that's 14+3+12+14+1 = 44 conv passes, plus
// colorspace prep/recombine and the final depth-to-space step.
class FSRCNNRenderer
{
    public:
        static bool init();
        static void destroy();

        // Runs the full pipeline on `rgb_tex` (RGBA, src_w x src_h - the
        // same post-render-scale-preset-downsample source EASU/NIS use).
        // Fixed 2x scale: always produces (src_w*2, src_h*2), written into
        // `out_w`/`out_h`. Returns a GL_RGBA32F texture owned internally
        // (valid until the next call) - the caller fits it to the actual
        // destination rect the normal way (the composite pass samples it
        // via UV, which is resolution-independent).
        static GLuint run(GLuint rgb_tex, int src_w, int src_h, int &out_w, int &out_h);
};
