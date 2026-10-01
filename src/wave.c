#include "gfx.h"
#include "scope.h"
#include "wave.h"

#define WAVE_COLOR ST7789_YELLOW

extern uint16_t adcBuf[BUFFER_LEN];
extern int atten;
extern float vdiv;
extern float trigVoltage;
extern uint8_t trig, trigged;
extern int trigPoint;

extern float tdiv;
extern uint32_t sampRate;
extern float sampPer;

extern float maxVoltage, minVoltage;
extern float measuredFreq, sigPer;

extern float offsetVoltage;

uint8_t topClip, bottomClip;

float frontendVoltage(uint16_t samp)
{
    return 2 * (((3.3 * samp) / 4096.0) - offsetVoltage);
}

float adcToVoltage(uint16_t samp)
{
    return (3.3 * samp) / 4096.0;
}

static void dottedHLine(int x, int y, int l);
static void dottedVLine(int x, int y, int l);

static void drawGraticule(uint16_t divx, uint16_t divy, uint16_t pix)
{
    uint16_t wit = divx * pix;
    uint16_t hei = divy * pix;

    for (int i = 0; i < wit; i += pix)
        dottedVLine(i, 0, hei);

    for (int i = 0; i < hei; i += pix)
        dottedHLine(0, i, wit);
}

static void dottedHLine(int x, int y, int l)
{
    for (int i = 0; i < l; i++)
    {
        if (i % 2)
            drawPixel(x + i, y, ST7789_WHITE);
        else
            drawPixel(x + i, y, ST7789_BLACK);
    }
}

static void dottedVLine(int x, int y, int l)
{
    for (int i = 0; i < l; i++)
    {
        if (i % 2)
            drawPixel(x, y + i, ST7789_WHITE);
        else
            drawPixel(x, y + i, ST7789_BLACK);
    }
}

static void drawTrace(const uint16_t *buf, uint16_t trig, uint16_t col)
{
    maxVoltage = LOWER_VOLTAGE;
    minVoltage = UPPER_VOLTAGE;

    int samplesToDraw = BUFFER_LEN - trig - 1;
    if (samplesToDraw > PLOT_W)
        samplesToDraw = PLOT_W;

    for (int i = 0; i < samplesToDraw; i++)
    {
        float voltage1 = atten * frontendVoltage(buf[i + trig]);
        float voltage2 = atten * frontendVoltage(buf[i + trig + 1]);
        if (voltage2 > maxVoltage)
            maxVoltage = voltage2;
        if (voltage2 < minVoltage)
            minVoltage = voltage2;

        topClip = 0;
        bottomClip = 0;
        int16_t y1 = (PIXDIV * YDIV / 2 - 1) - (voltage1 * PIXDIV / vdiv);
        int16_t y2 = (PIXDIV * YDIV / 2 - 1) - (voltage2 * PIXDIV / vdiv);
        if (y1 > PLOT_H - 1)
        {
            y1 = PLOT_H - 1;
            bottomClip = 1;
        }
        if (y2 > PLOT_H - 1)
        {
            y2 = PLOT_H - 1;
            bottomClip = 1;
        }
        if (y1 < 0)
        {
            y1 = 0;
            topClip = 1;
        }
        if (y2 < 0)
        {
            y2 = 0;
            topClip = 1;
        }
        drawLine(i, y1, i + 1, y2, col);
    }
}

void traceScreen(void)
{
    drawGraticule(XDIV, YDIV, PIXDIV);
    drawTrace(adcBuf, trigPoint, WAVE_COLOR);
}

void findTrigger(uint16_t *buf)
{
    int trigLevel = (int)((4096.0 * (trigVoltage / (2.0 * atten) + offsetVoltage)) / 3.3);
    int trigPoint2;

    trigPoint = 0;
    trigged = 0;
    measuredFreq = 0;

    for (int i = 1; i < BUFFER_LEN / 2 && trigged != 2; i++)
        if ((trig == RISING && buf[i] >= trigLevel && buf[i - 1] < trigLevel) ||
            (trig == FALLING && buf[i] <= trigLevel && buf[i - 1] > trigLevel))
        {
            if (!trigged)
            {
                trigPoint = i;
                trigged = 1;
            }
            else
            {
                trigPoint2 = i;
                trigged = 2;
            }
        }

    if (trigged == 2)
    {
        sigPer = sampPer * (trigPoint2 - trigPoint);
        measuredFreq = 1000000.0 / sigPer;
    }
}
