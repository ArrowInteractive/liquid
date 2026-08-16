#pragma once

// RAVU-Lite (radius=2) trained filter-selection weights, extracted from
// bjin/mpv-prescalers' generator source (LGPLv3):
// https://github.com/bjin/mpv-prescalers/blob/source/ravu-lite.py
// https://github.com/bjin/mpv-prescalers/blob/source/weights/ravu-lite_weights-r2.py
//
// RAVU is a RAISR-style (Rapid and Accurate Image Super Resolution)
// upscaler: for each source pixel, a local gradient structure tensor is
// computed over its 3x3 neighborhood (n = radius*2-1 = 3) and
// eigen-decomposed into (angle, strength, coherence); those are quantized
// into a bucket (24 x 4 x 3 = 288 buckets) that selects a trained 3x3 filter
// kernel, applied to produce all 4 of that pixel's 2x-upscale sub-pixel
// outputs at once (the kernel is symmetric under 180-degree rotation, so
// only 5 of the 9 taps are stored per bucket, each holding a vec4 of the 4
// sub-pixel positions' weights - the missing 4 taps and their per-position
// weights are recovered via that symmetry, see VideoRenderer's actual
// lookup code).
//
// min_strength[3] / min_coherence[2]: thresholds bucketing local gradient
// magnitude/coherence into 4 and 3 buckets respectively (bucket = count of
// thresholds the value exceeds).
// gaussian[3][3] (flattened row-major): Gaussian weights for the structure
// tensor accumulation.
// lut: flattened [angle(24)][strength(4)][coherence(3)][tap(5)][4] filter
// kernel table (row-major, matching the same nesting order used to bucket
// into the LUT's row index).
extern const float ravu_lite_r2_min_strength[3];
extern const float ravu_lite_r2_min_coherence[2];
extern const float ravu_lite_r2_gaussian[9];
extern const float ravu_lite_r2_lut[5760];
