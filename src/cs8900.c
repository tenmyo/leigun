/*
 ***************************************************************************************************
 *
 * Emulation of Cirrus Logic Crystal LAN CS8900A Ethernet controller 
 *	
 * State:
 *	Working with the linux-2.6.15.3 cs89x0 driver and with u-boot-1.1.4
 *	Not recommended to use with any other driver
 *	Memory mapped mode and dma mode is missing
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
#include <unistd.h>
#include <fcntl.h>
#include "bus.h"
#include "signode.h"
#include "linux-tap.h"
#include "m93c46.h"
#include "cs8900.h"
#include "sgstring.h"

#define IO_RXTXDATA0(base)	((base) + 0x00)
#define IO_RXTXDATA1(base)	((base) + 0x02)
#define IO_TXCMD(base)		((base) + 0x04)
#define IO_TXLENGTH(base)	((base) + 0x06)
#define IO_ISQ(base)		((base) + 0x08)
#define IO_PPTR(base)		((base) + 0x0a)
#define IO_PDATA0(base)		((base) + 0x0c)
#define IO_PDATA1(base)		((base) + 0x0e)

#define MEM_PROD_ID(base) 		((base)+0x00)
#define MEM_IOBASE(base)		((base)+0x20)
#define MEM_INTSEL(base)		((base)+0x22)
#define MEM_DMACHAN(base)		((base)+0x24)
#define MEM_DMASOF(base)		((base)+0x26)
#define MEM_DMAFC(base)			((base)+0x28)
#define MEM_RXDMABC(base)		((base)+0x2a)
#define MEM_MEMBASE(base)		((base)+0x2c)
#define MEM_BPROMBASE(base)		((base)+0x30)
#define	MEM_BPROMAMASK(base)		((base)+0x34)
#define MEM_EEPROMCMD(base)		((base)+0x40)
#define MEM_EEPROMDATA(base)		((base)+0x42)
#define MEM_RCVFBC(base)		((base)+0x50)
#define	MEM_RXCFG(base)			((base)+0x102)
#define		RXCFG_ADDR		(0x3)
#define		RXCFG_SKIP_1		(1<<6)
#define		RXCFG_STREAME		(1<<7)
#define		RXCFG_RXOKIE		(1<<8)
#define		RXCFG_RXDMAONLY 	(1<<9)
#define		RXCFG_AUTORXDMAE	(1<<10)
#define		RXCFG_BUFFERCRC		(1<<11)
#define		RXCFG_CRCERRORIE	(1<<12)
#define		RXCFG_RUNTIE		(1<<13)
#define		RXCFG_EXTRADATAIE	(1<<14)
#define	MEM_RXCTL(base)			((base)+0x104)
#define		RXCTL_ADDR		(0x5)
#define		RXCTL_IAHASHA		(1<<6)
#define		RXCTL_PROMISCUOSA	(1<<7)
#define		RXCTL_RXOKA		(1<<8)
#define		RXCTL_MULTICASTA	(1<<9)
#define		RXCTL_INDIVIDUALA	(1<<10)
#define		RXCTL_BROADCASTA	(1<<11)
#define		RXCTL_CRCERRORA		(1<<12)
#define		RXCTL_RUNTA		(1<<13)
#define		RXCTL_EXTRADATAA	(1<<14)
#define MEM_TXCFG(base)			((base)+0x106)
#define		TXCFG_ADDR		(0x7)
#define		TXCFG_LOSSOFCRSIE	(1<<6)
#define		TXCFG_SQERRORIE		(1<<7)
#define		TXCFG_TXOKIE		(1<<8)
#define		TXCFG_OOWIE		(1<<9)
#define		TXCFG_JABBERIE		(1<<10)
#define		TXCFG_ANYCOLLIE		(1<<11)
#define		TXCFG_16COLLIE		(1<<15)
#define MEM_TXCMDSTAT(base)		((base)+0x108)
#define		TXCMDSTAT_ADDR		(0x9)
#define MEM_BUFCFG(base)		((base)+0x10a)
#define		BUFCFG_ADDR		(0xb)
#define		BUFCFG_SWINTX		(1<<6)
#define		BUFCFG_RXDMAIE		(1<<7)
#define		BUFCFG_RDY4TXIE		(1<<8)
#define		BUFCFG_TXUNDERIE	(1<<9)
#define		BUFCFG_RXMISSIE		(1<<10)
#define		BUFCFG_RX128IE		(1<<11)
#define		BUFCFG_TXCOLOVFLOIE	(1<<12)
#define		BUFCFG_MISSOVFLOIE	(1<<13)
#define		BUFCFG_RXDESTIE		(1<<15)

#define	MEM_LINECTL(base)		((base)+0x112)
#define		LINECTL_ADDR		(0x13)
#define		LINECTL_SERRXON		(1<<6)
#define		LINECTL_SERTXON		(1<<7)
#define		LINECTL_AUIONLY		(1<<8)
#define		LINECTL_AUTOAUI10BT	(1<<9)
#define		LINECTL_MODBACKOFFE	(1<<11)
#define		LINECTL_POLARITYDIS	(1<<12)
#define		LINECTL_2PARTDEFDIS	(1<<13)
#define		LINECTL_LORXSQUELCH	(1<<14)
#define	MEM_SELFCTL(base)		((base)+0x114)
#define		SELFCTL_ADDR		(0x15)
#define		SELFCTL_RESET		(1<<6)
#define		SELFCTL_SWSUSP		(1<<8)
#define		SELFCTL_HWSLEEPE	(1<<9)
#define		SELFCTL_HWSTANDBY	(1<<10)
#define		SELFCTL_HC0E		(1<<12)
#define		SELFCTL_HC1E		(1<<13)
#define		SELFCTL_HCB0		(1<<14)
#define		SELFCTL_HCB1		(1<<15)

#define	MEM_BUSCTL(base)		((base)+0x116)
#define		BUSCTL_ADDR		(0x17)
#define		BUSCTL_RESETRXDMA	(1<<6)
#define 	BUSCTL_DMAEXTEND	(1<<8)
#define		BUSCTL_USESA		(1<<9)
#define		BUSCTL_MEMORYE		(1<<10)
#define		BUSCTL_DMABURST		(1<<11)
#define		BUSCTL_IOCHRDYE		(1<<12)
#define		BUSCTL_RXDMASIZE	(1<<13)
#define		BUSCTL_ENABLEIRQ	(1<<15)

#define MEM_TESTCTL(base)		((base)+0x118)
#define		TESTCTL_ADDR		(0x19)
#define		TESTCTL_DISABLELT	(1<<7)
#define		TESTCTL_ENDEC_LOOP	(1<<9)
#define 	TESTCTL_AUI_LOOP	(1<<10)
#define		TESTCTL_DISBACKOFF	(1<<11)
#define		TESTCTL_FDX		(1<<14)
#define MEM_STATEV(base)		((base)+0x120)
#define	MEM_RXEVENT(base)		((base)+0x124)
#define		RXEVENT_ADDR		(4)
#define		RXEVENT_IAHASH		(1<<6)
#define 	RXEVENT_DRIBBLEBITS	(1<<7)
#define		RXEVENT_RXOK		(1<<8)
#define		RXEVENT_HASHED		(1<<9)
#define		RXEVENT_INDADDR 	(1<<10)
#define		RXEVENT_BROADCAST	(1<<11)
#define		RXEVENT_CRCERROR	(1<<12)
#define		RXEVENT_RUNT		(1<<13)
#define		RXEVENT_EXTRADATA	(1<<14)

#define MEM_BUFEVENT(base)		((base)+0x12c)
#define		BUFEVENT_ADDR		(0xc)
#define		BUFEVENT_SWINT		(1<<6)
#define		BUFEVENT_RXDMAFRAME	(1<<7)
#define		BUFEVENT_RDY4TX		(1<<8)
#define		BUFEVENT_TXUNDER	(1<<9)
#define		BUFEVENT_RXMISS		(1<<10)
#define		BUFEVENT_RX128		(1<<11)
#define		BUFEVENT_RXDEST		(1<<15)
#define MEM_RXMISS(base)		((base)+0x130)
#define		RXMISS_ADDR		(0x10)
#define MEM_TXCOL(base)			((base)+0x132)
#define		TXCOL_ADDR		(0x12)
#define MEM_TXEVENT(base)		((base)+0x128)
#define 	TXEVENT_ADDR		(8)
#define		TXEVENT_LOSS_OF_CRS	(1<<6)
#define		TXEVENT_SQERR		(1<<7)
#define		TXEVENT_TXOK		(1<<8)
#define		TXEVENT_OOW		(1<<9)
#define		TXEVENT_JABBER		(1<<10)
#define		TXEVENT_NUMCOLS_MASK	(0xf<<11)
#define		TXEVENT_NUMCOLS_SHIFT	(11)
#define		TXEVENT_16COL		(1<<15)
#define	MEM_LINEST(base)		((base)+0x134)
#define		LINEST_ADDR		(0x14)
#define		LINEST_LINKOK		(1<<7)
#define		LINEST_AUI		(1<<8)
#define		LINEST_10BT		(1<<9)
#define		LINEST_POLOK		(1<<0xc)
#define		LINEST_CRS		(1<<0xe)
#define	MEM_SELFST(base)		((base)+0x136)
#define		SELFST_ADDR		(0x16)
#define		SELFST_3V3_ACT		(1<<6)
#define		SELFST_INITD		(1<<7)
#define		SELFST_SIBUSY		(1<<8)
#define		SELFST_EEPRESENT	(1<<9)
#define		SELFST_EEOK		(1<<10)
#define		SELFST_ELPRESENT	(1<<11)
#define		SELFST_EESIZE		(1<<12)
#define	MEM_BUSST(base)			((base)+0x138)
#define		BUSST_ADDR		(0x18)
#define		BUSST_TXBIDERR		(1<<7)
#define		BUSST_RDY4TXNOW		(1<<8)

#define	MEM_AUITDR(base)		((base)+0x13c)
#define		AUITDR_ADDR		(0x1c)
#define		AUITDR_DELAY_MASK 	(0xffc0)
#define		AUITDR_DELAY_SHIFT 	(6)
#define	MEM_TXCMD(base)			((base)+0x144)
#define MEM_TXLENGTH(base)		((base)+0x146)
#define MEM_LAF(base)			((base)+0x150)
#define MEM_INDADDR(base)		((base)+0x158)
#define MEM_RXSTAT(base)		((base)+0x400)
#define MEM_RXLENGTH(base)		((base)+0x402)
#define MEM_RXFRAME(base)		((base)+0x404)
#define MEM_TXFRAME(base)		((base)+0xA00)

typedef struct CS8900 {
	BusDevice bdev;
	int ether_fd;
	FIO_FileHandler input_fh;
	int pktsrc_is_enabled;
	int interrupt_posted;
	M93C46 *eeprom;
	int ee_write_enabled;

	uint16_t iobase;
	uint16_t intsel;
	uint16_t dmachan;
	uint16_t dmasof;
	uint16_t dmafc;
	uint16_t rxdmabc;
	uint32_t membase;
	uint32_t bprombase;
	uint32_t bpromamask;
	uint16_t eepromcmd;
	uint16_t eepromdata;
	uint16_t rcvfbc;
	uint16_t rxcfg;
	uint16_t rxctl;
	uint16_t txcfg;
	uint16_t rxevent;
	uint16_t bufevent;
	uint16_t rxmiss;
	uint16_t txcol;
	uint16_t txevent;
	uint16_t linest;
	uint16_t selfst;
	uint16_t busst;
	uint16_t auitdr;
	uint16_t txcmdstat;
	uint16_t bufcfg;
	uint16_t linectl;
	uint16_t selfctl;
	uint16_t busctl;
	uint16_t testctl;
	uint16_t txlength;
	uint8_t laf[8];
	uint8_t indaddr[6];

	/* For IO mode */
	uint16_t pptr;
	uint16_t pdata1;

	uint8_t memwin[3 * 1024];
	unsigned int tx_fifo_wp;
	unsigned int rx_fifo_rp;
	unsigned int rx_fifo_wp;
	uint8_t *txbuf;
	SigNode *intrqNode[4];
} CS8900;

static void enable_pktsrc(CS8900 * cs);
static void disable_pktsrc(CS8900 * cs);

static void
update_interrupts(CS8900 * cs)
{
	int interrupt = 0;
	uint16_t tx_ints =
	    TXEVENT_LOSS_OF_CRS | TXEVENT_SQERR | TXEVENT_TXOK | TXEVENT_OOW | TXEVENT_JABBER;
	uint16_t rx_ints = RXEVENT_RXOK | RXEVENT_CRCERROR | RXEVENT_RUNT | RXEVENT_EXTRADATA;
	uint16_t buf_ints = BUFEVENT_RX128 | BUFEVENT_RDY4TX | BUFEVENT_RXDEST | BUFEVENT_SWINT;
	//fprintf(stderr,"update ints with busctl %08x, rxev %08x, rxcfg %08x\n",cs->busctl,cs->rxevent,cs->rxcfg);

	if (cs->busctl & BUSCTL_ENABLEIRQ) {
		if (cs->rxevent & cs->rxcfg & rx_ints) {
			interrupt = 1;
		}
		if (cs->txevent & cs->txcfg & tx_ints) {
			interrupt = 1;
		}
		if (cs->bufevent & cs->bufcfg & buf_ints) {
			interrupt = 1;
		}
	}
	if (interrupt && !cs->interrupt_posted && (cs->intsel < 4)) {
		//fprintf(stderr,"CS8900 poste interrupt\n");
		SigNode_Set(cs->intrqNode[cs->intsel], SIG_HIGH);
		cs->interrupt_posted = 1;
	}
	if (!interrupt && cs->interrupt_posted && (cs->intsel < 4)) {
		//fprintf(stderr,"CS8900 unposte interrupt\n");
		SigNode_Set(cs->intrqNode[cs->intsel], SIG_LOW);
		cs->interrupt_posted = 0;
	}
}

/*
 * --------------------------------
 * MAC Address checks
 * --------------------------------
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
 * Check if incoming packet is for me
 * ----------------------------------------------
 */
static int
is_individual_address(CS8900 * cs, uint8_t * mac)
{
	if (memcmp(mac, cs->indaddr, 6) != 0) {
		return 0;
	}
	return 1;
}

/*
 * cs8900 moves to next frame on reading rxevent
 */
static void
update_receiver(CS8900 * cs)
{

	if (cs->rx_fifo_wp) {
		disable_pktsrc(cs);
	} else if (!(cs->linectl & LINECTL_SERRXON)) {
		disable_pktsrc(cs);
	} else {
		enable_pktsrc(cs);
	}
}

/*
 * ------------------------------------------------------------------
 * Handler for receiving packets from the tunneling filedescriptor 
 * ------------------------------------------------------------------
 */
static void
input_event(void *cd, int mask)
{
	CS8900 *cs = (CS8900 *) cd;
	uint8_t *rxbuf = cs->memwin + 4;
	int result;
	result = read(cs->ether_fd, rxbuf, 1532);
	if (result > 0) {
		uint16_t rxev;
		int broad = is_broadcast(rxbuf);
		int indiv = is_individual_address(cs, rxbuf);
		int promisca = cs->rxctl & RXCTL_PROMISCUOSA;
		int indiva = cs->rxctl & RXCTL_INDIVIDUALA;
		int broada = cs->rxctl & RXCTL_BROADCASTA;
		int rxoka = cs->rxctl & RXCTL_RXOKA;
		rxev = cs->rxevent;
		//fprintf(stderr,"RX event ind %d bc %d prom %d\n",indiv,broadcast,promisc);
		if (result < 64) {
			/* pad */
			memset(rxbuf + result, 0, 64 - result);
			result = 64;
		}
		if (broad) {
			rxev |= RXEVENT_BROADCAST;
		}
		if (indiv) {
			rxev |= RXEVENT_INDADDR;
		}
		if (!rxoka) {
			return;
		}
		if ((indiv && indiva) || (broad && broada) || promisca) {
			rxev |= RXEVENT_RXOK;
			cs->bufevent |= BUFEVENT_RXDEST;
			if (result >= 128) {
				cs->bufevent |= BUFEVENT_RX128;
			}
			cs->rxevent = rxev;
			cs->rcvfbc = result;
			cs->rx_fifo_rp = 0;
			cs->rx_fifo_wp = 4 + result;
			cs->memwin[0] = rxev & 0xff;
			cs->memwin[1] = (rxev >> 8) & 0xff;
			cs->memwin[2] = result & 0xff;
			cs->memwin[3] = (result >> 8) & 0xff;
			update_receiver(cs);
			update_interrupts(cs);
		}
	}
	return;
}

/*
 * ------------------------------------------------------------------
 * Read empty to avoid warnings in u-boot receiving old packets  
 * ------------------------------------------------------------------
 */
static void
clear_pktsrc(CS8900 * cs)
{
	uint8_t buf[100];
	int count;
	int i;
	for (i = 0; i < 500; i++) {
		count = read(cs->ether_fd, buf, 100);
		if (count <= 0) {
			break;
		}
	};
}

static void
enable_pktsrc(CS8900 * cs)
{
	if ((cs->pktsrc_is_enabled == 0) && (cs->ether_fd >= 0)) {
//                fprintf(stderr,"CS8900A: enable receiver\n");
		FIO_AddFileHandler(&cs->input_fh, cs->ether_fd, FIO_READABLE, input_event, cs);
		cs->pktsrc_is_enabled = 1;
	}
}

static void
disable_pktsrc(CS8900 * cs)
{
	//       fprintf(stderr,"CS8900A: disable receiver\n");
	if (cs->pktsrc_is_enabled) {
		FIO_RemoveFileHandler(&cs->input_fh);
		cs->pktsrc_is_enabled = 0;
	}
}

static void
transmit(CS8900 * cs)
{
	int result;
	//fprintf(stderr,"CS8900 transmit\n");
	if (!(cs->linectl & LINECTL_SERTXON)) {
		goto out;
	}
	if (cs->txlength > 1518) {
		fprintf(stderr, "CS8900 emulator: Paket to big: %d bytes\n", cs->txlength);
		goto out;
	}
	if (cs->txlength > cs->tx_fifo_wp) {
		fprintf(stderr, "CS8900 emulator: transmit, but not enough data %d wp %d\n",
			cs->txlength, cs->tx_fifo_wp);
		goto out;
	}
	//fcntl(cs->ether_fd,F_SETFL,0);
	result = write(cs->ether_fd, cs->txbuf, cs->txlength);
	//fcntl(cs->ether_fd,F_SETFL,O_NONBLOCK);
	cs->txevent |= TXEVENT_TXOK;
	cs->bufevent |= BUFEVENT_RDY4TX;
	update_interrupts(cs);
 out:
	cs->txlength = 0;
	cs->tx_fifo_wp = 0;
}

static void
cs8900_reset(CS8900 * cs)
{
	cs->iobase = 0x300;
	cs->intsel = 4;		/* No irq pin selected */
	cs->dmachan = 3;	/* No dmachan */
	cs->dmasof = 0;
	cs->dmafc = 0;
	cs->rxdmabc = 0;
	cs->membase = 0;
	cs->bprombase = 0;
	cs->rxcfg = 0x3;
	cs->rxevent = 0x4;
	cs->rxctl = 0x5;
	cs->txcfg = 0x7;
	cs->txevent = 0x8;
	cs->txcmdstat = 0x9;
	cs->bufcfg = 0xb;
	cs->bufevent = 0xc;
	cs->rxmiss = 0x10;
	cs->txcol = 0x12;
	cs->linectl = 0x13;
	cs->linest = 0x14;
	cs->selfctl = 0x15;
	cs->selfst = 0x16;
	cs->busctl = 0x17;
	cs->busst = 0x18;
	cs->testctl = 0x19;
	cs->auitdr = 0x1c;
	cs->busst = BUSST_RDY4TXNOW;	/* currently always ready */
	cs->linest = LINEST_LINKOK | LINEST_10BT | LINEST_POLOK;
	cs->selfst = SELFST_3V3_ACT | SELFST_INITD | SELFST_EEPRESENT | SELFST_EEOK | SELFST_EESIZE;
	cs->selfctl = 0;
	cs->rx_fifo_wp = cs->rx_fifo_rp = 0;
	cs->rcvfbc = 0;
	update_receiver(cs);
	update_interrupts(cs);
}

static uint32_t
mem_prod_id_read(void *clientData, uint32_t address, int rqlen)
{
	uint8_t id[] = { 0x0e, 0x63, 0x00, 0x0a };
	uint32_t value = 0;
	int ofs = address & 3;
	unsigned int index;
	int i;
	for (i = 0; i < rqlen; i++) {
		index = ofs * i;
		if (index < 4) {
			value = value | (id[index] << (i * 8));
		}
	}
	return value;
}

static void
mem_prod_id_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "CS8900: Product id is not writable\n");
	return;
}

static uint32_t
mem_iobase_read(void *clientData, uint32_t address, int rqlen)
{
	CS8900 *cs = (CS8900 *) clientData;
	return cs->iobase;
}

static void
mem_iobase_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	CS8900 *cs = (CS8900 *) clientData;
	//IO_Unmap      
	cs->iobase = value;
	//IO_Map
	fprintf(stderr, "CS8900: iobase write not complete\n");
	return;
}

/*
 * ---------------------------------------------------------
 * Select the irq pin
 * ---------------------------------------------------------
 */
static uint32_t
mem_intsel_read(void *clientData, uint32_t address, int rqlen)
{
	CS8900 *cs = (CS8900 *) clientData;
	return cs->intsel;
}

static void
mem_intsel_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	CS8900 *cs = (CS8900 *) clientData;
	if (value != cs->intsel) {
		if (cs->intsel < 4) {
			SigNode_Set(cs->intrqNode[cs->intsel], SIG_OPEN);
		}
		cs->intsel = value;
		cs->interrupt_posted = 0;
		update_interrupts(cs);
	}
	//fprintf(stderr,"Selected IRQ pin %d\n",value);
	return;
}

static uint32_t
mem_dmachan_read(void *clientData, uint32_t address, int rqlen)
{
	CS8900 *cs = (CS8900 *) clientData;
	return cs->dmachan;
}

static void
mem_dmachan_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	CS8900 *cs = (CS8900 *) clientData;
	cs->dmachan = value;
	return;
}

static uint32_t
mem_dmasof_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "CS8900: register %08x not implemented \n", address);
	return 0;
}

static void
mem_dmasof_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "CS8900: register %08x not implemented \n", address);
	return;
}

static uint32_t
mem_dmafc_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "CS8900: register %08x not implemented \n", address);
	return 0;
}

static void
mem_dmafc_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "CS8900: register %08x not implemented \n", address);
	return;
}

static uint32_t
mem_rxdmabc_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "CS8900: register %08x not implemented \n", address);
	return 0;
}

static void
mem_rxdmabc_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "CS8900: register %08x not implemented \n", address);
	return;
}

static uint32_t
mem_membase_read(void *clientData, uint32_t address, int rqlen)
{
	CS8900 *cs = (CS8900 *) clientData;
	return cs->membase;
}

static void
mem_membase_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	CS8900 *cs = (CS8900 *) clientData;
	// MemUnmap
	fprintf(stderr, "CS8900: Membase write not complete\n");
	cs->membase = value;
	// MemMap
	return;
}

static uint32_t
mem_bprombase_read(void *clientData, uint32_t address, int rqlen)
{
	CS8900 *cs = (CS8900 *) clientData;
	return cs->bprombase;
}

static void
mem_bprombase_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	CS8900 *cs = (CS8900 *) clientData;
	cs->bprombase = value;
	return;
}

static uint32_t
mem_bpromamask_read(void *clientData, uint32_t address, int rqlen)
{
	CS8900 *cs = (CS8900 *) clientData;
	return cs->bpromamask;
}

static void
mem_bpromamask_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	CS8900 *cs = (CS8900 *) clientData;
	cs->bpromamask = value;
	return;
}

static uint32_t
mem_eepromcmd_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "CS8900: EEPROM command register is not readable \n");
	return 0;
}

static void
mem_eepromcmd_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	CS8900 *cs = (CS8900 *) clientData;
	int i;
	int ob = (value >> 8) & 3;
	int addr = value & 0xff;
	//int elsel = (value >> 10) & 1;
	switch (ob) {
	    case 0:
		    switch (addr & 0x30) {
			case 0:
				cs->ee_write_enabled = 0;
				break;
			case 0x10:
				if (cs->ee_write_enabled) {
					for (i = 0; i < 64; i++) {
						m93c46_writew(cs->eeprom, cs->eepromdata, i);
					}
				}
				break;

			case 0x20:
				if (cs->ee_write_enabled) {
					for (i = 0; i < 64; i++) {
						m93c46_writew(cs->eeprom, 0xff, i);
					}
				}
				break;
			case 0x30:
				cs->ee_write_enabled = 1;
				break;
		    }
		    break;
	    case 1:
		    if (cs->ee_write_enabled) {
			    m93c46_writew(cs->eeprom, cs->eepromdata, addr & 0x3f);
		    }
		    break;
	    case 2:
		    cs->eepromdata = m93c46_readw(cs->eeprom, addr & 0x3f);
		    break;

	    case 3:
		    if (cs->ee_write_enabled) {
			    m93c46_writew(cs->eeprom, 0xff, addr & 0x3f);
		    }
		    break;
	    default:
		    fprintf(stderr, "unreachable code\n");
	}
	return;
}

static uint32_t
mem_eepromdata_read(void *clientData, uint32_t address, int rqlen)
{
	CS8900 *cs = (CS8900 *) clientData;
	return cs->eepromdata;
}

static void
mem_eepromdata_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	CS8900 *cs = (CS8900 *) clientData;
	cs->eepromdata = value;
	return;
}

/*
 * ---------------------------------------------------------------------
 * Receive Frame byte count: Number of bytes received in current frame
 * ---------------------------------------------------------------------
 */
static uint32_t
mem_rcvfbc_read(void *clientData, uint32_t address, int rqlen)
{
	CS8900 *cs = (CS8900 *) clientData;
	return cs->rcvfbc;
}

static void
mem_rcvfbc_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "CS8900: RCVFBC not writable\n");
	return;
}

/*
 * ------------------------------------------------------------------------------
 * RXCFG
 *	Bit 6  SKIP_1 		skip one frame in RX buffer. Self clearing ?????
 *	Bit 7  STREAME		stream transfer mode (only DMA)	
 *	Bit 8  RXOKIE		enable interrupt when a valid frame was received
 *	Bit 9  RXDMAONLY 	use rx dma mode 
 *	Bit 10 AUTORXDMAE 	switch automatically to dma mode
 *	Bit 11 BUFFERCRC	include buffercrc in rx frame 
 *	Bit 12 CRCERRORIE	enable interrupt on CRC error
 *	Bit 13 RUNTIE		enable interrupt if received frame is < 64 bytes
 *	Bit 14 EXTRADATAIE	enable interrupt when frame is > 1518 Bytes
 * ------------------------------------------------------------------------------
 */
static uint32_t
mem_rxcfg_read(void *clientData, uint32_t address, int rqlen)
{
	CS8900 *cs = (CS8900 *) clientData;
	return (cs->rxcfg & ~0x3f) | RXCFG_ADDR;
}

static void
mem_rxcfg_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	CS8900 *cs = (CS8900 *) clientData;
	cs->rxcfg = value;
	//fprintf(stderr,"New rxcfg 0x%04x\n",value);
	if (value & RXCFG_SKIP_1) {
		cs->rxevent = 0;
		cs->rx_fifo_rp = cs->rx_fifo_wp = 0;
		cs->rcvfbc = 0;
		memset(cs->memwin, 0, 4);
		update_receiver(cs);
	}
	if (value & (RXCFG_STREAME | RXCFG_RXDMAONLY | RXCFG_AUTORXDMAE)) {
		fprintf(stderr, "CS8900: DMA modes not supported\n");
	}
	update_interrupts(cs);
	return;
}

/*
 * -------------------------------------------------------------------------------
 * RXCTL
 * Bit 6 IAHASHA	accept frames passed by hash filter
 * Bit 7 PROMISCUOSA	accept frames with any address
 * Bit 8 RXOKA		accept frames with good crc
 * Bit 9 MULTICASTA	accept frames with a multicast address
 * Bit 10 INDIVIDUALA	accept frames with matching destination address
 * Bit 11 BROADCASTA	accept frames with dst address ff:ff:ff:ff:ff:ff
 * Bit 12 CRCERRORA	accept frames with crc error
 * Bit 13 RUNTA		accept short frames (<64 bytes)
 * Bit 14 EXTRADATAA	accept long frames (>1518 bytes) discard rest !
 * --------------------------------------------------------------------------------
 */
static uint32_t
mem_rxctl_read(void *clientData, uint32_t address, int rqlen)
{
	CS8900 *cs = (CS8900 *) clientData;
	return (cs->rxctl & ~0x3f) | RXCTL_ADDR;
}

static void
mem_rxctl_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	CS8900 *cs = (CS8900 *) clientData;
	cs->rxctl = value;
	update_receiver(cs);
	return;
}

/* 
 * ---------------------------------------------------------
 *  TXCFG
 *	Tx-Interrupt enable bits
 *
 *	LOSSOFCRSIE  (6)
 *	SQERRORIE    (7)
 *	TXOKIE	     (8)
 *	OOWIE	     (9)
 *	JABBERIE     (10)
 *	ANYCOLLIE    (11)
 *	COLLIE	     (15)
 * ---------------------------------------------------------
 */
static uint32_t
mem_txcfg_read(void *clientData, uint32_t address, int rqlen)
{
	CS8900 *cs = (CS8900 *) clientData;
	return (cs->txcfg & ~0x3f) | TXCFG_ADDR;
}

static void
mem_txcfg_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	CS8900 *cs = (CS8900 *) clientData;
	cs->txcfg = value;
	update_interrupts(cs);
	return;
}

/* 
 * ---------------------------------------------------------------------------------
 * RXEVENT 
 *	Bit  6: IAHASH frame was accepted by hash filter  
 *	Bit  7: DRIBBLEBITS frame had dribblebits 
 *	Bit  8: RXOK A valid frame was received (crc,length)
 *	Bit  9: HASHED frame was accepted by hash filter 
 *	Bit 10: INDADDR frame was accepted because of indiv addr match
 *	Bit 11: BROADCAST frame was accepted because address was ff:ff:ff:ff:ff:ff
 *	Bit 12: CRCERROR frame had crc error
 *	Bit 13: RUNT frame was shorter than 64 bytes
 *	Bit 14: EXTRADATA Frame was longer than 1518 bytes
 * 
 * ----------------------------------------------------------------------------------
 */
static uint32_t
mem_rxevent_read(void *clientData, uint32_t address, int rqlen)
{
	CS8900 *cs = (CS8900 *) clientData;
	uint16_t rxevent = cs->rxevent;
	cs->rxevent = 0;
	update_interrupts(cs);
	/* p.72 in manual says if FRAME reading is started it will be skipped on rxevent read */
	if (cs->rx_fifo_rp > 4) {
		cs->rx_fifo_rp = cs->rx_fifo_wp = 0;
		cs->rcvfbc = 0;
		memset(cs->memwin, 0, 4);	/* not verified on real chip */
		update_receiver(cs);
	}
	return rxevent | RXEVENT_ADDR;
}

static void
mem_rxevent_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "CS8900: RXEVENT register: not writable\n");
	return;
}

/*
 * ---------------------------------------------------------------------------------
 * BUFEVENT
 *	BUFEVENT_SWINT	(6) A Software initiated interrupt occurred
 *	RXDMAFRAME	(7) One or more frames have been tranfered by slave DMA
 *	RDY4TX		(8) A ready for tx event happened
 * 	TXUNDER		(9)
 *	RXMISS		(10)
 *	RX128		(11)
 *	RXDEST		(15)
 * ----------------------------------------------------------------------------------
 */
static uint32_t
mem_bufevent_read(void *clientData, uint32_t address, int rqlen)
{
	CS8900 *cs = (CS8900 *) clientData;
	uint16_t bufevent = cs->bufevent;
	cs->bufevent = 0;
	update_interrupts(cs);
	return (bufevent & ~0x3f) | BUFEVENT_ADDR;
}

static void
mem_bufevent_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "CS8900: BUFEVENT register: not writable\n");
	return;
}

static uint32_t
mem_rxmiss_read(void *clientData, uint32_t address, int rqlen)
{
	CS8900 *cs = (CS8900 *) clientData;
	uint16_t rxmiss = cs->rxmiss;
	cs->rxmiss = 0;
	update_interrupts(cs);
	return (rxmiss & ~0x3f) | RXMISS_ADDR;
}

static void
mem_rxmiss_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "CS8900: RXMISS register: not writable\n");
	return;
}

static uint32_t
mem_txcol_read(void *clientData, uint32_t address, int rqlen)
{
	CS8900 *cs = (CS8900 *) clientData;
	uint16_t txcol = cs->txcol;
	cs->txcol = 0;
	update_interrupts(cs);
	return txcol | TXCOL_ADDR;
}

static void
mem_txcol_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "CS8900: TXCOL register: not writable\n");
	return;
}

static uint32_t
mem_txevent_read(void *clientData, uint32_t address, int rqlen)
{
	CS8900 *cs = (CS8900 *) clientData;
	uint16_t txevent = cs->txevent;
	cs->txevent = 0;
	update_interrupts(cs);
	return txevent | TXEVENT_ADDR;
}

static void
mem_txevent_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "CS8900: TXEVENT register: not writable\n");
	return;
}

static uint32_t
mem_linest_read(void *clientData, uint32_t address, int rqlen)
{
	CS8900 *cs = (CS8900 *) clientData;
	return (cs->linest & ~0x3f) | LINEST_ADDR;
}

static void
mem_linest_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "CS8900: Linestatus not writable\n");
	return;
}

static uint32_t
mem_selfst_read(void *clientData, uint32_t address, int rqlen)
{
	CS8900 *cs = (CS8900 *) clientData;
	return (cs->selfst & ~0x3f) | SELFST_ADDR;
}

static void
mem_selfst_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "CS8900: SELFST not writable\n");
	return;
}

static uint32_t
mem_busst_read(void *clientData, uint32_t address, int rqlen)
{
	CS8900 *cs = (CS8900 *) clientData;
	return cs->busst | BUSST_ADDR;
}

static void
mem_busst_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "Bus status not writable\n");
	return;
}

static uint32_t
mem_auitdr_read(void *clientData, uint32_t address, int rqlen)
{
	CS8900 *cs = (CS8900 *) clientData;
	fprintf(stderr, "CS8900: register %08x not implemented\n", address);
	return (cs->auitdr & ~0x3f) | AUITDR_ADDR;
}

static void
mem_auitdr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "AUITDR not writable\n");
	return;
}

static uint32_t
mem_txcmdstat_read(void *clientData, uint32_t address, int rqlen)
{
	CS8900 *cs = (CS8900 *) clientData;
	uint32_t value = cs->txcmdstat;
	cs->txcmdstat = 0;
	return (value & ~0x3f) | TXCMDSTAT_ADDR;
}

static void
mem_txcmd_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	CS8900 *cs = (CS8900 *) clientData;
	if ((cs->txlength > 3) && (cs->tx_fifo_wp >= cs->txlength)) {
		transmit(cs);
	}
	return;
}

/*
 * ------------------------------------------------------------------------
 * BUFCFG
 *
 *	SWINTX		(6) Trigger a soft Interrupt. selfclearing ????
 *	RXDMAIE		(7) 
 *	RDY4TXIE	(8) Ready for TX event interrupt enable
 *	TXUNDERIE	(9)
 *	RXMISSIE	(10)
 *	RX128IE		(11) Interrupt when at least 128 are received
 *	TXCOLOVFLOIE	(12)
 *	MISSOVFLOIE	(13)
 *	RXDESTIE	(15) Interrupt when address match is detected
 * -----------------------------------------------------------------------
 */
static uint32_t
mem_bufcfg_read(void *clientData, uint32_t address, int rqlen)
{
	CS8900 *cs = (CS8900 *) clientData;
	return (cs->bufcfg & ~0x3f) | BUFCFG_ADDR;
}

static void
mem_bufcfg_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	CS8900 *cs = (CS8900 *) clientData;
	cs->bufcfg = value;
	if (value & BUFCFG_SWINTX) {
		cs->bufevent |= BUFEVENT_SWINT;
	}
	update_interrupts(cs);
	return;
}

static uint32_t
mem_linectl_read(void *clientData, uint32_t address, int rqlen)
{
	CS8900 *cs = (CS8900 *) clientData;
	return (cs->linectl & ~0x3f) | LINECTL_ADDR;
}

static void
mem_linectl_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	CS8900 *cs = (CS8900 *) clientData;
	uint8_t diff = value ^ cs->linectl;
	if (value & diff & LINECTL_SERRXON) {
		clear_pktsrc(cs);
	}
	cs->linectl = value;
	update_receiver(cs);
	return;
}

static uint32_t
mem_selfctl_read(void *clientData, uint32_t address, int rqlen)
{
	CS8900 *cs = (CS8900 *) clientData;
	return (cs->selfctl & ~0x3f) | SELFCTL_ADDR;
}

static void
mem_selfctl_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	CS8900 *cs = (CS8900 *) clientData;
	cs->selfctl = value & ~SELFCTL_RESET;
	if (value & SELFCTL_RESET) {
		cs8900_reset(cs);
		//fprintf(stderr,"selfctl reset not implemented\n");
	}
	return;
}

/*
 * ------------------------------------------------------------
 * RESETRXDMA 	 (6)
 * DMAEXTEND  	 (8)
 * USESA	 (9)
 * MEMORYE	(10) Enable the memory mapping 
 * DMABURST	(11)
 * IOCHRDYE	(12)
 * RXDMASIZE	(13)
 * ENABLEIRQ	(15) Global irq enable bit
 * ------------------------------------------------------------
*/
static uint32_t
mem_busctl_read(void *clientData, uint32_t address, int rqlen)
{
	CS8900 *cs = (CS8900 *) clientData;
	return (cs->busctl & ~0x3f) | BUSCTL_ADDR;
}

static void
mem_busctl_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	CS8900 *cs = (CS8900 *) clientData;
	cs->busctl = value & ~0x3f;
	update_interrupts(cs);
	return;
}

static uint32_t
mem_testctl_read(void *clientData, uint32_t address, int rqlen)
{
	CS8900 *cs = (CS8900 *) clientData;
	return (cs->testctl & ~0x3f) | TESTCTL_ADDR;
}

static void
mem_testctl_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	CS8900 *cs = (CS8900 *) clientData;
	cs->testctl = value;
	if (value & (TESTCTL_AUI_LOOP | TESTCTL_ENDEC_LOOP)) {
		fprintf(stderr, "CS8900: ENDEC and AUI LOOP modes not implemented\n");
	}
	return;
}

static uint32_t
mem_txlength_read(void *clientData, uint32_t address, int rqlen)
{
	CS8900 *cs = (CS8900 *) clientData;
	static int warned = 0;
	if (!warned) {
		fprintf(stderr, "CS8900: reading from writeonly register TXLENGTH\n");
		warned = 1;
	}
	return cs->txlength;
}

static void
mem_txlength_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	CS8900 *cs = (CS8900 *) clientData;
	cs->txlength = value;
	if ((cs->txlength > 3) && (cs->tx_fifo_wp >= cs->txlength)) {
		transmit(cs);
	}
	return;
}

static uint32_t
mem_laf_read(void *clientData, uint32_t address, int rqlen)
{
	CS8900 *cs = (CS8900 *) clientData;
	uint32_t value = 0;
	int ofs = address & 7;
	int i;
	for (i = 0; i < rqlen; i++) {
		if ((i + ofs) < 8) {
			value |= cs->laf[i + ofs] << (i * 8);
		}
	}
	return value;
}

static void
mem_laf_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	CS8900 *cs = (CS8900 *) clientData;
	int ofs = address & 7;
	int i;
	for (i = 0; i < rqlen; i++) {
		if ((i + ofs) < 8) {
			cs->laf[i + ofs] = value & 0xff;
			value = value >> 8;
		}
	}
	return;
}

static uint32_t
mem_indaddr_read(void *clientData, uint32_t address, int rqlen)
{
	CS8900 *cs = (CS8900 *) clientData;
	uint32_t value = 0;
	int ofs = address & 7;
	int i;
	for (i = 0; i < rqlen; i++) {
		if ((i + ofs) < 6) {
			value |= cs->indaddr[i + ofs] << (i * 8);
		}
	}
	//fprintf(stderr,"indaddr %d read: %04x\n",ofs,value);
	return value;
}

static void
mem_indaddr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	CS8900 *cs = (CS8900 *) clientData;
	int ofs = address & 7;
	int i;
	//fprintf(stderr,"Set mac\n");
	for (i = 0; i < rqlen; i++) {
		if ((i + ofs) < 6) {
			cs->indaddr[i + ofs] = value & 0xff;
			value = value >> 8;
		}
	}
	return;
}

/*
 * ---------------------------------------------------
 * Access to internal RX/TX buffer RAM
 * ---------------------------------------------------
 */

static void
mem_txframe_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	CS8900 *cs = (CS8900 *) clientData;
	int i;
	for (i = 0; i < rqlen; i++) {
		if (cs->tx_fifo_wp < 0x600) {
			cs->txbuf[cs->tx_fifo_wp++] = value & 0xff;
			value = value >> 8;
		}
	}
	if ((cs->txlength > 3) && (cs->tx_fifo_wp >= cs->txlength)) {
		transmit(cs);
	}
	return;
}

static uint32_t
mem_rxframe_read(void *clientData, uint32_t address, int rqlen)
{
	CS8900 *cs = (CS8900 *) clientData;
	uint16_t value = 0;
	unsigned int ofs = (address & 0xfff) - 0x400;
	int i;
	if ((ofs + rqlen) > 0x600) {
		rqlen = 0x600 - ofs;
	}
	if (ofs < 118) {
		for (i = 0; i < rqlen; i++) {
			value |= cs->memwin[ofs + i] << (i << 3);
		}
		cs->rx_fifo_rp = ofs + rqlen;
	} else {
		for (i = 0; i < rqlen; i++) {
			value |= cs->memwin[cs->rx_fifo_rp++] << (i << 3);
		}
	}
	/* Manual says when entire frame has been read it becomes inaccesible (p.72) */
	if (cs->rx_fifo_rp >= cs->rx_fifo_wp) {
		cs->rx_fifo_rp = cs->rx_fifo_wp = 0;
		cs->rcvfbc = 0;
		memset(cs->memwin, 0, 4);
		update_receiver(cs);
	}
	//fprintf(stderr,"rxframe read: ofs %04x, rqlen %d, result %04x, rp %04x\n",ofs,rqlen,value,cs->rx_fifo_rp);
	return value;
}

static uint32_t
io_rxtxdata0_read(void *clientData, uint32_t address, int rqlen)
{
	CS8900 *cs = (CS8900 *) clientData;
	uint32_t value = 0;
	int i;
	for (i = 0; i < rqlen; i++) {
		if (cs->rx_fifo_rp < cs->rx_fifo_wp) {
			value = value | (cs->memwin[cs->rx_fifo_rp++] << (i << 3));
		}
	}
	//fprintf(stderr,"rxtxdata %04x, rp %04x\n",value,cs->rx_fifo_rp);
	/* Manual says when entire frame has been read it becomes inaccesible (p.72) */
	if (cs->rx_fifo_rp >= cs->rx_fifo_wp) {
		cs->rx_fifo_rp = cs->rx_fifo_wp = 0;
		cs->rcvfbc = 0;
		memset(cs->memwin, 0, 4);
		update_receiver(cs);
	}
	return value;
}

static void
io_rxtxdata0_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	CS8900 *cs = (CS8900 *) clientData;
	int i;
	for (i = 0; i < rqlen; i++) {
		if (cs->tx_fifo_wp < 0x600) {
			cs->txbuf[cs->tx_fifo_wp++] = value & 0xff;
			value = value >> 8;
		}
	}
	if ((cs->txlength > 3) && (cs->tx_fifo_wp >= cs->txlength)) {
		transmit(cs);
	}
	return;
}

static uint32_t
io_rxtxdata1_read(void *clientData, uint32_t address, int rqlen)
{
	CS8900 *cs = (CS8900 *) clientData;
	return io_rxtxdata0_read(cs, address, rqlen);
}

static void
io_rxtxdata1_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	CS8900 *cs = (CS8900 *) clientData;
	io_rxtxdata0_write(cs, value, address, rqlen);
	return;
}

static void
io_txcmd_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	CS8900 *cs = (CS8900 *) clientData;
	mem_txcmd_write(cs, value, address, rqlen);
	return;
}

static uint32_t
io_txlength_read(void *clientData, uint32_t address, int rqlen)
{
	CS8900 *cs = (CS8900 *) clientData;
	return mem_txlength_read(cs, address, rqlen);
}

static void
io_txlength_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	CS8900 *cs = (CS8900 *) clientData;
	mem_txlength_write(cs, value, address, rqlen);
	return;
}

/*
 * ------------------------------------------------------------
 * ISQ read:
 * read pending event with highest priority.
 * Order is: rxev, txev, bufev, rxmiss, txcol
 * Missing: unpost interrupt when the first word is read,
 * reenable when 0 is read
 * ------------------------------------------------------------
 */
static uint32_t
io_isq_read(void *clientData, uint32_t address, int rqlen)
{
	CS8900 *cs = (CS8900 *) clientData;
	uint32_t value = 0;
	value = mem_rxevent_read(cs, address, rqlen);
	if (value & ~0x3f) {
		return value;
	}
	value = mem_txevent_read(cs, address, rqlen);
	if (value & ~0x3f) {
		return value;
	}
	value = mem_bufevent_read(cs, address, rqlen);
	if (value & ~0x3f) {
		return value;
	}
	value = mem_rxmiss_read(cs, address, rqlen);
	if (value & ~0x3f) {
		return value;
	}
	value = mem_txcol_read(cs, address, rqlen);
	if (value & ~0x3f) {
		return value;
	}
	value = 0;
	// reenable irq
	//fprintf(stderr,"ISQ: 0x%04x\n",value);
	return value;
}

static void
io_isq_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "CS8900: ISQ register is not writable\n");
	return;
}

static uint32_t
io_pptr_read(void *clientData, uint32_t address, int rqlen)
{
	CS8900 *cs = (CS8900 *) clientData;
	return (cs->pptr & ~(1 << 14)) | (3 << 12);
}

static void
io_pptr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	CS8900 *cs = (CS8900 *) clientData;
	cs->pptr = value;
	return;
}

static uint32_t
io_pdata0_read(void *clientData, uint32_t address, int rqlen)
{
	CS8900 *cs = (CS8900 *) clientData;
	uint16_t regaddr = cs->pptr & 0xfff;
	uint32_t value = 0;
	int size = 16;
	int autoinc = (cs->pptr >> 15) & 1;
	switch (regaddr) {
	    case MEM_PROD_ID(0):
		    value = mem_prod_id_read(cs, regaddr, rqlen);
		    size = 32;
		    break;

	    case MEM_IOBASE(0):
		    value = mem_iobase_read(cs, regaddr, rqlen);
		    break;

	    case MEM_INTSEL(0):
		    value = mem_intsel_read(cs, regaddr, rqlen);
		    break;

	    case MEM_DMACHAN(0):
		    value = mem_dmachan_read(cs, regaddr, rqlen);
		    break;

	    case MEM_DMASOF(0):
		    value = mem_dmasof_read(cs, regaddr, rqlen);
		    break;

	    case MEM_DMAFC(0):
		    value = mem_dmafc_read(cs, regaddr, rqlen);
		    break;

	    case MEM_RXDMABC(0):
		    value = mem_rxdmabc_read(cs, regaddr, rqlen);
		    break;

	    case MEM_MEMBASE(0):
		    value = mem_membase_read(cs, regaddr, rqlen);
		    size = 32;
		    break;

	    case MEM_BPROMBASE(0):
		    value = mem_bprombase_read(cs, regaddr, rqlen);
		    size = 32;
		    break;

	    case MEM_BPROMAMASK(0):
		    value = mem_bpromamask_read(cs, regaddr, rqlen);
		    size = 32;
		    break;

	    case MEM_EEPROMCMD(0):
		    value = mem_eepromcmd_read(cs, regaddr, rqlen);
		    break;

	    case MEM_EEPROMDATA(0):
		    value = mem_eepromdata_read(cs, regaddr, rqlen);
		    break;

	    case MEM_RCVFBC(0):
		    value = mem_rcvfbc_read(cs, regaddr, rqlen);
		    break;

	    case MEM_RXCFG(0):
		    value = mem_rxcfg_read(cs, regaddr, rqlen);
		    break;

	    case MEM_RXCTL(0):
		    value = mem_rxctl_read(cs, regaddr, rqlen);
		    break;

	    case MEM_TXCFG(0):
		    value = mem_txcfg_read(cs, regaddr, rqlen);
		    break;

	    case MEM_TXCMDSTAT(0):
		    value = mem_txcmdstat_read(cs, regaddr, rqlen);
		    break;

	    case MEM_BUFCFG(0):
		    value = mem_bufcfg_read(cs, regaddr, rqlen);
		    break;

	    case MEM_LINECTL(0):
		    value = mem_linectl_read(cs, regaddr, rqlen);
		    break;

	    case MEM_SELFCTL(0):
		    value = mem_selfctl_read(cs, regaddr, rqlen);
		    break;

	    case MEM_BUSCTL(0):
		    value = mem_busctl_read(cs, regaddr, rqlen);
		    break;

	    case MEM_TESTCTL(0):
		    value = mem_testctl_read(cs, regaddr, rqlen);
		    break;

	    case MEM_RXEVENT(0):
		    value = mem_rxevent_read(cs, regaddr, rqlen);
		    break;

	    case MEM_BUFEVENT(0):
		    value = mem_bufevent_read(cs, regaddr, rqlen);
		    break;

	    case MEM_RXMISS(0):
		    value = mem_rxmiss_read(cs, regaddr, rqlen);
		    break;

	    case MEM_TXCOL(0):
		    value = mem_txcol_read(cs, regaddr, rqlen);
		    break;

	    case MEM_TXEVENT(0):
		    value = mem_txevent_read(cs, regaddr, rqlen);
		    break;

	    case MEM_LINEST(0):
		    value = mem_linest_read(cs, regaddr, rqlen);
		    break;

	    case MEM_SELFST(0):
		    value = mem_selfst_read(cs, regaddr, rqlen);
		    break;

	    case MEM_BUSST(0):
		    value = mem_busst_read(cs, regaddr, rqlen);
		    break;

	    case MEM_AUITDR(0):
		    value = mem_auitdr_read(cs, regaddr, rqlen);
		    break;

		    //case MEM_STATEV(0):

	    case MEM_TXLENGTH(0):
		    value = mem_txlength_read(cs, regaddr, rqlen);
		    break;

	    case MEM_LAF(0):
	    case MEM_LAF(0) + 1:
	    case MEM_LAF(0) + 2:
	    case MEM_LAF(0) + 3:
	    case MEM_LAF(0) + 4:
	    case MEM_LAF(0) + 5:
		    value = mem_laf_read(cs, regaddr + 2, rqlen) << 16;
	    case MEM_LAF(0) + 6:
	    case MEM_LAF(0) + 7:
		    value = mem_laf_read(cs, regaddr, rqlen);
		    break;

	    case MEM_INDADDR(0):
	    case MEM_INDADDR(0) + 1:
	    case MEM_INDADDR(0) + 2:
	    case MEM_INDADDR(0) + 3:
		    value |= mem_indaddr_read(cs, regaddr + 2, rqlen) << 16;
	    case MEM_INDADDR(0) + 4:
	    case MEM_INDADDR(0) + 5:
		    value |= mem_indaddr_read(cs, regaddr, rqlen);
		    break;

	    default:
		    if (regaddr >= 0x400) {
			    value = mem_rxframe_read(cs, regaddr, rqlen);
		    }
		    break;
	}
	cs->pdata1 = value >> 16;
	if (autoinc) {
		cs->pptr = (cs->pptr + rqlen) & 0x8fff;
	}
	return value;
}

static void
io_pdata0_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	CS8900 *cs = (CS8900 *) clientData;
	uint16_t regaddr = cs->pptr & 0xfff;
	int size = 16;
	int autoinc = (cs->pptr >> 15) & 1;
	switch (regaddr) {
	    case MEM_PROD_ID(0):
		    mem_prod_id_write(cs, value, regaddr, rqlen);
		    break;

	    case MEM_IOBASE(0):
		    mem_iobase_write(cs, value, regaddr, rqlen);
		    break;

	    case MEM_INTSEL(0):
		    mem_intsel_write(cs, value, regaddr, rqlen);
		    break;

	    case MEM_DMACHAN(0):
		    mem_dmachan_write(cs, value, regaddr, rqlen);
		    break;

	    case MEM_DMASOF(0):
		    mem_dmasof_write(cs, value, regaddr, rqlen);
		    break;

	    case MEM_DMAFC(0):
		    mem_dmafc_write(cs, value, regaddr, rqlen);
		    break;

	    case MEM_RXDMABC(0):
		    mem_rxdmabc_write(cs, value, regaddr, rqlen);
		    break;

	    case MEM_MEMBASE(0):
		    mem_membase_write(cs, value, regaddr, rqlen);
		    size = 32;
		    break;

	    case MEM_BPROMBASE(0):
		    mem_bprombase_write(cs, value, regaddr, rqlen);
		    size = 32;
		    break;

	    case MEM_BPROMAMASK(0):
		    mem_bpromamask_write(cs, value, regaddr, rqlen);
		    size = 32;
		    break;

	    case MEM_EEPROMCMD(0):
		    mem_eepromcmd_write(cs, value, regaddr, rqlen);
		    break;

	    case MEM_EEPROMDATA(0):
		    mem_eepromdata_write(cs, value, regaddr, rqlen);
		    break;

	    case MEM_RCVFBC(0):
		    mem_rcvfbc_write(cs, value, regaddr, rqlen);
		    break;

	    case MEM_RXCFG(0):
		    mem_rxcfg_write(cs, value, regaddr, rqlen);
		    break;

	    case MEM_RXCTL(0):
		    mem_rxctl_write(cs, value, regaddr, rqlen);
		    break;

	    case MEM_TXCFG(0):
		    mem_txcfg_write(cs, value, regaddr, rqlen);
		    break;

	    case MEM_RXEVENT(0):
		    mem_rxevent_write(cs, value, regaddr, rqlen);
		    break;

	    case MEM_BUFEVENT(0):
		    mem_bufevent_write(cs, value, regaddr, rqlen);
		    break;

	    case MEM_RXMISS(0):
		    mem_rxmiss_write(cs, value, regaddr, rqlen);
		    break;

	    case MEM_TXCOL(0):
		    mem_txcol_write(cs, value, regaddr, rqlen);
		    break;

	    case MEM_TXEVENT(0):
		    mem_txevent_write(cs, value, regaddr, rqlen);
		    break;
	    case MEM_LINEST(0):
		    mem_linest_write(cs, value, regaddr, rqlen);
		    break;
	    case MEM_SELFST(0):
		    mem_selfst_write(cs, value, regaddr, rqlen);
		    break;
	    case MEM_BUSST(0):
		    mem_busst_write(cs, value, regaddr, rqlen);
		    break;

	    case MEM_AUITDR(0):
		    mem_auitdr_write(cs, value, regaddr, rqlen);
		    break;

		    //case MEM_STATEV(0):

	    case MEM_TXCMD(0):
		    mem_txcmd_write(cs, value, regaddr, rqlen);
		    break;

	    case MEM_BUFCFG(0):
		    mem_bufcfg_write(cs, value, regaddr, rqlen);
		    break;

	    case MEM_LINECTL(0):
		    mem_linectl_write(cs, value, regaddr, rqlen);
		    break;

	    case MEM_SELFCTL(0):
		    mem_selfctl_write(cs, value, regaddr, rqlen);
		    break;

	    case MEM_BUSCTL(0):
		    mem_busctl_write(cs, value, regaddr, rqlen);
		    break;

	    case MEM_TESTCTL(0):
		    mem_testctl_write(cs, value, regaddr, rqlen);
		    break;

	    case MEM_TXLENGTH(0):
		    mem_txlength_write(cs, value, regaddr, rqlen);
		    break;

	    case MEM_LAF(0):
	    case MEM_LAF(0) + 1:
	    case MEM_LAF(0) + 2:
	    case MEM_LAF(0) + 3:
	    case MEM_LAF(0) + 4:
	    case MEM_LAF(0) + 5:
	    case MEM_LAF(0) + 6:
	    case MEM_LAF(0) + 7:
		    mem_laf_write(cs, value, regaddr, rqlen);
		    break;

	    case MEM_INDADDR(0):
	    case MEM_INDADDR(0) + 1:
	    case MEM_INDADDR(0) + 2:
	    case MEM_INDADDR(0) + 3:
	    case MEM_INDADDR(0) + 4:
	    case MEM_INDADDR(0) + 5:
		    mem_indaddr_write(cs, value, regaddr, rqlen);
		    break;

	    default:
		    if (regaddr >= 0xa00) {
			    /* ???? is rxbuf writable ??? */
			    mem_txframe_write(cs, value, regaddr, rqlen);
		    }
	}
	if (autoinc) {
		cs->pptr = (cs->pptr + rqlen) & 0x8fff;
	}
	return;
}

static uint32_t
io_pdata1_read(void *clientData, uint32_t address, int rqlen)
{
	CS8900 *cs = (CS8900 *) clientData;
	return cs->pdata1;
}

static void
io_pdata1_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	CS8900 *cs = (CS8900 *) clientData;
	return io_pdata0_write(cs, value, address + 2, rqlen);
}

#if 0
static void
CS8900_MemMap(CS8900 * cs, uint32_t base)
{
	int i;
	uint32_t flags;
	IOH_New32(MEM_PROD_ID(base), mem_prod_id_read, mem_prod_id_write, cs);
	IOH_New16(MEM_IOBASE(base), mem_iobase_read, mem_iobase_write, cs);
	IOH_New16(MEM_IOBASE(base), mem_intsel_read, mem_intsel_write, cs);
	IOH_New16(MEM_DMACHAN(base), mem_dmachan_read, mem_dmachan_write, cs);
	IOH_New16(MEM_DMASOF(base), mem_dmasof_read, mem_dmasof_write, cs);
	IOH_New16(MEM_DMAFC(base), mem_dmafc_read, mem_dmafc_write, cs);
	IOH_New16(MEM_RXDMABC(base), mem_rxdmabc_read, mem_rxdmabc_write, cs);
	IOH_New32(MEM_MEMBASE(base), mem_membase_read, mem_membase_write, cs);
	IOH_New32(MEM_BPROMBASE(base), mem_bprombase_read, mem_bprombase_write, cs);
	IOH_New32(MEM_BPROMAMASK(base), mem_bpromamask_read, mem_bpromamask_write, cs);
	IOH_New16(MEM_EEPROMCMD(base), mem_eepromcmd_read, mem_eepromcmd_write, cs);
	IOH_New16(MEM_EEPROMDATA(base), mem_eepromdata_read, mem_eepromdata_write, cs);
	IOH_New16(MEM_RCVFBC(base), mem_rcvfbc_read, mem_rcvfbc_write, cs);
	IOH_New16(MEM_RXCFG(base), mem_rxcfg_read, mem_rxcfg_write, cs);
	IOH_New16(MEM_RXCTL(base), mem_rxctl_read, mem_rxctl_write, cs);
	IOH_New16(MEM_TXCFG(base), mem_txcfg_read, mem_txcfg_write, cs);
	IOH_New16(MEM_TXCMDSTAT(base), mem_txcmdstat_read, NULL, cs);
	IOH_New16(MEM_BUFCFG(base), mem_bufcfg_read, mem_bufcfg_write, cs);
	IOH_New16(MEM_LINECTL(base), mem_linectl_read, mem_linectl_write, cs);
	IOH_New16(MEM_SELFCTL(base), mem_selfctl_read, mem_selfctl_write, cs);
	IOH_New16(MEM_BUSCTL(base), mem_busctl_read, mem_busctl_write, cs);
	IOH_New16(MEM_TESTCTL(base), mem_testctl_read, mem_testctl_write, cs);

	IOH_New16(MEM_TXCMD(base), NULL, mem_txcmd_write, cs);
	IOH_New16(MEM_TXLENGTH(base), mem_txlength_read, mem_txlength_write, cs);
	IOH_New16(MEM_RXEVENT(base), mem_rxevent_read, mem_rxevent_write, cs);
	IOH_New16(MEM_BUFEVENT(base), mem_bufevent_read, mem_bufevent_write, cs);
	IOH_New16(MEM_RXMISS(base), mem_rxmiss_read, mem_rxmiss_write, cs);
	IOH_New16(MEM_TXCOL(base), mem_txcol_read, mem_txcol_write, cs);
	IOH_New16(MEM_TXEVENT(base), mem_txevent_read, mem_txevent_write, cs);
	IOH_New16(MEM_LINEST(base), mem_linest_read, mem_linest_write, cs);
	IOH_New16(MEM_SELFST(base), mem_selfst_read, mem_selfst_write, cs);
	IOH_New16(MEM_BUSST(base), mem_busst_read, mem_busst_write, cs);
	IOH_New16(MEM_AUITDR(base), mem_auitdr_read, mem_auitdr_write, cs);
	flags = IOH_FLG_OSZR_NEXT | IOH_FLG_OSZW_NEXT | IOH_FLG_PWR_RMW | IOH_FLG_PRD_RARP;
	for (i = 0; i < 8; i += 2) {
		IOH_New8f(MEM_LAF(base) + i, mem_laf_read, mem_laf_write, cs, flags);
	}
	for (i = 0; i < 6; i += 2) {
		IOH_New16f(MEM_INDADDR(base) + i, mem_indaddr_read, mem_indaddr_write, cs, flags);
	}
	IOH_New16(MEM_RXSTAT(base), mem_rxstat_read, mem_rxstat_write, cs, flags);
	IOH_New16(MEM_RXLENGTH(base), mem_rxlength_read, mem_rxlength_write, cs, flags);

	/* regions only can start aligned to multiple of 256 bytes */
	IOH_NewRegion(MEM_RXSTAT(base), 0x600, mem_rxframe_read, mem_rxframe_write, HOST_BYTEORDER,
		      cs);
	IOH_NewRegion(MEM_TXFRAME(base), 0x600, mem_txframe_read, mem_txframe_write, HOST_BYTEORDER,
		      cs);
	/* Map memory block with rx and tx frame here */
}

/*
 * Warning ! currently incomplete !!!
 */
static void
CS8900_MemUnmap(CS8900 * cs, uint32_t base)
{
	int i;
	IOH_Delete32(MEM_PROD_ID(base));
	IOH_Delete16(MEM_IOBASE(base));
	IOH_Delete16(MEM_IOBASE(base));
	IOH_Delete16(MEM_DMACHAN(base));
	IOH_Delete16(MEM_DMASOF(base));
	IOH_Delete16(MEM_DMAFC(base));
	IOH_Delete16(MEM_RXDMABC(base));
	IOH_Delete32(MEM_MEMBASE(base));
	IOH_Delete32(MEM_BPROMBASE(base));
	IOH_Delete32(MEM_BPROMAMASK(base));
	IOH_Delete16(MEM_EEPROMCMD(base));
	IOH_Delete16(MEM_EEPROMDATA(base));
	IOH_Delete16(MEM_RCVFBC(base));
	IOH_Delete16(MEM_RXCFG(base));
	IOH_Delete16(MEM_RXCTL(base));
	IOH_Delete16(MEM_TXCFG(base));
	IOH_Delete16(MEM_TXCMD(base));
	IOH_Delete16(MEM_TXLENGTH(base));
	for (i = 0; i < 8; i += 2) {
		IOH_Delete16(MEM_LAF(base) + i);
	}
	for (i = 0; i < 6; i += 2) {
		IOH_Delete8(MEM_INDADDR(base) + i);
	}
	IOH_Delete16(MEM_RXSTAT(base));
	IOH_Delete16(MEM_RXLENGTH(base));
	/* Unap memory block with rx and tx frame here */
	IOH_DeleteRegion(MEM_RXSTAT(base), 0x600);
	IOH_DeleteRegion(MEM_TXFRAME(base), 0x600);
}
#endif
static void
CS8900_IoUnmap(CS8900 * cs, uint32_t base)
{
	IOH_Delete16(IO_RXTXDATA0(base));
	IOH_Delete16(IO_RXTXDATA1(base));
	IOH_Delete16(IO_TXCMD(base));
	IOH_Delete16(IO_TXLENGTH(base));
	IOH_Delete16(IO_ISQ(base));
	IOH_Delete16(IO_PPTR(base));
	IOH_Delete16(IO_PDATA0(base));
	IOH_Delete16(IO_PDATA1(base));

}

static void
CS8900_IoMap(CS8900 * cs, uint32_t base)
{
	uint32_t flags;
	IOH_New16(IO_RXTXDATA0(base), io_rxtxdata0_read, io_rxtxdata0_write, cs);
	IOH_New16(IO_RXTXDATA1(base), io_rxtxdata1_read, io_rxtxdata1_write, cs);
	IOH_New16(IO_TXCMD(base), NULL, io_txcmd_write, cs);
	IOH_New16(IO_TXLENGTH(base), io_txlength_read, io_txlength_write, cs);
	IOH_New16(IO_ISQ(base), io_isq_read, io_isq_write, cs);
	IOH_New16(IO_PPTR(base), io_pptr_read, io_pptr_write, cs);
	flags = IOH_FLG_OSZR_NEXT | IOH_FLG_OSZW_NEXT;
	IOH_New16f(IO_PDATA0(base), io_pdata0_read, io_pdata0_write, cs, flags);
	IOH_New16f(IO_PDATA1(base), io_pdata1_read, io_pdata1_write, cs, flags);
}

static void
CS8900_Unmap(void *owner, uint32_t base, uint32_t mask)
{
	CS8900 *cs = (CS8900 *) owner;
	CS8900_IoUnmap(cs, base);

}

static void
CS8900_Map(void *owner, uint32_t base, uint32_t mask, uint32_t flags)
{
	CS8900 *cs = (CS8900 *) owner;
	CS8900_IoMap(cs, base);
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

BusDevice *
CS8900_New(const char *devname)
{
	CS8900 *cs;
	int i;
	char *eepromname = alloca(strlen(devname) + 20);
	cs = sg_new(CS8900);
	for (i = 0; i < 4; i++) {
		cs->intrqNode[i] = SigNode_New("%s.intrq%d", devname, i);
		if (!cs->intrqNode[i]) {
			fprintf(stderr, "CS8900: can not create interrupt request node\n");
			exit(1);
		}
	}
	cs->txbuf = cs->memwin + 0x600;
	cs->ether_fd = Net_CreateInterface(devname);
	fcntl(cs->ether_fd, F_SETFL, O_NONBLOCK);
	sprintf(eepromname, "%s.eeprom", devname);
	cs->eeprom = m93c46_New(eepromname);
	cs8900_reset(cs);
	cs->bdev.first_mapping = NULL;
	cs->bdev.Map = CS8900_Map;
	cs->bdev.UnMap = CS8900_Unmap;
	cs->bdev.owner = cs;
	cs->bdev.hw_flags = MEM_FLAG_WRITABLE | MEM_FLAG_READABLE;
	generate_random_hwaddr(cs->indaddr);	/* Until eeprom reading works */
	fprintf(stderr, "Crystal LAN CS8900 Ethernet controller created\n");
	return &cs->bdev;
}
