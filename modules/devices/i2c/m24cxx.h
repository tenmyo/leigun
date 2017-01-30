/*
 **********************************************************************************
 * Interface for Emulation of M24C64 I2C EEPROM
 **********************************************************************************
 */

#include "i2c.h"
I2C_Slave *M24Cxx_New(const char *type, const char *name);
typedef struct M24Cxx M24Cxx;
