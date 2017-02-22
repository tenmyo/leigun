/*
 *************************************************************************************************
 * Emulation of S3C2410 Memory Controller
 *
 * state: Not implemented
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

#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <bus.h>
#include <signode.h>
#include <configfile.h>
#include "s3c2410_memco.h"
#include <sgstring.h>

/* Base is 0x48000000 */
#define MC_BWSCON	((base)+0x00)
#define MC_BANKON0	((base)+0x04)
#define MC_BANKON1	((base)+0x08)
#define MC_BANKON2	((base)+0x0c)
#define MC_BANKON3	((base)+0x10)
#define MC_BANKON4	((base)+0x14)
#define MC_BANKON5	((base)+0x18)
#define MC_BANKON6	((base)+0x1C)
#define MC_BANKON7	((base)+0x20)
#define MC_BANKON(n)	((base)+0x04+(n<<2))
#define MC_REFRESH	((base)+0x24)
#define MC_BANKSIZE	((base)+0x28)
#define MC_MRSRB6	((base)+0x2c)
#define MC_MRSRB7	((base)+0x30)

typedef struct Memco {
	BusDevice bdev;
	uint32_t bwscon;
} Memco;

static uint32_t
bwscon_read(void *clientData, uint32_t address, int rqlen)
{
	Memco *mc = (Memco *) clientData;
	return mc->bwscon;
}

static void
bwscon_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
}

static uint32_t
bankon0_read(void *clientData, uint32_t address, int rqlen)
{
	return 0;
}

static void
bankon0_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
}

static uint32_t
bankon1_read(void *clientData, uint32_t address, int rqlen)
{
	return 0;
}

static void
bankon1_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
}

static uint32_t
bankon2_read(void *clientData, uint32_t address, int rqlen)
{
	return 0;
}

static void
bankon2_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
}

static uint32_t
bankon3_read(void *clientData, uint32_t address, int rqlen)
{
	return 0;
}

static void
bankon3_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
}

static uint32_t
bankon4_read(void *clientData, uint32_t address, int rqlen)
{
	return 0;
}

static void
bankon4_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
}

static uint32_t
bankon5_read(void *clientData, uint32_t address, int rqlen)
{
	return 0;
}

static void
bankon5_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
}

static uint32_t
bankon6_read(void *clientData, uint32_t address, int rqlen)
{
	return 0;
}

static void
bankon6_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
}

static uint32_t
bankon7_read(void *clientData, uint32_t address, int rqlen)
{
	return 0;
}

static void
bankon7_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
}

static uint32_t
refresh_read(void *clientData, uint32_t address, int rqlen)
{
	return 0;
}

static void
refresh_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
}

static uint32_t
banksize_read(void *clientData, uint32_t address, int rqlen)
{
	return 0;
}

static void
banksize_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
}

static uint32_t
mrsrb6_read(void *clientData, uint32_t address, int rqlen)
{
	return 0;
}

static void
mrsrb6_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
}

static uint32_t
mrsrb7_read(void *clientData, uint32_t address, int rqlen)
{
	return 0;
}

static void
mrsrb7_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
}

static void
S3C2410Memco_Map(void *owner, uint32_t base, uint32_t mask, uint32_t flags)
{
	Memco *mc = (Memco *) owner;
	IOH_New32(MC_BWSCON, bwscon_read, bwscon_write, mc);
	IOH_New32(MC_BANKON0, bankon0_read, bankon0_write, mc);
	IOH_New32(MC_BANKON1, bankon1_read, bankon1_write, mc);
	IOH_New32(MC_BANKON2, bankon2_read, bankon2_write, mc);
	IOH_New32(MC_BANKON3, bankon3_read, bankon3_write, mc);
	IOH_New32(MC_BANKON4, bankon4_read, bankon4_write, mc);
	IOH_New32(MC_BANKON5, bankon5_read, bankon5_write, mc);
	IOH_New32(MC_BANKON6, bankon6_read, bankon6_write, mc);
	IOH_New32(MC_BANKON7, bankon7_read, bankon7_write, mc);
	IOH_New32(MC_REFRESH, refresh_read, refresh_write, mc);
	IOH_New32(MC_BANKSIZE, banksize_read, banksize_write, mc);
	IOH_New32(MC_MRSRB6, mrsrb6_read, mrsrb6_write, mc);
	IOH_New32(MC_MRSRB7, mrsrb7_read, mrsrb7_write, mc);
}

static void
S3C2410Memco_UnMap(void *owner, uint32_t base, uint32_t mask)
{
	IOH_Delete32(MC_BWSCON);
	IOH_Delete32(MC_BANKON0);
	IOH_Delete32(MC_BANKON1);
	IOH_Delete32(MC_BANKON2);
	IOH_Delete32(MC_BANKON3);
	IOH_Delete32(MC_BANKON4);
	IOH_Delete32(MC_BANKON5);
	IOH_Delete32(MC_BANKON6);
	IOH_Delete32(MC_BANKON7);
	IOH_Delete32(MC_REFRESH);
	IOH_Delete32(MC_BANKSIZE);
	IOH_Delete32(MC_MRSRB6);
	IOH_Delete32(MC_MRSRB7);
}

BusDevice *
S3C2410_MemcoNew(const char *name)
{
	Memco *mc = sg_new(Memco);
	mc->bdev.first_mapping = NULL;
	mc->bdev.Map = S3C2410Memco_Map;
	mc->bdev.UnMap = S3C2410Memco_UnMap;
	mc->bdev.owner = mc;
	mc->bdev.hw_flags = MEM_FLAG_WRITABLE | MEM_FLAG_READABLE;
	return &mc->bdev;
}
