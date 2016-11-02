#define R8C23_REGSET_UART0	(0)
#define R8C23_REGSET_UART1	(1)
BusDevice *R8CUart_New(const char *name, unsigned int register_set);
