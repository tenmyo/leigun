#include "bus.h"
#include "nand.h"
BusDevice * AT91EbiNand_New(const char *name,NandFlash *nf,BusDevice *ecc);
