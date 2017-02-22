/*
 *************************************************************************************************
 *
 * Emulation of the NS9750 BBus DMA Controller 
 *
 * Status:
 *	Not yet working
 *
 * Copyright 2004 Jochen Karrer. All rights reserved.
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
#include <stdlib.h>
#include <string.h>
#include <bus.h>
#include <ns9750_bbus.h>
#include <ns9750_bbdma.h>
#include "signode.h"
#include "sgstring.h"

#if 1
#define dbgprintf(...) { fprintf(stderr,__VA_ARGS__); }
#else
#define dbgprintf(...)
#endif

/*
 * ------------------------------------------------
 * Register definitions
 * 	Warning: I count from 0 to 15, docu
 * 	counts channels from 1 to 16
 * ------------------------------------------------
 */
#define BB_DMA_BASE             (0x90000000)
#define BB_BDP(dma,channel)     (0x00+0x20*(channel)+(dma)*0x00110000)
#define BB_CTRL(dma,channel)    (0x10+0x20*(channel)+(dma)*0x00110000)
#define 	CTRL_CE  (1<<31)
#define 	CTRL_CA	 (1<<30)
#define		CTRL_BB_SHIFT  (28)
#define		CTRL_BB_MASK   (3<<28)
#define 	CTRL_MODE_MASK (3<<26)
#define 		CTRL_MODE_FLY_BY_WRITE (0<<26)
#define 		CTRL_MODE_FLY_BY_READ  (1<<26)
#define 	CTRL_BTE_MASK	(3<<24)
#define 	CTRL_REQ	(1<<23)
#define 	CTRL_BDR 	(1<<22)
#define		CTRL_SINC_N	(1<<21)
#define 	CTRL_SIZE_MASK	(3<<16)
#define 	CTRL_SIZE_SHIFT	(16)
#define		CTRL_STATE_MASK (0x3f << 10)
#define		CTRL_STATE_SHIFT (10)
#define		CTRL_INDEX_MASK	(0x3ff)
#define BB_STATIER(dma,channel) (0x14+0x20*(channel)+(dma)*0x00110000)
#define 	STATIER_NCIP	(1<<31)
#define		STATIER_ECIP	(1<<30)
#define		STATIER_NRIP	(1<<29)
#define		STATIER_CAIP	(1<<28)
#define		STATIER_PCIP	(1<<27)
#define		STATIER_NCIE	(1<<24)
#define		STATIER_ECIE	(1<<23)
#define		STATIER_NRIE	(1<<22)
#define		STATIER_CAIE	(1<<21)
#define		STATIER_PCIE	(1<<20)
#define		STATIER_WRAP	(1<<19)
#define		STATIER_IDONE	(1<<18)
#define		STATIER_LAST	(1<<17)
#define		STATIER_FULL	(1<<16)
#define		STATIER_BLEN	(0xffff)

typedef struct BufDescr {
	uint32_t src;
	uint32_t buflen;
	uint32_t dst;		/* not used, written back to mem as zero */
	uint32_t flags_status;
} BufDescr;

//} __attribute__((packed)) BufDescr;

#define BD_FLAG_WRAP	(1<<31)
#define BD_FLAG_INT	(1<<30)
#define BD_FLAG_LAST	(1<<29)
#define BD_FLAG_FULL	(1<<28)

struct BBusDMACtrl;

/* 
 * ------------------------------------------------------
 * We have 32 dma channels with different directions 
 * ------------------------------------------------------
 */
#define CAP_FBR	(1)
#define CAP_FBW	(2)
typedef struct
    DMA_ChannelSpec {
	char *description;	/* Who uses it */
	int controller;		/* 0 or 1 for the two DMA controllers */
	int channel;
	int cap;
} DMA_ChannelSpec;

struct BBusDMA_Channel {
	int nr;
	int irq_posted;
	DMA_ChannelSpec *spec;
	uint32_t fbdp;		/* First Buffer descriptor Pointer */
	uint32_t byte_index:10;	/* byte offset relative to fbdp */
	uint32_t dmactrl;
	uint32_t dmastatier;
	struct BBusDMACtrl *controller;
	SigNode *FbrDmaReq;
	SigNode *FbwDmaGnt;
	BufDescr bufDescr;
	uint16_t bytes_done;
	BBDMA_FbrProc *FbrProc;
	void *FbrClientData;
};
#define  CBDP(dmachan) ((((dmachan)->fbdp)&~0) + (dmachan)->byte_index)

struct BBusDMACtrl {
	BusDevice bdev;
	uint32_t dma_isr;	/* belongs to bbutil */
	BBusDMA_Channel dma[32];
};

/*
 * ------------------------------------------------------------
 * List of DMA Channels for NS9750
 * ------------------------------------------------------------
 */

DMA_ChannelSpec ns9750_dma_channels[] = {
	{
	 .description = "Serial A receiver",
	 .controller = 0,
	 .channel = 0,
	 .cap = CAP_FBW},
	{
	 .description = "Serial A transmitter",
	 .controller = 0,
	 .channel = 1,
	 .cap = CAP_FBR},
	{
	 .description = "Serial B receiver",
	 .controller = 0,
	 .channel = 2,
	 .cap = CAP_FBW},
	{
	 .description = "Serial B transmitter",
	 .controller = 0,
	 .channel = 3,
	 .cap = CAP_FBR},
	{
	 .description = "Serial C receiver",
	 .controller = 0,
	 .channel = 4,
	 .cap = CAP_FBW},
	{
	 .description = "Serial C transmitter",
	 .controller = 0,
	 .channel = 5,
	 .cap = CAP_FBR},
	{
	 .description = "Serial D receiver",
	 .controller = 0,
	 .channel = 6,
	 .cap = CAP_FBW},
	{
	 .description = "Serial D transmitter",
	 .controller = 0,
	 .channel = 7,
	 .cap = CAP_FBR},
	{
	 .description = "1284 command receiver",
	 .controller = 0,
	 .channel = 8,
	 .cap = CAP_FBW},
	{
	 .description = "Unused",
	 .controller = 0,
	 .channel = 9,
	 .cap = 0},
	{
	 .description = "1284 data receiver",
	 .controller = 0,
	 .channel = 10,
	 .cap = CAP_FBW},
	{
	 .description = "1284 data transmitter",
	 .controller = 0,
	 .channel = 11,
	 .cap = CAP_FBR},
	{
	 .description = "Unused",
	 .controller = 0,
	 .channel = 12,
	 .cap = 0},
	{
	 .description = "Unused",
	 .controller = 0,
	 .channel = 13,
	 .cap = 0},
	{
	 .description = "Unused",
	 .controller = 0,
	 .channel = 14,
	 .cap = 0},
	{
	 .description = "Unused",
	 .controller = 0,
	 .channel = 15,
	 .cap = 0},
	{
	 .description = "USB device control-OUT endpoint #0",
	 .controller = 1,
	 .channel = 0,
	 .cap = CAP_FBW},
	{
	 .description = "USB device control-IN endpoint #0",
	 .controller = 1,
	 .channel = 1,
	 .cap = CAP_FBR},
	{
	 .description = "USB device endpoint #1",
	 .controller = 1,
	 .channel = 2,
	 .cap = CAP_FBR | CAP_FBW},
	{
	 .description = "USB device endpoint #2",
	 .controller = 1,
	 .channel = 3,
	 .cap = CAP_FBR | CAP_FBW},
	{
	 .description = "USB device endpoint #3",
	 .controller = 1,
	 .channel = 4,
	 .cap = CAP_FBR | CAP_FBW},
	{
	 .description = "USB device endpoint #4",
	 .controller = 1,
	 .channel = 5,
	 .cap = CAP_FBR | CAP_FBW},
	{
	 .description = "USB device endpoint #5",
	 .controller = 1,
	 .channel = 6,
	 .cap = CAP_FBR | CAP_FBW},
	{
	 .description = "USB device endpoint #6",
	 .controller = 1,
	 .channel = 7,
	 .cap = CAP_FBR | CAP_FBW},
	{
	 .description = "USB device endpoint #7",
	 .controller = 1,
	 .channel = 8,
	 .cap = CAP_FBR | CAP_FBW},
	{
	 .description = "USB device endpoint #8",
	 .controller = 1,
	 .channel = 9,
	 .cap = CAP_FBR | CAP_FBW},
	{

	 .description = "USB device endpoint #9",
	 .controller = 1,
	 .channel = 10,
	 .cap = CAP_FBR | CAP_FBW},
	{
	 .description = "USB device endpoint #10",
	 .controller = 1,
	 .channel = 11,
	 .cap = CAP_FBR | CAP_FBW},
	{
	 .description = "USB device endpoint #11",
	 .controller = 1,
	 .channel = 12,
	 .cap = CAP_FBR | CAP_FBW},
	{
	 .description = "Unused",
	 .controller = 1,
	 .channel = 13,
	 .cap = 0},
	{
	 .description = "Unused",
	 .controller = 1,
	 .channel = 14,
	 .cap = 0},
	{
	 .description = "Unused",
	 .controller = 1,
	 .channel = 15,
	 .cap = 0},
};

static void
load_descriptor(BufDescr * bd, uint32_t addr)
{
	uint32_t src = addr;
	int i;
	uint32_t *data = (uint32_t *) bd;
	dbgprintf("load descriptor from 0x%08x\n", addr);
	for (i = 0; i < 4; i++) {
		*data = Bus_Read32(src);
		//Bus_Read(data,src,4);
		fprintf(stderr, "LD: %08x from %08x\n", *data, src);
		data++;
		src += 4;
	}
}

static void
store_descriptor(BufDescr * bd, uint32_t addr)
{
	int i;
	uint32_t src = addr;
	uint32_t *data = (uint32_t *) bd;
	for (i = 0; i < 4; i++) {
		Bus_Write32(*data, src);
		data++;
		src += 4;
	}
}

static void
dump_descriptor(BufDescr * bd)
{
	fprintf(stderr, "  src:           0x%08x\n", bd->src);
	fprintf(stderr, "  buflen:        0x%08x\n", bd->buflen);
	fprintf(stderr, "  flags_status:  0x%04x\n", bd->flags_status);
}

static void
load_next_transferdescriptor(BBusDMA_Channel * chan)
{
	BufDescr *bd = &chan->bufDescr;
	uint32_t cbdp;
	if (bd->flags_status & BD_FLAG_WRAP) {
		chan->byte_index = 0;
	} else {
		chan->byte_index += 16;
	}
	cbdp = CBDP(chan);
	load_descriptor(&chan->bufDescr, cbdp);
}

/*
 * -----------------------------------
 * Update single channel interrupt
 * -----------------------------------
 */

static void
update_interrupt(BBusDMA_Channel * chan)
{
	uint32_t statier = chan->dmastatier;
	uint8_t stat = statier >> 27;
	uint8_t ier = (statier >> 20) & 0x1f;
	if (stat & ier) {
		if (!chan->irq_posted) {
			if (chan->nr < 16) {
				fprintf(stderr, "POST BBDMA Interrupt\n");
				BBus_PostDmaIRQ(chan->nr);
			} else {
				fprintf(stderr, "Posting USB DMA irq not implemented\n");
			}
			chan->irq_posted = 1;
		}
	} else {
		if (chan->irq_posted) {
			if (chan->nr < 16) {
				fprintf(stderr, "UnPOST BBDMA Interrupt\n");
				BBus_UnPostDmaIRQ(chan->nr);
			} else {
				fprintf(stderr, "UnPosting USB DMA irq not implemented\n");
			}
			chan->irq_posted = 0;
		}
	}
}

/*
 * --------------------------------------------------------------
 * Interface functions for the users of the DMA Controller
 * --------------------------------------------------------------
 */

/*
 * -------------------------------------------------------------
 * RetireFbwDescriptor
 * 	FBW Channels (perirpheral to memory) set 
 * 	the full flag in the DMA descriptor after data is received 
 *	from peripheral
 * -------------------------------------------------------------
 */
__UNUSED__ static void
RetireFbwDescriptor(BBusDMA_Channel * chan)
{
	BufDescr *descr = &chan->bufDescr;
	uint32_t cbdp;
	descr->flags_status |= BD_FLAG_FULL;
	descr->dst = 0;		// Manual p 505 
	descr->buflen = descr->buflen - chan->bytes_done;
	cbdp = CBDP(chan);
	store_descriptor(&chan->bufDescr, cbdp);
}

/*
 * -------------------------------------------------------------
 * RetireFbrDescriptor
 * 	FBR Channels (memory to perirpheral) clear
 * 	the full flag in the DMA descriptor after data is sent 
 * -------------------------------------------------------------
 */
static void
RetireFbrDescriptor(BBusDMA_Channel * chan)
{
	BufDescr *descr = &chan->bufDescr;
	uint32_t cbdp;
	descr->flags_status &= ~BD_FLAG_FULL;
	descr->dst = 0;		// Manual p 505 
	descr->buflen = descr->buflen - chan->bytes_done;

	cbdp = CBDP(chan);
	store_descriptor(&chan->bufDescr, cbdp);
}

/*
 * -------------------------------------------------------------
 * BBDMA_Fbw 
 *	Fly by write, called from the device when it has data
 *	to be written to memory.
 * 	receive data from source and write to memory of host
 * -------------------------------------------------------------
 */
int
BBDMA_Fbw(BBusDMA_Channel * chan, uint8_t * buf, int count)
{
	//BBusDMACtrl *bbdma = chan->controller;
	BufDescr *descr;
	uint32_t mem_address;
	uint16_t buflen;
	int transferlen;

	if (!(chan->dmactrl & CTRL_CE)) {
		// dbgprintf("DMA Channel %d is not enabled, ignoring data\n");
		return 0;
	}
	if ((chan->dmactrl & CTRL_MODE_MASK) != CTRL_MODE_FLY_BY_WRITE) {
		fprintf(stderr, "BBDMA Bug: Use FBR channel for FBW\n");
		return 0;
	}
	descr = &chan->bufDescr;
	if ((descr->flags_status & BD_FLAG_FULL)) {
		// NRIP ?
		// update_interrupt();
		//chan->dmactrl &= ~CTRL_CE; /* ??? Netos sets it itself to 0 ???? */
		return 0;
	}
	mem_address = descr->src;
	buflen = descr->buflen;
	if (mem_address & 3) {
		fprintf(stderr, "BBDMA: Address not word aligned\n");
	}
	/* behaviour of real system is unknown  in this case */
	if (buflen < count) {
		transferlen = buflen;
	} else {
		transferlen = count;
	}
	Bus_Write(mem_address, buf, transferlen);
	if (buflen < count) {

	}
	return transferlen;
}

static int
DMA_Read(BBusDMA_Channel * chan, uint8_t * buf, int maxlen)
{
	//BBusDMACtrl *bbdma = chan->controller;
	BufDescr *descr;
	uint32_t mem_address;
	int buflen;
	int bytes_remaining;
	int count;

	if ((chan->dmactrl & CTRL_CE) == 0) {
		dbgprintf("DMA Channel %d is not enabled, reading from nowhere\n", chan->nr);
		return 0;
	}
	if ((chan->dmactrl & CTRL_CA) == 0) {
		chan->dmactrl = chan->dmactrl & ~CTRL_CE;
		chan->dmastatier |= STATIER_CAIP;
		dbgprintf("DMA Channel %d abort bit is still set\n", chan->nr);
		update_interrupt(chan);
		return 0;
	}
	if ((chan->dmactrl & CTRL_MODE_MASK) != CTRL_MODE_FLY_BY_READ) {
		fprintf(stderr, "BBDMA: Use Write Channel for reading data\n");
		return 0;
	}
	descr = &chan->bufDescr;
	mem_address = descr->src;
	buflen = descr->buflen;
	bytes_remaining = buflen - chan->bytes_done;
	if (!(descr->flags_status & BD_FLAG_FULL)) {
		//chan->dmactrl &= ~CTRL_CE; /* ???? Netos sets it itself to 0 in tx_dma_int */
		dbgprintf("Buffer not full\n");
		chan->dmastatier |= STATIER_NRIP;
		update_interrupt(chan);
		dump_descriptor(descr);
		return 0;
	}
	if (bytes_remaining > maxlen) {
		count = maxlen;
	} else {
		count = bytes_remaining;
	}
	Bus_Read(buf, mem_address, count);
	return count;
}

/*
 * -----------------------------------------------------
 *  DMA_Advance for FBR (Memory to peripheral)
 * -----------------------------------------------------
 */
static int
DMA_AdvanceFbr(BBusDMA_Channel * chan, unsigned int advance)
{

	BufDescr *descr = &chan->bufDescr;
	if (!(descr)) {
		fprintf(stderr, "No Buf descriptor but advance DMA pointer\n");
		return -1;
	}
	if ((chan->bytes_done + advance) > descr->buflen) {
		fprintf(stderr, "Emulator Bug: advance bigger than buflen\n");
		chan->dmactrl &= ~CTRL_CE;
		update_interrupt(chan);
		exit(432);
	}
	chan->bytes_done += advance;
	if (chan->bytes_done == descr->buflen) {
		chan->dmastatier |= STATIER_NCIP;
		RetireFbrDescriptor(chan);
		chan->bytes_done = 0;
		load_next_transferdescriptor(chan);
		if (!(descr->flags_status & BD_FLAG_FULL)) {
			chan->dmastatier |= STATIER_NRIP;
			//chan->dmactrl &= ~CTRL_CE; 
		}
		update_interrupt(chan);
	}
	return 0;
}

BBusDMA_Channel *
BBDMA_Connect(BBusDMACtrl * bbdma, unsigned int channel)
{
	if (channel < 32) {
		return &bbdma->dma[channel];
	}
	return NULL;
}

/*
 * -----------------------------------------------------------------------
 * All FBR (Fly by memory to peripheral) devices need to register
 * an event handler which is invoked from the DMA Controller when
 * there is data and DMA request is set
 * -----------------------------------------------------------------------
 */
void
BBDMA_SetDataSink(BBusDMA_Channel * chan, BBDMA_FbrProc * proc, void *clientData)
{
	chan->FbrProc = proc;
	chan->FbrClientData = clientData;
}

/*
 * -----------------------------------------------------------------
 * FBR - From memory to Peripheral
 *	Read in portions of 64 from Memory and
 *	try to put to the device. Advance the Transferdescriptor
 *	by number of bytes accepted by the Peripheral
 * ------------------------------------------------------------------
 */
static void
do_fbr(BBusDMA_Channel * chan)
{
	uint8_t buf[64];
	if (SigNode_Val(chan->FbrDmaReq) == SIG_LOW) {
		dbgprintf("do_fbr: No request\n");
	}
	while (SigNode_Val(chan->FbrDmaReq) == SIG_HIGH) {
		int count;
		int usedcount;
		if (!(chan->dmactrl & CTRL_CE)) {
			dbgprintf("do_fbr(%d): Channel not enabled\n", chan->nr);
			return;
		}
		fprintf(stderr, "do_fbr\n");
		if (!chan->FbrProc) {
			dbgprintf("do_fbr: No Fbr callback\n");
			return;
		}
		count = DMA_Read(chan, buf, 64);
		if (count <= 0) {
			dbgprintf("DMA_Read: failed with %d\n", count);
			return;
		}
		usedcount = chan->FbrProc(chan, buf, count, chan->FbrClientData);
		dbgprintf("FbrProc transfered %d bytes\n", usedcount);
		if (usedcount <= 0) {
			return;
		}
		if (DMA_AdvanceFbr(chan, usedcount) < 0) {
			return;
		}
	}
}

/*
 * -----------------------------------------------------------
 * This function might be invoked recursively !
 * -----------------------------------------------------------
 */
static void
fbr_dma_request_proc(struct SigNode *node, int value, void *clientData)
{
	BBusDMA_Channel *chan = clientData;
	if (value == SIG_HIGH) {
		fprintf(stderr, "got tx dma request\n");
		do_fbr(chan);
	} else if (value == SIG_LOW) {
		fprintf(stderr, "removed tx dma request %s\n", SigName(node));

	} else {
		fprintf(stderr, "Illegal value %d for DMA request node\n", value);
	}
}

/*
 * ---------------------------------------------------
 * Get/Set first Buffer descriptor pointer
 * ---------------------------------------------------
 */
static uint32_t
bdp_read(void *clientData, uint32_t address, int rqlen)
{
	BBusDMA_Channel *chan = clientData;
	dbgprintf("bbdma bdp read:  0x%08x\n", chan->fbdp);
	return chan->fbdp;
}

static void
bdp_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	BBusDMA_Channel *chan = clientData;
	chan->fbdp = value;
	dbgprintf("bbdma bdp write:  0x%08x\n", chan->fbdp);
	return;
}

/*
 * -------------------------------------------
 * DMA Control register
 * -------------------------------------------
 */

static uint32_t
dmactrl_read(void *clientData, uint32_t address, int rqlen)
{
	BBusDMA_Channel *chan = clientData;
	chan->dmactrl = (chan->dmactrl & ~0x3ff & ~0xfc00) | (chan->byte_index & 0x3ff);
	dbgprintf("bbdmactrl(%d) read: 0x%08x\n", chan->nr, chan->dmactrl);
	return chan->dmactrl;
}

/*
 * ----------------------------------------------------------
 * 
 * DMA-Ctrl Register
 *
 * ----------------------------------------------------------
 */
static void
dmactrl_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	BBusDMA_Channel *chan = clientData;
	uint32_t cbdp;
	uint32_t diff = chan->dmactrl ^ value;
	chan->dmactrl = value & ~(CTRL_BDR);
	if (diff & CTRL_CE) {
		chan->byte_index = 0;
	}
	if (value & CTRL_CA) {
		// close buffer is missing here
		chan->dmactrl = chan->dmactrl & ~CTRL_CE;
		chan->dmastatier |= STATIER_CAIP;
		update_interrupt(chan);
	} else if (((value & CTRL_BDR) && (value & CTRL_CE))
		   || ((diff & CTRL_CE) && (value & CTRL_CE))) {
		cbdp = CBDP(chan);
		chan->bytes_done = 0;
		fprintf(stderr, "fbdp %08x,byte index %04x\n", chan->fbdp, chan->byte_index);
		load_descriptor(&chan->bufDescr, cbdp);
		dump_descriptor(&chan->bufDescr);
		if (value & CTRL_CE) {
			if ((value & CTRL_MODE_MASK) == CTRL_MODE_FLY_BY_WRITE) {
				fprintf(stderr, "do FBW not implemented\n");
			} else if ((value & CTRL_MODE_MASK) == CTRL_MODE_FLY_BY_READ) {
				fprintf(stderr, "do FBR because of ChannelEnable(CE)\n");
				do_fbr(chan);
			}
		}
	}
	dbgprintf("bbdmactrl(%d) write: 0x%08x is now 0x%08x\n", chan->nr, value, chan->dmactrl);
	return;
}

/*
 * ----------------------------------------------
 * DMA Status/Interrupt enable register
 * ----------------------------------------------
 */

static uint32_t
dmastatier_read(void *clientData, uint32_t address, int rqlen)
{
	BBusDMA_Channel *chan = clientData;
	BufDescr *descr = &chan->bufDescr;
	chan->dmastatier = chan->dmastatier &
	    ~(STATIER_BLEN | STATIER_WRAP | STATIER_IDONE | STATIER_LAST | STATIER_FULL);
	chan->dmastatier |= (uint16_t) (descr->buflen - chan->bytes_done);
	chan->dmastatier |= (((uint32_t) descr->flags_status) >> 12) &
	    (STATIER_WRAP | STATIER_IDONE | STATIER_LAST | STATIER_FULL);
	dbgprintf("bbdma statier(%d) read: 0x%08x\n", chan->nr, chan->dmastatier);
	return chan->dmastatier;
}

static void
dmastatier_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	BBusDMA_Channel *chan = clientData;
	uint32_t clearmask = ~(value & 0xf0000000);
	uint32_t overtakemask = 0x7f00000;
	dbgprintf("bbdma statier(%d) write 0x%08x\n", chan->nr, value);
	chan->dmastatier &= clearmask;
	chan->dmastatier = (chan->dmastatier & ~overtakemask) | (value & overtakemask);
	update_interrupt(chan);
	return;
}

static void
BBusDMA_Map(void *owner, uint32_t base, uint32_t mask, uint32_t flags)
{
	BBusDMACtrl *bbdma = owner;
	int dma, channel;
	uint32_t reg;
	for (dma = 0; dma < 2; dma++) {
		for (channel = 0; channel < 16; channel++) {
			BBusDMA_Channel *dmachan = &bbdma->dma[channel + (dma << 4)];
			reg = base + BB_BDP(dma, channel);
			IOH_New32(reg, bdp_read, bdp_write, dmachan);
			reg = base + BB_CTRL(dma, channel);
			IOH_New32(reg, dmactrl_read, dmactrl_write, dmachan);
			reg = base + BB_STATIER(dma, channel);
			IOH_New32(reg, dmastatier_read, dmastatier_write, dmachan);
		}
	}
}

static void
BBusDMA_UnMap(void *owner, uint32_t base, uint32_t mask)
{
	int dma, channel;
	uint32_t reg;
	for (dma = 0; dma < 2; dma++) {
		for (channel = 0; channel < 16; channel++) {
			reg = base + BB_BDP(dma, channel);
			IOH_Delete32(reg);
			reg = base + BB_CTRL(dma, channel);
			IOH_Delete32(reg);
			reg = base + BB_STATIER(dma, channel);
			IOH_Delete32(reg);
		}
	}

}

BBusDMACtrl *
NS9750_BBusDMA_New(char *name)
{
	int i;
	BBusDMACtrl *bbdma = sg_new(BBusDMACtrl);
	bbdma->bdev.first_mapping = NULL;
	bbdma->bdev.Map = BBusDMA_Map;
	bbdma->bdev.UnMap = BBusDMA_UnMap;
	bbdma->bdev.owner = bbdma;
	bbdma->bdev.hw_flags = MEM_FLAG_WRITABLE | MEM_FLAG_READABLE;
	for (i = 0; i < 32; i++) {
		DMA_ChannelSpec *spec = &ns9750_dma_channels[i];
		BBusDMA_Channel *chan = &bbdma->dma[i];
		chan->nr = i;
		chan->controller = bbdma;
		chan->spec = spec;

		if (spec->cap & CAP_FBR) {
			chan->FbrDmaReq = SigNode_New("%s.%d.FbrDmaReq", name, i);
			if (!(chan->FbrDmaReq)) {
				fprintf(stderr, "Can not create FBR DMA request node for \"%s\"\n",
					name);
				exit(4327);
			}
			SigNode_Trace(chan->FbrDmaReq, fbr_dma_request_proc, chan);
		}
		if (spec->cap & CAP_FBW) {
			chan->FbwDmaGnt = SigNode_New("%s.%d.FbwDmaGnt", name, i);
			if (!(chan->FbwDmaGnt)) {
				fprintf(stderr,
					"Can not create FBW DMA request node for channel %d\n", i);
				exit(4325);
			}
			SigNode_Set(chan->FbwDmaGnt, SIG_LOW);	/* default is no Grant */
		}
		/* All registers are 0 on reset. (Not verified) */
		chan->fbdp = 0;
		chan->dmactrl = 0;
		chan->dmastatier = 0;
	}
	Mem_AreaAddMapping(&bbdma->bdev, BB_DMA_BASE, 0x200000,
			   MEM_FLAG_WRITABLE | MEM_FLAG_READABLE);
	fprintf(stderr, "NS9750 BBus DMA Controller created\n");
	return bbdma;
}
