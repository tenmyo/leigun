/*
 *************************************************************************************************
 *
 * Emulation of the Intel 82559 PCI Ethernet Controller
 *
 * State:
 *	Nothing works
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

#if 0
#define dbgprintf(x...) { fprintf(stderr,x); }
#else
#define dbgprintf(x...)
#endif

#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include "pci.h"
#include "fio.h"
#include "linux-tap.h"
#include "cycletimer.h"
#include "signode.h"
#include "i82559.h"
#include "sgstring.h"

#define MODIFY_MASKED(v,mod,mask) { (v) = ((v) & ~(mask)) | ((mod)&(mask)); }

#define REG_SCB_STATUS_WORD	(0)
#define REG_SCB_CMD_WORD	(2)
#define REG_SCB_GENERAL_POINTER (4)
#define REG_PORT		(8)
#define REG_EEPROM_CTRL		(0xe)
#define REG_MDI_CONTROL		(0x10)
#define 	MDI_CTRL_IE	(1<<29)
#define		MDI_CTRL_RDY	(1<<28)
#define REG_RX_DMA_BYTE_COUNT	(0x14)
#define REG_FLOW_CONTROL	(0x18)
#define REG_PMDR		(0x1b)
#define REG_GENERAL_CTRL_STS	(0x1c)
#define REG_GENERAL_CONTROL	(0x1c)
#define REG_GENERAL_STATUS	(0x1d)
#define REG_FUNCTION_EVENT	(0x30)
#define REG_FUNCTION_EVENT_MASK (0x34)
#define REG_FUNCTION_PRESENT_STATE (0x38)
#define REG_FORCE_EVENT		(0x3c)

typedef struct I82559 {
	PCI_Function pcifunc;
	PCI_Function *bridge;
	uint32_t bus_irq;

	int ether_fd;
	uint16_t pci_device_id;
	uint16_t pci_vendor_id;
	uint16_t pci_command;
	uint16_t pci_status;
	uint8_t pci_latency_timer;
	uint8_t pci_header_type;
	uint8_t pci_cacheline_size;
	uint32_t pci_bar0;	/* Doku says its prefetchable ?????? */
	uint32_t pci_bar1;
	uint32_t pci_bar2;
	uint32_t pci_bar3;
	uint32_t pci_bar4;
	uint32_t pci_bar5;
	uint32_t pci_sub_vendor_id;
	uint32_t pci_sub_device_id;
	uint8_t pci_irq_line;
	uint8_t pci_irq_pin;
	uint8_t pci_min_gnt;
	uint8_t pci_max_lat;
	uint32_t pci_rom;

	uint32_t scb_cmd_status_word;
	uint32_t ru_base;
	uint32_t cu_base;
	uint32_t dump_counter_address;
	uint32_t scb_general_pointer;
	uint32_t port;
	uint16_t eeprom_ctrl;
	uint32_t mdi_control;
	uint32_t rx_dma_byte_count;
	uint16_t flow_control;
	uint8_t reg_pmdr;
	uint8_t general_control;
	uint8_t general_status;
	uint32_t function_event;
	uint32_t function_event_mask;
	uint32_t function_present_state;
	uint32_t force_event;

	/* statistics */
	uint32_t tx_goodframes;
	uint32_t tx_maxcol;
	uint32_t tx_latecol;
	uint32_t tx_underrun;
	uint32_t tx_crs;
	uint32_t tx_deferred;
	uint32_t tx_single_coll;
	uint32_t tx_multicoll;
	uint32_t tx_coll;
	uint32_t rx_goodframes;
	uint32_t rx_crcerr;
	uint32_t rx_alignerr;
	uint32_t rx_resourceerrs;
	uint32_t rx_overrun;
	uint32_t rx_cdt;
	uint32_t rx_short;
	uint32_t rx_fctp;	/* Flow control transmission pause */
	uint32_t rx_fcrp;
	uint32_t rx_fcrunsupp;
	uint16_t tx_tco;
	uint16_t rx_tco;

	int interrupt_posted;
	SigNode *irqNode;
	//uint8_t cfg_byte_map[22];
} I82559;

/* Command opcodes in the Command Block List */
#define CBC_NOP 		(0)
#define CBC_ADDR_SETUP		(1)
#define CBC_CONFIGURE		(2)
#define CBC_MCAS		(3)	/* Multicast address setup */
#define CBC_TRANSMIT		(4)
#define CBC_LOAD_MICROCODE	(5)
#define CBC_DUMP		(6)
#define CBC_DIAGNOSE		(7)

/* opcodes for Command Unit commands */
#define CUC_NOP			(0)
#define	CUC_CUSTART		(1)
#define CUC_CURESUME		(2)
#define CUC_LDCA		(4)
#define	CUC_DUMPSTAT		(5)
#define CUC_LOADBASE		(6)
#define CUC_DUMPRSTSTAT		(7)
#define CUC_STATIC_RESUME	(0xa)

/* opcodes for Receive Unit commands */
#define RUC_NOP		(0)
#define RUC_RUSTART	(1)
#define RUC_RURESUME	(2)
#define RUC_RCVDMAREDIR	(3)
#define RUC_RUABORT	(4)
#define RUC_LOADHEADER	(5)
#define RUC_LOADBASE	(6)

/* Port Command codes */
#define PCMD_SWRST	(0)
#define PCMD_SLFTST	(1)
#define PCMD_SELRST	(2)
#define PCMD_DUMP	(3)
#define PCMD_WAKEUP	(7)

/* Status codes */
#define CUS_IDLE	(0)
#define CUS_SUSPENDED	(1)
#define CUS_LPQ_ACTIVE  (2)
#define CUS_HPQ_ACTIVE  (3)

#define RUS_IDLE		(0)
#define RUS_SUSPENDED		(1)
#define RUS_NO_RESOURCES	(2)
#define RUS_READY		(4)

/* Common Command Block Header */
typedef struct CBH {
	uint32_t cmd_status;
	uint32_t link_offset;
} CBH;

/* Command block header access */

static uint32_t
scb_status_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "i82559: reg %08x not implemented\n", address);
	return 0;
}

#define CMDWORD_SI (1<<25)
#define CMDWORD_M  (1<<24)
#define CMDWORD_RUCMD_MASK 	(7<<16)
#define CMDWORD_RUCMD_SHIFT 	(7)
#define CMDWORD_CUCMD_MASK 	(0xf<<20)
#define CMDWORD_CUCMD_SHIFT 	(20)

#define IRQMSK_CX	(1<<31)
#define IRQMSK_FR	(1<<30)
#define IRQMSK_CNA	(1<<29)
#define IRQMSK_RNR	(1<<28)
#define IRQMSK_ER	(1<<27)
#define IRQMSK_FCP	(1<<26)

#define IRQSTAT_CX	(1<<15)
#define IRQSTAT_FR	(1<<14)
#define IRQSTAT_CNA	(1<<13)
#define IRQSTAT_RNR	(1<<12)

#define IRQSTAT_MDI	(1<<11)
#define IRQSTAT_SWI	(1<<10)
#define IRQSTAT_FCP	(1<<8)

static void
update_interrupts(I82559 * ixx)
{
	int int_pending = 0;
	uint32_t sw = ixx->scb_cmd_status_word;
	uint32_t status;
	uint32_t mask;
	uint32_t m;
	m = sw & CMDWORD_M;
	if (!m) {
		status = sw & (IRQSTAT_CX | IRQSTAT_FR | IRQSTAT_CNA | IRQSTAT_RNR) >> 12;
		mask = sw & (IRQMSK_CX | IRQMSK_FR | IRQMSK_CNA | IRQMSK_RNR) >> 28;
		if (status & ~mask) {
			int_pending = 1;
		} else if ((sw & IRQSTAT_FCP) && !(sw & IRQMSK_FCP)) {
			int_pending = 1;
		} else if ((sw & IRQSTAT_MDI) && (ixx->mdi_control & MDI_CTRL_IE)) {
			int_pending = 1;
		} else if (sw & IRQSTAT_SWI) {
			int_pending = 1;
		}
	}
	if (int_pending) {
		if (!ixx->interrupt_posted) {
			SigNode_Set(ixx->irqNode, SIG_LOW);
			ixx->interrupt_posted = 1;
		}
	} else {
		if (ixx->interrupt_posted) {
			SigNode_Set(ixx->irqNode, SIG_HIGH);
			ixx->interrupt_posted = 0;
		}
	}
}

static void
scb_irqmask_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	I82559 *ixx = clientData;
	int si;
	si = value & CMDWORD_SI;
	ixx->scb_cmd_status_word = ((ixx->scb_cmd_status_word & 0x00ffffff) | (value & 0xfd000000));
	if (si) {
		ixx->scb_cmd_status_word |= IRQSTAT_SWI;
	}
	update_interrupts(ixx);
}

static void
scb_cmd_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	I82559 *ixx = clientData;
	uint32_t rucmd, cucmd;
	rucmd = (value & CMDWORD_RUCMD_MASK) >> CMDWORD_RUCMD_SHIFT;
	cucmd = (value & CMDWORD_CUCMD_MASK) >> CMDWORD_CUCMD_SHIFT;
	switch (rucmd) {
	    case RUC_NOP:
		    break;
	    case RUC_RUSTART:
		    // fetch rfa from general pointer
		    // enable receiver
		    fprintf(stderr, "rucmd %02x not implemented\n", rucmd);
		    break;
	    case RUC_RURESUME:
		    fprintf(stderr, "rucmd %02x not implemented\n", rucmd);
		    break;
	    case RUC_RCVDMAREDIR:
		    fprintf(stderr, "rucmd %02x not implemented\n", rucmd);
		    break;
	    case RUC_RUABORT:
		    fprintf(stderr, "rucmd %02x not implemented\n", rucmd);
		    break;
	    case RUC_LOADHEADER:
		    fprintf(stderr, "rucmd %02x not implemented\n", rucmd);
		    break;
	    case RUC_LOADBASE:
		    ixx->ru_base = ixx->scb_general_pointer;
		    break;

	    default:
		    fprintf(stderr, "rucmd %02x not implemented\n", rucmd);
	}
	switch (cucmd) {
	    case CUC_NOP:
		    break;

	    case CUC_CUSTART:
		    // fetch first cb address from general pointer
		    // start cb
		    fprintf(stderr, "cucmd %02x not implemented\n", cucmd);
		    break;

	    case CUC_CURESUME:
		    fprintf(stderr, "cucmd %02x not implemented\n", cucmd);
		    break;

	    case CUC_LDCA:
		    ixx->dump_counter_address = ixx->scb_general_pointer;
		    break;

	    case CUC_DUMPSTAT:
		    fprintf(stderr, "cucmd %02x not implemented\n", cucmd);
		    break;

	    case CUC_LOADBASE:
		    ixx->cu_base = ixx->scb_general_pointer;
		    break;

	    case CUC_DUMPRSTSTAT:
		    fprintf(stderr, "cucmd %02x not implemented\n", cucmd);
		    break;
	    case CUC_STATIC_RESUME:
		    fprintf(stderr, "cucmd %02x not implemented\n", cucmd);
		    break;
	    default:
		    fprintf(stderr, "cucmd %02x not implemented\n", cucmd);

	}
	fprintf(stderr, "i82559: write reg %08x not implemented\n", address);
	return;
}

static void
scb_ack_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{

}

static void
scb_command_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	I82559 *ixx = clientData;
	uint32_t cmdword;
	uint32_t mask;
	mask = (0xffffffffU >> (4 - rqlen)) << (address & 3);
	cmdword = (value << ((address & 3) << 3));
	if (mask & 0x0000ff00) {
		scb_ack_write(ixx, cmdword, address, rqlen);
	}
	if (mask & 0xff000000) {
		scb_irqmask_write(ixx, cmdword, address, rqlen);
	}
	if (mask & 0x00ff0000) {
		scb_cmd_write(ixx, cmdword, address, rqlen);
	}
	return;
}

static uint32_t
scb_general_pointer_read(void *clientData, uint32_t address, int rqlen)
{
	I82559 *ixx = clientData;
	return ixx->scb_general_pointer;
}

static void
scb_general_pointer_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	I82559 *ixx = clientData;
	ixx->scb_general_pointer = value;
	return;
}

static uint32_t
port_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "i82559: reg %08x not implemented\n", address);
	return 0;
}

static void
port_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "i82559: write reg %08x not implemented\n", address);
	return;
}

static uint32_t
eeprom_ctrl_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "i82559: reg %08x not implemented\n", address);
	return 0;
}

static void
eeprom_ctrl_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "i82559: write reg %08x not implemented\n", address);
	return;
}

static uint32_t
mdi_control_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "i82559: reg %08x not implemented\n", address);
	return 0;
}

static void
mdi_control_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "i82559: write reg %08x not implemented\n", address);
	return;
}

static uint32_t
rx_dma_byte_count_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "i82559: reg %08x not implemented\n", address);
	return 0;
}

static void
rx_dma_byte_count_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "i82559: write reg %08x not implemented\n", address);
	return;
}

static uint32_t
flow_control_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "i82559: reg %08x not implemented\n", address);
	return 0;
}

static void
flow_control_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "i82559: write reg %08x not implemented\n", address);
	return;
}

static uint32_t
general_status_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "i82559: reg %08x not implemented\n", address);
	return 0;
}

static void
general_ctrl_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "i82559: write reg %08x not implemented\n", address);
	return;
}

static uint32_t
function_event_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "i82559: reg %08x not implemented\n", address);
	return 0;
}

static void
function_event_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "i82559: write reg %08x not implemented\n", address);
	return;
}

static uint32_t
function_event_mask_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "i82559: reg %08x not implemented\n", address);
	return 0;
}

static void
function_event_mask_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "i82559: write reg %08x not implemented\n", address);
	return;
}

static uint32_t
force_event_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "i82559: reg %08x not implemented\n", address);
	return 0;
}

static void
force_event_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "i82559: write reg %08x not implemented\n", address);
	return;
}

typedef int MapProc(uint32_t addr, PCI_IOReadProc *, PCI_IOWriteProc *, void *cd);

static void
i82559_map(I82559 * ixx, MapProc * mapproc, uint32_t base)
{
	//base=base& ~0x7f;
//      printf("Map io base %08x\n",base);
//        mapproc(base+REG_CSR0,par_read,par_write,ste);
	mapproc(base + REG_SCB_STATUS_WORD, scb_status_read, scb_command_write, ixx);
	mapproc(base + REG_SCB_GENERAL_POINTER, scb_general_pointer_read, scb_general_pointer_write,
		ixx);
	mapproc(base + REG_PORT, port_read, port_write, ixx);
	mapproc(base + REG_EEPROM_CTRL, eeprom_ctrl_read, eeprom_ctrl_write, ixx);
	mapproc(base + REG_MDI_CONTROL, mdi_control_read, mdi_control_write, ixx);
	mapproc(base + REG_RX_DMA_BYTE_COUNT, rx_dma_byte_count_read, rx_dma_byte_count_write, ixx);
	mapproc(base + REG_FLOW_CONTROL, flow_control_read, flow_control_write, ixx);
	mapproc(base + REG_GENERAL_CTRL_STS, general_status_read, general_ctrl_write, ixx);
	mapproc(base + REG_FUNCTION_EVENT, function_event_read, function_event_write, ixx);
	mapproc(base + REG_FUNCTION_EVENT_MASK, function_event_mask_read, function_event_mask_write,
		ixx);
	mapproc(base + REG_FORCE_EVENT, force_event_read, force_event_write, ixx);
}

static void
i82559_map_io(I82559 * ixx, uint32_t base)
{
	MapProc *mapproc = PCI_RegisterIOH;
	base = base & ~0x3f;
	i82559_map(ixx, mapproc, base);
}

static void
i82559_map_mmio(I82559 * ixx, uint32_t base)
{
	MapProc *mapproc = PCI_RegisterMMIOH;
	base = base & ~0x3f;
	i82559_map(ixx, mapproc, base);
}

void
i82559_unmap_io(I82559 * ixx, uint32_t base)
{
	uint32_t i;
	base = base & ~0x3f;
	for (i = 0; i < 64; i += 4) {
		PCI_UnRegisterIOH(base + i);
	}
}

void
i82559_unmap_mmio(I82559 * ixx, uint32_t base)
{
	uint32_t i;
	base = base & ~0x3f;
	for (i = 0; i < 64; i += 4) {
		PCI_UnRegisterMMIOH(base + i);
	}
}

/*
 * --------------------------------------------------------
 * Called whenever PCI-Command register changes.
 * registers/unregisters  the IO and Memory spaces
 * --------------------------------------------------------
 */
static void
change_pci_command(I82559 * ixx, uint16_t value, uint16_t mask)
{
	dbgprintf("Change PCI command from %04x to %04x, mask %04x\n", ixx->pci_command, value,
		  mask);
	MODIFY_MASKED(ixx->pci_command, value, mask);
	// register/register iospace and mmio
	i82559_unmap_mmio(ixx, ixx->pci_bar0);
	i82559_unmap_io(ixx, ixx->pci_bar1);
	if (ixx->pci_command & 2) {
		i82559_map_mmio(ixx, ixx->pci_bar0);
	}
	if (ixx->pci_command & 1) {
		i82559_map_io(ixx, ixx->pci_bar1);
	}
}

/*
 * --------------------------------------------------------------
 * Implementation of the 82559 PCI Configuration Space
 * --------------------------------------------------------------
 */
static int
I82559_ConfigWrite(uint32_t value, uint32_t mask, uint8_t reg, PCI_Function * pcifunc)
{
	I82559 *ixx = pcifunc->owner;
	reg = reg >> 2;
	switch (reg) {
	    case 1:
		    change_pci_command(ixx, value, mask);
		    break;

	    case 0x3:
		    if (mask & 0xff00) {
			    ixx->pci_latency_timer = (value >> 8) & 0xff;
		    }
		    if (mask & 0xff0000) {
			    // header type not writable
		    }
		    if (mask & 0xff) {
			    ixx->pci_cacheline_size = (value) & 0x18;
		    }
		    break;
	    case 0x4:
		    if (ixx->pci_command & 2) {
			    i82559_unmap_mmio(ixx, ixx->pci_bar0);
		    }
		    MODIFY_MASKED(ixx->pci_bar0, value, mask);
		    ixx->pci_bar0 = ixx->pci_bar0 & ~0xfff;
		    if (ixx->pci_command & 2) {
			    i82559_map_mmio(ixx, ixx->pci_bar0);
		    }
		    break;
	    case 0x5:
		    //              dbgprintf("Write bar 1 to %08x\n",value);
		    if (ixx->pci_command & 1) {
			    i82559_unmap_io(ixx, ixx->pci_bar1);
		    }
		    MODIFY_MASKED(ixx->pci_bar1, value, mask);
		    ixx->pci_bar1 = (ixx->pci_bar1 & ~0x3f) | 1;
		    if (ixx->pci_command & 1) {
			    i82559_map_io(ixx, ixx->pci_bar1);
		    }
		    break;

	    case 0x6:
		    MODIFY_MASKED(ixx->pci_bar2, value, mask);
		    ixx->pci_bar1 = ixx->pci_bar1 & ~(1024 * 1024 - 1);
		    break;

	    case 0x7:
		    // bar3
		    break;
	    case 0x8:
		    // bar4
		    break;
	    case 0x9:
		    // bar5
		    break;
	    case 0xc:
		    MODIFY_MASKED(ixx->pci_rom, value, mask);
		    ixx->pci_rom = ixx->pci_rom & ~((1 << 20) - 1 - 1);	/* Lowest bit is enable */
		    // update flash mapping
		    break;
	    case 0xf:
		    if (mask & 0xff) {
			    ixx->pci_irq_line = value & 0xff;
		    }
		    break;
	    default:
		    break;
	}
	return 0;
}

/*
 * ----------------------------------------------------
 * PCI Configuration Space Read from a i82559 
 * ----------------------------------------------------
 */
static int
I82559_ConfigRead(uint32_t * value, uint8_t reg, PCI_Function * pcifunc)
{
	I82559 *ixx = pcifunc->owner;
	reg = reg >> 2;
	switch (reg) {
	    case 0:
		    *value = ixx->pci_vendor_id | (ixx->pci_device_id << 16);
		    break;

	    case 1:
		    // PCI Command, PCI Status
		    *value = (ixx->pci_status << 16) | ixx->pci_command;
		    break;

	    case 2:
		    // PCI Revision ID Class Code
		    *value = 0x02000008;
		    break;

	    case 3:
		    *value = (ixx->pci_cacheline_size) |
			(ixx->pci_latency_timer << 8) | (ixx->pci_header_type << 16);
		    break;
	    case 4:
		    // IO-Type Base Address with Window of 256 bytes
		    *value = ixx->pci_bar0;
//                      dbgprintf("read bar 0 %08x\n",*value);
		    break;

	    case 5:
		    // MMIO Type
		    *value = ixx->pci_bar1 | 1;
//                      dbgprintf("read bar 1 %08x\n",*value);
		    break;
	    case 6:
		    *value = ixx->pci_bar2;
		    break;
	    case 7:
		    *value = ixx->pci_bar3;
		    break;
	    case 8:
		    *value = ixx->pci_bar4;
		    break;
	    case 9:
		    *value = ixx->pci_bar5;
		    break;
	    case 0xa:
		    // Cardbus CIS Pointer
		    *value = 0;
		    break;

	    case 0xb:
		    *value = ixx->pci_sub_vendor_id | (ixx->pci_sub_device_id << 16);
		    break;
	    case 0xc:		// Expansion Rom
		    *value = ixx->pci_rom;
		    break;
	    case 0xd:		// Reserved
	    case 0xe:		// Reserved
		    *value = 0;
		    break;

	    case 0xf:
		    *value = ixx->pci_irq_line | (ixx->pci_irq_pin << 8) |
			(ixx->pci_min_gnt << 16) | (ixx->pci_max_lat << 24);
		    break;

	    case 0x37:
		    *value = 1 | (0x7e21 << 16);	/* Power management capabilities */
		    break;

		    /* Power Management status and consumption */
	    case 0x38:
		    *value = 0x00280000;
		    break;

	    default:
		    *value = 0;
		    break;

	}
//      dbgprintf("I82559-Config Read reg %d val %08x\n",reg*4, *value);
	return 0;
}

PCI_Function *
I82559_New(const char *devname, PCI_Function * bridge, int dev_nr, int bus_irq)
{
	I82559 *ixx = sg_new(I82559);
	ixx->ether_fd = Net_CreateInterface(devname);
	fcntl(ixx->ether_fd, F_SETFL, O_NONBLOCK);

	ixx->pci_device_id = PCI_DEVICE_ID_82559;
	ixx->pci_vendor_id = PCI_VENDOR_ID_INTEL;

	/* Docu does not tell default values (it says "x") */
	ixx->pci_command = 0x340;
	ixx->pci_status = 0x0290;
	ixx->pci_cacheline_size = 0;
	ixx->pci_latency_timer = 0;
	ixx->pci_header_type = PCI_HEADER_TYPE_NORMAL;
	ixx->pci_sub_vendor_id = 0;
	ixx->pci_sub_device_id = 0;
	ixx->pci_irq_line = 0;
	ixx->pci_irq_pin = 1;	// INTA
	ixx->pci_min_gnt = 8;	/* 2 us */
	ixx->pci_max_lat = 0x18;	/* 6 us */

	ixx->bus_irq = bus_irq;
	ixx->pcifunc.configRead = I82559_ConfigRead;
	ixx->pcifunc.configWrite = I82559_ConfigWrite;
	ixx->pcifunc.function = 0;
	ixx->pcifunc.dev = dev_nr;
	ixx->pcifunc.owner = ixx;
	ixx->bridge = bridge;
	ixx->irqNode = SigNode_New("%s.irq", devname);
	if (!ixx->irqNode) {
		fprintf(stderr, "i82559: can not create IRQ-Line for %s\n", devname);
		exit(342);
	}
	PCI_FunctionRegister(bridge, &ixx->pcifunc);
	fprintf(stderr, "Intel 82559 Ethernet Controller created in PCI Slot %d Bus-IRQ %d\n",
		dev_nr, bus_irq);
	return &ixx->pcifunc;
}
