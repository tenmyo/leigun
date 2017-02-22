/*
 **************************************************************************************************
 * sglib.c
 *    Some usefull functions needed everythere 
 *
 * (C) 2009 Jochen Karrer
 *
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

#include "sgtypes.h"
#include "sglib.h"
#include "stdbool.h"

/*
 ***************************************+
 * find gcd euklid used to reduce
 * clock multiplier/divider pairs
 ***************************************+
 */
static uint64_t
find_gcd_mod(uint64_t u, uint64_t v)
{
	uint64_t tmp;
	while (u > 0) {
		tmp = u;
		u = v % u;
		v = tmp;
	}
	return v;
}

/*
 ********************************************
 * reduce a fraction
 ********************************************
 */
void
FractionU64_Reduce(FractionU64_t * frac)
{
	uint64_t gcd = find_gcd_mod(frac->nom, frac->denom);
	if (gcd > 1) {
		frac->nom /= gcd;
		frac->denom /= gcd;
	}
}

static unsigned int
onecount_slow(const uint32_t value)
{
	uint32_t val = value;
	int ones = 0;
	while (val) {
		if (val & 1)
			ones++;
		val >>= 1;
	}
	return ones;
}

/*
 ******************************************************
 * Count ones in a word
 ******************************************************
 */
uint8_t sglib_onecount_map[256];

static void
init_onecount_map(void)
{
	int j;
	for (j = 0; j < 256; ++j) {
		sglib_onecount_map[j] = onecount_slow(j);
	}
}

/**
 ***********************************************************++
 * Decode a 32 Bit number from Gray code
 ***********************************************************++
 */

uint32_t
GrayDecodeU32(uint32_t to_decode)
{
	uint32_t result = to_decode ^ (to_decode >> 16);
	result ^= result >> 8;
	result ^= result >> 4;
	result ^= result >> 2;
	result ^= result >> 1;
	return result;
}

/**
 **************************************************************************
 * \fn unsigned int unicode_to_utf8(uint16_t ucs2,uint8_t *buf)
 * Convert an Unicode Codepoint value represented by an uint32_t 
 * to UTF8 encoding.
 * The buffer must have room for at least 4 bytes 
 * (3 bytes if restricted to ucs2)
 * \retval The number of bytes written to the buffer is returned.
 **************************************************************************
 */
unsigned int
unicode_to_utf8(uint32_t unicode, uint8_t * buf)
{
	if (unicode < 0x80) {
		buf[0] = unicode;
		return 1;
	} else if (unicode <= 0x7ff) {
		buf[0] = 0xc0 | (unicode >> 6);
		buf[1] = 0x80 | (unicode & 0x3f);
		return 2;
	} else if (unicode <= 0xffff) {
		buf[0] = 0xe0 | (unicode >> 12);
		buf[1] = 0x80 | ((unicode >> 6) & 0x3f);
		buf[2] = 0x80 | (unicode & 0x3f);
		return 3;
	} else if (unicode <= 0x1fffff) {
		buf[0] = 0xf0 | (unicode >> 18);
		buf[1] = 0x80 | ((unicode >> 12) & 0x3f);
		buf[2] = 0x80 | ((unicode >> 6) & 0x3f);
		buf[3] = 0x80 | (unicode & 0x3f);
		return 4;
	}
	return 0;
}

/**
 *******************************************************************************************
 * \fn bool utf8_to_unicode(Utf8ToUnicodeCtxt * ctxt, uint32_t * dst, uint8_t by)
 * Convert an UTF8 stream to unicode codepoint represented as uint32_t
 * Needs a Context structure which is initialized with zero's. 
 * \retval true if a unicode character is complete, false else.
 *******************************************************************************************
 */
bool
utf8_to_unicode(Utf8ToUnicodeCtxt * ctxt, uint32_t * dst, uint8_t inbyte)
{
	if (inbyte < 0x80) {
		ctxt->rembytes = 0;
		*dst = inbyte;
		return true;
	} else if (inbyte < 0xc0) {
		if (ctxt->rembytes) {
			ctxt->ass_buf = (ctxt->ass_buf << 6) | (inbyte & 0x3f);
			if ((--ctxt->rembytes) == 0) {
				*dst = ctxt->ass_buf;
				return true;
			}
		}
	} else if (inbyte < 0xe0) {
		ctxt->ass_buf = inbyte & 0x1f;
		ctxt->rembytes = 1;
	} else if (inbyte < 0xf0) {
		ctxt->ass_buf = inbyte & 0xf;
		ctxt->rembytes = 2;
	} else if (inbyte < 0xf8) {
		ctxt->ass_buf = inbyte & 0x7;
		ctxt->rembytes = 3;
	}
	return false;
}

void
SGLib_Init(void)
{
	init_onecount_map();
}
