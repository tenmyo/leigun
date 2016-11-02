#ifndef _SGLIB_H
#define _SGLIB_H
#include <stddef.h>
#include "sgtypes.h"
void FractionU64_Reduce(FractionU64_t *frac);
void SGLib_Init(void);

#define container_of(ptr, type, member) ({			\
        const typeof( ((type *)0)->member ) *__m = (ptr);	\
        (type *)( (char *)__m - offsetof(type,member) );})

extern uint8_t sglib_onecount_map[256];

static inline int
SGLib_OnecountU32(uint32_t w)
{
        int c=0;
        while (w) {
                c += sglib_onecount_map[w & 255];
                w >>= 8;
        }
        return c;
}

static inline int
SGLib_OnecountU8(uint8_t w)
{
	return sglib_onecount_map[w];
}

static inline uint32_t
GrayEncodeU32(uint32_t to_encode) 
{
        return to_encode ^ (to_encode >> 1);
}
uint32_t GrayDecodeU32(uint32_t to_decode);

static inline uint8_t
Bitreverse8(uint8_t x) {
        x = (x >> 4 & 0x0f) | (x << 4 & 0xf0);
        x = (x >> 2 & 0x33) | (x << 2 & 0xcc);
        x = (x >> 1 & 0x55) | (x << 1 & 0xaa);
        return x;
}

static inline uint16_t
Bitreverse16(uint16_t x)
{
        x = (x >> 8 & 0x00ff) | (x << 8 & 0xff00);
        x = (x >> 4 & 0x0f0f) | (x << 4 & 0xf0f0);
        x = (x >> 2 & 0x3333) | (x << 2 & 0xcccc);
        x = (x >> 1 & 0x5555) | (x << 1 & 0xaaaa);
        return x;
}

static inline uint32_t
Bitreverse32(uint32_t x)
{
        x = (x >> 16) | (x << 16);
        x = (x >> 8 & 0x00ff00ff) | (x << 8 & 0xff00ff00);
        x = (x >> 4 & 0x0f0f0f0f) | (x << 4 & 0xf0f0f0f0);
        x = (x >> 2 & 0x33333333) | (x << 2 & 0xcccccccc);
        x = (x >> 1 & 0x55555555) | (x << 1 & 0xaaaaaaaa);
        return x;
}

typedef struct Utf8ToUnicodeCtxt {
	uint32_t ass_buf;
        uint8_t rembytes;
} Utf8ToUnicodeCtxt;

unsigned int unicode_to_utf8(uint32_t unicode,uint8_t *buf);
int utf8_to_unicode(Utf8ToUnicodeCtxt *ctxt,uint32_t *dst,uint8_t by);
#endif
