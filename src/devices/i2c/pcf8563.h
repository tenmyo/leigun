/*
 ***********************************************************************************
 *
 * Interface for Emulation of I2C-Realtime Clock
 *
 ***********************************************************************************
 */

#include <i2c.h>
I2C_Slave * PCF8563_New(char *name);
typedef struct PCF8563 PCF8563;
