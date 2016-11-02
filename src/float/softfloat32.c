/*
 *******************************************************************************
 * Floating point emulation single precision
 *
 * State:  32 Bit add, sub, mul div and sqrt are working. Algorithms are slow.
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
#include <inttypes.h>
#include "softfloat.h"
#include "softfloat_int.h"
#include "compiler_extensions.h"

#define NEG(op) ((op)->isneg)
#define EXP(op) ((op)->exponent)
#define MAN(op)	((op)->mantissa)

/**
 *****************************************************************
 * Check for nan in functions with two arguments and one result
 * Returns true if a nan is found and copys the NaN to the
 * result.
 *****************************************************************
 */
static inline bool
NaN_Check2(SFloat32_t * a, SFloat32_t * b, SFloat32_t * r)
{
    if ((EXP(a) == 128) && MAN(a)) {
        *r = *a;
        return true;
    } else if ((EXP(b) == 128) && MAN(a)) {
        *r = *b;
        return true;
    }
    return false;
}

#ifndef clz64
#warning "Not using gcc builtin for clz64"
static int
clz64(uint64_t a)
{
    int i;
    for (i = 63; i >= 0; i--) {
        if ((a & (UINT64_C(1) << i)) != 0) {
            return 63 - i;
        }
    }
    return 64;
}
#endif

#ifndef clz32
#warning "Not using gcc builtin for clz32"
static int
clz32(uint32_t a)
{
    int i;
    for (i = 31; i >= 0; i--) {
        if ((a & (UINT32_C(1) << i)) != 0) {
            return 31 - i;
        }
    }
    return 32;
}
#endif

static inline uint32_t
shift_right32_sticky(uint32_t x, unsigned int shift)
{
    uint32_t result;
    if (shift < 32) {
        result = (x >> shift) | ! !(x & ((1 << shift) - 1));
    } else {
        result = ! !(x);
    }
    return result;
}

static inline uint64_t
shift_right64_sticky(uint64_t x, unsigned int shift)
{
    uint64_t result;
    if (shift < 64) {
        result = (x >> shift) | ! !(x & ((UINT64_C(1) << shift) - 1));
    } else {
        result = ! !(x);
    }
    return result;
}

static void
NormalizeUp32(SoftFloatContext * sf, SFloat32_t * a, int roundbits)
{
    int shift;
    if (MAN(a) == 0) {
        EXP(a) = -127;
        return;
    }
    shift = clz32(MAN(a)) - 31 + 23 + roundbits;
    if (shift <= 0) {
        return;
    }
    EXP(a) -= shift;
    if (EXP(a) <= -126) {
        shift -= -126 - EXP(a);
        EXP(a) = -126;
        if (shift <= 0) {
            return;
        }
    }
    MAN(a) <<= shift;
    dbgprintf(sf, "Normalize done %06x(%06x), roundbits %d, mask %06x\n",
              MAN(a), MAN(a) >> roundbits, roundbits, (1 << (23 + roundbits)));
}

/*
 **********************************************************
 * Increment the mantissa
 **********************************************************
 */
static inline void
mantUp(SoftFloatContext * sf, SFloat32_t * a, int roundbits)
{
    uint32_t mask = (1 << roundbits) - 1;

    MAN(a) = (MAN(a) + (1 << roundbits)) & ~mask;
    if (unlikely(MAN(a) & (1 << (24 + roundbits)))) {
        MAN(a) >>= 1;
        EXP(a)++;
        if (EXP(a) == 128) {
            MAN(a) = 0;
        }
        dbgprintf(sf, "Increment Exponent after rounding to %d\n", EXP(a));
    }
}

/*
 *******************************************************************
 * Round to nearest, or if both are same near to nearest even
 *******************************************************************
 */
static inline void
RoundNearestOrEven(SoftFloatContext * sf, SFloat32_t * a, int roundbits)
{
    uint32_t mask = (1 << roundbits) - 1;
    uint32_t mid = (1 << (roundbits - 1));
    uint32_t rem = MAN(a) & mask;
    if (rem > mid) {
        dbgprintf(sf, "Round up\n");
        mantUp(sf, a, roundbits);
        return;
    } else if (rem < mid) {
        /* round_down */
        dbgprintf(sf, "Round down\n");
        MAN(a) = MAN(a) & ~mask;
        return;
    } else {
        int odd = MAN(a) & (1 << roundbits);
        dbgprintf(sf, "Round nearest even\n");
        if (odd) {
            mantUp(sf, a, roundbits);
            return;
        } else {
            MAN(a) = MAN(a) & ~mask;
            return;
        }
    }
}

/**
 **************************************************************************************************
 * \fn static inline void RoundTowardsZero(SoftFloatContext *sf,SFloat32_t *a,int roundbits); 
 * Round towards Zero. Zeros the rounding bits from the mantissa.
 **************************************************************************************************
 */
static inline void
RoundTowardsZero(SoftFloatContext * sf, SFloat32_t * a, int roundbits)
{
    uint32_t mask = (1 << roundbits) - 1;
    MAN(a) = MAN(a) & ~mask;
    return;
}

/**
 ******************************************************************************
 * \fn RoundUp(SoftFloatContext *sf,SFloat32_t *a,int roundbits); 
 * Increment the mantissa if at least one of the rounding bits is not 0.
******************************************************************************
 */
static inline void
RoundUp(SoftFloatContext * sf, SFloat32_t * a, int roundbits)
{
    uint32_t mask = (1 << roundbits) - 1;
    if ((MAN(a) & mask) == 0) {
        return;
    }
    mantUp(sf, a, roundbits);
    return;
}

/**
 *****************************************************************************
 * \fn void RoundTowardsPlusInfinity(SoftFloatContext *sf,SFloat32_t *a,int roundbits) 
 * Round down the mantissa of negative numbers and round up the 
 * mantissa of positive numbers.
 *****************************************************************************
 */
static inline void
RoundTowardsPlusInfinity(SoftFloatContext * sf, SFloat32_t * a, int roundbits)
{
    if (NEG(a)) {
        RoundTowardsZero(sf, a, roundbits);
    } else {
        RoundUp(sf, a, roundbits);
    }
}

static inline void
RoundTowardsMinusInfinity(SoftFloatContext * sf, SFloat32_t * a, int roundbits)
{
    if (NEG(a)) {
        RoundUp(sf, a, roundbits);
    } else {
        RoundTowardsZero(sf, a, roundbits);
    }
}

/**
 **************************************************************************
 * \fn void RoundToAway(SoftFloatContext *sf,SFloat32_t *a,int roundbits); 
 * Round up if >= 0.5. This rounding mode is new in the 
 * p754/Draft 1.2.9 Standard
 **************************************************************************
 */
static inline void
RoundToAway(SoftFloatContext * sf, SFloat32_t * a, int roundbits)
{
    uint32_t mask = (1 << roundbits) - 1;
    uint32_t mid = (1 << (roundbits - 1));
    uint32_t rem = MAN(a) & mask;
    if (rem >= mid) {
        dbgprintf(sf, "Round up\n");
        mantUp(sf, a, roundbits);
    } else if (rem < mid) {
        dbgprintf(sf, "Round down\n");
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
Round32(SoftFloatContext * sf, SFloat32_t * a, int roundbits, SF_RoundingMode rm)
{
    switch (rm) {
        case SFM_ROUND_NEAREST_EVEN:
            RoundNearestOrEven(sf, a, roundbits);
            break;

        case SFM_ROUND_ZERO:
            RoundTowardsZero(sf, a, roundbits);
            break;

        case SFM_ROUND_PLUS_INF:
            RoundTowardsPlusInfinity(sf, a, roundbits);
            break;

        case SFM_ROUND_MINUS_INF:
            RoundTowardsMinusInfinity(sf, a, roundbits);
            break;

        case SFM_ROUND_TO_AWAY:
            RoundToAway(sf, a, roundbits);
            break;

        default:
            fprintf(stderr, "Rounding mode %d not implemented\n", sf->rounding_mode);
            exit(1);
    }
    if (EXP(a) >= 128) {
        EXP(a) = 128;
        MAN(a) = 0;
        SF_PostException(sf, SFE_OVERFLOW);
        return;
    }
}

/**
 *******************************************************************************
 *\fn void Add32(SoftFloatContext *sf,SFloat32_t *a,SFloat32_t *b,SFloat32_t *r) 
 * Add two softfloat numbers. 
 *******************************************************************************
 */
#define ADD_RNDBITS 2
static void
Add32(SoftFloatContext * sf, SFloat32_t * a, SFloat32_t * b, SFloat32_t * r)
{
    uint32_t manA, manB;
    manA = MAN(a) << ADD_RNDBITS;
    manB = MAN(b) << ADD_RNDBITS;
    if (EXP(a) == EXP(b)) {
        dbgprintf(sf, "Standard case EXP(a) == EXP(b) == %d\n", EXP(a));
        MAN(r) = manA + manB;
        dbgprintf(sf, "%08x + %08x = %08x\n", manA, manB, MAN(r));
        if (likely(MAN(r) & (1 << (24 + ADD_RNDBITS)))) {
            MAN(r) = shift_right32_sticky(MAN(r), 1);
            EXP(r) = EXP(a) + 1;
        } else {
            EXP(r) = EXP(a);
        }
    } else if (EXP(a) > EXP(b)) {
        uint32_t diff = EXP(a) - EXP(b);
        dbgprintf(sf, "shr mantissa b by %d\n", diff);
        manB = shift_right32_sticky(manB, diff);
        MAN(r) = manA + manB;
        EXP(r) = EXP(a);
        if (MAN(r) & (1 << (24 + ADD_RNDBITS))) {
            MAN(r) = shift_right32_sticky(MAN(r), 1);
            EXP(r)++;
        }
    } else if (EXP(b) > EXP(a)) {
        uint32_t diff = EXP(b) - EXP(a);
        manA = shift_right32_sticky(manA, diff);
        MAN(r) = manA + manB;
        dbgprintf(sf, "shr mantissa a by %d: %08x(%06x), B: %08x(%06x) res %08x(%06x)\n",
                  diff, manA, manA >> ADD_RNDBITS, manB, manB >> ADD_RNDBITS,
                  MAN(r), MAN(r) >> ADD_RNDBITS);
        dbgprintf(sf, "EXP(a) %d, EXP(b) %d\n", EXP(a), EXP(b));
        EXP(r) = EXP(b);
        if (MAN(r) & (1 << (24 + ADD_RNDBITS))) {
            MAN(r) = shift_right32_sticky(MAN(r), 1);
            EXP(r)++;
        }
    }
    Round32(sf, r, ADD_RNDBITS, sf->rounding_mode);
    MAN(r) >>= ADD_RNDBITS;
    NEG(r) = NEG(a);
}

/**
 ********************************************************************************************
 * \fn static void Sub32(SoftFloatContext *sf,SFloat32_t *a,SFloat32_t *b,SFloat32_t *r) 
 * Subtract two mantissas.
 ********************************************************************************************
 */
#define SUB_RNDBITS 3
static void
Sub32(SoftFloatContext * sf, SFloat32_t * a, SFloat32_t * b, SFloat32_t * r)
{
    uint32_t manA, manB;
    manA = MAN(a) << SUB_RNDBITS;
    manB = MAN(b) << SUB_RNDBITS;
    if (EXP(a) == EXP(b)) {
        EXP(r) = EXP(a);
        if (manA >= manB) {
            MAN(r) = manA - manB;
            NEG(r) = NEG(a);
        } else {
            MAN(r) = manB - manA;
            NEG(r) = !NEG(a);
        }
        dbgprintf(sf,
                  "SUB case EXP(A) == EXP(B), MAN(A) %08x, MAN(B) %08x, MAN(r) %08x, EXP(a) %d, EXP(r) %d\n",
                  manA, manB, MAN(r), EXP(a), EXP(r));
    } else if (EXP(a) > EXP(b)) {
        uint32_t diff = EXP(a) - EXP(b);
        /* This makes manA bigger than manB */
        manB = shift_right32_sticky(manB, diff);
        MAN(r) = manA - manB;
        EXP(r) = EXP(a);
        NEG(r) = NEG(a);
        dbgprintf(sf, "SUB case EXP(A) > EXP(B) NEG(r): %d\n", NEG(r));
        /* Normalize */
    } else if (EXP(b) > EXP(a)) {
        uint32_t diff = EXP(b) - EXP(a);
        /* This makes manB bigger than manA */
        manA = shift_right32_sticky(manA, diff);
        MAN(r) = manB - manA;
        EXP(r) = EXP(b);
        NEG(r) = !NEG(b);
        dbgprintf(sf, "SUB case EXP(B) > EXP(A) NEG(r): %d\n", NEG(r));
    } else {
        /* Make the comiler quiet about uninitialzed r */
        fprintf(stderr, "Reached the unreachable code\n");
        r = b;
    }
    NormalizeUp32(sf, r, SUB_RNDBITS);
    Round32(sf, r, SUB_RNDBITS, sf->rounding_mode);
    dbgprintf(sf, "SUB after round: %06x\n", MAN(r) >> 6);
    MAN(r) >>= SUB_RNDBITS;
}

Float32_t
Float32_Add(SoftFloatContext * sf, Float32_t fla, Float32_t flb)
{
    SFloat32_t a, b, r;
    UnpackFloat32(&a, fla);
    UnpackFloat32(&b, flb);
    if (NaN_Check2(&a, &b, &r) == true) {
        /* Do nothing */
    } else if (NEG(&a) == NEG(&b)) {
        dbgprintf(sf, "Adding by ADD32\n");
        Add32(sf, &a, &b, &r);
    } else {
        if ((EXP(&a) == 128) && (EXP(&b) == 128)) {
            dbgprintf(sf, "ADD32 of Inf of different sign\n");
            EXP(&r) = 128;
            MAN(&r) = 1 << 22;
            NEG(&r) = true;
        } else {
            dbgprintf(sf, "Adding by SUB32\n");
            NEG(&a) = !NEG(&a);
            Sub32(sf, &a, &b, &r);
            NEG(&r) = !NEG(&r);
        }
    }
    return PackFloat32(&r);
}

Float32_t
Float32_Sub(SoftFloatContext * sf, Float32_t fla, Float32_t flb)
{
    SFloat32_t a, b, r;
    UnpackFloat32(&a, fla);
    UnpackFloat32(&b, flb);
    if (NaN_Check2(&a, &b, &r) == true) {
        /* Do nothing */
    } else if (NEG(&a) == NEG(&b)) {
        if ((EXP(&a) == 128) && (EXP(&b) == 128)) {
            dbgprintf(sf, "SUB32 of Inf of same sign\n");
            EXP(&r) = 128;
            MAN(&r) = 1 << 22;
            NEG(&r) = true;
        } else {
            Sub32(sf, &a, &b, &r);
        }
    } else {
        NEG(&a) = !NEG(&a);
        Add32(sf, &a, &b, &r);
        NEG(&r) = !NEG(&r);
    }
    return PackFloat32(&r);
}

/**
 *********************************************************************************************
 * \fn void SFloat32_Mul(SoftFloatContext *sf,SFloat32_t *a,SFloat32_t *b,SFloat32_t *r) 
 * Multiply 32 Bit Softfloat numbers.
 *********************************************************************************************
 */
#define MUL_RND_BITS (2)
static inline void
SFloat32_Mul(SoftFloatContext * sf, SFloat32_t * a, SFloat32_t * b, SFloat32_t * r)
{
    uint64_t product;
    int lz, shift;
    /* NaN check */
    NEG(r) = NEG(a) ^ NEG(b);
    if ((MAN(a) == 0) || ((MAN(b) == 0))) {
        MAN(r) = 0;
        EXP(r) = -126;
        return;
    }
    product = (uint64_t) MAN(a) * (uint64_t) MAN(b);
    EXP(r) = EXP(a) + EXP(b) - 23 + MUL_RND_BITS;

    lz = clz64(product);
    dbgprintf(sf, "Leading zeros %d\n", lz);
    shift = 64 - 24 - MUL_RND_BITS - lz;
    if (shift > 0) {
        product = shift_right64_sticky(product, shift);
    } else if (shift < 0) {
        product = product << -shift;
    }
    EXP(r) += shift;
    MAN(r) = product;
    /* To big ? Make infinite */
    if (EXP(r) >= 128) {
        EXP(r) = 128;
        MAN(r) = 0;
        SF_PostException(sf, SFE_OVERFLOW);
        /* To small ? make denormalized and possibly 0 */
    } else if (EXP(r) < -126) {
        shift = -126 - EXP(r);
        EXP(r) = -126;
        MAN(r) = shift_right32_sticky(MAN(r), shift);
    }
    Round32(sf, r, MUL_RND_BITS, sf->rounding_mode);
    MAN(r) >>= MUL_RND_BITS;
}

Float32_t
Float32_Mul(SoftFloatContext * sf, Float32_t fla, Float32_t flb)
{
    SFloat32_t a, b, r;
    UnpackFloat32(&a, fla);
    UnpackFloat32(&b, flb);
    if (EXP(&a) == 128) {
        /* Infinite ? */
        if (MAN(&a) == 0) {
            /* Infinite * 0 */
            if ((EXP(&b) == -126) && (MAN(&b) == 0)) {
                SF_PostException(sf, SFE_INV_OP);
                /* result ? */
            }
        }
        r = a;
    } else if (EXP(&b) == 128) {
        /* Infinite */
        if (MAN(&b) == 0) {
            if (EXP(&a) == -126 && (MAN(&a) == 0)) {
                SF_PostException(sf, SFE_INV_OP);
                /* result ? */
            }
        }
        r = b;
    } else {
        SFloat32_Mul(sf, &a, &b, &r);
    }
    return PackFloat32(&r);
}

/**
 ****************************************************************************************
 * \fn void SFloat32_Div(SoftFloatContext *sf,SFloat32_t *a,SFloat32_t *b,SFloat32_t *r) 
 * Divide two softfloat 32 bit numbers.
 ****************************************************************************************
 */
#define DIV_RND_BITS (2)
static inline void
SFloat32_Div(SoftFloatContext * sf, SFloat32_t * a, SFloat32_t * b, SFloat32_t * r)
{
    uint32_t div;
    uint32_t rem;
    uint32_t divisor;
    int digits;
    NEG(r) = NEG(a) ^ NEG(b);
    rem = MAN(a);
    divisor = MAN(b);
    if (rem == 0) {
        MAN(r) = 0;
        EXP(r) = -126;
        return;
    }
    /* divisor = 0 -> return correctly signed infinite */
    if (divisor == 0) {
        MAN(r) = 0;
        EXP(r) = 128;
        SF_PostException(sf, SFE_DIV_ZERO);
        return;
    }
    EXP(r) = EXP(a) - EXP(b);
    div = 0;
    digits = 0;
    /* Adjust divisor an remainder */
    while (rem >= (divisor << 1)) {
        divisor <<= 1;
        EXP(r) += 1;
    }
    while (rem < divisor) {
        rem <<= 1;
        EXP(r) -= 1;
        dbgprintf(sf, "Adjust exp to %d, rem %08x, b %08x\n", EXP(r), rem, divisor);
    }
    do {
        div <<= 1;
        if (rem >= divisor) {
            rem -= divisor;
            div |= 1;
        }
        digits++;
        rem <<= 1;
        dbgprintf(sf, "Step %d, div %08x rem %08x\n", digits, div, rem);
    } while (digits < (24 + DIV_RND_BITS));
    if (rem) {
        div |= 1;
    }
    dbgprintf(sf, "div result is %08x\n", div);
    MAN(r) = div;
    /* To big ? make infinite */
    if (EXP(r) >= 128) {
        EXP(r) = 128;
        MAN(r) = 0;
        SF_PostException(sf, SFE_OVERFLOW);
        /* If to small try it with shifting */
    } else if (EXP(r) < -126) {
        int shift = -126 - EXP(r);
        EXP(r) = -126;
        MAN(r) = shift_right32_sticky(MAN(r), shift);
    }
    Round32(sf, r, DIV_RND_BITS, sf->rounding_mode);
    MAN(r) >>= DIV_RND_BITS;
}

Float32_t
Float32_Div(SoftFloatContext * sf, Float32_t fla, Float32_t flb)
{
    SFloat32_t a, b, r;
    UnpackFloat32(&a, fla);
    UnpackFloat32(&b, flb);
    if (EXP(&a) == 128) {
        /* Infinity */
        if (MAN(&a) == 0) {
            /* Inf div Inf */
            if ((EXP(&b) == 128) && (MAN(&b) == 0)) {
                dbgprintf(sf, "Inf div by inf\n");
                MAN(&r) = UINT32_C(1) << 22;
                EXP(&r) = 128;
                NEG(&r) = true;
                SF_PostException(sf, SFE_INV_OP);
            } else {
                r = b; /* Propagate NAN of b */
            }
        } else {
            /* Propagate NAN of a */
            r = a;
        }
    } else if (EXP(&b) == 128) {
        r = b;
    } else if ((EXP(&a) == -126) && (EXP(&b) == -126)
               && (MAN(&a) == 0) && MAN(&b) == 0) {
        dbgprintf(sf, "Both zero\n");
        /* Result is a NAN */
        EXP(&r) = 128; 
        MAN(&r) = (1 << 22);
        NEG(&r) = true;
        SF_PostException(sf, SFE_INV_OP);
    } else {
        SFloat32_Div(sf, &a, &b, &r);
    }
    return PackFloat32(&r);
}

void
SFloat32_Rem(SoftFloatContext * sf, SFloat32_t * a, SFloat32_t * b, SFloat32_t * r)
{
    fprintf(stderr, "Error: Using unimplemented Floating point Remainder\n");
    exit(1);
}

/**
 ************************************************************************
 * \fn SFloat32_Sqrt(SoftFloatContext *sf,SFloat32_t *a,SFloat32_t *r) 
 * Calculate the square root.
 * Don't know a better algorithm with exact rounding. 
 ************************************************************************
 */
#define SQRT_RND_BITS (2)
static inline void
SFloat32_Sqrt(SoftFloatContext * sf, SFloat32_t * a, SFloat32_t * r)
{
    uint64_t rem;
    uint32_t sqrt;
    uint64_t product;
    int i;
    NEG(r) = NEG(a);
    /* NaN Check */
    /* What does square root in case of -Inf ? */
    if (EXP(a) == 128) {
        EXP(r) = EXP(a);
        MAN(r) = MAN(a);
        return;
    }
    /* First check for 0 mantissa because sqrt(-0) is allowed */
    if (MAN(a) == 0) {
        MAN(r) = 0;
        EXP(r) = -126;
        return;
    } else if (NEG(a)) {
        SF_PostException(sf, SFE_INV_OP);
        EXP(r) = 128;
        MAN(r) = 1 << 22;
        return;
    }
    EXP(r) = EXP(a);
    rem = MAN(a);
    while ((rem & 0xff800000) == 0) {
        rem <<= 1;
        EXP(r)--;
    }
    /* Make radix-4 */
    if (EXP(r) & 1) {
        rem <<= 1;
    }
    EXP(r) >>= 1;
    sqrt = 0;
    rem <<= (2 * SQRT_RND_BITS + 23);
    for (i = 23 + SQRT_RND_BITS; i > 0; i--) {
        sqrt |= (1 << i);
        product = ((uint64_t) sqrt * (uint64_t) sqrt);
        if (product > rem) {
            sqrt &= ~(1 << i);
        }
    }
    product = ((uint64_t) sqrt * (uint64_t) sqrt);
    if ((rem - product) > 0) {
        sqrt |= 1;
    }
    MAN(r) = sqrt;
    Round32(sf, r, SQRT_RND_BITS, sf->rounding_mode);
    MAN(r) >>= SQRT_RND_BITS;
}

Float32_t
Float32_Sqrt(SoftFloatContext * sf, Float32_t fla)
{
    SFloat32_t a, r;
    UnpackFloat32(&a, fla);
    SFloat32_Sqrt(sf, &a, &r);
    return PackFloat32(&r);
}

static inline int
SFloat32_Cmp(SoftFloatContext * sf, SFloat32_t * a, SFloat32_t * b)
{
    bool abs_greater;
    if ((EXP(a) == 128) && (MAN(a) != 0)) {
        return SFC_UNORDERD;
    }
    if ((EXP(b) == 128) && (MAN(b) != 0)) {
        return SFC_UNORDERD;
    }
    /* With Different sign its simple */
    if (NEG(a) != NEG(b)) {
        /* 0 and -0 are the same */
        if ((MAN(a) == 0) && (MAN(b) == 0)) {
            return SFC_EQUAL;
        }
        if (NEG(a)) {
            return SFC_LESS;
        } else {
            return SFC_GREATER;
        }
    }
    /* Now a and b have the same sign */
    if (EXP(a) > EXP(b)) {
        abs_greater = true;
    } else if (EXP(b) > EXP(a)) {
        abs_greater = false;
    } else if (MAN(a) > MAN(b)) {
        abs_greater = true;
    } else if (MAN(a) < MAN(b)) {
        abs_greater = false;
    } else {
        return SFC_EQUAL;
    }
    if (abs_greater == ! !NEG(a)) {
        return SFC_LESS;
    } else {
        return SFC_GREATER;
    }
}

int
Float32_Cmp(SoftFloatContext * sf, Float32_t fla, Float32_t flb)
{
    SFloat32_t a, b;
    UnpackFloat32(&a, fla);
    UnpackFloat32(&b, flb);
    return SFloat32_Cmp(sf, &a, &b);
}

/**
 ****************************************************************
 * Warning, The Conversion routines are not writen to comply to
 * IEEE754 but tested against results of gcc softfloat.
 ****************************************************************
 */
#define TOINT64_RNDBITS 2
static inline int64_t
SFloat32_ToInt64(SoftFloatContext * sf, SFloat32_t * a, SF_RoundingMode rm)
{

    int64_t result;
    int rshift = 23 - EXP(a);
    dbgprintf(sf, "Rshift is %d\n", rshift);
    if ((rshift) >= 0) {
        MAN(a) <<= TOINT64_RNDBITS;
        MAN(a) = shift_right64_sticky(MAN(a), rshift);
        Round32(sf, a, TOINT64_RNDBITS, rm);
        MAN(a) >>= TOINT64_RNDBITS;
        if (NEG(a)) {
            return -(int64_t) MAN(a);
        } else {
            return MAN(a);
        }
    } else {
        if (NEG(a)) {
            result = -(int64_t) MAN(a);
        } else {
            result = MAN(a);
        }
        if ((-rshift) < (64 - 24)) {
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
 * \fn int64_t SFloat32_ToUInt64(SoftFloatContext *sf,SFloat32_t *a,SF_RoundingMode rm)
 * Contert a Softfloat32 to an uint64_t.
 ************************************************************************
 */
static inline uint64_t
SFloat32_ToUInt64(SoftFloatContext * sf, SFloat32_t * a, SF_RoundingMode rm)
{

    uint64_t result;
    int rshift = 23 - EXP(a);
    dbgprintf(sf, "Rshift is %d\n", rshift);
    if (NEG(a)) {
        dbgprintf(sf, "Uint is Neg\n");
        //return 0;
    }
    if ((rshift) >= 0) {
        MAN(a) <<= TOINT64_RNDBITS;
        MAN(a) = shift_right64_sticky(MAN(a), rshift);
        Round32(sf, a, TOINT64_RNDBITS, rm);
        MAN(a) >>= TOINT64_RNDBITS;
        if (NEG(a)) {
            return -MAN(a);
        } else {
            return MAN(a);
        }
    } else {
        result = MAN(a);
        if ((-rshift) < (64 - 23)) {
            result = result << (-rshift);
        } else {
            if (-rshift < 64) {
                result = result << (-rshift);
            } else {
                result = 0;
            }
        }
        if (NEG(a)) {
            return -result;
        } else {
            return result;
        }
    }
}

int64_t
Float32_ToInt64(SoftFloatContext * sf, Float32_t fla, SF_RoundingMode rm)
{
    SFloat32_t a;
    UnpackFloat32(&a, fla);
    return SFloat32_ToInt64(sf, &a, rm);
}

uint64_t
Float32_ToUInt64(SoftFloatContext * sf, Float32_t fla, SF_RoundingMode rm)
{
    SFloat32_t a;
    UnpackFloat32(&a, fla);
    return SFloat32_ToUInt64(sf, &a, rm);
}

int32_t
Float32_ToInt32(SoftFloatContext * sf, Float32_t fla, SF_RoundingMode rm)
{
    int64_t result;
    SFloat32_t a;
    UnpackFloat32(&a, fla);
    result = SFloat32_ToInt64(sf, &a, rm);
    dbgprintf(sf, "Result64: %016"PRIx64"\n", result);
    //fprintf(stderr, "Result64: %016"PRIx64"\n", result);
    if (result < INT64_C(-0x80000000)) {
        return 0x80000000;
    } else if (result > INT64_C(0x7fffffff)) {
        return 0x80000000;
    } else {
        return result;
    }
}

#define FLTOINT64_RNDBITS 2
Float32_t
Float32_FromInt64(SoftFloatContext * sf, int64_t in, SF_RoundingMode rm)
{
    SFloat32_t a;
    uint64_t manA;
    int lz;
    int rshift;
    if (in > 0) {
        NEG(&a) = false;
        manA = in;
    } else if (in == 0) {
        return 0;
    } else {
        NEG(&a) = true;
        manA = -in;
    }
    lz = clz64(manA);
    rshift = (64 - 24) - lz;
    dbgprintf(sf, "CLZ %d, rshift %d, man %016"PRIX64"\n", lz, rshift, manA);
    if (rshift <= 0) {
        EXP(&a) = 23 - (-rshift);
        MAN(&a) = manA << (-rshift);
    } else {
        EXP(&a) = 23 + rshift;
        if (rshift <= FLTOINT64_RNDBITS) {
            MAN(&a) = manA;
            Round32(sf, &a, rshift, rm);
            MAN(&a) >>= rshift;
        } else {
            MAN(&a) = shift_right64_sticky(manA, rshift - FLTOINT64_RNDBITS);
            dbgprintf(sf, "shrsticky %08x\n", MAN(&a));
            Round32(sf, &a, FLTOINT64_RNDBITS, rm);
            dbgprintf(sf, "rounded %08x\n", MAN(&a));
            MAN(&a) >>= FLTOINT64_RNDBITS;
        }
    }
    return PackFloat32(&a);
}

Float32_t
Float32_FromUInt64(SoftFloatContext * sf, uint64_t manA, SF_RoundingMode rm)
{
    SFloat32_t a;
    int lz;
    int rshift;

    NEG(&a) = false;
    lz = clz64(manA);
    rshift = (64 - 24) - lz;
    dbgprintf(sf, "CLZ %d, rshift %d, man %016"PRIX64"\n", lz, rshift, manA);
    if (rshift <= 0) {
        EXP(&a) = 23 - (-rshift);
        MAN(&a) = manA << (-rshift);
    } else {
        EXP(&a) = 23 + rshift;
        if (rshift <= FLTOINT64_RNDBITS) {
            MAN(&a) = manA;
            Round32(sf, &a, rshift, rm);
            MAN(&a) >>= rshift;
        } else {
            MAN(&a) = shift_right64_sticky(manA, rshift - FLTOINT64_RNDBITS);
            Round32(sf, &a, FLTOINT64_RNDBITS, rm);
            MAN(&a) >>= FLTOINT64_RNDBITS;
        }
    }
    return PackFloat32(&a);
}

#define CONV_RNDBITS (2)
Float32_t
Float32_FromFloat64(SoftFloatContext * sf, Float64_t fl)
{
    int rshift;
    SFloat64_t src;
    SFloat32_t dst;
    UnpackFloat64(&src, fl);
    dbgprintf(sf, "Man of src %"PRIx64" fl %"PRIx64"\n", MAN(&src), fl);
    if (EXP(&src) == 1024) {
        if (MAN(&src) == 0) {
            /* return Infinite; */
            MAN(&dst) = 0;
            EXP(&dst) = 128;
            NEG(&dst) = NEG(&src);
        } else {
            EXP(&dst) = 128;
            NEG(&dst) = NEG(&src);
            MAN(&dst) = MAN(&src) >> (52 - 23);
            dbgprintf(sf, "%"PRIx64" to %x\n", MAN(&src), MAN(&dst));
        }
    } else if (EXP(&src) > 128) {
        //return Infinite32;
        MAN(&dst) = 0;
        EXP(&dst) = 128;
        NEG(&dst) = NEG(&src);
    } else if (EXP(&src) < -1022) {
        if (NEG(&src)) {
            return (1 << 31);
        } else {
            return 0;
        }
    } else if (EXP(&src) < -126) {
        EXP(&dst) = -126;
        NEG(&dst) = NEG(&src);
        rshift = -126 - EXP(&src) + (52 - 23);
        dbgprintf(sf, "denormalized %d\n", rshift);
        MAN(&dst) = shift_right64_sticky(MAN(&src), rshift - CONV_RNDBITS);
        Round32(sf, &dst, CONV_RNDBITS, sf->rounding_mode);
        MAN(&dst) >>= CONV_RNDBITS;
    } else {
        EXP(&dst) = EXP(&src);
        NEG(&dst) = NEG(&src);
        rshift = (52 - 23);
        dbgprintf(sf, "Defaultshift %"PRIx64" of %d\n", MAN(&src), rshift);
        MAN(&dst) = shift_right64_sticky(MAN(&src), rshift - CONV_RNDBITS);
        Round32(sf, &dst, CONV_RNDBITS, sf->rounding_mode);
        MAN(&dst) >>= CONV_RNDBITS;
    }
    dbgprintf(sf, "before pack result %x, %x\n", MAN(&dst), PackFloat32(&dst));
    return PackFloat32(&dst);
}
