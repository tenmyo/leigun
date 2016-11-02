/*
 **************************************************************************************************
 *
 * Emulation of AT91 Advanced Interrupt Controller (AIC) 
 *
 * state: working but FIQ never tested 
 *
 *
 * Copyright 2006 Jochen Karrer. All rights reserved.
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
#include "bus.h"
#include "signode.h"
#include "configfile.h"
#include "at91_aic.h"
#include "sgstring.h"

#if 0
#define dbgprintf(x...) { fprintf(stderr,x); }
#else
#define dbgprintf(x...)
#endif

#define AIC_SMR(base,n) ((base)+0x04*(n))
#define		SMR_SRCTYPE_MASK	(3<<5)
#define		SMR_SRCTYPE_SHIFT	(5)
#define		    SRCTYPE_LOW		(0<<5)
#define		    SRCTYPE_NEGEDGE	(1<<5)
#define		    SRCTYPE_HIGH	(2<<5)
#define		    SRCTYPE_POSEDGE	(3<<5)
#define		SMR_PRIOR_MASK		(7<<0)
#define		SMR_PRIOR_SHIFT		(0)
#define AIC_SVR(base,n) ((base)+0x80+0x04*(n))
#define	AIC_IVR(base)	((base)+0x100)
#define AIC_FVR(base)	((base)+0x104)
#define AIC_ISR(base)	((base)+0x108)
#define AIC_IPR(base)	((base)+0x10c)
#define AIC_IMR(base)	((base)+0x110)
#define AIC_CISR(base)	((base)+0x114)
#define	AIC_IECR(base)	((base)+0x120)
#define	AIC_IDCR(base) 	((base)+0x124)
#define AIC_ICCR(base)	((base)+0x128)
#define AIC_ISCR(base)	((base)+0x12c)
#define	AIC_EOICR(base)	((base)+0x130)
#define AIC_SPU(base)	((base)+0x134)
#define AIC_DCR(base)	((base)+0x138)
#define	 	DCR_GMSK	(1<<1)
#define	 	DCR_PROT	(1<<0)

typedef struct IrqTraceInfo IrqTraceInfo;

typedef struct AT91Aic {
        BusDevice bdev;
        SigNode *irqOut;
	int interrupt_posted;
        SigNode *fiqOut;
	int finterrupt_posted;

	SigNode *irqIn[32];
	IrqTraceInfo *traceInfo[32];
	int stack_irqn[8]; 
	int stack_irqlvl[8];
	int stackptr;
	int curplvl; /* current interrupts priority level */

	uint32_t regSMR[32];
	uint32_t regSVR[32];
	uint32_t regISR;
	uint32_t isr_memorized; /* for PROT mode */ 
	uint32_t regIPR;
	uint32_t edge_ipr;
	uint32_t edge_mask;
	uint32_t level_ipr;
	uint32_t regIMR;
	uint32_t regSPU;
	uint32_t regDCR;
} AT91Aic;

struct IrqTraceInfo {
        int irqnr;
        AT91Aic *aic;
};

/*
 * ----------------------------------------------------------------------
 * Push the current privilege level and active interrupt onto a
 * stack to make room for a higher level interrupt
 * The first interrupt pushes curplvl = -1 and isr = 0 onto the stack
 * v0
 * ----------------------------------------------------------------------
 */
static void
push_current_on_stack(AT91Aic *aic) 
{
	int sp = aic->stackptr;
	if(sp > 7) {
		fprintf(stderr,"AT91Aic emulator bug: Interrupt stack overflow\n");
		exit(1);
	}
	dbgprintf("Push %d\n",aic->curplvl);
	aic->stack_irqlvl[sp] = aic->curplvl;
	aic->stack_irqn[sp] = aic->regISR;
	aic->stackptr++;
}

/*
 * -----------------------------------------------------------------------
 * pop_from_stack
 *	Restore the old privilege level and isr from stack
 * v0
 * -----------------------------------------------------------------------
 */
static void
pop_from_stack(AT91Aic *aic) 
{
	int sp = aic->stackptr - 1;
	if(sp >= 0) {
		aic->stackptr = sp;
		aic->curplvl = aic->stack_irqlvl[sp];
		aic->regISR = aic->stack_irqn[sp];
	}
	if(sp <= 0) {
		if((aic->curplvl != -1) || (aic->regISR != 0)) {
			fprintf(stderr,"AT91Aic emu Bug: privilege level or isr wrong in pop from stack\n"); 
			exit(1);
		}
	}
}

static inline void
update_ipr(AT91Aic *aic)
{
	aic->regIPR = (aic->level_ipr & ~aic->edge_mask) 
		    | (aic->edge_ipr & aic->edge_mask);
}
/*
 * ----------------------------------------------------------------------
 * Find the highest priority interrupt (starting with 1 because 0 is fiq)
 * Take the first one if more than one has the same plvl 
 * return < 0 if not found
 * v0
 * -----------------------------------------------------------------------
 */
static int 
highest_priority_irq(AT91Aic *aic,int *plevel_ret) 
{
	int i;
	int plevel;
	int irq = -1;
	int maxlevel=-1;
	update_ipr(aic);
	uint32_t pending = aic->regIPR & aic->regIMR;
	for(i = 1;i < 32;i++) {
		if(pending & (1 << i)) {
			plevel = aic->regSMR[i] & SMR_PRIOR_MASK;	
			if(plevel > maxlevel) {
				maxlevel = plevel;
				irq = i;
			}
			//fprintf(stderr,"Pending %u, plevel %u maxlevel %u\n",i,plevel,maxlevel);
		}
	}
	*plevel_ret = maxlevel;
	return irq;
}

static void
update_interrupts(AT91Aic *aic) 
{
	int interrupt = 0;
	int maxlevel = -2;
	int irq = -1;
	if(aic->regIPR & aic->regIMR) { /* & ~1 for fiq ? */
		irq = highest_priority_irq(aic,&maxlevel);
		if((maxlevel > aic->curplvl)) {
			interrupt = 1;
		}
	}
#if 0
	if(irq  > 1) {
		fprintf(stderr,"UDInt %d to %d cpl %d lvl %d ipr %04x imr %04x\n",irq,interrupt,aic->curplvl,maxlevel,aic->regIPR,aic->regIMR);
	}
#endif
	if(unlikely(aic->regDCR & DCR_GMSK)) {
		return;
	}
	if(interrupt) {
		if(!aic->interrupt_posted) {
			SigNode_Set(aic->irqOut,SIG_LOW);
			aic->interrupt_posted = 1;
		}
	} else {
		if(aic->interrupt_posted) {	
			SigNode_Set(aic->irqOut,SIG_HIGH);
			aic->interrupt_posted = 0;
		}
	}
	if(aic->regIPR & aic->regIMR & 1) {
		SigNode_Set(aic->fiqOut,SIG_LOW);
	} else {
		SigNode_Set(aic->fiqOut,SIG_HIGH);
	}
}

static inline uint32_t 
srctype_translate(uint32_t srctype,int irq) 
{
	if((irq >= 1) && (irq <= 25)) {
		srctype |= (1<<6);
	}
	/* Usb host port is low active */
	if(irq == 23) {
		srctype &= ~(1<<6);
	} 
	return srctype;
}
/*
 * -----------------------------------------------------------
 * int_source_change
 *      irq line trace, called whenever a change occurs
 * v0
 * -----------------------------------------------------------
 */
static void 
int_source_change(SigNode *node,int value,void *clientData)
{
        IrqTraceInfo *ti = (IrqTraceInfo *) clientData;
        AT91Aic *aic = ti->aic;
        int irq = ti->irqnr;
	uint32_t srctype = aic->regSMR[irq] & SMR_SRCTYPE_MASK;
	/* internal is always high level */
	srctype = srctype_translate(srctype,irq);
#if 0
	if((irq  == 7) || (irq == 1)) {
		fprintf(stderr,"irq %d changed to %d, type %d\n",irq,value,srctype);
	}
#endif
        if((value == SIG_LOW)) {
		if(srctype == SRCTYPE_LOW)  {
			aic->level_ipr |= (1<<irq);
		} else if(srctype == SRCTYPE_NEGEDGE) {
			aic->edge_ipr |= (1<<irq);
		} else if(srctype == SRCTYPE_HIGH) {
			aic->level_ipr &= ~(1<<irq);
		}
        } else if(value == SIG_HIGH) {
		if(srctype == SRCTYPE_HIGH) {
			aic->level_ipr |= (1<<irq);	
		} else if(srctype == SRCTYPE_POSEDGE) {
			aic->edge_ipr |= (1<<irq);
		} else if(srctype == SRCTYPE_LOW) {
			aic->level_ipr &= ~(1<<irq);
		}
        }
	update_ipr(aic);
        update_interrupts(aic);
        return;
}

/*
 * ---------------------------------------------------------------------
 * SMR - Source mode register
 * ---------------------------------------------------------------------
 */

static uint32_t
smr_read(void *clientData,uint32_t address,int rqlen)
{
	AT91Aic *aic = (AT91Aic *)clientData;
	int index = (address >> 2)& 0x1f;
        return aic->regSMR[index];
}


static void
smr_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	AT91Aic *aic = (AT91Aic *)clientData;
	int index = (address >> 2)& 0x1f;
	uint32_t srctype = value & SMR_SRCTYPE_MASK;
	uint32_t dsrctype;
	dsrctype = srctype ^ (aic->regSMR[index] & SMR_SRCTYPE_MASK);
	srctype = srctype_translate(srctype,index); 
	aic->regSMR[index] = value & (SMR_SRCTYPE_MASK | SMR_PRIOR_MASK);
	if(dsrctype && (srctype == SRCTYPE_HIGH)) {
		if(SigNode_Val(aic->irqIn[index]) == SIG_LOW) {
			aic->level_ipr &= ~(1<<index);			
		} else if(SigNode_Val(aic->irqIn[index]) == SIG_HIGH) {
			aic->level_ipr |= (1<<index);			
		}
		aic->edge_mask &= ~(1<<index);
	} else if(dsrctype && (srctype == SRCTYPE_LOW)) {
		if(SigNode_Val(aic->irqIn[index]) == SIG_LOW) {
			aic->level_ipr |= (1<<index);			
		} else if(SigNode_Val(aic->irqIn[index]) == SIG_HIGH) {
			aic->level_ipr &= ~(1<<index);			
		}
		aic->edge_mask &= ~(1<<index);
	} else if(dsrctype) {
		aic->edge_mask |= (1<<index);
	}
	update_ipr(aic);
	update_interrupts(aic);
}

/*
 * ---------------------------------------------------------
 * Source vector register
 * This address can be read form ivr when the interrupt
 * is the currently active one.
 * ---------------------------------------------------------
 */
static uint32_t
svr_read(void *clientData,uint32_t address,int rqlen)
{
	AT91Aic *aic = (AT91Aic *)clientData;
	int index = (address >> 2) & 0x1f;
        return aic->regSVR[index];
}


static void
svr_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	AT91Aic *aic = (AT91Aic *)clientData;
	int index = (address >> 2) & 0x1f;
	aic->regSVR[index] = value;
}

/*
 * --------------------------------------------------------------------------
 * Interrupt vector register: contains the vector programmed into the SRV
 * corresponding to the current interrupt. If there is no current intterrupt
 * read SPU
 * --------------------------------------------------------------------------
 */
static uint32_t
ivr_read(void *clientData,uint32_t address,int rqlen)
{
	AT91Aic *aic = (AT91Aic *)clientData;
	int irq,plevel;
	irq = highest_priority_irq(aic,&plevel);
	if(irq<0) {
		return aic->regSPU;
	}
	if(plevel <= aic->curplvl) {
		fprintf(stderr,"IVR read with no new irq\n");
		return aic->regSVR[irq];
	}

	/* If read from a debugger do not modify the state of the Interruptcontroller */
	if(!(aic->regDCR & DCR_PROT)) {
		push_current_on_stack(aic);
		aic->curplvl = aic->regSMR[irq] & SMR_PRIOR_MASK;	
		/* clear the current interrupt if edge */
		aic->edge_ipr &= ~(1<<irq);
		update_ipr(aic);
		update_interrupts(aic);
	} else {
		if(aic->regISR != irq) {
			aic->isr_memorized = aic->regISR;
		}
	}
	aic->regISR = irq;
	dbgprintf("IVR read 0x%08x\n",aic->regSVR[irq]);
	return aic->regSVR[irq];
}


static void
ivr_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	AT91Aic *aic = (AT91Aic *)clientData;
	int irq;
	if(!(aic->regDCR & DCR_PROT)) {
        	fprintf(stderr,"AT91Aic: IVR is only writable in protected mode\n");
		return;
	} 
	irq = aic->regISR;

	/* Hack */
	if(aic->regISR != aic->isr_memorized) {
		aic->regISR = aic->isr_memorized;
		push_current_on_stack(aic);
		aic->regISR = irq;
	}
	aic->curplvl = aic->regSMR[irq] & SMR_PRIOR_MASK;	
	aic->edge_ipr &= ~(1<<irq);
	update_ipr(aic);
	update_interrupts(aic);
}

/*
 * ------------------------------------------------------------------------
 * Contains the value from SVR0 when src 0 is active. When there is
 * no fast interrupt read SPU 
 * ------------------------------------------------------------------------
 */
static uint32_t
fvr_read(void *clientData,uint32_t address,int rqlen)
{
	AT91Aic *aic = (AT91Aic *)clientData;
	aic->edge_ipr &= ~(1<<0);
	update_ipr(aic);
	if(aic->regIPR & aic->regIMR & 1) {
		return aic->regSVR[0];
	} else {
		return aic->regSPU;
	}
}


static void
fvr_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
        fprintf(stderr,"AT91Aic: FVR is readonly\n");

}

/*
 * -----------------------------------------------------------------------
 * ISR: return the current interrupt source number
 * -----------------------------------------------------------------------
 */
static uint32_t
isr_read(void *clientData,uint32_t address,int rqlen)
{
	AT91Aic *aic = (AT91Aic *) clientData;
        return aic->regISR;
}


static void
isr_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
        fprintf(stderr,"AT91Aic: ISR is a writeonly register\n");;

}

/*
 * --------------------------------------------------------------------------
 * IPR: Interrupt pending register. Bitfield displaying if an interrupt is
 * pending (before masking by imr)
 * --------------------------------------------------------------------------
 */
static uint32_t
ipr_read(void *clientData,uint32_t address,int rqlen)
{
	AT91Aic *aic = (AT91Aic*) clientData;
	update_ipr(aic);
        return aic->regIPR;
}


static void
ipr_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
        fprintf(stderr,"AT91Aic: IPR is not writable\n");

}

/*
 * Interrupt Mask register
 */
static uint32_t
imr_read(void *clientData,uint32_t address,int rqlen)
{
	AT91Aic *aic = (AT91Aic*) clientData;
        return aic->regIMR;
}

static void
imr_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
        fprintf(stderr,"AT91Aic: IMR is not writable\n");

}

/*
 * -----------------------------------------------
 * Core interrupt status register
 * -----------------------------------------------
 */
static uint32_t
cisr_read(void *clientData,uint32_t address,int rqlen)
{
	AT91Aic *aic = (AT91Aic*) clientData;
	uint32_t val = 0;
	if(SigNode_Val(aic->irqOut) == SIG_HIGH) {
		val |= 2;
	}
	if(SigNode_Val(aic->fiqOut) == SIG_HIGH) {
		val |= 1;
	}
        return val;
}


static void
cisr_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
        fprintf(stderr,"AT91Aic: CISR is not writable\n");

}

/* 
 * ------------------------------------------------------------------------
 * AIC Interrupt Enable Command Register
 * Set the corresponding bit in the interrupt mask register or leave it
 * untouched
 * ------------------------------------------------------------------------
 */
static uint32_t
iecr_read(void *clientData,uint32_t address,int rqlen)
{
        fprintf(stderr,"AT91Aic: IECR is writeonly\n");
        return 0;
}


static void
iecr_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	AT91Aic *aic = (AT91Aic *)clientData;
	aic->regIMR |= value;
	update_interrupts(aic);
}

/* 
 * ------------------------------------------------------------------------
 * AIC Interrupt Disable Command Register
 * clear the corresponding bit in the interrupt mask register or leave it
 * untouched
 * ------------------------------------------------------------------------
 */
static uint32_t
idcr_read(void *clientData,uint32_t address,int rqlen)
{
        fprintf(stderr,"AT91Aic: IDCR is writeonly\n");
        return 0;
}


static void
idcr_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	AT91Aic *aic = (AT91Aic *)clientData;
	aic->regIMR &= ~value;
	update_interrupts(aic);
}

/*
 * ---------------------------------------------------------------------------
 * ICCR register:
 * 	Clear the corresponding bit in the 
 *	edge detector and probably in the Interrupt Pending register
 * ---------------------------------------------------------------------------
 */
static uint32_t
iccr_read(void *clientData,uint32_t address,int rqlen)
{
        fprintf(stderr,"AT91Aic: ICCR is writeonly\n");
        return 0;
}


static void
iccr_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	AT91Aic *aic = (AT91Aic *)clientData;
	aic->edge_ipr &= ~value;
	update_ipr(aic);
	/* level irqs should remain */
	update_interrupts(aic);
}

/*
 * --------------------------------------------------------------
 *  ISCR register
 *	Set the corresponding interrupt in the IPR
 * --------------------------------------------------------------
 */
static uint32_t
iscr_read(void *clientData,uint32_t address,int rqlen)
{
        fprintf(stderr,"AT91Aic: ISCR is writeonly\n") ;
        return 0;
}


static void
iscr_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	AT91Aic *aic = (AT91Aic *)clientData;
	aic->edge_ipr |= value;
	update_ipr(aic);
	update_interrupts(aic);
}

/* 
 * -----------------------------------------------------------
 * EOICR
 *	Signal the end of an interrupt treatment
 * -----------------------------------------------------------
 */
static uint32_t
eoicr_read(void *clientData,uint32_t address,int rqlen)
{
        fprintf(stderr,"AT91Aic: reading from write only register EOICR\n");
        return 0;
}


static void
eoicr_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	AT91Aic *aic = (AT91Aic *)clientData;
	pop_from_stack(aic);
	update_interrupts(aic);

}

static uint32_t
spu_read(void *clientData,uint32_t address,int rqlen)
{
	AT91Aic *aic = (AT91Aic *) clientData;
        return aic->regSPU;
}


static void
spu_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	AT91Aic *aic = (AT91Aic *) clientData;
	aic->regSPU = value;

}

/* 
 * ----------------------------------------------------------
 * DCR: Debug control register
 * ----------------------------------------------------------
 */
static uint32_t
dcr_read(void *clientData,uint32_t address,int rqlen)
{
	AT91Aic *aic = (AT91Aic *) clientData;
        return aic->regDCR;
}


static void
dcr_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	AT91Aic *aic = (AT91Aic *) clientData;
	uint32_t diff = aic->regDCR ^ value;
	aic->regDCR = value & 3;
	if(value & DCR_PROT)  {
        	fprintf(stderr,"AT91Aic: DCR register not fully implemented\n");
	}
	if(value & diff & DCR_GMSK) {
		SigNode_Set(aic->fiqOut,SIG_HIGH);
		SigNode_Set(aic->irqOut,SIG_HIGH);
		aic->interrupt_posted = 0;
	} else if(diff & DCR_GMSK) {
		update_interrupts(aic);
	}
}

static void
AT91Aic_Map(void *owner,uint32_t base,uint32_t mask,uint32_t flags)
{
        AT91Aic *aic = (AT91Aic*) owner;
	int i;
	for(i=0;i<32;i++) {
        	IOH_New32(AIC_SMR(base,i),smr_read,smr_write,aic);
        	IOH_New32(AIC_SVR(base,i),svr_read,svr_write,aic);
	}
	IOH_New32(AIC_IVR(base),ivr_read,ivr_write,aic);	
	IOH_New32(AIC_FVR(base),fvr_read,fvr_write,aic);
	IOH_New32(AIC_ISR(base),isr_read,isr_write,aic);
	IOH_New32(AIC_IPR(base),ipr_read,ipr_write,aic);
	IOH_New32(AIC_IMR(base),imr_read,imr_write,aic);
	IOH_New32(AIC_CISR(base),cisr_read,cisr_write,aic);
	IOH_New32(AIC_IECR(base),iecr_read,iecr_write,aic);
	IOH_New32(AIC_IDCR(base),idcr_read,idcr_write,aic);
	IOH_New32(AIC_ICCR(base),iccr_read,iccr_write,aic);
	IOH_New32(AIC_ISCR(base),iscr_read,iscr_write,aic);
	IOH_New32(AIC_EOICR(base),eoicr_read,eoicr_write,aic);
	IOH_New32(AIC_SPU(base),spu_read,spu_write,aic);
	IOH_New32(AIC_DCR(base),dcr_read,dcr_write,aic);
}

static void
AT91Aic_UnMap(void *owner,uint32_t base,uint32_t mask)
{
	int i;
	for(i=0;i<32;i++) {
        	IOH_Delete32(AIC_SMR(base,i));
        	IOH_Delete32(AIC_SVR(base,i));
	}
	IOH_Delete32(AIC_IVR(base));	
	IOH_Delete32(AIC_FVR(base));
	IOH_Delete32(AIC_ISR(base));
	IOH_Delete32(AIC_IPR(base));
	IOH_Delete32(AIC_IMR(base));
	IOH_Delete32(AIC_CISR(base));
	IOH_Delete32(AIC_IECR(base));
	IOH_Delete32(AIC_IDCR(base));
	IOH_Delete32(AIC_ICCR(base));
	IOH_Delete32(AIC_ISCR(base));
	IOH_Delete32(AIC_EOICR(base));
	IOH_Delete32(AIC_SPU(base));
	IOH_Delete32(AIC_DCR(base));

}

BusDevice *
AT91Aic_New(const char *name)
{
        AT91Aic *aic = sg_new(AT91Aic);
	int i;
        aic->irqOut = SigNode_New("%s.irq",name);
        aic->fiqOut = SigNode_New("%s.fiq",name);
        if(!aic->irqOut || !aic->fiqOut) {
                fprintf(stderr,"AT91Aic: Can not create interrupt signal lines\n");
		exit(1);
        }
	SigNode_Set(aic->irqOut,SIG_HIGH);
	SigNode_Set(aic->fiqOut,SIG_HIGH);
	aic->interrupt_posted = 0;
	aic->curplvl = -1;
	aic->stackptr = 0;
	for(i=0;i<32;i++) {
		IrqTraceInfo *ti = sg_new(IrqTraceInfo);
		ti->irqnr = i;
		ti->aic = aic;
		aic->traceInfo[i]  = ti;
		aic->irqIn[i] = SigNode_New("%s.irq%d",name,i);
		if(!aic->irqIn[i]) {
                	fprintf(stderr,"AT91Aic: Can't create interrupt input\n");
			exit(1);
		}
		SigNode_Trace(aic->irqIn[i],int_source_change,ti);
	}

	/* 
	   All aic registers have a reset value of 0 (except ipr which
	   depends on inputs)
	*/

        aic->bdev.first_mapping=NULL;
        aic->bdev.Map=AT91Aic_Map;
        aic->bdev.UnMap=AT91Aic_UnMap;
        aic->bdev.owner=aic;
        aic->bdev.hw_flags=MEM_FLAG_WRITABLE|MEM_FLAG_READABLE;
        fprintf(stderr,"AT91 AIC \"%s\" created\n",name);
        return &aic->bdev;
}

