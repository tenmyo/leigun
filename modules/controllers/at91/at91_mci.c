/*
 ***********************************************************************************************
 *
 * Emulation of the AT91RM9200 Multimedia Card Interface (MCI) 
 *
 *  State: not implemented 
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
#include "at91_mci.h"
#include "mmcdev.h"
#include "mmcard.h"
#include "sgstring.h"

#define AT91MCI_MAGIC 0x384fa1b4

#define MCI_CR(base) 	((base) + 0x00)
#define		CR_SWRST	(1<<7)
#define		CR_PWSDIS	(1<<3)
#define		CR_PWSEN	(1<<2)
#define		CR_MCIDIS	(1<<1)
#define		CR_MCIEN	(1<<0)
#define MCI_MR(base) 	((base) + 0x04)
#define		MR_BLKLEN_MASK	(0xff<<18)
#define		MR_BLKLEN_SHIFT	(18)
#define		MR_PDCMODE	(1<<15)
#define		MR_PCDPADV	(1<<14)
#define		MR_PWSDIV_MASK	(7<<8)
#define		MR_PWSDIV_SHIFT	(8)
#define		MR_CLKDIV_MASK	(0xff)

#define MCI_DTOR(base)	((base) + 0x08)
#define		DTOR_DTOMUL_MASK	(7 << 4)
#define		DTOR_DTOMUL_SHIFT	(4)
#define		DTOR_DTOMUL_1		(0 << 4)
#define		DTOR_DTOMUL_16		(1 << 4)
#define		DTOR_DTOMUL_128		(2 << 4)
#define		DTOR_DTOMUL_256		(3 << 4)
#define		DTOR_DTOMUL_1K		(4 << 4)
#define		DTOR_DTOMUL_4K		(5 << 4)
#define		DTOR_DTOMUL_64K		(6 << 4)
#define		DTOR_DTOMUL_1M		(7 << 4)

#define		DTOR_DTOCYC_MASK	(0xf)
#define		DTOR_DTOCYC_SHIFT	(0)

#define	MCI_SDCR(base) 	((base) + 0x0c)
#define		SDCR_SDCSEL	(3<<0)
#define		   SDCSEL_SLOTA		(0)
#define		   SDCSEL_SLOTB		(1)
#define MCI_ARGR(base) 	((base) + 0x10)
#define MCI_CMDR(base)	((base) + 0x14)
#define		CMDR_TRTYP_MASK		(3<<19)
#define		CMDR_TRTYP_SHIFT	(19)
#define			TRTYP_SINGLE	(0<<19)
#define			TRTYP_MULTIPLE	(1<<19)
#define			TRTYP_STREAM	(2<<19)
#define		CMDR_TRDIR		(1<<18)
#define		     TRDIR_WRITE	(0<<18)
#define		     TRDIR_READ		(1<<18)
#define		CMDR_TRCMD_MASK		(3<<16)
#define              TRCMD_NONE		(0 << 16)
#define              TRCMD_START	(1 << 16)
#define		     TRCMD_STOP		(2 << 16)

#define		CMDR_MAXLAT		(1<<12)
#define		CMDR_CPDCMD		(1<<11)
#define		CMDR_SPCMD		(7<<8)
#define		CMDR_RSPTYP_MASK	(3<<6)
#define		CMDR_RSPTYP_SHIFT	(6)
#define		 	RSPTYP_NONE	(0)
#define			RSPTYP_48	(1<<6)
#define			RSPTYP_136	(2<<6)
#define		CMDR_CMDNB		(0x3f)
#define MCI_BLKR(base)	((base) + 0x18)
#define MCI_RSPR(base,n)	((base) + 0x20 + ((n) << 2))	/* 4 locations */
#define	MCI_RDR(base)	((base)	+ 0x30)
#define	MCI_TDR(base)	((base) + 0x34)
#define MCI_SR(base)	((base) + 0x40)
#define		SR_UNRE		(1<<31)
#define		SR_OVRE		(1<<30)
#define		SR_DTOE		(1<<22)
#define		SR_DCRCE	(1<<21)
#define		SR_RTOE		(1<<20)
#define		SR_RENDE	(1<<19)
#define		SR_RCRCE	(1<<18)
#define		SR_RDIRE	(1<<17)
#define		SR_RINDE	(1<<16)
#define		SR_TXBUFE	(1<<15)
#define		SR_RXBUFF	(1<<14)
#define		SR_SDIOIRQB	(1<<9)
#define		SR_SDIOIRQA	(1<<8)
#define		SR_ENDTX	(1<<7)
#define		SR_ENDRX	(1<<6)
#define		SR_NOTBUSY	(1<<5)
#define		SR_DTIP		(1<<4)
#define		SR_BLKE		(1<<3)
#define		SR_TXRDY	(1<<2)
#define		SR_RXRDY	(1<<1)
#define		SR_CMDRDY	(1<<0)
#define	MCI_IER(base)	((base) + 0x44)
#define		IER_UNRE	(1<<31)
#define		IER_OVRE	(1<<30)
#define		IER_DTOE	(1<<22)
#define		IER_DCRCE	(1<<21)
#define		IER_RTOE	(1<<20)
#define		IER_RENDE	(1<<19)
#define		IER_RCRCE	(1<<18)
#define		IER_RDIRE	(1<<17)
#define		IER_RINDE	(1<<16)
#define		IER_TXBUFE	(1<<15)
#define		IER_RXBUFF	(1<<14)
#define		IER_SDIOIRQB	(1<<9)
#define		IER_SDIOIRQA	(1<<8)
#define		IER_ENDTX	(1<<7)
#define		IER_ENDRX	(1<<6)
#define		IER_NOTBUSY	(1<<5)
#define		IER_DTIP	(1<<4)
#define		IER_BLKE	(1<<3)
#define		IER_TXRDY	(1<<2)
#define		IER_RXRDY	(1<<1)
#define		IER_CMDRDY	(1<<0)
#define	MCI_IDR(base)	((base) + 0x48)
#define MCI_IMR(base)	((base) + 0x4c)

#define UDELAY_CMD	(5)

/* Did't find a fifo size in documentation */
#define RXFIFO_SIZE	(64)
#define TXFIFO_SIZE	(64)

#define RXFIFO_WP(mci)		((mci)->rxfifo_wp % RXFIFO_SIZE)
#define RXFIFO_RP(mci)		((mci)->rxfifo_rp % RXFIFO_SIZE)
#define RXFIFO_CNT(mci)		((mci)->rxfifo_wp - (mci)->rxfifo_rp)
#define RXFIFO_ROOM(mci)	(RXFIFO_SIZE - RXFIFO_CNT(mci))

#define TXFIFO_WP(mci)		((mci)->txfifo_wp % TXFIFO_SIZE)
#define TXFIFO_RP(mci)		((mci)->txfifo_rp % TXFIFO_SIZE)
#define TXFIFO_CNT(mci)		((mci)->txfifo_wp - (mci)->txfifo_rp)
#define TXFIFO_ROOM(mci)	(TXFIFO_SIZE - TXFIFO_CNT(mci))

typedef unsigned int uint;

typedef struct AT91Mci {
	BusDevice bdev;
	char *name;
	CycleTimer cmdDelayTimer;
	Clock_t *sdclk;
	Clock_t *clkIn;
	SigNode *sigIrq;
	SigNode *nDmaReq;
	uint32_t regCR;
	uint32_t regMR;
	uint32_t regDTOR;
	uint32_t regSDCR;
	uint32_t regARGR;
	uint32_t regCMDR;
	uint32_t regBLKR;
	uint8_t rsp[17];
	unsigned int rsprRP;

	uint8_t rxfifo[RXFIFO_SIZE];
	uint32_t rxfifo_wp;
	uint32_t rxfifo_rp;

	uint64_t rxCnt;

	uint8_t txfifo[TXFIFO_SIZE];
	uint32_t txfifo_wp;
	uint32_t txfifo_rp;
	uint64_t txCnt;

	uint32_t regRDR;
	uint32_t regTDR;
	uint32_t regSR;
	uint32_t regIMR;
	MMCDev *card[2];
} AT91Mci;

static void
update_interrupt(AT91Mci * mci)
{
	if (mci->regSR & mci->regIMR) {
		SigNode_Set(mci->sigIrq, SIG_HIGH);
	} else {
		SigNode_Set(mci->sigIrq, SIG_PULLDOWN);
	}
}

static void
do_cmd_delayed(void *clientData)
{
	AT91Mci *mci = (AT91Mci *) clientData;
	MMCResponse resp;
	int rsptyp = mci->regCMDR & CMDR_RSPTYP_MASK;
	int result;
	int bus = mci->regSDCR & 1;
	int cmd = mci->regCMDR & 0x3f;
	uint32_t arg;
	int i;
	if (!mci->card[bus]) {
		fprintf(stderr, "No card bus %d %s\n", bus, mci->name);
		mci->regSR |= SR_CMDRDY | SR_RTOE;
		update_interrupt(mci);
		return;
	}
	arg = mci->regARGR;
	//dbgprintf("Start cmd 0x%02x \n",cmd);
	fprintf(stderr, "Start cmd 0x%02x \n", cmd);
	result = MMCDev_DoCmd(mci->card[bus], cmd, arg, &resp);
	if (result != MMC_ERR_NONE) {
		//fprintf(stderr,"CMD result %d\n",result);
		mci->regSR |= SR_CMDRDY | SR_RTOE;
		update_interrupt(mci);
		return;
	}
	mci->rsprRP = 0;
	/* Handle the response from SD/MMC card */
	fprintf(stderr, "RSPTYP %u\n", rsptyp);
	if (rsptyp == RSPTYP_48) {
		for (i = 0; i < 5; i++) {
			mci->rsp[i] = resp.data[i + 1];
			fprintf(stderr, "%02x\n", resp.data[i]);
		}
	} else if (rsptyp == RSPTYP_136) {
		for (i = 1; i < 16; i++) {
			mci->rsp[i] = resp.data[i + 1];
		}
	} else if (rsptyp == RSPTYP_NONE) {
		/* put nothing to fifo */
	} else {
		fprintf(stderr, "SDHC emulator: Illegal response type %d\n", rsptyp);
	}
	mci->regSR |= SR_CMDRDY;
	/*
	 * ----------------------------------------------------------
	 * For the case that the transfer involves a data packet
	 * trigger the data transaction
	 * ----------------------------------------------------------
	 */

	if (((mci->regCMDR & CMDR_TRDIR) == TRDIR_WRITE) &&
	    (mci->regCMDR & CMDR_TRCMD_MASK) == TRCMD_START) {
		//SigNode_Set(mci->nDmaReq,SIG_LOW);
	}
#if 0
	if (sdhc->cmd_dat_cont & CMD_DAT_CONT_DATA_ENABLE) {
		sdhc->transfer_count = 0;
		if (sdhc->cmd_dat_cont & CMD_DAT_CONT_WRITE) {
			if (sdhc->status & STATUS_CARD_BUS_CLK_RUN) {
				CycleTimer_Mod(&sdhc->data_delay_timer,
					       MicrosecondsToCycles(UDELAY_DATA));
			} else {
				//fprintf(stderr,"Delay Data transaction until clock is running\n");
				sdhc->forClockWaitingTimer = &sdhc->data_delay_timer;
			}
		}
	}
#endif

}

/**
 **********************************************************************************************
 * Sink for data comming from the SD-Card.
 **********************************************************************************************
 */
static int
data_sink(void *dev, const uint8_t * buf, int len)
{
	AT91Mci *mci = (AT91Mci *) dev;
	unsigned int i;
	unsigned int bus = mci->regSDCR & 1;
	/* Read direction */
	uint32_t room;
	uint32_t blockCnt;
	uint32_t blockLen;
	uint64_t expectedBytes;
	if (!mci->card[bus]) {
		fprintf(stderr, "Bug: Data for nonexisting Card\n");
		return 0;
	}
	room = RXFIFO_ROOM(mci);
	if (len > room) {
		fprintf(stderr, "MCI RX fifo overflow\n");
		return len;
	}
	blockCnt = mci->regBLKR & 0xffff;
	blockLen = (mci->regBLKR >> 16) & 0xffff;
	expectedBytes = (uint64_t) blockCnt *blockLen;
	if (blockCnt && ((len + mci->rxCnt) > expectedBytes)) {
		len = expectedBytes - mci->rxCnt;
	}
	if (len == 0) {
		return len;
	}
	for (i = 0; i < len; i++) {
		mci->rxfifo[RXFIFO_WP(mci)] = buf[i];
		mci->rxfifo_wp++;
	}
	mci->rxCnt += len;
	mci->regSR |= SR_RXRDY;
	if (blockCnt && (mci->rxCnt == expectedBytes)) {
		mci->regSR |= SR_ENDRX;
	}
	update_interrupt(mci);
	SigNode_Set(mci->nDmaReq, SIG_LOW);
	return len;
}

static void
do_transfer_write(void *clientData)
{
	AT91Mci *mci = (AT91Mci *) clientData;
	uint32_t blockCnt;
	uint32_t blockLen;
	uint64_t totalBytes;
	uint32_t transferCnt;
	uint32_t fifoCnt;
	unsigned int bus = mci->regSDCR & 1;
	int result;
	blockCnt = mci->regBLKR & 0xffff;
	blockLen = (mci->regBLKR >> 16) & 0xffff;
	if (blockCnt) {
		totalBytes = (uint64_t) blockCnt *blockLen;
	} else {
		totalBytes = ~UINT64_C(0);
	}
	fifoCnt = TXFIFO_CNT(mci);
	if ((fifoCnt + mci->txCnt) > totalBytes) {
		transferCnt = totalBytes - mci->txCnt;
	}
	if ((mci->txfifo_rp + transferCnt) > TXFIFO_SIZE) {
		result =
		    MMCDev_Write(mci->card[bus], mci->txfifo + TXFIFO_RP(mci),
				 TXFIFO_SIZE - mci->txfifo_rp);
		transferCnt -= TXFIFO_SIZE - mci->txfifo_rp;
		mci->txfifo_rp += TXFIFO_SIZE - mci->txfifo_rp;
		mci->txCnt += TXFIFO_SIZE - mci->txfifo_rp;
	}
	if (mci->txfifo_rp + transferCnt > TXFIFO_SIZE) {
		fprintf(stderr, "MCI Bug: Fifo more than full\n");
		return;
	}
	result = MMCDev_Write(mci->card[bus], mci->txfifo + TXFIFO_RP(mci), transferCnt);
	mci->txfifo_rp += transferCnt;
	mci->txCnt += transferCnt;
	SigNode_Set(mci->nDmaReq, SIG_LOW);
	/* TXRDY doesn't care about ENDTX according to figure 39.9 */
	mci->regSR |= SR_TXRDY;
	if (mci->txCnt == totalBytes) {
		mci->regSR |= SR_ENDTX;
	}
	update_interrupt(mci);

}

static uint32_t
cr_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "Register 0x%08x is writeonly\n", address);
	return 0;
}

static void
cr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	AT91Mci *mci = clientData;
	uint32_t diff = mci->regCR ^ value;
	if ((value & CR_SWRST) || (diff & CR_SWRST)) {

	}
	fprintf(stderr, "Register 0x%08x not implemented\n", address);
}

static uint32_t
mr_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "Register 0x%08x not implemented\n", address);
	return 0;
}

static void
mr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	AT91Mci *mci = clientData;
	uint32_t clkdiv = value & 0xff;
	uint32_t psdiv = (value >> 8) & 7;
	uint32_t div;
#if 0
	if (powersaving) {
		div = 2 * (clkdiv + 1) * ((1 << psdiv) + 1);
	} else {

	}
#endif
	div = 2 * (clkdiv + 1);
	Clock_MakeDerived(mci->sdclk, mci->clkIn, 1, div);
	mci->regMR = value;
	fprintf(stderr, "Register 0x%08x not implemented\n", address);
}

static uint32_t
dtor_read(void *clientData, uint32_t address, int rqlen)
{
	AT91Mci *mci = clientData;
	return mci->regDTOR;
}

static void
dtor_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{

	AT91Mci *mci = clientData;
	mci->regDTOR = value & 0x7f;
}

static uint32_t
sdcr_read(void *clientData, uint32_t address, int rqlen)
{
	AT91Mci *mci = clientData;
	return mci->regSDCR;
}

static void
sdcr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	AT91Mci *mci = clientData;
	mci->regSDCR = value & 3;
	if (mci->regSDCR > 1) {
		fprintf(stderr, "MCI: Warning, write reserved value to SDCR\n");
	}
}

static uint32_t
argr_read(void *clientData, uint32_t address, int rqlen)
{
	AT91Mci *mci = (AT91Mci *) clientData;
	return mci->regARGR;
}

static void
argr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	AT91Mci *mci = (AT91Mci *) clientData;
	mci->regARGR = value;
}

static uint32_t
cmdr_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "MCI: Command register is writeonly\n");
	return 0;
}

static void
cmdr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	AT91Mci *mci = (AT91Mci *) clientData;
	if (!(mci->regSR & SR_CMDRDY)) {
		fprintf(stderr, "MCI CMDR: There is still a command in progress\n");
		return;
	}
	mci->regCMDR = value;
	/* Should use a timer here */
	mci->regSR &= ~SR_CMDRDY;
	CycleTimer_Mod(&mci->cmdDelayTimer, MicrosecondsToCycles(UDELAY_CMD));
	fprintf(stderr, "MCI command 0x%02x started\n", value);
}

static uint32_t
blkr_read(void *clientData, uint32_t address, int rqlen)
{
	AT91Mci *mci = (AT91Mci *) clientData;
	return mci->regBLKR;
}

static void
blkr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	AT91Mci *mci = (AT91Mci *) clientData;
	mci->regBLKR = value;
}

/**
 ************************************************************************************
 * The shity documentation says nothing about byte order of rspr register.
 * Assume target endianess.
 ************************************************************************************
 */
static uint32_t
rspr_read(void *clientData, uint32_t address, int rqlen)
{
	AT91Mci *mci = (AT91Mci *) clientData;
	uint32_t value = 0;
	fprintf(stdout, "\nRSPR read, rp %d\n", mci->rsprRP);
	if (mci->rsprRP <= 12) {
		value = be32_to_target(*(uint32_t *) (mci->rsp + mci->rsprRP));
		mci->rsprRP += 4;
	}
	return value;
}

static void
rspr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "MCI response register is readonly\n");
}

static uint32_t
rdr_read(void *clientData, uint32_t address, int rqlen)
{
	AT91Mci *mci = (AT91Mci *) clientData;
	uint32_t value;
	if (RXFIFO_CNT(mci) > 3) {
		value = mci->rxfifo[RXFIFO_RP(mci)];
		mci->rxfifo_rp++;
		value = (value << 8) | mci->rxfifo[RXFIFO_RP(mci)];
		mci->rxfifo_rp++;
		value = (value << 8) | mci->rxfifo[RXFIFO_RP(mci)];
		mci->rxfifo_rp++;
		value = (value << 8) | mci->rxfifo[RXFIFO_RP(mci)];
		mci->rxfifo_rp++;
		fprintf(stderr, "Resp read %08x\n", value);
		return be32_to_target(value);
	}
	return 0;
}

static void
rdr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "MCI: RDR register is readonly\n");
}

static uint32_t
tdr_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "MCI: TDR register is writeonly\n");
	return 0;
}

static void
tdr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "Register 0x%08x not implemented\n", address);
}

static uint32_t
sr_read(void *clientData, uint32_t address, int rqlen)
{
	AT91Mci *mci = (AT91Mci *) clientData;
	return mci->regSR;
}

static void
sr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "AT91Mci: SR is readonly\n");
}

static uint32_t
ier_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "IER Register is writeonly\n");
	return 0;
}

static void
ier_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	AT91Mci *mci = (AT91Mci *) clientData;
	mci->regIMR |= value & 0xc07fc3ff;
	update_interrupt(mci);
}

static uint32_t
idr_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "IDR Register is writeonly\n");
	return 0;
}

static void
idr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	AT91Mci *mci = (AT91Mci *) clientData;
	mci->regIMR &= ~(value & 0xc07fc3ff);
	update_interrupt(mci);
}

static uint32_t
imr_read(void *clientData, uint32_t address, int rqlen)
{
	AT91Mci *mci = (AT91Mci *) clientData;
	return mci->regIMR;
}

static void
imr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "IMR register is readonly\n");
}

int
AT91Mci_InsertCard(BusDevice * dev, MMCDev * card, unsigned int slot)
{
	AT91Mci *mci = (AT91Mci *) dev->owner;
	if (dev->magic != AT91MCI_MAGIC) {
		fprintf(stderr, "Type check error: Device is not an AT91 MCI\n");
		exit(2);
	}
	if (slot > 1) {
		fprintf(stderr, "Error: AT91Mci has only two MMC/SD card Busses\n");
		return -1;
	}
	mci->card[slot] = card;
	MMCard_AddListener(card, mci, 16, data_sink);
	return 0;
}

static void
AT91Mci_Map(void *owner, uint32_t base, uint32_t mask, uint32_t flags)
{
	AT91Mci *mci = (AT91Mci *) owner;
	unsigned int i;
	IOH_New32(MCI_CR(base), cr_read, cr_write, mci);
	IOH_New32(MCI_MR(base), mr_read, mr_write, mci);
	IOH_New32(MCI_DTOR(base), dtor_read, dtor_write, mci);
	IOH_New32(MCI_SDCR(base), sdcr_read, sdcr_write, mci);
	IOH_New32(MCI_ARGR(base), argr_read, argr_write, mci);
	IOH_New32(MCI_CMDR(base), cmdr_read, cmdr_write, mci);
	IOH_New32(MCI_BLKR(base), blkr_read, blkr_write, mci);
	for (i = 0; i < 4; i++) {
		IOH_New32(MCI_RSPR(base, i), rspr_read, rspr_write, mci);
	}
	IOH_New32(MCI_RDR(base), rdr_read, rdr_write, mci);
	IOH_New32(MCI_TDR(base), tdr_read, tdr_write, mci);
	IOH_New32(MCI_SR(base), sr_read, sr_write, mci);
	IOH_New32(MCI_IER(base), ier_read, ier_write, mci);
	IOH_New32(MCI_IDR(base), idr_read, idr_write, mci);
	IOH_New32(MCI_IMR(base), imr_read, imr_write, mci);
}

static void
AT91Mci_UnMap(void *owner, uint32_t base, uint32_t mask)
{
	unsigned int i;
	//IOH_Delete32(TWI_THR(base));
	IOH_Delete32(MCI_CR(base));
	IOH_Delete32(MCI_MR(base));
	IOH_Delete32(MCI_DTOR(base));
	IOH_Delete32(MCI_SDCR(base));
	IOH_Delete32(MCI_ARGR(base));
	IOH_Delete32(MCI_CMDR(base));
	IOH_Delete32(MCI_BLKR(base));
	for (i = 0; i < 4; i++) {
		IOH_Delete32(MCI_RSPR(base, i));
	}
	IOH_Delete32(MCI_RDR(base));
	IOH_Delete32(MCI_TDR(base));
	IOH_Delete32(MCI_SR(base));
	IOH_Delete32(MCI_IER(base));
	IOH_Delete32(MCI_IDR(base));
	IOH_Delete32(MCI_IMR(base));

}

int
AT91Mci_RemoveCard(BusDevice * dev, MMCDev * card, unsigned int slot)
{
	AT91Mci *mci = dev->owner;
	if (slot > 1) {
		return -1;
	}
	if (!mci->card[slot]) {
		fprintf(stderr, "Error: SD Card Slot is already empty, can not remove\n");
		return -1;
	}
	MMCard_RemoveListener(card, mci);
	mci->card[slot] = NULL;
	return 0;
}

BusDevice *
AT91Mci_New(const char *name)
{
	AT91Mci *mci = sg_new(AT91Mci);
	mci->sigIrq = SigNode_New("%s.irq", name);
	mci->nDmaReq = SigNode_New("%s.dma_req", name);
	if (!mci->sigIrq || !mci->nDmaReq) {
		fprintf(stderr, "Can not create signal lines for AT91Mci\n");
		exit(1);
	}
	mci->name = strdup(name);
	mci->bdev.first_mapping = NULL;
	mci->bdev.Map = AT91Mci_Map;
	mci->bdev.UnMap = AT91Mci_UnMap;
	mci->bdev.owner = mci;
	mci->bdev.hw_flags = MEM_FLAG_WRITABLE | MEM_FLAG_READABLE;
	mci->bdev.magic = AT91MCI_MAGIC;
	mci->clkIn = Clock_New("%s.clk", name);
	mci->sdclk = Clock_New("%s.sd_clk", name);
	mci->regSR = 0xc0e5;
	Clock_MakeDerived(mci->sdclk, mci->clkIn, 1, 1);
	CycleTimer_Init(&mci->cmdDelayTimer, do_cmd_delayed, mci);
	update_interrupt(mci);
	fprintf(stderr, "AT91 MCI \"%s\" created\n", name);
	return &mci->bdev;
}
