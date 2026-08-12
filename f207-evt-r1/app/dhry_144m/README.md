# Dhrystone 2.1 @ 144 MHz — CH32F207VCT6 (f207-evt-r1)

Classic Dhrystone 2.1 (dhry_1.c / dhry_2.c / dhry.h), **1,000,000 runs**, on
the f207-evt-r1 board (CH32F207VCT6) clocked at **144 MHz** (HSE 8 MHz → PLL
×18 → SYSCLK, HCLK 144 MHz, APB1 72 MHz, APB2 144 MHz — see
`board/board.c`). Built with **GNU arm-none-eabi-gcc** + CMake + Ninja.

## Results (measured on hardware, 144 MHz, GCC 15.3.1)

| Toolchain    | Flags                              | Dhrystones/s | DMIPS/MHz |
| ------------ | ---------------------------------- | ------------ | --------- |
| GCC 15.3.1   | `-Ofast -funroll-loops`            | 334,112      | 1.321      |

All builds print correct final values (Int_Glob=5, Arr_2_Glob = runs+10, …)
and each run exceeds the 2 s `Too_Small_Time` gate (~3.0 s at 144 MHz).

> ⚠ **Do not use LTO for Dhrystone.** GCC `-flto` sees the whole program and
> hoists loop-invariant work out of the timed loop, inflating the score. The
> LTO number is meaningless and is **excluded from the table above** (a known
> GCC artifact, not a real measurement).

## Build & flash

```bash
bash build.sh              # == cmake -G Ninja .. && ninja (GNU arm-none-eabi-gcc)
ninja flash                # WCH OpenOCD + WCH-Link (CMSIS-DAP) over SWD
ninja bin                  # optional raw .bin image
```

Open the USART1 console (`COM18` @ 115200 via the WCH-Link SERIAL). The
console prints the Dhrystones/s and DMIPS/MHz lines every ~13 s (3.0 s run +
10 s pause); capture a bit longer than one full cycle for a clean result line.

## Notes

* **SysTick**: the shared board layer (`board/board.c`) defines
  `SysTick_Handler` → tick counter, and `HAL_GetTick()` is the Dhrystone
  timer (1 ms resolution, `HZ` = `configTICK_RATE_HZ` = 1000).
* **RUN_NUMBER**: 1,000,000 — at 144 MHz a run takes ~3.0 s, comfortably above
  the 2 s `Too_Small_Time` gate.
* **Do not use LTO for Dhrystone** (see above).
* Console: USART1 (PA9/PA10) → WCH-Link SERIAL (`COM18` @ 115200).
