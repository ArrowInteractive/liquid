cmake_minimum_required(VERSION 3.10)

project(glad)

add_library(glad STATIC
    src/glad.c
)

target_include_directories(${PROJECT_NAME} PUBLIC include)