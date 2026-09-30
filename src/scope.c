#include <hardware/adc.h>
#include <hardware/clocks.h>
#include <hardware/dma.h>
#include <hardware/irq.h>
#include <hardware/sync.h>
#include <hardware/watchdog.h>

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
    adc_init();
    adc_gpio_init(PIN_ADC_IN);
    adc_select_input(0);
    adc_fifo_setup(true, true, 1, false, false);

    dma_channel_set_irq1_enabled(CAPTURE_DMA_CHANNEL, true);
    irq_set_exclusive_handler(dma_get_irq_num(1), captureDmaIrq);

    panelInit();

    splash();

    sampRate = (uint32_t)((PIXDIV * 1000000.0f) / tdiv);
    applySampleRate();
}

void sample(void)
{
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

    while (!captureDone)
        __wfe();

    adc_run(false);
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