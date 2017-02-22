/*
 **************************************************************************************************
 *
 * Emulation of the STE10/100 PCI Ethernet Controller 
 *
 * State: 
 *	Working with the linux Tulip driver
 *	WARNING: Please note that the real chips PHY does not work reliably 
 *	It sometimes stops receiving without giving any hint to the
 *	user. Without the workaround from STM it does never start
 *	receiving in 100MBit full duplex without autonegotioation.
 *	There are cases where the workaround does not work.
 *	So do not use this chip in your design. ST-Microelectronics
 *	told me that only A4 revision has this problem, but I have
 *	exactly the same problem with the newest (B2) revision.	
 *
 *
 * Copyright 2004 2005 Jochen Karrer. All rights reserved.
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
#include "ste10_100.h"

// include system header
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>

// include library header

// include user header
#include "signode.h"
#include "m93c46.h"
#include "linux-tap.h"
#include "cycletimer.h"
#include "sgstring.h"
#include "asyncmanager.h"


#if 0
#define dbgprintf(...) { fprintf(stderr,__VA_ARGS__); }
#else
#define dbgprintf(...)
#endif
/*
 * -----------------------------------------
 * Register offsets relative to Baseaddress
 * -----------------------------------------
 */
#define REG_CSR0  (0x00)
#define REG_CSR1  (0x08)
#define REG_CSR2  (0x10)
#define REG_CSR3  (0x18)
#define REG_CSR4  (0x20)
#define REG_CSR5  (0x28)
#define REG_CSR6  (0x30)
#define REG_CSR7  (0x38)
#define REG_CSR8  (0x40)
#define REG_CSR9  (0x48)
#define REG_CSR10 (0x50)
#define REG_CSR11 (0x58)
#define REG_CSR12 (0x60)
#define REG_CSR13 (0x68)
#define REG_CSR14 (0x70)
#define REG_CSR15 (0x78)
#define REG_CSR16 (0x80)
#define REG_CSR17 (0x84)
#define REG_CSR18 (0x88)
#define REG_CSR19 (0x8c)
#define REG_CSR20 (0x90)
#define REG_CSR21 (0x94)
#define REG_CSR22 (0x98)
#define REG_CSR23 (0x9c)
#define REG_CSR24 (0xa0)
#define REG_CSR25  (0xa4)
#define REG_CSR26  (0xa8)
#define REG_CSR27  (0xac)
#define REG_CSR28  (0xb0)

/* Transceiver Register */
#define REG_XR0	   (0xb4)
#define REG_XR1	   (0xb8)
#define REG_XR2	   (0xbc)
#define REG_XR3	   (0xc0)
#define REG_XR4	   (0xc4)
#define REG_XR5	   (0xc8)
#define REG_XR6	   (0xcc)
#define REG_XR7	   (0xd0)
#define REG_XR8	   (0xd4)
#define REG_XR9	   (0xd8)
#define REG_XR10   (0xdc)

#define CSR(ste,nr) ((ste)->csr[(nr)>>2])
#define CSR0(ste) CSR(ste,REG_CSR0)
#define CSR1(ste) CSR(ste,REG_CSR1)
#define CSR2(ste) CSR(ste,REG_CSR2)
#define CSR3(ste) CSR(ste,REG_CSR3)
#define CSR4(ste) CSR(ste,REG_CSR4)
#define CSR5(ste) CSR(ste,REG_CSR5)
#define CSR6(ste) CSR(ste,REG_CSR6)
#define CSR7(ste) CSR(ste,REG_CSR7)
#define CSR8(ste) CSR(ste,REG_CSR8)
#define CSR9(ste) CSR(ste,REG_CSR9)
#define CSR10(ste) CSR(ste,REG_CSR10)
#define CSR11(ste) CSR(ste,REG_CSR11)
#define CSR12(ste) CSR(ste,REG_CSR12)
#define CSR13(ste) CSR(ste,REG_CSR13)
#define CSR14(ste) CSR(ste,REG_CSR14)
#define CSR15(ste) CSR(ste,REG_CSR15)
#define CSR16(ste) CSR(ste,REG_CSR16)
#define CSR17(ste) CSR(ste,REG_CSR17)
#define CSR18(ste) CSR(ste,REG_CSR18)
#define CSR19(ste) CSR(ste,REG_CSR19)
#define CSR20(ste) CSR(ste,REG_CSR20)
#define CSR21(ste) CSR(ste,REG_CSR21)
#define CSR22(ste) CSR(ste,REG_CSR22)
#define CSR23(ste) CSR(ste,REG_CSR23)
#define CSR24(ste) CSR(ste,REG_CSR24)
#define CSR25(ste) CSR(ste,REG_CSR25)
#define CSR26(ste) CSR(ste,REG_CSR26)
#define CSR27(ste) CSR(ste,REG_CSR27)
#define CSR28(ste) CSR(ste,REG_CSR28)

#define XR(ste,n) (ste->csr[(n)>>2])
#define XR0(ste) 	XR(ste,REG_XR0)
#define XR1(ste) 	XR(ste,REG_XR1)
#define XR2(ste) 	XR(ste,REG_XR2)
#define XR3(ste)	XR(ste,REG_XR3)
#define XR4(ste) 	XR(ste,REG_XR4)
#define XR5(ste) 	XR(ste,REG_XR5)
#define XR6(ste) 	XR(ste,REG_XR6)
#define XR7(ste) 	XR(ste,REG_XR7)
#define XR8(ste) 	XR(ste,REG_XR8)
#define XR9(ste) 	XR(ste,REG_XR9)
#define XR10(ste) 	XR(ste,REG_XR10)

/*
 * -----------------------------------
 * Aliases with the Register Names
 * -----------------------------------
 */
#define PAR(ste)  	CSR0(ste)
#define TDR(ste)  	CSR1(ste)
#define RDR(ste)  	CSR2(ste)
#define RDB(ste)  	CSR3(ste)
#define TDB(ste)  	CSR4(ste)
#define SR(ste)   	CSR5(ste)
#define NAR(ste)  	CSR6(ste)
#define IER(ste)  	CSR7(ste)
#define LPC(ste)  	CSR8(ste)
#define SPR(ste)  	CSR9(ste)
#define TMR(ste)  	CSR11(ste)
#define WCSR(ste) 	CSR13(ste)
#define WPDR(ste)  	CSR14(ste)
#define WTMR(ste)  	CSR15(ste)
#define ACSR5(ste) 	CSR16(ste)
#define ACSR7(ste) 	CSR17(ste)
#define CR(ste)	  	CSR18(ste)
#define PCIC(ste)  	CSR19(ste)
#define PMCSR(ste) 	CSR20(ste)
#define TXBR(ste)  	CSR23(ste)
#define FROM(ste)  	CSR24(ste)
#define MAR0(ste)  	CSR27(ste)
#define MAR1(ste)  	CSR28(ste)

#define XCR(ste)  	XR0(ste)
#define XSR(ste)   	XR1(ste)
#define PID1(ste)  	XR2(ste)
#define PID2(ste)  	XR3(ste)
#define ANA(ste)	XR4(ste)
#define ANLPA(ste) 	XR5(ste)
#define ANE(ste)   	XR6(ste)
#define XMC(ste)   	XR7(ste)
#define XCIIS(ste) 	XR8(ste)
#define XIE(ste)   	XR9(ste)
#define CTR100(ste) 	XR10(ste)
#define		CTR100_DISCRM (1<<0)
#define		CTR100_DISMLT (1<<1)

typedef struct STE10_100 {
	PCI_Function pcifunc;
	PCI_Function *bridge;
	int bus_irq;		// irqline of parent
	int dev_nr;

	int ether_fd;		// file descriptor for the data
	PollHandle_t *input_fh;
	int receiver_is_enabled;
	SigNode *irqNode[4];
	int interrupt_posted;

	uint16_t device_id;
	uint16_t vendor_id;
	uint16_t command;
	uint16_t status;
	uint8_t latency_timer;
	uint8_t header_type;
	uint8_t cacheline_size;
	uint32_t bar0;
	uint32_t bar1;
	uint32_t bar2;
	uint32_t bar3;
	uint32_t bar4;
	uint32_t bar5;
	uint32_t sub_vendor_id;
	uint32_t sub_device_id;
	uint8_t irq_line;
	uint8_t irq_pin;
	uint8_t min_gnt;
	uint8_t max_lat;
	uint32_t driver_space;
	M93C46 *eprom;
	SigNode *edo;
	SigNode *edi;
	SigNode *eck;
	SigNode *eecs;

	uint32_t csr[64];
	uint8_t mac[6];
	uint32_t int_rxbufp;
	uint32_t int_txbufp;
	uint32_t *xr;
	int gptmr_stopped;
	CycleTimer gptmr;
	CycleCounter_t gptmr_last_update;
	int32_t gptmr_count;

	uint8_t rx_fifo[2048];
	uint32_t rxfifo_count;
} STE10_100;
#define MODIFY_MASKED(v,mod,mask) { (v) = ((v) & ~(mask)) | ((mod)&(mask)); }

void
STE_Reset(STE10_100 * ste)
{
	CSR6(ste) = 0x00080040;
	CSR8(ste) = 0;		//  LPC
	CSR9(ste) = 0xf;	// Serial
}

#define PAR_BAR		(1<<0)
#define PAR_DSL(x)	(((x)>>1)&0x1f)
#define PAR_BLE		(1<<7)
#define PAR_PBL(x)	(((x)>>8)&0x3f)
#define PAR_CAL(x)	(((x)>>14)&3)
#define PAR_TAP(x)	(((x)>>17)&3)
#define PAR_MRME	(1<<21)
#define PAR_MRLE	(1<<23)
#define PAR_MWIE	(1<<24)

#define SR_TCI 		(1)
#define SR_TPS 		(1<<1)
#define SR_TDU 		(1<<2)
#define SR_TJT 		(1<<3)
#define SR_TUF 		(1<<5)
#define SR_RCI 		(1<<6)
#define SR_RDU 		(1<<7)
#define SR_RPS 		(1<<8)
#define SR_RWT 		(1<<9)
#define SR_GPTT 	(1<<11)
#define SR_FBE  	(1<<13)
#define SR_AISS 	(1<<15)
#define SR_NISS 	(1<<16)
#define SR_RS_MASK 	(7<<17)
#define SR_RS_SHIFT 	(17)
#define 	SR_RS_STOP 	(0)
#define 	SR_RS_READD	(1<<17)
#define 	SR_RS_CHECK	(2<<17)
#define 	SR_RS_WAIT	(3<<17)
#define 	SR_RS_SUSPENDED	(4<<17)
#define 	SR_RS_FLUSH	(6<<17)
#define		SR_RS_WRITED	(7<<17)
#define SR_TS_MASK 	(7<<20)
#define SR_TS_SHIFT 	(20)
#define		SR_TS_STOP	(0)
#define		SR_TS_READD	(1<<20)
#define		SR_TS_TRANSMIT	(2<<20)
#define		SR_TS_FILL	(3<<20)
#define		SR_TS_SUSPENDED	(6<<20)
#define		SR_TS_WRITED	(7<<20)

#define SR_BET_MASK 	(7<<23)
#define SR_BET_SHIFT 	(23)

#define NAR_SR		(1<<1)
#define NAR_PB		(1<<3)
#define NAR_SBC		(1<<5)
#define NAR_PR		(1<<6)
#define NAR_MM		(1<<7)
#define NAR_OM_MASK	(3<<10)
#define NAR_OM_SHIFT    (10)
#define NAR_FC		(1<<12)
#define NAR_ST		(1<<13)
#define NAR_TR_MASK	(3<<14)
#define NAR_TR_SHIFT	(14)
#define NAR_SQE		(1<<19)
#define NAR_SF		(1<<21)

#define IER_TCIE	(1<<0)
#define IER_TPSIE	(1<<1)
#define IER_TDUIE	(1<<2)
#define IER_TJTTIE	(1<<3)
#define IER_TUIE	(1<<5)
#define IER_RCIE	(1<<6)
#define IER_RUIE	(1<<7)
#define IER_RSIE	(1<<8)
#define IER_RWTIE	(1<<9)
#define IER_GPTIE	(1<<11)
#define IER_FBEIE	(1<<13)
#define IER_AIE		(1<<15)
#define IER_NIE		(1<<16)

#define NORMAL_INT_MASK (SR_TCI |  SR_RCI | SR_TDU)
#define ABNORMAL_INT_MASK (SR_TPS |  SR_TJT | SR_TUF | SR_RDU | SR_RPS | SR_RWT | SR_GPTT | SR_FBE)
static void
update_interrupts(STE10_100 * ste, int line)
{
	uint32_t ier = IER(ste);
	uint32_t sr = SR(ste);
	uint32_t pending = sr & ier;
	dbgprintf("Update interrupts %08x, posted %d\n", pending, ste->interrupt_posted);
	if (((pending & NORMAL_INT_MASK) && (ier & IER_NIE))
	    || ((pending & ABNORMAL_INT_MASK) && (ier & IER_AIE))
	    ) {
		if (!ste->interrupt_posted) {
			dbgprintf("Poste Interrupt\n");
			PCI_PostIRQ(ste->bridge, ste->bus_irq);
			ste->interrupt_posted = 1;
		} else {
			dbgprintf("already posted\n");
		}
	} else {
		if (ste->interrupt_posted) {
			dbgprintf("Un Interrupt\n");
			PCI_UnPostIRQ(ste->bridge, ste->bus_irq);
			ste->interrupt_posted = 0;
		} else {
			dbgprintf("already UNposted\n");
		}
	}
}

/*
 * ------------------------------------------------
 * Emulation General Purpose timer 
 * ------------------------------------------------
 */
void
update_gptimer(STE10_100 * ste)
{
	CycleCounter_t cyclediff;
	CycleCounter_t now;
	int32_t period = (TMR(ste) & 0xffff);
	int periodic = TMR(ste) & (1 << 16);
	uint32_t cycles_per_count = MicrosecondsToCycles(205);
	int64_t gp_countdiff;
	int64_t gp_wraps;
	if (ste->gptmr_stopped) {
		return;
	}
	now = CycleCounter_Get();
	cyclediff = now - ste->gptmr_last_update;
	gp_countdiff = cyclediff / cycles_per_count;
	ste->gptmr_last_update = ste->gptmr_last_update + gp_countdiff * cycles_per_count;
	if (period > 0) {
		if (!periodic) {
			if (gp_countdiff >= ste->gptmr_count) {
				ste->gptmr_count = 0;
			} else {
				ste->gptmr_count -= gp_countdiff;
				if (!(SR(ste) & SR_GPTT)) {
					CycleTimer_Mod(&ste->gptmr,
						       ste->gptmr_count * cycles_per_count);
				}
			}
		} else {
			if (gp_countdiff < ste->gptmr_count) {
				ste->gptmr_count -= gp_countdiff;
			} else {
				gp_wraps = 1 + (gp_countdiff - ste->gptmr_count) / period;
				ste->gptmr_count =
				    ste->gptmr_count - (gp_countdiff - gp_wraps * period);
				/* can this happen ? */
				if (ste->gptmr_count == 0) {
					ste->gptmr_count = period;
				}
			}
			if (!(SR(ste) & SR_GPTT)) {
				CycleTimer_Mod(&ste->gptmr, ste->gptmr_count * cycles_per_count);
			}
		}
	} else {
		CycleTimer_Remove(&ste->gptmr);
	}
}

static void
gptmr_timeout(void *clientData)
{
	STE10_100 *ste = clientData;
	SR(ste) |= SR_GPTT;
	update_interrupts(ste, __LINE__);
}

static void
disable_receiver(STE10_100 * ste)
{
	dbgprintf("ste10/100: disable receiver\n");
	if (ste->receiver_is_enabled) {
		AsyncManager_PollStop(ste->input_fh);
		ste->receiver_is_enabled = 0;
	}
	SR(ste) = (SR(ste) & ~SR_RS_MASK) | SR_RS_STOP;
}

static inline void
clear_rxfifo(STE10_100 * ste)
{
	ste->rxfifo_count = 0;
}

#define RDES0_OWN (1<<31)
#define RDES0_LS (1<<8)
#define RDES0_FS (1<<9)
#define RDES1_CHAIN (1<<24)
#define RDES1_RER   (1<<25)

static int
fill_rxfifo(STE10_100 * ste)
{
	int result;
	char buf[2048];
	if (ste->rxfifo_count) {
		dbgprintf("Paket loss, rxfifo is full !\n");
		return read(ste->ether_fd, buf, 2048);
	}
	result = read(ste->ether_fd, ste->rx_fifo, 2048);
	if (result > 0) {
		ste->rxfifo_count = result;
	}
	return result;
}

static inline uint32_t
pci_descriptor_read32(STE10_100 * ste, uint32_t addr)
{
	return PCI_MasterRead32LE(ste->bridge, addr);
}

static inline void
pci_descriptor_write32(STE10_100 * ste, uint32_t value, uint32_t addr)
{
	PCI_MasterWrite32LE(ste->bridge, value, addr);
}

static inline void
pci_data_write(STE10_100 * ste, uint32_t addr, uint8_t * buf, uint32_t size)
{
	if (unlikely((PAR(ste) & PAR_BLE))) {
		fprintf(stderr, "PCI Master Write Big Endian is missing\n");
	} else {
		PCI_MasterWriteLE(ste->bridge, addr, buf, size);
	}
}

static inline void
pci_data_read(STE10_100 * ste, uint32_t addr, uint8_t * buf, uint32_t size)
{
	if (unlikely((PAR(ste) & PAR_BLE))) {
		fprintf(stderr, "PCI Master Write Big Endian is missing\n");
	} else {
		PCI_MasterReadLE(ste->bridge, addr, buf, size);
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
 * Check if incoming packet can be accepted
 * Multicast hash table is missing
 * ----------------------------------------------
 */
static int
match_address(STE10_100 * ste)
{
	if (is_broadcast(ste->rx_fifo)) {
		return 1;
	}
	/* check for promiscous mode */
	if (NAR(ste) & NAR_PR) {
		return 2;
	}
	if (memcmp(ste->rx_fifo, ste->mac, 6) != 0) {
		return 0;
	}
	return 3;
}

static inline int
receive(STE10_100 * ste)
{
	uint32_t buflen1 = 0, buflen2 = 0;
	uint32_t buf1;
	uint32_t buf2;
	uint32_t rdes0, rdes1, rdes2, rdes3;
	uint32_t chain;
	uint8_t *data = ste->rx_fifo;
	int count;
	// check if we are stopped
	if (SR(ste) & SR_RPS) {
		fprintf(stderr, "STE10/100 Warning: receive while RPS\n");
		clear_rxfifo(ste);
		return 0;
	}
	if (!match_address(ste)) {
		clear_rxfifo(ste);
		return 0;
	}
	rdes0 = pci_descriptor_read32(ste, ste->int_rxbufp);
	rdes1 = pci_descriptor_read32(ste, ste->int_rxbufp + 4);
	rdes2 = buf1 = pci_descriptor_read32(ste, ste->int_rxbufp + 8);
	rdes3 = buf2 = pci_descriptor_read32(ste, ste->int_rxbufp + 12);
	chain = rdes1 & RDES1_CHAIN;
	count = ste->rxfifo_count;
	if (!(rdes0 & RDES0_OWN)) {
		//  Receive suspended  
		SR(ste) |= SR_RDU;
		SR(ste) = (SR(ste) & ~SR_RS_MASK) | SR_RS_SUSPENDED;
		update_interrupts(ste, __LINE__);
		disable_receiver(ste);
		return -1;
	}
	buflen1 = rdes1 & 0x7ff;
	if (!chain) {
		buflen2 = (rdes1 >> 11) & 0x7ff;
	}
	if (buflen1 + buflen2 >= count) {
		int size1 = (count <= buflen1) ? count : buflen1;
		int size2 = (count <= buflen1) ? 0 : count - buflen1;
		pci_data_write(ste, buf1, data, size1);
		pci_data_write(ste, buf2, data, size2);
		clear_rxfifo(ste);
		/* set Paket length including 4 Byte CRC */
		rdes0 = (rdes0 & (~0x7fff0000)) | ((count + 4) << 16);
		rdes0 &= ~RDES0_OWN;
		rdes0 |= RDES0_FS | RDES0_LS;
		pci_descriptor_write32(ste, rdes0, ste->int_rxbufp);
	} else {
		dbgprintf("ste10/100 Warning: Rx Buffer smaller than paket\n");
	}

	if (rdes1 & RDES1_RER) {
		/*  End of Ring start from beginning */
		ste->int_rxbufp = CSR3(ste);
	} else {
		/* Advance to next entry */
		if (chain) {
			ste->int_rxbufp = rdes3;
		} else {
			ste->int_rxbufp += 16;
		}
	}
	SR(ste) |= SR_RCI;
	update_interrupts(ste, __LINE__);
	return 0;
}

static void
input_event(PollHandle_t *handle, int status, int events, void *clientdata)
{
	STE10_100 *ste = clientdata;
	int result;
	do {
		result = fill_rxfifo(ste);
		if (ste->rxfifo_count) {
			receive(ste);
		}
	} while (result > 0);
	return;
}

static void
enable_receiver(STE10_100 * ste)
{
	if (!ste->receiver_is_enabled && (ste->ether_fd >= 0)) {
		dbgprintf("ste10/100: enable receiver\n");
		AsyncManager_PollStart(ste->input_fh, ASYNCMANAGER_EVENT_READABLE, &input_event, ste);
		ste->receiver_is_enabled = 1;
	}
}

#define TDES0_OWN (1<<31)

#define TDES1_CHAIN (1<<24)
#define TDES1_TER   (1<<25)
#define TDES1_IC   (1<<31)
#define TDES1_LS  (1<<30)
#define TDES1_FS  (1<<29)

static void
transmit(STE10_100 * ste)
{
	int j;
	int count;
	uint32_t chain;
	uint8_t data[2048];
	uint32_t buf1, buf2;
	uint32_t tdes0, tdes1, tdes2, tdes3;
	uint32_t len1, len2;
	if (!(NAR(ste) & NAR_ST)) {
		fprintf(stderr,
			"STE10/100 warning: Try to transmit while transmitter is stopped\n");
	}
	for (j = 0; j < 1024; j++) {
		tdes0 = pci_descriptor_read32(ste, ste->int_txbufp);
		tdes1 = pci_descriptor_read32(ste, ste->int_txbufp + 4);
		buf1 = tdes2 = pci_descriptor_read32(ste, ste->int_txbufp + 8);
		buf2 = tdes3 = pci_descriptor_read32(ste, ste->int_txbufp + 12);
		dbgprintf("ste10/100 Transmit called\n");
		if (!(tdes0 & TDES0_OWN)) {
			SR(ste) |= SR_TDU;
			SR(ste) = (SR(ste) & ~SR_TS_MASK) | SR_TS_SUSPENDED;
			update_interrupts(ste, __LINE__);
			if (j == 0) {
				fprintf(stderr,
					"Warning: Transmit called but nothing to transmit\n");
			}
			return;
		}
		if ((tdes1 & (TDES1_LS | TDES1_FS)) != (TDES1_LS | TDES1_FS)) {
			fprintf(stderr, "Multi-Buffer frames not implemented\n");
			return;
		}
		chain = tdes1 & TDES1_CHAIN;
		len1 = tdes1 & 0x7ff;
		len2 = (tdes1 >> 11) & 0x7ff;
		dbgprintf("Transmit called\n");
		dbgprintf("buffer1 address %08x, len %d\n", tdes2, len1);
		dbgprintf("buffer2 address %08x, len %d\n", tdes3, len2);
		if (len1) {
			dbgprintf("\n");
			pci_data_read(ste, buf1, data, len1);
			count = 0;
			do {
				int result;
				result = write(ste->ether_fd, data + count, len1 - count);
				if (result > 0) {
					count += result;
				} else {
					if (errno != EAGAIN) {
						fprintf(stderr,
							"Transmit error writing to TAP fd\n");
						return;
					}
				}
			} while (count < len1);
		}
		if (len2 && !chain) {
			pci_data_read(ste, buf2, data, len2);
			count = 0;
			do {
				int result;
				result = write(ste->ether_fd, data + count, len2 - count);
				if (result > 0) {
					count += result;
				} else {
					if (errno != EAGAIN) {
						fprintf(stderr,
							"Transmit error writing to TAP fd\n");
						return;
					}
				}
			} while (count < len2);
		}
		tdes0 = tdes0 & ~TDES0_OWN;
		pci_descriptor_write32(ste, tdes0, ste->int_txbufp);
		if (tdes1 & TDES1_IC) {
			SR(ste) |= SR_TCI;
			dbgprintf("ste10/100: Poste einen TX-Interrupt, csr5 %08x ier %08x \n",
				  SR(ste), IER(ste));
			update_interrupts(ste, __LINE__);
		}
		if (chain) {
			// update only if not last descriptor;
			if (!(tdes1 & TDES1_TER)) {
				ste->int_txbufp = tdes3;
			} else {
				//SR(ste) |= SR_TDU;
				update_interrupts(ste, __LINE__);
				return;
			}
		} else {
			if (!(tdes1 & TDES1_TER)) {
				/* Advance to next */
				ste->int_txbufp += 16;
			} else {
				/* Restart from first */
				ste->int_txbufp = CSR4(ste);
			}
		}
	}
	fprintf(stderr, "STE10/100: To Much work in Transmit fifo\n");
}

static void
ser_port_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	STE10_100 *ste = clientData;
	if ((value & 0x4800) != 0x4800) {
		//dbgprintf("access to serial eeprom not enabled (%04x)\n",value);      
		CSR9(ste) = value;
		return;
	}
	if (value & 0x1) {
		SigNode_Set(ste->eecs, SIG_HIGH);
	} else {
		SigNode_Set(ste->eecs, SIG_LOW);
	}
	if (value & 0x4) {
		SigNode_Set(ste->edi, SIG_HIGH);
	} else {
		SigNode_Set(ste->edi, SIG_LOW);
	}
	if (value & 0x2) {
		SigNode_Set(ste->eck, SIG_HIGH);
	} else {
		SigNode_Set(ste->eck, SIG_LOW);
	}
	//pinstate=m93c46_feed(ste->eprom,pinstate);
	if (SigNode_Val(ste->edo) == SIG_HIGH) {
		value |= 0x8;
	} else {
		value &= ~0x8;
	}
	CSR9(ste) = value;
	return;
}

static uint32_t
ser_port_read(void *clientData, uint32_t address, int rqlen)
{
	STE10_100 *ste = clientData;
	return CSR9(ste);
}

/*
 * -----------------------
 * Status register
 * -----------------------
 */
static uint32_t
sr_read(void *clientData, uint32_t address, int rqlen)
{
	STE10_100 *ste = clientData;
	uint32_t sr = SR(ste) & ~(SR_AISS | SR_NISS);
	if (sr & (SR_TCI | SR_RCI | SR_TDU)) {
		sr = sr | SR_NISS;
	}
	if (sr & (SR_TPS | SR_TJT | SR_TUF | SR_RDU | SR_RPS | SR_RWT | SR_GPTT | SR_FBE)) {
		sr = sr | SR_AISS;
	}
	dbgprintf("ste10/100 SR read %08x, value %08x\n", address, SR(ste));
	return sr;
}

static void
sr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	STE10_100 *ste = clientData;
	/* clear these bits by writing one */
	uint32_t mask = ~(value & (SR_NISS | SR_AISS | SR_FBE | SR_GPTT | SR_RWT | SR_RPS |
				   SR_RDU | SR_RCI | SR_TUF | SR_TJT | SR_TDU | SR_TPS | SR_TCI));
	uint32_t new_sr = SR(ste) & mask;
	uint32_t diff = SR(ste) ^ new_sr;
	SR(ste) = new_sr;
	if (diff & SR_GPTT) {
		update_gptimer(ste);
	}
	update_interrupts(ste, __LINE__);
	dbgprintf("ste10/100 SR of addr %08x, value %08x, mask %08x, new sr: %08x\n", address,
		  value, mask, SR(ste));
	return;
}

/*
 * ----------------------------------
 * Network access register
 * ----------------------------------
 */
static uint32_t
nar_read(void *clientData, uint32_t address, int rqlen)
{
	STE10_100 *ste = clientData;
	dbgprintf("ste10/100 NAR read %08x, value %08x\n", address, CSR6(ste));
	return CSR6(ste);
}

#define NAR_RESERVED_MASK 	0xffd70315

static void
nar_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	STE10_100 *ste = clientData;
	NAR(ste) = value | 0xff970115;
#if 0
	//uint32_t diff = NAR(ste) ^ value;
	if (diff & NAR_RESERVED_MASK) {
		fprintf(stderr,
			"STE10/100: change of reserved bits to %08x NAR, diff: %08x bits %08x\n",
			value, diff, diff & NAR_RESERVED_MASK);
	}
	fprintf(stderr, "NAR write %08x ste %p\n", value, ste);
#endif
	if (!(value & NAR_SR)) {
		uint32_t state = SR(ste) & SR_RS_MASK;
		if ((state != SR_RS_STOP) && (state != SR_RS_SUSPENDED)) {
			SR(ste) = SR(ste) | SR_RPS;
			/* STE10/100 does NOT go to state stop, linuxdriver complains about
			   this, but i want to emulate the real chip, and not the documentation 
			   so I do the same
			 */
			//      SR(ste) = (SR(ste) & ~SR_RS_MASK) | SR_RS_STOP;

			/*      disable_receiver(ste);  If I do this nothing works anymore */
		}
	} else {
		SR(ste) = SR(ste) & ~SR_RPS;
		/* Should try this with real device */
		SR(ste) = (SR(ste) & ~SR_RS_MASK) | SR_RS_WAIT;
	}
	if (!(value & NAR_ST)) {
		/* Stop transmitter */
		SR(ste) = SR(ste) | SR_TPS;
		SR(ste) = (SR(ste) & ~SR_TS_MASK) | SR_TS_STOP;
	} else {
		SR(ste) = SR(ste) & ~SR_TPS;
		/* should try this with real device */
		SR(ste) = (SR(ste) & ~SR_TS_MASK) | SR_TS_SUSPENDED;
	}
	dbgprintf("ste10/100 NAR of addr %08x, value %08x\n", address, value);
	return;
}

/*
 * Interrupt enable_register
 */

static uint32_t
ier_read(void *clientData, uint32_t address, int rqlen)
{
	STE10_100 *ste = clientData;
	dbgprintf("ste10/100 IER read %08x, value %08x\n", address, IER(ste));
	return IER(ste);
}

static void
ier_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	STE10_100 *ste = clientData;
	IER(ste) = value;
	if ((value & NORMAL_INT_MASK) && ((value & IER_NIE) == 0)) {
		fprintf(stderr, "Warning NISS not set but some normal interrupts\n");
	}
	if ((value & ABNORMAL_INT_MASK) && ((value & IER_AIE) == 0)) {
		fprintf(stderr, "Warning AISS not set but some abnormal interrupts\n");
	}
	update_interrupts(ste, __LINE__);
	dbgprintf("ste10/100 IER write %08x, value %08x\n", address, value);
	return;
}

/*
 * ------------------------------------------------------
 * LPC lost packet counter CSR8
 * 	Return always 0 
 * ------------------------------------------------------
 */
static uint32_t
lpc_read(void *clientData, uint32_t address, int rqlen)
{
	STE10_100 *ste = clientData;
	dbgprintf("ste10/100 LPC read %08x, value %08x\n", address, CSR8(ste));
	return CSR8(ste);
}

static void
lpc_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	dbgprintf("ste10/100 unhandled LPC of addr %08x, value %08x\n", address, value);
	return;
}

/*
 * ---------------------------------------
 * PCI-Access Register
 * ---------------------------------------
 */
static uint32_t
par_read(void *clientData, uint32_t address, int rqlen)
{
	STE10_100 *ste = clientData;
	dbgprintf("ste10/100 PAR read %08x, value %08x\n", address, PAR(ste) & ~(uint32_t) 1);
	return PAR(ste) & ~(uint32_t) 1;	// clear reste flag   
}

static void
par_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	STE10_100 *ste = clientData;
	dbgprintf("ste10/100 PAR write of addr %08x, value %08x\n", address, value);
	PAR(ste) = value;
	if (value & 1) {
		STE_Reset(ste);
	}
	if (PAR_TAP(PAR(ste)) != 0) {
		fprintf(stderr, "STE10/100 Warning: autopolling not implemented\n");
	}
	return;
}

/*
 * ---------------------------------------
 * CSR1 Transmit demand Register. 
 * 	Writing triggers a transfer from 
 *	the host memory to the fifo
 * ---------------------------------------
 */
static uint32_t
tdr_read(void *clientData, uint32_t address, int rqlen)
{
	//STE10_100 *ste=clientData;
	dbgprintf("ste10/100 TDR read %08x, value %08x\n", address, 0xffffffff);
	return 0xffffffff;	// ??? written value of always 0xffffffff ???
}

static void
tdr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	STE10_100 *ste = clientData;
	dbgprintf("ste10/100 TDR write of addr %08x, value %08x\n", address, value);
	CSR1(ste) = value;
	transmit(ste);
	return;
}

/*
 * ---------------------------------------
 * CSR2 Receive demand Register. 
 * 	Writing triggers a transfer to
 *	the host memory
 * ---------------------------------------
 */
static uint32_t
rdr_read(void *clientData, uint32_t address, int rqlen)
{
	dbgprintf("ste10/100 RDR read %08x, value %08x\n", address, 0xffffffff);
	return 0xffffffff;	// ??? written value of always 0xffffffff ???
}

static void
rdr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	STE10_100 *ste = clientData;
	dbgprintf("ste10/100 RDR write of addr %08x, value %08x\n", address, value);
	CSR2(ste) = value;
	SR(ste) = (SR(ste) & ~SR_RS_MASK) | SR_RS_WAIT;
	enable_receiver(ste);
	//input_event(ste,0); 
	return;
}

/*
 * ---------------------------------------
 * CSR3: Receive descriptor base Register
 * ---------------------------------------
 */
static uint32_t
rdb_read(void *clientData, uint32_t address, int rqlen)
{
	STE10_100 *ste = clientData;
	dbgprintf("ste10/100 RDB read %08x, value %08x\n", address, CSR3(ste));
	return CSR3(ste);
}

static void
rdb_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	STE10_100 *ste = clientData;
	dbgprintf("ste10/100 RDB write of addr %08x, value %08x\n", address, value);
	CSR3(ste) = value;
	ste->int_rxbufp = value;
	return;
}

/*
 * ---------------------------------------
 * CSR4: Transmit descriptor base Register
 * ---------------------------------------
 */
static uint32_t
tdb_read(void *clientData, uint32_t address, int rqlen)
{
	STE10_100 *ste = clientData;
	dbgprintf("ste10/100 TDB read %08x, value %08x\n", address, CSR4(ste));
	return CSR4(ste);
}

static void
tdb_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	STE10_100 *ste = clientData;
	dbgprintf("ste10/100 TDB write of addr %08x, value %08x\n", address, value);
	CSR4(ste) = value;
	ste->int_txbufp = value;
	return;
}

/* 
 * ---------------------------------------------------------------------
 * PAR (CSR25) Physical address register 
 *	contains the MAC address. 4 Bytes in PAR0 and 2 in PAR1
 * ---------------------------------------------------------------------
 */
static void
par0_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	STE10_100 *ste = clientData;
	if (value & 0x1) {
		fprintf(stderr, "STE: Warning, device address is broadcast address\n");
	}
	ste->mac[0] = value & 0xff;
	ste->mac[1] = (value >> 8) & 0xff;
	ste->mac[2] = (value >> 16) & 0xff;
	ste->mac[3] = (value >> 24) & 0xff;
	dbgprintf("ste10/100 PAR0 (csr25) write addr %08x, value %08x\n", address, value);
	return;
}

static uint32_t
par0_read(void *clientData, uint32_t address, int rqlen)
{
	STE10_100 *ste = clientData;
	uint8_t *mac = ste->mac;
	uint32_t par0 = mac[0] | (mac[1] << 8) | (mac[2] << 16) | (mac[3] << 24);
//      dbgprintf("ste10/100 csr25 read %08x, value %08x\n",address,par0);
	return par0;
}

static void
par1_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	STE10_100 *ste = clientData;
	uint8_t *mac = ste->mac;
	mac[4] = value & 0xff;
	mac[5] = (value >> 8) & 0xff;
	dbgprintf("ste10/100 PAR1 (csr26) write addr %08x, value %08x\n", address, value);
	return;
}

static uint32_t
par1_read(void *clientData, uint32_t address, int rqlen)
{
	STE10_100 *ste = clientData;
	uint8_t *mac = ste->mac;
	uint32_t par1;
//      dbgprintf("ste10/100 csr26 read %08x, value %08x\n",address,par1);
	par1 = mac[4] | (mac[5] << 8);
	return par1;
}

/* 
 * ------------------------------------------------
 * Multicast address
 * ------------------------------------------------
 */
static void
mar0_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	STE10_100 *ste = clientData;
	MAR0(ste) = value;
	dbgprintf("ste10/100 MAR0 write addr %08x, value %08x\n", address, value);
	return;
}

static uint32_t
mar0_read(void *clientData, uint32_t address, int rqlen)
{
	STE10_100 *ste = clientData;
	dbgprintf("ste10/100 MAR0 read %08x, value %08x\n", address, MAR0(ste));
	return MAR0(ste);
}

static void
mar1_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	STE10_100 *ste = clientData;
	MAR1(ste) = value;
	dbgprintf("ste10/100 MAR1 write addr %08x, value %08x\n", address, value);
	return;
}

static uint32_t
mar1_read(void *clientData, uint32_t address, int rqlen)
{
	STE10_100 *ste = clientData;
	dbgprintf("ste10/100 MAR1 read %08x, value %08x\n", address, MAR1(ste));
	return MAR1(ste);
}

static uint32_t
unhandled_read(void *clientData, uint32_t address, int rqlen)
{
	dbgprintf("ste10/100 unhandled read of addr %08x\n", address);
	return 0;
}

static void
unhandled_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	dbgprintf("ste10/100 unhandled write of addr %08x, value %08x\n", address, value);
	return;
}

/*
 * ------------------------------------------------------------
 * GP-Timer Register read/write 
 * 	Reading the General purpose timer does not return 
 * 	the count written to the TMR register. It returns
 * 	the actual count. The real device stops the counter
 * 	on read when it is not periodic
 * ------------------------------------------------------------
 */

static uint32_t
tmr_read(void *clientData, uint32_t address, int rqlen)
{
	STE10_100 *ste = clientData;
	uint32_t periodic = TMR(ste) & (1 << 16);
	update_gptimer(ste);
	if (!periodic) {
		ste->gptmr_stopped = 1;
		CycleTimer_Remove(&ste->gptmr);
	}
	dbgprintf("STE: GP-Timer read\n");
	return ste->gptmr_count | 0xfffe0000 | periodic;
}

static void
tmr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	STE10_100 *ste = clientData;
	TMR(ste) = value;
	ste->gptmr_count = value & 0xffff;
	ste->gptmr_last_update = CycleCounter_Get();
	ste->gptmr_stopped = 0;
	update_gptimer(ste);
	dbgprintf("STE: GP-Timer write\n");
	return;
}

static uint32_t
wcsr_read(void *clientData, uint32_t address, int rqlen)
{
	dbgprintf("ste10/100 unhandled read of addr %08x\n", address);
	return 0;
}

static void
wcsr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	dbgprintf("ste10/100 unhandled write of addr %08x, value %08x\n", address, value);
	return;
}

static uint32_t
wpdr_read(void *clientData, uint32_t address, int rqlen)
{
	dbgprintf("ste10/100 unhandled read of addr %08x\n", address);
	return 0;
}

static void
wpdr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	dbgprintf("ste10/100 unhandled write of addr %08x, value %08x\n", address, value);
	return;
}

static uint32_t
wtmr_read(void *clientData, uint32_t address, int rqlen)
{
	dbgprintf("ste10/100 unhandled read of addr %08x\n", address);
	return 0;
}

static void
wtmr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	dbgprintf("ste10/100 unhandled write of addr %08x, value %08x\n", address, value);
	return;
}

static uint32_t
acsr5_read(void *clientData, uint32_t address, int rqlen)
{
	dbgprintf("ste10/100 unhandled read of addr %08x\n", address);
	return 0;
}

static void
acsr5_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	dbgprintf("ste10/100 unhandled write of addr %08x, value %08x\n", address, value);
	return;
}

static uint32_t
acsr7_read(void *clientData, uint32_t address, int rqlen)
{
	dbgprintf("ste10/100 unhandled read of addr %08x\n", address);
	return 0;
}

static void
acsr7_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	dbgprintf("ste10/100 unhandled write of addr %08x, value %08x\n", address, value);
	return;
}

/*
 * ------------------------------------------------
 * Command register
 * ------------------------------------------------
 */
static uint32_t
cr_read(void *clientData, uint32_t address, int rqlen)
{
	STE10_100 *ste = clientData;
	dbgprintf("ste10/100 CR read %08x, value %08x\n", address, CSR18(ste));
	return CSR18(ste);
}

static void
cr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	STE10_100 *ste = clientData;
	dbgprintf("ste10/100 CR write of addr %08x, value %08x\n", address, value);
	CSR18(ste) = value;
	return;
}

static uint32_t
pcic_read(void *clientData, uint32_t address, int rqlen)
{
	dbgprintf("ste10/100 unhandled read of addr %08x\n", address);
	return 0;
}

static void
pcic_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	dbgprintf("ste10/100 unhandled write of addr %08x, value %08x\n", address, value);
	return;
}

static uint32_t
pmcsr_read(void *clientData, uint32_t address, int rqlen)
{
	dbgprintf("ste10/100 unhandled read of addr %08x\n", address);
	return 0;
}

static void
pmcsr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	dbgprintf("ste10/100 unhandled write of addr %08x, value %08x\n", address, value);
	return;
}

static uint32_t
txbr_read(void *clientData, uint32_t address, int rqlen)
{
	dbgprintf("ste10/100 unhandled read of addr %08x\n", address);
	return 0;
}

static void
txbr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	dbgprintf("ste10/100 unhandled write of addr %08x, value %08x\n", address, value);
	return;
}

static uint32_t
from_read(void *clientData, uint32_t address, int rqlen)
{
	dbgprintf("ste10/100 unhandled read of addr %08x\n", address);
	return 0;
}

static void
from_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	dbgprintf("ste10/100 unhandled write of addr %08x, value %08x\n", address, value);
	return;
}

static uint32_t
xcr_read(void *clientData, uint32_t address, int rqlen)
{
	STE10_100 *ste = clientData;
	return XCR(ste) & 0xff80;
}

static void
xcr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	STE10_100 *ste = clientData;
	XCR(ste) = value;
	return;
}

static uint32_t
xsr_read(void *clientData, uint32_t address, int rqlen)
{
	STE10_100 *ste = clientData;
	return XSR(ste);
}

static void
xsr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "ste10/100 XSR is not writable\n");
	return;
}

static uint32_t
pid1_read(void *clientData, uint32_t address, int rqlen)
{
	STE10_100 *ste = clientData;
	return PID1(ste);
}

static void
pid1_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	dbgprintf("ste10/100 PID1 not writable\n");
	return;
}

static uint32_t
pid2_read(void *clientData, uint32_t address, int rqlen)
{
	STE10_100 *ste = clientData;
	return PID2(ste);
}

static void
pid2_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	dbgprintf("ste10/100 PID2 not writable\n");
	return;
}

static uint32_t
ana_read(void *clientData, uint32_t address, int rqlen)
{
	STE10_100 *ste = clientData;
	dbgprintf("ste10/100 unhandled read of addr %08x\n", address);
	return ANA(ste);
}

static void
ana_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	STE10_100 *ste = clientData;
	ANA(ste) = (value & 0x25e1) | 1;
	dbgprintf("ste10/100 unhandled write of addr %08x, value %08x\n", address, value);
	return;
}

static uint32_t
anlpa_read(void *clientData, uint32_t address, int rqlen)
{
	STE10_100 *ste = clientData;
	return ANLPA(ste);
}

static void
anlpa_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	dbgprintf("ste10/100 unhandled write of addr %08x, value %08x\n", address, value);
	return;
}

static uint32_t
ane_read(void *clientData, uint32_t address, int rqlen)
{
	STE10_100 *ste = clientData;
	return ANE(ste);
}

static void
ane_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	STE10_100 *ste = clientData;
	ANE(ste) = value;
	return;
}

static uint32_t
xmc_read(void *clientData, uint32_t address, int rqlen)
{
	STE10_100 *ste = clientData;
	return XMC(ste);
}

static void
xmc_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	dbgprintf("ste10/100 unhandled write of addr %08x, value %08x\n", address, value);
	return;
}

static uint32_t
xciis_read(void *clientData, uint32_t address, int rqlen)
{
	STE10_100 *ste = clientData;
	return XCIIS(ste);
}

static void
xciis_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	dbgprintf("ste10/100: Register not writable: addr %08x, value %08x\n", address, value);
	return;
}

static uint32_t
xie_read(void *clientData, uint32_t address, int rqlen)
{
	dbgprintf("ste10/100 unhandled read of addr %08x\n", address);
	return 0;
}

static void
xie_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	dbgprintf("ste10/100 unhandled write of addr %08x, value %08x\n", address, value);
	return;
}

static uint32_t
ctr100_read(void *clientData, uint32_t address, int rqlen)
{
	STE10_100 *ste = clientData;
	return CTR100(ste);
}

static void
ctr100_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	STE10_100 *ste = clientData;
	uint32_t diff = CTR100(ste) ^ value;
	CTR100(ste) = (value & ~0xc000) | 0xc40 | 0x1000;

	if (diff & value & CTR100_DISCRM) {
		fprintf(stderr, "*** STE10/100: Possible driver bug ! disscrambler is disabled\n");
	}
	if (diff & value & CTR100_DISMLT) {
		fprintf(stderr, "*** STE10/100: Possible driver bug ! MLT3 is disabled\n");
	}
	dbgprintf("ste10/100 missing functionality: write of addr %08x, value %08x\n", address,
		  value);
	return;
}

typedef int MapProc(uint32_t addr, PCI_IOReadProc *, PCI_IOWriteProc *, void *cd);

static void
ste_map(STE10_100 * ste, MapProc * mapproc, uint32_t base)
{
	base = base & ~0x7f;
//      printf("Map io base %08x\n",base);
	mapproc(base + REG_CSR0, par_read, par_write, ste);
	mapproc(base + REG_CSR1, tdr_read, tdr_write, ste);
	mapproc(base + REG_CSR2, rdr_read, rdr_write, ste);
	mapproc(base + REG_CSR3, rdb_read, rdb_write, ste);
	mapproc(base + REG_CSR4, tdb_read, tdb_write, ste);
	mapproc(base + REG_CSR5, sr_read, sr_write, ste);
	mapproc(base + REG_CSR6, nar_read, nar_write, ste);
	mapproc(base + REG_CSR7, ier_read, ier_write, ste);
	mapproc(base + REG_CSR8, lpc_read, lpc_write, ste);
	mapproc(base + REG_CSR9, ser_port_read, ser_port_write, ste);
	mapproc(base + REG_CSR10, unhandled_read, unhandled_write, ste);

	mapproc(base + REG_CSR11, tmr_read, tmr_write, ste);
	mapproc(base + REG_CSR12, unhandled_read, unhandled_write, ste);
	mapproc(base + REG_CSR13, wcsr_read, wcsr_write, ste);
	mapproc(base + REG_CSR14, wpdr_read, wpdr_write, ste);
	mapproc(base + REG_CSR15, wtmr_read, wtmr_write, ste);
	mapproc(base + REG_CSR16, acsr5_read, acsr5_write, ste);
	mapproc(base + REG_CSR17, acsr7_read, acsr7_write, ste);
	mapproc(base + REG_CSR18, cr_read, cr_write, ste);
	mapproc(base + REG_CSR19, pcic_read, pcic_write, ste);
	mapproc(base + REG_CSR20, pmcsr_read, pmcsr_write, ste);
	mapproc(base + REG_CSR21, unhandled_read, unhandled_write, ste);
	mapproc(base + REG_CSR22, unhandled_read, unhandled_write, ste);
	mapproc(base + REG_CSR23, txbr_read, txbr_write, ste);
	mapproc(base + REG_CSR24, from_read, from_write, ste);
	mapproc(base + REG_CSR25, par0_read, par0_write, ste);
	mapproc(base + REG_CSR26, par1_read, par1_write, ste);
	mapproc(base + REG_CSR27, mar0_read, mar0_write, ste);
	mapproc(base + REG_CSR28, mar1_read, mar1_write, ste);
	mapproc(base + REG_XR0, xcr_read, xcr_write, ste);
	mapproc(base + REG_XR1, xsr_read, xsr_write, ste);
	mapproc(base + REG_XR2, pid1_read, pid1_write, ste);
	mapproc(base + REG_XR3, pid2_read, pid2_write, ste);
	mapproc(base + REG_XR4, ana_read, ana_write, ste);
	mapproc(base + REG_XR5, anlpa_read, anlpa_write, ste);
	mapproc(base + REG_XR6, ane_read, ane_write, ste);
	mapproc(base + REG_XR7, xmc_read, xmc_write, ste);
	mapproc(base + REG_XR8, xciis_read, xciis_write, ste);
	mapproc(base + REG_XR9, xie_read, xie_write, ste);
	mapproc(base + REG_XR10, ctr100_read, ctr100_write, ste);
}

static void
ste_map_io(STE10_100 * ste)
{
	uint32_t base = ste->bar0;
	MapProc *mapproc = PCI_RegisterIOH;
	/* IO-Space enabled ? */
	if (!(ste->command & 1)) {
		fprintf(stderr, "STE: iospace not enabled %d\n", __LINE__);
	}
	base = base & ~0x7f;
	ste_map(ste, mapproc, base);
}

static void
ste_map_mmio(STE10_100 * ste)
{
	uint32_t base = ste->bar1;
	MapProc *mapproc = PCI_RegisterMMIOH;
	if (!(ste->command & 2)) {
		fprintf(stderr, "STE: MM iospace not enabled %d\n", __LINE__);
	}
	base = base & ~0x7f;
	ste_map(ste, mapproc, base);
}

void
ste_unmap_io(STE10_100 * ste)
{
	uint32_t base = ste->bar0;
	uint32_t i;
	base = base & ~0x7f;
	for (i = 0; i < 256; i += 4) {
		PCI_UnRegisterIOH(base + i);
	}
}

void
ste_unmap_mmio(STE10_100 * ste)
{
	uint32_t base = ste->bar1;
	uint32_t i;
	base = base & ~0x7f;
	for (i = 0; i < 256; i += 4) {
		PCI_UnRegisterMMIOH(base + i);
	}
}

/* 
 * --------------------------------------------------------------------------------
 * The Map Interface to the Outside (PCI Controller will call this when its window
 * changes or when Byteorder Changes)
 * --------------------------------------------------------------------------------
 */
static void
STE_Map(void *owner, uint32_t flags)
{
	STE10_100 *ste = owner;
	if (ste->command & 1) {
		ste_map_io(ste);
	}
	if (ste->command & 2) {
		ste_map_mmio(ste);
	}
}

static void
STE_UnMap(void *owner)
{
	STE10_100 *ste = owner;
	ste_unmap_io(ste);
	ste_unmap_mmio(ste);
}

/*
 * --------------------------------------------------------
 * Called whenever PCI-Command register changes.
 * registers/unregisters  the IO and Memory spaces
 * --------------------------------------------------------
 */
static void
change_pci_command(STE10_100 * ste, uint16_t value, uint16_t mask)
{
	dbgprintf("Change PCI command from %04x to %04x, mask %04x\n", ste->command, value, mask);
	MODIFY_MASKED(ste->command, value, mask);
	// register/register iospace and mmio
	ste_unmap_io(ste);
	ste_unmap_mmio(ste);
	if (ste->command & 1) {
		ste_map_io(ste);
	}
	if (ste->command & 2) {
		ste_map_mmio(ste);
	}
}

/*
 * ----------------------------------------------------
 * PCI Configuration Space Write to a STE10/100
 * ----------------------------------------------------
 */
static int
STE_ConfigWrite(uint32_t value, uint32_t mask, uint8_t reg, PCI_Function * pcifunc)
{
	STE10_100 *ste = pcifunc->owner;
	//printf("Write to STE-Configspace reg %d, value %08x,mask %08x\n",reg,value,mask);
	reg = reg >> 2;
	switch (reg) {
	    case 1:
		    // PCI Command, PCI Status
		    //dbgprintf("Got write %08x to %08x, mask %08x\n",value,reg,mask);
		    change_pci_command(ste, value, mask);
		    break;

	    case 0x3:
		    //fprintf(stderr,"STE Config write reg %d, value %08x,mask %08x\n",reg,value,mask);
		    if (mask & 0xff00) {
			    //fprintf(stderr,"LATENCY WRITE\n");
			    ste->latency_timer = (value >> 8) & 0xff;
		    }
		    if (mask & 0xff0000) {
			    // header type not writable
		    }
		    if (mask & 0xff) {
			    ste->cacheline_size = (value) & 0xff;
		    }
		    break;
	    case 0x4:
		    //              dbgprintf("Write bar 0 to %08x\n",value);
		    ste_unmap_io(ste);
		    MODIFY_MASKED(ste->bar0, value, mask);
		    ste->bar0 = ste->bar0 & ~0xff;
		    if (ste->command & 1) {
			    ste_map_io(ste);
		    }
		    break;
	    case 0x5:
		    //              dbgprintf("Write bar 1 to %08x\n",value);
		    ste_unmap_mmio(ste);
		    MODIFY_MASKED(ste->bar1, value, mask);
		    ste->bar1 = ste->bar1 & ~0xff;
		    if (ste->command & 2) {
			    ste_map_mmio(ste);
		    }
		    break;
	    case 0x6:
		    //MODIFY_MASKED(ste->bar2,value,mask);
		    break;
	    case 0x7:
		    //MODIFY_MASKED(ste->bar3,value,mask);
		    break;
	    case 0x8:
		    //MODIFY_MASKED(ste->bar4,value,mask);
		    break;
	    case 0x9:
		    //MODIFY_MASKED(ste->bar5,value,mask);
		    break;
	    case 0xf:
		    if (mask & 0xff) {
			    ste->irq_line = value & 0xff;
		    }
		    if (mask & 0xff00) {
			    int pin = (value >> 8) & 3;
			    if (pin != ste->irq_pin) {
				    SigNode_Set(ste->irqNode[ste->irq_pin], SIG_OPEN);
				    if (ste->interrupt_posted) {
					    SigNode_Set(ste->irqNode[pin], SIG_LOW);
				    } else {
					    SigNode_Set(ste->irqNode[pin], SIG_PULLUP);
				    }
				    ste->irq_pin = pin;
			    }
		    }
		    break;
	    case 0x10:
		    MODIFY_MASKED(ste->driver_space, value, mask & 0xffff);
		    break;
	    default:
		    break;
	}
	return 0;
}

/*
 * ----------------------------------------------------
 * PCI Configuration Space Read from a STE10/100
 * ----------------------------------------------------
 */
static int
STE_ConfigRead(uint32_t * value, uint8_t reg, PCI_Function * pcifunc)
{
	STE10_100 *ste = pcifunc->owner;
	reg = reg >> 2;
	switch (reg) {
	    case 0:
		    *value = ste->vendor_id | (ste->device_id << 16);
		    break;

	    case 1:
		    // PCI Command, PCI Status
		    *value = (ste->status << 16) | ste->command;
		    break;

	    case 2:
		    // PCI Revision ID Class Code
		    *value = 0x020000a1;
		    break;

	    case 3:
		    *value =
			(ste->cacheline_size) | (ste->latency_timer << 8) | (ste->
									     header_type << 16);
		    break;
	    case 4:
		    // IO-Type Base Address with Window of 256 bytes
		    *value = ste->bar0 | 1;
//                      dbgprintf("read bar 0 %08x\n",*value);
		    break;

	    case 5:
		    // MMIO Type
		    *value = ste->bar1;
//                      dbgprintf("read bar 1 %08x\n",*value);
		    break;

	    case 6:
		    *value = ste->bar2;
		    break;
	    case 7:
		    *value = ste->bar3;
		    break;
	    case 8:
		    *value = ste->bar4;
		    break;
	    case 9:
		    *value = ste->bar5;
		    break;
	    case 0xa:
		    // Cardbus CIS Pointer
		    *value = 0;
		    break;

	    case 0xb:
		    *value = ste->sub_vendor_id | (ste->sub_device_id << 16);
		    break;
	    case 0xc:		// Expansion Rom
	    case 0xd:		// Reserved
	    case 0xe:		// Reserved
		    *value = 0;
		    break;
	    case 0xf:
		    *value =
			ste->irq_line | (ste->irq_pin << 8) | (ste->min_gnt << 16) | (ste->
										      max_lat <<
										      24);
		    break;

	    case 0x10:
		    *value = ste->driver_space;
		    break;

	    case 0x20:
		    *value = (PCI_DEVICE_ID_STE10_100A << 16) | PCI_VENDOR_ID_STM;
		    break;
	    default:
		    *value = 0;
		    break;

	}
//      dbgprintf("STE-Config Read reg %d val %08x\n",reg*4, *value);
	return 0;
}

#define POLY 0x04C11DB6L

static uint16_t
eeprom_crc(uint8_t * data)
{
	uint32_t crc = 0xffffffff;
	uint32_t flipped_crc = 0;
	uint8_t cb;
	uint8_t msb;
	int i, bit;
	for (i = 0; i < 126; i++) {
		cb = data[i];
		for (bit = 0; bit < 8; bit++) {
			msb = (crc >> 31) & 1;
			crc <<= 1;
			if (msb ^ (cb & 1)) {
				crc ^= POLY;
				crc |= 1;
			}
			cb >>= 1;
		}
	}
	for (i = 0; i < 32; i++) {
		flipped_crc <<= 1;
		bit = crc & 1;
		crc >>= 1;
		flipped_crc += bit;
	}
	crc = flipped_crc ^ 0xffffffff;
	return crc & 0xffff;
}

static uint8_t
network_address_checksum(uint8_t * data)
{
	int i;
	uint8_t sum = 0;
	for (i = 0; i < 6; i++) {
		sum = (sum << 1) + ((sum >> 7) & 1) + data[8 + i];
	}
	return sum;
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
read_eeprom(STE10_100 * ste)
{
	int i;
	int mac_valid = 0;
	int eeprom_valid = 0;
	uint8_t data[128];
	uint16_t crc;
	for (i = 0; i < 128; i++) {
		data[i] = m93c46_readb(ste->eprom, i);
	}
	crc = data[126] + (data[127] << 8);
	if (eeprom_crc(data) != crc) {
		fprintf(stderr, "STE10/100 EEPROM has invalid CRC\n");
	} else {
		eeprom_valid = 1;
	}
	if (!eeprom_valid || (network_address_checksum(data) != data[0xe])) {
		uint8_t *m = ste->mac;
		generate_random_hwaddr(ste->mac);
		fprintf(stderr, "STE10/100 bad MAC address in eeprom, ");
		fprintf(stderr, "Using random mac %02x:%02x:%02x:%02x:%02x:%02x\n",
			m[0], m[1], m[2], m[3], m[4], m[5]);

	} else {
		mac_valid = 1;
	}
	if (mac_valid) {
		for (i = 0; i < 6; i++) {
			ste->mac[i] = data[8 + i];
		}
	}
	if (!eeprom_valid) {
		ste->device_id = PCI_DEVICE_ID_STE10_100A;
		ste->vendor_id = PCI_VENDOR_ID_STM;
		ste->sub_vendor_id = 0;
		ste->sub_device_id = 0;
		ste->min_gnt = 1;
		ste->max_lat = 9;
	} else {
		ste->device_id = data[0x20] + (data[0x21] << 8);
		ste->vendor_id = data[0x22] + (data[0x23] << 8);
		ste->sub_vendor_id = data[0x24] + (data[0x25] << 8);
		ste->sub_device_id = data[0x26] + (data[0x27] << 8);
		ste->min_gnt = data[0x28];
		ste->max_lat = data[0x29];
		CSR18(ste) = (data[0x2e] + (data[0x2f] << 8)) << 16;
	}
	return;
}

/*
 * --------------------------------------------------
 * Registered Functions for STE10/100 PCI Card
 * --------------------------------------------------
 */

PCI_Function *
STE_New(const char *devname, PCI_Function * bridge, int dev_nr, int bus_irq)
{
	STE10_100 *ste;
	int i;
	char *eepromname = alloca(strlen(devname) + 20);
	SigNode *mw_sclk;
	SigNode *mw_sdi;
	SigNode *mw_sdo;
	SigNode *mw_csel;
	ste = sg_new(STE10_100);
	ste->xr = &ste->csr[0xb4 >> 2];
	ste->dev_nr = dev_nr;

	/* Docu of PCI-Command Reg. is wrong, Chip is not enabled by default */
	ste->command = 0x340;
	ste->status = 0x0280;
	/*
	   ste->cacheline_size=8;
	   ste->latency_timer=64;
	 */
	ste->cacheline_size = 0;
	ste->latency_timer = 0;
	ste->header_type = PCI_HEADER_TYPE_NORMAL;
	ste->bar0 = 1;
	ste->bar1 = 0;
	ste->irq_line = 0;
	ste->irq_pin = 1;	// INTA
	ste->driver_space = 0;

	sprintf(eepromname, "%s.eeprom", devname);
	ste->eprom = m93c46_New(eepromname);
	ste->edo = SigNode_New("%s.edo", devname);
	ste->edi = SigNode_New("%s.edi", devname);
	ste->eck = SigNode_New("%s.eck", devname);
	ste->eecs = SigNode_New("%s.eecs", devname);
	mw_sclk = SigNode_Find("%s.sclk", eepromname);
	mw_sdi = SigNode_Find("%s.sdi", eepromname);
	mw_sdo = SigNode_Find("%s.sdo", eepromname);
	mw_csel = SigNode_Find("%s.csel", eepromname);
	if (!mw_sclk || !mw_sdi || !mw_sdo || !mw_csel) {
		fprintf(stderr, "STE: can not connect to microwire eeprom\n");
		exit(1);
	}
	for (i = 0; i < 4; i++) {
		ste->irqNode[i] = SigNode_New("%s.int%c", devname, 'A' + i);
		if (!ste->irqNode[i]) {
			fprintf(stderr, "STE: Can not create irqline \n");
			exit(1);
		}
	}
	SigNode_Link(ste->edo, mw_sdo);
	SigNode_Link(ste->edi, mw_sdi);
	SigNode_Link(ste->eck, mw_sclk);
	SigNode_Link(ste->eecs, mw_csel);
	SigNode_Set(ste->eecs, SIG_LOW);
	SigNode_Set(ste->edi, SIG_LOW);
	SigNode_Set(ste->eck, SIG_LOW);
	ste->ether_fd = Net_CreateInterface(devname);
	ste->input_fh = AsyncManager_PollInit(ste->ether_fd);
	STE_Reset(ste);
	XCR(ste) = 0x1000;
	XSR(ste) = 0x780d;
	PID1(ste) = 0x1c04;
	PID2(ste) = 0x10;
	ANA(ste) = 0x5e1;
	/* should be 0 after reset, change later */
	ANLPA(ste) = 0;
	ANLPA(ste) = 0x45e1;

	ANE(ste) = 0x1;
	XMC(ste) = 0;
	XCIIS(ste) = 0x200;
	XCIIS(ste) = 0x380;

	XIE(ste) = 0x0;

	CTR100(ste) = 0xdc0;
//      CTR100(ste) = 0x1dc8;

	SR(ste) = 0xfc005412;	/* Reserved bits are one in real chip */
	XR10(ste) = 0x1dd8;
	NAR(ste) = 0xff972117;

	CycleTimer_Init(&ste->gptmr, gptmr_timeout, ste);
	/* read eeprom here (EEPROM is read at power up, not at reset ! */
	read_eeprom(ste);

	ste->bus_irq = bus_irq;
	ste->pcifunc.configRead = STE_ConfigRead;
	ste->pcifunc.configWrite = STE_ConfigWrite;
	ste->pcifunc.Map = STE_Map;
	ste->pcifunc.UnMap = STE_UnMap;
	ste->pcifunc.function = 0;
	ste->pcifunc.dev = dev_nr;
	ste->pcifunc.owner = ste;
	ste->pcifunc.hw_flags = MEM_FLAG_WRITABLE | MEM_FLAG_READABLE;
	ste->bridge = bridge;
	PCI_FunctionRegister(bridge, &ste->pcifunc);
	fprintf(stderr, "STE10/100 created in PCI Slot %d Bus-IRQ %d\n", dev_nr, bus_irq);
	return &ste->pcifunc;
}
