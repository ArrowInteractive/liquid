cmake_minimum_required(VERSION 3.10)

project(Liquid)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

include_directories(include)

FILE(GLOB SOURCES src/*.cpp)

add_executable(liquid ${SOURCES})

add_subdirectory(vendor/glad)
add_subdirectory(vendor/ffmpeg)
add_subdirectory(vendor/SDL)

target_link_libraries(liquid
    glad
    ffmpeg
    SDL2
)
