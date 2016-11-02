/*
 **********************************************************************************
 *
 * Header for Intel Hex Record parser
 *
 * (C) 2005  Lightmaze Solutions AG
 *   Author: Jochen Karrer
 *
 * Status:
 *      Working, but Segmented address records are missing
 * 
 **********************************************************************************
 */

#include <stdint.h>
#include <stdbool.h>
bool IHex_FileIsIHex(const char *filename);
typedef int XY_IHexDataHandler(uint32_t addr, uint8_t * buf, int len, void *clientData);
int64_t XY_LoadIHexFile(const char *filename, XY_IHexDataHandler * cb, void *clientData);
