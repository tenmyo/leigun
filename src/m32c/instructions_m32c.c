/*
 *************************************************************************************************
 *
 * M32C instruction set
 *
 * State: complete with some bugs 
 *
 * Copyright 2010 Jochen Karrer. All rights reserved.
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
#include <cpu_m32c.h>
#include <assert.h>
#include "cycletimer.h"

#if 0
#define dbgprintf(x...) { dbgprintf(x); }
#else
#define dbgprintf(x...)
#endif

#define ISNEG(x) ((x)&(1<<31))
#define ISNOTNEG(x) (!((x)&(1<<31)))

#define ISNEGW(x) ((x)&(1<<15))
#define ISNOTNEGW(x) (!((x)&(1<<15)))

#define ISNEGB(x) ((x)&(1<<7))
#define ISNOTNEGB(x) (!((x)&(1<<7)))

/**
 * Flag list of supported General Addressing modes for each
 * instruction
 */
#define GAM_R0L		(1 << 0)
#define GAM_R0		(1 << 1)
#define GAM_R2R0	(1 << 2)
#define GAM_R1L		(1 << 3)
#define GAM_R1		(1 << 4)
#define GAM_R3R1	(1 << 5)
#define GAM_R0H		(1 << 6)
#define	GAM_R2		(1 << 7)
#define GAM_R1H		(1 << 8)
#define GAM_R3		(1 << 9)
#define GAM_A0		(1 << 10)
#define GAM_A1		(1 << 11)
#define GAM_IA0		(1 << 12)
#define GAM_IA1		(1 << 13)
#define GAM_DSP8IA0	(1 << 14)
#define GAM_DSP8IA1	(1 << 15)
#define GAM_DSP8ISB	(1 << 16)
#define GAM_DSP8IFB	(1 << 17)
#define GAM_DSP16IA0	(1 << 18)
#define GAM_DSP16IA1	(1 << 19)
#define GAM_DSP16ISB	(1 << 20)
#define GAM_DSP16IFB	(1 << 21)
#define GAM_DSP24IA0	(1 << 22)
#define GAM_DSP24IA1	(1 << 23)
#define GAM_ABS16	(1 << 24)
#define GAM_ABS24	(1 << 25)

#define GAM_ALL UINT32_C(0xffffffff)

#define signum(x) ((int)(((x) > 0)  ? 1  : ((x) < 0)  ? -1 : 0))

static inline uint16_t
bcd_to_uint16(uint16_t s)
{
	return (s & 0xf) +
	    10 * ((s >> 4) & 0xf) + 100 * ((s >> 8) & 0xf) + 1000 * ((s >> 12) & 0xf);
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
		return M32C_FLG_CARRY;
	} else {
		return 0;
	}

}

static inline uint16_t
add8_overflow(uint8_t op1, uint8_t op2, uint8_t result)
{
	if ((ISNEGB(op1) && ISNEGB(op2) && ISNOTNEGB(result))
	    || (ISNOTNEGB(op1) && ISNOTNEGB(op2) && ISNEGB(result))) {
		return M32C_FLG_OVERFLOW;
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
		return M32C_FLG_CARRY;
	} else {
		return 0;
	}
}

#if 0
static inline uint16_t
add16_overflow(uint16_t op1, uint16_t op2, uint16_t result)
{
	if ((ISNEGW(op1) && ISNEGW(op2) && ISNOTNEGW(result))
	    || (ISNOTNEGW(op1) && ISNOTNEGW(op2) && ISNEGW(result))) {
		return M32C_FLG_OVERFLOW;
	} else {
		return 0;
	}
}

static inline uint16_t
add32_carry(uint32_t op1, uint32_t op2, uint32_t result)
{
	if (((ISNEG(op1) && ISNEG(op2))
	     || (ISNEG(op1) && ISNOTNEG(result))
	     || (ISNEG(op2) && ISNOTNEG(result)))) {
		return M32C_FLG_CARRY;
	} else {
		return 0;
	}
}

static inline uint16_t
add32_overflow(uint32_t op1, uint32_t op2, uint32_t result)
{
	if ((ISNEG(op1) && ISNEG(op2) && ISNOTNEG(result))
	    || (ISNOTNEG(op1) && ISNOTNEG(op2) && ISNEG(result))) {
		return M32C_FLG_OVERFLOW;
	} else {
		return 0;
	}
}
#endif
static const uint8_t add_flagtab[8] = {
	0,
	M32C_FLG_CARRY,
	M32C_FLG_CARRY,
	M32C_FLG_OVERFLOW | M32C_FLG_CARRY,
	M32C_FLG_SIGN | M32C_FLG_OVERFLOW,
	M32C_FLG_SIGN,
	M32C_FLG_SIGN,
	M32C_FLG_SIGN | M32C_FLG_CARRY,
};

static inline void
addb_flags(uint32_t op1, uint32_t op2, uint32_t result)
{
	unsigned int index = ((op1 >> 7) & 1) | ((op2 >> 6) & 2) | ((result >> 5) & 4);
	uint8_t flags;
	flags = add_flagtab[index];
	if ((result & 0xff) == 0) {
		flags |= M32C_FLG_ZERO;
	}
	M32C_REG_FLG = (M32C_REG_FLG & ~(M32C_FLG_OVERFLOW | M32C_FLG_SIGN
					 | M32C_FLG_ZERO | M32C_FLG_CARRY)) | flags;
}

static inline void
addw_flags(uint32_t op1, uint32_t op2, uint32_t result)
{
	unsigned int index = ((op1 >> 15) & 1) | ((op2 >> 14) & 2) | ((result >> 13) & 4);
	uint8_t flags;
	flags = add_flagtab[index];
	if ((result & 0xffff) == 0) {
		flags |= M32C_FLG_ZERO;
	}
	M32C_REG_FLG = (M32C_REG_FLG & ~(M32C_FLG_OVERFLOW | M32C_FLG_SIGN
					 | M32C_FLG_ZERO | M32C_FLG_CARRY)) | flags;
}

static inline void
addl_flags(uint32_t op1, uint32_t op2, uint32_t result)
{
	unsigned int index = ((op1 >> 31) & 1) | ((op2 >> 30) & 2) | ((result >> 29) & 4);
	uint8_t flags;
	flags = add_flagtab[index];
	if ((result & 0xffff) == 0) {
		flags |= M32C_FLG_ZERO;
	}
	M32C_REG_FLG = (M32C_REG_FLG & ~(M32C_FLG_OVERFLOW | M32C_FLG_SIGN
					 | M32C_FLG_ZERO | M32C_FLG_CARRY)) | flags;
}

static inline void
add_flags(uint32_t op1, uint32_t op2, uint32_t result, int size)
{
	switch (size) {
	    case 1:
		    addb_flags(op1, op2, result);
		    break;
	    case 2:
		    addw_flags(op1, op2, result);
		    break;
	    case 4:
		    addl_flags(op1, op2, result);
		    break;
	}
}

static inline void
sgn_zero_flags_b(uint32_t result)
{
	M32C_REG_FLG &= ~(M32C_FLG_SIGN | M32C_FLG_ZERO);
	if ((result & 0xff) == 0) {
		M32C_REG_FLG |= M32C_FLG_ZERO;
	}
	if (ISNEGB(result)) {
		M32C_REG_FLG |= M32C_FLG_SIGN;
	}
}

static inline void
sgn_zero_flags_w(uint32_t result)
{
	M32C_REG_FLG &= ~(M32C_FLG_SIGN | M32C_FLG_ZERO);
	if ((result & 0xffff) == 0) {
		M32C_REG_FLG |= M32C_FLG_ZERO;
	}
	if (ISNEGW(result)) {
		M32C_REG_FLG |= M32C_FLG_SIGN;
	}
}

static inline void
sgn_zero_flags_l(uint32_t result)
{
	M32C_REG_FLG &= ~(M32C_FLG_SIGN | M32C_FLG_ZERO);
	if (result == 0) {
		M32C_REG_FLG |= M32C_FLG_ZERO;
	}
	if (ISNEG(result)) {
		M32C_REG_FLG |= M32C_FLG_SIGN;
	}
}

static inline void
sgn_zero_flags(uint32_t result, int size)
{
	M32C_REG_FLG &= ~(M32C_FLG_SIGN | M32C_FLG_ZERO);
	if (size == 2) {
		if ((result & 0xffff) == 0) {
			M32C_REG_FLG |= M32C_FLG_ZERO;
		}
		if (ISNEGW(result)) {
			M32C_REG_FLG |= M32C_FLG_SIGN;
		}
	} else if (size == 4) {
		if (result == 0) {
			M32C_REG_FLG |= M32C_FLG_ZERO;
		}
		if (ISNEG(result)) {
			M32C_REG_FLG |= M32C_FLG_SIGN;
		}
	} else {
		if ((result & 0xff) == 0) {
			M32C_REG_FLG |= M32C_FLG_ZERO;
		}
		if (ISNEGB(result)) {
			M32C_REG_FLG |= M32C_FLG_SIGN;
		}
	}
}

static inline void
and_flags(uint32_t result, int size)
{
	sgn_zero_flags(result, size);
}

static inline void
ext_flags(uint32_t result, int size)
{
	sgn_zero_flags(result, size);
}

static inline void
or_flags(uint32_t result, int size)
{
	sgn_zero_flags(result, size);
}

static inline void
xor_flags(uint32_t result, int size)
{
	sgn_zero_flags(result, size);
}

static inline void
rol_flags(uint32_t result, int size)
{
	sgn_zero_flags(result, size);
}

static inline void
ror_flags(uint32_t result, int size)
{
	sgn_zero_flags(result, size);
}

static inline void
rot_flags(uint32_t result, int size)
{
	sgn_zero_flags(result, size);
}

static inline void
sha_flags(uint32_t result, int size)
{
	sgn_zero_flags(result, size);
}

static inline void
shl_flags(uint32_t result, int size)
{
	sgn_zero_flags(result, size);
}

static inline void
mov_flags(uint32_t result, int size)
{
	sgn_zero_flags(result, size);
}

static inline void
movb_flags(uint32_t result)
{
	sgn_zero_flags_b(result);
}

static inline void
movw_flags(uint32_t result)
{
	sgn_zero_flags_w(result);
}

static inline void
movl_flags(uint32_t result)
{
	sgn_zero_flags_l(result);
}

static inline void
not_flags(uint32_t result, int size)
{
	sgn_zero_flags(result, size);
}

#if 0
static inline uint16_t
sub8_carry(uint16_t op1, uint16_t op2, uint16_t result)
{
	if (((ISNEGB(op1) && ISNOTNEGB(op2))
	     || (ISNEGB(op1) && ISNOTNEGB(result))
	     || (ISNOTNEGB(op2) && ISNOTNEGB(result)))) {
		return M32C_FLG_CARRY;
	} else {
		return 0;
	}
}

static inline uint16_t
sub8_overflow(uint8_t op1, uint8_t op2, uint8_t result)
{
	if ((ISNEGB(op1) && ISNOTNEGB(op2) && ISNOTNEGB(result))
	    || (ISNOTNEGB(op1) && ISNEGB(op2) && ISNEGB(result))) {
		return M32C_FLG_OVERFLOW;
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
		return M32C_FLG_CARRY;
	} else {
		return 0;
	}
}

static inline uint16_t
sub16_overflow(uint16_t op1, uint16_t op2, uint16_t result)
{
	if ((ISNEGW(op1) && ISNOTNEGW(op2) && ISNOTNEGW(result))
	    || (ISNOTNEGW(op1) && ISNEGW(op2) && ISNEGW(result))) {
		return M32C_FLG_OVERFLOW;
	} else {
		return 0;
	}
}

static inline uint16_t
sub32_carry(uint32_t op1, uint32_t op2, uint32_t result)
{
	if (((ISNEG(op1) && ISNOTNEG(op2))
	     || (ISNEG(op1) && ISNOTNEG(result))
	     || (ISNOTNEG(op2) && ISNOTNEG(result)))) {
		return M32C_FLG_CARRY;
	} else {
		return 0;
	}
}

static inline uint16_t
sub32_overflow(uint32_t op1, uint32_t op2, uint32_t result)
{
	if ((ISNEG(op1) && ISNOTNEG(op2) && ISNOTNEG(result))
	    || (ISNOTNEG(op1) && ISNEG(op2) && ISNEG(result))) {
		return M32C_FLG_OVERFLOW;
	} else {
		return 0;
	}
}
#endif

static const uint8_t sub_flagtab[8] = {
	M32C_FLG_CARRY,
	M32C_FLG_CARRY | M32C_FLG_OVERFLOW,
	0x00,
	M32C_FLG_CARRY,
	M32C_FLG_SIGN,
	M32C_FLG_CARRY | M32C_FLG_SIGN,
	M32C_FLG_OVERFLOW | M32C_FLG_SIGN,
	M32C_FLG_SIGN,
};

static inline void
subb_flags(uint32_t op1, uint32_t op2, uint32_t result)
{
	unsigned int index = ((op1 >> 7) & 1) | ((op2 >> 6) & 2) | ((result >> 5) & 4);
	uint8_t flags;
	flags = sub_flagtab[index];
	if ((result & 0xff) == 0) {
		flags |= M32C_FLG_ZERO;
	}
	M32C_REG_FLG = (M32C_REG_FLG & ~(M32C_FLG_OVERFLOW | M32C_FLG_SIGN
					 | M32C_FLG_ZERO | M32C_FLG_CARRY)) | flags;

#if 0
	fprintf(stderr, "{\n");
	for (i = 0; i < 8; i++) {
		fprintf(stderr, "	%u: 0x%02x\n", i, sub_flagtab[i]);
	}
	fprintf(stderr, "}\n");
#endif
}

static inline void
subw_flags(uint32_t op1, uint32_t op2, uint32_t result)
{
	unsigned int index = ((op1 >> 15) & 1) | ((op2 >> 14) & 2) | ((result >> 13) & 4);
	uint8_t flags;
	flags = sub_flagtab[index];
	if ((result & 0xffff) == 0) {
		flags |= M32C_FLG_ZERO;
	}
	M32C_REG_FLG = (M32C_REG_FLG & ~(M32C_FLG_OVERFLOW | M32C_FLG_SIGN
					 | M32C_FLG_ZERO | M32C_FLG_CARRY)) | flags;
}

static inline void
subl_flags(uint32_t op1, uint32_t op2, uint32_t result)
{
	unsigned int index = ((op1 >> 31) & 1) | ((op2 >> 30) & 2) | ((result >> 29) & 4);
	uint8_t flags;
	flags = sub_flagtab[index];
	if (result == 0) {
		flags |= M32C_FLG_ZERO;
	}
	M32C_REG_FLG = (M32C_REG_FLG & ~(M32C_FLG_OVERFLOW | M32C_FLG_SIGN
					 | M32C_FLG_ZERO | M32C_FLG_CARRY)) | flags;
}

static inline void
sub_flags(uint32_t op1, uint32_t op2, uint32_t result, int size)
{
	switch (size) {
	    case 1:
		    subb_flags(op1, op2, result);
		    break;
	    case 2:
		    subw_flags(op1, op2, result);
		    break;
	    case 4:
		    subl_flags(op1, op2, result);
		    break;
	}
}

static uint8_t condition_map[512];

static void
init_condition_map(void)
{
	int i;
	int result = 0;
	int carry = 0, zero = 0, sign = 0, ovl = 0;
	for (i = 0; i < 512; i++) {
		uint8_t flgs = i & 0x1f;
		uint8_t cnd = (i >> 5) & 0xf;
		zero = !!(flgs & M32C_FLG_ZERO);
		carry = !!(flgs & M32C_FLG_CARRY);
		sign = !!(flgs & M32C_FLG_SIGN);
		ovl = !!(flgs & M32C_FLG_OVERFLOW);
		switch (cnd & 0xf) {
		    case 0:	/* LTU/NC */
			    result = !carry;
			    break;

		    case 1:	/* LEU */
			    result = !(carry && !zero);
			    break;

		    case 2:	/* NE/NZ  */
			    result = !zero;
			    break;

		    case 3:	/* PZ */
			    result = !sign;
			    break;

		    case 4:	/* NO */
			    result = !ovl;
			    break;

		    case 5:	/* GT */
			    result = !((sign ^ ovl) || zero);
			    break;

		    case 6:	/* GE */
			    result = !(sign ^ ovl);
			    break;

		    case 7:	/*  gibts net */
			    result = 1;
			    break;

		    case 8:
			    /* GEU/C */
			    result = carry;
			    break;

		    case 9:	/* GTU */
			    result = (carry & !zero);
			    break;

		    case 10:	/* EQ/Z */
			    result = zero;
			    break;

		    case 11:	/* N */
			    result = sign;
			    break;

		    case 12:	/* O */
			    result = ovl;
			    break;

		    case 13:	/* LE */
			    result = (sign ^ ovl) || zero;
			    break;

		    case 14:	/* LT */
			    result = sign ^ ovl;
			    break;

		    default:
			    result = 0;
			    break;
		}
		condition_map[i] = result;
	}
}

/**
 **************************************************************
 * \fn int check_condition(uint8_t cnd); 
 * Check if flags satisfy a condition. 
 * v1
 **************************************************************
 */
static inline int
check_condition(uint8_t cnd)
{
	int index = ((int)cnd << 5) | (M32C_REG_FLG & 0x1f);
	return condition_map[index];
}

/*
 * General addressing mode access procs
 */
static void
gam_set_r0l(uint32_t value, uint32_t index)
{
	M32C_REG_R0L = value;
}

static void
gam_get_r0l(uint32_t * value, uint32_t index)
{
	*value = M32C_REG_R0L;
}

static void
gam_set_r0(uint32_t value, uint32_t index)
{
	M32C_REG_R0 = value;
}

static void
gam_get_r0(uint32_t * value, uint32_t index)
{
	*value = M32C_REG_R0;
}

static void
gam_set_r2r0(uint32_t value, uint32_t index)
{
	M32C_REG_R2 = value >> 16;
	M32C_REG_R0 = value & 0xffff;
}

static void
gam_get_r2r0(uint32_t * value, uint32_t index)
{
	*value = M32C_REG_R0 | ((uint32_t) M32C_REG_R2 << 16);
}

static void
gam_set_r1l(uint32_t value, uint32_t index)
{
	M32C_REG_R1L = value;

}

static void
gam_get_r1l(uint32_t * value, uint32_t index)
{
	*value = M32C_REG_R1L;
}

static void
gam_set_r1(uint32_t value, uint32_t index)
{
	M32C_REG_R1 = value;
}

static void
gam_get_r1(uint32_t * value, uint32_t index)
{
	*value = M32C_REG_R1;
}

static void
gam_set_r3r1(uint32_t value, uint32_t index)
{
	M32C_REG_R3 = (value >> 16);
	M32C_REG_R1 = value & 0xffff;
}

static void
gam_get_r3r1(uint32_t * value, uint32_t index)
{
	*value = M32C_REG_R1 | ((uint32_t) M32C_REG_R3 << 16);
}

static void
gam_set_r0h(uint32_t value, uint32_t index)
{
	M32C_REG_R0H = value;
}

static void
gam_get_r0h(uint32_t * value, uint32_t index)
{
	*value = M32C_REG_R0H;
}

static void
gam_set_r2(uint32_t value, uint32_t index)
{
	M32C_REG_R2 = value;
}

static void
gam_get_r2(uint32_t * value, uint32_t index)
{
	*value = M32C_REG_R2;
}

static void
gam_set_r1h(uint32_t value, uint32_t index)
{
	M32C_REG_R1H = value;
}

static void
gam_get_r1h(uint32_t * value, uint32_t index)
{
	*value = M32C_REG_R1H;
}

static void
gam_set_r3(uint32_t value, uint32_t index)
{
	M32C_REG_R3 = value;
}

static void
gam_get_r3(uint32_t * value, uint32_t index)
{
	*value = M32C_REG_R3;
}

static void
gam_set_a0_size8(uint32_t value, uint32_t index)
{
	M32C_REG_A0 = value & 0xffff;	/* Yes this is ok ! */
}

static void
gam_set_a0_size16(uint32_t value, uint32_t index)
{
	M32C_REG_A0 = value & 0xffff;
}

static void
gam_set_a0_size32(uint32_t value, uint32_t index)
{
	M32C_REG_A0 = value & 0xffffff;
}

static void
gam_get_a0_size8(uint32_t * value, uint32_t index)
{
	*value = M32C_REG_A0 & 0xff;
}

static void
gam_get_a0_size16(uint32_t * value, uint32_t index)
{
	*value = M32C_REG_A0 & 0xffff;
}

static void
gam_get_a0_size32(uint32_t * value, uint32_t index)
{
	*value = M32C_REG_A0;
}

static void
gam_set_a1_size8(uint32_t value, uint32_t index)
{
	M32C_REG_A1 = value & 0xffff;	/* Yes this is ok ! */
}

static void
gam_set_a1_size16(uint32_t value, uint32_t index)
{
	M32C_REG_A1 = value & 0xffff;
}

static void
gam_set_a1_size32(uint32_t value, uint32_t index)
{
	M32C_REG_A1 = value & 0xffffff;
}

static void
gam_get_a1_size8(uint32_t * value, uint32_t index)
{
	*value = M32C_REG_A1 & 0xff;
}

static void
gam_get_a1_size16(uint32_t * value, uint32_t index)
{
	*value = M32C_REG_A1 & 0xffff;
}

static void
gam_get_a1_size32(uint32_t * value, uint32_t index)
{
	*value = M32C_REG_A1;
}

static void
gam_set_ia0_size8(uint32_t value, uint32_t index)
{
	uint32_t addr = M32C_REG_A0 + index;
	addr = addr & 0xffffff;
	M32C_Write8(value, addr);
}

static void
gam_set_ia0_size16(uint32_t value, uint32_t index)
{
	uint32_t addr = M32C_REG_A0 + index;
	addr = addr & 0xffffff;
	M32C_Write16(value, addr);
}

static void
gam_set_ia0_size32(uint32_t value, uint32_t index)
{
	uint32_t addr = M32C_REG_A0 + index;
	addr = addr & 0xffffff;
	M32C_Write32(value, addr);
}

static void
gam_get_ia0_size8(uint32_t * value, uint32_t index)
{
	uint32_t addr = M32C_REG_A0 + index;
	addr = addr & 0xffffff;
	*value = M32C_Read8(addr);
}

static void
gam_get_ia0_size16(uint32_t * value, uint32_t index)
{
	uint32_t addr = M32C_REG_A0 + index;
	addr = addr & 0xffffff;
	*value = M32C_Read16(addr);
}

static void
gam_get_ia0_size32(uint32_t * value, uint32_t index)
{
	uint32_t addr = M32C_REG_A0 + index;
	addr = addr & 0xffffff;
	*value = M32C_Read32(addr);
}

static void
gam_set_ia1_size8(uint32_t value, uint32_t index)
{
	uint32_t addr = M32C_REG_A1 + index;
	addr = addr & 0xffffff;
	M32C_Write8(value, addr);
}

static void
gam_set_ia1_size16(uint32_t value, uint32_t index)
{
	uint32_t addr = M32C_REG_A1 + index;
	addr = addr & 0xffffff;
	M32C_Write16(value, addr);
}

static void
gam_set_ia1_size32(uint32_t value, uint32_t index)
{
	uint32_t addr = M32C_REG_A1 + index;
	addr = addr & 0xffffff;
	M32C_Write32(value, addr);
}

static void
gam_get_ia1_size8(uint32_t * value, uint32_t index)
{
	uint32_t addr = M32C_REG_A1 + index;
	addr = addr & 0xffffff;
	*value = M32C_Read8(addr);
}

static void
gam_get_ia1_size16(uint32_t * value, uint32_t index)
{
	uint32_t addr = M32C_REG_A1 + index;
	addr = addr & 0xffffff;
	*value = M32C_Read16(addr);
}

static void
gam_get_ia1_size32(uint32_t * value, uint32_t index)
{
	uint32_t addr = M32C_REG_A1 + index;
	addr = addr & 0xffffff;
	*value = M32C_Read32(addr);
}

static void
gam_set_dsp8ia0_size8(uint32_t value, uint32_t index)
{
	uint32_t addr = M32C_REG_A0 + M32C_Read8(M32C_REG_PC) + index;
	addr = addr & 0xffffff;
	M32C_Write8(value, addr);
}

static void
gam_set_dsp8ia0_size16(uint32_t value, uint32_t index)
{
	uint32_t addr = M32C_REG_A0 + M32C_Read8(M32C_REG_PC) + index;
	addr = addr & 0xffffff;
	M32C_Write16(value, addr);
}

static void
gam_set_dsp8ia0_size32(uint32_t value, uint32_t index)
{
	uint32_t addr = M32C_REG_A0 + M32C_Read8(M32C_REG_PC) + index;
	addr = addr & 0xffffff;
	M32C_Write32(value, addr);
}

static void
gam_get_dsp8ia0_size8(uint32_t * value, uint32_t index)
{
	uint32_t addr = M32C_REG_A0 + M32C_Read8(M32C_REG_PC) + index;
	addr = addr & 0xffffff;
	*value = M32C_Read8(addr);
}

static void
gam_get_dsp8ia0_size16(uint32_t * value, uint32_t index)
{
	uint32_t addr = M32C_REG_A0 + M32C_Read8(M32C_REG_PC) + index;
	addr = addr & 0xffffff;
	*value = M32C_Read16(addr);
}

static void
gam_get_dsp8ia0_size32(uint32_t * value, uint32_t index)
{
	uint32_t addr = M32C_REG_A0 + M32C_Read8(M32C_REG_PC) + index;
	addr = addr & 0xffffff;
	*value = M32C_Read32(addr);
}

static void
gam_set_dsp8ia1_size8(uint32_t value, uint32_t index)
{
	uint32_t addr = M32C_REG_A1 + M32C_Read8(M32C_REG_PC) + index;
	addr = addr & 0xffffff;
	M32C_Write8(value, addr);
}

static void
gam_set_dsp8ia1_size16(uint32_t value, uint32_t index)
{
	uint32_t addr = M32C_REG_A1 + M32C_Read8(M32C_REG_PC) + index;
	addr = addr & 0xffffff;
	M32C_Write16(value, addr);
}

static void
gam_set_dsp8ia1_size32(uint32_t value, uint32_t index)
{
	uint32_t addr = M32C_REG_A1 + M32C_Read8(M32C_REG_PC) + index;
	addr = addr & 0xffffff;
	M32C_Write32(value, addr);
}

static void
gam_get_dsp8ia1_size8(uint32_t * value, uint32_t index)
{
	uint32_t addr = M32C_REG_A1 + M32C_Read8(M32C_REG_PC) + index;
	addr = addr & 0xffffff;
	*value = M32C_Read8(addr);
}

static void
gam_get_dsp8ia1_size16(uint32_t * value, uint32_t index)
{
	uint32_t addr = M32C_REG_A1 + M32C_Read8(M32C_REG_PC) + index;
	addr = addr & 0xffffff;
	*value = M32C_Read16(addr);
}

static void
gam_get_dsp8ia1_size32(uint32_t * value, uint32_t index)
{
	uint32_t addr = M32C_REG_A1 + M32C_Read8(M32C_REG_PC) + index;
	addr = addr & 0xffffff;
	*value = M32C_Read32(addr);
}

static void
gam_set_dsp8isb_size8(uint32_t value, uint32_t index)
{
	uint32_t addr = M32C_REG_SB + M32C_Read8(M32C_REG_PC) + index;
	addr = addr & 0xffffff;
	M32C_Write8(value, addr);
}

static void
gam_set_dsp8isb_size16(uint32_t value, uint32_t index)
{
	uint32_t addr = M32C_REG_SB + M32C_Read8(M32C_REG_PC) + index;
	addr = addr & 0xffffff;
	M32C_Write16(value, addr);
}

static void
gam_set_dsp8isb_size32(uint32_t value, uint32_t index)
{
	uint32_t addr = M32C_REG_SB + M32C_Read8(M32C_REG_PC) + index;
	addr = addr & 0xffffff;
	M32C_Write32(value, addr);
}

static void
gam_get_dsp8isb_size8(uint32_t * value, uint32_t index)
{
	uint32_t addr = M32C_REG_SB + M32C_Read8(M32C_REG_PC) + index;
	addr = addr & 0xffffff;
	*value = M32C_Read8(addr);
}

static void
gam_get_dsp8isb_size16(uint32_t * value, uint32_t index)
{
	uint32_t addr = M32C_REG_SB + M32C_Read8(M32C_REG_PC) + index;
	addr = addr & 0xffffff;
	*value = M32C_Read16(addr);
}

static void
gam_get_dsp8isb_size32(uint32_t * value, uint32_t index)
{
	uint32_t addr = M32C_REG_SB + M32C_Read8(M32C_REG_PC) + index;
	addr = addr & 0xffffff;
	*value = M32C_Read32(addr);
}

static void
gam_set_dsp8ifb_size8(uint32_t value, uint32_t index)
{
	int8_t dsp8 = M32C_Read8(M32C_REG_PC);
	uint32_t addr = M32C_REG_FB + dsp8 + index;
	addr = addr & 0xffffff;
	M32C_Write8(value, addr);
}

static void
gam_set_dsp8ifb_size16(uint32_t value, uint32_t index)
{
	int8_t dsp8 = M32C_Read8(M32C_REG_PC);
	uint32_t addr = M32C_REG_FB + dsp8 + index;
	addr = addr & 0xffffff;
	M32C_Write16(value, addr);
}

static void
gam_set_dsp8ifb_size32(uint32_t value, uint32_t index)
{
	int8_t dsp8 = M32C_Read8(M32C_REG_PC);
	uint32_t addr = M32C_REG_FB + dsp8 + index;
	addr = addr & 0xffffff;
	M32C_Write32(value, addr);
}

static void
gam_get_dsp8ifb_size8(uint32_t * value, uint32_t index)
{
	int8_t dsp8 = M32C_Read8(M32C_REG_PC);
	uint32_t addr = M32C_REG_FB + dsp8 + index;
	addr = addr & 0xffffff;
	*value = M32C_Read8(addr);
}

static void
gam_get_dsp8ifb_size16(uint32_t * value, uint32_t index)
{
	int8_t dsp8 = M32C_Read8(M32C_REG_PC);
	uint32_t addr = M32C_REG_FB + dsp8 + index;
	addr = addr & 0xffffff;
	*value = M32C_Read16(addr);
}

static void
gam_get_dsp8ifb_size32(uint32_t * value, uint32_t index)
{
	int8_t dsp8 = M32C_Read8(M32C_REG_PC);
	uint32_t addr = M32C_REG_FB + dsp8 + index;
	addr = addr & 0xffffff;
	*value = M32C_Read32(addr);
}

static void
gam_set_dsp16ia0_size8(uint32_t value, uint32_t index)
{
	uint32_t addr = M32C_REG_A0 + M32C_Read16(M32C_REG_PC) + index;
	addr = addr & 0xffffff;
	M32C_Write8(value, addr);
}

static void
gam_set_dsp16ia0_size16(uint32_t value, uint32_t index)
{
	uint32_t addr = M32C_REG_A0 + M32C_Read16(M32C_REG_PC) + index;
	addr = addr & 0xffffff;
	M32C_Write16(value, addr);
}

static void
gam_set_dsp16ia0_size32(uint32_t value, uint32_t index)
{
	uint32_t addr = M32C_REG_A0 + M32C_Read16(M32C_REG_PC) + index;
	addr = addr & 0xffffff;
	M32C_Write32(value, addr);
}

static void
gam_get_dsp16ia0_size8(uint32_t * value, uint32_t index)
{
	uint32_t addr = M32C_REG_A0 + M32C_Read16(M32C_REG_PC) + index;
	addr = addr & 0xffffff;
	*value = M32C_Read8(addr);
}

static void
gam_get_dsp16ia0_size16(uint32_t * value, uint32_t index)
{
	uint32_t addr = M32C_REG_A0 + M32C_Read16(M32C_REG_PC) + index;
	addr = addr & 0xffffff;
	*value = M32C_Read16(addr);
}

static void
gam_get_dsp16ia0_size32(uint32_t * value, uint32_t index)
{
	uint32_t addr = M32C_REG_A0 + M32C_Read16(M32C_REG_PC) + index;
	addr = addr & 0xffffff;
	*value = M32C_Read32(addr);
}

static void
gam_set_dsp16ia1_size8(uint32_t value, uint32_t index)
{
	uint32_t addr = M32C_REG_A1 + M32C_Read16(M32C_REG_PC) + index;
	addr = addr & 0xffffff;
	M32C_Write8(value, addr);
}

static void
gam_set_dsp16ia1_size16(uint32_t value, uint32_t index)
{
	uint32_t addr = M32C_REG_A1 + M32C_Read16(M32C_REG_PC) + index;
	addr = addr & 0xffffff;
	M32C_Write16(value, addr);
}

static void
gam_set_dsp16ia1_size32(uint32_t value, uint32_t index)
{
	uint32_t addr = M32C_REG_A1 + M32C_Read16(M32C_REG_PC) + index;
	addr = addr & 0xffffff;
	M32C_Write32(value, addr);
}

static void
gam_get_dsp16ia1_size8(uint32_t * value, uint32_t index)
{
	uint32_t addr = M32C_REG_A1 + M32C_Read16(M32C_REG_PC) + index;
	addr = addr & 0xffffff;
	*value = M32C_Read8(addr);
}

static void
gam_get_dsp16ia1_size16(uint32_t * value, uint32_t index)
{
	uint32_t addr = M32C_REG_A1 + M32C_Read16(M32C_REG_PC) + index;
	addr = addr & 0xffffff;
	*value = M32C_Read16(addr);
}

static void
gam_get_dsp16ia1_size32(uint32_t * value, uint32_t index)
{
	uint32_t addr = M32C_REG_A1 + M32C_Read16(M32C_REG_PC) + index;
	addr = addr & 0xffffff;
	*value = M32C_Read32(addr);
}

static void
gam_set_dsp16isb_size8(uint32_t value, uint32_t index)
{
	uint32_t addr = M32C_REG_SB + M32C_Read16(M32C_REG_PC) + index;
	addr = addr & 0xffffff;
	M32C_Write8(value, addr);
}

static void
gam_set_dsp16isb_size16(uint32_t value, uint32_t index)
{
	uint32_t addr = M32C_REG_SB + M32C_Read16(M32C_REG_PC) + index;
	addr = addr & 0xffffff;
	M32C_Write16(value, addr);
}

static void
gam_set_dsp16isb_size32(uint32_t value, uint32_t index)
{
	uint32_t addr = M32C_REG_SB + M32C_Read16(M32C_REG_PC) + index;
	addr = addr & 0xffffff;
	M32C_Write32(value, addr);
}

static void
gam_get_dsp16isb_size8(uint32_t * value, uint32_t index)
{
	uint32_t addr = M32C_REG_SB + M32C_Read16(M32C_REG_PC) + index;
	addr = addr & 0xffffff;
	*value = M32C_Read8(addr);
}

static void
gam_get_dsp16isb_size16(uint32_t * value, uint32_t index)
{
	uint32_t addr = M32C_REG_SB + M32C_Read16(M32C_REG_PC) + index;
	addr = addr & 0xffffff;
	*value = M32C_Read16(addr);
}

static void
gam_get_dsp16isb_size32(uint32_t * value, uint32_t index)
{
	uint32_t addr = M32C_REG_SB + M32C_Read16(M32C_REG_PC) + index;
	addr = addr & 0xffffff;
	*value = M32C_Read32(addr);
}

static void
gam_set_dsp16ifb_size8(uint32_t value, uint32_t index)
{
	int16_t dsp16 = M32C_Read16(M32C_REG_PC);
	uint32_t addr = M32C_REG_FB + dsp16 + index;
	addr = addr & 0xffffff;
	M32C_Write8(value, addr);
}

static void
gam_set_dsp16ifb_size16(uint32_t value, uint32_t index)
{
	int16_t dsp16 = M32C_Read16(M32C_REG_PC);
	uint32_t addr = M32C_REG_FB + dsp16 + index;
	addr = addr & 0xffffff;
	M32C_Write16(value, addr);
}

static void
gam_set_dsp16ifb_size32(uint32_t value, uint32_t index)
{
	int16_t dsp16 = M32C_Read16(M32C_REG_PC);
	uint32_t addr = M32C_REG_FB + dsp16 + index;
	addr = addr & 0xffffff;
	M32C_Write32(value, addr);
}

static void
gam_get_dsp16ifb_size8(uint32_t * value, uint32_t index)
{
	int16_t dsp16 = M32C_Read16(M32C_REG_PC);
	uint32_t addr = M32C_REG_FB + dsp16 + index;
	addr = addr & 0xffffff;
	*value = M32C_Read8(addr);
}

static void
gam_get_dsp16ifb_size16(uint32_t * value, uint32_t index)
{
	int16_t dsp16 = M32C_Read16(M32C_REG_PC);
	uint32_t addr = M32C_REG_FB + dsp16 + index;
	addr = addr & 0xffffff;
	*value = M32C_Read16(addr);
}

static void
gam_get_dsp16ifb_size32(uint32_t * value, uint32_t index)
{
	int16_t dsp16 = M32C_Read16(M32C_REG_PC);
	uint32_t addr = M32C_REG_FB + dsp16 + index;
	addr = addr & 0xffffff;
	*value = M32C_Read32(addr);
}

static void
gam_set_dsp24ia0_size8(uint32_t value, uint32_t index)
{
	uint32_t addr = M32C_REG_A0 + M32C_Read24(M32C_REG_PC) + index;
	addr = addr & 0xffffff;
	M32C_Write8(value, addr);
}

static void
gam_set_dsp24ia0_size16(uint32_t value, uint32_t index)
{
	uint32_t addr = M32C_REG_A0 + M32C_Read24(M32C_REG_PC) + index;
	addr = addr & 0xffffff;
	M32C_Write16(value, addr);
}

static void
gam_set_dsp24ia0_size32(uint32_t value, uint32_t index)
{
	uint32_t addr = M32C_REG_A0 + M32C_Read24(M32C_REG_PC) + index;
	addr = addr & 0xffffff;
	M32C_Write32(value, addr);
}

static void
gam_get_dsp24ia0_size8(uint32_t * value, uint32_t index)
{
	uint32_t addr = M32C_REG_A0 + M32C_Read24(M32C_REG_PC) + index;
	addr = addr & 0xffffff;
	*value = M32C_Read8(addr);
}

static void
gam_get_dsp24ia0_size16(uint32_t * value, uint32_t index)
{
	uint32_t addr = M32C_REG_A0 + M32C_Read24(M32C_REG_PC) + index;
	addr = addr & 0xffffff;
	*value = M32C_Read16(addr);
}

static void
gam_get_dsp24ia0_size32(uint32_t * value, uint32_t index)
{
	uint32_t addr = M32C_REG_A0 + M32C_Read24(M32C_REG_PC) + index;
	addr = addr & 0xffffff;
	*value = M32C_Read32(addr);
}

static void
gam_set_dsp24ia1_size8(uint32_t value, uint32_t index)
{
	uint32_t addr = M32C_REG_A1 + M32C_Read24(M32C_REG_PC) + index;
	addr = addr & 0xffffff;
	M32C_Write8(value, addr);
}

static void
gam_set_dsp24ia1_size16(uint32_t value, uint32_t index)
{
	uint32_t addr = M32C_REG_A1 + M32C_Read24(M32C_REG_PC) + index;
	addr = addr & 0xffffff;
	M32C_Write16(value, addr);
}

static void
gam_set_dsp24ia1_size32(uint32_t value, uint32_t index)
{
	uint32_t addr = M32C_REG_A1 + M32C_Read24(M32C_REG_PC) + index;
	addr = addr & 0xffffff;
	M32C_Write32(value, addr);
}

static void
gam_get_dsp24ia1_size8(uint32_t * value, uint32_t index)
{
	uint32_t addr = M32C_REG_A1 + M32C_Read24(M32C_REG_PC) + index;
	addr = addr & 0xffffff;
	*value = M32C_Read8(addr);
}

static void
gam_get_dsp24ia1_size16(uint32_t * value, uint32_t index)
{
	uint32_t addr = M32C_REG_A1 + M32C_Read24(M32C_REG_PC) + index;
	addr = addr & 0xffffff;
	*value = M32C_Read16(addr);
}

static void
gam_get_dsp24ia1_size32(uint32_t * value, uint32_t index)
{
	uint32_t addr = M32C_REG_A1 + M32C_Read24(M32C_REG_PC) + index;
	addr = addr & 0xffffff;
	*value = M32C_Read32(addr);
}

static void
gam_set_abs16_size8(uint32_t value, uint32_t index)
{
	uint32_t addr = M32C_Read16(M32C_REG_PC) + index;
	if (addr & 0xff0000) {
		fprintf(stderr, "unverified addressing mode in %d\n", __LINE__);
		exit(1);
	}
	addr = addr & 0xffff;	/* ??????????????????? */
	M32C_Write8(value, addr);
}

static void
gam_set_abs16_size16(uint32_t value, uint32_t index)
{
	uint32_t addr = M32C_Read16(M32C_REG_PC) + index;
	if (addr & 0xff0000) {
		fprintf(stderr, "unverified addressing mode in %d\n", __LINE__);
		exit(1);
	}
	addr = addr & 0xffff;	/* ??????????????????? */
	M32C_Write16(value, addr);
}

static void
gam_set_abs16_size32(uint32_t value, uint32_t index)
{
	uint32_t addr = M32C_Read16(M32C_REG_PC) + index;
	if (addr & 0xff0000) {
		fprintf(stderr, "unverified addressing mode in %d\n", __LINE__);
		exit(1);
	}
	addr = addr & 0xffff;	/* ??????????????????? */
	M32C_Write32(value, addr);
}

static void
gam_get_abs16_size8(uint32_t * value, uint32_t index)
{
	uint32_t addr = M32C_Read16(M32C_REG_PC) + index;
	if (addr & 0xff0000) {
		fprintf(stderr, "unverified addressing mode in %d\n", __LINE__);
		exit(1);
	}
	addr = addr & 0xffff;	/* ????????????????? */
	*value = M32C_Read8(addr);
}

static void
gam_get_abs16_size16(uint32_t * value, uint32_t index)
{
	uint32_t addr = M32C_Read16(M32C_REG_PC) + index;
	if (addr & 0xff0000) {
		fprintf(stderr, "unverified addressing mode in %d\n", __LINE__);
		exit(1);
	}
	addr = addr & 0xffff;	/* ????????????????? */
	*value = M32C_Read16(addr);
}

static void
gam_get_abs16_size32(uint32_t * value, uint32_t index)
{
	uint32_t addr = M32C_Read16(M32C_REG_PC) + index;
	if (addr & 0xff0000) {
		fprintf(stderr, "unverified addressing mode in %d\n", __LINE__);
		exit(1);
	}
	addr = addr & 0xffff;	/* ????????????????? */
	*value = M32C_Read32(addr);
}

static void
gam_set_abs24_size8(uint32_t value, uint32_t index)
{
	uint32_t addr = M32C_Read24(M32C_REG_PC) + index;
	addr = addr & 0xffffff;
	M32C_Write8(value, addr);
}

static void
gam_set_abs24_size16(uint32_t value, uint32_t index)
{
	uint32_t addr = M32C_Read24(M32C_REG_PC) + index;
	addr = addr & 0xffffff;
	M32C_Write16(value, addr);
}

static void
gam_set_abs24_size32(uint32_t value, uint32_t index)
{
	uint32_t addr = M32C_Read24(M32C_REG_PC) + index;
	addr = addr & 0xffffff;
	M32C_Write32(value, addr);
}

static void
gam_get_abs24_size8(uint32_t * value, uint32_t index)
{
	uint32_t addr = M32C_Read24(M32C_REG_PC) + index;
	addr = addr & 0xffffff;
	*value = M32C_Read8(addr);
}

static void
gam_get_abs24_size16(uint32_t * value, uint32_t index)
{
	uint32_t addr = M32C_Read24(M32C_REG_PC) + index;
	addr = addr & 0xffffff;
	*value = M32C_Read16(addr);
}

static void
gam_get_abs24_size32(uint32_t * value, uint32_t index)
{
	uint32_t addr = M32C_Read24(M32C_REG_PC) + index;
	addr = addr & 0xffffff;
	*value = M32C_Read32(addr);
}

static void
gam_set_bad(uint32_t value, uint32_t index)
{
	fprintf(stderr, "Illegal addressing mode\n");
	exit(1);
}

static void
gam_get_bad(uint32_t * value, uint32_t index)
{
	fprintf(stderr, "Illegal addressing mode\n");
	exit(1);
}

static uint32_t
general_am_efa(int am, int *codelen, uint32_t existence_map)
{
	uint32_t addr;
	if (((existence_map >> am) & 1) == 0) {
		fprintf(stderr, "Nonexisting AM. Possible idecoder bug\n");
		exit(1);
	}
	switch (am) {
	    case 0x00:
		    *codelen = 0;
		    return M32C_REG_A0;

	    case 0x01:
		    *codelen = 0;
		    return M32C_REG_A1;

	    case 0x04:
		    *codelen = 1;
		    addr = M32C_REG_A0 + M32C_Read8(M32C_REG_PC);
		    addr = addr & 0xffffff;
		    return addr;

	    case 0x05:
		    *codelen = 1;
		    addr = M32C_REG_A1 + M32C_Read8(M32C_REG_PC);
		    addr = addr & 0xffffff;
		    return addr;

	    case 0x06:
		    *codelen = 1;
		    addr = M32C_REG_SB + M32C_Read8(M32C_REG_PC);
		    addr = addr & 0xffffff;
		    return addr;

	    case 0x07:		/* dsp8[fb] */
		    *codelen = 1;
		    addr = M32C_REG_FB + (int8_t) M32C_Read8(M32C_REG_PC);
		    addr = addr & 0xffffff;
		    return addr;

	    case 0x08:
		    *codelen = 2;
		    addr = M32C_REG_A0 + M32C_Read16(M32C_REG_PC);
		    addr = addr & 0xffffff;
		    return addr;

	    case 0x09:
		    *codelen = 2;
		    addr = M32C_REG_A1 + M32C_Read16(M32C_REG_PC);
		    addr = addr & 0xffffff;
		    return addr;

	    case 0x0a:
		    *codelen = 2;
		    addr = M32C_REG_SB + M32C_Read16(M32C_REG_PC);
		    addr = addr & 0xffffff;
		    return addr;

	    case 0x0b:		/* dsp16[fb] */
		    *codelen = 2;
		    addr = M32C_REG_FB + (int16_t) M32C_Read16(M32C_REG_PC);
		    addr = addr & 0xffffff;
		    return addr;

	    case 0x0c:
		    *codelen = 3;
		    addr = M32C_REG_A0 + M32C_Read24(M32C_REG_PC);
		    addr = addr & 0xffffff;
		    return addr;

	    case 0x0d:
		    *codelen = 3;
		    addr = M32C_REG_A1 + M32C_Read24(M32C_REG_PC);
		    addr = addr & 0xffffff;
		    return addr;

	    case 0x0f:
		    *codelen = 2;
		    addr = M32C_Read16(M32C_REG_PC);
		    return addr;

	    case 0x0e:
		    *codelen = 3;
		    addr = M32C_Read24(M32C_REG_PC);
		    return addr;

	    default:
		    *codelen = 0;
		    fprintf(stderr, "Illegal addressing mode %d in EFA calculation\n", am);
		    return 0;
	}
}

/* review pointer */
static GAM_GetProc *
general_am_get(int am, int size, int *codelen, uint32_t existence_map)
{
	if (((existence_map >> am) & 1) == 0) {
		fprintf(stderr, "Nonexisting AM. Possible idecoder bug\n");
		exit(1);
	}
	switch (am) {
	    case 0x12:
		    *codelen = 0;
		    if (size == 1) {
			    return gam_get_r0l;
		    } else if (size == 2) {
			    return gam_get_r0;
		    } else if (size == 4) {
			    return gam_get_r2r0;
		    } else {
			    fprintf(stderr, "Emulator bug: Bad size in GAM\n");
			    exit(1);
		    }
		    break;

	    case 0x13:
		    *codelen = 0;
		    if (size == 1) {
			    return gam_get_r1l;
		    } else if (size == 2) {
			    return gam_get_r1;
		    } else if (size == 4) {
			    return gam_get_r3r1;
		    } else {
			    fprintf(stderr, "Emulator bug: Bad size in GAM\n");
			    exit(1);
		    }
		    break;

	    case 0x10:
		    *codelen = 0;
		    if (size == 1) {
			    return gam_get_r0h;
		    } else if (size == 2) {
			    return gam_get_r2;
		    } else if (size == 4) {
			    fprintf(stderr, "Illegal 4 Byte addressing mode\n");
		    } else {
			    fprintf(stderr, "Emulator bug: Bad size in GAM\n");
			    exit(1);
		    }
		    break;

	    case 0x11:
		    *codelen = 0;
		    if (size == 1) {
			    return gam_get_r1h;
		    } else if (size == 2) {
			    return gam_get_r3;
		    } else if (size == 4) {
			    dbgprintf("Illegal 4 Byte addressing mode\n");
		    } else {
			    fprintf(stderr, "Emulator bug: Bad size in GAM\n");
			    exit(1);
		    }
		    break;

	    case 0x02:
		    *codelen = 0;
		    switch (size) {
			case 1:
				return gam_get_a0_size8;
			case 2:
				return gam_get_a0_size16;
			case 4:
				return gam_get_a0_size32;
		    }
		    break;

	    case 0x03:
		    *codelen = 0;
		    switch (size) {
			case 1:
				return gam_get_a1_size8;
			case 2:
				return gam_get_a1_size16;
			case 4:
				return gam_get_a1_size32;
		    }
		    break;

	    case 0x00:
		    *codelen = 0;
		    switch (size) {
			case 1:
				return gam_get_ia0_size8;
			case 2:
				return gam_get_ia0_size16;
			case 4:
				return gam_get_ia0_size32;
		    }
		    break;

	    case 0x01:
		    *codelen = 0;
		    switch (size) {
			case 1:
				return gam_get_ia1_size8;
			case 2:
				return gam_get_ia1_size16;
			case 4:
				return gam_get_ia1_size32;
		    }
		    break;

	    case 0x04:
		    *codelen = 1;
		    switch (size) {
			case 1:
				return gam_get_dsp8ia0_size8;
			case 2:
				return gam_get_dsp8ia0_size16;
			case 4:
				return gam_get_dsp8ia0_size32;
		    }
		    break;

	    case 0x05:
		    *codelen = 1;
		    switch (size) {
			case 1:
				return gam_get_dsp8ia1_size8;
			case 2:
				return gam_get_dsp8ia1_size16;
			case 4:
				return gam_get_dsp8ia1_size32;
		    }
		    break;

	    case 0x06:
		    *codelen = 1;
		    switch (size) {
			case 1:
				return gam_get_dsp8isb_size8;
			case 2:
				return gam_get_dsp8isb_size16;
			case 4:
				return gam_get_dsp8isb_size32;
		    }
		    break;

	    case 0x07:
		    *codelen = 1;
		    switch (size) {
			case 1:
				return gam_get_dsp8ifb_size8;
			case 2:
				return gam_get_dsp8ifb_size16;
			case 4:
				return gam_get_dsp8ifb_size32;
		    }
		    break;

	    case 0x08:
		    *codelen = 2;
		    switch (size) {
			case 1:
				return gam_get_dsp16ia0_size8;
			case 2:
				return gam_get_dsp16ia0_size16;
			case 4:
				return gam_get_dsp16ia0_size32;
		    }
		    break;

	    case 0x09:
		    *codelen = 2;
		    switch (size) {
			case 1:
				return gam_get_dsp16ia1_size8;
			case 2:
				return gam_get_dsp16ia1_size16;
			case 4:
				return gam_get_dsp16ia1_size32;
		    }
		    break;

	    case 0x0a:
		    *codelen = 2;
		    switch (size) {
			case 1:
				return gam_get_dsp16isb_size8;
			case 2:
				return gam_get_dsp16isb_size16;
			case 4:
				return gam_get_dsp16isb_size32;
		    }
		    break;

	    case 0x0b:
		    *codelen = 2;
		    switch (size) {
			case 1:
				return gam_get_dsp16ifb_size8;
			case 2:
				return gam_get_dsp16ifb_size16;
			case 4:
				return gam_get_dsp16ifb_size32;
		    }
		    break;

	    case 0x0c:
		    *codelen = 3;
		    switch (size) {
			case 1:
				return gam_get_dsp24ia0_size8;
			case 2:
				return gam_get_dsp24ia0_size16;
			case 4:
				return gam_get_dsp24ia0_size32;
		    }
		    break;

	    case 0x0d:
		    *codelen = 3;
		    switch (size) {
			case 1:
				return gam_get_dsp24ia1_size8;
			case 2:
				return gam_get_dsp24ia1_size16;
			case 4:
				return gam_get_dsp24ia1_size32;
		    }
		    break;

	    case 0x0f:
		    *codelen = 2;
		    switch (size) {
			case 1:
				return gam_get_abs16_size8;
			case 2:
				return gam_get_abs16_size16;
			case 4:
				return gam_get_abs16_size32;
		    }
		    break;

	    case 0x0e:
		    *codelen = 3;
		    switch (size) {
			case 1:
				return gam_get_abs24_size8;
			case 2:
				return gam_get_abs24_size16;
			case 4:
				return gam_get_abs24_size32;
		    }
		    break;

	    default:
		    fprintf(stderr, "Illegal addressing mode at %06x\n", M32C_REG_PC);
		    return gam_get_bad;
	}
	return gam_get_bad;
}

static GAM_SetProc *
general_am_set(int am, int size, int *codelen, uint32_t existence_map)
{
	if (((existence_map >> am) & 1) == 0) {
		fprintf(stderr, "Nonexisting AM. Possible idecoder bug\n");
		exit(1);
	}
	switch (am) {
	    case 0x12:
		    *codelen = 0;
		    if (size == 1) {
			    return gam_set_r0l;
		    } else if (size == 2) {
			    return gam_set_r0;
		    } else if (size == 4) {
			    return gam_set_r2r0;
		    } else {
			    fprintf(stderr, "Emulator bug: Bad size in GAM\n");
			    exit(1);
		    }
		    break;

	    case 0x13:
		    *codelen = 0;
		    if (size == 1) {
			    return gam_set_r1l;
		    } else if (size == 2) {
			    return gam_set_r1;
		    } else if (size == 4) {
			    return gam_set_r3r1;
		    } else {
			    fprintf(stderr, "Emulator bug: Bad size in GAM\n");
			    exit(1);
		    }
		    break;

	    case 0x10:
		    *codelen = 0;
		    if (size == 1) {
			    return gam_set_r0h;
		    } else if (size == 2) {
			    return gam_set_r2;
		    } else if (size == 4) {
			    dbgprintf("Bad 4 Byte addressing mode\n");
		    } else {
			    fprintf(stderr, "Emulator bug: Bad size in GAM\n");
			    exit(1);
		    }
		    break;

	    case 0x11:
		    *codelen = 0;
		    if (size == 1) {
			    return gam_set_r1h;
		    } else if (size == 2) {
			    return gam_set_r3;
		    } else if (size == 4) {
			    dbgprintf("Bad 4 Byte addressing mode\n");
		    } else {
			    fprintf(stderr, "Emulator bug: Bad size in GAM\n");
			    exit(1);
		    }
		    break;

	    case 0x02:
		    *codelen = 0;
		    switch (size) {
			case 1:
				return gam_set_a0_size8;
			case 2:
				return gam_set_a0_size16;
			case 4:
				return gam_set_a0_size32;
		    }
		    break;

	    case 0x03:
		    *codelen = 0;
		    switch (size) {
			case 1:
				return gam_set_a1_size8;
			case 2:
				return gam_set_a1_size16;
			case 4:
				return gam_set_a1_size32;
		    }
		    break;

	    case 0x00:
		    *codelen = 0;
		    switch (size) {
			case 1:
				return gam_set_ia0_size8;
			case 2:
				return gam_set_ia0_size16;
			case 4:
				return gam_set_ia0_size32;
		    }
		    break;

	    case 0x01:
		    *codelen = 0;
		    switch (size) {
			case 1:
				return gam_set_ia1_size8;
			case 2:
				return gam_set_ia1_size16;
			case 4:
				return gam_set_ia1_size32;
		    }
		    break;

	    case 0x04:
		    *codelen = 1;
		    switch (size) {
			case 1:
				return gam_set_dsp8ia0_size8;
			case 2:
				return gam_set_dsp8ia0_size16;
			case 4:
				return gam_set_dsp8ia0_size32;
		    }
		    break;

	    case 0x05:
		    *codelen = 1;
		    switch (size) {
			case 1:
				return gam_set_dsp8ia1_size8;
			case 2:
				return gam_set_dsp8ia1_size16;
			case 4:
				return gam_set_dsp8ia1_size32;
		    }
		    break;

	    case 0x06:
		    *codelen = 1;
		    switch (size) {
			case 1:
				return gam_set_dsp8isb_size8;
			case 2:
				return gam_set_dsp8isb_size16;
			case 4:
				return gam_set_dsp8isb_size32;
		    }
		    break;

	    case 0x07:
		    *codelen = 1;
		    switch (size) {
			case 1:
				return gam_set_dsp8ifb_size8;
			case 2:
				return gam_set_dsp8ifb_size16;
			case 4:
				return gam_set_dsp8ifb_size32;
		    }
		    break;

	    case 0x08:
		    *codelen = 2;
		    switch (size) {
			case 1:
				return gam_set_dsp16ia0_size8;
			case 2:
				return gam_set_dsp16ia0_size16;
			case 4:
				return gam_set_dsp16ia0_size32;
		    }
		    break;

	    case 0x09:
		    *codelen = 2;
		    switch (size) {
			case 1:
				return gam_set_dsp16ia1_size8;
			case 2:
				return gam_set_dsp16ia1_size16;
			case 4:
				return gam_set_dsp16ia1_size32;
		    }
		    break;

	    case 0x0a:
		    *codelen = 2;
		    switch (size) {
			case 1:
				return gam_set_dsp16isb_size8;
			case 2:
				return gam_set_dsp16isb_size16;
			case 4:
				return gam_set_dsp16isb_size32;
		    }
		    break;

	    case 0x0b:
		    *codelen = 2;
		    switch (size) {
			case 1:
				return gam_set_dsp16ifb_size8;
			case 2:
				return gam_set_dsp16ifb_size16;
			case 4:
				return gam_set_dsp16ifb_size32;
		    }
		    break;

	    case 0x0c:
		    *codelen = 3;
		    switch (size) {
			case 1:
				return gam_set_dsp24ia0_size8;
			case 2:
				return gam_set_dsp24ia0_size16;
			case 4:
				return gam_set_dsp24ia0_size32;
		    }
		    break;

	    case 0x0d:
		    *codelen = 3;
		    switch (size) {
			case 1:
				return gam_set_dsp24ia1_size8;
			case 2:
				return gam_set_dsp24ia1_size16;
			case 4:
				return gam_set_dsp24ia1_size32;
		    }
		    break;

	    case 0x0f:
		    *codelen = 2;
		    switch (size) {
			case 1:
				return gam_set_abs16_size8;
			case 2:
				return gam_set_abs16_size16;
			case 4:
				return gam_set_abs16_size32;
		    }
		    break;

	    case 0x0e:
		    *codelen = 3;
		    switch (size) {
			case 1:
				return gam_set_abs24_size8;
			case 2:
				return gam_set_abs24_size16;
			case 4:
				return gam_set_abs24_size32;
		    }
		    break;

	    default:
		    *codelen = 0;
		    fprintf(stderr, "Illegal addressing mode at %06x\n", M32C_REG_PC);
		    return gam_set_bad;
	}
	return gam_set_bad;
}

static void
setreg_cdi16(unsigned int am, uint16_t value, uint8_t exist_map)
{
	switch (am) {
	    case 0:
		    M32C_REG(dct0) = value;
		    break;
	    case 1:
		    M32C_REG(dct1) = value;
		    break;
	    case 2:
		    //fprintf(stderr,"Set reg flg to %04x\n",value);
		    M32C_SET_REG_FLG(value);
		    break;
	    case 3:
		    M32C_REG(svf) = value;
		    break;
	    case 4:
		    M32C_REG(drc0) = value;
		    break;
	    case 5:
		    M32C_REG(drc1) = value;
		    break;
	    case 6:
		    M32C_REG(dmd0) = value;
		    break;
	    case 7:
		    M32C_REG(dmd1) = value;
		    break;
	    default:
		    fprintf(stderr, "Bug: CDI16 Addr. mode with reg > 7\n");
		    exit(1);
		    break;
	}
}

static void
setreg_cdi24low(unsigned int am, uint32_t value, uint8_t exist_map)
{
	switch (am) {
	    case 0:
		    M32C_REG(intb) = value;
		    break;
	    case 1:
		    M32C_REG(sp) = value;
		    break;
	    case 2:
		    M32C_BANKREG(sb) = value;
		    break;
	    case 3:
		    M32C_BANKREG(fb) = value;
		    break;
	    case 4:
		    M32C_REG(svp) = value;
		    break;
	    case 5:
		    M32C_REG(vct) = value;
		    break;
	    case 6:
		    dbgprintf("Illegal CDI24 AM\n");
		    break;
	    case 7:
		    M32C_SET_REG_ISP(value);
		    break;
	    default:
		    fprintf(stderr, "Bug: CDI24 Addr. mode with reg > 7\n");
		    exit(1);
		    break;
	}
}

static void
setreg_cdi24high(unsigned int am, uint32_t value, uint8_t exist_map)
{
	switch (am) {
	    case 0:
		    dbgprintf("Illegal CDI24 AM\n");
		    break;
	    case 1:
		    dbgprintf("Illegal CDI24 AM\n");
		    break;
	    case 2:
		    M32C_REG(dma0) = value;
		    break;
	    case 3:
		    M32C_REG(dma1) = value;
		    break;
	    case 4:
		    M32C_REG(dra0) = value;
		    break;
	    case 5:
		    M32C_REG(dra1) = value;
		    break;
	    case 6:
		    M32C_REG(dsa0) = value;
		    break;
	    case 7:
		    M32C_REG(dsa1) = value;
		    break;
	    default:
		    fprintf(stderr, "Bug: CDI24 Addr. mode with reg > 7\n");
		    exit(1);
		    break;
	}
}

static void
getreg_cdi16(unsigned int am, uint16_t * value, uint8_t exist_map)
{
	switch (am) {
	    case 0:
		    *value = M32C_REG(dct0);
		    break;
	    case 1:
		    *value = M32C_REG(dct1);
		    break;
	    case 2:
		    *value = M32C_REG(flg);
		    //fprintf(stderr,"Got flg reg: %04x at %08x\n",*value, M32C_REG_PC);
		    break;
	    case 3:
		    *value = M32C_REG(svf);
		    break;
	    case 4:
		    *value = M32C_REG(drc0);
		    break;
	    case 5:
		    *value = M32C_REG(drc1);
		    break;
	    case 6:
		    *value = M32C_REG(dmd0);
		    break;
	    case 7:
		    *value = M32C_REG(dmd1);
		    break;
	    default:
		    fprintf(stderr, "Bug: CDI16 Addr. mode with reg > 7\n");
		    exit(1);
		    break;
	}
}

static void
getreg_cdi24low(unsigned int am, uint32_t * value, uint8_t exist_map)
{
	switch (am) {
	    case 0:
		    *value = M32C_REG(intb);
		    break;
	    case 1:
		    *value = M32C_REG(sp);
		    break;
	    case 2:
		    *value = M32C_BANKREG(sb);
		    break;
	    case 3:
		    *value = M32C_BANKREG(fb);
		    break;
	    case 4:
		    *value = M32C_REG(svp);
		    break;
	    case 5:
		    *value = M32C_REG(vct);
		    break;
	    case 6:
		    dbgprintf("Illegal CDI24 AM\n");
		    *value = 0;
		    break;
	    case 7:
		    *value = M32C_REG(isp);
		    break;
	    default:
		    fprintf(stderr, "Bug: CDI24 Addr. mode with reg > 7\n");
		    exit(1);
		    break;
	}
}

static void
getreg_cdi24high(unsigned int am, uint32_t * value, uint8_t exist_map)
{
	switch (am) {
	    case 0:
		    dbgprintf("Illegal CDI24 AM\n");
		    *value = 0;
		    break;
	    case 1:
		    dbgprintf("Illegal CDI24 AM\n");
		    *value = 0;
		    break;
	    case 2:
		    *value = M32C_REG(dma0);
		    break;
	    case 3:
		    *value = M32C_REG(dma1);
		    break;
	    case 4:
		    *value = M32C_REG(dra0);
		    break;
	    case 5:
		    *value = M32C_REG(dra1);
		    break;
	    case 6:
		    *value = M32C_REG(dsa0);
		    break;
	    case 7:
		    *value = M32C_REG(dsa1);
		    break;
	    default:
		    fprintf(stderr, "Bug: CDI24 Addr. mode with reg > 7\n");
		    exit(1);
		    break;
	}
}

#if 0
static int
am2bit_codelen(int am)
{
	switch (am) {
	    case 0:
		    return 0;
	    case 1:
		    return 2;
	    case 2:
	    case 3:
		    return 1;
	    default:
		    return 0;
	}
}
#endif

#if 0
static void
am2bit_set(int am, int *codelen, int datalen, uint32_t value, uint32_t index)
{
	uint32_t addr;
	int8_t dsp8_s;
	uint8_t dsp8_u;
	switch (am) {
	    case 0:
		    if (datalen == 1) {
			    M32C_REG_R0L = value;
		    } else if (datalen == 2) {
			    M32C_REG_R0 = value;
		    }
		    *codelen = 0;
		    break;
	    case 1:		/* ABS 16 */
		    addr = M32C_Read16(M32C_REG_PC) + index;
		    if (datalen == 1) {
			    M32C_Write8(value, addr);
		    } else if (datalen == 2) {
			    M32C_Write16(value, addr);
		    } else if (datalen == 4) {
			    M32C_Write32(value, addr);
		    }
		    *codelen = 2;
		    break;
	    case 2:		/* dsp:8[sb] */
		    dsp8_u = M32C_Read8(M32C_REG_PC);
		    addr = (M32C_REG_SB + dsp8_u + index) & 0xffffff;
		    if (datalen == 1) {
			    M32C_Write8(value, addr);
		    } else if (datalen == 2) {
			    M32C_Write16(value, addr);
		    } else if (datalen == 4) {
			    M32C_Write32(value, addr);
		    }
		    *codelen = 1;
		    break;

	    case 3:		/* dsp:8[fb] */
		    dsp8_s = M32C_Read8(M32C_REG_PC);
		    addr = (M32C_REG_FB + dsp8_s + index) & 0xffffff;
		    if (datalen == 1) {
			    M32C_Write8(value, addr);
		    } else if (datalen == 2) {
			    M32C_Write16(value, addr);
		    } else if (datalen == 4) {
			    M32C_Write32(value, addr);
		    }
		    *codelen = 1;
		    break;
	    default:
		    *codelen = 0;
	}
}
#endif

#if 0
static void
am2bit_get(int am, int *codelen, int datalen, uint32_t * value, uint32_t index)
{
	uint32_t addr;
	int8_t dsp8_s;
	uint8_t dsp8_u;
	switch (am) {
	    case 0:
		    if (datalen == 1) {
			    *value = M32C_REG_R0L;
		    } else if (datalen == 2) {
			    *value = M32C_REG_R0;
		    }
		    *codelen = 0;
		    break;
	    case 1:
		    addr = M32C_Read16(M32C_REG_PC) + index;
		    if (datalen == 1) {
			    *value = M32C_Read8(addr);
		    } else if (datalen == 2) {
			    *value = M32C_Read16(addr);
		    } else if (datalen == 4) {
			    *value = M32C_Read32(addr);
		    }
		    *codelen = 2;
		    break;
	    case 2:		/* dsp:8[sb] */
		    dsp8_u = M32C_Read8(M32C_REG_PC);
		    addr = (M32C_REG_SB + dsp8_u + index) & 0xffffff;
		    if (datalen == 1) {
			    *value = M32C_Read8(addr);
		    } else if (datalen == 2) {
			    *value = M32C_Read16(addr);
		    } else if (datalen == 4) {
			    *value = M32C_Read32(addr);
		    }
		    *codelen = 1;
		    break;

	    case 3:		/* dsp:8[fb] */
		    dsp8_s = M32C_Read8(M32C_REG_PC);
		    addr = (M32C_REG_FB + dsp8_s + index) & 0xffffff;
		    if (datalen == 1) {
			    *value = M32C_Read8(addr);
		    } else if (datalen == 2) {
			    *value = M32C_Read16(addr);
		    } else if (datalen == 4) {
			    *value = M32C_Read32(addr);
		    }
		    *codelen = 1;
		    break;
	    default:
		    *codelen = 0;
		    *value = 0;
	}
}
#endif

static void
am2bit_get_r0l(uint32_t * value, uint32_t index)
{
	*value = M32C_REG_R0L;
}

static void
am2bit_get_r0(uint32_t * value, uint32_t index)
{
	*value = M32C_REG_R0;
}

static void
am2bit_get_abs16_size8(uint32_t * value, uint32_t index)
{
	uint32_t addr;
	addr = M32C_Read16(M32C_REG_PC) + index;
	*value = M32C_Read8(addr);
}

static void
am2bit_get_abs16_size16(uint32_t * value, uint32_t index)
{
	uint32_t addr;
	addr = M32C_Read16(M32C_REG_PC) + index;
	*value = M32C_Read16(addr);
}

static void
am2bit_get_abs16_size32(uint32_t * value, uint32_t index)
{
	uint32_t addr;
	addr = M32C_Read16(M32C_REG_PC) + index;
	*value = M32C_Read32(addr);
}

static void
am2bit_get_dsp8_sb_size8(uint32_t * value, uint32_t index)
{
	uint8_t dsp8_u;
	uint32_t addr;
	dsp8_u = M32C_Read8(M32C_REG_PC);
	addr = (M32C_REG_SB + dsp8_u + index) & 0xffffff;
	*value = M32C_Read8(addr);
}

static void
am2bit_get_dsp8_sb_size16(uint32_t * value, uint32_t index)
{
	uint8_t dsp8_u;
	uint32_t addr;
	dsp8_u = M32C_Read8(M32C_REG_PC);
	addr = (M32C_REG_SB + dsp8_u + index) & 0xffffff;
	*value = M32C_Read16(addr);
}

static void
am2bit_get_dsp8_sb_size32(uint32_t * value, uint32_t index)
{
	uint8_t dsp8_u;
	uint32_t addr;
	dsp8_u = M32C_Read8(M32C_REG_PC);
	addr = (M32C_REG_SB + dsp8_u + index) & 0xffffff;
	*value = M32C_Read32(addr);
}

static void
am2bit_get_dsp8_fb_size8(uint32_t * value, uint32_t index)
{
	int8_t dsp8_s;
	uint32_t addr;
	dsp8_s = M32C_Read8(M32C_REG_PC);
	addr = (M32C_REG_FB + dsp8_s + index) & 0xffffff;
	*value = M32C_Read8(addr);
}

static void
am2bit_get_dsp8_fb_size16(uint32_t * value, uint32_t index)
{
	int8_t dsp8_s;
	uint32_t addr;
	dsp8_s = M32C_Read8(M32C_REG_PC);
	addr = (M32C_REG_FB + dsp8_s + index) & 0xffffff;
	*value = M32C_Read16(addr);
}

static void
am2bit_get_dsp8_fb_size32(uint32_t * value, uint32_t index)
{
	int8_t dsp8_s;
	uint32_t addr;
	dsp8_s = M32C_Read8(M32C_REG_PC);
	addr = (M32C_REG_FB + dsp8_s + index) & 0xffffff;
	*value = M32C_Read32(addr);
}

static AM2Bit_GetProc *
am2bit_getproc(int am, int *codelen, int datalen)
{
	AM2Bit_GetProc *proc = NULL;
	switch (am) {
	    case 0:
		    if (datalen == 1) {
			    proc = am2bit_get_r0l;
		    } else if (datalen == 2) {
			    proc = am2bit_get_r0;
		    }
		    *codelen = 0;
		    break;
	    case 1:
		    if (datalen == 1) {
			    proc = am2bit_get_abs16_size8;
		    } else if (datalen == 2) {
			    proc = am2bit_get_abs16_size16;
		    } else if (datalen == 4) {
			    proc = am2bit_get_abs16_size32;
		    }
		    *codelen = 2;
		    break;
	    case 2:		/* dsp:8[sb] */
		    if (datalen == 1) {
			    proc = am2bit_get_dsp8_sb_size8;
		    } else if (datalen == 2) {
			    proc = am2bit_get_dsp8_sb_size16;
		    } else if (datalen == 4) {
			    proc = am2bit_get_dsp8_sb_size32;
		    }
		    *codelen = 1;
		    break;

	    case 3:		/* dsp:8[fb] */
		    if (datalen == 1) {
			    proc = am2bit_get_dsp8_fb_size8;
		    } else if (datalen == 2) {
			    proc = am2bit_get_dsp8_fb_size16;
		    } else if (datalen == 4) {
			    proc = am2bit_get_dsp8_fb_size32;
		    }
		    *codelen = 1;
		    break;
	    default:
		    fprintf(stderr, "Reached unreachable code\n");
		    exit(1);
	}
	if (!proc) {
		fprintf(stderr, "Illegal AM2 addressing mode\n");
		exit(1);
	}
	return proc;
}

static void
am2bit_set_r0l(uint32_t value, uint32_t index)
{
	M32C_REG_R0L = value;
}

static void
am2bit_set_r0(uint32_t value, uint32_t index)
{
	M32C_REG_R0 = value;
}

static void
am2bit_set_abs16_size8(uint32_t value, uint32_t index)
{
	uint32_t addr = M32C_Read16(M32C_REG_PC) + index;
	M32C_Write8(value, addr);
}

static void
am2bit_set_abs16_size16(uint32_t value, uint32_t index)
{
	uint32_t addr = M32C_Read16(M32C_REG_PC) + index;
	M32C_Write16(value, addr);
}

static void
am2bit_set_abs16_size32(uint32_t value, uint32_t index)
{
	uint32_t addr = M32C_Read16(M32C_REG_PC) + index;
	M32C_Write32(value, addr);
}

static void
am2bit_set_dsp8_sb_size8(uint32_t value, uint32_t index)
{
	uint32_t addr;
	uint8_t dsp8_u;
	dsp8_u = M32C_Read8(M32C_REG_PC);
	addr = (M32C_REG_SB + dsp8_u + index) & 0xffffff;
	M32C_Write8(value, addr);
}

static void
am2bit_set_dsp8_sb_size16(uint32_t value, uint32_t index)
{
	uint32_t addr;
	uint8_t dsp8_u;
	dsp8_u = M32C_Read8(M32C_REG_PC);
	addr = (M32C_REG_SB + dsp8_u + index) & 0xffffff;
	M32C_Write16(value, addr);
}

static void
am2bit_set_dsp8_sb_size32(uint32_t value, uint32_t index)
{
	uint32_t addr;
	uint8_t dsp8_u;
	dsp8_u = M32C_Read8(M32C_REG_PC);
	addr = (M32C_REG_SB + dsp8_u + index) & 0xffffff;
	M32C_Write32(value, addr);
}

static void
am2bit_set_dsp8_fb_size8(uint32_t value, uint32_t index)
{
	uint32_t addr;
	int8_t dsp8_s;
	dsp8_s = M32C_Read8(M32C_REG_PC);
	addr = (M32C_REG_FB + dsp8_s + index) & 0xffffff;
	M32C_Write8(value, addr);
}

static void
am2bit_set_dsp8_fb_size16(uint32_t value, uint32_t index)
{
	uint32_t addr;
	int8_t dsp8_s;
	dsp8_s = M32C_Read8(M32C_REG_PC);
	addr = (M32C_REG_FB + dsp8_s + index) & 0xffffff;
	M32C_Write16(value, addr);
}

static void
am2bit_set_dsp8_fb_size32(uint32_t value, uint32_t index)
{
	uint32_t addr;
	int8_t dsp8_s;
	dsp8_s = M32C_Read8(M32C_REG_PC);
	addr = (M32C_REG_FB + dsp8_s + index) & 0xffffff;
	M32C_Write32(value, addr);
}

static AM2Bit_SetProc *
am2bit_setproc(int am, int *codelen, int datalen)
{
	AM2Bit_SetProc *proc = NULL;
	switch (am) {
	    case 0:
		    if (datalen == 1) {
			    proc = am2bit_set_r0l;
		    } else if (datalen == 2) {
			    proc = am2bit_set_r0;
		    }
		    *codelen = 0;
		    break;
	    case 1:		/* ABS 16 */
		    if (datalen == 1) {
			    proc = am2bit_set_abs16_size8;
		    } else if (datalen == 2) {
			    proc = am2bit_set_abs16_size16;
		    } else if (datalen == 4) {
			    proc = am2bit_set_abs16_size32;
		    }
		    *codelen = 2;
		    break;
	    case 2:		/* dsp:8[sb] */
		    if (datalen == 1) {
			    proc = am2bit_set_dsp8_sb_size8;
		    } else if (datalen == 2) {
			    proc = am2bit_set_dsp8_sb_size16;
		    } else if (datalen == 4) {
			    proc = am2bit_set_dsp8_sb_size32;
		    }
		    *codelen = 1;
		    break;

	    case 3:		/* dsp:8[fb] */
		    if (datalen == 1) {
			    proc = am2bit_set_dsp8_fb_size8;
		    } else if (datalen == 2) {
			    proc = am2bit_set_dsp8_fb_size16;
		    } else if (datalen == 4) {
			    proc = am2bit_set_dsp8_fb_size32;
		    }
		    *codelen = 1;
		    break;
	    default:
		    *codelen = 0;
	}
	if (!proc) {
		fprintf(stderr, "Illegal AM2 addressing mode\n");
		exit(1);
	}
	return proc;
}

/*
 * Only xchg uses this. 
 */
static uint32_t
am3bitreg_get(int am, int size)
{
	switch (am) {
	    case 0:
		    if (size == 2) {
			    return M32C_REG_R0;
		    } else if (size == 1) {
			    return M32C_REG_R0L;
		    }
		    break;
	    case 1:
		    if (size == 2) {
			    return M32C_REG_R1;
		    } else if (size == 1) {
			    return M32C_REG_R1L;
		    }
		    break;
	    case 2:
		    if (size == 1) {
			    return M32C_REG_A0 & 0xff;
		    } else if (size == 2) {
			    return M32C_REG_A0 & 0xffff;
		    }
		    break;
	    case 3:
		    if (size == 1) {
			    return M32C_REG_A1 & 0xff;
		    } else if (size == 2) {
			    return M32C_REG_A1 & 0xffff;
		    }
	    case 4:
		    if (size == 2) {
			    return M32C_REG_R2;
		    } else if (size == 1) {
			    return M32C_REG_R0H;
		    }
		    break;
	    case 5:
		    if (size == 2) {
			    return M32C_REG_R3;
		    } else if (size == 1) {
			    return M32C_REG_R1H;
		    }
		    break;
	}
	return 0;
}

static void
am3bitreg_set(int am, int size, uint32_t value)
{
	switch (am) {
	    case 0:
		    if (size == 2) {
			    M32C_REG_R0 = value;
		    } else if (size == 1) {
			    M32C_REG_R0L = value;
		    }
		    break;
	    case 1:
		    if (size == 2) {
			    M32C_REG_R1 = value;
		    } else if (size == 1) {
			    M32C_REG_R1L = value;
		    }
		    break;
	    case 2:
		    M32C_REG_A0 = value;
		    break;
	    case 3:
		    M32C_REG_A1 = value;
		    break;
	    case 4:
		    if (size == 2) {
			    M32C_REG_R2 = value;
		    } else if (size == 1) {
			    M32C_REG_R0H = value;
		    }
		    break;
	    case 5:
		    if (size == 2) {
			    M32C_REG_R3 = value;
		    } else if (size == 1) {
			    M32C_REG_R1H = value;
		    }
		    break;
	}
}

static void
set_bitaddr_r0l(uint8_t value, int32_t bitindex)
{
	M32C_REG_R0L = value;
}

static void
set_bitaddr_r0h(uint8_t value, int32_t bitindex)
{
	M32C_REG_R0H = value;
}

static void
set_bitaddr_r1l(uint8_t value, int32_t bitindex)
{
	M32C_REG_R1L = value;
}

static void
set_bitaddr_r1h(uint8_t value, int32_t bitindex)
{
	M32C_REG_R1H = value;
}

static void
set_bitaddr_a0(uint8_t value, int32_t bitindex)
{
	assert(((M32C_REG_A0 ^ value) & 0xffff00) == 0);
	M32C_REG_A0 = value;
}

static void
set_bitaddr_a1(uint8_t value, int32_t bitindex)
{
	assert(((M32C_REG_A1 ^ value) & 0xffff00) == 0);
	M32C_REG_A1 = value;
}

static void
set_bitaddr_ia0(uint8_t value, int32_t bitindex)
{
	uint32_t addr;
	uint32_t index;
	if (bitindex >= 0) {
		index = bitindex >> 3;
	} else {
		index = 0;
	}
	addr = M32C_REG_A0 + index;
	addr = addr & 0xffffff;
	M32C_Write8(value, addr);
}

static void
set_bitaddr_ia1(uint8_t value, int32_t bitindex)
{
	uint32_t addr;
	uint32_t index;
	if (bitindex >= 0) {
		index = bitindex >> 3;
	} else {
		index = 0;
	}
	addr = M32C_REG_A1 + index;
	addr = addr & 0xffffff;
	M32C_Write8(value, addr);
}

static void
set_bitaddr_base11_ia0(uint8_t value, int32_t bitindex)
{
	uint32_t base;
	uint32_t addr;
	uint32_t index;
	if (bitindex >= 0) {
		index = bitindex >> 3;
	} else {
		index = 0;
	}
	base = M32C_Read8(M32C_REG_PC);
	addr = base + M32C_REG_A0 + index;
	addr = addr & 0xffffff;
	M32C_Write8(value, addr);
}

static void
set_bitaddr_base11_ia1(uint8_t value, int32_t bitindex)
{
	uint32_t base;
	uint32_t addr;
	uint32_t index;
	if (bitindex >= 0) {
		index = bitindex >> 3;
	} else {
		index = 0;
	}
	base = M32C_Read8(M32C_REG_PC);
	addr = base + M32C_REG_A1 + index;
	addr = addr & 0xffffff;
	M32C_Write8(value, addr);
}

static void
set_bitaddr_base11_isb(uint8_t value, int32_t bitindex)
{
	uint32_t base;
	uint32_t addr;
	uint32_t index;
	if (bitindex >= 0) {
		index = bitindex >> 3;
	} else {
		index = 0;
	}
	base = M32C_Read8(M32C_REG_PC);
	addr = base + M32C_REG_SB + index;
	addr = addr & 0xffffff;
	M32C_Write8(value, addr);
}

static void
set_bitaddr_base11_ifb(uint8_t value, int32_t bitindex)
{
	uint32_t addr;
	int8_t sdsp8;
	uint32_t index;
	if (bitindex >= 0) {
		index = bitindex >> 3;
	} else {
		index = 0;
	}
	sdsp8 = M32C_Read8(M32C_REG_PC);
	addr = M32C_REG_FB + sdsp8 + index;
	addr = addr & 0xffffff;
	M32C_Write8(value, addr);
}

static void
set_bitaddr_base19_ia0(uint8_t value, int32_t bitindex)
{
	uint32_t base;
	uint32_t addr;
	uint32_t index;
	if (bitindex >= 0) {
		index = bitindex >> 3;
	} else {
		index = 0;
	}
	base = M32C_Read16(M32C_REG_PC);
	addr = base + M32C_REG_A0 + index;
	addr = addr & 0xffffff;
	M32C_Write8(value, addr);
}

static void
set_bitaddr_base19_ia1(uint8_t value, int32_t bitindex)
{
	uint32_t base;
	uint32_t addr;
	uint32_t index;
	if (bitindex >= 0) {
		index = bitindex >> 3;
	} else {
		index = 0;
	}
	base = M32C_Read16(M32C_REG_PC);
	addr = base + M32C_REG_A1 + index;
	addr = addr & 0xffffff;
	M32C_Write8(value, addr);
}

static void
set_bitaddr_base19_isb(uint8_t value, int32_t bitindex)
{
	uint32_t base;
	uint32_t addr;
	uint32_t index;
	if (bitindex >= 0) {
		index = bitindex >> 3;
	} else {
		index = 0;
	}
	base = M32C_Read16(M32C_REG_PC);
	addr = base + M32C_REG_SB + index;
	addr = addr & 0xffffff;
	M32C_Write8(value, addr);
}

static void
set_bitaddr_base19_ifb(uint8_t value, int32_t bitindex)
{
	uint32_t addr;
	uint32_t index;
	int16_t sdsp16;
	if (bitindex >= 0) {
		index = bitindex >> 3;
	} else {
		index = 0;
	}
	sdsp16 = M32C_Read16(M32C_REG_PC);
	addr = sdsp16 + M32C_REG_FB + index;
	addr = addr & 0xffffff;
	M32C_Write8(value, addr);
}

static void
set_bitaddr_base27_ia0(uint8_t value, int32_t bitindex)
{
	uint32_t base;
	uint32_t addr;
	uint32_t index;
	if (bitindex >= 0) {
		index = bitindex >> 3;
	} else {
		index = 0;
	}
	base = M32C_Read24(M32C_REG_PC);
	addr = base + M32C_REG_A0 + index;
	addr = addr & 0xffffff;
	M32C_Write8(value, addr);
}

static void
set_bitaddr_base27_ia1(uint8_t value, int32_t bitindex)
{
	uint32_t base;
	uint32_t addr;
	uint32_t index;
	if (bitindex >= 0) {
		index = bitindex >> 3;
	} else {
		index = 0;
	}
	base = M32C_Read24(M32C_REG_PC);
	addr = base + M32C_REG_A1 + index;
	addr = addr & 0xffffff;
	M32C_Write8(value, addr);
}

static void
set_bitaddr_base19(uint8_t value, int32_t bitindex)
{
	uint32_t base;
	uint32_t addr;
	uint32_t index;
	if (bitindex >= 0) {
		index = bitindex >> 3;
	} else {
		index = 0;
	}
	base = M32C_Read16(M32C_REG_PC);
	addr = base + index;
	addr = addr & 0xffffff;
	M32C_Write8(value, addr);
}

static void
set_bitaddr_base27(uint8_t value, int32_t bitindex)
{
	uint32_t base;
	uint32_t addr;
	uint32_t index;
	if (bitindex >= 0) {
		index = bitindex >> 3;
	} else {
		index = 0;
	}
	base = M32C_Read24(M32C_REG_PC);
	addr = base + index;
	addr = addr & 0xffffff;
	M32C_Write8(value, addr);
}

static uint32_t
get_bitaddr_r0l(int32_t bitindex)
{
	return M32C_REG_R0L;
}

static uint32_t
get_bitaddr_r0h(int32_t bitindex)
{
	return M32C_REG_R0H;
}

static uint32_t
get_bitaddr_r1l(int32_t bitindex)
{
	return M32C_REG_R1L;
}

static uint32_t
get_bitaddr_r1h(int32_t bitindex)
{
	return M32C_REG_R1H;
}

static uint32_t
get_bitaddr_a0(int32_t bitindex)
{
	return M32C_REG_A0;
}

static uint32_t
get_bitaddr_a1(int32_t bitindex)
{
	return M32C_REG_A1;
}

static uint32_t
get_bitaddr_ia0(int32_t bitindex)
{
	uint32_t addr;
	uint32_t index;
	if (bitindex >= 0) {
		index = bitindex >> 3;
	} else {
		index = 0;
	}
	addr = (M32C_REG_A0 + index) & 0xffffff;
	return M32C_Read8(addr);
}

static uint32_t
get_bitaddr_ia1(int32_t bitindex)
{
	uint32_t addr;
	uint32_t index;
	if (bitindex >= 0) {
		index = bitindex >> 3;
	} else {
		index = 0;
	}
	/* bit,[A1] */
	addr = (M32C_REG_A1 + index) & 0xffffff;
	return M32C_Read8(addr);
}

static uint32_t
get_bitaddr_base11_ia0(int32_t bitindex)
{
	uint32_t base;
	uint32_t addr;
	uint32_t index;
	if (bitindex >= 0) {
		index = bitindex >> 3;
	} else {
		index = 0;
	}
	base = M32C_Read8(M32C_REG_PC);
	addr = base + M32C_REG_A0 + index;
	addr = addr & 0xffffff;
	return M32C_Read8(addr);
}

static uint32_t
get_bitaddr_base11_ia1(int32_t bitindex)
{
	uint32_t base;
	uint32_t addr;
	uint32_t index;
	if (bitindex >= 0) {
		index = bitindex >> 3;
	} else {
		index = 0;
	}
	base = M32C_Read8(M32C_REG_PC);
	addr = base + M32C_REG_A1 + index;
	addr = addr & 0xffffff;
	return M32C_Read8(addr);
}

static uint32_t
get_bitaddr_base11_isb(int32_t bitindex)
{
	uint32_t base;
	uint32_t addr;
	uint32_t index;
	if (bitindex >= 0) {
		index = bitindex >> 3;
	} else {
		index = 0;
	}
	base = M32C_Read8(M32C_REG_PC);
	addr = base + M32C_REG_SB + index;
	addr = addr & 0xffffff;
	return M32C_Read8(addr);
}

static uint32_t
get_bitaddr_base11_ifb(int32_t bitindex)
{
	uint32_t addr;
	uint32_t index;
	int8_t sdsp8;
	if (bitindex >= 0) {
		index = bitindex >> 3;
	} else {
		index = 0;
	}
	sdsp8 = M32C_Read8(M32C_REG_PC);
	addr = M32C_REG_FB + sdsp8 + index;
	addr = addr & 0xffffff;
	return M32C_Read8(addr);
}

static uint32_t
get_bitaddr_base19_ia0(int32_t bitindex)
{
	uint32_t base;
	uint32_t addr;
	uint32_t index;
	if (bitindex >= 0) {
		index = bitindex >> 3;
	} else {
		index = 0;
	}
	base = M32C_Read16(M32C_REG_PC);
	addr = base + M32C_REG_A0 + index;
	addr = addr & 0xffffff;
	return M32C_Read8(addr);
}

static uint32_t
get_bitaddr_base19_ia1(int32_t bitindex)
{
	uint32_t base;
	uint32_t addr;
	uint32_t index;
	if (bitindex >= 0) {
		index = bitindex >> 3;
	} else {
		index = 0;
	}
	/* bit,base:19[A1] */
	base = M32C_Read16(M32C_REG_PC);
	addr = base + M32C_REG_A1 + index;
	addr = addr & 0xffffff;
	return M32C_Read8(addr);
}

static uint32_t
get_bitaddr_base19_isb(int32_t bitindex)
{
	uint32_t base;
	uint32_t addr;
	uint32_t index;
	if (bitindex >= 0) {
		index = bitindex >> 3;
	} else {
		index = 0;
	}
	base = M32C_Read16(M32C_REG_PC);
	addr = base + M32C_REG_SB + index;
	addr = addr & 0xffffff;
	return M32C_Read8(addr);
}

static uint32_t
get_bitaddr_base19_ifb(int32_t bitindex)
{
	uint32_t addr;
	uint32_t index;
	int16_t sdsp16;
	if (bitindex >= 0) {
		index = bitindex >> 3;
	} else {
		index = 0;
	}
	/* bit,base:19[FB] */
	sdsp16 = M32C_Read16(M32C_REG_PC);
	addr = M32C_REG_FB + sdsp16 + index;
	addr = addr & 0xffffff;
	return M32C_Read8(addr);
}

static uint32_t
get_bitaddr_base27_ia0(int32_t bitindex)
{
	uint32_t base;
	uint32_t addr;
	uint32_t index;
	if (bitindex >= 0) {
		index = bitindex >> 3;
	} else {
		index = 0;
	}
	/* bit,base:27[A0] */
	base = M32C_Read24(M32C_REG_PC);
	addr = base + M32C_REG_A0 + index;
	addr = addr & 0xffffff;
	return M32C_Read8(addr);
}

static uint32_t
get_bitaddr_base27_ia1(int32_t bitindex)
{
	uint32_t base;
	uint32_t addr;
	uint32_t index;
	if (bitindex >= 0) {
		index = bitindex >> 3;
	} else {
		index = 0;
	}
	/* bit,base:27[A1] */
	base = M32C_Read24(M32C_REG_PC);
	addr = base + M32C_REG_A1 + index;
	addr = addr & 0xffffff;
	return M32C_Read8(addr);
}

static uint32_t
get_bitaddr_base19(int32_t bitindex)
{
	uint32_t base;
	uint32_t addr;
	uint32_t index;
	if (bitindex >= 0) {
		index = bitindex >> 3;
	} else {
		index = 0;
	}
	/* bit,base:16 */
	base = M32C_Read16(M32C_REG_PC);
	addr = base + index;
	addr = addr & 0xffffff;
	return M32C_Read8(addr);
}

static uint32_t
get_bitaddr_base27(int32_t bitindex)
{
	uint32_t base;
	uint32_t addr;
	uint32_t index;
	if (bitindex >= 0) {
		index = bitindex >> 3;
	} else {
		index = 0;
	}
	/* bit,base:24 */
	base = M32C_Read24(M32C_REG_PC);
	addr = base + index;
	addr = addr & 0xffffff;
	return M32C_Read8(addr);
}

static SetBit_Proc *
set_bitaddrproc(int am)
{
	switch (am) {
	    case 0x12:
		    return set_bitaddr_r0l;

	    case 0x10:
		    return set_bitaddr_r0h;

	    case 0x13:
		    return set_bitaddr_r1l;

	    case 0x11:
		    return set_bitaddr_r1h;

	    case 0x02:
		    return set_bitaddr_a0;

	    case 0x03:
		    return set_bitaddr_a1;

	    case 0x00:
		    return set_bitaddr_ia0;

	    case 0x01:
		    return set_bitaddr_ia1;

	    case 0x04:
		    return set_bitaddr_base11_ia0;

	    case 0x05:
		    return set_bitaddr_base11_ia1;

	    case 0x06:
		    return set_bitaddr_base11_isb;

	    case 0x07:
		    return set_bitaddr_base11_ifb;

	    case 0x08:
		    return set_bitaddr_base19_ia0;

	    case 0x09:
		    return set_bitaddr_base19_ia1;

	    case 0x0a:
		    return set_bitaddr_base19_isb;

	    case 0x0b:
		    return set_bitaddr_base19_ifb;

	    case 0x0c:
		    return set_bitaddr_base27_ia0;

	    case 0x0d:
		    return set_bitaddr_base27_ia1;

	    case 0x0f:
		    return set_bitaddr_base19;

	    case 0x0e:
		    return set_bitaddr_base27;

	}
	fprintf(stderr, "Illegal bit addressing mode %d\n", am);
	exit(1);
}

static GetBit_Proc *
get_bitaddrproc(int am, int *codelen)
{
	switch (am) {
		    /* bit,R0L */
	    case 0x12:
		    *codelen = 0;
		    return get_bitaddr_r0l;

		    /* bit,R0H */
	    case 0x10:
		    *codelen = 0;
		    return get_bitaddr_r0h;

		    /* bit, R1L */
	    case 0x13:
		    *codelen = 0;
		    return get_bitaddr_r1l;

		    /* bit, R1H */
	    case 0x11:
		    *codelen = 0;
		    return get_bitaddr_r1h;

		    /* bit, A0 */
	    case 0x02:
		    *codelen = 0;
		    return get_bitaddr_a0;

		    /* bit, A1 */
	    case 0x03:
		    *codelen = 0;
		    return get_bitaddr_a1;

		    /* bit,[A0] */
	    case 0x00:
		    *codelen = 0;
		    return get_bitaddr_ia0;

		    /* bit,[A1] */
	    case 0x01:
		    *codelen = 0;
		    return get_bitaddr_ia1;

		    /* bit,base:11[A0] */
	    case 0x04:
		    *codelen = 1;
		    return get_bitaddr_base11_ia0;

		    /* bit,base:11[A1] */
	    case 0x05:
		    *codelen = 1;
		    return get_bitaddr_base11_ia1;

		    /* bit,base:11[SB] */
	    case 0x06:
		    *codelen = 1;
		    return get_bitaddr_base11_isb;

		    /* bit,base:11[FB] */
	    case 0x07:
		    *codelen = 1;
		    return get_bitaddr_base11_ifb;

		    /* bit,base:19[A0] */
	    case 0x08:
		    *codelen = 2;
		    return get_bitaddr_base19_ia0;

		    /* bit,base:19[A1] */
	    case 0x09:
		    *codelen = 2;
		    return get_bitaddr_base19_ia1;

		    /* bit,base:19[SB] */
	    case 0x0a:
		    *codelen = 2;
		    return get_bitaddr_base19_isb;

		    /* bit,base:19[FB] */
	    case 0x0b:
		    *codelen = 2;
		    return get_bitaddr_base19_ifb;

		    /* bit,base:27[A0] */
	    case 0x0c:
		    *codelen = 3;
		    return get_bitaddr_base27_ia0;

		    /* bit,base:27[A1] */
	    case 0x0d:
		    *codelen = 3;
		    return get_bitaddr_base27_ia1;

		    /* bit,base:16 */
	    case 0x0f:
		    *codelen = 2;
		    return get_bitaddr_base19;

		    /* bit,base:24 */
	    case 0x0e:
		    *codelen = 3;
		    return get_bitaddr_base27;
	    default:
		    fprintf(stderr, "Illegal bit addressing mode %d\n", am);
		    break;
	}
	fprintf(stderr, "Illegal bit addressing mode %d\n", am);
	exit(1);
	return NULL;
}

static inline void
_ModOpsize(int am, int *opsize)
{
	switch (am) {
	    case 0x02:
	    case 0x03:
		    if (*opsize == 1) {
			    *opsize = 2;
		    }
	}
}

static void
ModOpsizeError(const char *function)
{
	fprintf(stderr, "ModOpsize Not allowed PC 0x%06x in: %s\n", M32C_REG_PC, function);
	exit(1);
}

#define ModOpsize(am,opsize) { _ModOpsize((am),(opsize)); \
	if (*opsize == 2) { \
		/* fprintf(stderr,"ModOpsize: %s\n",__FUNCTION__); */ \
	} \
}

#define NotModOpsize(am) { \
	if((am == 2) || (am == 3)) { \
		ModOpsizeError(__FUNCTION__); \
	} \
}

/*
 *******************************************************************
 * Take the absolute value of dest and store it in dest
 * Test with real device: Carry flag is always cleared.
 * Untested: Does a write really occur if Dst is already positive ?
 * v0
 *******************************************************************
 */
static void
m32c_abs_size_dst(void)
{
	uint32_t Dst;
	int opsize = INSTR->opsize;
	M32C_REG_FLG &= ~(M32C_FLG_OVERFLOW | M32C_FLG_SIGN | M32C_FLG_ZERO | M32C_FLG_CARRY);
	INSTR->getdst(&Dst, M32C_INDEXSD());
	if (opsize == 2) {
		if (Dst & 0x8000) {
			Dst = 0 - Dst;
			if (Dst & 0x8000) {
				M32C_REG_FLG |= M32C_FLG_OVERFLOW;
				M32C_REG_FLG |= M32C_FLG_SIGN;
			}
		} else {
			if ((Dst & 0xffff) == 0) {
				M32C_REG_FLG |= M32C_FLG_ZERO;
			}
		}
		INSTR->setdst(Dst, M32C_INDEXWD());
	} else {		/* if(size == 1) */

		if (Dst & 0x80) {
			Dst = 0x00 - Dst;
			if (Dst & 0x80) {
				M32C_REG_FLG |= M32C_FLG_OVERFLOW;
				M32C_REG_FLG |= M32C_FLG_SIGN;
			}
		} else {
			if ((Dst & 0xff) == 0) {
				M32C_REG_FLG |= M32C_FLG_ZERO;
			}
		}
		INSTR->setdst(Dst, M32C_INDEXBD());
	}
	M32C_REG_PC += INSTR->codelen_dst;
	dbgprintf("m32c_abs_size_dst not tested\n");
}

/**
 */
void
m32c_setup_abs_size_dst(void)
{
	int dst = ((ICODE16() >> 6) & 3) | ((ICODE16() >> 7) & 0x1c);
	int size;
	int codelen_dst;
	if (ICODE16() & 0x100) {
		size = 2;
	} else {
		size = 1;
		NotModOpsize(dst);
	}
	INSTR->getdst = general_am_get(dst, size, &codelen_dst, 0xfffff);
	INSTR->setdst = general_am_set(dst, size, &codelen_dst, 0xfffff);
	INSTR->opsize = size;
	INSTR->codelen_dst = codelen_dst;
	INSTR->proc = m32c_abs_size_dst;
	INSTR->proc();
}

/*
 *******************************************************************
 * Take the absolute value of indirect dest and store it in 
 * indirect dest.
 * v0
 *******************************************************************
 */
static void
m32c_abs_size_idst(void)
{
	uint32_t Dst;
	uint32_t DstP;
	M32C_REG_FLG &= ~(M32C_FLG_OVERFLOW | M32C_FLG_SIGN | M32C_FLG_ZERO | M32C_FLG_CARRY);

	INSTR->getdstp(&DstP, 0);
	DstP = (DstP + M32C_INDEXSD()) & 0xffffff;
	if (INSTR->opsize == 2) {
		Dst = M32C_Read16(DstP);
		if (Dst & 0x8000) {
			Dst = 0x10000 - Dst;
			if (Dst & 0x8000) {
				M32C_REG_FLG |= M32C_FLG_OVERFLOW;
				M32C_REG_FLG |= M32C_FLG_SIGN;
			}
		}
		M32C_Write16(Dst, DstP);
		if ((Dst & 0xffff) == 0) {
			M32C_REG_FLG |= M32C_FLG_ZERO;
		}
	} else {		/* if(size == 1) */

		Dst = M32C_Read8(DstP);
		if (Dst & 0x80) {
			Dst = 0x100 - Dst;
			if (Dst & 0x80) {
				M32C_REG_FLG |= M32C_FLG_OVERFLOW;
				M32C_REG_FLG |= M32C_FLG_SIGN;
			}
		}
		M32C_Write8(Dst, DstP);
		if ((Dst & 0xff) == 0) {
			M32C_REG_FLG |= M32C_FLG_ZERO;
		}
	}
	M32C_REG_PC += INSTR->codelen_dst;
	dbgprintf("m32c_abs_size_idst not tested\n");
}

void
m32c_setup_abs_size_idst(void)
{
	int dst = ((ICODE24() >> 6) & 3) | ((ICODE24() >> 7) & 0x1c);
	int codelen_dst;
	M32C_REG_FLG &= ~(M32C_FLG_OVERFLOW | M32C_FLG_SIGN | M32C_FLG_ZERO | M32C_FLG_CARRY);

	if (ICODE24() & 0x100) {
		INSTR->opsize = 2;
	} else {
		INSTR->opsize = 1;
	}
	INSTR->getdstp = general_am_get(dst, 4, &codelen_dst, 0xfffff);
	INSTR->codelen_dst = codelen_dst;
	INSTR->proc = m32c_abs_size_idst;
	INSTR->proc();
}

/*
 ********************************************************************
 * \fn void m32c_adc_size_immdst(void)
 * Add an immediate from code memory to the src and the carry.
 * v1
 ********************************************************************
 */
static void
m32c_adc_size_immdst(void)
{
	uint32_t Src, Dst, Result;
	int opsize = INSTR->opsize;
	if (INSTR->srcsize == 2) {
		Src = M32C_Read16(M32C_REG_PC + INSTR->codelen_dst);
	} else {		/* if(INSTR->srcsize == 1) */

		Src = M32C_Read8(M32C_REG_PC + INSTR->codelen_dst);
	}
	INSTR->getdst(&Dst, M32C_INDEXSD());
	if (M32C_REG_FLG & M32C_FLG_CARRY) {
		Result = Dst + Src + 1;
	} else {
		Result = Dst + Src;
	}
	INSTR->setdst(Result, M32C_INDEXSD());
	add_flags(Dst, Src, Result, opsize);
	M32C_REG_PC += INSTR->codelen_dst + INSTR->srcsize;
	dbgprintf("m32c_adc_size_immdst not tested\n");
}

void
m32c_setup_adc_size_immdst(void)
{
	int dst;
	int opsize, srcsize;
	int codelen_dst;

	dst = ((ICODE24() >> 6) & 3) | ((ICODE24() >> 7) & 0x1c);
	if (ICODE24() & 0x100) {
		srcsize = opsize = 2;
	} else {
		srcsize = opsize = 1;
		ModOpsize(dst, &opsize);
	}
	INSTR->getdst = general_am_get(dst, opsize, &codelen_dst, 0xfffff);
	INSTR->setdst = general_am_set(dst, opsize, &codelen_dst, 0xfffff);
	INSTR->opsize = opsize;
	INSTR->srcsize = srcsize;
	INSTR->codelen_dst = codelen_dst;
	INSTR->proc = m32c_adc_size_immdst;
	INSTR->proc();
}

/**
 ************************************************************
 * \fn void m32c_adc_size_srcdst(void)
 * Add src to dst with carry.
 * v1
 ************************************************************
 */
static void
m32c_adc_size_srcdst(void)
{
	uint32_t Src, Dst, Result;
	int opsize = INSTR->opsize;
	INSTR->getsrc(&Src, M32C_INDEXSS());
	M32C_REG_PC += INSTR->codelen_src;
	INSTR->getdst(&Dst, M32C_INDEXSD());
	if (M32C_REG_FLG & M32C_FLG_CARRY) {
		Result = Dst + Src + 1;
	} else {
		Result = Dst + Src;
	}
	INSTR->setdst(Result, M32C_INDEXSD());
	add_flags(Dst, Src, Result, opsize);
	M32C_REG_PC += INSTR->codelen_dst;
	dbgprintf("m32c_adc_size_srcdst not tested\n");
}

void
m32c_setup_adc_size_srcdst(void)
{
	int dst, src;
	int opsize, srcsize;
	int codelen_src;
	int codelen_dst;
	dst = ((ICODE24() >> 6) & 3) | ((ICODE24() >> 7) & 0x1c);
	src = ((ICODE24() >> 4) & 3) | ((ICODE24() >> 10) & 0x1c);
	if (ICODE24() & 0x100) {
		srcsize = opsize = 2;
	} else {
		srcsize = opsize = 1;
		ModOpsize(dst, &opsize);
	}
	INSTR->srcsize = srcsize;
	INSTR->opsize = opsize;
	INSTR->getsrc = general_am_get(src, srcsize, &codelen_src, 0xfffff);
	INSTR->getdst = general_am_get(dst, opsize, &codelen_dst, 0xfffff);
	INSTR->setdst = general_am_set(dst, opsize, &codelen_dst, 0xfffff);
	INSTR->codelen_src = codelen_src;
	INSTR->codelen_dst = codelen_dst;
	INSTR->proc = m32c_adc_size_srcdst;
	INSTR->proc();
}

/**
 *************************************************************
 * \fn void m32c_adcf_size_dst(void)
 * Add the carry flag to the destination.
 * v0
 *************************************************************
 */
static void
m32c_adcf_size_dst(void)
{
	uint32_t Dst, Result;
	int opsize = INSTR->opsize;
	INSTR->getdst(&Dst, M32C_INDEXSD());
	if (M32C_REG_FLG & M32C_FLG_CARRY) {
		Result = Dst + 1;
	} else {
		Result = Dst;
	}
	INSTR->setdst(Result, M32C_INDEXSD());
	add_flags(Dst, 0, Result, opsize);
	M32C_REG_PC += INSTR->codelen_dst;
	dbgprintf("m32c_adcf_size_dst not tested\n");
}

void
m32c_setup_adcf_size_dst(void)
{
	int dst;
	int opsize;
	int codelen_dst;
	dst = ((ICODE16() >> 6) & 3) | ((ICODE16() >> 7) & 0x1c);
	if (ICODE16() & 0x100) {
		opsize = 2;
	} else {
		opsize = 1;
		NotModOpsize(dst);
	}
	INSTR->opsize = opsize;
	INSTR->getdst = general_am_get(dst, opsize, &codelen_dst, 0xfffff);
	INSTR->setdst = general_am_set(dst, opsize, &codelen_dst, 0xfffff);
	INSTR->codelen_dst = codelen_dst;
	INSTR->proc = m32c_adcf_size_dst;
	INSTR->proc();
}

/**
 ********************************************************
 * \fn void m32c_adcf_size_idst(void)
 * Add the carry flag to an indirect destination.
 * v1
 ********************************************************
 */
static void
m32c_adcf_b_idst(void)
{
	uint32_t Dst, Result;
	uint32_t DstP;
	INSTR->getdstp(&DstP, 0);
	DstP = (DstP + M32C_INDEXSD()) & 0xffffff;
	Dst = M32C_Read8(DstP);
	if (M32C_REG_FLG & M32C_FLG_CARRY) {
		Result = Dst + 1;
	} else {
		Result = Dst;
	}
	M32C_Write8(Result, DstP);
	addb_flags(Dst, 0, Result);
	M32C_REG_PC += INSTR->codelen_dst;
	dbgprintf("m32c_adcf_size_idst not tested\n");
}

static void
m32c_adcf_w_idst(void)
{
	uint32_t Dst, Result;
	uint32_t DstP;
	INSTR->getdstp(&DstP, 0);
	DstP = (DstP + M32C_INDEXSD()) & 0xffffff;
	Dst = M32C_Read16(DstP);
	if (M32C_REG_FLG & M32C_FLG_CARRY) {
		Result = Dst + 1;
	} else {
		Result = Dst;
	}
	M32C_Write16(Result, DstP);
	addw_flags(Dst, 0, Result);
	M32C_REG_PC += INSTR->codelen_dst;
	dbgprintf("m32c_adcf_size_idst not tested\n");
}

void
m32c_setup_adcf_size_idst(void)
{
	int dst;
	int codelen_dst;
	dst = ((ICODE24() >> 6) & 3) | ((ICODE24() >> 7) & 0x1c);
	if (ICODE24() & 0x100) {
		INSTR->proc = m32c_adcf_w_idst;
	} else {
		INSTR->proc = m32c_adcf_b_idst;
	}
	INSTR->getdstp = general_am_get(dst, 4, &codelen_dst, 0xfffff);
	INSTR->codelen_dst = codelen_dst;
	INSTR->proc();
}

/**
 ****************************************************
 * \fn void m32c_add_size_g_immdst(void)
 * Add an immediate to a destination.
 * v1
 ****************************************************
 */
static void
m32c_add_size_g_immdst(void)
{
	uint32_t Src, Dst, Result;
	int opsize = INSTR->opsize;
	INSTR->getdst(&Dst, M32C_INDEXSD());
	if (INSTR->srcsize == 2) {
		Src = M32C_Read16(M32C_REG_PC + INSTR->codelen_dst);
	} else {		/* if (immsize == 1) */

		Src = M32C_Read8(M32C_REG_PC + INSTR->codelen_dst);
	}
	Result = Dst + Src;
	INSTR->setdst(Result, M32C_INDEXSD());
	add_flags(Dst, Src, Result, opsize);
	M32C_REG_PC += INSTR->codelen_dst + INSTR->srcsize;
	dbgprintf("m32c_add_size_g_immdst not tested\n");
}

void
m32c_setup_add_size_g_immdst(void)
{
	int dst;
	int opsize, immsize;
	int codelen_dst;
	dst = ((ICODE16() >> 6) & 3) | ((ICODE16() >> 7) & 0x1c);
	if (ICODE16() & 0x100) {
		opsize = immsize = 2;
	} else {
		opsize = immsize = 1;
		ModOpsize(dst, &opsize);
	}
	INSTR->opsize = opsize;
	INSTR->srcsize = immsize;
	INSTR->getdst = general_am_get(dst, opsize, &codelen_dst, 0xfffff);
	INSTR->setdst = general_am_set(dst, opsize, &codelen_dst, 0xfffff);
	INSTR->codelen_dst = codelen_dst;
	INSTR->proc = m32c_add_size_g_immdst;
	INSTR->proc();
}

/**
 ****************************************************
 * \fn void m32c_add_size_g_immidst(void)
 * Add an immediate to a indirect destination.
 * v1
 ****************************************************
 */
static void
m32c_add_size_g_immidst(void)
{
	uint32_t Src, Dst, Result;
	uint32_t DstP;
	INSTR->getdstp(&DstP, 0);
	DstP = (DstP + M32C_INDEXSD()) & 0xffffff;
	if (INSTR->opsize == 2) {
		Dst = M32C_Read16(DstP);
		Src = M32C_Read16(M32C_REG_PC + INSTR->codelen_dst);
		Result = Dst + Src;
		M32C_Write16(Result, DstP);
	} else {		/* if (size == 1) */

		Dst = M32C_Read8(DstP);
		Src = M32C_Read8(M32C_REG_PC + INSTR->codelen_dst);
		Result = Dst + Src;
		M32C_Write8(Result, DstP);
	}
	add_flags(Dst, Src, Result, INSTR->opsize);
	M32C_REG_PC += INSTR->codelen_dst + INSTR->opsize;
	dbgprintf("m32c_add_size_g_immdst not tested\n");
}

void
m32c_setup_add_size_g_immidst(void)
{
	int dst;
	int opsize;
	int codelen_dst;
	dst = ((ICODE24() >> 6) & 3) | ((ICODE24() >> 7) & 0x1c);
	if (ICODE24() & 0x100) {
		opsize = 2;
	} else {
		opsize = 1;
	}
	INSTR->opsize = opsize;
	INSTR->getdstp = general_am_get(dst, 4, &codelen_dst, 0xfffff);
	INSTR->codelen_dst = codelen_dst;
	INSTR->proc = m32c_add_size_g_immidst;
	INSTR->proc();
}

/**
 ****************************************************
 * \fn void m32c_add_size_g_immdst(void)
 * Add an 32 bit immediate to a 32Bit destination.
 * v1
 ****************************************************
 */
static void
m32c_add_l_g_immdst(void)
{
	uint32_t Src, Dst, Result;
	INSTR->getdst(&Dst, M32C_INDEXLD());
	Src = M32C_Read32((M32C_REG_PC + INSTR->codelen_dst) & 0xffffff);
	Result = Dst + Src;
	INSTR->setdst(Result, M32C_INDEXLD());
	addl_flags(Dst, Src, Result);
	M32C_REG_PC += INSTR->codelen_dst + 4;
	dbgprintf("m32c_add_l_g_immdst not tested\n");
}

void
m32c_setup_add_l_g_immdst(void)
{
	int dst;
	int codelen_dst;
	dst = ((ICODE16() >> 6) & 3) | ((ICODE16() >> 7) & 0x1c);
	INSTR->getdst = general_am_get(dst, 4, &codelen_dst, 0xcffff);
	INSTR->setdst = general_am_set(dst, 4, &codelen_dst, 0xcffff);
	INSTR->codelen_dst = codelen_dst;
	INSTR->proc = m32c_add_l_g_immdst;
	INSTR->proc();
}

/**
 ************************************************************
 * void m32c_add_l_g_immidst(void)
 * Add an 32 bit immediate to a 32Bit indirect destination.
 * v1
 ************************************************************
 */
static void
m32c_add_l_g_immidst(void)
{
	uint32_t Src, Dst, Result;
	uint32_t DstP;
	int size = 4;
	INSTR->getdstp(&DstP, 0);
	DstP = (DstP + M32C_INDEXLD()) & 0xffffff;
	Dst = M32C_Read32(DstP);
	Src = M32C_Read32((M32C_REG_PC + INSTR->codelen_dst) & 0xffffff);
	Result = Dst + Src;
	M32C_Write32(Result, DstP);
	add_flags(Dst, Src, Result, size);
	M32C_REG_PC += INSTR->codelen_dst + size;
	dbgprintf("m32c_add_l_g_immdst not tested\n");
}

void
m32c_setup_add_l_g_immidst(void)
{
	int dst;
	int codelen_dst;
	dst = ((ICODE24() >> 6) & 3) | ((ICODE24() >> 7) & 0x1c);
	INSTR->getdstp = general_am_get(dst, 4, &codelen_dst, 0xcffff);
	INSTR->codelen_dst = codelen_dst;
	INSTR->proc = m32c_add_l_g_immidst;
	INSTR->proc();
}

/**
 *******************************************************************
 * \fn void m32c_add_size_q_immdst(void)
 * Add a 4 bit signed immediate to a destination.
 * v1
 *******************************************************************
 */
static void
m32c_add_b_q_immdst(void)
{
	uint32_t Dst, Result;
	int32_t Src = INSTR->Imm32;
	INSTR->getdst(&Dst, 0);
	Result = Dst + Src;
	INSTR->setdst(Result, 0);
	addb_flags(Dst, Src, Result);
	M32C_REG_PC += INSTR->codelen_dst;
}

static void
m32c_add_w_q_immdst(void)
{
	uint32_t Dst, Result;
	int32_t Src = INSTR->Imm32;
	INSTR->getdst(&Dst, 0);
	Result = Dst + Src;
	INSTR->setdst(Result, 0);
	addw_flags(Dst, Src, Result);
	M32C_REG_PC += INSTR->codelen_dst;
}

static void
m32c_add_l_q_immdst(void)
{
	uint32_t Dst, Result;
	int32_t Src = INSTR->Imm32;
	INSTR->getdst(&Dst, 0);
	Result = Dst + Src;
	INSTR->setdst(Result, 0);
	addl_flags(Dst, Src, Result);
	M32C_REG_PC += INSTR->codelen_dst;
}

void
m32c_setup_add_size_q_immdst(void)
{
	int dst;
	int opsize;
	int codelen_dst;
	int32_t Src;
	dst = ((ICODE16() >> 6) & 3) | ((ICODE16() >> 7) & 0x1c);
	Src = ((int32_t) ((ICODE16() & 0xf) << 28)) >> 28;
	if ((ICODE16() & 0x1100) == 0x1000) {
		opsize = 4;
		INSTR->cycles += 1;
		INSTR->proc = m32c_add_l_q_immdst;
	} else if ((ICODE16() & 0x1100) == 0x0100) {
		opsize = 2;
		INSTR->proc = m32c_add_w_q_immdst;
	} else if ((ICODE16() & 0x1100) == 0x0000) {
		opsize = 1;
		ModOpsize(dst, &opsize);
		if (opsize == 2) {
			Src = Src & 0xff;
			INSTR->proc = m32c_add_w_q_immdst;
		} else {
			INSTR->proc = m32c_add_b_q_immdst;
		}
	} else {
		dbgprintf("Illegal size in add_size_q_immdst\n");
		opsize = 1;
		INSTR->proc = m32c_add_b_q_immdst;
	}
	INSTR->getdst = general_am_get(dst, opsize, &codelen_dst, 0xfffff);
	INSTR->setdst = general_am_set(dst, opsize, &codelen_dst, 0xfffff);
	INSTR->codelen_dst = codelen_dst;
	INSTR->opsize = opsize;
	INSTR->Imm32 = Src;
	INSTR->proc();
}

/**
 *******************************************************************
 * \fn void m32c_add_size_q_immidst(void)
 * Add a 4 bit signed immediate to an indirect destination.
 * v1
 *******************************************************************
 */
static void
m32c_add_size_q_immidst(void)
{
	uint32_t Dst, Result;
	uint32_t DstP;
	int32_t Src;
	Src = INSTR->Imm32;
	INSTR->getdstp(&DstP, 0);
	if (INSTR->opsize == 2) {
		Dst = M32C_Read16(DstP);
		Result = Dst + Src;
		M32C_Write16(Result, DstP);
	} else if (INSTR->opsize == 4) {
		Dst = M32C_Read32(DstP);
		Result = Dst + Src;
		M32C_Write32(Result, DstP);
	} else {
		Dst = M32C_Read8(DstP);
		Result = Dst + Src;
		M32C_Write8(Result, DstP);
	}
	add_flags(Dst, Src, Result, INSTR->opsize);
	M32C_REG_PC += INSTR->codelen_dst;
	dbgprintf("m32c_add_size_q_immidst not tested\n");
}

void
m32c_setup_add_size_q_immidst(void)
{
	int dst;
	int32_t Src;
	int opsize;
	int codelen_dst;
	Src = (int32_t) ((ICODE24() & 0xf) << 28) >> 28;
	dst = ((ICODE24() >> 6) & 3) | ((ICODE24() >> 7) & 0x1c);
	if ((ICODE24() & 0x1100) == 0x1000) {
		opsize = 4;
	} else if ((ICODE24() & 0x1100) == 0x0100) {
		opsize = 2;
	} else if ((ICODE24() & 0x1100) == 0x0000) {
		opsize = 1;
	} else {
		dbgprintf("Illegal size in add_size_q_immdst\n");
		opsize = 1;
	}
	INSTR->Imm32 = Src;
	INSTR->opsize = opsize;
	INSTR->getdstp = general_am_get(dst, 4, &codelen_dst, 0xfffff);
	INSTR->codelen_dst = codelen_dst;
	INSTR->proc = m32c_add_size_q_immidst;
	INSTR->proc();
}

/**
 *********************************************************
 * \fn void m32c_add_size_s_immdst(void)
 * Add an 8/16 Bit immediate to the destination
 * v1
 *********************************************************
 */
static void
m32c_add_size_s_immdst(void)
{
	uint32_t imm;
	uint32_t Dst;
	uint32_t Result;
	INSTR->getam2bit(&Dst, 0);
	if (INSTR->opsize == 2) {
		imm = M32C_Read16(M32C_REG_PC + INSTR->codelen_dst);
	} else {
		imm = M32C_Read8(M32C_REG_PC + INSTR->codelen_dst);
	}
	Result = Dst + imm;
	INSTR->setam2bit(Result, 0);
	add_flags(Dst, imm, Result, INSTR->opsize);
	M32C_REG_PC += INSTR->codelen_dst + INSTR->opsize;
	dbgprintf("m32c_add_size_s_immdst: am %d res %d \n", dst, Result);
}

void
m32c_setup_add_size_s_immdst(void)
{
	int dst = (ICODE8() >> 4) & 3;
	int codelen_dst;
	int size;
	if (ICODE8() & 1) {
		size = 2;
	} else {
		size = 1;
	}
	INSTR->getam2bit = am2bit_getproc(dst, &codelen_dst, size);
	INSTR->setam2bit = am2bit_setproc(dst, &codelen_dst, size);
	INSTR->codelen_dst = codelen_dst;
	INSTR->opsize = size;
	INSTR->proc = m32c_add_size_s_immdst;
	INSTR->proc();
}

/**
 *********************************************************
 * \fn void m32c_add_size_s_immidst(void)
 * Add an 8/16 Bit immediate to the indirect destination.
 * v1
 *********************************************************
 */
static void
m32c_add_size_s_immidst(void)
{
	uint32_t imm;
	uint32_t Dst;
	uint32_t DstP;
	uint32_t Result;
	INSTR->getam2bit(&DstP, 0);
	if (INSTR->opsize == 2) {
		Dst = M32C_Read16(DstP);
		imm = M32C_Read16(M32C_REG_PC + INSTR->codelen_dst);
		Result = Dst + imm;
		M32C_Write16(Result, DstP);
	} else {
		Dst = M32C_Read8(DstP);
		imm = M32C_Read8(M32C_REG_PC + INSTR->codelen_dst);
		Result = Dst + imm;
		M32C_Write8(Result, DstP);
	}
	add_flags(Dst, imm, Result, INSTR->opsize);
	M32C_REG_PC += INSTR->codelen_dst + INSTR->opsize;
	dbgprintf("m32c_add_size_s_immdst: am %d res %d \n", dst, Result);
}

void
m32c_setup_add_size_s_immidst(void)
{
	int dst = (ICODE16() >> 4) & 3;
	int codelen_dst;
	int opsize;
	if (ICODE16() & 1) {
		opsize = 2;
	} else {
		opsize = 1;
	}
	INSTR->getam2bit = am2bit_getproc(dst, &codelen_dst, opsize);
	INSTR->opsize = opsize;
	INSTR->codelen_dst = codelen_dst;
	INSTR->proc = m32c_add_size_s_immidst;
	INSTR->proc();
}

/**
 **************************************************************************
 * \fn void m32c_add_l_s_imm_a0a1(void)
 * add one or two to Addressregister A0/A1
 * v1
 **************************************************************************
 */
static void
m32c_add_l_s_imm_a0(void)
{
	uint32_t Dst;
	uint32_t Result;
	Dst = M32C_REG_A0;
	Result = Dst + INSTR->Imm32;
	M32C_REG_A0 = Result & 0xffffff;
	add_flags(Dst, INSTR->Imm32, Result, 4);
}

static void
m32c_add_l_s_imm_a1(void)
{
	uint32_t Dst;
	uint32_t Result;
	Dst = M32C_REG_A1;
	Result = Dst + INSTR->Imm32;
	M32C_REG_A1 = Result & 0xffffff;
	add_flags(Dst, INSTR->Imm32, Result, 4);
}

void
m32c_setup_add_l_s_imm_a0a1(void)
{
	int dst = ICODE8() & 1;
	if (ICODE8() & 0x20) {
		INSTR->Imm32 = 2;
	} else {
		INSTR->Imm32 = 1;
	}
	if (dst) {
		INSTR->proc = m32c_add_l_s_imm_a1;
	} else {
		INSTR->proc = m32c_add_l_s_imm_a0;
	}
	INSTR->proc();
}

/*
 **************************************************************
 * \fn void m32c_add_size_g_srcdst(void)
 * Add a src and a destination using general addressing modes.
 * v1
 **************************************************************
 */
static void
m32c_add_size_g_srcdst(void)
{
	uint32_t Src, Dst, Result;
	int opsize = INSTR->opsize;
	INSTR->getsrc(&Src, M32C_INDEXSS());
	M32C_REG_PC += INSTR->codelen_src;
	INSTR->getdst(&Dst, M32C_INDEXSD());
	Result = Dst + Src;
	INSTR->setdst(Result, M32C_INDEXSD());
	add_flags(Dst, Src, Result, opsize);
	M32C_REG_PC += INSTR->codelen_dst;
	dbgprintf("m32c_add_size_g_srcdst not tested\n");
}

void
m32c_setup_add_size_g_srcdst(void)
{
	int dst, src;
	int codelen_src;
	int codelen_dst;
	int srcsize, opsize;
	dst = ((ICODE16() >> 6) & 3) | ((ICODE16() >> 7) & 0x1c);
	src = ((ICODE16() >> 4) & 3) | ((ICODE16() >> 10) & 0x1c);
	if (ICODE16() & 0x100) {
		srcsize = opsize = 2;
	} else {
		srcsize = opsize = 1;
		ModOpsize(dst, &opsize);
	}
	INSTR->getsrc = general_am_get(src, srcsize, &codelen_src, 0xfffff);
	INSTR->getdst = general_am_get(dst, opsize, &codelen_dst, 0xfffff);
	INSTR->setdst = general_am_set(dst, opsize, &codelen_dst, 0xfffff);
	INSTR->codelen_src = codelen_src;
	INSTR->codelen_dst = codelen_dst;
	INSTR->opsize = opsize;
	INSTR->srcsize = srcsize;
	INSTR->proc = m32c_add_size_g_srcdst;
	INSTR->proc();
}

/*
 **************************************************************
 * \fn void m32c_add_size_g_isrcdst(void)
 * Add an indirect src and a destination 
 * using general addressing modes.
 * v1
 **************************************************************
 */
static void
m32c_add_size_g_isrcdst(void)
{
	uint32_t Src, Dst, Result;
	uint32_t SrcP;
	int opsize = INSTR->opsize;
	int srcsize = INSTR->srcsize;
	INSTR->getsrcp(&SrcP, 0);
	SrcP = (SrcP + M32C_INDEXSS()) & 0xffffff;
	if (srcsize == 1) {
		Src = M32C_Read8(SrcP);
	} else {
		Src = M32C_Read16(SrcP);
	}
	M32C_REG_PC += INSTR->codelen_src;
	INSTR->getdst(&Dst, M32C_INDEXSD());
	Result = Dst + Src;
	INSTR->setdst(Result, M32C_INDEXSD());
	add_flags(Dst, Src, Result, opsize);
	M32C_REG_PC += INSTR->codelen_dst;
	dbgprintf("m32c_add_size_g_srcdst not tested\n");
}

void
m32c_setup_add_size_g_isrcdst(void)
{
	int dst, src;
	int opsize, srcsize;
	int codelen_src;
	int codelen_dst;
	dst = ((ICODE24() >> 6) & 3) | ((ICODE24() >> 7) & 0x1c);
	src = ((ICODE24() >> 4) & 3) | ((ICODE24() >> 10) & 0x1c);
	if (ICODE24() & 0x100) {
		opsize = srcsize = 2;
	} else {
		opsize = srcsize = 1;
		ModOpsize(dst, &opsize);
	}
	INSTR->getdst = general_am_get(dst, opsize, &codelen_dst, 0xfffff);
	INSTR->getsrcp = general_am_get(src, 4, &codelen_src, 0xfffff);
	INSTR->setdst = general_am_set(dst, opsize, &codelen_dst, 0xfffff);
	INSTR->codelen_dst = codelen_dst;
	INSTR->codelen_src = codelen_src;
	INSTR->opsize = opsize;
	INSTR->srcsize = srcsize;
	INSTR->proc = m32c_add_size_g_isrcdst;
	INSTR->proc();
}

/**
 **************************************************************
 * \fn void m32c_add_size_g_srcidst(void)
 * Add a src and an indirect destination 
 * using general addressing modes.
 * v1
 **************************************************************
 */
static void
m32c_add_size_g_srcidst(void)
{
	uint32_t Src, Dst, Result;
	uint32_t DstP;
	int opsize = INSTR->opsize;
	INSTR->getsrc(&Src, M32C_INDEXSS());
	M32C_REG_PC += INSTR->codelen_src;
	INSTR->getdstp(&DstP, 0);
	DstP = (DstP + M32C_INDEXSD()) & 0xffffff;
	if (INSTR->opsize == 1) {
		Dst = M32C_Read8(DstP);
		Result = Dst + Src;
		M32C_Write8(Result, DstP);
	} else {
		Dst = M32C_Read16(DstP);
		Result = Dst + Src;
		M32C_Write16(Result, DstP);
	}
	add_flags(Dst, Src, Result, opsize);
	M32C_REG_PC += INSTR->codelen_dst;
	dbgprintf("m32c_add_size_g_srcdst not tested\n");
}

void
m32c_setup_add_size_g_srcidst(void)
{
	int dst, src;
	int size;
	int codelen_src;
	int codelen_dst;
	dst = ((ICODE24() >> 6) & 3) | ((ICODE24() >> 7) & 0x1c);
	src = ((ICODE24() >> 4) & 3) | ((ICODE24() >> 10) & 0x1c);
	if (ICODE24() & 0x100) {
		size = 2;
	} else {
		size = 1;
	}
	INSTR->getdstp = general_am_get(dst, 4, &codelen_dst, 0xfffff);
	INSTR->getsrc = general_am_get(src, size, &codelen_src, 0xfffff);
	INSTR->codelen_dst = codelen_dst;
	INSTR->codelen_src = codelen_src;
	INSTR->opsize = size;
	INSTR->proc = m32c_add_size_g_srcidst;
	INSTR->proc();
}

/**
 **************************************************************
 * \fn void m32c_add_size_g_isrcidst(void)
 * Add a indirect src and an indirect destination 
 * using general addressing modes.
 * v1
 **************************************************************
 */
static void
m32c_add_size_g_isrcidst(void)
{
	uint32_t Src, Dst, Result;
	uint32_t DstP, SrcP;
	int opsize = INSTR->opsize;
	INSTR->getsrcp(&SrcP, 0);
	SrcP = (SrcP + M32C_INDEXSS()) & 0xffffff;
	M32C_REG_PC += INSTR->codelen_src;
	INSTR->getdstp(&DstP, 0);
	DstP = (DstP + M32C_INDEXSD()) & 0xffffff;
	if (opsize == 1) {
		Src = M32C_Read8(SrcP);
		Dst = M32C_Read8(DstP);
		Result = Dst + Src;
		M32C_Write8(Result, DstP);
	} else {
		Src = M32C_Read16(SrcP);
		Dst = M32C_Read16(DstP);
		Result = Dst + Src;
		M32C_Write16(Result, DstP);
	}
	add_flags(Dst, Src, Result, opsize);
	M32C_REG_PC += INSTR->codelen_dst;
	dbgprintf("m32c_add_size_g_srcdst not tested\n");
}

void
m32c_setup_add_size_g_isrcidst(void)
{
	int dst, src;
	int size;
	int codelen_src;
	int codelen_dst;
	dst = ((ICODE24() >> 6) & 3) | ((ICODE24() >> 7) & 0x1c);
	src = ((ICODE24() >> 4) & 3) | ((ICODE24() >> 10) & 0x1c);
	if (ICODE24() & 0x100) {
		size = 2;
	} else {
		size = 1;
	}
	INSTR->getdstp = general_am_get(dst, 4, &codelen_dst, 0xfffff);
	INSTR->getsrcp = general_am_get(src, 4, &codelen_src, 0xfffff);
	INSTR->codelen_dst = codelen_dst;
	INSTR->codelen_src = codelen_src;
	INSTR->opsize = size;
	INSTR->proc = m32c_add_size_g_isrcidst;
	INSTR->proc();
}

/**
 *******************************************************************
 * \fn void m32c_add_l_g_srcdst(void)
 * Add a long src and a destination using general addressing modes.
 * v1
 *******************************************************************
 */
static void
m32c_add_l_g_srcdst(void)
{
	uint32_t Src, Dst, Result;
	int size = 4;
	INSTR->getsrc(&Src, M32C_INDEXLS());
	M32C_REG_PC += INSTR->codelen_src;
	INSTR->getdst(&Dst, M32C_INDEXLD());
	Result = Dst + Src;
	INSTR->setdst(Result, M32C_INDEXLD());
	add_flags(Dst, Src, Result, size);
	M32C_REG_PC += INSTR->codelen_dst;
	dbgprintf("m32c_add_l_g_srcdst not tested\n");
}

void
m32c_setup_add_l_g_srcdst(void)
{
	int dst, src;
	int size = 4;
	int codelen_src;
	int codelen_dst;
	dst = ((ICODE16() >> 6) & 3) | ((ICODE16() >> 7) & 0x1c);
	src = ((ICODE16() >> 4) & 3) | ((ICODE16() >> 10) & 0x1c);
	INSTR->getdst = general_am_get(dst, size, &codelen_dst, 0xcffff);
	INSTR->getsrc = general_am_get(src, size, &codelen_src, 0xcffff);
	INSTR->setdst = general_am_set(dst, size, &codelen_dst, 0xcffff);
	INSTR->codelen_src = codelen_src;
	INSTR->codelen_dst = codelen_dst;
	INSTR->opsize = 4;
	if (INSTR->nrMemAcc == 2) {
		INSTR->cycles = 6;
	}
	INSTR->proc = m32c_add_l_g_srcdst;
	INSTR->proc();
}

/**
 *******************************************************************
 * \fn void m32c_add_l_g_isrcdst(void)
 * Add a long indirect src and a destination using general 
 * addressing modes.
 * v1
 *******************************************************************
 */
static void
m32c_add_l_g_isrcdst(void)
{
	uint32_t Src, Dst, Result;
	uint32_t SrcP;
	INSTR->getsrcp(&SrcP, 0);
	SrcP = (SrcP + M32C_INDEXLS()) & 0xffffff;
	Src = M32C_Read32(SrcP);
	M32C_REG_PC += INSTR->codelen_src;
	INSTR->getdst(&Dst, M32C_INDEXLD());
	Result = Dst + Src;
	INSTR->setdst(Result, M32C_INDEXLD());
	add_flags(Dst, Src, Result, 4);
	M32C_REG_PC += INSTR->codelen_dst;
	dbgprintf("m32c_add_l_g_srcdst not tested\n");
}

void
m32c_setup_add_l_g_isrcdst(void)
{
	int dst, src;
	int size = 4;
	int codelen_src;
	int codelen_dst;
	dst = ((ICODE24() >> 6) & 3) | ((ICODE24() >> 7) & 0x1c);
	src = ((ICODE24() >> 4) & 3) | ((ICODE24() >> 10) & 0x1c);
	INSTR->getdst = general_am_get(dst, size, &codelen_dst, 0xcffff);
	INSTR->getsrcp = general_am_get(src, 4, &codelen_src, 0xcffff);
	INSTR->setdst = general_am_set(dst, size, &codelen_dst, 0xcffff);
	INSTR->codelen_dst = codelen_dst;
	INSTR->codelen_src = codelen_src;
	INSTR->opsize = 4;
	if (INSTR->nrMemAcc == 2) {
		INSTR->cycles = 9;
	}
	INSTR->proc = m32c_add_l_g_isrcdst;
	INSTR->proc();
}

/**
 *******************************************************************
 * \fn void m32c_add_l_g_srcidst(void)
 * Add a long src and an indirect destination using general 
 * addressing modes.
 * v1
 *******************************************************************
 */
static void
m32c_add_l_g_srcidst(void)
{
	uint32_t Src, Dst, Result;
	uint32_t DstP;
	int size = 4;
	INSTR->getsrc(&Src, M32C_INDEXLS());
	M32C_REG_PC += INSTR->codelen_src;
	INSTR->getdstp(&DstP, 0);
	DstP = (DstP + M32C_INDEXLD()) & 0xffffff;
	Dst = M32C_Read32(DstP);
	Result = Dst + Src;
	M32C_Write32(Result, DstP);
	add_flags(Dst, Src, Result, size);
	M32C_REG_PC += INSTR->codelen_dst;
	dbgprintf("m32c_add_l_g_srcdst not tested\n");
}

void
m32c_setup_add_l_g_srcidst(void)
{
	int dst, src;
	int size = 4;
	int codelen_src;
	int codelen_dst;
	dst = ((ICODE24() >> 6) & 3) | ((ICODE24() >> 7) & 0x1c);
	src = ((ICODE24() >> 4) & 3) | ((ICODE24() >> 10) & 0x1c);
	INSTR->getdstp = general_am_get(dst, 4, &codelen_dst, 0xcffff);
	INSTR->getsrc = general_am_get(src, size, &codelen_src, 0xcffff);
	INSTR->codelen_dst = codelen_dst;
	INSTR->codelen_src = codelen_src;
	INSTR->opsize = 4;
	if (INSTR->nrMemAcc == 2) {
		INSTR->cycles = 9;
	}
	INSTR->proc = m32c_add_l_g_srcidst;
	INSTR->proc();
}

/**
 *******************************************************************
 * \fn void m32c_add_l_g_srcidst(void)
 * Add a long indirect src and an indirect destination 
 * using general addressing modes.
 * v1
 *******************************************************************
 */
static void
m32c_add_l_g_isrcidst(void)
{
	uint32_t Src, Dst, Result;
	uint32_t SrcP, DstP;
	int size = 4;
	INSTR->getsrcp(&SrcP, 0);
	SrcP = (SrcP + M32C_INDEXLS()) & 0xffffff;
	Src = M32C_Read32(SrcP);
	M32C_REG_PC += INSTR->codelen_src;
	INSTR->getdstp(&DstP, 0);
	DstP = (DstP + M32C_INDEXLD()) & 0xffffff;
	Dst = M32C_Read32(DstP);
	Result = Dst + Src;
	M32C_Write32(Result, DstP);
	add_flags(Dst, Src, Result, size);
	M32C_REG_PC += INSTR->codelen_dst;
	dbgprintf("m32c_add_l_g_srcdst not tested\n");
}

void
m32c_setup_add_l_g_isrcidst(void)
{
	int dst, src;
	int codelen_src;
	int codelen_dst;
	dst = ((ICODE24() >> 6) & 3) | ((ICODE24() >> 7) & 0x1c);
	src = ((ICODE24() >> 4) & 3) | ((ICODE24() >> 10) & 0x1c);
	INSTR->getdstp = general_am_get(dst, 4, &codelen_dst, 0xcffff);
	INSTR->getsrcp = general_am_get(src, 4, &codelen_src, 0xcffff);
	INSTR->codelen_src = codelen_src;
	INSTR->codelen_dst = codelen_dst;
	INSTR->opsize = 4;
	if (INSTR->nrMemAcc == 2) {
		INSTR->cycles = 12;
	}
	INSTR->proc = m32c_add_l_g_isrcidst;
	INSTR->proc();
}

/**
 ******************************************************************
 * \fn void m32c_add_l_g_imm16sp(void);
 * Sign Extend a 16 Bit immediate to 32 Bit and add it to the
 * stack pointer. The flags are calculated based on the
 * 32 Bit result but only 24 bits are stored.
 * v1
 ******************************************************************
 */
static void
m32c_add_l_g_imm16sp(void)
{
	int32_t imm32;
	uint32_t Dst, Result;
	imm32 = (int32_t) (int16_t) M32C_Read16(M32C_REG_PC);
	Dst = M32C_REG_SP;
	Result = Dst + imm32;
	M32C_REG_SP = Result & 0xffffff;
	add_flags(Dst, imm32, Result, 4);
	M32C_REG_PC += 2;
}

void
m32c_setup_add_l_g_imm16sp(void)
{
	INSTR->proc = m32c_add_l_g_imm16sp;
	INSTR->proc();
}

/*
 *************************************************************
 * \fn void m32c_add_l_q_imm3sp(void)
 * Add an immediate from 1 to 8 to the stack pointer.
 * v1
 *************************************************************
 */
static void
m32c_add_l_q_imm3sp(void)
{
	uint32_t imm3;
	uint32_t Dst, Result;
	imm3 = INSTR->Imm32;
	Dst = M32C_REG_SP;
	Result = Dst + imm3;
	M32C_REG_SP = Result & 0xffffff;
	add_flags(Dst, imm3, Result, 4);
	dbgprintf("m32c_add_l_q_imm3sp not tested\n");
}

void
m32c_setup_add_l_q_imm3sp(void)
{
	uint32_t imm3;
	imm3 = (ICODE8() & 1) | ((ICODE8() >> 3) & 0x6);
	imm3 += 1;
	INSTR->Imm32 = imm3;
	INSTR->proc = m32c_add_l_q_imm3sp;
	INSTR->proc();
}

/**
 *************************************************************
 * \fn void m32c_add_l_s_imm8sp(void)
 * Add an immediate to the stack pointer. The immediate
 * is sign extended to 32 Bit. The lower 24 Bit are stored.
 * The flags are based on the 32 Bit result.
 * v1
 *************************************************************
 */
static void
m32c_add_l_s_imm8sp(void)
{
	int32_t imm32;
	uint32_t Dst, Result;
	imm32 = (int32_t) (int8_t) M32C_Read8(M32C_REG_PC);
	Dst = M32C_REG_SP;
	Result = Dst + imm32;
	M32C_REG_SP = Result & 0xffffff;
	add_flags(Dst, imm32, Result, 4);
	M32C_REG_PC += 1;
	dbgprintf("m32c_add_l_s_imm8sp not tested\n");
}

void
m32c_setup_add_l_s_imm8sp(void)
{
	INSTR->proc = m32c_add_l_s_imm8sp;
	INSTR->proc();
}

/**
 **************************************************************
 * \fn void m32c_addx_immdst(void)
 * Sign extend an 8 Bit immediate from memory and add 
 * it to a 32 Bit destination.
 * v1
 **************************************************************
 */
static void
m32c_addx_immdst(void)
{
	uint32_t Dst, Result;
	int32_t Src;
	int opsize = 4;
	INSTR->getdst(&Dst, 0);
	Src = (int32_t) (int8_t) M32C_Read8(M32C_REG_PC + INSTR->codelen_dst);
	Result = Dst + Src;
	INSTR->setdst(Result, 0);
	add_flags(Dst, Src, Result, opsize);
	M32C_REG_PC += INSTR->codelen_dst + 1;
	dbgprintf("m32c_addx_immdst not tested\n");
}

void
m32c_setup_addx_immdst(void)
{
	int dst;
	int opsize = 4;
	int codelen_dst;
	dst = ((ICODE16() >> 6) & 3) | ((ICODE16() >> 7) & 0x1c);
	INSTR->getdst = general_am_get(dst, opsize, &codelen_dst, 0xcffff);
	INSTR->setdst = general_am_set(dst, opsize, &codelen_dst, 0xcffff);
	INSTR->codelen_dst = codelen_dst;
	INSTR->proc = m32c_addx_immdst;
	INSTR->proc();
}

/**
 *************************************************************************
 * \fn void m32c_addx_immidst(void)
 * Sign extend an 8 Bit immediate from memory and add 
 * it to an indirect 32 Bit destination.
 * v1
 *************************************************************************
 */
static void
m32c_addx_immidst(void)
{
	uint32_t Dst, Result;
	uint32_t DstP;
	int32_t Src;
	int size = 4;
	INSTR->getdstp(&DstP, 0);
	Dst = M32C_Read32(DstP);
	Src = (int32_t) (int8_t) M32C_Read8(M32C_REG_PC + INSTR->codelen_dst);
	Result = Dst + Src;
	M32C_Write32(Result, DstP);
	add_flags(Dst, Src, Result, size);
	M32C_REG_PC += INSTR->codelen_dst + 1;
	dbgprintf("m32c_addx_immidst not tested\n");
}

void
m32c_setup_addx_immidst(void)
{
	int dst;
	int codelen_dst;
	dst = ((ICODE16() >> 6) & 3) | ((ICODE16() >> 7) & 0x1c);
	INSTR->getdstp = general_am_get(dst, 4, &codelen_dst, 0xcffff);
	INSTR->codelen_dst = codelen_dst;
	INSTR->opsize = 4;
	INSTR->proc = m32c_addx_immidst;
	INSTR->proc();
}

/**
 ********************************************************************
 * \fn void m32c_addx_srcdst(void)
 * Add a 8 Bit value sign extended to 32 Bit to a 32 Bit destination.
 * v1
 ********************************************************************
 */
static void
m32c_addx_srcdst(void)
{
	uint32_t Dst, Result;
	uint32_t Src;
	int size_dst = 4;
	INSTR->getsrc(&Src, 0);
	M32C_REG_PC += INSTR->codelen_src;
	INSTR->getdst(&Dst, 0);
	Src = (int32_t) (int8_t) Src;
	Result = Dst + Src;
	INSTR->setdst(Result, 0);
	add_flags(Dst, Src, Result, size_dst);
	M32C_REG_PC += INSTR->codelen_dst;
	dbgprintf("m32c_addx_srcdst not tested\n");
}

void
m32c_setup_addx_srcdst(void)
{
	int dst, src;
	int codelen_src;
	int codelen_dst;
	int size_src = 1;
	int size_dst = 4;
	dst = ((ICODE16() >> 6) & 3) | ((ICODE16() >> 7) & 0x1c);
	src = ((ICODE16() >> 4) & 3) | ((ICODE16() >> 10) & 0x1c);
	INSTR->getdst = general_am_get(dst, size_dst, &codelen_dst, 0xcffff);
	INSTR->getsrc = general_am_get(src, size_src, &codelen_src, 0xfffff);
	INSTR->setdst = general_am_set(dst, size_dst, &codelen_dst, 0xcffff);
	INSTR->codelen_dst = codelen_dst;
	INSTR->codelen_src = codelen_src;
	if (INSTR->nrMemAcc == 2) {
		INSTR->cycles = 6;
	}
	INSTR->proc = m32c_addx_srcdst;
	INSTR->proc();
}

/**
 *******************************************************************************
 * \fn void m32c_addx_isrcdst(void)
 * Add an indirect immediate after sign extension from 8 to 32 bit to a
 * destination. 
 * v1
 *******************************************************************************
 */
static void
m32c_addx_isrcdst(void)
{
	uint32_t Dst, Result;
	uint32_t Src;
	uint32_t SrcP;
	int size_dst = 4;
	INSTR->getsrcp(&SrcP, 0);
	Src = (int32_t) (int8_t) M32C_Read8(SrcP);
	M32C_REG_PC += INSTR->codelen_src;
	INSTR->getdst(&Dst, 0);
	Result = Dst + Src;
	INSTR->setdst(Result, 0);
	add_flags(Dst, Src, Result, size_dst);
	M32C_REG_PC += INSTR->codelen_dst;
	dbgprintf("m32c_addx_isrcdst not tested\n");
}

void
m32c_setup_addx_isrcdst(void)
{
	int dst, src;
	int size_dst = 4;
	int codelen_src;
	int codelen_dst;
	dst = ((ICODE24() >> 6) & 3) | ((ICODE24() >> 7) & 0x1c);
	src = ((ICODE24() >> 4) & 3) | ((ICODE24() >> 10) & 0x1c);
	INSTR->getdst = general_am_get(dst, size_dst, &codelen_dst, 0xcffff);
	INSTR->getsrcp = general_am_get(src, 4, &codelen_src, 0xfffff);
	INSTR->setdst = general_am_set(dst, size_dst, &codelen_dst, 0xcffff);
	INSTR->codelen_dst = codelen_dst;
	INSTR->codelen_src = codelen_src;
	if (INSTR->nrMemAcc == 2) {
		INSTR->cycles = 9;
	}
	INSTR->proc = m32c_addx_isrcdst;
	INSTR->proc();
}

/**
 ***********************************************************************************
 * \fn void m32c_addx_srcidst(void)
 * Add a Src after sign extension from 8 to 32 Bit to an indirect destination.
 * v1
 ***********************************************************************************
 */
static void
m32c_addx_srcidst(void)
{
	uint32_t Dst, Result;
	uint32_t DstP;
	uint32_t Src;
	INSTR->getsrc(&Src, 0);
	Src = (int32_t) (int8_t) Src;
	M32C_REG_PC += INSTR->codelen_src;
	INSTR->getdstp(&DstP, 0);
	Dst = M32C_Read32(DstP);
	Result = Dst + Src;
	M32C_Write32(Result, DstP);
	add_flags(Dst, Src, Result, 4);
	M32C_REG_PC += INSTR->codelen_dst;
	dbgprintf("m32c_addx_srcidst not tested\n");
}

void
m32c_setup_addx_srcidst(void)
{
	int dst, src;
	int size_src = 1;
	int codelen_src;
	int codelen_dst;
	dst = ((ICODE24() >> 6) & 3) | ((ICODE24() >> 7) & 0x1c);
	src = ((ICODE24() >> 4) & 3) | ((ICODE24() >> 10) & 0x1c);
	INSTR->getdstp = general_am_get(dst, 4, &codelen_dst, 0xcffff);
	INSTR->getsrc = general_am_get(src, size_src, &codelen_src, 0xfffff);
	INSTR->codelen_dst = codelen_dst;
	INSTR->codelen_src = codelen_src;
	if (INSTR->nrMemAcc == 2) {
		INSTR->cycles = 9;
	}
	INSTR->proc = m32c_addx_srcidst;
	INSTR->proc();
}

/**
 ***********************************************************************************
 * \fn void m32c_addx_isrcidst(void)
 * Add an indirect Src after sign extension from 8 to 32 Bit to an 
 * indirect destination.
 * v1
 ***********************************************************************************
 */
static void
m32c_addx_isrcidst(void)
{
	uint32_t Dst, Result;
	uint32_t DstP;
	uint32_t Src;
	uint32_t SrcP;
	INSTR->getsrcp(&SrcP, 0);
	Src = (int32_t) (int8_t) M32C_Read8(SrcP);
	M32C_REG_PC += INSTR->codelen_src;
	INSTR->getdstp(&DstP, 0);
	Dst = M32C_Read32(DstP);
	Result = Dst + Src;
	M32C_Write32(Result, DstP);
	add_flags(Dst, Src, Result, 4);
	M32C_REG_PC += INSTR->codelen_dst;
	dbgprintf("m32c_addx_isrcidst not tested\n");
}

void
m32c_setup_addx_isrcidst(void)
{
	int dst, src;
	int codelen_src;
	int codelen_dst;
	dst = ((ICODE24() >> 6) & 3) | ((ICODE24() >> 7) & 0x1c);
	src = ((ICODE24() >> 4) & 3) | ((ICODE24() >> 10) & 0x1c);
	INSTR->getdstp = general_am_get(dst, 4, &codelen_dst, 0xcffff);
	INSTR->getsrcp = general_am_get(src, 4, &codelen_src, 0xfffff);
	INSTR->codelen_src = codelen_src;
	INSTR->codelen_dst = codelen_dst;
	if (INSTR->nrMemAcc == 2) {
		INSTR->cycles = 12;
	}
	INSTR->proc = m32c_addx_isrcidst;
	INSTR->proc();
}

/**
 *****************************************************************
 * void m32c_adjnz_size_immdstlbl(void)
 * Addition then jump on not zero.
 * v1
 *****************************************************************
 */
static void
m32c_adjnz_size_immdstlbl(void)
{
	uint32_t Dst, Result;
	int32_t Src;
	int opsize = INSTR->opsize;
	Src = INSTR->Imm32;
	INSTR->getdst(&Dst, 0);
	Result = Dst + Src;
	INSTR->setdst(Result, 0);
	add_flags(Dst, Src, Result, opsize);

	if (M32C_REG_FLG & M32C_FLG_ZERO) {
		M32C_REG_PC += INSTR->codelen_dst + 1;
	} else {
		int8_t dsp = M32C_Read8(M32C_REG_PC + INSTR->codelen_dst);
		M32C_REG_PC = (M32C_REG_PC + dsp) & 0xffffff;
		CycleCounter += 2;
	}
	dbgprintf("m32c_adjnz_size_immdstlbl not tested\n");
}

void
m32c_setup_adjnz_size_immdstlbl(void)
{
	int dst;
	int size;
	int codelen_dst;
	int32_t Src;
	dst = ((ICODE16() >> 6) & 3) | ((ICODE16() >> 7) & 0x1c);
	Src = ((int32_t) ((ICODE16() & 0xf) << 28)) >> 28;
	if (ICODE16() & 0x100) {
		size = 2;
	} else {
		size = 1;
		NotModOpsize(dst);
	}
	INSTR->getdst = general_am_get(dst, size, &codelen_dst, 0xfffff);
	INSTR->setdst = general_am_set(dst, size, &codelen_dst, 0xfffff);
	INSTR->codelen_dst = codelen_dst;
	INSTR->opsize = size;
	INSTR->Imm32 = Src;
	INSTR->proc = m32c_adjnz_size_immdstlbl;
	INSTR->proc();
}

/**
 ******************************************************************
 * \fn void m32c_and_size_immdst(void)
 * Calculate AND from an immediate and a destination.
 * v1
 ******************************************************************
 */
static void
m32c_and_size_immdst(void)
{
	uint32_t Src, Dst, Result;
	int srcsize = INSTR->srcsize;
	INSTR->getdst(&Dst, M32C_INDEXSD());
	if (srcsize == 2) {
		Src = M32C_Read16(M32C_REG_PC + INSTR->codelen_dst);
	} else {		/* if(INSTR-> srcsize == 1) */

		Src = M32C_Read8(M32C_REG_PC + INSTR->codelen_dst);
	}
	Result = Dst & Src;
	INSTR->setdst(Result, M32C_INDEXSD());
	and_flags(Result, INSTR->opsize);
	M32C_REG_PC += INSTR->codelen_dst + INSTR->srcsize;
	dbgprintf("m32c_and_size_immdst not tested\n");
}

void
m32c_setup_and_size_immdst(void)
{
	int dst;
	int srcsize, opsize;
	int codelen_dst;
	dst = ((ICODE16() >> 6) & 3) | ((ICODE16() >> 7) & 0x1c);
	if (ICODE16() & 0x100) {
		srcsize = opsize = 2;
	} else {
		srcsize = opsize = 1;
		ModOpsize(dst, &opsize);
	}
	INSTR->getdst = general_am_get(dst, opsize, &codelen_dst, 0xfffff);
	INSTR->setdst = general_am_set(dst, opsize, &codelen_dst, 0xfffff);
	INSTR->codelen_dst = codelen_dst;
	INSTR->opsize = opsize;
	INSTR->srcsize = srcsize;
	INSTR->proc = m32c_and_size_immdst;
	INSTR->proc();
}

/**
 ******************************************************************
 * \fn void m32c_and_size_immidst(void)
 * Calculate the logical and of an immediate and an indirect
 * destination. 
 * v1
 ******************************************************************
 */
static void
m32c_and_size_immidst(void)
{
	uint32_t Src, Dst, Result;
	uint32_t DstP;
	INSTR->getdstp(&DstP, 0);
	DstP = (DstP + M32C_INDEXSD()) & 0xffffff;
	if (INSTR->opsize == 2) {
		Dst = M32C_Read16(DstP);
		Src = M32C_Read16(M32C_REG_PC + INSTR->codelen_dst);
	} else {		/* if(INSTR->opsize == 1) */

		Dst = M32C_Read8(DstP);
		Src = M32C_Read8(M32C_REG_PC + INSTR->codelen_dst);
	}
	Result = Dst & Src;
	if (INSTR->opsize == 2) {
		M32C_Write16(Result, DstP);
	} else {
		M32C_Write8(Result, DstP);
	}
	and_flags(Result, INSTR->opsize);
	M32C_REG_PC += INSTR->codelen_dst + INSTR->opsize;
	dbgprintf("m32c_and_size_immidst not tested\n");
}

void
m32c_setup_and_size_immidst(void)
{
	int dst;
	int size;
	int codelen_dst;
	dst = ((ICODE24() >> 6) & 3) | ((ICODE24() >> 7) & 0x1c);
	if (ICODE24() & 0x100) {
		size = 2;
	} else {
		size = 1;
	}
	INSTR->getdstp = general_am_get(dst, 4, &codelen_dst, 0xfffff);
	INSTR->codelen_dst = codelen_dst;
	INSTR->opsize = size;
	INSTR->proc = m32c_and_size_immidst;
	INSTR->proc();
}

/**
 ****************************************************************
 * \fn void m32c_and_size_s_immdst(void)
 * Calculate the logical and of an immediate and a destination.
 * v1
 ****************************************************************
 */
static void
m32c_and_size_s_immdst(void)
{
	uint32_t imm;
	uint32_t Dst;
	uint32_t Result;
	INSTR->getam2bit(&Dst, M32C_INDEXSD());
	if (INSTR->opsize == 2) {
		imm = M32C_Read16(M32C_REG_PC + INSTR->codelen_dst);
	} else {
		imm = M32C_Read8(M32C_REG_PC + INSTR->codelen_dst);
	}
	Result = Dst & imm;
	INSTR->setam2bit(Result, M32C_INDEXSD());
	and_flags(Result, INSTR->opsize);
	M32C_REG_PC += INSTR->codelen_dst + INSTR->opsize;
	dbgprintf("m32c_and_size_s_immdst not tested\n");
}

void
m32c_setup_and_size_s_immdst(void)
{
	int size;
	int codelen_dst;
	int am = (ICODE8() >> 4) & 3;
	if (ICODE8() & 1) {
		size = 2;
	} else {
		size = 1;
	}
	INSTR->getam2bit = am2bit_getproc(am, &codelen_dst, size);
	INSTR->setam2bit = am2bit_setproc(am, &codelen_dst, size);
	INSTR->codelen_dst = codelen_dst;
	INSTR->opsize = size;
	INSTR->proc = m32c_and_size_s_immdst;
	INSTR->proc();
}

/**
 ****************************************************************
 * \fn void m32c_and_size_s_immidst(void)
 * Calculate the logical and of an immediate and an
 * indirect destination.
 * v1
 ****************************************************************
 */
static void
m32c_and_size_s_immidst(void)
{
	uint32_t imm;
	uint32_t Dst;
	uint32_t DstP;
	uint32_t Result;
	INSTR->getam2bit(&DstP, 0);
	DstP = (DstP + M32C_INDEXSD()) & 0xffffff;
	if (INSTR->opsize == 2) {
		Dst = M32C_Read16(DstP);
		imm = M32C_Read16(M32C_REG_PC + INSTR->codelen_dst);
	} else {
		Dst = M32C_Read8(DstP);
		imm = M32C_Read8(M32C_REG_PC + INSTR->codelen_dst);
	}
	Result = Dst & imm;
	if (INSTR->opsize == 2) {
		M32C_Write16(Result, DstP);
	} else {
		M32C_Write8(Result, DstP);
	}
	and_flags(Result, INSTR->opsize);
	M32C_REG_PC += INSTR->codelen_dst + INSTR->opsize;
	dbgprintf("m32c_and_size_s_immidst not tested\n");
}

void
m32c_setup_and_size_s_immidst(void)
{
	int size;
	int codelen_dst;
	int am = (ICODE16() >> 4) & 3;
	if (ICODE16() & 1) {
		size = 2;
	} else {
		size = 1;
	}
	INSTR->getam2bit = am2bit_getproc(am, &codelen_dst, 4);
	INSTR->codelen_dst = codelen_dst;
	INSTR->opsize = size;
	INSTR->proc = m32c_and_size_s_immidst;
	INSTR->proc();
}

/**
 *****************************************************************
 * \fn void m32c_and_size_g_srcdst(void)
 * Calculate the logical and of a src and a destination.
 * v1
 *****************************************************************
 */
static void
m32c_and_size_g_srcdst(void)
{
	uint32_t Src, Dst, Result;
	int opsize = INSTR->opsize;
	INSTR->getsrc(&Src, M32C_INDEXSS());
	M32C_REG_PC += INSTR->codelen_src;
	INSTR->getdst(&Dst, M32C_INDEXSD());
	Result = Dst & Src;
	INSTR->setdst(Result, M32C_INDEXSD());
	and_flags(Result, opsize);
	M32C_REG_PC += INSTR->codelen_dst;
}

void
m32c_setup_and_size_g_srcdst(void)
{
	int dst, src;
	int opsize, srcsize;
	int codelen_src;
	int codelen_dst;
	dst = ((ICODE16() >> 6) & 3) | ((ICODE16() >> 7) & 0x1c);
	src = ((ICODE16() >> 4) & 3) | ((ICODE16() >> 10) & 0x1c);
	if (ICODE16() & 0x100) {
		srcsize = opsize = 2;
	} else {
		srcsize = opsize = 1;
		ModOpsize(dst, &opsize);
	}
	INSTR->getdst = general_am_get(dst, opsize, &codelen_dst, 0xfffff);
	INSTR->getsrc = general_am_get(src, srcsize, &codelen_src, 0xfffff);
	INSTR->setdst = general_am_set(dst, opsize, &codelen_dst, 0xfffff);
	INSTR->codelen_dst = codelen_dst;
	INSTR->codelen_src = codelen_src;
	INSTR->opsize = opsize;
	INSTR->srcsize = srcsize;
	INSTR->proc = m32c_and_size_g_srcdst;
	INSTR->proc();
}

/**
 *****************************************************************
 * \fn void m32c_and_size_g_isrcdst(void)
 * Calculate the logical and of a indirect src and a destination.
 * v1
 *****************************************************************
 */
static void
m32c_and_size_g_isrcdst(void)
{
	uint32_t SrcP;
	uint32_t Src, Dst, Result;
	int opsize = INSTR->opsize;
	int srcsize = INSTR->srcsize;
	INSTR->getsrcp(&SrcP, 0);
	SrcP = (SrcP + M32C_INDEXSS()) & 0xffffff;
	M32C_REG_PC += INSTR->codelen_src;
	INSTR->getdst(&Dst, M32C_INDEXSD());
	if (srcsize == 2) {
		Src = M32C_Read16(SrcP);
	} else {
		Src = M32C_Read8(SrcP);
	}
	Result = Dst & Src;
	INSTR->setdst(Result, M32C_INDEXSD());
	and_flags(Result, opsize);
	M32C_REG_PC += INSTR->codelen_dst;
	dbgprintf("m32c_and_size_g_isrcdst not tested\n");
}

void
m32c_setup_and_size_g_isrcdst(void)
{
	int dst, src;
	int srcsize, opsize;
	int codelen_src;
	int codelen_dst;
	dst = ((ICODE24() >> 6) & 3) | ((ICODE24() >> 7) & 0x1c);
	src = ((ICODE24() >> 4) & 3) | ((ICODE24() >> 10) & 0x1c);
	if (ICODE24() & 0x100) {
		srcsize = opsize = 2;
	} else {
		srcsize = opsize = 1;
		ModOpsize(dst, &opsize);
	}
	INSTR->getdst = general_am_get(dst, opsize, &codelen_dst, 0xfffff);
	INSTR->getsrcp = general_am_get(src, 4, &codelen_src, 0xfffff);
	INSTR->setdst = general_am_set(dst, opsize, &codelen_dst, 0xfffff);
	INSTR->codelen_src = codelen_src;
	INSTR->codelen_dst = codelen_dst;
	INSTR->opsize = opsize;
	INSTR->srcsize = srcsize;
	INSTR->proc = m32c_and_size_g_isrcdst;
	INSTR->proc();
}

/**
 ********************************************************************
 * \fn void m32c_and_size_g_srcidst(void)
 * Calculate the logical and of a source and a indirect destination.
 * v1
 ********************************************************************
 */
static void
m32c_and_size_g_srcidst(void)
{
	uint32_t Src, Dst, Result;
	uint32_t DstP;
	int opsize = INSTR->opsize;
	INSTR->getsrc(&Src, M32C_INDEXSS());
	M32C_REG_PC += INSTR->codelen_src;
	INSTR->getdstp(&DstP, 0);
	DstP = (DstP + M32C_INDEXSD()) & 0xffffff;
	if (INSTR->opsize == 2) {
		Dst = M32C_Read16(DstP);
	} else {
		Dst = M32C_Read8(DstP);
	}
	Result = Dst & Src;
	if (opsize == 2) {
		M32C_Write16(Result, DstP);
	} else {
		M32C_Write8(Result, DstP);
	}
	and_flags(Result, opsize);
	M32C_REG_PC += INSTR->codelen_dst;
	dbgprintf("m32c_and_size_g_srcidst not tested\n");
}

void
m32c_setup_and_size_g_srcidst(void)
{
	int dst, src;
	int size;
	int codelen_src;
	int codelen_dst;
	dst = ((ICODE24() >> 6) & 3) | ((ICODE24() >> 7) & 0x1c);
	src = ((ICODE24() >> 4) & 3) | ((ICODE24() >> 10) & 0x1c);
	if (ICODE24() & 0x100) {
		size = 2;
	} else {
		size = 1;
	}
	INSTR->getdstp = general_am_get(dst, 4, &codelen_dst, 0xfffff);
	INSTR->getsrc = general_am_get(src, size, &codelen_src, 0xfffff);
	INSTR->codelen_src = codelen_src;
	INSTR->codelen_dst = codelen_dst;
	INSTR->opsize = size;
	INSTR->proc = m32c_and_size_g_srcidst;
	INSTR->proc();
}

/**
 ********************************************************************
 * \fn void m32c_and_size_g_isrcidst(void)
 * Calculate the logical and of an indirect source and 
 * an indirect destination.
 * v1
 ********************************************************************
 */
static void
m32c_and_size_g_isrcidst(void)
{
	uint32_t Src, Dst, Result;
	uint32_t DstP, SrcP;
	INSTR->getsrcp(&SrcP, 0);
	M32C_REG_PC += INSTR->codelen_src;
	INSTR->getdstp(&DstP, 0);
	SrcP = (SrcP + M32C_INDEXSS()) & 0xffffff;
	DstP = (DstP + M32C_INDEXSD()) & 0xffffff;
	if (INSTR->opsize == 2) {
		Dst = M32C_Read16(DstP);
		Src = M32C_Read16(SrcP);
		Result = Dst & Src;
		M32C_Write16(Result, DstP);
	} else {
		Dst = M32C_Read8(DstP);
		Src = M32C_Read8(SrcP);
		Result = Dst & Src;
		M32C_Write8(Result, DstP);
	}
	and_flags(Result, INSTR->opsize);
	M32C_REG_PC += INSTR->codelen_dst;
	dbgprintf("m32c_and_size_g_isrcidst not tested\n");
}

void
m32c_setup_and_size_g_isrcidst(void)
{
	int dst, src;
	int size;
	int codelen_src;
	int codelen_dst;
	dst = ((ICODE24() >> 6) & 3) | ((ICODE24() >> 7) & 0x1c);
	src = ((ICODE24() >> 4) & 3) | ((ICODE24() >> 10) & 0x1c);
	if (ICODE24() & 0x100) {
		size = 2;
	} else {
		size = 1;
	}
	INSTR->getdstp = general_am_get(dst, 4, &codelen_dst, 0xfffff);
	INSTR->getsrcp = general_am_get(src, 4, &codelen_src, 0xfffff);
	INSTR->codelen_src = codelen_src;
	INSTR->codelen_dst = codelen_dst;
	INSTR->opsize = size;
	INSTR->proc = m32c_and_size_g_isrcidst;
	INSTR->proc();
}

/**
 **************************************************************
 * \fn void m32c_band_src(void)
 * Logically and of a src and the carry is stored in carry. 
 * v1
 **************************************************************
 */
static void
m32c_band_src(void)
{
	int bit;
	uint32_t data;
	if (M32C_BITINDEX() >= 0) {
		bit = M32C_BITINDEX() & 7;
		CycleCounter += 1;
	} else {
		bit = INSTR->Arg2;
	}
	data = INSTR->getbit(M32C_BITINDEX());
	if (!(data & (1 << bit))) {
		M32C_REG_FLG &= ~M32C_FLG_CARRY;
	}
	M32C_REG_PC += INSTR->codelen_src;
}

void
m32c_setup_band_src(void)
{
	int codelen_src;
	int src = ((ICODE24() >> 6) & 3) | ((ICODE24() >> 7) & 0x1c);
	int bit = ICODE24() & 7;
	INSTR->Arg1 = src;
	INSTR->Arg2 = bit;
	INSTR->getbit = get_bitaddrproc(src, &codelen_src);
	INSTR->codelen_src = codelen_src;
	INSTR->proc = m32c_band_src;
	INSTR->proc();
}

/** 
 *********************************************************************
 * \fn void m32c_bclr(void)
 * Bit clear.
 * v1
 *********************************************************************
 */
static void
m32c_bclr(void)
{
	int bit;
	uint32_t data;
	data = INSTR->getbit(M32C_BITINDEX());
	if (M32C_BITINDEX() >= 0) {
		CycleCounter += 1;
		bit = M32C_BITINDEX() & 7;
	} else {
		bit = INSTR->Arg2;
	}
	data &= ~(1 << bit);
	INSTR->setbit(data, M32C_BITINDEX());
	M32C_REG_PC += INSTR->codelen_dst;
	CycleCounter += 2;	/* Should check if memory is in IO area before */
}

void
m32c_setup_bclr(void)
{
	int codelen;
	int dst = ((ICODE16() >> 6) & 3) | ((ICODE16() >> 7) & 0x1c);
	int bit = ICODE16() & 7;
	INSTR->getbit = get_bitaddrproc(dst, &codelen);
	INSTR->setbit = set_bitaddrproc(dst);
	INSTR->codelen_dst = codelen;
	INSTR->Arg1 = dst;
	INSTR->Arg2 = bit;
	INSTR->proc = m32c_bclr;
	INSTR->proc();
}

/**
 ****************************************************************
 * \fn void m32c_bitindex(void)
 * Store a bit index for the next instruction.
 * v1
 ****************************************************************
 */
static void
m32c_bitindex(void)
{
	uint32_t Src;
	INSTR->getsrc(&Src, 0);
	gm32c.bitindex = Src;
	M32C_REG_PC += INSTR->codelen_src;
	M32C_PostSignal(M32C_SIG_INHIBIT_IRQ | M32C_SIG_DELETE_INDEX);
}

void
m32c_setup_bitindex(void)
{
	int size;
	int codelen_src;
	int src = ((ICODE16() >> 6) & 3) | ((ICODE16() >> 7) & 0x1c);
	if (ICODE16() & 0x100) {
		size = 2;
	} else {
		size = 1;
	}
	INSTR->getsrc = general_am_get(src, size, &codelen_src, 0xfffff);
	INSTR->codelen_src = codelen_src;
	INSTR->opsize = size;
	INSTR->proc = m32c_bitindex;
	INSTR->proc();
}

/**
 ******************************************************************
 * \fn void m32c_bmcnd_dst(void)
 * Set or Clear a Bit depending on a condition.
 * v1
 ******************************************************************
 */
static void
m32c_bmcnd_dst(void)
{
	int bit;
	int cnd;
	uint32_t data;
	if (M32C_BITINDEX() >= 0) {
		CycleCounter += 1;
		bit = M32C_BITINDEX() & 7;
	} else {
		bit = INSTR->Arg1;
	}
	data = INSTR->getbit(M32C_BITINDEX());
	cnd = M32C_Read8(M32C_REG_PC + INSTR->codelen_dst) & 0xf;
	if (check_condition(cnd)) {
		data |= (1 << bit);
	} else {
		data &= ~(1 << bit);
	}
	INSTR->setbit(data, M32C_BITINDEX());
	M32C_REG_PC += INSTR->codelen_dst + 1;
	dbgprintf("m32c_bmcnd_dst not tested\n");
}

void
m32c_setup_bmcnd_dst(void)
{
	int dst = ((ICODE16() >> 6) & 3) | ((ICODE16() >> 7) & 0x1c);
	int codelen;
	int bit = ICODE16() & 7;
	INSTR->Arg1 = bit;
	INSTR->getbit = get_bitaddrproc(dst, &codelen);
	INSTR->setbit = set_bitaddrproc(dst);
	INSTR->codelen_dst = codelen;
	if (INSTR->nrMemAcc == 1) {
		INSTR->cycles = 4;
	}
	INSTR->proc = m32c_bmcnd_dst;
	INSTR->proc();
}

/**
 ****************************************************************
 * \fn void m32c_bmcnd_c(void)
 * Set or Clear the carry depending on a condition.
 * v1
 ****************************************************************
 */
static void
m32c_bmcnd_c(void)
{
	int cnd = INSTR->Arg1;
	if (check_condition(cnd)) {
		M32C_REG_FLG |= M32C_FLG_CARRY;
	} else {
		M32C_REG_FLG &= ~M32C_FLG_CARRY;
	}
	dbgprintf("m32c_bmcnd_c not tested\n");
}

void
m32c_setup_bmcnd_c(void)
{
	int cnd = (ICODE16() & 0x7) | ((ICODE16() >> 3) & 8);
	INSTR->Arg1 = cnd;
	INSTR->proc = m32c_bmcnd_c;
	INSTR->proc();
}

/**
 *******************************************************************
 * \fn void m32c_bnand_src(void)
 * Logically ands the C flag and the inverted source and stores
 * the result in the C flag.
 * Something is wrong with the Renesas manual with bnand.
 * I suspect that the given formula is wrong and the text is right.
 * v0
 *******************************************************************
 */
static void
m32c_bnand_src(void)
{
	int bit;
	int Src;
	uint32_t data;
	if (M32C_BITINDEX() >= 0) {
		CycleCounter += 1;
		bit = M32C_BITINDEX() & 7;
	} else {
		bit = INSTR->Arg2;
	}
	data = INSTR->getbit(M32C_BITINDEX());
	Src = data & (1 << bit);
	if (Src) {
		M32C_REG_FLG &= ~M32C_FLG_CARRY;
	}
	M32C_REG_PC += INSTR->codelen_src;
	dbgprintf("m32c_bnand_src not tested\n");
}

void
m32c_setup_bnand_src(void)
{
	int bit;
	int src = ((ICODE24() >> 6) & 3) | ((ICODE24() >> 7) & 0x1c);
	int codelen_src;
	bit = ICODE24() & 7;
	INSTR->getbit = get_bitaddrproc(src, &codelen_src);
	INSTR->codelen_src = codelen_src;
	INSTR->Arg2 = bit;
	INSTR->proc = m32c_bnand_src;
	INSTR->proc();
}

/**
 **************************************************************
 * \fn void m32c_bnor_src(void)
 * Logically ors the C flag and the inverted source and stores the
 * result in the C Flag.
 * v1
 **************************************************************
 */
static void
m32c_bnor_src0(void)
{
	int bit;
	int Src;
	uint32_t data;
	if (M32C_BITINDEX() >= 0) {
		CycleCounter += 1;
		bit = M32C_BITINDEX() & 7;
	} else {
		bit = INSTR->Arg1;
	}
	data = INSTR->getbit(M32C_BITINDEX());
	Src = data & (1 << bit);
	if (!Src) {
		M32C_REG_FLG |= M32C_FLG_CARRY;
	}
	M32C_REG_PC += INSTR->codelen_dst;
	dbgprintf("m32c_bnor_src0 not tested\n");
}

void
m32c_setup_bnor_src0(void)
{
	int src = ((ICODE24() >> 6) & 3) | ((ICODE24() >> 7) & 0x1c);
	int codelen_src;
	INSTR->getbit = get_bitaddrproc(src, &codelen_src);
	INSTR->codelen_src = codelen_src;
	INSTR->Arg1 = ICODE24() & 7;
	INSTR->proc = m32c_bnor_src0;
	INSTR->proc();
}

/**
 ******************************************************************
 * \fn void m32c_bnot_dst(void)
 * Invert a bit in destination. 
 * v1
 ******************************************************************
 */
static void
m32c_bnot_dst(void)
{
	int bit;
	uint32_t data;
	if (M32C_BITINDEX() >= 0) {
		CycleCounter += 1;
		bit = M32C_BITINDEX() & 7;
	} else {
		bit = INSTR->Arg1;
	}
	data = INSTR->getbit(M32C_BITINDEX());
	data ^= (1 << bit);
	INSTR->setbit(data, M32C_BITINDEX());
	M32C_REG_PC += INSTR->codelen_dst;
	dbgprintf("m32c_bnot_dst not tested\n");
}

void
m32c_setup_bnot_dst(void)
{
	int codelen_dst;
	int dst = ((ICODE16() >> 6) & 3) | ((ICODE16() >> 7) & 0x1c);
	int bit = ICODE16() & 7;
	INSTR->getbit = get_bitaddrproc(dst, &codelen_dst);
	INSTR->setbit = set_bitaddrproc(dst);
	INSTR->codelen_dst = codelen_dst;
	INSTR->Arg1 = bit;
	INSTR->proc = m32c_bnot_dst;
	INSTR->proc();
}

/**
 ******************************************************************* 
 * \fn void m32c_bntst_src(void)
 * Copy an inverted bit to carry and zero flag.
 * v1
 ******************************************************************* 
 */
static void
m32c_bntst_src(void)
{
	int bit;
	uint32_t data;
	if (M32C_BITINDEX() >= 0) {
		CycleCounter += 1;
		bit = M32C_BITINDEX() & 7;
	} else {
		bit = ICODE24() & 7;
	}
	data = INSTR->getbit(M32C_BITINDEX());
	if (data & (1 << bit)) {
		M32C_REG_FLG &= ~(M32C_FLG_CARRY | M32C_FLG_ZERO);
	} else {
		M32C_REG_FLG |= M32C_FLG_CARRY | M32C_FLG_ZERO;
	}
	M32C_REG_PC += INSTR->codelen_src;
	dbgprintf("m32c_bntst_src not tested\n");
}

void
m32c_setup_bntst_src(void)
{
	int src = ((ICODE24() >> 6) & 3) | ((ICODE24() >> 7) & 0x1c);
	int codelen_src;
	int bit = ICODE24() & 7;
	INSTR->getbit = get_bitaddrproc(src, &codelen_src);
	INSTR->codelen_src = codelen_src;
	INSTR->Arg1 = bit;
	INSTR->proc = m32c_bntst_src;
	INSTR->proc();
}

/**
 ***********************************************************************
 * \fn void m32c_bnxor_src(void)
 * Exclusive or inverted Src with carry flag and store the 
 * result in the carry flag. 
 * v1
 ***********************************************************************
 */
static void
m32c_bnxor_src(void)
{
	int bit;
	uint32_t data;
	if (M32C_BITINDEX() >= 0) {
		CycleCounter += 1;
		bit = M32C_BITINDEX() & 7;
	} else {
		bit = INSTR->Arg1;
	}
	data = INSTR->getbit(M32C_BITINDEX());
	if (!(data & (1 << bit))) {
		M32C_REG_FLG ^= M32C_FLG_CARRY;
	}
	M32C_REG_PC += INSTR->codelen_src;
	dbgprintf("m32c_bnxor_src not tested\n");
}

void
m32c_setup_bnxor_src(void)
{
	int bit = ICODE24() & 7;
	int src = ((ICODE24() >> 6) & 3) | ((ICODE24() >> 7) & 0x1c);
	int codelen_src;
	INSTR->getbit = get_bitaddrproc(src, &codelen_src);
	INSTR->codelen_src = codelen_src;
	INSTR->Arg1 = bit;
	INSTR->proc = m32c_bnxor_src;
	INSTR->proc();
}

/**
 ****************************************************************
 * \fn void m32c_bor_src(void)
 * Logically ORs the Src and the C flag and stores the result
 * in the C flag.
 * v1
 ****************************************************************
 */
static void
m32c_bor_src(void)
{
	int bit;
	uint32_t data;
	if (M32C_BITINDEX() >= 0) {
		CycleCounter += 1;
		bit = M32C_BITINDEX() & 7;
	} else {
		bit = INSTR->Arg1;
	}
	data = INSTR->getbit(M32C_BITINDEX());
	if (data & (1 << bit)) {
		M32C_REG_FLG |= M32C_FLG_CARRY;
	}
	M32C_REG_PC += INSTR->codelen_src;
	dbgprintf("m32c_bor_src not tested\n");
}

void
m32c_setup_bor_src(void)
{
	int bit = ICODE24() & 7;
	int src = ((ICODE24() >> 6) & 3) | ((ICODE24() >> 7) & 0x1c);
	int codelen_src;
	INSTR->getbit = get_bitaddrproc(src, &codelen_src);
	INSTR->codelen_src = codelen_src;
	INSTR->Arg1 = bit;
	INSTR->proc = m32c_bor_src;
	INSTR->proc();
}

/**
 ********************************************************************
 * \fn void m32c_brk(void)
 * Breakpoint instruction. 
 * v0
 ********************************************************************
 */
void
m32c_brk(void)
{
	uint16_t flg;
	return M32C_Break();

	flg = M32C_REG_FLG;
	M32C_SET_REG_FLG(M32C_REG_FLG & ~(M32C_FLG_I | M32C_FLG_D | M32C_FLG_U));

	M32C_REG_SP -= 2;
	M32C_Write16(flg, M32C_REG_SP);
	M32C_REG_SP -= 2;
	M32C_Write16(M32C_REG_PC >> 16, M32C_REG_SP);
	M32C_REG_SP -= 2;
	M32C_Write16(M32C_REG_PC & 0xffff, M32C_REG_SP);
	dbgprintf("m32c_brk, PC on stack: 0x%06x, SP %06x\n", M32C_REG_PC, M32C_REG_SP);
	if (M32C_Read8(0xffffe7) == 0xff) {
		M32C_REG_PC = M32C_Read24(M32C_REG_INTB);
	} else {
		M32C_REG_PC = M32C_Read24(0xFFFFE4);
	}
}

/**
 ********************************************************************
 * \fn void m32c_brk2(void)
 * Breakpoint instruction. Go to address stored ad 0x0020. 
 * v0
 ********************************************************************
 */
void
m32c_brk2(void)
{
	uint16_t flg;
	flg = M32C_REG_FLG;

	M32C_SET_REG_FLG(M32C_REG_FLG & ~(M32C_FLG_I | M32C_FLG_D | M32C_FLG_U));

	M32C_REG_SP -= 2;
	M32C_Write16(flg, M32C_REG_SP);
	M32C_REG_SP -= 2;
	M32C_Write16(M32C_REG_PC >> 16, M32C_REG_SP);
	M32C_REG_SP -= 2;
	M32C_Write16(M32C_REG_PC & 0xffff, M32C_REG_SP);
	M32C_REG_PC = M32C_Read24(0x20);
	dbgprintf("m32c_brk2 not tested\n");
}

/**
 *****************************************************************
 * \fn void m32c_bset_dst(void)
 * Set a bit in destination.
 * v1
 *****************************************************************
 */
static void
m32c_bset_dst(void)
{
	int bit;
	uint32_t data;
	if (M32C_BITINDEX() >= 0) {
		CycleCounter += 1;
		bit = M32C_BITINDEX() & 7;
	} else {
		bit = INSTR->Arg1;
	}
	data = INSTR->getbit(M32C_BITINDEX());
	data |= (1 << bit);
	INSTR->setbit(data, M32C_BITINDEX());
	M32C_REG_PC += INSTR->codelen_dst;
	CycleCounter += 2;	/* Should check if memory is in IO area before */
	dbgprintf("m32c_bset_dst not implemented\n");
}

void
m32c_setup_bset_dst(void)
{
	int bit = ICODE16() & 7;
	int dst = ((ICODE16() >> 6) & 3) | ((ICODE16() >> 7) & 0x1c);
	int codelen_dst;
	INSTR->getbit = get_bitaddrproc(dst, &codelen_dst);
	INSTR->setbit = set_bitaddrproc(dst);
	INSTR->codelen_dst = codelen_dst;
	INSTR->Arg1 = bit;
	INSTR->proc = m32c_bset_dst;
	INSTR->proc();
}

/**
 ******************************************************************
 * \fn void m32c_btst_g_src(void)
 * Bit Test: Copy the bit to carry and copy the inverted bit
 * to the zero flag.
 * v1
 ******************************************************************
 */
static void
m32c_btst_g_src(void)
{
	int bit;
	uint32_t data;
	if (M32C_BITINDEX() >= 0) {
		CycleCounter += 1;
		bit = M32C_BITINDEX() & 7;
	} else {
		bit = INSTR->Arg1;
	}
	data = INSTR->getbit(M32C_BITINDEX());
	if (data & (1 << bit)) {
		M32C_REG_FLG |= M32C_FLG_CARRY;
		M32C_REG_FLG &= ~M32C_FLG_ZERO;
	} else {
		M32C_REG_FLG |= M32C_FLG_ZERO;
		M32C_REG_FLG &= ~M32C_FLG_CARRY;
	}
	M32C_REG_PC += INSTR->codelen_src;
	dbgprintf("m32c_btst_g_src not implemented\n");
}

void
m32c_setup_btst_g_src(void)
{
	int bit = ICODE16() & 7;
	int src = ((ICODE16() >> 6) & 3) | ((ICODE16() >> 7) & 0x1c);
	int codelen_src;
	INSTR->getbit = get_bitaddrproc(src, &codelen_src);
	INSTR->codelen_src = codelen_src;
	INSTR->Arg1 = bit;
	INSTR->proc = m32c_btst_g_src;
	INSTR->proc();
}

/*
 *************************************************************
 * \fn void m32c_btst_s_src(void)
 * Transfer a bit from an absolute src to the carry flag
 * and the inverted bit to the zero flag. 
 * No bitindexed version is available.
 * v1
 *************************************************************
 */
static void
m32c_btst_s_src(void)
{
	uint16_t bit = INSTR->Arg1;
	uint32_t abs16 = M32C_Read16(M32C_REG_PC);
	uint32_t data;
	data = M32C_Read8(abs16);
	if (data & (1 << bit)) {
		M32C_REG_FLG |= M32C_FLG_CARRY;
		M32C_REG_FLG &= ~M32C_FLG_ZERO;
	} else {
		M32C_REG_FLG |= M32C_FLG_ZERO;
		M32C_REG_FLG &= ~M32C_FLG_CARRY;
	}
	M32C_REG_PC += 2;
	dbgprintf("m32c_btst_s_src not tested\n");
}

void
m32c_setup_btst_s_src(void)
{
	uint16_t bit = (ICODE8() & 1) | ((ICODE8() >> 3) & 0x6);
	INSTR->Arg1 = bit;
	INSTR->proc = m32c_btst_s_src;
	INSTR->proc();
}

/**
 ****************************************************************
 * \fn void m32c_btstc_dst(void)
 * Bit Test and clear. The bit in destination is tranfered
 * to the carry flag and inverted into the zero flag.
 * Then it is cleared in destination.
 * v1
 ****************************************************************
 */
static void
m32c_btstc_dst(void)
{
	int bit;
	uint32_t data;
	if (M32C_BITINDEX() >= 0) {
		CycleCounter += 1;
		bit = M32C_BITINDEX() & 7;
	} else {
		bit = INSTR->Arg1;
	}
	data = INSTR->getbit(M32C_BITINDEX());
	if (data & (1 << bit)) {
		M32C_REG_FLG |= M32C_FLG_CARRY;
		M32C_REG_FLG &= ~M32C_FLG_ZERO;
	} else {
		M32C_REG_FLG |= M32C_FLG_ZERO;
		M32C_REG_FLG &= ~M32C_FLG_CARRY;
	}
	data = data & ~(1 << bit);
	INSTR->setbit(data, M32C_BITINDEX());
	M32C_REG_PC += INSTR->codelen_dst;
	dbgprintf("m32c_btstc_dst not tested\n");
}

void
m32c_setup_btstc_dst(void)
{
	int bit = ICODE16() & 7;
	int dst = ((ICODE16() >> 6) & 3) | ((ICODE16() >> 7) & 0x1c);
	int codelen_dst;
	INSTR->getbit = get_bitaddrproc(dst, &codelen_dst);
	INSTR->setbit = set_bitaddrproc(dst);
	INSTR->codelen_dst = codelen_dst;
	INSTR->Arg1 = bit;
	INSTR->proc = m32c_btstc_dst;
	INSTR->proc();
}

/**
 ****************************************************************
 * \fn void m32c_btsts_dst(void)
 * Bit Test and set. The bit in destination is tranfered
 * to the carry flag and inverted into the zero flag.
 * Then it is set in destination.
 * v1
 ****************************************************************
 */
static void
m32c_btsts_dst(void)
{
	int bit;
	uint32_t data;
	if (M32C_BITINDEX() >= 0) {
		CycleCounter += 1;
		bit = M32C_BITINDEX() & 7;
	} else {
		bit = INSTR->Arg1;
	}
	data = INSTR->getbit(M32C_BITINDEX());
	if (data & (1 << bit)) {
		M32C_REG_FLG |= M32C_FLG_CARRY;
		M32C_REG_FLG &= ~M32C_FLG_ZERO;
	} else {
		M32C_REG_FLG |= M32C_FLG_ZERO;
		M32C_REG_FLG &= ~M32C_FLG_CARRY;
	}
	data = data | (1 << bit);
	INSTR->setbit(data, M32C_BITINDEX());
	M32C_REG_PC += INSTR->codelen_dst;
	dbgprintf("m32c_btsts_dst not tested\n");
}

void
m32c_setup_btsts_dst(void)
{
	int bit = ICODE16() & 7;
	int dst = ((ICODE16() >> 6) & 3) | ((ICODE16() >> 7) & 0x1c);
	int codelen_dst;
	INSTR->getbit = get_bitaddrproc(dst, &codelen_dst);
	INSTR->setbit = set_bitaddrproc(dst);
	INSTR->codelen_dst = codelen_dst;
	INSTR->Arg1 = bit;
	INSTR->proc = m32c_btsts_dst;
	INSTR->proc();
}

/**
 ********************************************************************
 * \fn void m32c_bxor_src(void)
 * Store the exclusive or of source and carry flag in carry flag. 
 * v1 
 ********************************************************************
 */
static void
m32c_bxor_src(void)
{
	int bit;
	uint32_t data;
	if (M32C_BITINDEX() >= 0) {
		CycleCounter += 1;
		bit = M32C_BITINDEX() & 7;
	} else {
		bit = INSTR->Arg1;
	}
	data = INSTR->getbit(M32C_BITINDEX());
	if (data & (1 << bit)) {
		M32C_REG_FLG ^= M32C_FLG_CARRY;
	}
	M32C_REG_PC += INSTR->codelen_src;
	dbgprintf("m32c_bxor_src not tested\n");
}

void
m32c_setup_bxor_src(void)
{
	int bit = ICODE24() & 7;
	int src = ((ICODE24() >> 6) & 3) | ((ICODE24() >> 7) & 0x1c);
	int codelen_src;
	INSTR->getbit = get_bitaddrproc(src, &codelen_src);
	INSTR->codelen_src = codelen_src;
	INSTR->Arg1 = bit;
	INSTR->proc = m32c_bxor_src;
	INSTR->proc();
}

/**
 **************************************************************
 * \fn void m32c_clip(void)
 * Make the number in dst to be between src1 and src2.
 * v1
 **************************************************************
 */
static void
m32c_clip(void)
{

	int32_t Imm1, Imm2, Dst;
	uint32_t tmpDst;
	uint32_t Result;
	int size = INSTR->opsize;
	INSTR->getdst(&tmpDst, M32C_INDEXSD());
	Dst = tmpDst;
	M32C_REG_PC += INSTR->codelen_dst;
	if (size == 2) {
		Imm1 = (int32_t) (int16_t) M32C_Read16(M32C_REG_PC);
		M32C_REG_PC += size;
		Imm2 = (int32_t) (int16_t) M32C_Read16(M32C_REG_PC);
		M32C_REG_PC += size;
		if (Imm1 > Dst) {
			Result = (uint16_t) Imm1;
			INSTR->setdst(Result, M32C_INDEXSD());
		}
		if (Imm2 < Dst) {
			Result = (uint16_t) Imm2;
			INSTR->setdst(Result, M32C_INDEXSD());
		}
	} else {
		Imm1 = (int32_t) (int8_t) M32C_Read8(M32C_REG_PC);
		M32C_REG_PC += size;
		Imm2 = (int32_t) (int8_t) M32C_Read8(M32C_REG_PC);
		M32C_REG_PC += size;
		if (Imm1 > Dst) {
			Result = (uint8_t) Imm1;
			INSTR->setdst(Result, M32C_INDEXSD());
		}
		if (Imm2 < Dst) {
			Result = (uint8_t) Imm2;
			INSTR->setdst(Result, M32C_INDEXSD());
		}
	}
	dbgprintf("m32c_clip not tested\n");
}

void
m32c_setup_clip(void)
{
	int dst;
	int size;
	int codelen_dst;
	dst = ((ICODE24() >> 6) & 3) | ((ICODE24() >> 7) & 0x1c);
	if (ICODE24() & 0x100) {
		size = 2;
	} else {
		size = 1;
	}
	INSTR->getdst = general_am_get(dst, size, &codelen_dst, 0xfffff);
	INSTR->setdst = general_am_set(dst, size, &codelen_dst, 0xfffff);
	INSTR->codelen_dst = codelen_dst;
	INSTR->opsize = size;
	INSTR->proc = m32c_clip;
	INSTR->proc();
}

/**
 **********************************************************************
 * \fn void m32c_cmp_g_size_immdst(void)
 * Compare destination with an 8 or 16 Bit immediate.
 * v1
 **********************************************************************
 */

static void
m32c_cmp_size_g_immdst(void)
{
	uint32_t Src, Dst, Result;
	int opsize = INSTR->opsize;
	int srcsize = INSTR->srcsize;
	INSTR->getdst(&Dst, M32C_INDEXSD());
	if (srcsize == 2) {
		Src = M32C_Read16(M32C_REG_PC + INSTR->codelen_dst);
	} else {		/* if (srcsize == 1) */

		Src = M32C_Read8(M32C_REG_PC + INSTR->codelen_dst);
	}
	Result = Dst - Src;
	sub_flags(Dst, Src, Result, opsize);
	M32C_REG_PC += INSTR->codelen_dst + srcsize;
	dbgprintf("m32c_cmp_size_immdst not tested\n");
}

void
m32c_setup_cmp_size_g_immdst(void)
{
	int dst;
	int srcsize, opsize;
	int codelen_dst;
	dst = ((ICODE16() >> 6) & 3) | ((ICODE16() >> 7) & 0x1c);
	if (ICODE16() & 0x100) {
		srcsize = opsize = 2;
	} else {
		srcsize = opsize = 1;
		ModOpsize(dst, &opsize);
	}
	INSTR->getdst = general_am_get(dst, opsize, &codelen_dst, 0xfffff);
	INSTR->codelen_dst = codelen_dst;
	INSTR->opsize = opsize;
	INSTR->srcsize = srcsize;
	INSTR->proc = m32c_cmp_size_g_immdst;
	INSTR->proc();
}

/**
 *****************************************************************
 * \fn void m32c_cmp_g_size_immidst(void)
 * Compare an immediate with an indirect destination.
 * v1
 *****************************************************************
 */
static void
m32c_cmp_size_g_immidst(void)
{
	uint32_t Src, Dst, Result;
	uint32_t DstP;
	int opsize = INSTR->opsize;
	INSTR->getdstp(&DstP, 0);
	DstP = (DstP + M32C_INDEXSD()) & 0xffffff;
	if (opsize == 2) {
		Dst = M32C_Read16(DstP);
		Src = M32C_Read16(M32C_REG_PC + INSTR->codelen_dst);
	} else {		/* if (size == 1) */

		Dst = M32C_Read8(DstP);
		Src = M32C_Read8(M32C_REG_PC + INSTR->codelen_dst);
	}
	Result = Dst - Src;
	sub_flags(Dst, Src, Result, opsize);
	M32C_REG_PC += INSTR->codelen_dst + opsize;
	dbgprintf("m32c_cmp_size_immdst not tested\n");
}

void
m32c_setup_cmp_size_g_immidst(void)
{
	int dst;
	int size;
	int codelen_dst;
	dst = ((ICODE24() >> 6) & 3) | ((ICODE24() >> 7) & 0x1c);
	if (ICODE24() & 0x100) {
		size = 2;
	} else {
		size = 1;
	}
	INSTR->getdstp = general_am_get(dst, 4, &codelen_dst, 0xfffff);
	INSTR->codelen_dst = codelen_dst;
	INSTR->opsize = size;
	INSTR->proc = m32c_cmp_size_g_immidst;
	INSTR->proc();
}

/**
 ********************************************************************
 * \fn void m32c_cmp_l_g_imm32dst(void)
 * Compare a 32 Bit immeadiate with a destination.
 * v1
 ********************************************************************
 */
static void
m32c_cmp_l_g_imm32dst(void)
{
	uint32_t Src, Dst, Result;
	int size = 4;
	INSTR->getdst(&Dst, M32C_INDEXLD());
	Src = M32C_Read32(M32C_REG_PC + INSTR->codelen_dst);
	Result = Dst - Src;
	sub_flags(Dst, Src, Result, size);
	M32C_REG_PC += INSTR->codelen_dst + size;
	dbgprintf("m32c_cmp_l_g_imm32dst not tested\n");
}

void
m32c_setup_cmp_l_g_imm32dst(void)
{
	int dst;
	int size = 4;
	int codelen_dst;
	dst = ((ICODE16() >> 6) & 3) | ((ICODE16() >> 7) & 0x1c);
	INSTR->getdst = general_am_get(dst, size, &codelen_dst, 0xcffff);
	INSTR->codelen_dst = codelen_dst;
	INSTR->opsize = 4;
	INSTR->proc = m32c_cmp_l_g_imm32dst;
	INSTR->proc();
}

/**
 ******************************************************************
 * \fn void m32c_cmp_l_g_imm32idst(void)
 * Compare a 32 Bit immediate with an indirect destination.
 * v1
 ******************************************************************
 */
static void
m32c_cmp_l_g_imm32idst(void)
{
	uint32_t Src, Dst, Result;
	uint32_t DstP;
	int size = 4;
	INSTR->getdstp(&DstP, 0);
	DstP = (DstP + M32C_INDEXLD()) & 0xffffff;
	Dst = M32C_Read32(DstP);
	Src = M32C_Read32(M32C_REG_PC + INSTR->codelen_dst);
	Result = Dst - Src;
	sub_flags(Dst, Src, Result, size);
	M32C_REG_PC += INSTR->codelen_dst + size;
	dbgprintf("m32c_cmp_l_g_imm32idst not tested\n");
}

void
m32c_setup_cmp_l_g_imm32idst(void)
{
	int dst;
	int size = 4;
	int codelen_dst;
	dst = ((ICODE24() >> 6) & 3) | ((ICODE24() >> 7) & 0x1c);
	INSTR->getdstp = general_am_get(dst, 4, &codelen_dst, 0xcffff);
	INSTR->codelen_dst = codelen_dst;
	INSTR->opsize = size;
	INSTR->proc = m32c_cmp_l_g_imm32idst;
	INSTR->proc();
}

/**
 ********************************************************************
 * \fn void m32c_cmp_size_q_immdst(void)
 * Compare a 4 bit immediate with a destination.
 * v1
 ********************************************************************
 */
static void
m32c_cmp_b_q_immdst(void)
{
	uint32_t Dst, Result;
	int32_t Src;
	Src = INSTR->Imm32;
	INSTR->getdst(&Dst, 0);
	Result = Dst - Src;
	subb_flags(Dst, Src, Result);
	M32C_REG_PC += INSTR->codelen_dst;
	dbgprintf("m32c_cmp_size_q_immdst not tested\n");
}

static void
m32c_cmp_w_q_immdst(void)
{
	uint32_t Dst, Result;
	int32_t Src;
	Src = INSTR->Imm32;
	INSTR->getdst(&Dst, 0);
	Result = Dst - Src;
	subw_flags(Dst, Src, Result);
	M32C_REG_PC += INSTR->codelen_dst;
	dbgprintf("m32c_cmp_size_q_immdst not tested\n");
}

void
m32c_setup_cmp_size_q_immdst(void)
{
	int dst;
	int opsize;
	int codelen_dst;
	int32_t Src;
	Src = (int32_t) ((ICODE16() & 0xf) << 28) >> 28;
	dst = ((ICODE16() >> 6) & 3) | ((ICODE16() >> 7) & 0x1c);
	if (ICODE16() & 0x100) {
		opsize = 2;
	} else {
		opsize = 1;
		ModOpsize(dst, &opsize);
		if (opsize == 2) {
			Src = Src & 0xff;
		}
	}
	if (opsize == 2) {
		INSTR->proc = m32c_cmp_w_q_immdst;
	} else {
		INSTR->proc = m32c_cmp_b_q_immdst;
	}
	INSTR->getdst = general_am_get(dst, opsize, &codelen_dst, 0xfffff);
	INSTR->codelen_dst = codelen_dst;
	INSTR->Imm32 = Src;
	INSTR->proc();
}

/**
 ************************************************************
 * \fn void m32c_cmp_size_q_immidst(void)
 * Compare an immediate with an indirect destination.
 * v1
 ************************************************************
 */
static void
m32c_cmp_size_q_immidst(void)
{
	uint32_t Dst, Result;
	int32_t Src;
	uint32_t DstP;
	Src = INSTR->Imm32;
	INSTR->getdstp(&DstP, 0);
	if (INSTR->opsize == 1) {
		Dst = M32C_Read8(DstP);
	} else {
		Dst = M32C_Read16(DstP);
	}
	Result = Dst - Src;
	sub_flags(Dst, Src, Result, INSTR->opsize);
	M32C_REG_PC += INSTR->codelen_dst;
	dbgprintf("m32c_cmp_size_q_immdst not tested\n");
}

void
m32c_setup_cmp_size_q_immidst(void)
{
	int dst;
	int32_t Src;
	int size;
	int codelen_dst;
	Src = (int32_t) ((ICODE24() & 0xf) << 28) >> 28;
	dst = ((ICODE24() >> 6) & 3) | ((ICODE24() >> 7) & 0x1c);
	if (ICODE24() & 0x100) {
		size = 2;
	} else {
		size = 1;
	}
	INSTR->getdstp = general_am_get(dst, 4, &codelen_dst, 0xfffff);
	INSTR->codelen_dst = codelen_dst;
	INSTR->opsize = size;
	INSTR->Imm32 = Src;
	INSTR->proc = m32c_cmp_size_q_immidst;
	INSTR->proc();
}

/**
 *****************************************************************
 * \fn void m32c_cmp_size_s_immdst(void)
 * Compare an immediate with a destination. 
 * v1
 *****************************************************************
 */
static void
m32c_cmp_size_s_immdst(void)
{
	uint32_t Dst;
	uint32_t imm;
	uint32_t result;
	INSTR->getam2bit(&Dst, 0);
	M32C_REG_PC += INSTR->codelen_dst;
	if (INSTR->opsize == 2) {
		imm = M32C_Read16(M32C_REG_PC);
	} else {
		imm = M32C_Read8(M32C_REG_PC);
	}
	M32C_REG_PC += INSTR->opsize;

	result = Dst - imm;
	sub_flags(Dst, imm, result, INSTR->opsize);
	dbgprintf("m32c_cmp_size_s_immdst PC %06x, cmp %d with %d\n", M32C_REG_PC, Dst, imm);
}

void
m32c_setup_cmp_size_s_immdst(void)
{
	int size;
	int codelen;
	int am = (ICODE8() >> 4) & 3;
	if (ICODE8() & 1) {
		size = 2;
	} else {
		size = 1;
	}
	INSTR->getam2bit = am2bit_getproc(am, &codelen, size);
	INSTR->codelen_dst = codelen;
	INSTR->opsize = size;
	INSTR->proc = m32c_cmp_size_s_immdst;
	INSTR->proc();
}

/**
 *********************************************************************
 * \fn void m32c_cmp_size_s_immidst(void)
 * Compare an immediate with an indirect destination.
 * v1
 *********************************************************************
 */
static void
m32c_cmp_size_s_immidst(void)
{
	uint32_t Dst;
	uint32_t DstP;
	uint32_t imm;
	uint32_t result;
	INSTR->getam2bit(&DstP, 0);
	M32C_REG_PC += INSTR->codelen_dst;
	if (INSTR->opsize == 2) {
		Dst = M32C_Read16(DstP);
		imm = M32C_Read16(M32C_REG_PC);
	} else {
		Dst = M32C_Read8(DstP);
		imm = M32C_Read8(M32C_REG_PC);
	}
	M32C_REG_PC += INSTR->opsize;
	result = Dst - imm;
	sub_flags(Dst, imm, result, INSTR->opsize);
	dbgprintf("m32c_cmp_size_s_immidst implemented PC %06x, cmp %d with %d\n", M32C_REG_PC, Dst,
		  imm);
}

void
m32c_setup_cmp_size_s_immidst(void)
{
	int size;
	int codelen_dst;
	int dst = (ICODE16() >> 4) & 3;
	if (ICODE16() & 1) {
		size = 2;
	} else {
		size = 1;
	}
	INSTR->getam2bit = am2bit_getproc(dst, &codelen_dst, 4);
	INSTR->codelen_dst = codelen_dst;
	INSTR->opsize = size;
	INSTR->proc = m32c_cmp_size_s_immidst;
	INSTR->proc();
}

/**
 ***********************************************************
 * \fn void m32c_cmp_size_g_srcdst(void)
 * Compare a src and a destination.
 * v1
 ***********************************************************
 */
static void
m32c_cmp_size_g_srcdst(void)
{
	uint32_t Src, Dst, Result;
	int opsize = INSTR->opsize;
	INSTR->getsrc(&Src, M32C_INDEXSS());
	M32C_REG_PC += INSTR->codelen_src;
	INSTR->getdst(&Dst, M32C_INDEXSD());
	M32C_REG_PC += INSTR->codelen_dst;
	Result = Dst - Src;
	sub_flags(Dst, Src, Result, opsize);
	dbgprintf("m32c_cmp_size_g_srcdst not tested\n");
}

void
m32c_setup_cmp_size_g_srcdst(void)
{
	int dst, src;
	int opsize, srcsize;
	int codelen_src;
	int codelen_dst;
	dst = ((ICODE16() >> 6) & 3) | ((ICODE16() >> 7) & 0x1c);
	src = ((ICODE16() >> 4) & 3) | ((ICODE16() >> 10) & 0x1c);
	if (ICODE16() & 0x100) {
		srcsize = opsize = 2;
	} else {
		srcsize = opsize = 1;
		ModOpsize(dst, &opsize);
	}
	INSTR->getdst = general_am_get(dst, opsize, &codelen_dst, 0xfffff);
	INSTR->getsrc = general_am_get(src, srcsize, &codelen_src, 0xfffff);
	INSTR->codelen_dst = codelen_dst;
	INSTR->codelen_src = codelen_src;
	INSTR->opsize = opsize;
	INSTR->srcsize = srcsize;
	INSTR->proc = m32c_cmp_size_g_srcdst;
	INSTR->proc();
}

/**
 ****************************************************************
 * \fn void m32c_cmp_size_g_isrcdst(void)
 * Compare an immediate source with a destination. 
 * v1
 ****************************************************************
 */
static void
m32c_cmp_size_g_isrcdst(void)
{
	uint32_t Src, Dst, Result;
	uint32_t SrcP;
	int opsize = INSTR->opsize;
	int srcsize = INSTR->srcsize;
	INSTR->getsrcp(&SrcP, 0);
	M32C_REG_PC += INSTR->codelen_src;
	SrcP = (SrcP + M32C_INDEXSS()) & 0xffffff;
	INSTR->getdst(&Dst, M32C_INDEXSD());
	M32C_REG_PC += INSTR->codelen_dst;
	if (srcsize == 2) {
		Src = M32C_Read16(SrcP);
	} else {
		Src = M32C_Read8(SrcP);
	}
	Result = Dst - Src;
	sub_flags(Dst, Src, Result, opsize);
	dbgprintf("m32c_cmp_size_g_isrcdst not tested\n");
}

void
m32c_setup_cmp_size_g_isrcdst(void)
{
	int dst, src;
	int opsize, srcsize;
	int codelen_src;
	int codelen_dst;
	dst = ((ICODE24() >> 6) & 3) | ((ICODE24() >> 7) & 0x1c);
	src = ((ICODE24() >> 4) & 3) | ((ICODE24() >> 10) & 0x1c);
	if (ICODE24() & 0x100) {
		srcsize = opsize = 2;
	} else {
		srcsize = opsize = 1;
		ModOpsize(dst, &opsize);
	}
	INSTR->getdst = general_am_get(dst, opsize, &codelen_dst, 0xfffff);
	INSTR->getsrcp = general_am_get(src, 4, &codelen_src, 0xfffff);
	INSTR->codelen_dst = codelen_dst;
	INSTR->codelen_src = codelen_src;
	INSTR->opsize = opsize;
	INSTR->srcsize = srcsize;
	INSTR->proc = m32c_cmp_size_g_isrcdst;
	INSTR->proc();
}

/**
 ******************************************************************
 * \fn void m32c_cmp_size_g_srcidst(void)
 * Compare a source with an indirect destination.
 * Operation size of .B operation is 8 Bit even if
 * the src register is A0/A1 ! 
 * v1
 ******************************************************************
 */
static void
m32c_cmp_size_g_srcidst(void)
{
	uint32_t Src, Dst, Result;
	uint32_t DstP;
	int opsize = INSTR->opsize;
	INSTR->getsrc(&Src, M32C_INDEXSS());
	M32C_REG_PC += INSTR->codelen_src;
	INSTR->getdstp(&DstP, 0);
	DstP = (DstP + M32C_INDEXSD()) & 0xffffff;
	M32C_REG_PC += INSTR->codelen_dst;
	if (opsize == 2) {
		Dst = M32C_Read16(DstP);
	} else {
		Dst = M32C_Read8(DstP);
	}
	Result = Dst - Src;
	sub_flags(Dst, Src, Result, opsize);
	dbgprintf("m32c_cmp_size_g_srcidst not implemented\n");
}

void
m32c_setup_cmp_size_g_srcidst(void)
{
	int dst, src;
	int size;
	int codelen_src;
	int codelen_dst;
	dst = ((ICODE24() >> 6) & 3) | ((ICODE24() >> 7) & 0x1c);
	src = ((ICODE24() >> 4) & 3) | ((ICODE24() >> 10) & 0x1c);
	if (ICODE24() & 0x100) {
		size = 2;
	} else {
		size = 1;
	}
	INSTR->getdstp = general_am_get(dst, 4, &codelen_dst, 0xfffff);
	INSTR->getsrc = general_am_get(src, size, &codelen_src, 0xfffff);
	INSTR->codelen_dst = codelen_dst;
	INSTR->codelen_src = codelen_src;
	INSTR->opsize = size;
	INSTR->proc = m32c_cmp_size_g_srcidst;
	INSTR->proc();
}

/**
 ********************************************************************
 * \fn void m32c_cmp_size_g_isrcidst(void)
 * Compare an indirect source with an indirect destination.
 * v1
 ********************************************************************
 */
static void
m32c_cmp_size_g_isrcidst(void)
{
	uint32_t Src, Dst, Result;
	uint32_t SrcP;
	uint32_t DstP;
	int opsize = INSTR->opsize;
	INSTR->getsrcp(&SrcP, 0);
	SrcP = (SrcP + M32C_INDEXSS()) & 0xffffff;
	M32C_REG_PC += INSTR->codelen_src;
	INSTR->getdstp(&DstP, 0);
	DstP = (DstP + M32C_INDEXSD()) & 0xffffff;
	M32C_REG_PC += INSTR->codelen_dst;
	if (opsize == 2) {
		Src = M32C_Read16(SrcP);
		Dst = M32C_Read16(DstP);
	} else {
		Src = M32C_Read8(SrcP);
		Dst = M32C_Read8(DstP);
	}
	Result = Dst - Src;
	sub_flags(Dst, Src, Result, opsize);
	dbgprintf("m32c_cmp_size_g_isrcidst not implemented\n");
}

void
m32c_setup_cmp_size_g_isrcidst(void)
{
	int dst, src;
	int size;
	int codelen_src;
	int codelen_dst;
	dst = ((ICODE24() >> 6) & 3) | ((ICODE24() >> 7) & 0x1c);
	src = ((ICODE24() >> 4) & 3) | ((ICODE24() >> 10) & 0x1c);
	if (ICODE24() & 0x100) {
		size = 2;
	} else {
		size = 1;
	}
	INSTR->getdstp = general_am_get(dst, 4, &codelen_dst, GAM_ALL);
	INSTR->getsrcp = general_am_get(src, 4, &codelen_src, GAM_ALL);
	INSTR->codelen_dst = codelen_dst;
	INSTR->codelen_src = codelen_src;
	INSTR->opsize = size;
	INSTR->proc = m32c_cmp_size_g_isrcidst;
	INSTR->proc();
}

/**
 ********************************************************************
 * \fn void m32c_cmp_l_g_srcdst(void)
 * Compare a long source with a long destination.
 * v1
 ********************************************************************
 */
static void
m32c_cmp_l_g_srcdst(void)
{
	uint32_t Src, Dst, Result;
	int size = 4;
	INSTR->getsrc(&Src, M32C_INDEXLS());
	M32C_REG_PC += INSTR->codelen_src;
	INSTR->getdst(&Dst, M32C_INDEXLD());
	M32C_REG_PC += INSTR->codelen_dst;
	Result = Dst - Src;
	sub_flags(Dst, Src, Result, size);
	dbgprintf("m32c_cmp_l_g_srcdst not tested\n");
}

void
m32c_setup_cmp_l_g_srcdst(void)
{
	int dst, src;
	int size = 4;
	int codelen_src;
	int codelen_dst;
	dst = ((ICODE16() >> 6) & 3) | ((ICODE16() >> 7) & 0x1c);
	src = ((ICODE16() >> 4) & 3) | ((ICODE16() >> 10) & 0x1c);
	INSTR->getdst = general_am_get(dst, size, &codelen_dst, 0xcffff);
	INSTR->getsrc = general_am_get(src, size, &codelen_src, 0xcffff);
	INSTR->codelen_dst = codelen_dst;
	INSTR->codelen_src = codelen_src;
	INSTR->cycles = 2 + INSTR->nrMemAcc * 3;
	INSTR->proc = m32c_cmp_l_g_srcdst;
	INSTR->proc();
}

/**
 ******************************************************************
 * \fn void m32c_cmp_l_g_isrcdst(void)
 * Compare a long indirect source with a long destination.
 * v1 
 ******************************************************************
 */
static void
m32c_cmp_l_g_isrcdst(void)
{
	uint32_t Src, Dst, Result;
	uint32_t SrcP;
	int size = 4;
	INSTR->getsrcp(&SrcP, 0);
	SrcP = (SrcP + M32C_INDEXLS()) & 0xffffff;
	M32C_REG_PC += INSTR->codelen_src;
	INSTR->getdst(&Dst, M32C_INDEXLD());
	M32C_REG_PC += INSTR->codelen_dst;
	Src = M32C_Read32(SrcP);
	Result = Dst - Src;
	sub_flags(Dst, Src, Result, size);
	dbgprintf("m32c_cmp_l_g_isrcdst not tested\n");
}

void
m32c_setup_cmp_l_g_isrcdst(void)
{
	int dst, src;
	int size = 4;
	int codelen_src;
	int codelen_dst;
	dst = ((ICODE24() >> 6) & 3) | ((ICODE24() >> 7) & 0x1c);
	src = ((ICODE24() >> 4) & 3) | ((ICODE24() >> 10) & 0x1c);
	INSTR->getdst = general_am_get(dst, size, &codelen_dst, 0xcffff);
	INSTR->getsrcp = general_am_get(src, 4, &codelen_src, 0xcffff);
	INSTR->codelen_src = codelen_src;
	INSTR->codelen_dst = codelen_dst;
	INSTR->cycles = 5 + INSTR->nrMemAcc * 3;
	INSTR->proc = m32c_cmp_l_g_isrcdst;
	INSTR->proc();
}

/**
 ******************************************************************
 * \fn void m32c_cmp_l_g_srcidst(void)
 * Compare a long source with an indirect long destination.
 * v1 
 ******************************************************************
 */

static void
m32c_cmp_l_g_srcidst(void)
{
	uint32_t Src, Dst, Result;
	uint32_t DstP;
	int size = 4;
	INSTR->getsrc(&Src, M32C_INDEXLS());
	M32C_REG_PC += INSTR->codelen_src;
	INSTR->getdstp(&DstP, 0);
	DstP = (DstP + M32C_INDEXLD()) & 0xffffff;
	M32C_REG_PC += INSTR->codelen_dst;
	Dst = M32C_Read32(DstP);
	Result = Dst - Src;
	sub_flags(Dst, Src, Result, size);
	dbgprintf("m32c_cmp_l_g_srcidst not tested\n");
}

void
m32c_setup_cmp_l_g_srcidst(void)
{
	int dst, src;
	int size = 4;
	int codelen_src;
	int codelen_dst;
	dst = ((ICODE24() >> 6) & 3) | ((ICODE24() >> 7) & 0x1c);
	src = ((ICODE24() >> 4) & 3) | ((ICODE24() >> 10) & 0x1c);
	INSTR->getdstp = general_am_get(dst, 4, &codelen_dst, 0xcffff);
	INSTR->getsrc = general_am_get(src, size, &codelen_src, 0xcffff);
	INSTR->codelen_src = codelen_src;
	INSTR->codelen_dst = codelen_dst;
	INSTR->cycles = 5 + INSTR->nrMemAcc * 3;
	INSTR->proc = m32c_cmp_l_g_srcidst;
	INSTR->proc();
}

/**
 ******************************************************************
 * \fn void m32c_cmp_l_g_isrcidst(void)
 * Compare a long indirect source with an indirect long destination.
 * v1 
 ******************************************************************
 */

static void
m32c_cmp_l_g_isrcidst(void)
{
	uint32_t Src, Dst, Result;
	uint32_t DstP, SrcP;
	int size = 4;
	INSTR->getsrcp(&SrcP, 0);
	SrcP = (SrcP + M32C_INDEXLS()) & 0xffffff;
	M32C_REG_PC += INSTR->codelen_src;
	INSTR->getdstp(&DstP, 0);
	DstP = (DstP + M32C_INDEXLD()) & 0xffffff;
	M32C_REG_PC += INSTR->codelen_dst;
	Dst = M32C_Read32(DstP);
	Src = M32C_Read32(SrcP);
	Result = Dst - Src;
	sub_flags(Dst, Src, Result, size);
	dbgprintf("m32c_cmp_l_g_isrcidst not tested\n");
}

void
m32c_setup_cmp_l_g_isrcidst(void)
{
	int dst, src;
	int codelen_src;
	int codelen_dst;
	dst = ((ICODE24() >> 6) & 3) | ((ICODE24() >> 7) & 0x1c);
	src = ((ICODE24() >> 4) & 3) | ((ICODE24() >> 10) & 0x1c);
	INSTR->getdstp = general_am_get(dst, 4, &codelen_dst, 0xcffff);
	INSTR->getsrcp = general_am_get(src, 4, &codelen_src, 0xcffff);
	INSTR->codelen_src = codelen_src;
	INSTR->codelen_dst = codelen_dst;
	INSTR->cycles = 8 + INSTR->nrMemAcc * 3;
	INSTR->proc = m32c_cmp_l_g_isrcidst;
	INSTR->proc();
}

/**
 ************************************************************
 * \fn void m32c_cmp_size_s_srcr0(void)
 * Compare a source with Register R0 / R0L 
 * v1
 ************************************************************
 */
static void
m32c_cmp_size_s_srcr0(void)
{
	int size = INSTR->opsize;
	uint32_t Src, Dst, Result;
	INSTR->getam2bit(&Src, 0);
	if (size == 2) {
		Dst = M32C_REG_R0;
	} else {
		Dst = M32C_REG_R0L;
	}
	Result = Dst - Src;
	sub_flags(Dst, Src, Result, size);
	M32C_REG_PC += INSTR->codelen_src;
	dbgprintf("m32c_cmp_size_s_r0 not tested\n");
}

void
m32c_setup_cmp_size_s_srcr0(void)
{
	int size;
	int am;
	int codelen;
	am = (ICODE8() >> 4) & 3;
	if (ICODE8() & 1) {
		size = 2;
	} else {
		size = 1;
	}
	if (am == 0) {
		fprintf(stderr, "Illegal AM %d for %s\n", am, __func__);
		exit(1);
	}
	INSTR->getam2bit = am2bit_getproc(am, &codelen, size);
	INSTR->codelen_src = codelen;
	INSTR->opsize = size;
	INSTR->proc = m32c_cmp_size_s_srcr0;
	INSTR->proc();
}

/**
 ****************************************************************
 * \fn void m32c_cmp_size_s_isrcr0(void)
 * Compare an indirect source with Register R0/R0L
 * v1
 ****************************************************************
 */
static void
m32c_cmp_size_s_isrcr0(void)
{
	int size = INSTR->opsize;
	uint32_t Src, Dst, Result;
	uint32_t SrcP;
	INSTR->getam2bit(&SrcP, 0);
	if (size == 2) {
		Src = M32C_Read16(SrcP);
		Dst = M32C_REG_R0;
	} else {
		Src = M32C_Read8(SrcP);
		Dst = M32C_REG_R0L;
	}
	Result = Dst - Src;
	sub_flags(Dst, Src, Result, size);
	M32C_REG_PC += INSTR->codelen_src;
	dbgprintf("m32c_cmp_size_s_r0 not tested\n");
}

void
m32c_setup_cmp_size_s_isrcr0(void)
{
	int size;
	int src;
	int codelen;
	src = (ICODE16() >> 4) & 3;
	if (ICODE16() & 1) {
		size = 2;
	} else {
		size = 1;
	}
	if (src == 0) {
		fprintf(stderr, "Illegal AM %d for %s\n", src, __func__);
		exit(1);
	}
	INSTR->getam2bit = am2bit_getproc(src, &codelen, 4);
	INSTR->codelen_src = codelen;
	INSTR->opsize = size;
	INSTR->proc = m32c_cmp_size_s_isrcr0;
	INSTR->proc();
}

/**
 **********************************************************************
 * \fn void m32c_cmpx_immdst(void)
 * Compare a sign extended 8 Bit immediate with a 32 Bit value 
 * v1
 **********************************************************************
 */
static void
m32c_cmpx_immdst(void)
{
	uint32_t Dst, Result;
	int32_t Src;
	int size = 4;
	INSTR->getdst(&Dst, 0);
	Src = (int32_t) (int8_t) M32C_Read8(M32C_REG_PC + INSTR->codelen_dst);
	Result = Dst - Src;
	sub_flags(Dst, Src, Result, size);
	M32C_REG_PC += INSTR->codelen_dst + 1;
	dbgprintf("m32c_cmpx_immdst not tested");
}

void
m32c_setup_cmpx_immdst(void)
{
	int dst;
	int size = 4;
	int codelen_dst;
	dst = ((ICODE16() >> 6) & 3) | ((ICODE16() >> 7) & 0x1c);
	INSTR->getdst = general_am_get(dst, size, &codelen_dst, 0xcffff);
	INSTR->codelen_dst = codelen_dst;
	INSTR->proc = m32c_cmpx_immdst;
	INSTR->proc();
}

/**
 ***********************************************************************
 * Compare a sign extended 8 Bit immediate with a 32 Bit value
 * from an indirect source.
 * v1
 ***********************************************************************
 */
static void
m32c_cmpx_immidst(void)
{
	uint32_t Dst, Result;
	uint32_t DstP;
	int32_t Src;
	int size = 4;
	int codelen_dst = INSTR->codelen_dst;
	INSTR->getdstp(&DstP, 0);
	Src = (int32_t) (int8_t) M32C_Read8(M32C_REG_PC + codelen_dst);
	Dst = M32C_Read32(DstP);
	Result = Dst - Src;
	sub_flags(Dst, Src, Result, size);
	M32C_REG_PC += codelen_dst + 1;
	dbgprintf("m32c_cmpx_immidst not tested\n");
}

void
m32c_setup_cmpx_immidst(void)
{
	int dst;
	int codelen_dst;
	dst = ((ICODE24() >> 6) & 3) | ((ICODE24() >> 7) & 0x1c);
	INSTR->getdstp = general_am_get(dst, 4, &codelen_dst, GAM_ALL);
	INSTR->codelen_dst = codelen_dst;
	INSTR->proc = m32c_cmpx_immidst;
	INSTR->proc();
}

/**
 *********************************************************************************
 * Helper function for bcd addition. 
 * Currently wrong for some non bcd numbers, but this is the current best effort.
 * Example: 0x1d91 + 0xb = 0x23fc
 *********************************************************************************
 */
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
 ***********************************************************************
 * \fn void m32c_dadc_size_immdst(void)
 * Decimal add an immediate and a carry to a destination.
 * v0
 ***********************************************************************
 */
static void
m32c_dadc_size_immdst(void)
{
	uint32_t Src, Dst, Result;
	int size = INSTR->opsize;
	int codelen_dst = INSTR->codelen_dst;
	INSTR->getdst(&Dst, 0);
	if (size == 2) {
		Src = M32C_Read16(M32C_REG_PC + codelen_dst);
	} else {		/* if(size == 1)  */

		Src = M32C_Read8(M32C_REG_PC + codelen_dst);
	}
	if (size == 2) {
		if (M32C_REG_FLG & M32C_FLG_CARRY) {
			Result = decimal_add_w(Dst, Src, 1);
		} else {
			Result = decimal_add_w(Dst, Src, 0);
		}
		M32C_REG_FLG &= ~(M32C_FLG_CARRY | M32C_FLG_SIGN | M32C_FLG_ZERO);
		if (Result > 0xffff) {
			M32C_REG_FLG |= M32C_FLG_CARRY;
		}
		if (Result & 0x8000) {
			M32C_REG_FLG |= M32C_FLG_SIGN;
		}
		if ((Result & 0xffff) == 0) {
			M32C_REG_FLG |= M32C_FLG_ZERO;
		}
	} else {
		if (M32C_REG_FLG & M32C_FLG_CARRY) {
			Result = decimal_add_b(Dst, Src, 1);
		} else {
			Result = decimal_add_b(Dst, Src, 0);
		}
		M32C_REG_FLG &= ~(M32C_FLG_CARRY | M32C_FLG_SIGN | M32C_FLG_ZERO);
		if (Result > 0xff) {
			M32C_REG_FLG |= M32C_FLG_CARRY;
		}
		if (Result & 0x80) {
			M32C_REG_FLG |= M32C_FLG_SIGN;
		}
		if ((Result & 0xff) == 0) {
			M32C_REG_FLG |= M32C_FLG_ZERO;
		}
	}
	INSTR->setdst(Result, 0);
	M32C_REG_PC += codelen_dst + size;
	fprintf(stderr, "%s\n", __FUNCTION__);
	dbgprintf("m32c_dadc_size_immdst not tested\n");
}

void
m32c_setup_dadc_size_immdst(void)
{
	int dst;
	int size;
	int codelen_dst;
	dst = ((ICODE24() >> 6) & 3) | ((ICODE24() >> 7) & 0x1c);
	if (ICODE24() & 0x100) {
		size = 2;
	} else {
		size = 1;
	}
	INSTR->getdst = general_am_get(dst, size, &codelen_dst, 0xfffff);
	INSTR->setdst = general_am_set(dst, size, &codelen_dst, 0xfffff);
	INSTR->codelen_dst = codelen_dst;
	INSTR->opsize = size;
	INSTR->proc = m32c_dadc_size_immdst;
	INSTR->proc();
}

/**
 *************************************************************
 * \fn void m32c_dadc_size_srcdst(void)
 * Add a decimal source to a destination.
 * v0
 *************************************************************
 */
static void
m32c_dadc_size_srcdst(void)
{
	uint32_t Src, Dst, Result;
	int size = INSTR->opsize;
	int codelen_src = INSTR->codelen_src;
	int codelen_dst = INSTR->codelen_dst;
	INSTR->getsrc(&Src, 0);
	M32C_REG_PC += codelen_src;
	INSTR->getdst(&Dst, 0);
	if (size == 2) {
		if (M32C_REG_FLG & M32C_FLG_CARRY) {
			Result = decimal_add_w(Dst, Src, 1);
		} else {
			Result = decimal_add_w(Dst, Src, 0);
		}
		M32C_REG_FLG &= ~(M32C_FLG_CARRY | M32C_FLG_SIGN | M32C_FLG_ZERO);
		if (Result > 0xffff) {
			M32C_REG_FLG |= M32C_FLG_CARRY;
		}
		if (Result & 0x8000) {
			M32C_REG_FLG |= M32C_FLG_SIGN;
		}
		if (!(Result & 0xffff)) {
			M32C_REG_FLG |= M32C_FLG_ZERO;
		}
	} else {
		if (M32C_REG_FLG & M32C_FLG_CARRY) {
			Result = decimal_add_b(Dst, Src, 1);
		} else {
			Result = decimal_add_b(Dst, Src, 0);
		}
		M32C_REG_FLG &= ~(M32C_FLG_CARRY | M32C_FLG_SIGN | M32C_FLG_ZERO);
		if (Result > 0xff) {
			M32C_REG_FLG |= M32C_FLG_CARRY;
		}
		if (Result & 0x80) {
			M32C_REG_FLG |= M32C_FLG_SIGN;
		}
		if (!(Result & 0xff)) {
			M32C_REG_FLG |= M32C_FLG_ZERO;
		}
	}
	INSTR->setdst(Result, 0);
	M32C_REG_PC += codelen_dst;
	fprintf(stderr, "%s\n", __FUNCTION__);
	dbgprintf("m32c_dadc_size_srcdst not tested\n");
}

void
m32c_setup_dadc_size_srcdst(void)
{
	int dst, src;
	int size;
	int codelen_src;
	int codelen_dst;
	dst = ((ICODE24() >> 6) & 3) | ((ICODE24() >> 7) & 0x1c);
	src = ((ICODE24() >> 4) & 3) | ((ICODE24() >> 10) & 0x1c);
	if (ICODE24() & 0x100) {
		size = 2;
	} else {
		size = 1;
	}
	INSTR->getdst = general_am_get(dst, size, &codelen_dst, 0xfffff);
	INSTR->getsrc = general_am_get(src, size, &codelen_src, 0xfffff);
	INSTR->setdst = general_am_set(dst, size, &codelen_dst, 0xfffff);
	INSTR->codelen_src = codelen_src;
	INSTR->codelen_dst = codelen_dst;
	INSTR->opsize = size;
	INSTR->proc = m32c_dadc_size_srcdst;
	INSTR->proc();
}

/** 
 ****************************************************************
 * \fn void m32c_dadd_size_immdst(void)
 * Decimal add an immediate to a destination. 
 ****************************************************************
 */

static void
m32c_dadd_size_immdst(void)
{
	uint32_t Src, Dst, Result;
	int size = INSTR->opsize;
	int codelen = INSTR->codelen_dst;
	INSTR->getdst(&Dst, 0);
	if (size == 2) {
		Src = M32C_Read16(M32C_REG_PC + codelen);
	} else {		/* if(size == 1) */

		Src = M32C_Read8(M32C_REG_PC + codelen);
	}
	M32C_REG_FLG &= ~(M32C_FLG_CARRY | M32C_FLG_SIGN | M32C_FLG_ZERO);
	if (size == 2) {
		if (M32C_REG_FLG & M32C_FLG_CARRY) {
			Result = decimal_add_w(Dst, Src, 1);
		} else {
			Result = decimal_add_w(Dst, Src, 0);
		}
		if (Result > 0xffff) {
			M32C_REG_FLG |= M32C_FLG_CARRY;
		}
		if (Result & 0x8000) {
			M32C_REG_FLG |= M32C_FLG_SIGN;
		}
		if (!(Result & 0xffff)) {
			M32C_REG_FLG |= M32C_FLG_ZERO;
		}
	} else {
		if (M32C_REG_FLG & M32C_FLG_CARRY) {
			Result = decimal_add_b(Dst, Src, 1);
		} else {
			Result = decimal_add_b(Dst, Src, 0);
		}
		if (Result > 0xff) {
			M32C_REG_FLG |= M32C_FLG_CARRY;
		}
		if (Result & 0x80) {
			M32C_REG_FLG |= M32C_FLG_SIGN;
		}
		if (!(Result & 0xff)) {
			M32C_REG_FLG |= M32C_FLG_ZERO;
		}
	}
	M32C_REG_PC += codelen + size;
	INSTR->setdst(Result, 0);
	fprintf(stderr, "%s\n", __FUNCTION__);
	dbgprintf("m32c_dadd_size_immdst not tested\n");
}

void
m32c_setup_dadd_size_immdst(void)
{
	int dst;
	int size;
	int codelen;
	dst = ((ICODE24() >> 6) & 3) | ((ICODE24() >> 7) & 0x1c);
	if (ICODE24() & 0x100) {
		size = 2;
	} else {
		size = 1;
	}
	INSTR->getdst = general_am_get(dst, size, &codelen, 0xfffff);
	INSTR->setdst = general_am_set(dst, size, &codelen, 0xfffff);
	INSTR->codelen_dst = codelen;
	INSTR->opsize = size;
	INSTR->proc = m32c_dadd_size_immdst;
	INSTR->proc();
}

/**
 ******************************************************************
 * \fn void m32c_dadd_size_srcdst(void)
 * Decimal add a source an a destination.
 * Non BCD handling: Real CPU calculates: 0x997a + 0x11 = 0x998b
 * 0x997b + 0xf = 0x9990. Seems to be a normal adder followed
 * by an Decimal adjust operation.
 * v0
 ******************************************************************
 */
static void
m32c_dadd_size_srcdst(void)
{
	uint32_t Src, Dst, Result;
	int size = INSTR->opsize;
	int codelen_src = INSTR->codelen_src;
	int codelen_dst = INSTR->codelen_dst;
	INSTR->getsrc(&Src, 0);
	M32C_REG_PC += codelen_src;
	INSTR->getdst(&Dst, 0);
	M32C_REG_FLG &= ~(M32C_FLG_CARRY | M32C_FLG_SIGN | M32C_FLG_ZERO);
	if (size == 2) {
		Result = decimal_add_w(Dst, Src, 0);
		if (Result > 0xffff) {
			M32C_REG_FLG |= M32C_FLG_CARRY;
		}
		if (Result & 0x8000) {
			M32C_REG_FLG |= M32C_FLG_SIGN;
		}
		if (!(Result & 0xffff)) {
			M32C_REG_FLG |= M32C_FLG_ZERO;
		}
	} else {
		Result = decimal_add_b(Dst, Src, 0);
		if (Result > 0xff) {
			M32C_REG_FLG |= M32C_FLG_CARRY;
		}
		if (Result & 0x80) {
			M32C_REG_FLG |= M32C_FLG_SIGN;
		}
		if (!(Result & 0xff)) {
			M32C_REG_FLG |= M32C_FLG_ZERO;
		}
	}
	INSTR->setdst(Result, 0);
	M32C_REG_PC += codelen_dst;
	fprintf(stderr, "%s\n", __FUNCTION__);
	dbgprintf("m32c_dadd_size_srcdst not tested\n");
}

void
m32c_setup_dadd_size_srcdst(void)
{
	int dst, src;
	int size;
	int codelen_src;
	int codelen_dst;
	dst = ((ICODE24() >> 6) & 3) | ((ICODE24() >> 7) & 0x1c);
	src = ((ICODE24() >> 4) & 3) | ((ICODE24() >> 10) & 0x1c);
	if (ICODE24() & 0x100) {
		size = 2;
	} else {
		size = 1;
	}
	INSTR->getdst = general_am_get(dst, size, &codelen_dst, 0xfffff);
	INSTR->getsrc = general_am_get(src, size, &codelen_src, 0xfffff);
	INSTR->setdst = general_am_set(dst, size, &codelen_dst, 0xfffff);
	INSTR->codelen_src = codelen_src;
	INSTR->codelen_dst = codelen_dst;
	INSTR->opsize = size;
	INSTR->proc = m32c_dadd_size_srcdst;
	INSTR->proc();
}

/**
 ********************************************************************
 * \fn void m32c_dec_size_dst(void)
 * Decrement a destination by one.
 * v1
 ********************************************************************
 */
static void
m32c_dec_size_dst(void)
{
	uint32_t Dst;
	int size = INSTR->opsize;
	int codelen_dst = INSTR->codelen_dst;
	INSTR->getdst(&Dst, M32C_INDEXSD());
	Dst--;
	M32C_REG_FLG &= ~(M32C_FLG_SIGN | M32C_FLG_ZERO);
	if (size == 2) {
		if (Dst == 0) {
			M32C_REG_FLG |= M32C_FLG_ZERO;
		} else if (Dst & 0x8000) {
			M32C_REG_FLG |= M32C_FLG_SIGN;
		}
	} else {
		if (Dst == 0) {
			M32C_REG_FLG |= M32C_FLG_ZERO;
		} else if (Dst & 0x80) {
			M32C_REG_FLG |= M32C_FLG_SIGN;
		}
	}
	INSTR->setdst(Dst, M32C_INDEXSD());
	M32C_REG_PC += codelen_dst;
	dbgprintf("m32c_dec_size_dst not tested\n");
}

void
m32c_setup_dec_size_dst(void)
{
	int dst;
	int size;
	int codelen_dst;
	dst = ((ICODE16() >> 6) & 3) | ((ICODE16() >> 7) & 0x1c);
	if (ICODE16() & 0x100) {
		size = 2;
	} else {
		size = 1;
		NotModOpsize(dst);
	}
	INSTR->getdst = general_am_get(dst, size, &codelen_dst, 0xfffff);
	INSTR->setdst = general_am_set(dst, size, &codelen_dst, 0xfffff);
	INSTR->codelen_dst = codelen_dst;
	INSTR->opsize = size;
	INSTR->proc = m32c_dec_size_dst;
	INSTR->proc();
}

/**
 ********************************************************************
 * \fn void m32c_dec_size_idst(void)
 * Decrement an indirect destination by one.
 * v1
 ********************************************************************
 */
static void
m32c_dec_size_idst(void)
{
	uint32_t Dst;
	uint32_t DstP;
	int size = INSTR->opsize;
	int codelen_dst = INSTR->codelen_dst;
	INSTR->getdstp(&DstP, 0);
	DstP = (DstP + M32C_INDEXSD()) & 0xffffff;
	M32C_REG_FLG &= ~(M32C_FLG_SIGN | M32C_FLG_ZERO);
	if (size == 2) {
		Dst = M32C_Read16(DstP);
		Dst--;
		if (Dst == 0) {
			M32C_REG_FLG |= M32C_FLG_ZERO;
		} else if (Dst & 0x8000) {
			M32C_REG_FLG |= M32C_FLG_SIGN;
		}
		M32C_Write16(Dst, DstP);
	} else {
		Dst = M32C_Read8(DstP);
		Dst--;
		if (Dst == 0) {
			M32C_REG_FLG |= M32C_FLG_ZERO;
		} else if (Dst & 0x80) {
			M32C_REG_FLG |= M32C_FLG_SIGN;
		}
		M32C_Write8(Dst, DstP);
	}
	M32C_REG_PC += codelen_dst;
	dbgprintf("m32c_dec_size_idst not tested\n");
}

void
m32c_setup_dec_size_idst(void)
{
	int dst;
	int size;
	int codelen_dst;
	dst = ((ICODE24() >> 6) & 3) | ((ICODE24() >> 7) & 0x1c);
	if (ICODE24() & 0x100) {
		size = 2;
	} else {
		size = 1;
	}
	INSTR->getdstp = general_am_get(dst, 4, &codelen_dst, 0xfffff);
	INSTR->codelen_dst = codelen_dst;
	INSTR->opsize = size;
	INSTR->proc = m32c_dec_size_idst;
	INSTR->proc();
}

/**
 ***************************************************************
 * \fn void m32c_div_size_imm(void)
 * Signed divide.  
 * Test with real device: Result is not assigned in case
 * of an overflow.
 * Manual is wrong: The remainders sign is NOT the same as the
 * dividers sign (See divx)
 * v1
 ***************************************************************
 */
static void
m32c_div_size_imm(void)
{
	if (ICODE16() & 0x10) {
		int16_t Div;
		int32_t Dst;
		int16_t remainder;
		int32_t quotient;
		Div = M32C_Read16(M32C_REG_PC);
		M32C_REG_PC += 2;
		if (!Div) {
			M32C_REG_FLG |= M32C_FLG_OVERFLOW;
		} else {
			Dst = M32C_REG_R0 | ((uint32_t) M32C_REG_R2 << 16);
			quotient = Dst / Div;
			remainder = Dst - (quotient * Div);
			assert((remainder == 0) || (signum(remainder) == signum(Dst)));
			if ((quotient < -32768) || (quotient > 32767)) {
				M32C_REG_FLG |= M32C_FLG_OVERFLOW;
			} else {
				M32C_REG_R0 = quotient;
				M32C_REG_R2 = remainder;
				M32C_REG_FLG &= ~M32C_FLG_OVERFLOW;
			}
		}
	} else {
		int8_t Div;
		int16_t Dst;
		int8_t remainder;
		int16_t quotient;
		Div = M32C_Read8(M32C_REG_PC);
		M32C_REG_PC += 1;
		if (!Div) {
			M32C_REG_FLG |= M32C_FLG_OVERFLOW;
		} else {
			Dst = M32C_REG_R0;
			quotient = Dst / Div;
			remainder = Dst - (quotient * Div);
			assert((remainder == 0) || (signum(remainder) == signum(Dst)));
			if ((quotient < -128) || (quotient > 127)) {
				M32C_REG_FLG |= M32C_FLG_OVERFLOW;
			} else {
				M32C_REG_R0L = quotient;
				M32C_REG_R0H = remainder;
				M32C_REG_FLG &= ~M32C_FLG_OVERFLOW;
			}
		}

	}
	dbgprintf("m32c_div_size_imm not tested\n");
}

void
m32c_setup_div_size_imm(void)
{
	INSTR->proc = m32c_div_size_imm;
	INSTR->proc();
}

/**
 ***************************************************************
 * \fn void m32c_div_size_src(void)
 * Signed divide.  
 * v1
 ***************************************************************
 */
static void
m32c_div_size_src(void)
{
	int size = INSTR->opsize;
	int codelen_src = INSTR->codelen_src;
	uint32_t Src;
	INSTR->getsrc(&Src, M32C_INDEXSD());
	if (size == 2) {
		int16_t Div;
		int32_t Dst;
		int16_t remainder;
		int32_t quotient;
		Div = Src;
		if (!Div) {
			M32C_REG_FLG |= M32C_FLG_OVERFLOW;
		} else {
			Dst = M32C_REG_R0 | ((uint32_t) M32C_REG_R2 << 16);
			quotient = Dst / Div;
			remainder = Dst - (quotient * Div);
			assert((remainder == 0) || (signum(remainder) == signum(Dst)));
			if ((quotient < -32768) || (quotient > 32767)) {
				M32C_REG_FLG |= M32C_FLG_OVERFLOW;
			} else {
				M32C_REG_R0 = quotient;
				M32C_REG_R2 = remainder;
				M32C_REG_FLG &= ~M32C_FLG_OVERFLOW;
			}
		}
	} else {
		int8_t Div;
		int16_t Dst;
		int8_t remainder;
		int16_t quotient;
		Div = Src;
		if (!Div) {
			M32C_REG_FLG |= M32C_FLG_OVERFLOW;
		} else {
			Dst = M32C_REG_R0;
			quotient = Dst / Div;
			remainder = Dst - (quotient * Div);
			assert((remainder == 0) || (signum(remainder) == signum(Dst)));
			if ((quotient < -128) || (quotient > 127)) {
				M32C_REG_FLG |= M32C_FLG_OVERFLOW;
			} else {
				M32C_REG_R0L = quotient;
				M32C_REG_R0H = remainder;
				M32C_REG_FLG &= ~M32C_FLG_OVERFLOW;
			}
		}

	}
	M32C_REG_PC += codelen_src;
	dbgprintf("m32c_div_size_src not tested\n");
}

void
m32c_setup_div_size_src(void)
{
	int size;
	int src;
	int codelen_src;
	src = ((ICODE16() >> 6) & 3) | ((ICODE16() >> 7) & 0x1c);
	if (ICODE16() & 0x100) {
		size = 2;
	} else {
		size = 1;
	}
	INSTR->getsrc = general_am_get(src, size, &codelen_src, 0xfffff);
	INSTR->codelen_src = codelen_src;
	INSTR->opsize = size;
	INSTR->proc = m32c_div_size_src;
	INSTR->proc();
}

/**
 ***************************************************************
 * \fn void m32c_div_size_isrc(void)
 * Signed divide with indirect source.  
 * v1
 ***************************************************************
 */
static void
m32c_div_size_isrc(void)
{
	int size = INSTR->opsize;
	int codelen_src = INSTR->codelen_src;
	uint32_t SrcP;
	INSTR->getsrcp(&SrcP, 0);
	SrcP = (SrcP + M32C_INDEXSD()) & 0xffffff;
	if (size == 2) {
		int16_t Div;
		int32_t Dst;
		int16_t remainder;
		int32_t quotient;
		Div = M32C_Read16(SrcP);
		if (!Div) {
			M32C_REG_FLG |= M32C_FLG_OVERFLOW;
		} else {
			Dst = M32C_REG_R0 | ((uint32_t) M32C_REG_R2 << 16);
			quotient = Dst / Div;
			remainder = Dst - (quotient * Div);
			assert((remainder == 0) || (signum(remainder) == signum(Dst)));
			if ((quotient < -32768) || (quotient > 32767)) {
				M32C_REG_FLG |= M32C_FLG_OVERFLOW;
			} else {
				M32C_REG_R0 = quotient;
				M32C_REG_R2 = remainder;
				M32C_REG_FLG &= ~M32C_FLG_OVERFLOW;
			}
		}
	} else {
		int8_t Div;
		int16_t Dst;
		int8_t remainder;
		int16_t quotient;
		Div = M32C_Read8(SrcP);
		if (!Div) {
			M32C_REG_FLG |= M32C_FLG_OVERFLOW;
		} else {
			Dst = M32C_REG_R0;
			quotient = Dst / Div;
			remainder = Dst - (quotient * Div);
			assert((remainder == 0) || (signum(remainder) == signum(Dst)));
			if ((quotient < -128) || (quotient > 127)) {
				M32C_REG_FLG |= M32C_FLG_OVERFLOW;
			} else {
				M32C_REG_R0L = quotient;
				M32C_REG_R0H = remainder;
				M32C_REG_FLG &= ~M32C_FLG_OVERFLOW;
			}
		}

	}
	M32C_REG_PC += codelen_src;
	dbgprintf("m32c_div_size_isrc not tested\n");
}

void
m32c_setup_div_size_isrc(void)
{
	int size;
	int src;
	int codelen_src;
	src = ((ICODE24() >> 6) & 3) | ((ICODE24() >> 7) & 0x1c);
	if (ICODE24() & 0x100) {
		size = 2;
	} else {
		size = 1;
	}
	INSTR->getsrcp = general_am_get(src, 4, &codelen_src, 0xfffff);
	INSTR->codelen_src = codelen_src;
	INSTR->opsize = size;
	INSTR->proc = m32c_div_size_isrc;
	INSTR->proc();
}

/**
 ***************************************************************
 * \fn void m32c_div_l_src(void)
 * Signed long divide.  
 * v1
 ***************************************************************
 */
static void
m32c_div_l_src(void)
{
	int codelen_src = INSTR->codelen_src;
	uint32_t Src;
	int32_t Div;
	int64_t Dst;
	int64_t quotient;
	int64_t remainder;

	INSTR->getsrc(&Src, M32C_INDEXLD());
	Div = (int32_t) Src;
	if (!Div) {
		M32C_REG_FLG |= M32C_FLG_OVERFLOW;
	} else {
		Dst = (int64_t) (int32_t) (M32C_REG_R0 | (M32C_REG_R2 << 16));
		quotient = Dst / Div;
		remainder = Dst - (Div * quotient);
		//fprintf(stderr,"%d / %d = %lld\n",Dst,Div,quotient);
		assert((remainder == 0) || (signum(remainder) == signum(Dst)));
		if ((quotient < INT64_C(-0x80000000)) || (quotient > INT64_C(0x7fffffff))) {
			M32C_REG_FLG |= M32C_FLG_OVERFLOW;
		} else {
			M32C_REG_R0 = quotient;
			M32C_REG_R2 = quotient >> 16;
			M32C_REG_FLG &= ~M32C_FLG_OVERFLOW;
		}
	}
	M32C_REG_PC += codelen_src;
	dbgprintf("m32c_div_l_src not tested\n");
}

void
m32c_setup_div_l_src(void)
{
	int size = 4;
	int src;
	int codelen_src;
	src = ((ICODE24() >> 6) & 3) | ((ICODE24() >> 7) & 0x1c);
	INSTR->getsrc = general_am_get(src, size, &codelen_src, 0xcffff);
	INSTR->codelen_src = codelen_src;
	INSTR->opsize = size;
	INSTR->proc = m32c_div_l_src;
	INSTR->proc();
}

/**
 ******************************************************************* 
 * \fn void m32c_divu_size_imm(void)
 * Unsigned divide.
 * Test with real device: No result is assigned in case of overflow.
 * v1
 ******************************************************************* 
 */
static void
m32c_divu_size_imm(void)
{
	if (ICODE16() & 0x10) {
		uint16_t Div;
		uint32_t Dst;
		uint16_t remainder;
		uint32_t quotient;
		Div = M32C_Read16(M32C_REG_PC);
		M32C_REG_PC += 2;
		if (!Div) {
			M32C_REG_FLG |= M32C_FLG_OVERFLOW;
		} else {
			Dst = M32C_REG_R0 | ((uint32_t) M32C_REG_R2 << 16);
			quotient = Dst / Div;
			remainder = Dst - (quotient * Div);
			if (quotient & 0xffff0000) {
				M32C_REG_FLG |= M32C_FLG_OVERFLOW;
			} else {
				M32C_REG_R0 = quotient;
				M32C_REG_R2 = remainder;
				M32C_REG_FLG &= ~M32C_FLG_OVERFLOW;
			}
		}
	} else {
		uint8_t Div;
		uint16_t Dst;
		uint8_t remainder;
		uint16_t quotient;
		Div = M32C_Read8(M32C_REG_PC);
		M32C_REG_PC += 1;
		if (!Div) {
			M32C_REG_FLG |= M32C_FLG_OVERFLOW;
		} else {
			Dst = M32C_REG_R0;
			quotient = Dst / Div;
			remainder = Dst - (quotient * Div);
			if (quotient & 0xff00) {
				M32C_REG_FLG |= M32C_FLG_OVERFLOW;
			} else {
				M32C_REG_R0L = quotient;
				M32C_REG_R0H = remainder;
				M32C_REG_FLG &= ~M32C_FLG_OVERFLOW;
			}
		}

	}
	dbgprintf("m32c_divu_size_imm not tested\n");
}

void
m32c_setup_divu_size_imm(void)
{
	INSTR->proc = m32c_divu_size_imm;
	INSTR->proc();
}

/**
 ******************************************************************* 
 * \fn void m32c_divu_size_src(void)
 * Unsigned divide.
 * v1
 ******************************************************************* 
 */
static void
m32c_divu_size_src(void)
{
	int size = INSTR->opsize;
	int codelen_src = INSTR->codelen_src;
	uint32_t Src;
	INSTR->getsrc(&Src, M32C_INDEXSD());
	if (size == 2) {
		uint16_t Div;
		uint32_t Dst;
		uint16_t remainder;
		uint32_t quotient;
		Div = Src;
		if (!Div) {
			M32C_REG_FLG |= M32C_FLG_OVERFLOW;
		} else {
			Dst = M32C_REG_R0 | ((uint32_t) M32C_REG_R2 << 16);
			quotient = Dst / Div;
			remainder = Dst - (quotient * Div);
			if (quotient & 0xffff0000) {
				M32C_REG_FLG |= M32C_FLG_OVERFLOW;
			} else {
				M32C_REG_R0 = quotient;
				M32C_REG_R2 = remainder;
				M32C_REG_FLG &= ~M32C_FLG_OVERFLOW;
			}
		}
	} else {
		uint8_t Div;
		uint16_t Dst;
		uint8_t remainder;
		uint16_t quotient;
		Div = Src;
		if (!Div) {
			M32C_REG_FLG |= M32C_FLG_OVERFLOW;
		} else {
			Dst = M32C_REG_R0;
			quotient = Dst / Div;
			remainder = Dst - (quotient * Div);
			if (quotient & 0xff00) {
				M32C_REG_FLG |= M32C_FLG_OVERFLOW;
			} else {
				M32C_REG_R0L = quotient;
				M32C_REG_R0H = remainder;
				M32C_REG_FLG &= ~M32C_FLG_OVERFLOW;
			}
		}

	}
	M32C_REG_PC += codelen_src;
	dbgprintf("m32c_divu_size_src not tested\n");
}

void
m32c_setup_divu_size_src(void)
{
	int size;
	int src;
	int codelen_src;
	src = ((ICODE16() >> 6) & 3) | ((ICODE16() >> 7) & 0x1c);
	if (ICODE16() & 0x100) {
		size = 2;
	} else {
		size = 1;
	}
	INSTR->getsrc = general_am_get(src, size, &codelen_src, 0xfffff);
	INSTR->codelen_src = codelen_src;
	INSTR->opsize = size;
	INSTR->proc = m32c_divu_size_src;
	INSTR->proc();
}

/**
 ******************************************************************* 
 * \fn void m32c_divu_size_isrc(void)
 * Unsigned divide by an indirect source.
 * v1
 ******************************************************************* 
 */
static void
m32c_divu_size_isrc(void)
{
	int size = INSTR->opsize;
	int codelen_src = INSTR->codelen_src;
	uint32_t SrcP;
	INSTR->getsrcp(&SrcP, 0);
	SrcP = (SrcP + M32C_INDEXSD()) & 0xffffff;
	if (size == 2) {
		uint16_t Div;
		uint32_t Dst;
		uint16_t remainder;
		uint32_t quotient;
		Div = M32C_Read16(SrcP);
		if (!Div) {
			M32C_REG_FLG |= M32C_FLG_OVERFLOW;
		} else {
			Dst = M32C_REG_R0 | ((uint32_t) M32C_REG_R2 << 16);
			quotient = Dst / Div;
			remainder = Dst - (quotient * Div);
			if (quotient & 0xffff0000) {
				M32C_REG_FLG |= M32C_FLG_OVERFLOW;
			} else {
				M32C_REG_R0 = quotient;
				M32C_REG_R2 = remainder;
				M32C_REG_FLG &= ~M32C_FLG_OVERFLOW;
			}
		}
	} else {
		uint8_t Div;
		uint16_t Dst;
		uint8_t remainder;
		uint16_t quotient;
		Div = M32C_Read8(SrcP);
		if (!Div) {
			M32C_REG_FLG |= M32C_FLG_OVERFLOW;
		} else {
			Dst = M32C_REG_R0;
			quotient = Dst / Div;
			remainder = Dst - (quotient * Div);
			if (quotient & 0xff00) {
				M32C_REG_FLG |= M32C_FLG_OVERFLOW;
			} else {
				M32C_REG_R0L = quotient;
				M32C_REG_R0H = remainder;
				M32C_REG_FLG &= ~M32C_FLG_OVERFLOW;
			}
		}

	}
	M32C_REG_PC += codelen_src;
	dbgprintf("m32c_divu_size_isrc not tested\n");
}

void
m32c_setup_divu_size_isrc(void)
{
	int size;
	int src;
	int codelen_src;
	src = ((ICODE24() >> 6) & 3) | ((ICODE24() >> 7) & 0x1c);
	if (ICODE24() & 0x100) {
		size = 2;
	} else {
		size = 1;
	}
	INSTR->getsrcp = general_am_get(src, 4, &codelen_src, 0xfffff);
	INSTR->codelen_src = codelen_src;
	INSTR->opsize = size;
	INSTR->proc = m32c_divu_size_isrc;
	INSTR->proc();
}

/*
 *************************************************************************
 * \fn void m32c_divu_l_src(void)
 * Unsigned divide long.
 * v1
 *************************************************************************
 */
static void
m32c_divu_l_src(void)
{
	int codelen_src = INSTR->codelen_src;
	uint32_t Src;
	uint32_t Div;
	uint64_t Dst;
	uint64_t quotient;
	INSTR->getsrc(&Src, M32C_INDEXSD());
	Div = Src;
	if (!Div) {
		M32C_REG_FLG |= M32C_FLG_OVERFLOW;
	} else {
		Dst = M32C_REG_R0 | ((uint32_t) M32C_REG_R2 << 16);
		quotient = Dst / Div;
		if (quotient & UINT64_C(0xffffffff00000000)) {
			M32C_REG_FLG |= M32C_FLG_OVERFLOW;
		} else {
			M32C_REG_R0 = quotient;
			M32C_REG_R2 = quotient >> 16;
			M32C_REG_FLG &= ~M32C_FLG_OVERFLOW;
		}
	}
	M32C_REG_PC += codelen_src;
	dbgprintf("m32c_divu_l_src not tested\n");
}

void
m32c_setup_divu_l_src(void)
{
	int size = 4;
	int src;
	int codelen_src;
	src = ((ICODE24() >> 6) & 3) | ((ICODE24() >> 7) & 0x1c);
	INSTR->getsrc = general_am_get(src, size, &codelen_src, 0xcffff);
	INSTR->codelen_src = codelen_src;
	INSTR->opsize = size;
	INSTR->proc = m32c_divu_l_src;
	INSTR->proc();
}

/*
 *************************************************************************
 * \fn void m32c_divx_size_imm(void)
 * Signed divide with remainder with same sign as divider. 
 * Test with real device: No result is assigned in case of overflow.
 * v1
 *************************************************************************
 */

static void
m32c_divx_w_imm(void)
{
	int16_t Div;
	int32_t Dst;
	int16_t remainder;
	int32_t quotient;
	Div = M32C_Read16(M32C_REG_PC);
	M32C_REG_PC += 2;
	if (!Div) {
		M32C_REG_FLG |= M32C_FLG_OVERFLOW;
	} else {
		Dst = M32C_REG_R0 | ((uint32_t) M32C_REG_R2 << 16);
		if (Div < -1) {
			quotient = (Dst - (Div + 1)) / Div;
		} else {
			quotient = Dst / Div;
		}
		remainder = Dst - (quotient * Div);
		assert((remainder == 0) || (signum(remainder) == signum(Div)));
		if (quotient != (int32_t) (int16_t) quotient) {
			M32C_REG_FLG |= M32C_FLG_OVERFLOW;
		} else {
			M32C_REG_R0 = quotient;
			M32C_REG_R2 = remainder;
			M32C_REG_FLG &= ~M32C_FLG_OVERFLOW;
		}
	}
	dbgprintf("m32c_divx_size_imm not tested\n");
}

static void
m32c_divx_b_imm(void)
{
	int8_t Div;
	int16_t Dst;
	int8_t remainder;
	int16_t quotient;
	Div = M32C_Read8(M32C_REG_PC);
	M32C_REG_PC += 1;
	if (!Div) {
		M32C_REG_FLG |= M32C_FLG_OVERFLOW;
	} else {
		Dst = M32C_REG_R0;
		if (Div < -1) {
			quotient = (Dst - (Div + 1)) / Div;
		} else {
			quotient = Dst / Div;
		}
		remainder = Dst - (quotient * Div);
		assert((remainder == 0) || (signum(remainder) == signum(Div)));
		if (quotient != (int16_t) (int8_t) quotient) {
			M32C_REG_FLG |= M32C_FLG_OVERFLOW;
		} else {
			M32C_REG_R0L = quotient;
			M32C_REG_R0H = remainder;
			M32C_REG_FLG &= ~M32C_FLG_OVERFLOW;
		}
	}
	dbgprintf("m32c_divx_size_imm not tested\n");
}

void
m32c_setup_divx_size_imm(void)
{
	if (ICODE16() & 0x10) {
		INSTR->proc = m32c_divx_w_imm;
	} else {
		INSTR->proc = m32c_divx_b_imm;
	}
	INSTR->proc();
}

/*
 *************************************************************************
 * \fn void m32c_divx_size_src(void)
 * Signed divide with remainder with same sign as divider. 
 * v1
 *************************************************************************
 */
static void
m32c_divx_size_src(void)
{
	int size = INSTR->opsize;
	int codelen_src = INSTR->codelen_src;
	uint32_t Src;
	INSTR->getsrc(&Src, M32C_INDEXSD());
	if (size == 2) {
		int16_t Div;
		int32_t Dst;
		int16_t remainder;
		int32_t quotient;
		Div = Src;
		if (!Div) {
			M32C_REG_FLG |= M32C_FLG_OVERFLOW;
		} else {
			Dst = (int32_t) (M32C_REG_R0 | ((uint32_t) M32C_REG_R2 << 16));
			if (Div < 0) {
				quotient = (Dst - (Div + 1)) / Div;
			} else {
				quotient = Dst / Div;
			}
			remainder = Dst - (quotient * Div);
			assert((remainder == 0) || (signum(remainder) == signum(Div)));
			if (quotient != (int32_t) (int16_t) quotient) {
				M32C_REG_FLG |= M32C_FLG_OVERFLOW;
			} else {
				M32C_REG_R0 = quotient;
				M32C_REG_R2 = remainder;
				M32C_REG_FLG &= ~M32C_FLG_OVERFLOW;
			}
		}
	} else {
		int8_t Div;
		int16_t Dst;
		int8_t remainder;
		int16_t quotient;
		Div = Src;
		if (!Div) {
			M32C_REG_FLG |= M32C_FLG_OVERFLOW;
		} else {
			Dst = (int16_t) M32C_REG_R0;
			if (Div < 0) {
				quotient = (Dst - (Div + 1)) / Div;
			} else {
				quotient = Dst / Div;
			}
			remainder = Dst - (quotient * Div);
			assert((remainder == 0) || (signum(remainder) == signum(Div)));
			if (quotient != (int16_t) (int8_t) quotient) {
				M32C_REG_FLG |= M32C_FLG_OVERFLOW;
			} else {
				M32C_REG_R0L = quotient;
				M32C_REG_R0H = remainder;
				M32C_REG_FLG &= ~M32C_FLG_OVERFLOW;
			}
		}

	}
	M32C_REG_PC += codelen_src;
	dbgprintf("m32c_divx_size_src not tested\n");
}

void
m32c_setup_divx_size_src(void)
{
	int size;
	int src;
	int codelen_src;
	src = ((ICODE16() >> 6) & 3) | ((ICODE16() >> 7) & 0x1c);

	if (ICODE16() & 0x100) {
		size = 2;
	} else {
		size = 1;
	}
	INSTR->getsrc = general_am_get(src, size, &codelen_src, 0xfffff);
	INSTR->codelen_src = codelen_src;
	INSTR->opsize = size;
	INSTR->proc = m32c_divx_size_src;
	INSTR->proc();
}

/**
 *****************************************************************************
 * \fn void m32c_divx_size_src(void)
 * Signed divide by indirect source with remainder with same sign as divider. 
 * v1
 *****************************************************************************
 */

static void
m32c_divx_size_isrc(void)
{
	int size = INSTR->opsize;
	int codelen_src = INSTR->codelen_src;
	uint32_t Src;
	uint32_t SrcP;
	INSTR->getsrcp(&SrcP, 0);
	SrcP = (SrcP + M32C_INDEXSD()) & 0xffffff;
	if (size == 2) {
		int16_t Div;
		int32_t Dst;
		int16_t remainder;
		int32_t quotient;
		Src = M32C_Read16(SrcP);
		Div = Src;
		if (!Div) {
			M32C_REG_FLG |= M32C_FLG_OVERFLOW;
		} else {
			Dst = (int32_t) (M32C_REG_R0 | ((uint32_t) M32C_REG_R2 << 16));
			if (Div < 0) {
				quotient = (Dst - (Div + 1)) / Div;
			} else {
				quotient = Dst / Div;
			}
			remainder = Dst - (quotient * Div);
			assert((remainder == 0) || (signum(remainder) == signum(Div)));
			if (quotient != (int32_t) (int16_t) quotient) {
				M32C_REG_FLG |= M32C_FLG_OVERFLOW;
			} else {
				M32C_REG_R0 = quotient;
				M32C_REG_R2 = remainder;
				M32C_REG_FLG &= ~M32C_FLG_OVERFLOW;
			}
		}
	} else {
		int8_t Div;
		int16_t Dst;
		int8_t remainder;
		int16_t quotient;
		Src = M32C_Read8(SrcP);
		Div = Src;
		if (!Div) {
			M32C_REG_FLG |= M32C_FLG_OVERFLOW;
		} else {
			Dst = (int16_t) M32C_REG_R0;
			if (Div < 0) {
				quotient = (Dst - (Div + 1)) / Div;
			} else {
				quotient = Dst / Div;
			}
			remainder = Dst - (quotient * Div);
			assert((remainder == 0) || (signum(remainder) == signum(Div)));
			if (quotient != (int16_t) (int8_t) quotient) {
				M32C_REG_FLG |= M32C_FLG_OVERFLOW;
			} else {
				M32C_REG_R0L = quotient;
				M32C_REG_R0H = remainder;
				M32C_REG_FLG &= ~M32C_FLG_OVERFLOW;
			}
		}

	}
	M32C_REG_PC += codelen_src;
	dbgprintf("m32c_divx_size_isrc not tested\n");
}

void
m32c_setup_divx_size_isrc(void)
{
	int size;
	int src;
	int codelen_src;

	src = ((ICODE24() >> 6) & 3) | ((ICODE24() >> 7) & 0x1c);
	if (ICODE24() & 0x100) {
		size = 2;
	} else {
		size = 1;
	}
	INSTR->getsrcp = general_am_get(src, 4, &codelen_src, 0xfffff);
	INSTR->codelen_src = codelen_src;
	INSTR->opsize = size;
	INSTR->proc = m32c_divx_size_isrc;
	INSTR->proc();
}

/**
 *****************************************************************************
 * \fn void m32c_divx_l_src(void)
 * Long Signed divide by indirect source with remainder with 
 * same sign as divider. 
 * v1
 *****************************************************************************
 */

static void
m32c_divx_l_src(void)
{
	int codelen_src = INSTR->codelen_src;
	uint32_t Src;
	int32_t Div;
	int64_t Dst;
	int64_t quotient;
	int64_t remainder;
	INSTR->getsrc(&Src, M32C_INDEXSD());
	Div = Src;
	if (!Div) {
		M32C_REG_FLG |= M32C_FLG_OVERFLOW;
	} else {
		Dst = (int64_t) (int32_t) (M32C_REG_R0 | ((uint32_t) M32C_REG_R2 << 16));
		if (Div < 0) {
			quotient = (Dst - (Div + 1)) / Div;
		} else {
			quotient = Dst / Div;
		}
		remainder = Dst - (quotient * Div);
		assert((remainder == 0) || (signum(remainder) == signum(Div)));
		if ((quotient < INT64_C(-0x80000000)) || (quotient > INT64_C(0x7fffffff))) {
			M32C_REG_FLG |= M32C_FLG_OVERFLOW;
		} else {
			M32C_REG_R0 = quotient;
			M32C_REG_R2 = quotient >> 16;
			M32C_REG_FLG &= ~M32C_FLG_OVERFLOW;
		}
	}
	M32C_REG_PC += codelen_src;
	dbgprintf("m32c_divx_l_src not tested\n");
}

void
m32c_setup_divx_l_src(void)
{
	int size = 4;
	int src;
	int codelen_src;
	src = ((ICODE24() >> 6) & 3) | ((ICODE24() >> 7) & 0x1c);
	INSTR->getsrc = general_am_get(src, size, &codelen_src, 0xcffff);
	INSTR->codelen_src = codelen_src;
	INSTR->opsize = size;
	INSTR->proc = m32c_divx_l_src;
	INSTR->proc();
}

/**
 *****************************************************************************
 * \fn void m32c_dsbb_size_immdst(void)
 * Decimal subtract with borrow.
 * Behaviour for non BCD numbers is wrong.
 * v0
 *****************************************************************************
 */
static void
m32c_dsbb_size_immdst(void)
{
	uint32_t Src, Dst, BcdResult;
	int32_t Result;
	int size = INSTR->opsize;
	int codelen_dst = INSTR->codelen_dst;
	INSTR->getdst(&Dst, 0);
	if (size == 2) {
		Src = M32C_Read16(M32C_REG_PC + codelen_dst);
	} else {		/* if(size == 1) */

		Src = M32C_Read8(M32C_REG_PC + codelen_dst);
	}
	Src = bcd_to_uint16(Src);
	Dst = bcd_to_uint16(Dst);
	if (M32C_REG_FLG & M32C_FLG_CARRY) {
		Result = Dst - Src;
	} else {
		Result = Dst - Src - 1;
	}
	M32C_REG_FLG &= ~(M32C_FLG_CARRY | M32C_FLG_SIGN | M32C_FLG_ZERO);
	if (size == 2) {
		if (Result < 0) {
			Result += 10000;
			M32C_REG_FLG |= M32C_FLG_CARRY;
		}
		BcdResult = uint16_to_bcd(Result);
		if (BcdResult & 0x8000) {
			M32C_REG_FLG |= M32C_FLG_SIGN;
		}
		if ((BcdResult & 0xffff) == 0) {
			M32C_REG_FLG |= M32C_FLG_ZERO;
		}
	} else {
		if (Result < 0) {
			Result += 100;
			M32C_REG_FLG |= M32C_FLG_CARRY;
		}
		BcdResult = uint16_to_bcd(Result);
		if (BcdResult & 0x80) {
			M32C_REG_FLG |= M32C_FLG_SIGN;
		}
		if ((BcdResult & 0xff) == 0) {
			M32C_REG_FLG |= M32C_FLG_ZERO;
		}
	}
	INSTR->setdst(uint16_to_bcd(Result), 0);
	M32C_REG_PC += codelen_dst + size;
	fprintf(stderr, "%s not tested\n", __FUNCTION__);
	dbgprintf("m32c_dsbb_size_immdst not tested\n");
}

void
m32c_setup_dsbb_size_immdst(void)
{
	int dst;
	int size;
	int codelen;
	dst = ((ICODE24() >> 6) & 3) | ((ICODE24() >> 7) & 0x1c);
	if (ICODE24() & 0x100) {
		size = 2;
	} else {
		size = 1;
	}
	INSTR->getdst = general_am_get(dst, size, &codelen, 0xfffff);
	INSTR->setdst = general_am_set(dst, size, &codelen, 0xfffff);
	INSTR->codelen_dst = codelen;
	INSTR->opsize = size;
	INSTR->proc = m32c_dsbb_size_immdst;
	INSTR->proc();
}

/**
 *****************************************************************************
 * \fn void m32c_dsbb_size_srcdst(void)
 * Decimal subtract with borrow. 
 * Behaviour for non BCD numbers is wrong !
 * v0
 *****************************************************************************
 */
static void
m32c_dsbb_size_srcdst(void)
{
	uint32_t Src, Dst, BcdResult;
	int32_t Result;
	int size = INSTR->opsize;
	int codelen_src = INSTR->codelen_src;
	int codelen_dst = INSTR->codelen_dst;
	INSTR->getsrc(&Src, 0);
	M32C_REG_PC += codelen_src;
	INSTR->getdst(&Dst, 0);
	Src = bcd_to_uint16(Src);
	Dst = bcd_to_uint16(Dst);
	if (M32C_REG_FLG & M32C_FLG_CARRY) {
		Result = Dst - Src;
	} else {
		Result = Dst - Src - 1;
	}
	M32C_REG_FLG &= ~(M32C_FLG_CARRY | M32C_FLG_SIGN | M32C_FLG_ZERO);
	if (size == 2) {
		if (Result < 0) {
			Result += 10000;
			M32C_REG_FLG |= M32C_FLG_CARRY;
		}
		BcdResult = uint16_to_bcd(Result);
		if (BcdResult & 0x8000) {
			M32C_REG_FLG |= M32C_FLG_SIGN;
		}
		if ((BcdResult & 0xffff) == 0) {
			M32C_REG_FLG |= M32C_FLG_ZERO;
		}
	} else {
		if (Result < 0) {
			Result += 100;
			M32C_REG_FLG |= M32C_FLG_CARRY;
		}
		BcdResult = uint16_to_bcd(Result);
		if (BcdResult & 0x80) {
			M32C_REG_FLG |= M32C_FLG_SIGN;
		}
		if ((BcdResult & 0xff) == 0) {
			M32C_REG_FLG |= M32C_FLG_ZERO;
		}
	}
	INSTR->setdst(BcdResult, 0);
	M32C_REG_PC += codelen_dst;
	fprintf(stderr, "%s not tested\n", __FUNCTION__);
	dbgprintf("m32c_dsbb_size_srcdst not tested\n");
}

void
m32c_setup_dsbb_size_srcdst(void)
{
	int dst, src;
	int size;
	int codelen_src;
	int codelen_dst;
	dst = ((ICODE24() >> 6) & 3) | ((ICODE24() >> 7) & 0x1c);
	src = ((ICODE24() >> 4) & 3) | ((ICODE24() >> 10) & 0x1c);
	if (ICODE24() & 0x100) {
		size = 2;
	} else {
		size = 1;
	}
	INSTR->getdst = general_am_get(dst, size, &codelen_dst, 0xfffff);
	INSTR->getsrc = general_am_get(src, size, &codelen_src, 0xfffff);
	INSTR->setdst = general_am_set(dst, size, &codelen_dst, 0xfffff);
	INSTR->codelen_src = codelen_src;
	INSTR->codelen_dst = codelen_dst;
	INSTR->opsize = size;
	INSTR->proc = m32c_dsbb_size_srcdst;
	INSTR->proc();
}

/**
 *****************************************************************************
 * \fn void m32c_dsub_size_immdst(void)
 * Decimal subtract.
 * Behaviour for non BCD numbers is wrong.
 * v0
 *****************************************************************************
 */
static void
m32c_dsub_size_immdst(void)
{
	uint32_t Src, Dst, BcdResult;
	int32_t Result;
	int size = INSTR->opsize;
	int codelen = INSTR->codelen_dst;
	INSTR->getdst(&Dst, 0);
	if (size == 2) {
		Src = M32C_Read16(M32C_REG_PC + codelen);
	} else {		/* if(size == 1) */

		Src = M32C_Read8(M32C_REG_PC + codelen);
	}
	Src = bcd_to_uint16(Src);
	Dst = bcd_to_uint16(Dst);
	Result = Dst - Src;
	M32C_REG_FLG &= ~(M32C_FLG_CARRY | M32C_FLG_SIGN | M32C_FLG_ZERO);
	if (size == 2) {
		if (Result < 0) {
			Result += 10000;
			M32C_REG_FLG |= M32C_FLG_CARRY;
		}
		BcdResult = uint16_to_bcd(Result);
		if (BcdResult & 0x8000) {
			M32C_REG_FLG |= M32C_FLG_SIGN;
		} else if ((BcdResult & 0xffff) == 0) {
			M32C_REG_FLG |= M32C_FLG_ZERO;
		}
	} else {
		if (Result < 0) {
			Result += 100;
			M32C_REG_FLG |= M32C_FLG_CARRY;
		}
		BcdResult = uint16_to_bcd(Result);
		if (BcdResult & 0x80) {
			M32C_REG_FLG |= M32C_FLG_SIGN;
		} else if ((BcdResult & 0xff) == 0) {
			M32C_REG_FLG |= M32C_FLG_ZERO;
		}
	}
	M32C_REG_PC += codelen + size;
	INSTR->setdst(uint16_to_bcd(Result), 0);
	fprintf(stderr, "%s not tested\n", __FUNCTION__);
	dbgprintf("m32c_dsub_size_immdst not tested\n");
}

void
m32c_setup_dsub_size_immdst(void)
{
	int dst;
	int size;
	int codelen;
	dst = ((ICODE24() >> 6) & 3) | ((ICODE24() >> 7) & 0x1c);
	if (ICODE24() & 0x100) {
		size = 2;
	} else {
		size = 1;
	}
	INSTR->getdst = general_am_get(dst, size, &codelen, 0xfffff);
	INSTR->setdst = general_am_set(dst, size, &codelen, 0xfffff);
	INSTR->codelen_dst = codelen;
	INSTR->opsize = size;
	INSTR->proc = m32c_dsub_size_immdst;
	INSTR->proc();
}

/**
 *****************************************************************************
 * \fn void m32c_dsub_size_srcdst(void)
 * Decimal subtract.
 * Behaviour for non BCD numbers is wrong.
 * v0
 *****************************************************************************
 */
static void
m32c_dsub_size_srcdst(void)
{
	uint32_t Src, Dst, BcdResult;
	int32_t Result;
	int size = INSTR->opsize;
	int codelen_src = INSTR->codelen_src;
	int codelen_dst = INSTR->codelen_dst;

	INSTR->getsrc(&Src, 0);
	M32C_REG_PC += codelen_src;
	INSTR->getdst(&Dst, 0);
	Src = bcd_to_uint16(Src);
	Dst = bcd_to_uint16(Dst);
	Result = Dst - Src;
	M32C_REG_FLG &= ~(M32C_FLG_CARRY | M32C_FLG_SIGN | M32C_FLG_ZERO);
	if (size == 2) {
		if (Result < 0) {
			Result += 10000;
			M32C_REG_FLG |= M32C_FLG_CARRY;
		}
		BcdResult = uint16_to_bcd(Result);
		if (BcdResult & 0x8000) {
			M32C_REG_FLG |= M32C_FLG_SIGN;
		} else if ((BcdResult & 0xffff) == 0) {
			M32C_REG_FLG |= M32C_FLG_ZERO;
		}
	} else {
		if (Result < 0) {
			Result += 100;
			M32C_REG_FLG |= M32C_FLG_CARRY;
		}
		BcdResult = uint16_to_bcd(Result);
		if (BcdResult & 0x80) {
			M32C_REG_FLG |= M32C_FLG_SIGN;
		} else if ((BcdResult & 0xff) == 0) {
			M32C_REG_FLG |= M32C_FLG_ZERO;
		}
	}
	INSTR->setdst(BcdResult, 0);
	M32C_REG_PC += codelen_dst;
	fprintf(stderr, "%s not tested\n", __FUNCTION__);
	dbgprintf("m32c_dsub_size_srcdst not tested\n");
}

void
m32c_setup_dsub_size_srcdst(void)
{
	int dst, src;
	int size;
	int codelen_src;
	int codelen_dst;
	dst = ((ICODE24() >> 6) & 3) | ((ICODE24() >> 7) & 0x1c);
	src = ((ICODE24() >> 4) & 3) | ((ICODE24() >> 10) & 0x1c);
	if (ICODE24() & 0x100) {
		size = 2;
	} else {
		size = 1;
	}
	INSTR->getdst = general_am_get(dst, size, &codelen_dst, 0xfffff);
	INSTR->getsrc = general_am_get(src, size, &codelen_src, 0xfffff);
	INSTR->setdst = general_am_set(dst, size, &codelen_dst, 0xfffff);
	INSTR->codelen_dst = codelen_dst;
	INSTR->codelen_src = codelen_src;
	INSTR->opsize = size;
	INSTR->proc = m32c_dsub_size_srcdst;
	INSTR->proc();
}

/**
 **********************************************************************************
 * \fn void m32c_enter_imm(void)
 * Begin a new stackframe.
 * v1
 **********************************************************************************
 */
void
m32c_enter_imm(void)
{
	uint8_t imm = M32C_Read8(M32C_REG_PC);
	M32C_REG_PC += 1;

	M32C_REG_SP = M32C_REG_SP - 2;
	M32C_Write16(M32C_REG_FB >> 16, M32C_REG_SP);

	M32C_REG_SP = M32C_REG_SP - 2;
	M32C_Write16(M32C_REG_FB & 0xffff, M32C_REG_SP);

	M32C_REG_FB = M32C_REG_SP;
	M32C_REG_SP -= imm;
	dbgprintf("m32c_enter_imm %d , SP %04x, FB %04x\n", imm, M32C_REG_SP, M32C_REG_FB);
}

/**
 **********************************************************************************
 * \fn void m32c_exitd(void)
 * Return to a previous stackframe and return from a subroutine.
 * v1
 **********************************************************************************
 */
void
m32c_exitd(void)
{
	M32C_REG_SP = M32C_REG_FB;

	M32C_REG_FB = M32C_Read16(M32C_REG_SP);
	M32C_REG_SP += 2;
	M32C_REG_FB |= (M32C_Read16(M32C_REG_SP) & 0xff) << 16;
	M32C_REG_SP += 2;

	M32C_REG_PC = M32C_Read16(M32C_REG_SP);
	M32C_REG_SP += 2;
	M32C_REG_PC |= (M32C_Read16(M32C_REG_SP) & 0xff) << 16;
	M32C_REG_SP += 2;
	dbgprintf("m32c_exitd not tested\n");
}

/**
 ********************************************************************************
 * \fn void m32c_exts_size_dst(void)
 * v1
 ********************************************************************************
 */
static void
m32c_exts_size_dst(void)
{
	int size = INSTR->opsize;
	uint32_t Dst;
	int codelen = INSTR->codelen_dst;
	INSTR->getdst(&Dst, 0);
	if (size == 2) {
		Dst = (int32_t) (int16_t) Dst;
	} else {
		Dst = (int32_t) (int8_t) Dst;
	}
	INSTR->setdst(Dst, 0);
	ext_flags(Dst, size << 1);
	M32C_REG_PC += codelen;
	dbgprintf("m32c_exts_size_dst not tested\n");
}

void
m32c_setup_exts_size_dst(void)
{
	int size;
	int codelen;
	int dst = ((ICODE16() >> 6) & 3) | ((ICODE16() >> 7) & 0x1c);
	if (ICODE16() & 0x100) {
		size = 2;
	} else {
		size = 1;
	}
	INSTR->getdst = general_am_get(dst, size, &codelen, 0xcffff);
	INSTR->setdst = general_am_set(dst, size << 1, &codelen, 0xcffff);
	INSTR->codelen_dst = codelen;
	INSTR->opsize = size;
	INSTR->proc = m32c_exts_size_dst;
	INSTR->proc();
}

/**
 ********************************************************************************
 * \fn void m32c_exts_b_src_dst(void)
 * v1
 ********************************************************************************
 */
static void
m32c_exts_b_src_dst(void)
{
	uint32_t Src, Dst;
	int codelen_src = INSTR->codelen_src;
	int codelen_dst = INSTR->codelen_dst;
	INSTR->getsrc(&Src, 0);
	M32C_REG_PC += codelen_src;
	Dst = (int32_t) (int8_t) Src;
	INSTR->setdst(Dst, 0);
	ext_flags(Dst, 2);
	M32C_REG_PC += codelen_dst;
	dbgprintf("m32c_exts_b_src_dst not tested\n");
}

void
m32c_setup_exts_b_src_dst(void)
{
	int codelen_src;
	int codelen_dst;
	int dst = ((ICODE24() >> 6) & 3) | ((ICODE24() >> 7) & 0x1c);
	int src = ((ICODE24() >> 4) & 3) | ((ICODE24() >> 10) & 0x1c);
	INSTR->getsrc = general_am_get(src, 1, &codelen_src, 0xffff3);
	INSTR->setdst = general_am_set(dst, 2, &codelen_dst, 0xfffff);
	INSTR->codelen_src = codelen_src;
	INSTR->codelen_dst = codelen_dst;
	INSTR->proc = m32c_exts_b_src_dst;
	INSTR->proc();
}

/**
 ********************************************************************************
 * \fn void m32c_extz_src_dst(void)
 * v1
 ********************************************************************************
 */
static void
m32c_extz_src_dst(void)
{
	uint32_t Src, Dst;
	int codelen_src = INSTR->codelen_src;
	int codelen_dst = INSTR->codelen_dst;
	INSTR->getsrc(&Src, 0);
	//fprintf(stderr,"A0: %06x Got %02x, codelen src %d\n",M32C_REG_A0,Src,codelen_src);
	M32C_REG_PC += codelen_src;
	Dst = Src;
	INSTR->setdst(Dst, 0);
	ext_flags(Dst, 2);
	M32C_REG_PC += codelen_dst;
	dbgprintf("m32c_extz_src_dst not tested\n");
}

void
m32c_setup_extz_src_dst(void)
{
	int codelen_src;
	int codelen_dst;
	int dst = ((ICODE24() >> 6) & 3) | ((ICODE24() >> 7) & 0x1c);
	int src = ((ICODE24() >> 4) & 3) | ((ICODE24() >> 10) & 0x1c);
	//fprintf(stderr,"Extz s %x, d %x\n",src,dst);
	INSTR->getsrc = general_am_get(src, 1, &codelen_src, 0xffff3);
	INSTR->setdst = general_am_set(dst, 2, &codelen_dst, 0xfffff);
	INSTR->codelen_src = codelen_src;
	INSTR->codelen_dst = codelen_dst;
	INSTR->proc = m32c_extz_src_dst;
	INSTR->proc();
}

/**
 **************************************************************
 * \fn void m32c_fclr(void)
 * Clear a bit in the flag register.
 * v1
 **************************************************************
 */
static void
m32c_fclr(void)
{
	int dst = INSTR->Arg1;
	M32C_SET_REG_FLG(M32C_REG_FLG & ~(1 << dst));
	dbgprintf("m32c_fclr not tested\n");
}

void
m32c_setup_fclr(void)
{
	INSTR->Arg1 = ICODE16() & 7;
	INSTR->proc = m32c_fclr;
	INSTR->proc();
}

/**
 *****************************************************************
 * \fn void m32c_freit(void)
 * Return from fast interrupt.
 * v1
 *****************************************************************
 */
void
m32c_freit(void)
{
	M32C_SET_REG_FLG(M32C_REG(svf));
	M32C_REG_PC = M32C_REG(svp);
	dbgprintf("m32c_freit not tested\n");
}

/**
 ************************************************************
 * \fn void m32c_fset(void)
 * Set a bit in the flag register.
 * v1
 ************************************************************
 */
static void
m32c_fset(void)
{
	int dst = INSTR->Arg1;
	M32C_SET_REG_FLG(M32C_REG_FLG | (1 << dst));
	dbgprintf("m32c_fset not tested\n");
}

void
m32c_setup_fset(void)
{
	INSTR->Arg1 = ICODE16() & 7;
	INSTR->proc = m32c_fset;
	INSTR->proc();
}

/**
 ***********************************************************
 * \fn void m32c_inc_size_dst(void)
 * Increment a destination by one.
 * v1
 ***********************************************************
 */
static void
m32c_inc_size_dst(void)
{
	uint32_t Dst;
	int opsize = INSTR->opsize;
	INSTR->getdst(&Dst, M32C_INDEXSD());
	Dst++;
	sgn_zero_flags(Dst, opsize);
	INSTR->setdst(Dst, M32C_INDEXSD());
	M32C_REG_PC += INSTR->codelen_dst;
	dbgprintf("m32c_inc_size_dst not tested\n");
}

void
m32c_setup_inc_size_dst(void)
{
	int dst;
	int size;
	int codelen_dst;
	dst = ((ICODE16() >> 6) & 3) | ((ICODE16() >> 7) & 0x1c);
	if (ICODE16() & 0x100) {
		size = 2;
	} else {
		size = 1;
	}
	INSTR->getdst = general_am_get(dst, size, &codelen_dst, GAM_ALL);
	INSTR->setdst = general_am_set(dst, size, &codelen_dst, GAM_ALL);
	INSTR->codelen_dst = codelen_dst;
	INSTR->opsize = size;
	INSTR->proc = m32c_inc_size_dst;
	INSTR->proc();
}

/**
 ***********************************************************
 * \fn void m32c_inc_size_idst(void)
 * Increment an indirect destination by one.
 * v1
 ***********************************************************
 */
static void
m32c_inc_size_idst(void)
{
	uint32_t DstP;
	int size = INSTR->opsize;
	int codelen_dst = INSTR->codelen_dst;
	INSTR->getdstp(&DstP, 0);
	DstP = (DstP + M32C_INDEXSD()) & 0xffffff;
	M32C_REG_FLG &= ~(M32C_FLG_SIGN | M32C_FLG_ZERO);
	if (size == 2) {
		uint16_t Dst;
		Dst = M32C_Read16(DstP);
		Dst++;
		if (Dst == 0) {
			M32C_REG_FLG |= M32C_FLG_ZERO;
		} else if (Dst & 0x8000) {
			M32C_REG_FLG |= M32C_FLG_SIGN;
		}
		M32C_Write16(Dst, DstP);
	} else {
		uint8_t Dst;
		Dst = M32C_Read8(DstP);
		Dst++;
		if (Dst == 0) {
			M32C_REG_FLG |= M32C_FLG_ZERO;
		} else if (Dst & 0x80) {
			M32C_REG_FLG |= M32C_FLG_SIGN;
		}
		M32C_Write8(Dst, DstP);
	}
	M32C_REG_PC += codelen_dst;
	dbgprintf("m32c_inc_size_idst not tested\n");
}

void
m32c_setup_inc_size_idst(void)
{
	int dst;
	int size;
	int codelen_dst;
	dst = ((ICODE24() >> 6) & 3) | ((ICODE24() >> 7) & 0x1c);
	if (ICODE24() & 0x100) {
		size = 2;
	} else {
		size = 1;
	}
	INSTR->getdstp = general_am_get(dst, 4, &codelen_dst, GAM_ALL);
	INSTR->codelen_dst = codelen_dst;
	INSTR->opsize = size;
	INSTR->proc = m32c_inc_size_idst;
	INSTR->proc();
}

/*
 *******************************************************
 * \fn void m32c_indexb_size_src(void)
 * Modify the src and destination address of the next
 * instruction.
 * v1
 *******************************************************
 */
static void
m32c_indexb_size_src(void)
{
	int codelen_src = INSTR->codelen_src;
	uint32_t Src;
	INSTR->getsrc(&Src, 0);
	gm32c.index_src = Src;
	gm32c.index_dst = Src;
	//gm32c.index_src_used = 0;
	//gm32c.index_dst_used = 0;
	M32C_REG_PC += codelen_src;
	M32C_PostSignal(M32C_SIG_INHIBIT_IRQ | M32C_SIG_DELETE_INDEX);
	dbgprintf("m32c_indexb_size_src not tested\n");
}

void
m32c_setup_indexb_size_src(void)
{
	int size;
	int codelen_src;
	int src = ((ICODE16() >> 6) & 3) | ((ICODE16() >> 7) & 0x1c);
	if (ICODE16() & 0x10) {
		size = 2;
	} else {
		size = 1;
	}
	INSTR->getsrc = general_am_get(src, size, &codelen_src, GAM_ALL);
	INSTR->codelen_src = codelen_src;
	INSTR->opsize = size;
	INSTR->proc = m32c_indexb_size_src;
	INSTR->proc();
}

/**
 ***************************************************************************
 * \fn void m32c_indexbd_size_src(void)
 * Modify the destination address of the next
 * instruction.
 * v1
 ***************************************************************************
 */
static void
m32c_indexbd_size_src(void)
{
	int codelen_src = INSTR->codelen_src;
	uint32_t Src;
	INSTR->getsrc(&Src, 0);
	//fprintf(stderr,"INDEXBD() sigs %08x\n",gm32c.signals);
	gm32c.index_dst = Src;
	//gm32c.index_dst_used = 0;
	M32C_REG_PC += codelen_src;
	M32C_PostSignal(M32C_SIG_INHIBIT_IRQ | M32C_SIG_DELETE_INDEX);
	dbgprintf("m32c_indexbd_size_src not tested\n");
}

void
m32c_setup_indexbd_size_src(void)
{
	int size;
	int codelen_src;
	int src = ((ICODE16() >> 6) & 3) | ((ICODE16() >> 7) & 0x1c);
	if (ICODE16() & 0x10) {
		size = 2;
	} else {
		size = 1;
	}
	INSTR->getsrc = general_am_get(src, size, &codelen_src, GAM_ALL);
	INSTR->codelen_src = codelen_src;
	INSTR->opsize = size;
	INSTR->proc = m32c_indexbd_size_src;
	INSTR->proc();
}

/**
 *************************************************************************
 * \fn void m32c_indexbs_size_src(void)
 * Modify the source address of the next instruction.
 * v1
 *************************************************************************
 */
static void
m32c_indexbs_size_src(void)
{
	int codelen_src = INSTR->codelen_src;
	uint32_t Src;
	INSTR->getsrc(&Src, 0);
	gm32c.index_src = Src;
	//gm32c.index_src_used = 0;
	M32C_REG_PC += codelen_src;
	M32C_PostSignal(M32C_SIG_INHIBIT_IRQ | M32C_SIG_DELETE_INDEX);
	dbgprintf("m32c_indexbs_size_src not tested\n");
}

void
m32c_setup_indexbs_size_src(void)
{
	int size;
	int codelen_src;
	int src = ((ICODE16() >> 6) & 3) | ((ICODE16() >> 7) & 0x1c);
	if (ICODE16() & 0x10) {
		size = 2;
	} else {
		size = 1;
	}
	INSTR->getsrc = general_am_get(src, size, &codelen_src, GAM_ALL);
	INSTR->codelen_src = codelen_src;
	INSTR->opsize = size;
	INSTR->proc = m32c_indexbs_size_src;
	INSTR->proc();
}

/**
 **********************************************************************
 * \fn void m32c_indexl_size_src(void)
 * Modify the source and destination address of the
 * next instruction by 4 times the value of source.
 * Src has to be smaller than 16384.
 * v1
 **********************************************************************
 */
static void
m32c_indexl_size_src(void)
{
	int codelen_src = INSTR->codelen_src;
	uint32_t Src;
	INSTR->getsrc(&Src, 0);
	Src = (Src << 2) & 0xffff;
	gm32c.index_src = Src;
	gm32c.index_dst = Src;
	//gm32c.index_src_used = 0;
	//gm32c.index_dst_used = 0;
	M32C_REG_PC += codelen_src;
	M32C_PostSignal(M32C_SIG_INHIBIT_IRQ | M32C_SIG_DELETE_INDEX);
	dbgprintf("m32c_indexl_size_src not tested\n");
}

void
m32c_setup_indexl_size_src(void)
{
	int size;
	int codelen_src;
	int src = ((ICODE16() >> 6) & 3) | ((ICODE16() >> 7) & 0x1c);
	if (ICODE16() & 0x10) {
		size = 2;
	} else {
		size = 1;
	}
	INSTR->getsrc = general_am_get(src, size, &codelen_src, GAM_ALL);
	INSTR->codelen_src = codelen_src;
	INSTR->opsize = size;
	INSTR->proc = m32c_indexl_size_src;
	INSTR->proc();
}

/**
 ***********************************************************************
 * \fn void m32c_indexld_size_src(void)
 * Modify the destination address of the next instruction by 4 times
 * the value of source. 
 * v1
 ***********************************************************************
 */
static void
m32c_indexld_size_src(void)
{
	int codelen_src = INSTR->codelen_src;
	uint32_t Src;
	INSTR->getsrc(&Src, 0);
	Src = (Src << 2) & 0xffff;
	gm32c.index_dst = Src;
	//gm32c.index_dst_used = 0;
	M32C_REG_PC += codelen_src;
	M32C_PostSignal(M32C_SIG_INHIBIT_IRQ | M32C_SIG_DELETE_INDEX);
	dbgprintf("m32c_indexld_size_src not tested\n");
}

void
m32c_setup_indexld_size_src(void)
{
	int size;
	int codelen_src;
	int src = ((ICODE16() >> 6) & 3) | ((ICODE16() >> 7) & 0x1c);
	if (ICODE16() & 0x10) {
		size = 2;
	} else {
		size = 1;
	}
	INSTR->getsrc = general_am_get(src, size, &codelen_src, GAM_ALL);
	INSTR->codelen_src = codelen_src;
	INSTR->opsize = size;
	INSTR->proc = m32c_indexld_size_src;
	INSTR->proc();
}

/**
 ***********************************************************************
 * \fn void m32c_indexld_size_src(void)
 * Modify the source address of the next instruction by 4 times
 * the value of source. 
 * v1
 ***********************************************************************
 */
static void
m32c_indexls_size_src(void)
{
	int codelen_src = INSTR->codelen_src;
	uint32_t Src;
	INSTR->getsrc(&Src, 0);
	Src = (Src << 2) & 0xffff;
	gm32c.index_src = Src;
	//gm32c.index_src_used = 0;
	M32C_REG_PC += codelen_src;
	M32C_PostSignal(M32C_SIG_INHIBIT_IRQ | M32C_SIG_DELETE_INDEX);
	dbgprintf("m32c_indexls_size_src not tested\n");
}

void
m32c_setup_indexls_size_src(void)
{
	int size;
	int codelen_src;
	int src = ((ICODE16() >> 6) & 3) | ((ICODE16() >> 7) & 0x1c);
	if (ICODE16() & 0x10) {
		size = 2;
	} else {
		size = 1;
	}
	INSTR->getsrc = general_am_get(src, size, &codelen_src, GAM_ALL);
	INSTR->codelen_src = codelen_src;
	INSTR->opsize = size;
	INSTR->proc = m32c_indexls_size_src;
	INSTR->proc();
}

/**
 ***************************************************************************
 * \fn void m32c_indexw_size_src(void)
 * Modify the source and destination address of the next instruction by 2
 * times the value of source.
 * v1
 ***************************************************************************
 */

static void
m32c_indexw_size_src(void)
{
	int codelen_src = INSTR->codelen_src;
	uint32_t Src;
	INSTR->getsrc(&Src, 0);
	Src = (Src << 1) & 0xffff;
	gm32c.index_src = Src;
	gm32c.index_dst = Src;
	//gm32c.index_src_used = 0;
	//gm32c.index_dst_used = 0;
	M32C_REG_PC += codelen_src;
	M32C_PostSignal(M32C_SIG_INHIBIT_IRQ | M32C_SIG_DELETE_INDEX);
	dbgprintf("m32c_indexw_size_src not tested\n");
}

void
m32c_setup_indexw_size_src(void)
{
	int size;
	int codelen_src;
	int src = ((ICODE16() >> 6) & 3) | ((ICODE16() >> 7) & 0x1c);
	if (ICODE16() & 0x10) {
		size = 2;
	} else {
		size = 1;
	}
	INSTR->getsrc = general_am_get(src, size, &codelen_src, GAM_ALL);
	INSTR->codelen_src = codelen_src;
	INSTR->opsize = size;
	INSTR->proc = m32c_indexw_size_src;
	INSTR->proc();
}

/**
 ***************************************************************************
 * \fn void m32c_indexwd_size_src(void)
 * Modify the destination address of the next instruction by 2
 * times the value of source.
 * v1
 ***************************************************************************
 */

static void
m32c_indexwd_size_src(void)
{
	int codelen_src = INSTR->codelen_src;;
	uint32_t Src;
	INSTR->getsrc(&Src, 0);
	Src = (Src << 1) & 0xffff;
	gm32c.index_dst = Src;
	//gm32c.index_dst_used = 0;
	M32C_REG_PC += codelen_src;
	M32C_PostSignal(M32C_SIG_INHIBIT_IRQ | M32C_SIG_DELETE_INDEX);
	dbgprintf("m32c_indexwd_size_src not tested\n");
}

void
m32c_setup_indexwd_size_src(void)
{
	int size;
	int codelen_src;
	int src = ((ICODE16() >> 6) & 3) | ((ICODE16() >> 7) & 0x1c);
	if (ICODE16() & 0x10) {
		size = 2;
	} else {
		size = 1;
	}
	INSTR->getsrc = general_am_get(src, size, &codelen_src, GAM_ALL);
	INSTR->codelen_src = codelen_src;
	INSTR->opsize = size;
	INSTR->proc = m32c_indexwd_size_src;
	INSTR->proc();
}

/**
 ***************************************************************************
 * \fn void m32c_indexws_size_src(void)
 * Modify the source address of the next instruction by 2
 * times the value of source.
 * v1
 ***************************************************************************
 */

static void
m32c_indexws_size_src(void)
{
	uint32_t Src;
	INSTR->getsrc(&Src, 0);
	Src = (Src << 1) & 0xffff;
	gm32c.index_src = Src;
	//gm32c.index_src_used = 0;
	M32C_REG_PC += INSTR->codelen_src;
	M32C_PostSignal(M32C_SIG_INHIBIT_IRQ | M32C_SIG_DELETE_INDEX);
	dbgprintf("m32c_indexws_size_src not tested\n");
}

void
m32c_setup_indexws_size_src(void)
{
	int size;
	int codelen_src;
	int src = ((ICODE16() >> 6) & 3) | ((ICODE16() >> 7) & 0x1c);
	if (ICODE16() & 0x10) {
		size = 2;
	} else {
		size = 1;
	}
	INSTR->getsrc = general_am_get(src, size, &codelen_src, GAM_ALL);
	INSTR->codelen_src = codelen_src;
	INSTR->opsize = size;
	INSTR->proc = m32c_indexws_size_src;
	INSTR->proc();
}

/**
 *********************************************************************************
 * \fn void m32c_int_imm(void)
 * Trigger a Software interrupt.
 * v0
 *********************************************************************************
 */
void
m32c_int_imm(void)
{
	int Src;
	uint16_t flg;
	Src = M32C_Read8(M32C_REG_PC) >> 2;
	M32C_REG_PC += 1;
	flg = M32C_REG_FLG;
	if (Src < 32) {
		M32C_SET_REG_FLG(M32C_REG_FLG & ~(M32C_FLG_I | M32C_FLG_D | M32C_FLG_U));
	} else if (Src < 64) {
		M32C_SET_REG_FLG(M32C_REG_FLG & ~(M32C_FLG_I | M32C_FLG_D));
	} else {
		dbgprintf("Reached unreachable code\n", Src);
		return;
	}
	M32C_REG_SP -= 2;
	M32C_Write16(flg, M32C_REG_SP);
	M32C_REG_SP -= 2;
	M32C_Write16(M32C_REG_PC >> 16, M32C_REG_SP);
	M32C_REG_SP -= 2;
	M32C_Write16(M32C_REG_PC & 0xffff, M32C_REG_SP);
	M32C_REG_PC = M32C_Read24((M32C_REG_INTB + (Src << 2)) & 0xffffff);
	dbgprintf("m32c_int_imm not tested\n");
}

/**
 *********************************************************************************
 * \fn void m32c_into(void)
 * Trigger an Overflow interrupt if the overflow flag is set.
 * v0
 *********************************************************************************
 */
void
m32c_into(void)
{
	uint16_t flg;
	if (!(M32C_REG_FLG & M32C_FLG_OVERFLOW)) {
		return;
	}
	flg = M32C_REG_FLG;
	M32C_SET_REG_FLG(M32C_REG_FLG & ~(M32C_FLG_I | M32C_FLG_D | M32C_FLG_U));

	M32C_REG_SP -= 2;
	M32C_Write16(flg, M32C_REG_SP);
	M32C_REG_SP -= 2;
	M32C_Write16(M32C_REG_PC >> 16, M32C_REG_SP);
	M32C_REG_SP -= 2;
	M32C_Write16(M32C_REG_PC & 0xffff, M32C_REG_SP);
	M32C_REG_PC = M32C_Read24(0xFFFFE0);
	dbgprintf("m32c_into not tested\n");
}

/**
 ********************************************************************************
 * \fn void m32c_jcnd(void)
 * Conditional Jump by an 8 bit displacement.
 * v0
 ********************************************************************************
 */
static void
m32c_jcnd(void)
{
	uint8_t cnd = INSTR->Arg1;
	int8_t dsp;
	if (check_condition(cnd)) {
		dsp = M32C_Read8(M32C_REG_PC);
		M32C_REG_PC += dsp;
		CycleCounter += 2;
		dbgprintf("m32c_jcnd True, jump to 0x%06x\n", M32C_REG_PC);
	} else {
		M32C_REG_PC++;
		dbgprintf("m32c_jcnd false\n");
	}
}

void
m32c_setup_jcnd(void)
{
	uint8_t cnd = (ICODE8() & 1) | ((ICODE8() >> 3) & 0xe);
	INSTR->Arg1 = cnd;
	INSTR->proc = m32c_jcnd;
	INSTR->proc();
}

/**
 ******************************************************************************
 * \fn void m32c_jmp_s
 * Short Jump (PC+2 to PC+9)
 * v0
 ******************************************************************************
 */
static void
m32c_jmp_s(void)
{
	int label = INSTR->Arg1;
	M32C_REG_PC += label + 1;
}

void
m32c_setup_jmp_s(void)
{
	int label = (ICODE8() & 1) | ((ICODE8() >> 3) & 0x6);
	INSTR->Arg1 = label;
	INSTR->proc = m32c_jmp_s;
	INSTR->proc();
}

/**
 ***************************************************************************** 
 * void m32c_jmp_b(void)
 * Jump by a signed 8 Bit displacement.
 * v0
 ***************************************************************************** 
 */
void
m32c_jmp_b(void)
{
	int8_t dsp8;
	dsp8 = M32C_Read8(M32C_REG_PC);
	M32C_REG_PC += dsp8;
	dbgprintf("m32c_jmp_b not tested\n");
}

/**
 ***************************************************************************** 
 * void m32c_jmp_w(void)
 * Jump by a signed 16 Bit displacement.
 * v0
 ***************************************************************************** 
 */
void
m32c_jmp_w(void)
{
	int16_t dsp16;
	dsp16 = M32C_Read16(M32C_REG_PC);
	M32C_REG_PC += dsp16;
	dbgprintf("m32c_jmp_w not tested\n");
}

/**
 ******************************************************************************
 * \fn void m32c_jmp_a(void)
 * v0
 ******************************************************************************
 */
void
m32c_jmp_a(void)
{
	uint32_t abs24 = M32C_Read24(M32C_REG_PC);
	M32C_REG_PC = abs24;
	dbgprintf("m32c_jmp_a to %04x\n", abs24);
}

/**
 ******************************************************************************
 * \fn void m32c_jmpi_w_src(void)
 * Add a 16 Bit signed source to the PC.
 * v0
 ******************************************************************************
 */
static void
m32c_jmpi_w_src(void)
{
	uint32_t Src;
	INSTR->getsrc(&Src, M32C_INDEXWD());
	M32C_REG_PC = (M32C_REG_PC - 2 + (int16_t) Src) & 0xffffff;
	dbgprintf("m32c_jmpi_w_src not tested\n");
}

void
m32c_setup_jmpi_w_src(void)
{
	int codelen_src;
	int src = ((ICODE16() >> 6) & 3) | ((ICODE16() >> 7) & 0x1c);
	INSTR->getsrc = general_am_get(src, 2, &codelen_src, GAM_ALL);
	INSTR->codelen_src = codelen_src;
	INSTR->cycles = 7 + INSTR->nrMemAcc;
	INSTR->proc = m32c_jmpi_w_src;
	INSTR->proc();
}

/**
 *****************************************************************************
 * \fn void m32c_jmpi_a_src(void)
 * Jump to an absolute address.
 *****************************************************************************
 */
static void
m32c_jmpi_a_src(void)
{
	uint32_t Src;
	INSTR->getsrc(&Src, M32C_INDEXLD());
	M32C_REG_PC = (Src) & 0xffffff;
	dbgprintf("m32c_jmpi_a_src not tested\n");
}

void
m32c_setup_jmpi_a_src(void)
{
	int codelen_src;
	int src = ((ICODE16() >> 6) & 3) | ((ICODE16() >> 7) & 0x1c);
	INSTR->getsrc = general_am_get(src, 4, &codelen_src, GAM_ALL);
	INSTR->proc = m32c_jmpi_a_src;
	INSTR->proc();
}

/**
 ****************************************************
 * \fn void m32c_jmps_imm8(void)
 * Special page jump. Probably the pseudo code in
 * the Software manual is wrong.
 * v0
 ****************************************************
 */
void
m32c_jmps_imm8(void)
{
	uint32_t Src = M32C_Read8(M32C_REG_PC);
	uint32_t addr = 0xfffffe - (Src << 1);
	M32C_REG_PC = 0xff0000 | M32C_Read16(addr);
	dbgprintf("m32c_jmps_imm8 not tested\n");
}

/**
 **********************************************************
 * \fn void m32c_jsr_w_label(void)
 * Jump to a subroutine.
 * v0
 **********************************************************
 */
void
m32c_jsr_w_label(void)
{
	int16_t dsp16 = M32C_Read16(M32C_REG_PC);
	M32C_REG_PC += 2;
	M32C_REG_SP -= 2;
	M32C_Write16((M32C_REG_PC >> 16) & 0xff, M32C_REG_SP);
	M32C_REG_SP -= 2;
	M32C_Write16(M32C_REG_PC, M32C_REG_SP);
	/* Max. Jump with +32768 from instruction Start in manual is wrong */
	M32C_REG_PC = (M32C_REG_PC + dsp16 - 2) & 0xffffff;
	dbgprintf("m32c_jsr_w_label not tested\n");
}

/**
 **********************************************************
 * \fn void m32c_jsr_a_label(void)
 * Jump to a subroutine at an absolute address. 
 * v0
 **********************************************************
 */
void
m32c_jsr_a_label(void)
{
	uint32_t abs24 = M32C_Read24(M32C_REG_PC);
	M32C_REG_PC += 3;
	M32C_REG_SP -= 2;
	M32C_Write16((M32C_REG_PC >> 16) & 0xff, M32C_REG_SP);
	M32C_REG_SP -= 2;
	M32C_Write16(M32C_REG_PC & 0xffff, M32C_REG_SP);
	M32C_REG_PC = abs24;
	dbgprintf("m32c_jsr_a_label SP %04x: Destination %06x\n", M32C_REG_SP, abs24);
}

/**
 ***************************************************************************
 * \fn void m32c_jsri_w_src(void)
 * Jump to a subroutine indirect.
 * v0
 ***************************************************************************
 */
static void
m32c_jsri_w_src(void)
{
	int codelen_src = INSTR->codelen_src;
	uint32_t Src;
	INSTR->getsrc(&Src, M32C_INDEXWD());
	M32C_REG_PC += codelen_src;
	M32C_REG_SP -= 2;
	M32C_Write16(M32C_REG_PC >> 16, M32C_REG_SP);
	M32C_REG_SP -= 2;
	M32C_Write16(M32C_REG_PC & 0xffff, M32C_REG_SP);
	M32C_REG_PC = (M32C_REG_PC + (int16_t) Src - codelen_src - 2) & 0xffffff;
	dbgprintf("m32c_jsri_w_src not tested\n");
}

void
m32c_setup_jsri_w_src(void)
{
	int src = ((ICODE16() >> 6) & 3) | ((ICODE16() >> 7) & 0x1c);
	int codelen_src;
	INSTR->getsrc = general_am_get(src, 2, &codelen_src, GAM_ALL);
	INSTR->codelen_src = codelen_src;
	INSTR->cycles = 7 + INSTR->nrMemAcc;
	INSTR->proc = m32c_jsri_w_src;
	INSTR->proc();
}

/**
 ****************************************************************************
 * \fn void m32c_jsri_a_src(void)
 * Jump to a subroutine indirect absolute. 
 * v0
 ****************************************************************************
 */
static void
m32c_jsri_a_src(void)
{
	uint32_t Src;
	INSTR->getsrc(&Src, M32C_INDEXLD());
	M32C_REG_PC += INSTR->codelen_src;
	M32C_REG_SP -= 2;
	M32C_Write16(M32C_REG_PC >> 16, M32C_REG_SP);
	M32C_REG_SP -= 2;
	M32C_Write16(M32C_REG_PC & 0xffff, M32C_REG_SP);
	M32C_REG_PC = Src & 0xffffff;
	dbgprintf("m32c_jsri_a_src not tested\n");
}

void
m32c_setup_jsri_a_src(void)
{
	int src = ((ICODE16() >> 6) & 3) | ((ICODE16() >> 7) & 0x1c);
	int codelen_src;
	INSTR->getsrc = general_am_get(src, 4, &codelen_src, GAM_ALL);
	INSTR->codelen_src = codelen_src;
	INSTR->proc = m32c_jsri_a_src;
	INSTR->proc();
}

/**
 ************************************************************************
 * \fn void m32c_jsrs_imm8(void)
 * Jump to a subroutine in special page. Pseudo code in Manual
 * is probably wrong.
 * v0
 ************************************************************************
 */
void
m32c_jsrs_imm8(void)
{
	uint32_t Src = M32C_Read8(M32C_REG_PC);
	uint32_t addr = 0xfffffe - (Src << 1);
	M32C_REG_PC += 1;
	M32C_REG_SP -= 2;
	M32C_Write16((M32C_REG_PC >> 16) & 0xff, M32C_REG_SP);
	M32C_REG_SP -= 2;
	M32C_Write16(M32C_REG_PC & 0xffff, M32C_REG_SP);
	M32C_REG_PC = 0xff0000 | M32C_Read16(addr);
	dbgprintf("m32c_jsrs_imm8 not tested\n");
}

/**
 ***********************************************************************
 * \fn void m32c_ldc_imm16_dst(void)
 * Load a 16 Bit conrol register with an immediate.
 * v0
 ***********************************************************************
 */
static void
m32c_ldc_imm16_dst(void)
{
	uint16_t imm16 = M32C_Read16(M32C_REG_PC);
	int dst = INSTR->Arg1;
	setreg_cdi16(dst, imm16, 0xff);
	M32C_REG_PC += 2;
	dbgprintf("m32c_ldc_imm16_dst not tested\n");
}

void
m32c_setup_ldc_imm16_dst(void)
{
	INSTR->Arg1 = ICODE16() & 7;
	INSTR->proc = m32c_ldc_imm16_dst;
	INSTR->proc();
}

/**
 ***************************************************
 * \fn void m32c_ldc_imm24_dst(void)
 * Load a 24 Bit control register with an immediate.
 * v0
 ***************************************************
 */
static void
m32c_ldc_imm24_dst(void)
{
	uint32_t imm24 = M32C_Read24(M32C_REG_PC);
	uint32_t am = INSTR->Arg1;
	setreg_cdi24low(am, imm24, 0xbf);
	M32C_REG_PC += 3;
	dbgprintf("m32c_ldc_imm24_dst low am %d, val %04x\n", am, imm24);
}

void
m32c_setup_ldc_imm24_dst(void)
{
	INSTR->Arg1 = ICODE16() & 7;
	INSTR->proc = m32c_ldc_imm24_dst;
	INSTR->proc();
}

/**
 ***************************************************
 * \fn void m32c_ldc_imm24_dst(void)
 * Load a 24 Bit control register with an immediate.
 * v0
 ***************************************************
 */
static void
m32c_ldc_imm24_dst2(void)
{
	uint32_t imm24 = M32C_Read24(M32C_REG_PC);
	uint32_t am = INSTR->Arg1;
	setreg_cdi24high(am, imm24, 0xfc);
	M32C_REG_PC += 3;
	dbgprintf("m32c_ldc_imm24_dst2 not tested\n");
}

void
m32c_setup_ldc_imm24_dst2(void)
{
	INSTR->Arg1 = ICODE16() & 7;
	INSTR->proc = m32c_ldc_imm24_dst2;
	INSTR->proc();
}

/**
 ************************************************************
 * \fn void m32c_ldc_src_dst(void)
 * Load a 16 bit control register from src.
 * v0
 ************************************************************
 */
static void
m32c_ldc_src_dst(void)
{
	uint32_t Src;
	int dst = INSTR->Arg1;
	INSTR->getsrc(&Src, 0);
	setreg_cdi16(dst, Src, 0xff);
	M32C_REG_PC += INSTR->codelen_src;
	dbgprintf("m32c_ldc_src_dst not tested\n");
}

void
m32c_setup_ldc_src_dst(void)
{
	int codelen_src;
	int src = ((ICODE24() >> 6) & 0x3) | ((ICODE24() >> 7) & 0x1c);
	int dst = ICODE24() & 0x7;
	INSTR->getsrc = general_am_get(src, 2, &codelen_src, GAM_ALL);
	INSTR->codelen_src = codelen_src;
	INSTR->Arg1 = dst;
	INSTR->cycles = 2 + INSTR->nrMemAcc * 4;
	INSTR->proc = m32c_ldc_src_dst;
	INSTR->proc();
}

/**
 ************************************************************
 * \fn void m32c_ldc_src_dst2(void)
 * Load a 24 bit control register from src.
 * v0
 ************************************************************
 */
static void
m32c_ldc_src_dst2(void)
{
	int dst = INSTR->Arg1;
	uint32_t Src;
	INSTR->getsrc(&Src, 0);
#if 0
	fprintf(stderr, "LDC am %d, reg %d value 0x%06x\n", src, dst, Src);
	fprintf(stderr, "at %06x: %02x\n", M32C_REG_PC, M32C_Read8(M32C_REG_PC));
	fprintf(stderr, "at %06x: %02x\n", M32C_REG_PC + 1, M32C_Read8(M32C_REG_PC + 1));
	fprintf(stderr, "at %06x: %02x\n", M32C_REG_PC + 2, M32C_Read8(M32C_REG_PC + 2));
	fprintf(stderr, "at %06x: %02x\n", M32C_REG_PC + 3, M32C_Read8(M32C_REG_PC + 3));
#endif
	setreg_cdi24low(dst, Src, 0xbf);
	M32C_REG_PC += INSTR->codelen_src;
	dbgprintf("m32c_ldc_src_dst2 not implemented\n");
}

void
m32c_setup_ldc_src_dst2(void)
{
	int codelen_src;
	int dst = ICODE16() & 0x7;
	int src = ((ICODE16() >> 6) & 0x3) | ((ICODE16() >> 7) & 0x1c);
	INSTR->getsrc = general_am_get(src, 4, &codelen_src, GAM_ALL);
	INSTR->codelen_src = codelen_src;
	INSTR->Arg1 = dst;
	INSTR->cycles = 2 + INSTR->nrMemAcc * 4;
	INSTR->proc = m32c_ldc_src_dst2;
	INSTR->proc();
}

/**
 ************************************************************
 * \fn void m32c_ldc_src_dst3(void)
 * Load a 24 bit control register from src.
 * v0
 ************************************************************
 */
static void
m32c_ldc_src_dst3(void)
{
	uint32_t Src;
	int dst = INSTR->Arg1;
	INSTR->getsrc(&Src, 0);
	setreg_cdi24high(dst, Src, 0xfc);
	M32C_REG_PC += INSTR->codelen_src;
	dbgprintf("m32c_ldc_src_dst3 not tested\n");
}

void
m32c_setup_ldc_src_dst3(void)
{
	int codelen_src;
	int dst = ICODE24() & 0x7;
	int src = ((ICODE24() >> 6) & 0x3) | ((ICODE24() >> 7) & 0x1c);
	INSTR->getsrc = general_am_get(src, 4, &codelen_src, GAM_ALL);
	INSTR->codelen_src = codelen_src;
	INSTR->Arg1 = dst;
	INSTR->proc = m32c_ldc_src_dst3;
	INSTR->proc();
}

/**
 *******************************************************************
 * \fn void m32c_ldctx(void)
 * Load a context as described in a table addressed by
 * table_base + offset.
 * v0
 *******************************************************************
 */
void
m32c_ldctx(void)
{
	uint32_t table_base;
	uint32_t offset;
	uint8_t regset;
	uint16_t spcorr;
	uint32_t regp;
	offset = M32C_Read16(M32C_REG_PC) << 1;
	table_base = M32C_Read24(M32C_REG_PC + 2);
	regset = M32C_Read8((table_base + offset) & 0xffffff);
	spcorr = M32C_Read8((table_base + offset + 1) & 0xffffff);
	regp = M32C_REG_SP;
	if (regset & 1) {
		M32C_REG_R0 = M32C_Read16(M32C_REG_SP);
		regp += 2;
		CycleCounter += 1;
	}
	if (regset & 2) {
		M32C_REG_R1 = M32C_Read16(M32C_REG_SP);
		regp += 2;
		CycleCounter += 1;
	}
	if (regset & 4) {
		M32C_REG_R2 = M32C_Read16(M32C_REG_SP);
		regp += 2;
		CycleCounter += 1;
	}
	if (regset & 8) {
		M32C_REG_R3 = M32C_Read16(M32C_REG_SP);
		regp += 2;
		CycleCounter += 1;
	}
	if (regset & 0x10) {
		M32C_REG_A0 = M32C_Read32(M32C_REG_SP) & 0xffffff;
		regp += 4;
		CycleCounter += 2;
	}
	if (regset & 0x20) {
		M32C_REG_A1 = M32C_Read32(M32C_REG_SP) & 0xffffff;
		regp += 4;
		CycleCounter += 2;
	}
	if (regset & 0x40) {
		M32C_REG_SB = M32C_Read32(M32C_REG_SP) & 0xffffff;
		regp += 4;
		CycleCounter += 2;
	}
	if (regset & 0x80) {
		M32C_REG_FB = M32C_Read32(M32C_REG_SP) & 0xffffff;
		regp += 4;
		CycleCounter += 2;
	}
	if (spcorr != 0) {
		dbgprintf("Bad spcorr value in Context\n");
	}
	M32C_REG_SP += spcorr;
	if (M32C_REG_SP != regp) {
		fprintf(stderr, "Unexpected regp in ldctx\n");
	}
	M32C_REG_PC += 5;
	dbgprintf("m32c_ldctx not tested\n");
}

/** 
 ***********************************************************************
 * \fn void m32c_ldipl_imm(void)
 * Set the interrupt privilege level.
 * v0
 ***********************************************************************
 */
static void
m32c_ldipl_imm(void)
{
	uint32_t imm3 = INSTR->Imm32;
	M32C_SET_REG_FLG((M32C_REG_FLG & ~M32C_FLG_IPL_MSK)
			 | (imm3 << M32C_FLG_IPL_SHIFT));
	// update interrupt status here
	dbgprintf("m32c_ldipl_imm not implemented\n");
}

void
m32c_setup_ldipl_imm(void)
{
	INSTR->Imm32 = ICODE16() & 7;
	INSTR->proc = m32c_ldipl_imm;
	INSTR->proc();
}

/**
 *********************************************************************
 * \fn void m32c_max_size_immdst(void)
 * Store the maximum of an immediate and a destination in destination. 
 * v0
 *********************************************************************
 */
static void
m32c_max_size_immdst(void)
{
	uint32_t Src, Dst;
	int size = INSTR->opsize;
	int codelen_dst = INSTR->codelen_dst;
	INSTR->getdst(&Dst, M32C_INDEXSD());
	if (size == 2) {
		Src = M32C_Read16((M32C_REG_PC + codelen_dst) & 0xffffff);
	} else {		/* if(size == 1) */

		Src = M32C_Read8((M32C_REG_PC + codelen_dst) & 0xffffff);
	}
	if (Src > Dst) {
		INSTR->setdst(Src, M32C_INDEXSD());
	}
	M32C_REG_PC += codelen_dst + size;
	dbgprintf("m32c_max_size_immdst not tested\n");
}

void
m32c_setup_max_size_immdst(void)
{
	int dst;
	int size;
	int codelen_dst;
	dst = ((ICODE24() >> 6) & 3) | ((ICODE24() >> 7) & 0x1c);
	if (ICODE24() & 0x100) {
		size = 2;
	} else {
		size = 1;
	}
	INSTR->getdst = general_am_get(dst, size, &codelen_dst, GAM_ALL);
	INSTR->setdst = general_am_set(dst, size, &codelen_dst, GAM_ALL);
	INSTR->codelen_dst = codelen_dst;
	INSTR->opsize = size;
	INSTR->proc = m32c_max_size_immdst;
	INSTR->proc();
}

/**
 ***********************************************************************
 * \fn void m32c_max_size_srcdst(void)
 * Store the maximum of source and destination in destination.
 * v0
 ***********************************************************************
 */
static void
m32c_max_size_srcdst(void)
{
	uint32_t Src, Dst;
	int codelen_src = INSTR->codelen_src;
	int codelen_dst = INSTR->codelen_dst;
	INSTR->getsrc(&Src, M32C_INDEXSS());
	M32C_REG_PC += codelen_src;
	INSTR->getdst(&Dst, M32C_INDEXSD());
	if (Src > Dst) {
		INSTR->setdst(Src, M32C_INDEXSD());
	}
	M32C_REG_PC += codelen_dst;
	dbgprintf("m32c_max_size_srcdst not tested\n");
}

void
m32c_setup_max_size_srcdst(void)
{
	int dst, src;
	int size;
	int codelen_src;
	int codelen_dst;
	dst = ((ICODE24() >> 6) & 3) | ((ICODE24() >> 7) & 0x1c);
	src = ((ICODE24() >> 4) & 3) | ((ICODE24() >> 10) & 0x1c);
	if (ICODE24() & 0x100) {
		size = 2;
	} else {
		size = 1;
	}
	INSTR->getdst = general_am_get(dst, size, &codelen_dst, GAM_ALL);
	INSTR->getsrc = general_am_get(src, size, &codelen_src, GAM_ALL);
	INSTR->setdst = general_am_set(dst, size, &codelen_dst, GAM_ALL);
	INSTR->codelen_src = codelen_src;
	INSTR->codelen_dst = codelen_dst;
	INSTR->opsize = size;
	INSTR->proc = m32c_max_size_srcdst;
	INSTR->proc();
}

/**
 ***********************************************************************
 * \fn void m32c_min_size_immdst(void)
 * Store the minimum of an immediate and a destination in destination.
 * v0
 ************************************************************************
 */
static void
m32c_min_size_immdst(void)
{
	uint32_t Src, Dst;
	int size = INSTR->opsize;
	int codelen_dst = INSTR->codelen_dst;
	INSTR->getdst(&Dst, M32C_INDEXSD());
	if (size == 2) {
		Src = M32C_Read16((M32C_REG_PC + codelen_dst) & 0xffffff);
	} else {		/* if(size == 1) */

		Src = M32C_Read8((M32C_REG_PC + codelen_dst) & 0xffffff);
	}
	if (Src < Dst) {
		INSTR->setdst(Src, M32C_INDEXSD());
	}
	M32C_REG_PC += codelen_dst + size;
	dbgprintf("m32c_min_size_immdst not tested\n");
}

void
m32c_setup_min_size_immdst(void)
{
	int dst;
	int size;
	int codelen_dst;
	dst = ((ICODE24() >> 6) & 3) | ((ICODE24() >> 7) & 0x1c);
	if (ICODE24() & 0x100) {
		size = 2;
	} else {
		size = 1;
	}
	INSTR->getdst = general_am_get(dst, size, &codelen_dst, GAM_ALL);
	INSTR->setdst = general_am_set(dst, size, &codelen_dst, GAM_ALL);
	INSTR->codelen_dst = codelen_dst;
	INSTR->opsize = size;
	INSTR->proc = m32c_min_size_immdst;
	INSTR->proc();
}

/**
 ***********************************************************************
 * \fn void m32c_min_size_srcdst(void)
 * Store the minimum of a source and a destination in destination.
 * v0
 ************************************************************************
 */
static void
m32c_min_size_srcdst(void)
{
	uint32_t Src, Dst;
	int codelen_src = INSTR->codelen_src;
	int codelen_dst = INSTR->codelen_dst;
	INSTR->getsrc(&Src, M32C_INDEXSS());
	M32C_REG_PC += codelen_src;
	INSTR->getdst(&Dst, M32C_INDEXSD());
	if (Src < Dst) {
		INSTR->setdst(Src, M32C_INDEXSD());
	}
	M32C_REG_PC += codelen_dst;
	dbgprintf("m32c_min_size_srcdst not tested\n");
}

void
m32c_setup_min_size_srcdst(void)
{
	int dst, src;
	int size;
	int codelen_src;
	int codelen_dst;
	dst = ((ICODE24() >> 6) & 3) | ((ICODE24() >> 7) & 0x1c);
	src = ((ICODE24() >> 4) & 3) | ((ICODE24() >> 10) & 0x1c);
	if (ICODE24() & 0x100) {
		size = 2;
	} else {
		size = 1;
	}
	INSTR->getdst = general_am_get(dst, size, &codelen_dst, GAM_ALL);
	INSTR->getsrc = general_am_get(src, size, &codelen_src, GAM_ALL);
	INSTR->setdst = general_am_set(dst, size, &codelen_dst, GAM_ALL);
	INSTR->codelen_dst = codelen_dst;
	INSTR->codelen_src = codelen_src;
	INSTR->opsize = size;
	INSTR->proc = m32c_min_size_srcdst;
	INSTR->proc();
}

/**
 *********************************************************
 * \fn void m32c_mov_size_g_immdst(void)
 * Move an immediate to a destination.
 * v0
 *********************************************************
 */
static void
m32c_mov_size_g_immdst(void)
{
	uint32_t Src, Result;
	int srcsize = INSTR->srcsize;
	int opsize = INSTR->opsize;
	int codelen_dst = INSTR->codelen_dst;
	if (srcsize == 2) {
		Src = M32C_Read16((M32C_REG_PC + codelen_dst) & 0xffffff);
	} else {		/* if (INSTR->srcsize == 1) */

		Src = M32C_Read8((M32C_REG_PC + codelen_dst) & 0xffffff);
	}
	Result = Src;
	INSTR->setdst(Result, M32C_INDEXSD());
	mov_flags(Result, opsize);
	M32C_REG_PC += codelen_dst + srcsize;
	dbgprintf("m32c_mov_size_g_immdst not tested\n");
}

void
m32c_setup_mov_size_g_immdst(void)
{
	int dst;
	int immsize, opsize;
	int codelen_dst;
	dst = ((ICODE16() >> 6) & 3) | ((ICODE16() >> 7) & 0x1c);
	if (ICODE16() & 0x100) {
		immsize = opsize = 2;
	} else {
		immsize = opsize = 1;
		ModOpsize(dst, &opsize);
	}
	INSTR->setdst = general_am_set(dst, opsize, &codelen_dst, GAM_ALL);
	INSTR->codelen_dst = codelen_dst;
	INSTR->opsize = opsize;
	INSTR->srcsize = immsize;
	INSTR->cycles = 1 + INSTR->nrMemAcc;
	INSTR->proc = m32c_mov_size_g_immdst;
	INSTR->proc();
}

/**
 *********************************************************
 * \fn void m32c_mov_size_g_immdst(void)
 * Move an immediate to an indirect destination.
 * v0
 *********************************************************
 */
static void
m32c_mov_size_g_immidst(void)
{
	uint32_t Src, Result;
	uint32_t DstP;
	int size = INSTR->opsize;
	int codelen_dst = INSTR->codelen_dst;
	INSTR->getdstp(&DstP, 0);
	DstP = (DstP + M32C_INDEXSD()) & 0xffffff;
	if (size == 2) {
		Src = M32C_Read16((M32C_REG_PC + codelen_dst) & 0xffffff);
		Result = Src;
		M32C_Write16(Result, DstP);
	} else {		/* if (size == 1) */

		Src = M32C_Read8((M32C_REG_PC + codelen_dst) & 0xffffff);
		Result = Src;
		M32C_Write8(Result, DstP);
	}
	mov_flags(Result, size);
	M32C_REG_PC += codelen_dst + size;
	dbgprintf("m32c_mov_size_g_immidst not tested\n");
}

void
m32c_setup_mov_size_g_immidst(void)
{
	int dst;
	int size;
	int codelen_dst;
	dst = ((ICODE24() >> 6) & 3) | ((ICODE24() >> 7) & 0x1c);
	if (ICODE24() & 0x100) {
		size = 2;
	} else {
		size = 1;
	}
	INSTR->getdstp = general_am_get(dst, 4, &codelen_dst, GAM_ALL);
	INSTR->codelen_dst = codelen_dst;
	INSTR->opsize = size;
	INSTR->cycles = 3 + INSTR->nrMemAcc;
	INSTR->proc = m32c_mov_size_g_immidst;
	INSTR->proc();
}

/**
 **********************************************************************
 * \fn void m32c_mov_l_g_immdst(void)
 * Move a long immediate to a destination.
 * v0
 **********************************************************************
 */
static void
m32c_mov_l_g_immdst(void)
{
	uint32_t Src, Result;
	Src = M32C_Read32(M32C_REG_PC + INSTR->codelen_dst);
	Result = Src;
	INSTR->setdst(Result, M32C_INDEXLD());
	mov_flags(Result, 4);
	M32C_REG_PC += INSTR->codelen_dst + 4;
	dbgprintf("m32c_mov_l_g_immdst not tested\n");
}

void
m32c_setup_mov_l_g_immdst(void)
{
	int dst;
	int codelen_dst;
	dst = ((ICODE16() >> 6) & 3) | ((ICODE16() >> 7) & 0x1c);
	INSTR->setdst = general_am_set(dst, 4, &codelen_dst, GAM_ALL);
	INSTR->codelen_dst = codelen_dst;
	INSTR->cycles = 2 + INSTR->nrMemAcc;
	INSTR->proc = m32c_mov_l_g_immdst;
	INSTR->proc();
}

/**
 **********************************************************************
 * \fn void m32c_mov_l_g_immdst(void)
 * Move a long immediate to an indirect destination.
 * v0
 **********************************************************************
 */

static void
m32c_mov_l_g_immidst(void)
{
	uint32_t Src, Result;
	uint32_t DstP;
	int codelen_dst = INSTR->codelen_dst;
	INSTR->getdstp(&DstP, 0);
	DstP = (DstP + M32C_INDEXLD()) & 0xffffff;
	Src = M32C_Read32(M32C_REG_PC + codelen_dst);
	Result = Src;
	M32C_Write32(Result, DstP);
	mov_flags(Result, 4);
	M32C_REG_PC += codelen_dst + 4;
	dbgprintf("m32c_mov_l_g_immidst not tested\n");
}

void
m32c_setup_mov_l_g_immidst(void)
{
	int dst;
	int codelen_dst;
	dst = ((ICODE24() >> 6) & 3) | ((ICODE24() >> 7) & 0x1c);
	INSTR->getdstp = general_am_get(dst, 4, &codelen_dst, GAM_ALL);
	INSTR->codelen_dst = codelen_dst;
	INSTR->cycles = 5 + INSTR->nrMemAcc;
	INSTR->proc = m32c_mov_l_g_immidst;
	INSTR->proc();
}

/**
 *********************************************************************
 * \fn void m32c_mov_size_q_imm4dst(void)
 * Move a 4 bit 2's complement immediate to a destination. 
 * v0
 *********************************************************************
 */
static void
m32c_mov_size_q_imm4dst(void)
{
	int32_t imm4 = INSTR->Imm32;
	int opsize = INSTR->opsize;
	INSTR->setdst(imm4, M32C_INDEXSD());
	mov_flags(imm4, opsize);
	M32C_REG_PC += INSTR->codelen_dst;
	dbgprintf("m32c_mov_size_q_imm4dst not tested cdl %d, dst %02x, ICODE16() %04x\n", codelen,
		  dst, ICODE16());
}

void
m32c_setup_mov_size_q_imm4dst(void)
{
	int32_t imm4 = ((int32_t) ((ICODE16() & 0xf) << 28)) >> 28;
	int codelen_dst;
	int opsize;
	int dst = ((ICODE16() >> 6) & 3) | ((ICODE16() >> 7) & 0x1c);
	if (ICODE16() & 0x100) {
		opsize = 2;
	} else {
		opsize = 1;
		ModOpsize(dst, &opsize);
		if (opsize == 2) {
			imm4 = imm4 & 0xff;
		}
	}
	INSTR->setdst = general_am_set(dst, opsize, &codelen_dst, GAM_ALL);
	INSTR->codelen_dst = codelen_dst;
	INSTR->opsize = opsize;
	INSTR->Imm32 = (uint32_t) imm4;
	INSTR->cycles = 1;
	INSTR->proc = m32c_mov_size_q_imm4dst;
	INSTR->proc();
}

/**
 **************************************************************************
 * \fn void m32c_mov_size_q_imm4idst(void)
 * Move a 4 bit 2's complement immediate to an indirect destination. 
 * v0
 **************************************************************************
 */
static void
m32c_mov_size_q_imm4idst(void)
{
	uint32_t DstP;
	int32_t imm4 = INSTR->Imm32;
	int size = INSTR->opsize;
	INSTR->getdstp(&DstP, 0);
	DstP = (DstP + M32C_INDEXSD()) & 0xffffff;	/* Index ???? */
	if (size == 2) {
		M32C_Write16(imm4, DstP);
	} else {
		M32C_Write8(imm4, DstP);
	}
	mov_flags(imm4, size);
	M32C_REG_PC += INSTR->codelen_dst;
	dbgprintf("m32c_mov_size_q_imm4idst not tested\n");
}

void
m32c_setup_mov_size_q_imm4idst(void)
{
	int32_t imm4 = ((int32_t) ((ICODE24() & 0xf) << 28)) >> 28;
	int codelen_dst;
	int size;
	int dst = ((ICODE24() >> 6) & 3) | ((ICODE24() >> 7) & 0x1c);
	if (ICODE24() & 0x100) {
		size = 2;
	} else {
		size = 1;
	}
	INSTR->getdstp = general_am_get(dst, 4, &codelen_dst, GAM_ALL);
	INSTR->codelen_dst = codelen_dst;
	INSTR->opsize = size;
	INSTR->Imm32 = imm4;
	INSTR->cycles = 4;
	INSTR->proc = m32c_mov_size_q_imm4idst;
	INSTR->proc();
}

/**
 ********************************************************************
 * \fn void m32c_mov_size_s_immdst(void)
 * Move an immediate to a destination.
 * v0
 ********************************************************************
 */
static void
m32c_mov_size_s_immdst(void)
{
	int size = INSTR->opsize;
	uint32_t Src;
	uint32_t Dst;
	int codelen_dst = INSTR->codelen_dst;
	if (size == 2) {
		Src = M32C_Read16(M32C_REG_PC + codelen_dst);
	} else {
		Src = M32C_Read8(M32C_REG_PC + codelen_dst);
	}
	Dst = Src;
	INSTR->setam2bit(Dst, M32C_INDEXSD());	/* Index ???? */
	mov_flags(Dst, size);
	M32C_REG_PC += codelen_dst + size;
	dbgprintf("m32c_mov_size_s_immdst not tested\n");
}

void
m32c_setup_mov_size_s_immdst(void)
{
	int dst = (ICODE8() >> 4) & 3;
	int size;
	int codelen_dst;
	//codelen_dst = am2bit_codelen(dst);
	if (ICODE8() & 1) {
		size = 2;
	} else {
		size = 1;
	}
	INSTR->setam2bit = am2bit_setproc(dst, &codelen_dst, size);
	INSTR->codelen_dst = codelen_dst;
	INSTR->opsize = size;
	INSTR->proc = m32c_mov_size_s_immdst;
	INSTR->proc();
}

/**
 ********************************************************************
 * \fn void m32c_mov_size_s_immidst(void)
 * Move an immediate to an indirect destination.
 * v0
 ********************************************************************
 */

static void
m32c_mov_size_s_immidst(void)
{
	int size = INSTR->opsize;
	uint32_t Src;
	uint32_t DstP;
	int codelen_dst = INSTR->codelen_dst;
	INSTR->getam2bit(&DstP, 0);
	if (size == 2) {
		Src = M32C_Read16(M32C_REG_PC + codelen_dst);
		DstP = (DstP + M32C_INDEXWD()) & 0xffffff;	/* Index ??? */
		M32C_Write16(Src, DstP);
	} else {
		Src = M32C_Read8(M32C_REG_PC + codelen_dst);
		DstP = (DstP + M32C_INDEXBD()) & 0xffffff;
		M32C_Write8(Src, DstP);
	}
	mov_flags(Src, size);
	M32C_REG_PC += codelen_dst + size;
	dbgprintf("m32c_mov_size_s_immidst not tested\n");
}

void
m32c_setup_mov_size_s_immidst(void)
{
	int dst = (ICODE16() >> 4) & 3;
	int size;
	int codelen_dst;
	if (ICODE16() & 1) {
		size = 2;
	} else {
		size = 1;
	}
	INSTR->getam2bit = am2bit_getproc(dst, &codelen_dst, 4);
	INSTR->codelen_dst = codelen_dst;
	INSTR->opsize = size;
	INSTR->proc = m32c_mov_size_s_immidst;
	INSTR->proc();
}

/**
 **********************************************************+
 * \fn void m32c_mov_size_s_imma0a1(void)
 * Move an 24 or 16 Bit immediate to A1/A0 
 * Check if flags are based on 16 Bit result for word operation
 * Check
 **********************************************************+
 */
static void
m32c_mov_w_s_imma0(void)
{
	uint32_t Dst;
	Dst = M32C_Read16((M32C_REG_PC) & 0xffffff);
	M32C_REG_PC += 2;
	M32C_REG_A0 = Dst;
	mov_flags(Dst, 2);
	dbgprintf("m32c_mov_size_s_imma0a1 not tested\n");
}

static void
m32c_mov_l_s_imma0(void)
{
	uint32_t Dst;
	Dst = M32C_Read24((M32C_REG_PC) & 0xffffff);
	M32C_REG_PC += 3;
	M32C_REG_A0 = Dst;
	mov_flags(Dst, 4);
	dbgprintf("m32c_mov_size_s_imma0a1 not tested\n");
}

static void
m32c_mov_w_s_imma1(void)
{
	uint32_t Dst;
	Dst = M32C_Read16((M32C_REG_PC) & 0xffffff);
	M32C_REG_PC += 2;
	M32C_REG_A1 = Dst;
	mov_flags(Dst, 2);
	dbgprintf("m32c_mov_size_s_imma0a1 not tested\n");
}

static void
m32c_mov_l_s_imma1(void)
{
	uint32_t Dst;
	Dst = M32C_Read24((M32C_REG_PC) & 0xffffff);
	M32C_REG_PC += 3;
	M32C_REG_A1 = Dst;
	mov_flags(Dst, 4);
	dbgprintf("m32c_mov_size_s_imma0a1 not tested\n");
}

void
m32c_setup_mov_size_s_imma0a1(void)
{
	int dst = ICODE8() & 1;
	int opsize;
	if (ICODE8() & 0x20) {
		opsize = 4;
	} else {
		opsize = 2;
	}
	if (dst) {
		if (opsize == 2) {
			INSTR->proc = m32c_mov_w_s_imma1;
			INSTR->cycles = 1;
		} else {
			INSTR->proc = m32c_mov_l_s_imma1;
			INSTR->cycles = 2;
		}
	} else {
		if (opsize == 2) {
			INSTR->cycles = 1;
			INSTR->proc = m32c_mov_w_s_imma0;
		} else {
			INSTR->cycles = 2;
			INSTR->proc = m32c_mov_l_s_imma0;
		}
	}
	INSTR->proc();
}

/**
 ************************************************************
 * \fn void m32c_mov_size_z_0dst(void)
 * Move a zero to a destination.
 * v0
 ************************************************************
 */
static void
m32c_mov_size_z_0dst(void)
{
	INSTR->setam2bit(0, M32C_INDEXSD());	/* INDEX ?? */
	M32C_REG_FLG &= ~M32C_FLG_SIGN;
	M32C_REG_FLG |= M32C_FLG_ZERO;
	M32C_REG_PC += INSTR->codelen_dst;
	dbgprintf("m32c_mov_size_z_0dst am %d, codelen %d\n", dst, codelen);
}

void
m32c_setup_mov_size_z_0dst(void)
{
	int dst = (ICODE8() >> 4) & 3;
	int codelen_dst;
	int size;
	if (ICODE8() & 1) {
		size = 2;
	} else {
		size = 1;
	}
	INSTR->setam2bit = am2bit_setproc(dst, &codelen_dst, size);
	INSTR->codelen_dst = codelen_dst;
	INSTR->opsize = size;
	INSTR->proc = m32c_mov_size_z_0dst;
	INSTR->proc();
}

/**
 ************************************************************
 * \fn void m32c_mov_size_z_0dst(void)
 * Move a zero to an indirect destination.
 * v0
 ************************************************************
 */
static void
m32c_mov_size_z_0idst(void)
{
	uint32_t DstP;
	int size = INSTR->opsize;
	INSTR->getam2bit(&DstP, 0);
	if (size == 2) {
		DstP = (DstP + M32C_INDEXWD()) & 0xffffff;	/* Index ??? */
		M32C_Write16(0, DstP);
	} else {
		DstP = (DstP + M32C_INDEXBD()) & 0xffffff;
		M32C_Write8(0, DstP);
	}
	M32C_REG_FLG &= ~M32C_FLG_SIGN;
	M32C_REG_FLG |= M32C_FLG_ZERO;
	M32C_REG_PC += INSTR->codelen_dst;
	dbgprintf("m32c_mov_size_z_0idst not tested\n");
}

void
m32c_setup_mov_size_z_0idst(void)
{
	int dst = (ICODE16() >> 4) & 3;
	int codelen_dst;
	int size;
	if (ICODE16() & 1) {
		size = 2;
	} else {
		size = 1;
	}
	INSTR->getam2bit = am2bit_getproc(dst, &codelen_dst, 4);
	INSTR->codelen_dst = codelen_dst;
	INSTR->opsize = size;
	INSTR->proc = m32c_mov_size_z_0idst;
	INSTR->proc();
}

/**
 *****************************************************************************
 * \fn void m32c_mov_size_g_srcdst(void)
 * Move a source to a destination.
 * v0
 *****************************************************************************
 */
static void
m32c_mov_b_g_srcdst(void)
{
	uint32_t Src, Dst;
	INSTR->getsrc(&Src, M32C_INDEXSS());
	M32C_REG_PC += INSTR->codelen_src;
	Dst = Src;
	INSTR->setdst(Dst, M32C_INDEXSD());
	movb_flags(Dst);
	M32C_REG_PC += INSTR->codelen_dst;
	dbgprintf("m32c_mov_size_srcdst not tested\n");
}

static void
m32c_mov_w_g_srcdst(void)
{
	uint32_t Src, Dst;
	INSTR->getsrc(&Src, M32C_INDEXSS());
	M32C_REG_PC += INSTR->codelen_src;
	Dst = Src;
	INSTR->setdst(Dst, M32C_INDEXSD());
	movw_flags(Dst);
	M32C_REG_PC += INSTR->codelen_dst;
}

static void
fast_mov_cycles(int add, int src, int dst)
{
	if (dst == 0x12 || (dst == 0x13) || (dst == 0x10) || (dst == 0x11)
	    || (dst == 0x2) || (dst == 0x3) || (dst == 0x0) || (dst == 0x1)) {
		switch (src) {
		    case 0x12:
		    case 0x13:
		    case 0x10:
		    case 0x11:
		    case 0x2:
		    case 0x3:
			    INSTR->cycles = 0;
			    break;
		    default:
			    INSTR->cycles = 2;
			    break;
		}
	} else {
		switch (src) {
		    case 0x12:
		    case 0x13:
		    case 0x10:
		    case 0x11:
		    case 0x2:
		    case 0x3:
			    INSTR->cycles = 0;
			    break;
		    default:
			    INSTR->cycles = 1;
			    break;
		}
	}
	INSTR->cycles += add;
}

void
m32c_setup_mov_size_g_srcdst(void)
{
	int dst, src;
	int opsize, srcsize;
	int codelen_src;
	int codelen_dst;
	dst = ((ICODE16() >> 6) & 3) | ((ICODE16() >> 7) & 0x1c);
	src = ((ICODE16() >> 4) & 3) | ((ICODE16() >> 10) & 0x1c);
	if (ICODE16() & 0x100) {
		opsize = srcsize = 2;
		INSTR->proc = m32c_mov_w_g_srcdst;
	} else {
		opsize = srcsize = 1;
		ModOpsize(dst, &opsize);
		if (opsize == 2) {
			INSTR->proc = m32c_mov_w_g_srcdst;
		} else {
			INSTR->proc = m32c_mov_b_g_srcdst;
		}
	}
	INSTR->getsrc = general_am_get(src, srcsize, &codelen_src, GAM_ALL);
	INSTR->setdst = general_am_set(dst, opsize, &codelen_dst, GAM_ALL);
	INSTR->codelen_src = codelen_src;
	INSTR->codelen_dst = codelen_dst;
	INSTR->opsize = opsize;
	INSTR->srcsize = srcsize;
	fast_mov_cycles(1, src, dst);
	INSTR->proc();
}

/**
 *****************************************************************************
 * \fn void m32c_mov_size_g_isrcdst(void)
 * Move an indirect source to a destination.
 * v0
 *****************************************************************************
 */
static void
m32c_mov_size_g_isrcdst(void)
{
	uint32_t Src, Dst;
	uint32_t SrcP;
	int opsize = INSTR->opsize;
	int srcsize = INSTR->srcsize;
	int codelen_src = INSTR->codelen_src;
	int codelen_dst = INSTR->codelen_dst;
	INSTR->getsrcp(&SrcP, 0);
	SrcP = (SrcP + M32C_INDEXSS()) & 0xffffff;
	if (srcsize == 2) {
		Src = M32C_Read16(SrcP);
	} else {
		Src = M32C_Read8(SrcP);
	}
	M32C_REG_PC += codelen_src;
	Dst = Src;
	INSTR->setdst(Dst, M32C_INDEXSD());
	mov_flags(Dst, opsize);
	M32C_REG_PC += codelen_dst;
	dbgprintf("m32c_mov_size_isrcdst not tested\n");
}

void
m32c_setup_mov_size_g_isrcdst(void)
{
	int dst, src;
	int opsize, srcsize;
	int codelen_src;
	int codelen_dst;
	dst = ((ICODE24() >> 6) & 3) | ((ICODE24() >> 7) & 0x1c);
	src = ((ICODE24() >> 4) & 3) | ((ICODE24() >> 10) & 0x1c);
	if (ICODE24() & 0x100) {
		opsize = srcsize = 2;
	} else {
		opsize = srcsize = 1;
		ModOpsize(dst, &opsize);
	}
	INSTR->getsrcp = general_am_get(src, 4, &codelen_src, GAM_ALL);
	INSTR->setdst = general_am_set(dst, opsize, &codelen_dst, GAM_ALL);
	INSTR->codelen_src = codelen_src;
	INSTR->codelen_dst = codelen_dst;
	INSTR->opsize = opsize;
	INSTR->srcsize = srcsize;
	fast_mov_cycles(4, src, dst);
	INSTR->proc = m32c_mov_size_g_isrcdst;
	INSTR->proc();
}

/**
 *****************************************************************************
 * \fn void m32c_mov_size_g_srcidst(void)
 * Move a source to an indirect destination.
 * v0
 *****************************************************************************
 */
static void
m32c_mov_size_g_srcidst(void)
{
	uint32_t Src, Dst;
	uint32_t DstP;
	int size = INSTR->opsize;
	int codelen_src = INSTR->codelen_src;
	int codelen_dst = INSTR->codelen_dst;
	INSTR->getsrc(&Src, M32C_INDEXSS());
	M32C_REG_PC += codelen_src;
	INSTR->getdstp(&DstP, 0);
	DstP = (DstP + M32C_INDEXSD()) & 0xffffff;
	Dst = Src;
	if (size == 2) {
		M32C_Write16(Dst, DstP);
	} else {
		M32C_Write8(Dst, DstP);
	}
	mov_flags(Dst, size);
	M32C_REG_PC += codelen_dst;
	dbgprintf("m32c_mov_size_srcidst not tested\n");
}

void
m32c_setup_mov_size_g_srcidst(void)
{
	int dst, src;
	int size;
	int codelen_src;
	int codelen_dst;
	dst = ((ICODE24() >> 6) & 3) | ((ICODE24() >> 7) & 0x1c);
	src = ((ICODE24() >> 4) & 3) | ((ICODE24() >> 10) & 0x1c);
	if (ICODE24() & 0x100) {
		size = 2;
	} else {
		size = 1;
	}
	INSTR->getsrc = general_am_get(src, size, &codelen_src, GAM_ALL);
	INSTR->getdstp = general_am_get(dst, 4, &codelen_dst, GAM_ALL);
	INSTR->codelen_src = codelen_src;
	INSTR->codelen_dst = codelen_dst;
	INSTR->opsize = size;
	fast_mov_cycles(4, src, dst);
	INSTR->proc = m32c_mov_size_g_srcidst;
	INSTR->proc();
}

/**
 *****************************************************************************
 * \fn void m32c_mov_size_g_isrcidst(void)
 * Move an indirect source to an indirect destination.
 * v0
 *****************************************************************************
 */
static void
m32c_mov_size_g_isrcidst(void)
{
	uint32_t Src, Dst;
	uint32_t SrcP, DstP;
	int size = INSTR->opsize;
	int codelen_src = INSTR->codelen_src;
	int codelen_dst = INSTR->codelen_dst;
	INSTR->getsrcp(&SrcP, 0);
	M32C_REG_PC += codelen_src;
	INSTR->getdstp(&DstP, 0);
	SrcP = (SrcP + M32C_INDEXSS()) & 0xffffff;
	DstP = (DstP + M32C_INDEXSD()) & 0xffffff;
	if (size == 2) {
		Src = M32C_Read16(SrcP);
		Dst = Src;
		M32C_Write16(Dst, DstP);
	} else {
		Src = M32C_Read8(SrcP);
		Dst = Src;
		M32C_Write8(Dst, DstP);
	}
	mov_flags(Dst, size);
	M32C_REG_PC += codelen_dst;
	dbgprintf("m32c_mov_size_isrcidst not tested\n");
}

void
m32c_setup_mov_size_g_isrcidst(void)
{
	int dst, src;
	int size;
	int codelen_src;
	int codelen_dst;
	dst = ((ICODE24() >> 6) & 3) | ((ICODE24() >> 7) & 0x1c);
	src = ((ICODE24() >> 4) & 3) | ((ICODE24() >> 10) & 0x1c);
	if (ICODE24() & 0x100) {
		size = 2;
	} else {
		size = 1;
	}
	INSTR->getsrcp = general_am_get(src, 4, &codelen_src, GAM_ALL);
	INSTR->getdstp = general_am_get(dst, 4, &codelen_dst, GAM_ALL);
	INSTR->codelen_src = codelen_src;
	INSTR->codelen_dst = codelen_dst;
	INSTR->opsize = size;
	fast_mov_cycles(7, src, dst);
	INSTR->proc = m32c_mov_size_g_isrcidst;
	INSTR->proc();
}

/**
 *****************************************************************************
 * \fn void m32c_mov_l_g_srcdst(void)
 * Move a long source to a destination.
 * v0
 *****************************************************************************
 */
static void
m32c_mov_l_g_srcdst(void)
{
	uint32_t Src;
	int size = 4;
	INSTR->getsrc(&Src, M32C_INDEXLS());
	M32C_REG_PC += INSTR->codelen_src;
	INSTR->setdst(Src, M32C_INDEXLD());
	mov_flags(Src, size);
	M32C_REG_PC += INSTR->codelen_dst;
	dbgprintf("m32c_mov_l_g_srcdst not tested\n");
}

void
m32c_setup_mov_l_g_srcdst(void)
{
	int dst, src;
	int codelen_src;
	int codelen_dst;
	int size = 4;
	dst = ((ICODE16() >> 6) & 3) | ((ICODE16() >> 7) & 0x1c);
	src = ((ICODE16() >> 4) & 3) | ((ICODE16() >> 10) & 0x1c);
	INSTR->setdst = general_am_set(dst, size, &codelen_dst, GAM_ALL);
	INSTR->getsrc = general_am_get(src, size, &codelen_src, GAM_ALL);
	INSTR->codelen_src = codelen_src;
	INSTR->codelen_dst = codelen_dst;
	fast_mov_cycles(2, src, dst);
	INSTR->proc = m32c_mov_l_g_srcdst;
	INSTR->proc();
}

/**
 *****************************************************************************
 * \fn void m32c_mov_l_g_isrcdst(void)
 * Move a long indirect source to a destination.
 * v0
 *****************************************************************************
 */
static void
m32c_mov_l_g_isrcdst(void)
{
	uint32_t Src;
	uint32_t SrcP;
	int size = 4;
	int codelen_src = INSTR->codelen_src;
	int codelen_dst = INSTR->codelen_dst;
	INSTR->getsrc(&SrcP, 0);
	SrcP = (SrcP + M32C_INDEXLS());
	M32C_REG_PC += codelen_src;
	Src = M32C_Read32(SrcP);
	INSTR->setdst(Src, M32C_INDEXLD());
	mov_flags(Src, size);
	M32C_REG_PC += codelen_dst;
	dbgprintf("m32c_mov_l_g_isrcdst not tested\n");
}

void
m32c_setup_mov_l_g_isrcdst(void)
{
	int dst, src;
	int size = 4;
	int codelen_src;
	int codelen_dst;
	dst = ((ICODE24() >> 6) & 3) | ((ICODE24() >> 7) & 0x1c);
	src = ((ICODE24() >> 4) & 3) | ((ICODE24() >> 10) & 0x1c);
	INSTR->setdst = general_am_set(dst, size, &codelen_dst, GAM_ALL);
	INSTR->getsrc = general_am_get(src, size, &codelen_src, GAM_ALL);
	INSTR->codelen_src = codelen_src;
	INSTR->codelen_dst = codelen_dst;
	fast_mov_cycles(5, src, dst);
	INSTR->proc = m32c_mov_l_g_isrcdst;
	INSTR->proc();
}

/**
 *****************************************************************************
 * \fn void m32c_mov_l_g_srcidst(void)
 * Move a long source to an indirect destination.
 * v0
 *****************************************************************************
 */
static void
m32c_mov_l_g_srcidst(void)
{
	uint32_t Src;
	uint32_t DstP;
	int size = 4;
	int codelen_src = INSTR->codelen_src;
	int codelen_dst = INSTR->codelen_dst;
	INSTR->getsrc(&Src, M32C_INDEXLS());
	M32C_REG_PC += codelen_src;
	INSTR->getdstp(&DstP, 0);
	DstP = (DstP + M32C_INDEXLD()) & 0xffffff;
	M32C_Write32(Src, DstP);
	mov_flags(Src, size);
	M32C_REG_PC += codelen_dst;
	dbgprintf("m32c_mov_l_g_srcidst not tested\n");
}

void
m32c_setup_mov_l_g_srcidst(void)
{
	int dst, src;
	int codelen_src;
	int codelen_dst;
	dst = ((ICODE24() >> 6) & 3) | ((ICODE24() >> 7) & 0x1c);
	src = ((ICODE24() >> 4) & 3) | ((ICODE24() >> 10) & 0x1c);
	INSTR->getdstp = general_am_get(dst, 4, &codelen_dst, GAM_ALL);
	INSTR->getsrc = general_am_get(src, 4, &codelen_src, GAM_ALL);
	INSTR->codelen_dst = codelen_dst;
	INSTR->codelen_src = codelen_src;
	fast_mov_cycles(5, src, dst);
	INSTR->proc = m32c_mov_l_g_srcidst;
	INSTR->proc();
}

/**
 *****************************************************************************
 * \fn void m32c_mov_l_g_isrcidst(void)
 * Move from long indirect source to an indirect destination.
 * v0
 *****************************************************************************
 */
static void
m32c_mov_l_g_isrcidst(void)
{
	uint32_t Src;
	uint32_t SrcP;
	uint32_t DstP;
	int codelen_src = INSTR->codelen_src;
	int codelen_dst = INSTR->codelen_dst;
	INSTR->getsrcp(&SrcP, 0);
	SrcP = (SrcP + M32C_INDEXLS()) & 0xffffff;
	M32C_REG_PC += codelen_src;
	INSTR->getdstp(&DstP, 0);
	DstP = (DstP + M32C_INDEXLD()) & 0xffffff;
	Src = M32C_Read32(SrcP);
	M32C_Write32(Src, DstP);
	mov_flags(Src, 4);
	M32C_REG_PC += codelen_dst;
	dbgprintf("m32c_mov_l_g_isrcidst not tested\n");
}

void
m32c_setup_mov_l_g_isrcidst(void)
{
	int dst, src;
	int codelen_src;
	int codelen_dst;
	dst = ((ICODE24() >> 6) & 3) | ((ICODE24() >> 7) & 0x1c);
	src = ((ICODE24() >> 4) & 3) | ((ICODE24() >> 10) & 0x1c);
	INSTR->getdstp = general_am_get(dst, 4, &codelen_dst, GAM_ALL);
	INSTR->getsrcp = general_am_get(src, 4, &codelen_src, GAM_ALL);
	INSTR->codelen_src = codelen_src;
	INSTR->codelen_dst = codelen_dst;
	fast_mov_cycles(8, src, dst);
	INSTR->proc = m32c_mov_l_g_isrcidst;
	INSTR->proc();
}

/**
 ************************************************************************************
 * \fn void m32c_mov_size_src_r0(void)
 * Move a source to register R0/R0L.
 * v0
 ************************************************************************************
 */
static void
m32c_mov_b_src_r0(void)
{
	uint32_t Src;
	int codelen = INSTR->codelen_src;
	INSTR->getam2bit(&Src, M32C_INDEXBS());
	M32C_REG_R0L = Src;
	mov_flags(Src, 1);
	M32C_REG_PC += codelen;
}

static void
m32c_mov_w_src_r0(void)
{
	uint32_t Src;
	int codelen = INSTR->codelen_src;
	INSTR->getam2bit(&Src, M32C_INDEXWS());	/* Index ??? */
	M32C_REG_R0 = Src;
	mov_flags(Src, 2);
	M32C_REG_PC += codelen;
	dbgprintf("m32c_mov_size_src_r0 not tested\n");
}

void
m32c_setup_mov_size_src_r0(void)
{
	int src = (ICODE8() >> 4) & 3;
	int codelen;
	if (ICODE8() & 1) {
		INSTR->getam2bit = am2bit_getproc(src, &codelen, 2);
		INSTR->proc = m32c_mov_w_src_r0;
	} else {
		INSTR->getam2bit = am2bit_getproc(src, &codelen, 1);
		INSTR->proc = m32c_mov_b_src_r0;
	}
	INSTR->codelen_src = codelen;
	INSTR->proc();
}

/**
 ************************************************************************************
 * \fn void m32c_mov_size_src_r0(void)
 * Move an indirect source to register R0/R0L.
 * v0
 ************************************************************************************
 */
static void
m32c_mov_b_isrc_r0(void)
{
	uint32_t Src;
	uint32_t SrcP;
	int codelen_src = INSTR->codelen_src;
	INSTR->getam2bit(&SrcP, 0);
	SrcP = (SrcP + M32C_INDEXBS()) & 0xffffff;
	Src = M32C_Read8(SrcP);
	M32C_REG_R0L = Src;
	mov_flags(Src, 1);
	M32C_REG_PC += codelen_src;
	dbgprintf("m32c_mov_size_isrc_r0 not tested\n");
}

static void
m32c_mov_w_isrc_r0(void)
{
	uint32_t Src;
	uint32_t SrcP;
	int codelen_src = INSTR->codelen_src;
	INSTR->getam2bit(&SrcP, 0);
	SrcP = (SrcP + M32C_INDEXWS()) & 0xffffff;
	Src = M32C_Read16(SrcP);
	M32C_REG_R0 = Src;
	mov_flags(Src, 2);
	M32C_REG_PC += codelen_src;
	dbgprintf("m32c_mov_size_isrc_r0 not tested\n");
}

void
m32c_setup_mov_size_isrc_r0(void)
{
	int src = (ICODE16() >> 4) & 3;
	int codelen_src;
	INSTR->getam2bit = am2bit_getproc(src, &codelen_src, 4);
	if (ICODE16() & 1) {
		INSTR->proc = m32c_mov_w_isrc_r0;
	} else {
		INSTR->proc = m32c_mov_b_isrc_r0;
	}
	INSTR->codelen_src = codelen_src;
	INSTR->proc();
}

/**
 ************************************************************************************
 * \fn void m32c_mov_size_src_r1(void)
 * Move a source to register R1/R1L.
 * v0
 ************************************************************************************
 */
static void
m32c_mov_b_src_r1(void)
{
	uint32_t Src;
	INSTR->getam2bit(&Src, M32C_INDEXBS());
	M32C_REG_R1L = Src;
	mov_flags(Src, 1);
	M32C_REG_PC += INSTR->codelen_src;
}

static void
m32c_mov_w_src_r1(void)
{
	uint32_t Src;
	INSTR->getam2bit(&Src, M32C_INDEXWS());	/* Index ??? */
	M32C_REG_R1 = Src;
	mov_flags(Src, 2);
	M32C_REG_PC += INSTR->codelen_src;
}

void
m32c_setup_mov_size_src_r1(void)
{
	int src = (ICODE8() >> 4) & 3;
	int codelen_src;
	if (ICODE8() & 1) {
		INSTR->getam2bit = am2bit_getproc(src, &codelen_src, 2);
		INSTR->proc = m32c_mov_w_src_r1;
	} else {
		INSTR->getam2bit = am2bit_getproc(src, &codelen_src, 1);
		INSTR->proc = m32c_mov_b_src_r1;
	}
	INSTR->codelen_src = codelen_src;
	INSTR->proc();
}

/**
 ************************************************************************************
 * \fn void m32c_mov_size_isrc_r1(void)
 * Move an indirect source to register R1/R1L.
 * v0
 ************************************************************************************
 */
static void
m32c_mov_b_isrc_r1(void)
{
	uint32_t Src;
	uint32_t SrcP;
	INSTR->getam2bit(&SrcP, 0);
	SrcP = (SrcP + M32C_INDEXBS()) & 0xffffff;
	Src = M32C_Read8(SrcP);
	M32C_REG_R1L = Src;
	mov_flags(Src, 1);
	M32C_REG_PC += INSTR->codelen_src;
	dbgprintf("m32c_mov_size_src_r1 not tested\n");
}

static void
m32c_mov_w_isrc_r1(void)
{
	uint32_t Src;
	uint32_t SrcP;
	INSTR->getam2bit(&SrcP, 0);
	SrcP = (SrcP + M32C_INDEXWS()) & 0xffffff;	/* Index ??? */
	Src = M32C_Read16(SrcP);
	M32C_REG_R1 = Src;
	mov_flags(Src, 2);
	M32C_REG_PC += INSTR->codelen_src;
	dbgprintf("m32c_mov_size_src_r1 not tested\n");
}

void
m32c_setup_mov_size_isrc_r1(void)
{
	int src = (ICODE16() >> 4) & 3;
	int codelen_src;
	INSTR->getam2bit = am2bit_getproc(src, &codelen_src, 4);
	INSTR->codelen_src = codelen_src;
	if (ICODE16() & 1) {
		INSTR->proc = m32c_mov_w_isrc_r1;
	} else {
		INSTR->proc = m32c_mov_b_isrc_r1;
	}
	INSTR->proc();
}

/**
 **********************************************************************************
 * \fn void m32c_mov_size_r0dst(void)
 * Move R0/R0L to a destination.
 * v0
 **********************************************************************************
 */
static void
m32c_mov_b_r0dst(void)
{
	uint32_t Src;
	Src = M32C_REG_R0L;
	INSTR->setam2bit(Src, M32C_INDEXBD());
	mov_flags(Src, 1);
	M32C_REG_PC += INSTR->codelen_dst;
	dbgprintf("m32c_mov_size_r0dst not tested\n");
}

static void
m32c_mov_w_r0dst(void)
{
	uint32_t Src;
	Src = M32C_REG_R0;
	INSTR->setam2bit(Src, M32C_INDEXWD());	/* Index ??? */
	mov_flags(Src, 2);
	M32C_REG_PC += INSTR->codelen_dst;
	dbgprintf("m32c_mov_size_r0dst not tested\n");
}

void
m32c_setup_mov_size_r0dst(void)
{
	int dst = (ICODE8() >> 4) & 3;
	int codelen_dst;
	if (ICODE8() & 1) {
		INSTR->setam2bit = am2bit_setproc(dst, &codelen_dst, 2);	/* Index ??? */
		INSTR->proc = m32c_mov_w_r0dst;
	} else {
		INSTR->setam2bit = am2bit_setproc(dst, &codelen_dst, 1);	/* Index ??? */
		INSTR->proc = m32c_mov_b_r0dst;
	}
	INSTR->codelen_dst = codelen_dst;
	INSTR->proc();
}

/**
 *********************************************************************************
 * \fn void m32c_mov_size_r0idst(void)
 * Move R0/R0L to an indirect destination.
 *********************************************************************************
 */
static void
m32c_mov_b_r0idst(void)
{
	uint32_t Src;
	uint32_t DstP;
	INSTR->getam2bit(&DstP, 0);
	Src = M32C_REG_R0L;
	DstP = (DstP + M32C_INDEXBD()) & 0xffffff;
	M32C_Write8(Src, DstP);
	mov_flags(Src, 1);
	M32C_REG_PC += INSTR->codelen_dst;
}

static void
m32c_mov_w_r0idst(void)
{
	uint32_t Src;
	uint32_t DstP;
	INSTR->getam2bit(&DstP, 0);
	Src = M32C_REG_R0;
	DstP = (DstP + M32C_INDEXWD()) & 0xffffff;	/* Index ??? */
	M32C_Write16(Src, DstP);
	mov_flags(Src, 2);
	M32C_REG_PC += INSTR->codelen_dst;
}

void
m32c_setup_mov_size_r0idst(void)
{
	int dst = (ICODE16() >> 4) & 3;
	int codelen_dst;
	INSTR->getam2bit = am2bit_getproc(dst, &codelen_dst, 4);
	if (ICODE16() & 1) {
		INSTR->proc = m32c_mov_w_r0idst;
	} else {
		INSTR->proc = m32c_mov_b_r0idst;
	}
	INSTR->codelen_dst = codelen_dst;
	INSTR->proc();
}

/**
 *******************************************************************************
 * \fn void m32c_mov_l_s_srca0a1(void)
 * Move a long source to register A0/A1.
 * v0
 *******************************************************************************
 */
static void
m32c_mov_l_s_srca0(void)
{
	uint32_t Src;
	INSTR->getam2bit(&Src, M32C_INDEXLS());
	M32C_REG_A0 = Src & 0xffffff;
	mov_flags(Src, 4);
	M32C_REG_PC += INSTR->codelen_src;
}

static void
m32c_mov_l_s_srca1(void)
{
	uint32_t Src;
	INSTR->getam2bit(&Src, M32C_INDEXLS());
	M32C_REG_A1 = Src & 0xffffff;
	mov_flags(Src, 4);
	M32C_REG_PC += INSTR->codelen_src;
}

void
m32c_setup_mov_l_s_srca0a1(void)
{
	int src = (ICODE8() >> 4) & 3;
	int codelen_src;
	INSTR->getam2bit = am2bit_getproc(src, &codelen_src, 4);
	INSTR->codelen_src = codelen_src;
	if (ICODE8() & 1) {
		INSTR->proc = m32c_mov_l_s_srca1;
	} else {
		INSTR->proc = m32c_mov_l_s_srca0;
	}
	INSTR->proc();
}

/**
 *******************************************************************************
 * \fn void m32c_mov_l_s_isrca0a1(void)
 * Move an indirect long source to register A0/A1.
 * v0
 *******************************************************************************
 */
static void
m32c_mov_l_s_isrca0(void)
{
	uint32_t SrcP;
	uint32_t Src;
	INSTR->getam2bit(&SrcP, 0);
	Src = M32C_Read32((SrcP + M32C_INDEXLS()) & 0xffffff) & 0xffffff;
	M32C_REG_A0 = Src;
	mov_flags(Src, 4);
	M32C_REG_PC += INSTR->codelen_src;
}

static void
m32c_mov_l_s_isrca1(void)
{
	uint32_t SrcP;
	uint32_t Src;
	INSTR->getam2bit(&SrcP, 0);
	Src = M32C_Read32((SrcP + M32C_INDEXLS()) & 0xffffff) & 0xffffff;
	M32C_REG_A1 = Src;
	mov_flags(Src, 4);
	M32C_REG_PC += INSTR->codelen_src;
}

void
m32c_setup_mov_l_s_isrca0a1(void)
{
	int src = (ICODE16() >> 4) & 3;
	int codelen_src;
	INSTR->getam2bit = am2bit_getproc(src, &codelen_src, 4);
	INSTR->codelen_src = codelen_src;
	if (ICODE16() & 1) {
		INSTR->proc = m32c_mov_l_s_isrca1;
	} else {
		INSTR->proc = m32c_mov_l_s_isrca0;
	}
	INSTR->proc();
}

/**
 *******************************************************************
 * \fn void m32c_mov_size_g_dsp8spdst(void)
 * move from stack+displacement to a destination.
 *******************************************************************
 */
static void
m32c_mov_size_g_dsp8spdst(void)
{
	int opsize = INSTR->opsize;
	int srcsize = INSTR->srcsize;
	int8_t dsp8;
	int codelen_dst = INSTR->codelen_dst;
	uint32_t Dst;
	dsp8 = M32C_Read8(M32C_REG_PC);
	M32C_REG_PC++;
	if (srcsize == 2) {
		Dst = M32C_Read16(M32C_REG_SP + dsp8);
	} else {
		Dst = M32C_Read8(M32C_REG_SP + dsp8);
	}
	INSTR->setdst(Dst, 0);
	mov_flags(Dst, opsize);
	M32C_REG_PC += codelen_dst;
	dbgprintf("m32c_mov_size_g_dsp8spdst not tested\n");
}

void
m32c_setup_mov_size_g_dsp8spdst(void)
{
	int dst;
	int opsize, srcsize;
	int codelen_dst;
	dst = ((ICODE16() >> 6) & 3) | ((ICODE16() >> 7) & 0x1c);
	if (ICODE16() & 0x100) {
		srcsize = opsize = 2;
	} else {
		srcsize = opsize = 1;
		ModOpsize(dst, &opsize);
	}
	INSTR->setdst = general_am_set(dst, opsize, &codelen_dst, GAM_ALL);
	INSTR->codelen_dst = codelen_dst;
	INSTR->opsize = opsize;
	INSTR->srcsize = srcsize;
	INSTR->cycles = 3;
	INSTR->proc = m32c_mov_size_g_dsp8spdst;
	INSTR->proc();
}

/*
 *************************************************************************
 * \fn void m32c_mov_size_g_srcdsp8sp(void)
 * Move a source to an address indicated by Starckpointer + displacement.
 * v0
 *************************************************************************
 */
static void
m32c_mov_size_g_srcdsp8sp(void)
{
	int size = INSTR->opsize;
	int8_t dsp8;
	int codelen_src = INSTR->codelen_src;
	uint32_t Src;
	INSTR->getsrc(&Src, 0);
	M32C_REG_PC += codelen_src;

	dsp8 = M32C_Read8(M32C_REG_PC);
	M32C_REG_PC++;
	if (size == 2) {
		M32C_Write16(Src, M32C_REG_SP + dsp8);
		mov_flags(Src, 2);
	} else {
		M32C_Write8(Src, M32C_REG_SP + dsp8);
		mov_flags(Src, 1);
	}
}

void
m32c_setup_mov_size_g_srcdsp8sp(void)
{
	int src;
	int size;
	int codelen_src;
	src = ((ICODE16() >> 6) & 3) | ((ICODE16() >> 7) & 0x1c);
	if (ICODE16() & 0x100) {
		size = 2;
	} else {
		size = 1;
	}
	INSTR->getsrc = general_am_get(src, size, &codelen_src, GAM_ALL);
	INSTR->codelen_src = codelen_src;
	INSTR->opsize = size;
	INSTR->cycles = 3;
	INSTR->proc = m32c_mov_size_g_srcdsp8sp;
	INSTR->proc();
}

/**
 ***************************************************************************
 * \fn void m32c_mova_srcdst(void)
 * Move the effective address of source to destination.
 * v0
 ***************************************************************************
 */
static void
m32c_mova_srcdst(void)
{
	int src = INSTR->Arg1;
	int dst = INSTR->Arg2;
	uint32_t Efa;
	int codelen_src;
	Efa = general_am_efa(src, &codelen_src, GAM_ALL);
	M32C_REG_PC += codelen_src;
	switch (dst) {
	    case 0:
		    M32C_REG_R0 = Efa;
		    M32C_REG_R2 = Efa >> 16;
		    break;
	    case 1:
		    M32C_REG_R1 = Efa;
		    M32C_REG_R3 = Efa >> 16;
		    break;
	    case 2:
		    M32C_REG_A0 = Efa;
		    break;
	    case 3:
		    M32C_REG_A1 = Efa;
		    break;
	}
	dbgprintf("m32c_mova_srcdst not tested\n");
}

void
m32c_setup_mova_srcdst(void)
{
	int src;
	int dst;
	dst = ICODE16() & 7;
	src = ((ICODE16() >> 6) & 3) | ((ICODE16() >> 7) & 0x1c);
	INSTR->Arg1 = src;
	INSTR->Arg2 = dst;
	INSTR->proc = m32c_mova_srcdst;
	INSTR->cycles = 2;
	INSTR->proc();
}

/**
 ******************************************************************
 * \fn void m32c_movdir_r0ldst(void)
 * Move nibble swapped from R0L to a destination.
 * v0
 ******************************************************************
 */
static void
m32c_movdir_r0ldst(void)
{
	int Oh = INSTR->Arg1;
	uint32_t Src, Dst;
	int codelen_dst = INSTR->codelen_dst;
	INSTR->getdst(&Dst, 0);
	Src = M32C_REG_R0L;
	switch (Oh) {
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
	INSTR->setdst(Dst, 0);
	M32C_REG_PC += codelen_dst;
	dbgprintf("m32c_movdir_r0ldst not tested\n");
}

void
m32c_setup_movdir_r0ldst(void)
{
	int Oh = (ICODE24() >> 4) & 3;
	int size = 1;
	int codelen_dst;
	int dst = ((ICODE24() >> 6) & 3) | ((ICODE24() >> 7) & 0x1c);
	INSTR->getdst = general_am_get(dst, size, &codelen_dst, GAM_ALL);
	INSTR->setdst = general_am_set(dst, size, &codelen_dst, GAM_ALL);
	INSTR->codelen_dst = codelen_dst;
	INSTR->Arg1 = Oh;
	switch (Oh) {
	    case 0:		/* LL */
	    case 3:		// HH
		    break;
	    case 1:		// HL
	    case 2:		// LH
		    INSTR->cycles += 3;
		    break;
	}
	INSTR->proc = m32c_movdir_r0ldst;
	INSTR->proc();
}

/**
 ******************************************************************
 * \fn void m32c_movdir_srcr0l(void)
 * Move nibble swapped from src to R0L.
 * v0
 ******************************************************************
 */
static void
m32c_movdir_srcr0l(void)
{
	int Oh = INSTR->Arg1;
	uint32_t Src, Dst;
	int codelen_src = INSTR->codelen_src;
	INSTR->getsrc(&Src, 0);
	Dst = M32C_REG_R0L;
	switch (Oh) {
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
	M32C_REG_R0L = Dst;
	M32C_REG_PC += codelen_src;
	fprintf(stderr, "%s\n", __FUNCTION__);
	dbgprintf("m32c_movdir_srcr0l not tested\n");
}

void
m32c_setup_movdir_srcr0l(void)
{
	int Oh = (ICODE24() >> 4) & 3;
	int size = 1;
	int codelen_src;
	int src = ((ICODE24() >> 6) & 3) | ((ICODE24() >> 7) & 0x1c);
	INSTR->getsrc = general_am_get(src, size, &codelen_src, GAM_ALL);
	INSTR->codelen_src = codelen_src;
	INSTR->Arg1 = Oh;
	switch (Oh) {
	    case 0:		/* LL */
	    case 3:		// HH
		    break;
	    case 1:		// HL
	    case 2:		// LH
		    INSTR->cycles += 3;
		    break;
	}
	INSTR->proc = m32c_movdir_srcr0l;
	INSTR->proc();
}

/**
 ************************************************************************
 * \fn void m32c_movx_immdst(void)
 * Move a sign extended 8 bit immediate to a 32 bit destination.
 * The manual says that this command can not be used with 
 * indexld, but Renesas nc308 compiler generates it with indexld.
 ************************************************************************
 */
static void
m32c_movx_immdst(void)
{
	uint32_t Dst;
	int size = 4;
	int codelen_dst = INSTR->codelen_dst;
	Dst = (int32_t) (int8_t) M32C_Read8(M32C_REG_PC + codelen_dst);
	INSTR->setdst(Dst, M32C_INDEXLD());
	M32C_REG_PC += codelen_dst + 1;
	mov_flags(Dst, size);
}

void
m32c_setup_movx_immdst(void)
{
	int dst;
	int size = 4;
	int codelen_dst;
	dst = ((ICODE16() >> 6) & 3) | ((ICODE16() >> 7) & 0x1c);
	INSTR->setdst = general_am_set(dst, size, &codelen_dst, GAM_ALL);
	INSTR->codelen_dst = codelen_dst;
	INSTR->cycles = 2;
	INSTR->proc = m32c_movx_immdst;
	INSTR->proc();
}

/**
 **************************************************************************
 * \fn void m32c_movx_immidst(void)
 * Move a sign extended 8 bit immediate to an indirect 32 bit destination.
 * Added INDEXLD because nc308 generates index with movx_immdst. 
 * So I suspect it will also work with immidst
 ***************************************************************************
 */
static void
m32c_movx_immidst(void)
{
	uint32_t Dst;
	uint32_t DstP;
	int size = 4;
	int codelen_dst = INSTR->codelen_dst;
	INSTR->getdstp(&DstP, 0);
	M32C_REG_PC += codelen_dst;
	Dst = (int32_t) (int8_t) M32C_Read8(M32C_REG_PC);
	M32C_REG_PC += 1;
	DstP = (DstP + M32C_INDEXLD()) & 0xffffff;
	M32C_Write32(Dst, DstP);
	mov_flags(Dst, size);
	dbgprintf("m32c_movx_immidst not tested\n");
}

void
m32c_setup_movx_immidst(void)
{
	int dst;
	int codelen_dst;
	dst = ((ICODE24() >> 6) & 3) | ((ICODE24() >> 7) & 0x1c);
	INSTR->getdstp = general_am_get(dst, 4, &codelen_dst, GAM_ALL);
	INSTR->codelen_dst = codelen_dst;
	INSTR->cycles = 5;
	INSTR->proc = m32c_movx_immidst;
	INSTR->proc();
}

/**
 ********************************************************************
 * \fn void m32c_mul_size_immdst(void)
 * Signed Multiply an immediate with a destination.
 * Mul has no influence on flags.
 * v1
 ********************************************************************
 */
static void
m32c_mul_size_immdst(void)
{
	uint32_t Src, Dst, Result;
	int size = INSTR->srcsize;
	int codelen_dst = INSTR->codelen_dst;
	INSTR->getdst(&Dst, M32C_INDEXSD());
	if (size == 2) {
		Dst = (int32_t) (int16_t) Dst;
		Src = (int32_t) (int16_t) M32C_Read16(M32C_REG_PC + codelen_dst);
		Result = (int32_t) Dst *(int32_t) Src;
		INSTR->setdst(Result, M32C_INDEXLD());
	} else if (size == 1) {
		Dst = (int32_t) (int8_t) Dst;
		Src = (int32_t) (int8_t) M32C_Read8(M32C_REG_PC + codelen_dst);
		Result = (int32_t) Dst *(int32_t) Src;
		INSTR->setdst(Result, M32C_INDEXWD());
	}
	M32C_REG_PC += codelen_dst + size;
	dbgprintf("m32c_mul_size_immdst not tested\n");
}

void
m32c_setup_mul_size_immdst(void)
{
	int dst;
	int size, resultsize;
	int codelen;
	dst = ((ICODE16() >> 6) & 3) | ((ICODE16() >> 7) & 0x1c);
	if (ICODE16() & 0x100) {
		size = 2;
		resultsize = 4;
	} else {
		size = 1;
		resultsize = 2;
	}
	INSTR->getdst = general_am_get(dst, size, &codelen, GAM_ALL);
	INSTR->setdst = general_am_set(dst, resultsize, &codelen, GAM_ALL);
	INSTR->codelen_dst = codelen;
	INSTR->srcsize = size;
	INSTR->proc = m32c_mul_size_immdst;
	INSTR->proc();
}

/**
 ********************************************************************
 * \fn void m32c_mul_size_immdst(void)
 * Signed Multiply an immediate with an indirect destination.
 * v1
 ********************************************************************
 */
static void
m32c_mul_size_immidst(void)
{
	uint32_t Src, Dst, Result;
	uint32_t DstP;
	int size = INSTR->srcsize;
	int codelen_dst = INSTR->codelen_dst;
	INSTR->getdstp(&DstP, 0);
	DstP = (DstP + M32C_INDEXSD()) & 0xffffff;
	if (size == 2) {
		Src = (int32_t) (int16_t) M32C_Read16(M32C_REG_PC + codelen_dst);
		Dst = (int32_t) (int16_t) M32C_Read16(DstP);
		Result = (int32_t) Dst *(int32_t) Src;
		M32C_Write32(Result, DstP);
	} else if (size == 1) {
		Src = (int32_t) (int8_t) M32C_Read8(M32C_REG_PC + codelen_dst);
		Dst = (int32_t) (int8_t) M32C_Read8(DstP);
		Result = (int32_t) Dst *(int32_t) Src;
		M32C_Write16(Result, DstP);
	}
	M32C_REG_PC += codelen_dst + size;
	dbgprintf("m32c_mul_size_immidst not tested\n");
}

void
m32c_setup_mul_size_immidst(void)
{
	int dst;
	int size;
	int codelen_dst;
	dst = ((ICODE16() >> 6) & 3) | ((ICODE16() >> 7) & 0x1c);
	if (ICODE16() & 0x100) {
		size = 2;
	} else {
		size = 1;
	}
	INSTR->getdstp = general_am_get(dst, 4, &codelen_dst, GAM_ALL);
	INSTR->codelen_dst = codelen_dst;
	INSTR->srcsize = size;
	INSTR->proc = m32c_mul_size_immidst;
	INSTR->proc();
}

/**
 ******************************************************************************
 * \fn void m32c_mul_size_srcdst(void)
 * Signed Multiply a source with a destination.
 * v1
 ******************************************************************************
 */
static void
m32c_mul_size_srcdst(void)
{
	uint32_t Src, Dst, Result;
	int size = INSTR->opsize;
	int codelen_src = INSTR->codelen_src;
	int codelen_dst = INSTR->codelen_dst;
	INSTR->getsrc(&Src, M32C_INDEXSS());
	M32C_REG_PC += codelen_src;
	INSTR->getdst(&Dst, M32C_INDEXSD());
	if (size == 2) {
		Src = (int32_t) (int16_t) Src;
		Dst = (int32_t) (int16_t) Dst;
		Result = (int32_t) Dst *(int32_t) Src;
		INSTR->setdst(Result, M32C_INDEXLD());
	} else {
		Src = (int32_t) (int8_t) Src;
		Dst = (int32_t) (int8_t) Dst;
		Result = (int32_t) Dst *(int32_t) Src;
		INSTR->setdst(Result, M32C_INDEXWD());
	}
	M32C_REG_PC += codelen_dst;
	dbgprintf("m32c_mul_size_srcdst not tested\n");
}

void
m32c_setup_mul_size_srcdst(void)
{
	int dst, src;
	int size, resultsize;
	int codelen_src;
	int codelen_dst;
	dst = ((ICODE16() >> 6) & 3) | ((ICODE16() >> 7) & 0x1c);
	src = ((ICODE16() >> 4) & 3) | ((ICODE16() >> 10) & 0x1c);
	if (ICODE16() & 0x100) {
		size = 2;
		resultsize = 4;
	} else {
		size = 1;
		resultsize = 2;
	}
	INSTR->getdst = general_am_get(dst, size, &codelen_dst, GAM_ALL);
	INSTR->getsrc = general_am_get(src, size, &codelen_src, GAM_ALL);
	INSTR->setdst = general_am_set(dst, resultsize, &codelen_dst, GAM_ALL);
	INSTR->codelen_src = codelen_src;
	INSTR->codelen_dst = codelen_dst;
	INSTR->opsize = size;
	INSTR->proc = m32c_mul_size_srcdst;
	INSTR->proc();
}

/**
 ******************************************************************************
 * \fn void m32c_mul_size_isrcdst(void)
 * Signed Multiply an indirect source with a destination.
 * v1
 ******************************************************************************
 */
static void
m32c_mul_size_isrcdst(void)
{
	uint32_t Src, Dst, Result;
	uint32_t SrcP;
	int size = INSTR->srcsize;
	int codelen_src = INSTR->codelen_src;
	int codelen_dst = INSTR->codelen_dst;
	INSTR->getsrcp(&SrcP, 0);
	SrcP = (SrcP + M32C_INDEXSS()) & 0xffffff;
	M32C_REG_PC += codelen_src;
	INSTR->getdst(&Dst, M32C_INDEXSD());
	if (size == 2) {
		Src = M32C_Read16(SrcP);
		Src = (int32_t) (int16_t) Src;
		Dst = (int32_t) (int16_t) Dst;
		Result = (int32_t) Dst *(int32_t) Src;
		INSTR->setdst(Result, M32C_INDEXLD());
	} else {
		Src = M32C_Read8(SrcP);
		Src = (int32_t) (int8_t) Src;
		Dst = (int32_t) (int8_t) Dst;
		Result = (int32_t) Dst *(int32_t) Src;
		INSTR->setdst(Result, M32C_INDEXWD());
	}
	M32C_REG_PC += codelen_dst;
	dbgprintf("m32c_mul_size_isrcdst not tested\n");
}

void
m32c_setup_mul_size_isrcdst(void)
{
	int dst, src;
	int size, resultsize;
	int codelen_src;
	int codelen_dst;
	dst = ((ICODE24() >> 6) & 3) | ((ICODE24() >> 7) & 0x1c);
	src = ((ICODE24() >> 4) & 3) | ((ICODE24() >> 10) & 0x1c);
	if (ICODE24() & 0x100) {
		size = 2;
		resultsize = 4;
	} else {
		size = 1;
		resultsize = 2;
	}
	INSTR->getdst = general_am_get(dst, size, &codelen_dst, GAM_ALL);
	INSTR->getsrcp = general_am_get(src, 4, &codelen_src, GAM_ALL);
	INSTR->setdst = general_am_set(dst, resultsize, &codelen_dst, GAM_ALL);
	INSTR->codelen_dst = codelen_dst;
	INSTR->codelen_src = codelen_src;
	INSTR->srcsize = size;
	INSTR->proc = m32c_mul_size_isrcdst;
	INSTR->proc();
}

/**
 ******************************************************************************
 * \fn void m32c_mul_size_srcidst(void)
 * Multiply a source with an indirect destination.
 * v1
 ******************************************************************************
 */
static void
m32c_mul_size_srcidst(void)
{
	uint32_t Src, Dst, Result;
	uint32_t DstP;
	int size = INSTR->opsize;
	int codelen_src = INSTR->codelen_src;
	int codelen_dst = INSTR->codelen_dst;
	INSTR->getsrc(&Src, M32C_INDEXSS());
	M32C_REG_PC += codelen_src;
	INSTR->getdstp(&DstP, 0);
	DstP = (DstP + M32C_INDEXSD()) & 0xffffff;
	if (size == 2) {
		Dst = M32C_Read16(DstP);
		Src = (int32_t) (int16_t) Src;
		Dst = (int32_t) (int16_t) Dst;
		Result = (int32_t) Dst *(int32_t) Src;
		M32C_Write32(Result, DstP);
	} else {
		Dst = M32C_Read8(DstP);
		Src = (int32_t) (int8_t) Src;
		Dst = (int32_t) (int8_t) Dst;
		Result = (int32_t) Dst *(int32_t) Src;
		M32C_Write16(Result, DstP);
	}
	M32C_REG_PC += codelen_dst;
	dbgprintf("m32c_mul_size_srcidst not tested\n");
}

void
m32c_setup_mul_size_srcidst(void)
{
	int dst, src;
	int size;
	int codelen_src;
	int codelen_dst;
	dst = ((ICODE24() >> 6) & 3) | ((ICODE24() >> 7) & 0x1c);
	src = ((ICODE24() >> 4) & 3) | ((ICODE24() >> 10) & 0x1c);
	if (ICODE24() & 0x100) {
		size = 2;
	} else {
		size = 1;
	}
	INSTR->getdstp = general_am_get(dst, 4, &codelen_dst, GAM_ALL);
	INSTR->getsrc = general_am_get(src, size, &codelen_src, GAM_ALL);
	INSTR->codelen_src = codelen_src;
	INSTR->codelen_dst = codelen_dst;
	INSTR->opsize = size;
	INSTR->proc = m32c_mul_size_srcidst;
	INSTR->proc();
}

/**
 ******************************************************************************
 * \fn void m32c_mul_size_isrcidst(void)
 * Signed Multiply an indirect source with an indirect destination.
 * v1
 ******************************************************************************
 */
static void
m32c_mul_size_isrcidst(void)
{
	uint32_t Src, Dst, Result;
	uint32_t SrcP, DstP;
	int size = INSTR->opsize;
	int codelen_src = INSTR->codelen_src;
	int codelen_dst = INSTR->codelen_dst;
	INSTR->getsrcp(&SrcP, 0);
	SrcP = (SrcP + M32C_INDEXSD()) & 0xffffff;
	M32C_REG_PC += codelen_src;
	INSTR->getdstp(&DstP, 0);
	DstP = (DstP + M32C_INDEXSD()) & 0xffffff;
	if (size == 2) {
		Src = M32C_Read16(SrcP);
		Dst = M32C_Read16(DstP);
		Src = (int32_t) (int16_t) Src;
		Dst = (int32_t) (int16_t) Dst;
		Result = (int32_t) Dst *(int32_t) Src;
		M32C_Write32(Result, DstP);
	} else {
		Src = M32C_Read8(SrcP);
		Dst = M32C_Read8(DstP);
		Src = (int32_t) (int8_t) Src;
		Dst = (int32_t) (int8_t) Dst;
		Result = (int32_t) Dst *(int32_t) Src;
		M32C_Write16(Result, DstP);
	}
	M32C_REG_PC += codelen_dst;
	dbgprintf("m32c_mul_size_isrcidst not tested\n");
}

void
m32c_setup_mul_size_isrcidst(void)
{
	int dst, src;
	int size;
	int codelen_src;
	int codelen_dst;
	dst = ((ICODE24() >> 6) & 3) | ((ICODE24() >> 7) & 0x1c);
	src = ((ICODE24() >> 4) & 3) | ((ICODE24() >> 10) & 0x1c);
	if (ICODE24() & 0x100) {
		size = 2;
	} else {
		size = 1;
	}
	INSTR->getdstp = general_am_get(dst, 4, &codelen_dst, GAM_ALL);
	INSTR->getsrcp = general_am_get(src, 4, &codelen_src, GAM_ALL);
	INSTR->codelen_dst = codelen_dst;
	INSTR->codelen_src = codelen_src;
	INSTR->opsize = size;
	INSTR->proc = m32c_mul_size_isrcidst;
	INSTR->proc();
}

/**
 *********************************************************************
 * \fn void m32c_mul_l_srcr2r0(void)
 * Signed Multiply a source with R2R0.
 * v1
 *********************************************************************
 */
static void
m32c_mul_l_srcr2r0(void)
{
	uint32_t Src, Dst, Result;
	int codelen_src = INSTR->codelen_src;
	INSTR->getsrc(&Src, M32C_INDEXLS());
	M32C_REG_PC += codelen_src;
	Dst = M32C_REG_R0 | (M32C_REG_R2 << 16);
	Result = (int32_t) Dst *(int32_t) Src;
	M32C_REG_R0 = Result & 0xffff;
	M32C_REG_R2 = Result >> 16;
	dbgprintf("m32c_mul_l_srcr2r0 not tested\n");
}

void
m32c_setup_mul_l_srcr2r0(void)
{
	int src;
	int size = 4;
	int codelen_src;
	src = ((ICODE24() >> 6) & 3) | ((ICODE24() >> 7) & 0x1c);
	INSTR->getsrc = general_am_get(src, size, &codelen_src, GAM_ALL);
	INSTR->codelen_src = codelen_src;
	if (INSTR->nrMemAcc > 0) {
		INSTR->cycles = 9;
	}
	INSTR->proc = m32c_mul_l_srcr2r0;
	INSTR->proc();
}

/**
 *********************************************************************
 * \fn void m32c_mulex_src(void)
 * Signed multiply of a 16 Bit Source with R2R0 stored in R1R2R0. 
 * v1
 *********************************************************************
 */
static void
m32c_mulex_src(void)
{
	uint32_t Src, Dst;
	uint64_t Result;
	int codelen_src = INSTR->codelen_src;
	INSTR->getsrc(&Src, 0);
	M32C_REG_PC += codelen_src;
	Dst = M32C_REG_R0 | (M32C_REG_R2 << 16);
	Result = (int64_t) (int32_t) Dst *(int64_t) (int16_t) Src;
	M32C_REG_R0 = Result & 0xffff;
	M32C_REG_R2 = (Result >> 16) & 0xffff;
	M32C_REG_R1 = (Result >> 32) & 0xffff;
	dbgprintf("m32c_mulex_src not tested\n");
}

void
m32c_setup_mulex_src(void)
{
	int src;
	int size = 2;
	int codelen_src;
	src = ((ICODE16() >> 6) & 3) | ((ICODE16() >> 7) & 0x1c);
	INSTR->getsrc = general_am_get(src, size, &codelen_src, GAM_ALL);
	INSTR->codelen_src = codelen_src;
	INSTR->proc = m32c_mulex_src;
	INSTR->proc();
}

/**
 *********************************************************************
 * \fn void m32c_mulex_src(void)
 * Signed multiply of an indirect 16 Bit Source 
 * with R2R0 stored in R1R2R0. 
 * v1
 *********************************************************************
 */
static void
m32c_mulex_isrc(void)
{
	uint32_t Src, Dst;
	uint32_t SrcP;
	uint64_t Result;
	int codelen_src = INSTR->codelen_src;
	INSTR->getsrcp(&SrcP, 0);
	Src = M32C_Read16(SrcP);
	M32C_REG_PC += codelen_src;
	Dst = M32C_REG_R0 | (M32C_REG_R2 << 16);
	Result = (int64_t) (int32_t) Dst *(int64_t) (int16_t) Src;
	M32C_REG_R0 = Result & 0xffff;
	M32C_REG_R2 = (Result >> 16) & 0xffff;
	M32C_REG_R1 = (Result >> 32) & 0xffff;
	dbgprintf("m32c_mulex_isrc not tested\n");
}

void
m32c_setup_mulex_isrc(void)
{
	int src;
	int codelen_src;
	src = ((ICODE24() >> 6) & 3) | ((ICODE24() >> 7) & 0x1c);
	INSTR->getsrcp = general_am_get(src, 4, &codelen_src, GAM_ALL);
	INSTR->codelen_src = codelen_src;
	INSTR->proc = m32c_mulex_isrc;
	INSTR->proc();
}

/**
 ******************************************************************
 * \fn void m32c_mulu_size_immdst(void)
 * Unsigned multiplication of an immediate with a destination.
 * v1
 ******************************************************************
 */
static void
m32c_mulu_size_immdst(void)
{
	uint32_t Src, Dst, Result;
	int size = INSTR->srcsize;
	int codelen_dst = INSTR->codelen_dst;
	INSTR->getdst(&Dst, M32C_INDEXSD());
	if (size == 2) {
		Src = M32C_Read16(M32C_REG_PC + codelen_dst);
		Result = Dst * Src;
		INSTR->setdst(Result, M32C_INDEXLD());
	} else if (size == 1) {
		Src = M32C_Read8(M32C_REG_PC + codelen_dst);
		Result = Dst * Src;
		INSTR->setdst(Result, M32C_INDEXWD());
	}
	M32C_REG_PC += codelen_dst + size;
	dbgprintf("m32c_mulu_size_immidst not implemented\n");
}

void
m32c_setup_mulu_size_immdst(void)
{
	int dst;
	int size, resultsize;
	int codelen_dst;
	dst = ((ICODE16() >> 6) & 3) | ((ICODE16() >> 7) & 0x1c);
	if (ICODE16() & 0x100) {
		size = 2;
		resultsize = 4;
	} else {
		size = 1;
		resultsize = 2;
	}
	INSTR->getdst = general_am_get(dst, size, &codelen_dst, GAM_ALL);
	INSTR->setdst = general_am_set(dst, resultsize, &codelen_dst, GAM_ALL);
	INSTR->codelen_dst = codelen_dst;
	INSTR->srcsize = size;
	INSTR->proc = m32c_mulu_size_immdst;
	INSTR->proc();
}

/**
 ******************************************************************
 * \fn void m32c_mulu_size_immdst(void)
 * Unsigned multiplication of an immediate with an indirect
 * destination.
 * v0
 ******************************************************************
 */
static void
m32c_mulu_size_immidst(void)
{
	uint32_t Src, Dst, Result;
	uint32_t DstP;
	int size = INSTR->opsize;
	int codelen_dst = INSTR->codelen_dst;
	INSTR->getdstp(&DstP, 0);
	if (size == 2) {
		DstP = (DstP + M32C_INDEXLD()) & 0xffffff;
		Dst = M32C_Read16(DstP);
		Src = M32C_Read16(M32C_REG_PC + codelen_dst);
		Result = Dst * Src;
		M32C_Write32(Result, DstP);
	} else if (size == 1) {
		DstP = (DstP + M32C_INDEXWD()) & 0xffffff;
		Dst = M32C_Read8(DstP);
		Src = M32C_Read8(M32C_REG_PC + codelen_dst);
		Result = Dst * Src;
		M32C_Write16(Result, DstP);
	}
	M32C_REG_PC += codelen_dst + size;
	dbgprintf("m32c_mulu_size_immidst not tested\n");
}

void
m32c_setup_mulu_size_immidst(void)
{
	int dst;
	int size;
	int codelen_dst;
	dst = ((ICODE24() >> 6) & 3) | ((ICODE24() >> 7) & 0x1c);
	if (ICODE24() & 0x100) {
		size = 2;
	} else {
		size = 1;
	}
	INSTR->getdstp = general_am_get(dst, size, &codelen_dst, GAM_ALL);
	INSTR->codelen_dst = codelen_dst;
	INSTR->opsize = size;
	INSTR->proc = m32c_mulu_size_immidst;
	INSTR->proc();
}

/**
 ************************************************************
 * \fn void m32c_mulu_size_srcdst(void)
 * Unsigned multiplication of src and destination.
 * v1
 ************************************************************
 */
static void
m32c_mulu_size_srcdst(void)
{
	uint32_t Src, Dst, Result;
	int codelen_src = INSTR->codelen_src;
	int codelen_dst = INSTR->codelen_dst;
	INSTR->getsrc(&Src, M32C_INDEXSS());
	M32C_REG_PC += codelen_src;
	INSTR->getdst(&Dst, M32C_INDEXSD());
	Result = Dst * Src;
	INSTR->setdst(Result, M32C_INDEXSD());
	M32C_REG_PC += codelen_dst;
}

void
m32c_setup_mulu_size_srcdst(void)
{
	int dst, src;
	int size, resultsize;
	int codelen_src;
	int codelen_dst;
	dst = ((ICODE16() >> 6) & 3) | ((ICODE16() >> 7) & 0x1c);
	src = ((ICODE16() >> 4) & 3) | ((ICODE16() >> 10) & 0x1c);
	if (ICODE16() & 0x100) {
		size = 2;
		resultsize = 4;
	} else {
		size = 1;
		resultsize = 2;
	}
	INSTR->getdst = general_am_get(dst, size, &codelen_dst, GAM_ALL);
	INSTR->getsrc = general_am_get(src, size, &codelen_src, GAM_ALL);
	INSTR->setdst = general_am_set(dst, resultsize, &codelen_dst, GAM_ALL);
	INSTR->codelen_src = codelen_src;
	INSTR->codelen_dst = codelen_dst;
	INSTR->srcsize = size;
	INSTR->proc = m32c_mulu_size_srcdst;
	INSTR->proc();
}

/**
 ************************************************************
 * \fn void m32c_mulu_size_isrcdst(void)
 * Unsigned multiplication of an indirect src and destination.
 * v1
 ************************************************************
 */
static void
m32c_mulu_size_isrcdst(void)
{
	uint32_t Src, Dst, Result;
	uint32_t SrcP;
	int size = INSTR->codelen_src;
	int codelen_src = INSTR->codelen_src;;
	int codelen_dst = INSTR->codelen_dst;
	INSTR->getsrcp(&SrcP, 0);
	SrcP = (SrcP + M32C_INDEXSS()) & 0xffffff;
	M32C_REG_PC += codelen_src;
	INSTR->getdst(&Dst, M32C_INDEXSD());
	if (size == 2) {
		Src = M32C_Read16(SrcP);
	} else {
		Src = M32C_Read8(SrcP);
	}
	Result = Dst * Src;
	INSTR->setdst(Result, M32C_INDEXSD());
	M32C_REG_PC += codelen_dst;
	dbgprintf("m32c_mulu_size_isrcdst not tested\n");
}

void
m32c_setup_mulu_size_isrcdst(void)
{
	int dst, src;
	int size, resultsize;
	int codelen_src;
	int codelen_dst;
	dst = ((ICODE24() >> 6) & 3) | ((ICODE24() >> 7) & 0x1c);
	src = ((ICODE24() >> 4) & 3) | ((ICODE24() >> 10) & 0x1c);
	if (ICODE24() & 0x100) {
		size = 2;
		resultsize = 4;
	} else {
		size = 1;
		resultsize = 2;
	}
	INSTR->getdst = general_am_get(dst, size, &codelen_dst, GAM_ALL);
	INSTR->getsrcp = general_am_get(src, 4, &codelen_src, GAM_ALL);
	INSTR->setdst = general_am_set(dst, resultsize, &codelen_dst, GAM_ALL);
	INSTR->codelen_src = codelen_src;
	INSTR->codelen_dst = codelen_dst;
	INSTR->srcsize = size;
	INSTR->proc = m32c_mulu_size_isrcdst;
	INSTR->proc();
}

/**
 ******************************************************************
 * \fn void m32c_mulu_size_srcidst(void)
 * Unsigned multiplication of a src and an indirect destination.
 * v1
 ******************************************************************
 */
static void
m32c_mulu_size_srcidst(void)
{
	uint32_t Src, Dst, Result;
	uint32_t DstP;
	int size = INSTR->opsize;
	int codelen_src = INSTR->codelen_src;
	int codelen_dst = INSTR->codelen_dst;;
	INSTR->getsrc(&Src, M32C_INDEXSS());
	M32C_REG_PC += codelen_src;
	INSTR->getdstp(&DstP, 0);
	DstP = (DstP + M32C_INDEXSD()) & 0xffffff;
	if (size == 2) {
		Dst = M32C_Read16(DstP);
		Result = Dst * Src;
		M32C_Write32(Result, DstP);
	} else {
		Dst = M32C_Read8(DstP);
		Result = Dst * Src;
		M32C_Write16(Result, DstP);
	}
	M32C_REG_PC += codelen_dst;
	dbgprintf("m32c_mulu_size_srcidst not implemented\n");
}

void
m32c_setup_mulu_size_srcidst(void)
{
	int dst, src;
	int size;
	int codelen_src;
	int codelen_dst;
	dst = ((ICODE24() >> 6) & 3) | ((ICODE24() >> 7) & 0x1c);
	src = ((ICODE24() >> 4) & 3) | ((ICODE24() >> 10) & 0x1c);
	if (ICODE24() & 0x100) {
		size = 2;
	} else {
		size = 1;
	}
	INSTR->getdstp = general_am_get(dst, 4, &codelen_dst, GAM_ALL);
	INSTR->getsrc = general_am_get(src, size, &codelen_src, GAM_ALL);
	INSTR->codelen_src = codelen_src;
	INSTR->codelen_dst = codelen_dst;
	INSTR->opsize = size;

	INSTR->proc = m32c_mulu_size_srcidst;
	INSTR->proc();
}

/**
 ******************************************************************
 * \fn void m32c_mulu_size_srcidst(void)
 * Unsigned multiplication of an indirect src and an 
 * indirect destination.
 * v0
 ******************************************************************
 */
static void
m32c_mulu_size_isrcidst(void)
{
	uint32_t Src, Dst, Result;
	uint32_t SrcP, DstP;
	int size = INSTR->opsize;
	int codelen_src = INSTR->codelen_src;
	int codelen_dst = INSTR->codelen_dst;
	INSTR->getsrcp(&SrcP, 0);
	SrcP = (SrcP + M32C_INDEXSS()) & 0xffffff;
	M32C_REG_PC += codelen_src;
	INSTR->getdstp(&DstP, 0);
	DstP = (DstP + M32C_INDEXSD()) & 0xffffff;
	if (size == 2) {
		Src = M32C_Read16(SrcP);
		Dst = M32C_Read16(DstP);
		Result = Dst * Src;
		M32C_Write32(Result, DstP);
	} else {
		Src = M32C_Read8(SrcP);
		Dst = M32C_Read8(DstP);
		Result = Dst * Src;
		M32C_Write16(Result, DstP);
	}
	M32C_REG_PC += codelen_dst;
	dbgprintf("m32c_mulu_size_isrcidst not tested\n");
}

void
m32c_setup_mulu_size_isrcidst(void)
{
	int dst, src;
	int size;
	int codelen_src;
	int codelen_dst;
	dst = ((ICODE24() >> 6) & 3) | ((ICODE24() >> 7) & 0x1c);
	src = ((ICODE24() >> 4) & 3) | ((ICODE24() >> 10) & 0x1c);
	if (ICODE24() & 0x100) {
		size = 2;
	} else {
		size = 1;
	}
	INSTR->getdstp = general_am_get(dst, 4, &codelen_dst, GAM_ALL);
	INSTR->getsrcp = general_am_get(src, size, &codelen_src, GAM_ALL);
	INSTR->codelen_dst = codelen_dst;
	INSTR->codelen_src = codelen_src;
	INSTR->opsize = size;
	INSTR->proc = m32c_mulu_size_isrcidst;
	INSTR->proc();
}

/**
 ******************************************************************
 * \fn void m32c_mulu_l_srcr2r0(void)
 * Unsigned long multiplication of a src and an R2R0.
 * v1
 ******************************************************************
 */
static void
m32c_mulu_l_srcr2r0(void)
{
	uint32_t Src, Dst, Result;
	int codelen_src = INSTR->codelen_src;
	INSTR->getsrc(&Src, M32C_INDEXSS());
	M32C_REG_PC += codelen_src;
	Dst = M32C_REG_R0 | ((uint32_t) M32C_REG_R2 << 16);
	Result = Dst * Src;
	M32C_REG_R0 = Result & 0xffff;
	M32C_REG_R2 = Result >> 16;
	dbgprintf("m32c_mulu_l_srcr2r0 not tested\n");
}

void
m32c_setup_mulu_l_srcr2r0(void)
{
	int src;
	int size = 4;
	int codelen_src;
	src = ((ICODE24() >> 6) & 3) | ((ICODE24() >> 7) & 0x1c);
	INSTR->getsrc = general_am_get(src, size, &codelen_src, GAM_ALL);
	INSTR->codelen_src = codelen_src;
	if (INSTR->nrMemAcc > 0) {
		INSTR->cycles = 9;
	}
	INSTR->proc = m32c_mulu_l_srcr2r0;
	INSTR->proc();
}

/**
 ***************************************************
 * \fn void m32c_neg_size_dst(void)
 * Calculate the two's complement.
 * v0
 ***************************************************
 */
static void
m32c_neg_size_dst(void)
{
	int size = INSTR->opsize;
	int codelen_dst = INSTR->codelen_dst;
	uint32_t Dst, Result;
	INSTR->getdst(&Dst, M32C_INDEXSD());
	Result = 0 - Dst;
	INSTR->setdst(Result, M32C_INDEXSD());
	sub_flags(0, Dst, Result, size);
	M32C_REG_PC += codelen_dst;
	dbgprintf("m32c_neg_size_dst not tested\n");
}

void
m32c_setup_neg_size_dst(void)
{
	int dst;
	int size;
	int codelen_dst;
	if (ICODE16() & 0x100) {
		size = 2;
	} else {
		size = 1;
	}
	dst = ((ICODE16() >> 6) & 3) | ((ICODE16() >> 7) & 0x1c);
	INSTR->getdst = general_am_get(dst, size, &codelen_dst, GAM_ALL);
	INSTR->setdst = general_am_set(dst, size, &codelen_dst, GAM_ALL);
	INSTR->codelen_dst = codelen_dst;
	INSTR->opsize = size;
	INSTR->proc = m32c_neg_size_dst;
	INSTR->proc();
}

/**
 ***************************************************
 * \fn void m32c_neg_size_dst(void)
 * Calculate the two's complement of an indirect
 * destination.
 * v0
 ***************************************************
 */
static void
m32c_neg_size_idst(void)
{
	int size = INSTR->opsize;
	int codelen_dst = INSTR->codelen_dst;
	uint32_t Dst, Result;
	uint32_t DstP;
	INSTR->getdstp(&DstP, 0);
	DstP = (DstP + M32C_INDEXSD()) & 0xffffff;
	if (size == 2) {
		Dst = M32C_Read16(DstP);
		Result = 0 - Dst;
		M32C_Write16(Result, DstP);
	} else {
		Dst = M32C_Read8(DstP);
		Result = 0 - Dst;
		M32C_Write8(Result, DstP);
	}
	sub_flags(0, Dst, Result, size);
	M32C_REG_PC += codelen_dst;
	dbgprintf("m32c_neg_size_idst not tested\n");
}

void
m32c_setup_neg_size_idst(void)
{
	int dst;
	int size;
	int codelen_dst;
	if (ICODE24() & 0x100) {
		size = 2;
	} else {
		size = 1;
	}
	dst = ((ICODE24() >> 6) & 3) | ((ICODE24() >> 7) & 0x1c);
	INSTR->getdstp = general_am_get(dst, 4, &codelen_dst, GAM_ALL);
	INSTR->codelen_dst = codelen_dst;
	INSTR->opsize = size;
	INSTR->proc = m32c_neg_size_idst;
	INSTR->proc();
}

/**
 **************************************************************
 * \fn void m32c_nop(void)
 * Do nothing.
 * v1
 **************************************************************
 */
void
m32c_nop(void)
{
	dbgprintf("m32c_nop\n");
}

/**
 **************************************************************
 * \fn void m32c_not_size_dst(void)
 * Invert a destination.
 * v0
 **************************************************************
 */
static void
m32c_not_size_dst(void)
{
	int size = INSTR->opsize;
	int codelen_dst = INSTR->codelen_dst;
	uint32_t Dst, Result;
	INSTR->getdst(&Dst, M32C_INDEXSD());
	Result = ~Dst;
	INSTR->setdst(Result, M32C_INDEXSD());
	not_flags(Result, size);
	M32C_REG_PC += codelen_dst;
}

void
m32c_setup_not_size_dst(void)
{
	int dst;
	int size;
	int codelen_dst;
	if (ICODE16() & 0x100) {
		size = 2;
	} else {
		size = 1;
	}
	dst = ((ICODE16() >> 6) & 3) | ((ICODE16() >> 7) & 0x1c);
	INSTR->getdst = general_am_get(dst, size, &codelen_dst, GAM_ALL);
	INSTR->setdst = general_am_set(dst, size, &codelen_dst, GAM_ALL);
	INSTR->codelen_dst = codelen_dst;
	INSTR->opsize = size;
	INSTR->proc = m32c_not_size_dst;
	INSTR->proc();
}

/**
 **************************************************************
 * \fn void m32c_not_size_idst(void)
 * Invert an indirect destination.
 * v0
 **************************************************************
 */
static void
m32c_not_size_idst(void)
{
	int size = INSTR->opsize;
	int codelen_dst = INSTR->codelen_dst;;
	uint32_t Dst, Result;
	uint32_t DstP;
	INSTR->getdstp(&DstP, 0);
	DstP = (DstP + M32C_INDEXSD()) & 0xffffff;
	if (size == 2) {
		Dst = M32C_Read16(DstP);
		Result = ~Dst;
		M32C_Write16(Result, DstP);
	} else {
		Dst = M32C_Read8(DstP);
		Result = ~Dst;
		M32C_Write8(Result, DstP);
	}
	not_flags(Result, size);
	M32C_REG_PC += codelen_dst;
	dbgprintf("m32c_not_size_idst not tested\n");
}

void
m32c_setup_not_size_idst(void)
{
	int dst;
	int size;
	int codelen_dst;
	if (ICODE24() & 0x100) {
		size = 2;
	} else {
		size = 1;
	}
	dst = ((ICODE24() >> 6) & 3) | ((ICODE24() >> 7) & 0x1c);
	INSTR->getdstp = general_am_get(dst, 4, &codelen_dst, GAM_ALL);
	INSTR->codelen_dst = codelen_dst;
	INSTR->opsize = size;
	INSTR->proc = m32c_not_size_idst;
	INSTR->proc();
}

/**
 **************************************************************
 * \fn void m32c_or_size_g_immdst(void)
 * Logical "OR" of an immediate and a destination.
 * v0
 **************************************************************
 */
static void
m32c_or_size_g_immdst(void)
{
	uint32_t Src, Dst, Result;
	int opsize = INSTR->opsize;
	int srcsize = INSTR->srcsize;
	int codelen = INSTR->codelen_dst;
	INSTR->getdst(&Dst, M32C_INDEXSD());
	if (srcsize == 2) {
		Src = M32C_Read16((M32C_REG_PC + codelen) & 0xffffff);
	} else {		/* if(srcsize == 1) */

		Src = M32C_Read8((M32C_REG_PC + codelen) & 0xffffff);
	}
	Result = Dst | Src;
	INSTR->setdst(Result, 0);
	or_flags(Result, opsize);
	M32C_REG_PC += codelen + srcsize;
}

void
m32c_setup_or_size_g_immdst(void)
{
	int dst;
	int opsize, srcsize;
	int codelen;
	dst = ((ICODE16() >> 6) & 3) | ((ICODE16() >> 7) & 0x1c);
	if (ICODE16() & 0x100) {
		srcsize = opsize = 2;
	} else {
		srcsize = opsize = 1;
		ModOpsize(dst, &opsize);
	}
	INSTR->getdst = general_am_get(dst, opsize, &codelen, GAM_ALL);
	INSTR->setdst = general_am_set(dst, opsize, &codelen, GAM_ALL);
	INSTR->codelen_dst = codelen;
	INSTR->opsize = opsize;
	INSTR->srcsize = srcsize;
	INSTR->proc = m32c_or_size_g_immdst;
	INSTR->proc();
}

/**
 ***************************************************************
 * \fn void m32c_or_size_g_immidst(void)
 * Logical "OR" of an immediate with an indirect destination.
 * v0
 ***************************************************************
 */
static void
m32c_or_size_g_immidst(void)
{
	uint32_t Src, Dst, Result;
	uint32_t DstP;
	int size = INSTR->opsize;
	int codelen_dst = INSTR->codelen_dst;
	INSTR->getdstp(&DstP, 0);
	DstP = (DstP + M32C_INDEXSD());
	if (size == 2) {
		Dst = M32C_Read16(DstP);
		Src = M32C_Read16((M32C_REG_PC + codelen_dst) & 0xffffff);
	} else {		/* if(size == 1) */

		Dst = M32C_Read8(DstP);
		Src = M32C_Read8((M32C_REG_PC + codelen_dst) & 0xffffff);
	}
	Result = Dst | Src;
	if (size == 2) {
		M32C_Write16(Result, DstP);
	} else {
		M32C_Write8(Result, DstP);
	}
	or_flags(Result, size);
	M32C_REG_PC += codelen_dst + size;
	dbgprintf("m32c_or_size_immidst not tested\n");
}

void
m32c_setup_or_size_g_immidst(void)
{
	int dst;
	int size;
	int codelen;
	dst = ((ICODE24() >> 6) & 3) | ((ICODE24() >> 7) & 0x1c);
	if (ICODE24() & 0x100) {
		size = 2;
	} else {
		size = 1;
	}
	INSTR->getdstp = general_am_get(dst, 4, &codelen, GAM_ALL);
	INSTR->codelen_dst = codelen;
	INSTR->opsize = size;
	INSTR->proc = m32c_or_size_g_immidst;
	INSTR->proc();
}

/**
 *************************************************************
 * \fn void m32c_or_size_s_immdst(void)
 * Logical OR of an immediate with a destination.
 * v0
 *************************************************************
 */
static void
m32c_or_size_s_immdst(void)
{
	uint32_t imm;
	uint32_t Dst;
	uint32_t Result;
	int size = INSTR->opsize;
	int codelen_dst = INSTR->codelen_dst;
	INSTR->getam2bit(&Dst, M32C_INDEXSD());
	if (size == 2) {
		imm = M32C_Read16((M32C_REG_PC + codelen_dst) & 0xffffff);
	} else {
		imm = M32C_Read8((M32C_REG_PC + codelen_dst) & 0xffffff);
	}
	Result = Dst | imm;
	INSTR->setam2bit(Result, M32C_INDEXSD());
	or_flags(Result, size);
	M32C_REG_PC += codelen_dst + size;
}

void
m32c_setup_or_size_s_immdst(void)
{
	int size;
	int codelen_dst;
	int dst = (ICODE8() >> 4) & 3;
	if (ICODE8() & 1) {
		size = 2;
	} else {
		size = 1;
	}
	INSTR->getam2bit = am2bit_getproc(dst, &codelen_dst, size);
	INSTR->setam2bit = am2bit_setproc(dst, &codelen_dst, size);
	INSTR->codelen_dst = codelen_dst;
	INSTR->opsize = size;
	INSTR->proc = m32c_or_size_s_immdst;
	INSTR->proc();
}

/**
 *************************************************************
 * \fn void m32c_or_size_s_immidst(void)
 * Logical OR of an immediate with an indirect destination.
 * v0
 *************************************************************
 */
static void
m32c_or_size_s_immidst(void)
{
	int size = INSTR->opsize;
	uint32_t imm;
	uint32_t Dst;
	uint32_t DstP;
	uint32_t Result;
	int codelen_dst = INSTR->codelen_dst;
	INSTR->getam2bit(&DstP, 0);
	DstP = (DstP + M32C_INDEXSD());
	if (size == 2) {
		Dst = M32C_Read16(DstP);
		imm = M32C_Read16((M32C_REG_PC + codelen_dst) & 0xffffff);
	} else {
		Dst = M32C_Read8(DstP);
		imm = M32C_Read8((M32C_REG_PC + codelen_dst) & 0xffffff);
	}
	Result = Dst | imm;
	if (size == 2) {
		M32C_Write16(Result, DstP);
	} else {
		M32C_Write8(Result, DstP);
	}
	or_flags(Result, size);
	M32C_REG_PC += codelen_dst + size;
}

void
m32c_setup_or_size_s_immidst(void)
{
	int size;
	int codelen_dst;
	int am = (ICODE16() >> 4) & 3;
	if (ICODE16() & 1) {
		size = 2;
	} else {
		size = 1;
	}
	INSTR->getam2bit = am2bit_getproc(am, &codelen_dst, 4);
	INSTR->codelen_dst = codelen_dst;
	INSTR->opsize = size;
	INSTR->proc = m32c_or_size_s_immidst;
	INSTR->proc();
}

/**
 *************************************************************
 * \fn void m32c_or_size_srcdst(void)
 * Logical OR of a source and a destination.
 * v0
 *************************************************************
 */
static void
m32c_or_size_srcdst(void)
{
	uint32_t Src, Dst, Result;
	int opsize = INSTR->opsize;
	int codelen_src = INSTR->codelen_src;
	int codelen_dst = INSTR->codelen_dst;
	INSTR->getsrc(&Src, M32C_INDEXSS());
	M32C_REG_PC += codelen_src;
	INSTR->getdst(&Dst, M32C_INDEXSD());
	Result = Dst | Src;
	INSTR->setdst(Result, M32C_INDEXSD());
	or_flags(Result, opsize);
	M32C_REG_PC += codelen_dst;
}

void
m32c_setup_or_size_srcdst(void)
{
	int dst, src;
	int srcsize, opsize;
	int codelen_src;
	int codelen_dst;
	dst = ((ICODE16() >> 6) & 3) | ((ICODE16() >> 7) & 0x1c);
	src = ((ICODE16() >> 4) & 3) | ((ICODE16() >> 10) & 0x1c);
	if (ICODE16() & 0x100) {
		srcsize = opsize = 2;
	} else {
		srcsize = opsize = 1;
		ModOpsize(dst, &opsize);
	}
	INSTR->getdst = general_am_get(dst, opsize, &codelen_dst, GAM_ALL);
	INSTR->getsrc = general_am_get(src, srcsize, &codelen_src, GAM_ALL);
	INSTR->setdst = general_am_set(dst, opsize, &codelen_dst, GAM_ALL);
	INSTR->codelen_dst = codelen_dst;
	INSTR->codelen_src = codelen_src;
	INSTR->opsize = opsize;
	INSTR->srcsize = srcsize;
	INSTR->proc = m32c_or_size_srcdst;
	INSTR->proc();
}

/**
 *************************************************************
 * \fn void m32c_or_size_isrcdst(void)
 * Logical OR of an indirect source and a destination.
 * v0
 *************************************************************
 */
static void
m32c_or_size_isrcdst(void)
{
	uint32_t SrcP;
	uint32_t Src, Dst, Result;
	int opsize = INSTR->opsize;
	int srcsize = INSTR->srcsize;
	int codelen_src = INSTR->codelen_src;
	int codelen_dst = INSTR->codelen_dst;
	INSTR->getsrcp(&SrcP, 0);
	SrcP = (SrcP + M32C_INDEXSS()) & 0xffffff;
	M32C_REG_PC += codelen_src;
	INSTR->getdst(&Dst, M32C_INDEXSD());
	if (srcsize == 2) {
		Src = M32C_Read16(SrcP);
	} else {
		Src = M32C_Read8(SrcP);
	}
	Result = Dst | Src;
	INSTR->setdst(Result, M32C_INDEXSD());
	or_flags(Result, opsize);
	M32C_REG_PC += codelen_dst;
	dbgprintf("m32c_or_size_isrcdst not tested\n");
}

void
m32c_setup_or_size_isrcdst(void)
{
	int dst, src;
	int opsize, srcsize;
	int codelen_src;
	int codelen_dst;
	dst = ((ICODE24() >> 6) & 3) | ((ICODE24() >> 7) & 0x1c);
	src = ((ICODE24() >> 4) & 3) | ((ICODE24() >> 10) & 0x1c);
	if (ICODE24() & 0x100) {
		srcsize = opsize = 2;
	} else {
		srcsize = opsize = 1;
		ModOpsize(dst, &opsize);
	}
	INSTR->getdst = general_am_get(dst, opsize, &codelen_dst, GAM_ALL);
	INSTR->getsrcp = general_am_get(src, 4, &codelen_src, GAM_ALL);
	INSTR->setdst = general_am_set(dst, opsize, &codelen_dst, GAM_ALL);
	INSTR->codelen_src = codelen_src;
	INSTR->codelen_dst = codelen_dst;
	INSTR->opsize = opsize;
	INSTR->srcsize = srcsize;

	INSTR->proc = m32c_or_size_isrcdst;
	INSTR->proc();
}

/**
 *************************************************************
 * \fn void m32c_or_size_srcidst(void)
 * Logical OR of a source and an indirect destination.
 * v0
 *************************************************************
 */
static void
m32c_or_size_srcidst(void)
{
	uint32_t Src, Dst, Result;
	uint32_t DstP;
	int size = INSTR->opsize;
	int codelen_src = INSTR->codelen_src;
	int codelen_dst = INSTR->codelen_dst;
	INSTR->getsrc(&Src, M32C_INDEXSS());
	M32C_REG_PC += codelen_src;
	INSTR->getdstp(&DstP, 0);
	DstP = (DstP + M32C_INDEXSD()) & 0xffffff;
	if (size == 2) {
		Dst = M32C_Read16(DstP);
	} else {
		Dst = M32C_Read8(DstP);
	}
	Result = Dst | Src;
	if (size == 2) {
		M32C_Write16(Result, DstP);
	} else {
		M32C_Write8(Result, DstP);
	}
	or_flags(Result, size);
	M32C_REG_PC += codelen_dst;
	dbgprintf("m32c_or_size_srcidst not tested\n");
}

void
m32c_setup_or_size_srcidst(void)
{
	int dst, src;
	int size;
	int codelen_src;
	int codelen_dst;
	dst = ((ICODE24() >> 6) & 3) | ((ICODE24() >> 7) & 0x1c);
	src = ((ICODE24() >> 4) & 3) | ((ICODE24() >> 10) & 0x1c);
	if (ICODE24() & 0x100) {
		size = 2;
	} else {
		size = 1;
	}
	INSTR->getdstp = general_am_get(dst, 4, &codelen_dst, GAM_ALL);
	INSTR->getsrc = general_am_get(src, size, &codelen_src, GAM_ALL);
	INSTR->codelen_src = codelen_src;
	INSTR->codelen_dst = codelen_dst;
	INSTR->opsize = size;

	INSTR->proc = m32c_or_size_srcidst;
	INSTR->proc();
}

/**
 *****************************************************************
 * \fn void m32c_or_size_isrcidst(void)
 * Logical OR of an indirect source and an indirect destination.
 * v0
 *****************************************************************
 */
static void
m32c_or_size_isrcidst(void)
{
	uint32_t Src, Dst, Result;
	uint32_t DstP, SrcP;
	int size = INSTR->opsize;
	int codelen_src = INSTR->codelen_src;
	int codelen_dst = INSTR->codelen_dst;
	INSTR->getsrcp(&SrcP, 0);
	SrcP = (SrcP + M32C_INDEXSS()) & 0xffffff;
	M32C_REG_PC += codelen_src;
	INSTR->getdstp(&DstP, 0);
	DstP = (DstP + M32C_INDEXSD()) & 0xffffff;
	if (size == 2) {
		Dst = M32C_Read16(DstP);
		Src = M32C_Read16(SrcP);
	} else {
		Dst = M32C_Read8(DstP);
		Src = M32C_Read8(SrcP);
	}
	Result = Dst | Src;
	if (size == 2) {
		M32C_Write16(Result, DstP);
	} else {
		M32C_Write8(Result, DstP);
	}
	or_flags(Result, size);
	M32C_REG_PC += codelen_dst;
	dbgprintf("m32c_or_size_isrcidst not tested\n");
}

void
m32c_setup_or_size_isrcidst(void)
{
	int dst, src;
	int size;
	int codelen_src;
	int codelen_dst;
	dst = ((ICODE24() >> 6) & 3) | ((ICODE24() >> 7) & 0x1c);
	src = ((ICODE24() >> 4) & 3) | ((ICODE24() >> 10) & 0x1c);
	if (ICODE24() & 0x100) {
		size = 2;
	} else {
		size = 1;
	}
	INSTR->getdstp = general_am_get(dst, 4, &codelen_dst, GAM_ALL);
	INSTR->getsrcp = general_am_get(src, 4, &codelen_src, GAM_ALL);
	INSTR->codelen_dst = codelen_dst;
	INSTR->codelen_src = codelen_src;
	INSTR->opsize = size;

	INSTR->proc = m32c_or_size_isrcidst;
	INSTR->proc();
}

/*
 *******************************************************
 * \fn void m32c_pop_size_dst(void)
 * Pop something from stack into a destination.
 *******************************************************
 */
static void
m32c_pop_b_dst(void)
{
	int codelen_dst = INSTR->codelen_dst;
	uint32_t Dst;
	Dst = M32C_Read8(M32C_REG_SP);
	INSTR->setdst(Dst, M32C_INDEXBD());
	M32C_REG_SP += 2;
	M32C_REG_PC += codelen_dst;
}

static void
m32c_pop_w_dst(void)
{
	int codelen_dst = INSTR->codelen_dst;
	uint32_t Dst;
	Dst = M32C_Read16(M32C_REG_SP);
	INSTR->setdst(Dst, M32C_INDEXWD());
	M32C_REG_SP += 2;
	M32C_REG_PC += codelen_dst;
}

void
m32c_setup_pop_size_dst(void)
{
	int dst;
	int codelen_dst;
	dst = ((ICODE16() >> 6) & 3) | ((ICODE16() >> 7) & 0x1c);
	if (ICODE16() & 0x100) {
		INSTR->setdst = general_am_set(dst, 2, &codelen_dst, GAM_ALL);
		INSTR->proc = m32c_pop_w_dst;
	} else {
		INSTR->setdst = general_am_set(dst, 1, &codelen_dst, GAM_ALL);
		INSTR->proc = m32c_pop_b_dst;
	}
	INSTR->cycles = 3;
	INSTR->codelen_dst = codelen_dst;
	INSTR->proc();
}

/*
 *******************************************************
 * \fn void m32c_pop_size_dst(void)
 * Pop something from stack into an indirect destination.
 * v0
 *******************************************************
 */
static void
m32c_pop_w_idst(void)
{
	int codelen_dst = INSTR->codelen_dst;
	uint32_t Dst;
	uint32_t DstP;
	INSTR->getdstp(&DstP, 0);
	DstP = (DstP + M32C_INDEXWD()) & 0xffffff;
	Dst = M32C_Read16(M32C_REG_SP);
	M32C_Write16(Dst, DstP);
	M32C_REG_SP += 2;
	M32C_REG_PC += codelen_dst;
	dbgprintf("m32c_pop_size_dst not tested\n");
}

static void
m32c_pop_b_idst(void)
{
	int codelen_dst = INSTR->codelen_dst;
	uint32_t Dst;
	uint32_t DstP;
	INSTR->getdstp(&DstP, 0);
	DstP = (DstP + M32C_INDEXBD()) & 0xffffff;
	Dst = M32C_Read8(M32C_REG_SP);
	M32C_Write8(Dst, DstP);
	M32C_REG_SP += 2;
	M32C_REG_PC += codelen_dst;
	dbgprintf("m32c_pop_size_dst not tested\n");
}

void
m32c_setup_pop_size_idst(void)
{
	int dst;
	int codelen_dst;
	dst = ((ICODE24() >> 6) & 3) | ((ICODE24() >> 7) & 0x1c);
	INSTR->getdstp = general_am_get(dst, 4, &codelen_dst, GAM_ALL);
	INSTR->codelen_dst = codelen_dst;
	INSTR->cycles = 6;
	if (ICODE24() & 0x100) {
		INSTR->proc = m32c_pop_w_idst;
	} else {
		INSTR->proc = m32c_pop_b_idst;
	}
	INSTR->proc();
}

/**
 ********************************************************************
 * \fn void m32c_popc_dst1(void)
 * Pop a 16 bit control register from the stack.
 ********************************************************************
 */
static void
m32c_popc_dst1(void)
{
	uint32_t Dst;
	int am = INSTR->Arg1;
	Dst = M32C_Read16(M32C_REG_SP);
	/* Keep the order of the following two instructions ! */
	M32C_REG_SP += 2;
	setreg_cdi16(am, Dst, 0xff);
}

void
m32c_setup_popc_dst1(void)
{
	int am = ICODE16() & 0x7;
	INSTR->Arg1 = am;
	INSTR->proc = m32c_popc_dst1;
	INSTR->proc();
}

/**
 ****************************************************************
 * \fn void m32c_popc_dst2(void)
 * Pop a 24 bit control register from stack.
 ****************************************************************
 */
static void
m32c_popc_dst2(void)
{
	uint32_t Dst;
	int am = INSTR->Arg1;
	Dst = M32C_Read24(M32C_REG_SP);
	/* Keep the order because dst might be SP */
	M32C_REG_SP += 4;
	setreg_cdi24low(am, Dst, 0x8f);
	dbgprintf("m32c_popc_dst2 not tested\n");
}

void
m32c_setup_popc_dst2(void)
{
	int am = ICODE16() & 7;
	INSTR->Arg1 = am;
	INSTR->proc = m32c_popc_dst2;
	INSTR->proc();
}

/**
 *************************************************************
 * \fn void m32c_popm_dst(void)
 * Pop multiple registers from stack
 *************************************************************
 */
void
m32c_popm_dst(void)
{
	uint32_t Dst = M32C_Read8(M32C_REG_PC);
	M32C_REG_PC++;
	uint32_t cycles = 0;
	if (Dst & 1) {
		M32C_REG_R0 = M32C_Read16(M32C_REG_SP);
		M32C_REG_SP += 2;
		cycles += 1;
	}
	if (Dst & 2) {
		M32C_REG_R1 = M32C_Read16(M32C_REG_SP);
		M32C_REG_SP += 2;
		cycles += 1;
	}
	if (Dst & 4) {
		M32C_REG_R2 = M32C_Read16(M32C_REG_SP);
		M32C_REG_SP += 2;
		cycles += 1;
	}
	if (Dst & 8) {
		M32C_REG_R3 = M32C_Read16(M32C_REG_SP);
		M32C_REG_SP += 2;
		cycles += 1;
	}
	if (Dst & 0x10) {
		M32C_REG_A0 = M32C_Read24(M32C_REG_SP);
		M32C_REG_SP += 4;
		cycles += 2;
	}
	if (Dst & 0x20) {
		M32C_REG_A1 = M32C_Read24(M32C_REG_SP);
		M32C_REG_SP += 4;
		cycles += 2;
	}
	if (Dst & 0x40) {
		M32C_REG_SB = M32C_Read24(M32C_REG_SP);
		M32C_REG_SP += 4;
		cycles += 2;
	}
	if (Dst & 0x80) {
		M32C_REG_FB = M32C_Read24(M32C_REG_SP);
		M32C_REG_SP += 4;
		cycles += 2;
	}
	CycleCounter += cycles;
	dbgprintf("m32c_popm_dst not tested\n");
}

/**
 ************************************************************
 * \fn void m32c_push_size_imm(void)
 * Push an immediate to stack.
 * v0
 ************************************************************
 */

static void
m32c_push_b_imm(void)
{
	uint32_t Imm;
	M32C_REG_SP -= 2;
	Imm = M32C_Read8(M32C_REG_PC);
	M32C_Write8(Imm, M32C_REG_SP);
	M32C_REG_PC += 1;
	dbgprintf("m32c_push_size_imm SP now %06x\n", M32C_REG_SP);
}

static void
m32c_push_w_imm(void)
{
	uint32_t Imm;
	M32C_REG_SP -= 2;
	Imm = M32C_Read16(M32C_REG_PC);
	M32C_Write16(Imm, M32C_REG_SP);
	M32C_REG_PC += 2;
	dbgprintf("m32c_push_size_imm SP now %06x\n", M32C_REG_SP);
}

void
m32c_setup_push_size_imm(void)
{
	if (ICODE8() & 1) {
		INSTR->proc = m32c_push_w_imm;
	} else {
		INSTR->proc = m32c_push_b_imm;
	}
	INSTR->proc();
}

/**
 **********************************************************
 * \fn void m32c_push_size_src(void)
 * Push a source to the stack.
 * v0
 **********************************************************
 */
static void
m32c_push_b_src(void)
{
	int codelen_src = INSTR->codelen_src;
	uint32_t Src;
	M32C_REG_SP -= 2;
	INSTR->getsrc(&Src, M32C_INDEXBD());
	M32C_Write8(Src, M32C_REG_SP);
	M32C_REG_PC += codelen_src;
	//fprintf(stderr,"m32c_push_size_src SP now %06x\n",M32C_REG_SP);
}

static void
m32c_push_w_src(void)
{
	int codelen_src = INSTR->codelen_src;
	uint32_t Src;
	M32C_REG_SP -= 2;
	INSTR->getsrc(&Src, M32C_INDEXWD());
	M32C_Write16(Src, M32C_REG_SP);
	M32C_REG_PC += codelen_src;
	//fprintf(stderr,"m32c_push_size_src SP now %06x\n",M32C_REG_SP);
}

void
m32c_setup_push_size_src(void)
{
	int src;
	int codelen_src;
	src = ((ICODE16() >> 6) & 3) | ((ICODE16() >> 7) & 0x1c);
	if (ICODE16() & 0x100) {
		INSTR->getsrc = general_am_get(src, 2, &codelen_src, GAM_ALL);
		INSTR->proc = m32c_push_w_src;
	} else {
		INSTR->getsrc = general_am_get(src, 1, &codelen_src, GAM_ALL);
		INSTR->proc = m32c_push_b_src;
	}
	INSTR->codelen_src = codelen_src;
	INSTR->proc();
}

/**
 **********************************************************
 * \fn void m32c_push_size_isrc(void)
 * Push from an indirect source to the stack.
 * v0
 **********************************************************
 */
static void
m32c_push_w_isrc(void)
{
	int codelen_src = INSTR->codelen_src;
	uint32_t Src;
	uint32_t SrcP;
	INSTR->getsrcp(&SrcP, 0);
	SrcP = (SrcP + M32C_INDEXWD()) & 0xffffff;
	Src = M32C_Read16(SrcP);
	M32C_Write16(Src, M32C_REG_SP);
	M32C_REG_PC += codelen_src;
	dbgprintf("m32c_push_size_src not tested\n");
}

static void
m32c_push_b_isrc(void)
{
	int codelen_src = INSTR->codelen_src;
	uint32_t Src;
	uint32_t SrcP;
	INSTR->getsrcp(&SrcP, 0);
	SrcP = (SrcP + M32C_INDEXBD()) & 0xffffff;
	Src = M32C_Read8(SrcP);
	M32C_Write8(Src, M32C_REG_SP);
	M32C_REG_PC += codelen_src;
	dbgprintf("m32c_push_size_src not tested\n");
}

void
m32c_setup_push_size_isrc(void)
{
	int src;
	int codelen_src;
	src = ((ICODE24() >> 6) & 3) | ((ICODE24() >> 7) & 0x1c);
	M32C_REG_SP -= 2;
	INSTR->getsrcp = general_am_get(src, 4, &codelen_src, GAM_ALL);
	INSTR->codelen_src = codelen_src;
	if (ICODE24() & 0x100) {
		INSTR->proc = m32c_push_w_isrc;
	} else {
		INSTR->proc = m32c_push_b_isrc;
	}
	INSTR->proc();
}

/**
 ****************************************************
 * \fn void m32c_push_l_imm32(void)
 * Push a 32 Bit immediate to the Stack
 * v0
 ****************************************************
 */
void
m32c_push_l_imm32(void)
{
	uint32_t Imm;
	Imm = M32C_Read32((M32C_REG_PC + M32C_INDEXLD()) & 0xffffff);
	M32C_REG_SP -= 4;
	M32C_Write32(Imm, M32C_REG_SP);
	M32C_REG_PC += 4;
	dbgprintf("m32c_push_l_imm32 not tested\n");
}

/**
 *************************************************************
 * \fn void m32c_push_l_src(void)
 * Push a 32 Bit value from a source to the stack. 
 * v0
 *************************************************************
 */
static void
m32c_push_l_src(void)
{
	int codelen_src = INSTR->codelen_src;
	uint32_t Src;
	INSTR->getsrc(&Src, M32C_INDEXLD());
	M32C_REG_SP -= 4;
	M32C_Write32(Src, M32C_REG_SP);
	M32C_REG_PC += codelen_src;
	dbgprintf("m32c_push_l_src not tested\n");
}

void
m32c_setup_push_l_src(void)
{
	int src;
	int codelen_src;
	src = ((ICODE16() >> 6) & 3) | ((ICODE16() >> 7) & 0x1c);
	INSTR->getsrc = general_am_get(src, 4, &codelen_src, GAM_ALL);
	INSTR->codelen_src = codelen_src;
	if (INSTR->nrMemAcc == 1) {
		INSTR->cycles = 5;
	} else {
		INSTR->cycles = 2;
	}
	INSTR->proc = m32c_push_l_src;
	INSTR->proc();
}

/**
 ****************************************************************
 * \fn void m32c_push_l_isrc(void)
 * Push a long from an indirect source to the stack.
 * v0
 ****************************************************************
 */
static void
m32c_push_l_isrc(void)
{
	int codelen_src = INSTR->codelen_src;
	uint32_t Src, SrcP;
	INSTR->getsrc(&SrcP, 0);
	SrcP = (SrcP + M32C_INDEXLD()) & 0xffffff;
	Src = M32C_Read32(SrcP);
	M32C_REG_SP -= 4;
	M32C_Write32(Src, M32C_REG_SP);
	M32C_REG_PC += codelen_src;
	dbgprintf("m32c_push_l_isrc not implemented\n");
}

void
m32c_setup_push_l_isrc(void)
{
	int src;
	int codelen_src;
	src = ((ICODE24() >> 6) & 3) | ((ICODE24() >> 7) & 0x1c);
	INSTR->getsrc = general_am_get(src, 4, &codelen_src, GAM_ALL);
	INSTR->codelen_src = codelen_src;
	if (INSTR->nrMemAcc == 1) {
		INSTR->cycles = 8;
	} else {
		INSTR->cycles = 5;
	}
	INSTR->proc = m32c_push_l_isrc;
	INSTR->proc();
}

/**
 **************************************************************
 * \fn void m32c_pusha_src(void)
 * Push an effective address onto the stack.
 * v0
 **************************************************************
 */
static void
m32c_pusha_src(void)
{
	int codelen;
	uint32_t Efa;
	int src = INSTR->Arg1;
	Efa = general_am_efa(src, &codelen, GAM_ALL);
	M32C_REG_SP -= 4;
	M32C_Write32(Efa, M32C_REG_SP);
	M32C_REG_PC += codelen;
	dbgprintf("m32c_pusha_src not tested\n");
}

void
m32c_setup_pusha_src(void)
{
	int src;
	src = ((ICODE16() >> 6) & 3) | ((ICODE16() >> 7) & 0x1c);
	INSTR->Arg1 = src;
	INSTR->cycles = 3;
	INSTR->proc = m32c_pusha_src;
	INSTR->proc();
}

/**
 ***************************************************************
 * \fn void m32c_pushc_src1(void)
 * Push a 16 Bit control register onto the stack.
 ***************************************************************
 */
static void
m32c_pushc_src1(void)
{
	uint16_t Src;
	int am = INSTR->Arg1;
	getreg_cdi16(am, &Src, 0xff);
	M32C_REG_SP -= 2;
	M32C_Write16(Src, M32C_REG_SP);
}

void
m32c_setup_pushc_src1(void)
{
	int am = ICODE16() & 0x7;
	INSTR->Arg1 = am;
	INSTR->proc = m32c_pushc_src1;
	INSTR->proc();
}

/**
 ***************************************************
 * \fn void m32c_pushc_src2(void)
 * Push a 24 bit control register onto the stack.
 * v0
 ***************************************************
 */
static void
m32c_pushc_src2(void)
{
	uint32_t Src;
	int am = INSTR->Arg1;
	/* Keep the order of the following two instructions ! */
	getreg_cdi24low(am, &Src, 0x8f);
	M32C_REG_SP -= 4;
	M32C_Write32(Src, M32C_REG_SP);
	dbgprintf("m32c_pushc_src2 not tested\n");
}

void
m32c_setup_pushc_src2(void)
{
	int am = ICODE16() & 0x7;
	INSTR->Arg1 = am;
	INSTR->proc = m32c_pushc_src2;
	INSTR->proc();
}

/**
 **********************************************************************
 * \fn void m32c_pushm_src(void)
 * Push multiple registers onto the stack.
 * v0
 **********************************************************************
 */
void
m32c_pushm_src(void)
{
	uint32_t src = M32C_Read8(M32C_REG_PC);
	M32C_REG_PC++;
	if (src & 1) {
		M32C_REG_SP -= 4;
		M32C_Write32(M32C_REG_FB, M32C_REG_SP);
		CycleCounter += 2;
	}
	if (src & 2) {
		M32C_REG_SP -= 4;
		M32C_Write32(M32C_REG_SB, M32C_REG_SP);
		CycleCounter += 2;
	}
	if (src & 4) {
		M32C_REG_SP -= 4;
		M32C_Write32(M32C_REG_A1, M32C_REG_SP);
	}
	CycleCounter += 2;
	if (src & 8) {
		M32C_REG_SP -= 4;
		M32C_Write32(M32C_REG_A0, M32C_REG_SP);
		CycleCounter += 2;
	}
	if (src & 0x10) {
		M32C_REG_SP -= 2;
		M32C_Write16(M32C_REG_R3, M32C_REG_SP);
		CycleCounter += 1;
	}
	if (src & 0x20) {
		M32C_REG_SP -= 2;
		M32C_Write16(M32C_REG_R2, M32C_REG_SP);
		CycleCounter += 1;
	}
	if (src & 0x40) {
		M32C_REG_SP -= 2;
		M32C_Write16(M32C_REG_R1, M32C_REG_SP);
		CycleCounter += 1;
	}
	if (src & 0x80) {
		M32C_REG_SP -= 2;
		M32C_Write16(M32C_REG_R0, M32C_REG_SP);
		CycleCounter += 1;
	}
	dbgprintf("m32c_pushm_src not tested\n");
}

/**
 *************************************************************
 * \fn void m32c_reit(void)
 * Return from interrupt.
 * v0
 *************************************************************
 */
void
m32c_reit(void)
{
	uint16_t flg;
    uint32_t pc;
	pc = M32C_Read16(M32C_REG_SP);
	M32C_REG_SP += 2;
	pc |= ((M32C_Read16(M32C_REG_SP) & 0xff) << 16);
	dbgprintf("Restored PC %06x from SP %06x\n", M32C_REG_PC, M32C_REG_SP);
	M32C_REG_SP += 2;
	flg = M32C_Read16(M32C_REG_SP);
	M32C_REG_SP += 2;
	M32C_SET_REG_FLG(flg);
    M32C_REG_PC = pc;
	CycleCounter += 5;
	dbgprintf("m32c_reit not tested\n");
}

/**
 ********************************************************************
 * Repeat multiply and addition.
 * Manual has contradictions about time of incrementation of
 * A0 and A1 and does not specify the overflow flag behaviour 
 * exactly.
 * State: buggy
 ********************************************************************
 */
static void
m32c_rmpa_size(void)
{
	int64_t r1r2r0;
	int32_t ma0, ma1;
	int size = INSTR->opsize;
	r1r2r0 = M32C_REG_R0 | (M32C_REG_R2 << 16) | (((int64_t) (int16_t) M32C_REG_R1) << 32);
	if (M32C_REG_R3) {
		M32C_REG_R3--;
		if (size == 2) {
			ma0 = (int32_t) (int16_t) M32C_Read16(M32C_REG_A0);
			ma1 = (int32_t) (int16_t) M32C_Read16(M32C_REG_A1);
			r1r2r0 += ma0 * ma1;
			if (M32C_REG_R3) {
				M32C_REG_A0 += 2;
				M32C_REG_A1 += 2;
			}
		} else {
			ma0 = (int32_t) (int8_t) M32C_Read8(M32C_REG_A0);
			ma1 = (int32_t) (int8_t) M32C_Read8(M32C_REG_A1);
			r1r2r0 += ma0 * ma1;
			if (M32C_REG_R3) {
				M32C_REG_A0 += 1;
				M32C_REG_A1 += 1;
			}
		}
		M32C_REG_PC -= 2;

		M32C_REG_R1 = r1r2r0 >> 32;
		M32C_REG_R2 = (r1r2r0 >> 16) & 0xffff;
		M32C_REG_R0 = r1r2r0 & 0xffff;
	} else {
		CycleCounter += 7;
		if ((r1r2r0 >= (UINT64_C(1) << 31))
		    || ((r1r2r0) < -(UINT64_C(1) << 31))) {
			M32C_REG_FLG |= M32C_FLG_OVERFLOW;
		} else {
			M32C_REG_FLG &= ~M32C_FLG_OVERFLOW;
		}
	}
	dbgprintf("m32c_rmpa_size not tested\n");
}

void
m32c_setup_rmpa_size(void)
{
	if (ICODE16() & 0x10) {
		INSTR->opsize = 2;
	} else {
		INSTR->opsize = 1;
	}
	INSTR->proc = m32c_rmpa_size;
	INSTR->proc();
}

/**
 *************************************************************
 * \fn void m32c_rolc_size_dst(void)
 * Rotate left by one with carry. 
 * Carry goes to lsb and msb goes to carry.
 * v0
 *************************************************************
 */
static void
m32c_rolc_size_dst(void)
{
	int opsize = INSTR->opsize;
	int codelen_dst = INSTR->codelen_dst;
	uint32_t Dst, Result;
	int carry_new;
	INSTR->getdst(&Dst, M32C_INDEXSD());
	if (opsize == 2) {
		carry_new = Dst & 0x8000;
	} else {
		carry_new = Dst & 0x80;
	}
	if (M32C_REG_FLG & M32C_FLG_CARRY) {
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
		M32C_REG_FLG |= M32C_FLG_CARRY;
	} else {
		M32C_REG_FLG &= ~M32C_FLG_CARRY;
	}
	INSTR->setdst(Result, M32C_INDEXSD());
	rol_flags(Result, opsize);
	M32C_REG_PC += codelen_dst;
	dbgprintf("m32c_rolc_size_dst not tested\n");
}

void
m32c_setup_rolc_size_dst(void)
{
	int dst;
	int size, opsize;
	int codelen_dst;
	dst = ((ICODE16() >> 6) & 3) | ((ICODE16() >> 7) & 0x1c);
	if (ICODE16() & 0x100) {
		opsize = size = 2;
	} else {
		opsize = size = 1;
		ModOpsize(dst, &opsize);
	}
	INSTR->getdst = general_am_get(dst, size, &codelen_dst, GAM_ALL);
	INSTR->setdst = general_am_set(dst, opsize, &codelen_dst, GAM_ALL);
	INSTR->codelen_dst = codelen_dst;
	INSTR->opsize = opsize;
	INSTR->proc = m32c_rolc_size_dst;
	INSTR->proc();
}

/**
 *************************************************************
 * \fn void m32c_rolc_size_idst(void)
 * Rotate an indirect destination left by one with carry. 
 * Carry goes to lsb and msb goes to carry.
 * v0
 *************************************************************
 */
static void
m32c_rolc_size_idst(void)
{
	int size = INSTR->opsize;
	int codelen_dst = INSTR->codelen_dst;
	uint32_t Dst, Result;
	uint32_t DstP;
	int carry_new;
	INSTR->getdstp(&DstP, 0);
	DstP = (DstP + M32C_INDEXSD());
	if (size == 2) {
		Dst = M32C_Read16(DstP);
		carry_new = Dst & 0x8000;
	} else {
		Dst = M32C_Read8(DstP);
		carry_new = Dst & 0x80;
	}
	if (M32C_REG_FLG & M32C_FLG_CARRY) {
		Result = (Dst << 1) | 1;
	} else {
		Result = Dst << 1;
	}
	if (carry_new) {
		M32C_REG_FLG |= M32C_FLG_CARRY;
	} else {
		M32C_REG_FLG &= ~M32C_FLG_CARRY;
	}
	if (size == 2) {
		M32C_Write16(Result, DstP);
	} else {
		M32C_Write8(Result, DstP);
	}
	rol_flags(Result, size);
	M32C_REG_PC += codelen_dst;
	fprintf(stderr, "%s used\n", __FUNCTION__);
	dbgprintf("m32c_rolc_size_idst not tested\n");
}

void
m32c_setup_rolc_size_idst(void)
{
	int dst;
	int size;
	int codelen_dst;
	if (ICODE24() & 0x100) {
		size = 2;
	} else {
		size = 1;
	}
	dst = ((ICODE24() >> 6) & 3) | ((ICODE24() >> 7) & 0x1c);
	INSTR->getdstp = general_am_get(dst, 4, &codelen_dst, GAM_ALL);
	INSTR->codelen_dst = codelen_dst;
	INSTR->opsize = size;
	INSTR->proc = m32c_rolc_size_idst;
	INSTR->proc();
}

/**
 ***********************************************************
 * \fn void m32c_rorc_size_dst(void)
 * Rotate right by one with carry. Carry goes to MSB and
 * LSB goes to carry.
 * v0
 ***********************************************************
 */
static void
m32c_rorc_size_dst(void)
{
	int opsize = INSTR->opsize;
	int codelen_dst = INSTR->codelen_dst;
	uint32_t Dst, Result;
	int carry_new;
	INSTR->getdst(&Dst, M32C_INDEXSD());
	carry_new = Dst & 1;
	if (M32C_REG_FLG & M32C_FLG_CARRY) {
		if (opsize == 2) {
			Result = (Dst >> 1) | 0x8000;
		} else {
			Result = (Dst >> 1) | 0x80;
		}
	} else {
		Result = Dst >> 1;
	}
	if (carry_new) {
		M32C_REG_FLG |= M32C_FLG_CARRY;
	} else {
		M32C_REG_FLG &= ~M32C_FLG_CARRY;
	}
	INSTR->setdst(Result, M32C_INDEXSD());
	ror_flags(Result, opsize);
	M32C_REG_PC += codelen_dst;
	dbgprintf("m32c_rorc_size_dst not tested\n");
}

void
m32c_setup_rorc_size_dst(void)
{
	int dst;
	int srcsize, opsize;
	int codelen_dst;
	dst = ((ICODE16() >> 6) & 3) | ((ICODE16() >> 7) & 0x1c);
	if (ICODE16() & 0x100) {
		opsize = srcsize = 2;
	} else {
		opsize = srcsize = 1;
		ModOpsize(dst, &opsize);
	}
	INSTR->getdst = general_am_get(dst, srcsize, &codelen_dst, GAM_ALL);
	INSTR->setdst = general_am_set(dst, opsize, &codelen_dst, GAM_ALL);
	INSTR->codelen_dst = codelen_dst;
	INSTR->opsize = opsize;
	INSTR->srcsize = srcsize;
	INSTR->proc = m32c_rorc_size_dst;
	INSTR->proc();
}

/**
 ***********************************************************
 * \fn void m32c_rorc_size_idst(void)
 * Rotate an indirect destination right by one with carry. 
 * Carry goes to MSB and * LSB goes to carry.
 * v0
 ***********************************************************
 */
static void
m32c_rorc_size_idst(void)
{
	int size = INSTR->opsize;
	int codelen_dst = INSTR->codelen_dst;
	uint32_t Dst, Result;
	uint32_t DstP;
	int carry_new;
	INSTR->getdstp(&DstP, 0);
	DstP = (DstP + M32C_INDEXSD()) & 0xffffff;
	if (size == 2) {
		Dst = M32C_Read16(DstP);
		carry_new = Dst & 1;
		if (M32C_REG_FLG & M32C_FLG_CARRY) {
			Result = (Dst >> 1) | 0x8000;
		} else {
			Result = Dst >> 1;
		}
	} else {
		Dst = M32C_Read8(DstP);
		carry_new = Dst & 1;
		if (M32C_REG_FLG & M32C_FLG_CARRY) {
			Result = (Dst >> 1) | 0x80;
		} else {
			Result = Dst >> 1;
		}
	}
	if (carry_new) {
		M32C_REG_FLG |= M32C_FLG_CARRY;
	} else {
		M32C_REG_FLG &= ~M32C_FLG_CARRY;
	}
	if (size == 2) {
		M32C_Write16(Result, DstP);
	} else {
		M32C_Write8(Result, DstP);
	}
	ror_flags(Result, size);
	M32C_REG_PC += codelen_dst;
	fprintf(stderr, "%s used\n", __FUNCTION__);
	dbgprintf("m32c_rorc_size_idst not tested\n");
}

void
m32c_setup_rorc_size_idst(void)
{
	int dst;
	int size;
	int codelen_dst;
	if (ICODE24() & 0x100) {
		size = 2;
	} else {
		size = 1;
	}
	dst = ((ICODE24() >> 6) & 3) | ((ICODE24() >> 7) & 0x1c);
	INSTR->getdstp = general_am_get(dst, 4, &codelen_dst, GAM_ALL);
	INSTR->codelen_dst = codelen_dst;
	INSTR->opsize = size;

	INSTR->proc = m32c_rorc_size_idst;
	INSTR->proc();
}

/*
 ********************************************************
 * \fn void m32c_rot_size_immdst(void)
 * Rotate left or right by a four bit immediate.
 * v0
 ********************************************************
 */
static void
m32c_rot_size_immdst(void)
{
	int rot = INSTR->Arg1;
	int right = INSTR->Arg2;
	int opsize = INSTR->opsize;
	uint32_t Dst;
	int codelen_dst = INSTR->codelen_dst;
	INSTR->getdst(&Dst, M32C_INDEXSD());
	if (right) {
		if (Dst & (1 << (rot - 1))) {
			M32C_REG_FLG |= M32C_FLG_CARRY;
		} else {
			M32C_REG_FLG &= ~M32C_FLG_CARRY;
		}
		if (opsize == 2) {
			Dst = (Dst >> rot) | (Dst << (16 - rot));
			Dst = Dst & 0xffff;
		} else {
			Dst = (Dst >> rot) | (Dst << (8 - rot));
			Dst = Dst & 0xff;
		}
	} else {
		if (opsize == 2) {
			if (Dst & (1 << (16 - rot))) {
				M32C_REG_FLG |= M32C_FLG_CARRY;
			} else {
				M32C_REG_FLG &= ~M32C_FLG_CARRY;
			}
			Dst = (Dst << rot) | (Dst >> (16 - rot));
			Dst = Dst & 0xffff;
		} else {
			if (Dst & (1 << (8 - rot))) {
				M32C_REG_FLG |= M32C_FLG_CARRY;
			} else {
				M32C_REG_FLG &= ~M32C_FLG_CARRY;
			}
			Dst = (Dst << rot) | (Dst >> (8 - rot));
			Dst = Dst & 0xff;
		}
	}
	rot_flags(Dst, opsize);
	INSTR->setdst(Dst, M32C_INDEXSD());
	M32C_REG_PC += codelen_dst;
	dbgprintf("m32c_rot_size_immdst not tested\n");
}

void
m32c_setup_rot_size_immdst(void)
{
	int rot = (ICODE16() & 7) + 1;
	int right = ICODE16() & 8;
	int size, opsize;
	int dst;
	int codelen_dst;
	dst = ((ICODE16() >> 6) & 3) | ((ICODE16() >> 7) & 0x1c);
	if (ICODE16() & 0x100) {
		opsize = size = 2;
	} else {
		opsize = size = 1;
		ModOpsize(dst, &opsize);
	}
	INSTR->getdst = general_am_get(dst, size, &codelen_dst, GAM_ALL);
	INSTR->setdst = general_am_set(dst, opsize, &codelen_dst, GAM_ALL);
	INSTR->codelen_dst = codelen_dst;
	INSTR->srcsize = size;
	INSTR->opsize = opsize;
	INSTR->Arg1 = rot;
	INSTR->Arg2 = right;
	INSTR->proc = m32c_rot_size_immdst;
	INSTR->proc();
}

/**
 ******************************************************************
 * \fn void m32c_rot_size_immidst(void)
 * Rotate an immediate destination left or right 
 * by a four bit immediate.
 * v0
 ******************************************************************
 */
static void
m32c_rot_size_immidst(void)
{
	int rot = INSTR->Arg1;
	int right = INSTR->Arg2;
	int size = INSTR->opsize;
	uint32_t Dst;
	uint32_t DstP;
	int codelen_dst = INSTR->codelen_dst;
	INSTR->getdstp(&DstP, 0);
	DstP = (DstP + M32C_INDEXSD());
	if (size == 2) {
		Dst = M32C_Read16(DstP);
	} else {
		Dst = M32C_Read8(DstP);
	}
	if (right) {
		if (Dst & (1 << (rot - 1))) {
			M32C_REG_FLG |= M32C_FLG_CARRY;
		} else {
			M32C_REG_FLG &= ~M32C_FLG_CARRY;
		}
		if (size == 2) {
			Dst = (Dst >> rot) | (Dst << (16 - rot));
		} else {
			Dst = (Dst >> rot) | (Dst << (8 - rot));
		}
	} else {
		if (size == 2) {
			if (Dst & (1 << (16 - rot))) {
				M32C_REG_FLG |= M32C_FLG_CARRY;
			} else {
				M32C_REG_FLG &= ~M32C_FLG_CARRY;
			}
			Dst = (Dst << rot) | (Dst >> (16 - rot));
		} else {
			if (Dst & (1 << (8 - rot))) {
				M32C_REG_FLG |= M32C_FLG_CARRY;
			} else {
				M32C_REG_FLG &= ~M32C_FLG_CARRY;
			}
			Dst = (Dst << rot) | (Dst >> (8 - rot));
		}
	}
	rot_flags(Dst, size);
	if (size == 2) {
		M32C_Write16(Dst, DstP);
	} else {
		M32C_Write8(Dst, DstP);
	}
	M32C_REG_PC += codelen_dst;
	dbgprintf("m32c_rot_size_immidst not tested\n");
}

void
m32c_setup_rot_size_immidst(void)
{
	int rot = (ICODE24() & 7) + 1;
	int right = ICODE24() & 8;
	int size;
	int dst;
	int codelen_dst;
	dst = ((ICODE24() >> 6) & 3) | ((ICODE24() >> 7) & 0x1c);
	if (ICODE24() & 0x100) {
		size = 2;
	} else {
		size = 1;
	}
	INSTR->getdstp = general_am_get(dst, 4, &codelen_dst, GAM_ALL);
	INSTR->codelen_dst = codelen_dst;
	INSTR->opsize = size;
	INSTR->Arg1 = rot;
	INSTR->Arg2 = right;
	INSTR->proc = m32c_rot_size_immidst;
	INSTR->proc();
}

/**
 *************************************************************
 * \fn void m32c_rot_size_r1hdst(void)
 * Rotate left or right by the value in register r1h.
 * v0
 *************************************************************
 */
static void
m32c_rot_size_r1hdst(void)
{
	int size = INSTR->opsize;
	int8_t r1h = M32C_REG_R1H;
	int rot = abs(r1h);
	int right = (r1h < 0);
	uint32_t Dst;
	int codelen_dst = INSTR->codelen_dst;
	INSTR->getdst(&Dst, M32C_INDEXSD());
	if (rot == 0) {
		/* do nothing */
	} else if (right) {
		if (Dst & (1 << (rot - 1))) {
			M32C_REG_FLG |= M32C_FLG_CARRY;
		} else {
			M32C_REG_FLG &= ~M32C_FLG_CARRY;
		}
		if (size == 2) {
			Dst = (Dst >> rot) | (Dst << (16 - rot));
		} else {
			Dst = (Dst >> rot) | (Dst << (8 - rot));
		}
		rot_flags(Dst, size);
		INSTR->setdst(Dst, M32C_INDEXSD());
	} else {
		if (size == 2) {
			if (Dst & (1 << (16 - rot))) {
				M32C_REG_FLG |= M32C_FLG_CARRY;
			} else {
				M32C_REG_FLG &= ~M32C_FLG_CARRY;
			}
			Dst = (Dst << rot) | (Dst >> (16 - rot));
		} else {
			if (Dst & (1 << (8 - rot))) {
				M32C_REG_FLG |= M32C_FLG_CARRY;
			} else {
				M32C_REG_FLG &= ~M32C_FLG_CARRY;
			}
			Dst = (Dst << rot) | (Dst >> (8 - rot));
		}
		rot_flags(Dst, size);
		INSTR->setdst(Dst, M32C_INDEXSD());
	}
	M32C_REG_PC += codelen_dst;
	dbgprintf("m32c_rot_size_r1hdst not tested\n");
}

void
m32c_setup_rot_size_r1hdst(void)
{
	int size;
	int dst;
	int codelen_dst;
	dst = ((ICODE16() >> 6) & 3) | ((ICODE16() >> 7) & 0x1c);
	if (ICODE16() & 0x100) {
		size = 2;
	} else {
		size = 1;
	}
	INSTR->getdst = general_am_get(dst, size, &codelen_dst, GAM_ALL);
	INSTR->setdst = general_am_set(dst, size, &codelen_dst, GAM_ALL);
	INSTR->codelen_dst = codelen_dst;
	INSTR->opsize = size;
	INSTR->proc = m32c_rot_size_r1hdst;
	INSTR->proc();
}

/**
 *************************************************************
 * \fn void m32c_rot_size_r1hdst(void)
 * Rotate an indirect destination left or right 
 * by the value in register r1h.
 * v0
 *************************************************************
 */
static void
m32c_rot_size_r1hidst(void)
{
	int8_t r1h = M32C_REG_R1H;
	int rot = abs(r1h);
	int right = (r1h < 0);
	uint32_t Dst;
	uint32_t DstP;
	int size = INSTR->opsize;
	int codelen_dst = INSTR->codelen_dst;

	INSTR->getdstp(&DstP, 0);
	DstP = (DstP + M32C_INDEXSD()) & 0xffffff;
	if (size == 2) {
		Dst = M32C_Read16(DstP);
	} else {
		Dst = M32C_Read8(DstP);
	}
	if (rot == 0) {
		/* Do nothing */
	} else if (right) {
		if (Dst & (1 << (rot - 1))) {
			M32C_REG_FLG |= M32C_FLG_CARRY;
		} else {
			M32C_REG_FLG &= ~M32C_FLG_CARRY;
		}
		if (size == 2) {
			Dst = (Dst >> rot) | (Dst << (16 - rot));
			M32C_Write16(Dst, DstP);
		} else {
			Dst = (Dst >> rot) | (Dst << (8 - rot));
			M32C_Write8(Dst, DstP);
		}
		rot_flags(Dst, size);
	} else {
		if (size == 2) {
			if (Dst & (1 << (16 - rot))) {
				M32C_REG_FLG |= M32C_FLG_CARRY;
			} else {
				M32C_REG_FLG &= ~M32C_FLG_CARRY;
			}
			Dst = (Dst << rot) | (Dst >> (16 - rot));
			M32C_Write16(Dst, DstP);
		} else {
			if (Dst & (1 << (8 - rot))) {
				M32C_REG_FLG |= M32C_FLG_CARRY;
			} else {
				M32C_REG_FLG &= ~M32C_FLG_CARRY;
			}
			Dst = (Dst << rot) | (Dst >> (8 - rot));
			M32C_Write8(Dst, DstP);
		}
		rot_flags(Dst, size);
	}
	M32C_REG_PC += codelen_dst;
	dbgprintf("m32c_rot_size_r1hidst not tested\n");
}

void
m32c_setup_rot_size_r1hidst(void)
{
	int size;
	int dst;
	int codelen_dst;
	dst = ((ICODE24() >> 6) & 3) | ((ICODE24() >> 7) & 0x1c);
	if (ICODE24() & 0x100) {
		size = 2;
	} else {
		size = 1;
	}
	INSTR->getdstp = general_am_get(dst, 4, &codelen_dst, GAM_ALL);
	INSTR->codelen_dst = codelen_dst;
	INSTR->opsize = size;
	INSTR->proc = m32c_rot_size_r1hidst;
	INSTR->proc();
}

/**
 *********************************************
 * \fn void m32c_rts(void)
 * Return from subroutine.
 * v0
 *********************************************
 */
void
m32c_rts(void)
{
	M32C_REG_PC = M32C_Read24(M32C_REG_SP) & 0xffffff;
	M32C_REG_SP += 4;
	dbgprintf("m32c_rts to %06x\n", pc);
}

/**
 ****************************************************************
 * \fn void m32c_sbb_size_immdst(void)
 * Subtract an immediate from a destination with borrow.
 * v0 
 ****************************************************************
 */

static void
m32c_sbb_size_immdst(void)
{
	uint32_t Src, Dst, Result;
	int opsize = INSTR->opsize;
	int srcsize = INSTR->srcsize;
	int codelen_dst = INSTR->codelen_dst;
	INSTR->getdst(&Dst, M32C_INDEXSD());
	if (srcsize == 2) {
		Src = M32C_Read16((M32C_REG_PC + codelen_dst) & 0xffffff);
	} else {		/* if(srcsize == 1) */

		Src = M32C_Read8((M32C_REG_PC + codelen_dst) & 0xffffff);
	}
	if (M32C_REG_FLG & M32C_FLG_CARRY) {
		Result = Dst - Src;
	} else {
		Result = Dst - Src - 1;
	}
	INSTR->setdst(Result, M32C_INDEXSD());
	sub_flags(Dst, Src, Result, opsize);
	M32C_REG_PC += codelen_dst + srcsize;
	dbgprintf("m32c_sbb_size_immdst not tested\n");
}

void
m32c_setup_sbb_size_immdst(void)
{
	int dst;
	int opsize, srcsize;
	int codelen_dst;
	dst = ((ICODE24() >> 6) & 3) | ((ICODE24() >> 7) & 0x1c);
	if (ICODE24() & 0x100) {
		srcsize = opsize = 2;
	} else {
		srcsize = opsize = 1;
		ModOpsize(dst, &opsize);
	}
	INSTR->getdst = general_am_get(dst, opsize, &codelen_dst, GAM_ALL);
	INSTR->setdst = general_am_set(dst, opsize, &codelen_dst, GAM_ALL);
	INSTR->codelen_dst = codelen_dst;
	INSTR->opsize = opsize;
	INSTR->srcsize = srcsize;
	INSTR->proc = m32c_sbb_size_immdst;
	INSTR->proc();
}

/**
 ****************************************************************
 * \fn void m32c_sbb_size_srcdst(void)
 * Subtract a source from a destination with borrow.
 * v0 
 ****************************************************************
 */

static void
m32c_sbb_size_srcdst(void)
{
	uint32_t Src, Dst, Result;
	int opsize = INSTR->opsize;
	int codelen_src = INSTR->codelen_src;
	int codelen_dst = INSTR->codelen_dst;
	INSTR->getsrc(&Src, M32C_INDEXSS());
	M32C_REG_PC += codelen_src;
	INSTR->getdst(&Dst, M32C_INDEXSD());
	if (M32C_REG_FLG & M32C_FLG_CARRY) {
		Result = Dst - Src;
	} else {
		Result = Dst - Src - 1;
	}
	INSTR->setdst(Result, M32C_INDEXSD());
	sub_flags(Dst, Src, Result, opsize);
	M32C_REG_PC += codelen_dst;
}

void
m32c_setup_sbb_size_srcdst(void)
{
	int dst, src;
	int srcsize, opsize;
	int codelen_src;
	int codelen_dst;
	dst = ((ICODE24() >> 6) & 3) | ((ICODE24() >> 7) & 0x1c);
	src = ((ICODE24() >> 4) & 3) | ((ICODE24() >> 10) & 0x1c);
	if (ICODE24() & 0x100) {
		srcsize = opsize = 2;
	} else {
		srcsize = opsize = 1;
		ModOpsize(dst, &opsize);
	}
	INSTR->getsrc = general_am_get(src, srcsize, &codelen_src, GAM_ALL);
	INSTR->getdst = general_am_get(dst, opsize, &codelen_dst, GAM_ALL);
	INSTR->setdst = general_am_set(dst, opsize, &codelen_dst, GAM_ALL);
	INSTR->codelen_src = codelen_src;
	INSTR->codelen_dst = codelen_dst;
	INSTR->opsize = opsize;
	INSTR->srcsize = srcsize;
	INSTR->proc = m32c_sbb_size_srcdst;
	INSTR->proc();
}

/**
 ************************************************************
 * \fn void m32c_sccnd_dst(void)
 * Store on condition.
 * v0
 ************************************************************
 */
static void
m32c_sccnd_dst(void)
{
	int cnd = INSTR->Arg1;
	int codelen_dst = INSTR->codelen_dst;
	if (check_condition(cnd)) {
		INSTR->setdst(1, M32C_INDEXWD());
	} else {
		INSTR->setdst(0, M32C_INDEXWD());
	}
	M32C_REG_PC += codelen_dst;
	dbgprintf("m32c_sccnd_dst not tested\n");
}

void
m32c_setup_sccnd_dst(void)
{
	int cnd = ICODE16() & 0xf;
	int codelen_dst;
	int dst;
	dst = ((ICODE16() >> 6) & 3) | ((ICODE16() >> 7) & 0x1c);
	INSTR->setdst = general_am_set(dst, 2, &codelen_dst, GAM_ALL);
	INSTR->codelen_dst = codelen_dst;
	/* I don't really believe the manual here: */
	INSTR->cycles = 1;
	INSTR->Arg1 = cnd;
	INSTR->proc = m32c_sccnd_dst;
	INSTR->proc();
}

/**
 ************************************************************
 * \fn void m32c_sccnd_idst(void)
 * Store on condition to an indirect destination.
 * v0
 ************************************************************
 */
static void
m32c_sccnd_idst(void)
{
	int cnd = INSTR->Arg1;
	int codelen_dst = INSTR->codelen_dst;
	uint32_t DstP;
	INSTR->getdstp(&DstP, 0);
	DstP = (DstP + M32C_INDEXWD()) & 0xffffff;
	if (check_condition(cnd)) {
		M32C_Write16(1, DstP);
	} else {
		M32C_Write16(0, DstP);
	}
	M32C_REG_PC += codelen_dst;
	dbgprintf("m32c_sccnd_idst not tested\n");
}

void
m32c_setup_sccnd_idst(void)
{
	int cnd = ICODE24() & 0xf;
	int codelen_dst;
	int dst;
	dst = ((ICODE24() >> 6) & 3) | ((ICODE24() >> 7) & 0x1c);
	INSTR->getdstp = general_am_get(dst, 4, &codelen_dst, GAM_ALL);
	INSTR->codelen_dst = codelen_dst;
	INSTR->Arg1 = cnd;
	/* I don't really believe the manual here: */
	INSTR->cycles = 4;
	INSTR->proc = m32c_sccnd_idst;
	INSTR->proc();
}

/**
 *******************************************************
 * \fn void m32c_scmpu_size(void)
 * String compare unequal.
 * v0
 *******************************************************
 */

static void
m32c_scmpu_b(void)
{
	uint8_t tmp0 = M32C_Read8(M32C_REG_A0);
	uint8_t tmp2 = M32C_Read8(M32C_REG_A1);
	uint8_t result = tmp0 - tmp2;
	sub_flags(tmp0, tmp2, result, 1);
	M32C_REG_A0 += 1;
	M32C_REG_A1 += 1;
	if ((tmp0 != 0) && (tmp0 == tmp2)) {
		M32C_REG_PC -= 2;
	} else if (tmp0 == 0) {
		/* 
		 *************************************************
		 * If terminaand M(A0) is 0 the 
		 * ZERO FLAG is not set. Tested with the real
		 * CPU. The manual is wrong here. R32C manual
		 * is better.
		 *************************************************
		 */
		CycleCounter += 6;
	} else {
		CycleCounter += 6;
	}
	dbgprintf("m32c_scmpu_size not tested\n");
}

static void
m32c_scmpu_w(void)
{
	uint8_t tmp0 = M32C_Read8(M32C_REG_A0);
	uint8_t tmp1 = M32C_Read8(M32C_REG_A0 + 1);
	uint8_t tmp2 = M32C_Read8(M32C_REG_A1);
	uint8_t tmp3 = M32C_Read8(M32C_REG_A1 + 1);
	uint8_t result;
	if ((tmp0 == tmp2) && (tmp0 != 0)) {
		result = tmp1 - tmp3;
		sub_flags(tmp1, tmp3, result, 1);
	} else {
		result = tmp0 - tmp2;
		sub_flags(tmp0, tmp2, result, 1);
	}
	M32C_REG_A0 += 2;
	M32C_REG_A1 += 2;
	if ((tmp0 != 0) && (tmp1 != 0)
	    && (tmp0 == tmp2) && (tmp1 == tmp3)) {
		M32C_REG_PC -= 2;
	} else if (tmp0 == 0) {
		/* 
		 *************************************************
		 * If terminaand M(A0) is 0 the 
		 * ZERO FLAG is not set. Tested with the real
		 * CPU. The manual is wrong here. R32C manual
		 * is better.
		 *************************************************
		 */
		CycleCounter += 8;
	} else {
		CycleCounter += 6;
	}
	dbgprintf("m32c_scmpu_size not tested\n");
}

void
m32c_setup_scmpu_size(void)
{
	if (ICODE16() & 0x10) {
		INSTR->cycles = 3;	/* I do two bytes per call, so this is 1.5 per byte */
		INSTR->proc = m32c_scmpu_w;
	} else {
		INSTR->cycles = 3;
		INSTR->proc = m32c_scmpu_b;
	}
	INSTR->proc();
}

/**
 ***************************************************************
 * \fn void m32c_sha_size_immdst(void)
 * Shift arithmetic left or right by a 4 Bit immediate.
 ***************************************************************
 */
static void
m32c_sha_size_immdst(void)
{
	int sha = INSTR->Arg1;
	int right = INSTR->Arg2;
	int opsize = INSTR->opsize;;
	uint32_t u32Dst;
	int32_t sDst;
	int codelen_dst = INSTR->codelen_src;
	INSTR->getdst(&u32Dst, M32C_INDEXSD());
	if (right) {
		if (opsize == 2) {
			sDst = (int32_t) (int16_t) u32Dst;
			if (sDst & (1 << (sha - 1))) {
				M32C_REG_FLG |= M32C_FLG_CARRY;
			} else {
				M32C_REG_FLG &= ~M32C_FLG_CARRY;
			}
			sDst = (sDst >> sha);
			sDst = sDst & 0xffff;
		} else {
			sDst = (int32_t) (int8_t) u32Dst;
			if (sDst & (1 << (sha - 1))) {
				M32C_REG_FLG |= M32C_FLG_CARRY;
			} else {
				M32C_REG_FLG &= ~M32C_FLG_CARRY;
			}
			sDst = (sDst >> sha);
			sDst = sDst & 0xff;
		}
		M32C_REG_FLG &= ~M32C_FLG_OVERFLOW;
	} else {
		if (opsize == 2) {
			sDst = (int32_t) (int16_t) u32Dst;
			if (sDst & (1 << (16 - sha))) {
				M32C_REG_FLG |= M32C_FLG_CARRY;
			} else {
				M32C_REG_FLG &= ~M32C_FLG_CARRY;
			}
			sDst = (sDst << sha);
			if (((sDst & UINT32_C(0xFFFF8000))
			     == UINT32_C(0xFFFF8000))
			    || ((sDst & UINT32_C(0xFFFF8000)) == 0)) {
				M32C_REG_FLG &= ~M32C_FLG_OVERFLOW;
			} else {
				M32C_REG_FLG |= M32C_FLG_OVERFLOW;
			}
			sDst = sDst & 0xffff;
		} else {
			sDst = (int32_t) (int8_t) u32Dst;
			if (sDst & (1 << (8 - sha))) {
				M32C_REG_FLG |= M32C_FLG_CARRY;
			} else {
				M32C_REG_FLG &= ~M32C_FLG_CARRY;
			}
			sDst = (sDst << sha);
			if (((sDst & UINT32_C(0xFFFFFF80))
			     == UINT32_C(0xFFFFFF80))
			    || ((sDst & UINT32_C(0xFFFFFF80)) == 0)) {
				M32C_REG_FLG &= ~M32C_FLG_OVERFLOW;
			} else {
				M32C_REG_FLG |= M32C_FLG_OVERFLOW;
			}
			sDst = sDst & 0xff;
		}
	}
	sha_flags(sDst, opsize);
	INSTR->setdst(sDst, M32C_INDEXSD());
	M32C_REG_PC += codelen_dst;
}

void
m32c_setup_sha_size_immdst(void)
{
	int sha = (ICODE16() & 7) + 1;
	int right = ICODE16() & 8;
	int size, opsize;
	int dst;
	int codelen_dst;
	dst = ((ICODE16() >> 6) & 3) | ((ICODE16() >> 7) & 0x1c);
	if (ICODE16() & 0x100) {
		opsize = size = 2;
	} else {
		opsize = size = 1;
		ModOpsize(dst, &opsize);
	}
	INSTR->getdst = general_am_get(dst, size, &codelen_dst, GAM_ALL);
	INSTR->setdst = general_am_set(dst, opsize, &codelen_dst, GAM_ALL);
	INSTR->codelen_dst = codelen_dst;
	INSTR->opsize = opsize;
	INSTR->srcsize = size;
	INSTR->Arg1 = sha;
	INSTR->Arg2 = right;
	INSTR->proc = m32c_sha_size_immdst;
	INSTR->proc();
}

/**
 ***************************************************************
 * \fn void m32c_sha_size_immidst(void)
 * Shift arithmetic left or right of an indirect destination 
 * by a 4 Bit immediate.
 ***************************************************************
 */
static void
m32c_sha_size_immidst(void)
{
	int sha = INSTR->Arg1;
	int right = INSTR->Arg2;
	int size = INSTR->opsize;
	uint32_t DstP;
	int32_t sDst;
	int codelen_dst = INSTR->codelen_dst;
	INSTR->getdstp(&DstP, 0);
	DstP = (DstP + M32C_INDEXSD());
	if (right) {
		if (size == 2) {
			sDst = (int32_t) (int16_t) M32C_Read16(DstP);
			if (sDst & (1 << (sha - 1))) {
				M32C_REG_FLG |= M32C_FLG_CARRY;
			} else {
				M32C_REG_FLG &= ~M32C_FLG_CARRY;
			}
			sDst = (sDst >> sha);
			sha_flags(sDst, size);
			M32C_Write16(sDst, DstP);
		} else {
			sDst = (int32_t) (int8_t) M32C_Read8(DstP);
			if (sDst & (1 << (sha - 1))) {
				M32C_REG_FLG |= M32C_FLG_CARRY;
			} else {
				M32C_REG_FLG &= ~M32C_FLG_CARRY;
			}
			sDst = (sDst >> sha);
			sha_flags(sDst, size);
			M32C_Write8(sDst, DstP);
		}
		M32C_REG_FLG &= ~M32C_FLG_OVERFLOW;
	} else {
		if (size == 2) {
			sDst = (int32_t) (int16_t) M32C_Read16(DstP);
			sDst = (sDst << sha);
			if (sDst & (1 << 16)) {
				M32C_REG_FLG |= M32C_FLG_CARRY;
			} else {
				M32C_REG_FLG &= ~M32C_FLG_CARRY;
			}
			if (((sDst & UINT32_C(0xFFFF8000))
			     == UINT32_C(0xFFFF8000))
			    || ((sDst & UINT32_C(0xFFFF8000)) == 0)) {
				M32C_REG_FLG &= ~M32C_FLG_OVERFLOW;
			} else {
				M32C_REG_FLG |= M32C_FLG_OVERFLOW;
			}
			sha_flags(sDst, size);
			M32C_Write16(sDst, DstP);
		} else {
			sDst = (int32_t) (int8_t) M32C_Read8(DstP);
			sDst = (sDst << sha);
			if (sDst & (1 << 8)) {
				M32C_REG_FLG |= M32C_FLG_CARRY;
			} else {
				M32C_REG_FLG &= ~M32C_FLG_CARRY;
			}
			if (((sDst & UINT32_C(0xFFFFFF80))
			     == UINT32_C(0xFFFFFF80))
			    || ((sDst & UINT32_C(0xFFFFFF80)) == 0)) {
				M32C_REG_FLG &= ~M32C_FLG_OVERFLOW;
			} else {
				M32C_REG_FLG |= M32C_FLG_OVERFLOW;
			}
			sha_flags(sDst, size);
			M32C_Write8(sDst, DstP);
		}
	}
	M32C_REG_PC += codelen_dst;
	dbgprintf("m32c_sha_size_immidst not tested\n");
}

void
m32c_setup_sha_size_immidst(void)
{
	int sha = (ICODE24() & 7) + 1;
	int right = ICODE24() & 8;
	int size;
	int dst;
	int codelen_dst;
	dst = ((ICODE24() >> 6) & 3) | ((ICODE24() >> 7) & 0x1c);
	if (ICODE24() & 0x100) {
		size = 2;
	} else {
		size = 1;
	}
	INSTR->getdstp = general_am_get(dst, 4, &codelen_dst, GAM_ALL);
	INSTR->codelen_dst = codelen_dst;
	INSTR->opsize = size;
	INSTR->Arg1 = sha;
	INSTR->Arg2 = right;
	INSTR->proc = m32c_sha_size_immidst;
	INSTR->proc();
}

/**
 ******************************************************************
 * \fn void m32c_sha_l_immdst(void)
 * Shift a long destination left or right by an 8 Bit immediate. 
 ******************************************************************
 */
static void
m32c_sha_l_immdst(void)
{
	int8_t imm8;
	int sha;
	int right;
	int size = 4;
	uint32_t u32Dst;
	int64_t Dst;
	int codelen_dst = INSTR->codelen_dst;
	INSTR->getdst(&u32Dst, M32C_INDEXLD());
	Dst = (int64_t) (int32_t) u32Dst;
	imm8 = M32C_Read8(M32C_REG_PC + codelen_dst);
	sha = abs(imm8);
	right = imm8 & 0x80;
	if (right) {
		if (Dst & (UINT64_C(1) << (sha - 1))) {
			M32C_REG_FLG |= M32C_FLG_CARRY;
		} else {
			M32C_REG_FLG &= ~M32C_FLG_CARRY;
		}
		Dst = (Dst >> sha);
		M32C_REG_FLG &= ~M32C_FLG_OVERFLOW;
		sha_flags(Dst, size);
		INSTR->setdst(Dst, M32C_INDEXSD());
	} else {
		Dst = (Dst << sha);
		if (Dst & (UINT64_C(1) << 32)) {
			M32C_REG_FLG |= M32C_FLG_CARRY;
		} else {
			M32C_REG_FLG &= ~M32C_FLG_CARRY;
		}
		if (((Dst & UINT64_C(0xFFFFFFFF80000000))
		     == UINT64_C(0xFFFFFFFF80000000))
		    || ((Dst & UINT64_C(0xFFFFFFFF80000000)) == 0)) {
			M32C_REG_FLG &= ~M32C_FLG_OVERFLOW;
		} else {
			M32C_REG_FLG |= M32C_FLG_OVERFLOW;
		}
		sha_flags(Dst, size);
		INSTR->setdst(Dst, M32C_INDEXSD());
	}
	M32C_REG_PC += codelen_dst + 1;
	dbgprintf("m32c_sha_l_immdst not tested\n");
}

void
m32c_setup_sha_l_immdst(void)
{
	int size = 4;
	int dst;
	int codelen_dst;
	dst = ((ICODE16() >> 6) & 3) | ((ICODE16() >> 7) & 0x1c);
	INSTR->getdst = general_am_get(dst, size, &codelen_dst, GAM_ALL);
	INSTR->setdst = general_am_set(dst, size, &codelen_dst, GAM_ALL);
	INSTR->codelen_dst = codelen_dst;
	INSTR->proc = m32c_sha_l_immdst;
	INSTR->proc();
}

/**
 ******************************************************************
 * \fn void m32c_sha_l_immidst(void)
 * Shift a long indirect destination left or right by an 
 * 8 Bit immediate. 
 * v0
 ******************************************************************
 */
static void
m32c_sha_l_immidst(void)
{
	int8_t imm8;
	int sha;
	int right;
	int size = 4;
	uint32_t DstP;
	int64_t Dst;
	int codelen_dst = INSTR->codelen_dst;
	INSTR->getdstp(&DstP, 0);
	DstP = (DstP + M32C_INDEXLD()) & 0xffffff;
	Dst = (int64_t) (int32_t) M32C_Read32(DstP);
	imm8 = M32C_Read8(M32C_REG_PC + codelen_dst);
	sha = abs(imm8);
	right = imm8 & 0x80;
	if (right) {
		if (Dst & (UINT64_C(1) << (sha - 1))) {
			M32C_REG_FLG |= M32C_FLG_CARRY;
		} else {
			M32C_REG_FLG &= ~M32C_FLG_CARRY;
		}
		Dst = (Dst >> sha);
		M32C_REG_FLG &= ~M32C_FLG_OVERFLOW;
	} else {
		Dst = (Dst << sha);
		if (Dst & (UINT64_C(1) << 32)) {
			M32C_REG_FLG |= M32C_FLG_CARRY;
		} else {
			M32C_REG_FLG &= ~M32C_FLG_CARRY;
		}
		if (((Dst & UINT64_C(0xFFFFFFFF80000000))
		     == UINT64_C(0xFFFFFFFF80000000))
		    || ((Dst & UINT64_C(0xFFFFFFFF80000000)) == 0)) {
			M32C_REG_FLG &= ~M32C_FLG_OVERFLOW;
		} else {
			M32C_REG_FLG |= M32C_FLG_OVERFLOW;
		}
	}
	sha_flags(Dst, size);
	M32C_Write32(Dst, DstP);
	M32C_REG_PC += codelen_dst + 1;
	dbgprintf("m32c_sha_l_immidst not tested\n");
}

void
m32c_setup_sha_l_immidst(void)
{
	int dst;
	int codelen_dst;
	dst = ((ICODE24() >> 6) & 3) | ((ICODE24() >> 7) & 0x1c);
	INSTR->getdstp = general_am_get(dst, 4, &codelen_dst, GAM_ALL);
	INSTR->codelen_dst = codelen_dst;
	INSTR->proc = m32c_sha_l_immidst;
	INSTR->proc();
}

/**
 **************************************************************
 * \fn void m32c_sha_size_r1hdst(void)
 * Shift arithmetic left or right a destination by r1h.
 * v0
 **************************************************************
 */
static void
m32c_sha_size_r1hdst(void)
{
	int opsize = INSTR->opsize;
	int8_t r1h = M32C_REG_R1H;
	int sha = abs(r1h);
	int right = (r1h < 0);
	uint32_t u32Dst;
	int64_t Dst;
	int codelen_dst = INSTR->codelen_dst;
	INSTR->getdst(&u32Dst, M32C_INDEXSD());
	if (sha == 0) {
		/* When the shift is 0 no flags are changed */
	} else if (right) {
		if (opsize == 2) {
			Dst = (int64_t) (int16_t) u32Dst;
			if (Dst & (1 << (sha - 1))) {
				M32C_REG_FLG |= M32C_FLG_CARRY;
			} else {
				M32C_REG_FLG &= ~M32C_FLG_CARRY;
			}
			Dst = (Dst >> sha);
		} else {
			Dst = (int64_t) (int8_t) u32Dst;
			if (Dst & (1 << (sha - 1))) {
				M32C_REG_FLG |= M32C_FLG_CARRY;
			} else {
				M32C_REG_FLG &= ~M32C_FLG_CARRY;
			}
			Dst = (Dst >> sha);
		}
		INSTR->setdst(Dst, M32C_INDEXSD());
		sha_flags(Dst, opsize);
		M32C_REG_FLG &= ~M32C_FLG_OVERFLOW;
	} else {
		if (opsize == 2) {
			Dst = (int64_t) (int16_t) u32Dst;
			Dst = (Dst << sha);
			if (((Dst & UINT64_C(0xFFFFFFFF8000))
			     == UINT64_C(0xFFFFFFFF8000))
			    || ((Dst & UINT64_C(0xFFFFFFFF8000)) == 0)) {
				M32C_REG_FLG &= ~M32C_FLG_OVERFLOW;
			} else {
				M32C_REG_FLG |= M32C_FLG_OVERFLOW;
			}
			if (Dst & (1 << 16)) {
				M32C_REG_FLG |= M32C_FLG_CARRY;
			} else {
				M32C_REG_FLG &= ~M32C_FLG_CARRY;
			}
		} else {
			Dst = (int64_t) (int8_t) u32Dst;
			Dst = (Dst << sha);
			if (((Dst & UINT64_C(0xFFFFFFFF80))
			     == UINT64_C(0xFFFFFFFF80))
			    || ((Dst & UINT64_C(0xFFFFFFFF80)) == 0)) {
				M32C_REG_FLG &= ~M32C_FLG_OVERFLOW;
			} else {
				M32C_REG_FLG |= M32C_FLG_OVERFLOW;
			}
			if (Dst & (1 << 8)) {
				M32C_REG_FLG |= M32C_FLG_CARRY;
			} else {
				M32C_REG_FLG &= ~M32C_FLG_CARRY;
			}
		}
		INSTR->setdst(Dst, M32C_INDEXSD());
		sha_flags(Dst, opsize);
	}
	M32C_REG_PC += codelen_dst;
	dbgprintf("m32c_sha_size_r1hdst not tested\n");
}

void
m32c_setup_sha_size_r1hdst(void)
{
	int size, opsize;
	int dst;
	int codelen_dst;
	dst = ((ICODE16() >> 6) & 3) | ((ICODE16() >> 7) & 0x1c);
	if (ICODE16() & 0x100) {
		opsize = size = 2;
	} else {
		opsize = size = 1;
		ModOpsize(dst, &opsize);
	}
	INSTR->getdst = general_am_get(dst, size, &codelen_dst, GAM_ALL);
	INSTR->setdst = general_am_set(dst, opsize, &codelen_dst, GAM_ALL);
	INSTR->codelen_dst = codelen_dst;
	INSTR->opsize = opsize;
	INSTR->srcsize = size;
	INSTR->proc = m32c_sha_size_r1hdst;
	INSTR->proc();
}

/**
 *****************************************************************
 * \fn void m32c_sha_size_r1hidst(void)
 * Shift arithmetic left or right an indirect destination by r1h.
 * v0
 ******************************************************************
 */
static void
m32c_sha_size_r1hidst(void)
{
	int size = INSTR->opsize;
	int8_t r1h = M32C_REG_R1H;
	int sha = abs(r1h);
	int right = (r1h < 0);
	uint32_t DstP;
	int64_t Dst;
	int codelen_dst = INSTR->codelen_dst;
	INSTR->getdstp(&DstP, 0);
	DstP = (DstP + M32C_INDEXSD());
	if (sha == 0) {
		/* In case of zero shift flags are not modified */
	} else if (right) {
		if (size == 2) {
			Dst = (int64_t) (int16_t) M32C_Read16(DstP);
			if (Dst & (1 << (sha - 1))) {
				M32C_REG_FLG |= M32C_FLG_CARRY;
			} else {
				M32C_REG_FLG &= ~M32C_FLG_CARRY;
			}
			Dst = (Dst >> sha);
			M32C_Write16(Dst, DstP);
			sha_flags(Dst, size);
		} else {
			Dst = (int64_t) (int8_t) M32C_Read8(DstP);
			if (Dst & (1 << (sha - 1))) {
				M32C_REG_FLG |= M32C_FLG_CARRY;
			} else {
				M32C_REG_FLG &= ~M32C_FLG_CARRY;
			}
			Dst = (Dst >> sha);
			M32C_Write8(Dst, DstP);
			sha_flags(Dst, size);
		}
		M32C_REG_FLG &= ~M32C_FLG_OVERFLOW;
	} else {
		if (size == 2) {
			Dst = (int64_t) (int32_t) M32C_Read16(DstP);
			Dst = (Dst << sha);
			if (((Dst & UINT64_C(0xFFFFFFFF8000))
			     == UINT64_C(0xFFFFFFFF8000))
			    || ((Dst & UINT64_C(0xFFFFFFFF8000)) == 0)) {
				M32C_REG_FLG &= ~M32C_FLG_OVERFLOW;
			} else {
				M32C_REG_FLG |= M32C_FLG_OVERFLOW;
			}
			if (Dst & (1 << 16)) {
				M32C_REG_FLG |= M32C_FLG_CARRY;
			} else {
				M32C_REG_FLG &= ~M32C_FLG_CARRY;
			}
			M32C_Write16(Dst, DstP);
			sha_flags(Dst, size);
		} else {
			Dst = (int64_t) (int8_t) M32C_Read8(DstP);
			Dst = (Dst << sha);
			if (((Dst & UINT64_C(0xFFFFFFFF80))
			     == UINT64_C(0xFFFFFFFF80))
			    || ((Dst & UINT64_C(0xFFFFFFFF80)) == 0)) {
				M32C_REG_FLG &= ~M32C_FLG_OVERFLOW;
			} else {
				M32C_REG_FLG |= M32C_FLG_OVERFLOW;
			}
			if (Dst & (1 << 8)) {
				M32C_REG_FLG |= M32C_FLG_CARRY;
			} else {
				M32C_REG_FLG &= ~M32C_FLG_CARRY;
			}
			M32C_Write8(Dst, DstP);
			sha_flags(Dst, size);
		}
	}
	M32C_REG_PC += codelen_dst;
	dbgprintf("m32c_sha_size_r1hidst not tested\n");
}

void
m32c_setup_sha_size_r1hidst(void)
{
	int size;
	int dst;
	int codelen_dst;
	dst = ((ICODE24() >> 6) & 3) | ((ICODE24() >> 7) & 0x1c);
	if (ICODE24() & 0x100) {
		size = 2;
	} else {
		size = 1;
	}
	INSTR->getdstp = general_am_get(dst, 4, &codelen_dst, GAM_ALL);
	INSTR->codelen_dst = codelen_dst;
	INSTR->opsize = size;
	INSTR->proc = m32c_sha_size_r1hidst;
	INSTR->proc();
}

/**
 *******************************************************************
 * \fn void m32c_sha_l_r1hdst(void)
 * Shift a long arithmetic left/right by value in register r1h
 * v0
 *******************************************************************
 */
static void
m32c_sha_l_r1hdst(void)
{
	int size = 4;
	int8_t r1h = M32C_REG_R1H;
	int sha = abs(r1h);
	int right = (r1h < 0);
	int64_t Dst;
	uint32_t u32Dst;
	int codelen_dst = INSTR->codelen_dst;
	INSTR->getdst(&u32Dst, M32C_INDEXLD());
	Dst = (int64_t) (int32_t) u32Dst;
	if (sha == 0) {
		/* In case of zero shift don't change flags */
	} else if (right) {
		if (Dst & (1 << (sha - 1))) {
			M32C_REG_FLG |= M32C_FLG_CARRY;
		} else {
			M32C_REG_FLG &= ~M32C_FLG_CARRY;
		}
		Dst = Dst >> sha;
		M32C_REG_FLG &= ~M32C_FLG_OVERFLOW;
		sha_flags(Dst, size);
		INSTR->setdst(Dst, M32C_INDEXLD());
	} else {
		Dst = Dst << sha;
		if (((Dst & UINT64_C(0xFFFFFFFF80000000)) == UINT64_C(0xFFFFFFFF80000000))
		    || ((Dst & UINT64_C(0xFFFFFFFF80000000)) == 0)) {
			M32C_REG_FLG &= ~M32C_FLG_OVERFLOW;
		} else {
			M32C_REG_FLG |= M32C_FLG_OVERFLOW;
		}
		if (Dst & (UINT64_C(1) << 32)) {
			M32C_REG_FLG |= M32C_FLG_CARRY;
		} else {
			M32C_REG_FLG &= ~M32C_FLG_CARRY;
		}
		sha_flags(Dst, size);
		INSTR->setdst(Dst, M32C_INDEXLD());
	}
	M32C_REG_PC += codelen_dst;
	dbgprintf("m32c_sha_l_r1hdst not tested\n");
}

void
m32c_setup_sha_l_r1hdst(void)
{
	int size = 4;
	int dst;
	int codelen_dst;
	dst = ((ICODE16() >> 6) & 3) | ((ICODE16() >> 7) & 0x1c);
	INSTR->getdst = general_am_get(dst, size, &codelen_dst, GAM_ALL);
	INSTR->setdst = general_am_set(dst, size, &codelen_dst, GAM_ALL);
	INSTR->codelen_dst = codelen_dst;
	INSTR->proc = m32c_sha_l_r1hdst;
	INSTR->proc();
}

/**
 *******************************************************************
 * \fn void m32c_sha_l_r1hidst(void)
 * Shift a long indirect destination arithmetic left/right 
 * by value in register r1h
 * v0
 *******************************************************************
 */
static void
m32c_sha_l_r1hidst(void)
{
	int size = 4;
	int8_t r1h = M32C_REG_R1H;
	int sha = abs(r1h);
	int right = (r1h < 0);
	int64_t Dst;
	uint32_t DstP;
	int codelen_dst = INSTR->codelen_dst;
	INSTR->getdst(&DstP, 0);
	DstP = (DstP + M32C_INDEXLD()) & 0xffffff;
	Dst = (int64_t) (int32_t) M32C_Read32(DstP);
	if (sha == 0) {
		/* In case of zero shift don't change flags */
	} else if (right) {
		if (Dst & (1 << (sha - 1))) {
			M32C_REG_FLG |= M32C_FLG_CARRY;
		} else {
			M32C_REG_FLG &= ~M32C_FLG_CARRY;
		}
		Dst = Dst >> sha;
		M32C_REG_FLG &= ~M32C_FLG_OVERFLOW;
		sha_flags(Dst, size);
		M32C_Write32(Dst, DstP);
	} else {
		Dst = Dst << sha;
		if (((Dst & UINT64_C(0xFFFFFFFF80000000))
		     == UINT64_C(0xFFFFFFFF80000000))
		    || ((Dst & UINT64_C(0xFFFFFFFF80000000)) == 0)) {
			M32C_REG_FLG &= ~M32C_FLG_OVERFLOW;
		} else {
			M32C_REG_FLG |= M32C_FLG_OVERFLOW;
		}
		if (Dst & (UINT64_C(1) << 32)) {
			M32C_REG_FLG |= M32C_FLG_CARRY;
		} else {
			M32C_REG_FLG &= ~M32C_FLG_CARRY;
		}
		sha_flags(Dst, size);
		M32C_Write32(Dst, DstP);
	}
	M32C_REG_PC += codelen_dst;
	dbgprintf("m32c_sha_l_r1hidst not tested\n");
}

void
m32c_setup_sha_l_r1hidst(void)
{
	int size = 4;
	int dst;
	int codelen_dst;
	dst = ((ICODE24() >> 6) & 3) | ((ICODE24() >> 7) & 0x1c);
	INSTR->getdst = general_am_get(dst, size, &codelen_dst, GAM_ALL);
	INSTR->codelen_dst = codelen_dst;
	INSTR->proc = m32c_sha_l_r1hidst;
	INSTR->proc();
}

/**
 *****************************************************************
 * \fn void m32c_shanc_l_immdst(void)
 * Shift aritmethic left/right by an immediate without carry.
 * v0
 *****************************************************************
 */
static void
m32c_shanc_l_immdst(void)
{
	int8_t imm8;
	int sha;
	int right;
	int size = 4;
	uint32_t tmpDst;
	int64_t Dst;
	int codelen_dst = INSTR->codelen_dst;
	INSTR->getdst(&tmpDst, 0);
	Dst = (int64_t) (int32_t) tmpDst;
	imm8 = M32C_Read8(M32C_REG_PC + codelen_dst);
	sha = abs(imm8);
	right = imm8 & 0x80;
	if (right) {
		Dst = (Dst >> sha);
	} else {
		Dst = (Dst << sha);
	}
	sha_flags(Dst, size);
	INSTR->setdst(Dst, 0);
	M32C_REG_PC += codelen_dst + 1;
	dbgprintf("m32c_shanc_l_immdst not tested\n");
}

void
m32c_setup_shanc_l_immdst(void)
{
	int size = 4;
	int dst;
	int codelen_dst;
	dst = ((ICODE16() >> 6) & 3) | ((ICODE16() >> 7) & 0x1c);
	INSTR->getdst = general_am_get(dst, size, &codelen_dst, GAM_ALL);
	INSTR->setdst = general_am_set(dst, size, &codelen_dst, GAM_ALL);
	INSTR->codelen_dst = codelen_dst;
	if (INSTR->nrMemAcc == 1) {
		INSTR->cycles = 7;
	}
	INSTR->proc = m32c_shanc_l_immdst;
	INSTR->proc();
}

/**
 *****************************************************************
 * \fn void m32c_shanc_l_immdst(void)
 * Shift a long aritmethic left/right by an immediate without carry.
 * v0
 *****************************************************************
 */
static void
m32c_shanc_l_immidst(void)
{
	int8_t imm8;
	int sha;
	int right;
	int size = 4;
	uint32_t DstP;
	int64_t Dst;		/* Must be 64 bit because Shift 32 is legal */
	int codelen_dst = INSTR->codelen_dst;
	INSTR->getdstp(&DstP, 0);
	Dst = (int64_t) (int32_t) M32C_Read32(DstP);
	imm8 = M32C_Read8(M32C_REG_PC + codelen_dst);
	sha = abs(imm8);
	right = imm8 & 0x80;
	if (right) {
		Dst = (Dst >> sha);
	} else {
		Dst = (Dst << sha);
	}
	sha_flags(Dst, size);
	M32C_Write32(Dst, DstP);
	M32C_REG_PC += codelen_dst + 1;
	dbgprintf("m32c_shanc_l_immidst not tested\n");
}

void
m32c_setup_shanc_l_immidst(void)
{
	int dst;
	int codelen_dst;
	dst = ((ICODE24() >> 6) & 3) | ((ICODE24() >> 7) & 0x1c);
	INSTR->getdstp = general_am_get(dst, 4, &codelen_dst, GAM_ALL);
	INSTR->codelen_dst = codelen_dst;
	INSTR->proc = m32c_shanc_l_immidst;
	if (INSTR->nrMemAcc == 1) {
		INSTR->cycles = 10;
	}
	INSTR->proc();
}

/**
 **************************************************************
 * \fn void m32c_shl_size_immdst(void) 
 * Shift logical left/right by an immediate.
 * v0
 **************************************************************
 */
static void
m32c_shl_size_immdst(void)
{
	int shl = INSTR->Arg1;
	int right = INSTR->Arg2;
	int opsize = INSTR->opsize;
	uint32_t Dst;
	int codelen_dst = INSTR->codelen_dst;
	INSTR->getdst(&Dst, M32C_INDEXSD());
	if (right) {
		if (Dst & (1 << (shl - 1))) {
			M32C_REG_FLG |= M32C_FLG_CARRY;
		} else {
			M32C_REG_FLG &= ~M32C_FLG_CARRY;
		}
		Dst = Dst >> shl;
	} else {
		Dst = Dst << shl;
		if (opsize == 2) {
			if (Dst & (1 << 16)) {
				M32C_REG_FLG |= M32C_FLG_CARRY;
			} else {
				M32C_REG_FLG &= ~M32C_FLG_CARRY;
			}
			Dst = Dst & 0xffff;
		} else {
			if (Dst & (1 << 8)) {
				M32C_REG_FLG |= M32C_FLG_CARRY;
			} else {
				M32C_REG_FLG &= ~M32C_FLG_CARRY;
			}
			Dst = Dst & 0xff;
		}
	}
	shl_flags(Dst, opsize);
	INSTR->setdst(Dst, M32C_INDEXSD());
	M32C_REG_PC += codelen_dst;

}

void
m32c_setup_shl_size_immdst(void)
{
	int shl = (ICODE16() & 7) + 1;
	int right = ICODE16() & 8;
	int size, opsize;
	int dst;
	int codelen_dst;
	dst = ((ICODE16() >> 6) & 3) | ((ICODE16() >> 7) & 0x1c);
	if (ICODE16() & 0x100) {
		opsize = size = 2;
	} else {
		opsize = size = 1;
		ModOpsize(dst, &opsize);
	}
	INSTR->getdst = general_am_get(dst, size, &codelen_dst, GAM_ALL);
	INSTR->setdst = general_am_set(dst, opsize, &codelen_dst, GAM_ALL);
	INSTR->codelen_dst = codelen_dst;
	INSTR->opsize = opsize;
	INSTR->srcsize = size;
	INSTR->Arg1 = shl;
	INSTR->Arg2 = right;
	INSTR->proc = m32c_shl_size_immdst;
	INSTR->proc();
}

/**
 ********************************************************
 * \fn void m32c_shl_size_immidst(void) 
 * Shift logical left/right an indirect destination by 
 * an immediate.
 * v0
 ********************************************************
 */
static void
m32c_shl_size_immidst(void)
{
	int shl = INSTR->Arg1;
	int right = INSTR->Arg2;
	int size = INSTR->opsize;
	int codelen_dst = INSTR->codelen_dst;
	uint32_t Dst;
	uint32_t DstP;
	INSTR->getdstp(&DstP, 0);
	DstP = (DstP + M32C_INDEXSD()) & 0xffffff;
	if (size == 2) {
		Dst = M32C_Read16(DstP);
	} else {
		Dst = M32C_Read8(DstP);
	}
	if (right) {
		if (Dst & (1 << (shl - 1))) {
			M32C_REG_FLG |= M32C_FLG_CARRY;
		} else {
			M32C_REG_FLG &= ~M32C_FLG_CARRY;
		}
		Dst = Dst >> shl;
	} else {
		Dst = Dst << shl;
		if (size == 2) {
			if (Dst & (1 << 16)) {
				M32C_REG_FLG |= M32C_FLG_CARRY;
			} else {
				M32C_REG_FLG &= ~M32C_FLG_CARRY;
			}
		} else {
			if (Dst & (1 << 8)) {
				M32C_REG_FLG |= M32C_FLG_CARRY;
			} else {
				M32C_REG_FLG &= ~M32C_FLG_CARRY;
			}
		}
	}
	shl_flags(Dst, size);
	if (size == 2) {
		M32C_Write16(Dst, DstP);
	} else {
		M32C_Write8(Dst, DstP);
	}
	M32C_REG_PC += codelen_dst;
}

void
m32c_setup_shl_size_immidst(void)
{
	int shl = (ICODE24() & 7) + 1;
	int right = ICODE24() & 8;
	int size;
	int dst;
	int codelen_dst;
	dst = ((ICODE24() >> 6) & 3) | ((ICODE24() >> 7) & 0x1c);
	if (ICODE24() & 0x100) {
		size = 2;
	} else {
		size = 1;
	}
	INSTR->getdstp = general_am_get(dst, 4, &codelen_dst, GAM_ALL);
	INSTR->codelen_dst = codelen_dst;
	INSTR->opsize = size;
	INSTR->Arg1 = shl;
	INSTR->Arg2 = right;
	INSTR->proc = m32c_shl_size_immidst;
	INSTR->proc();
}

/**
 ************************************************************************
 * \fn void m32c_shl_l_immdst(void)
 * Shift a long logically by an immediate to the left or to the right.
 * v0
 ************************************************************************
 */
static void
m32c_shl_l_immdst(void)
{
	int8_t imm8;
	int shl;
	int right;
	int size = 4;
	uint32_t u32Dst;
	uint64_t Dst;
	int codelen_dst = INSTR->codelen_dst;
	INSTR->getdst(&u32Dst, M32C_INDEXLD());
	Dst = u32Dst;
	imm8 = M32C_Read8(M32C_REG_PC + codelen_dst);
	shl = abs(imm8);
	right = (imm8 & 0x80);
	if (right) {
		if (Dst & (1 << (shl - 1))) {
			M32C_REG_FLG |= M32C_FLG_CARRY;
		} else {
			M32C_REG_FLG &= ~M32C_FLG_CARRY;
		}
		Dst = Dst >> shl;
	} else {
		Dst = Dst << shl;
		if (Dst & (UINT64_C(1) << 32)) {
			M32C_REG_FLG |= M32C_FLG_CARRY;
		} else {
			M32C_REG_FLG &= ~M32C_FLG_CARRY;
		}
	}
	shl_flags(Dst, size);
	INSTR->setdst(Dst, M32C_INDEXLD());
	M32C_REG_PC += codelen_dst + 1;
	dbgprintf("m32c_shl_l_immdst not tested\n");
}

void
m32c_setup_shl_l_immdst(void)
{
	int size = 4;
	int dst;
	int codelen_dst;
	dst = ((ICODE16() >> 6) & 3) | ((ICODE16() >> 7) & 0x1c);
	INSTR->getdst = general_am_get(dst, size, &codelen_dst, GAM_ALL);
	INSTR->setdst = general_am_set(dst, size, &codelen_dst, GAM_ALL);
	INSTR->codelen_dst = codelen_dst;
	INSTR->proc = m32c_shl_l_immdst;
	INSTR->proc();
}

/**
 ************************************************************************
 * \fn void m32c_shl_l_immidst(void)
 * Shift a long indirect destination logically by an immediate 
 * to the left or to the right.
 * v0
 ************************************************************************
 */
static void
m32c_shl_l_immidst(void)
{
	int8_t imm8;
	int shl;
	int right;
	int size = 4;
	uint64_t Dst;
	uint32_t DstP;
	int codelen_dst = INSTR->codelen_dst;
	INSTR->getdstp(&DstP, 0);
	DstP = (DstP + M32C_INDEXLD()) & 0xffffff;
	Dst = M32C_Read32(DstP);
	imm8 = M32C_Read8(M32C_REG_PC + codelen_dst);
	shl = abs(imm8);
	right = (imm8 & 0x80);
	if (right) {
		if (Dst & (1 << (shl - 1))) {
			M32C_REG_FLG |= M32C_FLG_CARRY;
		} else {
			M32C_REG_FLG &= ~M32C_FLG_CARRY;
		}
		Dst = Dst >> shl;
	} else {
		Dst = Dst << shl;
		if (Dst & (UINT64_C(1) << 32)) {
			M32C_REG_FLG |= M32C_FLG_CARRY;
		} else {
			M32C_REG_FLG &= ~M32C_FLG_CARRY;
		}
	}
	shl_flags(Dst, size);
	M32C_Write32(Dst, DstP);
	M32C_REG_PC += codelen_dst + 1;
	dbgprintf("m32c_shl_l_immidst not tested\n");
}

void
m32c_setup_shl_l_immidst(void)
{
	int dst;
	int codelen_dst;
	dst = ((ICODE24() >> 6) & 3) | ((ICODE24() >> 7) & 0x1c);
	INSTR->getdstp = general_am_get(dst, 4, &codelen_dst, GAM_ALL);
	INSTR->codelen_dst = codelen_dst;
	INSTR->proc = m32c_shl_l_immidst;
	INSTR->proc();
}

/*
 ************************************************************
 * \fn void m32c_shl_size_r1hdst(void)
 * Shift left a destination by the value in register R1.
 * v0
 ************************************************************
 */
static void
m32c_shl_size_r1hdst(void)
{
	int8_t r1h = M32C_REG_R1H;
	int shl = abs(r1h);
	int right = M32C_REG_R1H & 0x80;
	int size = INSTR->srcsize;
	int opsize = INSTR->opsize;
	uint32_t u32Dst;
	uint64_t Dst;
	int codelen_dst = INSTR->codelen_dst;
	INSTR->getdst(&u32Dst, M32C_INDEXSD());
	Dst = u32Dst;
	if (shl == 0) {
		/* In case of zero shift don't change the flags. */
	} else if (right) {
		if (Dst & (1 << (shl - 1))) {
			M32C_REG_FLG |= M32C_FLG_CARRY;
		} else {
			M32C_REG_FLG &= ~M32C_FLG_CARRY;
		}
		Dst = Dst >> shl;
		if (size == 2) {
			Dst = Dst & 0xffff;
		} else {
			Dst = Dst & 0xff;
		}
		shl_flags(Dst, opsize);
		INSTR->setdst(Dst, M32C_INDEXSD());
	} else {
		Dst = Dst << shl;
		if (opsize == 2) {
			if (Dst & (1 << 16)) {
				M32C_REG_FLG |= M32C_FLG_CARRY;
			} else {
				M32C_REG_FLG &= ~M32C_FLG_CARRY;
			}
			Dst = Dst & 0xffff;
		} else {
			if (Dst & (1 << 8)) {
				M32C_REG_FLG |= M32C_FLG_CARRY;
			} else {
				M32C_REG_FLG &= ~M32C_FLG_CARRY;
			}
			Dst = Dst & 0xff;
		}
		shl_flags(Dst, opsize);
		INSTR->setdst(Dst, M32C_INDEXSD());
	}
	M32C_REG_PC += codelen_dst;
	dbgprintf("m32c_shl_size_r1hdst not tested\n");
}

void
m32c_setup_shl_size_r1hdst(void)
{
	int size;
	int opsize;
	int dst;
	int codelen_dst;
	dst = ((ICODE16() >> 6) & 3) | ((ICODE16() >> 7) & 0x1c);
	if (ICODE16() & 0x100) {
		opsize = size = 2;
	} else {
		opsize = size = 1;
		ModOpsize(dst, &opsize);
	}
	INSTR->getdst = general_am_get(dst, size, &codelen_dst, GAM_ALL);
	INSTR->setdst = general_am_set(dst, opsize, &codelen_dst, GAM_ALL);
	INSTR->codelen_dst = codelen_dst;
	INSTR->opsize = opsize;
	INSTR->srcsize = size;
	INSTR->proc = m32c_shl_size_r1hdst;
	INSTR->proc();
}

/*
 ************************************************************
 * \fn void m32c_shl_size_r1hidst(void)
 * Shift left an indirect destination by the value in 
 * register r1h.
 * v0
 ************************************************************
 */
static void
m32c_shl_size_r1hidst(void)
{
	int8_t r1h = M32C_REG_R1H;
	int shl = abs(r1h);
	int right = M32C_REG_R1H & 0x80;
	int size = INSTR->opsize;
	int codelen_dst = INSTR->codelen_dst;
	uint64_t Dst;
	uint32_t DstP;
	INSTR->getdstp(&DstP, 0);
	DstP = (DstP + M32C_INDEXSD()) & 0xffffff;
	/* Is this also read if shift amount == 0 ? */
	if (size == 2) {
		Dst = M32C_Read16(DstP);
	} else {
		Dst = M32C_Read8(DstP);
	}
	if (shl == 0) {
		/* For zero shift flags are not changed */
	} else if (right) {
		if (Dst & (1 << (shl - 1))) {
			M32C_REG_FLG |= M32C_FLG_CARRY;
		} else {
			M32C_REG_FLG &= ~M32C_FLG_CARRY;
		}
		Dst = Dst >> shl;
		shl_flags(Dst, size);
	} else {
		Dst = Dst << shl;
		if (size == 2) {
			if (Dst & (1 << 16)) {
				M32C_REG_FLG |= M32C_FLG_CARRY;
			} else {
				M32C_REG_FLG &= ~M32C_FLG_CARRY;
			}
			Dst = Dst & 0xffff;
		} else {
			if (Dst & (1 << 8)) {
				M32C_REG_FLG |= M32C_FLG_CARRY;
			} else {
				M32C_REG_FLG &= ~M32C_FLG_CARRY;
			}
			Dst = Dst & 0xff;
		}
		shl_flags(Dst, size);
	}
	/* Is this also done if shift amount == 0 ? */
	if (size == 2) {
		M32C_Write16(Dst, DstP);
	} else {
		M32C_Write8(Dst, DstP);
	}
	M32C_REG_PC += codelen_dst;
	dbgprintf("m32c_shl_size_r1hidst not tested\n");
}

void
m32c_setup_shl_size_r1hidst(void)
{
	int size;
	int dst;
	int codelen_dst;
	dst = ((ICODE24() >> 6) & 3) | ((ICODE24() >> 7) & 0x1c);
	if (ICODE24() & 0x100) {
		size = 2;
	} else {
		size = 1;
	}
	INSTR->getdstp = general_am_get(dst, 4, &codelen_dst, GAM_ALL);
	INSTR->codelen_dst = codelen_dst;
	INSTR->opsize = size;
	INSTR->proc = m32c_shl_size_r1hidst;
	INSTR->proc();
}

/*
 ************************************************************
 * \fn void m32c_shl_size_r1hdst(void)
 * Shift left a long destination by the value in 
 * register R1.
 * v0
 ************************************************************
 */
static void
m32c_shl_l_r1hdst(void)
{
	int8_t r1h = M32C_REG_R1H;
	int shl = abs(r1h);
	int right = r1h & 0x80;
	int size = 4;
	uint32_t u32Dst;
	uint64_t Dst;
	int codelen_dst = INSTR->codelen_dst;
	INSTR->getdst(&u32Dst, M32C_INDEXLD());
	Dst = u32Dst;
	if (shl == 0) {
		/* Flags don't change if shift is zero */
	} else if (right) {
		if (Dst & (1 << (shl - 1))) {
			M32C_REG_FLG |= M32C_FLG_CARRY;
		} else {
			M32C_REG_FLG &= ~M32C_FLG_CARRY;
		}
		Dst = Dst >> shl;
		shl_flags(Dst, size);
		INSTR->setdst(Dst, M32C_INDEXLD());
	} else {
		Dst = Dst << shl;
		if (Dst & (UINT64_C(1) << 32)) {
			M32C_REG_FLG |= M32C_FLG_CARRY;
		} else {
			M32C_REG_FLG &= ~M32C_FLG_CARRY;
		}
		shl_flags(Dst, size);
		INSTR->setdst(Dst, M32C_INDEXLD());
	}
	M32C_REG_PC += codelen_dst;
	dbgprintf("m32c_shl_l_r1hdst not tested\n");
}

void
m32c_setup_shl_l_r1hdst(void)
{
	int size = 4;
	int dst;
	int codelen_dst;
	dst = ((ICODE16() >> 6) & 3) | ((ICODE16() >> 7) & 0x1c);
	INSTR->getdst = general_am_get(dst, size, &codelen_dst, GAM_ALL);
	INSTR->setdst = general_am_set(dst, size, &codelen_dst, GAM_ALL);
	INSTR->codelen_dst = codelen_dst;
	INSTR->proc = m32c_shl_l_r1hdst;
	INSTR->proc();
}

/*
 ************************************************************
 * \fn void m32c_shl_size_r1hidst(void)
 * Shift left a long indirect destination by the value in 
 * register R1.
 * v0
 ************************************************************
 */
static void
m32c_shl_l_r1hidst(void)
{
	int8_t r1h = M32C_REG_R1H;;
	int shl = abs(r1h);
	int right = (r1h & 0x80);
	int size = 4;
	uint64_t Dst;
	uint32_t DstP;
	int codelen_dst = INSTR->codelen_dst;
	INSTR->getdstp(&DstP, 0);
	DstP = (DstP + M32C_INDEXLD()) & 0xffffff;
	/* Is this really read if shift amount == 0 ? */
	Dst = M32C_Read32(DstP);
	if (shl == 0) {
		/* No change of flags when shift amount is zero */
	} else if (right) {
		if (Dst & (1 << (shl - 1))) {
			M32C_REG_FLG |= M32C_FLG_CARRY;
		} else {
			M32C_REG_FLG &= ~M32C_FLG_CARRY;
		}
		Dst = Dst >> shl;
		shl_flags(Dst, size);
		M32C_Write32(Dst, DstP);
	} else {
		Dst = Dst << shl;
		if (Dst & (UINT64_C(1) << 32)) {
			M32C_REG_FLG |= M32C_FLG_CARRY;
		} else {
			M32C_REG_FLG &= ~M32C_FLG_CARRY;
		}
		shl_flags(Dst, size);
		M32C_Write32(Dst, DstP);
	}
	M32C_REG_PC += codelen_dst;
	dbgprintf("m32c_shl_l_r1hidst not tested\n");
}

void
m32c_setup_shl_l_r1hidst(void)
{
	int dst;
	int codelen_dst;
	dst = ((ICODE24() >> 6) & 3) | ((ICODE24() >> 7) & 0x1c);
	INSTR->getdstp = general_am_get(dst, 4, &codelen_dst, GAM_ALL);
	INSTR->codelen_dst = codelen_dst;
	INSTR->proc = m32c_shl_l_r1hidst;
	INSTR->proc();
}

/**
 ***************************************************************
 * \fn void m32c_shlnc_l_immdst(void)
 * Shift logical without carry. 
 * v0
 ***************************************************************
 */
static void
m32c_shlnc_l_immdst(void)
{
	int8_t imm8;
	int shl;
	int right;
	int size = 4;
	uint32_t u32Dst;
	uint64_t Dst;
	int codelen_dst = INSTR->codelen_dst;
	INSTR->getdst(&u32Dst, 0);
	Dst = u32Dst;
	imm8 = M32C_Read8(M32C_REG_PC + codelen_dst);
	shl = abs(imm8);
	right = (imm8 & 0x80);
	if (right) {
		Dst = Dst >> shl;
	} else {
		Dst = Dst << shl;
	}
	shl_flags(Dst, size);
	INSTR->setdst(Dst, 0);
	M32C_REG_PC += codelen_dst + 1;
	dbgprintf("m32c_shlnc_l_immdst not tested\n");
}

void
m32c_setup_shlnc_l_immdst(void)
{
	int size = 4;
	int dst;
	int codelen_dst;
	dst = ((ICODE16() >> 6) & 3) | ((ICODE16() >> 7) & 0x1c);
	INSTR->getdst = general_am_get(dst, size, &codelen_dst, GAM_ALL);
	INSTR->setdst = general_am_set(dst, size, &codelen_dst, GAM_ALL);
	INSTR->codelen_dst = codelen_dst;
	if (INSTR->nrMemAcc == 1) {
		INSTR->cycles = 7;
	}
	INSTR->proc = m32c_shlnc_l_immdst;
	INSTR->proc();
}

/**
 ******************************************************************
 * \fn void m32c_shlnc_l_immidst(void)
 * Shift left an indirect destination without carry.
 * v0
 ******************************************************************
 */
static void
m32c_shlnc_l_immidst(void)
{
	int8_t imm8;
	int shl;
	int right;
	int size = 4;
	uint64_t Dst;
	uint32_t DstP;
	int codelen_dst = INSTR->codelen_dst;
	INSTR->getdstp(&DstP, 0);
	Dst = M32C_Read32(DstP);
	imm8 = M32C_Read8(M32C_REG_PC + codelen_dst);
	shl = abs(imm8);
	right = (imm8 & 0x80);
	if (right) {
		Dst = Dst >> shl;
	} else {
		Dst = Dst << shl;
	}
	shl_flags(Dst, size);
	M32C_Write32(Dst, DstP);
	M32C_REG_PC += codelen_dst + 1;
	dbgprintf("m32c_shlnc_l_immidst not tested\n");
}

void
m32c_setup_shlnc_l_immidst(void)
{
	int dst;
	int codelen_dst;
	dst = ((ICODE24() >> 6) & 3) | ((ICODE24() >> 7) & 0x1c);
	INSTR->getdstp = general_am_get(dst, 4, &codelen_dst, GAM_ALL);
	INSTR->codelen_dst = codelen_dst;
	if (INSTR->nrMemAcc == 1) {
		INSTR->cycles = 10;
	}
	INSTR->proc = m32c_shlnc_l_immidst;
	INSTR->proc();
}

/**
 *************************************************************************
 * \fn void m32c_sin_size(void)
 * String Input. Copy from a fixed source to many destinations.
 * v0
 *************************************************************************
 */
static void
m32c_sin_b(void)
{
	if (M32C_REG_R3 != 0) {
		uint8_t tmp;
		tmp = M32C_Read8(M32C_REG_A0);
		M32C_Write8(tmp, M32C_REG_A1);
		M32C_REG_A1 += 1;
		M32C_REG_R3 -= 1;
		M32C_REG_PC -= 2;
	} else {
		CycleCounter -= 1;
	}
	dbgprintf("m32c_sin_size not tested\n");
}

static void
m32c_sin_w(void)
{
	if (M32C_REG_R3 != 0) {
		uint16_t tmp;
		tmp = M32C_Read16(M32C_REG_A0);
		M32C_Write16(tmp, M32C_REG_A1);
		M32C_REG_A1 += 2;
		M32C_REG_R3 -= 1;
		M32C_REG_PC -= 2;
	} else {
		CycleCounter -= 1;
	}
	dbgprintf("m32c_sin_size not tested\n");
}

void
m32c_setup_sin_size(void)
{
	if (ICODE16() & 0x10) {
		INSTR->proc = m32c_sin_w;
	} else {
		INSTR->proc = m32c_sin_b;
	}
	INSTR->proc();
}

/**
 ************************************************************************
 * \fn void m32c_smovb_size(void)
 * String move backward.
 * v0
 ************************************************************************
 */
static void
m32c_smovb_b(void)
{
	if (M32C_REG_R3 != 0) {
		uint8_t tmp;
		tmp = M32C_Read8(M32C_REG_A0);
		M32C_Write8(tmp, M32C_REG_A1);
		M32C_REG_A0 -= 1;
		M32C_REG_A1 -= 1;
		M32C_REG_R3 -= 1;
		M32C_REG_PC -= 2;
	} else {
		CycleCounter -= 1;
	}
	dbgprintf("m32c_smovb_size not tested\n");
}

static void
m32c_smovb_w(void)
{
	if (M32C_REG_R3 != 0) {
		uint16_t tmp;
		tmp = M32C_Read16(M32C_REG_A0);
		M32C_Write16(tmp, M32C_REG_A1);
		M32C_REG_A0 -= 2;
		M32C_REG_A1 -= 2;
		M32C_REG_R3 -= 1;
		M32C_REG_PC -= 2;
	} else {
		CycleCounter -= 1;
	}
	dbgprintf("m32c_smovb_size not tested\n");
}

void
m32c_setup_smovb_size(void)
{
	if (ICODE16() & 0x10) {
		INSTR->proc = m32c_smovb_w;
	} else {
		INSTR->proc = m32c_smovb_b;
	}
	INSTR->proc();
}

/**
 ***********************************************************************
 * \fn void m32c_smovf_size(void)
 * String move forward.
 * v0
 ***********************************************************************
 */
static void
m32c_smovf_b(void)
{
	if (M32C_REG_R3 != 0) {
		uint8_t tmp;
		tmp = M32C_Read8(M32C_REG_A0);
		M32C_Write8(tmp, M32C_REG_A1);
		M32C_REG_A0 += 1;
		M32C_REG_A1 += 1;
		M32C_REG_R3 -= 1;
		M32C_REG_PC -= 2;
	} else {
		CycleCounter -= 1;
	}
	dbgprintf("m32c_smovf_size not tested.\n");
}

static void
m32c_smovf_w(void)
{
	if (M32C_REG_R3 != 0) {
		uint16_t tmp;
		tmp = M32C_Read16(M32C_REG_A0);
		M32C_Write16(tmp, M32C_REG_A1);
		M32C_REG_A0 += 2;
		M32C_REG_A1 += 2;
		M32C_REG_R3 -= 1;
		M32C_REG_PC -= 2;
	} else {
		CycleCounter -= 1;
	}
	dbgprintf("m32c_smovf_size not tested.\n");
}

void
m32c_setup_smovf_size(void)
{
	if (ICODE16() & 0x10) {
		INSTR->proc = m32c_smovf_w;
	} else {
		INSTR->proc = m32c_smovf_b;
	}
	INSTR->proc();
}

/**
 *************************************************************************
 * \fn void m32c_smovu_size(void)
 * Copy a string forward until a terminating 0 is found.
 * v0
 *************************************************************************
 */

static void
m32c_smovu_b(void)
{
	uint8_t tmp0;
	tmp0 = M32C_Read8(M32C_REG_A0);
	M32C_Write8(tmp0, M32C_REG_A1);
	M32C_REG_A0 += 1;
	M32C_REG_A1 += 1;
	if (tmp0 != 0) {
		M32C_REG_PC -= 2;
	} else {
		CycleCounter -= 1;
	}
	dbgprintf("m32c_smovu_size not tested\n");
}

static void
m32c_smovu_w(void)
{
	uint16_t tmp;
	uint8_t tmp0, tmp1;
	tmp = M32C_Read16(M32C_REG_A0);
	M32C_Write16(tmp, M32C_REG_A1);
	tmp0 = M32C_Read8(M32C_REG_A0);
	tmp1 = M32C_Read8(M32C_REG_A0 + 1);
	M32C_REG_A0 += 2;
	M32C_REG_A1 += 2;
	if ((tmp0 != 0) && (tmp1 != 0)) {
		M32C_REG_PC -= 2;
	} else {
		CycleCounter -= 1;
	}
	dbgprintf("m32c_smovu_size not tested\n");
}

void
m32c_setup_smovu_size(void)
{
	if (ICODE16() & 0x10) {
		INSTR->proc = m32c_smovu_w;
	} else {
		INSTR->proc = m32c_smovu_b;
	}
	INSTR->proc();
}

/**
 *************************************************************************
 * \fn void m32c_sout_size(void)
 * Copy a string to a constant destination.
 * v0
 *************************************************************************
 */

static void
m32c_sout_b(void)
{
	if (M32C_REG_R3 != 0) {
		uint8_t tmp;
		tmp = M32C_Read8(M32C_REG_A0);
		M32C_Write8(tmp, M32C_REG_A1);
		M32C_REG_A0 += 1;
		M32C_REG_R3 -= 1;
		M32C_REG_PC -= 2;
	} else {
		CycleCounter -= 1;
	}
	dbgprintf("m32c_sout_size not tested\n");
}

static void
m32c_sout_w(void)
{
	if (M32C_REG_R3 != 0) {
		uint16_t tmp;
		tmp = M32C_Read16(M32C_REG_A0);
		M32C_Write16(tmp, M32C_REG_A1);
		M32C_REG_A0 += 2;
		M32C_REG_R3 -= 1;
		M32C_REG_PC -= 2;
	}
	dbgprintf("m32c_sout_size not tested\n");
}

void
m32c_setup_sout_size(void)
{
	if (ICODE16() & 0x10) {
		INSTR->proc = m32c_sout_w;
	} else {
		INSTR->proc = m32c_sout_b;
	}
	INSTR->proc();
}

/*
 ***************************************************************************
 * \fn void m32c_sstr_size(void)
 * Fill memory with value from register R0L/R0 
 * v0
 ***************************************************************************
 */
static void
m32c_sstr_b(void)
{
	if (M32C_REG_R3 != 0) {
		M32C_Write8(M32C_REG_R0L, M32C_REG_A1);
		M32C_REG_A1 += 1;
		M32C_REG_R3 -= 1;
		M32C_REG_PC -= 2;
	}
	dbgprintf("m32c_sstr_size not tested\n");
}

static void
m32c_sstr_w(void)
{
	if (M32C_REG_R3 != 0) {
		M32C_Write16(M32C_REG_R0, M32C_REG_A1);
		M32C_REG_A1 += 2;
		M32C_REG_R3 -= 1;
		M32C_REG_PC -= 2;
	}
	dbgprintf("m32c_sstr_size not tested\n");
}

void
m32c_setup_sstr_size(void)
{
	if (ICODE16() & 0x10) {
		INSTR->proc = m32c_sstr_w;
	} else {
		INSTR->proc = m32c_sstr_b;
	}
	INSTR->proc();
}

/*
 *******************************************************************************
 * \fn void m32c_stc_srcdst1(void)
 * Store a 24 Bit control register into a destination.
 * v0
 *******************************************************************************
 */
static void
m32c_stc_srcdst1(void)
{
	int src = INSTR->Arg1;
	uint32_t Src;
	int codelen_dst = INSTR->codelen_dst;
	getreg_cdi24high(src, &Src, 0xfc);
	INSTR->setdst(Src, 0);
	M32C_REG_PC += codelen_dst;
	dbgprintf("m32c_stc_srcdst1 not tested\n");
}

void
m32c_setup_stc_srcdst1(void)
{
	int size = 4;
	int src;
	int dst;
	int codelen_dst;
	dst = ((ICODE24() >> 6) & 3) | ((ICODE24() >> 7) & 0x1c);
	src = ICODE24() & 7;
	INSTR->setdst = general_am_set(dst, size, &codelen_dst, GAM_ALL);
	INSTR->codelen_dst = codelen_dst;
	INSTR->Arg1 = src;
	INSTR->proc = m32c_stc_srcdst1;
	INSTR->proc();
}

/**
 ********************************************************************************
 * Store a 16 Bit control register into a destination.
 * v0
 ********************************************************************************
 */
static void
m32c_stc_srcdst2(void)
{
	int src = INSTR->Arg1;
	uint16_t Src;
	getreg_cdi16(src, &Src, 0xff);
	INSTR->setdst(Src, 0);
	M32C_REG_PC += INSTR->codelen_dst;
	dbgprintf("m32c_stc_srcdst2 not tested\n");
}

void
m32c_setup_stc_srcdst2(void)
{
	int size = 2;
	int dst;
	int src;
	int codelen_dst;
	dst = ((ICODE24() >> 6) & 3) | ((ICODE24() >> 7) & 0x1c);
	src = ICODE24() & 7;
	INSTR->setdst = general_am_set(dst, size, &codelen_dst, GAM_ALL);
	INSTR->codelen_dst = codelen_dst;
	INSTR->Arg1 = src;
	INSTR->proc = m32c_stc_srcdst2;
	INSTR->proc();
}

/**
 ******************************************************************************
 * \fn void m32c_stc_srcdst3(void)
 * Store a 24 Bit control register  into a destination.
 ******************************************************************************
 */
static void
m32c_stc_srcdst3(void)
{
	int src = INSTR->Arg1;
	uint32_t Src;
	int codelen_dst = INSTR->codelen_dst;
	getreg_cdi24low(src, &Src, 0xbf);
	INSTR->setdst(Src, 0);
	M32C_REG_PC += codelen_dst;
	dbgprintf("m32c_stc_srcdst3 not tested\n");
}

void
m32c_setup_stc_srcdst3(void)
{
	int size = 4;
	int dst;
	int src;
	int codelen_dst;
	dst = ((ICODE16() >> 6) & 3) | ((ICODE16() >> 7) & 0x1c);
	src = ICODE16() & 7;
	INSTR->setdst = general_am_set(dst, size, &codelen_dst, GAM_ALL);
	INSTR->codelen_dst = codelen_dst;
	INSTR->Arg1 = src;
	INSTR->proc = m32c_stc_srcdst3;
	INSTR->proc();
}

/**
 *********************************************************************
 * \fn void m32c_stctx_abs16abs24(void)
 * Store context onto stack as indicated  by table_base + offset.
 * v0
 *********************************************************************
 */
void
m32c_stctx_abs16abs24(void)
{
	uint32_t table_base;
	uint32_t offset;
	uint8_t regset;
	uint16_t spcorr;
	uint32_t regp;
	offset = M32C_Read16(M32C_REG_PC) << 1;
	table_base = M32C_Read24(M32C_REG_PC + 2);
	regset = M32C_Read8((table_base + offset) & 0xffffff);
	spcorr = M32C_Read8((table_base + offset + 1) & 0xffffff);
	regp = M32C_REG_SP;
	if (regset & 0x80) {
		regp -= 4;
		M32C_Write32(M32C_REG_FB, regp);
		CycleCounter += 2;
	}
	if (regset & 0x40) {
		regp -= 4;
		M32C_Write32(M32C_REG_SB, regp);
		CycleCounter += 2;
	}
	if (regset & 0x20) {
		regp -= 4;
		M32C_Write32(M32C_REG_A1, regp);
		CycleCounter += 2;
	}
	if (regset & 0x10) {
		regp -= 4;
		M32C_Write32(M32C_REG_A0, regp);
		CycleCounter += 2;
	}
	if (regset & 8) {
		regp -= 2;
		M32C_Write16(M32C_REG_R3, regp);
		CycleCounter += 1;
	}
	if (regset & 4) {
		regp -= 2;
		M32C_Write16(M32C_REG_R2, regp);
		CycleCounter += 1;
	}
	if (regset & 2) {
		regp -= 2;
		M32C_Write16(M32C_REG_R1, regp);
		CycleCounter += 1;
	}
	if (regset & 1) {
		regp -= 2;
		M32C_Write16(M32C_REG_R0, regp);
		CycleCounter += 1;
	}
	M32C_REG_SP -= spcorr;
	if (M32C_REG_SP != regp) {
		fprintf(stderr, "Unexpected spcorr value in Context\n");
	}
	M32C_REG_PC += 5;
	dbgprintf("m32c_stctx_abs16abs24 not tested\n");
}

/**
 *********************************************************************
 * \fn void m32c_stnz_size_immdst(void)
 * Store an immediate into a destination if not zero.
 * v0
 *********************************************************************
 */
static void
m32c_stnz_size_immdst(void)
{
	uint32_t Src;
	int srcsize = INSTR->srcsize;
	if (!(M32C_REG_FLG & M32C_FLG_ZERO)) {
		if (srcsize == 2) {
			Src = M32C_Read16(M32C_REG_PC + INSTR->codelen_dst);
		} else {
			Src = M32C_Read8(M32C_REG_PC + INSTR->codelen_dst);
		}
		INSTR->setdst(Src, M32C_INDEXSD());
	}
	M32C_REG_PC += INSTR->codelen_dst + srcsize;
	dbgprintf("m32c_stnz_size_immdst not tested\n");
}

void
m32c_setup_stnz_size_immdst(void)
{
	int dst;
	int srcsize, opsize;
	int codelen_dst;
	dst = ((ICODE16() >> 6) & 3) | ((ICODE16() >> 7) & 0x1c);
	if (ICODE16() & 0x100) {
		srcsize = opsize = 2;
	} else {
		srcsize = opsize = 1;
		/* Not necessary here */
		ModOpsize(dst, &opsize);
	}
	INSTR->setdst = general_am_set(dst, opsize, &codelen_dst, GAM_ALL);
	INSTR->codelen_dst = codelen_dst;
	INSTR->opsize = opsize;
	INSTR->srcsize = srcsize;
	INSTR->proc = m32c_stnz_size_immdst;
	INSTR->proc();
}

/**
 *********************************************************************
 * \fn void m32c_stnz_size_immdst(void)
 * Store an immediate into an indirect destination if not zero.
 * v0
 *********************************************************************
 */
static void
m32c_stnz_size_immidst(void)
{
	int size = INSTR->opsize;
	int codelen_dst = INSTR->codelen_dst;
	uint32_t Src;
	uint32_t DstP;
	INSTR->getdstp(&DstP, 0);
	DstP = (DstP + M32C_INDEXSD()) & 0xffffff;
	if (!(M32C_REG_FLG & M32C_FLG_ZERO)) {
		if (size == 2) {
			Src = M32C_Read16(M32C_REG_PC + codelen_dst);
			M32C_Write16(Src, DstP);
		} else {
			Src = M32C_Read8(M32C_REG_PC + codelen_dst);
			M32C_Write8(Src, DstP);
		}
	}
	M32C_REG_PC += codelen_dst + size;
	dbgprintf("m32c_stnz_size_immidst not tested\n");
}

void
m32c_setup_stnz_size_immidst(void)
{
	int dst;
	int opsize;
	int codelen_dst;
	if (ICODE24() & 0x100) {
		opsize = 2;
	} else {
		opsize = 1;
	}
	dst = ((ICODE24() >> 6) & 3) | ((ICODE24() >> 7) & 0x1c);
	INSTR->getdstp = general_am_get(dst, 4, &codelen_dst, GAM_ALL);
	INSTR->codelen_dst = codelen_dst;
	INSTR->opsize = opsize;
	INSTR->proc = m32c_stnz_size_immidst;
	INSTR->proc();
}

/**
 ***************************************************************************
 * \fn void m32c_stz_size_immdst(void)
 * Store an immediate into a destination if zero flag is set.
 * v0
 ***************************************************************************
 */
static void
m32c_stz_size_immdst(void)
{
	uint32_t Src;
	int opsize = INSTR->opsize;
	if (M32C_REG_FLG & M32C_FLG_ZERO) {
		if (opsize == 2) {
			Src = M32C_Read16(M32C_REG_PC + INSTR->codelen_dst);
		} else {
			Src = M32C_Read8(M32C_REG_PC + INSTR->codelen_dst);
		}
		INSTR->setdst(Src, M32C_INDEXSD());
	}
	M32C_REG_PC += INSTR->codelen_dst + opsize;
	dbgprintf("m32c_stz_size_immdst not tested\n");
}

void
m32c_setup_stz_size_immdst(void)
{
	int dst;
	int size;
	int codelen_dst;
	if (ICODE16() & 0x100) {
		size = 2;
	} else {
		size = 1;
	}
	dst = ((ICODE16() >> 6) & 3) | ((ICODE16() >> 7) & 0x1c);
	INSTR->setdst = general_am_set(dst, size, &codelen_dst, GAM_ALL);
	INSTR->codelen_dst = codelen_dst;
	INSTR->opsize = size;
	INSTR->cycles = 2;
	INSTR->proc = m32c_stz_size_immdst;
	INSTR->proc();
}

/**
 *****************************************************************************
 * \fn void m32c_stz_size_immidst(void)
 * Store if zero into an indirect destination. 
 *****************************************************************************
 */
static void
m32c_stz_size_immidst(void)
{
	int size = INSTR->opsize;
	int codelen_dst = INSTR->codelen_dst;
	uint32_t Src;
	uint32_t DstP;
	INSTR->getdstp(&DstP, 0);
	DstP = (DstP + M32C_INDEXSD()) & 0xffffff;
	if (M32C_REG_FLG & M32C_FLG_ZERO) {
		if (size == 2) {
			Src = M32C_Read16(M32C_REG_PC + codelen_dst);
			M32C_Write16(Src, DstP);
		} else {
			Src = M32C_Read8(M32C_REG_PC + codelen_dst);
			M32C_Write8(Src, DstP);
		}
	}
	M32C_REG_PC += codelen_dst + size;
	dbgprintf("m32c_stz_size_immidst not tested\n");
}

void
m32c_setup_stz_size_immidst(void)
{
	int dst;
	int size;
	int codelen_dst;
	if (ICODE24() & 0x100) {
		size = 2;
	} else {
		size = 1;
	}
	dst = ((ICODE24() >> 6) & 3) | ((ICODE24() >> 7) & 0x1c);
	INSTR->getdstp = general_am_get(dst, 4, &codelen_dst, GAM_ALL);
	INSTR->codelen_dst = codelen_dst;
	INSTR->opsize = size;
	INSTR->cycles = 5;
	INSTR->proc = m32c_stz_size_immidst;
	INSTR->proc();
}

/**
 *****************************************************************
 * \fn void m32c_stzx_size_imm1imm2dst(void)
 * Store src1 if zero else store source 2
 * v0 
 *****************************************************************
 */
static void
m32c_stzx_size_imm1imm2dst(void)
{
	uint32_t Src1, Src2;
	int size = INSTR->opsize;
	int codelen_dst = INSTR->codelen_dst;
	if (size == 2) {
		if (M32C_REG_FLG & M32C_FLG_ZERO) {
			Src1 = M32C_Read16(M32C_REG_PC + codelen_dst);
			INSTR->setdst(Src1, M32C_INDEXSD());
		} else {
			Src2 = M32C_Read16(M32C_REG_PC + codelen_dst + size);
			INSTR->setdst(Src2, M32C_INDEXSD());
		}
	} else {
		if (M32C_REG_FLG & M32C_FLG_ZERO) {
			Src1 = M32C_Read8(M32C_REG_PC + codelen_dst);
			INSTR->setdst(Src1, M32C_INDEXSD());
		} else {
			Src2 = M32C_Read8(M32C_REG_PC + codelen_dst + size);
			INSTR->setdst(Src2, M32C_INDEXSD());
		}
	}
	M32C_REG_PC += codelen_dst + size + size;
	dbgprintf("m32c_stzx_size_imm1imm2dst not tested\n");
}

void
m32c_setup_stzx_size_imm1imm2dst(void)
{
	int dst;
	int size;
	int codelen_dst;
	if (ICODE16() & 0x100) {
		size = 2;
	} else {
		size = 1;
	}
	dst = ((ICODE16() >> 6) & 3) | ((ICODE16() >> 7) & 0x1c);
	INSTR->setdst = general_am_set(dst, size, &codelen_dst, GAM_ALL);
	INSTR->codelen_dst = codelen_dst;
	INSTR->opsize = size;
	INSTR->cycles = 3;
	INSTR->proc = m32c_stzx_size_imm1imm2dst;
	INSTR->proc();
}

/**
 ***************************************************************************
 * \fn void m32c_stzx_size_imm1imm2idst(void)
 * Store src1 if zero else store src2 into an indirect destination.
 * v0
 ***************************************************************************
 */
static void
m32c_stzx_size_imm1imm2idst(void)
{
	uint32_t Src1, Src2, Dst;
	uint32_t DstP;
	int size = INSTR->opsize;
	int codelen_dst = INSTR->codelen_dst;
	INSTR->getdstp(&DstP, 0);
	DstP = (DstP + M32C_INDEXSD()) & 0xffffff;
	if (size == 2) {
		if (M32C_REG_FLG & M32C_FLG_ZERO) {
			Src1 = M32C_Read16(M32C_REG_PC + codelen_dst);
			Dst = Src1;
		} else {
			Src2 = M32C_Read16(M32C_REG_PC + codelen_dst + size);
			Dst = Src2;
		}
		M32C_Write16(Dst, DstP);
	} else {
		if (M32C_REG_FLG & M32C_FLG_ZERO) {
			Src1 = M32C_Read8(M32C_REG_PC + codelen_dst);
			Dst = Src1;
		} else {
			Src2 = M32C_Read8(M32C_REG_PC + codelen_dst + size);
			Dst = Src2;
		}
		M32C_Write8(Dst, DstP);
	}
	M32C_REG_PC += codelen_dst + size + size;
	dbgprintf("m32c_stzx_size_imm1imm2idst not tested\n");
}

void
m32c_setup_stzx_size_imm1imm2idst(void)
{
	int dst;
	int size;
	int codelen_dst;
	if (ICODE24() & 0x100) {
		size = 2;
	} else {
		size = 1;
	}
	dst = ((ICODE24() >> 6) & 3) | ((ICODE24() >> 7) & 0x1c);
	INSTR->getdstp = general_am_get(dst, 4, &codelen_dst, GAM_ALL);
	INSTR->codelen_dst = codelen_dst;
	INSTR->opsize = size;
	INSTR->cycles = 6;
	INSTR->proc = m32c_stzx_size_imm1imm2idst;
	INSTR->proc();
}

/**
 ***********************************************************************
 * \fn void m32c_sub_size_g_immdst(void)
 * Subtract an immediate from a destination.
 * v1
 ***********************************************************************
 */
static void
m32c_sub_size_g_immdst(void)
{
	uint32_t Src, Dst, Result;
	int srcsize = INSTR->srcsize;
	int opsize = INSTR->opsize;
	int codelen_dst = INSTR->codelen_dst;
	INSTR->getdst(&Dst, M32C_INDEXSD());
	if (srcsize == 2) {
		Src = M32C_Read16(M32C_REG_PC + codelen_dst);
	} else {		/* if (srcsize == 1) */

		Src = M32C_Read8(M32C_REG_PC + codelen_dst);
	}
	Result = Dst - Src;
	INSTR->setdst(Result, M32C_INDEXSD());
	sub_flags(Dst, Src, Result, opsize);
	M32C_REG_PC += codelen_dst + srcsize;
}

void
m32c_setup_sub_size_g_immdst(void)
{
	int dst;
	int srcsize, opsize;
	int codelen_dst;
	dst = ((ICODE16() >> 6) & 3) | ((ICODE16() >> 7) & 0x1c);
	if (ICODE16() & 0x100) {
		srcsize = opsize = 2;
	} else {
		srcsize = opsize = 1;
		ModOpsize(dst, &opsize);
	}
	INSTR->getdst = general_am_get(dst, opsize, &codelen_dst, GAM_ALL);
	INSTR->setdst = general_am_set(dst, opsize, &codelen_dst, GAM_ALL);
	INSTR->codelen_dst = codelen_dst;
	INSTR->opsize = opsize;
	INSTR->srcsize = srcsize;
	INSTR->proc = m32c_sub_size_g_immdst;
	INSTR->proc();
}

/**
 ***********************************************************************
 * \fn void m32c_sub_size_g_immidst(void)
 * Subtract an immediate from an indirectly addressed destination.
 * v1
 ***********************************************************************
 */
static void
m32c_sub_size_g_immidst(void)
{
	uint32_t Src, Dst, Result;
	uint32_t DstP;
	int size = INSTR->opsize;
	int codelen_dst = INSTR->codelen_dst;
	INSTR->getdstp(&DstP, 0);
	DstP = (DstP + M32C_INDEXSD()) & 0xffffff;
	if (size == 2) {
		Dst = M32C_Read16(DstP);
		Src = M32C_Read16(M32C_REG_PC + codelen_dst);
	} else {		/* if (size == 1) */

		Dst = M32C_Read8(DstP);
		Src = M32C_Read8(M32C_REG_PC + codelen_dst);
	}
	Result = Dst - Src;
	if (size == 2) {
		M32C_Write16(Result, DstP);
	} else {
		M32C_Write8(Result, DstP);
	}
	sub_flags(Dst, Src, Result, size);
	M32C_REG_PC += codelen_dst + size;
	dbgprintf("m32c_sub_size_g_immidst not tested\n");
}

void
m32c_setup_sub_size_g_immidst(void)
{
	int dst;
	int size;
	int codelen_dst;
	dst = ((ICODE24() >> 6) & 3) | ((ICODE24() >> 7) & 0x1c);
	if (ICODE24() & 0x100) {
		size = 2;
	} else {
		size = 1;
	}
	INSTR->getdstp = general_am_get(dst, 4, &codelen_dst, GAM_ALL);
	INSTR->codelen_dst = codelen_dst;
	INSTR->opsize = size;
	INSTR->proc = m32c_sub_size_g_immidst;
	INSTR->proc();
}

/**
 ***********************************************************************
 * \fn void m32c_sub_l_g_immdst(void)
 * Subtract an immediate from a destination.
 * v1
 ***********************************************************************
 */
static void
m32c_sub_l_g_immdst(void)
{
	uint32_t Src, Dst, Result;
	int size = 4;
	int codelen_dst = INSTR->codelen_dst;
	INSTR->getdst(&Dst, M32C_INDEXLD());
	Src = M32C_Read32(M32C_REG_PC + codelen_dst);
	Result = Dst - Src;
	INSTR->setdst(Result, M32C_INDEXLD());
	sub_flags(Dst, Src, Result, size);
	M32C_REG_PC += codelen_dst + size;
	dbgprintf("m32c_sub_l_g_immdst not tested\n");
}

void
m32c_setup_sub_l_g_immdst(void)
{
	int dst;
	int size = 4;
	int codelen_dst;
	dst = ((ICODE16() >> 6) & 3) | ((ICODE16() >> 7) & 0x1c);
	INSTR->getdst = general_am_get(dst, size, &codelen_dst, GAM_ALL);
	INSTR->setdst = general_am_set(dst, size, &codelen_dst, GAM_ALL);
	INSTR->codelen_dst = codelen_dst;
	INSTR->proc = m32c_sub_l_g_immdst;
	INSTR->proc();
}

/**
 ***********************************************************************
 * \fn void m32c_sub_l_g_immidst(void)
 * Subtract an immediate from an indirect destination.
 * v1
 ***********************************************************************
 */
static void
m32c_sub_l_g_immidst(void)
{
	uint32_t Src, Dst, Result;
	uint32_t DstP;
	int size = 4;
	int codelen_dst = INSTR->codelen_dst;
	INSTR->getdstp(&DstP, 0);
	DstP = (DstP + M32C_INDEXLD()) & 0xffffff;
	Dst = M32C_Read32(DstP);
	Src = M32C_Read32(M32C_REG_PC + codelen_dst);
	Result = Dst - Src;
	M32C_Write32(Result, DstP);
	sub_flags(Dst, Src, Result, size);
	M32C_REG_PC += codelen_dst + size;
	dbgprintf("m32c_sub_l_g_immidst not tested\n");
}

void
m32c_setup_sub_l_g_immidst(void)
{
	int dst;
	int codelen_dst;
	dst = ((ICODE24() >> 6) & 3) | ((ICODE24() >> 7) & 0x1c);
	INSTR->getdstp = general_am_get(dst, 4, &codelen_dst, GAM_ALL);
	INSTR->codelen_dst = codelen_dst;
	INSTR->proc = m32c_sub_l_g_immidst;
	INSTR->proc();
}

/**
 ***********************************************************************
 * \fn void m32c_sub_size_s_immdst(void)
 * Subtract an immediate from a destination (short). 
 * v1
 ***********************************************************************
 */
static void
m32c_sub_size_s_immdst(void)
{
	int size = INSTR->opsize;
	uint32_t imm;
	uint32_t Dst;
	uint32_t result;
	int codelen_dst = INSTR->codelen_dst;
	INSTR->getam2bit(&Dst, M32C_INDEXSD());
	if (size == 2) {
		imm = M32C_Read16(M32C_REG_PC + codelen_dst);
	} else {
		imm = M32C_Read8(M32C_REG_PC + codelen_dst);
	}
	result = Dst - imm;
	INSTR->setam2bit(result, M32C_INDEXSD());
	sub_flags(Dst, imm, result, size);
	M32C_REG_PC += codelen_dst + size;
}

void
m32c_setup_sub_size_s_immdst(void)
{
	int size;
	int codelen_dst;
	int dst = (ICODE8() >> 4) & 3;
	if (ICODE8() & 1) {
		size = 2;
	} else {
		size = 1;
	}
	INSTR->getam2bit = am2bit_getproc(dst, &codelen_dst, size);
	INSTR->setam2bit = am2bit_setproc(dst, &codelen_dst, size);
	INSTR->codelen_dst = codelen_dst;
	INSTR->opsize = size;
	INSTR->proc = m32c_sub_size_s_immdst;
	INSTR->proc();
}

/**
 *************************************************************************
 * \fn void m32c_sub_size_s_immdst(void)
 * Subtract an immediate from an indirectly addressed destination (short). 
 * v1
 *************************************************************************
 */
static void
m32c_sub_size_s_immidst(void)
{
	uint32_t imm;
	uint32_t Dst;
	uint32_t DstP;
	uint32_t Result;
	int size = INSTR->opsize;
	int codelen = INSTR->codelen_dst;
	INSTR->getam2bit(&DstP, 0);
	DstP = (DstP + M32C_INDEXSD()) & 0xffffff;
	if (size == 2) {
		Dst = M32C_Read16(DstP);
		imm = M32C_Read16(M32C_REG_PC + codelen);
	} else {
		Dst = M32C_Read8(DstP);
		imm = M32C_Read8(M32C_REG_PC + codelen);
	}
	Result = Dst - imm;
	if (size == 2) {
		M32C_Write16(Result, DstP);
	} else {
		M32C_Write8(Result, DstP);
	}
	sub_flags(Dst, imm, Result, size);
	M32C_REG_PC += codelen + size;
	dbgprintf("m32c_sub_size_s_immdst not tested\n");
}

void
m32c_setup_sub_size_s_immidst(void)
{
	int size;
	int codelen_dst;
	int dst = (ICODE16() >> 4) & 3;
	if (ICODE16() & 1) {
		size = 2;
	} else {
		size = 1;
	}
	INSTR->getam2bit = am2bit_getproc(dst, &codelen_dst, 4);
	INSTR->codelen_dst = codelen_dst;
	INSTR->opsize = size;
	INSTR->proc = m32c_sub_size_s_immidst;
	INSTR->proc();
}

/**
 ***************************************************************************
 * \fn void m32c_sub_size_g_srcdst(void)
 * Subtract src from destination. 
 * v1
 ***************************************************************************
 */
static void
m32c_sub_size_g_srcdst(void)
{
	uint32_t Src, Dst, Result;
	int opsize = INSTR->opsize;
	int codelen_src = INSTR->codelen_src;
	int codelen_dst = INSTR->codelen_dst;
	INSTR->getsrc(&Src, M32C_INDEXSS());
	M32C_REG_PC += codelen_src;
	INSTR->getdst(&Dst, M32C_INDEXSD());
	Result = Dst - Src;
	INSTR->setdst(Result, M32C_INDEXSD());
	sub_flags(Dst, Src, Result, opsize);
	M32C_REG_PC += codelen_dst;
	dbgprintf("m32c_sub_size_g_srcdst not tested\n");
}

void
m32c_setup_sub_size_g_srcdst(void)
{
	int dst, src;
	int srcsize, opsize;
	int codelen_src;
	int codelen_dst;
	dst = ((ICODE16() >> 6) & 3) | ((ICODE16() >> 7) & 0x1c);
	src = ((ICODE16() >> 4) & 3) | ((ICODE16() >> 10) & 0x1c);
	if (ICODE16() & 0x100) {
		srcsize = opsize = 2;
	} else {
		srcsize = opsize = 1;
		ModOpsize(dst, &opsize);
	}
	INSTR->getdst = general_am_get(dst, opsize, &codelen_dst, GAM_ALL);
	INSTR->getsrc = general_am_get(src, srcsize, &codelen_src, GAM_ALL);
	INSTR->setdst = general_am_set(dst, opsize, &codelen_dst, GAM_ALL);
	INSTR->codelen_dst = codelen_dst;
	INSTR->codelen_src = codelen_src;
	INSTR->opsize = opsize;
	INSTR->srcsize = srcsize;
	INSTR->proc = m32c_sub_size_g_srcdst;
	INSTR->proc();
}

/**
 *********************************************************************
 * \fn void m32c_sub_size_g_isrcdst(void)
 * Subtract an indirectly addressed source from a destination.
 * v1
 *********************************************************************
 */
static void
m32c_sub_size_g_isrcdst(void)
{
	uint32_t Src, Dst, Result;
	uint32_t SrcP;
	int srcsize = INSTR->srcsize;
	int opsize = INSTR->opsize;
	int codelen_src = INSTR->codelen_src;
	int codelen_dst = INSTR->codelen_dst;;
	INSTR->getsrcp(&SrcP, 0);
	SrcP = (SrcP + M32C_INDEXSS()) & 0xffffff;
	if (srcsize == 2) {
		Src = M32C_Read16(SrcP);
	} else {
		Src = M32C_Read8(SrcP);
	}
	M32C_REG_PC += codelen_src;
	INSTR->getdst(&Dst, M32C_INDEXSD());
	Result = Dst - Src;
	INSTR->setdst(Result, M32C_INDEXSD());
	sub_flags(Dst, Src, Result, opsize);
	M32C_REG_PC += codelen_dst;
	dbgprintf("m32c_sub_size_g_isrcdst not tested\n");
}

void
m32c_setup_sub_size_g_isrcdst(void)
{
	int dst, src;
	int srcsize, opsize;
	int codelen_src;
	int codelen_dst;
	dst = ((ICODE24() >> 6) & 3) | ((ICODE24() >> 7) & 0x1c);
	src = ((ICODE24() >> 4) & 3) | ((ICODE24() >> 10) & 0x1c);
	if (ICODE24() & 0x100) {
		srcsize = opsize = 2;
	} else {
		srcsize = opsize = 1;
		ModOpsize(dst, &opsize);
	}
	INSTR->getdst = general_am_get(dst, opsize, &codelen_dst, GAM_ALL);
	INSTR->getsrcp = general_am_get(src, 4, &codelen_src, GAM_ALL);
	INSTR->setdst = general_am_set(dst, opsize, &codelen_dst, GAM_ALL);
	INSTR->codelen_dst = codelen_dst;
	INSTR->codelen_src = codelen_src;
	INSTR->opsize = opsize;
	INSTR->srcsize = srcsize;

	INSTR->proc = m32c_sub_size_g_isrcdst;
	INSTR->proc();
}

/**
 ***************************************************************
 * \fn void m32c_sub_size_g_srcidst(void)
 * Subtract an indirectly addressed source from an indirectly
 * addressed destination.
 * v1
 ***************************************************************
 */
static void
m32c_sub_size_g_srcidst(void)
{
	uint32_t Src, Dst, Result;
	uint32_t DstP;
	int size = INSTR->opsize;
	int codelen_src = INSTR->codelen_src;
	int codelen_dst = INSTR->codelen_dst;
	INSTR->getsrc(&Src, M32C_INDEXSS());
	M32C_REG_PC += codelen_src;
	INSTR->getdstp(&DstP, 0);
	DstP = (DstP + M32C_INDEXSD()) & 0xffffff;
	if (size == 2) {
		Dst = M32C_Read16(DstP);
		Result = Dst - Src;
		M32C_Write16(Result, DstP);
	} else {
		Dst = M32C_Read8(DstP);
		Result = Dst - Src;
		M32C_Write8(Result, DstP);
	}
	sub_flags(Dst, Src, Result, size);
	M32C_REG_PC += codelen_dst;
	dbgprintf("m32c_sub_size_g_srcidst not tested\n");
}

void
m32c_setup_sub_size_g_srcidst(void)
{
	int dst, src;
	int size;
	int codelen_src;
	int codelen_dst;
	dst = ((ICODE24() >> 6) & 3) | ((ICODE24() >> 7) & 0x1c);
	src = ((ICODE24() >> 4) & 3) | ((ICODE24() >> 10) & 0x1c);
	if (ICODE24() & 0x100) {
		size = 2;
	} else {
		size = 1;
	}
	INSTR->getdstp = general_am_get(dst, 4, &codelen_dst, GAM_ALL);
	INSTR->getsrc = general_am_get(src, size, &codelen_src, GAM_ALL);
	INSTR->codelen_dst = codelen_dst;
	INSTR->codelen_src = codelen_src;
	INSTR->opsize = size;
	INSTR->proc = m32c_sub_size_g_srcidst;
	INSTR->proc();
}

/**
 ***************************************************************************
 * \fn void m32c_sub_size_g_isrcidst(void)
 * Subtract an indirectly addressed source from an indirectly addressed
 * destination.
 * v1
 ***************************************************************************
 */
static void
m32c_sub_size_g_isrcidst(void)
{
	uint32_t Src, Dst, Result;
	uint32_t DstP, SrcP;
	int size = INSTR->opsize;
	int codelen_src = INSTR->codelen_src;
	int codelen_dst = INSTR->codelen_dst;
	INSTR->getsrcp(&SrcP, 0);
	SrcP = (SrcP + M32C_INDEXSS()) & 0xffffff;
	M32C_REG_PC += codelen_src;
	INSTR->getdstp(&DstP, 0);
	DstP = (DstP + M32C_INDEXSD()) & 0xffffff;
	if (size == 2) {
		Src = M32C_Read16(SrcP);
		Dst = M32C_Read16(DstP);
		Result = Dst - Src;
		M32C_Write16(Result, DstP);
	} else {
		Src = M32C_Read8(SrcP);
		Dst = M32C_Read8(DstP);
		Result = Dst - Src;
		M32C_Write8(Result, DstP);
	}
	sub_flags(Dst, Src, Result, size);
	M32C_REG_PC += codelen_dst;
	dbgprintf("m32c_sub_size_g_isrcidst not tested\n");
}

void
m32c_setup_sub_size_g_isrcidst(void)
{
	int dst, src;
	int size;
	int codelen_src;
	int codelen_dst;
	dst = ((ICODE24() >> 6) & 3) | ((ICODE24() >> 7) & 0x1c);
	src = ((ICODE24() >> 4) & 3) | ((ICODE24() >> 10) & 0x1c);
	if (ICODE24() & 0x100) {
		size = 2;
	} else {
		size = 1;
	}
	INSTR->getdstp = general_am_get(dst, 4, &codelen_dst, GAM_ALL);
	INSTR->getsrcp = general_am_get(src, 4, &codelen_src, GAM_ALL);
	INSTR->codelen_src = codelen_src;
	INSTR->codelen_dst = codelen_dst;
	INSTR->opsize = size;
	INSTR->proc = m32c_sub_size_g_isrcidst;
	INSTR->proc();
}

/**
 **********************************************************************
 * \fn void m32c_sub_l_g_srcdst(void)
 * Subtract a src from a destination (long).
 * v1
 **********************************************************************
 */
static void
m32c_sub_l_g_srcdst(void)
{
	uint32_t Src, Dst, Result;
	int size = 4;
	int codelen_src = INSTR->codelen_src;
	int codelen_dst = INSTR->codelen_dst;
	INSTR->getsrc(&Src, M32C_INDEXLS());
	M32C_REG_PC += codelen_src;
	INSTR->getdst(&Dst, M32C_INDEXLD());
	Result = Dst - Src;
	INSTR->setdst(Result, M32C_INDEXLD());
	sub_flags(Dst, Src, Result, size);
	M32C_REG_PC += codelen_dst;
}

void
m32c_setup_sub_l_g_srcdst(void)
{
	int dst, src;
	int size = 4;
	int codelen_src;
	int codelen_dst;
	dst = ((ICODE16() >> 6) & 3) | ((ICODE16() >> 7) & 0x1c);
	src = ((ICODE16() >> 4) & 3) | ((ICODE16() >> 10) & 0x1c);
	INSTR->getdst = general_am_get(dst, size, &codelen_dst, GAM_ALL);
	INSTR->getsrc = general_am_get(src, size, &codelen_src, GAM_ALL);
	INSTR->setdst = general_am_set(dst, size, &codelen_dst, GAM_ALL);
	INSTR->codelen_src = codelen_src;
	INSTR->codelen_dst = codelen_dst;
	INSTR->proc = m32c_sub_l_g_srcdst;
	INSTR->proc();
}

/**
 **************************************************************************
 * \fn void m32c_sub_l_g_isrcdst(void)
 * Subtract an indirectly addressed source from a destination (long). 
 * v1
 **************************************************************************
 */
static void
m32c_sub_l_g_isrcdst(void)
{
	uint32_t Src, Dst, Result;
	uint32_t SrcP;
	int size = 4;
	int codelen_src = INSTR->codelen_src;
	int codelen_dst = INSTR->codelen_dst;
	INSTR->getsrcp(&SrcP, 0);
	SrcP = (SrcP + M32C_INDEXLS()) & 0xffffff;
	Src = M32C_Read32(SrcP);
	M32C_REG_PC += codelen_src;
	INSTR->getdst(&Dst, M32C_INDEXLD());
	Result = Dst - Src;
	INSTR->setdst(Result, M32C_INDEXLD());
	sub_flags(Dst, Src, Result, size);
	M32C_REG_PC += codelen_dst;
	dbgprintf("m32c_sub_l_g_isrcdst not tested\n");
}

void
m32c_setup_sub_l_g_isrcdst(void)
{
	int dst, src;
	int size = 4;
	int codelen_src;
	int codelen_dst;
	dst = ((ICODE24() >> 6) & 3) | ((ICODE24() >> 7) & 0x1c);
	src = ((ICODE24() >> 4) & 3) | ((ICODE24() >> 10) & 0x1c);
	INSTR->getdst = general_am_get(dst, size, &codelen_dst, GAM_ALL);
	INSTR->getsrcp = general_am_get(src, 4, &codelen_src, GAM_ALL);
	INSTR->setdst = general_am_set(dst, size, &codelen_dst, GAM_ALL);
	INSTR->codelen_src = codelen_src;
	INSTR->codelen_dst = codelen_dst;
	INSTR->proc = m32c_sub_l_g_isrcdst;
	INSTR->proc();
}

/**
 **********************************************************************
 * \fn void m32c_sub_l_g_srcidst(void)
 * Subtract a source from an indirectly addressed destination (long).
 * v1
 **********************************************************************
 */
static void
m32c_sub_l_g_srcidst(void)
{
	uint32_t Src, Dst, Result;
	uint32_t DstP;
	int size = 4;
	int codelen_src = INSTR->codelen_src;
	int codelen_dst = INSTR->codelen_dst;
	INSTR->getsrc(&Src, M32C_INDEXLS());
	M32C_REG_PC += codelen_src;
	INSTR->getdstp(&DstP, 0);
	DstP = (DstP + M32C_INDEXLD()) & 0xffffff;
	Dst = M32C_Read32(DstP);
	Result = Dst - Src;
	M32C_Write32(Result, DstP);
	sub_flags(Dst, Src, Result, size);
	M32C_REG_PC += codelen_dst;
	dbgprintf("m32c_sub_l_g_srcidst not tested\n");
}

void
m32c_setup_sub_l_g_srcidst(void)
{
	int dst, src;
	int size = 4;
	int codelen_src;
	int codelen_dst;
	dst = ((ICODE24() >> 6) & 3) | ((ICODE24() >> 7) & 0x1c);
	src = ((ICODE24() >> 4) & 3) | ((ICODE24() >> 10) & 0x1c);
	INSTR->getdstp = general_am_get(dst, 4, &codelen_dst, GAM_ALL);
	INSTR->getsrc = general_am_get(src, size, &codelen_src, GAM_ALL);
	INSTR->codelen_dst = codelen_dst;
	INSTR->codelen_src = codelen_src;
	INSTR->proc = m32c_sub_l_g_srcidst;
	INSTR->proc();
}

/**
 ***********************************************************************
 * \fn void m32c_sub_l_g_isrcidst(void)
 * Subtract an indirectly addressed source from an indirectly addressed
 * destination.
 * v0
 ***********************************************************************
 */
static void
m32c_sub_l_g_isrcidst(void)
{
	uint32_t Src, Dst, Result;
	uint32_t DstP, SrcP;
	int size = 4;
	int codelen_src = INSTR->codelen_src;
	int codelen_dst = INSTR->codelen_dst;
	INSTR->getsrcp(&SrcP, 0);
	SrcP = (SrcP + M32C_INDEXLS()) & 0xffffff;
	Src = M32C_Read32(SrcP);
	M32C_REG_PC += codelen_src;
	INSTR->getdstp(&DstP, 0);
	DstP = (DstP + M32C_INDEXLD()) & 0xffffff;
	Dst = M32C_Read32(DstP);
	Result = Dst - Src;
	M32C_Write32(Result, DstP);
	sub_flags(Dst, Src, Result, size);
	M32C_REG_PC += codelen_dst;
	dbgprintf("m32c_sub_l_g_isrcidst not tested\n");
}

void
m32c_setup_sub_l_g_isrcidst(void)
{
	int dst, src;
	int codelen_src;
	int codelen_dst;
	dst = ((ICODE24() >> 6) & 3) | ((ICODE24() >> 7) & 0x1c);
	src = ((ICODE24() >> 4) & 3) | ((ICODE24() >> 10) & 0x1c);
	INSTR->getdstp = general_am_get(dst, 4, &codelen_dst, GAM_ALL);
	INSTR->getsrcp = general_am_get(src, 4, &codelen_src, GAM_ALL);
	INSTR->codelen_src = codelen_src;
	INSTR->codelen_dst = codelen_dst;
	INSTR->proc = m32c_sub_l_g_isrcidst;
	INSTR->proc();
}

/**
 **********************************************************************
 * \fn void m32c_subx_immdst(void)
 * Subtract a sign extended 8 Bit immediate from a 32 Bit destination. 
 * v1
 **********************************************************************
 */
static void
m32c_subx_immdst(void)
{
	uint32_t Dst, Result;
	int32_t Src;
	int size = 4;
	int codelen_dst = INSTR->codelen_dst;
	INSTR->getdst(&Dst, 0);
	Src = (int32_t) (int8_t) M32C_Read8(M32C_REG_PC + codelen_dst);
	Result = Dst - Src;
	INSTR->setdst(Result, 0);
	sub_flags(Dst, Src, Result, size);
	M32C_REG_PC += codelen_dst + 1;
	dbgprintf("m32c_subx_immdst not tested\n");
}

void
m32c_setup_subx_immdst(void)
{
	int dst;
	int size = 4;
	int codelen_dst;
	dst = ((ICODE16() >> 6) & 3) | ((ICODE16() >> 7) & 0x1c);
	INSTR->getdst = general_am_get(dst, size, &codelen_dst, GAM_ALL);
	INSTR->setdst = general_am_set(dst, size, &codelen_dst, GAM_ALL);
	INSTR->proc = m32c_subx_immdst;
	INSTR->codelen_dst = codelen_dst;
	INSTR->proc();
}

/**
 **********************************************************************
 * \fn void m32c_subx_immidst(void)
 * subtract a sign extended immediate from an indirectly
 * addressed destination. 
 * v1
 **********************************************************************
 */
static void
m32c_subx_immidst(void)
{
	uint32_t Dst, Result;
	uint32_t DstP;
	int32_t Src;
	int size = 4;
	int codelen_dst = INSTR->codelen_dst;
	INSTR->getdstp(&DstP, 0);
	Dst = M32C_Read32(DstP);
	Src = (int32_t) (int8_t) M32C_Read8(M32C_REG_PC + codelen_dst);
	Result = Dst - Src;
	M32C_Write32(Result, DstP);
	sub_flags(Dst, Src, Result, size);
	M32C_REG_PC += codelen_dst + 1;
	dbgprintf("m32c_subx_immidst not tested\n");
}

void
m32c_setup_subx_immidst(void)
{
	int dst;
	int codelen_dst;
	dst = ((ICODE24() >> 6) & 3) | ((ICODE24() >> 7) & 0x1c);
	INSTR->getdstp = general_am_get(dst, 4, &codelen_dst, GAM_ALL);
	INSTR->codelen_dst = codelen_dst;
	INSTR->proc = m32c_subx_immidst;
	INSTR->proc();
}

/**
 *****************************************************************
 * \fn void m32c_subx_srcdst(void)
 * Subtract an 8 Bit source from a 32 Bit destination after
 * sign extension
 * v1
 *****************************************************************
 */
static void
m32c_subx_srcdst(void)
{
	uint32_t Dst, Result;
	uint32_t Src;
	int size_dst = 4;
	int codelen_src = INSTR->codelen_src;
	int codelen_dst = INSTR->codelen_dst;
	INSTR->getsrc(&Src, 0);
	M32C_REG_PC += codelen_src;
	INSTR->getdst(&Dst, 0);
	Src = (int32_t) (int8_t) Src;
	Result = Dst - Src;
	INSTR->setdst(Result, 0);
	sub_flags(Dst, Src, Result, size_dst);
	M32C_REG_PC += codelen_dst;
	dbgprintf("m32c_subx_srcdst not tested\n");
}

void
m32c_setup_subx_srcdst(void)
{
	int dst, src;
	int size_src = 1;
	int size_dst = 4;
	int codelen_src;
	int codelen_dst;
	dst = ((ICODE16() >> 6) & 3) | ((ICODE16() >> 7) & 0x1c);
	src = ((ICODE16() >> 4) & 3) | ((ICODE16() >> 10) & 0x1c);
	INSTR->getdst = general_am_get(dst, size_dst, &codelen_dst, GAM_ALL);
	INSTR->getsrc = general_am_get(src, size_src, &codelen_src, GAM_ALL);
	INSTR->setdst = general_am_set(dst, size_dst, &codelen_dst, GAM_ALL);
	INSTR->codelen_dst = codelen_dst;
	INSTR->codelen_src = codelen_src;
	if (INSTR->nrMemAcc == 2) {
		INSTR->cycles = 6;
	}
	INSTR->proc = m32c_subx_srcdst;
	INSTR->proc();
}

/**
 **************************************************************************
 * \fn void m32c_subx_isrcdst(void)
 * Subtract an 8 Bit indirectly addressed source from a 32 Bit immediate. 
 * v1
 **************************************************************************
 */
static void
m32c_subx_isrcdst(void)
{
	uint32_t Dst, Result;
	uint32_t Src;
	uint32_t SrcP;
	int size_dst = 4;
	int codelen_src = INSTR->codelen_src;
	int codelen_dst = INSTR->codelen_dst;
	INSTR->getsrcp(&SrcP, 0);
	Src = M32C_Read8(SrcP);
	M32C_REG_PC += codelen_src;
	INSTR->getdst(&Dst, 0);
	Src = (int32_t) (int8_t) Src;
	Result = Dst - Src;
	INSTR->setdst(Result, 0);
	sub_flags(Dst, Src, Result, size_dst);
	M32C_REG_PC += codelen_dst;
	dbgprintf("m32c_subx_isrcdst not tested\n");
}

void
m32c_setup_subx_isrcdst(void)
{
	int dst, src;
	int size_dst = 4;
	int codelen_src;
	int codelen_dst;
	dst = ((ICODE24() >> 6) & 3) | ((ICODE24() >> 7) & 0x1c);
	src = ((ICODE24() >> 4) & 3) | ((ICODE24() >> 10) & 0x1c);
	INSTR->getdst = general_am_get(dst, size_dst, &codelen_dst, GAM_ALL);
	INSTR->getsrcp = general_am_get(src, 4, &codelen_src, GAM_ALL);
	INSTR->setdst = general_am_set(dst, size_dst, &codelen_dst, GAM_ALL);
	INSTR->codelen_src = codelen_src;
	INSTR->codelen_dst = codelen_dst;
	if (INSTR->nrMemAcc == 2) {
		INSTR->cycles = 9;
	}
	INSTR->proc = m32c_subx_isrcdst;
	INSTR->proc();
}

/**
 ************************************************************
 * \fn void m32c_subx_srcidst(void)
 * Subtract a sign extended 8 Bit src from an indirectly
 * addressed destination.
 * v1
 ************************************************************
 */
static void
m32c_subx_srcidst(void)
{
	uint32_t Dst, Result;
	uint32_t Src;
	uint32_t DstP;
	int codelen_src = INSTR->codelen_src;
	int codelen_dst = INSTR->codelen_dst;
	INSTR->getsrc(&Src, 0);
	M32C_REG_PC += codelen_src;
	INSTR->getdstp(&DstP, 0);
	Dst = M32C_Read32(DstP);
	Src = (int32_t) (int8_t) Src;
	Result = Dst - Src;
	M32C_Write32(Result, DstP);
	sub_flags(Dst, Src, Result, 4);
	M32C_REG_PC += codelen_dst;
	dbgprintf("m32c_subx_srcidst not tested\n");
}

void
m32c_setup_subx_srcidst(void)
{
	int dst, src;
	int codelen_src;
	int codelen_dst;
	dst = ((ICODE24() >> 6) & 3) | ((ICODE24() >> 7) & 0x1c);
	src = ((ICODE24() >> 4) & 3) | ((ICODE24() >> 10) & 0x1c);
	INSTR->getdstp = general_am_get(dst, 4, &codelen_dst, GAM_ALL);
	INSTR->getsrc = general_am_get(src, 1, &codelen_src, GAM_ALL);
	INSTR->codelen_src = codelen_src;
	INSTR->codelen_dst = codelen_dst;
	if (INSTR->nrMemAcc == 2) {
		INSTR->cycles = 9;
	}
	INSTR->proc = m32c_subx_srcidst;
	INSTR->proc();
}

/**
 *******************************************************************
 * \fn void m32c_subx_isrcidst(void)
 * Subtract a sign extended indirectly addressed 8 bit source from 
 * an indirectly addressed 32 Bit destination.
 * v0
 *******************************************************************
 */
static void
m32c_subx_isrcidst(void)
{
	uint32_t Dst, Result;
	uint32_t Src;
	uint32_t SrcP, DstP;
	int size_dst = 4;
	int codelen_src = INSTR->codelen_src;
	int codelen_dst = INSTR->codelen_dst;
	INSTR->getsrcp(&SrcP, 0);
	Src = M32C_Read8(SrcP);
	M32C_REG_PC += codelen_src;
	INSTR->getdstp(&DstP, 0);
	Dst = M32C_Read32(DstP);
	Src = (int32_t) (int8_t) Src;
	Result = Dst - Src;
	M32C_Write32(Result, DstP);
	sub_flags(Dst, Src, Result, size_dst);
	M32C_REG_PC += codelen_dst;
	dbgprintf("m32c_subx_isrcidst not tested\n");
}

void
m32c_setup_subx_isrcidst(void)
{
	int dst, src;
	int codelen_src;
	int codelen_dst;
	dst = ((ICODE24() >> 6) & 3) | ((ICODE24() >> 7) & 0x1c);
	src = ((ICODE24() >> 4) & 3) | ((ICODE24() >> 10) & 0x1c);
	INSTR->getdstp = general_am_get(dst, 4, &codelen_dst, GAM_ALL);
	INSTR->getsrcp = general_am_get(src, 4, &codelen_src, GAM_ALL);
	INSTR->codelen_src = codelen_src;
	INSTR->codelen_dst = codelen_src;
	if (INSTR->nrMemAcc == 2) {
		INSTR->cycles = 12;
	}
	INSTR->proc = m32c_subx_isrcidst;
	INSTR->proc();
}

/**
 *****************************************************************
 * \fn void m32c_tst_size_g_immdst(void)
 * Calculate the logical and of an immediate and a destination.
 * Result is not stored. The zero and carry flags are modified.
 * v0
 *****************************************************************
 */
static void
m32c_tst_size_g_immdst(void)
{
	uint32_t Src, Dst, Result;
	int srcsize = INSTR->srcsize;
	int opsize = INSTR->opsize;
	int codelen_dst = INSTR->codelen_dst;
	INSTR->getdst(&Dst, M32C_INDEXSD());
	if (srcsize == 2) {
		Src = M32C_Read16(M32C_REG_PC + codelen_dst);
	} else {		/* if(srcsize == 1) */

		Src = M32C_Read8(M32C_REG_PC + codelen_dst);
	}
	Result = Dst & Src;
	and_flags(Result, opsize);
	M32C_REG_PC += codelen_dst + srcsize;
}

void
m32c_setup_tst_size_g_immdst(void)
{
	int dst;
	int srcsize, opsize;
	int codelen_dst;
	dst = ((ICODE16() >> 6) & 3) | ((ICODE16() >> 7) & 0x1c);
	if (ICODE16() & 0x100) {
		srcsize = opsize = 2;
	} else {
		srcsize = opsize = 1;
		ModOpsize(dst, &opsize);
	}
	INSTR->getdst = general_am_get(dst, opsize, &codelen_dst, GAM_ALL);
	INSTR->codelen_dst = codelen_dst;
	INSTR->srcsize = srcsize;
	INSTR->opsize = opsize;
	INSTR->proc = m32c_tst_size_g_immdst;
	INSTR->proc();
}

/**
 *********************************************************************** 
 * \fn void m32c_tst_size_s_immdst(void)
 * modify the flags depending on the result of the logical and of
 * an immediate and a destination.
 * v0
 *********************************************************************** 
 */
static void
m32c_tst_size_s_immdst(void)
{
	int size = INSTR->opsize;
	uint32_t imm;
	uint32_t Dst;
	uint32_t Result;
	int codelen_dst = INSTR->codelen_dst;
	INSTR->getam2bit(&Dst, M32C_INDEXSD());
	if (size == 2) {
		imm = M32C_Read16(M32C_REG_PC + codelen_dst);
	} else {
		imm = M32C_Read8(M32C_REG_PC + codelen_dst);
	}
	Result = Dst & imm;
	and_flags(Result, size);
	M32C_REG_PC += codelen_dst + size;
	dbgprintf("m32c_tst_size_immdst not tested\n");
}

void
m32c_setup_tst_size_s_immdst(void)
{
	int size;
	int codelen_dst;
	int dst = (ICODE8() >> 4) & 3;
	if (ICODE8() & 1) {
		size = 2;
	} else {
		size = 1;
	}
	INSTR->getam2bit = am2bit_getproc(dst, &codelen_dst, size);
	INSTR->codelen_dst = codelen_dst;
	INSTR->opsize = size;
	INSTR->proc = m32c_tst_size_s_immdst;
	INSTR->proc();
}

/**
 ************************************************************************
 * \fn void m32c_tst_size_g_srcdst(void)
 * Change the flags depending on the result of the logical and 
 * of a src and a destination.
 * v0
 ************************************************************************
 */
static void
m32c_tst_size_g_srcdst(void)
{
	uint32_t Src, Dst, Result;
	int opsize = INSTR->opsize;
	int codelen_src = INSTR->codelen_src;
	int codelen_dst = INSTR->codelen_dst;
	INSTR->getsrc(&Src, M32C_INDEXSS());
	M32C_REG_PC += codelen_src;
	INSTR->getdst(&Dst, M32C_INDEXSD());
	Result = Dst & Src;
	and_flags(Result, opsize);
	M32C_REG_PC += codelen_dst;
}

void
m32c_setup_tst_size_g_srcdst(void)
{
	int dst, src;
	int srcsize, opsize;
	int codelen_src;
	int codelen_dst;
	dst = ((ICODE24() >> 6) & 3) | ((ICODE24() >> 7) & 0x1c);
	src = ((ICODE24() >> 4) & 3) | ((ICODE24() >> 10) & 0x1c);
	if (ICODE24() & 0x100) {
		srcsize = opsize = 2;
	} else {
		srcsize = opsize = 1;
		ModOpsize(dst, &opsize);
	}
	INSTR->getdst = general_am_get(dst, opsize, &codelen_dst, GAM_ALL);
	INSTR->getsrc = general_am_get(src, opsize, &codelen_src, GAM_ALL);
	INSTR->codelen_src = codelen_src;
	INSTR->codelen_dst = codelen_dst;
	INSTR->opsize = opsize;
	INSTR->srcsize = srcsize;
	INSTR->proc = m32c_tst_size_g_srcdst;
	INSTR->proc();
}

/**
 **************************************************************
 * \fn void m32c_und(void)
 * Trigger an undefined instruction exception. 
 * v0
 **************************************************************
 */
void
m32c_und(void)
{
	uint16_t flg;
	flg = M32C_REG_FLG;

	M32C_SET_REG_FLG(M32C_REG_FLG & ~(M32C_FLG_I | M32C_FLG_D | M32C_FLG_U));

	M32C_REG_SP -= 2;
	M32C_Write16(flg, M32C_REG_SP);
	M32C_REG_SP -= 2;
	M32C_Write16(M32C_REG_PC >> 16, M32C_REG_SP);
	M32C_REG_SP -= 2;
	M32C_Write16(M32C_REG_PC & 0xffff, M32C_REG_SP);
	M32C_REG_PC = M32C_Read24(0xFFFFDC);
	dbgprintf("m32c_und not tested\n");
}

void
m32c_wait(void)
{
	dbgprintf("m32c_wait not implemented\n");
}

/**
 *************************************************************************
 * \fn void m32c_xchg_srcdst(void)
 * Exchange src and destination.
 * v0
 *************************************************************************
 */
static void
m32c_xchg_srcdst(void)
{
	int src = INSTR->Arg1;
	uint32_t Src, Dst;
	int size = INSTR->srcsize;
	int codelen_dst = INSTR->codelen_dst;
	INSTR->getdst(&Dst, M32C_INDEXSD());
	Src = am3bitreg_get(src, size);
	INSTR->setdst(Src, M32C_INDEXSD());
	am3bitreg_set(src, size, Dst);
	M32C_REG_PC += codelen_dst;
	dbgprintf("m32c_xchg_srcdst not tested\n");
}

void
m32c_setup_xchg_srcdst(void)
{
	int dst, src;
	int size, dstwsize;
	int codelen_dst;
	dst = ((ICODE16() >> 6) & 3) | ((ICODE16() >> 7) & 0x1c);
	src = ICODE16() & 7;
	if (ICODE16() & 0x100) {
		dstwsize = size = 2;
	} else {
		dstwsize = size = 1;
		ModOpsize(dst, &dstwsize);
	}
	INSTR->getdst = general_am_get(dst, size, &codelen_dst, GAM_ALL);
	INSTR->setdst = general_am_set(dst, dstwsize, &codelen_dst, GAM_ALL);
	INSTR->codelen_dst = codelen_dst;
	INSTR->opsize = dstwsize;
	INSTR->srcsize = size;
	INSTR->Arg1 = src;
	if (INSTR->nrMemAcc == 1) {
		INSTR->cycles = 4;
	}
	INSTR->proc = m32c_xchg_srcdst;
	INSTR->proc();
}

/**
 *****************************************************************
 * \fn void m32c_xchg_srcidst(void)
 * Exchange a source with an indirectly addressed destination.
 * v0
 *****************************************************************
 */
static void
m32c_xchg_srcidst(void)
{
	uint32_t Src, Dst;
	uint32_t DstP;
	int src = INSTR->Arg1;
	int size = INSTR->opsize;
	int codelen_dst = INSTR->codelen_dst;
	INSTR->getdstp(&DstP, 0);
	Src = am3bitreg_get(src, size);
	if (size == 2) {
		Dst = M32C_Read16(DstP);
		M32C_Write16(Src, DstP);
	} else {
		Dst = M32C_Read8(DstP);
		M32C_Write8(Src, DstP);
	}
	am3bitreg_set(src, size, Dst);
	M32C_REG_PC += codelen_dst;
	dbgprintf("m32c_xchg_srcidst not tested\n");
}

void
m32c_setup_xchg_srcidst(void)
{
	int dst, src;
	int size;
	int codelen_dst;
	dst = ((ICODE24() >> 6) & 3) | ((ICODE24() >> 7) & 0x1c);
	src = ICODE24() & 7;
	if (ICODE24() & 0x100) {
		size = 2;
	} else {
		size = 1;
	}
	INSTR->getdstp = general_am_get(dst, 4, &codelen_dst, GAM_ALL);
	INSTR->codelen_dst = codelen_dst;
	INSTR->opsize = size;
	INSTR->Arg1 = src;
	if (INSTR->nrMemAcc == 1) {
		INSTR->cycles = 7;
	}
	INSTR->proc = m32c_xchg_srcidst;
	INSTR->proc();
}

/**
 ***********************************************************************
 * \fn void m32c_xor_immdst(void)
 * Exclusive or of an immediate and a destination.
 * v0
 ***********************************************************************
 */
static void
m32c_xor_immdst(void)
{
	uint32_t Src, Dst, Result;
	int srcsize = INSTR->srcsize;
	int opsize = INSTR->opsize;
	int codelen_dst = INSTR->codelen_dst;
	INSTR->getdst(&Dst, M32C_INDEXSD());
	if (srcsize == 2) {
		Src = M32C_Read16((M32C_REG_PC + codelen_dst) & 0xffffff);
	} else {		/* if(srcsize == 1) */

		Src = M32C_Read8((M32C_REG_PC + codelen_dst) & 0xffffff);
	}
	Result = Dst ^ Src;
	INSTR->setdst(Result, M32C_INDEXSD());
	xor_flags(Result, opsize);
	M32C_REG_PC += codelen_dst + srcsize;
}

void
m32c_setup_xor_immdst(void)
{
	int dst;
	int srcsize, opsize;
	int codelen_dst;
	dst = ((ICODE16() >> 6) & 3) | ((ICODE16() >> 7) & 0x1c);
	if (ICODE16() & 0x100) {
		srcsize = opsize = 2;
	} else {
		srcsize = opsize = 1;
		ModOpsize(dst, &opsize);
	}
	INSTR->getdst = general_am_get(dst, opsize, &codelen_dst, GAM_ALL);
	INSTR->setdst = general_am_set(dst, opsize, &codelen_dst, GAM_ALL);
	INSTR->codelen_dst = codelen_dst;
	INSTR->opsize = opsize;
	INSTR->srcsize = srcsize;
	INSTR->proc = m32c_xor_immdst;
	INSTR->proc();
}

/**
 ****************************************************************
 * \fn void m32c_xor_srcdst(void)
 * Exclusive or of a source and a destination.
 * v0
 ****************************************************************
 */
static void
m32c_xor_srcdst(void)
{
	uint32_t Src, Dst, Result;
	int opsize = INSTR->opsize;
	int codelen_dst = INSTR->codelen_dst;
	int codelen_src = INSTR->codelen_src;
	INSTR->getsrc(&Src, M32C_INDEXSS());
	M32C_REG_PC += codelen_src;
	INSTR->getdst(&Dst, M32C_INDEXSD());
	Result = Dst ^ Src;
	INSTR->setdst(Result, M32C_INDEXSD());
	xor_flags(Result, opsize);
	M32C_REG_PC += codelen_dst;
}

void
m32c_setup_xor_srcdst(void)
{
	int dst, src;
	int srcsize, opsize;
	int codelen_dst;
	int codelen_src;
	dst = ((ICODE16() >> 6) & 3) | ((ICODE16() >> 7) & 0x1c);
	src = ((ICODE16() >> 4) & 3) | ((ICODE16() >> 10) & 0x1c);
	if (ICODE16() & 0x100) {
		srcsize = opsize = 2;
		ModOpsize(dst, &opsize);
	} else {
		srcsize = opsize = 1;
	}
	INSTR->getdst = general_am_get(dst, opsize, &codelen_dst, GAM_ALL);
	INSTR->setdst = general_am_set(dst, opsize, &codelen_dst, GAM_ALL);
	INSTR->getsrc = general_am_get(src, srcsize, &codelen_src, GAM_ALL);
	INSTR->codelen_dst = codelen_dst;
	INSTR->codelen_src = codelen_src;
	INSTR->opsize = opsize;
	INSTR->srcsize = srcsize;
	INSTR->proc = m32c_xor_srcdst;
	INSTR->proc();
}

/**
 ******************************************************************
 * \fn void m32c_xor_isrcdst(void)
 * Exclusive or of an indirect addressed source and a destination.
 ******************************************************************
 */
static void
m32c_xor_isrcdst(void)
{
	uint32_t Src, Dst, Result;
	uint32_t SrcP;
	int srcsize = INSTR->srcsize;
	int opsize = INSTR->opsize;
	int codelen_dst = INSTR->codelen_dst;
	int codelen_src = INSTR->codelen_src;
	INSTR->getsrcp(&SrcP, 0);
	if (srcsize == 2) {
		SrcP = (SrcP + M32C_INDEXWS()) & 0xffffff;
		Src = M32C_Read16(SrcP);
	} else {
		SrcP = (SrcP + M32C_INDEXBS()) & 0xffffff;
		Src = M32C_Read8(SrcP);
	}
	M32C_REG_PC += codelen_src;
	INSTR->getdst(&Dst, M32C_INDEXSD());
	Result = Dst ^ Src;
	INSTR->setdst(Result, M32C_INDEXSD());
	xor_flags(Result, opsize);
	M32C_REG_PC += codelen_dst;
	dbgprintf("m32c_xor_srcdst not tested\n");
}

void
m32c_setup_xor_isrcdst(void)
{
	int dst, src;
	int srcsize, opsize;
	int codelen_dst;
	int codelen_src;
	dst = ((ICODE24() >> 6) & 3) | ((ICODE24() >> 7) & 0x1c);
	src = ((ICODE24() >> 4) & 3) | ((ICODE24() >> 10) & 0x1c);
	if (ICODE24() & 0x100) {
		srcsize = opsize = 2;
	} else {
		srcsize = opsize = 1;
	}
	INSTR->getdst = general_am_get(dst, opsize, &codelen_dst, GAM_ALL);
	INSTR->setdst = general_am_set(dst, opsize, &codelen_dst, GAM_ALL);
	INSTR->getsrcp = general_am_get(src, 4, &codelen_src, GAM_ALL);
	INSTR->codelen_dst = codelen_dst;
	INSTR->codelen_src = codelen_src;
	INSTR->srcsize = srcsize;
	INSTR->opsize = opsize;

	INSTR->proc = m32c_xor_isrcdst;
	INSTR->proc();
}

/**
 ************************************************************************
 * \fn void m32c_xor_srcidst(void)
 * Exclusive or of a src with an indirectly addressed destination.
 * v0
 ************************************************************************
 */
static void
m32c_xor_srcidst(void)
{
	uint32_t Src, Dst, Result;
	uint32_t DstP;
	int size = INSTR->opsize;
	int codelen_dst = INSTR->codelen_dst;
	int codelen_src = INSTR->codelen_src;
	INSTR->getsrc(&Src, M32C_INDEXSS());
	M32C_REG_PC += codelen_src;
	INSTR->getdstp(&DstP, M32C_INDEXSD());
	if (size == 2) {
		DstP = (DstP + M32C_INDEXWD()) & 0xffffff;
		Dst = M32C_Read16(DstP);
		Result = Dst ^ Src;
		M32C_Write16(Result, DstP);
	} else {
		DstP = (DstP + M32C_INDEXBD()) & 0xffffff;
		Dst = M32C_Read8(DstP);
		Result = Dst ^ Src;
		M32C_Write8(Result, DstP);
	}
	xor_flags(Result, size);
	M32C_REG_PC += codelen_dst;
	dbgprintf("m32c_xor_srcdst not tested\n");
}

void
m32c_setup_xor_srcidst(void)
{
	int dst, src;
	int size;
	int codelen_dst;
	int codelen_src;
	dst = ((ICODE24() >> 6) & 3) | ((ICODE24() >> 7) & 0x1c);
	src = ((ICODE24() >> 4) & 3) | ((ICODE24() >> 10) & 0x1c);
	if (ICODE24() & 0x100) {
		size = 2;
	} else {
		size = 1;
	}
	INSTR->getdstp = general_am_get(dst, 4, &codelen_dst, GAM_ALL);
	INSTR->getsrc = general_am_get(src, size, &codelen_src, GAM_ALL);
	INSTR->codelen_dst = codelen_dst;
	INSTR->codelen_src = codelen_src;
	INSTR->opsize = size;
	INSTR->proc = m32c_xor_srcidst;
	INSTR->proc();
}

/**
 ************************************************************************
 * \fn void m32c_xor_isrcidst(void)
 * Exclusive or of an indirectly addressed source and an indirectly
 * addressed destination.
 * v0
 ************************************************************************
 */
static void
m32c_xor_isrcidst(void)
{
	uint32_t Src, Dst, Result;
	uint32_t DstP, SrcP;
	int size = INSTR->opsize;
	int codelen_dst = INSTR->codelen_dst;
	int codelen_src = INSTR->codelen_src;
	INSTR->getsrcp(&SrcP, M32C_INDEXSS());
	M32C_REG_PC += codelen_src;
	INSTR->getdstp(&DstP, M32C_INDEXSD());
	if (size == 2) {
		DstP = (DstP + M32C_INDEXWD()) & 0xffffff;
		SrcP = (SrcP + M32C_INDEXWS()) & 0xffffff;
		Dst = M32C_Read16(DstP);
		Src = M32C_Read16(SrcP);
		Result = Dst ^ Src;
		M32C_Write16(Result, DstP);
	} else {
		DstP = (DstP + M32C_INDEXBD()) & 0xffffff;
		SrcP = (SrcP + M32C_INDEXBS()) & 0xffffff;
		Dst = M32C_Read8(DstP);
		Src = M32C_Read8(SrcP);
		Result = Dst ^ Src;
		M32C_Write8(Result, DstP);
	}
	xor_flags(Result, size);
	M32C_REG_PC += codelen_dst;
	dbgprintf("m32c_xor_srcdst not tested\n");
}

void
m32c_setup_xor_isrcidst(void)
{
	int dst, src;
	int size;
	int codelen_dst;
	int codelen_src;
	dst = ((ICODE24() >> 6) & 3) | ((ICODE24() >> 7) & 0x1c);
	src = ((ICODE24() >> 4) & 3) | ((ICODE24() >> 10) & 0x1c);
	if (ICODE24() & 0x100) {
		size = 2;
	} else {
		size = 1;
	}
	INSTR->getdstp = general_am_get(dst, size, &codelen_dst, GAM_ALL);
	INSTR->getsrcp = general_am_get(src, 4, &codelen_src, GAM_ALL);
	INSTR->codelen_dst = codelen_dst;
	INSTR->codelen_src = codelen_src;
	INSTR->opsize = size;
	INSTR->proc = m32c_xor_isrcidst;
	INSTR->proc();
}

void
M32CInstructions_Init(void)
{
	init_condition_map();
}
