#include "bsp.h"

#include <hardware/gpio.h>
#include <hardware/irq.h>
#include <hardware/spi.h>
#include <hardware/timer.h>
#include <hardware/uart.h>
#include <stdio.h>


static spi_inst_t *const lcdSpi = spi0;

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

    spi_init(lcdSpi, LCD_SPI_BAUD_HZ);
    spi_set_format(lcdSpi, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
    gpio_set_function(PIN_LCD_MOSI, GPIO_FUNC_SPI);
    gpio_set_function(PIN_LCD_SCK, GPIO_FUNC_SPI);

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

void spi_write_bytes(const void *data, size_t len)
{
    spi_write_blocking(lcdSpi, (const uint8_t *)data, len);
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
