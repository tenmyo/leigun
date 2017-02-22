#ifndef _SRAM_H
#define _SRAM_H
#include "bus.h"
typedef struct SRam SRam;
BusDevice *SRam_New(char *name);
#endif
