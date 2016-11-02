/*
 *************************************************************************************************
 *
 * Emulation of MFC5282 Chip select module 
 *
 * state:  Not implemented
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
#include <sgstring.h>

#define CSM_CSAR(base,x)	((base) + 0x0 + (x) * 12)
#define CSM_CSMR(base,x)	((base) + 0x4 + (x) * 12)
#define		CSMR_V		(1<<0)
#define 	CSMR_UD		(1<<1)
#define		CSMR_UC		(1<<2)
#define		CSMR_SD		(1<<3)
#define		CSMR_SC		(1<<4)
#define		CSMR_CI		(1<<5)
#define		CSMR_AM		(1<<6)
#define		CSMR_WP		(1<<8)
#define		CSMR_BAM_MASK	(0xffff<<16)
#define		CSMR_BAM_SHIFT	(16)

#define CSM_CSCR(base,x)	((base) + 0x8 + (x) * 12)
#define		CSCR_BSTW	(1<<3)
#define		CSCR_BSTR	(1<<4)
#define		CSCR_BEM	(1<<5)
#define		CSCR_PS0	(1<<6)
#define		CSCR_PS1	(1<<7)
#define		CSCR_AA		(1<<8)
#define		CSCR_WS_MASK	(0xf << 10)
#define		CSCR_WS_SHIFT	(10)

typedef struct Csm Csm; 

typedef struct ChipSelect {
	Csm *csm;	
	BusDevice *dev;
	uint16_t csar;
	uint32_t csmr;
	uint32_t cscr;
} ChipSelect;

struct Csm {
	BusDevice bdev;
	ChipSelect cs[7];
};

static void
UpdateMappings(Csm *csm)
{
	if(!csm->cs[0].csmr & CSMR_V) {
		// Map it to everythere except ipsbar
	}
}
static uint32_t
csar_read(void *clientData,uint32_t address,int rqlen)
{
	ChipSelect *cs = (ChipSelect *) clientData;
        return cs->csar;
}

static void
csar_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	ChipSelect *cs = (ChipSelect *) clientData;
	cs->csar = value;
        fprintf(stderr,"CSM: csar not implemented\n");
	// update_mappings
}

static uint32_t
csmr_read(void *clientData,uint32_t address,int rqlen)
{
	ChipSelect *cs = (ChipSelect *) clientData;
        return cs->csmr;
}

static void
csmr_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	ChipSelect *cs = (ChipSelect *) clientData;
	cs->csmr = value;
        fprintf(stderr,"CSM: csmr not implemented\n");
	// update_mappings
}

static uint32_t
cscr_read(void *clientData,uint32_t address,int rqlen)
{
	ChipSelect *cs = (ChipSelect *) clientData;
	if(rqlen == 4) {
		return cs->cscr << 16;
	} else if((rqlen == 2) && ((address & 2) == 2)) {
		return cs->cscr;
	}
        return 0;
}

static void
cscr_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	ChipSelect *cs = (ChipSelect *) clientData;
	if(rqlen == 4) {
		cs->cscr  = value >> 16;
	} else if((rqlen == 2) && ((address & 2) == 2)) {
		cs->cscr = value;
	}
}

static void
Csm_Unmap(void *owner,uint32_t base,uint32_t mask)
{
	int i;
	for(i=0;i<7;i++) {
        	IOH_Delete32(CSM_CSAR(base,i));
        	IOH_Delete32(CSM_CSMR(base,i));
        	IOH_Delete32(CSM_CSCR(base,i));
	}
}

static void
Csm_Map(void *owner,uint32_t base,uint32_t mask,uint32_t mapflags)
{
        Csm *csm = (Csm *) owner;
	int i;
	for(i=0;i<7;i++) {
        	IOH_New32(CSM_CSAR(base,i),csar_read,csar_write,&csm->cs[i]);
        	IOH_New32(CSM_CSMR(base,i),csmr_read,csmr_write,&csm->cs[i]);
        	IOH_New32(CSM_CSCR(base,i),cscr_read,cscr_write,&csm->cs[i]);
	}
}

static void
Csm_RegisterDevice(Csm *csm,BusDevice *dev,unsigned int cs_nr) 
{
	ChipSelect *cs = &csm->cs[cs_nr];
	if(cs_nr >= 7) {
		fprintf(stderr,"Illegal Chip select %d\n",cs_nr);	
		exit(1);
	}
	cs = &csm->cs[cs_nr];
	cs->dev = dev;
	UpdateMappings(csm);
}

BusDevice *
MCF5282_CsmNew(const char *name)
{
        Csm *csm = sg_calloc(sizeof(Csm));
        csm->bdev.first_mapping=NULL;
        csm->bdev.Map=Csm_Map;
        csm->bdev.UnMap=Csm_Unmap;
        csm->bdev.owner=csm;
        csm->bdev.hw_flags=MEM_FLAG_WRITABLE|MEM_FLAG_READABLE;
        return &csm->bdev;
}
