/*
 *************************************************************************************************
 *
 * Emulation of Coldfire CPU 
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

#include <cpu_cf.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <idecode_cf.h>
#include <instructions_cf.h>
#include <cycletimer.h>
#include <configfile.h>
#include <mem_cf.h>

CFCpu g_CFCpu;

void
CF_SetRegCR(uint32_t value, int reg)
{
	switch (reg) {
	    case CR_REG_CACR:
		    g_CFCpu.reg_CACR = value;
		    break;
	    case CR_REG_ASID:
		    g_CFCpu.reg_ASID = value;
		    break;
	    case CR_REG_ACR0:
		    g_CFCpu.reg_ACR[0] = value;
		    break;
	    case CR_REG_ACR1:
		    g_CFCpu.reg_ACR[1] = value;
		    break;
	    case CR_REG_ACR2:
		    g_CFCpu.reg_ACR[2] = value;
		    break;
	    case CR_REG_ACR3:
		    g_CFCpu.reg_ACR[3] = value;
		    break;
	    case CR_REG_MMUBAR:
		    g_CFCpu.reg_MMUBAR = value;
		    break;

	    case CR_REG_VBR:
		    g_CFCpu.reg_VBR = value;
		    break;

	    case CR_REG_PC:
		    //  ? 
		    fprintf(stderr, "Control register 0x%02x not implemented\n", reg);
		    break;

	    case CR_REG_FLASHBAR:
		    fprintf(stderr, "CPU FLASHBAR 0x%08x\n", value);
		    g_CFCpu.reg_FLASHBAR = value;
		    break;

		    /* MCF5282 implements RAMBAR1 (0xc05) */
	    case CR_REG_RAMBAR:
		    fprintf(stderr, "CPU RAMBAR 0x%08x\n", value);
		    g_CFCpu.reg_RAMBAR = value;
		    break;

	    case CR_REG_MPCR:
	    case CR_REG_EDRAMBAR:
	    case CR_REG_SECMBAR:
		    fprintf(stderr, "Control register 0x%02x not implemented\n", reg);
		    break;

	    case CR_REG_MBAR:
		    g_CFCpu.reg_MBAR = value;
		    break;

	    case CR_REG_PCR1U0:
	    case CR_REG_PCR1L0:
	    case CR_REG_PCR2U0:
	    case CR_REG_PCR2L0:
	    case CR_REG_PCR3U0:
	    case CR_REG_PCR3L0:
	    case CR_REG_PCR1U1:
	    case CR_REG_PCR1L1:
	    case CR_REG_PCR2U1:
	    case CR_REG_PCR2L1:
	    case CR_REG_PCR3U1:
	    case CR_REG_PCR3L1:
	    default:
		    fprintf(stderr, "Control register 0x%02x not implemented\n", reg);
		    break;
	}
}

uint32_t
CF_GetRegCR(int reg)
{
	uint32_t value = 0;
	switch (reg) {
	    case CR_REG_CACR:
		    value = g_CFCpu.reg_CACR;
		    break;

	    case CR_REG_ASID:
		    value = g_CFCpu.reg_ASID;
		    break;

	    case CR_REG_ACR0:
		    value = g_CFCpu.reg_ACR[0];
		    break;

	    case CR_REG_ACR1:
		    value = g_CFCpu.reg_ACR[1];
		    break;

	    case CR_REG_ACR2:
		    value = g_CFCpu.reg_ACR[2];
		    break;

	    case CR_REG_ACR3:
		    value = g_CFCpu.reg_ACR[3];
		    break;

	    case CR_REG_MMUBAR:
		    value = g_CFCpu.reg_MMUBAR;
		    break;

	    case CR_REG_VBR:
		    value = g_CFCpu.reg_VBR;
		    break;

	    case CR_REG_PC:
		    //  ? 
		    fprintf(stderr, "Control register 0x%02x not implemented\n", reg);
		    break;

	    case CR_REG_FLASHBAR:
		    value = g_CFCpu.reg_RAMBAR;
		    break;

	    case CR_REG_RAMBAR:
		    value = g_CFCpu.reg_RAMBAR;
		    break;

	    case CR_REG_MPCR:
	    case CR_REG_EDRAMBAR:
	    case CR_REG_SECMBAR:
		    fprintf(stderr, "Control register 0x%02x not implemented\n", reg);
		    break;

	    case CR_REG_MBAR:
		    value = g_CFCpu.reg_MBAR;
		    break;

	    case CR_REG_PCR1U0:
	    case CR_REG_PCR1L0:
	    case CR_REG_PCR2U0:
	    case CR_REG_PCR2L0:
	    case CR_REG_PCR3U0:
	    case CR_REG_PCR3L0:
	    case CR_REG_PCR1U1:
	    case CR_REG_PCR1L1:
	    case CR_REG_PCR2U1:
	    case CR_REG_PCR2L1:
	    case CR_REG_PCR3U1:
	    case CR_REG_PCR3L1:
	    default:
		    fprintf(stderr, "Control register 0x%02x not implemented\n", reg);
		    break;
	}
	return value;
}

void
CF_CpuInit(void)
{
	int32_t cpu_clock = 66000000;
	const char *instancename = "coldfire";
	g_CFCpu.reg_D = &g_CFCpu.reg_GP[0];
	g_CFCpu.reg_A = &g_CFCpu.reg_GP[8];
	Config_ReadInt32(&cpu_clock, "global", "cpu_clock");
	CF_IDecoderNew();
	cf_init_condition_tab();
	CycleTimers_Init(instancename, cpu_clock);
	fprintf(stderr, "Initialized Coldfire CPU with %d HZ\n", cpu_clock);
	CF_SetRegPC(0);
	CF_SetRegD(HWCONFIG_D0_MFC5282, 0);
	CF_SetRegD(HWCONFIG_D1_MFC5282, 1);
	CF_REG_CCR = 0x2700;	/* CFPRM 1.5.1 */

}

static inline void
CheckSignals()
{
#if 0
	if (g_CFCpu.signals) {
		if (likely(g_CFCpu.signals & CF_SIG_IRQ)) {
			CF_Exception();
		}
	}
#endif
}

static void
dump_instruction()
{
	Instruction *instr = CF_InstructionFind(ICODE);
	fprintf(stderr, "%08x: %04x %s d0 %08x\n", CF_GetRegPC(), ICODE, instr->name,
		CF_GetRegD(0));
}

void
CF_Exception(uint32_t vecnum, uint8_t fault_status)
{
	uint32_t formvec = (4 << 28) | (vecnum << 18);
	uint32_t pc = CF_GetRegPC();
	uint32_t sp;
	uint16_t sr = CF_REG_CCR;
	int misalignment;
	formvec |= sr;
	sr |= CCRS_S;
	sr &= ~CCRS_T;
	CF_SetRegSR(sr);
	sp = CF_GetRegA(7);
	misalignment = ((sp + 3) & ~4) - sp;
	if (misalignment) {
		sp -= misalignment;
		CF_SetRegA(sp, 7);
	}
	formvec |= (misalignment << 28);
	formvec |= (fault_status & 3) << 16;
	formvec |= (fault_status & 0xc) << 24;
	Push4(pc);
	Push4(formvec);
}

void
CF_Interrupt(uint32_t vecnum, uint8_t fault_status, int priority)
{
	uint32_t formvec = (4 << 28) | (vecnum << 18);
	uint32_t pc = CF_GetRegPC();
	uint32_t sp;
	uint16_t sr = CF_REG_CCR;
	int misalignment;
	formvec |= sr;
	sr |= CCRS_S;
	sr &= ~(CCRS_T | CCRS_M);
	sr |= (priority & 7) << 8;
	CF_SetRegSR(sr);
	sp = CF_GetRegA(7);
	misalignment = ((sp + 3) & ~4) - sp;
	sp -= misalignment;
	formvec |= (misalignment << 28);
	formvec |= (fault_status & 3) << 16;
	formvec |= (fault_status & 0xc) << 24;
	Push4(pc);
	Push4(formvec);
}

void
CF_CpuRun(void)
{
	InstructionProc *iproc;
	uint32_t pc, sp;
	sp = CF_MemRead32(0);
	pc = CF_MemRead32(4);
	CF_SetRegA(sp, 7);
	CF_SetRegPC(pc);
	fprintf(stderr, "Starting Coldfire CPU at 0x%08x\n", pc);
	while (1) {
		pc = CF_GetRegPC();
		ICODE = CF_MemRead16(pc);
		iproc = InststructionProcFind(ICODE);
		dump_instruction();
		CF_SetRegPC(pc + 2);
		iproc();
		CycleCounter += 2;	/* Should be moved to iprocs */
		CycleTimers_Check();
		CheckSignals();
	}
}
