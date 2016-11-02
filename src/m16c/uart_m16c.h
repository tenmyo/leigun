#include "bus.h"

#define M16C65_REGSET_UART0	(0)
#define M16C65_REGSET_UART1	(1)
#define M16C65_REGSET_UART2	(2)
#define M16C65_REGSET_UART5	(3)
#define M16C65_REGSET_UART6	(4)
#define M16C65_REGSET_UART7	(5)

BusDevice *M16C_UartNew(const char *name, unsigned int register_set);
