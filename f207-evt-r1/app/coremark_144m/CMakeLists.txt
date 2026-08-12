# CoreMark 1.0.1 benchmark for the CH32F207VCT6 (f207-evt-r1) @ 144 MHz.
# GCC at the most aggressive optimization (-Ofast -funroll-loops), optionally
# with -flto (STM32_LTO=ON).
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

project(coremark_144m C ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(CORE ${CMAKE_CURRENT_SOURCE_DIR}/coremark_1_0_1)

add_executable(${PROJECT_NAME}.elf
    src/main.c
    src/utils.c
    src/core_portme.c
    ${CORE}/core_list_join.c
    ${CORE}/core_main.c
    ${CORE}/core_matrix.c
    ${CORE}/core_state.c
    ${CORE}/core_util.c
)

include(${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/ch32f207_board.cmake)
ch32f207_apply_board(${PROJECT_NAME}.elf "-Ofast -funroll-loops")

target_include_directories(${PROJECT_NAME}.elf PRIVATE
    src
    ${CORE}
)

include(${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/objcopy-targets.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/flash-targets.cmake)
