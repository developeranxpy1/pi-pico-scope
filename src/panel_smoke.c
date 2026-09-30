#include "bsp.h"
#include "panel_smoke.h"

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

static void data(const uint8_t *d, uint32_t n)
{
    tx(1, d, n);
}

void panelSmokeTest(void)
{
    static uint8_t chunk[4096];

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
    {
        uint8_t v = 0x55;
        data(&v, 1);
    }
    bspDelayMs(10);
    cmd(0x36);
    {
        uint8_t v = 0x00;
        data(&v, 1);
    }
    bspDelayMs(10);
    cmd(0x21);
    bspDelayMs(10);
    cmd(0x29);
    bspDelayMs(10);

    cmd(0x2A);
    {
        static const uint8_t w[4] = {0x00, 0x00, 0x00, 0xEF};
        data(w, 4);
    }
    cmd(0x2B);
    {
        static const uint8_t w[4] = {0x00, 0x00, 0x00, 0xEF};
        data(w, 4);
    }
    cmd(0x2C);

    for (unsigned i = 0; i < sizeof(chunk); i += 2)
    {
        chunk[i] = 0xF8;
        chunk[i + 1] = 0x00;
    }

    bspLcdSelect(true);
    bspLcdSetDc(1);
    for (int i = 0; i < 57600 / 2048; i++)
        spi_write_bytes(chunk, sizeof(chunk));
    bspLcdSelect(false);

    bspLedSet(true);
    bspDelayMs(10000);
    bspLedSet(false);
}
