/*
 **************************************************************************************************
 *
 * Emulation of AT91RM9200 System Timer (TC)
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

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "bus.h"
#include "signode.h"
#include "cycletimer.h"
#include "clock.h"
#include "at91_st.h"
#include "sgstring.h"

#if 0
#define dbgprintf(x...) { fprintf(stderr,x); }
#else
#define dbgprintf(x...)
#endif

#define ST_CR(base)	((base)+0x00)
#define		CR_WDRST	(1<<0)
#define ST_PIMR(base)	((base)+0x04)
#define	ST_WDMR(base)	((base)+0x08)
#define		WDMR_EXTEN	(1<<17)
#define		WDMR_RSTEN	(1<<16)
#define ST_RTMR(base)	((base)+0x0c)
#define ST_SR(base)	((base)+0x10)
#define		SR_PITS		(1<<0)
#define		SR_WDOVF	(1<<1)
#define		SR_RTTINC	(1<<2)
#define		SR_ALMS		(1<<3)
#define ST_IER(base)	((base)+0x14)
#define		IER_PITS	(1<<0)
#define		IER_WDOVF	(1<<1)
#define		IER_RTTINC	(1<<2)
#define		IER_ALMS	(1<<3)
#define ST_IDR(base)	((base)+0x18)
#define		IDR_PITS	(1<<0)
#define		IDR_WDOVF	(1<<1)
#define		IDR_RTTINC	(1<<2)
#define		IDR_ALMS	(1<<3)
#define ST_IMR(base)	((base)+0x1c)
#define		IMR_PITS	(1<<0)
#define		IMR_WDOVF	(1<<1)
#define		IMR_RTTINC	(1<<2)
#define		IMR_ALMS	(1<<3)
#define ST_RTAR(base)	((base)+0x20)
#define ST_CRTR(base)	((base)+0x24)

typedef struct AT91St {
	BusDevice bdev;
	Clock_t *slck;
	uint32_t wdmr;
	uint32_t wdg_count;
	uint32_t rtmr;
	uint32_t pimr;
	uint32_t ptr_count;
	uint32_t sr;
	uint32_t imr;
	uint32_t rtar;
	uint32_t crtv;		/* rt_count */
	CycleCounter_t last_ptr_update;
	CycleCounter_t ptr_saved_cpucycles;
	CycleCounter_t last_wdg_update;
	CycleCounter_t wdg_saved_cpucycles;
	CycleCounter_t last_rt_update;
	CycleCounter_t rt_saved_cpucycles;
	CycleTimer wdg_timer;
	CycleTimer ptr_timer;
	CycleTimer rtinc_timer;
	CycleTimer alarm_timer;
	SigNode *irqNode;
} AT91St;

static void
update_interrupt(AT91St * st)
{
	/* Positive level internal interrupt source wired or with mc dbgu st rtc and pmc */
	if (st->sr & st->imr) {
		SigNode_Set(st->irqNode, SIG_HIGH);
	} else {
		SigNode_Set(st->irqNode, SIG_PULLDOWN);
	}
}

static void
actualize_rt(AT91St * st)
{
	uint64_t diff, cycles, cyclelen = 1;
	uint32_t rtpres = st->rtmr & 0xffff;
	if (rtpres == 0) {
		rtpres = 0x10000;
	}
	diff = CycleCounter_Get() - st->last_rt_update;
	st->last_rt_update = CycleCounter_Get();
	st->rt_saved_cpucycles += diff;
	if (Clock_Freq(st->slck)) {
		cyclelen = ((uint64_t) rtpres * CycleTimerRate_Get() / Clock_Freq(st->slck));
	}
	if (cyclelen) {
		cycles = st->rt_saved_cpucycles / cyclelen;
	} else {
		cycles = 0;
	}
	st->rt_saved_cpucycles -= cycles * cyclelen;
	if ((st->crtv < st->rtar) && ((cycles + st->crtv) >= st->rtar)) {
		if (!(st->sr & SR_ALMS)) {
			st->sr |= SR_ALMS;
			update_interrupt(st);
		}
	}
	if (cycles) {
		st->crtv = (st->crtv + cycles) & 0xfffff;
		if (!(st->sr & SR_RTTINC)) {
			st->sr |= SR_RTTINC;
			update_interrupt(st);
		}
	}
}

static void
update_rt_event(AT91St * st)
{
	if (st->imr & SR_RTTINC) {
		fprintf(stderr, "AT91St: RTTINC interrupt not implemented\n");
	} else {
		CycleTimer_Remove(&st->alarm_timer);
	}
	if (st->imr & SR_ALMS) {
		fprintf(stderr, "Alarm interrupt not implemented\n");
	} else {
		CycleTimer_Remove(&st->alarm_timer);
	}
}

static void
alarm_event(void *clientData)
{
	AT91St *st = (AT91St *) clientData;
	actualize_rt(st);
	update_rt_event(st);
}

static void
actualize_ptr(AT91St * st)
{
	int reload_val = st->pimr & 0xffff;
	uint64_t diff;
	uint64_t cyclelen = 1;
	uint64_t decrement;
	if (reload_val == 0) {
		reload_val = 0x10000;
	}
	diff = CycleCounter_Get() - st->last_ptr_update;
	st->last_ptr_update = CycleCounter_Get();
	st->ptr_saved_cpucycles += diff;
	if (Clock_Freq(st->slck)) {
		cyclelen = CycleTimerRate_Get() / Clock_Freq(st->slck);
	}
	if (cyclelen) {
		decrement = st->ptr_saved_cpucycles / cyclelen;
	} else {
		decrement = 0;
	}
	st->ptr_saved_cpucycles -= decrement * cyclelen;
	if (decrement >= st->ptr_count) {
		uint64_t newcount;
		decrement -= st->ptr_count;
		newcount = decrement % reload_val;
		st->ptr_count = reload_val - newcount;
		st->sr |= SR_PITS;
		update_interrupt(st);
	} else {
		st->ptr_count -= decrement;
	}
}

static void
update_ptr_event(AT91St * st)
{
	uint64_t cyclelen;
	CycleCounter_t timeout;
	cyclelen = CycleTimerRate_Get() / Clock_Freq(st->slck);
	timeout = cyclelen * st->ptr_count - st->ptr_saved_cpucycles;
	CycleTimer_Mod(&st->ptr_timer, timeout);
}

static void
ptr_event(void *clientData)
{
	AT91St *st = (AT91St *) clientData;
	actualize_ptr(st);
	if (st->ptr_count > 0) {
		update_ptr_event(st);
	} else {
		fprintf(stderr, "AT91St: too much events from ptimer\n");
	}
}

static void
actualize_wdg(AT91St * st)
{
	uint64_t diff;
	uint64_t cyclelen, cycles;
	uint64_t freq;
	uint32_t reload_value = st->wdmr & 0xffff;
	if (reload_value == 0) {
		reload_value = 0x10000;
	}
	diff = CycleCounter_Get() - st->last_wdg_update;
	st->last_wdg_update = CycleCounter_Get();
	freq = Clock_Freq(st->slck) / 128;
	if (!freq)
		return;
	cyclelen = CycleTimerRate_Get() / freq;
	if (!cyclelen) {
		return;
	}
	st->wdg_saved_cpucycles += diff;
	cycles = st->wdg_saved_cpucycles / cyclelen;
	st->wdg_saved_cpucycles -= cycles * cyclelen;
	if (cycles >= st->wdg_count) {
		uint32_t newcount;
		cycles -= st->wdg_count;
		newcount = cycles % reload_value;
		st->wdg_count = reload_value - newcount;
		st->sr |= SR_WDOVF;
		update_interrupt(st);
		if (st->wdmr & WDMR_RSTEN) {
			fprintf(stderr, "watchdog reset\n");
			exit(0);
		}
	} else {
		st->wdg_count -= cycles;
	}
}

static void update_wdg_event(AT91St * st);

static void
wdg_timeout(void *clientData)
{
	AT91St *st = (AT91St *) clientData;
	actualize_wdg(st);
	if (st->wdg_count > 0) {
		update_wdg_event(st);
	} else {
		fprintf(stderr, "At91St: too much events from watchdog\n");
	}
}

static void
update_wdg_event(AT91St * st)
{
	uint64_t freq;
	uint64_t cyclelen;
	CycleCounter_t timeout;
	freq = Clock_Freq(st->slck) / 128;
	if (freq) {
		cyclelen = CycleTimerRate_Get() / freq;
		timeout = cyclelen * st->wdg_count - st->wdg_saved_cpucycles;
		CycleTimer_Mod(&st->wdg_timer, timeout);
	}
}

static uint32_t
cr_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "AT91St: CR register is writeonly\n");
	return 0;
}

static void
cr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	AT91St *st = (AT91St *) clientData;
	int reload_value;
	if (value & CR_WDRST) {
		reload_value = st->wdmr & 0xffff;
		if (reload_value == 0) {
			reload_value = 0x10000;
		}
		actualize_wdg(st);
		st->wdg_count = reload_value;
		update_wdg_event(st);
	}
}

static uint32_t
pimr_read(void *clientData, uint32_t address, int rqlen)
{
	AT91St *st = (AT91St *) clientData;
	return st->pimr;
}

static void
pimr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	AT91St *st = (AT91St *) clientData;
	actualize_ptr(st);
	int reload_val = value & 0xffff;
	if (reload_val == 0) {
		reload_val = 0x10000;
	}
	st->pimr = value;
	st->ptr_count = reload_val;
	update_ptr_event(st);
	dbgprintf("AT91St: Periodic timer reload val %d\n", reload_val);
}

static uint32_t
wdmr_read(void *clientData, uint32_t address, int rqlen)
{
	AT91St *st = (AT91St *) clientData;
	actualize_wdg(st);
	return st->wdmr;
}

static void
wdmr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	AT91St *st = (AT91St *) clientData;
	st->wdmr = value;
	update_wdg_event(st);
}

static uint32_t
rtmr_read(void *clientData, uint32_t address, int rqlen)
{
	AT91St *st = (AT91St *) clientData;
	return st->rtmr;
}

/*
 * Writing the rtmr register immediately reloads the clock divider and resets the
 * 20 Bit counter to 0
 */
static void
rtmr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	AT91St *st = (AT91St *) clientData;
	actualize_rt(st);
	st->crtv = 0;
	st->rtmr = value & 0xffff;
	update_rt_event(st);
}

static uint32_t
sr_read(void *clientData, uint32_t address, int rqlen)
{
	AT91St *st = (AT91St *) clientData;
	uint32_t retval = st->sr;
	st->sr = 0;
	update_interrupt(st);
	return retval;
}

static void
sr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "AT91St: SR register is writeonly\n");
}

static uint32_t
ier_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "AT91St: IER register is write only\n");
	return 0;
}

static void
ier_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	AT91St *st = (AT91St *) clientData;
	st->imr |= (value & 0xf);
	update_interrupt(st);
}

static uint32_t
idr_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "AT91St: IDR regsiter is writeonly\n");
	return 0;
}

static void
idr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	AT91St *st = (AT91St *) clientData;
	st->imr &= ~(value & 0xf);
	update_interrupt(st);
}

static uint32_t
imr_read(void *clientData, uint32_t address, int rqlen)
{
	AT91St *st = (AT91St *) clientData;
	return st->imr;
}

static void
imr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "AT91St: IMR register is readonly\n");
}

static uint32_t
rtar_read(void *clientData, uint32_t address, int rqlen)
{
	AT91St *st = (AT91St *) clientData;
	return st->rtar;
}

static void
rtar_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	AT91St *st = (AT91St *) clientData;
	actualize_rt(st);
	st->rtar = value & 0xfffff;
	update_rt_event(st);
}

static uint32_t
crtr_read(void *clientData, uint32_t address, int rqlen)
{
	AT91St *st = (AT91St *) clientData;
	actualize_rt(st);
	return st->crtv;
}

static void
crtr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "AT91St: CRTR is a readonly register\n");
}

static void
AT91St_Map(void *owner, uint32_t base, uint32_t mask, uint32_t flags)
{
	AT91St *st = (AT91St *) owner;
	IOH_New32(ST_CR(base), cr_read, cr_write, st);
	IOH_New32(ST_PIMR(base), pimr_read, pimr_write, st);
	IOH_New32(ST_WDMR(base), wdmr_read, wdmr_write, st);
	IOH_New32(ST_RTMR(base), rtmr_read, rtmr_write, st);
	IOH_New32(ST_SR(base), sr_read, sr_write, st);
	IOH_New32(ST_IER(base), ier_read, ier_write, st);
	IOH_New32(ST_IDR(base), idr_read, idr_write, st);
	IOH_New32(ST_IMR(base), imr_read, imr_write, st);
	IOH_New32(ST_RTAR(base), rtar_read, rtar_write, st);
	IOH_New32(ST_CRTR(base), crtr_read, crtr_write, st);
//        IOH_New32(base+0x2c,debug_read,debug_write,st);
}

static void
AT91St_UnMap(void *owner, uint32_t base, uint32_t mask)
{
	IOH_Delete32(ST_CR(base));
	IOH_Delete32(ST_PIMR(base));
	IOH_Delete32(ST_WDMR(base));
	IOH_Delete32(ST_RTMR(base));
	IOH_Delete32(ST_SR(base));
	IOH_Delete32(ST_IER(base));
	IOH_Delete32(ST_IDR(base));
	IOH_Delete32(ST_IMR(base));
	IOH_Delete32(ST_RTAR(base));
	IOH_Delete32(ST_CRTR(base));

}

BusDevice *
AT91St_New(const char *name)
{
	AT91St *st = sg_new(AT91St);
	st->slck = Clock_New("%s.slck", name);
	if (!st->slck) {
		fprintf(stderr, "Can not create input clock of AT91St \"%s\"\n", name);
		exit(1);
	}
	st->last_wdg_update = CycleCounter_Get();
	st->wdmr = 0x00020000;	/* EXTEN */
	st->wdg_count = 0x10000;
	st->rtmr = 0x8000;
	st->rtar = 0;		/* Maximum value = 2^20 seconds */
	st->irqNode = SigNode_New("%s.irq", name);
	SigNode_Set(st->irqNode, SIG_PULLDOWN);
	CycleTimer_Init(&st->wdg_timer, wdg_timeout, st);
	CycleTimer_Init(&st->ptr_timer, ptr_event, st);
	CycleTimer_Init(&st->alarm_timer, alarm_event, st);
	st->bdev.first_mapping = NULL;
	st->bdev.Map = AT91St_Map;
	st->bdev.UnMap = AT91St_UnMap;
	st->bdev.owner = st;
	st->bdev.hw_flags = MEM_FLAG_WRITABLE | MEM_FLAG_READABLE;
	update_interrupt(st);
	return &st->bdev;
}
