cmake_minimum_required(VERSION 3.11)
set(CMAKE_EXPORT_COMPILE_COMMANDS 1)

project(fm-synth LANGUAGES C)

add_executable(fm-synth src/main.c)
target_link_libraries(fm-synth PUBLIC raylib m) # MATH
target_include_directories(fm-synth PUBLIC include/)
