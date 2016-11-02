#include "bus.h"
#define M32C87_REGSET_CRCGEN	(0)
#define M16C65_REGSET_CRCGEN	(1)
BusDevice *M32C_CRCNew(const char *name, unsigned int register_set);
