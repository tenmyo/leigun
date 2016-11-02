/*
 *******************************************************************************
 * SDHCI SD Card host Controller Interface as defined in 
 * the Simplified_SD_Host_Controller_Spec 
 *
 * Copyright 2011 2013 Jochen Karrer. All rights reserved.
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

#include <unistd.h>
#include <stdint.h>
#include "bus.h"
#include "sdhci.h"
#include "sgstring.h"
#include "mmcard.h"
#include "mmcdev.h"
#include "signode.h"
#include "clock.h"
#include "cycletimer.h"

#if 0
#define dbgprintf(x...) { fprintf(stderr,x); }
#else
#define dbgprintf(x...)
#endif

#define UDELAY_CMD    (20)
#define UDELAY_DATA   (20)
#define UDELAY_WRITE  (10)

#define SD_SDMA(base)	((base) + 0x000)
#define SD_BSIZE(base) 	((base) + 0x004)
#define SD_BCNT(base) 	((base) + 0x006)
#define SD_ARG(base) 	((base) + 0x008)
#define SD_TMODE(base) 	((base) + 0x00C)
#define 	TMODE_CMDINDEX_MSK	(0x3f << 24)
#define		TMODE_CMDINDEX_SHIFT	(24)
#define 	TMODE_CTYPE_MSK		(0x3 << 22)
#define 	TMODE_DATSEL		(1 << 21)
#define 	TMODE_CICHK		(1 << 20)
#define 	TMODE_CRCHK		(1 << 19)
#define		TMODE_RTYPE_MSK		(0x3 << 16)
#define		RTYPE_NO_RESP		(0 << 16)
#define		RTYPE_R2		(1 << 16)
#define		RTYPE_134567		(2 << 16)
#define		RTYPE_R1BR5B		(3 << 16)
#define		TMODE_SPI		(1 << 7)
#define 	TMODE_ATACMD		(0x3 << 6)
#define		TMODE_MS		(1 << 5)
#define 	TMODE_DIR_R		(1 << 4)
#define		TMODE_ACMD12		(1 << 2)
#define 	TMODE_BCNTEN		(1 << 1)
#define 	TMODE_DMAEN		(1 << 0)
#define SD_CMD(base) 	((base) + 0x00E)
#define SD_RESP0(base) 	((base) + 0x010)
#define SD_RESP1(base) 	((base) + 0x012)
#define SD_RESP2(base) 	((base) + 0x014)
#define SD_RESP3(base) 	((base) + 0x016)
#define SD_RESP4(base) 	((base) + 0x018)
#define SD_RESP5(base) 	((base) + 0x01A)
#define SD_RESP6(base) 	((base) + 0x01C)
#define SD_RESP7(base) 	((base) + 0x01E)
#define SD_DATAL(base) 	((base) + 0x020)
#define SD_DATAH(base) 	((base) + 0x022)
#define SD_STATE(base)  ((base) + 0x024)
#define		STATE_DAT_MSK	(0x1ef00000)
#define		STATE_CMD	(1 << 24)
#define		STATE_SDWP	(1 << 19)
#define		STATE_SDCD	(1 << 18)
#define		STATE_CDST	(1 << 17)
#define		STATE_CDIN	(1 << 16)
#define		STATE_RDEN 	(1 << 11)
#define 	STATE_WREN	(1 << 10)
#define		STATE_RDACT	(1 << 9)
#define		STATE_WRACT	(1 << 8)
#define		STATE_DATACT	(1 << 2)
#define		STATE_NODAT	(1 << 1)
#define 	STATE_NOCMD	(1 << 0)
#define SD_CONTL(base)	((base) + 0x028)
#define SD_CONTH(base)	((base) + 0x02A)
#define SD_CLK(base)	((base) + 0x02C)
#define		CLK_CLKEN	(1 << 0)
#define		CLK_CLKRDY	(1 << 1)
#define		CLK_SCKEN	(1 << 2)
#define		CLK_SDCLKSEL_MSK	(0xff << 8)
#define		CLK_SDCLKSEL_SHIFT	(8)
#define		CLK_TIMEOUT_MSK		(0xf << 16)
#define		CLK_TIMEOUT_SHIFT	(16)
#define		CLK_RSTALL		(1 << 24)
#define		CLK_RSTCMD		(1 << 25)
#define		CLK_RSTDAT		(1 << 26)
#define SD_TIME(base)		((base) + 0x02E)
#define SD_SWRESET(base)	((base) + 0x02F)
#define SD_STSL(base)	((base) + 0x030)
#define SD_STSH(base)	((base) + 0x032)
#define		STS_ERR_ADMA	(1 << 25)
#define		STS_ERR_ACMD12 	(1 << 24)
#define		STS_ERR_CLIMIT 	(1 << 23)
#define		STS_ERR_DATEND 	(1 << 22)
#define		STS_ERR_DATCRC 	(1 << 21)
#define		STS_ERR_DATTIME	(1 << 20)
#define		STS_ERR_CMDEND 	(1 << 18)
#define		STS_ERR_CMDCRC 	(1 << 17)
#define		STS_ERR_CMDTIME	(1 << 16)

#define		STS_CDINT	(1 << 8)
#define		STS_CDOUT	(1 << 7)
#define		STS_CDIN	(1 << 6)
#define		STS_RDRDY 	(1 << 5)
#define		STS_WRRDY 	(1 << 4)
#define		STS_DMA		(1 << 3)
#define		STS_BLKGAP	(1 << 2)
#define		STS_TDONE 	(1 << 1)
#define		STS_CDONE 	(1 << 0)
#define SD_STSENL(base) ((base) + 0x034)
#define SD_STSENH(base) ((base) + 0x036)
#define SD_INTENL(base) ((base) + 0x038)
#define SD_INTENH(base) ((base) + 0x03A)
#define SD_CMD12ERR(base) ((base) + 0x03C)
#define SD_CAPL(base)	((base) + 0x040)
#define SD_CAPH(base)	((base) + 0x042)
#define SD_CURL(base)	((base) + 0x048)
#define SD_CURH(base)	((base) + 0x04A)
#define SD_FORCEL(base) ((base) + 0x050)
#define SD_FORCEH(base) ((base) + 0x052)
#define SD_ADMAERR(base) ((base) + 0x054)
#define SD_ADDR0(base) 	((base) + 0x058)
#define SD_ADDR1(base) 	((base) + 0x05A)
#define SD_ADDR2(base)	((base) + 0x05C)
#define SD_ADDR3(base) 	((base) + 0x05E)
#define SD_SLOT(base) 	((base) + 0x0FC)
#define SD_VERSION(base) ((base) + 0x0FE)

#define SDHCI_MAGIC 	0x62381233

#define OBUF_SIZE	(1024U)
#define OBUF_RP(sdc)	((sdc)->outbuf_rp % OBUF_SIZE)
#define OBUF_WP(sdc)	((sdc)->outbuf_wp % OBUF_SIZE)
#define OBUF_CNT(sdc)	(uint32_t)((sdc)->outbuf_wp - (sdc)->outbuf_rp)
#define OBUF_SPACE(sdc)	(OBUF_SIZE - OBUF_CNT(sdc))

#define IBUF_SIZE	(1024U)
#define IBUF_RP(sdc)	((sdc)->inbuf_rp % IBUF_SIZE)
#define IBUF_WP(sdc)	((sdc)->inbuf_wp % IBUF_SIZE)
#define IBUF_CNT(sdc)	(uint32_t)((sdc)->inbuf_wp - (sdc)->inbuf_rp)
#define IBUF_SPACE(sdc)	(IBUF_SIZE - IBUF_CNT(sdc))

typedef struct Sdhci {
	BusDevice bdev;
	uint32_t magic;
	MMCDev *card;
	SigNode *sigIrq;
	CycleTimer cmd_delay_timer;
	CycleTimer write_delay_timer;
	Clock_t *clk_in;
	Clock_t *sdclk;
	//CycleTimer data_delay_timer;
	uint32_t transfer_blks;
	uint32_t dma_expected_bytes;
	uint32_t sdma_bufsize;

	uint8_t outbuf[OBUF_SIZE];
	uint32_t outbuf_wp;
	uint32_t outbuf_rp;
	uint8_t inbuf[IBUF_SIZE];
	uint32_t inbuf_wp;
	uint32_t inbuf_rp;
	uint32_t regSDMA;
	uint16_t regBSIZE;
	uint16_t regBCNT;
	uint32_t regARG;
	uint32_t regTMDCMD;
	uint16_t regRESP[8];
	uint16_t regDATAL;
	uint16_t regDATAH;
	uint32_t regSTATE;
	uint32_t regCONT;
	uint16_t regCLK;
	uint8_t regTIME;
	uint8_t regSWRESET;
	uint32_t regSTS;
	uint32_t regSTSEN;
	uint32_t regINTEN;
	uint32_t regCMD12ERR;
	uint32_t regCAP;
	uint16_t regCURL;
	uint16_t regCURH;
	uint16_t regFORCEL;
	uint16_t regFORCEH;
	uint16_t regADMAERR;
	uint16_t regADDR0;
	uint16_t regADDR1;
	uint16_t regADDR2;
	uint16_t regADDR3;
	uint16_t regSLOT;
	uint16_t regVERSION;
} Sdhci;

static inline void
inc_sdma(Sdhci * sdc, int amount)
{
	sdc->regSDMA += amount;
}

static void
update_clock(Sdhci * sdc)
{
	int i;
	uint32_t sdclksel;
	uint32_t divider;
	if (!(sdc->regCLK & CLK_CLKEN)) {
		Clock_MakeDerived(sdc->sdclk, sdc->clk_in, 0, 1);
		sdc->regCLK &= ~CLK_CLKRDY;
		fprintf(stderr, "CLK is disabled\n");
		return;
	}
	sdclksel = (sdc->regCLK & CLK_SDCLKSEL_MSK) >> CLK_SDCLKSEL_SHIFT;
	if (sdclksel == 0) {
		i = -1;
	} else {
		for (i = 0; i < 8; i++) {
			if (sdclksel == (1 << i)) {
				break;
			}
		}
	}
	if (i == 8) {
		fprintf(stderr, "Warning: Illegal clock value for SDCLKSEL %08x\n", sdc->regCLK);
		Clock_MakeDerived(sdc->sdclk, sdc->clk_in, sdclksel + 1, 1);
		sdc->regCLK |= CLK_CLKRDY;
		return;
	}
	divider = 1 << (i + 1);
	Clock_MakeDerived(sdc->sdclk, sdc->clk_in, 1, divider);
	if (Clock_FreqNom(sdc->clk_in) > 0) {
		sdc->regCLK |= CLK_CLKRDY;
	} else {
		sdc->regCLK &= ~CLK_CLKRDY;
		fprintf(stderr, "No input clock\n");
		sleep(3);
	}
	dbgprintf("SDHCI: updated clock\n");
	return;
}

static void
update_interrupt(Sdhci * sdc)
{
	uint32_t sts = sdc->regSTS;
	uint32_t stsen = sdc->regSTSEN;
	uint32_t inten = sdc->regINTEN;
	if (sts & stsen & inten) {
		dbgprintf("Post SD-Interrupt\n");
		SigNode_Set(sdc->sigIrq, SIG_HIGH);
	} else {
		dbgprintf("Unpost SD-Interrupt %08x %08x %08x\n", sts, stsen, inten);
		SigNode_Set(sdc->sigIrq, SIG_LOW);
	}
}

/**
 *  
 */
static uint32_t
sdma_read(void *clientData, uint32_t address, int rqlen)
{
	Sdhci *sd = clientData;
	return sd->regSDMA;
}

static void
sdma_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	Sdhci *sd = clientData;
	sd->regSDMA = value;
}

/**
 ***********************************************************************
 * Bsize: Bits 0-11 and 15 are the block size for CMD17,18,24,25 and 53 
 * SDMABUF: size of the SDMA buffer. 4-512kb
 ***********************************************************************
 */
static uint32_t
bsize_read(void *clientData, uint32_t address, int rqlen)
{
	Sdhci *sd = clientData;
	return sd->regBSIZE;
}

static void
bsize_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	Sdhci *sd = clientData;
	sd->regBSIZE = value;
}

/*
 *********************************************************************
 * Block count, only valid when Block count enable in transfer mode
 * register is enabled. 0 means zero blocks !
 *********************************************************************
 */
static uint32_t
bcnt_read(void *clientData, uint32_t address, int rqlen)
{
	Sdhci *sd = clientData;
	return sd->regBCNT;
}

static void
bcnt_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	Sdhci *sd = clientData;
	sd->regBCNT = value;
}

/**
 *******************************************************************
 * This is the 32 bit argument to the SD/MMC command
 *******************************************************************
 */
static uint32_t
arg_read(void *clientData, uint32_t address, int rqlen)
{
	Sdhci *sd = clientData;
	return sd->regARG;
}

static void
arg_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	Sdhci *sd = clientData;
	dbgprintf("SDC-ARG 0x%08x\n", value);
	sd->regARG = value;
}

/**
 *************************************************************************
 * Transfer Mode register.
 * CMDINDEX:	0-63 This is the Command sent to the card
 * CTYPE: ??
 * DATSEL: Data is present and shall be transfered. 
 * CICHK: Check if the response contains the same index as the query. 
 * CRCHK: Check the CRC field in the response.
 * RTYPE: Response Type select
 * SPI:	  Spi Mode
 * ATACMD:	Device will send command completion signal for CE-Ata
 * MS:	Multiple block select for transfer.  
 * DIR:	1: Read (Card to Host),0: Write 
 * ACMD12: Send a CMD12 automatically after last block transfer.
 * BCNTEN: Enables the Block Count register.
 * DMAEN:  Enable dma 
 *************************************************************************
 */
static uint32_t
tmode_read(void *clientData, uint32_t address, int rqlen)
{
	Sdhci *sd = clientData;
	return sd->regTMDCMD;
}

static void
tmode_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	Sdhci *sdc = clientData;
	dbgprintf("Tmode 0x%08x\n", value);
	sdc->regTMDCMD = (sdc->regTMDCMD & 0xffff0000) | (value & 0xffff);
}

/**
 *************************************************************************
 *************************************************************************
 */
static uint32_t
cmd_read(void *clientData, uint32_t address, int rqlen)
{
	Sdhci *sd = clientData;
	return sd->regTMDCMD >> 16;
}

static void
cmd_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	Sdhci *sdc = clientData;
	sdc->regTMDCMD = (sdc->regTMDCMD & 0xffff) | (value << 16);
	dbgprintf("Command 0x%08x\n", value);
	CycleTimer_Mod(&sdc->cmd_delay_timer, MicrosecondsToCycles(UDELAY_CMD));
}

static uint32_t
resp0_read(void *clientData, uint32_t address, int rqlen)
{
	Sdhci *sd = clientData;
	return sd->regRESP[0];
}

static void
resp0_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	Sdhci *sd = clientData;
	sd->regRESP[0] = value;
}

static uint32_t
resp1_read(void *clientData, uint32_t address, int rqlen)
{
	Sdhci *sd = clientData;
	return sd->regRESP[1];
}

static void
resp1_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	Sdhci *sd = clientData;
	sd->regRESP[1] = value;
}

static uint32_t
resp2_read(void *clientData, uint32_t address, int rqlen)
{
	Sdhci *sd = clientData;
	return sd->regRESP[2];
}

static void
resp2_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	Sdhci *sd = clientData;
	sd->regRESP[2] = value;
}

static uint32_t
resp3_read(void *clientData, uint32_t address, int rqlen)
{
	Sdhci *sd = clientData;
	return sd->regRESP[3];
}

static void
resp3_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	Sdhci *sd = clientData;
	sd->regRESP[3] = value;
}

static uint32_t
resp4_read(void *clientData, uint32_t address, int rqlen)
{
	Sdhci *sd = clientData;
	return sd->regRESP[4];
}

static void
resp4_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	Sdhci *sd = clientData;
	sd->regRESP[4] = value;
}

static uint32_t
resp5_read(void *clientData, uint32_t address, int rqlen)
{
	Sdhci *sd = clientData;
	return sd->regRESP[5];
}

static void
resp5_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	Sdhci *sd = clientData;
	sd->regRESP[5] = value;
}

static uint32_t
resp6_read(void *clientData, uint32_t address, int rqlen)
{
	Sdhci *sd = clientData;
	return sd->regRESP[6];
}

static void
resp6_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	Sdhci *sd = clientData;
	sd->regRESP[6] = value;
}

static uint32_t
resp7_read(void *clientData, uint32_t address, int rqlen)
{
	Sdhci *sd = clientData;
	return sd->regRESP[7];
}

static void
resp7_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	Sdhci *sd = clientData;
	sd->regRESP[7] = value;
}

static uint32_t
datal_read(void *clientData, uint32_t address, int rqlen)
{
	int i;
	Sdhci *sdc = clientData;
	int rcnt = 0;
	uint32_t value = 0;
	for (i = 0; i < rqlen; i++) {
		value <<= 8;
		if (IBUF_CNT(sdc) > 0) {
			value |= sdc->inbuf[IBUF_RP(sdc)];
			sdc->inbuf_rp++;
			rcnt++;
		}
	}
	dbgprintf("SDHCI SD: %s: data L read(%d/%d) %04x\n", __func__, rcnt, IBUF_CNT(sdc), value);
	return value;
}

/**
 * SDHC spec says on p.15:
 * The Host Controller shall not start data transmission until a
 * full block of data is written to the data port.
 */
static void
datal_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	Sdhci *sdc = clientData;
	int i;
	for (i = 0; i < rqlen; i++) {
		uint8_t data = value & 0xff;
		if (OBUF_SPACE(sdc) > 0) {
			sdc->outbuf[OBUF_WP(sdc)] = data;
			sdc->outbuf_wp++;
		}
		value >>= 8;
	}
	// Check_start_transfer
	fprintf(stderr, "SDHCI SD: %s: Register not complete\n", __func__);
}

static uint32_t
datah_read(void *clientData, uint32_t address, int rqlen)
{
	int i;
	int rcnt = 0;
	Sdhci *sdc = clientData;
	uint32_t value = 0;
	for (i = 0; i < rqlen; i++) {
		value <<= 8;
		if (IBUF_CNT(sdc) > 0) {
			value |= sdc->inbuf[IBUF_RP(sdc)];
			sdc->inbuf_rp++;
			rcnt++;
		}
	}
	dbgprintf("SDHCI SD: %s: data H read(%d/%d) %04x\n", __func__, rcnt, IBUF_CNT(sdc), value);
	return value;
}

static void
datah_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	Sdhci *sdc = clientData;
	int i;
	for (i = 0; i < rqlen; i++) {
		uint8_t data = value & 0xff;
		sdc->outbuf[OBUF_WP(sdc)] = data;
		if (OBUF_SPACE(sdc) > 0) {
			sdc->outbuf_wp++;
		}
		value >>= 8;
	}
	// Check_start_transfer
	fprintf(stderr, "SDHCI SD: %s: Register not implemented\n", __func__);
}

/**
 *******************************************************************
 * The present state
 * State of the data lines, the cmd, the Write protect pin
 * The Card detect state, Card detect debouncing,
 * Card inseted, 
 * RDEN: For non DMA read transfers: Data is readable.
 * WREN: For non DMA transfers: Data is writable.
 * RDACT: Completeion of a write transfer.
 * WRACT: Completeion of a write tranfer.
 * DATACT: Data line of SD bus is in use.
 * NODAT: Command Inhibit DAT
 * NOCMD: Indicates the CMD line is in use
 *******************************************************************
 */
static uint32_t
state_read(void *clientData, uint32_t address, int rqlen)
{
	Sdhci *sd = clientData;
#if 0
	if (!(sd->regSTATE & STATE_SDWP)) {
		fprintf(stderr, "Shit, wp enabled\n");
	}
#endif
	return sd->regSTATE;
}

static void
state_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "SDHCI SD: %s: Writing to readonly register\n", __func__);
}

/**
 *******************************************************************
 * Contl/conth
 * WKOUT: Wakeup event on card removal 
 * WKIN:  Wakeup event on card insertion
 * WKINT: Wakeup event on Card interrupt
 * BGINT: Interrupt at block Gap.
 * RDWAIT: Read Wait Control
 * CONREQ: Continue Request
 * BGSTOP: Stop at Block Gap requesto 
 * VOLTSEL: Select the card Voltage (only 3.3 Volt is supported) 
 * POW: SD Bus Power
 * DETSEL: Card detect signal selection.
 * DETCD:  Card Detect Test Level
 * SD8: 8 Bit mode
 * SELDMA: SDMA, ADMA1, ADMA2, 64 Bit ADMA2
 * HS: High speed enable
 * SD4: 4bit or 1 bit mode
 * LED: LD control
 *******************************************************************
 */
static uint32_t
contl_read(void *clientData, uint32_t address, int rqlen)
{
	Sdhci *sdc = clientData;
	fprintf(stderr, "SDHCI SD: %s: Register not tested\n", __func__);
	return sdc->regCONT & 0xffff;
}

static void
contl_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	Sdhci *sdc = clientData;
	sdc->regCONT = (sdc->regCONT & 0xffff0000) | (value & 0xffff);
	fprintf(stderr, "SDHCI SD: %s: Register not implemented\n", __func__);
}

static uint32_t
conth_read(void *clientData, uint32_t address, int rqlen)
{
	Sdhci *sdc = clientData;
	fprintf(stderr, "SDHCI SD: %s: Register not tested\n", __func__);
	return (sdc->regCONT >> 16) & 0xffff;
}

static void
conth_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	Sdhci *sdc = clientData;
	sdc->regCONT = (sdc->regCONT & 0xffff) | ((value & 0xffff) << 16);
	fprintf(stderr, "SDHCI SD: %s: 0x%08x\n", __func__, sdc->regCONT);
}

/**
 */
/**
 ************************************************************************
 * Clock and timeout control
 * RSTDAT: Reset part of data circuit
 * RSTCMD: Reset part of CMD circuit
 * RSTALL: Reset all
 * TIMEOUT: Timeout for DAT line timeouts
 * SDCLKSEL: Select the SD-Card frequency
 * SDCKEN:   Enable the SD-Clock
 * CLKRDY: Clock is stable 
 * CLKEN: Internal clock enable
 ************************************************************************
 */

static uint32_t
clk_read(void *clientData, uint32_t address, int rqlen)
{
	Sdhci *sd = clientData;
	return sd->regCLK;
}

static void
clk_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	Sdhci *sdc = clientData;
	sdc->regCLK = value;
	dbgprintf("SDC: Clock register written with 0x%08x\n", value);
	update_clock(sdc);
}

static uint32_t
time_read(void *clientData, uint32_t address, int rqlen)
{
	Sdhci *sd = clientData;
	return sd->regTIME;
}

static void
time_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	Sdhci *sd = clientData;
	sd->regTIME = value;
	fprintf(stderr, "SDHCI SD: %s: Register not tested\n", __func__);
}

static uint32_t
swreset_read(void *clientData, uint32_t address, int rqlen)
{
	Sdhci *sd = clientData;
	return sd->regSWRESET;
}

static void
swreset_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	Sdhci *sd = clientData;
	sd->regSWRESET = value;
	sd->regSWRESET = 0;
	fprintf(stderr, "SDHCI SD: %s: Register not tested\n", __func__);
}

/** 
 **********************************************************************
 * Normal interrupt status.
 * ADMA: 	ADMA error
 * ACMD12:	Auto CMD12 error
 * CLIMIT:	Current Limit Error
 * DATAEND:	Data End Bit Error
 * DATACRC:	Data CRC error
 * DATTIME: 	Data Timeout Error
 * CINDEX:	Command Index Error
 * CMDEND:	Command End Bit Error
 * CMDCRC:	Command CRC error
 * CMDTIME:	Command Timeout Error
 * ERR:		Error Interrupt
 * CDINT:	Card Detect interrupt
 * CDOUT:	Card REmoval
 * CDIN:	Card Insertion.
 * RDRDY:	Buffer Read Ready.
 * WRRDY:	Buffer Write REady
 * DMA:		DMA interrupt
 * BLKGAP:	Block Gap Event
 * TDONE:	Transfer complete
 * CDONE:	Command Complete
 * The driver uses this in a way which suggests that it is cleared by
 * writing a one. Found no hint on this in the manual.
 **********************************************************************
 */
static uint32_t
stsl_read(void *clientData, uint32_t address, int rqlen)
{
	Sdhci *sdc = clientData;
	return sdc->regSTS & 0xffff;
}

static void
stsl_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	Sdhci *sdc = clientData;
	dbgprintf("SDHCI SD: %s: clear %04x\n", __func__, value);
	sdc->regSTS &= ~(value & 0xffff);
	update_interrupt(sdc);
}

static uint32_t
stsh_read(void *clientData, uint32_t address, int rqlen)
{
	Sdhci *sdc = clientData;
	return sdc->regSTS >> 16;
}

static void
stsh_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	Sdhci *sdc = clientData;
	dbgprintf("SDHCI SD: %s: clear %04x\n", __func__, value);
	sdc->regSTS &= ~(value << 16);
	update_interrupt(sdc);
}

/**
 ***************************************************************************
 * Interrupt Status Enable
 * For fields see stsl/h
 ***************************************************************************
 */
static uint32_t
stsenl_read(void *clientData, uint32_t address, int rqlen)
{
	Sdhci *sdc = clientData;
	return sdc->regSTSEN & 0xffff;
}

static void
stsenl_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	Sdhci *sdc = clientData;
	sdc->regSTSEN = (sdc->regSTSEN & 0xffff0000) | (value & 0xffff);
	dbgprintf("SDHCI SD: %s: Register not tested\n", __func__);
	update_interrupt(sdc);
}

static uint32_t
stsenh_read(void *clientData, uint32_t address, int rqlen)
{
	Sdhci *sdc = clientData;
	return sdc->regSTSEN >> 16;
}

static void
stsenh_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	Sdhci *sdc = clientData;
	sdc->regSTSEN = (sdc->regSTSEN & 0xffff) | ((value & 0xffff) << 16);
	dbgprintf("SDHCI SD: %s: Register not implemented\n", __func__);
	update_interrupt(sdc);
}

/**
 *************************************************************************
 * Normal interrupt signal enable
 *************************************************************************
 */
static uint32_t
intenl_read(void *clientData, uint32_t address, int rqlen)
{
	Sdhci *sdc = clientData;
	return sdc->regINTEN & 0xffff;
}

static void
intenl_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	Sdhci *sdc = clientData;
	sdc->regINTEN = (sdc->regINTEN & 0xffff0000) | value;
	dbgprintf("INTEN write\n");
	update_interrupt(sdc);
}

static uint32_t
intenh_read(void *clientData, uint32_t address, int rqlen)
{
	Sdhci *sdc = clientData;
	return (sdc->regINTEN >> 16) & 0xffff;
}

static void
intenh_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	Sdhci *sdc = clientData;
	sdc->regINTEN = (sdc->regINTEN & 0xffff) | (value << 16);
	update_interrupt(sdc);
}

/**
 * Auto cmd 12 error status
 */
static uint32_t
cmd12err_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "SDHCI SD: %s: Register not implemented\n", __func__);
	return 0;
}

static void
cmd12err_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "SDHCI SD: %s: Register not implemented\n", __func__);
}

/**
 *********************************************************
 * Read the maximum current capabilites
 *********************************************************
 */
static uint32_t
capl_read(void *clientData, uint32_t address, int rqlen)
{
	Sdhci *sdc = clientData;
	return sdc->regCAP & 0xffff;
}

static void
capl_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "SDHCI SD: %s: Register is not writable\n", __func__);
}

static uint32_t
caph_read(void *clientData, uint32_t address, int rqlen)
{
	Sdhci *sdc = clientData;
	return (sdc->regCAP >> 16) & 0xffff;
}

static void
caph_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "SDHCI SD: %s: Register is not writable\n", __func__);
}

/**
 * ??????????????????????????????????????????
 */
static uint32_t
curl_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "SDHCI SD: %s: Register not implemented\n", __func__);
	return 0;
}

static void
curl_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "SDHCI SD: %s: Register not implemented\n", __func__);
}

static uint32_t
curh_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "SDHCI SD: %s: Register not implemented\n", __func__);
	return 0;
}

static void
curh_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "SDHCI SD: %s: Register not implemented\n", __func__);
}

/**
 * Force Event for error Interrupt
 */
static uint32_t
forcel_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "SDHCI SD: %s: Register not implemented\n", __func__);
	return 0;
}

static void
forcel_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "SDHCI SD: %s: Register not implemented\n", __func__);
}

static uint32_t
forceh_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "SDHCI SD: %s: Register not implemented\n", __func__);
	return 0;
}

static void
forceh_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "SDHCI SD: %s: Register not implemented\n", __func__);
}

/**
 *************************************************************************
 * ADMA error status
 * LEN: length mismatch between descriptor and block count * block lenght
 * ERRSTATE: Error state
 *************************************************************************
 */
static uint32_t
admaerr_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "SDHCI SD: %s: Register not implemented\n", __func__);
	return 0;
}

static void
admaerr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "SDHCI SD: %s: Register not implemented\n", __func__);
}

/**
 * ADMA System Address
 */
static uint32_t
addr0_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "SDHCI SD: %s: Register not implemented\n", __func__);
	return 0;
}

static void
addr0_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "SDHCI SD: %s: Register not implemented\n", __func__);
	fprintf(stderr, "Addr0 : %04x\n", value);
}

static uint32_t
addr1_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "SDHCI SD: %s: Register not implemented\n", __func__);
	return 0;
}

static void
addr1_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "SDHCI SD: %s: Register not implemented\n", __func__);
	fprintf(stderr, "Addr0 : %04x\n", value);
	exit(1);
}

static uint32_t
addr2_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "SDHCI SD: %s: Register not implemented\n", __func__);
	return 0;
}

static void
addr2_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "SDHCI SD: %s: Register not implemented\n", __func__);
}

static uint32_t
addr3_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "SDHCI SD: %s: Register not implemented\n", __func__);
	return 0;
}

static void
addr3_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "SDHCI SD: %s: Register not implemented\n", __func__);
}

/**
 * Host Controller Version / Slot interrupt status
 */

static uint32_t
slot_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "SDHCI SD: %s: Register not implemented\n", __func__);
	return 0;
}

static void
slot_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "SDHCI SD: %s: Register not implemented\n", __func__);
}

static uint32_t
version_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "SDHCI SD: %s: Register not implemented\n", __func__);
	return 0;
}

static void
version_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "SDHCI SD: %s: Register not implemented\n", __func__);
}

static void
Sdhci_Map(void *owner, uint32_t base, uint32_t mask, uint32_t _flags)
{
	Sdhci *sd = owner;
	uint32_t flgs16 =
	    IOH_FLG_OSZR_NEXT | IOH_FLG_OSZW_NEXT | IOH_FLG_PWR_RMW | IOH_FLG_PRD_RARP;
	uint32_t flgs32 = IOH_FLG_PWR_RMW | IOH_FLG_PRD_RARP;
	IOH_New32f(SD_SDMA(base), sdma_read, sdma_write, sd, flgs32);
	IOH_New16f(SD_BSIZE(base), bsize_read, bsize_write, sd, flgs16);
	IOH_New16f(SD_BCNT(base), bcnt_read, bcnt_write, sd, flgs16);
	IOH_New32f(SD_ARG(base), arg_read, arg_write, sd, flgs32);
	IOH_New16f(SD_TMODE(base), tmode_read, tmode_write, sd, flgs16);
	IOH_New16f(SD_CMD(base), cmd_read, cmd_write, sd, flgs16);
	IOH_New16f(SD_RESP0(base), resp0_read, resp0_write, sd, flgs16);
	IOH_New16f(SD_RESP1(base), resp1_read, resp1_write, sd, flgs16);
	IOH_New16f(SD_RESP2(base), resp2_read, resp2_write, sd, flgs16);
	IOH_New16f(SD_RESP3(base), resp3_read, resp3_write, sd, flgs16);
	IOH_New16f(SD_RESP4(base), resp4_read, resp4_write, sd, flgs16);
	IOH_New16f(SD_RESP5(base), resp5_read, resp5_write, sd, flgs16);
	IOH_New16f(SD_RESP6(base), resp6_read, resp6_write, sd, flgs16);
	IOH_New16f(SD_RESP7(base), resp7_read, resp7_write, sd, flgs16);
	IOH_New16f(SD_DATAL(base), datal_read, datal_write, sd, flgs16);
	IOH_New16f(SD_DATAH(base), datah_read, datah_write, sd, flgs16);
	IOH_New16f(SD_STATE(base), state_read, state_write, sd, flgs32);
	IOH_New16f(SD_CONTL(base), contl_read, contl_write, sd, flgs16);
	IOH_New16f(SD_CONTH(base), conth_read, conth_write, sd, flgs16);
	IOH_New16f(SD_CLK(base), clk_read, clk_write, sd, flgs16);
	IOH_New8f(SD_TIME(base), time_read, time_write, sd, flgs16);
	IOH_New8f(SD_SWRESET(base), swreset_read, swreset_write, sd, flgs16);
	IOH_New16f(SD_STSL(base), stsl_read, stsl_write, sd, flgs16);
	IOH_New16f(SD_STSH(base), stsh_read, stsh_write, sd, flgs16);
	IOH_New16f(SD_STSENL(base), stsenl_read, stsenl_write, sd, flgs16);
	IOH_New16f(SD_STSENH(base), stsenh_read, stsenh_write, sd, flgs16);
	IOH_New16f(SD_INTENL(base), intenl_read, intenl_write, sd, flgs16);
	IOH_New16f(SD_INTENH(base), intenh_read, intenh_write, sd, flgs16);
	IOH_New32f(SD_CMD12ERR(base), cmd12err_read, cmd12err_write, sd, flgs32);
	IOH_New16f(SD_CAPL(base), capl_read, capl_write, sd, flgs16);
	IOH_New16f(SD_CAPH(base), caph_read, caph_write, sd, flgs16);
	IOH_New16f(SD_CURL(base), curl_read, curl_write, sd, flgs16);
	IOH_New16f(SD_CURH(base), curh_read, curh_write, sd, flgs16);
	IOH_New16f(SD_FORCEL(base), forcel_read, forcel_write, sd, flgs16);
	IOH_New16f(SD_FORCEH(base), forceh_read, forceh_write, sd, flgs16);
	IOH_New32f(SD_ADMAERR(base), admaerr_read, admaerr_write, sd, flgs32);
	IOH_New16f(SD_ADDR0(base), addr0_read, addr0_write, sd, flgs16);
	IOH_New16f(SD_ADDR1(base), addr1_read, addr1_write, sd, flgs16);
	IOH_New16f(SD_ADDR2(base), addr2_read, addr2_write, sd, flgs16);
	IOH_New16f(SD_ADDR3(base), addr3_read, addr3_write, sd, flgs16);
	IOH_New16f(SD_SLOT(base), slot_read, slot_write, sd, flgs16);
	IOH_New16f(SD_VERSION(base), version_read, version_write, sd, flgs16);
}

static void
Sdhci_UnMap(void *owner, uint32_t base, uint32_t mask)
{
	IOH_Delete32(SD_SDMA(base));
	IOH_Delete16(SD_BSIZE(base));
	IOH_Delete16(SD_BCNT(base));
	IOH_Delete32(SD_ARG(base));
	IOH_Delete16(SD_TMODE(base));
	IOH_Delete16(SD_CMD(base));
	IOH_Delete16(SD_RESP0(base));
	IOH_Delete16(SD_RESP1(base));
	IOH_Delete16(SD_RESP2(base));
	IOH_Delete16(SD_RESP3(base));
	IOH_Delete16(SD_RESP4(base));
	IOH_Delete16(SD_RESP5(base));
	IOH_Delete16(SD_RESP6(base));
	IOH_Delete16(SD_RESP7(base));
	IOH_Delete16(SD_DATAL(base));
	IOH_Delete16(SD_DATAH(base));
	IOH_Delete32(SD_STATE(base));
	IOH_Delete16(SD_CONTL(base));
	IOH_Delete16(SD_CONTH(base));
	IOH_Delete16(SD_CLK(base));
	IOH_Delete8(SD_TIME(base));
	IOH_Delete8(SD_SWRESET(base));
	IOH_Delete16(SD_STSL(base));
	IOH_Delete16(SD_STSH(base));
	IOH_Delete16(SD_STSENL(base));
	IOH_Delete16(SD_STSENH(base));
	IOH_Delete16(SD_INTENL(base));
	IOH_Delete16(SD_INTENH(base));
	IOH_Delete32(SD_CMD12ERR(base));
	IOH_Delete16(SD_CAPL(base));
	IOH_Delete16(SD_CAPH(base));
	IOH_Delete16(SD_CURL(base));
	IOH_Delete16(SD_CURH(base));
	IOH_Delete16(SD_FORCEL(base));
	IOH_Delete16(SD_FORCEH(base));
	IOH_Delete32(SD_ADMAERR(base));
	IOH_Delete16(SD_ADDR0(base));
	IOH_Delete16(SD_ADDR1(base));
	IOH_Delete16(SD_ADDR2(base));
	IOH_Delete16(SD_ADDR3(base));
	IOH_Delete16(SD_SLOT(base));
	IOH_Delete16(SD_VERSION(base));
}

static int
receive_data(void *dev, const uint8_t * buf, int len)
{
	Sdhci *sdc = dev;
	int i;
	//fprintf(stderr,"GOT %d bytes of data, expected %d, addr SDMA 0x%08x\n",len,sdc->dma_expected_bytes,sdc->regSDMA);

	if (sdc->dma_expected_bytes /* && DMAC_ENABLED */ ) {
		int cnt;
		if (len > sdc->dma_expected_bytes) {
			cnt = sdc->dma_expected_bytes;
		} else {
			cnt = len;
		}
		for (i = 0; i < cnt; i++) {
			if (sdc->regTMDCMD & TMODE_DMAEN) {
				Bus_Write8(buf[i], sdc->regSDMA);
				inc_sdma(sdc, 1);
				if ((sdc->regSDMA & (sdc->sdma_bufsize - 1)) == 0) {
					dbgprintf("SDMA buffer interrupt not implemented\n");
				}
				sdc->dma_expected_bytes--;
			} else {
				fprintf(stderr, "%02x - ", buf[i]);
				usleep(10000);
				sdc->inbuf[IBUF_WP(sdc)] = buf[i];
				sdc->inbuf_wp += 1;
			}
		}
	}
	if (sdc->dma_expected_bytes == 0) {

#if 1
		if (sdc->regTMDCMD & TMODE_MS) {
			if ((sdc->regTMDCMD & TMODE_BCNTEN) && sdc->regBCNT) {
				sdc->regBCNT--;
				if (sdc->regBCNT) {
					sdc->dma_expected_bytes =
					    (sdc->
					     regBSIZE & 0xfff) | ((sdc->regBSIZE >> 3) & 0x1000);
					return len;
				} else if (sdc->regTMDCMD & TMODE_ACMD12) {
					MMCResponse resp;
					int result;
					if (sdc->card) {
						result = MMCDev_DoCmd(sdc->card, 12, 0, &resp);
						if (result == MMC_ERR_NONE) {
							sdc->regRESP[6] =
							    (resp.data[3] << 8) | resp.data[4];
							sdc->regRESP[7] =
							    (resp.data[1] << 8) | resp.data[2];
						}
					}
					//fprintf(stderr,"Auto CMD 12\n");
					//sleep(1);
				}
			} else {
				sdc->dma_expected_bytes =
				    (sdc->regBSIZE & 0xfff) | ((sdc->regBSIZE >> 3) & 0x1000);
			}
		}
#endif
		sdc->regSTATE &= ~STATE_DATACT;
		sdc->regSTS |= STS_TDONE;
		//dbgprintf(stderr,"STS_DATADONE***\n");
		update_interrupt(sdc);
	}
	return len;
}

static void
do_transfer_write(void *clientData)
{
	Sdhci *sdc = clientData;
	int result;
	uint8_t buf[128];
	int count = 0;
	if ((sdc->regTMDCMD & TMODE_DIR_R) || !(sdc->regSTATE & STATE_DATACT)) {
		fprintf(stderr, "SD-Card controller: Unexpected data write\n");
		return;
	}
	while (count < sizeof(buf) && sdc->dma_expected_bytes /* && DMAC_ENABLED */ ) {
		if (sdc->regTMDCMD & TMODE_DMAEN) {
			buf[count] = Bus_Read8(sdc->regSDMA);
		} else {
			// fetch from fifo
		}
		inc_sdma(sdc, 1);
		//sdc->regSDMA++;
		sdc->dma_expected_bytes--;
		count++;
	}
	if (count) {
		if (sdc->card) {
			result = MMCDev_Write(sdc->card, buf, count);
			if (result) {
				/* do something */
			}
		}
	}
	dbgprintf("Written %d bytes\n", count);
	if (sdc->dma_expected_bytes == 0) {
		if (sdc->regTMDCMD & TMODE_MS) {
			if ((sdc->regTMDCMD & TMODE_BCNTEN) && sdc->regBCNT) {
				sdc->regBCNT--;
				if (sdc->regBCNT) {
					dbgprintf("BCNT now %d\n", sdc->regBCNT);
					sdc->dma_expected_bytes =
					    (sdc->
					     regBSIZE & 0xfff) | ((sdc->regBSIZE >> 3) & 0x1000);
					CycleTimer_Mod(&sdc->write_delay_timer,
						       MicrosecondsToCycles(UDELAY_WRITE));
					return;
				} else if (sdc->regTMDCMD & TMODE_ACMD12) {
					MMCResponse resp;
					int result;
					if (sdc->card) {
						result = MMCDev_DoCmd(sdc->card, 12, 0, &resp);
						if (result == MMC_ERR_NONE) {
							sdc->regRESP[6] =
							    (resp.data[3] << 8) | resp.data[4];
							sdc->regRESP[7] =
							    (resp.data[1] << 8) | resp.data[2];
						}
					}
					//fprintf(stderr,"Write Multi: Auto CMD 12\n");
					//sleep(1);
				}
			} else {
				sdc->dma_expected_bytes =
				    (sdc->regBSIZE & 0xfff) | ((sdc->regBSIZE >> 3) & 0x1000);
			}
		}
		sdc->regSTATE &= ~STATE_DATACT;
		sdc->regSTS |= STS_TDONE;
		//fprintf(stderr,"STS_DATADONE WRITE*****************\n");
		update_interrupt(sdc);
	} else {
		CycleTimer_Mod(&sdc->write_delay_timer, MicrosecondsToCycles(UDELAY_WRITE));
	}
	return;
}

/**
 ***********************************************************************************************
 * Do the command delayed by the typicall time required by a command
 ***********************************************************************************************
 */
static void
do_cmd_delayed(void *clientData)
{
	Sdhci *sdc = (Sdhci *) clientData;
	MMCResponse resp;
	int rtype = sdc->regTMDCMD & TMODE_RTYPE_MSK;
	int result;
	uint8_t cmd = (sdc->regTMDCMD & TMODE_CMDINDEX_MSK) >> TMODE_CMDINDEX_SHIFT;
	uint32_t arg;
	if (!sdc->card) {
		sdc->regSTATE &= ~(STATE_NOCMD | STATE_NODAT | STATE_DATACT
				   | STATE_WRACT | STATE_RDACT | STATE_RDEN);
		sdc->regSTS |= STS_ERR_CMDTIME;
		update_interrupt(sdc);
		//update_interrupt(sdc);
		return;
	}
	arg = sdc->regARG;
	result = MMCDev_DoCmd(sdc->card, cmd, sdc->regARG, &resp);
	dbgprintf("Done cmd %d with result %d\n", cmd, result);
	//sleep(1);
	if (result != MMC_ERR_NONE) {
		/* Hack, I don't know real response */
		sdc->regSTATE &= ~(STATE_NOCMD | STATE_NODAT | STATE_DATACT
				   | STATE_WRACT | STATE_RDACT | STATE_RDEN);
		//sdc->regSTS |= STS_CDONE;
#if 0
		if (result == MMC_ERR_BADCRC) {
			sdc->regSTS |= STS_ERR_CMDCRC;
		} else {
#endif
			sdc->regSTS |= STS_ERR_CMDTIME;
			update_interrupt(sdc);
			return;
		}
		if (resp.len > 17) {
			fprintf(stderr,
				"Emulator bug: SD/MMC card response longer than 17 Bytes\n");
			resp.len = 16;
		}
		/* Handle the response from SD/MMC card */
		dbgprintf("RTYPE %08x\n", rtype);
		if (rtype == RTYPE_NO_RESP) {
		} else if (rtype == RTYPE_R2) {
			fprintf(stderr, "RESPONSE_TYPE R2\n");
			sdc->regRESP[0] = resp.data[15] | (resp.data[14] << 8);
			sdc->regRESP[1] = resp.data[13] | (resp.data[12] << 8);
			sdc->regRESP[2] = resp.data[11] | (resp.data[10] << 8);
			sdc->regRESP[3] = resp.data[9] | (resp.data[8] << 8);
			sdc->regRESP[4] = resp.data[7] | (resp.data[6] << 8);
			sdc->regRESP[5] = resp.data[5] | (resp.data[4] << 8);
			sdc->regRESP[6] = resp.data[3] | (resp.data[2] << 8);
			sdc->regRESP[7] = resp.data[1] | (resp.data[0] << 8);
		} else if (rtype == RTYPE_134567) {
			sdc->regRESP[0] = (resp.data[3] << 8) | resp.data[4];
			sdc->regRESP[1] = (resp.data[1] << 8) | resp.data[2];
		} else if (rtype == RTYPE_R1BR5B) {
			sdc->regRESP[0] = (resp.data[3] << 8) | resp.data[4];
			sdc->regRESP[1] = (resp.data[1] << 8) | resp.data[2];
		} else {
			fprintf(stderr, "nonexisting RTYPE %d\n", rtype);
			exit(1);
		}
		sdc->regSTATE &= ~(STATE_NOCMD);
		sdc->regSTS |= STS_CDONE;
		update_interrupt(sdc);
		if (sdc->regTMDCMD & TMODE_DATSEL) {
			if ((sdc->regTMDCMD & TMODE_MS) && (sdc->regTMDCMD & TMODE_BCNTEN)) {
				if (sdc->regBCNT == 0) {
					return;
				}
			}
			sdc->dma_expected_bytes =
			    (sdc->regBSIZE & 0xfff) | ((sdc->regBSIZE >> 3) & 0x1000);
			sdc->sdma_bufsize = 4096 << ((sdc->regBSIZE >> 12) & 7);
			sdc->regSTATE |= STATE_DATACT;
			if (sdc->regTMDCMD & TMODE_DIR_R) {
				//CycleTimer_Mod(&sdc->data_delay_timer,MicrosecondsToCycles(UDELAY_DATA));
			} else {
				/* Direction is write */
				/* do data_write */
				CycleTimer_Mod(&sdc->write_delay_timer,
					       MicrosecondsToCycles(UDELAY_WRITE));
			}
		} else {
			sdc->regSTATE &= ~(STATE_NODAT);	/* ? */
		}
	}

	int
	 SDHCI_InsertCard(BusDevice * dev, MMCDev * card) {
		Sdhci *sdc = dev->owner;
		if (sdc->magic != SDHCI_MAGIC) {
			fprintf(stderr, "Inserted SD card into wrong device\n");
			exit(1);
		}
		if (sdc->card) {
			fprintf(stderr, "Error: Can not plug a second SD-Card\n");
			return -1;
		}
		sdc->card = card;
		/* Card present and not write protected */
		sdc->regSTATE |= STATE_SDCD | STATE_SDWP | STATE_CDST;
		MMCard_AddListener(card, sdc, 64, receive_data);
		return 0;
	}

	BusDevice *SDHCI_New(const char *name) {
		Sdhci *sdc = sg_new(Sdhci);
		sdc->magic = SDHCI_MAGIC;
		sdc->bdev.first_mapping = NULL;
		sdc->bdev.Map = Sdhci_Map;
		sdc->bdev.UnMap = Sdhci_UnMap;
		sdc->bdev.owner = sdc;
		sdc->bdev.hw_flags = MEM_FLAG_WRITABLE | MEM_FLAG_READABLE;
		sdc->sigIrq = SigNode_New("%s.irq", name);
		if (!sdc->sigIrq) {
			fprintf(stderr, "Can not create Interrupt line for SD-Card controller\n");
			exit(1);
		}
		sdc->clk_in = Clock_New("%s.clk", name);
		sdc->sdclk = Clock_New("%s.sdclk", name);
		if (!sdc->clk_in || !sdc->sdclk) {
			fprintf(stderr, "Can not create clocks for SD-Host \"%s\"\n", name);
			exit(1);
		}
		update_clock(sdc);
		CycleTimer_Init(&sdc->cmd_delay_timer, do_cmd_delayed, sdc);
		//CycleTimer_Init(&sdc->data_delay_timer,do_data_delayed,sdc);
		CycleTimer_Init(&sdc->write_delay_timer, do_transfer_write, sdc);
		sdc->regCAP = 0x69ef30b0;
		return &sdc->bdev;
	}
