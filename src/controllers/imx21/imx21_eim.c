/*
 *************************************************************************************************
*
 * Emulation of Freescale iMX21 External Interface Module (EIM)
 *
 * state: no functionality implemented
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
#include "imx21_eim.h"
#include "sgstring.h"

#define EIM_CS0U(base)		((base) + 0)
#define 	CSxU_SP			(1<<31)
#define		CSxU_WP			(1<<30)
#define		CSxU_BCDDCT_MASK	(3<<28)
#define		CSxU_BCDDCT_SHIFT	(28)
#define		CSxU_BCSRWA_MASK	(0xf<<24)
#define		CSxU_BCSRWA_SHIFT	(24)
#define		CSxU_PSZ_MASK		(0x3<<22)
#define		CSxU_PSZ_SHIFT		(22)
#define		CSxU_PME		(1<<21)
#define		CSxU_SYNC		(1<<20)
#define		CSxU_DOLRWN_MASK	(0xf<<16)
#define		CSxU_DOLRWN_SHIFT	(16)
#define		CSxU_CNC_MASK		(3<<14)
#define		CSxU_CNC_SHIFT		(14)
#define		CSxU_WSC_MASK		(0x3f<<8)
#define		CSxU_WSC_SHIFT		(8)
#define		CSxU_EW			(1<<7)
#define		CSxU_WWS_MASK		(7<<4)
#define		CSxU_WWS_SHIFT		(4)
#define		CSxU_EDC_MASK		(0xf)
#define		CSxU_EDC_SHIFT		(0)
#define EIM_CS0L(base)		((base) + 4)
#define		CSxL_OEA_MASK		(0xf<<28)
#define		CSxL_OEA_SHIFT		(28)
#define		CSxL_OEN_MASK		(0xf<<24)
#define		CSxL_OEN_SHIFT		(24)
#define		CSxL_WEA_MASK		(0xf<<20)
#define		CSxL_WEA_SHIFT		(20)
#define		CSxL_WEN_MASK		(0xf<<16)
#define		CSxL_WEN_SHIFT		(16)
#define		CSxL_CSA_MASK		(0xf<<12)
#define		CSxL_CSA_SHIFT		(12)
#define		CSxL_EBC		(1<<11)
#define		CSxL_DSZ_MASK		(7<<8)
#define		CSxL_DSZ_SHIFT		(8)
#define		CSxL_CSN_MASK		(0xf<<4)
#define		CSxL_CSN_SHIFT		(4)
#define 	CSxL_PSR		(1<<3)
#define		CSxL_CRE		(1<<2)
#define		CSxL_WRAP		(1<<1)
#define		CSxL_CSEN		(1<<0)
#define EIM_CS1U(base)  	((base) + 8)
#define EIM_CS1L(base) 		((base) + 0xc)
#define EIM_CS2U(base)		((base) + 0x10)
#define EIM_CS2L(base)		((base) + 0x14)
#define EIM_CS3U(base)		((base) + 0x18)
#define EIM_CS3L(base)		((base) + 0x1c)
#define EIM_CS4U(base)		((base) + 0x20)
#define EIM_CS4L(base)		((base) + 0x24)
#define EIM_CS5U(base)		((base) + 0x28)
#define EIM_CS5L(base)		((base) + 0x2c)
#define EIM_CNF(base)		((base) + 0x30)
#define		CNF_BCM		(1<<2)
#define		CNF_AGE		(1<<1)

typedef struct IMX21Eim {
	BusDevice bdev;

	/* The registers */
	uint32_t csx_u[6];
	uint32_t csx_l[6];
	uint32_t cnf;

	/* The connected devices */
	BusDevice *devices[6]; 
} IMX21Eim;

static void
dump_csu(IMX21Eim *eim,int idx) 
{
	uint32_t val = eim->csx_u[idx];
	fprintf(stderr,"cs%du is %08x\n",idx,val);
	fprintf(stderr,"CSU%d_SP %01x\n",idx,!!(val & CSxU_SP));
	fprintf(stderr,"CSU%d_WP %01x\n",idx,!!(val & CSxU_WP));
	fprintf(stderr,"CSU%d_BCDDCT %01x\n",idx,(val & CSxU_BCDDCT_MASK) >> CSxU_BCDDCT_SHIFT);
	fprintf(stderr,"CSU%d_BCSRWA %01x\n",idx,(val & CSxU_BCSRWA_MASK) >> CSxU_BCSRWA_SHIFT);
	fprintf(stderr,"CSU%d_PSZ %01x\n",idx,(val & CSxU_PSZ_MASK) >> CSxU_PSZ_SHIFT);
	fprintf(stderr,"CSU%d_PME %01x\n",idx,!!(val & CSxU_PME));
	fprintf(stderr,"CSU%d_SYNC %01x\n",idx,!!(val & CSxU_SYNC));
	fprintf(stderr,"CSU%d_DOLRWN %01x\n",idx,(val & CSxU_DOLRWN_MASK) >> CSxU_DOLRWN_SHIFT);
	fprintf(stderr,"CSU%d_CNC %01x\n",idx,(val & CSxU_CNC_MASK) >> CSxU_CNC_SHIFT);
	fprintf(stderr,"CSU%d_WSC %02x\n",idx,(val & CSxU_WSC_MASK) >> CSxU_WSC_SHIFT);
	fprintf(stderr,"CSU%d_EW %01x\n",idx,!!(val & CSxU_EW));
	fprintf(stderr,"CSU%d_WWS %01x\n",idx,(val & CSxU_WWS_MASK) >> CSxU_WWS_SHIFT);
	fprintf(stderr,"CSU%d_EDC %01x\n",idx,(val & CSxU_EDC_MASK) >> CSxU_EDC_SHIFT);
	
}

static void
dump_csl(IMX21Eim *eim,int idx) 
{
	uint32_t val = eim->csx_l[idx];
	fprintf(stderr,"cs%dl is %08x\n",idx,val);
	fprintf(stderr,"CSL%d_OEA %01x\n",idx,(val & CSxL_OEA_MASK) >> CSxL_OEA_SHIFT);
	fprintf(stderr,"CSL%d_OEN %01x\n",idx,(val & CSxL_OEN_MASK) >> CSxL_OEN_SHIFT);
	fprintf(stderr,"CSL%d_WEA %01x\n",idx,(val & CSxL_WEA_MASK) >> CSxL_WEA_SHIFT);
	fprintf(stderr,"CSL%d_WEN %01x\n",idx,(val & CSxL_WEN_MASK) >> CSxL_WEN_SHIFT);
	fprintf(stderr,"CSL%d_CSA %01x\n",idx,(val & CSxL_CSA_MASK) >> CSxL_CSA_SHIFT);
	fprintf(stderr,"CSL%d_EBC %01x\n",idx,!!(val & CSxL_EBC));
	fprintf(stderr,"CSL%d_DSZ %01x\n",idx,(val & CSxL_DSZ_MASK) >> CSxL_DSZ_SHIFT);
	fprintf(stderr,"CSL%d_CSN %01x\n",idx,(val & CSxL_CSN_MASK) >> CSxL_CSN_SHIFT);
	fprintf(stderr,"CSL%d_PSR %01x\n",idx,!!(val & CSxL_PSR));
	fprintf(stderr,"CSL%d_CRE %01x\n",idx,!!(val & CSxL_CRE));
	fprintf(stderr,"CSL%d_WRAP %01x\n",idx,!!(val & CSxL_WRAP));
	fprintf(stderr,"CSL%d_CSEN %01x\n",idx,!!(val & CSxL_CSEN));
	
}

/* 
 * -------------------------------------------------------------
 * CS0
 * 	Chip select for Boot device (usually flash)
 * -------------------------------------------------------------
 */
static uint32_t
eim_cs0u_read(void *clientData,uint32_t address,int rqlen)
{
	fprintf(stderr,"EIM register 0x%08x not implemented\n",address);
        return 0;
}

static void
eim_cs0u_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	fprintf(stderr,"EIM register 0x%08x not implemented\n",address);
}

static uint32_t
eim_cs0l_read(void *clientData,uint32_t address,int rqlen)
{
	fprintf(stderr,"EIM register 0x%08x not implemented\n",address);
        return 0;
}

static void
eim_cs0l_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	fprintf(stderr,"EIM register 0x%08x not implemented\n",address);
}

static uint32_t
eim_cs1u_read(void *clientData,uint32_t address,int rqlen)
{
	fprintf(stderr,"EIM register 0x%08x not implemented\n",address);
        return 0;
}

static void
eim_cs1u_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	IMX21Eim *eim = (IMX21Eim*) clientData;
	if(0) {
		dump_csu(eim,1);
	}
	eim->csx_u[1] = value;
}

static uint32_t
eim_cs1l_read(void *clientData,uint32_t address,int rqlen)
{
	fprintf(stderr,"EIM register 0x%08x not implemented\n",address);
        return 0;
}

static void
eim_cs1l_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	IMX21Eim *eim = (IMX21Eim*) clientData;
	eim->csx_l[1] = value;
	if(0) {
		dump_csl(eim,1);
	}
}

static uint32_t
eim_cs2u_read(void *clientData,uint32_t address,int rqlen)
{
	fprintf(stderr,"EIM register 0x%08x not implemented\n",address);
        return 0;
}

static void
eim_cs2u_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	fprintf(stderr,"EIM register 0x%08x not implemented\n",address);
}

static uint32_t
eim_cs2l_read(void *clientData,uint32_t address,int rqlen)
{
	fprintf(stderr,"EIM register 0x%08x not implemented\n",address);
        return 0;
}

static void
eim_cs2l_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	fprintf(stderr,"EIM register 0x%08x not implemented\n",address);
}

static uint32_t
eim_cs3u_read(void *clientData,uint32_t address,int rqlen)
{
	fprintf(stderr,"EIM register 0x%08x not implemented\n",address);
        return 0;
}

static void
eim_cs3u_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	fprintf(stderr,"EIM register 0x%08x not implemented\n",address);
}

static uint32_t
eim_cs3l_read(void *clientData,uint32_t address,int rqlen)
{
	fprintf(stderr,"EIM register 0x%08x not implemented\n",address);
        return 0;
}

static void
eim_cs3l_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	fprintf(stderr,"EIM register 0x%08x not implemented\n",address);
}

static uint32_t
eim_cs4u_read(void *clientData,uint32_t address,int rqlen)
{
	fprintf(stderr,"EIM register 0x%08x not implemented\n",address);
        return 0;
}

static void
eim_cs4u_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	fprintf(stderr,"EIM register 0x%08x not implemented\n",address);
}

static uint32_t
eim_cs4l_read(void *clientData,uint32_t address,int rqlen)
{
	fprintf(stderr,"EIM register 0x%08x not implemented\n",address);
        return 0;
}

static void
eim_cs4l_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	fprintf(stderr,"EIM register 0x%08x not implemented\n",address);
}

static uint32_t
eim_cs5u_read(void *clientData,uint32_t address,int rqlen)
{
	fprintf(stderr,"EIM register 0x%08x not implemented\n",address);
        return 0;
}

static void
eim_cs5u_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	fprintf(stderr,"EIM register 0x%08x not implemented\n",address);
}

static uint32_t
eim_cs5l_read(void *clientData,uint32_t address,int rqlen)
{
	fprintf(stderr,"EIM register 0x%08x not implemented\n",address);
        return 0;
}

static void
eim_cs5l_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	fprintf(stderr,"EIM register 0x%08x not implemented\n",address);
}


static uint32_t
eim_cnf_read(void *clientData,uint32_t address,int rqlen)
{
	fprintf(stderr,"EIM register 0x%08x not implemented\n",address);
        return 0;
}

static void
eim_cnf_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	fprintf(stderr,"EIM register 0x%08x not implemented\n",address);
}


static void
IMXEim_Unmap(void *owner,uint32_t base,uint32_t mask)
{
        IOH_Delete32(EIM_CS0U(base));
        IOH_Delete32(EIM_CS0L(base));
        IOH_Delete32(EIM_CS1U(base));
        IOH_Delete32(EIM_CS1L(base));
        IOH_Delete32(EIM_CS2U(base));
        IOH_Delete32(EIM_CS2L(base));
        IOH_Delete32(EIM_CS3U(base));
        IOH_Delete32(EIM_CS3L(base));
        IOH_Delete32(EIM_CS4U(base));
        IOH_Delete32(EIM_CS4L(base));
        IOH_Delete32(EIM_CS5U(base));
        IOH_Delete32(EIM_CS5L(base));
        IOH_Delete32(EIM_CNF(base));

}

static void
IMXEim_Map(void *owner,uint32_t base,uint32_t mask,uint32_t mapflags)
{

	IMX21Eim *eim = (IMX21Eim *) owner;
        IOH_New32(EIM_CS0U(base),eim_cs0u_read,eim_cs0u_write,eim);
        IOH_New32(EIM_CS0L(base),eim_cs0l_read,eim_cs0l_write,eim);
        IOH_New32(EIM_CS1U(base),eim_cs1u_read,eim_cs1u_write,eim);
        IOH_New32(EIM_CS1L(base),eim_cs1l_read,eim_cs1l_write,eim);
        IOH_New32(EIM_CS2U(base),eim_cs2u_read,eim_cs2u_write,eim);
        IOH_New32(EIM_CS2L(base),eim_cs2l_read,eim_cs2l_write,eim);
        IOH_New32(EIM_CS3U(base),eim_cs3u_read,eim_cs3u_write,eim);
        IOH_New32(EIM_CS3L(base),eim_cs3l_read,eim_cs3l_write,eim);
        IOH_New32(EIM_CS4U(base),eim_cs4u_read,eim_cs4u_write,eim);
        IOH_New32(EIM_CS4L(base),eim_cs4l_read,eim_cs4l_write,eim);
        IOH_New32(EIM_CS5U(base),eim_cs5u_read,eim_cs5u_write,eim);
        IOH_New32(EIM_CS5L(base),eim_cs5l_read,eim_cs5l_write,eim);
        IOH_New32(EIM_CNF(base),eim_cnf_read,eim_cnf_write,eim);
}

#if 0
IMX21_EimRegisterDevice(eimdev,CSX,dev)
#endif
BusDevice * 
IMX21_EimNew(const char *name) 
{
	IMX21Eim *eim;
	int i;
	eim = sg_new(IMX21Eim);
	eim->bdev.first_mapping=NULL;
        eim->bdev.Map=IMXEim_Map;
        eim->bdev.UnMap=IMXEim_Unmap;
        eim->bdev.owner=eim;
        eim->bdev.hw_flags=MEM_FLAG_WRITABLE|MEM_FLAG_READABLE;
	/* Should check boot config pins here to set device size (DSZ) */
	eim->csx_u[0] = 0x00003e00;
	eim->csx_l[0] = 0x20000801;
	for(i=1;i<6;i++) {
		eim->csx_u[i] = 0x00000000;
		eim->csx_l[i] = 0x00000800;
	}
	eim->cnf = 0;
	fprintf(stderr,"External memory interface module (EIM) created\n");
	return &eim->bdev;
}

