/*
 ***********************************************************************************
 *
 * Interface for Emulation of I2C A/D Converter
 *
 ***********************************************************************************
 */

#include <i2c.h>
I2C_Slave * PCF8591_New(char *name);
typedef struct PCF8591 PCF8591;
