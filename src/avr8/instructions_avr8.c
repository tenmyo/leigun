/*
 *************************************************************************************************
 *
 * Atmel AVR8 instruction Set. 
 *
 * State: Working and cycle accurate. 
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

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "avr8_cpu.h"
#include "cycletimer.h"
#include "compiler_extensions.h"
#include "instructions_avr8.h"
#include "sgstring.h"

#define IS_HALFNEG(x) !!((x) & 8)
#define IS_NOTHALFNEG(x) !((x) & 8)

#define IS_NEGB(x) !!((x) & 0x80)
#define IS_NOTNEGB(x) !((x) & 0x80)

#define IS_NEGW(x) !!((x) & 0x8000)
#define IS_NOTNEGW(x) !((x) & 0x8000)

/*
 ************************************************************
 * halffull_add_carry
 * 	Carry operation for 4 + 8 Bit addition
 * v1
 ************************************************************
 */
static inline uint8_t
halffull_add_carry(uint8_t op1, uint8_t op2, uint8_t result)
{
	uint8_t tmp = (op1 & op2) | (op1 & ~result) | (op2 & ~result);
	tmp = (tmp >> 7) | ((tmp & 8) << 2);
	return tmp;
}

static inline uint8_t
half_add_carry(uint8_t op1, uint8_t op2, uint8_t result)
{
	uint8_t tmp = (op1 & op2) | (op1 & ~result) | (op2 & ~result);
	tmp = (tmp & 8) << 2;
	return tmp;
}

/*
 *****************************************************************
 * halffull_sub_carry 
 *	Carry for 4 + 8 Bit subtraction
 * Borrow style carry rd - rr 
 * The carry definintion seems strange for borrow style ???
 * v1
 *****************************************************************
 */
static inline uint8_t
halffull_sub_carry(uint8_t rd, uint8_t rr, uint8_t r)
{
	uint8_t tmp = (~rd & rr) | (rr & r) | (r & ~rd);
	tmp = (tmp >> 7) | ((tmp & 8) << 2);
	return tmp;
}

static inline uint8_t
half_sub_carry(uint8_t rd, uint8_t rr, uint8_t r)
{
	uint8_t tmp = (~rd & rr) | (rr & r) | (r & ~rd);
	tmp = (tmp & 8) << 2;
	return tmp;
}

/*
 ****************************************************
 * sub8_carry
 * 	Carry for  Bit subtraction
 * v1
 * 3 - (-1) = 4, flags = 0x21 
 * 3 - (-128) = 0x83 (-125), flags = 0x0d
 * 
 * -2 - (-1) = -1, flags = 0x35 
 * 5 - 10 = -5, flags = 0x35 
 * 10 - 5 = 5, flags = 0 
 ****************************************************
 */
static inline uint8_t
sub8_carry(uint8_t rd, uint8_t rr, uint8_t r)
{
	return ((~rd & rr) | (rr & r) | (r & ~rd)) >> 7;

}

/*
 ********************************************************************
 * sub8_overflow
 * 	Overflow bit for 8 bit subtraction
 * v1
 * -5 - 127 = 0x7c flags 0x38 
 * 3 - (-128) = 0x83 (-125), flags = 0x0d
 ********************************************************************
 */

static inline uint8_t
sub8_overflow(uint8_t rd, uint8_t rr, uint8_t r)
{
	uint8_t tmp = (rd & ~rr & ~r) | (~rd & rr & r);
	return (tmp >> 4) & 8;
}

static inline uint8_t
sub16_carry(uint16_t rd, uint16_t rr, uint16_t r)
{
	return ((~rd & rr) | (rr & r) | (r & ~rd)) >> 15;

}

static inline uint8_t
sub16_overflow(uint16_t rd, uint16_t rr, uint16_t r)
{
	uint8_t tmp = (rd & ~rr & ~r) | (~rd & rr & r);
	return (tmp >> 12) & 8;
}

/*
 **********************************************************************
 * add8 overflow
 *	Overflow for 8 bit addition
 * v1
 **********************************************************************
 */

static inline uint8_t
add8_overflow(uint8_t op1, uint8_t op2, uint8_t result)
{
	return (((op1 & op2 & ~result) | (~op1 & ~op2 & result)) >> 4) & 8;
}

static inline uint8_t
flg_negative8(uint8_t result)
{
	if (IS_NEGB(result)) {
		return FLG_N;
	} else {
		return 0;
	}
}

/*
 **************************************************
 * flg_negative16
 * 	return FLG_N if 16 Bit word is negative
 * v1
 **************************************************
 */
static inline uint8_t
flg_negative16(uint16_t result)
{
	if (IS_NEGW(result)) {
		return FLG_N;
	} else {
		return 0;
	}
}

/*
 *******************************************
 * flg_sign 
 * 	FLG_N ^ FLG_V
 *******************************************
 */
static inline uint8_t
flg_sign(uint8_t sreg)
{
	return ((sreg << 2) ^ (sreg << 1)) & FLG_S;
}

static inline uint8_t
flg_zero(uint32_t result)
{
	if (result == 0) {
		return FLG_Z;
	} else {
		return 0;
	}
}

static inline void
add8_flags(uint32_t op1, uint32_t op2, uint32_t result)
{
	uint8_t sreg;
	uint8_t flags;
	unsigned int idx = (op1 >> 3) | ((op2 & 0xf8) << 2) | ((result & 0xf8) << 7);
	flags = gavr8.add_flags[idx];
	sreg = GET_SREG;
	sreg = (sreg & ~(FLG_V | FLG_S | FLG_Z | FLG_C | FLG_H | FLG_N)) | flags;
	if (result == 0) {
		sreg |= FLG_Z;
	}
	SET_SREG(sreg);
}

static inline void
sub8_flags(uint8_t op1, uint8_t op2, uint8_t result)
{
	unsigned int idx = (op1 >> 3) | ((op2 & 0xf8) << 2) | ((result & 0xf8) << 7);
	uint8_t flags;
	flags = gavr8.sub_flags[idx];
	SET_SREG((GET_SREG & ~(FLG_V | FLG_S | FLG_C | FLG_H)) | flags);
}

static inline void
sub16_flags(uint16_t op1, uint16_t op2, uint16_t result)
{
	unsigned int idx =
	    ((op1 & 0xf800) >> 11) | ((op2 & 0xf800) >> 6) | ((result & 0xf800) >> 1);
	uint8_t flags;
	flags = gavr8.sub_flags[idx];
	SET_SREG((GET_SREG & ~(FLG_V | FLG_S | FLG_C | FLG_H)) | flags);
}

/*
 *****************************************************
 * adc
 *	Add with carry
 * v1
 *****************************************************
 */
void
avr8_adc(void)
{
	uint16_t icode = ICODE;
	int rd = (icode >> 4) & 0x1f;
	int rr = (icode & 0xf) | ((icode >> 5) & 0x10);
	uint8_t Rd = AVR8_ReadReg(rd);
	uint8_t Rr = AVR8_ReadReg(rr);
	uint8_t R;
	uint8_t sreg = GET_SREG;
	uint8_t C = !!(sreg & FLG_C);
	R = Rd + Rr + C;
	AVR8_WriteReg(R, rd);
	add8_flags(Rd, Rr, R);
	CycleCounter += 1;
}

/*
 ********************************************
 * avr8_add
 * 	Add without carry
 * v1
 ********************************************
 */
void
avr8_add(void)
{
	uint16_t icode = ICODE;
	int rd = (icode >> 4) & 0x1f;
	int rr = (icode & 0xf) | ((icode >> 5) & 0x10);
	uint8_t Rd = AVR8_ReadReg(rd);
	uint8_t Rr = AVR8_ReadReg(rr);
	uint8_t R;
	R = Rd + Rr;
	AVR8_WriteReg(R, rd);
	add8_flags(Rd, Rr, R);
	CycleCounter += 1;
}

/*
 ***************************************************************
 * avr8_adiw
 *	Add an immediate (unsigned 0-63) to a register pair
 ***************************************************************
 */
void
avr8_adiw(void)
{
	uint16_t icode = ICODE;
	int rd = ((icode & 0x30) >> 3) + 24;
	uint16_t K = (icode & 0xf) | ((icode & 0x00c0) >> 2);
	uint16_t Rd = AVR8_ReadReg16(rd);
	uint16_t R = Rd + K;
	uint8_t sreg = GET_SREG;
	AVR8_WriteReg16(R, rd);
	sreg = sreg & ~(FLG_S | FLG_V | FLG_N | FLG_Z | FLG_C);
	sreg |= ((R & ~Rd) >> 12) & FLG_V;
	sreg |= ((~R & Rd) >> 15);	/* Carry */
	sreg |= flg_negative16(R);
	if (R == 0) {
		sreg |= FLG_Z;
	}
	sreg |= flg_sign(sreg);
	SET_SREG(sreg);
	CycleCounter += 2;
}

/*
 *****************************************************************
 * avr8_and
 *	"And" of two registers
 * v1
 *****************************************************************
 */
void
avr8_and(void)
{
	uint16_t icode = ICODE;
	int rd = (icode & 0x01f0) >> 4;
	int rr = (icode & 0xf) | ((icode >> 5) & 0x10);
	uint8_t Rd = AVR8_ReadReg(rd);
	uint8_t Rr = AVR8_ReadReg(rr);
	uint8_t R;
	uint8_t sreg = GET_SREG;
	R = Rd & Rr;
	AVR8_WriteReg(R, rd);
	sreg = sreg & ~(FLG_S | FLG_V | FLG_N | FLG_Z);
	sreg |= flg_negative8(R);
	sreg |= flg_zero(R);
	sreg |= flg_sign(sreg);
	SET_SREG(sreg);
	CycleCounter += 1;
}

/*
 ***************************************************************
 * avr8_andi
 * v1
 ***************************************************************
 */
void
avr8_andi(void)
{
	uint16_t icode = ICODE;
	int rd = ((icode & 0xf0) >> 4) + 16;
	uint8_t K = (icode & 0xf) | ((icode >> 4) & 0xf0);
	uint8_t Rd = AVR8_ReadReg(rd);
	uint8_t sreg = GET_SREG;
	uint8_t R;
	R = Rd & K;
	AVR8_WriteReg(R, rd);
	sreg = sreg & ~(FLG_S | FLG_V | FLG_N | FLG_Z);
	sreg |= flg_negative8(R);
	sreg |= flg_zero(R);
	sreg |= flg_sign(sreg);
	SET_SREG(sreg);
	CycleCounter += 1;
}

/*
 **********************************************************
 * avr8_asr
 * 	Arithmetic shift left by one
 * v1
 **********************************************************
 */
void
avr8_asr(void)
{
	uint16_t icode = ICODE;
	int rd = (icode >> 4) & 0x1f;
	int8_t Rd = AVR8_ReadReg(rd);
	uint8_t sreg = GET_SREG;
	int8_t R;
	R = (Rd >> 1);
	AVR8_WriteReg(R, rd);
	sreg = sreg & ~(FLG_S | FLG_V | FLG_N | FLG_Z | FLG_C);
	sreg |= flg_negative8(R);
	sreg |= flg_zero(R);
	sreg |= Rd & FLG_C;	/* FLG_C = 1 */
	sreg |= ((sreg << 1) ^ (sreg << 3)) & FLG_V;
	sreg |= flg_sign(sreg);
	SET_SREG(sreg);
	CycleCounter += 1;
}

/*
 **************************************************
 * avr8_bclr
 *	Bit clear in status register
 **************************************************
 */
void
avr8_bclr(void)
{
	uint16_t icode = ICODE;
	int s = (icode >> 4) & 7;
	uint8_t sreg = GET_SREG;
	sreg &= ~(1 << s);
	SET_SREG(sreg);
	if (s == 7) {
		AVR8_UpdateCpuSignals();
	}
	CycleCounter += 1;
}

/*
 *************************************************
 * avr8_bld
 * 	Load the T-Flag into a bit of a register
 * v1
 *************************************************
 */
void
avr8_bld(void)
{
	uint16_t icode = ICODE;
	int rd = (icode >> 4) & 0x1f;
	int b = icode & 0x7;
	uint8_t sreg = GET_SREG;
	uint8_t Rd = AVR8_ReadReg(rd);
	if (sreg & FLG_T) {
		Rd |= (1 << b);
	} else {
		Rd &= ~(1 << b);
	}
	AVR8_WriteReg(Rd, rd);
	CycleCounter += 1;
}

/*
 ****************************************************
 * avr8_brbc
 *	Branch if bit in SREG is clear
 * v1
 ****************************************************
 */
void
avr8_brbc(void)
{
	uint16_t icode = ICODE;
	int s = icode & 0x7;
	uint8_t sreg = GET_SREG;
	int8_t k = ((int8_t) ((icode >> 2) & 0xfe)) >> 1;
	/* Sign extend from signed 7 to signed 8 Bit */
	if (!(sreg & (1 << s))) {
		SET_REG_PC(GET_REG_PC + k);
		CycleCounter += 2;
	} else {
		CycleCounter += 1;
	}
}

/*
 ***********************************************
 * avr8_brbs
 * 	Branch if bit in SREG is set
 * v1
 ***********************************************
 */
void
avr8_brbs(void)
{
	uint16_t icode = ICODE;
	int s = icode & 0x7;
	uint8_t sreg = GET_SREG;
	int8_t k = ((int8_t) ((icode >> 2) & 0xfe)) >> 1;
	/* Sign extend from signed 7 to signed 8 Bit */
	if ((sreg & (1 << s))) {
		SET_REG_PC(GET_REG_PC + k);
		CycleCounter += 2;
	} else {
		CycleCounter += 1;
	}
}

void
avr8_break(void)
{
	fprintf(stderr, "avr8_break not implemented\n");
	CycleCounter += 1;
	AVR8_Break();
}

/*
 *******************************************************************
 * avr8_bset
 *	Bit set in SREG
 * v1
 *******************************************************************
 */
void
avr8_bset(void)
{
	uint16_t icode = ICODE;
	int s = (icode >> 4) & 0x7;
	uint8_t sreg = GET_SREG;
	/* Special case SEI with delayed interrupt enable */
	if (s == 7) {
		/* Untested: CPU behaviour with many consequent "sei" instructions */
		if (!(sreg & (1 << 7))) {
			//fprintf(stderr,"sei ******************** in %02x\n",GET_REG_PC << 1);
			sreg |= (1 << 7);
			gavr8.cpu_signals_raw |= AVR8_SIG_IRQENABLE;
			/* Update CPU signals before setting SREG ! This makes it delayed */
			AVR8_UpdateCpuSignals();
			SET_SREG(sreg);
		}
	} else {
		sreg |= (1 << s);
		SET_SREG(sreg);
	}
	CycleCounter += 1;
}

/*
 *************************************************************
 * avr8_bst
 *	Store bit from register in T-Flag
 * v1
 *************************************************************
 */
void
avr8_bst(void)
{
	uint16_t icode = ICODE;
	int rd = (icode >> 4) & 0x1f;
	int b = icode & 0x7;
	uint8_t sreg = GET_SREG;
	uint8_t Rd = AVR8_ReadReg(rd);

	if (Rd & (1 << b)) {
		sreg |= FLG_T;
	} else {
		sreg &= ~FLG_T;
	}
	SET_SREG(sreg);
	CycleCounter += 1;
}

/*
 * ------------------------------------------------------------
 * CALL instruction
 * Atmega8 doesn't have it in instruction set summary,
 * so this should be excluded for some CPU
 * The byteorder is not documented but was found on
 * http://savannah.nongnu.org/task/?3693 to be big endian
 * (least significant is pushed first and stack is growing
 * downwards) 
 * v1
 * ------------------------------------------------------------
 */
void
avr8_call(void)
{
	uint32_t k;
	uint16_t sp = GET_REG_SP;
	uint32_t nia = GET_REG_PC + 1;
	k = AVR8_ReadAppMem(GET_REG_PC);
	AVR8_WriteMem8(nia & 0xff, sp--);
	AVR8_WriteMem8((nia >> 8) & 0xff, sp--);
	SET_REG_SP(sp);
	SET_REG_PC(k);
	CycleCounter += 4;
}

void
avr8_call_24(void)
{
	uint16_t icode = ICODE;
	uint32_t k;
	uint32_t high_k;
	uint16_t sp = GET_REG_SP;
	uint32_t nia = GET_REG_PC + 1;
	k = AVR8_ReadAppMem(GET_REG_PC);
	high_k = (icode & 1) | ((icode >> 3) & 0x3e);
	k |= (high_k << 16);
	AVR8_WriteMem8(nia & 0xff, sp--);
	AVR8_WriteMem8((nia >> 8) & 0xff, sp--);
	AVR8_WriteMem8((nia >> 16) & 0xff, sp--);
	CycleCounter += 1;
	SET_REG_SP(sp);
//      fprintf(stderr,"Call %04x at %04x\n",k << 1,GET_REG_PC << 1);
	SET_REG_PC(k);
	CycleCounter += 4;
}

/*
 ************************************************************
 * avr8_cbi
 *	Clear bit in IO register
 * v1
 ************************************************************
 */
void
avr8_cbi(void)
{
	uint16_t icode = ICODE;
	int b = icode & 0x7;
	int A = (icode >> 3) & 0x1f;
	uint8_t ioval = AVR8_ReadIO8(A);
	ioval &= ~(1 << b);
	AVR8_WriteIO8(ioval, A);
	CycleCounter += 2;
}

/*
 **************************************************************
 * avr_com
 *	Complement of a register
 **************************************************************
 */
void
avr8_com(void)
{
	uint16_t icode = ICODE;
	int rd = (icode >> 4) & 0x1f;
	uint8_t Rd = AVR8_ReadReg(rd);
	uint8_t sreg = GET_SREG;
	Rd = 0xff - Rd;
	AVR8_WriteReg(Rd, rd);
	sreg = sreg & ~(FLG_S | FLG_V | FLG_N | FLG_Z);
	sreg |= FLG_C;
	sreg |= flg_negative8(Rd);
	sreg |= flg_zero(Rd);
	sreg |= flg_sign(sreg);
	SET_SREG(sreg);
	CycleCounter += 1;
}

/*
 ****************************************************************
 * avr8_cp
 *	Compare
 * v1
 ****************************************************************
 */
void
avr8_cp(void)
{
	uint16_t icode = ICODE;
	int rr = (icode & 0xf) | ((icode >> 5) & 0x10);
	int rd = (icode >> 4) & 0x1f;
	uint8_t Rr = AVR8_ReadReg(rr);
	uint8_t Rd = AVR8_ReadReg(rd);
	uint8_t R;
	R = Rd - Rr;
	sub8_flags(Rd, Rr, R);
	SET_SREG((GET_SREG & ~FLG_Z) | flg_zero(R));
	CycleCounter += 1;
}

/*
 *******************************************************************
 * avr8_cpc
 *	Compare with carry
 * v1
 *******************************************************************
 */

void
avr8_cpc(void)
{
	uint16_t icode = ICODE;
	int rr = (icode & 0xf) | ((icode >> 5) & 0x10);
	int rd = (icode >> 4) & 0x1f;
	uint8_t Rr = AVR8_ReadReg(rr);
	uint8_t Rd = AVR8_ReadReg(rd);
	uint8_t R;
	uint8_t sreg = GET_SREG;
	uint8_t C = !!(sreg & FLG_C);
	R = Rd - Rr - C;
	sub8_flags(Rd, Rr, R);
	if (R != 0) {
		SET_SREG(GET_SREG & ~FLG_Z);
	}
	CycleCounter += 1;
}

/*
 *******************************************************
 * avr8_cpi
 * 	Compare with immediate
 * v1
 *******************************************************
 */
void
avr8_cpi(void)
{
	uint16_t icode = ICODE;
	int rd = ((icode >> 4) & 0xf) | 0x10;
	uint8_t Rd = AVR8_ReadReg(rd);
	uint8_t K = (icode & 0xf) | ((icode >> 4) & 0xf0);
	uint8_t R;
	R = Rd - K;
	sub8_flags(Rd, K, R);
	SET_SREG((GET_SREG & ~FLG_Z) | flg_zero(R));
	CycleCounter += 1;
}

/*
 ************************************************************
 * avr8_cpse 
 * 	Compare and skip if equal
 ************************************************************
 */
void
avr8_cpse(void)
{
	uint16_t icode = ICODE;
	int rr = (icode & 0xf) | ((icode >> 5) & 0x10);
	int rd = (icode >> 4) & 0x1f;
	uint8_t Rr = AVR8_ReadReg(rr);
	uint8_t Rd = AVR8_ReadReg(rd);
	if (Rd == Rr) {
		AVR8_SkipInstruction();
	}
	CycleCounter += 1;
}

/*
 **********************************************************
 * avr8_dec
 * 	Decrement register by one
 * v1
 **********************************************************
 */
void
avr8_dec(void)
{
	uint16_t icode = ICODE;
	int rd = (icode >> 4) & 0x1f;
	uint8_t R;
	uint8_t Rd = AVR8_ReadReg(rd);
	uint8_t sreg = GET_SREG;
	R = Rd - 1;
	AVR8_WriteReg(R, rd);
	sreg = sreg & ~(FLG_S | FLG_V | FLG_N | FLG_Z);
	if (unlikely(Rd == 0x80)) {
		sreg |= FLG_V;
	}
	sreg |= flg_negative8(R);
	sreg |= flg_zero(R);
	sreg |= flg_sign(sreg);
	SET_SREG(sreg);
	CycleCounter += 1;
}

/*
 ************************************************************
 * avr8_eicall
 * 	Extended indirect call to subroutine
 * v1
 ************************************************************
 */

void
avr8_eicall(void)
{
	uint32_t z;
	uint32_t eind;
	uint32_t addr;
	uint16_t pc = GET_REG_PC;
	uint16_t sp = GET_REG_SP;
	z = AVR8_ReadReg16(NR_REG_Z);
	eind = AVR8_ReadMem8(IOA_EIND);
	addr = (eind << 16) | z;
	AVR8_WriteMem8(pc & 0xff, sp--);
	AVR8_WriteMem8((pc >> 8) & 0xff, sp--);
	AVR8_WriteMem8((pc >> 16) & 0xff, sp--);
	SET_REG_SP(sp);
	SET_REG_PC(addr);
	CycleCounter += 4;
}

/*
 **********************************************************
 * avr8_eijmp
 * 	Extended indirect jump
 * v1
 **********************************************************
 */
void
avr8_eijmp(void)
{
	uint32_t z;
	uint32_t eind;
	uint32_t addr;
	z = AVR8_ReadReg16(NR_REG_Z);
	eind = AVR8_ReadMem8(IOA_EIND);
	addr = (eind << 16) | z;
	SET_REG_PC(addr);
	CycleCounter += 2;
}

/*
 ************************************************************
 * avr8_elpm1
 *	Extended load from programm memory
 *	Case 1: R0 implied, Z unchanged
 * v1
 ************************************************************
 */

void
avr8_elpm1(void)
{
	uint32_t z;
	uint32_t rampz;
	uint32_t addr;
	uint8_t R;
	z = AVR8_ReadReg16(NR_REG_Z);
	rampz = RAMPZ;
	addr = (rampz << 16) | z;
	R = AVR8_ReadAppMem8(addr);
	AVR8_WriteReg(R, 0);
	CycleCounter += 3;
}

/*
 ************************************************************
 * avr8_elpm2
 *	Extended load from programm memory
 * 	Case 2: Z unchanged
 * v1
 ************************************************************
 */
void
avr8_elpm2(void)
{
	uint32_t z;
	uint32_t rampz;
	uint32_t addr;
	int rd = (ICODE >> 4) & 0x1f;
	uint8_t R;
	z = AVR8_ReadReg16(NR_REG_Z);
	rampz = RAMPZ;
	addr = (rampz << 16) | z;
	R = AVR8_ReadAppMem8(addr);
	AVR8_WriteReg(R, rd);
	CycleCounter += 3;
}

/*
 ************************************************************
 * avr8_elpm2
 *	Extended load from programm memory
 * 	Case 3: Z incremented 
 * v1
 ************************************************************
 */

void
avr8_elpm3(void)
{
	uint32_t z;
	uint32_t rampz;
	uint32_t addr;
	int rd = (ICODE >> 4) & 0x1f;
	uint8_t R;
	z = AVR8_ReadReg16(NR_REG_Z);
	rampz = RAMPZ;
	addr = (rampz << 16) | z;
	R = AVR8_ReadAppMem8(addr);
	AVR8_WriteReg(R, rd);
	addr++;
	rampz = addr >> 16;
	z = addr & 0xffff;
	RAMPZ = rampz;
	AVR8_WriteReg16(z, NR_REG_Z);
	CycleCounter += 3;
}

/*
 *******************************************************
 * avr8_eor
 * 	Exclusive or
 * v1
 *******************************************************
 */

void
avr8_eor(void)
{
	uint16_t icode = ICODE;
	int rd = (icode >> 4) & 0x1f;
	int rr = (icode & 0xf) | ((icode & 0x200) >> 5);
	uint8_t Rr, Rd, R;
	uint8_t sreg = GET_SREG;
	Rd = AVR8_ReadReg(rd);
	Rr = AVR8_ReadReg(rr);
	R = Rd ^ Rr;
	sreg = sreg & ~(FLG_S | FLG_V | FLG_N | FLG_Z);
	sreg |= flg_negative8(R);
	sreg |= flg_zero(R);
	sreg |= flg_sign(sreg);
	SET_SREG(sreg);
	AVR8_WriteReg(R, rd);
	CycleCounter += 1;
}

/*
 ******************************************************************************
 * avr8_fmul
 * 	Multiply two 1.7 numbers. The result is 1.15 format. The highest bit
 * 	of the 2.14 result is moved to the carry
 * v1
 ******************************************************************************
 */
void
avr8_fmul(void)
{
	uint16_t icode = ICODE;
	int rr = 0x10 | (icode & 7);
	int rd = 0x10 | ((icode >> 4) & 7);
	uint8_t Rr, Rd;
	uint16_t R;
	uint8_t sreg = GET_SREG;
	sreg = sreg & ~(FLG_C | FLG_Z);
	Rd = AVR8_ReadReg(rd);
	Rr = AVR8_ReadReg(rr);
	R = Rr * Rd;
	if (R & (1 << 15)) {
		sreg |= FLG_C;
	}
	R = R << 1;
	sreg |= flg_zero(R);
	SET_SREG(sreg);
	AVR8_WriteReg(R >> 8, 1);
	AVR8_WriteReg(R & 0xff, 0);
	CycleCounter += 2;
}

/*
 ****************************************************************
 * avr8_fmuls
 *	Multiply two signed 1.7 numbers to one 1.15 number
 *	the highest bit of the intermediate 2.14 result 
 * 	goes to the carry
 * v1
 ****************************************************************
 */
void
avr8_fmuls(void)
{
	uint16_t icode = ICODE;
	int rr = 0x10 | (icode & 7);
	int rd = 0x10 | ((icode >> 4) & 7);
	int8_t Rr, Rd;
	int16_t R;
	uint8_t sreg = GET_SREG;
	sreg = sreg & ~(FLG_C | FLG_Z);
	Rd = AVR8_ReadReg(rd);
	Rr = AVR8_ReadReg(rr);
	R = Rr * Rd;
	if (R & (1 << 15)) {
		sreg |= FLG_C;
	}
	R = R << 1;
	sreg |= flg_zero(R);
	SET_SREG(sreg);
	AVR8_WriteReg(R >> 8, 1);
	AVR8_WriteReg(R & 0xff, 0);
	CycleCounter += 2;
}

/*
 *******************************************************************
 * avr8_fmulsu
 * 	Multipliy a signed 1.7 number with an unsigned 1.7 number
 * v1
 *******************************************************************
 */
void
avr8_fmulsu(void)
{
	uint16_t icode = ICODE;
	int rr = 0x10 | (icode & 7);
	int rd = 0x10 | ((icode >> 4) & 7);
	uint8_t Rr;
	int8_t Rd;
	int16_t R;
	uint8_t sreg = GET_SREG;
	sreg = sreg & ~(FLG_C | FLG_Z);
	Rd = AVR8_ReadReg(rd);
	Rr = AVR8_ReadReg(rr);
	R = Rr * Rd;
	if (R & (1 << 15)) {
		sreg |= FLG_C;
	}
	R = R << 1;
	sreg |= flg_zero(R);
	SET_SREG(sreg);
	AVR8_WriteReg(R >> 8, 1);
	AVR8_WriteReg(R & 0xff, 0);
	CycleCounter += 2;
}

/*
 ***************************************************************************
 * avr8_icall
 *	Indirect call, Jump to Z register (upper 8 Bits padded with zero
 *	when 22 Bit PC is used)
 * v1
 ***************************************************************************
 */
void
avr8_icall(void)
{
	uint16_t z;
	uint16_t sp = GET_REG_SP;
	z = AVR8_ReadReg16(NR_REG_Z);
	uint16_t pc = GET_REG_PC;
	AVR8_WriteMem8(pc & 0xff, sp--);
	AVR8_WriteMem8((pc >> 8) & 0xff, sp--);
	SET_REG_SP(sp);
	SET_REG_PC(z);
	CycleCounter += 3;
}

void
avr8_icall_24(void)
{
	uint16_t z;
	uint16_t sp = GET_REG_SP;
	z = AVR8_ReadReg16(NR_REG_Z);
	uint16_t pc = GET_REG_PC;
	AVR8_WriteMem8(pc & 0xff, sp--);
	AVR8_WriteMem8((pc >> 8) & 0xff, sp--);
	AVR8_WriteMem8((pc >> 16) & 0xff, sp--);
	CycleCounter += 1;
	SET_REG_SP(sp);
	SET_REG_PC(z);
	CycleCounter += 4;
}

/*
 ***************************************************************************
 * avr8_ijmp
 *	Indirect jump to Z register. Upper 8 bits are paded with zero
 *	when 24 bit PC is used. 
 * v1
 ***************************************************************************
 */
void
avr8_ijmp(void)
{
	uint16_t z;
	z = AVR8_ReadReg16(NR_REG_Z);
	SET_REG_PC(z);
	CycleCounter += 2;
}

/*
 ************************************************************************
 * avr8_in
 *	Read from IO register 0x20-0x5f
 * v1
 ************************************************************************
 */
void
avr8_in(void)
{
	uint16_t icode = ICODE;
	int a = (icode & 0xf) | ((icode >> 5) & 0x30);
	int rd = (icode >> 4) & 0x1f;
	uint8_t in = AVR8_ReadIO8(a);
	AVR8_WriteReg(in, rd);
	CycleCounter += 1;
}

/*
 **********************************************************************
 * avr_inc
 * 	Increment a register by one
 * v1
 **********************************************************************
 */
void
avr8_inc(void)
{
	uint16_t icode = ICODE;
	int rd = (icode >> 4) & 0x1f;
	uint8_t Rd;
	uint8_t result;
	uint8_t sreg = GET_SREG;
	Rd = AVR8_ReadReg(rd);
	result = Rd + 1;
	AVR8_WriteReg(result, rd);
	sreg = sreg & ~(FLG_S | FLG_V | FLG_N | FLG_Z);
	if (Rd == 127) {
		sreg |= FLG_V;
	}
	sreg |= flg_negative8(result);
	sreg |= flg_zero(result);
	sreg |= flg_sign(sreg);
	SET_SREG(sreg);
	CycleCounter += 1;
}

/*
 *****************************************************************************
 * avr8_jmp
 *	Jump
 * v1
 *****************************************************************************
 */

void
avr8_jmp(void)
{
	uint16_t k = AVR8_ReadAppMem(GET_REG_PC);
	SET_REG_PC(k);
	CycleCounter += 3;
}

void
avr8_jmp_24(void)
{
	uint16_t icode1 = ICODE;
	uint16_t icode2 = AVR8_ReadAppMem(GET_REG_PC);
	uint32_t k;
	k = icode2 | (((icode1 & 1) | ((icode1 >> 3) & 0x3e)) << 16);
	SET_REG_PC(k);
	CycleCounter += 3;
}

/*
 ****************************************************************
 * avr8_ld1
 *	load from data space 
 *	Variant 1: Do not increment X register
 * v1
 ****************************************************************
 */

void
avr8_ld1(void)
{
	uint8_t Rd;
	uint16_t icode = ICODE;
	int rd = (icode >> 4) & 0x1f;
	uint16_t x = AVR8_ReadReg16(NR_REG_X);
	Rd = AVR8_ReadMem8(x);
	AVR8_WriteReg(Rd, rd);
	CycleCounter += 2;
}

/*
 **************************************************************
 * avr8_ld2
 *	Load from data space
 * 	Variant 2: Post increment X register
 * v1
 **************************************************************
 */

void
avr8_ld2(void)
{
	uint8_t Rd;
	uint16_t icode = ICODE;
	int rd = (icode >> 4) & 0x1f;
	uint16_t x = AVR8_ReadReg16(NR_REG_X);
	Rd = AVR8_ReadMem8(x);
	AVR8_WriteReg(Rd, rd);
	x++;
	AVR8_WriteReg16(x, NR_REG_X);
	CycleCounter += 2;
}

/*
 **************************************************************
 * avr8_ld3
 *	Load from data space
 *	Variant3: Pre decrement X register
 **************************************************************
 */
void
avr8_ld3(void)
{
	uint8_t Rd;
	uint16_t icode = ICODE;
	int rd = (icode >> 4) & 0x1f;
	uint16_t x = AVR8_ReadReg16(NR_REG_X);
	x--;
	AVR8_WriteReg16(x, NR_REG_X);
	Rd = AVR8_ReadMem8(x);
	AVR8_WriteReg(Rd, rd);
	CycleCounter += 2;
}

/*
 ********************************************************************
 * avr8_ldy2
 * 	Load indirect from dataspace using index Y 
 *	Variant 2: Post increment Y	
 * v1
 ********************************************************************
 */
void
avr8_ldy2(void)
{
	uint8_t Rd;
	uint16_t icode = ICODE;
	int rd = (icode >> 4) & 0x1f;
	uint16_t y = AVR8_ReadReg16(NR_REG_Y);
	Rd = AVR8_ReadMem8(y);
	AVR8_WriteReg(Rd, rd);
	y++;
	AVR8_WriteReg16(y, NR_REG_Y);
	CycleCounter += 2;
}

/*
 ********************************************************************
 * avr8_ldy3
 * 	Load indirect from dataspace using index Y 
 *	Variant 3: Pre decrement Y	
 * v1
 ********************************************************************
 */

void
avr8_ldy3(void)
{
	uint8_t Rd;
	uint16_t icode = ICODE;
	int rd = (icode >> 4) & 0x1f;
	uint16_t y = AVR8_ReadReg16(NR_REG_Y);
	y = y - 1;
	AVR8_WriteReg16(y, NR_REG_Y);
	Rd = AVR8_ReadMem8(y);
	AVR8_WriteReg(Rd, rd);
	CycleCounter += 2;
}

/*
 ***************************************************************
 * avr8_ldy4
 *	Load indirect from dataspace using index Y
 *	Variant 4: Load from Y + displacement, Y unchanged
 *	Includes variant 1 with no displacement
 * v1
 ***************************************************************
 */
void
avr8_ldy4(void)
{
	uint8_t Rd;
	uint16_t icode = ICODE;
	uint32_t addr;
	uint8_t q = (icode & 7) | ((icode >> 7) & 0x18) | ((icode >> 8) & 0x20);
	int rd = (icode >> 4) & 0x1f;
	uint16_t y = AVR8_ReadReg16(NR_REG_Y);
	addr = (y + q) & 0xffff;
	Rd = AVR8_ReadMem8(addr);
	AVR8_WriteReg(Rd, rd);
	CycleCounter += 2;
}

/*
 ********************************************************************
 * avr8_ldz2
 *	Load indirect from dataspace using index Z
 *	Variant 2:	Post increment of Z
 * v1
 ********************************************************************
 */
void
avr8_ldz2(void)
{
	uint8_t Rd;
	uint16_t icode = ICODE;
	int rd = (icode >> 4) & 0x1f;
	uint16_t z = AVR8_ReadReg16(NR_REG_Z);
	Rd = AVR8_ReadMem8(z);
	AVR8_WriteReg(Rd, rd);
	z = z + 1;
	AVR8_WriteReg16(z, NR_REG_Z);
	CycleCounter += 2;
}

/*
 ********************************************************
 * avr8_ldz3
 *	Load indirect from datapace using index Z
 *	Variant 3: Pre decrement Z
 ********************************************************
 */

void
avr8_ldz3(void)
{
	uint8_t Rd;
	uint16_t icode = ICODE;
	int rd = (icode >> 4) & 0x1f;
	uint16_t z = AVR8_ReadReg16(NR_REG_Z);
	z = z - 1;
	AVR8_WriteReg16(z, NR_REG_Z);
	Rd = AVR8_ReadMem8(z);
	AVR8_WriteReg(Rd, rd);
	CycleCounter += 2;
}

/*
 ********************************************************
 * avr8_ldz4
 *	Load indirect from datapace using index Z
 *	Variant 4: Displacement 0-63 
 * v1
 ********************************************************
 */

void
avr8_ldz4(void)
{
	uint8_t Rd;
	uint16_t icode = ICODE;
	uint32_t addr;
	uint8_t q = (icode & 7) | ((icode >> 7) & 0x18) | ((icode >> 8) & 0x20);
	int rd = (icode >> 4) & 0x1f;
	uint16_t z = AVR8_ReadReg16(NR_REG_Z);
	addr = (z + q) & 0xffff;
	Rd = AVR8_ReadMem8(addr);
	AVR8_WriteReg(Rd, rd);
	CycleCounter += 2;
}

/*
 ******************************************************************************
 * avr8_ldi
 * 	Load an 8 Bit immediate into a register from 16 to 31
 * v1
 ******************************************************************************
 */
void
avr8_ldi(void)
{
	uint16_t icode = ICODE;
	uint8_t K = (icode & 0xf) | ((icode >> 4) & 0xf0);
	int rd = ((icode >> 4) & 0xf) | 0x10;
	AVR8_WriteReg(K, rd);
	CycleCounter += 1;
}

/*
 ***************************************************************
 * avr_lds
 * 	Load an 8 Bit value from the dataspace to a register
 * v1
 ***************************************************************
 */
void
avr8_lds(void)
{
	uint16_t icode = ICODE;
	uint16_t icode2 = AVR8_ReadAppMem(GET_REG_PC);
	int rd = (icode >> 4) & 0x1f;
	uint32_t addr;
	uint8_t Rd;
	addr = icode2;
	SET_REG_PC(GET_REG_PC + 1);
	Rd = AVR8_ReadMem8(addr);
	AVR8_WriteReg(Rd, rd);
	CycleCounter += 2;
}

/*
 ************************************************************************
 * avr8_lpm1
 *	Load from Program memory 
 *	Variant 1: Z register unchanged, implied R0 as destination
 ************************************************************************
 */
void
avr8_lpm1(void)
{
	uint32_t addr;
	uint8_t pm;
	addr = AVR8_ReadReg16(NR_REG_Z);
	pm = AVR8_ReadAppMem8(addr);
	AVR8_WriteReg(pm, 0);
	CycleCounter += 3;
}

void
avr8_lpm1_24(void)
{
	uint32_t addr, rampz;
	uint8_t pm;
	addr = AVR8_ReadReg16(NR_REG_Z);
	rampz = AVR8_ReadMem8(IOA_RAMPZ);
	addr |= (rampz << 16);
	pm = AVR8_ReadAppMem8(addr);
	AVR8_WriteReg(pm, 0);
	CycleCounter += 3;
}

/*
 ******************************************************************
 * avr8_lpm2
 * 	Load from Program memory
 *	Variant 2: Z register unchanged 
 * v1
 ******************************************************************
 */

void
avr8_lpm2(void)
{
	uint16_t icode = ICODE;
	int rd = (icode >> 4) & 0x1f;
	uint32_t addr;
	uint16_t pm;
	addr = AVR8_ReadReg16(NR_REG_Z);
	pm = AVR8_ReadAppMem8(addr);
	AVR8_WriteReg(pm, rd);
	CycleCounter += 3;
	//fprintf(stderr,"LPM2 %02x from %04x at %04x\n",pm,addr,GET_REG_PC << 1);
}

void
avr8_lpm2_24(void)
{
	uint16_t icode = ICODE;
	int rd = (icode >> 4) & 0x1f;
	uint32_t addr, rampz;
	uint16_t pm;
	addr = AVR8_ReadReg16(NR_REG_Z);
	rampz = AVR8_ReadMem8(IOA_RAMPZ);
	addr |= (rampz << 16);
	pm = AVR8_ReadAppMem8(addr);
	AVR8_WriteReg(pm, rd);
	CycleCounter += 3;
	//fprintf(stderr,"LPM2 %02x from %04x at %04x\n",pm,addr,GET_REG_PC << 1);
}

/*
 *****************************************************************
 * avr8_lpm3
 *	Load from Program memory
 *	Variant 3: Post increment off address in REG_Z
 * v1
 *****************************************************************
 */
void
avr8_lpm3(void)
{
	uint16_t icode = ICODE;
	int rd = (icode >> 4) & 0x1f;
	uint16_t z;
	uint8_t pm;
	z = AVR8_ReadReg16(NR_REG_Z);
	pm = AVR8_ReadAppMem8(z);
	AVR8_WriteReg(pm, rd);
	z++;
	AVR8_WriteReg16(z, NR_REG_Z);
	CycleCounter += 3;
}

void
avr8_lpm3_24(void)
{
	uint16_t icode = ICODE;
	int rd = (icode >> 4) & 0x1f;
	uint32_t addr, rampz;
	uint8_t pm;
	addr = AVR8_ReadReg16(NR_REG_Z);
	rampz = AVR8_ReadMem8(IOA_RAMPZ);
	addr |= (rampz << 16);
	pm = AVR8_ReadAppMem8(addr);
	//fprintf(stderr,"LPM3 %02x from %04x at %04x\n",pm,addr,GET_REG_PC << 1);
	AVR8_WriteReg(pm, rd);
	addr = addr + 1;	/* No effect on RAMPZ ! */
	AVR8_WriteReg16(addr & 0xffff, NR_REG_Z);
	CycleCounter += 3;
}

/*
 *******************************************************************
 * avr8_lsr
 *	Logical shift right
 * v1
 *******************************************************************
 */
void
avr8_lsr(void)
{
	uint16_t icode = ICODE;
	int rd = (icode >> 4) & 0x1f;
	uint8_t Rd;
	uint8_t sreg = GET_SREG;
	sreg = sreg & ~(FLG_S | FLG_V | FLG_N | FLG_Z | FLG_C);
	Rd = AVR8_ReadReg(rd);
	if (Rd & 0x1) {
		sreg |= FLG_C;
		sreg |= FLG_S;
	} else {
		sreg |= FLG_V;
	}
	Rd >>= 1;
	AVR8_WriteReg(Rd, rd);
	if (Rd == 0) {
		sreg |= FLG_Z;
	}
	SET_SREG(sreg);
	CycleCounter += 1;
}

/*
 *******************************************************
 * avr8_mov
 * 	Copy from one register to another
 * v1
 *******************************************************
 */

void
avr8_mov(void)
{
	uint16_t icode = ICODE;
	uint8_t Rr;
	int rd, rr;
	rd = (icode >> 4) & 0x1f;
	rr = (icode & 0xf) | ((icode >> 5) & 0x10);
	Rr = AVR8_ReadReg(rr);
	AVR8_WriteReg(Rr, rd);
	CycleCounter += 1;
}

/*
 *************************************************
 * avr8_movw
 * 	Copy a register pair to another pair
 * v1
 *************************************************
 */
void
avr8_movw(void)
{
	uint16_t icode = ICODE;
	int rd, rr;
	uint8_t Rr;
	rd = (icode >> 3) & 0x1e;
	rr = (icode << 1) & 0x1e;
	Rr = AVR8_ReadReg(rr + 1);
	AVR8_WriteReg(Rr, rd + 1);
	Rr = AVR8_ReadReg(rr);
	AVR8_WriteReg(Rr, rd);
	CycleCounter += 1;
}

/*
 *************************************************************
 * avr8_mul
 * 	Multiply unsigned
 * v1	
 *************************************************************
 */

void
avr8_mul(void)
{
	uint16_t icode = ICODE;
	int rd, rr;
	uint8_t Rr, Rd;
	uint16_t result;
	uint8_t sreg = GET_SREG;
	rd = (icode >> 4) & 0x1f;
	rr = (icode & 0xf) | ((icode >> 5) & 0x10);
	Rr = AVR8_ReadReg(rr);
	Rd = AVR8_ReadReg(rd);
	result = Rd * Rr;
	AVR8_WriteReg(result >> 8, 1);
	AVR8_WriteReg(result & 0xff, 0);
	sreg = sreg & ~(FLG_Z | FLG_C);
	if (result & 0x8000) {
		sreg |= FLG_C;
	} else if (result == 0) {
		sreg |= FLG_Z;
	}
	SET_SREG(sreg);
	CycleCounter += 2;
}

/*
 *****************************************************************
 * avr8_muls
 * 	Multiply signed
 * v1
 *****************************************************************
 */
void
avr8_muls(void)
{
	uint16_t icode = ICODE;
	int rd, rr;
	int8_t Rr, Rd;
	int16_t result;
	uint8_t sreg = GET_SREG;
	rd = ((icode >> 4) & 0xf) | 0x10;
	rr = ((icode & 0xf)) | 0x10;
	Rr = AVR8_ReadReg(rr);
	Rd = AVR8_ReadReg(rd);
	result = Rd * Rr;
	AVR8_WriteReg(result >> 8, 1);
	AVR8_WriteReg(result & 0xff, 0);
	sreg = sreg & ~(FLG_Z | FLG_C);
	if (result & 0x8000) {
		sreg |= FLG_C;
	} else if (result == 0) {
		sreg |= FLG_Z;
	}
	SET_SREG(sreg);
	CycleCounter += 2;
}

/*
 *************************************************************
 * avr8_mulsu
 *	Multiply signed with unsigned
 * v1
 *************************************************************
 */

void
avr8_mulsu(void)
{
	uint16_t icode = ICODE;
	int rd, rr;
	uint8_t Rr;
	int8_t Rd;
	int16_t result;
	uint8_t sreg = GET_SREG;
	rd = ((icode >> 4) & 0x7) | 0x10;
	rr = (icode & 0x7) | 0x10;
	Rr = AVR8_ReadReg(rr);
	Rd = AVR8_ReadReg(rd);
	result = Rd * Rr;
	AVR8_WriteReg(result >> 8, 1);
	AVR8_WriteReg(result & 0xff, 0);
	sreg = sreg & ~(FLG_Z | FLG_C);
	if (result & 0x8000) {
		sreg |= FLG_C;
	} else if (result == 0) {
		sreg |= FLG_Z;
	}
	SET_SREG(sreg);
	CycleCounter += 2;
}

/*
 **************************************************************
 * avr_neg
 *	Negate (two's complement)
 **************************************************************
 */
void
avr8_neg(void)
{
	uint16_t icode = ICODE;
	int rd;
	uint8_t Rd, result;
	rd = (icode >> 4) & 0x1f;
	Rd = AVR8_ReadReg(rd);
	result = 0 - Rd;
	AVR8_WriteReg(result, rd);
	sub8_flags(0, Rd, result);
	SET_SREG((GET_SREG & ~FLG_Z) | flg_zero(result));
	CycleCounter += 1;
}

/*
 ********************************************************************
 * avr8_nop
 *	Use up time doing nothing	
 * v1
 ********************************************************************
 */
void
avr8_nop(void)
{
	/* Do nothing */
	CycleCounter += 1;
}

/*
 *********************************************************************
 * avr8_or
 *	OR
 * v1
 *********************************************************************
 */
void
avr8_or(void)
{
	uint16_t icode = ICODE;
	int rd = (icode & 0x01f0) >> 4;
	int rr = (icode & 0xf) | ((icode >> 5) & 0x10);
	uint8_t Rd = AVR8_ReadReg(rd);
	uint8_t Rr = AVR8_ReadReg(rr);
	uint8_t R;
	uint8_t sreg = GET_SREG;
	R = Rd | Rr;
	AVR8_WriteReg(R, rd);
	sreg = sreg & ~(FLG_S | FLG_V | FLG_N | FLG_Z);
	sreg |= flg_negative8(R);
	sreg |= flg_zero(R);
	sreg |= flg_sign(sreg);
	SET_SREG(sreg);
	CycleCounter += 1;
}

/*
 *********************************************************************
 * avr8_ori
 *	ORI
 * v1
 *********************************************************************
 */
void
avr8_ori(void)
{
	uint16_t icode = ICODE;
	int rd = ((icode & 0xf0) >> 4) | 0x10;
	uint8_t K = (icode & 0xf) | ((icode >> 4) & 0xf0);
	uint8_t Rd = AVR8_ReadReg(rd);
	uint8_t sreg = GET_SREG;
	uint8_t R;
	R = Rd | K;
	AVR8_WriteReg(R, rd);
	sreg = sreg & ~(FLG_S | FLG_V | FLG_N | FLG_Z);
	sreg |= flg_negative8(R);
	sreg |= flg_zero(R);
	sreg |= flg_sign(sreg);
	SET_SREG(sreg);
	CycleCounter += 1;
}

/*
 **********************************************************************
 * avr8_out
 * 	Write to a Register in iospace from address 0x20 to address 0x5f
 * v1
 **********************************************************************
 */
void
avr8_out(void)
{
	uint16_t icode = ICODE;
	int rr = (icode >> 4) & 0x1f;
	int addr = (icode & 0xf) | ((icode >> 5) & 0x30);
	uint8_t Rr = AVR8_ReadReg(rr);
	AVR8_WriteIO8(Rr, addr);
	CycleCounter += 1;
	//fprintf(stderr,"avr8_out: icode %04x at %04x: reg %02x, value %02x\n",icode,GET_REG_PC << 1,addr+0x20,Rr);
}

/*
 **************************************************
 * avr8_pop
 * 	Pop one byte from stack into a register
 *	The stack pointer is pre-incremented by 1
 * v1
 *************************************************
 */
void
avr8_pop(void)
{
	uint16_t icode = ICODE;
	int rd = (icode >> 4) & 0x1f;
	uint8_t Rd;
	uint16_t sp = GET_REG_SP;
	sp++;
	Rd = AVR8_ReadMem8(sp);
	AVR8_WriteReg(Rd, rd);
	SET_REG_SP(sp);
	CycleCounter += 2;
}

/*
 *************************************************************
 * avr8_push
 *	Push one Register to the stack
 *	The stack pointer is post decremented by one
 * v1
 *************************************************************
 */
void
avr8_push(void)
{
	uint16_t icode = ICODE;
	int rd = (icode >> 4) & 0x1f;
	uint8_t Rd;
	uint16_t sp = GET_REG_SP;
	Rd = AVR8_ReadReg(rd);
	AVR8_WriteMem8(Rd, sp);
	sp--;
	SET_REG_SP(sp);
	CycleCounter += 2;
}

/*
 **********************************************************************
 * avr8_rcall
 * 	Relative call
 * v1
 **********************************************************************
 */
void
avr8_rcall(void)
{

	uint16_t icode = ICODE;
	int16_t k;
	uint16_t sp = GET_REG_SP;
	uint16_t pc = GET_REG_PC;
	if (icode & 0x800) {
		k = 0xf800 | (icode & 0x7ff);
	} else {
		k = icode & 0x7ff;
	}
	AVR8_WriteMem8(pc & 0xff, sp--);
	AVR8_WriteMem8(pc >> 8, sp--);
	CycleCounter += 3;
	pc += k;
	SET_REG_PC(pc);
	SET_REG_SP(sp);
}

void
avr8_rcall_24(void)
{

	uint16_t icode = ICODE;
	int16_t k;
	uint16_t sp = GET_REG_SP;
	uint16_t pc = GET_REG_PC;
	if (icode & 0x800) {
		k = 0xf800 | (icode & 0x7ff);
	} else {
		k = icode & 0x7ff;
	}
	AVR8_WriteMem8(pc & 0xff, sp--);
	AVR8_WriteMem8((pc >> 8) & 0xff, sp--);
	AVR8_WriteMem8((pc >> 16) & 0xff, sp--);
	CycleCounter += 4;
	pc += k;
	SET_REG_PC(pc);
	SET_REG_SP(sp);
}

/*
 *********************************************************************
 * avr8_ret
 *	Return to address on stack
 *********************************************************************
 */

void
avr8_ret(void)
{
	uint16_t sp = GET_REG_SP;
	uint32_t pc = 0;
	pc |= AVR8_ReadMem8(++sp) << 8;
	pc |= AVR8_ReadMem8(++sp);
	SET_REG_SP(sp);
	SET_REG_PC(pc);
	CycleCounter += 4;
}

void
avr8_ret_24(void)
{
	uint16_t sp = GET_REG_SP;
	uint32_t pc = 0;
	pc = AVR8_ReadMem8(++sp) << 16;
	pc |= AVR8_ReadMem8(++sp) << 8;
	pc |= AVR8_ReadMem8(++sp);
	SET_REG_SP(sp);
	SET_REG_PC(pc);
	CycleCounter += 5;
}

/*
 **********************************************************************
 * avr8_reti
 *	Return from interrupt
 **********************************************************************
 */
void
avr8_reti(void)
{
	uint16_t sp = GET_REG_SP;
	uint16_t pc;
	pc = AVR8_ReadMem8(++sp) << 8;
	pc |= AVR8_ReadMem8(++sp);
	SET_REG_SP(sp);
	SET_REG_PC(pc);
	/* 
	 **************************************************************************
	 * xmega does not enable Interrupt on reti 
	 * but atmega does, so interrupt flag is set by the following
	 * reti proc on atmega. For xmega the reti proc is more complicated
	 **************************************************************************
	 */
	if (gavr8.avrReti) {
		gavr8.avrReti(gavr8.avrIrqData);
	}
	AVR8_UpdateCpuSignals();
	CycleCounter += 4;
}

void
avr8_reti_24(void)
{
	uint16_t sp = GET_REG_SP;
	uint32_t pc = 0;
	uint8_t sreg = GET_SREG;
	pc = AVR8_ReadMem8(++sp) << 16;
	pc |= AVR8_ReadMem8(++sp) << 8;
	pc |= AVR8_ReadMem8(++sp);
	sreg |= FLG_I;
	SET_REG_SP(sp);
	SET_REG_PC(pc);
	SET_SREG(sreg);
	AVR8_UpdateCpuSignals();
	CycleCounter += 5;
}

/*
 **********************************************************************
 * avr8_rjmp
 *	Jump relative
 * v1
 **********************************************************************
 */
void
avr8_rjmp(void)
{
	uint16_t icode = ICODE;
	int16_t k = ((int16_t) (icode << 4)) >> 4;
	uint16_t pc = GET_REG_PC;
	pc += k;
	SET_REG_PC(pc);
	CycleCounter += 2;
}

/*
 ****************************************************************
 * avr8_ror
 * 	Rotate right
 * v1
 ****************************************************************
 */
void
avr8_ror(void)
{
	uint16_t icode = ICODE;
	int rd = (icode >> 4) & 0x1f;
	uint8_t Rd;
	uint8_t R;
	uint8_t sreg = GET_SREG;
	Rd = AVR8_ReadReg(rd);
	R = Rd >> 1;
	if (sreg & FLG_C) {
		R |= 0x80;
	}
	sreg = sreg & ~(FLG_S | FLG_V | FLG_N | FLG_Z | FLG_C);
	if (Rd & 1) {
		sreg |= FLG_C;
	}
	if (R == 0) {
		sreg |= FLG_Z;
	} else if (R & 0x80) {
		sreg |= FLG_N;
	}
	if (!!(sreg & FLG_N) ^ !!(sreg & FLG_C)) {
		sreg |= FLG_V;
	}
	sreg |= flg_sign(sreg);
	AVR8_WriteReg(R, rd);
	SET_SREG(sreg);
	CycleCounter += 1;
}

/*
 *************************************************************
 * avr8_sbc
 *	Subtract with carry
 *	Compared with real cpu: 10 - 3 = 7 if carry clear
 *			        10 - 3 - Carry = 6 
 * v1
 *************************************************************
 */
void
avr8_sbc(void)
{
	uint16_t icode = ICODE;
	int rd = (icode >> 4) & 0x1f;
	int rr = (icode & 0xf) | ((icode >> 5) & 0x10);
	uint8_t Rd = AVR8_ReadReg(rd);
	uint8_t Rr = AVR8_ReadReg(rr);
	uint8_t R;
	uint8_t sreg = GET_SREG;
	uint8_t C = !!(sreg & FLG_C);
	R = Rd - Rr - C;
	AVR8_WriteReg(R, rd);
	sub8_flags(Rd, Rr, R);
	if (R != 0) {
		SET_SREG(GET_SREG & ~FLG_Z);
	}
	CycleCounter += 1;
}

/*
 ****************************************************************
 * avr8_sbci
 * 	Subtract immediate with carry
 * v1
 ****************************************************************
 */
void
avr8_sbci(void)
{
	uint16_t icode = ICODE;
	int rd = (((icode >> 4)) & 0xf) | 0x10;
	uint16_t K = (icode & 0xf) | ((icode >> 4) & 0xf0);
	uint8_t Rd = AVR8_ReadReg(rd);
	uint8_t sreg = GET_SREG;
	uint8_t C = !!(sreg & FLG_C);
	uint16_t R = Rd - K - C;
	AVR8_WriteReg(R, rd);
	sub8_flags(Rd, K, R);
	if (R != 0) {
		SET_SREG(GET_SREG & ~FLG_Z);
	}
	CycleCounter += 1;
}

/*
 **************************************
 * avr8_sbi
 *	Set bit in IO regsiter
 * v1;
 **************************************
 */
void
avr8_sbi(void)
{
	uint16_t icode = ICODE;
	int bit = icode & 7;
	int a = (icode >> 3) & 0x1f;
	uint8_t val;
	val = AVR8_ReadIO8(a);
	val |= (1 << bit);
	AVR8_WriteIO8(val, a);
	CycleCounter += 2;
}

/*
 ****************************************************
 * avr_sbic
 *	Skip if bit in IO register is clear 
 ****************************************************
 */
void
avr8_sbic(void)
{
	uint16_t icode = ICODE;
	int bit = icode & 7;
	int a = (icode >> 3) & 0x1f;
	uint8_t val;
	val = AVR8_ReadIO8(a);
	if (!(val & (1 << bit))) {
		AVR8_SkipInstruction();
	}
	CycleCounter += 1;
}

/*
 ****************************************************
 * avr8_sbis
 *	Skip if bit in IO register is set
 ****************************************************
 */
void
avr8_sbis(void)
{
	uint16_t icode = ICODE;
	int bit = icode & 7;
	int a = (icode >> 3) & 0x1f;
	uint8_t val;
	val = AVR8_ReadIO8(a);
	if (val & (1 << bit)) {
		AVR8_SkipInstruction();
	}
	CycleCounter += 1;
}

/*
 *******************************************************
 * avr8_sbiw
 *	Subtract immediate from word
 * v1
 *******************************************************
 */

void
avr8_sbiw(void)
{
	uint16_t icode = ICODE;
	int rd = ((icode & 0x30) >> 3) + 24;
	uint16_t K = (icode & 0xf) | ((icode & 0x00c0) >> 2);
	uint16_t Rd = AVR8_ReadReg16(rd);
	uint16_t R = Rd - K;
	AVR8_WriteReg16(R, rd);
	sub16_flags(Rd, K, R);
	if (R == 0) {
		SET_SREG(GET_SREG | FLG_Z);
	} else {
		SET_SREG(GET_SREG & ~FLG_Z);
	}
	CycleCounter += 2;
}

/*
 *********************************************************
 * avr8_sbrc
 *	Skip if bit in register is clear
 * v1
 *********************************************************
 */
void
avr8_sbrc(void)
{
	uint16_t icode = ICODE;
	int bit = icode & 7;
	int rr = (icode >> 4) & 0x1f;
	uint8_t Rr = AVR8_ReadReg(rr);
	if (!(Rr & (1 << bit))) {
		AVR8_SkipInstruction();
	}
	CycleCounter += 1;
}

/*
 ******************************************************
 * avr8_sbrs
 *	Skip if bit in register is set
 * v1
 ******************************************************
 */

void
avr8_sbrs(void)
{
	uint16_t icode = ICODE;
	int bit = icode & 7;
	int rr = (icode >> 4) & 0x1f;
	uint8_t Rr = AVR8_ReadReg(rr);
	if (Rr & (1 << bit)) {
		AVR8_SkipInstruction();
	}
	CycleCounter += 1;
}

void
avr8_sleep(void)
{
	CycleCounter += 1;
	fprintf(stderr, "avr8_sleep not implemented\n");
}

void
avr8_spm(void)
{
	CycleCounter += 1;
	fprintf(stderr, "avr8_spm not implemented\n");
}

/*
 ************************************************************
 * avr8_st1
 * 	Store into data space
 * 	Variant 1: X unchanged
 * v1
 ************************************************************
 */
void
avr8_st1(void)
{
	uint16_t icode = ICODE;
	uint16_t x;
	int rr = (icode >> 4) & 0x1f;
	uint8_t Rr;
	Rr = AVR8_ReadReg(rr);
	x = AVR8_ReadReg16(NR_REG_X);
	AVR8_WriteMem8(Rr, x);
	CycleCounter += 2;
}

/*
 *******************************************************************
 * avr8_st2
 *	Store in data space
 *	Variant 2: X post incremented 
 * v1
 *******************************************************************
 */

void
avr8_st2(void)
{
	uint16_t icode = ICODE;
	uint16_t x;
	int rr = (icode >> 4) & 0x1f;
	uint8_t Rr;
	Rr = AVR8_ReadReg(rr);
	x = AVR8_ReadReg16(NR_REG_X);
	AVR8_WriteMem8(Rr, x);
	x++;
	AVR8_WriteReg16(x, NR_REG_X);
	CycleCounter += 2;
}

/*
 **************************************************************************
 * avr8_st3
 *	Store to dataspace 
 *	Variant 3, pre decrement address
 **************************************************************************
 */
void
avr8_st3(void)
{
	uint16_t icode = ICODE;
	uint16_t x;
	int rr = (icode >> 4) & 0x1f;
	uint8_t Rr;
	Rr = AVR8_ReadReg(rr);
	x = AVR8_ReadReg16(NR_REG_X);
	x--;
	AVR8_WriteReg16(x, NR_REG_X);
	AVR8_WriteMem8(Rr, x);
	CycleCounter += 2;
}

/*
 ************************************************************
 * avr8_sty2
 *	Store in data space using index Y
 *	Variant 2: Y post incremented
 * v1
 ************************************************************
 */
void
avr8_sty2(void)
{
	uint16_t icode = ICODE;
	uint16_t y;
	int rr = (icode >> 4) & 0x1f;
	uint8_t Rr;
	Rr = AVR8_ReadReg(rr);
	y = AVR8_ReadReg16(NR_REG_Y);
	AVR8_WriteMem8(Rr, y);
	y++;
	AVR8_WriteReg16(y, NR_REG_Y);
	CycleCounter += 2;
}

/*
 ************************************************************
 * avr8_sty3
 *	Store in data space using index Y
 *	Variant 3: Y pre decremented
 * v1
 ************************************************************
 */
void
avr8_sty3(void)
{
	uint16_t icode = ICODE;
	uint16_t y;
	int rr = (icode >> 4) & 0x1f;
	uint8_t Rr;
	Rr = AVR8_ReadReg(rr);
	y = AVR8_ReadReg16(NR_REG_Y);
	y--;
	AVR8_WriteReg16(y, NR_REG_Y);
	AVR8_WriteMem8(Rr, y);
	CycleCounter += 2;
}

/*
 ****************************************************************
 * avr8_sty4
 *	Store in data space using index Y	
 *	Variant 4: Y unchanged, use displacement
 ****************************************************************
 */

void
avr8_sty4(void)
{
	uint16_t icode = ICODE;
	uint16_t y;
	uint8_t q = (icode & 7) | ((icode >> 7) & 0x18) | ((icode >> 8) & 0x20);
	uint16_t addr;
	int rr = (icode >> 4) & 0x1f;
	uint8_t Rr;
	Rr = AVR8_ReadReg(rr);
	y = AVR8_ReadReg16(NR_REG_Y);
	addr = (y + q);
	AVR8_WriteMem8(Rr, addr);
	CycleCounter += 2;
}

/*
 ****************************************************************
 * avr8_stz2
 * 	Store in data space using index Z
 *	Variant 2: Post increment Z
 * v1
 ****************************************************************
 */

void
avr8_stz2(void)
{
	uint16_t icode = ICODE;
	uint16_t z;
	int rr = (icode >> 4) & 0x1f;
	uint8_t Rr;
	Rr = AVR8_ReadReg(rr);
	z = AVR8_ReadReg16(NR_REG_Z);
	AVR8_WriteMem8(Rr, z);
	z++;
	AVR8_WriteReg16(z, NR_REG_Z);
	CycleCounter += 2;
}

/*
 ****************************************************************
 * avr8_stz3
 * 	Store in dataspace using Index register Z
 *	Variant 3: Pre decrement Z
 * v1
 ****************************************************************
 */
void
avr8_stz3(void)
{
	uint16_t icode = ICODE;
	uint16_t z;
	int rr = (icode >> 4) & 0x1f;
	uint8_t Rr;
	Rr = AVR8_ReadReg(rr);
	z = AVR8_ReadReg16(NR_REG_Z);
	z--;
	AVR8_WriteReg16(z, NR_REG_Z);
	AVR8_WriteMem8(Rr, z);
	CycleCounter += 2;
}

/*
 **********************************************************************
 * avr8_stz4
 * 	Store in dataspace using Index register Z
 *	Variant4: Z is unchanged, use displacement  
 * v1
 **********************************************************************
 */
void
avr8_stz4(void)
{
	uint16_t icode = ICODE;
	uint16_t z;
	uint8_t q = (icode & 7) | ((icode >> 7) & 0x18) | ((icode >> 8) & 0x20);
	uint32_t addr;
	int rr = (icode >> 4) & 0x1f;
	uint8_t Rr;
	Rr = AVR8_ReadReg(rr);
	z = AVR8_ReadReg16(NR_REG_Z);
	addr = (z + q) & 0xffff;
	/* 
	   If 3 byte address
	   addr |= rampz << 16;
	 */
	AVR8_WriteMem8(Rr, addr);
	CycleCounter += 2;
}

/*
 *****************************************************************
 * avr8_sts
 * 	Store direct to data space
 *****************************************************************
 */
void
avr8_sts(void)
{
	uint16_t icode1 = ICODE;
	int rr = (icode1 >> 4) & 0x1f;
	uint16_t k = AVR8_ReadAppMem((GET_REG_PC));
	uint8_t Rr;
	SET_REG_PC(GET_REG_PC + 1);
	Rr = AVR8_ReadReg(rr);
	AVR8_WriteMem8(Rr, k);
	CycleCounter += 2;
}

/*
 *******************************************************************
 * avr8_sub
 *	Subtract
 *******************************************************************
 */
void
avr8_sub(void)
{
	uint16_t icode = ICODE;
	int rd = (icode >> 4) & 0x1f;
	int rr = (icode & 0xf) | ((icode >> 5) & 0x10);
	uint8_t Rd = AVR8_ReadReg(rd);
	uint8_t Rr = AVR8_ReadReg(rr);
	uint8_t R;
	R = Rd - Rr;
	AVR8_WriteReg(R, rd);
	sub8_flags(Rd, Rr, R);
	SET_SREG((GET_SREG & ~FLG_Z) | flg_zero(R));
	CycleCounter += 1;
}

/*
 *******************************************************************
 * avr8_subi
 *	Subtract an immediate
 *******************************************************************
 */

void
avr8_subi(void)
{
	uint16_t icode = ICODE;
	int rd = ((icode >> 4) & 0xf) | 0x10;
	uint8_t Rd = AVR8_ReadReg(rd);
	uint8_t k = (icode & 0xf) | ((icode >> 4) & 0xf0);
	uint8_t R;
	R = Rd - k;
	AVR8_WriteReg(R, rd);
	sub8_flags(Rd, k, R);
	SET_SREG((GET_SREG & ~FLG_Z) | flg_zero(R));
	CycleCounter += 1;
}

void
avr8_swap(void)
{
	uint16_t icode = ICODE;
	int rd = (icode >> 4) & 0x1f;
	uint8_t Rd = AVR8_ReadReg(rd);
	Rd = ((Rd & 0xf) << 4) | ((Rd & 0xf0) >> 4);
	AVR8_WriteReg(Rd, rd);
	CycleCounter += 1;
}

void
avr8_wdr(void)
{
	SigNode_Set(gavr8.wdResetNode, SIG_LOW);
	SigNode_Set(gavr8.wdResetNode, SIG_HIGH);
	CycleCounter += 1;
}

void
avr8_undef(void)
{
	AVR8_DumpPcBuf();
	fprintf(stderr, "undefined AVR8 instruction %04x at %04x\n", ICODE, GET_REG_PC << 1);
	exit(1);
}

void
AVR8_InitInstructions(AVR8_Cpu * avr)
{
	uint8_t sreg;
	unsigned int i;
	uint8_t op1, op2, result;
	avr->add_flags = sg_calloc(32768);
	avr->sub_flags = sg_calloc(32768);
	for (i = 0; i < 32768; i++) {
		op1 = (i & 0x1f) << 3;
		op2 = ((i >> 5) & 0x1f) << 3;
		result = ((i >> 10) & 0x1f) << 3;
		sreg = 0;
		sreg |= add8_overflow(op1, op2, result);
		sreg |= halffull_add_carry(op1, op2, result);
		sreg |= flg_negative8(result);
		sreg |= flg_sign(sreg);
		avr->add_flags[i] = sreg;
	}
	for (i = 0; i < 32768; i++) {
		op1 = (i & 0x1f) << 3;
		op2 = ((i >> 5) & 0x1f) << 3;
		result = ((i >> 10) & 0x1f) << 3;
		sreg = 0;
		sreg |= sub8_overflow(op1, op2, result);
		sreg |= halffull_sub_carry(op1, op2, result);
		sreg |= flg_negative8(result);
		sreg |= flg_sign(sreg);
		avr->sub_flags[i] = sreg;
	}
}
