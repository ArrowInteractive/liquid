#pragma once

// Filter coefficient LUTs for the NIS (NVIDIA Image Scaling) upscaler,
// copied verbatim from NVIDIA's MIT-licensed reference source
// (NIS_Config.h, NVIDIAGameWorks/NVIDIAImageScaling). 64 phases x 8 floats
// each - only the first 6 of each row of 8 are used by the 6-tap filter
// (the trailing two are always 0, kept only to match NVIDIA's own texture
// packing: two RGBA32F texels per phase).
extern const float nis_coef_scale[64 * 8];
extern const float nis_coef_usm[64 * 8];
