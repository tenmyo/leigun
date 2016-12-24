/*
 ************************************************************************************************
 * Emulation of 74HC138 8 bit shift register with parallel output 
 *
 * state: Totaly untested
 *
 * Copyright 2015 Jochen Karrer. All rights reserved.
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
#include "74hc138.h"

#if 0
#define dbgprintf(...) fprintf(stderr,__VA_ARGS__)
#else
#define dbgprintf(...)
#endif

typedef struct HC138 {
	SigNode *sigA;	
	SigNode *sigB;	
    SigNode *sigC;
    SigNode *signE1; /* Fairchild names them nG2A,nG2B and G1 */
    SigNode *signE2;
    SigNode *sigE3;
    SigNode *sigY[8];
    uint8_t decodeChan;
    uint8_t enableVal;
} HC138;

/*
 **********************************************************************
 * \fn static void SigABC_Trace(SigNode * sig, int value, void *eventData)
 **********************************************************************
 */
static void
SigABC_Trace(SigNode * sig, int value, void *eventData)
{
	HC138 *hc = eventData;
    uint32_t decodeChan = 0;
    if(SigNode_Val(hc->sigA) == SIG_HIGH) {
        decodeChan |= 1;
    }
    if(SigNode_Val(hc->sigB) == SIG_HIGH) {
        decodeChan |= 2;
    }
    if(SigNode_Val(hc->sigC) == SIG_HIGH) {
        decodeChan |= 4;
    }
    if (hc->enableVal == 4) {
        SigNode_Set(hc->sigY[hc->decodeChan], SIG_HIGH);
        SigNode_Set(hc->sigY[decodeChan], SIG_LOW);
    }
    hc->decodeChan = decodeChan;
}

/**
 *******************************************************
 * Trace the Enable lines of the decoder.
 *******************************************************
 */
static void
SigEx_Trace(SigNode * sig, int value, void *eventData)
{
	HC138 *hc = eventData;
    uint32_t enableVal = 0;
    if(SigNode_Val(hc->signE1) == SIG_HIGH) {
        enableVal |= 1;
    }
    if(SigNode_Val(hc->signE2) == SIG_HIGH) {
        enableVal |= 2;
    }
    if(SigNode_Val(hc->sigE3) == SIG_HIGH) {
        enableVal |= 4;
    }
    if (hc->enableVal == 4) {
        SigNode_Set(hc->sigY[hc->decodeChan], SIG_LOW);
    } else {
        SigNode_Set(hc->sigY[hc->decodeChan], SIG_HIGH);
    }
    hc->enableVal = enableVal;
}


void
HC138_New(const char *name)
{
	HC138 *hc138 = sg_new(HC138);
	int i;
    hc138->sigA = SigNode_New("%s.A", name);
    hc138->sigB = SigNode_New("%s.B", name);
    hc138->sigC = SigNode_New("%s.C", name);
    hc138->signE1 = SigNode_New("%s.nE1", name);
    hc138->signE2 = SigNode_New("%s.nE2", name);
    hc138->sigE3 = SigNode_New("%s.E3", name);

	if(!hc138->sigA || !hc138->sigB || !hc138->sigC || !hc138->signE1
	   || !hc138->signE2 || !hc138->sigE3) 
	{
		fprintf(stderr,"Can not create signal lines for HC138 \"%s\"\n",name);
		exit(1);
	}
	for(i = 0; i < 8; i++) {
		hc138->sigY[i] = SigNode_New("%s.Y%u",name,i);
		if(!hc138->sigY[i]) {
			fprintf(stderr,"Can not create Output Signal for HC138 \"%s\"\n",name);
			exit(1);
		}
	}
	SigNode_Trace(hc138->sigA,SigABC_Trace,hc138);
	SigNode_Trace(hc138->sigB,SigABC_Trace,hc138);
	SigNode_Trace(hc138->sigC,SigABC_Trace,hc138);
	SigNode_Trace(hc138->signE1,SigEx_Trace,hc138);
	SigNode_Trace(hc138->signE2,SigEx_Trace,hc138);
	SigNode_Trace(hc138->sigE3, SigEx_Trace,hc138);
}
