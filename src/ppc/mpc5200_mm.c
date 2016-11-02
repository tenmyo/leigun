/*
 *************************************************************************************************
 * Emulation of MPC5200B Memory Management Module 
 *
 * state:  Not implemented
 *
 * Copyright 2008 Jochen Karrer. All rights reserved.
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
#include <time.h>
#include <sys/ioctl.h>
#include <sys/fcntl.h>
#include <bus.h>
#include <fio.h>
#include <signode.h>
#include <configfile.h>
#include <clock.h>
#include <cycletimer.h>
#include <sgstring.h>
#include <serial.h>

#define MM_MABR(base)		((base) + 0x00)
#define MM_CS0STA(base)		((base) + 0x04)
#define MM_CS0STO(base)		((base) + 0x08)
#define MM_CS1STA(base)		((base) + 0x0c)
#define MM_CS1STO(base) 	((base) + 0x10)
#define MM_CS2STA(base)		((base) + 0x14)
#define MM_CS2STO(base)		((base) + 0x18)
#define MM_CS3STA(base)		((base) + 0x1c)
#define MM_CS3STO(base)		((base) + 0x20)
#define MM_CS4STA(base)		((base) + 0x24)
#define MM_CS4STO(base)		((base) + 0x28)
#define MM_CS5STA(base)		((base) + 0x2c)
#define MM_CS5STO(base) 	((base) + 0x30)

#define MM_SDRAM_CS0(base)	((base) + 0x34)
#define MM_SDRAM_CS1(base)	((base) + 0x38)

#define MM_BOOTSTA(base) 	((base) + 0x4c)
#define MM_BOOTSTO(base)	((base) + 0x50)
#define	MM_IPBI_CR(base)	((base) + 0x54)
#define MM_CS6STA(base)		((base) + 0x58)
#define MM_CS6STO(base)		((base) + 0x5c)
#define MM_CS7STA(base)		((base) + 0x60)
#define MM_CS7STO(base)		((base) + 0x64)

typedef struct MM {
	BusDevice bdev;
	BusDevice *csdev[8];
	BusDevice *sdram[2];
	uint32_t cs_sta[8];
	uint32_t cs_sto[8];
	uint32_t sdram_cs[2];
} MM;

static uint32_t
mabr_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "MM MABR: register not implemented\n");
	return 0;
}

static void
mabr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "MM MABR: register not implemented\n");
}

static uint32_t
cs0sta_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "MM CS0STA: register not implemented\n");
	return 0;
}

static void
cs0sta_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "MM CS0STA: register not implemented\n");
}

static uint32_t
cs0sto_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "MM CS0STO: register not implemented\n");
	return 0;
}

static void
cs0sto_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "MM CS0STO: register not implemented\n");
}

static uint32_t
cs1sta_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "MM CS1STA: register not implemented\n");
	return 0;
}

static void
cs1sta_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "MM CS1STA: register not implemented\n");
}

static uint32_t
cs1sto_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "MM CS1STO: register not implemented\n");
	return 0;
}

static void
cs1sto_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "MM CS1STO: register not implemented\n");
}

static uint32_t
cs2sta_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "MM CS2STA: register not implemented\n");
	return 0;
}

static void
cs2sta_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "MM CS2STA: register not implemented\n");
}

static uint32_t
cs2sto_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "MM CS2STO: register not implemented\n");
	return 0;
}

static void
cs2sto_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "MM CS2STO: register not implemented\n");
}

static uint32_t
cs3sta_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "MM CS3STA: register not implemented\n");
	return 0;
}

static void
cs3sta_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "MM CS3STA: register not implemented\n");
}

static uint32_t
cs3sto_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "MM CS3STO: register not implemented\n");
	return 0;
}

static void
cs3sto_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "MM CS3STO: register not implemented\n");
}

static uint32_t
cs4sta_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "MM CS4STA: register not implemented\n");
	return 0;
}

static void
cs4sta_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "MM CS4STA: register not implemented\n");
}

static uint32_t
cs4sto_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "MM CS4STO: register not implemented\n");
	return 0;
}

static void
cs4sto_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "MM CS4STO: register not implemented\n");
}

static uint32_t
cs5sta_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "MM CS5STA: register not implemented\n");
	return 0;
}

static void
cs5sta_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "MM CS5STA: register not implemented\n");
}

static uint32_t
cs5sto_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "MM CS5STO: register not implemented\n");
	return 0;
}

static void
cs5sto_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "MM CS5STO: register not implemented\n");
}

static uint32_t
cs6sta_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "MM CS6STA: register not implemented\n");
	return 0;
}

static void
cs6sta_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "MM CS6STA: register not implemented\n");
}

static uint32_t
cs6sto_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "MM CS6STO: register not implemented\n");
	return 0;
}

static void
cs6sto_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "MM CS6STO: register not implemented\n");
}

static uint32_t
cs7sta_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "MM CS7STA: register not implemented\n");
	return 0;
}

static void
cs7sta_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "MM CS7STA: register not implemented\n");
}

static uint32_t
cs7sto_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "MM CS7STO: register not implemented\n");
	return 0;
}

static void
cs7sto_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "MM CS7STO: register not implemented\n");
}

static uint32_t
sdram_cs0_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "MM SDRAM_CS0: register not implemented\n");
	return 0;
}

static void
sdram_cs0_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "MM SDRAM_CS0: register not implemented\n");
}

static uint32_t
sdram_cs1_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "MM SDRAM_CS1: register not implemented\n");
	return 0;
}

static void
sdram_cs1_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "MM SDRAM_CS1: register not implemented\n");
}

static uint32_t
bootsta_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "MM Boot Start: register not implemented\n");
	return 0;
}

static void
bootsta_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "MM Boot start: register not implemented\n");
}

static uint32_t
bootsto_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "MM Boot stop: register not implemented\n");
	return 0;
}

static void
bootsto_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "MM Boot stop: register not implemented\n");
}

static uint32_t
ipbi_cr_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "MM IPBI_CR: register not implemented\n");
	return 0;
}

static void
ipbi_cr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "MM IPBI_CR: register not implemented\n");
}

static void
MM_Unmap(void *owner, uint32_t base, uint32_t mask)
{
//        IOH_Delete8(PSC_MR1(base));
}

static void
MM_Map(void *owner, uint32_t base, uint32_t mask, uint32_t mapflags)
{

	MM *mm = (MM *) owner;
	IOH_New32(MM_MABR(base), mabr_read, mabr_write, mm);
	IOH_New32(MM_CS0STA(base), cs0sta_read, cs0sta_write, mm);
	IOH_New32(MM_CS0STO(base), cs0sto_read, cs0sto_write, mm);
	IOH_New32(MM_CS1STA(base), cs1sta_read, cs1sta_write, mm);
	IOH_New32(MM_CS1STO(base), cs1sto_read, cs1sto_write, mm);
	IOH_New32(MM_CS2STA(base), cs2sta_read, cs2sta_write, mm);
	IOH_New32(MM_CS2STO(base), cs2sto_read, cs2sto_write, mm);
	IOH_New32(MM_CS3STA(base), cs3sta_read, cs3sta_write, mm);
	IOH_New32(MM_CS3STO(base), cs3sto_read, cs3sto_write, mm);
	IOH_New32(MM_CS4STA(base), cs4sta_read, cs4sta_write, mm);
	IOH_New32(MM_CS4STO(base), cs4sto_read, cs4sto_write, mm);
	IOH_New32(MM_CS5STA(base), cs5sta_read, cs5sta_write, mm);
	IOH_New32(MM_CS5STO(base), cs5sto_read, cs5sto_write, mm);

	IOH_New32(MM_SDRAM_CS0(base), sdram_cs0_read, sdram_cs0_write, mm);
	IOH_New32(MM_SDRAM_CS1(base), sdram_cs1_read, sdram_cs1_write, mm);

	IOH_New32(MM_BOOTSTA(base), bootsta_read, bootsta_write, mm);
	IOH_New32(MM_BOOTSTO(base), bootsto_read, bootsto_write, mm);
	IOH_New32(MM_IPBI_CR(base), ipbi_cr_read, ipbi_cr_write, mm);
	IOH_New32(MM_CS6STA(base), cs6sta_read, cs6sta_write, mm);
	IOH_New32(MM_CS6STO(base), cs6sto_read, cs6sto_write, mm);
	IOH_New32(MM_CS7STA(base), cs7sta_read, cs7sta_write, mm);
	IOH_New32(MM_CS7STO(base), cs7sto_read, cs7sto_write, mm);
}

BusDevice *
MPC5200_MMNew(const char *name)
{
	MM *mm = sg_calloc(sizeof(MM));
	mm->bdev.first_mapping = NULL;
	mm->bdev.Map = MM_Map;
	mm->bdev.UnMap = MM_Unmap;
	mm->bdev.owner = mm;
	mm->bdev.hw_flags = MEM_FLAG_WRITABLE | MEM_FLAG_READABLE;
	return &mm->bdev;
}
