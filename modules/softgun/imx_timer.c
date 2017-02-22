/*
 *************************************************************************************************
 *
 * Emulation of Freescale IMX General purpose Timer module
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
#include "imx_timer.h"
#include "senseless.h"
#include "sgstring.h"

/* default base is 0x10003000/4000/5000 for imx21 */
#define TCTL(base) 	((base)+0)
#define		TCTL_SWR		(1<<15)
#define		TCTL_CC			(1<<10)
#define		TCTL_OM			(1<<9)
#define		TCTL_FRR		(1<<8)
#define		TCTL_CAP_MASK		(3<<6)
#define		TCTL_CAP_SHIFT  	(6)
#define 	TCTL_CAPTEN		(1<<5)
#define		TCTL_COMPEN		(1<<4)
#define		TCTL_CLKSOURCE_MASK	(7<<1)
#define		TCTL_CLKSOURCE_SHIFT	(1)
#define 	TCTL_CLK_SOURCE(x)    (((x) & 0x7) <<  1)	/* clock source: */
#define 	TCTL_CLK_SOURCE_STOP  TCTL_CLK_SOURCE(0x00)	/* stop count */
#define 	TCTL_CLK_SOURCE_PC1   TCTL_CLK_SOURCE(0x01)	/* PERCLK1 to prescaler */
#define 	TCTL_CLK_SOURCE_QPC1  TCTL_CLK_SOURCE(0x02)	/* PERCLK1 / 4 to prescaler */
#define 	TCTL_CLK_SOURCE_TIN   TCTL_CLK_SOURCE(0x03)	/* TIN to prescaler */
#define 	TCTL_CLK_SOURCE_32K   TCTL_CLK_SOURCE(0x04)	/* 32kHz clock to prescaler */

#define		TCTL_TEN		(1<<0)

#define TPRER(base) 	((base)+0x4)
#define TCMP(base)  	((base)+0x8)
#define TCR(base)   	((base)+0xc)
#define TCN(base)	((base)+0x10)
#define TSTAT(base)	((base)+0x14)
#define		TSTAT_CAPT	(1<<1)
#define		TSTAT_COMP	(1<<0)

typedef struct IMX_Timer {
	BusDevice bdev;
	int interrupt_posted;
	SigNode *irqNode;
	SigNode *toutNode;
	SigNode *tinNode;
	SigTrace *tinTrace;
	CycleCounter_t last_timer_update;
	CycleCounter_t saved_cpu_cycles;
	CycleTimer event_timer;

	uint32_t tctl;
	uint32_t tprer;
	uint32_t tcmp;
	uint32_t tcr;
	uint32_t tcn;
	uint32_t tstat;
} IMX_Timer;

static void
software_reset(IMX_Timer * itmr)
{

	SigNode_Set(itmr->irqNode, SIG_HIGH);
	SigNode_Set(itmr->toutNode, SIG_LOW);
	itmr->tctl = itmr->tctl & TCTL_TEN;
	itmr->tprer = 0;
	itmr->tcmp = 0xffffffff;
	itmr->tcr = 0;
	itmr->tcn = 0;
	itmr->tstat = 0;
}

static void
update_interrupts(IMX_Timer * itmr)
{

	int interrupt = 0;
	if (itmr->tctl & TCTL_COMPEN) {
		if (itmr->tstat & TSTAT_COMP) {
			interrupt = 1;
		}
	}
	if (itmr->tctl & TCTL_CAPTEN) {
		if (itmr->tstat & TSTAT_CAPT) {
			interrupt = 1;
		}
	}
	if (interrupt) {
		if (!itmr->interrupt_posted) {
			SigNode_Set(itmr->irqNode, SIG_LOW);
			itmr->interrupt_posted = 1;
		}
	} else {
		if (itmr->interrupt_posted) {
			SigNode_Set(itmr->irqNode, SIG_HIGH);
			itmr->interrupt_posted = 0;
		}
	}
}

static uint32_t
get_divider(IMX_Timer * itmr)
{
	int clksource;
	uint32_t divider = 2;
	clksource = (itmr->tctl & TCTL_CLKSOURCE_MASK) >> TCTL_CLKSOURCE_SHIFT;
	switch (clksource) {
	    case 0:
		    return 0;
	    case 1:
		    divider = divider * ((itmr->tprer & 2047) + 1);
		    break;
	    case 2:
		    divider = 4 * divider * ((itmr->tprer & 2047) + 1);
		    break;

	    case 3:		/* TIN is clksource, events are not counted here */
		    return 0;

	    case 4:		/* 32 kHz */
	    case 5:		/* 32 kHz */
	    case 6:		/* 32 kHz */
	    case 7:		/* 32 kHz */
		    divider = CycleTimerRate_Get() >> 15;
		    //fprintf(stderr,"divider %d\n",divider); // jk
		    break;

	    default:
		    fprintf(stderr, "Unreachable code\n");
		    divider = 0;
	}
	return divider;
}

static void
do_match_action(IMX_Timer * itmr)
{
	itmr->tstat |= TSTAT_COMP;
	update_interrupts(itmr);
}

static void
actualize_tcn(IMX_Timer * itmr)
{
	uint64_t timer_cycles;
	uint32_t divider = 2;
	uint64_t tcnew;
	if (!(itmr->tctl & TCTL_TEN)) {
		itmr->last_timer_update = CycleCounter_Get();
		itmr->tcn = 0;	/* Resets the timer to 0! */
		return;
	}
	itmr->saved_cpu_cycles += CycleCounter_Get() - itmr->last_timer_update;
	itmr->last_timer_update = CycleCounter_Get();
	divider = get_divider(itmr);
	if (!divider) {
		return;
	}
	//fprintf(stderr,"saved %lld divider %d\n",itmr->saved_cpu_cycles,divider);
	timer_cycles = itmr->saved_cpu_cycles / divider;
	tcnew = itmr->tcn + timer_cycles;
	if ((itmr->tcn < itmr->tcmp) && (tcnew >= itmr->tcmp)) {
		do_match_action(itmr);
		if (itmr->tctl & TCTL_FRR) {
			tcnew = tcnew & 0xffffffff;
		} else {
			tcnew = tcnew % itmr->tcmp;
		}
	}
	itmr->tcn = tcnew;
#if 0
	if (timer_cycles) {
		fprintf(stderr, "new tcn %08x at %lx\n", itmr->tcn, CycleCounter_Get());	// jk
	}
#endif
	itmr->saved_cpu_cycles -= timer_cycles * divider;
}

static void do_event(void *clientData);

/*
 * ---------------------------------------------------------------
 * Update the compare event timer by calculating when the 
 * cmp event will occur 
 * ---------------------------------------------------------------
 */
static void
update_cmp_event_timer(IMX_Timer * itmr)
{
	uint32_t tcyc_until_event;
	CycleCounter_t ccyc_until_event;
	uint32_t divider;
	// enabled check is missing here
	if (!(itmr->tctl & TCTL_TEN) || !(itmr->tctl & TCTL_COMPEN)) {
		CycleTimer_Remove(&itmr->event_timer);
		return;
	}
	if (itmr->tcn <= itmr->tcmp) {
		tcyc_until_event = itmr->tcmp - itmr->tcn;
	} else {
		/* Free running mode stops at 0xffffffff */
		if (!(itmr->tctl & TCTL_FRR)) {
			CycleTimer_Remove(&itmr->event_timer);
			return;
		}
		fprintf(stderr, "should not happen: to late\n");
		tcyc_until_event = itmr->tcmp - itmr->tcn;
	}
	divider = get_divider(itmr);
	if (!divider) {
		return;
	}
	ccyc_until_event = (uint64_t) divider *tcyc_until_event - itmr->saved_cpu_cycles;
	//fprintf(stderr,"Cyc until %lld divider %d tcyc_until %d saved %lld\n",ccyc_until_event,divider,tcyc_until_event,itmr->saved_cpu_cycles);
	if (CycleTimer_IsActive(&itmr->event_timer)) {
		CycleTimer_Mod(&itmr->event_timer, ccyc_until_event);
	} else {
		CycleTimer_Add(&itmr->event_timer, ccyc_until_event, do_event, itmr);
	}
}

static void
do_event(void *clientData)
{
	IMX_Timer *itmr = (IMX_Timer *) clientData;
	actualize_tcn(itmr);
	update_cmp_event_timer(itmr);
}

static void
capture(SigNode * node, int value, void *clientData)
{
	IMX_Timer *itmr = (IMX_Timer *) clientData;
	int cap = (itmr->tctl & TCTL_CAP_MASK) >> TCTL_CAP_SHIFT;
	switch (cap) {
	    case 0:
		    fprintf(stderr, "Should't happen\n");
		    break;
	    case 1:
		    if (value == SIG_HIGH) {
			    actualize_tcn(itmr);
			    itmr->tcr = itmr->tcn;
		    }
		    break;
	    case 2:
		    if (value == SIG_LOW) {
			    actualize_tcn(itmr);
			    itmr->tcr = itmr->tcn;
		    }
		    break;
	    case 3:
		    actualize_tcn(itmr);
		    itmr->tcr = itmr->tcn;
		    break;
	}
}

/*
 * --------------------------------------------------------------
 * Timer Control register
 * 	
 * Bit 15:	SWR
 *	Software reset does not clear ten
 *	1 put timer into reset state, selfclearing
 *	0 normal mode	
 * Bit 10:	CC	
 *	0: halt timer when TEN=0
 *	1: clear counter when TEN=0	
 * Bit 9:	OM
 *	0: pulse output 1 clock period on compare event
 *	1: toggle output on compare event
 * Bit 8:	FRR
 *	0: restart timer on compare event
 *	1: continue counting to 0xffffffff on compare event
 * Bit 7,6:	CAP_MASK
 *	00: Capture disabled
 *	01: Capture on rising edge and generate interrupt
 *	10: Capture on falling edge and generate interrupt
 *	11: Capture on any edge and generate interrupt
 * Bit 5:	CAPTEN
 *	1: enable capture interrupt
 * Bit 4:	COMPEN
 *	1: enable compare interrupt
 * Bit 3,2,1:	CLKSOURCE
 *	000: Stop
 * 	001: PERCLK1 to prescaler
 *	010: PERCLK1/4 to prescaler
 *	011: TIN to prescaler
 *	1xx: 32kHz to prescaler
 *
 * Bit 0:	TEN
 *	Timer enable: not cleared by Sofware reset
 * --------------------------------------------------------------
 */

static uint32_t
tctl_read(void *clientData, uint32_t address, int rqlen)
{
	IMX_Timer *itmr = (IMX_Timer *) clientData;
	return itmr->tctl;
}

static void
tctl_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	IMX_Timer *itmr = (IMX_Timer *) clientData;
	if (value & TCTL_SWR) {
		software_reset(itmr);
		return;
	}
	actualize_tcn(itmr);
	if ((value & TCTL_CAP_MASK) == 0) {
		if (itmr->tinTrace) {
			SigNode_Untrace(itmr->tinNode, itmr->tinTrace);
		}
	} else {
		if (!itmr->tinTrace) {
			itmr->tinTrace = SigNode_Trace(itmr->tinNode, capture, itmr);
		}
	}
	itmr->tctl = value;
	update_cmp_event_timer(itmr);
	return;
}

/*
 * -----------------------------------------------------
 * TPRER
 *	Bits 0-10 Prescaler value
 * -----------------------------------------------------
 */
static uint32_t
tprer_read(void *clientData, uint32_t address, int rqlen)
{
	IMX_Timer *itmr = (IMX_Timer *) clientData;
	return itmr->tprer;
}

static void
tprer_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	IMX_Timer *itmr = (IMX_Timer *) clientData;
	actualize_tcn(itmr);
	itmr->tprer = value & 2047;
	update_cmp_event_timer(itmr);
	return;
}

/*
 * ---------------------------------------------------------------------
 * TCMP 
 * Bits 0-31: Timer compare register
 *	 generater compare event when count matches this register 
 * ----------------------------------------------------------------------
 */
static uint32_t
tcmp_read(void *clientData, uint32_t address, int rqlen)
{
	IMX_Timer *itmr = (IMX_Timer *) clientData;
	return itmr->tcmp;
}

static void
tcmp_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	IMX_Timer *itmr = (IMX_Timer *) clientData;
	actualize_tcn(itmr);
	itmr->tcmp = value;
	update_cmp_event_timer(itmr);
	return;
}

/*
 * -----------------------------------------------------------------
 * Timer capture register
 * 	Stores the counter value when a event occurs
 * -----------------------------------------------------------------
 */

static uint32_t
tcr_read(void *clientData, uint32_t address, int rqlen)
{
	IMX_Timer *itmr = (IMX_Timer *) clientData;
	return itmr->tcr;
}

static void
tcr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "TCR is read only\n");
	return;
}

/*
 * -------------------------------------
 * TCN
 *	Timer count 
 * -------------------------------------
 */
static uint32_t
tcn_read(void *clientData, uint32_t address, int rqlen)
{
	IMX_Timer *itmr = (IMX_Timer *) clientData;
	actualize_tcn(itmr);
	Senseless_Report(200);
	return itmr->tcn;
}

static void
tcn_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "Timer count is not writable\n");
	return;
}

/*
 * ------------------------------------------------------------------
 * TSTAT
 * 	Timer status register
 *	Bit 1: CAPT a capture event has occurred
 *	Bit 2: COMP a compare event has occurred
 * ------------------------------------------------------------------
 */
static uint32_t
tstat_read(void *clientData, uint32_t address, int rqlen)
{
	IMX_Timer *itmr = (IMX_Timer *) clientData;
	return itmr->tstat;
}

static void
tstat_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	IMX_Timer *itmr = (IMX_Timer *) clientData;
	uint32_t clearbits = value & (TSTAT_COMP | TSTAT_CAPT);
	itmr->tstat &= ~clearbits;
	update_interrupts(itmr);
	return;
}

static void
IMXTimer_UnMap(void *owner, uint32_t base, uint32_t mask)
{
	int i;
	for (i = 0; i < 0x18; i += 4) {
		IOH_Delete32(base + i);
	}
}

static void
IMXTimer_Map(void *owner, uint32_t base, uint32_t mask, uint32_t flags)
{
	IMX_Timer *itmr = (IMX_Timer *) owner;
	IOH_New32(TCTL(base), tctl_read, tctl_write, itmr);
	IOH_New32(TPRER(base), tprer_read, tprer_write, itmr);
	IOH_New32(TCMP(base), tcmp_read, tcmp_write, itmr);
	IOH_New32(TCR(base), tcr_read, tcr_write, itmr);
	IOH_New32(TCN(base), tcn_read, tcn_write, itmr);
	IOH_New32(TSTAT(base), tstat_read, tstat_write, itmr);
}

BusDevice *
IMXTimer_New(char *name)
{
	IMX_Timer *itmr = sg_new(IMX_Timer);
	itmr->irqNode = SigNode_New("%s.irq", name);
	if (!itmr->irqNode) {
		fprintf(stderr, "Cannot create irqNode for \"%s\"\n", name);
		exit(1);
	}
	SigNode_Set(itmr->irqNode, SIG_HIGH);
	itmr->toutNode = SigNode_New("%s.tout", name);
	if (!itmr->toutNode) {
		fprintf(stderr, "Cannot create toutNode for \"%s\"\n", name);
		exit(1);
	}
	SigNode_Set(itmr->toutNode, SIG_LOW);	// The manual says it is reset ????
	itmr->tinNode = SigNode_New("%s.tin", name);
	itmr->tcmp = 0xffffffff;
	itmr->bdev.first_mapping = NULL;
	itmr->bdev.Map = IMXTimer_Map;
	itmr->bdev.UnMap = IMXTimer_UnMap;
	itmr->bdev.owner = itmr;
	itmr->bdev.hw_flags = MEM_FLAG_WRITABLE | MEM_FLAG_READABLE;
	fprintf(stderr, "IMX Timer Module \"%s\" created\n", name);
	return &itmr->bdev;
}
