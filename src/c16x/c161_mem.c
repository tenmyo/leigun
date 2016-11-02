/*
 ******************************************************************************************************
 *
 * Emulation of the C161 memory controller 
 *
 *  State: Untested 
 *
 * Copyright 2005 Jochen Karrer. All rights reserved.
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

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "bus.h"
#include "sram.h"
#include "configfile.h"
#include "c161_mem.h"
#include "c16x_cpu.h"
#include "sgstring.h"

struct C161_Memco {
	BusDevice *dev[11];
	BusDevice **xdev;
	BusDevice *iram;
	uint32_t xbcon[6]; 
	uint32_t xadrs[6]; 
	uint32_t addrsel[5]; // addrsel[0] not reachable 
	uint32_t buscon[5];
};

/*
 * --------------------------------------
 * Rebuild complete memory map
 * --------------------------------------
 */
static void
C16x_rebuild_map(C161_Memco *memco) 
{
	int i,j;
	uint32_t all_rgad[11];
	uint32_t all_rgmask[11];
	uint32_t buscon[11];
	uint32_t minsize=4096;	
	int8_t *devicemap; 
	uint32_t start_addr;
	int devmapsize;
	fprintf(stderr,"C161 Rebuild map\n");	
	/* priority XADRS, Addrsel2,4 then addrsel1/3  */
	for(i=0;i<11;i++) {
                BusDevice *bdev=memco->dev[i];
                if(bdev) {
                        Mem_AreaDeleteMappings(bdev);
                }
        }

	for(i=0;i<11;i++) {
		all_rgad[i]=0xffffffff;
		all_rgmask[i]=0xfffffff0;
	}
	buscon[0]=memco->buscon[0];	
	all_rgmask[0]=0;
	all_rgad[0]=0;
	for(i=1;i<5;i++) {
		int rgsz = memco->addrsel[i] & ADDRSEL_RGSZ_MASK ;	
		uint32_t winsize = 4096 << rgsz;
		buscon[i]=memco->buscon[i];
		if(!(memco->buscon[i] & BUSCON_BUSACT)) {
			continue;	
		}
		all_rgad[i] = ((memco->addrsel[i] & ADDRSEL_RGSAD_MASK) << 5) & ~(winsize-1);
		all_rgmask[i] = ~(winsize-1);
		if(winsize<minsize) {
			minsize=winsize;
		}
		//fprintf(stderr,"i %d,buscon %04x, winsize %08x addr %08x\n",i,buscon[i],winsize,all_rgad[i]);
	}	
	for(i=0;i<6;i++) {
		int rgsz = memco->xadrs[i] & XADRS_RGSZ_MASK;
		uint32_t winsize = 256<<rgsz;
		uint32_t rgad = ((memco->xadrs[i] & XADRS_RGSAD_MASK)<<4) & ~(winsize-1);
		buscon[i+5]=memco->xbcon[i];
		if(!(memco->xbcon[i] & XBCON_BUSACT)) {
			continue;	
		}
		all_rgad[i+5]=rgad;
		all_rgmask[i+5] = ~(winsize-1);
		if(winsize<minsize) {
			minsize=winsize;
		}
	}	
	devmapsize = 2*1024*1024 / minsize;	
	devicemap = (int8_t*)alloca(devmapsize*sizeof(*devicemap));
	memset(devicemap,0,devmapsize*sizeof(*devicemap));
	for(j=0;j<devmapsize;j++) {
		uint32_t addr = minsize * j; 
		for(i=1;i<=3;i+=2) {
			if(!(buscon[i] & BUSCON_BUSACT)) {
				continue;
			}
			if((addr & all_rgmask[i]) == all_rgad[i]) {
				//fprintf(stderr,"match dev %d, addr %08x, minsize %d entry %d\n",i,addr,minsize,j);
				devicemap[j]=i;
			}
		}
		for(i=2;i<=4;i+=2) {
			if(!(buscon[i] & BUSCON_BUSACT)) {
				continue;
			}
			if((addr & all_rgmask[i]) == all_rgad[i]) {
				fprintf(stderr,"match dev %d, addr %08x, minsize %d entry %d\n",i,addr,minsize,j);
				devicemap[j]=i;
			}
		}
		for(i=5;i<11;i++) {
			if(!(buscon[i] & XBCON_BUSACT)) {
				continue;
			}
			if((addr & all_rgmask[i]) == all_rgad[i]) {
				//fprintf(stderr,"match dev %d, addr %08x, minsize %d entry %d\n",i,addr,minsize,j);
				devicemap[j]=i;
			}
		}
#if 1
		if((addr>=0xf000) && (addr<0x10000)) {
			fprintf(stderr,"setze dvicemap[%d] fuer addr %08x\n",j,addr);
			devicemap[j]=-1;
		}
#endif
	}
	start_addr=0;
	for(j=0;j<devmapsize;j++) 
	{
		uint32_t addr = j * minsize;
		if((j==(devmapsize-1)) || (devicemap[j] != devicemap[j+1])) {
			int device;
			BusDevice *bdev=NULL;
			uint32_t mapsize; 	
			uint32_t base; 

			device = devicemap[j];
			if(device<0) {
				fprintf(stderr,"Reserved for CPU index %d  last_addr %08x\n",j,addr);
			} else { 	
				bdev = memco->dev[device];
			}
			mapsize = minsize*(j+1)-start_addr; 	
			base = start_addr; 
			if(bdev) {
				if(buscon[device] & BUSCON_BUSACT) {
					fprintf(stderr,"Map at %08x, size %08x dev %d\n",base,mapsize,device);
					Mem_AreaAddMapping(bdev,base,mapsize,MEM_FLAG_READABLE | MEM_FLAG_WRITABLE);
				} else {
					fprintf(stderr,"Disabled Map at %08x, size %08x dev %d\n",base,mapsize,devicemap[j]);
				}
			} else {
				fprintf(stderr,"Map nothing at %08x, size %08x start_addr %08x\n",base,mapsize,start_addr);
			}
			start_addr=minsize*(j+1);
		} else {
			//fprintf(stderr,"no change block %d of %d, dev %d\n",j,devmapsize,devicemap[j]); // jk
		}
	}
}

/*
 * -------------------------------------------------------------
 * Addrsel
 * -------------------------------------------------------------
 */
static uint32_t
addrsel_read(void *clientData,uint32_t address,int rqlen) 
{
	C161_Memco *memco=(C161_Memco*)clientData;
	unsigned int index = ((address - SFR_ADDR(SFR_ADDRSEL1))>>1)+1; 
	if((index>4) || (index < 1)) {
		fprintf(stderr,"Emulator bug: ADDRSEL: Illegal index %d\n",index);
		exit(4324);
	}
	return memco->addrsel[index];	
}

static void
addrsel_write(void *clientData,uint32_t value,uint32_t address,int rqlen) 
{
	C161_Memco *memco=(C161_Memco*)clientData;
	unsigned int index = ((address - SFR_ADDR(SFR_ADDRSEL1))>>1)+1; 
	if((index>4) || (index < 1)) {
		fprintf(stderr,"Emulator bug: ADDRSEL: Illegal index %d\n",index);
		exit(4324);
	}
	memco->addrsel[index]=value;	
	fprintf(stderr,"ADDRSEL%d write %04x\n",index,value);
        C16x_rebuild_map(memco);
}


static uint32_t
buscon0_read(void *clientData,uint32_t address,int rqlen) 
{
	C161_Memco *memco=(C161_Memco*)clientData;
	return memco->buscon[0];	
}
static void
buscon0_write(void *clientData,uint32_t value,uint32_t address,int rqlen) 
{
	C161_Memco *memco=(C161_Memco*)clientData;
	memco->buscon[0]=value;	
	fprintf(stderr,"BUSCON0 write %04x, addr %08x\n",value,address);
        C16x_rebuild_map(memco);

}
static uint32_t
buscon_read(void *clientData,uint32_t address,int rqlen) 
{
	C161_Memco *memco=(C161_Memco*)clientData;
	unsigned int index = ((address - SFR_ADDR(SFR_BUSCON1))>>1)+1; 
	if(index>4) {
		fprintf(stderr,"Emulator bug: BUSCON: Illegal index %d\n",index);
		exit(4324);
	}
	return memco->buscon[index];	
}

static void
buscon_write(void *clientData,uint32_t value,uint32_t address,int rqlen) 
{
	C161_Memco *memco=(C161_Memco*)clientData;
	unsigned int index = ((address - SFR_ADDR(SFR_BUSCON1))>>1)+1; 
	if(index>4) {
		fprintf(stderr,"Emulator bug: BUSCON: Illegal index %d\n",index);
		exit(4324);
	}
	memco->buscon[index]=value;	
	fprintf(stderr,"BUSCON%d write %04x, addr %08x\n",index,value,address);
        C16x_rebuild_map(memco);
}
static uint32_t
xbcon_read(void *clientData,uint32_t address,int rqlen) 
{
	C161_Memco *memco=(C161_Memco*)clientData;
	unsigned int index = (address - ESFR_ADDR(ESFR_XBCON1))>>1; 
	if(index>5) {
		fprintf(stderr,"Emulator bug: XBCON: Illegal index %d\n",index);
		exit(4324);
	}
	return memco->xbcon[index];	
}
static void
xbcon_write(void *clientData,uint32_t value,uint32_t address,int rqlen) 
{
	C161_Memco *memco=(C161_Memco*)clientData;
	unsigned int index = (address - ESFR_ADDR(ESFR_XBCON1))>>1; 
	if(index>6) {
		fprintf(stderr,"Emulator bug: BUSCON: Illegal index %d\n",index);
		exit(4324);
	}
	memco->xbcon[index]=value;	
        C16x_rebuild_map(memco);
}

static uint32_t
xadrs_read(void *clientData,uint32_t address,int rqlen) 
{
	C161_Memco *memco=(C161_Memco*)clientData;
	unsigned int index = (address - ESFR_ADDR(ESFR_XADRS1))>>1; 
	if(index>5) {
		fprintf(stderr,"Emulator bug: XADRS: Illegal index %d\n",index);
		exit(4324);
	}
	return memco->xadrs[index];	
}

static void
xadrs_write(void *clientData,uint32_t value,uint32_t address,int rqlen) 
{
	C161_Memco *memco=(C161_Memco*)clientData;
	unsigned int index = (address - ESFR_ADDR(ESFR_XADRS1))>>1; 
	if(index>5) {
		fprintf(stderr,"Emulator bug: XADRS: Illegal index %d\n",index);
		exit(4324);
	}
	memco->xadrs[index]=value;	
        C16x_rebuild_map(memco);
}

C161_Memco *
C161_MemcoNew() 
{
	C161_Memco *memco;
	int i;
	fprintf(stderr,"New C161 Memory controller\n");
	memco = sg_new(C161_Memco);
	memco->xdev = &memco->dev[5];
	Config_AddString("[iram]\nsize: 3072\n");
	memco->iram=SRam_New("iram");	
	Mem_AreaAddMapping(memco->iram,0xf200,3072,MEM_FLAG_READABLE | MEM_FLAG_WRITABLE);	
	//exit(7490);
	for(i=0;i<5;i++) {
		if(i) {
			IOH_New16(SFR_ADDR(SFR_ADDRSEL1-1+i),addrsel_read,addrsel_write,memco);
			IOH_New16(SFR_ADDR(SFR_BUSCON1+i-1),buscon_read,buscon_write,memco);
		} else {
			IOH_New16(SFR_ADDR(SFR_BUSCON0),buscon0_read,buscon0_write,memco);
		}
	}
	for(i=0;i<6;i++) {
		IOH_New16(ESFR_ADDR(ESFR_XADRS1+i),xadrs_read,xadrs_write,memco);
		IOH_New16(ESFR_ADDR(ESFR_XBCON1+i),xbcon_read,xbcon_write,memco);
	}
	memco->buscon[0]=BUSCON_ALECTL | BUSCON_BUSACT;
	return memco;
}

void
C161_RegisterDevice(C161_Memco *memco,BusDevice *bdev,uint32_t cs) 
{
        if(cs>4) {
                fprintf(stderr,"Bug, only 5 Chipselects available but trying to set Nr. %d\n",cs);
                exit(4324);
        }
        if(memco->dev[cs]) {
                fprintf(stderr,"C161_RegisterDevice warning: There is already a device for CS%d\n",cs);
        }
        memco->dev[cs]=bdev;
        C16x_rebuild_map(memco);
}

