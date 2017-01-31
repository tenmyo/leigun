/*
 **********************************************************************************
 * Memory and IO-Memory Access for 32 Bit CPUs
 * (C) 2004  Lightmaze Solutions AG
 *   Author: Jochen Karrer
 **********************************************************************************
 */
#ifndef BUS_H
#define BUS_H
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <compiler_extensions.h>
#include "core/byteorder.h"

/*
 * ----------------------------------------------------
 * We can use maximum of two bits in hva for flags
 * because a 32 Bit alignment is guaranteed
 * ----------------------------------------------------
 */
#define PG_TRACED (1)
#define PG_FLAG_MASK (1)

/* The one level map */
extern uint8_t **mem_map_read, **mem_map_write;

#define IOH_FLG_BIG_ENDIAN	(1)
#define IOH_FLG_LITTLE_ENDIAN	(2)
#define IOH_FLG_HOST_ENDIAN	(4)
/*
 * -----------------------------------------------------------
 * Specify behaviour for partial or overized accesses to
 * IO handlers
 * -----------------------------------------------------------
 */
#define IOH_FLG_PA_CBSE	        (0x200)	/* Partial accesss: Call base proc      */
#define IOH_FLG_PWR_RMW		(8)	/* Partial write: Read Modify write     */
#define IOH_FLG_PRD_RARP	(0x10)	/* Partial read: Read All Return Partial */
#define IOH_FLG_OSZR_IGN	(0)	/* Oversized read: ignore */
#define IOH_FLG_OSZW_IGN	(0)	/* Oversized write: ignore */
#define IOH_FLG_OSZR_NEXT	(0x20)	/* Oversized read: next */
#define IOH_FLG_OSZW_NEXT	(0x40)	/* Oversized write: next */
#define IOH_FLG_OSZR_WRAP	(0x80)	/* Oversized read: wrap */
#define IOH_FLG_OSZW_WRAP	(0x100)	/* Oversized write: wrap */

#define IOH_HASH_SIZE	(16384)
#define IOH_HASH_MASK	(IOH_HASH_SIZE-1)

/*
 * -------------------------------
 * One level 1MB IO-Regions
 * -------------------------------
 */
#define IOH_MAP_ENTRIES	(4096)
#define IOH_MAP_SHIFT	(20)
#define IOH_MAP_BLOCKSIZE (1<<20)
#define IOH_MAP_BLOCKMASK ((IOH_MAP_BLOCKSIZE)-1)

/*
 * ----------------------------------
 * Two level 512 Byte IO-Regions
 * ----------------------------------
 */
#define IOH_FLVL_SZ 		(4096)
#define IOH_FLVL_MASK    	(0xfff00000)
#define IOH_FLVL_SHIFT	 	(20)
#define IOH_FLVL_BLOCKMASK 	((1<<20)-1)
#define IOH_SLVL_SZ		(2048)
#define IOH_SLVL_MASK		(0x000ffe00)
#define IOH_SLVL_SHIFT	 	(9)
#define IOH_SLVL_BLOCKMASK	(0x1ff)
#define IOH_SLVL_BLOCKSIZE      (0x200)

/*
 * ---------------------------
 * One level 32kB blocks 
 * ---------------------------
 */
#define MEM_MAP_SHIFT (15)
#define MEM_MAP_BLOCKSIZE (1 << MEM_MAP_SHIFT)
#define MEM_MAP_ENTRIES (1 << (32 - MEM_MAP_SHIFT))
#define MEM_MAP_BLOCKMASK ((MEM_MAP_BLOCKSIZE)-1)

/*
 * ---------------------------------------------
 * Two level PA to HVA translation can have
 * a minimum of 1kB blocks because this is
 * assumed in MMU implementations with HVA cache
 * ---------------------------------------------
 */

typedef struct TwoLevelMMap {
	unsigned int frst_lvl_sz;
	unsigned long frst_lvl_mask;
	unsigned int frst_lvl_shift;
	unsigned int scnd_lvl_sz;
	unsigned long scnd_lvl_mask;
	unsigned int scnd_lvl_shift;
	unsigned int scnd_lvl_blockmask;
	unsigned int scnd_lvl_blocksize;

	uint8_t ***flvlmap_read;
	uint32_t *flvl_map_read_use_count;
	uint8_t ***flvlmap_write;
	uint32_t *flvl_map_write_use_count;
} TwoLevelMMap;

extern TwoLevelMMap twoLevelMMap;

typedef uint32_t IOReadProc(void *clientData, uint32_t address, int rqlen);
typedef void IOWriteProc(void *clientData, uint32_t value, uint32_t address, int rqlen);
typedef struct IOHandler {
	struct IOHandler *next;
	uint32_t cpu_addr;
	IOReadProc *readproc;
	IOWriteProc *writeproc;
	void *clientData;
	uint8_t swap_endian;
	uint8_t len;
	uint32_t flags;
} IOHandler;
extern IOHandler **iohandlerHash;

void IOH_New8f(uint32_t cpu_addr, IOReadProc * readproc, IOWriteProc * writeproc, void *clientData,
	       uint32_t flags);
void IOH_New32f(uint32_t address, IOReadProc * readproc, IOWriteProc * writeproc, void *clientData,
		uint32_t flags);
void IOH_New16f(uint32_t address, IOReadProc * readproc, IOWriteProc * writeproc, void *clientData,
		uint32_t flags);
void IOH_Delete32(uint32_t address);
void IOH_Delete16(uint32_t address);
void IOH_Delete8(uint32_t address);
void IOH_NewRegion(uint32_t address, uint32_t length, IOReadProc * readproc,
		   IOWriteProc * writeproc, int byteorder, void *clientData);
void IOH_DeleteRegion(uint32_t address, uint32_t length);

static inline void
IOH_New8(uint32_t cpu_addr, IOReadProc * readproc, IOWriteProc * writeproc, void *clientData)
{
	uint32_t flags = IOH_FLG_HOST_ENDIAN | IOH_FLG_PRD_RARP;
	IOH_New8f(cpu_addr, readproc, writeproc, clientData, flags);
}

static inline void
IOH_New16(uint32_t cpu_addr, IOReadProc * readproc, IOWriteProc * writeproc, void *clientData)
{
	uint32_t flags = IOH_FLG_HOST_ENDIAN | IOH_FLG_PRD_RARP;
	IOH_New16f(cpu_addr, readproc, writeproc, clientData, flags);
}

static inline void
IOH_New32(uint32_t cpu_addr, IOReadProc * readproc, IOWriteProc * writeproc, void *clientData)
{
	uint32_t flags = IOH_FLG_HOST_ENDIAN;
	IOH_New32f(cpu_addr, readproc, writeproc, clientData, flags);
}

void IO_Write32(uint32_t value, uint32_t addr);
void IO_Write16(uint16_t value, uint32_t addr);
void IO_Write8(uint8_t value, uint32_t addr);
uint64_t IO_Read64(uint32_t addr);
uint32_t IO_Read32(uint32_t addr);
uint16_t IO_Read16(uint32_t addr);
uint8_t IO_Read8(uint32_t addr);

/*
 * ----------------------------------------------
 * MemMapping 
 *	Represents a single mapping of a chip
 *	One Chip can have multiple Mappings
 * ----------------------------------------------
 */
#define MEM_FLAG_READABLE (1<<0)
#define MEM_FLAG_WRITABLE (1<<1)

typedef struct MemMapping {
	uint32_t base;
	uint32_t mapsize;	/* not the size of the chip if it does not decode all address lines ! */
	uint32_t flags;
	struct MemMapping *next;
} MemMapping;

#define BSCMAGIC_DRAM_CMD	(4711)

typedef struct BusSpecialCycle {
	int magic;
	uint8_t data[0];	// variable sized
} BusSpecialCycle_t;

typedef struct BusDevice {
	/* public */
	void *owner;
	uint32_t magic;
	void (*Map) (void *owner, uint32_t base, uint32_t mask, uint32_t flags);
	void (*UnMap) (void *owner, uint32_t base, uint32_t mask);
	int (*specialCycle) (struct BusDevice * bdev, BusSpecialCycle_t * cycle);

	/* private */
	MemMapping *first_mapping;
	uint32_t hw_flags;

	/* Service provider functions */
	void (*postIRQ) (struct BusDevice *, int irq);
	void (*unPostIRQ) (struct BusDevice *, int irq);
	 uint32_t(*read32) (uint32_t addr);
	 uint16_t(*read16) (uint32_t addr);
	 uint8_t(*read8) (uint32_t addr);
	void (*readblock) (uint8_t * buf, uint32_t addr, uint32_t count);
	void (*write32) (uint32_t value, uint32_t addr);
	void (*write16) (uint16_t value, uint32_t addr);
	void (*write8) (uint8_t value, uint32_t addr);
	void (*writeblock) (uint32_t addr, uint8_t * buf, uint32_t count);

} BusDevice;

typedef struct Bus {
	uint32_t(*read32) (uint32_t addr);
	uint16_t(*read16) (uint32_t addr);
	uint8_t(*read8) (uint32_t addr);
	void (*readblock) (uint8_t * buf, uint32_t addr, uint32_t count);
	void (*write32) (uint32_t value, uint32_t addr);
	void (*write16) (uint16_t value, uint32_t addr);
	void (*write8) (uint8_t value, uint32_t addr);
	void (*writeblock) (uint32_t addr, uint8_t * buf, uint32_t count);
} Bus;

extern Bus *MainBus;
/*
 * -------------------------------------------------------------------------------
 * Called by the System Emulator to emulate its Memory Mapping through Chipselect 
 * lines
 * -------------------------------------------------------------------------------
 */
void Mem_AreaAddMapping(BusDevice * dev, uint32_t base, uint32_t mask, uint32_t flags);
void Mem_AreaDeleteMappings(BusDevice * dev);

/*
 * -------------------------------------------------------------------------------
 * Called by the Chip Emulator whenever it updates its mapping by itself 
 * -------------------------------------------------------------------------------
 */
void Mem_AreaUpdateMappings(BusDevice * dev);

/*
 * ---------------------------------------------
 * Configuration and creation of memory areas
 * ---------------------------------------------
 */

void Mem_MapRange(uint32_t base, uint8_t * start_mem, uint32_t devsize, uint32_t mapsize,
		  uint32_t flags);
void Mem_UnMapRange(uint32_t base, uint32_t mapsize);

/*
 * ----------------------------------------------------------------
 * Special bus cycles (For example SDRAM configuration cycles)
 * ----------------------------------------------------------------
 */
static inline int
Bus_DoSpecialCycle(BusDevice * bdev, BusSpecialCycle_t * cycle)
{
	if (!bdev->specialCycle) {
		return -1;
	}
	return bdev->specialCycle(bdev, cycle);
}

/*
 * -------------------------------------------------------------
 * Get Host Virtual Addresses for read and Write access
 * -------------------------------------------------------------
 */

static inline unsigned char *
Bus_GetHVAWrite(uint32_t addr)
{
	uint32_t index = (addr >> MEM_MAP_SHIFT);
	uint8_t *base = mem_map_write[index];
	if (likely(base)) {
		return base + (addr & (MEM_MAP_BLOCKMASK));
	} else {
		uint8_t **slvl_map;
		index = addr >> twoLevelMMap.frst_lvl_shift;
		if (!(slvl_map = twoLevelMMap.flvlmap_write[index])) {
			return NULL;
		}
		index = (addr & twoLevelMMap.scnd_lvl_mask) >> twoLevelMMap.scnd_lvl_shift;
		base = slvl_map[index];
		if (likely(base)) {
			if (unlikely(((long)base) & PG_TRACED)) {
				base -= PG_TRACED;
				slvl_map[index] = base;
				IO_Write8(0, addr);
			}
			return base + (addr & (twoLevelMMap.scnd_lvl_blockmask));
		} else {
			return NULL;
		}
	}
}

static inline unsigned char *
Bus_GetHVARead(uint32_t addr)
{
	uint32_t index = (addr >> MEM_MAP_SHIFT);
	uint8_t *base = mem_map_read[index];
	if (likely(base)) {
		return base + (addr & (MEM_MAP_BLOCKMASK));
	} else {
		uint8_t **slvl_map;
		index = addr >> twoLevelMMap.frst_lvl_shift;
		if (!(slvl_map = twoLevelMMap.flvlmap_read[index])) {
			return NULL;
		}
		index = (addr & twoLevelMMap.scnd_lvl_mask) >> twoLevelMMap.scnd_lvl_shift;
		base = slvl_map[index];
		if (likely(base)) {
			return base + (addr & twoLevelMMap.scnd_lvl_blockmask);
		} else {
			return NULL;
		}
	}
}

/* Host Memory access functions */
static inline uint64_t
HMemRead64(uint8_t * host_addr)
{
	return (*((uint64_t *) host_addr));
}

static inline uint32_t
HMemRead32(uint8_t * host_addr)
{
	return (*((uint32_t *) host_addr));
}

static inline uint16_t
HMemRead16(uint8_t * host_addr)
{
	return (*((uint16_t *) host_addr));
}

static inline uint8_t
HMemRead8(uint8_t * host_addr)
{
	return *((uint8_t *) host_addr);
}

static inline void
HMemWrite64(uint64_t value, uint8_t * addr)
{
	*((uint64_t *) addr) = (value);
}

static inline void
HMemWrite32(uint32_t value, uint8_t * addr)
{
	*((uint32_t *) addr) = (value);
}

static inline void
HMemWrite16(uint16_t value, uint8_t * addr)
{
	*((uint16_t *) addr) = (value);
}

static inline void
HMemWrite8(uint8_t value, uint8_t * addr)
{
	*((uint8_t *) addr) = value;
}

static inline uint32_t
Bus_FastRead32(uint32_t addr)
{
	uint8_t *base = mem_map_read[addr >> MEM_MAP_SHIFT];
	if (unlikely(!base)) {
		uint8_t **slvl_map;
		int index;
		index = addr >> twoLevelMMap.frst_lvl_shift;
		if (!(slvl_map = twoLevelMMap.flvlmap_read[index])) {
			fprintf(stdout, "1) No such memory: %08x\n", addr);
			exit(163);
		}
		index = (addr & twoLevelMMap.scnd_lvl_mask) >> twoLevelMMap.scnd_lvl_shift;
		base = slvl_map[index];
		if (base) {
			return HMemRead32(base + (addr & twoLevelMMap.scnd_lvl_blockmask));
		} else {
			/* Should better trigger External Abort */
			fprintf(stdout, "2) No such memory: %08x\n", addr);
			exit(7163);
		}

	}
	return HMemRead32(base + (addr & MEM_MAP_BLOCKMASK));
}

uint64_t Bus_Read64(uint32_t addr);
uint32_t Bus_Read32(uint32_t addr);
uint16_t Bus_Read16(uint32_t addr);
uint8_t Bus_Read8(uint32_t addr);
void Bus_Write64(uint64_t value, uint32_t addr);
void Bus_Write32(uint32_t value, uint32_t addr);
void Bus_Write16(uint16_t value, uint32_t addr);
void Bus_Write8(uint8_t value, uint32_t addr);
void Bus_Write(uint32_t addr, uint8_t * buf, uint32_t count);
void Bus_Read(uint8_t * buf, uint32_t addr, uint32_t count);
void Bus_WriteSwap32(uint32_t addr, uint8_t * buf, int count);
void Bus_ReadSwap32(uint8_t * buf, uint32_t addr, int count);

typedef void InvalidateCallback(void);
void Bus_Init(InvalidateCallback *, uint32_t min_blocksize);
uint32_t Bus_GetMinBlockSize(void);
int Mem_Load(char *filename, uint32_t addr);
void Mem_TracePage(uint32_t pgaddr);
void Mem_TraceRegion(uint32_t start, uint32_t length);
void Mem_UntracePage(uint32_t pgaddr);
void Mem_UntraceRegion(uint32_t start, uint32_t length);

static inline int
Mem_SmallPageSize()
{
	return twoLevelMMap.scnd_lvl_blocksize;
}
#endif				// BUS_H
