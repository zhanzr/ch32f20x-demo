# eth_http - embedded web server on the CH32F207VCT6 (f207-evt-r1)

A bare-metal web server over the on-board Ethernet using **lwIP 2.1.2** in
**raw-API / NO_SYS** mode, plus DHCP. It serves the bundled single-page site
(sources in `web/` + `public/`, packed by `build_web.py` into
`Inc/web_assets.h`). The HTTP server is a custom raw-TCP app
(`src/http_server.c`), not lwIP httpd.

The **CH32F207 integrates the Ethernet MAC together with a 10BASE-T PHY
transceiver** - the board's RJ45 connects straight to the chip, there is no
external PHY. The link is therefore **10 Mbit/s only**.

## Pages & API

The site is one page with three tabs (all CSS/JS inlined, gzip-compressed),
adapted from the STM32F769 `e_server` reference for this board's single LED
and two internal ADC channels:

* **LED control** - one checkbox for LED1 (PA0, low active, external wire);
  each click POSTs the new state to `/api/leds` (no page reload) and the
  checkbox re-syncs to the server's reply.
* **ADC values** - two canvas plots (VREFINT -> VDDA and die temperature) with
  a 1/2/4 s sample interval (persisted); samples are polled only while the tab
  is open, and the previous value is kept on timeout.
* **Board info** - architecture, LAN IP, public IP / geo / weather (shown as
  "N/A" on the board - no HTTP/TLS client), plus the board photo.

| Route                | Description                                     |
| -------------------- | ----------------------------------------------- |
| `GET /`              | the page: gzip, `Content-Encoding: gzip`        |
| `GET /api/leds`      | `{"leds":[0]}` (real LED GPIO state)            |
| `POST /api/leds`     | body `{"leds":[0]}` -> applies, `{"leds":[...]}` |
| `GET /api/adc`       | `{"vrefint_mv":..,"temp_c":..,"ts":..}`         |
| `GET /api/info`      | `{"arch","lan_ip","public_ip":null,"geo":null,"weather":null}` |
| `GET /public/*`      | raw image bytes (`image/webp`) from the bundle  |

The ADC measurement is the same as `blink_hello`: the two internal channels
gated by `TSVREFE` (die temperature IN16 + VREFINT IN17), with VDDA
back-calculated from the internal 1.20 V reference and the die temperature
from the per-chip **factory calibration** stored at info-ROM `0x1FFFF720`
(`Refer_Volt` / `Refer_Temper`, slope 4.3 mV/°C) - not the datasheet's wide-
scatter typical V25.

## Ethernet notes

* **Built-in 10BASE-T PHY**: enabled with `EXTEN->EXTEN_CTR |= EXTEN_ETH_10M_EN`
  and clocked by a 60 MHz PLL3 derived from the 8 MHz HSE
  (PREDIV2 /2 -> 4 MHz, x15 -> 60 MHz). The PHY registers are still reached
  over the MAC SMI bus (address 1). The `src/ethernetif.c` link handling
  (auto-negotiation + 10BASE-T MDIX polarity recovery) is a port of the WCH
  EVT `eth_driver_D8C_10M.c`.
* **Only 10 Mbit/s** - plenty for this page/API workload.
* The MAC is started only once the link is up; the ETH DMA interrupt is
  enabled at the same time and recovers the RX path from a receive-buffer
  underrun (a known CH32F20x quirk) by re-initialising the MAC.
* RX is polled in `main()` (`ethernetif_input`), no RTOS required.

## Bundled site (`Inc/web_assets.h`)

The site sources live in `web/` (`index.html`, `style.css`, `app.js`) and
`public/` (the board photo). `build_web.py` inlines the CSS/JS into
`index.html`, gzips it (served with `Content-Encoding: gzip`), and embeds the
photo as a raw array with a `path -> {ctype, data, len}` table. Regenerate
with `python build_web.py`.

## Build & flash

```bash
python build_web.py      # regenerate Inc/web_assets.h after editing web/
bash build.sh            # == cmake -G Ninja .. && ninja
ninja flash              # WCH OpenOCD + WCH-Link (CMSIS-DAP/SWD)
```

Open the USART1 console (`COMxx` @ 115200 via the WCH-Link SERIAL). When the
link is up and DHCP completes you will see the assigned IP; browse to
`http://<ip>/`.

Example console output:

```
=== eth_http on CH32F207VCT6 @ 144000000 Hz ===
HTTP server: http://<dhcp-ip>/  (DHCP enabled)
MAC: 38:3b:26:87:83:e6
ETH: looking for DHCP server ...
ETH: DHCP IP = 192.168.5.49
```

## DHCP through a Windows network bridge

The firmware is a standard DHCP client, so it works on any normal L2 path
(switch/router). If you connect it through a **Windows Network Bridge**
(Wi-Fi + Ethernet bridged on the PC), the board can get an IP from the router
provided the bridge forwards frames to the board's Ethernet member - Windows
bridges sometimes do not, in which case DHCP times out and the static IP
(`192.168.5.200`) is used. To isolate the board from the bridge, plug the
board straight into the router/switch (or give it a static IP on the bridge
subnet and ping it).
