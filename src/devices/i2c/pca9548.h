/*
 *******************************************************************************
 *
 * Interface definition for Emulation of PCA9548 8-channel I2C-Multiplexer
 *
 *******************************************************************************
 */

#include <i2c.h>
I2C_Slave * PCA9548_New(char *name);
typedef struct PCA9548 PCA9548;
