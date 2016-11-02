/*
 **************************************************************************************************
 *
 * Emulation of AT91 Parallel Input/Output Controller (PIO) 
 *
 * state: working 
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
#include "bus.h"
#include "fio.h"
#include "signode.h"
#include "configfile.h"
#include "at91_pio.h"
#include "sgstring.h"

#define	PIO_PER(base)	((base) + 0x00)
#define PIO_PDR(base)	((base) + 0x04)
#define PIO_PSR(base)	((base) + 0x08)
#define PIO_OER(base)	((base) + 0x10)
#define PIO_ODR(base)	((base) + 0x14)
#define PIO_OSR(base)	((base) + 0x18)
#define PIO_IFER(base)	((base) + 0x20)
#define PIO_IFDR(base)	((base) + 0x24)
#define PIO_IFSR(base)	((base) + 0x28)
#define PIO_SODR(base)	((base) + 0x30)
#define PIO_CODR(base)	((base) + 0x34)
#define PIO_ODSR(base)	((base) + 0x38)
#define PIO_PDSR(base)	((base) + 0x3C)
#define PIO_IER(base)	((base) + 0x40)
#define PIO_IDR(base)	((base) + 0x44)
#define PIO_IMR(base)	((base) + 0x48)
#define PIO_ISR(base)	((base) + 0x4c)
#define PIO_MDER(base)	((base) + 0x50)
#define PIO_MDDR(base) 	((base) + 0x54)
#define PIO_MDSR(base)	((base) + 0x58)
#define PIO_PUDR(base)	((base) + 0x60)
#define PIO_PUER(base)	((base) + 0x64)
#define PIO_PUSR(base)	((base) + 0x68)
#define PIO_ASR(base)	((base) + 0x70)
#define PIO_BSR(base)	((base) + 0x74)
#define PIO_ABSR(base)	((base) + 0x78)
#define PIO_OWER(base)	((base) + 0xa0)
#define PIO_OWDR(base)	((base) + 0xa4)
#define PIO_OWSR(base)	((base) + 0xa8)

typedef struct AT91Pio AT91Pio;

typedef struct OR_TraceInfo {
	int index;
	AT91Pio *pio;
} OR_TraceInfo;

typedef struct Edge_TraceInfo {
	int index;
	AT91Pio *pio;
} Edge_TraceInfo;

struct AT91Pio {
	BusDevice bdev;
	char *name;
	uint32_t psr;
	uint32_t osr;
	uint32_t ifsr;
	uint32_t odsr;
	uint32_t pdsr;
	uint32_t imr;
	uint32_t isr;
	uint32_t mdsr;
	uint32_t pusr;
	uint32_t absr;
	uint32_t owsr;
	SigNode *irqNode;
	SigNode *gnd;		/* Ground */
	SigNode *pad[32];
	SigNode *pullup[32];
	SigNode *data[32];	/* The data register (ODSR) values           */
	SigNode *ds[32];	/* The selected data (behind psr)            */
	SigNode *dout[32];	/* The output before the driver              */
	SigNode *nosr[32];	/* The inverted signal from osr register */
	SigNode *poe[32];	/* Peripheral output enable (behind A/B selector)   */
	SigNode *paoe[32];	/* Peripheral output A  enable */
	SigNode *pboe[32];	/* Peripheral output B  enable */
	SigNode *po[32];	/* Peripheral output (behind A/B selector)   */
	SigNode *pao[32];	/* Peripheral output A */
	SigNode *pbo[32];	/* Peripheral output B */
	SigNode *noea[32];	/* First input to "OR" gate connected to noe */
	SigNode *noeb[32];	/* Second input to "OR" gate connected to noe */
	/* The OR gate */
	OR_TraceInfo or_trace_info[32];
	Edge_TraceInfo edge_trace_info[32];
};

static void
update_interrupt(AT91Pio * pio)
{
	/* Internal module "wired or" positive level interrupt */
	if (pio->isr & pio->imr) {
		SigNode_Set(pio->irqNode, SIG_HIGH);
	} else {
		SigNode_Set(pio->irqNode, SIG_PULLDOWN);
	}
}

/*
 * ----------------------------------------------------------------------
 * PSR: Peripheral select register
 *	Selects between output (data+oe) from peripheral and
 *	output from data register / osr register
 * ----------------------------------------------------------------------
 */
static void
psr_update(AT91Pio * pio, uint32_t diff)
{
	int i;
	for (i = 0; i < 32; i++) {
		if (((diff >> i) & 1) == 0) {
			continue;
		}
		/* First Link data then the output enable to guarantee stability */
		if (pio->psr & (1 << i)) {
			SigNode_RemoveLink(pio->poe[i], pio->noea[i]);
			SigNode_RemoveLink(pio->po[i], pio->ds[i]);
			SigNode_Link(pio->data[i], pio->ds[i]);
			SigNode_Link(pio->nosr[i], pio->noea[i]);
		} else {
			SigNode_RemoveLink(pio->nosr[i], pio->noea[i]);
			SigNode_RemoveLink(pio->data[i], pio->ds[i]);
			SigNode_Link(pio->po[i], pio->ds[i]);
			SigNode_Link(pio->poe[i], pio->noea[i]);
		}
	}
}

static uint32_t
per_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "AT91Pio: PER register is writeonly\n");
	return 0;
}

static void
per_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	AT91Pio *pio = (AT91Pio *) clientData;
	uint32_t newvalue = pio->psr | value;
	uint32_t diff = pio->psr ^ newvalue;
	pio->psr = newvalue;
	psr_update(pio, diff);
}

static uint32_t
pdr_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "AT91Pio: PDR writeonly\n");
	return 0;
}

static void
pdr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	AT91Pio *pio = (AT91Pio *) clientData;
	uint32_t newvalue = pio->psr & ~value;
	uint32_t diff = pio->psr ^ newvalue;
	pio->psr = newvalue;
	psr_update(pio, diff);
}

static uint32_t
psr_read(void *clientData, uint32_t address, int rqlen)
{
	AT91Pio *pio = (AT91Pio *) clientData;
	return pio->psr;
}

static void
psr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "AT91Pio: PSR is not writable\n");
}

static uint32_t
oer_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "AT91Pio: OER is not readable\n");
	return 0;
}

static void
oer_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	AT91Pio *pio = (AT91Pio *) clientData;
	uint32_t diff = pio->osr ^ (pio->osr | value);
	int i;
	pio->osr = pio->osr | value;
	if (diff) {
		for (i = 0; i < 32; i++) {
			if (diff & (1 << i)) {
				SigNode_Set(pio->nosr[i], SIG_LOW);
			}
		}
	}
}

static uint32_t
odr_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "AT91Pio: ODR is not readable\n");
	return 0;
}

static void
odr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	AT91Pio *pio = (AT91Pio *) clientData;
	uint32_t diff = pio->osr ^ (pio->osr & ~value);
	int i;
	pio->osr = pio->osr & ~value;
	if (diff) {
		for (i = 0; i < 32; i++) {
			if (diff & (1 << i)) {
				SigNode_Set(pio->nosr[i], SIG_HIGH);
			}
		}
	}
}

static uint32_t
osr_read(void *clientData, uint32_t address, int rqlen)
{
	AT91Pio *pio = (AT91Pio *) clientData;
	return pio->osr;
}

static void
osr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "AT91Pio: OSR is not writable\n");
}

/*
 * -----------------------------------------------------------------
 * Currently the spike filter has no effect
 * -----------------------------------------------------------------
 */
static uint32_t
ifer_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "AT91Pio: IFER is not readable\n");
	return 0;
}

static void
ifer_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	AT91Pio *pio = (AT91Pio *) clientData;
	pio->ifsr |= value;
}

static uint32_t
ifdr_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "AT91Pio: IFDR is not readable\n");
	return 0;
}

static void
ifdr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	AT91Pio *pio = (AT91Pio *) clientData;
	pio->ifsr &= ~value;
}

static uint32_t
ifsr_read(void *clientData, uint32_t address, int rqlen)
{
	AT91Pio *pio = (AT91Pio *) clientData;
	return pio->ifsr;
}

static void
ifsr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "AT91Pio: IFSR is not writable\n");
}

static uint32_t
sodr_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "AT91Pio: SODR register is not readable\n");
	return 0;
}

static void
sodr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	AT91Pio *pio = (AT91Pio *) clientData;
	uint32_t diff = pio->odsr ^ (pio->odsr | value);
	int i;
	pio->odsr |= value;
	if (!diff) {
		return;
	}
	for (i = 0; i < 32; i++) {
		if (diff & (1 << i)) {
			SigNode_Set(pio->data[i], SIG_HIGH);
		}
	}
}

static uint32_t
codr_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "AT91Pio: CODR register is not readable\n");
	return 0;
}

static void
codr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	AT91Pio *pio = (AT91Pio *) clientData;
	uint32_t diff = pio->odsr ^ (pio->odsr & ~value);
	int i;
	pio->odsr &= ~value;
	if (!diff) {
		return;
	}
	for (i = 0; i < 32; i++) {
		if (diff & (1 << i)) {
			SigNode_Set(pio->data[i], SIG_LOW);
		}
	}
}

static uint32_t
odsr_read(void *clientData, uint32_t address, int rqlen)
{
	AT91Pio *pio = (AT91Pio *) clientData;
	return pio->odsr;
}

static void
odsr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	AT91Pio *pio = (AT91Pio *) clientData;
	int i;
	pio->odsr = (pio->odsr & ~pio->owsr) | (value & pio->owsr);
	for (i = 0; i < 32; i++) {
		if (!(pio->owsr & (1 << i))) {
			continue;
		}
		if (pio->odsr & (1 << i)) {
			SigNode_Set(pio->data[i], SIG_HIGH);
		} else {
			SigNode_Set(pio->data[i], SIG_LOW);
		}
	}
}

static void
pdsr_update(SigNode * node, int value, void *clientData)
{
	Edge_TraceInfo *ti = (Edge_TraceInfo *) clientData;
	int nr = ti->index;
	AT91Pio *pio = ti->pio;
	if (value) {
		pio->pdsr |= (1 << nr);
	} else {
		pio->pdsr &= ~(1 << nr);
	}
	pio->isr |= (1 << nr);
	update_interrupt(pio);
}

static uint32_t
pdsr_read(void *clientData, uint32_t address, int rqlen)
{
	AT91Pio *pio = (AT91Pio *) clientData;
	return pio->pdsr;
}

static void
pdsr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "AT91Pio: PDSR is not writable\n");
}

static uint32_t
ier_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "AT91Pio: IER is not readable\n");
	return 0;
}

static void
ier_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	AT91Pio *pio = (AT91Pio *) clientData;
	pio->imr |= value;
	update_interrupt(pio);
}

static uint32_t
idr_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "AT91Pio: IDR is not readable\n");
	return 0;
}

static void
idr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	AT91Pio *pio = (AT91Pio *) clientData;
	pio->imr &= ~value;
	update_interrupt(pio);
}

static uint32_t
isr_read(void *clientData, uint32_t address, int rqlen)
{
	AT91Pio *pio = (AT91Pio *) clientData;
	uint32_t isr = pio->isr;
	pio->isr = 0;
	update_interrupt(pio);
	return isr;
}

static void
isr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "AT91Pio: write location 0x%08x not implemented\n", address);
}

static uint32_t
imr_read(void *clientData, uint32_t address, int rqlen)
{
	AT91Pio *pio = (AT91Pio *) clientData;
	return pio->imr;
}

static void
imr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "AT91Pio: IMR is not writable\n");
}

/* 
 * This is the or gate: It is invoked by change of noeb and noea
 */

static void
noe_update(SigNode * node, int value, void *clientData)
{
	OR_TraceInfo *ti = (OR_TraceInfo *) clientData;
	int nr = ti->index;
	AT91Pio *pio = ti->pio;
	if ((SigNode_Val(pio->noea[nr]) == SIG_HIGH)
	    || (SigNode_Val(pio->noeb[nr]) == SIG_HIGH)) {
		SigNode_RemoveLink(pio->dout[nr], pio->pad[nr]);
	} else {
		if (!SigNode_Linked(pio->dout[nr], pio->pad[nr])) {
			SigNode_Link(pio->dout[nr], pio->pad[nr]);
		}
	}
}

/*
 * The mdsr 
 */

static void
mdsr_update(AT91Pio * pio, uint32_t diff)
{
	int i;
	for (i = 0; i < 32; i++) {
		if (((diff >> i) & 1) == 0) {
			continue;
		}
		/* First connect data, then noeb, to guarantee stable value */
		if (pio->mdsr & (1 << i)) {
			SigNode_RemoveLink(pio->gnd, pio->noeb[i]);
			SigNode_RemoveLink(pio->gnd, pio->dout[i]);
			SigNode_Link(pio->ds[i], pio->dout[i]);
			SigNode_Link(pio->ds[i], pio->noeb[i]);
		} else {
			SigNode_RemoveLink(pio->ds[i], pio->noeb[i]);
			SigNode_RemoveLink(pio->ds[i], pio->dout[i]);
			SigNode_Link(pio->gnd, pio->dout[i]);
			SigNode_Link(pio->gnd, pio->noeb[i]);
		}
	}
}

static uint32_t
mder_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "AT91Pio: MDER is not readable\n");
	return 0;
}

static void
mder_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	AT91Pio *pio = (AT91Pio *) clientData;
	uint32_t newvalue = pio->mdsr | value;
	uint32_t diff = pio->mdsr ^ newvalue;
	pio->mdsr = newvalue;
	mdsr_update(pio, diff);
}

static uint32_t
mddr_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "AT91Pio: MDER is not readable\n");
	return 0;
}

static void
mddr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	AT91Pio *pio = (AT91Pio *) clientData;
	uint32_t newvalue = pio->mdsr & ~value;
	uint32_t diff = newvalue ^ pio->mdsr;
	pio->mdsr = newvalue;
	mdsr_update(pio, diff);
}

static uint32_t
mdsr_read(void *clientData, uint32_t address, int rqlen)
{
	AT91Pio *pio = (AT91Pio *) clientData;
	return pio->mdsr;
}

static void
mdsr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "AT91Pio: MDSR is not writable\n");
}

static uint32_t
pudr_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "AT91Pio: PUDR is not readable\n");
	return 0;
}

static void
pudr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	AT91Pio *pio = (AT91Pio *) clientData;
	uint32_t diff = pio->pusr ^ (pio->pusr & ~value);
	int i;
	pio->pusr &= ~value;
	for (i = 0; i < 32; i++) {
		if (diff & (1 << i)) {
			SigNode_RemoveLink(pio->pullup[i], pio->pad[i]);
		}
	}
}

static uint32_t
puer_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "PUER register is not readable\n");
	return 0;
}

static void
puer_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	AT91Pio *pio = (AT91Pio *) clientData;
	uint32_t diff = pio->pusr ^ (pio->pusr | value);
	int i;
	pio->pusr |= value;
	for (i = 0; i < 32; i++) {
		if (diff & (1 << i)) {
			SigNode_Link(pio->pullup[i], pio->pad[i]);
		}
	}
}

static uint32_t
pusr_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "AT91Pio: read location 0x%08x not implemented\n", address);
	return 0;
}

static void
pusr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "AT91Pio: write location 0x%08x not implemented\n", address);
}

static void
absr_update(AT91Pio * pio, uint32_t diff)
{
	int i;
	for (i = 0; i < 32; i++) {
		if (((diff >> i) & 1) == 0) {
			continue;
		}
		if (pio->absr & (1 << i)) {
			SigNode_RemoveLink(pio->paoe[i], pio->poe[i]);
			SigNode_RemoveLink(pio->pao[i], pio->po[i]);
			SigNode_Link(pio->pboe[i], pio->poe[i]);
			SigNode_Link(pio->pbo[i], pio->po[i]);
		} else {
			SigNode_RemoveLink(pio->pboe[i], pio->poe[i]);
			SigNode_RemoveLink(pio->pbo[i], pio->po[i]);
			SigNode_Link(pio->paoe[i], pio->poe[i]);
			SigNode_Link(pio->pao[i], pio->po[i]);
		}
	}
}

static uint32_t
asr_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "AT91Pio: ASR register is writeonly\n");
	return 0;
}

static void
asr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	AT91Pio *pio = (AT91Pio *) clientData;
	uint32_t newvalue = pio->absr & ~value;
	uint32_t diff = pio->absr ^ newvalue;
	pio->absr = newvalue;
	absr_update(pio, diff);
}

static uint32_t
bsr_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "AT91Pio: BSR register is writeonly\n");
	return 0;
}

static void
bsr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	AT91Pio *pio = (AT91Pio *) clientData;
	uint32_t newvalue = pio->absr | value;
	uint32_t diff = pio->absr ^ newvalue;
	pio->absr = newvalue;
	absr_update(pio, diff);
}

static uint32_t
absr_read(void *clientData, uint32_t address, int rqlen)
{
	AT91Pio *pio = (AT91Pio *) clientData;
	return pio->absr;
}

static void
absr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "AT91Pio: ABSR is readonly\n");
}

static uint32_t
ower_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "AT91Pio: read location 0x%08x not implemented\n", address);
	return 0;
}

/*
 * -----------------------------------------------------
 * owsr makes odsr readonly / readwrite
 * -----------------------------------------------------
 */
static void
ower_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	AT91Pio *pio = (AT91Pio *) clientData;
	pio->owsr |= value;
}

static uint32_t
owdr_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "AT91Pio: OWDR is not readable\n");
	return 0;
}

static void
owdr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	AT91Pio *pio = (AT91Pio *) clientData;
	pio->owsr &= ~value;
	return;
}

static uint32_t
owsr_read(void *clientData, uint32_t address, int rqlen)
{
	AT91Pio *pio = (AT91Pio *) clientData;
	return pio->owsr;
}

static void
owsr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "AT91Pio: write location 0x%08x not implemented\n", address);
}

static void
AT91Pio_Map(void *owner, uint32_t base, uint32_t mask, uint32_t flags)
{
	AT91Pio *pio = (AT91Pio *) owner;
	IOH_New32(PIO_PER(base), per_read, per_write, pio);
	IOH_New32(PIO_PDR(base), pdr_read, pdr_write, pio);
	IOH_New32(PIO_PSR(base), psr_read, psr_write, pio);
	IOH_New32(PIO_OER(base), oer_read, oer_write, pio);
	IOH_New32(PIO_ODR(base), odr_read, odr_write, pio);
	IOH_New32(PIO_OSR(base), osr_read, osr_write, pio);
	IOH_New32(PIO_IFER(base), ifer_read, ifer_write, pio);
	IOH_New32(PIO_IFDR(base), ifdr_read, ifdr_write, pio);
	IOH_New32(PIO_IFSR(base), ifsr_read, ifsr_write, pio);
	IOH_New32(PIO_SODR(base), sodr_read, sodr_write, pio);
	IOH_New32(PIO_CODR(base), codr_read, codr_write, pio);
	IOH_New32(PIO_ODSR(base), odsr_read, odsr_write, pio);
	IOH_New32(PIO_PDSR(base), pdsr_read, pdsr_write, pio);
	IOH_New32(PIO_IER(base), ier_read, ier_write, pio);
	IOH_New32(PIO_IDR(base), idr_read, idr_write, pio);
	IOH_New32(PIO_IMR(base), imr_read, imr_write, pio);
	IOH_New32(PIO_ISR(base), isr_read, isr_write, pio);
	IOH_New32(PIO_MDER(base), mder_read, mder_write, pio);
	IOH_New32(PIO_MDDR(base), mddr_read, mddr_write, pio);
	IOH_New32(PIO_MDSR(base), mdsr_read, mdsr_write, pio);
	IOH_New32(PIO_PUDR(base), pudr_read, pudr_write, pio);
	IOH_New32(PIO_PUER(base), puer_read, puer_write, pio);
	IOH_New32(PIO_PUSR(base), pusr_read, pusr_write, pio);
	IOH_New32(PIO_ASR(base), asr_read, asr_write, pio);
	IOH_New32(PIO_BSR(base), bsr_read, bsr_write, pio);
	IOH_New32(PIO_ABSR(base), absr_read, absr_write, pio);
	IOH_New32(PIO_OWER(base), ower_read, ower_write, pio);
	IOH_New32(PIO_OWDR(base), owdr_read, owdr_write, pio);
	IOH_New32(PIO_OWSR(base), owsr_read, owsr_write, pio);
}

static void
AT91Pio_UnMap(void *owner, uint32_t base, uint32_t mask)
{
	IOH_Delete32(PIO_PER(base));
	IOH_Delete32(PIO_PDR(base));
	IOH_Delete32(PIO_PSR(base));
	IOH_Delete32(PIO_OER(base));
	IOH_Delete32(PIO_ODR(base));
	IOH_Delete32(PIO_OSR(base));
	IOH_Delete32(PIO_IFER(base));
	IOH_Delete32(PIO_IFDR(base));
	IOH_Delete32(PIO_IFSR(base));
	IOH_Delete32(PIO_SODR(base));
	IOH_Delete32(PIO_CODR(base));
	IOH_Delete32(PIO_ODSR(base));
	IOH_Delete32(PIO_PDSR(base));
	IOH_Delete32(PIO_IER(base));
	IOH_Delete32(PIO_IDR(base));
	IOH_Delete32(PIO_IMR(base));
	IOH_Delete32(PIO_ISR(base));
	IOH_Delete32(PIO_MDER(base));
	IOH_Delete32(PIO_MDDR(base));
	IOH_Delete32(PIO_MDSR(base));
	IOH_Delete32(PIO_PUDR(base));
	IOH_Delete32(PIO_PUER(base));
	IOH_Delete32(PIO_PUSR(base));
	IOH_Delete32(PIO_ASR(base));
	IOH_Delete32(PIO_BSR(base));
	IOH_Delete32(PIO_ABSR(base));
	IOH_Delete32(PIO_OWER(base));
	IOH_Delete32(PIO_OWDR(base));
	IOH_Delete32(PIO_OWSR(base));
}

BusDevice *
AT91Pio_New(const char *name)
{
	int i;
	AT91Pio *pio = sg_new(AT91Pio);
	pio->name = sg_strdup(name);
	if (!pio->name) {
		fprintf(stderr, "Out of memory allocating AT91Pio name\n");
		exit(1);
	}
	pio->irqNode = SigNode_New("%s.irq", name);
	pio->gnd = SigNode_New("%s.gnd", name);
	if (!pio->irqNode || !pio->gnd) {
		fprintf(stderr, "AT91Pio: Can not create signal line\n");
		exit(1);
	}
	SigNode_Set(pio->gnd, SIG_LOW);
	for (i = 0; i < 32; i++) {
		pio->pad[i] = SigNode_New("%s.pad%d", name, i);
		pio->pullup[i] = SigNode_New("%s.pullup%d", name, i);
		pio->data[i] = SigNode_New("%s.data%d", name, i);
		pio->ds[i] = SigNode_New("%s.ds%d", name, i);
		pio->dout[i] = SigNode_New("%s.dout%d", name, i);
		pio->nosr[i] = SigNode_New("%s.nosr%d", name, i);
		pio->poe[i] = SigNode_New("%s.poe%d", name, i);
		pio->paoe[i] = SigNode_New("%s.paoe%d", name, i);
		pio->pboe[i] = SigNode_New("%s.pboe%d", name, i);
		pio->po[i] = SigNode_New("%s.po%d", name, i);
		pio->pao[i] = SigNode_New("%s.pao%d", name, i);
		pio->pbo[i] = SigNode_New("%s.pbo%d", name, i);
		pio->noea[i] = SigNode_New("%s.noea%d", name, i);
		pio->noeb[i] = SigNode_New("%s.noeb%d", name, i);
		if (!pio->pad[i] || !pio->pullup[i] || !pio->nosr[i]
		    || !pio->poe[i] || !pio->paoe[i] || !pio->pboe[i] || !pio->po[i]
		    || !pio->pao[i] || !pio->pbo[i] || !pio->noea[i] || !pio->noeb[i]
		    || !pio->data[i] || !pio->ds[i] || !pio->dout[i]) {
			fprintf(stderr, "AT91Pio: Can not create signal line\n");
			exit(1);
		}
	}
	for (i = 0; i < 32; i++) {
		OR_TraceInfo *ti = &pio->or_trace_info[i];
		ti->pio = pio;
		ti->index = i;
		SigNode_Trace(pio->noea[i], noe_update, ti);
		SigNode_Trace(pio->noeb[i], noe_update, ti);
	}
	for (i = 0; i < 32; i++) {
		Edge_TraceInfo *ti = &pio->edge_trace_info[i];
		ti->pio = pio;
		ti->index = i;
		SigNode_Trace(pio->pad[i], pdsr_update, ti);
	}
	SigNode_Set(pio->gnd, SIG_LOW);
	for (i = 0; i < 32; i++) {
		SigNode_Set(pio->nosr[i], SIG_HIGH);
		SigNode_Set(pio->data[i], SIG_LOW);
		SigNode_Set(pio->pullup[i], SIG_PULLUP);
	}
	SigNode_Set(pio->irqNode, SIG_PULLDOWN);

	/* 
	 * All registers have initial value 0 
	 * So call all disable register writes
	 */
	per_write(pio, 0xffffffff, 0, 4);
	pdr_write(pio, 0xffffffff, 0, 4);

	oer_write(pio, 0xffffffff, 0, 4);
	odr_write(pio, 0xffffffff, 0, 4);

	ifer_write(pio, 0xffffffff, 0, 4);
	ifdr_write(pio, 0xffffffff, 0, 4);

	sodr_write(pio, 0xffffffff, 0, 4);
	codr_write(pio, 0xffffffff, 0, 4);

	idr_write(pio, 0xffffffff, 0, 4);

	mder_write(pio, 0xffffffff, 0, 4);
	mddr_write(pio, 0xffffffff, 0, 4);

	puer_write(pio, 0xffffffff, 0, 4);
	pudr_write(pio, 0xffffffff, 0, 4);

	bsr_write(pio, 0xffffffff, 0, 4);
	asr_write(pio, 0xffffffff, 0, 4);

	owdr_write(pio, 0xffffffff, 0, 4);
	/* Lets have the Peripheral A and B random values */
#if 0
	for (i = 0; i < 32; i++) {
		if (lrand48() > 1 << 30) {
			SigNode_Set(pio->pao[i], SIG_LOW);
		} else {
			SigNode_Set(pio->pao[i], SIG_HIGH);
		}
	}
#endif
	pio->bdev.first_mapping = NULL;
	pio->bdev.Map = AT91Pio_Map;
	pio->bdev.UnMap = AT91Pio_UnMap;
	pio->bdev.owner = pio;
	pio->bdev.hw_flags = MEM_FLAG_WRITABLE | MEM_FLAG_READABLE;
	fprintf(stderr, "AT91 PIO module\"%s\" created\n", name);
	return &pio->bdev;
}
