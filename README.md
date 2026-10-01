pi-pico-scope
=============

A port of [pillScopePlus](https://github.com/tvlad1234/pillScopePlus) from the
STM32F401 "Black Pill" to the **Raspberry Pi Pico (RP2040)**, driving an
**ST7735 128x160 SPI TFT** in landscape.

The oscilloscope logic (triggering, waveform measurements, graticule, UI, and
the TekScope serial formats) is derived from the original project. The hardware
layer, ADC timebase and display driver were written for RP2040.

Hardware
--------

### Display (ST7735 128x160, SPI, driven landscape 160x128)

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

SPI0 runs at 20 MHz (`LCD_SPI_BAUD_HZ`) in mode 0, lowered from 40 MHz to
eliminate colour glitches. Pixels are pushed over DMA channel 0, MSB first, and
CS is toggled per command/parameter rather than held low across a sequence.

The panel is addressed in **landscape**: `LCD_W 160` / `LCD_H 128`, so the
128x160 portrait panel is rotated rather than re-plumbed. `panelInit()` runs the
standard ST7735 sequence (`SWRESET`, `SLPOUT`, frame-rate/power/gamma tables,
`INVOFF`, `NORON`, `DISPON`), sets `COLMOD = 0x05` for 16-bit RGB565, applies
`MADCTL = 0xC8` for the portrait pass, then switches to `landscapeMadctl`
(`0xA0`) and sets the address window to `0,0,159,127`.

> `LCD_FLUSH_SETTLE_MS` in `src/bsp.h` is **0**, so `panelFlush()` does not pause
> between frames. The 10 ms post-flush settle delay that used to work around a
> colour-transition artefact has been removed; that constant is the knob to turn
> if the artefact returns.

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

Output: `build/pi-pico-scope.uf2` - hold BOOTSEL, plug the Pico in, copy the file
to the `RPI-RP2` drive. It unmounts on its own and the Pico reboots.

Requires `gcc-arm-none-eabi` and CMake 3.13+. Builds warning-free at
~88 KB flash / ~44 KB RAM (the 160x128 RGB565 framebuffer is 40 KB of that).

The TinyUSB submodule must be initialised (`git submodule update --init --depth 1
lib/tinyusb` in pico-sdk) or the USB CDC boot log will fail to link.

CMake options for isolating hardware problems, all `OFF` by default:

| Option                                  | Effect                                  |
|-----------------------------------------|-----------------------------------------|
| `PI_PICO_SCOPE_PANEL_PROBE`             | Run the display probe at boot, no UI    |
| `PI_PICO_SCOPE_SLOW_SPI_PROBE`          | 250 kHz software SPI for the probe      |
| `PI_PICO_SCOPE_REFERENCE_SPI_PROBE`     | 20 MHz hardware SPI0 for the probe      |
| `PI_PICO_SCOPE_CONSTANT_COLOR_TEST`     | Minimal reference colour loop           |

Using the scope
---------------

* **Select** cycles through V, T, S, A, D - V/div, trigger level, slope,
  attenuation, timebase.
* **Up** / **Down** adjust the selected parameter.
* **Up + Down** together enter auto-calibration (couple the probe to ground
  first). **All three** together resets the device.

Serial commands, on UART1 at 9600 baud:

* `s` - CSV of the capture, for the Tektronix TekScope app
* `S` - full raw capture for the companion ingest app
* `F` - half-buffer capture (faster) for the companion ingest app

The companion app is at https://github.com/tvlad1234/tekscopeIngest and should
work unchanged, since the wire format is preserved.

Screen layout
-------------

Plot geometry comes from `src/scope.h` (`PIXDIV 16`, `XDIV 8`, `YDIV 6`), giving
a 128 x 96 trace area with 8 x 6 divisions, a stats menu at `MENU_X` (= 128) and
the settings bar at `BAR_Y` (= 96):

```
+---------------+-------+
|               | Min   |
|  trace area   | Max   |
|  128 x 96     | Ppk   |
|  8 x 6 divs   | Freq  |
|               | Trig  |
+---------------+-------+
| V    T    S    A    D  |      BAR_Y + 1
| 2.0V 0.0V Rise 1x  20u  |      BAR_Y + 11
| U/D edit SEL next       |      BAR_Y + 22
+------------------------+
```

The bar labels are single letters with the value under each at a fixed 32 px
pitch, drawn entirely at **1x font**. An earlier revision used `setTextSize(2)`
for the value row; that was a debugging change, since reverted to
pillScopePlus's native 1x layout.

Because `gfx.c` drew one pixel too many in `drawFastHLine`/`drawFastVLine` in the
original, those were corrected, and `drawBitmap` now takes an explicit stride so
bitmaps of one width can be drawn on a framebuffer of another.

Console output
--------------

The UI does not use newlib `printf`; integers go through `printInt` and are drawn
straight into the framebuffer. `sprintf` is still used for the UART formats.
Separately, `printf` goes to **USB CDC** (a second COM port) purely for boot
diagnostics, so on-screen text is unaffected. USB CDC was unused upstream and
has not been ported; UART is the only data output.

At boot the firmware prints a log to USB CDC and runs `panelSelfTest()`, which
flashes six full-screen colour bars (red, green, blue, white, black, yellow) for
500 ms each while blinking the onboard LED. Seeing the bars means SPI and the
init sequence work and any remaining problem is in the UI; staying white means
the panel never accepted the commands.

Differences from the STM32 original
-----------------------------------

**Sample timebase.** The original drove the ADC from a hardware timer through
DMA. RP2040 has no ADC trigger input, so this project uses the chip's hardware
**free-run mode** with a programmed clock divider (`adc_run` +
`adc_set_clkdiv`). Samples are paced in hardware and pushed into RAM by DMA on
channel 1 with zero CPU involvement during capture. `applySampleRate()` reads the
achieved rate back from the ADC clock and derives `sampPer` from it, so timing
measurements stay honest even when the requested rate rounds imperfectly.

`ADC_MIN_PERIOD_TICKS` is 96, the minimum conversion period, which gives about
1.3 MSa/s of headroom on a 125 MHz ADC clock. Maximum rate is therefore about
**1.0 MSa/s**, versus 1.6 MSa/s on the STM32, so the fastest timebase is
**20 us/div** rather than 10 us/div. The timebase follows the usual 1-2-5
sequence:

```
20, 50, 100, 250, 500, 1000, 2500, 5000, 10000 us/div
```

`BUFFER_LEN` is 256 samples - two screens' worth at `PLOT_W`, so the trigger
search can look ahead of the point it finds.

**Display.** ST7735 128x160 in portrait became ST7735 128x160 driven in
landscape, and the framebuffer is 40 KB rather than 115 KB. All display access
goes through `src/panel.h` (`panelInit`, `panelFlush`, `panelSelfTest`,
`panelProbe`) so another controller can be added later without touching `gfx.c`,
`wave.c` or `ui.c`.

Known issues
------------

1. **`src/panel_st7789.c` is misnamed** - it is an ST7735 driver. Left alone to
   avoid touching a working display build; renaming is a mechanical follow-up
   that needs a matching edit to `CMakeLists.txt`.
2. **`printFreq()` in `src/ui.c` is dead code** - a `setTextSize(2)` frequency
   readout that is never called.

If you change `PIXDIV`, `XDIV` or `YDIV` in `src/scope.h`, keep
`PLOT_W + MENU_W <= LCD_W` and `PLOT_H + BAR_H <= LCD_H` or the menu and bar run
off the edge. `ui.c` also hardcodes the bar row offsets (`BAR_Y + 1`, `+ 11`,
`+ 22`) and the 32 px column pitch, so widening the trace area needs a matching
pass over those.

Things to check on hardware
---------------------------

These cannot be verified without the physical panel and are the most likely
places to need adjustment:

1. **ST7735 init sequence** in `src/panel_st7789.c`. The register values follow
   the common Waveshare/LilyGO ST7735 sequence. If the screen is blank, dim, or
   the colours are wrong, this is the first thing to change.
2. **MADCTL orientation** - the symbols are `portraitMadctl` (`0xC8`) and
   `landscapeMadctl` (`0xA0`, via `ST7735_BLACKTAB_ROTATION_1`). If the image is
   rotated or mirrored, change `landscapeMadctl` to `0xC0`, `0x60` or `0x00` and
   swap the address window that follows it to match. Rotation values change
   orientation only - they cannot fix an all-white panel. For wrong colour order,
   add the BGR bit, e.g. `0xA8`.
3. **SPI mode** is CPOL=0/CPHA=0 by default; `bspLcdSetSpiMode(1)` selects mode 3.
   If the panel stays white with correct wiring and a known-good init sequence,
   mode 3 is the next thing to try - some modules are sensitive to it.
4. **Trigger hysteresis.** `TRIG_HYST` in `src/wave.c` must exceed the noise on
   the analog input without exceeding the signal's downward excursion from the
   trigger level. Too small and the trace jitters; too large and the trigger
   never re-arms and the frequency reading drops to 0.

Licensing
---------

`src/scope.c`, `src/wave.c`, `src/ui.c`, `src/gfx.c`, `src/font.h`, `src/splash.h`
and the boot logo are derived from **pillScopePlus**, which is licensed under the
**Apache License 2.0**. See `LICENSE-APACHE-2.0` and `NOTICE`. Those files cannot
be relicensed as MIT; if you want a single licence for the whole repository,
Apache-2.0 is the safe choice.