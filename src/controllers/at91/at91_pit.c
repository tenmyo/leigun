/*
 ***********************************************************************************************
 *
 * Emulation of the AT91 Periodic Interval Timer (PIT)
 *
 *  State: not implemented 
 *
 * Copyright 2012 Jochen Karrer. All rights reserved.
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
#include <stdio.h>
#include <stdlib.h>
#include "signode.h"
#include "bus.h"
#include "sgstring.h"
#include "at91_pit.h"
#include "cycletimer.h"
#include "clock.h"
#include "senseless.h"

#define PIT_MR(base)	((base) + 0x00)
#define		MR_PITIEN	(1 << 25)
#define		MR_PITEN	(1 << 24)
#define PIT_SR(base)	((base) + 0x04)
#define		SR_PITS	(1 << 0)
#define PIT_PIVR(base)	((base) + 0x08)
#define PIT_PIIR(base)	((base) + 0x0c)

typedef struct AT91Pit {
	BusDevice bdev;
	CycleTimer eventTimer;
	uint32_t regMR;
	uint32_t regSR;
	uint32_t regPIVR;
	//uint32_t regPIIR;
	uint64_t lastActualizeCycles;
	CycleCounter_t accCycles;
	SigNode *sigIrq;
	Clock_t *clkIn;
	Clock_t *clkPit;
} AT91Pit;

#if 0
#define dbgprintf(x...) { fprintf(stderr,x); }
#else
#define dbgprintf(x...)
#endif

static void
update_interrupt(AT91Pit * pit)
{
	if ((pit->regSR & SR_PITS) && (pit->regMR & MR_PITIEN)) {
		dbgprintf("PIT irq\n");
		SigNode_Set(pit->sigIrq, SIG_HIGH);
	} else {
		dbgprintf("PIT unirq\n");
		SigNode_Set(pit->sigIrq, SIG_PULLDOWN);
	}
}

static void
actualize_counter(AT91Pit * pit)
{
	CycleCounter_t cycles = CycleCounter_Get();
	CycleCounter_t diffCycles;
	uint64_t cntrCycles;
	uint64_t pivValue;
	uint32_t pivPeriod;
	uint32_t picnt;
	FractionU64_t frac;
	frac = Clock_MasterRatio(pit->clkPit);
	if (!frac.nom || !frac.denom) {
		return;
	}
	diffCycles = cycles - pit->lastActualizeCycles;
	pit->lastActualizeCycles = cycles;
	pit->accCycles += diffCycles;
	cntrCycles = pit->accCycles / (frac.denom / frac.nom);
	//fprintf(stderr,"+ %lu, acc %lu, nom %llu, denom %llu\n",cntrCycles,pit->accCycles,frac.nom,frac.denom);
	pit->accCycles = pit->accCycles - cntrCycles * (frac.denom / frac.nom);
	pivPeriod = (pit->regMR & 0xfffff) + 1;
	pivValue = pit->regPIVR & 0xfffff;
	picnt = pit->regPIVR >> 20;
	pivValue = pivValue + cntrCycles;
//      fprintf(stderr,"pivValue %08llx period %08x, MR %08x\n",pivValue,pivPeriod, pit->regMR);
	if (pivValue >= pivPeriod) {
		if (pit->regMR & MR_PITEN) {
			picnt += pivValue / pivPeriod;
			pivValue = pivValue % pivPeriod;
		} else {
			pivValue = 0;
			picnt += 1;
		}
		pit->regSR |= SR_PITS;
		update_interrupt(pit);
	}
	pit->regPIVR = (picnt << 20) | (pivValue & 0xfffff);
}

/**
 ********************************************************************************************************
 *
 ********************************************************************************************************
 */
static void
update_timeout(AT91Pit * pit)
{
	FractionU64_t frac;
	uint64_t pivCycles;
	uint64_t pivPeriod;
	CycleCounter_t cycles;
	if ((pit->regMR & MR_PITIEN) == 0) {
		CycleTimer_Remove(&pit->eventTimer);
		return;
	}
	if (pit->regSR & SR_PITS) {
		//fprintf(stderr,"Already PITS\n");
		//return;
	}
	frac = Clock_MasterRatio(pit->clkPit);
	pivPeriod = (pit->regMR & 0xfffff) + 1;
	pivCycles = pivPeriod - (pit->regPIVR & 0xfffff);
	if (frac.nom) {
		cycles = pivCycles * (frac.denom / frac.nom);
	} else {
		fprintf(stderr, "nom is 0\n");
		cycles = 1000;
	}
	cycles -= pit->accCycles;
	//fprintf(stderr,"now %llu Timeout in %llu, pivPeriod %llu pivCycles %llu acc %llu, pivr %u\n",CycleCounter_Get(),cycles,pivPeriod,pivCycles,pit->accCycles,pit->regPIVR & 0xfffff);
	CycleTimer_Mod(&pit->eventTimer, cycles);
}

static void
timer_event(void *eventData)
{
	AT91Pit *pit = eventData;
	//fprintf(stderr,"Now Timer event %llu\n",CycleCounter_Get());
	actualize_counter(pit);
	update_timeout(eventData);
}

static uint32_t
mr_read(void *clientData, uint32_t address, int rqlen)
{
	AT91Pit *pit = clientData;
	return pit->regMR;
}

static void
mr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	AT91Pit *pit = clientData;
	actualize_counter(pit);
	//fprintf(stderr,"PIT MR: 0x%08x \n",value);
	pit->regMR = value & (0xfffff | MR_PITIEN | MR_PITEN);
	update_timeout(pit);
}

static uint32_t
sr_read(void *clientData, uint32_t address, int rqlen)
{
	AT91Pit *pit = clientData;
	actualize_counter(pit);
	return pit->regSR;
}

static void
sr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "PIT SR is readonly\n");
}

static uint32_t
pivr_read(void *clientData, uint32_t address, int rqlen)
{
	AT91Pit *pit = clientData;
	uint32_t pivr;
	actualize_counter(pit);
	pit->regSR &= ~SR_PITS;
	update_interrupt(pit);
	update_timeout(pit);
	Senseless_Report(150);
	dbgprintf("PIVR %u\n", pit->regPIVR);
	pivr = pit->regPIVR;
	pit->regPIVR &= 0x000fffff;
	return pivr;
}

static void
pivr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "PIT PIVR not implemented\n");
}

/**
 *******************************************************************************
 * PIIR is the same as PIVR but without modification on read
 *******************************************************************************
 */
static uint32_t
piir_read(void *clientData, uint32_t address, int rqlen)
{
	AT91Pit *pit = clientData;
	actualize_counter(pit);
	Senseless_Report(200);
	return pit->regPIVR;
}

static void
piir_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "PIT is readonly\n");
}

static void
AT91Pit_Map(void *owner, uint32_t base, uint32_t mask, uint32_t flags)
{
	AT91Pit *pit = (AT91Pit *) owner;
	IOH_New32(PIT_MR(base), mr_read, mr_write, pit);
	IOH_New32(PIT_SR(base), sr_read, sr_write, pit);
	IOH_New32(PIT_PIVR(base), pivr_read, pivr_write, pit);
	IOH_New32(PIT_PIIR(base), piir_read, piir_write, pit);
}

static void
AT91Pit_UnMap(void *owner, uint32_t base, uint32_t mask)
{
	IOH_Delete32(PIT_MR(base));
	IOH_Delete32(PIT_SR(base));
	IOH_Delete32(PIT_PIVR(base));
	IOH_Delete32(PIT_PIIR(base));
}

BusDevice *
AT91Pit_New(const char *name)
{
	AT91Pit *pit = sg_new(AT91Pit);
	pit->sigIrq = SigNode_New("%s.irq", name);
	if (!pit->sigIrq) {
		fprintf(stderr, "Can not create signal lines for AT91Pit\n");
		exit(1);
	}
	pit->bdev.first_mapping = NULL;
	pit->bdev.Map = AT91Pit_Map;
	pit->bdev.UnMap = AT91Pit_UnMap;
	pit->bdev.owner = pit;
	pit->bdev.hw_flags = MEM_FLAG_WRITABLE | MEM_FLAG_READABLE;
	pit->clkIn = Clock_New("%s.clk", name);
	pit->clkPit = Clock_New("%s.pit_clk", name);
	CycleTimer_Init(&pit->eventTimer, timer_event, pit);
	Clock_MakeDerived(pit->clkPit, pit->clkIn, 1, 16);
	update_interrupt(pit);
	fprintf(stderr, "AT91 PIT \"%s\" created\n", name);
	return &pit->bdev;
}
