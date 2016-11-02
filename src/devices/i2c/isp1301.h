/*
 **********************************************************************************
 *
 * Interface to Philips ISP1301 USB OTG transceiver 
 *
 **********************************************************************************
 */

#include <i2c.h>
#include <stdint.h>
I2C_Slave * ISP1301_New(char *name);

/* Lazy direct access */
typedef struct ISP1301  ISP1301;
void ISP1301_Write(ISP1301 *isp,uint8_t data,uint8_t addr);
uint8_t ISP1301_Read(ISP1301 *isp,uint8_t addr);
ISP1301 * ISP1301_GetPtr(I2C_Slave *slave);
