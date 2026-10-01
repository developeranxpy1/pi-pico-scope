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
