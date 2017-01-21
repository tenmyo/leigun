/*
 **********************************************************************************
 * Interface for Freescale i.MX21 SD-card host controller emulator
 *
 * (C) 2006 Jochen Karrer
 *   Author: Jochen Karrer
 **********************************************************************************
 */

#include "bus.h"
#include "mmcdev.h"
BusDevice *IMX21_SdhcNew(const char *name);
int IMX21Sdhc_InsertCard(BusDevice * dev, MMCDev * card);
int IMX21Sdhc_RemoveCard(BusDevice * dev, MMCDev * card);
