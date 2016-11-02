/**
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "sgstring.h"
#include "signode.h"
#include "bus.h"
#include "m16c65_timerAB.h"
#include "cycletimer.h"
#include "clock.h"

typedef struct TrRegisters {
	uint32_t aTABx;
	uint32_t aTABxMR;
	int8_t onsf_bit;
	int8_t tabsr_bit;
} TrRegisters;

/* Common Registers */
#define REG_TBSR	0x300
#define REG_TABSR	0x320
#define REG_ONSF	0x322
#define REG_TRGSR	0x323
#define REG_UDF		0x324

static TrRegisters trRegs[] = {
	{
	 /* Timer A0 */
	 .aTABx = 0x326,
	 .aTABxMR = 0x336,
	 .onsf_bit = 0,
	 .tabsr_bit = 0,
	 },
	{
	 /* Timer A1 */
	 .aTABx = 0x328,
	 .aTABxMR = 0x337,
	 .onsf_bit = 1,
	 .tabsr_bit = 1,
	 },
	{
	 /* Timer A2 */
	 .aTABx = 0x32a,
	 .aTABxMR = 0x338,
	 .onsf_bit = 2,
	 .tabsr_bit = 2,
	 },
	{
	 /* Timer A3 */
	 .aTABx = 0x32c,
	 .aTABxMR = 0x339,
	 .onsf_bit = 3,
	 .tabsr_bit = 3,
	 },
	{
	 /* Timer A4 */
	 .aTABx = 0x32e,
	 .aTABxMR = 0x33a,
	 .onsf_bit = 4,
	 .tabsr_bit = 4,
	 },
	{
	 /* Timer B0 */
	 .aTABx = 0x330,
	 .aTABxMR = 0x33b,
	 .onsf_bit = -1,
	 .tabsr_bit = 5,
	 },
	{
	 /* Timer B1 */
	 .aTABx = 0x332,
	 .aTABxMR = 0x33c,
	 .onsf_bit = -1,
	 .tabsr_bit = 6,
	 },
	{
	 /* Timer B2 */
	 .aTABx = 0x334,
	 .aTABxMR = 0x33d,
	 .onsf_bit = -1,
	 .tabsr_bit = 7,
	 },
	{
	 /* Timer B3 */
	 .aTABx = 0x310,
	 .aTABxMR = 0x31b,
	 .onsf_bit = -1,
	 .tabsr_bit = 8,
	 },
	{
	 /* Timer B4 */
	 .aTABx = 0x312,
	 .aTABxMR = 0x31c,
	 .onsf_bit = -1,
	 .tabsr_bit = 9,
	 },
	{
	 /* Timer B5 */
	 .aTABx = 0x314,
	 .aTABxMR = 0x31d,
	 .onsf_bit = -1,
	 .tabsr_bit = 10,
	 },
};

#define TABxMR_TMOD_MSK	(0x3)
#define 	TMOD_TIMER	(0)
#define		TMOD_EVCNT	(1)
#define		TMOD_PULSE	(2)

typedef struct TimerAB TimerAB;

typedef struct TimerModule {
	TimerAB *timerA[5];
	TimerAB *timerB[6];
	uint16_t regTABSR;
	uint8_t regTAONSF;
} TimerModule;

static TimerModule *g_TimerModule = NULL;

struct TimerAB {
	BusDevice bdev;
	char *name;
	TimerModule *tmodule;
	int regset_nr;
	TrRegisters *regSet;
	CycleCounter_t last_actualized;
	CycleCounter_t accumulated_cycles;
	CycleTimer event_timer;
	Clock_t *clk_cntr;
	SigNode *sigIrq;
	uint16_t regTABxReload;
	uint16_t regTABxCnt;
	uint8_t regTABxMR;
};

static void
actualize_counter(TimerAB * tmr)
{

	FractionU64_t frac;
	TimerModule *tmodule = tmr->tmodule;
	TrRegisters *regs = tmr->regSet;
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
	if ((frac.nom == 0) || (frac.denom == 0)) {
		fprintf(stderr, "Warning, No clock for timer module\n");
		return;
	}
	counter_cycles = acc * frac.nom / frac.denom;
	acc -= counter_cycles * frac.denom / frac.nom;
	tmr->accumulated_cycles = acc;
	period = tmr->regTABxReload + 1;
	tmr_mode = tmr->regTABxMR & TABxMR_TMOD_MSK;
	switch (tmr_mode) {
	    case TMOD_TIMER:
		    count = tmr->regTABxCnt;
		    if (counter_cycles > count) {
			    SigNode_Set(tmr->sigIrq, SIG_LOW);
			    SigNode_Set(tmr->sigIrq, SIG_HIGH);
			    count -= counter_cycles;
			    count = (count % period) + period;
			    tmr->regTABxCnt = count;
			    //fprintf(stderr,"%s: New count %lld, period %d, Reload %d\n",tmr->name,count,period,tmr->regTABxReload);
		    } else {
			    count -= counter_cycles;
		    }
		    //tm->tb_count = 
		    break;

	    case TMOD_PULSE:
		    if (regs->onsf_bit >= 0) {
			    tmodule->regTAONSF &= ~(1 << regs->onsf_bit);
		    } else {
			    fprintf(stderr, "One shot mode not existing\n");
		    }
		    //fprintf(stderr,"PM\n");
		    count = tmr->regTABxCnt;
		    if (counter_cycles > count) {
			    SigNode_Set(tmr->sigIrq, SIG_LOW);
			    SigNode_Set(tmr->sigIrq, SIG_HIGH);
			    tmr->regTABxCnt = 0;
		    } else {
			    count -= counter_cycles;
		    }
		    break;
	    default:
		    fprintf(stderr, "TMR-Mode %d not implemented\n", tmr_mode);
		    break;
	}
}

static void
update_timeout(TimerAB * tmr)
{
	TimerModule *tmodule = tmr->tmodule;
	TrRegisters *regs = tmr->regSet;
	FractionU64_t frac;
	int tmr_mode;
	uint64_t timer_cycles;
	uint64_t cpu_cycles;
	tmr_mode = tmr->regTABxMR & TABxMR_TMOD_MSK;
	frac = Clock_MasterRatio(tmr->clk_cntr);
	if (!frac.nom) {
		return;
	}
	switch (tmr_mode) {
	    case TMOD_PULSE:
		    if (regs->onsf_bit >= 0) {
			    if (!(tmodule->regTAONSF & (1 << regs->onsf_bit))) {
				    break;
			    }
		    } else {
			    fprintf(stderr, "Unknown timer mode %d\n", tmr_mode);
			    break;
		    }
	    case TMOD_TIMER:
		    if (regs->tabsr_bit >= 0) {
			    if (!(tmodule->regTABSR & (1 << regs->tabsr_bit))) {
				    break;
			    }
		    } else {
			    fprintf(stderr, "No start bit for timer\n");
		    }
		    timer_cycles = tmr->regTABxCnt + 1;
		    cpu_cycles = (timer_cycles * frac.denom) / frac.nom;
		    //fprintf(stderr,"sleep for %lld cpu cycles, %lld timer cycles\n",cpu_cycles,timer_cycles);
		    CycleTimer_Mod(&tmr->event_timer, cpu_cycles);
		    break;
	    default:
		    fprintf(stderr, "Unknown timer mode\n");
	}

}

static void
timerAB_event(void *clientData)
{
	TimerAB *tmr = (TimerAB *) clientData;
	actualize_counter(tmr);
	update_timeout(tmr);
	//fprintf(stderr,"Event at %llu\n",CycleCounter_Get());
}

static uint32_t
trmr_read(void *clientData, uint32_t address, int rqlen)
{
	TimerAB *tmr = clientData;
	return tmr->regTABxMR;
}

static void
trmr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	TimerAB *tmr = clientData;
	actualize_counter(tmr);
	tmr->regTABxMR = value;
	update_timeout(tmr);
}

static uint32_t
tr_read(void *clientData, uint32_t address, int rqlen)
{
	return 0;
}

static void
tr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	TimerAB *tmr = clientData;
	actualize_counter(tmr);
	tmr->regTABxReload = value;
	fprintf(stderr, "RELOAD value %08x\n", value);
	update_timeout(tmr);
}

static uint32_t
taonsf_read(void *clientData, uint32_t address, int rqlen)
{
	TimerModule *tmodule = clientData;
	return tmodule->regTAONSF;
}

static void
taonsf_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	TimerModule *tmodule = clientData;
	int i;
	tmodule->regTAONSF = value;
	fprintf(stderr, "ONSF write %02x\n", value);
	for (i = 0; i < 5; i++) {
		if (tmodule->timerA[i]) {
			update_timeout(tmodule->timerA[i]);
		}
	}

}

static uint32_t
tabsr_read(void *clientData, uint32_t address, int rqlen)
{
	TimerModule *tmodule = clientData;
	return tmodule->regTABSR & 0xff;
}

static void
tabsr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	TimerModule *tmodule = clientData;
	int i;
	tmodule->regTABSR = (tmodule->regTABSR & 0xff00) | value;
	fprintf(stderr, "TABSR write %02x\n", value);
	for (i = 0; i < 5; i++) {
		if (tmodule->timerA[i]) {
			update_timeout(tmodule->timerA[i]);
		}
	}
	for (i = 0; i < 3; i++) {
		if (tmodule->timerB[i]) {
			update_timeout(tmodule->timerB[i]);
		}
	}
}

static uint32_t
tbsr_read(void *clientData, uint32_t address, int rqlen)
{
	TimerModule *tmodule = clientData;
	return (tmodule->regTABSR >> 8) & 0xff;
}

static void
tbsr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	TimerModule *tmodule = clientData;
	int i;
	tmodule->regTABSR = (tmodule->regTABSR & 0xff) | (value << 8);
	fprintf(stderr, "TBSR write %02x\n", value);
	for (i = 3; i < 6; i++) {
		if (tmodule->timerB[i]) {
			update_timeout(tmodule->timerB[i]);
		}
	}
}

static void
M16CTimerAB_Unmap(void *owner, uint32_t base, uint32_t mask)
{
	TimerAB *tmr = owner;
	TrRegisters *regs = tmr->regSet;
	IOH_Delete16(regs->aTABx);
	IOH_Delete8(regs->aTABxMR);
}

static void
M16CTimerAB_Map(void *owner, uint32_t base, uint32_t mask, uint32_t mapflags)
{
	TimerAB *tmr = owner;
	TrRegisters *regs = tmr->regSet;
	uint32_t flags = IOH_FLG_HOST_ENDIAN | IOH_FLG_PRD_RARP | IOH_FLG_PA_CBSE;
	IOH_New8(regs->aTABxMR, trmr_read, trmr_write, tmr);
	IOH_New16f(regs->aTABx, tr_read, tr_write, tmr, flags);
}

BusDevice *
M16C65_TimerABNew(const char *name, unsigned int regset)
{
	TimerAB *tmr = sg_new(TimerAB);
	if (regset >= array_size(trRegs)) {
		fprintf(stderr, "Illegal register set selection %d\n", regset);
		exit(1);
	}
	if (!g_TimerModule) {
		g_TimerModule = sg_new(TimerModule);
		IOH_New8(REG_ONSF, taonsf_read, taonsf_write, g_TimerModule);
		IOH_New8(REG_TABSR, tabsr_read, tabsr_write, g_TimerModule);
		IOH_New8(REG_TBSR, tbsr_read, tbsr_write, g_TimerModule);
	}
	if (regset <= 5) {
		g_TimerModule->timerA[regset] = tmr;
	} else if (regset <= 11) {
		g_TimerModule->timerB[regset - 5] = tmr;
	}
	tmr->name = sg_strdup(name);
	tmr->tmodule = g_TimerModule;
	tmr->regset_nr = regset;
	tmr->regSet = &trRegs[regset];
	tmr->bdev.first_mapping = NULL;
	tmr->bdev.Map = M16CTimerAB_Map;
	tmr->bdev.UnMap = M16CTimerAB_Unmap;
	tmr->bdev.owner = tmr;
	tmr->bdev.hw_flags = MEM_FLAG_WRITABLE | MEM_FLAG_READABLE;
	tmr->sigIrq = SigNode_New("%s.irq", name);
	tmr->clk_cntr = Clock_New("%s.clk", name);
	if (!tmr->sigIrq || !tmr->clk_cntr) {
		fprintf(stderr, "Can not create Timer \"%s\"\n", name);
		exit(1);
	}
	Clock_SetFreq(tmr->clk_cntr, 14318000);
	CycleTimer_Init(&tmr->event_timer, timerAB_event, tmr);
	return &tmr->bdev;
}
