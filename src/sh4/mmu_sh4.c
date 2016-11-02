/*
 *************************************************************************************************
 * SH4 Memory management unit 
 *
 * State: not working
 *
 * Used ST40 User manual Volume 3 13061.pdf  
 *
 * Copyright 2009 Jochen Karrer. All rights reserved.
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

#include <mmu_sh4.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include "sgstring.h"
#include "cpu_sh4.h"

#define REG_PTEH(base)	((base) + 0)
#define 	PTEH_VPN_MASK		(0xfffffc00);
#define		PTEH_VPN_SHIFT		(10)
#define		PTEH_ASID_MASK		(0xff)

#define REG_PTEL(base)	((base) + 4)
#define		PTEL_WT			(1 << 0)
#define		PTEL_SH			(1 << 1)
#define		PTEL_D			(1 << 2)
#define		PTEL_C			(1 << 3)
#define		PTEL_SZ			(1 << 4)
#define		PTEL_PR_MASK		(3 << 5)
#define		PTEL_PR_SHIFT		(5)
#define			PR_0		(0)
#define			PR_1		(1 << 5)
#define			PR_2		(2 << 5)
#define			PR_3		(3 << 5)
#define		PTEL_SZ2		(1 << 7)
#define		PTEL_V			(1 << 8)
#define		PTEL_PPN_MASK		(0x1ffffc00)
#define		PTEL_PPN_SHIFT		(10)

#define REG_TTB(base)	((base) + 0x8)
#define REG_TEA(base)	((base) + 0xc)
#define REG_MMUCR(base)	((base) + 0x10)
#define		MMUCR_AT		(1 << 0)
#define		MMUCR_TI		(1 << 2)
#define		MMUCR_SV		(1 << 8)
#define		MMUCR_SQMD		(1 << 9)
#define		MMUCR_URC_MASK		(0x3f << 10)
#define		MMUCR_URC_SHIFT		(10)
#define		MMUCR_URB_MASK		(0x3f << 18)
#define		MMUCR_URB_SHIFT		(18)
#define		MMUCR_LRUI_MASK		(0x3f << 26)
#define		MMUCR_LRUI_SHIFT	(26)

/* PTEA is in Renesas SH4 manual Section 3.2 */
#define REG_PTEA(base)	((base) + 0x34)
#define		PTEA_SA_MASK	(0x7)
#define		PTEA_TC		(1 << 3)

/* Flag Bits in stucture taken from memory mapped representation in 3.7.2 */
#define TLBE_SH(tlbe) ((tlbe)->flags & PTEL_SH)
#define TLBE_D(tlbe) ((tlbe)->flags & PTEL_D)
#define TLBE_PR(tlbe) ((tlbe)->flags & PTEL_PR_MASK)
#define ITLBE_PR(tlbe) ((tlbe)->flags & (1 << 6))

typedef struct SH4_TLBE {
	uint32_t va;
	uint32_t pa;
	uint32_t sz_mask;
	uint32_t addr_mask;
	uint32_t asid;
	uint32_t flags;
} SH4_TLBE;

typedef struct SH4MMU {
	BusDevice bdev;
	uint32_t reg_pteh;
	uint32_t reg_ptel;
	uint32_t reg_ptea;
	uint32_t reg_ttb;
	uint32_t reg_tea;
	uint32_t reg_mmucr;
	SH4_TLBE itlb_entry[4];
	SH4_TLBE utlb_entry[64];
} SH4MMU;

static SH4MMU g_sh4mmu;

/*
 ***********************************************************
 * Write a tlb entry to a location in ITLB 
 ***********************************************************
 */
static void
record_tlbe_itlb(SH4_TLBE * tlbe)
{
	SH4MMU *mmu = &g_sh4mmu;
	static int cnt;
	cnt++;
	mmu->itlb_entry[cnt & 3] = *tlbe;
}

bool
match_tlbe(SH4_TLBE * tlbe, uint32_t va, int check_asid)
{
	SH4MMU *mmu = &g_sh4mmu;
	if ((va & tlbe->addr_mask) == tlbe->va) {
		if (check_asid && (TLBE_SH(tlbe) == 0)) {
			if (tlbe->asid == (mmu->reg_pteh & PTEH_ASID_MASK)) {
				return true;
			}
		} else {
			return true;
		}
	}
	return false;
}

uint32_t
SH4_TranslateUtlb(uint32_t va, unsigned int op_write)
{
	SH4MMU *mmu = &g_sh4mmu;
	SH4_TLBE *tlbe;
	SH4_TLBE *hit;
	bool check_asid;
	uint32_t pr;
	uint32_t sr = SH4_GetSR();
	unsigned int i;
	int matchcount;
	if ((va >= 0x80000000) && (va < 0xc0000000)) {
		return va & 0x1fffffff;
	} else if (va > 0xe0000000) {
		if (sr & SR_MD) {
			return va;
		} else if (va < 0xe4000000) {
			return va;
		} else {
			/* address error exception */
			if (op_write) {
				SH4_Exception(EX_WADDERR);
				SH4_AbortInstruction();
			} else {
				SH4_Exception(EX_RADDERR);
				SH4_AbortInstruction();
			}
		}
	}
	if ((mmu->reg_mmucr & MMUCR_AT) == 0) {
		return va & 0x1fffffff;
	}
	exit(23);
	matchcount = 0;
	hit = NULL;
	check_asid = ((mmu->reg_mmucr & MMUCR_SV) == 0) || ((sr & SR_MD) == 0);
	for (i = 0; i < 63; i++) {
		tlbe = &mmu->utlb_entry[i];
		if (match_tlbe(tlbe, va, check_asid)) {
			hit = tlbe;
			matchcount++;
		}
	}
	if (matchcount == 0) {
		if (op_write) {
			SH4_Exception(EX_WTLBMISS);
			SH4_AbortInstruction();
		} else {
			SH4_Exception(EX_RTLBMISS);
			SH4_AbortInstruction();
		}
		return va;
	} else if (matchcount > 1) {
		/* Data TLB multiple hit exception */
		SH4_Exception(EX_DTLBMULTIHIT);
		SH4_AbortInstruction();
		return va;
	}
	pr = TLBE_PR(hit);
	if (sr & SR_MD) {
		/* Privileged */
		switch (pr) {
		    case PR_0:
		    case PR_2:
			    /* allow read  */
			    if (op_write) {
				    SH4_Exception(EX_WRITEPROT);
				    SH4_AbortInstruction();

			    }
			    break;
		    case PR_1:
		    case PR_3:
			    // allow read and write with initial page write check 
			    if (op_write) {
				    if (TLBE_D(hit) == 0) {
					    /* Initial pw exception */
					    SH4_Exception(EX_FIRSTWRITE);
					    SH4_AbortInstruction();
				    }
			    }
			    break;
		}
	} else {
		switch (pr) {
		    case PR_0:
		    case PR_1:
			    /* Nothing is allowed */
			    if (op_write) {
				    SH4_Exception(EX_WRITEPROT);
				    SH4_AbortInstruction();
			    } else {
				    SH4_Exception(EX_READPROT);
				    SH4_AbortInstruction();
			    }
			    break;
		    case PR_2:
			    /* Allow read */
			    if (op_write) {
				    SH4_Exception(EX_WRITEPROT);
				    SH4_AbortInstruction();
			    }
			    break;
		    case PR_3:
			    /* allow read + write with initial page write ex */
			    if (op_write) {
				    if (TLBE_D(hit) == 0) {
					    SH4_Exception(EX_FIRSTWRITE);
					    SH4_AbortInstruction();
				    }
			    }
			    break;
		}

	}
	return hit->pa | (va & hit->sz_mask);
}

uint32_t
SH4_TranslateItlb(uint32_t va)
{
	SH4MMU *mmu = &g_sh4mmu;
	SH4_TLBE *tlbe;
	SH4_TLBE *hit = NULL;
	uint32_t sr = SH4_GetSR();
	unsigned int i;
	int check_asid = 0;
	int matchcount = 0;
	if ((va >= 0x80000000) & (va < 0xc0000000)) {
		return va & 0x1fffffff;
	} else if (va > 0xE0000000) {
		/* I'm not sure if "Acess prohibited" means this: */
		SH4_Exception(EX_EXECPROT);
		SH4_AbortInstruction();
	} else if ((mmu->reg_mmucr & MMUCR_AT) == 0) {
		return va & 0x1fffffff;
	}
	if (((mmu->reg_mmucr & MMUCR_SV) == 0) || ((sr & SR_MD) == 0)) {
		check_asid = 1;
	}
	for (i = 0; i < 4; i++) {
		tlbe = &mmu->itlb_entry[i];
		if (match_tlbe(tlbe, va, check_asid)) {
			hit = tlbe;
			matchcount++;
		}
	}
	if (matchcount == 0) {
		for (i = 0; i < 64; i++) {
			tlbe = &mmu->utlb_entry[i];
			if (match_tlbe(tlbe, va, check_asid)) {
				hit = tlbe;
				matchcount++;
			}
		}
		if (matchcount > 1) {
			/* Yes a DTLBMULTIHIT, see 3.7.1 in st40_um vol 3 */
			SH4_Exception(EX_DTLBMULTIHIT);
			SH4_AbortInstruction();
		} else if (matchcount == 0) {
			SH4_Exception(EX_ITLBMISS);
			SH4_AbortInstruction();
			return 0;
		} else {
			record_tlbe_itlb(tlbe);
		}
	}
	if (matchcount > 1) {
		SH4_Exception(EX_ITLBMULTIHIT);
		SH4_AbortInstruction();
		return 0;
	}
	if (!(sr & SR_MD) && !(ITLBE_PR(hit))) {
		SH4_Exception(EX_EXECPROT);
		SH4_AbortInstruction();
	}
	return (hit->pa & hit->addr_mask) | (va & hit->sz_mask);
}

/*
 ****************************************************************************
 * The PTEH register contains VPN and ASID of the location where 
 * an MMU exception occured.
 ****************************************************************************
 */
static uint32_t
pteh_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "%s: register %s not implemented\n", __FILE__, __func__);
	return 0;
}

static void
pteh_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "%s: register %s not implemented\n", __FILE__, __func__);
}

/*
 ***********************************************************************************
 * The PTEL register contains the management information and the 
 * physical page number to be recorded in the UTLB with the LDTLB instruction.
 * 
 * Bit 0: 	PTEL_WT
 * Bit 1:	PTEL_SH
 * Bit 2:	PTEL_D
 * Bit 3:	PTEL_C
 * Bit 4: 	PTEL_SZ
 * Bit 5+6:	PTEL_PR_MASK
 * Bit 7:	PTEL_SZ2
 * Bit 8:	PTEL_V
 * Bit 10-28:	PTEL_PPN
 ***********************************************************************************
 */
static uint32_t
ptel_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "%s: register %s not implemented\n", __FILE__, __func__);
	return 0;
}

static void
ptel_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "%s: register %s not implemented\n", __FILE__, __func__);
}

/*
 *********************************************************************************
 * PTEA: Page table entry assistance register
 * Assistance bits for PCMCIA access.
 *********************************************************************************
 */
static uint32_t
ptea_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "%s: register %s not implemented\n", __FILE__, __func__);
	return 0;
}

static void
ptea_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "%s: register %s not implemented\n", __FILE__, __func__);
}

/**
 *********************************************************************************
 * \fn void ttb_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
 * Translation table base register. 
 ********************************************************************************
 */
static uint32_t
ttb_read(void *clientData, uint32_t address, int rqlen)
{
	SH4MMU *mmu = (SH4MMU *) clientData;
	return mmu->reg_ttb;
}

static void
ttb_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	SH4MMU *mmu = (SH4MMU *) clientData;
	mmu->reg_ttb = value;
}

/*
 **************************************************************************************
 * TLB exception effective address register. After an MMU exception or
 * address error exception the virtual address at which the exception occurred  
 * is stored here. 
 **************************************************************************************
 */
static uint32_t
tea_read(void *clientData, uint32_t address, int rqlen)
{
	SH4MMU *mmu = (SH4MMU *) clientData;
	return mmu->reg_tea;
}

static void
tea_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	SH4MMU *mmu = (SH4MMU *) clientData;
	mmu->reg_tea = value;
}

/*
 *****************************************************************************************
 * Bit 0:	MMUCR_AT 	Enable Address Translation.
 * Bit 1: 	MMUCR_TI 	TLB Invalidate all UTLB+ITLB. Reads 0 always.
 * Bit 8:	MMUCR_SV	Single virtual mode. 
 * Bit 9:	MMUCR_SQMD	Store queue mode bit. 0: User may access queues.
 * Bit 10-15:	MMUCR_URC	Random counter selects the UTLB entry to be replaced.
 * Bit 18-23:	MMUCR_URB	URC is reseted when equial URB (URB > 0 only) 
 * Bit 26-31:   MMUCR_LRUI	Least recently used bitfield for ITLB.
 *****************************************************************************************
 */
static uint32_t
mmucr_read(void *clientData, uint32_t address, int rqlen)
{
	SH4MMU *mmu = (SH4MMU *) clientData;
	return mmu->reg_mmucr;
}

static void
mmucr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	SH4MMU *mmu = (SH4MMU *) clientData;
	mmu->reg_mmucr = value;
}

static void
MMU_Map(void *owner, uint32_t base, uint32_t mask, uint32_t flags)
{
	SH4MMU *mmu = (SH4MMU *) owner;
	IOH_New32(REG_PTEH(base), pteh_read, pteh_write, mmu);
	IOH_New32(REG_PTEL(base), ptel_read, ptel_write, mmu);
	IOH_New32(REG_PTEA(base), ptea_read, ptea_write, mmu);
	IOH_New32(REG_TTB(base), ttb_read, ttb_write, mmu);
	IOH_New32(REG_TEA(base), tea_read, tea_write, mmu);
	IOH_New32(REG_MMUCR(base), mmucr_read, mmucr_write, mmu);
}

static void
MMU_UnMap(void *owner, uint32_t base, uint32_t mask)
{
	IOH_Delete32(REG_PTEH(base));
	IOH_Delete32(REG_PTEL(base));
	IOH_Delete32(REG_PTEA(base));
	IOH_Delete32(REG_TTB(base));
	IOH_Delete32(REG_TEA(base));
	IOH_Delete32(REG_MMUCR(base));
}

BusDevice *
SH4MMU_New(const char *name)
{
	SH4MMU *mmu = &g_sh4mmu;	/* sg_new(SH4MMU);      */
	mmu->bdev.first_mapping = NULL;
	mmu->bdev.Map = MMU_Map;
	mmu->bdev.UnMap = MMU_UnMap;
	mmu->bdev.owner = mmu;
	mmu->bdev.hw_flags = MEM_FLAG_WRITABLE | MEM_FLAG_READABLE;
	fprintf(stderr, "Created SH4 Memory management unit \"%s\"\n", name);
	return &mmu->bdev;
}
