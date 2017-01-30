#include <stdint.h>

typedef uint8_t Spi_ByteExchangeProc(void *clientData, uint8_t data);
void ATM644_SpiNew(const char *name, uint32_t base, Spi_ByteExchangeProc *, void *clientData);
