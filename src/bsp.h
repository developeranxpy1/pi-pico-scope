#ifndef BSP_H
#define BSP_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define PIN_LCD_SCK   6
#define PIN_LCD_MOSI  7
#define PIN_LCD_CS    1
#define PIN_LCD_DC    2
#define PIN_LCD_RST   3
#define PIN_LCD_BL    0

#define PIN_ADC_IN    26
#define PIN_BTN_UP    15
#define PIN_BTN_SEL   16
#define PIN_BTN_DOWN  17
#define PIN_LED       25

#define PIN_UART_TX   4
#define PIN_UART_RX   5

#define LCD_SPI_BAUD_HZ      20000000u
#define LCD_FLUSH_SETTLE_MS  10
#define UART_BAUD            9600

void bspInit(void);
void bspDelayMs(uint32_t ms);

bool bspButtonDown(int pin);
void bspLedSet(bool on);

void bspUartWrite(const char *data, size_t len);
void bspUartRx(char c);

void bspLcdSelect(bool select);
void bspLcdReset(bool release);
void bspLcdBacklight(bool on);
void bspLcdSetDc(uint8_t dc);
void spi_write_bytes(const void *data, size_t len);

void printFloat(float v, int decimalDigits, char s[]);

#endif