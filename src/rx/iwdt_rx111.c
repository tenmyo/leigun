/*
 **********************************************************************************************
 * Renesas RX62N/RX63N Ethernet controller with Ethernet DMA controller 
 *
 * State: working 
 *
 * Copyright 2013 Jochen Karrer. All rights reserved.
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
#include <stdlib.h>
#include <unistd.h>
#include "bus.h"
#include "sglib.h"
#include "sgstring.h"
#include "cycletimer.h"
#include "clock.h"
#include "iwdt_rx111.h"
#include "signode.h"

#if 0
#define dbgprintf(...) { fprintf(stderr,__VA_ARGS__); }
#else
#define dbgprintf(...)
#endif

#define REG_IWDTRR(base)		((base) + 0x00)
#define REG_IWDTCR(base)		((base) + 0x02)
#define 	IWDTCR_TOPS_MSK		(0x3)
#define		IWDTCR_RPES_MSK		(0x3 << 8)
#define		IWDTCR_RPES_SHIFT	(8)
#define 	IWDTCR_RPSS_MSK		(0x3 << 12)
#define 	IWDTCR_RPSS_SHIFT	(12)
#define		IWDTCR_CKS_MSK		(0xf << 4)
#define		IWDTCR_CKS_SHIFT		(4)
#define REG_IWDTSR(base)		((base) + 0x04)
#define		IWDTSR_UNDFF		(1 << 14)
#define		IWDTSR_REFEF		(1 << 15)
#define REG_IWDTRCR(base)	((base) + 0x06)
#define		IWDTRCR_RSTIRQS		(1 << 7)

typedef struct RxIWDTA {
	BusDevice bdev;
	SigNode *sigIrq;
	CycleTimer eventTimer;
	CycleCounter_t lastUpdate;
	CycleCounter_t accCycles;
	Clock_t *clkIn;
	Clock_t *clkCntr;
	bool wdgRunning;
	uint8_t regIWDTRR;
	uint16_t regIWDTCR;
	uint16_t regIWDTSR;
	uint8_t regIWDTRCR;
} RxIWDTA;

static void
update_interrupt(RxIWDTA * iwdta)
{
	if (iwdta->regIWDTSR & (IWDTSR_UNDFF | IWDTSR_REFEF)
	    && !(iwdta->regIWDTRCR & IWDTRCR_RSTIRQS)) {
		SigNode_Set(iwdta->sigIrq, SIG_LOW);
	} else {
		SigNode_Set(iwdta->sigIrq, SIG_PULLUP);
	}
}

static void
wdg_timeout(RxIWDTA * iwdta)
{
	if ((iwdta->regIWDTRCR & IWDTRCR_RSTIRQS)) {
		usleep(10000);
		fprintf(stderr, "Watchdog Reset\n");
		usleep(10000);
		exit(0);
	} else {
		fprintf(stderr, "Watchdog Interrupt\n");
	}
}

static void
actualize_counter(RxIWDTA * iwdta)
{
	CycleCounter_t now = CycleCounter_Get();
	FractionU64_t frac;
	CycleCounter_t cycles = now - iwdta->lastUpdate;
	CycleCounter_t acc;
	uint64_t ctrCycles;
	uint16_t cntval;
	iwdta->lastUpdate = now;
	if (!(iwdta->wdgRunning)) {
		return;
	}
	acc = iwdta->accCycles + cycles;
	frac = Clock_MasterRatio(iwdta->clkCntr);
	if ((frac.nom == 0) || (frac.denom == 0)) {
		fprintf(stderr, "Bug: No clock for IWDTA module\n");
		return;
	}
	ctrCycles = acc * frac.nom / frac.denom;
	acc -= ctrCycles * frac.denom / frac.nom;
	iwdta->accCycles = acc;
	cntval = iwdta->regIWDTSR & 0x3fff;
	if ((ctrCycles > cntval)) {
		cntval = 0;
		iwdta->regIWDTSR |= IWDTSR_UNDFF;
		iwdta->regIWDTSR = (iwdta->regIWDTSR & (IWDTSR_REFEF | IWDTSR_UNDFF)) | cntval;
		wdg_timeout(iwdta);
		update_interrupt(iwdta);
	} else {
		cntval = cntval - ctrCycles;
		iwdta->regIWDTSR = (iwdta->regIWDTSR & (IWDTSR_REFEF | IWDTSR_UNDFF)) | cntval;
	}
}

/*
 **********************************************************************************
 * \fn static void update_timeout(RxIWDTA *iwdta)
 * Recalculate the time of underflow and start a timer.
 **********************************************************************************
 */
static void
update_timeout(RxIWDTA * iwdta)
{
	uint16_t nto;
	FractionU64_t frac;
	CycleCounter_t cpu_cycles;
	if (!iwdta->wdgRunning) {
		CycleTimer_Remove(&iwdta->eventTimer);
		return;
	}
	nto = (iwdta->regIWDTSR & 0x3fff) + 1;
	frac = Clock_MasterRatio(iwdta->clkCntr);
	if ((frac.nom == 0) || (frac.denom == 0)) {
		fprintf(stderr, "Bug: No clock for IWDTA module %" PRIu64 "/%" PRIu64 "\n", frac.nom,
			frac.denom);
		return;
	}
	cpu_cycles = ((uint64_t) nto * frac.denom) / frac.nom;
	//fprintf(stderr,"timeout in %u tmr cycles, CPU %"PRIu64"\n",nto,cpu_cycles);
	CycleTimer_Mod(&iwdta->eventTimer, cpu_cycles);
}

static void
timer_event(void *eventData)
{
	RxIWDTA *iwdta = eventData;
	actualize_counter(iwdta);
	update_timeout(iwdta);
}

static void
update_clock(RxIWDTA * iwdta)
{
	unsigned int cks = (iwdta->regIWDTCR & IWDTCR_CKS_MSK) >> IWDTCR_CKS_SHIFT;
	uint32_t div, mul = 1;
	switch (cks) {
	    case 0:
		    div = 1;
		    break;
	    case 2:
		    div = 16;
		    break;
	    case 3:
		    div = 32;
		    break;
	    case 4:
		    div = 64;
		    break;
	    case 5:
		    div = 256;
		    break;
	    case 7:
		    div = 128;
		    break;
	    default:
		    div = 1;
		    mul = 0;
		    fprintf(stderr, "Warning, prohibited clock setting %u for IWDTA\n", cks);
		    break;
	}
	//fprintf(stderr,"MUL %u DIV %u\n",mul,div);
	//sleep(1);
	Clock_MakeDerived(iwdta->clkCntr, iwdta->clkIn, mul, div);
}

static bool
refresh_watchdog(RxIWDTA * iwdta)
{
	uint16_t tout;
	uint16_t wstartcnt;
	uint16_t wendcnt;
	uint16_t rpes, rpss;
	uint16_t cntval;
	//fprintf(stderr,"IWDTCR %04x\n",iwdta->regIWDTCR);
	rpes = (iwdta->regIWDTCR & IWDTCR_RPES_MSK) >> IWDTCR_RPES_SHIFT;
	rpss = (iwdta->regIWDTCR & IWDTCR_RPSS_MSK) >> IWDTCR_RPSS_SHIFT;
	cntval = iwdta->regIWDTSR & 0x3fff;
	switch (iwdta->regIWDTCR & IWDTCR_TOPS_MSK) {
	    case 0:
		    tout = 128;
		    break;
	    case 1:
		    tout = 512;
		    break;
	    case 2:
		    tout = 1024;
		    break;
	    case 3:
	    default:
		    tout = 2048;
		    break;
	}
	switch (rpss) {
	    case 0:
		    wstartcnt = tout * 1 / 4;
		    break;
	    case 1:
		    wstartcnt = tout * 2 / 4;
		    break;
	    case 2:
		    wstartcnt = tout * 3 / 4;
		    break;
	    case 3:
		    wstartcnt = tout;
		    break;
	}
	switch (rpes) {
	    case 0:
		    wendcnt = tout * 3 / 4;
		    break;
	    case 1:
		    wendcnt = tout * 2 / 4;
		    break;
	    case 2:
		    wendcnt = tout * 1 / 4;
		    break;
	    case 3:
		    wendcnt = 0;
		    break;
	}
	if ((cntval > wstartcnt) && (cntval != tout)) {
		iwdta->regIWDTSR |= IWDTSR_REFEF;
		dbgprintf("IWDTA: Refresh outside of window, 0x%04x, start 0x%04x\n", cntval,
			  wstartcnt);
		wdg_timeout(iwdta);
		update_interrupt(iwdta);
		return false;
	}
	if ((iwdta->wdgRunning == true) && (cntval < wendcnt)) {
		iwdta->regIWDTSR |= IWDTSR_REFEF;
		dbgprintf("IWDTA: Refresh outside of window, 0x%04x, end 0x%04x\n", cntval, wendcnt);
		dbgprintf("rpes %u rpss %u\n", rpes, rpss);
		wdg_timeout(iwdta);
		update_interrupt(iwdta);
		return false;
	}
	iwdta->regIWDTSR = (iwdta->regIWDTSR & ~0x3fff) | tout;
	return true;
}

static uint32_t
iwdtrr_read(void *clientData, uint32_t address, int rqlen)
{
	RxIWDTA *iwdta = clientData;
	return iwdta->regIWDTRR;
}

static void
iwdtrr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	RxIWDTA *iwdta = clientData;
	if ((value == 0xff) && (iwdta->regIWDTRR == 0)) {
		actualize_counter(iwdta);
		if (refresh_watchdog(iwdta) == true) {
			iwdta->wdgRunning = true;
		}
		update_timeout(iwdta);
	}
	if (value == 0) {
		iwdta->regIWDTRR = 0;
	} else {
		iwdta->regIWDTRR = 0xff;
	}
}

static uint32_t
iwdtcr_read(void *clientData, uint32_t address, int rqlen)
{
	RxIWDTA *iwdta = clientData;
	return iwdta->regIWDTCR;
}

static void
iwdtcr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	RxIWDTA *iwdta = clientData;
	iwdta->regIWDTCR = value & 0x33f3;
	actualize_counter(iwdta);
	update_clock(iwdta);
	update_timeout(iwdta);
}

static uint32_t
iwdtsr_read(void *clientData, uint32_t address, int rqlen)
{
	RxIWDTA *iwdta = clientData;
	actualize_counter(iwdta);
	return iwdta->regIWDTSR;
}

static void
iwdtsr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	RxIWDTA *iwdta = clientData;
	//actualize_counter(iwdta);
	iwdta->regIWDTSR = iwdta->regIWDTSR & ((value & 0xc000) | 0x3fff);
	update_interrupt(iwdta);
}

static uint32_t
iwdtrcr_read(void *clientData, uint32_t address, int rqlen)
{
	RxIWDTA *iwdta = clientData;
	return iwdta->regIWDTRCR;
}

static void
iwdtrcr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	RxIWDTA *iwdta = clientData;
	iwdta->regIWDTRCR = value & IWDTRCR_RSTIRQS;
}

static void
Wdta_Map(void *owner, uint32_t base, uint32_t mask, uint32_t mapflags)
{
	RxIWDTA *iwdta = owner;
	IOH_New8(REG_IWDTRR(base), iwdtrr_read, iwdtrr_write, iwdta);
	IOH_New16(REG_IWDTCR(base), iwdtcr_read, iwdtcr_write, iwdta);
	IOH_New16(REG_IWDTSR(base), iwdtsr_read, iwdtsr_write, iwdta);
	IOH_New8(REG_IWDTRCR(base), iwdtrcr_read, iwdtrcr_write, iwdta);
}

static void
Wdta_Unmap(void *owner, uint32_t base, uint32_t mask)
{
	IOH_Delete8(REG_IWDTRR(base));
	IOH_Delete16(REG_IWDTCR(base));
	IOH_Delete16(REG_IWDTSR(base));
	IOH_Delete8(REG_IWDTRCR(base));
}

BusDevice *
RX111Iwdt_New(const char *name)
{
	RxIWDTA *iwdta = sg_new(RxIWDTA);
	iwdta->bdev.first_mapping = NULL;
	iwdta->bdev.Map = Wdta_Map;
	iwdta->bdev.UnMap = Wdta_Unmap;
	iwdta->bdev.owner = iwdta;
	iwdta->bdev.hw_flags = MEM_FLAG_WRITABLE | MEM_FLAG_READABLE;
	iwdta->sigIrq = SigNode_New("%s.irq", name);
	iwdta->clkIn = Clock_New("%s.clk", name);
	iwdta->clkCntr = Clock_New("%s.clkCntr", name);
	iwdta->regIWDTRR = 0xff;
	iwdta->regIWDTRCR = IWDTRCR_RSTIRQS;
	iwdta->regIWDTCR = 0x33f3;
	update_clock(iwdta);
	update_interrupt(iwdta);
	CycleTimer_Init(&iwdta->eventTimer, timer_event, iwdta);
	return &iwdta->bdev;
}
