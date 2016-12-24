/*
 *************************************************************************************************
 *
 * Memory and IO-Memory Access for 32 Bit bus with
 * IO-Handler registration and Translation Tables from Targets Physical
 * address (TPA) to hosts virtual Address (HVA)
 *
 *  Status:
 *	working 
 *
 * Copyright 2004 Jochen Karrer. All rights reserved.
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
#include "compiler_extensions.h"
#include "bus.h"

#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include "sgstring.h"
#include "loader.h"

Bus *MainBus;
/*
 * ------------------------------------
 * Hash for single IO-Ports
 * ------------------------------------
 */

IOHandler **iohandlerHash;

/*
 * ------------------------------------
 * Map for large IO-Windows 
 * ------------------------------------
 */

IOHandler **iohandlerMap;

/*
 * ------------------------------------
 * Two level Map for small IO-Windows 
 * ------------------------------------
 */

IOHandler ***iohandlerFlvlMap;
static unsigned int ioh_flvl_use_count[IOH_FLVL_SZ] = { 0, };

/*
 * -------------------------------------------
 * One level memory translation table vars 
 * -------------------------------------------
 */

uint8_t **mem_map_read;
uint8_t **mem_map_write;

/* 
 * -----------------------------------------
 * Two level memory translation table vars 
 * -----------------------------------------
 */

TwoLevelMMap twoLevelMMap;
InvalidateCallback *InvalidateProc;

static inline uint8_t *
twolevel_translate_r(uint32_t addr)
{
	int index;
	uint8_t *base;
	uint8_t **slvl_map;
	index = addr >> twoLevelMMap.frst_lvl_shift;
	if (unlikely(!(slvl_map = twoLevelMMap.flvlmap_read[index]))) {
		return NULL;
	}
	index = (addr & twoLevelMMap.scnd_lvl_mask) >> twoLevelMMap.scnd_lvl_shift;
	base = slvl_map[index];
	if (likely(base)) {
		return base + (addr & (twoLevelMMap.scnd_lvl_blockmask));
	} else {
		return NULL;
	}
}

static inline uint8_t *
twolevel_translate_w(uint32_t addr)
{
	int index;
	uint8_t *base;
	uint8_t **slvl_map;
	index = addr >> twoLevelMMap.frst_lvl_shift;
	if (unlikely(!(slvl_map = twoLevelMMap.flvlmap_write[index]))) {
		return NULL;
	}
	index = (addr & twoLevelMMap.scnd_lvl_mask) >> twoLevelMMap.scnd_lvl_shift;
	base = slvl_map[index];
	if (likely(base)) {
		if (unlikely(((unsigned long)base) & PG_TRACED)) {
			base -= PG_TRACED;
			slvl_map[index] = base;
			IO_Write8(0, addr);
		}
		return base + (addr & (twoLevelMMap.scnd_lvl_blockmask));
	} else {
		return NULL;
	}
}

/*
 * -----------------------------------------------
 * Bus Read
 *	Read from Memory or call an MMIO handler
 * -----------------------------------------------
 */

uint64_t
Bus_Read64(uint32_t addr)
{
	uint32_t index = (addr >> MEM_MAP_SHIFT);
	uint8_t *base = mem_map_read[index];
	if (likely(base)) {
		return HMemRead64(base + (addr & (MEM_MAP_BLOCKMASK)));
	} else {
		uint8_t *taddr = twolevel_translate_r(addr);
		if (taddr) {
			return HMemRead64(taddr);
		}
		return IO_Read64(addr);
	}
}

uint32_t
Bus_Read32(uint32_t addr)
{
	uint32_t index = (addr >> MEM_MAP_SHIFT);
	uint8_t *base = mem_map_read[index];
	if (likely(base)) {
		return HMemRead32(base + (addr & (MEM_MAP_BLOCKMASK)));
	} else {
		uint8_t *taddr = twolevel_translate_r(addr);
		if (taddr) {
			return HMemRead32(taddr);
		}
		return IO_Read32(addr);
	}
}

uint16_t
Bus_Read16(uint32_t addr)
{
	uint32_t index = (addr >> MEM_MAP_SHIFT);
	uint8_t *base = mem_map_read[index];
	if (likely(base)) {
		return HMemRead16(base + (addr & (MEM_MAP_BLOCKMASK)));
	} else {
		uint8_t *taddr = twolevel_translate_r(addr);
		if (taddr) {
			return HMemRead16(taddr);
		}
		return IO_Read16(addr);
	}
}

uint8_t
Bus_Read8(uint32_t addr)
{
	uint32_t index = (addr >> MEM_MAP_SHIFT);
	uint8_t *base = mem_map_read[index];
	if (likely(base)) {
		return HMemRead8(base + (addr & (MEM_MAP_BLOCKMASK)));
	} else {
		uint8_t *taddr = twolevel_translate_r(addr);
		if (taddr) {
			return HMemRead8(taddr);
		}
		return IO_Read8(addr);
	}
}

/*
 * --------------------------------------------
 * Bus Read
 *	Write to Memory or call an MMIO handler
 * --------------------------------------------
 */
void
Bus_Write64(uint64_t value, uint32_t addr)
{
	uint32_t index = (addr >> MEM_MAP_SHIFT);
	uint8_t *base = mem_map_write[index];
	if (likely(base)) {
		HMemWrite64(value, (base + (addr & (MEM_MAP_BLOCKMASK))));
	} else {
		uint8_t *taddr = twolevel_translate_w(addr);
		if (taddr) {
			return HMemWrite64(value, taddr);
		}
		//return IO_Write64(value,addr);
		return;
	}
}

void
Bus_Write32(uint32_t value, uint32_t addr)
{
	uint32_t index = (addr >> MEM_MAP_SHIFT);
	uint8_t *base = mem_map_write[index];
	if (likely(base)) {
		HMemWrite32(value, (base + (addr & (MEM_MAP_BLOCKMASK))));
	} else {
		uint8_t *taddr = twolevel_translate_w(addr);
		if (taddr) {
			return HMemWrite32(value, taddr);
		}
		return IO_Write32(value, addr);
	}
}

void
Bus_Write16(uint16_t value, uint32_t addr)
{
	uint32_t index = (addr >> MEM_MAP_SHIFT);
	uint8_t *base = mem_map_write[index];
	if (likely(base)) {
		HMemWrite16(value, (base + (addr & (MEM_MAP_BLOCKMASK))));
	} else {
		uint8_t *taddr = twolevel_translate_w(addr);
		if (taddr) {
			return HMemWrite16(value, taddr);
		}
		return IO_Write16(value, addr);
	}
}

void
Bus_Write8(uint8_t value, uint32_t addr)
{
	uint32_t index = (addr >> MEM_MAP_SHIFT);
	uint8_t *base = mem_map_write[index];
	if (likely(base)) {
		HMemWrite8(value, (base + (addr & (MEM_MAP_BLOCKMASK))));
	} else {
		uint8_t *taddr = twolevel_translate_w(addr);
		if (taddr) {
			return HMemWrite8(value, taddr);
		}
		return IO_Write8(value, addr);
	}
}

/*
 * --------------------------------------------
 * Generic Bus Access Functions for Transfer
 * of any block size. Mainly used for
 * non CPU bus masters.
 * --------------------------------------------
 */

void
Bus_Write(uint32_t addr, uint8_t * buf, uint32_t count)
{
	while (count) {
		Bus_Write8(*buf++, addr++);
		count--;
	}
}

void
Bus_WriteSwap32(uint32_t addr, uint8_t * buf, int count)
{
	while (count) {
		Bus_Write8(*buf++, (addr ^ 3));
		addr++;
		count--;
	}
}

void
Bus_Read(uint8_t * buf, uint32_t addr, uint32_t count)
{
	while (count) {
		*buf = Bus_Read8(addr++);
		buf++;
		count--;
	}
}

void
Bus_ReadSwap32(uint8_t * buf, uint32_t addr, int count)
{
	while (count) {
		*buf = Bus_Read8(addr ^ 3);
		addr++;
		buf++;
		count--;
	}
}

/*
 * ------------------------------------------------------------
 * Map/Unmap single 1MB Blocks 
 * ------------------------------------------------------------
 */
static void
Mem_MapBlock(uint32_t addr, uint8_t * host_mem, uint32_t flags)
{
	uint32_t index = (addr >> MEM_MAP_SHIFT);
	if (flags & MEM_FLAG_READABLE) {
		mem_map_read[index] = host_mem;
	}
	if (flags & MEM_FLAG_WRITABLE) {
		mem_map_write[index] = host_mem;
	}
}

/*
 * --------------------------------------------
 * Unmap a (large) block
 * If nothing is mapped return 0 else 1
 * --------------------------------------------
 */
static inline int
Mem_UnMapBlock(uint32_t addr)
{
	uint32_t index = (addr >> MEM_MAP_SHIFT);
	if (mem_map_read[index] || mem_map_write[index]) {
		mem_map_read[index] = mem_map_write[index] = NULL;
		return 1;
	} else {
		return 0;
	}
}

/*
 * ------------------------------------------------------------
 * Map/Unmap variable sized blocks in 2-Level Translation table 
 * ------------------------------------------------------------
 */

static void
Mem_Map2LvlBlock(uint32_t addr, uint8_t * host_mem, uint32_t flags)
{
	uint8_t **slvl_map_w;
	uint8_t **slvl_map_r;
	int index = addr >> twoLevelMMap.frst_lvl_shift;
	int sl_index = (addr & twoLevelMMap.scnd_lvl_mask) >> twoLevelMMap.scnd_lvl_shift;
	if (flags & MEM_FLAG_READABLE) {
		slvl_map_r = twoLevelMMap.flvlmap_read[index];
		if (unlikely(!slvl_map_r)) {
			slvl_map_r = twoLevelMMap.flvlmap_read[index] =
			    sg_calloc(sizeof(uint8_t *) * twoLevelMMap.scnd_lvl_sz);
		}
		twoLevelMMap.flvl_map_read_use_count[index]++;
		slvl_map_r[sl_index] = host_mem;
	}
	if (flags & MEM_FLAG_WRITABLE) {
		slvl_map_w = twoLevelMMap.flvlmap_write[index];
		if (unlikely(!slvl_map_w)) {
			slvl_map_w = twoLevelMMap.flvlmap_write[index] =
			    sg_calloc(sizeof(uint8_t *) * twoLevelMMap.scnd_lvl_sz);
		}
		twoLevelMMap.flvl_map_write_use_count[index]++;
		slvl_map_w[sl_index] = host_mem;
	}

}

static void
Mem_UnMap2LvlBlock(uint32_t addr)
{
	uint8_t **slvl_map_w;
	uint8_t **slvl_map_r;
	int index = addr >> twoLevelMMap.frst_lvl_shift;
	int sl_index = (addr & twoLevelMMap.scnd_lvl_mask) >> twoLevelMMap.scnd_lvl_shift;
	slvl_map_r = twoLevelMMap.flvlmap_read[index];
	slvl_map_w = twoLevelMMap.flvlmap_write[index];
	if (slvl_map_r) {
		if (slvl_map_r[sl_index]) {
			slvl_map_r[sl_index] = NULL;
			twoLevelMMap.flvl_map_read_use_count[index]--;
			if (twoLevelMMap.flvl_map_read_use_count[index] == 0) {
				free(slvl_map_r);
				slvl_map_r = twoLevelMMap.flvlmap_read[index] = NULL;
			}
		}
	}
	if (slvl_map_w) {
		if (slvl_map_w[sl_index]) {
			slvl_map_w[sl_index] = NULL;
			twoLevelMMap.flvl_map_write_use_count[index]--;
			if (twoLevelMMap.flvl_map_write_use_count[index] == 0) {
				free(slvl_map_w);
				slvl_map_w = twoLevelMMap.flvlmap_write[index] = NULL;
			}
		}
	}
}

/*
 * ----------------------------------------------------------
 * Add a mapping to the linked list of mappings of device
 * -----------------------------------------------------------
 */
void
Mem_AreaAddMapping(BusDevice * bdev, uint32_t base, uint32_t mapsize, uint32_t flags)
{
	MemMapping *mapping = sg_new(MemMapping);
	mapping->next = bdev->first_mapping;
	bdev->first_mapping = mapping;
	mapping->base = base;
	mapping->mapsize = mapsize;
	mapping->flags = flags & bdev->hw_flags;

	bdev->Map(bdev->owner, base, mapping->mapsize, flags);
	/* Check for traces ??? */
	/* Need to Invalidate Tlb because we cache Host Virtual Addresses */
	if (InvalidateProc) {
		InvalidateProc();
	}
}

/*
 * ---------------------------------------------------------------------------------
 * Map a range
 *	Forwards all decoded bits (devsize-1) to the device
 *	This function may only called by device emulators in response to
 *	a map request. 
 * ---------------------------------------------------------------------------------
 */
void
Mem_MapRange(uint32_t base, uint8_t * start_mem, uint32_t devsize, uint32_t mapsize, uint32_t flags)
{
	uint32_t count = 0;
	uint32_t addr;
	uint32_t pos = base & (devsize - 1);
	uint8_t *mem = start_mem + pos;
    //fprintf(stderr, "Mapping addr %08x mapsize %u, devsize %u, flags %08lx\n", base, mapsize , devsize,flags);
	for (addr = base; 1;) {
		if ((devsize >= MEM_MAP_BLOCKSIZE) && (mapsize >= MEM_MAP_BLOCKSIZE)
		    && !(addr & MEM_MAP_BLOCKMASK)) {
            //fprintf(stderr, "Mapping block %08x, mem %p\n", addr, mem);
			Mem_MapBlock(addr, mem, flags);
			mem += MEM_MAP_BLOCKSIZE;
			pos += MEM_MAP_BLOCKSIZE;
			count += MEM_MAP_BLOCKSIZE;
			addr += MEM_MAP_BLOCKSIZE;
			mapsize -= MEM_MAP_BLOCKSIZE;
		} else if ((mapsize >= twoLevelMMap.scnd_lvl_blocksize)
			   && !(addr & twoLevelMMap.scnd_lvl_blockmask)) {
            //fprintf(stderr, "Mapping 2lvl block %08x\n", addr);
			Mem_Map2LvlBlock(addr, mem, flags);
			mem += twoLevelMMap.scnd_lvl_blocksize;
			pos += twoLevelMMap.scnd_lvl_blocksize;
			count += twoLevelMMap.scnd_lvl_blocksize;
			addr += twoLevelMMap.scnd_lvl_blocksize;
			mapsize -= twoLevelMMap.scnd_lvl_blocksize;
		} else {
			if (mapsize) {
				fprintf(stderr, "Bus: Can not map completely, rest: %08x, addr %08x"
					", twoLevelMMap.scnd_lvl_blockmask %08x blocksize %08x\n",
					mapsize, addr, twoLevelMMap.scnd_lvl_blockmask,
					twoLevelMMap.scnd_lvl_blocksize);
				exit(3789);
			}
			return;
		}
		if (pos >= devsize) {
			pos = 0;
			mem = start_mem;
		}
		/* Special case 4GB */
		if (!count) {
			return;
		}
	}
}

static void
Mem_SplitLargePage(uint32_t pgaddr)
{
	uint32_t addr = pgaddr & ~MEM_MAP_BLOCKMASK;
	int index = (addr >> MEM_MAP_SHIFT);
	uint8_t *rhva = mem_map_read[index];
	uint8_t *whva = mem_map_write[index];
	uint8_t *hva = 0;
	uint32_t slvl_size = twoLevelMMap.scnd_lvl_blocksize;
	int flags;
	int count;
	flags = 0;
	if (!rhva && !whva) {
		/* nothing to split */
		return;
	}
	if ((rhva != whva) && (rhva != 0) && (whva != 0)) {
		fprintf(stderr, "Can not split page with different read/write addresses\n");
		return;
	}
	if (rhva) {
		flags |= MEM_FLAG_READABLE;
		hva = rhva;
	}
	if (whva) {
		flags |= MEM_FLAG_WRITABLE;
		hva = whva;
	}
	if (!Mem_UnMapBlock(addr)) {
		fprintf(stderr, "Failed to unmap block at %08x\n", addr);
	}
	for (count = 0; count < MEM_MAP_BLOCKSIZE; count += slvl_size) {
		Mem_Map2LvlBlock(addr + count, hva + count, flags);
	}
}

void
Mem_TracePage(uint32_t pgaddr)
{
	int index;
	uint8_t *hva;
	uint8_t **slvl_map;
	Mem_SplitLargePage(pgaddr);
	index = pgaddr >> twoLevelMMap.frst_lvl_shift;
	if (unlikely(!(slvl_map = twoLevelMMap.flvlmap_write[index]))) {
		fprintf(stderr, "no slvl map for addr %08x\n", pgaddr);	// jk
		return;
	}
	index = (pgaddr & twoLevelMMap.scnd_lvl_mask) >> twoLevelMMap.scnd_lvl_shift;
	hva = slvl_map[index];
	if (hva && !((unsigned long)hva & PG_TRACED)) {

		slvl_map[index] += PG_TRACED;
	} else {
		fprintf(stderr, "Page is already traced %08x hva %p %08x\n", pgaddr, hva,
			!((unsigned long)hva & PG_TRACED));
	}
	if (InvalidateProc) {
		InvalidateProc();
	}
}

void
Mem_UntracePage(uint32_t pgaddr)
{
	int index;
	uint8_t *hva;
	uint8_t **slvl_map;
	index = pgaddr >> twoLevelMMap.frst_lvl_shift;
	if (unlikely(!(slvl_map = twoLevelMMap.flvlmap_write[index]))) {
		fprintf(stderr, "no slvl map for addr %08x\n", pgaddr);	// jk
		return;
	}
	index = (pgaddr & twoLevelMMap.scnd_lvl_mask) >> twoLevelMMap.scnd_lvl_shift;
	hva = slvl_map[index];
	if (hva && ((unsigned long)hva & PG_TRACED)) {
		slvl_map[index] -= PG_TRACED;
		//fprintf(stderr,"Untraced page at %08x, hva %p\n",pgaddr,slvl_map[index]);
	} else {
		fprintf(stderr, "Page was not traced %08x, hva %p\n", pgaddr, slvl_map[index]);
	}
	if (InvalidateProc) {
		InvalidateProc();
	}
}

void
Mem_TraceRegion(uint32_t start, uint32_t length)
{
	uint32_t i;
	int slvl_size = twoLevelMMap.scnd_lvl_blocksize;
	fprintf(stderr, "Trace region from %08x length %08x\n", start, length);	// jk
	for (i = 0; i < length; i += slvl_size) {
		Mem_TracePage(start + i);
	}
}

void
Mem_UntraceRegion(uint32_t start, uint32_t length)
{
	uint32_t i;
	int slvl_size = twoLevelMMap.scnd_lvl_blocksize;
	fprintf(stderr, "Untrace region from %08x length %08x\n", start, length);	// jk
	for (i = 0; i < length; i += slvl_size) {
		Mem_UntracePage(start + i);
	}
}

/*
 * --------------------------------------------------------------------
 * Take existing mapping and split up a range from large pages
 * into small pages.
 * address may not wrap and size may not be 4G
 * --------------------------------------------------------------------
 */
void
Mem_SplitLargePages(uint32_t start, uint32_t size)
{
	uint32_t addr = start & ~MEM_MAP_BLOCKMASK;
	do {
		Mem_SplitLargePage(addr);
		addr += MEM_MAP_BLOCKSIZE;

	} while ((addr - start) < size);
}

/*
 * ---------------------------------------------------------------------------------
 * UnMap a range
 *	This function may only called by device emulators in response to
 *	an unmap request. 
 * ---------------------------------------------------------------------------------
 */
void
Mem_UnMapRange(uint32_t base, uint32_t mapsize)
{
	uint32_t addr;
	uint32_t count = 0;
	for (addr = base; 1;) {
		if ((mapsize >= MEM_MAP_BLOCKSIZE) && !(addr & MEM_MAP_BLOCKMASK)) {
			if (Mem_UnMapBlock(addr) > 0) {
				count += MEM_MAP_BLOCKSIZE;
				addr += MEM_MAP_BLOCKSIZE;
				mapsize -= MEM_MAP_BLOCKSIZE;
			} else {
				Mem_UnMap2LvlBlock(addr);
				count += twoLevelMMap.scnd_lvl_blocksize;
				addr += twoLevelMMap.scnd_lvl_blocksize;
				mapsize -= twoLevelMMap.scnd_lvl_blocksize;
			}
		} else if ((mapsize >= twoLevelMMap.scnd_lvl_blocksize)
			   && !(addr & twoLevelMMap.scnd_lvl_blockmask)) {
			Mem_UnMap2LvlBlock(addr);
			count += twoLevelMMap.scnd_lvl_blocksize;
			addr += twoLevelMMap.scnd_lvl_blocksize;
			mapsize -= twoLevelMMap.scnd_lvl_blocksize;
		} else {
			return;
		}
		/* Special case 4GB */
		if (!count) {
			return;
		}
	}
}

void
Mem_AreaDeleteMappings(BusDevice * bdev)
{
	while (bdev->first_mapping) {
		MemMapping *mapping = bdev->first_mapping;
		bdev->UnMap(bdev->owner, mapping->base, mapping->mapsize);
		bdev->first_mapping = mapping->next;
		free(mapping);
	}
	/* Need to Invalidate Tlb because we cache Host Virtual Addresses */
	if (InvalidateProc) {
		InvalidateProc();
	}
}

/*
 * ----------------------------------------------------------------------
 * Update Mappings is called by a chip emulator whenever it changes
 * its memory map by itself
 * ----------------------------------------------------------------------
 */
void
Mem_AreaUpdateMappings(BusDevice * bdev)
{
	MemMapping *cursor;
	/* First do a total Cleanup then map again */
	for (cursor = bdev->first_mapping; cursor; cursor = cursor->next) {
		bdev->UnMap(bdev->owner, cursor->base, cursor->mapsize);
	}
	for (cursor = bdev->first_mapping; cursor; cursor = cursor->next) {
		bdev->Map(bdev->owner, cursor->base, cursor->mapsize, cursor->flags);
	}
	/* Need to Invalidate Tlb because we cache Host Virtual Addresses */
	if (InvalidateProc) {
		InvalidateProc();
	}
}

/*
 * -------------------------------
 * IO-Handling
 * -------------------------------
 */
#define IOH_HASH(addr) ((addr) + ((addr)>>18))&IOH_HASH_MASK

IOHandler *
IOH_Find(uint32_t address)
{
	IOHandler *cursor;
	IOHandler **slvl_map;
	uint32_t hash = IOH_HASH(address);
	uint32_t index;
	for (cursor = iohandlerHash[hash]; cursor; cursor = cursor->next) {
		if (cursor->cpu_addr == address) {
			return cursor;
		}
	}
	index = (address >> IOH_MAP_SHIFT);
	if (iohandlerMap[index]) {
		return (iohandlerMap[index]);
	}
	index = (address >> IOH_FLVL_SHIFT);
	if ((slvl_map = iohandlerFlvlMap[index])) {
		index = (address & IOH_SLVL_MASK) >> IOH_SLVL_SHIFT;
		return slvl_map[index];
	}
	//fprintf(stderr,"JK: Addr %08x no IOH slevel %p, index %d\n",address,slvl_map,index);
	return NULL;
}

IOHandler *
IOH_HashFind(uint32_t address)
{
	IOHandler *cursor;
	uint32_t hash = IOH_HASH(address);
	for (cursor = iohandlerHash[hash]; cursor; cursor = cursor->next) {
		if (cursor->cpu_addr == address) {
			return cursor;
		}
	}
	return NULL;
}

static void
IOH_New(uint32_t cpu_addr, IOReadProc * readproc, IOWriteProc * writeproc, void *clientData,
	int len, uint32_t flags)
{
	uint32_t hash;
	IOHandler *h;
	int swap_endian;
	int byteorder;
	if (flags & IOH_FLG_BIG_ENDIAN) {
		byteorder = en_BIG_ENDIAN;
	} else if (flags & IOH_FLG_HOST_ENDIAN) {
		byteorder = HOST_BYTEORDER;
	} else {
		byteorder = en_LITTLE_ENDIAN;
	}
	if (byteorder != HOST_BYTEORDER) {
		swap_endian = 1;
	} else {
		swap_endian = 0;
	}
	if (IOH_HashFind(cpu_addr)) {
		fprintf(stderr, "Bug, more than one IO-Handler for address %08x\n", cpu_addr);
		exit(4324);
	}
	h = sg_new(IOHandler);
	h->cpu_addr = cpu_addr;
	h->clientData = clientData;
	h->readproc = readproc;
	h->writeproc = writeproc;
	hash = IOH_HASH(cpu_addr);
	h->next = iohandlerHash[hash];
	h->swap_endian = swap_endian;
	h->flags = flags;
	h->len = len;
	iohandlerHash[hash] = h;
}

void
IOH_New8f(uint32_t cpu_addr, IOReadProc * readproc, IOWriteProc * writeproc, void *clientData,
	  uint32_t flags)
{
	IOH_New(cpu_addr, readproc, writeproc, clientData, 1, flags);
}

void
IOH_New16f(uint32_t cpu_addr, IOReadProc * readproc, IOWriteProc * writeproc, void *clientData,
	   uint32_t flags)
{
	if (flags & (IOH_FLG_PA_CBSE)) {
		int i;
		for (i = 0; i < 2; i++) {
			IOH_New(cpu_addr + i, readproc, writeproc, clientData, 2, flags);
		}
	} else {
		IOH_New(cpu_addr, readproc, writeproc, clientData, 2, flags);
	}
}

void
IOH_New32f(uint32_t cpu_addr, IOReadProc * readproc, IOWriteProc * writeproc, void *clientData,
	   uint32_t flags)
{
	/* Partial access: call base */
	if (flags & IOH_FLG_PA_CBSE) {
		int i;
		for (i = 0; i < 4; i++) {
			IOH_New(cpu_addr + i, readproc, writeproc, clientData, 4, flags);
		}
	} else {
		IOH_New(cpu_addr, readproc, writeproc, clientData, 4, flags);
	}
}

/*
 * -------------------------------------------------------------
 * Allocate an iohandler and initialize the structure 
 * -------------------------------------------------------------
 */
static inline IOHandler *
allocate_iohandler(uint32_t addr, IOReadProc * readproc, IOWriteProc * writeproc, int swap_endian,
		   void *clientData)
{
	IOHandler *h;
	h = sg_new(IOHandler);
	h->cpu_addr = addr;
	h->clientData = clientData;
	h->readproc = readproc;
	h->writeproc = writeproc;
	h->next = NULL;
	h->swap_endian = swap_endian;
	return h;
}

static inline void
IOH_MapBlock(uint32_t addr, IOHandler * h)
{
	uint32_t index = addr >> IOH_MAP_SHIFT;
	if (iohandlerMap[index]) {
		fprintf(stderr, "IO-Region %08x already allocated !\n", addr);
		exit(5342);
	}
	iohandlerMap[index] = h;
}

static inline void
IOH_Map2LvlBlock(uint32_t addr, IOHandler * h)
{
	IOHandler **sl_map;
	int fl_index = addr >> IOH_FLVL_SHIFT;
	int sl_index;
	sl_map = iohandlerFlvlMap[fl_index];
	if (!sl_map) {
		//fprintf(stderr,"JK: This is a new FLEVEL\n");
		sl_map = iohandlerFlvlMap[fl_index] = sg_calloc(sizeof(uint8_t *) * IOH_SLVL_SZ);
	}
	sl_index = (addr & IOH_SLVL_MASK) >> IOH_SLVL_SHIFT;
	if (!sl_map[sl_index]) {
		sl_map[sl_index] = h;
		ioh_flvl_use_count[fl_index]++;
	} else {
		fprintf(stderr, "There is already a handler for IO-Region 0x%08x\n", addr);
		exit(3246);
	}
}

void
IOH_NewRegion(uint32_t addr, uint32_t length, IOReadProc * readproc, IOWriteProc * writeproc,
	      int flags, void *clientData)
{
	IOHandler *h;
	int swap_endian;
	int byteorder;
	if (flags & IOH_FLG_BIG_ENDIAN) {
		byteorder = en_BIG_ENDIAN;
	} else if (flags & IOH_FLG_HOST_ENDIAN) {
		byteorder = HOST_BYTEORDER;
	} else {
		byteorder = en_LITTLE_ENDIAN;
	}
	if (byteorder != HOST_BYTEORDER) {
		swap_endian = 1;
	} else {
		swap_endian = 0;
	}
	while (length) {
		if (!(addr & IOH_MAP_BLOCKMASK) && (length >= IOH_MAP_BLOCKSIZE)) {
			h = allocate_iohandler(addr, readproc, writeproc, swap_endian, clientData);
			h->flags = 0;
			h->len = 4;
			IOH_MapBlock(addr, h);
			length -= IOH_MAP_BLOCKSIZE;
			addr += IOH_MAP_BLOCKSIZE;
		} else if (!(addr & IOH_SLVL_BLOCKMASK) && (length >= IOH_SLVL_BLOCKSIZE)) {
			h = allocate_iohandler(addr, readproc, writeproc, swap_endian, clientData);
			h->flags = 0;
			h->len = 4;
			IOH_Map2LvlBlock(addr, h);
			length -= IOH_SLVL_BLOCKSIZE;
			addr += IOH_SLVL_BLOCKSIZE;
		} else {
			fprintf(stderr, "io region not correctly aligned addr 0x%08x, len 0x%08x\n",
				addr, length);
			exit(34245);
		}
	}
}

static inline void
IOH_UnMapBlock(uint32_t addr)
{
	int index;
	IOHandler *handler;
	index = addr >> IOH_MAP_SHIFT;
	handler = iohandlerMap[index];
	if (handler) {
		iohandlerMap[index] = NULL;
		free(handler);
	}
}

static inline void
IOH_UnMap2LvlBlock(uint32_t addr)
{
	int fl_index;
	int sl_index;
	IOHandler *handler;
	IOHandler **sl_map;
	fl_index = addr >> IOH_FLVL_SHIFT;
	sl_map = iohandlerFlvlMap[fl_index];
	if (!sl_map) {
		return;
	}
	sl_index = (addr & IOH_SLVL_MASK) >> IOH_SLVL_SHIFT;
	handler = sl_map[sl_index];
	if (handler) {
		sl_map[sl_index] = NULL;
		free(handler);
		ioh_flvl_use_count[fl_index]--;
		if (ioh_flvl_use_count[fl_index] == 0) {
			free(iohandlerFlvlMap[fl_index]);
			iohandlerFlvlMap[fl_index] = 0;
		}
	}
}

void
IOH_DeleteRegion(uint32_t addr, uint32_t length)
{
	// fprintf(stderr,"JK Delete Region %08x size 0x%08x\n",addr,length);
	while (length) {
		if (!(addr & IOH_MAP_BLOCKMASK) && (length >= IOH_MAP_BLOCKSIZE)) {
			IOH_UnMapBlock(addr);
			length -= IOH_MAP_BLOCKSIZE;
			addr += IOH_MAP_BLOCKSIZE;
		} else if (!(addr & IOH_SLVL_BLOCKMASK) && (length >= IOH_SLVL_BLOCKSIZE)) {
			IOH_UnMap2LvlBlock(addr);
			length -= IOH_SLVL_BLOCKSIZE;
			addr += IOH_SLVL_BLOCKSIZE;

		} else {
			fprintf(stderr,
				"IOH_DeleteRegion: only IO regions with alignment to 0x%08x allowed\n",
				IOH_SLVL_BLOCKSIZE);
			return;
		}
	}

}

uint32_t
IOH_Delete(uint32_t address)
{
	IOHandler *next, *cursor, *prev = NULL;
	uint32_t hash = IOH_HASH(address);
	uint32_t flags;
	for (cursor = iohandlerHash[hash]; cursor; prev = cursor, cursor = next) {
		next = cursor->next;
		if (cursor->cpu_addr == address) {
			if (prev) {
				prev->next = cursor->next;
			} else {
				iohandlerHash[hash] = cursor->next;
			}
			flags = cursor->flags;
			free(cursor);
			return flags;
		}
	}
	return 0;
}

void
IOH_Delete8(uint32_t address)
{
	IOH_Delete(address);
}

void
IOH_Delete16(uint32_t address)
{
	int i;
	uint32_t flags;
	for (i = 0; i < 2; i++) {
		flags = IOH_Delete(address);
		if (!(flags & IOH_FLG_PA_CBSE)) {
			break;
		}
	}
}

void
IOH_Delete32(uint32_t address)
{
	int i;
	uint32_t flags;
	for (i = 0; i < 4; i++) {
		flags = IOH_Delete(address);
		if (!(flags & IOH_FLG_PA_CBSE)) {
			break;
		}
	}
}

/*
 * ---------------------------------------
 * Warning: 64 Bit write does not work
 * because writeproc is still 32Bit
 * ---------------------------------------
 */
void
IO_Write64(uint64_t value, uint32_t addr)
{
	IOHandler *h = IOH_Find(addr);
	if (!h || !h->writeproc) {
		fprintf(stderr, "Write: No Handler for %08x\n", addr);
		return;
	}
	h->writeproc(h->clientData, value, addr, 4);
	return;
}

void
IO_Write32(uint32_t value, uint32_t addr)
{
	IOHandler *h = IOH_Find(addr);
	if (!h || !h->writeproc) {
		fprintf(stderr, "Write: No Handler for %08x, value %08x\n", addr, value);
		return;
	}
    //fprintf(stderr, "write32 %08x\n",value);
	if (likely(h->len == 4)) {
		if (!h->swap_endian) {
			h->writeproc(h->clientData, value, addr, 4);
		} else {
			h->writeproc(h->clientData, swap32(value), addr, 4);
		}
	} else if (likely(h->len == 2)) {
		if (h->swap_endian) {
			value = swap32(value);
		}
		h->writeproc(h->clientData, value, addr, 2);
		if (h->flags & IOH_FLG_OSZW_NEXT) {
			h = IOH_Find(addr + 2);
			if (h && h->writeproc) {
				h->writeproc(h->clientData, value >> 16, addr, 2);
			}
		}
	} else if (likely(h->len == 1)) {
		if (h->swap_endian) {
			value = swap32(value);
		}
		if (h->flags & IOH_FLG_OSZW_NEXT) {
			fprintf(stderr, "Please implement %s, line %d\n", __FILE__, __LINE__);
		} else {
			h->writeproc(h->clientData, value, addr, 4);
		}
	}
	return;
}

void
IO_Write16(uint16_t value, uint32_t addr)
{
	IOHandler *h = IOH_Find(addr);
	if (!h || !h->writeproc) {
		//fprintf(stderr,"No handler for %08x\n",addr);
		return;
	}
    //fprintf(stderr, "write16 %08x\n",value);
	if (likely(h->len >= 2)) {
		if (!h->swap_endian) {
			h->writeproc(h->clientData, value, addr, 2);
		} else {
			h->writeproc(h->clientData, swap16(value), addr, 2);
		}
	} else if (h->len == 1) {
		if (h->swap_endian) {
			value = swap16(value);
		}
		h->writeproc(h->clientData, value, addr, 1);
		if (h->flags & IOH_FLG_OSZW_NEXT) {
			h = IOH_Find(addr + 1);
			if (h) {
				h->writeproc(h->clientData, value >> 8, addr + 1, 1);
			}
		}
	}
	return;
}

//include "cpu_m32c.h"
void
IO_Write8(uint8_t value, uint32_t addr)
{
	IOHandler *h = IOH_Find(addr);
    uint32_t val32; 
    //fprintf(stderr, "write8 %08x: %08x\n",addr, value);
	if (!h || !h->writeproc) {
		//fprintf(stderr, "No iohandler for %08x, %08x\n", addr,M32C_REG_PC);
		fprintf(stderr, "No iohandler for %08x\n", addr);
		return;
	}
	if (likely(h->len == 1)) {
        val32 = value;
    } else if (h->len == 2) {
		if (h->swap_endian ^ (addr & 1)) {
			val32 = swap16((uint16_t)value);
		} else {
            val32 = value;
        }
        /* Shiften wenn addresse odd ist ? */
        if(h->flags &  IOH_FLG_PWR_RMW) {
            uint32_t mask = UINT32_C(0x00FF) << ((addr & 1) << 3);
            fprintf(stderr, "RMW addr %08x before %04x, mask %04x\n", addr, value, mask);
            if(h->readproc) {
                value = value | (h->readproc(h->clientData, addr, 1) & mask);
            }
            fprintf(stderr, "RMW missing, mask %04x\n", mask);
        } 
    } else /* if(h->len == 4) */ {
		if (h->swap_endian) {
			val32 = swap32((uint32_t)value);
		} else {
            val32 = value;
        }
    }
    h->writeproc(h->clientData, val32, addr, 1);
	return;
}

/*
 * ---------------------------------------------------
 * Warning: 64 Bit read does not work, because
 * readproc returns only 32 Bit
 * ---------------------------------------------------
 */
uint64_t
IO_Read64(uint32_t addr)
{
	IOHandler *h = IOH_Find(addr);
	if (!h || !h->readproc) {
		return 0;
	}
	return h->readproc(h->clientData, addr, 8);
}

uint32_t
IO_Read32(uint32_t addr)
{
	IOHandler *h;
	uint32_t value;
	h = IOH_Find(addr);
	if (!h || !h->readproc) {
		return 0;
	}
	if (likely(h->len == 4)) {
		if (!h->swap_endian) {
			return h->readproc(h->clientData, addr, 4);
		} else {
			return swap32(h->readproc(h->clientData, addr, 4));
		}
	} else if (h->len == 2) {
		if (h->flags & IOH_FLG_OSZR_NEXT) {
			value = h->readproc(h->clientData, addr, 2);
			h = IOH_Find(addr + 2);
			if (h && h->readproc) {
				value |= h->readproc(h->clientData, addr + 2, 2) << 16;
			}
		} else {
			value = h->readproc(h->clientData, addr, 4);
		}
		if (h && h->swap_endian) {
			value = swap32(value);
		}
	} else if (h->len == 1) {
		if (h->flags & IOH_FLG_OSZR_NEXT) {
			IOHandler *tmph;
			int i;
			value = 0;
			for (i = 0; i < 4; i++) {
				tmph = IOH_Find(addr + i);
				if (tmph && tmph->readproc) {
					value |=
					    tmph->readproc(tmph->clientData, addr + i,
							   1) << (i << 3);
				}
			}
		} else {
			value = h->readproc(h->clientData, addr, 4);
		}
		if (h->swap_endian) {
			value = swap32(value);
		}
	} else {
		fprintf(stderr, "Illegal handler len %d for addr 0x%08x\n", h->len, addr);
		return 0;
	}
	return value;
}

uint16_t
IO_Read16(uint32_t addr)
{
	IOHandler *h = IOH_Find(addr);
	uint32_t value;
	if (!h || !h->readproc) {
		return 0;
	}
	if (likely(h->len == 2)) {
		value = h->readproc(h->clientData, addr, 2);
		if (h->swap_endian) {
            fprintf(stderr, "SWAP16 %04x\n",value);
			value = swap16(value);
		}
	} else if (h->len == 1) {
		if (h->flags & IOH_FLG_OSZR_NEXT) {
			value = h->readproc(h->clientData, addr, 1);
			h = IOH_Find(addr + 1);
			if (h && h->readproc) {
				value = value | h->readproc(h->clientData, addr + 1, 1) << 8;
				if (h->swap_endian) {
					value = swap16(value);
				}
			}
		} else {
			value = h->readproc(h->clientData, addr, 2);
		}
	} else if (h->len == 4) {
		if (h->flags & IOH_FLG_PRD_RARP) {
			value = h->readproc(h->clientData, addr, 2);
			value = value >> ((addr & 3) << 3);
		} else {
			value = h->readproc(h->clientData, addr, 2);
		}
		if (h->swap_endian) {
			value = swap32(value);
		}
	} else {
		fprintf(stderr, "Illegal handler len %d for addr 0x%08x\n", h->len, addr);
		return 0;
	}
	return value;
}

uint8_t
IO_Read8(uint32_t addr)
{
	IOHandler *h = IOH_Find(addr);
	uint32_t value;
	if (!h || !h->readproc) {
		return 0;
	}
	value = h->readproc(h->clientData, addr, 1);
	if (likely(h->len == 1)) {
		return value;
	} else if (h->len == 2) {
		if (h->swap_endian) {
			value = swap16(value);
            if (h->flags & IOH_FLG_PRD_RARP) {
                value = value >> (((addr ^ 1) & 1) << 3);
                return value;
            }
		} else { 
            if (h->flags & IOH_FLG_PRD_RARP) {
                value = value >> ((addr & 1) << 3);
                return value;
            }
        }
	} else if (h->len == 4) {
		if (h->swap_endian) {
			value = swap32(value);
            if (h->flags & IOH_FLG_PRD_RARP) {
                value = value >> (((addr ^ 3) & 3) << 3);
                return value;
            }
		} else {
            if (h->flags & IOH_FLG_PRD_RARP) {
                //fprintf(stderr,"Value bef %02x\n",value);
                value = value >> ((addr & 3) << 3);
                //fprintf(stderr,"Value now %02x\n",value);
                return value;
            }
        }
	}
	return value;
}

static struct Bus mainBus = {
	.read32 = Bus_Read32,
	.read16 = Bus_Read16,
	.read8 = Bus_Read8,
	.readblock = Bus_Read,
	.write32 = Bus_Write32,
	.write16 = Bus_Write16,
	.write8 = Bus_Write8,
	.writeblock = Bus_Write
};

/*
 * -----------------------------------------------------
 * The interface to the loader
 * -----------------------------------------------------
 */
static int
load_to_bus(void *clientData, uint32_t addr, uint8_t * buf, unsigned int count, int flags)
{
	uint32_t min_blocksize = Bus_GetMinBlockSize();
	uint32_t blockmask = (min_blocksize - 1);
	while (count) {
		uint32_t len = min_blocksize - (addr & blockmask);
		uint32_t offs = addr & blockmask;
		uint8_t *host_mem = Bus_GetHVAWrite(addr & ~blockmask);
		if (!host_mem) {
			host_mem = Bus_GetHVARead(addr & ~blockmask);
		}
		if (!host_mem) {
			fprintf(stderr, "Bus: Cannot load record to memory at 0x%08x\n", addr);
			return -1;
		}
		if (len > count)
			len = count;
		if (len == 0) {
			fprintf(stderr, "Error: min_blocksize 0x%08x,blockmask 0x%08x, addr %08x\n",
				min_blocksize, blockmask, addr);
			exit(1);
		}
		if (flags & LOADER_FLAG_SWAP32) {
			int i;
			for (i = 0; i < len; i++) {
				uint8_t *dst = host_mem + ((offs + i) ^ 3);
				*dst = buf[i];
			}
		} else {
			memcpy(host_mem + offs, buf, len);
		}
		count -= len;
		addr += len;
		buf += len;
	}
	return 0;
}

/*
 * --------------------------------------------------------------
 * Create and Initialize memory and IO-Handler maps and Hashes
 * --------------------------------------------------------------
 */
uint32_t
Bus_GetMinBlockSize(void)
{
	return twoLevelMMap.scnd_lvl_blocksize;
}

void
Bus_Init(InvalidateCallback * invalidate, uint32_t min_memblocksize)
{
	int i;
	InvalidateProc = invalidate;

	twoLevelMMap.frst_lvl_sz = (4096);
	twoLevelMMap.frst_lvl_mask = (0xfff00000);
	twoLevelMMap.frst_lvl_shift = (20);
	for (i = 0; i < twoLevelMMap.frst_lvl_shift; i++) {
		if ((min_memblocksize >> i) == 1) {
			break;
		}
	}
	twoLevelMMap.scnd_lvl_shift = i;
	twoLevelMMap.scnd_lvl_blocksize = min_memblocksize;
	twoLevelMMap.scnd_lvl_blockmask = min_memblocksize - 1;
	twoLevelMMap.scnd_lvl_mask = (~0 << i) & ~twoLevelMMap.frst_lvl_mask;
	twoLevelMMap.scnd_lvl_sz =
	    (1 << (twoLevelMMap.frst_lvl_shift - twoLevelMMap.scnd_lvl_shift));

	iohandlerHash = sg_calloc(sizeof(IOHandler *) * IOH_HASH_SIZE);
	mem_map_read = sg_calloc(sizeof(uint8_t *) * MEM_MAP_ENTRIES);
	mem_map_write = sg_calloc(sizeof(uint8_t *) * MEM_MAP_ENTRIES);

	twoLevelMMap.flvlmap_read = sg_calloc(sizeof(uint8_t *) * twoLevelMMap.frst_lvl_sz);
	twoLevelMMap.flvlmap_write = sg_calloc(sizeof(uint8_t *) * twoLevelMMap.frst_lvl_sz);
	twoLevelMMap.flvl_map_read_use_count =
	    sg_calloc(sizeof(*twoLevelMMap.flvl_map_read_use_count) * twoLevelMMap.frst_lvl_sz);
	twoLevelMMap.flvl_map_write_use_count =
	    sg_calloc(sizeof(*twoLevelMMap.flvl_map_read_use_count) * twoLevelMMap.frst_lvl_sz);

	iohandlerMap = sg_calloc(sizeof(IOHandler *) * IOH_MAP_ENTRIES);
	iohandlerFlvlMap = sg_calloc(sizeof(IOHandler **) * IOH_FLVL_SZ);
	MainBus = &mainBus;
	Loader_RegisterBus("bus", load_to_bus, NULL);
	fprintf(stderr, "MemMap and IO-Handler Hash initialized\n");
}
