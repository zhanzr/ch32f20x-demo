# CH32F207VCT6 flash layout — 480 KB physical die, R0WAIT zones

> Reference notes (originally from a colleague) on why the "256 KB" CH32F207VCT6
> actually contains more Flash than its marketing part number suggests, and
> how to use the extra storage.

The datasheet note exists because the physical silicon die inside the
**CH32F207VCT6 actually contains 480 KB of Flash memory**, even though the
chip is marketed as a 256 KB model.

WCH splits this physical Flash into two performance zones: a
**zero-wait-state zone (`R0WAIT`)** and a **non-zero-wait-state zone**.

## Key architectural reasons

### 1. Zero-wait (`R0WAIT`) vs. non-zero-wait memory

The CH32F207 CPU runs at speeds up to **144 MHz**. Physical NOR Flash memory
cannot read bytes at 144 MHz without introducing delay cycles (wait states).

* **Zero-wait region (`R0WAIT`):** The first *N* kilobytes of Flash (e.g.,
  256 KB) are backed by high-speed hardware prefetch/cache buffers or
  specialized bus routing. The CPU fetches instructions from this region at
  full 144 MHz speed with **0 wait states**.
* **Non-zero-wait region:** The remaining memory beyond `R0WAIT` up to 480 KB
  (480 KB − R0WAIT) is physically accessible, but CPU fetches incur extra
  clock cycle delays (wait states), reducing execution speed.

### 2. Shared silicon die strategy

To save manufacturing costs, WCH manufactures a single silicon wafer (a 480 KB
Flash + 64 KB SRAM die) and uses it across multiple part numbers in the
CH32F20x / CH32V30x families.

Rather than permanently disabling the extra 224 KB of Flash
(480 KB − 256 KB) on 256 KB-rated chips, WCH leaves it accessible as
"extended non-zero-wait Flash" for developers who need extra storage space.

### 3. Configurable option bytes (`R0WAIT` tuning)

WCH allows developers to reconfigure the `R0WAIT` allocation using **User
Option Bytes** (via WCHISPTool, WCH-Link Utility, or code):

* You can configure `R0WAIT` to sizes like 192 KB, 224 KB, or 256 KB.
* The formula `480 KB − R0WAIT` calculates how much extended non-zero-wait
  storage remains based on your selected `R0WAIT` configuration.

## How to use this in practice

For embedded development on the CH32F207VCT6:

1. **High-performance code (`.text`):** Place critical interrupts, time-
   sensitive DSP routines, and core logic in the first 256 KB (`R0WAIT` area)
   so the CPU executes at maximum clock speed.
2. **Static resources / web assets (`.rodata`):** Place large binary arrays,
   web server static files (like gzipped HTML/CSS/JS arrays), image assets,
   or lookup tables in the extended non-zero-wait Flash region
   (256 KB → 480 KB). Reads from `.rodata` for network transfers or LCD
   updates are rarely bottlenecked by CPU Flash wait states.

## Quick reference

| Region            | Address range           | Wait states | Typical use            |
| ----------------- | ----------------------- | ----------- | ---------------------- |
| `R0WAIT` (0-wait) | `0x08000000` .. `R0WAIT`| 0           | `.text`, ISRs, hot loops |
| Extended (slower) | `R0WAIT` .. `0x08077FFF` (480 KB end) | > 0 | `.rodata`, assets, LUTs |
