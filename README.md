# Liquid Media Player 
Free and open-source media player written in C++. Currently in development.

[![CodeQL](https://github.com/ArrowInteractive/liquid/actions/workflows/codeql-analysis.yml/badge.svg)](https://github.com/ArrowInteractive/liquid/actions/workflows/codeql-analysis.yml)

# Build Guide

## Windows

Install the [MSYS2](https://www.msys2.org/ "MSYS2 Homepage") Building Platform for Windows, Then install the necessary packages through Pacman by using:

```pacman -S git base-devel mingw-w64-x86_64-cmake mingw-w64-x86_64-ffmpeg mingw-w64-x86_64-toolchain```

Then execute ```./cleanbuild.ps1``` (You may need to add the MSYS2 paths to the system environment variables.)

## Linux 

### Arch based systems:

Install the necessary packages through Pacman by using:
```sudo pacman -S git base-devel cmake ffmpeg```

Then execute ```./build.sh```

