/*
 *************************************************************************************************
 *
 * Emulation of the AT91 Watchdog 
 *
 * State: nothing is implemented 
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
#include <stdbool.h>
#include "clock.h"
#include "bus.h"
#include "at91_wdt.h"
#include "sglib.h"
#include "sgstring.h"
#include "cycletimer.h"
#include "sgtypes.h"
#include "signode.h"

#define WDT_CR(base) ((base) + 0x00)
#define WDT_MR(base) ((base) + 0x04)
#define		MR_WDIDLEHLT	(1 << 29)
#define		MR_WDDBGHLT		(1 << 28)
#define		MR_WDD_MSK	(0xfff << 16)
#define		MR_WDD_SHIFT	(16)
#define		MR_WDDIS		(1 << 15)
#define 	MR_WDRPR		(1 << 14)
#define 	MR_WDRSTEN		(1 << 13)
#define		MR_WDFIEN		(1 << 12)
#define		MR_WDV_MSK	(0xfff)
#define WDT_SR(base) ((base) + 0x08)
#define 	SR_WDUNF		(1 << 0)
#define 	SR_WDERR		(1 << 1)

typedef struct AT91Wdt {
	BusDevice bdev;
	Clock_t *clkSlckIn;
	CycleCounter_t lastActualized;
	CycleCounter_t accCycles;
	CycleTimer wdtTimer;
	uint32_t regCR;
	uint32_t regSR;
	uint32_t regMR;
	uint32_t wdtCounter;
	SigNode *sigIrq;
	bool mrWrittenOnce;
} AT91Wdt;

static void
actualize_counter(AT91Wdt *wdt)
{
	CycleCounter_t now;
	FractionU64_t frac;
	uint64_t wdt_cycles;
	now = CycleCounter_Get();
	CycleCounter_t cycles = now - wdt->lastActualized;
	wdt->accCycles += cycles;
	frac = Clock_MasterRatio(wdt->clkSlckIn);
	if(!frac.nom || !frac.denom) {
		return;
	}
	wdt_cycles = wdt->accCycles / (frac.denom / frac.nom); 
	wdt->accCycles = wdt->accCycles - wdt_cycles * (frac.denom / frac.nom);
	if(wdt_cycles >= wdt->wdtCounter) {
		if(wdt->wdtCounter != 0) {
			wdt->wdtCounter = 0;	
		}
	} else {
		wdt->wdtCounter -= wdt_cycles;
	}	
}

/**
 ***********************************************************************
 * \fn static void update_timeout(AT91Wdt *wdt)
 * Calculate the time until the watchdog reaches 0. 
 ***********************************************************************
 */
static void
update_timeout(AT91Wdt *wdt)
{
	uint64_t wdt_cycles;
        CycleCounter_t cycles;
        FractionU64_t frac;
        frac = Clock_MasterRatio(wdt->clkSlckIn);
        if(!frac.nom || !frac.denom) {
                fprintf(stderr,"Watchdog has No clock\n");
                return;
        }
        if(wdt->wdtCounter == 0) {
                return;
        }
        wdt_cycles = wdt->wdtCounter;
        cycles = wdt_cycles * (frac.denom / frac.nom);
        cycles -= wdt->accCycles;
        //fprintf(stderr,"Watchdog timeout in %llu, cycles %llu\n",wdt_cycles,cycles);
        CycleTimer_Mod(&wdt->wdtTimer,cycles);
}

static void
update_interrupt(AT91Wdt *wdt)
{
	if(wdt->regMR & MR_WDFIEN && (wdt->regSR & (SR_WDERR | SR_WDUNF))) {
		SigNode_Set(wdt->sigIrq,SIG_HIGH);
	} else {
		SigNode_Set(wdt->sigIrq,SIG_LOW);
	}
}

static void
wdt_timeout(void *eventData)
{
        AT91Wdt *wdt = eventData;
	if(wdt->regMR & MR_WDDIS) {
		return;
	}
        actualize_counter(wdt);
	wdt->regSR |= SR_WDUNF;
	update_interrupt(wdt);
	if(wdt->regMR & MR_WDRSTEN) {
		fflush(stderr);
		fflush(stdout);
		fprintf(stderr,"\nWatchdog timeout, Reset\n");
		fflush(stderr);
		exit(0);
	}
}

/**
 ******************************************************************************
 * Control register. Used to restart the watchdog it the key is matching
 * and the watchdog is in the allowed range.
 ******************************************************************************
 */
static uint32_t
cr_read(void *clientData,uint32_t address,int rqlen)
{
	fprintf(stderr,"WDT: %s is a writeonly register\n",__func__);
	return 0;
}

static void
cr_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	AT91Wdt *wdt = clientData;
	uint32_t wdd;
	if((value >> 24) != 0xa5) {
		fprintf(stderr,"Wrong key writing WDG control register\n");
		return;
	}
	actualize_counter(wdt);	
	wdd = (wdt->regMR >> 16)  & 0xfff;
	if(value & 1) {
		if((wdt->wdtCounter <= wdd)) {
			wdt->wdtCounter = wdt->regMR & 0xfff;
			update_timeout(wdt);
		} else {
			wdt->regSR |= SR_WDERR;
			update_interrupt(wdt);
		}
	}
}

/** 
 ******************************************************************************
 * \fn static uint32_t mr_read(void *clientData,uint32_t address,int rqlen)
 * Mode register. Configures the watchdog. This is a write once register.
 ******************************************************************************
 */
static uint32_t
mr_read(void *clientData,uint32_t address,int rqlen)
{
	AT91Wdt *wdt = clientData;
	return wdt->regMR;
}

static void
mr_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	AT91Wdt *wdt = clientData;
	if(wdt->mrWrittenOnce) {
		fprintf(stderr,"Watchdog mode register written more than once\n");
		return;
	}
	wdt->mrWrittenOnce = true;
	actualize_counter(wdt);
	wdt->regMR = value & 0x3FFFFFFF;
	update_timeout(wdt);
}

static uint32_t
sr_read(void *clientData,uint32_t address,int rqlen)
{
	AT91Wdt *wdt = clientData;
	actualize_counter(wdt);
	return wdt->regSR;
}

static void
sr_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	fprintf(stderr,"WDT: %s is readonly\n",__func__);
}

static void
AT91Wdt_Map(void *owner,uint32_t base,uint32_t mask,uint32_t flags)
{
	AT91Wdt *wdt = owner;
	IOH_New32(WDT_CR(base),cr_read,cr_write,wdt);
	IOH_New32(WDT_MR(base),mr_read,mr_write,wdt);
	IOH_New32(WDT_SR(base),sr_read,sr_write,wdt);
}

static void
AT91Wdt_UnMap(void *owner,uint32_t base,uint32_t mask)
{
	IOH_Delete32(WDT_CR(base));
	IOH_Delete32(WDT_MR(base));
	IOH_Delete32(WDT_SR(base));
}

BusDevice *
AT91Wdt_New(const char *name)
{
	AT91Wdt *wdt = sg_new(AT91Wdt);

	wdt->bdev.first_mapping = NULL;
	wdt->bdev.Map = AT91Wdt_Map;
	wdt->bdev.UnMap = AT91Wdt_UnMap;
	wdt->bdev.owner = wdt;
	wdt->bdev.hw_flags = MEM_FLAG_WRITABLE|MEM_FLAG_READABLE;
	wdt->clkSlckIn = Clock_New("%s.clk",name);
	wdt->mrWrittenOnce = false;
	wdt->sigIrq = SigNode_New("%s.irq",name);
	CycleTimer_Init(&wdt->wdtTimer,wdt_timeout,wdt);
	fprintf(stderr,"AT91 Watchdog timer \"%s\" created\n",name);
	return &wdt->bdev;
}

