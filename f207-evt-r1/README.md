# f207-evt-r1 — WCH CH32F20x-EVT-R1 (CH32F207VCT6)

## Board

* MCU: **CH32F207VCT6** (LQFP100, 256 KB flash, 64 KB SRAM, 144 MHz max,
  Cortex-M3).
* HSE: 8 MHz external crystal.
* LEDs: on-board LED1/LED2 are **not connected to the MCU** on this PCB —
  wired externally as needed. This repo's `blink_hello` uses **LED1 on PA0**
  (low active) through an external wire.
* USART1 console: **PA9 (TX) / PA10 (RX)**, 115200 8-N-1 → the on-board
  WCH-Link SERIAL port (`COM18` on this PC).
* Debug/flash probe: the on-board **WCH-Link** runs **CMSIS-DAP firmware**
  (VID:PID `1a86:8012`, SWD), so it is driven by the WCH OpenOCD build that
  ships the `wch_arm` flash driver — no Keil MDK required.

## Clock tree (144 MHz)

```
HSE 8 MHz → PLL (x18) → SYSCLK 144 MHz
  AHB  /1 → HCLK 144 MHz
  APB1 /2 → PCLK1  72 MHz
  APB2 /1 → PCLK2 144 MHz
```

Configured by `SystemInit()` in `drivers/CH32F20x/Source/system_ch32f20x.c`
(called from the startup file), enabled by the `SYSCLK_FREQ_144MHz_HSE` define
that `cmake/ch32f207_board.cmake` passes.

## Console

USART1 on PA9/PA10 at 115200 8-N-1, wired to the WCH-Link SERIAL port.
`printf()` reaches it through the shared board layer's `syscalls.c:_write()` →
`UART_PutChar()` → SPL `USART_SendData` (blocking polled).

## Repository layout

```
f207-evt-r1/
├── README.md       ← this file (board hardware + project list)
├── app/            ← per-project sources (blink_hello, ...)
├── board/          ← shared board layer (GCC startup, linker, UART, syscalls)
├── cmake/          ← shared toolchain / board / flash CMake modules
├── drivers/        ← vendored CH32F20x StdPeriphDriver + CMSIS (from the
│                     Keil WCH32F2xx_DFP 1.0.3 pack / WCH EVT, no Keil runtime);
│                     see drivers/README.md
└── scripts/        ← WCH OpenOCD board config for the WCH-Link
```

## Projects

| Project       | What it is                               |
| ------------- | ---------------------------------------- |
| `blink_hello` | LED1 (PA0, external wire) blink + USART1 console print |

## Build & flash

Each project has `build.sh`; the common workflow lives in the repository root
`../README.md`:

```bash
cd app/<project>
bash build.sh          # CMake + Ninja + GNU arm-none-eabi-gcc
ninja flash            # WCH OpenOCD + WCH-Link (CMSIS-DAP) over SWD
```
