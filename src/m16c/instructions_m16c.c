 /* 
  ************************************************************************************************ 
  *
  * M16C Instruction Set 
  *  
  * State: Working but buggy.
  *
  * Copyright 2009/2010 Jochen Karrer. All rights reserved.
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
  ************************************************************************************************ 
  */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "m16c_cpu.h"

#define ISNEGL(x) ((x)&(1<<31))
#define ISNOTNEGL(x) (!((x)&(1<<31)))
#define ISPOSL(x) (-(x) & (1<<31))

#define ISNEGW(x) ((x)&(1<<15))
#define ISNOTNEGW(x) (!((x)&(1<<15)))
#define ISPOSW(x) (-(x) & (1<<15))

#define ISNEGB(x) ((x)&(1<<7))
#define ISNOTNEGB(x) (!((x)&(1<<7)))
#define ISPOSB(x) (-(x) & (1<<7))
#define GAM_ALL 0xffffffff

#define dbgprintf(x...)

typedef void GAM_SetProc(uint32_t value, int datalen);
typedef void GAM_GetProc(uint32_t * value, int datalen);

static inline uint16_t
bcd_to_uint16(uint16_t s)
{
	return (s & 0xf) + 10 * ((s >> 4) & 0xf) + 100 * ((s >> 8) & 0xf) +
	    1000 * ((s >> 12) & 0xf);
}

static inline uint16_t
uint16_to_bcd(uint16_t u)
{
	unsigned int i;
	unsigned int digit = 0;
	uint16_t bcd = 0;
	for (i = 0; i < 4; i++) {
		digit = u % 10;
		bcd |= (digit << (i * 4));
		u = u / 10;
	}
	return bcd;
}

static inline uint16_t
add8_carry(uint8_t op1, uint8_t op2, uint8_t result)
{

	if (((ISNEGB(op1) && ISNEGB(op2))
	     || (ISNEGB(op1) && ISNOTNEGB(result))
	     || (ISNEGB(op2) && ISNOTNEGB(result)))) {
		return M16C_FLG_CARRY;
	} else {
		return 0;
	}

}

static inline uint16_t
add8_overflow(uint8_t op1, uint8_t op2, uint8_t result)
{
	if ((ISNEGB(op1) && ISNEGB(op2) && ISNOTNEGB(result))
	    || (ISNOTNEGB(op1) && ISNOTNEGB(op2) && ISNEGB(result))) {
		return M16C_FLG_OVERFLOW;
	} else {
		return 0;
	}
}

static inline uint16_t
add16_carry(uint16_t op1, uint16_t op2, uint16_t result)
{
	if (((ISNEGW(op1) && ISNEGW(op2))
	     || (ISNEGW(op1) && ISNOTNEGW(result))
	     || (ISNEGW(op2) && ISNOTNEGW(result)))) {
		return M16C_FLG_CARRY;
	} else {
		return 0;
	}
}

static inline uint16_t
add16_overflow(uint16_t op1, uint16_t op2, uint16_t result)
{
	if ((ISNEGW(op1) && ISNEGW(op2) && ISNOTNEGW(result))
	    || (ISNOTNEGW(op1) && ISNOTNEGW(op2) && ISNEGW(result))) {
		return M16C_FLG_OVERFLOW;
	} else {
		return 0;
	}
}

static inline void
add_flags(uint16_t op1, uint16_t op2, uint16_t result, int opsize)
{
	M16C_REG_FLG &= ~(M16C_FLG_OVERFLOW | M16C_FLG_SIGN | M16C_FLG_ZERO | M16C_FLG_CARRY);
	if (opsize == 2) {
		M16C_REG_FLG |= add16_carry(op1, op2, result);
		M16C_REG_FLG |= add16_overflow(op1, op2, result);
		if (result & 0x8000) {
			M16C_REG_FLG |= M16C_FLG_SIGN;
		} else if ((result & 0xffff) == 0) {
			M16C_REG_FLG |= M16C_FLG_ZERO;
		}
	} else {
		M16C_REG_FLG |= add8_carry(op1, op2, result);
		M16C_REG_FLG |= add8_overflow(op1, op2, result);
		if (result & 0x80) {
			M16C_REG_FLG |= M16C_FLG_SIGN;
		} else if ((result & 0xff) == 0) {
			M16C_REG_FLG |= M16C_FLG_ZERO;
		}
	}
}

/*
 * ---------------------------------------------------------
 * M16C has non borrow style carry like 6502
 * It subtracts by adding the complement. 
 * ---------------------------------------------------------
 */
static inline uint16_t
sub8_carry(uint16_t op1, uint16_t op2, uint16_t result)
{
	if (((ISNEGB(op1) && ISNOTNEGB(op2))
	     || (ISNEGB(op1) && ISNOTNEGB(result))
	     || (ISNOTNEGB(op2) && ISNOTNEGB(result)))) {
		return M16C_FLG_CARRY;
	} else {
		return 0;
	}
}

static inline uint16_t
sub16_carry(uint16_t op1, uint16_t op2, uint16_t result)
{
	if (((ISNEGW(op1) && ISNOTNEGW(op2))
	     || (ISNEGW(op1) && ISNOTNEGW(result))
	     || (ISNOTNEGW(op2) && ISNOTNEGW(result)))) {
		return M16C_FLG_CARRY;
	} else {
		return 0;
	}
}

static inline uint16_t
sub16_overflow(uint16_t op1, uint16_t op2, uint16_t result)
{
	if ((ISNEGW(op1) && ISNOTNEGW(op2) && ISNOTNEGW(result))
	    || (ISNOTNEGW(op1) && ISNEGW(op2) && ISNEGW(result))) {
		return M16C_FLG_OVERFLOW;
	} else {
		return 0;
	}
}

static inline uint16_t
sub8_overflow(uint8_t op1, uint8_t op2, uint8_t result)
{
	if ((ISNEGB(op1) && ISNOTNEGB(op2) && ISNOTNEGB(result))
	    || (ISNOTNEGB(op1) && ISNEGB(op2) && ISNEGB(result))) {
		return M16C_FLG_OVERFLOW;
	} else {
		return 0;
	}
}

static inline void
sub_flags(uint16_t op1, uint16_t op2, uint16_t result, int opsize)
{
	M16C_REG_FLG &= ~(M16C_FLG_OVERFLOW | M16C_FLG_SIGN | M16C_FLG_ZERO | M16C_FLG_CARRY);
	if (opsize == 2) {
		M16C_REG_FLG |= sub16_carry(op1, op2, result);
		M16C_REG_FLG |= sub16_overflow(op1, op2, result);
		if (result & 0x8000) {
			M16C_REG_FLG |= M16C_FLG_SIGN;
		} else if ((result & 0xffff) == 0) {
			M16C_REG_FLG |= M16C_FLG_ZERO;
		}
	} else {
		M16C_REG_FLG |= sub8_carry(op1, op2, result);
		M16C_REG_FLG |= sub8_overflow(op1, op2, result);
		if (result & 0x80) {
			M16C_REG_FLG |= M16C_FLG_SIGN;
		} else if ((result & 0xff) == 0) {
			M16C_REG_FLG |= M16C_FLG_ZERO;
		}
	}
}

/**
 *******************************************
 * Special to bmcnd with 8 bit condition
 *******************************************
 */
static int
check_condition_8bit(uint8_t cnd)
{
	int result = 0;
	int carry = 0, zero = 0, sign = 0, ovl = 0;
	if (M16C_REG_FLG & M16C_FLG_ZERO)
		zero = 1;
	if (M16C_REG_FLG & M16C_FLG_CARRY)
		carry = 1;
	if (M16C_REG_FLG & M16C_FLG_SIGN)
		sign = 1;
	if (M16C_REG_FLG & M16C_FLG_OVERFLOW)
		ovl = 1;
	switch (cnd) {
	    case 0:
		    /* GEU / C */
		    result = carry;
		    break;
	    case 1:
		    /* GTU */
		    result = (carry && !zero);
		    break;
	    case 2:
		    /* EQ / Z */
		    result = zero;
		    break;
	    case 3:
		    /* N */
		    result = sign;
		    break;
	    case 4:
		    /* LE */
		    result = (sign ^ ovl) || zero;
		    break;
	    case 5:
		    /* O */
		    result = ovl;
		    break;
	    case 6:
		    /* GE */
		    result = !(sign ^ ovl);
		    break;

	    case 0xf8:
		    /* LTU/NC */
		    result = !carry;
		    break;

	    case 0xf9:
		    /* LEU */
		    result = !(carry && !zero);
		    break;

	    case 0xfa:
		    /* NE/NZ */
		    result = !zero;
		    break;

	    case 0xfb:
		    /* PZ */
		    result = !sign;
		    break;

	    case 0xfc:
		    /* GT */
		    result = !((sign ^ ovl) || zero);
		    break;

	    case 0xfd:
		    /* NO */
		    result = !ovl;
		    break;

	    case 0xfe:
		    /* LT */
		    result = sign ^ ovl;
		    break;
	    default:
		    fprintf(stderr, "unknown condition %d\n", cnd);
		    exit(3474);

	}
	return result;
}

static int
check_condition(uint8_t cnd)
{
	int result = 0;
	int carry = 0, zero = 0, sign = 0, ovl = 0;
	if (M16C_REG_FLG & M16C_FLG_ZERO)
		zero = 1;
	if (M16C_REG_FLG & M16C_FLG_CARRY)
		carry = 1;
	if (M16C_REG_FLG & M16C_FLG_SIGN)
		sign = 1;
	if (M16C_REG_FLG & M16C_FLG_OVERFLOW)
		ovl = 1;
	switch (cnd & 0xf) {
	    case 0:
		    result = carry;
		    break;
	    case 1:
		    result = (carry && !zero);
		    break;
	    case 2:
		    result = zero;
		    break;
	    case 3:
		    result = sign;
		    break;
	    case 4:
		    result = !carry;
		    break;
	    case 5:		// LEU
		    result = !carry || zero;
		    break;
	    case 6:		// NE/NZ
		    result = !zero;
		    break;
	    case 7:
		    result = !sign;
		    break;
	    case 8:
		    //LE
		    result = (sign ^ ovl) || zero;
		    break;
	    case 9:
		    result = ovl;
		    break;
	    case 10:
		    result = !(sign ^ ovl);
		    break;
	    case 12:
		    result = !((sign ^ ovl) || zero);
		    break;

	    case 13:
		    result = !ovl;
		    break;

	    case 14:
		    result = sign ^ ovl;
		    break;
	    default:
		    fprintf(stderr, "unknown condition %d\n", cnd);
		    exit(3474);

	}
	return result;
}

/*
 * General addressing mode access procs
 */
static void
gam_set_r0l(uint32_t value, int datalen)
{
	M16C_REG_R0L = value;
}

static void
gam_get_r0l(uint32_t * value, int datalen)
{
	*value = M16C_REG_R0L;
}

static void
gam_set_r0(uint32_t value, int datalen)
{
	M16C_REG_R0 = value;
}

static void
gam_get_r0(uint32_t * value, int datalen)
{
	*value = M16C_REG_R0;
}

static void
gam_set_r2r0(uint32_t value, int datalen)
{
	M16C_REG_R2 = value >> 16;
	M16C_REG_R0 = value;
}

static void
gam_set_r0h(uint32_t value, int datalen)
{
	M16C_REG_R0H = value;
}

static void
gam_get_r0h(uint32_t * value, int datalen)
{
	*value = M16C_REG_R0H;
}

static void
gam_set_r1(uint32_t value, int datalen)
{
	M16C_REG_R1 = value;
}

static void
gam_set_r3r1(uint32_t value, int datalen)
{
	M16C_REG_R3 = value >> 16;
	M16C_REG_R1 = value;
}

static void
gam_get_r1(uint32_t * value, int datalen)
{
	*value = M16C_REG_R1;
}

static void
gam_set_r1l(uint32_t value, int datalen)
{
	M16C_REG_R1L = value;
}

static void
gam_get_r1l(uint32_t * value, int datalen)
{
	*value = M16C_REG_R1L;
}

static void
gam_set_r2(uint32_t value, int datalen)
{
	M16C_REG_R2 = value;
}

static void
gam_get_r2(uint32_t * value, int datalen)
{
	*value = M16C_REG_R2;
}

static void
gam_set_r1h(uint32_t value, int datalen)
{
	M16C_REG_R1H = value;
}

static void
gam_get_r1h(uint32_t * value, int datalen)
{
	*value = M16C_REG_R1H;
}

static void
gam_set_r3(uint32_t value, int datalen)
{
	M16C_REG_R3 = value;
}

static void
gam_get_r3(uint32_t * value, int datalen)
{
	*value = M16C_REG_R3;
}

static void
gam_set_a0(uint32_t value, int datalen)
{
	M16C_REG_A0 = value;
}

static void
gam_set_a1a0(uint32_t value, int datalen)
{
	M16C_REG_A1 = value >> 16;
	M16C_REG_A0 = value;
}

static void
gam_get_a0(uint32_t * value, int datalen)
{
	if (datalen == 1) {
		*value = M16C_REG_A0 & 0xff;
	} else if (datalen == 2) {
		*value = M16C_REG_A0 & 0xffff;
	} else {
		fprintf(stderr, "%s called with datalen %d\n", __FUNCTION__, datalen);
	}
}

static void
gam_set_a1(uint32_t value, int datalen)
{
	M16C_REG_A1 = value;
}

static void
gam_get_a1(uint32_t * value, int datalen)
{
	if (datalen == 1) {
		*value = M16C_REG_A1 & 0xff;
	} else if (datalen == 2) {
		*value = M16C_REG_A1 & 0xffff;
	} else {
		fprintf(stderr, "%s called with datalen %d\n", __FUNCTION__, datalen);
	}
}

static void
gam_set_ia0(uint32_t value, int datalen)
{
	uint32_t addr = M16C_REG_A0;
	switch (datalen) {
	    case 1:
		    M16C_Write8(value, addr);
		    break;
	    case 2:
		    M16C_Write16(value, addr);
		    break;
	    case 3:
		    M16C_Write24(value, addr);
		    break;
	    default:
		    dbgprintf("Bad datalen of %d bytes\n", datalen);
		    break;
	}
}

static void
gam_get_ia0(uint32_t * value, int datalen)
{
	uint32_t addr = M16C_REG_A0;
	switch (datalen) {
	    case 1:
		    *value = M16C_Read8(addr);
		    break;
	    case 2:
		    *value = M16C_Read16(addr);
		    break;
	    default:
		    dbgprintf("Bad datalen of %d bytes\n", datalen);
		    break;
	}
}

static void
gam_set_ia1(uint32_t value, int datalen)
{
	uint32_t addr = M16C_REG_A1;
	switch (datalen) {
	    case 1:
		    M16C_Write8(value, addr);
		    break;
	    case 2:
		    M16C_Write16(value, addr);
		    break;
	    case 3:
		    M16C_Write24(value, addr);
		    break;
	    default:
		    dbgprintf("Bad datalen of %d bytes\n", datalen);
		    break;
	}
}

static void
gam_get_ia1(uint32_t * value, int datalen)
{
	uint32_t addr = M16C_REG_A1;
	switch (datalen) {
	    case 1:
		    *value = M16C_Read8(addr);
		    break;
	    case 2:
		    *value = M16C_Read16(addr);
		    break;
	    default:
		    dbgprintf("Bad datalen of %d bytes\n", datalen);
		    break;
	}
}

static void
gam_set_dsp8ia0(uint32_t value, int datalen)
{
	uint32_t addr = M16C_REG_A0 + M16C_Read8(M16C_REG_PC);
	addr = addr & 0xfffff;
	switch (datalen) {
	    case 1:
		    M16C_Write8(value, addr);
		    break;
	    case 2:
		    M16C_Write16(value, addr);
		    break;
	    case 3:
		    M16C_Write24(value, addr);
		    break;
	    default:
		    dbgprintf("Bad datalen of %d bytes\n", datalen);
		    break;
	}
}

static void
gam_get_dsp8ia0(uint32_t * value, int datalen)
{
	uint32_t addr = M16C_REG_A0 + M16C_Read8(M16C_REG_PC);
	addr = addr & 0xfffff;
	switch (datalen) {
	    case 1:
		    *value = M16C_Read8(addr);
		    break;
	    case 2:
		    *value = M16C_Read16(addr);
		    break;
	    default:
		    dbgprintf("Bad datalen of %d bytes\n", datalen);
		    break;
	}
}

static void
gam_set_dsp8ia1(uint32_t value, int datalen)
{
	uint32_t addr = M16C_REG_A1 + M16C_Read8(M16C_REG_PC);
	addr = addr & 0xfffff;
	switch (datalen) {
	    case 1:
		    M16C_Write8(value, addr);
		    break;
	    case 2:
		    M16C_Write16(value, addr);
		    break;
	    case 3:
		    M16C_Write24(value, addr);
		    break;
	    default:
		    dbgprintf("Bad datalen of %d bytes\n", datalen);
		    break;
	}
}

static void
gam_get_dsp8ia1(uint32_t * value, int datalen)
{
	uint32_t addr = M16C_REG_A1 + M16C_Read8(M16C_REG_PC);
	addr = addr & 0xfffff;
	switch (datalen) {
	    case 1:
		    *value = M16C_Read8(addr);
		    break;
	    case 2:
		    *value = M16C_Read16(addr);
		    break;
	    default:
		    dbgprintf("Bad datalen of %d bytes\n", datalen);
		    break;
	}
}

static void
gam_set_dsp8isb(uint32_t value, int datalen)
{
	uint32_t addr = M16C_REG_SB + M16C_Read8(M16C_REG_PC);
	addr = addr & 0xfffff;
	switch (datalen) {
	    case 1:
		    M16C_Write8(value, addr);
		    break;
	    case 2:
		    M16C_Write16(value, addr);
		    break;
	    case 3:
		    M16C_Write24(value, addr);
		    break;
	    default:
		    dbgprintf("Bad datalen of %d bytes\n", datalen);
		    break;
	}
}

static void
gam_get_dsp8isb(uint32_t * value, int datalen)
{
	uint32_t addr = M16C_REG_SB + M16C_Read8(M16C_REG_PC);
	addr = addr & 0xfffff;
	switch (datalen) {
	    case 1:
		    *value = M16C_Read8(addr);
		    break;
	    case 2:
		    *value = M16C_Read16(addr);
		    break;
	    default:
		    dbgprintf("Bad datalen of %d bytes\n", datalen);
		    break;
	}
}

static void
gam_set_dsp8ifb(uint32_t value, int datalen)
{
	int8_t dsp8 = M16C_Read8(M16C_REG_PC);
	uint32_t addr = M16C_REG_FB + dsp8;
	addr = addr & 0xfffff;
	switch (datalen) {
	    case 1:
		    M16C_Write8(value, addr);
		    break;
	    case 2:
		    M16C_Write16(value, addr);
		    break;
	    case 3:
		    M16C_Write24(value, addr);
		    break;
	    default:
		    dbgprintf("Bad datalen of %d bytes\n", datalen);
		    break;
	}
}

static void
gam_get_dsp8ifb(uint32_t * value, int datalen)
{
	int8_t dsp8 = M16C_Read8(M16C_REG_PC);
	uint32_t addr = M16C_REG_FB + dsp8;
	addr = addr & 0xfffff;
	switch (datalen) {
	    case 1:
		    *value = M16C_Read8(addr);
		    break;
	    case 2:
		    *value = M16C_Read16(addr);
		    break;
	    default:
		    dbgprintf("Bad datalen of %d bytes\n", datalen);
		    break;
	}
}

static void
gam_set_dsp16ia0(uint32_t value, int datalen)
{
	uint32_t addr = M16C_REG_A0 + M16C_Read16(M16C_REG_PC);
	addr = addr & 0xfffff;
	switch (datalen) {
	    case 1:
		    M16C_Write8(value, addr);
		    break;
	    case 2:
		    M16C_Write16(value, addr);
		    break;
	    case 3:
		    M16C_Write24(value, addr);
		    break;
	    default:
		    dbgprintf("Bad datalen of %d bytes\n", datalen);
		    break;
	}
}

static void
gam_get_dsp20ia0(uint32_t * value, int datalen)
{
	uint32_t addr = M16C_REG_A0 + (M16C_Read24(M16C_REG_PC) & 0xfffff);
	addr = addr & 0xfffff;
	switch (datalen) {
	    case 1:
		    *value = M16C_Read8(addr);
		    break;
	    case 2:
		    *value = M16C_Read16(addr);
		    break;
	    default:
		    dbgprintf("Bad datalen of %d bytes\n", datalen);
		    break;
	}
}

static void
gam_get_dsp20ia1(uint32_t * value, int datalen)
{
	uint32_t addr = M16C_REG_A1 + (M16C_Read24(M16C_REG_PC) & 0xfffff);
	addr = addr & 0xfffff;
	switch (datalen) {
	    case 1:
		    *value = M16C_Read8(addr);
		    break;
	    case 2:
		    *value = M16C_Read16(addr);
		    break;
	    default:
		    dbgprintf("Bad datalen of %d bytes\n", datalen);
		    break;
	}
}

static void
gam_get_dsp16ia0(uint32_t * value, int datalen)
{
	uint32_t addr = M16C_REG_A0 + M16C_Read16(M16C_REG_PC);
	addr = addr & 0xfffff;
	switch (datalen) {
	    case 1:
		    *value = M16C_Read8(addr);
		    break;
	    case 2:
		    *value = M16C_Read16(addr);
		    break;
	    default:
		    dbgprintf("Bad datalen of %d bytes\n", datalen);
		    break;
	}
}

static void
gam_set_dsp16ia1(uint32_t value, int datalen)
{
	uint32_t addr = M16C_REG_A1 + M16C_Read16(M16C_REG_PC);
	addr = addr & 0xfffff;
	switch (datalen) {
	    case 1:
		    M16C_Write8(value, addr);
		    break;
	    case 2:
		    M16C_Write16(value, addr);
		    break;
	    case 3:
		    M16C_Write24(value, addr);
		    break;
	    default:
		    dbgprintf("Bad datalen of %d bytes\n", datalen);
		    break;
	}
}

static void
gam_get_dsp16ia1(uint32_t * value, int datalen)
{
	uint32_t addr = M16C_REG_A1 + M16C_Read16(M16C_REG_PC);
	addr = addr & 0xfffff;
	switch (datalen) {
	    case 1:
		    *value = M16C_Read8(addr);
		    break;
	    case 2:
		    *value = M16C_Read16(addr);
		    break;
	    default:
		    dbgprintf("Bad datalen of %d bytes\n", datalen);
		    break;
	}
}

static void
gam_set_dsp16isb(uint32_t value, int datalen)
{
	uint32_t addr = M16C_REG_SB + M16C_Read16(M16C_REG_PC);
	addr = addr & 0xfffff;
	switch (datalen) {
	    case 1:
		    M16C_Write8(value, addr);
		    break;
	    case 2:
		    M16C_Write16(value, addr);
		    break;
	    case 3:
		    M16C_Write24(value, addr);
		    break;
	    default:
		    dbgprintf("Bad datalen of %d bytes\n", datalen);
		    break;
	}
}

static void
gam_get_dsp16isb(uint32_t * value, int datalen)
{
	uint32_t addr = M16C_REG_SB + M16C_Read16(M16C_REG_PC);
	addr = addr & 0xfffff;
	switch (datalen) {
	    case 1:
		    *value = M16C_Read8(addr);
		    break;
	    case 2:
		    *value = M16C_Read16(addr);
		    break;
	    default:
		    dbgprintf("Bad datalen of %d bytes\n", datalen);
		    break;
	}
}

static void
gam_set_abs16(uint32_t value, int datalen)
{
	uint32_t addr = M16C_Read16(M16C_REG_PC);
	switch (datalen) {
	    case 1:
		    M16C_Write8(value, addr);
		    break;
	    case 2:
		    M16C_Write16(value, addr);
		    break;
	    case 3:
		    M16C_Write24(value, addr);
		    break;
	    default:
		    dbgprintf("Bad datalen of %d bytes\n", datalen);
		    break;
	}
}

static void
gam_get_abs16(uint32_t * value, int datalen)
{
	uint32_t addr = M16C_Read16(M16C_REG_PC);
	switch (datalen) {
	    case 1:
		    *value = M16C_Read8(addr);
		    break;
	    case 2:
		    *value = M16C_Read16(addr);
		    break;
	    default:
		    dbgprintf("Bad datalen of %d bytes\n", datalen);
		    break;
	}

}

static void
gam_set_bad(uint32_t value, int datalen)
{
	dbgprintf("Illegal addressing mode\n");
}

static void
gam_get_bad(uint32_t * value, int datalen)
{
	dbgprintf("Illegal addressing mode\n");
}

static GAM_GetProc *
general_am_get(int am, int size, int *codelen, uint32_t existence_map)
{
	switch (am) {
	    case 0:
		    *codelen = 0;
		    if (size == 1) {
			    return gam_get_r0l;
		    } else if (size == 2) {
			    return gam_get_r0;
		    } else {
			    fprintf(stderr, "Bug in AM line %d\n", __LINE__);
			    exit(1);
		    }
		    break;
	    case 1:
		    *codelen = 0;
		    if (size == 1) {
			    return gam_get_r0h;
		    } else if (size == 2) {
			    return gam_get_r1;
		    } else {
			    fprintf(stderr, "Bug in AM line %d\n", __LINE__);
			    exit(1);
		    }
		    break;
	    case 2:
		    *codelen = 0;
		    if (size == 2) {
			    return gam_get_r2;
		    } else if (size == 1) {
			    return gam_get_r1l;
		    } else {
			    fprintf(stderr, "Bug in AM line %d\n", __LINE__);
			    exit(1);
		    }
		    break;

	    case 3:
		    *codelen = 0;
		    if (size == 2) {
			    return gam_get_r3;
		    } else {
			    return gam_get_r1h;
		    }
	    case 4:
		    *codelen = 0;
		    return gam_get_a0;

	    case 5:
		    *codelen = 0;
		    return gam_get_a1;

	    case 6:
		    *codelen = 0;
		    return gam_get_ia0;

	    case 7:
		    *codelen = 0;
		    return gam_get_ia1;

	    case 8:
		    *codelen = 1;
		    return gam_get_dsp8ia0;

	    case 9:
		    *codelen = 1;
		    return gam_get_dsp8ia1;

	    case 10:
		    *codelen = 1;
		    return gam_get_dsp8isb;

	    case 11:
		    *codelen = 1;
		    return gam_get_dsp8ifb;

	    case 12:
		    *codelen = 2;
		    return gam_get_dsp16ia0;

	    case 13:
		    *codelen = 2;
		    return gam_get_dsp16ia1;

	    case 14:
		    *codelen = 2;
		    return gam_get_dsp16isb;

	    case 15:		/* abs 16 */
		    *codelen = 2;
		    return gam_get_abs16;
	    default:
		    *codelen = 0;
		    return gam_get_bad;
	}
}

static GAM_SetProc *
general_am_set(int am, int size, int *codelen, uint32_t existence_map)
{
	switch (am) {
	    case 0:
		    *codelen = 0;
		    if (size == 1) {
			    return gam_set_r0l;
		    } else if (size == 2) {
			    return gam_set_r0;
		    } else if (size == 3) {
			    return gam_set_r2r0;
		    } else if (size == 4) {
			    return gam_set_r2r0;
		    } else {
			    fprintf(stderr, "Bug in AM line %d\n", __LINE__);
			    exit(1);
		    }
		    break;
	    case 1:
		    *codelen = 0;
		    if (size == 1) {
			    return gam_set_r0h;
		    } else if (size == 2) {
			    return gam_set_r1;
		    } else if (size == 3) {
			    return gam_set_r3r1;
		    } else if (size == 4) {
			    return gam_set_r3r1;
		    } else {
			    fprintf(stderr, "Bug in AM line %d\n", __LINE__);
			    exit(1);
		    }
		    break;
	    case 2:
		    *codelen = 0;
		    if (size == 2) {
			    return gam_set_r2;
		    } else if (size == 1) {
			    return gam_set_r1l;
		    } else {
			    fprintf(stderr, "Bug in AM line %d\n", __LINE__);
			    exit(1);
		    }
		    break;

	    case 3:
		    *codelen = 0;
		    if (size == 2) {
			    return gam_set_r3;
		    } else {
			    return gam_set_r1h;
		    }
	    case 4:
		    *codelen = 0;
		    if (size == 4) {
			    return gam_set_a1a0;
		    } else if (size == 3) {
			    return gam_set_a1a0;
		    } else if (size == 2) {
			    return gam_set_a0;
		    } else {
			    return gam_set_a0;
		    }
		    break;

	    case 5:
		    *codelen = 0;
		    return gam_set_a1;

	    case 6:
		    *codelen = 0;
		    return gam_set_ia0;

	    case 7:
		    *codelen = 0;
		    return gam_set_ia1;

	    case 8:
		    *codelen = 1;
		    return gam_set_dsp8ia0;

	    case 9:
		    *codelen = 1;
		    return gam_set_dsp8ia1;

	    case 10:
		    *codelen = 1;
		    return gam_set_dsp8isb;

	    case 11:
		    *codelen = 1;
		    return gam_set_dsp8ifb;

	    case 12:
		    *codelen = 2;
		    return gam_set_dsp16ia0;

	    case 13:
		    *codelen = 2;
		    return gam_set_dsp16ia1;

	    case 14:
		    *codelen = 2;
		    return gam_set_dsp16isb;

	    case 15:		/* abs 16 */
		    *codelen = 2;
		    return gam_set_abs16;
	    default:
		    *codelen = 0;
		    return gam_set_bad;

	}
}

static GAM_SetProc *
general_am_set_mulextdst(int am, int size, int *codelen, uint32_t existence_map)
{
	switch (am) {
	    case 0:
		    *codelen = 0;
		    if (size == 2) {
			    return gam_set_r0;
		    } else if (size == 4) {
			    return gam_set_r2r0;
		    } else {
			    fprintf(stderr, "Bug in AM line %d\n", __LINE__);
			    exit(1);
		    }
		    break;
	    case 1:
		    *codelen = 0;
		    if (size == 2) {
			    fprintf(stderr, "Bug in AM line %d\n", __LINE__);
			    return gam_set_r1;
		    } else if (size == 4) {
			    return gam_set_r3r1;
		    } else {
			    fprintf(stderr, "Bug in AM line %d\n", __LINE__);
			    exit(1);
		    }
		    return gam_set_bad;
	    case 2:
		    *codelen = 0;
		    if (size == 2) {
			    /* This one is verified with real CPU ! */
			    return gam_set_r1;
		    } else {
			    fprintf(stderr, "Bug in AM line %d\n", __LINE__);
			    exit(1);
		    }
		    return gam_set_bad;

	    case 3:
		    fprintf(stderr, "Bug in AM line %d\n", __LINE__);
		    exit(1);
		    return gam_set_bad;
	    case 4:
		    *codelen = 0;
		    if (size == 4) {
			    return gam_set_a1a0;
		    } else if (size == 2) {
			    return gam_set_a0;
		    } else {
			    fprintf(stderr, "Bug in AM line %d\n", __LINE__);
			    exit(1);
		    }
		    return gam_set_bad;

	    case 5:
		    fprintf(stderr, "Bug in AM line %d\n", __LINE__);
		    exit(1);
		    return gam_set_bad;

	    case 6:
		    *codelen = 0;
		    return gam_set_ia0;

	    case 7:
		    *codelen = 0;
		    return gam_set_ia1;

	    case 8:
		    *codelen = 1;
		    return gam_set_dsp8ia0;

	    case 9:
		    *codelen = 1;
		    return gam_set_dsp8ia1;

	    case 10:
		    *codelen = 1;
		    return gam_set_dsp8isb;

	    case 11:
		    *codelen = 1;
		    return gam_set_dsp8ifb;

	    case 12:
		    *codelen = 2;
		    return gam_set_dsp16ia0;

	    case 13:
		    *codelen = 2;
		    return gam_set_dsp16ia1;

	    case 14:
		    *codelen = 2;
		    return gam_set_dsp16isb;

	    case 15:		/* abs 16 */
		    *codelen = 2;
		    return gam_set_abs16;
	    default:
		    *codelen = 0;
		    return gam_set_bad;

	}
}

static uint16_t
am1_get_eva(int admode, int *arglen)
{
	uint32_t addr;
	switch (admode) {
	    case 8:
		    {
			    uint8_t dsp8 = M16C_Read8(M16C_REG_PC);
			    addr = (M16C_REG_A0 + dsp8) & 0xfffff;
			    *arglen = 1;
		    }
		    break;

	    case 9:
		    {
			    uint8_t dsp8 = M16C_Read8(M16C_REG_PC);
			    addr = (M16C_REG_A1 + dsp8) & 0xfffff;
			    *arglen = 1;
		    }
		    break;

	    case 10:
		    {
			    uint8_t dsp8 = M16C_Read8(M16C_REG_PC);
			    addr = (M16C_REG_SB + dsp8) & 0xfffff;
			    *arglen = 1;
		    }
		    break;

	    case 11:
		    {
			    int8_t dsp8 = M16C_Read8(M16C_REG_PC);
			    addr = (M16C_REG_FB + dsp8) & 0xfffff;
			    *arglen = 1;
		    }
		    break;

	    case 12:
		    {
			    uint16_t dsp16 = M16C_Read16(M16C_REG_PC);
			    addr = (M16C_REG_A0 + dsp16) & 0xfffff;
			    *arglen = 2;
		    }
		    break;

	    case 13:
		    {
			    uint16_t dsp16 = M16C_Read16(M16C_REG_PC);
			    addr = (M16C_REG_A1 + dsp16) & 0xfffff;
			    *arglen = 2;
		    }
		    break;
	    case 14:
		    {
			    uint16_t dsp16 = M16C_Read16(M16C_REG_PC);
			    addr = (M16C_REG_SB + dsp16) & 0xfffff;
			    *arglen = 2;
		    }
		    break;

	    case 15:
		    {
			    uint16_t abs16 = M16C_Read16(M16C_REG_PC);
			    addr = abs16;
			    *arglen = 2;
		    }
		    break;
	    default:
		    fprintf(stderr, "wrong address mode EVA\n");
		    addr = 0;
            *arglen = 0;
		    break;
	}
	return addr;
}

static uint8_t
am2b_get(int am, int *arglen)
{
	uint8_t value;
	int admode = am & 7;	/* Remove this oneday */
	switch (admode) {
	    case 3:
		    // R0H;
		    *arglen = 0;
		    return M16C_REG_R0H;

	    case 4:
		    // R0L
		    *arglen = 0;
		    return M16C_REG_R0L;
	    case 5:
		    {
			    // DSP:8[SB]
			    uint8_t dsp8 = M16C_Read8(M16C_REG_PC);
			    uint32_t addr = M16C_REG_SB + dsp8;
			    value = M16C_Read8(addr);
			    *arglen = 1;
		    }
		    break;
	    case 6:
		    {
			    // DSP:8[FB]
			    int8_t dsp8 = M16C_Read8(M16C_REG_PC);
			    uint32_t addr = M16C_REG_FB + dsp8;
			    value = M16C_Read8(addr);
			    *arglen = 1;
		    }
		    break;
	    case 7:
		    {
			    uint16_t abs16 = M16C_Read16(M16C_REG_PC);
			    value = M16C_Read8(abs16);
			    *arglen = 2;
		    }
		    break;

	    default:
		    fprintf(stderr, "Illegal addressing mode 2: %d at PC 0x%06x\n", admode,
			    M16C_REG_PC);
		    *arglen = 0;
		    value = 0;
		    exit(1);
	}
	return value;
}

static void
am2b_set(uint16_t am, int *arglen, uint8_t value)
{
	int admode = am & 7;	/* remove this one day */
	switch (admode) {
	    case 3:
		    // R0H;
		    M16C_REG_R0H = value;
		    *arglen = 0;
		    break;

	    case 4:
		    // R0L
		    M16C_REG_R0L = value;
		    *arglen = 0;
		    break;

	    case 5:
		    {
			    // DSP:8[SB]
			    uint8_t dsp8 = M16C_Read8(M16C_REG_PC);
			    uint32_t addr = M16C_REG_SB + dsp8;
			    *arglen = 1;
			    M16C_Write8(value, addr);
		    }
		    break;
	    case 6:
		    {
			    // DSP:8[FB]
			    int8_t dsp8 = M16C_Read8(M16C_REG_PC);
			    uint32_t addr = M16C_REG_FB + dsp8;
			    *arglen = 1;
			    M16C_Write8(value, addr);
		    }
		    break;
	    case 7:
		    {
			    uint16_t abs16 = M16C_Read16(M16C_REG_PC);
			    M16C_Write8(value, abs16);
			    *arglen = 2;
		    }
		    break;

	    default:
		    fprintf(stderr, "write: Illegal addressing mode 2: %d\n", admode);
	}
	return;
}

/*
 * this needs a 3 Bit amode ! 
 */
static uint8_t
am3b_get(int amode, int *arglen)
{
	int src = amode & 3;
	uint8_t value;
	switch (src) {
	    case 0:
		    /* If dst is R0L src is R0H else src is R0L */
		    {
			    int dst = amode & 0x4;
			    if (dst) {
				    value = M16C_REG_R0L;
			    } else {
				    value = M16C_REG_R0H;
			    }
			    *arglen = 0;
		    }
		    break;
	    case 1:
		    /* DSP:8[SB] */
		    {
			    uint8_t dsp8 = M16C_Read8(M16C_REG_PC);
			    uint32_t addr = M16C_REG_SB + dsp8;
			    value = M16C_Read8(addr);
			    *arglen = 1;
		    }
		    break;

	    case 2:
		    /* DSP:8[FB] */
		    {
			    int8_t dsp8 = M16C_Read8(M16C_REG_PC);
			    uint32_t addr = M16C_REG_FB + dsp8;
			    value = M16C_Read8(addr);
			    *arglen = 1;
		    }
		    break;
	    case 3:
		    /* ABS16 */
		    {
			    uint16_t abs16 = M16C_Read16(M16C_REG_PC);
#if 0
			    fprintf(stderr, "Abs 16: %04x\n", abs16);
			    if (abs16 == 0x389b) {
				    exit(1);
			    }
#endif
			    value = M16C_Read8(abs16);
			    *arglen = 2;
		    }
		    break;
	    default:
		    /* Unreachable, make compiler quiet */
		    value = 0;
	}
	return value;
}

#if 1
static void
am3b_set(int amode, int *arglen, uint8_t value)
{
	int src = amode & 3;
	switch (src) {
	    case 0:
		    /* If dst is R0L src is R0H else src is R0L */
		    {
			    int dst = amode & 0x4;
			    if (dst) {
				    M16C_REG_R0L = value;
			    } else {
				    M16C_REG_R0H = value;
			    }
			    *arglen = 0;
		    }
		    break;
	    case 1:
		    /* DSP:8[SB] */
		    {
			    uint8_t dsp8 = M16C_Read8(M16C_REG_PC);
			    uint32_t addr = M16C_REG_SB + dsp8;
			    M16C_Write8(value, addr);
			    *arglen = 1;
		    }
		    break;

	    case 2:
		    /* DSP:8[FB] */
		    {
			    int8_t dsp8 = M16C_Read8(M16C_REG_PC);
			    uint32_t addr = M16C_REG_FB + dsp8;
			    M16C_Write8(value, addr);
			    *arglen = 1;
		    }
		    break;
	    case 3:
		    /* ABS16 */
		    {
			    uint16_t abs16 = M16C_Read16(M16C_REG_PC);
			    M16C_Write8(value, abs16);
			    *arglen = 2;
		    }
		    break;
	    default:
		    *arglen = 0;
		    /* Unreachable, make compiler quiet */
		    break;
	}
}
#endif
/*
 * -------------------------------------------------------------------
 * Test showed that M16C does Byte wide read-modify write
 * on memory bit operations
 * -------------------------------------------------------------------
 */
static uint32_t
get_bitaddr(int am, int *codelen, int *bitnr_ret)
{

	uint16_t value;
	uint16_t bitnr;
	uint16_t bytenr;
	switch (am) {
	    case 0:
		    // bit,R0
		    bitnr = M16C_Read8(M16C_REG_PC) & 0xf;
		    value = M16C_REG_R0;
		    *codelen = 1;
		    break;
	    case 1:
		    // bit,R1
		    bitnr = M16C_Read8(M16C_REG_PC) & 0xf;
		    value = M16C_REG_R1;
		    *codelen = 1;
		    break;
	    case 2:
		    // bit,R2
		    bitnr = M16C_Read8(M16C_REG_PC) & 0xf;
		    value = M16C_REG_R2;
		    *codelen = 1;
		    break;
	    case 3:
		    // bit,R3
		    bitnr = M16C_Read8(M16C_REG_PC) & 0xf;
		    value = M16C_REG_R3;
		    *codelen = 1;
		    break;
	    case 4:
		    // bit,A0
		    bitnr = M16C_Read8(M16C_REG_PC) & 0xf;
		    value = M16C_REG_A0;
		    *codelen = 1;
		    break;
	    case 5:
		    // bit,A1
		    bitnr = M16C_Read8(M16C_REG_PC) & 0xf;
		    value = M16C_REG_A1;
		    *codelen = 1;
		    break;

	    case 6:
		    // bit[A0]
		    bytenr = M16C_REG_A0 >> 3;
		    bitnr = M16C_REG_A0 & 0x7;
		    value = M16C_Read8(bytenr);
		    *codelen = 0;
		    break;

	    case 7:
		    // bit[A1]
		    bytenr = M16C_REG_A1 >> 3;
		    bitnr = M16C_REG_A1 & 0x7;
		    value = M16C_Read8(bytenr);
		    *codelen = 0;
		    break;

	    case 8:
		    // base:8[A0]
		    {
			    uint32_t base = M16C_Read8(M16C_REG_PC);
			    bytenr = (base + (M16C_REG_A0 >> 3)) & 0xffff;
			    bitnr = M16C_REG_A0 & 7;
			    value = M16C_Read8(bytenr);
			    *codelen = 1;
		    }
		    break;

	    case 9:
		    // base:8[A1]
		    {
			    uint32_t base = M16C_Read8(M16C_REG_PC);
			    bytenr = (base + (M16C_REG_A1 >> 3)) & 0xffff;
			    bitnr = M16C_REG_A1 & 7;
			    value = M16C_Read8(bytenr);
			    *codelen = 1;
		    }
		    break;

	    case 10:
		    // bit,base:8[SB]
		    {
			    uint8_t dsp = M16C_Read8(M16C_REG_PC);
			    bytenr = ((dsp >> 3) + M16C_REG_SB) & 0xffff;
			    bitnr = dsp & 7;
			    value = M16C_Read8(bytenr);
			    *codelen = 1;
		    }
		    break;

	    case 11:
		    // bit,base:8[FB]
		    {
			    int8_t dsp = M16C_Read8(M16C_REG_PC);
			    bytenr = ((dsp >> 3) + M16C_REG_FB) & 0xffff;
			    bitnr = dsp & 7;
			    value = M16C_Read8(bytenr);
			    *codelen = 1;
		    }
		    break;
	    case 12:
		    // base:16[A0]
		    {
			    uint16_t base = M16C_Read16(M16C_REG_PC);
			    bitnr = M16C_REG_A0 & 7;
			    bytenr = (base + (M16C_REG_A0 >> 3)) & 0xffff;
			    value = M16C_Read8(bytenr);
			    *codelen = 2;
		    }
		    break;

	    case 13:
		    // base:16[A1]
		    {
			    uint16_t base = M16C_Read16(M16C_REG_PC);
			    bitnr = M16C_REG_A1 & 7;
			    bytenr = (base + (M16C_REG_A1 >> 3)) & 0xffff;
			    value = M16C_Read8(bytenr);
			    *codelen = 2;
		    }
		    break;
	    case 14:
		    // bit,base:16[SB]
		    {
			    uint16_t dsp = M16C_Read16(M16C_REG_PC);
			    bitnr = dsp & 7;
			    bytenr = (M16C_REG_SB + (dsp >> 3)) & 0xffff;
			    value = M16C_Read8(bytenr);
			    *codelen = 2;
		    }
		    break;
	    case 15:
		    // bit,base:16
		    {
			    uint16_t dsp = M16C_Read16(M16C_REG_PC);
			    bitnr = dsp & 7;
			    bytenr = dsp >> 3;
			    value = M16C_Read8(bytenr);
			    *codelen = 2;
		    }
		    break;
	    default:
		    /* should not be reached, make compiler quiet */
		    bitnr = 0;
		    value = 0;
	}
	*bitnr_ret = bitnr;
	return value;
}

static void
set_bitaddr(int am, uint32_t value)
{
	//uint16_t bitnr;
	uint32_t bytenr;

	switch (am) {
	    case 0:
		    // bit,R0
		    M16C_REG_R0 = value;
		    break;
	    case 1:
		    // bit,R1
		    M16C_REG_R1 = value;
		    break;
	    case 2:
		    // bit,R2
		    M16C_REG_R2 = value;
		    break;

	    case 3:
		    // bit,R3
		    M16C_REG_R3 = value;
		    break;

	    case 4:
		    // bit,A0
		    M16C_REG_A0 = value;
		    break;

	    case 5:
		    // bit,A1
		    M16C_REG_A1 = value;
		    break;

	    case 6:
		    // bit[A0]
		    bytenr = M16C_REG_A0 >> 3;
		    M16C_Write8(value, bytenr);
		    break;

	    case 7:
		    // bit[A1]
		    bytenr = M16C_REG_A1 >> 3;
		    M16C_Write8(value, bytenr);
		    break;

	    case 8:
		    // base:8[A0]
		    {
			    uint32_t base = M16C_Read8(M16C_REG_PC);
			    bytenr = (base + (M16C_REG_A0 >> 3)) & 0xffff;
			    M16C_Write8(value, bytenr);
		    }
		    break;

	    case 9:
		    // base:8[A1]
		    {
			    uint32_t base = M16C_Read8(M16C_REG_PC);
			    bytenr = (base + (M16C_REG_A1 >> 3)) & 0xffff;
			    M16C_Write8(value, bytenr);
		    }
		    break;

	    case 10:
		    // bit,base:8[SB]
		    {
			    uint8_t dsp = M16C_Read8(M16C_REG_PC);
			    bytenr = (M16C_REG_SB + (dsp >> 3)) & 0xffff;
			    M16C_Write8(value, bytenr);
		    }
		    break;

	    case 11:
		    // bit,base:8[FB]
		    {
			    int8_t dsp = M16C_Read8(M16C_REG_PC);
			    bytenr = (M16C_REG_FB + (dsp >> 3)) & 0xffff;
			    M16C_Write8(value, bytenr);
		    }
		    break;
	    case 12:
		    // base:16[A0]
		    {
			    uint32_t base = M16C_Read16(M16C_REG_PC);
			    bytenr = (base + (M16C_REG_A0 >> 3)) & 0xffff;
			    M16C_Write8(value, bytenr);
		    }
		    break;

	    case 13:
		    // base:16[A1]
		    {
			    uint32_t base = M16C_Read16(M16C_REG_PC);
			    bytenr = (base + (M16C_REG_A1 >> 3)) & 0xffff;
			    M16C_Write8(value, bytenr);
		    }
		    break;

	    case 14:
		    // bit,base:16[SB]
		    {
			    uint16_t dsp = M16C_Read16(M16C_REG_PC);
			    bytenr = (M16C_REG_SB + (dsp >> 3)) & 0xffff;
			    M16C_Write8(value, bytenr);
		    }
		    break;
	    case 15:
		    // bit,base:16
		    {
			    uint16_t dsp = M16C_Read16(M16C_REG_PC);
			    bytenr = dsp >> 3;
			    M16C_Write8(value, bytenr);
		    }
		    break;
	    default:
		    /* should not be reached, make compiler quiet */
		    break;
	}
}

static inline void
sgn_zero_flags(uint32_t result, int size)
{
	M16C_REG_FLG &= ~(M16C_FLG_SIGN | M16C_FLG_ZERO);
	if (size == 2) {
		if (ISNEGW(result)) {
			M16C_REG_FLG |= M16C_FLG_SIGN;
		} else if ((result & 0xffff) == 0) {
			M16C_REG_FLG |= M16C_FLG_ZERO;
		}
	} else if (size == 1) {
		if (ISNEGB(result)) {
			M16C_REG_FLG |= M16C_FLG_SIGN;
		} else if ((result & 0xff) == 0) {
			M16C_REG_FLG |= M16C_FLG_ZERO;
		}
	} else {
		/* Should be undefined ??? */
		if (ISNEGL(result)) {
			M16C_REG_FLG |= M16C_FLG_SIGN;
		} else if ((result) == 0) {
			M16C_REG_FLG |= M16C_FLG_ZERO;
		}
	}
}

static inline void
ext_flags(uint32_t result, int size)
{
	sgn_zero_flags(result, size);
}

static inline void
mov_flags(uint32_t result, int size)
{
	sgn_zero_flags(result, size);
}

static inline void
and_flags(uint16_t result, int opsize)
{
	sgn_zero_flags(result, opsize);
}

static inline void
or_flags(uint16_t result, int opsize)
{
	return sgn_zero_flags(result, opsize);
}

static inline void
xor_flags(uint16_t result, int opsize)
{
	return sgn_zero_flags(result, opsize);
}

static inline void
not_flags(uint16_t result, int opsize)
{
	return sgn_zero_flags(result, opsize);
}

static inline void
ModOpsize(int am, int *opsize)
{
	switch (am) {
	    case 0x04:
	    case 0x05:
		    if (*opsize == 1) {
			    *opsize = 2;
		    }
	}
}

static void
ModOpsizeError(const char *function)
{
	fprintf(stderr, "ModOpsize Not allowed in: %s\n", function);
	exit(1);
}

#define NotModOpsize(am) { \
        if((am == 4) || (am == 5)) { \
                ModOpsizeError(__FUNCTION__); \
        } \
}

/**
 *********************************************************
 * \fn void m16c_abs_size_dst(void) 
 * Take the absolute of a destination. 
 * From test with M32C: Carry flag is always cleared
 * v0
 *********************************************************
 */
void
m16c_abs_size_dst()
{
	int size;
	int codelen_dst;
	GAM_GetProc *getdst;
	GAM_SetProc *setdst;
	uint32_t Dst;
	int dst = ICODE16() & 0xf;
	if (ICODE16() & 0x100) {
		size = 2;
	} else {
		size = 1;
		NotModOpsize(dst);	/* AM 8 Bit A0/A1 doesn't exist */
	}
	getdst = general_am_get(dst, size, &codelen_dst, GAM_ALL);
	setdst = general_am_set(dst, size, &codelen_dst, GAM_ALL);
	getdst(&Dst, size);
	M16C_REG_FLG &= ~(M16C_FLG_OVERFLOW | M16C_FLG_SIGN | M16C_FLG_ZERO | M16C_FLG_CARRY);
	if (size == 2) {
		if (Dst & 0x8000) {
			Dst = 0 - Dst;
			if (Dst & 0x8000) {
				M16C_REG_FLG |= M16C_FLG_OVERFLOW;
				M16C_REG_FLG |= M16C_FLG_SIGN;
			} else {
				if ((Dst & 0xffff) == 0) {
					M16C_REG_FLG |= M16C_FLG_ZERO;
				}
			}
		}
		setdst(Dst, size);
	} else {
		if (Dst & 0x80) {
			Dst = 0x00 - Dst;
			if (Dst & 0x80) {
				M16C_REG_FLG |= M16C_FLG_SIGN;
				M16C_REG_FLG |= M16C_FLG_OVERFLOW;
			}
		} else {
			if ((Dst & 0xff) == 0) {
				M16C_REG_FLG |= M16C_FLG_ZERO;
			}
		}
		setdst(Dst, size);
	}
	M16C_REG_PC += codelen_dst;
}

/*
 *******************************************************************************
 * \fn void m16c_adc_size_immdst(void)
 * add with carry.
 * v0
 *******************************************************************************
 */
void
m16c_adc_size_immdst(void)
{
	int dst;
	uint32_t Src, Dst, Result;
	int opsize, srcsize;
	int codelen_dst;
	GAM_GetProc *getdst;
	GAM_SetProc *setdst;
	dst = ICODE16() & 0xf;
	if (ICODE16() & 0x100) {
		srcsize = opsize = 2;
	} else {
		srcsize = opsize = 1;
		ModOpsize(dst, &opsize);
	}
	getdst = general_am_get(dst, opsize, &codelen_dst, GAM_ALL);
	setdst = general_am_set(dst, opsize, &codelen_dst, GAM_ALL);
	if (srcsize == 2) {
		Src = M16C_Read16(M16C_REG_PC + codelen_dst);
	} else if (srcsize == 1) {
		Src = M16C_Read8(M16C_REG_PC + codelen_dst);
	}
	getdst(&Dst, opsize);
	if (M16C_REG_FLG & M16C_FLG_CARRY) {
		Result = Dst + Src + 1;
	} else {
		Result = Dst + Src;
	}
	setdst(Result, opsize);
	add_flags(Dst, Src, Result, opsize);
	M16C_REG_PC += codelen_dst + srcsize;
	dbgprintf("m16c_adc_size_immdst not tested\n");
}

/**
 *****************************************************************
 * \fn void m16c_adc_size_srcdst(void)
 * Add a src and a carry to a destination.
 * v0
 *****************************************************************
 */
void
m16c_adc_size_srcdst(void)
{
	int dst, src;
	uint32_t Src, Dst, Result;
	int opsize, srcsize;
	int codelen_src;
	int codelen_dst;
	GAM_GetProc *getdst;
	GAM_GetProc *getsrc;
	GAM_SetProc *setdst;
	dst = ICODE16() & 0xf;
	src = (ICODE16() >> 4) & 0xf;
	if (ICODE16() & 0x100) {
		srcsize = opsize = 2;
	} else {
		srcsize = opsize = 1;
		ModOpsize(dst, &opsize);
	}
	getsrc = general_am_get(src, srcsize, &codelen_src, GAM_ALL);
	getdst = general_am_get(dst, opsize, &codelen_dst, GAM_ALL);
	setdst = general_am_set(dst, opsize, &codelen_dst, GAM_ALL);
	getsrc(&Src, srcsize);
	M16C_REG_PC += codelen_src;
	getdst(&Dst, opsize);
	if (M16C_REG_FLG & M16C_FLG_CARRY) {
		Result = Dst + Src + 1;
	} else {
		Result = Dst + Src;
	}
	setdst(Result, opsize);
	add_flags(Dst, Src, Result, opsize);
	M16C_REG_PC += codelen_dst;
	dbgprintf("m16c_adc_size_srcdst not tested\n");
}

/**
 ****************************************************************************
 * \fn void m16c_adcf_size_dst(void)
 * Add the carry flag to a destination.
 * v0
 ****************************************************************************
 */
void
m16c_adcf_size_dst(void)
{
	int dst;
	uint32_t Dst, Result;
	int size;
	int codelen_dst;
	GAM_GetProc *getdst;
	GAM_SetProc *setdst;
	dst = ICODE16() & 0xf;
	if (ICODE16() & 0x100) {
		size = 2;
	} else {
		size = 1;
		NotModOpsize(dst);	/* AM 8 Bit A0/A1 doesn't exist */
	}
	getdst = general_am_get(dst, size, &codelen_dst, GAM_ALL);
	setdst = general_am_set(dst, size, &codelen_dst, GAM_ALL);
	getdst(&Dst, size);
	if (M16C_REG_FLG & M16C_FLG_CARRY) {
		Result = Dst + 1;
	} else {
		Result = Dst;
	}
	setdst(Result, size);
	add_flags(Dst, 0, Result, size);
	M16C_REG_PC += codelen_dst;
	dbgprintf("m16c_adcf_size_dst not tested\n");
}

/**
 *************************************************************************
 * \fn void m16c_add_size_g_immdst(void)
 * Add an immediate to a destination.
 * v0
 *************************************************************************
 */
void
m16c_add_size_g_immdst(void)
{
	int dst;
	uint32_t Src, Dst, Result;
	int opsize, immsize;
	int codelen_dst;
	GAM_GetProc *getdst;
	GAM_SetProc *setdst;
	dst = ICODE16() & 0xf;
	if (ICODE16() & 0x100) {
		opsize = immsize = 2;
	} else {
		opsize = immsize = 1;
		ModOpsize(dst, &opsize);
	}
	getdst = general_am_get(dst, opsize, &codelen_dst, GAM_ALL);
	setdst = general_am_set(dst, opsize, &codelen_dst, GAM_ALL);
	getdst(&Dst, opsize);
	if (immsize == 2) {
		Src = M16C_Read16(M16C_REG_PC + codelen_dst);
	} else if (immsize == 1) {
		Src = M16C_Read8(M16C_REG_PC + codelen_dst);
	}
	Result = Dst + Src;
	setdst(Result, opsize);
	add_flags(Dst, Src, Result, opsize);
	M16C_REG_PC += codelen_dst + immsize;
	dbgprintf("m16c_add_size_g_immdst not tested\n");
}

/**
 ********************************************************************
 * \fn void m16c_add_size_q 
 * Add a 4 Bit signed immediate to a Destination. 
 * v0
 ********************************************************************
 */
void
m16c_add_size_q(void)
{
	int dst;
	uint32_t Dst, Result;
	int32_t Src;
	int opsize;
	int codelen_dst;
	GAM_GetProc *getdst;
	GAM_SetProc *setdst;
	Src = ((int32_t) ((ICODE16() & 0xf0) << 24)) >> 28;
	dst = ICODE16() & 0xf;
	if (ICODE16() & 0x100) {
		opsize = 2;
	} else {
		opsize = 1;
		ModOpsize(dst, &opsize);
		if (opsize == 2) {
			/* This makes the immediate unsigned ! */
			Src = Src & 0xff;
		}
	}
	getdst = general_am_get(dst, opsize, &codelen_dst, GAM_ALL);
	setdst = general_am_set(dst, opsize, &codelen_dst, GAM_ALL);
	getdst(&Dst, opsize);
	Result = Dst + Src;
	setdst(Result, opsize);
	add_flags(Dst, Src, Result, opsize);
	M16C_REG_PC += codelen_dst;
	dbgprintf("m16c_add_size_q not tested\n");
}

/**
 *****************************************************************************************
 * \fn void m16c_add_b_s_immdst(void)
 * v0
 *****************************************************************************************
 */
void
m16c_add_b_s_immdst(void)
{
	uint32_t imm;
	uint32_t Dst;
	uint32_t Result;
	int opsize = 1;
	int codelen;
	int dst = ICODE8() & 7;
	imm = M16C_Read8(M16C_REG_PC);
	M16C_REG_PC += opsize;
	Dst = am2b_get(dst, &codelen);
	Result = Dst + imm;
	am2b_set(dst, &codelen, Result);
	add_flags(Dst, imm, Result, opsize);
	M16C_REG_PC += codelen;
	dbgprintf("m16c_add_size_s_immdst: am %d res %d \n", dst, Result);
}

/**
 *********************************************************************************+
 * \fn void m16c_add_size_g_srcdst(void)
 * Add a source to a destination.
 * v0
 *********************************************************************************+
 */
void
m16c_add_size_g_srcdst(void)
{
	int dst, src;
	uint32_t Src, Dst, Result;
	int srcsize, opsize;
	int codelen_src;
	int codelen_dst;
	GAM_GetProc *getdst;
	GAM_GetProc *getsrc;
	GAM_SetProc *setdst;
	dst = ICODE16() & 0xf;
	src = (ICODE16() >> 4) & 0xf;
	if (ICODE16() & 0x100) {
		srcsize = opsize = 2;
	} else {
		srcsize = opsize = 1;
		ModOpsize(dst, &opsize);
	}
	getsrc = general_am_get(src, srcsize, &codelen_src, GAM_ALL);
	getdst = general_am_get(dst, opsize, &codelen_dst, GAM_ALL);
	setdst = general_am_set(dst, opsize, &codelen_dst, GAM_ALL);
	getsrc(&Src, srcsize);
	M16C_REG_PC += codelen_src;
	getdst(&Dst, opsize);
	Result = Dst + Src;
	setdst(Result, opsize);
	add_flags(Dst, Src, Result, opsize);
	M16C_REG_PC += codelen_dst;
	dbgprintf("m16c_add_size_g_srcdst not tested\n");
}

/**
 **************************************************************************
 * \fn void m16c_add_b_s_srcr0l(void)
 * v0
 **************************************************************************
 */
void
m16c_add_b_s_srcr0l(void)
{
	int size;
	int am;
	uint32_t Src, Dst, Result;
	int codelen;
	/* SRC-AM includes destination ! */
	am = ICODE8() & 7;
	size = 1;
	Src = am3b_get(am, &codelen);
	if (ICODE8() & 4) {
		Dst = M16C_REG_R0H;
		Result = Dst + Src;
		M16C_REG_R0H = Result;
	} else {
		Dst = M16C_REG_R0L;
		Result = Dst + Src;
		M16C_REG_R0L = Result;
	}
	add_flags(Dst, Src, Result, size);
	M16C_REG_PC += codelen;
	dbgprintf("m16c_add_b_s_r0lr0h not tested\n");
}

/**
 ****************************************************************************
 * \fn void m16c_add_size_g_imm_sp(void)
 * Add an immediate to the Stack pointer.
 * v0
 ****************************************************************************
 */

void
m16c_add_size_g_imm_sp(void)
{
	int size, opsize;
	int16_t imm16;
	int16_t Dst, Result;
	if (ICODE16() & 0x100) {
		size = 2;
	} else {
		size = 1;
	}
	if (size == 2) {
		imm16 = (int16_t) M16C_Read16(M16C_REG_PC);
		M16C_REG_PC += 2;
	} else {
		imm16 = (int16_t) (int8_t) M16C_Read8(M16C_REG_PC);
		M16C_REG_PC += 1;
	}
	Dst = M16C_REG_SP;
	Result = Dst + imm16;
	M16C_REG_SP = Result & 0xffff;
	/* opsize is always 16 bit even with 8 bit immediate */
	opsize = 2;
	add_flags(Dst, imm16, Result, opsize);
	dbgprintf("m16c_add_l_s_imm8sp not tested\n");
}

/*
 **********************************************************************
 * \fn void m16c_add_size_q_imm_sp() 
 * Add a 4 bit signed immediate to the 16 Bit stack pointer.
 * v0
 **********************************************************************
 */
void
m16c_add_size_q_imm_sp()
{
	int16_t Src = ((int16_t) ((ICODE16() & 0xf) << 12)) >> 12;
	int16_t Dst, Result;
	int opsize = 2;
	Dst = M16C_REG_SP;
	M16C_REG_SP = Result = Src + Dst;
	add_flags(Dst, Src, Result, opsize);
	dbgprintf("instr m16c_add_size_q_imm_sp(%04x) \n", ICODE16());
}

/** 
 *********************************************************************
 * add a 4 bit signed immediate to a destination and
 * jump if result is not zero.
 * v0
 *********************************************************************
 */
void
m16c_adjnz_size_immdst(void)
{
	int dst;
	uint32_t Dst, Result;
	int32_t Src;
	int size;
	int codelen_dst;
	GAM_GetProc *getdst;
	GAM_SetProc *setdst;
	dst = ICODE16() & 0xf;
	Src = ((int32_t) (ICODE16() & 0xf0) << 24) >> 28;
	if (ICODE16() & 0x100) {
		size = 2;
	} else {
		size = 1;
		NotModOpsize(dst);
	}
	getdst = general_am_get(dst, size, &codelen_dst, GAM_ALL);
	setdst = general_am_set(dst, size, &codelen_dst, GAM_ALL);
	getdst(&Dst, size);
	Result = Dst + Src;
	setdst(Result, size);
	//fprintf(stderr,"ADJNZ %04x, %04x,%04x %d\n",Dst,Src,Result,size);
	add_flags(Dst, Src, Result, size);
	if (!(M16C_REG_FLG & M16C_FLG_ZERO)) {
		int8_t dsp = M16C_Read8(M16C_REG_PC + codelen_dst);
		M16C_REG_PC = (M16C_REG_PC + dsp) & 0xfffff;
	} else {
		M16C_REG_PC += codelen_dst + 1;
	}
	dbgprintf("m16c_adjnz_size_immdstlbl not tested\n");
}

/**
 *********************************************************************************
 * \fn void m16c_and_size_g_immdst(void)
 * Logical and of a immediate and a destination.
 * v0
 *********************************************************************************
 */
void
m16c_and_size_g_immdst(void)
{
	int dst;
	uint32_t Src, Dst, Result;
	int srcsize, opsize;
	int codelen;
	GAM_GetProc *getdst;
	GAM_SetProc *setdst;
	dst = ICODE16() & 0xf;
	if (ICODE16() & 0x100) {
		srcsize = opsize = 2;
	} else {
		srcsize = opsize = 1;
		ModOpsize(dst, &opsize);
	}
	getdst = general_am_get(dst, opsize, &codelen, GAM_ALL);
	setdst = general_am_set(dst, opsize, &codelen, GAM_ALL);
	getdst(&Dst, opsize);
	if (srcsize == 2) {
		Src = M16C_Read16(M16C_REG_PC + codelen);
	} else if (srcsize == 1) {
		Src = M16C_Read8(M16C_REG_PC + codelen);
	}
	Result = Dst & Src;
	setdst(Result, opsize);
	and_flags(Result, opsize);
	M16C_REG_PC += codelen + srcsize;
	dbgprintf("m16c_and_size_immdst not tested\n");
}

/**
 *************************************************************
 * void m16c_and_b_s_immdst(void)
 * Logical and of an eight bit immediate and a destination.
 * v0
 *************************************************************
 */
void
m16c_and_b_s_immdst(void)
{
	int size;
	uint32_t imm;
	uint32_t Dst;
	uint32_t Result;
	int codelen_dst;
	int am = ICODE8() & 7;
	size = 1;
	imm = M16C_Read8(M16C_REG_PC);
	M16C_REG_PC += 1;
	Dst = am2b_get(am, &codelen_dst);
	Result = Dst & imm;
	am2b_set(am, &codelen_dst, Result);
	and_flags(Result, size);
	M16C_REG_PC += codelen_dst;
	dbgprintf("m16c_and_size_s_immdst not tested\n");
}

/**
 ********************************************************************************
 * \fn void m16c_and_size_g_srcdst(void)
 * Logical and of a source and a destination.
 * v0
 ********************************************************************************
 */
void
m16c_and_size_g_srcdst(void)
{
	int dst, src;
	uint32_t Src, Dst, Result;
	int opsize, srcsize;
	int codelen_src;
	int codelen_dst;
	GAM_GetProc *getdst;
	GAM_GetProc *getsrc;
	GAM_SetProc *setdst;
	dst = ICODE16() & 0xf;
	src = (ICODE16() >> 4) & 0xf;
	if (ICODE16() & 0x100) {
		srcsize = opsize = 2;
	} else {
		srcsize = opsize = 1;
		ModOpsize(dst, &opsize);
	}
	getdst = general_am_get(dst, opsize, &codelen_dst, GAM_ALL);
	getsrc = general_am_get(src, srcsize, &codelen_src, GAM_ALL);
	setdst = general_am_set(dst, opsize, &codelen_dst, GAM_ALL);
	getsrc(&Src, srcsize);
	M16C_REG_PC += codelen_src;
	getdst(&Dst, opsize);
	Result = Dst & Src;
	setdst(Result, opsize);
	and_flags(Result, opsize);
	M16C_REG_PC += codelen_dst;
}

/**
 **************************************************************
 * \fn void m16c_and_b_s_srcr0l() 
 * Logical and of a src and R0L/R0H
 * v0
 **************************************************************
 */
void
m16c_and_b_s_srcr0l()
{
	uint8_t Src, Dst, Result;
	int codelen;
	int am = ICODE8() & 7;
	Src = am3b_get(am, &codelen);
	M16C_REG_PC += codelen;
	if (ICODE8() & 4) {
		Dst = M16C_REG_R0H;
		M16C_REG_R0H = Result = Src & Dst;
	} else {
		Dst = M16C_REG_R0L;
		M16C_REG_R0L = Result = Src & Dst;
	}
	and_flags(Result, 1);
	dbgprintf("instr m16c_and_b_s_srcr0l(%04x)\n", ICODE8());
}

/**
 ************************************************************************
 * \fn void m16c_band_src() 
 * The logical and of a source bit and the Carry flag is stored in Carry
 * flag.
 * v0
 ************************************************************************
 */
void
m16c_band_src()
{
	int am = ICODE16() & 0xf;
	int codelen;
	int bit_nr;
	uint32_t data;
	data = get_bitaddr(am, &codelen, &bit_nr);
	if (!(data & (1 << bit_nr))) {
		M16C_REG_FLG &= ~M16C_FLG_CARRY;
	}
	M16C_REG_PC += codelen;
	dbgprintf("instr m16c_band_src(%04x)\n", ICODE16());
}

/**
 *************************************************************************
 * \fn void m16c_bclr_g_dst() 
 * Clear a bit in a destination.
 * v0
 *************************************************************************
 */
void
m16c_bclr_g_dst()
{
	int am = ICODE16() & 0xf;
	int codelen;
	int bit_nr;
	uint32_t value;
	value = get_bitaddr(am, &codelen, &bit_nr);
	value &= ~(1 << bit_nr);
	set_bitaddr(am, value);
	M16C_REG_PC += codelen;
	dbgprintf("instr m16c_bclr_g_dst(%04x)\n", ICODE16());
}

/**
 **************************************************************
 * Clear a bit in memory at an 11 Bit offset to Register SB 
 * v0
 **************************************************************
 */
void
m16c_bclr_s_bit_base()
{
	uint16_t bitnr = ICODE8() & 7;
	uint8_t value;
	int bytenr;
	bytenr = M16C_Read8(M16C_REG_PC);
	M16C_REG_PC += 1;
	value = M16C_Read8(bytenr + M16C_REG_SB);
	value &= ~(1 << bitnr);
	M16C_Write8(value, bytenr + M16C_REG_SB);
	dbgprintf("instr m16c_bclr_s_bit_base(%04x)\n", ICODE16());
}

/*
 ***********************************************************
 * \fn void m16c_bmcnd_dst() 
 * Set/Clear a bit depending on a condition. 
 * v0
 ***********************************************************
 */
void
m16c_bmcnd_dst()
{
	int dst = ICODE16() & 0xf;
	int codelen;
	int bit_nr;
	uint32_t data;
	uint8_t cnd;
	data = get_bitaddr(dst, &codelen, &bit_nr);
	cnd = M16C_Read8(M16C_REG_PC + codelen);
	if (check_condition_8bit(cnd)) {
		data |= (1 << bit_nr);
	} else {
		data &= ~(1 << bit_nr);
	}
	set_bitaddr(dst, data);
	M16C_REG_PC += codelen + 1;
	dbgprintf("instr m16c_bmcnd_dst(%04x)\n", ICODE16());
}

/*
 *****************************************************************
 * \fn void m16c_bmcnd_c() 
 * Set/Clear the carry flag depending on a condition.
 * v0
 *****************************************************************
 */
void
m16c_bmcnd_c()
{
	uint8_t cnd = ICODE16() & 0xf;
	if (check_condition(cnd)) {
		M16C_REG_FLG |= M16C_FLG_CARRY;
	} else {
		M16C_REG_FLG &= ~M16C_FLG_CARRY;
	}
	dbgprintf("instr m16c_bmcnd_c(%04x)\n", ICODE16());
}

/**
 **************************************************************
 * \fn void m16c_bnand_src(void);
 * M16C Manual is buggy here. R8C manual is good
 * v0
 **************************************************************
 */
void
m16c_bnand_src()
{
	int src = ICODE16() & 0xf;
	int codelen;
	int bit_nr;
	uint32_t data;
	data = get_bitaddr(src, &codelen, &bit_nr);
	if (data & (1 << bit_nr)) {
		M16C_REG_FLG &= ~M16C_FLG_CARRY;
	}
	M16C_REG_PC += codelen;
	dbgprintf("instr m16c_bnand_src(%04x)\n", ICODE16());
}

/**
 ******************************************************************
 * \fn void m16c_bnor_src(void) 
 * Logical Or of carry flag and a inverted source.
 * v0
 ******************************************************************
 */
void
m16c_bnor_src()
{
	int src = ICODE16() & 0xf;
	int codelen;
	int bit_nr;
	uint32_t data;
	data = get_bitaddr(src, &codelen, &bit_nr);
	if (!(data & (1 << bit_nr))) {
		M16C_REG_FLG |= M16C_FLG_CARRY;
	}
	M16C_REG_PC += codelen;
	dbgprintf("instr m16c_bnor_src(%04x)\n", ICODE16());
}

/**
 *********************************************************************** 
 * \fn void m16c_bnot_g_dst() 
 * Invert a bit. 
 * v0
 *********************************************************************** 
 */
void
m16c_bnot_g_dst()
{
	int dst = ICODE16() & 0xf;
	int codelen;
	int bit_nr;
	uint32_t data;
	data = get_bitaddr(dst, &codelen, &bit_nr);
	data ^= (1 << bit_nr);
	set_bitaddr(dst, data);
	M16C_REG_PC += codelen;
	dbgprintf("instr m16c_bnot_g_dst(%04x)\n", ICODE16());
}

/**
 *************************************************************
 * \fn void m16c_bnot_s_bit_base() 
 * Invert a bit placed at some offset to SB register.
 * v0 
 *************************************************************
 */
void
m16c_bnot_s_bit_base()
{
	uint16_t bitnr = ICODE8() & 7;
	uint8_t value;
	int bytenr;
	bytenr = M16C_Read8(M16C_REG_PC);
	M16C_REG_PC += 1;
	value = M16C_Read8(bytenr + M16C_REG_SB);
	value ^= ~(1 << bitnr);
	M16C_Write8(value, bytenr + M16C_REG_SB);
	dbgprintf("instr m16c_bnot_s_bit_base(%04x)\n", ICODE8());
}

/**
 ******************************************************************************
 * \fn void m16c_bntst_src() 
 * Write the inverted src bit to Carry and Zero flag. 
 * v0
 ******************************************************************************
 */
void
m16c_bntst_src()
{
	int src;
	int codelen;
	int bit_nr;
	uint32_t data;
	src = ICODE16() & 0xf;
	data = get_bitaddr(src, &codelen, &bit_nr);
	if (data & (1 << bit_nr)) {
		M16C_REG_FLG &= ~(M16C_FLG_CARRY | M16C_FLG_ZERO);
	} else {
		M16C_REG_FLG |= (M16C_FLG_CARRY | M16C_FLG_ZERO);
	}
	M16C_REG_PC += codelen;
	dbgprintf("instr m16c_bntst_src(%04x)\n", ICODE16());
}

/**
 ********************************************************************************
 * \fn void m16c_bnxor_src() 
 * Exclusive or of carry flag and an inverted source.
 * v0
 ********************************************************************************
 */
void
m16c_bnxor_src()
{
	int src;
	int codelen;
	uint32_t data;
	int bit_nr;
	src = ICODE16() & 0xf;
	data = get_bitaddr(src, &codelen, &bit_nr);
	if (!(data & (1 << bit_nr))) {
		M16C_REG_FLG ^= M16C_FLG_CARRY;
	}
	M16C_REG_PC += codelen;
	dbgprintf("instr m16c_bnxor_src(%04x)\n", ICODE16());
}

/*
 **************************************************************************************
 * \fn void m16c_bor_src(void)
 * Logical OR of a source and the Carry flag. 
 * v0
 **************************************************************************************
 */
void
m16c_bor_src()
{
	int src;
	int codelen;
	uint32_t data;
	int bit_nr;
	src = ICODE16() & 0xf;
	data = get_bitaddr(src, &codelen, &bit_nr);
	if (data & (1 << bit_nr)) {
		M16C_REG_FLG |= M16C_FLG_CARRY;
	}
	M16C_REG_PC += codelen;
	dbgprintf("instr m16c_bor_src(%04x)\n", ICODE16());
}

void
m16c_brk()
{
	//gm16c.verbose ^= 1;
	fprintf(stderr, "instr m16c_brk(%04x) at 0x%06x not implemented\n", ICODE8(), M16C_REG_PC);
}

/** 
 **************************************************************************
 * \fn void m16c_bset_g_dst() 
 * Set a bit in a destination.
 **************************************************************************
 */
void
m16c_bset_g_dst()
{
	int dst = ICODE16() & 0xf;
	int codelen;
	uint32_t data;
	int bit_nr;
	data = get_bitaddr(dst, &codelen, &bit_nr);
	data |= (1 << bit_nr);
	set_bitaddr(dst, data);
	M16C_REG_PC += codelen;
	dbgprintf("instr m16c_bset_g_dst(%04x)\n", ICODE16());
}

/**
 ***********************************************************
 * \fn void m16c_bset_s_bit_base() 
 * Set a bit in a destination.
 * v0
 ***********************************************************
 */
void
m16c_bset_s_bit_base()
{
	uint16_t bitnr = ICODE8() & 7;
	uint8_t value;
	int bytenr;
	bytenr = M16C_Read8(M16C_REG_PC);
	value = M16C_Read8(bytenr + M16C_REG_SB);
	value |= (1 << bitnr);
	M16C_Write8(value, bytenr + M16C_REG_SB);
	M16C_REG_PC += 1;
	dbgprintf("instr m16c_bset_s_bit_base(%04x)\n", ICODE8());
}

/**
 *************************************************************
 * \fn void m16c_btst_src() 
 * Test if a bit is set.
 * Negated source to Zero flag and source to Carry.
 *************************************************************
 */
void
m16c_btst_src()
{
	int src;
	int codelen;
	int bit_nr;
	uint32_t data;
	src = ICODE16() & 0xf;
	data = get_bitaddr(src, &codelen, &bit_nr);
	//fprintf(stderr,"btst src %d,data %02x bit %d\n",src,data,bit_nr);
	if (data & (1 << bit_nr)) {
		M16C_REG_FLG = (M16C_REG_FLG | M16C_FLG_CARRY) & ~M16C_FLG_ZERO;
	} else {
		M16C_REG_FLG = (M16C_REG_FLG | M16C_FLG_ZERO) & ~M16C_FLG_CARRY;
	}
	M16C_REG_PC += codelen;
	dbgprintf("instr m16c_btst_src(%04x)\n", ICODE16());
}

/**
 *********************************************************************************** 
 * \fn void m16c_btst_s_bit_base() 
 * v0 
 *********************************************************************************** 
 */
void
m16c_btst_s_bit_base()
{
	uint16_t bitnr = ICODE8() & 7;
	uint8_t value;
	int bytenr;
	bytenr = M16C_Read8(M16C_REG_PC);
	M16C_REG_PC += 1;
	value = M16C_Read8(bytenr + M16C_REG_SB);
	if (value & (1 << bitnr)) {
		M16C_REG_FLG = (M16C_REG_FLG | M16C_FLG_CARRY) & ~M16C_FLG_ZERO;
	} else {
		M16C_REG_FLG = (M16C_REG_FLG | M16C_FLG_ZERO) & ~M16C_FLG_CARRY;
	}
	dbgprintf("instr m16c_btst_s_bit_base(%04x)\n", ICODE8());
}

/**
 *************************************************************************************
 * \fn void m16c_btstc_dst(void) 
 * Test and clear a bit.
 * v0
 *************************************************************************************
 */
void
m16c_btstc_dst()
{
	int dst;
	int codelen;
	int bit_nr;
	uint32_t data;
	dst = ICODE16() & 0xf;
	data = get_bitaddr(dst, &codelen, &bit_nr);
	if (data & (1 << bit_nr)) {
		M16C_REG_FLG = (M16C_REG_FLG | M16C_FLG_CARRY) & ~M16C_FLG_ZERO;
	} else {
		M16C_REG_FLG = (M16C_REG_FLG | M16C_FLG_ZERO) & ~M16C_FLG_CARRY;
	}
	data = data & ~(1 << bit_nr);
	set_bitaddr(dst, data);
	M16C_REG_PC += codelen;
	dbgprintf("instr m16c_btstc_dst(%04x) not implemented\n", ICODE16());
}

/**
 **************************************************************************
 * \fn void m16c_btsts_dst() 
 * Test and set a bit.
 * v0
 **************************************************************************
 */
void
m16c_btsts_dst()
{
	int dst;
	int codelen;
	int bit_nr;
	uint32_t data;
	dst = ICODE16() & 0xf;
	data = get_bitaddr(dst, &codelen, &bit_nr);
	if (data & (1 << bit_nr)) {
		M16C_REG_FLG = (M16C_REG_FLG | M16C_FLG_CARRY) & ~M16C_FLG_ZERO;
	} else {
		M16C_REG_FLG = (M16C_REG_FLG | M16C_FLG_ZERO) & ~M16C_FLG_CARRY;
	}
	data = data | (1 << bit_nr);
	set_bitaddr(dst, data);
	M16C_REG_PC += codelen;
	dbgprintf("instr m16c_btsts_dst(%04x) not implemented\n", ICODE16());
}

/**
 *******************************************************************************
 * \fn void m16c_bxor_src(void) 
 * Exclusive or of a src and the Carry flag is stored in carry flag.
 * v0
 *******************************************************************************
 */
void
m16c_bxor_src()
{
	int src;
	int codelen;
	int bit_nr;
	uint32_t data;
	src = ICODE16() & 0xf;
	data = get_bitaddr(src, &codelen, &bit_nr);
	if (data & (1 << bit_nr)) {
		M16C_REG_FLG ^= M16C_FLG_CARRY;
	}
	M16C_REG_PC += codelen;
	dbgprintf("instr m16c_bxor_src(%04x) not implemented\n", ICODE16());
}

/**
 *****************************************************************************
 * \fn void m16c_cmp_size_g_immdst(void)
 * Compare an immediate source with a destination.
 * v0
 *****************************************************************************
 */
void
m16c_cmp_size_g_immdst(void)
{
	int dst;
	uint32_t Src, Dst, Result;
	int srcsize, opsize;
	int codelen_dst;
	GAM_GetProc *getdst;
	dst = ICODE16() & 0xf;
	if (ICODE16() & 0x100) {
		srcsize = opsize = 2;
	} else {
		srcsize = opsize = 1;
		ModOpsize(dst, &opsize);
	}
	getdst = general_am_get(dst, opsize, &codelen_dst, GAM_ALL);
	getdst(&Dst, opsize);
	if (srcsize == 2) {
		Src = M16C_Read16(M16C_REG_PC + codelen_dst);
	} else if (srcsize == 1) {
		Src = M16C_Read8(M16C_REG_PC + codelen_dst);
	}
	Result = Dst - Src;
	sub_flags(Dst, Src, Result, opsize);
	M16C_REG_PC += codelen_dst + srcsize;
	dbgprintf("m16c_cmp_size_immdst not tested\n");
}

/**
 *************************************************************************
 * \fn void m16c_cmp_size_q_immdst(void)
 * Compare a signed 4 Bit immediate with a destination 
 * v0
 *************************************************************************
 */
void
m16c_cmp_size_q_immdst(void)
{
	int dst;
	uint32_t Dst, Result;
	int32_t Src;
	int opsize;
	int codelen_dst;
	GAM_GetProc *getdst;
	Src = ((int32_t) (ICODE16() & 0xf0) << 24) >> 28;
	dst = ICODE16() & 0xf;
	if (ICODE16() & 0x100) {
		opsize = 2;
	} else {
		opsize = 1;
		ModOpsize(dst, &opsize);
		if (opsize == 2) {
			Src = Src & 0xff;
		}
	}
	getdst = general_am_get(dst, opsize, &codelen_dst, GAM_ALL);
	getdst(&Dst, opsize);
	Result = Dst - Src;
	sub_flags(Dst, Src, Result, opsize);
	M16C_REG_PC += codelen_dst;
	dbgprintf("m16c_cmp_size_q_immdst not tested\n");
}

/**
 *******************************************************************
 * \fn void m16c_cmp_b_s_immdst() 
 * Compare an immediate byte with a destination.
 * v0
 *******************************************************************
 */
void
m16c_cmp_b_s_immdst()
{
	int codelen;
	int opsize = 1;
	uint8_t imm;
	uint8_t Dst;
	uint8_t Result;
	int dst = ICODE8() & 7;
	imm = M16C_Read8(M16C_REG_PC);
	M16C_REG_PC += opsize;
	Dst = am2b_get(dst, &codelen);
	Result = Dst - imm;
	M16C_REG_PC += codelen;
	sub_flags(Dst, imm, Result, opsize);
	dbgprintf("instr m16c_cmp_b_s_immdst(%04x)\n", ICODE8());
}

/**
 **********************************************************************
 * \fn void m16c_cmp_size_g_srcdst(void)
 * Compare a source with a destination.
 * v0
 **********************************************************************
 */

void
m16c_cmp_size_g_srcdst(void)
{
	int dst, src;
	uint32_t Src, Dst, Result;
	int opsize, srcsize;
	int codelen_src;
	int codelen_dst;
	GAM_GetProc *getdst;
	GAM_GetProc *getsrc;
	dst = ICODE16() & 0xf;
	src = (ICODE16() >> 4) & 0xf;
	if (ICODE16() & 0x100) {
		srcsize = opsize = 2;
	} else {
		srcsize = opsize = 1;
		ModOpsize(dst, &opsize);
	}
	getdst = general_am_get(dst, opsize, &codelen_dst, GAM_ALL);
	getsrc = general_am_get(src, srcsize, &codelen_src, GAM_ALL);
	getsrc(&Src, srcsize);
	M16C_REG_PC += codelen_src;
	getdst(&Dst, opsize);
	M16C_REG_PC += codelen_dst;
	Result = Dst - Src;
	sub_flags(Dst, Src, Result, opsize);
	dbgprintf("m16c_cmp_size_g_srcdst not tested\n");
}

/**
 ***************************************************************************
 * \fn void m16c_cmp_b_s_srcr0l() 
 * v0
 ***************************************************************************
 */
void
m16c_cmp_b_s_srcr0l()
{
	uint8_t Src, Dst, Result;
	int codelen;
	int opsize = 1;
	int src = ICODE8() & 7;
	Src = am3b_get(src, &codelen);
	M16C_REG_PC += codelen;
	if (ICODE8() & (1 << 2)) {
		Dst = M16C_REG_R0H;
	} else {
		Dst = M16C_REG_R0L;
	}
	Result = Dst - Src;
	sub_flags(Dst, Src, Result, opsize);
	dbgprintf("instr m16c_cmp_b_s_srcr0l(%04x)\n", ICODE8());
}

static uint32_t
decimal_add_w(uint32_t dst, uint32_t src, uint32_t carry)
{
	uint32_t sum = src + dst + carry;
	if ((((sum & 0xf) < (src & 0xf)) || ((sum & 0xf) > 0x9))) {
		sum += 0x6;
	}
	if ((((sum & 0xf0) < (src & 0xf0)) || ((sum & 0xf0) > 0x90))) {
		sum += 0x60;
	}
	if ((((sum & 0xf00) < (src & 0xf00)) || ((sum & 0xf00) > 0x900))) {
		sum += 0x600;
	}
	if ((((sum & 0xf000) < (src & 0xf000)) || ((sum & 0xf000) > 0x9000))) {
		sum += 0x6000;
	}
	return sum;
}

static uint32_t
decimal_add_b(uint32_t dst, uint32_t src, uint32_t carry)
{
	uint32_t sum = src + dst + carry;
	if ((((sum & 0xf) < (src & 0xf)) || ((sum & 0xf) > 0x9))) {
		sum += 0x6;
	}
	if ((((sum & 0xf0) < (src & 0xf0)) || ((sum & 0xf0) > 0x90))) {
		sum += 0x60;
	}
	return sum;
}

/**
 **************************************************************
 * \fn void m16c_dadc_b_imm8_r0l() 
 * Decimal add with carry.
 * v0
 **************************************************************
 */
void
m16c_dadc_b_imm8_r0l()
{
	uint32_t Src = M16C_Read8(M16C_REG_PC);
	uint32_t Result;
	uint32_t Dst;
	M16C_REG_PC += 1;
	Dst = M16C_REG_R0L;
	if (M16C_REG_FLG & M16C_FLG_CARRY) {
		Result = decimal_add_b(Dst, Src, 1);
	} else {
		Result = decimal_add_b(Dst, Src, 0);
	}
	M16C_REG_FLG &= ~(M16C_FLG_CARRY | M16C_FLG_SIGN | M16C_FLG_ZERO);
	if (Result > 0xff) {
		M16C_REG_FLG |= M16C_FLG_CARRY;
	}
	if (Result & 0x80) {
		M16C_REG_FLG |= M16C_FLG_SIGN;
	}
	if (!(Result & 0xff)) {
		M16C_REG_FLG |= M16C_FLG_ZERO;
	}
	M16C_REG_R0L = Result;
	dbgprintf("instr m16c_dadc_b_imm8_r0l(%04x)\n", ICODE16());
}

/**
 *******************************************************************
 * \fn void m16c_dadc_w_imm16_r0() 
 * Decimal add word with carry. 
 * v0
 *******************************************************************
 */
void
m16c_dadc_w_imm16_r0()
{
	uint32_t Src = M16C_Read16(M16C_REG_PC);
	uint32_t Result;
	uint32_t Dst;
	M16C_REG_PC += 2;
	Dst = M16C_REG_R0;

	if (M16C_REG_FLG & M16C_FLG_CARRY) {
		Result = decimal_add_w(Dst, Src, 1);
	} else {
		Result = decimal_add_w(Dst, Src, 0);
	}
	M16C_REG_FLG &= ~(M16C_FLG_CARRY | M16C_FLG_SIGN | M16C_FLG_ZERO);
	if (Result > 0xffff) {
		M16C_REG_FLG |= M16C_FLG_CARRY;
	}
	if (Result & 0x8000) {
		M16C_REG_FLG |= M16C_FLG_SIGN;
	}
	if (!(Result & 0xffff)) {
		M16C_REG_FLG |= M16C_FLG_ZERO;
	}
	M16C_REG_R0 = Result;
	dbgprintf("instr m16c_dadc_w_imm16_r0(%04x) not implemented\n", ICODE16());
}

/**
 ************************************************************************
 * \fn void m16c_dadc_b_r0h_r0l() 
 * Decimal add with carry R0H and R0L
 * v0
 ************************************************************************
 */
void
m16c_dadc_b_r0h_r0l()
{
	uint32_t Result;
	uint32_t Src, Dst;
	Src = M16C_REG_R0H;
	Dst = M16C_REG_R0L;
	if (M16C_REG_FLG & M16C_FLG_CARRY) {
		Result = decimal_add_b(Dst, Src, 1);
	} else {
		Result = decimal_add_b(Dst, Src, 0);
	}
	M16C_REG_FLG &= ~(M16C_FLG_CARRY | M16C_FLG_SIGN | M16C_FLG_ZERO);
	if (Result > 0xff) {
		M16C_REG_FLG |= M16C_FLG_CARRY;
	}
	if (Result & 0x80) {
		M16C_REG_FLG |= M16C_FLG_SIGN;
	}
	if ((Result & 0xff) == 0) {
		M16C_REG_FLG |= M16C_FLG_ZERO;
	}
	M16C_REG_R0L = Result;
	dbgprintf("instr m16c_dadc_b_r0h_r0l(%04x) not implemented\n", ICODE16());
}

/**
 ***************************************************************** 
 * \fn void 
 * m16c_dadc_w_r0_r1() 
 * Decimal add the Src R1 and the Carry to the Destination R0.
 * v0
 ***************************************************************** 
 */
void
m16c_dadc_w_r0_r1()
{
	uint32_t Result;
	uint32_t Src, Dst;
	Src = M16C_REG_R1;
	Dst = M16C_REG_R0;

	if (M16C_REG_FLG & M16C_FLG_CARRY) {
		Result = decimal_add_w(Dst, Src, 1);
	} else {
		Result = decimal_add_w(Dst, Src, 0);
	}
	M16C_REG_FLG &= ~(M16C_FLG_CARRY | M16C_FLG_SIGN | M16C_FLG_ZERO);
	if (Result > 0xffff) {
		M16C_REG_FLG |= M16C_FLG_CARRY;
	}
	if (Result & 0x8000) {
		M16C_REG_FLG |= M16C_FLG_SIGN;
	}
	if (!(Result & 0xffff)) {
		M16C_REG_FLG |= M16C_FLG_ZERO;
	}

	M16C_REG_R0 = Result;
	dbgprintf("instr m16c_dadc_w_r0_r1(%04x) not implemented\n", ICODE16());
}

/**
 ********************************************************************
 * \fn void m16c_dadd_b_imm8_r0l() 
 * Decimal Add an 8 bit immediate to R0L.
 * v0
 ********************************************************************
 */
void
m16c_dadd_b_imm8_r0l()
{
	uint32_t Src = M16C_Read8(M16C_REG_PC);
	uint32_t Result;
	uint32_t Dst;
	M16C_REG_PC += 1;
	Dst = M16C_REG_R0L;
	Result = decimal_add_b(Dst, Src, 0);
	M16C_REG_FLG &= ~(M16C_FLG_CARRY | M16C_FLG_SIGN | M16C_FLG_ZERO);
	if (Result > 0xff) {
		M16C_REG_FLG |= M16C_FLG_CARRY;
	}
	if (Result & 0x80) {
		M16C_REG_FLG |= M16C_FLG_SIGN;
	}
	if (!(Result & 0xff)) {
		M16C_REG_FLG |= M16C_FLG_ZERO;
	}
	M16C_REG_R0L = Result;
	dbgprintf("instr m16c_dadd_b_imm8_r0l(%04x) not implemented\n", ICODE16());
}

/**
 ********************************************************************
 * \fn void m16c_dadd_w_imm16_r0() 
 * Decimal add an 16 bit immediate to register R0.
 * v0
 ******************************************************************** 
 */
void
m16c_dadd_w_imm16_r0()
{
	uint32_t Src = M16C_Read16(M16C_REG_PC);
	uint32_t Result;
	uint32_t Dst;
	M16C_REG_PC += 2;
	Dst = M16C_REG_R0;
	Result = decimal_add_w(Dst, Src, 0);
	M16C_REG_FLG &= ~(M16C_FLG_CARRY | M16C_FLG_SIGN | M16C_FLG_ZERO);
	if (Result > 0xffff) {
		M16C_REG_FLG |= M16C_FLG_CARRY;
	}
	if (Result & 0x8000) {
		M16C_REG_FLG |= M16C_FLG_SIGN;
	}
	if (!(Result & 0xffff)) {
		M16C_REG_FLG |= M16C_FLG_ZERO;
	}
	M16C_REG_R0 = Result;
	dbgprintf("instr m16c_dadd_w_imm16_r0(%04x) not implemented\n", ICODE16());
}

/**
 *************************************************************************
 * \fn void m16c_dadd_b_r0h_r0l() 
 * Add Register R0H tor register R0L.
 * v0
 *************************************************************************
 */
void
m16c_dadd_b_r0h_r0l()
{
	uint32_t Result;
	uint32_t Src, Dst;
	Src = M16C_REG_R0H;
	Dst = M16C_REG_R0L;
	Result = decimal_add_b(Dst, Src, 0);
	M16C_REG_FLG &= ~(M16C_FLG_CARRY | M16C_FLG_SIGN | M16C_FLG_ZERO);
	if (Result > 0xff) {
		M16C_REG_FLG |= M16C_FLG_CARRY;
	}
	if (Result & 0x80) {
		M16C_REG_FLG |= M16C_FLG_SIGN;
	}
	if ((Result & 0xff) == 0) {
		M16C_REG_FLG |= M16C_FLG_ZERO;
	}
	M16C_REG_R0L = Result;
	dbgprintf("instr m16c_dadd_b_r0h_r0l(%04x) not implemented\n", ICODE16());
}

/**
 ****************************************************************
 * \fn void m16c_dadd_w_r0_r1() 
 * Decimal Add register R1 to Register R0
 ****************************************************************
 */
void
m16c_dadd_w_r0_r1()
{
	uint32_t Result;
	uint32_t Src, Dst;
	Src = M16C_REG_R1;
	Dst = M16C_REG_R0;
	Result = decimal_add_w(Dst, Src, 0);
	M16C_REG_FLG &= ~(M16C_FLG_CARRY | M16C_FLG_SIGN | M16C_FLG_ZERO);
	if (Result > 0xffff) {
		M16C_REG_FLG |= M16C_FLG_CARRY;
	}
	if (Result & 0x8000) {
		M16C_REG_FLG |= M16C_FLG_SIGN;
	}
	if (!(Result & 0xffff)) {
		M16C_REG_FLG |= M16C_FLG_ZERO;
	}

	M16C_REG_R0 = Result;
	dbgprintf("instr m16c_dadd_w_r0_r1(%04x) not implemented\n", ICODE16());
}

/*
 *************************************************************
 * \fn void m16c_dec_b_dst() 
 * Decrement a destination. 
 * v0
 *************************************************************
 */
void
m16c_dec_b_dst()
{
	int codelen;
	uint8_t value;
	int am = ICODE8() & 7;
	value = am2b_get(am, &codelen);
	value--;
	sgn_zero_flags(value, 1);
	am2b_set(ICODE8(), &codelen, value);
	M16C_REG_PC += codelen;
	dbgprintf("instr m16c_dec_b_dst(%04x) \n", ICODE8());
}

/**
 ********************************************************
 * \fn void m16c_dec_w_dst() 
 * Decrement the 16 Bit destination A0/A1
 * v0
 ********************************************************
 */
void
m16c_dec_w_dst()
{
	uint16_t value;
	dbgprintf("instr m16c_dec_w_dst(%04x) not implemented\n", ICODE8());
	if (ICODE8() & (1 << 3)) {
		M16C_REG_A1--;
		value = M16C_REG_A1;

	} else {
		M16C_REG_A0--;
		value = M16C_REG_A0;
	}
	sgn_zero_flags(value, 2);
}

/**
 **********************************************************
 * \fn void m16c_div_b_imm() 
 * Divide R0 by an 8 Bit signed immediate.
 * Store quotient in R0L and remainder in R0H.
 * v0
 **********************************************************
 */
void
m16c_div_b_imm()
{
	int16_t r0;
	uint8_t remainder;
	int16_t quotient;
	int8_t div;
	div = M16C_Read8(M16C_REG_PC);
	M16C_REG_PC += 1;
	if (div) {
		r0 = M16C_REG_R0;
		quotient = r0 / div;
		remainder = r0 - (quotient * div);
		if ((quotient < -128) || (quotient > 127)) {
			M16C_REG_FLG |= M16C_FLG_OVERFLOW;
		} else {
			M16C_REG_R0L = quotient;
			M16C_REG_R0H = remainder;
			M16C_REG_FLG &= ~M16C_FLG_OVERFLOW;
		}
	} else {
		M16C_REG_FLG |= M16C_FLG_OVERFLOW;
	}
	dbgprintf("instr m16c_div_size_imm(%04x) not implemented\n", ICODE16());
}

/**
 *********************************************************
 * \fn void m16c_div_w_imm() 
 * Divide R2R0 by a 16 Bit immediate.
 * Store the quotient in R0 and the remainder in R2.
 * v0
 *********************************************************
 */
void
m16c_div_w_imm()
{
	int32_t Dst;
	uint16_t remainder;
	int32_t quotient;
	int16_t div;
	div = M16C_Read16(M16C_REG_PC);
	M16C_REG_PC += 2;
	if (div) {
		Dst = M16C_REG_R0 | ((uint32_t) M16C_REG_R2 << 16);
		quotient = Dst / div;
		remainder = Dst - (quotient * div);
		if ((quotient < -32768) || (quotient > 32767)) {
			M16C_REG_FLG |= M16C_FLG_OVERFLOW;
		} else {
			M16C_REG_R0 = quotient;
			M16C_REG_R2 = remainder;
			M16C_REG_FLG &= ~M16C_FLG_OVERFLOW;
		}
	} else {
		M16C_REG_FLG |= M16C_FLG_OVERFLOW;
	}
}

/**
 *************************************************************
 * \fn void m16c_div_b_src() 
 * divide R0 by a source. Store the result in R0L/R0H. 
 * v0
 *************************************************************
 */
void
m16c_div_b_src()
{
	int codelen_src;
	int size = 1;
	int16_t Dst;
	int8_t remainder;
	int16_t quotient;
	int8_t Div;
	int src = ICODE16() & 0xf;
	uint32_t Src;
	GAM_GetProc *getsrc;
	getsrc = general_am_get(src, size, &codelen_src, GAM_ALL);
	getsrc(&Src, size);
	M16C_REG_PC += codelen_src;
	Div = Src;
	if (Div) {
		Dst = M16C_REG_R0;
		quotient = Dst / Div;
		remainder = Dst - (quotient * Div);
		if ((quotient < -128) || (quotient > 127)) {
			M16C_REG_FLG |= M16C_FLG_OVERFLOW;
		} else {
			M16C_REG_R0L = quotient;
			M16C_REG_R0H = remainder;
			M16C_REG_FLG &= ~M16C_FLG_OVERFLOW;
		}
	} else {
		M16C_REG_FLG |= M16C_FLG_OVERFLOW;
	}
	dbgprintf("instr m16c_div_size_src(%04x)\n", ICODE16());
}

/**
 ***************************************************************
 * \fn void m16c_div_w_src() 
 * Divide R2R0 by a 16 Bit source.
 ***************************************************************
 */
void
m16c_div_w_src()
{
	int codelen_src;
	int size = 2;
	uint32_t Src;
	int32_t Dst;
	int16_t remainder;
	int32_t quotient;
	int16_t Div;
	int src = ICODE16() & 0xf;
	GAM_GetProc *getsrc;
	getsrc = general_am_get(src, size, &codelen_src, GAM_ALL);
	getsrc(&Src, size);
	M16C_REG_PC += codelen_src;
	Div = Src;
	if (Div) {
		Dst = M16C_REG_R0 | ((uint32_t) M16C_REG_R2 << 16);
		quotient = Dst / Div;
		remainder = Dst - (quotient * Div);
		if ((quotient < -32768) || (quotient > 32767)) {
			M16C_REG_FLG |= M16C_FLG_OVERFLOW;
		} else {
			M16C_REG_R0 = quotient;
			M16C_REG_R2 = remainder;
			M16C_REG_FLG &= ~M16C_FLG_OVERFLOW;
		}
	} else {
		M16C_REG_FLG |= M16C_FLG_OVERFLOW;
	}
	dbgprintf("instr m16c_div_size_src(%04x)\n", ICODE16());
}

/** 
 ****************************************************************
 * \fn void m16c_divu_b_imm() 
 * Unsigned divide of R0 by an 8 bit immediate.
 * v0
 ****************************************************************
 */
void
m16c_divu_b_imm()
{
	uint16_t Dst;
	uint8_t remainder;
	uint16_t quotient;
	uint8_t Div;
	Div = M16C_Read8(M16C_REG_PC);
	M16C_REG_PC += 1;
	if (Div) {
		Dst = M16C_REG_R0;
		quotient = Dst / Div;
		remainder = Dst - (quotient * Div);

		if (quotient & 0xff00) {
			M16C_REG_FLG |= M16C_FLG_OVERFLOW;
		} else {
			M16C_REG_R0L = quotient;
			M16C_REG_R0H = remainder;
			M16C_REG_FLG &= ~M16C_FLG_OVERFLOW;
		}
	} else {
		M16C_REG_FLG |= M16C_FLG_OVERFLOW;
	}
	dbgprintf("instr m16c_divu_size_imm(%04x)\n", ICODE16());
}

/**
 *******************************************************************
 * \fn void m16c_divu_w_imm() 
 * Divide R2R0 by a 16 bit immediate.
 * v0
 *******************************************************************
 */
void
m16c_divu_w_imm()
{
	uint32_t Dst;
	uint16_t remainder;
	uint32_t quotient;
	uint16_t Div;
	Div = M16C_Read16(M16C_REG_PC);
	M16C_REG_PC += 2;
	if (Div) {
		Dst = M16C_REG_R0 | ((uint32_t) M16C_REG_R2 << 16);
		quotient = Dst / Div;
		remainder = Dst - (quotient * Div);
		if (quotient & 0xffff0000) {
			M16C_REG_FLG |= M16C_FLG_OVERFLOW;
		} else {
			M16C_REG_R0 = quotient;
			M16C_REG_R2 = remainder;
			M16C_REG_FLG &= ~M16C_FLG_OVERFLOW;
		}
	} else {
		M16C_REG_FLG |= M16C_FLG_OVERFLOW;
	}
	dbgprintf("instr m16c_divu_w_imm(%04x)\n", ICODE16());
}

/**
 ******************************************************************
 * \fn void m16c_divu_b_src() 
 * Unsigned divide Register R0 by a source.
 * v0
 ******************************************************************
 */
void
m16c_divu_b_src()
{
	int codelen_src;
	int size = 1;
	int src = ICODE16() & 0xf;
	uint16_t Dst;
	uint8_t remainder;
	uint16_t quotient;
	uint8_t Div;
	uint32_t Src;
	GAM_GetProc *getsrc;
	getsrc = general_am_get(src, size, &codelen_src, GAM_ALL);
	getsrc(&Src, size);
	M16C_REG_PC += codelen_src;
	Div = Src;
	if (Div) {
		Dst = M16C_REG_R0;
		quotient = Dst / Div;
		remainder = Dst - (quotient * Div);
		if (quotient & 0xff00) {
			M16C_REG_FLG |= M16C_FLG_OVERFLOW;
		} else {
			M16C_REG_R0L = quotient;
			M16C_REG_R0H = remainder;
			M16C_REG_FLG &= ~M16C_FLG_OVERFLOW;
		}
	} else {
		M16C_REG_FLG |= M16C_FLG_OVERFLOW;
	}
	dbgprintf("instr m16c_divu_size_src(%04x) not implemented\n", ICODE16());
}

/**
 *******************************************************************
 * \fn void m16c_divu_w_src() 
 * Divide R2R0 by a source. 
 * v0
 *******************************************************************
 */
void
m16c_divu_w_src()
{
	int codelen_src;
	int size = 2;
	int src = ICODE16() & 0xf;
	uint32_t Dst;
	uint16_t remainder;
	uint32_t quotient;
	uint16_t Div;
	uint32_t Src;
	GAM_GetProc *getsrc;
	getsrc = general_am_get(src, size, &codelen_src, GAM_ALL);
	getsrc(&Src, size);
	M16C_REG_PC += codelen_src;
	Div = Src;
	if (Div) {
		Dst = M16C_REG_R0 | ((uint32_t) M16C_REG_R2 << 16);
		quotient = Dst / Div;
		remainder = Dst - (quotient * Div);
		if (quotient & 0xffff0000) {
			M16C_REG_FLG |= M16C_FLG_OVERFLOW;
		} else {
			M16C_REG_R0 = quotient;
			M16C_REG_R2 = remainder;
			M16C_REG_FLG &= ~M16C_FLG_OVERFLOW;
		}
	} else {
		M16C_REG_FLG |= M16C_FLG_OVERFLOW;
	}
	dbgprintf("instr m16c_divu_size_src(%04x) not implemented\n", ICODE16());
}

/*
 **************************************************************
 * \fn void m16c_divx_b_imm() 
 * Divide signed .
 * Remainder has the same sign as the divisor.
 * v0
 **************************************************************
 */
void
m16c_divx_b_imm()
{
	int8_t Div;
	int16_t Dst;
	int8_t remainder;
	int16_t quotient;
	Div = M16C_Read8(M16C_REG_PC);
	M16C_REG_PC += 1;
	if (!Div) {
		M16C_REG_FLG |= M16C_FLG_OVERFLOW;
	} else {
		Dst = M16C_REG_R0;
		if (Div < -1) {
			quotient = (Dst - (Div + 1)) / Div;
		} else {
			quotient = Dst / Div;
		}
		remainder = Dst - (quotient * Div);
		//assert((remainder == 0) || (signum(remainder) == signum(Div)));
		if (quotient != (int16_t) (int8_t) quotient) {
			M16C_REG_FLG |= M16C_FLG_OVERFLOW;
		} else {
			M16C_REG_R0L = quotient;
			M16C_REG_R0H = remainder;
			M16C_REG_FLG &= ~M16C_FLG_OVERFLOW;
		}
	}
	dbgprintf("instr m16c_divx_b_imm(%04x)\n", ICODE16());
}

/*
 ***************************************************************
 * \fn void m16c_divx_w_imm() 
 * Signed divide of R2R0 by a 16 Bit immediate.
 * Remainder has the same sign as the divisor.
 * v0
 ***************************************************************
 */
void
m16c_divx_w_imm()
{
	int16_t Div;
	int32_t Dst;
	int16_t remainder;
	int32_t quotient;
	Div = M16C_Read16(M16C_REG_PC);
	M16C_REG_PC += 2;
	if (!Div) {
		M16C_REG_FLG |= M16C_FLG_OVERFLOW;
	} else {
		Dst = M16C_REG_R0 | ((uint32_t) M16C_REG_R2 << 16);
		if (Div < -1) {
			quotient = (Dst - (Div + 1)) / Div;
		} else {
			quotient = Dst / Div;
		}
		remainder = Dst - (quotient * Div);
		//assert((remainder == 0) || (signum(remainder) == signum(Div)));
		if (quotient != (int32_t) (int16_t) quotient) {
			M16C_REG_FLG |= M16C_FLG_OVERFLOW;
		} else {
			M16C_REG_R0 = quotient;
			M16C_REG_R2 = remainder;
			M16C_REG_FLG &= ~M16C_FLG_OVERFLOW;
		}
	}
	dbgprintf("instr m16c_divx_size_imm(%04x) not implemented\n", ICODE16());
}

/**
 **********************************************************************
 * \fn void m16c_divx_b_src() 
 * Signed divide of R0 by a source.
 * The remainder has the same sign as the divisor.
 * v0
 **********************************************************************
 */

void
m16c_divx_b_src()
{
	int8_t Div;
	int16_t Dst;
	int8_t remainder;
	int16_t quotient;
	int size = 1;
	int src;
	int codelen_src;
	uint32_t Src;
	GAM_GetProc *getsrc;
	src = ICODE16() & 0xf;

	getsrc = general_am_get(src, size, &codelen_src, GAM_ALL);
	getsrc(&Src, size);
	Div = Src;
	if (!Div) {
		M16C_REG_FLG |= M16C_FLG_OVERFLOW;
	} else {
		Dst = (int16_t) M16C_REG_R0;
		if (Div < 0) {
			quotient = (Dst - (Div + 1)) / Div;
		} else {
			quotient = Dst / Div;
		}
		remainder = Dst - (quotient * Div);
		//assert((remainder == 0) || (signum(remainder) == signum(Div)));
		if (quotient != (int16_t) (int8_t) quotient) {
			M16C_REG_FLG |= M16C_FLG_OVERFLOW;
		} else {
			M16C_REG_R0L = quotient;
			M16C_REG_R0H = remainder;
			M16C_REG_FLG &= ~M16C_FLG_OVERFLOW;
		}
	}
	M16C_REG_PC += codelen_src;
	dbgprintf("instr m16c_divx_size_src(%04x) not implemented\n", ICODE16());
}

/**
 ******************************************************************
 * \fn void m16c_divx_w_src() 
 * Signed divide of R2R0 by a 16 bit source.
 * The remainder has the same sign as the divisor. 
 * v0
 ******************************************************************
 */
void
m16c_divx_w_src()
{
	int size = 2;
	int src;
	int codelen_src;
	uint32_t Src;
	int16_t Div;
	int32_t Dst;
	int16_t remainder;
	int32_t quotient;
	GAM_GetProc *getsrc;
	src = ICODE16() & 0xf;

	getsrc = general_am_get(src, size, &codelen_src, GAM_ALL);
	getsrc(&Src, size);
	Div = Src;
	if (!Div) {
		M16C_REG_FLG |= M16C_FLG_OVERFLOW;
	} else {
		Dst = (int32_t) (M16C_REG_R0 | ((uint32_t) M16C_REG_R2 << 16));
		if (Div < 0) {
			quotient = (Dst - (Div + 1)) / Div;
		} else {
			quotient = Dst / Div;
		}
		remainder = Dst - (quotient * Div);
		//assert((remainder == 0) || (signum(remainder) == signum(Div)));
		if (quotient != (int32_t) (int16_t) quotient) {
			M16C_REG_FLG |= M16C_FLG_OVERFLOW;
		} else {
			M16C_REG_R0 = quotient;
			M16C_REG_R2 = remainder;
			M16C_REG_FLG &= ~M16C_FLG_OVERFLOW;
		}
	}
	M16C_REG_PC += codelen_src;
	dbgprintf("instr m16c_divx_size_src(%04x) not implemented\n", ICODE16());
}

/*
 ***************************************************************
 * Decimal subtract
 * Wrong for non bcd numbers
 * v0
 ***************************************************************
 */
void
m16c_dsbb_b_imm8_r0l()
{
	uint8_t imm8;
	uint8_t Result;
	uint8_t Src, Dst;
	imm8 = M16C_Read8(M16C_REG_PC);
	M16C_REG_PC += 1;
	Src = bcd_to_uint16(imm8);
	Dst = bcd_to_uint16(M16C_REG_R0L);

	if (M16C_REG_FLG & M16C_FLG_CARRY) {
		Result = Dst - Src;
	} else {
		Result = Dst - Src - 1;
	}
	M16C_REG_FLG &= ~(M16C_FLG_CARRY | M16C_FLG_SIGN | M16C_FLG_ZERO);
	if (Result & 0x80) {
		Result += 100;
	} else {
		M16C_REG_FLG |= M16C_FLG_CARRY;
	}
	if (Result & 0x80) {
		M16C_REG_FLG |= M16C_FLG_SIGN;
	}
	if (!Result) {
		M16C_REG_FLG |= M16C_FLG_ZERO;
	}
	M16C_REG_R0L = uint16_to_bcd(Result);
	dbgprintf("instr m16c_dsbb_b_imm8_r0l(%04x) not implemented\n", ICODE16());
}

/*
 ******************************************************************
 * \fn void m16c_dsbb_w_imm16_r0() 
 * Decimal subtract a word from a 16 Bit immediate 
 * Wrong for non BCD numbers.
 ******************************************************************
 */
void
m16c_dsbb_w_imm16_r0()
{
	uint16_t imm16;
	uint16_t Result;
	uint16_t Src, Dst;
	imm16 = M16C_Read16(M16C_REG_PC);
	M16C_REG_PC += 2;
	Src = bcd_to_uint16(imm16);
	Dst = bcd_to_uint16(M16C_REG_R0);

	if (M16C_REG_FLG & M16C_FLG_CARRY) {
		Result = Dst - Src;
	} else {
		Result = Dst - Src - 1;
	}
	M16C_REG_FLG &= ~(M16C_FLG_CARRY | M16C_FLG_SIGN | M16C_FLG_ZERO);
	if (Result & 0x8000) {
		Result += 10000;
	} else {
		M16C_REG_FLG |= M16C_FLG_CARRY;
	}
	if (Result & 0x8000) {
		M16C_REG_FLG |= M16C_FLG_SIGN;
	}
	if (!Result) {
		M16C_REG_FLG |= M16C_FLG_ZERO;
	}
	M16C_REG_R0 = uint16_to_bcd(Result);
	dbgprintf("instr m16c_dsbb_w_imm16_r0(%04x) not implemented\n", ICODE16());
}

/**
 *************************************************************
 * \fn void m16c_dsbb_b_r0h_r0l() 
 * Wrong for non BCD numbers.
 * v0
 *************************************************************
 */
void
m16c_dsbb_b_r0h_r0l()
{
	uint8_t Result;
	uint8_t Src, Dst;
	Src = bcd_to_uint16(M16C_REG_R0H);
	Dst = bcd_to_uint16(M16C_REG_R0L);

	if (M16C_REG_FLG & M16C_FLG_CARRY) {
		Result = Dst - Src;
	} else {
		Result = Dst - Src - 1;
	}
	M16C_REG_FLG &= ~(M16C_FLG_CARRY | M16C_FLG_SIGN | M16C_FLG_ZERO);
	if (Result & 0x80) {
		Result += 100;
	} else {
		M16C_REG_FLG |= M16C_FLG_CARRY;
	}
	if (Result & 0x80) {
		M16C_REG_FLG |= M16C_FLG_SIGN;
	}
	if (!Result) {
		M16C_REG_FLG |= M16C_FLG_ZERO;
	}
	M16C_REG_R0L = uint16_to_bcd(Result);
	dbgprintf("instr m16c_dsbb_b_r0h_r0l(%04x) not implemented\n", ICODE16());
}

/**
 *************************************************
 * \fn void m16c_dsbb_w_r1_r0() 
 * Decimal subtract Register R1 from Register R0. 
 * Wrong for non BCD numbers.
 * v0
 *************************************************
 */
void
m16c_dsbb_w_r1_r0()
{
	uint16_t Result;
	uint16_t Src, Dst;
	Src = bcd_to_uint16(M16C_REG_R1);
	Dst = bcd_to_uint16(M16C_REG_R0);

	if (M16C_REG_FLG & M16C_FLG_CARRY) {
		Result = Dst - Src;
	} else {
		Result = Dst - Src - 1;
	}
	M16C_REG_FLG &= ~(M16C_FLG_CARRY | M16C_FLG_SIGN | M16C_FLG_ZERO);
	if (Result & 0x8000) {
		Result += 10000;
	} else {
		M16C_REG_FLG |= M16C_FLG_CARRY;
	}
	if (Result & 0x8000) {
		M16C_REG_FLG |= M16C_FLG_SIGN;
	}
	if (!Result) {
		M16C_REG_FLG |= M16C_FLG_ZERO;
	}
	M16C_REG_R0 = uint16_to_bcd(Result);
	dbgprintf("instr m16c_dsbb_w_r1_r0(%04x)\n", ICODE16());
}

/**
 *************************************************************
 * \fn void m16c_dsub_b_imm8_r0l() 
 * Wrong for non BCD numbers. 
 * v0
 *************************************************************
 */
void
m16c_dsub_b_imm8_r0l()
{
	uint8_t imm8;
	uint8_t Result;
	uint8_t Src, Dst;
	imm8 = M16C_Read8(M16C_REG_PC);
	M16C_REG_PC += 1;
	Src = bcd_to_uint16(imm8);
	Dst = bcd_to_uint16(M16C_REG_R0L);
	Result = Dst - Src;
	M16C_REG_FLG &= ~(M16C_FLG_CARRY | M16C_FLG_SIGN | M16C_FLG_ZERO);
	if (Result & 0x80) {
		Result = Result + 100;
	} else {
		M16C_REG_FLG |= M16C_FLG_CARRY;
	}
	if (Result & 0x80) {
		M16C_REG_FLG |= M16C_FLG_SIGN;
	}
	if (!Result) {
		M16C_REG_FLG |= M16C_FLG_ZERO;
	}
	M16C_REG_R0L = uint16_to_bcd(Result);
	dbgprintf("instr m16c_dsub_b_imm8_r0l(%04x) not implemented\n", ICODE16());
}

/**
 *******************************************************
 * \fn void m16c_dsub_w_imm16_r0() 
 * Decimal subtract a 16 bit immediate from R0.
 * Wrong for non BCD numbers.
 *******************************************************
 */
void
m16c_dsub_w_imm16_r0()
{
	uint16_t imm16;
	uint16_t Result;
	uint16_t Src, Dst;
	imm16 = M16C_Read16(M16C_REG_PC);
	M16C_REG_PC += 2;
	Src = bcd_to_uint16(imm16);
	Dst = bcd_to_uint16(M16C_REG_R0);
	Result = Dst - Src;
	M16C_REG_FLG &= ~(M16C_FLG_CARRY | M16C_FLG_SIGN | M16C_FLG_ZERO);
	if (Result & 0x8000) {
		Result += 10000;
	} else {
		M16C_REG_FLG |= M16C_FLG_CARRY;
	}
	if (Result & 0x8000) {
		M16C_REG_FLG |= M16C_FLG_SIGN;
	}
	if (Result == 0) {
		M16C_REG_FLG |= M16C_FLG_ZERO;
	}
	M16C_REG_R0 = uint16_to_bcd(Result);
	dbgprintf("instr m16c_dsub_w_imm16_r0(%04x) not implemented\n", ICODE16());
}

/**
 *****************************************************************
 * \fn void m16c_dsub_b_r0h_r0l() 
 * Wrong for non BCD numbers.
 * v0
 *****************************************************************
 */
void
m16c_dsub_b_r0h_r0l()
{
	uint8_t Result;
	uint8_t Src, Dst;
	Src = bcd_to_uint16(M16C_REG_R0H);
	Dst = bcd_to_uint16(M16C_REG_R0L);
	Result = Dst - Src;
	M16C_REG_FLG &= ~(M16C_FLG_CARRY | M16C_FLG_SIGN | M16C_FLG_ZERO);
	if (Result & 0x80) {
		Result += 100;
	} else {
		M16C_REG_FLG |= M16C_FLG_CARRY;
	}
	if (Result & 0x80) {
		M16C_REG_FLG |= M16C_FLG_SIGN;
	}
	if (Result == 0) {
		M16C_REG_FLG |= M16C_FLG_ZERO;
	}
	M16C_REG_R0L = uint16_to_bcd(Result);
	dbgprintf("instr m16c_dsub_b_r0h_r0l(%04x) not implemented\n", ICODE16());
}

/**
 *****************************************************************
 * \fn void m16c_dsub_w_r1_r0() 
 * Wrong for non BCD numbers.
 * v0
 *****************************************************************
 */
void
m16c_dsub_w_r1_r0()
{
	uint16_t Result;
	uint16_t Src, Dst;
	Src = bcd_to_uint16(M16C_REG_R1);
	Dst = bcd_to_uint16(M16C_REG_R0);
	Result = Dst - Src;
	M16C_REG_FLG &= ~(M16C_FLG_CARRY | M16C_FLG_SIGN | M16C_FLG_ZERO);
	if (Result & 0x8000) {
		Result += 10000;
	} else {
		M16C_REG_FLG |= M16C_FLG_CARRY;
	}
	if (Result & 0x8000) {
		M16C_REG_FLG |= M16C_FLG_SIGN;
	}
	if (!Result) {
		M16C_REG_FLG |= M16C_FLG_ZERO;
	}
	M16C_REG_R0 = uint16_to_bcd(Result);
	dbgprintf("instr m16c_dsub_w_r1_r0(%04x) not implemented\n", ICODE16());
}

/*
 **********************************************************************
 * \fn void m16c_enter() 
 * Enter a new stackframe. 
 * Store the old stackpointer in FB and decrement SP by an imm.
 * v0
 **********************************************************************
 */

void
m16c_enter()
{
	uint8_t imm = M16C_Read8(M16C_REG_PC);
	M16C_REG_PC += 1;
	M16C_REG_SP = M16C_REG_SP - 2;
	M16C_Write16(M16C_REG_FB, M16C_REG_SP);
	M16C_REG_FB = M16C_REG_SP;
	M16C_REG_SP -= imm;
	dbgprintf("instr m16c_enter(%04x)\n", ICODE16());
}

/**
 *******************************************************************
 * \fn void m16c_exitd() 
 * Exit a stack frame. Restore the stack pointer from FB and
 * Read the previous values of FB and PC from the stack.
 * v1
 *******************************************************************
 */
void
m16c_exitd()
{
	M16C_REG_SP = M16C_REG_FB;
	M16C_REG_FB = M16C_Read16(M16C_REG_SP);
	M16C_REG_SP += 2;
	M16C_REG_PC = M16C_Read16(M16C_REG_SP);
	M16C_REG_SP += 2;
	M16C_REG_PC |= (M16C_Read8(M16C_REG_SP) & 0xf) << 16;
	M16C_REG_SP += 1;
	dbgprintf("instr m16c_exitd(%04x) not implemented\n", ICODE16());
}

/**
 *******************************************************************
 * \fn void m16c_exts_b_dst(void)
 * Sign extend a byte to a word. 
 * v0
 *******************************************************************
 */
void
m16c_exts_b_dst(void)
{
	int size;
	uint32_t Dst;
	int codelen;
	GAM_GetProc *getdst;
	GAM_SetProc *setdst;
	int dst = ICODE16() & 0xf;
	size = 1;
	getdst = general_am_get(dst, size, &codelen, GAM_ALL);
	setdst = general_am_set_mulextdst(dst, size << 1, &codelen, GAM_ALL);
	getdst(&Dst, size);
	Dst = (int32_t) (int8_t) Dst;
	setdst(Dst, size << 1);
	ext_flags(Dst, size << 1);
	M16C_REG_PC += codelen;
	dbgprintf("m16c_exts_size_dst not tested\n");
}

/** 
 ******************************************************************
 * \fn void m16c_exts_w_r0() 
 * Sign extend a word in R0 to a 32 Bit value
 * in Registers R2R0.
 * Has no effect on Flags !
 * v0
 ******************************************************************
 */
void
m16c_exts_w_r0()
{
	uint32_t Dst = (int32_t) (int16_t) M16C_REG_R0;
	M16C_REG_R2 = Dst >> 16;
	dbgprintf("instr m16c_exts_w_r0(%04x)\n", ICODE16());
}

/*
 *******************************************
 * \fn void m16c_fclr_dst() 
 * Clear a bit in the flag register.
 * v0
 *******************************************
 */
void
m16c_fclr_dst()
{
	int dst = (ICODE16() >> 4) & 7;
	M16C_SET_REG_FLG(M16C_REG_FLG & ~(1 << dst));
	dbgprintf("instr m16c_fclr_dst(%04x)\n", ICODE16());
}

/**
 *********************************************************
 * \fn void m16c_fset_dst() 
 * Set a bit in the flag register.
 * v0
 *********************************************************
 */
void
m16c_fset_dst()
{
	int dst = (ICODE16() >> 4) & 7;
	M16C_SET_REG_FLG(M16C_REG_FLG | (1 << dst));
	dbgprintf("instr m16c_fset_dst(%04x)\n", ICODE16());
}

/**
 *********************************************************
 * \fn void m16c_inc_b_dst() 
 * Increment a byte at a destination.
 * v0
 *********************************************************
 */
void
m16c_inc_b_dst()
{
	int codelen;
	int dst = ICODE8() & 7;
	uint8_t value;
	value = am2b_get(dst, &codelen);
	value++;
	sgn_zero_flags(value, 1);
	am2b_set(dst, &codelen, value);
	M16C_REG_PC += codelen;
	dbgprintf("instr m16c_inc_b_dst(%04x)\n", ICODE8());
}

/**
 *****************************************************************
 * \fn void m16c_inc_w_dst() 
 * increment Register A0/A1
 * v0
 *****************************************************************
 */
void
m16c_inc_w_dst()
{
	uint16_t value;
	if (ICODE8() & (1 << 3)) {
		M16C_REG_A1++;
		value = M16C_REG_A1;

	} else {
		M16C_REG_A0++;
		value = M16C_REG_A0;
	}
	sgn_zero_flags(value, 2);
	dbgprintf("instr m16c_inc_w_dst(%04x)\n", ICODE8());
}

/*
 ****************************************************************
 * Stack layout described in rej09b0137_m16csm.pdf page 256
 ****************************************************************
 */
void
m16c_int_imm()
{
	uint32_t Src;
	uint16_t flg;
	Src = ICODE16() & 0x3f;
	flg = M16C_REG_FLG;
	if (Src < 32) {
		M16C_SET_REG_FLG(M16C_REG_FLG & ~(M16C_FLG_U | M16C_FLG_I | M16C_FLG_D));
	} else {
		M16C_SET_REG_FLG(M16C_REG_FLG & ~(M16C_FLG_I | M16C_FLG_D));
	}
	M16C_REG_SP -= 1;
	M16C_Write8(((M16C_REG_PC >> 16) & 0xf) | ((flg >> 8) & 0xf0), M16C_REG_SP);
	M16C_REG_SP -= 1;
	M16C_Write8(flg, M16C_REG_SP);
	M16C_REG_SP -= 2;
	M16C_Write16(M16C_REG_PC, M16C_REG_SP);
	M16C_REG_PC = M16C_Read24((M16C_REG_INTB + (Src << 2)) & 0xfffff) & 0xfffff;
	dbgprintf("instr m16c_int_imm(%04x) not implemented\n", ICODE16());
}

/*
 ********************************************************************
 * Stack layout described in rej09b0137_m16csm.pdf page 256
 ********************************************************************
 */
void
m16c_into()
{
	uint16_t flg;
	if (!(M16C_REG_FLG & M16C_FLG_OVERFLOW)) {
		return;
	}
	flg = M16C_REG_FLG;
	M16C_SET_REG_FLG(M16C_REG_FLG & ~(M16C_FLG_U | M16C_FLG_I | M16C_FLG_D));
	M16C_REG_SP -= 1;
	M16C_Write8(((M16C_REG_PC >> 16) & 0xf) | ((flg >> 8) & 0xf0), M16C_REG_SP);
	M16C_REG_SP -= 1;
	M16C_Write8(flg, M16C_REG_SP);
	M16C_REG_SP -= 2;
	M16C_Write16(M16C_REG_PC, M16C_REG_SP);
	M16C_REG_PC = M16C_Read24(0xfffe0) & 0xfffff;
	dbgprintf("instr m16c_int0(%04x)\n", ICODE8());
}

/**
 **************************************************************
 * \fn void m16c_jcnd1() 
 * v0
 **************************************************************
 */
void
m16c_jcnd1()
{
	uint8_t cnd = ICODE8() & 0x7;
	int8_t dsp = M16C_Read8(M16C_REG_PC);
	if (check_condition(cnd)) {
		M16C_REG_PC = (M16C_REG_PC + dsp) & 0xfffff;
	} else {
		M16C_REG_PC++;
	}
	dbgprintf("instr m16c_jcnd1(%04x) not implemented\n", ICODE8());
}

/**
 **************************************************************
 * \fn void m16c_jcnd2() 
 * v0
 **************************************************************
 */
void
m16c_jcnd2()
{
	uint8_t cnd = ICODE16() & 0xf;
	int8_t dsp = M16C_Read8(M16C_REG_PC);
	if (check_condition(cnd)) {
		M16C_REG_PC = (M16C_REG_PC + dsp) & 0xfffff;
	} else {
		M16C_REG_PC++;
	}
	dbgprintf("instr m16c_jcnd2(%04x)\n", ICODE16());
}

/**
 ****************************************************************
 * \fn void m16c_jmp_s() 
 * v0
 ****************************************************************
 */
void
m16c_jmp_s()
{
	uint8_t dsp = ICODE8() & 0x7;
	M16C_REG_PC = (M16C_REG_PC + dsp + 1) & 0xfffff;
	dbgprintf("instr m16c_jmp_s(%04x)\n", ICODE8());
}

/**
 ****************************************************************
 * \fn void m16c_jmp_b() 
 * v0
 ****************************************************************
 */
void
m16c_jmp_b()
{
	int8_t dsp = M16C_Read8(M16C_REG_PC);
	M16C_REG_PC = (M16C_REG_PC + dsp) & 0xfffff;
	dbgprintf("instr m16c_jmp_b(%04x)\n", ICODE8());
}

/**
 ********************************************************************************
 * \fn void m16c_jmp_w() 
 * v0
 ********************************************************************************
 */
void
m16c_jmp_w()
{
	int16_t dsp = M16C_Read16(M16C_REG_PC);
	M16C_REG_PC = (M16C_REG_PC + dsp) & 0xfffff;
	dbgprintf("instr m16c_jmp_w(%04x)\n", ICODE8());
}

/**
 *******************************************************************
 * \fn void m16c_jmp_a() 
 * v0
 *******************************************************************
 */
void
m16c_jmp_a()
{
	uint32_t addr = M16C_Read24(M16C_REG_PC) & 0xfffff;
	M16C_REG_PC = addr;
	dbgprintf("instr m16c_jmp_a(%04x)\n", ICODE8());
}

/* 
 ************************************************************************************
 * \fn static GAM_GetProc *jamw_am_get(int am,int *codelen,uint32_t existence_map)
 * special addressing modes for jumps 
 * word specifier for jump width 
 ************************************************************************************
 */
static GAM_GetProc *
jamw_am_get(int am, int *codelen, uint32_t existence_map)
{
	switch (am) {
	    case 0:
		    *codelen = 0;
		    return gam_get_r0;
	    case 1:
		    *codelen = 0;
		    return gam_get_r1;
	    case 2:
		    *codelen = 0;
		    return gam_get_r2;
	    case 3:
		    *codelen = 0;
		    return gam_get_r3;
	    case 4:
		    *codelen = 0;
		    return gam_get_a0;
	    case 5:
		    *codelen = 0;
		    return gam_get_a1;
	    case 6:
		    *codelen = 0;
		    return gam_get_ia0;
	    case 7:
		    *codelen = 0;
		    return gam_get_ia1;

	    case 8:
		    *codelen = 1;
		    return gam_get_dsp8ia0;

	    case 9:
		    *codelen = 1;
		    return gam_get_dsp8ia1;

	    case 10:
		    *codelen = 1;
		    return gam_get_dsp8isb;

	    case 11:
		    *codelen = 1;
		    return gam_get_dsp8ifb;

	    case 12:
		    *codelen = 2;
		    return gam_get_dsp20ia0;

	    case 13:
		    *codelen = 2;
		    return gam_get_dsp20ia1;

	    case 14:
		    *codelen = 2;
		    return gam_get_dsp16isb;

	    case 15:		/* abs 16 */
		    *codelen = 2;
		    return gam_get_abs16;
	    default:
		    *codelen = 0;
		    return gam_get_bad;
	}
}

/* 
 **************************************************************************
 * \fn static uint32_t jama_get(uint16_t am,int *arglen) 
 * special addressing modes for jumps 
 * address specifier for jump
 * v0
 **************************************************************************
 */

static uint32_t
jama_get(uint16_t am, int *arglen)
{
	uint32_t retval;
	switch (am) {
	    case 0:
		    retval = M16C_REG_R0 + ((uint32_t) M16C_REG_R2 << 16);
		    *arglen = 0;
		    break;
	    case 1:
		    retval = M16C_REG_R1 + ((uint32_t) M16C_REG_R3 << 16);
		    *arglen = 0;
		    break;
	    case 4:
		    retval = M16C_REG_A0 + ((uint32_t) M16C_REG_A1 << 16);
		    *arglen = 0;
		    break;
	    case 6:
		    /* [A0] */
		    retval = M16C_Read24(M16C_REG_A0);
		    *arglen = 0;
		    break;
	    case 7:
		    /* [A1] */
		    retval = M16C_Read24(M16C_REG_A1);
		    *arglen = 0;
		    break;
	    case 8:
		    {
			    uint8_t dsp8 = M16C_Read8(M16C_REG_PC);
			    *arglen = 1;
			    retval = M16C_Read24(M16C_REG_A0 + dsp8);
		    }
		    break;

	    case 9:
		    {
			    uint8_t dsp8 = M16C_Read8(M16C_REG_PC);
			    *arglen = 1;
			    retval = M16C_Read24(M16C_REG_A1 + dsp8);
		    }
		    break;

	    case 10:
		    {
			    uint8_t dsp8 = M16C_Read8(M16C_REG_PC);
			    *arglen = 1;
			    retval = M16C_Read24(M16C_REG_SB + dsp8);
		    }
		    break;

	    case 11:
		    {
			    int8_t dsp8 = M16C_Read8(M16C_REG_PC);
			    *arglen = 1;
			    retval = M16C_Read24(M16C_REG_FB + dsp8);
		    }
		    break;
	    case 12:
		    {
			    uint32_t dsp20 = M16C_Read24(M16C_REG_PC) & 0xfffff;
			    *arglen = 3;
			    retval = M16C_Read24((M16C_REG_A0 + dsp20) & 0xfffff);
		    }
		    break;
	    case 13:
		    {
			    uint32_t dsp20 = M16C_Read24(M16C_REG_PC) & 0xfffff;
			    *arglen = 3;
			    retval = M16C_Read24((M16C_REG_A1 + dsp20) & 0xfffff);
		    }
		    break;
	    case 14:		/* dsp16[SB] */
		    {
			    uint16_t dsp16 = M16C_Read16(M16C_REG_PC);
			    *arglen = 2;
			    retval = M16C_Read24((M16C_REG_SB + dsp16) & 0xfffff);
		    }
		    break;
	    case 15:		/* abs16 */
		    {
			    retval = M16C_Read16(M16C_REG_PC);
			    *arglen = 2;
		    }
		    break;
	    default:
		    fprintf(stderr, "Unknown addressing mode %d in line %d\n", am, __LINE__);
		    retval = 0;

	}
	return retval;
}

/**
 *****************************************************
 * Jump by src relative to start of instruction. 
 * v0
 *****************************************************
 */
void
m16c_jmpi_w_src()
{
	GAM_GetProc *getsrc;
	int codelen_src;
	uint32_t Src;
	int src = ICODE16() & 0xf;
	getsrc = jamw_am_get(src, &codelen_src, GAM_ALL);
	getsrc(&Src, 2);
	M16C_REG_PC = (M16C_REG_PC - 2 + (int16_t) Src) & 0xfffff;
}

/**
 ******************************************************************
 * \fn void m16c_jmpi_a() 
 * Jump indirect to an absolute address. 
 * v0
 ******************************************************************
 */
void
m16c_jmpi_a()
{
	int arglen;
	int am = ICODE16() & 0xf;
	uint32_t addr = jama_get(am, &arglen);
	M16C_REG_PC = addr & 0xfffff;
	dbgprintf("instr m16c_jmpi_a(%04x)\n", ICODE16());
}

/**
 ****************************************************************
 * \fn void m16c_jmps_imm8() 
 * Jump to special page.
 * Probably not available in R8C ?
 * v0
 ****************************************************************
 */
void
m16c_jmps_imm8()
{
	uint32_t Src = M16C_Read8(M16C_REG_PC);
	uint32_t addr = 0xffffe - (Src << 1);
	M16C_REG_PC = 0xf0000 | M16C_Read16(addr);
	dbgprintf("instr m16c_jmps_imm8(%04x)\n", ICODE8());
}

/**
 ************************************************************
 * \fn void m16c_jsr_w() 
 * Relative jump to a subroutine.
 * v0
 ************************************************************
 */
void
m16c_jsr_w()
{
	int16_t dsp16 = M16C_Read16(M16C_REG_PC);
	M16C_REG_PC += 2;
	M16C_REG_SP -= 1;
	M16C_Write8((M16C_REG_PC >> 16) & 0xf, M16C_REG_SP);
	M16C_REG_SP -= 2;
	M16C_Write16(M16C_REG_PC, M16C_REG_SP);
	M16C_REG_PC = (M16C_REG_PC + dsp16 - 2) & 0xfffff;
	dbgprintf("instr m16c_jsr_w(%04x)\n", ICODE8());
}

/**
 ********************************************************************
 * \fn void m16c_jsr_a() 
 * Jump to a subroutine with absolute address.
 * v0
 ********************************************************************
 */
void
m16c_jsr_a()
{
	uint32_t abs20 = M16C_Read24(M16C_REG_PC) & 0xfffff;
	M16C_REG_PC += 3;
	M16C_REG_SP -= 1;
	M16C_Write8((M16C_REG_PC >> 16) & 0xf, M16C_REG_SP);
	M16C_REG_SP -= 2;
	M16C_Write16(M16C_REG_PC, M16C_REG_SP);
	M16C_REG_PC = abs20;
	dbgprintf("instr m16c_jsr_a(%04x)\n", ICODE8());
}

/* 
 *********************************************************************
 * \fn void m16c_jsri_w() 
 * Jump to a subroutine indirect relative.
 * v0
 *********************************************************************
 */
void
m16c_jsri_w()
{
	GAM_GetProc *getsrc;
	int codelen_src;
	int src = ICODE16() & 0xf;
	uint32_t Src;
	getsrc = jamw_am_get(src, &codelen_src, GAM_ALL);
	getsrc(&Src, 2);
	M16C_REG_PC += codelen_src;
	M16C_REG_SP -= 1;
	M16C_Write8((M16C_REG_PC >> 16) & 0xf, M16C_REG_SP);
	M16C_REG_SP -= 2;
	M16C_Write16(M16C_REG_PC & 0xffff, M16C_REG_SP);
	M16C_REG_PC = (M16C_REG_PC + (int16_t) Src - codelen_src - 2) & 0xfffff;
	dbgprintf("m16c_jsri_w_src not tested\n");

}

/** 
 ********************************************************************
 * \fn void m16c_jsri_a() 
 * Jump to a subroutine indirect absolute.
 * v0
 ********************************************************************
 */
void
m16c_jsri_a()
{
	int codelen;
	int am = ICODE16() & 0xf;
	uint32_t abs20 = jama_get(am, &codelen) & 0xfffff;
	M16C_REG_PC += codelen;
	M16C_REG_SP -= 1;
	M16C_Write8((M16C_REG_PC >> 16) & 0xf, M16C_REG_SP);
	M16C_REG_SP -= 2;
	M16C_Write16(M16C_REG_PC, M16C_REG_SP);
	M16C_REG_PC = abs20;
	dbgprintf("instr m16c_jsri_a(%04x)\n", ICODE16());
}

/* 
 *******************************************************************
 * \fn void m16c_jsrs_imm8() 
 * Jump to a subroutine special page.
 * Probably not available on the R8C ?
 * v0
 *******************************************************************
 */
void
m16c_jsrs_imm8()
{
	uint32_t Src = M16C_Read8(M16C_REG_PC);
	uint32_t addr = 0xffffe - (Src << 1);
	M16C_REG_PC += 1;
	M16C_REG_SP -= 1;
	M16C_Write8((M16C_REG_PC >> 16) & 0xf, M16C_REG_SP);
	M16C_REG_SP -= 2;
	M16C_Write16(M16C_REG_PC, M16C_REG_SP);
	M16C_REG_PC = 0xf0000 | M16C_Read16(addr);
	dbgprintf("instr m16c_jsrs_imm8(%04x) not implemented\n", ICODE8());
}

/**
 ***************************************************************************
 * \fn void set_creg(int creg,uint16_t value) 
 * Set a control register.
 * v0
 ***************************************************************************
 */
void
set_creg(int creg, uint16_t value)
{
	switch (creg) {
	    case 0:
		    fprintf(stderr, "M16C: Unknown control register %d\n", creg);
		    return;

	    case 1:
		    M16C_REG_INTB = (M16C_REG_INTB & 0xf0000) | value;
		    return;

	    case 2:
		    M16C_REG_INTB = (M16C_REG_INTB & 0xffff) | (((uint32_t) value & 0xf) << 16);
		    return;

	    case 3:
		    M16C_SET_REG_FLG(value);
		    return;
	    case 4:
		    M16C_SET_REG_ISP(value);
		    return;
	    case 5:
		    M16C_REG_SP = value;
		    return;

	    case 6:
		    M16C_REG_SB = value;
		    return;
	    case 7:
		    M16C_REG_FB = value;
		    return;

	    default:
		    return;
	}
}

/**
 ********************************************************************
 * \fn static uint16_t get_creg(int creg); 
 * Get a control register.
 * v0
 ********************************************************************
 */
static uint16_t
get_creg(int creg)
{
	switch (creg) {
	    case 0:
		    fprintf(stderr, "M16C: Unknown control register %d\n", creg);
		    return 0;

	    case 1:
		    return M16C_REG_INTB & 0xffff;

	    case 2:
		    return M16C_REG_INTB >> 16;

	    case 3:
		    return M16C_REG_FLG;

	    case 4:
		    return M16C_REG_ISP;

	    case 5:
		    return M16C_REG_SP;

	    case 6:
		    return M16C_REG_SB;

	    case 7:
		    return M16C_REG_FB;

	    default:
		    fprintf(stderr, "unknown creg %d\n", creg);
		    return 0;
	}
}

/**
 ***************************************************************************
 * \fn void m16c_ldc_imm16_dst() 
 * Load a control register with an immediate 16 bit value.
 * v0
 ***************************************************************************
 */
void
m16c_ldc_imm16_dst()
{
	uint16_t imm16 = M16C_Read16(M16C_REG_PC);
	M16C_REG_PC += 2;
	set_creg((ICODE16() >> 4) & 0x7, imm16);
	dbgprintf("instr m16c_ldc_imm16_dst(%04x) not tested\n", ICODE16());
}

/**
 **************************************************************
 * \fn void m16c_ldc_srcdst(void) 
 * Load a control register from a source.
 * v0
 **************************************************************
 */
void
m16c_ldc_srcdst()
{
	uint32_t Src;
	int codelen_src;
	int size = 2;
	int src = ICODE16() & 0xf;
	int dst = (ICODE16() >> 4) & 0x7;
	GAM_GetProc *getsrc;
	getsrc = general_am_get(src, size, &codelen_src, GAM_ALL);
	getsrc(&Src, size);
	M16C_REG_PC += codelen_src;
	set_creg(dst, Src);
	dbgprintf("instr m16c_ldc_srcdst(%04x) not implemented\n", ICODE16());
}

/*
 ****************************************************************************
 * See assembler manual rej05b0085 for documentation of ldctx
 ****************************************************************************
 */
void
m16c_ldctx()
{
	uint32_t table_base;
	uint16_t abs16;
	uint32_t addr20;
	uint8_t regset;
	uint8_t spdiff = 0;
	uint16_t regp;
	abs16 = M16C_Read16(M16C_REG_PC);
	M16C_REG_PC += 2;
	table_base = M16C_Read24(M16C_REG_PC);
	M16C_REG_PC += 3;
	addr20 = (table_base + ((uint32_t) abs16 << 1)) & 0xfffff;
	regset = M16C_Read8(addr20);
	addr20 = (addr20 + 1) & 0xfffff;
	regp = M16C_REG_SP;
	if (regset & 1) {
		M16C_REG_R0 = M16C_Read16(regp);
		regp += 2;
	}
	if (regset & 2) {
		M16C_REG_R1 = M16C_Read16(regp);
		regp += 2;
	}
	if (regset & 4) {
		M16C_REG_R2 = M16C_Read16(regp);
		regp += 2;
	}
	if (regset & 8) {
		M16C_REG_R3 = M16C_Read16(regp);
		regp += 2;
	}
	if (regset & 0x10) {
		M16C_REG_A0 = M16C_Read16(regp);
		regp += 2;
	}
	if (regset & 0x20) {
		M16C_REG_A1 = M16C_Read16(regp);
		regp += 2;
	}
	if (regset & 0x40) {
		M16C_REG_SB = M16C_Read16(regp);
		regp += 2;
	}
	if (regset & 0x80) {
		M16C_REG_FB = M16C_Read16(regp);
		regp += 2;
	}
	spdiff = M16C_Read8(addr20);
	M16C_REG_SP += spdiff;
	if (regp != M16C_REG_SP) {
		fprintf(stderr, "LDCTX unexpected spdiff\n");
	}
	dbgprintf("instr m16c_ldctx(%04x) not implemented\n", ICODE16());
}

/**
 *******************************************************************
 * \fn void m16c_lde_size_abs20_dst(void) 
 * Load from extended data array.
 * v0
 *******************************************************************
 */
void
m16c_lde_size_abs20_dst(void)
{
	int size;
	int codelen_dst;
	GAM_SetProc *setdst;
	uint32_t abs20;
	uint32_t Dst;
	int dst = ICODE16() & 0xf;
	if (ICODE16() & 0x100) {
		size = 2;
	} else {
		size = 1;
	}
	setdst = general_am_set(dst, size, &codelen_dst, GAM_ALL);
	abs20 = M16C_Read24(M16C_REG_PC + codelen_dst) & 0xfffff;
	if (size == 2) {
		Dst = M16C_Read16(abs20);
	} else {
		Dst = M16C_Read8(abs20);
	}
	setdst(Dst, size);
	M16C_REG_PC += codelen_dst + 3;
	dbgprintf("instr m16c_lde_size_abs20_dst(%04x) not tested\n", ICODE16());
}

/** 
 *******************************************************************
 * \fn void m16c_lde_size_dsp_dst() 
 * Load from extended area with a 20 bit displacement to [A0]
 * v0
 ******************************************************************
 */
void
m16c_lde_size_dsp_dst()
{
	int size;
	int codelen_dst;
	uint32_t dsp20;
	uint32_t addr;
	uint32_t Dst;
	int dst = ICODE16() & 0xf;
	GAM_SetProc *setdst;
	if (ICODE16() & 0x100) {
		size = 2;
	} else {
		size = 1;
	}
	setdst = general_am_set(dst, size, &codelen_dst, GAM_ALL);
	dsp20 = M16C_Read24(M16C_REG_PC + codelen_dst);
	addr = (M16C_REG_A0 + dsp20) & 0xfffff;
	if (size == 2) {
		Dst = M16C_Read16(addr);
	} else {
		Dst = M16C_Read8(addr);
	}
	setdst(Dst, size);
	M16C_REG_PC += codelen_dst + 3;
	dbgprintf("instr m16c_lde_size_dsp_dst(%04x)\n", ICODE16());
}

/**
 **************************************************************************
 * \fn void m16c_lde_size_a1a0_dst() 
 * Load from extended area at address A1A0. 
 * v0
 **************************************************************************
 */
void
m16c_lde_size_a1a0_dst()
{
	int codelen_dst;
	int size;
	uint32_t addr = M16C_REG_A0 + (((uint32_t) M16C_REG_A1 & 0xf) << 16);
	uint32_t Dst;
	int dst = ICODE16() & 0xf;
	GAM_SetProc *setdst;
	if (ICODE16() & 0x100) {
		size = 2;
	} else {
		size = 1;
	}
	setdst = general_am_set(dst, size, &codelen_dst, GAM_ALL);
	if (size == 2) {
		Dst = M16C_Read16(addr);
	} else {
		Dst = M16C_Read8(addr);
	}
	setdst(Dst, size);
	M16C_REG_PC += codelen_dst;
	dbgprintf("instr m16c_lde_size_a1a0_dst(%04x)\n", ICODE16());
}

/**
 **************************************************************
 * \fn void m16c_ldipl_imm() 
 * Load Interrupt privilege level.
 * v0
 **************************************************************
 */
void
m16c_ldipl_imm()
{
	int ipl = ICODE16() & 7;
	M16C_SET_REG_FLG((M16C_REG_FLG & 0xff) | (ipl << 12));
}

/**
 ******************************************************************
 * \fn void m16c_mov_size_g_immdst() 
 * v0
 ******************************************************************
 */
void
m16c_mov_size_g_immdst()
{
	int dst;
	uint32_t Src, Result;
	int immsize, opsize;
	int codelen_dst;
	GAM_SetProc *setdst;
	dst = ICODE16() & 0xf;
	if (ICODE16() & 0x100) {
		immsize = opsize = 2;
	} else {
		immsize = opsize = 1;
		ModOpsize(dst, &opsize);
	}
	setdst = general_am_set(dst, opsize, &codelen_dst, GAM_ALL);
	if (immsize == 2) {
		Src = M16C_Read16(M16C_REG_PC + codelen_dst);
	} else if (immsize == 1) {
		Src = M16C_Read8(M16C_REG_PC + codelen_dst);
	}
	Result = Src;
	setdst(Result, opsize);
	mov_flags(Result, opsize);
	M16C_REG_PC += codelen_dst + immsize;
	dbgprintf("instr m16c_mov_size_g_immdst(%04x)\n", ICODE16());
}

/**
 ********************************************************************
 * \fn void m16c_mov_size_q_immdst() 
 * Mov a 4 Bit signed immediate to a destination.
 * v0
 ********************************************************************
 */
void
m16c_mov_size_q_immdst()
{
	GAM_SetProc *setdst;
	int32_t imm4 = ((int32_t) ((ICODE16() & 0xf0) << 24)) >> 28;
	int codelen;
	int opsize;
	int dst = ICODE16() & 0xf;
	if (ICODE16() & 0x100) {
		opsize = 2;
	} else {
		opsize = 1;
		ModOpsize(dst, &opsize);
		if (opsize == 2) {
			imm4 = imm4 & 0xff;
		}
	}
	setdst = general_am_set(dst, opsize, &codelen, GAM_ALL);
	setdst(imm4, opsize);
	mov_flags(imm4, opsize);
	M16C_REG_PC += codelen;
	dbgprintf("instr m16c_mov_size_q_immdst(%04x)\n", ICODE16());
}

/**
 *******************************************************************
 * \fn void m16c_mov_b_s_imm8_dst() 
 * mov an imm8 to a destination.
 * v0
 *******************************************************************
 */
void
m16c_mov_b_s_imm8_dst()
{
	int codelen;
	uint32_t imm8 = M16C_Read8(M16C_REG_PC);
	int dst = ICODE8() & 7;
	M16C_REG_PC += 1;
	am2b_set(dst, &codelen, imm8);
	mov_flags(imm8, 1);
	M16C_REG_PC += codelen;
	dbgprintf("instr m16c_mov_b_s_imm8_dst(%04x)\n", ICODE8());
}

/**
 **********************************************************
 * \fn void m16c_mov_size_s_immdst() 
 * Move a 8/16 bit immediate to Register A0/A1.
 * v0
 **********************************************************
 */
void
m16c_mov_size_s_immdst()
{
	uint16_t imm;
	int size, opsize;
	/* UNUSUAL polarity of size bit ! */
	if (ICODE8() & (1 << 6)) {
		size = 1;
		opsize = 2;
	} else {
		opsize = size = 2;
	}
	if (size == 2) {
		imm = M16C_Read16(M16C_REG_PC);
		M16C_REG_PC += 2;
	} else {
		imm = M16C_Read8(M16C_REG_PC);
		M16C_REG_PC += 1;
	}
	if (ICODE8() & 8) {
		M16C_REG_A1 = imm;
	} else {
		M16C_REG_A0 = imm;
	}
	mov_flags(imm, opsize);
	dbgprintf("instr m16c_mov_size_s_immdst(%04x)\n", ICODE8());
}

/**
 *************************************************************************
 * \fn void m16c_mov_b_z_0_dst() 
 * Move a zero to a destination.
 * v0
 *************************************************************************
 */
void
m16c_mov_b_z_0_dst()
{
	int codelen_dst;
	int am = ICODE8() & 7;
	am2b_set(am, &codelen_dst, 0);
	M16C_REG_PC += codelen_dst;
	M16C_REG_FLG |= M16C_FLG_ZERO;
	M16C_REG_FLG &= ~M16C_FLG_SIGN;
	dbgprintf("instr m16c_mov_b_z_0_dst(%04x) not implemented\n", ICODE8());
}

/**
 ***********************************************************************
 * \fn void m16c_mov_size_g_srcdst() 
 * Move a src to a destination. 
 * v0
 ***********************************************************************
 */
void
m16c_mov_size_g_srcdst()
{
	int dst, src;
	uint32_t Src, Dst;
	int opsize, srcsize;
	int codelen_src;
	int codelen_dst;
	GAM_GetProc *getsrc;
	GAM_SetProc *setdst;
	dst = ICODE16() & 0xf;
	src = (ICODE16() >> 4) & 0xf;
	if (ICODE16() & 0x100) {
		opsize = srcsize = 2;
	} else {
		opsize = srcsize = 1;
		ModOpsize(dst, &opsize);
	}
	getsrc = general_am_get(src, srcsize, &codelen_src, GAM_ALL);
	setdst = general_am_set(dst, opsize, &codelen_dst, GAM_ALL);
	getsrc(&Src, srcsize);
	M16C_REG_PC += codelen_src;
	Dst = Src;
	setdst(Dst, opsize);
	mov_flags(Dst, opsize);
	M16C_REG_PC += codelen_dst;
	//fprintf(stderr,"Moved %02x to dst %d\n",Src,dst);
	dbgprintf("instr m16c_mov_size_g_srcdst(%04x)\n", ICODE16());
}

/**
 ************************************************************************
 * \fn void m16c_mov_b_s_srcdst() 
 * Move a source to A0/A1. Not totally clear when
 * R0L and when R0H is addressed but suspected from other
 * instructions.
 * v0
 ************************************************************************
 */
void
m16c_mov_b_s_srcdst()
{
	int codelen_src;
	uint32_t Src;
	int src = ICODE8() & 7;
	Src = am3b_get(src, &codelen_src);
	if (ICODE8() & 4) {
		M16C_REG_A1 = Src;
	} else {
		M16C_REG_A0 = Src;
	}
	M16C_REG_PC += codelen_src;
	mov_flags(Src, 2);
	dbgprintf("instr m16c_mov_b_s_srcdst(%04x)\n", ICODE8());
}

/**
 ************************************************************
 * \fn void m16c_mov_b_r0dst() 
 * Move a byte from R0H/R0L to a destination
 * v0
 ************************************************************
 */

void
m16c_mov_b_r0dst()
{
	int codelen_dst;
	uint32_t Src;
	int dst = ICODE8() & 7;
	if (ICODE8() & 4) {
		Src = M16C_REG_R0H;
	} else {
		Src = M16C_REG_R0L;
	}
	am3b_set(dst, &codelen_dst, Src);
	M16C_REG_PC += codelen_dst;
	mov_flags(Src, 1);
	dbgprintf("instr m16c_mov_b_r0dst(%04x)\n", ICODE8());
}

/**
 ******************************************************
 * \fn void m16c_mov_b_s_r0() 
 * Move a byte from a source to R0H/R0L
 * v0
 ******************************************************
 */
void
m16c_mov_b_s_r0()
{
	int codelen_src;
	uint32_t Src;
	int src = ICODE8() & 7;
	Src = am3b_get(src, &codelen_src);
//      fprintf(stderr,"src %d, value %02x\n",src,Src);
	if (ICODE8() & 4) {
		M16C_REG_R0H = Src;
	} else {
		M16C_REG_R0L = Src;
	}
	M16C_REG_PC += codelen_src;
	mov_flags(Src, 1);
	dbgprintf("instr m16c_mov_b_s_r0(%04x)\n", ICODE8());
}

/**
 ****************************************************************
 * \fn void m16c_mov_size_g_dspdst() 
 * Move data from SP + signed displacement to a destination.
 * v0
 ****************************************************************
 */
void
m16c_mov_size_g_dspdst()
{
	int codelen_dst;
	int size, opsize;
	int8_t dsp8;
	int dst = ICODE16() & 0xf;
	uint32_t Dst;
	GAM_SetProc *setdst;
	if (ICODE16() & 0x100) {
		opsize = size = 2;
	} else {
		opsize = size = 1;
		ModOpsize(dst, &opsize);
	}
	setdst = general_am_set(dst, size, &codelen_dst, GAM_ALL);
	dsp8 = M16C_Read8(M16C_REG_PC + codelen_dst);
	if (size == 2) {
		Dst = M16C_Read16((M16C_REG_SP + dsp8) & 0xffff);
	} else {
		Dst = M16C_Read8((M16C_REG_SP + dsp8) & 0xffff);
	}
	setdst(Dst, opsize);
	mov_flags(Dst, opsize);
	M16C_REG_PC += codelen_dst + 1;
	dbgprintf("instr m16c_mov_size_g_dspdst(%04x)\n", ICODE16());
}

/**
 ****************************************************************
 * \fn void m16c_mov_size_g_srcdsp() 
 * Move a src to the address (SP + signed displacement).
 * v0
 ****************************************************************
 */
void
m16c_mov_size_g_srcdsp()
{
	int size, opsize;
	int codelen_src;
	int8_t dsp8;
	uint32_t Src;
	int src = ICODE16() & 0xf;
	GAM_GetProc *getsrc;
	if (ICODE16() & 0x100) {
		opsize = size = 2;
	} else {
		opsize = size = 1;
	}
	getsrc = general_am_get(src, size, &codelen_src, GAM_ALL);
	getsrc(&Src, size);
	dsp8 = M16C_Read8(M16C_REG_PC + codelen_src);
	if (size == 2) {
		M16C_Write16(Src, (M16C_REG_SP + dsp8) & 0xffff);
	} else {
		M16C_Write8(Src, (M16C_REG_SP + dsp8) & 0xffff);
	}
	mov_flags(Src, opsize);
	M16C_REG_PC += codelen_src + 1;
	dbgprintf("instr m16c_mov_size_g_srcdsp(%04x)\n", ICODE16());
}

/**
 *******************************************************************
 * Move an effective address of a src to a destination
 *******************************************************************
 */
void
m16c_mova_srcdst()
{
	int codelen_src;
	int codelen_dst;
	int src = ICODE16() & 0xf;
	int dst = (ICODE16() >> 4) & 0xf;
	uint32_t eva;
	int size = 2;
	GAM_SetProc *setdst;
	if (dst > 5) {
		fprintf(stderr, "MOVA: bad addressing mode\n");
	}
	setdst = general_am_set(dst, size, &codelen_dst, GAM_ALL);
	eva = am1_get_eva(src, &codelen_src);
	//fprintf(stderr,"Got eva %04x\n",eva);
	M16C_REG_PC += codelen_src;
	setdst(eva, size);
	dbgprintf("instr m16c_mova_srcdst(%04x)\n", ICODE16());
}

/*
 ******************************************************************
 * \fn void m16c_movdir_r0dst() 
 * Move some nibbles from R0L to a destination.
 ******************************************************************
 */
void
m16c_movdir_r0dst()
{
	int dir = (ICODE16() >> 4) & 3;
	GAM_GetProc *getdst;
	GAM_SetProc *setdst;
	uint32_t Src, Dst;
	int size = 1;
	int codelen_dst;
	int dst = ICODE16() & 0xf;
	getdst = general_am_get(dst, size, &codelen_dst, GAM_ALL);
	setdst = general_am_set(dst, size, &codelen_dst, GAM_ALL);
	getdst(&Dst, size);
	Src = M16C_REG_R0L;
	switch (dir) {
	    case 0:		/* LL */
		    Dst = (Dst & ~0xf) | (Src & 0xf);
		    break;
	    case 1:		/* HL */
		    Dst = (Dst & ~0x0f) | ((Src & 0xf0) >> 4);
		    break;
	    case 2:		/* LH */
		    Dst = (Dst & ~0xf0) | ((Src & 0xf) << 4);
		    break;
	    case 3:		/* HH */
		    Dst = (Dst & ~0xf0) | (Src & 0xf0);
		    break;
	    default:
		    // unreachable;
		    break;
	}
	setdst(Dst, size);
	M16C_REG_PC += codelen_dst;
	dbgprintf("%s\n", __FUNCTION__);

}

/**
 ****************************************************************
 * void m16c_movdir_srcr0l() 
 * Move some nibble from a source to R0L.
 * v0
 ****************************************************************
 */
void
m16c_movdir_srcr0l()
{
	int dir = (ICODE16() >> 4) & 3;
	GAM_GetProc *getsrc;
	uint32_t Src, Dst;
	int size = 1;
	int codelen_src;
	int src = ICODE16() & 0xf;
	getsrc = general_am_get(src, size, &codelen_src, GAM_ALL);
	getsrc(&Src, size);
	Dst = M16C_REG_R0L;
	switch (dir) {
	    case 0:		/* LL */
		    Dst = (Dst & ~0xf) | (Src & 0xf);
		    break;
	    case 1:		// HL
		    Dst = (Dst & ~0x0f) | ((Src & 0xf0) >> 4);
		    break;
	    case 2:		// LH
		    Dst = (Dst & ~0xf0) | ((Src & 0xf) << 4);
		    break;
	    case 3:		// HH
		    Dst = (Dst & ~0xf0) | (Src & 0xf0);
		    break;
	    default:
		    // unreachable;
		    break;
	}
	M16C_REG_R0L = Dst;
	M16C_REG_PC += codelen_src;
	dbgprintf("%s\n", __FUNCTION__);
}

/**
 *******************************************************************
 * \fn void m16c_mul_size_immdst() 
 * Multiply a destination with an immediate
 * v0
 *******************************************************************
 */
void
m16c_mul_size_immdst()
{
	int dst;
	uint32_t Src, Dst, Result;
	int size, resultsize;
	int codelen;
	GAM_GetProc *getdst;
	GAM_SetProc *setdst;
	dst = ICODE16() & 0xf;
	if (ICODE16() & 0x100) {
		size = 2;
		resultsize = 4;
	} else {
		size = 1;
		resultsize = 2;
	}
	getdst = general_am_get(dst, size, &codelen, GAM_ALL);
	setdst = general_am_set_mulextdst(dst, resultsize, &codelen, GAM_ALL);
	getdst(&Dst, size);
	if (size == 2) {
		Dst = (int32_t) (int16_t) Dst;
		Src = (int32_t) (int16_t) M16C_Read16(M16C_REG_PC + codelen);
		Result = (int32_t) Dst *(int32_t) Src;
		setdst(Result, 4);
	} else if (size == 1) {
		Dst = (int32_t) (int8_t) Dst;
		Src = (int32_t) (int8_t) M16C_Read8(M16C_REG_PC + codelen);
		Result = (int32_t) Dst *(int32_t) Src;
		setdst(Result, 2);
	}
	M16C_REG_PC += codelen + size;
	dbgprintf("instr m16c_mul_size_immdst(%04x)\n", ICODE16());
}

/**
 ********************************************************
 * \fn void m16c_mul_size_srcdst() 
 * Multiply a source with a destination.
 * v0
 ********************************************************
 */
void
m16c_mul_size_srcdst()
{
	int dst, src;
	uint32_t Src, Dst, Result;
	int size, resultsize;
	int codelen_src;
	int codelen_dst;
	GAM_GetProc *getdst;
	GAM_GetProc *getsrc;
	GAM_SetProc *setdst;
	dst = ICODE16() & 0xf;
	src = (ICODE16() >> 4) & 0xf;
	if (ICODE16() & 0x100) {
		size = 2;
		resultsize = 4;
	} else {
		size = 1;
		resultsize = 2;
	}
	getdst = general_am_get(dst, size, &codelen_dst, GAM_ALL);
	getsrc = general_am_get(src, size, &codelen_src, GAM_ALL);
	setdst = general_am_set_mulextdst(dst, resultsize, &codelen_dst, GAM_ALL);
	getsrc(&Src, size);
	M16C_REG_PC += codelen_src;
	getdst(&Dst, size);
	if (size == 2) {
		Src = (int32_t) (int16_t) Src;
		Dst = (int32_t) (int16_t) Dst;
		Result = (int32_t) Dst *(int32_t) Src;
		setdst(Result, 4);
	} else {
		Src = (int32_t) (int8_t) Src;
		Dst = (int32_t) (int8_t) Dst;
		Result = (int32_t) Dst *(int32_t) Src;
		setdst(Result, 2);
		//      fprintf(stderr,"Mul %d: %d  %d: %d result %d\n",src,Src,dst,Dst,Result);
	}
	M16C_REG_PC += codelen_dst;
	dbgprintf("instr m16c_mul_size_srcdst(%04x) not implemented\n", ICODE16());
}

/**
 *****************************************************************************
 * \fn void m16c_mulu_size_immdst() 
 * Unsigned multiply of a destination with an immediate.
 * v0
 *****************************************************************************
 */
void
m16c_mulu_size_immdst()
{
	int dst;
	uint32_t Src, Dst, Result;
	int size, resultsize;
	int codelen;
	GAM_GetProc *getdst;
	GAM_SetProc *setdst;
	dst = ICODE16() & 0xf;
	if (ICODE16() & 0x100) {
		size = 2;
		resultsize = 4;
	} else {
		size = 1;
		resultsize = 2;
	}
	getdst = general_am_get(dst, size, &codelen, GAM_ALL);
	setdst = general_am_set_mulextdst(dst, resultsize, &codelen, GAM_ALL);
	getdst(&Dst, size);
	if (size == 2) {
		Src = M16C_Read16(M16C_REG_PC + codelen);
		Result = Dst * Src;
		setdst(Result, 4);
	} else if (size == 1) {
		Src = M16C_Read8(M16C_REG_PC + codelen);
		Result = Dst * Src;
		setdst(Result, 2);
	}
	M16C_REG_PC += codelen + size;
	dbgprintf("instr m16c_mulu_size_immdst(%04x) not tested\n", ICODE16());
}

/**
 ****************************************************************************
 * \fn void m16c_mulu_size_srcdst() 
 * Unsigned multiply a destination with a source. 
 * v0
 ****************************************************************************
 */
void
m16c_mulu_size_srcdst()
{
	int dst, src;
	uint32_t Src, Dst, Result;
	int size, resultsize;
	int codelen_src;
	int codelen_dst;
	GAM_GetProc *getdst;
	GAM_GetProc *getsrc;
	GAM_SetProc *setdst;
	dst = ICODE16() & 0xf;
	src = (ICODE16() >> 4) & 0xf;
	if (ICODE16() & 0x100) {
		size = 2;
		resultsize = 4;
	} else {
		size = 1;
		resultsize = 2;
	}
	getdst = general_am_get(dst, size, &codelen_dst, GAM_ALL);
	getsrc = general_am_get(src, size, &codelen_src, GAM_ALL);
	setdst = general_am_set_mulextdst(dst, resultsize, &codelen_dst, GAM_ALL);
	getsrc(&Src, size);
	M16C_REG_PC += codelen_src;
	getdst(&Dst, size);
	Result = Dst * Src;
	setdst(Result, resultsize);
	M16C_REG_PC += codelen_dst;
	dbgprintf("instr m16c_mulu_size_srcdst(%04x) not tested\n", ICODE16());
}

/**
 ********************************************************************
 * \fn void m16c_neg_size_dst() 
 * Negate a destination.
 * v0
 ********************************************************************
 */
void
m16c_neg_size_dst()
{

	int dst;
	int size;
	int codelen_dst;
	uint32_t Dst, Result;
	GAM_GetProc *getdst;
	GAM_SetProc *setdst;
	if (ICODE16() & 0x100) {
		size = 2;
	} else {
		size = 1;
	}
	dst = ICODE16() & 0xf;
	getdst = general_am_get(dst, size, &codelen_dst, GAM_ALL);
	setdst = general_am_set(dst, size, &codelen_dst, GAM_ALL);
	getdst(&Dst, size);
	Result = 0 - Dst;
	setdst(Result, size);
	sub_flags(0, Dst, Result, size);
	M16C_REG_PC += codelen_dst;
	dbgprintf("instr m16c_neg_size_dst(%04x)\n", ICODE16());
}

/**
 *******************************************************
 * \fn void m16c_nop() 
 * Do nothing.
 * v0
 *******************************************************
 */
void
m16c_nop()
{
	dbgprintf("instr m16c_nop(%04x)\n", ICODE8());
}

/**
 ********************************************************
 * \fn void m16c_not_size_g_dst() 
 * Invert all bits in a Destination.
 * v0 
 ********************************************************
 */
void
m16c_not_size_g_dst()
{
	int dst;
	int size;
	int codelen_dst;
	uint32_t Dst, Result;
	GAM_GetProc *getdst;
	GAM_SetProc *setdst;
	if (ICODE16() & 0x100) {
		size = 2;
	} else {
		size = 1;
	}
	dst = ICODE16() & 0xf;
	getdst = general_am_get(dst, size, &codelen_dst, GAM_ALL);
	setdst = general_am_set(dst, size, &codelen_dst, GAM_ALL);
	getdst(&Dst, size);
	Result = ~Dst;
	setdst(Result, size);
	not_flags(Result, size);
	M16C_REG_PC += codelen_dst;
	dbgprintf("instr m16c_not_size_g_dst(%04x)\n", ICODE16());
}

/**
 **********************************************************************
 * \fn void m16c_not_b_s_dst() 
 * Invert all bits in a byte destination.
 * v0
 **********************************************************************
 */
void
m16c_not_b_s_dst()
{
	int codelen_dst;
	int dst;
	uint32_t Dst;
	uint32_t Result;
	int size = 1;
	dst = ICODE8() & 0x7;
	Dst = am2b_get(dst, &codelen_dst);
	Result = ~Dst;
	am2b_set(dst, &codelen_dst, Result);
	M16C_REG_PC += codelen_dst;
	not_flags(Result, size);
	dbgprintf("instr m16c_not_b_s_dst(%04x) not implemented\n", ICODE8());
}

/*
 **********************************************************************
 * \fn void m16c_or_size_g_immdst() 
 * Logical or of a destination an an immediate.
 * v0
 **********************************************************************
 */
void
m16c_or_size_g_immdst()
{
	int dst;
	uint32_t Src, Dst, Result;
	int opsize, srcsize;
	int codelen;
	GAM_GetProc *getdst;
	GAM_SetProc *setdst;
	dst = ICODE16() & 0xf;
	if (ICODE16() & 0x100) {
		srcsize = opsize = 2;
	} else {
		srcsize = opsize = 1;
		ModOpsize(dst, &opsize);
	}
	getdst = general_am_get(dst, opsize, &codelen, GAM_ALL);
	setdst = general_am_set(dst, opsize, &codelen, GAM_ALL);
	getdst(&Dst, opsize);
	if (srcsize == 2) {
		Src = M16C_Read16(M16C_REG_PC + codelen);
	} else if (srcsize == 1) {
		Src = M16C_Read8(M16C_REG_PC + codelen);
	}
	Result = Dst | Src;
	setdst(Result, opsize);
	or_flags(Result, opsize);
	M16C_REG_PC += codelen + srcsize;
	dbgprintf("instr m16c_or_size_g_immdst(%04x) not implemented\n", ICODE16());
}

/**
 **************************************************************************
 * \fn void m16c_or_b_s_immdst() 
 * Logical OR of an immediate and a byte destination. 
 * v0
 **************************************************************************
 */
void
m16c_or_b_s_immdst()
{
	int codelen_dst;
	uint32_t imm;
	uint32_t Dst;
	uint32_t Result;
	int dst = ICODE8() & 7;
	imm = M16C_Read8(M16C_REG_PC);
	M16C_REG_PC++;
	Dst = am2b_get(dst, &codelen_dst);
	Result = Dst | imm;
	am2b_set(dst, &codelen_dst, Result);
	M16C_REG_PC += codelen_dst;
	or_flags(Result, 1);
	dbgprintf("instr m16c_or_b_s_immdst(%04x)\n", ICODE8());
}

/**
 *******************************************************************
 * \fn void m16c_or_size_g_srcdst() 
 * Logical or of a source and a destination. 
 * v0
 *******************************************************************
 */
void
m16c_or_size_g_srcdst()
{
	int dst, src;
	uint32_t Src, Dst, Result;
	int srcsize, opsize;
	int codelen_src;
	int codelen_dst;
	GAM_GetProc *getdst;
	GAM_GetProc *getsrc;
	GAM_SetProc *setdst;
	dst = ICODE16() & 0xf;
	src = (ICODE16() >> 4) & 0xf;
	if (ICODE16() & 0x100) {
		srcsize = opsize = 2;
	} else {
		srcsize = opsize = 1;
		ModOpsize(dst, &opsize);
	}
	getdst = general_am_get(dst, opsize, &codelen_dst, GAM_ALL);
	getsrc = general_am_get(src, srcsize, &codelen_src, GAM_ALL);
	setdst = general_am_set(dst, opsize, &codelen_dst, GAM_ALL);
	getsrc(&Src, srcsize);
	M16C_REG_PC += codelen_src;
	getdst(&Dst, opsize);
	Result = Dst | Src;
	setdst(Result, opsize);
	or_flags(Result, opsize);
	M16C_REG_PC += codelen_dst;
	dbgprintf("instr m16c_or_size_g_srcdst(%04x)\n", ICODE16());
}

/**
 **********************************************************************
 * \fn void m16c_or_b_s_srcr0() 
 * OR of a byte from a Src and R0H/R0L is stored in R0H/R0L
 * v0
 **********************************************************************
 */
void
m16c_or_b_s_srcr0()
{
	int src;
	uint32_t Src, Dst, Result;
	int codelen_src;
	int size = 1;
	src = ICODE8() & 0x7;
	Src = am3b_get(src, &codelen_src);
	M16C_REG_PC += codelen_src;
	if (ICODE8() & 4) {
		Dst = M16C_REG_R0H;
		M16C_REG_R0H = Result = Src | Dst;
	} else {
		Dst = M16C_REG_R0L;
		M16C_REG_R0L = Result = Src | Dst;
	}
	or_flags(Result, size);
	dbgprintf("instr m16c_or_b_s_srcr0(%04x)\n", ICODE8());
}

/**
 ******************************************************************************* 
 * \fn void m16c_pop_size_g_dst() 
 * Pop something from the stack to a destination. 
 * v0
 ******************************************************************************* 
 */
void
m16c_pop_size_g_dst()
{
	int dst;
	int codelen_dst;
	uint32_t Dst;
	GAM_SetProc *setdst;
	dst = ICODE16() & 0xf;
	if (ICODE16() & 0x100) {
		setdst = general_am_set(dst, 2, &codelen_dst, GAM_ALL);
		Dst = M16C_Read16(M16C_REG_SP);
		setdst(Dst, 2);
		M16C_REG_SP += 2;
	} else {
		setdst = general_am_set(dst, 1, &codelen_dst, GAM_ALL);
		Dst = M16C_Read8(M16C_REG_SP);
		setdst(Dst, 1);
		M16C_REG_SP += 1;
	}
	M16C_REG_PC += codelen_dst;
	dbgprintf("instr m16c_pop_size_g_dst(%04x)\n", ICODE16());
}

/**
 ************************************************
 * \fn void m16c_pop_b_s_dst() 
 * Pop a byte from stack to R0H/R0L.
 * v0
 ************************************************
 */
void
m16c_pop_b_s_dst()
{
	uint32_t Dst;
	Dst = M16C_Read8(M16C_REG_SP);
	M16C_REG_SP += 1;
	if (ICODE8() & 8) {
		M16C_REG_R0H = Dst;
	} else {
		M16C_REG_R0L = Dst;
	}
	dbgprintf("instr m16c_pop_b_s_dst(%04x)\n", ICODE8());
}

/**
 *********************************************************
 * \fn void m16c_pop_w_s_dst() 
 * Pop a word from stack to Register A0/A1
 * v0
 *********************************************************
 */
void
m16c_pop_w_s_dst()
{
	uint32_t Dst;
	Dst = M16C_Read16(M16C_REG_SP);
	M16C_REG_SP += 2;
	if (ICODE8() & 8) {
		M16C_REG_A1 = Dst;
	} else {
		M16C_REG_A0 = Dst;
	}
	dbgprintf("instr m16c_pop_w_s_dst(%04x)\n", ICODE8());
}

/**
 *********************************************************
 * \fn void m16c_popc_dst() 
 * POP control register from stack.
 * v0
 *********************************************************
 */
void
m16c_popc_dst()
{
	uint32_t Dst;
	int dst = (ICODE16() >> 4) & 7;
	Dst = M16C_Read16(M16C_REG_SP);
	M16C_REG_SP += 2;
	set_creg(dst, Dst);
	dbgprintf("instr m16c_popc_dst(%04x)\n", ICODE16());
}

/* 
 ***********************************************************************
 * \fn void m16c_popm_dst() 
 * POPMultiple 
 * 	Lowest bit is R0, verified with real M16C: 0xed 0x01
 *	Order on stack verified with real device
 * v0
 ***********************************************************************
 */
void
m16c_popm_dst()
{
	uint8_t dst = M16C_Read8(M16C_REG_PC);
	M16C_REG_PC++;
	if (dst & 1) {
		M16C_REG_R0 = M16C_Read16(M16C_REG_SP);
		M16C_REG_SP += 2;
	}
	if (dst & 2) {
		M16C_REG_R1 = M16C_Read16(M16C_REG_SP);
		M16C_REG_SP += 2;
	}
	if (dst & 4) {
		M16C_REG_R2 = M16C_Read16(M16C_REG_SP);
		M16C_REG_SP += 2;
	}
	if (dst & 8) {
		M16C_REG_R3 = M16C_Read16(M16C_REG_SP);
		M16C_REG_SP += 2;
	}
	if (dst & 0x10) {
		M16C_REG_A0 = M16C_Read16(M16C_REG_SP);
		M16C_REG_SP += 2;
	}
	if (dst & 0x20) {
		M16C_REG_A1 = M16C_Read16(M16C_REG_SP);
		M16C_REG_SP += 2;
	}
	if (dst & 0x40) {
		M16C_REG_SB = M16C_Read16(M16C_REG_SP);
		M16C_REG_SP += 2;
	}
	if (dst & 0x80) {
		M16C_REG_FB = M16C_Read16(M16C_REG_SP);
		M16C_REG_SP += 2;
	}
	dbgprintf("instr m16c_popm_dst(%04x) not implemented\n", ICODE8());
}

/**
 ****************************************************************
 * \fn void m16c_push_size_g_imm() 
 * Push an immediate to the stack.
 * v0
 ****************************************************************
 */
void
m16c_push_size_g_imm()
{
	uint32_t Imm;
	if (ICODE16() & 0x100) {
		Imm = M16C_Read16(M16C_REG_PC);
		M16C_REG_PC += 2;
		M16C_REG_SP -= 2;
		M16C_Write16(Imm, M16C_REG_SP);
	} else {
		Imm = M16C_Read8(M16C_REG_PC);
		M16C_REG_PC += 1;
		M16C_REG_SP -= 1;
		M16C_Write8(Imm, M16C_REG_SP);
	}
	dbgprintf("instr m16c_push_size_g_imm(%04x)\n", ICODE16());
}

/**
 ***************************************************************
 * \fn void m16c_push_size_g_src() 
 * Push a source onto the stack.
 * v0
 ***************************************************************
 */
void
m16c_push_size_g_src()
{
	int src;
	int codelen_src;
	uint32_t Src;
	GAM_GetProc *getsrc;
	src = ICODE16() & 0xf;
	if (ICODE16() & 0x100) {
		M16C_REG_SP -= 2;
		getsrc = general_am_get(src, 2, &codelen_src, GAM_ALL);
		getsrc(&Src, 2);
		M16C_Write16(Src, M16C_REG_SP);
	} else {
		M16C_REG_SP -= 1;
		getsrc = general_am_get(src, 1, &codelen_src, GAM_ALL);
		getsrc(&Src, 1);
		M16C_Write8(Src, M16C_REG_SP);
	}
	M16C_REG_PC += codelen_src;
	//fprintf(stderr,"Pushed %04x\n",Src);
	dbgprintf("instr m16c_push_size_g_src(%04x)\n", ICODE16());
}

/**
 *****************************************************************
 * \fn void m16c_pushb_s_src() 
 * Push R0H/R0L onto the stack.
 * v0
 *****************************************************************
 */
void
m16c_pushb_s_src()
{
	uint32_t Src;
	if (ICODE8() & 8) {
		Src = M16C_REG_R0H;
	} else {
		Src = M16C_REG_R0L;
	}
	M16C_REG_SP -= 1;
	M16C_Write8(Src, M16C_REG_SP);
	dbgprintf("instr m16c_pushb_s_src(%04x)\n", ICODE8());
}

/**
 ******************************************************************
 * \fn void m16c_push_w_src() 
 * Push A0/A1 onto the stack. 
 ******************************************************************
 */
void
m16c_push_w_src()
{
	uint16_t Src;
	if (ICODE8() & 8) {
		Src = M16C_REG_A1;
	} else {
		Src = M16C_REG_A0;
	}
	M16C_REG_SP -= 2;
	M16C_Write16(Src, M16C_REG_SP);
	dbgprintf("instr m16c_push_w_src(%04x)\n", ICODE8());
}

/**
 ******************************************************************
 * \fn void m16c_pusha_src() 
 * Push an effective address onto the stack.
 * v0
 ******************************************************************
 */
void
m16c_pusha_src()
{
	uint16_t eva;
	int codelen_src;
	int src = ICODE16() & 0xf;
	eva = am1_get_eva(src, &codelen_src);
	M16C_REG_PC += codelen_src;
	M16C_REG_SP -= 2;
	M16C_Write16(eva, M16C_REG_SP);
	dbgprintf("instr m16c_pusha_src(%04x)\n", ICODE16());
}

/**
 ******************************************************************
 * \fn void m16c_pushc_src() 
 * Push a control register onto the stack.
 ******************************************************************
 */
void
m16c_pushc_src()
{
	uint32_t Src;
	int src = (ICODE16() >> 4) & 7;
	Src = get_creg(src);
	M16C_REG_SP -= 2;
	M16C_Write16(Src, M16C_REG_SP);
	dbgprintf("instr m16c_pushc_src(%04x)\n", ICODE16());
}

/*
 **********************************************************************
 * push multiple
 * 	Highest Bit is R0, verified with Real Device: 0xec 0x80
 *	Order on stack verified with real device
 * v0
 **********************************************************************
 */
void
m16c_pushm_src()
{
	uint8_t src = M16C_Read8(M16C_REG_PC);
	M16C_REG_PC++;
	if (src & 1) {
		M16C_REG_SP -= 2;
		M16C_Write16(M16C_REG_FB, M16C_REG_SP);
	}
	if (src & 2) {
		M16C_REG_SP -= 2;
		M16C_Write16(M16C_REG_SB, M16C_REG_SP);
	}
	if (src & 4) {
		M16C_REG_SP -= 2;
		M16C_Write16(M16C_REG_A1, M16C_REG_SP);
	}
	if (src & 8) {
		M16C_REG_SP -= 2;
		M16C_Write16(M16C_REG_A0, M16C_REG_SP);
	}
	if (src & 0x10) {
		M16C_REG_SP -= 2;
		M16C_Write16(M16C_REG_R3, M16C_REG_SP);
	}
	if (src & 0x20) {
		M16C_REG_SP -= 2;
		M16C_Write16(M16C_REG_R2, M16C_REG_SP);
	}
	if (src & 0x40) {
		M16C_REG_SP -= 2;
		M16C_Write16(M16C_REG_R1, M16C_REG_SP);
	}
	if (src & 0x80) {
		M16C_REG_SP -= 2;
		M16C_Write16(M16C_REG_R0, M16C_REG_SP);
	}
	dbgprintf("instr m16c_pushm_src(%04x) not implemented\n", ICODE8());
}

/**
 *********************************************************************
 * \fn void m16c_reit() 
 * Return from interrupt.
 * v0
 *********************************************************************
 */
void
m16c_reit()
{
	uint32_t pcml;
	uint16_t flg;
	uint32_t flghpch;
	pcml = M16C_Read16(M16C_REG_SP);
	M16C_REG_SP += 2;
	flg = M16C_Read8(M16C_REG_SP);
	M16C_REG_SP += 1;
	flghpch = M16C_Read8(M16C_REG_SP);
	M16C_REG_SP += 1;
	M16C_REG_PC = pcml | ((flghpch & 0xf) << 16);
	M16C_SET_REG_FLG(flg | ((flghpch & 0xf0) << 8));
	dbgprintf("instr m16c_reit(%04x) not implemented\n", ICODE8());
}

/**
 ******************************************************************
 * \fn void m16c_rmpa_b() 
 * Repeat multiply and addition.
 * v0
 ******************************************************************
 */
void
m16c_rmpa_b()
{
	int32_t r0, ma0, ma1;
	if (M16C_REG_R3) {
		r0 = M16C_REG_R0;
		ma0 = (int32_t) (int8_t) M16C_Read8(M16C_REG_A0);
		ma1 = (int32_t) (int8_t) M16C_Read8(M16C_REG_A1);
		r0 = r0 + ma0 * ma1;
		M16C_REG_R0 = r0;
		M16C_REG_A0 += 1;
		M16C_REG_A1 += 1;
		M16C_REG_R3--;
		M16C_REG_PC -= 2;
	}
	dbgprintf("instr m16c_rmpa(%04x) not implemented\n", ICODE16());
}

/**
 ********************************************************************
 * \fn void m16c_rmpa_w() 
 * Repeat Multiply and addition. 
 * v0
 ********************************************************************
 */
void
m16c_rmpa_w()
{
	int32_t r2r0, ma0, ma1;
	while (M16C_REG_R3) {
		r2r0 = M16C_REG_R0 | (((int32_t) M16C_REG_R2) << 16);
		ma0 = (int32_t) (int16_t) M16C_Read16(M16C_REG_A0);
		ma1 = (int32_t) (int16_t) M16C_Read16(M16C_REG_A1);
		r2r0 = r2r0 + ma0 * ma1;
		M16C_REG_R0 = r2r0 & 0xffff;
		M16C_REG_R2 = r2r0 >> 16;
		M16C_REG_A0 += 2;
		M16C_REG_A1 += 2;
		M16C_REG_R3--;
		M16C_REG_PC -= 2;
	}
	dbgprintf("instr m16c_rmpa(%04x) not implemented\n", ICODE16());
}

/**
 ********************************************************************
 * \fn void m16c_rolc_size_dst() 
 * Rotate left with Carry.
 * v0
 ********************************************************************
 */
void
m16c_rolc_size_dst()
{
	int dst;
	int size, opsize;
	int codelen_dst;
	uint32_t Dst, Result;
	int carry_new;
	GAM_GetProc *getdst;
	GAM_SetProc *setdst;
	dst = ICODE16() & 0xf;
	if (ICODE16() & 0x100) {
		opsize = size = 2;
	} else {
		opsize = size = 1;
		ModOpsize(dst, &opsize);
	}
	getdst = general_am_get(dst, size, &codelen_dst, GAM_ALL);
	setdst = general_am_set(dst, opsize, &codelen_dst, GAM_ALL);
	getdst(&Dst, opsize);
	if (opsize == 2) {
		carry_new = Dst & 0x8000;
	} else {
		carry_new = Dst & 0x80;
	}
	if (M16C_REG_FLG & M16C_FLG_CARRY) {
		Result = (Dst << 1) | 1;
	} else {
		Result = Dst << 1;
	}
	if (opsize == 2) {
		Result = Result & 0xffff;
	} else {
		Result = Result & 0xff;
	}
	if (carry_new) {
		M16C_REG_FLG |= M16C_FLG_CARRY;
	} else {
		M16C_REG_FLG &= ~M16C_FLG_CARRY;
	}
	setdst(Result, opsize);
	sgn_zero_flags(Result, opsize);
	M16C_REG_PC += codelen_dst;
	dbgprintf("instr m16c_rolc_size_dst(%04x)\n", ICODE16());
}

/**
 ***********************************************************************
 * \fn void m16c_rorc_size_dst() 
 * Rotate right with carry.
 * v0
 ***********************************************************************
 */
void
m16c_rorc_size_dst()
{

	int dst;
	int size, opsize;
	int codelen_dst;
	uint32_t Dst, Result;
	int carry_new;
	GAM_GetProc *getdst;
	GAM_SetProc *setdst;
	dst = ICODE16() & 0xf;
	if (ICODE16() & 0x100) {
		opsize = size = 2;
	} else {
		opsize = size = 1;
		ModOpsize(dst, &opsize);
	}
	getdst = general_am_get(dst, size, &codelen_dst, GAM_ALL);
	setdst = general_am_set(dst, opsize, &codelen_dst, GAM_ALL);
	getdst(&Dst, opsize);
	carry_new = Dst & 1;
	if (M16C_REG_FLG & M16C_FLG_CARRY) {
		if (opsize == 2) {
			Result = (Dst >> 1) | 0x8000;
		} else {
			Result = (Dst >> 1) | 0x80;
		}
	} else {
		Result = Dst >> 1;
	}
	if (carry_new) {
		M16C_REG_FLG |= M16C_FLG_CARRY;
	} else {
		M16C_REG_FLG &= ~M16C_FLG_CARRY;
	}
	setdst(Result, opsize);
	sgn_zero_flags(Result, opsize);
	M16C_REG_PC += codelen_dst;
	dbgprintf("instr m16c_rorc_size_dst(%04x)\n", ICODE16());
}

/**
 **********************************************************************
 * \fn void m16c_rot_size_immdst() 
 * Rotate left/right by an immediate  
 * v0
 **********************************************************************
 */
void
m16c_rot_size_immdst()
{

	int rot = ((ICODE16() >> 4) & 7) + 1;
	int right = ICODE16() & 0x80;
	int size, opsize;
	int dst;
	uint32_t Dst;
	int codelen_dst;
	GAM_GetProc *getdst;
	GAM_SetProc *setdst;
	dst = ICODE16() & 0xf;
	if (ICODE16() & 0x100) {
		opsize = size = 2;
	} else {
		opsize = size = 1;
		ModOpsize(dst, &opsize);
	}
	getdst = general_am_get(dst, size, &codelen_dst, GAM_ALL);
	setdst = general_am_set(dst, opsize, &codelen_dst, GAM_ALL);
	getdst(&Dst, opsize);
	if (right) {
		if (opsize == 2) {
			Dst = (Dst >> rot) | (Dst << (16 - rot));
			Dst = Dst & 0xffff;
			if (Dst & 0x8000) {
				M16C_REG_FLG |= M16C_FLG_CARRY;
			} else {
				M16C_REG_FLG &= ~M16C_FLG_CARRY;
			}
		} else {
			Dst = (Dst >> rot) | (Dst << (8 - rot));
			if (Dst & 0x80) {
				M16C_REG_FLG |= M16C_FLG_CARRY;
			} else {
				M16C_REG_FLG &= ~M16C_FLG_CARRY;
			}
			Dst = Dst & 0xff;
		}
	} else {
		if (opsize == 2) {
			Dst = (Dst << rot) | (Dst >> (16 - rot));
			Dst = Dst & 0xffff;
		} else {
			Dst = (Dst << rot) | (Dst >> (8 - rot));
			Dst = Dst & 0xff;
		}
		if (Dst & 1) {
			M16C_REG_FLG |= M16C_FLG_CARRY;
		} else {
			M16C_REG_FLG &= ~M16C_FLG_CARRY;
		}
	}
	sgn_zero_flags(Dst, opsize);
	setdst(Dst, opsize);
	M16C_REG_PC += codelen_dst;
	dbgprintf("instr m16c_rot_size_immdst(%04x)\n", ICODE16());
}

/**
 ******************************************************************************************
 * \fn void m16c_rot_size_r1hdst() 
 * Rotate a destination by the value in R1H
 * v0
 ******************************************************************************************
 */
void
m16c_rot_size_r1hdst()
{
	int size;
	int dst;
	int8_t r1h = M16C_REG_R1H;
	int rot = abs(r1h);
	int right = (r1h < 0);
	uint32_t Dst;
	int codelen_dst;
	GAM_GetProc *getdst;
	GAM_SetProc *setdst;
	dst = ICODE16() & 0xf;
	if (ICODE16() & 0x100) {
		size = 2;
	} else {
		size = 1;
	}
	getdst = general_am_get(dst, size, &codelen_dst, GAM_ALL);
	setdst = general_am_set(dst, size, &codelen_dst, GAM_ALL);
	getdst(&Dst, size);
	if (rot == 0) {
		/* do nothing */
	} else if (right) {
		if (size == 2) {
			Dst = (Dst >> rot) | (Dst << (16 - rot));
			if (Dst & 0x8000) {
				M16C_REG_FLG |= M16C_FLG_CARRY;
			} else {
				M16C_REG_FLG &= ~M16C_FLG_CARRY;
			}
		} else {
			Dst = (Dst >> rot) | (Dst << (8 - rot));
			if (Dst & 0x80) {
				M16C_REG_FLG |= M16C_FLG_CARRY;
			} else {
				M16C_REG_FLG &= ~M16C_FLG_CARRY;
			}
		}
		sgn_zero_flags(Dst, size);
		setdst(Dst, size);
	} else {
		if (size == 2) {
			Dst = (Dst << rot) | (Dst >> (16 - rot));
		} else {
			Dst = (Dst << rot) | (Dst >> (8 - rot));
		}
		if (Dst & 1) {
			M16C_REG_FLG |= M16C_FLG_CARRY;
		} else {
			M16C_REG_FLG &= ~M16C_FLG_CARRY;
		}
		sgn_zero_flags(Dst, size);
		setdst(Dst, size);
	}
	M16C_REG_PC += codelen_dst;
	dbgprintf("instr m16c_rot_size_r1hdst(%04x)\n", ICODE16());
}

/**
 *****************************************************************************
 * \fn void m16c_rts() 
 * Return from subroutine by popping the Programm counter from stack.
 * v0
 *****************************************************************************
 */
void
m16c_rts()
{
	M16C_REG_PC = M16C_Read24(M16C_REG_SP) & 0xfffff;
	M16C_REG_SP += 3;
	dbgprintf("instr m16c_rts(%04x)\n", ICODE8());
}

/**
 *************************************************************************
 * \fn void m16c_sbb_size_immdst() 
 * Subtract an immediate with borrow from a destination.
 * v0
 *************************************************************************
 */
void
m16c_sbb_size_immdst()
{
	int dst;
	uint32_t Src, Dst, Result;
	int opsize, srcsize, codelen_src;
	int codelen_dst;
	GAM_GetProc *getdst;
	GAM_SetProc *setdst;
	dst = ICODE16() & 0xf;
	if (ICODE16() & 0x100) {
		codelen_src = srcsize = opsize = 2;
	} else {
		codelen_src = srcsize = opsize = 1;
		ModOpsize(dst, &opsize);
	}
	getdst = general_am_get(dst, opsize, &codelen_dst, GAM_ALL);
	setdst = general_am_set(dst, opsize, &codelen_dst, GAM_ALL);
	getdst(&Dst, opsize);
	if (srcsize == 2) {
		Src = M16C_Read16(M16C_REG_PC + codelen_dst);
	} else if (srcsize == 1) {
		Src = M16C_Read8(M16C_REG_PC + codelen_dst);
	}
	if (M16C_REG_FLG & M16C_FLG_CARRY) {
		Result = Dst - Src;
	} else {
		Result = Dst - Src - 1;
	}
	setdst(Result, opsize);
	sub_flags(Dst, Src, Result, opsize);
	M16C_REG_PC += codelen_dst + codelen_src;
	dbgprintf("instr m16c_sbb_size_immdst(%04x)\n", ICODE16());
}

/**
 ***************************************************************
 * \fn void m16c_sbb_size_srcdst() 
 * Subtract a src from a destination with borrow.
 * v0
 ***************************************************************
 */
void
m16c_sbb_size_srcdst()
{
	int dst, src;
	uint32_t Src, Dst, Result;
	int srcsize, opsize;
	int codelen_src;
	int codelen_dst;
	GAM_GetProc *getdst;
	GAM_GetProc *getsrc;
	GAM_SetProc *setdst;
	dst = ICODE16() & 0xf;
	src = (ICODE16() >> 4) & 0xf;
	if (ICODE16() & 0x100) {
		srcsize = opsize = 2;
	} else {
		srcsize = opsize = 1;
		ModOpsize(dst, &opsize);
	}
	getsrc = general_am_get(src, srcsize, &codelen_src, GAM_ALL);
	getdst = general_am_get(dst, opsize, &codelen_dst, GAM_ALL);
	setdst = general_am_set(dst, opsize, &codelen_dst, GAM_ALL);
	getsrc(&Src, srcsize);
	M16C_REG_PC += codelen_src;
	getdst(&Dst, opsize);
	if (M16C_REG_FLG & M16C_FLG_CARRY) {
		Result = Dst - Src;
	} else {
		Result = Dst - Src - 1;
	}
	setdst(Result, opsize);
	sub_flags(Dst, Src, Result, opsize);
	M16C_REG_PC += codelen_dst;
	dbgprintf("instr m16c_sbb_size_srcdst(%04x)\n", ICODE16());
}

/**
 **********************************************************************
 * \fn void m16c_sha_size_immdst() 
 * Shift arithmetic left or right.
 * v0
 **********************************************************************
 */

void
m16c_sha_size_immdst()
{

	int sha = ((ICODE16() >> 4) & 7) + 1;
	int right = ICODE16() & 0x80;
	int size, opsize;
	int dst;
	uint32_t u32Dst;
	int32_t sDst;
	int codelen_dst;
	GAM_GetProc *getdst;
	GAM_SetProc *setdst;
	dst = ICODE16() & 0xf;
	if (ICODE16() & 0x100) {
		opsize = size = 2;
	} else {
		opsize = size = 1;
		ModOpsize(dst, &opsize);
	}
	getdst = general_am_get(dst, size, &codelen_dst, GAM_ALL);
	setdst = general_am_set(dst, opsize, &codelen_dst, GAM_ALL);
	getdst(&u32Dst, size);
	if (right) {
		if (opsize == 2) {
			sDst = (int32_t) (int16_t) u32Dst;
			if (sDst & (1 << (sha - 1))) {
				M16C_REG_FLG |= M16C_FLG_CARRY;
			} else {
				M16C_REG_FLG &= ~M16C_FLG_CARRY;
			}
			sDst = (sDst >> sha);
			sDst = sDst & 0xffff;
		} else {
			sDst = (int32_t) (int8_t) u32Dst;
			if (sDst & (1 << (sha - 1))) {
				M16C_REG_FLG |= M16C_FLG_CARRY;
			} else {
				M16C_REG_FLG &= ~M16C_FLG_CARRY;
			}
			sDst = (sDst >> sha);
			sDst = sDst & 0xff;
		}
		M16C_REG_FLG &= ~M16C_FLG_OVERFLOW;
	} else {
		if (opsize == 2) {
			sDst = (int32_t) (int16_t) u32Dst;
			if (sDst & (1 << (16 - sha))) {
				M16C_REG_FLG |= M16C_FLG_CARRY;
			} else {
				M16C_REG_FLG &= ~M16C_FLG_CARRY;
			}
			sDst = (sDst << sha);
			if (((sDst & UINT32_C(0xFFFF8000))
			     == UINT32_C(0xFFFF8000))
			    || ((sDst & UINT32_C(0xFFFF8000)) == 0)) {
				M16C_REG_FLG &= ~M16C_FLG_OVERFLOW;
			} else {
				M16C_REG_FLG |= M16C_FLG_OVERFLOW;
			}
			sDst = sDst & 0xffff;
		} else {
			sDst = (int32_t) (int8_t) u32Dst;
			if (sDst & (1 << (8 - sha))) {
				M16C_REG_FLG |= M16C_FLG_CARRY;
			} else {
				M16C_REG_FLG &= ~M16C_FLG_CARRY;
			}
			sDst = (sDst << sha);
			if (((sDst & UINT32_C(0xFFFFFF80))
			     == UINT32_C(0xFFFFFF80))
			    || ((sDst & UINT32_C(0xFFFFFF80)) == 0)) {
				M16C_REG_FLG &= ~M16C_FLG_OVERFLOW;
			} else {
				M16C_REG_FLG |= M16C_FLG_OVERFLOW;
			}
			sDst = sDst & 0xff;
		}
	}
	sgn_zero_flags(sDst, opsize);
	setdst(sDst, size);
	M16C_REG_PC += codelen_dst;
	dbgprintf("instr m16c_sha_size_immdst(%04x)\n", ICODE16());

}

/**
 ********************************************************************
 * \fn void m16c_sha_size_r1hdst() 
 * Shift arithmetic left or right by the value in R1H
 * v0
 ********************************************************************
 */

void
m16c_sha_size_r1hdst()
{
	int size, opsize;
	int dst;
	int8_t r1h = M16C_REG_R1H;
	int sha = abs(r1h);
	int right = (r1h < 0);
	uint32_t u32Dst;
	int32_t Dst;
	int codelen_dst;
	GAM_GetProc *getdst;
	GAM_SetProc *setdst;
	dst = ICODE16() & 0xf;
	if (ICODE16() & 0x100) {
		opsize = size = 2;
	} else {
		opsize = size = 1;
		ModOpsize(dst, &opsize);
	}
	getdst = general_am_get(dst, size, &codelen_dst, GAM_ALL);
	setdst = general_am_set(dst, opsize, &codelen_dst, GAM_ALL);
	getdst(&u32Dst, size);
	if (sha == 0) {
		/* When the shift is 0 no flags are changed */
	} else if (right) {
		if (opsize == 2) {
			Dst = (int32_t) (int16_t) u32Dst;
			if (Dst & (1 << (sha - 1))) {
				M16C_REG_FLG |= M16C_FLG_CARRY;
			} else {
				M16C_REG_FLG &= ~M16C_FLG_CARRY;
			}
			Dst = (Dst >> sha);
		} else {
			Dst = (int32_t) (int8_t) u32Dst;
			if (Dst & (1 << (sha - 1))) {
				M16C_REG_FLG |= M16C_FLG_CARRY;
			} else {
				M16C_REG_FLG &= ~M16C_FLG_CARRY;
			}
			Dst = (Dst >> sha);
		}
		setdst(Dst, opsize);
		sgn_zero_flags(Dst, opsize);
		M16C_REG_FLG &= ~M16C_FLG_OVERFLOW;
	} else {
		if (opsize == 2) {
			Dst = (int32_t) (int16_t) u32Dst;
			Dst = (Dst << sha);
			if (((Dst & UINT32_C(0xFFFF8000))
			     == UINT32_C(0xFFFF8000))
			    || ((Dst & UINT32_C(0xFFFF8000)) == 0)) {
				M16C_REG_FLG &= ~M16C_FLG_OVERFLOW;
			} else {
				M16C_REG_FLG |= M16C_FLG_OVERFLOW;
			}
			if (Dst & (1 << 16)) {
				M16C_REG_FLG |= M16C_FLG_CARRY;
			} else {
				M16C_REG_FLG &= ~M16C_FLG_CARRY;
			}
		} else {
			Dst = (int32_t) (int8_t) u32Dst;
			Dst = (Dst << sha);
			if (((Dst & UINT32_C(0xFFFFFF80))
			     == UINT32_C(0xFFFFFF80))
			    || ((Dst & UINT32_C(0xFFFFFF80)) == 0)) {
				M16C_REG_FLG &= ~M16C_FLG_OVERFLOW;
			} else {
				M16C_REG_FLG |= M16C_FLG_OVERFLOW;
			}
			if (Dst & (1 << 8)) {
				M16C_REG_FLG |= M16C_FLG_CARRY;
			} else {
				M16C_REG_FLG &= ~M16C_FLG_CARRY;
			}
		}
		setdst(Dst, opsize);
		sgn_zero_flags(Dst, opsize);
	}
	M16C_REG_PC += codelen_dst;
	dbgprintf("instr m16c_sha_size_r1hdst(%04x)\n", ICODE16());
}

/**
 ************************************************************************
 * \fn void m16c_sha_l_immdst() 
 * Shift arithmetic left or write of R2R0 or R3R1 by an immediate.
 * No overflow flag influence because it is a 32 Bit operation
 * v0
 ************************************************************************
 */
void
m16c_sha_l_immdst()
{
	int right = ICODE16() & 0x8;
	int shift = (ICODE16() & 7) + 1;
	int dst = (ICODE16() >> 4) & 1;
	int size = 4;
	int64_t Dst;
	if (dst) {
		Dst = (int64_t) (int32_t) (M16C_REG_R1 | ((uint32_t) M16C_REG_R3 << 16));
	} else {
		Dst = (int64_t) (int32_t) (M16C_REG_R0 | ((uint32_t) M16C_REG_R2 << 16));
	}
	if (right) {
		if (Dst & (UINT64_C(1) << (shift - 1))) {
			M16C_REG_FLG |= M16C_FLG_CARRY;
		} else {
			M16C_REG_FLG &= ~M16C_FLG_CARRY;
		}
		Dst = (Dst >> shift);
		sgn_zero_flags(Dst, size);
	} else {
		Dst = (Dst << shift);
		if (Dst & (UINT64_C(1) << 32)) {
			M16C_REG_FLG |= M16C_FLG_CARRY;
		} else {
			M16C_REG_FLG &= ~M16C_FLG_CARRY;
		}
		sgn_zero_flags(Dst, size);
	}
	if (dst) {
		M16C_REG_R3 = Dst >> 16;
		M16C_REG_R1 = Dst & 0xffff;
	} else {
		M16C_REG_R2 = Dst >> 16;
		M16C_REG_R0 = Dst & 0xffff;
	}
	dbgprintf("instr m16c_sha_l_immdst(%04x)\n", ICODE16());
}

/** 
 *************************************************************************
 * \fn void m16c_sha_l_r1hdst() 
 * Shift arithmetic left/right by r1h.
 * No influence on overflow flag for 32 Bit operation.
 * v0
 *************************************************************************
 */
void
m16c_sha_l_r1hdst()
{

	int8_t r1h = M16C_REG_R1H;
	int shift = abs(r1h);
	int right = (r1h < 0);
	int dstcode = (ICODE16() >> 4) & 1;
	int64_t Dst;
	int size = 4;
	if (shift == 0) {
		return;
	}
	if (dstcode) {
		Dst = (int64_t) (int32_t) (M16C_REG_R1 | ((uint32_t) M16C_REG_R3 << 16));
	} else {
		Dst = (int64_t) (int32_t) (M16C_REG_R0 | ((uint32_t) M16C_REG_R2 << 16));
	}
	if (right) {
		if (Dst & (1 << (shift - 1))) {
			M16C_REG_FLG |= M16C_FLG_CARRY;
		} else {
			M16C_REG_FLG &= ~M16C_FLG_CARRY;
		}
		Dst = Dst >> shift;
		sgn_zero_flags(Dst, size);
	} else {
		Dst = Dst << shift;
		if (Dst & (UINT64_C(1) << 32)) {
			M16C_REG_FLG |= M16C_FLG_CARRY;
		} else {
			M16C_REG_FLG &= ~M16C_FLG_CARRY;
		}
		sgn_zero_flags(Dst, size);
	}
	if (dstcode) {
		M16C_REG_R3 = Dst >> 16;
		M16C_REG_R1 = Dst & 0xffff;
	} else {
		M16C_REG_R2 = Dst >> 16;
		M16C_REG_R0 = Dst & 0xffff;
	}
	dbgprintf("instr m16c_sha_l_r1hdst(%04x) not implemented\n", ICODE16());
}

/**
 ************************************************************************
 * \fn void m16c_shl_size_immdst() 
 * Shift logical left/right by an immediate.
 * v0
 ************************************************************************
 */
void
m16c_shl_size_immdst()
{
	int shl = ((ICODE16() >> 4) & 7) + 1;
	int right = ICODE16() & 0x80;
	int size, opsize;
	int dst;
	uint32_t Dst;
	int codelen_dst;
	GAM_GetProc *getdst;
	GAM_SetProc *setdst;
	dst = ICODE16() & 0xf;
	if (ICODE16() & 0x100) {
		opsize = size = 2;
	} else {
		opsize = size = 1;
		ModOpsize(dst, &opsize);
	}
	getdst = general_am_get(dst, size, &codelen_dst, GAM_ALL);
	setdst = general_am_set(dst, opsize, &codelen_dst, GAM_ALL);
	getdst(&Dst, opsize);
	if (right) {
		if (Dst & (1 << (shl - 1))) {
			M16C_REG_FLG |= M16C_FLG_CARRY;
		} else {
			M16C_REG_FLG &= ~M16C_FLG_CARRY;
		}
		Dst = Dst >> shl;
	} else {
		Dst = Dst << shl;
		if (opsize == 2) {
			if (Dst & (1 << 16)) {
				M16C_REG_FLG |= M16C_FLG_CARRY;
			} else {
				M16C_REG_FLG &= ~M16C_FLG_CARRY;
			}
		} else {
			if (Dst & (1 << 8)) {
				M16C_REG_FLG |= M16C_FLG_CARRY;
			} else {
				M16C_REG_FLG &= ~M16C_FLG_CARRY;
			}
		}
	}
	sgn_zero_flags(Dst, opsize);
	setdst(Dst, opsize);
	M16C_REG_PC += codelen_dst;
	dbgprintf("instr m16c_shl_size_immdst(%04x) not implemented\n", ICODE16());
}

/**
 *******************************************************************************************
 * \fn void m16c_shl_size_r1hdst() 
 * Shift a destination logical by the value in Register R1H.
 * v0
 *******************************************************************************************
 */
void
m16c_shl_size_r1hdst()
{
	int8_t r1h = M16C_REG_R1H;
	int shl = abs(r1h);
	int right = (r1h < 0);
	int size, opsize;
	int dst;
	uint32_t u32Dst;
	uint64_t Dst;
	int codelen_dst;
	GAM_GetProc *getdst;
	GAM_SetProc *setdst;
	dst = ICODE16() & 0xf;
	if (ICODE16() & 0x100) {
		opsize = size = 2;
	} else {
		opsize = size = 1;
		NotModOpsize(dst);
	}
	getdst = general_am_get(dst, size, &codelen_dst, GAM_ALL);
	setdst = general_am_set(dst, opsize, &codelen_dst, GAM_ALL);
	getdst(&u32Dst, opsize);
	Dst = u32Dst;
	if (shl == 0) {
		/* In case of zero shift don't change the flags. */
	} else if (right) {
		if (Dst & (1 << (shl - 1))) {
			M16C_REG_FLG |= M16C_FLG_CARRY;
		} else {
			M16C_REG_FLG &= ~M16C_FLG_CARRY;
		}
		Dst = Dst >> shl;
		sgn_zero_flags(Dst, opsize);
		setdst(Dst, opsize);
	} else {
		Dst = Dst << shl;
		if (opsize == 2) {
			if (Dst & (1 << 16)) {
				M16C_REG_FLG |= M16C_FLG_CARRY;
			} else {
				M16C_REG_FLG &= ~M16C_FLG_CARRY;
			}
		} else {
			if (Dst & (1 << 8)) {
				M16C_REG_FLG |= M16C_FLG_CARRY;
			} else {
				M16C_REG_FLG &= ~M16C_FLG_CARRY;
			}
		}
		sgn_zero_flags(Dst, opsize);
		setdst(Dst, opsize);
	}
	M16C_REG_PC += codelen_dst;
	dbgprintf("instr m16c_shl_size_r1hdst(%04x)\n", ICODE16());
}

/**
 ******************************************************************************
 * \fn void m16c_shl_l_immdst() 
 * Shift a 32 Bit register logical by an immediate.
 * v0
 ******************************************************************************
 */
void
m16c_shl_l_immdst()
{
	int right = ICODE16() & 0x8;
	int shift = (ICODE16() & 7) + 1;
	int dst = (ICODE16() >> 4) & 1;
	int size = 4;
	uint64_t Dst;
	if (dst) {
		Dst = (M16C_REG_R1 | ((uint32_t) M16C_REG_R3 << 16));
	} else {
		Dst = (M16C_REG_R0 | ((uint32_t) M16C_REG_R2 << 16));
	}
	if (right) {
		if (Dst & (1 << (shift - 1))) {
			M16C_REG_FLG |= M16C_FLG_CARRY;
		} else {
			M16C_REG_FLG &= ~M16C_FLG_CARRY;
		}
		Dst = Dst >> shift;
	} else {
		Dst = Dst << shift;
		if (Dst & (UINT64_C(1) << 32)) {
			M16C_REG_FLG |= M16C_FLG_CARRY;
		} else {
			M16C_REG_FLG &= ~M16C_FLG_CARRY;
		}
	}
	sgn_zero_flags(Dst, size);
	if (dst) {
		M16C_REG_R3 = Dst >> 16;
		M16C_REG_R1 = Dst & 0xffff;
	} else {
		M16C_REG_R2 = Dst >> 16;
		M16C_REG_R0 = Dst & 0xffff;
	}
	dbgprintf("instr m16c_shl_immdst(%04x)\n", ICODE16());
}

/**
 ************************************************************************
 * \fn void m16c_shl_l_r1hdst() 
 * Shift logical by Register R1H. 
 * v0
 ************************************************************************
 */
void
m16c_shl_l_r1hdst()
{

	int8_t r1h = M16C_REG_R1H;
	int shift = abs(r1h);
	int right = (r1h < 0);
	int dstcode = (ICODE16() >> 4) & 1;
	uint64_t Dst;
	int size = 4;
	if (shift == 0) {
		return;
	}
	if (dstcode) {
		Dst = (M16C_REG_R1 | ((uint32_t) M16C_REG_R3 << 16));
	} else {
		Dst = (M16C_REG_R0 | ((uint32_t) M16C_REG_R2 << 16));
	}
	if (right) {
		if (Dst & (1 << (shift - 1))) {
			M16C_REG_FLG |= M16C_FLG_CARRY;
		} else {
			M16C_REG_FLG &= ~M16C_FLG_CARRY;
		}
		Dst = Dst >> shift;
		sgn_zero_flags(Dst, size);
	} else {
		Dst = Dst << shift;
		if (Dst & (UINT64_C(1) << 32)) {
			M16C_REG_FLG |= M16C_FLG_CARRY;
		} else {
			M16C_REG_FLG &= ~M16C_FLG_CARRY;
		}
		sgn_zero_flags(Dst, size);
	}
	if (dstcode) {
		M16C_REG_R3 = Dst >> 16;
		M16C_REG_R1 = Dst & 0xffff;
	} else {
		M16C_REG_R2 = Dst >> 16;
		M16C_REG_R0 = Dst & 0xffff;
	}
	dbgprintf("instr m16c_shl_r1hdst(%04x)\n", ICODE16());
}

/*
 *******************************************************************************
 * \fn void m16c_smovb_size() 
 * If R3 is not equal to zero a byte/word is copied the address is decremented
 * and the instruction is repeated.
 * v0
 *******************************************************************************
 */
void
m16c_smovb_size()
{
	if (M16C_REG_R3) {
		uint32_t addr = (((uint32_t) M16C_REG_R1H << 16) | M16C_REG_A0) & 0xfffff;
		if (ICODE16() & 0x100) {
			uint16_t val = M16C_Read16(addr);
			M16C_Write16(val, M16C_REG_A1);
			if (M16C_REG_A0 <= 1) {
				M16C_REG_R1H--;
			}
			M16C_REG_A0 -= 2;
			M16C_REG_A1 -= 2;
		} else {
			uint8_t val = M16C_Read8(addr);
			M16C_Write8(val, M16C_REG_A1);
			if (M16C_REG_A0 == 0) {
				M16C_REG_R1H--;
			}
			M16C_REG_A0 -= 1;
			M16C_REG_A1 -= 1;
		}
		M16C_REG_R3 -= 1;
		M16C_REG_PC -= 2;
	}
	dbgprintf("instr m16c_smovb_size(%04x) \n", ICODE16());
}

/**
 ******************************************************************************
 * \fn void m16c_smovf_size() 
 * String move forward. If register R3 is not equal to zero a byte/word is
 * copied and the src/dst address is incremented and the instruction is 
 * repeated.
 * v0
 ******************************************************************************
 */
void
m16c_smovf_size()
{
	//fprintf(stderr,"A0-A1 %06x-%06x\n",M16C_REG_A0,M16C_REG_A1);
	if (M16C_REG_R3) {
		uint32_t addr = (M16C_REG_R1H << 16) + M16C_REG_A0;
		if (ICODE16() & 0x100) {
			uint16_t val = M16C_Read16(addr);
			M16C_Write16(val, M16C_REG_A1);
			if (M16C_REG_A0 >= 0xfffe) {
				M16C_REG_R1H++;
			}
			M16C_REG_A0 += 2;
			M16C_REG_A1 += 2;
		} else {
			uint8_t val = M16C_Read16(addr);
			M16C_Write8(val, M16C_REG_A1);
			if (M16C_REG_A0 == 0xffff) {
				M16C_REG_R1H++;
			}
			M16C_REG_A0 += 1;
			M16C_REG_A1 += 1;
		}
		M16C_REG_R3 -= 1;
		M16C_REG_PC -= 2;
	}
	dbgprintf("instr m16c_smovf_size(%04x) not implemented\n", ICODE16());
}

/**
 ****************************************************************************************
 * \fn void m16c_sstr_size() 
 * memset operation. If R3 is not zero Register R0/R0L is written to a destination. 
 * and the instruction is repeated.
 * v0
 ****************************************************************************************
 */
void
m16c_sstr_size()
{
	//fprintf(stderr,"SSTR %06x\n",M16C_REG_A1);
	if (M16C_REG_R3) {
		if (ICODE16() & 0x100) {
			uint16_t val = M16C_REG_R0;
			M16C_Write16(val, M16C_REG_A1);
			M16C_REG_A1 += 2;
		} else {
			uint8_t val = M16C_REG_R0L;
			M16C_Write8(val, M16C_REG_A1);
			M16C_REG_A1 += 1;
		}
		M16C_REG_R3 -= 1;
		M16C_REG_PC -= 2;
	}
	dbgprintf("instr m16c_sstr_size(%04x) R3 %d\n", ICODE16(), M16C_REG_R3);
}

/*
 **********************************************************
 * \fn void m16c_stc_srcdst() 
 * Store from controlregister.
 * v0
 **********************************************************
 */
void
m16c_stc_srcdst()
{
	int creg = (ICODE16() >> 4) & 7;
	int size = 2;
	int codelen_dst;
	int dst = ICODE16() & 0xf;
	uint32_t Src = get_creg(creg);
	GAM_SetProc *setdst;
	setdst = general_am_set(dst, size, &codelen_dst, GAM_ALL);
	setdst(Src, size);
	M16C_REG_PC += codelen_dst;
	dbgprintf("instr m16c_stc_srcdst(%04x)\n", ICODE16());
}

/*
 **************************************************************************
 * This should be verified with real device !!!!!!!!!!!!! 
 **************************************************************************
 */
void
m16c_stc_pcdst(uint16_t icode)
{
	uint32_t Src = M16C_REG_PC - 2;	/* ????? */
	int size = 3;
	int codelen_dst;
	int dst = ICODE16() & 0xf;
	GAM_SetProc *setdst;
	setdst = general_am_set(dst, size, &codelen_dst, GAM_ALL);
	setdst(Src, size);
	M16C_REG_PC += codelen_dst;
	fprintf(stderr, "Warning STC PC specification is unprecise\n");
}

/*
 *************************************************************
 * documented in the assembly manual rej05b0085 
 *************************************************************
 */
void
m16c_stctx_abs16abs20()
{
	uint32_t table_base;
	uint16_t abs16;
	uint32_t addr20;
	uint8_t regset;
	uint8_t spdiff;
	uint16_t regp;
	abs16 = M16C_Read16(M16C_REG_PC);
	M16C_REG_PC += 2;
	table_base = M16C_Read24(M16C_REG_PC);
	M16C_REG_PC += 3;
	addr20 = (table_base + ((uint32_t) abs16 << 1)) & 0xfffff;
	regset = M16C_Read8(addr20);
	addr20 = (addr20 + 1) & 0xfffff;
	regp = M16C_REG_SP;
	if (regset & 0x80) {
		regp -= 2;
		M16C_Write16(M16C_REG_FB, regp);
	}
	if (regset & 0x40) {
		regp -= 2;
		M16C_Write16(M16C_REG_SB, regp);
	}
	if (regset & 0x20) {
		regp -= 2;
		M16C_Write16(M16C_REG_A1, regp);
	}
	if (regset & 0x10) {
		regp -= 2;
		M16C_Write16(M16C_REG_A0, regp);
	}
	if (regset & 8) {
		regp -= 2;
		M16C_Write16(M16C_REG_R3, regp);
	}
	if (regset & 4) {
		regp -= 2;
		M16C_Write16(M16C_REG_R2, regp);
	}
	if (regset & 2) {
		regp -= 2;
		M16C_Write16(M16C_REG_R1, regp);
	}
	if (regset & 1) {
		regp -= 2;
		M16C_Write16(M16C_REG_R0, regp);
	}
	spdiff = M16C_Read8(addr20);
	M16C_REG_SP -= spdiff;
	if (M16C_REG_SP != regp) {
		fprintf(stderr, "Unexpected SP in stctx\n");
	}
	dbgprintf("instr m16c_stctx_abs16abs20(%04x)\n", ICODE16());
}

/**
 ******************************************************************
 * \fn void m16c_ste_size_srcabs20() 
 * Store a source in the extended area with an absolute 20 Bit
 * address.
 * v0
 ******************************************************************
 */
void
m16c_ste_size_srcabs20()
{
	int codelen_src;
	uint32_t abs20;
	int size;
	uint32_t Src;
	GAM_GetProc *getsrc;
	int src = ICODE16() & 0xf;
	if (ICODE16() & 0x100) {
		size = 2;
	} else {
		size = 1;
	}
	getsrc = general_am_get(src, size, &codelen_src, GAM_ALL);
	getsrc(&Src, size);
	M16C_REG_PC += codelen_src;
	abs20 = M16C_Read24(M16C_REG_PC);
	M16C_REG_PC += 3;
	abs20 &= 0xfffff;
	if (size == 2) {
		M16C_Write16(Src, abs20);
	} else {
		M16C_Write8(Src, abs20);
	}
	dbgprintf("instr m16c_ste_size_srcabs20(%04x)\n", ICODE16());
}

/**
 *************************************************************************
 * \fn void m16c_ste_size_srcdsp20() 
 * Store a source in extended area with a 20 Bit offset to register A0.
 * v0
 *************************************************************************
 */
void
m16c_ste_size_srcdsp20()
{
	uint32_t dsp20, addr20;
	int codelen_src;
	int size;
	uint32_t Src;
	int src = ICODE16() & 0xf;
	GAM_GetProc *getsrc;
	if (ICODE16() & 0x100) {
		size = 2;
	} else {
		size = 1;
	}
	getsrc = general_am_get(src, size, &codelen_src, GAM_ALL);
	getsrc(&Src, size);
	M16C_REG_PC += codelen_src;
	dsp20 = M16C_Read24(M16C_REG_PC);
	M16C_REG_PC += 3;
	addr20 = (dsp20 + M16C_REG_A0) & 0xfffff;
	if (size == 2) {
		M16C_Write16(Src, addr20);
	} else {
		M16C_Write8(Src, addr20);
	}
	dbgprintf("instr m16c_ste_size_srcdsp20(%04x)\n", ICODE16());
}

/**
 ****************************************************************************
 * \fn void m16c_ste_size_srca1a0() 
 * Store a source in extended area addressed by Register A1A0
 * v0
 ****************************************************************************
 */
void
m16c_ste_size_srca1a0()
{
	int codelen_src;
	int size;
	uint32_t addr20;
	uint32_t Src;
	int src = ICODE16() & 0xf;
	GAM_GetProc *getsrc;
	if (ICODE16() & 0x100) {
		size = 2;
	} else {
		size = 1;
	}
	getsrc = general_am_get(src, size, &codelen_src, GAM_ALL);
	getsrc(&Src, size);
	M16C_REG_PC += codelen_src;
	addr20 = (M16C_REG_A0 | (M16C_REG_A1 << 16)) & 0xfffff;
	if (size == 2) {
		M16C_Write16(Src, addr20);
	} else {
		M16C_Write8(Src, addr20);
	}
	dbgprintf("instr m16c_ste_size_srca1a0(%04x)\n", ICODE16());
}

/**
 ***********************************************************************************
 * \fn void m16c_stnz_immdst() 
 * Store an immediate in a destination if Zero Flag is not set. 
 * v0
 ***********************************************************************************
 */
void
m16c_stnz_immdst()
{
	uint8_t imm8;
	int codelen;
	int dst = ICODE8() & 7;
	if (M16C_REG_FLG & M16C_FLG_ZERO) {
		/* required for codelen */
		M16C_REG_PC++;
		am2b_get(dst, &codelen);
	} else {
		imm8 = M16C_Read8(M16C_REG_PC);
		M16C_REG_PC++;
		am2b_set(dst, &codelen, imm8);
	}
	M16C_REG_PC += codelen;
	dbgprintf("instr m16c_stnz_immdst(%04x)\n", ICODE8());
}

/**
 *****************************************************************+
 * \fn void m16c_stz_immdst() 
 * Store an immediate in a destination if the zero flag is set.
 * v0
 *****************************************************************+
 */
void
m16c_stz_immdst()
{
	uint8_t imm8;
	int codelen;
	int dst = ICODE8() & 7;
	if (M16C_REG_FLG & M16C_FLG_ZERO) {
		imm8 = M16C_Read8(M16C_REG_PC);
		M16C_REG_PC++;
		am2b_set(dst, &codelen, imm8);
	} else {
		/* just for getting codelen of dst */
		M16C_REG_PC++;
		am2b_get(dst, &codelen);
	}
	M16C_REG_PC += codelen;
	dbgprintf("instr m16c_stz_immdst(%04x)\n", ICODE8());
}

/**
 ******************************************************************
 * \fn void m16c_stzx_immimmdst() 
 * Store immediate 1 if Zero flag is set, else store immediate 2.
 * v0
 ******************************************************************
 */
void
m16c_stzx_immimmdst()
{
	int imm8_1, imm8_2;
	int codelen;
	int dst = ICODE8() & 0x7;

	imm8_1 = M16C_Read8(M16C_REG_PC);
	M16C_REG_PC++;
	am2b_get(dst, &codelen);	// shit just for getting arglen 
	imm8_2 = M16C_Read8(M16C_REG_PC + codelen);
	if (M16C_REG_FLG & M16C_FLG_ZERO) {
		am2b_set(dst, &codelen, imm8_1);
	} else {
		am2b_set(dst, &codelen, imm8_2);
	}
	M16C_REG_PC += codelen + 1;
	dbgprintf("instr m16c_stzx_immimmdst(%04x)\n", ICODE8());
}

/*
 ************************************************************
 * \fn void m16c_sub_size_g_immdst() 
 * Subtract an immediate from a destination.
 * v0
 ************************************************************
 */
void
m16c_sub_size_g_immdst()
{
	int dst;
	uint32_t Src, Dst, Result;
	int srcsize, opsize;
	int codelen_dst;
	GAM_GetProc *getdst;
	GAM_SetProc *setdst;
	dst = ICODE16() & 0xf;
	if (ICODE16() & 0x100) {
		srcsize = opsize = 2;
	} else {
		srcsize = opsize = 1;
		ModOpsize(dst, &opsize);
	}
	getdst = general_am_get(dst, opsize, &codelen_dst, GAM_ALL);
	setdst = general_am_set(dst, opsize, &codelen_dst, GAM_ALL);
	getdst(&Dst, opsize);
	if (srcsize == 2) {
		Src = M16C_Read16(M16C_REG_PC + codelen_dst);
	} else if (srcsize == 1) {
		Src = M16C_Read8(M16C_REG_PC + codelen_dst);
	}
	Result = Dst - Src;
	setdst(Result, opsize);
	sub_flags(Dst, Src, Result, opsize);
	M16C_REG_PC += codelen_dst + srcsize;
	dbgprintf("instr m16c_sub_size_g_immdst(%04x)\n", ICODE16());
}

/**
 ***************************************************************************
 * \fn void m16c_sub_b_s_immdst() 
 * Subtract an 8 Bit immediate from a Destination.
 * V0 
 ***************************************************************************
 */
void
m16c_sub_b_s_immdst()
{
	int codelen_dst;
	uint32_t Dst, Src, Result;
	int dst = ICODE8() & 7;
	Src = M16C_Read8(M16C_REG_PC);
	M16C_REG_PC += 1;
	Dst = am2b_get(dst, &codelen_dst);
	Result = Dst - Src;
	am2b_set(dst, &codelen_dst, Result);
	M16C_REG_PC += codelen_dst;
	sub_flags(Dst, Src, Result, 1);
	dbgprintf("instr m16c_sub_b_s_immdst(%04x)\n", ICODE8());
}

/**
 ***************************************************************************************
 * \fn void m16c_sub_size_g_srcdst() 
 * Subtract a source from a destination.
 * v0
 ***************************************************************************************
 */
void
m16c_sub_size_g_srcdst()
{
	int dst, src;
	uint32_t Src, Dst, Result;
	int srcsize, opsize;
	int codelen_src;
	int codelen_dst;
	GAM_GetProc *getdst;
	GAM_GetProc *getsrc;
	GAM_SetProc *setdst;
	dst = ICODE16() & 0xf;
	src = (ICODE16() >> 4) & 0xf;
	if (ICODE16() & 0x100) {
		srcsize = opsize = 2;
	} else {
		srcsize = opsize = 1;
		ModOpsize(dst, &opsize);
	}
	getdst = general_am_get(dst, opsize, &codelen_dst, GAM_ALL);
	getsrc = general_am_get(src, srcsize, &codelen_src, GAM_ALL);
	setdst = general_am_set(dst, opsize, &codelen_dst, GAM_ALL);
	getsrc(&Src, srcsize);
	M16C_REG_PC += codelen_src;
	getdst(&Dst, opsize);
	Result = Dst - Src;
	setdst(Result, opsize);
	sub_flags(Dst, Src, Result, opsize);
	M16C_REG_PC += codelen_dst;
	dbgprintf("instr m16c_sub_size_g_srcdst(%04x)\n", ICODE16());
}

/**
 **************************************************************************** 
 * \fn void m16c_sub_b_srcr0lr0h() 
 * Subtract a Source from R0H/R0L.
 * v0
 **************************************************************************** 
 */
void
m16c_sub_b_srcr0lr0h()
{
	uint32_t Src, Dst, Result;
	int codelen_src;
	int src = ICODE8() & 7;
	Src = am3b_get(src, &codelen_src);
	M16C_REG_PC += codelen_src;
	if (ICODE8() & 4) {
		Dst = M16C_REG_R0H;
		M16C_REG_R0H = Result = Dst - Src;
	} else {
		Dst = M16C_REG_R0L;
		M16C_REG_R0L = Result = Dst - Src;
	}
	sub_flags(Dst, Src, Result, 0);
	dbgprintf("instr m16c_sub_b_srcr0lr0h(%04x)\n", ICODE8());
}

/**
 *****************************************************************************
 * \fn void m16c_tst_size_immdst() 
 * Logical and of an immediate and a destination without storing the result,
 * but with flag change. 
 * v0
 *****************************************************************************
 */
void
m16c_tst_size_immdst()
{
	int dst;
	uint32_t Src, Dst, Result;
	int srcsize, opsize;
	int codelen;
	GAM_GetProc *getdst;
	dst = ICODE16() & 0xf;
	if (ICODE16() & 0x100) {
		srcsize = opsize = 2;
	} else {
		srcsize = opsize = 1;
		ModOpsize(dst, &opsize);
	}
	getdst = general_am_get(dst, opsize, &codelen, GAM_ALL);
	getdst(&Dst, opsize);
	if (srcsize == 2) {
		Src = M16C_Read16(M16C_REG_PC + codelen);
	} else if (srcsize == 1) {
		Src = M16C_Read8(M16C_REG_PC + codelen);
	}
	Result = Dst & Src;
	and_flags(Result, opsize);
	M16C_REG_PC += codelen + srcsize;
	dbgprintf("instr m16c_tst_size_immdst(%04x)\n", ICODE16());
}

/**
 *************************************************************************************
 * \fn void m16c_tst_size_srcdst() 
 * Modify flags depending on result of the logical and of a source
 * and a destination.
 * v0
 *************************************************************************************
 */
void
m16c_tst_size_srcdst()
{
	int dst, src;
	uint32_t Src, Dst, Result;
	int srcsize, opsize;
	int codelen_src;
	int codelen_dst;
	GAM_GetProc *getdst;
	GAM_GetProc *getsrc;
	dst = ICODE16() & 0xf;
	src = (ICODE16() >> 4) & 0xf;
	if (ICODE16() & 0x100) {
		srcsize = opsize = 2;
	} else {
		srcsize = opsize = 1;
		ModOpsize(dst, &opsize);
	}
	getdst = general_am_get(dst, opsize, &codelen_dst, GAM_ALL);
	getsrc = general_am_get(src, opsize, &codelen_src, GAM_ALL);
	getsrc(&Src, srcsize);
	M16C_REG_PC += codelen_src;
	getdst(&Dst, opsize);
	Result = Dst & Src;
	and_flags(Result, opsize);
	M16C_REG_PC += codelen_dst;
	dbgprintf("instr m16c_tst_size_srcdst(%04x)\n", ICODE16());
}

/**
 ******************************************************************************
 * \fn void m16c_und() 
 * R8C manual says 0xFFFDC here even if its vector base is 0xFFDC.
 * Maybe the R8C manual is wrong ?
 ******************************************************************************
 */
void
m16c_und()
{
	uint16_t flg = M16C_REG_FLG;
	M16C_SET_REG_FLG(M16C_REG_FLG & ~(M16C_FLG_I | M16C_FLG_D | M16C_FLG_U));
	M16C_REG_SP -= 1;
	M16C_Write8(((M16C_REG_PC >> 16) & 0xf) | ((flg >> 8) & 0xf0), M16C_REG_SP);
	M16C_REG_SP -= 1;
	M16C_Write8(flg, M16C_REG_SP);
	M16C_REG_SP -= 2;
	M16C_Write16(M16C_REG_PC, M16C_REG_SP);
	M16C_REG_PC = M16C_Read24(0xfffdc) & 0xfffff;
	dbgprintf("Undefined instruction m16c_und(%04x)\n", ICODE8());
}

/*
 * -------------------------------------------------------------------------
 * Wait for interrupt
 * -------------------------------------------------------------------------
 */
void
m16c_wait()
{
	// wait for interrupt
	fprintf(stderr, "instr m16c_wait(%04x) not implemented\n", ICODE16());
}

/**
 ****************************************************************************
 * \fn void m16c_xchg_size_srcdst() 
 * Exchange a source with a destination. No flag change.
 * v0
 ****************************************************************************
 */
void
m16c_xchg_size_srcdst()
{
	int dst, src;
	uint32_t Src, Dst;
	int size, dstwsize;
	int codelen_dst, codelen_src;
	GAM_GetProc *getdst;
	GAM_SetProc *setdst;
	GAM_GetProc *getsrc;
	GAM_SetProc *setsrc;
	dst = ICODE16() & 0xf;
	src = (ICODE16() >> 4) & 3;
	if (ICODE16() & 0x100) {
		dstwsize = size = 2;
	} else {
		dstwsize = size = 1;
		ModOpsize(dst, &dstwsize);
	}
	getsrc = general_am_get(src, size, &codelen_src, GAM_ALL);
	setsrc = general_am_set(src, size, &codelen_src, GAM_ALL);
	getdst = general_am_get(dst, size, &codelen_dst, GAM_ALL);
	setdst = general_am_set(dst, dstwsize, &codelen_dst, GAM_ALL);
	getsrc(&Src, size);
	getdst(&Dst, size);
	setdst(Src, dstwsize);
	setsrc(Dst, size);
	M16C_REG_PC += codelen_dst;
	dbgprintf("instr m16c_xchg_size_srcdst(%04x) not implemented\n", ICODE16());
}

/**
 *******************************************************************************************
 * \fn void m16c_xor_size_immdst() 
 * Exclusive or of an immediate and a destination.
 * v0 
 *******************************************************************************************
 */
void
m16c_xor_size_immdst()
{
	int dst;
	uint32_t Src, Dst, Result;
	int srcsize, opsize;
	int codelen;
	GAM_GetProc *getdst;
	GAM_SetProc *setdst;
	dst = ICODE16() & 0xf;
	if (ICODE16() & 0x100) {
		srcsize = opsize = 2;
	} else {
		srcsize = opsize = 1;
		ModOpsize(dst, &opsize);
	}
	getdst = general_am_get(dst, opsize, &codelen, GAM_ALL);
	setdst = general_am_set(dst, opsize, &codelen, GAM_ALL);
	getdst(&Dst, opsize);
	if (srcsize == 2) {
		Src = M16C_Read16(M16C_REG_PC + codelen);
	} else if (srcsize == 1) {
		Src = M16C_Read8(M16C_REG_PC + codelen);
	}
	Result = Dst ^ Src;
	setdst(Result, opsize);
	xor_flags(Result, opsize);
	M16C_REG_PC += codelen + srcsize;
	dbgprintf("instr m16c_xor_size_immdst(%04x)\n", ICODE16());
}

/**
 ***************************************************************************************
 * \fn void m16c_xor_size_srcdst() 
 * Exclusive or of a source and a destination.
 ***************************************************************************************
 */
void
m16c_xor_size_srcdst()
{
	int dst, src;
	uint32_t Src, Dst, Result;
	int srcsize, opsize;
	int codelen_dst;
	int codelen_src;
	GAM_GetProc *getdst;
	GAM_SetProc *setdst;
	GAM_GetProc *getsrc;
	dst = ICODE16() & 0xf;
	src = (ICODE16() >> 4) & 0xf;
	if (ICODE16() & 0x100) {
		srcsize = opsize = 2;
	} else {
		srcsize = opsize = 1;
		ModOpsize(dst, &opsize);
	}
	getdst = general_am_get(dst, opsize, &codelen_dst, GAM_ALL);
	setdst = general_am_set(dst, opsize, &codelen_dst, GAM_ALL);
	getsrc = general_am_get(src, srcsize, &codelen_src, GAM_ALL);
	getsrc(&Src, srcsize);
	M16C_REG_PC += codelen_src;
	getdst(&Dst, opsize);
	Result = Dst ^ Src;
	setdst(Result, opsize);
	xor_flags(Result, opsize);
	M16C_REG_PC += codelen_dst;
	dbgprintf("instr m16c_xor_size_srcdst(%04x) not implemented\n", ICODE16());
}
