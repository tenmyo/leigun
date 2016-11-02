/*
 **************************************************************************
 * NetX XPEC emulation 
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

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>

#include "bus.h"
#include "sgstring.h"

typedef struct XPec {
	BusDevice bdev;
	uint32_t size;
	uint8_t *host_mem;
} XPec;

static void
XPec_Map(void *module_owner,uint32_t base,uint32_t mapsize,uint32_t flags) 
{
	XPec *xpec = module_owner;
	flags &= MEM_FLAG_READABLE|MEM_FLAG_WRITABLE;
	Mem_MapRange(base,xpec->host_mem,xpec->size,mapsize,flags);
}

static void
XPec_UnMap(void *module_owner,uint32_t base,uint32_t mapsize) {
	Mem_UnMapRange(base,mapsize); 
}

/*
 **************************************
 * XPEC New
 **************************************
 */
BusDevice *
XPec_New(const char *name) {
	XPec *xpec;
	xpec = sg_new(XPec);
	xpec->size = 0x4000;
	xpec->host_mem=sg_calloc(xpec->size);
	xpec->bdev.first_mapping=NULL;
	xpec->bdev.Map=XPec_Map;
	xpec->bdev.UnMap=XPec_UnMap;
	xpec->bdev.owner=xpec;
	xpec->bdev.hw_flags=MEM_FLAG_WRITABLE|MEM_FLAG_READABLE;
	fprintf(stderr,"XPEC \"%s\" created\n",name);
	return &xpec->bdev;
}
