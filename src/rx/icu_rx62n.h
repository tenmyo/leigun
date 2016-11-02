#include "bus.h"
#define RX_ICU_RX62N	(0)
#define RX_ICU_RX63N	(1)
#define RX_ICU_RX111	(2)
BusDevice *RXICU_New(const char *name, int variant);
