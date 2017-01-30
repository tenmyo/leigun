/*
 *************************************************************************************************
 *
 * Emulation of Analog Devices AD5372 DAC 
 *
 * State: Working, currently AD5373 variant is missing 
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
#include "ad537x.h"
#include "cycletimer.h"
#include "debugvars.h"

typedef struct AD537x {
	uint32_t shiftIn;
	unsigned int shiftInCnt;
	uint32_t shiftOut;
	bool shiftOutEnabled;
	unsigned int shiftOutCnt;
	CycleCounter_t syncHighTimeStamp;
	SigNode *sigSync;
	SigNode *sigSdi;
	SigNode *sigSclk;
	SigTrace *traceSclk;
	SigNode *sigSdo;
	SigNode *sigBusy;
	SigNode *sigReset;
	SigNode *sigClr;
	SigNode *sigLdac;
	int8_t	 *channelList[64];
	uint16_t regX1A[32];
	uint16_t regX1B[32];
	uint16_t regC[32];
	uint16_t regM[32];
	uint16_t regDACCode[32];	/* not readable/writable by user */
	uint32_t regControl;
	uint32_t regOFS0;
	uint32_t regOFS1;
	uint32_t regABSelect[4];
	double anaVRef;
	double anaVSigGnd;
	double anaVOut[32];
} AD537x;

/**
 *************************************************************************************
 * The channel list defines the channel grouping for commands which configure 
 * multiple channels at the same time.
 *************************************************************************************
 */
static int8_t channelLists[64][33] = {
	{
	 /* Address 0, All groups, all channels */
	 0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,-1
	},
	{0,1,2,3,4,5,6,7,-1},			/* 0x01 */
	{8,9,10,11,12,13,14,15,-1},		/* 0x02 */ 
	{16,17,18,19,20,21,22,23,-1},		/* 0x03 */
	{24,25,26,27,28,29,30,31,-1},		/* 0x04 */
	{-1},					/* 0x05 */
	{-1},					/* 0x06 */
	{-1},					/* 0x07 */
	{0,-1},					/* 0x08 */
	{1,-1},					/* 0x09 */
	{2,-1},					/* 0x0a */
	{3,-1},					/* 0x0b */
	{4,-1},					/* 0x0c */
	{5,-1},					/* 0x0d */
	{6,-1},					/* 0x0e */
	{7,-1},					/* 0x0f */
	{8,-1},					/* 0x10 */
	{9,-1},					/* 0x11 */
	{10,-1},				/* 0x12 */
	{11,-1},				/* 0x13 */
	{12,-1},				/* 0x14 */
	{13,-1},				/* 0x15 */
	{14,-1},				/* 0x16 */
	{15,-1},				/* 0x17 */
	{16,-1},				/* 0x18 */
	{17,-1},				/* 0x19 */
	{18,-1},				/* 0x1A */
	{19,-1},				/* 0x1B */
	{20,-1},				/* 0x1C */
	{21,-1},				/* 0x1D */
	{22,-1},				/* 0x1E */
	{23,-1},				/* 0x1F */
	{24,-1},				/* 0x20 */
	{25,-1},				/* 0x21 */
	{26,-1},				/* 0x22 */
	{27,-1},				/* 0x23 */
	{28,-1},				/* 0x24 */
	{29,-1},				/* 0x25 */
	{30,-1},				/* 0x26 */
	{31,-1},				/* 0x27 */
	{-1},					/* 0x28 */
	{-1},					/* 0x29 */
	{-1},					/* 0x2A */
	{-1},					/* 0x2B */
	{-1},					/* 0x2C */
	{-1},					/* 0x2D */
	{-1},					/* 0x2E */
	{-1},					/* 0x2F */
	{0,8,16,24,-1},				/* 0x30 */
	{1,9,17,25,-1},				/* 0x31 */
	{2,10,18,26,-1},			/* 0x32 */
	{3,11,19,27,-1},			/* 0x33 */
	{4,12,20,28,-1},			/* 0x34 */
	{5,13,21,29,-1},			/* 0x35 */
	{6,14,22,30,-1},			/* 0x36 */
	{7,15,23,31,-1},			/* 0x37 */
	{8,16,24,-1},				/* 0x38 */
	{9,17,25,-1},				/* 0x39 */
	{10,18,26,-1},				/* 0x3A */
	{11,19,27,-1},				/* 0x3B */
	{12,20,28,-1},				/* 0x3C */
	{13,21,29,-1},				/* 0x3D */
	{14,22,30,-1},				/* 0x3E */
	{15,23,31,-1},				/* 0x3F */
}; 


static inline void
update_AnalogOut(AD537x *ad,unsigned int channelNr)
{
	int32_t offsCode;		
	int32_t dacCode;		
	double anaVOut;
	
	if(channelNr < 8) {
		offsCode = ad->regOFS0;
	} else {
		offsCode = ad->regOFS1;
	}
	dacCode = ad->regDACCode[channelNr];
	anaVOut = (4 * ad->anaVRef * (dacCode - (offsCode * 4))) / 65536 + ad->anaVSigGnd;
	ad->anaVOut[channelNr] = anaVOut;
#if 0
	fsync(stdout);
	fprintf(stdout,"\nVOut %u is %f Volt\n",channelNr,anaVOut);
	fprintf(stdout,"dacCode %u, offsCode %u\n",dacCode,offsCode);
	fsync(stdout);
#endif
}

static void
updateDAC(AD537x *ad,unsigned int channelNr) 
{
	bool abselect;
	int32_t dacCode;
	int32_t inputCode; 
	uint16_t M,C;
	if(channelNr > 31) {
		fprintf(stderr,"Illegal channel number in %s\n",__func__);
		exit(1);
	}
	if(SigNode_Val(ad->sigLdac) == SIG_HIGH) {
		return;
	}
	abselect = (ad->regABSelect[channelNr >> 3] >> (channelNr & 7)) & 1;
	if(abselect) {
		inputCode = ad->regX1B[channelNr];
	} else {
		inputCode = ad->regX1A[channelNr];
	}
	/* Read them into int32_t for simpler calculation */
	M = ad->regM[channelNr];
	C = ad->regC[channelNr];
	dacCode = (int64_t)inputCode * ((uint32_t)M + 1) / 65536 + C - 32768;
	if((dacCode > 65535) || (dacCode < 0)) {
		fprintf(stderr,"DAC code for channel %u out of range: %d, inputCode %u, M %u, C %u\n",channelNr,dacCode,inputCode,M,C);
	}
	ad->regDACCode[channelNr] = dacCode;
	update_AnalogOut(ad,channelNr);	
}

static void
updateDAC_All(AD537x *ad) {
	int ch;
	for(ch = 0; ch < 32; ch++) {
		updateDAC(ad,ch);
	}
}

static void
updateDAC_Group(AD537x *ad,unsigned int groupNr) 
{
	int i;
	for(i = 0; i < 8; i++) {
		updateDAC(ad, i + (groupNr << 3));
	}
}

static void
AD537x_Readback(AD537x * ad, uint16_t f)
{
	uint32_t value = 0;
	uint32_t channel;
	channel = ((f >> 7) & 0x3f) - 8;
	if (channel > 31) {
		channel = 0;
	}
	switch ((f >> 13) & 7) {
	    case 0:
		    value = ad->regX1A[channel];
		    break;
	    case 1:
		    value = ad->regX1B[channel];
		    break;
	    case 2:
		    value = ad->regC[channel];
		    break;
	    case 3:
		    value = ad->regM[channel];
		    break;
	    case 4:
		    switch ((f >> 7) & 0x3f) {
			case 1:
				value = ad->regControl;
				break;
			case 2:
				value = ad->regOFS0;
				break;
			case 3:
				value = ad->regOFS1;
				break;
			case 6:
				value = ad->regABSelect[0];
				break;
			case 7:
				value = ad->regABSelect[1];
				break;
			case 8:
				value = ad->regABSelect[2];
				break;
			case 9:
				value = ad->regABSelect[3];
				break;
			default:
				break;

		    }
		    break;
	    default:
		    break;
	}
	ad->shiftOut = value;
	ad->shiftOutCnt = 24;
}

/**
 ******************************************************************************
 * \fn static void AD537x_SpecialFunction(AD537x *ad,uint8_t sc,uint16_t f)
 ******************************************************************************
 */
static void
AD537x_SpecialFunction(AD537x * ad, uint8_t sc, uint16_t f)
{
	switch (sc) {
	    case 0:		/* NOP */
		    break;;
	    case 1:
		    ad->regControl = f & 7;
		    break;
	    case 2:
		    ad->regOFS0 = f & 0x3fff;
		    updateDAC_Group(ad,0);
		    break;
	    case 3:
		    ad->regOFS1 = f & 0x3fff;
		    updateDAC_Group(ad,1);
		    updateDAC_Group(ad,2);
		    updateDAC_Group(ad,3);
		    break;
	    case 5:
		    AD537x_Readback(ad, f);
		    break;
	    case 6:
		    ad->regABSelect[0] = f & 0xff;
		    updateDAC_Group(ad,0);
		    break;
	    case 7:
		    ad->regABSelect[1] = f & 0xff;
		    updateDAC_Group(ad,1);
		    break;
	    case 8:
		    ad->regABSelect[2] = f & 0xff;
		    updateDAC_Group(ad,2);
		    break;
	    case 9:
		    ad->regABSelect[3] = f & 0xff;
		    updateDAC_Group(ad,3);
		    break;
	    case 0xb:
		    ad->regABSelect[0] = ad->regABSelect[1] =
			ad->regABSelect[2] = ad->regABSelect[3] = f & 0xff;
		    updateDAC_All(ad);
		    break;

	}
}

/*
 *************************************************************************
 * The X register
 *************************************************************************
 */

static void
AD537x_WriteDacData(AD537x * ad, int8_t *channelList, uint16_t value)
{
	int i;
	for(i = 0; channelList[i] >= 0; i++) {
		unsigned int channel = channelList[i];
		if(channel >= array_size(ad->regX1B)) {
			fprintf(stderr,"AD537x: Bad channel in list\n");
			exit(1);
		}
		if(ad->regControl & 4) {
			ad->regX1B[channel] = value;
		} else {
			ad->regX1A[channel] = value;
		}
		updateDAC(ad,channel);
	}
}

/*
 *************************************************************************
 * The C register
 *************************************************************************
 */
static void
AD537x_WriteDacOffset(AD537x * ad, int8_t *channelList, uint16_t value)
{

	int i;
	for(i = 0; channelList[i] >= 0; i++) {
		unsigned int channel = channelList[i];
		if(channel >= array_size(ad->regC)) {
			fprintf(stderr,"Bad channel in list\n");
			exit(1);
		}
		ad->regC[channel] = value;
		updateDAC(ad,channel);
	}
}

/**
 **********************************************************************
 * The "M" Register 
 **********************************************************************
 */
static void
AD537x_WriteDacGain(AD537x * ad, int8_t *channelList, uint16_t value)
{
	int i;
	for(i = 0; channelList[i] >= 0; i++) {
		unsigned int channel = channelList[i];
		if(channel >= array_size(ad->regM)) {
			fprintf(stderr,"Bad channel in list\n");
			exit(1);
		}
		ad->regM[channel] = value;
		updateDAC(ad,channel);
	}
}

static void
AD537x_Execute(AD537x * ad, uint32_t value)
{
	int8_t *channelList;
	uint32_t mode = (value >> 22) & 3;
	uint32_t addr = (value >> 16) & 0x3f;
	uint32_t data = value & 0xffff;
	channelList = channelLists[addr];
	switch (mode) {
	    case 0:
		    AD537x_SpecialFunction(ad, addr, data);
		    break;
	    case 1:
		    AD537x_WriteDacGain(ad, channelList, data);
		    break;
	    case 2:
		    AD537x_WriteDacOffset(ad, channelList, data);
		    break;
	    case 3:
		    AD537x_WriteDacData(ad, channelList, data);
		    break;
		    /* Make the silly compiler happy */
	    default:
		    break;
	}

}

static void
clk_change(SigNode * node, int value, void *clientData)
{
	AD537x *ad = clientData;
	int sdi;
	if (value == SIG_HIGH) {
		if (ad->shiftOutCnt && ad->shiftOutEnabled) {
			ad->shiftOutCnt--;
			if ((ad->shiftOut >> ad->shiftOutCnt) & 1) {
				SigNode_Set(ad->sigSdo, SIG_HIGH);
			} else {
				SigNode_Set(ad->sigSdo, SIG_LOW);
			}
		} else {
			ad->shiftOutEnabled = false;
			SigNode_Set(ad->sigSdo, SIG_OPEN);
		}
	} else {
		sdi = SigNode_Val(ad->sigSdi);
		if (ad->shiftInCnt < 24) {
			if (sdi == SIG_HIGH) {
				ad->shiftIn = (ad->shiftIn << 1) | 1;
			} else {
				ad->shiftIn = ad->shiftIn << 1;
			}
			ad->shiftInCnt++;
			//fprintf(stderr,"Shiftin %08x\n",ad->shiftIn);
		}
	}
}

static void
sync_change(SigNode * node, int value, void *clientData)
{
	AD537x *ad = clientData;
	CycleCounter_t now = CycleCounter_Get();
	//fprintf(stderr,"Sync change %u\n",value);
	if (value == SIG_LOW) {
		//StartWriteOperation
		if(ad->shiftOutCnt) {   
			if ((now - ad->syncHighTimeStamp) >= NanosecondsToCycles(270)) {
				ad->shiftOutEnabled = true;
			} else {
				fprintf(stderr,"AD537x: Sync high time to small\n");
			}
		}
		ad->shiftInCnt = 0;
		ad->shiftIn = 0;
		if (!ad->traceSclk) {
			ad->traceSclk = SigNode_Trace(ad->sigSclk, clk_change, ad);
		}
	} else {
		ad->syncHighTimeStamp = now;
		if(ad->shiftOutCnt) {
			SigNode_Set(ad->sigSdo, SIG_LOW);
		} else {
			SigNode_Set(ad->sigSdo, SIG_OPEN);
		}
		if (ad->traceSclk) {
			SigNode_Untrace(ad->sigSclk, ad->traceSclk);
			ad->traceSclk = NULL;
		}
		/* Input register addressed is updated here */
		if (ad->shiftInCnt == 24) {
			if(SigNode_Val(ad->sigSclk) != SIG_LOW) {
				fprintf(stderr,"AD537x SPI driver bug: Clock not low on rising sync\n");
			}
			AD537x_Execute(ad, ad->shiftIn);
		} else if(ad->shiftInCnt) {
			fprintf(stderr, "AD537x SPI driver bug: Shift in %u bits instead of 24\n",
				ad->shiftInCnt);
		}
	}

}

static void
reset_change(SigNode * node, int value, void *clientData)
{
	if (value == SIG_HIGH) {
		/** Make busy for some time and restore register values to default */
	}
	fprintf(stderr, "AD537x Reset not implemented\n");
}

static void
clr_change(SigNode * node, int value, void *clientData)
{
	fprintf(stderr, "AD537x CLR not implemented\n");
}

static void
ldac_change(SigNode * node, int value, void *clientData)
{
	AD537x *ad = clientData; 
	if(value == SIG_LOW) {
		updateDAC_All(ad);
	}
}

void
AD537x_New(const char *name)
{

	AD537x *ad = sg_new(AD537x);
	int i;
	ad->sigSync = SigNode_New("%s.sync", name);
	ad->sigSdi = SigNode_New("%s.sdi", name);
	ad->sigSclk = SigNode_New("%s.sclk", name);
	ad->sigSdo = SigNode_New("%s.sdo", name);
	ad->sigBusy = SigNode_New("%s.busy", name);
	ad->sigReset = SigNode_New("%s.reset", name);
	ad->sigClr = SigNode_New("%s.clr", name);
	ad->sigLdac = SigNode_New("%s.ldac", name);
	ad->shiftOutEnabled = false;
	ad->regOFS1 = ad->regOFS0 = 0x1555;
	ad->anaVRef = 5.0;
	ad->anaVSigGnd = 0.0;
	for(i = 0; i < 32; i++) {
		ad->regX1A[i] = 0x5554;
		ad->regX1B[i] = 0x5554;
		ad->regC[i] = 0x8000;
		ad->regM[i] = 0xffff;
	}
	if (!ad->sigSync || !ad->sigSdi || !ad->sigSclk || !ad->sigLdac ||
	    !ad->sigSdo || !ad->sigBusy || !ad->sigReset || !ad->sigClr) {
		fprintf(stderr, "Can not create signal lines for AD537x\n");
		exit(1);
	}
	SigNode_Trace(ad->sigSync, sync_change, ad);
	SigNode_Trace(ad->sigReset, reset_change, ad);
	SigNode_Trace(ad->sigClr, clr_change, ad);
	SigNode_Trace(ad->sigLdac, ldac_change, ad);
	updateDAC_All(ad);
	for (i = 0; i < 32; i++) {
                DbgExport_DBL(ad->anaVOut[i], "%s.VOut%d", name, i);
        }
}
