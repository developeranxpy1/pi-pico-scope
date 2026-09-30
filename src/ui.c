#include <stdio.h>
#include <string.h>

#include "bsp.h"
#include "gfx.h"
#include "scope.h"
#include "splash.h"
#include "ui.h"
#include "wave.h"

#define BLACK ST7789_BLACK
#define WHITE ST7789_WHITE

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
    for (int i = 0; i < BUFFER_LEN; i++)
        adcAvg += adcBuf[i];
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
    drawBitmap((LCD_W - 160) / 2, 24, 160, 128, 160, logo);
    setTextSize(1);
    setCursor(4, 160);
    setTextColor(WHITE, BLACK);
    printString("FW compiled: ");
    printString(__DATE__);
    setCursor(4, 174);
    printString("RP2040 + ST7789 240x240");
    panelFlush();
    bspDelayMs(2000);
}

void sideInfo(void)
{
    char st[15];

    setTextSize(1);
    setTextColor(BLACK, WHITE);

    setCursor(MENU_X + MENU_PAD, 2);
    printString("Min:");
    setCursor(MENU_X + MENU_PAD, 34);
    printString("Max:");
    setCursor(MENU_X + MENU_PAD, 66);
    printString("Ppk:");
    setCursor(MENU_X + MENU_PAD, 98);
    printString("Freq");

    setTextSize(2);
    setTextColor(WHITE, BLACK);

    printFloat(minVoltage, 1, st);
    setCursor(MENU_X + MENU_PAD, 14);
    printString(st);

    printFloat(maxVoltage, 1, st);
    setCursor(MENU_X + MENU_PAD, 46);
    printString(st);

    printFloat(maxVoltage - minVoltage, 1, st);
    setCursor(MENU_X + MENU_PAD, 78);
    printString(st);
    printString("V");

    printFreq();

    setTextSize(1);
    setCursor(MENU_X + MENU_PAD, 144);
    if (trigged)
    {
        setTextColor(ST7789_GREEN, BLACK);
        printString("Trig");
    }
    else
    {
        setTextColor(ST7789_GREEN, BLACK);
        setTextSize(1);
        printString("----");
    }
    setTextColor(WHITE, BLACK);
}

void settingsBar(void)
{
    static uint8_t sel = 0;
    char st[15];

    const int colX[5] = {2, 50, 98, 146, 194};

    setTextSize(1);
    if (sel == 0)
        setTextColor(BLACK, WHITE);
    else if (topClip || bottomClip)
        setTextColor(ST7789_RED, BLACK);
    else
        setTextColor(WHITE, BLACK);

    setCursor(colX[0], BAR_Y + 6);
    printString("Vdiv");

    setTextColor(WHITE, BLACK);
    setCursor(colX[1], BAR_Y + 6);
    printString("Trig");

    setCursor(colX[2], BAR_Y + 6);
    printString("Slope");

    setCursor(colX[3], BAR_Y + 6);
    printString("Atten");

    setCursor(colX[4], BAR_Y + 6);
    if (tdiv < 1000)
        printString("us/d");
    else
        printString("ms/d");

    if (sel == 1)
    {
        setTextColor(BLACK, WHITE);
        drawFastHLine(0, (int16_t)((PIXDIV * YDIV / 2 - 1) - (trigVoltage * PIXDIV / vdiv)), PLOT_W, ST7789_RED);
    }

    setTextSize(2);
    setTextColor(WHITE, BLACK);

    if (sel == 0)
        setTextColor(BLACK, WHITE);
    printFloat(vdiv, 1, st);
    setCursor(colX[0], BAR_Y + 18);
    printString(st);
    printString("V");

    setTextColor(WHITE, BLACK);
    if (sel == 1)
        setTextColor(BLACK, WHITE);
    printFloat(trigVoltage, 1, st);
    setCursor(colX[1], BAR_Y + 18);
    printString(st);

    setTextColor(WHITE, BLACK);
    if (sel == 2)
        setTextColor(BLACK, WHITE);
    setCursor(colX[2], BAR_Y + 18);
    if (trig == RISING)
        printString("Rise");
    else
        printString("Fall");

    setTextColor(WHITE, BLACK);
    if (sel == 3)
        setTextColor(BLACK, WHITE);
    setCursor(colX[3], BAR_Y + 18);
    printInt(atten);
    printString("x");

    setTextColor(WHITE, BLACK);
    if (sel == 4)
        setTextColor(BLACK, WHITE);
    setCursor(colX[4], BAR_Y + 18);
    if (tdiv < 1000)
        printInt((int)tdiv);
    else
    {
        printFloat(tdiv / 1000.0, 1, st);
        printString(st);
    }

    setTextSize(1);
    setTextColor(WHITE, BLACK);
    setCursor(2, BAR_Y + 44);
    printString("Sample rate: ");
    if (sampRate >= 1000000)
    {
        printInt((int)(sampRate / 1000000));
        printString(".");
        printInt((int)((sampRate % 1000000) / 100000));
        printString(" MSa/s");
    }
    else
    {
        printInt((int)(sampRate / 1000));
        printString(" kSa/s");
    }

    setCursor(2, BAR_Y + 58);
    printString("Up/Down: change   Select: next");

    if (bspButtonDown(PIN_BTN_UP))
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
            if (tdivIndex > 0)
            {
                tdivIndex--;
                tdiv = tdivTable[tdivIndex];
                scopeSetTdiv((uint32_t)((PIXDIV * 1000000.0f) / tdiv));
            }
        }
        bspDelayMs(150);
    }

    if (bspButtonDown(PIN_BTN_DOWN))
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
            if ((size_t)(tdivIndex + 1) < TDIV_STEPS)
            {
                tdivIndex++;
                tdiv = tdivTable[tdivIndex];
                scopeSetTdiv((uint32_t)((PIXDIV * 1000000.0f) / tdiv));
            }
        }
        bspDelayMs(150);
    }

    if (bspButtonDown(PIN_BTN_SEL))
    {
        sel++;
        bspDelayMs(150);
    }
    if (sel > 4)
        sel = 0;
}

void ui(void)
{
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

    panelFlush();
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
        float voltage = atten * frontendVoltage(adcBuf[i]);
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
            sprintf(buffer, "%d\n\r", adcBuf[i + trigPoint]);
            outputSerial(buffer, o);
        }
    else
        for (int i = 0; i < BUFFER_LEN; i++)
        {
            sprintf(buffer, "%d\n\r", adcBuf[i]);
            outputSerial(buffer, o);
        }

    sprintf(buffer, "SendWaveComplete!\n\r");
    outputSerial(buffer, o);
    sprintf(buffer, "\n\r");
    outputSerial(buffer, o);
}