# Liquid Media Player 
Lightweight media player written in C++ using FFmpeg and SDL2. Currently in development.

[![CodeQL](https://github.com/ArrowInteractive/liquid/actions/workflows/codeql-analysis.yml/badge.svg)](https://github.com/ArrowInteractive/liquid/actions/workflows/codeql-analysis.yml)

# Build Guide

## Linux 

### Installing Dependency

#### Arch based distributions:

Install the necessary packages using PACMAN:
```
sudo pacman -S --needed base-devel make cmake ninja ffmpeg
```

#### RPM based distributions(Fedora, RHEL, etc.):

Install the necessary packages using DNF(Incomplete list):
```
sudo dnf install ffmpeg ffmpeg-devel g++ gdb mesa-libGL-devel mesa-libGLU-devel mesa-libGLw-devel mesa-libOSMesa-devel libXext-devel alsa-lib-devel make cmake ninja-build
```

#### Debian based distributions:

Install the necessary packages using APT:
```
sudo apt install ffmpeg libavcodec-dev libavformat-dev libavfilter-dev libavdevice-dev libavutil-dev libswresample-dev libswscale-dev make cmake ninja-build
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

## Windows

Install the [MSYS2](https://www.msys2.org/ "MSYS2 Homepage") Building Platform for Windows, Then install the necessary packages through Pacman by using:

```
pacman -S --needed base-devel mingw-w64-x86_64-cmake mingw-w64-x86_64-ninja mingw-w64-x86_64-ffmpeg mingw-w64-x86_64-toolchain
```
Add the MSYS2 paths to your system environment variables.

They may look like this:
<b>C:\msys64\usr\bin and C:\msys64\mingw64\bin</b>

Open a PowerShell prompt as administrator and execute ```Set-ExecutionPolicy Unrestricted -Force``` to enable execution of PowerShell scripts.

Then execute ```.\build.ps1```

