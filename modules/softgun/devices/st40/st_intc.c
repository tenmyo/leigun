/*
 *************************************************************************************************
 * ST Interrupt controller 
 *
 * State: not implemented 
 *
 * Copyright 2009 Jochen Karrer. All rights reserved.
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
#include <sys/ioctl.h>

#include "bus.h"
#include "sgstring.h"
#include "signode.h"
#include "st_intc.h"

#define INTC_ICR(base)		((base) + 0x00)
#define		ICR_NMIL	(1 << 15)
#define		ICR_MAI		(1 << 14)
#define		ICR_NMIB	(1 << 9)
#define		ICR_NMIE	(1 << 8)
#define 	ICR_IRLM	(1 << 7)

#define INTC_IPRA(base)		((base) + 0x04)
#define INTC_IPRB(base)		((base) + 0x08)
#define INTC_IPRC(base)		((base) + 0x0c)
#define INTC_IPRD(base)		((base) + 0x10)
/* n = 0, 4 , 8 */
#define INTC2_INTPRIOn(n,base)		((base) + 0x300 + ((n) << 0))
#define INTC2_INTREQn(n,base)		((base) + 0x320 + ((n) << 0))
#define INTC2_INTMSKn(n,base)		((base) + 0x340 + ((n) << 0))
#define INTC2_INTMSKCLRn(n,base)	((base) + 0x360 + ((n) << 0))
#define INTC2_INTC2MODE(base)		((base) + 0x380)

typedef struct STIntC {
	BusDevice bdev;
	uint16_t icr;
	uint16_t ipra;
	uint16_t iprb;
	uint16_t iprc;
	uint16_t iprd;
} STIntC;

/*
 *********************************************************************************
 * Interrupt control register (ICR)
 * Bit 15: NMIL NMI-Input level 0 = low, 1 = high.
 * Bit 14: MAI	NMI interrupt mask 0 = enabled, 1 = disabled.
 * Bit 9:  NMIB NMI block mode when BL=1: 0 = hold pending, 1 = detect.
 * Bit 8:  NMIE	NMI edge select.
 * Bit 7:  IRLM 0 = level encoded, 1 = 4 independent interrupt requests. 
 *********************************************************************************
 */
static uint32_t
icr_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "%s: %s not implemented\n", __FILE__, __func__);
	return 0;
}

static void
icr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "%s: %s not implemented\n", __FILE__, __func__);
}

static uint32_t
ipra_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "%s: %s not implemented\n", __FILE__, __func__);
	return 0;
}

static void
ipra_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "%s: %s not implemented\n", __FILE__, __func__);
}

static uint32_t
iprb_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "%s: %s not implemented\n", __FILE__, __func__);
	return 0;
}

static void
iprb_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "%s: %s not implemented\n", __FILE__, __func__);
}

static uint32_t
iprc_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "%s: %s not implemented\n", __FILE__, __func__);
	return 0;
}

static void
iprc_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "%s: %s not implemented\n", __FILE__, __func__);
}

static uint32_t
iprd_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "%s: %s not implemented\n", __FILE__, __func__);
	return 0;
}

static void
iprd_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "%s: %s not implemented\n", __FILE__, __func__);
}

static uint32_t
intprio00_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "%s: %s not implemented\n", __FILE__, __func__);
	return 0;
}

static void
intprio00_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "%s: %s not implemented\n", __FILE__, __func__);
}

static uint32_t
intprio04_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "%s: %s not implemented\n", __FILE__, __func__);
	return 0;
}

static void
intprio04_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "%s: %s not implemented\n", __FILE__, __func__);
}

static uint32_t
intprio08_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "%s: %s not implemented\n", __FILE__, __func__);
	return 0;
}

static void
intprio08_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "%s: %s not implemented\n", __FILE__, __func__);
}

static uint32_t
intreq00_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "%s: %s not implemented\n", __FILE__, __func__);
	return 0;
}

static void
intreq00_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "%s: %s not implemented\n", __FILE__, __func__);
}

static uint32_t
intreq04_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "%s: %s not implemented\n", __FILE__, __func__);
	return 0;
}

static void
intreq04_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "%s: %s not implemented\n", __FILE__, __func__);
}

static uint32_t
intreq08_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "%s: %s not implemented\n", __FILE__, __func__);
	return 0;
}

static void
intreq08_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "%s: %s not implemented\n", __FILE__, __func__);
}

static uint32_t
intmsk00_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "%s: %s not implemented\n", __FILE__, __func__);
	return 0;
}

static void
intmsk00_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "%s: %s not implemented\n", __FILE__, __func__);
}

static uint32_t
intmsk04_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "%s: %s not implemented\n", __FILE__, __func__);
	return 0;
}

static void
intmsk04_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "%s: %s not implemented\n", __FILE__, __func__);
}

static uint32_t
intmsk08_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "%s: %s not implemented\n", __FILE__, __func__);
	return 0;
}

static void
intmsk08_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "%s: %s not implemented\n", __FILE__, __func__);
}

static uint32_t
intmskclr00_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "%s: %s not implemented\n", __FILE__, __func__);
	return 0;
}

static void
intmskclr00_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "%s: %s not implemented\n", __FILE__, __func__);
}

static uint32_t
intmskclr04_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "%s: %s not implemented\n", __FILE__, __func__);
	return 0;
}

static void
intmskclr04_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "%s: %s not implemented\n", __FILE__, __func__);
}

static uint32_t
intmskclr08_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "%s: %s not implemented\n", __FILE__, __func__);
	return 0;
}

static void
intmskclr08_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "%s: %s not implemented\n", __FILE__, __func__);
}

static uint32_t
intc2mode_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "%s: %s not implemented\n", __FILE__, __func__);
	return 0;
}

static void
intc2mode_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "%s: %s not implemented\n", __FILE__, __func__);
}

static void
StIntC_Map(void *owner, uint32_t base, uint32_t mask, uint32_t flags)
{
	STIntC *ic = (STIntC *) owner;
	IOH_New32(INTC_ICR(base), icr_read, icr_write, ic);
	IOH_New32(INTC_IPRA(base), ipra_read, ipra_write, ic);
	IOH_New32(INTC_IPRB(base), iprb_read, iprb_write, ic);
	IOH_New32(INTC_IPRC(base), iprc_read, iprc_write, ic);
	IOH_New32(INTC_IPRD(base), iprd_read, iprd_write, ic);

	IOH_New32(INTC2_INTPRIOn(0, base), intprio00_read, intprio00_write, ic);
	IOH_New32(INTC2_INTPRIOn(4, base), intprio04_read, intprio04_write, ic);
	IOH_New32(INTC2_INTPRIOn(8, base), intprio08_read, intprio08_write, ic);

	IOH_New32(INTC2_INTREQn(0, base), intreq00_read, intreq00_write, ic);
	IOH_New32(INTC2_INTREQn(4, base), intreq04_read, intreq04_write, ic);
	IOH_New32(INTC2_INTREQn(8, base), intreq08_read, intreq08_write, ic);

	IOH_New32(INTC2_INTMSKn(0, base), intmsk00_read, intmsk00_write, ic);
	IOH_New32(INTC2_INTMSKn(4, base), intmsk04_read, intmsk04_write, ic);
	IOH_New32(INTC2_INTMSKn(8, base), intmsk08_read, intmsk08_write, ic);

	IOH_New32(INTC2_INTMSKCLRn(0, base), intmskclr00_read, intmskclr00_write, ic);
	IOH_New32(INTC2_INTMSKCLRn(4, base), intmskclr04_read, intmskclr04_write, ic);
	IOH_New32(INTC2_INTMSKCLRn(8, base), intmskclr08_read, intmskclr08_write, ic);

	IOH_New32(INTC2_INTC2MODE(base), intc2mode_read, intc2mode_write, ic);
}

static void
StIntC_UnMap(void *owner, uint32_t base, uint32_t mask)
{
	int i;
	IOH_Delete32(INTC_ICR(base));
	IOH_Delete32(INTC_IPRA(base));
	IOH_Delete32(INTC_IPRB(base));
	IOH_Delete32(INTC_IPRC(base));
	IOH_Delete32(INTC_IPRD(base));
	for (i = 0; i < 12; i += 4) {
		IOH_Delete32(INTC2_INTPRIOn(0, base));
		IOH_Delete32(INTC2_INTREQn(0, base));
		IOH_Delete32(INTC2_INTMSKn(0, base));
		IOH_Delete32(INTC2_INTMSKCLRn(0, base));
	}
	IOH_Delete32(INTC2_INTC2MODE(base));
}

BusDevice *
StIntc_New(const char *devname)
{
	STIntC *ic = sg_new(STIntC);
	ic->bdev.first_mapping = NULL;
	ic->bdev.Map = StIntC_Map;
	ic->bdev.UnMap = StIntC_UnMap;
	ic->bdev.owner = ic;
	ic->bdev.hw_flags = MEM_FLAG_WRITABLE | MEM_FLAG_READABLE;
	return &ic->bdev;
}
