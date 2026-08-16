# Liquid Media Player 
Lightweight media player written in C++ using FFmpeg and SDL2. Currently in development.

[![CodeQL](https://github.com/ArrowInteractive/liquid/actions/workflows/codeql-analysis.yml/badge.svg)](https://github.com/ArrowInteractive/liquid/actions/workflows/codeql-analysis.yml)

# Upscaling

Liquid renders video through a custom OpenGL pipeline (decode -> upscale -> sharpen) instead of relying on a plain bilinear stretch, and can upscale in real time using any of four upscaling algorithms, faithfully ported from their official reference implementations:

- **AMD FidelityFX Super Resolution 1 (FSR1)** - the EASU edge-adaptive upscale pass followed by the RCAS contrast-adaptive sharpen pass, ported from AMD's MIT-licensed [`ffx_fsr1.h`](https://github.com/GPUOpen-Effects/FidelityFX-FSR).
- **NVIDIA Image Scaling (NIS)** - NVScaler's directional-filter upscale with its built-in adaptive sharpening, ported from NVIDIA's MIT-licensed [`NIS_Scaler.h`](https://github.com/NVIDIAGameWorks/NVIDIAImageScaling).
- **FSRCNN** - a small trained convolutional neural network (not a hand-tuned filter like the others), run as ~44 chained fragment-shader passes since OpenGL 3.3 core has no compute shaders and a fragment shader only writes 4 channels at a time, while this network's feature maps have 56 and 12 channels. Uses real pretrained weights (Apache-2.0) from [`Saafke/FSRCNN_Tensorflow`](https://github.com/Saafke/FSRCNN_Tensorflow), the same model OpenCV's own `dnn_superres` docs point to. Only supports a fixed 2x scale - anything else needed to fill the destination is a plain bilinear fit on top of that.
- **RAVU-Lite** - a RAISR-style (Rapid and Accurate Image Super Resolution) upscaler: a local gradient structure tensor is eigen-decomposed per pixel into (angle, strength, coherence), quantized into one of 288 buckets, each selecting a trained filter kernel. Ported from [`bjin/mpv-prescalers`](https://github.com/bjin/mpv-prescalers) (LGPLv3 - the one non-permissive dependency here; Liquid is GPLv2-or-later, which is compatible). Also a fixed 2x scale, same as FSRCNN.

All four run as plain OpenGL 3.3 core fragment shaders (no compute shaders, so they also work on macOS, which never got OpenGL compute support), and none depend on the vendor their name suggests - FSR1 and NIS run identically on any GPU, AMD, NVIDIA, Intel, or otherwise, and FSRCNN/RAVU-Lite are just small trained models/filters with no vendor ties at all.

### Controls

| Key | Action |
| --- | --- |
| `U` | Cycle the active upscaler: Bilinear (off) -> FSR1 (EASU + RCAS) -> NIS -> FSRCNN -> RAVU-Lite -> back to Bilinear |
| `R` | Cycle the render-scale quality preset: Native -> Ultra Quality (1.3x) -> Quality (1.5x) -> Balanced (1.7x) -> Performance (2.0x) |

The render-scale preset controls how much resolution is deliberately thrown away *before* the upscaler runs, then reconstructed back up to display size - the same tradeoff as picking a quality mode in a game's FSR/NIS setting, useful for judging how much detail an upscaler can actually recover at a given ratio. Native (the default) skips this and feeds the upscaler the full decoded frame.

Switching upscalers or presets shows a brief on-screen toast and logs the new state to the console, so it's easy to A/B compare - e.g. pause on a detailed frame and press `U` to flip between algorithms without losing your place.

# Build Guide

## Linux 

### Installing Dependency

#### Arch based distributions:

Install the necessary packages using PACMAN:
```
sudo pacman -S --needed base-devel make cmake ninja ffmpeg sdl2
```

#### RPM based distributions(Fedora, RHEL, etc.):

Install the necessary packages using DNF(Incomplete list):
```
sudo dnf install ffmpeg ffmpeg-devel g++ gdb mesa-libGL-devel mesa-libGLU-devel mesa-libGLw-devel mesa-libOSMesa-devel libXext-devel alsa-lib-devel make cmake ninja-build SDL2-devel
```

#### Debian based distributions:

Install the necessary packages using APT:
```
sudo apt install ffmpeg libavcodec-dev libavformat-dev libavfilter-dev libavdevice-dev libavutil-dev libswresample-dev libswscale-dev libsdl2-dev make cmake ninja-build
```

#### Void Linux

Install the necessary packages using XBPS:
```
sudo xbps-install cmake make ninja ffmpeg ffmpeg-devel pkg-config gdb SDL2-devel
```

### Building

To build the project:

ninja:
```
make build_ninja
```
or

make
```
make build_make
```

### Install

To Install to local binary:

ninja:
```
make install_ninja
```
make:
```
make install_make
```

## macOS

### Installing Dependency

Install the necessary packages using [Homebrew](https://brew.sh/ "Homebrew Homepage"):
```
brew install cmake ninja pkg-config ffmpeg sdl2
```

### Building

To build the project:

ninja:
```
make build_ninja
```
or

make
```
make build_make
```

### Install

To Install to local binary:

ninja:
```
make install_ninja
```
make:
```
make install_make
```

## Windows

Install the [MSYS2](https://www.msys2.org/ "MSYS2 Homepage") Building Platform for Windows,

First update the msys with pacman using:

```
pacman -Syu
```

Then install the necessary packages through Pacman by using:

```
pacman -Syu --needed mingw-w64-x86_64-cmake mingw-w64-x86_64-ninja mingw-w64-x86_64-ffmpeg mingw-w64-x86_64-toolchain mingw-w64-x86_64-SDL2
```
Add the MSYS2 paths to your system environment variables.

They may look like this:
<b>C:\msys64\usr\bin and C:\msys64\mingw64\bin</b>

Then execute ```cmake . -B build -G "MSYS Makefiles"```
change to build and use ```make```
