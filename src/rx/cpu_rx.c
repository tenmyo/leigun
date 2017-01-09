/*
 **********************************************************************************************
 * Renesas RX CPU simulation
 *
 * Copyright 2012 Jochen Karrer. All rights reserved.
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

#include <unistd.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <setjmp.h>
#include "cpu_rx.h"
#include "idecode_rx.h"
#include "cycletimer.h"
#include "softfloat.h"
#include "compiler_extensions.h"
#include "instructions_rx.h"
#include "configfile.h"

RX_Cpu g_RXCpu;

/*
 ***********************************************************************************
 * \fn static void RX_FloatExceptionProc(void *eventData,uint32_t exception)
 * Callback handler called by the floating point library when an exception occurs.
 * It translates the exception number from the Floating point library into
 * a register value in the FPSW status word.
 ***********************************************************************************
 */
static void
RX_FloatExceptionCallback(void *eventData, uint32_t exception)
{
	RX_Cpu *rx = eventData;
	switch (exception) {
	    case SFE_INV_OP:
		    rx->regFPSW |= FPSW_CV;
		    if (!(rx->regFPSW & FPSW_EV)) {
			    rx->regFPSW |= FPSW_FV | FPSW_FS;
		    }
		    break;

	    case SFE_DIV_ZERO:
		    rx->regFPSW |= FPSW_CZ;
		    if (!(rx->regFPSW & FPSW_EZ)) {
			    rx->regFPSW |= FPSW_FZ | FPSW_FS;
		    }
		    break;

	    case SFE_OVERFLOW:
		    rx->regFPSW |= FPSW_CO;
		    if (!(rx->regFPSW & FPSW_EO)) {
			    rx->regFPSW |= FPSW_FO | FPSW_FS;
		    }
		    break;

	    case SFE_UNDERFLOW:
		    rx->regFPSW |= FPSW_CU;
		    if (!(rx->regFPSW & FPSW_EU)) {
			    rx->regFPSW |= FPSW_FU | FPSW_FS;
		    }
		    break;

	    case SFE_INEXACT:
		    rx->regFPSW |= FPSW_CX;
		    if (!(rx->regFPSW & FPSW_EX)) {
			    rx->regFPSW |= FPSW_FX | FPSW_FS;
		    }
		    break;
	}
}

void
RX_Exception(uint32_t vector_addr)
{
	uint32_t Sp;
	uint32_t saved_psw;
	saved_psw = RX_REG_PSW;
	RX_SET_REG_PSW(RX_REG_PSW & ~(PSW_U | PSW_I | PSW_PM));
	Sp = RX_ReadReg(0);
	Sp -= 4;
	RX_Write32(saved_psw, Sp);
	Sp -= 4;
	RX_WriteReg(Sp, 0);
	RX_Write32(RX_REG_PC, Sp);
	RX_REG_PC = RX_Read32(vector_addr);
}

void
RX_Trap(unsigned int trap_nr)
{
	uint32_t vector_addr;
	vector_addr = RX_REG_INTB + (trap_nr << 2);
	RX_Exception(vector_addr);
}

static void
RX_FastInterrupt(void)
{
#if 0
    g_RXCpu.dbg_state = RXDBG_STOP;
    RX_SigDebugMode(1);
#endif

        fprintf(stderr,"Calling FINTV %08x at %lu\n", RX_REG_FINTV, CycleCounter_Get());
	RX_REG_BPSW = RX_REG_PSW;
	RX_SET_REG_PSW((RX_REG_PSW & ~(PSW_U | PSW_I | PSW_PM | PSW_IPL_MSK)) |
		       (15 << PSW_IPL_SHIFT));
	RX_REG_BPC = RX_REG_PC;
	RX_REG_PC = RX_REG_FINTV;
    SigNode_Set(g_RXCpu.sigIrqAck, SIG_LOW);
    SigNode_Set(g_RXCpu.sigIrqAck, SIG_HIGH);
    CycleCounter += 5;
}

static void
RX_Interrupt(unsigned int irq_no, unsigned int ipl)
{
	uint32_t vector_addr;
	//fprintf(stderr,"RX irq %d level %d\n",irq_no,ipl);
    uint32_t Sp;
    vector_addr = RX_REG_INTB + (irq_no << 2);
    uint32_t saved_psw = RX_REG_PSW;
    if (unlikely(ipl == 255)) {
        RX_FastInterrupt();
    } else {
        RX_SET_REG_PSW((RX_REG_PSW & ~(PSW_U | PSW_I | PSW_PM | PSW_IPL_MSK)) |
                   (ipl << PSW_IPL_SHIFT));
        Sp = RX_ReadReg(0);
        Sp -= 4;
        RX_Write32(saved_psw, Sp);
        Sp -= 4;
        RX_WriteReg(Sp, 0);
        RX_Write32(RX_REG_PC, Sp);
        RX_REG_PC = RX_Read32(vector_addr);
        //fprintf(stderr,"PC set to %08x, SP %08x\n",RX_REG_PC,Sp);
        /* This acks the interrupt if */
        SigNode_Set(g_RXCpu.sigIrqAck, SIG_LOW);
        SigNode_Set(g_RXCpu.sigIrqAck, SIG_HIGH);
    }
}



/**
 ***************************************************************************
 * Interface proc for the interrupt controller. Tell the CPU about the
 * highest IPL and its irqNo
 ***************************************************************************
 */
void
RX_PostInterrupt(uint8_t ipl, uint8_t irqNo)
{
	RX_Cpu *rx = &g_RXCpu;
	rx->pendingIpl = ipl;
	rx->pendingIrqNo = irqNo;
	//fprintf(stderr,"pending IPL %d, IRQ %d, CPU IPL %08x %08lld\n",ipl,irqNo, (RX_REG_PSW >> PSW_IPL_SHIFT) & 15, CycleCounter_Get() );
	RX_UpdateIrqSignal();
}

void
RX_NMInterrupt(unsigned int irq_no, unsigned int ipl)
{
	uint32_t Sp;
	uint32_t saved_psw;
	saved_psw = RX_REG_PSW;
	RX_SET_REG_PSW((RX_REG_PSW & ~(PSW_U | PSW_I | PSW_PM | PSW_IPL_MSK)) |
		       (15 << PSW_IPL_SHIFT));
	Sp = RX_ReadReg(0);
	Sp -= 4;
	RX_Write32(saved_psw, Sp);
	Sp -= 4;
	RX_WriteReg(Sp, 0);
	RX_Write32(RX_REG_PC, Sp);
	RX_REG_PC = RX_Read32(0xfffffff8);
}

static void
debugger_setreg(void *clientData, const uint8_t * data, uint32_t index, int len)
{
	if (len < 4) {
		return;
	}
	switch (index) {
	    case 0:
	    case 1:
	    case 2:
	    case 3:
	    case 4:
	    case 5:
	    case 6:
	    case 7:
	    case 8:
	    case 9:
	    case 10:
	    case 11:
	    case 12:
	    case 13:
	    case 14:
	    case 15:
		    RX_WriteReg(*(uint32_t *) data, index);
		    break;

	    case 16:
		    RX_SET_REG_USP(*(uint32_t *) data);
		    break;

	    case 17:
		    RX_SET_REG_ISP(*(uint32_t *) data);
		    break;

	    case 18:
		    RX_SET_REG_PSW(*(uint32_t *) data);
		    break;

	    case 19:
		    RX_REG_PC = *(uint32_t *) data;
		    break;

	    case 20:
		    RX_REG_INTB = *(uint32_t *) data;
		    break;

	    case 21:
		    RX_REG_BPSW = *(uint32_t *) data;
		    break;

	    case 22:
		    RX_REG_BPC = *(uint32_t *) data;
		    break;

	    case 23:
		    RX_REG_FINTV = *(uint32_t *) data;
		    break;

	    case 24:
		    RX_REG_FPSW = *(uint32_t *) data;
		    break;

	    case 25:
		    if (len < 8) {
			    fprintf(stderr, "SetReg Acc should be 8 bytes\n");
			    break;
		    }
		    RX_WriteACC(*(uint64_t *) data);
		    break;
	    default:
		    fprintf(stderr, "setreg: Illegal register %u\n", index);
		    break;
	}
	fprintf(stderr, "Debugger Setreg %d done\n", index);
	return;
}

static int
debugger_getreg(void *clientData, uint8_t * data, uint32_t index, int maxlen)
{
	if (maxlen < 4) {
		return -EINVAL;
	}
	switch (index) {
	    case 0:
	    case 1:
	    case 2:
	    case 3:
	    case 4:
	    case 5:
	    case 6:
	    case 7:
	    case 8:
	    case 9:
	    case 10:
	    case 11:
	    case 12:
	    case 13:
	    case 14:
	    case 15:
		    *(uint32_t *) data = RX_ReadReg(index);
		    return 4;
	    case 16:
		    *(uint32_t *) data = RX_GET_REG_USP();
		    return 4;
	    case 17:
		    *(uint32_t *) data = RX_GET_REG_ISP();
		    return 4;

	    case 18:
		    *(uint32_t *) data = RX_REG_PSW;
		    return 4;

	    case 19:
		    *(uint32_t *) data = RX_REG_PC;
		    fprintf(stderr, "Debugger get PC 0x%08x\n", RX_REG_PC);
		    return 4;

	    case 20:
		    *(uint32_t *) data = RX_REG_INTB;
		    return 4;

	    case 21:
		    *(uint32_t *) data = RX_REG_BPSW;
		    return 4;
	    case 22:
		    *(uint32_t *) data = RX_REG_BPC;
		    return 4;
	    case 23:
		    *(uint32_t *) data = RX_REG_FINTV;
		    return 4;

	    case 24:
		    *(uint32_t *) data = RX_REG_FPSW;
		    return 4;

	    case 25:
		    if (maxlen < 8) {
			    return -EINVAL;
		    }
		    *(uint64_t *) data = RX_ReadACC();
		    return 8;
	}
	return -EINVAL;
}

/**
 */
static int
debugger_stop(void *clientData)
{
	fprintf(stderr, "Debugger: Stop the CPU\n");
	g_RXCpu.dbg_state = RXDBG_STOP;
	RX_SigDebugMode(1);
	return -1;
}

static int
debugger_cont(void *clientData)
{
	fprintf(stderr, "Debugger: Continue\n");
	g_RXCpu.dbg_state = RXDBG_RUNNING;
	RX_SigDebugMode(0);
	return 0;
}

static int
debugger_step(void *clientData, uint64_t addr, int use_addr)
{

	fprintf(stderr, "Debugger: Step\n");
	if (use_addr) {
		RX_REG_PC = addr;
	}
	g_RXCpu.dbg_steps = 1;
	g_RXCpu.dbg_state = RXDBG_STEP;
	return -1;
}

static Dbg_TargetStat
debugger_get_status(void *clientData)
{
	if (g_RXCpu.dbg_state == RXDBG_STOPPED) {
		return DbgStat_SIGINT;
	} else if (g_RXCpu.dbg_state == RXDBG_RUNNING) {
		return DbgStat_RUNNING;
	} else {
		return -1;
	}
}

static ssize_t
debugger_getmem(void *clientData, uint8_t * data, uint64_t addr, uint32_t count)
{
	int i;
	for (i = 0; i < count; i++) {
		data[i] = RX_Read8(addr + i);
	}
	return count;
}

/**
 *******************************************************************
 *******************************************************************
 */
static ssize_t
debugger_setmem(void *clientData, const uint8_t * data, uint64_t addr, uint32_t count)
{
	int i;
	for (i = 0; i < count; i++) {
		RX_Write8(data[i], addr + i);
	}
	return count;
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
		/* RX brk instruction code is 0x00 */
		ins[0] = 0;
	} else {
		fprintf(stderr, "get_bkpt_ins: no bkpt instruction with len %d\n", len);
	}
}

/**
 ***********************************************************************
 * \fn RX_Cpu * RX_CpuNew(const char *instancename)
 ***********************************************************************
 */
RX_Cpu *
RX_CpuNew(const char *instancename)
{
	RX_Cpu *rx = &g_RXCpu;
	uint32_t cpu_clock = 123456789;
	memset(rx, 0, sizeof(RX_Cpu));
	RX_IDecoderNew();
	RXInstructions_Init();
	rx->floatContext = SFloat_New();
	rx->sigIrqAck = SigNode_New("%s.irqAck", instancename);
	if (!rx->sigIrqAck) {
		fprintf(stderr, "%s: Can not create IrqAck signal line\n", instancename);
		exit(1);
	}
	SigNode_Set(rx->sigIrqAck, SIG_HIGH);
	SFloat_SetExceptionProc(rx->floatContext, RX_FloatExceptionCallback, rx);
	Config_ReadUInt32(&cpu_clock, "global", "cpu_clock");
	CycleTimers_Init(instancename, cpu_clock);
	rx->dbgops.getreg = debugger_getreg;
	rx->dbgops.setreg = debugger_setreg;
	rx->dbgops.stop = debugger_stop;
	rx->dbgops.cont = debugger_cont;
	rx->dbgops.get_status = debugger_get_status;
	rx->dbgops.getmem = debugger_getmem;
	rx->dbgops.setmem = debugger_setmem;
	rx->dbgops.step = debugger_step;
	rx->dbgops.get_bkpt_ins = debugger_get_bkpt_ins;
	rx->debugger = Debugger_New(&rx->dbgops, rx);
#if 0
	rx->dbg_state = AVRDBG_RUNNING;
#endif
	rx->throttle = Throttle_New(instancename);
	return rx;
}

static void
Do_Debug(void)
{
	RX_Cpu *rx = &g_RXCpu;
	if (rx->dbg_state == RXDBG_RUNNING) {
		fprintf(stderr, "Debug mode is off, should not be called\n");
	} else if (rx->dbg_state == RXDBG_STEP) {
		if (rx->dbg_steps == 0) {
			rx->dbg_state = RXDBG_STOPPED;
			if (rx->debugger) {
				Debugger_Notify(rx->debugger, DbgStat_SIGTRAP);
			}
			RX_RestartIdecoder();
		} else {
			rx->dbg_steps--;
		}
	} else if (rx->dbg_state == RXDBG_STOP) {
		rx->dbg_state = RXDBG_STOPPED;
		if (rx->debugger) {
			Debugger_Notify(rx->debugger, DbgStat_SIGINT);
		}
		RX_RestartIdecoder();
	} else if (rx->dbg_state == RXDBG_BREAK) {
		if (rx->debugger) {
			if (Debugger_Notify(rx->debugger, DbgStat_SIGTRAP) > 0) {
				rx->dbg_state = RXDBG_STOPPED;
				RX_RestartIdecoder();
			}	/* Else no debugger session open */
		} else {
			/* should now do a normal break Exception. */
			rx->dbg_state = RXDBG_RUNNING;
		}
	} else {
		fprintf(stderr, "Unknown restart signal reason %d\n", rx->dbg_state);
	}
}

/**
 ******************************************************************************
 * Check for pending Signals
 ******************************************************************************
 */
static inline void
CheckSignals()
{
	if (g_RXCpu.signals) {
		if (likely(g_RXCpu.signals & RX_SIG_IRQ)) {
              //fprintf(stderr, "SigIrq\n");
			RX_Interrupt(g_RXCpu.pendingIrqNo, g_RXCpu.pendingIpl);
		}
#if 0
		if (g_RXCpu.signals & RX_SIG_FIRQ) {
			RX_FastInterrupt(g_RXCpu.pendingFirqNo);
		}
#endif
		if (unlikely(g_RXCpu.signals & RX_SIG_DBG)) {
			Do_Debug();
		}
	}
}

bool
RX_CheckForFloatingPointException(void)
{
	RX_Cpu *rx = &g_RXCpu;
	uint32_t exceptions = (rx->regFPSW << 8) & rx->regFPSW;
	if (exceptions & (FPSW_EV | FPSW_EO | FPSW_EZ | FPSW_EU | FPSW_EX)) {
		RX_FloatingPointException();
		return true;
	}
	return false;
}

bool
RX_CheckForUnimplementedException(void)
{
	RX_Cpu *rx = &g_RXCpu;
	if (rx->regFPSW & FPSW_DN) {
		return false;
	}
	if ((rx->regFPSW & FPSW_CE)) {
		RX_FloatingPointException();
		return true;
	} else {
		return false;
	}
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
	fprintf(stderr, "\nSimulator speed %d kHz\n", (int)(CycleCounter_Get() / time));
#ifdef PROFILE
	exit(0);
#endif

}

static CycleTimer calcSpeedTimer;

static void
RX_PrintIDCode()
{
	int i;
	uint8_t idcode[16];
	bool warn = false;
	bool enabled = false;
	uint8_t control_code;

	fprintf(stderr, "RX ID Code is: ");
	control_code = Bus_Read8(0xffffffa3);
	if ((control_code == 0x45) || (control_code == 0x52)) {
		enabled = true;
		warn = true;
		fprintf(stderr, "enabled(0x%02x) ", control_code);
	} else {
		fprintf(stderr, "disabled(0x%02x) ", control_code);
	}
	for (i = 1; i < 16; i++) {
		idcode[i] = Bus_Read8((0xffffffa0 + i) ^ 3);
		fprintf(stderr, "%02x", idcode[i]);
		if (i != 15) {
			fprintf(stderr, ":");
		}
		if ((idcode[i] != 0xff) && (idcode[i] != 0)) {
			warn = 1;
		}
	}
	fprintf(stderr, "\n");
	if (warn) {
		fprintf(stderr, "Warning, non 0xff IDCODE\n");
		sleep(3);
	}
}

void
RX_Run(void)
{
	//RX_Instruction *instr;
	RX_Cpu *rx = &g_RXCpu;
	uint32_t startvector = 0xfffffffc;
	uint32_t pc;
	uint32_t dbgwait;
	RX_PrintIDCode();
	if (Config_ReadUInt32(&pc, "global", "start_address") < 0) {
		pc = RX_Read32(startvector);
	}
	RX_REG_PC = pc;
	if (Config_ReadUInt32(&dbgwait, "global", "dbgwait") < 0) {
		dbgwait = 0;
	}
	if (dbgwait) {
		fprintf(stderr, "CPU is waiting for debugger connection at %08x\n", pc);
		rx->dbg_state = RXDBG_STOPPED;
		RX_SigDebugMode(1);
	} else {
		fprintf(stderr, "Starting CPU at 0x%08x\n", pc);
	}
	gettimeofday(&tv_start, NULL);
	CycleTimer_Add(&calcSpeedTimer, 2000000000, CalcSpeedProc, NULL);
	setjmp(rx->restart_idec_jump);
	while (rx->dbg_state == RXDBG_STOPPED) {
		struct timespec tout;
		tout.tv_nsec = 0;
		tout.tv_sec = 10000;
		FIO_WaitEventTimeout(&tout);
	}
	while (1) {
        static bool debug = false;
		CheckSignals();
		CycleTimers_Check();
		CycleCounter += 2;
		ICODE = RX_IFetch(RX_REG_PC);
		INSTR = RX_InstructionFind();
		//usleep(100);
#if 0
		if((RX_REG_PC == 0xfff44228)) {
            debug = true;
        }
        if (debug) {
		    fprintf(stderr, "%08x: %08x %s\n", RX_REG_PC, ICODE, INSTR->name);
        }
#endif
        //fprintf(stderr, "%08x: %08x %s\n", RX_REG_PC, ICODE, INSTR->name);
		INSTR->proc();
#if 0
		if((RX_REG_PC < 0xFFFFF000) & (RX_REG_PC > 0xFFF00000)) {
			RX_Break();
		}
#endif

		CheckSignals();
		CycleTimers_Check();
		CycleCounter += 2;
		ICODE = RX_IFetch(RX_REG_PC);
		INSTR = RX_InstructionFind();
#if 0
		if((RX_REG_PC == 0xfff44228)) {
            debug = true;
        }
        if (debug) {
		    fprintf(stderr, "%08x: %08x %s\n", RX_REG_PC, ICODE, INSTR->name);
        }
#endif
		//usleep(100);
        //fprintf(stderr, "%08x: %08x %s\n", RX_REG_PC, ICODE, INSTR->name);
		INSTR->proc();
#if 0
		fprintf(stderr, "%08x: %08x %s\n", RX_REG_PC, ICODE, INSTR->name);
#endif
	}
}
