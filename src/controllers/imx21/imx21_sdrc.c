/*
 *************************************************************************************************
 * Emulation of Freescale iMX21 SDRAM controller module 
 *
 * state: not implemented
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
#include "signode.h"
#include "imx21_sdrc.h"
#include "sgstring.h"

#define SDRC_SDCTL0(base)	((base)+0)
#define SDRC_SDCTL1(base)	((base)+0x4)
#define		SDCTL_SDE		(1<<31)
#define		SDCTL_SMODE_MASK	(0x7<<28)
#define		SDCTL_SMODE_SHIFT	(28)
#define		SDCTL_SMODE(x)	(((x)>>28) & 7)
#define		SDCTL_SP		(1<<27)
#define		SDCTL_ROW_MASK		(3<<24)
#define		SDCTL_ROW_SHIFT		(24)	
#define		SDCTL_ROW(x)	(((x)>>24) & 3)
#define		SDCTL_COL_MASK		(3<<20)
#define		SDCTL_COL_SHIFT		(20)
#define		SDCTL_COL(x)		(((x)>>20) & 3)
#define		SDCTL_IAM		(1<<19)
#define		SDCTL_DSIZ_MASK		(3<<16)
#define		SDCTL_DSIZ_SHIFT	(16)
#define		SDCTL_DSIZ(x)		(((x)>>16) & 3)
#define		SDCTL_SREFR_MASK	(3<<14)
#define		SDCTL_SREFR_SHIFT	(14)
#define		SDCTL_SREFR(x)		(((x)>>14) & 3)
#define		SDCTL_PWDT_MASK		(3<<12)
#define		SDCTL_PWDT_SHIFT	(12)
#define		SDCTL_PWDT(x)		(((x)>>12) & 3)
#define		SDCTL_CI_MASK		(3<<10)
#define		SDCTL_CI_SHIFT		(10)
#define		SDCTL_CI(x)		(((x)>>10) & 3)
#define		SDCTL_SCL_MASK		(3<<8)
#define		SDCTL_SCL_SHIFT		(8)
#define		SDCTL_SCL(x)		(((x)>>8) & 3)
#define		SDCTL_SRP		(1<<6)
#define		SDCTL_SRCD_MASK		(3<<4)
#define		SDCTL_SRCD_SHIFT	(4)
#define		SDCTL_SRCD(x)		(((x)>>4) & 3)
#define		SDCTL_SRC_MASK		(0xf)
#define		SDCTL_SRC_SHIFT		(0)
#define		SDCTL_SRC(x)		(((x)>>0) & 0xf)
#define	SDRC_SDRST(base)	((base)+0x18)
#define		SDRST_RST_MASK	(3<<30)
#define		SDRST_RST_SHIFT	(30)
#define		SDRST_RST(x)	(((x)>>30) & 0x3)
#define	SDRC_MISC(base)		((base)+0x14)
#define		MISC_OMA	(1<<31)
#define		MISC_RMA0	(1<<0)

typedef struct IMX21Sdrc {
	uint32_t sdctl[2];
	BusDevice *dram[2];
	uint32_t misc;
	BusDevice bdev;
} IMX21Sdrc;

static uint32_t
sdctl_read(void *clientData,uint32_t address,int rqlen)
{
	IMX21Sdrc *sdrc = (IMX21Sdrc*) clientData;
	int index = (address & 4) >> 2;
	
        fprintf(stderr,"SDRC: SDCTL%d register read\n",index);
        return sdrc->sdctl[index];
}

static void
sdctl_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	IMX21Sdrc *sdrc = (IMX21Sdrc*) clientData;
	int index = (address & 4) >> 2;
	int cl;
	int dump = 0;
	//BusDevice *dram = sdrc->dram[index];
	cl = SDCTL_SCL(value);
        fprintf(stderr,"SDRC: SDCTL%d register write %08x: CL%d\n",index,value,cl);
	if(dump) {
		fprintf(stderr,"SDE: %d\n",!!(value & SDCTL_SDE)); 
		fprintf(stderr,"SMODE: %d\n",(value & SDCTL_SMODE_MASK) >> SDCTL_SMODE_SHIFT); 
		fprintf(stderr,"SP: %d\n",!!(value & SDCTL_SP)); 
		fprintf(stderr,"ROW: %d\n",(value & SDCTL_ROW_MASK) >> SDCTL_ROW_SHIFT); 
		fprintf(stderr,"COL: %d\n",(value & SDCTL_COL_MASK) >> SDCTL_COL_SHIFT); 
		fprintf(stderr,"IAM: %d\n",!!(value & SDCTL_IAM)); 
		fprintf(stderr,"DSIZ: %d\n",(value & SDCTL_DSIZ_MASK) >> SDCTL_DSIZ_SHIFT); 
		fprintf(stderr,"SREFR: %d\n",(value & SDCTL_SREFR_MASK) >> SDCTL_SREFR_SHIFT); 
		fprintf(stderr,"PWDT: %d\n",(value & SDCTL_PWDT_MASK) >> SDCTL_PWDT_SHIFT); 
		fprintf(stderr,"CI: %d\n",(value & SDCTL_CI_MASK) >> SDCTL_CI_SHIFT); 
		fprintf(stderr,"SCL: %d\n",(value & SDCTL_SCL_MASK) >> SDCTL_SCL_SHIFT); 
		fprintf(stderr,"SRP: %d\n",!!(value & SDCTL_SRP)); 
		fprintf(stderr,"SRCD: %d\n",(value & SDCTL_SRCD_MASK) >> SDCTL_SRCD_SHIFT); 
		fprintf(stderr,"SRC: %d\n",(value & SDCTL_SRC_MASK) >> SDCTL_SRC_SHIFT); 
	}
	sdrc->sdctl[index] = value & 0xfb3fff7f;	
}

static uint32_t
sdrst_read(void *clientData,uint32_t address,int rqlen)
{
        fprintf(stderr,"SDRC: SDRST register read not implemented\n");
        return 0;
}

static void
sdrst_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
        fprintf(stderr,"SDRC: SDRST register write not implemented\n");
}
static uint32_t
misc_read(void *clientData,uint32_t address,int rqlen)
{
        fprintf(stderr,"SDRC: MISC register read not implemented\n");
        return 0;
}

static void
misc_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
        fprintf(stderr,"SDRC: MISC register write not implemented\n");
}

static void
IMXSdrc_Unmap(void *owner,uint32_t base,uint32_t mask)
{
        IOH_Delete32(SDRC_SDCTL0(base));
        IOH_Delete32(SDRC_SDCTL1(base));
        IOH_Delete32(SDRC_SDRST(base));
        IOH_Delete32(SDRC_MISC(base));
}

static void
IMXSdrc_Map(void *owner,uint32_t base,uint32_t mask,uint32_t mapflags)
{

        IMX21Sdrc *sdrc = (IMX21Sdrc *) owner;
        IOH_New32(SDRC_SDCTL0(base),sdctl_read,sdctl_write,sdrc);
	IOH_New32(SDRC_SDCTL1(base),sdctl_read,sdctl_write,sdrc);
	IOH_New32(SDRC_SDRST(base),sdrst_read,sdrst_write,sdrc);
	IOH_New32(SDRC_MISC(base),misc_read,misc_write,sdrc);
}

BusDevice *
IMX21_SdrcNew(const char *name,BusDevice *dram0,BusDevice *dram1)
{
        IMX21Sdrc *sdrc;
        sdrc = sg_new(IMX21Sdrc);
	sdrc->dram[0] = dram0;
	sdrc->dram[1] = dram1;
	sdrc->sdctl[0] = 0x01000300;
	sdrc->sdctl[1] = 0x01060300;
        sdrc->bdev.first_mapping=NULL;
        sdrc->bdev.Map=IMXSdrc_Map;
        sdrc->bdev.UnMap=IMXSdrc_Unmap;
        sdrc->bdev.owner=sdrc;
        sdrc->bdev.hw_flags=MEM_FLAG_WRITABLE|MEM_FLAG_READABLE;
        fprintf(stderr,"i.MX21 SDRAM controller (SDRC) created\n");
        return &sdrc->bdev;
}

