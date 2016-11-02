/*
 *************************************************************************************************
 *
 * Emulation of S3C2410 Interrupt Controller 
 *
 * state: Not implemented 
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
#include <bus.h>
#include <signode.h>
#include <configfile.h>
#include <s3c2410_intco.h>
#include <sgstring.h>

/* Base is 0x4a000000 */
#define INTCO_SRCPND(base) 	((base)+0x00)
#define	INTCO_INTMOD(base)	((base)+0x04)
#define INTCO_INTMSK(base)	((base)+0x08)
#define	INTCO_PRIORITY(base)	((base)+0x0c)
#define INTCO_INTPND(base)	((base)+0x10)
#define INTCO_INTOFFSET(base)	((base)+0x14)
#define INTCO_SUBSRCPND(base)	((base)+0x18)
#define INTCO_INTSUBMSK(base)	((base)+0x1c)

typedef struct IrqTraceInfo IrqTraceInfo;

typedef struct Intco {
	BusDevice bdev;
	SigNode *irqIn[32];
	unsigned int arb_rotation[7];
	uint32_t srcpnd;
	uint32_t intmod;
	uint32_t intmsk;
	uint32_t intpnd;
	uint32_t intoffset;
	uint32_t priority;
	uint32_t subsrcpnd;
	uint32_t intsubmsk;
	SigNode *irqOut;
	SigNode *fiqOut;
	IrqTraceInfo *traceInfo[32];
} Intco;

struct IrqTraceInfo {
	int irqnr;
	Intco *ic;
};

static int prio_seq[4][6] = {
	{0, 1, 2, 3, 4, 5},
	{0, 2, 3, 4, 1, 5},
	{0, 3, 4, 1, 2, 5},
	{0, 4, 1, 2, 3, 5}
};

static int groupmask_arr[] = {
	0x0000000f,
	0x000003f0,
	0x0000fc00,
	0x003f0000,
	0x0fc00000,
	0xf0000000
};

static void
update_interrupts(Intco * ic)
{
	unsigned int i, j;
	int arb_sel6 = (ic->priority >> 19) & 3;
	int arb_rot6 = (ic->priority >> 6) & 1;
	int *groupp = prio_seq[arb_sel6];
	for (i = 0; i < 6; i++) {
		int arb_sel, arb_rot;
		int *priop;
		int group = groupp[(i + ic->arb_rotation[6]) % 6];
		unsigned int groupmask;
		/* First check if any interrupt in this group is active */
		groupmask = groupmask_arr[group % 6];
		if (ic->srcpnd & ic->intmsk & groupmask) {
			arb_sel = (ic->priority >> ((group * 2) + 7)) & 3;
			arb_rot = (ic->priority >> group) & 1;
			priop = prio_seq[arb_sel];
			for (j = 0; j < 6; j++) {
				int irq =
				    priop[(j + ic->arb_rotation[group] % 6)] + (group * 6 - 2);
				if ((irq > 0) && (irq < 32)) {
					if (ic->srcpnd & (1 << irq)) {
						if (!(ic->intpnd & (1 << irq))) {
							ic->intoffset = irq;
							ic->intpnd = (1 << irq);
							if (arb_rot6) {
								ic->arb_rotation[6]++;
							}
							if (arb_rot) {
								ic->arb_rotation[group]++;
							}
							/* do arb rotation */
						}
						// post interrupt
						SigNode_Set(ic->irqOut, SIG_LOW);
						return;
					}
				}
			}
		}
	}
	SigNode_Set(ic->irqOut, SIG_HIGH);
}

/*
 * -------------------------------------------------------------
 * Interrupt source pending register
 * -------------------------------------------------------------
 */
static uint32_t
srcpnd_read(void *clientData, uint32_t address, int rqlen)
{
	Intco *ic = (Intco *) clientData;
	return ic->srcpnd;
}

static void
srcpnd_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	Intco *ic = (Intco *) clientData;
	ic->srcpnd &= ~value;	/* Write one to clear */
}

/*
 * -----------------------------------------------------------------
 * Intmod
 *	0: interrupt is a normal interrupt
 *	1: interrupt is a FIQ
 * Only one bit can be set to 1 (only 1 FIQ)
 * -----------------------------------------------------------------
 */
static uint32_t
intmod_read(void *clientData, uint32_t address, int rqlen)
{
	Intco *ic = (Intco *) clientData;
	return ic->intmod;
}

static void
intmod_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	Intco *ic = (Intco *) clientData;
	int i;
	for (i = 0; i < 32; i++) {
		if (value & (1 << i)) {
			// route this to fiq
			ic->intmod = (1 << i);
			//SigNode_Link();
			return;
		}
	}
	ic->intmod = 0;
	// update_interrupts(ic);

	/* route one interrupt to fiq */
}

/*
 * ------------------------------------------------------------
 * INTMSK 
 *	0 = interrupt is serviced
 *	1 = interrupt will not be serviced
 * ------------------------------------------------------------
 */
static uint32_t
intmsk_read(void *clientData, uint32_t address, int rqlen)
{
	Intco *ic = (Intco *) clientData;
	return ic->intmsk;
}

static void
intmsk_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	Intco *ic = (Intco *) clientData;
	ic->intmsk = value;
	update_interrupts(ic);
	return;
}

static uint32_t
priority_read(void *clientData, uint32_t address, int rqlen)
{
	Intco *ic = (Intco *) clientData;
	return ic->priority;
}

static void
priority_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	Intco *ic = (Intco *) clientData;
	int i;
	for (i = 0; i < 7; i++) {
		if ((value & (1 << i)) == 0) {
			ic->arb_rotation[i] = 0;
		}
	}
}

/*
 * ---------------------------------------------------------------------------
 * The Intpending register has maximum 1 bit set because its behind the
 * priority encoder
 * ---------------------------------------------------------------------------
 */
static uint32_t
intpnd_read(void *clientData, uint32_t address, int rqlen)
{
	return 0;
}

static void
intpnd_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	Intco *ic = (Intco *) clientData;
	ic->intpnd &= ~value;
	update_interrupts(ic);
}

/*
 * -------------------------------------------------------------
 * The intofsset says which bit is set in the intpnd register
 * -------------------------------------------------------------
 */
static uint32_t
intoffset_read(void *clientData, uint32_t address, int rqlen)
{
	Intco *ic = (Intco *) clientData;
	return ic->intoffset;
}

static void
intoffset_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "INTOFFSET is a readonly register\n");
}

static uint32_t
subsrcpnd_read(void *clientData, uint32_t address, int rqlen)
{
	Intco *ic = (Intco *) clientData;
	return ic->subsrcpnd;
}

static void
subsrcpnd_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	Intco *ic = (Intco *) clientData;
	ic->subsrcpnd &= ~value;
}

static uint32_t
intsubmsk_read(void *clientData, uint32_t address, int rqlen)
{
	Intco *ic = (Intco *) clientData;
	return ic->intsubmsk;
}

static void
intsubmsk_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	Intco *ic = (Intco *) clientData;
	ic->intsubmsk = value & 0x7ff;
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
	Intco *ic = ti->ic;
	int irq = ti->irqnr;
	if (value == SIG_LOW) {
		ic->srcpnd |= (1 << irq);
		update_interrupts(ic);
	} else if (value == SIG_HIGH) {
		// I think nothing happens here
	}
}

static void
S3C2410Intco_Map(void *owner, uint32_t base, uint32_t mask, uint32_t flags)
{
	Intco *ic = (Intco *) owner;
	IOH_New32(INTCO_SRCPND(base), srcpnd_read, srcpnd_write, ic);
	IOH_New32(INTCO_INTMOD(base), intmod_read, intmod_write, ic);
	IOH_New32(INTCO_INTMSK(base), intmsk_read, intmsk_write, ic);
	IOH_New32(INTCO_PRIORITY(base), priority_read, priority_write, ic);
	IOH_New32(INTCO_INTPND(base), intpnd_read, intpnd_write, ic);
	IOH_New32(INTCO_INTOFFSET(base), intoffset_read, intoffset_write, ic);
	IOH_New32(INTCO_SUBSRCPND(base), subsrcpnd_read, subsrcpnd_write, ic);
	IOH_New32(INTCO_INTSUBMSK(base), intsubmsk_read, intsubmsk_write, ic);
}

static void
S3C2410Intco_UnMap(void *owner, uint32_t base, uint32_t mask)
{
	IOH_Delete32(INTCO_SRCPND(base));
	IOH_Delete32(INTCO_INTMOD(base));
	IOH_Delete32(INTCO_INTMSK(base));
	IOH_Delete32(INTCO_PRIORITY(base));
	IOH_Delete32(INTCO_INTPND(base));
	IOH_Delete32(INTCO_INTOFFSET(base));
	IOH_Delete32(INTCO_SUBSRCPND(base));
	IOH_Delete32(INTCO_INTSUBMSK(base));
}

BusDevice *
S3C2410Intco_New(const char *name)
{
	Intco *ic = sg_new(Intco);
	int i;
	ic->bdev.first_mapping = NULL;
	ic->bdev.Map = S3C2410Intco_Map;
	ic->bdev.UnMap = S3C2410Intco_UnMap;
	ic->bdev.owner = ic;
	ic->bdev.hw_flags = MEM_FLAG_WRITABLE | MEM_FLAG_READABLE;
	for (i = 0; i < 32; i++) {
		IrqTraceInfo *ti = ic->traceInfo[i] = sg_new(IrqTraceInfo);
		ti->irqnr = i;
		ti->ic = ic;
		ic->irqIn[i] = SigNode_New("%s.irq%d", name, i);
		if (!ic->irqIn[i]) {
			fprintf(stderr, "S3C2410Intco: Can't create interrupt input\n");
			exit(1);
		}
		SigNode_Trace(ic->irqIn[i], int_source_change, ti);
	}
	ic->irqOut = SigNode_New("%s.irqOut", name);
	ic->fiqOut = SigNode_New("%s.fiqOut", name);
	fprintf(stderr, "S3C2410 Interrupt Controller \"%s\" created\n", name);
	return &ic->bdev;
}
