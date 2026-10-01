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

#ifndef LCD_SPI_BAUD_HZ
#define LCD_SPI_BAUD_HZ      20000000u
#endif

#ifndef LCD_USE_SOFT_SPI
#define LCD_USE_SOFT_SPI     0
#endif

#ifndef LCD_SOFT_SPI_HALF_PERIOD_US
#define LCD_SOFT_SPI_HALF_PERIOD_US 0
#endif

#define ENABLE_CAPTURE       1
#define LCD_FLUSH_SETTLE_MS  0
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
void bspLcdSetSpiMode(uint8_t mode);
void spi_write_bytes(const void *data, size_t len);
void bspLcdWriteDma(const void *data, size_t len);

void printFloat(float v, int decimalDigits, char s[]);

#endif
