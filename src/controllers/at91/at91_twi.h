#include <bus.h>
#define TWI_FEATURE_SLAVE	(1)
BusDevice * AT91Twi_New(const char *name,uint32_t features);

