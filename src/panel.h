#ifndef PANEL_H
#define PANEL_H

#include <stdint.h>

#define LCD_W 240
#define LCD_H 240

#define ST7789_BLACK   0x0000
#define ST7789_WHITE   0xFFFF
#define ST7789_RED     0xF800
#define ST7789_GREEN   0x07E0
#define ST7789_BLUE    0x001F
#define ST7789_CYAN    0x07FF
#define ST7789_MAGENTA 0xF81F
#define ST7789_YELLOW  0xFFE0
#define ST7789_ORANGE  0xFC00

void panelInit(void);
void panelFlush(void);

#endif