/*
 *************************************************************************************************
 *  CRC16 Calculation 
 *
 * state: working, Only Polynomial 0x1021 in msb first and lsb first bit order implemented.
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
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "sglib.h"

static uint16_t crctab_0x1021[256];
static uint16_t crctab_0x1021_rev[256];
static uint16_t crctab_0x8005[256];
static uint16_t crctab_0x8005_rev[256];

static void CRC16_CreateTab(void);
static void TestCRC16(void);

/*
 ******************************************************************
 * Slow Bitwise CRC, MSB first Variant.
 ******************************************************************
 */
static void
CRC16_Bitwise(uint8_t val, uint16_t * crc, uint16_t poly)
{
	int i;
	for (i = 7; i >= 0; i--) {
		int carry = !!(*crc & 0x8000);
		int inbit = !!(val & (1 << i));
		*crc = *crc << 1;
		if (inbit ^ carry) {
			*crc = *crc ^ poly;
		}
	}
}

/**
 **************************************************************************************
 * \fn static void CRC16Rev_Bitwise(uint8_t val, uint16_t * crc, uint16_t poly)
 * Bitorder: Shift out LSB first
 *************************************************************************************
 */
static void
CRC16Rev_Bitwise(uint8_t val, uint16_t * crc, uint16_t poly)
{
    int i;
    for (i = 0; i < 8; i++) {
        int carry = !!(*crc & 1);
        int inbit = !!(val & (1 << i));
        *crc = *crc >> 1;
        if (inbit ^ carry) {
            *crc = *crc ^ poly;
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
	for (i = 0; i < 256; i++) {
		crc = 0;
		CRC16_Bitwise(i, &crc, 0x1021);
		crctab_0x1021[i] = crc;

        crc = 0;
		CRC16Rev_Bitwise(i, &crc, 0x8408);
		crctab_0x1021_rev[i] = crc; 

		crc = 0;
		CRC16_Bitwise(i, &crc, 0x8005);
		crctab_0x8005[i] = crc;
		crctab_0x8005_rev[Bitreverse8(i)] = Bitreverse16(crc);
	}
}

/*
 **********************************************************************
 * CRC16
 *      Set Start value for the CRC (here 0x0000)
 **********************************************************************
 */
uint16_t
CRC16_0x1021_Start(uint16_t initval)
{
	return initval;
};

/**
 ***************************************************************************
 * \fn uint16_t CRC16(uint16_t crc,const uint8_t *data,int len)
 * The most commonly used 16 bit CRC with polynomial 0x1021
 ***************************************************************************
 */
uint16_t
CRC16_0x1021(uint16_t crc, const uint8_t * data, int len)
{
	uint8_t index;
	int i;
	for (i = 0; i < len; i++) {
		index = (crc >> 8) ^ data[i];
		crc = (crc << 8) ^ crctab_0x1021[index];
	}
	return crc;
}

/**
 ***************************************************************************
 * \fn uint16_t CRC16_0x8005(uint16_t crc,const uint8_t *data,int len)
 * Together with a start value of 0x4f4e this is the CRC algorithm
 * for the NAND flash parameter page. 
 ***************************************************************************
 */
uint16_t
CRC16_0x8005(uint16_t crc, const uint8_t * data, int len)
{
	uint8_t index;
	int i;
	for (i = 0; i < len; i++) {
		index = (crc >> 8) ^ data[i];
		crc = (crc << 8) ^ crctab_0x8005[index];
	}
	return crc;
}

/**
 ****************************************************************************
 * \fn uint16_t CRC16_0x1021Rev(uint16_t crc, const uint8_t * data, int len)
 * Polynomial 0x1021 LSB first also known as Polynomial 0x8408 
 ****************************************************************************
 */
uint16_t
CRC16_0x1021Rev(uint16_t crc, const uint8_t * data, int len)
{
	uint8_t index;
	int i;
	for (i = 0; i < len; i++) {
		index = (crc) ^ data[i];
		crc = (crc >> 8) ^ crctab_0x1021_rev[index];
	}
	return crc;
}

uint16_t
CRC16_0x8005Rev(uint16_t crc, const uint8_t * data, int len)
{
	uint8_t index;
	int i;
	for (i = 0; i < len; i++) {
		index = (crc) ^ data[i];
		crc = (crc >> 8) ^ crctab_0x8005_rev[index];
	}
	return crc;
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
TestCRC16()
{
	uint16_t crc;
	uint8_t data[512];
	memset(data, 0xff, 512);
	crc = CRC16_0x1021_Start(0);
	crc = CRC16_0x1021(crc, data, 512);
	if (crc != 0x7fa1) {
		fprintf(stderr, "\nCRC16 Test Vektor failed %04x\n", crc);
		exit(2);
	}
}

static void
TestCRC16Rev()
{
	uint16_t crc;
	uint8_t data[2] = { 0x01, 0x23 };
	crc = CRC16_0x1021_Start(0);
	crc = CRC16_0x1021Rev(crc, data, 2);
	if (crc != 0x0a41) {
		fprintf(stderr, "\nCRC16 Reverse Test Vektor failed %04x\n", crc);
		exit(2);
	}
}

/**
 *************************************************************************
 * Test with a vector created with the ONFI NAND flash parameter page
 * specification example.
 *************************************************************************
 */
static void
TestCRC16_8005()
{
	uint16_t crc;
	uint8_t data[2] = { 0xab, 0xcd };
	crc = CRC16_0x1021_Start(0x4f4E);
	crc = CRC16_0x8005(crc, data, 2);
	if (crc != 0x5b06) {
		fprintf(stderr, "\nCRC16 for poly 0x8005: Test Vektor failed %04x\n", crc);
		exit(2);
	}
}

/**
 *****************************************************
 * \fn void CRC16_Init(void) 
 * Initilaize the CRC16 Tables.
 *****************************************************
 */
void
CRC16_Init(void)
{
	CRC16_CreateTab();
	TestCRC16();
	TestCRC16Rev();
	TestCRC16_8005();
}

#ifdef _TEST
int
main()
{
	int i;
	uint16_t crc;
	uint8_t data[2] = { 0x80, 0xc4 };
	CRC16_Init();
	crc = CRC16_Start(0);
	crc = CRC16(crc, data, 2);
	fprintf(stderr, "CRC %04x\n", crc);
	data[0] = 0x01;
	data[1] = 0x23;
	crc = CRC16_Start(0);
	crc = CRC16Rev(crc, data, 1);
	fprintf(stderr, "CRC %04x\n", crc);
	crc = CRC16_Start(0);
	crc = CRC16Rev(crc, data, 2);
	fprintf(stderr, "CRC %04x\n", crc);

	crc = CRC16_Start(0x4f4E);
	data[0] = 0xAB;
	data[1] = 0xCD;
	crc = CRC16_8005(crc, data, 2);
	fprintf(stderr, "CRC_8005 %04x\n", crc);

	for (i = 0; i < 256; i++) {
		crc = CRC16_Start(0);
		data[0] = i;
		crc = CRC16_8005Rev(crc, data, 1);
		fprintf(stderr, "%04x\n", crc);
	}
	crc = CRC16_Start(0x4f4E);
	data[0] = 0xAB;
	data[1] = 0xCD;
	crc = CRC16_8005Rev(crc, data, 2);
	fprintf(stderr, "CRC_8005Rev %04x\n", crc);
}
#endif
