/*
 *************************************************************************************************
 *
 * Emulation of the AT91 Ethernet Controller (EMACB)
 *
 * State: working 
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
#include "at91_emacb.h"

// include system header
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

// include library header

// include user header
#include "bus.h"
#include "devices/phy/phy.h"
#include "signode.h"
#include "cycletimer.h"
#include "sgstring.h"
#include "crc32.h"
#include "linux-tap.h"

#include "asyncmanager.h"


#if 0
#define dbgprintf(...) { fprintf(stderr,__VA_ARGS__); }
#else
#define dbgprintf(...)
#endif

#define EMAC_NCR(base)	((base) + 0)
#define		NCR_THALT	(1 << 10)
#define 	NCR_TSTART	(1 << 9)
#define		NCR_BP		(1 << 8)
#define		NCR_WESTAT	(1 << 7)
#define		NCR_INCSTAT	(1 << 6)
#define		NCR_CLRSTAT	(1 << 5)
#define		NCR_MPE		(1 << 4)
#define		NCR_TE		(1 << 3)
#define		NCR_RE		(1 << 2)
#define		NCR_LLB		(1 << 1)
#define		NCR_LB		(1 << 0)
#define EMAC_NCFG(base)	((base) + 0x4)
#define		NCFG_IRXFCS	(1 << 19)
#define		NCFG_EFRHD	(1 << 18)
#define		NCFG_DRFCS	(1 << 17)
#define 	NCFG_RLCE	(1 << 16)
#define 	NCFG_RBOF_MASK	(3 << 14)
#define 	NCFG_RBOF_SHIFT	(14)
#define		NCFG_PAE	(1 << 13)
#define		NCFG_RTY	(1 << 12)
#define		NCFG_CLK_MASK	(3 << 10)
#define		NCFG_CLK_SHIFT	(10)
#define		NCFG_BIG	(1 << 8)
#define		NCFG_UNI	(1 << 7)
#define		NCFG_MTI	(1 << 6)
#define		NCFG_NBC	(1 << 5)
#define		NCFG_CAF	(1 << 4)
#define		NCFG_JFRAME	(1 << 3)
#define		NCFG_FD		(1 << 1)
#define		NCFG_SPD	(1 << 0)

#define EMAC_NSR(base)	((base) + 0x8)
#define		NSR_IDLE	(1 << 2)
#define		NSR_MDIO	(1 << 1)

#define EMAC_TSR(base)	((base) + 0x14)
#define		TSR_UND		(1 <<6)
#define		TSR_COMP	(1 << 5)
#define 	TSR_BEX		(1 << 4)
#define		TSR_TGO		(1 << 3)
#define		TSR_RLE		(1 << 2)
#define		TSR_COL		(1 << 1)
#define		TSR_UBR		(1 << 0)

#define EMAC_RBQP(base)		((base) + 0x18)
#define EMAC_TBQP(base)		((base) + 0x1c)
#define EMAC_RSR(base)		((base) + 0x20)
#define		RSR_OVR		(1<<2)
#define		RSR_REC		(1<<1)
#define		RSR_BNA		(1<<0)

#define EMAC_ISR(base)	((base) + 0x24)
#define		ISR_PFR		(1 << 12)
#define		ISR_HRESP	(1 << 11)
#define		ISR_ROVR	(1 << 10)
#define		ISR_TCOMP	(1 << 7)
#define		ISR_RXERR	(1 << 6)
#define		ISR_RLE		(1 << 5)
#define		ISR_TUND	(1 << 4)
#define		ISR_TXUBR	(1 << 3)
#define		ISR_RXUBR	(1 << 2)
#define 	ISR_RCOMP	(1 << 1)
#define		ISR_MFD		(1 << 0)

#define EMAC_IER(base)	((base) + 0x28)
#define		IER_PTZ		(1 << 12)
#define		IER_HRESP	(1 << 11)
#define		IER_ROVR	(1 << 10)
#define		IER_TCOMP	(1 << 7)
#define		IER_TXERR	(1 << 6)
#define		IER_RLE		(1 << 5)
#define		IER_TUND	(1 << 4)
#define		IER_TXUBR	(1 << 3)
#define		IER_RXUBR	(1 << 2)
#define 	IER_RCOMP	(1 << 1)
#define		IER_MFD		(1 << 0)

#define EMAC_IDR(base)	((base) + 0x2c)
#define		IDR_PTZ		(1 << 12)
#define		IDR_HRESP	(1 << 11)
#define		IDR_ROVR	(1 << 10)
#define		IDR_TCOMP	(1 << 7)
#define		IDR_TXERR	(1 << 6)
#define		IDR_RLE		(1 << 5)
#define		IDR_TUND	(1 << 4)
#define		IDR_TXUBR	(1 << 3)
#define		IDR_RXUBR	(1 << 2)
#define 	IDR_RCOMP	(1 << 1)
#define		IDR_MFD		(1 << 0)

#define EMAC_IMR(base)	((base) + 0x30)
#define		IMR_PTZ		(1 << 12)
#define		IMR_HRESP	(1 << 11)
#define		IMR_ROVR	(1 << 10)
#define		IMR_TCOMP	(1 << 7)
#define		IMR_TXERR	(1 << 6)
#define		IMR_RLE		(1 << 5)
#define		IMR_TUND	(1 << 4)
#define		IMR_TXUBR	(1 << 3)
#define		IMR_RXUBR	(1 << 2)
#define 	IMR_RCOMP	(1 << 1)
#define		IMR_MFD		(1 << 0)

#define EMAC_MAN(base)	((base) + 0x34)
#define		MAN_SOF_MASK		(3 << 30)
#define		MAN_SOF_SHIFT		(30)
#define		MAN_RW_MASK		(3 << 28)
#define		MAN_RW_SHIFT		(28)
#define			MAN_READ	(2<<28)
#define			MAN_WRITE	(1<<28)
#define		MAN_PHYA_MASK	(0x1f << 23)
#define		MAN_PHYA_SHIFT	(23)
#define		MAN_REGA_MASK	(0x1f << 18)
#define		MAN_REGA_SHIFT	(18)
#define		MAN_CODE_MASK	(3 << 16)
#define		MAN_CODE_SHIFT	(16)
#define		MAN_DATA_MASK	(0xffff)

#define EMAC_PTR(base)	((base) + 0x38)
#define EMAC_PFR(base)	((base) + 0x3c)
#define EMAC_FTO(base)	((base) + 0x40)
#define EMAC_SCF(base)	((base) + 0x44)
#define EMAC_MCF(base)	((base) + 0x48)
#define EMAC_FRO(base)	((base) + 0x4c)
#define EMAC_FCSE(base)	((base) + 0x50)
#define EMAC_ALE(base)	((base) + 0x54)
#define EMAC_DTF(base)	((base) + 0x58)
#define EMAC_LCOL(base)	((base) + 0x5c)
#define EMAC_ECOL(base)	((base) + 0x60)
#define EMAC_TUND(base)	((base) + 0x64)
#define EMAC_CSE(base)	((base) + 0x68)
#define EMAC_RRE(base)	((base) + 0x6c)
#define EMAC_ROV(base)	((base) + 0x70)
#define EMAC_RSE(base)	((base) + 0x74)
#define EMAC_ELE(base)	((base) + 0x78)
#define EMAC_RJA(base)	((base) + 0x7c)
#define EMAC_USF(base)	((base) + 0x80)
#define EMAC_STE(base)	((base) + 0x84)
#define EMAC_RLE(base)	((base) + 0x88)
#define	EMAC_HRB(base)	((base) + 0x90)
#define	EMAC_HRT(base)	((base) + 0x94)
#define	EMAC_SA1B(base)	((base) + 0x98)
#define EMAC_SA1T(base)	((base) + 0x9c)
#define	EMAC_SA2B(base)	((base) + 0xa0)
#define	EMAC_SA2T(base)	((base) + 0xa4)
#define	EMAC_SA3B(base)	((base) + 0xa8)
#define	EMAC_SA3T(base)	((base) + 0xac)
#define	EMAC_SA4B(base)	((base) + 0xb0)
#define	EMAC_SA4T(base)	((base) + 0xb4)
#define EMAC_TID(base)	((base) + 0xb8)
#define EMAC_USRIO(base) ((base) + 0xc0)
#define		USRIO_RMII	(1 << 0)
#define		USRIO_CLKEN	(1 << 1)

/* Address match bitfield definition for Receive buffer descriptor */
#define	RBDE_AM_BROADCAST	(1<<31)	/* all ones   */
#define RBDE_AM_MULTICAST	(1<<30)
#define RBDE_AM_UNICAST		(1<<29)	/* Hash match, not local address match */
#define	RBDE_AM_EXTADDR		(1<<28)
#define RBDE_AM_RSRVD		(1<<27)
#define	RBDE_AM_SA1		(1<<26)
#define	RBDE_AM_SA2		(1<<25)
#define	RBDE_AM_SA3		(1<<24)
#define	RBDE_AM_SA4		(1<<23)
#define	RBDE_AM_TYPEID		(1 << 22)
#define	RBDE_VLANTAG		(1 << 21)
#define RBDE_PRIOTAG		(1 << 20)
#define	RBDE_VLANPRIO_MSK	(3 << 17)
#define	RBDE_CFI		(1 << 16)
#define RBDE_EOF	(1 << 15)
#define RBDE_SOF	(1 << 14)

#define TBDE_USED	(1 << 31)
#define TBDE_WRAP	(1 << 30)
#define TBDE_RETRY_EX	(1 << 29)
#define TBDE_UNDERRUN	(1 << 28)
#define TBDE_EXHAUSTED	(1 << 27)
#define TBDE_NOCRC	(1 << 16)
#define TBDE_LASTBUF	(1 << 15)
#define TBDE_LEN_MSK	(0x7FF)

#define MAX_PHYS	(32)
typedef struct AT91Emacb {
	BusDevice bdev;
	int ether_fd;
	PollHandle_t *input_fh;
	int receiver_is_enabled;
	PHY_Device *phy[MAX_PHYS];
	CycleTimer rcvDelayTimer;
	CycleTimer txDelayTimer;
	SigNode *irqNode;
	uint32_t regNCR;
	uint32_t regNCFG;
	uint32_t regNSR;
	uint32_t regTSR;
	uint32_t regRBQP;
	uint32_t rbdscr_offs;
	uint32_t regTBQP;
	uint32_t tbdscr_offs;
	uint32_t regRSR;
	uint32_t regISR;
	uint32_t regIER;
	uint32_t regIDR;
	uint32_t regIMR;
	uint32_t regMAN;
	uint32_t regPTR;
	uint32_t regPFR;
	uint32_t regFTO;
	uint32_t regSCF;
	uint32_t regMCF;
	uint32_t regFRO;
	uint32_t regFCSE;
	uint32_t regALE;
	uint32_t regDTF;
	uint32_t regLCOL;
	uint32_t regECOL;
	uint32_t regTUND;
	uint32_t regCSE;
	uint32_t regRRE;
	uint32_t regROVR;
	uint32_t regRSE;
	uint32_t regELE;
	uint32_t regRJA;
	uint32_t regUSF;
	uint32_t regSTE;
	uint32_t regRLE;
	uint64_t regHASH;
	uint32_t pkt_matchmask;
	uint8_t regSA1[6];
	uint8_t regSA2[6];
	uint8_t regSA3[6];
	uint8_t regSA4[6];
	uint8_t regTID[2];
	uint32_t regUSRIO;
} AT91Emacb;

static void enable_receiver(AT91Emacb * emac);
static void disable_receiver(AT91Emacb * emac);

static void
update_interrupt(AT91Emacb * emac)
{
	if (emac->regISR & (~emac->regIMR) & 0x3Cff) {
		SigNode_Set(emac->irqNode, SIG_HIGH);
	} else {
		SigNode_Set(emac->irqNode, SIG_PULLDOWN);
	}
}

static void
update_receiver_status(AT91Emacb * emac)
{
	if (!(emac->regNCR & NCR_RE)) {
		disable_receiver(emac);
		return;
	} else {
		/* should do some overrun check here */
		if (!CycleTimer_IsActive(&emac->rcvDelayTimer)) {
			enable_receiver(emac);
		}
	}
}

/**
 **************************************************************
 * \fn static void transmit_pkt(AT91Emacb *emac) 
 * Read a packet from memory and transmit it. It is
 * stored in up to 128 fragments.
 **************************************************************
 */
static void
transmit_pkt(AT91Emacb * emac)
{
	uint32_t descr_addr;
	uint32_t tba;
	uint32_t tbstat;
	uint32_t buflen;
	uint32_t i;
	uint8_t pkt[2048];
	uint32_t pktsize = 0;
	for (i = 0; i < 128; i++) {
		descr_addr = emac->regTBQP | emac->tbdscr_offs;
		tba = Bus_Read32(descr_addr);
		tbstat = Bus_Read32(descr_addr | 4);
		if ((tbstat & TBDE_USED) != 0) {
			emac->regTSR &= ~TSR_TGO;
			emac->regTSR |= TSR_UBR;
			emac->regISR |= ISR_TXUBR;
			update_interrupt(emac);
			break;
		}
		if (tbstat & TBDE_WRAP) {
			emac->tbdscr_offs = 0;
		} else {
			emac->tbdscr_offs = (emac->tbdscr_offs + 8) & ((1024 << 3) - 1);
		}
		buflen = tbstat & 2047;
		if ((buflen + pktsize) > sizeof(pkt)) {
			fprintf(stderr, "pkt to large\n");
			// Set some error flag ?
			emac->regISR |= ISR_TUND;
			emac->regTSR |= TSR_UND;
			update_interrupt(emac);
			break;
		}
		Bus_Read(pkt + pktsize, tba, buflen);
		pktsize += buflen;
		if (i == 0) {
			/* Wrong ! Should be done not before the frame is transmitted */
			Bus_Write32(tbstat | TBDE_USED, descr_addr | 4);
		}
		if (tbstat & TBDE_LASTBUF) {
			if (pktsize < 60) {
				memset(pkt + pktsize, 0x00, 60 - pktsize);
				pktsize = 60;
			}
			if (write(emac->ether_fd, pkt, pktsize) == pktsize) {
				//emac->fra++;
				emac->regTSR |= TSR_COMP;
				emac->regISR |= ISR_TCOMP;
				update_interrupt(emac);
			}
			CycleTimer_Mod(&emac->txDelayTimer, NanosecondsToCycles(100 * pktsize));
			return;
		}
	}
}

/**
 ************************************************************************************
 * Update the transmitter status according to the contents of the NCR register.
 ************************************************************************************
 */
static void
update_transmitter_status(AT91Emacb * emac)
{
	if (emac->regNCR & NCR_TE) {
		if (emac->regNCR & NCR_TSTART) {
			emac->regNCR &= ~NCR_TSTART;
			if (!CycleTimer_IsActive(&emac->txDelayTimer)) {
				CycleTimer_Mod(&emac->txDelayTimer, NanosecondsToCycles(100 * 128));
			}
		}
	} else {
		emac->tbdscr_offs = 0;
	}
	if (!(emac->regNCR & NCR_TE) || (emac->regNCR & NCR_THALT)) {
		emac->regNCR &= ~NCR_THALT;
		if (CycleTimer_IsActive(&emac->txDelayTimer)) {
			CycleTimer_Remove(&emac->txDelayTimer);
		}
	}
}

static void
tx_delay_done(void *clientData)
{
	AT91Emacb *emac = (AT91Emacb *) clientData;
	transmit_pkt(emac);
}

static void
rcv_delay_done(void *clientData)
{
	AT91Emacb *emac = (AT91Emacb *) clientData;
	update_receiver_status(emac);
}

static inline int
is_broadcast(uint8_t * mac)
{
	uint8_t broadcast[] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };
	return (memcmp(mac, broadcast, 6) == 0);
}

/**
 *************************************************************************************
 * \fn static uint8_t hash_index(uint8_t *daddr) 
 * calculate a 6 Bit hash value which is used as Bit index in the 64 bit hash
 * register.
 * Not tested !
 *************************************************************************************
 */
static uint8_t
hash_index(uint8_t * daddr)
{
	uint8_t hash_index = daddr[0] & 0x3f;
	hash_index ^= ((daddr[0] >> 6) & 3) | ((daddr[1] << 2) & 0x3c);
	hash_index ^= ((daddr[1] >> 4) & 0xf) | ((daddr[2] << 4) & 0x30);
	hash_index ^= ((daddr[2] >> 2) & 0x3f);
	hash_index ^= daddr[3] & 0x3f;
	hash_index ^= ((daddr[3] >> 6) & 3) | ((daddr[4] << 2) & 0x3c);
	hash_index ^= ((daddr[4] >> 4) & 0xf) | ((daddr[5] << 4) & 0x30);
	hash_index ^= (daddr[5] >> 2) & 0x3f;
	return hash_index;
}

static inline int
match_hash(AT91Emacb * emac, uint8_t * pkt)
{
	return (emac->regHASH >> hash_index(pkt)) & 1;
}

static inline int
match_type_id(AT91Emacb * emac, uint8_t * pkt)
{
	if (memcmp(pkt + 13, emac->regTID, 2) == 0) {
		return 1;
	} else {
		return 0;
	}
}

/**
 **********************************************************************************************
 * \fn static uint32_t match_address(AT91Emacb *emac,uint8_t *packet) 
 * Calculate the match flags for the address match filter and the Receive buffer descriptor
 * status bytes by analyzing the destination address in the first bytes of the packet.
 **********************************************************************************************
 */
static uint32_t
match_address(AT91Emacb * emac, uint8_t * packet)
{
	uint32_t matchflags = 0;
	if (is_broadcast(packet)) {
		matchflags |= RBDE_AM_BROADCAST;
	}
	if (match_hash(emac, packet)) {
		if (packet[0] & 1) {
			matchflags |= RBDE_AM_MULTICAST;
		} else {
			matchflags |= RBDE_AM_UNICAST;
		}
	}
	if (memcmp(packet, emac->regSA1, 6) == 0) {
		matchflags |= RBDE_AM_SA1;
	}
	if (memcmp(packet, emac->regSA2, 6) == 0) {
		matchflags |= RBDE_AM_SA2;
	}
	if (memcmp(packet, emac->regSA3, 6) == 0) {
		matchflags |= RBDE_AM_SA3;
	}
	if (memcmp(packet, emac->regSA4, 6) == 0) {
		matchflags |= RBDE_AM_SA4;
	}
	if (match_type_id(emac, packet)) {
		matchflags |= RBDE_AM_TYPEID;
	}
	return matchflags;
}

/*
 * Write a packet to memory
 */
#define RBF_OWNER	(1<<0)	/* 1 = software owned */
#define RBF_WRAP	(1<<1)
#define RBF_ADDR	(0xfffffffc)

/**
 ****************************************************************************************
 * \fn void dma_write_packet(AT91Emacb *emac,uint8_t *pkt,int count,uint32_t matchflags)
 * Write a received packet to the system memory by dma. Split into packets of 
 * 128 bytes.
 ****************************************************************************************
 */
static void
dma_write_packet(AT91Emacb * emac, uint8_t * pkt, unsigned int count, uint32_t matchflags)
{
	uint32_t rba, rbstat;
	uint32_t wrlen;
	uint32_t wrcnt;
	uint32_t i;
	uint32_t rbof;
	/* Fetch receive buffer descriptor, RM9200 manual says the counter is ored ! */
	uint32_t descr_addr;
	rbof = (emac->regNCFG & NCFG_RBOF_MASK) >> NCFG_RBOF_SHIFT;
	for (i = 0, wrcnt = 0; count > 0; i++) {
		descr_addr = emac->regRBQP | emac->rbdscr_offs;
		dbgprintf("dma_write_packet: desc_addr %08x\n", descr_addr);
		rba = Bus_Read32(descr_addr);
		rbstat = Bus_Read32(descr_addr | 4);
		if (rba & RBF_OWNER) {
			dbgprintf("AT91Emacb: Receive buffer not available\n");
			emac->regISR |= ISR_RXUBR;
			emac->regRSR |= RSR_BNA;
			update_interrupt(emac);
			return;
		}
		if (i == 0) {
			rbstat = matchflags | RBDE_SOF;
		} else {
			rbstat = matchflags;
		}
		if (count > 128) {
			wrlen = 128;
		} else {
			wrlen = count;
			rbstat |= RBDE_EOF;
		}
		if ((wrlen + rbof) > 128) {
			wrlen = 128 - rbof;
		}
		Bus_Write((rba & RBF_ADDR) + rbof, pkt + wrcnt, wrlen);
		rbof = 0;	/* rbof is cleared after first fragment */
#if 0
		fprintf(stderr, "rba %08x rbstat %08x written %d bytes to %08x\n", rba, rbstat,
			wrlen, (rba & RBF_ADDR) + wrcnt);
		for (j = 0; j < 16; j++) {
			fprintf(stderr, "%02x ", pkt[wrcnt + j]);
		}
		fprintf(stderr, "\n");
#endif
		Bus_Write32(rba | RBF_OWNER, descr_addr);
		/* only the last one contains the full status ? */
		Bus_Write32(rbstat | (wrcnt + wrlen), descr_addr + 4);
		if (rba & RBF_WRAP) {
			dbgprintf("receive buffer WRAP\n");
			emac->rbdscr_offs = 0;
		} else {
			emac->rbdscr_offs = (emac->rbdscr_offs + 8) & ((1024 << 3) - 1);
		}
		count -= wrlen;
		wrcnt += wrlen;
	}
}

/*
 **************************************************************
 * static int input_event(void *cd,int mask)
 * Event handler eating received pakets.
 **************************************************************
 */
static void
input_event(PollHandle_t *handle, int status, int events, void *clientdata)
{
	AT91Emacb *emac = clientdata;
	int result;
	int dmasize;
	uint8_t buf[1536];
	uint8_t *crcbuf;
	uint32_t crc;
	uint32_t matchflags, match;
	while (emac->receiver_is_enabled) {
		result = read(emac->ether_fd, buf, 1532);
		if (result > 0) {
			if (result < 60) {
				/* PAD with 0 */
				memset(buf + result, 0, 60 - result);
				dmasize = 60;
			} else {
				dmasize = result;
			}
			if (!(emac->regNCFG & NCFG_DRFCS)) {
				crcbuf = buf + result;
				crc = EthernetCrc(0, buf, dmasize);
				crcbuf[0] = crc;
				crcbuf[1] = crc >> 8;
				crcbuf[2] = (crc >> 16);
				crcbuf[3] = crc >> 24;
				dmasize += 4;
			}
			matchflags = match_address(emac, buf);
			match = matchflags & emac->pkt_matchmask;
			if (!match && !(emac->regNCFG & NCFG_CAF)) {
				continue;
			}
			if ((result > (1522 - 4)) && !(emac->regNCFG & NCFG_BIG)) {
				dbgprintf("Discarding big packet: size %u\n", result);
				continue;
			}
			dma_write_packet(emac, buf, dmasize, matchflags);
			emac->regISR |= ISR_RCOMP;
			emac->regRSR |= RSR_REC;
			emac->regFRO++;
			update_interrupt(emac);
			disable_receiver(emac);
			CycleTimer_Mod(&emac->rcvDelayTimer, NanosecondsToCycles(result * 100));
			break;
		} else {
			return;
		}
	}
	return;
}

static void
enable_receiver(AT91Emacb * emac)
{
	if (!emac->receiver_is_enabled && (emac->ether_fd >= 0)) {
		dbgprintf("AT91Emacb: enable receiver\n");
		AsyncManager_PollStart(emac->input_fh, ASYNCMANAGER_EVENT_READABLE, &input_event, emac);
		emac->receiver_is_enabled = 1;
	}
}

static void
disable_receiver(AT91Emacb * emac)
{
	if (emac->receiver_is_enabled) {
		dbgprintf("AT91Emacb: disable receiver\n");
		AsyncManager_PollStop(emac->input_fh);
		emac->receiver_is_enabled = 0;
	}
}

static void
statistics_clear(AT91Emacb * emac)
{
	emac->regPFR = 0;
	emac->regFTO = 0;
	emac->regSCF = 0;
	emac->regMCF = 0;
	emac->regFRO = 0;
	emac->regFCSE = 0;
	emac->regALE = 0;
	emac->regDTF = 0;
	emac->regLCOL = 0;
	emac->regECOL = 0;
	emac->regTUND = 0;
	emac->regCSE = 0;
	emac->regRRE = 0;
	emac->regROVR = 0;
	emac->regRSE = 0;
	emac->regELE = 0;
	emac->regRJA = 0;
	emac->regUSF = 0;
	emac->regSTE = 0;
	emac->regRLE = 0;
}

static void
statistics_inc(AT91Emacb * emac)
{
	emac->regPFR++;
	emac->regFTO++;
	emac->regSCF++;
	emac->regMCF++;
	emac->regFRO++;
	emac->regFCSE++;
	emac->regALE++;
	emac->regDTF++;
	emac->regLCOL++;
	emac->regECOL++;
	emac->regTUND++;
	emac->regCSE++;
	emac->regRRE++;
	emac->regROVR++;
	emac->regRSE++;
	emac->regELE++;
	emac->regRJA++;
	emac->regUSF++;
	emac->regSTE++;
	emac->regRLE++;
}

static uint32_t
ncr_read(void *clientData, uint32_t address, int rqlen)
{
	AT91Emacb *emac = (AT91Emacb *) clientData;
	return emac->regNCR;
}

static void
ncr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	AT91Emacb *emac = (AT91Emacb *) clientData;
	emac->regNCR = value & 0x7ff;
	if (value & NCR_TSTART) {
		emac->regTSR |= TSR_TGO;
	}
	if (value & NCR_CLRSTAT) {
		statistics_clear(emac);
	}
	if (value & NCR_INCSTAT) {
		statistics_inc(emac);
	}
	update_receiver_status(emac);
	update_transmitter_status(emac);
}

/*
 ********************************************************************************************
 * NCFG register
 * Bit 19: IRXFCS Ignore RX FCS/CRC
 * Bit 18: EFRHD Enable receiving frames in half duplex mode while transmitting
 * Bit 17: DRFCS When set the FCS field is not copied to memory
 * Bit 16: RLCE When set the receive length is checked and discarded if wrong	
 * Bit 14: RBOF Receive buffer offset
 * Bit 13: PAE When set the transmitter pauses when a pause frame is received
 * Bit 12: RTY Retry test
 * Bit 10 - 11: CLK (MDC is MCK divided by 8,16,32 or 64)
 * Bit 8: BIG Receive frames up to 1536 bytes
 * Bit 7: UNI Receive Unicast hash packets	
 * Bit 6: MTI Receive multicast hash packets
 * Bit 5: NBC Do not receive broadcast packets
 * Bit 4: CAF Copy all frames	
 * Bit 3: JFRAME Enable jumbo frames
 * Bit 1: FD Full duplex
 * Bit 0: SPD 1 = 100MBit, 0 = 10MBit	
 *******************************************************************************************
 */

static uint32_t
ncfg_read(void *clientData, uint32_t address, int rqlen)
{
	AT91Emacb *emac = (AT91Emacb *) clientData;
	return emac->regNCFG;
}

static void
ncfg_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	AT91Emacb *emac = (AT91Emacb *) clientData;
	emac->regNCFG = value & 0x000FFDFB;
	if (value & NCFG_NBC) {
		emac->pkt_matchmask &= ~RBDE_AM_BROADCAST;
	} else {
		emac->pkt_matchmask |= RBDE_AM_BROADCAST;
	}
	if (value & NCFG_MTI) {
		emac->pkt_matchmask |= RBDE_AM_MULTICAST;
	} else {
		emac->pkt_matchmask &= ~RBDE_AM_MULTICAST;
	}
	if (value & NCFG_UNI) {
		emac->pkt_matchmask |= RBDE_AM_UNICAST;
	} else {
		emac->pkt_matchmask &= ~RBDE_AM_UNICAST;
	}
}

static uint32_t
nsr_read(void *clientData, uint32_t address, int rqlen)
{
	AT91Emacb *emac = (AT91Emacb *) clientData;
	return emac->regNSR;
}

static void
nsr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "emac: Status register is readonly\n");
}

static uint32_t
tsr_read(void *clientData, uint32_t address, int rqlen)
{
	AT91Emacb *emac = (AT91Emacb *) clientData;
	return emac->regTSR;
}

static void
tsr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	AT91Emacb *emac = (AT91Emacb *) clientData;
	uint32_t clearmask = value & (TSR_UBR | TSR_COL | TSR_RLE | TSR_BEX | TSR_COMP | TSR_UND);
	emac->regTSR &= ~clearmask;
}

static uint32_t
rbqp_read(void *clientData, uint32_t address, int rqlen)
{
	AT91Emacb *emac = (AT91Emacb *) clientData;
	return emac->regRBQP;
}

static void
rbqp_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	AT91Emacb *emac = (AT91Emacb *) clientData;
	emac->regRBQP = value & ~UINT32_C(3);
	emac->rbdscr_offs = 0;	/* ?????? When is this really reset */
}

static uint32_t
tbqp_read(void *clientData, uint32_t address, int rqlen)
{
	AT91Emacb *emac = (AT91Emacb *) clientData;
	return emac->regTBQP + emac->rbdscr_offs;
}

static void
tbqp_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	AT91Emacb *emac = (AT91Emacb *) clientData;
	if (emac->regTSR & TSR_TGO) {
		fprintf(stderr, "emac: TBQP can not be written while TSR_TGO\n");
	} else {
		emac->regTBQP = value & ~UINT32_C(3);
		emac->tbdscr_offs = 0;
	}
}

static uint32_t
rsr_read(void *clientData, uint32_t address, int rqlen)
{
	AT91Emacb *emac = (AT91Emacb *) clientData;
	return emac->regRSR;
}

static void
rsr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	AT91Emacb *emac = (AT91Emacb *) clientData;
	emac->regRSR &= ~(value & 7);

}

static uint32_t
isr_read(void *clientData, uint32_t address, int rqlen)
{
	AT91Emacb *emac = (AT91Emacb *) clientData;
	uint32_t isr = emac->regISR;
	emac->regISR = 0;
	update_interrupt(emac);
	return isr;
}

static void
isr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "emac: what does write isr ? \n");
}

static uint32_t
ier_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "emac: IER register is write only\n");
	return 0;
}

static void
ier_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	AT91Emacb *emac = (AT91Emacb *) clientData;
	emac->regIMR &= ~(value & 0x3cff);
	update_interrupt(emac);
}

static uint32_t
idr_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "emac: IDR register is write only\n");
	return 0;
}

static void
idr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	AT91Emacb *emac = (AT91Emacb *) clientData;
	emac->regIMR |= (value & 0x3cff);
	update_interrupt(emac);
}

static uint32_t
imr_read(void *clientData, uint32_t address, int rqlen)
{
	AT91Emacb *emac = (AT91Emacb *) clientData;
	return emac->regIMR;
}

static void
imr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "IMR is not writable\n");
}

static uint32_t
man_read(void *clientData, uint32_t address, int rqlen)
{
	AT91Emacb *emac = (AT91Emacb *) clientData;
	return emac->regMAN;
}

static void
man_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	AT91Emacb *emac = (AT91Emacb *) clientData;
	int read;
	int phya;
	int rega;
	PHY_Device *phy;
	uint16_t result;

	if (!(emac->regNCR & NCR_MPE)) {
		fprintf(stderr, "AT91Emacb: Phy management port is disabled\n");
		return;
	}
	if ((value & MAN_SOF_MASK) != (1 << MAN_SOF_SHIFT)) {
		fprintf(stderr, "AT91Emacb: MAN SOF\n");
		return;
	}
	if ((value & MAN_CODE_MASK) != (2 << MAN_CODE_SHIFT)) {
		fprintf(stderr, "AT91Emacb: MAN code error\n");
		return;
	}
	if ((value & MAN_RW_MASK) == MAN_READ) {
		read = 1;
	} else if ((value & MAN_RW_MASK) == MAN_WRITE) {
		read = 0;
	} else {
		fprintf(stderr, "AT91Emacb: MAN neither read nor write\n");
		return;
	}
	phya = (value & MAN_PHYA_MASK) >> MAN_PHYA_SHIFT;
	rega = (value & MAN_REGA_MASK) >> MAN_REGA_SHIFT;
	phy = emac->phy[phya];
	if (!phy) {
		return;
	}
	/* Should be delayed by a clock */
	if (read) {
		PHY_ReadRegister(phy, &result, rega);
		emac->regMAN = result | (value & 0xffff0000);
	} else {
		PHY_WriteRegister(phy, value & 0xffff, rega);
		emac->regMAN = value;	/* does this shift out and read 0 of 0xffff ? */
	}
}

/**
 ************************************************************
 * Pause time register.
 ************************************************************
 */
static uint32_t
ptr_read(void *clientData, uint32_t address, int rqlen)
{
	AT91Emacb *emac = (AT91Emacb *) clientData;
	return emac->regPTR;
}

static void
ptr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	AT91Emacb *emac = (AT91Emacb *) clientData;
	emac->regPTR = value & 0xFFFF;
}

static uint32_t
pfr_read(void *clientData, uint32_t address, int rqlen)
{
	AT91Emacb *emac = (AT91Emacb *) clientData;
	return emac->regPFR & 0xffff;
}

static void
pfr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{

	AT91Emacb *emac = (AT91Emacb *) clientData;
	if (emac->regNCR & NCR_WESTAT) {
		emac->regPFR = value;
	}
}

static uint32_t
fto_read(void *clientData, uint32_t address, int rqlen)
{
	AT91Emacb *emac = (AT91Emacb *) clientData;
	return emac->regFTO & 0xffffff;
}

static void
fto_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	AT91Emacb *emac = (AT91Emacb *) clientData;
	if (emac->regNCR & NCR_WESTAT) {
		emac->regFTO = value;
	}
}

static uint32_t
scf_read(void *clientData, uint32_t address, int rqlen)
{
	AT91Emacb *emac = (AT91Emacb *) clientData;
	return emac->regSCF & 0xffff;
}

static void
scf_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	AT91Emacb *emac = (AT91Emacb *) clientData;
	if (emac->regNCR & NCR_WESTAT) {
		emac->regSCF = value;
	}
}

static uint32_t
mcf_read(void *clientData, uint32_t address, int rqlen)
{
	AT91Emacb *emac = (AT91Emacb *) clientData;
	return emac->regMCF & 0xffff;
}

static void
mcf_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	AT91Emacb *emac = (AT91Emacb *) clientData;
	if (emac->regNCR & NCR_WESTAT) {
		emac->regMCF = value;
	}
}

static uint32_t
fro_read(void *clientData, uint32_t address, int rqlen)
{
	AT91Emacb *emac = (AT91Emacb *) clientData;
	return emac->regFRO & 0xffffff;
}

static void
fro_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	AT91Emacb *emac = (AT91Emacb *) clientData;
	if (emac->regNCR & NCR_WESTAT) {
		emac->regFRO = value;
	}
}

static uint32_t
fcse_read(void *clientData, uint32_t address, int rqlen)
{
	AT91Emacb *emac = (AT91Emacb *) clientData;
	return emac->regFCSE & 0xff;
}

static void
fcse_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	AT91Emacb *emac = (AT91Emacb *) clientData;
	if (emac->regNCR & NCR_WESTAT) {
		emac->regFCSE = value;
	}
}

static uint32_t
ale_read(void *clientData, uint32_t address, int rqlen)
{
	AT91Emacb *emac = (AT91Emacb *) clientData;
	return emac->regALE & 0xff;
}

static void
ale_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	AT91Emacb *emac = (AT91Emacb *) clientData;
	if (emac->regNCR & NCR_WESTAT) {
		emac->regALE = value;
	}
}

static uint32_t
dtf_read(void *clientData, uint32_t address, int rqlen)
{
	AT91Emacb *emac = (AT91Emacb *) clientData;
	return emac->regDTF & 0xffff;
}

static void
dtf_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	AT91Emacb *emac = (AT91Emacb *) clientData;
	if (emac->regNCR & NCR_WESTAT) {
		emac->regDTF = value;
	}
}

static uint32_t
lcol_read(void *clientData, uint32_t address, int rqlen)
{
	AT91Emacb *emac = (AT91Emacb *) clientData;
	return emac->regLCOL & 0xff;
}

static void
lcol_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	AT91Emacb *emac = (AT91Emacb *) clientData;
	if (emac->regNCR & NCR_WESTAT) {
		emac->regLCOL = value;
	}
}

static uint32_t
ecol_read(void *clientData, uint32_t address, int rqlen)
{
	AT91Emacb *emac = (AT91Emacb *) clientData;
	return emac->regECOL & 0xff;
}

static void
ecol_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	AT91Emacb *emac = (AT91Emacb *) clientData;
	if (emac->regNCR & NCR_WESTAT) {
		emac->regECOL = value;
	}
}

static uint32_t
tund_read(void *clientData, uint32_t address, int rqlen)
{
	AT91Emacb *emac = (AT91Emacb *) clientData;
	return emac->regTUND & 0xff;
}

static void
tund_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	AT91Emacb *emac = (AT91Emacb *) clientData;
	if (emac->regNCR & NCR_WESTAT) {
		emac->regTUND = value;
	}
}

static uint32_t
cse_read(void *clientData, uint32_t address, int rqlen)
{
	AT91Emacb *emac = (AT91Emacb *) clientData;
	return emac->regCSE & 0xff;
}

static void
cse_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	AT91Emacb *emac = (AT91Emacb *) clientData;
	if (emac->regNCR & NCR_WESTAT) {
		emac->regCSE = value;
	}
}

static uint32_t
rre_read(void *clientData, uint32_t address, int rqlen)
{
	AT91Emacb *emac = (AT91Emacb *) clientData;
	return emac->regRRE & 0xffff;
}

static void
rre_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	AT91Emacb *emac = (AT91Emacb *) clientData;
	if (emac->regNCR & NCR_WESTAT) {
		emac->regRRE = value;
	}
}

static uint32_t
rov_read(void *clientData, uint32_t address, int rqlen)
{
	AT91Emacb *emac = (AT91Emacb *) clientData;
	return emac->regROVR & 0xff;
}

static void
rov_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	AT91Emacb *emac = (AT91Emacb *) clientData;
	if (emac->regNCR & NCR_WESTAT) {
		emac->regROVR = value;
	}
}

static uint32_t
rse_read(void *clientData, uint32_t address, int rqlen)
{
	AT91Emacb *emac = (AT91Emacb *) clientData;
	return emac->regRSE & 0xff;
}

static void
rse_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	AT91Emacb *emac = (AT91Emacb *) clientData;
	if (emac->regNCR & NCR_WESTAT) {
		emac->regRSE = value;
	}
}

static uint32_t
ele_read(void *clientData, uint32_t address, int rqlen)
{
	AT91Emacb *emac = (AT91Emacb *) clientData;
	return emac->regELE & 0xff;
}

static void
ele_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	AT91Emacb *emac = (AT91Emacb *) clientData;
	if (emac->regNCR & NCR_WESTAT) {
		emac->regELE = value;
	}
}

static uint32_t
rja_read(void *clientData, uint32_t address, int rqlen)
{
	AT91Emacb *emac = (AT91Emacb *) clientData;
	return emac->regRJA & 0xff;
}

static void
rja_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	AT91Emacb *emac = (AT91Emacb *) clientData;
	if (emac->regNCR & NCR_WESTAT) {
		emac->regRJA = value;
	}
}

static uint32_t
usf_read(void *clientData, uint32_t address, int rqlen)
{
	AT91Emacb *emac = (AT91Emacb *) clientData;
	return emac->regUSF & 0xff;
}

static void
usf_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	AT91Emacb *emac = (AT91Emacb *) clientData;
	if (emac->regNCR & NCR_WESTAT) {
		emac->regUSF = value;
	}
}

static uint32_t
ste_read(void *clientData, uint32_t address, int rqlen)
{
	AT91Emacb *emac = (AT91Emacb *) clientData;
	return emac->regSTE & 0xff;
}

static void
ste_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	AT91Emacb *emac = (AT91Emacb *) clientData;
	if (emac->regNCR & NCR_WESTAT) {
		emac->regSTE = value;
	}
}

static uint32_t
rle_read(void *clientData, uint32_t address, int rqlen)
{
	AT91Emacb *emac = (AT91Emacb *) clientData;
	return emac->regRLE & 0xff;
}

static void
rle_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	AT91Emacb *emac = (AT91Emacb *) clientData;
	if (emac->regNCR & NCR_WESTAT) {
		emac->regRLE = value;
	}
}

static uint32_t
hrb_read(void *clientData, uint32_t address, int rqlen)
{
	AT91Emacb *emac = clientData;
	return emac->regHASH & 0xFFFFFFFF;
}

static void
hrb_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	AT91Emacb *emac = clientData;
	emac->regHASH = (emac->regHASH & UINT64_C(0xFFFFffff00000000)) | value;
}

static uint32_t
hrt_read(void *clientData, uint32_t address, int rqlen)
{
	AT91Emacb *emac = clientData;
	return (emac->regHASH >> 32) & 0xFFFFFFFF;
}

static void
hrt_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	AT91Emacb *emac = clientData;
	emac->regHASH = (emac->regHASH & UINT64_C(0xFFFFffff))
	    | (uint64_t) value << 32;
}

static uint32_t
sa1b_read(void *clientData, uint32_t address, int rqlen)
{
	AT91Emacb *emac = (AT91Emacb *) clientData;
	return (emac->regSA1[3] << 24) | (emac->regSA1[2] << 16)
	    | (emac->regSA1[1] << 8) | emac->regSA1[0];
}

static void
sa1b_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	AT91Emacb *emac = (AT91Emacb *) clientData;
	emac->pkt_matchmask &= ~RBDE_AM_SA1;
	emac->regSA1[3] = (value >> 24) & 0xff;
	emac->regSA1[2] = (value >> 16) & 0xff;
	emac->regSA1[1] = (value >> 8) & 0xff;
	emac->regSA1[0] = (value >> 0) & 0xff;
}

static uint32_t
sa1t_read(void *clientData, uint32_t address, int rqlen)
{
	AT91Emacb *emac = (AT91Emacb *) clientData;
	return (emac->regSA1[5] << 8) | emac->regSA1[4];
}

static void
sa1t_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	AT91Emacb *emac = (AT91Emacb *) clientData;
	emac->regSA1[5] = (value >> 8) & 0xff;
	emac->regSA1[4] = (value >> 0) & 0xff;
	emac->pkt_matchmask |= RBDE_AM_SA1;
}

static uint32_t
sa2b_read(void *clientData, uint32_t address, int rqlen)
{
	AT91Emacb *emac = (AT91Emacb *) clientData;
	return (emac->regSA2[3] << 24) | (emac->regSA2[2] << 16)
	    | (emac->regSA2[1] << 8) | emac->regSA2[0];
}

static void
sa2b_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	AT91Emacb *emac = (AT91Emacb *) clientData;
	emac->pkt_matchmask &= ~RBDE_AM_SA2;
	emac->regSA2[3] = (value >> 24) & 0xff;
	emac->regSA2[2] = (value >> 16) & 0xff;
	emac->regSA2[1] = (value >> 8) & 0xff;
	emac->regSA2[0] = (value >> 0) & 0xff;
}

static uint32_t
sa2t_read(void *clientData, uint32_t address, int rqlen)
{
	AT91Emacb *emac = (AT91Emacb *) clientData;
	return (emac->regSA2[5] << 8) | emac->regSA2[4];
}

static void
sa2t_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	AT91Emacb *emac = (AT91Emacb *) clientData;
	emac->regSA2[5] = (value >> 8) & 0xff;
	emac->regSA2[4] = (value >> 0) & 0xff;
	emac->pkt_matchmask |= RBDE_AM_SA2;
}

static uint32_t
sa3b_read(void *clientData, uint32_t address, int rqlen)
{
	AT91Emacb *emac = (AT91Emacb *) clientData;
	return (emac->regSA3[3] << 24) | (emac->regSA3[2] << 16)
	    | (emac->regSA3[1] << 8) | emac->regSA3[0];
}

static void
sa3b_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	AT91Emacb *emac = (AT91Emacb *) clientData;
	emac->pkt_matchmask &= ~RBDE_AM_SA3;
	emac->regSA3[3] = (value >> 24) & 0xff;
	emac->regSA3[2] = (value >> 16) & 0xff;
	emac->regSA3[1] = (value >> 8) & 0xff;
	emac->regSA3[0] = (value >> 0) & 0xff;
}

static uint32_t
sa3t_read(void *clientData, uint32_t address, int rqlen)
{
	AT91Emacb *emac = (AT91Emacb *) clientData;
	return (emac->regSA3[5] << 8) | emac->regSA3[4];
}

static void
sa3t_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	AT91Emacb *emac = (AT91Emacb *) clientData;
	emac->regSA3[5] = (value >> 8) & 0xff;
	emac->regSA3[4] = (value >> 0) & 0xff;
	emac->pkt_matchmask |= RBDE_AM_SA3;
}

static uint32_t
sa4b_read(void *clientData, uint32_t address, int rqlen)
{
	AT91Emacb *emac = (AT91Emacb *) clientData;
	return (emac->regSA4[3] << 24) | (emac->regSA4[2] << 16)
	    | (emac->regSA4[1] << 8) | emac->regSA4[0];
}

static void
sa4b_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	AT91Emacb *emac = (AT91Emacb *) clientData;
	emac->pkt_matchmask &= ~RBDE_AM_SA4;
	emac->regSA4[3] = (value >> 24) & 0xff;
	emac->regSA4[2] = (value >> 16) & 0xff;
	emac->regSA4[1] = (value >> 8) & 0xff;
	emac->regSA4[0] = (value >> 0) & 0xff;
}

static uint32_t
sa4t_read(void *clientData, uint32_t address, int rqlen)
{
	AT91Emacb *emac = (AT91Emacb *) clientData;
	return (emac->regSA4[5] << 8) | emac->regSA4[4];
}

static void
sa4t_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	AT91Emacb *emac = (AT91Emacb *) clientData;
	emac->regSA4[5] = (value >> 8) & 0xff;
	emac->regSA4[4] = (value >> 0) & 0xff;
	emac->pkt_matchmask |= RBDE_AM_SA4;
}

static uint32_t
tid_read(void *clientData, uint32_t address, int rqlen)
{
	return 0;
}

static void
tid_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	AT91Emacb *emac = (AT91Emacb *) clientData;
	emac->regTID[1] = (value >> 8) & 0xff;
	emac->regTID[0] = (value >> 0) & 0xff;
}

static uint32_t
usrio_read(void *clientData, uint32_t address, int rqlen)
{
	return 0;
}

static void
usrio_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
}

static void
AT91Emacb_Map(void *owner, uint32_t base, uint32_t mask, uint32_t flags)
{
	AT91Emacb *emac = (AT91Emacb *) owner;
	IOH_New32(EMAC_NCR(base), ncr_read, ncr_write, emac);
	IOH_New32(EMAC_NCFG(base), ncfg_read, ncfg_write, emac);
	IOH_New32(EMAC_NSR(base), nsr_read, nsr_write, emac);
	IOH_New32(EMAC_TSR(base), tsr_read, tsr_write, emac);
	IOH_New32(EMAC_RBQP(base), rbqp_read, rbqp_write, emac);
	IOH_New32(EMAC_TBQP(base), tbqp_read, tbqp_write, emac);
	IOH_New32(EMAC_RSR(base), rsr_read, rsr_write, emac);
	IOH_New32(EMAC_ISR(base), isr_read, isr_write, emac);
	IOH_New32(EMAC_IER(base), ier_read, ier_write, emac);
	IOH_New32(EMAC_IDR(base), idr_read, idr_write, emac);
	IOH_New32(EMAC_IMR(base), imr_read, imr_write, emac);
	IOH_New32(EMAC_MAN(base), man_read, man_write, emac);
	IOH_New32(EMAC_PTR(base), ptr_read, ptr_write, emac);
	IOH_New32(EMAC_PFR(base), pfr_read, pfr_write, emac);
	IOH_New32(EMAC_FTO(base), fto_read, fto_write, emac);
	IOH_New32(EMAC_SCF(base), scf_read, scf_write, emac);
	IOH_New32(EMAC_MCF(base), mcf_read, mcf_write, emac);
	IOH_New32(EMAC_FRO(base), fro_read, fro_write, emac);
	IOH_New32(EMAC_FCSE(base), fcse_read, fcse_write, emac);
	IOH_New32(EMAC_ALE(base), ale_read, ale_write, emac);
	IOH_New32(EMAC_DTF(base), dtf_read, dtf_write, emac);
	IOH_New32(EMAC_LCOL(base), lcol_read, lcol_write, emac);
	IOH_New32(EMAC_ECOL(base), ecol_read, ecol_write, emac);
	IOH_New32(EMAC_TUND(base), tund_read, tund_write, emac);
	IOH_New32(EMAC_CSE(base), cse_read, cse_write, emac);
	IOH_New32(EMAC_RRE(base), rre_read, rre_write, emac);
	IOH_New32(EMAC_ROV(base), rov_read, rov_write, emac);
	IOH_New32(EMAC_RSE(base), rse_read, rse_write, emac);
	IOH_New32(EMAC_ELE(base), ele_read, ele_write, emac);
	IOH_New32(EMAC_RJA(base), rja_read, rja_write, emac);
	IOH_New32(EMAC_USF(base), usf_read, usf_write, emac);
	IOH_New32(EMAC_STE(base), ste_read, ste_write, emac);
	IOH_New32(EMAC_RLE(base), rle_read, rle_write, emac);
	IOH_New32(EMAC_HRB(base), hrb_read, hrb_write, emac);
	IOH_New32(EMAC_HRT(base), hrt_read, hrt_write, emac);
	IOH_New32(EMAC_SA1B(base), sa1b_read, sa1b_write, emac);
	IOH_New32(EMAC_SA1T(base), sa1t_read, sa1t_write, emac);
	IOH_New32(EMAC_SA2B(base), sa2b_read, sa2b_write, emac);
	IOH_New32(EMAC_SA2T(base), sa2t_read, sa2t_write, emac);
	IOH_New32(EMAC_SA3B(base), sa3b_read, sa3b_write, emac);
	IOH_New32(EMAC_SA3T(base), sa3t_read, sa3t_write, emac);
	IOH_New32(EMAC_SA4B(base), sa4b_read, sa4b_write, emac);
	IOH_New32(EMAC_SA4T(base), sa4t_read, sa4t_write, emac);
	IOH_New32(EMAC_TID(base), tid_read, tid_write, emac);
	IOH_New32(EMAC_USRIO(base), usrio_read, usrio_write, emac);
}

static void
AT91Emacb_UnMap(void *owner, uint32_t base, uint32_t mask)
{
	IOH_Delete32(EMAC_NCR(base));
	IOH_Delete32(EMAC_NCFG(base));
	IOH_Delete32(EMAC_NSR(base));
	IOH_Delete32(EMAC_TSR(base));
	IOH_Delete32(EMAC_RBQP(base));
	IOH_Delete32(EMAC_TBQP(base));
	IOH_Delete32(EMAC_RSR(base));
	IOH_Delete32(EMAC_ISR(base));
	IOH_Delete32(EMAC_IER(base));
	IOH_Delete32(EMAC_IDR(base));
	IOH_Delete32(EMAC_IMR(base));
	IOH_Delete32(EMAC_MAN(base));
	IOH_Delete32(EMAC_PTR(base));
	IOH_Delete32(EMAC_PFR(base));
	IOH_Delete32(EMAC_FTO(base));
	IOH_Delete32(EMAC_SCF(base));
	IOH_Delete32(EMAC_MCF(base));
	IOH_Delete32(EMAC_FRO(base));
	IOH_Delete32(EMAC_FCSE(base));
	IOH_Delete32(EMAC_ALE(base));
	IOH_Delete32(EMAC_DTF(base));
	IOH_Delete32(EMAC_LCOL(base));
	IOH_Delete32(EMAC_ECOL(base));
	IOH_Delete32(EMAC_TUND(base));
	IOH_Delete32(EMAC_CSE(base));
	IOH_Delete32(EMAC_RRE(base));
	IOH_Delete32(EMAC_ROV(base));
	IOH_Delete32(EMAC_RSE(base));
	IOH_Delete32(EMAC_ELE(base));
	IOH_Delete32(EMAC_RJA(base));
	IOH_Delete32(EMAC_USF(base));
	IOH_Delete32(EMAC_STE(base));
	IOH_Delete32(EMAC_RLE(base));
	IOH_Delete32(EMAC_HRB(base));
	IOH_Delete32(EMAC_HRT(base));
	IOH_Delete32(EMAC_SA1B(base));
	IOH_Delete32(EMAC_SA1T(base));
	IOH_Delete32(EMAC_SA2B(base));
	IOH_Delete32(EMAC_SA2T(base));
	IOH_Delete32(EMAC_SA3B(base));
	IOH_Delete32(EMAC_SA3T(base));
	IOH_Delete32(EMAC_SA4B(base));
	IOH_Delete32(EMAC_SA4T(base));
	IOH_Delete32(EMAC_TID(base));
	IOH_Delete32(EMAC_USRIO(base));
}

int
AT91Emacb_RegisterPhy(BusDevice * dev, PHY_Device * phy, unsigned int phy_addr)
{
	AT91Emacb *emac = dev->owner;
	if (phy_addr >= MAX_PHYS) {
		fprintf(stderr, "AT91Emacb: Illegal PHY address %d\n", phy_addr);
		return -1;
	} else {
		emac->phy[phy_addr] = phy;
		return 0;
	}

}

BusDevice *
AT91Emacb_New(const char *name)
{
	AT91Emacb *emac = sg_new(AT91Emacb);
	emac->ether_fd = Net_CreateInterface(name);
	emac->input_fh = AsyncManager_PollInit(emac->ether_fd);
	emac->irqNode = SigNode_New("%s.irq", name);
	if (!emac->irqNode) {
		fprintf(stderr, "AT91Emacb: Can't create interrupt request line\n");
		exit(1);
	}
	SigNode_Set(emac->irqNode, SIG_PULLDOWN);

	/* The reset value initialization is complete ! all other values are 0 */
	emac->regNCFG = 0x800;
	emac->regNSR = 0x6;
	emac->regTSR = 0;
	emac->regIMR = 0x3fff;
	emac->bdev.first_mapping = NULL;
	emac->bdev.Map = AT91Emacb_Map;
	emac->bdev.UnMap = AT91Emacb_UnMap;
	emac->bdev.owner = emac;
	emac->bdev.hw_flags = MEM_FLAG_WRITABLE | MEM_FLAG_READABLE;
	emac->pkt_matchmask = 0;
	CycleTimer_Init(&emac->rcvDelayTimer, rcv_delay_done, emac);
	CycleTimer_Init(&emac->txDelayTimer, tx_delay_done, emac);
	fprintf(stderr, "AT91 Ethernet EMACB \"%s\" created\n", name);
	return &emac->bdev;
}
