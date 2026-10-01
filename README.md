pi-pico-scope
=============

A **cheap oscilloscope** made from a Raspberry Pi Pico.

An oscilloscope is a tool that draws electricity on a screen, so you can *see*
what a signal looks like. This one plugs into a circuit, listens to the voltage,
and draws it as a moving line.

This project is a copy of an older project called
[pillScopePlus](https://github.com/tvlad1234/pillScopePlus). That one ran on a
different chip (an STM32F401). This version runs on a Raspberry Pi Pico, which
is a tiny, cheap computer on a small circuit board.

Everything works: it draws the signal, measures it, tells you the frequency,
and can save captures to a computer.


What you need
-------------

You must buy these parts. They are not included.

**1. Raspberry Pi Pico** — the little brain. Any version works.

**2. A small screen** — an **ST7735**, which is 128 pixels wide and 160 pixels
tall. This one costs about $4.

**3. Some buttons** — four of them. Two would be hard to use, so we use three:
UP, SELECT, and DOWN.

**4. One LED** — we steal the LED that is already soldered onto the Pico.

**5. Four resistors and an op-amp** for the analog part (called the *frontend*):
two 68k, two 500k, and one LM358. Cheapest to buy, hardest to get right. See
"The one hard bit: resistors and the frontend" below.

> If you only have the Pico and the screen, the screen will light up but will
> always show a flat line, because there is no way to read electricity yet.


Wiring it up
------------

On the Pico, every wire has a number. The numbers are called **pins**, and they
are printed on the board, like `GP26`. Connect things to them like this:

### The screen

| Pico pin | Number | To what       | What it does                    |
|----------|--------|---------------|---------------------------------|
| 1        | GP0    | Backlight +   | Turns the screen's light on     |
| 2        | GP1    | CS            | Says "I'm talking to the screen" |
| 4        | GP2    | DC            | Tells screen data or command    |
| 5        | GP3    | RST           | Resets the screen               |
| 9        | GP6    | SCK           | The clock wire                  |
| 10       | GP7    | MOSI          | The data wire                   |
| 3        | GND    | Ground        | Shared "0 volts"                |
| 40       | 5V     | Power         | Gives the screen electricity    |

Also connect **pin 8** to the screen's **LEDK** (backlight minus).

The screen talks to the Pico using **SPI**. SPI is just a fast way for one chip
to send data to another, using a clock wire to say when each bit goes out.

### The buttons and LED

| What          | Pin | Note                        |
|---------------|-----|-----------------------------|
| Analog input  | GP26| Where the signal comes in   |
| UP button     | GP15|                             |
| SELECT button | GP16|                             |
| DOWN button   | GP17|                             |
| LED           | GP25| Lights up when a signal is found |


The one hard bit: resistors and the frontend
--------------------------------------------

**Short answer: yes, the analog mode needs resistors.** Without them you'll get
a flat line or a badly squashed one, and it isn't a software fault.

### Why you can't just wire the signal straight in

Two reasons, and both are hard limits of the Pico's measuring chip (the **ADC**):

1. **It can only read 0 to 3.3 volts.** It physically cannot see a negative
   voltage — it just reports zero. A normal sine wave sits half above and half
   below zero volts, so you'd see only the top half, squashed flat.
2. **3.3 V is the entire range.** Anything bigger than 3.3 V pegs the reading.

So we need to do two things to the signal before the Pico looks at it: **halve
it** (so it fits) and **lift it up by 1.65 V** (so it sits in the middle of the
range, where it has the most room). That 1.65 V becomes our new "zero".

### What you need

| Part             | Value  | What it does                                     |
|------------------|--------|--------------------------------------------------|
| 2 resistors      | 68k    | Make a **1.65 V** reference — our new "zero"     |
| 2 resistors      | 500k   | **Halve** whatever you are measuring              |
| 1 op-amp         | LM358  | Buffers, so measuring doesn't disturb the circuit |

How it works: the two 68k resistors sit across 3.3 V and make 1.65 V right down
the middle (3.3 x 68/136 = 1.65 V). The two 500k resistors halve your incoming
signal. The LM358 then holds that steady so the Pico doesn't load down whatever
you are measuring.

The software takes care of the rest. It subtracts the 1.65 V and doubles the
result, which puts your real voltage back:

```c
return 2 * (((3.3 * samp) / 4096.0) - offsetVoltage);
```

Check it with the maths: if your signal is **+1 V**, it arrives halved (0.5 V)
and lifted (2.15 V), and the formula gives back **1.0 V**. Correct.

> **After building the frontend, run auto-calibration.** It measures any small
> offset error and remembers the correction. Short the input to ground first,
> then hold **UP + DOWN**.

### Do I need them for logic analyser mode?

**Usually not.** Digital signals are already sitting between 0 and 3.3 V, so
you can wire them straight into **GP26, GP27, GP28 and GP29** and skip the
frontend completely.

Two warnings:

* **Never feed it 5 volts.** The Pico's pins are 3.3 V only and are **not**
  5 V tolerant — 5 V will damage the chip. Use a resistor divider or a level
  shifter for 5 V logic.
* **Put about 1k in series anyway**, just as insurance against a short or a
  mixed-up wire. It costs nothing and it saves the pin.

### ⚠️ Safety

**Do not share a ground between the Pico and the circuit you're probing.**

While the Pico is plugged into USB, its ground is joined to your computer's
ground. If you also join that to a mains-powered circuit, you have connected
your PC to mains through the ground wire. That is genuinely dangerous.

Power the thing you are testing from a battery or an isolated supply while you
probe it.


Making it (building)
--------------------

You turn the code into a file the Pico can run. That file is called a **UF2**
file, and it ends in `.uf2`.

You need two things installed first:

* **CMake** — a tool that helps build projects.
* **gcc-arm-none-eabi** — the compiler for small chips like the Pico.

The Raspberry Pi also publishes free code called the **Pico SDK**, which has all
the low-level instructions for talking to the chip. Download it, then:

```bash
export PICO_SDK_PATH=/path/to/pico-sdk   # point at where you put the SDK
cmake -S . -B build
cmake --build build -j4
```

When it finishes, you get `build/pi-pico-scope.uf2`. That is the file we need.

One extra step, only once: the SDK needs one more piece of free code called
TinyUSB. Inside the SDK folder run:

```bash
git submodule update --init --depth 1 lib/tinyusb
```

Skip this and the build will fail. That is normal, not your fault.


Putting it on the Pico
----------------------

The Pico cannot read files from a computer. Instead, it pretends to be a
tiny USB stick.

1. **Hold down the BOOTSEL button** on the Pico. This is the little button near
   the USB plug.
2. **Plug the Pico into the computer** — keep holding BOOTSEL.
3. A USB drive called **`RPI-RP2`** appears. Let go of BOOTSEL now.
4. **Drag `pi-pico-scope.uf2` onto it.**

That is it. The drive disappears on its own, and the Pico restarts with your new
code. You don't have to unplug anything.

> **Important:** don't edit files on GitHub's website while you have unpushed
> changes here. It has silently emptied the README before.


Using it
--------

You have three buttons. Here's the whole thing:

| What you press        | What happens                                |
|-----------------------|---------------------------------------------|
| **SELECT**            | Moves to the next setting                   |
| **UP** / **DOWN**     | Makes the chosen setting bigger or smaller |
| **UP + DOWN** together| Runs auto-calibration (see below)           |
| **All three** together| Restarts everything from scratch            |

The five settings you can change, and what they do:

| Letter | Name       | What it does                                              |
|--------|------------|-----------------------------------------------------------|
| **V**  | V/div      | How tall the wave is drawn. Bigger number = smaller wave. |
| **T**  | Trigger    | The voltage level the wave must cross to be drawn.        |
| **S**  | Slope      | Draw it on the way up (Rise) or down (Fall).               |
| **A**  | Atten      | How much to shrink the signal: 1x or 10x.                  |
| **D**  | Timebase   | How much time fits on the screen.                          |

**Why is there a "trigger"?** Because electricity is always moving and the
screen can't keep up. So we wait for the signal to cross a line you choose, and
*then* we start drawing. That's why the picture stands still instead of sliding
around. Without a trigger, the wave would never sit still.

**Auto-calibration** fixes one problem: the Pico's wires might not be quite
right, so a "0 volts" reading could actually be 0.05 volts off. To fix this,
**short the signal wire to ground first** (touch the tip to the ground wire),
then hold **UP + DOWN**. It'll measure how far off it is and remember the
correction.

The **LED lights up** when a signal is found. If it's off, the scope can't see
anything.


What's on the screen
--------------------

```
+---------------+-------+
|               | Min   |   <- smallest voltage seen
|  your wave    | Max   |   <- biggest voltage seen
|  drawn here   | Ppk   |   <- Max minus Min
|               | Freq  |   <- how many times per second
|               | Trig  |   <- says "Trig" if it found a wave
+---------------+-------+
| V    T    S    A    D  |   <- which setting you picked
| 2.0V 0.0V Rise 1x  20u  |   <- what each one is set to
| U/D edit SEL next       |   <- a reminder of the buttons
+------------------------+
```

**Min, Max, Ppk.** The lowest and highest voltages in one wave. `Ppk` means
"peak to peak" — it is just Max minus Min, which tells you how big the wave is.

**Freq.** How many times the wave goes up and down in one second. If it says
`1.0k`, that is 1,000 times per second. The suffix tells you the units: `Hz`
means times per second, `kHz` means thousands, `mHz` means millions.

**U/D** means "up and down". The line at the bottom is just reminding you that UP
and DOWN change the setting, and SELECT moves to the next one.


Getting captures onto a computer
--------------------------------

Plug a USB-to-serial cable into **GP4** (TX) and **GP5** (RX), at 9600 baud.
Then you can type letters into a serial terminal program:

| Type | What you get                                                        |
|------|----------------------------------------------------------------------|
| `s`  | A spreadsheet-style file (CSV) — opens in Excel. For Tektronix software. |
| `S`  | Every single sample number. Bigger and slower.                        |
| `F`  | Half as many samples. Faster, for quick checks.                       |

`CSV` just means a plain text file where commas separate the values, so any
spreadsheet program can open it.


How it works inside
-------------------

You do not need to read this part to use the scope. It just explains what's going
on, in case you're curious or something breaks.

**Reading the signal.** The Pico has a chip inside called an **ADC** (analog-to-digital
converter) that measures voltage. It can only measure 0 to 3.3 volts, so the
**frontend** board does two jobs: it makes the signal safe (so you don't blow
anything up) and it shifts a signal centred on 0V up to sit in the middle of
0–3.3V, which is the only range the ADC can read.

**Filling the memory fast.** Electricity changes way faster than the Pico's
brain can write things down one at a time. So the ADC runs on its own clock, and
the results get dumped into memory by **DMA** (Direct Memory Access), which is
hardware that copies data without asking the brain for help. The brain isn't even
involved. That's why the readings are accurate.

**Speed.** The fastest setting is about **1 million readings per second**. There
are 9 timebase settings:

```
20, 50, 100, 250, 500, 1000, 2500, 5000, 10000 microseconds per division
```

`us/div` means "microseconds across one square of the grid". The shorter that is,
the faster the wave looks.

**The bug we fixed.** Each capture takes 256 readings. At the slowest timebase
that takes 160 milliseconds to collect — but the code was only waiting 100 ms
before giving up. So it gave up early, and half the screen showed leftover data
from the previous capture. That's what made the wave look like it was wobbling
about once a second. The waiting time now matches the timebase.

**Trigger, again.** Raw noise near the trigger line could make the scope think the
wave crossed when it didn't, so the picture jumped around. We fixed this by
requiring the signal to move clearly past the line before it counts as a
crossing. It is called **hysteresis** — the same idea as a door that needs a
push to open and a bigger push to shut.


Troubleshooting
---------------

**The screen stays white.**
The screen never got the go-ahead. Try, in order: check the wires; try
**SPI mode 3** instead of mode 0 (some cheap screens insist on it); check that
the screen really is an ST7735.

**The screen is sideways or mirrored.**
Find `landscapeMadctl` in `src/panel_st7789.c` and try `0xC0`, `0x60` or `0x00`
instead of `0xA0`. If you change this, also swap the numbers in the address
window right below it. Rotating will not fix a white screen — it only turns the
picture.

**The colours are swapped (red looks blue).**
Add the "swap red and blue" bit to that same number. Try `0xA8`.

**The picture keeps flickering.**
There's too much noise on the signal. Try a shorter wire, or move the ground
wire. If it's the clip warning flickering, that's a separate small bug we fixed.

**The wave won't sit still.**
The trigger level (T) is probably sitting right where the noise is. Move it so
it's on a flat part of the wave instead of on a jumpy part. There is also a
setting called `TRIG_HYST` in `src/wave.c` you can turn up if there's a lot of
noise — but don't turn it up too far, or the scope will stop finding waves
altogether.

**No signal at all, just a flat line.**
Check the frontend first — it's the most likely culprit. Then check the input is
actually on GP26. Then try raising the attenuation (A) to 10x if the signal is
small.

**The frequency says 0.**
It didn't find two crossings, so it can't work out a period. Usually means the
signal is too small or too noisy. Try moving the trigger level.

**The build fails about TinyUSB.**
You skipped the `git submodule update` step. See "Making it".

**It built, but nothing happens.**
Check the BOOTSEL steps — a UF2 copied to the wrong drive does nothing.


Two small things left over
--------------------------

* `src/panel_st7789.c` is **misnamed**. It drives an ST7735, not an ST7789. It
  was left alone so the working version wouldn't break.
* `printFreq()` in `src/ui.c` is **dead code** — written but never called.
  Harmless. It can be deleted.


Credits
-------

The scope logic, the display drawing, the buttons, the measurements and the
startup logo all come from **pillScopePlus**, which is free to use under the
**Apache License 2.0**. Files like `src/scope.c`, `src/wave.c`, `src/ui.c`,
`src/gfx.c` and `src/splash.h` are that borrowed work and **cannot** be changed
to a different licence.

If you want one licence for the whole repository, **Apache-2.0** is the safe
choice, because it covers everything including the borrowed parts.

This project also uses the **Raspberry Pi Pico SDK**, which is free to use.