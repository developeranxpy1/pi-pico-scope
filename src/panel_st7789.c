#include <stdio.h>

#include "bsp.h"
#include "gfx.h"
#include "panel.h"

#define ST7789_SWRESET 0x01
#define ST7789_SLPOUT  0x11
#define ST7789_INVON   0x21
#define ST7789_DISPON  0x29
#define ST7789_CASET   0x2A
#define ST7789_RASET   0x2B
#define ST7789_RAMWR   0x2C
#define ST7789_MADCTL  0x36
#define ST7789_COLMOD  0x3A

#define MADCTL_VALUE   0x60
#define COLMOD_VALUE   0x55

int16_t _width = LCD_W;
int16_t _height = LCD_H;

static uint16_t frameBuffer[LCD_W * LCD_H];

static void writeTransaction(uint8_t dc, const void *data, size_t len)
{
    bspLcdSelect(true);
    bspLcdSetDc(dc);
    spi_write_bytes(data, len);
    bspLcdSelect(false);
}

static void cmd1(uint8_t c)
{
    writeTransaction(0, &c, 1);
}

static void cmdWithData(uint8_t c, uint8_t d)
{
    uint8_t buf[2] = {c, d};
    writeTransaction(0, buf, 1);
    writeTransaction(1, &buf[1], 1);
}

static void setAddrWindow(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1)
{
    uint8_t d[4];

    d[0] = (uint8_t)(x0 >> 8);
    d[1] = (uint8_t)(x0 & 0xFF);
    d[2] = (uint8_t)(x1 >> 8);
    d[3] = (uint8_t)(x1 & 0xFF);
    cmd1(ST7789_CASET);
    writeTransaction(1, d, 4);

    d[0] = (uint8_t)(y0 >> 8);
    d[1] = (uint8_t)(y0 & 0xFF);
    d[2] = (uint8_t)(y1 >> 8);
    d[3] = (uint8_t)(y1 & 0xFF);
    cmd1(ST7789_RASET);
    writeTransaction(1, d, 4);

    cmd1(ST7789_RAMWR);
}

static void fillFramebuf(uint16_t color)
{
    uint16_t v = (uint16_t)__builtin_bswap16(color);
    for (int p = 0; p < LCD_W * LCD_H; p++)
        frameBuffer[p] = v;
}

void panelInit(void)
{
    bspLcdBacklight(false);

    bspLcdReset(false);
    bspDelayMs(10);
    bspLcdReset(true);
    bspDelayMs(120);

    cmd1(ST7789_SWRESET);
    bspDelayMs(150);

    cmd1(ST7789_SLPOUT);
    bspDelayMs(120);

    cmdWithData(ST7789_COLMOD, COLMOD_VALUE);
    bspDelayMs(10);

    cmdWithData(ST7789_MADCTL, MADCTL_VALUE);
    bspDelayMs(10);

    cmd1(ST7789_INVON);
    bspDelayMs(10);

    cmd1(ST7789_DISPON);
    bspDelayMs(10);

    setAddrWindow(0, 0, LCD_W - 1, LCD_H - 1);

    bspLcdBacklight(true);
}

void panelFlush(void)
{
    bspLcdSelect(true);
    bspLcdSetDc(1);
    spi_write_bytes(frameBuffer, sizeof(frameBuffer));
    bspLcdSelect(false);
    bspDelayMs(LCD_FLUSH_SETTLE_MS);
}

void drawPixel(int16_t x, int16_t y, uint16_t color)
{
    if (x < 0 || x >= _width || y < 0 || y >= _height)
        return;
    frameBuffer[y * _width + x] = (uint16_t)__builtin_bswap16(color);
}

void panelSelfTest(void)
{
    static const uint16_t bars[] = {
        ST7789_RED, ST7789_GREEN, ST7789_BLUE,
        ST7789_WHITE, ST7789_BLACK, ST7789_YELLOW,
    };
    static const char names[] = "RGBWY";

    printf("panel: self test, %dx%d\n", LCD_W, LCD_H);

    for (unsigned i = 0; i < sizeof(bars) / sizeof(bars[0]); i++)
    {
        fillFramebuf(bars[i]);
        panelFlush();
        bspLedSet(true);
        printf("panel: bar %c = 0x%04X\n", names[i], bars[i]);
        fflush(stdout);
        bspDelayMs(700);
        bspLedSet(false);
    }

    printf("panel: self test done\n");
}