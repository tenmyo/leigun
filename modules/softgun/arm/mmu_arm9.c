/*
 *************************************************************************************************
 *
 * Emulation of the ARM Coprocessor cp15 
 * which is responsible for Memory Management
 *
 * Copyright 2004 2006 Jochen Karrer. All rights reserved.
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
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/time.h>
#include <sys/types.h>

#include "arm9cpu.h"
#include "cycletimer.h"
#include "mmu_arm9.h"
#include "bus.h"
#include "compiler_extensions.h"
#include "sgstring.h"

#include "byteorder.h"

typedef void McrProc(void *clientData, uint32_t icode, uint32_t value);
typedef uint32_t MrcProc(void *clientData, uint32_t icode);

typedef struct SystemCopro {
	uint32_t id;
	uint32_t ctrl;
	uint32_t mtbase;
	uint32_t mdac;
	uint32_t dfsr;
	uint32_t ifsr;
	uint32_t far;
	uint32_t mcctrl;
	uint32_t mtlbctrl;
	uint32_t dclck;		// DCache lock
	uint32_t iclck;		// ICache lock
	uint32_t mtlblck;
	uint32_t mpid;

	int64_t saved_cycles;	/* Halt Instruction */
	CycleCounter_t last_halt_cycles;
	struct timespec last_halt;
	SigNode *endianNode;
	int debugmode;

	/* The registers */
	McrProc *mcrProc[16];
	MrcProc *mrcProc[16];
	void *crn_clientData[16];

} SystemCopro;

static uint32_t translation_enabled = 0;
static uint32_t ttbl_base = 0;
static uint32_t sys_rom_protection = 0;
static uint32_t domain_access_reg;
static SystemCopro *gmmu;

#ifdef DEBUG
#define dbgprintf(...) { if(unlikely(debugflags & DEBUG_MMU)) { fprintf(stderr,__VA_ARGS__); } }
#else
#define dbgprintf(...)
#endif

#define MMU_FAR  (gmmu->far)
#define MMU_IFSR  (gmmu->ifsr)
#define MMU_DFSR  (gmmu->dfsr)
#define MMU_DEBUGMODE (gmmu->debugmode)

#define FSR_FS_MASK (0xf)
#define FSR_FS_DOMAIN_MASK (0xf0)
/*
 * ------------------------------------------------------------------
 * Terminal exception is for incorrectable errors
 * It is implementation defined which means that there is 
 * no documentation. NS9750 seems to set it on unaligned str
 * ------------------------------------------------------------------
 */
#define FS_TERMINAL_EX 	(2)
#define FS_VECTOR_EX	(0)
#define FS_ALIGN_EX	(1)
#define FS_TRANS_SECTION (5)
#define FS_TRANS_PAGE	 (7)
#define FS_PERM_SECTION (0xd)
#define FS_PERM_PAGE	(0xf)

#define SYS_ROM_SHIFT 2
#define AP_SHIFT 0
#define USER_SHIFT 4
#define WRITE_SHIFT 5

/*
 * -----------------------------------------------------------------
 * If an access is permitted or not is stored in a table which
 * is indexed by a value calculated from 
 * the Access permissions from the Pagetable-Entry,
 * the ROM-Mode Register of the MMU and the Current effective mode
 * Bit (in most cases from the CPSR), and Access type (Read, Write) 
 * -----------------------------------------------------------------
 */
static uint8_t permissionTable[64] = {
	// Privileged Mode, Read, Rom=0, System = 0     
	0, 1, 1, 1,
	// System = 1 
	1, 1, 1, 1,
	// Rom=1, System=0
	1, 1, 1, 1,
	// Rom=1, System=1 
	1, 1, 1, 1,
	// User Mode, Rom=0, System = 0
	0, 0, 1, 1,
	// System = 1
	0, 0, 1, 1,
	// Rom=1, System = 0
	1, 0, 1, 1,
	// Rom=1,System = 1
	0, 0, 1, 1,

	// Write
	// Privileged Mode, Write, Rom=0, System = 0    
	0, 1, 1, 1,
	// System = 1
	0, 1, 1, 1,
	// Rom = 1 System = 0
	0, 1, 1, 1,
	// Rom = 1 System = 1 
	0, 1, 1, 1,
	// User Mode, Write, Rom=0 System=0
	0, 0, 0, 1,
	// System = 1   
	0, 0, 0, 1,
	// Rom=1 System = 0
	0, 0, 0, 1,
	// System=1, Rom = 1
	0, 0, 0, 1
};

uint32_t mmu_word_addr_xor;
uint32_t mmu_byte_addr_xor;

void
MMU_SetDebugMode(int val)
{
	gmmu->debugmode = val;
}

int
MMU_Byteorder()
{
	if (gmmu->ctrl & MCTRL_BE) {
		return BYTE_ORDER_BIG;
	} else {
		return BYTE_ORDER_LITTLE;
	}
}

/*
 * ------------------------------------------------------
 * update_byteorder 
 *	Called whenever bytorder changes and on startup
 * ------------------------------------------------------
 */
static void
update_byteorder(SystemCopro * mmu)
{
	if (mmu->ctrl & MCTRL_BE) {
		fprintf(stderr, "MMU: Byteorder is now BE\n");
		mmu_byte_addr_xor = 0x3;
		mmu_word_addr_xor = 0x2;
		SigNode_Set(mmu->endianNode, SIG_HIGH);
		fprintf(stderr, "pc %08x\n", ARM_GET_NNIA);
	} else {
		fprintf(stderr, "MMU: Byteorder is now LE\n");
		mmu_byte_addr_xor = 0;
		mmu_word_addr_xor = 0;
		SigNode_Set(mmu->endianNode, SIG_LOW);
	}
}

/*
 * -----------------------------------------------------------
 * Calculate the index in the permission table
 * from the current mode, the access type and the mmu setting
 * -----------------------------------------------------------
 */
static inline int
calculate_perm_index(uint32_t ap, uint32_t access_type)
{
	uint32_t perm_index = ap | sys_rom_protection | (access_type & MMU_ACCESS_DATA_WRITE);
	if ((ARM_SIGNALING_MODE) == MODE_USER) {
		perm_index |= (1 << USER_SHIFT);
	}
	return perm_index;
}

static inline uint8_t
check_permission(uint32_t ap, uint32_t access_type, uint32_t domain_shift)
{
	uint32_t perm_index = calculate_perm_index(ap, access_type);
	uint8_t domain_access = (domain_access_reg >> domain_shift) & 3;
	if (unlikely(domain_access == 0)) {
		return 0;
	}
	if (unlikely(!permissionTable[perm_index])) {
		//fprintf(stderr,"dacreg %08x dac %02x pc %08x , ap %08x, atype %04x, srpr %08x, cpumode %02x, index %d\n",
		//      domain_access_reg,domain_access,REG_PC,ap,access_type,sys_rom_protection,REG_CPSR&0x1f, perm_index );
		if (likely(domain_access != 3)) {
			return 0;
		} else {
			return 1;
		}
	}
	return 1;
}

/*
 * -----------------------------------------
 * Page Translation faults
 * -----------------------------------------
 */
static inline void
MMU_translation_fault_section(uint32_t access_type, uint32_t far)
{
	//fprintf(stderr,"fault section %08x ptb %08x,atype %d pc: %08x\n",far,ttbl_base,access_type,REG_PC);
	if (likely(!MMU_DEBUGMODE)) {
		if (access_type & MMU_ACCESS_IFETCH) {
			MMU_IFSR = (MMU_IFSR & ~FSR_FS_MASK) | FS_TRANS_SECTION;
			ARM_Exception(EX_PABT, 4);
		} else {
			MMU_FAR = far;
			MMU_DFSR = (MMU_DFSR & ~FSR_FS_MASK) | FS_TRANS_SECTION;
			ARM_Exception(EX_DABT, 4);
		}
	}
	longjmp(gcpu.abort_jump, 1);
}

static inline void
MMU_translation_fault_page(uint32_t access_type, uint32_t far, uint32_t domain)
{
	//fprintf(stderr,"fault page %08x ptb %08x,atype %d pc: %08x\n",far,ttbl_base,access_type,REG_PC);
	if (likely(!MMU_DEBUGMODE)) {
		if (access_type & MMU_ACCESS_IFETCH) {
			MMU_IFSR =
			    (MMU_IFSR & ~(FSR_FS_MASK | FSR_FS_DOMAIN_MASK)) | FS_TRANS_PAGE |
			    (domain << 4);
			ARM_Exception(EX_PABT, 4);
		} else {
			MMU_FAR = far;
			MMU_DFSR =
			    (MMU_DFSR & ~(FSR_FS_MASK | FSR_FS_DOMAIN_MASK)) | FS_TRANS_PAGE |
			    (domain << 4);
			ARM_Exception(EX_DABT, 4);
		}
	}
	longjmp(gcpu.abort_jump, 1);
}

/*
 * ------------------------------------------------------
 * Access Permission violation faults
 * ------------------------------------------------------
 */
static inline void
MMU_permission_fault_page(uint32_t access_type, uint32_t far, uint32_t domain)
{
//      dbgprintf("Permission fault page at %04x\n",access_type);
	if (likely(!MMU_DEBUGMODE)) {
		if (access_type & MMU_ACCESS_IFETCH) {
			MMU_IFSR =
			    (MMU_IFSR & ~(FSR_FS_MASK | FSR_FS_DOMAIN_MASK)) | FS_PERM_PAGE |
			    (domain << 4);
			ARM_Exception(EX_PABT, 4);
		} else {
			MMU_FAR = far;
			MMU_DFSR =
			    (MMU_DFSR & ~(FSR_FS_MASK | FSR_FS_DOMAIN_MASK)) | FS_PERM_PAGE |
			    (domain << 4);
			ARM_Exception(EX_DABT, 4);
		}
	}
	longjmp(gcpu.abort_jump, 1);
}

static inline void
MMU_permission_fault_section(uint32_t access_type, uint32_t far, uint32_t domain)
{
//      dbgprintf("fault section %08x ptb %08x,atype %d pc: %08x\n",far,ttbl_base,access_type,REG_PC);
//      dbgprintf("Permission fault section at %04x\n",access_type);
	if (likely(!MMU_DEBUGMODE)) {
		if (access_type & MMU_ACCESS_IFETCH) {
			MMU_IFSR =
			    (MMU_IFSR & ~(FSR_FS_MASK | FSR_FS_DOMAIN_MASK)) | FS_PERM_SECTION |
			    (domain << 4);
			ARM_Exception(EX_PABT, 4);
		} else {
			MMU_FAR = far;
			MMU_DFSR =
			    (MMU_DFSR & ~(FSR_FS_MASK | FSR_FS_DOMAIN_MASK)) | FS_PERM_SECTION |
			    (domain << 4);
			ARM_Exception(EX_DABT, 4);
		}
	}
	longjmp(gcpu.abort_jump, 1);
}

/*
 * ------------------------------------------------------
 * Alignment Exceptions
 *	returns only if alignment exceptions are
 *	disabled. else it causes a data abort
 *	with Fault Status register set to  
 *      Alignment Exception
 * ------------------------------------------------------
 */
void
MMU_AlignmentException(uint32_t far)
{
	if (!do_alignment_check)
		return;
	MMU_FAR = far;
	MMU_DFSR = (MMU_DFSR & ~(FSR_FS_MASK)) | FS_ALIGN_EX;
	ARM_Exception(EX_DABT, 4);
	longjmp(gcpu.abort_jump, 1);
}

#define FLPD_TYPE_FAULT   (0)
#define FLPD_TYPE_COARSE  (1)
#define FLPD_TYPE_SECTION (2)
#define FLPD_TYPE_FINE	  (3)

#define SLPD_TYPE_FAULT  (0)
#define SLPD_TYPE_LARGE  (1)
#define SLPD_TYPE_SMALL  (2)
#define SLPD_TYPE_TINY   (3)

/*
 * ----------------------------------
 * Dump all registers of CPU
 * ----------------------------------
 */
__UNUSED__ static void
dump_regs()
{
	int i;
	for (i = 0; i < 16; i++) {
		uint32_t value;
		value = ARM9_ReadReg(i);
		fprintf(stderr, "R%02d: %08x    ", i, value);
		if ((i & 3) == 3) {
			fprintf(stderr, "\n");
		}
	}
	fprintf(stderr, "CPSR: %08x \n", REG_CPSR);
	fflush(stderr);
}

/*
 * ----------------------------------------
 * Dump a coarse Pagetable
 * ----------------------------------------
 */
static void
coarse_dump(SystemCopro * mmu, FILE * file, uint32_t pgt_base, uint32_t virtual)
{
	uint32_t slpd, slpdP = pgt_base;
	uint32_t index;
	for (index = 0; index < 256; index++, slpdP += 4) {
		slpd = Bus_FastRead32(slpdP);
		switch (slpd & 3) {
		    case SLPD_TYPE_FAULT:
			    break;
		    case SLPD_TYPE_LARGE:
			    fprintf(file, "  %08x large at %08x\n", virtual | (index << 12),
				    slpd & 0xffff0000);
			    break;
		    case SLPD_TYPE_SMALL:
			    fprintf(file, "  %08x small at %08x aps %02x\n",
				    virtual | (index << 12), slpd & 0xfffff000,
				    (slpd & 0xff0) >> 4);
			    break;
		    case SLPD_TYPE_TINY:
			    fprintf(file, "  %08x tiny at %08x\n", virtual | (index << 12),
				    slpd & 0xfffffc00);
			    break;
		}
	}
}

static int pgtable_nr = 0;

static void
dump_mctrl(SystemCopro * mmu, FILE * file)
{
	uint32_t data = mmu->ctrl;
	fprintf(file, "MCTRL: %08x ", mmu->ctrl);
	if (data & (1 << 2)) {
		fprintf(file, "DCache ");
	}
	if (data & (1 << 3)) {
		fprintf(file, "WBUF ");
	}
	if (data & (1 << 12)) {
		fprintf(file, "ICache ");
	}
	fprintf(file, "\n");
}

/*
 * ------------------------------------
 * Dump complete Pagetable
 * ------------------------------------
 */
__UNUSED__ static void
pgtables_dump(SystemCopro * mmu)
{
	char filename[50];
	FILE *file;

	uint32_t flpdP = ttbl_base;
	uint32_t flpd;
	uint32_t i;
	uint32_t pgt_base;
	sprintf(filename, "mmu_dump%d", pgtable_nr);
	file = fopen(filename, "w+");
	if (!file) {
		fprintf(stderr, "can't open mmu dump file\n");
		return;
	}
	fprintf(file, "MDAC: %08x\n", mmu->mdac);
	dump_mctrl(mmu, file);
	for (i = 0; i < 4096; i++, flpdP += 4) {
		flpd = Bus_FastRead32(flpdP);
		switch (flpd & 3) {
		    case FLPD_TYPE_FAULT:
			    break;

		    case FLPD_TYPE_COARSE:
			    pgt_base = flpd & 0xfffffc00;
			    fprintf(file, "%03x: coarse at %08x dom %02x\n", i, pgt_base,
				    flpd & 0x1e0 >> 5);
			    coarse_dump(mmu, file, pgt_base, i << 20);
			    break;

		    case FLPD_TYPE_SECTION:
			    fprintf(file, "%03x: section at %08x\n", i, flpd & 0xfff00000);
			    break;

		    case FLPD_TYPE_FINE:
			    fprintf(file, "%03x: fine at %08x\n", i, flpd & 0xfff00000);
			    break;
		}
	}
	fclose(file);
}

/*
 * ------------------------------------
 * Check if cacheable and bufferable
 * flags are set correctly
 * ------------------------------------
 */
static inline void
check_cachebuff(uint32_t pgd, uint32_t vaddr, uint32_t access_type)
{
#ifdef DEBUG
	if ((debugflags & DEBUG_MMU_CB) && ((pgd & 0xc) != 0xc)) {
		fprintf(stderr, "page not cache and bufferable at %08x atype %08x\n",
			ARM_GET_NNIA - 8, access_type);
	}
#endif
}

/*
 * -------------------------------------------
 * Static inline version of Page table walk 
 * -------------------------------------------
 */
uint32_t
MMU9_TranslateAddress(uint32_t addr, uint32_t access_type)
{
	uint32_t domain_shift;
	uint32_t taddr;		// translated address
	uint32_t flpdP;
	uint32_t flpd;
	if (unlikely(!translation_enabled)) {
		return addr;
	}
	flpdP = ttbl_base | ((addr & 0xfff00000) >> (20 - 2));
	flpd = Bus_FastRead32(flpdP);
	switch (flpd & 3) {
	    case FLPD_TYPE_FAULT:
		    MMU_translation_fault_section(access_type, addr);
		    exit(2);

	    case FLPD_TYPE_COARSE:
		    {
			    uint32_t pgt_base = flpd & 0xfffffc00;
			    uint32_t sl_table_index = (addr & 0x000ff000) >> (12 - 2);
			    uint32_t slpd, slpdP;
			    uint32_t ap;
			    slpdP = sl_table_index | pgt_base;
			    slpd = Bus_FastRead32(slpdP);
			    check_cachebuff(slpd, addr, access_type);
			    switch (slpd & 3) {
				case SLPD_TYPE_FAULT:
					MMU_translation_fault_page(access_type, addr,
								   (flpd >> 5) & 0xf);
					exit(123);

					/* Large Page 64k */
				case SLPD_TYPE_LARGE:
					domain_shift = (flpd >> 4) & 0x1e;
					ap = (slpd >> (4 + ((addr & 0xc000) >> 13))) & 3;
					if (unlikely
					    (!check_permission(ap, access_type, domain_shift))) {
						dbgprintf("Large Coarse Page Permission fault\n");
						MMU_permission_fault_page(access_type, addr,
									  (flpd >> 5) & 0xf);
					}
					taddr = (slpd & 0xffff0000) | (addr & 0xffff);
					return taddr;

					/* small page 4k */
				case SLPD_TYPE_SMALL:
					domain_shift = (flpd >> 4) & 0x1e;
					ap = (slpd >> (4 + ((addr & 0xc00) >> 9))) & 3;
					if (unlikely
					    (!check_permission(ap, access_type, domain_shift))) {
						MMU_permission_fault_page(access_type, addr,
									  (flpd >> 5) & 0xf);
					}
					taddr = (slpd & 0xfffff000) | (addr & 0xfff);
					return taddr;

				case SLPD_TYPE_TINY:
					fprintf(stderr,
						"Corrupt page table: Tiny Pages in Coarse Pagetable\n");
					exit(749);
			    }
		    }
	    case FLPD_TYPE_SECTION:
		    {
			    uint32_t ap = (flpd >> 10) & 3;
			    uint32_t sec_index = addr & 0x000fffff;
			    domain_shift = (flpd >> 4) & 0x1e;
			    check_cachebuff(flpd, addr, access_type);
			    if (unlikely(!check_permission(ap, access_type, domain_shift))) {
				    MMU_permission_fault_section(access_type, addr,
								 (flpd >> 5) & 0xf);
			    }
			    taddr = sec_index | (flpd & 0xfff00000);
			    return taddr;
		    }
	    case FLPD_TYPE_FINE:
		    {
			    uint32_t pgt_base = flpd & 0xfffff000;
			    uint32_t sl_table_index = (addr & 0x000ffc00) >> (9 - 2);
			    uint32_t slpd, slpdP;
			    uint32_t ap;
			    slpdP = sl_table_index | pgt_base;
			    slpd = Bus_FastRead32(slpdP);
			    check_cachebuff(slpd, addr, access_type);
			    switch (slpd & 3) {
				case SLPD_TYPE_FAULT:
					MMU_translation_fault_page(access_type, addr,
								   (flpd >> 5) & 0xf);
					exit(123);

				case SLPD_TYPE_LARGE:
					/* large page 64k */
					domain_shift = (flpd >> 4) & 0x1e;
					ap = (slpd >> (4 + ((addr & 0xc000) >> 13))) & 3;
					if (unlikely
					    (!check_permission(ap, access_type, domain_shift))) {
						MMU_permission_fault_page(access_type, addr,
									  (flpd >> 5) & 0xf);
					}
					taddr = (slpd & 0xffff0000) | (addr & 0xffff);
					return taddr;

				case SLPD_TYPE_SMALL:
					/* small page 4k */
					domain_shift = (flpd >> 4) & 0x1e;
					ap = (slpd >> (4 + ((addr & 0xc00) >> 9))) & 3;
					if (unlikely
					    (!check_permission(ap, access_type, domain_shift))) {
						MMU_permission_fault_page(access_type, addr,
									  (flpd >> 5) & 0xf);
					}
					taddr = (slpd & 0xfffff000) | (addr & 0xfff);
					return taddr;

				case SLPD_TYPE_TINY:
					/* tiny page 1k */
					domain_shift = (flpd >> 4) & 0x1e;
					ap = (slpd >> (4)) & 3;
					if (unlikely
					    (!check_permission(ap, access_type, domain_shift))) {
						MMU_permission_fault_page(access_type, addr,
									  (flpd >> 5) & 0xf);
					}
					taddr = (slpd & 0xfffffc00) | (addr & 0x3ff);
					return taddr;
			    }

		    }
		    fprintf(stderr, "Fine page tables not tested\n");
	    default:
		    fprintf(stderr, "Unreachable code\n");
		    break;
	}
	exit(376);
}

#if 0
uint32_t
MMU9_TranslateAddress(uint32_t addr, uint32_t access_type)
{
	uint32_t taddr = _MMU9_TranslateAddress(addr, access_type);
	return taddr;
}
#endif

static void
CrnHandler_New(SystemCopro * mmu, int crn, MrcProc * rproc, McrProc * wproc, void *cd)
{
	crn = crn & 0xf;
	mmu->mcrProc[crn] = wproc;
	mmu->mrcProc[crn] = rproc;
	mmu->crn_clientData[crn] = cd;
}

static void
id_write(void *clientData, uint32_t icode, uint32_t value)
{
	fprintf(stderr, "MMU:  CPU-ID is not writable\n");
}

static uint32_t
arm920t_id_read(void *clientData, uint32_t icode)
{
	uint32_t crm = icode & 0xf;
	uint32_t opcode_2 = (icode >> 5) & 0x7;
	if (crm != 0) {
		fprintf(stderr, "MMU: CRM is not zero(%d) when reading ID\n", crm);
		return 0;
	}
	if (opcode_2 == 1) {
		/* 16/16 kB Harvard Instruction/Data Cache */
		return 0x0d172172;
	} else if (opcode_2 == 2) {
		return 0;	// TCM not yet ?
	}
	return 0x41129200 | 0;

}

static uint32_t
arm926ejs_id_read(void *clientData, uint32_t icode)
{
	uint32_t crm = icode & 0xf;
	uint32_t opcode_2 = (icode >> 5) & 0x7;
	if (crm != 0) {
		fprintf(stderr, "MMU: CRM is not zero(%d) when reading ID\n", crm);
		return 0;
	}
	if (opcode_2 == 1) {
		/* 8/4 kB Harvard Instruction/Data Cache */
		return 0x1d0d2112;
	} else if (opcode_2 == 2) {
		return 0;	// TCM not yet ?
	}
	return 0x41069260 | 4;	// CPU revision 4

}

static uint32_t
ctrl_read(void *clientData, uint32_t icode)
{
	SystemCopro *mmu = (SystemCopro *) clientData;
	uint32_t crm = icode & 0xf;
	if (crm != 0) {
		fprintf(stderr, "MMU: CRM is not zero(%d) when reading CTRL\n", crm);
		return 0;
	}
	return (mmu->ctrl & ~0xffff0c78UL) | 0x00050078;
}

static void
ctrl_write(void *clientData, uint32_t icode, uint32_t value)
{
	SystemCopro *mmu = (SystemCopro *) clientData;
	uint32_t crm = icode & 0xf;
	uint32_t diff;
	dbgprintf("Setting MMU Control Register to %08x\n", value);
	if (crm != 0) {
		fprintf(stderr, "MMU: CRM is not zero(%d) when writing CTRL\n", crm);
		return;
	}
	/* Bit 3 is write buffer enable */
	diff = mmu->ctrl ^ value;
	mmu->ctrl = value;
	if (diff & MCTRL_BE) {
		update_byteorder(mmu);
	}
	translation_enabled = value & MCTRL_MMUEN;
	if (value & MCTRL_V) {
		mmu_vector_base = 0xffff0000;
	} else {
		mmu_vector_base = 0;
	}
	do_alignment_check = value & MCTRL_ALGNCHK;
	sys_rom_protection = ((value >> 8) & 0x3) << SYS_ROM_SHIFT;
	if (diff & (3 << 8)) {
		MMU_InvalidateTlb();
	}
#if 0
	if (value & MCTRL_LABT) {
		fprintf(stderr, "Late abort model\n");
	} else {
		fprintf(stderr, "Early abort model\n");
	}
#endif
	return;

}

static uint32_t
mtbase_read(void *clientData, uint32_t icode)
{
	SystemCopro *mmu = (SystemCopro *) clientData;
	uint32_t crm = icode & 0xf;
	if (crm == 0) {
		return mmu->mtbase;
	} else {
		fprintf(stderr, "mrc MTBASE with unknown crm %08x\n", crm);
		return 0;
	}
}

static void
mtbase_write(void *clientData, uint32_t icode, uint32_t value)
{
	SystemCopro *mmu = (SystemCopro *) clientData;
	mmu->mtbase = value;
	ttbl_base = value;
	dbgprintf("Setting Page table base to %08x\n", value);
	if (value & 0x3fff) {
		fprintf(stderr, "Bad page table base 0x%08x\n", value);
	}
	MMU_InvalidateTlb();
	return;
}

static uint32_t
mdac_read(void *clientData, uint32_t icode)
{
	SystemCopro *mmu = (SystemCopro *) clientData;
	return mmu->mdac;
}

static void
mdac_write(void *clientData, uint32_t icode, uint32_t value)
{
	SystemCopro *mmu = (SystemCopro *) clientData;
	uint32_t diff;
	dbgprintf("Load Domain Access Control Register with %08x\n", value);
	diff = mmu->mdac ^ value;
	mmu->mdac = value;
	domain_access_reg = value;
	if (diff) {
		MMU_InvalidateTlb();
	}
	return;

}

static uint32_t
mfstat_read(void *clientData, uint32_t icode)
{
	SystemCopro *mmu = (SystemCopro *) clientData;
	uint32_t crm = icode & 0xf;
	uint32_t opcode_2 = (icode >> 5) & 0x7;
	if (crm == 0) {
		if (opcode_2 == 0) {
			return mmu->dfsr;
		} else if (opcode_2 == 1) {
			return mmu->ifsr;
		} else {
			fprintf(stderr, "FSR read with unknown opcode_2 %08x\n", opcode_2);
			return 0;
		}
	} else {
		fprintf(stderr, "mrc MFSTAT with unknown crm %08x\n", crm);
		exit(1);
	}
}

static void
mfstat_write(void *clientData, uint32_t icode, uint32_t value)
{
	SystemCopro *mmu = (SystemCopro *) clientData;
	uint32_t crm = icode & 0xf;
	uint32_t opcode_2 = (icode >> 5) & 0x7;
	if (crm != 0) {
		fprintf(stderr, "write fsr with illegal crm %d\n", crm);
		return;
	}
	if (opcode_2 == 0) {
		mmu->dfsr = value;
	} else if (opcode_2 == 1) {
		mmu->ifsr = value;
	} else {
		fprintf(stderr, "illegal opcode_2 %d to MMU FSR\n", opcode_2);
	}

}

static uint32_t
mfaddr_read(void *clientData, uint32_t icode)
{
	SystemCopro *mmu = (SystemCopro *) clientData;
	return mmu->far;
}

static void
mfaddr_write(void *clientData, uint32_t icode, uint32_t value)
{
	SystemCopro *mmu = (SystemCopro *) clientData;
	uint32_t opcode_2 = (icode >> 5) & 0x7;
	if (opcode_2 == 0) {
		mmu->far = value;
	} else {
		fprintf(stderr, "illegal opcode_2 %d to MMU FAR\n", opcode_2);
	}
}

static uint32_t
mcctrl_read(void *clientData, uint32_t icode)
{
	return FLAG_Z;		// ????  
}

static void
mcctrl_write(void *clientData, uint32_t icode, uint32_t value)
{
	SystemCopro *mmu = (SystemCopro *) clientData;
	uint32_t crm = icode & 0xf;
	uint32_t opcode_2 = (icode >> 5) & 0x7;
	/* Wait for Interrupt instruction (halt) */
	if ((crm == 0) && (opcode_2 == 4)) {
		uint64_t nsecs;
		struct timespec *last_halt, tv_now;
		last_halt = &mmu->last_halt;
		clock_gettime(CLOCK_MONOTONIC, &tv_now);
		nsecs =
		    tv_now.tv_nsec - last_halt->tv_nsec + (int64_t) 1000000000 *(tv_now.tv_sec -
										 last_halt->tv_sec);
		mmu->saved_cycles = mmu->saved_cycles
		    /* Subtract the  cycles which should have simulated since the last halt */
		    - (CycleTimerRate_Get() / 1000000) * (nsecs / 1000) +
		    /* Add the cycles which really have been simulated since last halt */
		    (CycleCounter_Get() - mmu->last_halt_cycles);

		*last_halt = tv_now;

		/* 
		 * Catch up a maximum of one half of a second if emulator is 
		 * slower than real system 
		 */
		if (mmu->saved_cycles < -(int64_t) (CycleTimerRate_Get() / 2)) {
			mmu->saved_cycles = -(int64_t) (CycleTimerRate_Get() >> 1);
		} else if (mmu->saved_cycles > (int64_t) CycleTimerRate_Get()) {
			mmu->saved_cycles = CycleTimerRate_Get();
		}
		/* Jump over CPU cycles if not enough saved for sleeping 1 timeslice */
		while (mmu->saved_cycles < (CycleTimerRate_Get() / 100)
		       && (firstCycleTimerTimeout != ~(uint64_t) 0)) {

			/* leave halt even if an IRQ arrives when irqs are disabled in CPSR */
			if (gcpu.signals_raw & (ARM_SIG_IRQ | ARM_SIG_FIQ)) {
				mmu->last_halt_cycles = CycleCounter_Get();
				return;
			}
			mmu->saved_cycles += firstCycleTimerTimeout - CycleCounter_Get();
			CycleCounter = firstCycleTimerTimeout;
			CycleTimers_Check();
		}
		mmu->last_halt_cycles = CycleCounter_Get();
		/* Jump over time (sleep) if we have saved enough CPU cycles for sleeping 1 timeslice */
		while (mmu->saved_cycles >= (CycleTimerRate_Get() / 100)) {
			struct timespec tout;
			tout.tv_nsec = 10000000;	/* 10 ms */
			tout.tv_sec = 0;
			if (gcpu.signals_raw & (ARM_SIG_IRQ | ARM_SIG_FIQ)) {
				return;
			}
			// FIXME: FIO_WaitEventTimeout(&tout);
			clock_gettime(CLOCK_MONOTONIC, &tv_now);
			nsecs =
			    tv_now.tv_nsec - last_halt->tv_nsec +
			    (int64_t) 1000000000 *(tv_now.tv_sec - last_halt->tv_sec);
			mmu->saved_cycles -= (CycleTimerRate_Get() / 1000000) * (nsecs / 1000);
			*last_halt = tv_now;
		}
	} else {
		dbgprintf("Ignore Cache settings\n");
	}
	return;
}

static uint32_t
mtlbctrl_read(void *clientData, uint32_t icode)
{
	fprintf(stderr, "MMU: mtlbctrl register read not implemented\n");
	return 0;

}

static void
mtlbctrl_write(void *clientData, uint32_t icode, uint32_t value)
{
	//fprintf(stderr,"inv op2 %08x crm %08x,data %08x\n",opcode_2,crm,data);
	MMU_InvalidateTlb();
	dbgprintf("Invalidate TLB\n");
	return;

}

/*
 * ---------------------------------------------------
 * Cache lockdown register
 * ---------------------------------------------------
 */
static uint32_t
mclck_read(void *clientData, uint32_t icode)
{
	SystemCopro *mmu = (SystemCopro *) clientData;
	uint32_t crm = icode & 0xf;
	uint32_t opcode_2 = (icode >> 5) & 0x7;
	if (crm == 0) {
		/* Read Cache lockdown registers */
		if (opcode_2 == 0) {
			return mmu->dclck;
		} else if (opcode_2 == 1) {
			return mmu->iclck;
		} else {
			fprintf(stderr, "illegal opcode_2 %d to MMU Cache lockdown\n", opcode_2);
			return 0;
		}
	} else if (crm == 1) {
		fprintf(stderr, "MMU TCM not supported\n");
		return 0;
	} else {
		fprintf(stderr, "MMU MCLCK: Illegal value %d for crm\n", crm);
		return 0;
	}
}

static void
mclck_write(void *clientData, uint32_t icode, uint32_t value)
{
	SystemCopro *mmu = (SystemCopro *) clientData;
	uint32_t crm = icode & 0xf;
	uint32_t opcode_2 = (icode >> 5) & 0x7;
	if (crm == 0) {
		if ((value & 0xfffffff0) != 0x0000fff0) {
			fprintf(stderr, "MMU: illegal value for Cache Lockdown %08x\n", value);
		}
		if (opcode_2 == 0) {
			mmu->dclck = value;
		} else if (opcode_2 == 1) {
			mmu->iclck = value;
		} else {
			fprintf(stderr, "illegal opcode_2 %d to MMU Cache lockdown\n", opcode_2);
		}
	} else if (crm == 1) {
		fprintf(stderr, "MMU TCM not supported\n");
		return;
	} else {
		fprintf(stderr, "MMU MCLCK: Illegal value %d for crm\n", crm);
		return;
	}
}

static uint32_t
mtlblck_read(void *clientData, uint32_t icode)
{
	fprintf(stderr, "TLB locking not emulated\n");
	return 0;
}

static void
mtlblck_write(void *clientData, uint32_t icode, uint32_t value)
{
	fprintf(stderr, "TLB locking not emulated\n");
}

static uint32_t
mpid_read(void *clientData, uint32_t icode)
{
	return 0;
}

static void
mpid_write(void *clientData, uint32_t icode, uint32_t value)
{
	fprintf(stderr, "MMU: MVA addressing mode PID not implemented\n");
}

/* 
 * ---------------------------------------------
 * MMU MRC - Move from ArmCoprocessor to register
 * ---------------------------------------------
 */
static uint32_t
MMUmrc(ArmCoprocessor * copro, uint32_t icode)
{
	SystemCopro *mmu = copro->owner;
	uint32_t crn = (icode >> 16) & 0xf;
	if (ARM_SIGNALING_MODE == MODE_USER) {
		ARM_Exception(EX_UNDEFINED, 0);
		return 0;
	}

	if (mmu->mrcProc[crn]) {
		return mmu->mrcProc[crn] (mmu->crn_clientData[crn], icode);
	} else {
		fprintf(stderr, "MMU: register %d not implemented\n", crn);
		return 0;
	}

}

/*
 * ---------------------------------------------------------------------
 * MMU MCR - Move from Register to MMU-Coprocessor
 * ---------------------------------------------------------------------
 */
static void
MMUmcr(ArmCoprocessor * copro, uint32_t icode, uint32_t data)
{
	SystemCopro *mmu = copro->owner;
	uint32_t crn = (icode >> 16) & 0xf;

	if (ARM_SIGNALING_MODE == MODE_USER) {
		ARM_Exception(EX_UNDEFINED, 0);
		return;
	}
	if (mmu->mcrProc[crn]) {
		return mmu->mcrProc[crn] (mmu->crn_clientData[crn], icode, data);
	} else {
		fprintf(stderr, "MMU: mcr of register %d not implemented\n", crn);
	}
}

static ArmCoprocessor copro = {
	.mrc = MMUmrc,
	.mcr = MMUmcr,
	.cdp = NULL,
	.ldc = NULL,
	.stc = NULL
};

ArmCoprocessor *
MMU9_Create(const char *name, int endian, uint32_t mmu_type)
{
	SystemCopro *mmu;
//      uint32_t variant = mmu_type & 0xffff;
	uint32_t type = mmu_type & 0xffff0000;
	gmmu = mmu = sg_new(SystemCopro);
	mmu->ctrl = 0x50078;
	mmu->iclck = 0x0000fff0;
	mmu->dclck = 0x0000fff0;
	fprintf(stderr, "- Create MMU Coprocessor\n");

	mmu->endianNode = SigNode_New("%s.endian", name);
	if (!mmu->endianNode) {
		fprintf(stderr, "Can not create MMU. EndianNode \n");
		exit(3429);
	}
	if (endian == BYTE_ORDER_BIG) {
		mmu->ctrl |= MCTRL_BE;
	}
	update_byteorder(mmu);
	copro.owner = mmu;
	MMU_ArmInit();
	switch (type) {
	    case MMU_ARM926EJS:
		    CrnHandler_New(mmu, SYSCPR_ID, arm926ejs_id_read, id_write, mmu);
		    break;
	    case MMU_ARM920T:
		    CrnHandler_New(mmu, SYSCPR_ID, arm920t_id_read, id_write, mmu);
		    break;
	    default:
		    fprintf(stderr, "Nonexisting MMU type %08x\n", type);
		    break;
	}
	CrnHandler_New(mmu, SYSCPR_CTRL, ctrl_read, ctrl_write, mmu);
	CrnHandler_New(mmu, SYSCPR_MTBASE, mtbase_read, mtbase_write, mmu);
	CrnHandler_New(mmu, SYSCPR_MDAC, mdac_read, mdac_write, mmu);
	CrnHandler_New(mmu, SYSCPR_MFSTAT, mfstat_read, mfstat_write, mmu);
	CrnHandler_New(mmu, SYSCPR_MFADDR, mfaddr_read, mfaddr_write, mmu);
	CrnHandler_New(mmu, SYSCPR_MCCTRL, mcctrl_read, mcctrl_write, mmu);
	CrnHandler_New(mmu, SYSCPR_MTLBCTRL, mtlbctrl_read, mtlbctrl_write, mmu);
	CrnHandler_New(mmu, SYSCPR_MCLCK, mclck_read, mclck_write, mmu);
	CrnHandler_New(mmu, SYSCPR_MTLBLCK, mtlblck_read, mtlblck_write, mmu);
	CrnHandler_New(mmu, SYSCPR_MPID, mpid_read, mpid_write, mmu);

	return &copro;
}
