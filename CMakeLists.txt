cmake_minimum_required(VERSION 3.10)

project(Liquid)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED True)

add_subdirectory(vendor/glfw)

add_executable(liquid src/liquid.cpp)

target_link_libraries(liquid
    glfw
)
