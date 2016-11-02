/*
 *********************************************************************************************
 *
 * Intel 8051 instruction set emulation
 *
 * State: working, no codereview. 
 *
 * Copyright 2009 2013 Jochen Karrer. All rights reserved.
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
#include "cpu_mcs51.h"
#include "instructions_mcs51.h"

#define ISNEG(x) ((x) & (1 << 7))
#define ISNOTNEG(x) (!((x) & (1 << 7)))
#define ISHALFNEG(x) ((x) & (1 << 3))
#define ISNOTHALFNEG(x) (!((x) & (1 << 3)))

/**
 ****************************************************************************
 * OV is set if ther is a carry out of bit 7 but not bit 6
 * or if there is a carry out of bit 6, but not out of bit 7
 * See page 12 of Atmel doc0509.pdf
 ****************************************************************************
 */
static inline uint8_t
add8_overflow(uint8_t op1, uint8_t op2, uint8_t result)
{
	if (ISNEG(op1) && ISNEG(op2) && ISNOTNEG(result)) {
		return PSW_OV;
	} else if (ISNOTNEG(op1) && ISNOTNEG(op2) && ISNEG(result)) {
		return PSW_OV;
	}
	return 0;
}

/**
 ********************************************************************** 
 * \fn uint8_t add8_carry(uint8_t op1, uint8_t op2, uint8_t result)
 * Check if there is a carry out of bit 7.
 ********************************************************************** 
 */
static inline uint8_t
add8_carry(uint8_t op1, uint8_t op2, uint8_t result)
{
	if (((ISNEG(op1) && ISNEG(op2))
	     || (ISNEG(op1) && ISNOTNEG(result))
	     || (ISNEG(op2) && ISNOTNEG(result)))) {
		return PSW_CY;
	} else {
		return 0;
	}
}

static inline uint8_t
add4_carry(uint8_t op1, uint8_t op2, uint8_t result)
{
	if (((ISHALFNEG(op1) && ISHALFNEG(op2))
	     || (ISHALFNEG(op1) && ISNOTHALFNEG(result))
	     || (ISHALFNEG(op2) && ISNOTHALFNEG(result)))) {
		return PSW_AC;
	} else {
		return 0;
	}
}

/*
 ****************************************************
 * Borrow style carry. Sub sets carry if a borrow is
 * needed 
 ****************************************************
 */
static inline uint8_t
sub8_carry(uint8_t op1, uint8_t op2, uint8_t result)
{
	if (((ISNEG(op1) && ISNOTNEG(op2))
	     || (ISNEG(op1) && ISNOTNEG(result))
	     || (ISNOTNEG(op2) && ISNOTNEG(result)))) {
		return 0;
	} else {
		return PSW_CY;
	}
}

static inline uint8_t
sub4_carry(uint8_t op1, uint8_t op2, uint8_t result)
{
	if (((ISHALFNEG(op1) && ISNOTHALFNEG(op2))
	     || (ISHALFNEG(op1) && ISNOTHALFNEG(result))
	     || (ISNOTHALFNEG(op2) && ISNOTHALFNEG(result)))) {
		return 0;
	} else {
		return PSW_AC;
	}
}

/**
 **********************************************************************
 * Overflow is set if a borrow is needed into Bit 6 but not into Bit 7
 * or into bit bit 7 but not into bit 5
 **********************************************************************
 */
static inline uint8_t
sub8_overflow(uint8_t op1, uint8_t op2, uint8_t result)
{
	if ((ISNEG(op1) && ISNOTNEG(op2) && ISNOTNEG(result))
	    || (ISNOTNEG(op1) && ISNEG(op2) && ISNEG(result))) {
		return PSW_OV;
	} else {
		return 0;
	}
}

/**
 **************************************************************************
 * \fn void mcs51_acall(void) 
 * Absolute call with 11 Bit address. The subroutine must be in the
 * same 2K block as the first byte of the next instruction. 
 * The Return address is saved on the stack in little endian byte order
 **************************************************************************
 */
void
mcs51_acall(void)
{
	uint16_t addr;
	uint8_t sp;
	addr = MCS51_ReadPgmMem(GET_REG_PC) + ((ICODE & 0xe0) << 3);
	SET_REG_PC(GET_REG_PC + 1);
	sp = MCS51_GetRegSP();
	sp++;
	MCS51_WriteMemIndirect(GET_REG_PC & 0xff, sp);
	sp++;
	MCS51_WriteMemIndirect((GET_REG_PC >> 8) & 0xff, sp);
	addr |= GET_REG_PC & 0xf800;
	SET_REG_PC(addr);
}

/*
 *******************************************************************
 * \fn void mcs51_adda(void)
 * Opcode 0x28. Add a Register Value to the accumulator.
 *******************************************************************
 */
void
mcs51_adda(void)
{
	int reg = ICODE & 7;
	uint8_t A = MCS51_GetAcc();
	uint8_t R = MCS51_GetRegR(reg);
	uint8_t result = A + R;
	MCS51_SetAcc(result);
	PSW = PSW & ~(PSW_CY | PSW_AC | PSW_OV);
	PSW |= add8_carry(A, R, result) | add4_carry(A, R, result) | add8_overflow(A, R, result);
}

/**
 ******************************************************************
 * \fn void mcs51_addadir(void)
 * Add a direct addressed data to the accumulator. 
 ******************************************************************
 */
void
mcs51_addadir(void)
{
	uint8_t data;
	uint8_t addr;
	uint8_t result;
	uint8_t A;
	addr = MCS51_ReadPgmMem(GET_REG_PC);
	data = MCS51_ReadMemDirect(addr);
	SET_REG_PC(GET_REG_PC + 1);
	A = MCS51_GetAcc();
	result = A + data;
	PSW = PSW & ~(PSW_CY | PSW_AC | PSW_OV);
	PSW |= add8_carry(A, data, result) |
	    add4_carry(A, data, result) | add8_overflow(A, data, result);
	MCS51_SetAcc(result);
}

/*
 ********************************************************************
 * \fn void mcs51_addaari(void)
 * Add an indirectly addressed memory location to the Accumulator.
 *******************************************************************
 */
void
mcs51_addaari(void)
{
	uint8_t A;
	uint8_t R;
	uint8_t op2;
	uint8_t result;
	uint8_t reg = ICODE & 1;
	A = MCS51_GetAcc();
	R = MCS51_GetRegR(reg);
	op2 = MCS51_ReadMemIndirect(R);
	result = A + op2;
	PSW = PSW & ~(PSW_CY | PSW_AC | PSW_OV);
	PSW |= add8_carry(A, op2, result) |
	    add4_carry(A, op2, result) | add8_overflow(A, op2, result);
	MCS51_SetAcc(result);
}

/**
 ***************************************************************************
 * \fn void mcs51_addadata(void)
 * Add an immediate data byte from Programm memory addressed by the
 * Programm counter to the accumulator.
 ***************************************************************************
 */
void
mcs51_addadata(void)
{
	uint8_t A;
	uint8_t data;
	uint8_t result;
	data = MCS51_ReadPgmMem(GET_REG_PC);
	SET_REG_PC(GET_REG_PC + 1);
	A = MCS51_GetAcc();
	result = A + data;
	PSW = PSW & ~(PSW_CY | PSW_AC | PSW_OV);
	PSW |= add8_carry(A, data, result) |
	    add4_carry(A, data, result) | add8_overflow(A, data, result);
	MCS51_SetAcc(result);
}

/**
 *********************************************************************
 * \fn void mcs51_addcar(void)
 * Add Accumulator and Register with Carry and store the result
 * in the accumulator.
 *********************************************************************
 */
void
mcs51_addcar(void)
{
	int reg = ICODE & 7;
	uint8_t A = MCS51_GetAcc();
	uint8_t R = MCS51_GetRegR(reg);
	uint8_t C = ! !(PSW & PSW_CY);
	uint8_t result = A + R + C;
	PSW = PSW & ~(PSW_CY | PSW_AC | PSW_OV);
	PSW |= add8_carry(A, R, result) | add4_carry(A, R, result) | add8_overflow(A, R, result);
	MCS51_SetAcc(result);
}

/**
 **************************************************************************
 * \fn void mcs51_addcadir(void)
 * add accumulator with carry to a direct addressed memory location.
 **************************************************************************
 */
void
mcs51_addcadir(void)
{
	uint8_t data;
	uint8_t addr;
	uint8_t result;
	uint8_t A;
	uint8_t C = ! !(PSW & PSW_CY);
	addr = MCS51_ReadPgmMem(GET_REG_PC);
	data = MCS51_ReadMemDirect(addr);
	SET_REG_PC(GET_REG_PC + 1);
	A = MCS51_GetAcc();
	result = A + data + C;
	PSW = PSW & ~(PSW_CY | PSW_AC | PSW_OV);
	PSW |= add8_carry(A, data, result) |
	    add4_carry(A, data, result) | add8_overflow(A, data, result);
	MCS51_SetAcc(result);
}

/**
 ***********************************************************************
 * \fn void mcs51_addcaari(void)
 * Add accumulator with carry to a memory location indirectly addressed 
 * by an register.
 ***********************************************************************
 */
void
mcs51_addcaari(void)
{
	uint8_t A;
	uint8_t R;
	uint8_t C;
	uint8_t op2;
	uint8_t result;
	uint8_t reg = ICODE & 1;
	C = ! !(PSW & PSW_CY);
	A = MCS51_GetAcc();
	R = MCS51_GetRegR(reg);
	op2 = MCS51_ReadMemIndirect(R);
	result = A + op2 + C;
	PSW = PSW & ~(PSW_CY | PSW_AC | PSW_OV);
	PSW |= add8_carry(A, op2, result) |
	    add4_carry(A, op2, result) | add8_overflow(A, op2, result);
	MCS51_SetAcc(result);
}

/**
 ***************************************************************************
 * \fn void mcs51_addcadata(void)
 * Add accumulator with carry to a data byte from programm memory addressed
 * by the programm counter.
 ***************************************************************************
 */
void
mcs51_addcadata(void)
{
	uint8_t A;
	uint8_t data;
	uint8_t result;
	uint8_t C;
	C = ! !(PSW & PSW_CY);
	data = MCS51_ReadPgmMem(GET_REG_PC);
	SET_REG_PC(GET_REG_PC + 1);
	A = MCS51_GetAcc();
	result = A + data + C;
	PSW = PSW & ~(PSW_CY | PSW_AC | PSW_OV);
	PSW |= add8_carry(A, data, result) |
	    add4_carry(A, data, result) | add8_overflow(A, data, result);
	MCS51_SetAcc(result);
}

/**
 *******************************************************************
 * \fn void mcs51_ajmp(void);
 * Absolute Jump into the same 2kB page. The eleven bit address
 * is encoded in the instruction. 
 *******************************************************************
 */

void
mcs51_ajmp(void)
{
	uint16_t addr;
	addr = MCS51_ReadPgmMem(GET_REG_PC) + ((ICODE & 0xe0) << 3);
	SET_REG_PC(GET_REG_PC + 1);
	addr |= GET_REG_PC & 0xf800;
	SET_REG_PC(addr);
}

/**
 ********************************************************************
 * \fn void mcs51_anlrn(void)
 * Logical and of  Accumulator and an Register.
 ********************************************************************
 */
void
mcs51_anlarn(void)
{
	int reg = ICODE & 7;
	uint8_t A = MCS51_GetAcc();
	uint8_t R = MCS51_GetRegR(reg);
	uint8_t result = A & R;
	MCS51_SetAcc(result);
}

/**
 ***************************************************************
 * And of accumulator and a directly addressed memory location.
 * Logical and of the accumulator and a directly addressed 
 * memory location.
 ***************************************************************
 */
void
mcs51_anladir(void)
{
	uint8_t addr;
	uint8_t data;
	uint8_t result;
	uint8_t A;
	addr = MCS51_ReadPgmMem(GET_REG_PC);
	data = MCS51_ReadMemDirect(addr);
	SET_REG_PC(GET_REG_PC + 1);
	A = MCS51_GetAcc();
	result = A & data;
	MCS51_SetAcc(result);
}

/**
 ***********************************************************
 * \fn void mcs51_anlaari(void)
 * Logical and of accumulator and an indirectly addressed
 * Memory location.
 ***********************************************************
 */
void
mcs51_anlaari(void)
{
	uint8_t A;
	uint8_t R;
	uint8_t op2;
	uint8_t result;
	uint8_t reg = ICODE & 1;
	A = MCS51_GetAcc();
	R = MCS51_GetRegR(reg);
	op2 = MCS51_ReadMemIndirect(R);
	result = A & op2;
	MCS51_SetAcc(result);
}

/**
 ******************************************************************
 * \fn void mcs51_anladata(void)
 * Logical AND of accumulator and an immediate data byte
 ******************************************************************
 */
void
mcs51_anladata(void)
{
	uint8_t A;
	uint8_t data;
	uint8_t result;
	data = MCS51_ReadPgmMem(GET_REG_PC);
	SET_REG_PC(GET_REG_PC + 1);
	A = MCS51_GetAcc();
	result = A & data;
	MCS51_SetAcc(result);
}

/**
 **************************************************************************
 * \fn void mcs51_anldira(void)
 * Logical AND of a directly addressed memory location and the accumulator
 **************************************************************************
 */
void
mcs51_anldira(void)
{
	uint8_t data;
	uint8_t result;
	uint8_t A;
	uint8_t addr;
	addr = MCS51_ReadPgmMem(GET_REG_PC);
	data = MCS51_ReadLatchedMemDirect(addr);
	A = MCS51_GetAcc();
	result = A & data;
	MCS51_WriteMemDirect(result, addr);
	SET_REG_PC(GET_REG_PC + 1);
}

/**
 *****************************************************************
 * \fn void mcs51_anldirdata(void)
 * Logical AND of an directly addressed memory location 
 * and an immediate byte
 *****************************************************************
 */
void
mcs51_anldirdata(void)
{
	uint8_t addr = MCS51_ReadPgmMem(GET_REG_PC);
	uint8_t imm = MCS51_ReadPgmMem(GET_REG_PC + 1);
	uint8_t data = MCS51_ReadLatchedMemDirect(addr);
	uint8_t result;
	result = data & imm;
	MCS51_WriteMemDirect(result, addr);
	SET_REG_PC(GET_REG_PC + 2);
}

/**
 **************************************************************
 * \fn void mcs51_anlcbit(void)
 * The logical and of a bit from memory and the carry is 
 * stored in the carry.
 * Atmel doc0509.pdf page 16 sys that this does NOT read
 * the latched Pin state but the Input state.
 **************************************************************
 */
void
mcs51_anlcbit(void)
{
	uint8_t bitaddr = MCS51_ReadPgmMem(GET_REG_PC);
	uint8_t data = MCS51_ReadBit(bitaddr);
	uint8_t C;
	C = ! !(PSW & PSW_CY);
	if (data && C) {
		PSW |= PSW_CY;
	} else {
		PSW &= ~PSW_CY;
	}
	SET_REG_PC(GET_REG_PC + 1);
}

/**
 ***************************************************************
 * \fn void mcs51_anlcnbit(void)
 * Store the logical and of a bit and the inverted carry in
 * the carry.
 ***************************************************************
 */
void
mcs51_anlcnbit(void)
{
	uint8_t bitaddr = MCS51_ReadPgmMem(GET_REG_PC);
	uint8_t data = MCS51_ReadBit(bitaddr);
	uint8_t C;
	C = ! !(PSW & PSW_CY);
	if (C && !data) {
		PSW |= PSW_CY;
	} else {
		PSW &= ~PSW_CY;
	}
	SET_REG_PC(GET_REG_PC + 1);
}

/**
 ***********************************************************
 * \fn void mcs51_cjneadirrel(void)
 * Compare Accumulator with a directly addressed byte.
 * Jump if not equal. The jump destination is the 
 * NIA + a signed byte. If The accumulator is smaller than
 * the directly addressed byte then  the carry flag is
 * set, else cleared.
 ***********************************************************
 */
void
mcs51_cjneadirrel(void)
{
	uint8_t dira;
	uint8_t dir;
	int8_t rel;
	uint8_t A;
	A = MCS51_GetAcc();
	dira = MCS51_ReadPgmMem(GET_REG_PC);
	rel = MCS51_ReadPgmMem(GET_REG_PC + 1);
	dir = MCS51_ReadMemDirect(dira);
	if (A != dir) {
		SET_REG_PC(GET_REG_PC + rel + 2);
	} else {
		SET_REG_PC(GET_REG_PC + 2);
	}
	if (A < dir) {
		PSW |= PSW_CY;
	} else {
		PSW &= ~PSW_CY;
	}
}

/**
 *******************************************************************
 * \fn void mcs51_cjneadatarel(void)
 * Compare Acc with an immediate byte and jump if not equal.
 *******************************************************************
 */
void
mcs51_cjneadatarel(void)
{
	uint8_t A;
	uint8_t imm;
	int8_t rel;
	A = MCS51_GetAcc();
	imm = MCS51_ReadPgmMem(GET_REG_PC);
	rel = MCS51_ReadPgmMem(GET_REG_PC + 1);
	SET_REG_PC(GET_REG_PC + 2);
	if (A != imm) {
		SET_REG_PC(GET_REG_PC + rel);
	}
	if (A < imm) {
		PSW |= PSW_CY;
	} else {
		PSW &= ~PSW_CY;
	}
}

/**
 *****************************************************************
 * \fn void mcs51_cjnerdatarel(void)
 * Compare a Register with an immediate date byte an jump if not
 * equal. Carry is ist if the Register contents is smaller than
 * the data byte.
 *****************************************************************
 */
void
mcs51_cjnerdatarel(void)
{
	uint8_t R;
	uint8_t imm;
	int8_t rel;
	R = MCS51_GetRegR(ICODE & 7);
	imm = MCS51_ReadPgmMem(GET_REG_PC);
	rel = MCS51_ReadPgmMem(GET_REG_PC + 1);
	SET_REG_PC(GET_REG_PC + 2);
	if (R != imm) {
		SET_REG_PC(GET_REG_PC + rel);
#if 0
		if ((ICODE & 7) == 2) {
			fprintf(stderr, "jump, imm %u, R %u\n", imm, R);
		}
#endif
	} else {
		//      fprintf(stderr,"Dont jump\n");
	}
	if (R < imm) {
		PSW |= PSW_CY;
	} else {
		PSW &= ~PSW_CY;
	}
}

/**
 ************************************************************************
 * \fn void mcs51_cjneardatarel(void)
 * Compare a byte addressed by a register with an immediate data byte.
 * jump if not equal. Set carry if smaller. 
 ************************************************************************
 */
void
mcs51_cjneardatarel(void)
{
	uint8_t R;
	uint8_t imm;
	int8_t rel;
	uint8_t val;
	R = MCS51_GetRegR(ICODE & 1);
	imm = MCS51_ReadPgmMem(GET_REG_PC);
	rel = MCS51_ReadPgmMem(GET_REG_PC + 1);
	val = MCS51_ReadMemIndirect(R);
	SET_REG_PC(GET_REG_PC + 2);
	if (val != imm) {
		SET_REG_PC(GET_REG_PC + rel);
	}
	if (val < imm) {
		PSW |= PSW_CY;
	} else {
		PSW &= ~PSW_CY;
	}
}

/**
 ******************************************************************
 * \fn void mcs51_clra(void)
 * Clear the accumulator. No flags are affected.
 ******************************************************************
 */
void
mcs51_clra(void)
{
	MCS51_SetAcc(0);
}

/**
 ************************************************************
 * \fn void mcs51_clrc(void)
 * Clear carry flag.
 ************************************************************
 */
void
mcs51_clrc(void)
{
	PSW &= ~PSW_CY;
}

/**
 ******************************************************************
 * \fn void mcs51_clrbit(void)
 * Clear a bit addressed by a bit address encoded in the
 * instruction. Clear Bit uses the Latched variant of 
 * Register read.
 ******************************************************************
 */
void
mcs51_clrbit(void)
{
	uint8_t bitaddr;
	bitaddr = MCS51_ReadPgmMem(GET_REG_PC);
	MCS51_WriteBitLatched(0, bitaddr);
	SET_REG_PC(GET_REG_PC + 1);
}

/**
 ********************************************************
 * \fn void mcs51_cpla(void)
 * Complement Accumulator. One's complement. No flags are
 * affected.
 ********************************************************
 */
void
mcs51_cpla(void)
{
	MCS51_SetAcc(~MCS51_GetAcc());
}

/**
 **********************************************************
 * \fn void mcs51_cplc(void)
 * Invert the Carry flag
 **********************************************************
 */
void
mcs51_cplc(void)
{
	PSW ^= PSW_CY;
}

/**
 ***********************************************************************
 * \fn void mcs51_cplbit(void)
 * Invert a bit in memory. Uses the Latched version of port read.
 ***********************************************************************
 */
void
mcs51_cplbit(void)
{
	uint8_t bitaddr;
	uint8_t data;
	bitaddr = MCS51_ReadPgmMem(GET_REG_PC);
	data = MCS51_ReadBitLatched(bitaddr);
	MCS51_WriteBitLatched(!data, bitaddr);
	SET_REG_PC(GET_REG_PC + 1);
}

/**
 ******************************************************************
 * \đn void mcs51_da(void)
 * Decimal adjust the accumulator
 ******************************************************************
 */
void
mcs51_da(void)
{
	uint16_t A = MCS51_GetAcc();
	uint8_t AC = ! !(PSW & PSW_AC);
	uint8_t C = ! !(PSW & PSW_CY);
	if (((A & 0xf) > 9) || AC) {
		A = A + 6;
		if (A >= 0x100) {
			A = A & 0xff;
			C = 1;
		}
	}
	if (((A & 0xf0) > 90) || C) {
		A = A + 0x60;
		if (A >= 0x100) {
			A = A & 0xff;
			C = 1;
		}
	}
	MCS51_SetAcc(A);
	if (C) {
		PSW |= PSW_CY;
	} else {
		PSW &= ~PSW_CY;
	}
}

/**
 ******************************************************************
 * \fn void mcs51_deca(void)
 * Decrement the accumulator by one.
 ******************************************************************
 */
void
mcs51_deca(void)
{
	MCS51_SetAcc(MCS51_GetAcc() - 1);
}

/**
 *****************************************************************
 * \fn void mcs51_decr(void)
 * Decrement a register.
 *****************************************************************
 */
void
mcs51_decr(void)
{
	uint8_t reg = ICODE & 7;
	MCS51_SetRegR(MCS51_GetRegR(reg) - 1, reg);
}

/**
 *****************************************************************
 * Decrement a directly addressed byte in memory. This uses
 * The latched version of Read for 
 * Read-Modfiy-Write
 *****************************************************************
 */
void
mcs51_decdir(void)
{
	uint8_t addr = MCS51_ReadPgmMem(GET_REG_PC);
	uint8_t data;
	SET_REG_PC(GET_REG_PC + 1);
	data = MCS51_ReadLatchedMemDirect(addr);
	MCS51_WriteMemDirect(data - 1, addr);
}

/**
 **************************************************************************
 * \fn void mcs51_decari(void)
 * Decrement a byte indirectly adressed by a register. 
 **************************************************************************
 */
void
mcs51_decari(void)
{
	uint8_t reg = ICODE & 1;
	uint8_t addr = MCS51_GetRegR(reg);
	uint8_t val = MCS51_ReadMemIndirect(addr);
	MCS51_WriteMemIndirect(val - 1, addr);
}

/**
 ********************************************************************
 * Unsigned division of the Accumulator by the B register.
 * The B register will contain the remainder
 ********************************************************************
 */
void
mcs51_divab(void)
{
	uint8_t A;
	uint8_t B;
	uint8_t ab;
	A = MCS51_GetAcc();
	B = MCS51_GetRegB();
	if (B) {
		ab = A / B;
		B = A - (B * ab);
		A = ab;
		PSW &= ~(PSW_CY | PSW_OV);
	} else {
		PSW |= PSW_OV;
		PSW &= ~(PSW_CY);
	}
	MCS51_SetAcc(A);
	MCS51_SetRegB(B);
}

/**
 **************************************************************
 * \fn void mcs51_djnzrrel(void)
 * Decrement a register and jump if not zero.
 **************************************************************
 */
void
mcs51_djnzrrel(void)
{
	int8_t rela = MCS51_ReadPgmMem(GET_REG_PC);
	uint8_t r = ICODE & 0x7;
	uint8_t R;
	SET_REG_PC(GET_REG_PC + 1);
	R = MCS51_GetRegR(r);
	R -= 1;
	MCS51_SetRegR(R, r);
	if (R != 0) {
		SET_REG_PC(GET_REG_PC + rela);
	}
}

/**
 *********************************************************************
 * \fn void mcs51_djnzdirrel(void)
 * Decrement a directly addressed byte and jump if not zero.
 * This uses the latched version of Port Read.
 *********************************************************************
 */
void
mcs51_djnzdirrel(void)
{
	uint8_t dira = MCS51_ReadPgmMem(GET_REG_PC);
	uint8_t value;
	int8_t rela = MCS51_ReadPgmMem(GET_REG_PC + 1);
	SET_REG_PC(GET_REG_PC + 2);
	value = MCS51_ReadLatchedMemDirect(dira);
	value -= 1;
	MCS51_WriteMemDirect(value, dira);
	if (value != 0) {
		SET_REG_PC(GET_REG_PC + rela);
	}
}

/**
 ***********************************************************
 * \fn void mcs51_inca(void)
 * Increment the accumulator by one. No flags are affected.
 ***********************************************************
 */
void
mcs51_inca(void)
{
	MCS51_SetAcc(MCS51_GetAcc() + 1);
}

/**
 **************************************************************
 * \fn void mcs51_incr(void);
 * Increment a register.
 **************************************************************
 */
void
mcs51_incr(void)
{
	uint8_t reg = ICODE & 7;
	MCS51_SetRegR(MCS51_GetRegR(reg) + 1, reg);
}

/**
 *********************************************************************
 * \fn void mcs51_incdir(void)
 * Increment a directly addressed byte. Uses the Latch version
 * of Read for Read-Modify-Write
 *********************************************************************
 */
void
mcs51_incdir(void)
{
	uint8_t addr = MCS51_ReadPgmMem(GET_REG_PC);
	uint8_t data;
	SET_REG_PC(GET_REG_PC + 1);
	data = MCS51_ReadLatchedMemDirect(addr);
	MCS51_WriteMemDirect(data + 1, addr);
}

/**
 **********************************************************************
 * \fn void mcs51_incari(void)
 * Increment a byte indirectly addressed by a Register.
 **********************************************************************
 */
void
mcs51_incari(void)
{
	uint8_t reg = ICODE & 1;
	uint8_t addr = MCS51_GetRegR(reg);
	uint8_t val = MCS51_ReadMemIndirect(addr);
	MCS51_WriteMemIndirect(val + 1, addr);
}

/**
 ***************************************************************
 * \fn void mcs51_incdptr(void)
 * Increment the 16 bit data pointer.
 ***************************************************************
 */
void
mcs51_incdptr(void)
{
	MCS51_SetRegDptr(MCS51_GetRegDptr() + 1);
}

/**
 *****************************************************************
 * \fn void mcs51_jbbitrel(void)
 * Relative jump if bit is set. It uses the non latched version
 * of read. 
 *****************************************************************
 */
void
mcs51_jbbitrel(void)
{
	uint8_t bitaddr = MCS51_ReadPgmMem(GET_REG_PC);
	uint8_t data;
	int8_t rela = MCS51_ReadPgmMem(GET_REG_PC + 1);
	SET_REG_PC(GET_REG_PC + 2);
	data = MCS51_ReadBit(bitaddr);
	if (data) {
		SET_REG_PC(GET_REG_PC + rela);
	}
}

/**
 **************************************************************
 * \fn void mcs51_jbcbitrel(void)
 * Jump if Bit is set and clear bit.
 * This uses the latched version of read and write for 
 * Read-Modify-Write
 **************************************************************
 */
void
mcs51_jbcbitrel(void)
{
	uint8_t bitaddr = MCS51_ReadPgmMem(GET_REG_PC);
	uint8_t data;
	int8_t rela = MCS51_ReadPgmMem(GET_REG_PC + 1);
	SET_REG_PC(GET_REG_PC + 2);
	data = MCS51_ReadBitLatched(bitaddr);
	MCS51_WriteBitLatched(0, bitaddr);
	if (data) {
		SET_REG_PC(GET_REG_PC + rela);
	}
}

/**
 ********************************************************************
 * \fn void mcs51_jcrel(void)
 * Jump if carry is set.
 ********************************************************************
 */
void
mcs51_jcrel(void)
{
	uint8_t C;
	int8_t rela = MCS51_ReadPgmMem(GET_REG_PC);
	C = ! !(PSW & PSW_CY);
	SET_REG_PC(GET_REG_PC + 1);
	if (C) {
		SET_REG_PC(GET_REG_PC + rela);
	}
}

/**
 *************************************************************
 * \fn void mcs51_jmpaadptr(void)
 * Jump Indirect to sum of DPTR + Accumulator
 *************************************************************
 */
void
mcs51_jmpaadptr(void)
{
	uint16_t dptr = MCS51_GetRegDptr();
	uint8_t A = MCS51_GetAcc();
	SET_REG_PC(dptr + A);
}

/**
 **********************************************************************
 * \fn void mcs51_jnbbitrel(void)
 * Jump if Bit is not set. Uses the non latched variant of read.
 **********************************************************************
 */
void
mcs51_jnbbitrel(void)
{
	uint8_t bitaddr = MCS51_ReadPgmMem(GET_REG_PC);
	uint8_t data;
	int8_t rela = MCS51_ReadPgmMem(GET_REG_PC + 1);
	SET_REG_PC(GET_REG_PC + 2);
	data = MCS51_ReadBit(bitaddr);
	if (!data) {
		SET_REG_PC(GET_REG_PC + rela);
	}
}

/**
 **************************************************************
 * \fn void mcs51_jncrel(void)
 * Jump if Carry not set.
 **************************************************************
 */
void
mcs51_jncrel(void)
{
	uint8_t C;
	int8_t rela = MCS51_ReadPgmMem(GET_REG_PC);
	C = ! !(PSW & PSW_CY);
	SET_REG_PC(GET_REG_PC + 1);
	if (!C) {
		SET_REG_PC(GET_REG_PC + rela);
	}
}

/**
 *********************************************************
 * \fn void mcs51_jnzrel(void)
 * Jump if accumulator Not Zero.
 *********************************************************
 */
void
mcs51_jnzrel(void)
{
	uint8_t A;
	int8_t rela = MCS51_ReadPgmMem(GET_REG_PC);
	A = MCS51_GetAcc();
	SET_REG_PC(GET_REG_PC + 1);
	if (A != 0) {
		SET_REG_PC(GET_REG_PC + rela);
	}
}

/**
 *****************************************************************
 * \fn void mcs51_jzrel(void)
 * Jump if the accumulator is zero.
 *****************************************************************
 */
void
mcs51_jzrel(void)
{
	uint8_t A;
	int8_t rela = MCS51_ReadPgmMem(GET_REG_PC);
	A = MCS51_GetAcc();
	SET_REG_PC(GET_REG_PC + 1);
	if (A == 0) {
		SET_REG_PC(GET_REG_PC + rela);
	}
}

/** 
 **************************************************************************
 * \fn void mcs51_lcall(void)
 * LCALL calls a subroutine. The return address is stored on the stack.
 **************************************************************************
 */
void
mcs51_lcall(void)
{
	uint16_t addr;
	uint8_t sp;
	addr = (MCS51_ReadPgmMem(GET_REG_PC + 0) << 8) | (MCS51_ReadPgmMem(GET_REG_PC + 1));
	SET_REG_PC(GET_REG_PC + 2);
	sp = MCS51_GetRegSP();
	sp++;
	MCS51_WriteMemIndirect(GET_REG_PC & 0xff, sp);
	sp++;
	MCS51_WriteMemIndirect((GET_REG_PC >> 8) & 0xff, sp);
	MCS51_SetRegSP(sp);
	SET_REG_PC(addr);
}

/**
 ******************************************************************
 * Unconditional long jump to a 16 Bit immediate address. 
 ******************************************************************
 */
void
mcs51_ljmp(void)
{
	uint16_t addr;
	addr = (MCS51_ReadPgmMem(GET_REG_PC) << 8) | (MCS51_ReadPgmMem(GET_REG_PC + 1));
	SET_REG_PC(GET_REG_PC + 2);
	SET_REG_PC(addr);
}

/**
 ***************************************************************
 * \fn void mcs51_movarn(void)
 * Move a Register to the accumulator. No flags are affected.
 ***************************************************************
 */
void
mcs51_movarn(void)
{
	uint8_t R;
	uint8_t reg = ICODE & 7;
	R = MCS51_GetRegR(reg);
	MCS51_SetAcc(R);
}

/**
 ****************************************************************
 * \fn void mcs51_movadir(void)
 * move a directly addressed byte to the accumulatior
 ****************************************************************
 */
void
mcs51_movadir(void)
{
	uint8_t dira = MCS51_ReadPgmMem(GET_REG_PC);
	uint8_t data;
	SET_REG_PC(GET_REG_PC + 1);
	data = MCS51_ReadMemDirect(dira);
	MCS51_SetAcc(data);
}

/**
 *********************************************************
 * \fn void mcs51_movaari(void)
 * Move a byte indirectly addressed by a register to 
 * the accumulator.
 *********************************************************
 */
void
mcs51_movaari(void)
{
	uint8_t reg = ICODE & 1;
	uint8_t R = MCS51_GetRegR(reg);
	uint8_t data = MCS51_ReadMemIndirect(R);
	MCS51_SetAcc(data);
}

/**
 *************************************************************
 * \fn void mcs51_movadata(void)
 * Move a data byte addressed by PC to the accumulator
 * Length: 2
 * Cycles: 1
 *************************************************************
 */
void
mcs51_movadata(void)
{
	uint8_t imm8 = MCS51_ReadPgmMem(GET_REG_PC);
	SET_REG_PC(GET_REG_PC + 1);
	MCS51_SetAcc(imm8);
}

/**
 ********************************************************
 * \fn void mcs51_movra(void)
 * Move from accumulator to a Register
 ********************************************************
 */
void
mcs51_movra(void)
{
	uint8_t Acc;
	uint8_t reg = ICODE & 7;
	Acc = MCS51_GetAcc();
	MCS51_SetRegR(Acc, reg);
}

/** 
 ************************************************************
 * \fn void mcs51_movrdir(void)
 * Move a directly addressed byte to a register.
 ************************************************************
 */
void
mcs51_movrdir(void)
{
	uint8_t reg = ICODE & 7;
	uint8_t dira = MCS51_ReadPgmMem(GET_REG_PC);
	uint8_t data;
	SET_REG_PC(GET_REG_PC + 1);
	data = MCS51_ReadMemDirect(dira);
	MCS51_SetRegR(data, reg);
}

/**
 *************************************************************
 * \fn void mcs51_movrdata(void)
 * Move an immediate data byte to a register.
 *************************************************************
 */
void
mcs51_movrdata(void)
{
	uint8_t reg = ICODE & 7;
	uint8_t imm8 = MCS51_ReadPgmMem(GET_REG_PC);
	SET_REG_PC(GET_REG_PC + 1);
	MCS51_SetRegR(imm8, reg);
}

/**
 ********************************************************************
 * \fn void mcs51_movdira(void)
 * Move the Accumulator to a direct address
 ********************************************************************
 */
void
mcs51_movdira(void)
{
	uint8_t dira = MCS51_ReadPgmMem(GET_REG_PC);
	uint8_t data;
	SET_REG_PC(GET_REG_PC + 1);
	data = MCS51_GetAcc();
	MCS51_WriteMemDirect(data, dira);
}

/**
 **************************************************************************
 * \fn void mcs51_movdirr(void)
 * Move Register value to a direct address.
 * Length: 2 (1 Byte opcode with Register Number + 1 Byte direct address)
 * Cylces: 2
 **************************************************************************
 */
void
mcs51_movdirr(void)
{
	uint8_t reg = ICODE & 7;
	uint8_t dira = MCS51_ReadPgmMem(GET_REG_PC);
	uint8_t data;
	SET_REG_PC(GET_REG_PC + 1);
	data = MCS51_GetRegR(reg);
	MCS51_WriteMemDirect(data, dira);
}

/**
 ************************************************************************
 * Move a directly addressed byte to a directly Addressed destination.
 * Latching is unclear ????
 * Warning: Atmel Manual doc0509 is wrong with order of arguments !
 * The first argument is the src, the second one is the destination !
 * See keil homepage.
 ************************************************************************
 */
void
mcs51_movdirdir(void)
{
	uint8_t srca, dsta;
	uint8_t data;
	srca = MCS51_ReadPgmMem(GET_REG_PC);	/* Warning, atmel manual is wrong ! */
	dsta = MCS51_ReadPgmMem(GET_REG_PC + 1);
	SET_REG_PC(GET_REG_PC + 2);
	data = MCS51_ReadMemDirect(srca);
	MCS51_WriteMemDirect(data, dsta);
}

/**
 *********************************************************************
 * \fn void mcs51_movdirari(void)
 * Move a byte indirectly addressed by a register to a directly
 * addressed destination.
 *********************************************************************
 */
void
mcs51_movdirari(void)
{
	uint8_t dira = MCS51_ReadPgmMem(GET_REG_PC);
	uint8_t reg = ICODE & 1;
	uint8_t R = MCS51_GetRegR(reg);
	uint8_t data = MCS51_ReadMemIndirect(R);
	SET_REG_PC(GET_REG_PC + 1);
	MCS51_WriteMemDirect(data, dira);
}

/**
 ****************************************************************
 * \fn void mcs51_movdirdata(void)
 * Move a immediate data byte to a directly addressed location.
 ****************************************************************
 */
void
mcs51_movdirdata(void)
{
	uint8_t dira = MCS51_ReadPgmMem(GET_REG_PC);
	uint8_t imm = MCS51_ReadPgmMem(GET_REG_PC + 1);
	MCS51_WriteMemDirect(imm, dira);
	SET_REG_PC(GET_REG_PC + 2);
}

/**
 ************************************************************************
 * \fn void mcs51_movaria(void)
 * Move the accumulator th a memory location addressed by a register.
 ************************************************************************
 */
void
mcs51_movaria(void)
{
	uint8_t reg = ICODE & 1;
	uint8_t R = MCS51_GetRegR(reg);
	uint8_t A = MCS51_GetAcc();
	MCS51_WriteMemIndirect(A, R);
}

/**
 ******************************************************************
 * \fn void mcs51_movaridir(void)
 * move data from a directly addressed memory location to
 * an indirectly addressed memory location.
 ******************************************************************
 */
void
mcs51_movaridir(void)
{
	uint8_t reg = ICODE & 1;
	uint8_t dira = MCS51_ReadPgmMem(GET_REG_PC);
	uint8_t R = MCS51_GetRegR(reg);
	uint8_t data;
	SET_REG_PC(GET_REG_PC + 1);
	data = MCS51_ReadMemDirect(dira);
	MCS51_WriteMemIndirect(data, R);
}

/**
 *********************************************************************
 * \fn void mcs51_movaridata(void)
 * Move a immediate to and indirectly addressed memory location.
 *********************************************************************
 */
void
mcs51_movaridata(void)
{
	uint8_t reg = ICODE & 1;
	uint8_t imm = MCS51_ReadPgmMem(GET_REG_PC);
	uint8_t R = MCS51_GetRegR(reg);
	SET_REG_PC(GET_REG_PC + 1);
	MCS51_WriteMemIndirect(imm, R);
}

/**
 ****************************************************************
 * \fn void mcs51_movcbit(void)
 * Move a bit from memory to the Carry flag
 * This uses the non latched variant of ReadBit.
 ****************************************************************
 */
void
mcs51_movcbit(void)
{
	uint8_t bitaddr;
	uint8_t data;
	bitaddr = MCS51_ReadPgmMem(GET_REG_PC);
	SET_REG_PC(GET_REG_PC + 1);
	data = MCS51_ReadBit(bitaddr);
	if (data) {
		PSW = PSW | PSW_CY;
	} else {
		PSW = PSW & ~PSW_CY;
	}

}

/**
 *****************************************************************
 * \fn void mcs51_movbitc(void)
 * Move the Carry flag to a bit in memory. This uses the
 * latched variant of WriteBit.
 *****************************************************************
 */
void
mcs51_movbitc(void)
{
	uint8_t bitaddr;
	uint8_t C = ! !(PSW & PSW_CY);
	bitaddr = MCS51_ReadPgmMem(GET_REG_PC);
	SET_REG_PC(GET_REG_PC + 1);
	if (C) {
		MCS51_WriteBitLatched(1, bitaddr);
	} else {
		MCS51_WriteBitLatched(0, bitaddr);
	}
}

/**
 ***************************************************************
 * \fn void mcs51_movdptrdata(void)
 * Load an immediate 16 bit word to the data pointer.
 ***************************************************************
 */
void
mcs51_movdptrdata(void)
{
	uint16_t data = (MCS51_ReadPgmMem(GET_REG_PC) << 8) | (MCS51_ReadPgmMem(GET_REG_PC + 1));
	SET_REG_PC(GET_REG_PC + 2);
	MCS51_SetRegDptr(data);
}

/**
 *********************************************************************
 * \fn void mcs51_movcaadptr(void)
 * move from a place in Code space addressed by dptr + Accumulator
 * to the Accumulator.
 * Length: 1
 * Cycles: 2
 *********************************************************************
 */
void
mcs51_movcaadptr(void)
{
	uint16_t dptr = MCS51_GetRegDptr();
	uint8_t A = MCS51_GetAcc();
	uint8_t data = MCS51_ReadPgmMem(dptr + A);
	MCS51_SetAcc(data);
}

/**
 ******************************************************************
 * \fn void mcs51_movaapc(void)
 * Move from programm memory addressed by pc + acc to acc.
 ******************************************************************
 */
void
mcs51_movaapc(void)
{
	uint8_t data;
	data = MCS51_ReadPgmMem(GET_REG_PC + MCS51_GetAcc());
	MCS51_SetAcc(data);
}

/**
 ************************************************************************
 * \fn void mcs51_movxaari(void)
 * Move from external memory addressed by a register to accumulator.
 ************************************************************************
 */
void
mcs51_movxaari(void)
{
	uint8_t reg = ICODE & 1;
	uint8_t R = MCS51_GetRegR(reg);
	uint8_t data = MCS51_ReadExmem(R);
	MCS51_SetAcc(data);
}

/**
 ***********************************************************************
 * \fn void mcs51_movxaadptr(void)
 * Move form external memory addressed by data pointer to accumulator.
 ***********************************************************************
 */
void
mcs51_movxaadptr(void)
{
	uint16_t dptr = MCS51_GetRegDptr();
	uint8_t data = MCS51_ReadExmem(dptr);
	//fprintf(stderr,"Read %02x from %04x\n",data,dptr);
	MCS51_SetAcc(data);
}

/**
 ************************************************************************
 * \fn void mcs51_movxara(void)
 * Move accumulator to an Register-indirectly addressed memory location
 * in external memory.
 ************************************************************************
 */
void
mcs51_movxara(void)
{
	uint8_t A = MCS51_GetAcc();
	uint8_t reg = ICODE & 1;
	uint8_t R = MCS51_GetRegR(reg);
	MCS51_WriteExmem(A, R);
}

/**
 ***************************************************************************
 * \fn void mcs51_movxadptra(void)
 * move accumulator to a external memory location addressed by data pointer.
 ***************************************************************************
 */
void
mcs51_movxadptra(void)
{
	uint8_t A = MCS51_GetAcc();
	uint16_t dptr = MCS51_GetRegDptr();
	//fprintf(stderr,"Write Acc to exmem: %02x to %04x\n",A,dptr);
	MCS51_WriteExmem(A, dptr);
}

/**
 *********************************************************************
 * Multiply the Accumulator with the b register.
 * The lower byte of the result is stored in acc the higher 
 * byte is stored in B register.
 *********************************************************************
 */
void
mcs51_mulab(void)
{
	uint16_t A, B;
	uint16_t ab;
	A = MCS51_GetAcc();
	B = MCS51_GetRegB();
	ab = A * B;
	MCS51_SetAcc(ab & 0xff);
	MCS51_SetRegB(ab >> 8);
}

/**
 **********************************************************
 * \fn void mcs51_nop(void)
 * Use up time.
 **********************************************************
 */
void
mcs51_nop(void)
{
	/* Does nothing but using up time */
}

/**
 **********************************************************
 * \fn void mcs51_orlar(void)
 * The logical or of a register and the accumulator 
 * is stored in the accumulator.
 **********************************************************
 */
void
mcs51_orlar(void)
{
	int reg = ICODE & 7;
	uint8_t A = MCS51_GetAcc();
	uint8_t R = MCS51_GetRegR(reg);
	uint8_t result = A | R;
	MCS51_SetAcc(result);
}

/**
 **********************************************************************
 * \fn void mcs51_orladir(void)
 * The Logical or of a directly addressed memory location and the
 * accumulator is stored in Acc.
 **********************************************************************
 */
void
mcs51_orladir(void)
{
	uint8_t addr;
	uint8_t data;
	uint8_t result;
	uint8_t A;
	addr = MCS51_ReadPgmMem(GET_REG_PC);
	data = MCS51_ReadMemDirect(addr);
	SET_REG_PC(GET_REG_PC + 1);
	A = MCS51_GetAcc();
	result = A | data;
	MCS51_SetAcc(result);
}

/**
 **********************************************************************
 * \fn void mcs51_orlaari(void)
 * The Logical or of a  memory location addressed by a register and the
 * accumulator is stored in Acc.
 **********************************************************************
 */
void
mcs51_orlaari(void)
{
	uint8_t A;
	uint8_t R;
	uint8_t op2;
	uint8_t result;
	uint8_t reg = ICODE & 1;
	A = MCS51_GetAcc();
	R = MCS51_GetRegR(reg);
	op2 = MCS51_ReadMemIndirect(R);
	result = A | op2;
	MCS51_SetAcc(result);
}

/**
 ************************************************************************
 * \fn void mcs51_orladata(void)
 * Store the logical of of accumulator and immediate date in Acc.
 ************************************************************************
 */
void
mcs51_orladata(void)
{
	uint8_t A;
	uint8_t data;
	uint8_t result;
	data = MCS51_ReadPgmMem(GET_REG_PC);
	SET_REG_PC(GET_REG_PC + 1);
	A = MCS51_GetAcc();
	result = A | data;
	MCS51_SetAcc(result);
}

/**
 ************************************************************************
 * \fn void mcs51_orldira(void)
 * Store the logical or of Acc and a directly addressed memory location
 * in memory. This uses the the Latched Read for Read-Modify-Write
 ************************************************************************
 */
void
mcs51_orldira(void)
{
	uint8_t data;
	uint8_t result;
	uint8_t A;
	uint8_t addr;
	addr = MCS51_ReadPgmMem(GET_REG_PC);
	data = MCS51_ReadLatchedMemDirect(addr);
	A = MCS51_GetAcc();
	result = A | data;
	MCS51_WriteMemDirect(result, addr);
	SET_REG_PC(GET_REG_PC + 1);
}

/**
 ***********************************************************************
 * \fn void mcs51_orldirdata(void)
 * Store the logical and of a directly addressed memory location and
 * an immediate data byte in memory. This used the Latched port values
 * for the Read-Modify write operation.
 ***********************************************************************
 */
void
mcs51_orldirdata(void)
{
	uint8_t addr = MCS51_ReadPgmMem(GET_REG_PC);
	uint8_t imm = MCS51_ReadPgmMem(GET_REG_PC + 1);
	uint8_t data = MCS51_ReadLatchedMemDirect(addr);
	uint8_t result;
	result = data | imm;
	MCS51_WriteMemDirect(result, addr);
	SET_REG_PC(GET_REG_PC + 2);
}

/**
 *********************************************************************
 * \fn void mcs51_orlcbit(void)
 * Logical or of carry flag and a bit from memory is stored in Carry
 *********************************************************************
 */
void
mcs51_orlcbit(void)
{
	uint8_t bitaddr = MCS51_ReadPgmMem(GET_REG_PC);
	uint8_t data = MCS51_ReadBit(bitaddr);
	uint8_t C;
	C = ! !(PSW & PSW_CY);
	if (data || C) {
		PSW |= PSW_CY;
	} else {
		PSW &= ~PSW_CY;
	}
	SET_REG_PC(GET_REG_PC + 1);
}

/**
 ***************************************************************************
 * \fnvoid mcs51_orlcnbit(void)
 * Logical or of Carry and a negated bit from memory is stored in Carry.
 ***************************************************************************
 */
void
mcs51_orlcnbit(void)
{

	uint8_t bitaddr = MCS51_ReadPgmMem(GET_REG_PC);
	uint8_t data = MCS51_ReadBit(bitaddr);
	uint8_t C;
	C = ! !(PSW & PSW_CY);
	if (C || !data) {
		PSW |= PSW_CY;
	} else {
		PSW &= ~PSW_CY;
	}
	SET_REG_PC(GET_REG_PC + 1);
}

/**
 *********************************************************************
 * \fn void mcs51_popdir(void)
 * Pop from stack to a directly addressed memory location.
 * The pseudo code in Atmel doc509 manual is wrong because poping to
 * the stack pointer is possible.
 *********************************************************************
 */
void
mcs51_popdir(void)
{
	uint8_t dira = MCS51_ReadPgmMem(GET_REG_PC);
	uint8_t sp;
	uint8_t data;
	SET_REG_PC(GET_REG_PC + 1);
	sp = MCS51_GetRegSP();
	MCS51_SetRegSP(sp - 1);
	data = MCS51_ReadMemIndirect(sp);
	MCS51_WriteMemDirect(data, dira);
}

/**
 ************************************************************
 * \fn void mcs51_pushdir(void)
 * Push to stack from a directly addressed memory location.
 * The behaviour when Pushing the stackpointer is not clear,
 * because Pseudo code in manual is unreliable.
 ************************************************************
 */
void
mcs51_pushdir(void)
{
	uint8_t dira = MCS51_ReadPgmMem(GET_REG_PC);
	uint8_t sp;
	uint8_t data;
	SET_REG_PC(GET_REG_PC + 1);
	sp = MCS51_GetRegSP();
	sp++;
	MCS51_SetRegSP(sp);
	data = MCS51_ReadMemDirect(dira);
	MCS51_WriteMemIndirect(data, sp);
}

/**
 ****************************************************************
 * \fn void mcs51_ret(void)
 * Return from subroutine. Pops two bytes from stack and writes
 * them to the pc;
 ****************************************************************
 */
void
mcs51_ret(void)
{
	uint16_t pc;
	uint8_t sp;
	sp = MCS51_GetRegSP();
	pc = MCS51_ReadMemIndirect(sp) << 8;
	sp--;
	pc |= MCS51_ReadMemIndirect(sp);
	sp--;
	MCS51_SetRegSP(sp);
	SET_REG_PC(pc);
}

/**
 ****************************************************************
 * \fn void mcs51_reti(void)
 * Return from interrupt. Identical to ret, but additionaly
 * pops the IPL level from the Interrupt level stack.
 ****************************************************************
 */
void
mcs51_reti(void)
{
	uint16_t pc;
	uint8_t sp;
	sp = MCS51_GetRegSP();
	pc = MCS51_ReadMemIndirect(sp) << 8;
	sp--;
	pc |= MCS51_ReadMemIndirect(sp);
	sp--;
	MCS51_SetRegSP(sp);
	SET_REG_PC(pc);
	MCS51_PopIpl();
}

/**
 ***************************************************************
 * \fn void mcs51_rla(void)
 * Rotate accumulator left by one bit
 ***************************************************************
 */
void
mcs51_rla(void)
{
	uint8_t A;
	A = MCS51_GetAcc();
	if (A & 0x80) {
		A = (A << 1) | 1;
	} else {
		A = (A << 1);
	}
	MCS51_SetAcc(A);
}

/**
 ********************************************************
 * \fn void mcs51_rlca(void)
 * Rotate accumulator and Carry left.
 * The pseudo code in the Atmel manual is broken.
 ********************************************************
 */
void
mcs51_rlca(void)
{
	uint8_t A;
	uint8_t Cnew;
	A = MCS51_GetAcc();
	Cnew = ! !(A & 0x80);
	if (PSW & PSW_CY) {
		A = (A << 1) | 1;
	} else {
		A = (A << 1);
	}
	MCS51_SetAcc(A);
	if (Cnew) {
		PSW |= PSW_CY;
	} else {
		PSW &= ~PSW_CY;
	}
}

/**
 ******************************************************
 * \fn void mcs51_rra(void)
 * rotate right of Accumulator
 ******************************************************
 */
void
mcs51_rra(void)
{
	uint8_t A;
	A = MCS51_GetAcc();
	if (A & 0x1) {
		A = (A >> 1) | 0x80;
	} else {
		A = (A >> 1);
	}
	MCS51_SetAcc(A);
}

/**
 *******************************************************
 * \fn void mcs51_rrca(void)
 * Rotate right of Accumulator and carry.
 *******************************************************
 */
void
mcs51_rrca(void)
{
	uint8_t A;
	uint8_t Cnew;
	A = MCS51_GetAcc();
	Cnew = ! !(A & 0x1);
	if (PSW & PSW_CY) {
		A = (A >> 1) | 0x80;
	} else {
		A = (A >> 1);
	}
	MCS51_SetAcc(A);
	if (Cnew) {
		PSW |= PSW_CY;
	} else {
		PSW &= ~PSW_CY;
	}
}

/**
 ********************************************************
 * \fn void mcs51_setbc(void)
 * Set the carry bit
 ********************************************************
 */
void
mcs51_setbc(void)
{
	PSW |= PSW_CY;
}

/**
 *********************************************************
 * \fn void mcs51_setbbit(void)
 * Set a bit in memory. Uses the Latched bit write for
 * Read-Modify-Write
 *********************************************************
 */
void
mcs51_setbbit(void)
{
	uint8_t bitaddr = MCS51_ReadPgmMem(GET_REG_PC);
	SET_REG_PC(GET_REG_PC + 1);
	MCS51_WriteBitLatched(1, bitaddr);
}

/**
 ******************************************************************
 * void mcs51_sjmprel(void)
 * Short relative jump.
 ******************************************************************
 */
void
mcs51_sjmprel(void)
{
	int8_t rel = MCS51_ReadPgmMem(GET_REG_PC);
	SET_REG_PC(GET_REG_PC + 1 + rel);
}

/**
 ************************************************************** 
 * \fn void mcs51_subbar(void)
 * Subtract with borrow.
 ************************************************************** 
 */
void
mcs51_subbar(void)
{
	int reg = ICODE & 7;
	uint8_t A = MCS51_GetAcc();
	uint8_t R = MCS51_GetRegR(reg);
	uint8_t C = ! !(PSW & PSW_CY);
	uint8_t result = A - C - R;
	MCS51_SetAcc(result);
	PSW = PSW & ~(PSW_CY | PSW_AC | PSW_OV);
	PSW |= sub8_carry(A, R, result) | sub4_carry(A, R, result) | sub8_overflow(A, R, result);
}

/**
 *************************************************************************
 * \fn void mcs51_subbadir(void)
 * Subtract a directly addressed memory location from Acc minus Carry.
 *************************************************************************
 */
void
mcs51_subbadir(void)
{
	uint8_t data;
	uint8_t addr;
	uint8_t result;
	uint8_t A;
	uint8_t C = ! !(PSW & PSW_CY);
	addr = MCS51_ReadPgmMem(GET_REG_PC);
	data = MCS51_ReadMemDirect(addr);
	SET_REG_PC(GET_REG_PC + 1);
	A = MCS51_GetAcc();
	result = A - C - data;
	PSW = PSW & ~(PSW_CY | PSW_AC | PSW_OV);
	PSW |= sub8_carry(A, data, result) |
	    sub4_carry(A, data, result) | sub8_overflow(A, data, result);
	MCS51_SetAcc(result);
}

/**
 *************************************************************************
 *  \fn void mcs51_subbaari(void)
 * subtract an indirectly addressed byte from the accumulator minus carry.
 *************************************************************************
 */
void
mcs51_subbaari(void)
{
	uint8_t A;
	uint8_t R;
	uint8_t op2;
	uint8_t result;
	uint8_t C = ! !(PSW & PSW_CY);
	uint8_t reg = ICODE & 1;
	A = MCS51_GetAcc();
	R = MCS51_GetRegR(reg);
	op2 = MCS51_ReadMemIndirect(R);
	result = A - C - op2;
	PSW = PSW & ~(PSW_CY | PSW_AC | PSW_OV);
	PSW |= sub8_carry(A, op2, result) |
	    sub4_carry(A, op2, result) | sub8_overflow(A, op2, result);
	MCS51_SetAcc(result);
}

/**
 ***********************************************************************
 * \fn void mcs51_subbadata(void)
 * Subtract an immediate byte from accumulator minus carry 
 ***********************************************************************
 */
void
mcs51_subbadata(void)
{
	uint8_t A;
	uint8_t data;
	uint8_t result;
	uint8_t C = ! !(PSW & PSW_CY);
	data = MCS51_ReadPgmMem(GET_REG_PC);
	SET_REG_PC(GET_REG_PC + 1);
	A = MCS51_GetAcc();
	result = A - C - data;
	PSW = PSW & ~(PSW_CY | PSW_AC | PSW_OV);
	PSW |= sub8_carry(A, data, result) |
	    sub4_carry(A, data, result) | sub8_overflow(A, data, result);
	MCS51_SetAcc(result);
}

/**
 ****************************************************************
 * \fn void mcs51_swapa(void)
 *Swapp nibbles in the accumulator.
 ****************************************************************
 */
void
mcs51_swapa(void)
{
	uint8_t A = MCS51_GetAcc();
	A = (A >> 4) | (A << 4);
	MCS51_SetAcc(A);
}

/**
 ****************************************************************
 * Exchange a Register with the Accumulator.
 ****************************************************************
 */
void
mcs51_xchar(void)
{
	uint8_t reg = ICODE & 7;
	uint8_t A, R;
	A = MCS51_GetAcc();
	R = MCS51_GetRegR(reg);
	MCS51_SetAcc(R);
	MCS51_SetRegR(A, reg);
}

/**
 ***********************************************************************
 * \fn void mcs51_xchadir(void)
 * Exchange a directly addressed byte from memory with the Accumulator.
 ***********************************************************************
 */
void
mcs51_xchadir(void)
{
	uint8_t dira = MCS51_ReadPgmMem(GET_REG_PC);
	uint8_t A = MCS51_GetAcc();
	SET_REG_PC(GET_REG_PC + 1);
	MCS51_SetAcc(MCS51_ReadMemDirect(dira));
	MCS51_WriteMemDirect(A, dira);
}

/**
 *******************************************************************
 * \fn void mcs51_xchaari(void)
 * Exchange an indirectly addressed byte with the accumulator.
 *******************************************************************
 */
void
mcs51_xchaari(void)
{
	uint8_t A = MCS51_GetAcc();
	uint8_t reg = ICODE & 1;
	uint8_t R = MCS51_GetRegR(reg);
	uint8_t data = MCS51_ReadMemIndirect(R);
	MCS51_WriteMemIndirect(A, R);
	MCS51_SetAcc(data);
}

/**
 *******************************************************************
 * \fn void mcs51_xchdaari(void)
 * Exchange the lower nibble from accumulator with the lower
 * nibble of a indirectly addressed memory location.
 *******************************************************************
 */
void
mcs51_xchdaari(void)
{
	uint8_t reg = ICODE & 1;
	uint8_t A = MCS51_GetAcc();
	uint8_t R = MCS51_GetRegR(reg);
	uint8_t data = MCS51_ReadMemIndirect(R);
	MCS51_SetAcc((A & 0xf0) | (data & 0xf));
	MCS51_WriteMemIndirect((data & 0xf0) | (A & 0xf), R);
}

/**
 *********************************************************************
 * void mcs51_xrlar(void)
 * Exclusive Or of Accumulator and a register is stored in accumulator 
 *********************************************************************
 */
void
mcs51_xrlar(void)
{
	int reg = ICODE & 7;
	uint8_t A = MCS51_GetAcc();
	uint8_t R = MCS51_GetRegR(reg);
	uint8_t result = A ^ R;
	MCS51_SetAcc(result);
}

/**
 ***************************************************************************
 * \fn void mcs51_xrladir(void) 
 * Exclusive or of the accumulator and a directly addressed memory location
 * is stored in the accumulator
 ***************************************************************************
 */
void
mcs51_xrladir(void)
{
	uint8_t addr;
	uint8_t data;
	uint8_t result;
	uint8_t A;
	addr = MCS51_ReadPgmMem(GET_REG_PC);
	data = MCS51_ReadMemDirect(addr);
	SET_REG_PC(GET_REG_PC + 1);
	A = MCS51_GetAcc();
	result = A ^ data;
	MCS51_SetAcc(result);
}

/**
 ****************************************************************************
 * \fn void mcs51_xrlaari(void)
 * The exclusive OR of the Accumulator and an indirectly addressed memory 
 * location is stored in the Accumulator.
 ****************************************************************************
 */
void
mcs51_xrlaari(void)
{
	uint8_t A;
	uint8_t R;
	uint8_t op2;
	uint8_t result;
	uint8_t reg = ICODE & 1;
	A = MCS51_GetAcc();
	R = MCS51_GetRegR(reg);
	op2 = MCS51_ReadMemIndirect(R);
	result = A ^ op2;
	MCS51_SetAcc(result);
}

/**
 ******************************************************************
 * \fn void mcs51_xrladata(void)
 * Exclusive or of the accumulator and an immediate byte is
 * stored in the accumulator.
 ******************************************************************
 */
void
mcs51_xrladata(void)
{
	uint8_t A;
	uint8_t data;
	uint8_t result;
	data = MCS51_ReadPgmMem(GET_REG_PC);
	SET_REG_PC(GET_REG_PC + 1);
	A = MCS51_GetAcc();
	result = A ^ data;
	MCS51_SetAcc(result);
}

/**
 **************************************************************************
 * \fn void mcs51_xrldira(void) 
 * Exclusive OR of a directly addressed byte in memory and the Accumulator.
 * The result is stored in memory.
 **************************************************************************
 */
void
mcs51_xrldira(void)
{
	uint8_t data;
	uint8_t result;
	uint8_t A;
	uint8_t addr;
	addr = MCS51_ReadPgmMem(GET_REG_PC);
	data = MCS51_ReadLatchedMemDirect(addr);
	A = MCS51_GetAcc();
	result = A ^ data;
	MCS51_WriteMemDirect(result, addr);
	SET_REG_PC(GET_REG_PC + 1);
}

/**
 *******************************************************************
 * \fn void mcs51_xrldirdata(void)
 * Exclusive or of a directly addressed data byte with and immediate
 * is stored in memory.
 * It uses the Latched Port Values for Read-Modify-Write
 *******************************************************************
 */
void
mcs51_xrldirdata(void)
{
	uint8_t addr = MCS51_ReadPgmMem(GET_REG_PC);
	uint8_t imm = MCS51_ReadPgmMem(GET_REG_PC + 1);
	uint8_t data = MCS51_ReadLatchedMemDirect(addr);
	uint8_t result;
	//fprintf(stderr,"XRL addr %02x imm %02x, data %02x\n",addr,imm,data);
	result = data ^ imm;
	MCS51_WriteMemDirect(result, addr);
	SET_REG_PC(GET_REG_PC + 2);
}

void
mcs51_undef(void)
{
	fprintf(stderr, "Undefined MCS51 instruction 0x%02x\n", ICODE);
	exit(1);
}
