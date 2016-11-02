/*
 **********************************************************************************
 * I2C-Serializer/Deserializer
 *      Adapting between Logical levels and the byte level
 *      I2C-Chip interface
 *
 *
 * (C) 2004  Lightmaze Solutions AG
 *   Author: Jochen Karrer
 **********************************************************************************
 */

#include <i2c.h>

typedef struct I2C_SerDes I2C_SerDes;

void I2C_SerDesAddSlave(I2C_SerDes * serdes,I2C_Slave *slave,int addr);
int I2C_SerDesDetachSlave(I2C_SerDes * serdes,I2C_Slave *slave);
I2C_SerDes * I2C_SerDesNew(const char *name);
void SerDes_UnstretchScl(I2C_SerDes *serdes);
void SerDes_Decouple(I2C_SerDes *serdes);
