/*
 *******************************************************************************
 * ROM 
 *
 * Copyright 2010 Jochen Karrer. All rights reserved.
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
#include "rom.h"
#include "sgstring.h"
#include "diskimage.h"

typedef struct Rom {
	DiskImage *diskimage;
	BusDevice bdev;
	uint8_t *host_mem;
	uint32_t  size;
	uint32_t flags;
} Rom;

static void
Rom_Map(void *module_owner,uint32_t base,uint32_t mapsize,uint32_t flags) {
	Rom *rom = module_owner;
	flags &= MEM_FLAG_READABLE;
	Mem_MapRange(base,rom->host_mem,rom->size,mapsize,flags);
}

static void
Rom_UnMap(void *module_owner,uint32_t base,uint32_t mapsize) {
	Mem_UnMapRange(base,mapsize); 
}

static uint32_t
parse_memsize (char *str)
{
        uint32_t size;
        char c;
        if(sscanf(str,"%d",&size)!=1) {
                return 0;
        }
        if(sscanf(str,"%d%c",&size,&c)==1) {
                return size;
        }
        switch(tolower((unsigned char)c)) {
                case 'm':
                        return size*1024*1024;
                case 'k':
                        return size*1024;
        }
        return 0;
}

/*
 *****************************************************************
 * \fn BusDevice * Rom_New(char *rom_name); 
 * Create a new instance of ROM.
 *****************************************************************
 */
BusDevice *
Rom_New(char *rom_name) {
	char *sizestr,*imagedir;
	uint32_t size=0;
	Rom *rom;
	sizestr = Config_ReadVar(rom_name,"size");
	imagedir = Config_ReadVar("global","imagedir");
	if(sizestr) {
		size=parse_memsize(sizestr);
		if(size == 0) {
			fprintf(stderr,"ROM bank \"%s\" not present\n",rom_name);
			return NULL;
		}
	} else {
		fprintf(stderr,"ROM bank \"%s\" not present\n",rom_name);
		return NULL;
	}
	rom = sg_new(Rom);
	rom->size = size;
	if(imagedir) {
                char *mapfile = alloca(strlen(imagedir) + strlen(rom_name) + 20);
                sprintf(mapfile,"%s/%s.img",imagedir,rom_name);
                rom->diskimage = DiskImage_Open(mapfile,rom->size,DI_RDWR | DI_CREAT_FF);
                if(!rom->diskimage) {
                        fprintf(stderr,"Open disk image failed\n");
                        exit(42);
                }
                rom->host_mem = DiskImage_Mmap(rom->diskimage);
        } else {
                rom->host_mem = sg_calloc(rom->size);
                memset(rom->host_mem,0xff,rom->size);
        }
	rom->bdev.first_mapping = NULL;
	rom->bdev.Map = Rom_Map;
	rom->bdev.UnMap = Rom_UnMap;
	rom->bdev.owner = rom;
	rom->bdev.hw_flags=MEM_FLAG_WRITABLE|MEM_FLAG_READABLE;
	fprintf(stderr,"ROM bank \"%s\" with  size %ukB\n",rom_name,size/1024);
	return &rom->bdev;
}
