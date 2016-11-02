#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include "signode.h"
#include "sgstring.h"
#include "bus.h"
#include "m16c_cpu.h"
#include "irq_m16c65.h"
#include "cycletimer.h"

#define REG_INT7IC	(0x42)
#define REG_INT6IC	(0x43)
#define REG_INT3IC	(0x44)
#define REG_TB5IC	(0x45)
#define REG_TB4IC	(0x46)
#define REG_TB3IC	(0x47)
#define REG_INT5IC	(0x48)
#define REG_INT4IC	(0x49)
#define REG_BCNIC	(0x4a)
#define REG_DM0IC	(0x4b)
#define REG_DM1IC	(0x4c)
#define REG_KUPIC	(0x4d)
#define REG_ADIC	(0x4e)
#define REG_S2TIC	(0x4f)
#define REG_S2RIC	(0x50)
#define REG_S0TIC	(0x51)
#define REG_S0RIC	(0x52)
#define REG_S1TIC	(0x53)
#define REG_S1RIC	(0x54)
#define REG_TA0IC	(0x55)
#define REG_TA1IC	(0x56)
#define REG_TA2IC	(0x57)
#define REG_TA3IC	(0x58)
#define REG_TA4IC	(0x59)
#define REG_TB0IC	(0x5a)
#define REG_TB1IC	(0x5b)
#define REG_TB2IC	(0x5c)
#define REG_INT0IC	(0x5d)
#define REG_INT1IC	(0x5e)
#define REG_INT2IC	(0x5f)
#define REG_DM2IC	(0x69)
#define REG_DM3IC	(0x6A)
#define REG_U5BCNIC	(0x6B)
#define REG_S5TIC	(0x6C)
#define REG_S5RIC	(0x6D)
#define REG_U6BCNIC	(0x6E)
#define REG_S6TIC	(0x6F)
#define REG_S6RIC	(0x70)
#define REG_U7BCNIC	(0x71)
#define REG_S7TIC	(0x72)
#define REG_S7RIC	(0x73)
#define REG_IICIC	(0x7B)
#define REG_SCLDAIC	(0x7c)

#define	ICR_ILVL_MSK	(7)
#define ICR_IR		(1 << 3)
#define ICR_POL		(1 << 4)

typedef struct M16C_IntCo M16C_IntCo;

typedef struct M16C_Irq {
	M16C_IntCo *intco;
	SigNode *irqInput;
	SigTrace *irqTrace;
	uint8_t reg_icr;
	bool has_polarity_select;
	int int_nr;
} M16C_Irq;

struct M16C_IntCo {
	BusDevice bdev;
	M16C_Irq irq[64];
};

static void
update_interrupt(M16C_IntCo * intco)
{
	int max_ilvl = 0;
	int ilvl;
	int i;
	int int_no = -1;
	M16C_Irq *irq;
	for (i = 0; i < 64; i++) {
		irq = &intco->irq[i];
		if ((irq->reg_icr & ICR_IR) == 0) {
			continue;
		}
		ilvl = irq->reg_icr & ICR_ILVL_MSK;
		if (ilvl > max_ilvl) {
			max_ilvl = ilvl;
			int_no = irq->int_nr;
		}
	}
#if 0
	fprintf(stderr, "Post ilvl %d\n", max_ilvl);
	if (int_no == 54) {
		fprintf(stderr, "Maxlevel by %d at %llu\n", int_no, CycleCounter_Get());
	}
#endif
	M16C_PostILevel(max_ilvl, int_no);
#if 0
	if (max_ilvl > 0) {
		fprintf(stderr, "Poste einen Ilevel von %d, int_no %d\n", max_ilvl, int_no);
		exit(1);
	}
#endif
}

void
M16C65_AckIrq(BusDevice * intco_bdev, uint32_t intno)
{
	M16C_IntCo *intco = (M16C_IntCo *) intco_bdev;
	M16C_Irq *irq;
	if (intno < 64) {
		irq = &intco->irq[intno];
		irq->reg_icr &= ~ICR_IR;
		update_interrupt(intco);
	}
}

static uint32_t
icr_read(void *clientData, uint32_t address, int rqlen)
{
	M16C_Irq *irq = (M16C_Irq *) clientData;
	return irq->reg_icr;
}

static void
icr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	M16C_Irq *irq = (M16C_Irq *) clientData;
	if (irq->has_polarity_select == false) {
		value = value & ~ICR_POL;
	}
	irq->reg_icr = value & (irq->reg_icr | ~ICR_IR);
	update_interrupt(irq->intco);
}

/*
 ****************************************************************
 * int_source_change
 *      irq line trace, called whenever a change occurs
 ****************************************************************
 */
static void
int_source_change(SigNode * node, int value, void *clientData)
{
	M16C_Irq *irq = (M16C_Irq *) clientData;
	if (irq->reg_icr & ICR_POL) {
		if (value == SIG_HIGH) {
			irq->reg_icr |= ICR_IR;
		}
	} else {
		if (value == SIG_LOW) {
			irq->reg_icr |= ICR_IR;
		}
	}
	update_interrupt(irq->intco);
}

static void
M16CIntCo_Unmap(void *owner, uint32_t base, uint32_t mask)
{
	IOH_Delete8(REG_INT7IC);
	IOH_Delete8(REG_INT6IC);
	IOH_Delete8(REG_INT3IC);
	IOH_Delete8(REG_TB5IC);
	IOH_Delete8(REG_TB4IC);
	IOH_Delete8(REG_TB3IC);
	IOH_Delete8(REG_INT5IC);
	IOH_Delete8(REG_INT4IC);
	IOH_Delete8(REG_BCNIC);
	IOH_Delete8(REG_DM0IC);
	IOH_Delete8(REG_DM1IC);
	IOH_Delete8(REG_KUPIC);
	IOH_Delete8(REG_ADIC);
	IOH_Delete8(REG_S2TIC);
	IOH_Delete8(REG_S2RIC);
	IOH_Delete8(REG_S0TIC);
	IOH_Delete8(REG_S0RIC);
	IOH_Delete8(REG_S1TIC);
	IOH_Delete8(REG_S1RIC);
	IOH_Delete8(REG_TA0IC);
	IOH_Delete8(REG_TA1IC);
	IOH_Delete8(REG_TA2IC);
	IOH_Delete8(REG_TA3IC);
	IOH_Delete8(REG_TA4IC);
	IOH_Delete8(REG_TB0IC);
	IOH_Delete8(REG_TB1IC);
	IOH_Delete8(REG_TB2IC);
	IOH_Delete8(REG_INT0IC);
	IOH_Delete8(REG_INT1IC);
	IOH_Delete8(REG_INT2IC);
	IOH_Delete8(REG_DM2IC);
	IOH_Delete8(REG_DM3IC);
	IOH_Delete8(REG_U5BCNIC);
	IOH_Delete8(REG_S5TIC);
	IOH_Delete8(REG_S5RIC);
	IOH_Delete8(REG_U6BCNIC);
	IOH_Delete8(REG_S6TIC);
	IOH_Delete8(REG_S6RIC);
	IOH_Delete8(REG_U7BCNIC);
	IOH_Delete8(REG_S7TIC);
	IOH_Delete8(REG_S7RIC);
	IOH_Delete8(REG_IICIC);
	IOH_Delete8(REG_SCLDAIC);

}

static void
M16CIntCo_Map(void *owner, uint32_t base, uint32_t mask, uint32_t mapflags)
{

	M16C_IntCo *ic = (M16C_IntCo *) owner;

	IOH_New8(REG_INT7IC, icr_read, icr_write, &ic->irq[M16C_INT_INT7]);
	IOH_New8(REG_INT6IC, icr_read, icr_write, &ic->irq[M16C_INT_INT6]);
	IOH_New8(REG_INT3IC, icr_read, icr_write, &ic->irq[M16C_INT_INT3]);
	IOH_New8(REG_TB5IC, icr_read, icr_write, &ic->irq[M16C_INT_TIMER_B5]);
	IOH_New8(REG_TB4IC, icr_read, icr_write, &ic->irq[M16C_INT_TIMER_B4]);
	IOH_New8(REG_TB3IC, icr_read, icr_write, &ic->irq[M16C_INT_TIMER_B3]);
	IOH_New8(REG_INT5IC, icr_read, icr_write, &ic->irq[M16C_INT_INT5]);
	IOH_New8(REG_INT4IC, icr_read, icr_write, &ic->irq[M16C_INT_INT4]);
	IOH_New8(REG_BCNIC, icr_read, icr_write, &ic->irq[M16C_INT_UART2_SSTP]);
	IOH_New8(REG_DM0IC, icr_read, icr_write, &ic->irq[M16C_INT_DMA0]);
	IOH_New8(REG_DM1IC, icr_read, icr_write, &ic->irq[M16C_INT_DMA1]);
	IOH_New8(REG_KUPIC, icr_read, icr_write, &ic->irq[M16C_INT_KEY]);
	IOH_New8(REG_ADIC, icr_read, icr_write, &ic->irq[M16C_INT_ADC]);
	IOH_New8(REG_S2TIC, icr_read, icr_write, &ic->irq[M16C_INT_UART2_TX]);
	IOH_New8(REG_S2RIC, icr_read, icr_write, &ic->irq[M16C_INT_UART2_RX]);
	IOH_New8(REG_S0TIC, icr_read, icr_write, &ic->irq[M16C_INT_UART0_TX]);
	IOH_New8(REG_S0RIC, icr_read, icr_write, &ic->irq[M16C_INT_UART0_RX]);
	IOH_New8(REG_S1TIC, icr_read, icr_write, &ic->irq[M16C_INT_UART1_TX]);
	IOH_New8(REG_S1RIC, icr_read, icr_write, &ic->irq[M16C_INT_UART1_RX]);
	IOH_New8(REG_TA0IC, icr_read, icr_write, &ic->irq[M16C_INT_TIMER_A0]);
	IOH_New8(REG_TA1IC, icr_read, icr_write, &ic->irq[M16C_INT_TIMER_A1]);
	IOH_New8(REG_TA2IC, icr_read, icr_write, &ic->irq[M16C_INT_TIMER_A2]);
	IOH_New8(REG_TA3IC, icr_read, icr_write, &ic->irq[M16C_INT_TIMER_A3]);
	IOH_New8(REG_TA4IC, icr_read, icr_write, &ic->irq[M16C_INT_TIMER_A4]);
	IOH_New8(REG_TB0IC, icr_read, icr_write, &ic->irq[M16C_INT_TIMER_B0]);
	IOH_New8(REG_TB1IC, icr_read, icr_write, &ic->irq[M16C_INT_TIMER_B1]);
	IOH_New8(REG_TB2IC, icr_read, icr_write, &ic->irq[M16C_INT_TIMER_B2]);
	IOH_New8(REG_INT0IC, icr_read, icr_write, &ic->irq[M16C_INT_INT0]);
	IOH_New8(REG_INT1IC, icr_read, icr_write, &ic->irq[M16C_INT_INT1]);
	IOH_New8(REG_INT2IC, icr_read, icr_write, &ic->irq[M16C_INT_INT2]);
	IOH_New8(REG_DM2IC, icr_read, icr_write, &ic->irq[M16C_INT_DMA2]);
	IOH_New8(REG_DM3IC, icr_read, icr_write, &ic->irq[M16C_INT_DMA3]);
	IOH_New8(REG_U5BCNIC, icr_read, icr_write, &ic->irq[M16C_INT_UART5_SSTP]);
	IOH_New8(REG_S5TIC, icr_read, icr_write, &ic->irq[M16C_INT_UART5_TX]);
	IOH_New8(REG_S5RIC, icr_read, icr_write, &ic->irq[M16C_INT_UART5_RX]);
	IOH_New8(REG_U6BCNIC, icr_read, icr_write, &ic->irq[M16C_INT_UART6_SSTP]);
	IOH_New8(REG_S6TIC, icr_read, icr_write, &ic->irq[M16C_INT_UART6_TX]);
	IOH_New8(REG_S6RIC, icr_read, icr_write, &ic->irq[M16C_INT_UART6_RX]);
	IOH_New8(REG_U7BCNIC, icr_read, icr_write, &ic->irq[M16C_INT_UART7_SSTP]);
	IOH_New8(REG_S7TIC, icr_read, icr_write, &ic->irq[M16C_INT_UART7_TX]);
	IOH_New8(REG_S7RIC, icr_read, icr_write, &ic->irq[M16C_INT_UART7_RX]);
	IOH_New8(REG_IICIC, icr_read, icr_write, &ic->irq[M16C_INT_I2C]);
	IOH_New8(REG_SCLDAIC, icr_read, icr_write, &ic->irq[M16C_INT_SCL_SDA]);
}

BusDevice *
M16C65IntCo_New(const char *name)
{
	M16C_IntCo *ic = sg_new(M16C_IntCo);
	int i;
	for (i = 0; i < 64; i++) {
		M16C_Irq *irq = &ic->irq[i];
		irq->irqInput = SigNode_New("%s.irq%d", name, i);
		if (!irq->irqInput) {
			fprintf(stderr, "M16C64: Can not create interrupt %d\n", i);
			exit(1);
		}
		irq->intco = ic;
		irq->int_nr = i;
		irq->irqTrace = SigNode_Trace(irq->irqInput, int_source_change, irq);
		switch (i) {
		    case M16C_INT_INT7:
		    case M16C_INT_INT6:
		    case M16C_INT_INT3:
		    case M16C_INT_INT5:
		    case M16C_INT_INT4:
		    case M16C_INT_INT0:
		    case M16C_INT_INT1:
		    case M16C_INT_INT2:
			    irq->has_polarity_select = true;
			    break;
		    default:
			    irq->has_polarity_select = false;
			    break;
		}
	}
	ic->bdev.first_mapping = NULL;
	ic->bdev.Map = M16CIntCo_Map;
	ic->bdev.UnMap = M16CIntCo_Unmap;
	ic->bdev.owner = ic;
	ic->bdev.hw_flags = MEM_FLAG_WRITABLE | MEM_FLAG_READABLE;
	return &ic->bdev;
}
