/*
 *************************************************************************************************
 * Emulation of Freescale iMX21 DMA controller 
 *
 * state: working but timing not accurate, slow, 
 *        not tested, transfer direction "down" not implemented
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
#include <sys/ioctl.h>
#include <sys/fcntl.h>
#include "bus.h"
#include "fio.h"
#include "signode.h"
#include "imx21_dmac.h"
#include "configfile.h"
#include "sgstring.h"

#if 0
#define dbgprintf(x...) { fprintf(stderr,x); }
#else
#define dbgprintf(x...)
#endif

#define DMA_CH_BASE(base,x)  ((base) + 0x080 + 0x040 * (x))

#define DMA_DCR(base)       	((base) + 0x000)
#define		DCR_DAM		(1<<2)
#define		DCR_DRST	(1<<1)
#define		DCR_DEN		(1<<0)
#define DMA_DISR(base)          ((base) + 0x004)
#define DMA_DIMR(base)          ((base) + 0x008)
#define DMA_DBTOSR(base)        ((base) + 0x00C)
#define DMA_DRTOSR(base)        ((base) + 0x010)
#define DMA_DSESR(base)         ((base) + 0x014)
#define DMA_DBOSR(base)         ((base) + 0x018)
#define DMA_DBTOCR(base)        ((base) + 0x01C)
#define		DBTOCR_EN	(1<<15)
#define		DBTOCR_CNT_MASK (0x7fff)

#define DMA_WSRA(base)          ((base) + 0x040)
#define DMA_XSRA(base)          ((base) + 0x044)
#define DMA_YSRA(base)          ((base) + 0x048)
#define DMA_WSRB(base)          ((base) + 0x04C)
#define DMA_XSRB(base)          ((base) + 0x050)
#define DMA_YSRB(base)          ((base) + 0x054)

#define DMA_SAR(base,x)         (DMA_CH_BASE((base),(x)) + 0x000)
#define DMA_DAR(base,x)         (DMA_CH_BASE((base),(x)) + 0x004)
#define DMA_CNTR(base,x)        (DMA_CH_BASE((base),(x)) + 0x008)
#define DMA_CCR(base,x)         (DMA_CH_BASE((base),(x)) + 0x00C)
#define         CCR_ACRPT               (1<<14)
#define         CCR_DMOD_MASK           (3<<12)
#define         CCR_DMOD_SHIFT          (12)
#define         CCR_DMOD_LINEAR         (0x0 << 12)
#define         CCR_DMOD_2D             (0x1 << 12)
#define         CCR_DMOD_FIFO           (0x2 << 12)
#define         CCR_SMOD_MASK           (3<<10)
#define         CCR_SMOD_SHIFT          (10)
#define         CCR_SMOD_LINEAR         (0x0 << 10)
#define         CCR_SMOD_2D             (0x1 << 10)
#define         CCR_SMOD_FIFO           (0x2 << 10)
#define         CCR_MDIR                (1<<9)
#define         CCR_MSEL                (1<<8)
#define         CCR_DSIZ_MASK           (3<<6)
#define         CCR_DSIZ_SHIFT          (6)
#define         CCR_DSIZ_32             (0x0 << 6)
#define         CCR_DSIZ_8              (0x1 << 6)
#define         CCR_DSIZ_16             (0x2 << 6)
#define         CCR_SSIZ_MASK           (3<<4)
#define         CCR_SSIZ_SHIFT          (4)
#define         CCR_SSIZ_32             (0x0 << 4)
#define         CCR_SSIZ_8              (0x1 << 4)
#define         CCR_SSIZ_16             (0x2 << 4)
#define         CCR_REN                 (1<<3)
#define         CCR_RPT                 (1<<2)
#define         CCR_FRC                 (1<<1)
#define		CCR_CEN			(1<<0)
#define DMA_RSSR(base,x)        (DMA_CH_BASE((base),(x)) + 0x010)
#define DMA_BLR(base,x)         (DMA_CH_BASE((base),(x)) + 0x014)
#define DMA_RTOR(base,x)        (DMA_CH_BASE((base),(x)) + 0x018)
#define 	RTOR_EN            (1<<15)
#define 	RTOR_CLK           (1<<14)
#define 	RTOR_PSC           (1<<13)
#define DMA_BUCR(base,x)        (DMA_CH_BASE((base),(x)) + 0x018)
#define DMA_CCNR(base,x)        (DMA_CH_BASE((base),(x)) + 0x01C)

#define DMA_REQ_CSI_RX          31
#define DMA_REQ_CSI_STAT        30
#define DMA_REQ_BMI_RX          29
#define DMA_REQ_BMI_TX          28
#define DMA_REQ_UART1_TX        27
#define DMA_REQ_UART1_RX        26
#define DMA_REQ_UART2_TX        25
#define DMA_REQ_UART2_RX        24
#define DMA_REQ_UART3_TX        23
#define DMA_REQ_UART3_RX        22
#define DMA_REQ_UART4_TX        21
#define DMA_REQ_UART4_RX        20
#define DMA_REQ_CSPI1_TX        19
#define DMA_REQ_CSPI1_RX        18
#define DMA_REQ_CSPI2_TX        17
#define DMA_REQ_CSPI2_RX        16
#define DMA_REQ_SSI1_TX1        15
#define DMA_REQ_SSI1_RX1        14
#define DMA_REQ_SSI1_TX0        13
#define DMA_REQ_SSI1_RX0        12
#define DMA_REQ_SSI2_TX1        11
#define DMA_REQ_SSI2_RX1        10
#define DMA_REQ_SSI2_TX0        9
#define DMA_REQ_SSI2_RX0        8
#define DMA_REQ_SDHC1           7
#define DMA_REQ_SDHC2           6
#define DMA_FIRI_TX             5
#define DMA_FIRI_RX             4
#define DMA_EX                  3
#define DMA_REQ_CSPI3_TX        2
#define DMA_REQ_CSPI3_RX        1

typedef struct IMX21Dmac IMX21Dmac;

typedef struct DMAChan {
	IMX21Dmac *dmac;
	int nr;			/* my own number */
	SigNode *irqNode;
	SigNode *currReqLine;
	uint8_t xfer_buf[64];
	unsigned int xferbuf_rp;
	unsigned int xferbuf_wp;
	SigTrace *dmaReqTrace;
	int stop_dma;
	int interrupt_posted;
	uint32_t sar;
	uint32_t sar_buf;	/* double buffered */
	uint32_t dar;
	uint32_t dar_buf;
	uint32_t cntr;
	uint32_t cntr_buf;
	uint32_t ccr;
	uint32_t rssr;
	uint32_t blr;
	uint16_t rtor;
	uint16_t bucr;
	uint32_t ccnr;
} DMAChan;

struct IMX21Dmac {
	BusDevice bdev;
	DMAChan dmachan[16];
	SigNode *dmaReqNode[32];
	SigNode *dummyHighNode;
	uint16_t dcr;
	uint16_t disr;
	uint16_t dimr;
	uint16_t dbtosr;
	uint16_t drtosr;
	uint16_t dsesr;
	uint16_t dbosr;
	uint16_t dbtocr;
	uint16_t wsra;
	uint16_t xsra;
	uint16_t ysra;
	uint16_t wsrb;
	uint16_t xsrb;
	uint16_t ysrb;
};

static void
dmac_reset(IMX21Dmac * dmac)
{
	int i;

	dmac->dcr = 0;
	dmac->disr = 0;
	dmac->dimr = 0xffff;
	dmac->dbtosr = 0;
	dmac->drtosr = 0;
	dmac->dsesr = 0;
	dmac->dbosr = 0;
	dmac->dbtocr = 0;
	dmac->wsra = dmac->wsrb = 0;
	dmac->xsra = dmac->xsrb = 0;
	dmac->ysra = dmac->ysrb = 0;
	for (i = 0; i < 16; i++) {
		DMAChan *chan = &dmac->dmachan[i];
		chan->sar = 0;
		chan->dar = 0;
		chan->cntr = 0;
		chan->ccr = 0;
		chan->rssr = 0;
		/* Should untrace line and stop dma also */
		chan->currReqLine = dmac->dummyHighNode;
		chan->blr = 0;
		chan->rtor = 0;
		chan->bucr = 0;
		chan->ccnr = 0;
	}
}

static void
update_interrupts(IMX21Dmac * dmac)
{
	int i;
	uint16_t ints = (dmac->disr | dmac->dbtosr | dmac->drtosr | dmac->dsesr | dmac->dbosr)
	    & ~dmac->dimr;
	dbgprintf("DMAC: update ints %04x\n", ints);
	for (i = 0; i < 16; i++) {
		DMAChan *chan = &dmac->dmachan[i];
		if (ints & (1 << i)) {
			if (!chan->interrupt_posted) {
				chan->interrupt_posted = 1;
				dbgprintf("Post an DMA int chan %d\n", chan->nr);
				SigNode_Set(chan->irqNode, SIG_LOW);
			}
		} else {
			if (chan->interrupt_posted) {
				chan->interrupt_posted = 0;
				dbgprintf("UnPost an DMA int chan %d\n", chan->nr);
				SigNode_Set(chan->irqNode, SIG_HIGH);
			}
		}
	}
}

static void
dma_read(DMAChan * chan, int smod, int ssiz)
{
	uint32_t value, addr;
	addr = chan->sar;
	if (smod == CCR_SMOD_LINEAR) {
		addr += chan->ccnr;
	} else if (smod == CCR_SMOD_2D) {
		IMX21Dmac *dmac = chan->dmac;
		unsigned int xs, ys, ws, y, x;
		if (!(chan->ccr & CCR_MSEL)) {
			ws = dmac->wsra;
			xs = dmac->xsra;
			ys = dmac->ysra;
		} else {
			ws = dmac->wsrb;
			xs = dmac->xsrb;
			ys = dmac->ysrb;
		}
		if (xs) {
			y = chan->ccnr / xs;
			x = chan->ccnr - y * xs;
		} else {
			y = 0;
			x = 0;
		}
		addr += y * ws + x;
	} else if (smod == CCR_SMOD_FIFO) {
		/* Increment nothing when reading from fifo */
	}
	switch (ssiz) {
	    case 8:
		    if (chan->xferbuf_wp > 63) {
			    fprintf(stderr, "Emulator bug in %s line %d\n", __FILE__, __LINE__);
			    return;
		    }
		    chan->xfer_buf[chan->xferbuf_wp++] = Bus_Read8(addr);
		    break;
	    case 16:
		    if (chan->xferbuf_wp > 62) {
			    fprintf(stderr, "Emulator bug in %s line %d\n", __FILE__, __LINE__);
			    return;
		    }
		    value = Bus_Read16(addr);
		    chan->xfer_buf[chan->xferbuf_wp++] = value & 0xff;
		    chan->xfer_buf[chan->xferbuf_wp++] = (value >> 8) & 0xff;
		    break;
	    case 32:
		    if (chan->xferbuf_wp > 60) {
			    fprintf(stderr, "Emulator bug in %s line %d\n", __FILE__, __LINE__);
			    return;
		    }
		    value = Bus_Read32(addr);
		    chan->xfer_buf[chan->xferbuf_wp++] = value & 0xff;
		    chan->xfer_buf[chan->xferbuf_wp++] = (value >> 8) & 0xff;
		    chan->xfer_buf[chan->xferbuf_wp++] = (value >> 16) & 0xff;
		    chan->xfer_buf[chan->xferbuf_wp++] = (value >> 24) & 0xff;
		    break;
	    default:
		    break;
	}

}

static void
dma_write(DMAChan * chan, int dmod, int dsiz)
{
	uint32_t addr = chan->dar;
	uint32_t value;
	if (dmod == CCR_DMOD_LINEAR) {
		addr += chan->ccnr;
	} else if (dmod == CCR_DMOD_2D) {
		IMX21Dmac *dmac = chan->dmac;
		unsigned int xs, ys, ws, y, x;
		if (!(chan->ccr & CCR_MSEL)) {
			ws = dmac->wsra;
			xs = dmac->xsra;
			ys = dmac->ysra;
		} else {
			ws = dmac->wsrb;
			xs = dmac->xsrb;
			ys = dmac->ysrb;
		}
		if (xs) {
			y = chan->ccnr / xs;
			x = chan->ccnr - y * xs;
		} else {
			y = 0;
			x = 0;
		}
		addr += y * ws + x;
	} else if (dmod == CCR_DMOD_FIFO) {
		/* Increment nothing when reading from fifo */
	} else {
		fprintf(stderr, "DMAC: Illegal destination address mode\n");
	}
	switch (dsiz) {
	    case 8:
		    if (chan->xferbuf_rp > 63) {
			    fprintf(stderr, "Emulator bug in %s line %d\n", __FILE__, __LINE__);
			    return;
		    }
		    Bus_Write8(chan->xfer_buf[chan->xferbuf_rp++], addr);
		    break;
	    case 16:
		    if (chan->xferbuf_rp > 62) {
			    fprintf(stderr, "Emulator bug in %s line %d\n", __FILE__, __LINE__);
			    return;
		    }
		    value = chan->xfer_buf[chan->xferbuf_rp] | (chan->xfer_buf[chan->xferbuf_rp + 1] << 8);	/* Little endian ? */
		    chan->xferbuf_rp += 2;
		    Bus_Write16(value, addr);
		    break;
	    case 32:
		    if (chan->xferbuf_rp > 60) {
			    fprintf(stderr, "Emulator bug in %s line %d\n", __FILE__, __LINE__);
			    return;
		    }
		    value = chan->xfer_buf[0] | (chan->xfer_buf[1] << 8) |
			(chan->xfer_buf[2] << 16) | (chan->xfer_buf[3] << 24);
		    chan->xferbuf_rp += 4;
		    Bus_Write32(value, addr);
		    break;
	    default:
		    break;
	}
}

static void
do_dma(DMAChan * chan)
{
	IMX21Dmac *dmac = chan->dmac;
	uint32_t smod, dmod, dsiz, ssiz;
	chan->stop_dma = 0;
	int count = 0;
	//int burst_len = chan->blr & 0x3f; 
	dbgprintf("Entered DO-DMA\n");
	smod = (chan->ccr & CCR_SMOD_MASK);
	dmod = (chan->ccr & CCR_DMOD_MASK);
	switch (chan->ccr & CCR_SSIZ_MASK) {
	    case CCR_SSIZ_32:
		    ssiz = 32;
		    break;
	    case CCR_SSIZ_16:
		    ssiz = 16;
		    break;
	    case CCR_SSIZ_8:
		    ssiz = 8;
		    break;
	    default:
		    fprintf(stderr, "illegal ssiz\n");
		    return;
	}
	switch (chan->ccr & CCR_DSIZ_MASK) {
	    case CCR_DSIZ_32:
		    dsiz = 32;
		    break;
	    case CCR_DSIZ_16:
		    dsiz = 16;
		    break;
	    case CCR_DSIZ_8:
		    dsiz = 8;
		    break;
	    default:
		    fprintf(stderr, "illegal dsiz\n");
		    return;

	}
	if (chan->ccr & CCR_MDIR) {
		fprintf(stderr, "i.MX21 DMAC: downwards DMA direction not implemented\n");
		return;
	}
	while ((chan->ccnr < chan->cntr)
	       && ((SigNode_Val(chan->currReqLine) == SIG_LOW) || !(chan->ccr & CCR_REN))) {
		chan->xferbuf_wp = 0;
		chan->xferbuf_rp = 0;
		do {
			dma_read(chan, smod, ssiz);
			count += ssiz;
		} while (count < dsiz);
		do {
			dma_write(chan, dmod, dsiz);
			count -= dsiz;
		} while (count > 0);
		chan->ccnr = (ssiz > dsiz) ? chan->ccnr + (ssiz >> 3) : chan->ccnr + (dsiz >> 3);
	}
	if (chan->ccnr >= chan->cntr) {
		dmac->disr |= (1 << chan->nr);
		update_interrupts(dmac);
	}
	dbgprintf
	    ("**************** Leave Do DMA with CCNR %d of CNTR %d, disr 0x%04x, chan %d ssiz %d dsiz %d\n",
	     chan->ccnr, chan->cntr, dmac->disr, chan->nr, ssiz, dsiz);
}

static void
check_dma(DMAChan * chan)
{
	int sigval;
	sigval = SigNode_Val(chan->currReqLine);
	if (sigval == SIG_LOW) {
		do_dma(chan);
	}
}

static void
change_dmareq(SigNode * node, int value, void *clientData)
{
	DMAChan *chan = (DMAChan *) clientData;
	dbgprintf("DMAC: request line changed to state %d\n", value);
	if (value == SIG_LOW) {
		do_dma(chan);
	} else {
		chan->stop_dma = 1;
	}
}

static void
disable_dmareq(DMAChan * chan)
{
	if (chan->dmaReqTrace) {
		dbgprintf("DMAC: UnTracing dma request line\n");
		SigNode_Untrace(chan->currReqLine, chan->dmaReqTrace);
		chan->dmaReqTrace = NULL;
	}
}

static void
enable_dmareq(DMAChan * chan)
{
	IMX21Dmac *dmac = chan->dmac;
	if (chan->currReqLine != dmac->dummyHighNode) {
		dbgprintf("DMAC: Tracing dma request line\n");
		chan->dmaReqTrace = SigNode_Trace(chan->currReqLine, change_dmareq, chan);
		check_dma(chan);
	}
}

static void
update_dmareq(DMAChan * chan)
{
	IMX21Dmac *dmac = chan->dmac;
	if (!(dmac->dcr & DCR_DEN)) {
		return disable_dmareq(chan);
	}
	if (chan->ccr & (CCR_REN && CCR_CEN)) {
		enable_dmareq(chan);
	} else {
		dbgprintf("disable dmareq in %d\n", __LINE__);
		disable_dmareq(chan);
	}
}

static uint32_t
dcr_read(void *clientData, uint32_t address, int rqlen)
{
	IMX21Dmac *dmac = (IMX21Dmac *) clientData;
	return dmac->dcr;
}

static void
dcr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	IMX21Dmac *dmac = (IMX21Dmac *) clientData;
	uint32_t diff = dmac->dcr ^ value;
	int i;
	dmac->dcr = value & 7;
	if (value & DCR_DRST) {
		dmac_reset(dmac);
	}
	if (diff & DCR_DEN) {
		for (i = 0; i < 16; i++) {
			update_dmareq(&dmac->dmachan[i]);
		}
	}
	return;
}

/*
 * ---------------------------------------------------------------------
 * DISR
 * DMA interrupt status register
 * ---------------------------------------------------------------------
 */
static uint32_t
disr_read(void *clientData, uint32_t address, int rqlen)
{
	IMX21Dmac *dmac = (IMX21Dmac *) clientData;
	return dmac->disr;
}

static void
disr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	IMX21Dmac *dmac = (IMX21Dmac *) clientData;
	uint16_t clearmask = ~value;
	dmac->disr = dmac->disr & clearmask;
	update_interrupts(dmac);
	return;
}

/*
 * ------------------------------------------------------------------
 * DIMR
 *	DMA interrupt mask register
 * ------------------------------------------------------------------
 */
static uint32_t
dimr_read(void *clientData, uint32_t address, int rqlen)
{
	IMX21Dmac *dmac = (IMX21Dmac *) clientData;
	return dmac->dimr;
}

static void
dimr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	IMX21Dmac *dmac = (IMX21Dmac *) clientData;
	dmac->dimr = value;
	update_interrupts(dmac);
	return;
}

/*
 * ------------------------------------------------------------------------------
 * DBTOSR
 *	DMA Burst timeout status register
 * ------------------------------------------------------------------------------
 */
static uint32_t
dbtosr_read(void *clientData, uint32_t address, int rqlen)
{
	IMX21Dmac *dmac = (IMX21Dmac *) clientData;
	return dmac->dbtosr;
}

static void
dbtosr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	IMX21Dmac *dmac = (IMX21Dmac *) clientData;
	uint16_t clearmask = ~value;
	dmac->dbtosr &= clearmask;
	update_interrupts(dmac);
	return;
}

/*
 * ------------------------------------------------------------------------------
 * DRTOSR
 *	DMA Request timeout status register
 * ------------------------------------------------------------------------------
 */
static uint32_t
drtosr_read(void *clientData, uint32_t address, int rqlen)
{
	IMX21Dmac *dmac = (IMX21Dmac *) clientData;
	return dmac->drtosr;
}

static void
drtosr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	IMX21Dmac *dmac = (IMX21Dmac *) clientData;
	uint16_t clearmask = ~value;
	dmac->drtosr &= clearmask;
	return;
}

/*
 * ----------------------------------------------------------------------
 * DSESR
 * 	DMA transfer error status register
 * ----------------------------------------------------------------------
 */

static uint32_t
dsesr_read(void *clientData, uint32_t address, int rqlen)
{
	IMX21Dmac *dmac = (IMX21Dmac *) clientData;
	return dmac->dsesr;
}

static void
dsesr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	IMX21Dmac *dmac = (IMX21Dmac *) clientData;
	uint16_t clearmask = ~value;
	dmac->dsesr &= clearmask;
	return;
}

/*
 * ----------------------------------------------------------
 * DBOSR
 * DMA buffer overflow status register
 * ----------------------------------------------------------
 */
static uint32_t
dbosr_read(void *clientData, uint32_t address, int rqlen)
{
	IMX21Dmac *dmac = (IMX21Dmac *) clientData;
	return dmac->dbosr;
}

static void
dbosr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	IMX21Dmac *dmac = (IMX21Dmac *) clientData;
	uint16_t clearmask = ~value;
	dmac->dbosr &= clearmask;
	return;
}

/*
 * -------------------------------------------------------------------------------
 * DBTOCR
 * 	DMA burst timeout control register
 * -------------------------------------------------------------------------------
 */
static uint32_t
dbtocr_read(void *clientData, uint32_t address, int rqlen)
{
	IMX21Dmac *dmac = (IMX21Dmac *) clientData;
	return dmac->dbtocr;
}

static void
dbtocr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	IMX21Dmac *dmac = (IMX21Dmac *) clientData;
	dmac->dbtocr = value;
	return;
}

static uint32_t
wsra_read(void *clientData, uint32_t address, int rqlen)
{
	IMX21Dmac *dmac = (IMX21Dmac *) clientData;
	return dmac->wsra;
}

static void
wsra_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	IMX21Dmac *dmac = (IMX21Dmac *) clientData;
	dmac->wsra = value;
	return;
}

static uint32_t
xsra_read(void *clientData, uint32_t address, int rqlen)
{
	IMX21Dmac *dmac = (IMX21Dmac *) clientData;
	return dmac->xsra;
}

static void
xsra_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	IMX21Dmac *dmac = (IMX21Dmac *) clientData;
	dmac->xsra = value;
	return;
}

static uint32_t
ysra_read(void *clientData, uint32_t address, int rqlen)
{
	IMX21Dmac *dmac = (IMX21Dmac *) clientData;
	return dmac->ysra;
}

static void
ysra_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	IMX21Dmac *dmac = (IMX21Dmac *) clientData;
	dmac->ysra = value;
	return;
}

static uint32_t
wsrb_read(void *clientData, uint32_t address, int rqlen)
{
	IMX21Dmac *dmac = (IMX21Dmac *) clientData;
	return dmac->wsrb;
}

static void
wsrb_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	IMX21Dmac *dmac = (IMX21Dmac *) clientData;
	dmac->wsrb = value;
	return;
}

static uint32_t
xsrb_read(void *clientData, uint32_t address, int rqlen)
{
	IMX21Dmac *dmac = (IMX21Dmac *) clientData;
	return dmac->xsrb;
}

static void
xsrb_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	IMX21Dmac *dmac = (IMX21Dmac *) clientData;
	dmac->xsrb = value;
	return;
}

static uint32_t
ysrb_read(void *clientData, uint32_t address, int rqlen)
{
	IMX21Dmac *dmac = (IMX21Dmac *) clientData;
	return dmac->ysrb;
}

static void
ysrb_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	IMX21Dmac *dmac = (IMX21Dmac *) clientData;
	dmac->ysrb = value;
	return;
}

static uint32_t
sar_read(void *clientData, uint32_t address, int rqlen)
{
	DMAChan *chan = (DMAChan *) clientData;
	return chan->sar;
}

static void
sar_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	DMAChan *chan = (DMAChan *) clientData;
	chan->sar = value;
	return;
}

static uint32_t
dar_read(void *clientData, uint32_t address, int rqlen)
{
	DMAChan *chan = (DMAChan *) clientData;
	return chan->dar;
}

static void
dar_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	DMAChan *chan = (DMAChan *) clientData;
	chan->dar = value;
	return;
}

static uint32_t
cntr_read(void *clientData, uint32_t address, int rqlen)
{
	DMAChan *chan = (DMAChan *) clientData;
	return chan->cntr;
}

static void
cntr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	DMAChan *chan = (DMAChan *) clientData;
	chan->cntr = value & 0xffffff;
	return;
}

static uint32_t
ccr_read(void *clientData, uint32_t address, int rqlen)
{
	DMAChan *chan = (DMAChan *) clientData;
	return chan->ccr;
}

static void
ccr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	DMAChan *chan = (DMAChan *) clientData;
	uint16_t diff = value ^ chan->cntr;
	chan->ccr = value & 0x7fff;
	if ((diff & CCR_CEN) && (value & CCR_CEN)) {
		chan->ccnr = 0;
	}
	if (diff & (CCR_CEN | CCR_REN)) {
		/* 
		 * ---------------------------------------
		 * Check for dma_req line triggered DMA 
		 * ---------------------------------------
		 */
		update_dmareq(chan);
	}
	/* 
	 * -----------------------------------------------
	 * Check for Manually triggered DMA 
	 * -----------------------------------------------
	 */
	if ((value & CCR_CEN) && !(value & CCR_REN)) {
		do_dma(chan);
	}
	return;
}

static uint32_t
rssr_read(void *clientData, uint32_t address, int rqlen)
{
	DMAChan *chan = (DMAChan *) clientData;
	return chan->rssr;
}

static void
rssr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	DMAChan *chan = (DMAChan *) clientData;
	IMX21Dmac *dmac = chan->dmac;
	value = value & 0x1f;
	if (chan->rssr != value) {
		chan->rssr = value;
		disable_dmareq(chan);
		if (value == 0) {
			chan->currReqLine = dmac->dummyHighNode;
		} else {
			chan->currReqLine = dmac->dmaReqNode[value];
		}
		update_dmareq(chan);
	}
	return;
}

static uint32_t
blr_read(void *clientData, uint32_t address, int rqlen)
{
	DMAChan *chan = (DMAChan *) clientData;
	return chan->blr;
}

static void
blr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	DMAChan *chan = (DMAChan *) clientData;
	chan->blr = value & 0x3f;
	return;
}

/*
 * ---------------------------------------------------------------
 * Request Timeout / Bus Utilization control register
 * Currently has no effect in emulation
 * ---------------------------------------------------------------
 */

static uint32_t
rtor_bucr_read(void *clientData, uint32_t address, int rqlen)
{
	DMAChan *chan = (DMAChan *) clientData;
	if (chan->ccr & CCR_REN) {
		return chan->rtor;
	} else {
		return chan->bucr;
	}
}

static void
rtor_bucr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	DMAChan *chan = (DMAChan *) clientData;
	if (chan->ccr & CCR_REN) {
		chan->rtor = value;
	} else {
		chan->bucr = value;
	}
	return;
}

static uint32_t
ccnr_read(void *clientData, uint32_t address, int rqlen)
{
	DMAChan *chan = (DMAChan *) clientData;
	return chan->ccnr;
}

static void
ccnr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "IMX21 DMAC: CCNR not writable\n");
	return;
}

static void
IMXDmac_Unmap(void *owner, uint32_t base, uint32_t mask)
{
	int i;
	IOH_Delete32(DMA_DCR(base));
	IOH_Delete32(DMA_DISR(base));
	IOH_Delete32(DMA_DIMR(base));
	IOH_Delete32(DMA_DBTOSR(base));
	IOH_Delete32(DMA_DRTOSR(base));
	IOH_Delete32(DMA_DSESR(base));
	IOH_Delete32(DMA_DBOSR(base));
	IOH_Delete32(DMA_DBTOCR(base));

	IOH_Delete32(DMA_WSRA(base));
	IOH_Delete32(DMA_XSRA(base));
	IOH_Delete32(DMA_YSRA(base));
	IOH_Delete32(DMA_WSRB(base));
	IOH_Delete32(DMA_XSRB(base));
	IOH_Delete32(DMA_YSRB(base));

	for (i = 0; i < 16; i++) {
		IOH_Delete32(DMA_SAR(base, i));
		IOH_Delete32(DMA_DAR(base, i));
		IOH_Delete32(DMA_CNTR(base, i));
		IOH_Delete32(DMA_CCR(base, i));
		IOH_Delete32(DMA_RSSR(base, i));
		IOH_Delete32(DMA_BLR(base, i));
		IOH_Delete32(DMA_RTOR(base, i));
		IOH_Delete32(DMA_BUCR(base, i));
		IOH_Delete32(DMA_CCNR(base, i));
	}
}

static void
IMXDmac_Map(void *owner, uint32_t base, uint32_t mask, uint32_t mapflags)
{
	IMX21Dmac *dmac = (IMX21Dmac *) owner;
	int i;
	IOH_New32(DMA_DCR(base), dcr_read, dcr_write, dmac);
	IOH_New32(DMA_DISR(base), disr_read, disr_write, dmac);
	IOH_New32(DMA_DIMR(base), dimr_read, dimr_write, dmac);
	IOH_New32(DMA_DBTOSR(base), dbtosr_read, dbtosr_write, dmac);
	IOH_New32(DMA_DRTOSR(base), drtosr_read, drtosr_write, dmac);
	IOH_New32(DMA_DSESR(base), dsesr_read, dsesr_write, dmac);
	IOH_New32(DMA_DBOSR(base), dbosr_read, dbosr_write, dmac);
	IOH_New32(DMA_DBTOCR(base), dbtocr_read, dbtocr_write, dmac);

	IOH_New32(DMA_WSRA(base), wsra_read, wsra_write, dmac);
	IOH_New32(DMA_XSRA(base), xsra_read, xsra_write, dmac);
	IOH_New32(DMA_YSRA(base), ysra_read, ysra_write, dmac);
	IOH_New32(DMA_WSRB(base), wsrb_read, wsrb_write, dmac);
	IOH_New32(DMA_XSRB(base), xsrb_read, xsrb_write, dmac);
	IOH_New32(DMA_YSRB(base), ysrb_read, ysrb_write, dmac);

	for (i = 0; i < 16; i++) {
		DMAChan *chan = &dmac->dmachan[i];
		IOH_New32(DMA_SAR(base, i), sar_read, sar_write, chan);
		IOH_New32(DMA_DAR(base, i), dar_read, dar_write, chan);
		IOH_New32(DMA_CNTR(base, i), cntr_read, cntr_write, chan);
		IOH_New32(DMA_CCR(base, i), ccr_read, ccr_write, chan);
		IOH_New32(DMA_RSSR(base, i), rssr_read, rssr_write, chan);
		IOH_New32(DMA_BLR(base, i), blr_read, blr_write, chan);
		IOH_New32(DMA_RTOR(base, i), rtor_bucr_read, rtor_bucr_write, chan);
		IOH_New32(DMA_CCNR(base, i), ccnr_read, ccnr_write, chan);
	}
}

BusDevice *
IMX21_DmacNew(const char *name)
{
	IMX21Dmac *dmac = sg_new(IMX21Dmac);
	int i;
	dmac->dummyHighNode = SigNode_New("%s.high", name);
	if (!dmac->dummyHighNode) {
		fprintf(stderr, "Can not create dummy high node for DMAC\n");
	}
	SigNode_Set(dmac->dummyHighNode, SIG_HIGH);
	for (i = 0; i < 32; i++) {
		dmac->dmaReqNode[i] = SigNode_New("%s.dma_req%d", name, i);
		if (!dmac->dmaReqNode[i]) {
			fprintf(stderr,
				"i.MX21 DMA controller: Can not create DMA request node %d\n", i);
			exit(1);
		}
	}
	for (i = 0; i < 16; i++) {
		DMAChan *chan = &dmac->dmachan[i];
		chan->dmac = dmac;
		chan->nr = i;
		chan->irqNode = SigNode_New("%s.irq%d", name, i);
		chan->currReqLine = dmac->dummyHighNode;
		if (!chan->irqNode) {
			fprintf(stderr, "i.MX21 DMA controller: can not create irq node %d\n", i);
			exit(1);
		}
		SigNode_Set(chan->irqNode, SIG_PULLUP);
	}
	dmac->bdev.first_mapping = NULL;
	dmac->bdev.Map = IMXDmac_Map;
	dmac->bdev.UnMap = IMXDmac_Unmap;
	dmac->bdev.owner = dmac;
	dmac->bdev.hw_flags = MEM_FLAG_WRITABLE | MEM_FLAG_READABLE;
	fprintf(stderr, "i.MX21 DMA controller \"%s\" created\n", name);
	return &dmac->bdev;
}
