/*
 *************************************************************************************************
 * Emulation of Coldfire MCF5282 Chip Configuration Module Controller 
 *
 * state: Not implemented 
 *
 * Copyright 2008 Jochen Karrer. All rights reserved.
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

#include <bus.h>
#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <bus.h>
#include <fio.h>
#include <signode.h>
#include <configfile.h>
#include <cycletimer.h>
#include <clock.h>
#include <sgstring.h>
#include <mcf5282_ccm.h>

#define CCM_CCR(x) 	((x)+0x110004)
#define CCM_LPCR(x) 	((x)+0x110006)
#define CCM_RCON(x)	((x)+0x110008)
#define CCM_CIR(x) 	((x)+0x11000a)
#define CCM_RESERVED(x)	((x)+0x11000c)
#define CCM_UNIMPL(x)	((x)+0x110010)

typedef struct CCM {
	BusDevice bdev;
} CCM;

static uint32_t
ccr_read(void *clientData,uint32_t address,int rqlen)
{
        fprintf(stderr,"CCR not implented\n");
        return 0;
}

static void
ccr_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
        fprintf(stderr,"CCR not implented\n");
}

static uint32_t
lpcr_read(void *clientData,uint32_t address,int rqlen)
{
        fprintf(stderr,"CCR not implented\n");
        return 0;
}

static void
lpcr_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
        fprintf(stderr,"CCR not implented\n");
}

static uint32_t
rcon_read(void *clientData,uint32_t address,int rqlen)
{
        fprintf(stderr,"CCM-RCON not implented\n");
        return 0;
}

static void
rcon_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
        fprintf(stderr,"CCM-RCON not implented\n");
}

static uint32_t
cir_read(void *clientData,uint32_t address,int rqlen)
{
        fprintf(stderr,"Read CCM-CIR Chip identification register\n");
        return 0x2000;
}

static void
cir_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
        fprintf(stderr,"CCM-CIR not implented\n");
}

static void
CCM_Unmap(void *owner,uint32_t base,uint32_t mask)
{
	IOH_Delete16(CCM_CCR(base));
	IOH_Delete16(CCM_LPCR(base));
	IOH_Delete16(CCM_RCON(base));
	IOH_Delete16(CCM_CIR(base));
}

static void
CCM_Map(void *owner,uint32_t base,uint32_t mask,uint32_t mapflags)
{
	CCM *ccm = (CCM *) owner;
	IOH_New16(CCM_CCR(base),ccr_read,ccr_write,ccm);
	IOH_New16(CCM_LPCR(base),lpcr_read,lpcr_write,ccm);
	IOH_New16(CCM_RCON(base),rcon_read,rcon_write,ccm);
	IOH_New16(CCM_CIR(base),cir_read,cir_write,ccm);
}

BusDevice *
MFC5282_CCMNew(const char *name)
{
        CCM *ccm = sg_calloc(sizeof(CCM));
        ccm->bdev.first_mapping=NULL;
        ccm->bdev.Map=CCM_Map;
        ccm->bdev.UnMap=CCM_Unmap;
        ccm->bdev.owner=ccm;
        ccm->bdev.hw_flags=MEM_FLAG_WRITABLE|MEM_FLAG_READABLE;
        return &ccm->bdev;
}

