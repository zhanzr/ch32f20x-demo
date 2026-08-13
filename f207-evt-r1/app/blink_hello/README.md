# blink_hello �?CH32F207VCT6 (f207-evt-r1) template

Template for the f207-evt-r1 board: blinks **LED1 on PA0** (low active, wired
with an external wire) and samples the **ADC1 internal channels** �?the die
temperature sensor (IN16) and VREFINT (IN17) �?converting them to temperature
and VDDA, printed once per 500 ms toggle over the **USART1 console**
(PA9/PA10 �?WCH-Link SERIAL, `COMxx` @ 115200 8-N-1).

> The on-board LEDs of the f207-evt-r1 are **not** connected to the MCU on the
> PCB; for this demo LED1 is wired to **PA0 with an external wire** (LOW
> active: `Bit_RESET` = ON).

All of clock init (144 MHz PLL from the 8 MHz HSE), UART console, SysTick
tick and the newlib stubs come from the shared board layer (`board/` +
`cmake/` at the f207-evt-r1 root).

## ADC internal channels

The CH32F20x ADC has **16 external + 2 internal** signal sources. Both
internal channels are gated by the **TSVREFE** bit in `ADC_CTLR2`:

* **Temperature sensor �?ADC1_IN16.** Output voltage rises linearly with die
  temperature. Datasheet (CH32F207DS0 §4.3.22): `V25 = 1.40 V` typ @ 25 °C,
  `Avg_Slope = 4.3 mV/°C` typ (min/max 3.8/4.8), recommended sample time
  **17.1 µs** (239.5 cycles @ 14 MHz).
* **VREFINT �?ADC1_IN17.** Internal 1.20 V reference, used to back-calculate
  `VDDA` (`VDDA = VREFINT_typ × 4096 / raw`).

The sample is taken with `ADC_SampleTime_239Cycles5` and averaged 10×, then
offset-corrected with the ADC calibration value (`Get_CalibrationValue`).

### Factory calibration (do not use the typical V25)

The datasheet's **typical** `V25 = 1.40 V` has a wide per-chip scatter
(1.34–1.46 V), which alone would put the reading off by up to ~14 °C. Each
chip is therefore factory-calibrated: the info ROM word at **`0x1FFFF720`**
holds the temperature-sensor voltage in mV (`bits[15:0]`) at a factory
reference temperature in °C (`bits[31:16]`) - the same word the SPL helper
`TempSensor_Volt_To_Temper()` reads. The conversion becomes:

```
T (°C) = Refer_Temper - (VSENSE_mV - Refer_Volt) / 4.3
```

with `VSENSE_mV = raw × VDDA / 4096` using the measured (VREFINT-derived)
`VDDA`. On this board `Refer_Volt = 1394 mV @ 29 °C`.

> Note on the ADC clock: at the 144 MHz system clock, PCLK2 = 144 MHz and the
> only prescaler options are /2 /4 /6 /8, so `RCC_PCLK2_Div8` gives 18 MHz �?> above the datasheet 14 MHz max. This is a CH32F20x register limitation;
> WCH's own EVT examples run 12 MHz at 96 MHz. The demo still reads a
> sensible temperature (verified on hardware).

## Build & flash

```bash
bash build.sh              # CMake + Ninja + GNU arm-none-eabi-gcc
ninja flash                # WCH OpenOCD + WCH-Link (CMSIS-DAP mode) over SWD
ninja bin                  # optional raw .bin image
```

## Expected console output (verified on hardware)

```
=== blink_hello on CH32F207VCT6 @ 144000000 Hz ===
ADC internal channels: temperature (IN16) + VREFINT (IN17)
Calibration value: 6
Temp sensor cal: Refer_Volt=1394 mV @ Refer_Temper=29 C
[1] LED1 ON  | T= 31.4 C  VDDA=3.301 V  (IN16 raw 1717, VREFINT raw 1489)
[2] LED1 OFF | T= 31.4 C  VDDA=3.301 V  (IN16 raw 1717, VREFINT raw 1489)
...
```

`144000000 Hz` is the HSE 8 MHz �?PLL ×18 system clock (`SystemCoreClock`).
