/*
 ****************************************************************************************************
 *
 * Emulation of the AT91RM9200 Memory Controller (MC)
 *
 *  State: working 
 *
 * Copyright 2006 Jochen Karrer. All rights reserved.
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

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "bus.h"
#include "at91_mc.h"
#include "arm9cpu.h"
#include "sgstring.h"

#define MC_RCR(base)	((base)+0x00)
#define		RCR_RCB		(1<<0)
#define MC_ASR(base)	((base)+0x04)
#define MC_AASR(base)	((base)+0x08)
#define	MC_MPR(base)	((base)+0xc)

#define MC_MAGIC 74894519
#define NR_MANAGED_DEVS (11)

typedef struct AT91Mc {
	BusDevice bdev;
	BusDevice *managed_devs[NR_MANAGED_DEVS];
	int bms;
	int remap;
	uint32_t asr;
	uint32_t aasr;
	uint32_t mpr;
} AT91Mc;

static void
rebuild_map(AT91Mc * mc)
{
	int i;
	BusDevice *bdev;
	BusDevice *sram, *exmem, *irom;
	for (i = 0; i < NR_MANAGED_DEVS; i++) {
		bdev = mc->managed_devs[i];
		if (bdev) {
			Mem_AreaDeleteMappings(bdev);
		}
	}
	exmem = mc->managed_devs[AT91_AREA_EXMEM0];
	sram = mc->managed_devs[AT91_AREA_SRAM];
	irom = mc->managed_devs[AT91_AREA_IROM];
	if (exmem) {
		if (!mc->remap && (mc->bms == 0)) {
			Mem_AreaAddMapping(exmem, 0x00000000, 0x00100000,
					   MEM_FLAG_WRITABLE | MEM_FLAG_READABLE);
		}
		Mem_AreaAddMapping(exmem, 0x10000000, 0x10000000,
				   MEM_FLAG_WRITABLE | MEM_FLAG_READABLE);
	}
	if (sram) {
		Mem_AreaAddMapping(sram, 0x00200000, 0x00100000,
				   MEM_FLAG_WRITABLE | MEM_FLAG_READABLE);
		if (mc->remap) {
			Mem_AreaAddMapping(sram, 0x00000000, 0x00100000,
					   MEM_FLAG_WRITABLE | MEM_FLAG_READABLE);
		}
	}
	if (irom) {
		if (!mc->remap && (mc->bms != 0)) {
			Mem_AreaAddMapping(irom, 0x00000000, 0x00100000,
					   MEM_FLAG_WRITABLE | MEM_FLAG_READABLE);
		}
		Mem_AreaAddMapping(irom, 0x00100000, 0x00100000,
				   MEM_FLAG_WRITABLE | MEM_FLAG_READABLE);
	}
	for (i = 0; i < 7; i++) {
		BusDevice *csdev;
		csdev = mc->managed_devs[AT91_AREA_CS0 + i];
		if (csdev) {
			Mem_AreaAddMapping(csdev, (i + 1) * 0x10000000, 0x10000000,
					   MEM_FLAG_WRITABLE | MEM_FLAG_READABLE);
		}
	}

}

static uint32_t
rcr_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "AT91Mc: RCR register is write only\n");
	return 0;
}

static void
rcr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	AT91Mc *mc = (AT91Mc *) clientData;
	if (value & RCR_RCB) {
		mc->remap = !mc->remap;
		rebuild_map(mc);
	}
}

/*
 * --------------------------------------------------------
 * Abort status register
 * --------------------------------------------------------
 */
static uint32_t
asr_read(void *clientData, uint32_t address, int rqlen)
{
	AT91Mc *mc = (AT91Mc *) clientData;
	return mc->asr;
}

static void
asr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "AT91Mc: ASR register is read only\n");
}

static uint32_t
aasr_read(void *clientData, uint32_t address, int rqlen)
{
	AT91Mc *mc = (AT91Mc *) clientData;
	return mc->aasr;
}

static void
aasr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "AT91Mc: AASR register is read only\n");
}

/*
 * --------------------------------------------------------
 * Master priority register
 * --------------------------------------------------------
 */
static uint32_t
mpr_read(void *clientData, uint32_t address, int rqlen)
{
	AT91Mc *mc = (AT91Mc *) clientData;
	return mc->mpr;
}

static void
mpr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	AT91Mc *mc = (AT91Mc *) clientData;
	mc->mpr = value & 0x00007777;
}

static void
AT91Mc_Map(void *owner, uint32_t base, uint32_t mask, uint32_t flags)
{
	AT91Mc *mc = (AT91Mc *) owner;
	IOH_New32(MC_RCR(base), rcr_read, rcr_write, mc);
	IOH_New32(MC_ASR(base), asr_read, asr_write, mc);
	IOH_New32(MC_AASR(base), aasr_read, aasr_write, mc);
	IOH_New32(MC_MPR(base), mpr_read, mpr_write, mc);
}

static void
AT91Mc_UnMap(void *owner, uint32_t base, uint32_t mask)
{
	IOH_Delete32(MC_RCR(base));
	IOH_Delete32(MC_ASR(base));
	IOH_Delete32(MC_AASR(base));
	IOH_Delete32(MC_MPR(base));
}

/*
 * -----------------------------------------------------------------------------
 * AT91Mc currently only controls the areas which can be remapped
 * -----------------------------------------------------------------------------
 */
void
AT91Mc_RegisterDevice(BusDevice * mcdev, BusDevice * bdev, unsigned int area_id)
{
	AT91Mc *mc = mcdev->owner;
	if (mcdev->magic != MC_MAGIC) {
		fprintf(stderr, "AT91Mc: Type check error: mcdev is not a memory controller\n");
		exit(1);
	}
	if (area_id >= NR_MANAGED_DEVS) {
		fprintf(stderr, "Illegal area id in AT91Mc_RegisterDevice\n");
		exit(1);
	}
	mc->managed_devs[area_id] = bdev;
	rebuild_map(mc);
}

BusDevice *
AT91Mc_New(const char *name)
{
	AT91Mc *mc = sg_new(AT91Mc);
	mc->bdev.first_mapping = NULL;
	mc->bdev.Map = AT91Mc_Map;
	mc->bdev.UnMap = AT91Mc_UnMap;
	mc->bdev.owner = mc;
	mc->bdev.magic = MC_MAGIC;
	mc->bdev.hw_flags = MEM_FLAG_WRITABLE | MEM_FLAG_READABLE;
	mc->bms = 0;		/* Should be read from config file */
	mc->mpr = 0x3210;
	mc->asr = 0;
	mc->aasr = 0;
	fprintf(stderr, "AT91RM9200 MC \"%s\" created\n", name);
	return &mc->bdev;

}
