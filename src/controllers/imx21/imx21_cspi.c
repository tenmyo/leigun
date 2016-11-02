/*
 *************************************************************************************************
 *
 * Emulation of Freescale iMX21 Configurable Serial Peripheral Interface (CSPI) 
 *
 * state: Basically working and tested with DS1305 realtime clock. 
 *	  Interrupts and DMA is implemented but untested
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
#include "imx21_cspi.h"
#include "signode.h"
#include "cycletimer.h"
#include "clock.h"
#include "sgstring.h"

#if 0
#define dbgprintf(x...) { fprintf(stderr,x); }
#else
#define dbgprintf(x...)
#endif


#define CSPI_RXDATA_REG(base) 	((base)+0)
#define CSPI_TXDATA_REG(base) 	((base)+0x4)
#define CSPI_CTRL_REG(base) 	((base)+0x8)
#define	 	CSPI_CTRL_BURST		(1<<23)
#define		CSPI_CTRL_SDHC_SPIEN	(1<<22)	
#define		CSPI_CTRL_SWAP		(1<<21)
#define		CSPI_CTRL_CS_MASK	(3<<19)
#define		CSPI_CTRL_CS_SHIFT	(19)
#define		CSPI_CTRL_DATARATE_MASK	(0x1f<<14)
#define		CSPI_CTRL_DATARATE_SHIFT (14)
#define		CSPI_CTRL_DRCTL_MASK	(3<<12)
#define		CSPI_CTRL_DRCTL_SHIFT	(12)
#define		CSPI_CTRL_MODE		(1<<11)
#define		CSPI_CTRL_SPIEN		(1<<10)
#define		CSPI_CTRL_XCH		(1<<9)
#define		CSPI_CTRL_SSPOL		(1<<8)
#define		CSPI_CTRL_SSCTL		(1<<7)
#define		CSPI_CTRL_PHA		(1<<6)
#define		CSPI_CTRL_POL		(1<<5)
#define		CSPI_CTRL_BITCOUNT_MASK	(0x1f)
#define		CSPI_CTRL_BITCOUNT_SHIFT (0)
#define CSPI_INT_REG(base)	((base)+0xc)
#define		CSPI_INT_BOEN		(1<<17)
#define		CSPI_INT_ROEN		(1<<16)
#define		CSPI_INT_RFEN		(1<<15)
#define 	CSPI_INT_RHEN		(1<<14)
#define		CSPI_INT_RREN		(1<<13)
#define		CSPI_INT_TSHFEEN	(1<<12)
#define		CSPI_INT_TFEN		(1<<11)
#define		CSPI_INT_THEN		(1<<10)
#define		CSPI_INT_TEEN		(1<<9)
#define		CSPI_INT_BO		(1<<8)
#define		CSPI_INT_RO		(1<<7)
#define		CSPI_INT_RF		(1<<6)
#define		CSPI_INT_RH		(1<<5)
#define		CSPI_INT_RR		(1<<4)
#define		CSPI_INT_TSHFE  	(1<<3)
#define		CSPI_INT_TF		(1<<2)
#define		CSPI_INT_TH		(1<<1)
#define		CSPI_INT_TE		(1<<0)

#define CSPI_TEST_REG(base)	((base)+0x10)
#define		CSPI_TEST_LBC		(1<<14)
#define		CSPI_TEST_INIT 		(1<<13)
#define		CSPI_TEST_SS_ASSERT	(1<<12)
#define		CSPI_TEST_SSTATUS_MASK	(0xf<<8)
#define		CSPI_TEST_SSTATUS_SHIFT	(8)
#define		CSPI_TEST_RXCNT_MASK	(0xf<<4)
#define		CSPI_TEST_RXCNT_SHIFT	(4)
#define		CSPI_TEST_TXCNT_MASK	(0xf)
#define		CSPI_TEST_TXCNT_SHIFT	(0)
#define CSPI_PERIOD_REG(base)	((base)+0x14)
#define		CSPI_PERIOD_CSCR	(1<<15)
#define		CSPI_PERIOD_WAIT_MASK	(0x7fff)
#define		CSPI_PERIOD_WAIT_SHIFT	(0)

#define CSPI_DMA_REG(base)	((base)+0x18)
#define		CSPI_DMA_THDEN	(1<<15)
#define		CSPI_DMA_TEDEN	(1<<14)
#define		CSPI_DMA_RFDEN	(1<<13)
#define		CSPI_DMA_RHDEN	(1<<12)
#define		CSPI_DMA_THDMA	(1<<7)
#define		CSPI_DMA_TEDMA	(1<<6)
#define		CSPI_DMA_RFDMA	(1<<5)
#define		CSPI_DMA_RHDMA	(1<<4)

#define CSPI_RESET_REG(base)	((base)+0x1c)
#define		CSPI_RESET_START	(1<<0)

#define CSPI_MAX_ENGINE_CMDS (1024)

typedef struct IMX21_Cspi {
	BusDevice bdev;	
	char *name;
	uint64_t rxfifo_wp;
	uint64_t rxfifo_rp;
	uint32_t rxfifo[8];
	uint64_t txfifo_wp;
	uint64_t txfifo_rp;
	uint32_t txfifo[8];
	uint32_t controlreg;
	int cs_asserted;
	uint32_t intreg;
	uint32_t dmareg;
	uint32_t testreg;
	uint32_t periodreg;
	uint32_t resetreg;
	SigNode *irqNode;

	/* Low active DMA request lines */
	SigNode *rxDmaReqNode;
	SigNode *txDmaReqNode;
	Clock_t *clk; 		/* Input from perclk 2 */
	Clock_t *spiclk; 

	/* SPI signals */
	SigNode *mosiNode;
	SigNode *misoNode;
	SigNode *sclkNode;
	SigNode *ssNode[3];

	/* The Microengine */
	int engine_is_busy;	/* For sanity check */
	uint32_t programm[CSPI_MAX_ENGINE_CMDS];
	uint64_t pgmwp;
	uint64_t pgmrp;
	uint32_t in_shiftreg;
	CycleTimer nDelayTimer;
} IMX21Cspi;

#define RXFIFO_SIZE	(8)
#define TXFIFO_SIZE	(8)
#define TXFIFO_COUNT(cspi) ((cspi)->txfifo_wp - (cspi)->txfifo_rp)
#define RXFIFO_COUNT(cspi) ((cspi)->rxfifo_wp - (cspi)->rxfifo_rp)

static void
update_interrupt(IMX21Cspi *cspi)  
{
	uint32_t inten = (cspi->intreg & 0x3fe00) >> 9;
	uint32_t intstat = cspi->intreg & 0x1ff;
	if(intstat & inten) {
		SigNode_Set(cspi->irqNode,SIG_LOW);
	} else {
		SigNode_Set(cspi->irqNode,SIG_HIGH);
	}	
}
/*
 * -----------------------------------------------------------
 * Update the dma request lines
 * -----------------------------------------------------------
 */
static void
update_rxdma(IMX21Cspi *cspi) 
{
	uint32_t rxdmaen = (cspi->dmareg >> 12) & 3;
	uint32_t rxdmastat = (cspi->dmareg >> 4) & 3;
	if(rxdmastat & rxdmaen) {
		SigNode_Set(cspi->rxDmaReqNode,SIG_LOW);
	} else {
		SigNode_Set(cspi->rxDmaReqNode,SIG_HIGH);
	}
}

static void
update_txdma(IMX21Cspi *cspi) 
{
	uint32_t txdmaen = (cspi->dmareg >> 12) & 0xc;
	uint32_t txdmastat = (cspi->dmareg >> 4) & 0xc;
	if(txdmastat & txdmaen) {
		SigNode_Set(cspi->txDmaReqNode,SIG_LOW);
	} else {
		SigNode_Set(cspi->txDmaReqNode,SIG_HIGH);
	}
}

/*
 * --------------------------------------------------------------------------------------
 * Calculate the clock divider from datarate field in control register
 * --------------------------------------------------------------------------------------
 */
static void 
update_spiclk(IMX21Cspi *cspi) 
{
	int datarate = (cspi->controlreg & CSPI_CTRL_DATARATE_MASK) >> CSPI_CTRL_DATARATE_SHIFT;
	int divider;
	if((datarate == 0) || (datarate > 0x12)) {
		fprintf(stderr,"i.MX21 CSPI: Illegal data rate %d in control register\n",datarate);
	}
	if((datarate & 1)) {
		divider  = 3 * (1<<((datarate-1)/2));
	} else {
		divider = (1<<(datarate/2)) << 1;
	}
	Clock_MakeDerived(cspi->spiclk,cspi->clk,1,divider);
} 


/*
 * -------------------------------------------------------------
 * rxdata_read
 *	Read from SPI rx fifo
 * -------------------------------------------------------------
 */
static uint32_t
rxdata_read(void *clientData,uint32_t address,int rqlen)
{
        IMX21Cspi *cspi = (IMX21Cspi *) clientData;
	uint32_t value = cspi->rxfifo[cspi->rxfifo_rp % RXFIFO_SIZE];
	int rxcount;
	uint32_t intreg;
	uint32_t dmareg;
	dbgprintf("RXDATA read 0x%08x\n",value);
	if(cspi->rxfifo_rp < cspi->rxfifo_wp) {
		cspi->rxfifo_rp++;
		rxcount = RXFIFO_COUNT(cspi); 
		intreg = cspi->intreg;
		dmareg = cspi->dmareg;
		intreg &= ~CSPI_INT_RF; /* no longer full */
		dmareg &= ~CSPI_DMA_RFDMA;
		if(rxcount < (RXFIFO_SIZE/2)) {
			intreg &= ~CSPI_INT_RH; /* less than half */
			dmareg &= ~CSPI_DMA_RHDMA;
		}
		if(rxcount == 0) {
			intreg &= ~CSPI_INT_RR; /* empty */
		}
		if(intreg != cspi->intreg) {
			cspi->intreg = intreg;
			update_interrupt(cspi);
		}
		if(dmareg != cspi->dmareg) {
			cspi->dmareg = dmareg;
			update_rxdma(cspi);
		}
	}
	if(cspi->controlreg & CSPI_CTRL_SWAP) {
		return swap32(value);
	} else {
        	return value;
	}
}

static void
rxdata_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	fprintf(stderr,"CSPI: RX-Fifo is not a writable register\n");
        return;
}

static uint32_t
txdata_read(void *clientData,uint32_t address,int rqlen)
{
	fprintf(stderr,"CSPI: TX-Fifo is a writeonly register\n");
        return 0;
}

/*
 * -------------------------------------------------------------
 * txdata_write
 *	Write to the txdata fifo
 * -------------------------------------------------------------
 */
static void
txdata_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
        IMX21Cspi *cspi = (IMX21Cspi *) clientData;
	int txcount; 
	uint32_t intreg;
	uint32_t dmareg;
	if(cspi->controlreg & CSPI_CTRL_SWAP) {
		value = swap32(value);
	}
	txcount = TXFIFO_COUNT(cspi);
	if(txcount < TXFIFO_SIZE) {
		cspi->txfifo[cspi->txfifo_wp % TXFIFO_SIZE] = value;
		cspi->txfifo_wp++; txcount++;
		intreg = cspi->intreg;
		dmareg = cspi->dmareg;
		intreg &= ~(CSPI_INT_TSHFE | CSPI_INT_TE); /* not empty any more */
		dmareg &= ~CSPI_DMA_TEDMA;
		if(txcount >= (TXFIFO_SIZE/2)) {
			intreg  |= CSPI_INT_TH; /* half full */
		}
		if(txcount > (TXFIFO_SIZE/2)) {
			dmareg  &= ~CSPI_DMA_THDMA;
		}
		if(txcount == TXFIFO_SIZE) {
			intreg  |= CSPI_INT_TF; /* full */
		}
		if(intreg != cspi->intreg) {
			cspi->intreg = intreg;
			update_interrupt(cspi);
		}
		if(dmareg != cspi->dmareg) {
			cspi->dmareg = dmareg;
			update_txdma(cspi);
		}
	} else {
		fprintf(stderr,"CSPI warning: Txfifo overflow, please change driver\n");
	}
        return;
}

#define CMD_SETACT_MOSI		(0x01000000)
#define CMD_SETPHA_SCLK		(0x02000000)
#define CMD_SETIDLE_SCLK	(0x03000000)

#define CMD_DCLK_DELAY		(0x04000000)
#define CMD_SAMPLE_MISO		(0x05000000)
#define CMD_RXFIFO_PUT		(0x06000000)
#define CMD_SAMPLE_PERIOD_DELAY (0x07000000)
#define CMD_CS_START		(0x08000000)
#define CMD_BURST_CS_ASSERT	(0x09000000)
#define CMD_BURST_CS_DEASSERT	(0x0a000000)
#define CMD_CS_STOP		(0x0b000000)

static void
setidle_sclk(IMX21Cspi *cspi) 
{
	int pol = !!(cspi->controlreg & CSPI_CTRL_POL);
	if(pol) {
		SigNode_Set(cspi->sclkNode,SIG_HIGH);
	} else {
		SigNode_Set(cspi->sclkNode,SIG_LOW);
	}
}
/*
 * --------------------------------------------------
 * SETPHA_SCLK | phase : set sclk phase relative
 * to mosi phase in multiples of 180 degree  
 * 0 = positive edge on mosi edge
 * 1 = negative edge on mosi edge
 * ....
 * --------------------------------------------------
 */
static void
setpha_sclk(IMX21Cspi *cspi,int phase) 
{
	int pol = !!(cspi->controlreg & CSPI_CTRL_POL);
	int pha = !!(cspi->controlreg & CSPI_CTRL_PHA);
	phase = pol + pha + phase;
	if(phase & 1) {
		SigNode_Set(cspi->sclkNode,SIG_HIGH);
	} else {
		SigNode_Set(cspi->sclkNode,SIG_LOW);
	}
}
/*
 * --------------------------------------------------
 * SETACT_MOSI | 0 : set mosi to inactive level  
 * SETACT_MOSI | 1 : set mosi to active level
 * --------------------------------------------------
 */
static void
setact_mosi(IMX21Cspi *cspi,int value) 
{
	if(value) {
		SigNode_Set(cspi->mosiNode,SIG_HIGH);
	} else {
		SigNode_Set(cspi->mosiNode,SIG_LOW);
	}
}

static void
sample_miso(IMX21Cspi *cspi) 
{
	int miso;
	int loopback = cspi->testreg & CSPI_TEST_LBC;
	if(unlikely(loopback)) {
		/* Sample my own mosi signal */
		miso = SigNode_Val(cspi->mosiNode);
	} else {
		miso = SigNode_Val(cspi->misoNode);
	}
	if(miso == SIG_HIGH) {
		cspi->in_shiftreg = (cspi->in_shiftreg << 1) | 1;
	} else if(miso == SIG_LOW) {
		cspi->in_shiftreg <<= 1;
	} else {
		cspi->in_shiftreg <<= 1;
	}
}

/*
 * ----------------------------------------------------------
 * put the sampled miso value to the rx fifo
 * ----------------------------------------------------------
 */
static void
rxfifo_put(IMX21Cspi *cspi) 
{
	int bits = (cspi->controlreg & CSPI_CTRL_BITCOUNT_MASK)+1;
	uint32_t mask = 0xffffffff >> (32-bits);	
	uint32_t intreg;
	uint32_t dmareg;
	int rxcount = RXFIFO_COUNT(cspi);
	if(rxcount < RXFIFO_SIZE) {
		cspi->rxfifo[cspi->rxfifo_wp % RXFIFO_SIZE] = cspi->in_shiftreg & mask;
		cspi->rxfifo_wp++; rxcount ++;
		dbgprintf("rxfifo_put %08x\n", cspi->in_shiftreg & mask);
		intreg = cspi->intreg;
		dmareg = cspi->dmareg;
		intreg |= CSPI_INT_RR; /* receive ready */
		if(rxcount >= RXFIFO_SIZE) {
			intreg |= CSPI_INT_RF; /* full */
			dmareg |= CSPI_DMA_RFDMA;
		}
		if(rxcount >= (RXFIFO_SIZE/2)) {
			intreg |= CSPI_INT_RH; /* half full */
			dmareg |= CSPI_DMA_RHDMA;
		}
		if(intreg != cspi->intreg) {
			cspi->intreg = intreg;
			update_interrupt(cspi);
		}
		if(dmareg != cspi->dmareg) {
			cspi->dmareg = dmareg;
			update_rxdma(cspi);
		}
	} else {
		/* set fifo overflow flags and trigger an interrupt */
		cspi->intreg |= CSPI_INT_RO;
		update_interrupt(cspi);
	}
}

static void
cs_assert(IMX21Cspi *cspi) 
{
	unsigned int cs = (cspi->controlreg & CSPI_CTRL_CS_MASK) >> CSPI_CTRL_CS_SHIFT;
	int sspol = !!(cspi->controlreg & CSPI_CTRL_SSPOL);
	if(cs>2) {
		fprintf(stderr,"CSPI: non existing chip select line %d\n",cs); 
		return;
	}
	if(sspol) {
		SigNode_Set(cspi->ssNode[cs],SIG_HIGH);
	} else {
		SigNode_Set(cspi->ssNode[cs],SIG_LOW);
	}
	cspi->cs_asserted = 1;
}

static void
cs_deassert(IMX21Cspi *cspi) 
{
	unsigned int cs = (cspi->controlreg & CSPI_CTRL_CS_MASK) >> CSPI_CTRL_CS_SHIFT;
	int sspol = !!(cspi->controlreg & CSPI_CTRL_SSPOL);
	if(cs>2) {
		fprintf(stderr,"CSPI: non existing chip select line %d\n",cs); 
		return;
	}
	if(sspol) {
		SigNode_Set(cspi->ssNode[cs],SIG_LOW);
	} else {
		SigNode_Set(cspi->ssNode[cs],SIG_HIGH);
	}
	cspi->cs_asserted = 0;
}

/*
 * -----------------------------------------------------------------
 * update_cs
 * Updating the chipselect allows changing Chip select line
 * and polarity at any time, even during transaction
 * -----------------------------------------------------------------
 */
static void
update_cs(IMX21Cspi *cspi) 
{
	int i;
	unsigned int cs = (cspi->controlreg & CSPI_CTRL_CS_MASK) >> CSPI_CTRL_CS_SHIFT;
	int sspol = !!(cspi->controlreg & CSPI_CTRL_SSPOL);
	int cs_asserted = cspi->cs_asserted;
	int assval,deassval,val;
	if(sspol) {
		assval = SIG_HIGH; 
		deassval = SIG_LOW;
	} else {
		assval = SIG_LOW; 
		deassval = SIG_HIGH;
	}
	for(i=0;i<3;i++) {
		if(!cspi->ssNode[i])
			continue;	
		if(cs_asserted && (cs == i)) {
			val = assval; 
		} else {
			val = deassval;
		}
		SigNode_Set(cspi->ssNode[i],val);
	}
}

static inline void 
pgm_append(IMX21Cspi *cspi,uint32_t cmd)
{
	cspi->programm[cspi->pgmwp % CSPI_MAX_ENGINE_CMDS] = cmd; 
	cspi->pgmwp++;
}

#define PGM_DONE  	(4)
#define PGM_SLEEP 	(5)
#define PGM_DO_NEXT	(6)

static void run_programm(void *clientData);

static int 
eval_one_cmd(IMX21Cspi *cspi) 
{
	uint32_t cmd;
	uint32_t arg;
	if(cspi->pgmwp == cspi->pgmrp) {
		/* refill with next cmd */
		return PGM_DONE;
	}
	cmd = cspi->programm[cspi->pgmrp % CSPI_MAX_ENGINE_CMDS];
	arg = cmd & 0x00ffffff;
	cmd = cmd & 0xff000000;
	cspi->pgmrp++;
	//fprintf(stderr,"do cmd %08x, arg %08x\n",cmd,arg);
	switch(cmd) {
		case CMD_SETACT_MOSI:
			setact_mosi(cspi,arg);
			return PGM_DO_NEXT;
			break;

		case CMD_SETPHA_SCLK:
			setpha_sclk(cspi,arg);
			return PGM_DO_NEXT;
			break;

		case CMD_DCLK_DELAY: {
			/* setup timer */
			uint32_t spihz = Clock_Freq(cspi->spiclk);	
			uint32_t nsecs;
			if(spihz) {
				nsecs  = 1000000000 / spihz;
			} else {
				nsecs = 1; 
			}
			CycleCounter_t cycles = NanosecondsToCycles(nsecs);
			CycleTimer_Mod(&cspi->nDelayTimer,cycles);
			return PGM_SLEEP;
		}

		case CMD_SAMPLE_PERIOD_DELAY: {
			/* setup timer */
			uint64_t nsecs=0;
			CycleCounter_t cycles;
			int csrc = (cspi->periodreg >> 15) & 1;
			if(csrc) {
				/* CLK 32.768 khz */
				nsecs = (cspi->periodreg & 0x7fff) * 30517;
			} else {
				uint64_t hz = Clock_Freq(cspi->spiclk);
				if(hz) {
					nsecs = (uint64_t)1000000000 
						* (uint64_t)(cspi->periodreg & 0x7fff) / hz;
				}
			}
			cycles = NanosecondsToCycles(nsecs);
			CycleTimer_Mod(&cspi->nDelayTimer,cycles);
			return PGM_SLEEP;
		}
			
			
		case CMD_SAMPLE_MISO:
			sample_miso(cspi);
			return PGM_DO_NEXT;
			break;

		case CMD_RXFIFO_PUT:
			rxfifo_put(cspi);
			return PGM_DO_NEXT;
			break;

		case CMD_SETIDLE_SCLK:
			setidle_sclk(cspi);
			return PGM_DO_NEXT;
			break;

		case CMD_CS_START:
			cs_assert(cspi);
			return PGM_DO_NEXT;

		case CMD_BURST_CS_ASSERT:
			cs_assert(cspi);
			return PGM_DO_NEXT;

		case CMD_BURST_CS_DEASSERT:
			if(cspi->controlreg & CSPI_CTRL_SSCTL) {
				cs_deassert(cspi);
			}
			return PGM_DO_NEXT;

		case CMD_CS_STOP:
			if(cspi->testreg & CSPI_TEST_SS_ASSERT) { 
				cspi->cs_asserted = 0;
			} else {
				cs_deassert(cspi);
			}
			return PGM_DO_NEXT;

		default:
			fprintf(stderr,"CSPI interpreter error: invalid cmd 0x%08x\n",cmd);
			return PGM_DONE;
			break;
	}
}


static void 
fill_shiftreg(IMX21Cspi *cspi,uint32_t outval,int bits) 
{
	int i;
	int bit;
	int phase = 0;
	//fprintf(stderr,"Fill shiftreg with %08x, bits %d\n",outval,bits);
	for(i=bits-1;i>=0;i--) {
		bit = (outval >> i) & 1;
		pgm_append(cspi,CMD_SETACT_MOSI | bit);	
		pgm_append(cspi,CMD_SETPHA_SCLK | phase++);	
		pgm_append(cspi,CMD_DCLK_DELAY  | 1);	
		pgm_append(cspi,CMD_SAMPLE_MISO);
		pgm_append(cspi,CMD_SETPHA_SCLK | phase++);	
		pgm_append(cspi,CMD_DCLK_DELAY  | 1);	
	}
	pgm_append(cspi,CMD_SETIDLE_SCLK);	
	pgm_append(cspi,CMD_RXFIFO_PUT);

}

/*
 * -------------------------------------------------------------------
 * txfifo_to_shiftreg
 *	return true if something was witten into the shiftreg
 * -------------------------------------------------------------------
 */
static int
txfifo_to_shiftreg(IMX21Cspi *cspi,int refill) 
{
	int bits = (cspi->controlreg & CSPI_CTRL_BITCOUNT_MASK)+1;
	uint32_t outval = 0;
	uint32_t intreg;
	uint32_t dmareg;
	int txcount;
	if(!(cspi->controlreg & CSPI_CTRL_XCH)) {
		fprintf(stderr,"CSPI Warning, xchg is disabled\n");
		return 0;
	}
	txcount = TXFIFO_COUNT(cspi);
	if(txcount) {
		outval = cspi->txfifo[cspi->txfifo_rp % TXFIFO_SIZE];
		cspi->txfifo_rp++;txcount--;
		intreg = cspi->intreg;
		dmareg = cspi->dmareg;
		intreg &= ~CSPI_INT_TF; /* no longer full */
		if(txcount < (TXFIFO_SIZE/2)) {
			intreg &= ~CSPI_INT_TH; /* No longer half full */
		}
		if(txcount <= (TXFIFO_SIZE/2)) {
			dmareg |= CSPI_DMA_THDMA;
		}
		if(txcount == 0) {
			intreg |= CSPI_INT_TE;
			dmareg |= CSPI_DMA_TEDMA;
		}
		if(intreg != cspi->intreg) {
			cspi->intreg = intreg;
			update_interrupt(cspi);
		}
		if(dmareg != cspi->dmareg) {
			cspi->dmareg = dmareg;
			update_txdma(cspi);
		}
	} else {
		cspi->controlreg &= ~CSPI_CTRL_XCH;
		if(!(cspi->intreg & CSPI_INT_TSHFE)) {
			cspi->intreg |= CSPI_INT_TSHFE; /* shift + fifo empty */
			update_interrupt(cspi);
		}
		/* done, deassert cs */
		if(cspi->testreg & CSPI_TEST_SS_ASSERT) { 
			/* Shit, this is a hack */
			cspi->cs_asserted = 0;
		} else {
			cs_deassert(cspi);
		}
		return 0;
	}
	/* I'm not sure if sample period is in effect everytime or only between 
	 *  of one uninterrupted transmission samples 
	 */
	if(refill) {
		pgm_append(cspi,CMD_BURST_CS_DEASSERT);
		pgm_append(cspi,CMD_DCLK_DELAY  | 1);	
		pgm_append(cspi,CMD_BURST_CS_ASSERT);
		pgm_append(cspi,CMD_SAMPLE_PERIOD_DELAY);	
	} else {
		pgm_append(cspi,CMD_CS_START);
	}
	fill_shiftreg(cspi,outval,bits);
	return 1;
}

static void
run_programm(void *clientData) 
{
	IMX21Cspi *cspi = (IMX21Cspi*) clientData;
	int result;
	if(cspi->engine_is_busy) {
		fprintf(stderr,"IMX21CSPI Warning: Microengine is busy\n");
		return;
	}
	cspi->engine_is_busy = 1;
	do {
		result = eval_one_cmd(cspi); 
		if(result == PGM_DONE) {
			/* Try to refill the shiftreg */
			if(txfifo_to_shiftreg(cspi,1) <= 0) {
				dbgprintf("Nothing to refill\n");
				break;
			}
			dbgprintf("Refilled\n");
		} else if (result == PGM_SLEEP) {
			break;
		}
	} while (1);
	cspi->engine_is_busy = 0;
}

/*
 * ----------------------------------------------------------------------------------
 * Control register
 *	Bit 23 		BURST 	    Do multiple transfers without CE going to low ??
 *	Bit 22 		SDHC_SPIEN  
 *	Bit 21 		SWAP		
 *	Bit 19-20	CS	
 *	Bit 14-18	DATARATE
 *	Bit 12-13 	DRCTL	
 *	Bit 11		MODE	SPI Master or SPI slave (1==master)
 *	Bit 10		SPIEN	Enable the CSPI module
 *	Bit 9		XCH	Trigger an exchange in master mode 
 *	Bit 8		SSPOL	Polarity of Slave select signals 0: actlow 1: acthigh
 *	Bit 7		SSCTL 	0: SS removal in burst pause 1: no SS change in pause	
 *	Bit 6		PHA	Clock data phase relationship
 *	Bit 5		POL		SPICLK polarity 0 = act high 
 *	Bit 0-4		BITCOUNT: 	0-31 = 1-32 Bits are transfered
 *	
 * ---------------------------------------------------------------------------------
 */
static uint32_t
ctrl_read(void *clientData,uint32_t address,int rqlen)
{
        IMX21Cspi *cspi = (IMX21Cspi *) clientData;
        return cspi->controlreg;
}

static void
ctrl_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
        IMX21Cspi *cspi = (IMX21Cspi *) clientData;
	uint32_t diff;
	diff = cspi->controlreg ^ value;
	cspi->controlreg = (cspi->controlreg & CSPI_CTRL_XCH) | (value & 0x00ffffff);
	dbgprintf("%s: Controlreg write %08x, modified to %08x\n",cspi->name,value,cspi->controlreg);
	update_spiclk(cspi);
	if(diff & value  & CSPI_CTRL_XCH) {
		dbgprintf("Start XCH\n");
		txfifo_to_shiftreg(cspi,0);
		run_programm(cspi);
	}
	if(diff & CSPI_CTRL_CS_MASK) {
		update_cs(cspi);
	}
        return;
}

/*
 * ------------------------------------------------------------------
 * Interrupt Control and Status register 
 * ------------------------------------------------------------------
 */
static uint32_t
int_read(void *clientData,uint32_t address,int rqlen)
{
        IMX21Cspi *cspi = (IMX21Cspi *) clientData;
        return cspi->intreg;
}

static void
int_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
        IMX21Cspi *cspi = (IMX21Cspi *) clientData;
	cspi->intreg = (cspi->intreg & 0x1ff) | (value & 0x3fe00);
	update_interrupt(cspi);
        return;
}

/*
 * ------------------------------------------------------------------------------
 * Test register
 * Bit 14: 	LBC 		Loopback control
 * Bit 13: 	INIT		Initialize the state machine 
 * Bit 12: 	ASSERT		1: do not deassert SSn after Tx shiftreg empty
 * Bits 8-11: 	SSTATUS		Internal state machine status
 * Bits 4-7:  	RXCNT		number of words in rxfifo
 * Bits 0-3:  	TXCNT		number of words in txfifo
 * ------------------------------------------------------------------------------
 */
static uint32_t
test_read(void *clientData,uint32_t address,int rqlen)
{
        IMX21Cspi *cspi = (IMX21Cspi *) clientData;
	unsigned int txcnt = TXFIFO_COUNT(cspi);
	unsigned int rxcnt = RXFIFO_COUNT(cspi);
	if((txcnt > 8) || (rxcnt > 8)) {
		fprintf(stderr,"CSPI emulator bug txcnt %d, rxcnt %d\n",txcnt,rxcnt);
		return cspi->testreg;
	}
	return (cspi->testreg & ~0xff) | txcnt | (rxcnt <<4);
}

static void
test_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
        IMX21Cspi *cspi = (IMX21Cspi *) clientData;
	cspi->testreg = value & 0x7fff;
        return;
}
static uint32_t
period_read(void *clientData,uint32_t address,int rqlen)
{
        IMX21Cspi *cspi = (IMX21Cspi *) clientData;
        return cspi->periodreg;
}

static void
period_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
        IMX21Cspi *cspi = (IMX21Cspi *) clientData;
	cspi->periodreg = value & 0xffff;
        return;
}
static uint32_t
dma_read(void *clientData,uint32_t address,int rqlen)
{
        IMX21Cspi *cspi = (IMX21Cspi *) clientData;
        return cspi->dmareg;
}

static void
dma_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
        IMX21Cspi *cspi = (IMX21Cspi *) clientData;
	cspi->dmareg = (cspi->dmareg & 0xf0) | (value & 0xf000);
	update_rxdma(cspi);
	update_txdma(cspi);
        return;
}

static void 
reset_cspi(IMX21Cspi *cspi) 
{
	CycleTimer_Remove(&cspi->nDelayTimer);
	cspi->controlreg = 0x8000;
	cspi->intreg = CSPI_INT_TE | CSPI_INT_TH | CSPI_INT_TSHFE; /* Buggy manual ! */
	cspi->dmareg = 0;
	cspi->testreg = 0;
	cspi->periodreg = 0;
	cspi->pgmrp = cspi->pgmwp = 0;
	cspi->txfifo_rp = cspi->txfifo_wp = 0;
	cspi->rxfifo_rp = cspi->rxfifo_wp = 0;
	cspi->cs_asserted = 0;
	update_interrupt(cspi);
	update_rxdma(cspi);
	update_txdma(cspi);
	update_cs(cspi);
	update_spiclk(cspi);
}

static uint32_t
reset_read(void *clientData,uint32_t address,int rqlen)
{
        return 0;
}

static void
reset_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
        IMX21Cspi *cspi = (IMX21Cspi *) clientData;
	if(value & CSPI_RESET_START) {
		reset_cspi(cspi);
	}
        return;
}

static void
IMXCspi_UnMap(void *owner,uint32_t base,uint32_t mask)
{
	IOH_Delete32(CSPI_RXDATA_REG(base));
	IOH_Delete32(CSPI_TXDATA_REG(base));
	IOH_Delete32(CSPI_CTRL_REG(base));
	IOH_Delete32(CSPI_INT_REG(base));
	IOH_Delete32(CSPI_TEST_REG(base));
	IOH_Delete32(CSPI_PERIOD_REG(base));
	IOH_Delete32(CSPI_DMA_REG(base));
	IOH_Delete32(CSPI_RESET_REG(base));
}

static void
IMXCspi_Map(void *owner,uint32_t base,uint32_t mask,uint32_t flags)
{
	IMX21Cspi *cspi = (IMX21Cspi *) owner;
	IOH_New32(CSPI_RXDATA_REG(base),rxdata_read,rxdata_write,cspi);
	IOH_New32(CSPI_TXDATA_REG(base),txdata_read,txdata_write,cspi);
	IOH_New32(CSPI_CTRL_REG(base),ctrl_read,ctrl_write,cspi);
	IOH_New32(CSPI_INT_REG(base),int_read,int_write,cspi);
	IOH_New32(CSPI_TEST_REG(base),test_read,test_write,cspi);
	IOH_New32(CSPI_PERIOD_REG(base),period_read,period_write,cspi);
	IOH_New32(CSPI_DMA_REG(base),dma_read,dma_write,cspi);
	IOH_New32(CSPI_RESET_REG(base),reset_read,reset_write,cspi);
}

BusDevice *
IMX21_CspiNew(const char *name) 
{
	IMX21Cspi *cspi = sg_new(IMX21Cspi);	
	int i;
	cspi->name = sg_strdup(name);
	cspi->irqNode = SigNode_New("%s.irq",name);
	if(!cspi->irqNode) {
		fprintf(stderr,"Can not create interrupt request line for CSPI\n");
		exit(1);
	}
	cspi->mosiNode = SigNode_New("%s.mosi",name);
	cspi->misoNode = SigNode_New("%s.miso",name);
	cspi->sclkNode = SigNode_New("%s.sclk",name);
	if(!cspi->mosiNode || !cspi->misoNode || !cspi->sclkNode) {
		fprintf(stderr,"CSPI: Can not create SPI signal lines\n");
		exit(1);
	}
	for(i=0;i<3;i++) {
		cspi->ssNode[i] = SigNode_New("%s.ss%d",name,i);
		if(!cspi->ssNode[i]) {
			fprintf(stderr,"CSPI: Can not create chip select signal line\n");
			exit(1);
		}
	}
	cspi->rxDmaReqNode = SigNode_New("%s.rx_dmareq",name);
	cspi->txDmaReqNode = SigNode_New("%s.tx_dmareq",name);
	if(!cspi->rxDmaReqNode || !cspi->txDmaReqNode) {
		fprintf(stderr,"Can not create CSPI DMA request lines\n");
		exit(1);
	}
	CycleTimer_Init(&cspi->nDelayTimer,run_programm,cspi);
	cspi->cs_asserted = 0;
	cspi->bdev.first_mapping=NULL;
        cspi->bdev.Map=IMXCspi_Map;
        cspi->bdev.UnMap=IMXCspi_UnMap;
        cspi->bdev.owner=cspi;
        cspi->bdev.hw_flags=MEM_FLAG_WRITABLE|MEM_FLAG_READABLE;
	cspi->clk = Clock_New("%s.clk",name);
	cspi->spiclk = Clock_New("%s.spiclk",name);
	Clock_SetFreq(cspi->clk,33250006); 	/* Should be connected to perclk2 and not fixed here */
	reset_cspi(cspi);
	fprintf(stderr,"i.MX21 CSPI module \"%s\" created\n",name);
	return &cspi->bdev;
}
