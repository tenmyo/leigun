/*
 *************************************************************************************************
 * M16C-Flash
 *      Emulation of the internal flash of Renesas M16C  series
 *
 * Status: nothing is implemented 
 *
 * Copyright 2005 2011 Jochen Karrer. All rights reserved.
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

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <ctype.h>

#include "m16c_flash.h"
#include "configfile.h"
#include "cycletimer.h"
#include "bus.h"
#include "signode.h"
#include "diskimage.h"
#include "sgstring.h"

#define FMR0	(0x220)
#define		FMR0_FMSTP	(1<<3)
#define FMR1	(0x221)
#define FMR2	(0x222)
#define FMR6	(0x230)

#define CMD_NONE 		(0)
#define CMD_READ_ARRAY 		(0xff)
#define CMD_READ_STATUS 	(0x70)
#define CMD_CLEAR_STATUS	(0x50)
#define CMD_PROGRAM		(0x41)
#define CMD_BLOCK_ERASE		(0x20)
//#define CMD_ERASE_ALL_UNLOCKED        (0x77)
#define CMD_LOCK_BIT_PGM	(0x77)
#define CMD_READ_LOCK_BIT_STAT  (0x71)
#define CMD_BLOCK_BLANK_CHK  	(0x25)

#define MAP_READ_ARRAY  (0)
#define MAP_IO          (1)

typedef struct FlashSector {
	uint32_t sect_addr;
	uint32_t sect_size;
} FlashSector;

typedef struct M16CFlash {
	uint8_t regFmr0;
	uint8_t regFmr1;
	uint8_t regFmr2;
	uint8_t regFmr6;

	/* State space from two variables */
	int cycle;
	int cmd;
	int mode;		/* Read array or command mode */
	uint32_t write_addr;
	/*
	 * timing parameters 
	 */
	uint32_t pgm_word_us;
	uint32_t erase_sector_us;
	CycleCounter_t busy_until;

	BusDevice bdev;
	DiskImage *disk_image;
	uint8_t *host_mem;
	uint32_t map_addr;
	int size;
} M16CFlash;

static void
switch_to_readarray(M16CFlash * mflash)
{
	if (mflash->mode != MAP_READ_ARRAY) {
		mflash->mode = MAP_READ_ARRAY;
		Mem_AreaUpdateMappings(&mflash->bdev);
	}
}

static void
switch_to_iomode(M16CFlash * mflash)
{
	if (mflash->mode != MAP_IO) {
		mflash->mode = MAP_IO;
		Mem_AreaUpdateMappings(&mflash->bdev);
	}
}

static inline void
make_busy(M16CFlash * mflash, uint32_t useconds)
{
	mflash->busy_until = CycleCounter_Get() + MicrosecondsToCycles(useconds);
}

static void
cmd_block_erase(M16CFlash * mflash, uint32_t mem_addr)
{
	uint32_t idx = mem_addr - mflash->map_addr;
	FlashSector *fs = alloca(sizeof(FlashSector));
	if ((idx + 1) >= mflash->size) {
		return;
	}
	fs->sect_size = 65536;
	fs->sect_addr = idx & ~(fs->sect_size - 1);
	memset(mflash->host_mem + fs->sect_addr, 0xff, fs->sect_size);
	make_busy(mflash, 250000);
	fprintf(stderr, "M16C_Flash: Erased sector at %08x\n", mem_addr);
}

static void
cmd_program(M16CFlash * mflash, uint32_t mem_addr, uint32_t value)
{
	uint32_t idx = mem_addr - mflash->map_addr;
	if ((idx + 1) >= mflash->size) {
		return;
	}
	mflash->host_mem[idx + 0] &= (value & 0xff);
	mflash->host_mem[idx + 1] &= (value >> 8) & 0xff;
	make_busy(mflash, mflash->pgm_word_us);
}

static void
flash_write_first(M16CFlash * mflash, uint32_t value, uint32_t mem_addr, int rqlen)
{
	mflash->cmd = value & 0xff;
	switch (mflash->cmd) {

	    case CMD_READ_ARRAY:
		    switch_to_readarray(mflash);
		    break;

	    case CMD_READ_STATUS:
		    /* Read status register */
		    switch_to_iomode(mflash);
		    mflash->cycle++;
		    break;

	    case CMD_CLEAR_STATUS:
		    /* Clear status register  do immediately */
		    break;

	    case CMD_PROGRAM:
		    /* program  */
		    mflash->write_addr = mem_addr;
		    switch_to_iomode(mflash);
		    mflash->cycle++;
		    break;

	    case CMD_BLOCK_ERASE:
		    /* block erase */
		    switch_to_iomode(mflash);
		    mflash->cycle++;
		    break;

	    case CMD_LOCK_BIT_PGM:
		    /* lock bit program */
		    switch_to_iomode(mflash);
		    mflash->cycle++;
		    break;

	    case CMD_READ_LOCK_BIT_STAT:
		    /* read lock bit status */
		    switch_to_iomode(mflash);
		    mflash->cycle++;
		    break;

	    case CMD_BLOCK_BLANK_CHK:
		    fprintf(stderr, "M16CFlash: blank check not implemented\n");
		    //switch_to_iomode(mflash);
		    //mflash->cycle++;
		    break;

	    default:
		    fprintf(stderr, "M16CFlash: Unknown command 0x%04x\n", value);
		    break;
	}
}

static void
flash_write_second(M16CFlash * mflash, uint32_t value, uint32_t mem_addr, int rqlen)
{
	switch (mflash->cmd) {
	    case CMD_READ_STATUS:
		    fprintf(stderr, "M16CFlash command read status not implemented\n");
		    break;
	    case CMD_PROGRAM:
		    if (mflash->write_addr != mem_addr) {
			    fprintf(stderr, "Changed write addr in Programm cycle\n");
		    } else {
			    cmd_program(mflash, mem_addr, value);
		    }
		    break;
	    case CMD_BLOCK_ERASE:
		    cmd_block_erase(mflash, mem_addr);
		    break;
		    //case CMD_ERASE_ALL_UNLOCKED:
	    case CMD_LOCK_BIT_PGM:
	    case CMD_READ_LOCK_BIT_STAT:
		    fprintf(stderr, "M16CFlash command %02x not implemented\n", mflash->cmd);
		    break;
	    default:
		    fprintf(stderr, "M16CFlash illegal command %02x\n", mflash->cmd);
		    break;
	}
}

static void
flash_write(void *clientData, uint32_t value, uint32_t mem_addr, int rqlen)
{
	M16CFlash *mflash = (M16CFlash *) clientData;
	if (CycleCounter_Get() < mflash->busy_until) {
		return;
	}
	switch (mflash->cycle) {
	    case 1:
		    flash_write_first(mflash, value, mem_addr, rqlen);
		    break;
	    case 2:
		    flash_write_second(mflash, value, mem_addr, rqlen);
		    break;
	    default:
		    fprintf(stderr, "M16C flash emulator bug: cycle %d\n", mflash->cycle);
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
	fprintf(stderr, "M16CFlash: IO-mode read not implemented\n");
	return 0;
}

static void
Flash_Map(void *module_owner, uint32_t base, uint32_t mapsize, uint32_t flags)
{
	M16CFlash *mflash = module_owner;
	uint8_t *host_mem = mflash->host_mem;
	if (mflash->mode == MAP_READ_ARRAY) {
		flags &= MEM_FLAG_READABLE;
		Mem_MapRange(base, host_mem, mflash->size, mapsize, flags);
	}
	IOH_NewRegion(base, mapsize, flash_read, flash_write, HOST_BYTEORDER, mflash);
	mflash->map_addr = base;
}

static void
Flash_UnMap(void *module_owner, uint32_t base, uint32_t mapsize)
{
	Mem_UnMapRange(base, mapsize);
	IOH_DeleteRegion(base, mapsize);
}

/**
 ************************************************************************
 * FMR0 Flash Memory Control Register 0 
 * Bit 0: FMR00 RY/#BY status flag 0 == Busy 1 == Ready
 * Bit 1: FMR01 CPU rewrite mode select bit
 * Bit 2: FMR02 Lock bit disable select bit 
 * Bit 3: FMSTP Flash memory stop bit 0 = ena 1 = stop
 * Bit 4 - 5: reserved
 * Bit 6: Program status flag, 1 = Completed with error
 * Bit 7: Erase status flag, 1 = Completed with error
 ************************************************************************
 */
static uint32_t
fmr0_read(void *clientData, uint32_t address, int rqlen)
{
	M16CFlash *mflash = (M16CFlash *) clientData;
	return mflash->regFmr0;
}

static void
fmr0_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	M16CFlash *mflash = (M16CFlash *) clientData;
#if 0
	if (value & FMR0_CPU_REWRITE) {
		switch_to_iomode(mflash);	// ?????
	}
#endif
	mflash->regFmr0 = value;
	return;
}

/**
 ********************************************************************************
 *
 * FMR1 Flash memory Control 1
 * Bit 1: FMR11  1 == Enable write to FMR6
 * Bit 6: FMR16  Lock bit status flag 0 = Lock
 * Bit 7: FMR17  Data flash wait bit 0 = 1 wait, 1: See PM17 in PM1 reg.
 * 
 ********************************************************************************
 */
static uint32_t
fmr1_read(void *clientData, uint32_t address, int rqlen)
{
	M16CFlash *mflash = (M16CFlash *) clientData;
	return mflash->regFmr1;
}

static void
fmr1_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	return;
}

/**
 *******************************************************************
 * FMR2 Flash Memory Control Register 2
 * FMR22 Slow read mode enable	1 == enabled
 * FMR23 Low current consumption read mode enable 1 == enabled
 *******************************************************************
 */
static uint32_t
fmr2_read(void *clientData, uint32_t address, int rqlen)
{
	M16CFlash *mflash = (M16CFlash *) clientData;
	return mflash->regFmr2;
}

static void
fmr2_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	return;
}

/**
 **************************************************************************************
 * FMR6 Flash Memory Control Register 6
 * Bit0: FMR60 EW1 mode select bit 0: EW0 1: EW1
 * Bit 1: FMR61 Set to 1
 * Bit 5: Set to 0
 **************************************************************************************
 */
static uint32_t
fmr6_read(void *clientData, uint32_t address, int rqlen)
{
	M16CFlash *mflash = (M16CFlash *) clientData;
	return mflash->regFmr6;
}

static void
fmr6_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	return;
}

/*
 * ---------------------------------------------------------
 * The SFR part of the registers for flash is always at the 
 * same fixed location and will never be remapped
 * ---------------------------------------------------------
 */
static void
Flash_SFRMap(M16CFlash * mflash)
{
	IOH_New8(FMR0, fmr0_read, fmr0_write, mflash);
	IOH_New8(FMR1, fmr1_read, fmr1_write, mflash);
	IOH_New8(FMR2, fmr2_read, fmr2_write, mflash);
	IOH_New8(FMR6, fmr6_read, fmr6_write, mflash);
}

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
M16CFlash_New(const char *flash_name)
{

	M16CFlash *mflash = sg_new(M16CFlash);
	char *imagedir;
	char *sizestr;
	imagedir = Config_ReadVar("global", "imagedir");
	sizestr = Config_ReadVar(flash_name, "size");
	if (sizestr) {
		mflash->size = parse_memsize(sizestr);
		if (mflash->size == 0) {
			fprintf(stderr, "M16C Flash \"%s\" has zero size\n", flash_name);
			return NULL;
		}
	} else {
		mflash->size = 192 * 1024;
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
	mflash->cycle = 1;
	mflash->mode = MAP_READ_ARRAY;
	mflash->cmd = CMD_NONE;
	/* Timing setup */
	mflash->erase_sector_us = 250000;
	mflash->pgm_word_us = 50;

	mflash->bdev.first_mapping = NULL;
	mflash->bdev.Map = Flash_Map;
	mflash->bdev.UnMap = Flash_UnMap;
	mflash->bdev.owner = mflash;
	mflash->bdev.hw_flags = MEM_FLAG_READABLE;
	fprintf(stderr, "Created M16C Flash with size %d bytes\n", mflash->size);
	Flash_SFRMap(mflash);	/* ???? */
	return &mflash->bdev;
}
