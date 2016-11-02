/*
 *************************************************************************************************
 *
 * Emulation of the NS9750 PCI Brigde 
 *
 *  State: Working with Linux-2.6.10 
 *	Address translation is missing
 *
 * Copyright 2004 Jochen Karrer. All rights reserved.
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
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "pci.h"
#include "ns9750_pci.h"
#include "bus.h"
#include "byteorder.h"
#include "signode.h"
#include "sgstring.h"


#if 0
#define dbgprintf(x...) { fprintf(stderr,x); }
#else
#define dbgprintf(x...)
#endif

/* temporary hack */
static int gcpu_endian = 0;

typedef struct PCI_Bridge {
		uint16_t device_id;
		uint16_t vendor_id;
		uint16_t command;
		uint16_t status;
		uint32_t class_code;
		uint8_t  revision_id;
		uint8_t latency_timer;
		uint8_t header_type;
		uint32_t bar0;
		uint32_t bar1;
		uint32_t bar2;
		uint32_t bar3;
		uint32_t bar4;
		uint32_t bar5;
		uint32_t sub_vendor_id;
		uint32_t sub_system_id;
		uint8_t irq_line;
		uint8_t irq_pin;
		uint8_t min_gnt;
		uint8_t max_lat;

		// start of a linked list
		PCI_Function pcifunc;	
		// registers of bridge
		uint32_t config_addr;

		uint32_t parbcfg;
		uint32_t parbint;
		uint32_t parbinten;
		uint32_t pmisc;
		uint32_t pcfg0;
		uint32_t pcfg1;
		uint32_t pcfg2;
		uint32_t pcfg3;
		uint32_t pahbcfg;
		uint32_t pahberr;
		uint32_t pcierr;
		uint32_t pintr;
		uint32_t pinten;
		uint32_t paltmem0;
		uint32_t paltmem1;
		uint32_t paltio;
		uint32_t pmalt0;
		uint32_t pmalt1;
		uint32_t paltctl;
		uint32_t cmisc;
		uint32_t csktev;
		uint32_t cskmsk;
		uint32_t csktpst;
		uint32_t csktfev;
		uint32_t csktctl;
		SigNode *endianNode;
		SigTrace *endianTrace;
		int endian;
} PCI_Bridge;

/*
 * -------------------------------------------------------
 * change_endian
 *      Invoked when the Endian Signal Line changes
 * -------------------------------------------------------
 */
static void 
change_endian(SigNode *node,int value,void *clientData)
{
        PCI_Bridge *br = clientData;
	PCI_Function *cursor,*prev;
        if(value == SIG_HIGH) {
                fprintf(stderr,"PCI Byteorder conversion to little endian enabled\n");
                br->endian = en_BIG_ENDIAN;
		gcpu_endian = en_BIG_ENDIAN;
        } else if(value==SIG_LOW) {
                br->endian = en_LITTLE_ENDIAN;
		gcpu_endian = en_LITTLE_ENDIAN;
        } else {
                fprintf(stderr,"NS9750 PCI bridge: Endian is neither Little nor Big\n");
                exit(3424);
        }
	for(prev=cursor=&br->pcifunc;cursor;prev=cursor,cursor=cursor->next) {
		if(cursor->UnMap && cursor->Map) {
			cursor->UnMap(cursor->owner);
			cursor->Map(cursor->owner,cursor->hw_flags);
		}
	}
}


PCI_Function *
pci_find_function(PCI_Bridge *br,int bus,int device,int function) {
	PCI_Function *cursor;
	for(cursor=&br->pcifunc;cursor;cursor=cursor->next) {
		if((cursor->bus==bus)&&(cursor->dev==device) 
				&&(cursor->function==function)) {
			return cursor;
		}
	}
	return NULL;
}
/*
 * -----------------------------------------------------------
 * Register a new Function Module
 * -----------------------------------------------------------
 */
int
PCI_FunctionRegister(PCI_Function *bridgefunc,PCI_Function *newfunc) {
	PCI_Bridge *br=bridgefunc->owner;
	PCI_Function *cursor,*prev;
	if(pci_find_function(br,newfunc->bus,newfunc->dev,newfunc->function)) {
		fprintf(stderr,"Bug, PCI device at bus %d device %d function %d is already registered\n",newfunc->bus,newfunc->dev,newfunc->function);
		return -1;
	}
	newfunc->bus=bridgefunc->bus;
	for(prev=cursor=&br->pcifunc;cursor;prev=cursor,cursor=cursor->next) {
			
	}
	newfunc->next=NULL;
	prev->next=newfunc;
	if(!newfunc->configWrite || !newfunc->configRead || !newfunc->owner) {
		fprintf(stderr,"PCI Function structure filled incompletely\n");
	}
	return 0;
}

/*
 * -------------------------------------------------------------------------
 * The CONFIG_ADDRESS register stores the register number the device_nr 
 * the bus number and the function number required for the configuration
 * cycle which is generated by accessing CONFIG_DATA 
 * -------------------------------------------------------------------------
 */
static void 
pci_config_addr_write(void *cd,uint32_t value,uint32_t address,int rqlen) {
	PCI_Bridge *br=cd;
	if(rqlen!=4) {
		return;
	}
	br->config_addr=value;
	return;
}

static uint32_t 
pci_config_addr_read(void *cd,uint32_t address,int rqlen) {
	PCI_Bridge *br=cd;
	if(rqlen!=4) {
		return -1;
	}
        return br->config_addr;
}

/*
 * -------------------------------------------------------------------------
 * The CONFIG_DATA register creates a configuration write/read cycle
 * using the bus/device/function/register selected by a preceeding
 * write to CONFIG_ADDR 
 * -------------------------------------------------------------------------
 */
static void 
pci_config_data_write(void *cd,uint32_t value,uint32_t address,int rqlen) {
	PCI_Bridge *br=cd;
	int type,bus,device,function;
	uint32_t config_addr=br->config_addr;
	uint8_t reg;
	PCI_Function *pcifunc;
	bus=PCI_CONFIG_BUS(config_addr);
	device=PCI_CONFIG_DEVICE(config_addr);
	function=PCI_CONFIG_FUNCTION(config_addr);
	type=PCI_CONFIG_TYPE(config_addr);	
	reg=PCI_CONFIG_REGISTER(config_addr);
	if(br->endian != TARGET_BYTEORDER) {
		if(rqlen==1) {
			address^=3;
		} else if(rqlen==2) {
			address^=2;
		}
	}

	if(((bus==0) && (type!=0)) || ((bus!=0) && (type==0))) {
		dbgprintf("wrong access type %d for bus %d, config_addr %08x\n",type,bus,config_addr);
		return;
	}
	pcifunc=pci_find_function(br,bus,device,function);
	if(!pcifunc) {
//		dbgprintf("device not found,b %d,d %d,f %d\n",bus,device,function);
		return;
	}
	if(pcifunc->configWrite) {
		uint32_t mask;
		mask=~0U>>(8*(3-(rqlen-1)));
		mask<<=((address&3)*8);
		value=value<<(8*(address&3));
		pcifunc->configWrite(value,mask,reg,pcifunc);
		return;
	}
	return;
}

static uint32_t
pci_config_data_read(void *cd,uint32_t address,int rqlen) {
	uint32_t value;
	PCI_Bridge *br = cd;
	int type,bus,device,function;
	uint8_t reg;
	PCI_Function *pcifunc;
	uint32_t config_addr=br->config_addr;
	bus=PCI_CONFIG_BUS(config_addr);
	device=PCI_CONFIG_DEVICE(config_addr);
	function=PCI_CONFIG_FUNCTION(config_addr);
	type=PCI_CONFIG_TYPE(config_addr);	
	reg=PCI_CONFIG_REGISTER(config_addr);
	if(br->endian != TARGET_BYTEORDER) {
		if(rqlen==1) {
			address^=3;
		} else if(rqlen==2) {
			address^=2;
		}
	}
	if(((bus==0) && (type!=0)) || ((bus!=0) && (type==0))) {
		dbgprintf("wrong access type %d for bus %d, config_addr %08x\n",type,bus,config_addr);
		return 0;
	}
	pcifunc=pci_find_function(br,bus,device,function);
	if(!pcifunc) {
//		dbgprintf("device not found,b %d,d %d,f %d\n",bus,device,function);
		return 0;
	}
	if(pcifunc->configRead) {
		int result;
		uint32_t mask;
		mask=~0U>>(8*(3-(rqlen-1)));
		mask<<=((address&3)*8);
		result = pcifunc->configRead(&value,reg,pcifunc);
		value=(value&mask)>>8*(address&3);
//		fprintf(stderr,"Returne %08x, mask %08x, rqlen %d, addr %08x\n",value,mask,rqlen,address);
	} else {
		value=0;
	}
        return value;
}
/*
 * --------------------------------------------------------------------------
 * Implementation of the configuration space of the NS9750 PCI-Host Brigde
 * --------------------------------------------------------------------------
 */
int 
BR_ConfigRead(uint32_t *value,uint8_t reg,PCI_Function *pcifunc) 
{
	PCI_Bridge *br=pcifunc->owner;
	reg=reg>>2;
	switch(reg) {
		case 0:
			*value = br->vendor_id  | (br->device_id<<16);
			break;
		case 1:
			// PCI Command, PCI Status
			*value = br->command  | (br->status <<16);
			break;

		case 2:
			// PCI Revision ID Class Code
			*value = br->revision_id | (br->class_code<<8);
			break;

		case 3:
			*value = (br->latency_timer<<8) | (br->header_type<<16);
			break;
		case 4:
			if(br->pmisc & PMISC_ENBAR0) {
				*value = br->bar0;
			} else {
				*value=0;
			}
			break;
		case 5:
			if(br->pmisc & PMISC_ENBAR1) {
				*value = br->bar1;
			} else {
				*value=0;
			}
			break;
		case 6:
			if(br->pmisc & PMISC_ENBAR2) {
				*value = br->bar2;
			} else {
				*value=0;
			}
			break;
		case 7:
			if(br->pmisc & PMISC_ENBAR3) {
				*value = br->bar3;
			} else {
				*value=0;
			}
			break;
		case 8:
			if(br->pmisc & PMISC_ENBAR4) {
				*value = br->bar4;
			} else {
				*value=0;
			}
			break;
		case 9:
			if(br->pmisc & PMISC_ENBAR5) {
				*value = br->bar5;
			} else {
				*value=0;
			}
			break;
		case 0xa:
			// Cardbus CIS Pointer
			*value=0;
			break;

		case 0xb:
			*value = br->sub_vendor_id | (br->sub_system_id<<16);
			break;	
		case 0xc:	// Expansion Rom
		case 0xd:	// Reserved
		case 0xe:	// Reserved
			*value=0;
			break;
		case 0xf:
			*value=br->irq_line | (br->irq_pin<<8) | (br->min_gnt<<16) | (br->max_lat<<24);
			break;
		default:
			*value=0;
			break;
				
	}
//	dbgprintf("BR-Config Read reg %d val %08x\n",reg*4, *value);
	return 0;
}

#define MODIFY_MASKED(v,mod,mask) { (v) = ((v) & ~(mask)) | ((mod)&(mask)); }

int 
BR_ConfigWrite(uint32_t value,uint32_t mask,uint8_t reg,PCI_Function *pcifunc) 
{
	PCI_Bridge *br=pcifunc->owner;
	reg=reg>>2;
	switch(reg) {
		case 0x3:
			if(mask&0xff00) {	
				br->latency_timer=(value>>8)&0xff;
			}
			if(mask&0xff0000) {
				// header type not writable
			}
			break;
		case 0x4:
			/* Bar Write is ignored when not enabled */
			if(br->pmisc & PMISC_ENBAR0) {
				value=value & 0xf0000000;
				MODIFY_MASKED(br->bar0,value,mask);
			} else {
				dbgprintf("NS9750_Pci: Write to disabled BAR0\n");
			}
			break;
		case 0x5:
			if(br->pmisc & PMISC_ENBAR1) {
				value=value & 0xfc000000;
				MODIFY_MASKED(br->bar1,value,mask);
			} else {
				dbgprintf("NS9750_Pci: Write to disabled BAR1\n");
			}
			break;
		case 0x6:
			if(br->pmisc & PMISC_ENBAR2) {
				value=value & 0xff000000;
				MODIFY_MASKED(br->bar2,value,mask);
			} else {
				dbgprintf("NS9750_Pci: Write to disabled BAR2\n");
			}
			break;
		case 0x7:
			if(br->pmisc & PMISC_ENBAR3) {
				value=value & 0xffc00000;
				MODIFY_MASKED(br->bar3,value,mask);
			} else {
				dbgprintf("NS9750_Pci: Write to disabled BAR3\n");
			}
			break;
		case 0x8:
			if(br->pmisc & PMISC_ENBAR4) {
				value=value & 0xfff00000;
				MODIFY_MASKED(br->bar4,value,mask);
			} else {
				dbgprintf("NS9750_Pci: Write to disabled BAR4\n");
			}
			break;
		case 0x9:
			if(br->pmisc & PMISC_ENBAR5) {
				value=value & 0xfffc0000;
				MODIFY_MASKED(br->bar5,value,mask);
			} else {
				dbgprintf("NS9750_Pci: Write to disabled BAR5\n");
			}
			break;
		case 0xf:
			if(mask&0xff) {	
				br->irq_line=value&0xff;
			}
			break;
		default:
			break;
	}
	return 0;
}

static void
NS9750_PCIPostIRQ(BusDevice *dev,int irq) {
	Sysco_PostIrq(irq+IRQ_PCI_EXTERNAL_0);
}

static void
NS9750_PCIUnPostIRQ(BusDevice *dev,int irq) {
	Sysco_UnPostIrq(irq+IRQ_PCI_EXTERNAL_0);
}

PCI_Function *
BR_New(const char *name,int dev_nr) {
	PCI_Bridge *br;
	br = sg_new(PCI_Bridge);	
	br->device_id = PCI_DEVICE_ID_DIGI_NS9750;
	br->vendor_id = PCI_VENDOR_ID_DIGI;
	br->class_code=0x060000;
	br->revision_id=0;
	br->status=0x0880;
	br->command=6;
	br->header_type=PCI_HEADER_TYPE_NORMAL; // This is no PCI-PCI bridge
	br->latency_timer=64; 
	br->sub_vendor_id=0;
	br->sub_system_id=0;
	br->irq_line=0;
	br->irq_pin=1; // INTA
	br->min_gnt=6;
	br->max_lat=0;
	br->pcifunc.configRead=BR_ConfigRead;
	br->pcifunc.configWrite=BR_ConfigWrite;
	br->pcifunc.owner=br;
	br->pcifunc.bus=0;
	br->pcifunc.dev=dev_nr;
	br->pcifunc.function=0;
	/* default bars from NS9750 */
	br->bar0=0xf0000000;
	br->bar1=0xfc000000;
	br->bar2=0xff000000;
	br->bar3=0xffc00000;
	br->bar4=0xfff00000;
	br->bar5=0xfffc0000;
	 
	br->parbcfg=0;
	br->parbint=0;
	br->parbinten=0;
	br->pmisc=0;
	br->pcfg0=0x00c4114f;
	br->pcfg1=0x06000000;
	br->pcfg2=0;
	br->pcfg3=1;
	br->pahbcfg=1;
	br->pahberr=0;
	br->pcierr=0;
	br->pintr=0;
	br->pinten=0;
	br->paltmem0=0;
	br->paltmem1=0;
	br->paltio=0;
	br->pmalt0=0;
	br->pmalt1=0;
	br->paltctl=0;
	br->cmisc=0;
	br->csktev=0;
	br->cskmsk=0;
	br->csktpst=0;
	br->csktfev=0;
	br->csktctl=0;
        br->endianNode = SigNode_New("%s.cpu_endian",name);
        if(!br->endianNode) {
                fprintf(stderr,"Can not create Host. EndianNode\n");
                exit(3429);
        }
	br->endianTrace = SigNode_Trace(br->endianNode,change_endian,br);
	return &br->pcifunc;
}

/*
 * -------------------------------------
 * PCI-Mem is 256 MB from 0x80000000 
 * PCI-IO is 1MB  from 0xA0000000
 * -------------------------------------
 */
int
PCI_RegisterIOH(uint32_t pci_addr,PCI_IOReadProc *readproc,PCI_IOWriteProc *writeproc,void *clientData) {
	uint32_t cpu_addr=pci_addr;
	if((cpu_addr>0xa00fffff) || (cpu_addr<0xa0000000)) {
		return -1;
	} 
	if(gcpu_endian==HOST_BYTEORDER) {
		IOH_New32f(cpu_addr,readproc,writeproc,clientData,IOH_FLG_LITTLE_ENDIAN);
	} else {
		IOH_New32f(cpu_addr,readproc,writeproc,clientData,IOH_FLG_BIG_ENDIAN);
	}
	return 0;
}
int
PCI_UnRegisterIOH(uint32_t pci_addr) {
	uint32_t cpu_addr=pci_addr;
	if((cpu_addr>0xa00fffff) || (cpu_addr<0xa0000000)) {
		return -1;
	} 
	IOH_Delete32(cpu_addr);
	return 0;
}

int
PCI_RegisterMMIOH(uint32_t pci_addr,PCI_MMIOReadProc *readproc,PCI_MMIOWriteProc *writeproc,void *clientData) {
	// AHB to PCI Address translation not yet implemented  
	uint32_t cpu_addr=pci_addr;
	if((cpu_addr>0x8fffffff) || (cpu_addr<0x80000000)) {
		return -1;
	} 
	if(gcpu_endian == en_BIG_ENDIAN)  {
		IOH_New32f(cpu_addr,readproc,writeproc,clientData,IOH_FLG_BIG_ENDIAN); 
	} else {
		IOH_New32f(cpu_addr,readproc,writeproc,clientData,IOH_FLG_LITTLE_ENDIAN); 
	}
	return 0;
}

int
PCI_UnRegisterMMIOH(uint32_t pci_addr) {
	uint32_t cpu_addr=pci_addr;
	if((cpu_addr>0x8fffffff) || (cpu_addr<0x80000000)) {
		return -1;
	} 
	IOH_Delete32(cpu_addr);
	return 0;
}

static void 
parbcfg_write(void *clientData,uint32_t value,uint32_t address,int rqlen) {
	
	fprintf(stderr,"parbcfg not implemented\n");
        return;
}

static uint32_t 
parbcfg_read(void *clientData,uint32_t address,int rqlen) {
	fprintf(stderr,"pargcfg not implemented\n");
        return 0;
}

static void 
parbint_write(void *clientData,uint32_t value,uint32_t address,int rqlen) {
	PCI_Bridge *br=clientData;
	br->parbint ^= (value & 0x3f);
        return;
}

static uint32_t 
parbint_read(void *clientData,uint32_t address,int rqlen) {
	PCI_Bridge *br=clientData;
	return br->parbint;	
}

static void 
parbinten_write(void *clientData,uint32_t value,uint32_t address,int rqlen) {
	PCI_Bridge *br=clientData;
	br->parbinten = (value & 0x3f);
        return;
}

static uint32_t 
parbinten_read(void *clientData,uint32_t address,int rqlen) {
	PCI_Bridge *br=clientData;
	return br->parbinten;	
}

static void 
pmisc_write(void *clientData,uint32_t value,uint32_t address,int rqlen) {
	PCI_Bridge *br=clientData;
	br->pmisc = (value & 0x3f1);
        return;
}

static uint32_t 
pmisc_read(void *clientData,uint32_t address,int rqlen) {
	PCI_Bridge *br=clientData;
	return br->pmisc;
}

static  void
pcfg0_write(void *clientData,uint32_t value,uint32_t address,int rqlen) {
	PCI_Bridge *br=clientData;
	br->device_id=value>>16;
	br->vendor_id=value&0xffff;
        return;
}

static uint32_t
pcfg0_read(void *clientData,uint32_t address,int rqlen) {
	PCI_Bridge *br=clientData;
	return   (br->device_id<<16) | br->vendor_id;
}
static void 
pcfg1_write(void *clientData,uint32_t value,uint32_t address,int rqlen) {
	PCI_Bridge *br=clientData;
	br->class_code=value>>8;	
	br->revision_id=value&0xff;
        return;
}

static uint32_t
pcfg1_read(void *clientData,uint32_t address,int rqlen) {
	PCI_Bridge *br=clientData;
	return (br->class_code<<8) | (br->revision_id &0xff);
}
static void 
pcfg2_write(void *clientData,uint32_t value,uint32_t address,int rqlen) {
	PCI_Bridge *br=clientData;
	br->sub_vendor_id=value&0xffff;
	br->sub_system_id=value>>16;
        return;
}

static uint32_t 
pcfg2_read(void *clientData,uint32_t address,int rqlen) {
	PCI_Bridge *br=clientData;
	return  br->sub_vendor_id | (br->sub_system_id<<16);
}
static void 
pcfg3_write(void *clientData,uint32_t value,uint32_t address,int rqlen) {
	PCI_Bridge *br=clientData;
	br->max_lat=(value>>16)&0xff;
	br->min_gnt=(value>>8)&0xff;
	br->irq_pin=(value)&0xff;
        return;
}

static uint32_t 
pcfg3_read(void *clientData,uint32_t address,int rqlen) {
	PCI_Bridge *br=clientData;
	return br->irq_pin | (br->min_gnt<<8) |(br->max_lat<<16);
}

static void 
pahbcfg_write(void *clientData,uint32_t value,uint32_t address,int rqlen) {
	fprintf(stderr,"pahbcfg not implemented\n");
        return;
}

static uint32_t 
pahbcfg_read(void *clientData,uint32_t address,int rqlen) {
	fprintf(stderr,"pahbcfg not implemented\n");
        return 0;
}
static void 
pahberr_write(void *clientData,uint32_t value,uint32_t address,int rqlen) {
	fprintf(stderr,"pahberr not implemented\n");
        return;
}

static uint32_t 
pahberr_read(void *clientData,uint32_t address,int rqlen) {
	fprintf(stderr,"pahberr not implemented\n");
        return 0;
}

static void 
pcierr_write(void *clientData,uint32_t value,uint32_t address,int rqlen) {
	fprintf(stderr,"pcierr not implemented\n");
        return;
}

static uint32_t 
pcierr_read(void *clientData,uint32_t address,int rqlen) {
	fprintf(stderr,"pcierr not implemented\n");
        return 0;
}

static void 
pintr_write(void *clientData,uint32_t value,uint32_t address,int rqlen) {
	fprintf(stderr,"pintr not implemented\n");
        return;
}

static uint32_t 
pintr_read(void *clientData,uint32_t address,int rqlen) {
	fprintf(stderr,"pintr not implemented\n");
        return 0;
}

static void 
pinten_write(void *clientData,uint32_t value,uint32_t address,int rqlen) {
	fprintf(stderr,"pinten not implemented\n");
        return;
}

static uint32_t 
pinten_read(void *clientData,uint32_t address,int rqlen) {
	fprintf(stderr,"pinten not implemented\n");
        return 0;
}
static void 
paltmem0_write(void *clientData,uint32_t value,uint32_t address,int rqlen) {
	fprintf(stderr,"paltmem0 not implemented\n");
        return;
}

static uint32_t 
paltmem0_read(void *clientData,uint32_t address,int rqlen) {
	fprintf(stderr,"paltmem0 not implemented\n");
        return 0;
}

static void 
paltmem1_write(void *clientData,uint32_t value,uint32_t address,int rqlen) {
	fprintf(stderr,"paltmem1 not implemented\n");
        return;
}

static uint32_t 
paltmem1_read(void *clientData,uint32_t address,int rqlen) {
	fprintf(stderr,"paltmem1 not implemented\n");
        return 0;
}
static void 
paltio_write(void *clientData,uint32_t value,uint32_t address,int rqlen) {
	fprintf(stderr,"paltio not implemented\n");
        return;
}

static uint32_t 
paltio_read(void *clientData,uint32_t address,int rqlen) {
	fprintf(stderr,"paltio not implemented\n");
        return 0;
}

static void 
pmalt0_write(void *clientData,uint32_t value,uint32_t address,int rqlen) {
	fprintf(stderr,"pmalt0 not implemented\n");
        return;
}

static uint32_t 
pmalt0_read(void *clientData,uint32_t address,int rqlen) {
	fprintf(stderr,"pmalt0 not implemented\n");
        return 0;
}

static void 
pmalt1_write(void *clientData,uint32_t value,uint32_t address,int rqlen) {
	fprintf(stderr,"pmalt1 not implemented\n");
        return;
}

static uint32_t 
pmalt1_read(void *clientData,uint32_t address,int rqlen) {
	fprintf(stderr,"pmalt1 not implemented\n");
        return 0;
}

static void 
paltctl_write(void *clientData,uint32_t value,uint32_t address,int rqlen) {
	fprintf(stderr,"paltctl not implemented\n");
        return;
}

static uint32_t 
paltctl_read(void *clientData,uint32_t address,int rqlen) {
	fprintf(stderr,"paltctl not implemented\n");
        return 0;
}

static void 
cmisc_write(void *clientData,uint32_t value,uint32_t address,int rqlen) {
	fprintf(stderr,"cmisc not implemented\n");
        return;
}

static uint32_t 
cmisc_read(void *clientData,uint32_t address,int rqlen) {
	fprintf(stderr,"cmisc not implemented\n");
        return 0;
}

static void 
csktev_write(void *clientData,uint32_t value,uint32_t address,int rqlen) {
	fprintf(stderr,"csktev not implemented\n");
        return;
}

static uint32_t 
csktev_read(void *clientData,uint32_t address,int rqlen) {
	fprintf(stderr,"csktev not implemented\n");
        return 0;
}

static void 
cskmsk_write(void *clientData,uint32_t value,uint32_t address,int rqlen) {
	fprintf(stderr,"cskmsk not implemented\n");
        return;
}

static uint32_t 
cskmsk_read(void *clientData,uint32_t address,int rqlen) {
	fprintf(stderr,"cskmsk not implemented\n");
        return 0;
}

static void 
csktpst_write(void *clientData,uint32_t value,uint32_t address,int rqlen) {
	fprintf(stderr,"csktpst not implemented\n");
        return;
}

static uint32_t 
csktpst_read(void *clientData,uint32_t address,int rqlen) {
	fprintf(stderr,"csktpst not implemented\n");
        return 0;
}

static void 
csktfev_write(void *clientData,uint32_t value,uint32_t address,int rqlen) {
	fprintf(stderr,"csktfev not implemented\n");
        return;
}

static uint32_t 
csktfev_read(void *clientData,uint32_t address,int rqlen) {
	fprintf(stderr,"csktfev not implemented\n");
        return 0;
}
static void 
csktctl_write(void *clientData,uint32_t value,uint32_t address,int rqlen) {
	fprintf(stderr,"csktctl not implemented\n");
        return;
}

static uint32_t 
csktctl_read(void *clientData,uint32_t address,int rqlen) {
	fprintf(stderr,"csktctl not implemented\n");
        return 0;
}

static void
NS9750Pci_Map(void *owner,uint32_t base,uint32_t mapsize,uint32_t flags) {
	PCI_Function *brfunc=owner;
	PCI_Bridge *bridge=brfunc->owner;
	uint32_t cfg_ioflags = IOH_FLG_PA_CBSE | IOH_FLG_HOST_ENDIAN;
	/* Ignore Base */
	/* Configspace */
	IOH_New32f(PCI_CONFIG_ADDR,pci_config_addr_read,pci_config_addr_write,bridge,cfg_ioflags);
        IOH_New32f(PCI_CONFIG_DATA,pci_config_data_read,pci_config_data_write,bridge,cfg_ioflags);
	/* Rest of controller */	
	IOH_New32(base+PARB_PARBCFG,parbcfg_read,parbcfg_write,bridge); 
	IOH_New32(base+PARB_PARBINT,parbint_read,parbint_write,bridge);
	IOH_New32(base+PARB_PARBINTEN,parbinten_read,parbinten_write,bridge);
	IOH_New32(base+PARB_PMISC,pmisc_read,pmisc_write,bridge); 
	IOH_New32(base+PARB_PCFG_0,pcfg0_read,pcfg0_write,bridge); 
	IOH_New32(base+PARB_PCFG_1,pcfg1_read,pcfg1_write,bridge);
	IOH_New32(base+PARB_PCFG_2,pcfg2_read,pcfg2_write,bridge);
	IOH_New32(base+PARB_PCFG_3,pcfg3_read,pcfg3_write,bridge);
	IOH_New32(base+PARB_PAHBCFG,pahbcfg_read,pahbcfg_write,bridge);
	IOH_New32(base+PARB_PAHBERR,pahberr_read,pahberr_write,bridge);
	IOH_New32(base+PARB_PCIERR,pcierr_read,pcierr_write,bridge); 
	IOH_New32(base+PARB_PINTR,pintr_read,pintr_write,bridge); 
	IOH_New32(base+PARB_PINTEN,pinten_read,pinten_write,bridge);
	IOH_New32(base+PARB_PALTMEM0,paltmem0_read,paltmem0_write,bridge);
	IOH_New32(base+PARB_PALTMEM1,paltmem1_read,paltmem1_write,bridge);
	IOH_New32(base+PARB_PALTIO,paltio_read,paltio_write,bridge); 
	IOH_New32(base+PARB_PMALT0,pmalt0_read,pmalt0_write,bridge);
	IOH_New32(base+PARB_PMALT1,pmalt1_read,pmalt1_write,bridge);
	IOH_New32(base+PARB_PALTCTL,paltctl_read,paltctl_write,bridge);
	IOH_New32(base+PARB_CMISC,cmisc_read,cmisc_write,bridge);
	IOH_New32(base+PARB_CSKTEV,csktev_read,csktev_write,bridge);
	IOH_New32(base+PARB_CSKMSK,cskmsk_read,cskmsk_write,bridge);
	IOH_New32(base+PARB_CSKTPST,csktpst_read,csktpst_write,bridge);
	IOH_New32(base+PARB_CSKTFEV,csktfev_read,csktfev_write,bridge);
	IOH_New32(base+PARB_CSKTCTL,csktctl_read,csktctl_write,bridge);
}

static void
NS9750Pci_UnMap(void *owner,uint32_t base,uint32_t mapsize) {
	int i;
	IOH_Delete32(PCI_CONFIG_ADDR);
        IOH_Delete32(PCI_CONFIG_DATA);
	for(i=0;i<=0x4c;i+=4) {
		IOH_Delete32(base+i);	
	}
	for(i=0x1000;i<=0x1010;i+=4) {
		IOH_Delete32(base+i);	
	}
}

/*
 * The NS9750 pci bridge inverts accesses from
 * PCI devices
 */
static void 
PCIBr_Write32(uint32_t value,uint32_t addr) {
	if(gcpu_endian == TARGET_BYTEORDER) {
		Bus_Write32(value,addr);
	} else {
		Bus_Write32(swap32(value),addr);
	}	
}

static void 
PCIBr_Write(uint32_t addr,uint8_t *buf,uint32_t count) 
{
	if(gcpu_endian == TARGET_BYTEORDER) {
		return Bus_Write(addr,buf,count);
	} else {
		return Bus_WriteSwap32(addr,buf,count);
	}
}

static uint32_t 
PCIBr_Read32(uint32_t addr) {
	if(gcpu_endian == TARGET_BYTEORDER) {
		return Bus_Read32(addr);
	} else {
		return swap32(Bus_Read32(addr));
	}	
}

static void 
PCIBr_Read(uint8_t *buf,uint32_t addr,uint32_t count) 
{
	if(gcpu_endian == TARGET_BYTEORDER) {
		return Bus_Read(buf,addr,count);
	} else {
		return Bus_ReadSwap32(buf,addr,count);
	}
}

// Bus *
PCI_Function *
NS9750_PciInit(const char *devname,int dev_nr) {
//	Bus *bus = bla ;
	PCI_Function *bridgefunc;
	bridgefunc=BR_New(devname,dev_nr);
	bridgefunc->bdev.first_mapping=NULL;
        bridgefunc->bdev.Map=NS9750Pci_Map;
        bridgefunc->bdev.UnMap=NS9750Pci_UnMap;
        bridgefunc->bdev.owner=bridgefunc;
        bridgefunc->bdev.hw_flags=MEM_FLAG_WRITABLE|MEM_FLAG_READABLE;
        bridgefunc->bdev.postIRQ = NS9750_PCIPostIRQ;
        bridgefunc->bdev.unPostIRQ = NS9750_PCIUnPostIRQ;
        bridgefunc->bdev.read32 = PCIBr_Read32;
        bridgefunc->bdev.readblock= PCIBr_Read;
        bridgefunc->bdev.write32 = PCIBr_Write32;
        bridgefunc->bdev.writeblock = PCIBr_Write;
	Mem_AreaAddMapping(&bridgefunc->bdev,NS9xxx_PARBBASE,0x2000,MEM_FLAG_WRITABLE|MEM_FLAG_READABLE);
	fprintf(stderr,"NS9750 PCI Bridge with device nr %d\n",dev_nr);    
	return bridgefunc;
}
