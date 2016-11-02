/*
 **********************************************************************************************
 * Renesas RX62N Clock generation module
 *
 * State: Untested 
 *
 * Copyright 2012 Jochen Karrer. All rights reserved.
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
 **********************************************************************************************
 */

#include <stdint.h>
#include <inttypes.h>
#include "bus.h"
#include "sglib.h"
#include "sgstring.h"
#include "clk_rx62n.h"
#include "clock.h"
#include "signode.h"
#include "configfile.h"

#define REG_SCKCR(base)		0x00080020
#define	REG_BCKCR(base)		0x00080030
#define	REG_OSTDCR(base)	0x00080040
#define	REG_SUBOSCCR(base)	0x0008C28A

typedef struct RxClk {
	BusDevice bdev;
	uint32_t subOscFreq;
	Clock_t *clkMainOsc;
	Clock_t *clkSubOsc;
	Clock_t *clkPllOut;
	Clock_t *clk8;
	Clock_t *clk4;
	Clock_t *clk2;
	Clock_t *clk1;
	/* The output clocks */
	Clock_t *clkICLK;
	Clock_t *clkPCLK;
	Clock_t *clkBCLK;
	Clock_t *clkBCLKPin;

	uint32_t regSCKCR;
	uint8_t regBCKCR;
	uint16_t regOSTDCR;
	uint8_t regSUBOSCCR;
} RxClk;

/**
 **********************************************************************************************
 * \fn static void sckcr_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
 **********************************************************************************************
 */
static void
sckcr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	RxClk *clk = clientData;
	uint32_t ick, bck, pck;
	clk->regSCKCR = value & 0x0fcf0f00;
	ick = (value >> 24) & 0xf;
	bck = (value >> 16) & 0xf;
	pck = (value >> 8) & 0xf;
	switch (ick) {
	    case 0:
		    Clock_MakeDerived(clk->clkICLK, clk->clk8, 1, 1);
		    break;
	    case 1:
		    Clock_MakeDerived(clk->clkICLK, clk->clk4, 1, 1);
		    break;
	    case 2:
		    Clock_MakeDerived(clk->clkICLK, clk->clk2, 1, 1);
		    break;
	    case 3:
		    Clock_MakeDerived(clk->clkICLK, clk->clk1, 1, 1);
		    break;
	    default:
		    fprintf(stderr, "Illegal ICLK selection in SCKCR\n");
		    Clock_MakeDerived(clk->clkICLK, clk->clk1, 0, 1);
		    break;
	}
	switch (bck) {
	    case 0:
		    Clock_MakeDerived(clk->clkBCLK, clk->clk8, 1, 1);
		    break;
	    case 1:
		    Clock_MakeDerived(clk->clkBCLK, clk->clk4, 1, 1);
		    break;
	    case 2:
		    Clock_MakeDerived(clk->clkBCLK, clk->clk2, 1, 1);
		    break;
	    case 3:
		    Clock_MakeDerived(clk->clkBCLK, clk->clk1, 1, 1);
		    break;
	    default:
		    fprintf(stderr, "Illegal BCLK selection in SCKCR\n");
		    Clock_MakeDerived(clk->clkBCLK, clk->clk1, 0, 1);
		    break;
	}
	switch (pck) {
	    case 0:
		    Clock_MakeDerived(clk->clkPCLK, clk->clk8, 1, 1);
		    break;
	    case 1:
		    Clock_MakeDerived(clk->clkPCLK, clk->clk4, 1, 1);
		    break;
	    case 2:
		    Clock_MakeDerived(clk->clkPCLK, clk->clk2, 1, 1);
		    break;
	    case 3:
		    Clock_MakeDerived(clk->clkPCLK, clk->clk1, 1, 1);
		    break;
	    default:
		    fprintf(stderr, "Illegal PCLK selection in SCKCR\n");
		    Clock_MakeDerived(clk->clkPCLK, clk->clk1, 0, 1);
		    break;
	}
}

static uint32_t
sckcr_read(void *clientData, uint32_t address, int rqlen)
{
	RxClk *clk = clientData;
	return clk->regSCKCR;
}

static void
bckcr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	RxClk *clk = clientData;
	if (value & 1) {
		Clock_MakeDerived(clk->clkBCLKPin, clk->clkBCLK, 1, 2);
	} else {
		Clock_MakeDerived(clk->clkBCLKPin, clk->clkBCLK, 1, 1);
	}
	clk->regBCKCR = value & 1;
}

static uint32_t
bckcr_read(void *clientData, uint32_t address, int rqlen)
{
	RxClk *clk = clientData;
	return clk->regBCKCR;
}

static void
ostdcr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	RxClk *clk = clientData;
	uint8_t key = (value >> 8) & 0xff;
	if (key != 0xAC) {
		fprintf(stderr, "%s with wrong KEY\n", __func__);
		return;
	}
	clk->regOSTDCR = value & 0x80;
}

static uint32_t
ostdcr_read(void *clientData, uint32_t address, int rqlen)
{
	RxClk *clk = clientData;
	return clk->regOSTDCR;
}

/**
 *************************************************************************************************
 * \fn static void subosccr_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
 * Start/Stop the Sub Oscillator
 *************************************************************************************************
 */
static void
subosccr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	RxClk *clk = clientData;
	clk->regSUBOSCCR = value & 1;
	if (value == 0) {
		Clock_SetFreq(clk->clkSubOsc, 0);
		fprintf(stderr, "SubClockOscillator stopped\n");
	} else {
		Clock_SetFreq(clk->clkSubOsc, clk->subOscFreq);
	}
}

static uint32_t
subosccr_read(void *clientData, uint32_t address, int rqlen)
{
	RxClk *clk = clientData;
	return clk->regSUBOSCCR;
}

static void
CLK_Unmap(void *owner, uint32_t base, uint32_t mask)
{
	IOH_Delete32(REG_SCKCR(base));
	IOH_Delete16(REG_BCKCR(base));
	IOH_Delete16(REG_OSTDCR(base));
	IOH_Delete16(REG_SUBOSCCR(base));
}

static void
CLK_Map(void *owner, uint32_t base, uint32_t mask, uint32_t mapflags)
{
	RxClk *clk = owner;
	IOH_New32(REG_SCKCR(base), sckcr_read, sckcr_write, clk);
	IOH_New16(REG_BCKCR(base), bckcr_read, bckcr_write, clk);
	IOH_New16(REG_OSTDCR(base), ostdcr_read, ostdcr_write, clk);
	IOH_New16(REG_SUBOSCCR(base), subosccr_read, subosccr_write, clk);
}

/**
 **************************************************************************************
 * \fn BusDevice *RX62NClk_New(const char *name)
 * Create a new RX62N clock module
 **************************************************************************************
 */
BusDevice *
Rx62nClk_New(const char *name)
{
	RxClk *clk = sg_new(RxClk);
	uint32_t oscFreq = 12500000;
	clk->subOscFreq = 32768;
	Config_ReadUInt32(&oscFreq, "global", "crystal");
	Config_ReadUInt32(&clk->subOscFreq, "global", "subclk");
	clk->bdev.first_mapping = NULL;
	clk->bdev.Map = CLK_Map;
	clk->bdev.UnMap = CLK_Unmap;
	clk->bdev.owner = clk;
	clk->bdev.hw_flags = MEM_FLAG_WRITABLE | MEM_FLAG_READABLE;
	clk->clkMainOsc = Clock_New("%s.extal", name);
	clk->clkSubOsc = Clock_New("%s.subosc", name);
	clk->clkPllOut = Clock_New("%s.pll", name);
	clk->clk8 = Clock_New("%s.clk8", name);
	clk->clk4 = Clock_New("%s.clk4", name);
	clk->clk2 = Clock_New("%s.clk2", name);
	clk->clk1 = Clock_New("%s.clk1", name);
	/* The output clocks */
	clk->clkICLK = Clock_New("%s.iclk", name);
	clk->clkPCLK = Clock_New("%s.pclk", name);
	clk->clkBCLK = Clock_New("%s.bclk", name);
	clk->clkBCLKPin = Clock_New("%s.bclkpin", name);
	clk->regSUBOSCCR = 1;
	Clock_SetFreq(clk->clkMainOsc, oscFreq);
	Clock_SetFreq(clk->clkSubOsc, clk->subOscFreq);
	Clock_MakeDerived(clk->clkPllOut, clk->clkMainOsc, 8, 1);
	Clock_MakeDerived(clk->clk8, clk->clkPllOut, 1, 1);
	Clock_MakeDerived(clk->clk4, clk->clkPllOut, 1, 2);
	Clock_MakeDerived(clk->clk2, clk->clkPllOut, 1, 4);
	Clock_MakeDerived(clk->clk1, clk->clkPllOut, 1, 8);
	Clock_MakeDerived(clk->clkBCLKPin, clk->clkBCLK, 1, 1);
	sckcr_write(clk, 0x02020200, REG_SCKCR(0), 4);
	return &clk->bdev;
}
