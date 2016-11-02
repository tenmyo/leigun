/**
 * System Controller / Clock generation circuit of RX63N
 */

#include <stdint.h>
#include <inttypes.h>
#include "bus.h"
#include "sglib.h"
#include "sgstring.h"
#include "clk_rx63n.h"
#include "clock.h"
#include "signode.h"
#include "configfile.h"


#define REG_MDMONR(base)	0x80000
#define REG_MDSR(base)		0x80002 
#define REG_SYSCR0(base)	0x80006
#define REG_SYSCR1(base)	0x80008
#define REG_SBYCR(base)		0x8000C
#define	REG_MSTPCRA(base)	0x80010		
#define REG_MSTPCRB(base)	0x80014
#define REG_MSTPCRC(base)	0x80018
#define REG_MSTPCRD(base)	0x8001C


#define REG_SCKCR(base)		0x80020
#define REG_SCKCR2(base)	0x80024
#define REG_SCKCR3(base)	0x80026
#define REG_PLLCR(base)		0x80028
#define REG_PLLCR2(base)	0x8002a
#define		PLLCR2_PLLEN	(1)
#define	REG_BCKCR(base)		0x80030
#define REG_MOSCCR(base)	0x80032
#define		MOSCCR_MOSTP	(1)
#define REG_SOSCCR(base)	0x80033
#define REG_LOCOCR(base)	0x80034
#define REG_ILOCOCR(base)	0x80035
#define REG_HOCOCR(base)	0x80036
#define REG_OSTDCR(base)	0x80040
#define REG_OSTDSR(base)	0x80041
#define REG_MOFCR(base)		0x8C293
#define REG_HOCOPCR(base)	0x8C294

const char *gMstpName[128] = {
	NULL,
	NULL,
	NULL,
	NULL,
	"mstpA4",
	"mstpA5",
	NULL,
	NULL,
	NULL,
	"mstpA9",	
	"mstpA10",	
	"mstpA11",	
	"mstpA12",	
	"mstpA13",	
	"mstpA14",	
	"mstpA15",	
	NULL,
	"mstpA17",	
	NULL,
	"mstpA19",	
	NULL,
	NULL,
	NULL,
	"mstpA23",	
	"mstpA24",	
	NULL,
	NULL,
	"mstpA27",	
	"mstpA28",	
	"mstpA29",	
	NULL,
	"acse",	
	/* B */
	"mstpB0",	
	"mstpB1",	
	"mstpB2",	
	NULL,
	"mstpB4",	
	NULL,
	NULL,
	NULL,
	"mstpB8",	
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	"mstpB15",
	"mstpB16",
	"mstpB17",
	"mstpB18",
	"mstpB19",
	"mstpB20",
	"mstpB21",
	"mstpB22",
	"mstpB23",
	"mstpB24",
	"mstpB25",
	"mstpB26",
	"mstpB27",
	"mstpB28",
	"mstpB29",
	"mstpB30",
	"mstpB31",
	/* C */
	"mstpC0",
	"mstpC1",
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	"mstpC16",
	"mstpC17",
	"mstpC18",
	"mstpC19",
	NULL,
	NULL,
	"mstpC22",
	NULL,
	"mstpC24",
	"mstpC25",
	"mstpC26",
	"mstpC27",
	NULL,
	NULL,
	NULL,
	NULL,
	/* D */
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	"mstpD31",
};

/* Diagram on Page 244 */
typedef struct RxClk {
	BusDevice bdev;
	bool isInit;
	uint32_t subOscFreq;
	uint32_t mainOscFreq;
	SigNode *sigMstp[128];
	Clock_t *clkMainOsc;
	Clock_t *clkSubOsc;
	Clock_t *clkHoco;
	Clock_t *clkLoco;
	Clock_t *clkFDivIn;
	Clock_t *clkPllOut;
	Clock_t *clkDiv1;
	Clock_t *clkDiv2;
	Clock_t *clkDiv4;
	Clock_t *clkDiv8;
	Clock_t *clkDiv16;
	Clock_t *clkDiv32;
	Clock_t *clkDiv64;
	Clock_t *clkDiv6;
	Clock_t *clkDiv3;

	/* The output clocks */
	Clock_t *clkFCLK;
	Clock_t *clkICLK;
	Clock_t *clkPCKA;
	Clock_t *clkPCKB;
	Clock_t *clkBCLK;
	Clock_t *clkBCLKPin;
	Clock_t *clkIEBCK;
	Clock_t *clkUCK;
	Clock_t *clkIWDCLK;
	Clock_t *clkCANMCLK;
	//Clock_t *clkJTAGTCK;
	Clock_t *clkRTCSCLK;
	Clock_t *clkRTCMCLK;
	Clock_t *clkSDCLK;
	Clock_t *clkSDCLKPin;

	uint16_t regMDMONR;
	uint16_t regMDSR;
	uint16_t regSYSCR0;
	uint16_t regSYSCR1;
	uint16_t regSBYCR;
	uint32_t regMSTPCRA;
	uint32_t regMSTPCRB;
	uint32_t regMSTPCRC;
	uint32_t regMSTPCRD;
	
	uint32_t regSCKCR;
	uint16_t regSCKCR2;
	uint16_t regSCKCR3;
	uint16_t regPLLCR;
	uint8_t regPLLCR2;
	uint8_t regBCKCR;
	uint8_t regMOSCCR;
	uint8_t regSOSCCR;
	uint8_t regLOCOCR;
	uint8_t regILOCOCR;
	uint8_t regHOCOCR;
	uint8_t regOSTDCR;
	uint8_t regOSTDSR;
	uint8_t regMOFCR;
	uint8_t regHOCOPCR;
} RxClk;

/**
 * Helper for SCKCR
 */
static Clock_t *
GetClockBySelector(RxClk * rc, uint8_t selector)
{
	Clock_t *clk;
	switch (selector) {
	    case 0:
		    clk = rc->clkDiv1;
		    break;
	    case 1:
		    clk = rc->clkDiv2;
		    break;
	    case 2:
		    clk = rc->clkDiv4;
		    break;
	    case 3:
		    clk = rc->clkDiv8;
		    break;
	    case 4:
		    clk = rc->clkDiv16;
		    break;
	    case 5:
		    clk = rc->clkDiv32;
		    break;
	    case 6:
		    clk = rc->clkDiv64;
		    break;
	    default:
		    clk = rc->clkDiv1;
		    fprintf(stderr, "Illegal Clock selection %u\n", selector);
		    break;

	}
	return clk;
}

static void
mdmonr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "%s not writable\n", __func__);
}

static uint32_t
mdmonr_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "%s not implemented\n", __func__);
	return 0;
}

static void
mdsr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "%s not implemented\n", __func__);
}

static uint32_t
mdsr_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "Started in single chip mode\n");
	return 0;
}

static void
syscr0_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	RxClk *rc = clientData;
    if (value & 0x5a00 != 0x5a00) {
		fprintf(stderr, "Invalid Magic in %s\n", __func__);
        return;
    }
	rc->regSYSCR0 = value & 3;
	if ((rc->regSYSCR0 & 3) != 1) {
		fprintf(stderr, "Warning, unimplemented mode %08x in %s\n", value, __func__);
	}
}

static uint32_t
syscr0_read(void *clientData, uint32_t address, int rqlen)
{
	RxClk *rc = clientData;
	fprintf(stderr, "Read mode %u in %s\n", rc->regSYSCR0, __func__);
	return rc->regSYSCR0;
}

static void
syscr1_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	RxClk *rc = clientData;
	rc->regSYSCR1 = value & 1;
	if ((rc->regSYSCR1 & 1) != 1) {
		fprintf(stderr, "Warning, unimplemented mode %u in %s\n", value, __func__);
	}
}

static uint32_t
syscr1_read(void *clientData, uint32_t address, int rqlen)
{
	RxClk *rc = clientData;
	return rc->regSYSCR1;
}

static void
sbycr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "%s not implemented\n", __func__);
}

static uint32_t
sbycr_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "%s not implemented\n", __func__);
	return 0;
}

static void
mstpcra_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "%s not implemented\n", __func__);
}

static uint32_t
mstpcra_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "%s not implemented\n", __func__);
	return 0;
}

static void
mstpcrb_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "%s not implemented\n", __func__);
}

static uint32_t
mstpcrb_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "%s not implemented\n", __func__);
	return 0;
}

static void
mstpcrc_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "%s not implemented\n", __func__);
}

static uint32_t
mstpcrc_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "%s not implemented\n", __func__);
	return 0;
}

static void
mstpcrd_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "%s not implemented\n", __func__);
}

static uint32_t
mstpcrd_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "%s not implemented\n", __func__);
	return 0;
}


static void
sckcr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	RxClk *rc = clientData;
	uint32_t fck, ick, bck, pcka, pckb;
	uint8_t pstop0, pstop1;
	Clock_t *clk;
	fck = (value >> 28) & 0xf;
	ick = (value >> 24) & 0xf;
	bck = (value >> 16) & 0xf;
	pcka = (value >> 12) & 0xf;
	pckb = (value >> 8) & 0xf;
	pstop0 = (value >> 22) & 1;
	pstop1 = (value >> 23) & 1;
	clk = GetClockBySelector(rc, pckb);
	Clock_MakeDerived(rc->clkPCKB, clk, 1, 1);

	clk = GetClockBySelector(rc, pcka);
	Clock_MakeDerived(rc->clkPCKA, clk, 1, 1);

	clk = GetClockBySelector(rc, bck);
	Clock_MakeDerived(rc->clkBCLK, clk, 1, 1);
	Clock_MakeDerived(rc->clkSDCLK, clk, 1, 1);
	if (pstop0) {
		Clock_MakeDerived(rc->clkSDCLKPin, rc->clkSDCLK, 0, 1);
		// set pin high
	} else {
		Clock_MakeDerived(rc->clkSDCLKPin, rc->clkSDCLK, 1, 1);
	}
	if (pstop1) {
		Clock_MakeDerived(rc->clkBCLKPin, rc->clkBCLK, 0, 1);
		// set pin high
	} else {
		Clock_MakeDerived(rc->clkBCLKPin, rc->clkBCLK, 1, 1);
	}
	clk = GetClockBySelector(rc, ick);
	Clock_MakeDerived(rc->clkICLK, clk, 1, 1);

	clk = GetClockBySelector(rc, fck);
	Clock_MakeDerived(rc->clkFCLK, clk, 1, 1);
	rc->regSCKCR = value;
}

static uint32_t
sckcr_read(void *clientData, uint32_t address, int rqlen)
{
	RxClk *rc = clientData;
	return rc->regSCKCR;
}

static void
sckcr2_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	RxClk *rc = clientData;
	uint32_t iebck, uck;
	iebck = (value >> 0) & 0xf;
	uck = (value >> 4) & 0xf;
	switch (iebck) {
	    case 1:
		    Clock_MakeDerived(rc->clkIEBCK, rc->clkDiv2, 1, 1);
		    break;
	    case 2:
		    Clock_MakeDerived(rc->clkIEBCK, rc->clkDiv4, 1, 1);
		    break;
	    case 3:
		    Clock_MakeDerived(rc->clkIEBCK, rc->clkDiv8, 1, 1);
		    break;
	    case 4:
		    Clock_MakeDerived(rc->clkIEBCK, rc->clkDiv16, 1, 1);
		    break;
	    case 5:
		    Clock_MakeDerived(rc->clkIEBCK, rc->clkDiv32, 1, 1);
		    break;
	    case 6:
		    Clock_MakeDerived(rc->clkIEBCK, rc->clkDiv64, 1, 1);
		    break;
	    case 12:
		    Clock_MakeDerived(rc->clkIEBCK, rc->clkDiv6, 1, 1);
		    break;
	    default:
		    fprintf(stderr, "Bad IEBCK selection\n");
		    Clock_MakeDerived(rc->clkIEBCK, rc->clkDiv1, 0, 1);
		    break;
	}
	switch (uck) {
	    case 1:
		    Clock_MakeDerived(rc->clkUCK, rc->clkDiv2, 1, 1);
		    break;
	    case 2:
		    Clock_MakeDerived(rc->clkUCK, rc->clkDiv3, 1, 1);
		    break;
	    case 3:
		    Clock_MakeDerived(rc->clkUCK, rc->clkDiv4, 1, 1);
		    break;
	    default:
		    fprintf(stderr, "Bad UCK selection %d\n", uck);
		    Clock_MakeDerived(rc->clkUCK, rc->clkDiv1, 0, 1);
		    break;

	}
	rc->regSCKCR2 = value;
}

static uint32_t
sckcr2_read(void *clientData, uint32_t address, int rqlen)
{
	RxClk *rc = clientData;
	return rc->regSCKCR2;
}

static void
sckcr3_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	RxClk *rc = clientData;
	uint8_t clksel = (value >> 8) & 7;
	//Clock_t *clkFDivIn;
	switch (clksel) {
	    case 0:
		    Clock_MakeDerived(rc->clkFDivIn, rc->clkLoco, 1, 1);
		    break;
	    case 1:
		    Clock_MakeDerived(rc->clkFDivIn, rc->clkHoco, 1, 1);
		    break;
	    case 2:
		    Clock_MakeDerived(rc->clkFDivIn, rc->clkMainOsc, 1, 1);
		    break;
	    case 3:
		    Clock_MakeDerived(rc->clkFDivIn, rc->clkSubOsc, 1, 1);
		    break;
	    case 4:
		    Clock_MakeDerived(rc->clkFDivIn, rc->clkPllOut, 1, 1);
		    break;
	    default:
		    fprintf(stderr, "Illegal clock source selection for FDivIn\n");
		    break;
	}
	rc->regSCKCR3 = value & 0x700;
}

static uint32_t
sckcr3_read(void *clientData, uint32_t address, int rqlen)
{
	RxClk *rc = clientData;
	return rc->regSCKCR3;
}

static void
update_pllclock(RxClk *rc) {
	uint32_t plldiv = rc->regPLLCR & 3;
	uint32_t stc = (rc->regPLLCR >> 8) & 63;
	uint32_t mul, div;
	uint64_t freq;
	if ((rc->regPLLCR2 & PLLCR2_PLLEN) != 0) {
		Clock_MakeDerived(rc->clkPllOut, rc->clkMainOsc, 0,1);
		return;
	}
	switch (plldiv) {
	    case 0:
		    div = 1;
		    break;
	    case 1:
		    div = 2;
		    break;
	    case 3:
		    div = 4;
		    break;
	    default:
		    div = 1;
		    fprintf(stderr, "Illegal PLL divider\n");
		    break;
	}
	switch (stc) {
	    case 7:
		    mul = 8;
		    break;
	    case 9:
		    mul = 10;
		    break;
	    case 11:
		    mul = 12;
		    break;
	    case 15:
		    mul = 16;
		    break;
	    case 19:
		    mul = 20;
		    break;
	    case 23:
		    mul = 24;
		    break;
	    case 24:
		    mul = 25;
		    break;
	    case 49:
		    mul = 50;
		    break;
	    default:
		    mul = 1;
		    fprintf(stderr, "Illegal PLL stc setting %u\n", stc);
		    break;
	}
	//fprintf(stderr,"MUL %u, div %u\n",mul,div);
	freq = Clock_Freq(rc->clkMainOsc);
	if (((freq / div) > 16000000) || ((freq / div) < 4000000)) {
		fprintf(stderr, "PLL: Divided Frequency out of range: %" PRIu64 "\n", freq / div);
	}
	Clock_MakeDerived(rc->clkPllOut, rc->clkMainOsc, mul, div);
	freq = Clock_Freq(rc->clkPllOut);
	if ((freq > 200000000) || (freq < 104000000)) {
		if (!rc->isInit) {
			fprintf(stderr, "PLL: Output Frequency out of range %" PRIu64 "\n", freq);
		}
	}
}

static void
pllcr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	RxClk *rc = clientData;
	if ((rc->regPLLCR2 & PLLCR2_PLLEN) == 0) {
		fprintf(stderr, "PLL is operating, can not change PLLCR\n");
		return;
	}
	rc->regPLLCR = value;
	update_pllclock(rc);
	return;
}

static uint32_t
pllcr_read(void *clientData, uint32_t address, int rqlen)
{
	RxClk *rc = clientData;
	return rc->regPLLCR;
}

static void
pllcr2_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	RxClk *rc = clientData;
	uint64_t freq;
	rc->regPLLCR2 = value & 1;
	update_pllclock(rc);
	freq = Clock_Freq(rc->clkFDivIn);
	if(freq == 0) {
		fprintf(stderr,"Crash because CPU clock is disabled\n");
		exit(1);
	}
#if 0
	if(value) {
		if ((rc->regSCKCR3 & 0x700) == 0x400) {
			fprintf(stderr,"Crash because of disabling PLL while in use\n");
			exit(1);
		}
	}
#endif
}

static uint32_t
pllcr2_read(void *clientData, uint32_t address, int rqlen)
{
	RxClk *rc = clientData;
	return rc->regPLLCR2;
}

static void
bckcr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	//RxClk *rc = clientData;
#if 0
	if (value & 1) {
		Clock_MakeDerived(rc->clkBCKLPin, rc->clkBCLK, 1, 2);
	} else {
		Clock_MakeDerived(rc->clkBCKLPin, rc->clkBCLK, 1, 1);
	}
#endif
	fprintf(stderr, "%s not implemented\n", __func__);
}

static uint32_t
bckcr_read(void *clientData, uint32_t address, int rqlen)
{
	//RxClk *clk = clientData;
	fprintf(stderr, "%s not implemented\n", __func__);
	return 0;
}

static void
mosccr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	RxClk *rc = clientData;
	if (value & 1) {
		Clock_SetFreq(rc->clkMainOsc, 0);
	} else {
		Clock_SetFreq(rc->clkMainOsc, rc->mainOscFreq);
	}
	rc->regMOSCCR = value & 1;
}

static uint32_t
mosccr_read(void *clientData, uint32_t address, int rqlen)
{
	RxClk *rc = clientData;
	return rc->regMOSCCR;
}

static void
sosccr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	RxClk *rc = clientData;
	if (value & 1) {
		// RTC ????? missing here
		Clock_SetFreq(rc->clkSubOsc, 0);
	} else {
		Clock_SetFreq(rc->clkSubOsc, rc->subOscFreq);
	}
	rc->regSOSCCR = value & 1;
}

static uint32_t
sosccr_read(void *clientData, uint32_t address, int rqlen)
{
	RxClk *rc = clientData;
	return rc->regSOSCCR;
}

static void
lococr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	RxClk *rc = clientData;
	if (value & 1) {
		Clock_SetFreq(rc->clkLoco, 0);
	} else {
		Clock_SetFreq(rc->clkLoco, 125000);
	}
	rc->regLOCOCR = value & 1;
}

static uint32_t
lococr_read(void *clientData, uint32_t address, int rqlen)
{
	RxClk *rc = clientData;
	return rc->regLOCOCR;
}

static void
ilococr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	RxClk *rc = clientData;
	if (value & 1) {
		Clock_SetFreq(rc->clkIWDCLK, 0);
	} else {
		Clock_SetFreq(rc->clkIWDCLK, 125000);
	}
	rc->regILOCOCR = value & 1;
}

static uint32_t
ilococr_read(void *clientData, uint32_t address, int rqlen)
{
	RxClk *rc = clientData;
	return rc->regILOCOCR;
}

static void
hococr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	RxClk *rc = clientData;
	if (value & 1) {
		Clock_SetFreq(rc->clkHoco, 0);
	} else {
		Clock_SetFreq(rc->clkHoco, 50000000);
	}
	rc->regHOCOCR = value & 1;
}

static uint32_t
hococr_read(void *clientData, uint32_t address, int rqlen)
{
	RxClk *rc = clientData;
	return rc->regHOCOCR;
}

static void
ostdcr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	//RxClk *clk = clientData;
	fprintf(stderr, "%s not implemented\n", __func__);
}

static uint32_t
ostdcr_read(void *clientData, uint32_t address, int rqlen)
{
	//RxClk *clk = clientData;
	fprintf(stderr, "%s not implemented\n", __func__);
	return 0;
}

static void
ostdsr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	//RxClk *clk = clientData;
	fprintf(stderr, "%s not implemented\n", __func__);
}

static uint32_t
ostdsr_read(void *clientData, uint32_t address, int rqlen)
{
	//RxClk *clk = clientData;
	fprintf(stderr, "%s not implemented\n", __func__);
	return 0;
}

static void
mofcr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	//RxClk *clk = clientData;
	fprintf(stderr, "%s not implemented\n", __func__);
}

static uint32_t
mofcr_read(void *clientData, uint32_t address, int rqlen)
{
	//RxClk *clk = clientData;
	fprintf(stderr, "%s not implemented\n", __func__);
	return 0;
}

static void
hocopcr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	//RxClk *clk = clientData;
	fprintf(stderr, "%s not implemented\n", __func__);
}

static uint32_t
hocopcr_read(void *clientData, uint32_t address, int rqlen)
{
	//RxClk *clk = clientData;
	fprintf(stderr, "%s not implemented\n", __func__);
	return 0;
}

static void
OSC_Unmap(void *owner, uint32_t base, uint32_t mask)
{
	IOH_Delete16(REG_MDMONR(base));
	IOH_Delete16(REG_MDSR(base));
	IOH_Delete16(REG_SYSCR0(base));
	IOH_Delete16(REG_SYSCR1(base));
	IOH_Delete16(REG_SBYCR(base));
	IOH_Delete32(REG_MSTPCRA(base));
	IOH_Delete32(REG_MSTPCRB(base));
	IOH_Delete32(REG_MSTPCRC(base));
	IOH_Delete32(REG_MSTPCRD(base));
	IOH_Delete32(REG_SCKCR(base));
	IOH_Delete16(REG_SCKCR2(base));
	IOH_Delete16(REG_SCKCR3(base));
	IOH_Delete16(REG_PLLCR(base));
	IOH_Delete8(REG_PLLCR2(base));
	IOH_Delete8(REG_BCKCR(base));
	IOH_Delete8(REG_MOSCCR(base));
	IOH_Delete8(REG_SOSCCR(base));
	IOH_Delete8(REG_LOCOCR(base));
	IOH_Delete8(REG_ILOCOCR(base));
	IOH_Delete8(REG_HOCOCR(base));
	IOH_Delete8(REG_OSTDCR(base));
	IOH_Delete8(REG_OSTDSR(base));
	IOH_Delete8(REG_MOFCR(base));
	IOH_Delete8(REG_HOCOPCR(base));
}

static void
OSC_Map(void *owner, uint32_t base, uint32_t mask, uint32_t mapflags)
{
	RxClk *clk = owner;
	IOH_New16(REG_MDMONR(base), mdmonr_read, mdmonr_write, clk);
	IOH_New16(REG_MDSR(base), mdsr_read, mdsr_write, clk);
	IOH_New16(REG_SYSCR0(base), syscr0_read, syscr0_write, clk);
	IOH_New16(REG_SYSCR1(base), syscr1_read, syscr1_write, clk);
	IOH_New16(REG_SBYCR(base), sbycr_read, sbycr_write, clk);
	IOH_New32(REG_MSTPCRA(base), mstpcra_read, mstpcra_write, clk);
	IOH_New32(REG_MSTPCRB(base), mstpcrb_read, mstpcrb_write, clk);
	IOH_New32(REG_MSTPCRC(base), mstpcrc_read, mstpcrc_write, clk);
	IOH_New32(REG_MSTPCRD(base), mstpcrd_read, mstpcrd_write, clk);
	IOH_New32(REG_SCKCR(base), sckcr_read, sckcr_write, clk);
	IOH_New16(REG_SCKCR2(base), sckcr2_read, sckcr2_write, clk);
	IOH_New16(REG_SCKCR3(base), sckcr3_read, sckcr3_write, clk);
	IOH_New16(REG_PLLCR(base), pllcr_read, pllcr_write, clk);
	IOH_New8(REG_PLLCR2(base), pllcr2_read, pllcr2_write, clk);
	IOH_New8(REG_BCKCR(base), bckcr_read, bckcr_write, clk);
	IOH_New8(REG_MOSCCR(base), mosccr_read, mosccr_write, clk);
	IOH_New8(REG_SOSCCR(base), sosccr_read, sosccr_write, clk);
	IOH_New8(REG_LOCOCR(base), lococr_read, lococr_write, clk);
	IOH_New8(REG_ILOCOCR(base), ilococr_read, ilococr_write, clk);
	IOH_New8(REG_HOCOCR(base), hococr_read, hococr_write, clk);
	IOH_New8(REG_OSTDCR(base), ostdcr_read, ostdcr_write, clk);
	IOH_New8(REG_OSTDSR(base), ostdsr_read, ostdsr_write, clk);
	IOH_New8(REG_MOFCR(base), mofcr_read, mofcr_write, clk);
	IOH_New8(REG_HOCOPCR(base), hocopcr_read, hocopcr_write, clk);
}

BusDevice *
Rx63nClk_New(const char *name)
{
	RxClk *rc = sg_new(RxClk);
	int i;
	rc->isInit = true;
	rc->mainOscFreq = 12500000;
	rc->subOscFreq = 32768;
	Config_ReadUInt32(&rc->mainOscFreq, "global", "crystal");
	Config_ReadUInt32(&rc->subOscFreq, "global", "subclk");

	rc->bdev.first_mapping = NULL;
	rc->bdev.Map = OSC_Map;
	rc->bdev.UnMap = OSC_Unmap;
	rc->bdev.owner = rc;
	rc->bdev.hw_flags = MEM_FLAG_WRITABLE | MEM_FLAG_READABLE;
	for (i = 0; i < 128; i++) {
		SigNode *sig;
		if (gMstpName[i]) {
			sig = SigNode_New("%s.%s", name, gMstpName[i]);
			if(!sig) {
				fprintf(stderr, "can not create signal %s\n", gMstpName[i]);
				exit(1);
			}
		}
	}
	rc->clkMainOsc = Clock_New("%s.extal", name);;
	rc->clkSubOsc = Clock_New("%s.subosc", name);
	rc->clkHoco = Clock_New("%s.hoco", name);
	rc->clkLoco = Clock_New("%s.loco", name);
	rc->clkFDivIn = Clock_New("%s.fdivin", name);
	rc->clkPllOut = Clock_New("%s.pllout", name);
	rc->clkDiv1 = Clock_New("%s.div1", name);
	rc->clkDiv2 = Clock_New("%s.div2", name);
	rc->clkDiv4 = Clock_New("%s.div4", name);
	rc->clkDiv8 = Clock_New("%s.div8", name);
	rc->clkDiv16 = Clock_New("%s.div16", name);
	rc->clkDiv32 = Clock_New("%s.div32", name);
	rc->clkDiv64 = Clock_New("%s.div64", name);
	rc->clkDiv6 = Clock_New("%s.div6", name);
	rc->clkDiv3 = Clock_New("%s.div3", name);

	/* The output clocks */
	rc->clkFCLK = Clock_New("%s.fclk", name);
	rc->clkICLK = Clock_New("%s.iclk", name);
	rc->clkPCKA = Clock_New("%s.pcka", name);
	rc->clkPCKB = Clock_New("%s.pckb", name);
	rc->clkBCLK = Clock_New("%s.bclk", name);
	rc->clkBCLKPin = Clock_New("%s.bclkpin", name);
	rc->clkIEBCK = Clock_New("%s.iebck", name);
	rc->clkUCK = Clock_New("%s.uck", name);
	rc->clkIWDCLK = Clock_New("%s.iwdclk", name);
	rc->clkCANMCLK = Clock_New("%s.canmclk", name);
	//clkJTAGTCK;
	rc->clkRTCSCLK = Clock_New("%s.rtcsclk", name);
	rc->clkRTCMCLK = Clock_New("%s.rtcmclk", name);
	rc->clkSDCLK = Clock_New("%s.sdclk", name);
	rc->clkSDCLKPin = Clock_New("%s.sdclkpin", name);
	Clock_SetFreq(rc->clkMainOsc, rc->mainOscFreq);
	Clock_SetFreq(rc->clkSubOsc, rc->subOscFreq);
	Clock_MakeDerived(rc->clkDiv1, rc->clkFDivIn, 1, 1);
	Clock_MakeDerived(rc->clkDiv2, rc->clkFDivIn, 1, 2);
	Clock_MakeDerived(rc->clkDiv4, rc->clkFDivIn, 1, 4);
	Clock_MakeDerived(rc->clkDiv8, rc->clkFDivIn, 1, 8);
	Clock_MakeDerived(rc->clkDiv16, rc->clkFDivIn, 1, 16);
	Clock_MakeDerived(rc->clkDiv32, rc->clkFDivIn, 1, 32);
	Clock_MakeDerived(rc->clkDiv64, rc->clkFDivIn, 1, 64);
	Clock_MakeDerived(rc->clkDiv6, rc->clkFDivIn, 1, 6);
	Clock_MakeDerived(rc->clkDiv3, rc->clkFDivIn, 1, 3);
	rc->regPLLCR2 = 1;
#if 0
	Clock_MakeDerived(rc->clkBCLKPin, clk->clkBCLK, 1, 1);
	sckcr_write(clk, 0x02020200, REG_SCKCR(0), 4);
#endif
	sckcr_write(rc, 0, REG_SCKCR(0), 4);
	/* 
	 *********************************************************************
	 * The default value of SCKCR2 is a prohibited one. Checked with the
	 * real board.
	 *********************************************************************
	 */
	rc->regSCKCR2 = 0x11;
	rc->regSCKCR3 = 0;
	rc->regPLLCR = 7 << 8;
	rc->regLOCOCR = 0;
	rc->regILOCOCR = 0;
	rc->regHOCOCR = 0; /* Should depend on OFS register */
	sckcr2_write(rc, rc->regSCKCR2, REG_SCKCR2(0), 2);
	sckcr3_write(rc, rc->regSCKCR3, REG_SCKCR3(0), 2);
	pllcr_write(rc, rc->regPLLCR, REG_PLLCR(0), 2);
	lococr_write(rc, rc->regLOCOCR, REG_LOCOCR(0), 1);
	ilococr_write(rc, rc->regILOCOCR, REG_ILOCOCR(0), 1);
	hococr_write(rc, rc->regHOCOCR, REG_HOCOCR(0), 1);
	rc->isInit = false;
	return &rc->bdev;
}
