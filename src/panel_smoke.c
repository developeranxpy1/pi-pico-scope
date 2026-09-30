#include <stdio.h>

#include "bsp.h"
#include "panel_smoke.h"

static uint8_t chunk[4096];

static void tx(uint8_t dc, const uint8_t *d, uint32_t n)
{
    bspLcdSelect(true);
    bspLcdSetDc(dc);
    spi_write_bytes(d, n);
    bspLcdSelect(false);
}

static void cmd(uint8_t c)
{
    tx(0, &c, 1);
}

static void dat1(uint8_t v)
{
    tx(1, &v, 1);
}

static void initPanel(uint8_t colmod)
{
    bspLcdBacklight(true);

    bspLcdReset(false);
    bspDelayMs(10);
    bspLcdReset(true);
    bspDelayMs(120);

    cmd(0x01);
    bspDelayMs(150);
    cmd(0x11);
    bspDelayMs(120);
    cmd(0x3A);
    dat1(colmod);
    bspDelayMs(10);
    cmd(0x36);
    dat1(0x00);
    bspDelayMs(10);
    cmd(0x21);
    bspDelayMs(10);
    cmd(0x29);
    bspDelayMs(10);

    cmd(0x2A);
    {
        static const uint8_t w[4] = {0x00, 0x00, 0x00, 0xEF};
        tx(1, w, 4);
    }
    cmd(0x2B);
    {
        static const uint8_t w[4] = {0x00, 0x00, 0x00, 0xEF};
        tx(1, w, 4);
    }
    cmd(0x2C);
}

static void paintRgb565(uint16_t color)
{
    for (unsigned i = 0; i < sizeof(chunk); i += 2)
    {
        chunk[i] = (uint8_t)(color >> 8);
        chunk[i + 1] = (uint8_t)(color & 0xFF);
    }

    bspLcdSelect(true);
    bspLcdSetDc(1);
    for (int i = 0; i < 57600 / 2048; i++)
        spi_write_bytes(chunk, sizeof(chunk));
    bspLcdSelect(false);
}

void panelSmokeTest(void)
{
    static const struct {
        uint16_t color;
        const char *name;
    } sweep[] = {
        {0x0000, "BLACK"},
        {0xFFFF, "WHITE"},
        {0xF800, "RED"},
        {0x07E0, "GREEN"},
        {0x001F, "BLUE"},
        {0x5555, "GREY"},
    };
    const unsigned n = sizeof(sweep) / sizeof(sweep[0]);

    initPanel(0x55);

    printf("smoke: sweep start, %u steps, RGB565 big-endian\n", n);
    fflush(stdout);

    for (unsigned i = 0; i < n; i++)
    {
        paintRgb565(sweep[i].color);
        bspLedSet(true);
        printf("smoke: %d/%u %s 0x%04X\n", i + 1, n, sweep[i].name, sweep[i].color);
        fflush(stdout);
        bspDelayMs(2500);
        bspLedSet(false);
    }

    printf("smoke: sweep done\n");
    fflush(stdout);
}
