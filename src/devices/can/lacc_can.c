/*
 ***************************************************************************************************
 *
 * Lightmaze ARM-Controller CAN-Module Emulation module 
 * 	This programm assembles two SJA1000 CAN-Controller Chips
 *	into one device
 *	
 * Copyright 2004 Jochen Karrer. All rights reserved.
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

#include <string.h>
#include "bus.h"
#include "sja1000.h"
#include "lacc_can.h"
#include "ns9750_timer.h"	/* Unclean, interrupts should be done with signals */
#include "sgstring.h"

#define NR_SJAS (2)
typedef struct LaccCAN {
	BusDevice bdev;
	SJA1000 *sja[NR_SJAS];
} LaccCAN;

/*
 * --------------------------------------------
 * Identify CPLD 
 * --------------------------------------------
 */
static uint32_t
version_read(void *clientData, uint32_t addr, int rqlen)
{
	return 0x13;
}

static void
LaccCAN_Map(void *module_owner, uint32_t base, uint32_t mapsize, uint32_t flags)
{
	LaccCAN *lcan = module_owner;
	int i;
	for (i = 0; i < NR_SJAS; i++) {
		//printf("Map at %08x\n",base+i*0x200);
		SJA1000_Map(lcan->sja[i], base + i * 0x200);
	}
	IOH_New8(base + 0x3ff, version_read, NULL, NULL);

}

static void
LaccCAN_UnMap(void *module_owner, uint32_t base, uint32_t mapsize)
{
	LaccCAN *lcan = module_owner;
	int i;
	for (i = 0; i < NR_SJAS; i++) {
		//printf("UnMap at %08x\n",base+i*0x200);
		SJA1000_UnMap(lcan->sja[i], base + i * 0x200);
	}
	IOH_Delete8(base + 0x3ff);
}

BusDevice *
LaccCAN_New()
{
	LaccCAN *lcan = sg_new(LaccCAN);

	lcan->bdev.first_mapping = NULL;
	lcan->bdev.Map = LaccCAN_Map;
	lcan->bdev.UnMap = LaccCAN_UnMap;
	lcan->bdev.owner = lcan;
	lcan->bdev.hw_flags = MEM_FLAG_WRITABLE | MEM_FLAG_READABLE;
	lcan->sja[0] = SJA1000_New(&lcan->bdev, "acc_can0");
	lcan->sja[1] = SJA1000_New(&lcan->bdev, "acc_can1");
	fprintf(stderr, "Dual SJA1000 CAN-Controller module created\n");
	return &lcan->bdev;
}
