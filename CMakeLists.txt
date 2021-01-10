cmake_minimum_required(VERSION 3.17)
project(vm6502 VERSION 0.9 DESCRIPTION "Emulator for a very simple 6502 CPU based VM" LANGUAGES C)
set(CMAKE_C_STANDARD 11)

configure_file (
        "${PROJECT_SOURCE_DIR}/config.h.in"
        "${PROJECT_BINARY_DIR}/config.h"
)

add_executable(vm6502 main.c fake6502.c fake6502.h output.c output.h input.h input.c dissassembler.c dissassembler.h watches.c watches.h util.c util.h breakpoints.c breakpoints.h screen.c screen.h)
target_include_directories(vm6502 PUBLIC ${PROJECT_BINARY_DIR})
