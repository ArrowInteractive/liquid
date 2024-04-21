cmake_minimum_required(VERSION 3.10)

project(Liquid)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

#finding the ffmpeg config

find_package(PkgConfig REQUIRED)
pkg_check_modules(AVCODEC     REQUIRED IMPORTED_TARGET libavcodec)
pkg_check_modules(AVFORMAT    REQUIRED IMPORTED_TARGET libavformat)
pkg_check_modules(AVFILTER    REQUIRED IMPORTED_TARGET libavfilter)
pkg_check_modules(AVDEVICE    REQUIRED IMPORTED_TARGET libavdevice)
pkg_check_modules(AVUTIL      REQUIRED IMPORTED_TARGET libavutil)
pkg_check_modules(SWRESAMPLE  REQUIRED IMPORTED_TARGET libswresample)
pkg_check_modules(SWSCALE     REQUIRED IMPORTED_TARGET libswscale)

add_library(ffmpeg INTERFACE IMPORTED GLOBAL)

target_link_libraries(ffmpeg INTERFACE
    PkgConfig::AVCODEC
    PkgConfig::AVFORMAT
    PkgConfig::AVFILTER
    PkgConfig::AVDEVICE
    PkgConfig::AVUTIL
    PkgConfig::SWRESAMPLE
    PkgConfig::SWSCALE
)

set(imgui_renderer "SDL_Renderer2")
set(imgui_platform "SDL2")

if (WIN32)
    message(STATUS ">>> Not Linux")
    set(CMAKE_EXE_LINKER_FLAGS "-static")
endif (WIN32)

include_directories(include)

FILE(GLOB SOURCES src/*.cpp)

add_executable(liquid ${SOURCES})
# add dependencies
include(cmake/CPM.cmake)

CPMAddPackage("gh:Curve/imgui-cmake#c35523c")

target_link_libraries(liquid
    ffmpeg
    imgui
)
