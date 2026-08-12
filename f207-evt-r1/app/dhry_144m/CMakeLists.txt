# Dhrystone 2.1 benchmark for the CH32F207VCT6 (f207-evt-r1) @ 144 MHz.
# GCC at the most aggressive optimization (-Ofast -funroll-loops), optionally
# with -flto (STM32_LTO=ON - but see the README: do not use LTO for Dhrystone).
#
# Workflow:
#   bash build.sh                      # CMake + Ninja + GNU arm-none-eabi-gcc
#   ninja flash                        # WCH OpenOCD + WCH-Link (CMSIS-DAP/SWD)
#   ninja bin                          # optional raw .bin image

cmake_minimum_required(VERSION 3.13)

if(POLICY CMP0123)
    cmake_policy(SET CMP0123 NEW)
endif()

option(STM32_LTO "Enable -flto with the GCC toolchain" OFF)

if(NOT CMAKE_TOOLCHAIN_FILE)
    set(CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/arm-none-eabi-toolchain.cmake")
endif()

project(dhry_144m C ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(${PROJECT_NAME}.elf
    src/main.c
    src/utils.c
    src/dhry_1.c
    src/dhry_2.c
)

include(${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/ch32f207_board.cmake)
ch32f207_apply_board(${PROJECT_NAME}.elf "-Ofast -funroll-loops")

include(${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/objcopy-targets.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/flash-targets.cmake)
