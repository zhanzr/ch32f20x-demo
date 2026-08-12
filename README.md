# ch32f207-demo

Bare-metal CH32F207 demo/template projects, built with a **pure GCC + CLI**
toolchain — no Keil, no vendor SPL glue. Each board directory holds its own
applications plus a shared SPL-based board layer.

## Boards

| Board | MCU | Core | Clock | RAM | Flash | Debug probe |
| ----- | --- | ---- | ----- | --- | ----- | ----------- |
| f207-evt-r1 (WCH CH32F20x-EVT-R1) | CH32F207VCT6 | Cortex-M3 | 144 MHz | 64 KB | 256 KB | on-board WCH-Link (CMSIS-DAP) |

See `f207-evt-r1/README.md` for the board's hardware details and its project list.

## Toolchain

* Compiler: GNU arm-none-eabi-gcc (any recent toolchain; tested with the Arm
  GNU Toolchain 15.3.1).
* Build: CMake + Ninja (each project has a `build.sh` wrapper).
* Drivers: the WCH **CH32F20x StdPeriphDriver** + CMSIS vendored from the
  Keil `WCH32F2xx_DFP` 1.0.3 pack (`D:\Arm\Packs\Keil\WCH32F2xx_DFP`) into
  `<board>/drivers/` — the Keil MDK runtime is **not** used.
* Flashing: **WCH OpenOCD** (the build bundled with MounRiver Studio, which
  ships the `wch_arm` flash driver) driving the on-board **WCH-Link** in
  CMSIS-DAP mode over SWD.

## Build & flash

Same flow for every project:

```bash
cd f207-evt-r1/app/<project>
bash build.sh          # == cmake -G Ninja .. && ninja   (produces .elf + .hex)
ninja flash            # WCH OpenOCD + WCH-Link (CMSIS-DAP) over SWD
```

Then open the project's USART1 console (115200 8-N-1). On f207-evt-r1 the
console is `COM18` (WCH-Link SERIAL).
