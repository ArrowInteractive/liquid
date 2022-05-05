# Liquid Media Player 
Lightweight media player written in C++ using FFmpeg and SDL2. Currently in development.

[![CodeQL](https://github.com/ArrowInteractive/liquid/actions/workflows/codeql-analysis.yml/badge.svg)](https://github.com/ArrowInteractive/liquid/actions/workflows/codeql-analysis.yml)

# Build Guide

## Linux 

### Arch based distributions:

Install the necessary packages using PACMAN:
```
sudo pacman -S --needed base-devel cmake ninja ffmpeg
```

Then execute ```./build.sh```

### RPM based distributions(Fedora, RHEL, etc.):

Install the necessary packages using DNF(Incomplete list):
```
sudo dnf install ffmpeg ffmpeg-devel g++ gdb mesa-libGL-devel mesa-libGLU-devel mesa-libGLw-devel mesa-libOSMesa-devel libXext-devel alsa-lib-devel
```

Then execute ```./build.sh```

### Debian based distributions:

Install the necessary packages using APT:
```
sudo apt install ffmpeg libavcodec-dev libavformat-dev libavfilter-dev libavdevice-dev libavutil-dev libswresample-dev libswscale-dev cmake ninja-build
```

Then execute ```./build.sh```

### Void Linux

Install the necessary packages using XBPS:
```
sudo xbps-install cmake ninja ffmpeg ffmpeg-devel pkg-config gdb SDL2-devel
```

Then execute ```./build.sh```


## Windows

Install the [MSYS2](https://www.msys2.org/ "MSYS2 Homepage") Building Platform for Windows, Then install the necessary packages through Pacman by using:

```
pacman -S --needed base-devel mingw-w64-x86_64-cmake mingw-w64-x86_64-ninja mingw-w64-x86_64-ffmpeg mingw-w64-x86_64-toolchain
```
Add the MSYS2 paths to your system environment variables.

They may look like this:
<b>C:\msys64\usr\bin and C:\msys64\mingw64\bin</b>

Then execute ```./cleanbuild.ps1```

