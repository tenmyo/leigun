#include "bus.h"
#include "mmcard.h"
BusDevice * AT91Mci_New(const char *name);
int AT91Mci_InsertCard(BusDevice *dev,MMCDev *card,unsigned int slot);
int AT91Mci_RemoveCard(BusDevice *dev,MMCDev *card,unsigned int slot);
