/*
 ****************************************************************************
 * Interface definition for MDIO devices Clause 45 IEEE 803.2
 ****************************************************************************
 */

#ifndef MDIODEV_H
#define MDIODEV_H

#include <stdint.h>
typedef struct MDIO_Device {
	void *owner;
	void (*opAddress) (struct MDIO_Device *, uint16_t addr);
	void (*opWrite) (struct MDIO_Device *, uint16_t value);
	void (*opRead) (struct MDIO_Device *, uint16_t * value,int incr);
} MDIO_Device;

static inline void 
MDIODev_Read(MDIO_Device * md, uint16_t * valP, int incr)
{
	return md->opRead(md, valP, incr);
}

static inline void 
MDIODev_Write(MDIO_Device * md, uint16_t value)
{
	return md->opWrite(md, value);
}

static inline void 
MDIODev_Address(MDIO_Device * md, uint16_t addr)
{
	return md->opAddress(md, addr);
}

#endif
