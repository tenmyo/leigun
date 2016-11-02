#include "bus.h"
#define M16C65_REGSET_SIO3	(0)
#define M16C65_REGSET_SIO4	(1)
BusDevice * M16C65_SioNew(const char *name,unsigned int register_set);
