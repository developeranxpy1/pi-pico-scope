#include <hardware/dma.h>

#include "bsp.h"
#include "gfx.h"
#include "panel.h"

#define ST7789_SWRESET 0x01
#define ST7789_SLPOUT  0x11
#define ST7789_NORON   0x13
#define ST7789_INVON   0x21
#define ST7789_DISPON  0x29
#define ST7789_CASET   0x2A
#define ST7789_RASET   0x2B
#define ST7789_RAMWR   0x2C
#define ST7789_MADCTL  0x36
#define ST7789_COLMOD  0x3A

#define MADCTL_VALUE   0x00

int16_t _width = LCD_W;
int16_t _height = LCD_H;

static uint16_t frameBuffer[LCD_W * LCD_H];

static void cmd1(uint8_t c)
{
    bspLcdCommand(c);
}

static void cmd2(uint8_t c, uint8_t a)
{
    bspLcdCommand(c);
    bspLcdData(&a, 1);
}

static void cmd3(uint8_t c, uint8_t a, uint8_t b)
{
    bspLcdCommand(c);
    bspLcdData(&a, 1);
    bspLcdData(&b, 1);
}

static void setAddrWindow(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1)
{
    uint8_t d[4];
    bspLcdCommand(ST7789_CASET);
    d[0] = (uint8_t)(x0 >> 8);
    d[1] = (uint8_t)(x0 & 0xFF);
    d[2] = (uint8_t)(x1 >> 8);
    d[3] = (uint8_t)(x1 & 0xFF);
    bspLcdData(d, 4);

    bspLcdCommand(ST7789_RASET);
    d[0] = (uint8_t)(y0 >> 8);
    d[1] = (uint8_t)(y0 & 0xFF);
    d[2] = (uint8_t)(y1 >> 8);
    d[3] = (uint8_t)(y1 & 0xFF);
    bspLcdData(d, 4);

    bspLcdCommand(ST7789_RAMWR);
}

void panelInit(void)
{
    bspLcdBacklight(false);

    bspLcdReset(false);
    bspDelayMs(20);
    bspLcdReset(true);
    bspDelayMs(120);

    bspLcdSelect(true);

    cmd1(ST7789_SWRESET);
    bspDelayMs(150);

    cmd1(ST7789_SLPOUT);
    bspDelayMs(120);

    cmd2(ST7789_COLMOD, 0x55);
    bspDelayMs(10);

    cmd3(0xB0, 0x0C, 0x0C);
    cmd3(0xB1, 0x30, 0x2F);
    cmd3(0xB2, 0x2C, 0x2F);
    cmd3(0xB3, 0x30, 0x2F);
    cmd3(0xB4, 0x30, 0x2F);
    cmd2(0xB7, 0xC5);
    cmd3(0xC0, 0x09, 0x09);
    cmd3(0xC1, 0x09, 0x05);
    cmd3(0xC2, 0x09, 0x05);
    cmd3(0xC3, 0x09, 0x09);
    cmd3(0xC4, 0x09, 0x09);

    cmd2(ST7789_MADCTL, MADCTL_VALUE);

    cmd1(ST7789_INVON);
    bspDelayMs(10);

    cmd1(ST7789_NORON);
    bspDelayMs(10);

    cmd1(ST7789_DISPON);
    bspDelayMs(20);

    setAddrWindow(0, 0, LCD_W - 1, LCD_H - 1);

    bspLcdSelect(false);
    bspLcdBacklight(true);
}

void panelFlush(void)
{
    bspLcdSelect(true);
    setAddrWindow(0, 0, LCD_W - 1, LCD_H - 1);
    bspLcdDataDma(frameBuffer, (size_t)(LCD_W * LCD_H));
    bspLcdSelect(false);
    bspDelayMs(LCD_FLUSH_SETTLE_MS);
}

void drawPixel(int16_t x, int16_t y, uint16_t color)
{
    if (x < 0 || x >= _width || y < 0 || y >= _height)
        return;
    frameBuffer[y * _width + x] = color;
}