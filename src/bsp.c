#include "bsp.h"

#include <hardware/gpio.h>
#include <hardware/dma.h>
#include <hardware/irq.h>
#include <hardware/spi.h>
#include <hardware/timer.h>
#include <hardware/uart.h>
#include <stdio.h>


static spi_inst_t *const lcdSpi = spi0;
#define LCD_DMA_CHANNEL 0

static void uartIrqHandler(void)
{
    while (uart_is_readable(uart1))
    {
        char c = (char)uart_getc(uart1);
        bspUartRx(c);
    }
}

void bspInit(void)
{
    gpio_init(PIN_LED);
    gpio_set_dir(PIN_LED, GPIO_OUT);
    gpio_put(PIN_LED, 0);

    const int buttons[] = { PIN_BTN_UP, PIN_BTN_SEL, PIN_BTN_DOWN };
    for (unsigned i = 0; i < sizeof(buttons) / sizeof(buttons[0]); i++)
    {
        gpio_init(buttons[i]);
        gpio_set_dir(buttons[i], GPIO_IN);
        gpio_pull_up(buttons[i]);
    }

#if LCD_USE_SOFT_SPI
    gpio_init(PIN_LCD_SCK);
    gpio_set_dir(PIN_LCD_SCK, GPIO_OUT);
    gpio_put(PIN_LCD_SCK, 0);

    gpio_init(PIN_LCD_MOSI);
    gpio_set_dir(PIN_LCD_MOSI, GPIO_OUT);
    gpio_put(PIN_LCD_MOSI, 0);
#else
    spi_init(lcdSpi, LCD_SPI_BAUD_HZ);
    bspLcdSetSpiMode(0);
    gpio_set_function(PIN_LCD_MOSI, GPIO_FUNC_SPI);
    gpio_set_function(PIN_LCD_SCK, GPIO_FUNC_SPI);
    dma_channel_claim(LCD_DMA_CHANNEL);
#endif

    gpio_init(PIN_LCD_CS);
    gpio_set_dir(PIN_LCD_CS, GPIO_OUT);
    gpio_put(PIN_LCD_CS, 1);

    gpio_init(PIN_LCD_DC);
    gpio_set_dir(PIN_LCD_DC, GPIO_OUT);

    gpio_init(PIN_LCD_RST);
    gpio_set_dir(PIN_LCD_RST, GPIO_OUT);
    gpio_put(PIN_LCD_RST, 1);

    uart_init(uart1, UART_BAUD);
    uart_set_format(uart1, 8, 1, 0);
    gpio_set_function(PIN_UART_TX, UART_FUNCSEL_NUM(uart1, PIN_UART_TX));
    gpio_set_function(PIN_UART_RX, UART_FUNCSEL_NUM(uart1, PIN_UART_RX));
    uart_set_irq_enables(uart1, true, false);
    irq_set_exclusive_handler(UART_IRQ_NUM(uart1), uartIrqHandler);
}

void bspDelayMs(uint32_t ms)
{
    while (ms--)
        busy_wait_us_32(1000);
}

bool bspButtonDown(int pin)
{
    return !gpio_get(pin);
}

void bspLedSet(bool on)
{
    gpio_put(PIN_LED, on);
}

void bspUartWrite(const char *data, size_t len)
{
    uart_write_blocking(uart1, data, len);
}

void bspLcdSetDc(uint8_t dc)
{
    gpio_put(PIN_LCD_DC, dc);
}

void bspLcdSetSpiMode(uint8_t mode)
{
#if LCD_USE_SOFT_SPI
    (void)mode;
#else
    if (mode)
        spi_set_format(lcdSpi, 8, SPI_CPOL_1, SPI_CPHA_1, SPI_MSB_FIRST);
    else
        spi_set_format(lcdSpi, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
#endif
}

void spi_write_bytes(const void *data, size_t len)
{
    const uint8_t *p = (const uint8_t *)data;

#if LCD_USE_SOFT_SPI
    for (size_t i = 0; i < len; i++)
    {
        uint8_t b = p[i];
        for (int bit = 7; bit >= 0; bit--)
        {
            gpio_put(PIN_LCD_MOSI, (b >> bit) & 1u);
#if LCD_SOFT_SPI_HALF_PERIOD_US
            busy_wait_us_32(LCD_SOFT_SPI_HALF_PERIOD_US);
#endif
            gpio_put(PIN_LCD_SCK, 1);
#if LCD_SOFT_SPI_HALF_PERIOD_US
            busy_wait_us_32(LCD_SOFT_SPI_HALF_PERIOD_US);
#endif
            gpio_put(PIN_LCD_SCK, 0);
        }
    }
#else
    spi_write_blocking(lcdSpi, p, len);
#endif
}

void bspLcdWriteDma(const void *data, size_t len)
{
#if LCD_USE_SOFT_SPI
    spi_write_bytes(data, len);
#else
    dma_channel_config_t cfg = dma_channel_get_default_config(LCD_DMA_CHANNEL);
    channel_config_set_transfer_data_size(&cfg, DMA_SIZE_8);
    channel_config_set_read_increment(&cfg, true);
    channel_config_set_write_increment(&cfg, false);
    channel_config_set_dreq(&cfg, spi_get_dreq(lcdSpi, true));

    dma_channel_configure(LCD_DMA_CHANNEL, &cfg, &spi_get_hw(lcdSpi)->dr,
                          data, dma_encode_transfer_count((uint)len), true);
    dma_channel_wait_for_finish_blocking(LCD_DMA_CHANNEL);
    while (spi_is_busy(lcdSpi))
    {
    }
#endif
}

void bspLcdSelect(bool select)
{
    gpio_put(PIN_LCD_CS, !select);
}

void bspLcdReset(bool release)
{
    gpio_put(PIN_LCD_RST, release);
}

void bspLcdBacklight(bool on)
{
    gpio_init(PIN_LCD_BL);
    gpio_set_dir(PIN_LCD_BL, GPIO_OUT);
    gpio_put(PIN_LCD_BL, on);
}

void printFloat(float v, int decimalDigits, char s[])
{
    uint8_t neg = 0;
    if (v < 0)
    {
        neg = 1;
        v = v - (2.0 * v);
    }
    int i = 1;
    int intPart, fractPart;
    for (; decimalDigits != 0; i *= 10, decimalDigits--)
        ;
    intPart = (int)v;
    fractPart = (int)((v - (float)(int)v) * i);
    if (fractPart < 0)
        fractPart *= -1;
    if (neg)
        sprintf(s, "-%i.%i", intPart, fractPart);
    else
        sprintf(s, "%i.%i", intPart, fractPart);
}
