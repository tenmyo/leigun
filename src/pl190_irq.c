/*
 * --------------------------------------------------------------------------
 *
 * Emulation of ARM Primecell PL190 Vectored interrupt controller 
 *
 * Status: Never used, totally untested
 * 	Chaining of multiple Interrupt controllers not implemented
 *
 *	The interrupt source pins are inverted (active low) and
 *	renamed to nVICINTSOURCE%d because this fits better into
 *	the concept of softgun.
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

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "bus.h"
#include "signode.h"
#include "pl190_irq.h"
#include "sgstring.h"

#define VICIRQSTATUS	(0)
#define VICFIQSTATUS	(4)
#define VICRAWINTR 	(8)	
#define VICINTSELECT	(0xc)
#define VICINTENABLE	(0x10)
#define VICINTENCLEAR	(0x14)
#define VICSOFTINT	(0x18)
#define VICSOFTINTCLEAR	(0x1c)
#define VICPROTECTION	(0x20)
#define VICVECTADDR	(0x30)
#define VICDEFVECTADDR  (0x34)
#define VICVECTADDRN(n) (0x100 + ((n)<<2))
#define VICVECCNTL(n)	(0x200 + ((n)<<2))
#define		VECCNTL_ENA	(1<<5)
#define VICPERIPHID(n)  (0xfe0 + ((n)<<2))
#define VICPCELLID(n)	(0xff0 + ((n)<<2))

/* Test only regsiters */
#define VICITCR		(0x300)
#define VICITIP1	(0x304)
#define VICITIP2	(0x308)
#define VICITOP1	(0x30c)
#define VICITOP2	(0x310)

#define PRIV_LEVEL_INVALID (255)

typedef struct IrqTraceInfo IrqTraceInfo;
typedef struct PL190 {
	BusDevice bdev;
	/* Interrupt priority logic */
	int required_pl; /* only Interrupt with pl <= required_pl will be triggered */
	int vectaddr_pl;  /* The privilege level of the interrupt currently in vectaddr */
	int pl_stack[16];
	unsigned int pl_stackp;

	uint32_t irqstatus;
	uint32_t fiqstatus;

	uint32_t hardintr; /* emulator internal only */

	uint32_t rawintr;	
	uint32_t intselect;
	uint32_t intenable;
	uint32_t intenclear;
	uint32_t softint;
	uint32_t protection;
	uint32_t vectaddr;
	uint32_t defvectaddr;
	uint32_t vectaddrn[16];
	uint32_t veccntl[16];
	SigNode *intSourceNode[32];
	SigTrace *intSourceTrace[32];
	struct IrqTraceInfo *traceInfo[32];	

	/* output nodes */
	SigNode *irqNode;
	SigNode *fiqNode;
} PL190;

struct IrqTraceInfo {
	int nr;	
	PL190 *pl190;
};


static void 
update_interrupts(PL190 *pl) 
{
	uint32_t  intstatus;
	int first_vecint = -1;
	int i;
	pl->rawintr = pl->softint | pl->hardintr;
	intstatus = pl->rawintr & pl->intenable;
	pl->irqstatus = intstatus  &  ~pl->intselect;
	pl->fiqstatus = intstatus & pl->intselect;
	if(pl->fiqstatus) {
		SigNode_Set(pl->fiqNode,SIG_LOW);
	} else {
		SigNode_Set(pl->fiqNode,SIG_HIGH);
 	}
	if(pl->irqstatus) {
		/* Vectored interrupt logic use irqstatus as input */
		for(i=0;i <= pl->required_pl;i++) {
			int srcirq;
			uint32_t veccntl = pl->veccntl[i];
			if(!(veccntl & (VECCNTL_ENA))) {
				continue;
			}
			srcirq = veccntl & 0x1f;		
			if(pl->irqstatus & (1<<srcirq)) {
				if(first_vecint < 0) {
					first_vecint = i;
				}
				break;
			}
		}
		if(first_vecint >= 0) {
			pl->vectaddr = pl->vectaddrn[first_vecint];
			pl->vectaddr_pl = first_vecint;
		} else {
			pl->vectaddr = pl->defvectaddr;
			pl->vectaddr_pl = PRIV_LEVEL_INVALID;
		}	
		SigNode_Set(pl->irqNode,SIG_LOW);
	} else {
		SigNode_Set(pl->irqNode,SIG_HIGH);
	}
}

/*
 * -----------------------------------------------------------
 * int_source_change
 * 	irq line trace, called whenever a change occurs
 * -----------------------------------------------------------
 */
static void 
int_source_change(SigNode *node,int value,void *clientData)
{
	IrqTraceInfo *ti = (IrqTraceInfo *) clientData;
	PL190 *pl = ti->pl190;
	int irq = ti->nr;
	if((value == SIG_LOW) || (value == SIG_PULLDOWN)) {
		pl->hardintr |= (1<<irq);
	} else {
		pl->hardintr &= ~(1<<irq);
	}
	update_interrupts(pl);
}

uint32_t
irqstatus_read(void *clientData,uint32_t address,int rqlen) 
{
	PL190 *pl = (PL190 *) clientData;
	return pl->irqstatus;
}

static void
irqstatus_write(void *clientData,uint32_t value,uint32_t address,int rqlen) 
{
        fprintf(stderr,"PL190 warning: IRQstatus is not writable\n");
        return;
}

uint32_t
fiqstatus_read(void *clientData,uint32_t address,int rqlen) 
{
	PL190 *pl = (PL190 *) clientData;
	return pl->fiqstatus;
}

static void
fiqstatus_write(void *clientData,uint32_t value,uint32_t address,int rqlen) 
{
        fprintf(stderr,"FIQstatus is not writable\n");
        return;
}

uint32_t
rawintr_read(void *clientData,uint32_t address,int rqlen) 
{
	PL190 *pl = (PL190 *) clientData;
	return pl->rawintr;
}

static void
rawintr_write(void *clientData,uint32_t value,uint32_t address,int rqlen) 
{
        fprintf(stderr,"VIC register %08x is not implemented\n",address);
        return;
}
/*
 * ---------------------------------------------------------
 * Intselect register
 * 	Select if Intsource is IRQ or FIQ
 * ---------------------------------------------------------
 */
uint32_t
intselect_read(void *clientData,uint32_t address,int rqlen) 
{
	PL190 *pl = (PL190 *)clientData;
	return pl->intselect;
}

static void
intselect_write(void *clientData,uint32_t value,uint32_t address,int rqlen) 
{
	PL190 *pl = (PL190 *)clientData;
	pl->intselect = value;	
	update_interrupts(pl);
        return;
}

uint32_t
intenable_read(void *clientData,uint32_t address,int rqlen) 
{
	PL190  *pl = (PL190*) clientData;
	return pl->intenable;
}

static void
intenable_write(void *clientData,uint32_t value,uint32_t address,int rqlen) 
{
	PL190  *pl = (PL190*) clientData;
	pl->intenable |= value;
	update_interrupts(pl);	
        return;
}
uint32_t
intenclear_read(void *clientData,uint32_t address,int rqlen) 
{
	fprintf(stderr,"VIC register intenclear is not readable\n");
	return 0;
}

static void
intenclear_write(void *clientData,uint32_t value,uint32_t address,int rqlen) 
{
	PL190  *pl = (PL190*) clientData;
	pl->intenable &= ~value;
	update_interrupts(pl);
        return;
}
/*
 * ------------------------------------------------------
 * softint register
 *	Trigger an interrupt by software
 * ------------------------------------------------------
 */
uint32_t
softint_read(void *clientData,uint32_t address,int rqlen) 
{
	PL190 *pl = (PL190 *)clientData;
	return pl->softint;
}

static void
softint_write(void *clientData,uint32_t value,uint32_t address,int rqlen) 
{
	PL190 *pl = (PL190 *)clientData;
	pl->softint |= value;
	update_interrupts(pl);
        return;
}

uint32_t
softintclear_read(void *clientData,uint32_t address,int rqlen) 
{
	fprintf(stderr,"PL190: Softintclear is write only\n");
	return 0;
}

static void
softintclear_write(void *clientData,uint32_t value,uint32_t address,int rqlen) 
{
	PL190 *pl = (PL190 *) clientData;
	pl->softint &= ~value;
	update_interrupts(pl);
        return;
}

uint32_t
protection_read(void *clientData,uint32_t address,int rqlen) 
{
	return 0;
}

static void
protection_write(void *clientData,uint32_t value,uint32_t address,int rqlen) 
{
        fprintf(stderr,"PL190: Protection not implemented\n");
        return;
}

static void
push_privilege_level(PL190 *pl) {
	if(pl->vectaddr_pl != PRIV_LEVEL_INVALID) {
		if(pl->pl_stackp < 16) {
			pl->pl_stack[pl->pl_stackp] = pl->required_pl;
			pl->pl_stackp++;
		} else {
			fprintf(stderr,"PL190 Bug: privilege level stack overflow\n");
			exit(1);
		}
		/* This may result in -1 ! This is Ok and intended */
		pl->required_pl = pl->vectaddr_pl - 1; 
	}
}

static void
pop_privilege_level(PL190 *pl) {
	if(pl->pl_stackp > 0) {
		pl->pl_stackp--;
		pl->required_pl = pl->pl_stack[pl->pl_stackp];
	}
}
/*
 *******************************************************************************
 * When reading the vectaddr the interrupt priority logic is updated.
 * The vectaddr disappears after reading (Tested with LPC2106)
 *******************************************************************************
 */
uint32_t
vectaddr_read(void *clientData,uint32_t address,int rqlen) 
{
	PL190 *pl = (PL190 *) clientData;
	uint32_t retval = pl->vectaddr;
	push_privilege_level(pl);
	update_interrupts(pl);
	return retval;
}

/*
 **************************************************************************************
 * When writing the vectaddr the old interrupt privilege level comes into effect
 * by popping from the stack 
 **************************************************************************************
 */
static void
vectaddr_write(void *clientData,uint32_t value,uint32_t address,int rqlen) 
{
	PL190 *pl = (PL190 *) clientData;
	pop_privilege_level(pl);
	update_interrupts(pl);
        return;
}

/*
 * --------------------------------------------------------------
 * defvectaddr is the address for all non vectored interrupts
 * --------------------------------------------------------------
 */
uint32_t
defvectaddr_read(void *clientData,uint32_t address,int rqlen) 
{
	PL190 *pl = (PL190 *)clientData;
	return pl->defvectaddr;
}

static void
defvectaddr_write(void *clientData,uint32_t value,uint32_t address,int rqlen) 
{
	PL190 *pl = (PL190 *)clientData;
	pl->defvectaddr = value;
        return;
}

uint32_t
vectaddrn_read(void *clientData,uint32_t address,int rqlen) 
{
	PL190 *pl = (PL190 *)clientData;
	unsigned int index = ((address - VICVECTADDRN(0)) & 0x3f) >> 2;
	return pl->vectaddrn[index];
}

static void
vectaddrn_write(void *clientData,uint32_t value,uint32_t address,int rqlen) 
{
	PL190 *pl = (PL190 *)clientData;
	unsigned int index = ((address - VICVECTADDRN(0)) & 0x3f) >> 2;
	pl->vectaddrn[index] = value;
        return;
}

uint32_t
veccntl_read(void *clientData,uint32_t address,int rqlen) 
{
	PL190 *pl = (PL190 *)clientData;
	unsigned int index = ((address - VICVECCNTL(0)) & 0x3f) >> 2;
	return pl->veccntl[index];
}

static void
veccntl_write(void *clientData,uint32_t value,uint32_t address,int rqlen) 
{
	PL190 *pl = (PL190 *)clientData;
	unsigned int index = ((address - VICVECCNTL(0)) & 0x3f) >> 2;
	uint32_t oldval;
	oldval = pl->veccntl[index];
	pl->veccntl[index]=value;

	update_interrupts(pl);
        return;
}

uint32_t
periphid_read(void *clientData,uint32_t address,int rqlen) 
{
	static uint8_t perid[]={0x90,0x11,0x10,0};
	uint32_t index = (address>>2) & 0x3;
	return perid[index];
}

static void
periphid_write(void *clientData,uint32_t value,uint32_t address,int rqlen) 
{
        fprintf(stderr,"VIC register PERIPHID %08x is not writable\n",address);
        return;
}
uint32_t
pcellid_read(void *clientData,uint32_t address,int rqlen) 
{
	static uint8_t cellid[]={0x0d,0xf0,0x05,0xB1};
	uint32_t index = (address>>2) & 0x3;
	return cellid[index];
}

static void
pcellid_write(void *clientData,uint32_t value,uint32_t address,int rqlen) 
{
        fprintf(stderr,"VIC pcellid register at %08x is not writable\n",address);
        return;
}

uint32_t
vicitcr_read(void *clientData,uint32_t address,int rqlen) 
{
        fprintf(stderr,"PL190: Read Test mode register\n");
	return 0; 
}

static void
vicitcr_write(void *clientData,uint32_t value,uint32_t address,int rqlen) 
{
        fprintf(stderr,"PL190: Write Test mode register VICITRC: 0x%08x\n",value);
        return;
}

uint32_t
vicitip1_read(void *clientData,uint32_t address,int rqlen) 
{
        fprintf(stderr,"PL190: Read Test input register VICITIP1\n");
	return 0; 
}

static void
vicitip1_write(void *clientData,uint32_t value,uint32_t address,int rqlen) 
{
        fprintf(stderr,"PL190: Write Test mode register VICITIP1: 0x%08x\n",value);
        return;
}

uint32_t
vicitip2_read(void *clientData,uint32_t address,int rqlen) 
{
        fprintf(stderr,"PL190: Read Test input register VICITIP2\n");
	return 0; 
}

static void
vicitip2_write(void *clientData,uint32_t value,uint32_t address,int rqlen) 
{
        fprintf(stderr,"PL190: Write Test mode register VICITIP2: 0x%08x\n",value);
        return;
}

uint32_t
vicitop1_read(void *clientData,uint32_t address,int rqlen) 
{
        fprintf(stderr,"PL190: Read Test input register VICITOP1\n");
	return 0; 
}

static void
vicitop1_write(void *clientData,uint32_t value,uint32_t address,int rqlen) 
{
        fprintf(stderr,"PL190: Write Test mode register VICITOP1: 0x%08x\n",value);
        return;
}
uint32_t
vicitop2_read(void *clientData,uint32_t address,int rqlen) 
{
        fprintf(stderr,"PL190: Read Test input register VICITOP2\n");
	return 0; 
}

static void
vicitop2_write(void *clientData,uint32_t value,uint32_t address,int rqlen) 
{
        fprintf(stderr,"PL190: Write Test mode register VICITOP2: 0x%08x\n",value);
        return;
}

static void
PL190_Map(void *owner,uint32_t base,uint32_t mask,uint32_t flags)
{
        PL190 *pl=(PL190*)owner;
	int i;
	IOH_New32(base+VICIRQSTATUS,irqstatus_read,irqstatus_write,pl);
	IOH_New32(base+VICFIQSTATUS,fiqstatus_read,fiqstatus_write,pl);
	IOH_New32(base+VICRAWINTR,rawintr_read,rawintr_write,pl);
	IOH_New32(base+VICINTSELECT,intselect_read,intselect_write,pl);
	IOH_New32(base+VICINTENABLE,intenable_read,intenable_write,pl);
	IOH_New32(base+VICINTENCLEAR,intenclear_read,intenclear_write,pl);
	IOH_New32(base+VICSOFTINT,softint_read,softint_write,pl);
	IOH_New32(base+VICSOFTINTCLEAR,softintclear_read,softintclear_write,pl);
	IOH_New32(base+VICPROTECTION,protection_read,protection_write,pl);
	IOH_New32(base+VICVECTADDR,vectaddr_read,vectaddr_write,pl);
	IOH_New32(base+VICDEFVECTADDR,defvectaddr_read,defvectaddr_write,pl);
	for(i=0;i<16;i++) {
		IOH_New32(base+VICVECTADDRN(i),vectaddrn_read,vectaddrn_write,pl);
		IOH_New32(base+VICVECCNTL(i),veccntl_read,veccntl_write,pl);
	}
	for(i=0;i<4;i++) {
		IOH_New32(base+VICPERIPHID(i),periphid_read,periphid_write,pl);
		IOH_New32(base+VICPCELLID(i),pcellid_read,pcellid_write,pl);
	}
	IOH_New32(base + VICITCR,vicitcr_read,vicitcr_write,pl);
	IOH_New32(base + VICITIP1,vicitip1_read,vicitip1_write,pl);
	IOH_New32(base + VICITIP2,vicitip2_read,vicitip2_write,pl);
	IOH_New32(base + VICITOP1,vicitop1_read,vicitop1_write,pl);
	IOH_New32(base + VICITOP2,vicitop2_read,vicitop2_write,pl);
}

static void
PL190_UnMap(void *owner,uint32_t base,uint32_t mask)
{
	int i;
	IOH_Delete32(base+VICIRQSTATUS);
	IOH_Delete32(base+VICFIQSTATUS);
	IOH_Delete32(base+VICRAWINTR);
	IOH_Delete32(base+VICINTSELECT);
	IOH_Delete32(base+VICINTENABLE);
	IOH_Delete32(base+VICINTENCLEAR);
	IOH_Delete32(base+VICSOFTINT);
	IOH_Delete32(base+VICSOFTINTCLEAR);
	IOH_Delete32(base+VICPROTECTION);
	IOH_Delete32(base+VICVECTADDR);
	IOH_Delete32(base+VICDEFVECTADDR);
	for(i=0;i<16;i++) {
		IOH_Delete32(base+VICVECTADDRN(i));
		IOH_Delete32(base+VICVECCNTL(i));
	}
	for(i=0;i<4;i++) {
		IOH_Delete32(base+VICPERIPHID(i));
		IOH_Delete32(base+VICPCELLID(i));
	}

}

BusDevice *
PL190_New(const char *name) 
{
	PL190 *pl;
	int i;
	pl = sg_new(PL190);
	for(i=0;i<32;i++) {
		IrqTraceInfo *ti = sg_new(IrqTraceInfo);
		ti->nr = i;
		ti->pl190 = pl;
		pl->traceInfo[i] = ti;
		if(!(pl->intSourceNode[i]=SigNode_New("%s.nVICINTSOURCE%d",name,i))) {	
			exit(324);
		}
		pl->intSourceTrace[i] = SigNode_Trace(pl->intSourceNode[i],int_source_change,ti);
	}
	if(!(pl->irqNode = SigNode_New("%s.irq",name))) 
	{
		exit(2);
	} 
	if(!(pl->fiqNode = SigNode_New("%s.fiq",name))) 
	{
		exit(2);
	} 
	SigNode_Set(pl->irqNode,SIG_HIGH);	
	SigNode_Set(pl->fiqNode,SIG_HIGH);	

	/* Initialize priority encoder */
	pl->required_pl = 15; 
	pl->vectaddr_pl = PRIV_LEVEL_INVALID;
	pl->pl_stackp = 0;

	pl->bdev.first_mapping=NULL;
        pl->bdev.Map=PL190_Map;
        pl->bdev.UnMap=PL190_UnMap;
        pl->bdev.owner=pl;
        pl->bdev.hw_flags=MEM_FLAG_WRITABLE|MEM_FLAG_READABLE;
	fprintf(stderr,"PL190 Interrupt Controller created\n");
	return &pl->bdev;
}
