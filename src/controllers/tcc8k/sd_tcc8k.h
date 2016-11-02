#include "bus.h"
#include "mmcdev.h"
BusDevice *TCC8K_SdNew(const char *name);
int TCC8K_InsertCard(BusDevice *dev,MMCDev *card);
