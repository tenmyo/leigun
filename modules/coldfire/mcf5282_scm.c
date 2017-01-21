/*
 *************************************************************************************************
 * Emulation of MFC5282 System Control Module 
 *
 * state:  Not implemented
 *
 * Copyright 2008 Jochen Karrer. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification, are
 * permitted provided that the following conditions are met:
 *
 *   1. Redistributions of source code must retain the above copyright notice, this list of
 *       conditions and the following disclaimer.
 *
 *   2. Redistributions in binary form must reproduce the above copyright notice, this list
 *       of conditions and the following disclaimer in the documentation and/or other materials
 *       provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY Jochen Karrer ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * The views and conclusions contained in the software and documentation are those of the
 * authors and should not be interpreted as representing official policies, either expressed
 * or implied, of Jochen Karrer.
 *
 *************************************************************************************************
 */

#include <bus.h>
#include <sgstring.h>
#include <mcf5282_scm.h>

#define SCM_IPSBAR(base) 	((base) + 0x00)
#define SCM_RAMBAR(base)	((base) + 0x08)
#define SCM_CRSR(base)		((base) + 0x10)
#define SCM_CWCR(base)		((base) + 0x11)
#define SCM_LPICR(base)		((base) + 0x12)
#define SCM_CWSR(base)		((base) + 0x13)
#define SCM_MPARK(base)		((base) + 0x1c)
#define SCM_MPR(base)		((base) + 0x20)
#define SCM_PACR0(base)		((base) + 0x24)
#define SCM_PACR1(base)		((base) + 0x25)
#define SCM_PACR2(base)		((base) + 0x26)
#define SCM_PACR3(base)		((base) + 0x27)
#define SCM_PACR4(base)		((base) + 0x28)
#define SCM_PACR5(base)		((base) + 0x2a)
#define SCM_PACR6(base)		((base) + 0x2b)
#define SCM_PACR7(base) 	((base) + 0x2c)
#define SCM_PACR8(base)		((base) + 0x2e)
#define SCM_GPACR0(base)	((base) + 0x30)
#define SCM_GPACR1(base)	((base) + 0x31)

#define CSM_CSAR(base,x)        ((base) + 0x0 + (x) * 12)
#define CSM_CSMR(base,x)        ((base) + 0x4 + (x) * 12)
#define         CSMR_V          (1<<0)
#define         CSMR_UD         (1<<1)
#define         CSMR_UC         (1<<2)
#define         CSMR_SD         (1<<3)
#define         CSMR_SC         (1<<4)
#define         CSMR_CI         (1<<5)
#define         CSMR_AM         (1<<6)
#define         CSMR_WP         (1<<8)
#define         CSMR_BAM_MASK   (0xffff<<16)
#define         CSMR_BAM_SHIFT  (16)

#define CSM_CSCR(base,x)        ((base) + 0x8 + (x) * 12)
#define         CSCR_BSTW       (1<<3)
#define         CSCR_BSTR       (1<<4)
#define         CSCR_BEM        (1<<5)
#define         CSCR_PS0        (1<<6)
#define         CSCR_PS1        (1<<7)
#define         CSCR_AA         (1<<8)
#define         CSCR_WS_MASK    (0xf << 10)
#define         CSCR_WS_SHIFT   (10)

typedef struct Scm Scm;

typedef struct ChipSelect {
	Scm *scm;
	BusDevice *dev;
	uint16_t csar;
	uint32_t csmr;
	uint32_t cscr;
} ChipSelect;

#define MAX_IPSDEVS	(16)

struct Scm {
	BusDevice bdev;
	uint32_t ipsbar;
	uint32_t ipsbar_offset[MAX_IPSDEVS];
	BusDevice *ipsdev[MAX_IPSDEVS];
	uint32_t rambar;
	BusDevice *rambar_dev;
	ChipSelect cs[7];
	uint8_t crsr;
	uint8_t cwcr;
	uint8_t lpicr;
	uint8_t cwsr;
	uint32_t mpark;
	uint8_t mpr;
	uint8_t pacr0;
	uint8_t pacr1;
	uint8_t pacr2;
	uint8_t pacr3;
	uint8_t pacr4;
	uint8_t pacr5;
	uint8_t pacr6;
	uint8_t pacr7;
	uint8_t pacr8;
	uint8_t gpacr0;
	uint8_t gpacr1;
};
#define GIGA (1024U*1024U*1024U)
static void
Scm_UpdateMappings(Scm * scm)
{
	int i;
	int valid = scm->ipsbar & 1;
	uint32_t ips_addr = scm->ipsbar & (3 << 30);
	for (i = 0; i < MAX_IPSDEVS; i++) {
		if (scm->ipsdev[i]) {
			uint32_t devbase = ips_addr + scm->ipsbar_offset[i];
			Mem_AreaDeleteMappings(scm->ipsdev[i]);
			if (valid) {
				Mem_AreaAddMapping(scm->ipsdev[i], devbase, GIGA,
						   MEM_FLAG_WRITABLE | MEM_FLAG_READABLE);
			}
		}
	}
	if (!(scm->cs[0].csmr & CSMR_V)) {
		BusDevice *cs0_dev = scm->cs[0].dev;
		if (cs0_dev && (ips_addr > 0)) {
			Mem_AreaAddMapping(cs0_dev, 0, ips_addr,
					   MEM_FLAG_WRITABLE | MEM_FLAG_READABLE);
		}
		if (cs0_dev && (ips_addr < 3 * GIGA)) {
			uint32_t start = ips_addr + GIGA;
			uint32_t size = 0 - start;
			Mem_AreaAddMapping(cs0_dev, start, size,
					   MEM_FLAG_WRITABLE | MEM_FLAG_READABLE);
		}
		return;
	}
	for (i = 0; i < 7; i++) {
		//      ChipSelect *cs = &scm->cs[i];
	}
}

static uint32_t
csar_read(void *clientData, uint32_t address, int rqlen)
{
	ChipSelect *cs = (ChipSelect *) clientData;
	return cs->csar;
}

static void
csar_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	ChipSelect *cs = (ChipSelect *) clientData;
	cs->csar = value;
	fprintf(stderr, "CSM: csar not implemented\n");
	// update_mappings
}

static uint32_t
csmr_read(void *clientData, uint32_t address, int rqlen)
{
	ChipSelect *cs = (ChipSelect *) clientData;
	return cs->csmr;
}

static void
csmr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	ChipSelect *cs = (ChipSelect *) clientData;
	cs->csmr = value;
	fprintf(stderr, "CSM: csmr not implemented\n");
	// update_mappings
}

static uint32_t
cscr_read(void *clientData, uint32_t address, int rqlen)
{
	ChipSelect *cs = (ChipSelect *) clientData;
	if (rqlen == 4) {
		return cs->cscr << 16;
	} else if ((rqlen == 2) && ((address & 2) == 2)) {
		return cs->cscr;
	}
	return 0;
}

static void
cscr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	ChipSelect *cs = (ChipSelect *) clientData;
	if (rqlen == 4) {
		cs->cscr = value >> 16;
	} else if ((rqlen == 2) && ((address & 2) == 2)) {
		cs->cscr = value;
	}
}

/*
 *************************************************************
 * IPSBAR selects the  base address of a 1GB memory space
 * associated with the on-chip peripherals
 *************************************************************
 */
static uint32_t
ipsbar_read(void *clientData, uint32_t address, int rqlen)
{
	Scm *scm = (Scm *) clientData;
	return scm->ipsbar;
}

static void
ipsbar_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	Scm *scm = (Scm *) clientData;
	fprintf(stderr, "IPSBAR write: value %08x\n", value);
	if (scm->ipsbar != value) {
		Scm_UpdateMappings(scm);
	}
	scm->ipsbar = value;
}

/*
 ***********************************************************************
 * RAMBAR SRAM base address as seen by peripherals.
 * CPU may see a different address and has its own RAMBAR register
 ***********************************************************************
 */
static uint32_t
rambar_read(void *clientData, uint32_t address, int rqlen)
{
	Scm *scm = (Scm *) clientData;
	return scm->rambar;
}

static void
rambar_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	Scm *scm = (Scm *) clientData;
	fprintf(stderr, "Write RAMBAR address %08x value %08x, len %d\n", address, value, rqlen);
	scm->rambar = value;

}

/*
 ******************************************************************
 * Core reset status register	  		(CRSR)
 * Core watchdog control register 		(CWCR)
 * Core watchdog service register 		(CWSR)
 * Low Power Interrupt status Control register  (LPICR) 
 ******************************************************************
 */
static uint32_t
crsr_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "SCM crsr not implented\n");
	return 0;
}

static void
crsr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "SCM crsr not implented\n");
}

/*
 **************************************************************
 * Bus Master Park Register (System bus arbitration)
 **************************************************************
 */

static uint32_t
mpark_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "SCM mpark not implented\n");
	return 0;
}

static void
mpark_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "SCM mpark not implented\n");
}

/*
 ****************************************
 * System access control Unit (SACU)
 ****************************************
 */

/*
 *****************************************************
 * MPR Master Privilege Register 
 *****************************************************
 */
static uint32_t
mpr_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "SCM mpr not implented\n");
	return 0;
}

static void
mpr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "SCM mpr not implented\n");
}

/*
 **************************************************
 * Periperal Access Control Registers
 **************************************************
 */
static uint32_t
pacr0_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "SCM pacr0 not implented\n");
	return 0;
}

static void
pacr0_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "SCM pacr0 not implented\n");
}

static uint32_t
pacr4_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "SCM pacr4 not implented\n");
	return 0;
}

static void
pacr4_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "SCM pacr4 not implented\n");
}

static uint32_t
pacr7_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "SCM pacr7 not implented\n");
	return 0;
}

static void
pacr7_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "SCM pacr7 not implented\n");
}

/*
 **********************************************************
 * Grouped Peripheral Access Control Registers
 **********************************************************
 */
static uint32_t
gpacr0_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "SCM gpacr0 not implented\n");
	return 0;
}

static void
gpacr0_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "SCM gpacr0 not implented\n");
}

static void
Scm_Unmap(void *owner, uint32_t base, uint32_t mask)
{
	int i;
	IOH_Delete32(SCM_IPSBAR(base));
	IOH_Delete32(SCM_RAMBAR(base));
	IOH_Delete32(SCM_CRSR(base));
	IOH_Delete32(SCM_MPARK(base));
	IOH_Delete32(SCM_MPR(base));
	IOH_Delete32(SCM_PACR0(base));
	IOH_Delete32(SCM_PACR4(base));
	IOH_Delete32(SCM_PACR7(base));
	IOH_Delete32(SCM_GPACR0(base));
	for (i = 0; i < 7; i++) {
		IOH_Delete32(CSM_CSAR(base, i));
		IOH_Delete32(CSM_CSMR(base, i));
		IOH_Delete32(CSM_CSCR(base, i));
	}
}

static void
Scm_Map(void *owner, uint32_t base, uint32_t mask, uint32_t mapflags)
{
	int i;
	Scm *scm = (Scm *) owner;
	IOH_New32(SCM_IPSBAR(base), ipsbar_read, ipsbar_write, scm);
	IOH_New32(SCM_RAMBAR(base), rambar_read, rambar_write, scm);
	IOH_New32(SCM_CRSR(base), crsr_read, crsr_write, scm);
	IOH_New32(SCM_MPARK(base), mpark_read, mpark_write, scm);
	IOH_New32(SCM_MPR(base), mpr_read, mpr_write, scm);
	IOH_New32(SCM_PACR0(base), pacr0_read, pacr0_write, scm);
	IOH_New32(SCM_PACR4(base), pacr4_read, pacr4_write, scm);
	IOH_New32(SCM_PACR7(base), pacr7_read, pacr7_write, scm);
	IOH_New32(SCM_GPACR0(base), gpacr0_read, gpacr0_write, scm);
	for (i = 0; i < 7; i++) {
		IOH_New32(CSM_CSAR(base, i), csar_read, csar_write, &scm->cs[i]);
		IOH_New32(CSM_CSMR(base, i), csmr_read, csmr_write, &scm->cs[i]);
		IOH_New32(CSM_CSCR(base, i), cscr_read, cscr_write, &scm->cs[i]);
	}
}

/*
 ****************************************************************************
 * Register an internal device which has a address relative to IPSBAR
 ****************************************************************************
 */
void
MCF5282Scm_RegisterIpsbarDevice(Scm * scm, BusDevice * bdev, uint32_t ipsbar_offset)
{
	int i;
	for (i = 0; i < MAX_IPSDEVS; i++) {
		if (scm->ipsdev[i] == NULL) {
			scm->ipsdev[i] = bdev;
			scm->ipsbar_offset[i] = ipsbar_offset;
			break;
		}
	}
	if (i == MAX_IPSDEVS) {
		fprintf(stderr, "Not enough room for IPSDEVS, increment MAX_IPSDEVS\n");
		exit(1);
	}
	/* To lazy to add only a single mapping for the new device */
	Scm_UpdateMappings(scm);
}

void
MCF5282Scm_RegisterRambarDevice(Scm * scm, BusDevice * bdev)
{
	if (scm->rambar_dev) {
		fprintf(stderr, "RAMBAR device already registered\n");
		exit(1);
	}
	scm->rambar_dev = bdev;
	fprintf(stderr, "Mapping RAMBAR device not implemented\n");
	//Scm_UpdateMappings(scm);
}

void
MCF5282Csm_RegisterDevice(Scm * scm, BusDevice * dev, unsigned int cs_nr)
{
	ChipSelect *cs = &scm->cs[cs_nr];
	if (cs_nr >= 7) {
		fprintf(stderr, "Illegal Chip select %d\n", cs_nr);
		exit(1);
	}
	cs = &scm->cs[cs_nr];
	cs->dev = dev;
	Scm_UpdateMappings(scm);
}

MCF5282ScmCsm *
MCF5282_ScmCsmNew(const char *name)
{
	Scm *scm = sg_calloc(sizeof(Scm));
	scm->ipsbar = 0x40000000;
	scm->rambar = 0;
#if 0
	scm->crsr;
	scm->cwcr;
	scm->lpicr;
	scm->cwsr;
	scm->mpark;
	scm->mpr;
	scm->pacr0;
	scm->pacr1;
	scm->pacr2;
	scm->pacr3;
	scm->pacr4;
	scm->pacr5;
	scm->pacr6;
	scm->pacr7;
	scm->pacr8;
	scm->gpacr0;
	scm->gpacr1;
#endif
	scm->bdev.first_mapping = NULL;
	scm->bdev.Map = Scm_Map;
	scm->bdev.UnMap = Scm_Unmap;
	scm->bdev.owner = scm;
	scm->bdev.hw_flags = MEM_FLAG_WRITABLE | MEM_FLAG_READABLE;
	MCF5282Scm_RegisterIpsbarDevice(scm, &scm->bdev, 0);
	return scm;
}
