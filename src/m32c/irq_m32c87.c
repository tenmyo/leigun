/* 
 ***************************************************************************************************
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
 ***************************************************************************************************
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "signode.h"
#include "sgstring.h"
#include "bus.h"
#include "cpu_m32c.h"
#include "irq_m32c87.h"
#include "cycletimer.h"

#define REG_TA0IC	0x06c
#define REG_TA1IC	0x08c
#define REG_TA2IC	0x06e
#define REG_TA3IC	0x08e
#define REG_TA4IC	0x070

#define REG_TB0IC	0x094
#define REG_TB1IC	0x076
#define REG_TB2IC	0x096
#define REG_TB3IC	0x078
#define REG_TB4IC	0x098
#define REG_TB5IC	0x069

#define REG_S0TIC	0x090
#define REG_S1TIC	0x092
#define REG_S2TIC	0x089
#define REG_S3TIC	0x08b
#define REG_S4TIC	0x08d

#define REG_S0RIC	0x072
#define REG_S1RIC	0x074
#define REG_S2RIC	0x06b
#define REG_S3RIC	0x06d
#define REG_S4RIC	0x06f

#define REG_BCN0IC	0x071
#define REG_BCN1IC	0x091
#define REG_BCN2IC	0x08f
#define REG_BCN3IC	0x071
#define REG_BCN4IC	0x092

#define REG_DM0IC	0x068
#define REG_DM1IC	0x088
#define REG_DM2IC	0x06a
#define REG_DM3IC	0x08a

#define REG_AD0IC	0x073
#define REG_KUPIC	0x093

#define	REG_IIO0IC	0x075
#define	REG_IIO1IC	0x095
#define	REG_IIO2IC	0x077
#define	REG_IIO3IC	0x097
#define	REG_IIO4IC	0x079
#define	REG_IIO5IC	0x099

#define REG_IIO6IC	0x07b
#define REG_IIO7IC	0x09b
#define REG_IIO8IC	0x07d
#define REG_IIO9IC	0x09d
#define REG_IIO10IC	0x07f
#define REG_IIO11IC	0x081

#define REG_CAN0IC	0x09d
#define	REG_CAN1IC	0x07f
#define	REG_CAN2IC	0x081
#define	REG_CAN3IC	0x075
#define	REG_CAN4IC	0x095
#define	REG_CAN5IC	0x099

/* Interrupt lines with polarity and Level/Edge switching */

#define REG_INT0IC	0x9e
#define REG_INT1IC	0x7e
#define REG_INT2IC	0x9c
#define REG_INT3IC	0x7c
#define REG_INT4IC	0x9a
#define REG_INT5IC	0x7a

#define REG_IFSRA	0x31e
#define REG_IFSR	0x31f

#define	ICR_ILVL_MSK	(7)
#define ICR_IR		(1 << 3)
#define ICR_POL		(1 << 4)
#define ICR_LVS		(1 << 5)

typedef struct M32C_IntCo M32C_IntCo; 

typedef struct M32C_Irq {
	M32C_IntCo *intco;
	SigNode *irqInput;
	SigTrace *irqTrace;
	int ifsr_bit;
	int int_nr;
	uint8_t reg_icr;
} M32C_Irq;

struct M32C_IntCo {
	BusDevice bdev;
	uint8_t regIfsr;
	uint8_t regIfsra;
	M32C_Irq irq[64];
};

static void
update_interrupt(M32C_IntCo *intco) 
{
	int max_ilvl = 0;
	int ilvl;
	int i;
	int int_no = -1;
	M32C_Irq *irq;
	for(i = 0; i < 64; i ++) {
		irq = &intco->irq[i];
		if((irq->reg_icr & ICR_IR) == 0) {
			continue;
		}
		ilvl = irq->reg_icr & ICR_ILVL_MSK;
		if(ilvl > max_ilvl) {
			max_ilvl = ilvl;
			int_no = irq->int_nr;
		}
	}
	//fprintf(stderr,"Maxlevel by %d at %llu\n",int_no,CycleCounter_Get());
	#if 0
	if(int_no == 31) {
	 	fprintf(stderr,"Maxlevel by %d at %llu\n",int_no,CycleCounter_Get());
	}
	#endif
	M32C_PostILevel(max_ilvl,int_no);	
#if 0
	if(max_ilvl > 0) {
		fprintf(stderr,"Poste einen Ilevel von %d, int_no %d\n",max_ilvl,int_no);
		exit(1);
	}
#endif
}

void
M32C_AckIrq(BusDevice *intco_bdev,unsigned int intno) 
{
	M32C_IntCo *intco = (M32C_IntCo *) intco_bdev;
	M32C_Irq *irq;
#if 0
	if(intno == 21) {
		fprintf(stderr,"Ack intno %d at %llu\n",intno,CyclesToMilliseconds(CycleCounter_Get()));
	}
#endif
#if 0
	if(intno == 54) {
		fprintf(stderr,"Ack intno %d at %llu\n",intno,CycleCounter_Get());
	}
#endif
	if(intno < 64) {
		irq = &intco->irq[intno];
		irq->reg_icr &=  ~ICR_IR;
		update_interrupt(intco); 
	}
}

static uint32_t
icr_read(void *clientData,uint32_t address,int rqlen)
{
	M32C_Irq *irq = (M32C_Irq *) clientData;
#if 0
	static uint8_t last;
	if(irq->reg_icr != last) {
		fprintf(stderr,"read ICR: %02x\n",irq->reg_icr);
		last = irq->reg_icr;
	}
#endif
        return irq->reg_icr;
}

static void
icr_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	M32C_Irq *irq = (M32C_Irq *) clientData;
	uint8_t diff = irq->reg_icr;
	irq->reg_icr = (irq->reg_icr & value & ICR_IR) | (value & ~ICR_IR);
	diff ^= irq->reg_icr;
	if(diff & ICR_IR) {
		update_interrupt(irq->intco);
	}	
}


/**
 ************************************************************
 * \fn static void update_level_interrupt(M32C_Irq *irq); 
 ************************************************************
 */
static void
update_level_interrupt(M32C_Irq *irq) {
	uint8_t value = irq->reg_icr;
	if(value & ICR_LVS) {
		if(value & ICR_POL) {
			if(SigNode_Val(irq->irqInput) == SIG_HIGH) {
				irq->reg_icr |= ICR_IR;
			} else {
				irq->reg_icr &= ~ICR_IR;
			}
		} else {
			if(SigNode_Val(irq->irqInput) == SIG_LOW) {
				irq->reg_icr |= ICR_IR;
			} else {
				irq->reg_icr &= ~ICR_IR;
			}
		}
		update_interrupt(irq->intco);
	}
}

static uint32_t
exticr_read(void *clientData,uint32_t address,int rqlen)
{
	M32C_Irq *irq = (M32C_Irq *) clientData;
        return irq->reg_icr;
}

static void
exticr_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	M32C_Irq *irq = (M32C_Irq *) clientData;
        irq->reg_icr = value & (irq->reg_icr | ~ICR_IR);
	if(value & ICR_LVS) {
		update_level_interrupt(irq);
	} else {
		update_interrupt(irq->intco);
	}
}

static uint32_t
ifsra_read(void *clientData,uint32_t address,int rqlen)
{
	M32C_IntCo *ic = clientData;
	return ic->regIfsra;
}

static void
ifsra_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	M32C_IntCo *ic = clientData;
	ic->regIfsra = value;
}

static uint32_t
ifsr_read(void *clientData,uint32_t address,int rqlen)
{
	M32C_IntCo *ic = clientData;
	return ic->regIfsr;
}

static void
ifsr_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	M32C_IntCo *ic = clientData;
	ic->regIfsr = value;
}
/*
 ****************************************************************
 * int_src_change
 *      irq line trace, called whenever a change occurs
 ****************************************************************
 */
static void
int_src_change(SigNode *node,int value,void *clientData)
{
        M32C_Irq *irq = (M32C_Irq*) clientData;
        if((value == SIG_LOW)) {
		if((irq->reg_icr & ICR_IR) == 0) {
			irq->reg_icr |= ICR_IR;
       			update_interrupt(irq->intco);
		#if 0
		if(irq->int_nr == M32C_INT_TIMER_B5) {
			fprintf(stderr,"TIMER B5 %llu: %u\n",CycleCounter_Get(),value);
			usleep(100000);
		}
		#endif
		} else {
		#if 0
		if(irq->int_nr == M32C_INT_TIMER_B5) {
			fprintf(stderr,"timer B5 %llu: %u\n",CycleCounter_Get(),value);
			usleep(100000);
		}
		#endif
		}
        } 
}

/**
 ********************************************************************************
 * \fn static void extint_src_change(SigNode *node,int value,void *clientData)
 ********************************************************************************
 */
static void
extint_src_change(SigNode *node,int value,void *clientData)
{
        M32C_Irq *irq = (M32C_Irq*) clientData;
	M32C_IntCo *ic = irq->intco;
	if(irq->reg_icr & ICR_LVS) {
		update_level_interrupt(irq); 
	} else if(irq->reg_icr & ICR_IR) {
		/* Do nothing in this case */
	} else if(ic->regIfsr & (1 << irq->ifsr_bit)) {
		irq->reg_icr |= ICR_IR;
		update_interrupt(irq->intco);
	} else { 
		if(irq->reg_icr & ICR_POL) {
			if((value == SIG_HIGH)) {
				irq->reg_icr |= ICR_IR;
			} 
		} else {
			if((value == SIG_LOW)) {
				irq->reg_icr |= ICR_IR;
			} 
		}	
		update_interrupt(irq->intco);
	}
}

static void
M32CIntCo_Unmap(void *owner,uint32_t base,uint32_t mask)
{
	IOH_Delete8(REG_TA0IC);
	IOH_Delete8(REG_TA1IC);
	IOH_Delete8(REG_TA2IC);
	IOH_Delete8(REG_TA3IC);
	IOH_Delete8(REG_TA4IC);

	IOH_Delete8(REG_TB0IC);
	IOH_Delete8(REG_TB1IC);
	IOH_Delete8(REG_TB2IC);
	IOH_Delete8(REG_TB3IC);
	IOH_Delete8(REG_TB4IC);
	IOH_Delete8(REG_TB5IC);

	IOH_Delete8(REG_S0TIC);
	IOH_Delete8(REG_S1TIC);
	IOH_Delete8(REG_S2TIC);
	IOH_Delete8(REG_S3TIC);
	IOH_Delete8(REG_S4TIC);

	IOH_Delete8(REG_S0RIC);
	IOH_Delete8(REG_S1RIC);
	IOH_Delete8(REG_S2RIC);
	IOH_Delete8(REG_S3RIC);
	IOH_Delete8(REG_S4RIC);

	IOH_Delete8(REG_BCN0IC);
	IOH_Delete8(REG_BCN1IC);
	IOH_Delete8(REG_BCN2IC);

	IOH_Delete8(REG_DM0IC);
	IOH_Delete8(REG_DM1IC);
	IOH_Delete8(REG_DM2IC);
	IOH_Delete8(REG_DM3IC);

	IOH_Delete8(REG_AD0IC);
	IOH_Delete8(REG_KUPIC);

	IOH_Delete8(REG_IIO0IC);
	IOH_Delete8(REG_IIO1IC);
	IOH_Delete8(REG_IIO2IC);
	IOH_Delete8(REG_IIO3IC);
	IOH_Delete8(REG_IIO4IC);
	IOH_Delete8(REG_IIO5IC);

	IOH_Delete8(REG_IIO6IC);
	IOH_Delete8(REG_IIO7IC);
	IOH_Delete8(REG_IIO8IC);
	IOH_Delete8(REG_IIO9IC);
	IOH_Delete8(REG_IIO10IC);
	IOH_Delete8(REG_IIO11IC);

	/* Interrupt lines with polarity and Level/Edge switching */

	IOH_Delete8(REG_INT0IC);
	IOH_Delete8(REG_INT1IC);
	IOH_Delete8(REG_INT2IC);
	IOH_Delete8(REG_INT3IC);
	IOH_Delete8(REG_INT4IC);
	IOH_Delete8(REG_INT5IC);

	IOH_Delete8(REG_IFSR);
}

static void
M32CIntCo_Map(void *owner,uint32_t base,uint32_t mask,uint32_t mapflags)
{

	M32C_IntCo *ic = (M32C_IntCo *) owner;
	IOH_New8(REG_TA0IC,icr_read,icr_write,&ic->irq[M32C_INT_TIMER_A0]);
	IOH_New8(REG_TA1IC,icr_read,icr_write,&ic->irq[M32C_INT_TIMER_A1]);
	IOH_New8(REG_TA2IC,icr_read,icr_write,&ic->irq[M32C_INT_TIMER_A2]);
	IOH_New8(REG_TA3IC,icr_read,icr_write,&ic->irq[M32C_INT_TIMER_A3]);
	IOH_New8(REG_TA4IC,icr_read,icr_write,&ic->irq[M32C_INT_TIMER_A4]);

	IOH_New8(REG_TB0IC,icr_read,icr_write,&ic->irq[M32C_INT_TIMER_B0]);
	IOH_New8(REG_TB1IC,icr_read,icr_write,&ic->irq[M32C_INT_TIMER_B1]);
	IOH_New8(REG_TB2IC,icr_read,icr_write,&ic->irq[M32C_INT_TIMER_B2]);
	IOH_New8(REG_TB3IC,icr_read,icr_write,&ic->irq[M32C_INT_TIMER_B3]);
	IOH_New8(REG_TB4IC,icr_read,icr_write,&ic->irq[M32C_INT_TIMER_B4]);
	IOH_New8(REG_TB5IC,icr_read,icr_write,&ic->irq[M32C_INT_TIMER_B5]);

	IOH_New8(REG_S0TIC,icr_read,icr_write,&ic->irq[M32C_INT_UART0_TX]);
	IOH_New8(REG_S1TIC,icr_read,icr_write,&ic->irq[M32C_INT_UART1_TX]);
	IOH_New8(REG_S2TIC,icr_read,icr_write,&ic->irq[M32C_INT_UART2_TX]);
	IOH_New8(REG_S3TIC,icr_read,icr_write,&ic->irq[M32C_INT_UART3_TX]);
	IOH_New8(REG_S4TIC,icr_read,icr_write,&ic->irq[M32C_INT_UART4_TX]);

	IOH_New8(REG_S0RIC,icr_read,icr_write,&ic->irq[M32C_INT_UART0_RX]);
	IOH_New8(REG_S1RIC,icr_read,icr_write,&ic->irq[M32C_INT_UART1_RX]);
	IOH_New8(REG_S2RIC,icr_read,icr_write,&ic->irq[M32C_INT_UART2_RX]);
	IOH_New8(REG_S3RIC,icr_read,icr_write,&ic->irq[M32C_INT_UART3_RX]);
	IOH_New8(REG_S4RIC,icr_read,icr_write,&ic->irq[M32C_INT_UART4_RX]);
	
	IOH_New8(REG_BCN0IC,icr_read,icr_write,&ic->irq[M32C_INT_BCN_UART03]);
	IOH_New8(REG_BCN1IC,icr_read,icr_write,&ic->irq[M32C_INT_BCN_UART14]);
	IOH_New8(REG_BCN2IC,icr_read,icr_write,&ic->irq[M32C_INT_BCN_UART2]);

	IOH_New8(REG_DM0IC,icr_read,icr_write,&ic->irq[M32C_INT_DMA0]);
	IOH_New8(REG_DM1IC,icr_read,icr_write,&ic->irq[M32C_INT_DMA1]);
	IOH_New8(REG_DM2IC,icr_read,icr_write,&ic->irq[M32C_INT_DMA2]);
	IOH_New8(REG_DM3IC,icr_read,icr_write,&ic->irq[M32C_INT_DMA3]);

	IOH_New8(REG_AD0IC,icr_read,icr_write,&ic->irq[M32C_INT_AD0]);
	IOH_New8(REG_KUPIC,icr_read,icr_write,&ic->irq[M32C_INT_KEYINPUT]);

	IOH_New8(REG_IIO0IC,icr_read,icr_write,&ic->irq[M32C_INT_IIO0]);
	IOH_New8(REG_IIO1IC,icr_read,icr_write,&ic->irq[M32C_INT_IIO1]);
	IOH_New8(REG_IIO2IC,icr_read,icr_write,&ic->irq[M32C_INT_IIO2]);
	IOH_New8(REG_IIO3IC,icr_read,icr_write,&ic->irq[M32C_INT_IIO3]);
	IOH_New8(REG_IIO4IC,icr_read,icr_write,&ic->irq[M32C_INT_IIO4]);
	IOH_New8(REG_IIO5IC,icr_read,icr_write,&ic->irq[M32C_INT_IIO5]);
	IOH_New8(REG_IIO6IC,icr_read,icr_write,&ic->irq[M32C_INT_IIO6]);
	IOH_New8(REG_IIO7IC,icr_read,icr_write,&ic->irq[M32C_INT_IIO7]);
	IOH_New8(REG_IIO8IC,icr_read,icr_write,&ic->irq[M32C_INT_IIO8]);
	IOH_New8(REG_IIO9IC,icr_read,icr_write,&ic->irq[M32C_INT_IIO9]);
	IOH_New8(REG_IIO10IC,icr_read,icr_write,&ic->irq[M32C_INT_IIO10]);
	IOH_New8(REG_IIO11IC,icr_read,icr_write,&ic->irq[M32C_INT_IIO11]);

	/* External Interrupt lines with polarity and Level/Edge switching */
	IOH_New8(REG_INT0IC,exticr_read,exticr_write,&ic->irq[M32C_INT_INT0]);
	IOH_New8(REG_INT1IC,exticr_read,exticr_write,&ic->irq[M32C_INT_INT1]);
	IOH_New8(REG_INT2IC,exticr_read,exticr_write,&ic->irq[M32C_INT_INT2]);
	IOH_New8(REG_INT3IC,exticr_read,exticr_write,&ic->irq[M32C_INT_INT3]);
	IOH_New8(REG_INT4IC,exticr_read,exticr_write,&ic->irq[M32C_INT_INT4]);
	IOH_New8(REG_INT5IC,exticr_read,exticr_write,&ic->irq[M32C_INT_INT5]);
	
	/* The interrupt controler */
	IOH_New8(REG_IFSRA,ifsra_read,ifsra_write,ic);
	IOH_New8(REG_IFSR,ifsr_read,ifsr_write,ic);
}


BusDevice * 
M32C87IntCo_New(const char *name) 
{
	M32C_IntCo *ic = sg_new(M32C_IntCo);
	int i;
	for(i = 0;i < 64; i++) {
		M32C_Irq *irq = &ic->irq[i];	
		irq->irqInput = SigNode_New("%s.irq%d",name,i);
		if(!irq->irqInput) {
			fprintf(stderr,"M32C87: Can not create interrupt %d\n",i);
			exit(1);
		}
		irq->intco = ic;
		irq->int_nr = i;
		switch(i) {
			case M32C_INT_INT0:
				irq->irqTrace = SigNode_Trace(irq->irqInput,extint_src_change,irq);
				irq->ifsr_bit = 0;
				break;
			case M32C_INT_INT1:
				irq->irqTrace = SigNode_Trace(irq->irqInput,extint_src_change,irq);
				irq->ifsr_bit = 1;
				break;
			case M32C_INT_INT2:
				irq->irqTrace = SigNode_Trace(irq->irqInput,extint_src_change,irq);
				irq->ifsr_bit = 2;
				break;
			case M32C_INT_INT3:
				irq->irqTrace = SigNode_Trace(irq->irqInput,extint_src_change,irq);
				irq->ifsr_bit = 3;
				break;
			case M32C_INT_INT4:
				irq->irqTrace = SigNode_Trace(irq->irqInput,extint_src_change,irq);
				irq->ifsr_bit = 4;
				break;
			case M32C_INT_INT5:
				irq->irqTrace = SigNode_Trace(irq->irqInput,extint_src_change,irq);
				irq->ifsr_bit = 5;
				break;
			default:
				irq->irqTrace = SigNode_Trace(irq->irqInput,int_src_change,irq);
				break;
		}
	}
	ic->bdev.first_mapping = NULL;
        ic->bdev.Map = M32CIntCo_Map;
        ic->bdev.UnMap = M32CIntCo_Unmap;
        ic->bdev.owner = ic;
        ic->bdev.hw_flags = MEM_FLAG_WRITABLE | MEM_FLAG_READABLE;
	return &ic->bdev;
}
