/*
 *************************************************************************************************
 *
 * Emulation of Freescale i.MX21 GPIO module 
 *
 * state: 
 *	Should work, not tested. 
 *      Primary and alternate functions not implemented 
 *	Interrupt on positive edge is tested 
 *
 *	Docu does't tell how input from pad to Primary/Alternate function
 *	is connected. I suspect that it is always connected, no matter
 *	which mode. For example RTS Change interrupt on PB26 works
 *	even if in gpio mode
 *
 * Copyright 2006 Jochen Karrer. All rights reserved.
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
#include <sys/fcntl.h>
#include "bus.h"
#include "fio.h"
#include "signode.h"
#include "imx21_gpio.h"
#include "configfile.h"
#include "sgstring.h"

//define IMX_GPIO_BASE              (0x15000 + IMX_IO_BASE)

#define GPIO_DDIR(base,x) 	((base) + 0x00 + (x) * 0x100)
#define GPIO_OCR1(base,x) 	((base) + 0x04 + (x) * 0x100)
#define GPIO_OCR2(base,x) 	((base) + 0x08 + (x) * 0x100)
#define		OCR_AIN	(0)
#define		OCR_BIN	(1)
#define		OCR_CIN	(2)
#define		OCR_DR  (3)
#define GPIO_ICONFA1(base,x)	((base) + 0x0c + (x) * 0x100)
#define GPIO_ICONFA2(base,x) 	((base) + 0x10 + (x) * 0x100)
#define GPIO_ICONFB1(base,x) 	((base) + 0x14 + (x) * 0x100)
#define GPIO_ICONFB2(base,x) 	((base) + 0x18 + (x) * 0x100)
#define GPIO_DR(base,x)      	((base) + 0x1c + (x) * 0x100)
#define GPIO_GIUS(base,x)    	((base) + 0x20 + (x) * 0x100)
#define GPIO_SSR(base,x)     	((base) + 0x24 + (x) * 0x100)
#define GPIO_ICR1(base,x)    	((base) + 0x28 + (x) * 0x100)
#define GPIO_ICR2(base,x)    	((base) + 0x2c + (x) * 0x100)
#define GPIO_IMR(base,x)     	((base) + 0x30 + (x) * 0x100)
#define GPIO_ISR(base,x)     	((base) + 0x34 + (x) * 0x100)
#define GPIO_GPR(base,x)     	((base) + 0x38 + (x) * 0x100)
#define GPIO_SWR(base,x)     	((base) + 0x3c + (x) * 0x100)
#define		SWR_RESET	(1<<0)
#define GPIO_PUEN(base,x)    	((base) + 0x40 + (x) * 0x100)
#define GPIO_PMASK(base)	((base) + 0x600)

#define NUM_PORTS (6)

struct IMX21Gpio;
struct GpioPort;

typedef struct Gin_TraceInfo {
	struct GpioPort *port;
	int index;
} Gin_TraceInfo;

typedef struct GpioPort {
	struct IMX21Gpio *gpio;	/* parent */
	int index;
	uint32_t ddir;
	uint32_t ocr1;
	uint32_t ocr2;
	uint32_t iconfa1;
	uint32_t iconfa2;
	uint32_t iconfb1;
	uint32_t iconfb2;
	uint32_t dr;
	uint32_t gius;
	uint32_t ssr;
	uint32_t icr1;
	uint32_t icr2;
	uint32_t imr;
	uint32_t isr;
	uint32_t gpr;
	uint32_t swr;
	uint32_t puen;

	/* The CPU pin  is the same as G_IN */
	SigNode *cpuPinNode[32];
	Gin_TraceInfo traceInfo[32];
	SigTrace *ginTrace[32];
	SigNode *gpioNode[32];
	SigNode *ainNode[32];
	SigNode *binNode[32];
	SigNode *cinNode[32];
	SigNode *aoutNode[32];
	SigNode *boutNode[32];
} GpioPort;

typedef struct IMX21Gpio {
	BusDevice bdev;
	GpioPort port[NUM_PORTS];
	SigNode *irqNode;
	uint32_t pmask;
} IMX21Gpio;

typedef struct PinInfo {
	int port;		/* GPIO-A=0 GPIO-F=5  */
	int bit;		/* 0-31 */
	char *gpio_name;
	char *pin_name;		/* Pin name of BGA */
	char *pf_name;		/* Primary function */
	char *af_name;		/* Alternate function */
} PinInfo;

PinInfo pinTable[] = {
	{0, 0, "gpioA.0", NULL},
	{0, 1, "gpioA.1", NULL},
	{0, 2, "gpioA.2", NULL},
	{0, 3, "gpioA.3", NULL},
	{0, 4, "gpioA.4", NULL},
	{0, 5, "gpioA.5", "imx21.f4"},
	{0, 6, "gpioA.6", "imx21.d2"},
	{0, 7, "gpioA.7", "imx21.c1"},
	{0, 8, "gpioA.8", "imx21.d1"},
	{0, 9, "gpioA.9", "imx21.c2"},
	{0, 10, "gpioA.10", "imx21.e2"},
	{0, 11, "gpioA.11", "imx21.b2"},
	{0, 12, "gpioA.12", "imx21.c3"},
	{0, 13, "gpioA.13", "imx21.b1"},
	{0, 14, "gpioA.14", "imx21.e1"},
	{0, 15, "gpioA.15", "imx21.a1"},
	{0, 16, "gpioA.16", "imx21.c4"},
	{0, 17, "gpioA.17", "imx21.b3"},
	{0, 18, "gpioA.18", "imx21.a2"},
	{0, 19, "gpioA.19", "imx21.d3"},
	{0, 20, "gpioA.20", "imx21.a3"},
	{0, 21, "gpioA.21", "imx21.e3"},
	{0, 22, "gpioA.22", "imx21.b4"},
	{0, 23, "gpioA.23", "imx21.c5"},
	{0, 24, "gpioA.24", "imx21.a4"},
	{0, 25, "gpioA.25", "imx21.d4"},
	{0, 26, "gpioA.26", "imx21.b5"},
	{0, 27, "gpioA.27", "imx21.e4"},
	{0, 28, "gpioA.28", "imx21.a5"},
	{0, 29, "gpioA.29", "imx21.c6"},
	{0, 30, "gpioA.30", "imx21.b6"},
	{0, 31, "gpioA.31", "imx21.a6"},
	{1, 0, "gpioB.0", NULL},
	{1, 1, "gpioB.1", NULL},
	{1, 2, "gpioB.2", NULL},
	{1, 3, "gpioB.3", NULL},
	{1, 4, "gpioB.4", "imx21.b7"},
	{1, 5, "gpioB.5", "imx21.d7"},
	{1, 6, "gpioB.6", "imx21.a7"},
	{1, 7, "gpioB.7", "imx21.c7"},
	{1, 8, "gpioB.8", "imx21.b8"},
	{1, 9, "gpioB.9", "imx21.d8"},
	{1, 10, "gpioB.10", "imx21.a8"},
	{1, 11, "gpioB.11", "imx21.c8"},
	{1, 12, "gpioB.12", "imx21.d9"},
	{1, 13, "gpioB.13", "imx21.g9"},
	{1, 14, "gpioB.14", "imx21.b9"},
	{1, 15, "gpioB.15", "imx21.c9"},
	{1, 16, "gpioB.16", "imx21.a9"},
	{1, 17, "gpioB.17", "imx21.h9"},
	{1, 18, "gpioB.18", "imx21.b10"},
	{1, 19, "gpioB.19", "imx21.d10"},
	{1, 20, "gpioB.20", "imx21.a10"},
	{1, 21, "gpioB.21", "imx21.c10"},
	{1, 22, "gpioB.22", "imx21.g10"},
	{1, 23, "gpioB.23", "imx21.b11"},
	{1, 24, "gpioB.24", "imx21.c11"},
	{1, 25, "gpioB.25", "imx21.g11"},
	{1, 26, "gpioB.26", "imx21.a11"},
	{1, 27, "gpioB.27", "imx21.a12"},
	{1, 28, "gpioB.28", "imx21.d11"},
	{1, 29, "gpioB.29", "imx21.h12"},
	{1, 30, "gpioB.30", "imx21.c12"},
	{1, 31, "gpioB.31", "imx21.d12"},
	{2, 0, "gpioC.0", NULL},
	{2, 1, "gpioC.1", NULL},
	{2, 2, "gpioC.2", NULL},
	{2, 3, "gpioC.3", NULL},
	{2, 4, "gpioC.4", NULL},
	{2, 5, "gpioC.5", "imx21.g12"},
	{2, 6, "gpioC.6", "imx21.b12"},
	{2, 7, "gpioC.7", "imx21.d13"},
	{2, 8, "gpioC.8", "imx21.a13"},
	{2, 9, "gpioC.9", "imx21.h13"},
	{2, 10, "gpioC.10", "imx21.b13"},
	{2, 11, "gpioC.11", "imx21.g13"},
	{2, 12, "gpioC.12", "imx21.c13"},
	{2, 13, "gpioC.13", "imx21.d14"},
	{2, 14, "gpioC.14", "imx21.a14"},
	{2, 15, "gpioC.15", "imx21.c14"},
	{2, 16, "gpioC.16", "imx21.b14"},
	{2, 17, "gpioC.17", "imx21.d15"},
	{2, 18, "gpioC.18", "imx21.a15"},
	{2, 19, "gpioC.19", "imx21.e16"},
	{2, 20, "gpioC.20", "imx21.b15"},
	{2, 21, "gpioC.21", "imx21.d16"},
	{2, 22, "gpioC.22", "imx21.c15"},
	{2, 23, "gpioC.23", "imx21.a16"},
	{2, 24, "gpioC.24", "imx21.b16"},
	{2, 25, "gpioC.25", "imx21.a17"},
	{2, 26, "gpioC.26", "imx21.a18"},
	{2, 27, "gpioC.27", "imx21.d17"},
	{2, 28, "gpioC.28", "imx21.a19"},
	{2, 29, "gpioC.29", "imx21.c16"},
	{2, 30, "gpioC.30", "imx21.b17"},
	{2, 31, "gpioC.31", "imx21.c17"},

	{3, 0, "gpioD.0", NULL},
	{3, 1, "gpioD.1", NULL},
	{3, 2, "gpioD.2", NULL},
	{3, 3, "gpioD.3", NULL},
	{3, 4, "gpioD.4", NULL},
	{3, 5, "gpioD.5", NULL},
	{3, 6, "gpioD.6", NULL},
	{3, 7, "gpioD.7", NULL},
	{3, 8, "gpioD.8", NULL},
	{3, 9, "gpioD.9", NULL},
	{3, 10, "gpioD.10", NULL},
	{3, 11, "gpioD.11", NULL},
	{3, 12, "gpioD.12", NULL},
	{3, 13, "gpioD.13", NULL},
	{3, 14, "gpioD.14", NULL},
	{3, 15, "gpioD.15", NULL},
	{3, 16, "gpioD.16", NULL},
	{3, 17, "gpioD.17", "imx21.b18"},
	{3, 18, "gpioD.18", "imx21.c18"},
	{3, 19, "gpioD.19", "imx21.b19"},
	{3, 20, "gpioD.20", "imx21.c19"},
	{3, 21, "gpioD.21", "imx21.d18"},
	{3, 22, "gpioD.22", "imx21.d19"},
	{3, 23, "gpioD.23", "imx21.e17"},
	{3, 24, "gpioD.24", "imx21.e19"},
	{3, 25, "gpioD.25", "imx21.h11"},
	{3, 26, "gpioD.26", "imx21.e18"},
	{3, 27, "gpioD.27", "imx21.f16"},
	{3, 28, "gpioD.28", "imx21.f19"},
	{3, 29, "gpioD.29", "imx21.h10"},
	{3, 30, "gpioD.30", "imx21.f17"},
	{3, 31, "gpioD.31", "imx21.j12"},

	{4, 0, "gpioE.0", "imx21.h17"},
	{4, 1, "gpioE.1", "imx21.j19"},
	{4, 2, "gpioE.2", "imx21.j13"},
	{4, 3, "gpioE.3", "imx21.g18"},
	{4, 4, "gpioE.4", "imx21.j16"},
	{4, 5, "gpioE.5", "imx21.h19"},
	{4, 6, "gpioE.6", "imx21.l16"},
	{4, 7, "gpioE.7", "imx21.m16"},
	{4, 8, "gpioE.8", "imx21.l19"},
	{4, 9, "gpioE.9", "imx21.m17"},
	{4, 10, "gpioE.10", "imx21.l18"},
	{4, 11, "gpioE.11", "imx21.l17"},
	{4, 12, "gpioE.12", "imx21.l13"},
	{4, 13, "gpioE.13", "imx21.k10"},
	{4, 14, "gpioE.14", "imx21.m19"},
	{4, 15, "gpioE.15", "imx21.m18"},
	{4, 16, "gpioE.16", "imx21.n19"},
	{4, 17, "gpioE.17", "imx21.v15"},
	{4, 18, "gpioE.18", "imx21.n16"},
	{4, 19, "gpioE.19", "imx21.n18"},
	{4, 20, "gpioE.20", "imx21.p16"},
	{4, 21, "gpioE.21", "imx21.t17"},
	{4, 22, "gpioE.22", "imx21.p17"},
	{4, 23, "gpioE.23", "imx21.r16"},
	{4, 24, "gpioE.24", NULL},
	{4, 25, "gpioE.25", NULL},
	{4, 26, "gpioE.26", NULL},
	{4, 27, "gpioE.27", NULL},
	{4, 28, "gpioE.28", NULL},
	{4, 29, "gpioE.29", NULL},
	{4, 30, "gpioE.30", NULL},
	{4, 31, "gpioE.31", NULL},

	{5, 0, "gpioF.0", "imx21.m12"},
	{5, 1, "gpioF.1", "imx21.t15"},
	{5, 2, "gpioF.2", "imx21.l12"},
	{5, 3, "gpioF.3", "imx21.u15"},
	{5, 4, "gpioF.4", "imx21.u14"},
	{5, 5, "gpioF.5", "imx21.w13"},
	{5, 6, "gpioF.6", "imx21.t13"},
	{5, 7, "gpioF.7", "imx21.w12"},
	{5, 8, "gpioF.8", "imx21.u13"},
	{5, 9, "gpioF.9", "imx21.l11"},
	{5, 10, "gpioF.10", "imx21.t12"},
	{5, 11, "gpioF.11", "imx21.u12"},
	{5, 12, "gpioF.12", "imx21.t11"},
	{5, 13, "gpioF.13", "imx21.v13"},
	{5, 14, "gpioF.14", "imx21.m11"},
	{5, 15, "gpioF.15", "imx21.v12"},
	{5, 16, "gpioF.16", "imx21.u11"},
	{5, 17, "gpioF.17", NULL},
	{5, 18, "gpioF.18", NULL},
	{5, 19, "gpioF.19", NULL},
	{5, 20, "gpioF.20", NULL},
	{5, 21, "gpioF.21", "imx21.u5"},
	{5, 22, "gpioF.22", "imx21.v5"},
	{5, 23, "gpioF.23", NULL},
	{5, 24, "gpioF.24", NULL},
	{5, 25, "gpioF.25", NULL},
	{5, 26, "gpioF.26", NULL},
	{5, 27, "gpioF.27", NULL},
	{5, 28, "gpioF.28", NULL},
	{5, 29, "gpioF.29", NULL},
	{5, 30, "gpioF.30", NULL},
	{5, 31, "gpioF.31", NULL},
	{-1, -1, NULL, NULL}
};

#include <arm9cpu.h>
static inline void
check_interrupts()
{
#if 1
	if (!(REG_CPSR & FLAG_I)) {
		static int count = 0;
		if (count < 10) {
			count++;
			fprintf(stderr,
				"Emulator: Warning, interrupts not disabled: possible nonatomic gpio write\n");
		}
	}
#endif
}

/*
 * ------------------------------------------------------
 * return the node value (SIG_LOW or SIG_HIGH)
 * ------------------------------------------------------
 */
static int
get_gout(GpioPort * port, int index)
{
	int oc;
	int result = 0;
	uint64_t ocr = ((uint64_t) port->ocr2 << 32) | (port->ocr1);
	oc = (ocr >> (index << 1)) & 3;
	switch (oc) {
	    case 0:
		    result = (SigNode_Val(port->ainNode[index]) == SIG_HIGH);
		    break;
	    case 1:
		    result = (SigNode_Val(port->binNode[index]) == SIG_HIGH);
		    break;
	    case 2:
		    result = (SigNode_Val(port->cinNode[index]) == SIG_HIGH);
		    break;
	    case 3:
		    result = (port->dr >> index) & 1;
		    break;
	}
	if (result) {
		return SIG_HIGH;
	} else {
		return SIG_LOW;
	}
}

static void
update_gpioouts(GpioPort * port)
{
	int i;
	for (i = 0; i < 32; i++) {
		if (port->ddir & (1 << i)) {
			// output
			SigNode_Set(port->gpioNode[i], get_gout(port, i));
		} else {
			// input
			if (port->puen & (1 << i)) {
				SigNode_Set(port->gpioNode[i], SIG_PULLUP);
			} else {
				SigNode_Set(port->gpioNode[i], SIG_OPEN);
			}
		}
	}
}

static void
update_aouts(GpioPort * port)
{
	uint64_t iconfa = ((uint64_t) port->iconfa2 << 32) | port->iconfa1;
	int ic;
	int i;
	int value = SIG_LOW;
	for (i = 0; i < 32; i++) {
		ic = (iconfa >> (i << 1)) & 3;
		switch (ic) {
		    case 0:	/* gpio in */
			    value = SigNode_Val(port->gpioNode[i]);
			    break;
		    case 1:	/* isr */
			    if (port->isr & (1 << i)) {
				    value = SIG_HIGH;
			    } else {
				    value = SIG_LOW;
			    }
			    break;
		    case 2:	/* 0 */
			    value = SIG_LOW;
			    break;
		    case 3:	/* 1 */
			    value = SIG_HIGH;
			    break;
		}
		if ((value == SIG_HIGH || value == SIG_PULLUP)) {
			SigNode_Set(port->aoutNode[i], SIG_HIGH);
		} else {
			SigNode_Set(port->aoutNode[i], SIG_LOW);
		}
	}
}

static void
update_bouts(GpioPort * port)
{
	uint64_t iconfb = ((uint64_t) port->iconfb2 << 32) | port->iconfb1;
	int ic;
	int i;
	int value = SIG_LOW;
	for (i = 0; i < 32; i++) {
		ic = (iconfb >> (i << 1)) & 3;
		switch (ic) {
		    case 0:	/* gpio in */
			    value = SigNode_Val(port->gpioNode[i]);
			    break;
		    case 1:	/* isr */
			    if (port->isr & (1 << i)) {
				    value = SIG_HIGH;
			    } else {
				    value = SIG_LOW;
			    }
			    break;
		    case 2:	/* 0 */
			    value = SIG_LOW;
			    break;
		    case 3:	/* 1 */
			    value = SIG_HIGH;
			    break;
		}
		if ((value == SIG_HIGH || value == SIG_PULLUP)) {
			SigNode_Set(port->boutNode[i], SIG_HIGH);
		} else {
			SigNode_Set(port->boutNode[i], SIG_LOW);
		}
	}
}

static void
update_interrupt(IMX21Gpio * gpio)
{
	int interrupt = 0;
	int i;
	for (i = 0; i < NUM_PORTS; i++) {
		GpioPort *port = &gpio->port[i];
		if (!(gpio->pmask & (1 << i))) {
			continue;
		}
		if (port->isr & port->imr) {
			interrupt = 1;
			//fprintf(stderr,"%d port isr %08x imr %08x \n",i,port->isr,port->imr);
		}
	}
	if (interrupt) {
		/* post an interrupt */
		//fprintf(stderr,"GPIO: post irq\n");
		SigNode_Set(gpio->irqNode, SIG_LOW);

	} else {
		/* unpost an interrupt */
		//fprintf(stderr,"GPIO: unpost irq\n");
		SigNode_Set(gpio->irqNode, SIG_HIGH);
	}
}

/*
 * ---------------------------------------------------------------------------------------------
 * Update Interrupt Control register. This function is called whenever interrupt control
 * register 1 or 2 is changed. The interrupt control register contains the interrupt
 * condition: rising edge, falling edge, level 1 or level 0 
 * For interrupts the raw signal on the pins is used no matter if the pin
 * is in gpio mode or in any other mode
 * ---------------------------------------------------------------------------------------------
 */
static void
update_icr(GpioPort * port)
{
	uint64_t icr = ((uint64_t) port->icr2 << 32) | port->icr1;
	int value;
	int i;
	int ic;
	for (i = 0; i < 32; i++) {
		if (!port->cpuPinNode[i]) {
			continue;
		}
		ic = (icr >> (i << 1)) & 3;
		int interrupt = -1;
		switch (ic) {
		    case 0:	/* no rising edge on icr change */
		    case 1:	/* no falling edge on icr change */
			    break;
		    case 2:	/* high level */
			    value = SigNode_Val(port->cpuPinNode[i]);
			    if ((value == SIG_HIGH) || (value == SIG_PULLUP)) {
				    interrupt = 1;
			    }
			    break;
		    case 3:	/* low level */
			    value = SigNode_Val(port->cpuPinNode[i]);
			    if ((value == SIG_LOW) || (value == SIG_PULLDOWN)) {
				    interrupt = 1;
			    }
			    break;
		}
		if (interrupt == 1) {
			port->isr |= (1 << i);
		}
	}
	update_interrupt(port->gpio);
}

static void
reset_port(GpioPort * port)
{
	port->ddir = 0;
	port->ocr1 = port->ocr2 = 0;
	port->iconfa1 = port->iconfa2 = 0xffffffff;
	port->iconfb1 = port->iconfb2 = 0xffffffff;
	port->dr = 0;
	port->gius = 0;		/* Should be fetched from configfile */
	port->ssr = 0;
	port->icr1 = port->icr2 = 0;
	port->imr = 0;
	port->isr = 0;
	port->gpr = 0;
	port->swr = 0;
	port->puen = 0xffffffff;
	update_gpioouts(port);
	update_aouts(port);
	update_bouts(port);
	update_interrupt(port->gpio);
}

/*
 * -------------------------------------------------------------------
 * DDIR 
 *	Switch the direction of the GPIO pins.
 *	(Has no effect when not in gpio mode)
 * -------------------------------------------------------------------
 */
static void
ddir_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	GpioPort *port = (GpioPort *) clientData;
	port->ddir = value;
	update_gpioouts(port);
	return;
}

static uint32_t
ddir_read(void *clientData, uint32_t address, int rqlen)
{
	GpioPort *port = (GpioPort *) clientData;
	return port->ddir;
}

/*
 * -------------------------------------------------------------------
 * OCR1 / OCR2
 * 	select the source for GPIO in output mode (input A,B,C,DR) 
 * -------------------------------------------------------------------
 */
static void
ocr1_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	GpioPort *port = (GpioPort *) clientData;
	port->ocr1 = value;
	check_interrupts();
	update_gpioouts(port);
	return;
}

static uint32_t
ocr1_read(void *clientData, uint32_t address, int rqlen)
{
	GpioPort *port = (GpioPort *) clientData;
	return port->ocr1;
}

static void
ocr2_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	GpioPort *port = (GpioPort *) clientData;
	port->ocr2 = value;
	check_interrupts();
	update_gpioouts(port);
	return;
}

static uint32_t
ocr2_read(void *clientData, uint32_t address, int rqlen)
{
	GpioPort *port = (GpioPort *) clientData;
	return port->ocr2;
}

/*
 * -------------------------------------------------------
 * ICONFA1/ICONFA2
 * 	Select the source for A_OUT[0..31] (GPIO,ISR,0,1)
 * -------------------------------------------------------
 */
static void
iconfa1_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	GpioPort *port = (GpioPort *) clientData;
	port->iconfa1 = value;
	check_interrupts();
	update_aouts(port);
	return;
}

static uint32_t
iconfa1_read(void *clientData, uint32_t address, int rqlen)
{
	GpioPort *port = (GpioPort *) clientData;
	return port->iconfa1;
}

static void
iconfa2_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	GpioPort *port = (GpioPort *) clientData;
	port->iconfa2 = value;
	check_interrupts();
	update_aouts(port);
	return;
}

static uint32_t
iconfa2_read(void *clientData, uint32_t address, int rqlen)
{
	GpioPort *port = (GpioPort *) clientData;
	return port->iconfa2;
}

/*
 * ---------------------------------------------------------------------
 * ICONFB1/ICONFB2
 * 	Select the source for B_OUT[0..31] (GPIO,ISR,0,1)
 * ---------------------------------------------------------------------
 */
static void
iconfb1_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	GpioPort *port = (GpioPort *) clientData;
	port->iconfb1 = value;
	check_interrupts();
	update_bouts(port);
	return;
}

static uint32_t
iconfb1_read(void *clientData, uint32_t address, int rqlen)
{
	GpioPort *port = (GpioPort *) clientData;
	return port->iconfb1;
}

static void
iconfb2_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	GpioPort *port = (GpioPort *) clientData;
	port->iconfb2 = value;
	check_interrupts();
	update_bouts(port);
	return;
}

static uint32_t
iconfb2_read(void *clientData, uint32_t address, int rqlen)
{
	GpioPort *port = (GpioPort *) clientData;
	return port->iconfb2;
}

/*
 * ----------------------------------------------------------------------------
 * DR
 * 	Data register. Written to the GPIO pins when muxed to G_OUT
 *	else stored
 * ---------------------------------------------------------------------------
 */
static void
dr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	GpioPort *port = (GpioPort *) clientData;
	port->dr = value;
	check_interrupts();
	update_gpioouts(port);
	return;
}

static uint32_t
dr_read(void *clientData, uint32_t address, int rqlen)
{
	GpioPort *port = (GpioPort *) clientData;
	return port->dr;
}

/*
 * -----------------------------------------------------------------------------------------
 * GIUS
 * 	GPIO in use
 *	When 1 pin is controlled by gpio module, else by primary or alternate function
 * -----------------------------------------------------------------------------------------
 */

static void
gius_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	GpioPort *port = (GpioPort *) clientData;
	uint32_t diff = port->gius ^ value;
	int i;
	port->gius = value;
	check_interrupts();
	for (i = 0; i < 32; i++) {
		if (!(diff & (1 << i))) {
			continue;
		}
		if (!port->cpuPinNode[i]) {
			continue;
		}
		if (value & (1 << i)) {
			SigNode_Link(port->cpuPinNode[i], port->gpioNode[i]);
		} else {
			SigNode_RemoveLink(port->cpuPinNode[i], port->gpioNode[i]);
		}
	}
	update_gpioouts(port);
	return;
}

static uint32_t
gius_read(void *clientData, uint32_t address, int rqlen)
{
	GpioPort *port = (GpioPort *) clientData;
	return port->gius;
}

/*
 * -------------------------------------------------------------------------------
 * SSR 
 *	Sample status register
 *	contains the value of the GPIO pins sampled on every clock tick
 * -------------------------------------------------------------------------------
 */
static void
ssr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "GPIO: Sample status register not writable\n");
	return;
}

static uint32_t
ssr_read(void *clientData, uint32_t address, int rqlen)
{
	GpioPort *port = (GpioPort *) clientData;
	int i;
	uint32_t ssr = 0;
	for (i = 0; i < 32; i++) {
		int val;
		if (port->cpuPinNode[i]) {
			val = SigNode_Val(port->cpuPinNode[i]);
			if (val == SIG_HIGH) {
				ssr |= (1 << i);
			}
		}
	}
	//fprintf(stderr,"SSR%d: %08x\n",port->index,ssr);
	return ssr;
}

/*
 * -------------------------------------------------------------------------------
 * ICR1 / ICR2
 * 	Select interrupt mode (rising,falling,high,low)
 * -------------------------------------------------------------------------------
 */
static void
icr1_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	GpioPort *port = (GpioPort *) clientData;
	port->icr1 = value;
	check_interrupts();
	update_icr(port);
	return;
}

static uint32_t
icr1_read(void *clientData, uint32_t address, int rqlen)
{
	GpioPort *port = (GpioPort *) clientData;
	return port->icr1;
}

static void
icr2_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	GpioPort *port = (GpioPort *) clientData;
	port->icr2 = value;
	check_interrupts();
	update_icr(port);
	return;
}

static uint32_t
icr2_read(void *clientData, uint32_t address, int rqlen)
{
	GpioPort *port = (GpioPort *) clientData;
	return port->icr2;
}

/*
 * -------------------------------------------------------------
 * IMR
 * 	Interrupt mask register : 0 masked 1 unmasked
 * -------------------------------------------------------------
 */
static void
imr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	GpioPort *port = (GpioPort *) clientData;
	uint32_t diff = port->imr ^ value;
	port->imr = value;
	if (diff) {
		update_interrupt(port->gpio);
	}
	return;
}

static uint32_t
imr_read(void *clientData, uint32_t address, int rqlen)
{
	GpioPort *port = (GpioPort *) clientData;
	return port->imr;
}

/*
 * -------------------------------------------------------------
 * ISR
 * 	Interrupt status register 
 *	indicates when interrupt condition in ICR was satisfied
 *	w1c
 * -------------------------------------------------------------
 */
static void
isr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	GpioPort *port = (GpioPort *) clientData;
	uint32_t mask = value & port->isr;
	port->isr = port->isr ^ mask;
	update_interrupt(port->gpio);
	return;
}

static uint32_t
isr_read(void *clientData, uint32_t address, int rqlen)
{
	GpioPort *port = (GpioPort *) clientData;
	return port->isr;
}

/*
 * ---------------------------------------------------------------------
 * GPR
 * 	Select between primary and alternate function when not in
 *	GPIO mode
 * ---------------------------------------------------------------------
 */
static void
gpr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	GpioPort *port = (GpioPort *) clientData;
	port->gpr = value;
	/* Here a signal ID should be given to the pin */
	return;
}

static uint32_t
gpr_read(void *clientData, uint32_t address, int rqlen)
{
	GpioPort *port = (GpioPort *) clientData;
	return port->gpr;
}

/*
 * -------------------------------------------------------------------------
 * SWR register
 *	Software reset register: reset port when bit 0 is set. selfclearing
 * -------------------------------------------------------------------------
 */

static void
swr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	GpioPort *port = (GpioPort *) clientData;
	if (value & SWR_RESET) {
		reset_port(port);
	}
	return;
}

static uint32_t
swr_read(void *clientData, uint32_t address, int rqlen)
{
	return 0;
}

/*
 * --------------------------------------------------------------------------
 * PUEN
 *	Enables Pullup resistor
 * --------------------------------------------------------------------------
 */
static void
puen_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	GpioPort *port = (GpioPort *) clientData;
	port->puen = value;
	update_gpioouts(port);
	//fprintf(stderr,"PUEN%c %08x\n",'a'+port->index,value);
	return;
}

static uint32_t
puen_read(void *clientData, uint32_t address, int rqlen)
{
	GpioPort *port = (GpioPort *) clientData;
	return port->puen;
}

/*
 * ----------------------------------------------------------------------------
 * PMASK 
 * 	Port Interrupt mask register
 * 	0=interrupt masked 1=not masked
 *	Bit  0: PTA
 *      ...........
 *	Bit  5: PTF
 * ----------------------------------------------------------------------------
 */
static void
pmask_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	IMX21Gpio *gpio = (IMX21Gpio *) clientData;
	gpio->pmask = value;
	update_interrupt(gpio);
	return;
}

static uint32_t
pmask_read(void *clientData, uint32_t address, int rqlen)
{
	IMX21Gpio *gpio = (IMX21Gpio *) clientData;
	return gpio->pmask;
}

static void
IMX21Gpio_Map(void *owner, uint32_t base, uint32_t mask, uint32_t flags)
{
	IMX21Gpio *gpio = owner;
	GpioPort *port;
	int i;
	for (i = 0; i < NUM_PORTS; i++) {
		port = &gpio->port[i];
		IOH_New32(GPIO_DDIR(base, i), ddir_read, ddir_write, port);
		IOH_New32(GPIO_OCR1(base, i), ocr1_read, ocr1_write, port);
		IOH_New32(GPIO_OCR2(base, i), ocr2_read, ocr2_write, port);
		IOH_New32(GPIO_ICONFA1(base, i), iconfa1_read, iconfa1_write, port);
		IOH_New32(GPIO_ICONFA2(base, i), iconfa2_read, iconfa2_write, port);
		IOH_New32(GPIO_ICONFB1(base, i), iconfb1_read, iconfb1_write, port);
		IOH_New32(GPIO_ICONFB2(base, i), iconfb2_read, iconfb2_write, port);
		IOH_New32(GPIO_DR(base, i), dr_read, dr_write, port);
		IOH_New32(GPIO_GIUS(base, i), gius_read, gius_write, port);
		IOH_New32(GPIO_SSR(base, i), ssr_read, ssr_write, port);
		IOH_New32(GPIO_ICR1(base, i), icr1_read, icr1_write, port);
		IOH_New32(GPIO_ICR2(base, i), icr2_read, icr2_write, port);
		IOH_New32(GPIO_IMR(base, i), imr_read, imr_write, port);
		IOH_New32(GPIO_ISR(base, i), isr_read, isr_write, port);
		IOH_New32(GPIO_GPR(base, i), gpr_read, gpr_write, port);
		IOH_New32(GPIO_SWR(base, i), swr_read, swr_write, port);
		IOH_New32(GPIO_PUEN(base, i), puen_read, puen_write, port);
	}
	IOH_New32(GPIO_PMASK(base), pmask_read, pmask_write, gpio);
}

static void
IMX21Gpio_Unmap(void *owner, uint32_t base, uint32_t mask)
{
	int i;
	for (i = 0; i < NUM_PORTS; i++) {
		IOH_Delete32(GPIO_DDIR(base, i));
		IOH_Delete32(GPIO_OCR1(base, i));
		IOH_Delete32(GPIO_OCR2(base, i));
		IOH_Delete32(GPIO_ICONFA1(base, i));
		IOH_Delete32(GPIO_ICONFA2(base, i));
		IOH_Delete32(GPIO_ICONFB1(base, i));
		IOH_Delete32(GPIO_ICONFB2(base, i));
		IOH_Delete32(GPIO_DR(base, i));
		IOH_Delete32(GPIO_GIUS(base, i));
		IOH_Delete32(GPIO_SSR(base, i));
		IOH_Delete32(GPIO_ICR1(base, i));
		IOH_Delete32(GPIO_ICR2(base, i));
		IOH_Delete32(GPIO_IMR(base, i));
		IOH_Delete32(GPIO_ISR(base, i));
		IOH_Delete32(GPIO_GPR(base, i));
		IOH_Delete32(GPIO_SWR(base, i));
		IOH_Delete32(GPIO_PUEN(base, i));
	}
	IOH_Delete32(GPIO_PMASK(base));
}

static void
gin_trace_proc(struct SigNode *node, int value, void *clientData)
{
	Gin_TraceInfo *ti = clientData;
	GpioPort *port = ti->port;
	int index = ti->index;
	uint64_t icr = ((uint64_t) port->icr2 << 32) | port->icr1;
	int ic = (icr >> (index << 1)) & 3;
	switch (ic) {
	    case 0:		/* rising */
	    case 2:		/* high */
		    if ((value == SIG_HIGH) || (value == SIG_PULLUP)) {
			    port->isr |= (1 << index);
			    update_interrupt(port->gpio);
		    }
		    break;
	    case 1:		/* falling */
	    case 3:		/* low */
		    if ((value == SIG_LOW) || (value == SIG_PULLDOWN)) {
			    port->isr |= (1 << index);
			    update_interrupt(port->gpio);
		    }
		    break;
	}
}

static PinInfo *
find_pininfo(int portindex, int bit)
{
	int i;
	static int index = 0;
	for (i = 0; i < 32 * NUM_PORTS; i++) {
		if (pinTable[index].bit == -1) {
			index = 0;
		}
		if ((pinTable[index].bit == bit) && (pinTable[index].port == portindex)) {
			return &pinTable[index];
		}
		index++;
	}
	fprintf(stderr, "Gpio port %d bit %d  not found\n", portindex, bit);
	return NULL;
}

/*
 * --------------------------------------------------------------------------------
 * Create one GPIO port and the corresponding processor pin
 * --------------------------------------------------------------------------------
 */
static void
create_port(GpioPort * port, int index, const char *modulename)
{
	int i;
	char *portname = alloca(strlen(modulename) + 20);
	sprintf(portname, "%s%c", modulename, 'A' + index);
	for (i = 0; i < 32; i++) {
		Gin_TraceInfo *ti = &port->traceInfo[i];
		PinInfo *pin_info = find_pininfo(index, i);
		if (!pin_info) {
			fprintf(stderr, "GPIO pin %d of port %d is missing in table\n", i, index);
			exit(1);
		}
		ti->port = port;
		ti->index = i;
		if (pin_info->pin_name) {
			port->cpuPinNode[i] = SigNode_New("%s", pin_info->pin_name);
			port->ginTrace[i] = SigNode_Trace(port->cpuPinNode[i], gin_trace_proc, ti);
		}
		port->gpioNode[i] = SigNode_New("%s.%d", portname, i);
		port->ainNode[i] = SigNode_New("%s.ain%d", portname, i);
		port->binNode[i] = SigNode_New("%s.bin%d", portname, i);
		port->cinNode[i] = SigNode_New("%s.cin%d", portname, i);
		port->aoutNode[i] = SigNode_New("%s.aout%d", portname, i);
		port->boutNode[i] = SigNode_New("%s.bout%d", portname, i);
	}
}

__UNUSED__ static int
pin_trace(struct SigNode *node, int value, void *clientData)
{
	fprintf(stderr, "Pin changed to %d\n", value);
	return 0;
}

BusDevice *
IMX21_GpioNew(const char *modulename)
{
	IMX21Gpio *gpio;
	int i;
	gpio = sg_new(IMX21Gpio);
	gpio->irqNode = SigNode_New("%s.irq", modulename);	// irq output
	if (!gpio->irqNode) {
		fprintf(stderr, "Can not create irq node\n");
	}
	/* Initialize parent pointers */
	for (i = 0; i < NUM_PORTS; i++) {
		GpioPort *port = &gpio->port[i];
		port->gpio = gpio;
		port->index = i;
		create_port(port, i, modulename);
	}
	for (i = 0; i < NUM_PORTS; i++) {
		GpioPort *port = &gpio->port[i];
		reset_port(port);
	}
	gpio->pmask = 0x3f;	/* all interrupts enabled by default */
	gpio->bdev.first_mapping = NULL;
	gpio->bdev.Map = IMX21Gpio_Map;
	gpio->bdev.UnMap = IMX21Gpio_Unmap;
	gpio->bdev.owner = gpio;
	gpio->bdev.hw_flags = MEM_FLAG_WRITABLE | MEM_FLAG_READABLE;
	update_interrupt(gpio);
//      SigNode_Trace(gpio->port[2].cpuPinNode[11],pin_trace,NULL);
	fprintf(stderr, "IMX21 GPIO module \"%s\" created\n", modulename);
	return &gpio->bdev;
}
