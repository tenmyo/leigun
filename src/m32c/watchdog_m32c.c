/*
 ***************************************************************************************************** 
 * M32C Watchdog simulation.
 * 
 * State: implemented, untested
 *
 * Copyright 2010 Jochen Karrer. All rights reserved.
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
 ***************************************************************************************************** 
 */
#include <bus.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>
#include "sgstring.h"
#include "clock.h"
#include "cycletimer.h"
#include "signode.h"
#include "watchdog_m32c.h"

#define REG_WDTS	(0x0e)
#define REG_WDC		(0xf)
#define		WDC_WDC7	(1 << 7)
#define		WDC_WDC5	(1 << 5)

typedef struct Watchdog {
	CycleTimer wdtTimer;
	CycleCounter_t accCycles;
	CycleCounter_t lastActualized;
	BusDevice bdev;
	Clock_t *clkIn;
	Clock_t *clkWdt;
	uint32_t wdtCounter;
	uint8_t regWDC;
} Watchdog;


static void
actualize_counter(Watchdog *wdt) 
{
	CycleCounter_t cycles;
	CycleCounter_t now;
	FractionU64_t frac;
	uint64_t wdt_cycles;
	uint64_t div;

	now = CycleCounter_Get();
	cycles = now - wdt->lastActualized;
	wdt->lastActualized = now;
	wdt->accCycles += cycles;
	frac = Clock_MasterRatio(wdt->clkWdt);
	if(!frac.nom || !frac.denom) {
		fprintf(stderr,"No clock\n");
		return;
	}
	div  = frac.denom / frac.nom;
	if(div == 0) {
		div = 1;
	}
	wdt_cycles = wdt->accCycles / div;
	wdt->accCycles = wdt->accCycles - wdt_cycles * div;
	if(wdt_cycles >= wdt->wdtCounter) {
		if(wdt->wdtCounter != 0) {
			// Trigger the event here
			wdt->wdtCounter = 0;
		}
	} else {
		wdt->wdtCounter -= wdt_cycles;
	}
	//fprintf(stderr,"Updated counter to %u\n",wdt->wdtCounter);
}

static void
update_timeout(Watchdog *wdt)
{
	uint64_t wdt_cycles;
	CycleCounter_t cycles;
	FractionU64_t frac;
        frac = Clock_MasterRatio(wdt->clkWdt);
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
wdt_timeout(void *eventData) 
{
	Watchdog *wdt = eventData;
	actualize_counter(wdt);
	fflush(stderr);
	fflush(stdout);
	fprintf(stderr,"\nWatchdog timeout, Reset\n");
	fflush(stderr);
	exit(0);
}

static uint32_t
wdts_read(void *clientData,uint32_t address,int rqlen)
{
        return 0;
}

static void
wdts_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	Watchdog *wdt = clientData;
	actualize_counter(wdt);
	wdt->wdtCounter = 0x7fff;
	update_timeout(wdt);
}

static void
update_wdt_clock(Watchdog *wdt) 
{
	uint32_t divider;
	if(wdt->regWDC & WDC_WDC7) {
		divider = 128;
	} else {
		divider = 16;
	}
	Clock_MakeDerived(wdt->clkWdt,wdt->clkIn,1,divider);
}

static uint32_t
wdc_read(void *clientData,uint32_t address,int rqlen)
{
	Watchdog *wdt = clientData;
	actualize_counter(wdt);
	return (wdt->regWDC & 0xe0) | ((wdt->wdtCounter & 0x7c00) >> 10);
}

static void
wdc_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	Watchdog *wdt = clientData;
	actualize_counter(wdt);
	wdt->regWDC = value & 0xe0;
	wdt->regWDC |= WDC_WDC5;
	update_wdt_clock(wdt);
	update_timeout(wdt);
}

static void
M32CWatchdog_Unmap(void *owner,uint32_t base,uint32_t mask)
{
        IOH_Delete16(REG_WDTS);
}

static void
M32CWatchdog_Map(void *owner,uint32_t base,uint32_t mask,uint32_t mapflags)
{
	Watchdog *wdt = owner;
	uint32_t flags = IOH_FLG_HOST_ENDIAN | IOH_FLG_OSZR_NEXT | IOH_FLG_OSZW_NEXT;
	IOH_New8f(REG_WDTS,wdts_read,wdts_write,wdt,flags);
	IOH_New8f(REG_WDC,wdc_read,wdc_write,wdt,flags);
}

BusDevice *
M32C_WatchdogNew(const char *name)
{
	Watchdog *wdt = sg_new(Watchdog);
	wdt->bdev.first_mapping=NULL;
	wdt->bdev.Map=M32CWatchdog_Map;
	wdt->bdev.UnMap=M32CWatchdog_Unmap;
	wdt->bdev.owner=wdt;
	wdt->bdev.hw_flags=MEM_FLAG_WRITABLE|MEM_FLAG_READABLE;
	wdt->clkIn = Clock_New("%s.clk",name);
	wdt->clkWdt = Clock_New("%s.clkWdt",name);
	CycleTimer_Init(&wdt->wdtTimer,wdt_timeout,wdt);
	wdt->wdtCounter = 0;
	update_wdt_clock(wdt);
	return &wdt->bdev;
}
