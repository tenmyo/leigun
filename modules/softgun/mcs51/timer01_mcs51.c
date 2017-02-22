/**
 ***********************************************************
 * The Timers 0 an 1 of 8051
 ***********************************************************
 */

#include <stdint.h>
#include <stdbool.h>
#include <inttypes.h>
#include "cpu_mcs51.h"
#include "sglib.h"
#include "sgstring.h"
#include "timer01_mcs51.h"
#include "clock.h"
#include "cycletimer.h"
#include "signode.h"

#if 0
#define dbgprintf(...) { fprintf(stderr,__VA_ARGS__); }
#else
#define dbgprintf(...)
#endif

#define REG_TCON	0x88
#define 	TCON_TF1	(1 << 7)
#define		TCON_TR1	(1 << 6)
#define		TCON_TF0	(1 << 5)
#define 	TCON_TR0	(1 << 4)
#define		TCON_IE1	(1 << 3)
#define		TCON_IT1	(1 << 2)
#define	 	TCON_IE0	(1 << 1)
#define		TCON_IT0	(1 << 0)
#define REG_TMOD	0x89
#define		TMOD_GATE1	(1 << 7)
#define 	TMOD_CT1	(1 << 6)
#define 	TMOD_M11	(1 << 5)
#define		TOMD_M01	(1 << 4)
#define		TMOD_GATE0	(1 << 3)
#define		TMOD_CT0	(1 << 2)
#define		TMOD_M10	(1 << 1)
#define		TMOD_M00	(1 << 0)
#define REG_TH0		0x8c
#define REG_TL0		0x8a
#define REG_TH1		0x8d
#define REG_TL1		0x8b

typedef struct Timer {
	Clock_t *clkIn;
	const char *name;
	uint8_t regTCON;
	uint8_t regTMOD;
	uint8_t regTH0;
	uint8_t regTH1;
	uint8_t regTL0;
	uint8_t regTL1;
	CycleCounter_t lastActTmr0;
	CycleCounter_t lastActTmr1;
	CycleCounter_t accCyclesTmr0;
	CycleCounter_t accCyclesTmr1;
	CycleTimer eventTmr0;
	CycleTimer eventTmr1;
	SigNode *sigINT0;	/* Interrupt input pin INT0 */
	SigNode *sigINT1;	/* Interrupt input pin INT1 */
	SigNode *sigIrq0;	/* Interrupt out (to the Intco) */
	SigNode *sigIrq1;	/* Interrupt out (to the Intco) */
	SigNode *sigAckIrq0;	/* Interrupt acknowledge from the INTCO */
	SigNode *sigAckIrq1;
} Timer;

/*
 *******************************************************
 * Generate the Interupt signals
 *******************************************************
 */
static void
update_interrupt0(Timer * tmr)
{
	if (tmr->regTCON & TCON_TF0) {
		dbgprintf("update Interrupt from Timer0 to LOW\n");
		SigNode_Set(tmr->sigIrq0, SIG_LOW);
	} else {
		dbgprintf("update Interrupt from Timer0 to HIGH\n");
		SigNode_Set(tmr->sigIrq0, SIG_HIGH);
	}
}

static void
update_interrupt1(Timer * tmr)
{
	if (tmr->regTCON & TCON_TF1) {
		//fprintf(stderr,"Post Interrupt TF1\n");
		SigNode_Set(tmr->sigIrq1, SIG_LOW);
	} else {
		//fprintf(stderr,"Unpost Interrupt TF1\n");
		SigNode_Set(tmr->sigIrq1, SIG_HIGH);
	}
}

static void
actualize_timer0(Timer * tmr)
{
	FractionU64_t frac;
	uint64_t acc, counter_cycles, th_cycles;
	uint8_t tl;
	uint16_t period;
	bool tmr0_running;
	int mode;
	CycleCounter_t now = CycleCounter_Get();
	CycleCounter_t cyclesSinceLast = now - tmr->lastActTmr0;
	tmr->lastActTmr0 = now;
	acc = tmr->accCyclesTmr0 + cyclesSinceLast;
	frac = Clock_MasterRatio(tmr->clkIn);
	if ((frac.nom == 0) || (frac.denom == 0)) {
		fprintf(stderr, "Warning, No clock for Timer0\n");
		return;
	}
	counter_cycles = acc * frac.nom / frac.denom;
	acc -= counter_cycles * frac.denom / frac.nom;
	tmr->accCyclesTmr0 = acc;
	mode = (tmr->regTMOD & 3);
	tmr0_running = (tmr->regTCON & TCON_TR0) &&
	    (!(tmr->regTMOD & TMOD_GATE0) || SigNode_Val(tmr->sigINT0) == SIG_HIGH);
	switch (mode) {
	    case 0:		/* 8 Bit + 5 Bit prescaler = 13 Bit */
		    if (tmr0_running) {
			    tl = tmr->regTL0 & 0x1f;
			    th_cycles = (tl + counter_cycles) / 32;
			    tmr->regTL0 = (tl + counter_cycles) % 32;
			    if ((tmr->regTH0 + th_cycles) >= 0x100) {
				    tmr->regTCON |= TCON_TF0;
				    update_interrupt0(tmr);
			    }
			    tmr->regTH0 += th_cycles;
		    }
		    break;
	    case 1:		/* 16 Bit mode */
		    if (tmr0_running) {
			    tl = tmr->regTL0;
			    th_cycles = (tl + counter_cycles) / 256;
			    tmr->regTL0 = tl + counter_cycles;
			    if ((tmr->regTH0 + th_cycles) >= 0x100) {
				    tmr->regTCON |= TCON_TF0;
				    update_interrupt0(tmr);
			    }
			    tmr->regTH0 += th_cycles;
		    }
		    break;

	    case 2:		/* 8 Bit with 8 Bit reload value */
		    if (tmr0_running) {
			    period = tmr->regTH0 + 1;
			    if (counter_cycles + tmr->regTL0 >= 0x100) {
				    tmr->regTCON |= TCON_TF0;
				    update_interrupt0(tmr);
			    }
			    tmr->regTL0 = (counter_cycles + tmr->regTL0) % period;
		    }
		    break;

	    case 3:		/* Two 8 Bit counters */
		    if (tmr0_running) {
			    if (counter_cycles + tmr->regTL0 >= 0x100) {
				    tmr->regTCON |= TCON_TF0;
				    update_interrupt0(tmr);
			    }
			    tmr->regTL0 = counter_cycles + tmr->regTL0;
		    }
		    if (tmr->regTCON & TCON_TR1) {
			    if (counter_cycles + tmr->regTH0 >= 0x100) {
				    tmr->regTCON |= TCON_TF1;
				    update_interrupt1(tmr);
			    }
			    tmr->regTH0 = counter_cycles + tmr->regTH0;
		    }
		    break;
	}
}

static void
actualize_timer1(Timer * tmr)
{
	FractionU64_t frac;
	uint64_t acc, counter_cycles, th_cycles;
	uint8_t tl;
	uint16_t period;
	bool tmr1_running;
	int mode;

	CycleCounter_t now = CycleCounter_Get();
	CycleCounter_t cyclesSinceLast = now - tmr->lastActTmr1;
	tmr->lastActTmr1 = now;
	acc = tmr->accCyclesTmr1 + cyclesSinceLast;
	frac = Clock_MasterRatio(tmr->clkIn);
	if ((frac.nom == 0) || (frac.denom == 0)) {
		fprintf(stderr, "Warning, No clock for Timer1\n");
		return;
	}
	counter_cycles = acc * frac.nom / frac.denom;
	acc -= counter_cycles * frac.denom / frac.nom;
	tmr->accCyclesTmr1 = acc;
	mode = ((tmr->regTMOD >> 4) & 3);
	tmr1_running = (tmr->regTCON & TCON_TR1) &&
	    (!(tmr->regTMOD & TMOD_GATE1) || SigNode_Val(tmr->sigINT1) == SIG_HIGH);
	switch (mode) {
	    case 0:		/* 8 Bit + 5 Bit prescaler = 13 Bit */
		    if (tmr1_running) {
			    tl = tmr->regTL1 & 0x1f;
			    th_cycles = (tl + counter_cycles) / 32;
			    tmr->regTL1 = (tl + counter_cycles) % 32;
			    if ((tmr->regTH1 + th_cycles) >= 0x100) {
				    tmr->regTCON |= TCON_TF1;
				    update_interrupt1(tmr);
			    }
			    tmr->regTH1 += th_cycles;
		    }
		    break;

	    case 1:		/* 16 Bit mode */
		    if (tmr1_running) {
			    tl = tmr->regTL1;
			    th_cycles = (tl + counter_cycles) / 256;
			    tmr->regTL1 = tl + counter_cycles;
			    if ((tmr->regTH1 + th_cycles) >= 0x100) {
				    tmr->regTCON |= TCON_TF1;
				    update_interrupt1(tmr);
			    }
			    tmr->regTH1 += th_cycles;
		    }
		    break;
	    case 2:		/* 8 Bit with 8 Bit reload value */
		    if (tmr1_running) {
			    period = tmr->regTH1 + 1;
			    if (counter_cycles + tmr->regTL1 >= 0x100) {
				    tmr->regTCON |= TCON_TF1;
				    update_interrupt1(tmr);
			    }
			    tmr->regTL1 = (counter_cycles + tmr->regTL1) % period;
		    }
		    break;
	    case 3:		/* Stopped */
		    break;

	}
}

static void
update_timeout0(Timer * tmr)
{
	int mode;
	mode = ((tmr->regTMOD >> 0) & 3);
	uint16_t cnt;
	int32_t tmout_cycles = -1;
	bool tmr1_running = tmr->regTCON & TCON_TR1;
	bool tmr0_running = (tmr->regTCON & TCON_TR0) &&
	    (!(tmr->regTMOD & TMOD_GATE0) || SigNode_Val(tmr->sigINT0) == SIG_HIGH);

	switch (mode) {
	    case 0:
		    if (tmr0_running) {
			    cnt = (tmr->regTL0 & 0x1f) | ((uint16_t) tmr->regTH0 << 5);
			    tmout_cycles = (UINT32_C(1) << 14) - cnt;
		    }
		    break;
	    case 1:
		    if (tmr0_running) {
			    cnt = (tmr->regTL0) | ((uint16_t) tmr->regTH0 << 8);
			    tmout_cycles = (UINT32_C(1) << 16) - cnt;
			    dbgprintf("tmr0 mode %u %u, TL0 %u TH0 %u\n", mode, tmout_cycles,
				      tmr->regTL0, tmr->regTH0);
		    }
		    break;
	    case 2:
		    if (tmr0_running) {
			    tmout_cycles = 0x100 - tmr->regTL0;
		    }
		    break;
	    case 3:
		    if (tmr0_running) {
			    tmout_cycles = 0x100 - tmr->regTL0;
		    }
		    if (tmr1_running) {
			    if ((tmout_cycles >= 0) && (tmr->regTH0 > tmr->regTL0)) {
				    tmout_cycles = 0x100 - tmr->regTH0;
			    }
		    }
		    break;
	    default:
		    break;
	}
	if (tmout_cycles >= 0) {
		uint64_t cycles;
		FractionU64_t frac;
		frac = Clock_MasterRatio(tmr->clkIn);
		if ((frac.nom == 0) || (frac.denom == 0)) {
			fprintf(stderr, "Warning, No clock for Timer1\n");
			return;
		}
		cycles = (tmout_cycles * frac.denom) / frac.nom;
		dbgprintf("New timeout in %" PRIu64 " cycles\n", cycles);
		CycleTimer_Mod(&tmr->eventTmr0, cycles);
	} else {
		CycleTimer_Remove(&tmr->eventTmr0);
	}
}

static void
update_timeout1(Timer * tmr)
{
	int mode;
	mode = ((tmr->regTMOD >> 4) & 3);
	uint16_t cnt;
	int32_t tmout_cycles = -1;
	bool tmr1_running = (tmr->regTCON & TCON_TR1) &&
	    (!(tmr->regTMOD & TMOD_GATE1) || SigNode_Val(tmr->sigINT1) == SIG_HIGH);
	if (!tmr1_running) {
		//Timer_Cancel(timeoutoutEventTimer);
		return;
	}
	switch (mode) {
	    case 0:
		    cnt = (tmr->regTL1 & 0x1f) | ((uint16_t) tmr->regTH1 << 5);
		    tmout_cycles = (UINT32_C(1) << 14) - cnt;
		    break;
	    case 1:
		    cnt = (tmr->regTL1) | ((uint16_t) tmr->regTH1 << 8);
		    tmout_cycles = (UINT32_C(1) << 16) - cnt;
		    break;
	    case 2:
		    tmout_cycles = 0x100 - tmr->regTL0;
		    break;
	    case 3:
		    break;
	    default:
		    break;
	}
	if (tmout_cycles >= 0) {
		uint64_t cycles;
		FractionU64_t frac;
		frac = Clock_MasterRatio(tmr->clkIn);
		if ((frac.nom == 0) || (frac.denom == 0)) {
			fprintf(stderr, "Warning, No clock for Timer1\n");
			return;
		}
		cycles = (tmout_cycles * frac.denom) / frac.nom;
		CycleTimer_Mod(&tmr->eventTmr1, cycles);
	} else {
		CycleTimer_Remove(&tmr->eventTmr1);
	}
}

static void
TMR0_Event(void *eventData)
{
	Timer *tmr = eventData;
	actualize_timer0(tmr);
	update_timeout0(tmr);

}

static void
TMR1_Event(void *eventData)
{
	Timer *tmr = eventData;
	actualize_timer1(tmr);
	update_timeout1(tmr);
}

static uint8_t
tcon_read(void *eventData, uint8_t addr)
{
	Timer *tmr = eventData;
	return tmr->regTCON;
}

static void
tcon_write(void *eventData, uint8_t addr, uint8_t value)
{
	Timer *tmr = eventData;
	actualize_timer0(tmr);
	actualize_timer1(tmr);
	tmr->regTCON = value;
	update_timeout0(tmr);
	update_timeout1(tmr);

}

static uint8_t
tmod_read(void *eventData, uint8_t addr)
{
	Timer *tmr = eventData;
	return tmr->regTMOD;
}

static void
tmod_write(void *eventData, uint8_t addr, uint8_t value)
{
	Timer *tmr = eventData;
	actualize_timer0(tmr);
	actualize_timer1(tmr);
	tmr->regTMOD = value;
	update_timeout0(tmr);
	update_timeout1(tmr);
}

static uint8_t
th0_read(void *eventData, uint8_t addr)
{
	Timer *tmr = eventData;
	actualize_timer0(tmr);
	return tmr->regTH0;
}

static void
th0_write(void *eventData, uint8_t addr, uint8_t value)
{
	Timer *tmr = eventData;
	actualize_timer0(tmr);
	tmr->regTH0 = value;
	update_timeout0(tmr);
}

static uint8_t
tl0_read(void *eventData, uint8_t addr)
{
	Timer *tmr = eventData;
	actualize_timer0(tmr);
	return tmr->regTL0;
}

static void
tl0_write(void *eventData, uint8_t addr, uint8_t value)
{
	Timer *tmr = eventData;
	actualize_timer0(tmr);
	tmr->regTL0 = value;
	update_timeout0(tmr);
}

static uint8_t
th1_read(void *eventData, uint8_t addr)
{
	Timer *tmr = eventData;
	actualize_timer1(tmr);
	return tmr->regTH1;
}

static void
th1_write(void *eventData, uint8_t addr, uint8_t value)
{
	Timer *tmr = eventData;
	actualize_timer1(tmr);
	tmr->regTH1 = value;
	update_timeout1(tmr);
}

static uint8_t
tl1_read(void *eventData, uint8_t addr)
{
	Timer *tmr = eventData;
	actualize_timer1(tmr);
	return tmr->regTL1;
}

static void
tl1_write(void *eventData, uint8_t addr, uint8_t value)
{
	Timer *tmr = eventData;
	actualize_timer1(tmr);
	tmr->regTL1 = value;
	update_timeout1(tmr);
}

static void
Timer0_IntAck(struct SigNode *sig, int value, void *eventData)
{
	Timer *tmr = eventData;
	if (value == SIG_LOW) {
		dbgprintf("Timer0 Ack Int\n");
		tmr->regTCON &= ~TCON_TF0;
		update_interrupt0(tmr);
	}
}

static void
Timer1_IntAck(struct SigNode *sig, int value, void *eventData)
{
	Timer *tmr = eventData;
	if (value == SIG_LOW) {
		tmr->regTCON &= ~TCON_TF1;
		update_interrupt1(tmr);
	}
}

/**
 *************************************************************************************
 * Create New timer0/timer1
 *************************************************************************************
 */
void
MCS51Timer01_New(const char *name)
{
	Timer *tmr = sg_new(Timer);
	/* The input clock is the already divided FTxClCLOCK */
	tmr->clkIn = Clock_New("%s.clk", name);
	tmr->name = strdup(name);
	tmr->sigINT0 = SigNode_New("%s.INT0", name);
	tmr->sigINT1 = SigNode_New("%s.INT1", name);
	tmr->sigIrq0 = SigNode_New("%s0.irq", name);
	tmr->sigIrq1 = SigNode_New("%s1.irq", name);
	tmr->sigAckIrq0 = SigNode_New("%s0.ackIrq", name);
	tmr->sigAckIrq1 = SigNode_New("%s1.ackIrq", name);
	if (!tmr->sigINT0 || !tmr->sigINT1 || !tmr->sigIrq0 || !tmr->sigIrq0 || !tmr->sigIrq1
	    || !tmr->sigAckIrq0 || !tmr->sigAckIrq1) {
		fprintf(stderr, "Can not create signal lines for Timer0/1\n");
		exit(1);
	}
	SigNode_Set(tmr->sigAckIrq0, SIG_PULLUP);
	SigNode_Set(tmr->sigAckIrq1, SIG_PULLUP);

	SigNode_Trace(tmr->sigAckIrq0, Timer0_IntAck, tmr);
	SigNode_Trace(tmr->sigAckIrq1, Timer1_IntAck, tmr);

	MCS51_RegisterSFR(REG_TCON, tcon_read, NULL, tcon_write, tmr);
	MCS51_RegisterSFR(REG_TMOD, tmod_read, NULL, tmod_write, tmr);
	MCS51_RegisterSFR(REG_TH0, th0_read, NULL, th0_write, tmr);
	MCS51_RegisterSFR(REG_TL0, tl0_read, NULL, tl0_write, tmr);
	MCS51_RegisterSFR(REG_TH1, th1_read, NULL, th1_write, tmr);
	MCS51_RegisterSFR(REG_TL1, tl1_read, NULL, tl1_write, tmr);
	CycleTimer_Init(&tmr->eventTmr0, TMR0_Event, tmr);
	CycleTimer_Init(&tmr->eventTmr1, TMR1_Event, tmr);
}
