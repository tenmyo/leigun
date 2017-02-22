/*
 *************************************************************************************************
 *
 * 32 Bit CRC calculations
 *
 * state: working, no interface defined
 *
 * Copyright 2007 2011 Jochen Karrer. All rights reserved.
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

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "compiler_extensions.h"
#include "sglib.h"
#include "initializer.h"

/*
 * --------------------------------------------------
 * Bitwise version for init of crctabs
 * Takes only one Byte of data because it is
 * normally only used during initialization
 * --------------------------------------------------
 */
static uint32_t
CRC32_bitwise(uint32_t crc, uint8_t val, uint32_t poly)
{
	int i;
	for (i = 7; i >= 0; i--) {
		int carry = !!(crc & 0x80000000);
		int inbit = !!(val & (1 << i));
		crc = crc << 1;
		if (inbit ^ carry) {
			crc = crc ^ poly;
		}
	}
	return crc;
}

/*
 * ------------------------------------------------------
 * The bitreverse version
 * ------------------------------------------------------
 */
static uint32_t
CRC32rev_bitwise(uint32_t crc, uint8_t val, uint32_t poly)
{
	int i;
	for (i = 0; i < 8; i++) {
		int carry = !!(crc & 0x80000000);
		int inbit = !!(val & (1 << i));
		crc = crc << 1;
		if (inbit ^ carry) {
			crc = crc ^ poly;
		}
	}
	return crc;
}

static uint32_t tab_04C11DB7[256];
static uint32_t tab_04C11DB7_rev[256];
/* ethernet tab includes initial and final ^ 0xffffffff */
static uint32_t tab_ethernet[256];
static uint32_t tab_1EDC6F41[256];
static uint32_t tab_1EDC6F41_rev[256];
static uint32_t tab_000000AF[256];

/*
 * ------------------------------------------------------------------
 * Initialize one CRC tab for a given polynom. When reflected
 * input is used all calculations are done bit reversed.
 * ------------------------------------------------------------------
 */
static void
CRC32Tab_Init(uint32_t * crctab, uint32_t poly, int reverse)
{
	int i;
	for (i = 0; i < 256; i++) {
		if (reverse) {
			crctab[i] = Bitreverse32(CRC32rev_bitwise(0, i, poly));
		} else {
			crctab[i] = CRC32_bitwise(0, i, poly);
		}
	}
}

/**
 ********************************************************************
 * Calculate a crc with inverted logic. This saves the initial 
 * and final 0xffffffff. 
 ********************************************************************
 */
static uint32_t
CRC32_revnegative(uint32_t crc, uint8_t val, uint32_t poly)
{
	int i;
	for (i = 0; i < 8; i++) {
		int carry = !(crc & 1);
		int inbit = !!(val & (1 << i));
		crc = (crc >> 1) | (UINT32_C(1) << 31);
		if (inbit ^ carry) {
			crc = crc ^ poly;
		}
	}
	return crc;
}

/**
 ********************************************************************
 * Initialize the CRC table for ethernet without initial and final
 * 0xffffffff. This is done by using negative logic.
 ********************************************************************
 */
static void
CRC32Tab_EthernetInit(uint32_t * crctab)
{
	unsigned int i;
	for (i = 0; i < 256; i++) {
		crctab[i] = CRC32_revnegative(0, i, 0xEDB88320);
	}
}

static uint32_t CRC32(uint32_t crc, uint32_t * crctab, void *vdata, int len);
static uint32_t CRC32rev(uint32_t crc, uint32_t * crctab, void *vdata, int len);

static uint32_t
CRC32rev(uint32_t crc, uint32_t * crctab, void *vdata, int len)
{
	uint8_t *data = (uint8_t *) vdata;
	uint8_t index;
	int i;
	for (i = 0; i < len; i++) {
		index = (crc) ^ data[i];
		crc = (crc >> 8) ^ crctab[index];
	}
	return crc;
}

static uint32_t
CRC32(uint32_t crc, uint32_t * crctab, void *vdata, int len)
{
	uint8_t *data = (uint8_t *) vdata;
	uint8_t index;
	int i;
	for (i = 0; i < len; i++) {
		index = (crc >> 24) ^ data[i];
		crc = (crc << 8) ^ crctab[index];
	}
	return crc;
}

/*
 * -------------------------------------------------------------
 * Self test for correctness. Values are taken from
 * "A PAINLESS GUIDE TO CRC ERROR DETECTION ALGORITHMS" 
 * from Ross N. Williams
 * -------------------------------------------------------------
 */
static void
CRC32Tabs_Test(void)
{
	uint32_t crc;
	int i;
	/* This data set is an example from IEEE802.3 */
	/* The result is transmitted LSB first 0x94 0xD2 0x54 0xAC */
	uint8_t data[] = { 0xbe, 0xd7, 0x23, 0x47, 0x6b, 0x8f, 0xb3, 0x14, 0x5e, 0xfb, 0x35, 0x59 };

	char *bla = "123456789";
	/* CRC-32/ADCCP Pkzip */
	crc = CRC32rev(0xffffffff, tab_04C11DB7_rev, bla, strlen(bla)) ^ 0xffffffff;

	if (crc != 0xCBF43926) {
		fprintf(stderr, "CRC Test1 failed: %08x\n", crc);
		exit(1);
	}

	crc = 0;
	for (i = 0; i < 126; i++) {
		crc = CRC32rev(crc, tab_ethernet, data, sizeof(data));
	}
	if (crc != 0xAC54D294) {
		fprintf(stderr, "Ethernet CRC32 selftest failed\n");
		exit(1);
	}

	/* CRC-32/Posix */
	crc = CRC32(0x0, tab_04C11DB7, bla, strlen(bla)) ^ 0xffffffff;
	if (crc != 0x765E7680) {
		fprintf(stderr, "CRC Test2 failed: %08x\n", crc);
		exit(1);
	}

	crc = CRC32rev(0xffffffff, tab_1EDC6F41_rev, bla, strlen(bla)) ^ 0xffffffff;
	if (crc != 0xE3069283) {
		fprintf(stderr, "CRC Test3 failed: %08x\n", crc);
		exit(1);
	}

	/* CRC-32/XFER */
	crc = CRC32(0x0, tab_000000AF, bla, strlen(bla));
	if (crc != 0xBD0BE338) {
		fprintf(stderr, "CRC Test4 failed: %08x\n", crc);
		exit(1);
	}
}

/*
 * ------------------------------------------------------------------
 * Initialize all crc tabs 
 * ------------------------------------------------------------------
 */
INITIALIZER(CRC32Tabs_Init)
{
	CRC32Tab_Init(tab_04C11DB7, 0x04C11DB7, 0);
	CRC32Tab_Init(tab_04C11DB7_rev, 0x04C11DB7, 1);
	CRC32Tab_Init(tab_1EDC6F41, 0x1EDC6F41, 0);
	CRC32Tab_Init(tab_1EDC6F41_rev, 0x1EDC6F41, 1);
	CRC32Tab_Init(tab_000000AF, 0x000000AF, 0);
	CRC32Tab_EthernetInit(tab_ethernet);
	CRC32Tabs_Test();
}

/**
 *****************************************
 * Now the interface procs
 *****************************************
 */
uint32_t
EthernetCrc(uint32_t crc, uint8_t * data, uint32_t size)
{
	return CRC32rev(crc, tab_ethernet, data, size);
}

/*
 * ------------------------------------------------
 * Main Programm for standalone test
 * ------------------------------------------------
 */
#ifdef TEST
int
main()
{
	int i;
	CRC32Tabs_Init();
	CRC32Tabs_Test();
	fprintf(stdout, "tab_04C11DB7_rev[256] = {\n");
	for (i = 0; i < 256; i++) {
		fprintf(stdout, "0x%08x,", tab_04C11DB7_rev[i]);
		if ((i & 7) == 7) {
			fprintf(stdout, "\n");
		}
	}
	fprintf(stdout, "};\n");
	exit(0);
}
#endif
