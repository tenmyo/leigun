#include <bus.h>
#define AT91_PID_TABLE_RM9200	(0)
#define AT91_PID_TABLE_SAM9263	(1)

BusDevice *AT91Pmc_New(const char *name, unsigned int pi_table);
