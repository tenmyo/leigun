/*
 *************************************************************************************************
 * Emulation of Coldfire Programmable Interrupt Timers 
 *
 * State: not implemented
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
#include <signode.h>
#include <configfile.h>
#include <clock.h>
#include <cycletimer.h>
#include <sgstring.h>
#include "coldfire/mcf5282_pit.h"

#define PIT_PSCR(base)	((base) + 0x0)
#define		PSCR_PRE_MASK	(0xf<<8)
#define		PSCR_PRE_SHIFT	(8)
#define 	PSCR_DOZE	(1<<6)
#define		PSCR_HALTED	(1<<5)
#define 	PSCR_OVW	(1<<4)
#define		PSCR_PIE	(1<<3)
#define		PSCR_PIF	(1<<2)
#define		PSCR_RLD	(1<<1)
#define		PSCR_EN		(1<<0)

#define PIT_PMR(base)	((base) + 0x2)
#define PIT_PCNTR(base)	((base) + 0x4)

typedef struct Pit {
	BusDevice bdev;
	Clock_t *clockIn;
	CycleCounter_t last_counter_update;
	CycleCounter_t remainder;
	uint16_t pcsr;
	uint16_t pmr;
	uint16_t pcntr;
} Pit;

static void
actualize_counter(Pit * pit)
{
	int pre;
	uint32_t prediv;
//      uint32_t cycles_per_period;
//      Frequency_t freq = Clock_Freq(pit->clockIn); 
	pit->remainder += CycleCounter_Get() - pit->last_counter_update;
	pre = (pit->pcsr >> 12) & 0xf;
	prediv = (4 << pre);
//      cycles_per_period = 
}

static uint32_t
pcsr_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "PIT pcsr not implented\n");
	return 0;
}

static void
pcsr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	Pit *pit = (Pit *) clientData;
	pit->pcsr = value;
	// update 
	fprintf(stderr, "PIT pcsr not implented\n");
}

static uint32_t
pmr_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "PIT pmr not implented\n");
	return 0;
}

static void
pmr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "PIT pmr not implented\n");
}

static uint32_t
pcntr_read(void *clientData, uint32_t address, int rqlen)
{
	Pit *pit = (Pit *) clientData;
	actualize_counter(pit);
	return pit->pcntr;
}

static void
pcntr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "PIT pcntr not implented\n");
}

static void
Pit_Unmap(void *owner, uint32_t base, uint32_t mask)
{
	IOH_Delete16(PIT_PSCR(base));
	IOH_Delete16(PIT_PMR(base));
	IOH_Delete16(PIT_PCNTR(base));
}

static void
Pit_Map(void *owner, uint32_t base, uint32_t mask, uint32_t mapflags)
{
	Pit *pit = (Pit *) owner;
	IOH_New16(PIT_PSCR(base), pcsr_read, pcsr_write, pit);
	IOH_New16(PIT_PMR(base), pmr_read, pmr_write, pit);
	IOH_New16(PIT_PCNTR(base), pcntr_read, pcntr_write, pit);
}

BusDevice *
MCF5282_PitNew(const char *name)
{
	Pit *pit = sg_calloc(sizeof(Pit));
	pit->pcsr = 0;
	pit->pmr = 0xffff;
	pit->pcntr = 0xffff;
	pit->bdev.first_mapping = NULL;
	pit->bdev.Map = Pit_Map;
	pit->bdev.UnMap = Pit_Unmap;
	pit->bdev.owner = pit;
	pit->bdev.hw_flags = MEM_FLAG_WRITABLE | MEM_FLAG_READABLE;
	return &pit->bdev;
}
