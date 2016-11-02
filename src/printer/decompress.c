/*
 *************************************************************************************************
 *
 *  PCL raster data decompressor Mode 9 and Mode 10
 *
 * state: working
 *
 * Copyright 2006 Jochen Karrer. All rights reserved.
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
#include <decompress.h>

/*
 * Mode10 Decompress
 * Returns the number of RGB pixels in the buffer
 */


enum
{
    eeNewPixel = 0x0,
    eeWPixel = 0x20, 	/* left side (West) pixel */
    eeNEPixel = 0x40,	/* North east side pixel (right up) */	
    eeCachedColor = 0x60
};

inline uint32_t get3pixel (uint8_t* src)
{
    return (src[0] << 16) | (src[1] << 8) | (src[2]);
} 


/*
 * ------------------------------------------------------------------------
 * Delta encoding is relative to lastUpperPixel
 * Pixel: 
 *	High Byte (red) is stored first
 * 	Bits 0-7 blue 
 * 	Bits 8-15 green 
 *	Bits 16-23 red 
 * 
 *
 * Delta: 
 *	16 Bit value. High byte is stored first
 *	Bits 0-4   signed (delta blue)/2 (db 0x20 = -32 to db=0x1e = 30)
 *	Bits 5-9   signed delta green (-16 to 15)
 *	Bits 10-14 signed delta red (-16 to 15)
 *	Bit 15 	Marker for delta encoding
 *	
 * -----------------------------------------------------------------------
 */
static inline uint32_t
mode10_get_pixel(const uint8_t *src,uint32_t oldpixel,unsigned int *indexP,int maxlen) {
	uint32_t pixel;
	src+=*indexP;
	if(maxlen < 2) {
		return 0xffffff;
	}
	if(src[0] & 0x80) {
		uint16_t delta;
		int32_t dr,dg,db;
		int r,g,b;
		delta = (src[0]<<8) | src[1];
		dr = (delta & 0x7c00) >> 10;	
		dg = (delta & 0x3e0) >> 5;
		db = (delta & 0x1f) << 1;
		if(dr & 0x10) {
			dr |= 0xffffffe0;
		}
		if(dg & 0x10) {
			dg |= 0xffffffe0;
		}
		if(db & 0x20) {
			db |= 0xffffffd0;
		}
		r = ((oldpixel >> 16) + dr) & 0xff; 
		g = ((oldpixel >> 8)  + dg) & 0xff;
		b = ((oldpixel >> 0)  + db) & 0xff;
		pixel = (r << 16) | (g << 8) | (b);
		fprintf(stderr,"Delta\n");
		*indexP+=2;
	} else {
		if(maxlen < 3) {
			return 0xffffff;
		}
		pixel = ((src[0] << 16) | (src[1]<<8) | (src[2]))<<1;
		/* 
		 * Don't know how lowest bit of blue is produced correctly 
		 * to avoid error accumulation in seedrow
		 */
		if(pixel & 0x80)  {
			pixel |= 1;
		}
		*indexP+=3;
	}
	return pixel;
}

/*
 * --------------------------------------------------------------------------------------------------
 * Mode10 decompressor
 *
 * Command Byte:
 *	Bit 7: 1 = Run length, 0 = literal encoding
 * 	Bits 5-6: Pixel Source
 *		  0: New Pixel will follow in data stream
 * 		  1: First pixel is a copy of the West side pixel (left)
 *		  2: First pixel is a copy of the North east side pixel (right in seedrow) 
 *		  3: Cached color: First pixel is the first pixel of last iteration
 *	Bits 3-4: Seed Count (Number of bytes which are identical to previous line)
 *		  If Seedcount is 3 then the following bytes will be added up including the 
 *		  first no 0xff byte 
 *	Bits 0-2: Repeat count (runlen - 2) for RLE and (litcount - 1) for Literal mode
 *		  If the repeat_count field is 7 then the coding of the pixels is followed by
 *		  an additional repeat count byte. This repeats until the first non 255 byte
 *		  is found as repeat count.
 * --------------------------------------------------------------------------------------------------
 */
int
Mode10_Decompress(uint8_t * const dst,const uint8_t *src,int dstlen,int srclen) 
{
	int j;
	uint32_t cachedColor = 0x00ffffff;
	uint32_t pixel = 0xffffff;				
	uint8_t cmd;
	int rle;
	uint8_t *seedrow = dst;
	unsigned int dI = 0; /* destination Index */
	unsigned int sI = 0; /* source Index */
	while(sI<srclen) {
		cmd = src[sI++];
		rle = cmd & 0x80;
		int seedcnt;
		int runlen;
		int repeat;
		int pixel_source = cmd  & 0x60;
		seedcnt = (cmd >> 3) & 0x3;
		if(seedcnt == 3) {
			do {
				if(sI >= srclen) {
					return -1;
				}
				seedcnt+= src[sI];
			} while(src[sI++] == 0xff); 
		}
		runlen = (cmd & 0x7);
		if(runlen == 7) {
			repeat = 1;
		} else {
			repeat = 0;
		}
		dI+=seedcnt*3;
		if(pixel_source == eeNewPixel) {
			/* do nothing in this case */
		} else  if(pixel_source == eeWPixel) {
			if(dI>2) {
				pixel = (dst[dI-3] << 16) | (dst[dI-2]<<8) | (dst[dI-1]); 
			} else {
				pixel = 0xffffff;
				fprintf(stderr,"Mode10 decompressor: First pixel is eeWPixel encoded\n");
			}
		} else if(pixel_source == eeNEPixel) {
			/* North East pixel is still in seedrow */
			if(((dI+5) < dstlen) && seedrow) {
				pixel = (seedrow[dI+3] << 16) | (seedrow[dI+4] << 8) | seedrow[dI+5];
			} else {
				pixel = 0xffffff;
				fprintf(stderr,"Mode10 decompressor: Pixel not in seedrow\n");
			}
		} else {	// eeCachedColor
			pixel = cachedColor;	
		}
		if(rle) {
			/* RLE is ok */
			runlen+=2;
			if(pixel_source == eeNewPixel) {
				if((dI+3) > dstlen) {
					return -2;
				}
				pixel = get3pixel(dst+dI);
				pixel = mode10_get_pixel(src,pixel,&sI,srclen - sI);
				cachedColor = pixel;
			}
			if(repeat) {
				do {
					if(sI >= srclen) {
						return -1;
					}
					runlen+= src[sI];
				} while(src[sI++] == 0xff); 
			}
			for(j=0;j<runlen;j++) {
				if((dI+3) > dstlen) {
					return -2;
				}
				dst[dI++] = (pixel >> 16) & 0xff;	
				dst[dI++] = (pixel >> 8) & 0xff;	
				dst[dI++] = (pixel >> 0) & 0xff;	
			}
		} else {
			runlen+=1;
			if(pixel_source == eeNewPixel) {
				if((dI+3) > dstlen) {
					return -2;
				}
				pixel = get3pixel(dst+dI);
				pixel = mode10_get_pixel(src,pixel,&sI,srclen - sI);
				cachedColor = pixel;
				runlen--;	
			}
			if((dI+3) > dstlen) {
				return -2;
			}
			dst[dI++] = (pixel >> 16) & 0xff;	
			dst[dI++] = (pixel >> 8) & 0xff;	
			dst[dI++] = (pixel >> 0) & 0xff;	
			while(1) {
				for(j=0;j<runlen;j++) {
					if((dI+3) > dstlen) {
						return -2;
					}
					pixel = get3pixel(dst+dI);
					pixel = mode10_get_pixel(src,pixel,&sI,srclen - sI);
					//cachedColor = pixel; /* I think it is only updated on first literal pixel */
					dst[dI++] = (pixel >> 16) & 0xff;	
					dst[dI++] = (pixel >> 8) & 0xff;	
					dst[dI++] = (pixel >> 0) & 0xff;	
				}
				if(repeat) {
					runlen = src[sI++];	
					if(runlen != 255) {
						repeat=0;
					}
				} else {
					break; 
				}
			} 
		}
	}
	return dI;
}

/*
 * ------------------------------------------------------------------------
 * dst should already contain the seedrow
 * ------------------------------------------------------------------------
 */

int
Mode9_Decompress(uint8_t *dst,uint8_t *src ,int buflen,int srclen)
{
        int i,dst_wp = 0;
        for(i=0;i<srclen;) {
                uint8_t cmd=src[i++];
                if(cmd & 0x80) { // rle
                        int replacement_count = (cmd & 31)+2;
			int offset_count = (cmd >> 5) & 3;
			if(offset_count == 3) {
                                do {
					if(i >= srclen) {
						return - 1;
					}
                                        offset_count+=src[i];
                                } while (src[i++]==255) ;
			}
                        if(replacement_count == 33) {
                                do {
					if(i >= srclen) {
						return - 1;
					}
                                        replacement_count+=src[i];
                                } while (src[i++]==255) ;
                        }
			dst_wp += offset_count;
			if((dst_wp + replacement_count) > buflen) {
				return -2;
			}
			if(i >= srclen) {
				return -1;
			}
                        while(replacement_count) {
                                dst[dst_wp++] = src[i];
                                replacement_count--;
                        }
                        i++;
                } else {
                        int count = (cmd & 7)+1;
			int offset_count = (cmd >> 3) & 0xf;
			if(offset_count == 15) {
                                do {
					if(i >= srclen) {
						return - 1;
					}
                                        offset_count+=src[i];
                                } while(src[i++] == 255);
			}
                        if(count == 8) {
                                do {
					if(i >= srclen) {
						return - 1;
					}
                                        count+=src[i];
                                } while(src[i++] == 255);
                        }
			dst_wp += offset_count;
			if(dst_wp + count > buflen) {
				return -2;
			}
			if(i >= srclen) {
				return -1;
			}
                        while(count) {
                                dst[dst_wp++] = src[i];
                                i++;
                                count--;
                        }
                }
        }
        return dst_wp;
}
