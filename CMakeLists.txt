cmake_minimum_required(VERSION 3.10)

project(Liquid)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

if (WIN32)
    message(STATUS ">>> Not Linux")
    set(CMAKE_EXE_LINKER_FLAGS "-static")
endif (WIN32)

include_directories(include)

FILE(GLOB SOURCES src/*.cpp)

add_executable(liquid ${SOURCES})
add_subdirectory(vendor/ffmpeg)
add_subdirectory(vendor/imgui-cmake)

target_link_libraries(liquid
    ffmpeg
    SDL2main
    SDL2-static
    imgui
)