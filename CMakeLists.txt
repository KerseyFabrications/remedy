cmake_minimum_required(VERSION 3.10)
project(remedy VERSION 0.3.0 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Compiler flags
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -Wpedantic")
set(CMAKE_C_FLAGS_DEBUG "-g -O0")
set(CMAKE_C_FLAGS_RELEASE "-O2")

# Find dependencies via pkg-config
find_package(PkgConfig REQUIRED)
pkg_check_modules(NCURSESW REQUIRED ncursesw)
pkg_check_modules(CMARK_GFM REQUIRED libcmark-gfm)

# Include directories
include_directories(${PROJECT_SOURCE_DIR}/include)
include_directories(${NCURSESW_INCLUDE_DIRS})
include_directories(${CMARK_GFM_INCLUDE_DIRS})

# Source files
set(SOURCES
    src/main.c
    src/md_parser.c
    src/render_line.c
    src/renderer.c
    src/syntax_highlight.c
    src/search.c
    src/sanitize.c
    src/lint.c
    src/table_layout.c
    src/kitty_graphics.c
    src/stb_image_impl.c
    src/pager.c
    src/input.c
)

# Create executable
add_executable(remedy ${SOURCES})
target_compile_definitions(remedy PRIVATE REMEDY_VERSION="${PROJECT_VERSION}")

# Link libraries
target_link_libraries(remedy
    ${NCURSESW_LIBRARIES}
    ${CMARK_GFM_LIBRARIES}
    m
)

# Link directories (pkg-config may put libs in non-standard paths)
target_link_directories(remedy PRIVATE
    ${NCURSESW_LIBRARY_DIRS}
    ${CMARK_GFM_LIBRARY_DIRS}
)

# Install
include(GNUInstallDirs)
install(TARGETS remedy RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES man/remedy.1 DESTINATION ${CMAKE_INSTALL_MANDIR}/man1)
