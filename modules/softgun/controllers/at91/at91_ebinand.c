/**
 ************************************************************************
 * Bus Interface to NAND
 ************************************************************************
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "i2c.h"
#include "bus.h"
#include "signode.h"
#include "sgstring.h"
#include "at91_ecc.h"
#include "at91_ebinand.h"
#include "nand.h"

typedef struct AT91EbiNand {
	BusDevice bdev;
	BusDevice *eccDev;
	char *name;
	NandFlash *nf[2];
} AT91EbiNand;

static uint32_t
ebinand_read(void *clientData, uint32_t address, int rqlen)
{
	AT91EbiNand *en = clientData;
	uint32_t ale = (address >> 21) & 1;
	uint32_t cle = (address >> 22) & 1;
	uint32_t value = 0;
	ale = ale ? NFCTRL_ALE : 0;
	cle = cle ? NFCTRL_CLE : 0;
	/* CLE at Read ???? does this make sense */
	if (cle && en->eccDev) {
		AT91Ecc_ResetEC(en->eccDev);
	}
	if (en->nf[0]) {
		value = NandFlash_Read(en->nf[0], ale | cle);
		//fprintf(stderr,"Blar %08x\n",value);
	}
	if (!cle && !ale && en->eccDev) {
		AT91Ecc_Feed(en->eccDev, value, rqlen);
	}
	return value;
}

static void
ebinand_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	AT91EbiNand *en = clientData;
	uint32_t ale = (address >> 21) & 1;
	uint32_t cle = (address >> 22) & 1;
	ale = ale ? NFCTRL_ALE : 0;
	cle = cle ? NFCTRL_CLE : 0;
	if (cle && en->eccDev) {
		/* Read or write ? 0x00 or 0x80 */
		if ((value & 0x7f) == 0) {
			AT91Ecc_ResetEC(en->eccDev);
		}
	}
	if (!cle && !ale && en->eccDev) {
		AT91Ecc_Feed(en->eccDev, value, rqlen);
	}
	if (en->nf[0]) {
		//fprintf(stderr,"Bla %08x, val %08x\n",address,value);
		NandFlash_Write(en->nf[0], value & 0xff, ale | cle);
	}
}

static void
AT91EbiNand_Map(void *owner, uint32_t base, uint32_t mask, uint32_t flags)
{
	AT91EbiNand *en = (AT91EbiNand *) owner;
	IOH_NewRegion(base, 256 * 1024 * 1024, ebinand_read, ebinand_write, 0, en);
}

static void
AT91EbiNand_UnMap(void *owner, uint32_t base, uint32_t mapsize)
{
	IOH_DeleteRegion(base, mapsize);
}

BusDevice *
AT91EbiNand_New(const char *name, NandFlash * nf, BusDevice * ecc)
{
	AT91EbiNand *en = sg_new(AT91EbiNand);
	en->nf[0] = nf;
	en->name = strdup(name);
	en->bdev.first_mapping = NULL;
	en->bdev.Map = AT91EbiNand_Map;
	en->bdev.UnMap = AT91EbiNand_UnMap;
	en->bdev.owner = en;
	en->bdev.hw_flags = MEM_FLAG_WRITABLE | MEM_FLAG_READABLE;
	en->eccDev = ecc;
	return &en->bdev;
}
