/*
 ***********************************************************************************************
 * M32C A/D converter module
 *
 * Copyright 2010 Jochen Karrer. All rights reserved.
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
 ***********************************************************************************************
 */
#if 0
#define dbgprintf(...) { fprintf(stderr,__VA_ARGS__); }
#else
#define dbgprintf(...)
#endif

#define REG_AD0_0(base)		(0x380)
#define REG_AD0_1(base)		(0x382)
#define REG_AD0_2(base)		(0x384)
#define REG_AD0_3(base)		(0x386)
#define REG_AD0_4(base)		(0x388)
#define REG_AD0_5(base)		(0x38a)
#define REG_AD0_6(base)		(0x38c)
#define REG_AD0_7(base)		(0x38e)

#define REG_AD0CON4(base)	(0x392)
#define	REG_AD0CON2(base)	(0x394)
#define		AD0CON2_SMP		(1 << 0)
#define REG_AD0CON3(base)	(0x395)
#define		AD0CON3_DUS		(1 << 0)
#define		AD0CON3_MSSS	(1 << 1)
#define		AD0CON3_CKS2	(1 << 2)
#define 	AD0CON3_MFS0	(1 << 3)
#define 	AD0CON3_MFS1	(1 << 4)
#define REG_AD0CON0(base)	(0x396)
#define		AD0CON0_CH0	(1 << 0)
#define		AD0CON0_CH1	(1 << 1)
#define		AD0CON0_CH2	(1 << 2)
#define		AD0CON0_MD0	(1 << 3)
#define		AD0CON0_MD1	(1 << 4)
#define		AD0CON_MD	(3 << 3)
#define			MD_ONESHOT	(0 << 3)
#define			MD_REPEAT	(1 << 3)
#define			MD_SWEEP	(2 << 3)
#define			MD_REPSWEEP	(3 << 3)
#define		AD0CON0_TRG	(1 << 5)
#define		AD0CON0_ADST	(1 << 6)
#define		AD0CON0_CKS0	(1 << 7)
#define REG_AD0CON1(base)	(0x397)
#define		AD0CON1_SCAN	(3 << 0)
#define		AD0CON1_BITS	(1 << 3)
#define		AD0CON1_CKS1	(1 << 4)


#include "bus.h"
#include "sgstring.h"
#include "signode.h"
#include "cycletimer.h"
#include "adc_m32c.h"
#include "clock.h"

struct M32C_Adc {
	BusDevice bdev;
	Clock_t *clkIn;
	Clock_t *clkPhiAD;
	CycleTimer meas_timer;
	uint8_t reg_con0;
	uint8_t reg_con1;
	uint8_t reg_con2;
	uint8_t reg_con3;
	uint8_t reg_con4;
	uint16_t reg_ad[8];
	SigNode *sigIrq;
	uint16_t vref_mv;
	uint16_t anin_mv[32];
};

static void
post_interrupt(M32C_Adc * adc)
{
	SigNode_Set(adc->sigIrq, SIG_LOW);
	SigNode_Set(adc->sigIrq, SIG_HIGH);
}

static void
update_clock(M32C_Adc *adc) 
{
	uint8_t cks0 = !!(adc->reg_con0 & AD0CON0_CKS0);
	uint8_t cks1 = !!(adc->reg_con1 & AD0CON1_CKS1);
	uint8_t cks2 = !!(adc->reg_con2 & AD0CON3_CKS2);
	uint8_t cks = cks0 | (cks1 << 1) | (cks2 << 2);
	uint32_t div;
	switch(cks) {
		case 0:
			div = 4;
			break;
		case 1:
			div = 2;
			break;
		case 2:
			div = 3;
			break;
		case 3:
			div = 1;
			break;
		case 4:
			div = 8;
			break;
		case 6:
			div = 6;
			break;
		default:
			fprintf(stderr,"Illegal A/D clock divider\n");
			div = 100000;
			break;
	}
	Clock_MakeDerived(adc->clkPhiAD,adc->clkIn,1,div);
}

static void
update_channel(M32C_Adc * adc, unsigned int channel, unsigned int dstreg)
{
	if (channel >= array_size(adc->anin_mv)) {
		fprintf(stderr, "Bug in %s line %d\n", __FILE__, __LINE__);
		exit(1);
	}
	if (adc->vref_mv) {
		uint32_t adval = adc->anin_mv[channel] * 1023 / 3300;
		if (adval < 1024) {
			adc->reg_ad[dstreg] = adval;
		} else {
			adc->reg_ad[dstreg] = 1023;
		}
		dbgprintf("ADC meas ch %d: %d\n", channel, adval);
	} else {
		dbgprintf("ADC VREF is 0\n");
	}
}

static void
meassurement_done(void *clientData)
{
	M32C_Adc *adc = clientData;
	unsigned int channel;
	unsigned int aps = (adc->reg_con2 >> 1) & 3;
	unsigned int md = (adc->reg_con0 & AD0CON_MD);
	unsigned int dstreg;
	int i;
	channel = (aps << 3);
	if (md == MD_ONESHOT) {
		dstreg = adc->reg_con0 & 7;
		channel |= adc->reg_con0 & 7;
		update_channel(adc, channel, dstreg);
	} else if (md == MD_SWEEP) {
		unsigned int scan = adc->reg_con1 & AD0CON1_SCAN;
		unsigned int cnt = 2 + 2 * scan;
		//fprintf(stderr,"Scan from %d: cnt %d\n",channel,cnt);
		for (i = 0; i < cnt; i++) {
			update_channel(adc, channel | i, i);
		}
	} else {
		fprintf(stderr, "Unimplemented ADC mode 0x%x\n", md);
	}
	adc->reg_con0 &= ~AD0CON0_ADST;
	post_interrupt(adc);
}

static uint32_t
ad0_read(void *clientData, uint32_t address, int rqlen)
{
	M32C_Adc *adc = clientData;
	return adc->reg_ad[0];
}

static uint32_t
ad1_read(void *clientData, uint32_t address, int rqlen)
{
	M32C_Adc *adc = clientData;
	return adc->reg_ad[1];
}

static uint32_t
ad2_read(void *clientData, uint32_t address, int rqlen)
{
	M32C_Adc *adc = clientData;
	return adc->reg_ad[2];
}

static uint32_t
ad3_read(void *clientData, uint32_t address, int rqlen)
{
	M32C_Adc *adc = clientData;
	return adc->reg_ad[3];
}

static uint32_t
ad4_read(void *clientData, uint32_t address, int rqlen)
{
	M32C_Adc *adc = clientData;
	return adc->reg_ad[4];
}

static uint32_t
ad5_read(void *clientData, uint32_t address, int rqlen)
{
	M32C_Adc *adc = clientData;
	return adc->reg_ad[5];
}

static uint32_t
ad6_read(void *clientData, uint32_t address, int rqlen)
{
	M32C_Adc *adc = clientData;
	return adc->reg_ad[6];
}

static uint32_t
ad7_read(void *clientData, uint32_t address, int rqlen)
{
	M32C_Adc *adc = clientData;
	return adc->reg_ad[7];
}

static void
ad0_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "A/D converter value not readable\n");
}

static uint32_t
ad0con0_read(void *clientData, uint32_t address, int rqlen)
{
	M32C_Adc *adc = clientData;
	return adc->reg_con0;
}

static void
ad0con0_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	M32C_Adc *adc = clientData;
	unsigned int md;
	uint32_t required_time;
	uint8_t diff = value ^ adc->reg_con0;
	uint32_t requiredPhiCycles;
	FractionU64_t ratio;
	bool shold = !!(adc->reg_con2 & AD0CON2_SMP);
	bool tenbits = !!(adc->reg_con1 & AD0CON1_BITS);

	if(shold) {
			if(tenbits) {
				requiredPhiCycles = 33;
			} else {
				requiredPhiCycles = 28;
			}
	} else {
			if(tenbits) {
				requiredPhiCycles = 59;
			} else {
				requiredPhiCycles = 49;
			}
	}
	adc->reg_con0 = value;
	if(diff & AD0CON0_CKS0) {
		update_clock(adc);
	}
	md = (value & AD0CON_MD);
	ratio = Clock_MasterRatio(adc->clkPhiAD);
	required_time = requiredPhiCycles * ratio.denom / ratio.nom;
	if (md & AD0CON0_MD1) {
		unsigned int scan = adc->reg_con1 & AD0CON1_SCAN;
		unsigned int cnt = 2 + 2 * scan;
		required_time *= cnt;
	}
	if (value & AD0CON0_ADST) {
		//fprintf(stderr,"Required %u, md %08x\n",required_time,value);
		if (!CycleTimer_IsActive(&adc->meas_timer)) {
			CycleTimer_Mod(&adc->meas_timer, required_time);
		}
	}
}

static uint32_t
ad0con1_read(void *clientData, uint32_t address, int rqlen)
{
	M32C_Adc *adc = clientData;
	return adc->reg_con1;
}

static void
ad0con1_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	M32C_Adc *adc = clientData;
	uint8_t diff = value ^ adc->reg_con1;
	adc->reg_con1 = value;
	if(diff & AD0CON1_CKS1) {
		update_clock(adc);
	}
}

static uint32_t
ad0con2_read(void *clientData, uint32_t address, int rqlen)
{
	M32C_Adc *adc = clientData;
	return adc->reg_con2;
}

static void
ad0con2_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	M32C_Adc *adc = clientData;
	adc->reg_con2 = value;
}

static uint32_t
ad0con3_read(void *clientData, uint32_t address, int rqlen)
{
	M32C_Adc *adc = clientData;
	return adc->reg_con3;
}

static void
ad0con3_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	M32C_Adc *adc = clientData;
	uint8_t diff = value ^ adc->reg_con3;
	adc->reg_con3 = value;
	if(diff & AD0CON3_CKS2) {
		update_clock(adc);
	}
}

static uint32_t
ad0con4_read(void *clientData, uint32_t address, int rqlen)
{
	M32C_Adc *adc = clientData;
	return adc->reg_con4;
}

static void
ad0con4_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	M32C_Adc *adc = clientData;
	adc->reg_con4 = value;
}

static void
M32CAdc_Unmap(void *owner, uint32_t base, uint32_t mask)
{
	IOH_Delete16(REG_AD0_0(base));
	IOH_Delete16(REG_AD0_1(base));
	IOH_Delete16(REG_AD0_2(base));
	IOH_Delete16(REG_AD0_3(base));
	IOH_Delete16(REG_AD0_4(base));
	IOH_Delete16(REG_AD0_5(base));
	IOH_Delete16(REG_AD0_6(base));
	IOH_Delete16(REG_AD0_7(base));
	IOH_Delete8(REG_AD0CON0(base));
	IOH_Delete8(REG_AD0CON1(base));
	IOH_Delete8(REG_AD0CON2(base));
	IOH_Delete8(REG_AD0CON3(base));
	IOH_Delete8(REG_AD0CON4(base));
}

static void
M32CAdc_Map(void *owner, uint32_t _base, uint32_t mask, uint32_t mapflags)
{
	M32C_Adc *adc = owner;
	uint32_t flags = IOH_FLG_HOST_ENDIAN | IOH_FLG_PRD_RARP | IOH_FLG_PA_CBSE;
	IOH_New16f(REG_AD0_0(base), ad0_read, ad0_write, adc, flags);
	IOH_New16f(REG_AD0_1(base), ad1_read, ad0_write, adc, flags);
	IOH_New16f(REG_AD0_2(base), ad2_read, ad0_write, adc, flags);
	IOH_New16f(REG_AD0_3(base), ad3_read, ad0_write, adc, flags);
	IOH_New16f(REG_AD0_4(base), ad4_read, ad0_write, adc, flags);
	IOH_New16f(REG_AD0_5(base), ad5_read, ad0_write, adc, flags);
	IOH_New16f(REG_AD0_6(base), ad6_read, ad0_write, adc, flags);
	IOH_New16f(REG_AD0_7(base), ad7_read, ad0_write, adc, flags);
	IOH_New8(REG_AD0CON0(base), ad0con0_read, ad0con0_write, adc);
	IOH_New8(REG_AD0CON1(base), ad0con1_read, ad0con1_write, adc);
	IOH_New8(REG_AD0CON2(base), ad0con2_read, ad0con2_write, adc);
	IOH_New8(REG_AD0CON3(base), ad0con3_read, ad0con3_write, adc);
	IOH_New8(REG_AD0CON4(base), ad0con4_read, ad0con4_write, adc);
}

/**
 * Set the value of an A/D channel
 */
void
M32C_AdChSet(M32C_Adc * adc, unsigned int channel, uint16_t mvolt)
{
	if (channel >= array_size(adc->anin_mv)) {
		fprintf(stderr, "M32C_Adc: Illegal A/D channel %d\n", channel);
		return;
	}
	adc->anin_mv[channel] = mvolt;
}

M32C_Adc *
M32C_AdcNew(const char *name)
{
	M32C_Adc *adc = sg_new(M32C_Adc);
	adc->bdev.first_mapping = NULL;
	adc->bdev.Map = M32CAdc_Map;
	adc->bdev.UnMap = M32CAdc_Unmap;
	adc->bdev.owner = adc;
	adc->bdev.hw_flags = MEM_FLAG_WRITABLE | MEM_FLAG_READABLE;
	adc->vref_mv = 3300;
	adc->sigIrq = SigNode_New("%s.irq", name);
	adc->clkIn = Clock_New("%s.clk",name);
	adc->clkPhiAD = Clock_New("%s.phiAD",name);
	if(!adc->clkIn || !adc->clkPhiAD) {
		fprintf(stderr, "Can not create M32C ADC clock\n");
		exit(1);
	}
	if (!adc->sigIrq) {
		fprintf(stderr, "Can not create M32C ADC interrupt\n");
		exit(1);
	}
	SigNode_Set(adc->sigIrq, SIG_HIGH);
	CycleTimer_Init(&adc->meas_timer, meassurement_done, adc);
	update_clock(adc);
	return adc;
}
