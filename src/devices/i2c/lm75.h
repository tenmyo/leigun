/*
 **********************************************************************************
 *
 * Interface for Emulation of LM75 Temperature Sensor 
 *
 **********************************************************************************
 */

#include <i2c.h>
I2C_Slave * LM75_New(char *name);
typedef struct LM75 LM75;
