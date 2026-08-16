#pragma once

// Pretrained FSRCNN(d=56, s=12, m=4), 2x scale, weights - extracted from
// Saafke/FSRCNN_Tensorflow's FSRCNN_x2.pb (Apache-2.0), the same model
// OpenCV's own dnn_superres docs point to:
// https://github.com/Saafke/FSRCNN_Tensorflow/blob/master/models/FSRCNN_x2.pb
//
// TensorFlow conv2d filter layout: [kernel_h, kernel_w, in_channels,
// out_channels], row-major. Biases and PReLU slopes are per-output-channel.
// Architecture (all convs stride 1, SAME/zero padding):
//   conv1 (f1,b1) 5x5,  1->56, PReLU(alpha1)   feature extraction
//   conv2 (f2,b2) 1x1, 56->12, PReLU(alpha2)   shrink
//   conv3..6 (f3..6,b3..6) 3x3, 12->12, PReLU(alpha3..6) each  mapping x4
//   conv7 (f7,b7) 1x1, 12->56, PReLU(alpha7)   expand
//   conv8 (f8,b8) 1x1, 56->4 (no activation)
//   depth_to_space(block=2) + bias b8 -> single-channel output at 2x
//   resolution (sub-pixel/pixel-shuffle upsampling, not a learned deconv -
//   this specific export uses that variant of FSRCNN).
// Operates on the Y (luma) channel only; Cb/Cr are upsampled separately.
extern const float fsrcnn_f1[1400];
extern const float fsrcnn_b1[56];
extern const float fsrcnn_alpha1[56];
extern const float fsrcnn_f2[672];
extern const float fsrcnn_b2[12];
extern const float fsrcnn_alpha2[12];
extern const float fsrcnn_f3[1296];
extern const float fsrcnn_f4[1296];
extern const float fsrcnn_f5[1296];
extern const float fsrcnn_f6[1296];
extern const float fsrcnn_b3[12];
extern const float fsrcnn_b4[12];
extern const float fsrcnn_b5[12];
extern const float fsrcnn_b6[12];
extern const float fsrcnn_alpha3[12];
extern const float fsrcnn_alpha4[12];
extern const float fsrcnn_alpha5[12];
extern const float fsrcnn_alpha6[12];
extern const float fsrcnn_f7[672];
extern const float fsrcnn_b7[56];
extern const float fsrcnn_alpha7[56];
extern const float fsrcnn_f8[224];
extern const float fsrcnn_b8[1];
