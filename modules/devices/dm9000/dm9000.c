/*
 *************************************************************************************************
 *
 * Emulation of the Davicom DM9000 Ethernet Controller
 *
 * State:
 *      Working with the linux dm9000 driver for kernel 2.6 
 *	CRC is missing, 
 *	receive fifo overflow behaviour may not match real device 
 *
 *
 * Copyright 2004 2005 2006 Jochen Karrer. All rights reserved.
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
#include "dm9000.h"

// include system header
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// include library header

// include user header
#include "cycletimer.h"
#include "configfile.h"
#include "devices/phy/phy.h"
#include "linux-tap.h"
#include "m93c46.h"
#include "sgstring.h"
#include "signode.h"
#include "core/asyncmanager.h"
#include "initializer.h"


#if 0
#define dbgprintf(...) { fprintf(stderr,__VA_ARGS__); }
#else
#define dbgprintf(...)
#endif

#define DM_NCR 		(0)
#define		NCR_EXT_PHY	(1<<7)
#define		NCR_WAKEEN	(1<<6)
#define		NCR_FCOL	(1<<4)
#define		NCR_FDX		(1<<3)
#define		NCR_LBK_MASK	(3<<1)
#define		NCR_LBK_SHIFT	(1)
#define 	NCR_RST		(1<<0)
#define DM_NSR 		(1)
#define 	NSR_SPEED 	(1<<7)
#define 	NSR_LINKST 	(1<<6)
#define 	NSR_WAKEST	(1<<5)
#define 	NSR_TX2_END	(1<<3)
#define 	NSR_TX1_END	(1<<2)
#define 	NSR_RXOV	(1<<1)

#define DM_TCR  		(2)
#define		TCR_TJDIS	(1<<6)
#define		TCR_ECECM	(1<<5)
#define		TCR_PAD_DIS2	(1<<4)
#define		TCR_CRC_DIS2	(1<<3)
#define		TCR_PAD_DIS1	(1<<2)
#define		TCR_CRC_DIS1	(1<<1)
#define		TCR_TXREQ	(1<<0)

#define DM_TSR_I 	(3)
#define DM_TSR_II	(4)
#define		TSR_TJTO 	(1<<7)
#define		TSR_LCAR	(1<<6)
#define		TSR_NC		(1<<5)
#define		TSR_LCOL	(1<<4)
#define		TSR_COL		(1<<3)
#define		TSR_EC		(1<<2)

#define DM_RCR		(5)
#define 	RCR_WTDIS	(1<<6)
#define 	RCR_DIS_LONG	(1<<5)
#define 	RCR_DIS_CRC	(1<<4)
#define 	RCR_ALL		(1<<3)
#define 	RCR_RUNT	(1<<2)
#define 	RCR_PRMSC	(1<<1)
#define 	RCR_RXEN	(1<<0)

#define DM_RSR		(6)
#define		RSR_RF		(1<<7)
#define		RSR_MF		(1<<6)
#define		RSR_LCS		(1<<5)
#define		RSR_RWTO	(1<<4)
#define		RSR_PLE		(1<<3)
#define		RSR_AE		(1<<2)
#define		RSR_CE		(1<<1)
#define		RSR_FOE		(1<<0)

#define DM_ROCR		(7)
#define		ROCR_RXFU (1<<7)
#define		ROCR_ROC_MASK (0x7f)

#define DM_BPTR		(8)
#define DM_FCTR		(9)
#define DM_FCR		(0xa)
#define		FCR_TXP0  	(1<<7)
#define 	FCR_TXPF  	(1<<6)
#define 	FCR_TXPEN 	(1<<5)
#define		FCR_BKPA	(1<<4)
#define		FCR_BKPM	(1<<3)
#define		FCR_RXPS	(1<<2)
#define		FCR_RXPCS	(1<<1)
#define		FCR_FLCE	(1<<0)

#define DM_EPCR		(0xb)
#define		EPCR_REEP 	(1<<5)
#define		EPCR_WEP	(1<<4)
#define		EPCR_EPOS	(1<<3)
#define		EPCR_ERPRR	(1<<2)
#define		EPCR_ERPRW	(1<<1)
#define		EPCR_ERRE	(1<<0)

#define DM_EPAR		(0xc)
#define		EPAR_PHY_ADDR_MASK 	(3<<6)
#define		EPAR_PHY_ADDR_SHIFT	(6)
#define		EPAR_EROA_MASK		(0x3f)
#define		EPAR_EROA_SHIFT		(0)

#define DM_EPDRL	(0xd)
#define DM_EPDRH	(0xe)
#define DM_WCR		(0xf)
#define		WCR_LINKEN	(1<<5)
#define		WCR_SAMPLEEN	(1<<4)
#define		WCR_MAGICEN	(1<<3)
#define		WCR_LINKST	(1<<2)
#define		WCR_SAMPLEST	(1<<1)
#define		WCR_MAGICST	(1<<0)

#define DM_PAR0		(0x10)
#define DM_PAR1		(0x11)
#define DM_PAR2		(0x12)
#define DM_PAR3		(0x13)
#define DM_PAR4		(0x14)
#define DM_PAR5		(0x15)
#define DM_MAR0		(0x16)
#define DM_MAR1		(0x17)
#define DM_MAR2		(0x18)
#define DM_MAR3		(0x19)
#define DM_MAR4		(0x1a)
#define DM_MAR5		(0x1b)
#define DM_MAR6		(0x1c)
#define DM_MAR7		(0x1d)
#define DM_GPCR		(0x1e)
#define DM_GPR		(0x1f)
#define		GPR_PHY_DIS 	(1<<0)
#define DM_TRPAL	(0x22)
#define DM_TRPAH	(0x23)
#define DM_RWPAL	(0x24)
#define DM_RWPAH	(0x25)
#define DM_VID0		(0x28)
#define DM_VID1		(0x29)
#define DM_PID0		(0x2a)
#define DM_PID1		(0x2b)
#define DM_CHIPR	(0x2c)
#define DM_SMCR		(0x2f)
#define		SMCR_SM_EN	(1<<7)
#define		SMCR_FLC	(1<<2)
#define		SMCR_FB1	(1<<1)
#define		SMCR_FB0	(1<<0)

/* Only in newer revisions of the chip */
#define DM_ETXCSR	(0x30)
#define DM_TCSCR	(0x31)
#define	DM_RCSCSR	(0x32)
#define DM_MPAR		(0x33)
#define DM_LEDCR	(0x34)
#define	DM_BUSCR	(0x38)
#define	DM_INTCR	(0x39)
#define DM_SCCR		(0x50)
#define DM_RSCCR	(0x51)

#define DM_MRCMDX	(0xf0)
#define DM_MRCMD	(0xf2)
#define DM_MRRL		(0xf4)
#define DM_MRRH		(0xf5)
#define DM_MWCMDX	(0xf6)
#define DM_MWCMD	(0xf8)
#define DM_MWRL		(0xfa)
#define DM_MWRH		(0xfb)
#define DM_TXPLL	(0xfc)
#define DM_TXPLH	(0xfd)
#define DM_ISR		(0xfe)
#define 	ISR_IOMODE_SHIFT (6)
#define 	ISR_IOMODE_MASK (3<<6)
#define		IOMODE_16	(0)
#define		IOMODE_32	(1)
#define		IOMODE_8	(2)
#define		IOMODE_RESERVED	(3)
#define		ISR_ROOS (1<<3)
#define		ISR_ROS  (1<<2)
#define		ISR_PTS  (1<<1)
#define		ISR_PRS  (1<<0)
#define DM_IMR		(0xff)
#define		IMR_PAR	 (1<<7)
#define		IMR_ROOM (1<<3)
#define 	IMR_ROM	 (1<<2)
#define		IMR_PTM	 (1<<1)
#define		IMR_PRM	 (1<<0)

/* Davicom internal Phy registers */
#define DMP_CONTROL             (0)
#define DMP_STATUS              (1)
#define DMP_PHY_ID1             (2)
#define DMP_PHY_ID2             (3)
#define DMP_AN_ADVERTISE        (4)
#define DMP_AN_LP_ABILITY       (5)
#define DMP_AN_EXPANSION        (6)
#define	DMP_DSCR		(16)
#define	DMP_DSCSR		(17)
#define	DMP_BTCSR10		(18)

typedef struct DM9000 DM9000;
typedef uint32_t DMReadProc(DM9000 * dm, uint8_t index, int rqlen);
typedef void DMWriteProc(DM9000 * dm, uint32_t value, uint8_t index, int rqlen);

#define MAX_PHYS (32)

struct DM9000 {
	int register_spacing;	/* (bytes) for io-register spacing */
	int buswidth;		/* unit is bytes */
	M93C46 *eprom;
	int eeprom_valid;
	uint8_t randmac[6];	/* random mac for case of invalid eeprom */
	PHY_Device *extphy[MAX_PHYS];
	PHY_Device iphy;
	/* The signal levels for the interrupts */
	int int_active;
	int int_inactive;
	SigNode *irqNode;
	int interrupt_posted;
	BusDevice bdev;
	int ether_fd;
	PollHandle_t *input_fh;
	int receiver_is_enabled;

	DMReadProc *read_reg[256];
	DMWriteProc *write_reg[256];
	uint8_t index_reg;
	uint64_t index_chg_cnt;	/* some shity registers do not update when index is not changed */
	CycleTimer epcr_busyTimer;	/* eeprom or Phy access in progress timer */

	int tx_pktindex;
	uint8_t sram[16384];
	uint32_t rxfifo_wp;
	uint32_t rxfifo_rp;
	int rxfifo_count;
	uint32_t txfifo_wp;
	uint32_t txfifo_rp;
	uint16_t reg_addr;
	/* The registers */
	uint8_t ncr;
	uint8_t nsr;
	uint8_t tcr;
	uint8_t tsr_i;
	uint8_t tsr_ii;
	uint8_t rcr;
	uint8_t rsr;
	uint8_t rocr;
	uint8_t bptr;
	uint8_t fctr;
	uint8_t fcr;
	uint8_t epcr;
	uint8_t epar;
	uint8_t epdrl;
	uint8_t epdrh;
	uint8_t wcr;
	uint8_t par[6];
	uint8_t mar[8];
	uint8_t gpcr;
	uint8_t gpr;
	uint8_t trpal;
	uint8_t trpah;
	uint8_t rwpal;
	uint8_t rwpah;
	uint8_t vidl;
	uint8_t vidh;
	uint8_t pidl;
	uint8_t pidh;
	uint8_t chipr;
	uint8_t smcr;

	/* only in new revision */
	uint8_t etxcsr;
	uint8_t tcscr;
	uint8_t rcscsr;
	uint8_t mpar;
	uint8_t ledcr;
	uint8_t buscr;
	uint8_t intcr;
	uint8_t sccr;
	uint8_t rsccr;

	uint32_t mrcmd_latch;	/* Maximum size for 32Bit bus */
	uint64_t mrcmdx_idx_tag;
	uint8_t mrcmd;
	uint8_t mwcmdx;
	uint8_t mwcmd;
	uint8_t txpll;
	uint8_t txplh;
	uint8_t isr;
	uint8_t imr;

/* Internal PHY register */
	uint16_t control;
	uint16_t status;
	uint16_t phy_id1;
	uint16_t phy_id2;
	uint16_t an_advertise;
	uint16_t an_lp_ability;
	uint16_t an_expansion;
	/* Davicom proprietary registers */
	uint16_t dscr;
	uint16_t dscsr;
	uint16_t btcsr10;
};

static inline int
phy_is_enabled(DM9000 * dm)
{
	if (dm->gpr & GPR_PHY_DIS) {
		return 0;
	} else {
		return 1;
	}
}

static void enable_receiver(DM9000 * dm);
static void disable_receiver(DM9000 * dm);

static void
update_receiver_status(DM9000 * dm)
{
	int max_pktsize;
	if (!(dm->rcr & RCR_RXEN)) {
		disable_receiver(dm);
		return;
	}
	/* add  header +datalen + crc + maxpad + leading 0 for next packet */
	max_pktsize = 4 + 1522 + 4 + 3 + 1;
	if ((13 * 1024 - dm->rxfifo_count) <= max_pktsize) {
		//      fprintf(stderr,"to full, count %d,rp %d, wp %d\n",dm->rxfifo_count,dm->rxfifo_rp,dm->rxfifo_wp);
		disable_receiver(dm);
	} else {
		enable_receiver(dm);
	}
}

static inline void
update_rxfifo_count(DM9000 * dm)
{
	dm->rxfifo_count = dm->rxfifo_wp - dm->rxfifo_rp;
	if (dm->rxfifo_count < 0) {
		dm->rxfifo_count += 13 * 1024;
	}
}

/*
 * ---------------------------------------------------------
 * Interrupts use the same logic as in real chip
 * ---------------------------------------------------------
 */
static void
update_interrupts(DM9000 * dm)
{
	/* Only lower 4 Bits are interupt bits */
	if ((dm->isr & dm->imr) & 0xf) {
		if (!dm->interrupt_posted) {
			SigNode_Set(dm->irqNode, dm->int_active);
			dm->interrupt_posted = 1;
		}
	} else {
		if (dm->interrupt_posted) {
			SigNode_Set(dm->irqNode, dm->int_inactive);	// ? SIG_OPEN or SIG_PULLUP ?
			dm->interrupt_posted = 0;
		}
	}
}

static inline void
rxfifo_putc(DM9000 * dm, uint8_t c)
{
	dm->sram[dm->rxfifo_wp] = c;
	dm->rxfifo_wp = dm->rxfifo_wp + 1;
	dm->rxfifo_count++;
	if (dm->rxfifo_wp >= 16 * 1024) {
		dm->rxfifo_wp = 3 * 1024;
	}
}

/*
 * ------------------------------------------
 * Put a packet into the rxfifo
 * ------------------------------------------
 */
void
rxfifo_put_packet(DM9000 * dm, uint8_t * buf, int count)
{
	uint8_t status = 0;	//  Wrong, same format as RSR
	uint8_t iomode;
	int pktsize;
	int i;
	uint32_t rp, wp;
	dbgprintf("Emu: Put a packet with len %d\n", count);
	rp = dm->rxfifo_rp;
	wp = dm->rxfifo_wp;
	if (rp <= wp) {
		rp += 13 * 1024;
	}
	/* add  header +datalen + crc + maxpad + leading 0 for next packet */
	pktsize = 4 + count + 4 + 3 + 1;
	if ((wp + pktsize) > rp) {
		fprintf(stderr, "DM9000: Rxfifo overflow rp %d, wp %d\n", rp, wp);
		dm->nsr |= NSR_RXOV;
		dm->rocr = (dm->rocr & 0x80) | ((dm->rocr & 0x7f) + 1);
		dm->isr |= ISR_ROS;
		dm->rsr |= RSR_FOE;
		update_interrupts(dm);
		return;
	} else {
		dm->nsr &= ~NSR_RXOV;
		dm->rsr &= ~RSR_FOE;
	}
	rxfifo_putc(dm, 1);
	rxfifo_putc(dm, status);
	rxfifo_putc(dm, (count + 4) & 0xff);
	rxfifo_putc(dm, ((count + 4) >> 8) & 0xff);
	/* Put the data */
	for (i = 0; i < count; i++) {
		rxfifo_putc(dm, buf[i]);
	}
	/* CRC calculation is missing here */
	for (i = 0; i < 4; i++) {
		rxfifo_putc(dm, 0);
	}
	/* Align */
	iomode = (dm->isr & ISR_IOMODE_MASK) >> ISR_IOMODE_SHIFT;
	switch (iomode) {
	    case IOMODE_32:
		    while (dm->rxfifo_wp & 3) {
			    rxfifo_putc(dm, 0xff);
		    }
		    break;
	    case IOMODE_16:
		    if (dm->rxfifo_wp & 1) {
			    dbgprintf("IOMODE16 padding\n");
			    rxfifo_putc(dm, 0xff);
		    }
		    break;
	    case IOMODE_8:
		    break;
	    default:
		    fprintf(stderr, "DM9000: Illegal iomode %d\n", iomode);
	}
	dm->sram[dm->rxfifo_wp] = 0;	/* Declare next packet as not yet received */
	dbgprintf("next packet will be at %d\n", dm->rxfifo_wp);
	return;
}

/*
 * -------------------------------------------
 * Check MAC address if it is an Broadcast 
 * -------------------------------------------
 */
static inline int
is_broadcast(uint8_t * mac)
{
	if (mac[0] & 1) {
		return 1;
	}
	return 0;
}

/*
 * ----------------------------------------------
 * Check if incoming packet can be accepted
 * Multicast hash table is missing
 * ----------------------------------------------
 */
static int
match_address(DM9000 * dm, uint8_t * packet)
{
	if (is_broadcast(packet)) {
		return 1;
	}
	/* check for promiscous mode */
	if (dm->rcr & RCR_PRMSC) {
		return 2;
	}
	if (memcmp(packet, dm->par, 6) != 0) {
		int i;
		for (i = 0; i < 6; i++) {
			fprintf(stderr, "par[%d]: %02x, packet[%d]: %02x\n", i, dm->par[i], i,
				packet[i]);
		}
		return 0;
	}
	return 3;
}

/*
 * --------------------------------------------------------------
 * input_event
 * 	The Event handler which reads the incoming datastream and
 * 	puts it to the rxfifo in the SRAM
 * --------------------------------------------------------------
 */
static void
input_event(PollHandle_t *handle, int status, int events, void *clientdata)
{
	DM9000 *dm = clientdata;
	int result;
	uint8_t buf[2048];
	do {
		result = read(dm->ether_fd, buf, 2048);
		if (result > 0) {
			if (!phy_is_enabled(dm)) {
				continue;
			}
			if (!match_address(dm, buf)) {
				continue;
			}
			if ((result > (1522 - 4)) && (dm->rcr & RCR_DIS_LONG)) {
				continue;
			}
			if (result < 60) {
				rxfifo_put_packet(dm, buf, 60);
			} else {
				rxfifo_put_packet(dm, buf, result);
			}
			dm->isr |= ISR_PRS;
			update_interrupts(dm);
			update_receiver_status(dm);
		}
		if (!dm->receiver_is_enabled) {
			break;
		}
	} while (result > 0);
	return;
}

/*  
 * ------------------------------------------------------------------
 * Read empty to avoid warnings in u-boot receiving old packets
 * ------------------------------------------------------------------
 */
static void
clear_pktsrc(DM9000 * dm)
{
	uint8_t buf[100];
	int count;
	int i;
	for (i = 0; i < 500; i++) {
		count = read(dm->ether_fd, buf, 100);
		if (count <= 0) {
			break;
		}
	};
}

/*
 * --------------------------------------------------------
 * enable/disable_receiver
 * Create/Remove Event handler for incoming packets
 * --------------------------------------------------------
 */
static void
enable_receiver(DM9000 * dm)
{
	if (!dm->receiver_is_enabled && (dm->ether_fd >= 0)) {
		dbgprintf("DM9000: enable receiver\n");
		AsyncManager_PollStart(dm->input_fh, ASYNCMANAGER_EVENT_READABLE, &input_event, dm);
		dm->receiver_is_enabled = 1;
	}
}

static void
disable_receiver(DM9000 * dm)
{
	dbgprintf("DM9000: disable receiver\n");
	if (dm->receiver_is_enabled) {
		AsyncManager_PollStop(dm->input_fh);
		dm->receiver_is_enabled = 0;
	}
}

static void
generate_random_hwaddr(uint8_t * hwaddr)
{
	int i;
	for (i = 0; i < 6; i++) {
		hwaddr[i] = lrand48() & 0xff;
	}
	/* Make locally assigned unicast address from it */
	hwaddr[0] = (hwaddr[0] & 0xfe) | 0x02;
}

static void
soft_reset(DM9000 * dm)
{
	dm->ncr = (dm->ncr & 0xc0) | NCR_FDX;
	dm->nsr = NSR_LINKST;
	dm->tcr = 0;
	dm->tsr_i = 0;
	dm->tsr_ii = 0;
	dm->rcr = 0;
	dm->rsr = 0;
	dm->rocr = 0;
	dm->bptr = 0x37;	/* Register 8 */
	dm->fctr = 0x38;	/* Register 9 */
	dm->fcr = 0;
	dm->epcr = 0;
	dm->epar = 0x40;	/* Register 0xc */
	/* dm->epdrl */
	/* dm->epdrh */
	dm->wcr = 0;
	/* uint8_t par[6]; */
	/* uint8_t mar[8]; */
	dm->gpcr = 1;
	dm->gpr = 1;
	dm->trpal = dm->trpah = 0;
	dm->rwpal = 4;
	dm->rwpah = 0;
	dm->vidl = 0x46;
	dm->vidh = 0x0a;
	dm->pidl = 0;
	dm->pidh = 0x90;
	dm->chipr = 0;
	dm->smcr = 0;
	dm->rxfifo_count = 0;
	dm->rxfifo_rp = dm->rxfifo_wp = 0xc00;
	dm->txfifo_rp = dm->txfifo_wp = 0;
	dm->sram[dm->rxfifo_rp] = 0;	/* Is this set zeroed on RXEN ? jk */
	switch (dm->buswidth) {
	    case 1:
		    dm->isr = 0x80;
		    break;
	    case 2:
		    dm->isr = 0;	/* IOMODE 16 Bit */
		    break;
	    case 4:
		    dm->isr = 0x40;
		    break;
	    default:
		    fprintf(stderr, "DM9000 Emulator: Illegal buswidth %d bytes\n", dm->buswidth);

	}
	dm->imr = 0;

	dm->tx_pktindex = 0;	// ????
	update_interrupts(dm);
}

/*
 * --------------------------------------------------
 * Transmit the next packet in the TX-SRAM
 * --------------------------------------------------
 */
static void
transmit_packet(DM9000 * dm)
{
	int len = dm->txpll | (dm->txplh << 8);
	int result;
	int i;
	uint8_t iomode;
	char packet[1600];
	if (len > 1600) {
		dbgprintf("packet to big: %d bytes\n", len);
		return;
	}
	if ((len + dm->txfifo_rp) > 3 * 1024) {
		for (i = 0; i < len; i++) {
			packet[i] = dm->sram[dm->txfifo_rp];
			dm->txfifo_rp = (dm->txfifo_rp + 1) % (3 * 1024);
		}
		if (phy_is_enabled(dm)) {
			result = write(dm->ether_fd, packet, len);
		}
	} else {
		if (phy_is_enabled(dm)) {
			result = write(dm->ether_fd, &dm->sram[dm->txfifo_rp], len);
		}
		dm->txfifo_rp = (dm->txfifo_rp + len) % (3 * 1024);
	}
	if (dm->tx_pktindex == 0) {
		dm->tsr_i = 0;
		dm->nsr |= NSR_TX1_END;
	} else {
		dm->tsr_ii = 0;
		dm->nsr |= NSR_TX2_END;
	}
	dm->isr |= ISR_PTS;

	iomode = (dm->isr & ISR_IOMODE_MASK) >> ISR_IOMODE_SHIFT;
	/* Align the rp to iosize */
	switch (iomode) {
	    case IOMODE_32:
		    while (dm->txfifo_rp & 3) {
			    dm->txfifo_rp = (dm->txfifo_rp + 1) % (3 * 1024);
		    }
		    break;
	    case IOMODE_16:
		    if (dm->txfifo_rp & 1) {
			    dm->txfifo_rp = (dm->txfifo_rp + 1) % (3 * 1024);
		    }
		    break;

	    case IOMODE_8:
		    break;
	    default:
		    fprintf(stderr, "DM9000: Illegal iomode %d\n", iomode);
	}
	update_interrupts(dm);
	dm->tx_pktindex = (dm->tx_pktindex + 1) & 1;
}

/*
 * -------------------------------------------------------------------------------------------
 * releoad_eprom
 * 	Read the M93C46 eeprom connected to the DM9000 and check if the MAC address is good.
 * 	If yes then read and use the MAC. else return with an error. 
 * -------------------------------------------------------------------------------------------
 */
static int
reload_eeprom(DM9000 * dm)
{
	uint8_t data[128];
	uint8_t *mac = data;
	uint16_t pin_control;
	uint16_t auto_load_control;
	int i;
	int result = 0;
	for (i = 0; i < 128; i++) {
		data[i] = m93c46_readb(dm->eprom, i);
	}
	/* check if it is a broadcast address */
	if (mac[0] & 1) {
		result = -1;
	}
	for (i = 0; i < 6; i++) {
		if (mac[i] != 0) {
			break;
		}
	}
	if (i == 6)
		result = -1;
	if (result >= 0) {
		dm->eeprom_valid = 1;
		for (i = 0; i < 6; i++) {
			dm->par[i] = mac[i];
		}
	} else {
		for (i = 0; i < 6; i++) {
			dm->par[i] = dm->randmac[i];
		}
	}
	auto_load_control = data[6] + (data[7] << 8);
	if ((auto_load_control & 0xc) == 4) {
		pin_control = data[12] + (data[13] << 8);
		switch (pin_control & 0x18) {
		    case 0:
			    dm->int_active = SIG_HIGH;
			    dm->int_inactive = SIG_LOW;
			    dm->intcr = 0;
			    break;
		    case 8:
			    dm->int_active = SIG_LOW;
			    dm->int_inactive = SIG_HIGH;
			    dm->intcr = 1;
			    break;

		    case 0x10:
			    dm->int_active = SIG_OPEN;
			    dm->int_inactive = SIG_LOW;
			    dm->intcr = 2;
			    break;

		    case 0x18:
			    dm->int_active = SIG_LOW;
			    dm->int_inactive = SIG_OPEN;
			    dm->intcr = 3;
			    break;
		}
	} else {
		dm->int_active = SIG_HIGH;
		dm->int_inactive = SIG_LOW;
		dm->intcr = 0;
	}
	if (dm->interrupt_posted) {
		SigNode_Set(dm->irqNode, dm->int_active);
	} else {
		SigNode_Set(dm->irqNode, dm->int_inactive);
	}
	return result;
}

static uint32_t
ncr_read(DM9000 * dm, uint8_t index, int rqlen)
{
	return dm->ncr;
}

static void
ncr_write(DM9000 * dm, uint32_t value, uint8_t index, int rqlen)
{
	dm->ncr = value & ~1;
#if 1
	if (value & NCR_RST) {
		// should auto clear after 10 uS
		soft_reset(dm);	// doesn't work anymore with this !
	}
#endif
	if (value & NCR_EXT_PHY) {
		fprintf(stderr, "DM9000 ncr: External Phy not supported\n");
	}
}

/*
 * -----------------------------
 * Network status register
 * complete
 * -----------------------------
 */
static uint32_t
nsr_read(DM9000 * dm, uint8_t index, int rqlen)
{
	uint8_t clearbits = NSR_WAKEST | NSR_TX2_END | NSR_TX1_END;
	uint8_t retval = dm->nsr;
	dm->nsr &= ~clearbits;
	return retval;
}

static void
nsr_write(DM9000 * dm, uint32_t value, uint8_t index, int rqlen)
{
	uint8_t clearbits = value & (NSR_WAKEST | NSR_TX2_END | NSR_TX1_END);
	dm->nsr = dm->nsr & ~clearbits;
}

static uint32_t
tcr_read(DM9000 * dm, uint8_t index, int rqlen)
{
	return dm->tcr;
}

static void
tcr_write(DM9000 * dm, uint32_t value, uint8_t index, int rqlen)
{
	dm->tcr = value & ~TCR_TXREQ;
	if (value & TCR_TXREQ) {
		transmit_packet(dm);
	}
}

static uint32_t
tsr_i_read(DM9000 * dm, uint8_t index, int rqlen)
{
	return dm->tsr_i;
}

static void
tsr_i_write(DM9000 * dm, uint32_t value, uint8_t index, int rqlen)
{
	fprintf(stderr, "DM9000: TSR_I is not writable\n");
}

static uint32_t
tsr_ii_read(DM9000 * dm, uint8_t index, int rqlen)
{
	return dm->tsr_ii;
}

static void
tsr_ii_write(DM9000 * dm, uint32_t value, uint8_t index, int rqlen)
{
	fprintf(stderr, "DM9000: TSR_II is not writable\n");
}

static uint32_t
rcr_read(DM9000 * dm, uint8_t index, int rqlen)
{
	return dm->rcr;
}

static void
rcr_write(DM9000 * dm, uint32_t value, uint8_t index, int rqlen)
{
	uint32_t diff = dm->rcr ^ value;
	if (diff & value & RCR_RXEN) {
		/* Is sram[wp] zeroed on RXEN ? */
		clear_pktsrc(dm);
		enable_receiver(dm);
	} else {
		disable_receiver(dm);
	}
	dm->rcr = value & 0x3f;
}

static uint32_t
rsr_read(DM9000 * dm, uint8_t index, int rqlen)
{
	return dm->rsr;
}

static void
rsr_write(DM9000 * dm, uint32_t value, uint8_t index, int rqlen)
{
	fprintf(stderr, "DM9000: RSR is read only\n");
}

/*
 * ---------------------------------------------------------
 * ROCR is read and clear
 *	Documentation does not tell me if this is  
 *	Clear on read, or clear on write
 * ---------------------------------------------------------
 */
static uint32_t
rocr_read(DM9000 * dm, uint8_t index, int rqlen)
{
	uint8_t rocr = dm->rocr;
	dm->rocr = 0;
	fprintf(stderr, "Warning: ROCR implementation may be bad\n");
	return rocr;
}

static void
rocr_write(DM9000 * dm, uint32_t value, uint8_t index, int rqlen)
{
	fprintf(stderr, "Warning: ROCR implementation may be bad\n");
	dm->rocr = dm->rocr & value;
}

static uint32_t
bptr_read(DM9000 * dm, uint8_t index, int rqlen)
{
	return dm->bptr;
}

static void
bptr_write(DM9000 * dm, uint32_t value, uint8_t index, int rqlen)
{
	dm->bptr = value;
}

/*
 * -------------------------------------------------------------------------
 * Flow control Threshold register
 * Bits 0-3 LWOT: Watermark in kbytes when a Pause 0 packet is sent
 * Bits 4-7 HWOT: Watermark in kbytes when a pause 0xffff packet is sent
 * -------------------------------------------------------------------------
 */
static uint32_t
fctr_read(DM9000 * dm, uint8_t index, int rqlen)
{
	return dm->fctr;
}

static void
fctr_write(DM9000 * dm, uint32_t value, uint8_t index, int rqlen)
{
	dm->fctr = value;
}

/*
 * ------------------------------------------------------------------------
 * FCR Flow Control register
 * Bit 7: TXP0: Send a pause packet with 0x0000 (no pause)
 * Bit 6: TXPF: Send a pause packet with 0xffff
 * Bit 5: TXPEN: Force Enable transmitting of pause packets
 * Bit 4: BKPA: Backpressure mode for all packets (JAM, Half Duplex)
 * Bit 3: BKPM: Backpressure mode for matching packets (JAM, Half Duplex)
 * Bit 2: RXPS: Rx pause packet status latched
 * Bit 1: RXPCS: Pause packet current status of receiver (1=pause)
 * Bit 0: FLCE Enable Flow control (listen on pause packets)
 * ------------------------------------------------------------------------
 */
static uint32_t
fcr_read(DM9000 * dm, uint8_t index, int rqlen)
{
	uint8_t fcr = dm->fcr;
	dm->fcr &= ~FCR_RXPS;
	return fcr;
}

static void
fcr_write(DM9000 * dm, uint32_t value, uint8_t index, int rqlen)
{
	dm->fcr =
	    (dm->
	     fcr & (FCR_RXPS | FCR_RXPCS)) | (value & ~(FCR_RXPS | FCR_RXPCS | FCR_TXP0 |
							FCR_TXPF));
}

static uint32_t
epcr_read(DM9000 * dm, uint8_t index, int rqlen)
{
	//fprintf(stderr,"EPCR read %04x\n",dm->epcr); // jk
	return dm->epcr;
}

static void
epcr_write(DM9000 * dm, uint32_t value, uint8_t index, int rqlen)
{
	uint32_t diff = value ^ dm->epcr;
	dm->epcr = (value & ~EPCR_ERRE) | (dm->epcr & EPCR_ERRE);

	if (value & EPCR_EPOS) {
		PHY_Device *phy;
		int reg = dm->epar & 0x1f;
		uint16_t phyval;
		if (dm->ncr & NCR_EXT_PHY) {
			phy = dm->extphy[(dm->epar >> 6) & 3];
		} else {
			phy = &dm->iphy;
		}
		if (!phy) {
			dbgprintf("DM9000 EPCR: Phy not found\n");
			return;
		}
		/* This is not true in real device ! On
		 * write ERRE is cleared before it is ready 
		 */
		if (dm->epcr & EPCR_ERRE) {
			return;
		}
		if (diff & value & EPCR_ERPRR) {
			PHY_ReadRegister(phy, &phyval, reg);
			dm->epdrl = phyval & 0xff;
			dm->epdrh = (phyval >> 8);
			dm->epcr |= EPCR_ERRE;
			CycleTimer_Mod(&dm->epcr_busyTimer, MicrosecondsToCycles(20));
		} else if (diff & value & EPCR_ERPRW) {
			phyval = dm->epdrl | (dm->epdrh << 8);
			PHY_WriteRegister(phy, phyval, reg);
			dm->epcr |= EPCR_ERRE;
			CycleTimer_Mod(&dm->epcr_busyTimer, MicrosecondsToCycles(20));
		}
	} else {
		// eeprom
		int addr = (dm->epar & EPAR_EROA_MASK) >> EPAR_EROA_SHIFT;
		if (dm->epcr & EPCR_ERRE) {
			return;
		}
		if (diff & value & EPCR_ERPRR) {
			uint16_t val;
			if (dm->eeprom_valid || (addr > 2)) {
				val = m93c46_readw(dm->eprom, addr);
			} else {
				val = dm->randmac[2 * addr] | (dm->randmac[2 * addr + 1] << 1);
			}
			dm->epdrl = val & 0xff;
			dm->epdrh = val >> 8;
			dm->epcr |= EPCR_ERRE;
			CycleTimer_Mod(&dm->epcr_busyTimer, MicrosecondsToCycles(20));
		} else if (diff & value & EPCR_ERPRW) {
			uint16_t val = dm->epdrl | (dm->epdrh << 8);
			if (value & EPCR_WEP) {
				m93c46_writew(dm->eprom, val, addr);
				dm->epcr |= EPCR_ERRE;
				CycleTimer_Mod(&dm->epcr_busyTimer, MicrosecondsToCycles(20));
			} else {
				fprintf(stderr, "DM9000 emu: eeprom is writeprotected\n");
			}
		}
	}
	if (diff & value & EPCR_REEP) {
		if (dm->epcr & EPCR_ERRE) {
			return;
		}
		dm->epcr |= EPCR_ERRE;
		reload_eeprom(dm);
		CycleTimer_Mod(&dm->epcr_busyTimer, MicrosecondsToCycles(100));
	}
}

/*
 * EEProm and Phy address Register
 */
static uint32_t
epar_read(DM9000 * dm, uint8_t index, int rqlen)
{
	return dm->epar;
}

static void
epar_write(DM9000 * dm, uint32_t value, uint8_t index, int rqlen)
{
	dm->epar = value;
}

/*
 * EEProm and Phy data Register
 */
static uint32_t
epdrl_read(DM9000 * dm, uint8_t index, int rqlen)
{
	return dm->epdrl;
}

static void
epdrl_write(DM9000 * dm, uint32_t value, uint8_t index, int rqlen)
{
	dm->epdrl = value;
}

static uint32_t
epdrh_read(DM9000 * dm, uint8_t index, int rqlen)
{
	return dm->epdrh;
}

static void
epdrh_write(DM9000 * dm, uint32_t value, uint8_t index, int rqlen)
{
	dm->epdrh = value;
}

/*
 * --------------------------------------------------------------------
 * WCR
 * 	Documentation says not when the Status change bits are
 * 	cleared
 * --------------------------------------------------------------------
 */
static uint32_t
wcr_read(DM9000 * dm, uint8_t index, int rqlen)
{
	return dm->wcr;
}

static void
wcr_write(DM9000 * dm, uint32_t value, uint8_t index, int rqlen)
{
	dm->wcr = dm->wcr & (WCR_MAGICST | WCR_SAMPLEST | WCR_LINKST | 0xc0);
	dm->wcr |= value & (WCR_MAGICEN | WCR_SAMPLEEN | WCR_LINKEN);
}

/*
 * ----------------------------------------------------
 * Physical address registers
 * ----------------------------------------------------
 */
static uint32_t
par_read(DM9000 * dm, uint8_t index, int rqlen)
{
	unsigned int n = index - 0x10;
	uint32_t value = 0;
	if (n > 6) {
		fprintf(stderr, "DM9000 emulator bug in par_read\n");
		return 0;
	}
	value |= dm->par[n];
	return value;
}

static void
par_write(DM9000 * dm, uint32_t value, uint8_t index, int rqlen)
{
	unsigned int n = index - 0x10;
	dbgprintf("PAR write 0x%02x to n %d,rqlen %d\n", value, n, rqlen);
	if (n > 6) {
		fprintf(stderr, "DM9000 emulator bug in par_write\n");
		return;
	}
	/* fprintf(stderr,"Write PAR %d val 0x%02x\n",index,value); */
	dm->par[n] = value;
}

/*
 * ----------------------------------------------------
 * Multicast address registers
 * ----------------------------------------------------
 */

static uint32_t
mar_read(DM9000 * dm, uint8_t index, int rqlen)
{
	unsigned int n = index - 0x16;
	uint32_t value;
	if (n > 8) {
		fprintf(stderr, "DM9000 emulator bug in mar_read, index %d\n", index);
		return 0;
	}
	value = dm->mar[n];
	return value;
}

static void
mar_write(DM9000 * dm, uint32_t value, uint8_t index, int rqlen)
{
	unsigned int n = index - 0x16;
	if (n > 8) {
		fprintf(stderr, "DM9000 emulator bug in mar_write, index %d\n", index);
		return;
	}
	dm->mar[n] = value;
}

static uint32_t
gpcr_read(DM9000 * dm, uint8_t index, int rqlen)
{
	return dm->gpcr;
}

static void
gpcr_write(DM9000 * dm, uint32_t value, uint8_t index, int rqlen)
{
	dm->gpcr = value;
//        fprintf(stderr,"DM9000 GPCR not fully implemented value %02x\n",value);
}

static uint32_t
gpr_read(DM9000 * dm, uint8_t index, int rqlen)
{
	return dm->gpr;
}

static void
gpr_write(DM9000 * dm, uint32_t value, uint8_t index, int rqlen)
{
	dm->gpr = value;
	//       fprintf(stderr,"DM9000 GPR not fully implemented value %02x\n",value);
}

static uint32_t
trpal_read(DM9000 * dm, uint8_t index, int rqlen)
{
	return dm->txfifo_rp;
}

static void
trpal_write(DM9000 * dm, uint32_t value, uint8_t index, int rqlen)
{
	fprintf(stderr, "DM9000 TRPAL is not writable\n");
}

static uint32_t
trpah_read(DM9000 * dm, uint8_t index, int rqlen)
{
	return dm->txfifo_rp >> 8;
}

static void
trpah_write(DM9000 * dm, uint32_t value, uint8_t index, int rqlen)
{
	fprintf(stderr, "DM9000 TRPAH is not writable\n");
}

static uint32_t
rwpal_read(DM9000 * dm, uint8_t index, int rqlen)
{
	return dm->rxfifo_wp;
}

static void
rwpal_write(DM9000 * dm, uint32_t value, uint8_t index, int rqlen)
{
	fprintf(stderr, "DM9000 RWPAL is not writable");
}

static uint32_t
rwpah_read(DM9000 * dm, uint8_t index, int rqlen)
{
	return dm->rwpah >> 8;
}

static void
rwpah_write(DM9000 * dm, uint32_t value, uint8_t index, int rqlen)
{
	dbgprintf("DM9000 RWPAH is not writable\n");
}

static uint32_t
vidl_read(DM9000 * dm, uint8_t index, int rqlen)
{
	return dm->vidl;
}

static void
vidl_write(DM9000 * dm, uint32_t value, uint8_t index, int rqlen)
{
	fprintf(stderr, "DM9000: Vendor ID is not writable\n");
}

static uint32_t
vidh_read(DM9000 * dm, uint8_t index, int rqlen)
{
	return dm->vidh;
}

static void
vidh_write(DM9000 * dm, uint32_t value, uint8_t index, int rqlen)
{
	fprintf(stderr, "DM9000: Vendor ID is not writable\n");
}

static uint32_t
pidl_read(DM9000 * dm, uint8_t index, int rqlen)
{
	return dm->pidl;
}

static void
pidl_write(DM9000 * dm, uint32_t value, uint8_t index, int rqlen)
{
	fprintf(stderr, "DM9000: Product ID is not writable\n");
}

static uint32_t
pidh_read(DM9000 * dm, uint8_t index, int rqlen)
{
	return dm->pidh;
}

static void
pidh_write(DM9000 * dm, uint32_t value, uint8_t index, int rqlen)
{
	fprintf(stderr, "DM9000: Product ID is not writable\n");
}

static uint32_t
chipr_read(DM9000 * dm, uint8_t index, int rqlen)
{
	return dm->chipr;
}

static void
chipr_write(DM9000 * dm, uint32_t value, uint8_t index, int rqlen)
{
	dbgprintf("DM9000 CHIPR is not writable\n");
}

static uint32_t
smcr_read(DM9000 * dm, uint8_t index, int rqlen)
{
	dbgprintf("Not implemented 0x%02x\n", index);
	return 0;
}

static void
smcr_write(DM9000 * dm, uint32_t value, uint8_t index, int rqlen)
{
	dbgprintf("Not implemented 0x%02x\n", index);
}

/*
 * -------------------------------------------
 * Early transmit control/status register
 * -------------------------------------------
 */
static uint32_t
etxcsr_read(DM9000 * dm, uint8_t index, int rqlen)
{
	fprintf(stderr, "DM9000: Not implemented 0x%02x\n", index);
	return 0;
}

static void
etxcsr_write(DM9000 * dm, uint32_t value, uint8_t index, int rqlen)
{
	dm->etxcsr = value & 0x83;
	fprintf(stderr, "DM9000: ETXCSR has no effect\n");
}

/*
 * ---------------------------------------
 * Check sum control register
 * ---------------------------------------
 */
static uint32_t
tcscr_read(DM9000 * dm, uint8_t index, int rqlen)
{
	fprintf(stderr, "DM9000: Not implemented 0x%02x\n", index);
	return 0;
}

static void
tcscr_write(DM9000 * dm, uint32_t value, uint8_t index, int rqlen)
{
	dbgprintf("DM9000: Checksum control register not implemented\n", index);
}

/*
 * -------------------------------------------------------------
 * Receive Check sum status Register
 * -------------------------------------------------------------
 */
static uint32_t
rcscsr_read(DM9000 * dm, uint8_t index, int rqlen)
{
	fprintf(stderr, "DM9000: Not implemented 0x%02x\n", index);
	return 0;
}

static void
rcscsr_write(DM9000 * dm, uint32_t value, uint8_t index, int rqlen)
{
	dbgprintf("DM9000: Not implemented 0x%02x\n", index);
}

/*
 * ----------------------------------------------------------
 * MII PHY Adress Register
 * ----------------------------------------------------------
 */
static uint32_t
mpar_read(DM9000 * dm, uint8_t index, int rqlen)
{
	fprintf(stderr, "DM9000: Not implemented 0x%02x\n", index);
	return 0;
}

static void
mpar_write(DM9000 * dm, uint32_t value, uint8_t index, int rqlen)
{
	dbgprintf("DM9000: Not implemented 0x%02x\n", index);
}

/*
 * ---------------------------------------------------------
 * Led Pin control register. Switch between GPIO mode and
 * SMII mode 
 * ---------------------------------------------------------
 */
static uint32_t
ledcr_read(DM9000 * dm, uint8_t index, int rqlen)
{
	fprintf(stderr, "DM9000: Not implemented 0x%02x\n", index);
	return 0;
}

static void
ledcr_write(DM9000 * dm, uint32_t value, uint8_t index, int rqlen)
{
	dbgprintf("DM9000: Not implemented 0x%02x\n", index);
}

static uint32_t
buscr_read(DM9000 * dm, uint8_t index, int rqlen)
{
	return dm->buscr;
}

static void
buscr_write(DM9000 * dm, uint32_t value, uint8_t index, int rqlen)
{
	dm->buscr = value;
}

static uint32_t
intcr_read(DM9000 * dm, uint8_t index, int rqlen)
{
	return dm->intcr;
}

static void
intcr_write(DM9000 * dm, uint32_t value, uint8_t index, int rqlen)
{

	switch (value & 3) {
	    case 0:
		    dm->int_active = SIG_HIGH;
		    dm->int_inactive = SIG_LOW;
		    break;
	    case 1:
		    dm->int_active = SIG_LOW;
		    dm->int_inactive = SIG_HIGH;
		    break;

	    case 2:
		    dm->int_active = SIG_OPEN;
		    dm->int_inactive = SIG_LOW;
		    break;

	    case 3:
		    dm->int_active = SIG_LOW;
		    dm->int_inactive = SIG_OPEN;
		    break;
	}
	dm->intcr = value & 3;
}

static uint32_t
sccr_read(DM9000 * dm, uint8_t index, int rqlen)
{
	fprintf(stderr, "DM9000: Not implemented 0x%02x\n", index);
	return 0;
}

static void
sccr_write(DM9000 * dm, uint32_t value, uint8_t index, int rqlen)
{
	dbgprintf("DM9000: Not implemented 0x%02x\n", index);
}

static uint32_t
rsccr_read(DM9000 * dm, uint8_t index, int rqlen)
{
	fprintf(stderr, "DM9000: Not implemented 0x%02x\n", index);
	return 0;
}

static void
rsccr_write(DM9000 * dm, uint32_t value, uint8_t index, int rqlen)
{
	dbgprintf("DM9000: Not implemented 0x%02x\n", index);
}

/*
 * -------------------------------------------------------------------
 * MRCMDX returns the value read from SRAM on previous Cycle !
 * It does not update the cached value from SRAM when previous cycle
 * had the same index
 * -------------------------------------------------------------------
 */
static uint32_t
mrcmdx_read(DM9000 * dm, uint8_t index, int rqlen)
{
	int i;
	uint32_t rp = dm->rxfifo_rp;
	uint32_t value = 0;
	uint32_t retval;
	/* 
	 * -------------------------------------------
	 * If index did not change since last read of
	 * mrcmdx an outdated value is returned
	 * -------------------------------------------
	 */
	if (dm->mrcmdx_idx_tag == dm->index_chg_cnt) {
		return dm->mrcmd_latch;
	}
	for (i = 0; i < dm->buswidth; i++) {
		value = value | (dm->sram[rp] << (i * 8));
		rp = (rp + 1);
		if (rp >= 16384) {
			rp = 3 * 1024;
		}
	}
	dm->mrcmdx_idx_tag = dm->index_chg_cnt;
	/* 
	 * ------------------------------------------------------
	 * Return the value which was latched by previous read
	 * and store the new version in the latch
	 * ------------------------------------------------------
	 */
	retval = dm->mrcmd_latch;
	dm->mrcmd_latch = value;
	return retval;
}

static void
mrcmdx_write(DM9000 * dm, uint32_t value, uint8_t index, int rqlen)
{
	fprintf(stderr, "DM9000: mrcmdx register is not writeable\n");
}

/*
 * ---------------------------------------------------------------------------
 * MRCMD: Read from rx fifo 
 * ---------------------------------------------------------------------------
 */
static uint32_t
mrcmd_read(DM9000 * dm, uint8_t index, int rqlen)
{
	int i;
	uint32_t rp = dm->rxfifo_rp;
	uint32_t value = 0;
	uint32_t retval;
	rp = rp + dm->buswidth;
	if (rp >= 16384) {
		rp = 3 * 1024 + (rp & (dm->buswidth - 1));
	}
	dm->rxfifo_rp = rp;
	for (i = 0; i < dm->buswidth; i++) {
		value = value | (dm->sram[rp] << (i * 8));
		rp = (rp + 1);
		dm->rxfifo_count--;
		if (rp >= 16384) {
			rp = 3 * 1024;
		}
	}
	/* 
	 * ---------------------------------------------------------
	 * Return the value which was latched on a previous read 
	 * and store the next value from fifo in the latch.
	 * ---------------------------------------------------------
	 */
	retval = dm->mrcmd_latch;
	dm->mrcmd_latch = value;
	update_receiver_status(dm);
	return retval;
}

static void
mrcmd_write(DM9000 * dm, uint32_t value, uint8_t index, int rqlen)
{
	fprintf(stderr, "DM9000 MRCMD: Register is not writable\n");
}

static uint32_t
mrrl_read(DM9000 * dm, uint8_t index, int rqlen)
{
	return dm->rxfifo_rp;
}

static void
mrrl_write(DM9000 * dm, uint32_t value, uint8_t index, int rqlen)
{
	dm->rxfifo_rp = (dm->rxfifo_rp & 0xff00) | value;
	update_rxfifo_count(dm);
	update_receiver_status(dm);
}

static uint32_t
mrrh_read(DM9000 * dm, uint8_t index, int rqlen)
{
	return dm->rxfifo_rp >> 8;
}

static void
mrrh_write(DM9000 * dm, uint32_t value, uint8_t index, int rqlen)
{
	dm->rxfifo_rp = (dm->rxfifo_rp & 0xff) | (value << 8);
	update_rxfifo_count(dm);
	update_receiver_status(dm);
}

static uint32_t
mwcmdx_read(DM9000 * dm, uint8_t index, int rqlen)
{
	fprintf(stderr, "DM9000: MWCMDX register is write only\n");
	return 0;
}

static void
mwcmdx_write(DM9000 * dm, uint32_t value, uint8_t index, int rqlen)
{
	int i;
	uint32_t wp = dm->txfifo_wp;
	for (i = 0; i < dm->buswidth; i++) {
		dm->sram[wp] = (value >> (i * 8)) & 0xff;
		wp = (wp + 1) % (3 * 1024);
	}
	if (rqlen > 1) {
		fprintf(stderr, "Warning: Endianness of mwcmdx write unclear\n");
	}
}

static uint32_t
mwcmd_read(DM9000 * dm, uint8_t index, int rqlen)
{
	fprintf(stderr, "DM9000: MWCMD register is write only\n");
	return 0;
}

/*
 * -------------------------------------------------
 * mwcmd_write
 * -------------------------------------------------
 */
static void
mwcmd_write(DM9000 * dm, uint32_t value, uint8_t index, int rqlen)
{
	int i;
	for (i = 0; i < dm->buswidth; i++) {
		dm->sram[dm->txfifo_wp] = (value >> (i * 8)) & 0xff;
		dm->txfifo_wp = (dm->txfifo_wp + 1) % (3 * 1024);
	}
}

static uint32_t
mwrl_read(DM9000 * dm, uint8_t index, int rqlen)
{
	return dm->txfifo_wp;
}

static void
mwrl_write(DM9000 * dm, uint32_t value, uint8_t index, int rqlen)
{
	dm->txfifo_wp = (dm->txfifo_wp & 0xff) | value;
}

static uint32_t
mwrh_read(DM9000 * dm, uint8_t index, int rqlen)
{
	return dm->txfifo_wp >> 8;
}

static void
mwrh_write(DM9000 * dm, uint32_t value, uint8_t index, int rqlen)
{
	dm->txfifo_wp = (dm->txfifo_wp & 0xff) | (value << 8);
}

static uint32_t
txpll_read(DM9000 * dm, uint8_t index, int rqlen)
{
	return dm->txpll;
}

static void
txpll_write(DM9000 * dm, uint32_t value, uint8_t index, int rqlen)
{
	dm->txpll = value;
}

static uint32_t
txplh_read(DM9000 * dm, uint8_t index, int rqlen)
{
	return dm->txplh;
}

static void
txplh_write(DM9000 * dm, uint32_t value, uint8_t index, int rqlen)
{
	dm->txplh = value;
}

static uint32_t
isr_read(DM9000 * dm, uint8_t index, int rqlen)
{
	return dm->isr;
}

static void
isr_write(DM9000 * dm, uint32_t value, uint8_t index, int rqlen)
{
	uint32_t clearbits = (value & 0xf);
	dm->isr &= ~clearbits;
	dbgprintf("Isr: cleared bits 0x%08x, isr 0x%08x\n", clearbits, dm->isr);
	update_interrupts(dm);
}

static uint32_t
imr_read(DM9000 * dm, uint8_t index, int rqlen)
{
	return dm->imr;
}

static void
imr_write(DM9000 * dm, uint32_t value, uint8_t index, int rqlen)
{
	uint32_t diff = value ^ dm->imr;
	dm->imr = (dm->imr & ~0x8f) | (value & 0x8f);
	/* DM9000 seems to set rxfifo_rp only on positive edge of PAR ? */
	if ((dm->imr & IMR_PAR) && (diff & IMR_PAR)) {
		dm->rxfifo_rp = (dm->rxfifo_rp & 0xff) | 0x0c00;
		update_rxfifo_count(dm);
		update_receiver_status(dm);
	}
	update_interrupts(dm);
}

void
DMReg_New(uint8_t index, DMReadProc rproc, DMWriteProc * wproc, DM9000 * dm)
{
	dm->read_reg[index] = rproc;
	dm->write_reg[index] = wproc;
}

static void
create_registers(DM9000 * dm)
{
	DMReg_New(DM_NCR, ncr_read, ncr_write, dm);
	DMReg_New(DM_NSR, nsr_read, nsr_write, dm);
	DMReg_New(DM_TCR, tcr_read, tcr_write, dm);
	DMReg_New(DM_TSR_I, tsr_i_read, tsr_i_write, dm);
	DMReg_New(DM_TSR_II, tsr_ii_read, tsr_ii_write, dm);
	DMReg_New(DM_RCR, rcr_read, rcr_write, dm);
	DMReg_New(DM_RSR, rsr_read, rsr_write, dm);
	DMReg_New(DM_ROCR, rocr_read, rocr_write, dm);
	DMReg_New(DM_BPTR, bptr_read, bptr_write, dm);
	DMReg_New(DM_FCTR, fctr_read, fctr_write, dm);
	DMReg_New(DM_FCR, fcr_read, fcr_write, dm);
	DMReg_New(DM_EPCR, epcr_read, epcr_write, dm);
	DMReg_New(DM_EPAR, epar_read, epar_write, dm);
	DMReg_New(DM_EPDRL, epdrl_read, epdrl_write, dm);
	DMReg_New(DM_EPDRH, epdrh_read, epdrh_write, dm);
	DMReg_New(DM_WCR, wcr_read, wcr_write, dm);
	DMReg_New(DM_PAR0, par_read, par_write, dm);
	DMReg_New(DM_PAR1, par_read, par_write, dm);
	DMReg_New(DM_PAR2, par_read, par_write, dm);
	DMReg_New(DM_PAR3, par_read, par_write, dm);
	DMReg_New(DM_PAR4, par_read, par_write, dm);
	DMReg_New(DM_PAR5, par_read, par_write, dm);
	DMReg_New(DM_MAR0, mar_read, mar_write, dm);
	DMReg_New(DM_MAR1, mar_read, mar_write, dm);
	DMReg_New(DM_MAR2, mar_read, mar_write, dm);
	DMReg_New(DM_MAR3, mar_read, mar_write, dm);
	DMReg_New(DM_MAR4, mar_read, mar_write, dm);
	DMReg_New(DM_MAR5, mar_read, mar_write, dm);
	DMReg_New(DM_MAR6, mar_read, mar_write, dm);
	DMReg_New(DM_MAR7, mar_read, mar_write, dm);
	DMReg_New(DM_GPCR, gpcr_read, gpcr_write, dm);
	DMReg_New(DM_GPR, gpr_read, gpr_write, dm);
	DMReg_New(DM_TRPAL, trpal_read, trpal_write, dm);
	DMReg_New(DM_TRPAH, trpah_read, trpah_write, dm);
	DMReg_New(DM_RWPAL, rwpal_read, rwpal_write, dm);
	DMReg_New(DM_RWPAH, rwpah_read, rwpah_write, dm);
	DMReg_New(DM_VID0, vidl_read, vidl_write, dm);
	DMReg_New(DM_VID1, vidh_read, vidh_write, dm);
	DMReg_New(DM_PID0, pidl_read, pidl_write, dm);
	DMReg_New(DM_PID1, pidh_read, pidh_write, dm);
	DMReg_New(DM_CHIPR, chipr_read, chipr_write, dm);
	DMReg_New(DM_SMCR, smcr_read, smcr_write, dm);

	/* Only new revision */
	DMReg_New(DM_ETXCSR, etxcsr_read, etxcsr_write, dm);
	DMReg_New(DM_TCSCR, tcscr_read, tcscr_write, dm);
	DMReg_New(DM_RCSCSR, rcscsr_read, rcscsr_write, dm);
	DMReg_New(DM_MPAR, mpar_read, mpar_write, dm);
	DMReg_New(DM_LEDCR, ledcr_read, ledcr_write, dm);
	DMReg_New(DM_BUSCR, buscr_read, buscr_write, dm);
	DMReg_New(DM_INTCR, intcr_read, intcr_write, dm);
	DMReg_New(DM_SCCR, sccr_read, sccr_write, dm);
	DMReg_New(DM_RSCCR, rsccr_read, rsccr_write, dm);

	DMReg_New(DM_MRCMDX, mrcmdx_read, mrcmdx_write, dm);
	DMReg_New(DM_MRCMD, mrcmd_read, mrcmd_write, dm);
	DMReg_New(DM_MRRL, mrrl_read, mrrl_write, dm);
	DMReg_New(DM_MRRH, mrrh_read, mrrh_write, dm);
	DMReg_New(DM_MWCMDX, mwcmdx_read, mwcmdx_write, dm);
	DMReg_New(DM_MWCMD, mwcmd_read, mwcmd_write, dm);
	DMReg_New(DM_MWRL, mwrl_read, mwrl_write, dm);
	DMReg_New(DM_MWRH, mwrh_read, mwrh_write, dm);
	DMReg_New(DM_TXPLL, txpll_read, txpll_write, dm);
	DMReg_New(DM_TXPLH, txplh_read, txplh_write, dm);
	DMReg_New(DM_ISR, isr_read, isr_write, dm);
	DMReg_New(DM_IMR, imr_read, imr_write, dm);
}

static void
index_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	DM9000 *dm = (DM9000 *) clientData;
	if (dm->index_reg != value) {
		dm->index_reg = value;
		dm->index_chg_cnt++;
	}
}

static uint32_t
index_read(void *clientData, uint32_t address, int rqlen)
{
	DM9000 *dm = (DM9000 *) clientData;
	return dm->index_reg;
}

static void
data_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	DM9000 *dm = (DM9000 *) clientData;
	if (dm->write_reg[dm->index_reg]) {
		dm->write_reg[dm->index_reg] (dm, value, dm->index_reg, rqlen);
	} else {
		fprintf(stderr, "DM9000 write: No such register: %02x\n", dm->index_reg);
		return;
	}
}

static uint32_t
data_read(void *clientData, uint32_t address, int rqlen)
{
	DM9000 *dm = (DM9000 *) clientData;
	if (dm->read_reg[dm->index_reg]) {
		return dm->read_reg[dm->index_reg] (dm, dm->index_reg, rqlen);
	} else {
		fprintf(stderr, "DM9000 read: No such register: %02x\n", dm->index_reg);
		return 0;
	}
}

static void
DM9000_Map(void *owner, uint32_t base, uint32_t mask, uint32_t flags)
{
	DM9000 *dm = (DM9000 *) owner;
	if (dm->register_spacing >= 2) {
		IOH_New16(base + 0, index_read, index_write, dm);
		IOH_New16(base + dm->register_spacing, data_read, data_write, dm);
	} else {
		IOH_New8(base + 0, index_read, index_write, dm);
		IOH_New8(base + dm->register_spacing, data_read, data_write, dm);
	}
}

static void
DM9000_UnMap(void *owner, uint32_t base, uint32_t mask)
{
	DM9000 *dm = (DM9000 *) owner;
	if (dm->register_spacing >= 2) {
		IOH_Delete16(base);
		IOH_Delete16(base + dm->register_spacing);
	} else {
		IOH_Delete8(base);
		IOH_Delete8(base + dm->register_spacing);
	}
}

static int
iphy_read(PHY_Device * phy, uint16_t * value, int reg)
{
	DM9000 *dm = (DM9000 *) phy->owner;
	switch (reg) {
	    case DMP_CONTROL:
		    *value = dm->control;
		    break;

	    case DMP_STATUS:
		    *value = dm->status;
		    break;

	    case DMP_PHY_ID1:
		    *value = dm->phy_id1;
		    break;

	    case DMP_PHY_ID2:
		    *value = dm->phy_id2;
		    break;

	    case DMP_AN_ADVERTISE:
		    *value = dm->an_advertise;
		    break;
	    case DMP_AN_LP_ABILITY:
		    *value = dm->an_lp_ability;
		    break;
	    case DMP_AN_EXPANSION:
		    *value = dm->an_expansion;
		    break;
	    case DMP_DSCR:
		    *value = dm->dscr;
		    break;

	    case DMP_DSCSR:
		    *value = dm->dscsr;
		    break;

	    case DMP_BTCSR10:
		    *value = dm->btcsr10;
		    break;
	    default:
		    fprintf(stderr, "Illegal Phy register 0x%02x\n", reg);
		    return -1;
	}
	return 0;
}

static int
iphy_write(PHY_Device * phy, uint16_t value, int reg)
{
	DM9000 *dm = (DM9000 *) phy->owner;
	switch (reg) {
	    case DMP_CONTROL:
		    dm->control = value;
		    dbgprintf("write to phy reg 0x%02x value 0x%04x\n", reg, value);
		    break;

	    case DMP_STATUS:
		    fprintf(stderr, "write to phy reg 0x%02x not implemented\n", reg);
		    break;

	    case DMP_PHY_ID1:
		    fprintf(stderr, "write to phy reg 0x%02x not implemented\n", reg);
		    break;

	    case DMP_PHY_ID2:
		    fprintf(stderr, "write to phy reg 0x%02x not implemented\n", reg);
		    break;

	    case DMP_AN_ADVERTISE:
		    fprintf(stderr, "write to phy reg 0x%02x not implemented\n", reg);
		    break;

	    case DMP_AN_LP_ABILITY:
		    fprintf(stderr, "write to phy reg 0x%02x not implemented\n", reg);
		    break;

	    case DMP_AN_EXPANSION:
		    fprintf(stderr, "write to phy reg 0x%02x not implemented\n", reg);
		    break;

	    case DMP_DSCR:
		    fprintf(stderr, "write to phy reg 0x%02x not implemented\n", reg);
		    break;

	    case DMP_DSCSR:
		    fprintf(stderr, "write to phy reg 0x%02x not implemented\n", reg);
		    break;

	    case DMP_BTCSR10:
		    fprintf(stderr, "write to phy reg 0x%02x not implemented\n", reg);
		    break;

	    default:
		    fprintf(stderr, "Illegal Phy register 0x%02x\n", reg);
		    return -1;
	}
	return 0;
}

/*
 * --------------------------------------------------------------------------
 * Timer handler, called when access to external phy or eeprom is completed
 * --------------------------------------------------------------------------
 */
void
epy_access_done(void *clientData)
{
	DM9000 *dm = (DM9000 *) clientData;
	dm->epcr = dm->epcr & ~EPCR_ERRE;
}

BusDevice *
DM9000_New(const char *devname, int register_spacing)
{
	DM9000 *dm = sg_new(DM9000);
	char *epromname = (char *)alloca(strlen(devname) + 20);
	dm->ether_fd = Net_CreateInterface(devname);
	dm->input_fh = AsyncManager_PollInit(dm->ether_fd);

	dm->bdev.first_mapping = NULL;
	dm->bdev.Map = DM9000_Map;
	dm->bdev.UnMap = DM9000_UnMap;
	dm->bdev.owner = dm;
	dm->bdev.hw_flags = MEM_FLAG_WRITABLE | MEM_FLAG_READABLE;
	create_registers(dm);

	dm->irqNode = SigNode_New("%s.irq", devname);
	if (!dm->irqNode) {
		fprintf(stderr, "DM9000: Can not create IrqRequest Node for dev %s\n", devname);
		exit(3425);
	}
	dm->buswidth = 2;
	soft_reset(dm);
	dm->control = 0x3100;
	dm->status = 0x782d;	// Link up, negotiation complete
	dm->phy_id1 = 0x0181;
	dm->phy_id2 = 0xb8c0;
	dm->an_advertise = 0x01e1;
	dm->an_lp_ability = 0x45e1;
	dm->an_expansion = 0;
	dm->dscr = 0x410;
	dm->buscr = 0x61;
	dm->dscsr = 0x8010;
	dm->btcsr10 = 0x7800;
	sprintf(epromname, "%s.eeprom", devname);
	dm->eprom = m93c46_New(epromname);
	/* Phy registers */

	dm->iphy.owner = dm;
	dm->iphy.readreg = iphy_read;
	dm->iphy.writereg = iphy_write;
	dm->register_spacing = register_spacing;
	CycleTimer_Init(&dm->epcr_busyTimer, epy_access_done, dm);
	generate_random_hwaddr(dm->randmac);	/* only used when eeprom invalid */
	if (reload_eeprom(dm) < 0) {
		fprintf(stderr,
			"DM9000 Warning: eeprom contents is invalid. Using a Random MAC address\n");
		sleep(3);
	}

	fprintf(stderr, "DM9000 Ethernet Controller created with register spacing %d bytes\n",
		dm->register_spacing);
	return &dm->bdev;
}

INITIALIZER(dm9000_init)
{
	fprintf(stderr, "Davicom DM9000 module loaded, built %s %s\n", __DATE__, __TIME__);
}
