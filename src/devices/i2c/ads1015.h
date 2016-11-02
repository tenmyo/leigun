/*
 ****************************************************************************
 * Interface for Emulation of I2C A/D Converter
 ******************************************************************************
 */

#include <i2c.h>
I2C_Slave * ADS1015_New(char *name);
typedef struct ADS1015 ADS1015;
