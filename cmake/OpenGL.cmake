find_package(OpenGL REQUIRED)

add_library(opengl INTERFACE IMPORTED GLOBAL)

target_link_libraries(opengl INTERFACE
    OpenGL::GL
)
