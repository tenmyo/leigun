/**
 ****************************************************************
 * AT89C51 Interrupt logic
 * doc4182 page 160
 ****************************************************************
 */

#include "sgstring.h"
#include "sglib.h"
#include "cpu_mcs51.h"
#include "ints_at89c51.h"
#include "signode.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#if 0
#define dbgprintf(...) { fprintf(stderr,__VA_ARGS__); }
#else
#define dbgprintf(...)
#endif

#define REG_IEN0	(0xa8)
#define REG_IEN1	(0xe8)
#define REG_IPL0	(0xb8)
#define REG_IPL1	(0xf8)
#define REG_IPH0	(0xb7)
#define REG_IPH1	(0xf7)

typedef struct Interrupt {
	const char *name;
	const char *ackName;
	int index;
	uint16_t vectAddr;
} Interrupt;

static Interrupt ints[] = {
	{
	 .name = "intX0",
	 .index = 0,
	 .vectAddr = 0x0003,
	 },
	{
	 .name = "intT0",
	 .ackName = "ackIntT0",
	 .index = 1,
	 .vectAddr = 0x000b,
	 },
	{
	 .name = "intX1",
	 .index = 2,
	 .vectAddr = 0x0013,
	 },
	{
	 .name = "intT1",
	 .ackName = "ackIntT1",
	 .index = 3,
	 .vectAddr = 0x001b,
	 },
	{
	 .name = "intS",
	 .index = 4,
	 .vectAddr = 0x0023,
	 },
	{
	 .name = "intT2",
	 .index = 5,
	 .vectAddr = 0x002B,
	 },
	{
	 .name = "intPCA",
	 .index = 6,
	 .vectAddr = 0x0033,
	 },
	{
	 .name = "intCAN",
	 .index = 8,
	 .vectAddr = 0x003b,
	 },
	{
	 .name = "intADC",
	 .index = 9,
	 .vectAddr = 0x0043,
	 },
	{
	 .name = "intTIM",
	 .index = 10,
	 .vectAddr = 0x004b,
	 },
	{
	 .name = "intSPI",
	 .index = 11,
	 .vectAddr = 0x0053,
	 }
};

typedef struct AT89C51Intco {
	uint16_t regIPL;
	uint16_t regIPH;
	uint16_t regIEN;
	Interrupt *irqOutstanding;
	SigNode *sigAckIn;	/* Coming from the CPU */
	SigNode *sigInt[16];
	SigNode *sigAckInt[16];
	SigTrace *traceInt[16];
	Interrupt *interrupt[16];
} AT89C51Intco;

/**
 *************************************************************
 * find the interrupt
 *************************************************************
 */
static Interrupt *
find_interrupt(AT89C51Intco * intco, uint8_t * iplRet)
{
	uint8_t maxIpl = 0;
	uint8_t ipl;
	uint8_t ien;
	Interrupt *irq = NULL;
	int i;
	if (((intco->regIEN >> 7) & 1) == 0) {
		return NULL;
	}
	for (i = 15; i >= 0; i--) {
		if (!intco->sigInt[i]) {
			continue;
		}
		if (SigNode_Val(intco->sigInt[i]) == SIG_HIGH) {
			continue;
		}
		ien = (intco->regIEN >> i) & 1;
		if (!ien) {
			continue;
		}
		ipl = ((intco->regIPH >> i) & 1) << 1;
		ipl |= (intco->regIPL >> i) & 1;
		if (ipl > maxIpl) {
			maxIpl = ipl;
		}
		if (ipl == maxIpl) {
			irq = intco->interrupt[i];
			dbgprintf("Interrupt index %u\n", i);
		}
		//*iplRet = maxIpl;
	}
	if (irq) {
		*iplRet = maxIpl;
	} else {
		*iplRet = 0;
	}
	dbgprintf("Find Interrupt found ipl %u\n", maxIpl);
	return irq;
}

/**
 *****************************************************************
 * \fn static void update_interrupt(AT89C51Intco *intco) 
 *****************************************************************
 */
static void
update_interrupt(AT89C51Intco * intco)
{
	Interrupt *irq;
	uint8_t ipl;
	irq = find_interrupt(intco, &ipl);
	if (irq) {
		dbgprintf("Post ILVL %u, vect 0x%04x\n", ipl, irq->vectAddr);
		intco->irqOutstanding = irq;
		MCS51_PostILvl(ipl, irq->vectAddr);
	} else {
		dbgprintf("UnPost INT\n");
		intco->irqOutstanding = NULL;
		MCS51_PostILvl(-1, 0);
	}
}

static void
sig_int_trace(SigNode * sig, int value, void *eventData)
{
	AT89C51Intco *intco;
	intco = eventData;
	dbgprintf("Triggered sigIntTrace\n");
	update_interrupt(intco);
}

static void
update_signal_traces(AT89C51Intco * intco)
{
	bool enabled;
	int i;
	uint8_t ien;
	for (i = 15; i >= 0; i--) {
		if (!intco->sigInt[i]) {
			continue;
		}
		ien = (intco->regIEN >> i) & 1;
		if (!((intco->regIEN >> 7) & 1)) {
			enabled = false;
		} else if (ien) {
			enabled = true;
		} else {
			enabled = false;
		}
		if (enabled) {
			if (!intco->traceInt[i]) {
				dbgprintf("TRACE int %i\n", i);
				intco->traceInt[i] =
				    SigNode_Trace(intco->sigInt[i], sig_int_trace, intco);
			}
		} else {
			if (intco->traceInt[i]) {
				dbgprintf("UNTRACE int %i\n", i);
				SigNode_Untrace(intco->sigInt[i], intco->traceInt[i]);
				intco->traceInt[i] = NULL;
			}
		}
	}
}

static uint8_t
ien0_read(void *eventData, uint8_t addr)
{
	AT89C51Intco *intco = eventData;
	return intco->regIEN & 0xff;
}

static void
ien0_write(void *eventData, uint8_t addr, uint8_t value)
{
	AT89C51Intco *intco = eventData;
	intco->regIEN = (intco->regIEN & 0xff00) | value;
	dbgprintf("IEN0: %04x\n", intco->regIEN);
	update_signal_traces(intco);
	update_interrupt(intco);
}

static uint8_t
ien1_read(void *eventData, uint8_t addr)
{
	AT89C51Intco *intco = eventData;
	return (intco->regIEN >> 8) & 0xff;
}

static void
ien1_write(void *eventData, uint8_t addr, uint8_t value)
{
	AT89C51Intco *intco = eventData;
	intco->regIEN = (intco->regIEN & 0xff) | ((uint32_t) value << 8);
	dbgprintf("IEN1: %02x\n", value);
	update_signal_traces(intco);
	update_interrupt(intco);
}

static uint8_t
ipl0_read(void *eventData, uint8_t addr)
{
	AT89C51Intco *intco = eventData;
	return intco->regIPL & 0xff;
}

static void
ipl0_write(void *eventData, uint8_t addr, uint8_t value)
{
	AT89C51Intco *intco = eventData;
	intco->regIPL = (intco->regIPL & 0xff00) | value;
	update_interrupt(intco);
}

static uint8_t
ipl1_read(void *eventData, uint8_t addr)
{
	AT89C51Intco *intco = eventData;
	return (intco->regIPL >> 8) & 0xff;
}

static void
ipl1_write(void *eventData, uint8_t addr, uint8_t value)
{
	AT89C51Intco *intco = eventData;
	intco->regIPL = (intco->regIPL & 0xff) | ((uint32_t) value << 8);
	update_interrupt(intco);
}

static uint8_t
iph0_read(void *eventData, uint8_t addr)
{
	AT89C51Intco *intco = eventData;
	return intco->regIPH & 0xff;
}

static void
iph0_write(void *eventData, uint8_t addr, uint8_t value)
{
	AT89C51Intco *intco = eventData;
	intco->regIPH = (intco->regIPH & 0xff00) | value;
	update_interrupt(intco);
}

static uint8_t
iph1_read(void *eventData, uint8_t addr)
{
	AT89C51Intco *intco = eventData;
	return (intco->regIPH >> 8) & 0xff;
}

static void
iph1_write(void *eventData, uint8_t addr, uint8_t value)
{
	AT89C51Intco *intco = eventData;
	intco->regIPH = (intco->regIPH & 0xff) | ((uint32_t) value << 8);
	update_interrupt(intco);
}

/**
 ************************************************************************** 
 * Negative logic like interrupts itself.
 ************************************************************************** 
 */
static void
AT89C51Intco_AckIrq(struct SigNode *sig, int value, void *eventData)
{
	AT89C51Intco *intco = eventData;
	Interrupt *irq;
	if (value == SIG_LOW) {
		irq = intco->irqOutstanding;
		if (irq && (irq->index < array_size(intco->sigAckInt))
		    && intco->sigAckInt[irq->index]) {
			SigNode_Set(intco->sigAckInt[irq->index], SIG_LOW);
			SigNode_Set(intco->sigAckInt[irq->index], SIG_HIGH);
		}
	}
}

void
AT89C51Intco_New(const char *name)
{
	AT89C51Intco *intco = sg_new(AT89C51Intco);
	int i;
	intco->sigAckIn = SigNode_New("%s.ackInt", name);
	if (!intco->sigAckIn) {
		fprintf(stderr, "Can not create ack in signal for \"%s\"\n", name);
		exit(1);
	}
	SigNode_Set(intco->sigAckIn, SIG_PULLUP);
	SigNode_Trace(intco->sigAckIn, AT89C51Intco_AckIrq, intco);

	for (i = 0; i < array_size(ints); i++) {
		Interrupt *irq = &ints[i];
		int index = irq->index;
		intco->sigInt[index] = SigNode_New("%s.%s", name, irq->name);
		if (!intco->sigInt[index]) {
			fprintf(stderr, "Can not create interrupt\n");
			exit(1);
		}
		SigNode_Set(intco->sigInt[index], SIG_PULLUP);
		if (irq->ackName) {
			intco->sigAckInt[index] = SigNode_New("%s.%s", name, irq->ackName);
			if (!intco->sigAckInt[index]) {
				fprintf(stderr, "Can not create interrupt ack line \"%s\"\n",
					irq->ackName);
				exit(1);
			}
		}
		intco->interrupt[index] = irq;
	}
	MCS51_RegisterSFR(REG_IEN0, ien0_read, NULL, ien0_write, intco);
	MCS51_RegisterSFR(REG_IEN1, ien1_read, NULL, ien1_write, intco);
	MCS51_RegisterSFR(REG_IPL0, ipl0_read, NULL, ipl0_write, intco);
	MCS51_RegisterSFR(REG_IPL1, ipl1_read, NULL, ipl1_write, intco);
	MCS51_RegisterSFR(REG_IPH0, iph0_read, NULL, iph0_write, intco);
	MCS51_RegisterSFR(REG_IPH1, iph1_read, NULL, iph1_write, intco);
}
