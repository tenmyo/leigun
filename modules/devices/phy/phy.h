/*
 * ---------------------------------------------
 * Interface definitions for Ethernet PHY's
 * (C) 2004  Lightmaze Solutions AG
 *   Author: Jochen Karrer
 * ---------------------------------------------
 */

#ifndef PHY_H
#define PHY_H

#include <stdint.h>
typedef struct PHY_Device {
	void *owner;
	int (*writereg) (struct PHY_Device *, uint16_t value, int reg);
	int (*readreg) (struct PHY_Device *, uint16_t * value, int reg);
} PHY_Device;

static inline int
PHY_ReadRegister(PHY_Device * phy, uint16_t * value, int reg)
{
	return phy->readreg(phy, value, reg);
}

static inline int
PHY_WriteRegister(PHY_Device * phy, uint16_t value, int reg)
{
	return phy->writereg(phy, value, reg);
}

#endif
