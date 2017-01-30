/*
 *************************************************************************************************
 *
 * Emulation of the AT91 Peripheral DMA Controller (PDC) 
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

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "i2c.h"
#include "bus.h"
#include "signode.h"
#include "cycletimer.h"
#include "clock.h"
#include "at91_pdc.h"
#include "sgstring.h"

#define PDC_RPR(base)	((base)+0x100)	/* Receive Pointer Register */
#define PDC_RCR(base) 	((base)+0x104)	/* Receive Counter Register */
#define PDC_TPR(base)	((base)+0x108)	/* Transmit Pointer Register */
#define PDC_TCR(base)	((base)+0x10c)	/* Transmit Counter Register */
#define PDC_RNPR(base)	((base)+0x110)	/* Receive Next Pointer Register */
#define PDC_RNCR(base)	((base)+0x114)	/* Receive Next Counter Register */
#define PDC_TNPR(base)	((base)+0x118)	/* Transmit Next Pointer Register */
#define PDC_TNCR(base)	((base)+0x11c)	/* Transmit Next Counter Register */

#define PDC_PTCR(base)	((base)+0x120)	/* Transfer Control Register */
#define   PDC_RXTEN          (1 << 0)	/* Receiver Transfer Enable */
#define   PDC_RXTDIS         (1 << 1)	/* Receiver Transfer Disable */
#define   PDC_TXTEN          (1 << 8)	/* Transmitter Transfer Enable */
#define   PDC_TXTDIS         (1 << 9)	/* Transmitter Transfer Disable */

#define PDC_PTSR(base)	((base)+0x124)	/* Transfer Status Register */

typedef struct AT91Pdc {
	BusDevice bdev;
	SigNode *irqNode;
	SigNode *rxDmaReq;
	SigNode *txDmaReq;
	CycleTimer *rxtimer;
	CycleTimer *txtimer;
	uint32_t rpr;
	uint32_t rcr;
	uint32_t tpr;
	uint32_t tcr;
	uint32_t rnpr;
	uint32_t rncr;
	uint32_t tnpr;
	uint32_t tncr;
	uint32_t ptcr;
	uint32_t ptsr;
} AT91Pdc;

/* Receive Pointer Register */
static uint32_t
rpr_read(void *clientData, uint32_t address, int rqlen)
{
	AT91Pdc *pdc = (AT91Pdc *) clientData;
	return pdc->rpr;
}

static void
rpr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	AT91Pdc *pdc = (AT91Pdc *) clientData;
	pdc->rpr = value;
}

/* Receive Counter Register */
static uint32_t
rcr_read(void *clientData, uint32_t address, int rqlen)
{
	AT91Pdc *pdc = (AT91Pdc *) clientData;
	return pdc->rcr;
}

static void
rcr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	AT91Pdc *pdc = (AT91Pdc *) clientData;
	pdc->rcr = value & 0xffff;
}

/* Transmit Pointer Register */
static uint32_t
tpr_read(void *clientData, uint32_t address, int rqlen)
{
	AT91Pdc *pdc = (AT91Pdc *) clientData;
	return pdc->tpr;
}

static void
tpr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	AT91Pdc *pdc = (AT91Pdc *) clientData;
	pdc->tpr = value;
}

/* Transmit Counter Register */
static uint32_t
tcr_read(void *clientData, uint32_t address, int rqlen)
{
	AT91Pdc *pdc = (AT91Pdc *) clientData;
	return pdc->tcr;
}

static void
tcr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	AT91Pdc *pdc = (AT91Pdc *) clientData;
	pdc->tcr = value & 0xffff;
	fprintf(stderr, "Register %08x is not implemented\n", address);
}

/* Receive Next Pointer Register */
static uint32_t
rnpr_read(void *clientData, uint32_t address, int rqlen)
{
	AT91Pdc *pdc = (AT91Pdc *) clientData;
	return pdc->rnpr;
}

static void
rnpr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	AT91Pdc *pdc = (AT91Pdc *) clientData;
	pdc->rnpr = value;
}

/* Receive Next Counter Register */
static uint32_t
rncr_read(void *clientData, uint32_t address, int rqlen)
{
	AT91Pdc *pdc = (AT91Pdc *) clientData;
	return pdc->rncr;
}

static void
rncr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	AT91Pdc *pdc = (AT91Pdc *) clientData;
	pdc->rncr = value & 0xffff;
}

/* Transmit Next Pointer Register */
static uint32_t
tnpr_read(void *clientData, uint32_t address, int rqlen)
{
	AT91Pdc *pdc = (AT91Pdc *) clientData;
	return pdc->tnpr;
}

static void
tnpr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	AT91Pdc *pdc = (AT91Pdc *) clientData;
	pdc->tnpr = value;
}

/* Transmit Next Counter Register */
static uint32_t
tncr_read(void *clientData, uint32_t address, int rqlen)
{
	AT91Pdc *pdc = (AT91Pdc *) clientData;
	return pdc->tncr;
}

static void
tncr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	AT91Pdc *pdc = (AT91Pdc *) clientData;
	pdc->tncr = value & 0xffff;
}

/* Transfer Control Register */
static uint32_t
ptcr_read(void *clientData, uint32_t address, int rqlen)
{
	AT91Pdc *pdc = (AT91Pdc *) clientData;
	return pdc->ptcr;
}

static void
ptcr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	AT91Pdc *pdc = (AT91Pdc *) clientData;
	pdc->ptcr = value & 0x303;
	fprintf(stderr, "Register %08x is not implemented\n", address);
}

/* Transfer Status Register */
static uint32_t
ptsr_read(void *clientData, uint32_t address, int rqlen)
{
	AT91Pdc *pdc = (AT91Pdc *) clientData;
	return pdc->ptsr;
}

static void
ptsr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "AT91Pdc: Status register is readonly\n");
}

static void
AT91Pdc_Map(void *owner, uint32_t base, uint32_t mask, uint32_t flags)
{
	AT91Pdc *pdc = (AT91Pdc *) owner;
	IOH_New32(PDC_RPR(base), rpr_read, rpr_write, pdc);
	IOH_New32(PDC_RCR(base), rcr_read, rcr_write, pdc);
	IOH_New32(PDC_TPR(base), tpr_read, tpr_write, pdc);
	IOH_New32(PDC_TCR(base), tcr_read, tcr_write, pdc);
	IOH_New32(PDC_RNPR(base), rnpr_read, rnpr_write, pdc);
	IOH_New32(PDC_RNCR(base), rncr_read, rncr_write, pdc);
	IOH_New32(PDC_TNPR(base), tnpr_read, tnpr_write, pdc);
	IOH_New32(PDC_TNCR(base), tncr_read, tncr_write, pdc);
	IOH_New32(PDC_PTCR(base), ptcr_read, ptcr_write, pdc);
	IOH_New32(PDC_PTSR(base), ptsr_read, ptsr_write, pdc);
}

static void
AT91Pdc_UnMap(void *owner, uint32_t base, uint32_t mask)
{
	IOH_Delete32(PDC_RPR(base));
	IOH_Delete32(PDC_RCR(base));
	IOH_Delete32(PDC_TPR(base));
	IOH_Delete32(PDC_TCR(base));
	IOH_Delete32(PDC_RNPR(base));
	IOH_Delete32(PDC_RNCR(base));
	IOH_Delete32(PDC_TNPR(base));
	IOH_Delete32(PDC_TNCR(base));
	IOH_Delete32(PDC_PTCR(base));
	IOH_Delete32(PDC_PTSR(base));

}

BusDevice *
AT91Pdc_New(const char *name)
{
	AT91Pdc *pdc = sg_new(AT91Pdc);
	pdc->bdev.first_mapping = NULL;
	pdc->bdev.Map = AT91Pdc_Map;
	pdc->bdev.UnMap = AT91Pdc_UnMap;
	pdc->bdev.owner = pdc;
	pdc->bdev.hw_flags = MEM_FLAG_WRITABLE | MEM_FLAG_READABLE;
	//update_interrupt(pdc);
	fprintf(stderr, "AT91 PDC \"%s\" created\n", name);
	return &pdc->bdev;
}
