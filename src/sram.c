/*
 **************************************************************************************************
 * SRAM Emulation
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
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>

#include "bus.h"
#include "configfile.h"
#include "sgstring.h"

typedef struct SRam {
	BusDevice bdev;
	uint8_t *host_mem;
	uint32_t size;
	uint32_t flags;
} SRam;

static void
SRam_Map(void *module_owner, uint32_t base, uint32_t mapsize, uint32_t flags)
{
	SRam *sram = module_owner;
	flags &= MEM_FLAG_READABLE | MEM_FLAG_WRITABLE;
	Mem_MapRange(base, sram->host_mem, sram->size, mapsize, flags);
}

static void
SRam_UnMap(void *module_owner, uint32_t base, uint32_t mapsize)
{
	Mem_UnMapRange(base, mapsize);
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
 * --------------------
 * DRAM New
 * --------------------
 */
BusDevice *
SRam_New(char *sram_name)
{
	char *sizestr;
	uint32_t size = 0;
	SRam *sram;
	sizestr = Config_ReadVar(sram_name, "size");
	if (sizestr) {
		size = parse_memsize(sizestr);
		if (size == 0) {
			fprintf(stderr, "SRAM bank \"%s\" not present\n", sram_name);
			return NULL;
		}
	} else {
		fprintf(stderr, "SRAM bank \"%s\" not present\n", sram_name);
		return NULL;
	}
	sram = sg_new(SRam);
	sram->host_mem = sg_calloc(size);
	memset(sram->host_mem, 0xff, size);
	sram->size = size;
	sram->bdev.first_mapping = NULL;
	sram->bdev.Map = SRam_Map;
	sram->bdev.UnMap = SRam_UnMap;
	sram->bdev.owner = sram;
	sram->bdev.hw_flags = MEM_FLAG_WRITABLE | MEM_FLAG_READABLE;
	fprintf(stderr, "SRAM bank \"%s\" with  size %.1fkB\n", sram_name, size / 1024.);
	return &sram->bdev;
}
