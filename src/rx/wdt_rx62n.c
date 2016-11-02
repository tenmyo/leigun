#include <stdint.h>
#include <inttypes.h>
#include <stdlib.h>
#include <unistd.h>
#include "bus.h"
#include "sglib.h"
#include "sgstring.h"
#include "cycletimer.h"
#include "clock.h"
#include "wdt_rx62n.h"
#include "cpu_rx.h"

#define	REG_TCSR(base)		((base) + 0x00)
#define		TCSR_CKS_MSK	(0x7)
#define		TCSR_TME	(1 << 5)
#define		TCSR_TMS	(1 << 6)
#define	REG_TCNT(base)		((base) + 0x01)
#define	REG_RSTCSR(base)	((base) + 0x02)
#define		RSTCSR_WOVF	(0x80)
#define		RSTCSR_RSTE	(0x40)
#define	REG_WINA(base)		((base) + 0x00)
#define	REG_WINB(base)		((base) + 0x00)

typedef struct WdgTimer {
	BusDevice bdev;
	CycleTimer eventTimer;
	CycleCounter_t lastUpdate;
	CycleCounter_t accCycles;
	Clock_t *clkIn;
	Clock_t *clkCntr;
	uint8_t regTCSR;
	uint8_t regTCNT;
	uint8_t regRSTCSR;
	uint8_t wovfReadFlag;
} WdgTimer;

static void
actualize_counter(WdgTimer * wdt)
{
	CycleCounter_t now = CycleCounter_Get();
	FractionU64_t frac;
	CycleCounter_t cycles = now - wdt->lastUpdate;
	CycleCounter_t acc;
	uint64_t ctrCycles;
//        uint16_t nto;
	wdt->lastUpdate = now;
	if (!(wdt->regTCSR & TCSR_TME)) {
		return;
	}
	acc = wdt->accCycles + cycles;
	frac = Clock_MasterRatio(wdt->clkCntr);
	if ((frac.nom == 0) || (frac.denom == 0)) {
		fprintf(stderr, "Warning, No clock for watchdog module\n");
		return;
	}
	ctrCycles = acc * frac.nom / frac.denom;
	acc -= ctrCycles * frac.denom / frac.nom;
	wdt->accCycles = acc;
	if ((ctrCycles + wdt->regTCNT) >= 256) {
		if ((wdt->regTCSR & TCSR_TMS)) {
			// Watchdog mode
			if (wdt->regRSTCSR & RSTCSR_RSTE) {
				usleep(10000);
				fprintf(stderr, "Watchdog Timeout\n");
				usleep(10000);
				//RX_Break();
				exit(0);
			} else {
				fprintf(stderr, "Watchdog WOV\n");
			}
		} else {
			fprintf(stderr, "Watchdog Interval\n");
		}
	}
	wdt->regTCNT = (ctrCycles + wdt->regTCNT) % 256;
}

static void
update_timeout(WdgTimer * wdt)
{
	uint16_t nto;
	FractionU64_t frac;
	CycleCounter_t cpu_cycles;
	if (!(wdt->regTCSR & TCSR_TME)) {
		CycleTimer_Remove(&wdt->eventTimer);
		return;
	}
	nto = 256 - wdt->regTCNT;
	frac = Clock_MasterRatio(wdt->clkCntr);
	if ((frac.nom == 0) || (frac.denom == 0)) {
		fprintf(stderr, "Warning, No clock for timer module\n");
		return;
	}
	cpu_cycles = ((uint64_t) nto * frac.denom) / frac.nom;
	//dbgprintf("timeout in %u tmr cycles, CPU %"PRIu64"\n",nto,cpu_cycles);
	CycleTimer_Mod(&wdt->eventTimer, cpu_cycles);
}

static void
timer_event(void *eventData)
{
	WdgTimer *wdt = eventData;
	actualize_counter(wdt);
	update_timeout(wdt);
}

static void
update_clock(WdgTimer * wdt)
{
	unsigned int cks = wdt->regTCSR & TCSR_CKS_MSK;
	uint32_t div;
	switch (cks) {
	    case 0:
		    div = 4;
		    break;
	    case 1:
		    div = 64;
		    break;
	    case 2:
		    div = 128;
		    break;
	    case 3:
		    div = 512;
		    break;
	    case 4:
		    div = 2048;
		    break;
	    case 5:
		    div = 8192;
		    break;
	    case 6:
		    div = 32768;
		    break;
	    case 7:
		    div = 131072;
		    break;
	    default:
		    /* Unreachable */
		    div = 1;
		    break;
	}
	Clock_MakeDerived(wdt->clkCntr, wdt->clkIn, 1, div);
}

static uint32_t
tcsr_read(void *clientData, uint32_t address, int rqlen)
{
	WdgTimer *wdt = clientData;
	return wdt->regTCSR | 0x18;
}

static void
wina_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	WdgTimer *wdt = clientData;
	if ((value & 0xff00) == 0x5a00) {
		actualize_counter(wdt);
		wdt->regTCNT = value & 0xff;
		update_timeout(wdt);
	} else if ((value & 0xff00) == 0xa500) {
		actualize_counter(wdt);
		wdt->regTCSR = value & 0xff;
		if ((value & 0x98) != 0x98) {
			fprintf(stderr, "WDT TCSR write: Illegal value %02x\n", value);
		}
		update_clock(wdt);
		update_timeout(wdt);
	} else {
		fprintf(stderr, "Illegal magic writing to WDT WINA\n");
	}
}

static uint32_t
tcnt_read(void *clientData, uint32_t address, int rqlen)
{
	WdgTimer *wdt = clientData;
	actualize_counter(wdt);
	return wdt->regTCNT;
}

static uint32_t
rstcsr_read(void *clientData, uint32_t address, int rqlen)
{
	WdgTimer *wdt = clientData;
	if (wdt->regRSTCSR & RSTCSR_WOVF) {
		wdt->wovfReadFlag = 1;
	}
	return wdt->regRSTCSR | 0x1f;
}

static void
winb_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	WdgTimer *wdt = clientData;
	actualize_counter(wdt);
	if ((value & 0xff00) == 0xa500) {
		if (value & 1) {
			return;
		}
		if (!wdt->wovfReadFlag) {
			fprintf(stderr, "WINB Clear WOVF without reading before\n");
		}
		wdt->wovfReadFlag = 0;
		wdt->regRSTCSR &= ~RSTCSR_WOVF;
	} else if ((value & 0xff00) == 0x5a00) {
		wdt->regRSTCSR &= ~RSTCSR_RSTE;
		wdt->regRSTCSR |= (value & RSTCSR_RSTE);
		update_timeout(wdt);
	} else {
		fprintf(stderr, "Bad magic writing WINB of WDT\n");
	}
}

static void
Wdt_Map(void *owner, uint32_t base, uint32_t mask, uint32_t mapflags)
{
	WdgTimer *wdt = owner;
	IOH_New8(REG_TCSR(base), tcsr_read, wina_write, wdt);
	IOH_New8(REG_TCNT(base), tcnt_read, NULL, wdt);
	IOH_New8(REG_RSTCSR(base), rstcsr_read, winb_write, wdt);
}

static void
Wdt_Unmap(void *owner, uint32_t base, uint32_t mask)
{
	IOH_Delete8(REG_TCSR(base));
	IOH_Delete8(REG_TCNT(base));
	IOH_Delete8(REG_RSTCSR(base));
}

BusDevice *
RX62NWdg_New(const char *name)
{
	WdgTimer *wdt = sg_new(WdgTimer);
	wdt->bdev.first_mapping = NULL;
	wdt->bdev.Map = Wdt_Map;
	wdt->bdev.UnMap = Wdt_Unmap;
	wdt->bdev.owner = wdt;
	wdt->bdev.hw_flags = MEM_FLAG_WRITABLE | MEM_FLAG_READABLE;
	wdt->clkIn = Clock_New("%s.clk", name);
	wdt->clkCntr = Clock_New("%s.clkCntr", name);
	update_clock(wdt);
	CycleTimer_Init(&wdt->eventTimer, timer_event, wdt);
	return &wdt->bdev;
}
