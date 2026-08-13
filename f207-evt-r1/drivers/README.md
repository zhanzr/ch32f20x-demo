# Vendored drivers

The WCH CH32F20x StdPeriphDriver (SPL) + CMSIS, taken verbatim from the Keil
pack / WCH EVT so no Keil MDK runtime is ever needed to build these projects:

| Source | Path in this repo |
| ------ | ----------------- |
| Keil `WCH32F2xx_DFP` 1.0.3 `Device/Include` | `CH32F20x/Include/` (`ch32f20x.h`, `system_ch32f20x.h`) |
| Keil `WCH32F2xx_DFP` 1.0.3 `Device/Source` | `CH32F20x/Source/` (`system_ch32f20x.c`) |
| Keil `WCH32F2xx_DFP` 1.0.3 `Device/StdPeriph_Driver/{inc,src}` | `CH32F20x/StdPeriphDriver/{inc,src}/` |
| WCH EVT `EXAM/SRC/CMSIS` (`core_cm3.h`, `core_cm3.c`) | `CMSIS/` |

## Local patches (only two, both toolchain-only)

1. **`CH32F20x/Source/system_ch32f20x.c`** — the internal
   `#define SYSCLK_FREQ_96MHz_HSE` (and siblings) are commented out so the
   system-clock frequency is chosen purely by the build
   (`cmake/ch32f207_board.cmake` passes `-DSYSCLK_FREQ_144MHz_HSE=144000000`).
   No functional change; it just moves the WCH "uncomment one of these"
   selection to the command line.

2. **`CMSIS/core_cm3.c`** — the three `strex{b,h,w}` inline-assembly
   intrinsics use `"=&r"` for the result register instead of `"=r"`. The Arm
   ABI forbids `strex` source == destination, which newer GNU assemblers
   (GCC 15) reject. This is the standard CMSIS/GCC fix; behaviour is
   identical.
