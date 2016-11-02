/*
 *******************************************************************************************************
 *
 * Emulation of the C161 Interrupt controller 
 *
 * State:
 *	Nothing works
 *
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signode.h>
#include "bus.h"
#include "sgstring.h"

typedef struct C161_Intco C161_Intco;

typedef struct TrapInfo {
	int nr;
	char *name;
	uint16_t icr_addr;
} TrapInfo;

typedef struct C161_Trap {
	TrapInfo *trapInfo;
	C161_Intco *intco;
	uint32_t vector_addr;

	SigNode *trapNode;
	SigTrace *traceProc;
	uint16_t icr;	/* The Interrupt control register */
	int pinstate;

} C161_Trap;

struct 
C161_Intco {
	C161_Trap trap[128];
	int ipl; // should this better be in cpu structure ?
};

TrapInfo c161_traps[] = {
	{
		nr: 0x22,
		name: "irq0",
		icr_addr: 0xff60
	},
	{
		nr: 0x23,
		name: "irq1",
		icr_addr: 0xff62
	},
	{
		nr: 0x24,
		name: "irq2",
		icr_addr: 0xff64
	},
	{
		nr: 0x25,
		name: "irq3",
		icr_addr: 0xff66
	},
	{
		nr: 0x26,
		name: "irq4",
		icr_addr: 0xff68
	},
	{
		nr: 0x27,
		name: "irq5",
		icr_addr: 0xff6a
	},
	{
		nr: 0x2a,
		name: "irq6",
		icr_addr: 0xff6c
	},
	{
		nr: 0x2b,
		name: "irq7",
		icr_addr: 0xff6e
	},
	{
		nr: 0x2c,
		name: "irq8",
		icr_addr: 0xff70
	},
	{
		nr: 0x47,
		name: "irq9",
		icr_addr: 0xf19c
	},
	{
		nr: 0x2d,
		name: "irq10",
		icr_addr: 0xff72
	},
	{
		nr: 0x2e,
		name: "irq11",
		icr_addr: 0xff74	
	},
	{
		nr: 0x2f,
		name: "irq12",
		icr_addr: 0xff76
	},
	{
		nr: 0x46,
		name: "irq13",
		icr_addr: 0xf194
	},
	{
		nr: 0x45,
		name: "irq14",
		icr_addr: 0xf18c
	},
	{
		nr: 0x44,
		name: "irq15",
		icr_addr: 0xf184
	},
	{
		nr: 0x3c,
		name: "irq16",
		icr_addr: 0xf178
	},
	{
		nr: 0x3b,
		name: "irq17",
		icr_addr: 0xf176
	},
	{
		nr: 0x3a,
		name: "irq18",
		icr_addr: 0xf174
	},
	{
		nr: 0x39,
		name: "irq19",
		icr_addr: 0xf172
	},
	{
		nr: 0x38,
		name: "irq20",
		icr_addr: 0xf170
	},
	{
		nr: 0x37,
		name: "irq21",
		icr_addr: 0xf16e
	},
	{
		nr: 0x36,
		name: "irq22",
		icr_addr: 0xf16c,
	},
	{
		nr: 0x35,
		name: "irq23",
		icr_addr: 0xf16a
	},
	{
		nr: 0x34,
		name: "irq24",
		icr_addr: 0xf168
	},
	{
		nr: 0x33,
		name: "irq25",
		icr_addr: 0xf166
	},
	{
		nr: 0x32,
		name: "irq26",
		icr_addr: 0xf164	
	},
	{
		nr: 0x31,
		name: "irq27",
		icr_addr: 0xf162
	},
	{
		nr: 0x30,
		name: "irq28",
		icr_addr:  0xf160
	},
	{
		nr: 0x17,
		name: "irq29",
		icr_addr: 0xff86
	},
	{
		nr: 0x16,
		name: "irq30",
		icr_addr: 0xff84,
	},
	{
		nr: 0x15,
		name: "irq31",
		icr_addr: 0xff82
	},
	{
		nr: 0x14,
		name: "irq32",
		icr_addr: 0xff80,
	},
	{
		nr: 0x13,
		name: "irq33",
		icr_addr: 0xff7e,
	}, 
	{
		nr: 0x12,
		name: "irq34",
		icr_addr: 0xff7c,
	},
	{
		nr: 0x11,
		name: "irq35",
		icr_addr: 0xff7a,
	},
	{
		nr: 0x10,
		name: "irq36",
		icr_addr: 0xff78,
	},
	{
		nr: 0x20,
		name: "irq37",
		icr_addr: 0xff9c,
	},	
	{
		nr: 0x21,
		name: "irq38",
		icr_addr: 0xff9e,
	},
	{
		nr: 0x3d,
		name: "irq39",
		icr_addr: 0xf17a,
	},
	{
		nr: 0x3e,
		name: "irq40",
		icr_addr: 0xf17c,
	},
	{
		nr: 0x28,
		name: "irq41",
		icr_addr: 0xff98,
	},
	{
		nr: 0x18,
		name: "firq0",
		icr_addr: 0xff88,
	},
	{
		nr: 0x19,
		name: "firq1",
		icr_addr: 0xff8a,
	},
	{
		nr: 0x1a,
		name: "firq2",
		icr_addr: 0xff8c,
	},
	{
		nr: 0x1b,
		name: "firq3",
		icr_addr: 0xff8e,
	},
	{
		nr: 0x1c,
		name: "firq4",
		icr_addr: 0xff90,
	},
	{
		nr: 0x1d,
		name: "firq5",
		icr_addr: 0xff92,
	},
	{
		nr: 0x1e,
		name: "firq6",
		icr_addr: 0xff94
	},
	{
		nr: 0x1f,
		name: "firq7",
		icr_addr: 0xff96,
	},
	{
		nr: 0x40,
		name: "xb0",
		icr_addr: 0xf186,
	},
	{
		nr: 0x41,
		name: "xb1",
		icr_addr: 0xf18e
	},
	{
		nr: 0x43,
		name: "xb3",
		icr_addr: 0xf19e
	},
	{
		nr: 0x4c,
		name: "clisn",
		icr_addr: 0xffa8
	}
};


static TrapInfo *
trapinfo_find(int trap_nr) {
	int nr_traps = sizeof(c161_traps)/sizeof(TrapInfo);
	int i;
	for(i=0;i<nr_traps;i++) {
		TrapInfo *ti=&c161_traps[i];
		if(ti->nr == trap_nr) {
			return ti;
		}
	}
	return NULL;
}

static void 
trap_signal_trace(struct SigNode * node,int value, void *clientData)
{
	C161_Trap *trap = (C161_Trap*)clientData;
	TrapInfo *ti = trap->trapInfo;
	if((value == SIG_LOW) && (trap->pinstate == SIG_HIGH)) 
	{
		uint32_t icr_address = ti->icr_addr;
		//uint16_t reg = ((icr_address & 0x1ff)>>1);
		uint16_t icr_val;
		int8_t ilvl;
		int8_t glvl;
		icr_val = Bus_Read16(icr_address);
		ilvl = (icr_val >> 2) 	& 0xf; 
		glvl = icr_val & 0x3;
		// C16x_PostInterrupt(ILVL);
	}
	trap->pinstate=value;
}

void
C161_IntcoNew(char *devname) 
{
	int i;
	C161_Intco *intco = sg_new(C161_Intco);
	C161_Trap *trap;
	for(i=0;i<128;i++) {
		TrapInfo *ti = trapinfo_find(i);
		trap=&intco->trap[i];
		trap->intco = intco;
		trap->trapInfo = ti;
		if(ti) {
			trap->trapNode = SigNode_New("%s.%s",devname,ti->name);
			if(!trap->trapNode) {
				fprintf(stderr,"Can not create node for trap %d\n",i);
				exit(190);
			}
			SigNode_Trace(trap->trapNode,trap_signal_trace,trap);	
		}
		trap->vector_addr = i * 4;
	}	
		
}
