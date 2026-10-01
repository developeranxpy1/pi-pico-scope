#include "gfx.h"
#include "scope.h"
#include "wave.h"

#define WAVE_COLOR ST7789_YELLOW
#define WAVE2_COLOR ST7789_CYAN

/* Interleaved capture accessor. With ADC_CHANNELS == 1 this collapses to
   buf[i], so the original single-channel path is byte-for-byte unchanged. */
#define CH_SAMP(buf, i, ch) ((buf)[((i) * ADC_CHANNELS) + (ch)])

/* Trigger hysteresis in ADC counts. Rejects single-sample noise near the
   threshold: simulated jitter in the trigger point drops from ~19 samples to
   ~0.5 across amplitudes of 100-700 counts. Must stay well under the signal's
   downward excursion from the threshold or the trigger never re-arms - at 80
   counts a 30-count signal stopped triggering entirely. */
#define TRIG_HYST 32

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

static void drawTrace(const uint16_t *buf, uint16_t trig, uint16_t col, uint8_t ch,
                      int updateStats)
{
    if (updateStats)
    {
        maxVoltage = LOWER_VOLTAGE;
        minVoltage = UPPER_VOLTAGE;
    }

    int samplesToDraw = BUFFER_LEN - trig - 1;
    /* drawLine(i, .., i + 1, ..) paints x = samplesToDraw, so clamping to
       PLOT_W would draw one column into the stats menu at x = PLOT_W. */
    if (samplesToDraw > PLOT_W - 1)
        samplesToDraw = PLOT_W - 1;

    topClip = 0;
    bottomClip = 0;

    for (int i = 0; i < samplesToDraw; i++)
    {
        float voltage1 = atten * frontendVoltage(CH_SAMP(buf, i + trig, ch));
        float voltage2 = atten * frontendVoltage(CH_SAMP(buf, i + trig + 1, ch));
        if (updateStats)
        {
            if (voltage2 > maxVoltage)
                maxVoltage = voltage2;
            if (voltage2 < minVoltage)
                minVoltage = voltage2;
        }

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

#if LOGIC_ANALYSER
#define LOGIC_COLOR ST7789_GREEN
#define LOGIC_TRIGGER_COLOR ST7789_RED

/* Digital lane display. Each lane is a square wave: a high level drawn near the
   top of its band, a low level near the bottom, with the transition drawn
   vertically. Thresholding the ADC round-robin keeps the timing jitter-free,
   which matters far more for digital than it does for analog. */
static void drawLogicTrace(const uint16_t *buf, uint16_t trig)
{
    const int laneH = PLOT_H / LOGIC_LANES;
    int samplesToDraw = BUFFER_LEN - trig - 1;
    if (samplesToDraw > PLOT_W - 1)
        samplesToDraw = PLOT_W - 1;

    static const char laneName[LOGIC_LANES][3] = {"26", "27", "28", "29"};

    for (int lane = 0; lane < LOGIC_LANES; lane++)
    {
        int top = lane * laneH + 2;
        int bot = (lane + 1) * laneH - 2;
        int lastX = -1;
        uint8_t lastState = 0;

        for (int i = 0; i < samplesToDraw; i++)
        {
            uint16_t s = buf[(i + trig) * LOGIC_LANES + lane];
            uint8_t state = (s >= LOGIC_THRESHOLD) ? 1 : 0;
            int y = state ? top : bot;

            if (lastX >= 0 && state != lastState)
                drawFastVLine(i, state ? top : bot, bot - top, LOGIC_TRIGGER_COLOR);
            drawPixel(i, y, LOGIC_COLOR);
            lastState = state;
            lastX = i;
        }

        setTextColor(ST7789_WHITE, ST7789_BLACK);
        setTextSize(1);
        setCursor(0, lane * laneH + 1);
        printString(laneName[lane]);
    }
    setTextColor(ST7789_WHITE, ST7789_BLACK);
}

void traceScreen(void)
{
#if LOGIC_ANALYSER
    drawLogicTrace(adcBuf, trigPoint);
#else
    drawGraticule(XDIV, YDIV, PIXDIV);
    drawTrace(adcBuf, trigPoint, WAVE_COLOR, trigChannel, 1);
#if ADC_CHANNELS >= 2
    drawTrace(adcBuf, trigPoint, WAVE2_COLOR, (uint8_t)(1 - trigChannel), 0);
#endif
#endif
}
#else
void traceScreen(void)
{
    drawGraticule(XDIV, YDIV, PIXDIV);
    /* Stats come from the trigger channel only, so the readings stay tied to
       one signal instead of flickering between two. */
    drawTrace(adcBuf, trigPoint, WAVE_COLOR, trigChannel, 1);
#if ADC_CHANNELS >= 2
    drawTrace(adcBuf, trigPoint, WAVE2_COLOR, (uint8_t)(1 - trigChannel), 0);
#endif
}
#endif

void findTrigger(uint16_t *buf, uint8_t ch)
{
    int trigLevel = (int)((4096.0 * (trigVoltage / (2.0 * atten) + offsetVoltage)) / 3.3);
    int trigPoint2;

    /* A bare level crossing retriggers on noise that merely touches the
       threshold, which makes the trace jump frame to frame. Require the signal
       to travel TRIG_HYST counts clear of the threshold before it is armed
       again, so only a real crossing counts. */
    const int hyst = TRIG_HYST;

    trigPoint = 0;
    trigged = 0;
    measuredFreq = 0;

    int armed = (trig == RISING) ? (CH_SAMP(buf, 0, ch) < trigLevel - hyst)
                                 : (CH_SAMP(buf, 0, ch) > trigLevel + hyst);

    for (int i = 1; i < BUFFER_LEN / 2 && trigged != 2; i++)
    {
        uint16_t cur = CH_SAMP(buf, i, ch);
        uint16_t prev = CH_SAMP(buf, i - 1, ch);
        int crossed = (trig == RISING) ? (cur >= trigLevel && prev < trigLevel)
                                       : (cur <= trigLevel && prev > trigLevel);

        if (armed && crossed)
        {
            armed = 0;
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

        if ((trig == RISING) ? (cur < trigLevel - hyst) : (cur > trigLevel + hyst))
            armed = 1;
    }

    if (trigged == 2)
    {
        sigPer = sampPer * (trigPoint2 - trigPoint);
        measuredFreq = 1000000.0 / sigPer;
    }
}
