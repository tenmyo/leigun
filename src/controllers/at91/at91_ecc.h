#include "bus.h"
typedef struct AT91Ecc AT91Ecc;
BusDevice *AT91Ecc_New(const char *name);
void AT91Ecc_ResetEC(BusDevice * ecc);
void AT91Ecc_Feed(BusDevice * ecc, uint16_t data, int width);
