/**
 ******************************************************************
 * Clock generator of the RX-111
 ******************************************************************
 */

#include <stdint.h>
#include <inttypes.h>
#include "bus.h"
#include "sglib.h"
#include "sgstring.h"
#include "clk_rx111.h"
#include "clock.h"
#include "signode.h"
#include "configfile.h"

#define REG_SCKCR(base)     0x00080020
#define REG_SCKCR3(base)    0x00080026
#define REG_PLLCR(base)     0x00080028
#define REG_PLLCR2(base)    0x0008002a
#define     PLLCR2_PLLEN    (1 << 0)
#define REG_MOSCCR(base)    0x00080032
#define REG_SOSCCR(base)    0x00080033
#define REG_LOCOCR(base)    0x00080034
#define REG_ILOCOCR(base)   0x00080035
#define REG_HOCOCR(base)    0x00080036
#define REG_OSCOVFSR(base)  0x0008003c
#define     OSCOVFSR_MOOVF  (1 << 0)
#define     OSCOVFSR_PLOVF  (1 << 2)
#define     OSCOVFSR_HCOVF  (1 << 3)
#define REG_OSTDCR(base)    0x00080040
#define REG_OSTDSR(base)    0x00080041
#define REG_MOSCWTCR(base)  0x000800a2
#define REG_HOCOWTCR(base)  0x000800a5
#define REG_CKOCR(base)     0x000800e3
#define     CKOCR_CKOSTP    (1 << 15)
#define REG_MOFCR(base)     0x0008c293

typedef struct RxClk {
    BusDevice bdev;
    uint32_t subOscFreq;
    uint32_t mainOscFreq;
    uint32_t locoOscFreq;
    uint32_t iwdtOscFreq;
    uint32_t hocoOscFreq;

    Clock_t *clkMainOsc;
    Clock_t *clkSubOsc;
    Clock_t *clkHoco;
    Clock_t *clkLoco;
    Clock_t *clkIwdt;
    Clock_t *clkFDivIn;
    Clock_t *clkPllOut;
    Clock_t *clkDiv1;
    Clock_t *clkDiv2;
    Clock_t *clkDiv4;
    Clock_t *clkDiv8;
    Clock_t *clkDiv16;
    Clock_t *clkDiv32;
    Clock_t *clkDiv64;

    /* The output clocks */
    Clock_t *clkFCLK;
    Clock_t *clkICLK;
    Clock_t *clkPCKB;
    Clock_t *clkPCKD;
    Clock_t *clkCLKOUT;

    uint32_t regSCKCR;
    uint16_t regSCKCR3;
    uint16_t regPLLCR;
    uint8_t regPLLCR2;
    uint8_t regMOSCCR;
    uint8_t regSOSCCR;
    uint8_t regLOCOCR;
    uint8_t regILOCOCR;
    uint8_t regHOCOCR;
    uint8_t regOSCOVFSR;
    uint8_t regOSTDCR;
    uint8_t regOSTDSR;
    uint8_t regMOSCWTCR;
    uint8_t regHOCOWTCR;
    uint8_t regCKOCR;
    uint8_t regMOFCR;
} RxClk;

/**
 ************************************************************
 * Helper for SCKCR
 ************************************************************
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

static uint32_t
sckcr_read(void *clientData, uint32_t address, int rqlen)
{
    RxClk *rc = clientData;
    return rc->regSCKCR;
}

static void
sckcr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
    RxClk *rc = clientData;
    uint32_t fck, ick, pckb, pckd;
    Clock_t *clk;
    fck = (value >> 28) & 0xf;
    ick = (value >> 24) & 0xf;
    pckb = (value >> 8) & 0xf;
    pckd = (value >> 0) & 0xf;

    clk = GetClockBySelector(rc, pckb);
    Clock_MakeDerived(rc->clkPCKD, clk, 1, 1);

    clk = GetClockBySelector(rc, pckb);
    Clock_MakeDerived(rc->clkPCKB, clk, 1, 1);

    clk = GetClockBySelector(rc, ick);
    Clock_MakeDerived(rc->clkICLK, clk, 1, 1);

    clk = GetClockBySelector(rc, fck);
    Clock_MakeDerived(rc->clkFCLK, clk, 1, 1);

    rc->regSCKCR = value & 0xff000f0f;;
}

static uint32_t
sckcr3_read(void *clientData, uint32_t address, int rqlen)
{
    RxClk *rc = clientData;
    return rc->regSCKCR3; 
}

static void
sckcr3_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
    RxClk *rc = clientData;
    uint8_t clksel = (value >> 8) & 7;
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
        case 2:
            div = 4;
            break;
        default:
            div = 1;
            fprintf(stderr, "Illegal PLL divider\n");
            break;
    }
    switch (stc) {
        case 0xb:
            mul = 6;
            break;
        case 0xf:
            mul = 8;
            break;
        default:
            mul = 1;
            fprintf(stderr, "Illegal PLL stc setting %u\n", stc);
            break;
    }
    freq = Clock_Freq(rc->clkMainOsc);
    if (((freq / div) > 8000000) || ((freq / div) < 4000000)) {
        fprintf(stderr, "PLL: Divided Frequency out of range: %" PRIu64 "\n", freq / div);
    }
    Clock_MakeDerived(rc->clkPllOut, rc->clkMainOsc, mul, div);
    freq = Clock_Freq(rc->clkPllOut);
    if ((freq > (48 * 1000 * 1000)) || (freq < 32000000)) {
        //if (!rc->isInit) {
            fprintf(stderr, "PLL: Output Frequency out of range %" PRIu64 "\n", freq);
        //}
    }
}

static uint32_t
pllcr_read(void *clientData, uint32_t address, int rqlen)
{
    RxClk *rc = clientData;
    return rc->regPLLCR;
}

static void
pllcr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
    RxClk *rc = clientData;
    if ((rc->regPLLCR2 & PLLCR2_PLLEN) == 0) {
        fprintf(stderr, "PLL is operating, can not change PLLCR\n");
        return;
    }
    rc->regPLLCR = value & 0x3f03;
    update_pllclock(rc);
    return;
}

static uint32_t
pllcr2_read(void *clientData, uint32_t address, int rqlen)
{
    RxClk *rc = clientData;
    return rc->regPLLCR2; 
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
        fprintf(stderr,"Crash because CPU clock is 0 Hz\n");
        exit(1);
    }
}

static uint32_t
mosccr_read(void *clientData, uint32_t address, int rqlen)
{
    RxClk *rc = clientData;
    return rc->regMOSCCR;
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
sosccr_read(void *clientData, uint32_t address, int rqlen)
{
    RxClk *rc = clientData;
    return rc->regSOSCCR;
}

static void
sosccr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
    RxClk *rc = clientData;
    if (value & 1) {
        // RTC interconnection is missing here
        Clock_SetFreq(rc->clkSubOsc, 0);
    } else {
        Clock_SetFreq(rc->clkSubOsc, rc->subOscFreq);
    }
    rc->regSOSCCR = value & 1;
}

static uint32_t
lococr_read(void *clientData, uint32_t address, int rqlen)
{
    RxClk *rc = clientData;
    return rc->regLOCOCR;
}

static void
lococr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
    RxClk *rc = clientData;
    if (value & 1) {
        Clock_SetFreq(rc->clkLoco, 0);
    } else {
        Clock_SetFreq(rc->clkLoco, rc->locoOscFreq);
    }
    rc->regLOCOCR = value & 1;
}

static uint32_t
ilococr_read(void *clientData, uint32_t address, int rqlen)
{
    RxClk *rc = clientData;
    fprintf(stderr, "ILOCOCR read\n");
    return rc->regILOCOCR;
}

static void
ilococr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
    RxClk *rc = clientData;
    if (value & 1) {
        if (rc->regILOCOCR & 1) {
            fprintf(stderr, "Internal watchtog clock cannot be stopped\n");
        } else {
            Clock_SetFreq(rc->clkIwdt, 0);
        }
    } else {
        Clock_SetFreq(rc->clkIwdt, rc->iwdtOscFreq);
    }
    fprintf(stderr, "ILOCOCR %u\n", value);
    rc->regILOCOCR = value & 1;
}

static uint32_t
hococr_read(void *clientData, uint32_t address, int rqlen)
{
    RxClk *rc = clientData;
    return rc->regHOCOCR;
}

static void
hococr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
    RxClk *rc = clientData;
    if (value & 1) {
        Clock_SetFreq(rc->clkHoco, 0);
    } else {
        Clock_SetFreq(rc->clkHoco, rc->hocoOscFreq);
    }
    rc->regHOCOCR = value & 1;
}

static uint32_t
oscovfsr_read(void *clientData, uint32_t address, int rqlen)
{
    /* Currently the delay is not implemented */
    RxClk *rc = clientData;
    uint8_t value = 0;
    if (Clock_Freq(rc->clkMainOsc) > 0) {
        value |= OSCOVFSR_MOOVF;
    } 
    if (Clock_Freq(rc->clkPllOut) > 0) {
        value |= OSCOVFSR_PLOVF;
    }
    if (Clock_Freq(rc->clkHoco) > 0) {
        value |= OSCOVFSR_HCOVF;
    }
    return value; 
}

static void
oscovfsr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
    fprintf(stderr,"%s: Register should not be written\n", __func__); 
}

/**
 **************************************************************************
 * Oscillation Stop Detection Control Register 
 **************************************************************************
 */
static uint32_t
ostdcr_read(void *clientData, uint32_t address, int rqlen)
{
    fprintf(stderr,"%s: not implemented \n", __func__); 
    return 0; 
}

static void
ostdcr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
    fprintf(stderr,"%s: not implemented \n", __func__); 
}

/**
 ***************************************************************************************
 * \fn static uint32_t ostdsr_read(void *clientData, uint32_t address, int rqlen);
 * Oscillation Stop Detection Status Register
 ***************************************************************************************
 */
static uint32_t
ostdsr_read(void *clientData, uint32_t address, int rqlen)
{
    fprintf(stderr,"%s: not implemented \n", __func__); 
    return 0; 
}

static void
ostdsr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
    fprintf(stderr,"%s: not implemented \n", __func__); 
}

/**
 *****************************************************************
 * Main Clock Oscillator Wait Control Register
 *****************************************************************
 */
static uint32_t
moscwtcr_read(void *clientData, uint32_t address, int rqlen)
{
    fprintf(stderr,"%s: not implemented \n", __func__); 
    return 0; 
}

static void
moscwtcr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
    fprintf(stderr,"%s: not implemented \n", __func__); 
}

/**
 *********************************************************************
 * High Speed On Chip Oscillator Wait Control Register
 *********************************************************************
 */
static uint32_t
hocowtcr_read(void *clientData, uint32_t address, int rqlen)
{
    fprintf(stderr,"%s: not implemented \n", __func__); 
    return 0; 
}

static void
hocowtcr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
    fprintf(stderr,"%s: not implemented \n", __func__); 
}

/*
 **********************************************************************
 * \fn static uint32_t ckocr_read(void *clientData, uint32_t address, int rqlen)
 *
 **********************************************************************
 */

static uint32_t
ckocr_read(void *clientData, uint32_t address, int rqlen)
{
    RxClk *rc = clientData; 
    return rc->regCKOCR;  
}

static void
ckocr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
    RxClk *rc = clientData; 
    Clock_t *clk;
    uint32_t div;
    uint16_t ckosel; 
    uint16_t ckodiv; 
    rc->regCKOCR = value & 0xf700;
    if (value & CKOCR_CKOSTP) {
        Clock_Decouple(rc->clkCLKOUT);
        Clock_SetFreq(rc->clkCLKOUT, 0);
        return;
    }
    ckosel = (value >> 8) & 7;
    ckodiv = (value >> 12) & 7;
    switch(ckodiv) {
       case 0:
            div = 1; 
            break;
        case 1:
            div = 2;
            break;
        case 2:
            div = 4;
            break;
        case 3:
            div = 8;
            break;
        case 4:
            div= 16;
            break;
        default: 
            fprintf(stderr,"Illegal clock divider for clk out\n");
            div = 0; 
            break;
    } 
    switch(ckosel) {
        case 0:
            clk = rc->clkLoco;
            break;
        case 1:
            clk = rc->clkHoco;
            break;
        case 2:
            clk = rc->clkMainOsc;
            break;
        case 3:
            clk = rc->clkSubOsc;
            break;
        default:
            clk = NULL;
    } 
    if (clk && div) {
        Clock_MakeDerived(rc->clkCLKOUT, clk, 1, div);
    } else {
        Clock_Decouple(rc->clkCLKOUT);
    }
}

static uint32_t
mofcr_read(void *clientData, uint32_t address, int rqlen)
{
    fprintf(stderr,"%s: not implemented \n", __func__); 
    return 0; 
}

static void
mofcr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
    fprintf(stderr,"%s: not implemented \n", __func__); 
}

static void
RxClk_Unmap(void *owner, uint32_t base, uint32_t mask)
{
    IOH_Delete32(REG_SCKCR(base));
    IOH_Delete16(REG_SCKCR3(base));
    IOH_Delete16(REG_PLLCR(base));
    IOH_Delete8(REG_PLLCR2(base));
    IOH_Delete8(REG_MOSCCR(base));
    IOH_Delete8(REG_SOSCCR(base));
    IOH_Delete8(REG_LOCOCR(base));
    IOH_Delete8(REG_ILOCOCR(base));
    IOH_Delete8(REG_HOCOCR(base));
    IOH_Delete8(REG_OSCOVFSR(base));
    IOH_Delete8(REG_OSTDCR(base));
    IOH_Delete8(REG_OSTDSR(base));
    IOH_Delete8(REG_MOSCWTCR(base));
    IOH_Delete8(REG_HOCOWTCR(base));
    IOH_Delete8(REG_CKOCR(base));
    IOH_Delete8(REG_MOFCR(base));
}

static void
RxClk_Map(void *owner, uint32_t base, uint32_t mask, uint32_t mapflags)
{
    RxClk *clk = owner;
    IOH_New32(REG_SCKCR(base), sckcr_read, sckcr_write, clk);
    IOH_New16(REG_SCKCR3(base), sckcr3_read, sckcr3_write, clk);
    IOH_New16(REG_PLLCR(base), pllcr_read, pllcr_write, clk);
    IOH_New8(REG_PLLCR2(base), pllcr2_read, pllcr2_write, clk);
    IOH_New8(REG_MOSCCR(base), mosccr_read, mosccr_write, clk);
    IOH_New8(REG_SOSCCR(base), sosccr_read, sosccr_write, clk);
    IOH_New8(REG_LOCOCR(base), lococr_read, lococr_write, clk);
    IOH_New8(REG_ILOCOCR(base), ilococr_read, ilococr_write, clk);
    IOH_New8(REG_HOCOCR(base), hococr_read, hococr_write, clk);
    IOH_New8(REG_OSCOVFSR(base), oscovfsr_read, oscovfsr_write, clk);
    IOH_New8(REG_OSTDCR(base), ostdcr_read, ostdcr_write, clk);
    IOH_New8(REG_OSTDSR(base), ostdsr_read, ostdsr_write, clk);
    IOH_New8(REG_MOSCWTCR(base), moscwtcr_read, moscwtcr_write, clk);
    IOH_New8(REG_HOCOWTCR(base), hocowtcr_read, hocowtcr_write, clk);
    IOH_New8(REG_CKOCR(base), ckocr_read, ckocr_write, clk);
    IOH_New8(REG_MOFCR(base), mofcr_read, mofcr_write, clk);
}

BusDevice *
Rx111Clk_New(const char *name)
{
    RxClk *rc = sg_new(RxClk);

    rc->bdev.first_mapping = NULL;
    rc->bdev.Map = RxClk_Map;
    rc->bdev.UnMap = RxClk_Unmap;
    rc->bdev.owner = rc;
    rc->bdev.hw_flags = MEM_FLAG_WRITABLE | MEM_FLAG_READABLE;

    //rc->isInit = true;
    rc->mainOscFreq = 0;
    rc->subOscFreq = 32768;
    rc->locoOscFreq = 4*1000 * 1000;
    rc->iwdtOscFreq = 15000;
    rc->hocoOscFreq = 32*1000*1000;
    Config_ReadUInt32(&rc->mainOscFreq, "global", "crystal");
    Config_ReadUInt32(&rc->subOscFreq, "global", "subclk");
    if ((rc->mainOscFreq  < 1000000) || (rc->mainOscFreq > 20000000)) {
        if (rc->mainOscFreq != 0) { /* No oscillator */
            fprintf(stderr, "Warning: Main Oscillator freq %u is out of range\n", rc->mainOscFreq);
        }
    }
    rc->clkMainOsc = Clock_New("%s.extal", name);;
    rc->clkSubOsc = Clock_New("%s.subosc", name);
    rc->clkHoco = Clock_New("%s.hoco", name);
    rc->clkLoco = Clock_New("%s.loco", name);
    rc->clkIwdt = Clock_New("%s.iwdt", name);
    rc->clkFDivIn = Clock_New("%s.fdivin", name);
    rc->clkPllOut = Clock_New("%s.pllout", name);
    rc->clkDiv1 = Clock_New("%s.div1", name);
    rc->clkDiv2 = Clock_New("%s.div2", name);
    rc->clkDiv4 = Clock_New("%s.div4", name);
    rc->clkDiv8 = Clock_New("%s.div8", name);
    rc->clkDiv16 = Clock_New("%s.div16", name);
    rc->clkDiv32 = Clock_New("%s.div32", name);
    rc->clkDiv64 = Clock_New("%s.div64", name);

    rc->clkFCLK = Clock_New("%s.fclk", name);
    rc->clkICLK = Clock_New("%s.iclk", name);
    rc->clkPCKB = Clock_New("%s.pckb", name);
    rc->clkPCKD = Clock_New("%s.pckd", name);
    rc->clkCLKOUT = Clock_New("%s.clkout", name);

    Clock_MakeDerived(rc->clkDiv1, rc->clkFDivIn, 1, 1);
    Clock_MakeDerived(rc->clkDiv2, rc->clkFDivIn, 1, 2);
    Clock_MakeDerived(rc->clkDiv4, rc->clkFDivIn, 1, 4);
    Clock_MakeDerived(rc->clkDiv8, rc->clkFDivIn, 1, 8);
    Clock_MakeDerived(rc->clkDiv16, rc->clkFDivIn, 1, 16);
    Clock_MakeDerived(rc->clkDiv32, rc->clkFDivIn, 1, 32);
    Clock_MakeDerived(rc->clkDiv64, rc->clkFDivIn, 1, 64);


    rc->regPLLCR2 = PLLCR2_PLLEN;
    rc->regMOSCCR = 1;
    rc->regSOSCCR = 0;
    rc->regILOCOCR = 1;
    hococr_write(rc, 1, REG_HOCOCR(0), 1);
    lococr_write(rc, 0, 0, 1);
    pllcr_write(rc, 0x0f00, 0, 2);
    sckcr3_write(rc, 0, 0, 2);
    pllcr2_write(rc, PLLCR2_PLLEN, 0, 2);
    sckcr_write(rc, 0x33000303, 0, 4);
    ilococr_write(rc, 1, 0, 1);
    /* TODO: HOCOCR reset value should depend on FSR1 in Flash */
    return &rc->bdev;
}
