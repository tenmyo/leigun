 /*
  **********************************************************************************************
  * Renesas RX62N IO-Port simulator 
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

#include <stdint.h>
#include "sgstring.h"
#include "sglib.h"
#include "bus.h"
#include "signode.h"
#include "ioport_rx62n.h"

#if 0
#define dbgprintf(...) { fprintf(stderr,__VA_ARGS__);fflush(stderr);}
#else
#define dbgprintf(...)
#endif

#define REG_DDR(base,port_nr)	((base) + 0x00 + (port_nr))
#define REG_DR(base,port_nr)	((base) + 0x20 + (port_nr))
#define REG_PORT(base,port_nr)	((base) + 0x40 + (port_nr))
#define REG_ICR(base,port_nr)	((base) + 0x60 + (port_nr))
#define REG_ODR(base,port_nr)	((base) + 0x80 + (port_nr))

/* Maximum (176 pin variant) is 15 ports */
#define NR_IOPORTS	(15)

typedef struct RxIoPorts RxIoPorts;

typedef struct IoPort {
	RxIoPorts *ports;
	uint8_t portNr;
	uint8_t regDDR;
	uint8_t regDR;
	uint8_t regICR;
	uint8_t regODR;
	SigNode *sigPort[8];
} IoPort;

struct RxIoPorts {
	BusDevice bdev;
	IoPort ioPort[NR_IOPORTS];
};

static void
Update_Output(IoPort * iop)
{
	unsigned int i;
	for (i = 0; i < 8; i++) {
		if (iop->regDDR & (1 << i)) {
			if (iop->regDR & (1 << i)) {
				if (iop->regODR & (1 << i)) {
					SigNode_Set(iop->sigPort[i], SIG_OPEN);
				} else {
					SigNode_Set(iop->sigPort[i], SIG_HIGH);
				}
			} else {
				SigNode_Set(iop->sigPort[i], SIG_LOW);
			}
		} else {
			SigNode_Set(iop->sigPort[i], SIG_OPEN);
		}
	}
}

static uint32_t
ddr_read(void *clientData, uint32_t address, int rqlen)
{
	IoPort *iop = clientData;
	return iop->regDDR;
}

static void
ddr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	IoPort *iop = clientData;
	iop->regDDR = value;
	Update_Output(iop);
}

static uint32_t
dr_read(void *clientData, uint32_t address, int rqlen)
{
	IoPort *iop = clientData;
	return iop->regDR;
}

static void
dr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	IoPort *iop = clientData;
	iop->regDR = value;
	Update_Output(iop);
}

static uint32_t
port_read(void *clientData, uint32_t address, int rqlen)
{
	IoPort *iop = clientData;
	uint8_t value = 0;
	unsigned int i;
	for (i = 0; i < 8; i++) {
		if (SigNode_Val(iop->sigPort[i]) == SIG_HIGH) {
			value |= (1 << i);
		}
	}
	return value;
}

static void
port_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "Port not writable\n");
}

static uint32_t
icr_read(void *clientData, uint32_t address, int rqlen)
{
	IoPort *iop = clientData;
	return iop->regICR;
}

static void
icr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	IoPort *iop = clientData;
	fprintf(stderr, "Warning, reg ICR not implemented\n");
	iop->regICR = value;
}

static uint32_t
odr_read(void *clientData, uint32_t address, int rqlen)
{
	IoPort *iop = clientData;
	return iop->regODR;
}

static void
odr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	IoPort *iop = clientData;
	iop->regODR = value;
	fprintf(stderr, "Warning, not all ports have an ODR\n");
}

static void
RxPorts_Unmap(void *owner, uint32_t base, uint32_t mask)
{
	unsigned int i;
	for (i = 0; i < NR_IOPORTS; i++) {
		IOH_Delete8(REG_DDR(base, i));
		IOH_Delete8(REG_DR(base, i));
		IOH_Delete8(REG_PORT(base, i));
		IOH_Delete8(REG_ICR(base, i));
		IOH_Delete8(REG_ODR(base, i));
	}
}

static void
RxPorts_Map(void *owner, uint32_t base, uint32_t mask, uint32_t mapflags)
{
	RxIoPorts *ports = owner;
	unsigned int i;
	for (i = 0; i < NR_IOPORTS; i++) {
		IoPort *iop = &ports->ioPort[i];
		IOH_New8(REG_DDR(base, i), ddr_read, ddr_write, iop);
		IOH_New8(REG_DR(base, i), dr_read, dr_write, iop);
		IOH_New8(REG_PORT(base, i), port_read, port_write, iop);
		IOH_New8(REG_ICR(base, i), icr_read, icr_write, iop);
		IOH_New8(REG_ODR(base, i), odr_read, odr_write, iop);
	}
}

static const char *portname = "0123456789ABCDEG";

BusDevice *
Rx62nIoPorts_New(const char *name)
{
	unsigned int i, j;
	RxIoPorts *ports = sg_new(RxIoPorts);
	for (i = 0; i < NR_IOPORTS; i++) {
		IoPort *iop = &ports->ioPort[i];
		iop->ports = ports;
		for (j = 0; j < 8; j++) {
			iop->sigPort[j] = SigNode_New("%s.P%c.%u", name, portname[i], j);
			if (!iop->sigPort[j]) {
				fprintf(stderr, "Can not create io port\n");
				exit(1);
			}
			iop->portNr = i;
		}
	}
	ports->bdev.first_mapping = NULL;
	ports->bdev.Map = RxPorts_Map;
	ports->bdev.UnMap = RxPorts_Unmap;
	ports->bdev.owner = ports;
	ports->bdev.hw_flags = MEM_FLAG_WRITABLE | MEM_FLAG_READABLE;
	return &ports->bdev;
}
