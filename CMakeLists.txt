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

find_package(SDL2 REQUIRED)
if ( NOT SDL2_FOUND )
    message ( FATAL_ERROR ">>> SDL2 not found!" )
endif ( NOT SDL2_FOUND )

include_directories(${GLEW_INCLUDE_DIRS})
include_directories(${SDL2_INCLUDE_DIR})
include_directories(include)

FILE(GLOB SOURCES src/*.cpp)

add_executable(liquid ${SOURCES})
add_subdirectory(vendor/ffmpeg)
add_subdirectory(vendor/imgui)

target_link_libraries(liquid
    ffmpeg
    imgui
    ${SDL2_LIBRARY}
)