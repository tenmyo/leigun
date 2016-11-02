/*
 **************************************************************************************************
 *
 * Standard  ARM9 Instruction Set and Addressing Modes 
 *
 * State: working
 *
 * Used ARM Reference Manual DDI0100 for implementation
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
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "instructions_arm.h"
#include "arm9cpu.h"
#include "mmu_arm9.h"
#include "compiler_extensions.h"
#include "sgstring.h"
#include "sglib.h"

#if defined(__i386__) || defined (__i486__) || defined(__i586__)
#define USE_ASM 0
#else
#define USE ASM 0
#endif

#ifdef DEBUG
#define dbgprintf(x...) { if(unlikely(debugflags&DEBUG_INSTRUCTIONS)) { fprintf(stderr,x); fflush(stderr); } }
#else
#define dbgprintf(x...)
//define dbgprintf(x...) { fprintf(stderr,x); }
#endif
#define ISNEG(x) ((x)&(1<<31))
#define ISNOTNEG(x) (!((x)&(1<<31)))

static uint32_t add_flagmap[4096];
static uint32_t sub_flagmap[4096];

#define testbit(bit_nr,value) (value&(1<<bit_nr))

#if USE_ASM
static uint32_t
asm_sub(uint32_t * diff_32, uint32_t op1_32, uint32_t op2_32)
{
	uint32_t diff, flags;
 asm("subl %3, %1\n\t" "pushfl     \n\t" "popl %0":"=g"(flags), "=r"(diff)
 :	    "1"(op1_32), "g"(op2_32)
 :	    "cc");
	*diff_32 = diff;
	return sub_flagmap[flags & 0xfff];
}

static inline uint32_t
asm_add(uint32_t * result, uint32_t op1, uint32_t op2)
{
	uint32_t sum, flags;
 asm("addl %3, %1 \n\t" "pushfl      \n\t" "popl   %0":"=g"(flags), "=r"(sum)
 :	    "1"(op1), "g"(op2)
 :	    "cc");
	*result = sum;
	return add_flagmap[flags & 0xfff];
}

static inline uint32_t
asm_cmp(uint32_t op1, uint32_t op2)
{
	uint32_t flags;
 asm("cmpl %2, %1\n\t" "pushfl     \n\t" "popl %0":"=g"(flags)
 :	    "r"(op1), "g"(op2)
 :	    "cc");
	return sub_flagmap[flags & 0xfff];
}
#endif				/* USE_ASM */

#define IA32_FLAG_C (1<<0)
#define IA32_FLAG_Z (1<<6)
#define IA32_FLAG_N (1<<7)
#define IA32_FLAG_V (1<<11)

/* DDI0100 Glossarry-VII Page 785 */
static inline int
InAPrivilegedMode(void)
{
	return (ARM_SIGNALING_MODE != MODE_USER);
}

/*
 * ------------------------------------
 * Helper functions for add and sub
 * ------------------------------------
 */
static inline uint32_t
sub_carry(uint32_t Rn, uint32_t Rm, uint32_t R)
{
	if ((Rn < (Rn - Rm)) || ((Rn - Rm) < R)) {
		return 0;
	} else {
		return FLAG_C;
	}
}

static inline uint32_t
sub_carry_nocarry(uint32_t Rn, uint32_t Rm, uint32_t R)
{
	if (Rn < R) {
		return 0;
	} else {
		return FLAG_C;
	}
}

static inline uint32_t
add_carry(uint32_t op1, uint32_t op2, uint32_t result)
{
	if (((op1 + op2) < op1) || ((op1 + op2) > result)) {
		return FLAG_C;
	} else {
		return 0;
	}
}

/*
 * Carry flag calculation for addition without use of carryflag
 * for calcualtion of result
 */
#if 0
#define add_carry_nocarry add_carry
#else
static inline uint32_t
add_carry_nocarry(uint32_t op1, uint32_t op2, uint32_t result)
{
	if (result < op1) {
		return FLAG_C;
	} else {
		return 0;
	}
}
#endif

static inline uint32_t
sub_overflow(uint32_t op1, uint32_t op2, uint32_t result)
{
	return (((op1 & ~op2 & ~result) | (~op1 & op2 & result)) >> 3) & FLAG_V;
}

static inline uint32_t
add_overflow(int32_t op1, int32_t op2, int32_t result)
{
	return (((op1 & op2 & ~result) | (~op1 & ~op2 & result)) >> 3) & FLAG_V;
}

/*
 * -----------------------------------------------------------
 * Conditons map allows a fast lookup if condition is true 
 * The bits of the condition code and the FLAGS of the CPU
 * are merged to an table index where the precalculated 
 * result is stored
 * -----------------------------------------------------------
 */

char *ARM_ConditionMap;
#define CONDITION_INDEX(icode,cpsr) ((((icode)>>28)&0xf) | (((cpsr)>>24)&0xf0))
#define CONDITION_UNINDEX_CPSR(index) (((index)&0xf0)<<24)
#define CONDITION_UNINDEX_ICODE(index) (((index)&0xf)<<28)

static inline int
check_condition(const uint32_t icode)
{
	if (likely((icode & 0xf0000000) == 0xe0000000)) {
		return 1;
	} else {
		return ARM_ConditionMap[CONDITION_INDEX(icode, REG_CPSR)];
	}
}

static inline void
init_condition_map(void)
{
	int i;
	char *m = ARM_ConditionMap = (char *)sg_calloc(256 * sizeof(*m));
	for (i = 0; i < 256; i++) {
		uint32_t cpsr = CONDITION_UNINDEX_CPSR(i);
		uint32_t icode = CONDITION_UNINDEX_ICODE(i);
		uint32_t condition = icode >> 28;
		switch (condition) {
		    case COND_EQ:
			    if (cpsr & FLAG_Z) {
				    m[i] = 1;
			    }
			    break;
		    case COND_NE:
			    if (!(cpsr & FLAG_Z)) {
				    m[i] = 1;
			    }
			    break;
		    case COND_CSHS:
			    if (cpsr & FLAG_C) {
				    m[i] = 1;
			    }
			    break;
		    case COND_CCLO:
			    if (!(cpsr & FLAG_C)) {
				    m[i] = 1;
			    }
			    break;
		    case COND_MI:
			    if (cpsr & FLAG_N) {
				    m[i] = 1;
			    }
			    break;
		    case COND_PL:
			    if (!(cpsr & FLAG_N)) {
				    m[i] = 1;
			    }
			    break;
		    case COND_VS:
			    if (cpsr & FLAG_V) {
				    m[i] = 1;
			    }
			    break;
		    case COND_VC:
			    if (!(cpsr & FLAG_V)) {
				    m[i] = 1;
			    }
			    break;
		    case COND_HI:
			    if ((cpsr & FLAG_C) && !(cpsr & FLAG_Z)) {
				    m[i] = 1;
			    }
			    break;
		    case COND_LS:
			    if (!(cpsr & FLAG_C) || (cpsr & FLAG_Z)) {
				    m[i] = 1;
			    }
			    break;
		    case COND_GE:
			    if (((cpsr & FLAG_N) && (cpsr & FLAG_V))
				|| (!(cpsr & FLAG_N) && !(cpsr & FLAG_V))) {
				    m[i] = 1;
			    }
			    break;
		    case COND_LT:
			    if (((cpsr & FLAG_N) && !(cpsr & FLAG_V))
				|| (!(cpsr & FLAG_N) && (cpsr & FLAG_V))) {
				    m[i] = 1;
			    }
			    break;
		    case COND_GT:
			    if (!(cpsr & FLAG_Z)
				&& (((cpsr & FLAG_N) && (cpsr & FLAG_V))
				    || (!(cpsr & FLAG_N) && !(cpsr & FLAG_V)))) {
				    m[i] = 1;
			    }
			    break;
		    case COND_LE:
			    if ((cpsr & FLAG_Z) || ((cpsr & FLAG_N) && !(cpsr & FLAG_V))
				|| (!(cpsr & FLAG_N) && (cpsr & FLAG_V))) {
				    m[i] = 1;
			    }
			    break;
		    case COND_AL:
			    m[i] = 1;
			    break;
		    case COND_ILLEGAL:
			    break;
		}
	}

}

/*
 * ----------------------------------------------
 * Addressing mode 1 from DDI0100 Chapter 5.1
 * Verified, ok
 * ----------------------------------------------
 */
static uint32_t
am1_imm()
{
	// v 5.1.3 Immediate 
	//uint32_t shifter_operand;
	uint32_t icode = ICODE;
	uint32_t immed8 = icode & 0xff;
	uint32_t rotate_shift = (icode >> 7) & 0x1e;
	if (rotate_shift) {
		AM_SCRATCH1 = immed8 >> (rotate_shift) | immed8 << (32 - rotate_shift);
		return (AM_SCRATCH1 & (1 << 31)) >> (31 - FLAG_C_SHIFT);

	} else {
		AM_SCRATCH1 = immed8;
		return REG_CPSR & FLAG_C;
	}
}

static uint32_t
am1_reg()
{
	// v 5.1.4 Register
	int rm = ICODE & 0xf;
	AM_SCRATCH1 = ARM9_ReadReg(rm);
	return REG_CPSR & FLAG_C;
}

static uint32_t
am1_rorx()
{
	// v 5.1.13 ror with extend
	uint32_t icode = ICODE;
	uint32_t Rm;
	int rm = icode & 0xf;
	Rm = ARM9_ReadReg(rm);
	AM_SCRATCH1 = (Rm >> 1) | ((REG_CPSR & FLAG_C) << (31 - FLAG_C_SHIFT));
	return (Rm & 1) << FLAG_C_SHIFT;
}

/*
 * ---------------------------------------------
 * 5.1.8 Logical shift right by register
 * 	ARM does shift  by (reg & 255)
 * 	IA32 does shift right by (reg & 31)
 * ---------------------------------------------
 */
static uint32_t
am1_lsrr()
{
	uint32_t icode = ICODE;
	uint8_t RsLow;
	uint32_t Rm;
	int rm = icode & 0xf;
	int rs = (icode >> 8) & 0xf;

	Rm = ARM9_ReadReg(rm);
	RsLow = ARM9_ReadReg(rs);
	if (unlikely(RsLow == 0)) {
		AM_SCRATCH1 = Rm;
		return REG_CPSR & FLAG_C;
	} else if (likely(RsLow < 32)) {
		AM_SCRATCH1 = Rm >> RsLow;
		if (Rm & (1 << (RsLow - 1))) {
			return FLAG_C;
		} else {
			return 0;
		}
	} else if (unlikely(RsLow == 32)) {
		AM_SCRATCH1 = 0;
		if (Rm & (1 << 31)) {
			return FLAG_C;
		} else {
			return 0;
		}
	} else {
		AM_SCRATCH1 = 0;
		return 0;
	}
}

static uint32_t
am1_asrr()
{
	/* 5.1.10 Arithmetic shift right by register */
	uint32_t icode = ICODE;
	uint8_t RsLow;
	uint32_t Rm;

	Rm = ARM9_ReadReg(icode & 0xf);
	RsLow = ARM9_ReadReg((icode >> 8) & 0xf);
	if (unlikely(RsLow == 0)) {
		AM_SCRATCH1 = Rm;
		return REG_CPSR & FLAG_C;
	} else if (likely(RsLow < 32)) {
		AM_SCRATCH1 = ((int32_t) Rm) >> RsLow;
		if (Rm & (1 << (RsLow - 1))) {
			return FLAG_C;
		} else {
			return 0;
		}
	} else {
		if (Rm & (1 << 31)) {
			AM_SCRATCH1 = 0xffffffff;
			return FLAG_C;
		} else {
			AM_SCRATCH1 = 0;
			return 0;
		}
	}
}

static uint32_t
am1_lslr()
{
	// 5.1.6 lsl by reg 
	uint32_t icode = ICODE;
	uint8_t RsLow;
	uint32_t Rm;
	Rm = ARM9_ReadReg(icode & 0xf);
	RsLow = ARM9_ReadReg((icode >> 8) & 0xf);
	if (unlikely(RsLow == 0)) {
		AM_SCRATCH1 = Rm;
		return REG_CPSR & FLAG_C;
	} else if (likely(RsLow < 32)) {
		AM_SCRATCH1 = Rm << RsLow;
		if ((Rm & (1 << (32 - RsLow)))) {
			return FLAG_C;
		} else {
			return 0;
		}
	} else if (RsLow == 32) {
		AM_SCRATCH1 = 0;
		return (Rm & 1) << FLAG_C_SHIFT;
	} else {
		AM_SCRATCH1 = 0;
		return 0;
	}
}

static uint32_t
am1_rorr()
{
	// v 5.1.12 ror by register
	uint32_t icode = ICODE;
	uint8_t RsLow;
	uint32_t Rm;
	Rm = ARM9_ReadReg(icode & 0xf);
	RsLow = ARM9_ReadReg((icode >> 8) & 0xf);
	if (unlikely(RsLow == 0)) {
		AM_SCRATCH1 = Rm;
		return REG_CPSR & FLAG_C;
	} else if (unlikely((RsLow & 0x1f) == 0)) {
		AM_SCRATCH1 = Rm;
		if (Rm & (1 << 31)) {
			return FLAG_C;
		} else {
			return 0;
		}
	} else {
		AM_SCRATCH1 = (Rm >> (RsLow & 0x1f)) | (Rm << (32 - (RsLow & 0x1f)));
		if (Rm & (1 << ((RsLow & 0x1f) - 1))) {
			return FLAG_C;
		} else {
			return 0;
		}
	}
}

static uint32_t
am1_lsli()
{
	// v 5.1.5 Logical shift left by immediate
	uint32_t icode = ICODE;
	uint32_t Rm;
	uint32_t shift_imm = (icode >> 7) & 0x1f;
	Rm = ARM9_ReadReg(icode & 0xf);
	if (shift_imm == 0) {
		AM_SCRATCH1 = Rm;
		return REG_CPSR & FLAG_C;
	} else {
		AM_SCRATCH1 = Rm << shift_imm;
		if (Rm & (1 << (32 - shift_imm))) {
			return FLAG_C;
		} else {
			return 0;
		}
	}
}

static uint32_t
am1_lsri()
{
	// v 5.1.7 Logical shift right by immediate
	uint32_t icode = ICODE;
	uint32_t Rm;
	uint32_t shift_imm = (icode >> 7) & 0x1f;
	Rm = ARM9_ReadReg(icode & 0xf);
	if (shift_imm == 0) {
		AM_SCRATCH1 = 0;
		return (Rm & (1 << 31)) >> (31 - FLAG_C_SHIFT);
	} else {
		AM_SCRATCH1 = Rm >> shift_imm;
		if (Rm & (1 << (shift_imm - 1))) {
			return FLAG_C;
		} else {
			return 0;
		}
	}
}

static uint32_t
am1_asri()
{
	// v 5.1.9 asr by immediate
	uint32_t icode = ICODE;
	uint32_t Rm;
	uint32_t shift_imm = (icode >> 7) & 0x1f;
	Rm = ARM9_ReadReg(icode & 0xf);
	if (shift_imm == 0) {
		// means shift32
//              fprintf(stderr,"called\n");
		AM_SCRATCH1 = ((int32_t) Rm) >> 31;
		return AM_SCRATCH1 & FLAG_C;
#if 0
		if (Rm & (1 << 31)) {
			AM_SCRATCH1 = 0xffffffff;
			return FLAG_C;
		} else {
			AM_SCRATCH1 = 0;
			return 0;
		}
#endif
	} else {
		AM_SCRATCH1 = ((int32_t) Rm) >> shift_imm;
		if (Rm & (1 << (shift_imm - 1))) {
			return FLAG_C;
		} else {
			return 0;
		}
	}
}

static uint32_t
am1_rori()
{
	// v 5.1.11 ror by immediate 
	uint32_t icode = ICODE;
	uint32_t Rm;
	uint32_t shift_imm = (icode >> 7) & 0x1f;
	Rm = ARM9_ReadReg(icode & 0xf);
	AM_SCRATCH1 = (Rm >> shift_imm) | (Rm << (32 - shift_imm));
	if (Rm & (1 << (shift_imm - 1))) {
		return FLAG_C;
	} else {
		return 0;
	}
}

static uint32_t
am1_undefined()
{
	uint32_t icode = ICODE;
	Instruction *instr = InstructionFind(icode);
	fprintf(stderr, "Unknown addressing Mode 1, icode %08x at %08x %s\n", icode, ARM_GET_NNIA,
		instr->name);
	ARM_Exception(EX_UNDEFINED, 0);
	return 0;
}

typedef uint32_t Am1_Proc(void);
static Am1_Proc **am1_map;
#define AM1_INDEX(icode) ((((icode)>>4)&0xff)|(((icode>>17))&0x700))
//define AM1_OFFSET(icode) ((((icode)>>2)&0x3fc)|(((icode>>15))&0x1c00))
#define AM1_UNINDEX(index) ((((index)&0xff)<<4) | (((index)&0x700)<<17))

void
admode1_create_mapping()
{
	int i;
	uint32_t icode;
	am1_map = (Am1_Proc **) sg_calloc(sizeof(Am1_Proc *) * 2048);
	for (i = 0; i < 2048; i++) {
		icode = AM1_UNINDEX(i);
		if ((icode & 0x0e000000) == 0x02000000) {
			// v 5.1.3 Immediate 
			am1_map[i] = am1_imm;
		} else if ((icode & 0x0e000ff0) == 0x00000000) {
			// v 5.1.4 Register
			am1_map[i] = am1_reg;
		} else if ((icode & 0x0e000ff0) == 0x00000060) {
			// v 5.1.13 ror with extend
			am1_map[i] = am1_rorx;
		} else if ((icode & 0x0e0000f0) == 0x00000030) {
			// 5.1.8 Logical shift right by register
			am1_map[i] = am1_lsrr;
		} else if ((icode & 0x0e0000f0) == 0x00000050) {
			// v 5.1.10 asr by register
			am1_map[i] = am1_asrr;
		} else if ((icode & 0x0e0000f0) == 0x00000010) {
			// 5.1.6 lsl by reg 
			am1_map[i] = am1_lslr;
		} else if ((icode & 0x0e0000f0) == 0x00000070) {
			// v 5.1.12 ror by register
			am1_map[i] = am1_rorr;
		} else if ((icode & 0x0e000070) == 0x00000000) {
			// v 5.1.5 Logical shift left by immediate
			am1_map[i] = am1_lsli;
		} else if ((icode & 0x0e000070) == 0x00000020) {
			// v 5.1.7 Logical shift right by immediate
			am1_map[i] = am1_lsri;
		} else if ((icode & 0x0e000070) == 0x00000040) {
			// v 5.1.9 asr by immediate
			am1_map[i] = am1_asri;
		} else if ((icode & 0x0e000070) == 0x00000060) {
			// v 5.1.11 ror by immediate 
			am1_map[i] = am1_rori;
		} else {
			am1_map[i] = am1_undefined;
		}
	}
}

/*
 * ----------------------------------------------
 * Addressing mode 1 distributor 
 * ----------------------------------------------
 */
static inline uint32_t
get_data_processing_operand(uint32_t icode)
{
	Am1_Proc *proc = am1_map[AM1_INDEX(icode)];
	return proc();
}

/*
 * --------------------------------------------
 * Address Mode 2 from Chapter 5.2 in DDI0100
 * verified ok, conditions ?
 * --------------------------------------------
 */
static uint32_t
am2_imm_post(uint32_t * new_Rn)
{
	// v 5.2.8 word byte immediate postindexed + unknown other mode
	uint32_t icode = ICODE;
	uint32_t addr;
	uint32_t Rn = *new_Rn;
	uint32_t U;
	uint32_t offset = icode & 0xfff;
	dbgprintf("Immediate postindexed %08x\n", icode);
	addr = Rn;
	U = testbit(23, icode);
	if (U) {
		Rn = Rn + offset;
	} else {
		Rn = Rn - offset;
	}
	*new_Rn = Rn;
	return addr;
}

static uint32_t
am2_reg_post(uint32_t * new_Rn)
{
	// v 5.2.9 word byte register postindexed WARNING
	uint32_t icode = ICODE;
	uint32_t addr;
	uint32_t Rn = *new_Rn;
	uint32_t U;
	uint32_t Rm;
	addr = Rn;
	Rm = ARM9_ReadReg(icode & 0xf);
	U = testbit(23, icode);
	if (U) {
		Rn = Rn + Rm;
	} else {
		Rn = Rn - Rm;
	}
	*new_Rn = Rn;
	return addr;
}

static uint32_t
am2_sreg_post(uint32_t * new_Rn)
{
	// v 5.2.10 word byte scaled register post indexed
	uint32_t icode = ICODE;
	uint32_t addr;
	uint32_t Rn = *new_Rn;
	uint32_t U;
	int shift = (icode >> 5) & 3;
	int shift_imm = (icode >> 7) & 0x1f;
	uint32_t offset;
	uint32_t Rm;
	Rm = ARM9_ReadReg(icode & 0xf);
	switch (shift) {
	    case 0:		// LSL
		    offset = Rm << shift_imm;
		    break;

	    case 1:		// LSR
		    if (shift_imm == 0) {	// shift32
			    offset = 0;
		    } else {
			    offset = Rm >> shift_imm;
		    }
		    break;

	    case 2:		// ASR
		    if (shift_imm == 0) {	// shift32
			    if (Rm & (1 << 31)) {
				    offset = 0xffffffff;
			    } else {
				    offset = 0;
			    }
		    } else {
			    offset = ((int32_t) Rm) >> shift_imm;
		    }
		    break;

	    case 3:		// ROR/RRX
		    if (shift_imm == 0) {
			    if (REG_CPSR & FLAG_C) {
				    offset = (1 << 31);
			    } else {
				    offset = 0;
			    }
			    offset |= Rm >> 1;
		    } else {
			    offset = (Rm >> shift_imm) | (Rm << (32 - shift_imm));
		    }
		    break;
	    default:
		    fprintf(stderr, "should be unreachable\n");
		    exit(3);
	}
	addr = Rn;
	U = testbit(23, icode);
	if (U) {
		Rn = Rn + offset;
	} else {
		Rn = Rn - offset;
	}
	*new_Rn = Rn;
	return addr;
}

static uint32_t
am2_imm_offs(uint32_t * new_Rn)
{
	// v 5.2.2 load store word or unsigned byte immediate offset
	uint32_t icode = ICODE;
	uint32_t addr;
	uint32_t Rn = *new_Rn;
	uint32_t U;
	uint32_t offset = icode & 0xfff;
	U = testbit(23, icode);
	if (U) {
		addr = Rn + offset;
	} else {
		addr = Rn - offset;
	}
	return addr;

}

static uint32_t
am2_reg_offs(uint32_t * new_Rn)
{
	// v 5.2.3 word byte register offset
	uint32_t icode = ICODE;
	uint32_t addr;
	uint32_t Rn = *new_Rn;
	uint32_t U;
	int rm = (icode) & 0xf;
	uint32_t Rm;
	Rm = ARM9_ReadReg(rm);
	U = testbit(23, icode);
	if (U) {
		addr = Rn + Rm;
	} else {
		addr = Rn - Rm;
	}
	return addr;
}

static uint32_t
am2_sreg_offs(uint32_t * new_Rn)
{
	// v 5.2.4 word byte scaled register offset
	uint32_t icode = ICODE;
	uint32_t addr;
	uint32_t Rn = *new_Rn;
	uint32_t U;
	int shift = (icode >> 5) & 3;
	int shift_imm = (icode >> 7) & 0x1f;
	uint32_t offset;
	uint32_t Rm;
	Rm = ARM9_ReadReg(icode & 0xf);
	switch (shift) {
	    case 0:		// LSL
		    offset = Rm << shift_imm;
		    break;

	    case 1:		// LSR
		    if (shift_imm == 0) {	// shift32
			    offset = 0;
		    } else {
			    offset = Rm >> shift_imm;
		    }
		    break;

	    case 2:		// ASR
		    if (shift_imm == 0) {	// shift32
			    if (Rm & (1 << 31)) {
				    offset = 0xffffffff;
			    } else {
				    offset = 0;
			    }
		    } else {
			    offset = ((int32_t) Rm) >> shift_imm;
		    }
		    break;

	    case 3:		// ROR/RRX
		    if (shift_imm == 0) {
			    if (REG_CPSR & FLAG_C) {
				    offset = (1 << 31);
			    } else {
				    offset = 0;
			    }
			    offset |= Rm >> 1;
		    } else {
			    offset = (Rm >> shift_imm) | (Rm << (32 - shift_imm));
		    }
		    break;
	    default:
		    fprintf(stderr, "should be unreachable\n");
		    exit(3);
	}
	U = testbit(23, icode);
	if (U) {
		addr = Rn + offset;
	} else {
		addr = Rn - offset;
	}
	return addr;

}

static uint32_t
am2_imm_pre(uint32_t * new_Rn)
{
	// v 5.2.5 word byte immediate pre indexed
	uint32_t icode = ICODE;
	uint32_t addr;
	uint32_t Rn = *new_Rn;
	uint32_t U;
	uint32_t offset = icode & 0xfff;
	dbgprintf("Immediate preindexed %08x\n", icode);
	U = testbit(23, icode);
	if (U) {
		Rn = Rn + offset;
	} else {
		Rn = Rn - offset;
	}
	addr = Rn;
	*new_Rn = Rn;
	return addr;
}

static uint32_t
am2_reg_pre(uint32_t * new_Rn)
{
	// v 5.2.6 word byte register pre indexed
	uint32_t icode = ICODE;
	uint32_t Rn = *new_Rn;
	uint32_t U;
	uint32_t Rm;
	Rm = ARM9_ReadReg(icode & 0xf);
	U = testbit(23, icode);
	if (U) {
		Rn = Rn + Rm;
	} else {
		Rn = Rn - Rm;
	}
	*new_Rn = Rn;
	return Rn;
}

static uint32_t
am2_sreg_pre(uint32_t * new_Rn)
{
	// v 5.2.7 word byte scaled register preindex
	uint32_t icode = ICODE;
	uint32_t addr;
	uint32_t Rn = *new_Rn;
	uint32_t U;
	int rm = (icode) & 0xf;
	int shift = (icode >> 5) & 3;
	int shift_imm = (icode >> 7) & 0x1f;
	uint32_t offset;
	uint32_t Rm;
	Rm = ARM9_ReadReg(rm);
	switch (shift) {
	    case 0:		// LSL
		    offset = Rm << shift_imm;
		    break;

	    case 1:		// LSR
		    if (shift_imm == 0) {	// shift32
			    offset = 0;
		    } else {
			    offset = Rm >> shift_imm;
		    }
		    break;

	    case 2:		// ASR
		    if (shift_imm == 0) {	// shift32
			    if (ISNEG(Rm)) {
				    offset = ~0U;
			    } else {
				    offset = 0;
			    }
		    } else {
			    offset = ((int32_t) Rm) >> shift_imm;
		    }
		    break;

	    case 3:		// ROR/RRX
		    if (shift_imm == 0) {
			    if (REG_CPSR & FLAG_C) {
				    offset = (1 << 31);
			    } else {
				    offset = 0;
			    }
			    offset |= Rm >> 1;
		    } else {
			    offset = (Rm >> shift_imm) | (Rm << (32 - shift_imm));
		    }
		    break;
	    default:
		    fprintf(stderr, "This can never happen, File %s, line %d\n", __FILE__,
			    __LINE__);
		    exit(3);
	}
	U = testbit(23, icode);
	if (U) {
		addr = Rn + offset;
	} else {
		addr = Rn - offset;
	}
	Rn = addr;
	*new_Rn = Rn;
	return addr;
}

static uint32_t
am2_undefined(uint32_t * new_Rn)
{
	fprintf(stderr, "Unknown addressing mode in instruction %08x at %08x\n", ICODE,
		ARM_GET_NNIA);
	ARM_Exception(EX_UNDEFINED, 0);
	return 0;
}

typedef uint32_t Am2_Proc(uint32_t * new_Rn);
static Am2_Proc **am2_map;
#define AM2_INDEX(icode) (((icode)&0xff0)|(((icode>>24))&0xf) | (((icode)>>9)&0x1000))
#define AM2_UNINDEX(index) (((index)&0xff0) | (((index)&0xf)<<24) | (((index)&0x1000)<<9))

void
admode2_create_mapping()
{
	int i;
	uint32_t icode;
	am2_map = (Am2_Proc **) sg_calloc(sizeof(Am2_Proc *) * 8192);
	for (i = 0; i < 8192; i++) {
		icode = AM2_UNINDEX(i);
		if ((icode & 0x0f000000) == 0x04000000) {	// allow also for ldrbt
			// v 5.2.8 word byte immediate postindexed + unknown other mode
			am2_map[i] = am2_imm_post;
		} else if ((icode & 0x0f000ff0) == 0x06000000) {
			// v 5.2.9 word byte register postindexed WARNING
			am2_map[i] = am2_reg_post;
		} else if ((icode & 0x0f000010) == 0x06000000) {
			// v 5.2.10 word byte scaled register post indexed
			am2_map[i] = am2_sreg_post;
		} else if ((icode & 0x0f200000) == 0x05000000) {
			// v 5.2.2 load store word or unsigned byte immediate offset
			am2_map[i] = am2_imm_offs;
		} else if ((icode & 0x0f200ff0) == 0x07000000) {
			// v 5.2.3 word byte register offset
			am2_map[i] = am2_reg_offs;
		} else if ((icode & 0x0f200010) == 0x07000000) {
			// v 5.2.4 word byte scaled register offset
			am2_map[i] = am2_sreg_offs;
		} else if ((icode & 0x0f200000) == 0x05200000) {
			// v 5.2.5 word byte immediate pre indexed
			am2_map[i] = am2_imm_pre;
		} else if ((icode & 0x0f200ff0) == 0x07200000) {
			// v 5.2.6 word byte register pre indexed
			am2_map[i] = am2_reg_pre;
		} else if ((icode & 0x0f200010) == 0x07200000) {
			// v 5.2.7 word byte scaled register preindex
			am2_map[i] = am2_sreg_pre;
		} else {
			am2_map[i] = am2_undefined;
		}
	}
}

/*
 * --------------------------------------------
 * Address Mode 2 from Chapter 5.2 in DDI0100
 * verified ok, conditions ?
 * --------------------------------------------
 */

static inline uint32_t
addr_mode2(uint32_t * new_Rn)
{
	Am2_Proc *proc = am2_map[AM2_INDEX(ICODE)];
	return proc(new_Rn);
}

/*
 * ------------------------------------------------------
 * Addressing Mode 3 Miscellaneous Loads and Stores
 * Chapter 5.3 in DDI0100 manual
 * Ok
 * ------------------------------------------------------
 */

static uint32_t
am3_immoff()
{
	// v 5.3.2 Immediate offset
	uint32_t icode = ICODE;
	uint32_t U;
	uint32_t immedL = icode & 0xf;
	uint32_t immedH = (icode >> 8) & 0xf;
	uint32_t offset8;
	int rn = (icode >> 16) & 0xf;
	uint32_t Rn;
	Rn = ARM9_ReadReg(rn);
	offset8 = (immedH << 4) | immedL;
	U = testbit(23, icode);
	AM3_UPDATE_RN = 0;
	if (U) {
		return Rn + offset8;
	} else {
		return Rn - offset8;
	}
}

static uint32_t
am3_regoff()
{
	// v 5.3.3 Register offset
	uint32_t icode = ICODE;
	uint32_t Rn, Rm;
	uint32_t U;
	int rm = icode & 0xf;
	int rn = (icode >> 16) & 0xf;
	Rm = ARM9_ReadReg(rm);
	Rn = ARM9_ReadReg(rn);
	U = testbit(23, icode);
	AM3_UPDATE_RN = 0;
	if (U) {
		return Rn + Rm;
	} else {
		return Rn - Rm;
	}
}

static uint32_t
am3_immpre()
{
	// v 5.3.4 Immediate pre indexed        
	uint32_t icode = ICODE;
	uint32_t Rn;
	uint32_t U;
	unsigned int immedL = icode & 0xf;
	unsigned int immedH = (icode >> 8) & 0xf;
	unsigned int offset8;
	int rn = (icode >> 16) & 0xf;
	Rn = ARM9_ReadReg(rn);
	offset8 = (immedH << 4) | immedL;
	U = testbit(23, icode);
	if (U) {
		Rn = Rn + offset8;
	} else {
		Rn = Rn - offset8;
	}
	AM3_NEW_RN = Rn;
	AM3_UPDATE_RN = 1;
	return Rn;
}

static uint32_t
am3_regpre()
{
	// v 5.3.5 Register pre indexed
	uint32_t icode = ICODE;
	uint32_t Rn, Rm;
	uint32_t U;
	int rn = (icode >> 16) & 0xf;
	int rm = icode & 0xf;
	Rm = ARM9_ReadReg(rm);
	Rn = ARM9_ReadReg(rn);
	U = testbit(23, icode);
	if (U) {
		Rn = Rn + Rm;
	} else {
		Rn = Rn - Rm;
	}
	AM3_NEW_RN = Rn;
	AM3_UPDATE_RN = 1;
	return Rn;
}

static uint32_t
am3_immpost()
{
	// 5.3.6 Immediate post indexed
	uint32_t icode = ICODE;
	uint32_t Rn;
	uint32_t U;
	uint32_t addr;
	unsigned int immedL = icode & 0xf;
	unsigned int immedH = (icode >> 8) & 0xf;
	unsigned int offset8;
	int rn = (icode >> 16) & 0xf;
	Rn = ARM9_ReadReg(rn);
	offset8 = (immedH << 4) | immedL;
	addr = Rn;
	U = testbit(23, icode);
	if (U) {
		Rn = Rn + offset8;
	} else {
		Rn = Rn - offset8;
	}
	AM3_NEW_RN = Rn;
	AM3_UPDATE_RN = 1;
	return addr;
}

static uint32_t
am3_regpost()
{
	// 5.3.7 Register post indexed
	uint32_t icode = ICODE;
	uint32_t U;
	uint32_t addr;
	int rm = icode & 0xf;
	int rn = (icode >> 16) & 0xf;
	uint32_t Rn, Rm;
	Rn = ARM9_ReadReg(rn);
	Rm = ARM9_ReadReg(rm);
	addr = Rn;
	U = testbit(23, icode);
	if (U) {
		Rn = Rn + Rm;
	} else {
		Rn = Rn - Rm;
	}
	AM3_NEW_RN = Rn;
	AM3_UPDATE_RN = 1;
	return addr;
}

static uint32_t
am3_undef()
{
	fprintf(stderr, "Undefined addressing mode 3\n");
	ARM_Exception(EX_UNDEFINED, 0);
	AM3_UPDATE_RN = 0;
	return 0;
}

#define AM3_INDEX(icode) ((((icode)>>20)&0xf6) | (((icode)>>4)&9))
#define AM3_UNINDEX(index) ((((index)&0xf6)<<20) | (((index)&9)<<4))
typedef uint32_t Am3_Proc(void);
static Am3_Proc **am3_map;

void
admode3_create_mapping()
{
	int i;
	am3_map = (Am3_Proc **) sg_calloc(sizeof(Am3_Proc *) * 256);
	for (i = 0; i < 256; i++) {
		uint32_t icode = AM3_UNINDEX(i);
		if ((icode & 0x0f600090) == 0x01400090) {
			// 5.3.2 immediate offset
			am3_map[i] = am3_immoff;
		} else if ((icode & 0x0f600090) == 0x01000090) {
			// v 5.3.3 Register offset
			am3_map[i] = am3_regoff;
		} else if ((icode & 0x0f600090) == 0x01600090) {
			// v 5.3.4 Immediate pre indexed        
			am3_map[i] = am3_immpre;
		} else if ((icode & 0x0f600090) == 0x01200090) {
			// v 5.3.5 Register pre indexed
			am3_map[i] = am3_regpre;
		} else if ((icode & 0x0f600090) == 0x00400090) {
			// 5.3.6 Immediate post indexed
			am3_map[i] = am3_immpost;
		} else if ((icode & 0x0f600090) == 0x00000090) {
			// 5.3.7 Register post indexed
			am3_map[i] = am3_regpost;
		} else {
			am3_map[i] = am3_undef;
		}
	}
}

static inline int
addr_mode3()
{
	return am3_map[AM3_INDEX(ICODE)] ();
}

/*
 * --------------------------------------
 * load/store multiple
 * --------------------------------------
 */
void
armv5_lsm()
{
	uint32_t icode = ICODE;
	uint32_t P, U, S, W, L;
	unsigned int ones;
	int rn;
	unsigned int bank;
	int i;
	uint32_t start_address;
	uint32_t Rn;
	if (!check_condition(icode)) {
		return;
	}
	ones = SGLib_OnecountU32(icode & 0xffff);
	rn = (icode >> 16) & 0xf;

	/* Unaligned LDM/STM ignores last two bits (A4-35 DDI100) */
	Rn = ARM9_ReadReg(rn) & ~3;

	U = testbit(23, icode);
	P = testbit(24, icode);	// increment before or after ?
	if (P) {
		// before ! Wrong in Doku
		if (U) {
			// increment
			start_address = Rn + 4;
			Rn = Rn + 4 * ones;
		} else {
			// decrement    
			start_address = Rn - 4 * ones;
			Rn = Rn - 4 * ones;
		}
	} else {
		// after
		if (U) {
			// increment 
			start_address = Rn;
		} else {
			// decrement;
			start_address = Rn + 4 * (1 - ones);
		}

	}
	L = testbit(20, icode);	// Load/Store
	S = testbit(22, icode);	// Modify Flags ?
	if (S && L && (icode & (1 << 15))) {
		bank = ARM_BANK;
	} else if (S) {
		bank = MODE_USER;
	} else {
		// default bank is current bank
		bank = ARM_BANK;
	}
	if (L) {
		uint32_t value[16];
		if (bank == ARM_BANK) {
			for (i = 0; i < 15; i++) {
				if (icode & (1 << i)) {
					value[i] = MMU_Read32(start_address);
					start_address += 4;
				}
			}
			if (icode & (1 << 15)) {
				value[15] = MMU_Read32(start_address) & 0xfffffffe;
			}
			for (i = 0; i < 16; i++) {
				if (icode & (1 << i)) {
					ARM9_WriteReg(value[i], i);
				}
			}
		} else {
			for (i = 0; i < 15; i++) {
				if (icode & (1 << i)) {
					value[i] = MMU_Read32(start_address);
					start_address += 4;
				}
			}
			if (icode & (1 << 15)) {
				value[15] = MMU_Read32(start_address) & 0xfffffffe;
			}
			for (i = 0; i < 8; i++) {
				if (icode & (1 << i)) {
					ARM9_WriteReg(value[i], i);
				}
			}
			for (i = 8; i < 15; i++) {
				if (icode & (1 << i)) {
					ARM9_WriteRegBank(value[i], i, bank);
				}
			}
			if (icode & (1 << 15)) {
				ARM9_WriteReg(value[15], 15);
			}
		}
	} else {
		uint32_t value;
		if (bank == ARM_BANK) {
			for (i = 0; i < 16; i++) {
				if (icode & (1 << i)) {
					value = ARM9_ReadReg(i);
					MMU_Write32(value, start_address);
					start_address += 4;
				}
			}

		} else {
			for (i = 0; i < 8; i++) {
				if (icode & (1 << i)) {
					value = ARM9_ReadRegNot15(i);
					MMU_Write32(value, start_address);
					start_address += 4;
				}
			}
			for (i = 8; i < 15; i++) {
				if (icode & (1 << i)) {
					value = ARM9_ReadRegBank(i, bank);
					MMU_Write32(value, start_address);
					start_address += 4;
				}
			}
			if (icode & (1 << 15)) {
				value = ARM9_ReadReg(15);
				MMU_Write32(value, start_address);
			}
		}
	}
	W = testbit(21, icode);	// Update Base Register ?
	if (W) {
		if (!P) {
			// increment/decrement after 
			if (U) {
				Rn = Rn + 4 * ones;
			} else {
				Rn = Rn - 4 * ones;
			}
		}
		ARM9_WriteReg(Rn, rn);
	}
	if (S && L && (icode & (1 << 15))) {
		if (likely(MODE_HAS_SPSR)) {
			SET_REG_CPSR(REG_SPSR);
		} else {
			fprintf(stderr, "RegBank %02x has no spsr in line %d\n", ARM_BANK,
				__LINE__);
		}
	}
	CycleCounter += (ones << 1);
	dbgprintf("Done LSM addr %08x L %d\n", start_address, L ? 1 : 0);
}

void
armv5_adc()
{
	uint32_t icode = ICODE;
	int rn, rd;
	int carry;
	uint32_t S;
	uint32_t cpsr = REG_CPSR;
	uint32_t op1, op2, result;
	if (!check_condition(icode)) {
		return;
	}
	rn = (icode >> 16) & 0xf;
	rd = (icode >> 12) & 0xf;
	op1 = ARM9_ReadReg(rn);
	carry = cpsr & FLAG_C;
	get_data_processing_operand(icode);
	op2 = AM_SCRATCH1;
	if (carry) {
		result = op1 + op2 + 1;
	} else {
		result = op1 + op2;
	}
	ARM9_WriteReg(result, rd);
	S = testbit(20, icode);
	if (S) {
		cpsr &= ~(FLAG_N | FLAG_Z | FLAG_C | FLAG_V);
		if (!result) {
			cpsr |= FLAG_Z;
		}
		if (ISNEG(result)) {
			cpsr |= FLAG_N;
		}
		cpsr |= add_carry(op1, op2, result);
		cpsr |= add_overflow(op1, op2, result);
		if (rd == 15) {
			if (MODE_HAS_SPSR) {
				SET_REG_CPSR(REG_SPSR);
			} else {
				fprintf(stderr, "Mode has no spsr in line %d\n", __LINE__);
			}
		} else {
			REG_CPSR = cpsr;
		}
	}
	dbgprintf("ADC result op1 %08x,op2 %08x, result %08x\n", op1, op2, result);
}

#if USE_ASM
void
armv5_add()
{
	uint32_t icode = ICODE;
	int rn, rd;
	uint32_t S;
	uint32_t Rn, op2, result;
	if (!check_condition(icode)) {
		dbgprintf("ADD: condition not true\n");
		return;
	}
	rd = (icode >> 12) & 0xf;
	rn = (icode >> 16) & 0xf;
	Rn = ARM9_ReadReg(rn);
	get_data_processing_operand(icode);
	op2 = AM_SCRATCH1;
	S = testbit(20, icode);
	if (S) {
		uint32_t cpsr = REG_CPSR;
		uint32_t new_flags;
		new_flags = asm_add(&result, Rn, op2);
		ARM9_WriteReg(result, rd);
		cpsr = (cpsr & ~(FLAG_N | FLAG_Z | FLAG_C | FLAG_V))
		    | new_flags;
		if (rd == 15) {
			if (MODE_HAS_SPSR) {
				SET_REG_CPSR(REG_SPSR);
			} else {
				fprintf(stderr, "Mode has no spsr in line %d\n", __LINE__);
			}
		} else {
			REG_CPSR = cpsr;
		}
	} else {
		result = Rn + op2;
		ARM9_WriteReg(result, rd);
	}
	dbgprintf("ADD result op1 %08x,op2 %08x, result %08x\n", Rn, op2, result);
}
#else
void
armv5_add()
{
	uint32_t icode = ICODE;
	int rn, rd;
	uint32_t S;
	uint32_t op1, op2, result;
	if (!check_condition(icode)) {
		dbgprintf("ADD: condition not true\n");
		return;
	}
	rd = (icode >> 12) & 0xf;
	rn = (icode >> 16) & 0xf;
	op1 = ARM9_ReadReg(rn);
	get_data_processing_operand(icode);
	op2 = AM_SCRATCH1;
	result = op1 + op2;
	ARM9_WriteReg(result, rd);
	S = testbit(20, icode);
	if (S) {
		uint32_t cpsr = REG_CPSR;
		cpsr &= ~(FLAG_N | FLAG_Z | FLAG_C | FLAG_V);
		if (result == 0) {
			cpsr |= FLAG_Z;
		} else if (ISNEG(result)) {
			cpsr |= FLAG_N;
		}
		cpsr |= add_carry_nocarry(op1, op2, result);
		cpsr |= add_overflow(op1, op2, result);
		if (rd == 15) {
			if (MODE_HAS_SPSR) {
				SET_REG_CPSR(REG_SPSR);
			} else {
				fprintf(stderr, "Mode has no spsr in line %d\n", __LINE__);
			}
		} else {
			REG_CPSR = cpsr;
		}
	}
	dbgprintf("ADD result op1 %08x,op2 %08x, result %08x\n", op1, op2, result);
}
#endif
void
armv5_and()
{
	uint32_t icode = ICODE;
	int rn, rd;
	uint32_t cpsr = REG_CPSR;
	uint32_t Rn, op2, result;
	uint32_t S;
	if (!check_condition(icode)) {
		return;
	}
	rd = (icode >> 12) & 0xf;
	rn = (icode >> 16) & 0xf;
	Rn = ARM9_ReadReg(rn);
	cpsr &= ~(FLAG_N | FLAG_Z | FLAG_C);
	cpsr |= get_data_processing_operand(icode);
	op2 = AM_SCRATCH1;
	result = Rn & op2;
	ARM9_WriteReg(result, rd);
	S = testbit(20, icode);
	if (S) {
		if (!result) {
			cpsr |= FLAG_Z;
		}
		if (ISNEG(result)) {
			cpsr |= FLAG_N;
		}
		if (rd == 15) {
			if (MODE_HAS_SPSR) {
				SET_REG_CPSR(REG_SPSR);
			} else {
				fprintf(stderr, "Mode has no spsr in line %d\n", __LINE__);
			}
		} else {
			REG_CPSR = cpsr;
		}
	}
	dbgprintf("AND result op1 %08x,op2 %08x, result %08x\n", Rn, op2, result);
}

void
armv5_b()
{
	uint32_t icode = ICODE;
	int32_t signed_immed;
	if (!check_condition(icode)) {
		return;
	}
	signed_immed = ((int32_t) (icode << 8)) >> 6;
	ARM_SET_NIA(ARM_GET_NNIA + signed_immed);
}

void
armv5_bl()
{
	uint32_t icode = ICODE;
	int32_t signed_immed;
	if (!check_condition(icode)) {
		return;
	}
	signed_immed = ((int32_t) (icode << 8)) >> 6;
	REG_LR = ARM_GET_NNIA - 4;
	ARM_SET_NIA(ARM_GET_NNIA + signed_immed);
}

void
armv5_bbl()
{
	uint32_t icode = ICODE;
	uint32_t L;
	int32_t signed_immed;
	if (!check_condition(icode)) {
		return;
	}
	signed_immed = ((int32_t) (icode << 8)) >> 6;
	L = testbit(24, icode);
	if (L) {
		REG_LR = ARM_GET_NNIA - 4;
	}
	ARM_SET_NIA(ARM_GET_NNIA + signed_immed);
}

void
armv5_bic()
{
	uint32_t icode = ICODE;
	int rn, rd;
	uint32_t S;
	uint32_t cpsr = REG_CPSR;
	uint32_t Rn, op2, result;
	if (!check_condition(icode)) {
		return;
	}
	rd = (icode >> 12) & 0xf;
	rn = (icode >> 16) & 0xf;
	Rn = ARM9_ReadReg(rn);
	cpsr &= ~(FLAG_N | FLAG_Z | FLAG_C);
	cpsr |= get_data_processing_operand(icode);
	op2 = AM_SCRATCH1;
	result = Rn & ~op2;
	ARM9_WriteReg(result, rd);
	S = testbit(20, icode);
	if (S) {
		if (result == 0) {
			cpsr |= FLAG_Z;
		}
		if (ISNEG(result)) {
			cpsr |= FLAG_N;
		}
		if (unlikely(rd == 15)) {
			if (MODE_HAS_SPSR) {
				SET_REG_CPSR(REG_SPSR);
			} else {
				fprintf(stderr, "Mode has no spsr in line %d\n", __LINE__);
			}
		} else {
			REG_CPSR = cpsr;
		}
	}
	dbgprintf("BIC result op1 %08x,op2 %08x, result %08x\n", Rn, op2, result);
}

void
armv5_bkpt()
{
	ARM_Break();
}

void
armv5_blx1()
{
	uint32_t icode = ICODE;
	int32_t offset;
	uint32_t h = (icode >> 23) & 2;
	offset = ((int32_t) (icode << 8)) >> 6;
	offset |= h;
	REG_LR = ARM_GET_NNIA - 4;
	REG_CPSR |= FLAG_T;
	ARM_SET_NIA(ARM_GET_NNIA + offset);
	/* Enter Thumb */
	ARM_RestartIdecoder();
}

void
armv5_blx2bx()
{
	uint32_t icode = ICODE;
	uint32_t L;
	int rm;
	uint32_t Rm;
	if (!check_condition(icode)) {
		return;
	}
	rm = icode & 0xf;
	Rm = ARM9_ReadReg(rm);
	L = testbit(5, icode);
	if (L) {
		REG_LR = ARM_NIA;
	}
	ARM_SET_NIA(Rm & 0xfffffffe);
	if (Rm & 1) {
		/* Enter Thumb mode */
		REG_CPSR |= FLAG_T;
		ARM_RestartIdecoder();
	}

}

void
armv5_bxj()
{
	fprintf(stderr, "Branch and change to Jazelle state not implemented\n");
	armv5_und();
}

void
armv5_cdp()
{
	uint32_t icode = ICODE;
	int cp;
	ArmCoprocessor *copro;
	if (!check_condition(icode)) {
		return;
	}
	cp = (icode >> 8) & 0xf;
	copro = gcpu.copro[cp];
	if (!copro) {
		dbgprintf("CDP: No ArmCoprocessor %d\n", cp);
		ARM_Exception(EX_UNDEFINED, 0);
	} else {
		if (copro->cdp) {
			copro->cdp(copro, icode);
		} else {
			fprintf(stderr, "ArmCoprocessor %d does not support CDP\n", cp);
			ARM_Exception(EX_UNDEFINED, 0);
		}
	}
}

void
armv5_clz()
{
	uint32_t icode = ICODE;
	int rm, rd;
	uint32_t count;
	uint32_t Rm;
	if (!check_condition(icode)) {
		return;
	}
	rd = (icode >> 12) & 0xf;
	rm = (icode) & 0xf;
	Rm = ARM9_ReadReg(rm);
	count = clz32(Rm);
	ARM9_WriteReg(count, rd);
	dbgprintf("CLZ of %08x gives %d\n", Rm, count);
//      fprintf(stderr,"Not tested clz\n"); exit(2);
}

void
armv5_cmn()
{
	uint32_t icode = ICODE;
	int rn;
	uint32_t cpsr = REG_CPSR;
	uint32_t Rn, op2, result;
	if (!check_condition(icode)) {
		return;
	}
	rn = (icode >> 16) & 0xf;
	Rn = ARM9_ReadReg(rn);
	cpsr &= ~(FLAG_N | FLAG_Z | FLAG_C | FLAG_V);
	get_data_processing_operand(icode);
	op2 = AM_SCRATCH1;
	result = Rn + op2;
	if (ISNEG(result)) {
		cpsr |= FLAG_N;
	} else if (result == 0) {
		cpsr |= FLAG_Z;
	}
	cpsr |= add_carry_nocarry(Rn, op2, result);
	cpsr |= add_overflow(Rn, op2, result);
	REG_CPSR = cpsr;
}

#if USE_ASM
void
armv5_cmp()
{
	uint32_t icode = ICODE;
	int rn;
	uint32_t cpsr = REG_CPSR;
	uint32_t Rn, op, new_flags;
	if (!check_condition(icode)) {
		return;
	}
	rn = (icode >> 16) & 0xf;
	Rn = ARM9_ReadReg(rn);
	get_data_processing_operand(icode);
	op = AM_SCRATCH1;
	new_flags = asm_cmp(Rn, op);
	cpsr = (cpsr & ~(FLAG_N | FLAG_Z | FLAG_C | FLAG_V))
	    | new_flags;
	REG_CPSR = cpsr;
	dbgprintf("CMP result op1 %08x,op2 %08x, cpsr %08x\n", Rn, op, cpsr);
}
#else
void
armv5_cmp()
{
	uint32_t icode = ICODE;
	int rn;
	uint32_t cpsr = REG_CPSR;
	uint32_t Rn, op, result;
	if (!check_condition(icode)) {
		return;
	}
	rn = (icode >> 16) & 0xf;
	Rn = ARM9_ReadReg(rn);
	get_data_processing_operand(icode);
	op = AM_SCRATCH1;
	result = Rn - op;
	cpsr &= ~(FLAG_N | FLAG_Z | FLAG_C | FLAG_V);
	if (result == 0) {
		cpsr |= FLAG_Z;
	}
	if (ISNEG(result)) {
		cpsr |= FLAG_N;
	}
	cpsr |= sub_carry_nocarry(Rn, op, result);
	cpsr |= sub_overflow(Rn, op, result);
	REG_CPSR = cpsr;
	dbgprintf("CMP result op1 %08x,op2 %08x, result %08x cpsr %08x\n", Rn, op, result, cpsr);
}
#endif
void
armv5_eor()
{
	uint32_t icode = ICODE;
	int rn, rd;
	uint32_t cpsr = REG_CPSR;
	uint32_t S;
	uint32_t op1, op2, result;
	if (!check_condition(icode)) {
		return;
	}
	rd = (icode >> 12) & 0xf;
	rn = (icode >> 16) & 0xf;
	op1 = ARM9_ReadReg(rn);
	cpsr &= ~(FLAG_N | FLAG_Z | FLAG_C);
	cpsr |= get_data_processing_operand(icode);
	op2 = AM_SCRATCH1;
	result = op1 ^ op2;
	ARM9_WriteReg(result, rd);
	S = testbit(20, icode);
	if (S) {
		if (!result) {
			cpsr |= FLAG_Z;
		}
		if (ISNEG(result)) {
			cpsr |= FLAG_N;
		}
		if (rd == 15) {
			if (MODE_HAS_SPSR) {
				SET_REG_CPSR(REG_SPSR);
			} else {
				fprintf(stderr, "Mode has no spsr in line %d\n", __LINE__);
			}
		} else {
			REG_CPSR = cpsr;
		}
	}
	dbgprintf("EOR result op1 %08x,op2 %08x, result %08x\n", op1, op2, result);
}

void
armv5_ldc()
{
	uint32_t icode = ICODE;
	int cp;
	ArmCoprocessor *copro;
	if (!check_condition(icode)) {
		return;
	}
	cp = (icode >> 8) & 0xf;
	copro = gcpu.copro[cp];
	if (!copro) {
		dbgprintf("LDC: No ArmCoprocessor %d\n", cp);
		ARM_Exception(EX_UNDEFINED, 0);
	} else {
		if (copro->ldc) {
			copro->ldc(copro, icode);
		} else {
			fprintf(stderr, "ArmCoprocessor %d does not support LDC\n", cp);
			ARM_Exception(EX_UNDEFINED, 0);
		}
	}
	dbgprintf("address %08x, instruction %08x\n", ARM_GET_NNIA, icode);
}

static inline void
armv5_strwub()
{
	uint32_t icode = ICODE;
	int rd;
	uint32_t addr, value;
	int rn;
	uint32_t Rn, Rn_new;
	uint32_t B;
	if (!check_condition(icode)) {
		return;
	}
	rd = (icode >> 12) & 0xf;
	rn = (icode >> 16) & 0xf;
	Rn = ARM9_ReadReg(rn);
	Rn_new = Rn;
	addr = addr_mode2(&Rn_new);
	B = testbit(22, icode);
	if (!B) {
		value = ARM9_ReadReg(rd);
		if (unlikely(addr & 3)) {
			MMU_AlignmentException(addr);
			fprintf(stderr, "Warning: unaligned str icode %08x, addr %08x, pc %08x\n",
				icode, addr, ARM_GET_NNIA);
			addr = addr & ~3;
		}
		MMU_Write32(value, addr);

	} else {
		value = ARM9_ReadReg(rd);
		MMU_Write8(value, addr);
	}
	if (Rn_new != Rn) {
		ARM9_WriteReg(Rn_new, rn);
	}
	dbgprintf("Stored value %08x from register %d to addr %08x\n", value, rd, addr);
}

inline void
armv5_ldr()
{
	uint32_t icode = ICODE;
	int rn, rd;
	uint32_t addr, value;
	uint32_t Rn, Rn_new;
	if (!check_condition(icode)) {
		return;
	}
	rd = (icode >> 12) & 0xf;
	rn = (icode >> 16) & 0xf;
	Rn = ARM9_ReadReg(rn);
	Rn_new = Rn;
	addr = addr_mode2(&Rn_new);
	if (unlikely(addr & 3)) {
		unsigned int shift;
		uint32_t memval;
		MMU_AlignmentException(addr);
		fprintf(stderr, "Warning: Unaligned ldr  from %08x to reg %d at PC %08x\n", addr,
			rd, ARM_GET_NNIA - 8);
		memval = MMU_Read32(addr & ~3);
		shift = (addr & 3) << 3;
		value = (memval >> shift) | (memval << (32 - shift));
	} else {
		value = MMU_Read32(addr);
	}
	if (likely(rd != 15)) {
		ARM9_WriteReg(value, rd);
	} else {
		if (unlikely(value & 1)) {
			/* Goto Thumb mode */
			REG_CPSR |= FLAG_T;
			ARM_SET_NIA(value & 0xfffffffe);
			ARM9_WriteReg(Rn_new, rn);
			ARM_RestartIdecoder();
		} else {
			ARM_SET_NIA(value & 0xfffffffc);
		}
	}
	if (Rn_new != Rn) {
		ARM9_WriteReg(Rn_new, rn);
	}
}

inline void
armv5_ldrb()
{
	uint32_t icode = ICODE;
	int rd, rn;
	uint32_t addr, value;
	uint32_t Rn, Rn_new;
	if (!check_condition(icode)) {
		return;
	}
	rd = (icode >> 12) & 0xf;
	if (rd == 0xf) {
		/* This is the PLD (preload) instruction */
		return;
	}
	rn = (icode >> 16) & 0xf;
	Rn = ARM9_ReadReg(rn);
	Rn_new = Rn;
	addr = addr_mode2(&Rn_new);
	{
		uint8_t v;
		v = MMU_Read8(addr);
		value = v;
		ARM9_WriteReg(value, rd);
	}
	if (Rn_new != Rn) {
		ARM9_WriteReg(Rn_new, rn);
	}
}

void
armv5_ldrbt()
{
	gcpu.signaling_mode = MODE_USER;
	armv5_ldrb();
	gcpu.signaling_mode = REG_CPSR & 0x1f;
}

void
armv5_ldrh()
{
	uint32_t icode = ICODE;
	int rd;
	uint32_t addr;
	uint32_t Rd;
	uint16_t value;
	if (!check_condition(icode)) {
		return;
	}
	rd = (icode >> 12) & 0xf;
	addr = addr_mode3();
	if (unlikely(addr & 1)) {
		MMU_AlignmentException(addr);
		fprintf(stderr, "Unaligned ldrh at 0x%08x rd %d gives unpredictable results !\n",
			ARM_GET_NNIA - 8, rd);
		return;
	}
	value = MMU_Read16(addr);
	Rd = value;
	ARM9_WriteReg(Rd, rd);
	if (AM3_UPDATE_RN) {
		int rn = (icode >> 16) & 0xf;
		ARM9_WriteReg(AM3_NEW_RN, rn);
	}
	dbgprintf("LDRH from %08x got %04x to Register %d, now %08x\n", addr, value, rd, Rd);
}

void
armv5_ldrsb()
{
	uint32_t icode = ICODE;
	int rd;
	uint32_t addr;
	uint32_t Rd;
	int8_t value;
	if (!check_condition(icode)) {
		return;
	}
	rd = (icode >> 12) & 0xf;
	addr = addr_mode3();
	value = MMU_Read8(addr);
	Rd = (int32_t) value;
	ARM9_WriteReg(Rd, rd);
	if (AM3_UPDATE_RN) {
		int rn = (icode >> 16) & 0xf;
		ARM9_WriteReg(AM3_NEW_RN, rn);
	}
	dbgprintf("LDRSB\n");
}

void
armv5_ldrsh()
{
	uint32_t icode = ICODE;
	int rd;
	uint32_t addr;
	uint32_t Rd;
	int16_t value;
	if (!check_condition(icode)) {
		return;
	}
	rd = (icode >> 12) & 0xf;
	addr = addr_mode3();
	if (unlikely(addr & 1)) {
		MMU_AlignmentException(addr);
		fprintf(stderr, "Unaligned ldrh instruction gives unpredictable results\n");
		return;
	}
	value = MMU_Read16(addr);
	Rd = (int32_t) value;
	ARM9_WriteReg(Rd, rd);
	if (AM3_UPDATE_RN) {
		int rn = (icode >> 16) & 0xf;
		ARM9_WriteReg(AM3_NEW_RN, rn);
	}
	dbgprintf("LDRSH\n");
}

void
armv5_ldrt()
{
	gcpu.signaling_mode = MODE_USER;
	armv5_ldr();
	gcpu.signaling_mode = REG_CPSR & 0x1f;
}

void
armv5_mcr()
{
	uint32_t icode = ICODE;
	int rd, cp_num;
	uint32_t Rd;
	ArmCoprocessor *copro;
	if (!check_condition(icode)) {
		return;
	}
	cp_num = (icode >> 8) & 0xf;
	rd = (icode >> 12) & 0xf;
	copro = gcpu.copro[cp_num];
	if (copro && copro->mcr) {
		Rd = ARM9_ReadReg(rd);
		copro->mcr(copro, icode, Rd);
	} else {
		dbgprintf("ArmCoprocessor %d MCR operation not found\n", cp_num);
		ARM_Exception(EX_UNDEFINED, 0);
	}
}

void
armv5_mla()
{
	uint32_t icode = ICODE;
	int rd, rn, rs, rm;
	uint32_t S;
	uint32_t Rn, Rs, Rm, Rd;
	uint32_t cpsr = REG_CPSR;
	if (!check_condition(icode)) {
		return;
	}
	rd = (icode >> 16) & 0xf;
	rm = icode & 0xf;
	rs = (icode >> 8) & 0xf;
	rn = (icode >> 12) & 0xf;
	Rn = ARM9_ReadReg(rn);
	Rs = ARM9_ReadReg(rs);
	Rm = ARM9_ReadReg(rm);
	Rd = Rm * Rs + Rn;
	ARM9_WriteReg(Rd, rd);
	S = testbit(20, icode);
	if (S) {
		cpsr &= ~(FLAG_N | FLAG_Z);
		if (Rd == 0)
			cpsr |= FLAG_Z;
		if (ISNEG(Rd)) {
			cpsr |= FLAG_N;
		}
		REG_CPSR = cpsr;
	}
	dbgprintf("MLA\n");
}

void
armv5_mov()
{
	uint32_t icode = ICODE;
	uint32_t S;
	uint32_t cpsr;
	uint32_t Rd;
	int rd;
	if (!check_condition(icode)) {
		return;
	}
	rd = (icode >> 12) & 0xf;
	cpsr = REG_CPSR;
	cpsr &= ~(FLAG_N | FLAG_Z | FLAG_C);
	cpsr |= get_data_processing_operand(icode);
	Rd = AM_SCRATCH1;
	dbgprintf("MOV value %08x to reg %d \n", Rd, rd);
	ARM9_WriteReg(Rd, rd);
	S = testbit(20, icode);
	if (S) {
		if (rd == 15) {
			if (MODE_HAS_SPSR) {
				SET_REG_CPSR(REG_SPSR);
			} else {
				fprintf(stderr, "Mode has no spsr in line %d pc %08x icode %08x\n",
					__LINE__, ARM_GET_NNIA, icode);
			}
		} else {
			if (!Rd) {
				cpsr |= FLAG_Z;
			}
			if (ISNEG(Rd)) {
				cpsr |= FLAG_N;
			}
			REG_CPSR = cpsr;
		}
	}
}

void
armv5_mrc()
{
	uint32_t icode = ICODE;
	int rd, cp_num;
	uint32_t Rd;
	ArmCoprocessor *copro;
	if (!check_condition(icode)) {
		return;
	}

	rd = (icode >> 12) & 0xf;
	cp_num = (icode >> 8) & 0xf;
	copro = gcpu.copro[cp_num];
	if (copro && copro->mrc) {
		Rd = copro->mrc(copro, icode);
		if (rd == 15) {
			REG_CPSR = (REG_CPSR & 0x0fffffff) | (Rd & 0xf0000000);
			dbgprintf("Rd %08x, new cpsr %08x at %08x\n", Rd, REG_CPSR, ARM_GET_NNIA);
		} else {
			ARM9_WriteReg(Rd, rd);
		}
		dbgprintf("MRC returned %08x to register %d\n", Rd, rd);
	} else {
		dbgprintf("MRC: ArmCoprocessor %d not found\n", cp_num);
		ARM_Exception(EX_UNDEFINED, 0);
	}
}

void
armv5_mrc2()
{
	fprintf(stderr, "No coprocessor supporting mrc2\n");
	ARM_Exception(EX_UNDEFINED, 0);
}

void
armv5_mrs()
{
	uint32_t icode = ICODE;
	uint32_t R;
	int rd;
	uint32_t Rd;
	if (!check_condition(icode)) {
		return;
	}
	rd = (icode >> 12) & 0xf;
	R = testbit(22, icode);
	if (R) {
		if (MODE_HAS_SPSR) {
			Rd = REG_SPSR;
			ARM9_WriteReg(Rd, rd);
		} else {
			fprintf(stderr, "Warning access to SPSR in mode %d\n", ARM_BANK);
		}
	} else {
		Rd = REG_CPSR;
		ARM9_WriteReg(Rd, rd);
	}
	dbgprintf("MRS  to %d\n", rd);
}

/**
 *******************************************************************************************
 * ARMV5 Version of MSR. 
 *******************************************************************************************
 */
void
armv5_msr()
{
	uint32_t icode = ICODE;
	uint32_t R;
	uint32_t I;		// immediate ?
	uint32_t field_mask;
	uint32_t operand;

	if (!check_condition(icode)) {
		return;
	}
	field_mask = (icode >> 16) & 0xf;
	I = testbit(25, icode);	// immediate ?
	if (I) {
		unsigned int immed8 = icode & 0xff;
		unsigned int rotate_imm = (icode >> 7) & 0x1e;
		if (rotate_imm) {
			operand = (immed8 >> (rotate_imm)) | (immed8 << (32 - rotate_imm));
		} else {
			operand = immed8;
		}
	} else {
		int rm = icode & 0xf;
		uint32_t Rm;
		Rm = ARM9_ReadReg(rm);
		operand = Rm;
	}
	R = testbit(22, icode);
	if (R == 0) {
		uint32_t cpsr = REG_CPSR;
		if ((field_mask & 1) && InAPrivilegedMode()) {
			cpsr = (cpsr & 0xffffff00) | (operand & 0xff);
		}
		if ((field_mask & 2) && InAPrivilegedMode()) {
			cpsr = (cpsr & 0xffff00ff) | (operand & 0xff00);
		}
		if ((field_mask & 4) && InAPrivilegedMode()) {
			cpsr = (cpsr & 0xff00ffff) | (operand & 0xff0000);
		}
		if ((field_mask & 8)) {
			cpsr = (cpsr & 0x00ffffff) | (operand & 0xff000000);
		}
		SET_REG_CPSR(cpsr);
		dbgprintf("msrr new CPSR %08x\n", REG_CPSR);
	} else {
		if (MODE_HAS_SPSR) {
			uint32_t spsr = REG_SPSR;
			if (field_mask & 1) {
				spsr = (spsr & 0xffffff00) | (operand & 0xff);
			}
			if (field_mask & 2) {
				spsr = (spsr & 0xffff00ff) | (operand & 0xff00);
			}
			if (field_mask & 4) {
				spsr = (spsr & 0xff00ffff) | (operand & 0xff0000);
			}
			if (field_mask & 8) {
				spsr = (spsr & 0x00ffffff) | (operand & 0xff000000);
			}
			REG_SPSR = spsr;
			dbgprintf("msrr new SPSR %08x\n", REG_SPSR);
		} else {
			fprintf(stderr, "Mode %02x has no spsr in line %d, pc %08x icode %08x\n",
				ARM_BANK, __LINE__, ARM_GET_NNIA, icode);
		}
	}
	dbgprintf("MSR  \n");
}

/**
 ********************************************************************************************
 * \fn void armv6_msr() 
 * Bits always modifyable are N,Z,C,V,Q,GE[3:0] and E (mask 0xF80F0000)
 * Bits which should never be modified (unpredictable result) are J,T (mask 0x01000020)
 * Bits A,I,F,M[4:0] can only be modified in a privileged mode. Only secure privilege
 * modes can enter Monitor mode.
 ********************************************************************************************
 */
void
armv6_msr()
{

}

void
armv5_msri()
{
	return armv5_msr();
}

void
armv5_msrr()
{
	return armv5_msr();
}

void
armv5_mul()
{
	uint32_t icode = ICODE;

	int rd, rs, rm;
	uint32_t S;
	uint32_t Rs, Rm, Rd;
	uint32_t cpsr = REG_CPSR;
	if (!check_condition(icode)) {
		return;
	}
	rd = (icode >> 16) & 0xf;
	rm = icode & 0xf;
	rs = (icode >> 8) & 0xf;
	Rs = ARM9_ReadReg(rs);
	Rm = ARM9_ReadReg(rm);
	Rd = Rm * Rs;
	ARM9_WriteReg(Rd, rd);
	S = testbit(20, icode);
	if (S) {
		cpsr &= ~(FLAG_N | FLAG_Z);
		if (Rd == 0) {
			cpsr |= FLAG_Z;
		}
		if (ISNEG(Rd)) {
			cpsr |= FLAG_N;
		}
		REG_CPSR = cpsr;
	}
	dbgprintf("MUL\n");
}

void
armv5_mvn()
{
	uint32_t icode = ICODE;
	int S;
	uint32_t cpsr;
	uint32_t value;
	int rd;
	if (!check_condition(icode)) {
		return;
	}
	rd = (icode >> 12) & 0xf;
	cpsr = REG_CPSR;
	cpsr &= ~(FLAG_N | FLAG_Z | FLAG_C);
	cpsr |= get_data_processing_operand(icode);
	value = AM_SCRATCH1;
	value ^= 0xffffffff;
	dbgprintf("MVN value %08x to reg %d \n", value, rd);
	ARM9_WriteReg(value, rd);
	S = testbit(20, icode);
	if (S) {
		if (value == 0) {
			cpsr |= FLAG_Z;
		}
		if (ISNEG(value)) {
			cpsr |= FLAG_N;
		}
		if (rd == 15) {
			if (MODE_HAS_SPSR) {
				SET_REG_CPSR(REG_SPSR);
			} else {
				fprintf(stderr, "Mode has no spsr in line %d\n", __LINE__);
			}
		} else {
			REG_CPSR = cpsr;
		}
	}
}

void
armv5_orr()
{
	uint32_t icode = ICODE;
	int rn, rd;
	uint32_t cpsr = REG_CPSR;
	uint32_t Rn, op2, result;
	int S;
	if (!check_condition(icode)) {
		return;
	}
	rd = (icode >> 12) & 0xf;
	rn = (icode >> 16) & 0xf;
	Rn = ARM9_ReadReg(rn);
	cpsr &= ~(FLAG_N | FLAG_Z | FLAG_C);
	cpsr |= get_data_processing_operand(icode);
	op2 = AM_SCRATCH1;
	result = Rn | op2;
	ARM9_WriteReg(result, rd);
	S = testbit(20, icode);
	if (S) {
		if (result == 0) {
			cpsr |= FLAG_Z;
		}
		if (ISNEG(result)) {
			cpsr |= FLAG_N;
		}
		if (rd == 15) {
			if (MODE_HAS_SPSR) {
				SET_REG_CPSR(REG_SPSR);
			} else {
				fprintf(stderr, "Mode has no spsr in line %d\n", __LINE__);
			}
		} else {
			REG_CPSR = cpsr;
		}
	}
	dbgprintf("orr result op1 %08x,op2 %08x, result %08x\n", Rn, op2, result);
}

#if USE_ASM
void
armv5_rsb()
{
	uint32_t icode = ICODE;
	int rn, rd;
	uint32_t S;
	uint32_t cpsr = REG_CPSR;
	uint32_t Rn, op, result;
	if (!check_condition(icode)) {
		return;
	}
	rd = (icode >> 12) & 0xf;
	rn = (icode >> 16) & 0xf;
	Rn = ARM9_ReadReg(rn);
	cpsr &= ~(FLAG_N | FLAG_Z | FLAG_C | FLAG_V);
	get_data_processing_operand(icode);
	op = AM_SCRATCH1;
	S = testbit(20, icode);
	if (S) {
		uint32_t new_flags;
		new_flags = asm_sub(&result, op, Rn);
		ARM9_WriteReg(result, rd);
		cpsr |= new_flags;
		if (unlikely(rd == 15)) {
			if (MODE_HAS_SPSR) {
				SET_REG_CPSR(REG_SPSR);
			} else {
				fprintf(stderr, "Mode has no spsr in line %d\n", __LINE__);
			}
		} else {
			REG_CPSR = cpsr;
		}
		dbgprintf("RSB result op1 %08x,op2 %08x, result %08x flags %08x\n", Rn, op, result,
			  cpsr);
	} else {
		result = op - Rn;
		ARM9_WriteReg(result, rd);
		dbgprintf("RSB result op1 %08x,op2 %08x, result %08x\n", Rn, op, result);
	}
}
#else
void
armv5_rsb()
{
	uint32_t icode = ICODE;
	int rn, rd;
	uint32_t S;
	uint32_t cpsr = REG_CPSR;
	uint32_t Rn, op, result;
	if (!check_condition(icode)) {
		return;
	}
	rd = (icode >> 12) & 0xf;
	rn = (icode >> 16) & 0xf;
	Rn = ARM9_ReadReg(rn);
	cpsr &= ~(FLAG_N | FLAG_Z | FLAG_C | FLAG_V);
	get_data_processing_operand(icode);
	op = AM_SCRATCH1;
	result = op - Rn;
	ARM9_WriteReg(result, rd);
	S = testbit(20, icode);
	if (S) {
		if (result == 0) {
			cpsr |= FLAG_Z;
		}
		if (ISNEG(result)) {
			cpsr |= FLAG_N;
		}
		cpsr |= sub_carry_nocarry(op, Rn, result);
		cpsr |= sub_overflow(op, Rn, result);
		if (rd == 15) {
			if (MODE_HAS_SPSR) {
				SET_REG_CPSR(REG_SPSR);
			} else {
				fprintf(stderr, "Mode has no spsr in line %d\n", __LINE__);
			}
		} else {
			REG_CPSR = cpsr;
		}
		dbgprintf("RSB result op1 %08x,op2 %08x, result %08x flags %08x\n", Rn, op, result,
			  cpsr);
	} else {
		dbgprintf("RSB result op1 %08x,op2 %08x, result %08x\n", Rn, op, result);
	}
}
#endif
void
armv5_rsc()
{
	uint32_t icode = ICODE;
	int rn, rd;
	uint32_t S;
	uint32_t cpsr = REG_CPSR;
	uint32_t carry = cpsr & FLAG_C;
	uint32_t Rn, shifter_operand, result;
	if (!check_condition(icode)) {
		return;
	}
	rd = (icode >> 12) & 0xf;
	rn = (icode >> 16) & 0xf;
	Rn = ARM9_ReadReg(rn);
	cpsr &= ~(FLAG_N | FLAG_Z | FLAG_C | FLAG_V);
	get_data_processing_operand(icode);
	shifter_operand = AM_SCRATCH1;
	if (carry) {
		result = shifter_operand - Rn;
	} else {
		result = shifter_operand - Rn - 1;
	}
	S = testbit(20, icode);
	ARM9_WriteReg(result, rd);
	if (S) {
		if (!result) {
			cpsr |= FLAG_Z;
		}
		if (ISNEG(result)) {
			cpsr |= FLAG_N;
		}
		cpsr |= sub_carry(shifter_operand, Rn, result);
		cpsr |= sub_overflow(shifter_operand, Rn, result);
		if (rd == 15) {
			if (MODE_HAS_SPSR) {
				SET_REG_CPSR(REG_SPSR);
			} else {
				fprintf(stderr, "Mode has no spsr in line %d\n", __LINE__);
			}
		} else {
			REG_CPSR = cpsr;
		}
	}
}

void
armv5_sbc()
{
	uint32_t icode = ICODE;
	int rn, rd;
	uint32_t S;
	uint32_t cpsr = REG_CPSR;
	uint32_t carry = cpsr & FLAG_C;
	uint32_t Rn, sub, Rd;
	if (!check_condition(icode)) {
		return;
	}
	rd = (icode >> 12) & 0xf;
	rn = (icode >> 16) & 0xf;
	Rn = ARM9_ReadReg(rn);
	cpsr &= ~(FLAG_N | FLAG_Z | FLAG_C | FLAG_V);
	get_data_processing_operand(icode);
	sub = AM_SCRATCH1;
	if (carry) {
		Rd = Rn - sub;
	} else {
		Rd = Rn - sub - 1;
	}
	ARM9_WriteReg(Rd, rd);
	S = testbit(20, icode);
	if (S) {
		if (Rd == 0) {
			cpsr |= FLAG_Z;
		}
		if (ISNEG(Rd)) {
			cpsr |= FLAG_N;
		}
		cpsr |= sub_carry(Rn, sub, Rd);
		cpsr |= sub_overflow(Rn, sub, Rd);
		if (rd == 15) {
			if (MODE_HAS_SPSR) {
				SET_REG_CPSR(REG_SPSR);
			} else {
				fprintf(stderr, "Mode has no spsr in line %d\n", __LINE__);
			}
		} else {
			REG_CPSR = cpsr;
		}
	}
	dbgprintf("Warning untested SBC\n");
}

void
armv5_smlal()
{

	uint32_t icode = ICODE;
	int rdl, rdh, rs, rm;
	uint32_t S;
	uint32_t Rs, Rm, Rdl, Rdh;
	int64_t result;
	uint32_t cpsr = REG_CPSR;
	if (!check_condition(icode)) {
		return;
	}
	rdh = (icode >> 16) & 0xf;
	rdl = (icode >> 12) & 0xf;
	rs = (icode >> 8) & 0xf;
	rm = icode & 0xf;
	Rs = ARM9_ReadReg(rs);
	Rm = ARM9_ReadReg(rm);
	Rdl = ARM9_ReadReg(rdl);
	Rdh = ARM9_ReadReg(rdh);
	result = Rdl | (((uint64_t) Rdh) << 32);
	result += ((int64_t) (int32_t) Rm) * ((int64_t) (int32_t) Rs);
	Rdh = result >> 32;
	Rdl = result;
	ARM9_WriteReg(Rdl, rdl);
	ARM9_WriteReg(Rdh, rdh);
	S = testbit(20, icode);
	if (S) {
		cpsr &= ~(FLAG_N | FLAG_Z);
		if ((Rdl == 0) && (Rdh == 0)) {
			cpsr |= FLAG_Z;
		}
		if (ISNEG(Rdh)) {
			cpsr |= FLAG_N;
		}
		REG_CPSR = cpsr;
	}
	dbgprintf("SMLAL\n");

}

void
armv5_smull()
{
	uint32_t icode = ICODE;
	int rdl, rdh, rs, rm;
	uint32_t S;
	uint32_t Rs, Rm, Rdl, Rdh;
	int64_t result;
	uint32_t cpsr = REG_CPSR;
	if (!check_condition(icode)) {
		return;
	}
	rdh = (icode >> 16) & 0xf;
	rdl = (icode >> 12) & 0xf;
	rs = (icode >> 8) & 0xf;
	rm = icode & 0xf;
	Rs = ARM9_ReadReg(rs);
	Rm = ARM9_ReadReg(rm);
	Rdl = ARM9_ReadReg(rdl);
	Rdh = ARM9_ReadReg(rdh);
	result = ((int64_t) (int32_t) Rm) * ((int64_t) (int32_t) Rs);
	Rdh = result >> 32;
	Rdl = result;
	ARM9_WriteReg(Rdh, rdh);
	ARM9_WriteReg(Rdl, rdl);
	S = testbit(20, icode);
	if (S) {
		cpsr &= ~(FLAG_N | FLAG_Z);
		if ((Rdl == 0) && (Rdh == 0)) {
			cpsr |= FLAG_Z;
		}
		if (ISNEG(Rdh)) {
			cpsr |= FLAG_N;
		}
		REG_CPSR = cpsr;
	}
	dbgprintf("SMULL %d*%d is %lld\n", Rm, Rs, result);
}

void
armv5_stc()
{
	uint32_t icode = ICODE;
	int cp;
	ArmCoprocessor *copro;
	if (!check_condition(icode)) {
		return;
	}
	cp = (icode >> 8) & 0xf;
	copro = gcpu.copro[cp];
	if (!copro) {
		dbgprintf("STC: No ArmCoprocessor %d\n", cp);
		ARM_Exception(EX_UNDEFINED, 0);
	} else {
		if (copro->stc) {
			copro->stc(copro, icode);
		} else {
			fprintf(stderr, "ArmCoprocessor %d does not support STC\n", cp);
			ARM_Exception(EX_UNDEFINED, 0);
		}
	}
}

void
armv5_str()
{
//      fprintf(stderr,"icode %08x at addr %08x\n",icode,ARM_GET_NNIA);
	return armv5_strwub();
}

void
armv5_strb()
{
	return armv5_strwub();
}

void
armv5_strbt()
{
	gcpu.signaling_mode = MODE_USER;
	armv5_strwub();
	gcpu.signaling_mode = REG_CPSR & 0x1f;
}

void
armv5_strh()
{
	uint32_t icode = ICODE;
	int rd;
	uint32_t addr;
	uint32_t Rd;
	if (!check_condition(icode)) {
		return;
	}
	rd = (icode >> 12) & 0xf;
	addr = addr_mode3();
	if (unlikely(addr & 1)) {
		MMU_AlignmentException(addr);
		fprintf(stderr, "Unaligned strh at %08x rd %d gives unpredictable results\n",
			ARM_GET_NNIA - 8, rd);
		return;
	}
	Rd = ARM9_ReadReg(rd);
	MMU_Write16(Rd, addr);
	if (AM3_UPDATE_RN) {
		int rn = (icode >> 16) & 0xf;
		ARM9_WriteReg(AM3_NEW_RN, rn);
	}
	dbgprintf("STRH to value %04x to %08x\n", Rd, addr);
}

void
armv5_strt()
{
	gcpu.signaling_mode = MODE_USER;
	armv5_strwub();
	gcpu.signaling_mode = REG_CPSR & 0x1f;
}

void
armv5_sub()
{
	uint32_t icode = ICODE;
	int rn, rd;
	uint32_t S;
	uint32_t Rn, op, result;
	if (!check_condition(icode)) {
		return;
	}
	rd = (icode >> 12) & 0xf;
	rn = (icode >> 16) & 0xf;
	Rn = ARM9_ReadReg(rn);
	get_data_processing_operand(icode);
	op = AM_SCRATCH1;
	result = Rn - op;
	ARM9_WriteReg(result, rd);
	S = testbit(20, icode);
	if (S) {
		if (rd == 15) {
			if (MODE_HAS_SPSR) {
				SET_REG_CPSR(REG_SPSR);
			} else {
				fprintf(stderr,
					"Unpredictable result: Mode has no spsr in line %d\n",
					__LINE__);
			}
		} else {
			uint32_t cpsr = REG_CPSR;
			cpsr &= ~(FLAG_N | FLAG_Z | FLAG_C | FLAG_V);
			if (!result) {
				cpsr |= FLAG_Z;
			}
			if (ISNEG(result)) {
				cpsr |= FLAG_N;
			}
			cpsr |= sub_carry_nocarry(Rn, op, result);
			cpsr |= sub_overflow(Rn, op, result);
			REG_CPSR = cpsr;
		}
	}
	dbgprintf("SUB result op1 %08x,op2 %08x, result %08x\n", Rn, op, result);
}

void
armv5_swi()
{
	ARM_Exception(EX_SWI, 0);
}

void
armv5_swp()
{
	uint32_t icode = ICODE;
	int rn;
	int rd;
	int rm;
	int rot;
	uint32_t Rn, Rm, temp, val;
	if (!check_condition(icode)) {
		return;
	}
	rd = (icode >> 12) & 0xf;
	rm = icode & 0xf;
	rn = (icode >> 16) & 0xf;
	Rn = ARM9_ReadReg(rn);
	Rm = ARM9_ReadReg(rm);
	val = MMU_Read32(Rn);
	rot = (Rn & 3) << 3;
	if (rot) {
		temp = (val >> rot) | (val << (32 - rot));
	} else {
		temp = val;
	}
	MMU_Write32(Rm, Rn);
	ARM9_WriteReg(temp, rd);
	dbgprintf("SWP\n");
}

void
armv5_swpb()
{
	uint32_t icode = ICODE;
	int rn;
	int rd;
	int rm;
	uint32_t Rn, Rm, Rd;
	uint8_t val;
	if (!check_condition(icode)) {
		return;
	}
	rd = (icode >> 12) & 0xf;
	rm = icode & 0xf;
	rn = (icode >> 16) & 0xf;
	Rn = ARM9_ReadReg(rn);
	Rm = ARM9_ReadReg(rm);
	val = MMU_Read8(Rn);
	MMU_Write8(Rm, Rn);
	Rd = val;
	ARM9_WriteReg(Rd, rd);
	dbgprintf("SWPB\n");
}

void
armv5_teq()
{
	uint32_t icode = ICODE;
	int rn;
	uint32_t cpsr = REG_CPSR;
	uint32_t Rn, op2, alu_out;
	if (!check_condition(icode)) {
		return;
	}
	rn = (icode >> 16) & 0xf;
	Rn = ARM9_ReadReg(rn);
	cpsr &= ~(FLAG_N | FLAG_Z | FLAG_C);
	cpsr |= get_data_processing_operand(icode);
	op2 = AM_SCRATCH1;
	alu_out = Rn ^ op2;
	if (!alu_out) {
		cpsr |= FLAG_Z;
	}
	if (ISNEG(alu_out)) {
		cpsr |= FLAG_N;
	}
	REG_CPSR = cpsr;
	dbgprintf("TEQ op1 %08x,op2 %08x, result %08x\n", Rn, op2, alu_out);
}

void
armv5_tst()
{
	uint32_t icode = ICODE;
	int rn;
	uint32_t Rn, op2, result;
	uint32_t cpsr = REG_CPSR;
	if (!check_condition(icode)) {
		return;
	}
	cpsr &= ~(FLAG_N | FLAG_Z | FLAG_C);
	rn = (icode >> 16) & 0xf;
	Rn = ARM9_ReadReg(rn);
	cpsr |= get_data_processing_operand(icode);
	op2 = AM_SCRATCH1;
	result = Rn & op2;
	if (!result) {
		cpsr |= FLAG_Z;
	}
	if (ISNEG(result)) {
		cpsr |= FLAG_N;
	}
	REG_CPSR = cpsr;
	dbgprintf("tst %08x and %08x result %08x\n", Rn, op2, result);
}

void
armv5_umlal()
{
	uint32_t icode = ICODE;
	int rdl, rdh, rs, rm;
	uint32_t S;
	uint32_t Rs, Rm, Rdl, Rdh;
	uint64_t result;
	uint32_t cpsr = REG_CPSR;
	if (!check_condition(icode)) {
		return;
	}
	rdh = (icode >> 16) & 0xf;
	rdl = (icode >> 12) & 0xf;
	rm = icode & 0xf;
	rs = (icode >> 8) & 0xf;
	Rs = ARM9_ReadReg(rs);
	Rm = ARM9_ReadReg(rm);
	Rdl = ARM9_ReadReg(rdl);
	Rdh = ARM9_ReadReg(rdh);
	result = Rdl + (((uint64_t) Rdh) << 32);
	result += ((uint64_t) Rm) * ((uint64_t) Rs);
	Rdh = result >> 32;
	Rdl = result;
	ARM9_WriteReg(Rdl, rdl);
	ARM9_WriteReg(Rdh, rdh);
	S = testbit(20, icode);
	if (S) {
		cpsr &= ~(FLAG_N | FLAG_Z);
		if ((Rdl == 0) && (Rdh == 0)) {
			cpsr |= FLAG_Z;
		}
		if (ISNEG(Rdh)) {
			cpsr |= FLAG_N;
		}
		REG_CPSR = cpsr;
	}
	dbgprintf("UMLAL\n");
}

void
armv5_umull()
{
	uint32_t icode = ICODE;
	int rdl, rdh, rs, rm;
	uint32_t S;
	uint32_t Rs, Rm, Rdl, Rdh;
	uint64_t result;
	uint32_t cpsr = REG_CPSR;
	if (!check_condition(icode)) {
		return;
	}
	rdh = (icode >> 16) & 0xf;
	rdl = (icode >> 12) & 0xf;
	rs = (icode >> 8) & 0xf;
	rm = icode & 0xf;
	Rs = ARM9_ReadReg(rs);
	Rm = ARM9_ReadReg(rm);
	result = ((uint64_t) Rm) * ((uint64_t) Rs);
	Rdh = result >> 32;
	Rdl = result;
	ARM9_WriteReg(Rdl, rdl);
	ARM9_WriteReg(Rdh, rdh);
	S = testbit(20, icode);
	if (S) {
		cpsr &= ~(FLAG_N | FLAG_Z);
		if ((Rdl == 0) && (Rdh == 0)) {
			cpsr |= FLAG_Z;
		}
		if (ISNEG(Rdh)) {
			cpsr |= FLAG_N;
		}
		REG_CPSR = cpsr;
	}
	dbgprintf("UMULL\n");
}

/* 
 * --------------------------------------------------------
 * Enhanced DSP Intructions from DDI100 Chapter A10
 * --------------------------------------------------------
 */
void
armv5_ldrd()
{
	uint32_t icode = ICODE;
	int rd1, rd2;
	uint32_t Rd1, Rd2;
	uint32_t addr;
	if (!check_condition(icode)) {
		return;
	}
	/*fprintf(stderr,"Warning: untested instruction LDRD\n"); */
	rd1 = (icode >> 12) & 0xf;
	rd2 = rd1 | 1;
	if (rd1 & 1) {
		fprintf(stderr, "LDRD: only even numbered destination registers are allowed\n");
		return;
	}
	addr = addr_mode3();
	if (addr & 7) {
		MMU_AlignmentException(addr);
		addr = addr & ~7;
	}
	Rd1 = MMU_Read32(addr);
	Rd2 = MMU_Read32(addr + 4);
	ARM9_WriteReg(Rd1, rd1);
	ARM9_WriteReg(Rd2, rd2);
	if (AM3_UPDATE_RN) {
		int rn = (icode >> 16) & 0xf;
		ARM9_WriteReg(AM3_NEW_RN, rn);
	}
}

void
armv5_mcrr()
{
	fprintf(stderr, "No coprocessor supporting MCRR\n");
	ARM_Exception(EX_UNDEFINED, 0);
}

void
armv5_mrrc()
{
	fprintf(stderr, "No coprocessor supporting MRRC\n");
	ARM_Exception(EX_UNDEFINED, 0);
}

/*
 * ----------------------------------------------------
 * The manual doesn't say if QADD can clear Q-Flag
 * ----------------------------------------------------
 */

void
armv5_qadd()
{
	uint32_t icode = ICODE;
	int rn, rm, rd;
	int32_t Rn, Rm;
	int64_t result;
	if (!check_condition(icode)) {
		dbgprintf("QADD: condition not true\n");
		return;
	}
	rn = (icode >> 16) & 0xf;
	rm = icode & 0xf;
	rd = (icode >> 12) & 0xf;
	fprintf(stderr, "Warning: untested instruction QADD\n");
	Rn = ARM9_ReadReg(rn);
	Rm = ARM9_ReadReg(rm);
	result = Rn + Rm;
	if (result > ((1LL << 31) - 1)) {
		result = (1LL << 31) - 1;
		REG_CPSR = REG_CPSR | FLAG_Q;
	} else if (result < -(1LL << 31)) {
		result = -(1LL << 31);
		REG_CPSR = REG_CPSR | FLAG_Q;
	}
	ARM9_WriteReg(result, rd);
	dbgprintf("QADD result op1 %08x,op2 %08x, result %08llx\n", Rn, Rm, result);
}

void
armv5_qdadd()
{
	uint32_t icode = ICODE;
	int rn, rm, rd;
	int32_t Rn, Rm;
	int64_t dRn, result;
	if (!check_condition(icode)) {
		dbgprintf("QDADD: condition not true\n");
		return;
	}
	rn = (icode >> 16) & 0xf;
	rm = icode & 0xf;
	rd = (icode >> 12) & 0xf;
	fprintf(stderr, "Warning: untested instruction QDADD\n");
	Rn = ARM9_ReadReg(rn);
	Rm = ARM9_ReadReg(rm);
	dRn = Rn + Rn;
	if (dRn > ((1LL << 31) - 1)) {
		dRn = (1LL << 31) - 1;
		REG_CPSR = REG_CPSR | FLAG_Q;
	} else if (dRn < -(1LL << 31)) {
		dRn = -(1LL << 31);
		REG_CPSR = REG_CPSR | FLAG_Q;
	}
	result = Rm + dRn;
	if (result > ((1LL << 31) - 1)) {
		result = (1LL << 31) - 1;
		REG_CPSR = REG_CPSR | FLAG_Q;
	} else if (result < -(1LL << 31)) {
		result = -(1LL << 31);
		REG_CPSR = REG_CPSR | FLAG_Q;
	}
	ARM9_WriteReg(result, rd);
	dbgprintf("QDADD result op1 %08x,op2 %08x, result %08llx\n", Rn, Rm, result);
}

void
armv5_qdsub()
{
	uint32_t icode = ICODE;
	int rn, rm, rd;
	int32_t Rn, Rm;
	int64_t dRn, result;
	if (!check_condition(icode)) {
		dbgprintf("QDSUB: condition not true\n");
		return;
	}
	fprintf(stderr, "Warning: untested instruction QDSUB\n");
	rn = (icode >> 16) & 0xf;
	rm = icode & 0xf;
	rd = (icode >> 12) & 0xf;
	Rn = ARM9_ReadReg(rn);
	Rm = ARM9_ReadReg(rm);
	dRn = Rn + Rn;
	if (dRn > ((1LL << 31) - 1)) {
		dRn = (1LL << 31) - 1;
		REG_CPSR = REG_CPSR | FLAG_Q;
	} else if (dRn < -(1LL << 31)) {
		dRn = -(1LL << 31);
		REG_CPSR = REG_CPSR | FLAG_Q;
	}
	result = Rm - dRn;
	if (result > ((1LL << 31) - 1)) {
		result = (1LL << 31) - 1;
		REG_CPSR = REG_CPSR | FLAG_Q;
	} else if (result < -(1LL << 31)) {
		result = -(1LL << 31);
		REG_CPSR = REG_CPSR | FLAG_Q;
	}
	ARM9_WriteReg(result, rd);
	dbgprintf("QDSUB result op1 %08x,op2 %08x, result %08llx\n", Rn, Rm, result);
}

void
armv5_qsub()
{
	uint32_t icode = ICODE;
	int rn, rm, rd;
	int32_t Rn, Rm;
	int64_t result;
	if (!check_condition(icode)) {
		dbgprintf("QSUB: condition not true\n");
		return;
	}
	rn = (icode >> 16) & 0xf;
	rm = icode & 0xf;
	rd = (icode >> 12) & 0xf;
	fprintf(stderr, "Warning: untested instruction QSUB\n");
	Rn = ARM9_ReadReg(rn);
	Rm = ARM9_ReadReg(rm);
	result = Rm - Rn;
	if (result > ((1LL << 31) - 1)) {
		result = (1LL << 31) - 1;
		REG_CPSR = REG_CPSR | FLAG_Q;
	} else if (result < -(1LL << 31)) {
		result = -(1LL << 31);
		REG_CPSR = REG_CPSR | FLAG_Q;
	}
	ARM9_WriteReg(result, rd);
	dbgprintf("QADD result op1 %08x,op2 %08x, result %08llx\n", Rn, Rm, result);

}

void
armv5_smlaxy()
{
	uint32_t icode = ICODE;
	int rn, rm, rd, rs;
	uint32_t Rn, Rm, Rd, Rs;
	int32_t operand1, operand2;
	int64_t result;
	int x, y;
	if (!check_condition(icode)) {
		dbgprintf("SLMAXY: condition not true\n");
		return;
	}
	rn = (icode >> 12) & 0xf;
	rm = icode & 0xf;
	rd = (icode >> 16) & 0xf;
	rs = (icode >> 8) & 0xf;
	fprintf(stderr, "Warning: untested instruction SMLA<x><y>\n");
	Rn = ARM9_ReadReg(rn);
	Rm = ARM9_ReadReg(rm);
	Rs = ARM9_ReadReg(rs);
	x = testbit(5, icode);
	if (x == 0) {
		operand1 = (int16_t) (Rm);
	} else {
		operand1 = (int16_t) (Rm >> 16);
	}
	y = testbit(6, icode);
	if (y == 0) {
		operand2 = (int16_t) (Rs);
	} else {
		operand2 = (int16_t) (Rs >> 16);
	}
	result = (operand1 * operand2) + Rn;
	if ((result > ((1LL << 31) - 1)) || (result < -(1LL << 31))) {
		REG_CPSR = REG_CPSR | FLAG_Q;
	}
	Rd = (uint32_t) result;
	ARM9_WriteReg(Rd, rd);
}

void
armv5_smlalxy()
{
	uint32_t icode = ICODE;
	int rdhi, rdlo, rm, rs;
	uint32_t RdHi, RdLo, Rm, Rs;
	uint64_t Rd;
	int64_t operand1, operand2;
	int64_t result;
	int x, y;
	if (!check_condition(icode)) {
		dbgprintf("SMLALXY: condition not true\n");
		return;
	}
	rm = icode & 0xf;
	rs = (icode >> 8) & 0xf;
	rdhi = (icode >> 16) & 0xf;
	rdlo = (icode >> 12) & 0xf;
	fprintf(stderr, "Warning: untested instruction SMLAL<x><y>\n");
	Rm = ARM9_ReadReg(rm);
	Rs = ARM9_ReadReg(rs);
	RdHi = ARM9_ReadReg(rdhi);
	RdLo = ARM9_ReadReg(rdlo);
	Rd = RdLo | ((uint64_t) RdHi << 32);
	x = testbit(5, icode);
	if (x == 0) {
		operand1 = (int16_t) (Rm);
	} else {
		operand1 = (int16_t) (Rm >> 16);
	}
	y = testbit(6, icode);
	if (y == 0) {
		operand2 = (int16_t) (Rs);
	} else {
		operand2 = (int16_t) (Rs >> 16);
	}
	result = (operand1 * operand2) + Rd;
	RdLo = (uint64_t) result;
	RdHi = ((uint64_t) result) >> 32;
	ARM9_WriteReg(RdLo, rdlo);
	ARM9_WriteReg(RdHi, rdhi);
}

void
armv5_smlawy()
{
	uint32_t icode = ICODE;
	int rn, rm, rd, rs;
	uint32_t Rn, Rm, Rd, Rs;
	int64_t operand1, operand2;
	int64_t result;
	int32_t product;
	int y;
	if (!check_condition(icode)) {
		dbgprintf("SMLAWY: condition not true\n");
		return;
	}
	rn = (icode >> 12) & 0xf;
	rm = icode & 0xf;
	rd = (icode >> 16) & 0xf;
	rs = (icode >> 8) & 0xf;
	Rn = ARM9_ReadReg(rn);
	Rm = ARM9_ReadReg(rm);
	Rs = ARM9_ReadReg(rs);
	operand1 = (int64_t) (int32_t) Rm;
	y = testbit(6, icode);
	if (y == 0) {
		operand2 = (int16_t) (Rs);
	} else {
		operand2 = (int16_t) (Rs >> 16);
	}
	product = (operand1 * operand2) >> 16;
	result = product + Rn;
	if (add_overflow(product, Rn, result)) {
		REG_CPSR = REG_CPSR | FLAG_Q;
	}
	Rd = (uint32_t) (result);
	ARM9_WriteReg(Rd, rd);
}

/*
 * ------------------------------------------
 * SMUL <x> <y>
 * observed to be generated by gcc-3.4.3
 * v1
 * ------------------------------------------
 */
void
armv5_smulxy()
{
	uint32_t icode = ICODE;
	int rm, rd, rs;
	uint32_t Rm, Rd, Rs;
	int32_t operand1, operand2;
	int64_t result;
	int x, y;
	if (!check_condition(icode)) {
		dbgprintf("SMULXY: condition not true\n");
		return;
	}
	/* fprintf(stderr,"SMUL<x><y> at %08x\n",ARM_GET_NNIA); */
	rm = icode & 0xf;
	rs = (icode >> 8) & 0xf;
	rd = (icode >> 16) & 0xf;
	Rm = ARM9_ReadReg(rm);
	Rs = ARM9_ReadReg(rs);
	x = testbit(5, icode);
	if (x == 0) {
		operand1 = (int16_t) (Rm);
	} else {
		operand1 = (int16_t) (Rm >> 16);
	}
	y = testbit(6, icode);
	if (y == 0) {
		operand2 = (int16_t) (Rs);
	} else {
		operand2 = (int16_t) (Rs >> 16);
	}
	result = (operand1 * operand2);
	Rd = (uint32_t) result;
	ARM9_WriteReg(Rd, rd);
}

void
armv5_smulwy()
{
	uint32_t icode = ICODE;
	int rm, rd, rs;
	uint32_t Rm, Rd, Rs;
	int64_t operand1, operand2;
	int64_t result;
	int y;
	if (!check_condition(icode)) {
		dbgprintf("SMULWY: condition not true\n");
		return;
	}
	rm = icode & 0xf;
	rd = (icode >> 16) & 0xf;
	rs = (icode >> 8) & 0xf;
	Rm = ARM9_ReadReg(rm);
	Rs = ARM9_ReadReg(rs);
	operand1 = (int64_t) (int32_t) Rm;
	y = testbit(6, icode);
	if (y == 0) {
		operand2 = (int64_t) (int16_t) (Rs);
	} else {
		operand2 = (int64_t) (int16_t) (Rs >> 16);
	}
	result = (operand1 * operand2);
	result = result >> 16;
	Rd = (uint32_t) (result);
	ARM9_WriteReg(Rd, rd);
}

void
armv5_strd()
{
	uint32_t icode = ICODE;
	int rd1, rd2;
	uint32_t Rd1, Rd2;
	uint32_t addr;
	if (!check_condition(icode)) {
		return;
	}
	/*fprintf(stderr,"Warning: untested instruction STRD\n"); */
	rd1 = (icode >> 12) & 0xf;
	rd2 = rd1 | 1;
	if (rd1 & 1) {
		fprintf(stderr, "STRD: only even numbered destination registers are allowed\n");
		return;
	}
	addr = addr_mode3();
	if (addr & 7) {
		MMU_AlignmentException(addr);
		addr = addr & ~7;
	}
	Rd1 = ARM9_ReadReg(rd1);
	Rd2 = ARM9_ReadReg(rd2);
	MMU_Write32(Rd1, addr);
	MMU_Write32(Rd2, addr + 4);
	if (AM3_UPDATE_RN) {
		int rn = (icode >> 16) & 0xf;
		ARM9_WriteReg(AM3_NEW_RN, rn);
	}
}

void
armv5_und()
{
	uint32_t icode = ICODE;
	fprintf(stderr, "Instruction not found for icode %08x at pc %08x\n", icode, ARM_GET_NNIA);
	ARM_Exception(EX_UNDEFINED, 0);
}

void
InitInstructions()
{
	int i;
	for (i = 0; i < 4096; i++) {
		uint32_t flags = 0;
		if (i & IA32_FLAG_C) {
			flags |= FLAG_C;
		}
		if (i & IA32_FLAG_Z) {
			flags |= FLAG_Z;
		}
		if (i & IA32_FLAG_N) {
			flags |= FLAG_N;
		}
		if (i & IA32_FLAG_V) {
			flags |= FLAG_V;
		}
		add_flagmap[i] = flags;
		sub_flagmap[i] = flags ^ FLAG_C;
	}
	admode1_create_mapping();
	admode2_create_mapping();
	admode3_create_mapping();
	init_condition_map();
}
