# f207-evt-r1 board layer

Shared board support for every app in this repository (WCH CH32F20x-EVT-R1,
CH32F207VCT6). Each project's `CMakeLists.txt` attaches this layer plus the
WCH CH32F20x StdPeriphDriver sources with one call:

```cmake
include(${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/ch32f207_board.cmake)
ch32f207_apply_board(${PROJECT_NAME}.elf "-O2")
```

## What the layer provides

| File                      | Purpose                                                        |
| ------------------------- | -------------------------------------------------------------- |
| `board.c` / `board.h`     | `Board_Init()`, `SystemClock_Config()`, 1 ms SysTick tick (`HAL_GetTick`/`HAL_Delay`), `Delay_Us`, `Error_Handler()` |
| `uart_printf.c` / `.h`    | USART1 console init + blocking `UART_PutChar` / `UART_GetChar` |
| `syscalls.c`              | newlib retarget: `_write` → USART1, `_sbrk`, `_fstat`, ...      |
| `startup_ch32f20x_D8C.S`  | CMSIS GCC startup for CH32F205/207 (D8C) vector table, GNU as syntax |
| `ch32f207vc.ld`           | GNU ld script: 256 KB flash @ 0x08000000, 64 KB RAM @ 0x20000000 |
| `ch32f20x_conf.h`         | SPL module selection (GPIO/RCC/USART/MISC/...)                  |

`cmake/ch32f207_board.cmake` (one level up) wires these together with the
CH32F20x SPL sources, include paths and the `CH32F20x_D8C` /
`SYSCLK_FREQ_144MHz_HSE` defines. The SPL + CMSIS files themselves live in
`../drivers/` — see `../drivers/README.md` for their provenance and the two
toolchain-only local patches (system clock select + core_cm3.c STREX fix).

## Clock

144 MHz from the 8 MHz HSE via PLL ×18: HCLK = 144 MHz, PCLK1 = 72 MHz
(APB1 /2), PCLK2 = 144 MHz (APB2 /1, USART1 is on APB2).

`SystemInit()` (from `drivers/CH32F20x/Source/system_ch32f20x.c`, called by
the startup file before `main`) programs the PLL; `SystemClock_Config()` in
board.c only refreshes `SystemCoreClock`.

## Console

USART1 on **PA9 (TX) / PA10 (RX)**, 115200 8-N-1, wired to the on-board
WCH-Link SERIAL (`COM18` on this PC). `printf()` output reaches it through
`syscalls.c:_write()` → `UART_PutChar()` → `USART_SendData` (blocking).

## Adding a new app

Copy `../app/blink_hello` as a template, or start from:

```cmake
cmake_minimum_required(VERSION 3.13)
if(NOT CMAKE_TOOLCHAIN_FILE)
    set(CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/arm-none-eabi-toolchain.cmake")
endif()
project(my_app C ASM)
add_executable(my_app.elf src/main.c)
include(${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/ch32f207_board.cmake)
ch32f207_apply_board(my_app.elf "-O2")
include(${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/objcopy-targets.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/flash-targets.cmake)
```

`main.c` calls `Board_Init()` first (SystemCoreClock + UART + delay), then the
app code.
