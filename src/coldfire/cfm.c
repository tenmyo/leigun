/*
 *************************************************************************************************
 *
 * Emulation of the Coldfire Flash Module (CFM)
 *
 * State:
 *	Not implemented
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

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "bus.h"
#include "configfile.h"
#include "sgstring.h"
#include "diskimage.h"
#include "cfm.h"

typedef struct CFFlash {
	BusDevice bdev;
	uint32_t size;
	uint8_t *host_mem;
	DiskImage *disk_image;
} CFFlash;

static void
cfm_write(void *clientData,uint32_t value,uint32_t addr,int rqlen)
{
	fprintf(stderr,"Write to Flash addr %08x value %08x len %d\n",addr,value,rqlen);
}


static void
Flash_UnMap(void *module_owner,uint32_t base,uint32_t mapsize)
{
        Mem_UnMapRange(base,mapsize);
        IOH_DeleteRegion(base,mapsize);
}

static void
Flash_Map(void *module_owner,uint32_t base,uint32_t mapsize,uint32_t flags)
{
	CFFlash *cfm = (CFFlash *) module_owner;
	uint32_t readflags = flags & MEM_FLAG_READABLE;
	uint32_t writeflags = flags & MEM_FLAG_WRITABLE;
        Mem_MapRange(base,cfm->host_mem,cfm->size,mapsize,readflags);
	IOH_NewRegion(base,cfm->size,NULL,cfm_write,writeflags,cfm);
}

BusDevice *
CFM_New(const char *flash_name) 
{
	char *imagedir;
	CFFlash *cfm = sg_calloc(sizeof(*cfm)); 
	imagedir = Config_ReadVar("global","imagedir");
	cfm->size = 512*1024;
	if(imagedir) {
                char *mapfile = alloca(strlen(imagedir) + strlen(flash_name)+20);
                sprintf(mapfile,"%s/%s.img",imagedir,flash_name);
                cfm->disk_image = DiskImage_Open(mapfile,cfm->size,DI_RDWR | DI_CREAT_FF);
                if(!cfm->disk_image) {
                        fprintf(stderr,"Open disk image failed\n");
                        exit(1);
                }
                cfm->host_mem=DiskImage_Mmap(cfm->disk_image);
        } else {
                cfm->host_mem=sg_calloc(cfm->size);
                memset(cfm->host_mem,0xff,cfm->size);
        }
	cfm->bdev.first_mapping=NULL;
        cfm->bdev.Map=Flash_Map;
        cfm->bdev.UnMap=Flash_UnMap;
        cfm->bdev.owner=cfm;
        cfm->bdev.hw_flags=MEM_FLAG_READABLE;
        fprintf(stderr,"Created Coldfire Flash Module with size %d\n",cfm->size);
	return &cfm->bdev;
}
