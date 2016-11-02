/*
 *************************************************************************************************
 *
 * LPC2106 timer
 *	Emulation of the timers of Philips LPC2106
 *	
 * Status: complete, never used, totally untested 
 *
 * Copyright 2005 Jochen Karrer. All rights reserved.
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
#include "clock.h"
#include "cycletimer.h"
#include "sgstring.h"

/* Register definitions */
#define TMR_IR		(0x00)
#define		IR_MR0	(1<<0)
#define		IR_MR1	(1<<1)
#define		IR_MR2	(1<<2)
#define		IR_MR3	(1<<3)
#define		IR_CR0	(1<<4)
#define		IR_CR1	(1<<5)
#define		IR_CR2	(1<<6)
#define		IR_CR3	(1<<7)
#define TMR_TCR		(0x04)
#define		TCR_ENA	(1<<0)
#define		TCR_RST	(1<<1)
#define TMR_TC		(0x08)
#define TMR_PR		(0x0c)
#define TMR_PC		(0x10)
#define TMR_MCR		(0x14)
#define		MCR_INT_MR0	(1<<0)
#define		MCR_RST_MR0	(1<<1)
#define		MCR_STP_MR0	(1<<2)
#define		MCR_INT_MR1	(1<<3)
#define		MCR_RST_MR1	(1<<4)
#define		MCR_STP_MR1	(1<<5)
#define		MCR_INT_MR2	(1<<6)
#define		MCR_RST_MR2	(1<<7)
#define		MCR_STP_MR2	(1<<8)
#define		MCR_INT_MR3	(1<<9)
#define		MCR_RST_MR3	(1<<10)
#define		MCR_STP_MR3	(1<<11)
#define TMR_MR0		(0x18)
#define TMR_MR1		(0x1c)
#define TMR_MR2		(0x20)
#define TMR_MR3		(0x24)
#define TMR_CCR		(0x28)
#define		CCR_CAPRISE0	(1<<0)
#define		CCR_CAPFALL0	(1<<1)
#define		CCR_CAPINT0	(1<<2)	
#define		CCR_CAPRISE1	(1<<3)
#define		CCR_CAPFALL1	(1<<4)
#define		CCR_CAPINT1	(1<<5)	
#define		CCR_CAPRISE2	(1<<6)
#define		CCR_CAPFALL2	(1<<7)
#define		CCR_CAPINT2	(1<<8)	
#define		CCR_CAPRISE3	(1<<9)
#define		CCR_CAPFALL3	(1<<10)
#define		CCR_CAPINT3	(1<<11)	

#define TMR_CR0		(0x2c)
#define TMR_CR1		(0x30)
#define TMR_CR2		(0x34)
#define TMR_CR3		(0x38)
#define TMR_EMR		(0x3c)
#define		EMR_EXMATCH0		(1<<0)
#define		EMR_EXMATCH1		(1<<1)
#define		EMR_EXMATCH2		(1<<2)
#define		EMR_EXMATCH3		(1<<3)
#define		EMR_EXMCR0_MASK		(3<<4)
#define		EMR_EXMCR0_SHIFT	(4)
#define		EMR_EXMCR1_MASK		(3<<6)
#define		EMR_EXMCR1_SHIFT	(6)
#define		EMR_EXMCR2_MASK		(3<<8)
#define		EMR_EXMCR2_SHIFT	(8)
#define		EMR_EXMCR3_MASK		(3<<10)
#define		EMR_EXMCR3_SHIFT	(10)

struct LPCTimer;
typedef struct CaptureTraceInfo {
	struct LPCTimer *tmr;	
	int line;
} CaptureTraceInfo;


typedef struct LPCTimer {
	BusDevice bdev;
	Clock_t *clk_pclk;
	uint32_t ir;
	uint32_t tc;
	uint32_t tcr;
	uint32_t pr;
	uint32_t pc;
	uint32_t mcr;
	uint32_t emr;
	CycleCounter_t last_timer_update;
	CycleCounter_t saved_cycles;
	CycleTimer event_timer;
	uint32_t mr[4];
	uint32_t ccr;
	uint32_t cr[4];
	SigNode *irqNode;
	SigNode *capNode[4];
	int oldCapStatus[4];
	SigNode *matNode[4];	
	CaptureTraceInfo capTi[4];
} LPCTimer;

static void
update_interrupts(LPCTimer *tmr) 
{
	if(tmr->ir) {
		SigNode_Set(tmr->irqNode,SIG_LOW);
	} else {
		SigNode_Set(tmr->irqNode,SIG_HIGH);
	}	
}

/*
 * ----------------------------------------------------------
 * do_match_action
 * 	When counter matches the MR register 
 *	trigger an interrupt, reset or stop the counter 
 * ----------------------------------------------------------
 */
static void
do_match_action(LPCTimer *tmr,int index) 
{
	int action = (tmr->mcr >> (3*index)) & 7; 
	/* Interrupt on MRx */
	if(action & MCR_INT_MR0) {
		tmr->ir |= (IR_MR0<<index);
		update_interrupts(tmr);
	}
	/* Reset on MRx */	
	if(action & MCR_RST_MR0) {
		tmr->tc = 0;
	}
	/* Stop on MRx */	
	if(action & MCR_STP_MR0) {
		tmr->tcr &= ~1;
	}
}

/*
 * ---------------------------------------------------------------------
 * do_exmatch_action
 * 	Update external match pins if required
 * ---------------------------------------------------------------------
 */
static void
do_exmatch_action(LPCTimer *tmr,int index) 
{
	int exmat_action = (tmr->emr >> (EMR_EXMCR0_SHIFT + 2*index)) & 3;
	if(exmat_action  == 1) {
		SigNode_Set(tmr->matNode[index],SIG_LOW);
		tmr->emr &= ~(1<<index);
	} else if(exmat_action == 2) {
		SigNode_Set(tmr->matNode[index],SIG_HIGH);
		tmr->emr |= (1<<index);
	} else if(exmat_action == 3) {
		int current = SigNode_Val(tmr->matNode[index]);
		if(current==SIG_HIGH) {
			SigNode_Set(tmr->matNode[index],SIG_LOW);
			tmr->emr |= (1<<index);
		} else {
			SigNode_Set(tmr->matNode[index],SIG_HIGH);
			tmr->emr &= ~(1<<index);
		}
	
	}
	
}

/*
 * -------------------------------------------------------------------------
 * Actualize timers
 * 	Timer counter register are not updated every clock cycle, but
 *	only if someone reads it or modifies some timer register
 *	this function brings the timer counter into an actual state
 *	and if a match happened since last read it does the match action 
 * -------------------------------------------------------------------------
 */
static void
actualize_timer(LPCTimer *tmr)
{
	int i;
	CycleCounter_t cpu_cycles,pc_cycles,tc_cycles;
	uint64_t old_tc,new_tc;
	double clkdiv = Clock_Freq(tmr->clk_pclk) / CycleTimerRate_Get();
	if(!(tmr->tcr &	TCR_ENA) || (tmr->tcr &	TCR_RST)) {
		return;
	}
	cpu_cycles = CycleCounter_Get() - tmr->last_timer_update;	
	tmr->last_timer_update = CycleCounter_Get();
	pc_cycles = (tmr->saved_cycles + cpu_cycles) / clkdiv; 
	tmr->saved_cycles -= pc_cycles * clkdiv;

	tc_cycles = (tmr->pc + pc_cycles) / ((uint64_t)tmr->pr + 1);
	tmr->pc = (tmr->pc + pc_cycles) % ((uint64_t)tmr->pr + 1);	
	old_tc = tmr->tc;
	new_tc = tmr->tc = tmr->tc + tc_cycles;
	for(i=0;i<4;i++) {
		if((old_tc < tmr->mr[i]) && (new_tc >= (uint64_t)tmr->mr[i])) {
			if(((tmr->mcr >> (3*i)) & 7) != 0) {
				do_match_action(tmr,i);
			}
			do_exmatch_action(tmr,i);
		}
	}
}

static void do_event(void *clientData);

/*
 * ---------------------------------------------------------------
 * update_events
 *
 * 	Update the timer to invoke do_event for first event which
 * 	needs to be triggered
 * ---------------------------------------------------------------
 */

static void
update_events(LPCTimer *tmr)
{
	uint32_t timediff,mindiff;
	int enable_timer = 0;
	int i;
	CycleCounter_t sleep_cycles;
	if(!(tmr->tcr &	TCR_ENA) || (tmr->tcr &	TCR_RST)) {
		CycleTimer_Remove(&tmr->event_timer);
		return;
	}
	mindiff = ~0;
	for(i=0;i<4;i++) {
		int action = (tmr->mcr >> (3*i)) & 7; 
		int exmat_action = (tmr->emr >> (EMR_EXMCR0_SHIFT + 2*i)) & 3;
		/* Is there any event enabled ? */ 
		if( action  || exmat_action) {
			timediff = tmr->mr[i] - tmr->tc;
			if(timediff<mindiff) {
				mindiff = timediff;	
			}
			enable_timer = 1;
		}
	}
	sleep_cycles = (uint64_t)mindiff * ((uint64_t)tmr->pr+1) - tmr->pc;  
	sleep_cycles *= CycleTimerRate_Get() / Clock_Freq(tmr->clk_pclk);
	if(enable_timer) {
		CycleTimer_Mod(&tmr->event_timer,sleep_cycles);
	} else {
		CycleTimer_Remove(&tmr->event_timer);
	}
}

static void 
do_event(void *clientData) 
{
	LPCTimer *tmr = (LPCTimer *)clientData;
	actualize_timer(tmr);
	update_events(tmr);
}

/*
 * -----------------------------------------------------------
 * capture
 *	Called when the external Capture line changes
 *	It eventually updates the CR registers and eventually 
 *      triggers an interrupt
 * -----------------------------------------------------------
 */
static void 
capture(SigNode *node,int value,void *clientData)
{
	CaptureTraceInfo *ti = (CaptureTraceInfo *) clientData;
	LPCTimer *tmr = ti->tmr;
	int index = ti->line;
	int oldstatus = tmr->oldCapStatus[index];
	int action = (tmr->ccr >> (3*index)) & 7;
	actualize_timer(tmr);
	if(action & CCR_CAPRISE0) {
		if(((oldstatus == SIG_LOW) || (oldstatus == SIG_PULLDOWN))
			&& ((value == SIG_HIGH) || (value == SIG_PULLUP))) {
			tmr->cr[index] = tmr->tc;
			if(action & CCR_CAPINT0) {
				tmr->ir = tmr->ir | (IR_CR0 << index);
				update_interrupts(tmr);
			}
		}
	}
	if(action & CCR_CAPFALL0) {
		if(((value == SIG_LOW) || (value == SIG_PULLDOWN))
			&& ((oldstatus == SIG_HIGH) || (oldstatus == SIG_PULLUP))) {
			tmr->cr[index] = tmr->tc;
			if(action & CCR_CAPINT0) {
				tmr->ir = tmr->ir | (IR_CR0 << index);
				update_interrupts(tmr);
			}
		}
	}
	tmr->oldCapStatus[index] = value;
}

/*
 * ---------------------------------------------------------------------------
 * Interrupt register
 * 	read: identify interrupt source
 *	write: a one clears the corresponding interrupt request
 * ---------------------------------------------------------------------------
 */
uint32_t
ir_read(void *clientData,uint32_t address,int rqlen)
{
	LPCTimer *tmr = (LPCTimer *) clientData;
	return tmr->ir;
	return 0;	
}
static void
ir_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	LPCTimer *tmr = (LPCTimer *) clientData;
	tmr->ir = tmr->ir & ~value;
	update_interrupts(tmr);
        return;
}
/*
 * --------------------------------------------------------------
 * Timer Control register
 * 	enable / disable or reset counter 
 * --------------------------------------------------------------
 */
static uint32_t
tcr_read(void *clientData,uint32_t address,int rqlen)
{
        fprintf(stderr,"timer register %08x is not implemented\n",address);
	return 0;	
}

static void
tcr_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	LPCTimer *tmr = (LPCTimer *) clientData;
	if(tmr->tcr == value) {
		return;
	}
	actualize_timer(tmr);
	tmr->tcr = value;
	if(value & TCR_RST) {
		/* Should be delayed to next positive clock edge */
		tmr->tc = 0;	
		tmr->pc = 0;	
	}
	update_events(tmr);
        fprintf(stderr,"TCR register write %08x\n",value);
        return;
}

/*
 * ----------------------------------------------------------------
 * Timer counter
 *	incremented when prescaler reaches terminal count
 *	writable ????
 * ----------------------------------------------------------------
 */
uint32_t
tc_read(void *clientData,uint32_t address,int rqlen)
{
	LPCTimer *tmr = (LPCTimer *) clientData;
	actualize_timer(tmr);
	return tmr->tc;	
}
static void
tc_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
        fprintf(stderr,"timer register %08x is not implemented\n",address);
        return;
}

/*
 * ---------------------------------------------------------
 * Prescaler Register:
 * 	Specify maximum for Prescaler counter
 * ---------------------------------------------------------
 */
uint32_t
pr_read(void *clientData,uint32_t address,int rqlen)
{
	LPCTimer *tmr = (LPCTimer *) clientData;
	return tmr->pr;	
}
static void
pr_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	LPCTimer *tmr = (LPCTimer *) clientData;
	actualize_timer(tmr);
	tmr->pr = value;
	update_events(tmr);
        return;
}

/*
 * --------------------------------------------------------
 * Prescale counter
 * 	When prescaler reaches its maximum it is reset
 *	on next pclk and tc is incremented 
 * --------------------------------------------------------
 */
uint32_t
pc_read(void *clientData,uint32_t address,int rqlen)
{
	LPCTimer *tmr = (LPCTimer *) clientData;
	actualize_timer(tmr);
	return tmr->pc;
}

/* 
 * ---------------------------------------------------
 * pc_write
 *	Is the prescaler counter writable ???
 * ---------------------------------------------------
 */
static void
pc_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	LPCTimer *tmr = (LPCTimer *) clientData;
	actualize_timer(tmr);
	// update timers
	//tmr->pc = value;
	// update_events
        return;
}

/* 
 * --------------------------------------------------------------------
 * Match control register
 * 	Determine the action when a counter match is detected	
 * --------------------------------------------------------------------
 */

uint32_t
mcr_read(void *clientData,uint32_t address,int rqlen)
{
	LPCTimer *tmr = (LPCTimer *) clientData;
	return tmr->mcr;
}

static void
mcr_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	LPCTimer *tmr = (LPCTimer *) clientData;
	actualize_timer(tmr);
	tmr->mcr = value;
	update_events(tmr);
        return;
}

/*
 * ---------------------------------------------
 * Match register read
 * ---------------------------------------------
 */
uint32_t
mr_read(void *clientData,uint32_t address,int rqlen)
{
	LPCTimer *tmr = (LPCTimer *) clientData;
	unsigned int index = ((address & 0x1f)-TMR_MR0)>>2;
	if(index>3) {
		fprintf(stderr,"Emulator bug in timer: illegal mr index %d\n",index);
		return 0;
	}
	return tmr->mr[index];	
}
static void
mr_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	LPCTimer *tmr = (LPCTimer *) clientData;
	unsigned int index = ((address & 0x1f)-TMR_MR0)>>2;
	if(index>3) {
		fprintf(stderr,"Emulator bug in timer: illegal mr index %d\n",index);
		return;
	}
	actualize_timer(tmr);
	tmr->mr[index]=value;	
	update_events(tmr);
}

/*
 * ----------------------------------------------------------------------
 * Capture control register
 *	Determine the action when edge of capture input is detected
 * ----------------------------------------------------------------------
 */
uint32_t
ccr_read(void *clientData,uint32_t address,int rqlen)
{
	LPCTimer *tmr = (LPCTimer *) clientData;
	return tmr->ccr;	
}

static void
ccr_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	LPCTimer *tmr = (LPCTimer *) clientData;
	actualize_timer(tmr);
	tmr->ccr = value;
	update_events(tmr);
        return;
}

/*
 * ------------------------------------------------------------------------
 *  Capture register
 *	Loaded with the timer counter value when a matching event occurs
 * ------------------------------------------------------------------------
 */
uint32_t
cr_read(void *clientData,uint32_t address,int rqlen)
{
	LPCTimer *tmr = (LPCTimer *) clientData;
	unsigned int index = ((address & 0x1f)-TMR_CR0)>>2;
	if(index>3) {
		fprintf(stderr,"Emulator bug in timer: illegal mr index %d\n",index);
		return 0;
	}
	/* actualize_timer(tmr);   */
	return tmr->cr[index];	
}

/* 
 * -----------------------------------------------
 * cr_write 
 *	Is this writable at all ?????
 * -----------------------------------------------
 */
static void
cr_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	//LPCTimer *tmr = (LPCTimer *) clientData;
	unsigned int index = ((address & 0x1f)-TMR_CR0)>>2;
	if(index>3) {
		fprintf(stderr,"Emulator bug in timer: illegal mr index %d\n",index);
		return;
	}
	//tmr->cr[index]=value;	
	return;
}

/*
 * -----------------------------------------------------------------
 * External match register
 * 	Determines the signal on the external match outputs
 * -----------------------------------------------------------------
 */
uint32_t
emr_read(void *clientData,uint32_t address,int rqlen)
{
	LPCTimer *tmr = (LPCTimer *) clientData;
	return tmr->emr;	
}

static void
emr_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	LPCTimer *tmr = (LPCTimer *) clientData;
	/* Not sure if lower for bits can be written */
	tmr->emr = (tmr->emr & 0xf) | (value & ~0xf);
        return;
}

static void 
Tmr_Map(void *owner,uint32_t base,uint32_t mask,uint32_t flags) {
	LPCTimer *tmr = (LPCTimer *) owner;
	IOH_New32(base+TMR_IR,ir_read,ir_write,tmr);
	IOH_New32(base+TMR_TCR,tcr_read,tcr_write,tmr);
	IOH_New32(base+TMR_TC,tc_read,tc_write,tmr);
	IOH_New32(base+TMR_PR,pr_read,pr_write,tmr);
	IOH_New32(base+TMR_PC,pc_read,pc_write,tmr);
	IOH_New32(base+TMR_MCR,mcr_read,mcr_write,tmr);
	IOH_New32(base+TMR_MR0,mr_read,mr_write,tmr);
	IOH_New32(base+TMR_MR1,mr_read,mr_write,tmr);
	IOH_New32(base+TMR_MR2,mr_read,mr_write,tmr);
	IOH_New32(base+TMR_MR3,mr_read,mr_write,tmr);
	IOH_New32(base+TMR_CCR,ccr_read,ccr_write,tmr);
	IOH_New32(base+TMR_CR0,cr_read,cr_write,tmr);
	IOH_New32(base+TMR_CR1,cr_read,cr_write,tmr);
	IOH_New32(base+TMR_CR2,cr_read,cr_write,tmr);
	IOH_New32(base+TMR_CR3,cr_read,cr_write,tmr);
	IOH_New32(base+TMR_EMR,emr_read,emr_write,tmr);
}

static void
Tmr_UnMap(void *owner,uint32_t base,uint32_t mask)
{
	uint32_t i;
	for(i=0;i<0x20;i+=4) {
		IOH_Delete32(base+i);
	}
}
  

BusDevice *
LPC2106Timer_New(const char *name) 
{
	LPCTimer *tmr = sg_new(LPCTimer);
	int i;
	tmr->irqNode = SigNode_New("%s.irq",name);
	if(!tmr->irqNode) {
		fprintf(stderr,"Cannot create irqNode\n");	
		exit(7);
	}
	for(i=0;i<4;i++) {
		tmr->matNode[i] = SigNode_New("%s.mat%d",name,i);
		tmr->capNode[i] = SigNode_New("%s.cap%d",name,i); 
		if(!tmr->capNode[i] || ! tmr->matNode[i]) {
			fprintf(stderr,"Cannot create cap/mat node %d\n",i);
			exit(8);
		}
	}
	for(i=0;i<4;i++) {
		CaptureTraceInfo *ti = &tmr->capTi[i];
		ti->tmr = tmr;
		ti->line = i;
		SigNode_Trace(tmr->capNode[i],capture,ti); 
		SigNode_Set(tmr->matNode[i],SIG_LOW);
	}
	tmr->bdev.first_mapping=NULL;
        tmr->bdev.Map=Tmr_Map;
        tmr->bdev.UnMap=Tmr_UnMap;
        tmr->bdev.owner=tmr;
        tmr->bdev.hw_flags=MEM_FLAG_WRITABLE|MEM_FLAG_READABLE;
	tmr->clk_pclk = Clock_New("%s.pclk",name);
	CycleTimer_Init(&tmr->event_timer,do_event,tmr);
	fprintf(stderr,"LPC2106 Timer \"%s\" created\n",name);
	return &tmr->bdev;	
}
