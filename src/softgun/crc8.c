/**
 ***********************************************************************
 * CRC8 calculation for polynomyal x^8 + x^2 + x^1 + 1
 ***********************************************************************
 */
#include "crc8.h"
#include "compiler_extensions.h"
#include "sglib.h"
#include "sgstring.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "initializer.h"

#define POLY_7 7
static uint8_t tab_07[256];
static uint8_t tab_07_rev[256];

/**
 *****************************************************************
 * Slow Bitwise calculation of CRC
 *****************************************************************
 */
static uint8_t
Crc8_Bitwise(uint8_t val, uint8_t crc, uint8_t poly)
{
	int8_t i;
	for (i = 7; i >= 0; i--) {
		uint8_t carry = crc >> 7;
		uint8_t inbit = ! !(val & (1 << i));
		crc = crc << 1;
		if (carry != inbit) {
			crc = crc ^ poly;
		}
	}
	return crc;
}

/**
 **********************************************************************************
 * \fn static void CRC8_Test(void)
 * Test if the CRC generator is correctly working with a well known Test string.
 ***********************************************************************************
 */
static void
CRC8_Test(void)
{
	uint8_t crc8 = 0x00;
	char *str = "Pferd";
	crc8 = Crc8_Poly7(crc8, (uint8_t *) str, strlen(str));
	if (crc8 != 0x6d) {
		fprintf(stderr, "CRC8 Unit Test failed, doing self murder\n");
		exit(1);
	}
}

/**
 *******************************************************************
 * Initialize the tables for fast bytewise calculation of CRC
 *******************************************************************
 */
INITIALIZER(CRC8Tab_Init)
{
	uint8_t *crctab = tab_07;
	uint8_t *crctab_poly7_rev = tab_07_rev;
	uint8_t poly = POLY_7;
	int i;
	for (i = 0; i < 256; i++) {
		crctab[i] = Crc8_Bitwise(0, i, poly);
		crctab_poly7_rev[Bitreverse8(i)] = Bitreverse8(crctab[i]);
	}
	CRC8_Test();
}

/**
 *******************************************************************************
 * \fn uint8_t Crc8_Poly7(uint8_t crc,uint8_t *data,uint16_t count)
 *******************************************************************************
 */
uint8_t
Crc8_Poly7(uint8_t crc, uint8_t * data, uint32_t count)
{
	uint32_t i;
	uint8_t index;
	for (i = 0; i < count; i++) {
		index = crc ^ data[i];
		crc = tab_07[index];
	}
	return crc;
}

/**
 * MSB first variant
 */
uint8_t
Crc8_Poly7Rev(uint8_t crc, uint8_t * data, uint32_t count)
{
	uint32_t i;
	uint8_t index;
	for (i = 0; i < count; i++) {
		index = crc ^ data[i];
		crc = tab_07_rev[index];
	}
	return crc;
}

#ifdef TEST
int
main()
{
	int i;
	CRC8_Test();
	for (i = 0; i < 256; i++) {
		if ((i & 15) == 0) {
			fprintf(stdout, "\n");
		}
		fprintf(stdout, "0x%02x,", tab_07[i]);
	}
	fprintf(stdout, "\n");
	exit(0);
}

#endif				/*  */
