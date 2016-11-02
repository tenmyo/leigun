typedef struct AVR8_Adc  AVR8_Adc;
AVR8_Adc * AVR8_AdcNew(const char *name,uint16_t base);
typedef uint32_t AVR8_AdcReadProc(void *clientData);
void AVR8_AdcRegisterSource(AVR8_Adc *adc,unsigned int channel,AVR8_AdcReadProc *proc,void *clientData);
