#ifndef UI_H
#define UI_H

#include <stdint.h>

void splash(void);
void ui(void);
void sideInfo(void);
void settingsBar(void);
void autoCal(void);
void outputSerial(const char *s, uint8_t o);
void outputCSV(uint8_t o);
void outputTek(uint8_t o);

#endif