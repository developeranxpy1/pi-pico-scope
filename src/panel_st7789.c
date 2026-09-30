#include <hardware/dma.h>

#include <stdio.h>

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

#define MADCTL_VALUE   0x60
#define PANEL_SPI_MODE 0

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

static void panelHardwareReset(void)
{
    bspLcdBacklight(false);
    bspLcdReset(false);
    bspDelayMs(20);
    bspLcdReset(true);
    bspDelayMs(120);
    bspLcdSelect(true);
}

static void panelInitSequence(int mode, uint8_t madctl)
{
    bspLcdSetSpiMode(mode);

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

    cmd2(ST7789_MADCTL, madctl);

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

void panelInit(void)
{
    panelHardwareReset();
    panelInitSequence(PANEL_SPI_MODE, MADCTL_VALUE);
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
void panelSelfTest(void)
{
    static const uint16_t bars[] = {
        ST7789_RED, ST7789_GREEN, ST7789_BLUE,
        ST7789_WHITE, ST7789_BLACK, ST7789_YELLOW,
    };
    static const char names[] = "RGBWY";

    printf("panel: self test, %dx%d, blocking SPI\n", LCD_W, LCD_H);

    for (unsigned i = 0; i < sizeof(bars) / sizeof(bars[0]); i++)
    {
        for (int p = 0; p < LCD_W * LCD_H; p++)
            frameBuffer[p] = bars[i];
        panelFlush();
        bspLedSet(true);
        printf("panel: bar %c = 0x%04X\n", names[i], bars[i]);
        bspDelayMs(500);
        bspLedSet(false);
    }

    printf("panel: self test done\n");
}

void panelProbe(void)
{
    static const struct {
        uint8_t mode;
        uint8_t madctl;
        uint16_t color;
        const char *name;
    } cfgs[] = {
        {0, 0x60, ST7789_RED,    "mode0 madctl 0x60"},
        {1, 0x60, ST7789_GREEN,  "mode3 madctl 0x60"},
        {0, 0x00, ST7789_BLUE,   "mode0 madctl 0x00"},
        {1, 0x00, ST7789_YELLOW, "mode3 madctl 0x00"},
    };
    const unsigned n = sizeof(cfgs) / sizeof(cfgs[0]);

    printf("probe: %u configs, each 2s\n", n);

    for (unsigned i = 0; i < n; i++)
    {
        panelHardwareReset();
        panelInitSequence(cfgs[i].mode, cfgs[i].madctl);

        for (int p = 0; p < LCD_W * LCD_H; p++)
            frameBuffer[p] = cfgs[i].color;
        panelFlush();

        bspLedSet(true);
        printf("probe %u/%u: %-20s colour 0x%04X\n", i + 1, n, cfgs[i].name, cfgs[i].color);
        fflush(stdout);
        bspDelayMs(2000);
        bspLedSet(false);
    }

    bspLcdBacklight(false);
    printf("probe: done - if one bar held steady, that config works\n");
}
