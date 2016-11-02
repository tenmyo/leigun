/*
 *****************************************************************************************************
 *
 * Emulation of AT91SAM7A3 Embedded flash controller and Flash chip 
 *
 * state: not implemented 
 *
 * Copyright 2009 Jochen Karrer. All rights reserved.
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
#include <diskimage.h>
#include <configfile.h>
#include <ctype.h>
#include <at91sam_efc.h>

typedef struct Efc {
	BusDevice efcdev;
	BusDevice flashdev;
	DiskImage *disk_image;
	uint32_t size;
	void *host_mem;
} Efc;

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

static void
AT91Efc_Map(void *owner, uint32_t base, uint32_t mapsize, uint32_t flags)
{
//        Efc *efc = (Efc *) owner;
//        IOH_New32(PMC_SCER(base),scer_read,scer_write,pmc);
}

static void
AT91Efc_UnMap(void *owner, uint32_t base, uint32_t mapsize)
{
}

static void
AT91Flash_Map(void *owner, uint32_t base, uint32_t mapsize, uint32_t flags)
{
	Efc *efc = (Efc *) owner;
	Mem_MapRange(base, efc->host_mem, efc->size, mapsize, flags);
}

static void
AT91Flash_UnMap(void *owner, uint32_t base, uint32_t mapsize)
{
	Mem_UnMapRange(base, mapsize);
}

void
AT91SAM7_EfcNew(BusDevice ** flash, BusDevice ** efcdev, const char *efcname, const char *flashname)
{
	Efc *efc = sg_calloc(sizeof(Efc));
	char *directory;
	char *mapfile = NULL;
	char *sizestr;
	uint32_t flash_size;
	directory = Config_ReadVar("global", "imagedir");
	if (directory) {
		mapfile = alloca(strlen(directory) + strlen(flashname) + 20);
		sprintf(mapfile, "%s/%s.img", directory, flashname);
	}
	sizestr = Config_ReadVar(flashname, "size");
	if (sizestr) {
		flash_size = parse_memsize(sizestr);
		if (flash_size == 0) {
			fprintf(stderr, "AT91 flash bank \"%s\" has zero size\n", flashname);
			return;
		}
	} else {
		fprintf(stderr, "size for flash size \"%s\" not configured\n", flashname);
		exit(1);
	}
	efc->size = flash_size;
	if (mapfile) {
		efc->disk_image = DiskImage_Open(mapfile, efc->size, DI_RDWR | DI_CREAT_FF);
		if (!efc->disk_image) {
			fprintf(stderr, "Open disk image failed\n");
			exit(42);
		}
		efc->host_mem = DiskImage_Mmap(efc->disk_image);
	}
	efc->efcdev.first_mapping = NULL;
	efc->efcdev.Map = AT91Efc_Map;
	efc->efcdev.UnMap = AT91Efc_UnMap;
	efc->efcdev.owner = efc;
	efc->efcdev.hw_flags = MEM_FLAG_WRITABLE | MEM_FLAG_READABLE;

	efc->flashdev.first_mapping = NULL;
	efc->flashdev.Map = AT91Flash_Map;
	efc->flashdev.UnMap = AT91Flash_UnMap;
	efc->flashdev.owner = efc;
	efc->flashdev.hw_flags = MEM_FLAG_WRITABLE | MEM_FLAG_READABLE;
	*flash = &efc->flashdev;
	*efcdev = &efc->efcdev;

}
