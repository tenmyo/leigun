/**
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "sgstring.h"
#include "signode.h"
#include "bus.h"
#include "r8c23_timerAB.h"
#include "cycletimer.h"
#include "clock.h"

typedef struct TrabRegisters {
	int32_t aTRxCR;
	int32_t aTRxIOC;
	int32_t aTRxOCR;
	int32_t aTRxMR;
	int32_t aTRxPRE;
	int32_t aTRxSC;
	int32_t aTRxPR;
	int32_t aTRx;
} TrabRegisters;

static TrabRegisters trabRegs[] = {
        {
		/* Timer A */
		.aTRxCR = 0x100,
		.aTRxIOC = 0x101,
		.aTRxOCR = -1,
		.aTRxMR = 0x102,
		.aTRxSC = -1,
		.aTRxPR = -1,
		.aTRxPRE = 0x103,
		.aTRx = 0x104,
	},
        {
		/* Timer B */
		.aTRxCR = 0x108,
		.aTRxOCR = 0x109,
		.aTRxIOC = 0x10a,
		.aTRxMR = 0x10B,
		.aTRxPRE = 0x10C,
		.aTRxSC = 0x10d,
		.aTRxPR	= 0x10e,
		.aTRx = -1,
	},
};

#define TRxMR_TMOD_MSK	(0x3)
#define 	TMOD_TIMER	(0)
#define		TMOD_WFG	(1)
#define		TMOD_ONESHOT	(2)
#define		TMOD_WFGONE	(3)
typedef struct TimerAB {
	BusDevice bdev;
	int regset_nr;
	TrabRegisters *regSet; 
	CycleCounter_t last_actualized;	
	CycleCounter_t accumulated_cycles;
        CycleTimer event_timer;
	Clock_t *clk_cntr;
	SigNode *sigIrq;
	uint8_t regTRxCR;
	uint8_t regTRxIOC;
	uint8_t regTRxOCR;
	uint8_t regTRxMR;
	uint8_t regTRxPRE;
	uint8_t regTRxSC;
	uint8_t regTRxPR; /* Same as TRx */
	//uint8_t regTRx;
	uint8_t tmr_pre_cnt;
	uint8_t tmr_cnt;
} TimerAB;

static void
actualize_counter(TimerAB *tmr) 
{

	FractionU64_t frac;
        uint64_t elapsed_cycles;
        uint64_t acc;
        int64_t count;
        uint64_t counter_cycles;
        uint32_t period;
        int tmr_mode;
        elapsed_cycles = CycleCounter_Get() - tmr->last_actualized;
        tmr->last_actualized = CycleCounter_Get();
        acc = tmr->accumulated_cycles + elapsed_cycles;
        frac = Clock_MasterRatio(tmr->clk_cntr);
        if((frac.nom == 0) || (frac.denom == 0)) {
                fprintf(stderr,"Warning, No clock for timer module\n");
                return;
        }
	counter_cycles = acc * frac.nom / frac.denom;
        acc -= counter_cycles * frac.denom / frac.nom;
        tmr->accumulated_cycles = acc;
	period = (tmr->regTRxPRE + 1) * (tmr->regTRxPR + 1);
	tmr_mode = tmr->regTRxMR & TRxMR_TMOD_MSK;
	switch(tmr_mode) {
 		case TMOD_TIMER:
                        count = (tmr->tmr_cnt * (tmr->regTRxPRE + 1)) + tmr->tmr_pre_cnt;
                        if(counter_cycles > count) {
                                SigNode_Set(tmr->sigIrq,SIG_LOW);
                                SigNode_Set(tmr->sigIrq,SIG_HIGH);
                                count -= counter_cycles;
                                count = (count % period) + period;
				tmr->tmr_cnt = count / (tmr->regTRxPRE + 1);
				tmr->tmr_pre_cnt = count - (tmr->tmr_cnt * (tmr->regTRxPRE + 1));
                        } else {
				count -= counter_cycles;
                        }
                        //tm->tb_count = 
			break;	
		case TMOD_WFG:
		case TMOD_ONESHOT:
		case TMOD_WFGONE:
               	default:
                        fprintf(stderr,"TMR-Mode %d not implemented\n",tmr_mode);
                        break;
	}
}

static void
update_timeout(TimerAB *tmr)
{
	FractionU64_t frac;
        int tmr_mode;
        uint64_t timer_cycles;
        uint64_t cpu_cycles;
        tmr_mode = tmr->regTRxMR & TRxMR_TMOD_MSK;
        frac = Clock_MasterRatio(tmr->clk_cntr);
        if(!frac.nom) {
                return;
        }
        switch(tmr_mode) {
                case TMOD_TIMER:
                        timer_cycles = (tmr->tmr_cnt * (tmr->regTRxPRE + 1)) + 
					tmr->tmr_pre_cnt + 1;
                        cpu_cycles = (timer_cycles * frac.denom) / frac.nom;
			//fprintf(stderr,"sleep for %lld cpu cycles, %lld timer cycles\n",cpu_cycles,timer_cycles);
                        CycleTimer_Mod(&tmr->event_timer,cpu_cycles);
                        break;
        }

}

static void
timerAB_event(void *clientData)
{
	TimerAB *tmr = (TimerAB*)clientData;
        actualize_counter(tmr);
        update_timeout(tmr);
	//fprintf(stderr,"Event at %llu\n",CycleCounter_Get());
}

static uint32_t
trcr_read(void *clientData,uint32_t address,int rqlen)
{	
	return 0;
}

static void
trcr_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
}

static uint32_t
trioc_read(void *clientData,uint32_t address,int rqlen)
{	
	return 0;
}

static void
trioc_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
}

static uint32_t
trocr_read(void *clientData,uint32_t address,int rqlen)
{	
	return 0;
}

static void
trocr_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
}

static uint32_t
trmr_read(void *clientData,uint32_t address,int rqlen)
{	
	TimerAB *tmr = clientData;
	return tmr->regTRxMR;
}
static void
trmr_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	TimerAB *tmr = clientData;
	actualize_counter(tmr);
	tmr->regTRxMR = value;
	update_timeout(tmr);
}

static uint32_t
trpre_read(void *clientData,uint32_t address,int rqlen)
{	
	TimerAB *tmr = clientData;
	actualize_counter(tmr);
	return tmr->regTRxPRE;
}
static void
trpre_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	TimerAB *tmr = clientData;
	actualize_counter(tmr);
	tmr->regTRxPRE = value;
	//fprintf(stderr,"Trpre is %d\n",value);
	//exit(1);
	update_timeout(tmr);
}

static uint32_t
trsc_read(void *clientData,uint32_t address,int rqlen)
{	
	return 0;
}
static void
trsc_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
}

static uint32_t
trpr_read(void *clientData,uint32_t address,int rqlen)
{	
#if 0
	TimerAB *tmr = clientData;
	return tmr->regTRxPR;
#endif
	return 0;
}
static void
trpr_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	TimerAB *tmr = clientData;
	actualize_counter(tmr);
	tmr->regTRxPR = value;
	update_timeout(tmr);
}

static uint32_t
tr_read(void *clientData,uint32_t address,int rqlen)
{	
	return 0;
}
static void
tr_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	TimerAB *tmr = clientData;
	actualize_counter(tmr);
	tmr->regTRxPR = value;
	update_timeout(tmr);
}

static void
R8CTimerAB_Unmap(void *owner,uint32_t base,uint32_t mask)
{
	TimerAB *tmr = owner;
	TrabRegisters *regs = tmr->regSet;
	IOH_Delete8(regs->aTRxCR);
	IOH_Delete8(regs->aTRxIOC);
	if(regs->aTRxOCR >= 0) {
		IOH_Delete8(regs->aTRxOCR);
	}
	IOH_Delete8(regs->aTRxMR);
	IOH_Delete8(regs->aTRxPRE);
	if(regs->aTRxSC >= 0) {
		IOH_Delete8(regs->aTRxSC);
	}
	if(regs->aTRxPR >= 0) {
		IOH_Delete8(regs->aTRxPR);
	}
	if(regs->aTRx >= 0) {
		IOH_Delete8(regs->aTRx);
	}	
}

static void
R8CTimerAB_Map(void *owner,uint32_t base,uint32_t mask,uint32_t mapflags)
{
	TimerAB *tmr = owner;
	TrabRegisters *regs = tmr->regSet;
//        uint32_t flags = IOH_FLG_HOST_ENDIAN | IOH_FLG_PRD_RARP | IOH_FLG_PA_CBSE;
	IOH_New8(regs->aTRxCR,trcr_read,trcr_write,tmr);
	IOH_New8(regs->aTRxIOC,trioc_read,trioc_write,tmr);
	if(regs->aTRxOCR >= 0) {
		IOH_New8(regs->aTRxOCR,trocr_read,trocr_write,tmr);
	}
	IOH_New8(regs->aTRxMR,trmr_read,trmr_write,tmr);
	IOH_New8(regs->aTRxPRE,trpre_read,trpre_write,tmr);
	if(regs->aTRxSC >= 0) {
		IOH_New8(regs->aTRxSC,trsc_read,trsc_write,tmr);
	}
	if(regs->aTRxPR >= 0) {
		IOH_New8(regs->aTRxPR,trpr_read,trpr_write,tmr);
	}
	if(regs->aTRx >= 0) {
		IOH_New8(regs->aTRx,tr_read,tr_write,tmr);
	}	
}

BusDevice *
R8C23_TimerABNew(const char *name,unsigned int regset)
{
	TimerAB *tmr = sg_new(TimerAB);	
	if(regset >= array_size(trabRegs)) {
                fprintf(stderr,"Illegal register set selection %d\n",regset);
                exit(1);
        }
        tmr->regset_nr = regset;
        tmr->regSet = &trabRegs[regset];
	
	tmr->bdev.first_mapping=NULL;
        tmr->bdev.Map=R8CTimerAB_Map;
        tmr->bdev.UnMap=R8CTimerAB_Unmap;
        tmr->bdev.owner=tmr;
        tmr->bdev.hw_flags=MEM_FLAG_WRITABLE|MEM_FLAG_READABLE;
	tmr->sigIrq = SigNode_New("%s.irq",name);
	tmr->clk_cntr = Clock_New("%s.clk",name);
        Clock_SetFreq(tmr->clk_cntr,2500000);
	CycleTimer_Init(&tmr->event_timer,timerAB_event,tmr);
	#if 0
	 /* As long as clock module doesn't exist */
        Clock_SetFreq(tmr->clk_f1,24000000);
        Clock_SetFreq(tmr->clk_f8,3000000);
        Clock_SetFreq(tmr->clk_f2n,3000000);
        Clock_SetFreq(tmr->clk_fc32,24000000/32);
	#endif
	return &tmr->bdev;
}
