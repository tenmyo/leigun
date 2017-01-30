/*
 **************************************************************************************************
 *
 * Emulation of AITC interrupt controller
 *
 * Status: working but incomplete 
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

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "bus.h"
#include "signode.h"
#include "aitc.h"
#include "sgstring.h"

#define INTCNTL(base)		((base)+0)
#define		INTCNTL_ABFLAG	(1<<25)
#define		INTCNTL_ABFEN	(1<<24)
#define		INTCNTL_NDIS	(1<<22)
#define		INTCNTL_FDIS	(1<<21)
#define		INTCNTL_NIAD	(1<<20)
#define		INTCNTL_FIAD	(1<<19)
#define		INTCNTL_MD	(1<<16)
#define		INTCNTL_POINTER_MASK	(0x3ff<<2)

#define NIMASK(base)		((base)+4)
#define INTENNUM(base)		((base)+8)
#define INTDISNUM(base) 	((base)+0xc)
#define INTENABLEH(base) 	((base)+0x10)
#define INTENABLEL(base) 	((base)+0x14)
#define INTTYPEH(base)		((base)+0x18)
#define INTTYPEL(base) 		((base)+0x1c)
#define NIPRIORITY7(base)	((base)+0x20)
#define NIPRIORITY6(base)	((base)+0x24)
#define NIPRIORITY5(base)	((base)+0x28)
#define NIPRIORITY4(base)	((base)+0x2c)
#define NIPRIORITY3(base)	((base)+0x30)
#define NIPRIORITY2(base)	((base)+0x34)
#define NIPRIORITY1(base)	((base)+0x38)
#define NIPRIORITY0(base)	((base)+0x3c)
#define NIVECSR(base)		((base)+0x40)
#define FIVECSR(base)		((base)+0x44)
#define INTSRCH(base)		((base)+0x48)
#define INTSRCL(base)		((base)+0x4c)
#define INTFRCH(base)		((base)+0x50)
#define INTFRCL(base)		((base)+0x54)
#define NIPNDH(base)		((base)+0x58)
#define NIPNDL(base)		((base)+0x5c)
#define FIPNDH(base)		((base)+0x60)
#define FIPNDL(base)		((base)+0x64)

typedef struct IrqTraceInfo IrqTraceInfo;

typedef struct Aitc {
	BusDevice bdev;
	SigNode *nIntinNode[64];
	SigTrace *intSourceTrace[64];
	struct IrqTraceInfo *traceInfo[64];

	uint32_t intcntl;
	uint32_t nimask;
	uint32_t intennum;
	uint32_t intdisnum;
	uint64_t intenable;
	uint64_t inttype;
	uint8_t nipr[64];
	uint16_t nivector;
	uint16_t niprilvl;
	uint32_t fivecsr;
	uint64_t intsrc;
	uint64_t intfrc;
	uint64_t nipnd;
	uint64_t fipnd;
	/* Interrupt controller output */
	SigNode *irqNode;
	SigNode *fiqNode;
} Aitc;

struct IrqTraceInfo {
	int nr;
	Aitc *aitc;
};

static void
update_interrupts(Aitc * ai)
{
	int i;
	uint64_t raw_interrupts;
	uint64_t fipnd;
	uint64_t nipnd;
	raw_interrupts = ai->intsrc | ai->intfrc;
	ai->fipnd = fipnd = raw_interrupts & ai->intenable & ai->inttype;
	ai->nipnd = nipnd = raw_interrupts & ai->intenable & ~ai->inttype;
	if (ai->fipnd) {
		/* post fiq */
		SigNode_Set(ai->fiqNode, SIG_LOW);
	} else {
		SigNode_Set(ai->fiqNode, SIG_HIGH);
	}
	if (ai->nipnd) {
		int maxlevel = 0;
		int interrupt = -1;
		for (i = 0; i < 64; i++) {
			if (ai->nipnd & ((uint64_t) 1 << i)) {
				if (ai->nipr[i] > maxlevel) {
					maxlevel = ai->nipr[i];
				}
			}
		}
		//fprintf(stderr,"Max nipri is %d, nimask is %08x\n",maxlevel,ai->nimask);
		if ((ai->nimask == 0x1f) || (maxlevel > ai->nimask)) {
			for (i = 63; i >= 0; i--) {
				if (ai->nipnd & ((uint64_t) 1 << i)) {
					if (ai->nipr[i] == maxlevel) {
						maxlevel = ai->nipr[i];
						interrupt = i;
						break;
					}
				}
			}
		}
		if (interrupt >= 0) {
			//fprintf(stderr,"AITC Int ON\n");
			SigNode_Set(ai->irqNode, SIG_LOW);
		} else {
			//fprintf(stderr,"AITC Int OFF\n");
			SigNode_Set(ai->irqNode, SIG_HIGH);
		}
	} else {
		//fprintf(stderr,"AITC Int OFF\n");
		SigNode_Set(ai->irqNode, SIG_HIGH);
	}
}

/*
 * -----------------------------------------------------------
 * int_source_change
 *      irq line trace, called whenever a change occurs
 * -----------------------------------------------------------
 */
static void
int_source_change(SigNode * node, int value, void *clientData)
{
	IrqTraceInfo *ti = (IrqTraceInfo *) clientData;
	Aitc *ai = ti->aitc;
	int irq = ti->nr;
	if (value == SIG_LOW) {
		ai->intsrc |= ((uint64_t) 1 << irq);
	} else {
		ai->intsrc &= ~((uint64_t) 1 << irq);
	}
	update_interrupts(ai);
}

/*
 * -----------------------------------------------------------------
 * Interrupt control register
 *	ABFLAG: Indication risen bus Arbitration priority 
 * -----------------------------------------------------------------
 */
static uint32_t
intcntl_read(void *clientData, uint32_t address, int rqlen)
{
	Aitc *ai = (Aitc *) clientData;
	return ai->intcntl;
}

static void
intcntl_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	Aitc *ai = (Aitc *) clientData;

	ai->intcntl = (value & 0x01790ffc) | (ai->intcntl & INTCNTL_ABFLAG);
	/* Write 1 to clear */
	if (value & INTCNTL_ABFLAG) {
		ai->intcntl = ai->intcntl & ~INTCNTL_ABFLAG;
	}
	return;
}

static uint32_t
nimask_read(void *clientData, uint32_t address, int rqlen)
{
	Aitc *ai = (Aitc *) clientData;
	return ai->nimask;
}

static void
nimask_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	Aitc *ai = (Aitc *) clientData;
	ai->nimask = value & 0x1f;
	return;
}

static uint32_t
intennum_read(void *clientData, uint32_t address, int rqlen)
{
	return 0;
}

static void
intennum_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	Aitc *ai = (Aitc *) clientData;
	// enable interrupt source
	if (value < 64) {
		ai->intenable |= ((uint64_t) 1 << value);
		update_interrupts(ai);
	} else {
		fprintf(stderr, "AITC: Illegal interrupt number %d\n", value);
	}
	return;
}

static uint32_t
intdisnum_read(void *clientData, uint32_t address, int rqlen)
{
	return 0;
}

static void
intdisnum_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	Aitc *ai = (Aitc *) clientData;
	// disable interrupt source
	//fprintf(stderr,"intdisnum %d\n",value);
	if (value < 64) {
		ai->intenable &= ~((uint64_t) 1 << value);
		update_interrupts(ai);
	} else {
		fprintf(stderr, "AITC: Illegal interrupt number %d\n", value);
	}
	return;
}

static uint32_t
intenableh_read(void *clientData, uint32_t address, int rqlen)
{
	Aitc *ai = (Aitc *) clientData;
	return ai->intenable >> 32;
}

static void
intenableh_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	Aitc *ai = (Aitc *) clientData;
	ai->intenable = (ai->intenable & 0xffffffff) | ((uint64_t) value << 32);
	update_interrupts(ai);
	return;
}

static uint32_t
intenablel_read(void *clientData, uint32_t address, int rqlen)
{
	Aitc *ai = (Aitc *) clientData;
	return ai->intenable;
}

static void
intenablel_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	Aitc *ai = (Aitc *) clientData;
	ai->intenable = (ai->intenable & 0xffffffff00000000LL) | value;
	update_interrupts(ai);
	return;
}

/* Fast or Normal Interrupt (FIQ/IRQ) */
static uint32_t
inttypeh_read(void *clientData, uint32_t address, int rqlen)
{
	Aitc *ai = (Aitc *) clientData;
	return ai->inttype >> 32;
}

static void
inttypeh_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	Aitc *ai = (Aitc *) clientData;
	ai->inttype = (ai->inttype & 0xffffffff) | ((uint64_t) value << 32);
	update_interrupts(ai);
	return;
}

static uint32_t
inttypel_read(void *clientData, uint32_t address, int rqlen)
{
	Aitc *ai = (Aitc *) clientData;
	return ai->inttype;
}

static void
inttypel_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	Aitc *ai = (Aitc *) clientData;
	ai->inttype = (ai->inttype & 0xffffffff00000000LL) | value;
	update_interrupts(ai);
	return;
}

static uint32_t
nipriority_read(void *clientData, uint32_t address, int rqlen)
{
	Aitc *ai = (Aitc *) clientData;
	uint32_t value = 0;
	int index = (address >> 2) & 7;
	unsigned int i;
	for (i = 0; i < 8; i++) {
		value |= ai->nipr[(index << 3) + i] << (i * 4);
	}
	return value;
}

static void
nipriority_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	Aitc *ai = (Aitc *) clientData;
	int index = (address >> 2) & 7;
	unsigned int i;
	for (i = 0; i < 8; i++) {
		ai->nipr[(index << 3) + i] = (value >> (i * 4)) & 0xf;
	}
	update_interrupts(ai);
	return;
}

static uint32_t
nivecsr_read(void *clientData, uint32_t address, int rqlen)
{
	Aitc *ai = (Aitc *) clientData;
	int i;
	uint16_t nivector = -1;
	uint16_t niprilvl = -1;
	for (i = 0; i < 64; i++) {
		if (ai->nipnd & ((uint64_t) 1 << i)) {
			if (ai->nipr[i] > (int16_t) niprilvl) {
				niprilvl = ai->nipr[i];
				nivector = i;
			}
		}
	}
	return ((uint32_t) nivector << 16) | niprilvl;
}

static void
nivecsr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "AITC: Trying to write nivecsr register (ro)\n");
	return;
}

static uint32_t
fivecsr_read(void *clientData, uint32_t address, int rqlen)
{
	Aitc *ai = (Aitc *) clientData;
	int i;
	if (ai->fipnd) {
		for (i = 63; i >= 0; i--) {
			if (ai->fipnd & ((uint64_t) 1 << i)) {
				ai->fivecsr = i;
				break;
			}
		}
	} else {
		ai->fivecsr = (int64_t) - 1;
	}
	return ai->fivecsr;
}

static void
fivecsr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "AITC: Trying to write fivecsr register (ro)\n");
	return;
}

static uint32_t
intsrch_read(void *clientData, uint32_t address, int rqlen)
{
	Aitc *ai = (Aitc *) clientData;
	return ai->intsrc >> 32;
}

static void
intsrch_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "AITC: Trying to write INTSRC_H register (ro)\n");
	return;
}

static uint32_t
intsrcl_read(void *clientData, uint32_t address, int rqlen)
{
	Aitc *ai = (Aitc *) clientData;
	return ai->intsrc & 0xffffffff;
}

static void
intsrcl_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "AITC: Trying to write INTSRC_L register (ro)\n");
	return;
}

static uint32_t
intfrch_read(void *clientData, uint32_t address, int rqlen)
{
	Aitc *ai = (Aitc *) clientData;
	return ai->intfrc >> 32;
}

static void
intfrch_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	Aitc *ai = (Aitc *) clientData;
	ai->intfrc = (ai->intfrc & 0xffffffff) | value;
	update_interrupts(ai);
	return;
}

static uint32_t
intfrcl_read(void *clientData, uint32_t address, int rqlen)
{
	Aitc *ai = (Aitc *) clientData;
	return ai->intfrc;
}

static void
intfrcl_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	Aitc *ai = (Aitc *) clientData;
	ai->intfrc = (ai->intfrc & 0xffffffff00000000LL) | value;
	update_interrupts(ai);
	return;
}

static uint32_t
nipndh_read(void *clientData, uint32_t address, int rqlen)
{
	Aitc *ai = (Aitc *) clientData;
	return ai->nipnd >> 32;
}

static void
nipndh_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "AITC: nipndh not writable\n");
	return;
}

static uint32_t
nipndl_read(void *clientData, uint32_t address, int rqlen)
{
	Aitc *ai = (Aitc *) clientData;
	return ai->nipnd;
}

static void
nipndl_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "AITC: nipndl not writable\n");
	return;
}

static uint32_t
fipndh_read(void *clientData, uint32_t address, int rqlen)
{
	Aitc *ai = (Aitc *) clientData;
	return ai->fipnd >> 32;
}

static void
fipndh_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "AITC: fipndh not writable\n");
	return;
}

static uint32_t
fipndl_read(void *clientData, uint32_t address, int rqlen)
{
	Aitc *ai = (Aitc *) clientData;
	return ai->fipnd;
}

static void
fipndl_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "AITC: fipndl not writable\n");
	return;
}

static void
Aitc_UnMap(void *owner, uint32_t base, uint32_t mask)
{
	IOH_Delete32(INTCNTL(base));
	IOH_Delete32(NIMASK(base));
	IOH_Delete32(INTENNUM(base));
	IOH_Delete32(INTDISNUM(base));
	IOH_Delete32(INTENABLEH(base));
	IOH_Delete32(INTENABLEL(base));
	IOH_Delete32(INTTYPEH(base));
	IOH_Delete32(INTTYPEL(base));
	IOH_Delete32(NIPRIORITY7(base));
	IOH_Delete32(NIPRIORITY6(base));
	IOH_Delete32(NIPRIORITY5(base));
	IOH_Delete32(NIPRIORITY4(base));
	IOH_Delete32(NIPRIORITY3(base));
	IOH_Delete32(NIPRIORITY2(base));
	IOH_Delete32(NIPRIORITY1(base));
	IOH_Delete32(NIPRIORITY0(base));
	IOH_Delete32(NIVECSR(base));
	IOH_Delete32(FIVECSR(base));
	IOH_Delete32(INTSRCH(base));
	IOH_Delete32(INTFRCH(base));
	IOH_Delete32(INTFRCL(base));
	IOH_Delete32(NIPNDH(base));
	IOH_Delete32(NIPNDL(base));
	IOH_Delete32(FIPNDH(base));
	IOH_Delete32(FIPNDL(base));
}

static void
Aitc_Map(void *owner, uint32_t base, uint32_t mask, uint32_t flags)
{
	Aitc *ai = (Aitc *) owner;
	IOH_New32(INTCNTL(base), intcntl_read, intcntl_write, ai);
	IOH_New32(NIMASK(base), nimask_read, nimask_write, ai);
	IOH_New32(INTENNUM(base), intennum_read, intennum_write, ai);
	IOH_New32(INTDISNUM(base), intdisnum_read, intdisnum_write, ai);
	IOH_New32(INTENABLEH(base), intenableh_read, intenableh_write, ai);
	IOH_New32(INTENABLEL(base), intenablel_read, intenablel_write, ai);
	IOH_New32(INTTYPEH(base), inttypeh_read, inttypeh_write, ai);
	IOH_New32(INTTYPEL(base), inttypel_read, inttypel_write, ai);
	IOH_New32(NIPRIORITY7(base), nipriority_read, nipriority_write, ai);
	IOH_New32(NIPRIORITY6(base), nipriority_read, nipriority_write, ai);
	IOH_New32(NIPRIORITY5(base), nipriority_read, nipriority_write, ai);
	IOH_New32(NIPRIORITY4(base), nipriority_read, nipriority_write, ai);
	IOH_New32(NIPRIORITY3(base), nipriority_read, nipriority_write, ai);
	IOH_New32(NIPRIORITY2(base), nipriority_read, nipriority_write, ai);
	IOH_New32(NIPRIORITY1(base), nipriority_read, nipriority_write, ai);
	IOH_New32(NIPRIORITY0(base), nipriority_read, nipriority_write, ai);
	IOH_New32(NIVECSR(base), nivecsr_read, nivecsr_write, ai);
	IOH_New32(FIVECSR(base), fivecsr_read, fivecsr_write, ai);
	IOH_New32(INTSRCH(base), intsrch_read, intsrch_write, ai);
	IOH_New32(INTSRCL(base), intsrcl_read, intsrcl_write, ai);
	IOH_New32(INTFRCH(base), intfrch_read, intfrch_write, ai);
	IOH_New32(INTFRCL(base), intfrcl_read, intfrcl_write, ai);
	IOH_New32(NIPNDH(base), nipndh_read, nipndh_write, ai);
	IOH_New32(NIPNDL(base), nipndl_read, nipndl_write, ai);
	IOH_New32(FIPNDH(base), fipndh_read, fipndh_write, ai);
	IOH_New32(FIPNDL(base), fipndl_read, fipndl_write, ai);
}

BusDevice *
Aitc_New(const char *name)
{
	Aitc *ai;
	int i;
	ai = sg_new(Aitc);
	ai->irqNode = SigNode_New("%s.irq", name);
	ai->fiqNode = SigNode_New("%s.fiq", name);
	if (!ai->irqNode || !ai->fiqNode) {
		fprintf(stderr, "can not create irqnodes\n");
		exit(2);
	}
	ai->nimask = 0x1f;
	for (i = 0; i < 64; i++) {
		IrqTraceInfo *ti = sg_new(IrqTraceInfo);
		ai->nIntinNode[i] = SigNode_New("%s.nIntSrc%d", name, i);
		if (!ai->nIntinNode[i]) {
			fprintf(stderr, "Can not create interrupt node for irq %d\n", i);
			exit(2);
		}
		ai->intSourceTrace[i] = SigNode_Trace(ai->nIntinNode[i], int_source_change, ti);
		if (!ti) {
			fprintf(stderr, "out of memory allocating IrqTrace\n");
			exit(342);
		}
		ti->nr = i;
		ti->aitc = ai;
		ai->traceInfo[i] = ti;
	}
	update_interrupts(ai);
	ai->bdev.first_mapping = NULL;
	ai->bdev.Map = Aitc_Map;
	ai->bdev.UnMap = Aitc_UnMap;
	ai->bdev.owner = ai;
	ai->bdev.hw_flags = MEM_FLAG_WRITABLE | MEM_FLAG_READABLE;
	fprintf(stderr, "AITC ARM Interrupt Controller created\n");
	return &ai->bdev;
}
