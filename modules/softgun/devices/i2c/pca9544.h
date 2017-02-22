/*
 **********************************************************************************
 *
 * Interface definition for Emulation of PCA9544 4-channel I2C-Multiplexer
 *
 **********************************************************************************
 */

#include "i2c.h"
I2C_Slave *PCA9544_New(char *name);
typedef struct PCA9544 PCA9544;
