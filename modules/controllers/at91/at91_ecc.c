/*
 ***********************************************************************************************
 *
 * Emulation of the AT91 Error correction controller (ECC) 
 *
 *  State: working
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

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "i2c.h"
#include "bus.h"
#include "signode.h"
#include "sgstring.h"
#include "sglib.h"
#include "at91_ecc.h"

#define REG_ECC_CR(base)	((base) + 0x00)
#define REG_ECC_MR(base)	((base) + 0x04)
#define REG_ECC_SR(base)	((base) + 0x08)
#define		SR_RECERR	(1 << 0)
#define		SR_ECCERR	(1 << 1)
#define		SR_MULERR	(1 << 2)
#define REG_ECC_PR(base)	((base) + 0x0c)
#define REG_ECC_NPR(base)	((base) + 0x10)

struct AT91Ecc {
	BusDevice bdev;
	const char *name;
	uint32_t regCR;
	uint32_t regMR;
	uint32_t pagesize;
	uint32_t regSR;
	uint32_t regPR;
	uint32_t regNPR;
	/* State machine eating incoming bytes */
	uint32_t wordCnt;	/* Number of bytes/words in ecc block */
	uint16_t readnPar;
	uint16_t readPar;

	uint16_t calcnPar;
	uint16_t calcPar;
};

static uint8_t *precalc_col8_tab = NULL;
static uint8_t *precalc_col8_ntab = NULL;
static uint8_t *precalc_bytepar = NULL;

#define BIT(x,n) (!!(((x) >> (n)) & 1))

static void
precalc_col_parity(void)
{
	int i, j;
	uint8_t *tab8, *ntab8, *tabpar;
	uint8_t P1, P2, P4;
	uint8_t P1x, P2x, P4x;
	if (precalc_col8_tab) {
		return;
	}
	tab8 = precalc_col8_tab = sg_calloc(256);
	ntab8 = precalc_col8_ntab = sg_calloc(256);
	for (i = 0; i < 256; i++) {
		P1 = BIT(i, 7) ^ BIT(i, 5) ^ BIT(i, 3) ^ BIT(i, 1);
		P2 = BIT(i, 7) ^ BIT(i, 6) ^ BIT(i, 3) ^ BIT(i, 2);
		P4 = BIT(i, 7) ^ BIT(i, 6) ^ BIT(i, 5) ^ BIT(i, 4);
		P1x = BIT(i, 6) ^ BIT(i, 4) ^ BIT(i, 2) ^ BIT(i, 0);
		P2x = BIT(i, 5) ^ BIT(i, 4) ^ BIT(i, 1) ^ BIT(i, 0);
		P4x = BIT(i, 3) ^ BIT(i, 2) ^ BIT(i, 1) ^ BIT(i, 0);
		tab8[i] = P1 | (P2 << 1) | (P4 << 2);
		ntab8[i] = (P1x << 0) | (P2x << 1) | (P4x << 2);
	}
	tabpar = precalc_bytepar = sg_calloc(256);
	for (i = 0; i < 256; i++) {
		uint8_t par = 0;
		for (j = 0; j < 8; j++) {
			par ^= BIT(i, j);
		}
		tabpar[i] = par;
	}
}

/**
 ************************************************************
 * \fn static void ecc_feed_byte(AT91Ecc *ecc,uint8_t value) 
 * Feed one byte into the parity generator.  Used if the
 * flash is connected with a bus width of 8 Bit 
 ************************************************************
 */
static void
ecc_feed_byte(AT91Ecc * ecc, uint8_t value)
{
	uint32_t cnt = ecc->wordCnt;
	uint8_t par;
	ecc->wordCnt++;
	par = precalc_bytepar[value];
	if (cnt >= ecc->pagesize) {
		switch (cnt - ecc->pagesize) {
		    case 0:
			    ecc->readPar = value;
			    break;
		    case 1:
			    ecc->readPar |= ((uint16_t) value << 8);
			    break;
		    case 2:
			    ecc->readnPar = value;
			    break;
		    case 3:
			    ecc->readnPar |= ((uint16_t) value << 8);
			    break;
		    default:
			    //fprintf(stderr,"To much data\n");
			    break;
		}
		return;
	}
	ecc->calcPar ^= precalc_col8_tab[value];
	ecc->calcnPar ^= precalc_col8_ntab[value];
	if (par) {
		ecc->calcPar ^= (cnt & 0xfff) << 4;
		ecc->calcnPar ^= ((cnt ^ 0xfff) & 0xfff) << 4;
	}
}

/**
 ****************************************************************************************
 * \fn static void ecc_feed_word(AT91Ecc *ecc,uint16_t value) 
 * Feed one 16 Bit word into the parity generator. Used if the Bus width of the
 * NAND flash is 16 Bit.
 * untested !
 ****************************************************************************************
 */
static void
ecc_feed_word(AT91Ecc * ecc, uint16_t value)
{
	uint32_t cnt = ecc->wordCnt;
	uint8_t par;
	ecc->wordCnt++;
	par = precalc_bytepar[value & 0xff] ^ precalc_bytepar[value >> 8];
	if (cnt >= ecc->pagesize) {
		switch (cnt - ecc->pagesize) {
		    case 0:
			    ecc->readPar = value;
			    break;
		    case 1:
			    ecc->readnPar = value;
			    break;
		    default:
			    //fprintf(stderr,"To much data\n");
			    break;
		}
		return;
	}
	if (par) {
		ecc->calcPar ^= (cnt & 0xfff) << 4;
		ecc->calcnPar ^= ((cnt ^ 0xfff) & 0xfff) << 4;
	}
	ecc->calcPar ^= precalc_col8_tab[value & 0xff];
	ecc->calcPar ^= precalc_col8_tab[(value >> 8) & 0xff];
	ecc->calcnPar ^= precalc_col8_ntab[value & 0xff];
	ecc->calcnPar ^= precalc_col8_ntab[(value >> 8) & 0xff];
	ecc->calcPar ^= (precalc_bytepar[value >> 8] << 3);
	ecc->calcnPar ^= (precalc_bytepar[value & 0xff] << 3);
}

/**
 ***************************************************************************
 * \fn static void reset_parity(AT91Ecc *ecc); 
 ***************************************************************************
 */
static inline void
reset_parity(AT91Ecc * ecc)
{
	ecc->readPar = ecc->readnPar = 0;
	ecc->calcPar = ecc->calcnPar = 0;
	ecc->wordCnt = 0;
}

static uint32_t
cr_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "%s is writeonly\n", __func__);
	return 0;
}

static void
cr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	AT91Ecc *ecc = clientData;
	if (value & 1) {
		reset_parity(ecc);
	}
}

/**
 ***************************************************************
 * Mode register. Selects the size of an ECC page.
 ***************************************************************
 */
static uint32_t
mr_read(void *clientData, uint32_t address, int rqlen)
{
	AT91Ecc *ecc = clientData;
	return ecc->regMR;
}

static void
mr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	AT91Ecc *ecc = clientData;
	ecc->regMR = value & 3;
	ecc->pagesize = 512 << (value & 3);
}

/**
 *************************************************************************************************
 * Status register. 
 * It is currently unclear if the status register is updated whenever it is read or whenever
 * a page is feed completely into the ECC controller. Should be tested with the real device.
 *************************************************************************************************
 */

static uint32_t
sr_read(void *clientData, uint32_t address, int rqlen)
{
	AT91Ecc *ecc = clientData;
	uint32_t ones;
	uint32_t syndrome;
	uint32_t result;
	syndrome = (ecc->readPar ^ ecc->calcPar) | ((ecc->readnPar ^ ecc->calcnPar) << 16);
	ones = SGLib_OnecountU32(syndrome);
	//fprintf(stderr,"%s is not implemented par %04x npar %04x cpar %04x cnpar %04x, wordCnt %u\n",__func__,ecc->readPar,ecc->readnPar,ecc->calcPar,ecc->calcnPar,ecc->wordCnt);
	if (ones == 0) {
		result = 0;
	} else if (ones == 1) {
		result = SR_ECCERR;
	} else if (ones == (12 + (ecc->regMR & 3))) {
		result = SR_RECERR;
	} else {
		result = SR_MULERR;
	}
	return result;
}

static void
sr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "AT91ECC: Status register is readonly\n");
}

static uint32_t
pr_read(void *clientData, uint32_t address, int rqlen)
{
	AT91Ecc *ecc = clientData;
	return ecc->calcPar ^ ecc->readPar;
}

static void
pr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "AT91ECC parity is not writable\n");
}

static uint32_t
npr_read(void *clientData, uint32_t address, int rqlen)
{
	AT91Ecc *ecc = clientData;
	return ecc->calcnPar ^ ecc->readnPar;
}

static void
npr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "AT91ECC nparity is not writable\n");
}

void
AT91Ecc_Feed(BusDevice * bd, uint16_t data, int width)
{
	AT91Ecc *ecc = container_of(bd, AT91Ecc, bdev);
	if (width == 1) {
		ecc_feed_byte(ecc, data);
	} else if (width == 2) {
		ecc_feed_word(ecc, data);
	}
}

void
AT91Ecc_ResetEC(BusDevice * bd)
{
	AT91Ecc *ecc = container_of(bd, AT91Ecc, bdev);
	reset_parity(ecc);
}

static void
AT91Ecc_Map(void *owner, uint32_t base, uint32_t mask, uint32_t flags)
{
	AT91Ecc *ecc = (AT91Ecc *) owner;
	IOH_New32(REG_ECC_CR(base), cr_read, cr_write, ecc);
	IOH_New32(REG_ECC_MR(base), mr_read, mr_write, ecc);
	IOH_New32(REG_ECC_SR(base), sr_read, sr_write, ecc);
	IOH_New32(REG_ECC_PR(base), pr_read, pr_write, ecc);
	IOH_New32(REG_ECC_NPR(base), npr_read, npr_write, ecc);
}

static void
AT91Ecc_UnMap(void *owner, uint32_t base, uint32_t mask)
{
	IOH_Delete32(REG_ECC_CR(base));
	IOH_Delete32(REG_ECC_MR(base));
	IOH_Delete32(REG_ECC_SR(base));
	IOH_Delete32(REG_ECC_PR(base));
	IOH_Delete32(REG_ECC_NPR(base));
}

BusDevice *
AT91Ecc_New(const char *name)
{
	AT91Ecc *ecc = sg_new(AT91Ecc);
	ecc->name = strdup(name);
	ecc->bdev.first_mapping = NULL;
	ecc->bdev.Map = AT91Ecc_Map;
	ecc->bdev.UnMap = AT91Ecc_UnMap;
	ecc->bdev.owner = ecc;
	ecc->bdev.hw_flags = MEM_FLAG_WRITABLE | MEM_FLAG_READABLE;
	precalc_col_parity();
	return &ecc->bdev;
}
