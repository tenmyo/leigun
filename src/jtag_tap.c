/*
 ***********************************************************************************************
 *
 * Emulation of a JTAG Test access port state machine
 *
 * Copyright 2009 2012 Jochen Karrer. All rights reserved.
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
#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/fcntl.h>
#include "fio.h"
#include "signode.h"
#include "configfile.h"
#include "cycletimer.h"
#include "clock.h"
#include "sgstring.h"
#include "jtag_tap.h"
#include "compiler_extensions.h"
#include "sglib.h"

#if 0
#define dbgprintf(x...) { fprintf(stderr,x); }
#else
#define dbgprintf(x...)
#endif

typedef enum TapState {
        TapExit2DR = 0x0,
        TapExit1DR = 0x1,
        TapShiftDR = 0x2,
        TapPauseDR = 0x3,
        TapSelectIRScan = 0x4,
        TapUpdateDR = 0x5,
        TapCaptureDR = 0x6,
        TapSelectDRScan = 0x7,
        TapExit2IR = 0x8,
        TapExit1IR = 0x9,
        TapShiftIR = 0xa,
        TapPauseIR = 0xb,
        TapRunTestIdle = 0xc,
        TapUpdateIR = 0xd,
        TapCaptureIR = 0xe,
        TapTestLogicReset = 0xf
} TapState;

__UNUSED__ static char *statenames[0x10] = {
       "TapExit2DR",
       "TapExit1DR",
        "TapShiftDR",
        "TapPauseDR",
        "TapSelectIRScan",
        "TapUpdateDR",
        "TapCaptureDR",
        "TapSelectDRScan",
        "TapExit2IR",
        "TapExit1IR",
        "TapShiftIR",
        "TapPauseIR",
        "TapRunTestIdle",
        "TapUpdateIR",
        "TapCaptureIR",
        "TapTestLogicReset",
};
typedef struct JTAG_Tap {
	char *name;
	JTAG_Operations *jops;
	void *owner;
	int shiftlen;
	int shiftbufsize;
	uint8_t *shift_data;
	TapState tapState;
	int next_tdi;
	SigNode *tdi;
	SigNode *tdo;
	SigNode *tms;
	SigNode *tck;
	SigNode *nTrst;
	uint8_t bitorder;
} JTAG_Tap;

/**
 *******************************************************************************
 * \fnstatic void store_shiftdata(JTAG_Tap *tap,uint8_t *data,int shiftlen) 
 *******************************************************************************
 */
static void
store_shiftdata(JTAG_Tap *tap,uint8_t *data,int shiftlen) 
{
	if(!data && shiftlen) {
		fprintf(stderr,"Bug: Shiftlen %d, but no databuffer\n",shiftlen);
		exit(1);
	}
	tap->shift_data = data;
	tap->shiftlen = shiftlen;
	return;
}

/**
 **********************************************************************************
 * \fn static void write_tdo(JTAG_Tap *tap) 
 * Set the TDO line to the value found in the last bit of the shift register. 
 **********************************************************************************
 */
static void
write_tdo(JTAG_Tap *tap) 
{
	int bit;
	int by;
	int tdo;
	if(!tap->shift_data || !tap->shiftlen) {
		return;
	}
	if(tap->bitorder == JTAG_TAP_ORDER_MSBFIRST)
	{
		bit = tap->shiftlen - 1;
		by = 0;
		bit = 7 - (bit & 7);	
	} else {
		bit = tap->shiftlen - 1;
		by = bit >> 3;	
		bit = bit & 7;
	}
	tdo = (tap->shift_data[by] >> bit) & 1;
	if(tdo) {
		SigNode_Set(tap->tdo,SIG_HIGH);
	} else {
		SigNode_Set(tap->tdo,SIG_LOW);
	}
}

/**
 ***********************************************************************
 * \fn static void shiftin_tdi(JTAG_Tap *tap);
 * Shift the shfitregister by one to the right and set the first bit 
 * in the shift register to the value of  the TDI line.
 * Slow because it shifts always everything.
 ***********************************************************************
 */
static void
shiftin_tdi(JTAG_Tap *tap) 
{
	int tdi; 
	int bytes = (tap->shiftlen + 7) >> 3;
	int i;
	uint8_t carry;
	if(!tap->shift_data || !tap->shiftlen) {
		return;
	}
	tdi = SigNode_Val(tap->tdi);
	dbgprintf("Shiftin %d\n",tdi);
	if(tap->bitorder == JTAG_TAP_ORDER_MSBFIRST)
	{
		if(tdi == SIG_HIGH) {
			carry = 0x80;
		} else {
			carry = 0;
		}
		for(i = bytes - 1; i >= 0; i--) {
			uint8_t nextcarry;
			nextcarry = (tap->shift_data[i] & 1) << 7;
			tap->shift_data[i] = (tap->shift_data[i] >> 1) | carry; 
			carry = nextcarry;
		}	
	} else {
		if(tdi == SIG_HIGH) {
			carry = 0x1;
		} else {
			carry = 0;
		}
		for(i = 0; i < bytes;i++) {
			uint8_t nextcarry;
			nextcarry = (tap->shift_data[i] & 0x80) >> 7;
			tap->shift_data[i] = (tap->shift_data[i] << 1) | carry; 
			carry = nextcarry;
		}	
	}
}

static void 
traceNTrst(SigNode *sig,int value,void *clientData)
{
	return;
}

static void 
traceTck(SigNode *sig,int value,void *clientData)
{
	JTAG_Tap *tap = (JTAG_Tap *) clientData;
	int tms;
	uint8_t *dataP;
	int datalen;
	if(value == SIG_LOW) {
		/* 
		 *************************************************
		 * Sampling is done on the rising edge 
		 * TDO changes on falling edge
		 *************************************************
		 */
		switch(tap->tapState) {
			case TapShiftIR:
			case TapShiftDR:
				write_tdo(tap);
				break;
			default:
				break;
		}
		return; 
	}
	tms = SigNode_Val(tap->tms);
	dbgprintf("PCLK with tms %d, tdi %d\n",tms,SigNode_Val(tap->tdi));
	if(tms == SIG_LOW) {
		switch(tap->tapState) {
			case TapExit2DR:
				tap->tapState = TapShiftDR;
				/* write TDO */
				break;

			case TapExit1DR:
				tap->tapState = TapPauseDR;
				break;
				
			case TapShiftDR:
				/* Write TDO */
				/* Get TDI and shift in */
				shiftin_tdi(tap);
				break;

			case TapPauseDR:
				break; 

			case TapSelectIRScan:
				tap->tapState = TapCaptureIR;
				datalen = 0; /* For the not handled case */
				tap->jops->captureIR(tap->owner,&dataP,&datalen);
				store_shiftdata(tap,dataP,datalen);
				break;

			case TapUpdateDR:
				tap->tapState = TapRunTestIdle;
				break;

			case TapCaptureDR:
				tap->tapState = TapShiftDR;	
				break;

			case TapSelectDRScan:
				tap->tapState = TapCaptureDR;	
				datalen = 0; /* For the not handled case */
				tap->jops->captureDR(tap->owner,&dataP,&datalen);
				store_shiftdata(tap,dataP,datalen);
				break;

			case TapExit2IR:
				tap->tapState = TapShiftIR;
				break;

			case TapExit1IR:
				tap->tapState = TapPauseIR;
				break;

			case TapShiftIR:
				shiftin_tdi(tap);
				break;

			case TapPauseIR:
				break;

			case TapRunTestIdle:
				break;

			case TapUpdateIR:
				tap->tapState = TapRunTestIdle;	
				break;

			case TapCaptureIR:
				tap->tapState = TapShiftIR;	
				break;

			case TapTestLogicReset:
				tap->tapState = TapRunTestIdle;
				break;
		}
	} else if(tms == SIG_HIGH) {
		switch(tap->tapState) {
			case TapExit2DR:
				tap->tapState = TapUpdateDR;
				tap->jops->updateDR(tap->owner);
				break;

			case TapExit1DR:
				tap->tapState = TapUpdateDR;
				tap->jops->updateDR(tap->owner);
				break;	

			case TapShiftDR:
				tap->tapState = TapExit1DR;
				shiftin_tdi(tap);
				break;

			case TapPauseDR:
				tap->tapState = TapExit2DR;
				break;

			case TapSelectIRScan:
				tap->tapState = TapTestLogicReset;
				break;

			case TapUpdateDR:
				tap->tapState = TapSelectDRScan;
				break;

			case TapCaptureDR:
				tap->tapState = TapExit1DR;	
				break;

			case TapSelectDRScan:
				tap->tapState = TapSelectIRScan;
				break;

			case TapExit2IR:
				tap->tapState = TapUpdateIR;
				tap->jops->updateIR(tap->owner);
				break;

			case TapExit1IR:
				tap->tapState = TapUpdateIR;
				tap->jops->updateIR(tap->owner);
				break;
				
			case TapShiftIR:
				tap->tapState = TapExit1IR;
				shiftin_tdi(tap);
				break;

			case TapPauseIR:
				tap->tapState = TapExit2IR;
				break;

			case TapRunTestIdle:
				tap->tapState = TapSelectDRScan;
				break;

			case TapUpdateIR:
				tap->tapState = TapSelectDRScan;
				break;

			case TapCaptureIR:
				tap->tapState = TapExit1IR;
				break;

			case TapTestLogicReset:
				break;
		}
	}
#if 1
	{ 
		static int oldstate;
		if(tap->tapState != oldstate) {
			dbgprintf("%d->%s\n",tms,statenames[tap->tapState]);
			oldstate = tap->tapState;
		}
	}
#endif
	return;
}
void
JTagTap_New(const char *name,JTAG_Operations *jops,void *owner) 
{
	JTAG_Tap *tap;	
	tap = sg_new(JTAG_Tap);
	tap->name = sg_strdup(name);
	tap->jops = jops;
	tap->owner = owner;
	tap->tdi = SigNode_New("%s.tdi",name);
	tap->tdo = SigNode_New("%s.tdo",name);
	tap->tms = SigNode_New("%s.tms",name);
	tap->tck = SigNode_New("%s.tck",name);
	tap->nTrst = SigNode_New("%s.nTrst",name);
	tap->tapState = TapTestLogicReset;
	tap->bitorder = jops->bitorder;
	SigNode_Trace(tap->tck,traceTck,tap);
	SigNode_Trace(tap->nTrst,traceNTrst,tap);
}
