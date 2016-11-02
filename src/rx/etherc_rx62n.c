/*
 **********************************************************************************************
 * Renesas RX62N Ethernet controller 
 *
 * State: Not implemented 
 *
 * Copyright 2012 Jochen Karrer. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification, are
 * permitted provided that the following conditions are met:
 *
 *   1. Redistributions of source code must retain the above copyright notice, this list of
 *       conditions and the following disclaimer.
 *
 *    2. Redistributions in binary form must reproduce the above copyright notice, this list
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
 **********************************************************************************************
 */

#include <stdint.h>
#include "sgstring.h" 
#include "sglib.h" 
#include "bus.h"
#include "etherc_rx62n.h"

#define REG_ECMR(base)		((base) + 0x00)
#define REG_RFLR(base)		((base) + 0x08)
#define REG_ECSR(base)		((base) + 0x10)
#define REG_ECSIPR(base)	((base) + 0x18)
#define REG_PIR(base)		((base) + 0x20)
#define REG_PSR(base)		((base) + 0x28)
#define REG_RDMLR(base)		((base) + 0x40)
#define	REG_IPGR(base)		((base) + 0x50)
#define REG_APR(base)		((base) + 0x54)
#define REG_MPR(base)		((base) + 0x58)
#define REG_RFCF(base)		((base) + 0x60)
#define REG_TPAUSER(base)	((base) + 0x64)
#define REG_TPAUSECR(base)	((base) + 0x68)
#define REG_BCFRR(base)		((base) + 0x6c)
#define REG_MAHR(base)		((base) + 0xc0)
#define REG_MALR(base)		((base) + 0xc8)
#define REG_TROCR(base)		((base) + 0xd0)
#define REG_CDCR(base)		((base) + 0xd4)
#define REG_LCCR(base)		((base) + 0xd8)
#define REG_CNDCR(base)		((base) + 0xdc)
#define REG_CEFCR(base)		((base) + 0xe4)
#define REG_FRECR(base)		((base) + 0xe8)
#define REG_TSFRCR(base)	((base) + 0xec)
#define REG_TLFRCR(base)	((base) + 0xf0)
#define REG_RFCR(base)		((base) + 0xf4)
#define REG_MAFCR(base)		((base) + 0xf8)

typedef struct RxEth {
	BusDevice bdev;
} RxEth;

static uint32_t
ecmr_read(void *clientData,uint32_t address,int rqlen)
{
        return 0;
}

static void
ecmr_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
}

static uint32_t
rflr_read(void *clientData,uint32_t address,int rqlen)
{
        return 0; 
}

static void
rflr_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
}

static uint32_t
ecsr_read(void *clientData,uint32_t address,int rqlen)
{
        return 0; 
}

static void
ecsr_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
}

static uint32_t
ecsipr_read(void *clientData,uint32_t address,int rqlen)
{
        return 0;
}

static void
ecsipr_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
}

static uint32_t
pir_read(void *clientData,uint32_t address,int rqlen)
{
        return 0;
}

static void
pir_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
}

static uint32_t
psr_read(void *clientData,uint32_t address,int rqlen)
{
        return 0;
}

static void
psr_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
}

static uint32_t
rdmlr_read(void *clientData,uint32_t address,int rqlen)
{
        return 0;
}

static void
rdmlr_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
}

static uint32_t
ipgr_read(void *clientData,uint32_t address,int rqlen)
{
        return 0;
}

static void
ipgr_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
}

static uint32_t
apr_read(void *clientData,uint32_t address,int rqlen)
{
        return 0;
}

static void
apr_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{

}

static uint32_t
mpr_read(void *clientData,uint32_t address,int rqlen)
{
        return 0;
}

static void
mpr_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{

}

static uint32_t
rfcf_read(void *clientData,uint32_t address,int rqlen)
{
        return 0;
}

static void
rfcf_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{

}

static uint32_t
tpauser_read(void *clientData,uint32_t address,int rqlen)
{
        return 0;
}

static void
tpauser_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{

}

static uint32_t
tpausecr_read(void *clientData,uint32_t address,int rqlen)
{
        return 0;
}

static void
tpausecr_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{

}

static uint32_t
bcfrr_read(void *clientData,uint32_t address,int rqlen)
{
        return 0;
}

static void
bcfrr_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{

}

static uint32_t
mahr_read(void *clientData,uint32_t address,int rqlen)
{
        return 0;
}

static void
mahr_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{

}

static uint32_t
malr_read(void *clientData,uint32_t address,int rqlen)
{
        return 0;
}

static void
malr_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{

}

static uint32_t
trocr_read(void *clientData,uint32_t address,int rqlen)
{
        return 0;
}

static void
trocr_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{

}

static uint32_t
cdcr_read(void *clientData,uint32_t address,int rqlen)
{
        return 0;
}

static void
cdcr_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{

}

static uint32_t
lccr_read(void *clientData,uint32_t address,int rqlen)
{
        return 0;
}

static void
lccr_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{

}

static uint32_t
cndcr_read(void *clientData,uint32_t address,int rqlen)
{
        return 0;
}

static void
cndcr_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{

}

static uint32_t
cefcr_read(void *clientData,uint32_t address,int rqlen)
{
        return 0;
}

static void
cefcr_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{

}

static uint32_t
frecr_read(void *clientData,uint32_t address,int rqlen)
{
        return 0;
}

static void
frecr_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{

}

static uint32_t
tsfrcr_read(void *clientData,uint32_t address,int rqlen)
{
        return 0;
}

static void
tsfrcr_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{

}

static uint32_t
tlfrcr_read(void *clientData,uint32_t address,int rqlen)
{
        return 0;
}

static void
tlfrcr_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{

}

static uint32_t
rfcr_read(void *clientData,uint32_t address,int rqlen)
{
        return 0;
}

static void
rfcr_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{

}

static uint32_t
mafcr_read(void *clientData,uint32_t address,int rqlen)
{
        return 0;
}

static void
mafcr_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{

}

static void
RxEth_Unmap(void *owner,uint32_t base,uint32_t mask)
{
	IOH_Delete32(REG_ECMR(base));
	IOH_Delete32(REG_RFLR(base));
	IOH_Delete32(REG_ECSR(base));
	IOH_Delete32(REG_ECSIPR(base));
	IOH_Delete32(REG_PIR(base));
	IOH_Delete32(REG_PSR(base));
	IOH_Delete32(REG_RDMLR(base));
	IOH_Delete32(REG_IPGR(base));
	IOH_Delete32(REG_APR(base));
	IOH_Delete32(REG_MPR(base));
	IOH_Delete32(REG_RFCF(base));
	IOH_Delete32(REG_TPAUSER(base));
	IOH_Delete32(REG_TPAUSECR(base));
	IOH_Delete32(REG_BCFRR(base));
	IOH_Delete32(REG_MAHR(base));
	IOH_Delete32(REG_MALR(base));
	IOH_Delete32(REG_TROCR(base));
	IOH_Delete32(REG_CDCR(base));
	IOH_Delete32(REG_LCCR(base));
	IOH_Delete32(REG_CNDCR(base));
	IOH_Delete32(REG_CEFCR(base));
	IOH_Delete32(REG_FRECR(base));
	IOH_Delete32(REG_TSFRCR(base));
	IOH_Delete32(REG_TLFRCR(base));
	IOH_Delete32(REG_RFCR(base));
	IOH_Delete32(REG_MAFCR(base));
}

static void
RxEth_Map(void *owner,uint32_t base,uint32_t mask,uint32_t mapflags)
{
        RxEth *re = owner;
	IOH_New32(REG_ECMR(base),ecmr_read,ecmr_write,re);
	IOH_New32(REG_RFLR(base),rflr_read,rflr_write,re);
	IOH_New32(REG_ECSR(base),ecsr_read,ecsr_write,re);
	IOH_New32(REG_ECSIPR(base),ecsipr_read,ecsipr_write,re);
	IOH_New32(REG_PIR(base),pir_read,pir_write,re);
	IOH_New32(REG_PSR(base),psr_read,psr_write,re);
	IOH_New32(REG_RDMLR(base),rdmlr_read,rdmlr_write,re);
	IOH_New32(REG_IPGR(base),ipgr_read,ipgr_write,re);
	IOH_New32(REG_APR(base),apr_read,apr_write,re);
	IOH_New32(REG_MPR(base),mpr_read,mpr_write,re);
	IOH_New32(REG_RFCF(base),rfcf_read,rfcf_write,re);
	IOH_New32(REG_TPAUSER(base),tpauser_read,tpauser_write,re);
	IOH_New32(REG_TPAUSECR(base),tpausecr_read,tpausecr_write,re);
	IOH_New32(REG_BCFRR(base),bcfrr_read,bcfrr_write,re);
	IOH_New32(REG_MAHR(base),mahr_read,mahr_write,re);
	IOH_New32(REG_MALR(base),malr_read,malr_write,re);
	IOH_New32(REG_TROCR(base),trocr_read,trocr_write,re);
	IOH_New32(REG_CDCR(base),cdcr_read,cdcr_write,re);
	IOH_New32(REG_LCCR(base),lccr_read,lccr_write,re);
	IOH_New32(REG_CNDCR(base),cndcr_read,cndcr_write,re);
	IOH_New32(REG_CEFCR(base),cefcr_read,cefcr_write,re);
	IOH_New32(REG_FRECR(base),frecr_read,frecr_write,re);
	IOH_New32(REG_TSFRCR(base),tsfrcr_read,tsfrcr_write,re);
	IOH_New32(REG_TLFRCR(base),tlfrcr_read,tlfrcr_write,re);
	IOH_New32(REG_RFCR(base),rfcr_read,rfcr_write,re);
	IOH_New32(REG_MAFCR(base),mafcr_read,mafcr_write,re);
}

BusDevice *
Rx62nEtherC_New(const char *name)
{
	RxEth *re = sg_new(RxEth);
	re->bdev.first_mapping = NULL;
        re->bdev.Map = RxEth_Map;
        re->bdev.UnMap = RxEth_Unmap;
        re->bdev.owner = re;
        re->bdev.hw_flags = MEM_FLAG_WRITABLE|MEM_FLAG_READABLE;
	return &re->bdev;
}
