/*
 *************************************************************************************************
 *
 * Emulation of the NS9750 System Controller 
 *
 * Copyright 2004 Jochen Karrer. All rights reserved.
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
#include <string.h>
#include <unistd.h>

#include "arm9cpu.h"
#include "cycletimer.h"
#include "bus.h"
#include "ns9750_timer.h"
#include "configfile.h"
#include "signode.h"
#include "senseless.h"
#include "sgstring.h"
#if 0 
#define dbgprintf(x...) { fprintf(stderr,x); }
#else
#define dbgprintf(x...)
#endif

#define clearbit(bitnr,x) ((x)=(x)&~(1<<(bitnr)))
#define setbit(bitnr,x) ((x)=(x)|(1<<(bitnr)))
#define testbit(bitnr,x) ((x)&(1<<(bitnr)))

static int extirqs[]={IRQ_EXTERNAL_0,IRQ_EXTERNAL_1,IRQ_EXTERNAL_2,IRQ_EXTERNAL_3};
uint32_t irq_LR_stack[32];
uint32_t irq_CPSR_stack[32];

/*
 * ffnz = Find First nonzero Bit in word. Undefined if no zero exists,
 * so code should check against ~0U first..
 */
static inline unsigned long 
ffnz(unsigned long word)
{
        int k;

        k = 31;
        if (word & 0x0000ffff) { k -= 16; word <<= 16; }
        if (word & 0x00ff0000) { k -= 8;  word <<= 8;  }
        if (word & 0x0f000000) { k -= 4;  word <<= 4;  }
        if (word & 0x30000000) { k -= 2;  word <<= 2;  }
        if (word & 0x40000000) { k -= 1; }
        return k;
}

typedef struct Timer {
	int nr;
	uint32_t trcv; // reload count
	uint32_t trr;  // timer read
	uint32_t tcr;  // timer control
	CycleCounter_t last_update;
	CycleCounter_t saved_cycles;
	CycleTimer irq_timer;
} Timer;

typedef struct Sysco  Sysco;

typedef struct Irq_TraceInfo {
	struct Sysco *sysco;	
	int irq;
	int irq_index;
} Irq_TraceInfo;

struct Sysco {
	Timer *timer[16];
	int irq_posted;
	int fiq_posted;
	uint16_t sys_tis;
	int	 srcirq_is_posted[32]; /* this is the primary storage for incoming irqs */
	uint32_t sys_israw;		/* translated by intid ? */
	uint32_t sys_ivarv[32]; // Interrupt Vectors
	uint8_t  sys_icfg[32];
	uint32_t sys_intid;
	// mapping table between interrupt sources and interrupt ids
	uint32_t intid_map[32];

	uint32_t irqmask;	  // internal only mask of vectored Interruptcontroller
	uint32_t old_irqmask[32]; // irqmask before an interrupt was entered
	uint32_t old_intid[32];

	/* Interrupt enable registers are connected to the Source Interrupt inputs, not to INTID ! */
	uint32_t src_ienmask;	 
	uint32_t src_fienmask;

	/* shadow registers of src_ienmask and src_fienmask, remapped by interruptcontroller */
	uint32_t out_ienmask;
	uint32_t out_fienmask;

	uint32_t wdg_config;
	uint32_t wdg_counter;
	CycleCounter_t wdg_last_update;
	CycleCounter_t wdg_remaining_cycles;
	CycleTimer wdg_timer;

	uint32_t clkcfg;
	uint32_t modrst;
	uint32_t pll_config;
	uint32_t misc_config;
	uint32_t genid;
	uint32_t extint[4];
	int	 extint_posted[4];
	int	 extint_oldsts[4]; /* for edge detection */
	/* interrupts */
	SigNode *irqNode[32];
	Irq_TraceInfo irqTraceInfo[32];
	SigNode  *extIrqNode[4];
	SigTrace *extIrqTrace[4];
	Irq_TraceInfo extIrqTraceInfo[4];

	uint32_t gcfg;
	uint32_t brc[4];
	uint32_t ahb_errmonconf;
	uint32_t ahb_arbtout;
	uint32_t ahb_errstat1;
	uint32_t ahb_errstat2;
};

Sysco *sysco;

static void
update_irqs_pending(Sysco *sysco) {
	if(sysco->sys_israw & sysco->out_ienmask & sysco->irqmask) {
		if(!sysco->irq_posted) {
			ARM_PostIrq();	
			sysco->irq_posted=1;
		} 
	} else {
		if(sysco->irq_posted) {
			ARM_UnPostIrq();	
			sysco->irq_posted=0;
		}
	}
	if(sysco->sys_israw & sysco->out_fienmask & sysco->irqmask) {
		if(!sysco->fiq_posted) {
			ARM_PostFiq();	
			sysco->fiq_posted=1;
		}
	} else {
		if(sysco->fiq_posted) {
			ARM_UnPostFiq();	
			sysco->fiq_posted=0;
		}
	}
}

int 
Sysco_PostIrq(int isource_nr) {
	int intid;
	int result=0;
	intid = sysco->intid_map[isource_nr];	
	setbit(intid,sysco->sys_israw);
	sysco->srcirq_is_posted[isource_nr]=1;
	dbgprintf("Posting an irq\n");
	if(sysco->sys_israw & sysco->out_ienmask & sysco->irqmask) {
		if(!sysco->irq_posted) {
			dbgprintf("Posting really\n");
			ARM_PostIrq();	
			sysco->irq_posted=1;
		} else {
			dbgprintf("Alread Posted\n");
		}
		result=1;
	} else {
			dbgprintf("Nothing to post, raw %x ien %x irqm %x\n",sysco->sys_israw,sysco->out_ienmask,sysco->irqmask);
	}
	if(sysco->sys_israw & sysco->out_fienmask & sysco->irqmask) {
		if(!sysco->fiq_posted) {
			ARM_PostFiq();	
			sysco->fiq_posted=1;
		}
		result=1;
	}
	return result;
}

void 
Sysco_UnPostIrq(int isource_nr) {
	int intid;
	intid=sysco->intid_map[isource_nr];	
	clearbit(intid,sysco->sys_israw);
	sysco->srcirq_is_posted[isource_nr]=0;
	dbgprintf("Unposte einen irq\n");
	if(!(sysco->sys_israw & sysco->out_ienmask & sysco->irqmask)) {
		if(sysco->irq_posted) {
			ARM_UnPostIrq();	
			sysco->irq_posted=1;
		}
	}
	if(!(sysco->sys_israw & sysco->out_fienmask & sysco->irqmask)) {
		if(sysco->fiq_posted) {
			ARM_UnPostFiq();	
			sysco->fiq_posted=0;
		}
	}
}

static void 
irq_trace_proc(struct SigNode * node,int value, void *clientData)
{
	Irq_TraceInfo *irqInfo = clientData; 
	//Sysco *sysco = irqInfo->sysco;
	int isource_nr = irqInfo->irq_index;
	if(value == SIG_HIGH) {
		Sysco_UnPostIrq(isource_nr); 
	} else {
		Sysco_PostIrq(isource_nr); 
	}
}
/* 
 * --------------------------------
 * Timer Read Register 
 * --------------------------------
 */
void 
update_trr(Timer *timer) {
	uint32_t tcr=timer->tcr;
	uint32_t tlcs;
	uint32_t tm;
	uint32_t down;
	uint32_t reload;
	unsigned int shift;
	uint32_t timer32;
	uint64_t count;
	if(!(tcr & SYS_TCR_TEN)) {
		return;
	}
	dbgprintf("Update TRR, timer %d\n",timer->nr);
	tlcs = ((tcr & SYS_TCR_TLCS) >> SYS_TCR_TLCS_SHIFT);
	if(tlcs==7) {
		fprintf(stderr,"External Pulse Events not implemented\n");
		return;
	} else {
		shift = tlcs;
	}
	tm = ((tcr & SYS_TCR_TM)>>SYS_TCR_TM_SHIFT);
	down = tcr & SYS_TCR_UDS;
	timer32 = tcr & SYS_TCR_TSZ;
	reload = tcr & SYS_TCR_REN;
	if(!timer32) {
		fprintf(stderr,"16 Bit Counters not implemented\n");
	}
	switch(tm) {
		case 0:
			timer->saved_cycles += CycleCounter_Get() - timer->last_update;
			count = timer->saved_cycles >> shift;
			timer->saved_cycles -= count << shift;
			if(down) {
				count = timer->trr - count;
			} else {
				count = timer->trr + count;
			}
			if(count & 0xffffffff00000000LL) {
				if(!reload) {
					if(down) {
						count = 0;
					} else {
						count =0xffffffff;
					}

				} else {
					if(down) {
						// underflow on down
						uint32_t div = timer->trcv;
						uint32_t n;
						if(div!=0) {
							n=1+~count/div;
							count=count+n*div;
						} else {
							count=0;
						}
					} else {
						// overflow on up
						uint32_t div = 0xffffffff-timer->trcv;	
						int n;
						if(div!=0) {
							n=count/div;
							count=-(count-n*div);
						} else {
							count=0xffffffff;
						}
					}
				}	
			}
			timer->trr = count;
			break;
			
		default:
			fprintf(stderr,"Timer mode %d not implemented\n",tm);
	}
	timer->last_update=CycleCounter_Get();
}

/*
 * -------------------------------------
 * Timer Reload count registers
 * -------------------------------------
 */
static void 
timer_trcv_write(void *cd,uint32_t value,uint32_t address,int rqlen) {
	Timer *timer=(Timer*)cd;
	update_trr(timer);
	timer->trcv=value;
	dbgprintf("New reload count of %08x for timer %u\n",value,timer->nr);
	return;
}

static uint32_t 
timer_trcv_read(void *cd,uint32_t address,int rqlen) {
	Timer *timer=(Timer*)cd;
	dbgprintf("Read reload count %08x\n",timer->trcv);
	return timer->trcv;
}
/*
 * -----------------------------------------------
 * return CPU cycles until a timer times out 
 * -----------------------------------------------
 */
static uint64_t 
calculate_remaining(Timer *timer) {
	uint32_t tcr=timer->tcr;
	uint32_t down=tcr&SYS_TCR_UDS;
	uint32_t timer32=tcr&SYS_TCR_TSZ;
	uint32_t ticks;	
	uint32_t tlcs;
	if(timer32) {
		if(down) {
			ticks=timer->trr;
		} else {
			ticks=~timer->trr;
		}
	} else {
		if(down) {
			ticks=timer->trr;
		} else {
			ticks=(~timer->trr)&0xffff;
		}

	}
	tlcs=((tcr&SYS_TCR_TLCS)>>SYS_TCR_TLCS_SHIFT);
	if(tlcs==7) {
		fprintf(stderr,"External Pulse Events not implemented\n");
		return 0;
	}
	ticks+=1;
//	fprintf(stderr,"ticks is %d, trr is %d\n",ticks,timer->trr);
	return ticks << tlcs;
}

static uint32_t 
timer_trr_read(void *cd,uint32_t address,int rqlen) {
	Timer *timer=(Timer*)cd;
	update_trr(timer);
	Senseless_Report(200);
	dbgprintf("Timer Read Register %08x\n",timer->trr);
	return timer->trr;
}

/*
 * ---------------------------------------------------------------------
 * Timer Control register
 * ---------------------------------------------------------------------
 */

/* translate timer number to raw interrupt nr */
static int israw_bit[16]={16,17,18,19,20,21,22,23,24,24,25,25,26,26,27,27};

static void 
timer_irq(void *cd) {
	
	Timer *timer=(Timer*)cd;
	uint32_t tcr=timer->tcr;
	uint32_t reload=tcr&SYS_TCR_REN;
	dbgprintf("Ein Timer IRQ for %u\n",timer->nr);
	if(reload) {
		update_trr(timer);
	} else {
		fprintf(stderr,"No reload\n");
	}
	// set the bits  global Timer Interrupt status register 
	sysco->sys_tis|=(1<<timer->nr);
	Sysco_PostIrq(israw_bit[timer->nr]);
}

#define TCR_NEED_UPDATE_MASK (SYS_TCR_TEN|SYS_TCR_TLCS|SYS_TCR_TM|SYS_TCR_INTS \
		|SYS_TCR_UDS|SYS_TCR_TSZ|SYS_TCR_REN)
static void 
timer_tcr_write(void *cd,uint32_t new_tcr,uint32_t address,int rqlen) {
	uint64_t remaining;
	Timer *timer=(Timer*)cd;
	uint32_t old_tcr=timer->tcr;
	uint32_t diff_tcr = old_tcr ^ new_tcr;
	int new_tlcs;

	dbgprintf("New Timer Control of %08x\n",new_tcr);

	if((diff_tcr & TCR_NEED_UPDATE_MASK)) { 
		update_trr(timer);
	}
	if((new_tcr & SYS_TCR_INTC) && !(old_tcr & SYS_TCR_INTC)) {
		sysco->sys_tis &= ~(1<<timer->nr);
		Sysco_UnPostIrq(israw_bit[timer->nr]);
		if(!CycleTimer_IsActive(&timer->irq_timer)) {
			remaining=calculate_remaining(timer);
			CycleTimer_Add(&timer->irq_timer,remaining,timer_irq,timer);
		}
	}
	if((new_tcr&SYS_TCR_INTS)) {
		if(!(old_tcr&SYS_TCR_INTS)) {
			remaining=calculate_remaining(timer);
			CycleTimer_Remove(&timer->irq_timer);
			CycleTimer_Add(&timer->irq_timer,remaining,timer_irq,timer);
			dbgprintf("Started Timer, remaining %08llx\n",remaining);
		}
	} else {
		dbgprintf("Removed Timer, remaining\n");
		CycleTimer_Remove(&timer->irq_timer);
	}
	new_tlcs = ((new_tcr & SYS_TCR_TLCS) >> SYS_TCR_TLCS_SHIFT);
	if(new_tlcs < 7) {
		timer->saved_cycles  &= ((1 << new_tlcs) - 1);
	}
	timer->tcr = new_tcr;
	return;
}

/* Timer Controller Register Read */
static uint32_t 
timer_tcr_read(void *cd,uint32_t address,int rqlen) {
	Timer *timer=(Timer*)cd;
	return timer->tcr;
}

/* Timer Interrupt Status Register Read */
static uint32_t 
timer_tis_read(void *cd,uint32_t address,int rqlen) {
	dbgprintf("Read TIS %08x\n",sysco->sys_tis);
	return sysco->sys_tis;
}

/* System Controller Raw Interrupt status read */
static uint32_t 
sys_israw_read(void *cd,uint32_t address,int rqlen) {
	return sysco->sys_israw;
}

/*
 * ------------------------------------------------------------------
 * Update the watchdog timer value whenever it is read or when
 * clock source changes
 * ------------------------------------------------------------------
 */
static void 
update_wdg_counter(Sysco *sysco) {
	uint32_t clock_select;
	int shift=0;
	int remainder=0;
	CycleCounter_t wd_cycles=0,cpu_cycles,now;
	if(!(sysco->wdg_config & SWDG_SWWE)) {
		return;
	}
	clock_select = SWDG_SWTCS(sysco->wdg_config);
	now= CycleCounter_Get();
	cpu_cycles = now - sysco->wdg_last_update;
	switch(clock_select) {
		case 0:
		case 1:
		case 2:
		case 3:
		case 4:
		case 5:
			shift=(clock_select+1);
			wd_cycles = cpu_cycles >> shift;
			remainder=cpu_cycles-(wd_cycles<<shift);
			break;
		case 6:
		case 7:
			fprintf(stderr,"Illegal watchdog timer clock source\n");
			break;
	}
	if(wd_cycles > sysco->wdg_counter) {
		sysco->wdg_counter=0;
	} else {
		sysco->wdg_counter -= wd_cycles;
	}
	sysco->wdg_last_update=now-remainder;
}

static void
wdg_calculate_remaining(Sysco *sysco) {
	uint32_t clock_select = SWDG_SWTCS(sysco->wdg_config);
	int shift=0;
	switch(clock_select) {
		case 0:
		case 1:
		case 2:
		case 3:
		case 4:
		case 5:
			shift=(clock_select+1);
			break;
		case 6:
		case 7:
			fprintf(stderr,"Illegal watchdog timer clock source\n");
			break;
	}
	sysco->wdg_remaining_cycles = ((uint64_t)sysco->wdg_counter) << shift; 
}

static void
watchdog_timeout(void *cd) {
	Sysco *sysco = cd;
	sysco->wdg_counter=0;
	if(!(sysco->wdg_config & SWDG_SWWE)) {
		fprintf(stderr,"Emulator bug: Watchdog timeout on disabled watchdog\n");
		return;
	}
	if(sysco->wdg_config & SWDG_SWWIC) {
		fprintf(stderr,"Watchdog Reset\n");
		exit(4323);
	} else {
		Sysco_PostIrq(IRQ_WATCHDOG_TIMER); 
	}  
}
/*
 * ---------------------------------------------------------------
 * Update the Watchdog Cycle timer 
 * ---------------------------------------------------------------
 */

static uint32_t 
swdgcfg_read(void *cd,uint32_t address,int rqlen) {
	Sysco *sysco = cd;
	return sysco->wdg_config;
}

static void 
swdgcfg_write(void *cd,uint32_t value,uint32_t address,int rqlen) {
	Sysco *sysco = cd;
	uint32_t diff;
	value = value | (sysco->wdg_config & SWDG_SWWE);
	diff=sysco->wdg_config ^ value;	
	if(sysco->wdg_config==value) {
		return;
	}	
	update_wdg_counter(sysco);
	if(diff & SWDG_SWWE) {
		CycleTimer_Remove(&sysco->wdg_timer);
		wdg_calculate_remaining(sysco);
		CycleTimer_Add(&sysco->wdg_timer,sysco->wdg_remaining_cycles,watchdog_timeout,sysco);
	}
	if((diff & SWDG_SWWI) && (value & SWDG_SWWI))  {
		Sysco_UnPostIrq(IRQ_WATCHDOG_TIMER); 
	}
	sysco->wdg_config=value;
	return;
}

	
static uint32_t 
swdgtmr_read(void *cd,uint32_t address,int rqlen) {
	Sysco *sysco = cd;
	update_wdg_counter(sysco);
	return sysco->wdg_counter;
}

static void 
swdgtmr_write(void *cd,uint32_t value,uint32_t address,int rqlen) {
	Sysco *sysco = cd;
	sysco->wdg_counter=value;
	sysco->wdg_last_update=CycleCounter_Get();
	CycleTimer_Remove(&sysco->wdg_timer);
	if(sysco->wdg_config & SWDG_SWWE) {
		wdg_calculate_remaining(sysco);
		CycleTimer_Add(&sysco->wdg_timer,sysco->wdg_remaining_cycles,watchdog_timeout,sysco);
	}
	return;
}

/*
 * ------------------------------------------------------
 * Interrupt status active is Read only
 * and shows all active (unmasked) Interrupt requests	
 * ------------------------------------------------------
 */
static uint32_t 
sys_isa_read(void *cd,uint32_t address,int rqlen) {
	return sysco->sys_israw & (sysco->out_ienmask|sysco->out_fienmask) & sysco->irqmask;
}
/*
 * -----------------------------------------------------
 * Interrupt Service routine Address  
 * 	Read: Mask the current Interrupt and all lower
 *	Priority Interrupts
 * -----------------------------------------------------
 */ 
static uint32_t 
sys_isra_read(void *cd,uint32_t address,int rqlen) {
	uint32_t pending = sysco->sys_israw & (sysco->out_ienmask|sysco->out_fienmask) & sysco->irqmask;
	int intid;
	dbgprintf("Read ISRA\n");
	if(!pending) {
		//fprintf(stderr,"Warning: Read ISRA without pending interrupts\n");
		return 0;
	}
	// Lowest Interrupt id has the highest priority
	intid=ffnz(pending);
	if(sysco->old_irqmask[intid]!=~0U) {
		fprintf(stderr,"Bug, recursion in interrupt %d, lr %08x, old irqmask %08x\n",intid,REG_LR,sysco->old_irqmask[intid]);	
		exit(4324);
	}
	sysco->old_intid[intid]=sysco->sys_intid;
	sysco->old_irqmask[intid]=sysco->irqmask;
	if(intid==0) {
		sysco->irqmask=0;
	} else {
		sysco->irqmask=~0U >>	(32-intid);
	}
	if(intid == sysco->sys_intid) {
		fprintf(stderr,"Warning, recursion in interrupts\n");
		exit(4324);
	}
	sysco->sys_intid=intid;
	update_irqs_pending(sysco);
	return sysco->sys_ivarv[intid];
}
/*
 * -----------------------------------------------------
 * Interrupt Service routine Address  
 * Write: Any Value 
 *        UnMask the current Interrupt and all lower
 * -----------------------------------------------------
 */ 
static void 
sys_isra_write(void *cd,uint32_t value,uint32_t address,int rqlen) {
	int irq=sysco->sys_intid;
	if(irq<32) {
		if(!testbit(irq,sysco->irqmask)) {
			sysco->irqmask=sysco->old_irqmask[irq];
			sysco->sys_intid=sysco->old_intid[irq];
			sysco->old_irqmask[irq]=~0U;
			update_irqs_pending(sysco);
		} else {
			fprintf(stderr,"Warning irq acknowledged but not pending\n");
		}
	} else {
		fprintf(stderr,"Ack Interrupt but none pending %08x\n",ARM_NIA);
	}
	return;
}
	
/*
 * ---------------------------------------------------------
 * Interrupt Vector Address Register Values (IVARV)
 * ---------------------------------------------------------
 */
static uint32_t 
sys_ivarv_read(void *cd,uint32_t address,int rqlen) {
	int nr=(address-SYS_IVARV(0))>>2;
	return sysco->sys_ivarv[nr]; 
}

static void 
sys_ivarv_write(void *cd,uint32_t value,uint32_t address,int rqlen) {
	int nr=(address-SYS_IVARV(0))>>2;
	sysco->sys_ivarv[nr]=value; 
	return;
}

/*
 * ---------------------------------------
 * Interrupt Configuration Registers
 * ---------------------------------------
 */
static uint32_t 
sys_icfg_read(void *cd,uint32_t address,int rqlen) {
	int nr=(address&~3)-SYS_ICFG_BASE;
	int i;
	uint32_t result=0;
	if((nr>28) || (rqlen != 4)) {
		fprintf(stderr,"Illegal access to icfg register addr %08x,rqlen %d\n",address,rqlen);
		return 0;
	}
	for(i=nr;i<nr+4;i++) {
		result=(result<<8) | sysco->sys_icfg[i];
	}
	//fprintf(stderr,"icfg read %08x value %08x\n",address,*value);
	return result;
}

/*
 * 
 */
static void
update_out_ienmasks(Sysco *sysco) {
	int src_irq;
	sysco->out_ienmask=0;
	sysco->out_fienmask=0;
	for(src_irq=0;src_irq<32;src_irq++) {
		int intid=sysco->intid_map[src_irq];		
		if(testbit(src_irq,sysco->src_ienmask)) {
			setbit(intid,sysco->out_ienmask);	
		}
		if(testbit(src_irq,sysco->src_fienmask)) {
			setbit(intid,sysco->out_fienmask);	
		}
	}
	update_irqs_pending(sysco);
}

static void
update_israw(Sysco *sysco) {
	int src_irq;
	sysco->out_ienmask=0;
	sysco->out_fienmask=0;
	sysco->sys_israw=0;
	for(src_irq=0;src_irq<32;src_irq++) {
		int intid=sysco->intid_map[src_irq];		
		if(sysco->srcirq_is_posted[src_irq]) {
			setbit(intid,sysco->sys_israw);
		}
	}
	update_irqs_pending(sysco);
}

/*
 * ------------------------------------------------------------
 * Should not allow byte access, because real cpu doesn't
 * ------------------------------------------------------------
 */
static void 
sys_icfg_write(void *cd,uint32_t value,uint32_t address,int rqlen) {
	uint32_t diff;
	int source_irq;
	int intid;
	//fprintf(stderr,"ICFG write\n");
	if(rqlen>1) {
		dbgprintf("RQLEN :%u\n",rqlen);
		sys_icfg_write(cd,value>>8,address+1,rqlen-1);
	}
	source_irq=(address-SYS_ICFG_BASE)^3;
	if(source_irq>31) {
		fprintf(stderr,"Illegal access to ICFG-Register,address %08x,int rqlen %d\n",address,rqlen);
		exit(342);
	}
	intid=value&SYS_ICFG_ISD_MASK;
	diff=sysco->sys_icfg[source_irq]^value;
	dbgprintf("Write ICFG %d with value 0x%02x old 0x%02x\n",source_irq,value&0xff,sysco->sys_icfg[source_irq]); // jk	
	sysco->sys_icfg[source_irq]=value;
	if(diff&SYS_ICFG_IE_MASK) {
		if(value&SYS_ICFG_IT_FIQ) {
			if(value&SYS_ICFG_IE_MASK) {
				setbit(source_irq,sysco->src_fienmask);
				setbit(intid,sysco->out_fienmask);
			} else {
				clearbit(source_irq,sysco->src_fienmask);
				clearbit(intid,sysco->out_fienmask);
			}
			
		} else {
			if(value&SYS_ICFG_IE_MASK) {
				if(!testbit(intid,sysco->irqmask)) {
					fprintf(stderr,"Unmask IRQ while not acknowledged at %08x\n",ARM_NIA);
				}
				dbgprintf("IEN for %u\n",source_irq);
				setbit(source_irq,sysco->src_ienmask);
				setbit(intid,sysco->out_ienmask);
			} else {
				clearbit(source_irq,sysco->src_ienmask);
				clearbit(intid,sysco->out_ienmask);
			}
		}
//		fprintf(stderr,"src_ienmask %08x\n",sysco->src_ienmask);
		update_irqs_pending(sysco);
	} 
	if(sysco->intid_map[source_irq]!=intid) {
		dbgprintf("Map %u to intid %u\n",source_irq,intid);
		sysco->intid_map[source_irq]=intid;
		update_out_ienmasks(sysco);
		update_israw(sysco);
	}
	return;
}
/*
 * ------------------------------------------------------
 * Return the number of the current active Interrupt
 * ------------------------------------------------------
 */
static uint32_t 
sys_intid_read(void *cd,uint32_t address,int rqlen) {
//	printf("Read INTID %d\n",*data);
	return sysco->sys_intid;	
}

static uint32_t 
sys_misc_config_read(void *cd,uint32_t address,int rqlen) {
	Sysco *sysco = cd;
	return sysco->misc_config; 
}

static void 
sys_misc_config_write(void *cd,uint32_t value,uint32_t address,int rqlen) {
	fprintf(stderr,"warning: Write to misc config not implemented\n");
	sysco->misc_config = (sysco->misc_config & ~(MISCCFG_ENDM | MISCCFG_MCCM)) 
		| (value & (MISCCFG_ENDM | MISCCFG_MCCM));
	return;
}
static uint32_t 
sys_pll_config_read(void *cd,uint32_t address,int rqlen) {
	Sysco *sysco = cd;
	return sysco->pll_config; 
}

static void 
sys_pll_config_write(void *cd,uint32_t value,uint32_t address,int rqlen) {
	fprintf(stderr,"Warning: Write to pll config not implemented\n");
	sleep(1);
	return;
}

static uint32_t 
clkcfg_read(void *cd,uint32_t address,int rqlen) {
	Sysco *sysco=cd;
	return sysco->clkcfg;
}

static void 
clkcfg_write(void *cd,uint32_t value,uint32_t address,int rqlen) {
	Sysco *sysco=cd;
	sysco->clkcfg=value;
	fprintf(stderr,"Warning Clock config write does nothing\n");
	// update mappings
	return;
}

static uint32_t 
modrst_read(void *cd,uint32_t address,int rqlen) {
	Sysco *sysco=cd;
	return sysco->modrst;
}

static void 
modrst_write(void *cd,uint32_t value,uint32_t address,int rqlen) {
	Sysco *sysco=cd;
	sysco->modrst=value;
	fprintf(stderr,"Warning sleep control write does nothing\n");
	// update mapping
	return;
}

static void 
init_genid(Sysco *sysco) {
	char gpio_name[20];
	int32_t result;
	int i;
	uint32_t genid=0xffffffff;

	static uint8_t gpios[]={1,3,5,6, 7,9,11,13, 14,15,16,18, 21,22,23,25, 26,27,28,29, 
			30,31,32,33, 34,35,36,37, 38,39,40,41};
	for(i=0;i<32;i++) {
		sprintf(gpio_name,"gpio%d",gpios[i]);
		if(Config_ReadInt32(&result,"ns9750",gpio_name)>=0) {
			genid = (genid & ~(1<<i)) | ((result & 1)<<i);	
		}
	}
	sysco->genid=genid;
	fprintf(stderr,"Initial GENID register: %08x\n",genid);
}

static uint32_t 
genid_read(void *cd,uint32_t address,int rqlen) {
	Sysco *sysco=cd;
	return sysco->genid;
}

static void 
genid_write(void *cd,uint32_t value,uint32_t address,int rqlen) {
	fprintf(stderr,"GENID is not writable\n");
	return;
}

/*
 * ---------------------------------------------------------------
 *
 * ---------------------------------------------------------------
 */
static uint32_t 
extint_read(void *cd,uint32_t address,int rqlen) {
	uint32_t index=(address-SYS_EXTINT0)>>2;
	if(index>3) {
		return -1;
	}
	return sysco->extint[index];
}

static void 
extint_write(void *cd,uint32_t value,uint32_t address,int rqlen) {
	Sysco *sc = cd;
	uint32_t index=(address-SYS_EXTINT0)>>2;
	uint32_t diff; 
	int irq;
	if(index>3) { 
		return;
	}
	diff = sysco->extint[index] ^ value;
	/* Hack ! should be update_interrupts */
	if(diff & (EXTINT_LVEDG | EXTINT_PLTY)) {
		irq=extirqs[index];
		Sysco_UnPostIrq(irq);
		sc->extint_posted[index]=0;
	}
	if((value & EXTINT_CLR) && (value & EXTINT_LVEDG)) {
		if(sc->extint_posted[index]) {
			irq=extirqs[index];
			Sysco_UnPostIrq(irq);
			sc->extint_posted[index]=0;
		}
	}
	sysco->extint[index]=value & ~EXTINT_CLR; /* ????? not verified with real device ! */
	return;
}

static uint32_t
ahb_brc_read(void *cd,uint32_t address,int rqlen) {
	Sysco *sysco=cd;
	uint32_t brcnr=(address-SYS_AHB_BRC0)>>2;
	if(brcnr>3)
		return -1;
	return sysco->brc[brcnr];
}

static void 
ahb_brc_write(void *cd,uint32_t value,uint32_t address,int rqlen) {
	Sysco *sysco=cd;
	uint32_t brcnr=(address-SYS_AHB_BRC0)>>2;
	if(brcnr>3) {
		return;
	}
	sysco->brc[brcnr]=value;
	return;
}

static uint32_t 
ahb_arbtout_read(void *cd,uint32_t address,int rqlen) {
	Sysco *sysco=cd;
	return sysco->ahb_arbtout;
}

static void 
ahb_arbtout_write(void *cd,uint32_t value,uint32_t address,int rqlen) {
	Sysco *sysco=cd;
	sysco->ahb_arbtout=value;
	return;
}

static uint32_t 
ahb_errstat1_read(void *cd,uint32_t address,int rqlen) {
	Sysco *sysco=cd;
	uint32_t data = sysco->ahb_errstat1;
	sysco->ahb_errstat1=0; // reset on read
	return data;
}
static void 
ahb_errstat1_write(void *cd,uint32_t value,uint32_t address,int rqlen) {
	fprintf(stderr,"AHB errstat1 is not writable\n");
	return;
}

static uint32_t 
ahb_errstat2_read(void *cd,uint32_t address,int rqlen) {
	Sysco *sysco=cd;
	uint32_t data = sysco->ahb_errstat2;
	sysco->ahb_errstat2=0; // reset on read
	return data;
}
static void 
ahb_errstat2_write(void *cd,uint32_t value,uint32_t address,int rqlen) {
	fprintf(stderr,"AHB errstat2 is not writable\n");
	return;
}

static uint32_t 
ahb_errmonconf_read(void *cd,uint32_t address,int rqlen) {
	Sysco *sysco=cd;
	return sysco->ahb_errmonconf;	
}

static void 
ahb_errmonconf_write(void *cd,uint32_t value,uint32_t address,int rqlen) {
	sysco->ahb_errmonconf=value & ~(1<<22);
	return;
}

/*
 * -------------------------------------------------------------
 * AHB Gen Config Register is not documented fully
 * Netos uses some of the bits to check if booted from debugger
 * -------------------------------------------------------------
 */
static uint32_t 
ahb_gcfg_read(void *cd,uint32_t address,int rqlen) {
	Sysco *sysco=cd;
	return sysco->gcfg;
}

static void 
ahb_gcfg_write(void *cd,uint32_t value,uint32_t address,int rqlen) {
	Sysco *sysco=cd;
	sysco->gcfg=value;
	return;
}

void 
init_misc_config(Sysco *sc) {
	uint32_t misc;
	int32_t result;

	Config_ReadInt32(&result,"ns9750","Revision");
	misc = result<<24; 

	Config_ReadInt32(&result,"ns9750","rtck");
	misc=misc | (result&1) << 13; 

	misc=misc | (1<<12);

	Config_ReadInt32(&result,"ns9750","reset_done");
	misc=misc | (result&1) << 11; 

	Config_ReadInt32(&result,"ns9750","boot_strap_0");
	misc=misc | (result&1) << 10; 

	Config_ReadInt32(&result,"ns9750","boot_strap_4");
	misc=misc | (result&1) << 9; 

	Config_ReadInt32(&result,"ns9750","boot_strap_3");
	misc=misc | (result&1) << 8; 

	Config_ReadInt32(&result,"ns9750","boot_strap_2");
	misc=misc | (result&1) << 7; 

	Config_ReadInt32(&result,"ns9750","boot_strap_1");
	misc=misc | (result&1) << 6; 

	/* CS1 polarity is the inverse of the gpio49 */
	Config_ReadInt32(&result,"ns9750","gpio49");
	if(result==0) {
		misc=misc | (1 << 5); 
	}

	/* Endian is inverted */
	Config_ReadInt32(&result,"ns9750","gpio44");
	if(result==0) {
		misc=misc | (1 << 3); 
	}
	sc->misc_config=misc;
	fprintf(stderr,"Initial Misc Config %08x\n",sc->misc_config);
}

void 
init_pll_config(Sysco *sc) 
{
	uint32_t pll=0;
	uint32_t multiplier_sel=0;
	uint32_t multiplier;
	int result;
	if(Config_ReadInt32(&result,"ns9750","gpio19")>=0) {
		pll=pll | ((result&1)<<25);
		pll=pll | ((result&1)<<9);
	}
	if(Config_ReadInt32(&result,"ns9750","gpio2")>=0) {
		pll=pll | ((result&1)<<24);
		pll=pll | ((result&1)<<8);
	}
	if(Config_ReadInt32(&result,"ns9750","gpio0")>=0) {
		pll=pll | ((result&1)<<23);
		pll=pll | ((result&1)<<7);
	}
	if(Config_ReadInt32(&result,"ns9750","gpio24")>=0) {
		pll=pll | ((result&1)<<22);
		pll=pll | ((result&1)<<6);
	}
	if(Config_ReadInt32(&result,"ns9750","gpio22")>=0) {
		pll=pll | ((result&1)<<21);
		pll=pll | ((result&1)<<5);
	}
	if(Config_ReadInt32(&result,"ns9750","gpio17")>=0) {
		pll=pll | ((result&1)<<20);
		multiplier_sel |= ((result&1)<<4);
	}
	if(Config_ReadInt32(&result,"ns9750","gpio12")>=0) {
		pll=pll | ((result&1)<<19);
		multiplier_sel |= ((result&1)<<3);
	}
	if(Config_ReadInt32(&result,"ns9750","gpio10")>=0) {
		pll=pll | ((result&1)<<18);
		multiplier_sel |= ((result&1)<<2);
	}
	if(Config_ReadInt32(&result,"ns9750","gpio8")>=0) {
		pll=pll | ((result&1)<<17);
		multiplier_sel |= ((result&1)<<1);
	}
	if(Config_ReadInt32(&result,"ns9750","gpio4")>=0) {
		pll=pll | ((result&1)<<16);
		multiplier_sel |= ((result&1)<<0);
	}
	/* Determine multiplier */
	switch(multiplier_sel) {
		case 0x1a:
			multiplier = 32; 
			break;
		case 0x1b:	/* Warning ! Bug in Manual. case value 0x04 is double */
			fprintf(stderr,"PLL-Config: Warning: duplicate case Value in NS9750 Manual !\n");
			sleep(3);
			multiplier = 31; 
			break;
		case 0x18:
			multiplier = 30; 
			break;
		case 0x19:
			multiplier = 29; 
			break;
		case 0x1e:
			multiplier = 28; 
			break;
		case 0x1f:
			multiplier = 27; 
			break;
		case 0x1c:
			multiplier = 26; 
			break;
		case 0x1d:
			multiplier = 25; 
			break;
		case 0x12:
			multiplier = 24; 
			break;
		case 0x13:
			multiplier = 23; 
			break;
		case 0x10:
			multiplier = 22; 
			break;
		case 0x11:
			multiplier = 21; 
			break;
		case 0x16:
			multiplier = 20; 
			break;
		case 0x17:
			multiplier = 19; 
			break;
		case 0x14:
			multiplier = 18; 
			break;
		case 0x15:
			multiplier = 17; 
			break;
		case 0x0a:
			multiplier = 16; 
			break;
		case 0x0b:
			multiplier = 15; 
			break;
		case 0x08:
			multiplier = 14; 
			break;
		case 0x09:
			multiplier = 13; 
			break;
		case 0x0e:
			multiplier = 12; 
			break;
		case 0x0f:
			multiplier = 11; 
			break;
		case 0x0c: 	
			multiplier = 10; 
			break;
		case 0x0d:
			multiplier = 9; 
			break;
		case 0x02:
			multiplier = 8; 
			break;
		case 0x03:
			multiplier = 7; 
			break;
		case 0x00:
			multiplier = 6; 
			break;
		case 0x01:
			multiplier = 5; 
			break;
		case 0x06:
			multiplier = 4; 
			break;
		case 0x07:
			multiplier = 3; 
			break;
		case 0x04:
			fprintf(stderr,"PLL-Config: Warning: duplicate case Value in NS9750 Manual !\n");
			sleep(3);
			multiplier = 2; 
			break;
		case 0x05:
			multiplier = 1; 
			break;
		default: /* Impossible */ 
			multiplier=1;
			break;
	}
	pll |= (multiplier-1);
	sc->pll_config=pll;
	fprintf(stderr,"Initial PLL Config %08x\n",pll);
}

/*
 * -----------------------------------------
 * Handle changes in extirq line 
 * -----------------------------------------
 */
static void 
extirq_trace_proc(struct SigNode * node,int value, void *clientData)
{
	Irq_TraceInfo *ti = clientData;
	Sysco *sysco = ti->sysco;
	int index = ti->irq_index;
	int postval=0;
	int update_interrupt=0;
	uint32_t eintctrl;
	if(value==SIG_HIGH) {
		sysco->extint[index] |= EXTINT_STS;
	} else {
		sysco->extint[index] &= ~EXTINT_STS;
	}
	eintctrl = sysco->extint[index];
	if(eintctrl & EXTINT_LVEDG)  {
		if(eintctrl & EXTINT_PLTY) {
			if((sysco->extint_oldsts[index] == SIG_HIGH) && (value==SIG_LOW)) {
				update_interrupt=1;
				postval=1;
			}
		} else {
			if((sysco->extint_oldsts[index] == SIG_LOW) && (value==SIG_HIGH)) {
				update_interrupt=1;
				postval=1;
			} 
		}
	} else {
		if(eintctrl & EXTINT_PLTY) {
			if(value==SIG_LOW) {
				update_interrupt=1;
				postval=1;
			} else {
				update_interrupt=1;
				postval=0;
			}
		} else {
			if(value==SIG_HIGH) {
				update_interrupt=1;
				postval=1;
			} else {
				update_interrupt=1;
				postval=0;
			}
		}
	}
	if(update_interrupt) {
		if(postval) {
			if(!sysco->extint_posted[index]) {
				Sysco_PostIrq(ti->irq); 
				sysco->extint_posted[index]=1;
			}
		} else {
			if(sysco->extint_posted[index]) {
				Sysco_UnPostIrq(ti->irq); 
				sysco->extint_posted[index]=0;
			}
		}
	}
	sysco->extint_oldsts[index]=value;
}

static  void
SyscoMap(Sysco *sc) {
	int i;
	IOH_New32(SYS_AHB_GCFG,ahb_gcfg_read,ahb_gcfg_write,sysco);
	IOH_New32(SYS_AHB_BRC0,ahb_brc_read,ahb_brc_write,sysco);
	IOH_New32(SYS_AHB_BRC1,ahb_brc_read,ahb_brc_write,sysco);
	IOH_New32(SYS_AHB_BRC2,ahb_brc_read,ahb_brc_write,sysco);
	IOH_New32(SYS_AHB_BRC3,ahb_brc_read,ahb_brc_write,sysco);

	IOH_New32(SYS_AHB_ARBTOUT,ahb_arbtout_read,ahb_arbtout_write,sysco); 
	IOH_New32(SYS_AHB_ERRSTAT1,ahb_errstat1_read,ahb_errstat1_write,sysco);
	IOH_New32(SYS_AHB_ERRSTAT2,ahb_errstat2_read,ahb_errstat2_write,sysco);
	IOH_New32(SYS_AHB_ERRMON,ahb_errmonconf_read,ahb_errmonconf_write,sysco);

	IOH_New32(SYS_TIS,timer_tis_read,NULL,sysco);
	IOH_New32(SYS_SWDGCFG,swdgcfg_read,swdgcfg_write,sysco);
	IOH_New32(SYS_SWDGTMR,swdgtmr_read,swdgtmr_write,sysco);
	IOH_New32(SYS_CLKCFG,clkcfg_read,clkcfg_write,sysco);
	IOH_New32(SYS_MODRST,modrst_read,modrst_write,sysco);
	IOH_New32(SYS_MISCCFG,sys_misc_config_read,sys_misc_config_write,sysco);
	IOH_New32(SYS_PLLCFG,sys_pll_config_read,sys_pll_config_write,sysco);
	IOH_New32(SYS_INTID,sys_intid_read,NULL,sysco);
	IOH_New32(SYS_ISRAW,sys_israw_read,NULL,sysco);
	IOH_New32(SYS_ISA,sys_isa_read,NULL,sysco);
	IOH_New32(SYS_ISRA,sys_isra_read,sys_isra_write,sysco);
	for(i=0;i<16;i++) {
		Timer *timer = sc->timer[i];
		IOH_New32(SYS_TRCV(i),timer_trcv_read,timer_trcv_write,timer);
		IOH_New32(SYS_TRR(i),timer_trr_read,NULL,timer);
		IOH_New32(SYS_TCR(i),timer_tcr_read,timer_tcr_write,timer);
	}
	for(i=0;i<32;i++) {
		IOH_New32(SYS_IVARV(i),sys_ivarv_read,sys_ivarv_write,sysco);
		IOH_New8(SYS_ICFG(i),sys_icfg_read,sys_icfg_write,sysco);
	}
	IOH_New32(SYS_GENID,genid_read,genid_write,sysco);
	IOH_New32(SYS_EXTINT0,extint_read,extint_write,sysco);
	IOH_New32(SYS_EXTINT1,extint_read,extint_write,sysco);
	IOH_New32(SYS_EXTINT2,extint_read,extint_write,sysco);
	IOH_New32(SYS_EXTINT3,extint_read,extint_write,sysco);
}

void
NS9750_TimerInit(const char *name) {
	int i;
	Sysco *sc = sg_new(Sysco);
        sysco=sc;
	sysco->irqmask=~0;

	for(i=0;i<16;i++) {
		Timer *timer = sg_new(Timer);
		timer->nr=i;
		sc->timer[i]=timer;
	}
	sysco->sys_intid=32;
	for(i=0;i<32;i++) {
		sysco->old_irqmask[i]=~0U;
	}
	sysco->wdg_counter=0xffffffff; // Docu is wrong!
	sysco->clkcfg=0x7d;
	sysco->modrst=0x7d;
	sysco->brc[0]=0x80818082;
	sysco->brc[1]=0x80838084;
	sysco->brc[2]=0x80858086;
	sysco->brc[3]=0;
	sysco->ahb_arbtout=0xffffffff;
	sysco->ahb_errmonconf=0x200;
	sysco->gcfg = 0x2000;
	init_misc_config(sc);
	init_pll_config(sc);
	init_genid(sc);
	for(i = 0;i < 32;i++) {
		Irq_TraceInfo *ti = &sc->irqTraceInfo[i];
		sc->irqNode[i] = SigNode_New("ns9750sysco.irq%d",i);
		if(!sc->irqNode[i]) {
			fprintf(stderr,"Can not create sysco irq %d\n",i);
		}
		SigNode_Set(sc->irqNode[i],SIG_PULLUP); 
		ti->irq_index = i;
		ti->sysco = sc;
		SigNode_Trace(sc->irqNode[i],irq_trace_proc,ti);
	}
	for(i=0;i<4;i++) {
		Irq_TraceInfo *ti = &sc->extIrqTraceInfo[i];
		sc->extIrqNode[i] = SigNode_New("ns9750sysco.extirq%d",i);
		if(!sc->extIrqNode[i]) {
			fprintf(stderr,"Can not create sysco extirq node\n");
			exit(3490);
		}
		SigNode_Set(sc->extIrqNode[i],SIG_PULLUP); 
		ti->irq=extirqs[i];	
		ti->irq_index=i;	
		ti->sysco=sc;	
		sc->extIrqTrace[i] = SigNode_Trace(sc->extIrqNode[i],extirq_trace_proc,ti);
		sc->extint[i]=0;
	}
	SyscoMap(sc);
}
