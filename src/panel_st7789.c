#include <stdio.h>

#include "bsp.h"
#include "gfx.h"
#include "panel.h"

#define ST77XX_SWRESET 0x01
#define ST77XX_SLPOUT  0x11
#define ST77XX_NORON   0x13
#define ST77XX_INVOFF  0x20
#define ST77XX_DISPON  0x29
#define ST77XX_CASET   0x2A
#define ST77XX_RASET   0x2B
#define ST77XX_RAMWR   0x2C
#define ST77XX_MADCTL  0x36
#define ST77XX_COLMOD  0x3A

#define ST7735_FRMCTR1 0xB1
#define ST7735_FRMCTR2 0xB2
#define ST7735_FRMCTR3 0xB3
#define ST7735_INVCTR  0xB4
#define ST7735_PWCTR1  0xC0
#define ST7735_PWCTR2  0xC1
#define ST7735_PWCTR3  0xC2
#define ST7735_PWCTR4  0xC3
#define ST7735_PWCTR5  0xC4
#define ST7735_VMCTR1  0xC5
#define ST7735_GMCTRP1 0xE0
#define ST7735_GMCTRN1 0xE1

#define ST7735_BLACKTAB_ROTATION_1 0xA0

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

static void cmd1(uint8_t command)
{
    writeTransaction(0, &command, 1);
}

static void cmdWithBytes(uint8_t command, const uint8_t *data, size_t len)
{
    cmd1(command);
    writeTransaction(1, data, len);
}

static void setAddrWindow(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1)
{
    uint8_t data[4] = {
        (uint8_t)(x0 >> 8), (uint8_t)x0,
        (uint8_t)(x1 >> 8), (uint8_t)x1,
    };
    cmdWithBytes(ST77XX_CASET, data, sizeof(data));

    data[0] = (uint8_t)(y0 >> 8);
    data[1] = (uint8_t)y0;
    data[2] = (uint8_t)(y1 >> 8);
    data[3] = (uint8_t)y1;
    cmdWithBytes(ST77XX_RASET, data, sizeof(data));
    cmd1(ST77XX_RAMWR);
}

static void fillFramebuf(uint16_t color)
{
    uint16_t value = (uint16_t)__builtin_bswap16(color);
    for (size_t pixel = 0; pixel < LCD_W * LCD_H; pixel++)
        frameBuffer[pixel] = value;
}

void panelInit(void)
{
    static const uint8_t frameRate[] = {0x01, 0x2C, 0x2D};
    static const uint8_t partialFrameRate[] = {0x01, 0x2C, 0x2D, 0x01, 0x2C, 0x2D};
    static const uint8_t power1[] = {0xA2, 0x02, 0x84};
    static const uint8_t power3[] = {0x0A, 0x00};
    static const uint8_t power4[] = {0x8A, 0x2A};
    static const uint8_t power5[] = {0x8A, 0xEE};
    static const uint8_t positiveGamma[] = {
        0x02, 0x1C, 0x07, 0x12, 0x37, 0x32, 0x29, 0x2D,
        0x29, 0x25, 0x2B, 0x39, 0x00, 0x01, 0x03, 0x10,
    };
    static const uint8_t negativeGamma[] = {
        0x03, 0x1D, 0x07, 0x06, 0x2E, 0x2C, 0x29, 0x2D,
        0x2E, 0x2E, 0x37, 0x3F, 0x00, 0x00, 0x02, 0x10,
    };
    static const uint8_t inversionControl = 0x07;
    static const uint8_t power2 = 0xC5;
    static const uint8_t power5v = 0x0E;
    static const uint8_t colorMode = 0x05;
    static const uint8_t portraitMadctl = 0xC8;
    static const uint8_t landscapeMadctl = ST7735_BLACKTAB_ROTATION_1;

    bspLcdBacklight(true);
    bspLcdReset(false);
    bspDelayMs(10);
    bspLcdReset(true);
    bspDelayMs(120);

    cmd1(ST77XX_SWRESET);
    bspDelayMs(150);
    cmd1(ST77XX_SLPOUT);
    bspDelayMs(500);

    cmdWithBytes(ST7735_FRMCTR1, frameRate, sizeof(frameRate));
    cmdWithBytes(ST7735_FRMCTR2, frameRate, sizeof(frameRate));
    cmdWithBytes(ST7735_FRMCTR3, partialFrameRate, sizeof(partialFrameRate));
    cmdWithBytes(ST7735_INVCTR, &inversionControl, 1);
    cmdWithBytes(ST7735_PWCTR1, power1, sizeof(power1));
    cmdWithBytes(ST7735_PWCTR2, &power2, 1);
    cmdWithBytes(ST7735_PWCTR3, power3, sizeof(power3));
    cmdWithBytes(ST7735_PWCTR4, power4, sizeof(power4));
    cmdWithBytes(ST7735_PWCTR5, power5, sizeof(power5));
    cmdWithBytes(ST7735_VMCTR1, &power5v, 1);
    cmd1(ST77XX_INVOFF);
    cmdWithBytes(ST77XX_MADCTL, &portraitMadctl, 1);
    cmdWithBytes(ST77XX_COLMOD, &colorMode, 1);
    setAddrWindow(0, 0, 127, 159);
    cmdWithBytes(ST7735_GMCTRP1, positiveGamma, sizeof(positiveGamma));
    cmdWithBytes(ST7735_GMCTRN1, negativeGamma, sizeof(negativeGamma));
    cmd1(ST77XX_NORON);
    bspDelayMs(10);
    cmd1(ST77XX_DISPON);
    bspDelayMs(100);

    cmdWithBytes(ST77XX_MADCTL, &landscapeMadctl, 1);
    setAddrWindow(0, 0, LCD_W - 1, LCD_H - 1);
}

void panelFlush(void)
{
    setAddrWindow(0, 0, LCD_W - 1, LCD_H - 1);
    bspLcdSelect(true);
    bspLcdSetDc(1);
    bspLcdWriteDma(frameBuffer, sizeof(frameBuffer));
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

    for (size_t index = 0; index < sizeof(bars) / sizeof(bars[0]); index++)
    {
        fillFramebuf(bars[index]);
        panelFlush();
        bspDelayMs(500);
    }
}

void panelProbe(void)
{
    panelSelfTest();
}
