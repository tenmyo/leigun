/*
 *************************************************************************************************
 *
 * LPC-Flash 
 *      Emulation of the internal flash of Philips LPC  series
 *
 * Status: Reading from flash is implemented 
 *	Mirror mapping, writing and state machine are missing
 *
 * Copyright 2005 Jochen Karrer. All rights reserved.
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

#include "lpcflash.h"
#include "configfile.h"
#include "cycletimer.h"
#include "bus.h"
#include "signode.h"
#include "diskimage.h"
#include "sgstring.h"

/*
 * Taken from
 * http://water.cse.unsw.edu.au/esdk/lpc2/boot-loader.html
 */
#define LPCF_BASE (0x3fff8000)
#define LPCF_LIMIT_SELECT(base)		((base) + 0x00)
#define LPCF_WRITE_OFFSET(base)		((base) + 0x04)
#define LPCF_WRITEWORD0(base)		((base) + 0x08)
#define LPCF_WRITEWORD1(base)		((base) + 0x0c)
#define LPCF_WRITEWORD2(base)		((base) + 0x10)
#define LPCF_WRITEWORD3(base)		((base) + 0x14)
#define LPCF_FLASH_CTRL(base)		((base) + 0x18)
#define	 	FLASH_CTRL_WRITE	(1)
#define	 	FLASH_CTRL_ERASE	(2)
#define	 	FLASH_CTRL_OPEN		(4)
#define	 	FLASH_CTRL_LOCK		(8)
#define	 	FLASH_CTRL_LOAD		(0x10)
#define LPCF_SECTOR_SEL(base)		((base) + 0x1C)
#define LPCF_WRITE_CNT(base)		((base) + 0x20)
#define LPCF_LOCK_STATUS(base)		((base) + 0x24)
#define LPCF_TIMING_1(base)		((base) + 0x28)
#define LPCF_TIMING_2(base)		((base) + 0x2c)

typedef struct LPCFlash {
	BusDevice bdev;
	DiskImage *disk_image;
	uint8_t *host_mem;
	int size;
	uint32_t limit_select;
	uint32_t write_offset;
	uint32_t writebuf[4];
	uint32_t flash_ctrl;
	uint32_t sector_sel;
	uint32_t write_cnt;
	uint32_t lock_status;
	uint32_t timing_1;
	uint32_t timing_2;
} LPCFlash;

static uint32_t
limit_select_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "LPCFlash register %08x not implemented\n", address);
	return 0;
}

static void
limit_select_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "LPCFlash register %08x not implemented\n", address);
	return;
}

static uint32_t
write_offset_read(void *clientData, uint32_t address, int rqlen)
{
	LPCFlash *lflash = (LPCFlash *) clientData;
	return lflash->write_offset;
}

static void
write_offset_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	LPCFlash *lflash = (LPCFlash *) clientData;
	lflash->write_offset = (value & ~511);
	return;
}

static uint32_t
writeword_read(void *clientData, uint32_t address, int rqlen)
{
	LPCFlash *lflash = (LPCFlash *) clientData;
	unsigned int index = ((address & 0x1f) - 8) >> 2;
	if (index > 3) {
		fprintf(stderr, "LPCF: Illegal index of writeword\n");
		return 0;
	}
	return lflash->writebuf[index];
}

static void
writeword_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	LPCFlash *lflash = (LPCFlash *) clientData;
	unsigned int index = ((address & 0x1f) - 8) >> 2;
	if (index > 3) {
		fprintf(stderr, "LPCF: Illegal index of writeword\n");
		return;
	}
	lflash->writebuf[index] = value;
	return;
}

static uint32_t
flash_ctrl_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "LPCFlash register %08x not implemented\n", address);
	return 0;
}

static void
flash_ctrl_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "LPCFlash register %08x not implemented\n", address);
	return;
}

static uint32_t
sector_sel_read(void *clientData, uint32_t address, int rqlen)
{
	LPCFlash *lflash = (LPCFlash *) clientData;
	return lflash->sector_sel;
}

static void
sector_sel_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	LPCFlash *lflash = (LPCFlash *) clientData;
	lflash->sector_sel = value;
	return;
}

static uint32_t
write_cnt_read(void *clientData, uint32_t address, int rqlen)
{
	LPCFlash *lflash = (LPCFlash *) clientData;
	return lflash->write_cnt;
}

static void
write_cnt_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	LPCFlash *lflash = (LPCFlash *) clientData;
	lflash->write_cnt = value;
	return;
}

static uint32_t
lock_status_read(void *clientData, uint32_t address, int rqlen)
{
	LPCFlash *lflash = (LPCFlash *) clientData;
	return lflash->lock_status;
}

static void
lock_status_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	LPCFlash *lflash = (LPCFlash *) clientData;
	lflash->lock_status = value;
	return;
}

static uint32_t
timing_1_read(void *clientData, uint32_t address, int rqlen)
{
	LPCFlash *lflash = (LPCFlash *) clientData;
	return lflash->timing_1;
}

static void
timing_1_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	LPCFlash *lflash = (LPCFlash *) clientData;
	lflash->timing_1 = value;
	return;
}

static uint32_t
timing_2_read(void *clientData, uint32_t address, int rqlen)
{
	LPCFlash *lflash = (LPCFlash *) clientData;
	return lflash->timing_2;
}

static void
timing_2_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	LPCFlash *lflash = (LPCFlash *) clientData;
	lflash->timing_1 = value;
	return;
}

static void
Flash_Map(void *module_owner, uint32_t base, uint32_t mapsize, uint32_t flags)
{
	LPCFlash *lflash = module_owner;
	uint8_t *host_mem = lflash->host_mem;
	uint32_t ctrlbase = LPCF_BASE;
	if (1) {
		flags &= MEM_FLAG_READABLE;
		Mem_MapRange(base, host_mem, lflash->size, mapsize, flags);
	} else {
		//IOH_NewRegion(base,mapsize,flash_read,flash_write,HOST_BYTEORDER,lflash);
	}
	IOH_New32(LPCF_LIMIT_SELECT(ctrlbase), limit_select_read, limit_select_write, lflash);
	IOH_New32(LPCF_WRITE_OFFSET(ctrlbase), write_offset_read, write_offset_write, lflash);
	IOH_New32(LPCF_WRITEWORD0(ctrlbase), writeword_read, writeword_write, lflash);
	IOH_New32(LPCF_WRITEWORD1(ctrlbase), writeword_read, writeword_write, lflash);
	IOH_New32(LPCF_WRITEWORD2(ctrlbase), writeword_read, writeword_write, lflash);
	IOH_New32(LPCF_WRITEWORD3(ctrlbase), writeword_read, writeword_write, lflash);
	IOH_New32(LPCF_FLASH_CTRL(ctrlbase), flash_ctrl_read, flash_ctrl_write, lflash);
	IOH_New32(LPCF_SECTOR_SEL(ctrlbase), sector_sel_read, sector_sel_write, lflash);
	IOH_New32(LPCF_WRITE_CNT(ctrlbase), write_cnt_read, write_cnt_write, lflash);
	IOH_New32(LPCF_LOCK_STATUS(ctrlbase), lock_status_read, lock_status_write, lflash);
	IOH_New32(LPCF_TIMING_1(ctrlbase), timing_1_read, timing_1_write, lflash);
	IOH_New32(LPCF_TIMING_2(ctrlbase), timing_2_read, timing_2_write, lflash);
}

static void
Flash_UnMap(void *module_owner, uint32_t base, uint32_t mapsize)
{
	Mem_UnMapRange(base, mapsize);
	IOH_DeleteRegion(base, mapsize);
}

static uint32_t
parse_memsize(char *str)
{
	uint32_t size;
	char c;
	if (sscanf(str, "%d", &size) != 1) {
		return 0;
	}
	if (sscanf(str, "%d%c", &size, &c) == 1) {
		return size;
	}
	switch (tolower((unsigned char)c)) {
	    case 'm':
		    return size * 1024 * 1024;
	    case 'k':
		    return size * 1024;
	}
	return 0;
}

/*
 * -----------------------------------------------------------------
 * Create new lpc flash with diskimage
 * -----------------------------------------------------------------
 */
BusDevice *
LPCFlash_New(const char *flash_name)
{
	LPCFlash *lflash = sg_new(LPCFlash);
	char *imagedir;
	char *sizestr;
	uint32_t flash_size;
	imagedir = Config_ReadVar("global", "imagedir");
	sizestr = Config_ReadVar(flash_name, "size");
	if (sizestr) {
		flash_size = parse_memsize(sizestr);
		if (flash_size == 0) {
			fprintf(stderr, "LPC flash bank \"%s\" not present\n", flash_name);
			return NULL;
		}
	} else {
		fprintf(stderr, "size for LPC-flash \"%s\" not configured\n", flash_name);
		exit(1);
	}
	lflash->size = flash_size;
	if (imagedir) {
		char *mapfile = alloca(strlen(imagedir) + strlen(flash_name) + 20);
		sprintf(mapfile, "%s/%s.img", imagedir, flash_name);
		lflash->disk_image = DiskImage_Open(mapfile, lflash->size, DI_RDWR | DI_CREAT_FF);
		if (!lflash->disk_image) {
			fprintf(stderr, "Open disk image failed\n");
			exit(42);
		}
		lflash->host_mem = DiskImage_Mmap(lflash->disk_image);
	} else {
		lflash->host_mem = sg_calloc(lflash->size);
		memset(lflash->host_mem, 0xff, lflash->size);
	}
	lflash->bdev.first_mapping = NULL;
	lflash->bdev.Map = Flash_Map;
	lflash->bdev.UnMap = Flash_UnMap;
	lflash->bdev.owner = lflash;
	lflash->bdev.hw_flags = MEM_FLAG_READABLE;
	fprintf(stderr, "Created LPC Flash with size %d bytes\n", flash_size);
	return &lflash->bdev;
}
