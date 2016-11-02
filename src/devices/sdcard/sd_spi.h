#include "mmcard.h"
typedef struct SD_Spi SD_Spi;
SD_Spi *SDSpi_New(const char *name, MMCDev * card);
uint8_t SDSpi_ByteExchange(void *clientData, uint8_t data);
