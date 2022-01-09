cmake_minimum_required(VERSION 3.10)

project(Liquid)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

if(LINUX) 
        message(STATUS ">>> Linux")
else()
        message(STATUS ">>> Not Linux")
        set(CMAKE_EXE_LINKER_FLAGS "-static")
endif()

include_directories(include)

FILE(GLOB SOURCES src/*.cpp)

add_executable(liquid ${SOURCES})

add_subdirectory(vendor/glad)
add_subdirectory(vendor/ffmpeg)
add_subdirectory(vendor/SDL)

target_link_libraries(liquid
    glad
    ffmpeg
    SDL2main
    SDL2-static
)
