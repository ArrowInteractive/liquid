# Liquid Media Player 
Free and open-source media player written in C++. Currently in development.

[![CodeQL](https://github.com/ArrowInteractive/liquid/actions/workflows/codeql-analysis.yml/badge.svg)](https://github.com/ArrowInteractive/liquid/actions/workflows/codeql-analysis.yml)

# Build Guide

## Linux 

### Arch based systems:

Install the necessary packages through Pacman by using:
```
sudo pacman -S base-devel cmake ninja ffmpeg sdl2
```

Then execute ```./build.sh```

### Debian based systems:

Install the necessary packages through APT by using:
```
sudo apt install ffmpeg libavcodec-dev libavformat-dev libavfilter-dev libavdevice-dev libavutil-dev libswresample-dev libswscale-dev libsdl2-dev cmake ninja-build
```

Then execute ```./build.sh```


## Windows

Install the [MSYS2](https://www.msys2.org/ "MSYS2 Homepage") Building Platform for Windows, Then install the necessary packages through Pacman by using:

```
pacman -S base-devel mingw-w64-x86_64-cmake mingw-w64-x86_64-ninja mingw-w64-x86_64-ffmpeg mingw-w64-x86_64-toolchain
```
Then execute ```./cleanbuild.ps1``` (You may want to add the MSYS2 paths to the system environment variables.)

