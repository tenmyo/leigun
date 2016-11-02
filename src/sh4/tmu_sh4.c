/*
 ************************************************************************************************
 * SH4 Timer Unit (TMU) 
 *
 * State: working with u-boot 
 *
 * Used Renesas SH-4 Software Manual REJ09B0318-0600
 *
 * Copyright 2009 Jochen Karrer. All rights reserved.
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
#include <stdlib.h>
#include <stdio.h>
#include "bus.h" 
#include "clock.h" 
#include "cycletimer.h" 
#include "sgstring.h"
#include "sh4/tmu_sh4.h"
#include "sgtypes.h"
#include "signode.h"
#include "senseless.h"
#include "sglib.h"

#define TMU_TOCR(base)	((base) + 0x00)
#define		TOCR_TCOE	(1 << 0)

#define	TMU_TSTR(base)	((base) + 0x04)
#define		TSTR_STR0	(1 << 0)
#define		TSTR_STR1	(1 << 1)
#define		TSTR_STR2	(1 << 2)

#define TMU_TCOR0(base)	((base) + 0x08)
#define TMU_TCNT0(base)	((base) + 0x0c)
#define TMU_TCR0(base)	((base) + 0x10)
#define		TCR_TPSC_MSK	(7)
#define		TCR_CKEG_MSK	(3 << 3)
#define		TCR_CKEG_SHIFT	(3)
#define		TCR_UNIE	(1 << 5)
#define		TCR_UNF		(1 << 8)

#define TMU_TCOR1(base)	((base) + 0x14)
#define TMU_TCNT1(base)	((base) + 0x18)
#define TMU_TCR1(base)	((base) + 0x1c)
#define TMU_TCOR2(base)	((base) + 0x20)
#define TMU_TCNT2(base)	((base) + 0x24)
#define TMU_TCR2(base)	((base) + 0x28)
#define		TCR2_ICPE_MSK	(3 << 6)
#define		TCR2_ICPE_SHIFT (6)
#define		TCR2_ICPF	(1 << 9)

#define TMU_TCPR2(base)	((base) + 0x2c)

typedef struct SH4TMU  SH4TMU;

typedef struct SH4Timer {
	SH4TMU *tmu;
	Clock_t *clk_timer;
	int index;
	uint32_t tcor;
	uint32_t tcnt;
	uint32_t tcr;
	CycleCounter_t last_actualized;
	CycleCounter_t saved_cycles;
	CycleTimer event_timer;
} SH4Timer; 

struct SH4TMU {
	BusDevice bdev;
	Clock_t *clk_in;
	SH4Timer tmr[3];
	uint32_t tocr;
	uint32_t tstr;
	SigNode *sigIrq;
};

static void 
update_interrupt(SH4TMU *tmu)
{
	int i;
	int irq = 0;
	for(i = 0;i < 3;i++) {
		if((tmu->tmr[i].tcr & TCR_UNF) && (tmu->tmr[i].tcr & TCR_UNIE)) {
			irq = 1;
		}
	}
	if(irq) {
		SigNode_Set(tmu->sigIrq,SIG_LOW);
	} else {
		SigNode_Set(tmu->sigIrq,SIG_OPEN);
	}	
}

static void
update_timeout(SH4Timer *tmr) 
{
	if((tmr->tmu->tstr & (1 << tmr->index)) == 0) {
		CycleTimer_Remove(&tmr->event_timer);
		return;
	}	
	if(tmr->tcr & TCR_UNIE) {
		FractionU64_t frac;
		uint64_t counter_cycles = tmr->tcnt;
		uint64_t master_cycles;
		frac = Clock_MasterRatio(tmr->clk_timer);
		if(frac.nom) {
			master_cycles = counter_cycles * frac.denom / frac.nom;
			master_cycles -= tmr->saved_cycles;
			CycleTimer_Mod(&tmr->event_timer,master_cycles);
		}
	} else {
		CycleTimer_Remove(&tmr->event_timer);
	}
}

static void
actualize_counter(SH4Timer *tmr) 
{
	uint64_t counter_cycles;
	uint64_t tcnt;
	FractionU64_t frac;
	/* Check if timer is running */
	if((tmr->tmu->tstr & (1 << tmr->index)) == 0) {
		tmr->last_actualized = CycleCounter_Get();	
		return;
	}	
	tcnt = tmr->tcnt;
	tmr->saved_cycles += CycleCounter_Get() - tmr->last_actualized;
	tmr->last_actualized = CycleCounter_Get();	
	frac = Clock_MasterRatio(tmr->clk_timer);
	if(!frac.nom || !frac.denom) {
		fprintf(stderr,"Bad clock\n");
		return;
	}
	counter_cycles = tmr->saved_cycles * frac.nom / frac.denom; 
	//fprintf(stderr,"Counter cycles %lld nom %lld, denom %lld, saved %lld\n",counter_cycles,frac.nom,frac.denom,tmr->saved_cycles);
	tmr->saved_cycles -= counter_cycles * frac.denom / frac.nom;
	if(counter_cycles <= tcnt) {
		tmr->tcnt = tcnt - counter_cycles;	
	} else {
		uint64_t period = (uint64_t)tmr->tcor + 1;
		counter_cycles -= tcnt;	
		tmr->tcnt = tmr->tcor - (counter_cycles % period);
		tmr->tcr |= TCR_UNF;
		update_interrupt(tmr->tmu);
	}
}
static void 
update_clock(SH4Timer *tmr)
{
	uint32_t divider = 0;
	switch (tmr->tcr & 0x7) {
		case 0:
			divider = 4;
			break;
		case 1:
			divider = 16;
			break;
		case 2:
			divider = 64;
			break;
		case 3:
			divider = 64;
			break;
			
		case 4:
			divider = 1024;
			break;

		default:
		case 5:
		case 6:
		case 7:
			fprintf(stderr,"Clock source %d not implemented\n",tmr->tcr & 7);
	}
	if(divider) {
		Clock_MakeDerived(tmr->clk_timer,tmr->tmu->clk_in,1,divider);
	}
}

static void
timer_event(void *clientData)
{
        SH4Timer *tmr = (SH4Timer *)clientData;
        actualize_counter(tmr);
        update_timeout(tmr);
}

static uint32_t
tocr_read(void *clientData,uint32_t address,int rqlen)
{
	SH4TMU *tmu = (SH4TMU *) clientData;
	return tmu->tocr;
}

static void
tocr_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	fprintf(stderr,"%s: register %s not implemented\n",__FILE__,__func__);
}

static uint32_t
tstr_read(void *clientData,uint32_t address,int rqlen)
{
	SH4TMU *tmu = (SH4TMU *) clientData;
	return tmu->tstr;
}

static void
tstr_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	SH4TMU *tmu = (SH4TMU *) clientData;
	uint32_t diff = tmu->tstr ^ value;
	tmu->tstr = value & 0x7;
	if(diff & TSTR_STR0) {
		actualize_counter(&tmu->tmr[0]);
		// update_timeout(tmu,0);
	} 
	if(diff & TSTR_STR1) {
		actualize_counter(&tmu->tmr[1]);
		// update_timeout(tmu,1);
	} 
	if(diff & TSTR_STR2) {
		actualize_counter(&tmu->tmr[2]); 
		// update_timeout(tmu,2);
	} 
}

static uint32_t
tcor_read(void *clientData,uint32_t address,int rqlen)
{
	SH4Timer *tmr = (SH4Timer *) clientData;
	return tmr->tcor;
}

static void
tcor_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	SH4Timer *tmr = (SH4Timer *) clientData;
	actualize_counter(tmr);
	tmr->tcor = value;
}

static uint32_t
tcnt_read(void *clientData,uint32_t address,int rqlen)
{
	SH4Timer *tmr = (SH4Timer *) clientData;
	actualize_counter(tmr);
	Senseless_Report(200);
	return tmr->tcnt;
}

static void
tcnt_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	SH4Timer *tmr = (SH4Timer *) clientData;
	actualize_counter(tmr);
	tmr->tcnt = value;
	update_timeout(tmr);
}


static uint32_t
tcr_read(void *clientData,uint32_t address,int rqlen)
{
	SH4Timer *tmr = (SH4Timer *) clientData;
	return tmr->tcr;
}

static void
tcr_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	SH4Timer *tmr = (SH4Timer *) clientData;
	actualize_counter(tmr);
	tmr->tcr = value;
	update_clock(tmr);
	update_timeout(tmr);
}

static uint32_t
tcpr2_read(void *clientData,uint32_t address,int rqlen)
{
	fprintf(stderr,"%s: register %s not implemented\n",__FILE__,__func__);
	return 0;
}

static void
tcpr2_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	fprintf(stderr,"%s: register %s not implemented\n",__FILE__,__func__);
}


static void
TMU_Map(void *owner,uint32_t base,uint32_t mask,uint32_t flags)
{
        SH4TMU *tmu = (SH4TMU *) owner;
	IOH_New32(TMU_TOCR(base),tocr_read,tocr_write,tmu);
	IOH_New32(TMU_TSTR(base),tstr_read,tstr_write,tmu);
	IOH_New32(TMU_TCOR0(base),tcor_read,tcor_write,&tmu->tmr[0]);
	IOH_New32(TMU_TCNT0(base),tcnt_read,tcnt_write,&tmu->tmr[0]);
	IOH_New32(TMU_TCR0(base),tcr_read,tcr_write,&tmu->tmr[0]);
	IOH_New32(TMU_TCOR1(base),tcor_read,tcor_write,&tmu->tmr[1]);
	IOH_New32(TMU_TCNT1(base),tcnt_read,tcnt_write,&tmu->tmr[1]);
	IOH_New32(TMU_TCR1(base),tcr_read,tcr_write,&tmu->tmr[1]);
	IOH_New32(TMU_TCOR2(base),tcor_read,tcor_write,&tmu->tmr[2]);
	IOH_New32(TMU_TCNT2(base),tcnt_read,tcnt_write,&tmu->tmr[2]);
	IOH_New32(TMU_TCR2(base),tcr_read,tcr_write,&tmu->tmr[2]);
	IOH_New32(TMU_TCPR2(base),tcpr2_read,tcpr2_write,&tmu->tmr[2]);
}

static void
TMU_UnMap(void *owner,uint32_t base,uint32_t mask)
{
	IOH_Delete32(TMU_TOCR(base));
	IOH_Delete32(TMU_TSTR(base));
	IOH_Delete32(TMU_TCOR0(base));
	IOH_Delete32(TMU_TCNT0(base));
	IOH_Delete32(TMU_TCR0(base));
	IOH_Delete32(TMU_TCOR1(base));
	IOH_Delete32(TMU_TCNT1(base));
	IOH_Delete32(TMU_TCR1(base));
	IOH_Delete32(TMU_TCOR2(base));
	IOH_Delete32(TMU_TCNT2(base));
	IOH_Delete32(TMU_TCR2(base));
	IOH_Delete32(TMU_TCPR2(base));
}


BusDevice *
SH4TMU_New(const char *name)
{
	SH4TMU *tmu = sg_new(SH4TMU);
	int i;

	tmu->bdev.first_mapping = NULL;
        tmu->bdev.Map = TMU_Map;
        tmu->bdev.UnMap = TMU_UnMap;
        tmu->bdev.owner= tmu;
        tmu->bdev.hw_flags=MEM_FLAG_WRITABLE|MEM_FLAG_READABLE;
	tmu->clk_in = Clock_New("%s.clk_in",name);
	if(!tmu->clk_in) {
		fprintf(stderr,"Can not create input clock for Timer \"%s\"\n",name);
		exit(1);
	}
	tmu->sigIrq = SigNode_New("%s.irq",name);
	if(!tmu->sigIrq) {
		fprintf(stderr,
			"Cannot create IRQ signal for timer module \"%s\"\n",name);
		exit(1);
	}
	for(i = 0;i < 3;i++) {
		SH4Timer *tmr = &tmu->tmr[i];
		tmr->tmu = tmu;
		tmr->clk_timer = Clock_New("%s.clk_timer%d",name,i);
		tmr->index = i;
		CycleTimer_Init(&tmr->event_timer,timer_event,tmr);
		update_clock(tmr);
		tmr->tcor = 0xffffffff;
		tmr->tcnt = 0xffffffff;
	}
	fprintf(stderr,"Created SH4 Timer Module (TMU) \"%s\"\n",name);
	return &tmu->bdev;
}
