/*
 ***************************************************************************************************
 *
 * Emulation of AT91RM9200 Power Management Controller (PMC)
 *
 * state: working 
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
#include "bus.h"
#include "signode.h"
#include "cycletimer.h"
#include "clock.h"
#include "at91_pmc.h"
#include "sgstring.h"
#include "configfile.h"

#define PMC_SCER(base)		((base)+0x00)
#define PMC_SCDR(base)		((base)+0x04)
#define PMC_SCSR(base)		((base)+0x08)
#define PMC_PCER(base)		((base)+0x10)
#define PMC_PCDR(base)		((base)+0x14)
#define	PMC_PCSR(base)		((base)+0x18)
#define CKGR_MOR(base)		((base)+0x20)
#define 	MOR_MOSCEN	(1<<0)
#define	CKGR_MCFR(base)		((base)+0x24)
#define CKGR_PLLAR(base)	((base)+0x28)
#define CKGR_PLLBR(base)	((base)+0x2c)
#define PMC_MCKR(base)		((base)+0x30)
#define		MCKR_MDIV_MASK	(3<<8)
#define		MCKR_MDIV_SHIFT	(8)
#define		MCKR_PRES_MASK	(7<<2)
#define		MCKR_PRES_SHIFT	(2)
#define		MCKR_CSS_MASK	(3)
#define		  MCKR_CSS_SCLK		(0)
#define		  MCKR_CSS_MAINCK	(1)
#define		  MCKR_CSS_PLLACK	(2)
#define		  MCKR_CSS_PLLBCK	(3)
#define PMC_PCK0(base)		((base)+0x40)
#define PMC_PCK1(base)		((base)+0x44)
#define PMC_PCK2(base)		((base)+0x48)
#define PMC_PCK3(base)		((base)+0x4c)
#define		PCK_CSS_MASK	(3)
#define		    PCK_CSS_SLCK	(0)
#define		    PCK_CSS_MAINCK	(1)
#define		    PCK_CSS_PLLACK	(2)
#define		    PCK_CSS_PLLBCK	(3)
#define		PCK_PRES_MASK	(7 << 2)
#define		PCK_PRES_SHIFT	(2)
#define PMC_IER(base)		((base)+0x60)
#define PMC_IDR(base)		((base)+0x64)
#define PMC_SR(base)		((base)+0x68)
#define		SR_PCK3RDY	(1<<11)
#define		SR_PCK2RDY	(1<<10)
#define		SR_PCK1RDY	(1<<9)
#define		SR_PCK0RDY	(1<<8)
#define		SR_MCKRDY	(1<<3)
#define		SR_LOCKB	(1<<2)
#define		SR_LOCKA	(1<<1)
#define		SR_MOSCS	(1<<0)
#define PMC_IMR(base)		((base)+0x6c)

static char *pidTableRM9200[32] = {
	NULL,
	NULL,
	"pc_pioa",
	"pc_piob",
	"pc_pioc",
	"pc_piod",
	"pc_usart0",
	"pc_usart1",
	"pc_usart2",
	"pc_usart3",
	"pc_msi",
	"pc_udp",
	"pc_twi",
	"pc_spi",
	"pc_ssc0",
	"pc_ssc1",
	"pc_ssc2",
	"pc_tc0",
	"pc_tc1",
	"pc_tc2",
	"pc_tc3",
	"pc_tc4",
	"pc_tc5",
	"pc_uhp",
	"pc_emac",
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
};

static char *pidTableSAM9263[32] = {
	NULL,			/* "pc_aic" */
	"pc_sysc",		/* "pc_sysc" */
	"pc_pioa",
	"pc_piob",
	"pc_piocde",
	NULL,
	NULL,
	"pc_us0",
	"pc_us1",
	"pc_us2",
	"pc_mci0",
	"pc_mci1",
	"pc_can",
	"pc_twi",
	"pc_spi0",
	"pc_spi1",
	"pc_ssc0",
	"pc_ssc1",
	"pc_ac97",
	"pc_tc012",
	"pc_pwmc",
	"pc_emac",
	NULL,
	"pc_2dge",
	"pc_udp",
	"pc_isi",
	"pc_lcdc",
	"pc_dma",
	NULL,
	"pc_uhp",
	NULL,			/* "pc_aic2", */
	NULL,			/* "pc_aic3", */
};

typedef struct AT91Pmc {
	BusDevice bdev;
	SigNode *irqNode;
	uint32_t scsr;
	uint32_t pcsr;
	uint32_t mor;
	uint32_t mcfr;
	uint32_t pllar;
	uint32_t pllbr;
	uint32_t mckr;
	uint32_t pck[4];
	uint32_t sr;
	uint32_t imr;
	Clock_t *slck;
	uint32_t freqXin;
	Clock_t *main_clk;
	Clock_t *plla_clk;
	Clock_t *pllb_clk;
	Clock_t *cpu_clk;
	Clock_t *mck;
	Clock_t *udpck;
	Clock_t *uhpck;
	Clock_t *pck_clk[4];
	/* Peripheral clocks */
	Clock_t *pcClk[32];

} AT91Pmc;

static void
dump_clocks(AT91Pmc * pmc)
{
//      Clock_DumpTree(pmc->main_clk);
}

static void
update_interrupt(AT91Pmc * pmc)
{
	/* System interupt is wired or, positive level */
	if (pmc->sr & pmc->imr) {
		SigNode_Set(pmc->irqNode, SIG_HIGH);
	} else {
		SigNode_Set(pmc->irqNode, SIG_PULLDOWN);
	}
}

static void
update_pck_n(AT91Pmc * pmc, unsigned int index)
{
	uint32_t css;
	int prescaler;
	int ena = !!(pmc->scsr & (1 << (index + 8)));
	if (index > 3) {
		return;
	}
	css = pmc->pck[index] & PCK_CSS_MASK;
	prescaler = 1 << ((pmc->pck[index] & PCK_PRES_MASK) >> PCK_PRES_SHIFT);
	Clock_t *srcclk;
	fprintf(stderr, "PCK%d css %u\n", index, css);
	switch (css) {
	    case PCK_CSS_SLCK:
		    srcclk = pmc->slck;
		    break;
	    case PCK_CSS_MAINCK:
		    srcclk = pmc->main_clk;
		    break;
	    case PCK_CSS_PLLACK:
		    srcclk = pmc->plla_clk;
		    break;
	    case PCK_CSS_PLLBCK:
		    srcclk = pmc->pllb_clk;
		    break;
	    default:
		    return;
	}
	if (ena) {
		Clock_MakeDerived(pmc->pck_clk[index], srcclk, 1, prescaler);
		pmc->sr |= (1 << (8 + index));
	} else {
		Clock_MakeDerived(pmc->pck_clk[index], srcclk, 0, prescaler);
		pmc->sr &= ~(1 << (8 + index));
	}
	update_interrupt(pmc);
}

static uint32_t
scer_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "AT91Pmc: read from writeonly register SCER\n");
	return 0;
}

static void
scer_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	AT91Pmc *pmc = (AT91Pmc *) clientData;
	int i;
	pmc->scsr |= value & 0xf17;
	for (i = 0; i < 4; i++) {
		update_pck_n(pmc, i);
	}
}

static uint32_t
scdr_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "AT91Pmc: read from writeonly register SDER\n");
	return 0;
}

static void
scdr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	AT91Pmc *pmc = (AT91Pmc *) clientData;
	int i;
	pmc->scsr &= ~(value & 0xf17);
	for (i = 0; i < 4; i++) {
		update_pck_n(pmc, i);
	}
}

static uint32_t
scsr_read(void *clientData, uint32_t address, int rqlen)
{
	AT91Pmc *pmc = (AT91Pmc *) clientData;
	return pmc->scsr;
}

static void
scsr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "AT91Pmc:  write to readonly register SDSR\n");
}

static uint32_t
pcer_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "AT91Pmc: read from writeonly register PCER\n");
	return 0;
}

static void
pcer_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	AT91Pmc *pmc = (AT91Pmc *) clientData;
	unsigned int i;
	uint32_t diff = pmc->pcsr ^ (pmc->pcsr | value);
	pmc->pcsr |= value;
	for (i = 0; i < 32; i++) {
		if (diff & (1 << i) && pmc->pcClk[i]) {
			Clock_MakeDerived(pmc->pcClk[i], pmc->mck, 1, 1);
		}
	}
	dump_clocks(pmc);
}

static uint32_t
pcdr_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "AT91Pmc: read from writeonly register PCDR\n");
	return 0;
}

static void
pcdr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	AT91Pmc *pmc = (AT91Pmc *) clientData;
	unsigned int i;
	uint32_t diff = pmc->pcsr ^ (pmc->pcsr & ~value);
	pmc->pcsr &= ~value;
	for (i = 0; i < 32; i++) {
		if (diff & (1 << i) && pmc->pcClk[i]) {
			Clock_MakeDerived(pmc->pcClk[i], pmc->mck, 0, 1);
		}
	}
}

static uint32_t
pcsr_read(void *clientData, uint32_t address, int rqlen)
{
	AT91Pmc *pmc = (AT91Pmc *) clientData;
	return pmc->pcsr;
}

static void
pcsr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "AT91Pmc: write to readonly register PCSR\n");
}

static uint32_t
mor_read(void *clientData, uint32_t address, int rqlen)
{
	AT91Pmc *pmc = (AT91Pmc *) clientData;
	return pmc->mor;
}

static void
mor_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	AT91Pmc *pmc = (AT91Pmc *) clientData;
	pmc->mor = value & 0xff01;
	if (value & MOR_MOSCEN) {
		Clock_SetFreq(pmc->main_clk, pmc->freqXin);
		pmc->sr |= SR_MOSCS;
		fprintf(stderr, "AT91Pmc: Enabled master clock\n");
		//dump_clocks(pmc);
	} else {
		Clock_SetFreq(pmc->main_clk, 0);
		pmc->sr &= ~SR_MOSCS;
	}
	update_interrupt(pmc);
}

static uint32_t
mcfr_read(void *clientData, uint32_t address, int rqlen)
{
	AT91Pmc *pmc = (AT91Pmc *) clientData;
	uint32_t mcfr = 0;
	if (Clock_Freq(pmc->slck) && (pmc->mor & MOR_MOSCEN)) {
		mcfr = 16 * Clock_Freq(pmc->main_clk) / Clock_Freq(pmc->slck);
		mcfr |= (1 << 16);
	} else {
		fprintf(stderr, "Can not read MCFR with disabled main clock or without slck\n");
	}
	return mcfr;
}

static void
mcfr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "AT91Pmc: Illegal write to readonly register MCFR\n");
}

static uint32_t
pllar_read(void *clientData, uint32_t address, int rqlen)
{
	AT91Pmc *pmc = (AT91Pmc *) clientData;
	return pmc->pllar;
}

static void
pllar_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	AT91Pmc *pmc = (AT91Pmc *) clientData;
	pmc->pllar = value;
	int mul = ((value >> 16) & 0x7ff) + 1;
	int div = value & 0xff;
	if (div == 0) {
		Clock_MakeDerived(pmc->plla_clk, pmc->main_clk, 0, 1);
		pmc->sr &= ~SR_LOCKA;
	} else {
		Clock_MakeDerived(pmc->plla_clk, pmc->main_clk, mul, div);
		pmc->sr |= SR_LOCKA;
	}
	update_interrupt(pmc);
}

static uint32_t
pllbr_read(void *clientData, uint32_t address, int rqlen)
{
	AT91Pmc *pmc = (AT91Pmc *) clientData;
	return pmc->pllbr;
}

static void
pllbr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	AT91Pmc *pmc = (AT91Pmc *) clientData;
	pmc->pllbr = value;
	int mul = ((value >> 16) & 0x7ff) + 1;
	int div = value & 0xff;
	if (div == 0) {
		Clock_MakeDerived(pmc->pllb_clk, pmc->main_clk, 0, 1);
		pmc->sr &= ~SR_LOCKB;
	} else {
		pmc->sr |= SR_LOCKB;
		if (value & (1 << 28)) {
			Clock_MakeDerived(pmc->pllb_clk, pmc->main_clk, mul, div * 2);
		} else {
			Clock_MakeDerived(pmc->pllb_clk, pmc->main_clk, mul, div);
		}
	}
	update_interrupt(pmc);
}

static uint32_t
mckr_read(void *clientData, uint32_t address, int rqlen)
{
	AT91Pmc *pmc = (AT91Pmc *) clientData;
	return pmc->mckr;
}

static void
mckr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	AT91Pmc *pmc = (AT91Pmc *) clientData;
	pmc->mckr = value;
	int mdiv = ((value & MCKR_MDIV_MASK) >> MCKR_MDIV_SHIFT) + 1;
	int prescaler = 1 << ((value & MCKR_PRES_MASK) >> MCKR_PRES_SHIFT);
	uint32_t css = value & MCKR_CSS_MASK;
	switch (css) {
	    case MCKR_CSS_SCLK:
		    Clock_MakeDerived(pmc->cpu_clk, pmc->slck, 1, prescaler);
		    break;
	    case MCKR_CSS_MAINCK:
		    Clock_MakeDerived(pmc->cpu_clk, pmc->main_clk, 1, prescaler);
		    break;
	    case MCKR_CSS_PLLACK:
		    Clock_MakeDerived(pmc->cpu_clk, pmc->plla_clk, 1, prescaler);
		    break;

	    case MCKR_CSS_PLLBCK:
		    Clock_MakeDerived(pmc->cpu_clk, pmc->pllb_clk, 1, prescaler);
		    break;
	}
	Clock_MakeDerived(pmc->mck, pmc->cpu_clk, 1, mdiv);
	/* 
	 * SR_MCKRDY should be set on a clock trace handler as soon as there is
	 * a nonzero frequency on mck
	 */
	pmc->sr |= SR_MCKRDY;
	update_interrupt(pmc);
	//fprintf(stderr,"mdiv %d\n",mdiv);
	//sleep(3);
	//dump_clocks(pmc);
}

static uint32_t
pck_read(void *clientData, uint32_t address, int rqlen)
{
	AT91Pmc *pmc = (AT91Pmc *) clientData;
	int index = (address >> 2) & 3;
	return pmc->pck[index];
}

static void
pck_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	AT91Pmc *pmc = (AT91Pmc *) clientData;
	int index = (address >> 2) & 3;
	pmc->pck[index] = value & 0x1f;
	update_pck_n(pmc, index);
}

static uint32_t
ier_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "AT91Pmc: Illegal read from writeonly register IER\n");
	return 0;
}

static void
ier_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	AT91Pmc *pmc = (AT91Pmc *) clientData;
	pmc->imr |= (value & 0x0f0f);
	update_interrupt(pmc);
}

static uint32_t
idr_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "AT91Pmc: Illegal read from writeonly register IDR\n");
	return 0;
}

static void
idr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	AT91Pmc *pmc = (AT91Pmc *) clientData;
	pmc->imr &= ~(value & 0x0f0f);
	update_interrupt(pmc);
}

static uint32_t
sr_read(void *clientData, uint32_t address, int rqlen)
{
	AT91Pmc *pmc = (AT91Pmc *) clientData;
	return pmc->sr;
}

static void
sr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "AT91Pmc: Illegal write to readonly SR\n");
}

static uint32_t
imr_read(void *clientData, uint32_t address, int rqlen)
{
	AT91Pmc *pmc = (AT91Pmc *) clientData;
	return pmc->imr;
}

static void
imr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "AT91Pmc: IMR register is readonly\n");
}

static void
AT91Pmc_Map(void *owner, uint32_t base, uint32_t mask, uint32_t flags)
{
	AT91Pmc *pmc = (AT91Pmc *) owner;
	IOH_New32(PMC_SCER(base), scer_read, scer_write, pmc);
	IOH_New32(PMC_SCDR(base), scdr_read, scdr_write, pmc);
	IOH_New32(PMC_SCSR(base), scsr_read, scsr_write, pmc);
	IOH_New32(PMC_PCER(base), pcer_read, pcer_write, pmc);
	IOH_New32(PMC_PCDR(base), pcdr_read, pcdr_write, pmc);
	IOH_New32(PMC_PCSR(base), pcsr_read, pcsr_write, pmc);
	IOH_New32(CKGR_MOR(base), mor_read, mor_write, pmc);
	IOH_New32(CKGR_MCFR(base), mcfr_read, mcfr_write, pmc);
	IOH_New32(CKGR_PLLAR(base), pllar_read, pllar_write, pmc);
	IOH_New32(CKGR_PLLBR(base), pllbr_read, pllbr_write, pmc);
	IOH_New32(PMC_MCKR(base), mckr_read, mckr_write, pmc);
	IOH_New32(PMC_PCK0(base), pck_read, pck_write, pmc);
	IOH_New32(PMC_PCK1(base), pck_read, pck_write, pmc);
	IOH_New32(PMC_PCK2(base), pck_read, pck_write, pmc);
	IOH_New32(PMC_PCK3(base), pck_read, pck_write, pmc);
	IOH_New32(PMC_IER(base), ier_read, ier_write, pmc);
	IOH_New32(PMC_IDR(base), idr_read, idr_write, pmc);
	IOH_New32(PMC_SR(base), sr_read, sr_write, pmc);
	IOH_New32(PMC_IMR(base), imr_read, imr_write, pmc);
}

static void
AT91Pmc_UnMap(void *owner, uint32_t base, uint32_t mask)
{
	IOH_Delete32(PMC_SCER(base));
	IOH_Delete32(PMC_SCDR(base));
	IOH_Delete32(PMC_SCSR(base));
	IOH_Delete32(PMC_PCER(base));
	IOH_Delete32(PMC_PCDR(base));
	IOH_Delete32(PMC_PCSR(base));
	IOH_Delete32(CKGR_MOR(base));
	IOH_Delete32(CKGR_MCFR(base));
	IOH_Delete32(CKGR_PLLAR(base));
	IOH_Delete32(CKGR_PLLBR(base));
	IOH_Delete32(PMC_MCKR(base));
	IOH_Delete32(PMC_PCK0(base));
	IOH_Delete32(PMC_PCK1(base));
	IOH_Delete32(PMC_PCK2(base));
	IOH_Delete32(PMC_PCK3(base));
	IOH_Delete32(PMC_IER(base));
	IOH_Delete32(PMC_IDR(base));
	IOH_Delete32(PMC_SR(base));
	IOH_Delete32(PMC_IMR(base));

}

BusDevice *
AT91Pmc_New(const char *name, unsigned int pid_table)
{
	unsigned int i;
	char **pidTable;

	AT91Pmc *pmc = sg_new(AT91Pmc);
	pmc->irqNode = SigNode_New("%s.irq", name);
	if (!pmc->irqNode) {
		fprintf(stderr, "Failed to create irq line for AT91 PMC\n");
		exit(1);
	}
	SigNode_Set(pmc->irqNode, SIG_PULLDOWN);
	pmc->freqXin = 18432000;
	Config_ReadUInt32(&pmc->freqXin, "global", "xin");
	switch (pid_table) {
	    case AT91_PID_TABLE_RM9200:
		    pidTable = pidTableRM9200;
		    break;
	    case AT91_PID_TABLE_SAM9263:
		    pidTable = pidTableSAM9263;
		    break;
	    default:
		    fprintf(stderr, "Selected nonexisting PID table %u\n", pid_table);
		    exit(1);
	}
	pmc->slck = Clock_New("%s.slck", name);
	pmc->main_clk = Clock_New("%s.main_clk", name);
	pmc->plla_clk = Clock_New("%s.plla_clk", name);
	pmc->pllb_clk = Clock_New("%s.pllb_clk", name);
	pmc->cpu_clk = Clock_New("%s.cpu_clk", name);
	pmc->mck = Clock_New("%s.mck", name);
	pmc->udpck = Clock_New("%s.udpck", name);
	pmc->uhpck = Clock_New("%s.uhpck", name);
	pmc->pck_clk[0] = Clock_New("%s.pck0", name);
	pmc->pck_clk[1] = Clock_New("%s.pck1", name);
	pmc->pck_clk[2] = Clock_New("%s.pck2", name);
	pmc->pck_clk[3] = Clock_New("%s.pck3", name);
	Clock_SetFreq(pmc->slck, 32768);
	for (i = 0; i < 32; i++) {
		if (pidTable[i] != 0) {
			pmc->pcClk[i] = Clock_New("%s.%s", name, pidTable[i]);
		}
	}
	pmc->scsr = 0x01;
	pmc->pcsr = 0xffffffff;
	pcdr_write(pmc, 0xffffffff, 0, 4);
	pmc->mor = 0;
	pmc->pllar = 0x3f00;
	pmc->pllbr = 0x3f00;
	pmc->mckr = 0;
	pmc->pck[0] = pmc->pck[1] = pmc->pck[2] = pmc->pck[3] = 0;
	pmc->sr = 0x0f0e;
	pmc->imr = 0;
	pllar_write(pmc, pmc->pllar, 0, 4);
	pllbr_write(pmc, pmc->pllbr, 0, 4);
	mckr_write(pmc, pmc->mckr, 0, 4);
	pmc->bdev.first_mapping = NULL;
	pmc->bdev.Map = AT91Pmc_Map;
	pmc->bdev.UnMap = AT91Pmc_UnMap;
	pmc->bdev.owner = pmc;
	pmc->bdev.hw_flags = MEM_FLAG_WRITABLE | MEM_FLAG_READABLE;
	update_interrupt(pmc);
	fprintf(stderr, "AT91 variant %u Power Management Controller created\n", pid_table);
	return &pmc->bdev;
}
