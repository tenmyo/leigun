/*
 * ----------------------------------------------------
 *
 * Definitions required for all PCI Implementations
 *
 * (C) 2004  Lightmaze Solutions AG
 *   Author: Jochen Karrer
 *
 * ----------------------------------------------------
 */

#ifndef PCI_H
#define PCI_H
#include <stdint.h>
#include "bus.h"
#include "controllers/ns9750/ns9750_timer.h"

#define PCI_CONFIG_ENABLED(confreg)     (((confreg)>>31)&1)
#define PCI_CONFIG_BUS(confreg)         (((confreg)>>16)&0xff)
#define PCI_CONFIG_DEVICE(confreg)      (((confreg)>>11)&0x1f)
#define PCI_CONFIG_FUNCTION(confreg)    (((confreg)>>8)&0x7)
#define PCI_CONFIG_TYPE(confreg)        (((confreg)>>0)&3)
#define 	PCI_CONFIG_TYPE_0	(0)	/* local */
#define 	PCI_CONFIG_TYPE_1	(1)	/* behind bridge */
#define PCI_CONFIG_REGISTER(confreg)    (((confreg)&0xfc)>>0)

#define  PCI_HEADER_TYPE_NORMAL 0
#define  PCI_HEADER_TYPE_BRIDGE 1
#define  PCI_HEADER_TYPE_CARDBUS 2

#define PCI_VENDOR_ID_DIGI              (0x114f)
#define PCI_VENDOR_ID_VIA               (0x1106)
#define PCI_DEVICE_ID_VT6105M		(0x3053)
#define PCI_DEVICE_ID_DIGI_NS9750	(0xc4)
#define PCI_DEVICE_ID_STE10_100A	(0x2774)
//define PCI_DEVICE_ID_STE10_100A       (0x981)
#define PCI_VENDOR_ID_STM		(0x104a)
#define PCI_VENDOR_ID_INTEL		(0x8086)
#define PCI_DEVICE_ID_82559		(0x1229)

#define PCI_INTA	(0)
#define PCI_INTB	(1)
#define PCI_INTC	(2)
#define PCI_INTD	(3)

#define PCI_DEVICE(x)	(x)

typedef struct PCI_Function {
	BusDevice bdev;
	void *owner;
	int bus;
	int dev;
	int function;

	int (*configRead) (uint32_t * value, uint8_t reg, struct PCI_Function *);
	int (*configWrite) (uint32_t value, uint32_t mask, uint8_t reg, struct PCI_Function *);

	/* 
	 * --------------------------------------------------------------
	 * PCI devices MAP themselves. So this is only required 
	 * because of Memory Window changes or byteorder Changes
	 * --------------------------------------------------------------
	 */
	void (*Map) (void *owner, uint32_t flags);
	void (*UnMap) (void *owner);
	uint32_t hw_flags;

	struct PCI_Function *next;
} PCI_Function;

typedef uint32_t PCI_IOReadProc(void *clientData, uint32_t address, int rqlen);
typedef void PCI_IOWriteProc(void *clientData, uint32_t value, uint32_t address, int rqlen);
typedef uint32_t PCI_MMIOReadProc(void *clientData, uint32_t address, int rqlen);
typedef void PCI_MMIOWriteProc(void *clientData, uint32_t value, uint32_t address, int rqlen);

int PCI_RegisterIOH(uint32_t pci_addr, PCI_IOReadProc * readproc, PCI_IOWriteProc * writeproc,
		    void *clientData);
int PCI_RegisterMMIOH(uint32_t pci_addr, PCI_MMIOReadProc * readproc, PCI_MMIOWriteProc * writeproc,
		      void *clientData);
int PCI_UnRegisterMMIOH(uint32_t pci_addr);
int PCI_UnRegisterIOH(uint32_t pci_addr);

/* Host Byte Order Access */
static inline void
PCI_MasterWrite32(PCI_Function * bridge, uint32_t value, uint32_t addr)
{
	return bridge->bdev.write32(value, addr);
}

static inline uint32_t
PCI_MasterRead32(PCI_Function * bridge, uint32_t addr)
{
	return bridge->bdev.read32(addr);
}

/* Write a value in host Byteorder as little endian to the target Bus  */
static inline void
PCI_MasterWrite32LE(PCI_Function * bridge, uint32_t value, uint32_t addr)
{
	return bridge->bdev.write32(host32_to_le(value), addr);
}

/* Read in little endian from target bus and return in host Byteorder   */
static inline uint32_t
PCI_MasterRead32LE(PCI_Function * bridge, uint32_t addr)
{
	return le32_to_host(bridge->bdev.read32(addr));
}

static inline void
PCI_MasterWrite32BE(PCI_Function * bridge, uint32_t value, uint32_t addr)
{
	return bridge->bdev.write32(host32_to_be(value), addr);
}

/* Read in little endian from target bus and return in host Byteorder   */
static inline uint32_t
PCI_MasterRead32BE(PCI_Function * bridge, uint32_t addr)
{
	return be32_to_host(bridge->bdev.read32(addr));
}

static inline void
PCI_MasterReadLE(PCI_Function * bridge, uint32_t addr, uint8_t * buf, int count)
{
	return bridge->bdev.readblock(buf, addr, count);
}

static inline void
PCI_MasterWriteLE(PCI_Function * bridge, uint32_t addr, uint8_t * buf, int count)
{
	return bridge->bdev.writeblock(addr, buf, count);
}

static inline void
PCI_PostIRQ(PCI_Function * brfunc, int irq)
{
	brfunc->bdev.postIRQ(&brfunc->bdev, irq);
}

static inline void
PCI_UnPostIRQ(PCI_Function * brfunc, int irq)
{
	brfunc->bdev.unPostIRQ(&brfunc->bdev, irq);
}

int PCI_FunctionRegister(PCI_Function * bridgefunc, PCI_Function * newfunc);

#endif
