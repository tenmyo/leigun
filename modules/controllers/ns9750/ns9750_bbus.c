/*
 *************************************************************************************************
 *
 * ns9750_bbus
 * 	Emulation of the NS9750 BBus and the BBus utility
 *	 
 * Copyright 2004 Jochen Karrer. All rights reserved.
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

#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "ns9750_timer.h"
#include "ns9750_bbus.h"
#include "signode.h"
#include "bus.h"
#include "configfile.h"
#include "sgstring.h"

#define GPIO_MODE(bbus,n) (((bbus)->gpiocfg[(n)>>3] >> (((n)&7)<<2)) & 0xf)

typedef struct BBus BBus;

typedef struct GPIO_TraceInfo {
	struct BBus *bbus;
	int gpio_nr;
} GPIO_TraceInfo;

/*
 * ---------------------------------------------------------------
 * Special function trace info
 * Used as argument for GPIO special function which is invoked on
 * change off GPIO pin if not in GPIO mode
 * ---------------------------------------------------------------
 */
typedef struct SF_TraceInfo {
	struct BBus *bbus;
	int gpio_nr;
	/* Two client Data fields */
	uint32_t traceData;
	void *clientData;
} SF_TraceInfo;

/*
 * -------------------------------------------------------------
 * Function to be called when gpio pin is not in GPIO mode
 * -------------------------------------------------------------
 */

typedef struct Irq_TraceInfo {
	struct BBus *bbus;
	int irq_nr;
} Irq_TraceInfo;

typedef struct BB_IrqDescr {
	char *name;
} BB_IrqDescr;

static BB_IrqDescr ns9750_interrupts[] = {
	{"irq_dma"},
	{"irq_usb"},
	{"irq_sbrx"},
	{"irq_sbtx"},
	{"irq_sarx"},
	{"irq_satx"},
	{"irq_scrx"},
	{"irq_sctx"},
	{"irq_sdrx"},
	{"irq_sdtx"},
	{"irq_i2c"},
	{"irq_1284"},
	{NULL},
	{NULL},
	{NULL},
	{NULL},
	{NULL},
	{NULL},
	{NULL},
	{NULL},
	{NULL},
	{NULL},
	{NULL},
	{NULL},
	{"irq_ahbdma1"},
	{"irq_ahbdma2"},
	{NULL},
	{NULL},
	{NULL},
	{NULL},
	{NULL},
	{"irq_glbl"}
};

struct BBus {
	BusDevice bdev;
	uint32_t ien;
	uint32_t is;
	int nr_gpios;		/* 50 for NS9750, 73 for NS9360 */
	int interrupt_posted;

	uint32_t mstrrst;
	SigNode *resetNode[13];

	uint32_t gpiocfg[10];	/* counting in manual starts with 1. 
				 * My array start with 0, NS9750 has 7, NS9360 has 10  
				 */
	uint32_t i_gpiodir[3];	/* emulator internal only (1 is out 0 is in) */
	uint32_t gpioctrl[3];	/* NS9750 2, NS9360 3 */
	uint32_t gpiostat[3];
	SigNode *gpioNode[73];	/* 50 for NS9750 73 for NS9360 */
	SigTrace *gpioTrace[73];
	GPIO_TraceInfo gpioTraceInfo[73];

	/* Interrupt nodes */
	SigNode *irqNode[32];
	Irq_TraceInfo irqTraceInfo[32];
	SigNode *extIrqNode[4];

	int extirq_linked[4];	/* link is done on mode change of gpio PIN */
	uint32_t monitor;
	uint32_t dma_isr;
	uint32_t dma_ier;
	int dma_irq_posted;
	uint32_t usb_cfg;
	uint32_t endian;
	SigNode *endianNode[10];
	uint32_t wakeup;
};

static BBus *bbus;

static char *ns9750_reset_nodes[32] = {
	"rst_dma",
	"rst_usb",
	"rst_serb",
	"rst_sera",
	"rst_serc",
	"rst_serd",
	"rst_1284",
	"rst_i2c",
	"rst_util",
};

static char *ns9360_reset_nodes[32] = {
	"rst_dma",
	NULL,
	"rst_serb",
	"rst_sera",
	"rst_serc",
	"rst_serd",
	"rst_1284",
	"rst_i2c",
	"rst_util",
	"rst_rtc1",
	"rst_rtc2",
	"rst_usbhst",
	"rst_usbdvc"
};

static char *ns9750_bbu_endians[10] = {
	"bbdma",
	"usb",
	"serB",
	"serA",
	"serC",
	"serD",
	"ieee1284",
	"i2c"
};

static char *ns9360_bbu_endians[10] = {
	"bbdma",
	NULL,
	"serB",
	"serA",
	"serC",
	"serD",
	NULL,
	NULL,
	NULL,
	"usbhst"
};

static void
bb_update_interrupts(BBus * bb)
{
	if ((bb->is & bb->ien) && (bb->ien & (1 << BB_IRQ_GLBL))) {
		if (!bb->interrupt_posted) {
			Sysco_PostIrq(IRQ_BBUS_AGGREGATE);
			bb->interrupt_posted = 1;
		}
	} else {
		if (bb->interrupt_posted) {
			Sysco_UnPostIrq(IRQ_BBUS_AGGREGATE);
			bb->interrupt_posted = 0;
		}
	}
}

void
BBus_PostIRQ(int subint)
{
	BBus *bb = bbus;
	bb->is |= (1 << subint);
	bb_update_interrupts(bb);
}

void
BBus_UnPostIRQ(int subint)
{
	BBus *bb = bbus;
	bb->is &= ~(1 << subint);
	bb_update_interrupts(bb);
}

void
bb_update_dma_irq()
{
	BBus *bb = bbus;
	if (bb->dma_isr & bb->dma_ier) {
		if (!bb->dma_irq_posted) {
			fprintf(stderr, "BBUS post BB_IRQ_DMA\n");
			BBus_PostIRQ(BB_IRQ_DMA);
			bb->dma_irq_posted = 1;
		}
	} else {
		if (bb->dma_irq_posted) {
			fprintf(stderr, "BBUS unpost BB_IRQ_DMA\n");
			BBus_UnPostIRQ(BB_IRQ_DMA);
			bb->dma_irq_posted = 0;
		}
	}

}

void
BBus_PostDmaIRQ(int subint)
{
	BBus *bb = bbus;
	bb->dma_isr |= (1 << subint);
	if (bb->dma_isr & bb->dma_ier) {
		if (!bb->dma_irq_posted) {
			fprintf(stderr, "BBUS post BB_IRQ_DMA\n");
			BBus_PostIRQ(BB_IRQ_DMA);
			bb->dma_irq_posted = 1;
		}
	}
}

void
BBus_UnPostDmaIRQ(int subint)
{
	BBus *bb = bbus;
	bb->dma_isr &= ~(1 << subint);
	if ((bb->dma_isr & bb->dma_ier) == 0) {
		if (bb->dma_irq_posted) {
			fprintf(stderr, "BBUS unpost BB_IRQ_DMA\n");
			BBus_UnPostIRQ(BB_IRQ_DMA);
			bb->dma_irq_posted = 0;
		}
	}
}

static uint32_t
bb_is_read(void *clientData, uint32_t address, int rqlen)
{
	BBus *bb = clientData;
	return bb->is;
}

static void
bb_is_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	BBus *bb = clientData;
	bb->is = value;
	//fprintf(stderr,"BBus Interrupt status write\n");
	return;
}

static uint32_t
bb_ien_read(void *clientData, uint32_t address, int rqlen)
{
	BBus *bb = clientData;
	return bb->ien;
}

static void
bb_ien_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	BBus *bb = clientData;
	bb->ien = value;
	//fprintf(stderr,"BBus IEN write %08x,IS %08x \n",value,bb->is);
	bb_update_interrupts(bb);
	return;
}

/*
 * ------------------------------------------------
 * Reset Signal update:
 * Reset Nodes are active high
 * ------------------------------------------------
 */
void
bbu_mstrrst_update_signals(BBus * bb)
{
	int i;
	uint32_t value = bb->mstrrst;
	for (i = 0; i < 13; i++) {
		SigNode *node = bb->resetNode[i];
		if (!node) {
			continue;
		}
		if (value & (1 << i)) {
			//fprintf(stderr,"update node %s %d HIGH\n",SigName(node),i);
			SigNode_Set(node, SIG_HIGH);
		} else {
			//fprintf(stderr,"update node %s %d LOW\n",SigName(node),i);
			SigNode_Set(node, SIG_LOW);
		}
	}
}

static uint32_t
bbu_mstrrst_read(void *clientData, uint32_t address, int rqlen)
{
	BBus *bb = clientData;
	return bb->mstrrst;
}

static void
bbu_mstrrst_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	BBus *bb = clientData;
	bb->mstrrst = value;
//      fprintf(stderr,"Master Reset write at %08x, %08x\n",address,value);
	bbu_mstrrst_update_signals(bb);
	return;
}

/*
 * ---------------------------------------------------------
 * bbu_update_gpio
 * 	This function has to be called whenever  gpioctrl or 
 *	i_gpiodir changes. It updates the logical Signals 
 *
 * n is 0...2 for gpio 1..3
 * ----------------------------------------------------------
 */
static void
bbu_update_gpio(BBus * bb, unsigned int n)
{
	unsigned int i;
	int sigval;
	int first_node = n * 32;
	for (i = 0; i < 32; i++) {
		uint32_t mask = 1 << i;
		int gpio = i + first_node;
		if (gpio >= bb->nr_gpios) {
			break;
		}
		if (bb->i_gpiodir[n] & mask) {
			if (bb->gpioctrl[n] & mask) {
				sigval = SIG_HIGH;
			} else {
				sigval = SIG_LOW;
			}
		} else {
			sigval = SIG_OPEN;
		}
		sigval = SigNode_Set(bb->gpioNode[gpio], sigval);
	}
}

static void
irq_trace_proc(struct SigNode *node, int value, void *clientData)
{
	Irq_TraceInfo *ti = clientData;
	//BBus * bb = ti->bbus;
	int irq = ti->irq_nr;
	if (value == SIG_LOW) {
		BBus_PostIRQ(irq);
	} else if (value == SIG_HIGH) {
		BBus_UnPostIRQ(irq);
	}
}

/*
 * ---------------------------------------------------------------------
 * gpio_trace_proc 
 * 	will be called whenever the level of a GPIO pin
 * 	changes. The Change will be reported in the gpiostat registers 
 * ---------------------------------------------------------------------
 */
static void
gpio_trace_proc(struct SigNode *node, int value, void *clientData)
{
	GPIO_TraceInfo *ti = clientData;
	BBus *bb = ti->bbus;
	int gpio = ti->gpio_nr;
	uint32_t mask;
	int index = (gpio >> 5) & 3;
	mask = (1 << (gpio & 31));
	if (value == SIG_HIGH) {
		bb->gpiostat[index] |= mask;
	} else if (value == SIG_LOW) {
		bb->gpiostat[index] &= ~mask;
	}
}

/*
 * ---------------------------------------------------------
 * BBus Utility GPIO Configuration 
 * Manual starts counting of gpiocfg with 1 at 90600010
 * array index starts with 0 in the emulator
 * ---------------------------------------------------------
 */

static inline unsigned int
gpiocfg_index_from_address(uint32_t address)
{
	unsigned int index;
	if (address <= BB_GPIOCFG_7) {
		index = (address - BB_GPIOCFG_1) >> 2;
	} else if (address >= BB_GPIOCFG_8) {
		index = ((address - BB_GPIOCFG_8) >> 2) + 8;
	} else {
		fprintf(stderr, "Emulator Bug: Illegal gpio-cfg address %08x\n", address);
		index = 0;
	}
	return index;
}

static uint32_t
bbu_gpiocfg_read(void *clientData, uint32_t address, int rqlen)
{
	BBus *bb = clientData;
	unsigned int index = gpiocfg_index_from_address(address);
	if (index > ((bb->nr_gpios - 1) / 8)) {
		fprintf(stderr, "Illegal gpiocfg index\n");
		return -1;
	}
	return bb->gpiocfg[index];
}

#include "arm/arm9cpu.h"
static inline void
check_interrupts()
{
#if 1
	if (!(REG_CPSR & FLAG_I)) {
		static int count = 0;
		if (count < 10) {
			count++;
			fprintf(stderr,
				"Emulator: Warning, interrupts not disabled during gpio access\n");
		}
	}
#endif
}

static void
bbu_gpiocfg_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	BBus *bb = clientData;
	int i;
	unsigned int cfg_index = gpiocfg_index_from_address(address);
	unsigned int first_gpio = cfg_index << 3;
	unsigned int dir_index = first_gpio >> 5;
	if (cfg_index > ((bb->nr_gpios - 1) / 8)) {
		fprintf(stderr, "Illegal gpiocfg cfg_index\n");
		return;
	}
	check_interrupts();
	if (value == bb->gpiocfg[cfg_index]) {
		return;
	}
	bb->gpiocfg[cfg_index] = value;
	for (i = 0; i < 8; i++) {
		unsigned int gpio = first_gpio + i;
		int mode = GPIO_MODE(bb, gpio);
		if (mode == 0xb) {
			bb->i_gpiodir[dir_index] |= 1 << (gpio & 31);
		} else if (mode == 0x3) {
			bb->i_gpiodir[dir_index] &= ~(1 << (gpio & 31));
		} else {
			bb->i_gpiodir[dir_index] &= ~(1 << (gpio & 31));

#if 1
			if ((gpio == 1) && ((mode & 3) == 2)) {
			} else if ((gpio == 13) && ((mode & 3) == 1)) {
			} else if ((gpio == 7) && ((mode & 3) == 2)) {
			} else if ((gpio == 28) && ((mode & 3) == 0)) {
			} else if ((gpio == 11) && ((mode & 3) == 1)) {
			} else if ((gpio == 32) && ((mode & 3) == 0)) {
			} else if ((gpio == 18) && ((mode & 3) == 2)) {
			} else if ((gpio == 40) && ((mode & 3) == 1)) {
			} else {
			}
#endif
		}
	}
	//fprintf(stderr,"Write GPIO Config %d value %08x\n",cfg_index+1,value);
	bbu_update_gpio(bb, dir_index);
	return;
}

static uint32_t
bbu_gpioctrl1_read(void *clientData, uint32_t address, int rqlen)
{
	BBus *bb = clientData;
	return bb->gpioctrl[0];
}

static void
bbu_gpioctrl1_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	BBus *bb = clientData;
	check_interrupts();
	bb->gpioctrl[0] = value;
	bbu_update_gpio(bb, 0);
	return;
}

static uint32_t
bbu_gpioctrl2_read(void *clientData, uint32_t address, int rqlen)
{
	BBus *bb = clientData;
	return bb->gpioctrl[1];
}

static void
bbu_gpioctrl2_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	BBus *bb = clientData;
	check_interrupts();
	bb->gpioctrl[1] = value;
	bbu_update_gpio(bb, 1);
	return;
}

static uint32_t
bbu_gpioctrl3_read(void *clientData, uint32_t address, int rqlen)
{
	BBus *bb = clientData;
	return bb->gpioctrl[2];
}

static void
bbu_gpioctrl3_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	BBus *bb = clientData;
	check_interrupts();
	bb->gpioctrl[2] = value;
	bbu_update_gpio(bb, 2);
	return;
}

static uint32_t
bbu_gpiostat1_read(void *clientData, uint32_t address, int rqlen)
{
	BBus *bb = clientData;
	bb->gpiostat[0] = bb->gpiostat[0] & ~bb->i_gpiodir[0];
	bb->gpiostat[0] = bb->gpiostat[0] | (bb->gpioctrl[0] & bb->i_gpiodir[0]);
	return bb->gpiostat[0];
}

static void
bbu_gpiostat1_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "GPIO-stat1 is now writable\n");
	return;
}

static uint32_t
bbu_gpiostat2_read(void *clientData, uint32_t address, int rqlen)
{
	BBus *bb = clientData;
	bb->gpiostat[1] = bb->gpiostat[1] & ~bb->i_gpiodir[1];
	bb->gpiostat[1] = bb->gpiostat[1] | (bb->gpioctrl[1] & bb->i_gpiodir[1]);
	return bb->gpiostat[1];
}

static void
bbu_gpiostat2_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "GPIO-stat2 is not writable\n");
	return;
}

static uint32_t
bbu_gpiostat3_read(void *clientData, uint32_t address, int rqlen)
{
	BBus *bb = clientData;
	bb->gpiostat[2] = bb->gpiostat[2] & ~bb->i_gpiodir[2];
	bb->gpiostat[2] = bb->gpiostat[2] | (bb->gpioctrl[2] & bb->i_gpiodir[2]);
	return bb->gpiostat[2];
}

static void
bbu_gpiostat3_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "GPIO-stat3 is not writable\n");
	return;
}

static uint32_t
bbu_monitor_read(void *clientData, uint32_t address, int rqlen)
{
	BBus *bb = clientData;
	fprintf(stderr, "BBU Monitor read not implemented\n");
	return bb->monitor;
}

static void
bbu_monitor_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	BBus *bb = clientData;
	if (value != 0) {
		fprintf(stderr, "BBU Monitor is not written 0: 0x%08x\n", value);
	}
	bb->monitor = value;
	return;
}

static uint32_t
bbu_isr_read(void *clientData, uint32_t address, int rqlen)
{
	BBus *bb = clientData;
	return bb->dma_isr;
	return 0;
}

static void
bbu_isr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "BBU DMA isr can not be written \n");
	return;
}

static uint32_t
bbu_ier_read(void *clientData, uint32_t address, int rqlen)
{
	BBus *bb = clientData;
	return bb->dma_ier;
}

static void
bbu_ier_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	BBus *bb = clientData;
	bb->dma_ier = value & 0xffff;
	bb_update_dma_irq();
	return;
}

static uint32_t
bbu_usb_cfg_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "BBU usb cfg read not implemented\n");
	return 0;
}

static void
bbu_usb_cfg_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "BBU usb cfg write not implemented\n");
	return;
}

static void
update_endian_nodes(BBus * bbu)
{
	int i;
	for (i = 0; i < 10; i++) {
		if (!bbu->endianNode[i]) {
			continue;
		}
		if (bbu->endian & (1 << i)) {
			SigNode_Set(bbu->endianNode[i], SIG_HIGH);
		} else {
			SigNode_Set(bbu->endianNode[i], SIG_LOW);
		}
	}
}

static uint32_t
bbu_endian_read(void *clientData, uint32_t address, int rqlen)
{
	BBus *bbu = clientData;
	fprintf(stderr, "BBU endian read\n");
	return bbu->endian;
}

static void
bbu_endian_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	BBus *bbu = clientData;
	fprintf(stderr, "BBU endian 0x%08x partially implemented\n", value);
	bbu->endian = value;
	update_endian_nodes(bbu);
	return;
}

static uint32_t
bbu_wakeup_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "BBU wakeup read not implemented\n");
	return 0;
}

static void
bbu_wakeup_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "BBU wakeup write not implemented\n");
	return;
}

/*
 * ------------------------------------------------------------------
 *  This currently ignores  Baseaddress
 * ------------------------------------------------------------------
 */
static void
NS9750BB_Map(void *owner, uint32_t base, uint32_t mapsize, uint32_t flags)
{
	BBus *bb = owner;
	int i;
	IOH_New32(BB_IS, bb_is_read, bb_is_write, bb);
	IOH_New32(BB_IEN, bb_ien_read, bb_ien_write, bb);
	IOH_New32(BB_MSTRRST, bbu_mstrrst_read, bbu_mstrrst_write, bb);
	for (i = 0; i < 7; i++) {
		IOH_New32(BB_GPIOCFG_1 + (i << 2), bbu_gpiocfg_read, bbu_gpiocfg_write, bb);
	}
	if (bb->nr_gpios > 56) {
		IOH_New32(BB_GPIOCFG_8, bbu_gpiocfg_read, bbu_gpiocfg_write, bb);
	}
	if (bb->nr_gpios > 64) {
		IOH_New32(BB_GPIOCFG_9, bbu_gpiocfg_read, bbu_gpiocfg_write, bb);
	}
	if (bb->nr_gpios > 72) {
		IOH_New32(BB_GPIOCFG_10, bbu_gpiocfg_read, bbu_gpiocfg_write, bb);
	}
	IOH_New32(BB_GPIOCTRL_1, bbu_gpioctrl1_read, bbu_gpioctrl1_write, bb);
	IOH_New32(BB_GPIOCTRL_2, bbu_gpioctrl2_read, bbu_gpioctrl2_write, bb);
	IOH_New32(BB_GPIOSTAT_1, bbu_gpiostat1_read, bbu_gpiostat1_write, bb);
	IOH_New32(BB_GPIOSTAT_2, bbu_gpiostat2_read, bbu_gpiostat2_write, bb);
	if (bb->nr_gpios > 64) {
		IOH_New32(BB_GPIOCTRL_3, bbu_gpioctrl3_read, bbu_gpioctrl3_write, bb);
		IOH_New32(BB_GPIOSTAT_3, bbu_gpiostat3_read, bbu_gpiostat3_write, bb);
	}

	IOH_New32(BB_MONITOR, bbu_monitor_read, bbu_monitor_write, bb);
	IOH_New32(BB_DMA_ISR, bbu_isr_read, bbu_isr_write, bb);
	IOH_New32(BB_DMA_IER, bbu_ier_read, bbu_ier_write, bb);
	IOH_New32(BB_USB_CFG, bbu_usb_cfg_read, bbu_usb_cfg_write, bb);
	IOH_New32(BB_ENDIAN, bbu_endian_read, bbu_endian_write, bb);
	IOH_New32(BB_WAKEUP, bbu_wakeup_read, bbu_wakeup_write, bb);

}

/*
 * -----------------------------------------------------------
 * UnMap the BB
 * -----------------------------------------------------------
 */
static void
NS9750BB_UnMap(void *owner, uint32_t base, uint32_t mapsize)
{
	int i;
	IOH_Delete32(BB_IS);
	IOH_Delete32(BB_IEN);
	IOH_Delete32(BB_MSTRRST);
	for (i = 0; i < 7; i++) {
		IOH_Delete32(BB_GPIOCFG_1 + (i << 2));
	}
	IOH_Delete32(BB_GPIOCFG_8);
	IOH_Delete32(BB_GPIOCFG_9);
	IOH_Delete32(BB_GPIOCFG_10);

	IOH_Delete32(BB_GPIOCTRL_1);
	IOH_Delete32(BB_GPIOCTRL_2);
	IOH_Delete32(BB_GPIOCTRL_3);

	IOH_Delete32(BB_GPIOSTAT_1);
	IOH_Delete32(BB_GPIOSTAT_2);
	IOH_Delete32(BB_GPIOSTAT_3);

	IOH_Delete32(BB_MONITOR);
	IOH_Delete32(BB_DMA_ISR);
	IOH_Delete32(BB_DMA_IER);
	IOH_Delete32(BB_USB_CFG);
	IOH_Delete32(BB_ENDIAN);
	IOH_Delete32(BB_WAKEUP);
}

/*
 * ---------------------------------
 * The BBus destructor 
 * ---------------------------------
 */
void
NS9xxx_BBusDel(BusDevice * dev)
{
	int i;
	BBus *bb = dev->owner;
	for (i = 0; i < bb->nr_gpios; i++) {
		SigNode_Delete(bb->gpioNode[i]);
		bb->gpioNode[i] = NULL;
	}
}

/*
 * ---------------------------------
 * Create the BBus
 * mode is "NS9750" or "NS9360"
 * ---------------------------------
 */
BusDevice *
NS9xxx_BBusNew(char *mode, char *devname)
{
	BBus *bb = sg_new(BBus);
	char **rst_nodes;
	char **endian_nodes;
	char str[80];
	int i;
	bbus = bb;
	if (!strcmp(mode, "NS9750")) {
		rst_nodes = ns9750_reset_nodes;
		endian_nodes = ns9750_bbu_endians;
		bb->nr_gpios = 50;
		bb->mstrrst = 0x1fff;
	} else if (!strcmp(mode, "NS9360")) {
		rst_nodes = ns9360_reset_nodes;
		endian_nodes = ns9360_bbu_endians;
		bb->nr_gpios = 73;
		bb->mstrrst = 0x1ffd;
	} else {
		fprintf(stderr, "NS9xxx_BBUS: unknown mode %s\n", mode);
		exit(235);
	}
	for (i = 0; i < 13; i++) {
		if (!rst_nodes[i]) {
			continue;
		}
		//sprintf(str,"bbutil.%s",rst_nodes[i]);
		bb->resetNode[i] = SigNode_New("bbutil.%s", rst_nodes[i]);
		if (!bb->resetNode[i]) {
			fprintf(stderr, "can not create node %s\n", str);
		}
	}
	for (i = 0; i < 10; i++) {
		if (!endian_nodes[i]) {
			continue;
		}
		//sprintf(str,"bbutil.endian_%s",endian_nodes[i]);
		bb->endianNode[i] = SigNode_New("bbutil.endian_%s", endian_nodes[i]);
		if (!bb->endianNode[i]) {
			fprintf(stderr, "can not create node %s\n", str);
		}
	}
	bbu_mstrrst_update_signals(bb);
	for (i = 0; i < 10; i++) {
		bb->gpiocfg[i] = 0x33333333;
	}

	bb->gpioctrl[0] = 0;
	bb->gpioctrl[1] = 0;
	bb->gpiostat[0] = 0;
	// bb->gpiostat[1]= // calculate from config
	bb->monitor = 0x80000fff;	// write 0
	bb->dma_isr = 0;
	bb->dma_ier = 0;
	bb->usb_cfg = 7;
#if TARGET_BIG_ENDIAN
	bb->endian = 0x1001;	/* | usbhost for ns9360 missing here */
#else
	bb->endian = 0;
#endif
	bb->wakeup = 0;
	/*
	 * ----------------------------------------------------------------------
	 * Create GPIO nodes, Set Traces and eventually pull them up or down
	 * ----------------------------------------------------------------------
	 */
	for (i = 0; i < bb->nr_gpios; i++) {
		int result;
		int gpioval;
		SigNode *gpionode;
		char confname[50];
		GPIO_TraceInfo *ti = &bb->gpioTraceInfo[i];
		gpionode = bb->gpioNode[i] = SigNode_New("bbutil.gpio.%d", i);
		ti->bbus = bb;
		ti->gpio_nr = i;
		if (!gpionode) {
			exit(5324);
		}
		SigNode_Set(bb->gpioNode[i], SIG_PULLUP);
		bb->gpioTrace[i] = SigNode_Trace(gpionode, gpio_trace_proc, ti);

		sprintf(confname, "gpio%d", i);
		result = Config_ReadInt32(&gpioval, "ns9750", confname);
		if (result >= 0) {
			SigNode *pull;
			pull = SigNode_New("bbutil.gpio.%d.pull", i);
			if (!pull) {
				exit(5328);
			}
			if (gpioval) {
				SigNode_Set(pull, SIG_PULLUP);
			} else {
				SigNode_Set(pull, SIG_PULLDOWN);
			}
			SigNode_Link(pull, gpionode);
		}
	}
	for (i = 0; i < 32; i++) {
		BB_IrqDescr *idesc = &ns9750_interrupts[i];
		if (idesc->name) {
			Irq_TraceInfo *ti = &bb->irqTraceInfo[i];
			if (!(bb->irqNode[i] = SigNode_New("%s.%s", devname, idesc->name))) {
				fprintf(stderr, "Can not create bbus irq node %s\n", str);
				exit(3);
			}
			SigNode_Set(bb->irqNode[i], SIG_PULLUP);
			ti->irq_nr = i;
			ti->bbus = bb;
			SigNode_Trace(bb->irqNode[i], irq_trace_proc, ti);
		}
	}
	update_endian_nodes(bb);
	//SigName_Link("bbutil.gpio.35","bbutil.gpio.37");
	bb->bdev.first_mapping = NULL;
	bb->bdev.Map = NS9750BB_Map;
	bb->bdev.UnMap = NS9750BB_UnMap;
	bb->bdev.owner = bb;
	bb->bdev.hw_flags = MEM_FLAG_WRITABLE | MEM_FLAG_READABLE;
	Mem_AreaAddMapping(&bb->bdev, BB_BRIDGE_BASE, BB_BRIDGE_MAPSIZE,
			   MEM_FLAG_WRITABLE | MEM_FLAG_READABLE);
	return &bb->bdev;
}
