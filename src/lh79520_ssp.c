/*
 * --------------------------------------------------------------------------------
 * Emulation of Sharp LH79520 Synchronous Serial Port (SSP)
 *
 * state: Not implemented 
 *
 * Copyright 2007 Jochen Karrer. All rights reserved.
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

#define SSPCR0(base) 	((base)+0x000)
#define	SSPCR0_SCR_MASK		(0xffffff << 8)
#define	SSPCR0_SCR_SHIFT	(8)
#define SSPCR_SPH		(1<<7)
#define SSPCR_SPO		(1<<6)
#define SSPCR_FRF_MASK		(3<<4)
#define	SSPCR_FRF_SHIFT		(4)
#define	SSPCR_DSS_MASK		(0xf)
#define	SSPCR_DSS_SHIFT		(0)
#define SSPCR1(base)	((base)+0x004)
#define SSPCR1_SSE	(1<<4)
#define	SSPCR1_LBM	(1<<3)
#define SSPCR1_RORIE	(1<<2)
#define SSPCR1_TIE	(1<<1)
#define	SSPCR1_RIE	(1<<0)
#define	SSPDR(base)	((base)+0x008)
#define SSPSR(base)	((base)+0x00c)
#define	SSPSR_BSY	(1<<4)
#define	SSPSR_RFF	(1<<3)
#define	SSPSR_RNE	(1<<2)
#define	SSPSR_TNF	(1<<1)
#define SSPSR_TFE	(1<<0)
#define SSPCPSR(base)	((base)+0x010)
#define	SSPCPSR_CPSDVSR_MASK	(0xff)
#define	SSPCPSR_CPSDVSR_SHIFT	(0)
#define SSPIIR(base)	((base)+0x014)
#define SSPICR(base)	((base)+0x014)
#define SSPRXTO(base)	((base)+0x018)

#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/fcntl.h>
#include "bus.h"
#include "signode.h"
#include "cycletimer.h"
#include "clock.h"
#include "lh79520_ssp.h"
#include "sgstring.h"

typedef struct LH_SSP {
	BusDevice bdev;
	Clock_t *clk_in; 
	Clock_t *clk_out;
	SigNode *sspclk;
	SigNode *sspfrm;
	SigNode *ssptx;
	SigNode *ssprx;
	SigNode *sspen;
} LH_SSP; 

static uint32_t
sspcr0_read(void *clientData,uint32_t address,int rqlen)
{
        return 0; 
}

static void
sspcr0_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{

}

static uint32_t
sspcr1_read(void *clientData,uint32_t address,int rqlen)
{
        return 0;
}

static void
sspcr1_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{

}
static uint32_t
sspdr_read(void *clientData,uint32_t address,int rqlen)
{
        return 0;
}

static void
sspdr_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{

}

static uint32_t
sspsr_read(void *clientData,uint32_t address,int rqlen)
{
	return 0;
}

static void
sspsr_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{

}

static uint32_t
sspcpsr_read(void *clientData,uint32_t address,int rqlen)
{
        return 0;
}

static void
sspcpsr_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{

}

static uint32_t
sspiir_read(void *clientData,uint32_t address,int rqlen)
{
        return 0;
}

static void
sspicr_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{

}

static uint32_t
ssprxto_read(void *clientData,uint32_t address,int rqlen)
{
        return 0;
}

static void
ssprxto_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{

}

static void
LHSSP_UnMap(void *owner,uint32_t base,uint32_t mask)
{
	IOH_Delete32(SSPCR0(base));
	IOH_Delete32(SSPCR1(base));
	IOH_Delete32(SSPDR(base));
	IOH_Delete32(SSPSR(base));
	IOH_Delete32(SSPCPSR(base));
	IOH_Delete32(SSPIIR(base));
	IOH_Delete32(SSPICR(base));
	IOH_Delete32(SSPRXTO(base));
}

static void
LHSSP_Map(void *owner,uint32_t base,uint32_t mask,uint32_t flags)
{
	LH_SSP *ssp = (LH_SSP *) owner;
	IOH_New32(SSPCR0(base),sspcr0_read,sspcr0_write,ssp);
	IOH_New32(SSPCR1(base),sspcr1_read,sspcr1_write,ssp);
	IOH_New32(SSPDR(base),sspdr_read,sspdr_write,ssp);
	IOH_New32(SSPSR(base),sspsr_read,sspsr_write,ssp);
	IOH_New32(SSPCPSR(base),sspcpsr_read,sspcpsr_write,ssp);
	IOH_New32(SSPIIR(base),sspiir_read,NULL,ssp);
	IOH_New32(SSPICR(base),NULL,sspicr_write,ssp);
	IOH_New32(SSPRXTO(base),ssprxto_read,ssprxto_write,ssp);
}

BusDevice *
LH79520SSP_New(const char *name) 
{
	LH_SSP *ssp = sg_new(LH_SSP);	
	ssp->bdev.first_mapping=NULL;
        ssp->bdev.Map=LHSSP_Map;
        ssp->bdev.UnMap=LHSSP_UnMap;
        ssp->bdev.owner=ssp;
        ssp->bdev.hw_flags=MEM_FLAG_WRITABLE|MEM_FLAG_READABLE;
	/* The ssp_clock is the interface to the outside) */
	ssp->clk_in = Clock_New("%s.ssp_clock",name);
	ssp->clk_out = Clock_New("%s.sspclk",name);
	return &ssp->bdev;
}

