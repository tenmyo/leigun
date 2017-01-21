/*
 *************************************************************************************************
 *
 * Emulation of the Microchip ENC28J60 Ethernet Controller
 *
 * State:
 *	Working, MAC Hash Table filter and Pattern match filter not tested 
 *
 *
 * Copyright 2012 Jochen Karrer. All rights reserved.
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
// include self header
#include "compiler_extensions.h"
#include "enc28j60.h"

// include system header
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

// include library header

// include user header
#include "cycletimer.h"
#include "configfile.h"
#include "linux-tap.h"
#include "crc32.h"
#include "signode.h"
#include "sgstring.h"
#include "core/asyncmanager.h"


#define DROP 0
/* High nibble is Bank Nr */
#define OFS_BANK0	(0)
#define OFS_BANK1	(0x20)
#define OFS_BANK2	(0x40)
#define OFS_BANK3	(0x60)
#define REG_WAIT	(0x00)

#define REG_ERDPTL	(OFS_BANK0 | 0x00)
#define REG_ERDPTH	(OFS_BANK0 | 0x01)
#define REG_EWRPTL	(OFS_BANK0 | 0x02)
#define REG_EWRPTH	(OFS_BANK0 | 0x03)
#define REG_ETXSTL	(OFS_BANK0 | 0x04)
#define	REG_ETXSTH	(OFS_BANK0 | 0X05)
#define REG_ETXNDL	(OFS_BANK0 | 0x06)
#define REG_ETXNDH	(OFS_BANK0 | 0x07)
#define REG_ERXSTL	(OFS_BANK0 | 0x08)
#define REG_ERXSTH	(OFS_BANK0 | 0x09)
#define REG_ERXNDL	(OFS_BANK0 | 0x0a)
#define REG_ERXNDH	(OFS_BANK0 | 0x0b)
#define REG_ERXRDPTL	(OFS_BANK0 | 0x0c)
#define REG_ERXRDPTH	(OFS_BANK0 | 0x0d)
#define REG_ERXWRPTL 	(OFS_BANK0 | 0x0e)
#define REG_ERXWRPTH	(OFS_BANK0 | 0x0f)
#define REG_EDMASTL	(OFS_BANK0 | 0x10)
#define REG_EDMASTH	(OFS_BANK0 | 0x11)
#define REG_EDMANDL	(OFS_BANK0 | 0x12)
#define REG_EDMANDH	(OFS_BANK0 | 0x13)
#define REG_EDMADSTL	(OFS_BANK0 | 0x14)
#define REG_EDMADSTH	(OFS_BANK0 | 0x15)
#define REG_EDMACSL	(OFS_BANK0 | 0x16)
#define REG_EDMACSH	(OFS_BANK0 | 0x17)

#define REG_EIE		(OFS_BANK0 | 0x1b)
#define		EIE_INTIE	(1 << 7)
#define		EIE_PKTIE	(1 << 6)
#define		EIE_DMAIE	(1 << 5)
#define 	EIE_LINKIE	(1 << 4)
#define		EIE_TXIE	(1 << 3)
#define		EIE_WOLIE	(1 << 2)
#define 	EIE_TXERIE	(1 << 1)
#define		EIE_RXERIE	(1 << 0)

#define REG_EIR		(OFS_BANK0 | 0x1c)
#define		EIR_PKTIF	(1 << 6)
#define		EIR_DMAIF	(1 << 5)
#define 	EIR_LINKIF	(1 << 4)
#define		EIR_TXIF	(1 << 3)
#define		EIR_WOLIF	(1 << 2)
#define		EIR_TXERIF	(1 << 1)
#define		EIR_RXERIF	(1 << 0)

#define REG_ESTAT	(OFS_BANK0 | 0x1d)
#define		ESTAT_INT	(1 << 7)
#define		ESTAT_LATECOL	(1 << 4)
#define		ESTAT_RXBUSY	(1 << 2)
#define 	ESTAT_TXABRT	(1 << 1)
#define 	ESTAT_CLKRDY	(1 << 0)
#define REG_ECON2	(OFS_BANK0 | 0x1e)
#define 	ECON2_AUTOINC	(1 << 7)
#define		ECON2_PKTDEC	(1 << 6)
#define		ECON2_PWRSV	(1 << 5)
#define		ECON2_VRPS	(1 << 3)

#define REG_ECON1	(OFS_BANK0 | 0x1f)
#define		ECON1_TXRST	(1 << 7)
#define		ECON1_RXRST	(1 << 6)
#define		ECON1_DMAST	(1 << 5)
#define		ECON1_CSUMEN	(1 << 4)
#define		ECON1_TXRTS	(1 << 3)
#define		ECON1_RXEN	(1 << 2)
#define		ECON1_BSEL1	(1 << 1)
#define 	ECON1_BSEL1_SHIFT	(1)
#define		ECON1_BSEL0	(1 << 0)
#define 	ECON1_BSEL0_SHIFT	(0)

#define REG_EHT0	(OFS_BANK1 | 0x00)
#define REG_EHT1	(OFS_BANK1 | 0x01)
#define REG_EHT2	(OFS_BANK1 | 0x02)
#define REG_EHT3	(OFS_BANK1 | 0x03)
#define REG_EHT4	(OFS_BANK1 | 0x04)
#define REG_EHT5	(OFS_BANK1 | 0x05)
#define REG_EHT6	(OFS_BANK1 | 0x06)
#define REG_EHT7	(OFS_BANK1 | 0x07)
#define REG_EPMM0	(OFS_BANK1 | 0x08)
#define REG_EPMM1 	(OFS_BANK1 | 0x09)
#define REG_EPMM2	(OFS_BANK1 | 0x0a)
#define	REG_EPMM3	(OFS_BANK1 | 0x0b)
#define	REG_EPMM4	(OFS_BANK1 | 0x0c)
#define	REG_EPMM5	(OFS_BANK1 | 0x0d)
#define	REG_EPMM6	(OFS_BANK1 | 0x0e)
#define	REG_EPMM7	(OFS_BANK1 | 0x0f)
#define REG_EPMCSL	(OFS_BANK1 | 0x10)
#define REG_EPMCSH	(OFS_BANK1 | 0x11)
#define REG_EPMOL	(OFS_BANK1 | 0x14)
#define REG_EPMOH	(OFS_BANK1 | 0x15)
#define REG_EWOLIE	(OFS_BANK1 | 0x16)
#define	EWOLIE_UCWOLIE	(1 << 7)
#define EWOLIE_AWOLIE	(1 << 6)
#define EWOLIE_PMWOLIE	(1 << 4)
#define EWOLIE_MPWOLIE	(1 << 3)
#define EWOLIE_HTWOLIE	(1 << 2)
#define EWOLIE_MCWOLIE	(1 << 1)
#define EWOLIE_BCWOLIE	(1 << 0)

#define REG_EWOLIR	(OFS_BANK1 | 0x17)
#define EWOLIR_UCWOLIF	(1 << 7)
#define EWOLIR_AWOLIF	(1 << 6)
#define EWOLIR_PMWOLIF	(1 << 4)
#define EWOLIR_MPWOLIF	(1 << 3)
#define EWOLIR_HTWOLIF	(1 << 2)
#define EWOLIR_MCWOLIF	(1 << 1)
#define EWOLIF_BCWOLIF	(1 << 0)
#define REG_ERXFCON	(OFS_BANK1 | 0x18)
#define ERXFCON_UCEN	(1 << 7)
#define ERXFCON_ANDOR	(1 << 6)
#define ERXFCON_CRCEN	(1 << 5)
#define ERXFCON_PMEN	(1 << 4)
#define ERXFCON_MPEN	(1 << 3)
#define ERXFCON_HTEN	(1 << 2)
#define ERXFCON_MCEN	(1 << 1)
#define ERXFCON_BCEN	(1 << 0)
#define REG_EPKTCNT	(OFS_BANK1 | 0x19)

#define REG_MACON1	(OFS_BANK2 | 0x00 | REG_WAIT)
#define MACON1_LOOPBK	(1 << 4)
#define MACON1_TXPAUS	(1 << 3)
#define MACON1_RXPAUS	(1 << 2)
#define MACON1_PASALL	(1 << 1)
#define MACON1_MAXREN	(1 << 0)

#define REG_MACON2	(OFS_BANK2 | 0x01 | REG_WAIT)
#define MACON2_MARST	(1 << 7)
#define MACON2_RNDRST	(1 << 6)
#define MACON2_MARXRST	(1 << 3)
#define MACON2_RFUNRST	(1 << 2)
#define MACON2_MATXRST	(1 << 1)
#define MACON2_TFUNRST	(1 << 0)

#define REG_MACON3	(OFS_BANK2 | 0x02 | REG_WAIT)
#define MACON3_PADCFG2	(1 << 7)
#define MACON3_PADCFG1	(1 << 6)
#define MACON3_PADCFG0	(1 << 5)
#define MACON3_TXCRCEN	(1 << 4)
#define MACON3_PHDRLEN	(1 << 3)
#define MACON3_HFRMEN	(1 << 2)
#define MACON3_FRMLNEN	(1 << 1)
#define MACON3_FULDPX	(1 << 0)

#define REG_MACON4	(OFS_BANK2 | 0x03 | REG_WAIT)
#define MACON4_DEFER	(1 << 6)
#define MACON4_BPEN	(1 << 5)
#define MACON4_NOBKOFF	(1 << 4)
#define MACON4_LONGPRE	(1 << 1)
#define	MACON4_PUREPRE	(1 << 0)

#define REG_MABBIPG	(OFS_BANK2 | 0x04 | REG_WAIT)
#define REG_MAIPGL	(OFS_BANK2 | 0x06 | REG_WAIT)
#define REG_MAIPGH	(OFS_BANK2 | 0x07 | REG_WAIT)
#define REG_MACLCON1	(OFS_BANK2 | 0x08 | REG_WAIT)
#define REG_MACLCON2	(OFS_BANK2 | 0x09 | REG_WAIT)
#define REG_MAMXFLL	(OFS_BANK2 | 0x0a | REG_WAIT)
#define REG_MAMXFLH	(OFS_BANK2 | 0x0b | REG_WAIT)

/* Only documented in old 39662a document */
#define REG_MAPHSUP	(OFS_BANK2 | 0x0d | REG_WAIT)
#define	MAPHSUP_RSTINTFC	(1 << 7)
#define MAPHSUP_RSTRMII		(1 << 6)

#define REG_MICON	(OFS_BANK2 | 0x11 | REG_WAIT)
#define MICON_RSTMII	(1 << 7)

#define REG_MICMD	(OFS_BANK2 | 0x12 | REG_WAIT)
#define MICMD_MIISCAN	(1 << 1)
#define MICMD_MIIRD	(1 << 0)

#define REG_MIREGADR	(OFS_BANK2 | 0x14 | REG_WAIT)
#define REG_MIWRL	(OFS_BANK2 | 0x16 | REG_WAIT)
#define REG_MIWRH	(OFS_BANK2 | 0x17 | REG_WAIT)
#define REG_MIRDL	(OFS_BANK2 | 0x18 | REG_WAIT)
#define REG_MIRDH	(OFS_BANK2 | 0x19 | REG_WAIT)
#define REG_MAADR5	(OFS_BANK3 | 0x00 | REG_WAIT)
#define REG_MAADR6	(OFS_BANK3 | 0x01 | REG_WAIT)
#define REG_MAADR3	(OFS_BANK3 | 0x02 | REG_WAIT)
#define REG_MAADR4	(OFS_BANK3 | 0x03 | REG_WAIT)
#define REG_MAADR1	(OFS_BANK3 | 0x04 | REG_WAIT)
#define REG_MAADR2	(OFS_BANK3 | 0x05 | REG_WAIT)
#define REG_EBSTSD	(OFS_BANK3 | 0x06)
#define REG_EBSTCON	(OFS_BANK3 | 0x07)
#define EBSTCON_PSV2	(1 << 7)
#define EBSTCON_PSV1	(1 << 6)
#define EBSTCON_PSV0	(1 << 5)
#define EBSTCON_PSEL	(1 << 4)
#define EBSTCON_TMSEL1	(1 << 3)
#define EBSTCON_TMSEL0	(1 << 2)
#define EBSTCON_TME	(1 << 1)
#define EBSTCON_BISTST	(1 << 0)

#define REG_EBSTCSL	(OFS_BANK3 | 0x08)
#define REG_EBSTCSH	(OFS_BANK3 | 0x09)
#define REG_MISTAT	(OFS_BANK3 | 0x0a | REG_WAIT)
#define MISTAT_NVALID	(1 << 2)
#define MISTAT_SCAN	(1 << 1)
#define MISTAT_BUSY	(1 << 0)

#define REG_EREVID	(OFS_BANK3 | 0x12)
#define REG_ECOCON	(OFS_BANK3 | 0x15)
#define		ECOCON_CONCON2	(1 << 2)
#define		ECOCON_CONCON1	(1 << 1)
#define		ECOCON_CONCON0	(1 << 0)
#define REG_EFLOCON	(OFS_BANK3 | 0x17)
#define		EFLOCON_FULDPXS	(1 << 2)
#define		EFLOCON_FCEN1	(1 << 1)
#define		EFLOCON_FCEN0	(1 << 0)

#define REG_EPAUSL	(OFS_BANK3 | 0x18)
#define REG_EPAUSH	(OFS_BANK3 | 0x19)

/**
 * Now the PHY registers
 */
#define PHYR_PHCON1	0x00
#define		PHCON1_PRST	(1 << 15)
#define		PHCON1_PLOOPBK	(1 << 14)
#define		PHCON1_PPWSRV	(1 << 11)
#define		PHCON1_PDPXMD	(1 << 8)

#define PHYR_PHSTAT1	0x01
#define		PHSTAT1_PFDPX	(1 << 12)
#define		PHSTAT1_PHDPX	(1 << 11)
#define		PHSTAT1_LLSTAT	(1 << 2)
#define		PHSTAT1_JBSTAT	(1 << 1)
#define PHYR_PHID1	0x02
#define PHYR_PHID2	0x03
#define PHYR_PHCON2	0x10
#define		PHCON2_FRCLNK	(1 << 14)
#define		PHCON2_TXDIS	(1 << 13)
#define		PHCON2_JABBER	(1 << 10)
#define 	PHCON2_HDLDIS	(1 << 8)
#define PHYR_PHSTAT2	0x11
#define		PHSTAT2_TXSTAT	(1 << 13)
#define		PHSTAT2_RXSTAT	(1 << 12)
#define		PHSTAT2_COLSTAT	(1 << 11)
#define		PHSTAT2_LSTAT	(1 << 10)
#define		PHSTAT2_DPXSTAT	(1 << 9)
#define		PHSTAT2_PLRITY	(1 << 4)
#define PHYR_PHIE	0x12
#define		PHIE_PLNKIE	(1 << 4)
#define		PHIE_PGEIE	(1 << 1)
#define PHYR_PHIR	0x13
#define		PHIR_PLNKIF	(1 << 4)
#define		PHIR_PGIF	(1 << 2)
#define PHYR_PHLCON	0x14
#define		PHLCON_STRCH	(1 << 0)

#define TSV_BIT_VLAN_FRM	(51)
#define TSV_BIT_BCKPRESSURE	(50)
#define TSV_BIT_TXPAUSE_CTRL	(49)
#define TSV_BIT_TXCTRL		(48)
#define TSV_BIT_TXUNDERRUN	(31)
#define TSV_BIT_TXGIANT		(30)
#define TSV_BIT_TXLATECOL	(29)
#define TSV_BIT_TXEXCOL		(28)
#define TSV_BIT_TXEXDEFER	(27)
#define TSV_BIT_TXDEFER		(26)
#define TSV_BIT_TXBROADCAST	(25)
#define TSV_BIT_TXMULTICAST	(24)
#define TSV_BIT_TXDONE		(23)
#define TSV_BIT_TXLOUTOFRANGE	(22)
#define TSV_BIT_TXLENCHKERR	(21)
#define TSV_BIT_TXCRCERR	(20)

#define OP_RCR	0x00
#define OP_RBM	0x3A
#define OP_WCR	0x40
#define OP_WBM	0x7A
#define OP_BFS	0x80
#define OP_BFC	0xA0
#define OP_SRC	0xFF

#define MOD_LO(x,lo) ((x) = (x & 0xff00) | (lo))
#define MOD_HI(x,hi) ((x) = (x & 0x00ff) | ((uint16_t)(hi) << 8))

#if 0
#define dbgprintf(...) fprintf(stderr,__VA_ARGS__)
#else
#define dbgprintf(...)
#endif

#define RXTXBUF_SIZE	(8192)

struct Enc28j60 {
	int state;
	uint32_t chksum;
	int ether_fd;
	PollHandle_t *input_fh;
	int rx_is_enabled;
	CycleTimer miCmdTimer;
	CycleTimer transmitTimer;
	CycleTimer dmaTimer;
	uint8_t rxtx_buf[RXTXBUF_SIZE];
	uint32_t rxdrop;
	uint32_t txdrop;
	uint8_t tsv_buf[7];
	uint8_t currOpcode;
	uint8_t currArg;
	uint8_t currBank;
	SigNode *sigMosi;	/* connected to MOSI */
	SigNode *sigSck;	/* connected to SCK */
	SigTrace *sckTrace;
	SigNode *sigCsN;	/* connected to SPI CS */
	SigTrace *CsNTrace;
	SigNode *sigMiso;	/* connected to SPI MISO */
	SigNode *sigIrq;
	int shiftin_cnt;
	int shiftout_cnt;
	uint8_t shift_in;
	uint8_t shift_out;

	uint16_t regERDPT;
	uint16_t regEWRPT;	/* Write buffer Pointer */
	uint16_t regETXST;
	uint16_t regETXND;
	uint16_t regERXST;
	uint16_t regERXND;

	/* The rx buffer read pointer */
	uint16_t regERXRDPTL;	/* Buffer for low byte of the ERXDPT */
	uint16_t regERXRDPT;

	/* The rx buffer write pointer has an internal copy */
	uint16_t regERXWRPTint;
	uint16_t regERXWRPT;

	uint16_t regEDMAST;
	uint16_t dmaSrc;
	uint16_t regEDMAND;
	uint16_t regEDMADST;
	uint16_t dmaDst;
	uint16_t regEDMACS;
	uint8_t regEIE;
	uint8_t regEIR;
	uint8_t regESTAT;
	uint8_t regECON2;
	uint8_t regECON1;
	uint8_t regEHT[8];
	uint8_t regEPMM[8];
	uint16_t regEPMCS;
	uint16_t regEPMO;
	uint8_t regEWOLIE;
	uint8_t regEWOLIR;
	uint8_t regERXFCON;
	uint8_t regEPKTCNT;
	uint8_t regMACON1;
	uint8_t regMACON2;
	uint8_t regMACON3;
	uint8_t regMACON4;
	uint8_t regMABBIPG;
	uint16_t regMAIPG;
	uint8_t regMACLCON1;
	uint8_t regMACLCON2;
	uint16_t regMAMXFL;
	uint8_t regMAPHSUP;
	uint8_t regMICON;
	uint8_t regMICMD;
	uint8_t regMIREGADR;
	uint16_t regMIWR;
	uint16_t regMIRD;
	uint8_t regMAADR[6];
	uint8_t regEBSTSD;
	uint8_t regEBSTCON;
	uint16_t regEBSTCS;
	uint8_t regMISTAT;
	uint8_t regEREVID;
	uint8_t regECOCON;
	uint8_t regEFLOCON;
	uint16_t regEPAUS;

	bool miiwrite;
	/* PHY registers */
	uint16_t phyrPHCON1;
	uint16_t phyrPHSTAT1;
	uint16_t phyrPHID1;
	uint16_t phyrPHID2;
	uint16_t phyrPHCON2;
	uint16_t phyrPHSTAT2;
	uint16_t phyrPHIE;
	uint16_t phyrPHIR;
	uint16_t phyrPHLCON;
};

#define STATE_CMD		(0)
#define STATE_DELAYED_RCR	(1)
#define STATE_FINISHED		(2)
#define STATE_RCR		(3)
#define STATE_WCR		(4)
#define STATE_WBM		(5)
#define STATE_RBM		(6)
#define STATE_BFS		(7)
#define STATE_BFC		(8)

/**
 **********************************************************
 * Fold the upper half of the checksum into the lower
 **********************************************************
 */
static void
update_edmacs(Enc28j60 * enc)
{
	uint32_t sum = enc->chksum;
	while (sum >> 16) {
		sum = (sum & 0xffff) + (sum >> 16);
	}
	sum = ~sum;
	enc->regEDMACS = sum;
}

/**
 **********************************************************
 * Fetch a cksum byte
 **********************************************************
 */
static bool
fetch_csum_byte(Enc28j60 * enc, uint8_t * data)
{
	enc->dmaSrc = enc->dmaSrc % RXTXBUF_SIZE;
	*data = enc->rxtx_buf[enc->dmaSrc];
	//fprintf(stderr,"Feched %02x from %04x\n",*data,enc->dmaSrc);
	if (enc->dmaSrc == enc->regEDMAND) {
		return false;
	}
	if (enc->dmaSrc == enc->regERXND) {
		enc->dmaSrc = enc->regERXST;
	} else {
		enc->dmaSrc = (enc->dmaSrc + 1) % RXTXBUF_SIZE;
	}
	return true;
}

static bool
dma_one_byte(Enc28j60 * enc, uint8_t * data)
{
	enc->dmaSrc = enc->dmaSrc % RXTXBUF_SIZE;
	enc->dmaDst = enc->dmaDst % RXTXBUF_SIZE;
	enc->rxtx_buf[enc->dmaDst] = enc->rxtx_buf[enc->dmaSrc];
	//fprintf(stderr,"Feched %02x from %04x\n",*data,enc->dmaSrc);
	if (enc->dmaSrc == enc->regEDMAND) {
		return false;
	}
	if (enc->dmaSrc == enc->regERXND) {
		enc->dmaSrc = enc->regERXST;
	} else {
		enc->dmaSrc = (enc->dmaSrc + 1) % RXTXBUF_SIZE;
	}
	enc->dmaDst = (enc->dmaDst + 1) % RXTXBUF_SIZE;
	return true;
}

static void
update_interrupt(Enc28j60 * enc)
{
	if (enc->regEIR & enc->regEIE) {
		SigNode_Set(enc->sigIrq, SIG_LOW);
	} else {
		SigNode_Set(enc->sigIrq, SIG_PULLUP);
		//SigNode_Set(enc->sigIrq,SIG_HIGH);
	}
}

/**
 ****************************************************************************
 * Do DMA
 ****************************************************************************
 */
static void
do_dma(void *evData)
{
	Enc28j60 *enc = evData;
	uint8_t data[2];
	uint16_t chkword;
	bool res;
	if (enc->regECON1 & ECON1_CSUMEN) {
		res = fetch_csum_byte(enc, data + 1);
		if (res == true) {
			res = fetch_csum_byte(enc, data + 0);
		} else {
			data[0] = 0;
		}
		chkword = data[0] | ((uint16_t) data[1] << 8);
		enc->chksum += chkword;
		if (res == true) {
			CycleTimer_Mod(&enc->dmaTimer, NanosecondsToCycles(45));
		} else {
			enc->regEIR |= EIR_DMAIF;
			update_interrupt(enc);
		}
	} else {
		res = dma_one_byte(enc, data + 1);
		if (res == true) {
			CycleTimer_Mod(&enc->dmaTimer, NanosecondsToCycles(45));
		} else {
			enc->regEIR |= EIR_DMAIF;
			update_interrupt(enc);
		}
	}
	return;
}

static bool
rcr_is_delayed(Enc28j60 * enc, uint8_t reg_no)
{
	switch (reg_no) {
	    case REG_MACON1:
	    case REG_MACON2:
	    case REG_MACON3:
	    case REG_MACON4:
	    case REG_MABBIPG:
	    case REG_MAIPGL:
	    case REG_MAIPGH:
	    case REG_MACLCON1:
	    case REG_MACLCON2:
	    case REG_MAMXFLL:
	    case REG_MAMXFLH:
	    case REG_MAPHSUP:
	    case REG_MICON:
	    case REG_MICMD:
	    case REG_MIREGADR:
	    case REG_MIWRL:
	    case REG_MIWRH:
	    case REG_MIRDL:
	    case REG_MIRDH:
	    case REG_MAADR1:
	    case REG_MAADR2:
	    case REG_MAADR3:
	    case REG_MAADR4:
	    case REG_MAADR5:
	    case REG_MAADR6:
	    case REG_MISTAT:
		    return true;
	    default:
		    return false;
	}
}

/**
 ****************************************************************
 * Checksum statemachine, returning the inverted TCP/IP checksum 
 * in host endian (if toggle starts with 0).
 ****************************************************************
 */
static uint16_t
chksum(uint8_t data, uint16_t startval, uint8_t * toggle)
{
	uint32_t sum = startval;
	if (*toggle & 1) {
		sum += data;
	} else {
		sum += (uint16_t) data << 8;
	}
	*toggle = !*toggle;
	while (sum >> 16) {
		sum = (sum & 0xffff) + (sum >> 16);
	}
	return sum;
}

/**
 **********************************************************************
 * Warning, The index is not verified against the real chip
 **********************************************************************
 */
static inline uint32_t
calculate_hash_index(uint8_t * pkt)
{
	uint32_t crc;
	uint32_t index;
	crc = EthernetCrc(0, pkt, 6);
	index = (~crc >> 26) & 0x3f;	/* The first 6 bits of the CRC inverted CRC */
	return index;
}

/**
 ****************************************************************************
 * \fn static void test_hash_calculator(void); 
 * Test the hash calculator for correctness.
 * Test values are from Crystal semiconductor AN194. I hope
 * the hash values are the same for ENC28J60
 ****************************************************************************
 */
static void
test_hash_calculator(void)
{
	unsigned int idx;
	int i;
	uint8_t mac[] = { 0, 0, 0, 0, 0, 0 };
	uint8_t testmacs[] = { 0x85, 0x87, 0x43, 0xed, 0x4D };
	uint8_t testresults[] = { 0, 19, 49, 57, 63 };
	for (i = 0; i < array_size(testmacs); i++) {
		mac[0] = testmacs[i];
		idx = calculate_hash_index(mac);
		if (idx != testresults[i]) {
			fprintf(stderr, "Hash index calculator selftest failed\n");
			exit(1);
		}
	}
}

static bool
match_hash(Enc28j60 * enc, uint8_t * pkt)
{
	int index = calculate_hash_index(pkt);
	int byte_nr = index >> 3;
	int bit_nr = index & 7;
	if ((enc->regEHT[byte_nr] >> bit_nr) & 1) {
		return true;
	} else {
		return false;
	}
}

/**
 ***********************************************************************
 * \fn static bool match_pattern(Enc28j60 *enc,uint8_t *pkt,int pktlen) 
 ***********************************************************************
 */
static bool
match_pattern(Enc28j60 * enc, uint8_t * pkt, int pktlen)
{
	uint8_t toggle = 0;
	uint16_t sum = 0;
	int i, j;
	if (pktlen < (64 + enc->regEPMO)) {
		return false;
	}
	for (i = 0; i < 8; i++) {
		for (j = 0; j < 8; j++) {
			if ((enc->regEPMM[i] >> j) & 1) {
				sum = chksum(pkt[enc->regEPMO + j + (i << 3)], sum, &toggle);
			}
		}
	}
	sum = ~sum;
	if (sum == enc->regEPMCS) {
		return true;
	} else {
		return false;
	}
}

/**
 ***********************************************
 * returns true if enc mac address matches
 ***********************************************
 */
static inline bool
match_unicast(Enc28j60 * enc, uint8_t * mac)
{
	return (memcmp(mac, enc->regMAADR, 6) == 0);
}

/**
 ***********************************************
 * returns true if enc mac address matches
 ***********************************************
 */
static inline bool
match_broadcast(Enc28j60 * enc, uint8_t * mac)
{
	uint8_t broadcast[] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };
	return (memcmp(mac, broadcast, 6) == 0);
}

static inline bool
match_multicast(Enc28j60 * enc, uint8_t * mac)
{
	return !!(mac[0] & 1);
}

/*
 ****************************************************************
 * Check if incoming packet can be accepted
 * Multicast hash table is missing
 ****************************************************************
 */
static bool
match_address(Enc28j60 * enc, uint8_t * pkt, uint16_t pktlen)
{
	bool andor = !!(enc->regERXFCON & ERXFCON_ANDOR);
	uint8_t erxfcon = enc->regERXFCON;
	if (andor) {
		if (erxfcon & ERXFCON_UCEN) {
			if (!match_unicast(enc, pkt)) {
				return false;
			}
		}
		if (erxfcon & ERXFCON_BCEN) {
			if (!match_broadcast(enc, pkt)) {
				return false;
			}
		}
		if (erxfcon & ERXFCON_PMEN) {
			if (!match_pattern(enc, pkt, pktlen)) {
				return false;
			}
		}
		if (erxfcon & ERXFCON_HTEN) {
			if (!match_hash(enc, pkt)) {
				return false;
			}
		}
		if (erxfcon & ERXFCON_MCEN) {
			if (!match_multicast(enc, pkt)) {
				return false;
			}
		}
		return true;
	} else {
		if (erxfcon & ERXFCON_UCEN) {
			if (match_unicast(enc, pkt)) {
				return true;
			}
		}
		if (erxfcon & ERXFCON_BCEN) {
			if (match_broadcast(enc, pkt)) {
				return true;
			}
		}
		if (erxfcon & ERXFCON_PMEN) {
			if (match_pattern(enc, pkt, pktlen)) {
				return true;
			}
		}
		if (erxfcon & ERXFCON_HTEN) {
			if (match_hash(enc, pkt)) {
				return true;
			}
		}
		if (erxfcon & ERXFCON_MCEN) {
			if (match_multicast(enc, pkt)) {
				return true;
			}
		}
		return false;
	}
}

/**
 ********************************************************************************
 * \fn static void write_rx_data(Enc28j60 *enc,uint8_t *data,uint16_t pktlen) 
 ********************************************************************************
 */
static void
write_rx_data(Enc28j60 * enc, uint8_t * data, uint16_t pktlen)
{
	int i;
	for (i = 0; i < pktlen; i++) {
		// increment after ?
		if (enc->regERXWRPTint >= RXTXBUF_SIZE) {
			fprintf(stderr, "ENC: Should not happen, buffer pointer out of window\n");
		} else {
			enc->rxtx_buf[enc->regERXWRPTint] = data[i];
		}
		if (enc->regERXWRPTint == enc->regERXND) {
			enc->regERXWRPTint = enc->regERXST;
		} else {
			enc->regERXWRPTint = (enc->regERXWRPTint + 1) % RXTXBUF_SIZE;
		}
		/* Real device checks after increment and not before write */
		if (enc->regERXWRPTint == enc->regERXRDPT) {
			fprintf(stderr, "ENC28J60 ERROR: reached ERXRDPT\n");
			enc->regEIR |= EIR_RXERIF;
			update_interrupt(enc);
			break;
		}
	}
}

/**
 **********************************************************************
 * \fn static void write_rx_header(Enc28j60 *enc,uint16_t pktlen) 
 * write the rx header
 **********************************************************************
 */
static void
write_rx_header(Enc28j60 * enc, uint16_t pktlen)
{
	uint8_t rxheader[6] = { 0, };
	uint16_t next_pkt;
	uint16_t rxbuflen;
	uint16_t hdrlen = 6;
	uint32_t bufcnt;
	rxbuflen = enc->regERXND - enc->regERXST + 1;
	if (pktlen & 1) {
		pktlen++;
	}
	bufcnt = enc->regERXWRPT - enc->regERXST + pktlen + hdrlen;
	next_pkt = enc->regERXST + (bufcnt % rxbuflen);
	rxheader[0] = next_pkt & 0xff;
	rxheader[1] = (next_pkt >> 8) & 0xff;
	rxheader[2] = pktlen & 0xff;
	rxheader[3] = (pktlen >> 8) & 0xff;
	rxheader[4] = 0xc0;
	rxheader[5] = 0;
	//fprintf(stderr,"Write packet to %d, next %d\n",enc->regERXWRPT,next_pkt);
	write_rx_data(enc, rxheader, 6);
}

/**
 ***********************************************************************
 * \fn static void rx_pkt_complete(Enc28j60 *enc); 
 ***********************************************************************
 */
static void
rx_pkt_complete(Enc28j60 * enc)
{
	uint8_t dummy = 0;
	if (enc->regERXWRPTint & 1) {
		write_rx_data(enc, &dummy, 1);
	}
	if (enc->regEPKTCNT < 255) {
		enc->regEPKTCNT++;
		enc->regERXWRPT = enc->regERXWRPTint;
		enc->regEIR |= EIR_PKTIF;
	} else {
		/* Forget the packet */
		enc->regERXWRPTint = enc->regERXWRPT;
		enc->regEIR |= EIR_RXERIF;
	}
	update_interrupt(enc);
}

/*
 *************************************************************
 *************************************************************
 */
static bool
check_rx_frame_length(Enc28j60 * enc, uint16_t frame_length)
{
	if (enc->regMACON3 & MACON3_HFRMEN) {
		return true;
	}
	if (enc->regMACON3 & MACON3_FRMLNEN) {
		if (frame_length > enc->regMAMXFL) {
			return false;
		}
	}
	return true;
}

/**
 ****************************************************************************************
 * \fn static void receive_pkt(Enc28j60 *enc, uint8_t *pktbuf, unsigned int pktlen) 
 * Write a packet from pktbuf to the internal memory of the ENC28J60 if the MAC address
 * matches. Also calculate the Ethernet CRC and append it to the reception buffer.  
 ****************************************************************************************
 */
static void
receive_pkt(Enc28j60 *enc, uint8_t *pktbuf, unsigned int pktlen) 
{
	uint16_t frame_length;
	uint32_t crc;
	uint8_t *crcbuf;
    frame_length = pktlen + 4;
    if (enc->rxdrop && ((rand() % 100) < enc->rxdrop)) {
        fprintf(stdout, "Drop RX packet\n");
        return;
    }
    if (frame_length > 1522) {
        /* Discard jumbo frames frames for now */
        return;
    }
    if (check_rx_frame_length(enc, frame_length) == false) {
        return;
    }
    crcbuf = pktbuf + pktlen;
    crc = EthernetCrc(0, pktbuf, pktlen);
    crcbuf[0] = crc;
    crcbuf[1] = crc >> 8;
    crcbuf[2] = (crc >> 16);
    crcbuf[3] = crc >> 24;
    //fprintf(stderr,"Received a paket\n");
    if (!match_address(enc, pktbuf, pktlen)) {
        return;
    }
    write_rx_header(enc, frame_length);
    write_rx_data(enc, pktbuf, frame_length);
    rx_pkt_complete(enc);
}

/**
 *******************************************************************************************
 * \fn static void rx_event(void *cd, int mask)
 * RX event handler reads the paket from the Ethernet backend and puts it into
 * the ENC28J60 receiver.
 *******************************************************************************************
 */
static void
rx_event(PollHandle_t *handle, int status, int events, void *clientdata)
{
	uint8_t buf[2048];
	Enc28j60 *enc = clientdata;
	int result;
	do {
		result = read(enc->ether_fd, buf, 2048);
		if (result <= 0) {
			continue;
		}
        receive_pkt(enc, buf, result);
	} while (result > 0);
	return;
}

/**
 ************************************************************
 * \fn static void enable_rx(Enc28j60 *enc) 
 * Enable the receiver by installing the RX callback.
 ************************************************************
 */
static void
enable_rx(Enc28j60 * enc)
{
	if (enc->rx_is_enabled) {
		return;
	}
	if (enc->ether_fd < 0) {
		dbgprintf("ENC28J60: ether fd < 0\n");
		return;
	}
	dbgprintf("ENC28J60: enable receiver\n");
	AsyncManager_PollStart(enc->input_fh, ASYNCMANAGER_EVENT_READABLE, &rx_event, enc);
	enc->rx_is_enabled = 1;
}

/**
 ********************************************************************
 * \fn static void disable_rx(Enc28j60 *enc)
 * Disable the Receiver by removing the RX callback handler.
 ********************************************************************
 */
static void
disable_rx(Enc28j60 * enc)
{
	if (enc->rx_is_enabled) {
		dbgprintf("ENC28J60: disable receiver\n");
		AsyncManager_PollStop(enc->input_fh);
		enc->rx_is_enabled = 0;
	}
}

static void
mod_tsv_bit(Enc28j60 * enc, unsigned int bit_nr, bool value)
{
	int by;
	int bit;
	if (bit_nr >= (8 * sizeof(enc->tsv_buf))) {
		fprintf(stderr, "ENC28J60 Bug, illegal TSV bit number\n");
		return;
	}
	by = bit_nr >> 3;
	bit = bit_nr & 7;
	if (value) {
		enc->tsv_buf[by] |= (1 << bit);
	} else {
		enc->tsv_buf[by] &= ~(1 << bit);
	}
}

#define CTRL_POVERIDE	(1 << 0)
#define CTRL_PCRCEN	(1 << 1)
#define CTRL_PPADEN	(1 << 2)
#define CTRL_PHUGEN	(1 << 3)

static bool
get_tx_crcen(Enc28j60 * enc, uint8_t ctrl)
{
	if (ctrl & CTRL_POVERIDE) {
		return !!(ctrl & CTRL_PCRCEN);
	} else {
		return !!(enc->regMACON3 & MACON3_TXCRCEN);
	}
}

static bool
get_tx_hugen(Enc28j60 * enc, uint8_t ctrl)
{
	if (ctrl & CTRL_POVERIDE) {
		return !!(ctrl & CTRL_PHUGEN);
	} else {
		return !!(enc->regMACON3 & MACON3_HFRMEN);
	}
}

static bool
check_tx_crc(Enc28j60 * enc, uint8_t * pktbuf, uint16_t frame_length)
{
	uint32_t crc;
	uint8_t *crcbuf;
	if (frame_length >= 4) {
		crcbuf = pktbuf + frame_length - 4;
		crc = EthernetCrc(0, pktbuf, 6);
		if ((crcbuf[0] == (crc & 0xff)) && (crcbuf[1] = (crc >> 8) & 0xff) &&
		    (crcbuf[2] == ((crc >> 16) & 0xff)) && (crcbuf[3] == ((crc >> 24) & 0xff))) {
			return true;
		} else {
			fprintf(stderr, "ENC28J60: tx crc error\n");
			return false;
		}
	} else {
		return false;
	}
}

#define PADCFG_NOPAD1		(0)
#define PADCFG_PAD60_CRC	(1)
#define PADCFG_NOPAD2		(2)
#define PADCFG_PAD64_CRC	(3)
#define PADCFG_NOPAD3		(4)
#define PADCFG_PADVLAN		(5)
#define PADCFG_NOPAD4		(6)
#define PADCFG_PAD64_CRC2	(7)

/**
 ***************************************************************************************
 * \fn static bool check_frame_length(Enc28j60 *enc,uint8_t ctrl,uint16_t frame_len) 
 * check if the framesize is legal.
 ***************************************************************************************
 */
static bool
check_tx_frame_length(Enc28j60 * enc, uint8_t ctrl, uint16_t frame_len)
{
	if (get_tx_hugen(enc, ctrl) == true) {
		return true;
	} else if (!(enc->regMACON3 & MACON3_FRMLNEN)) {
		return true;
	} else if (frame_len <= enc->regMAMXFL) {
		return true;
	} else {
		return false;
	}
}

/**
 * get the padconfig
 */
static void
get_padcfg_crc(Enc28j60 * enc, uint8_t * pkt, uint8_t ctrl, uint16_t * min_frame_len,
	       bool * add_crc)
{
	uint16_t typelen;
	bool crcen = get_tx_crcen(enc, ctrl);
	bool vlan;
	uint8_t padcfg = enc->regMACON3 >> 5;
	if ((pkt[12] == 0x81) && (pkt[13] == 0)) {
		vlan = true;
		mod_tsv_bit(enc, TSV_BIT_VLAN_FRM, true);
		typelen = ((uint16_t) pkt[16] << 8) | pkt[17];
	} else {
		typelen = ((uint16_t) pkt[12] << 8) | pkt[13];
		vlan = false;
	}
	if (typelen > 1500) {
		mod_tsv_bit(enc, TSV_BIT_TXLOUTOFRANGE, true);
	}
	switch (padcfg) {
	    case PADCFG_NOPAD1:
	    case PADCFG_NOPAD2:
	    case PADCFG_NOPAD3:
	    case PADCFG_NOPAD4:
		    *add_crc = crcen;
		    *min_frame_len = 0;
		    break;
	    case PADCFG_PAD60_CRC:
		    if (!crcen) {
			    fprintf(stderr, "ENC28J60: CRCEN mismatches PADCFG\n");
		    }
		    *add_crc = crcen;
		    *min_frame_len = 64;
		    break;

	    case PADCFG_PAD64_CRC:
	    case PADCFG_PAD64_CRC2:
		    if (!crcen) {
			    fprintf(stderr, "ENC28J60: CRCEN mismatches PADCFG\n");
		    }
		    *add_crc = crcen;
		    *min_frame_len = 68;
		    break;

	    case PADCFG_PADVLAN:
		    if ((pkt[12] == 0x81) && (pkt[13] == 0)) {
			    *min_frame_len = 68;
		    } else {
			    *min_frame_len = 64;
		    }
		    *add_crc = crcen;
		    break;
	    default:
		    /* Unreachable but the compiler doesn't know */
		    *add_crc = 0;
		    *min_frame_len = 0;
		    break;
	}
}

/**
 **************************************************************************************
 * \fn static void do_transmit(void *eventData)
 * Transmit a packet from the internal RAM of the ENC28J60.  If the PHY is
 * in half duplex mode then feed the packet back to the receiver (Self reception).
 **************************************************************************************
 */
static void
do_transmit(void *eventData)
{
	Enc28j60 *enc = eventData;
	uint8_t *pktbuf = alloca(RXTXBUF_SIZE);
	uint16_t start = enc->regETXST & (RXTXBUF_SIZE - 1);
	uint16_t last = enc->regETXND & (RXTXBUF_SIZE - 1);
	uint16_t i;
	bool add_crc;
	uint16_t min_frame_len;
	int result;
	uint32_t pktlen = 0;
	uint32_t frame_length;	/* pktlen + crc */
	uint8_t ctrl;
	if (start == last) {
		return;
	}
	i = start;
	ctrl = enc->rxtx_buf[i];
	i++, i = i & (RXTXBUF_SIZE - 1);

	for (; (pktlen < 65536) && (i != last); i++, i = i & (RXTXBUF_SIZE - 1)) {

		pktbuf[pktlen] = enc->rxtx_buf[i];
		pktlen++;
	}
	/* Is transmitten including "last" */
	pktbuf[pktlen] = enc->rxtx_buf[i];
	pktlen++;
	get_padcfg_crc(enc, pktbuf, ctrl, &min_frame_len, &add_crc);
	if (min_frame_len > (pktlen + 4)) {
		for (i = pktlen; i < min_frame_len - 4; i++) {
			pktbuf[i] = 0;
		}
		pktlen = min_frame_len - 4;
		frame_length = min_frame_len;
	} else {
		if (add_crc) {
			frame_length = pktlen + 4;
		} else if (pktlen >= 4) {
			frame_length = pktlen;
			pktlen -= 4;
			if (check_tx_crc(enc, pktbuf, frame_length) == false) {
				mod_tsv_bit(enc, TSV_BIT_TXCRCERR, true);
			}
		} else {
			fprintf(stderr, "ENC28J60: PKT shorter 4 bytes can not contain a CRC\n");
			return;
		}
	}
	if (match_broadcast(enc, pktbuf)) {
		mod_tsv_bit(enc, TSV_BIT_TXBROADCAST, true);
	}
	/* broadcast ist also a multicast ???? */
	if (match_multicast(enc, pktbuf)) {
		mod_tsv_bit(enc, TSV_BIT_TXMULTICAST, true);
	}
	if (check_tx_frame_length(enc, ctrl, frame_length) == false) {
		fprintf(stderr, "ENC28J60: TX frame to big %d\n", pktlen);
		mod_tsv_bit(enc, TSV_BIT_TXLENCHKERR, true);
	} else {
		//fprintf(stderr,"Start tx size %d\n",pktlen);
		if (enc->ether_fd > 0) {
			if (enc->txdrop && ((rand() % 100) < enc->txdrop)) {
				fprintf(stdout, "Drop TX packet\n");
			} else {
                if (!(enc->phyrPHCON1 & PHCON1_PLOOPBK) && !(enc->phyrPHCON2 & PHCON2_TXDIS)) {
                    fcntl(enc->ether_fd, F_SETFL, 0);
                    result = write(enc->ether_fd, pktbuf, pktlen);
                    fcntl(enc->ether_fd, F_SETFL, O_NONBLOCK);
                }
			}
		}
        if (!(enc->phyrPHCON1 & PHCON1_PDPXMD)) {	
            if (!(enc->phyrPHCON2 & PHCON2_HDLDIS) || (enc->phyrPHCON1 & PHCON1_PLOOPBK)) {
                /* self receive when phy Half duplex is enabled and HDLDIS is not set */
                receive_pkt(enc, pktbuf, pktlen);
            }
            if (enc->regMACON3 & MACON3_FULDPX)	{
                fprintf(stderr, 
                    "ENC28J60 driver Bug: MAC and PHY disagree in FULLDUPLEX settings\n");
            }
        } else {
            if ((enc->phyrPHCON1 & PHCON1_PLOOPBK)) {
                  receive_pkt(enc, pktbuf, pktlen);
            }
            if (!(enc->regMACON3 & MACON3_FULDPX))	{
                fprintf(stderr, 
                    "ENC28J60 driver Bug: MAC and PHY disagree in FULLDUPLEX settings\n");
            }
        }
	}
	/* Little endian */
	enc->tsv_buf[0] = frame_length & 0xff;
	enc->tsv_buf[1] = (frame_length >> 8) & 0xff;
	/* This should be checked with a real device */
	enc->tsv_buf[4] = (frame_length + 10) & 0xff;
	enc->tsv_buf[4] = ((frame_length + 10) >> 8) & 0xff;
	/*
	 * write Transmit Status Vector 
	 */
	for (i = 0; i < 7; i++) {
		uint16_t addr = (enc->regETXND + i) & (RXTXBUF_SIZE - 1);
		enc->rxtx_buf[addr] = enc->tsv_buf[i];
	}
	mod_tsv_bit(enc, TSV_BIT_TXDONE, true);
	enc->regECON1 &= ~ECON1_TXRTS;;
	enc->regEIR |= EIR_TXIF;
	update_interrupt(enc);
}

/**
 *****************************************************
 * \fn static void start_tx(Enc28j60 *enc); 
 * Start the transmission. Delayed by the time
 * required to transmit it on a 10MBit ethernet
 * with no collisions.
 *****************************************************
 */
static void
start_tx(Enc28j60 * enc)
{
	uint16_t start = enc->regETXST & (RXTXBUF_SIZE - 1);
	uint16_t last = enc->regETXND & (RXTXBUF_SIZE - 1);
	uint16_t size = last - start + 12;
	uint32_t nanoseconds = size * 8 * 100;
	memset(enc->tsv_buf, 0, sizeof(enc->tsv_buf));
	CycleTimer_Mod(&enc->transmitTimer, NanosecondsToCycles(nanoseconds));
}

static uint16_t
Phy_Read(Enc28j60 * enc, uint8_t reg_nr)
{
	//fprintf(stderr,"PHYREAD\n");
    uint16_t value;
	switch (reg_nr) {
	    case PHYR_PHCON1:
		    value = enc->phyrPHCON1;
            break;

	    case PHYR_PHSTAT1:
		    value = enc->phyrPHSTAT1;
            break;

	    case PHYR_PHID1:
		    value = enc->phyrPHID1;
            break;

	    case PHYR_PHID2:
		    value = enc->phyrPHID2;
            break;

	    case PHYR_PHCON2:
		    value = enc->phyrPHCON2;
            break;

	    case PHYR_PHSTAT2:
		    value = enc->phyrPHSTAT2;
            break;

	    case PHYR_PHIE:
		    value = enc->phyrPHIE;
            break;

	    case PHYR_PHIR:
		    value = enc->phyrPHIR;
            break;

	    case PHYR_PHLCON:
		    value = enc->phyrPHLCON;
            break;

	    default:
            value = 0;
		    break;
	}
	return value;
}

static void
Phy_Write(Enc28j60 * enc, uint16_t value, uint8_t reg_nr)
{
	switch (reg_nr) {
	    case PHYR_PHCON1:
            /* Duplex bit decides about self reception */
		    enc->phyrPHCON1 = value;
            break;

	    case PHYR_PHSTAT1:
		    enc->phyrPHSTAT1 = value;
            break;

	    case PHYR_PHID1:
		    enc->phyrPHID1 = value;
            break;

	    case PHYR_PHID2:
		    enc->phyrPHID2 = value;
            break;
    
	    case PHYR_PHCON2:
		    enc->phyrPHCON2 = value;
            break;

	    case PHYR_PHSTAT2:
		    enc->phyrPHSTAT2 = value;
            break;

	    case PHYR_PHIE:
		    enc->phyrPHIE = value;
            break;

	    case PHYR_PHIR:
		    enc->phyrPHIR = value;
            break;

	    case PHYR_PHLCON:
		    enc->phyrPHLCON = value;
            break;

	    default:
		    break;
	}
	fprintf(stderr, "PHYWRITE 0x%02x: 0x%04x\n", reg_nr, value);
}

static void
econ1_write(Enc28j60 * enc, uint8_t value)
{
	//fprintf(stderr,"ECON1 write %02x\n",value);
	enc->currBank = value & 3;
	enc->regECON1 = value & ~ECON1_TXRST;;
	if ((value & (ECON1_RXRST | ECON1_RXEN)) == ECON1_RXEN) {
		enable_rx(enc);
	} else {
		disable_rx(enc);
	}
	if ((value & (ECON1_TXRTS | ECON1_TXRST)) == ECON1_TXRTS) {
		start_tx(enc);
	}
	if (value & ECON1_DMAST) {
		if (value & ECON1_CSUMEN) {
			if (!CycleTimer_IsActive(&enc->dmaTimer)) {
				enc->chksum = 0;
				enc->dmaSrc = enc->regEDMAST;
				CycleTimer_Mod(&enc->dmaTimer, NanosecondsToCycles(45));
			}
		} else {
			if (!CycleTimer_IsActive(&enc->dmaTimer)) {
				enc->dmaSrc = enc->regEDMAST;
				enc->dmaDst = enc->regEDMADST;
				CycleTimer_Mod(&enc->dmaTimer, NanosecondsToCycles(45));
			}
		}
	} else {
		if (CycleTimer_IsActive(&enc->dmaTimer)) {
			CycleTimer_Remove(&enc->dmaTimer);
			enc->regEIR |= EIR_DMAIF;
			update_interrupt(enc);
		}
	}
}

static void
econ2_write(Enc28j60 * enc, uint8_t value)
{
	enc->regECON2 = value;
	if (value & ECON2_PKTDEC) {
		//fprintf(stderr,"PKTDEC\n");
		if (enc->regEPKTCNT) {
			enc->regEPKTCNT--;
			if (enc->regEPKTCNT == 0) {
				enc->regEIR &= ~EIR_PKTIF;
				update_interrupt(enc);
			}
		}
	}
}

/**
 ***************************************************************
 * \fn static void micmd_timer_proc(void *eventData) 
 ***************************************************************
 */
static void
micmd_timer_proc(void *eventData)
{
	Enc28j60 *enc = eventData;
	uint16_t value;
	if (enc->regMICMD & MICMD_MIIRD) {
		value = Phy_Read(enc, enc->regMIREGADR);
		enc->regMIRD = value;
		enc->regMISTAT &= ~MISTAT_NVALID;
		enc->regMISTAT &= ~MISTAT_BUSY;
	} else if (enc->regMICMD & MICMD_MIISCAN) {
		value = Phy_Read(enc, enc->regMIREGADR);
		enc->regMIRD = value;
		enc->regMISTAT &= ~MISTAT_NVALID;
	} else if (enc->miiwrite) {
		enc->miiwrite = false;
		Phy_Write(enc, enc->regMIWR, enc->regMIREGADR);
		enc->regMISTAT &= ~MISTAT_BUSY;
	} else {
		fprintf(stderr, "ENC28J60: Unexpected completion of micmd\n");
		enc->regMISTAT &= ~MISTAT_NVALID;
		enc->regMISTAT &= ~MISTAT_BUSY;
	}
}

static void
micmd_write(Enc28j60 * enc, uint8_t value)
{
	enc->regMICMD = value & (MICMD_MIISCAN | MICMD_MIIRD);
	if (value & MICMD_MIIRD) {
		if (CycleTimer_IsActive(&enc->miCmdTimer)) {
			fprintf(stderr, "ENC28J60 MICMD Timer is busy\n");
		} else {
			enc->regMISTAT |= MISTAT_NVALID;
			enc->regMISTAT |= MISTAT_BUSY;
			CycleTimer_Mod(&enc->miCmdTimer, NanosecondsToCycles(10240));
		}
	} else if (value & MICMD_MIISCAN) {
		if (CycleTimer_IsActive(&enc->miCmdTimer)) {
			fprintf(stderr, "ENC28J60 MICMD Timer is busy\n");
		} else {
			enc->regMISTAT |= MISTAT_NVALID;
			CycleTimer_Mod(&enc->miCmdTimer, NanosecondsToCycles(10240));
		}
	}
}

/**
 *************************************************************************
 * \fn static bool opc_wcr(Enc28j60 *enc,uint8_t reg_no,uint8_t data); 
 * Write to control register.
 *************************************************************************
 */
static bool
opc_wcr(Enc28j60 * enc, uint8_t reg_no, uint8_t data)
{
	//fprintf(stderr,"Write %02x val %02x\n",reg_no,data);
	switch (reg_no) {
	    case REG_ERDPTL:
		    MOD_LO(enc->regERDPT, data);
		    break;
	    case REG_ERDPTH:
		    MOD_HI(enc->regERDPT, data);
		    enc->regERDPT &= (RXTXBUF_SIZE - 1);
		    break;

		    /* Write buffer Pointer */
	    case REG_EWRPTL:
		    MOD_LO(enc->regEWRPT, data);
		    break;

	    case REG_EWRPTH:
		    MOD_HI(enc->regEWRPT, data);
		    enc->regEWRPT &= (RXTXBUF_SIZE - 1);
		    break;

	    case REG_ETXSTL:
		    MOD_LO(enc->regETXST, data);
		    break;

	    case REG_ETXSTH:
		    MOD_HI(enc->regETXST, data);
		    enc->regETXST &= (RXTXBUF_SIZE - 1);
		    break;

	    case REG_ETXNDL:
		    MOD_LO(enc->regETXND, data);
		    break;

	    case REG_ETXNDH:
		    MOD_HI(enc->regETXND, data);
		    enc->regETXND &= (RXTXBUF_SIZE - 1);
		    break;

		    /* Start of RX buffer configuration register */
	    case REG_ERXSTL:
		    MOD_LO(enc->regERXST, data);
		    enc->regERXWRPTint = enc->regERXST;;
		    enc->regERXWRPT = enc->regERXWRPTint;
		    break;

	    case REG_ERXSTH:
		    MOD_HI(enc->regERXST, data);
		    enc->regERXST %= RXTXBUF_SIZE;
		    enc->regERXWRPTint = enc->regERXST;;
		    enc->regERXWRPT = enc->regERXWRPTint;
		    break;

		    /* End of RX buffer configuration register */

	    case REG_ERXNDL:
		    MOD_LO(enc->regERXND, data);
		    /* 
		     ********************************************************
		     * The errata sheet says this does work only sometimes,
		     *  in some cases it wil write a 0 
		     ********************************************************
		     */
		    enc->regERXWRPTint = enc->regERXST;;
		    enc->regERXWRPT = enc->regERXWRPTint;
		    break;

	    case REG_ERXNDH:
		    MOD_HI(enc->regERXND, data);
		    enc->regERXND %= RXTXBUF_SIZE;
		    /* 
		     ********************************************************
		     * The errata sheet says this does work only sometimes,
		     *  in some cases it wil write a 0 
		     ********************************************************
		     */
		    enc->regERXWRPTint = enc->regERXST;;
		    enc->regERXWRPT = enc->regERXWRPTint;
		    break;
		    /* Rx buffer read pointer */
	    case REG_ERXRDPTL:
		    enc->regERXRDPTL = data;	/* buffer the low byte */
		    break;

	    case REG_ERXRDPTH:
		    enc->regERXRDPT = (((uint16_t) data << 8) | enc->regERXRDPTL) % RXTXBUF_SIZE;
		    break;

		    /* 
		     * Rx buffer write pointer 
		     * Not shure if it effects the internal copy or  both;
		     */
	    case REG_ERXWRPTL:
		    MOD_LO(enc->regERXWRPT, data);
		    break;

	    case REG_ERXWRPTH:
		    MOD_HI(enc->regERXWRPT, data);
		    enc->regERXWRPT %= RXTXBUF_SIZE;
		    enc->regERXWRPTint = enc->regERXWRPT;
		    break;

	    case REG_EDMASTL:
		    MOD_LO(enc->regEDMAST, data);
		    break;

	    case REG_EDMASTH:
		    MOD_HI(enc->regEDMAST, data);
		    enc->regEDMAST %= RXTXBUF_SIZE;
		    break;

	    case REG_EDMANDL:
		    MOD_LO(enc->regEDMAND, data);
		    break;

	    case REG_EDMANDH:
		    MOD_HI(enc->regEDMAND, data);
		    enc->regEDMAND %= RXTXBUF_SIZE;
		    break;

	    case REG_EDMADSTL:
		    MOD_LO(enc->regEDMADST, data);
		    break;

	    case REG_EDMADSTH:
		    MOD_HI(enc->regEDMADST, data);
		    enc->regEDMADST %= RXTXBUF_SIZE;
		    break;

	    case REG_EDMACSL:
		    break;

	    case REG_EDMACSH:
		    break;

	    case (OFS_BANK0 | REG_EIE):
	    case (OFS_BANK1 | REG_EIE):
	    case (OFS_BANK2 | REG_EIE):
	    case (OFS_BANK3 | REG_EIE):
		    enc->regEIE = data;
		    update_interrupt(enc);
		    break;

	    case (OFS_BANK0 | REG_EIR):
	    case (OFS_BANK1 | REG_EIR):
	    case (OFS_BANK2 | REG_EIR):
	    case (OFS_BANK3 | REG_EIR):
			/** Only some bits are writable */
		    enc->regEIR = enc->regEIR & (data | 0x54);
		    update_interrupt(enc);
		    break;

	    case (OFS_BANK0 | REG_ESTAT):
	    case (OFS_BANK1 | REG_ESTAT):
	    case (OFS_BANK2 | REG_ESTAT):
	    case (OFS_BANK3 | REG_ESTAT):
		    break;

	    case (REG_ECON2 | OFS_BANK0):
	    case (REG_ECON2 | OFS_BANK1):
	    case (REG_ECON2 | OFS_BANK2):
	    case (REG_ECON2 | OFS_BANK3):
		    econ2_write(enc, data);
		    break;

	    case (REG_ECON1 | OFS_BANK0):
	    case (REG_ECON1 | OFS_BANK1):
	    case (REG_ECON1 | OFS_BANK2):
	    case (REG_ECON1 | OFS_BANK3):
		    econ1_write(enc, data);
		    break;

	    case REG_EHT0:
		    enc->regEHT[0] = data;
		    break;

	    case REG_EHT1:
		    enc->regEHT[1] = data;
		    break;

	    case REG_EHT2:
		    enc->regEHT[2] = data;
		    break;

	    case REG_EHT3:
		    enc->regEHT[3] = data;
		    break;

	    case REG_EHT4:
		    enc->regEHT[4] = data;
		    break;

	    case REG_EHT5:
		    enc->regEHT[5] = data;
		    break;

	    case REG_EHT6:
		    enc->regEHT[6] = data;
		    break;

	    case REG_EHT7:
		    enc->regEHT[7] = data;
		    break;

	    case REG_EPMM0:
		    enc->regEPMM[0] = data;
		    break;

	    case REG_EPMM1:
		    enc->regEPMM[1] = data;
		    break;

	    case REG_EPMM2:
		    enc->regEPMM[2] = data;
		    break;

	    case REG_EPMM3:
		    enc->regEPMM[3] = data;
		    break;

	    case REG_EPMM4:
		    enc->regEPMM[4] = data;
		    break;

	    case REG_EPMM5:
		    enc->regEPMM[5] = data;
		    break;

	    case REG_EPMM6:
		    enc->regEPMM[6] = data;
		    break;

	    case REG_EPMM7:
		    enc->regEPMM[7] = data;
		    break;

	    case REG_EPMCSL:
		    MOD_LO(enc->regEPMCS, data);
		    break;

	    case REG_EPMCSH:
		    MOD_HI(enc->regEPMCS, data);
		    break;

	    case REG_EPMOL:
		    MOD_LO(enc->regEPMO, data);
		    break;

	    case REG_EPMOH:
		    MOD_HI(enc->regEPMO, data);
		    enc->regEPMO &= 0x1fff;
		    break;

	    case REG_EWOLIE:
		    break;

	    case REG_EWOLIR:
		    break;

	    case REG_ERXFCON:
		    enc->regERXFCON = data;
		    break;

	    case REG_EPKTCNT:
		    /* Not writable */
		    break;

	    case REG_MACON1:
		    enc->regMACON1 = data;
		    break;

	    case REG_MACON2:
		    enc->regMACON2 = data;
		    break;

	    case REG_MACON3:
		    enc->regMACON3 = data;
		    break;

	    case REG_MACON4:
		    enc->regMACON4 = data;
		    break;

	    case REG_MABBIPG:
		    enc->regMABBIPG = data;
		    break;

	    case REG_MAIPGL:
		    MOD_LO(enc->regMAIPG, data);
		    break;

	    case REG_MAIPGH:
		    MOD_HI(enc->regMAIPG, data);
		    break;

	    case REG_MACLCON1:
		    enc->regMACLCON1 = data;
		    break;

	    case REG_MACLCON2:
		    enc->regMACLCON2 = data;
		    break;

	    case REG_MAMXFLL:
		    MOD_LO(enc->regMAMXFL, data);
		    break;

	    case REG_MAMXFLH:
		    MOD_HI(enc->regMAMXFL, data);
		    break;

	    case REG_MAPHSUP:
		    /* Only in old 39662a.pdf, not recommended to use */
		    break;

	    case REG_MICON:
		    /* Only in old 39662a.pdf, not recommended to use */
		    break;

	    case REG_MICMD:
		    micmd_write(enc, data);
		    break;

	    case REG_MIREGADR:
		    enc->regMIREGADR = data;
		    break;

	    case REG_MIWRL:
		    MOD_LO(enc->regMIWR, data);
		    break;

	    case REG_MIWRH:
		    MOD_HI(enc->regMIWR, data);
		    if (CycleTimer_IsActive(&enc->miCmdTimer)) {
			    fprintf(stderr, "ENC28J60 MICMD Timer is busy\n");
		    } else {
			    enc->miiwrite = true;
			    CycleTimer_Mod(&enc->miCmdTimer, NanosecondsToCycles(10240));
		    }
		    break;

	    case REG_MIRDL:
		    break;

	    case REG_MIRDH:
		    break;

	    case REG_MAADR2:
		    enc->regMAADR[1] = data;
		    break;

	    case REG_MAADR1:
		    enc->regMAADR[0] = data;
		    break;

	    case REG_MAADR4:
		    enc->regMAADR[3] = data;
		    break;

	    case REG_MAADR3:
		    enc->regMAADR[2] = data;
		    break;

	    case REG_MAADR6:
		    enc->regMAADR[5] = data;
		    break;

	    case REG_MAADR5:
		    enc->regMAADR[4] = data;
		    break;

	    case REG_EBSTSD:
		    break;

	    case REG_EBSTCON:
		    break;

	    case REG_EBSTCSL:
		    break;

	    case REG_EBSTCSH:
		    break;

	    case REG_MISTAT:
		    break;

	    case REG_EREVID:
		    /* This is a readonly register */
		    break;

	    case REG_ECOCON:
		    break;

	    case REG_EFLOCON:
		    break;

	    case REG_EPAUSL:
		    break;

	    case REG_EPAUSH:
		    break;

	    default:
		    return false;
	}
	return true;
}

/**
 *********************************************************************
 * regno is a composite of bank addr
 *********************************************************************
 */
static uint8_t
opc_rcr(Enc28j60 * enc, uint8_t reg_no)
{
	uint8_t value = 0;
	switch (reg_no) {
	    case REG_ERDPTL:
		    value = enc->regERDPT & 0xff;
		    break;

	    case REG_ERDPTH:
		    value = (enc->regERDPT >> 8) & 0xff;
		    break;

	    case REG_EWRPTL:
		    value = enc->regEWRPT & 0xff;
		    break;

	    case REG_EWRPTH:
		    value = (enc->regEWRPT >> 8) & 0xff;
		    break;

	    case REG_ETXSTL:
		    value = (enc->regETXST) & 0xff;
		    break;

	    case REG_ETXSTH:
		    value = (enc->regETXST >> 8) & 0xff;
		    break;

	    case REG_ETXNDL:
		    value = enc->regETXND & 0xff;
		    break;

	    case REG_ETXNDH:
		    value = (enc->regETXND >> 8) & 0xff;
		    break;

	    case REG_ERXSTL:
		    value = enc->regERXST & 0xff;
		    break;

	    case REG_ERXSTH:
		    value = (enc->regERXST >> 8) & 0xff;
		    break;

	    case REG_ERXNDL:
		    value = enc->regERXND & 0xff;
		    break;

	    case REG_ERXNDH:
		    value = (enc->regERXND >> 8) & 0xff;
		    break;

	    case REG_ERXRDPTL:
		    value = enc->regERXRDPT & 0xff;
		    break;

	    case REG_ERXRDPTH:
		    value = (enc->regERXRDPT >> 8) & 0xff;
		    break;

	    case REG_ERXWRPTL:
		    value = enc->regERXWRPT & 0xff;
		    break;

	    case REG_ERXWRPTH:
		    value = (enc->regERXWRPT >> 8) & 0xff;
		    break;

	    case REG_EDMASTL:
		    value = enc->regEDMAST & 0xff;
		    break;

	    case REG_EDMASTH:
		    value = (enc->regEDMAST >> 8) & 0xff;
		    break;

	    case REG_EDMANDL:
		    value = enc->regEDMAND & 0xff;
		    break;

	    case REG_EDMANDH:
		    value = (enc->regEDMAND >> 8) & 0xff;
		    break;

	    case REG_EDMADSTL:
		    value = enc->regEDMADST & 0xff;
		    break;

	    case REG_EDMADSTH:
		    value = (enc->regEDMADST >> 8) & 0xff;
		    break;

	    case REG_EDMACSL:
		    update_edmacs(enc);
		    value = (enc->regEDMACS & 0xff);
		    break;

	    case REG_EDMACSH:
		    update_edmacs(enc);
		    value = (enc->regEDMACS >> 8) & 0xff;
		    break;

	    case (OFS_BANK0 | REG_EIE):
	    case (OFS_BANK1 | REG_EIE):
	    case (OFS_BANK2 | REG_EIE):
	    case (OFS_BANK3 | REG_EIE):
		    value = enc->regEIE;
		    break;

	    case (OFS_BANK0 | REG_EIR):
	    case (OFS_BANK1 | REG_EIR):
	    case (OFS_BANK2 | REG_EIR):
	    case (OFS_BANK3 | REG_EIR):
		    value = enc->regEIR;
		    //fprintf(stderr,"EIR read 0x%02x\n",value);
		    break;

	    case (OFS_BANK0 | REG_ESTAT):
	    case (OFS_BANK1 | REG_ESTAT):
	    case (OFS_BANK2 | REG_ESTAT):
	    case (OFS_BANK3 | REG_ESTAT):
		    value = enc->regESTAT;
		    break;

	    case (OFS_BANK0 | REG_ECON2):
	    case (OFS_BANK1 | REG_ECON2):
	    case (OFS_BANK2 | REG_ECON2):
	    case (OFS_BANK3 | REG_ECON2):
		    value = enc->regECON2;
		    break;

	    case (OFS_BANK0 | REG_ECON1):
	    case (OFS_BANK1 | REG_ECON1):
	    case (OFS_BANK2 | REG_ECON1):
	    case (OFS_BANK3 | REG_ECON1):
		    value = enc->regECON1;
		    break;

	    case REG_EHT0:
		    value = enc->regEHT[0];
		    break;

	    case REG_EHT1:
		    value = enc->regEHT[1];
		    break;

	    case REG_EHT2:
		    value = enc->regEHT[2];
		    break;

	    case REG_EHT3:
		    value = enc->regEHT[3];
		    break;

	    case REG_EHT4:
		    value = enc->regEHT[4];
		    break;

	    case REG_EHT5:
		    value = enc->regEHT[5];
		    break;

	    case REG_EHT6:
		    value = enc->regEHT[6];
		    break;

	    case REG_EHT7:
		    value = enc->regEHT[7];
		    break;

	    case REG_EPMM0:
		    value = enc->regEPMM[0];
		    break;

	    case REG_EPMM1:
		    value = enc->regEPMM[1];
		    break;

	    case REG_EPMM2:
		    value = enc->regEPMM[2];
		    break;

	    case REG_EPMM3:
		    value = enc->regEPMM[3];
		    break;

	    case REG_EPMM4:
		    value = enc->regEPMM[4];
		    break;

	    case REG_EPMM5:
		    value = enc->regEPMM[5];
		    break;

	    case REG_EPMM6:
		    value = enc->regEPMM[6];
		    break;

	    case REG_EPMM7:
		    value = enc->regEPMM[7];
		    break;

	    case REG_EPMCSL:
		    value = enc->regEPMCS & 0xff;
		    break;

	    case REG_EPMCSH:
		    value = (enc->regEPMCS >> 8) & 0xff;
		    break;

	    case REG_EPMOL:
		    value = (enc->regEPMO & 0xff);
		    break;

	    case REG_EPMOH:
		    value = (enc->regEPMO >> 8) & 0xff;
		    break;

	    case REG_EWOLIE:
		    value = enc->regEWOLIE;
		    break;

	    case REG_EWOLIR:
		    value = enc->regEWOLIR;
		    break;

	    case REG_ERXFCON:
		    value = enc->regERXFCON;
		    break;

	    case REG_EPKTCNT:
		    value = enc->regEPKTCNT;
		    break;

	    case REG_MACON1:
		    value = enc->regMACON1;
		    break;

	    case REG_MACON2:
		    value = enc->regMACON2;
		    break;

	    case REG_MACON3:
		    value = enc->regMACON3;
		    break;

	    case REG_MACON4:
		    value = enc->regMACON4;
		    break;

	    case REG_MABBIPG:
		    value = enc->regMABBIPG;
		    break;

	    case REG_MAIPGL:
		    value = enc->regMAIPG & 0xff;
		    break;

	    case REG_MAIPGH:
		    value = (enc->regMAIPG >> 8) & 0xff;
		    break;

	    case REG_MACLCON1:
		    value = enc->regMACLCON1;
		    break;

	    case REG_MACLCON2:
		    value = enc->regMACLCON2;
		    break;

	    case REG_MAMXFLL:
		    value = enc->regMAMXFL & 0xff;
		    break;

	    case REG_MAMXFLH:
		    value = (enc->regMAMXFL >> 8) & 0xff;
		    break;

	    case REG_MAPHSUP:
		    value = enc->regMAPHSUP;
		    break;

	    case REG_MICON:
		    value = enc->regMICON;
		    break;

	    case REG_MICMD:
		    value = enc->regMICMD;
		    break;

	    case REG_MIREGADR:
		    value = enc->regMIREGADR;
		    break;

	    case REG_MIWRL:
		    value = enc->regMIWR & 0xff;
		    break;

	    case REG_MIWRH:
		    value = (enc->regMIWR >> 8) & 0xff;
		    break;

	    case REG_MIRDL:
		    value = (enc->regMIRD) & 0xff;
		    break;

	    case REG_MIRDH:
		    value = (enc->regMIRD >> 8) & 0xff;
		    break;

	    case REG_MAADR1:
		    value = enc->regMAADR[0];
		    break;

	    case REG_MAADR2:
		    value = enc->regMAADR[1];
		    break;

	    case REG_MAADR3:
		    value = enc->regMAADR[2];
		    break;

	    case REG_MAADR4:
		    value = enc->regMAADR[3];
		    break;

	    case REG_MAADR5:
		    value = enc->regMAADR[4];
		    break;

	    case REG_MAADR6:
		    value = enc->regMAADR[5];
		    break;

	    case REG_EBSTSD:
		    value = enc->regEBSTSD;
		    break;

	    case REG_EBSTCON:
		    value = enc->regEBSTCON;
		    break;

	    case REG_EBSTCSL:
		    value = enc->regEBSTCS & 0xff;
		    break;

	    case REG_EBSTCSH:
		    value = (enc->regEBSTCS >> 8) & 0xff;
		    break;

	    case REG_MISTAT:
		    value = enc->regMISTAT;
		    break;

	    case REG_EREVID:
		    value = enc->regEREVID;
		    break;

	    case REG_ECOCON:
		    value = enc->regECOCON;
		    break;

	    case REG_EFLOCON:
		    value = enc->regEFLOCON;
		    break;

	    case REG_EPAUSL:
		    value = enc->regEPAUS & 0xff;
		    break;

	    case REG_EPAUSH:
		    value = (enc->regEPAUS >> 8) & 0xff;
		    break;
	}
	return value;
}

/**
 **********************************************************
 * \fn static uint8_t opc_rbm(Enc28j60 *enc) 
 **********************************************************
 */
static uint8_t
opc_rbm(Enc28j60 * enc)
{
	uint8_t data;
	if (enc->regERDPT >= RXTXBUF_SIZE) {
		fprintf(stderr, "ENC28J60 should never happen, RX BUFP outside of buffer\n");
		return 0;
	}
	data = enc->rxtx_buf[enc->regERDPT];
	return data;
}

/**
 * Increment RBM pointer before reading the next byte
 */
static void
opc_rbm_inc(Enc28j60 * enc)
{
	if (enc->regECON2 & ECON2_AUTOINC) {
		if (enc->regERDPT == enc->regERXND) {
			enc->regERDPT = enc->regERXST;
		} else {
			enc->regERDPT += 1;
		}
		enc->regERDPT %= RXTXBUF_SIZE;
		//fprintf(stdout,"INCED ERDPT to %04x\n",enc->regERDPT);
	}
}

/*
 *****************************************************************************
 * \fn static void opc_wbm(Enc28j60 *enc,uint8_t data) 
 *****************************************************************************
 */
static void
opc_wbm(Enc28j60 * enc, uint8_t data)
{
	if (enc->regEWRPT >= RXTXBUF_SIZE) {
		fprintf(stderr, "ENC28J60:: should never happen, TX BUFP outside of buffer\n");
		sleep(1);
		return;
	}
	enc->rxtx_buf[enc->regEWRPT] = data;
	if (enc->regECON2 & ECON2_AUTOINC) {
		enc->regEWRPT = (enc->regEWRPT + 1) % RXTXBUF_SIZE;
	}
	return;
}

/**
 *********************************************************************************
 * \fn static void opc_bfs(Enc28j60 *enc,uint8_t reg_no,uint8_t ormask) 
 * Set some bits.
 *********************************************************************************
 */
static void
opc_bfs(Enc28j60 * enc, uint8_t reg_no, uint8_t ormask)
{
	uint8_t value;
	value = opc_rcr(enc, reg_no);
	value |= ormask;
	//fprintf(stderr,"BFS 0x%02x, or 0x%02x\n",reg_no,ormask);
	opc_wcr(enc, reg_no, value);
}

/**
 **********************************************************************************
 * \fn static void opc_bfc(Enc28j60 *enc,uint8_t reg_no,uint8_t andnotmask) 
 * Bit field clear. 
 **********************************************************************************
 */
static void
opc_bfc(Enc28j60 * enc, uint8_t reg_no, uint8_t andnotmask)
{
	uint8_t value;
	value = opc_rcr(enc, reg_no);
	value &= ~andnotmask;
	opc_wcr(enc, reg_no, value);
}

static void
enc_system_reset(Enc28j60 * enc)
{
	int i;
	CycleTimer_Remove(&enc->miCmdTimer);
	enc->miiwrite = 0;
	enc->regEIE = 0;
	enc->regEIR = 0;
	enc->regESTAT = ESTAT_CLKRDY;
	enc->regECON2 = ECON2_AUTOINC;
	enc->regECON1 = 0;
	enc->regERDPT = 0x05fa;
	enc->regEWRPT = 0;
	enc->regETXST = 0;
	enc->regETXND = 0;
	enc->regERXST = 0x5fa;
	enc->regERXND = 0x1fff;
	enc->regERXRDPT = 0x5fa;
	enc->regERXWRPT = 0;
	enc->regEDMAST = 0;
	enc->regEDMAND = 0;
	enc->regEDMADST = 0;
	enc->regEDMACS = 0;
	for (i = 0; i < 8; i++) {
		enc->regEHT[i] = 0;
		enc->regEPMM[i] = 0;
	}
	enc->regEPMCS = 0;
	enc->regEPMO = 0;
	enc->regERXFCON = 0xa1;
	enc->regEPKTCNT = 0;
	enc->regMACON1 = 0;
	enc->regMACON3 = 0;
	enc->regMACON4 = 0;
	enc->regMABBIPG = 0;
	enc->regMAIPG = 0;
	enc->regMACLCON1 = 0xf;
	enc->regMACLCON2 = 0x37;
	enc->regMAMXFL = 0x600;
	enc->regMICMD = 0;
	enc->regMIREGADR = 0;
	enc->regMIWR = 0;
	enc->regMIRD = 0;
	memset(enc->regMAADR, 0, sizeof(enc->regMAADR));
	enc->regEBSTSD = 0;
	enc->regEBSTCON = 0;
	enc->regEBSTCS = 0;
	enc->regEBSTCS = 0;
	enc->regMISTAT = 0;
	enc->regEREVID = 6;
	enc->regECOCON = 4;
	enc->regEFLOCON = 0;
	enc->regEPAUS = 0x1000;

	enc->phyrPHCON1 = 0;
	enc->phyrPHSTAT1 = 0x1800 | PHSTAT1_LLSTAT;
	enc->phyrPHID1 = 0x83;
	enc->phyrPHID2 = 0x1400;
	enc->phyrPHCON2 = 0;
	enc->phyrPHSTAT2 = PHSTAT2_LSTAT;
	enc->phyrPHIE = 0;
	enc->phyrPHIR = 0;
	enc->phyrPHLCON = 0x3422;
	update_interrupt(enc);
	dbgprintf("ENC28J60 system reset\n");
}

/**
 **************************************************************
 * \fn static void spi_byte_in(Enc28j60 *enc,uint8_t data) 
 * Feed a byte to the state machine.
 **************************************************************
 */
static void
spi_byte_in(Enc28j60 * enc, uint8_t data)
{
	//fprintf(stderr,"Byte in %02x, state %d\n",data,enc->state);
	switch (enc->state) {
	    case STATE_CMD:
		    enc->currOpcode = data >> 5;
		    enc->currArg = data & 0x1f;
		    switch (enc->currOpcode) {
			case (OP_RCR >> 5):
				if (rcr_is_delayed(enc, enc->currArg | (enc->currBank << 5))) {
					enc->state = STATE_DELAYED_RCR;
				} else {
					uint8_t value;
					value = opc_rcr(enc, enc->currArg | (enc->currBank << 5));
					enc->shift_out = value;
					enc->shiftout_cnt = 8;
					enc->state = STATE_RCR;
				}
				break;

			case (OP_RBM >> 5):
				enc->shift_out = opc_rbm(enc);;
				enc->shiftout_cnt = 8;
				enc->state = STATE_RBM;
				break;

			case (OP_WCR >> 5):
				/* wait for one date byte */
				enc->state = STATE_WCR;
				break;

			case (OP_WBM >> 5):
				enc->state = STATE_WBM;
				break;

			case (OP_BFS >> 5):
				enc->state = STATE_BFS;
				break;

			case (OP_BFC >> 5):
				enc->state = STATE_BFC;
				break;

			case (OP_SRC >> 5):
				if (data == OP_SRC) {
					enc_system_reset(enc);
				}
				break;
		    }
		    break;

	    case STATE_DELAYED_RCR:
		    {
			    uint8_t value;
			    value = opc_rcr(enc, enc->currArg | (enc->currBank << 5));
			    enc->state = STATE_FINISHED;
			    enc->shift_out = value;
			    enc->shiftout_cnt = 8;
		    }
		    break;

		    /* In undelayed rcr the byte is readable two times on the bus */
	    case STATE_RCR:
		    enc->shiftout_cnt = 8;
		    enc->state = STATE_FINISHED;
		    break;

	    case STATE_WCR:
		    opc_wcr(enc, enc->currArg | (enc->currBank << 5), data);
		    enc->state = STATE_FINISHED;
		    break;

	    case STATE_BFS:
		    opc_bfs(enc, enc->currArg | (enc->currBank << 5), data);
		    enc->state = STATE_FINISHED;
		    break;

	    case STATE_BFC:
		    opc_bfc(enc, enc->currArg | (enc->currBank << 5), data);
		    enc->state = STATE_FINISHED;
		    break;

	    case STATE_RBM:
		    opc_rbm_inc(enc);
		    enc->shift_out = opc_rbm(enc);;
		    enc->shiftout_cnt = 8;
		    break;

	    case STATE_WBM:
		    opc_wbm(enc, data);
		    break;

	    case STATE_FINISHED:
		    break;
	}
}

/*
 ***********************************************************************************
 * The parallel mode (lower simulation depth) interface
 ***********************************************************************************
 */
uint8_t
Enc28j80_SpiByteExchange(void *clientData, uint8_t data)
{
	Enc28j60 *enc = (Enc28j60 *) clientData;
	uint8_t retdata = enc->shift_out;
	spi_byte_in(enc, data);
	return retdata;
}

/**
 *******************************************************************************************
 * \fn static void spi_clk_change(SigNode *node,int value,void *clientData)
 *******************************************************************************************
 */
static void
spi_clk_change(SigNode * node, int value, void *clientData)
{
	Enc28j60 *enc = (Enc28j60 *) clientData;
	if (value == SIG_HIGH) {
		//fprintf(stderr,"TS %lld\n",CycleCounter_Get());
		if (SigNode_Val(enc->sigMosi) == SIG_HIGH) {
			enc->shift_in = (enc->shift_in << 1) | 1;
		} else {
			enc->shift_in = (enc->shift_in << 1);
		}
		enc->shiftin_cnt++;
		if (enc->shiftin_cnt == 8) {
			spi_byte_in(enc, enc->shift_in);
			enc->shiftin_cnt = 0;
		}
	} else if (value == SIG_LOW) {
		/* If there is no next byte goto high impedance */
#if 0
		if (enc->shiftout_cnt == 8) {
			fprintf(stderr, "SHIFTOUT %02x\n", enc->shift_out);
		}
#endif
		if (enc->shiftout_cnt > 0) {
			enc->shiftout_cnt--;
			if (enc->shift_out & (1 << enc->shiftout_cnt)) {
				SigNode_Set(enc->sigMiso, SIG_HIGH);
			} else {
				SigNode_Set(enc->sigMiso, SIG_LOW);
			}
		} else {
			SigNode_Set(enc->sigMiso, SIG_OPEN);
		}
	}
	return;
}

/**
 **************************************************************************
 * \fn static void spi_cs_change(SigNode *node,int value,void *clientData)
 **************************************************************************
 */
static void
spi_cs_change(SigNode * node, int value, void *clientData)
{
	Enc28j60 *enc = (Enc28j60 *) clientData;
	if (value == SIG_LOW) {
		enc->state = STATE_CMD;
		enc->shift_in = 0;
		enc->shiftin_cnt = 0;

		enc->shift_out = 0xff;
		/** 
                 ***********************************************************
                 * If clock is already low shiftout first bit immediately 
                 ***********************************************************
                 */
		if (SigNode_Val(enc->sigSck) == SIG_LOW) {
			//enc->shift_out = spi_fetch_next_byte(enc);
			if (enc->shift_out & 0x80) {
				SigNode_Set(enc->sigMiso, SIG_HIGH);
			} else {
				SigNode_Set(enc->sigMiso, SIG_LOW);
			}
			enc->shiftout_cnt = 1;
			enc->shift_out <<= 1;
		} else {
			fprintf(stderr, "Clock not low on CS low\n");
			enc->shiftout_cnt = 0;
		}
		if (enc->sckTrace) {
			fprintf(stderr, "Bug: clock trace already exists\n");
			return;
		}
		enc->sckTrace = SigNode_Trace(enc->sigSck, spi_clk_change, enc);
	} else {
		SigNode_Set(enc->sigMiso, SIG_OPEN);
		if (enc->sckTrace) {
			SigNode_Untrace(enc->sigSck, enc->sckTrace);
			enc->sckTrace = NULL;
		}
	}
	dbgprintf("CS of Ethernet chip %d\n", value);
	return;
}

/**
 ***************************************************************************
 * \fn void ENC28J60_New(const char *name); 
 ***************************************************************************
 */
Enc28j60 *
Enc28j60_New(const char *name)
{
	Enc28j60 *enc = sg_new(Enc28j60);
	enc->regECON2 = ECON2_AUTOINC;
	enc->sigSck = SigNode_New("%s.sck", name);
	enc->sigMosi = SigNode_New("%s.mosi", name);
	enc->sigMiso = SigNode_New("%s.miso", name);
	enc->sigCsN = SigNode_New("%s.ncs", name);
	enc->sigIrq = SigNode_New("%s.irq", name);
	if (!enc->sigSck || !enc->sigMosi || !enc->sigMiso || !enc->sigCsN || !enc->sigIrq) {
		fprintf(stderr, "Can not create signal lines for SPI flash \"%s\"\n", name);
		exit(1);
	}
	/* Paket dropping, useful for TCP testing */
	enc->rxdrop = enc->txdrop = 0;
	Config_ReadUInt32(&enc->rxdrop, name, "drop");
	Config_ReadUInt32(&enc->txdrop, name, "drop");
	Config_ReadUInt32(&enc->rxdrop, name, "rxdrop");
	Config_ReadUInt32(&enc->txdrop, name, "txdrop");

	CycleTimer_Init(&enc->miCmdTimer, micmd_timer_proc, enc);
	CycleTimer_Init(&enc->transmitTimer, do_transmit, enc);
	CycleTimer_Init(&enc->dmaTimer, do_dma, enc);
	SigNode_Set(enc->sigIrq, SIG_PULLUP);
	enc->CsNTrace = SigNode_Trace(enc->sigCsN, spi_cs_change, enc);
	enc->ether_fd = Net_CreateInterface(name);
	enc->input_fh = AsyncManager_PollInit(enc->ether_fd);
	enc_system_reset(enc);
	test_hash_calculator();
	return enc;
}
