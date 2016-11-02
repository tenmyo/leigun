/*
 *************************************************************************************************
 *
 * Emulation of the AT91RM9200 External Bus Interface (EBI)
 *
 *  State: Registers writable but no functionality 
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
#include "at91_ebi.h"
#include "sgstring.h"

#define EBI_CSA(base) 		((base)+0x00)
#define		CS4A	(1<<4)
#define		CS3A	(1<<3)
#define		CS1A	(1<<1)
#define		CS0A	(1<<0)
#define	EBI_CFGR(base)		((base)+0x04)
#define		CFGR_DBPUC	(1<<0)
#define SMC_CSR(base,n) 	((base)+0x10+0x04*(n))
#define		CSR_RWHOLD_MASK		(7<<28)
#define		CSR_RWHOLD_SHIFT	(28)
#define		CSR_RWSETUP_MASK	(7<<24)
#define		CSR_RWSETUP_SHIFT	(24)
#define		CSR_ACSS_MASK		(3<<16)
#define		CSR_ACSS_SHIFT		(16)
#define		CSR_DRP			(1<<15)
#define		CSR_DBW_MASK		(3<<13)
#define		CSR_DBW_SHIFT		(13)
#define		CSR_BAT			(1<<12)
#define		CSR_TDF_MASK		(0xf<<8)
#define		CSR_TDF_SHIFT		(8)
#define		CSR_WSEN		(1<<7)
#define		CSR_NWS_MASK		(0x7f)
#define		CSR_NWS_SHIFT		(0)
#define	SDRAMC_MR(base)		((base)+0x30+0x00)
#define		MR_DBW		(1<<4)
#define		MR_MODE_MASK	(0xf)
#define		MR_MODE_SHIFT	(0)
#define SDRAMC_TR(base)		((base)+0x30+0x04)
#define SDRAMC_CR(base)		((base)+0x30+0x08)
#define		CR_TXSR_MASK	(0xf << 27)
#define		CR_TXSR_SHIFT	(27)
#define		CR_TRAS_MASK	(0xf << 23)
#define		CR_TRAS_SHIFT	(23)
#define		CR_TRCD_MASK	(0xf<<19)
#define		CR_TRCD_SHIFT	(19)
#define		CR_TRP_MASK	(0xf<<15)
#define		CR_TRP_SHIFT	(15)
#define		CR_TRC_MASK	(0xf<<11)
#define		CR_TRC_SHIFT	(11)
#define 	CR_TWR_MASK	(0xf<<7)
#define 	CR_TWR_SHIFT	(0xf<<7)
#define		CR_CAS_MASK	(0x3<<5)
#define		CR_CAS_SHIFT	(5)
#define		CR_NB		(1<<4)
#define		CR_NR_MASK	(3<<2)
#define		CR_NR_SHIFT	(2)
#define		CR_NC_MASK	(3)
#define		CR_NC_SHIFT	(0)
#define SDRAMC_SRR(base)	((base)+0x30+0x0c)
#define		SRR_SRCB	(1<<0)
#define SDRAMC_LPR(base)	((base)+0x30+0x10)
#define		LPR_LPCB	(1<<0)
#define SDRAMC_IER(base)	((base)+0x30+0x14)
#define SDRAMC_IDR(base)	((base)+0x30+0x18)
#define SDRAMC_IMR(base)	((base)+0x30+0x1c)
#define		IMR_RES		(1<<0)
#define SDRAMC_ISR(base)	((base)+0x30+0x20)
#define		ISR_RES		(1<<0)
#define BFC_MR(base)		((base)+0x60+0x00)
#define		BFC_MR_RXDYEN	(1<<19)
#define		BFC_MR_MUXEN	(1<<18)
#define		BFC_MR_BFOER	(1<<17)
#define		BFC_MR_BAAEN	(1<<16)
#define		BFC_OEL_MASK	(3<<12)
#define		BFC_OEL_SHIFT	(12)
#define		BFC_PAGES_MASK	(7<<8)
#define		BFC_PAGES_SHIFT	(8)
#define		BFC_AVL_MASK	(0xf<<4)
#define		BFC_AVL_SHIFT	(4)
#define		BFC_BFCC_MASK	(3<<2)
#define		BFC_BFCC_SHIFT	(2)
#define		BFC_BFCOM_MASK	(3)
#define		BFC_BFCOM_SHIFT	(0)

typedef struct AT91Ebi {
	BusDevice bdev;
	uint32_t ebi_csa;
	uint32_t ebi_cfgr;
	uint32_t smc_csr[8];
	uint32_t sdramc_mr;
	uint32_t sdramc_tr;
	uint32_t sdramc_cr;
	uint32_t sdramc_srr;
	uint32_t sdramc_lpr;
	uint32_t sdramc_imr;
	uint32_t sdramc_isr;
	uint32_t bfc;
} AT91Ebi;

static void
update_interrupt(AT91Ebi * ebi)
{

}

static uint32_t
ebi_csa_read(void *clientData, uint32_t address, int rqlen)
{
	AT91Ebi *ebi = (AT91Ebi *) clientData;
	return ebi->ebi_csa;
}

static void
ebi_csa_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	AT91Ebi *ebi = (AT91Ebi *) clientData;
	ebi->ebi_csa = value & 0x1b;
}

static uint32_t
ebi_cfgr_read(void *clientData, uint32_t address, int rqlen)
{
	AT91Ebi *ebi = (AT91Ebi *) clientData;
	return ebi->ebi_cfgr;
}

static void
ebi_cfgr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	AT91Ebi *ebi = (AT91Ebi *) clientData;
	ebi->ebi_cfgr = (value & 1);
}

static uint32_t
smc_csr_read(void *clientData, uint32_t address, int rqlen)
{
	AT91Ebi *ebi = (AT91Ebi *) clientData;
	int wsen;
	uint32_t value;
	/* Shit hack */
	int index = ((address - 0x70) >> 2) & 7;
	value = ebi->smc_csr[index];
	wsen = !!(value & CSR_WSEN);
	if (!wsen) {
		value &= ~CSR_NWS_MASK;
	}
	return value;
}

static void
smc_csr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	AT91Ebi *ebi = (AT91Ebi *) clientData;
	/* Shit hack */
	int index = ((address - 0x70) >> 2) & 7;
	value = value & 0x7703ffff;
	uint16_t rwhold, rwsetup, acss, drp, dbw, bat, tdf, wsen, nws;

	rwhold = (value & CSR_RWHOLD_MASK) >> CSR_RWHOLD_SHIFT;
	rwsetup = (value & CSR_RWSETUP_MASK) >> CSR_RWSETUP_SHIFT;
	acss = (value & CSR_ACSS_MASK) >> CSR_ACSS_SHIFT;
	drp = !!(value & CSR_DRP);
	dbw = (value & CSR_DBW_MASK) >> CSR_DBW_SHIFT;
	bat = !!(value & CSR_BAT);
	tdf = (value & CSR_TDF_MASK) >> CSR_TDF_SHIFT;
	wsen = !!(value & CSR_WSEN);
	nws = (value & CSR_NWS_MASK) >> CSR_NWS_SHIFT;
	fprintf(stderr,
		"*** AT91Ebi: %08x CSR%d: rwhold %d, rwsetup %d, acss %d, drp %d, dbw %d, bat %d,tdf %d wsen %d, nws %d\n",
		value, index, rwhold, rwsetup, acss, drp, dbw, bat, tdf, wsen, nws);
	ebi->smc_csr[index] = value;
}

static uint32_t
sdramc_mr_read(void *clientData, uint32_t address, int rqlen)
{
	AT91Ebi *ebi = (AT91Ebi *) clientData;
	return ebi->sdramc_mr;
}

static void
sdramc_mr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	AT91Ebi *ebi = (AT91Ebi *) clientData;
	ebi->sdramc_mr = value & 0x1f;
}

static uint32_t
sdramc_tr_read(void *clientData, uint32_t address, int rqlen)
{
	AT91Ebi *ebi = (AT91Ebi *) clientData;
	return ebi->sdramc_tr;
}

static void
sdramc_tr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	AT91Ebi *ebi = (AT91Ebi *) clientData;
	ebi->sdramc_tr = value & 0xfff;
}

static uint32_t
sdramc_cr_read(void *clientData, uint32_t address, int rqlen)
{
	AT91Ebi *ebi = (AT91Ebi *) clientData;
	return ebi->sdramc_cr;
}

static void
sdramc_cr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	AT91Ebi *ebi = (AT91Ebi *) clientData;
	ebi->sdramc_cr = value & 0x7fffffff;
}

static uint32_t
sdramc_srr_read(void *clientData, uint32_t address, int rqlen)
{
	AT91Ebi *ebi = (AT91Ebi *) clientData;
	return ebi->sdramc_srr;
}

static void
sdramc_srr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	AT91Ebi *ebi = (AT91Ebi *) clientData;
	ebi->sdramc_srr = value & 1;
}

static uint32_t
sdramc_lpr_read(void *clientData, uint32_t address, int rqlen)
{
	AT91Ebi *ebi = (AT91Ebi *) clientData;
	return ebi->sdramc_lpr;
}

static void
sdramc_lpr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	AT91Ebi *ebi = (AT91Ebi *) clientData;
	ebi->sdramc_lpr = value & 1;
}

static uint32_t
sdramc_ier_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "AT91Ebi: SDRAMC_IER is not readable\n");
	return 0;
}

static void
sdramc_ier_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	AT91Ebi *ebi = (AT91Ebi *) clientData;
	ebi->sdramc_imr |= value & 1;
	update_interrupt(ebi);
}

static uint32_t
sdramc_idr_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "AT91Ebi: SDRAMC_IDR is not readable\n");
	return 0;
}

static void
sdramc_idr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	AT91Ebi *ebi = (AT91Ebi *) clientData;
	ebi->sdramc_imr &= ~(value & 1);
	update_interrupt(ebi);
}

static uint32_t
sdramc_imr_read(void *clientData, uint32_t address, int rqlen)
{
	AT91Ebi *ebi = (AT91Ebi *) clientData;
	return ebi->sdramc_imr;
}

static void
sdramc_imr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "AT91Ebi: SDRAMC_IMR is not writable\n");
}

static uint32_t
sdramc_isr_read(void *clientData, uint32_t address, int rqlen)
{
	AT91Ebi *ebi = (AT91Ebi *) clientData;
	return ebi->sdramc_isr;
}

static void
sdramc_isr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "AT91Ebi: SDRAMC_ISR is not writeable\n");
}

static uint32_t
bfc_read(void *clientData, uint32_t address, int rqlen)
{
	AT91Ebi *ebi = (AT91Ebi *) clientData;
	return ebi->bfc;
}

static void
bfc_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	AT91Ebi *ebi = (AT91Ebi *) clientData;
	ebi->bfc = value & 0x000f37ff;
}

static void
AT91Ebi_Map(void *owner, uint32_t base, uint32_t mask, uint32_t flags)
{
	AT91Ebi *ebi = (AT91Ebi *) owner;
	int i;
	IOH_New32(EBI_CSA(base), ebi_csa_read, ebi_csa_write, ebi);
	IOH_New32(EBI_CFGR(base), ebi_cfgr_read, ebi_cfgr_write, ebi);
	for (i = 0; i < 8; i++) {
		IOH_New32(SMC_CSR(base, i), smc_csr_read, smc_csr_write, ebi);
	}
	IOH_New32(SDRAMC_MR(base), sdramc_mr_read, sdramc_mr_write, ebi);
	IOH_New32(SDRAMC_TR(base), sdramc_tr_read, sdramc_tr_write, ebi);
	IOH_New32(SDRAMC_CR(base), sdramc_cr_read, sdramc_cr_write, ebi);
	IOH_New32(SDRAMC_SRR(base), sdramc_srr_read, sdramc_srr_write, ebi);
	IOH_New32(SDRAMC_LPR(base), sdramc_lpr_read, sdramc_lpr_write, ebi);
	IOH_New32(SDRAMC_IER(base), sdramc_ier_read, sdramc_ier_write, ebi);
	IOH_New32(SDRAMC_IDR(base), sdramc_idr_read, sdramc_idr_write, ebi);
	IOH_New32(SDRAMC_IMR(base), sdramc_imr_read, sdramc_imr_write, ebi);
	IOH_New32(SDRAMC_ISR(base), sdramc_isr_read, sdramc_isr_write, ebi);
	IOH_New32(BFC_MR(base), bfc_read, bfc_write, ebi);
}

static void
AT91Ebi_UnMap(void *owner, uint32_t base, uint32_t mask)
{
	int i;
	IOH_Delete32(EBI_CSA(base));
	IOH_Delete32(EBI_CFGR(base));
	for (i = 0; i < 8; i++) {
		IOH_Delete32(SMC_CSR(base, i));
	}
	IOH_Delete32(SDRAMC_MR(base));
	IOH_Delete32(SDRAMC_TR(base));
	IOH_Delete32(SDRAMC_CR(base));
	IOH_Delete32(SDRAMC_SRR(base));
	IOH_Delete32(SDRAMC_LPR(base));
	IOH_Delete32(SDRAMC_IER(base));
	IOH_Delete32(SDRAMC_IDR(base));
	IOH_Delete32(SDRAMC_IMR(base));
	IOH_Delete32(SDRAMC_ISR(base));
	IOH_Delete32(BFC_MR(base));
}

BusDevice *
AT91Ebi_New(const char *name)
{
	AT91Ebi *ebi = sg_new(AT91Ebi);
	int i;
	ebi->ebi_csa = 0;
	ebi->ebi_cfgr = 0;
	for (i = 0; i < 8; i++) {
		ebi->smc_csr[i] = 0x2000;
	}
	ebi->sdramc_mr = 0x10;
	ebi->sdramc_tr = 0x800;
	ebi->sdramc_cr = 0x2a99c140;
	ebi->sdramc_lpr = 0;
	ebi->sdramc_imr = 0;
	ebi->sdramc_isr = 0;
	ebi->bfc = 0;
	ebi->bdev.first_mapping = NULL;
	ebi->bdev.Map = AT91Ebi_Map;
	ebi->bdev.UnMap = AT91Ebi_UnMap;
	ebi->bdev.owner = ebi;
	ebi->bdev.hw_flags = MEM_FLAG_WRITABLE | MEM_FLAG_READABLE;
	return &ebi->bdev;
}
