/* 
 ********************************************************************************************** 
 * M32C CPU simulation
 *
 * Copyright 2010 Jochen Karrer. All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without modification, are
 * permitted provided that the following conditions are met:
 * 
 *   1. Redistributions of source code must retain the above copyright notice, this list of
 *       conditions and the following disclaimer.
 * 
 *    2. Redistributions in binary form must reproduce the above copyright notice, this list
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
 ********************************************************************************************** 
 */
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <string.h>
#include "cycletimer.h"
#include "configfile.h"
#include "setjmp.h"
#include "cpu_m32c.h"
#include "initializer.h"

#define REG_NULLPTR	(0x0)
#define REG_RLVL	(0x9f)
#define		RLVL_RLVL_MSK	(0x7)
#define		RLVL_FSIT	(1 << 3)
#define 	RLVL_DMACIISEL	(1 << 5)

#define REG_PRCR	(0x0a)
#define		PRCR_PRC0	(1 << 0)
#define		PRCR_PRC1	(1 << 1)
#define		PRCR_PRC2	(1 << 2)
#define		PRCR_PRC3	(1 << 3)

/* global variable for faster access */
M32C_Cpu gm32c;

static inline void
M32C_UpdateIPL(void)
{
	int cpu_ilvl = (M32C_REG_FLG & M32C_FLG_IPL_MSK) >> M32C_FLG_IPL_SHIFT;
	if (gm32c.pending_ilvl > cpu_ilvl) {
		//fprintf(stderr,"SigIrq, lvl %u, cpu %u\n",gm32c.pending_ilvl,cpu_ilvl);
		M32C_PostSignal(M32C_SIG_IRQ);
#if 0
		if (gm32c.pending_intno == 54) {
			fprintf(stderr,
				"Post CPU irq at %llu, mask %08x, sigs %08x, PC %06x, idx %x:%x\n",
				CycleCounter_Get(), gm32c.signals_mask, gm32c.signals, M32C_REG_PC,
				M32C_INDEXLS, M32C_INDEXLD);
		}
#endif
	} else {
		M32C_UnpostSignal(M32C_SIG_IRQ);
	}
}

void
M32C_PostILevel(int ilvl, int int_no)
{
	gm32c.pending_ilvl = ilvl;
	gm32c.pending_intno = int_no;
	M32C_UpdateIPL();
}

void
_M32C_SetRegFlg(uint16_t flg, uint16_t diff)
{
	if (diff & M32C_FLG_I) {
		if (unlikely(flg & M32C_FLG_I)) {
			gm32c.signals_mask |= M32C_SIG_IRQ;
		} else {
			gm32c.signals_mask &= ~M32C_SIG_IRQ;
		}
		M32C_UpdateSignals();
	}
	if (diff & M32C_FLG_U) {
		if (flg & M32C_FLG_U) {
            {
                CycleCounter_t newTime = CycleCounter_Get(); 
                if (gm32c.currInterruptStart && (newTime - gm32c.currInterruptStart) > gm32c.maxInterruptCycles) {
                    gm32c.maxInterruptCycles = newTime - gm32c.currInterruptStart;
                    //fprintf(stderr, "******** maxInterruptCycles = %" PRIu64 ",  at %08x\n", gm32c.maxInterruptCycles,  M32C_REG_PC);
                }
            }
			gm32c.reg_isp = M32C_REG_SP;
			M32C_REG_SP = gm32c.reg_usp;
		} else {
            gm32c.currInterruptStart = CycleCounter_Get();

			gm32c.reg_usp = M32C_REG_SP;
			M32C_REG_SP = gm32c.reg_isp;
		}
	}
	if (diff & M32C_FLG_BANK) {
		/* 
		 *****************************************************************
		 * Switch bank by syncing current regs to actual regset and
		 * copying the new regset to the working copy.
		 *****************************************************************
		 */
		if (flg & M32C_FLG_BANK) {
			gm32c.banks[0] = gm32c.regs;
			gm32c.regs = gm32c.banks[1];
			//fprintf(stderr,"Bank 1 at %llu\n",CyclesToMicroseconds(CycleCounter_Get()));
		} else {
			gm32c.banks[1] = gm32c.regs;
			gm32c.regs = gm32c.banks[0];
			//fprintf(stderr,"Bank 0 at %llu\n",CyclesToMicroseconds(CycleCounter_Get()));
		}
	}
	if (diff & M32C_FLG_IPL_MSK) {
		M32C_UpdateIPL();
	}
}

void
M32C_SyncRegSets(void)
{
	if (M32C_REG_FLG & M32C_FLG_BANK) {
		gm32c.banks[1] = gm32c.regs;
	} else {
		gm32c.banks[0] = gm32c.regs;
	}
	if (M32C_REG_FLG & M32C_FLG_U) {
		gm32c.reg_usp = M32C_REG_SP;
	} else {
		gm32c.reg_isp = M32C_REG_SP;
	}
}

static int
debugger_getreg(void *clientData, uint8_t * data, uint32_t index, int maxlen)
{
	int i;
	uint32_t value[2];
	int reg_cnt = 0;
	int reg_size = 0;
	M32C_SyncRegSets();
#if 1
	switch (index) {
	    case 0:
		    /* R0 */
		    value[0] = M32C_REG_BANK0(r0);
		    value[1] = M32C_REG_BANK1(r0);
		    reg_cnt = 2;
		    reg_size = 2;
		    break;
	    case 1:
		    /* R1 */
		    value[0] = M32C_REG_BANK0(r1);
		    value[1] = M32C_REG_BANK1(r1);
		    reg_cnt = 2;
		    reg_size = 2;
		    break;
	    case 2:
		    /* R2 */
		    value[0] = M32C_REG_BANK0(r2);
		    value[1] = M32C_REG_BANK1(r2);
		    reg_cnt = 2;
		    reg_size = 2;
		    break;
	    case 3:
		    /* R3 */
		    value[0] = M32C_REG_BANK0(r3);
		    value[1] = M32C_REG_BANK1(r3);
		    reg_cnt = 2;
		    reg_size = 2;
		    break;
	    case 4:
		    /* A0 */
		    value[0] = M32C_REG_BANK0(a0);
		    value[1] = M32C_REG_BANK1(a0);
		    reg_cnt = 2;
		    reg_size = 3;
		    break;
	    case 5:
		    /* A1 */
		    value[0] = M32C_REG_BANK0(a1);
		    value[1] = M32C_REG_BANK1(a1);
		    reg_cnt = 2;
		    reg_size = 3;
		    break;
	    case 6:
		    /* FB */
		    value[0] = M32C_REG_FB;
		    value[1] = M32C_REG_FB;
		    reg_cnt = 2;
		    reg_size = 3;
		    break;
	    case 7:
		    /* SB */
		    value[0] = M32C_REG_SB;
		    value[1] = M32C_REG_SB;
		    reg_cnt = 2;
		    reg_size = 3;
		    break;
	    case 8:
		    /* USP */
		    //value = M32C_REG_USP;
		    value[0] = M32C_REG(usp);
		    reg_cnt = 1;
		    reg_size = 3;
		    break;

	    case 9:
		    /* ISP */
		    reg_cnt = 1;
		    reg_size = 3;
		    value[0] = M32C_REG(isp);
		    break;

	    case 10:
		    /* INTB */
		    value[0] = M32C_REG_INTB;
		    reg_cnt = 1;
		    reg_size = 3;
		    break;

	    case 11:
		    /* PC */
		    value[0] = M32C_REG_PC;
		    reg_cnt = 1;
		    reg_size = 3;
		    break;
	    case 12:
		    /* FLG */
		    value[0] = M32C_REG_FLG;
		    reg_cnt = 1;
		    reg_size = 2;
		    break;
	    case 13:
		    /* SVF */
		    value[0] = M32C_REG(svf);
		    reg_cnt = 1;
		    reg_size = 2;
		    break;
	    case 14:
		    value[0] = M32C_REG(svp);
		    reg_cnt = 1;
		    reg_size = 3;
		    break;
	    case 15:
		    value[0] = M32C_REG(vct);
		    reg_cnt = 1;
		    reg_size = 3;
		    break;
	    case 16:
		    value[0] = M32C_REG(dmd0);
		    value[1] = M32C_REG(dmd1);
		    reg_cnt = 2;
		    reg_size = 1;
		    break;
	    case 17:
		    value[0] = M32C_REG(dct0);
		    value[1] = M32C_REG(dct1);
		    reg_cnt = 2;
		    reg_size = 2;
		    break;
	    case 18:
		    value[0] = M32C_REG(drc0);
		    value[1] = M32C_REG(drc1);
		    reg_cnt = 2;
		    reg_size = 2;
		    break;
	    case 19:
		    value[0] = M32C_REG(dma0);
		    value[1] = M32C_REG(dma1);
		    reg_cnt = 2;
		    reg_size = 3;
		    break;
	    case 20:
		    value[0] = M32C_REG(dsa0);
		    value[1] = M32C_REG(dsa1);
		    reg_cnt = 2;
		    reg_size = 3;
		    break;
	    case 21:
		    value[0] = M32C_REG(dra0);
		    value[1] = M32C_REG(dra1);
		    reg_cnt = 2;
		    reg_size = 3;
		    break;
	}
#endif
	for (i = 0; i < reg_cnt; i++) {
		if (reg_size == 3) {
			*data++ = value[i] & 0xff;
			*data++ = (value[i] >> 8) & 0xff;
			*data++ = (value[i] >> 16) & 0xff;
		} else if (reg_size == 2) {
			*data++ = value[i] & 0xff;
			*data++ = (value[i] >> 8) & 0xff;
		} else if (reg_size == 1) {
			*data++ = value[i] & 0xff;
		}
	}
	return reg_cnt * reg_size;
}

static int
debugger_stop(void *clientData)
{

	gm32c.dbg_state = M32CDBG_STOP;
	M32C_PostSignal(M32C_SIG_DBG);
	fprintf(stderr, "Got stop: msk %08x, sig %08x\n", gm32c.signals_mask, gm32c.signals_raw);
	//exit(1);
	return -1;
}

static int
debugger_cont(void *clientData)
{
	gm32c.dbg_state = M32CDBG_RUNNING;
	/* Should only be called if there are no breakpoints */
	M32C_UnpostSignal(M32C_SIG_DBG);
	return 0;
}

static int
debugger_step(void *clientData, uint64_t addr, int use_addr)
{
	if (use_addr) {
		M32C_REG_PC = addr;
	}
	M32C_PostSignal(M32C_SIG_DBG);
	gm32c.dbg_steps = 1;
	gm32c.dbg_state = M32CDBG_STEP;
	return -1;
}

static Dbg_TargetStat
debugger_get_status(void *clientData)
{
	if ((gm32c.dbg_state == M32CDBG_STOP) ||
	  (gm32c.dbg_state == M32CDBG_STOPPED)) {
		return DbgStat_SIGINT;
	} else if (gm32c.dbg_state == M32CDBG_RUNNING) {
		return DbgStat_RUNNING;
	} else {
		fprintf(stderr,"Target in state %d\n",gm32c.dbg_state);
		return -1;
	}
}

/*
 ****************************************************************************
 * get_bkpt_ins
 *      returns the operation code of the breakpoint instruction.
 *      Needed by the debugger to insert breakpoints
 ****************************************************************************
 */
static void
debugger_get_bkpt_ins(void *clientData, uint8_t * ins, uint64_t addr, int len)
{
	if (len == 1) {
		ins[0] = 0x8;
	} else {

	}
}

static ssize_t
debugger_getmem(void *clientData, uint8_t * data, uint64_t addr, uint32_t len)
{
	int count;
	count = 0;
	for (; len > 0; len--, count++, data++) {
		uint8_t value = M32C_Read8(addr + count);
		*data = value;
	}
	return count;
}

static ssize_t
debugger_setmem(void *clientData, const uint8_t * data, uint64_t addr, uint32_t len)
{
	int count = 0;
	for (; len > 0; len--, count++, data++) {
		uint8_t value = *data;
		M32C_Write8(value, addr + count);
	}
	return count;
}
static uint32_t
nullptr_read(void *clientData, uint32_t address, int rqlen)
{
    fflush(stdout);
    fprintf(stderr, "\n\nReading from NULL address at PC: 0x%06x\n", M32C_REG_PC);
    exit(1);
}

static void 
nullptr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
    fflush(stdout);
    fprintf(stderr, "\n\nWriting NULL address at PC: 0x%06x\n", M32C_REG_PC);
    exit(1);
}

static uint32_t
prcr_read(void *clientData, uint32_t address, int rqlen)
{
	M32C_Cpu *cpu = clientData;
	return cpu->regPRCR;
}

static void
prcr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	M32C_Cpu *cpu = clientData;
	int i;
	cpu->regPRCR = value & 0xf;
	for (i = 0; i < 4; i++) {
		if (value & (1 << i)) {
			SigNode_Set(cpu->sigPRC[i], SIG_HIGH);
		} else {
			SigNode_Set(cpu->sigPRC[i], SIG_LOW);
		}
	}
}

static uint32_t
rlvl_read(void *clientData, uint32_t address, int rqlen)
{
	M32C_Cpu *cpu = clientData;
	return cpu->regRLVL;
}

static void
rlvl_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	M32C_Cpu *cpu = clientData;
	cpu->regRLVL = value & 0x2f;
}

M32C_Cpu *
M32C_CpuNew(const char *instancename, BusDevice * intco)
{
	M32C_Cpu *cpu = &gm32c;
	uint32_t cpu_clock = 24000000;
	int i;
	M32CInstructions_Init();
	memset(cpu, 0, sizeof(M32C_Cpu));
	gm32c.intco = intco;
	M32C_IDecoderNew();
	gm32c.bitindex = -1;
	Config_ReadUInt32(&cpu_clock, "global", "cpu_clock");
	CycleTimers_Init(instancename, cpu_clock);

	cpu->throttle = Throttle_New(instancename);
	cpu->dbg_state = M32CDBG_RUNNING;
	cpu->dbgops.stop = debugger_stop;
	cpu->dbgops.cont = debugger_cont;
	cpu->dbgops.step = debugger_step;
	cpu->dbgops.get_bkpt_ins = debugger_get_bkpt_ins;
	cpu->dbgops.get_status = debugger_get_status;
	cpu->dbgops.getmem = debugger_getmem;
	cpu->dbgops.setmem = debugger_setmem;
	cpu->dbgops.getreg = debugger_getreg;
#if 0
	cpu->dbgops.setreg = debugger_setreg;
#endif
	cpu->debugger = Debugger_New(&cpu->dbgops, cpu);
	cpu->signals_mask = M32C_SIG_DBG | M32C_SIG_DELETE_INDEX | M32C_SIG_INHIBIT_IRQ;
	for (i = 0; i < 4; i++) {
		cpu->sigPRC[i] = SigNode_New("%s.prc%u", instancename, i);
		if (!cpu->sigPRC[i]) {
			fprintf(stderr, "Can not create PRC signal %u\n", i);
			exit(1);
		}
		SigNode_Set(cpu->sigPRC[i], SIG_LOW);
	}
	IOH_New8(REG_PRCR, prcr_read, prcr_write, cpu);
	IOH_New8(REG_RLVL, rlvl_read, rlvl_write, cpu);
	IOH_New32(REG_NULLPTR, nullptr_read, nullptr_write, cpu);

	cpu->readEntryPa = 0xffffffff;
	cpu->ifetchEntryPa = 0xffffffff;
	cpu->writeEntryPa = 0xffffffff;
	return cpu;
}

void
M32C_InvalidateHvaCache(void)
{
	gm32c.readEntryPa = 0xffffffff;
	gm32c.ifetchEntryPa = 0xffffffff;
	gm32c.writeEntryPa = 0xffffffff;
}

static void
M32C_Interrupt(void)
{
	uint32_t irq_no;
	uint32_t intb;
	uint16_t flg = M32C_REG_FLG;
	uint16_t newflg;
	irq_no = gm32c.pending_intno;
	//fprintf(stderr,"Handling pending int %d\n",irq_no);
	newflg = flg & ~(M32C_FLG_I | M32C_FLG_D | M32C_FLG_U | M32C_FLG_IPL_MSK);
	newflg = newflg | (gm32c.pending_ilvl << M32C_FLG_IPL_SHIFT);
	if ((gm32c.pending_ilvl != 7) || ((gm32c.regRLVL & RLVL_FSIT) == 0)) {
		M32C_SET_REG_FLG(newflg);
		M32C_REG_SP -= 2;
		M32C_Write16(flg, M32C_REG_SP);
		M32C_REG_SP -= 2;
		M32C_Write16((M32C_REG_PC >> 16) & 0xff, M32C_REG_SP);
		M32C_REG_SP -= 2;
		M32C_Write16(M32C_REG_PC & 0xffff, M32C_REG_SP);
		intb = M32C_REG_INTB;
		M32C_REG_PC = M32C_Read24(intb + (irq_no << 2));
		CycleCounter += 7;
	} else {
		if (gm32c.regRLVL & RLVL_DMACIISEL) {
			fprintf(stderr, "\nDMA mode for interrupt level 7 not implemented\n");
			exit(1);
		} else {
			M32C_SET_REG_FLG(newflg);
			M32C_REG(svf) = flg;
			M32C_REG(svp) = M32C_REG_PC;
			M32C_REG_PC = M32C_REG(vct);
			CycleCounter += 5;
		}
	}
	M32C_AckIrq(gm32c.intco, gm32c.pending_intno);
#if 0
	fprintf(stderr, "Doing interrupt %d, INTB is %06x, PC is %06x\n",
		irq_no, intb, M32C_REG_PC);
#endif
}

static void
Do_Debug(void)
{
	if (gm32c.dbg_state == M32CDBG_RUNNING) {
		fprintf(stderr, "Debug mode is off, should not be called\n");
	} else if (gm32c.dbg_state == M32CDBG_STEP) {
		if (gm32c.dbg_steps == 0) {
			gm32c.dbg_state = M32CDBG_STOPPED;
			if (gm32c.debugger) {
				Debugger_Notify(gm32c.debugger, DbgStat_SIGTRAP);
			}
			M32C_RestartIdecoder();
		} else {
			gm32c.dbg_steps--;
		}
	} else if (gm32c.dbg_state == M32CDBG_STOP) {
		gm32c.dbg_state = M32CDBG_STOPPED;
		if (gm32c.debugger) {
			Debugger_Notify(gm32c.debugger, DbgStat_SIGINT);
		}
		M32C_RestartIdecoder();
	} else if (gm32c.dbg_state == M32CDBG_BREAK) {
		if (gm32c.debugger) {
			if (Debugger_Notify(gm32c.debugger, DbgStat_SIGTRAP) > 0) {
				gm32c.dbg_state = M32CDBG_STOPPED;
				M32C_RestartIdecoder();
			}	/* Else no debugger session open */
		} else {
			//M32C Exception break
			gm32c.dbg_state = M32CDBG_RUNNING;
		}
	} else {
		fprintf(stderr, "Unknown restart signal reason %d\n", gm32c.dbg_state);
	}
}

static inline void
CheckSignals()
{
	if (gm32c.signals) {
		//fprintf(stderr,"Signals %08x\n",gm32c.signals);
		if (gm32c.signals & M32C_SIG_DBG) {
			fprintf(stderr, "at %08x %" PRIu64 "\n", M32C_REG_PC,
				CycleCounter_Get());
		}
		if ((gm32c.signals & M32C_SIG_DELETE_INDEX) &&
			  (!(gm32c.signals & M32C_SIG_INHIBIT_IRQ))) {
			gm32c.signals_raw &= ~M32C_SIG_DELETE_INDEX;;
			gm32c.signals &= ~M32C_SIG_DELETE_INDEX;;
			gm32c.index_src = gm32c.index_dst = 0;
			gm32c.bitindex = -1;
#if 0
			if (gm32c.index_src_used == 0) {
				M32C_Instruction *instr;
				instr = M32C_InstructionFind(ICODE);
				fprintf(stderr, "Deleting unused index %s\n", instr->name);

			}
			if (gm32c.index_dst_used == 0) {
				M32C_Instruction *instr;
				instr = M32C_InstructionFind(ICODE);
				fprintf(stderr, "Deleting unused index %s\n", instr->name);

			}
#endif
		}
		if (gm32c.signals & M32C_SIG_INHIBIT_IRQ) {
			gm32c.signals_raw &= ~M32C_SIG_INHIBIT_IRQ;
			gm32c.signals &= ~M32C_SIG_INHIBIT_IRQ;
		} else if (gm32c.signals & M32C_SIG_IRQ) {
			M32C_Interrupt();
		}
	}
	if (unlikely(gm32c.signals & M32C_SIG_DBG)) {
		Do_Debug();
	}
#if 0
	if (unlikely(gm32c.cpu_signals & M32C_SIG_RESTART_IDEC)) {
		M32C_RestartIdecoder();
	}
#endif
}

static struct timeval tv_start;
static void
CalcSpeedProc(void *eventData)
{
	struct timeval tv_now;
	unsigned int time;
	gettimeofday(&tv_now, NULL);
	time = (tv_now.tv_sec - tv_start.tv_sec) * 1000
	    + ((tv_now.tv_usec - tv_start.tv_usec) / 1000);
//	fprintf(stderr, "\nSimulator speed %d kHz\n", (int)(CycleCounter_Get() / time));
#ifdef PROFILE
	//exit(0);
#endif

}

static CycleTimer calcSpeedTimer;

static void
M32C_PrintIDCode()
{
	uint8_t idcode[7];
	idcode[0] = Bus_Read8(0xFFFFDF);
	idcode[1] = Bus_Read8(0xFFFFE3);
	idcode[2] = Bus_Read8(0xFFFFEB);
	idcode[3] = Bus_Read8(0xFFFFEF);
	idcode[4] = Bus_Read8(0xFFFFF3);
	idcode[5] = Bus_Read8(0xFFFFF7);
	idcode[6] = Bus_Read8(0xFFFFFB);
	fprintf(stderr, "M32C ID Code is %02x%02x%02x%02x%02x%02x%02x\n",
		idcode[0], idcode[1], idcode[2], idcode[3], idcode[4], idcode[5], idcode[6]);
}

uint32_t
_M32C_Read32(uint32_t addr)
{
	uint8_t *hva;
	hva = Bus_GetHVARead(addr);
	if (hva == NULL) {
		return IO_Read32(addr);
	}
	gm32c.readEntryHva = hva - (addr & 0x3ff);
	gm32c.readEntryPa = addr & 0xfffffc00;
	return HMemRead32(hva);
}

uint16_t
_M32C_Read16(uint32_t addr)
{
	uint8_t *hva;
	hva = Bus_GetHVARead(addr);
	if (hva == NULL) {
		return IO_Read16(addr);
	}
	gm32c.readEntryHva = hva - (addr & 0x3ff);
	gm32c.readEntryPa = addr & 0xfffffc00;
	return HMemRead16(hva);
}

uint8_t
_M32C_Read8(uint32_t addr)
{
	uint8_t *hva;
	hva = Bus_GetHVARead(addr);
	if (hva == NULL) {
		return IO_Read8(addr);
	}
	gm32c.readEntryHva = hva - (addr & 0x3ff);
	gm32c.readEntryPa = addr & 0xfffffc00;
	return HMemRead8(hva);
}

void
_M32C_Write8(uint8_t value, uint32_t addr)
{
	uint8_t *hva;
	hva = Bus_GetHVAWrite(addr);
	if (hva == NULL) {
		return IO_Write8(value, addr);
	}
	gm32c.writeEntryHva = hva - (addr & 0x3ff);
	gm32c.writeEntryPa = addr & 0xfffffc00;
	return HMemWrite8(value, hva);
}

void
_M32C_Write16(uint16_t value, uint32_t addr)
{
	uint8_t *hva;
	hva = Bus_GetHVAWrite(addr);
	if (hva == NULL) {
		return IO_Write16(value, addr);
	}
	gm32c.writeEntryHva = hva - (addr & 0x3ff);
	gm32c.writeEntryPa = addr & 0xfffffc00;
	return HMemWrite16(value, hva);
}

void
_M32C_Write32(uint32_t value, uint32_t addr)
{
	uint8_t *hva;
	hva = Bus_GetHVAWrite(addr);
	if (hva == NULL) {
		return IO_Write32(value, addr);
	}
	gm32c.writeEntryHva = hva - (addr & 0x3ff);
	gm32c.writeEntryPa = addr & 0xfffffc00;
	return HMemWrite32(value, hva);
}

__UNUSED__ static void
M32C_DumpRegisters(void)
{
	fprintf(stderr,"R0: %04x   R1: %04x   R2: %04x R3: %04x\n",
		M32C_REG_R0,M32C_REG_R1,M32C_REG_R2,M32C_REG_R3);
	fprintf(stderr,"A0: %06x A1: %06x\n",
		M32C_REG_A0,M32C_REG_A1);
}

void
M32C_Run(void)
{
	M32C_Cpu *m32 = &gm32c;
	uint32_t startaddr;
	uint32_t dbgwait;

	gettimeofday(&tv_start, NULL);
	CycleTimer_Add(&calcSpeedTimer, 200000000, CalcSpeedProc, NULL);

	if (Config_ReadUInt32(&startaddr, "global", "start_address") < 0) {
		startaddr = M32C_Read24(M32C_VECTOR_RESET);
	}
	if (Config_ReadUInt32(&dbgwait, "global", "dbgwait") < 0) {
		dbgwait = 0;
	}
	M32C_REG_PC = startaddr;
	M32C_PrintIDCode();
	if (dbgwait) {
		fprintf(stderr, "CPU is waiting for debugger connection at %08x\n", startaddr);
		gm32c.dbg_state = M32CDBG_STOPPED;
		M32C_PostSignal(M32C_SIG_DBG);
	} else {
		fprintf(stderr, "Starting CPU at 0x%06x\n", M32C_REG_PC);
	}
	setjmp(m32->restart_idec_jump);
	while (m32->dbg_state == M32CDBG_STOPPED) {
		struct timespec tout;
		tout.tv_nsec = 0;
		tout.tv_sec = 10000;
		FIO_WaitEventTimeout(&tout);
	}
	while (1) {
		CheckSignals();
		CycleTimers_Check();
		ICODE = M32C_IFetch(M32C_REG_PC);
		INSTR = M32C_InstructionFind(ICODE);
		M32C_REG_PC += INSTR->len;
		INSTR->proc();
        //fprintf(stderr,"PC: %08x\n",M32C_REG_PC);
//              CycleCounter += INSTR->cycles + 1;

		if ((M32C_REG_PC & 1) && !(INSTR->len & 1)) {
			CycleCounter += INSTR->cycles + 1;
		} else {
			CycleCounter += INSTR->cycles;
		}

		CheckSignals();
		CycleTimers_Check();
		ICODE = M32C_IFetch(M32C_REG_PC);
		INSTR = M32C_InstructionFind(ICODE);
		M32C_REG_PC += INSTR->len;
        //fprintf(stderr,"PC: %08x\n",M32C_REG_PC);
		INSTR->proc();
		//CycleCounter += INSTR->cycles;
#if 1
		if ((M32C_REG_PC & 1) && !(INSTR->len & 1)) {
			CycleCounter += INSTR->cycles + 1;
		} else {
			CycleCounter += INSTR->cycles;
		}
#endif
#if 0

		if (!(M32C_REG_PC & 15)) {
			CycleCounter += INSTR->cycles + 1;
		} else {
			CycleCounter += INSTR->cycles;
		}
		//fprintf(stdout,"n %06x s %x\n",M32C_REG_PC,gm32c.signals);
#endif
	}
}

static void
NoSched_Timeout(void *eventData)
{
	fprintf(stderr, "No sched at %08x\n", M32C_REG_PC);
}

static CycleTimer noSchedTimer;

void
M32C_Break(void)
{
	static CycleCounter_t lastBreak;
	uint32_t address;
	CycleCounter_t diff;
	diff = CycleCounter_Get() - lastBreak;
	lastBreak = CycleCounter_Get();
	//CycleTimer_Mod(&noSchedTimer, 24000);
	//fprintf(stdout, "Pause of %" PRIu64 " usec\n", CyclesToMicroseconds(diff));
	fprintf(stdout, "Pause of %" PRIu64 " nsec\n", CyclesToNanoseconds(diff));
    //exit(1);
	return;
//	fprintf(stdout, "Pause of %" PRIu64 " usec\n", CyclesToMicroseconds(diff));
	if (diff > MillisecondsToCycles(1)) {
	//fprintf(stderr,"SP %08x\n",M32C_REG_SP);
	//return;
	}
//	fprintf(stdout, "Brk at %08x time %" PRIu64 " diff %" PRIu64"\n", M32C_REG_PC, CycleCounter_Get(),diff);
	//address = M32C_REG_R0 | (M32C_REG_R2 << 16);
	//address = M32C_REG_A0; 
	//address = M32C_REG_R1 | (M32C_REG_R3 << 16);
	M32C_DumpRegisters();
	//fprintf(stdout, "Addr %06x\n",address);	
	//fprintf(stdout, "%c\n",M32C_Read8(address));	
	//return;
	gm32c.dbg_state = M32CDBG_BREAK;
	//M32C_REG_PC = M32C_REG_PC - 1;
	M32C_PostSignal(M32C_SIG_DBG);
	M32C_RestartIdecoder();
}

INITIALIZER(cpu_init)
{
	CycleTimer_Init(&noSchedTimer, NoSched_Timeout, NULL);
}
