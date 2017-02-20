//===-- arm/arm9cpu.c ---------------------------------------------*- C -*-===//
//
//              The Leigun Embedded System Simulator Platform : modules
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
//===----------------------------------------------------------------------===//
///
/// @file
/// Emulation of the ARM CPU, Initialization and main loop
///
//===----------------------------------------------------------------------===//

// clang-format off
/*
 *************************************************************************************************
 *
 * Emulation of the ARM CPU, Initialization and 
 * main loop
 *
 * Copyright 2004 2009 Jochen Karrer. All rights reserved.
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
// clang-format on

//==============================================================================
//= Dependencies
//==============================================================================
// Main Module Header
#include "arm9cpu.h"

// Local/Private Headers
#include "idecode_arm.h"
#include "instructions_arm.h"
#include "mmu_arm9.h"
#include "thumb_decode.h"

// Leigun Core Headers
#include "bus.h"
#include "configfile.h"
#include "coprocessor.h"
#include "cycletimer.h"
#include "xy_tree.h"
#include "core/device.h"
#include "core/globalclock.h"

// External headers

// System headers
#include <unistd.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <fcntl.h>
#include <string.h>
#include <setjmp.h>
#include <errno.h>
#include <sys/types.h>
#include <time.h>
#include <sys/time.h>


//==============================================================================
//= Constants(also Enumerations)
//==============================================================================
#define VERBOSE 0
#define MPU_NAME "ARM9"
#define MPU_DESCRIPTION "ARM9"
#define MPU_DEFAULTCONFIG \
    "[global]\n" \
    "cpu_clock: 200000000\n" \
    "start_address: 0\n" \
    "dbgwait: 0\n" \
    "\n"


//==============================================================================
//= Types
//==============================================================================


//==============================================================================
//= Variables
//==============================================================================
ARM9 gcpu;

uint32_t debugflags = 0;
uint32_t mmu_vector_base = 0;
uint32_t do_alignment_check = 0;

CycleTimer htimer;


//==============================================================================
//= Function declarations(static)
//==============================================================================
#ifdef DEBUG
#define dbgprintf(...) { if(unlikely(debugflags & DEBUG_INSTRUCTIONS)) { fprintf(stderr,__VA_ARGS__);fflush(stderr); } }
#else
#define dbgprintf(...)
#endif
static int debugger_getreg(void *clientData, uint8_t * data, uint32_t index, int maxlen);
static void debugger_setreg(void *clientData, const uint8_t * data, uint32_t index, int len);
static void debugger_get_bkpt_ins(void *clientData, uint8_t * ins, uint64_t addr, int len);
static int debugger_stop(void *clientData);
static int debugger_cont(void *clientData);
static int debugger_step(void *clientData, uint64_t addr, int use_addr);
static Dbg_TargetStat debugger_get_status(void *clientData);
static ssize_t debugger_getmem(void *clientData, uint8_t * data, uint64_t addr, uint32_t len);
static ssize_t debugger_setmem(void *clientData, const uint8_t * data, uint64_t addr, uint32_t len);
static void ARM9_InitRegs(ARM9 * arm);
static void hello_proc(void *cd);
static void irq_change(SigNode * node, int value, void *clientData);
static void fiq_change(SigNode * node, int value, void *clientData);
static void arm_throttle(void *clientData);
static void ARM_ThrottleInit(ARM9 * arm);
static void dump_stack(void);
static void dump_regs(void);
static void Do_Debug(void);
static inline void CheckSignals(void);
static inline void debug_print_instruction(uint32_t icode);
static void Thumb_Loop(void);
static void ARM9_Loop32(void);

static Device_MPU_t *create(void);
static void run(GlobalClock_LocalClock_t *clk, void *data);


//==============================================================================
//= Function definitions(static)
//==============================================================================

/*
 * ----------------------------------------------
 * First the operations for debugger access to the
 * ARM system. GDB expects registers to be in
 * target byteorder
 * ----------------------------------------------
 */
static int
debugger_getreg(void *clientData, uint8_t * data, uint32_t index, int maxlen)
{
	uint32_t value;
	if (maxlen < 4)
		return -EINVAL;
	if (index < 15) {
		value = ARM9_ReadReg(index);
	} else if (index == 15) {
		if (REG_CPSR & FLAG_T) {
			value = THUMB_NIA;
		} else {
			value = ARM_NIA;
		}
		dbgprintf("Read reg PC %08x\n", value);
	} else if (index < 25) {
		value = 0;
	} else if (index == 25) {
		value = REG_CPSR;
	} else {
		return 0;
	}
	if (MMU_Byteorder() == BYTE_ORDER_BIG) {
		BYTE_WriteToBe32(data, 0, value);
	} else {
		BYTE_WriteToLe32(data, 0, value);
	}
	return 4;
}

static void
debugger_setreg(void *clientData, const uint8_t * data, uint32_t index, int len)
{
	uint32_t value;
	if (len != 4)
		return;
	if (MMU_Byteorder() == BYTE_ORDER_BIG) {
		BYTE_WriteToBe32(&value, 0, *((uint32_t *)data));
	} else {
		BYTE_WriteToLe32(&value, 0, *((uint32_t *)data));
	}
	if (index < 16) {
		ARM9_WriteReg(value, index);
	} else if (index < 25) {
		return;
	} else if (index == 25) {
		SET_REG_CPSR(value);
	} else {
		return;
	}
	return;
}

/*
 * ---------------------------------------------------------------------
 * get_bkpt_ins
 * 	returns the operation code of the breakpoint instruction.
 *	Needed by the debugger to insert breakpoints
 * ---------------------------------------------------------------------
 */
static void
debugger_get_bkpt_ins(void *clientData, uint8_t * ins, uint64_t addr, int len)
{
	if (len == 2) {
		uint16_t value = 0xbe00;
		if (MMU_Byteorder() == BYTE_ORDER_BIG) {
			BYTE_WriteToBe16(ins, 0, value);
		} else {
			BYTE_WriteToLe16(ins, 0, value);
		}
	} else if (len == 4) {
		uint32_t value = 0xe1200070;
		if (MMU_Byteorder() == BYTE_ORDER_BIG) {
			BYTE_WriteToBe32(ins, 0, value);
		} else {
			BYTE_WriteToLe32(ins, 0, value);
		}
	}
}

static int
debugger_stop(void *clientData)
{

	gcpu.dbg_state = DBG_STATE_STOP;
	ARM_SigDebugMode(true);
	return -1;
}

static int
debugger_cont(void *clientData)
{
	//fprintf(stderr,"ARM cont\n");
	gcpu.dbg_state = DBG_STATE_RUNNING;
	/* Should only be called if there are no breakpoints */
	ARM_SigDebugMode(false);
	return 0;
}

static int
debugger_step(void *clientData, uint64_t addr, int use_addr)
{
	if (use_addr) {
		ARM_SET_NIA(addr);
	}
	gcpu.dbg_steps = 1;
	gcpu.dbg_state = DBG_STATE_STEP;
	return -1;
}

static Dbg_TargetStat
debugger_get_status(void *clientData)
{
	if (gcpu.dbg_state == DBG_STATE_STOPPED) {
		return DbgStat_SIGINT;
	} else if (gcpu.dbg_state == DBG_STATE_RUNNING) {
		return DbgStat_RUNNING;
	} else {
		return -1;
	}
}

/*
 * --------------------------------------------------------------------
 * Gdb getmem backend 
 * --------------------------------------------------------------------
 */

static ssize_t
debugger_getmem(void *clientData, uint8_t * data, uint64_t addr, uint32_t len)
{
	int count;
	/* catch exceptions from MMU */
	count = 0;
	MMU_SetDebugMode(1);
	if (setjmp(gcpu.abort_jump)) {
		MMU_SetDebugMode(0);
		return count;
	}
	for (; len >= 4; len -= 4, count += 4, data += 4) {
		uint32_t value = MMU_Read32(addr + count);
		if (MMU_Byteorder() == BYTE_ORDER_BIG) {
			BYTE_WriteToBe32(data, 0, value);
		} else {
			BYTE_WriteToLe32(data, 0, value);
		}
	}
	for (; len > 2; len -= 2, count += 2, data += 2) {
		uint16_t value = MMU_Read16(addr + count);
		if (MMU_Byteorder() == BYTE_ORDER_BIG) {
			BYTE_WriteToBe16(data, 0, value);
		} else {
			BYTE_WriteToLe16(data, 0, value);
		}
	}
	for (; len > 0; len--, count++, data++) {
		uint8_t value = MMU_Read8(addr + count);
		*data = value;
	}
	MMU_SetDebugMode(0);
	return count;
}

static ssize_t
debugger_setmem(void *clientData, const uint8_t * data, uint64_t addr, uint32_t len)
{
	int count = 0;
	//fprintf(stderr,"ARM readmem\n");
	/* catch exceptions from MMU */
	MMU_SetDebugMode(1);
	if (setjmp(gcpu.abort_jump)) {
		MMU_SetDebugMode(0);
		return count;
	}
	for (; len >= 4; len -= 4, count += 4, data += 4) {
		uint32_t value = *((uint32_t *) data);
		if (MMU_Byteorder() == BYTE_ORDER_BIG) {
			MMU_Write32(BYTE_HToBe32(value), addr + count);
		} else {
			MMU_Write32(BYTE_HToLe32(value), addr + count);
		}
	}
	for (; len > 2; len -= 2, count += 2, data += 2) {
		uint16_t value = *((uint32_t *) data);
		if (MMU_Byteorder() == BYTE_ORDER_BIG) {
			MMU_Write16(BYTE_HToBe16(value), addr +  count);
		} else {
			MMU_Write16(BYTE_HToLe16(value), addr + count);
		}
	}
	for (; len > 0; len--, count++, data++) {
		uint8_t value = *data;
		MMU_Write8(value, addr + count);
	}
	MMU_SetDebugMode(0);
	return count;
}

/* 
 * -------------------------------------------------------------
 * Setup register Pointers to the mode dependent register sets 
 * -------------------------------------------------------------
 */
static void
ARM9_InitRegs(ARM9 * arm)
{
	int i;
	arm->regSet[MODE_USER].r0 = &arm->r0;
	arm->regSet[MODE_FIQ].r0 = &arm->r0;
	arm->regSet[MODE_IRQ].r0 = &arm->r0;
	arm->regSet[MODE_SVC].r0 = &arm->r0;
	arm->regSet[MODE_ABORT].r0 = &arm->r0;
	arm->regSet[MODE_UNDEFINED].r0 = &arm->r0;
	arm->regSet[MODE_SYSTEM].r0 = &arm->r0;

	arm->regSet[MODE_USER].r1 = &arm->r1;
	arm->regSet[MODE_FIQ].r1 = &arm->r1;
	arm->regSet[MODE_IRQ].r1 = &arm->r1;
	arm->regSet[MODE_SVC].r1 = &arm->r1;
	arm->regSet[MODE_ABORT].r1 = &arm->r1;
	arm->regSet[MODE_UNDEFINED].r1 = &arm->r1;
	arm->regSet[MODE_SYSTEM].r1 = &arm->r1;

	arm->regSet[MODE_USER].r2 = &arm->r2;
	arm->regSet[MODE_FIQ].r2 = &arm->r2;
	arm->regSet[MODE_IRQ].r2 = &arm->r2;
	arm->regSet[MODE_SVC].r2 = &arm->r2;
	arm->regSet[MODE_ABORT].r2 = &arm->r2;
	arm->regSet[MODE_UNDEFINED].r2 = &arm->r2;
	arm->regSet[MODE_SYSTEM].r2 = &arm->r2;

	arm->regSet[MODE_USER].r3 = &arm->r3;
	arm->regSet[MODE_FIQ].r3 = &arm->r3;
	arm->regSet[MODE_IRQ].r3 = &arm->r3;
	arm->regSet[MODE_SVC].r3 = &arm->r3;
	arm->regSet[MODE_ABORT].r3 = &arm->r3;
	arm->regSet[MODE_UNDEFINED].r3 = &arm->r3;
	arm->regSet[MODE_SYSTEM].r3 = &arm->r3;

	arm->regSet[MODE_USER].r4 = &arm->r4;
	arm->regSet[MODE_FIQ].r4 = &arm->r4;
	arm->regSet[MODE_IRQ].r4 = &arm->r4;
	arm->regSet[MODE_SVC].r4 = &arm->r4;
	arm->regSet[MODE_ABORT].r4 = &arm->r4;
	arm->regSet[MODE_UNDEFINED].r4 = &arm->r4;
	arm->regSet[MODE_SYSTEM].r4 = &arm->r4;

	arm->regSet[MODE_USER].r5 = &arm->r5;
	arm->regSet[MODE_FIQ].r5 = &arm->r5;
	arm->regSet[MODE_IRQ].r5 = &arm->r5;
	arm->regSet[MODE_SVC].r5 = &arm->r5;
	arm->regSet[MODE_ABORT].r5 = &arm->r5;
	arm->regSet[MODE_UNDEFINED].r5 = &arm->r5;
	arm->regSet[MODE_SYSTEM].r5 = &arm->r5;

	arm->regSet[MODE_USER].r6 = &arm->r6;
	arm->regSet[MODE_FIQ].r6 = &arm->r6;
	arm->regSet[MODE_IRQ].r6 = &arm->r6;
	arm->regSet[MODE_SVC].r6 = &arm->r6;
	arm->regSet[MODE_ABORT].r6 = &arm->r6;
	arm->regSet[MODE_UNDEFINED].r6 = &arm->r6;
	arm->regSet[MODE_SYSTEM].r6 = &arm->r6;

	arm->regSet[MODE_USER].r7 = &arm->r7;
	arm->regSet[MODE_FIQ].r7 = &arm->r7;
	arm->regSet[MODE_IRQ].r7 = &arm->r7;
	arm->regSet[MODE_SVC].r7 = &arm->r7;
	arm->regSet[MODE_ABORT].r7 = &arm->r7;
	arm->regSet[MODE_UNDEFINED].r7 = &arm->r7;
	arm->regSet[MODE_SYSTEM].r7 = &arm->r7;

	arm->regSet[MODE_USER].r8 = &arm->r8;
	arm->regSet[MODE_IRQ].r8 = &arm->r8;
	arm->regSet[MODE_SVC].r8 = &arm->r8;
	arm->regSet[MODE_ABORT].r8 = &arm->r8;
	arm->regSet[MODE_UNDEFINED].r8 = &arm->r8;
	arm->regSet[MODE_SYSTEM].r8 = &arm->r8;
	arm->regSet[MODE_FIQ].r8 = &arm->r8_fiq;

	arm->regSet[MODE_USER].r9 = &arm->r9;
	arm->regSet[MODE_IRQ].r9 = &arm->r9;
	arm->regSet[MODE_SVC].r9 = &arm->r9;
	arm->regSet[MODE_ABORT].r9 = &arm->r9;
	arm->regSet[MODE_UNDEFINED].r9 = &arm->r9;
	arm->regSet[MODE_SYSTEM].r9 = &arm->r9;
	arm->regSet[MODE_FIQ].r9 = &arm->r9_fiq;

	arm->regSet[MODE_USER].r10 = &arm->r10;
	arm->regSet[MODE_IRQ].r10 = &arm->r10;
	arm->regSet[MODE_SVC].r10 = &arm->r10;
	arm->regSet[MODE_ABORT].r10 = &arm->r10;
	arm->regSet[MODE_UNDEFINED].r10 = &arm->r10;
	arm->regSet[MODE_SYSTEM].r10 = &arm->r10;
	arm->regSet[MODE_FIQ].r10 = &arm->r10_fiq;

	arm->regSet[MODE_USER].r11 = &arm->r11;
	arm->regSet[MODE_IRQ].r11 = &arm->r11;
	arm->regSet[MODE_SVC].r11 = &arm->r11;
	arm->regSet[MODE_ABORT].r11 = &arm->r11;
	arm->regSet[MODE_UNDEFINED].r11 = &arm->r11;
	arm->regSet[MODE_SYSTEM].r11 = &arm->r11;
	arm->regSet[MODE_FIQ].r11 = &arm->r11_fiq;

	arm->regSet[MODE_USER].r12 = &arm->r12;
	arm->regSet[MODE_IRQ].r12 = &arm->r12;
	arm->regSet[MODE_SVC].r12 = &arm->r12;
	arm->regSet[MODE_ABORT].r12 = &arm->r12;
	arm->regSet[MODE_UNDEFINED].r12 = &arm->r12;
	arm->regSet[MODE_SYSTEM].r12 = &arm->r12;
	arm->regSet[MODE_FIQ].r12 = &arm->r12_fiq;

	arm->regSet[MODE_USER].r13 = &arm->r13;
	arm->regSet[MODE_SYSTEM].r13 = &arm->r13;
	arm->regSet[MODE_IRQ].r13 = &arm->r13_irq;
	arm->regSet[MODE_SVC].r13 = &arm->r13_svc;
	arm->regSet[MODE_ABORT].r13 = &arm->r13_abt;
	arm->regSet[MODE_UNDEFINED].r13 = &arm->r13_und;
	arm->regSet[MODE_FIQ].r13 = &arm->r13_fiq;

	arm->regSet[MODE_USER].r14 = &arm->r14;
	arm->regSet[MODE_SYSTEM].r14 = &arm->r14;
	arm->regSet[MODE_IRQ].r14 = &arm->r14_irq;
	arm->regSet[MODE_SVC].r14 = &arm->r14_svc;
	arm->regSet[MODE_ABORT].r14 = &arm->r14_abt;
	arm->regSet[MODE_UNDEFINED].r14 = &arm->r14_und;
	arm->regSet[MODE_FIQ].r14 = &arm->r14_fiq;

	arm->regSet[MODE_USER].pc = &arm->registers[15];
	arm->regSet[MODE_SYSTEM].pc = &arm->registers[15];
	arm->regSet[MODE_IRQ].pc = &arm->registers[15];
	arm->regSet[MODE_SVC].pc = &arm->registers[15];
	arm->regSet[MODE_ABORT].pc = &arm->registers[15];
	arm->regSet[MODE_UNDEFINED].pc = &arm->registers[15];
	arm->regSet[MODE_FIQ].pc = &arm->registers[15];

	arm->regSet[MODE_IRQ].spsr = &arm->spsr_irq;
	arm->regSet[MODE_SVC].spsr = &arm->spsr_svc;
	arm->regSet[MODE_ABORT].spsr = &arm->spsr_abt;
	arm->regSet[MODE_UNDEFINED].spsr = &arm->spsr_und;
	arm->regSet[MODE_FIQ].spsr = &arm->spsr_fiq;

	if (arm->cpuArchitecture == ARCH_ARMV7) {
		arm->regSet[MODE_SECMON].r0 = &arm->r0;
		arm->regSet[MODE_SECMON].r1 = &arm->r1;
		arm->regSet[MODE_SECMON].r2 = &arm->r2;
		arm->regSet[MODE_SECMON].r3 = &arm->r3;
		arm->regSet[MODE_SECMON].r4 = &arm->r4;
		arm->regSet[MODE_SECMON].r5 = &arm->r5;
		arm->regSet[MODE_SECMON].r6 = &arm->r6;
		arm->regSet[MODE_SECMON].r7 = &arm->r7;
		arm->regSet[MODE_SECMON].r8 = &arm->r8;
		arm->regSet[MODE_SECMON].r9 = &arm->r9;
		arm->regSet[MODE_SECMON].r10 = &arm->r10;
		arm->regSet[MODE_SECMON].r11 = &arm->r11;
		arm->regSet[MODE_SECMON].r12 = &arm->r12;
		arm->regSet[MODE_SECMON].r13 = &arm->r13_mon;
		arm->regSet[MODE_SECMON].r14 = &arm->r14_mon;
		arm->regSet[MODE_SECMON].pc = &arm->registers[15];
		arm->regSet[MODE_SECMON].spsr = &arm->spsr_mon;
	}
	/* To avoid segfault if CPU is in illegal mode initialize with some nonsense */
	/* LPC 2106 ignore mode bit 4 of CPSR! Should be implemented and Tested with other CPU's */
	for (i = 0; i < 32; i++) {
		if (arm->regSet[i].r0 == NULL) {
			arm->regSet[i].r0 = &arm->reg_dummy;
		}
		if (arm->regSet[i].r1 == NULL) {
			arm->regSet[i].r1 = &arm->reg_dummy;
		}
		if (arm->regSet[i].r2 == NULL) {
			arm->regSet[i].r2 = &arm->reg_dummy;
		}
		if (arm->regSet[i].r3 == NULL) {
			arm->regSet[i].r3 = &arm->reg_dummy;
		}
		if (arm->regSet[i].r4 == NULL) {
			arm->regSet[i].r4 = &arm->reg_dummy;
		}
		if (arm->regSet[i].r5 == NULL) {
			arm->regSet[i].r5 = &arm->reg_dummy;
		}
		if (arm->regSet[i].r6 == NULL) {
			arm->regSet[i].r6 = &arm->reg_dummy;
		}
		if (arm->regSet[i].r7 == NULL) {
			arm->regSet[i].r7 = &arm->reg_dummy;
		}
		if (arm->regSet[i].r8 == NULL) {
			arm->regSet[i].r8 = &arm->reg_dummy;
		}
		if (arm->regSet[i].r9 == NULL) {
			arm->regSet[i].r9 = &arm->reg_dummy;
		}
		if (arm->regSet[i].r10 == NULL) {
			arm->regSet[i].r10 = &arm->reg_dummy;
		}
		if (arm->regSet[i].r11 == NULL) {
			arm->regSet[i].r11 = &arm->reg_dummy;
		}
		if (arm->regSet[i].r12 == NULL) {
			arm->regSet[i].r12 = &arm->reg_dummy;
		}
		if (arm->regSet[i].r13 == NULL) {
			arm->regSet[i].r13 = &arm->reg_dummy;
		}
		if (arm->regSet[i].r14 == NULL) {
			arm->regSet[i].r14 = &arm->reg_dummy;
		}
		if (arm->regSet[i].pc == NULL) {
			arm->regSet[i].pc = &arm->reg_dummy;
		}
	}
	fprintf(stderr, "- Register Pointers initialized\n");
}

static void
hello_proc(void *cd)
{
	struct timeval *tv_start;
	struct timeval tv_now;
	unsigned int time;
	gettimeofday(&tv_now, NULL);
	tv_start = &gcpu.starttime;
	time = (tv_now.tv_sec - tv_start->tv_sec) * 1000
	    + ((tv_now.tv_usec - tv_start->tv_usec) / 1000);
	dbgprintf("\nSimulator speed %d kHz\n", (int)(CycleCounter_Get() / time));
	//fprintf(stderr,"\nSimulator speed %d kHz\n",(int) (CycleCounter_Get()/time));
#ifdef PROFILE
	exit(0);
#endif
//      CycleTimer_Add(&htimer,10000000000LL,hello_proc,NULL);
}

static void
irq_change(SigNode * node, int value, void *clientData)
{
	if ((value == SIG_LOW) || (value == SIG_PULLDOWN)) {
		ARM_PostIrq();
	} else {
		ARM_UnPostIrq();
	}
}

static void
fiq_change(SigNode * node, int value, void *clientData)
{
	if ((value == SIG_LOW) || (value == SIG_PULLDOWN)) {
		ARM_PostFiq();
	} else {
		ARM_UnPostFiq();
	}
}

/*
 *************************************************************************+
 * Throttle timer for OS not using the halt instruction 
 *************************************************************************+
 */
static void
arm_throttle(void *clientData)
{
	ARM9 *arm = (ARM9 *) clientData;;
	struct timespec tv_now;
	uint32_t nsecs;
	int64_t exp_cpu_cycles, done_cpu_cycles;
	done_cpu_cycles = CycleCounter_Get() - arm->last_throttle_cycles;
	arm->cycles_ahead += done_cpu_cycles;
	do {
		struct timespec tout;
		tout.tv_nsec = 10000000;	/* 10 ms */
		tout.tv_sec = 0;

		clock_gettime(CLOCK_MONOTONIC, &tv_now);
		nsecs = (tv_now.tv_nsec - arm->tv_last_throttle.tv_nsec) +
		    (int64_t) 1000000000 *(tv_now.tv_sec - arm->tv_last_throttle.tv_sec);
		exp_cpu_cycles = NanosecondsToCycles(nsecs);
		if (arm->cycles_ahead > exp_cpu_cycles) {
			// FIXME: FIO_WaitEventTimeout(&tout);
		}
	} while (arm->cycles_ahead > exp_cpu_cycles);
	arm->cycles_ahead -= exp_cpu_cycles;
	/*
	 **********************************************************
	 * Forget about catch up if CPU is more than on second
	 * behind to avoid a longer phase of overspeed. Sound
	 * doesn't like it !
	 **********************************************************
	 */
	if (-arm->cycles_ahead > (CycleTimerRate_Get() >> 2)) {
		arm->cycles_ahead = 0;
	}
	arm->last_throttle_cycles = CycleCounter_Get();
	arm->tv_last_throttle = tv_now;
	CycleTimer_Mod(&arm->throttle_timer, CycleTimerRate_Get() / 10);
	return;
}

static void
ARM_ThrottleInit(ARM9 * arm)
{
	clock_gettime(CLOCK_MONOTONIC, &arm->tv_last_throttle);
	arm->last_throttle_cycles = 0;
	CycleTimer_Add(&arm->throttle_timer, CycleTimerRate_Get() / 25, arm_throttle, arm);
}

static void
dump_stack(void)
{
	int i;
	uint32_t sp, value;
	sp = ARM9_ReadReg(0xd);
	for (i = 0; i < (72 / 4); i++) {
		if ((i & 3) == 0) {
			dbgprintf("%08x:  ", sp + 4 * i);
		}
		value = MMU_Read32(sp + 4 * i);
		dbgprintf("%08x ", value);
		if ((i & 3) == 3) {
			dbgprintf("\n");
		}
	}
	dbgprintf("\n");

}

__UNUSED__ static void
dump_regs(void)
{
	int i;
	for (i = 0; i < 16; i++) {
		uint32_t value;
		value = ARM9_ReadReg(i);
		dbgprintf("R%02d: %08x    ", i, value);
		if ((i & 3) == 3) {
			dbgprintf("\n");
		}
	}
	dbgprintf("CPSR: %08x \n", REG_CPSR);
	fflush(stderr);
}

static void
Do_Debug(void)
{
	fprintf(stderr, "one at %08x\n", ARM_NIA);
	if (likely(gcpu.dbg_state == DBG_STATE_RUNNING)) {
		fprintf(stderr, "Debug mode is of, should not be called\n");
	} else if (gcpu.dbg_state == DBG_STATE_STEP) {
		if (gcpu.dbg_steps == 0) {
			gcpu.dbg_state = DBG_STATE_STOPPED;
			fprintf(stderr, "stopped at CIA %08x\n", ARM_GET_CIA);
			if (gcpu.debugger) {
				Debugger_Notify(gcpu.debugger, DbgStat_SIGTRAP);
			}
			ARM_RestartIdecoder();
		} else {
			fprintf(stderr, "step at CIA %08x\n", ARM_GET_CIA);
			gcpu.dbg_steps--;
		}
	} else if (gcpu.dbg_state == DBG_STATE_STOP) {
		fprintf(stderr, "stopped at CIA %08x\n", ARM_GET_CIA);
		gcpu.dbg_state = DBG_STATE_STOPPED;
		if (gcpu.debugger) {
			Debugger_Notify(gcpu.debugger, DbgStat_SIGTRAP);
		}
		ARM_RestartIdecoder();
	} else if (gcpu.dbg_state == DBG_STATE_BREAK) {
		fprintf(stderr, "break at CIA %08x\n", ARM_GET_CIA);
		if (gcpu.debugger) {
			/* Stop only if the debugger shows a reaction */
			if (Debugger_Notify(gcpu.debugger, DbgStat_SIGINT) > 0) {
				gcpu.dbg_state = DBG_STATE_STOPPED;
				ARM_RestartIdecoder();
			}
		}
		ARM_Exception(EX_PABT, 4);
		gcpu.dbg_state = DBG_STATE_RUNNING;
	} else {
		fprintf(stderr, "Unknown restart signal reason %d\n", gcpu.dbg_state);
	}
}

/*
 * -----------------------------------------------------------------------
 * Check Signals
 * 	Called after every instruction to check for queued IO-jobs
 *	and for the signals IRQ and FIQ
 * -----------------------------------------------------------------------
 */
static inline void
CheckSignals(void)
{
	if (gcpu.signals) {
		if (likely(gcpu.signals & ARM_SIG_IRQ)) {
			ARM_Exception(EX_IRQ, 4);
		}
		if (unlikely(gcpu.signals & ARM_SIG_FIQ)) {
			ARM_Exception(EX_FIQ, 4);
		}
		if (unlikely(gcpu.signals & ARM_SIG_DEBUGMODE)) {
			Do_Debug();
		}
		if (unlikely(gcpu.signals & ARM_SIG_RESTART_IDEC)) {
			ARM_RestartIdecoder();
		}
	}
}

static inline void
debug_print_instruction(uint32_t icode)
{
#ifdef DEBUG
	if (unlikely(debugflags)) {
		if (debugflags & 1) {
			Instruction *instr;
			instr = InstructionFind(icode);
			dbgprintf("Instruction %08x, name %s at %08x\n", icode, instr->name,
				  ARM_NIA);

		}
		if (debugflags & 2) {
			dump_regs();
			dump_stack();
			//usleep(10000);
		}
	}
#endif
}

static void
Thumb_Loop(void)
{
	ThumbInstructionProc *iproc;
	ThumbInstruction *instr;
	//fprintf(stderr,"Entering Thumb loop\n");
	setjmp(gcpu.abort_jump);
	while (1) {
		GlobalClock_ConsumeCycle(gcpu.clk, 2);
		CycleCounter += 2;
		CheckSignals();
		ICODE = MMU_IFetch16(ARM_NIA);
		ARM_NIA += 2;
		instr = ThumbInstruction_Find(ICODE);
		//fprintf(stderr,"Instruction %08x, name %s at %08x\n",ICODE,instr->name,ARM_NIA);
		iproc = ThumbInstructionProc_Find(ICODE);
		iproc();
		CycleTimers_Check();
	}
}

/*
 * ---------------------------------------------
 * The main loop for 32Bit instruction set
 * ---------------------------------------------
 */
static void
ARM9_Loop32(void)
{
	InstructionProc *iproc;
	/* Exceptions use goto (longjmp) */
	setjmp(gcpu.abort_jump);
	while (1) {
#if VERBOSE
		fprintf(stdout, "CIA %08x\n", ARM_GET_CIA);
#endif
		CheckSignals();
		GlobalClock_ConsumeCycle(gcpu.clk, 6);
		CycleCounter += 6;
		ICODE = MMU_IFetch(ARM_NIA);
		ARM_NIA += 4;
		iproc = InstructionProcFind(ICODE);
		debug_print_instruction(ICODE);
		iproc();
		CycleTimers_Check();

#if VERBOSE
		fprintf(stdout, "CIA %08x\n", ARM_GET_CIA);
#endif
		CheckSignals();
		ICODE = MMU_IFetch(ARM_NIA);
		ARM_NIA += 4;
		iproc = InstructionProcFind(ICODE);
		debug_print_instruction(ICODE);
		iproc();

#if VERBOSE
		fprintf(stdout, "CIA %08x\n", ARM_GET_CIA);
		fflush(stdout);
#endif
		CheckSignals();
		ICODE = MMU_IFetch(ARM_NIA);
		ARM_NIA += 4;
		iproc = InstructionProcFind(ICODE);
		debug_print_instruction(ICODE);
		iproc();
	}
}

/*
 * -----------------------------------------------------
 * Create a new ARM9 CPU
 * -----------------------------------------------------
 */
static Device_MPU_t *
create(void)
{
	uint32_t cpu_clock = 200000000;
	int i;
	const char *instancename = "arm";
	ARM9 *arm = &gcpu;
	Device_MPU_t *dev;
	dev = calloc(1, sizeof(*dev));
	dev->base.self = arm;
	Config_ReadUInt32(&cpu_clock, "global", "cpu_clock");
	fprintf(stderr, "Creating ARM9 CPU with clock %d HZ\n", cpu_clock);
	memset(arm, 0, sizeof(ARM9));
	IDecoder_New();
	ARM9_InitRegs(&gcpu);
	InitInstructions();
	ThumbDecoder_New();
	SET_REG_CPSR(MODE_SVC | FLAG_F | FLAG_I);
	GlobalClock_Registor(&run, dev, cpu_clock);
	CycleTimers_Init(instancename, cpu_clock);
	CycleTimer_Add(&htimer, 285000000, hello_proc, NULL);
	arm->irqNode = SigNode_New("%s.irq", instancename);
	arm->fiqNode = SigNode_New("%s.fiq", instancename);
	if (!arm->irqNode || !arm->fiqNode) {
		fprintf(stderr, "Can not create interrupt nodes for ARM CPU\n");
		exit(2);
	}
	arm->irqTrace = SigNode_Trace(arm->irqNode, irq_change, arm);
	arm->irqTrace = SigNode_Trace(arm->fiqNode, fiq_change, arm);
	arm->dbgops.getreg = debugger_getreg;
	arm->dbgops.setreg = debugger_setreg;
	arm->dbgops.stop = debugger_stop;
	arm->dbgops.step = debugger_step;
	arm->dbgops.cont = debugger_cont;
	arm->dbgops.get_status = debugger_get_status;
	arm->dbgops.getmem = debugger_getmem;
	arm->dbgops.setmem = debugger_setmem;
	arm->dbgops.get_bkpt_ins = debugger_get_bkpt_ins;
	arm->debugger = Debugger_New(&arm->dbgops, arm);
	gcpu.signal_mask |= ARM_SIG_RESTART_IDEC | ARM_SIG_DEBUGMODE;
	ARM_ThrottleInit(arm);
	for (i = 0; i < 16; i++) {
		char regname[10];
		uint32_t value;
		sprintf(regname, "r%d", i);
		if (Config_ReadUInt32(&value, instancename, regname) >= 0) {
			ARM9_WriteReg(value, i);
		}
	}
	return dev;
}

static void
run(GlobalClock_LocalClock_t *clk, void *data)
{
	Device_MPU_t *dev = data;
	ARM9 *arm = dev->base.self;
	uint32_t addr = 0;
	uint32_t dbgwait;
	arm->clk = clk;
	if (Config_ReadUInt32(&addr, "global", "start_address") < 0) {
		addr = 0;
	}
	if (Config_ReadUInt32(&dbgwait, "global", "dbgwait") < 0) {
		dbgwait = 0;
	}
	if (dbgwait) {
		fprintf(stderr, "CPU is waiting for debugger connection at %08x\n", addr);
		gcpu.dbg_state = DBG_STATE_STOPPED;
		ARM_SigDebugMode(true);
	} else {
		fprintf(stderr, "Starting CPU at %08x\n", addr);
	}
	gettimeofday(&gcpu.starttime, NULL);
	ARM_NIA = addr;
	/* A long jump to this label redecides which main loop is used  */
	setjmp(gcpu.restart_idec_jump);
	gcpu.signals &= ~ARM_SIG_RESTART_IDEC;
	gcpu.signals_raw &= ~ARM_SIG_RESTART_IDEC;
	while (1) {
		if (unlikely(gcpu.dbg_state == DBG_STATE_STOPPED)) {
			struct timespec tout;
			tout.tv_nsec = 0;
			tout.tv_sec = 10000;
			// FIXME: FIO_WaitEventTimeout(&tout);
			sleep(1);
		} else {
			if (REG_CPSR & FLAG_T) {
				Thumb_Loop();
			} else {
				ARM9_Loop32();
			}
		}
	}
}


//==============================================================================
//= Function definitions(global)
//==============================================================================
void
ARM_set_reg_cpsr(uint32_t new_cpsr)
{
	uint32_t bank = new_cpsr & 0x1f;
	uint32_t diff_cpsr = new_cpsr ^ gcpu.reg_cpsr;
	gcpu.reg_cpsr = new_cpsr;
	if (gcpu.reg_bank != bank) {
		if (likely(gcpu.reg_bank != MODE_FIQ)) {
			uint32_t *rp;
			*gcpu.regSet[gcpu.reg_bank].r13 = gcpu.registers[13];
			*gcpu.regSet[gcpu.reg_bank].r14 = gcpu.registers[14];
			rp = gcpu.regSet[gcpu.reg_bank].spsr;
			if (rp)
				*rp = gcpu.registers[16];
		} else {
			gcpu.r8_fiq = gcpu.registers[8];
			gcpu.r9_fiq = gcpu.registers[9];
			gcpu.r10_fiq = gcpu.registers[10];
			gcpu.r11_fiq = gcpu.registers[11];
			gcpu.r12_fiq = gcpu.registers[12];
			gcpu.r13_fiq = gcpu.registers[13];
			gcpu.r14_fiq = gcpu.registers[14];
			gcpu.spsr_fiq = gcpu.registers[16];
		}
		if (likely(bank != MODE_FIQ)) {
			uint32_t *rp;
			gcpu.registers[13] = *gcpu.regSet[bank].r13;
			gcpu.registers[14] = *gcpu.regSet[bank].r14;
			rp = gcpu.regSet[bank].spsr;
			if (rp)
				gcpu.registers[16] = *rp;
		} else {
			gcpu.registers[8] = gcpu.r8_fiq;
			gcpu.registers[9] = gcpu.r9_fiq;
			gcpu.registers[10] = gcpu.r10_fiq;
			gcpu.registers[11] = gcpu.r11_fiq;
			gcpu.registers[12] = gcpu.r12_fiq;
			gcpu.registers[13] = gcpu.r13_fiq;
			gcpu.registers[14] = gcpu.r14_fiq;
			gcpu.registers[16] = gcpu.spsr_fiq;
		}
		gcpu.signaling_mode = gcpu.reg_bank = bank;
	}
	if (unlikely(new_cpsr & FLAG_I)) {
		gcpu.signal_mask &= ~ARM_SIG_IRQ;
	} else {
		gcpu.signal_mask |= ARM_SIG_IRQ;
	}
	if (unlikely(new_cpsr & FLAG_F)) {
		gcpu.signal_mask &= ~ARM_SIG_FIQ;
	} else {
		gcpu.signal_mask |= ARM_SIG_FIQ;
	}
	if (unlikely(diff_cpsr & FLAG_T)) {
		ARM_PostRestartIdecoder();
	}
	gcpu.signals = gcpu.signals_raw & gcpu.signal_mask;
}

/*
 *********************************************************
 * \fn ARM_Exception(int exception,int nia_offset); 
 * Generate an exception and call the handler.
 * \param exception 
 * ----------------------------------------------------
 */
void
ARM_Exception(ARM_ExceptionID exception, int nia_offset)
{
	uint32_t old_cpsr = REG_CPSR;
	int new_mode = EX_TO_MODE(exception);
	uint32_t new_pc = EX_TO_PC(exception);
	uint32_t retaddr;

	retaddr = ARM_NIA + nia_offset;

	new_pc |= mmu_vector_base;
	/* Save CPSR to SPSR in the bank of the new mode */
	/* clear Thumb, set 0-4 to mode , disable IRQ */
	SET_REG_CPSR((REG_CPSR & (0xffffffe0 & ~FLAG_T)) | new_mode | FLAG_I);
	if (new_mode == MODE_FIQ) {
		REG_CPSR = REG_CPSR | FLAG_F;
	}
	ARM9_WriteReg(old_cpsr, REG_NR_SPSR);
	REG_LR = retaddr;
	/* jump to exception vector address */
	ARM_SET_NIA(new_pc);
}

DEVICE_REGISTER_MPU(MPU_NAME, MPU_DESCRIPTION, &create, MPU_DEFAULTCONFIG);

