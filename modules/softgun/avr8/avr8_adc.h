#ifndef AVR8_ADC_H
#define AVR8_ADC_H
#include <stdint.h>
typedef struct AVR8_Adc AVR8_Adc;
AVR8_Adc *AVR8_AdcNew(const char *name, uint16_t base);
typedef uint32_t AVR8_AdcReadProc(void *clientData);
void AVR8_AdcRegisterSource(AVR8_Adc * adc, unsigned int channel, AVR8_AdcReadProc * proc,
			    void *clientData);

#endif
