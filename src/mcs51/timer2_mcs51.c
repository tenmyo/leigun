/*
 **********************************************************
 * Timer2
 **********************************************************
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "sgstring.h"
#include "cpu_mcs51.h"
#include "timer2_mcs51.h"
#include "signode.h"
#include "clock.h"
#include "debugvars.h"

#define REG_T2CON 	0xc8
#define		T2CON_TF2	(1 << 7)
#define		T2CON_EXF2	(1 << 6)
#define		T2CON_RCLK	(1 << 5)
#define		T2CON_TCLK	(1 << 4)
#define		T2CON_EXEN2	(1 << 3)
#define		T2CON_TR2	(1 << 2)
#define		T2CON_CT2	(1 << 1)
#define		T2CON_CPRL2	(1 << 0)

#define REG_T2MOD	0xc9
#define		T2MOD_DCEN	(1 << 0)
#define		T2MOD_T2OE	(1 << 1)
#define REG_TH2		0xcd
#define REG_TL2		0xcc
#define REG_RCAP2H	0xcb
#define REG_RCAP2L	0xca

typedef struct Timer2 {
	uint8_t regT2CON;
	uint8_t regT2MOD;
	uint16_t regT2;
	uint8_t regRCAP2H;
	uint8_t regRCAP2L;
	SigNode *sigIrq;
	SigNode *sigT2;
	SigNode *sigT2EX;
	SigTrace *traceT2;
	Clock_t *clkIn;
} Timer2;

/**
 ************************************************************************************
 * Post an interrupt when the Timer overflow flag is set
 ************************************************************************************
 */
static void
update_interrupt(Timer2 * tmr)
{
	if (tmr->regT2CON & T2CON_TF2) {
		SigNode_Set(tmr->sigIrq, SIG_LOW);
	} else {
		SigNode_Set(tmr->sigIrq, SIG_PULLUP);
	}
}

static void
actualize_timer(Timer2 * tmr)
{
}

static void
update_timeout(Timer2 * tmr)
{
}

/**
 **************************************************************
 * The signal trace for the T2 input. Counts up or down
 * when in counter mode.
 **************************************************************
 */
static void
T2Trace(SigNode * sig, int value, void *clientData)
{
	Timer2 *tmr = clientData;
	int dir = 1;
	uint16_t rcap2;
	/* Does it really count positive Edges */
	if (value != SIG_HIGH) {
		return;
	}
	if (tmr->regT2MOD & T2MOD_DCEN) {
		if (SigNode_Val(tmr->sigT2EX) == SIG_HIGH) {
			dir = 1;
		} else {
			dir = -1;
		}
	} else {
		//fprintf(stderr,"Not DCEN\n");
	}
	if (dir > 0) {
		if (tmr->regT2 == 0xffff) {
			rcap2 = tmr->regRCAP2L | ((uint16_t) tmr->regRCAP2H << 8);
			tmr->regT2 = rcap2;
			tmr->regT2CON |= T2CON_TF2;
			update_interrupt(tmr);
		} else {
			tmr->regT2 += dir;
		}
	} else {
		rcap2 = tmr->regRCAP2L | ((uint16_t) tmr->regRCAP2H << 8);
		if (tmr->regT2 == rcap2) {
			tmr->regT2 = 0xffff;
			tmr->regT2CON |= T2CON_TF2;
			//update_interrupt(tmr);
		} else {
			tmr->regT2 += dir;
		}
	}
	//fprintf(stderr,"T2 posedge of input signal dir %d, T2 %u\n",dir, tmr->regT2);
}

static uint8_t
t2con_read(void *eventData, uint8_t addr)
{
	Timer2 *tmr = eventData;
	return tmr->regT2CON;
}

static void
t2con_write(void *eventData, uint8_t addr, uint8_t value)
{
	Timer2 *tmr = eventData;
	uint8_t diff;
	/* Not clear if TF2 can be set ! */
	diff = tmr->regT2CON ^ value;
	tmr->regT2CON = value; 

	if(diff & T2CON_TF2) {
		update_interrupt(tmr);
	}
	if (value & T2CON_CT2) {
		if (!tmr->traceT2) {
			tmr->traceT2 = SigNode_Trace(tmr->sigT2, T2Trace, tmr);
		}
	} else {
		if (tmr->traceT2) {
			SigNode_Untrace(tmr->sigT2, tmr->traceT2);
			tmr->traceT2 = NULL;
		}
	}
}

static uint8_t
t2mod_read(void *eventData, uint8_t addr)
{
	Timer2 *tmr = eventData;
	return tmr->regT2MOD;
}

static void
t2mod_write(void *eventData, uint8_t addr, uint8_t value)
{
	Timer2 *tmr = eventData;
	fprintf(stderr,"Warning, incomplete T2MOD: value 0x%02x\n",value);
	tmr->regT2MOD = value;
}

static uint8_t
th2_read(void *eventData, uint8_t addr)
{
	uint8_t retval;
	Timer2 *tmr = eventData;
	actualize_timer(tmr);
	retval = tmr->regT2 >> 8;
	return retval;
}

static void
th2_write(void *eventData, uint8_t addr, uint8_t value)
{
	Timer2 *tmr = eventData;
	fprintf(stderr, "TH2 write 0x%02x \n", value);
	tmr->regT2 = (tmr->regT2 & 0xff) | ((uint16_t) value << 8);
}

static uint8_t
tl2_read(void *eventData, uint8_t addr)
{
	uint8_t retval;
	Timer2 *tmr = eventData;
	actualize_timer(tmr);
	retval = tmr->regT2 & 0xff;
	//fprintf(stderr, "T2 is %u\n",tmr->regT2);
	return retval;
}

static void
tl2_write(void *eventData, uint8_t addr, uint8_t value)
{
	Timer2 *tmr = eventData;
	fprintf(stderr, "TL2 write 0x%02x \n", value);
	tmr->regT2 = (tmr->regT2 & 0xff00) | (value & 0xff);
}

static uint8_t
rcap2h_read(void *eventData, uint8_t addr)
{
	Timer2 *tmr = eventData;
	return tmr->regRCAP2H;
}

static void
rcap2h_write(void *eventData, uint8_t addr, uint8_t value)
{
	Timer2 *tmr = eventData;
	actualize_timer(tmr);
	fprintf(stderr, "RCAP2H not implemented\n");
	update_timeout(tmr);
}

static uint8_t
rcap2l_read(void *eventData, uint8_t addr)
{
	Timer2 *tmr = eventData;
	return tmr->regRCAP2L;
}

static void
rcap2l_write(void *eventData, uint8_t addr, uint8_t value)
{
	Timer2 *tmr = eventData;
	actualize_timer(tmr);
	fprintf(stderr, "RCAP2L not implemented\n");
	update_timeout(tmr);
}

/**
 ******************************************************************
 * \fn void MCS51Timer2_New(const char *name)
 ******************************************************************
 */
void
MCS51Timer2_New(const char *name)
{
	Timer2 *tmr = sg_new(Timer2);
	tmr->clkIn = Clock_New("%s.clk", name);
	if (!tmr->clkIn) {
		fprintf(stderr, "Can not create Timer2 input clock\n");
		exit(1);
	}
	tmr->sigIrq = SigNode_New("%s.irq", name);
	tmr->sigT2 = SigNode_New("%s.t2", name);
	tmr->sigT2EX = SigNode_New("%s.t2ex", name);
	if (!tmr->sigIrq || !tmr->sigT2 || !tmr->sigT2EX) {
		fprintf(stderr, "Can not create Timer2 signal line\n");
		exit(1);
	}
	MCS51_RegisterSFR(REG_T2CON, t2con_read, NULL, t2con_write, tmr);
	MCS51_RegisterSFR(REG_T2MOD, t2mod_read, NULL, t2mod_write, tmr);
	MCS51_RegisterSFR(REG_TH2, th2_read, NULL, th2_write, tmr);
	MCS51_RegisterSFR(REG_TL2, tl2_read, NULL, tl2_write, tmr);
	MCS51_RegisterSFR(REG_RCAP2H, rcap2h_read, NULL, rcap2h_write, tmr);
	MCS51_RegisterSFR(REG_RCAP2L, rcap2l_read, NULL, rcap2l_write, tmr);
	DbgExport_U16(tmr->regT2,"%s.T2",name);
}
