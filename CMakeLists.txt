cmake_minimum_required(VERSION 3.10)

project(Liquid)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

set(imgui_render "SDL")
set(imgui_backend "SDL")

if (WIN32)
    message(STATUS ">>> Not Linux")
    set(CMAKE_EXE_LINKER_FLAGS "-static")
endif (WIN32)

find_package(OpenGL REQUIRED)
find_package(GLEW REQUIRED)

include_directories(include)
include_directories(vendor/sdl-gpu/include)
include_directories(${OPENGL_INCLUDE_DIRS})
include_directories(${GLEW_INCLUDE_DIRS})

FILE(GLOB SOURCES src/*.cpp)

add_executable(liquid ${SOURCES})
add_subdirectory(vendor/ffmpeg)
add_subdirectory(vendor/SDL)
add_subdirectory(vendor/sdl-gpu)
add_subdirectory(vendor/imgui)

target_link_libraries(liquid
    ffmpeg
    ${OPENGL_LIBRARIES}
    ${GLEW_LIBRARIES}
    SDL2main
    SDL2-static
    SDL_gpu
    imgui
)