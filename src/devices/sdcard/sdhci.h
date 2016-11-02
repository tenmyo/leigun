#include "bus.h"
#include "mmcdev.h"
BusDevice *SDHCI_New(const char *name);
int SDHCI_InsertCard(BusDevice * dev, MMCDev * card);
