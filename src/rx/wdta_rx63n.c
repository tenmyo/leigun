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
#include "wdta_rx63n.h"
#include "signode.h"

#if 0
#define dbgprintf(x...) { fprintf(stderr,x); }
#else
#define dbgprintf(x...)
#endif

#define REG_WDTRR(base)		((base) + 0x00)
#define REG_WDTCR(base)		((base) + 0x02)
#define 	WDTCR_TOPS_MSK		(0x3)
#define		WDTCR_RPES_MSK		(0x3 << 8)
#define		WDTCR_RPES_SHIFT	(8)
#define 	WDTCR_RPSS_MSK		(0x3 << 12)
#define 	WDTCR_RPSS_SHIFT	(12)
#define		WDTCR_CKS_MSK		(0xf << 4)
#define		WDTCR_CKS_SHIFT		(4)
#define REG_WDTSR(base)		((base) + 0x04)
#define		WDTSR_UNDFF		(1 << 14)
#define		WDTSR_REFEF		(1 << 15)
#define REG_WDTRCR(base)	((base) + 0x06)
#define		WDTRCR_RSTIRQS		(1 << 7)

typedef struct RxWDTA {
	BusDevice bdev;
	SigNode *sigIrq;
	CycleTimer eventTimer;
	CycleCounter_t lastUpdate;
	CycleCounter_t accCycles;
	Clock_t *clkIn;
	Clock_t *clkCntr;
	bool wdgRunning;
	uint8_t regWDTRR;
	uint16_t regWDTCR;
	uint16_t regWDTSR;
	uint8_t regWDTRCR;
} RxWDTA;

static void
update_interrupt(RxWDTA * wdta)
{
	if (wdta->regWDTSR & (WDTSR_UNDFF | WDTSR_REFEF)
	    && !(wdta->regWDTRCR & WDTRCR_RSTIRQS)) {
		SigNode_Set(wdta->sigIrq, SIG_LOW);
	} else {
		SigNode_Set(wdta->sigIrq, SIG_PULLUP);
	}
}

static void
wdg_timeout(RxWDTA * wdta)
{
	if ((wdta->regWDTRCR & WDTRCR_RSTIRQS)) {
		usleep(10000);
		fprintf(stderr, "Watchdog Reset\n");
		usleep(10000);
		exit(0);
	} else {
		fprintf(stderr, "Watchdog Interrupt\n");
	}
}

static void
actualize_counter(RxWDTA * wdta)
{
	CycleCounter_t now = CycleCounter_Get();
	FractionU64_t frac;
	CycleCounter_t cycles = now - wdta->lastUpdate;
	CycleCounter_t acc;
	uint64_t ctrCycles;
	uint16_t cntval;
	wdta->lastUpdate = now;
	if (!(wdta->wdgRunning)) {
		return;
	}
	acc = wdta->accCycles + cycles;
	frac = Clock_MasterRatio(wdta->clkCntr);
	if ((frac.nom == 0) || (frac.denom == 0)) {
		fprintf(stderr, "Bug: No clock for WDTA module\n");
		return;
	}
	ctrCycles = acc * frac.nom / frac.denom;
	acc -= ctrCycles * frac.denom / frac.nom;
	wdta->accCycles = acc;
	cntval = wdta->regWDTSR & 0x3fff;
	if ((ctrCycles > cntval)) {
		cntval = 0;
		wdta->regWDTSR |= WDTSR_UNDFF;
		wdta->regWDTSR = (wdta->regWDTSR & (WDTSR_REFEF | WDTSR_UNDFF)) | cntval;
		wdg_timeout(wdta);
		update_interrupt(wdta);
	} else {
		cntval = cntval - ctrCycles;
		wdta->regWDTSR = (wdta->regWDTSR & (WDTSR_REFEF | WDTSR_UNDFF)) | cntval;
	}
}

/*
 **********************************************************************************
 * \fn static void update_timeout(RxWDTA *wdta)
 * Recalculate the time of underflow and start a timer.
 **********************************************************************************
 */
static void
update_timeout(RxWDTA * wdta)
{
	uint16_t nto;
	FractionU64_t frac;
	CycleCounter_t cpu_cycles;
	if (!wdta->wdgRunning) {
		CycleTimer_Remove(&wdta->eventTimer);
		return;
	}
	nto = (wdta->regWDTSR & 0x3fff) + 1;
	frac = Clock_MasterRatio(wdta->clkCntr);
	if ((frac.nom == 0) || (frac.denom == 0)) {
		fprintf(stderr, "Bug: No clock for WDTA module %" PRIu64 "/%" PRIu64 "\n", frac.nom,
			frac.denom);
		return;
	}
	cpu_cycles = ((uint64_t) nto * frac.denom) / frac.nom;
	//fprintf(stderr,"timeout in %u tmr cycles, CPU %"PRIu64"\n",nto,cpu_cycles);
	CycleTimer_Mod(&wdta->eventTimer, cpu_cycles);
}

static void
timer_event(void *eventData)
{
	RxWDTA *wdta = eventData;
	actualize_counter(wdta);
	update_timeout(wdta);
}

static void
update_clock(RxWDTA * wdta)
{
	unsigned int cks = (wdta->regWDTCR & WDTCR_CKS_MSK) >> WDTCR_CKS_SHIFT;
	uint32_t div, mul = 1;
	switch (cks) {
	    case 0:
		    div = 4;
		    break;
	    case 4:
		    div = 64;
		    break;
	    case 0xf:
		    div = 128;
		    break;
	    case 6:
		    div = 512;
		    break;
	    case 7:
		    div = 2048;
		    break;
	    case 8:
		    div = 8192;
		    break;
	    default:
		    div = 1;
		    mul = 0;
		    fprintf(stderr, "Warning, prohibited clock setting %u for WDTA\n", cks);
		    break;
	}
	//fprintf(stderr,"MUL %u DIV %u\n",mul,div);
	//sleep(1);
	Clock_MakeDerived(wdta->clkCntr, wdta->clkIn, mul, div);
}

static bool
refresh_watchdog(RxWDTA * wdta)
{
	uint16_t tout;
	uint16_t wstartcnt;
	uint16_t wendcnt;
	uint16_t rpes, rpss;
	uint16_t cntval;
	//fprintf(stderr,"WDTCR %04x\n",wdta->regWDTCR);
	rpes = (wdta->regWDTCR & WDTCR_RPES_MSK) >> WDTCR_RPES_SHIFT;
	rpss = (wdta->regWDTCR & WDTCR_RPSS_MSK) >> WDTCR_RPSS_SHIFT;
	cntval = wdta->regWDTSR & 0x3fff;
	switch (wdta->regWDTCR & WDTCR_TOPS_MSK) {
	    case 0:
		    tout = 1023;
		    break;
	    case 1:
		    tout = 4095;
		    break;
	    case 2:
		    tout = 8191;
		    break;
	    case 3:
	    default:
		    tout = 16383;
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
		wdta->regWDTSR |= WDTSR_REFEF;
		dbgprintf("WDTA: Refresh outside of window, 0x%04x, start 0x%04x\n", cntval,
			  wstartcnt);
		wdg_timeout(wdta);
		update_interrupt(wdta);
		return false;
	}
	if ((wdta->wdgRunning == true) && (cntval < wendcnt)) {
		wdta->regWDTSR |= WDTSR_REFEF;
		dbgprintf("WDTA: Refresh outside of window, 0x%04x, end 0x%04x\n", cntval, wendcnt);
		dbgprintf("rpes %u rpss %u\n", rpes, rpss);
		wdg_timeout(wdta);
		update_interrupt(wdta);
		return false;
	}
	wdta->regWDTSR = (wdta->regWDTSR & ~0x3fff) | tout;
	return true;
}

static uint32_t
wdtrr_read(void *clientData, uint32_t address, int rqlen)
{
	RxWDTA *wdta = clientData;
	return wdta->regWDTRR;
}

static void
wdtrr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	RxWDTA *wdta = clientData;
	if ((value == 0xff) && (wdta->regWDTRR == 0)) {
		actualize_counter(wdta);
		if (refresh_watchdog(wdta) == true) {
			wdta->wdgRunning = true;
		}
		update_timeout(wdta);
	}
	if (value == 0) {
		wdta->regWDTRR = 0;
	} else {
		wdta->regWDTRR = 0xff;
	}
}

static uint32_t
wdtcr_read(void *clientData, uint32_t address, int rqlen)
{
	RxWDTA *wdta = clientData;
	return wdta->regWDTCR;
}

static void
wdtcr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	RxWDTA *wdta = clientData;
	wdta->regWDTCR = value & 0x33f3;
	actualize_counter(wdta);
	update_clock(wdta);
	update_timeout(wdta);
}

static uint32_t
wdtsr_read(void *clientData, uint32_t address, int rqlen)
{
	RxWDTA *wdta = clientData;
	actualize_counter(wdta);
	return wdta->regWDTSR;
}

static void
wdtsr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	RxWDTA *wdta = clientData;
	//actualize_counter(wdta);
	wdta->regWDTSR = wdta->regWDTSR & ((value & 0xc000) | 0x3fff);
	update_interrupt(wdta);
}

static uint32_t
wdtrcr_read(void *clientData, uint32_t address, int rqlen)
{
	RxWDTA *wdta = clientData;
	return wdta->regWDTRCR;
}

static void
wdtrcr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	RxWDTA *wdta = clientData;
	wdta->regWDTRCR = value & WDTRCR_RSTIRQS;
}

static void
Wdta_Map(void *owner, uint32_t base, uint32_t mask, uint32_t mapflags)
{
	RxWDTA *wdta = owner;
	IOH_New8(REG_WDTRR(base), wdtrr_read, wdtrr_write, wdta);
	IOH_New16(REG_WDTCR(base), wdtcr_read, wdtcr_write, wdta);
	IOH_New16(REG_WDTSR(base), wdtsr_read, wdtsr_write, wdta);
	IOH_New8(REG_WDTRCR(base), wdtrcr_read, wdtrcr_write, wdta);
}

static void
Wdta_Unmap(void *owner, uint32_t base, uint32_t mask)
{
	IOH_Delete8(REG_WDTRR(base));
	IOH_Delete16(REG_WDTCR(base));
	IOH_Delete16(REG_WDTSR(base));
	IOH_Delete8(REG_WDTRCR(base));
}

BusDevice *
RX63NWDTA_New(const char *name)
{
	RxWDTA *wdta = sg_new(RxWDTA);
	wdta->bdev.first_mapping = NULL;
	wdta->bdev.Map = Wdta_Map;
	wdta->bdev.UnMap = Wdta_Unmap;
	wdta->bdev.owner = wdta;
	wdta->bdev.hw_flags = MEM_FLAG_WRITABLE | MEM_FLAG_READABLE;
	wdta->sigIrq = SigNode_New("%s.irq", name);
	wdta->clkIn = Clock_New("%s.clk", name);
	wdta->clkCntr = Clock_New("%s.clkCntr", name);
	wdta->regWDTRR = 0xff;
	wdta->regWDTRCR = WDTRCR_RSTIRQS;
	wdta->regWDTCR = 0x33f3;
	update_clock(wdta);
	update_interrupt(wdta);
	CycleTimer_Init(&wdta->eventTimer, timer_event, wdta);
	return &wdta->bdev;
}
