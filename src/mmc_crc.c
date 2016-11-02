/*
 *************************************************************************************************
 *  
 *  CRC7/CRC16 Calculation for MMC/SD-Cards
 *
 * state: working 
 *
 * Copyright 2007 Jochen Karrer. All rights reserved.
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
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "mmc_crc.h"

static uint16_t crctab[256];
static uint8_t crc7tab[256];
static int crctab_initialized = 0;

static void CRC16_CreateTab(void);
static void CRC7_CreateTab(void);
static void TestCRC7(void); 
static void TestCRC16(void);

/*
 * ------------------------------------------
 * Slow Bitwise CRC
 * ------------------------------------------
 */
static void
CRC7_Bitwise(uint8_t *crc,uint8_t val)
{
        int i;
        for(i=7;i>=0;i--) {
                int carry = *crc & 0x40;
                int inbit = !!(val & (1<<i));
                *crc = (*crc<<1) & 0x7f;
                if(carry)
                        inbit = !inbit;
                if(inbit) {
                        *crc = *crc ^ 0x9;
                }
        }
}

/*
 * -------------------------------------------------------
 * Create the CRC table for the fast CRC calculation
 * using the slow bitwise crc
 * -------------------------------------------------------
 */
static void
CRC7_CreateTab()
{
        uint8_t crc;
        int i;
        for(i=0;i<256;i++) {
                crc=0;
                CRC7_Bitwise(&crc,i);
                crc7tab[i] = crc;
        }
}

void
MMC_CRC7(uint8_t *crc,const uint8_t *vals,int len)
{
        uint8_t index;
        int i;
        for(i=0;i<len;i++) {
                index = ( *crc << 1 ) ^ vals[i];
                *crc = (( *crc << 7) ^ crc7tab[index & 0xff]) & 0x7f;
        }
}

void
MMC_CRC7Init(uint8_t *crc,uint16_t initval)
{
	if(!crctab_initialized) {
		CRC16_CreateTab();
        	CRC7_CreateTab();
		crctab_initialized = 1;	
		TestCRC16(); 
		TestCRC7(); 
	}
        *crc = initval;
};


/*
 * ------------------------------------------
 * Slow Bitwise CRC
 * ------------------------------------------
 */
static void
CRC16_Bitwise(uint8_t val,uint16_t *crc)
{
        int i;
        for(i=7;i>=0;i--) {
                int carry = *crc & 0x8000;
                int inbit = !!(val & (1<<i));
                *crc = *crc<<1;
                if(carry)
                        inbit = !inbit;
                if(inbit) {
                        *crc = *crc ^ 0x1021;
                }
        }
}

/*
 * -------------------------------------------------------
 * Create the CRC table for the fast CRC calculation
 * using the slow bitwise crc
 * -------------------------------------------------------
 */
static void
CRC16_CreateTab()
{
        uint16_t crc;
        int i;
        for(i=0;i<256;i++) {
                crc=0;
                CRC16_Bitwise(i,&crc);
                crctab[i] = crc;
        }
}


/*
 * ----------------------------------------------------------------
 * CRC16
 *      Set Start value for the CRC (here 0x0000)
 * ----------------------------------------------------------------
 */
void
MMC_CRC16Init(uint16_t *crc,uint16_t initval)
{
	if(!crctab_initialized) {
		CRC16_CreateTab();
        	CRC7_CreateTab();
		crctab_initialized = 1;	
		TestCRC16(); 
		TestCRC7(); 
	}
        *crc = initval;

};

/*
 * ---------------------------------------------------------------------------
 * Test the CRC7 with the test vectors given in SD Simplified Physical Layer
 * Specification Version 2.0
 * ---------------------------------------------------------------------------
 */
static void
TestCRC7() {
        uint8_t crc;
        uint8_t data1[5] = {0x40,0,0,0,0};
        uint8_t data2[5] = {0x51,0,0,0,0};
        uint8_t data3[5] = {0x11,0,0,0,11};
	MMC_CRC7Init(&crc,0);
        MMC_CRC7(&crc,data1,5);
	if(crc != 0x4a) {
        	fprintf(stderr,"\nMMC_CRC7 Test Vector 1 failed: %02x\n",crc);
		exit(1);
	}
	MMC_CRC7Init(&crc,0);
        MMC_CRC7(&crc,data2,5);
	if(crc != 0x2a) {
        	fprintf(stderr,"\nMMC_CRC7 Test Vector 2 failed: %02x\n",crc);
		exit(1);
	}
	MMC_CRC7Init(&crc,0);
        MMC_CRC7(&crc,data3,5);
	if(crc != 0x33) {
        	fprintf(stderr,"\nMMC_CRC7 Test Vector 3 failed: %02x\n",crc);
		exit(1);
	}
}

/*
 * ------------------------------------------------------------------
 * TestCRC16
 * 	Test if CRC16 is working correctly
 * 	The Testvector from the SD-Spec is not a good example because
 * 	its a block with all bits set. You may not see byte reversal
 * 	bugs
 * ------------------------------------------------------------------
 */
static void
TestCRC16() {
        uint16_t crc;
        uint8_t data[512];
	uint8_t data2[] = { 0x08,0x15,0x47,0x11 };
	memset(data,0xff,512);
	MMC_CRC16Init(&crc,0);
        MMC_CRC16(&crc,data,512);
	if(crc != 0x7fa1) {
		fprintf(stderr,"\nMMC_CRC16 Test Vektor failed %04x\n",crc);
		exit(0);
	}
	MMC_CRC16Init(&crc,0);
        MMC_CRC16(&crc,data2,4);
	if(crc != 0xbb1b) {
		fprintf(stderr,"\nMMC_CRC16 Test Vektor failed %04x\n",crc);
		exit(0);
	}
}

void
MMC_CRC16(uint16_t *crc,const uint8_t *vals,int len)
{
        uint8_t index;
        int i;
        for(i=0;i<len;i++) {
                index = ( *crc>>8 ) ^ vals[i];
                *crc = ( *crc<<8 ) ^ crctab[index];
        }
}

