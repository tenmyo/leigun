/*
 *************************************************************************************************
 * Emulation of Freescale Multi layer AHB Crossbar Switch (MAX) 
 *
 * state: simply stores the register values. Has no effect 
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

#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/fcntl.h>
#include "bus.h"
#include "imx21_max.h"
#include "sgstring.h"

#define MAX_MPR(base,n)		((base)+(n)*0x100+0x000)
#define MAX_AMPR(base,n)	((base)+(n)*0x100+0x004)
#define MAX_SGPCR(base,n)	((base)+(n)*0x100+0x010)
#define		SGPCR_RO	(1<<31)
#define		SGPCR_HLP	(1<<30)
#define MAX_ASGPCR(base,n)	((base)+(n)*0x100+0x014)
#define MAX_MGPCR(base,n)	((base)+(n)*0x100+0x800)

typedef struct IMX21Max {
	uint32_t mpr[4];
	uint32_t ampr[4];
	uint32_t sgpcr[4];
	uint32_t asgpcr[4];
	uint32_t mgpcr[6];
	BusDevice bdev;
} IMX21Max;

static uint32_t
mpr_read(void *clientData, uint32_t address, int rqlen)
{
	IMX21Max *max = (IMX21Max *) clientData;
	int index = ((address) >> 8) & 3;
	return max->mpr[index];
}

static void
mpr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	IMX21Max *max = (IMX21Max *) clientData;
	int index = ((address) >> 8) & 3;
	/* Check if it is in readonly mode first */
	if (max->sgpcr[index] & SGPCR_RO) {
		return;
	}
	max->mpr[index] = value & 0x00777777;
	return;
}

static uint32_t
ampr_read(void *clientData, uint32_t address, int rqlen)
{
	IMX21Max *max = (IMX21Max *) clientData;
	int index = ((address) >> 8) & 3;
	return max->ampr[index];
}

static void
ampr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	IMX21Max *max = (IMX21Max *) clientData;
	int index = ((address) >> 8) & 3;
	/* Check if it is in readonly mode first */
	if (max->sgpcr[index] & SGPCR_RO) {
		return;
	}
	max->ampr[index] = value & 0x00777777;
	return;
}

static uint32_t
sgpcr_read(void *clientData, uint32_t address, int rqlen)
{
	IMX21Max *max = (IMX21Max *) clientData;
	int index = ((address) >> 8) & 3;
	return max->sgpcr[index];
}

static void
sgpcr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	IMX21Max *max = (IMX21Max *) clientData;
	int index = ((address) >> 8) & 3;
	/* Check if it is in readonly mode first */
	if (max->sgpcr[index] & SGPCR_RO) {
		return;
	}
	max->sgpcr[index] = value & 0xc0000337;
	return;
}

static uint32_t
asgpcr_read(void *clientData, uint32_t address, int rqlen)
{
	IMX21Max *max = (IMX21Max *) clientData;
	int index = ((address) >> 8) & 3;
	return max->asgpcr[index];
}

static void
asgpcr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	IMX21Max *max = (IMX21Max *) clientData;
	int index = ((address) >> 8) & 3;
	/* Check if it is in readonly mode first */
	if (max->sgpcr[index] & SGPCR_RO) {
		return;
	}
	max->asgpcr[index] = value & 0x40000337;
	return;
}

static uint32_t
mgpcr_read(void *clientData, uint32_t address, int rqlen)
{
	IMX21Max *max = (IMX21Max *) clientData;
	int index = ((address) >> 8) & 7;
	return max->mgpcr[index];
}

static void
mgpcr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	IMX21Max *max = (IMX21Max *) clientData;
	int index = ((address) >> 8) & 7;
	max->mgpcr[index] = value;
	return;
}

static void
IMXMax_Unmap(void *owner, uint32_t base, uint32_t mask)
{
	int i;
	for (i = 0; i < 4; i++) {
		IOH_Delete32(MAX_MPR(base, i));
		IOH_Delete32(MAX_AMPR(base, i));
		IOH_Delete32(MAX_SGPCR(base, i));
		IOH_Delete32(MAX_ASGPCR(base, i));
	}
	for (i = 0; i < 6; i++) {
		IOH_Delete32(MAX_MGPCR(base, i));
	}
}

static void
IMXMax_Map(void *owner, uint32_t base, uint32_t mask, uint32_t mapflags)
{
	int i;
	IMX21Max *max = (IMX21Max *) owner;
	for (i = 0; i < 4; i++) {
		IOH_New32(MAX_MPR(base, i), mpr_read, mpr_write, max);
		IOH_New32(MAX_AMPR(base, i), ampr_read, ampr_write, max);
		IOH_New32(MAX_SGPCR(base, i), sgpcr_read, sgpcr_write, max);
		IOH_New32(MAX_ASGPCR(base, i), asgpcr_read, asgpcr_write, max);
	}
	for (i = 0; i < 6; i++) {
		IOH_New32(MAX_MGPCR(base, i), mgpcr_read, mgpcr_write, max);
	}
}

BusDevice *
IMX21_MaxNew(const char *name)
{
	IMX21Max *max = sg_new(IMX21Max);
	int i;
	max->bdev.first_mapping = NULL;
	max->bdev.Map = IMXMax_Map;
	max->bdev.UnMap = IMXMax_Unmap;
	max->bdev.owner = max;
	max->bdev.hw_flags = MEM_FLAG_WRITABLE | MEM_FLAG_READABLE;
	for (i = 0; i < 4; i++) {
		max->mpr[i] = 0x76543210;
		max->ampr[i] = 0x76543210;
		max->sgpcr[i] = 0;
		max->asgpcr[i] = 0;
	}
	for (i = 0; i < 6; i++) {
		max->mgpcr[i] = 0;
	}
	return &max->bdev;
}
