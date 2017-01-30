/*
 * ----------------------------------------------------
 *
 * Instruction set of the Infineon C16x 
 *
 * State: Untested but implemented
 *
 * Copyright 2005 Jochen Karrer. All rights reserved.
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
#include "c16x_cpu.h"
#include "sgstring.h"

#define ISNEG(x) ((x)&(1<<15))
#define ISNOTNEG(x) (!((x)&(1<<15)))
#define ISPOS(x) (-(x) & (1<<15))

#define ISNEGB(x) ((x)&(1<<7))
#define ISNOTNEGB(x) (!((x)&(1<<7)))
#define ISPOSB(x) (-(x) & (1<<7))

/*
 * -------------------------
 * Condition Endodings
 * p. 38 of ISM
 * -------------------------
 */
#define CC_UC	(0)
#define CC_Z	(2)
#define CC_NZ	(3)
#define CC_V	(4)
#define CC_NV	(5)
#define CC_N	(6)
#define CC_NN	(7)
#define CC_C	(8)
#define CC_NC	(9)
#define CC_EQ   (2)
#define CC_NE	(3)
#define CC_ULT  (8)
#define CC_ULE  (0xf)
#define CC_UGE  (9)
#define CC_UGT  (0xe)
#define CC_SLT  (0xc)
#define CC_SLE  (0xb)
#define CC_SGE  (0xd)
#define CC_SGT  (0xa)
#define CC_NET	(1)

#define CM_TYPE char
CM_TYPE *condition_map;
#define CONDITION_INDEX(icode,psw) (((icode) & 0xf0) | ((psw) & 0xf))
#define CONDITION_UNINDEX_PSW(index) ((index)&0xf)
#define CONDITION_UNINDEX_CC(index) (((index)&0xf0)>>4)

static void
init_condition_map()
{
	uint16_t psw;
	uint16_t cc;
	int i;
	CM_TYPE *m = condition_map = (CM_TYPE *) sg_calloc(256 * sizeof(*m));

	for (i = 0; i < 256; i++) {
		cc = CONDITION_UNINDEX_CC(i);
		psw = CONDITION_UNINDEX_PSW(i);
		switch (cc) {
		    case CC_UC:
			    condition_map[i] = 1;
			    break;

		    case CC_NET:
			    if (!((psw & PSW_FLAG_Z) || (psw & PSW_FLAG_E))) {
				    condition_map[i] = 1;
			    }
			    break;

		    case CC_Z:
			    if (psw & PSW_FLAG_Z) {
				    condition_map[i] = 1;
			    }
			    break;
		    case CC_NZ:
			    if (!(psw & PSW_FLAG_Z)) {
				    condition_map[i] = 1;
			    }
			    break;

		    case CC_V:
			    if (psw & PSW_FLAG_V) {
				    condition_map[i] = 1;
			    }
			    break;

		    case CC_NV:
			    if (!(psw & PSW_FLAG_V)) {
				    condition_map[i] = 1;
			    }
			    break;

		    case CC_N:
			    if (psw & PSW_FLAG_N) {
				    condition_map[i] = 1;
			    }
			    break;

		    case CC_NN:
			    if (!(psw & PSW_FLAG_N)) {
				    condition_map[i] = 1;
			    }
			    break;

		    case CC_C:
			    if (psw & PSW_FLAG_C) {
				    condition_map[i] = 1;
			    }
			    break;

		    case CC_NC:
			    if (!(psw & PSW_FLAG_C)) {
				    condition_map[i] = 1;
			    }

		    case CC_SGT:	/* ok */
			    if (!(psw & PSW_FLAG_Z) && (((psw & PSW_FLAG_N) && (psw & PSW_FLAG_V))
							|| (!(psw & PSW_FLAG_N)
							    && !(psw & PSW_FLAG_V)))) {
				    condition_map[i] = 1;
			    }
			    break;

		    case CC_SLE:
			    if ((psw & PSW_FLAG_Z) ||
				((psw & PSW_FLAG_N) && !(psw & PSW_FLAG_V)) ||
				(!(psw & PSW_FLAG_N) && (psw & PSW_FLAG_V))) {
				    condition_map[i] = 1;
			    }
			    break;

		    case CC_SLT:
			    if (((psw & PSW_FLAG_N) && !(psw & PSW_FLAG_V))
				|| (!(psw & PSW_FLAG_N) && (psw & PSW_FLAG_V))) {
				    condition_map[i] = 1;
			    }

			    break;

		    case CC_SGE:
			    if (((psw & PSW_FLAG_N) && (psw & PSW_FLAG_V))
				|| (!(psw & PSW_FLAG_N) && !(psw & PSW_FLAG_V))) {
				    condition_map[i] = 1;
			    }
			    break;

		    case CC_UGT:
			    if (!(psw & PSW_FLAG_Z) && !(psw & PSW_FLAG_C)) {
				    condition_map[i] = 1;
			    }
			    break;

		    case CC_ULE:
			    if ((psw & PSW_FLAG_Z) || (psw & PSW_FLAG_C)) {
				    condition_map[i] = 1;
			    }
			    break;
			    fprintf(stderr, "unknown Condition code 0x%02x\n", cc);
			    exit(347);
		}

	}
}

/*
 * ---------------------------------
 * returns 1 if condition is true
 * ---------------------------------
 */

static inline int
check_condition(uint8_t icode)
{
	int index = CONDITION_INDEX(icode, REG_PSW);
	fprintf(stderr, "index %02x value %d\n", index, condition_map[index]);
	return condition_map[index];
}

static inline uint16_t
add_carry(uint16_t op1, uint16_t op2, uint16_t result)
{
	if (((ISNEG(op1) && ISNEG(op2))
	     || (ISNEG(op1) && ISNOTNEG(result))
	     || (ISNEG(op2) && ISNOTNEG(result)))) {
		return PSW_FLAG_C;
	} else {
		return 0;
	}
}

static inline uint16_t
add_overflow(uint16_t op1, uint16_t op2, uint16_t result)
{
	if ((ISNEG(op1) && ISNEG(op2) && ISNOTNEG(result))
	    || (ISNOTNEG(op1) && ISNOTNEG(op2) && ISNEG(result))) {
		return PSW_FLAG_V;
	} else {
		return 0;
	}
}

static inline uint16_t
add_carry_b(uint8_t op1, uint8_t op2, uint8_t result)
{
	if (((ISNEGB(op1) && ISNEGB(op2))
	     || (ISNEGB(op1) && ISNOTNEGB(result))
	     || (ISNEGB(op2) && ISNOTNEGB(result)))) {
		return PSW_FLAG_C;
	} else {
		return 0;
	}
}

static inline uint16_t
add_overflow_b(uint8_t op1, uint8_t op2, uint8_t result)
{
	if ((ISNEGB(op1) && ISNEGB(op2) && ISNOTNEGB(result))
	    || (ISNOTNEGB(op1) && ISNOTNEGB(op2) && ISNEGB(result))) {
		return PSW_FLAG_V;
	} else {
		return 0;
	}
}

/* 
 * -------------------------------------------------------------
 * Borrow style carry definition, 
 * inverted to 6502 and ARM style which do complement addition
 * -------------------------------------------------------------
 */
static inline uint16_t
sub_carry(uint16_t op1, uint16_t op2, uint16_t result)
{
	if (((ISNEG(op1) && ISNOTNEG(op2))
	     || (ISNEG(op1) && ISNOTNEG(result))
	     || (ISNOTNEG(op2) && ISNOTNEG(result)))) {
		return 0;
	} else {
		return PSW_FLAG_C;
	}
}

static inline uint16_t
sub_overflow(uint16_t op1, uint16_t op2, uint16_t result)
{
	if ((ISNEG(op1) && ISNOTNEG(op2) && ISNOTNEG(result))
	    || (ISNOTNEG(op1) && ISNEG(op2) && ISNEG(result))) {
		return PSW_FLAG_V;
	} else {
		return 0;
	}
}

static inline uint16_t
sub_carry_b(uint16_t op1, uint16_t op2, uint16_t result)
{
	if (((ISNEGB(op1) && ISNOTNEGB(op2))
	     || (ISNEGB(op1) && ISNOTNEGB(result))
	     || (ISNOTNEGB(op2) && ISNOTNEGB(result)))) {
		return 0;
	} else {
		return PSW_FLAG_C;
	}
}

static inline uint16_t
sub_overflow_b(uint8_t op1, uint8_t op2, uint8_t result)
{
	if ((ISNEGB(op1) && ISNOTNEGB(op2) && ISNOTNEGB(result))
	    || (ISNOTNEGB(op1) && ISNEGB(op2) && ISNEGB(result))) {
		return PSW_FLAG_V;
	} else {
		return 0;
	}
}

/*
 * ----------------------------------------------------------------------
 * Add  Rw1 = Rw1+Rw2 
 * v.1	
 * ----------------------------------------------------------------------
 */
void
c16x_add_rw_rw(uint8_t * icodeP)
{
	int n = (icodeP[1] & 0xf0) >> 4;
	int m = (icodeP[1] & 0xf);
	uint16_t op1, op2;
	uint16_t result;
	op1 = C16x_ReadGpr16(n);
	op2 = C16x_ReadGpr16(m);
	result = op1 + op2;
	C16x_SetGpr16(result, n);

	REG_PSW &= ~(PSW_FLAG_E | PSW_FLAG_Z | PSW_FLAG_V | PSW_FLAG_C | PSW_FLAG_N);
	if (result == 0x8000) {
		REG_PSW |= PSW_FLAG_E;
	} else if (result == 0) {
		REG_PSW |= PSW_FLAG_Z;
	}
	if (result & 0x8000) {
		REG_PSW |= PSW_FLAG_N;
	}
	if (add_carry(op1, op2, result)) {
		REG_PSW |= PSW_FLAG_C;
	}
	if (add_overflow(op1, op2, result)) {
		REG_PSW |= PSW_FLAG_V;
	}
}

/*
 * ------------------------------
 * Addb Rb Rb
 * v1
 * ------------------------------
 */
void
c16x_addb_rb_rb(uint8_t * icodeP)
{
	int n = (icodeP[1] & 0xf0) >> 4;
	int m = (icodeP[1] & 0xf);
	uint8_t op1, op2;
	uint8_t result;
	op1 = C16x_ReadGpr8(n);
	op2 = C16x_ReadGpr8(m);
	result = op1 + op2;
	C16x_SetGpr8(result, n);

	REG_PSW &= ~(PSW_FLAG_E | PSW_FLAG_Z | PSW_FLAG_V | PSW_FLAG_C | PSW_FLAG_N);
	if (result == 0x80) {
		REG_PSW |= PSW_FLAG_E;
	} else if (result == 0) {
		REG_PSW |= PSW_FLAG_Z;
	}
	if (result & 0x80) {
		REG_PSW |= PSW_FLAG_N;
	}
	if (add_carry_b(op1, op2, result)) {
		REG_PSW |= PSW_FLAG_C;
	}
	if (add_overflow_b(op1, op2, result)) {
		REG_PSW |= PSW_FLAG_V;
	}
}

/*
 * --------------------------------------------------------------
 * Add reg mem
 * v1
 * --------------------------------------------------------------
 */
void
c16x_add_reg_mem(uint8_t * icodeP)
{
	uint8_t reg = icodeP[1];
	uint16_t maddr = ((uint16_t) icodeP[3] << 8) | icodeP[2];
	uint16_t op1, op2;
	uint16_t result;
	op1 = C16x_ReadReg16(reg);
	op2 = C16x_MemRead16(maddr);
	result = op1 + op2;
	C16x_SetReg16(result, reg);

	REG_PSW &= ~(PSW_FLAG_E | PSW_FLAG_Z | PSW_FLAG_V | PSW_FLAG_C | PSW_FLAG_N);
	if (result == 0x8000) {
		REG_PSW |= PSW_FLAG_E;
	} else if (result == 0) {
		REG_PSW |= PSW_FLAG_Z;
	}
	if (result & 0x8000) {
		REG_PSW |= PSW_FLAG_N;
	}
	if (add_carry(op1, op2, result)) {
		REG_PSW |= PSW_FLAG_C;
	}
	if (add_overflow(op1, op2, result)) {
		REG_PSW |= PSW_FLAG_V;
	}
}

/*
 * ---------------------------------------------------
 * ADDB reg mem
 * v1
 * ---------------------------------------------------
 */
void
c16x_addb_reg_mem(uint8_t * icodeP)
{
	uint8_t reg = icodeP[1];
	uint16_t maddr = (icodeP[3] << 8) | icodeP[2];
	uint16_t op1, op2;
	uint8_t result;
	op1 = C16x_ReadReg8(reg);
	op2 = C16x_MemRead8(maddr);
	result = op1 + op2;
	C16x_SetReg8(result, reg);

	REG_PSW &= ~(PSW_FLAG_E | PSW_FLAG_Z | PSW_FLAG_V | PSW_FLAG_C | PSW_FLAG_N);
	if (result == 0x80) {
		REG_PSW |= PSW_FLAG_E;
	} else if (result == 0) {
		REG_PSW |= PSW_FLAG_Z;
	}
	if (result & 0x80) {
		REG_PSW |= PSW_FLAG_N;
	}
	if (add_carry_b(op1, op2, result)) {
		REG_PSW |= PSW_FLAG_C;
	}
	if (add_overflow_b(op1, op2, result)) {
		REG_PSW |= PSW_FLAG_V;
	}
}

/*
 * ------------------------------------------------------
 * Add mem reg
 * v1		
 * ------------------------------------------------------
 */

void
c16x_add_mem_reg(uint8_t * icodeP)
{
	uint8_t reg = icodeP[1];
	uint16_t maddr = (icodeP[3] << 8) | icodeP[2];
	uint16_t op1, op2;
	uint16_t result;
	op1 = C16x_MemRead16(maddr);
	op2 = C16x_ReadReg16(reg);
	result = op1 + op2;
	C16x_MemWrite16(result, maddr);

	REG_PSW &= ~(PSW_FLAG_E | PSW_FLAG_Z | PSW_FLAG_V | PSW_FLAG_C | PSW_FLAG_N);
	if (result == 0x8000) {
		REG_PSW |= PSW_FLAG_E;
	} else if (result == 0) {
		REG_PSW |= PSW_FLAG_Z;
	}
	if (result & 0x8000) {
		REG_PSW |= PSW_FLAG_N;
	}
	if (add_carry(op1, op2, result)) {
		REG_PSW |= PSW_FLAG_C;
	}
	if (add_overflow(op1, op2, result)) {
		REG_PSW |= PSW_FLAG_V;
	}

}

/*
 * ------------------------------------------------
 * ADDB mem reg
 * v1
 * ------------------------------------------------
 */
void
c16x_addb_mem_reg(uint8_t * icodeP)
{
	uint8_t reg = icodeP[1];
	uint16_t maddr = (icodeP[3] << 8) | icodeP[2];
	uint8_t op1, op2;
	uint8_t result;
	op1 = C16x_MemRead8(maddr);
	op2 = C16x_ReadReg8(reg);
	result = op1 + op2;
	C16x_MemWrite8(result, maddr);

	REG_PSW &= ~(PSW_FLAG_E | PSW_FLAG_Z | PSW_FLAG_V | PSW_FLAG_C | PSW_FLAG_N);
	if (result == 0x80) {
		REG_PSW |= PSW_FLAG_E;
	} else if (result == 0) {
		REG_PSW |= PSW_FLAG_Z;
	}
	if (result & 0x80) {
		REG_PSW |= PSW_FLAG_N;
	}
	if (add_carry_b(op1, op2, result)) {
		REG_PSW |= PSW_FLAG_C;
	}
	if (add_overflow_b(op1, op2, result)) {
		REG_PSW |= PSW_FLAG_V;
	}
}

/*
 * -------------------------------------------------------------
 * ADD reg data16
 * v1
 * -------------------------------------------------------------
 */
void
c16x_add_reg_data16(uint8_t * icodeP)
{
	uint8_t reg = icodeP[1];
	uint16_t data16 = (icodeP[3] << 8) | icodeP[2];
	uint16_t op1, op2;
	uint16_t result;
	op1 = C16x_ReadReg16(reg);
	op2 = data16;
	result = op1 + op2;
	C16x_SetReg16(result, reg);

	REG_PSW &= ~(PSW_FLAG_E | PSW_FLAG_Z | PSW_FLAG_V | PSW_FLAG_C | PSW_FLAG_N);
	if (result == 0x8000) {
		REG_PSW |= PSW_FLAG_E;
	} else if (result == 0) {
		REG_PSW |= PSW_FLAG_Z;
	}
	if (result & 0x8000) {
		REG_PSW |= PSW_FLAG_N;
	}
	if (add_carry(op1, op2, result)) {
		REG_PSW |= PSW_FLAG_C;
	}
	if (add_overflow(op1, op2, result)) {
		REG_PSW |= PSW_FLAG_V;
	}
}

/*
 * -----------------------------------------------------------
 * ADD reg data8
 * v1
 * -----------------------------------------------------------
 */
void
c16x_addb_reg_data8(uint8_t * icodeP)
{
	uint8_t reg = icodeP[1] & 0xff;
	uint8_t data8 = icodeP[2];
	uint8_t op1, op2;
	uint8_t result;
	op1 = C16x_ReadReg8(reg);
	op2 = data8;
	result = op1 + op2;
	C16x_SetReg8(result, reg);

	REG_PSW &= ~(PSW_FLAG_E | PSW_FLAG_Z | PSW_FLAG_V | PSW_FLAG_C | PSW_FLAG_N);
	if (result == 0x80) {
		REG_PSW |= PSW_FLAG_E;
	} else if (result == 0) {
		REG_PSW |= PSW_FLAG_Z;
	}
	if (result & 0x80) {
		REG_PSW |= PSW_FLAG_N;
	}
	if (add_carry_b(op1, op2, result)) {
		REG_PSW |= PSW_FLAG_C;
	}
	if (add_overflow_b(op1, op2, result)) {
		REG_PSW |= PSW_FLAG_V;
	}
}

/*
 * -------------------------------------------------
 * ADD rw x
 * v1
 * -------------------------------------------------
 */
void
c16x_add_rw_x(uint8_t * icodeP)
{
	int n = (icodeP[1] & 0xf0) >> 4;
	uint16_t addr;
	int ri;
	uint16_t op1, op2;
	uint16_t result;
	int subcmd = (icodeP[1] & 0x0c) >> 2;
	switch (subcmd) {
	    case 0:
	    case 1:
		    op1 = C16x_ReadGpr16(n);
		    op2 = icodeP[1] & 0x7;
		    result = op1 + op2;
		    C16x_SetGpr16(result, n);
		    break;

	    case 2:
		    ri = icodeP[1] & 0x3;
		    addr = C16x_ReadGpr16(ri);
		    op1 = C16x_ReadGpr16(n);
		    op2 = C16x_MemRead16(addr);
		    result = op1 + op2;
		    C16x_SetGpr16(result, n);
		    break;

	    case 3:
		    ri = icodeP[1] & 0x3;
		    addr = C16x_ReadGpr16(ri);
		    op1 = C16x_ReadGpr16(n);
		    op2 = C16x_MemRead16(addr);
		    result = op1 + op2;
		    C16x_SetGpr16(result, n);
		    C16x_SetGpr16(addr + 2, ri);	/* Post Increment by 1 Word */
		    break;
	    default:
		    return;
	}
	REG_PSW &= ~(PSW_FLAG_E | PSW_FLAG_Z | PSW_FLAG_V | PSW_FLAG_C | PSW_FLAG_N);
	if (result == 0x8000) {
		REG_PSW |= PSW_FLAG_E;
	} else if (result == 0) {
		REG_PSW |= PSW_FLAG_Z;
	}
	if (result & 0x8000) {
		REG_PSW |= PSW_FLAG_N;
	}
	if (add_carry(op1, op2, result)) {
		REG_PSW |= PSW_FLAG_C;
	}
	if (add_overflow(op1, op2, result)) {
		REG_PSW |= PSW_FLAG_V;
	}
}

/*
 * -----------------------------------------------------------------
 * ADDB rb x
 *	v1	
 * -----------------------------------------------------------------
 */
void
c16x_addb_rb_x(uint8_t * icodeP)
{
	int n = (icodeP[1] & 0xf0) >> 4;
	uint16_t addr;
	int ri;
	uint8_t op1, op2;
	uint8_t result;
	int subcmd = (icodeP[1] & 0x0c) >> 2;
	switch (subcmd) {
	    case 0:
	    case 1:
		    op1 = C16x_ReadGpr8(n);
		    op2 = icodeP[1] & 0x7;
		    result = op1 + op2;
		    C16x_SetGpr8(result, n);
		    break;

	    case 2:
		    ri = icodeP[1] & 0x3;
		    addr = C16x_ReadGpr16(ri);
		    op1 = C16x_ReadGpr8(n);
		    op2 = C16x_MemRead8(addr);
		    result = op1 + op2;
		    C16x_SetGpr8(result, n);
		    break;

	    case 3:
		    ri = icodeP[1] & 0x3;
		    addr = C16x_ReadGpr16(ri);
		    op1 = C16x_ReadGpr8(n);
		    op2 = C16x_MemRead8(addr);
		    result = op1 + op2;
		    C16x_SetGpr8(result, n);
		    C16x_SetGpr16(addr + 2, ri);
		    break;
	    default:
		    return;
	}
	REG_PSW &= ~(PSW_FLAG_E | PSW_FLAG_Z | PSW_FLAG_V | PSW_FLAG_C | PSW_FLAG_N);
	if (result == 0x80) {
		REG_PSW |= PSW_FLAG_E;
	} else if (result == 0) {
		REG_PSW |= PSW_FLAG_Z;
	}
	if (result & 0x80) {
		REG_PSW |= PSW_FLAG_N;
	}
	if (add_carry_b(op1, op2, result)) {
		REG_PSW |= PSW_FLAG_C;
	}
	if (add_overflow_b(op1, op2, result)) {
		REG_PSW |= PSW_FLAG_V;
	}
}

/*
 * -------------------------------------------
 * Byte field low byte
 * v1
 * -------------------------------------------
 */
void
c16x_bfldl_boff_mask8_data8(uint8_t * icodeP)
{
	uint16_t op1 = C16x_ReadBitoff(icodeP[1]);
	uint16_t op2 = icodeP[2];
	uint16_t op3 = icodeP[3];
	uint16_t result;
	result = (op1 & ~(op2)) | op3;
	C16x_WriteBitoff(result, icodeP[1]);
	REG_PSW &= ~(PSW_FLAG_E | PSW_FLAG_Z | PSW_FLAG_V | PSW_FLAG_C | PSW_FLAG_N);
	if (result == 0) {
		REG_PSW |= PSW_FLAG_Z;
	}
	if (result & 0x8000) {
		REG_PSW |= PSW_FLAG_N;
	}
}

/*
 * ------------------------------------------------
 * Mul Rw Rw
 * v1
 * ------------------------------------------------
 */
void
c16x_mul_rw_rw(uint8_t * icodeP)
{
	int n = (icodeP[1] & 0xf0) >> 4;
	int m = (icodeP[1] & 0xf);
	int32_t result;
	int32_t op1 = (int16_t) C16x_ReadGpr16(n);
	int32_t op2 = (int16_t) C16x_ReadGpr16(m);
	result = op1 * op2;
	REG_MDL = result;
	REG_MDH = result >> 16;
	REG_PSW &= ~(PSW_FLAG_E | PSW_FLAG_Z | PSW_FLAG_V | PSW_FLAG_C | PSW_FLAG_N);
	if (((result & 0xffff8000) != 0) && ((result & 0xffff8000) != 0xffff8000)) {
		REG_PSW |= PSW_FLAG_V;
	}
	if (result == 0) {
		REG_PSW |= PSW_FLAG_Z;
	}
	if (result & 0x80000000) {
		REG_PSW |= PSW_FLAG_N;
	}
}

/*
 * ------------------------------------------------
 * Rol rw rw
 * v1
 * ------------------------------------------------
 */
void
c16x_rol_rw_rw(uint8_t * icodeP)
{
	int n = (icodeP[1] & 0xf0) >> 4;
	int m = (icodeP[1] & 0xf);
	uint16_t rot = C16x_ReadGpr16(m) & 0xf;
	uint16_t val = C16x_ReadGpr16(n);
	uint16_t result;
	REG_PSW &= ~(PSW_FLAG_E | PSW_FLAG_Z | PSW_FLAG_V | PSW_FLAG_C | PSW_FLAG_N);
	if (rot) {
		result = (val << rot) | (val >> (16 - rot));
		if (result & 1) {
			REG_PSW |= PSW_FLAG_C;
		}
	} else {
		result = val;
	}
	if (result == 0) {
		REG_PSW |= PSW_FLAG_Z;
	}
	if (result & 0x8000) {
		REG_PSW |= PSW_FLAG_N;
	}
	C16x_SetGpr16(result, n);
}

/*
 * ----------------------------------------------------
 * The (conditional) relative jump
 * v1
 * ----------------------------------------------------
 */
void
c16x_jmpr_cc_rel(uint8_t * icodeP)
{
	int8_t offs;
	fprintf(stderr, "icodeP[0] %02x icodeP[1] %02x\n", icodeP[0], icodeP[1]);
	fprintf(stderr, "PSW %02x\n", REG_PSW);
	if (check_condition(icodeP[0])) {
		offs = icodeP[1];
		REG_IP = REG_IP + offs + offs;
	}
}

/*
 * -------------------------------------------------------
 * Clear Bit
 * v1
 * -------------------------------------------------------
 */
void
c16x_bclr(uint8_t * icodeP)
{
	int bit = (icodeP[0] & 0xf0) >> 4;
	uint8_t bitaddr = icodeP[1];
	uint16_t value;
	value = C16x_ReadBitaddr(bitaddr);
	REG_PSW &= ~(PSW_FLAG_E | PSW_FLAG_Z | PSW_FLAG_V | PSW_FLAG_C | PSW_FLAG_N);
	if (value & (1 << bit)) {
		REG_PSW |= PSW_FLAG_N;
	} else {
		REG_PSW |= PSW_FLAG_Z;
	}
	value &= ~(1 << bit);
	C16x_WriteBitaddr(value, bitaddr);
}

/*
 * -------------------------------------------------------
 * Set Bit
 * v1
 * -------------------------------------------------------
 */

void
c16x_bset(uint8_t * icodeP)
{
	int bit = (icodeP[0] & 0xf0) >> 4;
	uint8_t bitaddr = icodeP[1];
	uint16_t value;
	value = C16x_ReadBitaddr(bitaddr);
	REG_PSW &= ~(PSW_FLAG_E | PSW_FLAG_Z | PSW_FLAG_V | PSW_FLAG_C | PSW_FLAG_N);
	if (value & (1 << bit)) {
		REG_PSW |= PSW_FLAG_N;
	} else {
		REG_PSW |= PSW_FLAG_Z;
	}
	value |= (1 << bit);
	C16x_WriteBitaddr(value, bitaddr);
}

/*
 * -----------------------------------------------------
 * ADDC
 * v1
 * -----------------------------------------------------
 */
void
c16x_addc_rw_rw(uint8_t * icodeP)
{
	int n = (icodeP[1] & 0xf0) >> 4;
	int m = (icodeP[1] & 0xf);
	uint16_t op1, op2;
	uint16_t result;
	op1 = C16x_ReadGpr16(n);
	op2 = C16x_ReadGpr16(m);
	if (REG_PSW & PSW_FLAG_C) {
		result = op1 + op2 + 1;
	} else {
		result = op1 + op2;
	}
	REG_PSW &= ~(PSW_FLAG_E | PSW_FLAG_V | PSW_FLAG_C | PSW_FLAG_N);
	if (result == 0x8000) {
		REG_PSW |= PSW_FLAG_E;
	}
	if (result & 0x8000) {
		REG_PSW |= PSW_FLAG_N;
	}
	if (result != 0) {
		REG_PSW &= ~PSW_FLAG_Z;
	}
	if (add_carry(op1, op2, result)) {
		REG_PSW |= PSW_FLAG_C;
	}
	if (add_overflow(op1, op2, result)) {
		REG_PSW |= PSW_FLAG_V;
	}
	C16x_SetGpr16(result, n);
}

/*
 * -----------------------------------------------------
 * ADDCB
 * v1	
 * -----------------------------------------------------
 */
void
c16x_addcb_rb_rb(uint8_t * icodeP)
{
	int n = (icodeP[1] & 0xf0) >> 4;
	int m = (icodeP[1] & 0xf);
	uint8_t op1, op2;
	uint8_t result;
	op1 = C16x_ReadGpr8(n);
	op2 = C16x_ReadGpr8(m);
	if (REG_PSW & PSW_FLAG_C) {
		result = op1 + op2 + 1;
	} else {
		result = op1 + op2;
	}
	REG_PSW &= ~(PSW_FLAG_E | PSW_FLAG_V | PSW_FLAG_C | PSW_FLAG_N);
	if (result == 0x80) {
		REG_PSW |= PSW_FLAG_E;
	}
	if (result & 0x80) {
		REG_PSW |= PSW_FLAG_N;
	}
	if (result != 0) {
		REG_PSW &= ~PSW_FLAG_Z;
	}
	if (add_carry_b(op1, op2, result)) {
		REG_PSW |= PSW_FLAG_C;
	}
	if (add_overflow_b(op1, op2, result)) {
		REG_PSW |= PSW_FLAG_V;
	}
	C16x_SetGpr8(result, n);
}

/*
 * -----------------------------------------------------
 * ADDC reg mem
 * v1
 * -----------------------------------------------------
 */
void
c16x_addc_reg_mem(uint8_t * icodeP)
{
	uint8_t reg = icodeP[1] & 0xff;
	uint16_t maddr = (icodeP[3] << 8) | icodeP[2];
	uint16_t op1, op2;
	uint16_t result;
	op1 = C16x_ReadReg16(reg);
	op2 = C16x_MemRead16(maddr);
	if (REG_PSW & PSW_FLAG_C) {
		result = op1 + op2 + 1;
	} else {
		result = op1 + op2;
	}
	REG_PSW &= ~(PSW_FLAG_E | PSW_FLAG_V | PSW_FLAG_C | PSW_FLAG_N);
	if (result == 0x8000) {
		REG_PSW |= PSW_FLAG_E;
	}
	if (result & 0x8000) {
		REG_PSW |= PSW_FLAG_N;
	}
	if (result != 0) {
		REG_PSW &= ~PSW_FLAG_Z;
	}
	if (add_carry(op1, op2, result)) {
		REG_PSW |= PSW_FLAG_C;
	}
	if (add_overflow(op1, op2, result)) {
		REG_PSW |= PSW_FLAG_V;
	}
	C16x_SetReg16(result, reg);
}

/*
 * ----------------------------------------------------
 * addcb_reg_mem
 * v1
 * ----------------------------------------------------
 */
void
c16x_addcb_reg_mem(uint8_t * icodeP)
{
	uint8_t reg = icodeP[1] & 0xff;
	uint16_t maddr = (icodeP[3] << 8) | icodeP[2];
	uint8_t op1, op2;
	uint8_t result;
	op1 = C16x_ReadReg8(reg);
	op2 = C16x_MemRead8(maddr);
	if (REG_PSW & PSW_FLAG_C) {
		result = op1 + op2 + 1;
	} else {
		result = op1 + op2;
	}
	REG_PSW &= ~(PSW_FLAG_E | PSW_FLAG_V | PSW_FLAG_C | PSW_FLAG_N);
	if (result == 0x80) {
		REG_PSW |= PSW_FLAG_E;
	}
	if (result & 0x80) {
		REG_PSW |= PSW_FLAG_N;
	}
	if (result != 0) {
		REG_PSW &= ~PSW_FLAG_Z;
	}
	if (add_carry_b(op1, op2, result)) {
		REG_PSW |= PSW_FLAG_C;
	}
	if (add_overflow_b(op1, op2, result)) {
		REG_PSW |= PSW_FLAG_V;
	}
	C16x_SetReg8(result, reg);
}

/*
 * ------------------------------------------------------------
 * ADDC mem reg
 * v1
 * ------------------------------------------------------------
 */
void
c16x_addc_mem_reg(uint8_t * icodeP)
{
	uint8_t reg = icodeP[1] & 0xff;
	uint16_t maddr = (icodeP[3] << 8) | icodeP[2];
	uint16_t op1, op2;
	uint16_t result;
	op1 = C16x_MemRead16(maddr);
	op2 = C16x_ReadReg16(reg);
	if (REG_PSW & PSW_FLAG_C) {
		result = op1 + op2 + 1;
	} else {
		result = op1 + op2;
	}
	REG_PSW &= ~(PSW_FLAG_E | PSW_FLAG_V | PSW_FLAG_C | PSW_FLAG_N);
	if (result == 0x8000) {
		REG_PSW |= PSW_FLAG_E;
	}
	if (result & 0x8000) {
		REG_PSW |= PSW_FLAG_N;
	}
	if (result != 0) {
		REG_PSW &= ~PSW_FLAG_Z;
	}
	if (add_carry(op1, op2, result)) {
		REG_PSW |= PSW_FLAG_C;
	}
	if (add_overflow(op1, op2, result)) {
		REG_PSW |= PSW_FLAG_V;
	}
	C16x_MemWrite16(result, maddr);
}

/*
 * --------------------------------------------------------------
 * addcb mem reg
 * v1
 * --------------------------------------------------------------
 */
void
c16x_addcb_mem_reg(uint8_t * icodeP)
{
	uint8_t reg = icodeP[1] & 0xff;
	uint16_t maddr = ((uint16_t) icodeP[3] << 8) | icodeP[2];
	uint8_t op1, op2;
	uint8_t result;
	op1 = C16x_MemRead8(maddr);
	op2 = C16x_ReadReg8(reg);
	if (REG_PSW & PSW_FLAG_C) {
		result = op1 + op2 + 1;
	} else {
		result = op1 + op2;
	}
	REG_PSW &= ~(PSW_FLAG_E | PSW_FLAG_V | PSW_FLAG_C | PSW_FLAG_N);
	if (result == 0x80) {
		REG_PSW |= PSW_FLAG_E;
	}
	if (result & 0x80) {
		REG_PSW |= PSW_FLAG_N;
	}
	if (result != 0) {
		REG_PSW &= ~PSW_FLAG_Z;
	}
	if (add_carry_b(op1, op2, result)) {
		REG_PSW |= PSW_FLAG_C;
	}
	if (add_overflow_b(op1, op2, result)) {
		REG_PSW |= PSW_FLAG_V;
	}
	C16x_MemWrite8(result, maddr);
}

/*
 * ------------------------------------------------------
 * ADDC
 * v1
 * ------------------------------------------------------
 */
void
c16x_addc_reg_data16(uint8_t * icodeP)
{
	uint8_t reg = icodeP[1] & 0xff;
	uint16_t data16 = ((uint16_t) icodeP[3] << 8) | icodeP[2];
	uint16_t op1, op2;
	uint16_t result;
	op1 = C16x_ReadReg16(reg);
	op2 = data16;
	if (REG_PSW & PSW_FLAG_C) {
		result = op1 + op2 + 1;
	} else {
		result = op1 + op2;
	}
	REG_PSW &= ~(PSW_FLAG_E | PSW_FLAG_V | PSW_FLAG_C | PSW_FLAG_N);
	if (result == 0x8000) {
		REG_PSW |= PSW_FLAG_E;
	}
	if (result & 0x8000) {
		REG_PSW |= PSW_FLAG_N;
	}
	if (result != 0) {
		REG_PSW &= ~PSW_FLAG_Z;
	}
	if (add_carry(op1, op2, result)) {
		REG_PSW |= PSW_FLAG_C;
	}
	if (add_overflow(op1, op2, result)) {
		REG_PSW |= PSW_FLAG_V;
	}
	C16x_SetReg16(result, reg);
}

/*
 * ------------------------------------------------------
 * ADDCB reg data8
 * v1
 * ------------------------------------------------------
 */
void
c16x_addcb_reg_data8(uint8_t * icodeP)
{
	uint8_t reg = icodeP[1] & 0xff;
	uint8_t data8 = icodeP[2];
	uint8_t op1, op2;
	uint8_t result;
	op1 = C16x_ReadReg8(reg);
	op2 = data8;
	if (REG_PSW & PSW_FLAG_C) {
		result = op1 + op2 + 1;
	} else {
		result = op1 + op2;
	}
	C16x_SetReg8(result, reg);
	REG_PSW &= ~(PSW_FLAG_E | PSW_FLAG_V | PSW_FLAG_C | PSW_FLAG_N);
	if (result == 0x80) {
		REG_PSW |= PSW_FLAG_E;
	}
	if (result & 0x80) {
		REG_PSW |= PSW_FLAG_N;
	}
	if (result != 0) {
		REG_PSW &= ~PSW_FLAG_Z;
	}
	if (add_carry_b(op1, op2, result)) {
		REG_PSW |= PSW_FLAG_C;
	}
	if (add_overflow_b(op1, op2, result)) {
		REG_PSW |= PSW_FLAG_V;
	}
}

/*
 * -------------------------------------------------------------
 * ADDC rw x
 * v1
 * -------------------------------------------------------------
 */
void
c16x_addc_rw_x(uint8_t * icodeP)
{
	int n = (icodeP[1] & 0xf0) >> 4;
	uint16_t addr;
	int ri;
	uint16_t op1, op2;
	uint16_t result;
	int subcmd = (icodeP[1] & 0x0c) >> 2;
	op1 = C16x_ReadGpr16(n);
	switch (subcmd) {
	    case 0:
	    case 1:
		    op1 = C16x_ReadGpr16(n);
		    op2 = icodeP[1] & 0x7;
		    result = op1 + op2;
		    if (REG_PSW & PSW_FLAG_C) {
			    result++;
		    }
		    break;

	    case 2:
		    ri = icodeP[1] & 0x3;
		    addr = C16x_ReadGpr16(ri);
		    op2 = C16x_MemRead16(addr);
		    result = op1 + op2;
		    if (REG_PSW & PSW_FLAG_C) {
			    result++;
		    }
		    break;

	    case 3:
		    ri = icodeP[1] & 0x3;
		    addr = C16x_ReadGpr16(ri);
		    op2 = C16x_MemRead16(addr);
		    result = op1 + op2;
		    if (REG_PSW & PSW_FLAG_C) {
			    result++;
		    }
		    C16x_SetGpr16(addr + 2, ri);
		    break;
	    default:
		    fprintf(stderr, "reached unreachable code\n");
		    return;
	}
	C16x_SetGpr16(result, n);
	REG_PSW &= ~(PSW_FLAG_E | PSW_FLAG_V | PSW_FLAG_C | PSW_FLAG_N);
	if (result == 0x8000) {
		REG_PSW |= PSW_FLAG_E;
	}
	if (result & 0x8000) {
		REG_PSW |= PSW_FLAG_N;
	}
	if (result != 0) {
		REG_PSW &= ~PSW_FLAG_Z;
	}
	if (add_carry(op1, op2, result)) {
		REG_PSW |= PSW_FLAG_C;
	}
	if (add_overflow(op1, op2, result)) {
		REG_PSW |= PSW_FLAG_V;
	}
}

/*
 * --------------------------------------------
 * addcb rb x
 * v1
 * --------------------------------------------
 */
void
c16x_addcb_rb_x(uint8_t * icodeP)
{
	int n = (icodeP[1] & 0xf0) >> 4;
	uint16_t addr;
	int ri;
	uint8_t op1, op2;
	uint8_t result;
	int subcmd = (icodeP[1] & 0x0c) >> 2;
	op1 = C16x_ReadGpr8(n);
	switch (subcmd) {
	    case 0:
	    case 1:
		    op2 = icodeP[1] & 0x7;
		    result = op1 + op2;
		    if (REG_PSW & PSW_FLAG_C) {
			    result++;
		    }
		    break;

	    case 2:
		    ri = icodeP[1] & 0x3;
		    addr = C16x_ReadGpr16(ri);
		    op2 = C16x_MemRead8(addr);
		    result = op1 + op2;
		    if (REG_PSW & PSW_FLAG_C) {
			    result++;
		    }
		    break;

	    case 3:
		    ri = icodeP[1] & 0x3;
		    addr = C16x_ReadGpr16(ri);
		    op2 = C16x_MemRead8(addr);
		    result = op1 + op2;
		    if (REG_PSW & PSW_FLAG_C) {
			    result++;
		    }
		    C16x_SetGpr16(addr + 2, ri);
		    break;
	    default:
		    fprintf(stderr, "reached unreachable code\n");
		    return;
	}
	REG_PSW &= ~(PSW_FLAG_E | PSW_FLAG_V | PSW_FLAG_C | PSW_FLAG_N);
	if (result == 0x80) {
		REG_PSW |= PSW_FLAG_E;
	}
	if (result & 0x80) {
		REG_PSW |= PSW_FLAG_N;
	}
	if (result != 0) {
		REG_PSW &= ~PSW_FLAG_Z;
	}
	if (add_carry_b(op1, op2, result)) {
		REG_PSW |= PSW_FLAG_C;
	}
	if (add_overflow_b(op1, op2, result)) {
		REG_PSW |= PSW_FLAG_V;
	}
	C16x_SetGpr8(result, n);
}

/*
 * -------------------------------------------------------
 * BFLDH boff mask8 data8
 * v1
 * -------------------------------------------------------
 */
void
c16x_bfldh_boff_mask8_data8(uint8_t * icodeP)
{
	uint16_t op1 = C16x_ReadBitoff(icodeP[1]);
	uint16_t op2 = icodeP[2];
	uint16_t op3 = icodeP[3];
	uint16_t result;
	result = (op1 & ~(op2 << 8)) | (op3 << 8);
	C16x_WriteBitoff(result, icodeP[1]);
	REG_PSW &= ~(PSW_FLAG_E | PSW_FLAG_Z | PSW_FLAG_V | PSW_FLAG_C | PSW_FLAG_N);
	if (result == 0) {
		REG_PSW |= PSW_FLAG_Z;
	}
	if (result & 0x8000) {
		REG_PSW |= PSW_FLAG_N;
	}
}

/*
 * ------------------------------------------------------
 * MULU Rw Rw
 * v1
 * ------------------------------------------------------
 */
void
c16x_mulu_rw_rw(uint8_t * icodeP)
{
	int n = (icodeP[1] & 0xf0) >> 4;
	int m = (icodeP[1] & 0xf);
	uint32_t result;
	uint32_t op1 = C16x_ReadGpr16(n);
	uint32_t op2 = C16x_ReadGpr16(m);
	result = op1 * op2;
	REG_MDL = result;
	REG_MDH = result >> 16;
	REG_PSW &= ~(PSW_FLAG_E | PSW_FLAG_Z | PSW_FLAG_V | PSW_FLAG_C | PSW_FLAG_N);
	if (result & 0xffff0000) {
		REG_PSW |= PSW_FLAG_V;
	}
	if (result == 0) {
		REG_PSW |= PSW_FLAG_Z;
	}
	if (result & 0x80000000) {
		REG_PSW |= PSW_FLAG_N;
	}
}

/*
 * --------------------------------------------------
 * Rol Rw data 4
 * v1
 * --------------------------------------------------
 */
void
c16x_rol_rw_data4(uint8_t * icodeP)
{
	uint16_t rot = (icodeP[1] & 0xf0) >> 4;
	int n = (icodeP[1] & 0xf);
	uint16_t val = C16x_ReadGpr16(n);
	uint16_t result;
	REG_PSW &= ~(PSW_FLAG_E | PSW_FLAG_Z | PSW_FLAG_V | PSW_FLAG_C | PSW_FLAG_N);
	if (rot) {
		result = (val << rot) | (val >> (16 - rot));
		if (result & 1) {
			REG_PSW |= PSW_FLAG_C;
		}
	} else {
		result = val;
	}
	if (result == 0) {
		REG_PSW |= PSW_FLAG_Z;
	}
	if (result & 0x8000) {
		REG_PSW |= PSW_FLAG_N;
	}
	C16x_SetGpr16(val, n);
}

/*
 * ------------------------------------------------------
 * Sub Rw Rw
 * v1
 * ------------------------------------------------------
 */

void
c16x_sub_rw_rw(uint8_t * icodeP)
{
	int n = (icodeP[1] & 0xf0) >> 4;
	int m = (icodeP[1] & 0xf);
	uint16_t op1, op2;
	uint16_t result;
	op1 = C16x_ReadGpr16(n);
	op2 = C16x_ReadGpr16(m);
	result = op1 - op2;
	C16x_SetGpr16(result, n);
	REG_PSW &= ~(PSW_FLAG_E | PSW_FLAG_Z | PSW_FLAG_V | PSW_FLAG_C | PSW_FLAG_N);
	if (result == 0x8000) {
		REG_PSW |= PSW_FLAG_E;
	}
	if (result == 0) {
		REG_PSW |= PSW_FLAG_Z;
	}
	if (result & 0x8000) {
		REG_PSW |= PSW_FLAG_N;
	}
	if (sub_carry(op1, op2, result)) {
		REG_PSW |= PSW_FLAG_C;
	}
	if (sub_overflow(op1, op2, result)) {
		REG_PSW |= PSW_FLAG_V;
	}

}

/*
 * -----------------------------------------------
 * SUBB rb rb
 * v1
 * -----------------------------------------------
 */
void
c16x_subb_rb_rb(uint8_t * icodeP)
{
	int n = (icodeP[1] & 0xf0) >> 4;
	int m = (icodeP[1] & 0xf);
	uint8_t op1, op2;
	uint8_t result;

	op1 = C16x_ReadGpr8(n);
	op2 = C16x_ReadGpr8(m);
	result = op1 - op2;
	C16x_SetGpr8(result, n);

	REG_PSW &= ~(PSW_FLAG_E | PSW_FLAG_Z | PSW_FLAG_V | PSW_FLAG_C | PSW_FLAG_N);
	if (result == 0x80) {
		REG_PSW |= PSW_FLAG_E;
	} else if (result == 0) {
		REG_PSW |= PSW_FLAG_Z;
	}
	if (result & 0x80) {
		REG_PSW |= PSW_FLAG_N;
	}
	if (sub_carry_b(op1, op2, result)) {
		REG_PSW |= PSW_FLAG_C;
	}
	if (sub_overflow_b(op1, op2, result)) {
		REG_PSW |= PSW_FLAG_V;
	}
}

/*
 * ---------------------------------------------------------
 * SUB reg mem
 * v1
 * ---------------------------------------------------------
 */
void
c16x_sub_reg_mem(uint8_t * icodeP)
{
	uint8_t reg = icodeP[1] & 0xff;
	uint16_t maddr = (icodeP[3] << 8) | icodeP[2];
	uint16_t op1, op2;
	uint16_t result;
	op1 = C16x_ReadReg16(reg);
	op2 = C16x_MemRead16(maddr);
	result = op1 - op2;
	C16x_SetReg16(result, reg);
	REG_PSW &= ~(PSW_FLAG_E | PSW_FLAG_Z | PSW_FLAG_V | PSW_FLAG_C | PSW_FLAG_N);
	if (result == 0x8000) {
		REG_PSW |= PSW_FLAG_E;
	}
	if (result == 0) {
		REG_PSW |= PSW_FLAG_Z;
	}
	if (result & 0x8000) {
		REG_PSW |= PSW_FLAG_N;
	}
	if (sub_carry(op1, op2, result)) {
		REG_PSW |= PSW_FLAG_C;
	}
	if (sub_overflow(op1, op2, result)) {
		REG_PSW |= PSW_FLAG_V;
	}
}

/*
 * ---------------------------------------------------------
 * subb reg mem
 * v1	
 * ---------------------------------------------------------
 */
void
c16x_subb_reg_mem(uint8_t * icodeP)
{
	uint8_t reg = icodeP[1] & 0xff;
	uint16_t maddr = (icodeP[3] << 8) | icodeP[2];
	uint8_t op1, op2;
	uint8_t result;
	op1 = C16x_ReadReg8(reg);
	op2 = C16x_MemRead8(maddr);
	result = op1 - op2;
	C16x_SetReg8(result, reg);

	REG_PSW &= ~(PSW_FLAG_E | PSW_FLAG_Z | PSW_FLAG_V | PSW_FLAG_C | PSW_FLAG_N);
	if (result == 0x80) {
		REG_PSW |= PSW_FLAG_E;
	} else if (result == 0) {
		REG_PSW |= PSW_FLAG_Z;
	}
	if (result & 0x80) {
		REG_PSW |= PSW_FLAG_N;
	}
	if (sub_carry_b(op1, op2, result)) {
		REG_PSW |= PSW_FLAG_C;
	}
	if (sub_overflow_b(op1, op2, result)) {
		REG_PSW |= PSW_FLAG_V;
	}
}

/*
 * ---------------------------------------------------------------------
 * sub mem reg
 * v1
 * ---------------------------------------------------------------------
 */
void
c16x_sub_mem_reg(uint8_t * icodeP)
{
	uint8_t reg = icodeP[1] & 0xff;
	uint16_t maddr = (icodeP[3] << 8) | icodeP[2];
	uint16_t op1, op2;
	uint16_t result;
	op1 = C16x_MemRead16(maddr);
	op2 = C16x_ReadReg16(reg);
	result = op1 - op2;
	C16x_MemWrite16(result, maddr);

	REG_PSW &= ~(PSW_FLAG_E | PSW_FLAG_Z | PSW_FLAG_V | PSW_FLAG_C | PSW_FLAG_N);
	if (result == 0x8000) {
		REG_PSW |= PSW_FLAG_E;
	} else if (result == 0) {
		REG_PSW |= PSW_FLAG_Z;
	}
	if (result & 0x8000) {
		REG_PSW |= PSW_FLAG_N;
	}
	if (sub_carry(op1, op2, result)) {
		REG_PSW |= PSW_FLAG_C;
	}
	if (sub_overflow(op1, op2, result)) {
		REG_PSW |= PSW_FLAG_V;
	}
}

/*
 * ----------------------------------------------------------
 * SUBB mem reg
 * v1
 * ----------------------------------------------------------
 */
void
c16x_subb_mem_reg(uint8_t * icodeP)
{
	uint8_t reg = icodeP[1] & 0xff;
	uint16_t maddr = (icodeP[3] << 8) | icodeP[2];
	uint8_t op1, op2;
	uint8_t result;
	op1 = C16x_MemRead8(maddr);
	op2 = C16x_ReadReg8(reg);
	result = op1 - op2;
	C16x_MemWrite8(result, maddr);

	REG_PSW &= ~(PSW_FLAG_E | PSW_FLAG_Z | PSW_FLAG_V | PSW_FLAG_C | PSW_FLAG_N);
	if (result == 0x80) {
		REG_PSW |= PSW_FLAG_E;
	} else if (result == 0) {
		REG_PSW |= PSW_FLAG_Z;
	}
	if (result & 0x80) {
		REG_PSW |= PSW_FLAG_N;
	}
	if (sub_carry_b(op1, op2, result)) {
		REG_PSW |= PSW_FLAG_C;
	}
	if (sub_overflow_b(op1, op2, result)) {
		REG_PSW |= PSW_FLAG_V;
	}
}

/*
 * ----------------------------------------------------------------
 * SUB reg data16
 * v1	
 * ----------------------------------------------------------------
 */
void
c16x_sub_reg_data16(uint8_t * icodeP)
{
	uint8_t reg = icodeP[1] & 0xff;
	uint16_t data16 = (icodeP[3] << 8) | icodeP[2];
	uint16_t op1, op2;
	uint16_t result;
	op1 = C16x_ReadReg16(reg);
	op2 = data16;
	result = op1 - op2;
	C16x_SetReg16(result, reg);

	REG_PSW &= ~(PSW_FLAG_E | PSW_FLAG_Z | PSW_FLAG_V | PSW_FLAG_C | PSW_FLAG_N);
	if (result == 0x8000) {
		REG_PSW |= PSW_FLAG_E;
	} else if (result == 0) {
		REG_PSW |= PSW_FLAG_Z;
	}
	if (result & 0x8000) {
		REG_PSW |= PSW_FLAG_N;
	}
	if (sub_carry(op1, op2, result)) {
		REG_PSW |= PSW_FLAG_C;
	}
	if (sub_overflow(op1, op2, result)) {
		REG_PSW |= PSW_FLAG_V;
	}
}

/*
 * ----------------------------------------------------------
 * SUBB reg data8
 * v1
 * ----------------------------------------------------------
 */
void
c16x_subb_reg_data8(uint8_t * icodeP)
{
	uint8_t reg = icodeP[1] & 0xff;
	uint8_t data8 = icodeP[2];
	uint8_t op1, op2;
	uint8_t result;
	op1 = C16x_ReadReg8(reg);
	op2 = data8;
	result = op1 - op2;
	C16x_SetReg8(result, reg);

	REG_PSW &= ~(PSW_FLAG_E | PSW_FLAG_Z | PSW_FLAG_V | PSW_FLAG_C | PSW_FLAG_N);
	if (result == 0x80) {
		REG_PSW |= PSW_FLAG_E;
	} else if (result == 0) {
		REG_PSW |= PSW_FLAG_Z;
	}
	if (result & 0x80) {
		REG_PSW |= PSW_FLAG_N;
	}
	if (sub_carry_b(op1, op2, result)) {
		REG_PSW |= PSW_FLAG_C;
	}
	if (sub_overflow_b(op1, op2, result)) {
		REG_PSW |= PSW_FLAG_V;
	}
}

/*
 * ---------------------------------------------------
 * SUB rw x
 * v1
 * ---------------------------------------------------
 */
void
c16x_sub_rw_x(uint8_t * icodeP)
{
	int n = (icodeP[1] & 0xf0) >> 4;
	uint16_t addr;
	int ri;
	uint16_t op1, op2;
	uint16_t result;
	int subcmd = (icodeP[1] & 0x0c) >> 2;
	op1 = C16x_ReadGpr16(n);
	switch (subcmd) {
	    case 0:
	    case 1:
		    op2 = icodeP[1] & 0x7;
		    break;

	    case 2:
		    ri = icodeP[1] & 0x3;
		    addr = C16x_ReadGpr16(ri);
		    op2 = C16x_MemRead16(addr);
		    break;

	    case 3:
		    ri = icodeP[1] & 0x3;
		    addr = C16x_ReadGpr16(ri);
		    op2 = C16x_MemRead16(addr);
		    C16x_SetGpr16(addr + 2, ri);
		    break;
	    default:
		    return;
	}
	result = op1 - op2;
	C16x_SetGpr16(result, n);

	REG_PSW &= ~(PSW_FLAG_E | PSW_FLAG_Z | PSW_FLAG_V | PSW_FLAG_C | PSW_FLAG_N);
	if (result == 0x8000) {
		REG_PSW |= PSW_FLAG_E;
	} else if (result == 0) {
		REG_PSW |= PSW_FLAG_Z;
	}
	if (result & 0x8000) {
		REG_PSW |= PSW_FLAG_N;
	}
	if (sub_carry(op1, op2, result)) {
		REG_PSW |= PSW_FLAG_C;
	}
	if (sub_overflow(op1, op2, result)) {
		REG_PSW |= PSW_FLAG_V;
	}
}

/*
 * -------------------------------------------------------
 * SUBB rb x
 * v1
 * -------------------------------------------------------
 */
void
c16x_subb_rb_x(uint8_t * icodeP)
{
	int n = (icodeP[1] & 0xf0) >> 4;
	uint16_t addr;
	int ri;
	uint8_t op1, op2;
	uint8_t result;
	int subcmd = (icodeP[1] & 0x0c) >> 2;
	op1 = C16x_ReadGpr8(n);
	switch (subcmd) {
	    case 0:
	    case 1:
		    op2 = icodeP[1] & 0x7;
		    break;

	    case 2:
		    ri = icodeP[1] & 0x3;
		    addr = C16x_ReadGpr16(ri);
		    op2 = C16x_MemRead8(addr);
		    break;

	    case 3:
		    ri = icodeP[1] & 0x3;
		    addr = C16x_ReadGpr16(ri);
		    op2 = C16x_MemRead8(addr);
		    C16x_SetGpr16(addr + 2, ri);
		    break;
	    default:
		    return;
	}
	result = op1 - op2;
	C16x_SetGpr8(result, n);
	REG_PSW &= ~(PSW_FLAG_E | PSW_FLAG_Z | PSW_FLAG_V | PSW_FLAG_C | PSW_FLAG_N);
	if (result == 0x80) {
		REG_PSW |= PSW_FLAG_E;
	}
	if (result == 0) {
		REG_PSW |= PSW_FLAG_Z;
	}
	if (result & 0x80) {
		REG_PSW |= PSW_FLAG_N;
	}
	if (sub_carry_b(op1, op2, result)) {
		REG_PSW |= PSW_FLAG_C;
	}
	if (sub_overflow_b(op1, op2, result)) {
		REG_PSW |= PSW_FLAG_V;
	}
}

/*
 * -----------------------------------------------------------------------
 * bcmp bitaddr
 * v1
 * -----------------------------------------------------------------------
 */
void
c16x_bcmp_bitaddr_bitaddr(uint8_t * icodeP)
{
	uint16_t qqaddr = icodeP[1];
	uint16_t qbit = (icodeP[3] & 0xf0) >> 4;
	uint16_t zzaddr = icodeP[2];
	uint16_t zbit = icodeP[3] & 0xf;
	uint16_t op1 = C16x_ReadBitaddr(qqaddr);
	uint16_t op2 = C16x_ReadBitaddr(zzaddr);
	int bit1 = (op1 >> qbit) & 1;
	int bit2 = (op2 >> zbit) & 1;
	REG_PSW &= ~(PSW_FLAG_E | PSW_FLAG_Z | PSW_FLAG_V | PSW_FLAG_C | PSW_FLAG_N);
	if (bit1 | bit2) {
		REG_PSW = REG_PSW | PSW_FLAG_V;
	} else {
		REG_PSW = REG_PSW | PSW_FLAG_Z;
	}
	if (bit1 ^ bit2) {
		REG_PSW = REG_PSW | PSW_FLAG_N;
	}
	if (bit1 & bit2) {
		REG_PSW = REG_PSW | PSW_FLAG_C;
	}
}

/*
 * ------------------------------------------------------
 * Prior Rw Rw 
 * v1
 * ------------------------------------------------------
 */
void
c16x_prior_rw_rw(uint8_t * icodeP)
{
	int n = icodeP[1] >> 4;
	int m = icodeP[1] & 0xf;
	uint16_t tmp = C16x_ReadGpr16(m);
	uint16_t count = 0;
	REG_PSW &= ~(PSW_FLAG_E | PSW_FLAG_Z | PSW_FLAG_V | PSW_FLAG_C | PSW_FLAG_N);
	if (tmp == 0) {
		REG_PSW = REG_PSW | PSW_FLAG_Z;
	} else {
		while (!(tmp & (1 << 15))) {
			tmp = tmp << 1;
			count++;
		}
	}
	C16x_SetGpr16(count, n);
}

/*
 * ---------------------------------------------------------------
 * Ror rw rw
 * v1
 * ---------------------------------------------------------------
 */
void
c16x_ror_rw_rw(uint8_t * icodeP)
{
	int n = (icodeP[1] & 0xf0) >> 4;
	int m = (icodeP[1] & 0xf);
	uint16_t op1, rot;
	uint16_t result;
	op1 = C16x_ReadGpr16(n);
	rot = C16x_ReadGpr16(m) & 0xf;
	REG_PSW &= ~(PSW_FLAG_V | PSW_FLAG_C | PSW_FLAG_E | PSW_FLAG_N | PSW_FLAG_C);
	if (rot) {
		result = op1 >> rot;
		if ((result << rot) != op1) {
			REG_PSW |= PSW_FLAG_V;
		}
		result = result | (op1 << (16 - rot));
		if (result & (1 << 15)) {
			REG_PSW |= PSW_FLAG_C;
		}
	} else {
		result = op1;
	}
	C16x_SetGpr16(result, n);
	if (result == 0) {
		REG_PSW |= PSW_FLAG_Z;
	}
	if (result & 0x8000) {
		REG_PSW |= PSW_FLAG_N;
	}
}

/*
 * ------------------------------------------------------
 * Subc rw rw
 * v1
 * ------------------------------------------------------
 */

void
c16x_subc_rw_rw(uint8_t * icodeP)
{
	int n = (icodeP[1] & 0xf0) >> 4;
	int m = (icodeP[1] & 0xf);
	uint16_t op1, op2;
	uint16_t result;
	op1 = C16x_ReadGpr16(n);
	op2 = C16x_ReadGpr16(m);
	if (REG_PSW & PSW_FLAG_C) {
		result = op1 - op2 - 1;
	} else {
		result = op1 - op2;
	}
	C16x_SetGpr16(result, n);
	REG_PSW &= ~(PSW_FLAG_E | PSW_FLAG_V | PSW_FLAG_C | PSW_FLAG_N);
	if (result == 0x8000) {
		REG_PSW |= PSW_FLAG_E;
	}
	if (result == 0) {
		REG_PSW &= ~PSW_FLAG_Z;
	}
	if (result & 0x8000) {
		REG_PSW |= PSW_FLAG_N;
	}
	if (sub_carry(op1, op2, result)) {
		REG_PSW |= PSW_FLAG_C;
	}
	if (sub_overflow(op1, op2, result)) {
		REG_PSW |= PSW_FLAG_V;
	}
}

/*
 * ----------------------------------------------------------
 * SUBC rb rb
 * v1
 * ----------------------------------------------------------
 */
void
c16x_subcb_rb_rb(uint8_t * icodeP)
{
	int n = (icodeP[1] & 0xf0) >> 4;
	int m = (icodeP[1] & 0xf);
	uint8_t op1, op2;
	uint8_t result;
	op1 = C16x_ReadGpr8(n);
	op2 = C16x_ReadGpr8(m);
	if (REG_PSW & PSW_FLAG_C) {
		result = op1 - op2 - 1;
	} else {
		result = op1 - op2;
	}
	C16x_SetGpr8(result, n);
	REG_PSW &= ~(PSW_FLAG_E | PSW_FLAG_V | PSW_FLAG_C | PSW_FLAG_N);
	if (result == 0x80) {
		REG_PSW |= PSW_FLAG_E;
	}
	if (result != 0) {
		REG_PSW &= ~PSW_FLAG_Z;
	}
	if (result & 0x80) {
		REG_PSW |= PSW_FLAG_N;
	}
	if (sub_carry_b(op1, op2, result)) {
		REG_PSW |= PSW_FLAG_C;
	}
	if (sub_overflow_b(op1, op2, result)) {
		REG_PSW |= PSW_FLAG_V;
	}
}

/*
 * ------------------------------------------------------------------
 * SUBC reg mem
 * v1
 * ------------------------------------------------------------------
 */
void
c16x_subc_reg_mem(uint8_t * icodeP)
{
	uint8_t reg = icodeP[1] & 0xff;
	uint16_t maddr = (icodeP[3] << 8) | icodeP[2];
	uint16_t op1, op2;
	uint16_t result;
	op1 = C16x_ReadReg16(reg);
	op2 = C16x_MemRead16(maddr);
	if (REG_PSW & PSW_FLAG_C) {
		result = op1 - op2 - 1;
	} else {
		result = op1 - op2;
	}
	C16x_SetReg16(result, reg);
	REG_PSW &= ~(PSW_FLAG_E | PSW_FLAG_V | PSW_FLAG_C | PSW_FLAG_N);
	if (result == 0x8000) {
		REG_PSW |= PSW_FLAG_E;
	}
	if (result != 0) {
		REG_PSW &= ~PSW_FLAG_Z;
	}
	if (result & 0x8000) {
		REG_PSW |= PSW_FLAG_N;
	}
	if (sub_carry(op1, op2, result)) {
		REG_PSW |= PSW_FLAG_C;
	}
	if (sub_overflow(op1, op2, result)) {
		REG_PSW |= PSW_FLAG_V;
	}
}

/*
 * -----------------------------------------------
 * subcb reg mem
 * v1
 * -----------------------------------------------
 */
void
c16x_subcb_reg_mem(uint8_t * icodeP)
{
	uint8_t reg = icodeP[1] & 0xff;
	uint16_t maddr = (icodeP[3] << 8) | icodeP[2];
	uint8_t op1, op2;
	uint8_t result;
	op1 = C16x_ReadReg8(reg);
	op2 = C16x_MemRead8(maddr);
	if (REG_PSW & PSW_FLAG_C) {
		result = op1 - op2 - 1;
	} else {
		result = op1 - op2;
	}
	C16x_SetReg8(result, reg);
	REG_PSW &= ~(PSW_FLAG_E | PSW_FLAG_V | PSW_FLAG_C | PSW_FLAG_N);
	if (result == 0x80) {
		REG_PSW |= PSW_FLAG_E;
	}
	if (result != 0) {
		REG_PSW &= ~PSW_FLAG_Z;
	}
	if (result & 0x80) {
		REG_PSW |= PSW_FLAG_N;
	}
	if (sub_carry_b(op1, op2, result)) {
		REG_PSW |= PSW_FLAG_C;
	}
	if (sub_overflow_b(op1, op2, result)) {
		REG_PSW |= PSW_FLAG_V;
	}
}

/*
 * -------------------------------------------------------------
 * SUBC mem reg
 * v1
 * -------------------------------------------------------------
 */
void
c16x_subc_mem_reg(uint8_t * icodeP)
{
	uint8_t reg = icodeP[1] & 0xff;
	uint16_t maddr = (icodeP[3] << 8) | icodeP[2];
	uint16_t op1, op2;
	uint16_t result;
	op1 = C16x_MemRead16(maddr);
	op2 = C16x_ReadReg16(reg);
	if (REG_PSW & PSW_FLAG_C) {
		result = op1 - op2 - 1;
	} else {
		result = op1 - op2;
	}
	C16x_MemWrite16(result, maddr);
	REG_PSW &= ~(PSW_FLAG_E | PSW_FLAG_V | PSW_FLAG_C | PSW_FLAG_N);
	if (result == 0x8000) {
		REG_PSW |= PSW_FLAG_E;
	}
	if (result != 0) {
		REG_PSW &= ~PSW_FLAG_Z;
	}
	if (result & 0x8000) {
		REG_PSW |= PSW_FLAG_N;
	}
	if (sub_carry(op1, op2, result)) {
		REG_PSW |= PSW_FLAG_C;
	}
	if (sub_overflow(op1, op2, result)) {
		REG_PSW |= PSW_FLAG_V;
	}
}

/*
 * ------------------------------------------------------
 * SUBCB mem reg
 * v1
 * ------------------------------------------------------
 */
void
c16x_subcb_mem_reg(uint8_t * icodeP)
{
	uint8_t reg = icodeP[1] & 0xff;
	uint16_t maddr = (icodeP[3] << 8) | icodeP[2];
	uint8_t op1, op2;
	uint8_t result;
	op1 = C16x_MemRead8(maddr);
	op2 = C16x_ReadReg8(reg);
	if (REG_PSW & PSW_FLAG_C) {
		result = op1 - op2 - 1;
	} else {
		result = op1 - op2;
	}
	C16x_MemWrite8(result, maddr);
	REG_PSW &= ~(PSW_FLAG_E | PSW_FLAG_V | PSW_FLAG_C | PSW_FLAG_N);
	if (result == 0x80) {
		REG_PSW |= PSW_FLAG_E;
	} else if (result == 0) {
		REG_PSW |= PSW_FLAG_Z;
	}
	if (result & 0x80) {
		REG_PSW |= PSW_FLAG_N;
	}
	if (sub_carry_b(op1, op2, result)) {
		REG_PSW |= PSW_FLAG_C;
	}
	if (sub_overflow_b(op1, op2, result)) {
		REG_PSW |= PSW_FLAG_V;
	}
}

/*
 * ---------------------------------------------
 * SUBC reg data16
 * ---------------------------------------------
 */
void
c16x_subc_reg_data16(uint8_t * icodeP)
{
	uint8_t reg = icodeP[1] & 0xff;
	uint16_t data16 = (icodeP[3] << 8) | icodeP[2];
	uint16_t op1, op2;
	uint16_t result;
	op1 = C16x_ReadReg16(reg);
	op2 = data16;
	if (REG_PSW & PSW_FLAG_C) {
		result = op1 - op2 - 1;
	} else {
		result = op1 - op2;
	}
	C16x_SetReg16(result, reg);
	REG_PSW &= ~(PSW_FLAG_E | PSW_FLAG_V | PSW_FLAG_C | PSW_FLAG_N);
	if (result == 0x8000) {
		REG_PSW |= PSW_FLAG_E;
	}
	if (result != 0) {
		REG_PSW &= ~PSW_FLAG_Z;
	}
	if (result & 0x8000) {
		REG_PSW |= PSW_FLAG_N;
	}
	if (sub_carry(op1, op2, result)) {
		REG_PSW |= PSW_FLAG_C;
	}
	if (sub_overflow(op1, op2, result)) {
		REG_PSW |= PSW_FLAG_V;
	}
}

/*
 * ------------------------------------------------------
 * SUBCB reg data8
 * v1
 * ------------------------------------------------------
 */
void
c16x_subcb_reg_data8(uint8_t * icodeP)
{
	uint8_t reg = icodeP[1] & 0xff;
	uint8_t data8 = icodeP[2];
	uint8_t op1, op2;
	uint8_t result;
	op1 = C16x_ReadReg8(reg);
	op2 = data8;
	if (REG_PSW & PSW_FLAG_C) {
		result = op1 - op2 - 1;
	} else {
		result = op1 - op2;
	}
	C16x_SetReg8(result, reg);
	REG_PSW &= ~(PSW_FLAG_E | PSW_FLAG_V | PSW_FLAG_C | PSW_FLAG_N);
	if (result == 0x80) {
		REG_PSW |= PSW_FLAG_E;
	}
	if (result != 0) {
		REG_PSW &= ~PSW_FLAG_Z;
	}
	if (result & 0x80) {
		REG_PSW |= PSW_FLAG_N;
	}
	if (sub_carry_b(op1, op2, result)) {
		REG_PSW |= PSW_FLAG_C;
	}
	if (sub_overflow_b(op1, op2, result)) {
		REG_PSW |= PSW_FLAG_V;
	}
}

/*
 * ---------------------------------------------------
 * SUBC rw x
 * v1
 * ---------------------------------------------------
 */
void
c16x_subc_rw_x(uint8_t * icodeP)
{
	int n = (icodeP[1] & 0xf0) >> 4;
	uint16_t addr;
	int ri;
	uint16_t op1, op2;
	uint16_t result = 0;
	int subcmd = (icodeP[1] & 0x0c) >> 2;
	op1 = C16x_ReadGpr16(n);
	switch (subcmd) {
	    case 0:
	    case 1:
		    op2 = icodeP[1] & 0x7;
		    result = op1 - op2;
		    if (REG_PSW & PSW_FLAG_C) {
			    result--;
		    }
		    break;

	    case 2:
		    ri = icodeP[1] & 0x3;
		    addr = C16x_ReadGpr16(ri);
		    op2 = C16x_MemRead16(addr);
		    result = op1 - op2;
		    if (REG_PSW & PSW_FLAG_C) {
			    result--;
		    }
		    break;

	    case 3:
		    ri = icodeP[1] & 0x3;
		    addr = C16x_ReadGpr16(ri);
		    op2 = C16x_MemRead16(addr);
		    result = op1 - op2;
		    if (REG_PSW & PSW_FLAG_C) {
			    result--;
		    }
		    C16x_SetGpr16(addr + 2, ri);
		    break;
	    default:
		    fprintf(stderr, "reached unreachable code\n");
		    return;
	}
	C16x_SetGpr16(result, n);
	REG_PSW &= ~(PSW_FLAG_E | PSW_FLAG_V | PSW_FLAG_C | PSW_FLAG_N);
	if (result == 0x8000) {
		REG_PSW |= PSW_FLAG_E;
	}
	if (result != 0) {
		REG_PSW &= ~PSW_FLAG_Z;
	}
	if (result & 0x8000) {
		REG_PSW |= PSW_FLAG_N;
	}
	if (sub_carry(op1, op2, result)) {
		REG_PSW |= PSW_FLAG_C;
	}
	if (sub_overflow(op1, op2, result)) {
		REG_PSW |= PSW_FLAG_V;
	}
}

/*
 * ----------------------------------------------------------
 * SUBCB rb x
 * v1
 * ----------------------------------------------------------
 */
void
c16x_subcb_rb_x(uint8_t * icodeP)
{
	int n = (icodeP[1] & 0xf0) >> 4;
	uint16_t addr;
	int ri;
	uint8_t op1, op2;
	uint8_t result = 0;
	int subcmd = (icodeP[1] & 0x0c) >> 2;
	op1 = C16x_ReadGpr8(n);
	switch (subcmd) {
	    case 0:
	    case 1:
		    op2 = icodeP[1] & 0x7;
		    result = op1 - op2;
		    if (REG_PSW & PSW_FLAG_C) {
			    result--;
		    }
		    break;

	    case 2:
		    ri = icodeP[1] & 0x3;
		    addr = C16x_ReadGpr16(ri);
		    op2 = C16x_MemRead8(addr);
		    result = op1 - op2;
		    if (REG_PSW & PSW_FLAG_C) {
			    result--;
		    }
		    break;

	    case 3:
		    ri = icodeP[1] & 0x3;
		    addr = C16x_ReadGpr16(ri);
		    op2 = C16x_MemRead8(addr);
		    result = op1 - op2;
		    if (REG_PSW & PSW_FLAG_C) {
			    result--;
		    }
		    C16x_SetGpr16(addr + 2, ri);
		    break;
	    default:
		    fprintf(stderr, "reached unreachable code\n");
		    return;
	}
	C16x_SetGpr8(result, n);
	REG_PSW &= ~(PSW_FLAG_E | PSW_FLAG_V | PSW_FLAG_C | PSW_FLAG_N);
	if (result == 0x80) {
		REG_PSW |= PSW_FLAG_E;
	}
	if (result != 0) {
		REG_PSW &= ~PSW_FLAG_Z;
	}
	if (result & 0x80) {
		REG_PSW |= PSW_FLAG_N;
	}
	if (sub_carry_b(op1, op2, result)) {
		REG_PSW |= PSW_FLAG_C;
	}
	if (sub_overflow_b(op1, op2, result)) {
		REG_PSW |= PSW_FLAG_V;
	}
}

/*
 * --------------------------------------------------
 * BMOVN bitaddr bitaddr
 * v1
 * --------------------------------------------------
 */
void
c16x_bmovn_bitaddr_bitaddr(uint8_t * icodeP)
{
	uint16_t qqaddr = icodeP[1];
	uint16_t qbit = (icodeP[3] & 0xf0) >> 4;
	uint16_t zzaddr = icodeP[2];
	uint16_t zbit = icodeP[3] & 0xf;
	uint16_t op1 = C16x_ReadBitaddr(qqaddr);
	uint16_t op2 = C16x_ReadBitaddr(zzaddr);
	uint16_t result;
	int bit2 = (op2 >> zbit) & 1;
	REG_PSW &= ~(PSW_FLAG_E | PSW_FLAG_Z | PSW_FLAG_V | PSW_FLAG_C | PSW_FLAG_N);
	if (bit2) {
		REG_PSW |= PSW_FLAG_N;
		result = op1 & ~(1 << qbit);
	} else {
		REG_PSW |= PSW_FLAG_Z;
		result = op1 | (1 << qbit);
	}
	C16x_WriteBitaddr(result, qqaddr);
}

/*
 * -----------------------------------------------------------------
 * ROR rw data 4
 * v1
 * -----------------------------------------------------------------
 */
void
c16x_ror_rw_data4(uint8_t * icodeP)
{
	uint16_t rot = (icodeP[1] & 0xf0) >> 4;
	int n = (icodeP[1] & 0xf);
	uint16_t op1;
	uint16_t result;
	op1 = C16x_ReadGpr16(n);
	REG_PSW &= ~(PSW_FLAG_E | PSW_FLAG_Z | PSW_FLAG_V | PSW_FLAG_C | PSW_FLAG_N);
	if (rot) {
		result = op1 >> rot;
		if ((result << rot) != op1) {
			REG_PSW |= PSW_FLAG_V;
		}
		result = result | (op1 << (16 - rot));
		if (result & (1 << 15)) {
			REG_PSW |= PSW_FLAG_C;
		}
	} else {
		result = op1;
	}
	C16x_SetGpr16(result, n);
	if (result == 0) {
		REG_PSW |= PSW_FLAG_Z;
	}
	if (result & 0x8000) {
		REG_PSW |= PSW_FLAG_N;
	}
}

/*
 * ------------------------------------------------------
 * CMP rw rw
 * v1
 * ------------------------------------------------------
 */

void
c16x_cmp_rw_rw(uint8_t * icodeP)
{
	int n = (icodeP[1] & 0xf0) >> 4;
	int m = (icodeP[1] & 0xf);
	uint16_t op1, op2;
	uint16_t result;
	op1 = C16x_ReadGpr16(n);
	op2 = C16x_ReadGpr16(m);
	result = op1 - op2;
	REG_PSW &= ~(PSW_FLAG_E | PSW_FLAG_Z | PSW_FLAG_V | PSW_FLAG_C | PSW_FLAG_N);
	if (result == 0x8000) {
		REG_PSW |= PSW_FLAG_E;
	}
	if (result == 0) {
		REG_PSW |= PSW_FLAG_Z;
	}
	if (result & 0x8000) {
		REG_PSW |= PSW_FLAG_N;
	}
	if (sub_carry(op1, op2, result)) {
		REG_PSW |= PSW_FLAG_C;
	}
	if (sub_overflow(op1, op2, result)) {
		REG_PSW |= PSW_FLAG_V;
	}

}

/*
 * --------------------------------------------------------
 * CMPB rb rb
 * v1
 * --------------------------------------------------------
 */
void
c16x_cmpb_rb_rb(uint8_t * icodeP)
{
	int n = (icodeP[1] & 0xf0) >> 4;
	int m = (icodeP[1] & 0xf);
	uint8_t op1, op2;
	uint8_t result;
	op1 = C16x_ReadGpr8(n);
	op2 = C16x_ReadGpr8(m);
	result = op1 - op2;

	REG_PSW &= ~(PSW_FLAG_E | PSW_FLAG_Z | PSW_FLAG_V | PSW_FLAG_C | PSW_FLAG_N);
	if (result == 0x80) {
		REG_PSW |= PSW_FLAG_E;
	}
	if (result == 0) {
		REG_PSW |= PSW_FLAG_Z;
	}
	if (result & 0x80) {
		REG_PSW |= PSW_FLAG_N;
	}
	if (sub_carry_b(op1, op2, result)) {
		REG_PSW |= PSW_FLAG_C;
	}
	if (sub_overflow_b(op1, op2, result)) {
		REG_PSW |= PSW_FLAG_V;
	}
}

/*
 * --------------------------------------
 * CMP reg mem
 * v1
 * --------------------------------------
 */
void
c16x_cmp_reg_mem(uint8_t * icodeP)
{
	uint8_t reg = icodeP[1] & 0xff;
	uint16_t maddr = (icodeP[3] << 8) | icodeP[2];
	uint16_t op1, op2;
	uint16_t result;
	op1 = C16x_ReadReg16(reg);
	op2 = C16x_MemRead16(maddr);
	result = op1 - op2;
	REG_PSW &= ~(PSW_FLAG_E | PSW_FLAG_Z | PSW_FLAG_V | PSW_FLAG_C | PSW_FLAG_N);
	if (result == 0x8000) {
		REG_PSW |= PSW_FLAG_E;
	}
	if (result == 0) {
		REG_PSW |= PSW_FLAG_Z;
	}
	if (result & 0x8000) {
		REG_PSW |= PSW_FLAG_N;
	}
	if (sub_carry(op1, op2, result)) {
		REG_PSW |= PSW_FLAG_C;
	}
	if (sub_overflow(op1, op2, result)) {
		REG_PSW |= PSW_FLAG_V;
	}
}

/*
 * ------------------------------------------------------------
 * CMPB reg mem
 * v1
 * ------------------------------------------------------------
 */
void
c16x_cmpb_reg_mem(uint8_t * icodeP)
{
	uint8_t reg = icodeP[1] & 0xff;
	uint16_t maddr = (icodeP[3] << 8) | icodeP[2];
	uint8_t op1, op2;
	uint8_t result;
	op1 = C16x_ReadReg8(reg);
	op2 = C16x_MemRead8(maddr);
	result = op1 - op2;

	REG_PSW &= ~(PSW_FLAG_E | PSW_FLAG_Z | PSW_FLAG_V | PSW_FLAG_C | PSW_FLAG_N);
	if (result == 0x80) {
		REG_PSW |= PSW_FLAG_E;
	}
	if (result == 0) {
		REG_PSW |= PSW_FLAG_Z;
	}
	if (result & 0x80) {
		REG_PSW |= PSW_FLAG_N;
	}
	if (sub_carry_b(op1, op2, result)) {
		REG_PSW |= PSW_FLAG_C;
	}
	if (sub_overflow_b(op1, op2, result)) {
		REG_PSW |= PSW_FLAG_V;
	}
}

/*
 * ---------------------------------------
 * CMP reg data16
 * v1
 * ---------------------------------------
 */
void
c16x_cmp_reg_data16(uint8_t * icodeP)
{
	uint8_t reg = icodeP[1] & 0xff;
	uint16_t data16 = (icodeP[3] << 8) | icodeP[2];
	uint16_t op1, op2;
	uint16_t result;
	op1 = C16x_ReadReg16(reg);
	op2 = data16;
	result = op1 - op2;

	REG_PSW &= ~(PSW_FLAG_E | PSW_FLAG_Z | PSW_FLAG_V | PSW_FLAG_C | PSW_FLAG_N);
	if (result == 0x8000) {
		REG_PSW |= PSW_FLAG_E;
	}
	if (result == 0) {
		REG_PSW |= PSW_FLAG_Z;
	}
	if (result & 0x8000) {
		REG_PSW |= PSW_FLAG_N;
	}
	if (sub_carry(op1, op2, result)) {
		REG_PSW |= PSW_FLAG_C;
	}
	if (sub_overflow(op1, op2, result)) {
		REG_PSW |= PSW_FLAG_V;
	}
}

/*
 * --------------------------------------------------------
 * CMPB reg data8
 * v1	 
 * --------------------------------------------------------
 */
void
c16x_cmpb_reg_data8(uint8_t * icodeP)
{
	uint8_t reg = icodeP[1] & 0xff;
	uint8_t data8 = icodeP[2];
	uint8_t op1, op2;
	uint8_t result;
	op1 = C16x_ReadReg8(reg);
	op2 = data8;
	result = op1 - op2;

	REG_PSW &= ~(PSW_FLAG_E | PSW_FLAG_Z | PSW_FLAG_V | PSW_FLAG_C | PSW_FLAG_N);
	if (result == 0x80) {
		REG_PSW |= PSW_FLAG_E;
	}
	if (result == 0) {
		REG_PSW |= PSW_FLAG_Z;
	}
	if (result & 0x80) {
		REG_PSW |= PSW_FLAG_N;
	}
	if (sub_carry_b(op1, op2, result)) {
		REG_PSW |= PSW_FLAG_C;
	}
	if (sub_overflow_b(op1, op2, result)) {
		REG_PSW |= PSW_FLAG_V;
	}
}

/*
 * ------------------------------------------------------
 * CMP rw x
 * v1
 * ------------------------------------------------------
 */

void
c16x_cmp_rw_x(uint8_t * icodeP)
{
	int n = (icodeP[1] & 0xf0) >> 4;
	uint16_t addr;
	int ri;
	uint16_t op1, op2;
	uint16_t result;
	int subcmd = (icodeP[1] & 0x0c) >> 2;
	op1 = C16x_ReadGpr16(n);
	switch (subcmd) {
	    case 0:
	    case 1:
		    op2 = icodeP[1] & 0x7;
		    result = op1 - op2;
		    break;

	    case 2:
		    ri = icodeP[1] & 0x3;
		    addr = C16x_ReadGpr16(ri);
		    op2 = C16x_MemRead16(addr);
		    result = op1 - op2;
		    break;

	    case 3:
		    ri = icodeP[1] & 0x3;
		    addr = C16x_ReadGpr16(ri);
		    op2 = C16x_MemRead16(addr);
		    result = op1 - op2;
		    C16x_SetGpr16(addr + 2, ri);
		    break;
	    default:
		    return;
	}
	REG_PSW &= ~(PSW_FLAG_E | PSW_FLAG_Z | PSW_FLAG_V | PSW_FLAG_C | PSW_FLAG_N);
	if (result == 0x8000) {
		REG_PSW |= PSW_FLAG_E;
	} else if (result == 0) {
		REG_PSW |= PSW_FLAG_Z;
	}
	if (result & 0x8000) {
		REG_PSW |= PSW_FLAG_N;
	}
	if (sub_carry(op1, op2, result)) {
		REG_PSW |= PSW_FLAG_C;
	}
	if (sub_overflow(op1, op2, result)) {
		REG_PSW |= PSW_FLAG_V;
	}
}

/*
 * -----------------------------------------------
 * CMPB rb x
 * v1
 * -----------------------------------------------
 */
void
c16x_cmpb_rb_x(uint8_t * icodeP)
{
	int n = (icodeP[1] & 0xf0) >> 4;
	uint16_t addr;
	int ri;
	uint8_t op1, op2;
	uint8_t result;
	int subcmd = (icodeP[1] & 0x0c) >> 2;
	op1 = C16x_ReadGpr8(n);
	switch (subcmd) {
	    case 0:
	    case 1:
		    op2 = icodeP[1] & 0x7;
		    result = op1 - op2;
		    break;

	    case 2:
		    ri = icodeP[1] & 0x3;
		    addr = C16x_ReadGpr16(ri);
		    op2 = C16x_MemRead8(addr);
		    result = op1 - op2;
		    break;

	    case 3:
		    ri = icodeP[1] & 0x3;
		    addr = C16x_ReadGpr16(ri);
		    op2 = C16x_MemRead8(addr);
		    result = op1 - op2;
		    C16x_SetGpr16(addr + 2, ri);
		    break;
	    default:
		    return;
	}
	REG_PSW &= ~(PSW_FLAG_E | PSW_FLAG_Z | PSW_FLAG_V | PSW_FLAG_C | PSW_FLAG_N);
	if (result == 0x80) {
		REG_PSW |= PSW_FLAG_E;
	}
	if (result == 0) {
		REG_PSW |= PSW_FLAG_Z;
	}
	if (result & 0x80) {
		REG_PSW |= PSW_FLAG_N;
	}
	if (sub_carry_b(op1, op2, result)) {
		REG_PSW |= PSW_FLAG_C;
	}
	if (sub_overflow_b(op1, op2, result)) {
		REG_PSW |= PSW_FLAG_V;
	}
}

/*
 * ------------------------------------------------
 * BMOV
 * v1
 * ------------------------------------------------
 */
void
c16x_bmov_bitaddr_bitaddr(uint8_t * icodeP)
{
	uint16_t qqaddr = icodeP[1];
	uint16_t qbit = (icodeP[3] & 0xf0) >> 4;
	uint16_t zzaddr = icodeP[2];
	uint16_t zbit = icodeP[3] & 0xf;
	uint16_t op1 = C16x_ReadBitaddr(qqaddr);
	uint16_t op2 = C16x_ReadBitaddr(zzaddr);
	uint16_t result;
	int bit2 = (op2 >> zbit) & 1;
	REG_PSW &= ~(PSW_FLAG_E | PSW_FLAG_Z | PSW_FLAG_V | PSW_FLAG_C | PSW_FLAG_N);
	if (bit2) {
		REG_PSW |= PSW_FLAG_N;
		result = op1 | (1 << qbit);
	} else {
		REG_PSW |= PSW_FLAG_Z;
		result = op1 & ~(1 << qbit);
	}
	C16x_WriteBitaddr(result, qqaddr);
}

/*
 * -------------------------------------------------------------------
 * DIV:
 * 	Warning: Instruction Set manual is broken. Hopefully I
 * 	guessed the DIV instruction correctly
 * v0
 * -------------------------------------------------------------------
 */

void
c16x_div_rw(uint8_t * icodeP)
{
	int n = icodeP[1] & 0xf;
	int16_t op1 = C16x_ReadGpr16(n);
	int16_t mdl = REG_MDL, mdh = REG_MDH;
	REG_PSW &= ~(PSW_FLAG_E | PSW_FLAG_Z | PSW_FLAG_V | PSW_FLAG_C | PSW_FLAG_N);
	REG_MDC |= MDC_MDRIU;
	if (op1) {
		mdh = REG_MDH = mdl % op1;
		mdl = REG_MDL = mdl / op1;
		REG_MDL = mdl;
		REG_MDH = mdh;
	} else {
		REG_PSW |= PSW_FLAG_V;
	}
	if (mdl == 0) {
		REG_PSW |= PSW_FLAG_Z;
	}
	if (mdl & 0x8000) {
		REG_PSW |= PSW_FLAG_N;
	}
	fprintf(stderr, "Warning: div instruction Register specification unclear in ISM\n");
}

/*
 * -------------------------------------------------------------------
 * SHL
 *  v1
 * -------------------------------------------------------------------
 */

void
c16x_shl_rw_rw(uint8_t * icodeP)
{
	int n = (icodeP[1] & 0xf0) >> 4;
	int m = (icodeP[1] & 0xf);
	uint16_t op1, op2;
	uint16_t result;
	op1 = C16x_ReadGpr16(n);
	op2 = C16x_ReadGpr16(m) & 0xf;
	REG_PSW &= ~(PSW_FLAG_E | PSW_FLAG_Z | PSW_FLAG_V | PSW_FLAG_C | PSW_FLAG_N);
	if (op2) {
		result = op1 << op2;
		if (op1 && (1 << (op2 - 1))) {
			REG_PSW |= PSW_FLAG_C;
		}
	} else {
		result = op1;
	}
	C16x_SetGpr16(result, n);

	if (result == 0) {
		REG_PSW |= PSW_FLAG_Z;
	}
	if (result & 0x8000) {
		REG_PSW |= PSW_FLAG_N;
	}
}

/*
 * -----------------------------------------------
 * XOR rw rw
 * v1
 * -----------------------------------------------
 */
void
c16x_xor_rw_rw(uint8_t * icodeP)
{
	int n = (icodeP[1] & 0xf0) >> 4;
	int m = (icodeP[1] & 0xf);
	uint16_t op1, op2;
	uint16_t result;
	op1 = C16x_ReadGpr16(n);
	op2 = C16x_ReadGpr16(m);
	result = op1 ^ op2;
	C16x_SetGpr16(result, n);

	REG_PSW &= ~(PSW_FLAG_E | PSW_FLAG_Z | PSW_FLAG_V | PSW_FLAG_C | PSW_FLAG_N);
	if (result == 0x8000) {
		REG_PSW |= PSW_FLAG_E;
	}
	if (result == 0) {
		REG_PSW |= PSW_FLAG_Z;
	}
	if (result & 0x8000) {
		REG_PSW |= PSW_FLAG_N;
	}
}

/*
 * ------------------------------------------------
 * XORB rb rb
 * v1
 * ------------------------------------------------
 */
void
c16x_xorb_rb_rb(uint8_t * icodeP)
{
	int n = (icodeP[1] & 0xf0) >> 4;
	int m = (icodeP[1] & 0xf);
	uint8_t op1, op2;
	uint8_t result;
	op1 = C16x_ReadGpr8(n);
	op2 = C16x_ReadGpr8(m);
	result = op1 ^ op2;
	C16x_SetGpr8(result, n);

	REG_PSW &= ~(PSW_FLAG_E | PSW_FLAG_Z | PSW_FLAG_V | PSW_FLAG_C | PSW_FLAG_N);
	if (result == 0x80) {
		REG_PSW |= PSW_FLAG_E;
	}
	if (result == 0) {
		REG_PSW |= PSW_FLAG_Z;
	}
	if (result & 0x80) {
		REG_PSW |= PSW_FLAG_N;
	}
}

/*
 * ---------------------------------------------------
 * XOR reg mem
 * v1
 * ---------------------------------------------------
 */
void
c16x_xor_reg_mem(uint8_t * icodeP)
{
	uint8_t reg = icodeP[1] & 0xff;
	uint16_t maddr = (icodeP[3] << 8) | icodeP[2];
	uint16_t op1, op2;
	uint16_t result;
	op1 = C16x_ReadReg16(reg);
	op2 = C16x_MemRead16(maddr);
	result = op1 ^ op2;
	C16x_SetReg16(result, reg);

	REG_PSW &= ~(PSW_FLAG_E | PSW_FLAG_Z | PSW_FLAG_V | PSW_FLAG_C | PSW_FLAG_N);
	if (result == 0x8000) {
		REG_PSW |= PSW_FLAG_E;
	}
	if (result == 0) {
		REG_PSW |= PSW_FLAG_Z;
	}
	if (result & 0x8000) {
		REG_PSW |= PSW_FLAG_N;
	}
}

/*
 * ---------------------------------------------------
 * XORB reg mem
 * v1
 * ---------------------------------------------------
 */
void
c16x_xorb_reg_mem(uint8_t * icodeP)
{
	uint8_t reg = icodeP[1] & 0xff;
	uint16_t maddr = (icodeP[3] << 8) | icodeP[2];
	uint8_t op1, op2;
	uint8_t result;
	op1 = C16x_ReadReg8(reg);
	op2 = C16x_MemRead8(maddr);
	result = op1 ^ op2;
	C16x_SetReg8(result, reg);

	REG_PSW &= ~(PSW_FLAG_E | PSW_FLAG_Z | PSW_FLAG_V | PSW_FLAG_C | PSW_FLAG_N);
	if (result == 0x80) {
		REG_PSW |= PSW_FLAG_E;
	}
	if (result == 0) {
		REG_PSW |= PSW_FLAG_Z;
	}
	if (result & 0x80) {
		REG_PSW |= PSW_FLAG_N;
	}
}

/*
 * ------------------------------------------------
 * XOR mem reg
 * v0
 * ------------------------------------------------
 */
void
c16x_xor_mem_reg(uint8_t * icodeP)
{
	uint8_t reg = icodeP[1] & 0xff;
	uint16_t maddr = (icodeP[3] << 8) | icodeP[2];
	uint16_t op1, op2;
	uint16_t result;
	op1 = C16x_MemRead16(maddr);
	op2 = C16x_ReadReg16(reg);
	result = op1 ^ op2;
	C16x_MemWrite16(result, maddr);

	REG_PSW &= ~(PSW_FLAG_E | PSW_FLAG_Z | PSW_FLAG_V | PSW_FLAG_C | PSW_FLAG_N);
	if (result == 0x8000) {
		REG_PSW |= PSW_FLAG_E;
	}
	if (result == 0) {
		REG_PSW |= PSW_FLAG_Z;
	}
	if (result & 0x8000) {
		REG_PSW |= PSW_FLAG_N;
	}
}

/*
 * ----------------------------------------------
 * XORB mem reg
 * ----------------------------------------------
 */
void
c16x_xorb_mem_reg(uint8_t * icodeP)
{
	uint8_t reg = icodeP[1] & 0xff;
	uint16_t maddr = (icodeP[3] << 8) | icodeP[2];
	uint8_t op1, op2;
	uint8_t result;
	op1 = C16x_MemRead8(maddr);
	op2 = C16x_ReadReg8(reg);
	result = op1 ^ op2;
	C16x_MemWrite8(result, maddr);

	REG_PSW &= ~(PSW_FLAG_E | PSW_FLAG_Z | PSW_FLAG_V | PSW_FLAG_C | PSW_FLAG_N);
	if (result == 0x80) {
		REG_PSW |= PSW_FLAG_E;
	}
	if (result == 0) {
		REG_PSW |= PSW_FLAG_Z;
	}
	if (result & 0x80) {
		REG_PSW |= PSW_FLAG_N;
	}
}

/*
 * -------------------------------------------------------
 * XOR reg data16
 * -------------------------------------------------------
 */
void
c16x_xor_reg_data16(uint8_t * icodeP)
{
	uint8_t reg = icodeP[1] & 0xff;
	uint16_t data16 = (icodeP[3] << 8) | icodeP[2];
	uint16_t op1, op2;
	uint16_t result;
	op1 = C16x_ReadReg16(reg);
	op2 = data16;
	result = op1 ^ op2;
	C16x_SetReg16(result, reg);

	REG_PSW &= ~(PSW_FLAG_E | PSW_FLAG_Z | PSW_FLAG_V | PSW_FLAG_C | PSW_FLAG_N);
	if (result == 0x8000) {
		REG_PSW |= PSW_FLAG_E;
	}
	if (result == 0) {
		REG_PSW |= PSW_FLAG_Z;
	}
	if (result & 0x8000) {
		REG_PSW |= PSW_FLAG_N;
	}
}

/*
 * ------------------------------------------------------
 * XORB reg data8
 * ------------------------------------------------------
 */
void
c16x_xorb_reg_data8(uint8_t * icodeP)
{
	uint8_t reg = icodeP[1] & 0xff;
	uint8_t data8 = icodeP[2];
	uint8_t op1, op2;
	uint8_t result;
	op1 = C16x_ReadReg8(reg);
	op2 = data8;
	result = op1 ^ op2;
	C16x_SetReg8(result, reg);

	REG_PSW &= ~(PSW_FLAG_E | PSW_FLAG_Z | PSW_FLAG_V | PSW_FLAG_C | PSW_FLAG_N);
	if (result == 0x80) {
		REG_PSW |= PSW_FLAG_E;
	}
	if (result == 0) {
		REG_PSW |= PSW_FLAG_Z;
	}
	if (result & 0x80) {
		REG_PSW |= PSW_FLAG_N;
	}
}

/*
 * ----------------------------------------------------------------------
 * XOR rw x
 * v1
 * ----------------------------------------------------------------------
 */
void
c16x_xor_rw_x(uint8_t * icodeP)
{
	int n = (icodeP[1] & 0xf0) >> 4;
	uint16_t addr;
	int ri;
	uint16_t op1, op2;
	uint16_t result;
	int subcmd = (icodeP[1] & 0x0c) >> 2;
	op1 = C16x_ReadGpr16(n);
	switch (subcmd) {
	    case 0:
	    case 1:
		    op2 = icodeP[1] & 0x7;
		    result = op1 ^ op2;
		    break;

	    case 2:
		    ri = icodeP[1] & 0x3;
		    addr = C16x_ReadGpr16(ri);
		    op2 = C16x_MemRead16(addr);
		    result = op1 ^ op2;
		    break;

	    case 3:
		    ri = icodeP[1] & 0x3;
		    addr = C16x_ReadGpr16(ri);
		    op2 = C16x_MemRead16(addr);
		    result = op1 ^ op2;
		    C16x_SetGpr16(addr + 2, ri);
		    break;
	    default:
		    fprintf(stderr, "reached Unreachable code\n");
		    return;
	}
	C16x_SetGpr16(result, n);
	REG_PSW &= ~(PSW_FLAG_E | PSW_FLAG_Z | PSW_FLAG_V | PSW_FLAG_C | PSW_FLAG_N);
	if (result == 0x8000) {
		REG_PSW |= PSW_FLAG_E;
	}
	if (result == 0) {
		REG_PSW |= PSW_FLAG_Z;
	}
	if (result & 0x8000) {
		REG_PSW |= PSW_FLAG_N;
	}
}

/*
 * --------------------------------------------------
 * XORB rb x
 * --------------------------------------------------
 */
void
c16x_xorb_rb_x(uint8_t * icodeP)
{
	int n = (icodeP[1] & 0xf0) >> 4;
	uint16_t addr;
	int ri;
	uint8_t op1, op2;
	uint8_t result;
	int subcmd = (icodeP[1] & 0x0c) >> 2;
	op1 = C16x_ReadGpr8(n);
	switch (subcmd) {
	    case 0:
	    case 1:
		    op2 = icodeP[1] & 0x7;
		    result = op1 ^ op2;
		    break;

	    case 2:
		    ri = icodeP[1] & 0x3;
		    addr = C16x_ReadGpr16(ri);
		    op2 = C16x_MemRead8(addr);
		    result = op1 ^ op2;
		    break;

	    case 3:
		    ri = icodeP[1] & 0x3;
		    addr = C16x_ReadGpr16(ri);
		    op2 = C16x_MemRead8(addr);
		    result = op1 ^ op2;
		    C16x_SetGpr16(addr + 2, ri);
		    break;
	    default:
		    return;
	}
	C16x_SetGpr8(result, n);
	REG_PSW &= ~(PSW_FLAG_E | PSW_FLAG_Z | PSW_FLAG_V | PSW_FLAG_C | PSW_FLAG_N);
	if (result == 0x80) {
		REG_PSW |= PSW_FLAG_E;
	}
	if (result == 0) {
		REG_PSW |= PSW_FLAG_Z;
	}
	if (result & 0x80) {
		REG_PSW |= PSW_FLAG_N;
	}
}

/*
 * -----------------------------------------------------
 * BOR 
 * v1
 * -----------------------------------------------------
 */
void
c16x_bor_bitaddr_bitaddr(uint8_t * icodeP)
{
	uint16_t qqaddr = icodeP[1];
	uint16_t qbit = (icodeP[3] & 0xf0) >> 4;
	uint16_t zzaddr = icodeP[2];
	uint16_t zbit = icodeP[3] & 0xf;
	uint16_t op1 = C16x_ReadBitaddr(qqaddr);
	uint16_t op2 = C16x_ReadBitaddr(zzaddr);
	uint16_t result;
	int bit1 = (op1 >> qbit) & 1;
	int bit2 = (op2 >> zbit) & 1;
	REG_PSW &= ~(PSW_FLAG_E | PSW_FLAG_Z | PSW_FLAG_V | PSW_FLAG_C | PSW_FLAG_N);
	if (bit1 ^ bit2) {
		REG_PSW = REG_PSW | PSW_FLAG_N;
	}
	if (bit1 & bit2) {
		REG_PSW = REG_PSW | PSW_FLAG_C;
	}
	if (bit1 | bit2) {
		REG_PSW = REG_PSW | PSW_FLAG_V;
		result = op1 | (1 << qbit);
	} else {
		REG_PSW = REG_PSW | PSW_FLAG_Z;
		result = op1 & ~(1 << qbit);
	}
	C16x_WriteBitaddr(result, qqaddr);
}

/*
 * ----------------------------------------------------
 * DIVU
 * v0
 * ----------------------------------------------------
 */
void
c16x_divu_rw(uint8_t * icodeP)
{
	int n = icodeP[1] & 0xf;
	uint16_t op1 = C16x_ReadGpr16(n);
	uint16_t mdl = REG_MDL, mdh = REG_MDH;
	REG_PSW &= ~(PSW_FLAG_E | PSW_FLAG_Z | PSW_FLAG_V | PSW_FLAG_C | PSW_FLAG_N);
	REG_MDC |= MDC_MDRIU;
	if (op1) {
		mdh = REG_MDH = mdl % op1;
		mdl = REG_MDL = mdl / op1;
	} else {
		REG_PSW |= PSW_FLAG_V;
	}
	if (mdl == 0) {
		REG_PSW |= PSW_FLAG_Z;
	}
	if (mdl & 0x8000) {
		REG_PSW |= PSW_FLAG_N;
	}
	fprintf(stderr, "Warning: divu instruction Register specification unclear in ISM\n");
}

/*
 * ------------------------------------------------------------------
 * SHL rw data 4
 * v1
 * ------------------------------------------------------------------
 */
void
c16x_shl_rw_data4(uint8_t * icodeP)
{
	uint16_t shift = (icodeP[1] & 0xf0) >> 4;
	int n = (icodeP[1] & 0xf);
	uint16_t op1 = C16x_ReadGpr16(n);
	uint16_t result;
	REG_PSW &= ~(PSW_FLAG_C | PSW_FLAG_V | PSW_FLAG_Z | PSW_FLAG_N | PSW_FLAG_E);
	result = (op1 << shift);
	if (shift) {
		if (op1 && (1 << (16 - shift))) {
			REG_PSW |= PSW_FLAG_C;
		}
	}
	if (result == 0) {
		REG_PSW |= PSW_FLAG_Z;
	}
	if (result & 0x8000) {
		REG_PSW |= PSW_FLAG_N;
	}
	C16x_SetGpr16(result, n);
}

/*
 * --------------------------------------------------------------
 * AND rw rw
 * v0
 * --------------------------------------------------------------
 */

void
c16x_and_rw_rw(uint8_t * icodeP)
{
	int n = (icodeP[1] & 0xf0) >> 4;
	int m = (icodeP[1] & 0xf);
	uint16_t op1, op2;
	uint16_t result;
	op1 = C16x_ReadGpr16(n);
	op2 = C16x_ReadGpr16(m);
	result = op1 & op2;
	REG_PSW &= ~(PSW_FLAG_C | PSW_FLAG_V | PSW_FLAG_Z | PSW_FLAG_N | PSW_FLAG_E);
	C16x_SetGpr16(result, n);
	if (result == 0x8000) {
		REG_PSW |= PSW_FLAG_E;
	}
	if (result == 0) {
		REG_PSW |= PSW_FLAG_Z;
	}
	if (result & 0x8000) {
		REG_PSW |= PSW_FLAG_N;
	}
}

/*
 * -------------------------------------------------
 * ANDB rb rb
 * v0
 * -------------------------------------------------
 */
void
c16x_andb_rb_rb(uint8_t * icodeP)
{
	int n = (icodeP[1] & 0xf0) >> 4;
	int m = (icodeP[1] & 0xf);
	uint8_t op1, op2;
	uint8_t result;
	op1 = C16x_ReadGpr8(n);
	op2 = C16x_ReadGpr8(m);
	result = op1 & op2;
	REG_PSW &= ~(PSW_FLAG_E | PSW_FLAG_Z | PSW_FLAG_V | PSW_FLAG_C | PSW_FLAG_N);
	C16x_SetGpr8(result, n);
	if (result == 0x80) {
		REG_PSW |= PSW_FLAG_E;
	}
	if (result == 0) {
		REG_PSW |= PSW_FLAG_Z;
	}
	if (result & 0x80) {
		REG_PSW |= PSW_FLAG_N;
	}
}

/*
 * ----------------------------------------
 * AND reg mem
 * v0
 * ----------------------------------------
 */
void
c16x_and_reg_mem(uint8_t * icodeP)
{
	uint8_t reg = icodeP[1] & 0xff;
	uint16_t maddr = (icodeP[3] << 8) | icodeP[2];
	uint16_t op1, op2;
	uint16_t result;
	op1 = C16x_ReadReg16(reg);
	op2 = C16x_MemRead16(maddr);
	result = op1 & op2;
	C16x_SetReg16(result, reg);
	REG_PSW &= ~(PSW_FLAG_E | PSW_FLAG_Z | PSW_FLAG_V | PSW_FLAG_C | PSW_FLAG_N);
	if (result == 0x8000) {
		REG_PSW |= PSW_FLAG_E;
	}
	if (result == 0) {
		REG_PSW |= PSW_FLAG_Z;
	}
	if (result & 0x8000) {
		REG_PSW |= PSW_FLAG_N;
	}
}

/*
 * -----------------------------------------------
 * ANDB reg mem
 * -----------------------------------------------
 */
void
c16x_andb_reg_mem(uint8_t * icodeP)
{
	uint8_t reg = icodeP[1] & 0xff;
	uint16_t maddr = ((uint16_t) icodeP[3] << 8) | icodeP[2];
	uint8_t op1, op2;
	uint8_t result;
	op1 = C16x_ReadReg8(reg);
	op2 = C16x_MemRead8(maddr);
	result = op1 & op2;
	C16x_SetReg8(result, reg);
	REG_PSW &= ~(PSW_FLAG_E | PSW_FLAG_Z | PSW_FLAG_V | PSW_FLAG_C | PSW_FLAG_N);
	if (result == 0x80) {
		REG_PSW |= PSW_FLAG_E;
	}
	if (result == 0) {
		REG_PSW |= PSW_FLAG_Z;
	}
	if (result & 0x80) {
		REG_PSW |= PSW_FLAG_N;
	}
}

/*
 * -----------------------------------------------------------------------
 * AND mem reg
 * v0
 * -----------------------------------------------------------------------
 */
void
c16x_and_mem_reg(uint8_t * icodeP)
{
	uint8_t reg = icodeP[1] & 0xff;
	uint16_t maddr = (icodeP[3] << 8) | icodeP[2];
	uint16_t op1, op2;
	uint16_t result;
	op1 = C16x_MemRead16(maddr);
	op2 = C16x_ReadReg16(reg);
	result = op1 & op2;
	C16x_MemWrite16(result, maddr);
	REG_PSW &= ~(PSW_FLAG_E | PSW_FLAG_Z | PSW_FLAG_V | PSW_FLAG_C | PSW_FLAG_N);
	if (result == 0x8000) {
		REG_PSW |= PSW_FLAG_E;
	}
	if (result == 0) {
		REG_PSW |= PSW_FLAG_Z;
	}
	if (result & 0x8000) {
		REG_PSW |= PSW_FLAG_N;
	}
}

/*
 * ANDB mem reg
 */
void
c16x_andb_mem_reg(uint8_t * icodeP)
{
	uint8_t reg = icodeP[1] & 0xff;
	uint16_t maddr = (icodeP[3] << 8) | icodeP[2];
	uint16_t op1, op2;
	uint8_t result;
	op1 = C16x_MemRead8(maddr);
	op2 = C16x_ReadReg8(reg);
	result = op1 & op2;
	C16x_MemWrite8(result, maddr);
	REG_PSW &= ~(PSW_FLAG_E | PSW_FLAG_Z | PSW_FLAG_V | PSW_FLAG_C | PSW_FLAG_N);
	if (result == 0x80) {
		REG_PSW |= PSW_FLAG_E;
	}
	if (result == 0) {
		REG_PSW |= PSW_FLAG_Z;
	}
	if (result & 0x80) {
		REG_PSW |= PSW_FLAG_N;
	}
}

/* 
 * -------------------------------------------------------------------
 * AND reg data16
 * v0
 * -------------------------------------------------------------------
 */
void
c16x_and_reg_data16(uint8_t * icodeP)
{
	uint8_t reg = icodeP[1] & 0xff;
	uint16_t data16 = ((uint16_t) icodeP[3] << 8) | icodeP[2];
	uint16_t op1, op2;
	uint16_t result;
	op1 = C16x_ReadReg16(reg);
	op2 = data16;
	result = op1 & op2;
	C16x_SetReg16(result, reg);
	REG_PSW &= ~(PSW_FLAG_E | PSW_FLAG_Z | PSW_FLAG_V | PSW_FLAG_C | PSW_FLAG_N);
	if (result == 0x8000) {
		REG_PSW |= PSW_FLAG_E;
	}
	if (result == 0) {
		REG_PSW |= PSW_FLAG_Z;
	}
	if (result & 0x8000) {
		REG_PSW |= PSW_FLAG_N;
	}
}

/*
 * ----------------------------------------------------------------
 * ANDB reg data8
 * ----------------------------------------------------------------
 */
void
c16x_andb_reg_data8(uint8_t * icodeP)
{
	uint8_t reg = icodeP[1] & 0xff;
	uint8_t data8 = icodeP[2];
	uint8_t op1, op2;
	uint8_t result;
	op1 = C16x_ReadReg8(reg);
	op2 = data8;
	result = op1 & op2;
	C16x_SetReg8(result, reg);
	REG_PSW &= ~(PSW_FLAG_E | PSW_FLAG_Z | PSW_FLAG_V | PSW_FLAG_C | PSW_FLAG_N);
	if (result == 0x80) {
		REG_PSW |= PSW_FLAG_E;
	}
	if (result == 0) {
		REG_PSW |= PSW_FLAG_Z;
	}
	if (result & 0x80) {
		REG_PSW |= PSW_FLAG_N;
	}
}

/*
 * --------------------------------------------------
 * AND rw x
 * v0
 * --------------------------------------------------
 */
void
c16x_and_rw_x(uint8_t * icodeP)
{
	int n = (icodeP[1] & 0xf0) >> 4;
	uint16_t addr;
	int ri;
	uint16_t op1, op2;
	uint16_t result;
	int subcmd = (icodeP[1] & 0x0c) >> 2;
	op1 = C16x_ReadGpr16(n);
	switch (subcmd) {
	    case 0:
	    case 1:
		    op2 = icodeP[1] & 0x7;
		    result = op1 & op2;
		    break;

	    case 2:
		    ri = icodeP[1] & 0x3;
		    addr = C16x_ReadGpr16(ri);
		    op2 = C16x_MemRead16(addr);
		    result = op1 & op2;
		    break;

	    case 3:
		    ri = icodeP[1] & 0x3;
		    addr = C16x_ReadGpr16(ri);
		    op2 = C16x_MemRead16(addr);
		    result = op1 & op2;
		    C16x_SetGpr16(addr + 2, ri);
		    break;
	    default:
		    fprintf(stderr, "reached unreachable code\n");
		    return;
	}
	C16x_SetGpr16(result, n);
	REG_PSW &= ~(PSW_FLAG_E | PSW_FLAG_Z | PSW_FLAG_V | PSW_FLAG_C | PSW_FLAG_N);
	if (result == 0x8000) {
		REG_PSW |= PSW_FLAG_E;
	}
	if (result == 0) {
		REG_PSW |= PSW_FLAG_Z;
	}
	if (result & 0x8000) {
		REG_PSW |= PSW_FLAG_N;
	}
}

/*
 * ----------------------------------------------------------------
 * ANDB rb x
 * v0
 * ----------------------------------------------------------------
 */
void
c16x_andb_rb_x(uint8_t * icodeP)
{
	int n = (icodeP[1] & 0xf0) >> 4;
	uint16_t addr;
	int ri;
	uint8_t op1, op2;
	uint8_t result;
	int subcmd = (icodeP[1] & 0x0c) >> 2;
	op1 = C16x_ReadGpr8(n);
	switch (subcmd) {
	    case 0:
	    case 1:
		    op2 = icodeP[1] & 0x7;
		    result = op1 & op2;
		    break;

	    case 2:
		    ri = icodeP[1] & 0x3;
		    addr = C16x_ReadGpr16(ri);
		    op2 = C16x_MemRead8(addr);
		    result = op1 & op2;
		    break;

	    case 3:
		    ri = icodeP[1] & 0x3;
		    addr = C16x_ReadGpr16(ri);
		    op2 = C16x_MemRead8(addr);
		    result = op1 & op2;
		    C16x_SetGpr16(addr + 2, ri);
		    break;
	    default:
		    fprintf(stderr, "reached unreachable code\n");
		    return;
	}
	C16x_SetGpr8(result, n);
	REG_PSW &= ~(PSW_FLAG_E | PSW_FLAG_Z | PSW_FLAG_V | PSW_FLAG_C | PSW_FLAG_N);
	if (result == 0x80) {
		REG_PSW |= PSW_FLAG_E;
	}
	if (result == 0) {
		REG_PSW |= PSW_FLAG_Z;
	}
	if (result & 0x80) {
		REG_PSW |= PSW_FLAG_N;
	}
}

/*
 * ---------------------------------------------
 * BAND
 * v0
 * ---------------------------------------------
 */
void
c16x_band_bitaddr_bitaddr(uint8_t * icodeP)
{
	uint16_t qqaddr = icodeP[1];
	uint16_t qbit = (icodeP[3] & 0xf0) >> 4;
	uint16_t zzaddr = icodeP[2];
	uint16_t zbit = icodeP[3] & 0xf;
	uint16_t op1 = C16x_ReadBitaddr(qqaddr);
	uint16_t op2 = C16x_ReadBitaddr(zzaddr);
	uint16_t result;
	int bit1 = (op1 >> qbit) & 1;
	int bit2 = (op2 >> zbit) & 1;
	REG_PSW &= ~(PSW_FLAG_E | PSW_FLAG_Z | PSW_FLAG_V | PSW_FLAG_C | PSW_FLAG_N);
	if (bit1 | bit2) {
		REG_PSW |= PSW_FLAG_V;
	} else {
		REG_PSW |= PSW_FLAG_Z;
	}
	if (bit1 ^ bit2) {
		REG_PSW |= PSW_FLAG_N;
	}
	if (bit1 & bit2) {
		REG_PSW |= PSW_FLAG_C;
		result = op1 | (1 << qbit);
	} else {
		result = op1 & ~(1 << qbit);
	}
	C16x_WriteBitaddr(result, qqaddr);
}

/*
 * ------------------------------------------------
 * DIVL rw
 * ------------------------------------------------
 */
void
c16x_divl_rw(uint8_t * icodeP)
{
	int n = icodeP[1] & 0xf;
	int16_t op1 = C16x_ReadGpr16(n);
	int32_t md = (REG_MDL) | (REG_MDH << 16);
	int16_t mdl, mdh;
	REG_PSW &= ~(PSW_FLAG_E | PSW_FLAG_Z | PSW_FLAG_V | PSW_FLAG_C | PSW_FLAG_N);
	REG_MDC |= MDC_MDRIU;
	if (op1) {
		mdh = md % op1;
		mdl = md / op1;
		REG_MDL = mdl;
		REG_MDH = mdh;
	} else {
		REG_PSW |= PSW_FLAG_V;
	}
	if (REG_MDL == 0) {
		REG_PSW |= PSW_FLAG_Z;
	}
	if (REG_MDL & 0x8000) {
		REG_PSW |= PSW_FLAG_N;
	}
	fprintf(stderr, "Warning: divl instruction Register specification unclear in ISM\n");
}

/*
 * -------------------------------------------------
 * SHR rw rw
 * v0
 * -------------------------------------------------
 */
void
c16x_shr_rw_rw(uint8_t * icodeP)
{
	int n = (icodeP[1] & 0xf0) >> 4;
	int m = (icodeP[1] & 0xf);
	uint16_t op1, shift;
	uint16_t result;
	op1 = C16x_ReadGpr16(n);
	shift = C16x_ReadGpr16(m) & 0xf;
	REG_PSW &= ~(PSW_FLAG_E | PSW_FLAG_Z | PSW_FLAG_V | PSW_FLAG_C | PSW_FLAG_N);
	if (shift) {
		result = (op1 >> shift);
		if ((result << shift) != op1) {
			REG_PSW |= PSW_FLAG_V;
		}
		if (op1 & (1 << (shift - 1))) {
			REG_PSW |= PSW_FLAG_C;
		}
	} else {
		result = op1;
	}
	C16x_SetGpr16(result, n);
	if (result == 0) {
		REG_PSW |= PSW_FLAG_Z;
	}
	if (result & 0x8000) {
		REG_PSW |= PSW_FLAG_N;
	}
}

/*
 * ----------------------------------------------
 * OR rw rw
 * ----------------------------------------------
 */
void
c16x_or_rw_rw(uint8_t * icodeP)
{
	int n = (icodeP[1] & 0xf0) >> 4;
	int m = (icodeP[1] & 0xf);
	uint16_t op1, op2;
	uint16_t result;
	op1 = C16x_ReadGpr16(n);
	op2 = C16x_ReadGpr16(m);
	result = op1 | op2;
	C16x_SetGpr16(result, n);
	REG_PSW &= ~(PSW_FLAG_E | PSW_FLAG_Z | PSW_FLAG_V | PSW_FLAG_C | PSW_FLAG_N);
	if (result == 0x8000) {
		REG_PSW |= PSW_FLAG_E;
	}
	if (result == 0) {
		REG_PSW |= PSW_FLAG_Z;
	}
	if (result & 0x8000) {
		REG_PSW |= PSW_FLAG_N;
	}
}

/*
 * ---------------------------------------------------------
 * ORB rb rb
 * ---------------------------------------------------------
 */
void
c16x_orb_rb_rb(uint8_t * icodeP)
{
	int n = (icodeP[1] & 0xf0) >> 4;
	int m = (icodeP[1] & 0xf);
	uint8_t op1, op2;
	uint8_t result;
	op1 = C16x_ReadGpr8(n);
	op2 = C16x_ReadGpr8(m);
	result = op1 | op2;
	C16x_SetGpr8(result, n);
	REG_PSW &= ~(PSW_FLAG_E | PSW_FLAG_Z | PSW_FLAG_V | PSW_FLAG_C | PSW_FLAG_N);
	if (result == 0x80) {
		REG_PSW |= PSW_FLAG_E;
	} else if (result == 0) {
		REG_PSW |= PSW_FLAG_Z;
	}
	if (result & 0x80) {
		REG_PSW |= PSW_FLAG_N;
	}
}

/*
 * -------------------------------------------------------
 * OR reg mem
 * v1
 * -------------------------------------------------------
 */
void
c16x_or_reg_mem(uint8_t * icodeP)
{
	uint8_t reg = icodeP[1] & 0xff;
	uint16_t maddr = (icodeP[3] << 8) | icodeP[2];
	uint16_t op1, op2;
	uint16_t result;
	op1 = C16x_ReadReg16(reg);
	op2 = C16x_MemRead16(maddr);
	result = op1 | op2;
	C16x_SetReg16(result, reg);
	REG_PSW &= ~(PSW_FLAG_E | PSW_FLAG_Z | PSW_FLAG_V | PSW_FLAG_C | PSW_FLAG_N);
	if (result == 0x8000) {
		REG_PSW |= PSW_FLAG_E;
	}
	if (result == 0) {
		REG_PSW |= PSW_FLAG_Z;
	}
	if (result & 0x8000) {
		REG_PSW |= PSW_FLAG_N;
	}
}

/*
 * -------------------------------------------------------
 * ORB reg mem
 * -------------------------------------------------------
 */
void
c16x_orb_reg_mem(uint8_t * icodeP)
{
	uint8_t reg = icodeP[1] & 0xff;
	uint16_t maddr = (icodeP[3] << 8) | icodeP[2];
	uint8_t op1, op2;
	uint8_t result;
	op1 = C16x_ReadReg8(reg);
	op2 = C16x_MemRead8(maddr);
	result = op1 | op2;
	C16x_SetReg8(result, reg);

	REG_PSW &= ~(PSW_FLAG_E | PSW_FLAG_Z | PSW_FLAG_V | PSW_FLAG_C | PSW_FLAG_N);
	if (result == 0x80) {
		REG_PSW |= PSW_FLAG_E;
	} else if (result == 0) {
		REG_PSW |= PSW_FLAG_Z;
	}
	if (result & 0x80) {
		REG_PSW |= PSW_FLAG_N;
	}
}

/* 
 * ----------------------------------------------------
 * OR mem reg
 * v1
 * ----------------------------------------------------
 */
void
c16x_or_mem_reg(uint8_t * icodeP)
{
	uint8_t reg = icodeP[1] & 0xff;
	uint16_t maddr = (icodeP[3] << 8) | icodeP[2];
	uint16_t op1, op2;
	uint16_t result;
	op1 = C16x_MemRead16(maddr);
	op2 = C16x_ReadReg16(reg);
	result = op1 | op2;
	C16x_MemWrite16(result, maddr);
	REG_PSW &= ~(PSW_FLAG_E | PSW_FLAG_Z | PSW_FLAG_V | PSW_FLAG_C | PSW_FLAG_N);
	if (result == 0x8000) {
		REG_PSW |= PSW_FLAG_E;
	}
	if (result == 0) {
		REG_PSW |= PSW_FLAG_Z;
	}
	if (result & 0x8000) {
		REG_PSW |= PSW_FLAG_N;
	}
}

/*
 * ----------------------------------------------
 * ORB mem reg
 * v1
 * ----------------------------------------------
 */
void
c16x_orb_mem_reg(uint8_t * icodeP)
{
	uint8_t reg = icodeP[1] & 0xff;
	uint16_t maddr = (icodeP[3] << 8) | icodeP[2];
	uint8_t op1, op2;
	uint8_t result;
	op1 = C16x_MemRead8(maddr);
	op2 = C16x_ReadReg8(reg);
	result = op1 | op2;
	C16x_MemWrite8(result, maddr);
	REG_PSW &= ~(PSW_FLAG_E | PSW_FLAG_Z | PSW_FLAG_V | PSW_FLAG_C | PSW_FLAG_N);
	if (result == 0x80) {
		REG_PSW |= PSW_FLAG_E;
	} else if (result == 0) {
		REG_PSW |= PSW_FLAG_Z;
	}
	if (result & 0x80) {
		REG_PSW |= PSW_FLAG_N;
	}
}

/*
 * ----------------------------------------------------------
 * OR reg data16
 * v1
 * ----------------------------------------------------------
 */
void
c16x_or_reg_data16(uint8_t * icodeP)
{
	uint8_t reg = icodeP[1] & 0xff;
	uint16_t data16 = (icodeP[3] << 8) | icodeP[2];
	uint16_t op1, op2;
	uint16_t result;
	op1 = C16x_ReadReg16(reg);
	op2 = data16;
	result = op1 | op2;
	C16x_SetReg16(result, reg);

	REG_PSW &= ~(PSW_FLAG_E | PSW_FLAG_Z | PSW_FLAG_V | PSW_FLAG_C | PSW_FLAG_N);
	if (result == 0x8000) {
		REG_PSW |= PSW_FLAG_E;
	}
	if (result == 0) {
		REG_PSW |= PSW_FLAG_Z;
	}
	if (result & 0x8000) {
		REG_PSW |= PSW_FLAG_N;
	}
}

/*
 * ---------------------------------------------
 * ORB reg data8
 * v1
 * ---------------------------------------------
 */
void
c16x_orb_reg_data8(uint8_t * icodeP)
{
	uint8_t reg = icodeP[1] & 0xff;
	uint8_t data8 = icodeP[2];
	uint8_t op1, op2;
	uint8_t result;
	op1 = C16x_ReadReg8(reg);
	op2 = data8;
	result = op1 | op2;
	C16x_SetReg8(result, reg);

	REG_PSW &= ~(PSW_FLAG_E | PSW_FLAG_Z | PSW_FLAG_V | PSW_FLAG_C | PSW_FLAG_N);
	if (result == 0x80) {
		REG_PSW |= PSW_FLAG_E;
	}
	if (result == 0) {
		REG_PSW |= PSW_FLAG_Z;
	}
	if (result & 0x80) {
		REG_PSW |= PSW_FLAG_N;
	}
}

/*
 * -----------------------------------------
 * OR rw x
 * v1
 * -----------------------------------------
 */
void
c16x_or_rw_x(uint8_t * icodeP)
{
	int n = (icodeP[1] & 0xf0) >> 4;
	uint16_t addr;
	int ri;
	uint16_t op1, op2;
	uint16_t result;
	int subcmd = (icodeP[1] & 0x0c) >> 2;
	op1 = C16x_ReadGpr16(n);
	switch (subcmd) {
	    case 0:
	    case 1:
		    op2 = icodeP[1] & 0x7;
		    result = op1 | op2;
		    break;

	    case 2:
		    ri = icodeP[1] & 0x3;
		    addr = C16x_ReadGpr16(ri);
		    op2 = C16x_MemRead16(addr);
		    result = op1 | op2;
		    break;

	    case 3:
		    ri = icodeP[1] & 0x3;
		    addr = C16x_ReadGpr16(ri);
		    op2 = C16x_MemRead16(addr);
		    result = op1 | op2;
		    C16x_SetGpr16(addr + 2, ri);
		    break;
	    default:
		    return;
	}
	C16x_SetGpr16(result, n);
	REG_PSW &= ~(PSW_FLAG_E | PSW_FLAG_Z | PSW_FLAG_V | PSW_FLAG_C | PSW_FLAG_N);
	if (result == 0x8000) {
		REG_PSW |= PSW_FLAG_E;
	}
	if (result == 0) {
		REG_PSW |= PSW_FLAG_Z;
	}
	if (result & 0x8000) {
		REG_PSW |= PSW_FLAG_N;
	}
}

/*
 * -------------------------------------------
 * ORB rb x
 * v1
 * -------------------------------------------
 */
void
c16x_orb_rb_x(uint8_t * icodeP)
{
	int n = (icodeP[1] & 0xf0) >> 4;
	uint16_t addr;
	int ri;
	uint8_t op1, op2;
	uint8_t result;
	int subcmd = (icodeP[1] & 0x0c) >> 2;
	op1 = C16x_ReadGpr8(n);
	switch (subcmd) {
	    case 0:
	    case 1:
		    op2 = icodeP[1] & 0x7;
		    result = op1 | op2;
		    break;

	    case 2:
		    ri = icodeP[1] & 0x3;
		    addr = C16x_ReadGpr16(ri);
		    op2 = C16x_MemRead8(addr);
		    result = op1 | op2;
		    break;

	    case 3:
		    ri = icodeP[1] & 0x3;
		    addr = C16x_ReadGpr16(ri);
		    op2 = C16x_MemRead8(addr);
		    result = op1 | op2;
		    C16x_SetGpr16(addr + 2, ri);
		    break;
	    default:
		    return;
	}
	C16x_SetGpr8(result, n);
	REG_PSW &= ~(PSW_FLAG_E | PSW_FLAG_Z | PSW_FLAG_V | PSW_FLAG_C | PSW_FLAG_N);
	if (result == 0x80) {
		REG_PSW |= PSW_FLAG_E;
	}
	if (result == 0) {
		REG_PSW |= PSW_FLAG_Z;
	}
	if (result & 0x80) {
		REG_PSW |= PSW_FLAG_N;
	}
}

/*
 * ----------------------------------------------------
 * BXOR
 * ----------------------------------------------------
 */
void
c16x_bxor_bitaddr_bitaddr(uint8_t * icodeP)
{
	uint16_t qqaddr = icodeP[1];
	uint16_t qbit = (icodeP[3] & 0xf0) >> 4;
	uint16_t zzaddr = icodeP[2];
	uint16_t zbit = icodeP[3] & 0xf;
	uint16_t op1 = C16x_ReadBitaddr(qqaddr);
	uint16_t op2 = C16x_ReadBitaddr(zzaddr);
	uint16_t result;
	int bit1 = (op1 >> qbit) & 1;
	int bit2 = (op2 >> zbit) & 1;
	REG_PSW &= ~(PSW_FLAG_C | PSW_FLAG_V | PSW_FLAG_Z | PSW_FLAG_N | PSW_FLAG_E);
	if (bit1 | bit2) {
		REG_PSW |= PSW_FLAG_V;
	} else {
		REG_PSW |= PSW_FLAG_Z;
	}
	if (bit1 ^ bit2) {
		REG_PSW |= PSW_FLAG_N;
		result = op1 | (1 << qbit);
	} else {
		result = op1 & ~(1 << qbit);
	}
	if (bit1 & bit2) {
		REG_PSW |= PSW_FLAG_C;
	}
	C16x_WriteBitaddr(result, qqaddr);
}

/*
 * ---------------------------------------------------------------
 * DIVLU
 * v0
 * ---------------------------------------------------------------
 */
void
c16x_divlu_rw(uint8_t * icodeP)
{
	int n = icodeP[1] & 0xf;
	uint16_t op1 = C16x_ReadGpr16(n);
	uint32_t md = (REG_MDH << 16) | REG_MDL;
	REG_PSW &= ~(PSW_FLAG_E | PSW_FLAG_Z | PSW_FLAG_V | PSW_FLAG_C | PSW_FLAG_N);
	REG_MDC |= MDC_MDRIU;
	if (op1) {
		REG_MDH = md % op1;
		REG_MDL = md / op1;
	} else {
		REG_PSW |= PSW_FLAG_V;
	}
	if (REG_MDL == 0) {
		REG_PSW |= PSW_FLAG_Z;
	}
	if (REG_MDL & 0x8000) {
		REG_PSW |= PSW_FLAG_N;
	}
	fprintf(stderr, "Warning: divlu instruction Register specification unclear in ISM\n");
}

/*
 * -----------------------------------------------------
 * SHR
 * -----------------------------------------------------
 */
void
c16x_shr_rw_data4(uint8_t * icodeP)
{
	uint16_t shift = (icodeP[1] & 0xf0) >> 4;
	int n = (icodeP[1] & 0xf);
	uint16_t op1 = C16x_ReadGpr16(n);
	uint16_t result;
	REG_PSW &= ~(PSW_FLAG_E | PSW_FLAG_Z | PSW_FLAG_V | PSW_FLAG_C | PSW_FLAG_N);
	if (shift) {
		result = (op1 >> shift);
		if ((result << shift) != op1) {
			REG_PSW |= PSW_FLAG_V;
		}
		if (op1 & (1 << (shift - 1))) {
			REG_PSW |= PSW_FLAG_C;
		}
	} else {
		result = op1;
	}
	if (result == 0) {
		REG_PSW |= PSW_FLAG_Z;
	}
	if (result & 0x8000) {
		REG_PSW |= PSW_FLAG_N;
	}
	C16x_SetGpr16(result, n);
}

/*
 * ---------------------------------------------------
 * CMPI1 rw data4
 * v1
 * ---------------------------------------------------
 */
void
c16x_cmpi1_rw_data4(uint8_t * icodeP)
{
	int n = icodeP[1] & 0xf;
	uint16_t op1 = C16x_ReadGpr16(n);
	uint16_t op2 = (icodeP[1] & 0xf0) >> 4;
	uint16_t result;
	result = op1 - op2;
	REG_PSW &= ~(PSW_FLAG_E | PSW_FLAG_Z | PSW_FLAG_V | PSW_FLAG_C | PSW_FLAG_N);
	if (result == 0x8000) {
		REG_PSW |= PSW_FLAG_E;
	}
	if (result == 0) {
		REG_PSW |= PSW_FLAG_Z;
	}
	if (result & 0x8000) {
		REG_PSW |= PSW_FLAG_N;
	}
	if (sub_carry(op1, op2, result)) {
		REG_PSW |= PSW_FLAG_C;
	}
	if (sub_overflow(op1, op2, result)) {
		REG_PSW |= PSW_FLAG_V;
	}
	C16x_SetGpr16(op1 + 1, n);
}

/*
 * --------------------------------------------------------
 * NEG rw x
 * v0
 * --------------------------------------------------------
 */
void
c16x_neg_rw(uint8_t * icodeP)
{
	int n = icodeP[1] >> 4;
	uint16_t op1 = C16x_ReadGpr16(n);
	uint16_t result = -op1;
	REG_PSW &= ~(PSW_FLAG_E | PSW_FLAG_Z | PSW_FLAG_V | PSW_FLAG_C | PSW_FLAG_N);
	if (result == 0x8000) {
		REG_PSW |= PSW_FLAG_E;
	}
	if (result == 0) {
		REG_PSW |= PSW_FLAG_Z;
	}
	if (result & 0x8000) {
		REG_PSW |= PSW_FLAG_N;
	}
	if (sub_carry(0, op1, result)) {
		REG_PSW |= PSW_FLAG_C;
	}
	if (sub_overflow(0, op1, result)) {
		REG_PSW |= PSW_FLAG_V;
	}

}

/*
 * ------------------------------------
 * CMPI1 rw mem
 * v0
 * ------------------------------------
 */
void
c16x_cmpi1_rw_mem(uint8_t * icodeP)
{
	int n = icodeP[1] & 0xf;
	uint16_t op1 = C16x_ReadGpr16(n);
	uint16_t maddr = (icodeP[3] << 8) | icodeP[2];
	uint16_t op2 = C16x_MemRead16(maddr);
	uint16_t result;
	result = op1 - op2;
	REG_PSW &= ~(PSW_FLAG_E | PSW_FLAG_Z | PSW_FLAG_V | PSW_FLAG_C | PSW_FLAG_N);
	if (result == 0x8000) {
		REG_PSW |= PSW_FLAG_E;
	}
	if (result == 0) {
		REG_PSW |= PSW_FLAG_Z;
	}
	if (result & 0x8000) {
		REG_PSW |= PSW_FLAG_N;
	}
	if (sub_carry(op1, op2, result)) {
		REG_PSW |= PSW_FLAG_C;
	}
	if (sub_overflow(op1, op2, result)) {
		REG_PSW |= PSW_FLAG_V;
	}
	C16x_SetGpr16(op1 + 1, n);
}

/*
 * -----------------------------------------------------
 * MOV [rw] mem
 * v1
 * -----------------------------------------------------
 */
void
c16x_mov__rw__mem(uint8_t * icodeP)
{
	int n = (icodeP[1] & 0xf);
	uint16_t srcaddr = icodeP[2] | (icodeP[3] << 8);
	uint16_t op2 = C16x_MemRead16(srcaddr);
	uint16_t dstaddr = C16x_ReadGpr16(n);
	uint16_t result = op2;
	C16x_MemWrite16(result, dstaddr);
	REG_PSW &= ~(PSW_FLAG_E | PSW_FLAG_Z | PSW_FLAG_N);
	if (result == 0x8000) {
		REG_PSW |= PSW_FLAG_E;
	}
	if (result == 0) {
		REG_PSW |= PSW_FLAG_Z;
	}
	if (result & 0x8000) {
		REG_PSW |= PSW_FLAG_N;
	}
}

/*
 * -------------------------------------------------
 * CMPI1 rw data16
 * -------------------------------------------------
 */
void
c16x_cmpi1_rw_data16(uint8_t * icodeP)
{
	int n = icodeP[1] & 0xf;
	uint16_t op1 = C16x_ReadGpr16(n);
	uint16_t data16 = (icodeP[3] << 8) | icodeP[2];
	uint16_t op2 = data16;
	uint16_t result;
	result = op1 - op2;
	REG_PSW &= ~(PSW_FLAG_E | PSW_FLAG_Z | PSW_FLAG_V | PSW_FLAG_C | PSW_FLAG_N);
	if (result == 0x8000) {
		REG_PSW |= PSW_FLAG_E;
	}
	if (result == 0) {
		REG_PSW |= PSW_FLAG_Z;
	}
	if (result & 0x8000) {
		REG_PSW |= PSW_FLAG_N;
	}
	if (sub_carry(op1, op2, result)) {
		REG_PSW |= PSW_FLAG_C;
	}
	if (sub_overflow(op1, op2, result)) {
		REG_PSW |= PSW_FLAG_V;
	}
	C16x_SetGpr16(op1 + 1, n);
}

void
c16x_idle(uint8_t * icodeP)
{
	fprintf(stderr, "sleep until an interrupt occurs not implemented\n");
	return;
}

/*
 * -----------------------------------------------------------------------
 * MOV [-Rwm] Rw
 * v1
 * -----------------------------------------------------------------------
 */
void
c16x_mov__mrw__rw(uint8_t * icodeP)
{
	int n = (icodeP[1] & 0xf0) >> 4;
	int m = (icodeP[1] & 0xf);
	uint16_t result = C16x_ReadGpr16(n);
	uint16_t dstaddr = C16x_ReadGpr16(m) - 2;
	C16x_SetGpr16(dstaddr, m);
	C16x_MemWrite16(result, dstaddr);
	REG_PSW &= ~(PSW_FLAG_E | PSW_FLAG_Z | PSW_FLAG_N);
	if (result == 0x8000) {
		REG_PSW |= PSW_FLAG_E;
	}
	if (result == 0) {
		REG_PSW |= PSW_FLAG_Z;
	}
	if (result & 0x8000) {
		REG_PSW |= PSW_FLAG_N;
	}

}

/*
 * ------------------------------------------------
 * MOVB [-Rwm] Rbn
 * ------------------------------------------------
 */
void
c16x_movb__mrw__rb(uint8_t * icodeP)
{
	int n = (icodeP[1] & 0xf0) >> 4;
	int m = (icodeP[1] & 0xf);
	uint8_t result = C16x_ReadGpr8(n);
	uint16_t dstaddr = C16x_ReadGpr16(m) - 1;
	C16x_SetGpr16(dstaddr, m);
	C16x_MemWrite8(result, dstaddr);
	REG_PSW &= ~(PSW_FLAG_E | PSW_FLAG_Z | PSW_FLAG_N);
	if (result == 0x80) {
		REG_PSW |= PSW_FLAG_E;
	} else if (result == 0) {
		REG_PSW |= PSW_FLAG_Z;
	}
	if (result & 0x80) {
		REG_PSW |= PSW_FLAG_N;
	}
}

/*
 * -----------------------------------
 * jb bitaddr rel
 * v0
 * -----------------------------------
 */
void
c16x_jb_bitaddr_rel(uint8_t * icodeP)
{
	uint8_t qq = icodeP[1];
	int q = icodeP[3] >> 4;
	int8_t rel = icodeP[2];
	uint16_t bitfield;
	bitfield = C16x_ReadBitaddr(qq);
	if ((bitfield & (1 << q))) {
		REG_IP = REG_IP + rel + rel;
	}
}

/*
 * -------------------------------------
 * CMPI2 rw data4
 * v1
 * -------------------------------------
 */
void
c16x_cmpi2_rw_data4(uint8_t * icodeP)
{
	int n = icodeP[1] & 0xf;
	uint16_t op1 = C16x_ReadGpr16(n);
	uint16_t op2 = (icodeP[1] & 0xf0) >> 4;
	uint16_t result;
	result = op1 - op2;
	REG_PSW &= ~(PSW_FLAG_E | PSW_FLAG_Z | PSW_FLAG_V | PSW_FLAG_C | PSW_FLAG_N);
	if (result == 0x8000) {
		REG_PSW |= PSW_FLAG_E;
	}
	if (result == 0) {
		REG_PSW |= PSW_FLAG_Z;
	}
	if (result & 0x8000) {
		REG_PSW |= PSW_FLAG_N;
	}
	if (sub_carry(op1, op2, result)) {
		REG_PSW |= PSW_FLAG_C;
	}
	if (sub_overflow(op1, op2, result)) {
		REG_PSW |= PSW_FLAG_V;
	}
	C16x_SetGpr16(op1 + 2, n);
}

/*
 * ------------------------------------
 * CPL Rw
 * ------------------------------------
 */
void
c16x_cpl_rw(uint8_t * icodeP)
{
	int n = (icodeP[1] & 0xf0) >> 4;
	uint16_t op1 = C16x_ReadGpr16(n);
	uint16_t result = ~op1;
	C16x_SetGpr16(result, n);
	REG_PSW &= ~(PSW_FLAG_E | PSW_FLAG_Z | PSW_FLAG_V | PSW_FLAG_C | PSW_FLAG_N);
	if (result == 0x8000) {
		REG_PSW |= PSW_FLAG_E;
	} else if (result == 0) {
		REG_PSW |= PSW_FLAG_Z;
	}
	if (result & 0x8000) {
		REG_PSW |= PSW_FLAG_N;
	}
}

/*
 * ------------------------------------------
 * CMPI2 Rw mem
 * v1
 * ------------------------------------------
 */
void
c16x_cmpi2_rw_mem(uint8_t * icodeP)
{
	int n = icodeP[1] & 0xf;
	uint16_t op1 = C16x_ReadGpr16(n);
	uint16_t maddr = (icodeP[3] << 8) | icodeP[2];
	uint16_t op2 = C16x_MemRead16(maddr);
	uint16_t result;
	result = op1 - op2;
	C16x_SetGpr16(op1 + 2, n);
	REG_PSW &= ~(PSW_FLAG_E | PSW_FLAG_Z | PSW_FLAG_V | PSW_FLAG_C | PSW_FLAG_N);
	if (result == 0x8000) {
		REG_PSW |= PSW_FLAG_E;
	}
	if (result == 0) {
		REG_PSW |= PSW_FLAG_Z;
	}
	if (result & 0x8000) {
		REG_PSW |= PSW_FLAG_N;
	}
	if (sub_carry(op1, op2, result)) {
		REG_PSW |= PSW_FLAG_C;
	}
	if (sub_overflow(op1, op2, result)) {
		REG_PSW |= PSW_FLAG_V;
	}
}

/*
 * -----------------------------------------
 * MOV mem [Rwn]
 * v0
 * -----------------------------------------
 */
void
c16x_mov_mem__rw_(uint8_t * icodeP)
{
	int n = (icodeP[1] & 0xf);
	uint16_t dstaddr = icodeP[2] | (icodeP[3] << 8);
	uint16_t srcaddr = C16x_ReadGpr16(n);
	uint16_t result = C16x_MemRead16(srcaddr);
	C16x_MemWrite16(result, dstaddr);
	REG_PSW &= ~(PSW_FLAG_E | PSW_FLAG_Z | PSW_FLAG_N);
	if (result == 0x8000) {
		REG_PSW |= PSW_FLAG_E;
	}
	if (result == 0) {
		REG_PSW |= PSW_FLAG_Z;
	}
	if (result & 0x8000) {
		REG_PSW |= PSW_FLAG_N;
	}
}

/*
 * ----------------------------------------
 * CMPI2 rw data16
 * v0
 * ----------------------------------------
 */
void
c16x_cmpi2_rw_data16(uint8_t * icodeP)
{
	int n = icodeP[1] & 0xf;
	uint16_t op1 = C16x_ReadGpr16(n);
	uint16_t data16 = (icodeP[3] << 8) | icodeP[2];
	uint16_t op2 = data16;
	uint16_t result;
	result = op1 - op2;
	C16x_SetGpr16(op1 + 2, n);
	REG_PSW &= ~(PSW_FLAG_E | PSW_FLAG_Z | PSW_FLAG_V | PSW_FLAG_C | PSW_FLAG_N);
	if (result == 0x8000) {
		REG_PSW |= PSW_FLAG_E;
	}
	if (result == 0) {
		REG_PSW |= PSW_FLAG_Z;
	}
	if (result & 0x8000) {
		REG_PSW |= PSW_FLAG_N;
	}
	if (sub_carry(op1, op2, result)) {
		REG_PSW |= PSW_FLAG_C;
	}
	if (sub_overflow(op1, op2, result)) {
		REG_PSW |= PSW_FLAG_V;
	}
}

/*
 * -------------------------------------------
 * PWRDN
 * v1
 * -------------------------------------------
 */
void
c16x_pwrdn(uint8_t * icodeP)
{
	fprintf(stderr, "C16x CPU Power Down\n");
	exit(0);
}

/*
 * --------------------------------------------
 * MOV Rwn [Rwm+]
 * v0
 * --------------------------------------------
 */

void
c16x_mov_rw__rwp_(uint8_t * icodeP)
{

	int n = (icodeP[1] & 0xf0) >> 4;
	int m = (icodeP[1] & 0xf);
	uint16_t srcaddr = C16x_ReadGpr16(m);
	uint16_t result = C16x_MemRead16(srcaddr);
	C16x_SetGpr16(srcaddr + 2, m);
	C16x_SetGpr16(result, n);
	REG_PSW &= ~(PSW_FLAG_E | PSW_FLAG_Z | PSW_FLAG_N);
	if (result == 0x8000) {
		REG_PSW |= PSW_FLAG_E;
	}
	if (result == 0) {
		REG_PSW |= PSW_FLAG_Z;
	}
	if (result & 0x8000) {
		REG_PSW |= PSW_FLAG_N;
	}
}

/*
 * -------------------------------------------------------------
 * MOVB Rbn [Rwn+]
 * v0
 * -------------------------------------------------------------
 */
void
c16x_movb_rb__rwp_(uint8_t * icodeP)
{
	int n = (icodeP[1] & 0xf0) >> 4;
	int m = (icodeP[1] & 0xf);
	uint16_t srcaddr = C16x_ReadGpr16(m);
	uint8_t result = C16x_MemRead8(srcaddr);
	C16x_SetGpr16(srcaddr + 1, m);
	C16x_SetGpr8(result, n);
	REG_PSW &= ~(PSW_FLAG_E | PSW_FLAG_Z | PSW_FLAG_N);
	if (result == 0x80) {
		REG_PSW |= PSW_FLAG_E;
	} else if (result == 0) {
		REG_PSW |= PSW_FLAG_Z;
	}
	if (result & 0x80) {
		REG_PSW |= PSW_FLAG_N;
	}
}

/*
 * -----------------------------------------------------
 * JNB bitaddr rel
 * v1
 * -----------------------------------------------------
 */
void
c16x_jnb_bitaddr_rel(uint8_t * icodeP)
{
	uint8_t bitaddr = icodeP[1];
	int bit = icodeP[3] >> 4;
	int8_t rel = icodeP[2];
	uint16_t bitfield;
	bitfield = C16x_ReadBitaddr(bitaddr);
	if (!(bitfield & (1 << bit))) {
		REG_IP = REG_IP + rel + rel;
	}
}

/*
 * ----------------------------------------------------
 * Trap
 * v0
 * ----------------------------------------------------
 */
void
c16x_trap_ntrap7(uint8_t * icodeP)
{
	uint16_t trap8 = icodeP[1] & 0xfe;
	REG_SP = REG_SP - 2;
	C16x_MemWrite16(REG_PSW, REG_SP);
	if (SYSCON_SGTDIS == 0) {
		REG_SP = REG_SP - 2;
		C16x_MemWrite16(REG_CSP, REG_SP);
		REG_CSP = 0;
	}
	REG_SP = REG_SP - 2;
	C16x_MemWrite16(REG_IP, REG_SP);
	REG_IP = trap8 << 1;
}

/*
 * ------------------------------------------------
 * JMPI cc [Rw]
 * v0
 * ------------------------------------------------
 */
void
c16x_jmpi_cc__rw_(uint8_t * icodeP)
{
	int n = icodeP[1] & 0xf;
	int addr;
	if (check_condition(icodeP[1])) {
		addr = C16x_ReadGpr16(n);
		REG_IP = C16x_MemRead16(addr);
	}
}

/*
 * -----------------------------------------------------
 * CMPD1 Rw data4
 * v1
 * -----------------------------------------------------
 */
void
c16x_cmpd1_rw_data4(uint8_t * icodeP)
{
	int n = icodeP[1] & 0xf;
	uint16_t op1 = C16x_ReadGpr16(n);
	uint16_t op2 = (icodeP[1] & 0xf0) >> 4;
	uint16_t result;
	result = op1 - op2;
	C16x_SetGpr16(op1 - 1, n);
	REG_PSW &= ~(PSW_FLAG_E | PSW_FLAG_Z | PSW_FLAG_V | PSW_FLAG_C | PSW_FLAG_N);
	if (result == 0x8000) {
		REG_PSW |= PSW_FLAG_E;
	}
	if (result == 0) {
		REG_PSW |= PSW_FLAG_Z;
	}
	if (result & 0x8000) {
		REG_PSW |= PSW_FLAG_N;
	}
	if (sub_carry(op1, op2, result)) {
		REG_PSW |= PSW_FLAG_C;
	}
	if (sub_overflow(op1, op2, result)) {
		REG_PSW |= PSW_FLAG_V;
	}
}

/*
 * ---------------------------------------------------
 * NEGB Rb
 * ---------------------------------------------------
 */
void
c16x_negb_rb(uint8_t * icodeP)
{
	int n = icodeP[1] >> 4;
	uint8_t op1 = C16x_ReadGpr8(n);
	uint8_t result = -op1;
	C16x_SetGpr8(result, n);
	REG_PSW &= ~(PSW_FLAG_E | PSW_FLAG_Z | PSW_FLAG_V | PSW_FLAG_C | PSW_FLAG_N);
	if (result == 0x80) {
		REG_PSW |= PSW_FLAG_E;
	}
	if (result == 0) {
		REG_PSW |= PSW_FLAG_Z;
	}
	if (result & 0x80) {
		REG_PSW |= PSW_FLAG_N;
	}
	if (sub_carry_b(0, op1, result)) {
		REG_PSW |= PSW_FLAG_C;
	}
	if (sub_overflow_b(0, op1, result)) {
		REG_PSW |= PSW_FLAG_V;
	}
}

/*
 * ------------------------------------------------
 * CMPD1 rw mem
 * v0
 * ------------------------------------------------
 */
void
c16x_cmpd1_rw_mem(uint8_t * icodeP)
{
	int n = icodeP[1] & 0xf;
	uint16_t op1 = C16x_ReadGpr16(n);
	uint16_t maddr = (icodeP[3] << 8) | icodeP[2];
	uint16_t result;
	uint16_t op2 = C16x_MemRead16(maddr);
	result = op1 - op2;
	C16x_SetGpr16(op1 - 1, n);
	REG_PSW &= ~(PSW_FLAG_E | PSW_FLAG_Z | PSW_FLAG_V | PSW_FLAG_C | PSW_FLAG_N);
	if (result == 0x8000) {
		REG_PSW |= PSW_FLAG_E;
	} else if (result == 0) {
		REG_PSW |= PSW_FLAG_Z;
	}
	if (result & 0x8000) {
		REG_PSW |= PSW_FLAG_N;
	}
	if (sub_carry(op1, op2, result)) {
		REG_PSW |= PSW_FLAG_C;
	}
	if (sub_overflow(op1, op2, result)) {
		REG_PSW |= PSW_FLAG_V;
	}
}

/*
 * -------------------------------------------------------------
 * MOVB [Rw] mem
 * v0
 * -------------------------------------------------------------
 */
void
c16x_movb__rw__mem(uint8_t * icodeP)
{
	int n = (icodeP[1] & 0xf);
	uint16_t srcaddr = icodeP[2] | (icodeP[3] << 8);
	uint8_t result = C16x_MemRead8(srcaddr);
	uint16_t dstaddr = C16x_ReadGpr16(n);
	C16x_MemWrite8(result, dstaddr);
	REG_PSW &= ~(PSW_FLAG_E | PSW_FLAG_Z | PSW_FLAG_N);
	if (result == 0x80) {
		REG_PSW |= PSW_FLAG_E;
	}
	if (result == 0) {
		REG_PSW |= PSW_FLAG_Z;
	}
	if (result & 0x80) {
		REG_PSW |= PSW_FLAG_N;
	}
}

void
c16x_diswdt(uint8_t * icodeP)
{
	if ((icodeP[1] != 0x5a) || (icodeP[2] != 0xa5) || (icodeP[3] != 0xa5)) {
		fprintf(stderr, "illegal format if diswdt\n");
		fprintf(stderr, "%02x %02x %02x %02x\n", icodeP[0], icodeP[1], icodeP[2],
			icodeP[3]);
		return;
	}
}

/*
 * ------------------------------------
 * CMPD1 Rw data16
 * v0
 * ------------------------------------
 */
void
c16x_cmpd1_rw_data16(uint8_t * icodeP)
{
	int n = icodeP[1] & 0xf;
	uint16_t op1 = C16x_ReadGpr16(n);
	uint16_t data16 = (icodeP[3] << 8) | icodeP[2];
	uint16_t op2 = data16;
	uint16_t result;
	result = op1 - op2;
	C16x_SetGpr16(op1 - 1, n);
	REG_PSW &= ~(PSW_FLAG_E | PSW_FLAG_Z | PSW_FLAG_V | PSW_FLAG_C | PSW_FLAG_N);
	if (result == 0x8000) {
		REG_PSW |= PSW_FLAG_E;
	}
	if (result == 0) {
		REG_PSW |= PSW_FLAG_Z;
	}
	if (result & 0x8000) {
		REG_PSW |= PSW_FLAG_N;
	}
	if (sub_carry(op1, op2, result)) {
		REG_PSW |= PSW_FLAG_C;
	}
	if (sub_overflow(op1, op2, result)) {
		REG_PSW |= PSW_FLAG_V;
	}
}

/*
 * --------------------------------------------------------------------
 * Service Watchdog timer is a protected instruction
 * v0
 * --------------------------------------------------------------------
 */
void
c16x_srvwdt(uint8_t * icodeP)
{
	if ((icodeP[1] != 0x58) || (icodeP[2] != 0xa7) || (icodeP[3] != 0xa7)) {
		fprintf(stderr, "illegal format if srvwdt\n");
		fprintf(stderr, "%02x %02x %02x %02x\n", icodeP[0], icodeP[1], icodeP[2],
			icodeP[3]);
		return;
	}

}

/*
 * ---------------------------------------------------------------------
 * MOV Rwn [Rwm]
 * v0
 * ---------------------------------------------------------------------
 */
void
c16x_mov_rw__rw_(uint8_t * icodeP)
{
	int n = icodeP[1] >> 4;
	int m = icodeP[1] & 0xf;
	uint16_t srcaddr = C16x_ReadGpr16(m);
	uint16_t result = C16x_MemRead16(srcaddr);
	C16x_SetGpr16(result, n);

	REG_PSW &= ~(PSW_FLAG_E | PSW_FLAG_Z | PSW_FLAG_N);
	if (result == 0x8000) {
		REG_PSW |= PSW_FLAG_E;
	}
	if (result == 0) {
		REG_PSW |= PSW_FLAG_Z;
	}
	if (result & 0x8000) {
		REG_PSW |= PSW_FLAG_N;
	}
}

/*
 * -----------------------------------------------------------
 *  MOVB Rbn [Rwm]
 * -----------------------------------------------------------
 */
void
c16x_movb_rb__rw_(uint8_t * icodeP)
{
	int n = icodeP[1] >> 4;
	int m = icodeP[1] & 0xf;
	uint16_t srcaddr = C16x_ReadGpr16(m);
	uint8_t result = C16x_MemRead8(srcaddr);
	C16x_SetGpr8(result, n);

	REG_PSW &= ~(PSW_FLAG_E | PSW_FLAG_Z | PSW_FLAG_N);
	if (result == 0x80) {
		REG_PSW |= PSW_FLAG_E;
	}
	if (result == 0) {
		REG_PSW |= PSW_FLAG_Z;
	}
	if (result & 0x80) {
		REG_PSW |= PSW_FLAG_N;
	}
}

/*
 * ----------------------------------------------------------
 * JBC
 * v0
 * ----------------------------------------------------------
 */
void
c16x_jbc_bitaddr_rel(uint8_t * icodeP)
{
	uint8_t bitaddr = icodeP[1];
	int bit = icodeP[3] >> 4;
	int8_t rel = icodeP[2];
	uint16_t bitfield;
	bitfield = C16x_ReadBitaddr(bitaddr);
	REG_PSW &= ~(PSW_FLAG_E | PSW_FLAG_Z | PSW_FLAG_V | PSW_FLAG_C | PSW_FLAG_N);

	if ((bitfield & (1 << bit))) {
		REG_IP = REG_IP + rel + rel;
		bitfield = bitfield & ~(1 << bit);
		C16x_WriteBitaddr(bitfield, bitaddr);
		REG_PSW |= PSW_FLAG_N;
	} else {
		REG_PSW |= PSW_FLAG_Z;
	}
}

/*
 * ----------------------------------------------------------
 * CALLI cc [Rwn]
 * v0
 * ----------------------------------------------------------
 */
void
c16x_calli_cc__rw_(uint8_t * icodeP)
{
	int n = icodeP[1] & 0xf;
	uint16_t maddr = C16x_ReadGpr16(n);
	uint16_t op2;
	if (!check_condition(icodeP[1])) {
		return;
	}
	op2 = C16x_MemRead16(maddr);
	REG_SP = REG_SP - 2;
	C16x_MemWrite16(REG_IP, REG_SP);
	REG_IP = op2;
}

/*
 * ------------------------------------------------
 * ASHR Rwn Rwm
 * v1
 * ------------------------------------------------
 */
void
c16x_ashr_rw_rw(uint8_t * icodeP)
{
	int n = (icodeP[1] & 0xf0) >> 4;
	int m = (icodeP[1] & 0xf);
	int16_t op1, shift;
	int16_t result;
	op1 = C16x_ReadGpr16(n);
	shift = C16x_ReadGpr16(m) & 0xf;
	REG_PSW &= ~(PSW_FLAG_E | PSW_FLAG_Z | PSW_FLAG_V | PSW_FLAG_C | PSW_FLAG_N);
	if (shift) {
		result = (op1 >> shift);
		if ((result << shift) != op1) {
			REG_PSW |= PSW_FLAG_V;
		}
		if (op1 & (1 << (shift - 1))) {
			REG_PSW |= PSW_FLAG_C;
		}
	} else {
		result = op1;
	}
	C16x_SetGpr16(result, n);
	if (result & 0x8000) {
		REG_PSW |= PSW_FLAG_N;
	}
	if (result == 0) {
		REG_PSW |= PSW_FLAG_Z;
	}
}

/*
 * ---------------------------------------------------------------
 * CMPD2 Rw data4
 * v1
 * ---------------------------------------------------------------
 */
void
c16x_cmpd2_rw_data4(uint8_t * icodeP)
{
	int n = icodeP[1] & 0xf;
	uint16_t op1 = C16x_ReadGpr16(n);
	uint16_t op2 = (icodeP[1] & 0xf0) >> 4;
	uint16_t result;
	result = op1 - op2;
	C16x_SetGpr16(op1 - 2, n);
	REG_PSW &= ~(PSW_FLAG_E | PSW_FLAG_Z | PSW_FLAG_V | PSW_FLAG_C | PSW_FLAG_N);
	if (result == 0x8000) {
		REG_PSW |= PSW_FLAG_E;
	}
	if (result == 0) {
		REG_PSW |= PSW_FLAG_Z;
	}
	if (result & 0x8000) {
		REG_PSW |= PSW_FLAG_N;
	}
	if (sub_carry(op1, op2, result)) {
		REG_PSW |= PSW_FLAG_C;
	}
	if (sub_overflow(op1, op2, result)) {
		REG_PSW |= PSW_FLAG_V;
	}
}

/*
 * ----------------------------
 * CPLB Rb
 * ----------------------------
 */
void
c16x_cplb_rb(uint8_t * icodeP)
{
	int n = (icodeP[1] & 0xf0) >> 4;
	uint8_t op1 = C16x_ReadGpr8(n);
	uint8_t result = ~op1;
	C16x_SetGpr8(result, n);
	REG_PSW &= ~(PSW_FLAG_E | PSW_FLAG_Z | PSW_FLAG_V | PSW_FLAG_C | PSW_FLAG_N);
	if (result == 0x80) {
		REG_PSW |= PSW_FLAG_E;
	}
	if (result == 0) {
		REG_PSW |= PSW_FLAG_Z;
	}
	if (result & 0x80) {
		REG_PSW |= PSW_FLAG_N;
	}
}

/*
 * --------------------------------------------------------
 * CMPD2 Rw mem
 * v0
 * --------------------------------------------------------
 */
void
c16x_cmpd2_rw_mem(uint8_t * icodeP)
{
	int n = icodeP[1] & 0xf;
	uint16_t op1 = C16x_ReadGpr16(n);
	uint16_t maddr = (icodeP[3] << 8) | icodeP[2];
	uint16_t op2 = C16x_MemRead16(maddr);
	uint16_t result;
	result = op1 - op2;
	C16x_SetGpr16(op1 - 2, n);
	REG_PSW &= ~(PSW_FLAG_E | PSW_FLAG_Z | PSW_FLAG_V | PSW_FLAG_C | PSW_FLAG_N);
	if (result == 0x8000) {
		REG_PSW |= PSW_FLAG_E;
	}
	if (result == 0) {
		REG_PSW |= PSW_FLAG_Z;
	}
	if (result & 0x8000) {
		REG_PSW |= PSW_FLAG_N;
	}
	if (sub_carry(op1, op2, result)) {
		REG_PSW |= PSW_FLAG_C;
	}
	if (sub_overflow(op1, op2, result)) {
		REG_PSW |= PSW_FLAG_V;
	}
}

/*
 * --------------------------------------------
 * MOVB mem [Rw]
 * v0
 * --------------------------------------------
 */
void
c16x_movb_mem__rw_(uint8_t * icodeP)
{
	int n = icodeP[1] & 0xf;
	uint16_t srcaddr = C16x_ReadGpr16(n);
	uint16_t dstaddr = icodeP[2] | (icodeP[3] << 8);
	uint16_t result = C16x_MemRead16(srcaddr);
	C16x_MemWrite16(result, dstaddr);

	REG_PSW &= ~(PSW_FLAG_E | PSW_FLAG_Z | PSW_FLAG_N);
	if (result == 0x8000) {
		REG_PSW |= PSW_FLAG_E;
	}
	if (result == 0) {
		REG_PSW |= PSW_FLAG_Z;
	}
	if (result & 0x8000) {
		REG_PSW |= PSW_FLAG_N;
	}
}

void
c16x_einit(uint8_t * icodeP)
{
	fprintf(stderr, "EINIT not implemented\n");
}

/*
 * ----------------------------------------------
 * CMPD2 rw data16
 * v1
 * ----------------------------------------------
 */
void
c16x_cmpd2_rw_data16(uint8_t * icodeP)
{
	int n = icodeP[1] & 0xf;
	uint16_t op1 = C16x_ReadGpr16(n);
	uint16_t data16 = (icodeP[3] << 8) | icodeP[2];
	uint16_t op2 = data16;
	uint16_t result;
	result = op1 - op2;
	REG_PSW &= ~(PSW_FLAG_E | PSW_FLAG_Z | PSW_FLAG_V | PSW_FLAG_C | PSW_FLAG_N);
	C16x_SetGpr16(op1 - 2, n);
	if (result == 0x8000) {
		REG_PSW |= PSW_FLAG_E;
	}
	if (result == 0) {
		REG_PSW |= PSW_FLAG_Z;
	}
	if (result & 0x8000) {
		REG_PSW |= PSW_FLAG_N;
	}
	if (sub_carry(op1, op2, result)) {
		REG_PSW |= PSW_FLAG_C;
	}
	if (sub_overflow(op1, op2, result)) {
		REG_PSW |= PSW_FLAG_V;
	}
}

/*
 * ---------------------------------------------
 * Software reset is a protected instruction
 * ---------------------------------------------
 */
void
c16x_srst(uint8_t * icodeP)
{
	// C16x_Reset
	fprintf(stderr, "Software reset not implemented\n");
}

/*
 * ----------------------------------------
 * MOV [Rwm] Rwn 
 * v0
 * ----------------------------------------
 */
void
c16x_mov__rw__rw(uint8_t * icodeP)
{
	int n = icodeP[1] >> 4;
	int m = icodeP[1] & 0xf;
	uint16_t dstaddr = C16x_ReadGpr16(m);
	uint16_t result = C16x_ReadGpr16(n);
	C16x_MemWrite16(result, dstaddr);

	REG_PSW &= ~(PSW_FLAG_E | PSW_FLAG_Z | PSW_FLAG_N);
	if (result == 0x8000) {
		REG_PSW |= PSW_FLAG_E;
	}
	if (result == 0) {
		REG_PSW |= PSW_FLAG_Z;
	}
	if (result & 0x8000) {
		REG_PSW |= PSW_FLAG_N;
	}
}

/*
 * --------------------------------------
 * MOVB [Rwm] Rbn
 * v0
 * --------------------------------------
 */
void
c16x_movb__rw__rb(uint8_t * icodeP)
{
	int n = icodeP[1] >> 4;
	int m = icodeP[1] & 0xf;
	uint16_t dstaddr = C16x_ReadGpr16(m);
	uint16_t result = C16x_ReadGpr8(n);
	C16x_MemWrite8(result, dstaddr);

	REG_PSW &= ~(PSW_FLAG_E | PSW_FLAG_Z | PSW_FLAG_N);
	if (result == 0x80) {
		REG_PSW |= PSW_FLAG_E;
	}
	if (result == 0) {
		REG_PSW |= PSW_FLAG_Z;
	}
	if (result & 0x80) {
		REG_PSW |= PSW_FLAG_N;
	}
}

/*
 * ------------------------------------------------
 * JNBS
 * v0
 * ------------------------------------------------
 */
void
c16x_jnbs_bitaddr_rel(uint8_t * icodeP)
{
	uint8_t bitaddr = icodeP[1];
	int bit = icodeP[3] >> 4;
	int8_t rel = icodeP[2];
	uint16_t bitfield;
	bitfield = C16x_ReadBitaddr(bitaddr);
	REG_PSW &= ~(PSW_FLAG_E | PSW_FLAG_Z | PSW_FLAG_V | PSW_FLAG_C | PSW_FLAG_N);
	if (!(bitfield & (1 << bit))) {
		REG_IP = REG_IP + rel + rel;
		bitfield |= (1 << bit);
		C16x_WriteBitaddr(bitfield, bitaddr);
		REG_PSW |= PSW_FLAG_Z;
	} else {
		REG_PSW |= PSW_FLAG_N;
	}
}

/*
 * --------------------------------------------
 * CALLR rel
 * v0
 * --------------------------------------------
 */
void
c16x_callr_rel(uint8_t * icodeP)
{
	int8_t rel = icodeP[1];
	REG_SP = REG_SP - 2;
	C16x_MemWrite16(REG_IP, REG_SP);
	REG_IP = REG_IP + rel + rel;
}

/*
 * -----------------------------------------
 * ASHR Rw data4
 * v0
 * -----------------------------------------
 */
void
c16x_ashr_rw_data4(uint8_t * icodeP)
{
	int n = icodeP[1] & 0xf;
	int16_t shift = (icodeP[1] & 0xf0) >> 4;
	int16_t op1;
	int16_t result;
	op1 = C16x_ReadGpr16(n);
	REG_PSW &= ~(PSW_FLAG_E | PSW_FLAG_Z | PSW_FLAG_V | PSW_FLAG_C | PSW_FLAG_N);
	if (shift) {
		result = (op1 >> shift);
		if ((result << shift) != op1) {
			REG_PSW |= PSW_FLAG_V;
		}
		if (op1 & (1 << (shift - 1))) {
			REG_PSW |= PSW_FLAG_C;
		}
	} else {
		result = op1;
	}
	if (result & 0x8000) {
		REG_PSW |= PSW_FLAG_N;
	}
	if (result == 0) {
		REG_PSW |= PSW_FLAG_Z;
	}
	C16x_SetGpr16(result, n);
}

/*
 * --------------------------------
 * MOVBZ Rwn Rbm
 * v1
 * --------------------------------
 */
void
c16x_movbz_rw_rb(uint8_t * icodeP)
{
	int n = icodeP[1] >> 4;
	int m = icodeP[1] & 0xf;
	uint8_t result = C16x_ReadGpr8(m);
	C16x_SetGpr16(result, n);
	REG_PSW &= ~(PSW_FLAG_E | PSW_FLAG_Z | PSW_FLAG_N);
	if (result == 0) {
		REG_PSW |= PSW_FLAG_Z;
	}
}

/*
 * ---------------------------------------------------
 * MOVBZ reg mem
 * v1
 * ---------------------------------------------------
 */

void
c16x_movbz_reg_mem(uint8_t * icodeP)
{
	uint8_t reg = icodeP[1];
	uint16_t srcaddr = icodeP[2] | (icodeP[3] << 8);
	uint8_t result = C16x_MemRead8(srcaddr);
	C16x_SetReg16(result, reg);
	REG_PSW &= ~(PSW_FLAG_E | PSW_FLAG_Z | PSW_FLAG_N);
	if (result == 0) {
		REG_PSW |= PSW_FLAG_Z;
	}
}

/*
 * ----------------------------------------------
 * MOV [Rwm+#data16] Rwn
 * v0
 * ----------------------------------------------
 */
void
c16x_mov__rwpdata16__rw(uint8_t * icodeP)
{
	int n = (icodeP[1] >> 4) & 0xf;
	int m = icodeP[1] & 0xf;
	uint16_t data16 = icodeP[2] | (icodeP[3] << 8);
	uint16_t result = C16x_ReadGpr16(n);
	uint16_t dstaddr = C16x_ReadGpr16(m) + data16;
	C16x_MemWrite16(result, dstaddr);

	REG_PSW &= ~(PSW_FLAG_E | PSW_FLAG_Z | PSW_FLAG_N);
	if (result == 0x8000) {
		REG_PSW |= PSW_FLAG_E;
	} else if (result == 0) {
		REG_PSW |= PSW_FLAG_Z;
	}
	if (result & 0x8000) {
		REG_PSW |= PSW_FLAG_N;
	}
}

/*
 * -----------------------------------------
 * MOVBZ mem reg
 * v1
 * -----------------------------------------
 */
void
c16x_movbz_mem_reg(uint8_t * icodeP)
{
	uint8_t reg = icodeP[1];
	uint16_t dstaddr = icodeP[2] | (icodeP[3] << 8);
	uint8_t result = C16x_ReadReg8(reg);
	C16x_MemWrite16(result, dstaddr);
	REG_PSW &= ~(PSW_FLAG_E | PSW_FLAG_Z | PSW_FLAG_N);
	if (result == 0) {
		REG_PSW |= PSW_FLAG_Z;
	}
}

/*
 * -------------------------------------------
 * SCXT reg data16
 * -------------------------------------------
 */
void
c16x_scxt_reg_data16(uint8_t * icodeP)
{
	uint8_t reg = icodeP[1];
	uint16_t data16 = icodeP[2] + (icodeP[3] << 8);
	uint16_t value = C16x_ReadReg16(reg);
	REG_SP = REG_SP - 2;
	C16x_MemWrite16(value, REG_SP);
	C16x_SetReg16(data16, reg);
}

/*
 * --------------------------------------
 * MOV [Rwn] [Rwm]
 * --------------------------------------
 */
void
c16x_mov__rw___rw_(uint8_t * icodeP)
{
	int n = icodeP[1] >> 4;
	int m = icodeP[1] & 0xf;
	uint16_t dstaddr = C16x_ReadGpr16(n);
	uint16_t srcaddr = C16x_ReadGpr16(m);
	uint16_t result = C16x_MemRead16(srcaddr);
	C16x_MemWrite16(result, dstaddr);

	REG_PSW &= ~(PSW_FLAG_E | PSW_FLAG_Z | PSW_FLAG_N);
	if (result == 0x8000) {
		REG_PSW |= PSW_FLAG_E;
	}
	if (result == 0) {
		REG_PSW |= PSW_FLAG_Z;
	}
	if (result & 0x8000) {
		REG_PSW |= PSW_FLAG_N;
	}
}

/*
 * -------------------------------------------------
 * MOVB [Rwn] [Rwm]
 * v0
 * -------------------------------------------------
 */

void
c16x_movb__rw___rw_(uint8_t * icodeP)
{
	int n = icodeP[1] >> 4;
	int m = icodeP[1] & 0xf;
	uint16_t dstaddr = C16x_ReadGpr16(n);
	uint16_t srcaddr = C16x_ReadGpr16(m);
	uint8_t result = C16x_MemRead8(srcaddr);
	C16x_MemWrite8(result, dstaddr);

	REG_PSW &= ~(PSW_FLAG_E | PSW_FLAG_Z | PSW_FLAG_N);
	if (result == 0x80) {
		REG_PSW |= PSW_FLAG_E;
	} else if (result == 0) {
		REG_PSW |= PSW_FLAG_Z;
	}
	if (result & 0x80) {
		REG_PSW |= PSW_FLAG_N;
	}
}

/*
 * --------------------------------------------
 * CALLA cc addr
 * --------------------------------------------
 */
void
c16x_calla_cc_addr(uint8_t * icodeP)
{
	uint16_t op2 = (icodeP[3] << 8) | icodeP[2];
	if (!check_condition(icodeP[1])) {
		return;
	}
	REG_SP = REG_SP - 2;
	C16x_MemWrite16(REG_IP, REG_SP);
	REG_IP = op2;
}

/*
 * ---------------------------------------
 * RET
 * v1
 * ---------------------------------------
 */
void
c16x_ret(uint8_t * icodeP)
{
	REG_IP = C16x_MemRead16(REG_SP);
	REG_SP = REG_SP + 2;
}

/*
 * ----------------------------------------
 * NOP
 * v1
 * ----------------------------------------
 */
void
c16x_nop(uint8_t * icodeP)
{
	/* nothing */
}

/*
 * ----------------------------------------
 * MOVBS Rwn Rbm
 * ----------------------------------------
 */
void
c16x_movbs_rw_rb(uint8_t * icodeP)
{
	int n = icodeP[1] >> 4;
	int m = icodeP[1] & 0xf;
	int16_t result = (int8_t) C16x_ReadGpr8(m);
	C16x_SetGpr16(result, n);
	REG_PSW &= ~(PSW_FLAG_E | PSW_FLAG_Z | PSW_FLAG_N);
	if (result == 0) {
		REG_PSW |= PSW_FLAG_Z;
	}
	if (result & 0x8000) {
		REG_PSW |= PSW_FLAG_N;
	}
}

/*
 * --------------------------------------------------
 * ATOMIC + EXTR irang2 (both instruction code D1)
 * v0 
 * --------------------------------------------------
 */
void
c16x_atomic_extr_irang2(uint8_t * icodeP)
{
	int irang2 = (((icodeP[1] & 0x30) >> 4) & 3) + 1;
	int instr = (icodeP[1] & 0xc0) >> 6;
	if (instr == 0) {	/* ATOMIC instruction */
		gc16x.lock_counter = irang2;
	} else if (instr == 2) {	/* EXTR instruction */
		gc16x.lock_counter = irang2;
		/* causes all register and bitoff and bitattr to go to esfr */
		gc16x.extmode |= EXTMODE_ESFR;
	}
}

/*
 * -----------------------------------------------------
 * MOVBS reg mem
 * v0
 * -----------------------------------------------------
 */

void
c16x_movbs_reg_mem(uint8_t * icodeP)
{
	uint8_t reg = icodeP[1];
	uint16_t srcaddr = icodeP[2] | (icodeP[3] << 8);
	int16_t result = (int8_t) C16x_MemRead16(srcaddr);
	C16x_SetReg16(result, reg);
	REG_PSW &= ~(PSW_FLAG_E | PSW_FLAG_Z | PSW_FLAG_N);
	if (result == 0) {
		REG_PSW |= PSW_FLAG_Z;
	}
	if (result & 0x8000) {
		REG_PSW |= PSW_FLAG_N;
	}
}

/*
 * --------------------------------------------------
 * MOV Rwn [Rwm+data16]
 * --------------------------------------------------
 */
void
c16x_mov_rw__rwpdata16_(uint8_t * icodeP)
{
	int n = (icodeP[1] >> 4) & 0xf;
	int m = icodeP[1] & 0xf;
	uint16_t data16 = icodeP[2] | (icodeP[3] << 8);
	uint16_t srcaddr = C16x_ReadGpr16(m) + data16;
	uint16_t result = C16x_MemRead16(srcaddr);
	C16x_SetGpr16(result, n);

	REG_PSW &= ~(PSW_FLAG_E | PSW_FLAG_Z | PSW_FLAG_N);
	if (result == 0x8000) {
		REG_PSW |= PSW_FLAG_E;
	}
	if (result == 0) {
		REG_PSW |= PSW_FLAG_Z;
	}
	if (result & 0x8000) {
		REG_PSW |= PSW_FLAG_N;
	}
}

/*
 * -------------------------------------------------------------
 * MOVBS mem reg
 * v1
 * -------------------------------------------------------------
 */
void
c16x_movbs_mem_reg(uint8_t * icodeP)
{
	uint8_t reg = icodeP[1];
	int dstaddr = icodeP[2] | (icodeP[3] << 8);
	int16_t result = (int8_t) C16x_ReadReg16(reg);
	C16x_MemWrite16(result, dstaddr);
	REG_PSW &= ~(PSW_FLAG_E | PSW_FLAG_Z | PSW_FLAG_N);
	if (result == 0) {
		REG_PSW |= PSW_FLAG_Z;
	}
	if (result & 0x8000) {
		REG_PSW |= PSW_FLAG_N;
	}

}

/*
 * ----------------------------------------------------
 * SCXT reg mem
 * v0
 * ----------------------------------------------------
 */
void
c16x_scxt_reg_mem(uint8_t * icodeP)
{
	uint8_t reg = icodeP[1];
	uint16_t maddr = icodeP[2] + (icodeP[3] << 8);
	uint16_t tmp1 = C16x_ReadReg16(reg);
	uint16_t tmp2 = C16x_MemRead16(maddr);
	REG_SP = REG_SP - 2;
	C16x_MemWrite16(tmp1, REG_SP);
	C16x_SetReg16(tmp2, reg);
}

/* 
 * -------------------------------------------------------------------
 * Instruction 0xd7  (EXTP EXTPR EXTS EXTSR)
 *	The documentation does not tell what happens on reading the
 *	dpp register during a exts sequence
 * v0
 * -------------------------------------------------------------------
 */
void
c16x_extp_exts_p10(uint8_t * icodeP)
{
	C16x *c16x = &gc16x;
	int irang = ((icodeP[1] & 0x30) >> 4) + 1;
	int instr = ((icodeP[1] & 0xc0) >> 6);
	c16x->lock_counter = irang;
	if (instr == 0) {	/* exts */
		/* Overrides the standard dpp and indirect addressing modes for 
		   some cycles */
		uint8_t seg = icodeP[2];
		c16x->extmode |= EXTMODE_SEG;
		c16x->extmode &= ~EXTMODE_PAGE;
		c16x->extaddr = seg << 16;
	} else if (instr == 2) {	/* extsr */
		/* Same like exts but additionally use esfr */
		uint8_t seg = icodeP[2];
		c16x->extmode |= EXTMODE_SEG | EXTMODE_ESFR;
		c16x->extmode &= ~EXTMODE_PAGE;
		c16x->extaddr = seg << 16;
	} else if (instr == 1) {	/* extp  */
		/* switch datapage for some instructions */
		uint16_t datapage = icodeP[2] | ((icodeP[3] & 3) << 8);
		c16x->extmode |= EXTMODE_PAGE;
		c16x->extmode &= ~EXTMODE_SEG;
		c16x->extaddr = datapage << 14;
		/* make backup of current data page */
	} else if (instr == 3) {	/* extpr */
		/* additionally switch to esfr */
		uint16_t datapage = icodeP[2] | ((icodeP[3] & 3) << 8);
		c16x->extmode |= EXTMODE_PAGE | EXTMODE_ESFR;
		c16x->extmode &= ~EXTMODE_SEG;
		c16x->extaddr = datapage << 14;
	}
	fprintf(stderr, "extp exts p10 not implemented\n");
}

/*
 * ---------------------------------------------------
 * MOV [Rwn+] [Rwm]
 * ---------------------------------------------------
 */
void
c16x_mov__rwp___rw_(uint8_t * icodeP)
{
	int n = icodeP[1] >> 4;
	int m = icodeP[1] & 0xf;
	uint16_t dstaddr = C16x_ReadGpr16(n);
	uint16_t srcaddr = C16x_ReadGpr16(m);
	uint16_t result = C16x_MemRead16(srcaddr);
	C16x_MemWrite16(result, dstaddr);
	C16x_SetGpr16(dstaddr + 2, n);
	REG_PSW &= ~(PSW_FLAG_E | PSW_FLAG_Z | PSW_FLAG_N);
	if (result == 0x8000) {
		REG_PSW |= PSW_FLAG_E;
	}
	if (result == 0) {
		REG_PSW |= PSW_FLAG_Z;
	}
	if (result & 0x8000) {
		REG_PSW |= PSW_FLAG_N;
	}

}

/* 
 * -----------------------------------------------------
 * MOVB [Rwn+] [Rwm]
 * v0
 * -----------------------------------------------------
 */
void
c16x_movb__rwp___rw_(uint8_t * icodeP)
{
	int n = (icodeP[1] & 0xf0) >> 4;
	int m = icodeP[1] & 0xf;
	uint16_t dstaddr = C16x_ReadGpr16(n);
	uint16_t srcaddr = C16x_ReadGpr16(m);
	uint8_t result = C16x_MemRead16(srcaddr);
	C16x_MemWrite8(result, dstaddr);
	C16x_SetGpr16(dstaddr + 1, n);
	C16x_MemWrite8(result, dstaddr);
	REG_PSW &= ~(PSW_FLAG_E | PSW_FLAG_Z | PSW_FLAG_N);
	if (result == 0x80) {
		REG_PSW |= PSW_FLAG_E;
	} else if (result == 0) {
		REG_PSW |= PSW_FLAG_Z;
	}
	if (result & 0x80) {
		REG_PSW |= PSW_FLAG_N;
	}
}

/*
 * ---------------------------------------------
 * CALLS seg caddr
 * v1
 * ---------------------------------------------
 */
void
c16x_calls_seg_caddr(uint8_t * icodeP)
{

	uint8_t seg = icodeP[1];
	uint16_t addr = (icodeP[3] << 8) | icodeP[2];
	REG_SP = REG_SP - 2;
	C16x_MemWrite16(REG_CSP, REG_SP);
	REG_SP = REG_SP - 2;
	C16x_MemWrite16(REG_IP, REG_SP);
	REG_CSP = seg;
	REG_IP = addr;
	fprintf(stderr, "calls sp %08x\n", REG_SP);
}

/*
 * ----------------------------------------------------
 * RETS
 * v1	
 * ----------------------------------------------------
 */
void
c16x_rets(uint8_t * icodeP)
{
	fprintf(stderr, "rets sp %04x\n", REG_SP);
	REG_IP = C16x_MemRead16(REG_SP);
	fprintf(stderr, "newIP %04x\n", REG_IP);
	REG_SP = REG_SP + 2;
	REG_CSP = C16x_MemRead16(REG_SP);
	fprintf(stderr, "newCSP %04x\n", REG_CSP);
	REG_SP = REG_SP + 2;
}

/*
 * -------------------------------
 * EXTP EXTS Rwm irang2 
 * v0
 * -------------------------------
 */
void
c16x_extp_exts_rwirang(uint8_t * icodeP)
{
	C16x *c16x = &gc16x;
	int irang = ((icodeP[1] & 0x30) >> 4) + 1;
	int m = icodeP[1] & 0xf;
	c16x->lock_counter = irang;
	if ((icodeP[1] & 0xc0) == 0) {
		/* exts irang */
		uint16_t seg = C16x_ReadGpr16(m);
		c16x->extmode |= EXTMODE_SEG;
		c16x->extmode &= ~EXTMODE_PAGE;
		c16x->extaddr = seg << 16;
	} else if ((icodeP[1] & 0xc0) == 0x40) {
		/* extp rwirang */
		uint16_t datapage = C16x_ReadGpr16(m);
		c16x->extmode |= EXTMODE_PAGE;
		c16x->extmode &= ~EXTMODE_SEG;
		c16x->extaddr = datapage << 14;
	}
	fprintf(stderr, "exts extp rwirang\n");
}

/*
 * -----------------------------------------------
 * MOV rw data4
 * v1
 * -----------------------------------------------
 */
void
c16x_mov_rw_data4(uint8_t * icodeP)
{
	uint16_t result = icodeP[1] >> 4;
	int n = icodeP[1] & 0xf;
	C16x_SetGpr16(result, n);
	REG_PSW &= ~(PSW_FLAG_E | PSW_FLAG_Z | PSW_FLAG_N);
	if (result == 0) {
		REG_PSW |= PSW_FLAG_Z;
	}

}

/*
 * -------------------------------------------------
 * MOV rb data4
 * v1
 * -------------------------------------------------
 */
void
c16x_mov_rb_data4(uint8_t * icodeP)
{
	uint8_t result = icodeP[1] >> 4;
	int n = icodeP[1] & 0xf;
	C16x_SetGpr8(result, n);
	REG_PSW &= ~(PSW_FLAG_E | PSW_FLAG_Z | PSW_FLAG_N);
	if (result == 0) {
		REG_PSW |= PSW_FLAG_Z;
	}
}

/*
 * ------------------------------------------------------
 * PCALL reg caddr
 * v0
 * ------------------------------------------------------
 */
void
c16x_pcall_reg_caddr(uint8_t * icodeP)
{
	uint8_t reg = icodeP[1];
	uint16_t maddr = icodeP[2] | (icodeP[3] << 8);
	uint16_t op1 = C16x_ReadReg16(reg);
	uint16_t op2;
	uint16_t tmp;
	uint16_t result;
	op2 = C16x_MemRead16(maddr);
	result = tmp = op1;
	REG_SP = REG_SP - 2;
	C16x_MemWrite16(tmp, REG_SP);
	REG_SP = REG_SP - 2;
	C16x_MemWrite16(REG_IP, REG_SP);
	REG_IP = op2;
	if (result == 0x8000) {
		REG_PSW |= PSW_FLAG_E;
	}
	if (result & 0x8000) {
		REG_PSW |= PSW_FLAG_N;
	}
	if (result == 0) {
		REG_PSW |= PSW_FLAG_Z;
	}
}

/*
 * -----------------------------------------------
 * MOVB [Rwm+data16] Rbn
 * v0
 * -----------------------------------------------
 */
void
c16x_movb__rwpdata16__rb(uint8_t * icodeP)
{
	int n = icodeP[1] >> 4;
	int m = icodeP[1] & 0xf;
	uint16_t data16 = icodeP[2] | (icodeP[3] << 8);
	uint8_t result = C16x_ReadGpr8(n);
	uint16_t dstaddr = C16x_ReadGpr16(m) + data16;
	C16x_MemWrite16(result, dstaddr);
	REG_PSW &= ~(PSW_FLAG_E | PSW_FLAG_Z | PSW_FLAG_N);
	if (result == 0x80) {
		REG_PSW |= PSW_FLAG_E;
	}
	if (result & 0x80) {
		REG_PSW |= PSW_FLAG_N;
	}
	if (result == 0) {
		REG_PSW |= PSW_FLAG_Z;
	}
}

/*
 * -----------------------------------------------
 * MOV reg data16
 * v0
 * -----------------------------------------------
 */
void
c16x_mov_reg_data16(uint8_t * icodeP)
{
	uint8_t reg = icodeP[1];
	uint16_t data16 = icodeP[2] | (icodeP[3] << 8);
	uint16_t result = data16;
	C16x_SetReg16(result, reg);
	REG_PSW &= ~(PSW_FLAG_E | PSW_FLAG_Z | PSW_FLAG_N);
	if (result == 0x8000) {
		REG_PSW |= PSW_FLAG_E;
	}
	if (result & 0x8000) {
		REG_PSW |= PSW_FLAG_N;
	}
	if (result == 0) {
		REG_PSW |= PSW_FLAG_Z;
	}
}

/*
 * -------------------------------------------
 * MOVB reg data8
 * v0
 * -------------------------------------------
 */

void
c16x_movb_reg_data8(uint8_t * icodeP)
{
	uint8_t reg = icodeP[1];
	uint8_t data8 = icodeP[2];
	uint8_t result = data8;
	C16x_SetReg8(result, reg);
	REG_PSW &= ~(PSW_FLAG_E | PSW_FLAG_Z | PSW_FLAG_N);
	if (result == 0x80) {
		REG_PSW |= PSW_FLAG_E;
	}
	if (result & 0x80) {
		REG_PSW |= PSW_FLAG_N;
	}
	if (result == 0) {
		REG_PSW |= PSW_FLAG_Z;
	}
}

/*
 * --------------------------------------------------------
 * MOV [Rwn] [Rwm+]
 * v0
 * --------------------------------------------------------
 */
void
c16x_mov__rw___rwp_(uint8_t * icodeP)
{
	int n = icodeP[1] >> 4;
	int m = icodeP[1] & 0xf;
	uint16_t dstaddr = C16x_ReadGpr16(n);
	uint16_t srcaddr = C16x_ReadGpr16(m);
	uint16_t result = C16x_MemRead16(srcaddr);
	C16x_SetGpr16(srcaddr + 2, m);
	C16x_MemWrite16(result, dstaddr);
	REG_PSW &= ~(PSW_FLAG_E | PSW_FLAG_Z | PSW_FLAG_N);
	if (result == 0x8000) {
		REG_PSW |= PSW_FLAG_E;
	}
	if (result == 0) {
		REG_PSW |= PSW_FLAG_Z;
	}
	if (result & 0x8000) {
		REG_PSW |= PSW_FLAG_N;
	}

}

/*
 * ---------------------------------------
 * MOVB [Rwn] [Rwm+]
 * v1
 * ---------------------------------------
 */
void
c16x_movb__rw___rwp_(uint8_t * icodeP)
{
	int n = icodeP[1] >> 4;
	int m = icodeP[1] & 0xf;
	uint16_t dstaddr = C16x_ReadGpr16(n);
	uint16_t srcaddr = C16x_ReadGpr16(m);
	uint8_t result = C16x_MemRead16(srcaddr);
	C16x_SetGpr16(srcaddr + 1, m);
	C16x_MemWrite8(result, dstaddr);
	REG_PSW &= ~(PSW_FLAG_E | PSW_FLAG_Z | PSW_FLAG_N);
	if (result == 0x80) {
		REG_PSW |= PSW_FLAG_E;
	}
	if (result == 0) {
		REG_PSW |= PSW_FLAG_Z;
	}
	if (result & 0x80) {
		REG_PSW |= PSW_FLAG_N;
	}
}

/*
 * ------------------------------------------
 * JMPA cc cardr
 * v1
 * ------------------------------------------
 */
void
c16x_jmpa_cc_caddr(uint8_t * icodeP)
{
	uint16_t jmpaddr;
	if (check_condition(icodeP[1])) {
		jmpaddr = icodeP[2] | (icodeP[3] << 8);
		REG_IP = jmpaddr;
	}
}

/*
 * -------------------------------------------
 * RETP reg
 * v0
 * -------------------------------------------
 */
void
c16x_retp_reg(uint8_t * icodeP)
{
	uint8_t reg = icodeP[1];
	uint16_t result;
	REG_IP = C16x_MemRead16(REG_SP);
	REG_SP = REG_SP + 2;
	result = C16x_MemRead16(REG_SP);
	REG_SP = REG_SP + 2;
	C16x_SetReg16(result, reg);
	REG_PSW &= ~(PSW_FLAG_E | PSW_FLAG_Z | PSW_FLAG_N);
	if (result == 0x8000) {
		REG_PSW |= PSW_FLAG_E;
	}
	if (result == 0) {
		REG_PSW |= PSW_FLAG_Z;
	}
	if (result & 0x8000) {
		REG_PSW |= PSW_FLAG_N;
	}
}

/*
 * -----------------------------------------
 * PUSH reg
 * v0
 * -----------------------------------------
 */
void
c16x_push_reg(uint8_t * icodeP)
{
	uint8_t reg = icodeP[1];
	uint16_t value = C16x_ReadReg16(reg);
	REG_SP = REG_SP - 2;
	C16x_MemWrite16(value, REG_SP);
}

/*
 * ----------------------------------------------
 * MOV Rwn Rwm
 * ----------------------------------------------
 */
void
c16x_mov_rw_rw(uint8_t * icodeP)
{
	int n = icodeP[1] >> 4;
	int m = icodeP[1] & 0xf;
	uint16_t op2 = C16x_ReadGpr16(m);
	uint16_t result = op2;
	C16x_SetGpr16(result, n);

	REG_PSW &= ~(PSW_FLAG_E | PSW_FLAG_Z | PSW_FLAG_N);
	if (result == 0x8000) {
		REG_PSW |= PSW_FLAG_E;
	}
	if (result == 0) {
		REG_PSW |= PSW_FLAG_Z;
	}
	if (result & 0x8000) {
		REG_PSW |= PSW_FLAG_N;
	}
}

/*
 * --------------------------
 * MOVB Rbn Rbm
 * --------------------------
 */

void
c16x_movb_rb_rb(uint8_t * icodeP)
{
	int n = icodeP[1] >> 4;
	int m = icodeP[1] & 0xf;
	uint8_t result = C16x_ReadGpr8(m);
	C16x_SetGpr8(result, n);

	REG_PSW &= ~(PSW_FLAG_E | PSW_FLAG_Z | PSW_FLAG_N);
	if (result == 0x80) {
		REG_PSW |= PSW_FLAG_E;
	}
	if (result == 0) {
		REG_PSW |= PSW_FLAG_Z;
	}
	if (result & 0x80) {
		REG_PSW |= PSW_FLAG_N;
	}
}

/*
 * -----------------------------------------
 * MOV reg mem
 * v0
 * -----------------------------------------
 */
void
c16x_mov_reg_mem(uint8_t * icodeP)
{
	uint8_t reg = icodeP[1];
	uint16_t maddr = icodeP[2] | (icodeP[3] << 8);
	uint16_t result = C16x_MemRead16(maddr);
	C16x_SetReg16(result, reg);
	REG_PSW &= ~(PSW_FLAG_E | PSW_FLAG_Z | PSW_FLAG_N);
	if (result == 0x8000) {
		REG_PSW |= PSW_FLAG_E;
	} else if (result == 0) {
		REG_PSW |= PSW_FLAG_Z;
	}
	if (result & 0x8000) {
		REG_PSW |= PSW_FLAG_N;
	}
}

/*
 * -----------------------------------------
 * MOVB reg mem
 * v0
 * -----------------------------------------
 */
void
c16x_movb_reg_mem(uint8_t * icodeP)
{
	uint8_t reg = icodeP[1];
	uint16_t maddr = icodeP[2] | (icodeP[3] << 8);
	uint8_t result = C16x_MemRead8(maddr);
	C16x_SetReg8(result, reg);
	REG_PSW &= ~(PSW_FLAG_E | PSW_FLAG_Z | PSW_FLAG_N);
	if (result == 0x80) {
		REG_PSW |= PSW_FLAG_E;
	}
	if (result == 0) {
		REG_PSW |= PSW_FLAG_Z;
	}
	if (result & 0x80) {
		REG_PSW |= PSW_FLAG_N;
	}
}

/*
 * ------------------------------------------------
 * MOVB Rbn [Rwm+data16]
 * v0
 * ------------------------------------------------
 */

void
c16x_movb_rb__rwpdata16_(uint8_t * icodeP)
{
	int n = icodeP[1] >> 4;
	int m = icodeP[1] & 0xf;
	uint16_t data16 = icodeP[2] | (icodeP[3] << 8);
	uint16_t srcaddr = C16x_ReadGpr16(m) + data16;
	uint8_t result = C16x_MemRead8(srcaddr);
	C16x_SetGpr8(result, n);
	REG_PSW &= ~(PSW_FLAG_E | PSW_FLAG_Z | PSW_FLAG_N);
	if (result == 0x80) {
		REG_PSW |= PSW_FLAG_E;
	} else if (result == 0) {
		REG_PSW |= PSW_FLAG_Z;
	}
	if (result & 0x80) {
		REG_PSW |= PSW_FLAG_N;
	}

}

/*
 * ---------------------------------------
 * MOV mem reg
 * ---------------------------------------
 */
void
c16x_mov_mem_reg(uint8_t * icodeP)
{
	uint8_t reg = icodeP[1];
	uint16_t maddr = icodeP[2] | (icodeP[3] << 8);
	uint16_t result = C16x_ReadReg16(reg);
	C16x_MemWrite16(result, maddr);
	REG_PSW &= ~(PSW_FLAG_E | PSW_FLAG_Z | PSW_FLAG_N);
	if (result == 0x8000) {
		REG_PSW |= PSW_FLAG_E;
	} else if (result == 0) {
		REG_PSW |= PSW_FLAG_Z;
	}
	if (result & 0x8000) {
		REG_PSW |= PSW_FLAG_N;
	}
}

/*
 * -------------------------------------------
 * MOVB mem reg 
 * v0
 * -------------------------------------------
 */

void
c16x_movb_mem_reg(uint8_t * icodeP)
{
	uint8_t reg = icodeP[1];
	uint16_t maddr = icodeP[2] | (icodeP[3] << 8);
	uint8_t result = C16x_ReadReg8(reg);
	C16x_MemWrite8(result, maddr);
	REG_PSW &= ~(PSW_FLAG_E | PSW_FLAG_Z | PSW_FLAG_N);
	if (result == 0x80) {
		REG_PSW |= PSW_FLAG_E;
	} else if (result == 0) {
		REG_PSW |= PSW_FLAG_Z;
	}
	if (result & 0x80) {
		REG_PSW |= PSW_FLAG_N;
	}
}

/*
 * -----------------------------------------------
 * Inter segment jump
 * v0
 * -----------------------------------------------
 */
void
c16x_jmps_seg_caddr(uint8_t * icodeP)
{
	uint8_t op1 = icodeP[1];
	uint16_t op2 = icodeP[2] | (icodeP[3] << 8);
	REG_CSP = (REG_CSP & ~0xff) | op1;
	REG_IP = op2;
}

/*
 * ----------------------------------------------
 * RETI
 * v1	
 * ----------------------------------------------
 */
void
c16x_reti(uint8_t * icodeP)
{
	if (icodeP[1] != 0x88) {
		fprintf(stderr, "Bug: Not a RETI instruction\n");
		return;
	}
	REG_IP = C16x_MemRead16(REG_SP);
	REG_SP += 2;
	if (SYSCON_SGTDIS == 0) {
		REG_CSP = C16x_MemRead16(REG_SP);
		REG_SP += 2;
	}
	REG_PSW = C16x_MemRead16(REG_SP);
	REG_SP += 2;
}

/*
 * ----------------------------------------
 * POP reg
 * v0
 * ----------------------------------------
 */
void
c16x_pop_reg(uint8_t * icodeP)
{
	uint16_t op1 = icodeP[1];
	C16x_SetReg16(C16x_MemRead16(REG_SP), op1);
	REG_SP += 2;
}

void
c16x_illegal_opcode(uint8_t * icodeP)
{
	fprintf(stderr, "Illegal opcode 0x%02x\n", icodeP[0]);
}

void
C16x_InitInstructions()
{
	fprintf(stderr, "Init instructions\n");
	init_condition_map();
}
