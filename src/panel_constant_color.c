#include <pico/stdio_usb.h>

#include <stdint.h>

#include "bsp.h"

#define ST7789_SWRESET 0x01
#define ST7789_SLPOUT  0x11
#define ST7789_INVON   0x21
#define ST7789_DISPON  0x29
#define ST7789_CASET   0x2A
#define ST7789_RASET   0x2B
#define ST7789_RAMWR   0x2C
#define ST7789_MADCTL  0x36
#define ST7789_COLMOD  0x3A

static uint8_t chunk[4096];

void bspUartRx(char c)
{
    (void)c;
}

static void command(uint8_t value)
{
    bspLcdSelect(true);
    bspLcdSetDc(0);
    spi_write_bytes(&value, 1);
    bspLcdSelect(false);
}

static void data(const uint8_t *values, size_t length)
{
    bspLcdSelect(true);
    bspLcdSetDc(1);
    spi_write_bytes(values, length);
    bspLcdSelect(false);
}

static void initializePanel(void)
{
    static const uint8_t rgb565[] = {0x55};
    static const uint8_t madctl[] = {0x00};
    static const uint8_t window[] = {0x00, 0x00, 0x00, 0xEF};

    bspLcdBacklight(true);
    bspLcdReset(false);
    bspDelayMs(10);
    bspLcdReset(true);
    bspDelayMs(120);

    command(ST7789_SWRESET);
    bspDelayMs(150);
    command(ST7789_SLPOUT);
    bspDelayMs(120);
    command(ST7789_COLMOD);
    data(rgb565, sizeof(rgb565));
    bspDelayMs(10);
    command(ST7789_MADCTL);
    data(madctl, sizeof(madctl));
    bspDelayMs(10);
    command(ST7789_INVON);
    bspDelayMs(10);
    command(ST7789_DISPON);
    bspDelayMs(10);

    command(ST7789_CASET);
    data(window, sizeof(window));
    command(ST7789_RASET);
    data(window, sizeof(window));
    command(ST7789_RAMWR);
}

static void fillChunk(const uint8_t color[2])
{
    for (size_t offset = 0; offset < sizeof(chunk); offset += 2)
    {
        chunk[offset] = color[0];
        chunk[offset + 1] = color[1];
    }
}

int main(void)
{
    static const uint8_t colors[][2] = {
        {0xF8, 0x00}, {0x07, 0xE0}, {0x00, 0x1F}, {0xFF, 0xE0},
        {0xF8, 0x1F}, {0x00, 0x00}, {0xFF, 0xFF},
    };

    stdio_usb_init();
    bspInit();
    initializePanel();

    while (true)
    {
        for (size_t color = 0; color < sizeof(colors) / sizeof(colors[0]); color++)
        {
            fillChunk(colors[color]);
            bspLcdSelect(true);
            bspLcdSetDc(1);
            size_t bytesRemaining = 240u * 240u * 2u;
            while (bytesRemaining)
            {
                size_t bytesToWrite = bytesRemaining;
                if (bytesToWrite > sizeof(chunk))
                    bytesToWrite = sizeof(chunk);
                spi_write_bytes(chunk, bytesToWrite);
                bytesRemaining -= bytesToWrite;
            }
            bspLcdSelect(false);
            bspDelayMs(500);
        }
    }
}
