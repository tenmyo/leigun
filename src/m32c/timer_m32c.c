/*
 ******************************************************************************************************* 
 * M32C Timer block A+B simulation.
 *
 * State: Clocks are missing, only Counter mode is implemented.
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
 ******************************************************************************************************* 
 */
#include <unistd.h>
#include <inttypes.h>
#include <bus.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include "sgstring.h"
#include "clock.h"
#include "cycletimer.h"
#include "signode.h"
#include "timer_m32c.h"

typedef struct TimerB_Regs {
	uint32_t TBiMR;
	uint32_t TBi;
} TimerB_Regs;

typedef struct TimerA_Regs {
	uint32_t TAiMR;
	uint32_t TAi;
} TimerA_Regs;

static TimerA_Regs tma_regs[] = {
	{
		.TAiMR = 0x0356,
		.TAi = 0x0346,
	},
	{
		.TAiMR = 0x0357,
		.TAi = 0x0348,
	},
	{
		.TAiMR = 0x0358,
		.TAi = 0x34a,
	},
	{
		.TAiMR = 0x0359,
		.TAi = 0x34c,
	},
	{
		.TAiMR = 0x035A,
		.TAi = 0x34e,
	},
};

static TimerB_Regs tmb_regs[] = {
	{
		.TBiMR = 0x35b,
		.TBi = 0x350, 
	},
	{
		.TBiMR = 0x35c,
		.TBi = 0x352, 
	},
	{
		.TBiMR = 0x35d,
		.TBi = 0x354, 
	},
	{
		.TBiMR = 0x31b,
		.TBi = 0x310, 
	},
	{
		.TBiMR = 0x31c,
		.TBi = 0x312, 
	},
	{
		.TBiMR = 0x31d,
		.TBi = 0x314, 
	},
};
#define TBMR_TMOD	(3 << 0)
#define		TBMOD_TIMER	(0)
#define		TBMOD_EVENT_CNT	(1)
#define		TBMOD_PULSE_PER	(2)
#define	TAMR_TMOD	(3 << 0)
#define		TAMOD_TIMER	(0)
#define		TAMOD_EVENT_CNT	(1)
#define		TAMOD_ONESHOT	(2)
#define		TAMOD_PWM	(3)

#define	TAMR_MR	(7 << 3)
#define		TAMR_MR_GFDISA0	(0)
#define		TAMR_MR_GFDISA1	(1 << 3)
#define		TAMR_MR_GFLOW	(2 << 3)
#define 	TAMR_MR_GFHIGH	(3 << 3)

#define TBMR_MR		(0xf << 2)
#define TBMR_TCK_MSK	(3 << 6)
#define TBMR_TCK_SHIFT	(6)
#define		TCK_F1 		(0)
#define		TCK_F8 		(1 << 6)
#define		TCK_F2N		(2 << 6)
#define		TCK_FC32	(3 << 6)


#define TIMERB_TBSR 	(0x300)
#define TIMERA_TCSPR	(0x35f)
#define TIMER_TABSR	(0x340)
#define TIMER_ONSF	(0x342)
#define TIMER_TRGSR	(0x343)
#define TIMER_UDF	(0x344)

typedef struct M32C_TimerBlock  M32C_TimerBlock;

typedef struct M32C_TimerB {
	M32C_TimerBlock *mtb;
	int timer_nr;
	bool tstart_bit;
	uint8_t reg_tbimr;
	uint16_t tb_count;
	uint16_t tb_reload_value;
	Clock_t *clk_cntr;
	CycleCounter_t last_actualized;
	CycleCounter_t accumulated_cycles;
	CycleTimer event_timer;
	SigNode *sigIrq;
	const char *name;
} M32C_TimerB;

typedef struct M32C_TimerA {
	M32C_TimerBlock *mtb;
	int timer_nr;
	bool tstart_bit;
	bool tonsf_bit;
	uint8_t reg_taimr;
	uint16_t ta_count;
	uint16_t ta_reload_value;
	SigNode *sigTAiIN;
	SigNode *sigTAiOUT;
	SigTrace *traceTAiIN;
	SigTrace *traceTAiOUT;
	Clock_t *clk_cntr;
	CycleCounter_t last_actualized;
        CycleCounter_t accumulated_cycles;
	CycleTimer event_timer;
	SigNode *sigIrq;
	const char *name;
} M32C_TimerA;

struct M32C_TimerBlock {
	BusDevice bdev;
	M32C_TimerA *timerA[5];
	M32C_TimerB *timerB[6];
	Clock_t *clk_f1;
	Clock_t *clk_f8;
	Clock_t *clk_f2n;
	Clock_t *clk_fc32;
	uint8_t reg_tabsr;
	uint8_t reg_tbsr;
	uint8_t reg_onsf;
	uint8_t reg_tgsr;
	uint8_t reg_udf;
};



static void
clock_update_a(M32C_TimerA *tm) 
{
	M32C_TimerBlock *mtb = tm->mtb;
	int clk_source = (tm->reg_taimr & TBMR_TCK_MSK);
	int tmr_mode = (tm->reg_taimr & TBMR_TMOD);
	if(tmr_mode == TAMOD_EVENT_CNT) {
		Clock_Decouple(tm->clk_cntr);
		Clock_SetFreq(tm->clk_cntr,0);
		return;
	}
	switch(clk_source) {
		case TCK_F1:
			Clock_MakeDerived(tm->clk_cntr,mtb->clk_f1,1,1);
			break;

		case TCK_F8:
			Clock_MakeDerived(tm->clk_cntr,mtb->clk_f8,1,1);
			break;

		case TCK_F2N:
			Clock_MakeDerived(tm->clk_cntr,mtb->clk_f2n,1,1);
			break;

		case TCK_FC32:
			Clock_MakeDerived(tm->clk_cntr,mtb->clk_fc32,1,1);
			break;
	}
}

static void
clock_update_b(M32C_TimerB *tm) 
{
	M32C_TimerBlock *mtb = tm->mtb;
	int clk_source = (tm->reg_tbimr & TBMR_TCK_MSK);
	int tmr_mode = (tm->reg_tbimr & TBMR_TMOD);
	if(tmr_mode == TBMOD_EVENT_CNT) {
		Clock_Decouple(tm->clk_cntr);
		Clock_SetFreq(tm->clk_cntr,0);
		return;
	}
	switch(clk_source) {
		case TCK_F1:
			Clock_MakeDerived(tm->clk_cntr,mtb->clk_f1,1,1);
			break;

		case TCK_F8:
			Clock_MakeDerived(tm->clk_cntr,mtb->clk_f8,1,1);
			break;

		case TCK_F2N:
			Clock_MakeDerived(tm->clk_cntr,mtb->clk_f2n,1,1);
			break;

		case TCK_FC32:
			Clock_MakeDerived(tm->clk_cntr,mtb->clk_fc32,1,1);
			break;
	}
}

static void
actualize_counter_b(M32C_TimerB *tm) 
{
	FractionU64_t frac;	
	uint64_t elapsed_cycles;
	uint64_t acc;
	int64_t count;
	uint64_t counter_cycles;
	uint32_t period;
	int tmr_mode;
	elapsed_cycles = CycleCounter_Get() - tm->last_actualized;
	tm->last_actualized = CycleCounter_Get();
	acc = tm->accumulated_cycles + elapsed_cycles; 
	frac = Clock_MasterRatio(tm->clk_cntr);
	if((frac.nom == 0) || (frac.denom == 0)) {
		fprintf(stderr,"Warning, No clock for %s\n",tm->name);
		return;
	}
	if(tm->tb_reload_value == 0) {
		return;
	}
	counter_cycles = acc * frac.nom / frac.denom;
	acc -= counter_cycles * frac.denom / frac.nom;
        tm->accumulated_cycles = acc;
	period = tm->tb_reload_value + 1;
	tmr_mode = tm->reg_tbimr & 3;
	switch(tmr_mode) {
		case TBMOD_TIMER:
			count = tm->tb_count;
			if(counter_cycles > tm->tb_count) {
				SigNode_Set(tm->sigIrq,SIG_LOW);
				SigNode_Set(tm->sigIrq,SIG_HIGH);
				count -= counter_cycles;		
				tm->tb_count = (count % period) + period;
#if 0
				if(tm->timer_nr == 3) {
					//fprintf(stderr,"Triggered the interrupt at %08lld\n",CyclesToMilliseconds(CycleCounter_Get()));
					fprintf(stderr,"Triggered the interrupt at %08lld\n",CycleCounter_Get());
				}
#endif
			} else {
				tm->tb_count -= counter_cycles;
			}
			
			#if 0
			if(tm->timer_nr == 0) {
			fprintf(stderr,"Updated timer b%d to %d, counter_cycles %lld\n",tm->timer_nr,tm->tb_count,counter_cycles);
			}
			#endif
			break;
		default:
			fprintf(stderr,"TMR-Mode %d not implemented\n",tmr_mode);
			break;
	}
	
}

static void
event_count_a(SigNode *node,int sigval,void *eventData) 
{
	M32C_TimerA *tm = eventData;
	uint8_t mr = tm->reg_taimr;
	uint8_t dirsrc = (mr >> 4) & 1; 
	uint8_t dir;
	uint8_t countpol = (mr >> 3) & 1;
	uint8_t twophase = 0;
	uint8_t multfour = 0;
	uint8_t free_running = (mr >> 6) & 1; 
	if((tm->timer_nr >= 2) && (tm->timer_nr <= 4)) {
		twophase = (tm->mtb->reg_udf >> (3 + tm->timer_nr)) & 1;
	}
	if(twophase && (tm->timer_nr == 4)) {
		multfour = 1;
	} else if(twophase && (tm->timer_nr == 3)) {
		multfour = (mr >> 7) & 1;
	}
	/* counterpol must be 0 for two phase mode */
	if(!multfour && (countpol == 1) && (sigval == SIG_LOW)) {
		return;
	}
	if(!multfour && (countpol == 0) && (sigval == SIG_HIGH)) {
		return;
	}
	if(multfour) {
		if(node == tm->sigTAiIN) {
			if(sigval == SIG_HIGH) {
				if(SigNode_Val(tm->sigTAiOUT) == SIG_HIGH) {
					dir = 1;
				} else {
					dir = 0;
				}
			} else {
				if(SigNode_Val(tm->sigTAiOUT) == SIG_HIGH) {
					dir = 0;
				} else {
					dir = 1;
				}
			}
		} else {
			if(sigval == SIG_HIGH) {
				if(SigNode_Val(tm->sigTAiIN) == SIG_HIGH) {
					dir = 0;
				} else {
					dir = 1;
				}
			} else {
				if(SigNode_Val(tm->sigTAiIN) == SIG_HIGH) {
					dir = 1;
				} else {
					dir = 0;
				}
			}
		}
	} else {
		if(node == tm->sigTAiOUT) {
			return;
		}
		if(dirsrc == 0) { /* UDF register */
			dir = (tm->mtb->reg_udf >> tm->timer_nr) & 1;
		} else {
			/* High level means increment when taiin is low */
			dir = (SigNode_Val(tm->sigTAiOUT) == SIG_HIGH);
		}
		if(twophase && (sigval == SIG_HIGH)) {
			dir ^= 1;
		}
	}
	if(dir == 0) {	/* decrement */
		if(tm->ta_count == 0) {
			/* Trigger Interrupt */
			SigNode_Set(tm->sigIrq,SIG_LOW);
			SigNode_Set(tm->sigIrq,SIG_HIGH);
			if(!free_running) {
				tm->ta_count = tm->ta_reload_value;
			}
		} else {
			tm->ta_count = tm->ta_count - 1;
		}
	} else {	/* increment */
		if(tm->ta_count == 0xffff) {
			/* Trigger Interrupt */
			SigNode_Set(tm->sigIrq,SIG_LOW);
			SigNode_Set(tm->sigIrq,SIG_HIGH);
			if(!free_running) {
				tm->ta_count = tm->ta_reload_value;
			}
		} else {
			tm->ta_count = tm->ta_count + 1;
		}
	}
}
/**
 ******************************************************************************
 *
 ******************************************************************************
 */
static void
actualize_trigger_select_a(M32C_TimerA *tm) 
{
	M32C_TimerBlock *mtb = tm->mtb;
	uint8_t tmod;
	int idx = tm->timer_nr;
	int taitgl;
	tmod = tm->reg_taimr & TAMR_TMOD;
	if(!tm->tstart_bit || (tmod != TAMOD_EVENT_CNT)) {
		if(tm->traceTAiIN) {
			SigNode_Untrace(tm->sigTAiIN,tm->traceTAiIN);
			tm->traceTAiIN = NULL;
		}
		if(tm->traceTAiOUT) {
			SigNode_Untrace(tm->sigTAiOUT,tm->traceTAiOUT);
			tm->traceTAiOUT = NULL;
		}
		return;
	}
	if(idx == 0) {
		taitgl = (mtb->reg_onsf >> 6) & 3;
	} else {
		taitgl = (mtb->reg_tgsr >> ((idx - 1) * 2)) & 3;
	}
	switch(taitgl) {
		case 0:
			tm->traceTAiIN = SigNode_Trace(tm->sigTAiIN,event_count_a,tm);
			tm->traceTAiOUT = SigNode_Trace(tm->sigTAiOUT,event_count_a,tm);
			break;
		default:
			fprintf(stderr,"%s: trigger select %u not implemented\n",tm->name,taitgl);
			break;
	}
}

static void
actualize_counter_a(M32C_TimerA *tm) 
{
	FractionU64_t frac;	
	uint64_t elapsed_cycles;
	uint64_t acc;
	int64_t count;
	uint64_t counter_cycles;
	uint32_t period;
	int tmr_mode;
	elapsed_cycles = CycleCounter_Get() - tm->last_actualized;
	tm->last_actualized = CycleCounter_Get();
	acc = tm->accumulated_cycles + elapsed_cycles; 
	frac = Clock_MasterRatio(tm->clk_cntr);
	if((frac.nom == 0) || (frac.denom == 0)) {
		fprintf(stderr,"Warning, No clock for %s\n",tm->name);
		return;
	}
	if(tm->ta_reload_value == 0) {
		return;
	}
	counter_cycles = acc * frac.nom / frac.denom;
	acc -= counter_cycles * frac.denom / frac.nom;
        tm->accumulated_cycles = acc;
	period = tm->ta_reload_value + 1;
	tmr_mode = tm->reg_taimr & 3;
	switch(tmr_mode) {
		case TAMOD_TIMER:
			count = tm->ta_count;
			if(counter_cycles > tm->ta_count) {
				SigNode_Set(tm->sigIrq,SIG_LOW);
				SigNode_Set(tm->sigIrq,SIG_HIGH);
				count -= counter_cycles;		
				tm->ta_count = (count % period) + period;
			} else {
				tm->ta_count -= counter_cycles;
			}
			break;

		case TAMOD_ONESHOT:
			if(counter_cycles > tm->ta_count) {
				fprintf(stderr,"Triggering an Interrupt\n");
				SigNode_Set(tm->sigIrq,SIG_LOW);
				SigNode_Set(tm->sigIrq,SIG_HIGH);
				tm->ta_count = 0;
			} else {
				tm->ta_count -= counter_cycles;
			}
			break;

		default:
			fprintf(stderr,"TMR-Mode %d not implemented\n",tmr_mode);
			break;
	}
	
}

static void
update_timeout_b(M32C_TimerB *tm) 
{
	FractionU64_t frac;	
	int tmr_mode;
	uint64_t timer_cycles;
	uint64_t cpu_cycles;
	tmr_mode = tm->reg_tbimr & 3;
	frac = Clock_MasterRatio(tm->clk_cntr);
	if(!frac.nom) {
		return;
	}
#if 0
	if(tm->timer_nr == 5) {
		fprintf(stderr,"Timer 5 update\n");
	}
#endif
	if(!tm->tstart_bit) {
		//fprintf(stderr,"Timer %d is not active \n",tm->timer_nr);
		CycleTimer_Remove(&tm->event_timer);
		return;
	}
	switch(tmr_mode) {
		case TBMOD_TIMER:
			timer_cycles = tm->tb_count + 1;
			cpu_cycles = (timer_cycles * frac.denom) / frac.nom;
			#if 0
			if(tm->timer_nr == 0) {
				fprintf(stderr,"sleep for %lld cpu cycles\n",cpu_cycles);
			}
			#endif
			CycleTimer_Mod(&tm->event_timer,cpu_cycles);
			break;
			
	}	
}

static void
timer_b_event(void *clientData)
{
        M32C_TimerB *tm = (M32C_TimerB *)clientData;
	//fprintf(stderr,"Timer B event,reload value %d\n",tm->tb_reload_value);
	//fprintf(stderr,"tb_count before: %d\n",tm->tb_count);
        actualize_counter_b(tm);
	//fprintf(stderr,"tb_count after: %d\n",tm->tb_count);
        update_timeout_b(tm);
}

static void
update_timeout_a(M32C_TimerA *tm) 
{
	FractionU64_t frac;	
	int tmr_mode;
	uint64_t timer_cycles;
	uint64_t cpu_cycles;
	tmr_mode = tm->reg_taimr & 3;
	frac = Clock_MasterRatio(tm->clk_cntr);
	if(!frac.nom) {
		fprintf(stderr,"No clock for timer %s\n",tm->name);
		return;
	}
	if(!tm->tstart_bit) {
		fprintf(stderr,"Timer %d is not active \n",tm->timer_nr);
		CycleTimer_Remove(&tm->event_timer);
		return;
	}
	switch(tmr_mode) {
		case TAMOD_TIMER:
			timer_cycles = tm->ta_count + 1;
			cpu_cycles = (timer_cycles * frac.denom) / frac.nom;
			//fprintf(stderr,"Timeout tc %lld, cpu %lld cycles\n",timer_cycles,cpu_cycles);
			//fprintf(stderr,"Freq %f\n",Clock_DFreq(tm->clk_cntr));
			CycleTimer_Mod(&tm->event_timer,cpu_cycles);
			break;
		case TAMOD_ONESHOT:
			if(tm->ta_count) {
				timer_cycles = tm->ta_count + 1;
				cpu_cycles = (timer_cycles * frac.denom) / frac.nom;
				fprintf(stderr,"Timeout tc %"PRIu64", cpu %"PRIu64" cycles\n",timer_cycles,cpu_cycles);
				fprintf(stderr,"Freq %f\n",Clock_DFreq(tm->clk_cntr));
				CycleTimer_Mod(&tm->event_timer,cpu_cycles);
			} else {
				fprintf(stderr,"reached final value\n");
			}
			break;
		
			
	}	
}

static void
timer_a_event(void *clientData)
{
        M32C_TimerA *tm = (M32C_TimerA *)clientData;
        actualize_counter_a(tm);
        update_timeout_a(tm);
}


static uint32_t
taimr_read(void *clientData,uint32_t address,int rqlen)
{
	M32C_TimerA *ta = (M32C_TimerA *) clientData;
        return ta->reg_taimr;
}

static void
taimr_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	M32C_TimerA *ta = (M32C_TimerA *) clientData;
	uint8_t mr = value & TAMR_MR; 
	uint8_t tmod = value & TAMR_TMOD;
	actualize_counter_a(ta);
	ta->reg_taimr = value;
	if(tmod == TAMOD_TIMER) {
		switch(mr) {
			case TAMR_MR_GFDISA0:
			case TAMR_MR_GFDISA1:
				break;
			default:
				fprintf(stderr,"%s Gate Funktion is not implemented\n",ta->name);
				break;
		}
	} else if(tmod == TAMOD_EVENT_CNT) {

	}
	/* Maybe this should be restricted to diff in mode */
	actualize_trigger_select_a(ta);
	clock_update_a(ta);
}

static uint32_t
tbimr_read(void *clientData,uint32_t address,int rqlen)
{
	M32C_TimerB *tb = (M32C_TimerB *) clientData;
	return tb->reg_tbimr;
}

static void
tbimr_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	M32C_TimerB *tb = (M32C_TimerB *) clientData;
	actualize_counter_b(tb);
	tb->reg_tbimr = value;
	clock_update_b(tb);
}

static uint32_t
tbi_read(void *clientData,uint32_t address,int rqlen)
{
	M32C_TimerB *tb = (M32C_TimerB *) clientData;
	/* ??? */
	return tb->tb_reload_value; 
}

#include "cpu_m32c.h"
static void
tbi_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	M32C_TimerB *tb = (M32C_TimerB *) clientData;
	actualize_counter_b(tb);
	tb->tb_reload_value = value;	
	if(value == 1) {
		fprintf(stderr,"Reload value %d set at %06x\n",value,M32C_REG_PC);
		exit(1);
	}
#if 0
	if(tb->timer_nr == 5) {
		fprintf(stderr,"TB5 reload value %08x\n",value);
		sleep(1);
	}
#endif
	if(tb->tstart_bit == 0) {
		tb->tb_count = value;
	}
}

static uint32_t
tai_read(void *clientData,uint32_t address,int rqlen)
{
	M32C_TimerA *ta = (M32C_TimerA *) clientData;
	/* ??? */
	return ta->ta_reload_value; 
}

static void
tai_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	M32C_TimerA *ta = (M32C_TimerA *) clientData;
	actualize_counter_a(ta);
	ta->ta_reload_value = value;	
	if(ta->tstart_bit == 0) {
		ta->ta_count = value;
	}
}
static uint32_t
tabsr_read(void *clientData,uint32_t address,int rqlen)
{
        M32C_TimerBlock *mtb = (M32C_TimerBlock *) clientData; 
        return mtb->reg_tabsr;
}

static void
tabsr_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	int i;
        M32C_TimerBlock *mtb = (M32C_TimerBlock *) clientData; 
	uint32_t diff = value ^ mtb->reg_tabsr;
	mtb->reg_tabsr = value;
	for(i=0;i<5;i++) {
		if(diff & (1 << i)) {
			M32C_TimerA *timer = mtb->timerA[i];
			timer->tstart_bit = (value >> i) & 1;
			//fprintf(stderr,"Diff in tabsr %d\n",i);
			update_timeout_a(timer);
			actualize_trigger_select_a(timer);
		}	
	}
	for(i=5;i<8;i++) {
		if(diff & (1 << i)) {
			M32C_TimerB *timer = mtb->timerB[i-5];
			//fprintf(stderr,"Diff in tabsr %d\n",i);
			timer->tstart_bit = (value >> i) & 1;
			update_timeout_b(timer);
		}
	}
}

static uint32_t
tbsr_read(void *clientData,uint32_t address,int rqlen)
{
        M32C_TimerBlock *mtb = (M32C_TimerBlock *) clientData; 
        return mtb->reg_tbsr;
}

static void
tbsr_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	int i;
        M32C_TimerBlock *mtb = (M32C_TimerBlock *) clientData; 
	uint32_t diff = value ^ mtb->reg_tbsr;
	mtb->reg_tbsr = value;
	for(i = 5;i < 8;i++) {
		if(diff & (1 << i)) {
			M32C_TimerB *timer = mtb->timerB[i-2];
			timer->tstart_bit = (value >> i) & 1;
			update_timeout_b(timer);
		}
	}
}

static uint32_t
onsf_read(void *clientData,uint32_t address,int rqlen)
{
	M32C_TimerBlock *mtb = (M32C_TimerBlock *)clientData;
        return mtb->reg_onsf & ~0xe0;
}

static void
onsf_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	M32C_TimerBlock *mtb = (M32C_TimerBlock *)clientData;
	uint32_t diff = value ^ mtb->reg_onsf;
        int i;
	mtb->reg_onsf = value;
	for(i = 0;i < 5;i++) {
		if(diff & (1 << i)) {
			M32C_TimerA *timer = mtb->timerA[i];
			timer->tonsf_bit = (value >> i) & 1;
			update_timeout_a(timer);
		}
	}
}

static uint32_t
trgsr_read(void *clientData,uint32_t address,int rqlen)
{
	M32C_TimerBlock *mtb = (M32C_TimerBlock *)clientData;
        return mtb->reg_tgsr; 
}
static void
trgsr_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	M32C_TimerBlock *mtb = (M32C_TimerBlock *)clientData;
	unsigned int i;
	mtb->reg_tgsr = value;
	for(i = 1; i < 4; i++) {
		actualize_trigger_select_a(mtb->timerA[i]);
	}
}

static uint32_t
udf_read(void *clientData,uint32_t address,int rqlen)
{
	M32C_TimerBlock *mtb = (M32C_TimerBlock *)clientData;
        return mtb->reg_udf & 0x1f; 
}

static void
udf_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	M32C_TimerBlock *mtb = (M32C_TimerBlock *)clientData;
	mtb->reg_udf = value;
}

static void
M32CTimers_Unmap(void *owner,uint32_t base,uint32_t mask)
{
        int i;
	for(i = 0; i < 5; i++) {
        	TimerA_Regs *tr = &tma_regs[i];
		IOH_Delete8(tr->TAiMR);
		IOH_Delete16(tr->TAi);;
	}
	for(i = 0; i < 6; i++) {
        	TimerB_Regs *tr = &tmb_regs[i];
		IOH_Delete8(tr->TBiMR);
		IOH_Delete16(tr->TBi);;
	}
	IOH_Delete8(TIMERB_TBSR);
	IOH_Delete8(TIMER_ONSF);
	IOH_Delete8(TIMER_TRGSR);
	IOH_Delete8(TIMER_UDF);
}

static void
M32CTimers_Map(void *owner,uint32_t base,uint32_t mask,uint32_t mapflags)
{
        int i;
        M32C_TimerBlock *mtb = (M32C_TimerBlock *) owner; 
	M32C_TimerB *tb;
	M32C_TimerA *ta;
	for(i = 0; i < 5; i++) {
		TimerA_Regs *tr;
		tr = &tma_regs[i];
		ta = mtb->timerA[i];
		IOH_New8(tr->TAiMR,taimr_read,taimr_write,ta);
		IOH_New16(tr->TAi,tai_read,tai_write,ta);
	}
	for(i = 0; i < 6; i++) {
		TimerB_Regs *tr;
		tr = &tmb_regs[i];
		tb = mtb->timerB[i];
		IOH_New8(tr->TBiMR,tbimr_read,tbimr_write,tb);
		IOH_New16(tr->TBi,tbi_read,tbi_write,tb);
	}
	IOH_New8(TIMERB_TBSR,tbsr_read,tbsr_write,mtb);
	IOH_New8(TIMER_TABSR,tabsr_read,tabsr_write,mtb);
	IOH_New8(TIMER_ONSF,onsf_read,onsf_write,mtb);
	IOH_New8(TIMER_TRGSR,trgsr_read,trgsr_write,mtb);
	IOH_New8(TIMER_UDF,udf_read,udf_write,mtb);
}


static M32C_TimerB * 
M32CTimerB_New(const char *name,M32C_TimerBlock *mtb) 
{
	M32C_TimerB *tb = sg_new(M32C_TimerB);	
	tb->mtb = mtb;
	CycleTimer_Init(&tb->event_timer,timer_b_event,tb);
	tb->sigIrq = SigNode_New("%s.irq",name);
	if(!tb->sigIrq) {
		fprintf(stderr,"Creation of timer B Interrupt line failed\n");
		exit(1);
	}
	SigNode_Set(tb->sigIrq,SIG_HIGH);
	tb->clk_cntr = Clock_New("%s.clk",name);
	if(!tb->clk_cntr) {
		fprintf(stderr,"Can not create clock for %s\n",name);
		exit(1);
	}
	tb->name = sg_strdup(name);
	return tb;
}

static M32C_TimerA * 
M32CTimerA_New(const char *name,M32C_TimerBlock *mtb) 
{
	M32C_TimerA *ta = sg_new(M32C_TimerA);	
	ta->mtb = mtb;
	CycleTimer_Init(&ta->event_timer,timer_a_event,ta);
	ta->sigIrq = SigNode_New("%s.irq",name);
	if(!ta->sigIrq) {
		fprintf(stderr,"Creation of timer A Interrupt line failed\n");
		exit(1);
	}
	SigNode_Set(ta->sigIrq,SIG_HIGH);
	ta->clk_cntr = Clock_New("%s.clk",name);
	if(!ta->clk_cntr) {
		fprintf(stderr,"Can not create clock for %s\n",name);
		exit(1);
	}
	ta->name = sg_strdup(name);
	return ta;
}

BusDevice *
M32CTimerBlock_New(const char *name) 
{
        M32C_TimerBlock *mtb = sg_new(M32C_TimerBlock);
	int i;
        mtb->bdev.first_mapping=NULL;
        mtb->bdev.Map=M32CTimers_Map;
        mtb->bdev.UnMap=M32CTimers_Unmap;
        mtb->bdev.owner=mtb;
        mtb->bdev.hw_flags=MEM_FLAG_WRITABLE|MEM_FLAG_READABLE;
	mtb->clk_f1 = Clock_New("%s.clk_f1",name);
	mtb->clk_f8 = Clock_New("%s.clk_f8",name);
	mtb->clk_f2n = Clock_New("%s.clk_f2n",name);
	mtb->clk_fc32 = Clock_New("%s.clk_fc32",name);
	for(i = 0;i < 5; i++) {
		char *tname = alloca(strlen(name) + 10);
		sprintf(tname,"%sA%d",name,i);
		mtb->timerA[i] = M32CTimerA_New(tname,mtb);
		mtb->timerA[i]->timer_nr = i;
		mtb->timerA[i]->sigTAiIN = SigNode_New("%s.ta%din",name,i);
		mtb->timerA[i]->sigTAiOUT = SigNode_New("%s.ta%dout",name,i);
		if(!mtb->timerA[i]->sigTAiOUT || !mtb->timerA[i]->sigTAiIN) {
			fprintf(stderr,"Failed to create timer signal node\n");
			exit(1);
		}
		clock_update_a(mtb->timerA[i]);
	}
	for(i = 0;i < 6; i++) {
		char *tname = alloca(strlen(name) + 10);
		sprintf(tname,"%sB%d",name,i);
		mtb->timerB[i] = M32CTimerB_New(tname,mtb);
		mtb->timerB[i]->timer_nr = i;
		clock_update_b(mtb->timerB[i]);
	}
	return &mtb->bdev;
}
