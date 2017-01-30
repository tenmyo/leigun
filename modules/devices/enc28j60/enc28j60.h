#include <stdint.h>
typedef struct Enc28j60 Enc28j60;
Enc28j60 *Enc28j60_New(const char *name);
uint8_t Enc28j80_SpiByteExchange(void *clientData, uint8_t data);
