# Shared CH32F207VC (f207-evt-r1 board) board layer for the apps.
# The board support (144 MHz clock from the 8 MHz HSE, USART1 console on
# PA9/PA10, newlib stubs, GCC startup, linker script) plus the WCH CH32F20x
# StdPeriphDriver sources are attached to a target with
# ch32f207_apply_board().
#
# Usage (from a project CMakeLists.txt, after add_executable()):
#   include(${CMAKE_CURRENT_SOURCE_DIR}/../cmake/ch32f207_board.cmake)
#   ch32f207_apply_board(${PROJECT_NAME}.elf "-O2")
#
# Requires the project to enable ASM (project(X C ASM)).
#
# Clock: SYSCLK_FREQ_144MHz_HSE is passed so system_ch32f20x.c (called by the
# startup file) programs PLL x18 from the 8 MHz HSE -> 144 MHz. Override with
# -DSYSCLK_FREQ=96000000 etc. before including this file.

set(F207_ROOT
    "${CMAKE_CURRENT_LIST_DIR}/.." CACHE PATH
    "Root of the f207-evt-r1 board tree (contains board/, cmake/, drivers/)")

set(BOARD_DIR ${F207_ROOT}/board)
set(CH32_DRV ${F207_ROOT}/drivers)
set(CH32_SPL ${CH32_DRV}/CH32F20x/StdPeriphDriver)
set(CH32_INC ${CH32_DRV}/CH32F20x/Include)
set(CH32_CMSIS ${CH32_DRV}/CMSIS)

# Linker script for the CH32F207VC build (256 KB flash, 64 KB RAM).
set(CH32F207_LINKER_SCRIPT "${BOARD_DIR}/ch32f207vc.ld" CACHE FILEPATH
    "Linker script for the CH32F207VC build")

# System init source. The vendored copy in board/ has SYSCLK_FREQ_144MHz_HSE
# defined via the CH32_SYSCLK cache variable below so the PLL -> 144 MHz tree
# is configured by SystemInit() at reset.
set(CH32F207_SYSTEM_SOURCE "${BOARD_DIR}/../drivers/CH32F20x/Source/system_ch32f20x.c" CACHE FILEPATH
    "System init source for the CH32F207VC build")

if(NOT DEFINED CH32_SYSCLK_MHZ)
    set(CH32_SYSCLK_MHZ "144" CACHE STRING
        "System clock frequency in MHz (HSE: 48/56/72/96/120/144)")
endif()
math(EXPR _SYSCLK_HZ "${CH32_SYSCLK_MHZ}000000")
set(_SYSCLK_DEF "SYSCLK_FREQ_${CH32_SYSCLK_MHZ}MHz_HSE=${_SYSCLK_HZ}")

function(ch32f207_apply_board TGT OPT)
    separate_arguments(OPT_LIST NATIVE_COMMAND "${OPT}")

    set(_WARN_FLAGS
        -Wall
        -Wno-unused-but-set-variable -Wno-unused-function
        -Wno-unused-variable -Wno-unused-parameter -Wno-maybe-uninitialized)

    target_sources(${TGT} PRIVATE
        ${BOARD_DIR}/board.c
        ${BOARD_DIR}/uart_printf.c
        ${BOARD_DIR}/syscalls.c
        ${BOARD_DIR}/startup_ch32f20x_D8C.S
        ${CH32F207_SYSTEM_SOURCE}
        ${CH32_CMSIS}/core_cm3.c
        ${CH32_SPL}/src/ch32f20x_gpio.c
        ${CH32_SPL}/src/ch32f20x_rcc.c
        ${CH32_SPL}/src/ch32f20x_usart.c
        ${CH32_SPL}/src/ch32f20x_misc.c
    )

    target_include_directories(${TGT} PRIVATE
        ${BOARD_DIR}
        ${CH32_INC}
        ${CH32_SPL}/inc
        ${CH32_CMSIS}
    )

    target_compile_definitions(${TGT} PRIVATE
        CH32F20x_D8C
        ${_SYSCLK_DEF}
    )

    target_compile_options(${TGT} PRIVATE
        -mcpu=cortex-m3 -mthumb
        ${OPT_LIST} -g
        -ffunction-sections -fdata-sections ${_WARN_FLAGS}
    )

    set_target_properties(${TGT} PROPERTIES
        LINK_FLAGS "-mcpu=cortex-m3 -mthumb ${OPT} -Wl,--gc-sections -nostartfiles -Wl,-Map=${PROJECT_NAME}.map -T ${CH32F207_LINKER_SCRIPT} -lc -lm"
    )

    # newlib/libgcc's thumb multilib objects are built with -fshort-enums and
    # lack .note.GNU-stack, so a normal link spews benign warnings. Silence
    # them (the sizes match the ARM EABI defaults our objects use):
    set_property(TARGET ${TGT} APPEND_STRING PROPERTY
        LINK_FLAGS " -Wl,--no-enum-size-warning -Wl,--no-wchar-size-warning -Wl,--no-warn-execstack")

    # Link-time optimization (optional). GCC LTO loses the newlib syscall-stub
    # definitions in syscalls.c, so compile that one file without LTO.
    if(STM32_LTO)
        target_compile_options(${TGT} PRIVATE -flto)
        set_property(TARGET ${TGT} APPEND_STRING PROPERTY LINK_FLAGS " -flto")
        set_source_files_properties(${BOARD_DIR}/syscalls.c PROPERTIES
            COMPILE_OPTIONS "-fno-lto")
    endif()
endfunction()
