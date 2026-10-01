#include <hardware/adc.h>
#include <hardware/clocks.h>
#include <hardware/dma.h>
#include <hardware/irq.h>
#include <hardware/sync.h>
#include <hardware/watchdog.h>
#include <pico/time.h>

#include <string.h>

#include "bsp.h"
#include "gfx.h"
#include "scope.h"
#include "ui.h"
#include "wave.h"

#define ADC_MIN_PERIOD_TICKS 96
#define ADC_DREQ DREQ_ADC
#define CAPTURE_DMA_CHANNEL 1

uint16_t adcBuf[BUFFER_LEN];

int atten = 1;
float vdiv = 2;

uint8_t trigged;
int trigPoint;
float trigVoltage = 0;
uint8_t trig = RISING;

float tdiv = 20;
uint32_t sampRate;
float sampPer;

float maxVoltage, minVoltage;
float measuredFreq, sigPer;

float offsetVoltage = 1.6540283;

static volatile uint8_t captureDone;
volatile uint8_t captureTimedOut;

static void captureDmaIrq(void)
{
    if (dma_channel_get_irq1_status(CAPTURE_DMA_CHANNEL))
    {
        dma_channel_acknowledge_irq1(CAPTURE_DMA_CHANNEL);
        captureDone = 1;
        __sev();
    }
}

static void applySampleRate(void)
{
    uint32_t adcHz = clock_get_hz(clk_adc);
    uint32_t wantTicks = (adcHz + sampRate / 2) / sampRate;
    uint32_t periodTicks = (wantTicks < ADC_MIN_PERIOD_TICKS) ? ADC_MIN_PERIOD_TICKS : wantTicks;

    adc_set_clkdiv((float)(periodTicks - 1));

    uint32_t effectiveRate = adcHz / periodTicks;
    sampPer = 1000000.0f / (float)effectiveRate;
}

void scopeInit(void)
{
    memset(adcBuf, 0, sizeof(adcBuf));

#if ENABLE_CAPTURE
    adc_init();
    adc_gpio_init(PIN_ADC_IN);
    adc_select_input(0);
    adc_fifo_setup(true, true, 1, false, false);

    dma_channel_claim(CAPTURE_DMA_CHANNEL);
    dma_channel_set_irq1_enabled(CAPTURE_DMA_CHANNEL, true);
    irq_set_exclusive_handler(dma_get_irq_num(1), captureDmaIrq);
    irq_set_enabled(dma_get_irq_num(1), true);
#endif

    panelInit();

    splash();

    sampRate = (uint32_t)((PIXDIV * 1000000.0f) / tdiv);
#if ENABLE_CAPTURE
    applySampleRate();
#endif
}

void sample(void)
{
#if !ENABLE_CAPTURE
    return;
#endif

    adc_fifo_drain();

    dma_channel_config_t cfg = dma_channel_get_default_config(CAPTURE_DMA_CHANNEL);
    channel_config_set_read_increment(&cfg, false);
    channel_config_set_write_increment(&cfg, true);
    channel_config_set_dreq(&cfg, ADC_DREQ);
    channel_config_set_transfer_data_size(&cfg, DMA_SIZE_16);

    captureDone = 0;
    dma_channel_configure(CAPTURE_DMA_CHANNEL, &cfg, adcBuf, &adc_hw->fifo,
                          dma_encode_transfer_count(BUFFER_LEN), true);

    adc_run(true);

    /* The capture takes BUFFER_LEN * sampPer microseconds. At the slowest
       timebase (10 ms/div) that is 160 ms, so the old fixed 100 ms deadline
       aborted every capture there and left a half-old buffer on screen. Scale
       the deadline to the real sample rate and keep headroom for the DMA tail. */
    uint32_t captureUs = (uint32_t)(sampPer * (float)BUFFER_LEN);
    uint32_t deadline = time_us_32() + captureUs + 50000u;
    while (!captureDone && (int32_t)(time_us_32() - deadline) < 0)
    {
    }

    captureTimedOut = !captureDone;
    adc_run(false);
    if (captureTimedOut)
    {
        dma_channel_abort(CAPTURE_DMA_CHANNEL);
        /* A partly filled buffer mixes this frame with the previous one, which
           reads as a jumping trace. Flatten it to the 0 V line so a dropped
           capture is obvious instead of showing stale samples. */
        uint16_t zeroCount = (uint16_t)((4096.0f * offsetVoltage) / 3.3f);
        for (int i = 0; i < BUFFER_LEN; i++)
            adcBuf[i] = zeroCount;
    }
    adc_fifo_drain();
}

void scopeLoop(void)
{
    sample();

    findTrigger(adcBuf);
    if (trigged)
        bspLedSet(true);

    ui();
    bspLedSet(false);
}

void scopeSetTdiv(uint32_t newRate)
{
    sampRate = newRate;
    applySampleRate();
}

void scopeResetDevice(void)
{
    panelFlush();
    bspDelayMs(50);
    watchdog_enable(1000, 1);
    while (1)
    {
    }
}

void bspUartRx(char c)
{
    extern uint8_t outputFlag, fast;
    if (c == 's')
        outputFlag = 2;
    else if (c == 'S')
    {
        outputFlag = 4;
        fast = 0;
    }
    else if (c == 'F')
    {
        outputFlag = 4;
        fast = 1;
    }
}
