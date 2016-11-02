/*
 *************************************************************************************************
 * Floating point emulation double precision
 *
 * State:  64 Bit add, sub, mul div and sqrt are working. Algorithms are slow.
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

#include "softfloat.h"
#include "softfloat_int.h"
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include "compiler_extensions.h"

#define NEG(op) ((op)->isneg)
#define EXP(op) ((op)->exponent)
#define MAN(op)	((op)->mantissa)

typedef struct U128 {
	uint64_t low;
	uint64_t high;
} U128;

/**
 ******************************************************
 * \fn oid mul64x64to128(uint64_t a,uint64_t b,U128 *prod)
 * Multiply two 64 Bit Integers to a 128 Bit integer.
 ******************************************************
 */
static void 
mul64x64to128(uint64_t a,uint64_t b,U128 *prod)
{
	uint64_t alow,blow;
	uint64_t ahigh,bhigh;
	uint64_t middle1,middle2,sum;
	alow = (uint32_t)a;
	blow = (uint32_t)b;
	ahigh = a >> 32;
	bhigh = b >> 32;
	prod->low = alow * blow;
	prod->high = ahigh * bhigh;
	middle1 = alow * bhigh;
	middle2 = ahigh * blow;
	sum = prod->low + (middle1 << 32);
	if(sum < prod->low) {
		prod->high++;
	}
	prod->low = sum;
	prod->high += middle1 >> 32;

	sum = prod->low + (middle2 << 32);
	if(sum < prod->low) {
		prod->high++;
	}
	prod->low = sum;
	prod->high += middle2 >> 32;
}

/**
 ****************************************************************
 * \fn static void shift_right128_sticky(U128 *prod,int n)
 * Shift right a 128 Bit number n digits to the right.
 * n must be smaller than 128. Bits shifted out are or'ed into
 * the last bit (sticky bit).
 ***************************************************************
 */
static void 
shift_right128_sticky(U128 *number,int n)
{
	uint64_t shiftout; 
	if(n > 63) {
		if(n == 64) {
			shiftout = 0;
		} else {
			shiftout = number->high & ((UINT64_C(1) << (n - 64)) - 1);
		}
		number->low = (number->high >> (n - 64)) | !!shiftout | !!number->low;
	} else if(n) {
		shiftout = number->low & ((UINT64_C(1) << n) - 1);
		number->low = (number->low >> n) | !!shiftout;
		shiftout = number->high & ((UINT64_C(1) << n) - 1);
		number->high = number->high >> n;
		number->low |= shiftout << (64 - n);
	}
}

static inline uint64_t 
shift_right64_sticky(uint64_t x,unsigned int shift) 
{
	uint64_t result;
	if(shift < 64) {
		result = (x >> shift) | !!(x & ((UINT64_C(1) << shift) - 1));
	} else {
		result = !!(x);
	}
	return result;
}

/**
 **********************************************************
 * \fn void shift_left128(U128 *a,int n)
 * Shift left a 128 Bit number  by 0-127 bits.
 **********************************************************
 */
static void 
shift_left128(U128 *a,int n)
{
	if(n > 63) {
		a->high = a->low << (n - 64);
		a->low = 0;
	} else if(n) {
		a->high = a->high << n;	
		a->high |= (a->low >> (64 - n));
		a->low = a->low << n;
	}
}

/**
 ************************************************************
 * \fn bool greater128(U128 *a,U128 *b);
 * Compare two 128 Bit numbers and return true if
 * the first is greater than the second one.
 ************************************************************
 */
static bool 
greater128(U128 *a,U128 *b)
{
	if(a->high > b->high) {
		return true;
	} else if(a->high < b->high) {
		return false;
	}
	if(a->low > b->low) {
		return true;
	} else {
		return false;
	}
}

#ifndef clz32
#warning "Not using gcc builtin for clz32"
static int
clz32(uint32_t a)
{
        int i;
        for(i=31;i >= 0; i--) {
                if((a & (UINT32_C(1) << i)) !=  0) {
                        return 31 - i;
                }
        }
        return 32;
}
#endif

#ifndef clz64
#warning "Not using gcc builtin for clz64"
static inline int
clz64(uint64_t a)
{
        int i;
        for(i=63;i >= 0; i--) {
                if((a & (UINT64_C(1) << i)) !=  0) {
                        return 63 - i;
                }
        }
        return 64;
}
#endif

/**
 ********************************************************
 * \fn int clz128(U128 *a)
 * Count leading zeros of a 128 Bit number.
 * Should be replaced by a faster algorithm.
 ********************************************************
 */
static int 
clz128(U128 *a)
{
	int i;
	for(i=63;i >= 0; i--) {
		if((a->high & (UINT64_C(1) << i)) !=  0) {
			return 63 - i;
		}
	}
	for(i = 63;i >= 0;i--) {
		if((a->low & (UINT64_C(1) << i)) !=  0) {
			return 127 - i;
		}
	}
	return 128;
}


static void
NormalizeUp64(SoftFloatContext *sf,SFloat64_t *a,int roundbits) {
        int shift = clz64(MAN(a)) - 63 + 52 + roundbits;
        if(shift <= 0) {
                return;
        }
        EXP(a) -= shift;
        if(unlikely(EXP(a) <= -1022)) {
                shift -=  -1022 - EXP(a);
                EXP(a) = -1022;
                if(shift <= 0) {
                        return;
                }
        }
        MAN(a) <<= shift;
	dbgprintf(sf,"Normalize done %llx(%llx), roundbits %d\n",
		MAN(a),MAN(a) >> roundbits,roundbits);
}

/*
 **********************************************************
 * Increment the mantissa
 **********************************************************
 */
static inline void
mantUp(SoftFloatContext *sf,SFloat64_t *a,int roundbits)
{
	uint64_t mask = (1 << roundbits) - 1; 
	MAN(a) = (MAN(a) + (1 << roundbits)) & ~mask;
	if(unlikely(MAN(a) & (UINT64_C(1) << (53 + roundbits)))) {
		MAN(a) >>= 1;
		EXP(a)++;
		if(EXP(a) == 1024) {
			MAN(a) = 0;		
		} 
		dbgprintf(sf,"Increment Exponent after rounding to %d\n",EXP(a));
	} 
}

/*
 *******************************************************************
 * Round to nearest, or if both are same near to nearest even
 *******************************************************************
 */
static inline void
RoundNearestOrEven(SoftFloatContext *sf,SFloat64_t *a,int roundbits) {
	uint64_t mask = (1 << roundbits) - 1; 
	uint32_t mid = (1 << (roundbits - 1)); 
	uint32_t rem = MAN(a) & mask;
	if(rem  > mid) {
		dbgprintf(sf,"Round up\n");
		mantUp(sf,a,roundbits);
		return;
	} else if(rem < mid) {
		/* round_down */
		dbgprintf(sf,"Round down\n");
		MAN(a) = MAN(a) & ~mask;
		return;
	} else {
		int odd = MAN(a) & (1 << roundbits);
		dbgprintf(sf,"Round nearest even\n");
		if(odd) {
			mantUp(sf,a,roundbits);
			return;
		} else {
			MAN(a) = MAN(a) & ~mask;
			return;
		}
	}
}

/**
 **************************************************************************************************
 * \fn static inline void RoundTowardsZero(SoftFloatContext *sf,SFloat64_t *a,int roundbits); 
 * Round towards Zero. Zeros the rounding bits from the mantissa.
 **************************************************************************************************
 */
static inline void
RoundTowardsZero(SoftFloatContext *sf,SFloat64_t *a,int roundbits) {
	uint64_t mask = (1 << roundbits) - 1; 
	MAN(a) = MAN(a) & ~mask;
	return;
}

/**
 ******************************************************************************
 * \fn RoundUp(SoftFloatContext *sf,SFloat64_t *a,int roundbits); 
 * Increment the mantissa if at least one of the rounding bits is not 0.
******************************************************************************
 */
static inline void
RoundUp(SoftFloatContext *sf,SFloat64_t *a,int roundbits) {
	uint32_t mask = (1 << roundbits) - 1; 
	if((MAN(a) & mask) == 0) {
		return;
	}
	mantUp(sf,a,roundbits);
	return;
}

/**
 *****************************************************************************
 * \fn void RoundTowardsPlusInfinity(SoftFloatContext *sf,SFloat64_t *a,int roundbits) 
 * Round down the mantissa of negative numbers and round up the 
 * mantissa of positive numbers.
 *****************************************************************************
 */
static inline void
RoundTowardsPlusInfinity(SoftFloatContext *sf,SFloat64_t *a,int roundbits) 
{
	if(NEG(a)) {
		RoundTowardsZero(sf,a,roundbits);
	} else {
		RoundUp(sf,a,roundbits);
	}
}

static inline void
RoundTowardsMinusInfinity(SoftFloatContext *sf,SFloat64_t *a,int roundbits) 
{
	if(NEG(a)) {
		RoundUp(sf,a,roundbits);
	} else {
		RoundTowardsZero(sf,a,roundbits);
	}
}

/**
 **************************************************************************
 * \fn void RoundToAway(SoftFloatContext *sf,SFloat64_t *a,int roundbits); 
 * Round up if >= 0.5. This rounding mode is new in the 
 * p754/Draft 1.2.9 Standard
 **************************************************************************
 */
static inline void
RoundToAway(SoftFloatContext *sf,SFloat64_t *a,int roundbits) {
	uint64_t mask = (1 << roundbits) - 1; 
	uint32_t mid = (1 << (roundbits - 1)); 
	uint32_t rem = MAN(a) & mask;
	if(rem >= mid) {
		dbgprintf(sf,"Round up\n");
		mantUp(sf,a,roundbits);
	} else if(rem < mid) {
		dbgprintf(sf,"Round down\n");
		MAN(a) = MAN(a) & ~mask;
		return;
	} 
}

/**
 ********************************************************************
 * roundToIntegralTiesTEven
 * roundToIntegralTiesToAway
 * roundToIntegrapTowardZero
 * roundToIntegralTowardPositive
 * roundToIntegralTowardNegative
 * roundToIntegralExact
 * nextUp
 * nextDown
 ********************************************************************
 */
static void
Round64(SoftFloatContext *sf,SFloat64_t *a,int roundbits,SF_RoundingMode rm) {
	switch(rm) {
		case SFM_ROUND_NEAREST_EVEN:
			RoundNearestOrEven(sf,a,roundbits);
			break;

		case SFM_ROUND_ZERO:
			RoundTowardsZero(sf,a,roundbits);
			break;

		case SFM_ROUND_PLUS_INF:
			RoundTowardsPlusInfinity(sf,a,roundbits);
			break;

		case SFM_ROUND_MINUS_INF:
			RoundTowardsMinusInfinity(sf,a,roundbits);
			break;

		case SFM_ROUND_TO_AWAY:
			RoundToAway(sf,a,roundbits); 
			break;

		default:
			fprintf(stderr,"Rounding mode %d not implemented\n",sf->rounding_mode);
			exit(1);
	}
	if(EXP(a) >= 1024) {
		EXP(a) = 1024;
		MAN(a) = 0;
		return;
	}
}

/**
 *******************************************************************************
 *\fn void Add64(SoftFloatContext *sf,SFloat64_t *a,SFloat64_t *b,SFloat64_t *r) 
 * Add two softfloat numbers. 
 *******************************************************************************
 */
#define ADD_RNDBITS 2
static void 
Add64(SoftFloatContext *sf,SFloat64_t *a,SFloat64_t *b,SFloat64_t *r) 
{
	uint64_t manA,manB;
	manA = MAN(a) << ADD_RNDBITS;
	manB = MAN(b) << ADD_RNDBITS;
	if(EXP(a) == 1024) {
		dbgprintf(sf,"NAN A Case (exponent 1024)\n");
		EXP(r) = EXP(a);
		MAN(r) = MAN(a);
		NEG(r) = NEG(a);
		return;
	} else if(EXP(b) == 1024) {
		dbgprintf(sf,"NAN B Case (exponent 1024)\n");
		EXP(r) = EXP(b);
		MAN(r) = MAN(b);
		NEG(r) = NEG(b);
		return;
	}
	if(EXP(a) == EXP(b)) {
		dbgprintf(sf,"Standard case EXP(a) == EXP(b) == %d\n",EXP(a));
		MAN(r) = manA + manB;
		if(likely(MAN(r) & (UINT64_C(1) << (53 + ADD_RNDBITS)))) {
			MAN(r) = shift_right64_sticky(MAN(r),1);
			EXP(r) = EXP(a) + 1;
		} else {
			EXP(r) = EXP(a);
		}
	} else if(EXP(a) > EXP(b)) {
		uint32_t diff = EXP(a) - EXP(b);
		dbgprintf(sf,"SHR mantissa b by %d\n",diff);
		manB = shift_right64_sticky(manB,diff);
		MAN(r) = manA + manB;
		EXP(r) = EXP(a);
		if(MAN(r)  & (UINT64_C(1) << (53 + ADD_RNDBITS))) {
			MAN(r) = (MAN(r) >> 1) | (MAN(r) & 1);
			EXP(r)++;
		}
	} else if(EXP(b) > EXP(a)) {
		uint32_t diff = EXP(b) - EXP(a);
		dbgprintf(sf,"shr mantissa a by %d\n", diff);
		dbgprintf(sf,"bmanA %014llx(%014llx)\n"
			,manA,manA >> ADD_RNDBITS);
		manA = shift_right64_sticky(manA,diff);
		dbgprintf(sf,"manA %014llx(%014llx)\n"
			,manA,manA >> ADD_RNDBITS);
		MAN(r) = manA + manB;
		dbgprintf(sf,"result %014llx(%014llx)\n"
			,MAN(r),MAN(r) >> ADD_RNDBITS);
		EXP(r) = EXP(b);
		if(MAN(r)  & (UINT64_C(1) << (53 + ADD_RNDBITS))) {
			MAN(r) = (MAN(r) >> 1) | (MAN(r) & 1);
			EXP(r)++;
		}
	}
	Round64(sf,r,ADD_RNDBITS,sf->rounding_mode);
	MAN(r) >>= ADD_RNDBITS;
	NEG(r) = NEG(a);
}

/**
 ********************************************************************************************
 * \fn static void Sub64(SoftFloatContext *sf,SFloat64_t *a,SFloat64_t *b,SFloat64_t *r) 
 * Subtract two mantissas.
 ********************************************************************************************
 */
#define SUB_RNDBITS 3
static void 
Sub64(SoftFloatContext *sf,SFloat64_t *a,SFloat64_t *b,SFloat64_t *r) 
{
	uint64_t manA,manB;
	manA = MAN(a) << SUB_RNDBITS;
	manB = MAN(b) << SUB_RNDBITS;
	if(EXP(a) == 1024) {
		EXP(r) = EXP(a);
		MAN(r) = MAN(a);
		NEG(r) = NEG(a);
		return;
	} else if(EXP(b) == 1024) {
		MAN(r) = MAN(b);
		EXP(r) = EXP(b);
		/* Sign problems here ! */
		NEG(r) = NEG(b);
		return;
	}
	if(EXP(a) == EXP(b)) {
		EXP(r) = EXP(a);
		if(manA >= manB) {
			MAN(r) = manA - manB;
			NEG(r) = NEG(a);
		} else {
			MAN(r) = manB - manA;
			NEG(r) = !NEG(a);
		}
		dbgprintf(sf,"SUB case EXP(A) == EXP(B), MAN(A) %08llx, MAN(B) %08llx, MAN(r) %08llx\n",manA,manB,MAN(r));
	} else if(EXP(a) > EXP(b)) {
		uint32_t diff = EXP(a) - EXP(b);
		/* This makes manA bigger than manB */ 
		manB = shift_right64_sticky(manB,diff);
		MAN(r) = manA - manB;
		EXP(r) = EXP(a);
		NEG(r) = NEG(a);
		dbgprintf(sf,"SUB case EXP(A) > EXP(B) NEG(r): %d\n",NEG(r));
		dbgprintf(sf,"manA %013llx(%013llx) manB %013llx(%013llx)\n"
			,manA,manA >> SUB_RNDBITS,manB,manB >> SUB_RNDBITS);
		/* Normalize */
	} else if(EXP(b) > EXP(a)) {
		uint32_t diff = EXP(b) - EXP(a);
		dbgprintf(sf,"vor shift %014llx(%014llx)\n",
			manA,manA >> SUB_RNDBITS);

		/* This makes manB bigger than manA */ 
		manA = shift_right64_sticky(manA,diff);
		MAN(r) = manB - manA;
		EXP(r) = EXP(b);
		NEG(r) = !NEG(b);
		dbgprintf(sf,"SUB case EXP(B):%d > EXP(A):%d NEG(r): %d, sh %d\n",EXP(b),EXP(a),
			NEG(r),diff);
		dbgprintf(sf,"manA %013llx(%013llx) manB %013llx(%013llx)\n"
			,manA,manA >> SUB_RNDBITS,manB,manB >> SUB_RNDBITS);
	}
	NormalizeUp64(sf,r,SUB_RNDBITS);
	Round64(sf,r,SUB_RNDBITS,sf->rounding_mode);
	dbgprintf(sf,"SUB after round: %06llx\n",MAN(r) >> 6);
	MAN(r) >>= SUB_RNDBITS;
}

/**
 ****************************************************************************
 * Float64_t Float64_Add(SoftFloatContext *sf,Float64_t fla,Float64_t flb);
 * Add two 64 Bit numbers.
 ****************************************************************************
 */
Float64_t 
Float64_Add(SoftFloatContext *sf,Float64_t fla,Float64_t flb) 
{
        SFloat64_t a,b,r;
        UnpackFloat64(&a,fla);
        UnpackFloat64(&b,flb);
	if(NEG(&a) == NEG(&b)) {
		dbgprintf(sf,"Adding by ADD64\n");
		Add64(sf,&a,&b,&r);
	} else {
		dbgprintf(sf,"Adding by SUB64\n");
		NEG(&a) = !NEG(&a);
		Sub64(sf,&a,&b,&r);
		NEG(&r) = !NEG(&r);
	}
	return PackFloat64(&r);
}

Float64_t 
Float64_Sub(SoftFloatContext *sf,Float64_t fla,Float64_t flb) 
{
        SFloat64_t a,b,r;
        UnpackFloat64(&a,fla);
        UnpackFloat64(&b,flb);
	if(NEG(&a) == NEG(&b)) {
		Sub64(sf,&a,&b,&r);
	} else {
		NEG(&a) = !NEG(&a);
		Add64(sf,&a,&b,&r);
		NEG(&r) = !NEG(&r);
	}
	return PackFloat64(&r);
}

/**
 *********************************************************************************************
 * \fn void SFloat64_Mul(SoftFloatContext *sf,SFloat64_t *a,SFloat64_t *b,SFloat64_t *r) 
 * Multiply 64 Bit Softfloat numbers
 *********************************************************************************************
 */

#define MUL_RND_BITS (2)
static inline void
SFloat64_Mul(SoftFloatContext *sf,SFloat64_t *a,SFloat64_t *b,SFloat64_t *r) 
{
	U128 product;
	int lz,shift;
	/* NaN check */
	NEG(r) = NEG(a) ^ NEG(b);	
	if((MAN(a) == 0) || ((MAN(b) == 0))) {
		MAN(r) = 0;	
		EXP(r) = -1022;
		return;
	}
	mul64x64to128(MAN(a),MAN(b),&product);
	EXP(r) = EXP(a) + EXP(b) - 52 + MUL_RND_BITS;
	lz = clz128(&product);
	dbgprintf(sf,"Leading zeros %d\n",lz);
	shift = 128 - 53 - MUL_RND_BITS - lz;
	if(shift > 0) {
		shift_right128_sticky(&product,shift);
		EXP(r) += shift;
	} else if (shift < 0) {
		shift_left128(&product,-shift);
	}
	MAN(r) = product.low;
	/* To big ? Make infinite */
	if(EXP(r) >= 1024) {
		EXP(r) = 1024;
		MAN(r) = 0;
	/* To small ? make denormalized and possibly 0 */
	} else if(EXP(r) < -1022) {
		shift = -1022 - EXP(r);
		EXP(r) = -1022;
		MAN(r) = shift_right64_sticky(MAN(r),shift);
	}
	Round64(sf,r,MUL_RND_BITS,sf->rounding_mode); 
	MAN(r) >>= MUL_RND_BITS;
}

Float64_t 
Float64_Mul(SoftFloatContext *sf,Float64_t fla,Float64_t flb) 
{
        SFloat64_t a,b,r;
        UnpackFloat64(&a,fla);
        UnpackFloat64(&b,flb);
	if(EXP(&a) == 1024) {
                /* Infinite ? */
                if(MAN(&a) == 0) {
                        /* Infinite x 0 */
                        if((EXP(&b) == -1022) && (MAN(&b) == 0)) {
                                SF_PostException(sf,SFE_INV_OP);
                                /* result ? */
                        }
                }
                r = a;
        } else if(EXP(&b) == 1024) {
                /* Infinite */
                if(MAN(&b) == 0) {
			/* 0 x Infinite */
                        if(EXP(&a) == -1022 && (MAN(&a) == 0)) {
                                SF_PostException(sf,SFE_INV_OP);
                                /* result ? */
                        }
                }
                r = b;
        } else {
		SFloat64_Mul(sf,&a,&b,&r);
	}
	return PackFloat64(&r);
}

/**
 ****************************************************************************************
 * \fn void SFloat64_Div(SoftFloatContext *sf,SFloat64_t *a,SFloat64_t *b,SFloat64_t *r) 
 * Divide two softfloat 32 bit numbers.
 ****************************************************************************************
 */
#define DIV_RND_BITS (2)
static inline void
SFloat64_Div(SoftFloatContext *sf,SFloat64_t *a,SFloat64_t *b,SFloat64_t *r) 
{
	uint64_t div;
	uint64_t rem;
	uint64_t divisor;
	int digits;
	NEG(r) = NEG(a) ^ NEG(b);
	rem = MAN(a); 
	if(rem == 0) {
                MAN(r) = 0;
                EXP(r) = -1022;
                return ;
        }
	divisor = MAN(b);
	/* divisor = 0 -> return correctly signed infinite */
	if(divisor == 0) {
		MAN(r) = 0;
		EXP(r) = 128;
		SF_PostException(sf,SFE_DIV_ZERO);
		return;
	} 
	EXP(r) = EXP(a) - EXP(b);
	div = 0; digits = 0;
	/* Adjust divisor an remainder */
	while(rem >= (divisor << 1)) {
		divisor <<= 1;	
		EXP(r) += 1;
	}
	while(rem < divisor) {
		rem <<= 1;
		EXP(r) -= 1;
		dbgprintf(sf,"Adjust exp to %d, rem %08llx, b %08llx\n",EXP(r),rem,divisor);
	}
	do {
		div <<= 1;
		if(rem >= divisor) {
			rem -= divisor;
			div |= 1;
		}
		digits++;
		rem <<= 1;
		dbgprintf(sf,"Step %d, div %08llx rem %08llx\n",digits,div,rem);
	} while(digits < (53 + DIV_RND_BITS));
	if(rem) {
		div |= 1;
	}	
	dbgprintf(sf,"div result is %08llx\n",div);
	MAN(r) = div;
	/* To big ? make infinite */
	if(EXP(r) >= 1024) {
		EXP(r) = 1024;
		MAN(r) = 0;
	/* If to small try it with shifting */
	} else if(EXP(r) < -1022) {
		int shift = -1022 - EXP(r);
		EXP(r) = -1022;
		MAN(r) = shift_right64_sticky(MAN(r),shift);
	}
	Round64(sf,r,DIV_RND_BITS,sf->rounding_mode); 
	MAN(r) >>= DIV_RND_BITS;	
}

Float64_t 
Float64_Div(SoftFloatContext *sf,Float64_t fla,Float64_t flb) 
{
        SFloat64_t a,b,r;
        UnpackFloat64(&a,fla);
        UnpackFloat64(&b,flb);
	if(EXP(&a) == 1024) {
                /* Infinity */
                if(MAN(&a) == 0) {
                        /* Inf div Inf */
                        if((EXP(&b) == 1024) && (MAN(&b) == 0)) {
                                SF_PostException(sf,SFE_INV_OP);
                        }
                }
                r = a;
        } else if(EXP(&b) == 1024) {
                r = b;
        } else if((EXP(&a) == -1022) && (EXP(&b) == -1022)
                && (MAN(&a) == 0) && MAN(&b) == 0) {
                SF_PostException(sf,SFE_INV_OP);
                /* Result ? */
                r = a;
        } else {
		SFloat64_Div(sf,&a,&b,&r);
	}
	return PackFloat64(&r);
}

Float64_t
Float64_Rem(SoftFloatContext *sf,Float64_t fla,Float64_t flb) 
{
	fprintf(stderr,"Error: Using unimplemented Floating point Remainder\n");
	exit(1);
}

/**
 ************************************************************************
 * \fn SFloat64_Sqrt(SoftFloatContext *sf,SFloat64_t *a,SFloat64_t *r) 
 * Calculate the square root.
 * Don't know a better algorithm with exact rounding. 
 ************************************************************************
 */
#define SQRT_RND_BITS (2)
static inline void
SFloat64_Sqrt(SoftFloatContext *sf,SFloat64_t *a,SFloat64_t *r) 
{
	U128 rem;
	uint64_t manA;
	uint64_t sqrt;
	U128 product;
	int i;
	NEG(r) = NEG(a);
	/* NaN/Inf Check */
	if(EXP(a) == 1024) {
		EXP(r) = EXP(a);	
		MAN(r) = MAN(a);
		return;
	}
	/* First check for 0 mantissa because sqrt(-0) is allowed */
	if(MAN(a) == 0) {
		MAN(r) = 0;
		EXP(r) = -1022;
		return;
	} else if(NEG(a)) {
		SF_PostException(sf,SFE_INV_OP);
		EXP(r) = 1024;
		MAN(r) = UINT64_C(1) << 51;
		return;
	}
	EXP(r) = EXP(a);
	manA = MAN(a);
	while((manA & (UINT64_C(0xfff) << 52)) == 0) {
		manA <<= 1;
		EXP(r)--;
	} 
	/* Make radix-4 */
	if(EXP(r) & 1) {
		manA <<= 1;
	}
	EXP(r) >>= 1; 
	rem.low = manA;
	rem.high = 0;
	sqrt = 0;
	shift_left128(&rem,2*SQRT_RND_BITS + 52);
	for(i = 52 + SQRT_RND_BITS;i > 0; i--) {
		sqrt |= (UINT64_C(1) << i);
		mul64x64to128(sqrt,sqrt,&product);
		dbgprintf(sf,"step %d: %016llx-%016llx\n",
			i,product.high,product.low);
		if(greater128(&product,&rem)) {
			sqrt &= ~(UINT64_C(1) << i);
		}
	}
	mul64x64to128(sqrt,sqrt,&product);
	if(greater128(&rem,&product)) {
		sqrt |= 1;
	}
	MAN(r) = sqrt;	
	Round64(sf,r,SQRT_RND_BITS,sf->rounding_mode); 
	MAN(r) >>= SQRT_RND_BITS;	
}

Float64_t 
Float64_Sqrt(SoftFloatContext *sf,Float64_t fla) 
{
        SFloat64_t a,r;
        UnpackFloat64(&a,fla);
	SFloat64_Sqrt(sf,&a,&r);
	return PackFloat64(&r);
}

static inline int
SFloat64_Cmp(SoftFloatContext *sf,SFloat64_t *a,SFloat64_t *b)
{
        bool abs_greater;
        if((EXP(a) == 1024) && (MAN(a) != 0)) {
                return SFC_UNORDERD;
        }
        if((EXP(b) == 1024) && (MAN(b) != 0)) {
                return SFC_UNORDERD;
        }
        /* With Different sign its simple */
        if(NEG(a) != NEG(b)) {
                /* 0 and -0 are the same */
                if((MAN(a) == 0) && (MAN(b) == 0)) {
                        return SFC_EQUAL;
                }
                if(NEG(a)) {
                        return SFC_LESS;
                } else {
                        return SFC_GREATER;
                }
        }
        /* Now a and b have the same sign */
        if(EXP(a) > EXP(b)) {
                abs_greater = true;
        } else if(EXP(b) > EXP(a)) {
                abs_greater = false;
        } else if(MAN(a) > MAN(b)) {
                abs_greater = true;
        } else if(MAN(a) < MAN(b)) {
                abs_greater = false;
        } else {
                return SFC_EQUAL;
        }
        if(abs_greater == !!NEG(a)) {
                return SFC_LESS;
        } else {
                return SFC_GREATER;
        }
}

int
Float64_Cmp(SoftFloatContext *sf,Float32_t fla,Float32_t flb)
{
        SFloat64_t a,b;
        UnpackFloat64(&a,fla);
        UnpackFloat64(&b,flb);
        return SFloat64_Cmp(sf,&a,&b);
}


/**
 ****************************************************************
 * Warning, The Conversion routines are not writen to comply to
 * IEEE754 but tested against results of gcc softfloat.
 ****************************************************************
 */
#define TOINT64_RNDBITS 2
static inline int64_t
SFloat64_ToInt64(SoftFloatContext *sf,SFloat64_t *a,SF_RoundingMode rm)
{
   
	int64_t result;
	int rshift = 52 - EXP(a);	
	dbgprintf(sf,"Rshift is %d\n",rshift);
	if((rshift) >= 0) {
		MAN(a) <<= TOINT64_RNDBITS;
		MAN(a) = shift_right64_sticky(MAN(a),rshift);
		Round64(sf,a,TOINT64_RNDBITS,rm);
		MAN(a) >>= TOINT64_RNDBITS;
		if(NEG(a)) {
			return -(int64_t)MAN(a);	
		} else {
			return MAN(a);
		}
	} else {
		if(NEG(a)) {
			result = -(int64_t)MAN(a);
		} else {
			result = MAN(a);
		}
		if((-rshift) < (64 - 53)) {
			result = result << (-rshift);	
		} else {
			/* Intel with GNU-C gives this result */
			result = (UINT64_C(1) << 63);
		}
		return result;	
	}
}

/**
 ************************************************************************
 * \fn int64_t SFloat64_ToUInt64(SoftFloatContext *sf,SFloat32_t *a,SF_RoundingMode rm)
 * Contert a Softfloat64 to an uint64_t.
 ************************************************************************
 */
static inline uint64_t
SFloat64_ToUInt64(SoftFloatContext *sf,SFloat64_t *a,SF_RoundingMode rm)
{
   
	uint64_t result;
	int rshift = 52 - EXP(a);	
	dbgprintf(sf,"Rshift is %d\n",rshift);
	if(NEG(a)) {
		dbgprintf(sf,"Uint can not be Neg\n");
	}
	if((rshift) >= 0) {
		MAN(a) <<= TOINT64_RNDBITS;
		MAN(a) = shift_right64_sticky(MAN(a),rshift);
		Round64(sf,a,TOINT64_RNDBITS,rm);
		MAN(a) >>= TOINT64_RNDBITS;
		if(NEG(a)) {
			return -MAN(a);
		} else {
			return MAN(a);
		}
	} else {
		result = MAN(a);
		if((-rshift) < (64 - 52)) {
			result = result << (-rshift);	
		} else {
			if(-rshift < 64) {
				result = result << (-rshift);	
			} else {
				result = 0;
			}
		}
		if(NEG(a)) {
			return -result;
		} else {
			return result;	
		}
	}
}

int64_t
Float64_ToInt64(SoftFloatContext *sf,Float64_t fla,SF_RoundingMode rm)
{
	SFloat64_t a;
	UnpackFloat64(&a,fla);
	return SFloat64_ToInt64(sf,&a,rm);
}

uint64_t
Float64_ToUInt64(SoftFloatContext *sf,Float64_t fla,SF_RoundingMode rm)
{
	SFloat64_t a;
	UnpackFloat64(&a,fla);
	return SFloat64_ToUInt64(sf,&a,rm);
}

int32_t
Float64_ToInt32(SoftFloatContext *sf,Float64_t fla,SF_RoundingMode rm)
{
	int64_t result;
	SFloat64_t a;
	UnpackFloat64(&a,fla);
	result = SFloat64_ToInt64(sf,&a,rm);
	dbgprintf(sf,"Result64: %016llx\n",result);
	if(result < INT64_C(-0x80000000)) {
		return 0x80000000;	
	} else if(result > INT64_C(0x7fffffff)) {
		return 0x80000000;	
	} else {
		return result;
	}
}

#define FLTOINT64_RNDBITS 2
Float64_t
Float64_FromInt64(SoftFloatContext *sf,int64_t in,SF_RoundingMode rm)
{
	SFloat64_t a;
	uint64_t manA;
	int lz;
	int rshift;
	if(in > 0) {
		NEG(&a)	= false;
		manA = in;
	} else if(in == 0) {
		return 0;
	} else {
		NEG(&a) = true;
		manA = -in;	
	}
	lz = clz64(manA);	
	rshift = (64 - 53) - lz;
	dbgprintf(sf,"CLZ %d, rshift %d, man %016llX\n",lz,rshift,manA); 
	if(rshift <= 0) {
		EXP(&a) = 52 - (-rshift);	
		MAN(&a) = manA << (-rshift);	
	} else {
		EXP(&a) = 52 + rshift;
		if(rshift <= FLTOINT64_RNDBITS) {
			MAN(&a) = manA; 
			Round64(sf,&a,rshift,rm);
			MAN(&a) >>= rshift;
		} else {
			MAN(&a) = shift_right64_sticky(manA,rshift - FLTOINT64_RNDBITS);
			dbgprintf(sf,"shrsticky %016llx\n",MAN(&a));
			Round64(sf,&a,FLTOINT64_RNDBITS,rm);
			dbgprintf(sf,"rounded %016llx\n",MAN(&a));
			MAN(&a) >>= FLTOINT64_RNDBITS;
		}
	} 	
	return PackFloat64(&a);
}

Float64_t
Float64_FromUInt64(SoftFloatContext *sf,uint64_t manA,SF_RoundingMode rm)
{
	SFloat64_t a;
	int lz;
	int rshift;

	NEG(&a)	= false;
	lz = clz64(manA);	
	rshift = (64 - 53) - lz;
	dbgprintf(sf,"CLZ %d, rshift %d, man %016llX\n",lz,rshift,manA); 
	if(rshift <= 0) {
		EXP(&a) = 52 - (-rshift);	
		MAN(&a) = manA << (-rshift);	
	} else {
		EXP(&a) = 52 + rshift;
		if(rshift <= FLTOINT64_RNDBITS) {
			MAN(&a) = manA; 
			Round64(sf,&a,rshift,rm);
			MAN(&a) >>= rshift;
		} else {
			MAN(&a) = shift_right64_sticky(manA,rshift - FLTOINT64_RNDBITS);
			Round64(sf,&a,FLTOINT64_RNDBITS,rm);
			MAN(&a) >>= FLTOINT64_RNDBITS;
		}
	} 	
	return PackFloat64(&a);
}

/**
 *****************************************************************************
 * \fn Float64_t Float64_FromFloat32(SoftFloatContext *sf,Float32_t fl) 
 * Conversion from 32 bit float to 64 bit double
 *****************************************************************************
 */

Float64_t
Float64_FromFloat32(SoftFloatContext *sf,Float32_t fl) 
{
	SFloat32_t src;
	SFloat64_t dst;
        UnpackFloat32(&src,fl);
	NEG(&dst) = NEG(&src);
	if(EXP(&src) == 128) {
		/* Infinite and NaN */
		EXP(&dst) = 1024;
		MAN(&dst) = (uint64_t)MAN(&src) << (52 - 23);
	} else if(EXP(&src) == -126) {
		/* subnormal and 0 */
		int lz = clz32(MAN(&src));
		int shift = lz - (32 - 24);  
		dbgprintf(sf,"subnormal, lz %d shift %d\n",lz,shift);
		EXP(&dst) = EXP(&src) - shift;
		MAN(&dst) = (uint64_t)MAN(&src) << (shift + (52 - 23));
	} else {
		EXP(&dst) = EXP(&src);
		MAN(&dst) = (uint64_t)MAN(&src) << (52 - 23);
	}
	return PackFloat64(&dst);
}

