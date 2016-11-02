#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "signode.h"
#include "sgstring.h"
#include "bus.h"
#include "m16c_cpu.h"
#include "irq_r8c23.h"
#include "cycletimer.h"

#define REG_C01WKIC 	(0x0043)
#define REG_C0RECIC	(0x0044)
#define REG_C0TRMIC	(0x0045)
#define REG_C01ERRIC	(0x0046)
#define REG_TREIC	(0x004A)
#define REG_KUPIC	(0x004D)
#define REG_ADIC	(0x004E)
#define REG_S0TIC	(0x0051)
#define REG_S0RIC	(0x0052)
#define REG_S1TIC	(0x0053)
#define REG_S1RIC	(0x0054)
#define REG_TRAIC	(0x0056)
#define REG_TRBIC	(0x0058)
#define REG_TRD0IC 	(0x0048)
#define REG_TRD1IC	(0x0049)
#define REG_SSUIC	(0x004F)
#define REG_INT2IC	(0x0055)
#define REG_INT1IC	(0x0059)
#define REG_INT3IC	(0x005A)
#define REG_INT0IC	(0x005D)

#define	ICR_ILVL_MSK	(7)
#define ICR_IR		(1 << 3)

typedef struct R8C_IntCo R8C_IntCo; 

typedef struct R8C_Irq {
	R8C_IntCo *intco;
	SigNode *irqInput;
	SigTrace *irqTrace;
	uint8_t reg_icr;
	int int_nr;
} R8C_Irq;

struct R8C_IntCo {
	BusDevice bdev;
	R8C_Irq irq[32];
};

static void
update_interrupt(R8C_IntCo *intco) 
{
	int max_ilvl = 0;
	int ilvl;
	int i;
	int int_no = -1;
	R8C_Irq *irq;
	for(i = 0; i < 32; i ++) {
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
	#if 0
	if(int_no == 54) {
	 	fprintf(stderr,"Maxlevel by %d at %llu\n",int_no,CycleCounter_Get());
	}
	#endif
	M16C_PostILevel(max_ilvl,int_no);	
#if 0
	if(max_ilvl > 0) {
		fprintf(stderr,"Poste einen Ilevel von %d, int_no %d\n",max_ilvl,int_no);
		exit(1);
	}
#endif
}

void
R8C23_AckIrq(BusDevice *intco_bdev,uint32_t intno) 
{
	R8C_IntCo *intco = (R8C_IntCo *) intco_bdev;
	R8C_Irq *irq;
	if(intno < 32) {
		irq = &intco->irq[intno];
		irq->reg_icr &=  ~ICR_IR;
		update_interrupt(intco); 
	}
}

static uint32_t
icr_read(void *clientData,uint32_t address,int rqlen)
{
	R8C_Irq *irq = (R8C_Irq *) clientData;
        return irq->reg_icr;
}

static void
icr_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	R8C_Irq *irq = (R8C_Irq *) clientData;
        irq->reg_icr = value | (irq->reg_icr & ICR_IR);
	update_interrupt(irq->intco);
}


/*
 ****************************************************************
 * int_source_change
 *      irq line trace, called whenever a change occurs
 ****************************************************************
 */
static void
int_source_change(SigNode *node,int value,void *clientData)
{
        R8C_Irq *irq = (R8C_Irq*) clientData;
        if((value == SIG_LOW) || (value == SIG_PULLDOWN)) {
		irq->reg_icr |= ICR_IR;
        } 
       	update_interrupt(irq->intco);
}

static void
R8CIntCo_Unmap(void *owner,uint32_t base,uint32_t mask)
{
	IOH_Delete8(REG_C01WKIC);
	IOH_Delete8(REG_C0RECIC);
	IOH_Delete8(REG_C0TRMIC);
	IOH_Delete8(REG_C01ERRIC);
	IOH_Delete8(REG_TRD0IC);
	IOH_Delete8(REG_TRD1IC);
	IOH_Delete8(REG_TREIC);
	IOH_Delete8(REG_KUPIC);
	IOH_Delete8(REG_ADIC);
	IOH_Delete8(REG_SSUIC);
	IOH_Delete8(REG_S0TIC);
	IOH_Delete8(REG_S0RIC);
	IOH_Delete8(REG_S1TIC);
	IOH_Delete8(REG_S1RIC);
	IOH_Delete8(REG_TRAIC);
	IOH_Delete8(REG_TRBIC);
	IOH_Delete8(REG_INT2IC);
	IOH_Delete8(REG_INT1IC);
	IOH_Delete8(REG_INT3IC);
	IOH_Delete8(REG_INT0IC);

}

static void
R8CIntCo_Map(void *owner,uint32_t base,uint32_t mask,uint32_t mapflags)
{

	R8C_IntCo *ic = (R8C_IntCo *) owner;

	IOH_New8(REG_C01WKIC,icr_read,icr_write,&ic->irq[R8C_INT_CAN0_WAKE_UP]); 
	IOH_New8(REG_C0RECIC,icr_read,icr_write,&ic->irq[R8C_INT_CAN0_RX]);
	IOH_New8(REG_C0TRMIC,icr_read,icr_write,&ic->irq[R8C_INT_CAN0_TX]);
	IOH_New8(REG_C01ERRIC,icr_read,icr_write,&ic->irq[R8C_INT_CAN0_ERR]);
	IOH_New8(REG_TRD0IC,icr_read,icr_write,&ic->irq[R8C_INT_TIMER_RD0]);
	IOH_New8(REG_TRD1IC,icr_read,icr_write,&ic->irq[R8C_INT_TIMER_RD1]);
	IOH_New8(REG_TREIC,icr_read,icr_write,&ic->irq[R8C_INT_TIMER_RE]);
	IOH_New8(REG_KUPIC,icr_read,icr_write,&ic->irq[R8C_INT_KEY]);
	IOH_New8(REG_ADIC,icr_read,icr_write,&ic->irq[R8C_INT_AD]);
	IOH_New8(REG_SSUIC,icr_read,icr_write,&ic->irq[R8C_INT_SSI]);
	IOH_New8(REG_S0TIC,icr_read,icr_write,&ic->irq[R8C_INT_UART0_TX]);
	IOH_New8(REG_S0RIC,icr_read,icr_write,&ic->irq[R8C_INT_UART0_RX]);
	IOH_New8(REG_S1TIC,icr_read,icr_write,&ic->irq[R8C_INT_UART1_TX]);
	IOH_New8(REG_S1RIC,icr_read,icr_write,&ic->irq[R8C_INT_UART1_RX]);
	IOH_New8(REG_TRAIC,icr_read,icr_write,&ic->irq[R8C_INT_TIMER_RA]);
	IOH_New8(REG_TRBIC,icr_read,icr_write,&ic->irq[R8C_INT_TIMER_RB]);
	IOH_New8(REG_INT2IC,icr_read,icr_write,&ic->irq[R8C_INT_INT2]);
	IOH_New8(REG_INT1IC,icr_read,icr_write,&ic->irq[R8C_INT_INT1]);
	IOH_New8(REG_INT3IC,icr_read,icr_write,&ic->irq[R8C_INT_INT3]);
	IOH_New8(REG_INT0IC,icr_read,icr_write,&ic->irq[R8C_INT_INT0]);
}


BusDevice * 
R8C23IntCo_New(const char *name) 
{
	R8C_IntCo *ic = sg_new(R8C_IntCo);
	int i;
	for(i = 0;i < 32; i++) {
		R8C_Irq *irq = &ic->irq[i];	
		irq->irqInput = SigNode_New("%s.irq%d",name,i);
		if(!irq->irqInput) {
			fprintf(stderr,"R8C: Can not create interrupt %d\n",i);
			exit(1);
		}
		irq->intco = ic;
		irq->int_nr = i;
		irq->irqTrace = SigNode_Trace(irq->irqInput,int_source_change,irq);
	}
	ic->bdev.first_mapping = NULL;
        ic->bdev.Map = R8CIntCo_Map;
        ic->bdev.UnMap = R8CIntCo_Unmap;
        ic->bdev.owner = ic;
        ic->bdev.hw_flags = MEM_FLAG_WRITABLE | MEM_FLAG_READABLE;
	return &ic->bdev;
}
