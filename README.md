pi-pico-scope
=============

A port of [pillScopePlus](https://github.com/tvlad1234/pillScopePlus) from the
STM32F401 "Black Pill" to the **Raspberry Pi Pico (RP2040)**, driving an
**ST7789 240x240 SPI TFT**.

The oscilloscope logic (triggering, waveform measurements, graticule, UI, and the
TekScope serial formats) is derived from the original project. The hardware layer,
ADC timebase and display driver were rewritten for RP2040.

Hardware
--------

### Display (ST7789 240x240, SPI)

| Pico pin | GP  | Signal        | Notes                                    |
|----------|-----|---------------|------------------------------------------|
| 1        | 0   | LEDA          | Backlight enable, driven HIGH at boot    |
| 2        | 1   | CS            | Chip select, active low                  |
| 4        | 2   | DC            | Data / command                           |
| 5        | 3   | RST           | Hardware reset, active low               |
| 9        | 6   | SCK           | SPI0 clock                               |
| 10       | 7   | MOSI          | SPI0 data                                |
| 8        | -   | LEDK          | Backlight cathode                        |
| 3        | -   | GND           | Common ground                            |
| 40       | -   | 5V (VBUS)     | Module has its own 3.3V regulator         |

SPI0 runs at 20 MHz (lowered from 40 MHz to eliminate the colour glitches you
observed), SPI mode 0, and the driver waits 10 ms after each full-screen flush
because of the colour-transition issue you found.

### Scope hardware

| Function        | GP  | Notes                                        |
|-----------------|-----|----------------------------------------------|
| Analog input    | 26  | ADC channel 0, 0-3.3 V                       |
| Up button       | 15  | Internal pull-up, active low                 |
| Select button   | 16  | Internal pull-up, active low                 |
| Down button     | 17  | Internal pull-up, active low                 |
| Onboard LED     | 25  | Lit while the trace is triggered             |
| UART TX         | 4   | 9600 8N1, captured waveforms                 |
| UART RX         | 5   | 9600 8N1, command input                      |

**You still need the analog frontend.** The RP2040 ADC can only measure
0 - 3.3 V, so the input must be attenuated by 2x and biased to ~1.65 V, exactly
as in the original (two 68k resistors for the reference, two 500k for the
attenuator, LM358 buffer). Feed the frontend output to GP26, and remember the
scope and the device under test must **not** share a ground reference.

Building
--------

```bash
export PICO_SDK_PATH=/path/to/pico-sdk
cmake -S . -B build
cmake --build build -j4
```

Output: `build/pi-pico-scope.uf2` — hold BOOTSEL, plug the Pico in, copy the file
to the `RPI-RP2` drive.

Requires `gcc-arm-none-eabi` and CMake 3.13+. Builds warning-free at
~73 KB flash / ~117 KB RAM (the 240x240 RGB565 framebuffer is 115 KB of that).

Using the scope
---------------

* **Select** cycles through Vdiv, Trig, Slope, Atten, and timebase.
* **Up** / **Down** adjust the selected parameter.
* **Up + Down** together enter auto-calibration (couple the probe to ground
  first). **All three** together resets the device.

Serial commands, on UART1 at 9600 baud:

* `s` - CSV of the capture, for the Tektronix TekScope app
* `S` - full raw capture for the companion ingest app
* `F` - half-buffer capture (faster) for the companion ingest app

The companion app is at https://github.com/tvlad1234/tekscopeIngest and should
work unchanged, since the wire format is preserved.

Differences from the STM32 original
-----------------------------------

**Sample timebase.** The original drove the ADC from a hardware timer through
DMA. RP2040 has no ADC trigger input, but this project uses the chip's hardware
**free-run mode** with a programmed clock divider (`adc_run` + `adc_set_clkdiv`),
so samples are paced in hardware and pushed into RAM by DMA with **zero CPU
involvement** during capture. `scope.c` reads back the achieved rate from the ADC
clock and uses it for the timing measurements, so frequency readings stay honest
even when the requested rate rounds imperfectly.

Maximum rate is about **1.0 MSa/s** (96 ADC clocks is the minimum conversion
period, giving ~1.3 MSa/s of headroom on a 125 MHz ADC clock), versus 1.6 MSa/s
on the STM32. The fastest timebase is therefore **20 us/div** rather than
10 us/div, and the timebase follows the usual 1-2-5 sequence:

```
20, 50, 100, 250, 500, 1000, 2500, 5000, 10000 us/div
```

**Display.** ST7735 128x160 (colour) became ST7789 240x240 (colour). The framebuffer
is unchanged in design - gfx primitives draw into RAM and the whole buffer is
pushed over SPI each frame. Layout was redesigned for the square panel:

```
+--------------------+---------+
|                    |  Min    |
|   trace area       |  Max    |
|   160 x 160        |  Ppk    |
|   8 x 8 divisions  |  Freq   |
|                    |  Trig   |
+--------------------+---------+
| Vdiv  Trig  Slope  Atten  us/d  |
| 2.0V  0.0  Rise   1x    20     |
| Sample rate: 1.0 MSa/s          |
+--------------------------------+
```

Because `gfx.c` drew one pixel too many in `drawFastHLine`/`drawFastVLine` in the
original, those were corrected, and `drawBitmap` now takes an explicit stride so
bitmaps of one width can be drawn on a framebuffer of another.

**Console output.** The UI no longer uses newlib `printf`; integers go through
`printInt` so nothing has to be routed to a `_write` handler. `sprintf` is still
used for the UART formats.

**USB CDC** was already unused upstream and has not been ported; the UART path
is the only data output.

Things to check on hardware
---------------------------

These cannot be verified without the physical panel, and are the most likely
places to need adjustment:

1. **ST7789 init sequence** in `src/panel_st7789.c`. The register values follow
   the common Waveshare/LilyGO sequence for a 240x240 ST7789. If the screen is
   blank, dim, or the colours are wrong, this is the first thing to change.
2. **MADCTL orientation** (`MADCTL_VALUE` in the same file). Set to `0x00`. If
   the image is rotated or mirrored, try `0x60`, `0xC0`, or `0xA0`. Some modules
   need `MADCTL_BGR` (0x08) added for correct colour order.
3. **ST7789 is not the panel you first mentioned.** The Nokia 105 2G 2023
   (TA-1557) screen is a 128x64 reflective monochrome panel with a raw FPC
   connector and an unknown controller. Driving it needs its pinout and its init
   sequence, and the UI would need a monochrome layout. All display access goes
   through `src/panel.h` (`panelInit`, `panelFlush`, `drawPixel`) specifically so
   such a driver can be added later without touching `gfx.c`, `wave.c` or `ui.c`.

Licensing
---------

`src/scope.c`, `src/wave.c`, `src/ui.c`, `src/gfx.c`, `src/font.h`, `src/splash.h`
and the boot logo are derived from **pillScopePlus**, which is licensed under the
**Apache License 2.0**. See `LICENSE-APACHE-2.0` and `NOTICE`. Those files cannot
be relicensed as MIT; if you want a single licence for the whole repository,
Apache-2.0 is the safe choice.