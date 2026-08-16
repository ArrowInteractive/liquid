cmake_minimum_required(VERSION 3.10)

project(Liquid)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

if(WIN32)
  message(STATUS ">>> Not Linux")
  set(CMAKE_EXE_LINKER_FLAGS "-static -mwindows")
endif()

include_directories(include)

FILE(GLOB_RECURSE SOURCES src/*.cpp)

add_executable(liquid ${SOURCES})
# add dependencies
include(cmake/FFMPEG.cmake)
include(cmake/SDL2.cmake)
include(cmake/OpenGL.cmake)

target_link_libraries(liquid
    ffmpeg
    sdl2
    opengl
)
