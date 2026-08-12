# blink_hello: minimal CH32F207VCT6 (f207-evt-r1) template.
# Blinks LED1 (PA0, low active, connected with an external wire) and prints
# the current system clock + LED state over USART1 (WCH-Link SERIAL,
# 115200 8-N-1) on every toggle. Intended as the starting point for new
# f207-evt-r1 projects: clock (144 MHz PLL), UART console, delays and the
# newlib stubs all come from the shared board layer (../../board + ../../cmake).
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

project(blink_hello C ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(${PROJECT_NAME}.elf
    src/main.c
)

include(${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/ch32f207_board.cmake)
ch32f207_apply_board(${PROJECT_NAME}.elf "-O2")

include(${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/objcopy-targets.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/flash-targets.cmake)
