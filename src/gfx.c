#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "font.h"
#include "gfx.h"

int16_t cursor_x = 0;
int16_t cursor_y = 0;
uint16_t textcolor = ST7789_WHITE;
uint16_t textbgcolor = ST7789_BLACK;
uint16_t textsize_x = 1;
uint16_t textsize_y = 1;

void drawBitmap(int16_t x0, int16_t y0, int16_t w, int16_t h, int16_t stride, const uint16_t *bmap)
{
    for (int16_t i = 0; i < w; i++)
        for (int16_t j = 0; j < h; j++)
            drawPixel(x0 + i, y0 + j, bmap[i + j * stride]);
}

void drawCircle(int16_t x0, int16_t y0, int16_t r, uint16_t color)
{
    int16_t f = 1 - r;
    int16_t ddF_x = 1;
    int16_t ddF_y = -2 * r;
    int16_t x = 0;
    int16_t y = r;

    drawPixel(x0, y0 + r, color);
    drawPixel(x0, y0 - r, color);
    drawPixel(x0 + r, y0, color);
    drawPixel(x0 - r, y0, color);

    while (x < y)
    {
        if (f >= 0)
        {
            y--;
            ddF_y += 2;
            f += ddF_y;
        }
        x++;
        ddF_x += 2;
        f += ddF_x;

        drawPixel(x0 + x, y0 + y, color);
        drawPixel(x0 - x, y0 + y, color);
        drawPixel(x0 + x, y0 - y, color);
        drawPixel(x0 - x, y0 - y, color);
        drawPixel(x0 + y, y0 + x, color);
        drawPixel(x0 - y, y0 + x, color);
        drawPixel(x0 + y, y0 - x, color);
        drawPixel(x0 - y, y0 - x, color);
    }
}

void drawRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color)
{
    drawFastHLine(x, y, w, color);
    drawFastHLine(x, y + h - 1, w, color);
    drawFastVLine(x, y, h, color);
    drawFastVLine(x + w - 1, y, h, color);
}

void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color)
{
    for (int16_t i = x; i < x + w; i++)
        drawFastVLine(i, y, h, color);
}

void clearDisplay(void)
{
    fillRect(0, 0, _width, _height, ST7789_BLACK);
}

void drawChar(int16_t x, int16_t y, unsigned char c, uint16_t color,
              uint16_t bg, uint8_t size_x, uint8_t size_y)
{
    if ((x >= _width) ||
        (y >= _height) ||
        ((x + 6 * size_x - 1) < 0) ||
        ((y + 8 * size_y - 1) < 0))
        return;

    if (c >= 176)
        c++;

    for (int8_t i = 0; i < 5; i++)
    {
        uint8_t line = font[c * 5 + i];
        for (int8_t j = 0; j < 8; j++, line >>= 1)
        {
            if (line & 1)
            {
                if (size_x == 1 && size_y == 1)
                    drawPixel(x + i, y + j, color);
                else
                    fillRect(x + i * size_x, y + j * size_y, size_x, size_y, color);
            }
            else if (bg != color)
            {
                if (size_x == 1 && size_y == 1)
                    drawPixel(x + i, y + j, bg);
                else
                    fillRect(x + i * size_x, y + j * size_y, size_x, size_y, bg);
            }
        }
    }
    if (bg != color)
    {
        if (size_x == 1 && size_y == 1)
            drawFastVLine(x + 5, y, 7, bg);
        else
            fillRect(x + 5 * size_x, y, size_x, 8 * size_y, bg);
    }
}

void setCursor(int16_t x, int16_t y)
{
    cursor_x = x;
    cursor_y = y;
}

void setTextColor(uint16_t c, uint16_t bg)
{
    textcolor = c;
    textbgcolor = bg;
}

void setTextSize(uint16_t s)
{
    textsize_x = s;
    textsize_y = s;
}

void writeChar(char c)
{
    if (c == '\n')
    {
        cursor_x = 0;
        cursor_y += textsize_y * 8;
    }
    else if (c != '\r')
    {
        if ((cursor_x + textsize_x * 6) > _width)
        {
            cursor_x = 0;
            cursor_y += textsize_y * 8;
        }
        drawChar(cursor_x, cursor_y, c, textcolor, textbgcolor, textsize_x, textsize_y);
        cursor_x += textsize_x * 6;
    }
}

void printInt(int v)
{
    char s[12];
    sprintf(s, "%d", v);
    printString(s);
}

void printString(const char *s)
{
    size_t n = strlen(s);
    for (size_t i = 0; i < n; i++)
        writeChar(s[i]);
}

void drawFastVLine(int16_t x, int16_t y, int16_t h, uint16_t color)
{
    for (int16_t i = y; i < y + h; i++)
        drawPixel(x, i, color);
}

void drawFastHLine(int16_t x, int16_t y, int16_t l, uint16_t color)
{
    for (int16_t i = x; i < x + l; i++)
        drawPixel(i, y, color);
}

void drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color)
{
    int16_t steep = abs(y1 - y0) > abs(x1 - x0);
    if (steep)
    {
        int16_t t = x0; x0 = y0; y0 = t;
        t = x1; x1 = y1; y1 = t;
    }

    if (x0 > x1)
    {
        int16_t t = x0; x0 = x1; x1 = t;
        t = y0; y0 = y1; y1 = t;
    }

    int16_t dx = x1 - x0;
    int16_t dy = abs(y1 - y0);
    int16_t err = dx / 2;
    int16_t ystep = (y0 < y1) ? 1 : -1;

    for (; x0 <= x1; x0++)
    {
        if (steep)
            drawPixel(y0, x0, color);
        else
            drawPixel(x0, y0, color);

        err -= dy;
        if (err < 0)
        {
            y0 += ystep;
            err += dx;
        }
    }
}