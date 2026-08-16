
find_package(PkgConfig REQUIRED)
pkg_check_modules(SDL2 REQUIRED IMPORTED_TARGET sdl2)

add_library(sdl2 INTERFACE IMPORTED GLOBAL)

target_link_libraries(sdl2 INTERFACE
    PkgConfig::SDL2
)
