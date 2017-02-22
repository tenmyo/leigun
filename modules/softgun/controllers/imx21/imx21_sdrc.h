/*
 **********************************************************************************
 * Emulation of Freescale iMX21 SDRAM controller module 
 *
 * (C) 2006 Jochen Karrer
 *   Author: Jochen Karrer
 **********************************************************************************
 */

#include <bus.h>
BusDevice *IMX21_SdrcNew(const char *name, BusDevice * dram1, BusDevice * dram2);
