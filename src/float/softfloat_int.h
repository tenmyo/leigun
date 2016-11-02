/*
 *******************************************************************************
 * Internal header file for floating point emulation 
 * (C) 2009 Jochen Karrer
 *
 *  This program is free software; you can distribute it and/or modify it
 *  under the terms of the GNU General Public License (Version 2) as
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope it will be useful, but WITHOUT
 *  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 *  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 *  for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  59 Temple Place - Suite 330, Boston MA 02111-1307, USA.
 *
 ********************************************************************************
 */

#ifdef DEBUG_SFLOAT
#define dbgprintf(sf,x...) { if((sf)->debug) { fprintf(stderr,x);} }
#else
#define dbgprintf(sf,x...) { }
#endif

#define NEG(op) ((op)->isneg)
#define EXP(op) ((op)->exponent)
#define MAN(op)	((op)->mantissa)

static inline Float32_t
PackFloat32(SFloat32_t * sf)
{
    int exponent;
    Float32_t fl32;
    exponent = sf->exponent + 127;
    if ((exponent == 1) && !(sf->mantissa & (1 << 23))) {
        exponent = 0;
    }
    fl32 = (sf->mantissa & ((1 << 23) - 1)) | (exponent << 23) | (sf->isneg << 31);
    return fl32;
}

static inline void
UnpackFloat32(SFloat32_t * sf, Float32_t fl)
{
    sf->exponent = ((fl >> 23) & 0xff) - 127;
    sf->mantissa = fl & 0x7fffff;
    if (sf->exponent == -127) {
        sf->exponent++;
    } else if(sf->exponent != 128) {
        sf->mantissa |= (1 << 23);
    }
    sf->isneg = ((int32_t) fl) < 0;
}

/**
 ******************************************************************
 * \fn Float64_t PackFloat64(SFloat64_t *sf)
 * Convert a Floating poing number from the structure format to
 * a 64 Bit representation.
 ******************************************************************
 */
static inline Float64_t
PackFloat64(SFloat64_t * sf)
{
    int exponent;
    Float64_t dbl;
    exponent = sf->exponent + 1023;
    if ((exponent == 1) && !(sf->mantissa & (UINT64_C(1) << 52))) {
        exponent = 0;
    }
    dbl = (sf->mantissa & ((UINT64_C(1) << 52) - 1)) |
        ((uint64_t) exponent << 52) | ((uint64_t) sf->isneg << 63);
    return dbl;
}

/**
 **************************************************************
 * \fn void UnpackFloat64(SFloat64_t *sf,Float64_t dbl); 
 * Convert a float from the 64 Bit packed format to
 * the internal structure representation. 
 **************************************************************
 */
static inline void
UnpackFloat64(SFloat64_t * sf, Float64_t dbl)
{
    sf->exponent = ((dbl >> 52) & 0x7ff) - 1023;
    sf->mantissa = dbl & ((UINT64_C(1) << 52) - 1);
    if (sf->exponent == -1023) {
        sf->exponent++;
    } else if(sf->exponent != 1024) {
        sf->mantissa |= (UINT64_C(1) << 52);
    }
    sf->isneg = ((int64_t) dbl) < 0;
}

static inline void
SF_PostException(SoftFloatContext * sf, uint32_t exception)
{
    sf->exceptions |= exception;
}
