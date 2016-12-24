 /*
  **********************************************************************************************
  * Renesas RX63N IO-Port simulator 
  *
  * Copyright 2013 Jochen Karrer. All rights reserved.
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

#include <stdint.h>
#include "sgstring.h"
#include "sglib.h"
#include "bus.h"
#include "signode.h"
#include "ioport_rx63n.h"

#if 0
#define dbgprintf(...) { fprintf(stderr,__VA_ARGS__);fflush(stderr);}
#else
#define dbgprintf(...)
#endif

/* Base is 0x8C000 */
#define REG_PDR(base,port_nr)	((base) + port_nr)
#define REG_PODR(base,port_nr)	((base) + 0x20 + (port_nr))
#define REG_PIDR(base,port_nr)	((base) + 0x40 + (port_nr))
#define REG_PMR(base,port_nr)	((base) + 0x60 + (port_nr))
#define REG_ODR0(base,port_nr)	((base) + 0x80 + ((port_nr) << 1))
#define REG_ODR1(base,port_nr)	((base) + 0x81 + ((port_nr) << 1))
#define REG_PCR(base,port_nr)	((base) + 0xc0 + (port_nr))
#define REG_DSCR(base,port_nr)	((base) + 0xe0 + (port_nr))
#define	REG_PSRA(base)		((base) + 0x121)	/* Only 64 pin package */

#define NR_IOPORTS      (19)
typedef struct RxIoPorts RxIoPorts;
typedef struct IoPort IoPort;

typedef struct IoPin {
	IoPort *iop;
	char *name;
	unsigned int pinNr;
	SigNode *sigP;		/*< The port Pin */
	SigNode *sigPO;		/*< The Port output driver input */
	SigNode *sigPU;		/*< The pullup */
	SigNode *sigPDR;	/*< The direction register */
	SigNode *sigDR;		/*< The output driver direction input */
	SigNode *sigODR;	/*< Open drain, HIGH is open drain */
	SigNode *sigPODR;	/*< The output register value */
	SigNode *sigPMR;	/*< Peripheral Module mode register */
	SigNode *sigPMO;	/*< Peripheral Module output */
	SigNode *sigPMOE;	/*< Peripheral Module output enable */
	SigNode *sigPMI;	/*< Peripheral Module input */
	SigTrace *traceOpenDrain;
} IoPin;

struct IoPort {
	RxIoPorts *rxIoPort;
	uint8_t portNr;
	uint8_t regPDR;		/*< Port Direction Register */
	uint8_t regPODR;	/*< Port Output Data Register */
	uint8_t regPIDR;	/*< Port Input Data Register */
	uint8_t regPMR;		/*< Port Mode Register (Peripheral/IO) */
	uint8_t regODR0;	/*< Open Drain Control Register 0 */
	uint8_t regPCR;		/*< Pull Up Resistor Control Register */
	uint8_t regDSCR;	/*< Drive Capacity Control Register (DSCR) */
	IoPin pin[8];
};

struct RxIoPorts {
	BusDevice bdev;
	uint8_t regPSRA;
	IoPort ioPort[NR_IOPORTS];
};

/**
 *****************************************************************************
 * This is the open drain function.
 *****************************************************************************
 */

static void
OutbufferOpenDrain(SigNode * sig, int value, void *eventData)
{
	IoPin *pin = eventData;
	if (value == SIG_LOW) {
		SigNode_Set(pin->sigP, SIG_LOW);
	} else {
		SigNode_Set(pin->sigP, SIG_OPEN);
	}
}

static void
OutbufferDRTrace(SigNode * sig, int value, void *eventData)
{
	IoPin *pin = eventData;
	dbgprintf("%s: DR change to %u\n", pin->name, value);
	if (SigNode_Val(pin->sigDR) == SIG_LOW) {
		if (pin->traceOpenDrain) {
			SigNode_Untrace(pin->sigPO, pin->traceOpenDrain);
			pin->traceOpenDrain = NULL;
		}
		SigNode_RemoveLink(pin->sigPO, pin->sigP);
		SigNode_Set(pin->sigP, SIG_OPEN);
	} else {
		if (SigNode_Val(pin->sigODR) == SIG_HIGH) {
			SigNode_RemoveLink(pin->sigPO, pin->sigP);
			if (!pin->traceOpenDrain) {
				pin->traceOpenDrain =
				    SigNode_Trace(pin->sigPO, OutbufferOpenDrain, pin);
			}
		} else {
			if (!SigNode_Linked(pin->sigPO, pin->sigP)) {
				SigNode_Link(pin->sigPO, pin->sigP);
				dbgprintf("%s: DRLinked\n", pin->name);
			} else {
				dbgprintf("%s: Was already DRLinked\n", pin->name);
			}
			if (pin->traceOpenDrain) {
				SigNode_Untrace(pin->sigPO, pin->traceOpenDrain);
				pin->traceOpenDrain = NULL;
			}
			SigNode_Set(pin->sigP, SIG_OPEN);
		}
	}
	dbgprintf("%s is now %u\n", pin->name, SigNode_Val(pin->sigP));
}

static void
OutbufferODRTrace(SigNode * sig, int value, void *eventData)
{
	OutbufferDRTrace(sig, value, eventData);
}

static uint32_t
pdr_read(void *clientData, uint32_t address, int rqlen)
{
	IoPort *iop = clientData;
	return iop->regPDR;
}

static void
pdr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	int i;
	uint8_t diff, msk;
	IoPort *iop = clientData;
	diff = iop->regPDR ^ value;
	for (i = 0; i < 8; i++) {
		IoPin *pin = &iop->pin[i];
		msk = (1 << i);
		if (!(diff & msk)) {
			continue;
		}
#if 0
        if (iop->portNr == 0) {
		    fprintf(stderr,"Change pdr %s to %u\n",pin->name, !!(value & msk));
        }
#endif
		if (value & msk) {
			SigNode_Set(pin->sigPDR, SIG_HIGH);
		} else {
			SigNode_Set(pin->sigPDR, SIG_LOW);
		}
	}
	iop->regPDR = value;
}

static uint32_t
podr_read(void *clientData, uint32_t address, int rqlen)
{
	IoPort *iop = clientData;
	return iop->regPODR;
}

static void
podr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	int i;
	IoPort *iop = clientData;
	uint8_t diff, msk;
	diff = iop->regPODR ^ value;
//	for (i = 0; i < 8; i++) {
	for (i = 7; i >= 0; i--) {
		IoPin *pin = &iop->pin[i];
		msk = (1 << i);
		if (!(diff & msk)) {
			continue;
		}
#if 0
        if (iop->portNr == 0) {
		    fprintf(stderr,"Change pin %s to %u\n",pin->name, !!(value & msk));
        }
#endif
		//fprintf(stderr,"Change pin %s\n",pin->name);
		if (value & msk) {
			SigNode_Set(pin->sigPODR, SIG_HIGH);
		} else {
			SigNode_Set(pin->sigPODR, SIG_LOW);
		}
	}
	iop->regPODR = value;
}

static uint32_t
pidr_read(void *clientData, uint32_t address, int rqlen)
{
	unsigned int i;
	uint8_t value = 0;
	IoPort *iop = clientData;
	for (i = 0; i < 8; i++) {
		if (SigNode_Val(iop->pin[i].sigP) == SIG_HIGH) {
			value |= (1 << i);
		}
	}
	return value;
}

static void
pidr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "%s not writable\n", __func__);
}

static uint32_t
pmr_read(void *clientData, uint32_t address, int rqlen)
{
	IoPort *iop = clientData;
	return iop->regPMR;
}

static void
pmr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	IoPort *iop = clientData;
	uint8_t diff, msk;
	int i;
	diff = value ^ iop->regPMR;
	for (i = 0; i < 8; i++) {
		IoPin *pin = &iop->pin[i];
		msk = (1 << i);
		if (!(diff & msk)) {
			continue;
		}
		if (value & msk) {
			dbgprintf("Hier %u\n",__LINE__);
			SigNode_RemoveLink(pin->sigDR, pin->sigPDR);
			/* Data */
			SigNode_Set(pin->sigPO, SigNode_Val(pin->sigPO));
			SigNode_RemoveLink(pin->sigPO, pin->sigPODR);
			SigNode_Set(pin->sigPO, SigNode_Val(pin->sigPMO));
			SigNode_Link(pin->sigP, pin->sigPMO);
			SigNode_Set(pin->sigPO, SIG_OPEN);

			SigNode_Link(pin->sigDR, pin->sigPMOE);
		} else {
			dbgprintf("Hier %u\n",__LINE__);
			SigNode_RemoveLink(pin->sigDR, pin->sigPMOE);

			SigNode_Set(pin->sigPO, SigNode_Val(pin->sigPO));
			SigNode_RemoveLink(pin->sigP, pin->sigPMO);
			SigNode_Set(pin->sigPO, SigNode_Val(pin->sigPODR));
			SigNode_Link(pin->sigPO, pin->sigPODR);
			//SigNode_Set(pin->sigPO, SIG_OPEN);

			SigNode_Link(pin->sigDR, pin->sigPDR);
		}
	}
	iop->regPMR = value;
}

static uint32_t
odr0_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "%s not implemented\n", __func__);
	return 0;
}

static void
odr0_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "%s not implemented\n", __func__);
}

static uint32_t
odr1_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "%s not implemented\n", __func__);
	return 0;
}

static void
odr1_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "%s not implemented\n", __func__);
}

static uint32_t
pcr_read(void *clientData, uint32_t address, int rqlen)
{
	IoPort *iop = clientData;
	return iop->regPCR;
}

static void
pcr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	uint8_t diff, msk;
	int i;
	IoPort *iop = clientData;
	diff = value ^ iop->regPCR;
	for (i = 0; i < 8; i++) {
		IoPin *pin = &iop->pin[i];
		msk = 1 << i;
		if (diff & msk) {
			if (value & msk) {
				SigNode_Link(pin->sigPU, pin->sigP);
			} else {
				SigNode_RemoveLink(pin->sigPU, pin->sigP);
			}
		}
	}
	iop->regPCR = value;
}

static uint32_t
dscr_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "%s not implemented\n", __func__);
	return 0;
}

static void
dscr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "%s not implemented\n", __func__);
}

static uint32_t
psra_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "%s not implemented\n", __func__);
	return 0;
}

static void
psra_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "%s not implemented\n", __func__);
}

static void
RxPorts_Unmap(void *owner, uint32_t base, uint32_t mask)
{
	unsigned int i;
	for (i = 0; i < NR_IOPORTS; i++) {
		IOH_Delete8(REG_PDR(base, i));
		IOH_Delete8(REG_PODR(base, i));
		IOH_Delete8(REG_PIDR(base, i));
		IOH_Delete8(REG_PMR(base, i));
		IOH_Delete8(REG_ODR0(base, i));
		IOH_Delete8(REG_ODR1(base, i));
		IOH_Delete8(REG_PCR(base, i));
		IOH_Delete8(REG_DSCR(base, i));
	}
	IOH_Delete8(REG_PSRA(base));
}

static void
RxPorts_Map(void *owner, uint32_t base, uint32_t mask, uint32_t mapflags)
{
	RxIoPorts *rio = owner;
	unsigned int i;

	for (i = 0; i < NR_IOPORTS; i++) {
		IoPort *iop = &rio->ioPort[i];
		IOH_New8(REG_PDR(base, i), pdr_read, pdr_write, iop);
		IOH_New8(REG_PODR(base, i), podr_read, podr_write, iop);
		IOH_New8(REG_PIDR(base, i), pidr_read, pidr_write, iop);
		IOH_New8(REG_PMR(base, i), pmr_read, pmr_write, iop);
		IOH_New8(REG_ODR0(base, i), odr0_read, odr0_write, iop);
		IOH_New8(REG_ODR1(base, i), odr1_read, odr1_write, iop);
		IOH_New8(REG_PCR(base, i), pcr_read, pcr_write, iop);
		IOH_New8(REG_DSCR(base, i), dscr_read, dscr_write, iop);
	}
	IOH_New8(REG_PSRA(base), psra_read, psra_write, rio);

}

static const char *portname = "0123456789ABCDEFGxJ";

BusDevice *
Rx63nIoPorts_New(const char *name)
{
	int i, j;
	RxIoPorts *rio = sg_new(RxIoPorts);
	for (i = 0; i < NR_IOPORTS; i++) {
		IoPort *iop = &rio->ioPort[i];
		iop->rxIoPort = rio;
		if (portname[i] == 0) {
			fprintf(stderr, "Not enough portnames: %s\n", portname);
			exit(1);
		}
		iop->portNr = i;
		for (j = 0; j < 8; j++) {
			char str[30];
			IoPin *pin = &iop->pin[j];
			sprintf(str, "pin%c.%u", portname[i], j);
			pin->name = strdup(str);
			pin->pinNr = j;
			pin->iop = iop;
			pin->sigP = SigNode_New("%s.P%c.%u", name, portname[i], j);
			pin->sigPO = SigNode_New("%s.po%c.%u", name, portname[i], j);
			pin->sigPU = SigNode_New("%s.pu%c.%u", name, portname[i], j);
			pin->sigDR = SigNode_New("%s.dr%c.%u", name, portname[i], j);
			pin->sigPDR = SigNode_New("%s.pdr%c.%u", name, portname[i], j);
			pin->sigPMR = SigNode_New("%s.pmr%c.%u", name, portname[i], j);
			pin->sigODR = SigNode_New("%s.odr%c.%u", name, portname[i], j);
			pin->sigPODR = SigNode_New("%s.podr%c.%u", name, portname[i], j);
			pin->sigPMO = SigNode_New("%s.pmo%c.%u", name, portname[i], j);
			pin->sigPMOE = SigNode_New("%s.pmoe%c.%u", name, portname[i], j);
			pin->sigPMI = SigNode_New("%s.pmi%c.%u", name, portname[i], j);
			if (!pin->sigP || !pin->sigPU || !pin->sigDR || !pin->sigPMOE
			    || !pin->sigPMR || !pin->sigPMO || !pin->sigPMI
			    || !pin->sigODR || !pin->sigPO || !pin->sigPDR || !pin->sigPODR) {
				fprintf(stderr, "Can not create io port %d pin %d\n", i, j);
				exit(1);
			}
			SigNode_Set(pin->sigPU, SIG_PULLUP);
			SigNode_Set(pin->sigODR, SIG_LOW);
			SigNode_Set(pin->sigPODR, SIG_LOW);
			SigNode_Set(pin->sigPDR, SIG_LOW);
			SigNode_Set(pin->sigDR, SIG_PULLDOWN);
			SigNode_Link(pin->sigDR, pin->sigPDR);
			SigNode_Link(pin->sigPODR, pin->sigPO);
			SigNode_Trace(pin->sigDR, OutbufferDRTrace, pin);
			SigNode_Trace(pin->sigODR, OutbufferODRTrace, pin);
		}
	}
	rio->bdev.first_mapping = NULL;
	rio->bdev.Map = RxPorts_Map;
	rio->bdev.UnMap = RxPorts_Unmap;
	rio->bdev.owner = rio;
	rio->bdev.hw_flags = MEM_FLAG_WRITABLE | MEM_FLAG_READABLE;
	return &rio->bdev;
}
