/*
 *************************************************************************************************
 *
 * Emulation of the AT91 SDRAM controller (SDRAMC)
 *
 * State: nothing is implemented 
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
#include "at91_sdramc.h"
#include "sglib.h"
#include "sgstring.h"

#define SDRAMC_MR(base)		((base) + 0x00)
#define SDRAMC_TR(base)		((base) + 0x04)
#define SDRAMC_CR(base)		((base) + 0x08)
#define SDRAMC_LPR(base)	((base) + 0x10)
#define SDRAMC_IER(base)	((base) + 0x14)
#define SDRAMC_IDR(base)	((base) + 0x18)
#define SDRAMC_IMR(base)	((base) + 0x1c)
#define SDRAMC_ISR(base)	((base) + 0x20)
#define SDRAMC_MDR(base)	((base) + 0x24)

typedef struct AT91Sdramc {
	BusDevice bdev;
	uint32_t regLPR;
} AT91Sdramc;

static uint32_t
mr_read(void *clientData,uint32_t address,int rqlen)
{
        fprintf(stderr,"%s not implemented\n",__func__);
        return 0;
}

static void
mr_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
        fprintf(stderr,"%s not implemented\n",__func__);
}

static uint32_t
tr_read(void *clientData,uint32_t address,int rqlen)
{
        fprintf(stderr,"%s not implemented\n",__func__);
        return 0;
}

static void
tr_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
        fprintf(stderr,"%s not implemented\n",__func__);
}

static uint32_t
cr_read(void *clientData,uint32_t address,int rqlen)
{
        fprintf(stderr,"%s not implemented\n",__func__);
        return 0;
}

static void
cr_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
        fprintf(stderr,"%s not implemented\n",__func__);
}


static uint32_t
lpr_read(void *clientData,uint32_t address,int rqlen)
{
	AT91Sdramc *sd = clientData;
        return sd->regLPR;
}

static void
lpr_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	AT91Sdramc *sd = clientData;
	sd->regLPR = value & 0x00003F73;
}


static uint32_t
ier_read(void *clientData,uint32_t address,int rqlen)
{
        fprintf(stderr,"%s not implemented\n",__func__);
        return 0;
}

static void
ier_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
        fprintf(stderr,"%s not implemented\n",__func__);
}

static uint32_t
idr_read(void *clientData,uint32_t address,int rqlen)
{
        fprintf(stderr,"%s not implemented\n",__func__);
        return 0;
}

static void
idr_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
        fprintf(stderr,"%s not implemented\n",__func__);
}

static uint32_t
imr_read(void *clientData,uint32_t address,int rqlen)
{
        fprintf(stderr,"%s not implemented\n",__func__);
        return 0;
}

static void
imr_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
        fprintf(stderr,"%s not implemented\n",__func__);
}

static uint32_t
isr_read(void *clientData,uint32_t address,int rqlen)
{
        fprintf(stderr,"%s not implemented\n",__func__);
        return 0;
}

static void
isr_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
        fprintf(stderr,"%s not implemented\n",__func__);
}

static uint32_t
mdr_read(void *clientData,uint32_t address,int rqlen)
{
        fprintf(stderr,"%s not implemented\n",__func__);
        return 0;
}

static void
mdr_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
        fprintf(stderr,"%s not implemented\n",__func__);
}

static void
AT91Sdramc_Map(void *owner,uint32_t base,uint32_t mask,uint32_t flags)
{
        AT91Sdramc *sd = owner;
	IOH_New32(SDRAMC_MR(base),mr_read,mr_write,sd);
	IOH_New32(SDRAMC_TR(base),tr_read,tr_write,sd);
	IOH_New32(SDRAMC_CR(base),cr_read,cr_write,sd);
	IOH_New32(SDRAMC_LPR(base),lpr_read,lpr_write,sd);
	IOH_New32(SDRAMC_IER(base),ier_read,ier_write,sd);
	IOH_New32(SDRAMC_IDR(base),idr_read,idr_write,sd);
	IOH_New32(SDRAMC_IMR(base),imr_read,imr_write,sd);
	IOH_New32(SDRAMC_ISR(base),isr_read,isr_write,sd);
	IOH_New32(SDRAMC_MDR(base),mdr_read,mdr_write,sd);
}

static void
AT91Sdramc_UnMap(void *owner,uint32_t base,uint32_t mask)
{
	IOH_Delete32(SDRAMC_MR(base));
	IOH_Delete32(SDRAMC_TR(base));
	IOH_Delete32(SDRAMC_CR(base));
	IOH_Delete32(SDRAMC_LPR(base));
	IOH_Delete32(SDRAMC_IER(base));
	IOH_Delete32(SDRAMC_IDR(base));
	IOH_Delete32(SDRAMC_IMR(base));
	IOH_Delete32(SDRAMC_ISR(base));
	IOH_Delete32(SDRAMC_MDR(base));
}

BusDevice *
AT91Sdramc_New(const char *name)
{
        AT91Sdramc *sd = sg_new(AT91Sdramc);

        sd->bdev.first_mapping = NULL;
        sd->bdev.Map = AT91Sdramc_Map;
        sd->bdev.UnMap = AT91Sdramc_UnMap;
        sd->bdev.owner = sd;
        sd->bdev.hw_flags = MEM_FLAG_WRITABLE|MEM_FLAG_READABLE;
        fprintf(stderr,"AT91 SDRAM controller \"%s\" created\n",name);
        return &sd->bdev;
}

