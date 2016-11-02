/*
 **********************************************************************************************
 * Renesas RX CPU simulation header file
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

#ifndef _CPU_RX_H
#define _CPU_RX_H
#include <stdint.h>
#include <setjmp.h>
#include "byteorder.h"
#include "bus.h"
#include "softfloat.h"
#include "signode.h"
#include "debugger.h"
#include "mainloop_events.h"
#include "cycletimer.h"
#include "throttle.h"

#define RX_SIG_IRQ	(1 << 0)
#define RX_SIG_DBG	(1 << 1)

typedef void RX_InstructionProc(void);

typedef struct RX_Instruction {
        RX_InstructionProc *proc;
        struct RX_Instruction **subTab;
        uint32_t mask;
        uint32_t icode;
        uint32_t *pRs;
        uint32_t *pRs2;
        uint32_t *pRd;
        uint32_t arg1;
        uint32_t arg2;
        uint32_t arg3;
        char *name;
        uint32_t len;
} RX_Instruction;

typedef enum RX_DebugState {
        RXDBG_RUNNING = 0,
        RXDBG_STOP = 1,
        RXDBG_STOPPED = 2,
        RXDBG_STEP = 3,
        RXDBG_BREAK = 4
} RX_DebugState;

typedef struct RX_Cpu {
	RX_Instruction *instr;
	uint32_t icode;
	uint32_t regR[16];
	uint32_t regISP;	
	uint32_t regUSP;
	uint32_t regINTB;
	uint32_t regPC;
	uint32_t regPSW;
	uint32_t regBPC;
	uint32_t regBPSW;
	uint32_t regFINTV;
	uint32_t regFPSW;
	uint64_t regACC;
	uint32_t pendingIpl;
	uint32_t pendingIrqNo;
	uint32_t signals;
	SoftFloatContext *floatContext;
	SigNode *sigIrqAck; /* Acknowledge output for the Interrupt controller */
	Debugger *debugger;
        DebugBackendOps dbgops;
	jmp_buf restart_idec_jump;
        RX_DebugState dbg_state;
        int dbg_steps;
	 /* Throttling cpu to real speed */
	Throttle *throttle;
} RX_Cpu;

extern RX_Cpu g_RXCpu;

#define PSW_C	(UINT32_C(1) << 0)
#define PSW_Z	(UINT32_C(1) << 1)
#define PSW_S	(UINT32_C(1) << 2)
#define PSW_O	(UINT32_C(1) << 3)
#define PSW_I	(UINT32_C(1) << 16)
#define PSW_U	(UINT32_C(1) << 17)
#define PSW_PM	(UINT32_C(1) << 20)
#define PSW_IPL_MSK	(UINT32_C(0xf) << 24)
#define PSW_IPL_SHIFT	(24)

#define FPSW_RM_MSK	(3)
#define FPSW_CV		(1 << 2)
#define FPSW_CO		(1 << 3)
#define FPSW_CZ		(1 << 4)
#define FPSW_CU		(1 << 5)
#define FPSW_CX		(1 << 6)
#define FPSW_CE		(1 << 7)
#define FPSW_DN		(1 << 8)
#define FPSW_EV		(1 << 10)
#define FPSW_EO		(1 << 11)
#define FPSW_EZ		(1 << 12)
#define FPSW_EU		(1 << 13)
#define FPSW_EX		(1 << 14)
#define FPSW_FV		(1 << 26)
#define FPSW_FO		(1 << 27)
#define FPSW_FZ		(1 << 28)
#define FPSW_FU		(1 << 29)
#define FPSW_FX		(1 << 30)
#define FPSW_FS		(1 << 31)

#define ICODE   (g_RXCpu.icode)
#if 1
#define ICODE8()  (be32_to_host(ICODE) >> 24)
#define ICODE16() (be32_to_host(ICODE) >> 16)
#define ICODE24() (be32_to_host(ICODE) >> 8) 
#define ICODE32() (be32_to_host(ICODE)) 
#define ICODE_BE (ICODE);
#else
#define ICODE8()  (ICODE >> 24)
#define ICODE16() (ICODE >> 16)
#define ICODE24() (ICODE >> 8) 
#define ICODE32() (ICODE >> 0) 
#endif
#define INSTR   (g_RXCpu.instr)

#define RX_REG_PC	(g_RXCpu.regPC)
#define RX_REG_FPSW	(g_RXCpu.regFPSW)
#define RX_REG_PSW	(g_RXCpu.regPSW)
#define RX_REG_BPSW	(g_RXCpu.regBPSW)
#define RX_REG_BPC	(g_RXCpu.regBPC)
#define RX_REG_FINTV	(g_RXCpu.regFINTV)
#define RX_REG_INTB	(g_RXCpu.regINTB)
#define RX_FLOAT_CONTEXT (g_RXCpu.floatContext)

static inline void
RX_UpdateIrqSignal(void)
{
        uint32_t ipl = (RX_REG_PSW & PSW_IPL_MSK) >> PSW_IPL_SHIFT;
        if((RX_REG_PSW & PSW_I) && (g_RXCpu.pendingIpl > ipl)) {
                g_RXCpu.signals |= RX_SIG_IRQ;
                mainloop_event_pending = 1;
        } else {
                g_RXCpu.signals &= ~RX_SIG_IRQ;
        }
}


static inline void
RX_SigDebugMode(uint32_t value) {
	if(value) {
		g_RXCpu.signals |= RX_SIG_DBG;
		mainloop_event_pending = 1;
	} else {
		g_RXCpu.signals &= ~RX_SIG_DBG;
	}
}

static inline void
RX_RestartIdecoder(void) {
	longjmp(g_RXCpu.restart_idec_jump,1);
}

static inline void
RX_Break(void) {
        g_RXCpu.dbg_state = RXDBG_BREAK;
        RX_SigDebugMode(1);
        RX_RestartIdecoder();
}

static inline void 
RX_SET_REG_PSW(uint32_t value)
{
	uint32_t diff = value ^ g_RXCpu.regPSW;
	if(diff & PSW_U) {
		if(value & PSW_U) {
			g_RXCpu.regISP = g_RXCpu.regR[0];
			g_RXCpu.regR[0] = g_RXCpu.regUSP;
		} else {
			g_RXCpu.regUSP = g_RXCpu.regR[0];
			g_RXCpu.regR[0] = g_RXCpu.regISP;
		}
	}
	g_RXCpu.regPSW = value;
	if(diff & (PSW_I | PSW_IPL_MSK)) {
		RX_UpdateIrqSignal();
	}
}

/* 
 ************************************************************************
 * This proc is rarely called with unchanged IPL. So there is no diff
 * check here.
 ************************************************************************
 */
static inline void
RX_SET_IPL(uint32_t value)
{
	g_RXCpu.regPSW = (RX_REG_PSW & ~PSW_IPL_MSK) | value;
	RX_UpdateIrqSignal();
}

static inline uint32_t
RX_GET_REG_USP(void) {
	if(RX_REG_PSW & PSW_U) {
		return g_RXCpu.regR[0];
	} else { 
		return g_RXCpu.regUSP;
	} 
}

static inline uint32_t 
RX_GET_REG_ISP(void) { 
	if(RX_REG_PSW & PSW_U) {
		return g_RXCpu.regISP;
	} else { 
		return g_RXCpu.regR[0];
	} 
}

static inline void
RX_SET_REG_USP(uint32_t value) { 
	if(RX_REG_PSW & PSW_U) {
		g_RXCpu.regR[0] = value;
	} else { 
		g_RXCpu.regUSP = value;
	} 
}

static inline void 
RX_SET_REG_ISP(uint32_t value) { 
	if(RX_REG_PSW & PSW_U) {
		g_RXCpu.regISP = value;
	} else { 
		g_RXCpu.regR[0] = value;
	} 
}

static inline uint32_t 
RX_ReadReg(unsigned int reg)
{
	return g_RXCpu.regR[reg];
}

static inline uint32_t* 
RX_RegP(unsigned int reg)
{
	return &g_RXCpu.regR[reg];
}

/**
 **************************************************************************
 * \fn static inline void RX_WriteReg(uint32_t value,unsigned int reg)
 **************************************************************************
 */
static inline void 
RX_WriteReg(uint32_t value,unsigned int reg)
{
	g_RXCpu.regR[reg] = value;
}


static inline uint64_t 
RX_ReadACC(void)
{
	return g_RXCpu.regACC;
}

static inline void
RX_WriteACC(uint64_t value) 
{
	g_RXCpu.regACC = value;
}
/*
 ****************************************************************
 * Always fetch a full 32 Bit word with no alignment check.
 * Only the upper 24 bits are used for decoding in a two level
 * table.
 ****************************************************************
 */
static inline uint32_t
RX_IFetch(uint32_t addr) {
        uint32_t icode;
#if 0
	icode = be32_to_host(Bus_FastRead32(addr));
#else
	icode = Bus_FastRead32(addr);
#endif
        return icode;
}

static inline uint32_t
RX_Read32(uint32_t addr) {
        uint32_t data = Bus_Read32(addr);
        return data;
}

/*
 *********************************************
 * Typecast ! not conversion
 *********************************************
 */
static inline Float32_t 
RX_CastToFloat32(uint32_t value)
{
	return (Float32_t)value;
}

static inline uint32_t 
RX_CastFromFloat32(Float32_t value)
{
	return (Float32_t)value;
}

static inline uint16_t
RX_Read16(uint32_t addr) {
        uint16_t data = Bus_Read16(addr);
        return data;
}

static inline uint32_t
RX_Read24(uint32_t addr) {
	uint32_t data = Bus_Read8(addr);
        data |= Bus_Read8(addr + 1) << 8;
        data |= Bus_Read8(addr + 2) << 16;
        return data;
}

static inline uint8_t
RX_Read8(uint32_t addr) {
        uint8_t data = Bus_Read8(addr);
        return data;
}

static inline void
RX_Write32(uint32_t value,uint32_t addr) {
        Bus_Write32(value,addr);
}

static inline void 
RX_Write16(uint16_t value,uint32_t addr) {
       Bus_Write16(value,addr);
}

static inline void 
RX_Write24(uint32_t value,uint32_t addr) {
	Bus_Write8(value,addr);	
	Bus_Write8(value >> 8,addr + 1);	
	Bus_Write8(value >> 16,addr + 2);	
}

static inline void 
RX_Write8(uint8_t value, uint32_t addr) {
        Bus_Write8(value,addr);
}

void RX_Exception(uint32_t vector_addr);
void RX_Interrupt(unsigned int irq_no,unsigned int ipl);
void RX_Trap(unsigned int trap_nr);


static inline void
RX_FloatingPointException(void) {
        RX_Exception(0xffffffe4);
}

static inline void
RX_AccessException(void) {
        RX_Exception(0xffffffd4);
}

static inline void
RX_PrivilegedInstructionException(void) {
        RX_Exception(0xffffffd0);
}
static inline void
RX_UndefinedInstructionException(void) {
        RX_Exception(0xffffffdc);
}

bool RX_CheckForFloatingPointException(void);

bool RX_CheckForUnimplementedException(void);

RX_Cpu * RX_CpuNew(const char *instancename);
void RX_Run(void);
void RX_PostInterrupt(uint8_t ipl,uint8_t irqNo);

#endif
