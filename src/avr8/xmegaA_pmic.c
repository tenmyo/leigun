/*
 *************************************************************************************************
 *
 * Atmel XMega-A programmable interrupt controller (PMIC) 
 *
 * State: nothing implemented 
 *
 * Copyright 2010 Jochen Karrer. All rights reserved.
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
#include <stdio.h>
#include <string.h>
#include "sgstring.h"
#include "serial.h"
#include "clock.h"
#include "cycletimer.h"
#include "signode.h"
#include "xmegaA_pmic.h"
#include "avr8_cpu.h"


#define REG_PMIC_STATUS(base)	((base) + 0x00)
#define		STATUS_NMIEX	(1 << 7)
#define		STATUS_HILVLEX	(1 << 2)
#define		STATUS_MEDLVLEX	(1 << 1)
#define		STATUS_LOLVLEX	(1 << 0)
#define REG_PMIC_INTPRI(base)	((base) + 0x01)
#define REG_PMIC_CTRL(base)	((base) + 0x02)
#define		CTRL_RREN	(1 << 7)
#define		CTRL_IVLSEL	(1 << 6)
#define		CTRL_HILVLEN	(1 << 2)
#define		CTRL_MEDLVLEN	(1 << 1)
#define		CTRL_LOLVLEN	(1 << 0)

#define 	INTLVL_DISA	(0)
#define 	INTLVL_LO	(1)
#define 	INTLVL_MED	(2)
#define 	INTLVL_HI	(3)

typedef struct PMicIrq {
	int irqvect_no;
	int level;
	int busy;
	struct PMicIrq *next;
	struct PMicIrq *prev;
} PMicIrq;

struct PMic {
	unsigned int nr_ints;
	/* Array */
	PMicIrq *allIrqs;	
	PMicIrq *nextIrq;
	/* Queues */
	PMicIrq *loIrqs;
	PMicIrq *midIrqs;
	PMicIrq *hiIrqs;
	uint8_t regStatus;
	uint8_t regIntPri;
	uint8_t regCtrl;
};

/**
 *******************************************************************
 * \fn static void Pmic_SearchForInt(PMic *pmic) 
 *******************************************************************
 */
static void
Pmic_SearchForInt(PMic *pmic) 
{
	int i;
	PMicIrq *irq;
	uint8_t intmsk = pmic->regCtrl & ~pmic->regStatus;
	if(intmsk & CTRL_HILVLEN) {
		for(i = 0;i < pmic->nr_ints;i++) {
			irq = &pmic->allIrqs[i];
			if(irq->level == INTLVL_HI) {
				pmic->nextIrq = irq;
				AVR8_PostSignal(AVR8_SIG_IRQ);
				return;			
			}	
		}
	}
	if(intmsk & CTRL_MEDLVLEN) {
		for(i = 0;i < pmic->nr_ints;i++) {
			irq = &pmic->allIrqs[i];
			if(irq->level == INTLVL_MED) {
				pmic->nextIrq = irq;
				AVR8_PostSignal(AVR8_SIG_IRQ);
				return;			
			}	
		}
	}
	if(intmsk & CTRL_LOLVLEN) {
		for(i = 0;i < pmic->nr_ints;i++) {
			int nr;
			if(pmic->regCtrl & CTRL_RREN) {
				nr = (i + 1 + pmic->regIntPri) % pmic->nr_ints;
			} else {
				nr = i;
			}
			irq = &pmic->allIrqs[nr];
			if(irq->level == INTLVL_LO) {
				pmic->nextIrq = irq;
				AVR8_PostSignal(AVR8_SIG_IRQ);
				if(pmic->regCtrl & CTRL_RREN) {
					pmic->regIntPri = nr;
				}
				return;			
			}	
		}
	}
	pmic->nextIrq = NULL;
	gavr8.cpu_signals_raw &= ~AVR8_SIG_IRQ;
	AVR8_UpdateCpuSignals();
	AVR8_UnpostSignal(AVR8_SIG_IRQ);
} 

/**
 ********************************************************************
 * \fn static void Pmic_Interrupt(void *irqData)
 ********************************************************************
 */
static void
Pmic_Interrupt(void *irqData)
{
	PMic *pmic = irqData;
	PMicIrq *irq;
        uint16_t sp = GET_REG_SP;
        uint32_t pc = GET_REG_PC;
	irq = pmic->nextIrq;
	if(!irq) {
		fprintf(stderr,"Got nonexisting interrupt\n");
		AVR8_UnpostSignal(AVR8_SIG_IRQ);
		return;
	}
	//SendAckToIrqOwner;
        //SigNode_Set(gavr8.irqAckNode[irqvect],SIG_LOW);
        //SigNode_Set(gavr8.irqAckNode[irqvect],SIG_HIGH);

        AVR8_WriteMem8(pc & 0xff,sp--);
        AVR8_WriteMem8((pc >> 8) & 0xff,sp--);
        if(gavr8.pc24bit) {
                AVR8_WriteMem8((pc >> 16) & 0xff,sp--);
                CycleCounter+=1;
        }
	/* XMega does not clear Interrupt flag ! */
	switch(irq->level) {
		case INTLVL_DISA:
			fprintf(stderr,"Should not execute disabled interrupt\n");
			exit(1);
			break;
		case INTLVL_LO:
			pmic->regStatus |= STATUS_LOLVLEX;
			break;
		case INTLVL_MED:
			pmic->regStatus |= STATUS_MEDLVLEX;
			break;
		case INTLVL_HI:
			pmic->regStatus |= STATUS_HILVLEX;
			break;
		default:
			fprintf(stderr,"Illegal Interrupt level %d\n",irq->level);
			exit(1);
	}	
	/* Search for higher level ints and evenutally unpost irq signal to cpu */
	Pmic_SearchForInt(pmic); 
        SET_REG_SP(sp);
        CycleCounter+=4;
        SET_REG_PC((irq->irqvect_no) << 1);
}

static void
Pmic_Reti(void *irqData)
{
	PMic *pmic = irqData;
	if(pmic->regStatus & STATUS_HILVLEX) {
		pmic->regStatus &= ~STATUS_HILVLEX;
	} else if(pmic->regStatus & STATUS_MEDLVLEX) {
		pmic->regStatus &= ~STATUS_MEDLVLEX;
	} else if(pmic->regStatus & STATUS_LOLVLEX) {
		pmic->regStatus &= ~STATUS_LOLVLEX;
	}
	/* Would be enough to search selectively */
	Pmic_SearchForInt(pmic); 
}

static uint8_t
status_read(void *clientData,uint32_t address)
{
	PMic *pmic = clientData;
        return pmic->regStatus;
}

/**
 * The status register is writable according to atmel document
 * AVR1008 doc8241.pdf
 */
static void
status_write(void *clientData,uint8_t value,uint32_t address)
{
	PMic *pmic = clientData;
	pmic->regStatus = value;
	// Update something is missing here
}

/**
 ***********************************************************************************
 * intpri is the round robin counter for the low level interrupts.
 * It stops when rren is cleared. Has to be set to 0 manually if
 * normal order is wanted. It contains the number of the last acked
 * low level irq.
 ***********************************************************************************
 */
static uint8_t
intpri_read(void *clientData,uint32_t address)
{
	PMic *pmic = clientData;
        return pmic->regIntPri;
}

static void
intpri_write(void *clientData,uint8_t value,uint32_t address)
{
	PMic *pmic = clientData;
        pmic->regIntPri = value;
	Pmic_SearchForInt(pmic); 
}

/**
 *************************************************************************
 * With the control register the different interrupt levels can be
 * enabled or disabled.
 *************************************************************************
 */
static uint8_t
ctrl_read(void *clientData,uint32_t address)
{
	PMic *pmic = clientData;
        return pmic->regCtrl;
}

static void
ctrl_write(void *clientData,uint8_t value,uint32_t address)
{
	PMic *pmic = clientData;
        pmic->regCtrl = value;
	// update_interrupts() is missing here
}

/**
 ***********************************************************************
 * Insert into linked list with highest priority first.
 ***********************************************************************
 */
static void
Insert_List(PMic *pmic,PMicIrq **listHead,PMicIrq *irq) 
{
	PMicIrq *cursor,*prev;
	if(irq->busy) {
		// return;	
	}
	irq->busy = 1;
	for(prev = NULL,cursor = *listHead;cursor;prev = cursor,cursor = cursor->next) {
		if(irq->irqvect_no > cursor->irqvect_no) {
			break;
		}
	}
	irq->next = cursor;
	irq->prev = prev;
	if(!prev) {
		*listHead = irq;
	} else {
		prev->next = irq;
	}
}

void 
PMIC_PostInterrupt(PMic *pmic,unsigned int intno,unsigned int intlv) 
{
	PMicIrq *irq;
	if(intno >= pmic->nr_ints) {
		fprintf(stderr,"PMIC: Bug, Interrupt number %d is out of range\n",intno);
		exit(1);
	}
	irq = &pmic->allIrqs[intno];	
	switch(intlv) {
		case 0:
			// remove_from lists;
			break;
		case 1:
			Insert_List(pmic,&pmic->loIrqs,irq);	
			break;
		case 2:
			Insert_List(pmic,&pmic->midIrqs,irq);	
			break;
		case 3:
			Insert_List(pmic,&pmic->hiIrqs,irq);
			break;
	}
}

PMic *
XMegaA_PmicNew(const char *name,unsigned int nr_irqvects)
{
	uint32_t base = 0xa0;
	PMic *pmic = sg_new(PMic);
	AVR8_RegisterIntco(Pmic_Interrupt,Pmic_Reti,pmic);
	AVR8_RegisterIOHandler(REG_PMIC_STATUS(base),status_read,status_write,pmic);
	AVR8_RegisterIOHandler(REG_PMIC_INTPRI(base),intpri_read,intpri_write,pmic);
	AVR8_RegisterIOHandler(REG_PMIC_CTRL(base),ctrl_read,ctrl_write,pmic);
	return pmic;
}
