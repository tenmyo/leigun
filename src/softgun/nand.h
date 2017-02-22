#ifndef _NAND_H
#define _NAND_H
#include <stdint.h>
#include "diskimage.h"
#include "signode.h"

#if 0
#define NFCTRL_nCE	(2)
#define NFCTRL_nWE	(4)
#endif
#define NFCTRL_ALE	(2)
#define NFCTRL_CLE	(1)
typedef struct NandFlash NandFlash;

NandFlash *NandFlash_New(const char *name);
uint8_t NandFlash_Read(NandFlash * nf, uint8_t signalLines);
void NandFlash_Write(NandFlash * nf, uint8_t data, uint8_t signalLines);

#endif
