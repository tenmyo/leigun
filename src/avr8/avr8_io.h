#ifndef _AVR8_IO_H
#define _AVR8_IO_H
#include <stdint.h>

typedef uint8_t AVR8_IoReadProc(void *clientData,uint32_t address);
typedef void AVR8_IoWriteProc(void * clientData,uint8_t value,uint32_t address);
typedef struct AVR8_Iohandler {
	AVR8_IoReadProc *ioReadProc;
	AVR8_IoWriteProc *ioWriteProc;
	void *clientData;
} AVR8_Iohandler;
#endif
