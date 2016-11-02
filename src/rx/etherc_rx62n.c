/*
 **********************************************************************************************
 * Renesas RX62N/RX63N Ethernet controller with Ethernet DMA controller 
 *
 * State: Working but many registers missing 
 *
 * Copyright 2012 2013 Jochen Karrer. All rights reserved.
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
#include <stdbool.h>
#include <fcntl.h>
#include <unistd.h>
#include "sgstring.h"
#include "sglib.h"
#include "bus.h"
#include "etherc_rx62n.h"
#include "linux-tap.h"
#include "byteorder.h"
#include "cycletimer.h"
#include "fio.h"
#include "signode.h"
#include "configfile.h"

#define REG_EDMR(base)		((base) + 0x0000)
#define		EDMR_SWR		(1 << 0)
#define		EDMR_DL_MSK		(3 << 4)
#define		EDMR_DL_SHIFT		(4)
#define		EDMR_DE			(1 << 6)
#define REG_EDTRR(base)		((base) + 0x0008)
#define		EDTRR_TR		(1 << 0)
#define REG_EDRRR(base)		((base) + 0x0010)
#define		EDRRR_RR		(1 << 0)
#define REG_TDLAR(base)		((base) + 0x0018)
#define REG_RDLAR(base)		((base) + 0x0020)
#define REG_EESR(base)		((base) + 0x0028)
#define		EESR_CERF		(1 << 0)
#define		EESR_PRE		(1 << 1)
#define		EESR_RTSF		(1 << 2)
#define		EESR_RTLF		(1 << 3)
#define		EESR_RRF		(1 << 4)
#define		EESR_RMAF		(1 << 7)
#define		EESR_TRO		(1 << 8)
#define		EESR_CD			(1 << 9)
#define		EESR_DLC		(1 << 10)
#define		EESR_CND		(1 << 11)
#define		EESR_RFOF		(1 << 16)
#define		EESR_RDE		(1 << 17)
#define		EESR_FR			(1 << 18)
#define		EESR_TFUF		(1 << 19)
#define		EESR_TDE		(1 << 20)
#define		EESR_TC			(1 << 21)
#define		EESR_ECI		(1 << 22)
#define		EESR_ADE		(1 << 23)
#define		EESR_RFCOF		(1 << 24)
#define		EESR_RABT		(1 << 25)
#define		EESR_TABT		(1 << 26)
#define		EESR_TWB		(1 << 30)

#define	REG_EESIPR(base)	((base) + 0x0030)
#define		EESIPR_CERFIP		(1 << 0)
#define		EESIPR_PREIP		(1 << 1)
#define		EESIRP_RTSFIP		(1 << 2)
#define		EESIPR_RTLFIP		(1 << 3)
#define		EESIPR_RRFIP		(1 << 4)
#define		EESIPR_RMAFIP		(1 << 7)
#define		EESIPR_TROIP		(1 << 8)
#define		EESIPR_CDIP		(1 << 9)
#define		EESIPR_DLCIP		(1 << 10)
#define		EESIPR_CNDIP		(1 << 11)
#define		EESIPR_RFOFIP		(1 << 16)
#define		EESIPR_RDEIP		(1 << 17)
#define		EESIPR_FRIP		(1 << 18)
#define		EESIPR_TFUFIP		(1 << 19)
#define		EESIPR_TDEIP		(1 << 20)
#define		EESIPR_TCIP		(1 << 21)
#define		EESIPR_ECIIP		(1 << 22)
#define		EESIPR_ADEIP		(1 << 23)
#define		EESIPR_RFCOFIP		(1 << 24)
#define		EESIRP_RABTIP		(1 << 25)
#define		EESIPR_TABTIP		(1 << 26)
#define		EESIPR_TWBIP		(1 << 30)
#define REG_TRSCER(base)	((base) + 0x0038)
#define		TRSCER_CERFCE		(1 << 0)
#define		TRSCER_PRECE		(1 << 1)
#define		TRSCER_RTSFCE		(1 << 2)
#define		TRSCER_RTLFCE		(1 << 3)
#define		TRSCER_RRFCE		(1 << 4)
#define		TRSCER_RMAFCE		(1 << 7)
#define		TRSCER_TROCE		(1 << 8)
#define		TRSCER_CDCE		(1 << 9)
#define		TRSCER_DLCCE		(1 << 10)
#define		TRSCER_CNDCE		(1 << 11)
#define REG_RMFCR(base)		((base) + 0x0040)
#define	REG_TFTR(base)		((base) + 0x0048)
#define REG_FDR(base)		((base) + 0x0050)
#define REG_RMCR(base)		((base) + 0x0058)
#define		RMCR_RNR		(1 << 0)
#define		RMCR_RNC		(1 << 1)
#define REG_TFUCR(base)		((base) + 0x0064)
#define REG_RFOCR(base)		((base) + 0x0068)
#define REG_RBWAR(base)		((base) + 0x00c8)
#define REG_RDFAR(base)		((base) + 0x00cc)
#define	REG_TBRAR(base)		((base) + 0x00d4)
#define REG_TDFAR(base)		((base) + 0x00d8)
#define REG_FCFTR(base)		((base) + 0x0070)
#define REG_RPADIR(base)	((base) + 0x0078)
#define REG_TRIMD(base)		((base) + 0x007c)
#define		TRIMD_TIS	(1 << 0)
#define		TRIMD_TIM	(1 << 4)
#define REG_IOSR(base)		((base) + 0x006c)
#define		IOSR_ELB	(1 << 0)
/**
 *******************************************************
 * Now the registers of the Ethernet controller
 *******************************************************
 */
#define REG_ECMR(base)		((base) + 0x100)
#define		ECMR_PRM	(1 << 0)
#define 	ECMR_DM		(1 << 1)
#define		ECMR_RTM	(1 << 2)
#define		ECMR_ILB	(1 << 3)
#define		ECMR_TE		(1 << 5)
#define		ECMR_RE		(1 << 6)
#define		ECMR_MPDE	(1 << 9)
#define		ECMR_PRCEF	(1 << 12)
#define		ECMR_TXF	(1 << 16)
#define		ECMR_RXF	(1 << 17)
#define		ECMR_PFR	(1 << 18)
#define		ECMR_ZPF	(1 << 19)
#define		ECMR_TPC	(1 << 20)

#define REG_RFLR(base)		((base) + 0x108)
#define REG_ECSR(base)		((base) + 0x110)
#define		ECSR_ICD	(1 << 0)
#define		ECSR_MPD	(1 << 1)
#define		ECSR_LCHGN	(1 << 2)
#define		ECSR_PSTRO	(1 << 4)
#define		ECSR_BFR	(1 << 5)
#define REG_ECSIPR(base)	((base) + 0x118)
#define		ECSIPR_ICDIP		(1 << 0)
#define		ECSIPR_MPDIP		(1 << 1)
#define		ECSIPR_LCHNGIP		(1 << 2)
#define		ECSIPR_PSRTOIP		(1 << 4)
#define		ECSIPR_BFSIPR		(1 << 5)
#define REG_PIR(base)		((base) + 0x120)
#define		PIR_MDC			(1 << 0)
#define		PIR_MMD			(1 << 1)
#define		PIR_MDO			(1 << 2)
#define		PIR_MDI			(1 << 3)
#define REG_PSR(base)		((base) + 0x128)
#define		PSR_LMON	(1 << 0)
#define REG_RDMLR(base)		((base) + 0x140)
#define	REG_IPGR(base)		((base) + 0x150)
#define REG_APR(base)		((base) + 0x154)
#define REG_MPR(base)		((base) + 0x158)
#define REG_RFCF(base)		((base) + 0x160)
#define REG_TPAUSER(base)	((base) + 0x164)
#define REG_TPAUSECR(base)	((base) + 0x168)
#define REG_BCFRR(base)		((base) + 0x16c)
#define REG_MAHR(base)		((base) + 0x1c0)
#define REG_MALR(base)		((base) + 0x1c8)
#define REG_TROCR(base)		((base) + 0x1d0)
#define REG_CDCR(base)		((base) + 0x1d4)
#define REG_LCCR(base)		((base) + 0x1d8)
#define REG_CNDCR(base)		((base) + 0x1dc)
#define REG_CEFCR(base)		((base) + 0x1e4)
#define REG_FRECR(base)		((base) + 0x1e8)
#define REG_TSFRCR(base)	((base) + 0x1ec)
#define REG_TLFRCR(base)	((base) + 0x1f0)
#define REG_RFCR(base)		((base) + 0x1f4)
#define REG_MAFCR(base)		((base) + 0x1f8)

#define TXFIFO_SIZE	(2048)
#define TXFIFO_RP(re)	((re)->txfifo_rp % TXFIFO_SIZE)
#define TXFIFO_WP(re)	((re)->txfifo_wp % TXFIFO_SIZE)
#define RXFIFO_SIZE	(2048)
#define RXFIFO_RP(re)	((re)->rxfifo_rp % RXFIFO_SIZE)
#define RXFIFO_WP(re)	((re)->rxfifo_wp % RXFIFO_SIZE)

typedef struct RxEth {
	BusDevice bdev;
	CycleTimer txTimer;
	CycleTimer rxTimer;

	FIO_FileHandler inputFH;
	int receiver_is_enabled;
	SigNode *sigIRQ;
	SigNode *sigMDC;
	SigNode *sigMDIO;
	SigNode *sigLMON;

	uint32_t rxdropPerc; /* Percent of packets thrown away */
	uint32_t txdropPerc;

	uint8_t txfifo[TXFIFO_SIZE];
	uint8_t rxfifo[RXFIFO_SIZE];
	uint16_t rxfifo_wp;
	uint16_t rxfifo_rp;
	uint16_t txfifo_wp;
	uint16_t txfifo_rp;
	int ether_fd;
	uint32_t regEDMR;
	uint32_t regEDTRR;
	uint32_t regEDRRR;
	uint32_t regTDLAR;
	uint32_t regRDLAR;
	uint32_t regEESR;
	uint32_t regEESIPR;
	uint32_t regTRSCER;
	uint32_t regRMFCR;
	uint32_t regTFTR;
	uint32_t regFDR;
	uint32_t regRMCR;
	uint32_t regTFUCR;
	uint32_t regRFOCR;
	uint32_t regRBWAR;
	uint32_t regRDFAR;
	uint32_t regTBRAR;
	uint32_t regTDFAR;
	uint32_t regFCFTR;
	uint32_t regRPADIR;
	uint32_t regTRIMD;
	uint32_t regIOSR;

	uint32_t regECMR;
	uint32_t regRFLR;
	uint32_t regECSR;
	uint32_t regECSIPR;
	uint32_t regPIR;
	uint32_t regPSR;
	uint32_t regRDMLR;
	uint32_t regIPGR;
	uint32_t regAPR;
	uint32_t regMPR;
	uint32_t regRFCF;
	uint32_t regTPAUSER;
	uint32_t regTPAUSECR;
	uint32_t regBCFRR;
	uint8_t MAC[6];
	uint32_t regTROCR;
	uint32_t regCDCR;
	uint32_t regLCCR;
	uint32_t regCNDCR;
	uint32_t regCEFCR;
	uint32_t regFRECR;
	uint32_t regTSFRCR;
	uint32_t regTLFRCR;
	uint32_t regRFCR;
	uint32_t regMAFCR;
} RxEth;

/**
 *********************************************************************
 * Each transmit descriptor is cleared to 0 by a reset 
 *********************************************************************
 */
#define TXDS_ACT        (UINT32_C(1) << 31)
#define TXDS_DLE        (UINT32_C(1) << 30)
#define TXDS_FP1        (UINT32_C(1) << 29)
#define TXDS_FP0        (UINT32_C(1) << 28)
#define TXDS_FE         (UINT32_C(1) << 27)
#define TXDS_WBI        (UINT32_C(1) << 26)

#define TFS_DTA         (UINT32_C(1) << 8)	/* Detect Transmission Abort    */
#define TFS_DNC         (UINT32_C(1) << 3)	/* Detect No Carrier            */
#define TFS_DLC         (UINT32_C(1) << 2)	/* Detect Loss of Carrier       */
#define TFS_DDC         (UINT32_C(1) << 1)	/* Detect Delayed Collision     */
#define TFS_TRO         (UINT32_C(1) << 0)	/* Transmit Retry Over */

typedef struct TransmitDescriptor {
	uint32_t td0Status;
	uint32_t td1TBL;	/* Transmit buffer len */
	uint32_t td2TBA;	/* Transmit buffer address */
} TransmitDescriptor;

/**
 *********************************************************************
 * Each receive descriptor is cleared to 0 by a reset 
 *********************************************************************
 */

#define RXDS_ACT        (UINT32_C(1) << 31)
#define RXDS_DLE        (UINT32_C(1) << 30)
#define RXDS_FP1        (UINT32_C(1) << 29)
#define RXDS_FP0        (UINT32_C(1) << 28)
#define RXDS_FE         (UINT32_C(1) << 27)

#define RFS_OVL         (UINT32_C(1) << 9)
#define RFS_DRA         (UINT32_C(1) << 8)
#define RFS_RMAF        (UINT32_C(1) << 7)
#define RFS_RRF         (UINT32_C(1) << 4)
#define RFS_RTLF        (UINT32_C(1) << 3)
#define RFS_RTSF        (UINT32_C(1) << 2)
#define RFS_PRE         (UINT32_C(1) << 1)
#define RFS_CERF        (UINT32_C(1) << 0)

typedef struct ReceiveDescriptor {
	uint32_t rd0Status;
	uint16_t rd1RFL;
	uint16_t rd1RBL;
	uint32_t rd2RBA;
} ReceiveDescriptor;

static void
update_interrupt(RxEth * re)
{
	if (re->regEESR & re->regEESIPR) {
		//fprintf(stderr,"Update interrupt to LOW\n");
		SigNode_Set(re->sigIRQ, SIG_LOW);
	} else {
		//fprintf(stderr,"Update interrupt to High\n");
		SigNode_Set(re->sigIRQ, SIG_PULLUP);
	}
}

static void
update_eci_interrupt(RxEth * re)
{
	if (re->regECSR & re->regECSIPR) {
		re->regEESR |= EESR_ECI;
	} else {
		re->regEESR &= ~EESR_ECI;
	}
	update_interrupt(re);
}

static void rxpkt_sink(void *eventData, int mask);
static void
enable_receiver(RxEth * re)
{
	if (!re->receiver_is_enabled && (re->ether_fd >= 0)) {
		//dbgprintf("RXEth:: enable receiver\n");
		FIO_AddFileHandler(&re->inputFH, re->ether_fd, FIO_READABLE, rxpkt_sink, re);
		re->receiver_is_enabled = 1;
	}
}

static void
disable_receiver(RxEth * re)
{
	if (re->receiver_is_enabled && (re->ether_fd >= 0)) {
		FIO_RemoveFileHandler(&re->inputFH);
		re->receiver_is_enabled = 0;
	}
}

/**
 *****************************************************************
 * static inline bool match_unicast(RxEth *re, uint8_t * mac)
 * returns true if enc mac address matches
 *****************************************************************
 */
static inline bool
match_unicast(RxEth * re, uint8_t * mac)
{
	return (memcmp(mac, re->MAC, 6) == 0);
}

/**
 ********************************************************************
 * \fn static inline bool match_broadcast(RxEth *re, uint8_t * mac)
 * returns true if the MAC address is a broadcast 
 ********************************************************************
 */
static inline bool
match_broadcast(RxEth * re, uint8_t * mac)
{
	uint8_t broadcast[] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };
	return (memcmp(mac, broadcast, 6) == 0);
}

/**
 ********************************************************************
 * \fn static inline bool match_multicast(RxEth *re, uint8_t * mac)
 * returns true if the MAC address is a multicast. 
 ********************************************************************
 */
static inline bool
match_multicast(RxEth * re, uint8_t * mac)
{
	return ! !(mac[0] & 1);
}

/**
 ****************************************************************************************
 * \fn static void rx_descriptor_read(RxEth *re,ReceiveDescriptor *rdscr)
 ****************************************************************************************
 */
static void
rx_descriptor_read(RxEth * re, ReceiveDescriptor * rdscr)
{
	uint32_t rdfar = re->regRDFAR;
	if (re->regEDMR & EDMR_DE) {
		uint32_t tmp;
		rdscr->rd0Status = le32_to_host(Bus_Read32(rdfar));
		tmp = le32_to_host(Bus_Read32(rdfar + 4));
		rdscr->rd1RFL = tmp & 0xffff;
		rdscr->rd1RBL = tmp >> 16;
		rdscr->rd2RBA = le32_to_host(Bus_Read32(rdfar + 8));
	} else {
		uint32_t tmp;
		rdscr->rd0Status = be32_to_host(Bus_Read32(rdfar));
		tmp = be32_to_host(Bus_Read32(rdfar + 4));
		rdscr->rd1RFL = tmp & 0xffff;
		rdscr->rd1RBL = tmp >> 16;
		rdscr->rd2RBA = be32_to_host(Bus_Read32(rdfar + 8));
	}
}

static void
rx_descriptor_write(RxEth * re, ReceiveDescriptor * rdscr)
{
	uint32_t rdfar = re->regRDFAR;
	uint32_t tmp;
	tmp = rdscr->rd1RFL;
	tmp |= ((uint32_t) rdscr->rd1RBL) << 16;
	if (re->regEDMR & EDMR_DE) {
		Bus_Write32(host32_to_le(rdscr->rd0Status), rdfar);
		Bus_Write32(host32_to_le(tmp), rdfar + 4);
	} else {
		Bus_Write32(host32_to_be(rdscr->rd0Status), rdfar);
		Bus_Write32(host32_to_be(tmp), rdfar + 4);
	}
}

/**
 **************************************************************************
 * \fn static void read_tx_descriptor(RxEth *re,TransmitDescriptor *tdscr) 
 * Read a txdescriptor from memory.
 **************************************************************************
 */
static void
read_tx_descriptor(RxEth * re, TransmitDescriptor * tdscr)
{
	/* Byte order ? */
	uint32_t tdfar = re->regTDFAR;
	if (re->regEDMR & EDMR_DE) {
		tdscr->td0Status = le32_to_host(Bus_Read32(tdfar));
		tdscr->td1TBL = le32_to_host(Bus_Read32(tdfar + 4)) >> 16;
		tdscr->td2TBA = le32_to_host(Bus_Read32(tdfar + 8));
	} else {
		tdscr->td0Status = be32_to_host(Bus_Read32(tdfar));
		tdscr->td1TBL = be32_to_host(Bus_Read32(tdfar + 4)) >> 16;
		tdscr->td2TBA = be32_to_host(Bus_Read32(tdfar + 8));
	}
}

/**
 ******************************************************************
 * Write back the tx descriptor status
 ******************************************************************
 */
static void
write_tx_descriptor_status(RxEth * re, TransmitDescriptor * tdscr)
{
	uint32_t tdfar = re->regTDFAR;
	if (re->regEDMR & EDMR_DE) {
		Bus_Write32(host32_to_le(tdscr->td0Status), tdfar);
	} else {
		Bus_Write32(host32_to_be(tdscr->td0Status), tdfar);
	}
}

/**
 **************************************************************
 * \fn void transmit(RxEth *re)
 * Transmit a paket in the TX-Buffer.
 **************************************************************
 */
static void
transmit(RxEth * re)
{
	int i;
	ssize_t result;
	uint16_t len;
	uint8_t pkt[TXFIFO_SIZE];
	len = (re->txfifo_wp - re->txfifo_rp) & 0xffff;
	if (len > TXFIFO_SIZE) {
		len = TXFIFO_SIZE;
	}
	for (i = 0; i < len; i++) {
		pkt[i] = re->txfifo[TXFIFO_RP(re)];
		re->txfifo_rp++;
	}
	if (len < 60) {
		memset(pkt + len, 0x00, 60 - len);
		len = 60;
	} else if (len > TXFIFO_SIZE) {
		len = TXFIFO_SIZE;
	}
	if (re->txdropPerc && ((rand() % 100) < re->txdropPerc)) {
		fprintf(stdout, "Drop TX packet\n");
	} else if(re->ether_fd >= 0) {
		fcntl(re->ether_fd, F_SETFL, 0);
		result = write(re->ether_fd, pkt, len);
		fcntl(re->ether_fd, F_SETFL, O_NONBLOCK);
	}
}

/**
 *********************************************************************
 * \fn static void copy_tfs(RxEth *re,TransmitDescriptor *txdscr)
 * Copy the transmit status to the descripor.
 * All TFS fields are always copied to the txdescriptor.
 * The  TRSCER register has only influence on the TXDS_FE bit. 
 *********************************************************************
 */
static void
copy_tfs(RxEth * re, TransmitDescriptor * txdscr)
{
	uint32_t tfs;
	bool tfe;
	tfs = re->regEESR & (EESR_TRO | EESR_CD | EESR_DLC | EESR_CND) >> 8;

	txdscr->td0Status = (txdscr->td0Status & ~UINT32_C(0x10f)) | tfs;
	tfe = ! !(re->regEESR & ((~(re->regTRSCER >> 8) & 0x0f) | 0x100));
	if (tfe) {
		txdscr->td0Status |= TXDS_FE;
	} else {
		txdscr->td0Status &= ~TXDS_FE;
	}
}

static uint32_t
dma_one_tx_buffer(RxEth * re)
{
	uint8_t tx_frm_pos;
	uint32_t i;
	TransmitDescriptor txdesc;
	read_tx_descriptor(re, &txdesc);
	tx_frm_pos = (txdesc.td0Status >> 28) & 3;
	if (!(txdesc.td0Status & TXDS_ACT)) {
		return 0;
	}
	/* First buffer of a frame ? */
	if (tx_frm_pos == 2) {
		re->txfifo_rp = re->txfifo_wp = 0;
	}
	for (i = 0; i < txdesc.td1TBL; i++) {
		uint8_t data;
		data = Bus_Read8(txdesc.td2TBA + i);
		re->txfifo[TXFIFO_WP(re)] = data;
		re->txfifo_wp++;
	}
	re->regTBRAR = txdesc.td2TBA + (i & ~0x1f);
	txdesc.td0Status &= ~(TXDS_ACT);
	/* If last or only frame transmit it */
	if ((tx_frm_pos == 1) || (tx_frm_pos == 3)) {
		transmit(re);
		if ((re->regTRIMD & TRIMD_TIS) && !(re->regTRIMD & TRIMD_TIM)) {
			re->regEESR |= EESR_TWB;
		}
		re->regEESR |= EESR_TC;
	}
	if ((re->regTRIMD & TRIMD_TIS) && (re->regTRIMD & TRIMD_TIM)) {
		if (txdesc.td0Status & TXDS_WBI) {
			/* trigger transmit interrupt */
			re->regEESR |= EESR_TWB;
		}
	}
	copy_tfs(re, &txdesc);
	write_tx_descriptor_status(re, &txdesc);
	update_interrupt(re);
	if (txdesc.td0Status & TXDS_DLE) {
		re->regTDFAR = re->regTDLAR;
	} else {
		uint16_t descLen;
		switch ((re->regEDMR & EDMR_DL_MSK) >> EDMR_DL_SHIFT) {
		    case 0:
			    descLen = 16;
			    break;
		    case 1:
			    descLen = 32;
			    break;
		    case 2:
			    descLen = 64;
			    break;
		    default:
		    case 3:
			    descLen = 16;
		}
		re->regTDFAR += descLen;
	}
	return txdesc.td1TBL;
}

static void
transmit_timer_proc(void *eventData)
{
	RxEth *re = eventData;
	uint32_t len;
	len = dma_one_tx_buffer(re);
	if (len) {
		CycleTimer_Mod(&re->txTimer, NanosecondsToCycles(len * 100));
	} else {
		re->regEDTRR = 0;
	}
}

/**
 ******************************************************************
 * All RFS fields are always copied to the descriptor
 * The TRSCER register has only influence on the RXDS_FE bit
 ******************************************************************
 */
static void
copy_rfs(RxEth * re, ReceiveDescriptor * rxdscr)
{
	uint32_t rfs;
	bool rfe;
	rfs = re->regEESR & (EESR_CERF | EESR_PRE | EESR_RTSF |
			     EESR_RTLF | EESR_RRF | EESR_RMAF | EESR_RFOF);
	//rfs = rfs & ((~re->regTRSCER & 0x9f)  | 0x300);

	rxdscr->rd0Status = (rxdscr->rd0Status & ~0x39f) | rfs;

	rfe = ! !(rfs = re->regEESR & ((~re->regTRSCER & 0x9f) | 0x300));
	if (rfe) {
		rxdscr->rd0Status |= RXDS_FE;
	} else {
		rxdscr->rd0Status &= ~RXDS_FE;
	}
}

/*
 ******************************************************************************
 * \fn void rxpkt_dma_writepkt(RxEth *re,uint8_t *pkt,unsigned int count)
 * Write a paket to the memory.
 ******************************************************************************
 */

static void
rxpkt_dma_writepkt(RxEth * re, uint8_t * pkt, unsigned int count)
{
	ReceiveDescriptor rxdscr;
	unsigned int len;
	unsigned int frmcnt;
	uint32_t rfp;
	uint32_t i;
	uint8_t *data = pkt;
	frmcnt = 0;
	if (match_multicast(re, pkt)) {
		re->regEESR |= EESR_RMAF;
		if (re->regMAFCR != ~UINT32_C(0)) {
			re->regMAFCR++;
		}
	} else {
		re->regEESR &= ~EESR_RMAF;
	}
	while (count) {
		rx_descriptor_read(re, &rxdscr);
		if (!(rxdscr.rd0Status & RXDS_ACT)) {
			if (!(re->regRMCR & RMCR_RNC)) {
				re->regEDRRR = 0;
				disable_receiver(re);
			}
			return;
		}
		if (rxdscr.rd1RBL >= count) {
			len = count;
		} else {
			len = rxdscr.rd1RBL;
			if (rxdscr.rd1RBL == 0) {
				fprintf(stderr, "Driver Bug: Zero Receive Buffer Len\n");
				return;
			}
		}

		re->regRBWAR = (rxdscr.rd2RBA + (len & ~0x1f));
		for (i = 0; i < len; i++) {
			Bus_Write8(data[i], rxdscr.rd2RBA + i);
		}
		rxdscr.rd1RFL = len;
		count -= len;
		data += len;
		rxdscr.rd0Status = rxdscr.rd0Status & ~RXDS_ACT;
		frmcnt++;
		if (count == 0) {
			if (frmcnt == 1) {
				rfp = 3;
			} else {
				rfp = 1;
			}
			re->regEESR |= EESR_FR;
			update_interrupt(re);
		} else {
			if (frmcnt == 1) {
				rfp = 2;
			} else {
				rfp = 0;
			}
		}
		rxdscr.rd0Status = (rxdscr.rd0Status & ~(UINT32_C(3) << 28)) | (rfp << 28);
		copy_rfs(re, &rxdscr);
		rx_descriptor_write(re, &rxdscr);
		if (rxdscr.rd0Status & RXDS_DLE) {
			re->regRDFAR = re->regRDLAR;
		} else {
			uint16_t descLen;
			switch ((re->regEDMR & EDMR_DL_MSK) >> EDMR_DL_SHIFT) {
			    case 0:
				    descLen = 16;
				    break;
			    case 1:
				    descLen = 32;
				    break;
			    case 2:
				    descLen = 64;
				    break;
			    default:
			    case 3:
				    descLen = 16;
			}
			re->regRDFAR += descLen;
		}
	}
}

static bool
check_addr_accept(RxEth * re, uint8_t * pkt)
{
	if (re->regECMR & ECMR_PRM) {
		/* Promiscuous mode accepts every address */
		fprintf(stderr, "PRM\n");
		return true;
	}
	if (match_broadcast(re, pkt) == true) {
		return true;
	}
	if (match_multicast(re, pkt) == true) {
		//fprintf(stderr,"Mult %02x %02x %02x %02x\n",pkt[0],pkt[1],pkt[2],pkt[3]);
		return true;
	}
	if (match_unicast(re, pkt) == true) {
		return true;
	}
	return false;
}

static void
rxpkt_sink(void *eventData, int mask)
{
	RxEth *re = eventData;
	int result;
	uint8_t pktbuf[2048];

	while (re->receiver_is_enabled) {
		result = read(re->ether_fd, pktbuf, sizeof(pktbuf));
		if (result <= 0) {
			break;
		}
		if (re->rxdropPerc && ((rand() % 100) < re->rxdropPerc)) {
			fprintf(stdout, "Drop RX packet\n");
			continue;
		}
		if (check_addr_accept(re, pktbuf) == true) {
			rxpkt_dma_writepkt(re, pktbuf, result);
			if (!(re->regRMCR & RMCR_RNR)) {
				re->regEDRRR = 0;
				disable_receiver(re);
			}
		}
	}
}

static void
edmac_reset(RxEth * re)
{
	re->regEDMR = 0;
	re->regEDTRR = 0;
	re->regEDRRR = 0;
	/* re->regTDLAR is not reset */
	re->regRDLAR = 0;
	re->regEESR = 0;
	re->regEESIPR = 0;
	re->regTRSCER = 0;
	/* re->regRMFCR is not reset */
	re->regTFTR = 0;
	re->regFDR = 0x00000700;
	re->regRMCR = 0;
	/* re->regTFUCR is not reset */
	/* re->regRFOCR is not reset */
	re->regRBWAR = 0;
	re->regRDFAR = 0;
	re->regTBRAR = 0;
	re->regTDFAR = 0;
	re->regFCFTR = 0x00070007;
	re->regRPADIR = 0x00070007;
	re->regTRIMD = 0;
	re->regIOSR = 0;
}

/*
 ****************************************************************************
 * EDMAC Mode Register
 * Bit 0: SWR Software reset 1: Internal hardware reset. Reads as 0
 * Bit 4,5: DL Transmit/Receive Descriptor Length.
 *	    00: 16 Bytes 01: 32 Bytes 10: 64 Bytes 11: 16 Bytes
 * Bit 6: DE Transmission Data Endian 0: Big endian (longword access)
 ****************************************************************************
 */
static uint32_t
edmr_read(void *clientData, uint32_t address, int rqlen)
{
	RxEth *re = (RxEth *) clientData;
	return re->regEDMR;
}

static void
edmr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	RxEth *re = (RxEth *) clientData;
	re->regEDMR = value & 0x70;
	if (value & EDMR_SWR) {
		edmac_reset(re);
	}
}

/*
 ****************************************************************************
 * EDTRR EDMAC Transmit Request Register
 * Bit 0: TR 1: Transmission start 
 ****************************************************************************
 */
static uint32_t
edtrr_read(void *clientData, uint32_t address, int rqlen)
{
	RxEth *re = (RxEth *) clientData;
	return re->regEDTRR;
}

static void
edtrr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	RxEth *re = (RxEth *) clientData;
	if (value & EDTRR_TR) {
		if (!CycleTimer_IsActive(&re->txTimer)) {
			// should read first descriptor TBL here
			CycleTimer_Mod(&re->txTimer, NanosecondsToCycles(64 * 100));
		}
	}
	re->regEDTRR = value & 1;
}

/*
 ****************************************************************************
 * EDRRR EDMAC Receive Request Register
 * Bit 0: RR Receive Request 1: Read receive descriptor and become ready to 
 *           receive
 ****************************************************************************
 */
static uint32_t
edrrr_read(void *clientData, uint32_t address, int rqlen)
{
	RxEth *re = (RxEth *) clientData;
	return re->regEDRRR;
}

static void
edrrr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	RxEth *re = (RxEth *) clientData;
	if (value & EDRRR_RR) {
		if (re->regECMR & ECMR_RE) {
			enable_receiver(re);
		}
	} else {
		disable_receiver(re);
		//abortdma
		fprintf(stderr, "RXEth: Non recommended way to disable receiver\n");
	}
	re->regEDRRR = value & EDRRR_RR;

}

/*
 ****************************************************************************
 * Transmit Descriptor List Start Address Register TDLAR
 * Start address must be aligned according to descriptor length.
 * This is the start Address ! Not the current address which is stored in
 * the tdfar. 
 ****************************************************************************
 */
static uint32_t
tdlar_read(void *clientData, uint32_t address, int rqlen)
{
	RxEth *re = (RxEth *) clientData;
	return re->regTDLAR;;
}

static void
tdlar_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	RxEth *re = (RxEth *) clientData;
	re->regTDLAR = value;
	re->regTDFAR = value;
	/* Check boundaries */
}

/**
 **************************************************************************
 * RDLAR Receive Descriptor List Start Address Register
 * Address must be aligned according to the descriptor length.
 **************************************************************************
 */
static uint32_t
rdlar_read(void *clientData, uint32_t address, int rqlen)
{
	RxEth *re = (RxEth *) clientData;
	return re->regRDLAR;
}

static void
rdlar_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	RxEth *re = (RxEth *) clientData;
	re->regRDLAR = value;
	re->regRDFAR = value;
}

/**
 **************************************************************************
 * EESR Etherc/EDMAC Status Register
 * Bit 0:  CERF  CRC Error on Received Frame
 * Bit 1:  PRE   PHY-LSI Receiver Error
 * Bit 2:  RTSF  Receive Too-Short Frame
 * Bit 3:  RTLF  Receive Too-Long  Frame
 * Bit 4:  RRF   Receive Residual Bit Frame
 * Bit 7:  RMAF  Receive Multicast Frame
 * Bit 8:  TRO	 Transmit Retry Over
 * Bit 9:  CD	 Delayed Collision Detect
 * Bit 10: DLC	 Detect Loss of Carrier 
 * Bit 11: CND   Carrier Not Detect
 * Bit 16: RFOF	 Receive FIFO Overflow
 * Bit 17: RDE	 Receive Descriptor Empty
 * Bit 18: FR    Frame Reception
 * Bit 19: TFUF  Transmit FIFO Underflow
 * Bit 20: TDE	 Transmit Descriptor Empty
 * Bit 21: TC    Frame Transmit Complete
 * Bit 22: ECI   ETHERC Status Register Source
 * Bit 23: ADE   Address Error
 * Bit 24: RFCOF Receive Frame Counter Overflow
 * Bit 25: RABT  Receive Abort Detect
 * Bit 26: TABT  Transmit Abort Detect
 * Bit 30: TWB   Write Back Complete
 **************************************************************************
 */
static uint32_t
eesr_read(void *clientData, uint32_t address, int rqlen)
{
	RxEth *re = clientData;
	return re->regEESR;;
}

static void
eesr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	RxEth *re = clientData;
	re->regEESR &= ~value;
	update_interrupt(re);
}

/**
 **************************************************************************
 * EESIPR Interrupt enables for Status Interrupts.
 **************************************************************************
 */
static uint32_t
eesipr_read(void *clientData, uint32_t address, int rqlen)
{
	RxEth *re = clientData;
	return re->regEESIPR;;
}

static void
eesipr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	RxEth *re = clientData;
	re->regEESIPR = value;
	update_interrupt(re);
}

/**
 **************************************************************************
 * TRSCER Transmit/Receive Status Copy Enable Register
 * This register decides which bits of the status are copied
 * to the receive descriptor RFS bits
 **************************************************************************
 */

static uint32_t
trscer_read(void *clientData, uint32_t address, int rqlen)
{
	RxEth *re = clientData;
	return re->regTRSCER;
}

static void
trscer_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	RxEth *re = clientData;
	re->regTRSCER = value & 0x0f9f;
}

/*
 **************************************************************************
 * RMFCR Receive Missed-Frame Counter Register
 * Bit 0-15: MFC Missed Frame counter
 **************************************************************************
 */
static uint32_t
rmfcr_read(void *clientData, uint32_t address, int rqlen)
{
	RxEth *re = clientData;
	return re->regRMFCR;;
}

static void
rmfcr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	RxEth *re = clientData;
	re->regRMFCR = 0;
}

/*
 **************************************************************************
 * TFTR Transmit FIFO Threshold Register 
 **************************************************************************
 */
static uint32_t
tftr_read(void *clientData, uint32_t address, int rqlen)
{
	RxEth *re = clientData;
	return re->regTFTR;
}

static void
tftr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	RxEth *re = clientData;
	re->regTFTR = value & 0x7ff;
	if ((value > 0x200) || (value <= 0xc)) {
		if (value != 0) {
			fprintf(stderr, "Illegal value 0x%x in %s\n", value, __func__);
		}
	}
}

/**
 **************************************************************************
 * FDR FIFO Depth Register
 * Bit 0-4:  Receive FIFO Size
 * Bit 8-12: Transmit FIFO Size
 **************************************************************************
 */
static uint32_t
fdr_read(void *clientData, uint32_t address, int rqlen)
{
	RxEth *re = clientData;
	return re->regFDR;;
}

static void
fdr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	RxEth *re = clientData;
	re->regFDR = value & 0x1f1f;
	if (value & 0xfffff0f0) {
		fprintf(stderr, "Illegal value 0x%x in %s\n", value, __func__);
	}
}

/*
 **************************************************************************
 * RMCR Receiving Method Control Register
 * Bit 0: RNR Receive Request Bit Reset
 * Bit 1: RNC Receive Request Bit Non-Reset Mode
 **************************************************************************
 */
static uint32_t
rmcr_read(void *clientData, uint32_t address, int rqlen)
{
	RxEth *re = clientData;
	return re->regRMCR;
}

static void
rmcr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	RxEth *re = clientData;
	re->regRMCR = value & 3;
}

/**
 *************************************************************************
 * TFUCR Transmit FIFO Underrun Counter 
 * Saturates at 0xffff, reset to 0 by write of any value
 *************************************************************************
 */
static uint32_t
tfucr_read(void *clientData, uint32_t address, int rqlen)
{
	RxEth *re = clientData;
	return re->regTFUCR;
}

static void
tfucr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	RxEth *re = clientData;
	re->regTFUCR = 0;
}

/**
 *************************************************************************
 * RFOCR Receive FIFO Overflow Counter
 * Saturates at 0xffff and is reset to 0 by writing any value
 *************************************************************************
 */
static uint32_t
rfocr_read(void *clientData, uint32_t address, int rqlen)
{
	RxEth *re = clientData;
	return re->regRFOCR;
}

static void
rfocr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	RxEth *re = clientData;
	re->regRFOCR = 0;
}

/*
 ****************************************************************************
 * RBWAR Receive Buffer Write Address Register
 * Read only register for monitoring the reception.
 ****************************************************************************
 */
static uint32_t
rbwar_read(void *clientData, uint32_t address, int rqlen)
{
	RxEth *re = clientData;
	return re->regRBWAR;
}

static void
rbwar_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "Writing to readonly %s\n", __func__);
}

/*
 *****************************************************************************
 * RDFAR Receive Descriptor Fetch Address Register
 * Read only register for monitoring Receive descriptor fetches.
 *****************************************************************************
 */
static uint32_t
rdfar_read(void *clientData, uint32_t address, int rqlen)
{
	RxEth *re = clientData;
	return re->regRDFAR;;
}

static void
rdfar_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "Writing to readonly register: %s\n", __func__);
}

/*
 *****************************************************************************
 * TBRAR Transmit Buffer Read Address Register
 * Read only register for monitoring the transmit read address.
 *****************************************************************************
 */
static uint32_t
tbrar_read(void *clientData, uint32_t address, int rqlen)
{
	RxEth *re = clientData;
	return re->regTBRAR;
}

static void
tbrar_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "Writing to readonly register: %s\n", __func__);
}

/**
 *******************************************************************************
 * TDFAR Transmit Descriptor Fetch Address Register
 * Read only register for monitoring the access to transmit buffer descriptors
 *******************************************************************************
 */
static uint32_t
tdfar_read(void *clientData, uint32_t address, int rqlen)
{
	RxEth *re = clientData;
	return re->regTDFAR;;
}

static void
tdfar_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "Writing to readonly register: %s\n", __func__);
}

/**
 ****************************************************************************
 * FCFTR Flow Control Start FIFO Threshold Register
 ****************************************************************************
 */
static uint32_t
fcftr_read(void *clientData, uint32_t address, int rqlen)
{
	RxEth *re = clientData;
	return re->regFCFTR;
}

static void
fcftr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	RxEth *re = clientData;
	re->regFCFTR = value & 0x00070007;
}

/*
 ***************************************************************************
 * RPADIR Receive Data Insert Register
 ***************************************************************************
 */
static uint32_t
rpadir_read(void *clientData, uint32_t address, int rqlen)
{
	RxEth *re = clientData;
	return re->regRPADIR;
}

static void
rpadir_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	RxEth *re = clientData;
	re->regRPADIR = value & 0x0003003f;
	if (value & 0x00030000) {
		fprintf(stderr, "Error: Padding not implemented in %s\n", __func__);
		exit(1);
	}
}

/*
 ****************************************************************************
 * TRIMD Transmit Interrupt Setting Register
 * Bit 0: TIS Transmit Interrupt Setting
 * Bit 4: TIM Transmit Interrupt Mode
 ****************************************************************************
 */
static uint32_t
trimd_read(void *clientData, uint32_t address, int rqlen)
{
	RxEth *re = clientData;
	return re->regTRIMD;
}

static void
trimd_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	RxEth *re = clientData;
	re->regTRIMD = value;
}

/**
 ****************************************************************************
 * IOSR Independent Output Signal Setting Register
 * RX62N only
 ****************************************************************************
 */
static uint32_t
iosr_read(void *clientData, uint32_t address, int rqlen)
{
	return 0;;
}

static void
iosr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
}

static uint32_t
ecmr_read(void *clientData, uint32_t address, int rqlen)
{
	RxEth *re = clientData;
	return re->regECMR;;
}

static void
ecmr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	RxEth *re = clientData;
	re->regECMR = value;
}

static uint32_t
rflr_read(void *clientData, uint32_t address, int rqlen)
{
	RxEth *re = clientData;
	return re->regRFLR;
}

static void
rflr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	RxEth *re = clientData;
	re->regRFLR = value & 0x3fff;
}

static uint32_t
ecsr_read(void *clientData, uint32_t address, int rqlen)
{
	RxEth *re = clientData;
	return re->regECSR;
}

static void
ecsr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	RxEth *re = clientData;
	re->regECSR = re->regECSR & ~(value);
	update_eci_interrupt(re);
}

static uint32_t
ecsipr_read(void *clientData, uint32_t address, int rqlen)
{
	RxEth *re = clientData;
	return re->regECSIPR;
}

static void
ecsipr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	RxEth *re = clientData;
	re->regECSIPR = value & 0x37;
}

static uint32_t
pir_read(void *clientData, uint32_t address, int rqlen)
{
	RxEth *re = clientData;
	uint32_t mdc, mdi;
	if (SigNode_Val(re->sigMDC) == SIG_HIGH) {
		mdc = 1;
	} else {
		mdc = 0;
	}
	if (SigNode_Val(re->sigMDIO) == SIG_HIGH) {
		mdi = 1 << 3;
	} else {
		mdi = 0;
	}
	return (re->regPIR & ~0x9) | mdi | mdc;
}

static void
pir_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	RxEth *re = clientData;
	re->regPIR = value & ~0x9;
	if (value & PIR_MMD) {
		if (value & PIR_MDO) {
			SigNode_Set(re->sigMDIO, SIG_HIGH);
		} else {
			SigNode_Set(re->sigMDIO, SIG_LOW);
		}
	} else {
		SigNode_Set(re->sigMDIO, SIG_PULLUP);
	}
	/* Intenionally set the clock after setting the data */
	if (value & PIR_MDC) {
		SigNode_Set(re->sigMDC, SIG_HIGH);
	} else {
		SigNode_Set(re->sigMDC, SIG_LOW);
	}
}

static uint32_t
psr_read(void *clientData, uint32_t address, int rqlen)
{
	RxEth *re = clientData;
	if (SigNode_Val(re->sigLMON) == SIG_HIGH) {
		return 1;
	} else {
		return 0;
	}
}

static void
psr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "RxEth: Writing to readonly register PSR\n");
}

static uint32_t
rdmlr_read(void *clientData, uint32_t address, int rqlen)
{
	RxEth *re = clientData;
	return re->regRDMLR;
}

static void
rdmlr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	RxEth *re = clientData;
	re->regRDMLR = value & 0xfffff;
	if (value) {
		fprintf(stderr, "RxEth: Non standard value for RDMLR\n");
	}
}

static uint32_t
ipgr_read(void *clientData, uint32_t address, int rqlen)
{
	RxEth *re = clientData;
	return re->regIPGR;
}

static void
ipgr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	RxEth *re = clientData;
	re->regIPGR = value & 0x1f;
	if (re->regIPGR != 0x14) {
		fprintf(stderr, "RxEth: Non recommended value 0x%x in IPGR\n", value);
	}
}

static uint32_t
apr_read(void *clientData, uint32_t address, int rqlen)
{
	RxEth *re = clientData;
	return re->regAPR;
}

static void
apr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	RxEth *re = clientData;
	re->regAPR = value & 0xffff;
}

/**
 ******************************************************
 * Manual pause register
 ******************************************************
 */
static uint32_t
mpr_read(void *clientData, uint32_t address, int rqlen)
{
	RxEth *re = clientData;
	return re->regMPR;
}

static void
mpr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	RxEth *re = clientData;
	re->regMPR = value & 0xffff;
}

static uint32_t
rfcf_read(void *clientData, uint32_t address, int rqlen)
{
	RxEth *re = clientData;
	return re->regRFCF;
}

static void
rfcf_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	RxEth *re = clientData;
	re->regRFCF = value & 0xff;
}

static uint32_t
tpauser_read(void *clientData, uint32_t address, int rqlen)
{
	RxEth *re = clientData;
	return re->regTPAUSER;
}

static void
tpauser_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	RxEth *re = clientData;
	re->regTPAUSER = value & 0xffff;

}

static uint32_t
tpausecr_read(void *clientData, uint32_t address, int rqlen)
{
	RxEth *re = clientData;;
	return re->regTPAUSECR;
}

static void
tpausecr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "RxEth: Writing to readonly TPAUSECR\n");
}

static uint32_t
bcfrr_read(void *clientData, uint32_t address, int rqlen)
{
	RxEth *re = clientData;
	return re->regBCFRR;
}

static void
bcfrr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	RxEth *re = clientData;
	re->regBCFRR = value & 0xffff;
	if (value != 0) {
		fprintf(stderr, "RxEth: Warning: Broadcast limit not implemented\n");
	}
}

static uint32_t
mahr_read(void *clientData, uint32_t address, int rqlen)
{
	RxEth *re = clientData;
	uint32_t value = ((uint32_t) re->MAC[0] << 24) | ((uint32_t) re->MAC[1] << 16) |
	    ((uint32_t) re->MAC[2] << 8) | re->MAC[3];
	return value;
}

static void
mahr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	RxEth *re = clientData;
	re->MAC[0] = (value >> 24) & 0xff;
	re->MAC[1] = (value >> 16) & 0xff;
	re->MAC[2] = (value >> 8) & 0xff;
	re->MAC[3] = (value >> 0) & 0xff;
}

static uint32_t
malr_read(void *clientData, uint32_t address, int rqlen)
{
	RxEth *re = clientData;
	uint32_t value;
	value = ((uint32_t) re->MAC[4] << 8) | ((uint32_t) re->MAC[5]);
	return value;
}

static void
malr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	RxEth *re = clientData;
	re->MAC[4] = (value >> 8) & 0xff;
	re->MAC[5] = value & 0xff;
}

static uint32_t
trocr_read(void *clientData, uint32_t address, int rqlen)
{
	RxEth *re = clientData;
	return re->regTROCR;
}

static void
trocr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	RxEth *re = clientData;
	re->regTROCR = 0;
}

static uint32_t
cdcr_read(void *clientData, uint32_t address, int rqlen)
{
	RxEth *re = clientData;
	return re->regCDCR;
}

static void
cdcr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	RxEth *re = clientData;
	re->regCDCR = 0;
}

static uint32_t
lccr_read(void *clientData, uint32_t address, int rqlen)
{
	RxEth *re = clientData;
	return re->regLCCR;
}

static void
lccr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	RxEth *re = clientData;
	re->regLCCR = 0;
}

static uint32_t
cndcr_read(void *clientData, uint32_t address, int rqlen)
{
	RxEth *re = clientData;
	return re->regCNDCR;
}

static void
cndcr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	RxEth *re = clientData;
	re->regCNDCR = 0;
}

static uint32_t
cefcr_read(void *clientData, uint32_t address, int rqlen)
{
	RxEth *re = clientData;
	return re->regCEFCR;
}

static void
cefcr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	RxEth *re = clientData;
	re->regCEFCR = 0;
}

static uint32_t
frecr_read(void *clientData, uint32_t address, int rqlen)
{
	RxEth *re = clientData;
	return re->regFRECR;
}

static void
frecr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	RxEth *re = clientData;
	re->regFRECR = 0;
}

static uint32_t
tsfrcr_read(void *clientData, uint32_t address, int rqlen)
{
	RxEth *re = clientData;
	return re->regTSFRCR;
}

static void
tsfrcr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	RxEth *re = clientData;
	re->regTSFRCR = 0;
}

static uint32_t
tlfrcr_read(void *clientData, uint32_t address, int rqlen)
{
	RxEth *re = clientData;
	return re->regTLFRCR;
}

static void
tlfrcr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	RxEth *re = clientData;
	re->regTLFRCR = 0;

}

static uint32_t
rfcr_read(void *clientData, uint32_t address, int rqlen)
{
	RxEth *re = clientData;
	return re->regRFCR;
}

static void
rfcr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	RxEth *re = clientData;
	re->regRFCR = 0;
}

static uint32_t
mafcr_read(void *clientData, uint32_t address, int rqlen)
{
	RxEth *re = clientData;
	return re->regMAFCR;
}

static void
mafcr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	RxEth *re = clientData;
	re->regMAFCR = 0;
}

static void
RxEth_Unmap(void *owner, uint32_t base, uint32_t mask)
{
	/* First the DMA controller */
	IOH_Delete32(REG_EDMR(base));
	IOH_Delete32(REG_EDTRR(base));
	IOH_Delete32(REG_EDRRR(base));
	IOH_Delete32(REG_TDLAR(base));
	IOH_Delete32(REG_RDLAR(base));
	IOH_Delete32(REG_EESR(base));
	IOH_Delete32(REG_EESIPR(base));
	IOH_Delete32(REG_TRSCER(base));
	IOH_Delete32(REG_RMFCR(base));
	IOH_Delete32(REG_TFTR(base));
	IOH_Delete32(REG_FDR(base));
	IOH_Delete32(REG_RMCR(base));
	IOH_Delete32(REG_TFUCR(base));
	IOH_Delete32(REG_RFOCR(base));
	IOH_Delete32(REG_RBWAR(base));
	IOH_Delete32(REG_RDFAR(base));
	IOH_Delete32(REG_TBRAR(base));
	IOH_Delete32(REG_TDFAR(base));
	IOH_Delete32(REG_FCFTR(base));
	IOH_Delete32(REG_RPADIR(base));
	IOH_Delete32(REG_TRIMD(base));
	IOH_Delete32(REG_IOSR(base));
	/* Now the Ethernet controller */
	IOH_Delete32(REG_ECMR(base));
	IOH_Delete32(REG_RFLR(base));
	IOH_Delete32(REG_ECSR(base));
	IOH_Delete32(REG_ECSIPR(base));
	IOH_Delete32(REG_PIR(base));
	IOH_Delete32(REG_PSR(base));
	IOH_Delete32(REG_RDMLR(base));
	IOH_Delete32(REG_IPGR(base));
	IOH_Delete32(REG_APR(base));
	IOH_Delete32(REG_MPR(base));
	IOH_Delete32(REG_RFCF(base));
	IOH_Delete32(REG_TPAUSER(base));
	IOH_Delete32(REG_TPAUSECR(base));
	IOH_Delete32(REG_BCFRR(base));
	IOH_Delete32(REG_MAHR(base));
	IOH_Delete32(REG_MALR(base));
	IOH_Delete32(REG_TROCR(base));
	IOH_Delete32(REG_CDCR(base));
	IOH_Delete32(REG_LCCR(base));
	IOH_Delete32(REG_CNDCR(base));
	IOH_Delete32(REG_CEFCR(base));
	IOH_Delete32(REG_FRECR(base));
	IOH_Delete32(REG_TSFRCR(base));
	IOH_Delete32(REG_TLFRCR(base));
	IOH_Delete32(REG_RFCR(base));
	IOH_Delete32(REG_MAFCR(base));
}

static void
RxEth_Map(void *owner, uint32_t base, uint32_t mask, uint32_t mapflags)
{
	RxEth *re = owner;
	IOH_New32(REG_EDMR(base), edmr_read, edmr_write, re);
	IOH_New32(REG_EDTRR(base), edtrr_read, edtrr_write, re);
	IOH_New32(REG_EDRRR(base), edrrr_read, edrrr_write, re);
	IOH_New32(REG_TDLAR(base), tdlar_read, tdlar_write, re);
	IOH_New32(REG_RDLAR(base), rdlar_read, rdlar_write, re);
	IOH_New32(REG_EESR(base), eesr_read, eesr_write, re);
	IOH_New32(REG_EESIPR(base), eesipr_read, eesipr_write, re);
	IOH_New32(REG_TRSCER(base), trscer_read, trscer_write, re);
	IOH_New32(REG_RMFCR(base), rmfcr_read, rmfcr_write, re);
	IOH_New32(REG_TFTR(base), tftr_read, tftr_write, re);
	IOH_New32(REG_FDR(base), fdr_read, fdr_write, re);
	IOH_New32(REG_RMCR(base), rmcr_read, rmcr_write, re);
	IOH_New32(REG_TFUCR(base), tfucr_read, tfucr_write, re);
	IOH_New32(REG_RFOCR(base), rfocr_read, rfocr_write, re);
	IOH_New32(REG_RBWAR(base), rbwar_read, rbwar_write, re);
	IOH_New32(REG_RDFAR(base), rdfar_read, rdfar_write, re);
	IOH_New32(REG_TBRAR(base), tbrar_read, tbrar_write, re);
	IOH_New32(REG_TDFAR(base), tdfar_read, tdfar_write, re);
	IOH_New32(REG_FCFTR(base), fcftr_read, fcftr_write, re);
	IOH_New32(REG_RPADIR(base), rpadir_read, rpadir_write, re);
	IOH_New32(REG_TRIMD(base), trimd_read, trimd_write, re);
	IOH_New32(REG_IOSR(base), iosr_read, iosr_write, re);
	IOH_New32(REG_ECMR(base), ecmr_read, ecmr_write, re);
	IOH_New32(REG_RFLR(base), rflr_read, rflr_write, re);
	IOH_New32(REG_ECSR(base), ecsr_read, ecsr_write, re);
	IOH_New32(REG_ECSIPR(base), ecsipr_read, ecsipr_write, re);
	IOH_New32(REG_PIR(base), pir_read, pir_write, re);
	IOH_New32(REG_PSR(base), psr_read, psr_write, re);
	IOH_New32(REG_RDMLR(base), rdmlr_read, rdmlr_write, re);
	IOH_New32(REG_IPGR(base), ipgr_read, ipgr_write, re);
	IOH_New32(REG_APR(base), apr_read, apr_write, re);
	IOH_New32(REG_MPR(base), mpr_read, mpr_write, re);
	IOH_New32(REG_RFCF(base), rfcf_read, rfcf_write, re);
	IOH_New32(REG_TPAUSER(base), tpauser_read, tpauser_write, re);
	IOH_New32(REG_TPAUSECR(base), tpausecr_read, tpausecr_write, re);
	IOH_New32(REG_BCFRR(base), bcfrr_read, bcfrr_write, re);
	IOH_New32(REG_MAHR(base), mahr_read, mahr_write, re);
	IOH_New32(REG_MALR(base), malr_read, malr_write, re);
	IOH_New32(REG_TROCR(base), trocr_read, trocr_write, re);
	IOH_New32(REG_CDCR(base), cdcr_read, cdcr_write, re);
	IOH_New32(REG_LCCR(base), lccr_read, lccr_write, re);
	IOH_New32(REG_CNDCR(base), cndcr_read, cndcr_write, re);
	IOH_New32(REG_CEFCR(base), cefcr_read, cefcr_write, re);
	IOH_New32(REG_FRECR(base), frecr_read, frecr_write, re);
	IOH_New32(REG_TSFRCR(base), tsfrcr_read, tsfrcr_write, re);
	IOH_New32(REG_TLFRCR(base), tlfrcr_read, tlfrcr_write, re);
	IOH_New32(REG_RFCR(base), rfcr_read, rfcr_write, re);
	IOH_New32(REG_MAFCR(base), mafcr_read, mafcr_write, re);
}

BusDevice *
Rx62nEtherC_New(const char *name)
{
	RxEth *re = sg_new(RxEth);
	re->bdev.first_mapping = NULL;
	re->bdev.Map = RxEth_Map;
	re->bdev.UnMap = RxEth_Unmap;
	re->bdev.owner = re;
	re->bdev.hw_flags = MEM_FLAG_WRITABLE | MEM_FLAG_READABLE;
	re->ether_fd = Net_CreateInterface(name);
	if (re->ether_fd >= 0) {
		fcntl(re->ether_fd, F_SETFL, O_NONBLOCK);
	}
	CycleTimer_Init(&re->txTimer, transmit_timer_proc, re);
	re->sigIRQ = SigNode_New("%s.irq", name);
	re->sigMDC = SigNode_New("%s.mdc", name);
	re->sigMDIO = SigNode_New("%s.mdio", name);
	re->sigLMON = SigNode_New("%s.lmon", name);
	if (!re->sigIRQ || !re->sigMDC || !re->sigMDIO || !re->sigLMON) {
		fprintf(stderr, "Can not create a signal line for %s\n", name);
		exit(1);
	}
	re->rxdropPerc = re->txdropPerc = 0;
	Config_ReadUInt32(&re->rxdropPerc, name, "drop");
	Config_ReadUInt32(&re->txdropPerc, name, "drop");
	Config_ReadUInt32(&re->rxdropPerc, name, "rxdrop");
	Config_ReadUInt32(&re->txdropPerc, name, "txdrop");

	//CycleTimer_Init(&re->rxTimer, receive_timer_proc,re);
	/* EDMAC register init */
	re->regTDLAR = 0;
	re->regRMFCR = 0;
	re->regTFUCR = 0;
	re->regRFOCR = 0;
	edmac_reset(re);
	/* Ethernet controller register init */
	re->regECMR = 0;
	re->regRFLR = 0;
	re->regECSR = 0;
	re->regECSIPR = 0;
	re->regPIR = 0;
	re->regPSR = 0;
	re->regRDMLR = 0;
	re->regIPGR = 0x00000014;
	re->regAPR = 0;
	re->regMPR = 0;
	re->regRFCF = 0;
	re->regTPAUSER = 0;
	re->regTPAUSECR = 0;
	re->regBCFRR = 0;
	memset(re->MAC, 0, 6);
	re->regTROCR = 0;
	re->regCDCR = 0;
	re->regLCCR = 0;
	re->regCNDCR = 0;
	re->regCEFCR = 0;
	re->regFRECR = 0;
	re->regTSFRCR = 0;
	re->regTLFRCR = 0;
	re->regRFCR = 0;
	re->regMAFCR = 0;
	update_interrupt(re);
	return &re->bdev;
}
