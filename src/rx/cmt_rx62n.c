/*
 **********************************************************************************************
 * Renesas RX62N Compare Match Timer (CMT) module  
 *
 * State: working 
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
#include <inttypes.h>
#include "bus.h"
#include "sglib.h"
#include "sgstring.h"
#include "cmt_rx62n.h"
#include "clock.h"
#include "cycletimer.h"
#include "signode.h"

#define REG_CMSTR(base) ((base) + 0)
#define 	CMSTR_STR0
#define		CMSTR_STR1
#define REG_CMCR(base)	((base) + 0)
#define		CMCR_CKS_MSK	(3)
#define		CMCR_CKS_SHIFT	(0)
#define		CMCR_CMIE	(1 << 6)
#define	REG_CMCNT(base)	((base) + 2)
#define	REG_CMCOR(base)	((base) + 4)

typedef struct CMT CMT;
typedef struct CMTMod CMTMod;

struct CMT {
	CMTMod *cmtmod;
	uint16_t cmtstrSTR;
	uint16_t regCMCR;
	uint16_t regCMCNT;
	uint16_t regCMCOR;
	Clock_t *clkIn;
	Clock_t *clkCntr;
	SigNode *sigIrq;
	CycleCounter_t lastUpdate;
	CycleCounter_t accCycles;
	CycleTimer eventTimer;
};

struct CMTMod {
	BusDevice bdev;
	uint8_t regCMSTR;
	CMT *cmt[2];
};

static void
actualize_counter(CMT * cmt)
{
	CycleCounter_t now = CycleCounter_Get();
	FractionU64_t frac;
	CycleCounter_t cycles = now - cmt->lastUpdate;
	CycleCounter_t acc;
	uint64_t ctrCycles;
	uint16_t nto;
	cmt->lastUpdate = now;
	if (!cmt->cmtstrSTR) {
		return;
	}
	acc = cmt->accCycles + cycles;
	frac = Clock_MasterRatio(cmt->clkCntr);
	if ((frac.nom == 0) || (frac.denom == 0)) {
		fprintf(stderr, "Warning, No clock for timer module\n");
		return;
	}
	ctrCycles = acc * frac.nom / frac.denom;
	acc -= ctrCycles * frac.denom / frac.nom;
	cmt->accCycles = acc;
	/* Now calculate the time to the next timeout */
	nto = cmt->regCMCOR - cmt->regCMCNT;
	//fprintf(stderr,"Ctr cycles %llu, cpu cycles %llu new ack %llu, nto %u\n",ctrCycles,cycles,acc,nto);
	if (cmt->regCMCOR) {
		cmt->regCMCNT = (ctrCycles + cmt->regCMCNT) % cmt->regCMCOR;
	}
	if (ctrCycles >= nto) {
		if (cmt->regCMCR & CMCR_CMIE) {
			SigNode_Set(cmt->sigIrq, SIG_LOW);
			SigNode_Set(cmt->sigIrq, SIG_HIGH);
		}
	}
}

static void
update_timeout(CMT * cmt)
{				/* next Timeout: Intentional wrap */
	FractionU64_t frac;
	uint16_t nto;
	CycleCounter_t cpu_cycles;
	if (!cmt->cmtstrSTR) {
		CycleTimer_Remove(&cmt->eventTimer);
		return;
	}
	nto = cmt->regCMCOR - cmt->regCMCNT;
	frac = Clock_MasterRatio(cmt->clkCntr);
	if ((frac.nom == 0) || (frac.denom == 0)) {
		fprintf(stderr, "Warning, No clock for timer module\n");
		return;
	}
	//cpu_cycles = ((uint64_t)nto * frac.denom + (frac.nom - 1)) / frac.nom;
	//fprintf(stderr,"cpu_cycles %llu\n",cpu_cycles);
	cpu_cycles = ((uint64_t) nto * frac.denom) / frac.nom;
#if 0
	fprintf(stderr, "cpu_cycles %llu, %llu / %llu, nto %u CMCOR %u CMCNT %u\n",
		cpu_cycles, frac.nom, frac.denom, nto, cmt->regCMCOR, cmt->regCMCNT);
#endif
	if (cpu_cycles >= cmt->accCycles) {
		cpu_cycles -= cmt->accCycles;
	} else if (cpu_cycles == 0) {
		return;
	} else {
		fprintf(stderr, "shouldn't happen cy %" PRIu64 " ac %" PRIu64 "\n", cpu_cycles,
			cmt->accCycles);
		cpu_cycles = 0;
	}
	if (cmt->regCMCR & CMCR_CMIE) {
		CycleTimer_Mod(&cmt->eventTimer, cpu_cycles);
	} else {
		CycleTimer_Remove(&cmt->eventTimer);
	}
}

static uint32_t
cmstr_read(void *clientData, uint32_t address, int rqlen)
{
	CMTMod *cmtmod = clientData;
	return cmtmod->regCMSTR;
}

static void
cmstr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	CMTMod *cmtmod = clientData;
	uint16_t diff = cmtmod->regCMSTR ^ value;
	if (diff & 1) {
		actualize_counter(cmtmod->cmt[0]);
	}
	if (diff & 2) {
		actualize_counter(cmtmod->cmt[1]);
	}
	actualize_counter(cmtmod->cmt[1]);
	cmtmod->regCMSTR = value & 3;
	cmtmod->cmt[0]->cmtstrSTR = value & 1;
	cmtmod->cmt[1]->cmtstrSTR = ! !(value & 2);
	if (diff & 1) {
		update_timeout(cmtmod->cmt[0]);
	}
	if (diff & 2) {
		update_timeout(cmtmod->cmt[1]);
	}
}

static uint32_t
cmcr_read(void *clientData, uint32_t address, int rqlen)
{
	CMT *cmt = clientData;
	return cmt->regCMCR;
}

static void
cmcr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	CMT *cmt = clientData;
	uint32_t cks = value & 3;
	uint32_t div;
	actualize_counter(cmt);
	cmt->regCMCR = value;
	div = 8 << (2 * cks);
	Clock_MakeDerived(cmt->clkCntr, cmt->clkIn, 1, div);
	update_timeout(cmt);
}

static uint32_t
cmcnt_read(void *clientData, uint32_t address, int rqlen)
{
	CMT *cmt = clientData;
	actualize_counter(cmt);
	return cmt->regCMCNT;
}

static void
cmcnt_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	CMT *cmt = clientData;
	actualize_counter(cmt);
	cmt->regCMCNT = value;
	update_timeout(cmt);
}

static uint32_t
cmcor_read(void *clientData, uint32_t address, int rqlen)
{
	CMT *cmt = clientData;
	return cmt->regCMCOR;
}

static void
cmcor_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	CMT *cmt = clientData;
	actualize_counter(cmt);
	cmt->regCMCOR = value;
	update_timeout(cmt);
}

static void
CMT_Unmap(void *owner, uint32_t base, uint32_t mask)
{
	IOH_Delete16(REG_CMCR(base));
	IOH_Delete16(REG_CMCNT(base));
	IOH_Delete16(REG_CMCOR(base));
}

static void
CMT_Map(void *owner, uint32_t base, uint32_t mask, uint32_t mapflags)
{
	CMT *cmt = owner;
	IOH_New16(REG_CMCR(base), cmcr_read, cmcr_write, cmt);
	IOH_New16(REG_CMCNT(base), cmcnt_read, cmcnt_write, cmt);
	IOH_New16(REG_CMCOR(base), cmcor_read, cmcor_write, cmt);
}

static void
CMTMod_Unmap(void *owner, uint32_t base, uint32_t mask)
{
	int i;
	CMTMod *cmtmod = owner;
	IOH_Delete16(REG_CMSTR(base));
	for (i = 0; i < 2; i++) {
		CMT_Unmap(cmtmod->cmt[i], base + 2 + i * 6, mask);
	}
}

static void
CMTMod_Map(void *owner, uint32_t base, uint32_t mask, uint32_t mapflags)
{
	int i;
	CMTMod *cmtmod = owner;
	IOH_New16(REG_CMSTR(base), cmstr_read, cmstr_write, cmtmod);
	for (i = 0; i < 2; i++) {
		CMT_Map(cmtmod->cmt[i], base + 2 + i * 6, mask, mapflags);
	}
}

static void
timer_event(void *eventData)
{
	CMT *cmt = eventData;
	actualize_counter(cmt);
	if (cmt->regCMCNT != 0) {
		static int cntr = 0;
		cntr++;
		//if(cntr < 20000) {
		fprintf(stderr, "Cnt not 0 after event %u, acc %" PRIu64 "\n",
			cmt->regCMCNT, cmt->accCycles);
		//}
		//exit(1);
	}
	update_timeout(cmt);
}

static CMT *
CMT_New(const char *name)
{
	CMT *cmt = sg_new(CMT);
	cmt->clkIn = Clock_New("%s.clk", name);
	cmt->clkCntr = Clock_New("%s.clkCntr", name);
	Clock_MakeDerived(cmt->clkCntr, cmt->clkIn, 1, 8);
	/* Only until clocktree exists */
	cmt->sigIrq = SigNode_New("%s.irq", name);
	CycleTimer_Init(&cmt->eventTimer, timer_event, cmt);
	return cmt;
}

BusDevice *
CMTMod_New(const char *name, uint32_t base_nr)
{
	CMTMod *cmtmod = sg_new(CMTMod);
	int i;
	char *cmtname = alloca(strlen(name) + 10);
	for (i = 0; i < 2; i++) {
		sprintf(cmtname, "%s%d", name, i + base_nr);
		cmtmod->cmt[i] = CMT_New(cmtname);
		cmtmod->cmt[i]->cmtmod = cmtmod;
	}
	cmtmod->bdev.first_mapping = NULL;
	cmtmod->bdev.Map = CMTMod_Map;
	cmtmod->bdev.UnMap = CMTMod_Unmap;
	cmtmod->bdev.owner = cmtmod;
	cmtmod->bdev.hw_flags = MEM_FLAG_WRITABLE | MEM_FLAG_READABLE;
	return &cmtmod->bdev;
}
