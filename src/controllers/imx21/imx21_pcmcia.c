/*
 *************************************************************************************************
 * Emulation of Freescale i.MX21 PCMCIA Controller  
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
#include "imx21_pcmcia.h"
#include "sgstring.h"

#define PCMCIA_PIPR(base)	((base) + 0x00)
#define PCMCIA_PSCR(base)	((base) + 0x04)
#define	PCMCIA_PER(base)	((base) + 0x08)
#define PCMCIA_PBR0(base)	((base) + 0x0c)
#define	PCMCIA_PBR1(base)	((base) + 0x10)
#define	PCMCIA_PBR2(base)	((base) + 0x14)
#define	PCMCIA_PBR3(base)	((base) + 0x18)
#define	PCMCIA_PBR4(base)	((base) + 0x1c)
#define	PCMCIA_POR0(base)	((base) + 0x28)
#define	PCMCIA_POR1(base)	((base) + 0x2c)
#define	PCMCIA_POR2(base)	((base) + 0x30)
#define	PCMCIA_POR3(base)	((base) + 0x34)
#define	PCMCIA_POR4(base)	((base) + 0x38)
#define PCMCIA_POFR0(base)	((base) + 0x44)
#define PCMCIA_POFR1(base)	((base) + 0x48)
#define PCMCIA_POFR2(base)	((base) + 0x4c)
#define PCMCIA_POFR3(base)	((base) + 0x50)
#define PCMCIA_POFR4(base)	((base) + 0x54)
#define PCMCIA_PGCR(base)	((base) + 0x60)
#define PCMCIA_PGSR(base)	((base) + 0x64)

typedef struct IMX21Pcmcia {
	BusDevice bdev;
} IMX21Pcmcia;

static uint32_t
pipr_read(void *clientData,uint32_t address,int rqlen)
{
	fprintf(stderr,"PCMCIA: register 0x%08x not implemented\n",address);
	return 0;
}

static void
pipr_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	fprintf(stderr,"PCMCIA: write register 0x%08x not implemented\n",address);
}

static uint32_t
pscr_read(void *clientData,uint32_t address,int rqlen)
{
	fprintf(stderr,"PCMCIA: register 0x%08x not implemented\n",address);
	return 0;
}

static void
pscr_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	fprintf(stderr,"PCMCIA: write register 0x%08x not implemented\n",address);
}
static uint32_t
per_read(void *clientData,uint32_t address,int rqlen)
{
	fprintf(stderr,"PCMCIA: register 0x%08x not implemented\n",address);
	return 0;
}

static void
per_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	fprintf(stderr,"PCMCIA: write register 0x%08x not implemented\n",address);
}
static uint32_t
pbr0_read(void *clientData,uint32_t address,int rqlen)
{
	fprintf(stderr,"PCMCIA: register 0x%08x not implemented\n",address);
	return 0;
}

static void
pbr0_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	fprintf(stderr,"PCMCIA: write register 0x%08x not implemented\n",address);
}
static uint32_t
pbr1_read(void *clientData,uint32_t address,int rqlen)
{
	fprintf(stderr,"PCMCIA: register 0x%08x not implemented\n",address);
	return 0;
}

static void
pbr1_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	fprintf(stderr,"PCMCIA: write register 0x%08x not implemented\n",address);
}
static uint32_t
pbr2_read(void *clientData,uint32_t address,int rqlen)
{
	fprintf(stderr,"PCMCIA: register 0x%08x not implemented\n",address);
	return 0;
}

static void
pbr2_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	fprintf(stderr,"PCMCIA: write register 0x%08x not implemented\n",address);
}
static uint32_t
pbr3_read(void *clientData,uint32_t address,int rqlen)
{
	fprintf(stderr,"PCMCIA: register 0x%08x not implemented\n",address);
	return 0;
}

static void
pbr3_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	fprintf(stderr,"PCMCIA: write register 0x%08x not implemented\n",address);
}
static uint32_t
pbr4_read(void *clientData,uint32_t address,int rqlen)
{
	fprintf(stderr,"PCMCIA: register 0x%08x not implemented\n",address);
	return 0;
}

static void
pbr4_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	fprintf(stderr,"PCMCIA: write register 0x%08x not implemented\n",address);
}
static uint32_t
por0_read(void *clientData,uint32_t address,int rqlen)
{
	fprintf(stderr,"PCMCIA: register 0x%08x not implemented\n",address);
	return 0;
}

static void
por0_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	fprintf(stderr,"PCMCIA: write register 0x%08x not implemented\n",address);
}
static uint32_t
por1_read(void *clientData,uint32_t address,int rqlen)
{
	fprintf(stderr,"PCMCIA: register 0x%08x not implemented\n",address);
	return 0;
}

static void
por1_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	fprintf(stderr,"PCMCIA: write register 0x%08x not implemented\n",address);
}
static uint32_t
por2_read(void *clientData,uint32_t address,int rqlen)
{
	fprintf(stderr,"PCMCIA: register 0x%08x not implemented\n",address);
	return 0;
}

static void
por2_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	fprintf(stderr,"PCMCIA: write register 0x%08x not implemented\n",address);
}
static uint32_t
por3_read(void *clientData,uint32_t address,int rqlen)
{
	fprintf(stderr,"PCMCIA: register 0x%08x not implemented\n",address);
	return 0;
}

static void
por3_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	fprintf(stderr,"PCMCIA: write register 0x%08x not implemented\n",address);
}
static uint32_t
por4_read(void *clientData,uint32_t address,int rqlen)
{
	fprintf(stderr,"PCMCIA: register 0x%08x not implemented\n",address);
	return 0;
}

static void
por4_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	fprintf(stderr,"PCMCIA: write register 0x%08x not implemented\n",address);
}
static uint32_t
pofr0_read(void *clientData,uint32_t address,int rqlen)
{
	fprintf(stderr,"PCMCIA: register 0x%08x not implemented\n",address);
	return 0;
}

static void
pofr0_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	fprintf(stderr,"PCMCIA: write register 0x%08x not implemented\n",address);
}
static uint32_t
pofr1_read(void *clientData,uint32_t address,int rqlen)
{
	fprintf(stderr,"PCMCIA: register 0x%08x not implemented\n",address);
	return 0;
}

static void
pofr1_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	fprintf(stderr,"PCMCIA: write register 0x%08x not implemented\n",address);
}
static uint32_t
pofr2_read(void *clientData,uint32_t address,int rqlen)
{
	fprintf(stderr,"PCMCIA: register 0x%08x not implemented\n",address);
	return 0;
}

static void
pofr2_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	fprintf(stderr,"PCMCIA: write register 0x%08x not implemented\n",address);
}
static uint32_t
pofr3_read(void *clientData,uint32_t address,int rqlen)
{
	fprintf(stderr,"PCMCIA: register 0x%08x not implemented\n",address);
	return 0;
}

static void
pofr3_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	fprintf(stderr,"PCMCIA: write register 0x%08x not implemented\n",address);
}
static uint32_t
pofr4_read(void *clientData,uint32_t address,int rqlen)
{
	fprintf(stderr,"PCMCIA: register 0x%08x not implemented\n",address);
	return 0;
}

static void
pofr4_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	fprintf(stderr,"PCMCIA: write register 0x%08x not implemented\n",address);
}
static uint32_t
pgcr_read(void *clientData,uint32_t address,int rqlen)
{
	fprintf(stderr,"PCMCIA: register 0x%08x not implemented\n",address);
	return 0;
}

static void
pgcr_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	fprintf(stderr,"PCMCIA: write register 0x%08x not implemented\n",address);
}
static uint32_t
pgsr_read(void *clientData,uint32_t address,int rqlen)
{
	fprintf(stderr,"PCMCIA: register 0x%08x not implemented\n",address);
	return 0;
}

static void
pgsr_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	fprintf(stderr,"PCMCIA: write register 0x%08x not implemented\n",address);
}

static void
IMXPcmcia_Unmap(void *owner,uint32_t base,uint32_t mask)
{
	IOH_Delete32(PCMCIA_PIPR(base));
	IOH_Delete32(PCMCIA_PSCR(base));
	IOH_Delete32(PCMCIA_PER(base));
	IOH_Delete32(PCMCIA_PBR0(base));
	IOH_Delete32(PCMCIA_PBR1(base));
	IOH_Delete32(PCMCIA_PBR2(base));
	IOH_Delete32(PCMCIA_PBR3(base));
	IOH_Delete32(PCMCIA_PBR4(base));
	IOH_Delete32(PCMCIA_POR0(base));
	IOH_Delete32(PCMCIA_POR1(base));
	IOH_Delete32(PCMCIA_POR2(base));
	IOH_Delete32(PCMCIA_POR3(base));
	IOH_Delete32(PCMCIA_POR4(base));
	IOH_Delete32(PCMCIA_POFR0(base));
	IOH_Delete32(PCMCIA_POFR1(base));
	IOH_Delete32(PCMCIA_POFR2(base));
	IOH_Delete32(PCMCIA_POFR3(base));
	IOH_Delete32(PCMCIA_POFR4(base));
	IOH_Delete32(PCMCIA_PGCR(base));
	IOH_Delete32(PCMCIA_PGSR(base));
}

static void
IMXPcmcia_Map(void *owner,uint32_t base,uint32_t mask,uint32_t mapflags)
{
	IMX21Pcmcia *pc = (IMX21Pcmcia *) owner;
	IOH_New32(PCMCIA_PIPR(base),pipr_read,pipr_write,pc);
	IOH_New32(PCMCIA_PSCR(base),pscr_read,pscr_write,pc);
	IOH_New32(PCMCIA_PER(base),per_read,per_write,pc);
	IOH_New32(PCMCIA_PBR0(base),pbr0_read,pbr0_write,pc);
	IOH_New32(PCMCIA_PBR1(base),pbr1_read,pbr1_write,pc);
	IOH_New32(PCMCIA_PBR2(base),pbr2_read,pbr2_write,pc);
	IOH_New32(PCMCIA_PBR3(base),pbr3_read,pbr3_write,pc);
	IOH_New32(PCMCIA_PBR4(base),pbr4_read,pbr4_write,pc);
	IOH_New32(PCMCIA_POR0(base),por0_read,por0_write,pc);
	IOH_New32(PCMCIA_POR1(base),por1_read,por1_write,pc);
	IOH_New32(PCMCIA_POR2(base),por2_read,por2_write,pc);
	IOH_New32(PCMCIA_POR3(base),por3_read,por3_write,pc);
	IOH_New32(PCMCIA_POR4(base),por4_read,por4_write,pc);
	IOH_New32(PCMCIA_POFR0(base),pofr0_read,pofr0_write,pc);
	IOH_New32(PCMCIA_POFR1(base),pofr1_read,pofr1_write,pc);
	IOH_New32(PCMCIA_POFR2(base),pofr2_read,pofr2_write,pc);
	IOH_New32(PCMCIA_POFR3(base),pofr3_read,pofr3_write,pc);
	IOH_New32(PCMCIA_POFR4(base),pofr4_read,pofr4_write,pc);
	IOH_New32(PCMCIA_PGCR(base),pgcr_read,pgcr_write,pc);
	IOH_New32(PCMCIA_PGSR(base),pgsr_read,pgsr_write,pc);
}

BusDevice *
IMX21Pcmcia_New(const char *name)
{
	IMX21Pcmcia *pc = sg_new(IMX21Pcmcia);	
	pc->bdev.first_mapping=NULL;
        pc->bdev.Map=IMXPcmcia_Map;
        pc->bdev.UnMap=IMXPcmcia_Unmap;
       	pc->bdev.owner=pc;
        pc->bdev.hw_flags=MEM_FLAG_WRITABLE|MEM_FLAG_READABLE;
        return &pc->bdev;

}
