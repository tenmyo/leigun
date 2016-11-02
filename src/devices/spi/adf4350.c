/*
 *************************************************************************************************
 *
 * Emulation of Analog Devices ADF4350 Synthesizer 
 *
 * State: Not working 
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

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <inttypes.h>
#include "sglib.h"
#include "sgstring.h"
#include "signode.h"
#include "adf4350.h"
#include "cycletimer.h"
#include "debugvars.h"


typedef struct ADF4350 {
	SigNode *sigClk;
	SigNode *sigData;
	SigNode *sigLoadEnable;
	SigNode *sigLockDetect;
	uint32_t shiftReg;
} ADF4350;

static void
reg0_write(ADF4350 *adf, uint32_t val) 
{
	fprintf(stderr,"%s: %08x\n", __func__, val);
}
static void
reg1_write(ADF4350 *adf, uint32_t val) 
{
	fprintf(stderr,"%s: %08x\n", __func__, val);
}
static void
reg2_write(ADF4350 *adf, uint32_t val) 
{
	fprintf(stderr,"%s: %08x\n", __func__, val);
}
static void
reg3_write(ADF4350 *adf, uint32_t val) 
{
	fprintf(stderr,"%s: %08x\n", __func__, val);
}
static void
reg4_write(ADF4350 *adf, uint32_t val) 
{
	fprintf(stderr,"%s: %08x\n", __func__, val);
}
static void
reg5_write(ADF4350 *adf, uint32_t val) 
{
	fprintf(stderr,"%s: %08x\n", __func__, val);
}
static void
reg6_write(ADF4350 *adf, uint32_t val) 
{
	fprintf(stderr,"%s: %08x\n", __func__, val);
}
static void
reg7_write(ADF4350 *adf, uint32_t val) 
{
	fprintf(stderr,"%s: %08x\n", __func__, val);
}

static void
ADF4350_Load(ADF4350 *adf, uint32_t loadVal) 
{
	uint32_t regNum = loadVal & 7; /* The control Bits */
	switch (regNum) {
		case 0:
			reg0_write(adf, loadVal);
			break;
		case 1:
			reg1_write(adf, loadVal);
			break;
		case 2:
			reg2_write(adf, loadVal);
			break;
		case 3:
			reg3_write(adf, loadVal);
			break;
		case 4:
			reg4_write(adf, loadVal);
			break;
		case 5:
			reg5_write(adf, loadVal);
			break;
		case 6:
			reg6_write(adf, loadVal);
			break;
		case 7:
			reg7_write(adf, loadVal);
			break;
		default:
			break;
	}
}

/**
 **********************************************************************************************************
 * \fn static void SigClockChange(SigNode * node, int value, void *clientData)
 * Signal trace callback procedure called whenever the clock line changes.
 * The data bit is shifted in on positive clock edge and changes on the negative clock edge.
 **********************************************************************************************************
 */
static void
SigClockChange(SigNode * node, int value, void *clientData)
{
	ADF4350 *adf = clientData;
	if (value == SIG_LOW) {
		return;
	}
	if (SigNode_Val(adf->sigData) == SIG_HIGH) {
		adf->shiftReg = (adf->shiftReg << 1) | 1;
	} else {
		adf->shiftReg = (adf->shiftReg << 1);
	}
}

/**
 ************************************************************************************************************
 * \fn static void SigLoadEnableChange(SigNode * node, int value, void *clientData)
 * The shiftReg is loaded on the positive edge of the Load enable signal.
 * The clock must be low on Load enable;
 ************************************************************************************************************
 */
static void
SigLoadEnableChange(SigNode * node, int value, void *clientData)
{
	ADF4350 *adf = clientData;
	if (value == SIG_LOW) {
		return;
	}
	if (SigNode_Val(adf->sigClk) != SIG_LOW) {
		fprintf(stderr, "ADF4350 warning: clock not low on load enable\n");
	}
	ADF4350_Load(adf, adf->shiftReg);
}

void
ADF4350_New(const char *name)
{

        ADF4350 *adf = sg_new(ADF4350);
        int i;
	adf->sigClk = SigNode_New("%s.sclk", name);
	adf->sigData = SigNode_New("%s.sdi", name);
	adf->sigLoadEnable = SigNode_New("%s.load", name);
	adf->sigLockDetect = SigNode_New("%s.lockDetect", name);

        if (!adf->sigClk || !adf->sigData || !adf->sigLoadEnable || !adf->sigLockDetect) 
	{ 
                fprintf(stderr, "Can not create signal lines for ADF4350\n");
                exit(1);
        }
        SigNode_Trace(adf->sigClk, SigClockChange, adf);
        SigNode_Trace(adf->sigLoadEnable, SigLoadEnableChange, adf);
	fprintf(stderr,"Created ADF4350: \"%s\"\n", name);
}
