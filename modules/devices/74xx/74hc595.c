/*
 ************************************************************************************************
 * Emulation of 74HC595 8 bit shift register with parallel output 
 *
 * state: Not tested
 *
 * Copyright 2013 Jochen Karrer. All rights reserved.
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
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include "signode.h"
#include "sgstring.h"
#include "74hc595.h"

#if 0
#define dbgprintf(...) fprintf(stderr,__VA_ARGS__)
#else
#define dbgprintf(...)
#endif

typedef struct HC595 {
	SigNode *sigQ7S;	/* Output of the last shift register stage */
	SigNode *sigQ[8];	/* Parallel data out */
	SigNode *sig_nMR;	/* Master Reset */
	SigNode *sigSHCP;	/* Shift register clock input */
	SigNode *sigSTCP;	/* Storage register clock input */
	SigNode *sig_nOE;	/* Output enable */
	SigNode *sigDS;		/* Serial data input */
	SigTrace *traceSHCP;
	uint8_t regShift;
	uint8_t regStorage;
} HC595;

/*
 **********************************************************************
 * \fn static void SHCP_Trace(SigNode * sig, int value, void *eventData)
 * Shift register clock input event handler.
 **********************************************************************
 */
static void
SHCP_Trace(SigNode * sig, int value, void *eventData)
{
	HC595 *hc595 = eventData;
	if(value == SIG_HIGH) {
		bool carry = !!(hc595->regShift & 0x80);
		hc595->regShift <<= 1;	
		if(SigNode_Val(hc595->sigDS) == SIG_HIGH) {
			hc595->regShift |= 1;
		}
		if((carry)) {
			SigNode_Set(hc595->sigQ7S,SIG_HIGH);
		} else {
			SigNode_Set(hc595->sigQ7S,SIG_LOW);
		}
		dbgprintf("Shift reg is 0x%02x\n",hc595->regShift);
	}
}

static void
update_dataouts(HC595 *hc595) 
{
	int i;
	for(i = 0; i < 8; i++) {
		if(hc595->regStorage & (1 << i)) {
			SigNode_Set(hc595->sigQ[i],SIG_HIGH);
		} else {
			SigNode_Set(hc595->sigQ[i],SIG_LOW);
		}
	}
}
/**
 *********************************************************************
 * Storage register clock input. A positive edge transfers the
 * data from shift register to storage register.
 *********************************************************************
 */
static void
STCP_Trace(SigNode * sig, int value, void *eventData)
{
	HC595 *hc595 = eventData;
	dbgprintf("Change of Storage Clock STCP to %u\n",value);
	if(value == SIG_HIGH) {
		hc595->regStorage = hc595->regShift;	
		dbgprintf("Updated the storage register to 0x%02x\n",hc595->regStorage);
		if(SigNode_Val(hc595->sig_nOE) == SIG_LOW) {
			dbgprintf("Updating the Pins\n");
			update_dataouts(hc595);
		}
	}
}

/**
 **************************************************************************
 * \fn static void nOE_Trace(SigNode * sig, int value, void *eventData)
 * Output Enable: Set all outputs to OPEN when high, else activate them
 **************************************************************************
 */
static void
nOE_Trace(SigNode * sig, int value, void *eventData)
{
	HC595 *hc595 = eventData;
	int i;
	if(SigNode_Val(hc595->sig_nOE) == SIG_LOW) {
		update_dataouts(hc595);
	} else {
		for(i = 0; i < 8; i++) {
			SigNode_Set(hc595->sigQ[i],SIG_OPEN);
		}
	}
}

/**
 *****************************************************************
 * Reset of the shift register.
 *****************************************************************
 */
static void
nMR_Trace(SigNode * sig, int value, void *eventData)
{
	HC595 *hc595 = eventData;
	dbgprintf("MR trace goes to %u\n",value);
	if(value == SIG_LOW) {
		hc595->regShift = 0;
		/* Remove the shift clock */
		if(hc595->traceSHCP) {
			SigNode_Untrace(hc595->sigSHCP,hc595->traceSHCP);
			hc595->traceSHCP = NULL;
		}
	} else {
		if(!hc595->traceSHCP) {
			hc595->traceSHCP = SigNode_Trace(hc595->sigSHCP,SHCP_Trace,hc595);
		}
	}
}

void
HC595_New(const char *name)
{
	HC595 *hc595 = sg_new(HC595);
	int i;
	hc595->sigQ7S = SigNode_New("%s.Q7S",name);
	hc595->sig_nMR = SigNode_New("%s.nMR",name);
	hc595->sigSHCP = SigNode_New("%s.SHCP",name);
	hc595->sigSTCP = SigNode_New("%s.STCP",name);
	hc595->sig_nOE = SigNode_New("%s.nOE",name);
	hc595->sigDS = SigNode_New("%s.DS",name);
	if(!hc595->sigQ7S || !hc595->sig_nMR || !hc595->sigSHCP || !hc595->sigSTCP
	   || !hc595->sig_nOE || !hc595->sigDS) 
	{
		fprintf(stderr,"Can not create signal lines for HC595 \"%s\"\n",name);
		exit(1);
	}
	for(i = 0; i < 8; i++) {
		hc595->sigQ[i] = SigNode_New("%s.Q%u",name,i);
		if(!hc595->sigQ[i]) {
			fprintf(stderr,"Can not create Output Signal for HC595 \"%s\"\n",name);
			exit(1);
		}
	}
	SigNode_Trace(hc595->sigSTCP,STCP_Trace,hc595);
	SigNode_Trace(hc595->sig_nOE,nOE_Trace,hc595);
	SigNode_Trace(hc595->sig_nMR,nMR_Trace,hc595);
	SigNode_Set(hc595->sig_nMR,SIG_LOW);
	SigNode_Set(hc595->sig_nMR,SIG_OPEN);
	SigNode_Set(hc595->sig_nOE,SIG_HIGH);
	SigNode_Set(hc595->sig_nOE,SIG_OPEN);
}
