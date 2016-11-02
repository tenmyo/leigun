/*
 *************************************************************************************************
 *
 * Emulation of the NS9750 USB Controller 
 *
 * Status:
 *	Enabling and disabling the OHCI host controller
 *	works. 
 *	Interrupt status and enable is implemented but
 *	untested
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

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <bus.h>
#include <ns9750_bbus.h>
#include <ns9750_usb.h>
#include <usb_ohci.h>
#include "signode.h"
#include "sgstring.h"

#if 0
#define dbgprintf(x...) { fprintf(stderr,x); }
#else
#define dbgprintf(x...)
#endif

typedef struct NS9750Usb NS9750Usb;

typedef struct IrqTraceInfo {
	int irqnr;
	NS9750Usb *usb;
} IrqTraceInfo;
struct NS9750Usb {
	uint32_t gctrl;
	uint32_t dctrl;
	uint32_t gien;
	uint32_t gist;
	uint32_t dev_ipr;
	BusDevice bdev;
	BusDevice *ohcidev;
	//SigNode *irqOut;
	SigNode *irqIn[32];	/* not all are used */
	IrqTraceInfo irq_trace_info[32];
	int interrupt_posted;
};

static void
usb_update_interrupts(NS9750Usb * usb)
{
	uint32_t mask = 0;
	if (usb->gien & NS9750_USB_GIEN_GBL_EN) {
		mask = 0xffffffff;
	}
	if (!(usb->gien & NS9750_USB_GIEN_GBL_DMA)) {
		mask = mask & 0x1fff;
	}
	if (usb->gist & usb->gien & mask) {
		if (!usb->interrupt_posted) {
			BBus_PostIRQ(BB_IRQ_USB);
			usb->interrupt_posted = 1;
		}
	} else {
		if (usb->interrupt_posted) {
			BBus_UnPostIRQ(BB_IRQ_USB);
			usb->interrupt_posted = 0;
		}
	}
}

static void
irq_change(SigNode * node, int value, void *clientData)
{
	IrqTraceInfo *ti = (IrqTraceInfo *) clientData;
	NS9750Usb *usb = ti->usb;
	int irq = ti->irqnr;
	if (value == SIG_LOW) {
		usb->gist |= (1 << irq);
	} else {
		usb->gist &= ~(1 << irq);
	}
	usb_update_interrupts(usb);
}

/*
 * -----------------------------------------------------------
 * Documentation about the GCTRL DRST Bits seems to be wrong
 * -----------------------------------------------------------
 */

static uint32_t
usb_gctrl_read(void *clientData, uint32_t address, int rqlen)
{
	NS9750Usb *usb = clientData;
	if (usb->gctrl & NS9750_USB_GCTRL_HSTDEV) {
		return NS9750_USB_GCTRL_HSTDEV | 0x823;
	} else {
		return usb->gctrl | 0x602;
	}
}

static void
usb_gctrl_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	NS9750Usb *usb = clientData;
	uint32_t diff = value ^ usb->gctrl;
	dbgprintf("GCTRL Write value %08x\n", value);
	usb->gctrl = value;
	if (diff & (NS9750_USB_GCTRL_HSTDEV)) {
		if (((value & NS9750_USB_GCTRL_HRST) == 0)) {
			OhciHC_Enable(usb->ohcidev);
		} else {
			// Does a reset of all registers
			OhciHC_Disable(usb->ohcidev);
		}
	}
	return;
}

static uint32_t
usb_dctrl_read(void *clientData, uint32_t address, int rqlen)
{
	NS9750Usb *usb = clientData;
	return usb->dctrl;
}

static void
usb_dctrl_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	NS9750Usb *usb = clientData;
	usb->dctrl = value;
	return;
}

static uint32_t
usb_gien_read(void *clientData, uint32_t address, int rqlen)
{
	NS9750Usb *usb = clientData;
	return usb->gien;
}

static void
usb_gien_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	NS9750Usb *usb = clientData;
	usb->gien = value;
	usb_update_interrupts(usb);
	return;
}

static uint32_t
usb_gist_read(void *clientData, uint32_t address, int rqlen)
{
	NS9750Usb *usb = clientData;
	if (usb->gist & 0x07ffc000) {
		return usb->gist | NS9750_USB_GIEN_GBL_DMA;
	} else {
		return usb->gist;
	}
}

/*
 * -----------------------------------------------
 * Interrupt Status is toggled when writing a 1
 * -----------------------------------------------
 */
static void
usb_gist_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	NS9750Usb *usb = clientData;
	uint32_t xor = value & 0x3c2;
	// what to do with the other fields ?   
	usb->gist ^= xor;
	usb_update_interrupts(usb);
}

static uint32_t
usb_dev_ipr_read(void *clientData, uint32_t address, int rqlen)
{
	NS9750Usb *usb = clientData;
	return usb->dev_ipr;
}

static void
usb_dev_ipr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	NS9750Usb *usb = clientData;
	usb->dev_ipr = value;
}

/*
 * ------------------------------------------------------------------
 *  This ignores all given Baseaddresses because 
 *  the addresses are unchangeable
 * ------------------------------------------------------------------
 */
static void
NS9750Usb_Map(void *owner, uint32_t base, uint32_t mapsize, uint32_t flags)
{
	NS9750Usb *usb = owner;
	if (base != NS9750_USB_BASE) {
		fprintf(stderr, "Error: NS9750 USB is not remappable to address %08x\n", base);
		exit(3245);
	}
	IOH_New32(NS9750_USB_GCTRL, usb_gctrl_read, usb_gctrl_write, usb);
	IOH_New32(NS9750_USB_DCTRL, usb_dctrl_read, usb_dctrl_write, usb);
	IOH_New32(NS9750_USB_GIEN, usb_gien_read, usb_gien_write, usb);
	IOH_New32(NS9750_USB_GIST, usb_gist_read, usb_gist_write, usb);
	IOH_New32(NS9750_USB_DEV_IPR, usb_dev_ipr_read, usb_dev_ipr_write, usb);
}

static void
NS9750Usb_UnMap(void *owner, uint32_t base, uint32_t mapsize)
{
	IOH_Delete32(NS9750_USB_GCTRL);
	IOH_Delete32(NS9750_USB_DCTRL);
	IOH_Delete32(NS9750_USB_GIEN);
	IOH_Delete32(NS9750_USB_GIST);
	IOH_Delete32(NS9750_USB_DEV_IPR);
}

BusDevice *
NS9750Usb_New(const char *name)
{
	NS9750Usb *usb;
	char *str;
	int i;
	usb = sg_new(NS9750Usb);
	for (i = 0; i < 32; i++) {
		IrqTraceInfo *ti = &usb->irq_trace_info[i];
		usb->irqIn[i] = SigNode_New("%s.irq%d", name, i);
		if (!usb->irqIn[i]) {
			fprintf(stderr, "NS9750Usb: Can not create irq line\n");
			exit(1);
		}
		SigNode_Trace(usb->irqIn[i], irq_change, ti);
	}
	usb->gctrl = 0x823;	// real device says this, manual says 0xe03
	usb->dctrl = 0x0;
	usb->gien = 0;

	// GIST startup value with device 0xa1f, without 0x801, reset value 0x200
	usb->gist = 0xa1f;
	usb->dev_ipr = 0;
	usb->bdev.first_mapping = NULL;
	usb->bdev.Map = NS9750Usb_Map;
	usb->bdev.UnMap = NS9750Usb_UnMap;
	usb->bdev.owner = usb;
	usb->bdev.hw_flags = MEM_FLAG_WRITABLE | MEM_FLAG_READABLE;
	usb->bdev.read32 = Bus_Read32;
	usb->bdev.write32 = Bus_Write32;
	usb->ohcidev = OhciHC_New("ns9750_ohci", MainBus);
	if (!usb->ohcidev) {
		fprintf(stderr, "Can't create ohcidev\n");
		exit(4324);
	}
	str = alloca(strlen(name) + 50);
	sprintf(str, "%s.irq%d", name, NS9750_USB_IRQ_OHCI);
	SigName_Link("ns9750_ohci.irq", str);
	/* We add the mapping here because we always have the same address */
	Mem_AreaAddMapping(&usb->bdev, NS9750_USB_BASE, 0x20000,
			   MEM_FLAG_WRITABLE | MEM_FLAG_READABLE);
	Mem_AreaAddMapping(usb->ohcidev, NS9750_OHCI_BASE, 0x20000,
			   MEM_FLAG_WRITABLE | MEM_FLAG_READABLE);
	fprintf(stderr, "NS9750 OHCI USB Host Controller created\n");
	return &usb->bdev;
}
