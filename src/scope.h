#ifndef SCOPE_H
#define SCOPE_H

#include "panel.h"

#define PIXDIV 16
#define XDIV  8
#define YDIV  6

#define PLOT_W (PIXDIV * XDIV)
#define PLOT_H (PIXDIV * YDIV)

#define MENU_X PLOT_W
#define MENU_W (LCD_W - PLOT_W)
#define BAR_Y  PLOT_H
#define BAR_H  (LCD_H - PLOT_H)

#define BUFFER_LEN (2 * PIXDIV * XDIV)

/* ---- Capture topology ---------------------------------------------------
   The RP2040 ADC is a single 4-channel multiplexer: it converts one pin per
   instant and round-robins between the enabled ones. So N channels means the
   per-channel rate is (total rate / N), not the total rate. Both features
   below default to OFF so the original single-channel analog path is unchanged.
   ---------------------------------------------------------------------- */

/* Analog channels captured per frame. 1 = CH1 only (original behaviour).
   2 = CH1 on GP26 and CH2 on GP27, interleaved, each at half the rate. */
#ifndef ADC_CHANNELS
#define ADC_CHANNELS 1
#endif

/* Logic analyser mode: 0 = analog scope (default), 1 = digital lanes.
   Uses the same ADC round-robin, thresholded into high/low, which keeps the
   timing jitter-free but caps the usable rate at roughly 250 kSa/s per pin. */
#ifndef LOGIC_ANALYSER
#define LOGIC_ANALYSER 0
#endif

#if LOGIC_ANALYSER
/* Only GP26..GP29 are ADC-capable, so that is the hard limit on lane count. */
#define LOGIC_LANES 4
#define LOGIC_THRESHOLD 2048
/* In logic mode the interleaved group is one digital sample, so the group size
   is the lane count rather than ADC_CHANNELS. */
#define CAPTURE_CHANS LOGIC_LANES
#else
#define CAPTURE_CHANS ADC_CHANNELS
#endif

#define CAPTURE_LEN (CAPTURE_CHANS * BUFFER_LEN)

#define UPPER_VOLTAGE (atten * 3.3)
#define LOWER_VOLTAGE (atten * -3.3)

#define RISING  1
#define FALLING 0

void scopeInit(void);
void scopeLoop(void);
void sample(void);
void scopeSetTdiv(uint32_t sampleRate);
void scopeResetDevice(void);

#endif
