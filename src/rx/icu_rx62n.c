/*
 **********************************************************************************************
 * Renesas RX62N Interrupt control unit simulation. 
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

#include "bus.h"
#include "sgstring.h"
#include "signode.h"
#include "icu_rx62n.h"
#include "cpu_rx.h"

#if 0
#define dbgprintf(x...) { fprintf(stderr,x); }
#else
#define dbgprintf(x...)
#endif

#define REG_IR(base,irq)	((base) + (irq))
#define		IR_IR	(1 << 0)
#define REG_DTC(base,dtc)	((base) + 0x100 + (dtc))
#define		DTC_DTCE	(1 << 0)
#define REG_IER(base,ier)	((base) + 0x200 + (ier))
#define		IER_IEN0	(1 << 0)
#define		IER_IEN1	(1 << 1)
#define		IER_IEN2	(1 << 2)
#define		IER_IEN3	(1 << 3)
#define		IER_IEN4	(1 << 4)
#define		IER_IEN5	(1 << 5)
#define		IER_IEN6	(1 << 6)
#define		IER_IEN7	(1 << 7)
#define REG_SWINTR(base)	((base) + 0x2E0)
#define		SWINTR_SWINT	(1 << 0)
#define REG_FIR(base)		((base) + 0x2F0)
#define		FIR_FVCT_MSK	(0xff)
#define		FIR_FIEN	(1 << 15)
#define REG_IPR(base,ipr)	((base) + 0x300 + (ipr))
#define		IPR_IPR_MSK	(0x0f)
#define REG_DMRSR(base,idx)	((base) + 0x400 + (idx << 2))
#define REG_IRQCR(base,idx)	((base) + 0x500 + (idx))
#define		IRQCR_IRQMD_MSK		(3 << 2)
#define		IRQCR_IRQMD_SHIFT	(2)
#define			IRQMD_LO_LVL	(0)
#define			IRQMD_N_EDGE	(1 << 2)
#define			IRQMD_P_EDGE	(2 << 2)
#define			IRQMD_HI_LVL	(3 << 2)
#define REG_NMISR(base)		((base) + 0x580)
#define		NMISR_NMIST	(1 << 0)
#define		NMISR_LVDST	(1 << 1)
#define		NMISR_OSTST	(1 << 2)
#define REG_NMIER(base)		((base) + 0x581)
#define		NMIER_NMIEN	(1 << 0)
#define		NMIER_LVDEN	(1 << 1)
#define		NMIER_OSTEN	(1 << 2)
#define REG_NMICLR(base)	((base) + 0x582)
#define		NMICLR_NMICLR	(1 << 0)
#define		NMICLR_OSTCLR	(1 << 2)
#define REG_NMICR(base)		((base) + 0x583)
#define		NMICR_NMIMD	(1 << 3)

typedef struct ICU ICU;
typedef struct Irq {
	ICU *icu;
	uint8_t irqNr;
	uint8_t regIR;
	uint8_t regDTC;
	uint8_t regIRQCR;
	SigNode *sigIrq;
	
	uint8_t ipl;  /* while in active list */
	struct Irq *prev;
	struct Irq *next;
} Irq;

struct ICU {
	BusDevice bdev;
	Irq irq[256];
	Irq *firstActiveIrq;
	SigNode *sigIrqAck;
	int8_t regIER[32];
	uint8_t regSWINTR;
	/* Shit, multiple interrupts share an IPR */
	uint8_t regIPR[0x90];	
	const int16_t *irqToIpr;
	uint8_t regDMRSR[4];
	/* Only 16 (extint) are accessable by the CPU */
	uint8_t regNMISR;
	uint8_t regNMIER;
	uint8_t regNMICLR;
	uint8_t regNMICR;
};

/**
 *************************************************************
 * Initial values for the IRQ mode registers.
 *************************************************************
 */
static const uint8_t rx62nIrqcr[256] =  
{
/*  0  */   IRQMD_LO_LVL, IRQMD_LO_LVL, IRQMD_LO_LVL, IRQMD_LO_LVL,
/*  4  */   IRQMD_LO_LVL, IRQMD_LO_LVL, IRQMD_LO_LVL, IRQMD_LO_LVL,
/*  8  */   IRQMD_LO_LVL, IRQMD_LO_LVL, IRQMD_LO_LVL, IRQMD_LO_LVL,
/*  12  */  IRQMD_LO_LVL, IRQMD_LO_LVL, IRQMD_LO_LVL, IRQMD_LO_LVL,
/*  16  */  IRQMD_LO_LVL, IRQMD_LO_LVL, IRQMD_LO_LVL, IRQMD_LO_LVL,
/*  20  */  IRQMD_LO_LVL, IRQMD_LO_LVL, IRQMD_LO_LVL, IRQMD_N_EDGE,
/*  24  */  IRQMD_LO_LVL, IRQMD_LO_LVL, IRQMD_LO_LVL, IRQMD_N_EDGE,
/*  28  */  IRQMD_N_EDGE, IRQMD_N_EDGE, IRQMD_N_EDGE, IRQMD_N_EDGE,
/*  32  */  IRQMD_LO_LVL, IRQMD_LO_LVL, IRQMD_LO_LVL, IRQMD_LO_LVL,
/*  36  */  IRQMD_N_EDGE, IRQMD_N_EDGE, IRQMD_N_EDGE, IRQMD_LO_LVL,
/*  40  */  IRQMD_N_EDGE, IRQMD_N_EDGE, IRQMD_N_EDGE, IRQMD_LO_LVL,
/*  44  */  IRQMD_LO_LVL, IRQMD_N_EDGE, IRQMD_N_EDGE, IRQMD_LO_LVL,
/*  48  */  IRQMD_LO_LVL, IRQMD_N_EDGE, IRQMD_N_EDGE, IRQMD_LO_LVL,
/*  52  */  IRQMD_LO_LVL, IRQMD_LO_LVL, IRQMD_LO_LVL, IRQMD_LO_LVL,
/*  56  */  IRQMD_N_EDGE, IRQMD_N_EDGE, IRQMD_N_EDGE, IRQMD_N_EDGE,
/*  60  */  IRQMD_N_EDGE, IRQMD_LO_LVL, IRQMD_N_EDGE, IRQMD_N_EDGE,
/*  64  */  IRQMD_LO_LVL, IRQMD_LO_LVL, IRQMD_LO_LVL, IRQMD_LO_LVL,
/*  68  */  IRQMD_LO_LVL, IRQMD_LO_LVL, IRQMD_LO_LVL, IRQMD_LO_LVL,
/*  72  */  IRQMD_LO_LVL, IRQMD_LO_LVL, IRQMD_LO_LVL, IRQMD_LO_LVL,
/*  76  */  IRQMD_LO_LVL, IRQMD_LO_LVL, IRQMD_LO_LVL, IRQMD_LO_LVL,
/*  80  */  IRQMD_LO_LVL, IRQMD_LO_LVL, IRQMD_LO_LVL, IRQMD_LO_LVL,
/*  84  */  IRQMD_LO_LVL, IRQMD_LO_LVL, IRQMD_LO_LVL, IRQMD_LO_LVL,
/*  88  */  IRQMD_LO_LVL, IRQMD_LO_LVL, IRQMD_LO_LVL, IRQMD_LO_LVL,
/*  92  */  IRQMD_N_EDGE, IRQMD_LO_LVL, IRQMD_LO_LVL, IRQMD_LO_LVL,
/*  96  */  IRQMD_N_EDGE, IRQMD_LO_LVL, IRQMD_N_EDGE, IRQMD_N_EDGE,
/*  100 */  IRQMD_LO_LVL, IRQMD_LO_LVL, IRQMD_N_EDGE, IRQMD_LO_LVL,
/*  104 */  IRQMD_LO_LVL, IRQMD_LO_LVL, IRQMD_LO_LVL, IRQMD_LO_LVL,
/*  108 */  IRQMD_LO_LVL, IRQMD_LO_LVL, IRQMD_LO_LVL, IRQMD_LO_LVL,
/*  112 */  IRQMD_LO_LVL, IRQMD_LO_LVL, IRQMD_N_EDGE, IRQMD_N_EDGE,
/*  116  */  IRQMD_N_EDGE, IRQMD_LO_LVL, IRQMD_N_EDGE, IRQMD_N_EDGE,
/*  120  */  IRQMD_N_EDGE, IRQMD_LO_LVL, IRQMD_N_EDGE, IRQMD_N_EDGE,
/*  124  */  IRQMD_N_EDGE, IRQMD_LO_LVL, IRQMD_N_EDGE, IRQMD_N_EDGE,
/*  128  */  IRQMD_N_EDGE, IRQMD_LO_LVL, IRQMD_N_EDGE, IRQMD_N_EDGE,
/*  132  */  IRQMD_N_EDGE, IRQMD_LO_LVL, IRQMD_N_EDGE, IRQMD_N_EDGE,
/*  136  */  IRQMD_N_EDGE, IRQMD_LO_LVL, IRQMD_N_EDGE, IRQMD_N_EDGE,
/*  140  */  IRQMD_N_EDGE, IRQMD_LO_LVL, IRQMD_N_EDGE, IRQMD_N_EDGE,
/*  144  */  IRQMD_N_EDGE, IRQMD_LO_LVL, IRQMD_N_EDGE, IRQMD_N_EDGE,
/*  148  */  IRQMD_N_EDGE, IRQMD_LO_LVL, IRQMD_N_EDGE, IRQMD_N_EDGE,
/*  152  */  IRQMD_N_EDGE, IRQMD_LO_LVL, IRQMD_N_EDGE, IRQMD_N_EDGE,
/*  156  */  IRQMD_N_EDGE, IRQMD_LO_LVL, IRQMD_N_EDGE, IRQMD_N_EDGE,
/*  160  */  IRQMD_N_EDGE, IRQMD_LO_LVL, IRQMD_N_EDGE, IRQMD_N_EDGE,
/*  164  */  IRQMD_N_EDGE, IRQMD_LO_LVL, IRQMD_N_EDGE, IRQMD_N_EDGE,
/*  168  */  IRQMD_N_EDGE, IRQMD_LO_LVL, IRQMD_LO_LVL, IRQMD_LO_LVL,
/*  172  */  IRQMD_LO_LVL, IRQMD_LO_LVL, IRQMD_N_EDGE, IRQMD_N_EDGE,
/*  176  */  IRQMD_N_EDGE, IRQMD_LO_LVL, IRQMD_N_EDGE, IRQMD_N_EDGE,
/*  180  */  IRQMD_N_EDGE, IRQMD_LO_LVL, IRQMD_N_EDGE, IRQMD_N_EDGE,
/*  184  */  IRQMD_N_EDGE, IRQMD_N_EDGE, IRQMD_LO_LVL, IRQMD_LO_LVL,
/*  188  */  IRQMD_LO_LVL, IRQMD_LO_LVL, IRQMD_LO_LVL, IRQMD_LO_LVL,
/*  192  */  IRQMD_LO_LVL, IRQMD_LO_LVL, IRQMD_LO_LVL, IRQMD_LO_LVL,
/*  196  */  IRQMD_LO_LVL, IRQMD_LO_LVL, IRQMD_N_EDGE, IRQMD_N_EDGE,
/*  200  */  IRQMD_N_EDGE, IRQMD_N_EDGE, IRQMD_N_EDGE, IRQMD_N_EDGE,
/*  204  */  IRQMD_LO_LVL, IRQMD_LO_LVL, IRQMD_LO_LVL, IRQMD_LO_LVL,
/*  208  */  IRQMD_LO_LVL, IRQMD_LO_LVL, IRQMD_LO_LVL, IRQMD_LO_LVL,
/*  212  */  IRQMD_LO_LVL, IRQMD_LO_LVL, IRQMD_LO_LVL, IRQMD_N_EDGE,
/*  216  */  IRQMD_N_EDGE, IRQMD_LO_LVL, IRQMD_LO_LVL, IRQMD_N_EDGE,
/*  220  */  IRQMD_N_EDGE, IRQMD_LO_LVL, IRQMD_LO_LVL, IRQMD_N_EDGE,
/*  224  */  IRQMD_N_EDGE, IRQMD_LO_LVL, IRQMD_LO_LVL, IRQMD_N_EDGE,
/*  228  */  IRQMD_N_EDGE, IRQMD_LO_LVL, IRQMD_LO_LVL, IRQMD_LO_LVL,
/*  232  */  IRQMD_LO_LVL, IRQMD_LO_LVL, IRQMD_LO_LVL, IRQMD_N_EDGE,
/*  236  */  IRQMD_N_EDGE, IRQMD_LO_LVL, IRQMD_LO_LVL, IRQMD_N_EDGE,
/*  240  */  IRQMD_N_EDGE, IRQMD_LO_LVL, IRQMD_LO_LVL, IRQMD_LO_LVL,
/*  244  */  IRQMD_LO_LVL, IRQMD_LO_LVL, IRQMD_LO_LVL, IRQMD_N_EDGE,
/*  248  */  IRQMD_N_EDGE, IRQMD_LO_LVL, IRQMD_LO_LVL, IRQMD_N_EDGE,
/*  252  */  IRQMD_N_EDGE, IRQMD_LO_LVL, IRQMD_LO_LVL, IRQMD_LO_LVL
};

static const int16_t rx62nIrqToIpr[256] = {
/* 0   */  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
/* 16  */   0, -1, -1, -1, -1,  1, -1,  2, -1, -1, -1,  3,  4,  5,  6,  7, 
/* 32  */   8,  9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 20, 20, 20, 
/* 48  */  21, 21, 21, 21, 21, -1, -1, -1, 24, 24, 24, 24, 24, 29, 30, 31,
/* 64  */  32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47,
/* 80  */  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 58, 59, 60, -1, -1, -1,
/* 96  */  64, -1, 68, 69, -1, -1, 72, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
/* 112 */  -1, -1, 81, 81, 81, 81, 82, 82, 82, 83, 83, 84, 84, 85, 85, 86,
/* 128 */  86, 87, 87, 87, 87, 88, 89, 89, 89, 89, 90, 91, 91, 91, 92, 92,
/* 144 */  92, 92, 93, 93, 93, 94, 94, 95, 95, 96, 96, 97, 97, 98, 98, 98,
/* 160 */  98, 99,100,100,100,100,101,102,102,102,103,103,103,103,104,104,
/* 176 */ 104,105,105,105,106,106,106,107,107,107, -1, -1, -1, -1, -1, -1,
/* 192 */  -1, -1, -1, -1, -1, -1,112,113,114,115,116,117, -1, -1, -1, -1,
/* 208 */  -1, -1, -1, -1, -1, -1,128,128,128,128,129,129,129,129,130,130, 
/* 224 */ 130,130,131,131,131,131, -1, -1, -1, -1,133,133,133,133,134,134,
/* 240 */ 134,134, -1, -1, -1, -1,136,137,138,139,140,141,142,143,144,145
};


static void
PostInterruptPL(ICU *icu) 
{
	Irq *irq = icu->firstActiveIrq;
	if(irq == NULL) {
		dbgprintf("No first active\n");
		RX_PostInterrupt(0,0);
	} else {
		dbgprintf("First one is irq %d, ipl %d\n",irq->irqNr,irq->ipl);
		RX_PostInterrupt(irq->ipl,irq->irqNr);
	}

}

/**
 *********************************************************************************
 * \fn static void update_interrupt(Irq *irq) 
 *********************************************************************************
 */
static void
update_interrupt(Irq *irq) 
{
	ICU *icu = irq->icu;
	Irq *cursor,*prev;
	int16_t ipl;
	unsigned int ipr_idx;
	/* If the irq is already in the list remove it for reinsertion or not */
	if(irq->prev) {
		if(irq->next) {
			irq->next->prev = irq->prev;
		}
		irq->prev->next = irq->next;
		irq->prev = NULL;
		irq->next = NULL;
	} else if(icu->firstActiveIrq == irq) {
		if(irq->next) {
			irq->next->prev = NULL;
		}
		icu->firstActiveIrq = irq->next;	
		irq->next = NULL;
	}
	ipr_idx = icu->irqToIpr[irq->irqNr];
	if((ipr_idx < 0) || (ipr_idx >= array_size(icu->regIPR))) {
		fprintf(stderr,"Bug: No interrupt priority for irq %u\n",irq->irqNr);
		exit(1);
	}
	ipl = icu->regIPR[ipr_idx] & 0xf;
	if(!(irq->regIR & IR_IR) || (ipl == 0)) {
		PostInterruptPL(icu);
		return;
	}
	dbgprintf("inserting IPL of %d from ipr_idx %d\n",ipl,ipr_idx);
	/* Now inserting into the list is necessary */
	irq->ipl = ipl;
	if(icu->firstActiveIrq == NULL) {
		dbgprintf("It is the first one\n");
		irq->next = NULL;
		irq->prev = NULL;
		icu->firstActiveIrq = irq;
	} else {
		/* Insert into list sorted by priority */
		dbgprintf("Not th first\n");
		for(prev = NULL,cursor = icu->firstActiveIrq; cursor; 
				prev = cursor,cursor = cursor->next) {
			if((irq->ipl > cursor->ipl) || 
			  ((irq->ipl == cursor->ipl) && (irq->irqNr < cursor->irqNr))) 
			{
				irq->next = cursor;
				irq->prev = cursor->prev;
				cursor->prev = irq;	
				if(prev) {
					prev->next = irq;
				} else {
					icu->firstActiveIrq = irq;
				}
				break;
			}
		}
		if(prev && !cursor) {
			irq->next = NULL;
			irq->prev = prev;
			prev->next = irq;
		}
	}
	dbgprintf("Now posting the first active one\n");
	PostInterruptPL(icu);
}

/**
 *****************************************************************************************
 * AckInterrupt is called by the CPU. It acknowledges the first active IRQ because
 * this was the last one which was posted to the CPU.
 * The interrupt is acknowlegded on negative edge currently only.
 *****************************************************************************************
 */
static void
AckInterrupt(SigNode *sig,int value,void *eventData)
{
	ICU *icu = eventData;
	Irq *irq = icu->firstActiveIrq;
	if(value != SIG_LOW) {
		return;
	}
	if(!irq) {
		fprintf(stderr,"Bug: CPU acked non posted interrupt\n");
		return;
	}
	/* 
	 ***********************************************************
	 * Edge interrupts are cleared on ack by the CPU. 
	 * Others are removed from Interrupts source 
	 ***********************************************************
	 */
	switch(irq->regIRQCR & IRQCR_IRQMD_MSK) {
		case IRQMD_N_EDGE:
		case IRQMD_P_EDGE:
			irq->regIR &= ~IR_IR;
			update_interrupt(irq);
			dbgprintf("Acked an Edge IRQ\n");
			break;
	}
}

static void
update_level_interrupt(Irq *irq) 
{
	switch(irq->regIRQCR & IRQCR_IRQMD_MSK) {
		case IRQMD_LO_LVL:
			if(SigNode_Val(irq->sigIrq) == SIG_LOW) {
				irq->regIR |= IR_IR;
			} else {
				irq->regIR &= ~IR_IR;
			}
			update_interrupt(irq);
			break;
		case IRQMD_HI_LVL:
			if(SigNode_Val(irq->sigIrq) == SIG_HIGH) {
				irq->regIR |= IR_IR;
			} else {
				irq->regIR &= ~IR_IR;
			}
			update_interrupt(irq);
			break;
	}
}

static uint32_t
ir_read(void *clientData,uint32_t address,int rqlen)
{
	Irq *irq = clientData;
        return irq->regIR;
}

static void
ir_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	Irq *irq = clientData;
	switch(irq->regIRQCR & IRQCR_IRQMD_MSK) {
		case IRQMD_HI_LVL:
		case IRQMD_LO_LVL:
			irq->regIR = value & ~IR_IR;
			break;
		case IRQMD_N_EDGE:	
		case IRQMD_P_EDGE:	
			irq->regIR = value;
			update_interrupt(irq);
			break;
	}
	
}

static uint32_t
dtc_read(void *clientData,uint32_t address,int rqlen)
{
        return 0;
}

static void
dtc_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
}

static uint32_t
ier_read(void *clientData,uint32_t address,int rqlen)
{
        return 0;
}

static void
ier_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
}

static uint32_t
swintr_read(void *clientData,uint32_t address,int rqlen)
{
        return 0;
}

static void
swintr_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
}

static uint32_t
ipr_read(void *clientData,uint32_t address,int rqlen)
{
	ICU *icu = clientData;
	unsigned int idx = address & 0xff;
	if(idx > array_size(icu->regIPR)) {
		fprintf(stderr,"Illegal index %d of IPR register\n",idx);
		return 0;
	}
        return icu->regIPR[idx];
}

static void
ipr_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	ICU *icu = clientData;
	unsigned int idx = address & 0xff;
	if(idx > array_size(icu->regIPR)) {
		fprintf(stderr,"Illegal index %d of IPR register\n",idx);
		return;
	}
        icu->regIPR[idx] = value;
}


static uint32_t
dmrsr_read(void *clientData,uint32_t address,int rqlen)
{
        return 0;
}

static void
dmrsr_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
}

static uint32_t
irqcr_read(void *clientData,uint32_t address,int rqlen)
{
	Irq *irq = clientData;
        return irq->regIRQCR;
}

static void
irqcr_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	Irq *irq = clientData;
        irq->regIRQCR = value;
	update_level_interrupt(irq);
}

static uint32_t
nmisr_read(void *clientData,uint32_t address,int rqlen)
{
        return 0;
}

static void
nmisr_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
}

static uint32_t
nmier_read(void *clientData,uint32_t address,int rqlen)
{
        return 0;
}

static void
nmier_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
}

static uint32_t
nmiclr_read(void *clientData,uint32_t address,int rqlen)
{
        return 0;
}

static void
nmiclr_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
}

static uint32_t
nmicr_read(void *clientData,uint32_t address,int rqlen)
{
        return 0;
}

static void
nmicr_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
}

/**
 ****************************************************************
 *
 ****************************************************************
 */
static void
SigIrqTraceProc(SigNode *sig,int value,void *eventData)
{
	Irq *irq = eventData;
	uint8_t old = irq->regIR;
	uint8_t diff;
	switch(irq->regIRQCR & IRQCR_IRQMD_MSK) {
		case IRQMD_LO_LVL:
			if(value == SIG_LOW) {
				irq->regIR |= IR_IR;
			} else {
				irq->regIR &= ~IR_IR;
			}
			break;

		case IRQMD_N_EDGE:
			if(value == SIG_LOW) {
				irq->regIR |= IR_IR;
			}
			break;

		case IRQMD_P_EDGE:
			if(value == SIG_HIGH) {
				irq->regIR |= IR_IR;
			}
			break;

		case IRQMD_HI_LVL:
			if(value == SIG_HIGH) {
				irq->regIR |= IR_IR;
			} else {
				irq->regIR &= ~IR_IR;
			}
	} 
	diff = old ^ irq->regIR;
	if(diff) {
		update_interrupt(irq);
	}
}

/**
 ******************************************************************************
 * static void ICU_UnMap(void *module_owner,uint32_t base,uint32_t mapsize)
 * Remove the registers of the ICU from the address space.
 ******************************************************************************
 */
static void
ICU_UnMap(void *module_owner,uint32_t base,uint32_t mapsize)
{
	ICU *icu = module_owner;
	unsigned int i;
	for(i = 0; i < array_size(icu->irq);i++) {
		IOH_Delete8(REG_IR(base,i));
		IOH_Delete8(REG_DTC(base,i));
	}
	for(i = 0; i < array_size(icu->regIER);i++) {
		IOH_Delete8(REG_IER(base,i));
	}
	IOH_New8(REG_SWINTR(base),swintr_read,swintr_write,icu);
	for(i = 0; i < array_size(icu->regIPR);i++) {
		IOH_Delete8(REG_IPR(base,i));
	}
	for(i = 0; i < array_size(icu->regDMRSR);i++) {
		IOH_Delete8(REG_DMRSR(base,i));
	}
	for(i = 0; i < 16;i++) {
		IOH_Delete8(REG_IRQCR(base,i));
	}
	IOH_Delete8(REG_NMISR(base));
	IOH_Delete8(REG_NMIER(base));
	IOH_Delete8(REG_NMICLR(base));
	IOH_Delete8(REG_NMICR(base));
}

static void
ICU_Map(void *module_owner,uint32_t base,uint32_t mapsize,uint32_t flags)
{
	ICU *icu = module_owner;
	unsigned int i;
	for(i = 0; i < array_size(icu->irq);i++) {
		Irq *irq = &icu->irq[i];
		IOH_New8(REG_IR(base,i),ir_read,ir_write,irq);
		IOH_New8(REG_DTC(base,i),dtc_read,dtc_write,irq);
	}
	for(i = 0; i < array_size(icu->regIER);i++) {
		IOH_New8(REG_IER(base,i),ier_read,ier_write,icu);
	}
	IOH_New8(REG_SWINTR(base),swintr_read,swintr_write,icu);

	for(i = 0; i < array_size(icu->regIPR);i++) {
		IOH_New8(REG_IPR(base,i),ipr_read,ipr_write,icu);
	}
	for(i = 0; i < array_size(icu->regDMRSR);i++) {
		IOH_New8(REG_DMRSR(base,i),dmrsr_read,dmrsr_write,icu);
	}
	for(i = 0; i < 16;i++) {
		IOH_New8(REG_IRQCR(base,i),irqcr_read,irqcr_write,&icu->irq[i + 64]);
	}
	IOH_New8(REG_NMISR(base),nmisr_read,nmisr_write,icu);
	IOH_New8(REG_NMIER(base),nmier_read,nmier_write,icu);
	IOH_New8(REG_NMICLR(base),nmiclr_read,nmiclr_write,icu);
	IOH_New8(REG_NMICR(base),nmicr_read,nmicr_write,icu);
}

BusDevice *
RX62NICU_New(const char *name) 
{
	int i;
	ICU *icu = sg_new(ICU);
	icu->irqToIpr = rx62nIrqToIpr;
	icu->bdev.first_mapping = NULL;
        icu->bdev.Map = ICU_Map;
        icu->bdev.UnMap = ICU_UnMap;
        icu->bdev.owner = icu;
        icu->bdev.hw_flags = MEM_FLAG_READABLE | MEM_FLAG_WRITABLE;
	for(i = 0; i < 256;i++) {
		Irq *irq = &icu->irq[i];
		irq->icu = icu;
		irq->irqNr = i;
		irq->regIRQCR = rx62nIrqcr[i];
		irq->sigIrq = SigNode_New("%s.irq%d",name,i);
		if(!irq->sigIrq) {
			fprintf(stderr,"Can not create interrupt line\n");
			exit(1);
		}
		SigNode_Trace(irq->sigIrq,SigIrqTraceProc,irq);
	}
	icu->sigIrqAck = SigNode_New("%s.irqAck",name);
	if(!icu->sigIrqAck) {
		fprintf(stderr,"Can not create IRQ Acknowlege line\n");
		exit(1);
	}
	SigNode_Trace(icu->sigIrqAck,AckInterrupt,icu);
	fprintf(stderr,"Created RX62N Interrupt control unit\n");
	return &icu->bdev;
}
