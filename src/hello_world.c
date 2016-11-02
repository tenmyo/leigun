/*
 *************************************************************************************************
 *
 * The hello world emulator is an example or a template for emulator 
 * module programmers
 *
 * State: working but does nothing which makes sense 
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

#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include "bus.h"
#include "signode.h"
#include "cycletimer.h"
#include "sgstring.h"

#if 0
#define dbgprintf(x...) { fprintf(stderr,x); }
#else
#define dbgprintf(x...)
#endif

#define HELLO_PRINT_REG(base)	((base)+0)
#define HELLO_INT_TRIGGER(base)	((base)+0x4)
#define HELLO_INT_MASK(base)	((base)+0x8)
#define		INT_SOFTINT	(1<<4)
#define HELLO_INT_STATUS(base)	((base)+0xc)

#define MAX_PHYS	(32)
typedef struct HelloWorldDev {
	BusDevice bdev;		/* HelloWorldDev inherits from BusDevice */
	char *name;		/* The instance name                     */
	SigNode *irqNode;	/* The interrupt Line                    */

	/* The registers */
	uint32_t int_mask;
	uint32_t int_status;
} HelloWorldDev;

/*
 * --------------------------------------------------------------------------------------
 * update_interrupt  is called whenever a condition changes which might
 * have an effect on the interrupt (Interrupt mask change, Interrupt clear command ...)
 * --------------------------------------------------------------------------------------
 */
static void
update_interrupt(HelloWorldDev * hwd)
{
	if (hwd->int_status & hwd->int_mask) {
		SigNode_Set(hwd->irqNode, SIG_LOW);
	} else {
		SigNode_Set(hwd->irqNode, SIG_PULLUP);
	}
}

/*
 * --------------------------------------------------------
 * The hello_print register: It returns 0x08154711 on read
 * and prints "Hello World" to the stderr of the emulator 
 * --------------------------------------------------------
 */
static uint32_t
hello_print_read(void *clientData, uint32_t address, int rqlen)
{
	//HelloWorldDev *hwd = (HelloWorldDev *)clientData;
	return 0x08154711;
}

static void
hello_print_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "Hello world %08x\n", value);
}

/*
 * -----------------------------------------------------
 * The Interrupt trigger register: Reading returns 0 
 * Writing any value triggers an Interrupt by setting
 * the SOFTINT bit in the interrupt status register
 * -----------------------------------------------------
 */
static uint32_t
int_trigger_read(void *clientData, uint32_t address, int rqlen)
{
	return 0;
}

static void
int_trigger_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	HelloWorldDev *hwd = (HelloWorldDev *) clientData;
	hwd->int_status |= INT_SOFTINT;
	update_interrupt(hwd);
}

/*
 * ----------------------------------------------------------------------
 * The Interrupt mask register allows enabling/disabling the Interrupts
 * Reading returns the current mask, writing sets the interrupt mask
 * ----------------------------------------------------------------------
 */
static uint32_t
int_mask_read(void *clientData, uint32_t address, int rqlen)
{
	HelloWorldDev *hwd = (HelloWorldDev *) clientData;
	return hwd->int_mask;
}

static void
int_mask_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	HelloWorldDev *hwd = (HelloWorldDev *) clientData;
	hwd->int_mask = value;
	update_interrupt(hwd);
}

/*
 * --------------------------------------------------------------
 * Interrupt status register: contains the interrupts which
 * are currently pending. Writing a one to a bit clears the
 * corresponding interrupt  (W1C)
 * --------------------------------------------------------------
 */
static uint32_t
int_status_read(void *clientData, uint32_t address, int rqlen)
{
	HelloWorldDev *hwd = (HelloWorldDev *) clientData;
	return hwd->int_status;
}

static void
int_status_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	HelloWorldDev *hwd = (HelloWorldDev *) clientData;
	hwd->int_status &= ~value;
	update_interrupt(hwd);
}

static void
HelloWorldDev_Map(void *owner, uint32_t base, uint32_t mask, uint32_t flags)
{
	HelloWorldDev *hwd = (HelloWorldDev *) owner;
	IOH_New32(HELLO_PRINT_REG(base), hello_print_read, hello_print_write, hwd);
	IOH_New32(HELLO_INT_TRIGGER(base), int_trigger_read, int_trigger_write, hwd);
	IOH_New32(HELLO_INT_MASK(base), int_mask_read, int_mask_write, hwd);
	IOH_New32(HELLO_INT_STATUS(base), int_status_read, int_status_write, hwd);
}

static void
HelloWorldDev_UnMap(void *owner, uint32_t base, uint32_t mask)
{
	IOH_Delete32(HELLO_PRINT_REG(base));
	IOH_Delete32(HELLO_INT_TRIGGER(base));
	IOH_Delete32(HELLO_INT_MASK(base));
	IOH_Delete32(HELLO_INT_STATUS(base));

}

BusDevice *
HelloWorldDev_New(const char *name)
{
	HelloWorldDev *hwd = sg_new(HelloWorldDev);
	hwd->irqNode = SigNode_New("%s.irq", name);
	if (!hwd->irqNode) {
		fprintf(stderr, "HelloWorldDevice: Can't create interrupt request line\n");
		exit(1);
	}
	SigNode_Set(hwd->irqNode, SIG_PULLDOWN);

	/* Initialize the registers */
	hwd->int_mask = 0x00;
	hwd->bdev.first_mapping = NULL;
	hwd->bdev.Map = HelloWorldDev_Map;
	hwd->bdev.UnMap = HelloWorldDev_UnMap;
	hwd->bdev.owner = hwd;
	hwd->bdev.hw_flags = MEM_FLAG_WRITABLE | MEM_FLAG_READABLE;
	fprintf(stderr, "HelloWorld Device \"%s\" created\n", name);
	return &hwd->bdev;
}
