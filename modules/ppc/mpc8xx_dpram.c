/*
 *************************************************************************************************
 *
 * MPC8xx Dualport RAM emulation 
 *
 * (C) 2008 Jochen Karrer 
 *   Author: Jochen Karrer
 *
 * Copyright 2006 Jochen Karrer. All rights reserved.
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
#include <sgstring.h>

#include <bus.h>
#include <configfile.h>
#include "mpc8xx_dpram.h"
#include <assert.h>

#define DPRAM_MAGIC (0x73829345)

typedef struct DPRamTrace {
	void *clientData;
	DPRamWriteProc *proc;
	uint32_t addr;
	struct DPRamTrace *next;
} DPRamTrace;

typedef struct DPRam {
	BusDevice bdev;
	uint32_t magic;
	uint8_t *host_mem;
	uint32_t size;
	uint32_t immr_offset;
	unsigned int hashsize;
	DPRamTrace **traceHash;
} DPRam;

static uint32_t
DPRam_read(void *clientData, uint32_t mem_addr, int rqlen)
{
	uint32_t value;
	DPRam *dpram = (DPRam *) clientData;
	void *mem = &dpram->host_mem[mem_addr & (dpram->size - 1)];
	switch (rqlen) {
	    case 4:
		    value = be32_to_host(*(uint32_t *) mem);
		    break;
	    case 2:
		    value = be16_to_host(*(uint16_t *) mem);
		    break;
	    case 1:
	    default:
		    value = *(uint8_t *) mem;
		    break;
	}
	return value;
}

static void
DPRam_write(void *clientData, uint32_t value, uint32_t mem_addr, int rqlen)
{
	DPRam *dpram = clientData;
	DPRamTrace *trace;
	uint32_t addr = mem_addr & (dpram->size - 1);
	uint32_t hash_index = mem_addr % dpram->hashsize;
	void *mem;
	trace = dpram->traceHash[hash_index];
	while (trace) {
		if (trace->addr == addr) {
			trace->proc(trace->clientData, value, addr, rqlen);
			return;
		}
		trace = trace->next;
	}
	mem = &dpram->host_mem[addr];
	switch (rqlen) {
	    case 4:
		    *(uint32_t *) mem = host32_to_be(value);
		    break;
	    case 2:
		    *(uint16_t *) mem = host16_to_be(value);
		    break;
	    case 1:
		    *(uint8_t *) mem = value;
		    break;
	}
}

void
DPRam_Trace(BusDevice * dev, uint32_t addr, int len, DPRamWriteProc * proc, void *cd)
{
	DPRamTrace *trace;
	uint32_t hash_index;
	DPRam *dpram = (DPRam *) dev->owner;

	assert(dpram->magic = DPRAM_MAGIC);
	hash_index = addr % dpram->hashsize;
	trace = sg_calloc(sizeof(DPRamTrace));
	trace->proc = proc;
	trace->clientData = cd;
	trace->next = dpram->traceHash[hash_index];
	dpram->traceHash[hash_index] = trace;
}

static void
DPRam_Map(void *module_owner, uint32_t base, uint32_t mapsize, uint32_t flags)
{
	DPRam *dpram = module_owner;
	assert(dpram->magic = DPRAM_MAGIC);
	fprintf(stderr, "Mapping DPRAM ************* to 0x%08x, size 0x%04x\n",
		base + dpram->immr_offset, dpram->size);
	IOH_NewRegion(base + dpram->immr_offset, dpram->size, DPRam_read, DPRam_write,
		      HOST_BYTEORDER, dpram);
}

static void
DPRam_UnMap(void *module_owner, uint32_t base, uint32_t mapsize)
{
	DPRam *dpram = module_owner;
	assert(dpram->magic = DPRAM_MAGIC);
	fprintf(stderr, "Unmapping DPRAM *************\n");
	exit(1);
	IOH_DeleteRegion(base + dpram->immr_offset, mapsize);
}

uint32_t
MPC_DPRamRead(BusDevice * bdev, uint32_t addr, int size)
{
	DPRam *dpram = (DPRam *) bdev->owner;
	int i;
	uint32_t value = 0;
	uint8_t *m = dpram->host_mem;
	assert(dpram->magic = DPRAM_MAGIC);
	for (i = 0; i < 4; i++) {
		if ((addr + i) < dpram->size) {
			value = (value << 8) | m[addr + i];
		}

	}
	return value;
}

void
MPC_DPRamWrite(BusDevice * bdev, uint32_t value, uint32_t addr, int size)
{
	DPRam *dpram = (DPRam *) bdev->owner;
	uint8_t *m = dpram->host_mem;
	int i;
	for (i = size - 1; i >= 0; i--) {
		m[addr + i] = value;
		value >>= 8;
	}
}

/*
 * -------------------------------------------------------
 * Constructor for MPC8xx Dualport RAM
 * -------------------------------------------------------
 */
BusDevice *
MPC8xx_DPRamNew(char *dpram_name, uint32_t size, uint32_t immr_offset)
{
	DPRam *dpram;
	dpram = sg_calloc(sizeof(DPRam));
	dpram->host_mem = sg_calloc(size);
	memset(dpram->host_mem, 0xff, size);
	dpram->hashsize = 67;
	dpram->size = size;
	dpram->immr_offset = immr_offset;
	dpram->traceHash = sg_calloc(sizeof(DPRamTrace *) * dpram->hashsize);
	dpram->bdev.first_mapping = NULL;
	dpram->bdev.Map = DPRam_Map;
	dpram->bdev.UnMap = DPRam_UnMap;
	dpram->bdev.owner = dpram;
	dpram->bdev.hw_flags = MEM_FLAG_WRITABLE | MEM_FLAG_READABLE;
	fprintf(stderr, "DPRAM bank \"%s\" with  size %ukB\n", dpram_name, size / 1024);
	return &dpram->bdev;
}
