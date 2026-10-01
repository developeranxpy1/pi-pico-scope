#ifndef WAVE_H
#define WAVE_H

#include <stdint.h>

float adcToVoltage(uint16_t samp);
float frontendVoltage(uint16_t samp);
void traceScreen(void);
void findTrigger(uint16_t *buf, uint8_t ch);

#endif