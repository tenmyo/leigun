/*
 ************************************************************************************************
 * M32C-Flash
 *      Emulation of the internal flash of Renesas M32C  series
 *
 * Status: untested 
 *
 * Copyright 2009/2010 Jochen Karrer. All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without modification, are
 * permitted provided that the following conditions are met:
 * 
 *   1. Redistributions of source code must retain the above copyright notice, this list of
 *       conditions and the following disclaimer.
 * 
 *    2. Redistributions in binary form must reproduce the above copyright notice, this list
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
 ************************************************************************************************
 */

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <ctype.h>

#include "flash_m32c.h"
#include "configfile.h"
#include "cycletimer.h"
#include "bus.h"
#include "signode.h"
#include "diskimage.h"
#include "sgstring.h"
#include "cpu_m32c.h"

#define FMR0	(0x57)
#define		FMR00_RDY	(1 << 0)
#define		FMR01_CPUREW	(1 << 1)
#define		FMR02_LOCK	(1 << 2)
#define		FMR03_FMSTP	(1 << 3)
#define		FMR05_USERROM	(1 << 5)
#define		FMR06_PGMSTAT	(1 << 6)
#define		FMR07_ERASESTAT	(1 << 7)
#define FMR1	(0x55)

#define CMD_NONE 		(0)
#define CMD_READ_ARRAY 		(1)
#define CMD_READ_STATUS 	(2)
#define CMD_CLEAR_STATUS	(3)
#define CMD_PROGRAM		(4)
#define CMD_BLOCK_ERASE		(5)
#define CMD_ERASE_ALL_UNLOCKED	(6)
#define CMD_LOCK_BIT_PGM	(7)
#define CMD_READ_LOCK_BIT_STAT  (8)

#define MAP_READ_ARRAY  (0)
#define MAP_IO          (1)

typedef struct FlashSector {
	uint32_t ofs;
	uint32_t size;
} FlashSector;

/**
 **********************************************************************
 * Sector size table needed for Erasing the flash.
 **********************************************************************
 */
FlashSector const m32c87_sectors[] = {
	{0x000000, 0x10000},
	{0x010000, 0x10000},
	{0x020000, 0x10000},
	{0x030000, 0x10000},
	{0x040000, 0x10000},
	{0x050000, 0x10000},
	{0x060000, 0x10000},
	{0x070000, 0x10000},
	{0x080000, 0x10000},
	{0x090000, 0x10000},
	{0x0A0000, 0x10000},
	{0x0B0000, 0x10000},
	{0x0C0000, 0x10000},
	{0x0D0000, 0x10000},
	{0x0E0000, 0x10000},
	{0x0F0000, 0x8000},
	{0x0F8000, 0x2000},
	{0x0FA000, 0x2000},
	{0x0FC000, 0x2000},
	{0x0FE000, 0x1000},
	{0x0FF000, 0x1000}
};

typedef struct M32CFlash {
	uint8_t fmr0;
	uint8_t fmr1;
	const FlashSector *sector_info;
	int sector_count;

	/* State space from two variables */
	int cycle;
	int cmd;
	int mode;		/* Read array or command mode */
	uint32_t write_addr;

	CycleCounter_t busy_until;
	BusDevice bdev;
	DiskImage *disk_image;
	uint8_t *host_mem;
	uint32_t size;
	SigNode *sigNMI;
} M32CFlash;

static inline void
make_busy(M32CFlash * mflash, uint32_t useconds)
{
	mflash->busy_until = CycleCounter_Get() + MicrosecondsToCycles(useconds);
}

/**
 *************************************************************
 * Linear search for a sector. Silly and slow
 *************************************************************
 */
static const FlashSector *
FindSector(M32CFlash * mflash, uint32_t ofs)
{
	int i;
	FlashSector const *sect;
	for (i = 0; i < mflash->sector_count; i++) {
		sect = &mflash->sector_info[i];
		if ((ofs >= sect->ofs) && (ofs < (sect->ofs + sect->size))) {
			return sect;
		}
	}
	return NULL;
}

static void
switch_to_readarray(M32CFlash * mflash)
{
	if (mflash->mode != MAP_READ_ARRAY) {
		mflash->mode = MAP_READ_ARRAY;
		Mem_AreaUpdateMappings(&mflash->bdev);
	}
}

static void
switch_to_iomode(M32CFlash * mflash)
{
	if (mflash->mode != MAP_IO) {
		mflash->mode = MAP_IO;
		Mem_AreaUpdateMappings(&mflash->bdev);
	}
}

static void
flash_write_first(M32CFlash * mflash, uint32_t value, uint32_t mem_addr, int rqlen)
{
	switch (value & 0xff) {
	    case 0xff:
		    switch_to_readarray(mflash);
		    break;

	    case 0x70:
		    /* Read status register */
		    switch_to_iomode(mflash);
		    mflash->cmd = CMD_READ_STATUS;
		    mflash->cycle++;
		    break;

	    case 0x50:
		    /* Clear status register  do immediately */
		    break;

	    case 0x40:
		    /* program  */
		    //fprintf(stderr,"PGM cmd\n");
		    mflash->write_addr = mem_addr;
		    switch_to_iomode(mflash);	// ??????
		    mflash->cmd = CMD_PROGRAM;
		    mflash->cycle++;
		    break;

	    case 0x20:
		    /* block erase */
		    switch_to_iomode(mflash);	// ??????
		    mflash->cmd = CMD_BLOCK_ERASE;
		    mflash->cycle++;
		    break;

	    case 0xa7:
		    /* erase all unlocked */
		    switch_to_iomode(mflash);	// ??????
		    mflash->cmd = CMD_ERASE_ALL_UNLOCKED;
		    mflash->cycle++;
		    break;

	    case 0x77:
		    /* lock bit program */
		    switch_to_iomode(mflash);	// ??????
		    mflash->cmd = CMD_LOCK_BIT_PGM;
		    mflash->cycle++;
		    break;

	    case 0x71:
		    /* read lock bit status */
		    switch_to_iomode(mflash);	// ??????
		    mflash->cmd = CMD_READ_LOCK_BIT_STAT;
		    mflash->cycle++;
		    break;
	    default:
		    fprintf(stderr, "M32CFlash: Unknown command 0x%04x at PC %06x\n", value,
			    M32C_REG_PC);
		    break;
	}
}

/**
 * erase one sector of flash
 */
static void
cmd_block_erase(M32CFlash * mflash, uint32_t mem_addr)
{
	uint32_t ofs = mem_addr & (mflash->size - 1);
	const FlashSector *fs = FindSector(mflash, ofs);
	uint32_t busy_us;
	if (!fs) {
		fprintf(stderr, "Bug, Flash sector at addr %08x not found\n", mem_addr);
		return;
	}
	if (SigNode_Val(mflash->sigNMI) == SIG_LOW) {
		fprintf(stderr, "Flash erase ignored due to NMI\n");
		return;
	}
	memset(mflash->host_mem + fs->ofs, 0xff, fs->size);
	switch (fs->size) {
	    case 0x10000:
		    busy_us = 800000;
		    break;
	    case 0x8000:
		    busy_us = 500000;
		    break;
	    case 0x2000:
	    case 0x1000:
		    busy_us = 300000;
		    break;
	    default:
		    fprintf(stderr, "Unexpected sector size of %u bytes\n", fs->size);
		    busy_us = 300000;
		    exit(1);
	}
	if ((lrand48() & 0xff) < 4) {
		busy_us = 4000000;
		//fprintf(stderr,"Maximum flash erase time\n");
	}
	make_busy(mflash, busy_us);
	//fprintf(stderr,"M32C_Flash: Erased sector at %08x ts %lld\n",mem_addr,CycleCounter_Get());
}

static void
cmd_program(M32CFlash * mflash, uint32_t mem_addr, uint32_t value)
{
	static int max_pgm = 0;
	uint32_t ofs = mem_addr & (mflash->size - 1) & ~1;
	if (SigNode_Val(mflash->sigNMI) == SIG_LOW) {
		fprintf(stderr, "Flash write ignored due to NMI\n");
	} else {
		mflash->host_mem[ofs + 0] &= (value & 0xff);
		mflash->host_mem[ofs + 1] &= ((value >> 8) & 0xff);
		max_pgm = (max_pgm + 1) % 32;
		if (max_pgm == 0) {
			make_busy(mflash, 300);
		} else {
			make_busy(mflash, 25);
		}
	}
}

static void
flash_write_second(M32CFlash * mflash, uint32_t value, uint32_t mem_addr, int rqlen)
{
	switch (mflash->cmd) {
	    case CMD_READ_STATUS:
		    fprintf(stderr, "M32CFlash command %02x not implemented\n", mflash->cmd);
		    break;

	    case CMD_PROGRAM:
		    if (mflash->write_addr != mem_addr) {
			    fprintf(stderr,
				    "Changed write address in CMD_PROGRAM from %08x to %08x\n",
				    mflash->write_addr, mem_addr);
		    } else {
			    cmd_program(mflash, mem_addr, value);
		    }
		    break;

	    case CMD_BLOCK_ERASE:
		    fprintf(stderr, "Erase %08x\n", mem_addr);
		    cmd_block_erase(mflash, mem_addr);
		    break;
	    case CMD_ERASE_ALL_UNLOCKED:
		    fprintf(stderr, "M32CFlash command %02x not implemented\n", mflash->cmd);
		    break;
	    case CMD_LOCK_BIT_PGM:
		    fprintf(stderr, "M32CFlash command %02x not implemented\n", mflash->cmd);
		    break;
	    case CMD_READ_LOCK_BIT_STAT:
		    fprintf(stderr, "M32CFlash command %02x not implemented\n", mflash->cmd);
		    break;
	    default:
		    fprintf(stderr, "M32CFlash illegal command state %02x\n", mflash->cmd);
		    break;
	}
}

static void
flash_write(void *clientData, uint32_t value, uint32_t mem_addr, int rqlen)
{
	M32CFlash *mflash = (M32CFlash *) clientData;
	if (CycleCounter_Get() < mflash->busy_until) {
		return;
	}
	if (!(mflash->fmr0 & FMR01_CPUREW)) {
		fprintf(stderr, "Not in CPU rewrite mode at 0x%06x, FB %06x\n",
			M32C_REG_PC, M32C_REG_FB);
		return;
	}
	//fprintf(stderr,"Flash write %04x to %08x\n",value,mem_addr);
	switch (mflash->cycle) {
	    case 1:
		    flash_write_first(mflash, value, mem_addr, rqlen);
		    break;
	    case 2:
		    flash_write_second(mflash, value, mem_addr, rqlen);
		    mflash->cycle = 1;
		    break;
	    default:
		    fprintf(stderr, "M32C flash emulator bug: cycle %d\n", mflash->cycle);
	}
}

/*
 * -----------------------------------------------------------------------------
 * In MAP_IO mode this flash_read is used, else direct access to mmaped file
 * -----------------------------------------------------------------------------
 */
static uint32_t
flash_read(void *clientData, uint32_t mem_addr, int rqlen)
{
	fprintf(stderr, "M32CFlash: IO-mode %08x read not implemented\n", mem_addr);
	return 0;
}

static void
Flash_Map(void *module_owner, uint32_t base, uint32_t mapsize, uint32_t flags)
{
	M32CFlash *mflash = module_owner;
	uint8_t *host_mem = mflash->host_mem;
	if (mflash->mode == MAP_READ_ARRAY) {
		flags &= ~MEM_FLAG_WRITABLE;
		Mem_MapRange(base, host_mem, mflash->size, mapsize, flags);
		//fprintf(stderr,"Mapped %p size %d, mapsize %d\n",host_mem,mflash->size,mapsize); 
	}
	IOH_NewRegion(base, mapsize, flash_read, flash_write, HOST_BYTEORDER, mflash);
}

static void
Flash_UnMap(void *module_owner, uint32_t base, uint32_t mapsize)
{
	Mem_UnMapRange(base, mapsize);
	IOH_DeleteRegion(base, mapsize);
}

static uint32_t
fmr0_read(void *clientData, uint32_t address, int rqlen)
{
	M32CFlash *mflash = (M32CFlash *) clientData;
	uint32_t value = mflash->fmr0;
	if (CycleCounter_Get() < mflash->busy_until) {
		value &= ~FMR00_RDY;
	} else {
		value |= FMR00_RDY;
	}
	return value;
}

static void
fmr0_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	M32CFlash *mflash = (M32CFlash *) clientData;
	mflash->fmr0 = value & 0x3E;
	return;
}

static uint32_t
fmr1_read(void *clientData, uint32_t address, int rqlen)
{
	M32CFlash *mflash = (M32CFlash *) clientData;
	return mflash->fmr1;
}

static void
fmr1_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	M32CFlash *mflash = (M32CFlash *) clientData;
	mflash->fmr1 = value & 0xb2;
	return;
}

/*
 * ---------------------------------------------------------
 * The SFR part of the registers for flash is always at the 
 * same fixed location and will never be remapped
 * ---------------------------------------------------------
 */
static void
Flash_SFRMap(M32CFlash * mflash)
{
	IOH_New8(FMR0, fmr0_read, fmr0_write, mflash);
	IOH_New8(FMR1, fmr1_read, fmr1_write, mflash);

}

/*
 * Should be moved to some common place
 */
static uint32_t
parse_memsize(char *str)
{
	uint32_t size;
	unsigned char c;
	if (sscanf(str, "%d", &size) != 1) {
		return 0;
	}
	if (sscanf(str, "%d%c", &size, &c) == 1) {
		return size;
	}
	switch (tolower(c)) {
	    case 'm':
		    return size * 1024 * 1024;
	    case 'k':
		    return size * 1024;
	}
	return 0;
}

BusDevice *
M32CFlash_New(const char *flash_name)
{
	M32CFlash *mflash = sg_new(M32CFlash);
	char *imagedir;
	char *sizestr;
	imagedir = Config_ReadVar("global", "imagedir");
	sizestr = Config_ReadVar(flash_name, "size");
	if (sizestr) {
		mflash->size = parse_memsize(sizestr);
		if (mflash->size == 0) {
			fprintf(stderr, "M32C Flash \"%s\" has zero size\n", flash_name);
			return NULL;
		}
	} else {
		mflash->size = 1024 * 1024;
	}
	if (imagedir) {
		char *mapfile = alloca(strlen(imagedir) + strlen(flash_name) + 20);
		sprintf(mapfile, "%s/%s.img", imagedir, flash_name);
		mflash->disk_image = DiskImage_Open(mapfile, mflash->size, DI_RDWR | DI_CREAT_FF);
		if (!mflash->disk_image) {
			fprintf(stderr, "Open disk image failed\n");
			exit(42);
		}
		mflash->host_mem = DiskImage_Mmap(mflash->disk_image);
		//fprintf(stderr,"Mapped to %08x\n",mflash->host_mem);
	} else {
		mflash->host_mem = sg_calloc(mflash->size);
		memset(mflash->host_mem, 0xff, mflash->size);
	}
	mflash->sector_info = m32c87_sectors;
	mflash->sector_count = array_size(m32c87_sectors);
	mflash->cycle = 1;
	mflash->mode = MAP_READ_ARRAY;
	mflash->cmd = CMD_NONE;

	mflash->bdev.first_mapping = NULL;
	mflash->bdev.Map = Flash_Map;
	mflash->bdev.UnMap = Flash_UnMap;
	mflash->bdev.owner = mflash;
	mflash->bdev.hw_flags = MEM_FLAG_READABLE;
	mflash->sigNMI = SigNode_New("%s.nmi", flash_name);
	if (!mflash->sigNMI) {
		fprintf(stderr, "Can not create NMI line for flash\n");
		exit(1);
	}
	fprintf(stderr, "Created M32C Flash with size %d bytes\n", mflash->size);
	Flash_SFRMap(mflash);	/* ???? */
	return &mflash->bdev;
}
