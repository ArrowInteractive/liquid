cmake_minimum_required(VERSION 3.10)

project(Liquid)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

set(imgui_renderer "SDL_Renderer2")
set(imgui_platform "SDL2")

if(WIN32)
  message(STATUS ">>> Not Linux")
  set(CMAKE_EXE_LINKER_FLAGS "-static -mwindows")
endif()

include_directories(include)

FILE(GLOB_RECURSE SOURCES src/*.cpp)

add_executable(liquid ${SOURCES})
# add dependencies
include(cmake/CPM.cmake)
include(cmake/FFMPEG.cmake)

CPMAddPackage(
  NAME imgui_cmake
  GITHUB_REPOSITORY Curve/imgui-cmake
  GIT_TAG c35523cf4ab6cae03d48672d8a9922016a8a816f
)

target_link_libraries(liquid
    ffmpeg
    imgui
)
