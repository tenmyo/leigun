/*
 *************************************************************************************************
 *
 * External Interrupts of ATMegaXX4 
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
#include <string.h>
#include "avr8_io.h"
#include "avr8_cpu.h"
#include "sgstring.h"
#include "signode.h"
#include "atm644_extint.h"
#include "compiler_extensions.h"

#if 0
#define dbgprintf(...) fprintf(stderr,__VA_ARGS__)
#else
#define dbgprintf(...)
#endif

#define EICRA(base)	(0x69)
#define 	EICRA_ISC21   (1 << 5)
#define 	EICRA_ISC20   (1 << 4)
#define 	EICRA_ISC11   (1 << 3)
#define 	EICRA_ISC10   (1 << 2)
#define 	EICRA_ISC01   (1 << 1)
#define 	EICRA_ISC00   (1 << 0)
#define		INT_LOW_LEVEL		(0)
#define 	INT_EDGE_ANY		(1)
#define		INT_FALLING_EDGE	(2)
#define		INT_RISING_EDGE		(3)

#define EIMSK(base)	(0x3D)
#define 	EIMSK_INT2    (1 << 2)
#define 	EIMSK_INT1    (1 << 1)
#define 	EIMSK_INT0    (1 << 0)

#define EIFR(base)	(0x3C)
#define 	EIFR_INTF2   (1 << 2)
#define 	EIFR_INTF1   (1 << 1)
#define 	EIFR_INTF0   (1 << 0)

/*
 ***************************************************************************
 * Also include the Pin change interrupts. The individual PCIMSK registers
 * are included in the IO-Port module
 ***************************************************************************
 */
#define PCICR(base)	(0x68)
#define 	PCICR_PCIE3   (1 << 3)
#define 	PCICR_PCIE2   (1 << 2)
#define 	PCICR_PCIE1   (1 << 1)
#define 	PCICR_PCIE0   (1 << 0)

#define PCIFR(base) 	(0x3B)
#define 	PCIFR_PCIF3   (1 << 3)
#define 	PCIFR_PCIF2   (1 << 2)
#define 	PCIFR_PCIF1   (1 << 1)
#define 	PCIFR_PCIF0   (1 << 0)
typedef struct ATM644_ExtInt ATM644_ExtInt;

typedef struct PCInt {
	ATM644_ExtInt *ei;
	int index;
	SigNode *pcint;
	SigNode *irqNode;
	SigNode *irqAckNode;
	SigTrace *pcintTrace;
} PCInt;

typedef struct ExtInt {
	ATM644_ExtInt *ei;
	int index;
	SigNode *extint;
	SigNode *irqNode;
	SigNode *irqAckNode;
	SigTrace *extintTrace;
} ExtInt;

struct ATM644_ExtInt {
	uint8_t eicra;
	uint8_t eimsk;
	uint8_t eifr;
	uint8_t pcicr;
	uint8_t pcifr;
	ExtInt extint[3];
	PCInt pcint[4];
};
static void
update_pcint(ATM644_ExtInt * ei, int i)
{
	uint8_t ints = ei->pcifr & ei->pcicr;
	if (ints & (1 << i)) {
		SigNode_Set(ei->pcint[i].irqNode, SIG_LOW);
	} else {
		SigNode_Set(ei->pcint[i].irqNode, SIG_OPEN);
	}
}

static void
update_extint(ATM644_ExtInt * ei, int i)
{
	uint8_t ints = ei->eimsk & ei->eifr;
	dbgprintf("update_extints %02x\n", ints);
	if (ints & (1 << i)) {
		SigNode_Set(ei->extint[i].irqNode, SIG_LOW);
	} else {
		SigNode_Set(ei->extint[i].irqNode, SIG_OPEN);
	}
}

static void
trigger_extint(ATM644_ExtInt * ei, ExtInt * extInt)
{
	uint8_t msk = (1 << extInt->index);
	if (!(ei->eifr & msk)) {
		ei->eifr |= msk;
		update_extint(ei, extInt->index);
	}
}

static void
untrigger_extint(ATM644_ExtInt * ei, ExtInt * extInt)
{
	uint8_t msk = (1 << extInt->index);
	if ((ei->eifr & msk)) {
		ei->eifr &= ~msk;
		update_extint(ei, extInt->index);
	}
}

static void
extint_trace(struct SigNode *sig, int value, void *clientData)
{
	ExtInt *extint = (ExtInt *) clientData;
	ATM644_ExtInt *ei = extint->ei;
	uint8_t edge = (ei->eicra >> (extint->index << 1)) & 3;
	dbgprintf("Got extint trace on %d\n", extint->index);
	switch (edge) {
	    case INT_LOW_LEVEL:
		    if (value == SIG_LOW) {
			    trigger_extint(ei, extint);
		    } else {
			    untrigger_extint(ei, extint);
		    }
		    break;

	    case INT_FALLING_EDGE:
		    if (value == SIG_LOW) {
			    trigger_extint(ei, extint);
		    }
		    break;

	    case INT_EDGE_ANY:
		    trigger_extint(ei, extint);
		    break;

	    case INT_RISING_EDGE:
		    if (value == SIG_LOW) {
			    trigger_extint(ei, extint);
		    }
		    break;
	}

}

static void
pcint_trace(struct SigNode *sig, int value, void *clientData)
{
	PCInt *pcint = (PCInt *) clientData;
	ATM644_ExtInt *ei = pcint->ei;
	ei->pcifr |= (1 << pcint->index);
	update_pcint(ei, pcint->index);
}

static void
update_extint_trace(ATM644_ExtInt * ei, int index)
{
	ExtInt *extint = &ei->extint[index];
	if (ei->eimsk & (1 << index)) {
		if (!extint->extintTrace) {
			dbgprintf("Adding extint trace for %d\n", index);
			extint->extintTrace = SigNode_Trace(extint->extint, extint_trace, extint);
		}
	} else {
		if (extint->extintTrace) {
			dbgprintf("Removign extint trace for %d\n", index);
			SigNode_Untrace(extint->extint, extint->extintTrace);
			extint->extintTrace = NULL;
		}
	}
}

static void
update_pcint_trace(ATM644_ExtInt * ei, int index)
{
	PCInt *pcint = &ei->pcint[index];
	if (ei->pcicr & (1 << index)) {
		if (!pcint->pcintTrace) {
			pcint->pcintTrace = SigNode_Trace(pcint->pcint, pcint_trace, pcint);
		}
	} else {
		if (pcint->pcintTrace) {
			SigNode_Untrace(pcint->pcint, pcint->pcintTrace);
			pcint->pcintTrace = NULL;
		}
	}
}

static uint8_t
eicra_read(void *clientData, uint32_t address)
{
	ATM644_ExtInt *ei = (ATM644_ExtInt *) clientData;
	return ei->eicra;
}

static void
eicra_write(void *clientData, uint8_t value, uint32_t address)
{
	ATM644_ExtInt *ei = (ATM644_ExtInt *) clientData;
	int i;
	ei->eicra = value;
	/* Level interrupts are updated immediately */
	for (i = 0; i < 3; i++) {
		if (!(ei->eimsk & (1 << i))) {
			continue;
		}
		if (((ei->eicra >> (i << 1)) & 3) == INT_LOW_LEVEL) {
			if (SigNode_Val(ei->extint[i].extint) == SIG_LOW) {
				dbgprintf("EICRA Triggering level interrupt %d\n", i);
				trigger_extint(ei, &ei->extint[i]);
			} else {
				dbgprintf("EICRA Triggering level interrupt %d\n", i);
				untrigger_extint(ei, &ei->extint[i]);
			}
		}
	}
}

static uint8_t
eimsk_read(void *clientData, uint32_t address)
{
	ATM644_ExtInt *ei = (ATM644_ExtInt *) clientData;
	return ei->eimsk;
}

static void
eimsk_write(void *clientData, uint8_t value, uint32_t address)
{
	int i;
	ATM644_ExtInt *ei = (ATM644_ExtInt *) clientData;
	uint8_t diff = value ^ ei->eimsk;
	ei->eimsk = value;
	for (i = 0; i < 3; i++) {
		if (diff & (1 << i)) {
			update_extint_trace(ei, i);
			update_extint(ei, i);
		}
	}
}

static uint8_t
eifr_read(void *clientData, uint32_t address)
{
	ATM644_ExtInt *ei = (ATM644_ExtInt *) clientData;
	return ei->eifr;
}

static void
eifr_write(void *clientData, uint8_t value, uint32_t address)
{
	ATM644_ExtInt *ei = (ATM644_ExtInt *) clientData;
	uint8_t clear = value & 7;
	int i;
	if (ei->eifr & clear) {
		ei->eifr &= ~clear;
		/* level interrupts are immediately retriggered */
		for (i = 0; i < 3; i++) {
			if (!(clear & (1 << i))) {
				continue;
			}
			if (((ei->eicra >> (i << 1)) & 3) == INT_LOW_LEVEL) {
				if (SigNode_Val(ei->extint[i].extint) == SIG_LOW) {
					if (ei->eimsk & (1 << i)) {
						trigger_extint(ei, &ei->extint[i]);
					}
				} else {
					untrigger_extint(ei, &ei->extint[i]);
				}
			}
			update_extint(ei, i);
		}
	}
}

static uint8_t
pcicr_read(void *clientData, uint32_t address)
{
	ATM644_ExtInt *ei = (ATM644_ExtInt *) clientData;
	return ei->pcicr;
}

static void
pcicr_write(void *clientData, uint8_t value, uint32_t address)
{
	ATM644_ExtInt *ei = (ATM644_ExtInt *) clientData;
	uint8_t diff = value ^ ei->pcicr;
	int i;
	for (i = 0; i < 4; i++) {
		if (diff & (1 << i)) {
			/* 
			 * Seems like real hardware doesn't monitor the
			 * changes in pcifr when pcimsk is 0
			 */
			update_pcint_trace(ei, i);
			update_pcint(ei, i);
		}
	}
}

static uint8_t
pcifr_read(void *clientData, uint32_t address)
{
	ATM644_ExtInt *ei = (ATM644_ExtInt *) clientData;
	return ei->pcifr;
}

static void
pcifr_write(void *clientData, uint8_t value, uint32_t address)
{
	ATM644_ExtInt *ei = (ATM644_ExtInt *) clientData;
	uint8_t clear = value & 0xf;
	int i;
	ei->pcifr &= ~clear;
	for (i = 0; i < 4; i++) {
		if (clear & (1 << i)) {
			update_pcint(ei, i);
		}
	}
}

static void
pcintAck(SigNode * node, int value, void *clientData)
{
	PCInt *pcint = (PCInt *) clientData;
	ATM644_ExtInt *ei = pcint->ei;
	uint8_t mask;
	if (value == SIG_LOW) {
		mask = (1 << pcint->index);
		if (ei->pcifr & mask) {
			ei->pcifr &= ~mask;
			update_pcint(ei, pcint->index);
		} else {
			fprintf(stderr, "Bug, pcint ack with no interrupt posted\n");
		}
	}
}

static void
extintAck(SigNode * node, int value, void *clientData)
{
	ExtInt *extInt = (ExtInt *) clientData;
	ATM644_ExtInt *ei = extInt->ei;
	uint8_t msk;
	if (value == SIG_LOW) {
		msk = (1 << extInt->index);
		if ((ei->eifr & msk)) {
			ei->eifr &= ~msk;
			update_extint(ei, extInt->index);
		} else {
			fprintf(stderr, "Bug, extInt ack with no interrupt posted\n");
		}
		dbgprintf(stderr, "Got extInt ack for %d\n", extInt->index);
	}
}

void
ATM644_ExtIntNew(const char *name)
{
	ATM644_ExtInt *ei = sg_new(ATM644_ExtInt);
	int i;
	AVR8_RegisterIOHandler(EICRA(base), eicra_read, eicra_write, ei);
	AVR8_RegisterIOHandler(EIMSK(base), eimsk_read, eimsk_write, ei);
	AVR8_RegisterIOHandler(EIFR(base), eifr_read, eifr_write, ei);
	AVR8_RegisterIOHandler(PCICR(base), pcicr_read, pcicr_write, ei);
	AVR8_RegisterIOHandler(PCIFR(base), pcifr_read, pcifr_write, ei);
	for (i = 0; i < 3; i++) {
		ei->extint[i].ei = ei;
		ei->extint[i].index = i;
		ei->extint[i].extint = SigNode_New("%s.extint_in%d", name, i);
		ei->extint[i].irqNode = SigNode_New("%s.extint_out%d", name, i);
		ei->extint[i].irqAckNode = SigNode_New("%s.extintAck%d", name, i);
		SigNode_Trace(ei->extint[i].irqAckNode, extintAck, &ei->extint[i]);
		if (!ei->extint[i].extint) {
			fprintf(stderr, "ATM644_ExtInt: Can not create EXTINT interrupt lines\n");
			exit(1);
		}
	}
	for (i = 0; i < 4; i++) {
		ei->pcint[i].ei = ei;
		ei->pcint[i].index = i;
		ei->pcint[i].pcint = SigNode_New("%s.pcint_in%d", name, i);
		ei->pcint[i].irqNode = SigNode_New("%s.pcint_out%d", name, i);
		ei->pcint[i].irqAckNode = SigNode_New("%s.pcintAck%d", name, i);
		SigNode_Trace(ei->pcint[i].irqAckNode, pcintAck, &ei->pcint[i]);
		if (!ei->pcint[i].pcint) {
			fprintf(stderr, "ATM644_ExtInt: Can not create PCINT interrupt lines\n");
			exit(1);
		}
	}
	fprintf(stderr, "Created ATM644 External interrupt controller\n");
}
