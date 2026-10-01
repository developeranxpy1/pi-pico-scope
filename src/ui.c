#include <stdio.h>
#include <string.h>
#include <pico/time.h>

#include "bsp.h"
#include "gfx.h"
#include "scope.h"
#include "splash.h"
#include "ui.h"
#include "wave.h"

#define BLACK ST7789_BLACK
#define WHITE ST7789_WHITE

extern uint16_t adcBuf[CAPTURE_LEN];
extern uint8_t trigChannel;
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
extern uint8_t topClip, bottomClip;

volatile uint8_t outputFlag = 0;
uint8_t fast = 1;
uint8_t autocalFlag = 0;

static const float tdivTable[] = {20, 50, 100, 250, 500, 1000, 2500, 5000, 10000};
#define TDIV_STEPS (sizeof(tdivTable) / sizeof(tdivTable[0]))
static int tdivIndex = 0;

#define MENU_PAD 4
#define BLOCK_GAP 32

static void printFreq(void)
{
    char st[15];
    setTextSize(2);
    setCursor(MENU_X + MENU_PAD, 110);
    if (measuredFreq >= 1000)
    {
        if (measuredFreq >= 100000)
            printInt((int)measuredFreq / 1000);
        else
        {
            printFloat(measuredFreq / 1000, 1, st);
            printString(st);
        }
        setTextSize(1);
        setCursor(MENU_X + MENU_PAD, 128);
        printString("kHz");
    }
    else
    {
        printInt((int)measuredFreq);
        setTextSize(1);
        setCursor(MENU_X + MENU_PAD, 128);
        printString("Hz");
    }
    setTextSize(1);
}

void autoCal(void)
{
    clearDisplay();
    setTextSize(1);
    setCursor(0, 0);
    setTextColor(BLACK, WHITE);
    printString("Autocalibration\n\n");
    setTextColor(WHITE, BLACK);
    printString("Couple input to ground\nThen press Select");
    panelFlush();
    while (!bspButtonDown(PIN_BTN_SEL))
        ;
    bspDelayMs(150);

    sample();

    clearDisplay();
    setTextSize(1);
    setCursor(0, 0);
    setTextColor(BLACK, WHITE);
    printString("Autocalibration\n\n");
    setTextColor(WHITE, BLACK);

    uint32_t adcAvg = 0;
    /* Offset is calibrated from CH1 only; averaging the interleaved buffer
       would fold CH2 in whenever ADC_CHANNELS > 1. */
    for (int i = 0; i < BUFFER_LEN; i++)
        adcAvg += adcBuf[i * ADC_CHANNELS];
    adcAvg /= BUFFER_LEN;

    offsetVoltage = adcToVoltage(adcAvg);

    char st[15];
    printFloat(offsetVoltage, 2, st);
    printString("Offset voltage: ");
    printString(st);
    printString("V\n");

    printFloat(frontendVoltage(0), 2, st);
    printString("Min input voltage: ");
    printString(st);
    printString("V\n");

    printFloat(frontendVoltage(4096), 2, st);
    printString("Max input voltage: ");
    printString(st);
    printString("V\n");

    panelFlush();

    while (bspButtonDown(PIN_BTN_SEL))
        ;
    bspDelayMs(150);
}

void splash(void)
{
    clearDisplay();
    drawBitmap(0, 0, LCD_W, LCD_H, LCD_W, logo);
    setTextSize(1);
    panelFlush();
    bspDelayMs(2000);
}

void sideInfo(void)
{
    char st[15];

    setTextSize(1);
    printFloat(minVoltage, 1, st);
    setTextColor(BLACK, WHITE);
    setCursor(MENU_X, 1);
    printString("Min:");
    setTextColor(WHITE, BLACK);
    setCursor(MENU_X, 10);
    printString(st);

    printFloat(maxVoltage, 1, st);
    setTextColor(BLACK, WHITE);
    setCursor(MENU_X, 21);
    printString("Max:");
    setTextColor(WHITE, BLACK);
    setCursor(MENU_X, 30);
    printString(st);

    setTextColor(BLACK, WHITE);
    setCursor(MENU_X, 41);
    printString("Ppk:");
    setTextColor(WHITE, BLACK);
    setCursor(MENU_X, 51);
    printFloat(maxVoltage - minVoltage, 1, st);
    printString(st);
    printString("V");

    setTextColor(BLACK, WHITE);
    setCursor(MENU_X, 61);
    printString("Freq");
    setTextColor(WHITE, BLACK);
    setCursor(MENU_X, 71);
    if (measuredFreq >= 1000)
    {
        if (measuredFreq >= 100000)
            printInt((int)measuredFreq / 1000);
        else
        {
            printFloat(measuredFreq / 1000, 1, st);
            printString(st);
        }
        setCursor(MENU_X, 81);
        printString("kHz");
    }
    else
    {
        printInt((int)measuredFreq);
        setCursor(MENU_X, 81);
        printString("Hz");
    }

    if (trigged)
    {
        setTextColor(ST7789_GREEN, BLACK);
        setCursor(MENU_X, 91);
        printString("Trig");
    }
    setTextColor(WHITE, BLACK);
}

/* Button handling used to block on bspDelayMs(150) after every press. That
   froze the entire scope - capture included - for 150 ms, which is why moving
   through the settings felt slow and dragged the frame rate down. Debounce is
   now time-based and non-blocking: the UI keeps running full speed and a press
   is simply ignored until the gap expires. */
#define NAV_GAP_US 40000u
static uint32_t lastNavUs;

static int navEdge(int pin, uint32_t gapUs)
{
    if (!bspButtonDown(pin))
        return 0;

    uint32_t now = time_us_32();
    if ((int32_t)(now - lastNavUs) < (int32_t)gapUs)
        return 0;

    lastNavUs = now;
    return 1;
}

void settingsBar(void)
{
    static uint8_t sel = 0;
    char st[15];

    setTextSize(1);
    if (topClip || bottomClip)
        setTextColor(ST7789_RED, BLACK);
    else
        setTextColor(WHITE, BLACK);
    setCursor(0, 105);
    printString("Vdiv");

    setTextColor(WHITE, BLACK);
    setCursor(30, 105);
    printString("Trig");
    setCursor(60, 105);
    printString("Slope");
    setCursor(95, 105);
    printString("Atten");
    setCursor(130, 105);
    if (tdiv < 100)
        printString("us/d");
    else
        printString("ms/d");

    if (sel == 0)
    {
        if (topClip || bottomClip)
            setTextColor(ST7789_RED, ST7789_WHITE);
        else
            setTextColor(ST7789_BLACK, ST7789_WHITE);
    }
    printFloat(vdiv, 1, st);
    setCursor(0, 115);
    printString(st);
    printString("V");

    setTextColor(WHITE, BLACK);
    if (sel == 1)
    {
        setTextColor(BLACK, WHITE);
        drawFastHLine(0, (int16_t)((PIXDIV * YDIV / 2 - 1) - (trigVoltage * PIXDIV / vdiv)), PLOT_W, ST7789_RED);
    }
    printFloat(trigVoltage, 1, st);
    setCursor(30, 115);
    printString(st);

    setTextColor(WHITE, BLACK);
    if (sel == 2)
        setTextColor(BLACK, WHITE);
    setCursor(60, 115);
    if (trig == RISING)
        printString("Rise");
    else
        printString("Fall");

    setTextColor(WHITE, BLACK);
    if (sel == 3)
        setTextColor(BLACK, WHITE);
    setCursor(95, 115);
    printInt(atten);
    printString("x");

    setTextColor(WHITE, BLACK);
    if (sel == 4)
        setTextColor(BLACK, WHITE);
    setCursor(130, 115);
    if (tdiv < 100)
        printInt((int)tdiv);
    else if (tdiv < 1000)
    {
        printString("0.");
        printInt((int)tdiv / 100);
    }
    else
        printInt((int)tdiv / 1000);

    if (navEdge(PIN_BTN_UP, NAV_GAP_US) || navEdge(PIN_NAV_UP, NAV_GAP_US))
    {
        if (sel == 0)
        {
            if (vdiv > 0.5)
                vdiv -= 0.5;
        }
        else if (sel == 1)
        {
            trigVoltage -= 0.1;
        }
        else if (sel == 2)
        {
            trig = FALLING;
        }
        else if (sel == 3)
        {
            atten = 1;
        }
        else if (sel == 4)
        {
            if (tdiv > 1000)
                tdiv -= 1000;
            else if (tdiv > 100)
                tdiv -= 100;
            else if (tdiv > 10)
                tdiv -= 10;
            scopeSetTdiv((uint32_t)((PIXDIV * 1000000.0f) / tdiv));
        }
    }

    if (navEdge(PIN_BTN_DOWN, NAV_GAP_US) || navEdge(PIN_NAV_DOWN, NAV_GAP_US))
    {
        if (sel == 0)
        {
            if (vdiv < 9)
                vdiv += 0.5;
        }
        else if (sel == 1)
        {
            trigVoltage += 0.1;
        }
        else if (sel == 2)
        {
            trig = RISING;
        }
        else if (sel == 3)
        {
            atten = 10;
        }
        else if (sel == 4)
        {
            if (tdiv >= 1000)
                tdiv += 1000;
            else if (tdiv >= 100)
                tdiv += 100;
            else
                tdiv += 10;
            scopeSetTdiv((uint32_t)((PIXDIV * 1000000.0f) / tdiv));
        }
    }

    if (navEdge(PIN_BTN_SEL, NAV_GAP_US) || navEdge(PIN_NAV_RIGHT, NAV_GAP_US))
    {
        sel++;
    }

    /* Nav pad LEFT steps back through the five settings, wrapping at zero.
       SELECT and RIGHT both step forward, so either hand works. */
    if (navEdge(PIN_NAV_LEFT, NAV_GAP_US))
    {
        sel = (sel == 0) ? 4 : sel - 1;
    }

    if (sel > 4)
        sel = 0;
}

/* Frame timing, so the real refresh rate can be read from the USB log instead
   of guessed. The full-screen SPI push alone costs 16.38 ms at 20 MHz, which
   caps the frame rate at 61 fps however fast the drawing is. */
#define FRAME_DIAG_PERIOD 32
static uint32_t fRenderSum, fFlushSum, fCount;

static void frameStats(uint32_t renderUs, uint32_t flushUs)
{
    fRenderSum += renderUs;
    fFlushSum += flushUs;
    if (++fCount < FRAME_DIAG_PERIOD)
        return;

    uint32_t frames = fCount;
    uint32_t renderAvg = fRenderSum / frames;
    uint32_t flushAvg = fFlushSum / frames;
    fRenderSum = 0;
    fFlushSum = 0;
    fCount = 0;

    uint32_t totalAvg = renderAvg + flushAvg;
    printf("[fps] render=%lu us  flush=%lu us  total=%lu us  => %lu fps\r\n",
           (unsigned long)renderAvg, (unsigned long)flushAvg,
           (unsigned long)totalAvg,
           (unsigned long)(totalAvg ? 1000000u / totalAvg : 0));
}

void ui(void)
{
    uint32_t tStart = time_us_32();
    clearDisplay();

    if (bspButtonDown(PIN_BTN_UP) && bspButtonDown(PIN_BTN_DOWN))
    {
        autocalFlag = 1;
        if (bspButtonDown(PIN_BTN_SEL))
        {
            extern void scopeResetDevice(void);
            scopeResetDevice();
        }
    }

    if (autocalFlag)
    {
        autoCal();
        autocalFlag = 0;
    }

    traceScreen();
    sideInfo();
    settingsBar();
    uint32_t tRender = time_us_32();

    if (outputFlag)
    {
        if (outputFlag < 3)
        {
            outputCSV(outputFlag);
            outputFlag = 0;
        }
        else
        {
            outputTek(outputFlag - 2);
            outputFlag = 0;
        }
    }

    uint32_t tFlushStart = time_us_32();
    panelFlush();
    frameStats(tRender - tStart, time_us_32() - tFlushStart);
}

void outputSerial(const char *s, uint8_t o)
{
    if (o == 2)
        bspUartWrite(s, strlen(s));
}

void outputCSV(uint8_t o)
{
    char st[15];
    char s1[15];
    char buffer[40];

    setTextSize(1);
    setCursor(2, 5);
    setTextColor(BLACK, WHITE);
    printString("Sending data");
    printString(" via UART");
    panelFlush();

    sprintf(buffer, "\033[2J\033[H\033[3J");
    outputSerial(buffer, o);

    sprintf(buffer, "Model,TekscopeSW\n\r");
    outputSerial(buffer, o);

    sprintf(buffer, "Label,CH1\n\r");
    outputSerial(buffer, o);

    sprintf(buffer, "Waveform Type,ANALOG\n\r");
    outputSerial(buffer, o);

    sprintf(buffer, "Horizontal Units,s\n\r");
    outputSerial(buffer, o);

    printFloat(sampPer, 2, st);
    sprintf(buffer, "Sample Interval,%sE-06\n\r", st);
    outputSerial(buffer, o);

    sprintf(buffer, "Record Length,%d\n\r", BUFFER_LEN);
    outputSerial(buffer, o);

    sprintf(buffer, "Zero Index,%d\n\r", trigPoint);
    outputSerial(buffer, o);
    bspDelayMs(5);

    sprintf(buffer, "Vertical Units,V\n\r");
    outputSerial(buffer, o);

    sprintf(buffer, ",\n\rLabels,\n\r");
    outputSerial(buffer, o);

    sprintf(buffer, "TIME,CH1\n\r");
    outputSerial(buffer, o);

    for (int i = 0; i < BUFFER_LEN; i++)
    {
        float voltage = atten * frontendVoltage(adcBuf[i * ADC_CHANNELS]);
        printFloat(voltage, 3, st);
        printFloat((float)i * sampPer, 3, s1);
        sprintf(buffer, "%sE-06,%s\n\r", s1, st);
        outputSerial(buffer, o);
    }
}

void outputTek(uint8_t o)
{
    char st[15];
    char buffer[40];

    setTextSize(1);
    setCursor(2, 5);
    setTextColor(BLACK, WHITE);
    printString("Sending data");
    printString(" via UART");
    panelFlush();

    sprintf(buffer, "BeginWave!\n\r");
    outputSerial(buffer, o);

    printFloat(sampPer, 2, st);
    sprintf(buffer, "%s\n\r", st);
    outputSerial(buffer, o);

    if (fast)
        sprintf(buffer, "%d\n\r", BUFFER_LEN / 2);
    else
        sprintf(buffer, "%d\n\r", BUFFER_LEN);
    outputSerial(buffer, o);

    if (fast)
        sprintf(buffer, "%d\n\r", 0);
    else
        sprintf(buffer, "%d\n\r", trigPoint);
    outputSerial(buffer, o);

    printFloat(offsetVoltage, 4, st);
    sprintf(buffer, "%s\n\r", st);
    outputSerial(buffer, o);

    sprintf(buffer, "%d\n\r", atten);
    outputSerial(buffer, o);
    bspDelayMs(1);

    if (fast)
        for (int i = 0; i < BUFFER_LEN / 2; i++)
        {
            sprintf(buffer, "%d\n\r", adcBuf[(i + trigPoint) * ADC_CHANNELS]);
            outputSerial(buffer, o);
        }
    else
        for (int i = 0; i < BUFFER_LEN; i++)
        {
            sprintf(buffer, "%d\n\r", adcBuf[i * ADC_CHANNELS]);
            outputSerial(buffer, o);
        }

    sprintf(buffer, "SendWaveComplete!\n\r");
    outputSerial(buffer, o);
    sprintf(buffer, "\n\r");
    outputSerial(buffer, o);
}
