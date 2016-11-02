/*
 *******************************************************************************
 * Floating point emulation header file 
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

#ifndef _SOFTFLOAT_H
#define _SOFTFLOAT_H

#include <stdint.h>
#include <stdbool.h>
#include "sgstring.h"

/* Soft float mode definitions */
typedef enum SF_RoundingMode  {
	SFM_ROUND_NEAREST_EVEN = 1,
	SFM_ROUND_ZERO = 2,
	SFM_ROUND_PLUS_INF = 3,
	SFM_ROUND_MINUS_INF = 4,
	SFM_ROUND_TO_AWAY = 5 /* p754/D1.2.9 */
} SF_RoundingMode;

/* Soft float exceptions */
#define SFE_INV_OP		(1)
#define	SFE_DIV_ZERO		(2)
#define SFE_OVERFLOW		(4)
#define SFE_UNDERFLOW		(8)
#define SFE_INEXACT		(0x10)

/* Soft float mutual exclusive comparison results */
#define SFC_LESS	(1)
#define SFC_EQUAL	(2)
#define SFC_GREATER	(4)
#define SFC_UNORDERD	(8)

typedef void SoftFloatExceptionProc(void *eventData,uint32_t exception); 

typedef struct SoftFloatContext {
	SF_RoundingMode rounding_mode;
	uint32_t exceptions;
	SoftFloatExceptionProc *exceptionProc;
	void *exceptionEventData;
	int debug;	
} SoftFloatContext;

static inline SF_RoundingMode
SoftFloat_RoundingMode(SoftFloatContext *ctxt) {
	return ctxt->rounding_mode;
}
/*
 **********************************************************
 * Internal representation of Floating point numbers
 **********************************************************
 */
typedef struct SFloat32_t {
	bool isneg;
	int exponent;		/**< Exponent in two's complement */
	uint32_t mantissa;	/**< Mantissa  */
} SFloat32_t;

typedef struct SFloat64_t {
	bool isneg;
	int exponent;  		/**< Exponent in two's complement */
	uint64_t mantissa;	/**< Mantissa */ 
} SFloat64_t;

typedef uint32_t Float32_t;
typedef uint64_t Float64_t;

Float32_t Float32_Add(SoftFloatContext *sf,Float32_t fla,Float32_t flb);
Float32_t Float32_Sub(SoftFloatContext *sf,Float32_t fla,Float32_t flb);
Float32_t Float32_Mul(SoftFloatContext *sf,Float32_t fla,Float32_t flb);
Float32_t Float32_Div(SoftFloatContext *sf,Float32_t fla,Float32_t flb);
Float32_t Float32_Rem(SoftFloatContext *sf,Float32_t fla,Float32_t flb);
Float32_t Float32_Sqrt(SoftFloatContext *sf,Float32_t fla);
int Float32_Cmp(SoftFloatContext *sf,Float32_t fla,Float32_t flb);

static inline bool 
Float32_IsPlusMinusNull(Float32_t fl) {
	return !(fl & 0x7fffffff);
}

static inline bool 
Float32_IsNegative(Float32_t fl) {
	return !!(fl & 0x80000000);
}

static inline bool 
Float32_IsNan(Float32_t fl) {
	return  (((fl >> 23) & 0xff) == 0xff) && !!(fl & 0x007fffff);
}

static inline bool 
Float32_IsPlusMinusInf(Float32_t fl) {
	return  (((fl >> 23) & 0xff) == 0xff) && !(fl & 0x007fffff);
}
/* Draft 1.2.9 calls it subnormal */
static inline bool 
Float32_IsSubnormal(Float32_t fl) {
	return  (((fl >> 23) & 0xff) == 0) && (fl & 0x007fffff);
}

Float64_t Float64_Add(SoftFloatContext *sf,Float64_t fla,Float64_t flb);
Float64_t Float64_Sub(SoftFloatContext *sf,Float64_t fla,Float64_t flb);
Float64_t Float64_Mul(SoftFloatContext *sf,Float64_t fla,Float64_t flb);
Float64_t Float64_Div(SoftFloatContext *sf,Float64_t fla,Float64_t flb);
Float64_t Float64_Rem(SoftFloatContext *sf,Float64_t fla,Float64_t flb);
Float64_t Float64_Sqrt(SoftFloatContext *sf,Float64_t fla);
int Float64_Cmp(SoftFloatContext *sf,Float32_t fla,Float32_t flb);

int64_t Float32_ToInt64(SoftFloatContext *sf,Float32_t fla,SF_RoundingMode rm);
uint64_t Float32_ToUInt64(SoftFloatContext *sf,Float32_t fla,SF_RoundingMode rm);
int32_t Float32_ToInt32(SoftFloatContext *sf,Float32_t fla,SF_RoundingMode rm);
Float32_t Float32_FromInt64(SoftFloatContext *sf,int64_t in,SF_RoundingMode rm);

static inline Float32_t
Float32_FromInt32(SoftFloatContext *sf,int32_t in,SF_RoundingMode rm) {
	return Float32_FromInt64(sf,in,rm);
}

Float32_t Float32_FromUInt64(SoftFloatContext *sf,uint64_t in,SF_RoundingMode rm);

int64_t Float64_ToInt64(SoftFloatContext *sf,Float64_t fla,SF_RoundingMode rm);
uint64_t Float64_ToUInt64(SoftFloatContext *sf,Float64_t fla,SF_RoundingMode rm);
int32_t Float64_ToInt32(SoftFloatContext *sf,Float64_t fla,SF_RoundingMode rm);
Float64_t Float64_FromInt64(SoftFloatContext *sf,int64_t in,SF_RoundingMode rm);

static inline Float64_t
Float64_FromInt32(SoftFloatContext *sf,int32_t in) {
	return Float64_FromInt64(sf,in,SFM_ROUND_ZERO);
}
Float64_t Float64_FromUInt64(SoftFloatContext *sf,uint64_t in,SF_RoundingMode rm);

Float32_t Float32_FromFloat64(SoftFloatContext *sf,Float64_t fl);
Float64_t Float64_FromFloat32(SoftFloatContext *sf,Float32_t fl);


static inline SoftFloatContext *
SFloat_New(void) {
	SoftFloatContext *sf = sg_new(SoftFloatContext);
	sf->rounding_mode = SFM_ROUND_NEAREST_EVEN;
	return sf;
}

static inline void
SFloat_Debug(SoftFloatContext *sf,int debugmode) {
	sf->debug = debugmode;	
}

static inline void
SFloat_SetExceptionProc(SoftFloatContext *sf,SoftFloatExceptionProc *proc,void *eventData)
{
	sf->exceptionProc = proc;
	sf->exceptionEventData = eventData;
}
#endif
