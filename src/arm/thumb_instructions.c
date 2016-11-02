/*
 *********************************************************************************************** 
 *
 * ARM Thumb instruction set
 *
 * Copyright 2009 Jochen Karrer. All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without modification, are
 * permitted provided that the following conditions are met:
 * 
 *   1. Redistributions of source code must retain the above copyright notice, this list of
 *       conditions and the following disclaimer.
 * 
 *    2. Redistributions in binary form must reproduce the above copyright notice, this list
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
 ***********************************************************************************************
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "arm9cpu.h"
#include "thumb_instructions.h"
#include "instructions_arm.h"
#include "mmu_arm9.h"
#include "sglib.h"

#if 0
#define dbgprintf(x...) dbgprintf(x)
#else
#define dbgprintf(x...)
#endif

#define ISNEG(x) ((x)&(1<<31))
#define ISNOTNEG(x) (!((x)&(1<<31)))

#define CONDITION_INDEX(condition,cpsr) ((condition) | (((cpsr)>>24)&0xf0))

static inline int
thumb_check_condition(uint32_t condition)
{
	if (likely((condition & 0x0f) == 0x0e)) {
		return 1;
	} else {
		return ARM_ConditionMap[CONDITION_INDEX(condition, REG_CPSR)];
	}
}

static inline uint32_t
sub_carry(uint32_t op1, uint32_t op2, uint32_t result)
{
	if (((ISNEG(op1) && ISNOTNEG(op2))
	     || (ISNEG(op1) && ISNOTNEG(result))
	     || (ISNOTNEG(op2) && ISNOTNEG(result)))) {
		return FLAG_C;
	} else {
		return 0;
	}
}

static inline uint32_t
add_carry(uint32_t op1, uint32_t op2, uint32_t result)
{
	if (((ISNEG(op1) && ISNEG(op2))
	     || (ISNEG(op1) && ISNOTNEG(result))
	     || (ISNEG(op2) && ISNOTNEG(result)))) {
		return FLAG_C;
	} else {
		return 0;
	}
}

static inline uint32_t
sub_overflow(uint32_t op1, uint32_t op2, uint32_t result)
{
	if ((ISNOTNEG(op1) && ISNEG(op2) && ISNEG(result)) ||
	    (ISNEG(op1) && ISNOTNEG(op2) && ISNOTNEG(result))) {
		return FLAG_V;
	} else {
		return 0;
	}
}

static inline uint32_t
add_overflow(uint32_t op1, uint32_t op2, uint32_t result)
{
	if ((ISNEG(op1) && ISNEG(op2) && ISNOTNEG(result))
	    || (ISNOTNEG(op1) && ISNOTNEG(op2) && ISNEG(result))) {
		return FLAG_V;
	} else {
		return 0;
	}
}

/**
 ****************************************************************
 * \fn void th_adc();
 * Add two registers with carry.
 * v1
 ****************************************************************
 */
void
th_adc()
{
	uint32_t icode = ICODE;
	int rm, rd;
	uint32_t carry;
	uint32_t cpsr = REG_CPSR;
	uint32_t Rm, Rd, result;
	rd = icode & 7;
	rm = (icode >> 3) & 7;
	Rm = Thumb_ReadReg(rm);
	Rd = Thumb_ReadReg(rd);
	carry = cpsr & FLAG_C;
	result = Rd + Rm + !!carry;
	cpsr &= ~(FLAG_N | FLAG_Z | FLAG_C | FLAG_V);
	if (ISNEG(result)) {
		cpsr |= FLAG_N;
	} else if (result == 0) {
		cpsr |= FLAG_Z;
	}
	cpsr |= add_carry(Rd, Rm, result);
	cpsr |= add_overflow(Rd, Rm, result);
	REG_CPSR = cpsr;
	Thumb_WriteReg(result, rd);
	dbgprintf("Thumb adc not tested\n");
}

/**
 *********************************************************
 * \fn void th_add_1();
 * v1 
 *********************************************************
 */
void
th_add_1()
{
	int rn, rd;
	uint32_t icode = ICODE;
	uint32_t cpsr = REG_CPSR;
	uint32_t immed3;
	uint32_t Rn, Rd;
	rd = icode & 7;
	rn = (icode >> 3) & 7;
	immed3 = (ICODE >> 6) & 7;
	Rn = Thumb_ReadReg(rn);
	Rd = Rn + immed3;
	cpsr &= ~(FLAG_N | FLAG_Z | FLAG_C | FLAG_V);
	if (ISNEG(Rd)) {
		cpsr |= FLAG_N;
	} else if (Rd == 0) {
		cpsr |= FLAG_Z;
	}
	cpsr |= add_carry(Rn, immed3, Rd);
	cpsr |= add_overflow(Rn, immed3, Rd);
	REG_CPSR = cpsr;
	Thumb_WriteReg(Rd, rd);
	dbgprintf("Thumb add_1 not tested\n");
}

/**
 *************************************************************
 * \fn void th_add_2(void)
 * Add an 8 Bit immediate to a register.
 * v1
 *************************************************************
 */
void
th_add_2()
{
	int rd;
	uint32_t icode = ICODE;
	uint32_t cpsr = REG_CPSR;
	uint32_t immed8;
	uint32_t Rd;
	uint32_t result;
	immed8 = (icode & 0xff);
	rd = (icode >> 8) & 7;
	Rd = Thumb_ReadReg(rd);
	result = Rd + immed8;
	cpsr &= ~(FLAG_N | FLAG_Z | FLAG_C | FLAG_V);
	if (ISNEG(result)) {
		cpsr |= FLAG_N;
	} else if (result == 0) {
		cpsr |= FLAG_Z;
	}
	cpsr |= add_carry(Rd, immed8, result);
	cpsr |= add_overflow(Rd, immed8, result);
	REG_CPSR = cpsr;
	Thumb_WriteReg(result, rd);
	dbgprintf("Thumb add_2 not tested\n");
}

/**
 *****************************************************
 * \fn void th_add_3();
 * Add two registers and store the result in a third.
 * v1
 *****************************************************
 */
void
th_add_3()
{
	int rd, rn, rm;
	uint32_t icode = ICODE;
	uint32_t cpsr = REG_CPSR;
	uint32_t Rd, Rm, Rn;
	rd = icode & 7;
	rn = (icode >> 3) & 7;
	rm = (icode >> 6) & 7;
	Rn = Thumb_ReadReg(rn);
	Rm = Thumb_ReadReg(rm);
	Rd = Rn + Rm;
	cpsr &= ~(FLAG_N | FLAG_Z | FLAG_C | FLAG_V);
	if (ISNEG(Rd)) {
		cpsr |= FLAG_N;
	} else if (Rd == 0) {
		cpsr |= FLAG_Z;
	}
	cpsr |= add_carry(Rn, Rm, Rd);
	cpsr |= add_overflow(Rn, Rm, Rd);
	REG_CPSR = cpsr;
	Thumb_WriteReg(Rd, rd);
	dbgprintf("Thumb add_3 not tested\n");
}

/**
 ******************************************************* 
 * \fn void th_add_4() 
 * Add High registers.
 * v1
 ******************************************************* 
 */
void
th_add_4()
{
	int rd, rm;
	uint32_t icode = ICODE;
	uint32_t Rd, Rm;
	uint32_t result;
	rd = (icode & 7) | ((icode >> 4) & 8);
	rm = (icode >> 3) & 0xf;
	Rd = Thumb_ReadHighReg(rd);
	Rm = Thumb_ReadHighReg(rm);
	result = Rd + Rm;
	Thumb_WriteReg(result, rd);
	dbgprintf("Thumb add_4 not tested\n");
}

/**
 *****************************************************************
 * \fn void th_add_5(); 
 * Add a shifted immediate to the PC and store it in a register.
 * v1
 *****************************************************************
 */
void
th_add_5()
{
	int rd = (ICODE >> 8) & 7;
	uint32_t immed_8 = ICODE & 0xff;
	uint32_t result;
	result = (THUMB_GET_NNIA & 0xfffffffc) + (immed_8 << 2);
	Thumb_WriteReg(result, rd);
	dbgprintf("Thumb add_5 not tested\n");
}

/**
 *****************************************************************
 * \fn void th_add_6() 
 * Add a shifted immediate value to the SP and store the 
 * result in a register.
 * v1
 *****************************************************************
 */
void
th_add_6()
{
	int rd = (ICODE >> 8) & 7;
	uint32_t immed_8 = ICODE & 0xff;
	uint32_t result;
	result = Thumb_ReadReg(13) + (immed_8 << 2);
	Thumb_WriteReg(result, rd);
	dbgprintf("Thumb add_6 not tested\n");
}

/**
 ***************************************************************
 * \fn void th_add7()
 * Add a shifted immediate to the Stackpointer.
 * v1 
 ***************************************************************
 */
void
th_add_7()
{
	uint32_t immed_7 = ICODE & 0x7f;
	uint32_t result;
	result = Thumb_ReadReg(13) + (immed_7 << 2);
	Thumb_WriteReg(result, 13);
	dbgprintf("Thumb add_7 not tested\n");

}

/**
 ***************************************************************
 * \fn void th_and()
 * And's two Registers and stores the result in one of them.
 * v1
 ***************************************************************
 */
void
th_and()
{
	uint32_t cpsr = REG_CPSR;
	int rd = ICODE & 7;
	int rm = (ICODE >> 3) & 7;
	uint32_t Rd, Rm;
	Rm = Thumb_ReadReg(rm);
	Rd = Thumb_ReadReg(rd);
	Rd = Rd & Rm;
	cpsr &= ~(FLAG_N | FLAG_Z);
	if (ISNEG(Rd)) {
		cpsr |= FLAG_N;
	} else if (Rd == 0) {
		cpsr |= FLAG_Z;
	}
	REG_CPSR = cpsr;
	Thumb_WriteReg(Rd, rd);
	dbgprintf("Thumb and not implemented\n");
}

/**
 **************************************************************
 * \fn void th_asr_1(); 
 * Arithmetic shift right by an immediate.
 * v1
 **************************************************************
 */
void
th_asr_1()
{
	uint32_t cpsr = REG_CPSR;
	int rd = ICODE & 7;
	int rm = (ICODE >> 3) & 7;
	uint32_t immed_5 = (ICODE >> 6) & 0x1f;
	int32_t Rd, Rm;
	cpsr &= ~(FLAG_N | FLAG_Z);
	Rm = Thumb_ReadReg(rm);
	if (immed_5 == 0) {
		if ((Rm >> 31) & 1) {
			cpsr |= FLAG_C;
			Rd = 0xffffffff;
		} else {
			cpsr &= ~FLAG_C;
			Rd = 0;
		}
	} else {
		if ((Rm >> (immed_5 - 1)) & 1) {
			cpsr |= FLAG_C;
		} else {
			cpsr &= ~FLAG_C;
		}
		Rd = Rm >> immed_5;
	}
	if (ISNEG(Rd)) {
		cpsr |= FLAG_N;
	} else if (Rd == 0) {
		cpsr |= FLAG_Z;
	}
	REG_CPSR = cpsr;
	Thumb_WriteReg(Rd, rd);
	dbgprintf("Thumb asr_1 not implemented\n");
}

/**
 ****************************************************************
 * \fn void th_asr_2(); 
 * Arithmetic shift right by a register value.
 * v1
 ****************************************************************
 */
void
th_asr_2()
{
	uint32_t cpsr = REG_CPSR;
	int rd = ICODE & 7;
	int rs = (ICODE >> 3) & 7;
	uint32_t Rs;
	int32_t Rd;
	cpsr &= ~(FLAG_N | FLAG_Z);
	Rs = Thumb_ReadReg(rs) & 0xff;
	Rd = Thumb_ReadReg(rd);
	if (Rs == 0) {
		/* Do nothing */
	} else if (Rs < 32) {
		if ((Rd >> (Rs - 1)) & 1) {
			cpsr |= FLAG_C;
		} else {
			cpsr &= ~FLAG_C;
		}
		Rd = Rd >> Rs;
	} else {
		if ((Rd >> 31) & 1) {
			Rd = 0xffffffff;
			cpsr |= FLAG_C;
		} else {
			Rd = 0;
			cpsr &= ~FLAG_C;
		}
	}
	if (ISNEG(Rd)) {
		cpsr |= FLAG_N;
	} else if (Rd == 0) {
		cpsr |= FLAG_Z;
	}
	REG_CPSR = cpsr;
	Thumb_WriteReg(Rd, rd);
	dbgprintf("Thumb asr_2 not tested\n");
}

/**
 *********************************************************
 * \fn void th_b_1(); 
 * Branch conditional by a shifted 8 Bit signed immediate.
 * v1
 *********************************************************
 */
void
th_b_1()
{
	int32_t offset;
	uint32_t pc;
	int cond = (ICODE >> 8) & 0xf;
	if (thumb_check_condition(cond)) {
		offset = ((int32_t) (int8_t) (ICODE & 0xff)) << 1;
		pc = THUMB_GET_NNIA + offset;
		ARM_SET_NIA(pc);
	}
	dbgprintf("Thumb b_1 not tested\n");
}

/**
 *************************************************************
 * \fn void th_b_2(); 
 * Branch unconditional by a signed 11 bit immediate.
 * v1
 *************************************************************
 */
void
th_b_2()
{
	int32_t immed;
	immed = ((int32_t) ((ICODE & 0x7ff) << 21)) >> 20;
	ARM_SET_NIA(THUMB_GET_NNIA + immed);
	dbgprintf("Thumb b_2 not tested\n");
}

/*
 *********************************************************
 * \fn void th_bic(); 
 * Bit clear (and not).
 * v1
 *********************************************************
 */
void
th_bic()
{
	int rd = ICODE & 7;
	int rm = (ICODE >> 3) & 7;
	uint32_t Rd, Rm;
	uint32_t cpsr = REG_CPSR;
	cpsr &= ~(FLAG_N | FLAG_Z);
	Rd = Thumb_ReadReg(rd);
	Rm = Thumb_ReadReg(rm);
	Rd = Rd & ~Rm;
	if (ISNEG(Rd)) {
		cpsr |= FLAG_N;
	} else if (Rd == 0) {
		cpsr |= FLAG_Z;
	}
	REG_CPSR = cpsr;
	Thumb_WriteReg(Rd, rd);
	dbgprintf("Thumb bic not implemented\n");
}

void
th_bkpt()
{
	ARM_Break();
}

/**
 *************************************************************************
 * \fn void th_bl_blx(); 
 * long branch with link consiting of two instructions.
 * v1
 *************************************************************************
 */
void
th_bl_blx()
{
	uint32_t icode = ICODE;
	int h = (icode >> 11) & 3;
	int32_t offset;
	uint32_t lr;
	switch (h) {
	    case 2:
		    offset = ((int32_t) ((icode & 0x7ff) << 21)) >> 9;
		    REG_LR = THUMB_GET_NNIA + offset;
		    break;

	    case 3:
		    lr = REG_LR;
		    offset = (icode & 0x7ff);
		    REG_LR = ARM_NIA | 1;
		    ARM_SET_NIA(lr + (offset << 1));
		    break;

	    case 1:
		    lr = REG_LR;
		    offset = (icode & 0x7ff);
		    REG_LR = ARM_NIA | 1;
		    ARM_SET_NIA((lr + (offset << 1)) & 0xfffffffc);
		    REG_CPSR &= ~FLAG_T;
		    ARM_RestartIdecoder();
		    break;
	}
	dbgprintf("Thumb bl_blx\n");
}

/**
 ***********************************************************
 * \fn void th_blx_2() 
 * Branch with link to an address in a register.
 * v1
 ***********************************************************
 */
void
th_blx_2()
{
	int rm;
	uint32_t Rm;
	uint32_t icode = ICODE;
	rm = (icode >> 3) & 0xf;
	/* Use of R15 is illegal */
	Rm = Thumb_ReadReg(rm);
	REG_LR = ARM_NIA | 1;
	ARM_SET_NIA(Rm & 0xfffffffe);
	dbgprintf("Thumb blx_2\n");
	if (!(Rm & 1)) {
		/* Leave Thumb mode */
		REG_CPSR &= ~FLAG_T;
		ARM_RestartIdecoder();
	}
}

/**
 **********************************************************
 * \fn void th_bx(); 
 * Branch and exchange is used for jump between ARM and
 * Thumb mode.
 * v1
 **********************************************************
 */
void
th_bx()
{
	int rm;
	uint32_t Rm;
	uint32_t icode = ICODE;
	rm = (icode >> 3) & 0xf;
	Rm = Thumb_ReadHighReg(rm);
	ARM_SET_NIA(Rm & 0xfffffffe);
	dbgprintf("BX to reg %d: %08x\n", rm, Rm);
	if (!(Rm & 1)) {
		/* Leave Thumb mode */
		REG_CPSR &= ~FLAG_T;
		ARM_RestartIdecoder();
	}
}

/**
 ********************************************************
 * \fn void th_cmn(); 
 * Compare Negative.
 * v1
 ********************************************************
 */
void
th_cmn()
{
	int rn = ICODE & 7;
	int rm = (ICODE >> 3) & 7;
	uint32_t Rn = Thumb_ReadReg(rn);
	uint32_t Rm = Thumb_ReadReg(rm);
	uint32_t cpsr = REG_CPSR;
	uint32_t result;
	result = Rn + Rm;

	cpsr &= ~(FLAG_N | FLAG_Z | FLAG_C | FLAG_V);
	if (ISNEG(result)) {
		cpsr |= FLAG_N;
	} else if (result == 0) {
		cpsr |= FLAG_Z;
	}
	cpsr |= add_carry(Rn, Rm, result);
	cpsr |= add_overflow(Rn, Rm, result);
	REG_CPSR = cpsr;
	dbgprintf("Thumb cmn not tested\n");
}

/**
 ********************************************************
 * \fn void th_cmp_1();
 * Compare Register with immediate 8 bit value.
 * v1
 ********************************************************
 */
void
th_cmp_1()
{
	int rn = (ICODE >> 8) & 0x7;
	uint32_t immed_8 = ICODE & 0xff;
	uint32_t Rn, result;
	uint32_t cpsr = REG_CPSR;
	Rn = Thumb_ReadReg(rn);
	result = Rn - immed_8;
	cpsr &= ~(FLAG_N | FLAG_Z | FLAG_C | FLAG_V);
	if (ISNEG(result)) {
		cpsr |= FLAG_N;
	} else if (result == 0) {
		cpsr |= FLAG_Z;
	}
	cpsr |= sub_carry(Rn, immed_8, result);
	cpsr |= sub_overflow(Rn, immed_8, result);
	REG_CPSR = cpsr;
	dbgprintf("Thumb cmp_1 not tested\n");
}

/**
 *********************************************************
 * \fn void th_cmp_2(); 
 * Compare two register values.
 * v1
 *********************************************************
 */
void
th_cmp_2()
{
	int rn = ICODE & 7;
	int rm = (ICODE >> 3) & 7;
	uint32_t Rm, Rn;
	uint32_t result;
	uint32_t cpsr = REG_CPSR;
	Rm = Thumb_ReadReg(rm);
	Rn = Thumb_ReadReg(rn);
	result = Rn - Rm;
	cpsr &= ~(FLAG_N | FLAG_Z | FLAG_C | FLAG_V);
	if (ISNEG(result)) {
		cpsr |= FLAG_N;
	} else if (result == 0) {
		cpsr |= FLAG_Z;
	}
	cpsr |= sub_carry(Rn, Rm, result);
	cpsr |= sub_overflow(Rn, Rm, result);
	REG_CPSR = cpsr;
	dbgprintf("Thumb cmp_2 not tested\n");
}

/**
 *******************************************************
 * \fn void th_cmp_3();
 * Compare high registers.
 * v1
 *******************************************************
 */
void
th_cmp_3()
{
	int rn = (ICODE & 7) | ((ICODE >> 4) & 0x8);
	int rm = (ICODE >> 3) & 0xf;
	uint32_t Rm, Rn;
	uint32_t result;
	uint32_t cpsr = REG_CPSR;
	Rm = Thumb_ReadHighReg(rm);
	Rn = Thumb_ReadHighReg(rn);
	result = Rn - Rm;
	cpsr &= ~(FLAG_N | FLAG_Z | FLAG_C | FLAG_V);
	if (ISNEG(result)) {
		cpsr |= FLAG_N;
	} else if (result == 0) {
		cpsr |= FLAG_Z;
	}
	cpsr |= sub_carry(Rn, Rm, result);
	cpsr |= sub_overflow(Rn, Rm, result);
	REG_CPSR = cpsr;
	dbgprintf("Thumb cmp_3 not tested\n");
}

/**
 ******************************************************
 * \fn void th_eor(); 
 * Exclusive or of tho register values.
 * v1
 ******************************************************
 */
void
th_eor()
{
	uint32_t cpsr = REG_CPSR;
	int rd = ICODE & 7;
	int rm = (ICODE >> 3) & 7;
	uint32_t Rd, Rm;
	Rm = Thumb_ReadReg(rm);
	Rd = Thumb_ReadReg(rd);
	Rd = Rd ^ Rm;
	cpsr &= ~(FLAG_N | FLAG_Z);
	if (ISNEG(Rd)) {
		cpsr |= FLAG_N;
	} else if (Rd == 0) {
		cpsr |= FLAG_Z;
	}
	REG_CPSR = cpsr;
	Thumb_WriteReg(Rd, rd);
	dbgprintf("Thumb eor not tested\n");
}

/*
 *****************************************************************
 * \fn void th_ldmia_base_restored(); 
 * Load multiple.
 * Todo: Check if base register is also restored if Rn is in
 *       load list. 
 * v1
 *****************************************************************
 */
void
th_ldmia_base_restored()
{
	uint32_t register_list = (ICODE & 0xff);
	int rn = (ICODE >> 8) & 0x7;
	int i;
	uint32_t Rn, value;
	uint32_t Rn_new = 0;	/* Make compiler happy */
	Rn = Thumb_ReadReg(rn);
	for (i = 0; i < 8; i++) {
		if (register_list & (1 << i)) {
			if (i == rn) {
				Rn_new = MMU_Read32(Rn);
			} else {
				value = MMU_Read32(Rn);
				Thumb_WriteReg(value, i);
			}
			Rn += 4;
		}
	}
	if (register_list & (1 << rn)) {
		Thumb_WriteReg(Rn_new, rn);
	} else {
		Thumb_WriteReg(Rn, rn);
	}
	dbgprintf("Thumb ldmia base restored not tested\n");
}

void
th_ldmia_base_updated()
{
	uint32_t register_list = (ICODE & 0xff);
	int rn = (ICODE >> 8) & 0x7;
	int i;
	uint32_t Rn, value;
	Rn = Thumb_ReadReg(rn);
	for (i = 0; i < 8; i++) {
		if (register_list & (1 << i)) {
			value = MMU_Read32(Rn);
			Thumb_WriteReg(value, i);
			Rn += 4;
			/* is this done every step or all at once ? */
			if (!(register_list & (1 << rn))) {
				Thumb_WriteReg(Rn, rn);
			}
		}
	}
	dbgprintf("Thumb ldmia base_updated not tested\n");
}

/**
 *************************************************************
 * \fn void th_ldr_1(); 
 * Load a register from memory.
 * v1
 *************************************************************
 */
void
th_ldr_1()
{
	int rd = ICODE & 7;
	int rn = (ICODE >> 3) & 7;
	uint32_t Rn, Rd;
	uint32_t immed5 = (ICODE >> 6) & 0x1f;
	uint32_t addr;
	Rn = Thumb_ReadReg(rn);
	addr = Rn + (immed5 << 2);
	if (likely((addr & 3) == 0)) {
		Rd = MMU_Read32(addr);
		Thumb_WriteReg(Rd, rd);
	} else {
		MMU_AlignmentException(addr);
	}
	dbgprintf("Thumb ldr_1 not tested\n");
}

/**
 ***********************************************************
 * \fn void th_ldr_2();
 * Load a register from an address calculated from
 * two registers. Used for base + large offset loading.
 * v1
 ***********************************************************
 */
void
th_ldr_2()
{
	int rd = ICODE & 7;
	int rn = (ICODE >> 3) & 7;
	int rm = (ICODE >> 6) & 7;
	uint32_t Rd, Rm, Rn, addr;
	Rn = Thumb_ReadReg(rn);
	Rm = Thumb_ReadReg(rm);
	addr = Rn + Rm;
	if (likely((addr & 3) == 0)) {
		Rd = MMU_Read32(addr);
		Thumb_WriteReg(Rd, rd);
	} else {
		MMU_AlignmentException(addr);
	}
	dbgprintf("Thumb ldr_2 not tested\n");
}

/**
 ***************************************************************
 * \fn void th_ldr_3(); 
 * Load a register from PC + immediate offset. 
 * v1
 ***************************************************************
 */
void
th_ldr_3()
{
	int rd = (ICODE >> 8) & 7;
	uint32_t immed_8 = (ICODE & 0xff);
	uint32_t Rd, addr;
	addr = (THUMB_GET_NNIA & ~UINT32_C(3)) + (immed_8 << 2);
	Rd = MMU_Read32(addr);
	Thumb_WriteReg(Rd, rd);
	dbgprintf("Thumb ldr_3 not tested\n");
}

/**
 **************************************************************
 * \fn void th_ldr_4(); 
 * Load a register from SP as base + an immediate offset.
 * v1
 **************************************************************
 */
void
th_ldr_4()
{
	int rd = (ICODE >> 8) & 7;
	uint32_t immed_8 = (ICODE & 0xff);
	uint32_t Rd, addr;
	addr = Thumb_ReadReg(13) + (immed_8 << 2);
	if (likely((addr & 3) == 0)) {
		Rd = MMU_Read32(addr);
		Thumb_WriteReg(Rd, rd);
	} else {
		MMU_AlignmentException(addr);
	}
	dbgprintf("Thumb ldr_4 not tested\n");
}

/**
 *************************************************************
 * \fn void th_ldrb_1(); 
 * Load a Byte to a register. The address is the
 * sum of a register value and a five bit immediate.
 * v1
 *************************************************************
 */
void
th_ldrb_1()
{
	int rd = ICODE & 7;
	int rn = (ICODE >> 3) & 7;
	uint32_t Rn, Rd;
	uint32_t immed5 = (ICODE >> 6) & 0x1f;
	uint32_t addr;
	Rn = Thumb_ReadReg(rn);
	addr = Rn + immed5;
	Rd = MMU_Read8(addr);
	Thumb_WriteReg(Rd, rd);
	dbgprintf("Thumb ldrb_1 not tested\n");
}

/**
 ************************************************************
 * \fn void th_ldrb_2();
 * Load a Byte from memory into a register.
 * The address is the sum of two register values.
 * v1
 ************************************************************
 */
void
th_ldrb_2()
{
	int rd = ICODE & 7;
	int rn = (ICODE >> 3) & 7;
	int rm = (ICODE >> 6) & 7;
	uint32_t Rd, Rm, Rn, addr;
	Rn = Thumb_ReadReg(rn);
	Rm = Thumb_ReadReg(rm);
	addr = Rn + Rm;
	Rd = MMU_Read8(addr);
	Thumb_WriteReg(Rd, rd);
	dbgprintf("Thumb ldrb_2 not tested\n");
}

/**
 ************************************************************
 * \fn void th_ldrh_1(); 
 * Load a halfword from memory into a register.
 * The address is the sum of a register value and a 
 * 5 Bit immediate.
 * v1
 ************************************************************
 */
void
th_ldrh_1()
{
	int rd = ICODE & 7;
	int rn = (ICODE >> 3) & 7;
	uint32_t Rn, Rd;
	uint32_t immed5 = (ICODE >> 6) & 0x1f;
	uint32_t addr;
	Rn = Thumb_ReadReg(rn);
	addr = Rn + (immed5 << 1);
	if (likely((addr & 1) == 0)) {
		Rd = MMU_Read16(addr);
		Thumb_WriteReg(Rd, rd);
	} else {
		MMU_AlignmentException(addr);
	}
	dbgprintf("Thumb ldrh_1 not tested\n");
}

/**
 ***********************************************************
 * \fn void th_ldrh_2(); 
 * Load a halfword from memory. The address is the sum
 * of two register values. 
 ***********************************************************
 */
void
th_ldrh_2()
{
	int rd = ICODE & 7;
	int rn = (ICODE >> 3) & 7;
	int rm = (ICODE >> 6) & 7;
	uint32_t Rd, Rm, Rn, addr;
	Rn = Thumb_ReadReg(rn);
	Rm = Thumb_ReadReg(rm);
	addr = Rn + Rm;
	if (likely((addr & 1) == 0)) {
		Rd = MMU_Read16(addr);
		Thumb_WriteReg(Rd, rd);
	} else {
		MMU_AlignmentException(addr);
	}
	dbgprintf("Thumb ldrh_2 not tested\n");
}

/**
 *****************************************************************
 * \fn void th_ldrsb(); 
 * Load a Byte from memory, sign extend it to 32 Bit and
 * store it in a register. The address is the sum of
 * two register values. 
 * v1
 *****************************************************************
 */
void
th_ldrsb()
{
	int rd = ICODE & 7;
	int rn = (ICODE >> 3) & 7;
	int rm = (ICODE >> 6) & 7;
	uint32_t Rd, Rm, Rn, addr;
	Rn = Thumb_ReadReg(rn);
	Rm = Thumb_ReadReg(rm);
	addr = Rn + Rm;
	Rd = (int32_t) (int8_t) MMU_Read8(addr);
	Thumb_WriteReg(Rd, rd);
	dbgprintf("Thumb ldrsb not tested\n");
}

/**
 ****************************************************************
 * \fn void th_ldrsh()
 * Load a halfword from memory, sign extend it to 32 Bits and
 * store it in a register. The address is the sum of two 
 * register values.
 * v1
 ****************************************************************
 */
void
th_ldrsh()
{
	int rd = ICODE & 7;
	int rn = (ICODE >> 3) & 7;
	int rm = (ICODE >> 6) & 7;
	uint32_t Rd, Rm, Rn, addr;
	Rn = Thumb_ReadReg(rn);
	Rm = Thumb_ReadReg(rm);
	addr = Rn + Rm;
	if (likely((addr & 1) == 0)) {
		Rd = (int32_t) (int16_t) MMU_Read16(addr);
		Thumb_WriteReg(Rd, rd);
	} else {
		MMU_AlignmentException(addr);
	}
	dbgprintf("Thumb ldrsh not tested\n");
}

/**
 *******************************************************************
 * \fn void th_lsl_1() 
 * Logical shift left of a register by an immediate.
 * v1
 *******************************************************************
 */
void
th_lsl_1()
{
	int rd = ICODE & 7;
	int rm = (ICODE >> 3) & 7;
	uint32_t immed_5 = (ICODE >> 6) & 0x1f;
	uint32_t Rd, Rm;
	uint32_t cpsr = REG_CPSR;
	Rm = Thumb_ReadReg(rm);
	Rd = Rm << immed_5;
	if (immed_5 != 0) {
		if ((Rm >> (32 - immed_5)) & 1) {
			cpsr |= FLAG_C;
		} else {
			cpsr &= ~FLAG_C;
		}
	}
	Thumb_WriteReg(Rd, rd);
	cpsr &= ~(FLAG_N | FLAG_Z);
	if (ISNEG(Rd)) {
		cpsr |= FLAG_N;
	} else if (Rd == 0) {
		cpsr |= FLAG_Z;
	}
	REG_CPSR = cpsr;
	dbgprintf("Thumb lsl_1: rd %d, rm %d, imm %d result %08x->%08x not tested\n", rd, rm,
		  immed_5, Rm, Rd);
}

/**
 ***********************************************************************
 * \fn void th_lsl_2(); 
 * Logical shift left a register value by a register value.
 * The maximal shift is 255 because only the lower 8 Bits of Rs are used.
 * v1
 ***********************************************************************
 */
void
th_lsl_2()
{
	uint32_t cpsr = REG_CPSR;
	int rd = ICODE & 7;
	int rs = (ICODE >> 3) & 7;
	uint32_t Rslow;
	uint32_t Rd;
	cpsr &= ~(FLAG_N | FLAG_Z);
	Rslow = Thumb_ReadReg(rs) & 0xff;
	Rd = Thumb_ReadReg(rd);
	if (Rslow == 0) {
		/* Do nothing */
	} else if (Rslow < 32) {
		if ((Rd >> (32 - Rslow)) & 1) {
			cpsr |= FLAG_C;
		} else {
			cpsr &= ~FLAG_C;
		}
		Rd = Rd << Rslow;
	} else if (Rslow == 32) {
		if (Rd & 1) {
			cpsr |= FLAG_C;
		} else {
			cpsr &= ~FLAG_C;
		}
		Rd = 0;
	} else {
		cpsr &= ~FLAG_C;
		Rd = 0;
	}
	if (ISNEG(Rd)) {
		cpsr |= FLAG_N;
	} else if (Rd == 0) {
		cpsr |= FLAG_Z;
	}
	REG_CPSR = cpsr;
	Thumb_WriteReg(Rd, rd);
	dbgprintf("Thumb lsl_2 not tested\n");
}

/**
 ******************************************************************
 * \fn void th_lsr_1(); 
 * Logical shift right of a register by a five bit immediate.
 * v1 
 ******************************************************************
 */
void
th_lsr_1()
{
	uint32_t cpsr = REG_CPSR;
	uint32_t immed_5 = (ICODE >> 6) & 0x1f;
	int rd = ICODE & 7;
	int rm = (ICODE >> 3) & 7;
	uint32_t Rd, Rm;
	cpsr &= ~(FLAG_N | FLAG_Z);
	Rm = Thumb_ReadReg(rm);
	if (immed_5 == 0) {
		if ((Rm >> 31) & 1) {
			cpsr |= FLAG_C;
		} else {
			cpsr &= ~FLAG_C;
		}
		Rd = 0;
	} else {
		if ((Rm >> (immed_5 - 1)) & 1) {
			cpsr |= FLAG_C;
		} else {
			cpsr &= ~FLAG_C;
		}
		Rd = Rm >> immed_5;
	}
	if (ISNEG(Rd)) {
		cpsr |= FLAG_N;
	} else if (Rd == 0) {
		cpsr |= FLAG_Z;
	}
	REG_CPSR = cpsr;
	Thumb_WriteReg(Rd, rd);
	dbgprintf("Thumb lsr_1 not tested\n");
}

/**
 *************************************************************
 * \fn void th_lsr_2() 
 * Logical shift right of a register by a register value.
 * The shift range is 0 - 255. Only the lowest byte
 * of Rs is used.
 * v1
 *************************************************************
 */
void
th_lsr_2()
{
	uint32_t cpsr = REG_CPSR;
	int rd = ICODE & 7;
	int rs = (ICODE >> 3) & 7;
	uint32_t Rslow;
	uint32_t Rd;
	cpsr &= ~(FLAG_N | FLAG_Z);
	Rslow = Thumb_ReadReg(rs) & 0xff;
	Rd = Thumb_ReadReg(rd);
	if (Rslow == 0) {
		/* Do nothing */
	} else if (Rslow < 32) {
		if ((Rd >> (Rslow - 1)) & 1) {
			cpsr |= FLAG_C;
		} else {
			cpsr &= ~FLAG_C;
		}
		Rd = Rd >> Rslow;
	} else if (Rslow == 32) {
		if ((Rd >> 31) & 1) {
			cpsr |= FLAG_C;
		} else {
			cpsr &= ~FLAG_C;
		}
		Rd = 0;
	} else {
		cpsr &= ~FLAG_C;
		Rd = 0;
	}
	if (ISNEG(Rd)) {
		cpsr |= FLAG_N;
	} else if (Rd == 0) {
		cpsr |= FLAG_Z;
	}
	REG_CPSR = cpsr;
	Thumb_WriteReg(Rd, rd);
	dbgprintf("Thumb lsr_2 not tested\n");
}

/**
 ************************************************************
 * \fn void th_mov_2(); 
 * Move an eight bit immediate to a register. 
 * v1
 ************************************************************
 */
void
th_mov_1()
{
	uint32_t immed_8 = ICODE & 0xff;
	int rd = (ICODE >> 8) & 7;
	uint32_t cpsr = REG_CPSR;
	uint32_t Rd;
	Rd = immed_8;
	cpsr &= ~(FLAG_N | FLAG_Z);
	/* Can not be negative */
	if (Rd == 0) {
		cpsr |= FLAG_Z;
	}
	REG_CPSR = cpsr;
	Thumb_WriteReg(Rd, rd);
	dbgprintf("Thumb mov_1: written 0x%02x to R%d\n", Rd, rd);
}

/*
 *******************************************************************
 * \fn void th_mov_3(); 
 * Move a value from one register to another register.
 * This is the high register variant.
 * v1
 *******************************************************************
 */
void
th_mov_3()
{
	int rd = (ICODE & 7) | ((ICODE >> 4) & 8);
	int rm = (ICODE >> 3) & 0xf;
	Thumb_WriteReg(Thumb_ReadHighReg(rm), rd);
	dbgprintf("Thumb mov_3 not tested\n");
}

/**
 ********************************************************************
 * \fn th_mul()
 * Multiply two registers.
 * v1
 ********************************************************************
 */
void
th_mul()
{
	int rd = ICODE & 7;
	int rm = (ICODE >> 3) & 7;
	uint32_t Rm, Rd;
	uint32_t cpsr = REG_CPSR;
	Rm = Thumb_ReadReg(rm);
	Rd = Thumb_ReadReg(rd);
	Rd = Rd * Rm;
	Thumb_WriteReg(Rd, rd);
	cpsr &= ~(FLAG_N | FLAG_Z);
	if (ISNEG(Rd)) {
		cpsr |= FLAG_N;
	} else if (Rd == 0) {
		cpsr |= FLAG_Z;
	}
	REG_CPSR = cpsr;
	dbgprintf("Thumb mul not tested\n");
}

/**
 ************************************************************
 * \fn th_mvn()
 * Move the bitwise NOT of a register value to another one.
 * v1
 ************************************************************
 */
void
th_mvn()
{
	int rd = (ICODE & 7);
	int rm = (ICODE >> 3) & 7;
	uint32_t Rd, Rm;
	uint32_t cpsr = REG_CPSR;
	Rm = Thumb_ReadReg(rm);
	Rd = ~Rm;
	Thumb_WriteReg(Rd, rd);
	cpsr &= ~(FLAG_N | FLAG_Z);
	if (ISNEG(Rd)) {
		cpsr |= FLAG_N;
	} else if (Rd == 0) {
		cpsr |= FLAG_Z;
	}
	REG_CPSR = cpsr;
	dbgprintf("Thumb mvn not tested\n");
}

/**
 *******************************************************************
 * \fn void th_neg();
 * Negate the value of a register and store the result in a 
 * second register.
 * v1
 *******************************************************************
 */
void
th_neg()
{
	int rd = (ICODE & 7);
	int rm = (ICODE >> 3) & 7;
	uint32_t Rd, Rm;
	uint32_t cpsr = REG_CPSR;
	Rm = Thumb_ReadReg(rm);
	Rd = 0 - Rm;
	Thumb_WriteReg(Rd, rd);
	cpsr &= ~(FLAG_N | FLAG_Z | FLAG_C | FLAG_V);
	if (ISNEG(Rd)) {
		cpsr |= FLAG_N;
	} else if (Rd == 0) {
		cpsr |= FLAG_Z;
	}
	cpsr |= sub_carry(0, Rm, Rd);
	cpsr |= sub_overflow(0, Rm, Rd);
	REG_CPSR = cpsr;
	dbgprintf("Thumb neg not implemented\n");
}

/**
 ********************************************************************
 * \fn void th_orr();
 * Logical or of two register values.
 * v1
 ********************************************************************
 */
void
th_orr()
{
	uint32_t cpsr = REG_CPSR;
	int rd = ICODE & 7;
	int rm = (ICODE >> 3) & 7;
	uint32_t Rd, Rm;
	Rm = Thumb_ReadReg(rm);
	Rd = Thumb_ReadReg(rd);
	Rd = Rd | Rm;
	cpsr &= ~(FLAG_N | FLAG_Z);
	if (ISNEG(Rd)) {
		cpsr |= FLAG_N;
	} else if (Rd == 0) {
		cpsr |= FLAG_Z;
	}
	REG_CPSR = cpsr;
	Thumb_WriteReg(Rd, rd);
	dbgprintf("Thumb orr not tested\n");
}

/**
 *****************************************************************
 * \fn void th_pop_archv5();
 * Pop registers from a list (R0-R7) and eventually the PC (R15). 
 * v1
 *****************************************************************
 */
void
th_pop_archv5()
{
	uint32_t register_list = (ICODE & 0xff);
	int R = !!(ICODE & 0x100);
	int i;
	uint32_t value;
	uint32_t addr = Thumb_ReadReg(13);
	for (i = 0; i < 8; i++) {
		if (register_list & (1 << i)) {
			value = MMU_Read32(addr);
			//dbgprintf("Popped R%d from %08x: %08x\n",i,addr,value);
			Thumb_WriteReg(value, i);
			addr += 4;
		}
	}
	if (R) {
		value = MMU_Read32(addr);
		addr += 4;
		Thumb_WriteReg(addr, 13);
		if (value & 1) {
			ARM_SET_NIA(value & 0xfffffffe);
			/* Flag T is already set */
		} else {
			/* Manual says unpredictable if pc is not aligned */
			ARM_SET_NIA(value & 0xfffffffc);
			REG_CPSR &= ~FLAG_T;
			ARM_RestartIdecoder();
		}
	} else {
		Thumb_WriteReg(addr, 13);
	}
	dbgprintf("Thumb pop not implemented\n");
}

/*
 ************************************************
 * No influence on T flag in arch < v4
 ************************************************
 */
void
th_pop_archv4()
{
	uint32_t register_list = (ICODE & 0xff);
	int R = !!(ICODE & 0x100);
	int i;
	uint32_t value;
	uint32_t addr = Thumb_ReadReg(13);
	for (i = 0; i < 8; i++) {
		if (register_list & (1 << i)) {
			value = MMU_Read32(addr);
			addr += 4;
		}
	}
	if (R) {
		value = MMU_Read32(addr);
		addr += 4;
		Thumb_WriteReg(addr, 13);
		ARM_SET_NIA(value & 0xfffffffe);
	} else {
		Thumb_WriteReg(addr, 13);
	}
	dbgprintf("Thumb pop not implemented\n");
}

/**
 *************************************************************
 * \fn void th_push(); 
 * Push a set of registers onto the stack. 
 *************************************************************
 */
void
th_push()
{
	uint32_t register_list = (ICODE & 0xff);
	uint32_t Sp;
	uint32_t addr;
	int R = !!(ICODE & 0x100);
	int i;
	Sp = Thumb_ReadReg(13);
	/* Shity method for counting bits */
	Sp -= SGLib_OnecountU8(register_list) << 2;
#if 0
	for (i = 0; i < 8; i++) {
		if (register_list & (1 << i)) {
			Sp -= 4;
		}
	}
#endif
	if (R) {
		Sp -= 4;
	}
	addr = Sp;
	for (i = 0; i < 8; i++) {
		if (register_list & (1 << i)) {
			MMU_Write32(Thumb_ReadReg(i), addr);
			addr += 4;
		}
	}
	if (R) {
		MMU_Write32(Thumb_ReadReg(14), addr);
		//dbgprintf("Pushed LR to addr %08x\n",addr);
	}
	Thumb_WriteReg(Sp, 13);
	dbgprintf("Thumb push not tested\n");
}

/**
 ***************************************************************
 * \fn th_ror();
 * Rotate a register right by a value in another register.
 * v1
 ***************************************************************
 */
void
th_ror()
{
	int rd = ICODE & 7;
	int rs = (ICODE >> 3) & 7;
	uint32_t Rd, Rs;
	uint32_t cpsr = REG_CPSR;
	Rd = Thumb_ReadReg(rd);
	Rs = Thumb_ReadReg(rs);
	if ((Rs & 0xff) == 0) {
		/* Do nothing */
	} else if ((Rs & 0x1f) == 0) {
		if ((Rd >> 31) & 1) {
			cpsr |= FLAG_C;
		} else {
			cpsr &= ~FLAG_C;
		}
	} else {
		if ((Rd >> ((Rs & 0x1f) - 1)) & 1) {
			cpsr |= FLAG_C;
		} else {
			cpsr &= ~FLAG_C;
		}
		Rd = (Rd >> (Rs & 0x1f)) | (Rd << (32 - (Rs & 0x1f)));
	}
	cpsr &= ~(FLAG_N | FLAG_Z);
	if (ISNEG(Rd)) {
		cpsr |= FLAG_N;
	} else if (Rd == 0) {
		cpsr |= FLAG_Z;
	}
	REG_CPSR = cpsr;
	Thumb_WriteReg(Rd, rd);
	dbgprintf("Thumb ror not tested\n");
}

/**
 ***********************************************************
 * \fn void th_sbc()
 * subtract with carry.
 * v1
 ***********************************************************
 */
void
th_sbc()
{
	uint32_t icode = ICODE;
	int rm, rd;
	uint32_t carry;
	uint32_t cpsr = REG_CPSR;
	uint32_t Rm, Rd, result;
	rm = (icode >> 3) & 0x7;
	rd = icode & 0x7;
	Rm = Thumb_ReadReg(rm);
	Rd = Thumb_ReadReg(rd);
	carry = cpsr & FLAG_C;
	result = Rd - Rm - !carry;
	cpsr &= ~(FLAG_N | FLAG_Z | FLAG_C | FLAG_V);
	if (ISNEG(result)) {
		cpsr |= FLAG_N;
	} else if (result == 0) {
		cpsr |= FLAG_Z;
	}
	cpsr |= sub_carry(Rd, Rm, result);
	cpsr |= sub_overflow(Rd, Rm, result);
	REG_CPSR = cpsr;
	Thumb_WriteReg(result, rd);
	dbgprintf("Thumb sbc not tested\n");
}

/**
 ************************************************************
 * \fn void th_stmia_base_restored(); 
 * Store multiple registers from a list in memory.
 * The base register is not updated before the operation
 * is not complete.
 * v1
 ************************************************************
 */
void
th_stmia_base_restored()
{
	int rn = (ICODE >> 8) & 7;
	uint32_t register_list = ICODE & 0xff;
	uint32_t Rn = Thumb_ReadReg(rn);
	int i;

	for (i = 0; i < 8; i++) {
		if (register_list & (1 << i)) {
			MMU_Write32(Thumb_ReadReg(i), Rn);
		}
		Rn += 4;
	}
	Thumb_WriteReg(Rn, rn);
	dbgprintf("Thumb stmia base restored not tested\n");
}

/**
 ************************************************************
 * \fn void th_str_1()
 * Store a register in memory. The address is calculated
 * from a register value and a five bit offset.
 * v1
 ************************************************************
 */
void
th_str_1()
{
	int rd = ICODE & 7;
	int rn = (ICODE >> 3) & 7;
	uint32_t Rn, Rd;
	uint32_t immed5 = (ICODE >> 6) & 0x1f;
	uint32_t addr;
	Rn = Thumb_ReadReg(rn);
	addr = Rn + (immed5 << 2);
	if (likely((addr & 3) == 0)) {
		Rd = Thumb_ReadReg(rd);
		MMU_Write32(Rd, addr);
	} else {
		MMU_AlignmentException(addr);
	}
	dbgprintf("Thumb str_1 not tested\n");
}

/**
 **********************************************************
 * \fn void th_str_2(); 
 * Store a register value in memory.
 * v1
 **********************************************************
 */
void
th_str_2()
{
	int rd = ICODE & 7;
	int rn = (ICODE >> 3) & 7;
	int rm = (ICODE >> 6) & 7;
	uint32_t Rd, Rm, Rn, addr;
	Rn = Thumb_ReadReg(rn);
	Rm = Thumb_ReadReg(rm);
	addr = Rn + Rm;
	if (likely((addr & 3) == 0)) {
		Rd = Thumb_ReadReg(rd);
		MMU_Write32(Rd, addr);
	} else {
		MMU_AlignmentException(addr);
	}
	dbgprintf("Thumb str_2 not implemented\n");
}

/**
 ************************************************************
 * \fn void th_str_3();
 * Read from memory using Stack + immediate as address.
 * v1
 ************************************************************
 */
void
th_str_3()
{
	int rd = (ICODE >> 8) & 7;
	uint32_t immed_8 = (ICODE & 0xff);
	uint32_t Rd, addr;
	addr = Thumb_ReadReg(13) + (immed_8 << 2);
	if (likely((addr & 3) == 0)) {
		Rd = Thumb_ReadReg(rd);
		MMU_Write32(Rd, addr);
	} else {
		MMU_AlignmentException(addr);
	}
	dbgprintf("Thumb str_3 not tested\n");
}

/*
 *********************************************************************
 * \fn void th_strb_1() 
 * Write a byte to memory. The address is: base register + immediate.
 * v1
 *********************************************************************
 */
void
th_strb_1()
{
	int rd = ICODE & 7;
	int rn = (ICODE >> 3) & 7;
	uint32_t Rn, Rd;
	uint32_t immed5 = (ICODE >> 6) & 0x1f;
	uint32_t addr;
	Rn = Thumb_ReadReg(rn);
	addr = Rn + immed5;
	Rd = Thumb_ReadReg(rd);
	MMU_Write8(Rd, addr);
	dbgprintf("Thumb strb_1 not implemented\n");
}

/**
 ***********************************************************************
 * \fn void th_strb_2(); 
 * Write a byte to memory. The address is calculated as the sum
 * of a base register and an offset register.
 * v1
 ***********************************************************************
 */
void
th_strb_2()
{
	int rd = ICODE & 7;
	int rn = (ICODE >> 3) & 7;
	int rm = (ICODE >> 6) & 7;
	uint32_t Rd, Rm, Rn, addr;
	Rn = Thumb_ReadReg(rn);
	Rm = Thumb_ReadReg(rm);
	addr = Rn + Rm;
	Rd = Thumb_ReadReg(rd);
	MMU_Write8(Rd, addr);
	dbgprintf("Thumb strb_2 not tested\n");
}

/**
 *********************************************************************
 * \fn void th_strh_1() 
 * Write a halfword to memory. The address is calculated as the
 * sum of a base register and a five bit immediate shifted by one.
 * v1
 *********************************************************************
 */
void
th_strh_1()
{
	int rd = ICODE & 7;
	int rn = (ICODE >> 3) & 7;
	uint32_t Rn, Rd;
	uint32_t immed5 = (ICODE >> 6) & 0x1f;
	uint32_t addr;
	Rn = Thumb_ReadReg(rn);
	addr = Rn + (immed5 << 1);
	/* 
	 ********************************************
	 * DDI0100 says Bits 0 and 1 must be 0 
	 * This is probably wrong
	 ********************************************
	 */
	if ((addr & 1) == 0) {
		Rd = Thumb_ReadReg(rd);
		MMU_Write16(Rd, addr);
	} else {
		MMU_AlignmentException(addr);
	}
	dbgprintf("Thumb strh_1 not implemented\n");
}

/**
 *******************************************************************
 * \fn void th_strh_2(); 
 * Write a halfword to memory. The address is calculated as the
 * sum of a base and an offset register.
 * v1
 *******************************************************************
 */
void
th_strh_2()
{
	int rd = ICODE & 7;
	int rn = (ICODE >> 3) & 7;
	int rm = (ICODE >> 6) & 7;
	uint32_t Rd, Rm, Rn, addr;
	Rn = Thumb_ReadReg(rn);
	Rm = Thumb_ReadReg(rm);
	addr = Rn + Rm;
	/* 
	 ********************************************
	 * DDI0100 says Bits 0 and 1 must be 0 
	 * This is probably wrong.
	 ********************************************
	 */
	if (likely((addr & 1) == 0)) {
		Rd = Thumb_ReadReg(rd);
		MMU_Write16(Rd, addr);
	} else {
		MMU_AlignmentException(addr);
	}
	dbgprintf("Thumb strh_2 not tested\n");
}

/**
 *******************************************************************
 * \fn void th_sub_1(); 
 * Subtract a small constant from the value of a register and
 * store it in a second register.
 * v1
 *******************************************************************
 */
void
th_sub_1()
{
	int rn, rd;
	uint32_t icode = ICODE;
	uint32_t cpsr = REG_CPSR;
	uint32_t immed3;
	uint32_t Rn, Rd;
	immed3 = (ICODE >> 6) & 7;
	rn = (icode >> 3) & 0x7;
	rd = icode & 0x7;
	Rn = Thumb_ReadReg(rn);
	Rd = Rn - immed3;
	cpsr &= ~(FLAG_N | FLAG_Z | FLAG_C | FLAG_V);
	if (ISNEG(Rd)) {
		cpsr |= FLAG_N;
	} else if (Rd == 0) {
		cpsr |= FLAG_Z;
	}
	cpsr |= sub_carry(Rn, immed3, Rd);
	cpsr |= sub_overflow(Rn, immed3, Rd);
	REG_CPSR = cpsr;
	Thumb_WriteReg(Rd, rd);
	dbgprintf("Thumb sub_1 not implemented\n");
}

/**
 *************************************************************
 * \fn void th_sub_2();
 * Subtract an eight bit immediate from a reigster.
 * v1
 *************************************************************
 */
void
th_sub_2()
{
	int rd;
	uint32_t icode = ICODE;
	uint32_t cpsr = REG_CPSR;
	uint32_t immed8;
	uint32_t Rd;
	uint32_t result;
	immed8 = (icode & 0xff);
	rd = (icode >> 8) & 0x7;
	Rd = Thumb_ReadReg(rd);
	result = Rd - immed8;
	cpsr &= ~(FLAG_N | FLAG_Z | FLAG_C | FLAG_V);
	if (ISNEG(result)) {
		cpsr |= FLAG_N;
	} else if (result == 0) {
		cpsr |= FLAG_Z;
	}
	cpsr |= sub_carry(Rd, immed8, result);
	cpsr |= sub_overflow(Rd, immed8, result);
	REG_CPSR = cpsr;
	Thumb_WriteReg(result, rd);
	dbgprintf("Thumb sub_2 not tested\n");
}

/**
 **********************************************************************
 * \fn void th_sub_3();
 * Subtract two register valuse and store in a third.
 * v1
 **********************************************************************
 */
void
th_sub_3()
{
	int rd, rn, rm;
	uint32_t icode = ICODE;
	uint32_t cpsr = REG_CPSR;
	uint32_t Rd, Rm, Rn;
	rd = icode & 0x7;
	rn = (icode >> 3) & 0x7;
	rm = (icode >> 6) & 0x7;
	Rd = Thumb_ReadReg(rd);
	Rn = Thumb_ReadReg(rn);
	Rm = Thumb_ReadReg(rm);
	Rd = Rn - Rm;
	cpsr &= ~(FLAG_N | FLAG_Z | FLAG_C | FLAG_V);
	if (ISNEG(Rd)) {
		cpsr |= FLAG_N;
	} else if (Rd == 0) {
		cpsr |= FLAG_Z;
	}
	cpsr |= sub_carry(Rn, Rm, Rd);
	cpsr |= sub_overflow(Rn, Rm, Rd);
	REG_CPSR = cpsr;
	Thumb_WriteReg(Rn, rd);
	dbgprintf("Thumb sub_3 not tested\n");
}

/**
 *************************************************************
 * \fn th_sub_4();
 * Subtract the a shifted 7 bit immediate from stack pointer.
 * v1
 *************************************************************
 */
void
th_sub_4()
{
	uint32_t immed_7 = ICODE & 0x7f;
	uint32_t result;
	result = Thumb_ReadReg(13) - (immed_7 << 2);
	Thumb_WriteReg(result, 13);
	dbgprintf("Thumb sub_4 not tested\n");
}

/**
 ************************************************************
 * Software Interrupt. 
 * v1
 ************************************************************
 */
void
th_swi()
{
	ARM_Exception(EX_SWI, 0);
	dbgprintf("Thumb swi not yet tested\n");
}

/*
 ************************************************************
 * \fn void th_tst() 
 * Logical and of two register values without 
 * storing the result.
 * v1
 ************************************************************
 */
void
th_tst()
{
	uint32_t cpsr = REG_CPSR;
	int rn = ICODE & 7;
	int rm = (ICODE >> 3) & 7;
	uint32_t Rn, Rm, result;
	Rm = Thumb_ReadReg(rm);
	Rn = Thumb_ReadReg(rn);
	result = Rn & Rm;
	cpsr &= ~(FLAG_N | FLAG_Z);
	if (ISNEG(result)) {
		cpsr |= FLAG_N;
	} else if (result == 0) {
		cpsr |= FLAG_Z;
	}
	REG_CPSR = cpsr;
	dbgprintf("Thumb tst not tested\n");
}

void
th_undefined()
{
	fprintf(stderr, "Thumb undefined not implemented\n");
}
