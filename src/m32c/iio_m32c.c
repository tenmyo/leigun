/*
 ***********************************************************************************************
 * M32C Intelligent IO module
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
#include "signode.h"
#include "bus.h"
#include "sgstring.h"
#include "iio_m32c.h"
#include "cpu_m32c.h"
#include "clock.h"

#define REG_IIO0IR	(0xa0)
#define REG_IIO1IR	(0xa1)
#define REG_IIO2IR	(0xa2)
#define REG_IIO3IR	(0xa3)
#define REG_IIO4IR	(0xa4)
#define REG_IIO5IR	(0xa5)
#define REG_IIO6IR	(0xa6)
#define REG_IIO7IR	(0xa7)
#define REG_IIO8IR	(0xa8)
#define REG_IIO9IR	(0xa9)
#define REG_IIO10IR	(0xaa)
#define REG_IIO11IR	(0xab)

#define REG_IIO0IE	(0xb0)
#define REG_IIO1IE	(0xb1)
#define REG_IIO2IE	(0xb2)
#define REG_IIO3IE	(0xb3)
#define REG_IIO4IE	(0xb4)
#define REG_IIO5IE	(0xb5)
#define REG_IIO6IE	(0xb6)
#define REG_IIO7IE	(0xb7)
#define REG_IIO8IE	(0xb8)
#define REG_IIO9IE	(0xb9)
#define REG_IIO10IE	(0xba)
#define REG_IIO11IE	(0xbb)
#define		IE_IRLT	(1)

#define REG_G1BT	(0x120)
#define REG_G1BCR0	(0x122)
#define		G1BCR0_BCK_MSK		(3)
#define		G1BCR0_BCK_SHIFT	(0)
#define		G1BCR0_DIV_MSK		(0x1f << 2)
#define		G1BCR0_DIV_SHIFT	(2)
#define		G1BCR0_IT		(1 << 7)
#define REG_G1BCR1	(0x123)
#define		G1BCR1_RST1	(1 << 1)
#define		G1BCR1_RST2	(1 << 2)
#define		G1BCR1_BTS	(1 << 4)
#define		G1BCR1_UD_MSK	(3 << 5)
#define		G1BCR1_UD_SHIFT (5)
#define REG_G1TMCR0	(0x118)
#define REG_G1TMCR1	(0x119)
#define REG_G1TMCR2	(0x11A)
#define REG_G1TMCR3	(0x11B)
#define REG_G1TMCR4	(0x11C)
#define REG_G1TMCR5	(0x11D)
#define REG_G1TMCR6	(0x11E)
#define REG_G1TMCR7	(0x11F)
#define		G1TMCR_CTS_MSK		(3)
#define		G1TMCR_CTS_SHIFT	(0)
#define		G1TMCR_DF_MSK		(3 << 2)
#define		G1TMCR_DF_SHIFT		(2)
#define		G1TMCR_GT		(1 << 4)
#define		G1TMCR_GOC		(1 << 5)
#define		G1TMCR_GSC		(1 << 6)
#define		G1TMCR_PR		(1 << 7)
#define REG_G1TPR6	(0x124)
#define REG_G1TPR7	(0x125)
#define REG_G1TM0	(0x100)
#define REG_G1PO0	(0x100)
#define REG_G1TM1	(0x102)
#define REG_G1PO1	(0x102)
#define REG_G1TM2	(0x104)
#define REG_G1PO2	(0x104)
#define REG_G1TM3	(0x106)
#define REG_G1PO3	(0x106)
#define REG_G1TM4	(0x108)
#define REG_G1PO4	(0x108)
#define REG_G1TM5	(0x10A)
#define REG_G1PO5	(0x10A)
#define REG_G1TM6	(0x10C)
#define REG_G1PO6	(0x10C)
#define REG_G1TM7	(0x10E)
#define REG_G1PO7	(0x10E)
#define REG_G1POCR0	(0x110)
#define	REG_G1POCR1	(0x111)
#define REG_G1POCR2	(0x112)
#define REG_G1POCR3	(0x113)
#define REG_G1POCR4	(0x114)
#define REG_G1POCR5	(0x115)
#define REG_G1POCR6	(0x116)
#define REG_G1POCR7	(0x117)
#define		G1POCR_MOD_MSK		(7)
#define		G1POCR_MOD_SHIFT	(0)
#define		G1POCR_IVL		(1 << 4)
#define		G1POCR_RLD		(1 << 5)
#define		G1POCR0_BTRE		(1 << 6)
#define		G1POCR_INV		(1 << 7)
#define REG_G1FS	(0x127)
#define	REG_G1FE	(0x126)
#define REG_G2BT	(0x160)
#define REG_G2BCR0	(0x162)
#define		G2BCR1_RST0	(1 << 0)
#define		G2BCR1_RST1	(1 << 1)
#define		G2BCR1_RST2	(1 << 2)
#define		G2BCR1_BTS	(1 << 4)
#define		G2BCR1_PRP	(1 << 6)
#define REG_G2BCR1	(0x163)
#define REG_G2POCR0	(0x150)
#define REG_G2POCR1	(0x151)
#define REG_G2POCR2	(0x152)
#define REG_G2POCR3	(0x153)
#define REG_G2POCR4	(0x154)
#define REG_G2POCR5	(0x155)
#define REG_G2POCR6	(0x156)
#define REG_G2POCR7	(0x157)
#define		G2POCR_MOD_MSK		(7)
#define		G2POCR_MOD_SHIFT	(0)
#define		G2POCR_PRT		(1 << 3)
#define		G2POCR_IVL		(1 << 4)
#define		G2POCR_RLD		(1 << 5)
#define		G2POCR_RTP		(1 << 6)
#define		G2POCR_INV		(1 << 7)
#define REG_G2PO0	(0x140)
#define REG_G2PO1	(0x142)
#define REG_G2PO2	(0x144)
#define REG_G2PO3	(0x146)
#define REG_G2PO4	(0x148)
#define REG_G2PO5	(0x14A)
#define REG_G2PO6	(0x14C)
#define REG_G2PO7	(0x14E)
#define REG_G2RTP	(0x167)
#define REG_G2FE	(0x166)
#define REG_BTSR	(0x164)
#define		BTSR_BT1S	(1 << 1)
#define		BTSR_BT2S	(1 << 2)

typedef struct M32C_IIO M32C_IIO; 

typedef struct IIOChannel {
	M32C_IIO *iio;
	unsigned int idx;
	SigNode *sigOut;
	CycleTimer pwmEventTimer;
	int activeLvl;
	int inactiveLvl;
} IIOChannel;

struct M32C_IIO {
	BusDevice bdev;
	/* The interrupt inputs */
	SigNode *irqCan10;
	SigNode *irqU5r;
	SigNode *irqSio0r;
	SigNode *irqG0ri;
	SigNode *irqTm13;

	SigNode *irqCan11;
	SigNode *irqU5t;
	SigNode *irqSio0t;
	SigNode *irqG0to;
	SigNode *irqTm14;

	SigNode *irqSio1r;
	SigNode *irqG1ri;
	SigNode *irqTm12;

	SigNode *irqSio1t;
	SigNode *irqG1To;
	SigNode *irqPo27;	
	SigNode *irqTm10;

	SigNode *irqSrt0;
	SigNode *irqSrt1;
	SigNode *irqBt1;
	SigNode *irqTm17;

	SigNode *irqCan12;
	SigNode *irqCan1wu;
	SigNode *irqSio2r;
	SigNode *irqPo21;

	SigNode *irqSio2t;
	SigNode *irqPo20;

	SigNode *irqIe0;
	SigNode *irqPo22;

	SigNode *irqIe1;
	SigNode *irqIe2;
	SigNode *irqBt2;
	SigNode *irqPo23;
	SigNode *irqTm11;

	SigNode *irqCan00;
	SigNode *irqInt6;
	SigNode *irqU6r;
	SigNode *irqPo24;
	SigNode *irqTm15;

	SigNode *irqCan01;
	SigNode *irqInt7;
	SigNode *irqU6t;
	SigNode *irqPo25;
	SigNode *irqTm16;

	SigNode *irqCan02;
	SigNode *irqInt8;
	SigNode *irqPo26;

	/* The interrupt outputs */
	SigNode *irqIIO[12];

	uint8_t regIr[12];
	uint8_t regIe[12];
	
	/* Waveform generation registers */
	uint16_t regG1BT;
	uint16_t regG2BT;
	uint8_t regG1BCR0;
	uint8_t regG2BCR0;
	uint8_t regG1BCR1;
	uint8_t regG2BCR1;
	uint8_t regG1POCR[8];
	uint8_t regG2POCR[8];
	uint8_t regG1TMCR[8];
	uint8_t regG2TMCR[8];
	uint8_t retG1PTR6;
	uint8_t regG1PTR7;
	uint16_t regG1TM[8];

	uint16_t regG1PO[8];
	uint16_t regG2PO[8];
	uint16_t regG1POint[8];
	uint16_t regG2POint[8];
	uint8_t regG1FS;
	uint8_t regG2FE;
	uint8_t regG1FE;
	uint8_t regBTSR;

	//SigNode *sigOutC1[8];
	//SigNode *sigOutC2[8];
	Clock_t *clkIn;
	Clock_t *clkBT1;
	Clock_t *clkBT2;
	CycleCounter_t bt1LastUpdate;
	CycleCounter_t bt1ResetStamp;
	CycleCounter_t bt2LastUpdate;
	CycleCounter_t bt1AccCycles;
	CycleCounter_t bt2AccCycles;
	uint32_t gp1Period;
	uint32_t gp2Period;

	CycleTimer c1PwmEventTimer[8];
	CycleTimer c2PwmEventTimer[8];

	IIOChannel bt1Channel[8];
	IIOChannel bt2Channel[8];
};

#if 0
static void
set_pwm_out(M32C_IIO *iio,unsigned int group,unsigned int chNr)
{
	uint16_t pwidth;
	IIOChannel *ch;
	if(chNr > 7) {
		return;
	}
	if(group == 0) {
		ch = &iio->bt1Channel[chNr];
		pwidth = iio->regG1POint[chNr];
		if(pwidth > 30) {
			SigNode_Set(ch->sigOut,SIG_LOW);
		} else {
			SigNode_Set(ch->sigOut,SIG_HIGH);
		}
	} else {
		ch = &iio->bt2Channel[chNr];
		pwidth = iio->regG2PO[chNr];
		if(pwidth > 30) {
			SigNode_Set(ch->sigOut,SIG_LOW);
		} else {
			SigNode_Set(ch->sigOut,SIG_HIGH);
		}
	}	
}
#endif
/**
 *****************************************************************
 * \fn static void update_iio_irq(M32C_IIO *iio,int index) 
 *****************************************************************
 */
static void
update_iio_irq(M32C_IIO *iio,int index) 
{
	uint8_t irq = iio->regIr[index] & iio->regIe[index];
	if(index == 9999) {
	 	fprintf(stderr,"IR %02x mask %02x, Irq is %02x\n",iio->regIr[index],iio->regIe[index],irq);
	}
	if(irq && (iio->regIe[index] & IE_IRLT)) {
		if(index == 9999) {
			fprintf(stderr,"Posting IIO%d Interrupt\n",index);
		}
		SigNode_Set(iio->irqIIO[index],SIG_LOW);	
	} else {
		//fprintf(stderr,"Unposting IIO%d Interrupt\n",index);
		SigNode_Set(iio->irqIIO[index],SIG_HIGH);	
	}
}

static void
IIO0_Update(SigNode *node,int value,void *clientData)
{
	M32C_IIO *iio = (M32C_IIO *) clientData;
	uint8_t ir = 0; 
	if(SigNode_Val(iio->irqCan10) == SIG_LOW) {
		ir |= (1 << 7);
	}
	if(SigNode_Val(iio->irqU5r) == SIG_LOW) {
		ir |= (1 << 6);
	}
	if(SigNode_Val(iio->irqSio0r) == SIG_LOW) {
		ir |= (1 << 5);
	}
	if(SigNode_Val(iio->irqG0ri) == SIG_LOW) {
		ir |= (1 << 4);
	}
	if(SigNode_Val(iio->irqTm13) == SIG_LOW) {
		ir |= (1 << 2);
	}
	iio->regIr[0] |= ir;
	if(ir) {
		update_iio_irq(iio,0);
	}
}

/**
 ******************************************************************************************
 * \fn static void IIO1_Update(SigNode *node,int value,void *clientData)
 ******************************************************************************************
 */
static void
IIO1_Update(SigNode *node,int value,void *clientData)
{
	M32C_IIO *iio = (M32C_IIO *) clientData;
	uint8_t ir = 0; 
	if(SigNode_Val(iio->irqCan11) == SIG_LOW) {
		ir |= (1 << 7);
	}
	if(SigNode_Val(iio->irqU5t) == SIG_LOW) {
		ir |= (1 << 6);
	}
	if(SigNode_Val(iio->irqSio0t) == SIG_LOW) {
		ir |= (1 << 5);
	}
	if(SigNode_Val(iio->irqG0to) == SIG_LOW) {
		ir |= (1 << 4);
	}
	if(SigNode_Val(iio->irqTm14) == SIG_LOW) {
		ir |= (1 << 2);
	}
	iio->regIr[1] |= ir;
	if(ir) {
		update_iio_irq(iio,1);
	}
}

static void
IIO2_Update(SigNode *node,int value,void *clientData)
{
	M32C_IIO *iio = (M32C_IIO *) clientData;
	uint8_t ir = 0; 
	if(SigNode_Val(iio->irqSio1r) == SIG_LOW) {
		ir |= (1 << 5);
	}
	if(SigNode_Val(iio->irqG1ri) == SIG_LOW) {
		ir |= (1 << 4);
	}
	if(SigNode_Val(iio->irqTm12) == SIG_LOW) {
		ir |= (1 << 2);
	}
	iio->regIr[2] |= ir;
	if(ir) {
		update_iio_irq(iio,2);
	}
}

static void
IIO3_Update(SigNode *node,int value,void *clientData)
{
	M32C_IIO *iio = (M32C_IIO *) clientData;
	uint8_t ir = 0; 
	if(SigNode_Val(iio->irqSio1t) == SIG_LOW) {
		ir |= (1 << 5);
	}
	if(SigNode_Val(iio->irqG1To) == SIG_LOW) {
		ir |= (1 << 4);
	}
	if(SigNode_Val(iio->irqPo27) == SIG_LOW) {
		ir |= (1 << 3);
	}
	if(SigNode_Val(iio->irqTm10) == SIG_LOW) {
		ir |= (1 << 2);
	}
	iio->regIr[3] |= ir;
	if(ir) {
		update_iio_irq(iio,3);
	}
}

static void
IIO4_Update(SigNode *node,int value,void *clientData)
{
	M32C_IIO *iio = (M32C_IIO *) clientData;
	uint8_t ir = 0; 
	if(SigNode_Val(iio->irqSrt0) == SIG_LOW) {
		ir |= (1 << 7);
	}
	if(SigNode_Val(iio->irqSrt1) == SIG_LOW) {
		ir |= (1 << 6);
	}
	if(SigNode_Val(iio->irqBt1) == SIG_LOW) {
		ir |= (1 << 4);
	}
	if(SigNode_Val(iio->irqTm17) == SIG_LOW) {
		ir |= (1 << 2);
	}
	iio->regIr[4] |= ir;
	if(ir) {
		update_iio_irq(iio,4);
	}
}

static void
IIO5_Update(SigNode *node,int value,void *clientData)
{
	M32C_IIO *iio = (M32C_IIO *) clientData;
	uint8_t ir = 0; 
	if(SigNode_Val(iio->irqCan12) == SIG_LOW) {
		ir |= (1 << 7);
	}
	if(SigNode_Val(iio->irqCan1wu) == SIG_LOW) {
		ir |= (1 << 6);
	}
	if(SigNode_Val(iio->irqSio2r) == SIG_LOW) {
		ir |= (1 << 4);
	}
	if(SigNode_Val(iio->irqPo21) == SIG_LOW) {
		ir |= (1 << 2);
	}
	iio->regIr[5] |= ir;
	if(ir) {
		update_iio_irq(iio,5);
	}
}

static void
IIO6_Update(SigNode *node,int value,void *clientData)
{
	M32C_IIO *iio = (M32C_IIO *) clientData;
	uint8_t ir = 0; 
	if(SigNode_Val(iio->irqSio2t) == SIG_LOW) {
		ir |= (1 << 4);
	}
	if(SigNode_Val(iio->irqPo20) == SIG_LOW) {
		ir |= (1 << 2);
	}
	iio->regIr[6] |= ir;
	if(ir) {
		update_iio_irq(iio,6);
	}
}

static void
IIO7_Update(SigNode *node,int value,void *clientData)
{
	M32C_IIO *iio = (M32C_IIO *) clientData;
	uint8_t ir = 0; 
	if(SigNode_Val(iio->irqIe0) == SIG_LOW) {
		ir |= (1 << 4);
	}
	if(SigNode_Val(iio->irqPo22) == SIG_LOW) {
		ir |= (1 << 2);
	}
	iio->regIr[7] |= ir;
	if(ir) {
		update_iio_irq(iio,7);
	}
}

static void
IIO8_Update(SigNode *node,int value,void *clientData)
{
	M32C_IIO *iio = (M32C_IIO *) clientData;
	uint8_t ir = 0; 
	if(SigNode_Val(iio->irqIe1) == SIG_LOW) {
		ir |= (1 << 7);
	}
	if(SigNode_Val(iio->irqIe2) == SIG_LOW) {
		ir |= (1 << 6);
	}
	if(SigNode_Val(iio->irqBt2) == SIG_LOW) {
		ir |= (1 << 4);
	}
	if(SigNode_Val(iio->irqPo23) == SIG_LOW) {
		ir |= (1 << 2);
	}
	if(SigNode_Val(iio->irqTm11) == SIG_LOW) {
		ir |= (1 << 1);
	}
	iio->regIr[8] |= ir;
	if(ir) {
		update_iio_irq(iio,8);
	}
}

static void
IIO9_Update(SigNode *node,int value,void *clientData)
{
	M32C_IIO *iio = (M32C_IIO *) clientData;
	uint8_t ir = 0; 
	if(SigNode_Val(iio->irqCan00) == SIG_LOW) {
		ir |= (1 << 7);
	}
	if(SigNode_Val(iio->irqInt6) == SIG_LOW) {
		ir |= (1 << 6);
	}
	if(SigNode_Val(iio->irqU6r) == SIG_LOW) {
		ir |= (1 << 5);
	}
	if(SigNode_Val(iio->irqPo24) == SIG_LOW) {
		ir |= (1 << 2);
	}
	if(SigNode_Val(iio->irqTm15) == SIG_LOW) {
		ir |= (1 << 1);
	}
	iio->regIr[9] |= ir;
	if(ir) {
		update_iio_irq(iio,9);
	}
}

static void
IIO10_Update(SigNode *node,int value,void *clientData)
{
	M32C_IIO *iio = (M32C_IIO *) clientData;
	uint8_t ir = 0; 
	if(SigNode_Val(iio->irqCan01) == SIG_LOW) {
		ir |= (1 << 7);
	}
	if(SigNode_Val(iio->irqInt7) == SIG_LOW) {
		ir |= (1 << 6);
	}
	if(SigNode_Val(iio->irqU6t) == SIG_LOW) {
		ir |= (1 << 5);
	}
	if(SigNode_Val(iio->irqPo25) == SIG_LOW) {
		ir |= (1 << 2);
	}
	if(SigNode_Val(iio->irqTm16) == SIG_LOW) {
		ir |= (1 << 1);
	}
	iio->regIr[10] |= ir;
	if(ir) {
		update_iio_irq(iio,10);
	}
}

static void
IIO11_Update(SigNode *node,int value,void *clientData)
{
	M32C_IIO *iio = (M32C_IIO *) clientData;
	uint8_t ir = 0; 
	SigNode_Trace(iio->irqCan02,IIO11_Update,iio);
	SigNode_Trace(iio->irqInt8,IIO11_Update,iio);
	SigNode_Trace(iio->irqPo26,IIO11_Update,iio);
	if(SigNode_Val(iio->irqCan02) == SIG_LOW) {
		ir |= (1 << 7);
	}
	if(SigNode_Val(iio->irqInt8) == SIG_LOW) {
		ir |= (1 << 6);
	}
	if(SigNode_Val(iio->irqPo26) == SIG_LOW) {
		ir |= (1 << 2);
	}
	iio->regIr[11] |= ir;
	if(ir) {
		update_iio_irq(iio,11);
	}
}

static uint32_t
iio0ir_read(void *clientData,uint32_t address,int rqlen)
{
	int index = 0;
	M32C_IIO *iio = (M32C_IIO *) clientData;
        return iio->regIr[index];
}

static void
iio0ir_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	M32C_IIO *iio = (M32C_IIO *) clientData;
	int index = 0;
	iio->regIr[index] = iio->regIr[index] & value;
	update_iio_irq(iio,index);
}

static uint32_t
iio1ir_read(void *clientData,uint32_t address,int rqlen)
{
	int index = 1;
	M32C_IIO *iio = (M32C_IIO *) clientData;
        return iio->regIr[index];
}

static void
iio1ir_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	M32C_IIO *iio = (M32C_IIO *) clientData;
	int index = 1;
	iio->regIr[index] = iio->regIr[index] & value;
	update_iio_irq(iio,index);
}

static uint32_t
iio2ir_read(void *clientData,uint32_t address,int rqlen)
{
	int index = 2;
	M32C_IIO *iio = (M32C_IIO *) clientData;
        return iio->regIr[index];
}

static void
iio2ir_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	M32C_IIO *iio = (M32C_IIO *) clientData;
	int index = 2;
	iio->regIr[index] = iio->regIr[index] & value;
	update_iio_irq(iio,index);
}

static uint32_t
iio3ir_read(void *clientData,uint32_t address,int rqlen)
{
	int index = 3;
	M32C_IIO *iio = (M32C_IIO *) clientData;
        return iio->regIr[index];
}

static void
iio3ir_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	M32C_IIO *iio = (M32C_IIO *) clientData;
	int index = 3;
	iio->regIr[index] = iio->regIr[index] & value;
	update_iio_irq(iio,index);
}

static uint32_t
iio4ir_read(void *clientData,uint32_t address,int rqlen)
{
	int index = 4;
	M32C_IIO *iio = (M32C_IIO *) clientData;
        return iio->regIr[index];
}

static void
iio4ir_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	M32C_IIO *iio = (M32C_IIO *) clientData;
	int index = 4;
	iio->regIr[index] = iio->regIr[index] & value;
	update_iio_irq(iio,index);
}

static uint32_t
iio5ir_read(void *clientData,uint32_t address,int rqlen)
{
	int index = 5;
	M32C_IIO *iio = (M32C_IIO *) clientData;
        return iio->regIr[index];
}

static void
iio5ir_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	M32C_IIO *iio = (M32C_IIO *) clientData;
	int index = 5;
	iio->regIr[index] = iio->regIr[index] & value;
	update_iio_irq(iio,index);
}

static uint32_t
iio6ir_read(void *clientData,uint32_t address,int rqlen)
{
	int index = 6;
	M32C_IIO *iio = (M32C_IIO *) clientData;
        return iio->regIr[index];
}

static void
iio6ir_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	M32C_IIO *iio = (M32C_IIO *) clientData;
	int index = 6;
	iio->regIr[index] = iio->regIr[index] & value;
	update_iio_irq(iio,index);
}

static uint32_t
iio7ir_read(void *clientData,uint32_t address,int rqlen)
{
	int index = 7;
	M32C_IIO *iio = (M32C_IIO *) clientData;
        return iio->regIr[index];
}

static void
iio7ir_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	M32C_IIO *iio = (M32C_IIO *) clientData;
	int index = 7;
	iio->regIr[index] = iio->regIr[index] & value;
	update_iio_irq(iio,index);
}

static uint32_t
iio8ir_read(void *clientData,uint32_t address,int rqlen)
{
	int index = 8;
	M32C_IIO *iio = (M32C_IIO *) clientData;
        return iio->regIr[index];
}

static void
iio8ir_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	M32C_IIO *iio = (M32C_IIO *) clientData;
	int index = 8;
	iio->regIr[index] = iio->regIr[index] & value;
	update_iio_irq(iio,index);
}
static uint32_t
iio9ir_read(void *clientData,uint32_t address,int rqlen)
{
	int index = 9;
	M32C_IIO *iio = (M32C_IIO *) clientData;
        return iio->regIr[index];
}

static void
iio9ir_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	M32C_IIO *iio = (M32C_IIO *) clientData;
	int index = 9;
	iio->regIr[index] = iio->regIr[index] & value;
	update_iio_irq(iio,index);
}
static uint32_t
iio10ir_read(void *clientData,uint32_t address,int rqlen)
{
	int index = 10;
	M32C_IIO *iio = (M32C_IIO *) clientData;
	//fprintf(stderr,"iio10ir read %02x\n",iio->regIr[index]);
        return iio->regIr[index];
}

static void
iio10ir_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	M32C_IIO *iio = (M32C_IIO *) clientData;
	int index = 10;
	//fprintf(stderr,"iio10ir write %02x\n",value);
	iio->regIr[index] = iio->regIr[index] & value;
	update_iio_irq(iio,index);
}

static uint32_t
iio11ir_read(void *clientData,uint32_t address,int rqlen)
{
	int index = 11;
	M32C_IIO *iio = (M32C_IIO *) clientData;
        return iio->regIr[index];
}

static void
iio11ir_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	M32C_IIO *iio = (M32C_IIO *) clientData;
	int index = 11;
	iio->regIr[index] = iio->regIr[index] & value;
	update_iio_irq(iio,index);
}

static uint32_t
iioXie_read(void *clientData,uint32_t address,int rqlen)
{
	unsigned int index = address - REG_IIO0IE;
	M32C_IIO *iio = (M32C_IIO *) clientData;
	if(index > 11) {
		fprintf(stderr,"Bug in %s line %d\n",__FILE__,__LINE__);
		exit(1);
	}
	return iio->regIe[index]; 
}
static void
iioXie_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	unsigned int index = address - REG_IIO0IE;
	M32C_IIO *iio = (M32C_IIO *) clientData;
	if(index > 11) {
		fprintf(stderr,"Bug in %s line %d\n",__FILE__,__LINE__);
		exit(1);
	}
	iio->regIe[index] = value; 
	update_iio_irq(iio,index);
}

/*
 ****************************************************************
 * Timers / PWM
 ****************************************************************
 */

static void
update_bt1_clock(M32C_IIO *iio) 
{
	uint8_t bck = iio->regG1BCR0 & 3;
	uint32_t mul,div;
	div = ((iio->regG1BCR0 & 0x7c) >> 1) + 2;
	if(div == 64) {
		div = 1;
	}
	switch(bck) {
		case 0:
			mul = 0; 
			break;
		case 2:
			mul = 0;
			fprintf(stderr,"Two phase pulse signal input not imlemented\n");
			break;
		case 3:
			mul = 1;
			break;
		default:
			mul = 0; 
			fprintf(stderr,"Illegal clock source %u in G2BCR0\n",bck);	
			break;
	}
	Clock_MakeDerived(iio->clkBT1,iio->clkIn,mul,div);
}

/**
 ****************************************************************
 * \fn static void update_bt2_clock(M32C_IIO *iio) 
 * Invoked whenever a register changes which has an influence
 * on BaseTimer2 clock.
 ****************************************************************
 */
static void
update_bt2_clock(M32C_IIO *iio) 
{
	uint8_t bck = iio->regG2BCR0 & 3;
	uint32_t mul,div;
	div = ((iio->regG2BCR0 & 0x7c) >> 1) + 2;
	if(div == 64) {
		div = 1;
	}
	switch(bck) {
		case 0:
			mul = 0; 
			break;
		case 3:
			mul = 1;
			break;
		default:
			mul = 0; 
			fprintf(stderr,"Illegal clock source %u in G2BCR0\n",bck);	
			break;
	}
	Clock_MakeDerived(iio->clkBT2,iio->clkIn,mul,div);
}

/**
 ****************************************************************
 * \fn static void actualize_bt1(M32C_IIO *iio)
 ****************************************************************
 */
static void
actualize_bt1(M32C_IIO *iio)
{
	CycleCounter_t now;
	FractionU64_t frac;
	CycleCounter_t cycles;
	CycleCounter_t acc;
	uint64_t ctrCycles;
	uint32_t period;
	unsigned int i;
	now = CycleCounter_Get();
	cycles = now - iio->bt1LastUpdate;
	iio->bt1LastUpdate = now;
	if(!(iio->regBTSR & BTSR_BT1S)) {
		return;
	}
	acc = iio->bt1AccCycles + cycles;
	frac = Clock_MasterRatio(iio->clkBT1);
	if((frac.nom == 0) || (frac.denom == 0)) {
		fprintf(stderr,"No clock for BT1\n");
		return;
	}
	ctrCycles = acc * frac.nom / frac.denom;
	acc -= ctrCycles * frac.denom / frac.nom;
	iio->bt1AccCycles = acc;
	period = 0x10000; 
	if(iio->regG1BCR1 & G1BCR1_RST1) {
		period = iio->regG1POint[0] + 2;
		if(period > 0x10000) {
			period = 0x10000;
		}
	}
	if(iio->regG1POCR[0] & G1POCR0_BTRE) {
		if(period > 0x400) {
			period = 0x400;
		}
	}
	if((iio->regG1BT + ctrCycles) >= period) {
		iio->regG1BT = (iio->regG1BT + ctrCycles) % period;
		iio->bt1ResetStamp = now - iio->regG1BT * frac.denom / frac.nom;
		for(i = 0; i < 8; i++) {
			if(iio->regG1POCR[i] &	G1POCR_RLD) {
				iio->regG1POint[i] = iio->regG1PO[i];
			}
		}
	} else {
		iio->regG1BT = iio->regG1BT + ctrCycles;
	}
	iio->gp1Period = period;
}

/**
 ***********************************************************************************
 * \fn static void actualize_bt2(M32C_IIO *iio)
 ***********************************************************************************
 */
static void
actualize_bt2(M32C_IIO *iio)
{
	CycleCounter_t now;
	FractionU64_t frac;
	CycleCounter_t cycles;
	CycleCounter_t acc;
	uint64_t ctrCycles = 0;
	uint32_t period;
	bool wrap = false;
	unsigned int i;
	now = CycleCounter_Get();
	cycles = now - iio->bt2LastUpdate;
	iio->bt2LastUpdate = now;
	if(!(iio->regBTSR & BTSR_BT2S)) {
		return;
	}
	acc = iio->bt2AccCycles + cycles;
	frac = Clock_MasterRatio(iio->clkBT2);
	if((frac.nom == 0) || (frac.denom == 0)) {
		fprintf(stderr,"No clock for BT2\n");
		return;
	}
	if(!(iio->regG2BCR1 & G2BCR1_RST0)) {
		ctrCycles = acc * frac.nom / frac.denom;
		acc -= ctrCycles * frac.denom / frac.nom;
		iio->bt2AccCycles = acc;
	}
	period = 0x10000; 
	if(iio->regG2BCR1 & G2BCR1_RST1) {
		period = iio->regG1POint[0] + 2;
		if(period > 0xffff) {
			period = 0x10000;
		}
	}
	if(iio->regG2BCR1 & G2BCR1_RST0) {
		actualize_bt1(iio);
		acc = now - iio->bt1ResetStamp;
		ctrCycles = acc * frac.nom / frac.denom;
		acc -= ctrCycles * frac.denom / frac.nom;
		iio->bt2AccCycles = acc;
		if(ctrCycles >= period) {
			wrap = true;
		}
		iio->regG2BT = ctrCycles % period;
		iio->gp2Period = iio->gp1Period;	/* Hack */
	} else {
		if(iio->regG2BT + ctrCycles >= period) {
			wrap = true;
			iio->regG2BT = (iio->regG2BT + ctrCycles) % period;
		} else {
			iio->regG2BT = iio->regG2BT + ctrCycles;
		}
		iio->gp2Period = period;
	}
	if(wrap) {
		for(i = 0; i < 8; i++) {
			if(iio->regG2POCR[i] &	G2POCR_RLD) {
				iio->regG2POint[i] = iio->regG2PO[i];
			}
		}
	}
}


static void
update_timeout_c1(M32C_IIO *iio,unsigned int chNr)
{
	IIOChannel *ch = &iio->bt1Channel[chNr];
	uint32_t period;
	int32_t tmrCycles;
	FractionU64_t frac;
	CycleCounter_t cycles;
	period = iio->gp1Period;
	frac = Clock_MasterRatio(iio->clkBT1);
	if((frac.nom == 0) || (frac.denom == 0)) {
		return;
	}
	if((period > iio->regG1POint[chNr]) && 
		((iio->regG1POint[chNr] > 0) | (iio->regG1POint[chNr] > 0))) {
		tmrCycles = iio->regG1POint[chNr] - iio->regG1BT;
		if(tmrCycles <= 0) {
			tmrCycles = period - iio->regG1BT;
			SigNode_Set(ch->sigOut,ch->inactiveLvl); 
		} else {
			if(iio->regG1POint[chNr] > 0) {
				SigNode_Set(ch->sigOut,ch->activeLvl); 
			} else {
				SigNode_Set(ch->sigOut,ch->inactiveLvl); 
			}
		}
		if(frac.nom) {
			cycles = tmrCycles * frac.denom / frac.nom;
			//fprintf(stderr,"period %u, gpo %u cnt %u Timeout in %llu cycles, tcyc %ld\n",
			//	period,iio->regG1POint[chNr],iio->regG1BT,cycles,tmrCycles);
			CycleTimer_Mod(&iio->bt1Channel[chNr].pwmEventTimer,cycles);
		}
	} else {
		SigNode_Set(ch->sigOut,ch->inactiveLvl);
	}
}

static void
update_timeout_c2(M32C_IIO *iio,unsigned int chNr)
{
	IIOChannel *ch = &iio->bt2Channel[chNr];
	uint32_t period;
	int32_t tmrCycles;
	FractionU64_t frac;
	CycleCounter_t cycles;
	period = iio->gp2Period;
	frac = Clock_MasterRatio(iio->clkBT2);
	if((frac.nom == 0) || (frac.denom == 0)) {
		return;
	}
	if((period > iio->regG2POint[chNr]) && 
		((iio->regG2POint[chNr] > 0) | (iio->regG2POint[chNr] > 0))) {
		tmrCycles = iio->regG2POint[chNr] - iio->regG2BT;
		/* If I'm behind the switching point tmrCycles is <= 0 */
		if(tmrCycles <= 0) {
			tmrCycles = period - iio->regG2BT;
			SigNode_Set(ch->sigOut,ch->inactiveLvl); 
		} else {
			if(iio->regG2POint[chNr] > 0) {
				SigNode_Set(ch->sigOut,ch->activeLvl); 
			} else {
				SigNode_Set(ch->sigOut,ch->inactiveLvl);
			}
		}
		if(frac.nom) {
			cycles = tmrCycles * frac.denom / frac.nom;
			//fprintf(stderr,"period %u, gpo %u cnt %u Timeout in %llu cycles, tcyc %ld\n",
			//	period,iio->regG2POint[chNr],iio->regG2BT,cycles,tmrCycles);
			CycleTimer_Mod(&iio->bt2Channel[chNr].pwmEventTimer,cycles);
		} else {
			fprintf(stderr,"No clock for BT2\n");
		}
	} else {
		SigNode_Set(ch->sigOut,ch->inactiveLvl); /* Should be inactive */
//		fprintf(stderr,"period %u, gpo %u,%u\n",
//				period,iio->regG2POint[chNr],iio->regG2PO[chNr]);
		// Cancel the event handler
	}
}

static void
update_timeout_bt1(M32C_IIO *iio)
{
	unsigned int i;
	for(i = 0; i < 8; i++) {
		//if(iio->regG1FE & (1 << i) && SigNode_IsTraced(iio->bt1Channel[i].sigOut)) {
		if((iio->regG1FE >> i) & 1) {
			update_timeout_c1(iio,i);	
		} else {
			// Delete the event Timer
		}
	}
}

static void
update_timeout_bt2(M32C_IIO *iio)
{
	unsigned int i;
	for(i = 0; i < 8; i++) {
		if(iio->regG2FE & (1 << i) && SigNode_IsTraced(iio->bt2Channel[i].sigOut)) {
			update_timeout_c2(iio,i);	
		} else {
			// Delete the event Timer
		}
	}
}

static void
pwm_event_bt1(void *eventData) 
{
	IIOChannel *ch = eventData;
	actualize_bt1(ch->iio);
	update_timeout_c1(ch->iio,ch->idx);
	//fprintf(stderr,"PWM event timer 1 ch %u\n",ch->idx);
}

static void
pwm_event_bt2(void *eventData) 
{
	IIOChannel *ch = eventData;
	actualize_bt2(ch->iio);
	update_timeout_c2(ch->iio,ch->idx);
}

static uint32_t
g1bt_read(void *clientData,uint32_t address,int rqlen)
{
	M32C_IIO *iio = clientData;
	actualize_bt1(iio);
	return iio->regG1BT;
}

static void
g1bt_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	M32C_IIO *iio = clientData;
	actualize_bt1(iio);
	iio->regG1BT = value;
	update_timeout_bt1(iio);
}

static uint32_t
g1bcr0_read(void *clientData,uint32_t address,int rqlen)
{
	M32C_IIO *iio = clientData;
	return iio->regG1BCR0;
}

static void
g1bcr0_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	M32C_IIO *iio = clientData;
	iio->regG1BCR0 = value;
	update_bt1_clock(iio);
}

static uint32_t
g1bcr1_read(void *clientData,uint32_t address,int rqlen)
{
	M32C_IIO *iio = clientData;
	return iio->regG1BCR1;
}

static void
g1bcr1_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	M32C_IIO *iio = clientData;
	iio->regG1BCR1 = value;
}

static uint32_t
g1tmcr0_read(void *clientData,uint32_t address,int rqlen)
{
	 return 0;
}

static void
g1tmcr0_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
}
static uint32_t
g1tmcr1_read(void *clientData,uint32_t address,int rqlen)
{
	 return 0;
}

static void
g1tmcr1_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
}

static uint32_t
g1tmcr2_read(void *clientData,uint32_t address,int rqlen)
{
	 return 0;
}

static void
g1tmcr2_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
}

static uint32_t
g1tmcr3_read(void *clientData,uint32_t address,int rqlen)
{
	 return 0;
}

static void
g1tmcr3_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
}

static uint32_t
g1tmcr4_read(void *clientData,uint32_t address,int rqlen)
{
	 return 0;
}

static void
g1tmcr4_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
}
static uint32_t
g1tmcr5_read(void *clientData,uint32_t address,int rqlen)
{
	 return 0;
}

static void
g1tmcr5_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
}

static uint32_t
g1tmcr6_read(void *clientData,uint32_t address,int rqlen)
{
	 return 0;
}

static void
g1tmcr6_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
}

static uint32_t
g1tmcr7_read(void *clientData,uint32_t address,int rqlen)
{
	 return 0;
}

static void
g1tmcr7_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
}

static uint32_t
g1tpr6_read(void *clientData,uint32_t address,int rqlen)
{
	 return 0;
}

static void
g1tpr6_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
}

static uint32_t
g1tpr7_read(void *clientData,uint32_t address,int rqlen)
{
	 return 0;
}

static void
g1tpr7_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
}

static uint32_t
g1tmpo0_read(void *clientData,uint32_t address,int rqlen)
{
	M32C_IIO *iio = (M32C_IIO *) clientData;
	return iio->regG1PO[0];
}

static void
g1tmpo0_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	M32C_IIO *iio = (M32C_IIO *) clientData;
	actualize_bt1(iio);
	iio->regG1PO[0] = value;
	if((iio->regG1POCR[0] & G1POCR_RLD) == 0) {
		iio->regG1POint[0] = value;
	}
	update_timeout_c1(iio,0);
}

static uint32_t
g1tmpo1_read(void *clientData,uint32_t address,int rqlen)
{
	M32C_IIO *iio = (M32C_IIO *) clientData;
	return iio->regG1PO[1];
}

static void
g1tmpo1_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	M32C_IIO *iio = (M32C_IIO *) clientData;
	actualize_bt1(iio);
	iio->regG1PO[1] = value;
	if((iio->regG1POCR[1] & G1POCR_RLD) == 0) {
		iio->regG1POint[1] = value;
	}
	update_timeout_c1(iio,1);
}

static uint32_t
g1tmpo2_read(void *clientData,uint32_t address,int rqlen)
{
	M32C_IIO *iio = (M32C_IIO *) clientData;
	return iio->regG1PO[2];
}

static void
g1tmpo2_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	M32C_IIO *iio = (M32C_IIO *) clientData;
	actualize_bt1(iio);
	iio->regG1PO[2] = value;
	if((iio->regG1POCR[2] & G1POCR_RLD) == 0) {
		iio->regG1POint[2] = value;
	}
	update_timeout_c1(iio,2);
}

static uint32_t
g1tmpo3_read(void *clientData,uint32_t address,int rqlen)
{
	M32C_IIO *iio = (M32C_IIO *) clientData;
	return iio->regG1PO[3];
}

static void
g1tmpo3_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	M32C_IIO *iio = (M32C_IIO *) clientData;
	actualize_bt1(iio);
	iio->regG1PO[3] = value;
	if((iio->regG1POCR[3] & G1POCR_RLD) == 0) {
		iio->regG1POint[3] = value;
	}
	update_timeout_c1(iio,3);
}

static uint32_t
g1tmpo4_read(void *clientData,uint32_t address,int rqlen)
{
	M32C_IIO *iio = (M32C_IIO *) clientData;
	return iio->regG1PO[4];
}

static void
g1tmpo4_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	M32C_IIO *iio = (M32C_IIO *) clientData;
	actualize_bt1(iio);
	iio->regG1PO[4] = value;
	if((iio->regG1POCR[4] & G1POCR_RLD) == 0) {
		iio->regG1POint[4] = value;
	}
	update_timeout_c1(iio,4);
}

static uint32_t
g1tmpo5_read(void *clientData,uint32_t address,int rqlen)
{
	M32C_IIO *iio = (M32C_IIO *) clientData;
	return iio->regG1PO[5];
}

static void
g1tmpo5_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	M32C_IIO *iio = (M32C_IIO *) clientData;
	actualize_bt1(iio);
	iio->regG1PO[5] = value;
	if((iio->regG1POCR[5] & G1POCR_RLD) == 0) {
		iio->regG1POint[5] = value;
	}
	update_timeout_c1(iio,5);
}

static uint32_t
g1tmpo6_read(void *clientData,uint32_t address,int rqlen)
{
	M32C_IIO *iio = (M32C_IIO *) clientData;
	return iio->regG1PO[6];
}

static void
g1tmpo6_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	M32C_IIO *iio = (M32C_IIO *) clientData;
	actualize_bt1(iio);
	iio->regG1PO[6] = value;
	if((iio->regG1POCR[6] & G1POCR_RLD) == 0) {
		iio->regG1POint[6] = value;
	}
	update_timeout_c1(iio,6);
}

static uint32_t
g1tmpo7_read(void *clientData,uint32_t address,int rqlen)
{
	M32C_IIO *iio = (M32C_IIO *) clientData;
	return iio->regG1PO[7];
}

static void
g1tmpo7_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	M32C_IIO *iio = (M32C_IIO *) clientData;
	actualize_bt1(iio);
	iio->regG1PO[7] = value;
	if((iio->regG1POCR[7] & G1POCR_RLD) == 0) {
		iio->regG1POint[7] = value;
	}
	update_timeout_c1(iio,7);
}

static uint32_t
g1pocr0_read(void *clientData,uint32_t address,int rqlen)
{
	M32C_IIO *iio = (M32C_IIO *) clientData;
	return iio->regG1POCR[0];
}

static void
g1pocr0_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	M32C_IIO *iio = (M32C_IIO *) clientData;
	actualize_bt1(iio);
	iio->regG1POCR[0] = value;
	update_timeout_bt1(iio);
}

static uint32_t
g1pocr1_read(void *clientData,uint32_t address,int rqlen)
{
	M32C_IIO *iio = (M32C_IIO *) clientData;
	return iio->regG1POCR[1];
}

static void
g1pocr1_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	M32C_IIO *iio = (M32C_IIO *) clientData;
	//IIOChannel *ch = &iio->bt1Channel[0];
	actualize_bt1(iio);
	iio->regG1POCR[1] = value;
	update_timeout_bt1(iio);
}

static uint32_t
g1pocr2_read(void *clientData,uint32_t address,int rqlen)
{
	M32C_IIO *iio = (M32C_IIO *) clientData;
	return iio->regG1POCR[2];
}

static void
g1pocr2_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	M32C_IIO *iio = (M32C_IIO *) clientData;
	actualize_bt1(iio);
	iio->regG1POCR[2] = value;
	update_timeout_bt1(iio);
}

static uint32_t
g1pocr3_read(void *clientData,uint32_t address,int rqlen)
{
	M32C_IIO *iio = (M32C_IIO *) clientData;
	return iio->regG1POCR[3];
}

static void
g1pocr3_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	M32C_IIO *iio = (M32C_IIO *) clientData;
	actualize_bt1(iio);
	iio->regG1POCR[3] = value;
	update_timeout_bt1(iio);
}

static uint32_t
g1pocr4_read(void *clientData,uint32_t address,int rqlen)
{
	M32C_IIO *iio = (M32C_IIO *) clientData;
	return iio->regG1POCR[4];
}

static void
g1pocr4_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	M32C_IIO *iio = (M32C_IIO *) clientData;
	actualize_bt1(iio);
	iio->regG1POCR[4] = value;
	update_timeout_bt1(iio);
}

static uint32_t
g1pocr5_read(void *clientData,uint32_t address,int rqlen)
{
	M32C_IIO *iio = (M32C_IIO *) clientData;
	return iio->regG1POCR[5];
}

static void
g1pocr5_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	M32C_IIO *iio = (M32C_IIO *) clientData;
	actualize_bt1(iio);
	iio->regG1POCR[5] = value;
	update_timeout_bt1(iio);
}

static uint32_t
g1pocr6_read(void *clientData,uint32_t address,int rqlen)
{
	M32C_IIO *iio = (M32C_IIO *) clientData;
	return iio->regG1POCR[6];
}

static void
g1pocr6_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	M32C_IIO *iio = (M32C_IIO *) clientData;
	actualize_bt1(iio);
	iio->regG1POCR[6] = value;
	update_timeout_bt1(iio);
}

static uint32_t
g1pocr7_read(void *clientData,uint32_t address,int rqlen)
{
	M32C_IIO *iio = (M32C_IIO *) clientData;
	return iio->regG1POCR[7];
}

static void
g1pocr7_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	M32C_IIO *iio = (M32C_IIO *) clientData;
	actualize_bt1(iio);
	iio->regG1POCR[7] = value;
	update_timeout_bt1(iio);
}

static uint32_t
g1fs_read(void *clientData,uint32_t address,int rqlen)
{
	M32C_IIO *iio = clientData;
	return iio->regG1FS;
}

static void
g1fs_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	M32C_IIO *iio = clientData;
	iio->regG1FS = value;
	if(value) {
		fprintf(stderr,"IIO: Warning, g1fs not implemented\n");
	}
}

static uint32_t
g1fe_read(void *clientData,uint32_t address,int rqlen)
{
	M32C_IIO *iio = clientData;
	return iio->regG1FE;
}

static void
g1fe_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	M32C_IIO *iio = clientData;
	iio->regG1FE = value;
	update_timeout_bt1(iio);
}

static uint32_t
g2bt_read(void *clientData,uint32_t address,int rqlen)
{
	M32C_IIO *iio = clientData;
	actualize_bt2(iio);
	return iio->regG2BT;
}

static void
g2bt_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	M32C_IIO *iio = clientData;
	actualize_bt2(iio);
	iio->regG2BT = value;
	update_timeout_bt2(iio);
}

static uint32_t
g2bcr0_read(void *clientData,uint32_t address,int rqlen)
{
	M32C_IIO *iio = clientData;
	return iio->regG2BCR0;
}

static void
g2bcr0_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	M32C_IIO *iio = clientData;
	iio->regG2BCR0 = value;
	update_bt2_clock(iio);
}

static uint32_t
g2bcr1_read(void *clientData,uint32_t address,int rqlen)
{

	M32C_IIO *iio = clientData;
	return iio->regG2BCR1;
}

static void
g2bcr1_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	M32C_IIO *iio = clientData;
	actualize_bt2(iio);
	iio->regG2BCR1 = value;
	update_timeout_bt2(iio);
}

static uint32_t
g2pocr0_read(void *clientData,uint32_t address,int rqlen)
{
	M32C_IIO *iio = clientData;
	return iio->regG2POCR[0];
}

static void
g2pocr0_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	M32C_IIO *iio = clientData;
	actualize_bt2(iio);
	iio->regG2POCR[0] = value;
	update_timeout_bt2(iio);
}

static uint32_t
g2pocr1_read(void *clientData,uint32_t address,int rqlen)
{
	M32C_IIO *iio = clientData;
	return iio->regG2POCR[1];
}

static void
g2pocr1_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	M32C_IIO *iio = clientData;
	actualize_bt2(iio);
	iio->regG2POCR[1] = value;
	update_timeout_bt2(iio);
}

static uint32_t
g2pocr2_read(void *clientData,uint32_t address,int rqlen)
{
	M32C_IIO *iio = clientData;
	return iio->regG2POCR[2];
}

static void
g2pocr2_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	M32C_IIO *iio = clientData;
	actualize_bt2(iio);
	iio->regG2POCR[2] = value;
	update_timeout_bt2(iio);
}

static uint32_t
g2pocr3_read(void *clientData,uint32_t address,int rqlen)
{
	M32C_IIO *iio = clientData;
	return iio->regG2POCR[3];
}

static void
g2pocr3_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	M32C_IIO *iio = clientData;
	actualize_bt2(iio);
	iio->regG2POCR[3] = value;
	update_timeout_bt2(iio);
}

static uint32_t
g2pocr4_read(void *clientData,uint32_t address,int rqlen)
{
	M32C_IIO *iio = clientData;
	return iio->regG2POCR[4];
}

static void
g2pocr4_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	M32C_IIO *iio = clientData;
	actualize_bt2(iio);
	iio->regG2POCR[4] = value;
	update_timeout_bt2(iio);
}

static uint32_t
g2pocr5_read(void *clientData,uint32_t address,int rqlen)
{
	M32C_IIO *iio = clientData;
	return iio->regG2POCR[5];
}

static void
g2pocr5_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	M32C_IIO *iio = clientData;
	actualize_bt2(iio);
	iio->regG2POCR[5] = value;
	update_timeout_bt2(iio);
}

static uint32_t
g2pocr6_read(void *clientData,uint32_t address,int rqlen)
{
	M32C_IIO *iio = clientData;
	return iio->regG2POCR[6];
}

static void
g2pocr6_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	M32C_IIO *iio = clientData;
	actualize_bt2(iio);
	iio->regG2POCR[6] = value;
	update_timeout_bt2(iio);
}

static uint32_t
g2pocr7_read(void *clientData,uint32_t address,int rqlen)
{
	M32C_IIO *iio = clientData;
	return iio->regG2POCR[7];
}

static void
g2pocr7_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	M32C_IIO *iio = clientData;
	actualize_bt2(iio);
	iio->regG2POCR[7] = value;
	update_timeout_bt2(iio);
}

static uint32_t
g2po0_read(void *clientData,uint32_t address,int rqlen)
{
	M32C_IIO *iio = (M32C_IIO *) clientData;
	return iio->regG2PO[0];
}

static void
g2po0_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	M32C_IIO *iio = (M32C_IIO *) clientData;
	actualize_bt2(iio);
	iio->regG2PO[0] = value;
	if((iio->regG2POCR[0] & G2POCR_RLD) == 0) {
		iio->regG2POint[0] = value;
	} 
	update_timeout_c2(iio,0);
}

static uint32_t
g2po1_read(void *clientData,uint32_t address,int rqlen)
{
	M32C_IIO *iio = (M32C_IIO *) clientData;
	return iio->regG2PO[1];
}

static void
g2po1_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	M32C_IIO *iio = (M32C_IIO *) clientData;
	actualize_bt2(iio);
	iio->regG2PO[1] = value;
	if((iio->regG2POCR[1] & G2POCR_RLD) == 0) {
		iio->regG2POint[1] = value;
	} 
	update_timeout_c2(iio,1);
}

static uint32_t
g2po2_read(void *clientData,uint32_t address,int rqlen)
{
	M32C_IIO *iio = (M32C_IIO *) clientData;
	return iio->regG2PO[2];
}

static void
g2po2_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	M32C_IIO *iio = (M32C_IIO *) clientData;
	actualize_bt2(iio);
	iio->regG2PO[2] = value;
	if((iio->regG2POCR[2] & G2POCR_RLD) == 0) {
		iio->regG2POint[2] = value;
	} 
	update_timeout_c2(iio,2);
}

static uint32_t
g2po3_read(void *clientData,uint32_t address,int rqlen)
{
	M32C_IIO *iio = (M32C_IIO *) clientData;
	return iio->regG2PO[3];
}

static void
g2po3_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	M32C_IIO *iio = (M32C_IIO *) clientData;
	actualize_bt2(iio);
	iio->regG2PO[3] = value;
	if((iio->regG2POCR[3] & G2POCR_RLD) == 0) {
		iio->regG2POint[3] = value;
	} 
	update_timeout_c2(iio,3);
}

static uint32_t
g2po4_read(void *clientData,uint32_t address,int rqlen)
{
	M32C_IIO *iio = (M32C_IIO *) clientData;
	return iio->regG2PO[4];
}

static void
g2po4_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	M32C_IIO *iio = (M32C_IIO *) clientData;
	actualize_bt2(iio);
	iio->regG2PO[4] = value;
	if((iio->regG2POCR[4] & G2POCR_RLD) == 0) {
		iio->regG2POint[4] = value;
	} 
	update_timeout_c2(iio,4);
}

static uint32_t
g2po5_read(void *clientData,uint32_t address,int rqlen)
{
	M32C_IIO *iio = (M32C_IIO *) clientData;
	return iio->regG2PO[5];
}

static void
g2po5_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	M32C_IIO *iio = (M32C_IIO *) clientData;
	actualize_bt2(iio);
	iio->regG2PO[5] = value;
	if((iio->regG2POCR[5] & G2POCR_RLD) == 0) {
		iio->regG2POint[5] = value;
	} 
	update_timeout_c2(iio,5);
}

static uint32_t
g2po6_read(void *clientData,uint32_t address,int rqlen)
{
	M32C_IIO *iio = (M32C_IIO *) clientData;
	return iio->regG2PO[6];
}

static void
g2po6_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	M32C_IIO *iio = (M32C_IIO *) clientData;
	actualize_bt2(iio);
	iio->regG2PO[6] = value;
	if((iio->regG2POCR[6] & G2POCR_RLD) == 0) {
		iio->regG2POint[6] = value;
	} 
	update_timeout_c2(iio,6);
}

static uint32_t
g2po7_read(void *clientData,uint32_t address,int rqlen)
{
	M32C_IIO *iio = (M32C_IIO *) clientData;
	return iio->regG2PO[7];
}

static void
g2po7_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	M32C_IIO *iio = (M32C_IIO *) clientData;
	actualize_bt2(iio);
	iio->regG2PO[7] = value;
	if((iio->regG2POCR[7] & G2POCR_RLD) == 0) {
		iio->regG2POint[7] = value;
	} 
	update_timeout_c2(iio,7);
}

static uint32_t
g2rtp_read(void *clientData,uint32_t address,int rqlen)
{
	 return 0;
}

static void
g2rtp_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
}

static uint32_t
g2fe_read(void *clientData,uint32_t address,int rqlen)
{
	M32C_IIO *iio = clientData;
	return iio->regG2FE;
}

static void
g2fe_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	M32C_IIO *iio = clientData;
	iio->regG2FE = value;
	update_timeout_bt2(iio);
}

static uint32_t
btsr_read(void *clientData,uint32_t address,int rqlen)
{
	M32C_IIO *iio = clientData;
	return iio->regBTSR | 0xf0;
}

static void
btsr_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	M32C_IIO *iio = clientData;
	actualize_bt1(iio);
	actualize_bt2(iio);
	iio->regBTSR = value & (BTSR_BT1S | BTSR_BT2S);
	update_timeout_bt1(iio);
	update_timeout_bt2(iio);
	if(value & 0xf0) {
		fprintf(stderr,"IIO Error: Upper bits of REG_BTSR not 0: 0x%02x\n",value);
	}
}

static void
M32CIIO_Map(void *owner,uint32_t base,uint32_t mask,uint32_t mapflags)
{
	M32C_IIO *iio = (M32C_IIO *) owner;
	IOH_New8(REG_IIO0IR,iio0ir_read,iio0ir_write,iio);
	IOH_New8(REG_IIO1IR,iio1ir_read,iio1ir_write,iio);
	IOH_New8(REG_IIO2IR,iio2ir_read,iio2ir_write,iio);
	IOH_New8(REG_IIO3IR,iio3ir_read,iio3ir_write,iio);
	IOH_New8(REG_IIO4IR,iio4ir_read,iio4ir_write,iio);
	IOH_New8(REG_IIO5IR,iio5ir_read,iio5ir_write,iio);
	IOH_New8(REG_IIO6IR,iio6ir_read,iio6ir_write,iio);
	IOH_New8(REG_IIO7IR,iio7ir_read,iio7ir_write,iio);
	IOH_New8(REG_IIO8IR,iio8ir_read,iio8ir_write,iio);
	IOH_New8(REG_IIO9IR,iio9ir_read,iio9ir_write,iio);
	IOH_New8(REG_IIO10IR,iio10ir_read,iio10ir_write,iio);
	IOH_New8(REG_IIO11IR,iio11ir_read,iio11ir_write,iio);
	IOH_New8(REG_IIO0IE,iioXie_read,iioXie_write,iio);
	IOH_New8(REG_IIO1IE,iioXie_read,iioXie_write,iio);
	IOH_New8(REG_IIO2IE,iioXie_read,iioXie_write,iio);
	IOH_New8(REG_IIO3IE,iioXie_read,iioXie_write,iio);
	IOH_New8(REG_IIO4IE,iioXie_read,iioXie_write,iio);
	IOH_New8(REG_IIO5IE,iioXie_read,iioXie_write,iio);
	IOH_New8(REG_IIO6IE,iioXie_read,iioXie_write,iio);
	IOH_New8(REG_IIO7IE,iioXie_read,iioXie_write,iio);
	IOH_New8(REG_IIO8IE,iioXie_read,iioXie_write,iio);
	IOH_New8(REG_IIO9IE,iioXie_read,iioXie_write,iio);
	IOH_New8(REG_IIO10IE,iioXie_read,iioXie_write,iio);
	IOH_New8(REG_IIO11IE,iioXie_read,iioXie_write,iio);

	IOH_New16(REG_G1BT,g1bt_read,g1bt_write,iio);
	IOH_New8(REG_G1BCR0,g1bcr0_read,g1bcr0_write,iio);
	IOH_New8(REG_G1BCR1,g1bcr1_read,g1bcr1_write,iio);
	IOH_New8(REG_G1TMCR0,g1tmcr0_read,g1tmcr0_write,iio);
	IOH_New8(REG_G1TMCR1,g1tmcr1_read,g1tmcr1_write,iio);
	IOH_New8(REG_G1TMCR2,g1tmcr2_read,g1tmcr2_write,iio);
	IOH_New8(REG_G1TMCR3,g1tmcr3_read,g1tmcr3_write,iio);
	IOH_New8(REG_G1TMCR4,g1tmcr4_read,g1tmcr4_write,iio);
	IOH_New8(REG_G1TMCR5,g1tmcr5_read,g1tmcr5_write,iio);
	IOH_New8(REG_G1TMCR6,g1tmcr6_read,g1tmcr6_write,iio);
	IOH_New8(REG_G1TMCR7,g1tmcr7_read,g1tmcr7_write,iio);
	IOH_New8(REG_G1TPR6,g1tpr6_read,g1tpr6_write,iio);
	IOH_New8(REG_G1TPR7,g1tpr7_read,g1tpr7_write,iio);
	IOH_New16(REG_G1TM0,g1tmpo0_read,g1tmpo0_write,iio);
	IOH_New16(REG_G1TM1,g1tmpo1_read,g1tmpo1_write,iio);
	IOH_New16(REG_G1TM2,g1tmpo2_read,g1tmpo2_write,iio);
	IOH_New16(REG_G1TM3,g1tmpo3_read,g1tmpo3_write,iio);
	IOH_New16(REG_G1TM4,g1tmpo4_read,g1tmpo4_write,iio);
	IOH_New16(REG_G1TM5,g1tmpo5_read,g1tmpo5_write,iio);
	IOH_New16(REG_G1TM6,g1tmpo6_read,g1tmpo6_write,iio);
	IOH_New16(REG_G1TM7,g1tmpo7_read,g1tmpo7_write,iio);
	IOH_New8(REG_G1POCR0,g1pocr0_read,g1pocr0_write,iio);
	IOH_New8(REG_G1POCR1,g1pocr1_read,g1pocr1_write,iio);
	IOH_New8(REG_G1POCR2,g1pocr2_read,g1pocr2_write,iio);
	IOH_New8(REG_G1POCR3,g1pocr3_read,g1pocr3_write,iio);
	IOH_New8(REG_G1POCR4,g1pocr4_read,g1pocr4_write,iio);
	IOH_New8(REG_G1POCR5,g1pocr5_read,g1pocr5_write,iio);
	IOH_New8(REG_G1POCR6,g1pocr6_read,g1pocr6_write,iio);
	IOH_New8(REG_G1POCR7,g1pocr7_read,g1pocr7_write,iio);
	IOH_New8(REG_G1FS,g1fs_read,g1fs_write,iio);
	IOH_New8(REG_G1FE,g1fe_read,g1fe_write,iio);
	IOH_New16(REG_G2BT,g2bt_read,g2bt_write,iio);
	IOH_New8(REG_G2BCR0,g2bcr0_read,g2bcr0_write,iio);
	IOH_New8(REG_G2BCR1,g2bcr1_read,g2bcr1_write,iio);
	IOH_New8(REG_G2POCR0,g2pocr0_read,g2pocr0_write,iio);
	IOH_New8(REG_G2POCR1,g2pocr1_read,g2pocr1_write,iio);
	IOH_New8(REG_G2POCR2,g2pocr2_read,g2pocr2_write,iio);
	IOH_New8(REG_G2POCR3,g2pocr3_read,g2pocr3_write,iio);
	IOH_New8(REG_G2POCR4,g2pocr4_read,g2pocr4_write,iio);
	IOH_New8(REG_G2POCR5,g2pocr5_read,g2pocr5_write,iio);
	IOH_New8(REG_G2POCR6,g2pocr6_read,g2pocr6_write,iio);
	IOH_New8(REG_G2POCR7,g2pocr7_read,g2pocr7_write,iio);
	IOH_New16(REG_G2PO0,g2po0_read,g2po0_write,iio);
	IOH_New16(REG_G2PO1,g2po1_read,g2po1_write,iio);
	IOH_New16(REG_G2PO2,g2po2_read,g2po2_write,iio);
	IOH_New16(REG_G2PO3,g2po3_read,g2po3_write,iio);
	IOH_New16(REG_G2PO4,g2po4_read,g2po4_write,iio);
	IOH_New16(REG_G2PO5,g2po5_read,g2po5_write,iio);
	IOH_New16(REG_G2PO6,g2po6_read,g2po6_write,iio);
	IOH_New16(REG_G2PO7,g2po7_read,g2po7_write,iio);
	IOH_New8(REG_G2RTP,g2rtp_read,g2rtp_write,iio);
	IOH_New8(REG_G2FE,g2fe_read,g2fe_write,iio);
	IOH_New8(REG_BTSR,btsr_read,btsr_write,iio);
}

static void
M32CIIO_Unmap(void *owner,uint32_t base,uint32_t mask)
{
	IOH_Delete8(REG_IIO0IR);
	IOH_Delete8(REG_IIO1IR);
	IOH_Delete8(REG_IIO2IR);
	IOH_Delete8(REG_IIO3IR);
	IOH_Delete8(REG_IIO4IR);
	IOH_Delete8(REG_IIO5IR);
	IOH_Delete8(REG_IIO6IR);
	IOH_Delete8(REG_IIO7IR);
	IOH_Delete8(REG_IIO8IR);
	IOH_Delete8(REG_IIO9IR);
	IOH_Delete8(REG_IIO10IR);
	IOH_Delete8(REG_IIO11IR);
	IOH_Delete8(REG_IIO0IE);
	IOH_Delete8(REG_IIO1IE);
	IOH_Delete8(REG_IIO2IE);
	IOH_Delete8(REG_IIO3IE);
	IOH_Delete8(REG_IIO4IE);
	IOH_Delete8(REG_IIO5IE);
	IOH_Delete8(REG_IIO6IE);
	IOH_Delete8(REG_IIO7IE);
	IOH_Delete8(REG_IIO8IE);
	IOH_Delete8(REG_IIO9IE);
	IOH_Delete8(REG_IIO10IE);
	IOH_Delete8(REG_IIO11IE);

	IOH_Delete16(REG_G1BT);
	IOH_Delete8(REG_G1BCR0);
	IOH_Delete8(REG_G1BCR1);
	IOH_Delete8(REG_G1TMCR0);
	IOH_Delete8(REG_G1TMCR1);
	IOH_Delete8(REG_G1TMCR2);
	IOH_Delete8(REG_G1TMCR3);
	IOH_Delete8(REG_G1TMCR4);
	IOH_Delete8(REG_G1TMCR5);
	IOH_Delete8(REG_G1TMCR6);
	IOH_Delete8(REG_G1TMCR7);
	IOH_Delete8(REG_G1TPR6);
	IOH_Delete8(REG_G1TPR7);
	IOH_Delete16(REG_G1TM0);
	IOH_Delete16(REG_G1TM1);
	IOH_Delete16(REG_G1TM2);
	IOH_Delete16(REG_G1TM3);
	IOH_Delete16(REG_G1TM4);
	IOH_Delete16(REG_G1TM5);
	IOH_Delete16(REG_G1TM6);
	IOH_Delete16(REG_G1TM7);
	IOH_Delete8(REG_G1POCR0);
	IOH_Delete8(REG_G1POCR1);
	IOH_Delete8(REG_G1POCR2);
	IOH_Delete8(REG_G1POCR3);
	IOH_Delete8(REG_G1POCR4);
	IOH_Delete8(REG_G1POCR5);
	IOH_Delete8(REG_G1POCR6);
	IOH_Delete8(REG_G1POCR7);
	IOH_Delete8(REG_G1FS);
	IOH_Delete8(REG_G1FE);
	IOH_Delete16(REG_G2BT);
	IOH_Delete8(REG_G2BCR0);
	IOH_Delete8(REG_G2BCR1);
	IOH_Delete8(REG_G2POCR0);
	IOH_Delete8(REG_G2POCR1);
	IOH_Delete8(REG_G2POCR2);
	IOH_Delete8(REG_G2POCR3);
	IOH_Delete8(REG_G2POCR4);
	IOH_Delete8(REG_G2POCR5);
	IOH_Delete8(REG_G2POCR6);
	IOH_Delete8(REG_G2POCR7);
	IOH_Delete16(REG_G2PO0);
	IOH_Delete16(REG_G2PO1);
	IOH_Delete16(REG_G2PO2);
	IOH_Delete16(REG_G2PO3);
	IOH_Delete16(REG_G2PO4);
	IOH_Delete16(REG_G2PO5);
	IOH_Delete16(REG_G2PO6);
	IOH_Delete16(REG_G2PO7);
	IOH_Delete8(REG_G2RTP);
	IOH_Delete8(REG_G2FE);
	IOH_Delete8(REG_BTSR);
}


BusDevice *
M32CIIO_New(const char *name) 
{
	M32C_IIO *iio;
	int i;

	iio = sg_new(M32C_IIO);

	/* IIO0 Group */
	iio->irqCan10 = SigNode_New("%s.irqCan10",name);
	iio->irqU5r = SigNode_New("%s.irqU5r",name);
	iio->irqSio0r = SigNode_New("%s.irqSio0r",name);
	iio->irqG0ri = SigNode_New("%s.irqG0Ri",name);
	iio->irqTm13 = SigNode_New("%s.irqTm13",name);
	SigNode_Trace(iio->irqCan10,IIO0_Update,iio);
	SigNode_Trace(iio->irqU5r,IIO0_Update,iio);
	SigNode_Trace(iio->irqSio0r,IIO0_Update,iio);
	SigNode_Trace(iio->irqG0ri,IIO0_Update,iio);
	SigNode_Trace(iio->irqTm13,IIO0_Update,iio);
	
	iio->irqCan11 = SigNode_New("%s.irqCan11",name);
	iio->irqU5t = SigNode_New("%s.irqU5t",name);
	iio->irqSio0t = SigNode_New("%s.irqSio0t",name);
	iio->irqG0to = SigNode_New("%s.irqG0to",name);
	iio->irqTm14 = SigNode_New("%s.irqTm14",name);
	SigNode_Trace(iio->irqCan11,IIO1_Update,iio);
	SigNode_Trace(iio->irqU5t,IIO1_Update,iio);
	SigNode_Trace(iio->irqSio0t,IIO1_Update,iio);
	SigNode_Trace(iio->irqG0to,IIO1_Update,iio);
	SigNode_Trace(iio->irqTm14,IIO1_Update,iio);

	iio->irqSio1r = SigNode_New("%s.irqSio1r",name);
	iio->irqG1ri = SigNode_New("%s.irqG1ri",name);
	iio->irqTm12 = SigNode_New("%s.irqTm12",name);
	SigNode_Trace(iio->irqSio1r,IIO2_Update,iio);
	SigNode_Trace(iio->irqG1ri,IIO2_Update,iio);
	SigNode_Trace(iio->irqTm12,IIO2_Update,iio);

	iio->irqSio1t = SigNode_New("%s.irqSio1t",name);
	iio->irqG1To = SigNode_New("%s.irqG1To",name);
	iio->irqPo27 = SigNode_New("%s.irqPo27",name);	
	iio->irqTm10 = SigNode_New("%s.irqTm10",name);
	SigNode_Trace(iio->irqSio1t,IIO3_Update,iio);
	SigNode_Trace(iio->irqG1To,IIO3_Update,iio);
	SigNode_Trace(iio->irqPo27,IIO3_Update,iio);
	SigNode_Trace(iio->irqTm10,IIO3_Update,iio);

	iio->irqSrt0 = SigNode_New("%s.irqSrt0",name);
	iio->irqSrt1 = SigNode_New("%s.irqSrt1",name);
	iio->irqBt1 = SigNode_New("%s.irqBt1",name);
	iio->irqTm17 = SigNode_New("%s.irqTm17",name);
	SigNode_Trace(iio->irqSrt0,IIO4_Update,iio);
	SigNode_Trace(iio->irqSrt1,IIO4_Update,iio);
	SigNode_Trace(iio->irqBt1,IIO4_Update,iio);
	SigNode_Trace(iio->irqTm17,IIO4_Update,iio);

	iio->irqCan12 = SigNode_New("%s.irqCan12",name);
	iio->irqCan1wu = SigNode_New("%s.irqCan1wu",name);
	iio->irqSio2r = SigNode_New("%s.irqSio2r",name);
	iio->irqPo21 = SigNode_New("%s.irqPo21",name);
	SigNode_Trace(iio->irqCan12,IIO5_Update,iio);
	SigNode_Trace(iio->irqCan1wu,IIO5_Update,iio);
	SigNode_Trace(iio->irqSio2r,IIO5_Update,iio);
	SigNode_Trace(iio->irqPo21,IIO5_Update,iio);

	iio->irqSio2t = SigNode_New("%s.irqSio2t",name);
	iio->irqPo20 = SigNode_New("%s.irqPo20",name);
	SigNode_Trace(iio->irqSio2t,IIO6_Update,iio);
	SigNode_Trace(iio->irqPo20,IIO6_Update,iio);

	iio->irqIe0 = SigNode_New("%s.irqIe0",name);
	iio->irqPo22 = SigNode_New("%s.irqPo22",name);
	SigNode_Trace(iio->irqIe0,IIO7_Update,iio);
	SigNode_Trace(iio->irqPo22,IIO7_Update,iio);

	iio->irqIe1 = SigNode_New("%s.irqIe1",name);
	iio->irqIe2 = SigNode_New("%s.irqIe2",name);
	iio->irqBt2 = SigNode_New("%s.irqBt2",name);
	iio->irqPo23 = SigNode_New("%s.irqPo23",name);
	iio->irqTm11 = SigNode_New("%s.irqTm11",name);
	SigNode_Trace(iio->irqIe1,IIO8_Update,iio);
	SigNode_Trace(iio->irqIe2,IIO8_Update,iio);
	SigNode_Trace(iio->irqBt2,IIO8_Update,iio);
	SigNode_Trace(iio->irqPo23,IIO8_Update,iio);
	SigNode_Trace(iio->irqTm11,IIO8_Update,iio);

	iio->irqCan00 = SigNode_New("%s.irqCan00",name);
	iio->irqInt6 = SigNode_New("%s.irqInt6",name);
	iio->irqU6r = SigNode_New("%s.irqU6r",name);
	iio->irqPo24 = SigNode_New("%s.irqPo24",name);
	iio->irqTm15 = SigNode_New("%s.irqTm15",name);
	SigNode_Trace(iio->irqCan00,IIO9_Update,iio);
	SigNode_Trace(iio->irqInt6,IIO9_Update,iio);
	SigNode_Trace(iio->irqU6r,IIO9_Update,iio);
	SigNode_Trace(iio->irqPo24,IIO9_Update,iio);
	SigNode_Trace(iio->irqTm15,IIO9_Update,iio);

	iio->irqCan01 = SigNode_New("%s.irqCan01",name);
	iio->irqInt7 = SigNode_New("%s.irqInt7",name);
	iio->irqU6t = SigNode_New("%s.irqU6t",name);
	iio->irqPo25 = SigNode_New("%s.irqPo25",name);
	iio->irqTm16 = SigNode_New("%s.irqTm16",name);
	SigNode_Trace(iio->irqCan01,IIO10_Update,iio);
	SigNode_Trace(iio->irqInt7,IIO10_Update,iio);
	SigNode_Trace(iio->irqU6t,IIO10_Update,iio);
	SigNode_Trace(iio->irqPo25,IIO10_Update,iio);
	SigNode_Trace(iio->irqTm16,IIO10_Update,iio);

	iio->irqCan02 = SigNode_New("%s.irqCan02",name);
	iio->irqInt8 = SigNode_New("%s.irqInt8",name);
	iio->irqPo26 = SigNode_New("%s.irqPo26",name);
	SigNode_Trace(iio->irqCan02,IIO11_Update,iio);
	SigNode_Trace(iio->irqInt8,IIO11_Update,iio);
	SigNode_Trace(iio->irqPo26,IIO11_Update,iio);

	for(i = 0; i < 12; i++) {
		iio->irqIIO[i] = SigNode_New("%s.irqIIO%d",name,i);	
		if(!iio->irqIIO[i]) {
			fprintf(stderr,"Can not create signal line for IIO irq\n");
			exit(1);
		}	
		SigNode_Set(iio->irqIIO[i],SIG_HIGH);
	}
	for(i = 0; i < 8; i++) {
		IIOChannel *bt1ch = &iio->bt1Channel[i];
		IIOChannel *bt2ch = &iio->bt2Channel[i];
		bt1ch->iio = bt2ch->iio = iio;
		bt1ch->idx = bt2ch->idx = i;
		bt1ch->sigOut = SigNode_New("%s.outC1_%d",name,i);
		bt2ch->sigOut = SigNode_New("%s.outC2_%d",name,i);
		bt1ch->activeLvl = bt2ch->activeLvl = SIG_HIGH;
		bt1ch->inactiveLvl = bt2ch->inactiveLvl = SIG_LOW;
		if((bt1ch->sigOut == NULL)  || (bt2ch->sigOut == NULL)) {
			fprintf(stderr,"Can not create signal line for sigOut\n");
			exit(1);
		}
		CycleTimer_Init(&bt1ch->pwmEventTimer,pwm_event_bt1,bt1ch);
		CycleTimer_Init(&bt2ch->pwmEventTimer,pwm_event_bt2,bt2ch);
	}
	iio->bdev.first_mapping = NULL;
        iio->bdev.Map = M32CIIO_Map;
        iio->bdev.UnMap = M32CIIO_Unmap;
        iio->bdev.owner = iio;
        iio->bdev.hw_flags = MEM_FLAG_WRITABLE | MEM_FLAG_READABLE;
	iio->clkIn = Clock_New("%s.clk",name);
	iio->clkBT1 = Clock_New("%s.clkBT1",name);	
	iio->clkBT2 = Clock_New("%s.clkBT2",name);	
	if(!iio->clkIn || !iio->clkBT1 || !iio->clkBT2) {
		fprintf(stderr,"Can not create clock for IIO module\n");
		exit(1);
	}
	fprintf(stderr,"Created M32C Intelligent IO Module (IIO)\n");
        return &iio->bdev;
}
