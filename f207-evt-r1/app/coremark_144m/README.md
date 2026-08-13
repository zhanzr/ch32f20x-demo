# CoreMark 1.0 @ 144 MHz â€?CH32F207VCT6 (f207-evt-r1)

EEMBC CoreMark 1.0 (stock `coremark_1_0_1` sources), **3,000 iterations**, on
the f207-evt-r1 board (CH32F207VCT6) clocked at **144 MHz** (HSE 8 MHz â†?PLL
Ã—18 â†?SYSCLK, HCLK 144 MHz, APB1 72 MHz, APB2 144 MHz â€?see
`board/board.c`). Built with **GNU arm-none-eabi-gcc** + CMake + Ninja.

## Results (measured on hardware, 144 MHz, GCC 15.3.1)

| Toolchain  | Flags                              | CoreMark 1.0 | Iterations/s | Total time |
| ---------- | ---------------------------------- | ------------ | ------------ | ---------- |
| GCC 15.3.1 | `-Ofast -funroll-loops`            | 381.13       | 381.13       | 10.50 s    |

The build prints **`Correct operation validated.`** with the expected CRCs
(seedcrc 0xe9f5, crcfinal 0x25b5).

## Build & flash

```bash
bash build.sh              # == cmake -G Ninja .. && ninja (GNU arm-none-eabi-gcc)
ninja flash                # WCH OpenOCD + WCH-Link (CMSIS-DAP) over SWD
ninja bin                  # optional raw .bin image
```

Open the USART1 console (`COMxx` @ 115200 via the WCH-Link SERIAL). Capture
at least ~12 s so one full (~11 s) run completes and the final
`CoreMark 1.0 : <score> / <compiler> / Static` line is printed.

## Notes

* **SysTick**: the shared board layer (`board/board.c`) defines
  `SysTick_Handler` â†?1 ms tick counter; `HAL_GetTick()` is the CORE_TICKS
  timer.
* **ITERATIONS**: 4,000 â€?at 144 MHz a run takes ~10.5 s, valid (CoreMark
  rejects runs shorter than 10 s; 3,000 only gave 7.87 s). The
  list/matrix/state CRCs are iteration-independent, so the reference
  validation (`Correct operation validated.`) still passes.
* Port uses `SEED_VOLATILE` (fixed volatile seeds, so the known-CRC validation
  still matches), `MEM_LOCATION "Static"`, `HAS_FLOAT 1`.
* Console: USART1 (PA9/PA10) â†?WCH-Link SERIAL (`COMxx` @ 115200).
