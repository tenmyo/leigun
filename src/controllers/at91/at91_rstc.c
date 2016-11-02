/*
 *************************************************************************************************
 *
 * Emulation of the AT91 Reset controller
 *
 * State: minimalistic implementation 
 *
 * Copyright 2012 Jochen Karrer. All rights reserved.
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
#include "bus.h"
#include "at91_rstc.h"
#include "sglib.h"
#include "sgstring.h"

#define RSTC_CR(base) ((base) + 0x00)
#define		CR_PROCRST	(1 << 0)
#define		CR_PERRST	(1 << 1)
#define		CR_EXTRST	(1 << 3)
#define RSTC_SR(base) ((base) + 0x04)
#define		SR_URSTS	(1 << 0)
#define		SR_RSTYP_MSK	(0x7 << 8)
#define		RSTYP_GENERAL	(0)
#define		RSTYP_WAKEUP	(1 << 8)
#define		RSTYP_WATCHDOG	(2 << 8)
#define		RSTYP_SWRESET	(3 << 8)
#define		RSTYP_USER	(4 << 8)
#define 	SR_NRSTL	(1 << 16)
#define 	SR_SRCMP	(1 << 17)
#define RSTC_MR(base) ((base) + 0x08)

typedef struct AT91Rstc {
	BusDevice bdev;
	uint32_t regCR;
	uint32_t regSR;
	uint32_t regMR;
} AT91Rstc;

static uint32_t
cr_read(void *clientData,uint32_t address,int rqlen)
{
	fprintf(stderr,"Reset controller Control register not implemented\n");
	return 0;
}

static void
cr_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	if(((value >> 24) & 0xFF) != 0xA5) {
		fprintf(stderr,"Reset controller: Wrong Key 0x%x\n",value);
		return;
	}
	if(value & CR_PROCRST) {
		fprintf(stderr,"Reset Controller: Processor reset: 0x%x\n",value);
		exit(0);
	}
	fprintf(stderr,"Reset Controller CR: 0x%08x not implemented\n",value);
}

static uint32_t
sr_read(void *clientData,uint32_t address,int rqlen)
{
	AT91Rstc *rs = clientData;
	uint32_t value;
	value = rs->regSR | SR_NRSTL;
	rs->regSR  &= ~SR_URSTS;
	return value;
}

static void
sr_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	fprintf(stderr,"Reset controller status register is writeonly\n");
}

static uint32_t
mr_read(void *clientData,uint32_t address,int rqlen)
{
	AT91Rstc *rs = clientData;
	return rs->regMR;
}

static void
mr_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	AT91Rstc *rs = clientData;
	if((value >> 24) != 0xa5) {
		fprintf(stderr,"Wrong password for Reset controller mode register: 0x%08x\n",value);
		return;
	}
	rs->regMR = value & 0xF11;
}


static void
AT91Rstc_Map(void *owner,uint32_t base,uint32_t mask,uint32_t flags)
{
	AT91Rstc *rs = owner;
	IOH_New32(RSTC_CR(base),cr_read,cr_write,rs);
	IOH_New32(RSTC_SR(base),sr_read,sr_write,rs);
	IOH_New32(RSTC_MR(base),mr_read,mr_write,rs);
}

static void
AT91Rstc_UnMap(void *owner,uint32_t base,uint32_t mask)
{
	IOH_Delete32(RSTC_CR(base));
	IOH_Delete32(RSTC_SR(base));
	IOH_Delete32(RSTC_MR(base));
}

BusDevice *
AT91Rstc_New(const char *name)
{
        AT91Rstc *rs = sg_new(AT91Rstc);

        rs->bdev.first_mapping = NULL;
        rs->bdev.Map = AT91Rstc_Map;
        rs->bdev.UnMap = AT91Rstc_UnMap;
        rs->bdev.owner = rs;
        rs->bdev.hw_flags = MEM_FLAG_WRITABLE|MEM_FLAG_READABLE;
	rs->regSR = SR_URSTS;
        fprintf(stderr,"AT91 Reset controller \"%s\" created\n",name);
        return &rs->bdev;
}

