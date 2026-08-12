# blink_hello — CH32F207VCT6 (f207-evt-r1) template

Template for the f207-evt-r1 board: blinks **LED1 on PA0** (low active) and
prints the current system clock plus the LED state once per 500 ms toggle over
the **USART1 console** (PA9/PA10 → WCH-Link SERIAL, `COM18` @ 115200 8-N-1).

> The on-board LEDs of the f207-evt-r1 are **not** connected to the MCU on the
> PCB; for this demo LED1 is wired to **PA0 with an external wire** (LOW
> active: `Bit_RESET` = ON).

All of clock init (144 MHz PLL from the 8 MHz HSE), UART console, SysTick
delays and the newlib stubs come from the shared board layer (`board/` +
`cmake/` at the f207-evt-r1 root).

## Layout

```
src/main.c      LED (PA0) init + main loop (toggle LED1, print state)
CMakeLists.txt  wires the app to the shared board layer
build.sh        CMake + Ninja + GNU arm-none-eabi-gcc wrapper
```

## Build & flash

```bash
bash build.sh              # CMake + Ninja + GNU arm-none-eabi-gcc
ninja flash                # WCH OpenOCD + WCH-Link (CMSIS-DAP mode) over SWD
ninja bin                  # optional raw .bin image
```

## Expected console output (verified on hardware)

```
=== blink_hello on CH32F207VCT6 @ 144000000 Hz ===
LED1 (PA0, low active, external wire) toggling every 500 ms ...
[1] LED1 ON
[2] LED1 OFF
...
```

`144000000 Hz` is the HSE 8 MHz → PLL ×18 system clock (`SystemCoreClock`).
