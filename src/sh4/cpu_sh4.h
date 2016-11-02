#ifndef _SH4CPU_H
#define _SH4CPU_H
#include <stdint.h>
#include <setjmp.h>
#include "idecode_sh4.h"
#include "signode.h"
#include "mainloop_events.h"
#include "compiler_extensions.h"
#include "debugger.h"
#include "softfloat.h"

/* The status register bits (Page 13) */
#define SR_T		(1 << 0)
#define	SR_S		(1 << 1)
#define SR_IMASK_MSK	(0xf0)
#define SR_IMASK_SHIFT	(4)
#define SR_Q		(1 << 8)
#define SR_M		(1 << 9)
#define SR_FD		(1 << 15)	/* FPU disable bit, Disabled when 1 */
#define SR_BL		(1 << 28)	/* Interrupt/exception block bit */
#define SR_RB		(1 << 29)	/* Register Bank */
#define SR_MD		(1 << 30)	/* 0 = User mode, 1 = Privileged mode */

/* Floating point status register bits */
#define FPSCR_RM_MSK 	(0x3)	/* Rounding modes */
#define		RM_NEAREST	(0x00)
#define		RM_ZERO		(0x01)
/* FPU exception fields */
#define	FPSCR_FPUERR_CS		(1 << 17)
#define	FPSCR_INVOP_CS		(1 << 16)
#define	FPSCR_INVOP_EN		(1 << 11)
#define	FPSCR_INVOP_FL		(1 << 6)
#define FPSCR_DIVZERO_CS	(1 << 15)
#define FPSCR_DIVZERO_EN	(1 << 10)
#define FPSCR_DIVZERO_FL	(1 << 5)
#define FPSCR_OFL_CS		(1 << 14)
#define FPSCR_OFL_EN		(1 << 9)
#define FPSCR_OFL_FL		(1 << 4)
#define FPSCR_UFL_CS		(1 << 13)
#define	FPSCR_UFL_EN		(1 << 8)
#define	FPSCR_UFL_FL		(1 << 3)
#define FPSCR_INEX_CS		(1 << 12)
#define FPSCR_INEX_EN		(1 << 7)
#define FPSCR_INEX_FL		(1 << 2)

#define FPSCR_DN		(1 << 18)
#define FPSCR_PR		(1 << 19)
#define FPSCR_SZ		(1 << 20)
#define FPSCR_FR		(1 << 21)

/* Names taken from ST40 vol 3 Chap 5.4 Page 109 */
typedef enum SH4_Exception_e {
	EX_POWERON,
	EX_MANRESET,
	EX_HUDIRESET,
	EX_ITLBMULTIHIT,
	EX_DTLBMULTIHIT,
	EX_UBRKBEFORE,
	EX_IADDERR,
	EX_ITLBMISS,
	EX_EXECPROT,
	EX_RESINST,
	EX_ILLSLOT,
	EX_FPUDIS,
	EX_SLOTFPUDIS,
	EX_RADDERR,
	EX_WADDERR,
	EX_RTLBMISS,
	EX_WTLBMISS,
	EX_READPROT,
	EX_WRITEPROT,
	EX_FPUEXC,
	EX_FIRSTWRITE,
	EX_TRAP,
	EX_UBRKAFTER,
	EX_NMI,
	EX_IRL_INT_0,
	EX_IRL_INT_1,
	EX_IRL_INT_2,
	EX_IRL_INT_3,
	EX_IRL_INT_4,
	EX_IRL_INT_5,
	EX_IRL_INT_6,
	EX_IRL_INT_7,
	EX_IRL_INT_8,
	EX_IRL_INT_9,
	EX_IRL_INT_A,
	EX_IRL_INT_B,
	EX_IRL_INT_C,
	EX_IRL_INT_D,
	EX_IRL_INT_E,
	EX_PERINT_TMU0,
	EX_PERINT_TMU1,
	EX_PERINT_TMU2,
	EX_PERINT_TMU2_TICPI2,
	EX_PERINT_TMU3,
	EX_PERINT_TMU4,
	EX_PERINT_RTC_ATI,
	EX_PERINT_RTC_PRI,
	EX_PERINT_RTC_CUI,
	EX_PERINT_SCI_ERI,
	EX_PERINT_SCI_RXI,
	EX_PERINT_SCI_TXI,
	EX_PERINT_SCI_TEI,
	EX_PERINT_WDT_ITI,
	EX_PERINT_REF_RCMI,
	EX_PERINT_REF_ROVI,
	EX_PERINT_HUDI,
	EX_PERINT_GPIO,
	EX_PERINT_DMAC_DMTE0,
	EX_PERINT_DMAC_DMTE1,
	EX_PERINT_DMAC_DMTE2,
	EX_PERINT_DMAC_DMTE3,
	EX_PERINT_DMAC_DMAE,
	EX_PERINT_SCIF_ERI,
	EX_PERINT_SCIF_RXI,
	EX_PERINT_SCIF_BRI,
	EX_PERINT_SCIF_TXI,
	EX_PERINT_PCIC_PCISERR,
	EX_PERINT_PCIC_PCIERR,
	EX_PERINT_PCIC_PCIPWDWN,
	EX_PERINT_PCIC_PCIPWON,
	EX_PERINT_PCIC_PCIDMA0,
	EX_PERINT_PCIC_PCIDMA1,
	EX_PERINT_PCIC_PCIDMA2,
	EX_PERINT_PCIC_PCIDMA3,
} SH4_Exception_e;

#define SH4_SIG_IRQ		(1)
#define SH4_SIG_DBG		(2)
#define SH4_SIG_RESTART_IDEC	(4)

typedef enum SH4_DebugState {
	SH4DBG_RUNNING = 0,
	SH4DBG_STOP = 1,
	SH4DBG_STOPPED = 2,
	SH4DBG_STEP = 3,
	SH4DBG_BREAK = 4
} SH4_DebugState;

typedef struct SH4Cpu {
	uint32_t icode;		/* Is only 16 Bit but compiler is better for 32 bit */
	uint32_t regR[16];
	uint32_t regRBank0[8];
	uint32_t regRBank1[8];
	uint32_t regSR;
	uint32_t regSSR;
	uint32_t regGBR;
	uint32_t regMACH;
	uint32_t regMACL;
	uint32_t regPR;
	uint32_t regVBR;
	uint32_t regPC;
	uint32_t regSPC;
	uint32_t regSGR;
	uint32_t regDBR;
	/* The floating point registers */
	SoftFloatContext *sfloat;
	uint32_t regFPUL;	/* Communication register */
	uint32_t regFPSCR;
	uint32_t regFR[32];

	SigNode *sigIrq;
	uint32_t signals_raw;
	uint32_t signals_mask;
	uint32_t signals;

	uint32_t io_trapa;
	uint32_t io_expevt;
	uint32_t io_intevt;

	Debugger *debugger;
	DebugBackendOps dbgops;
	jmp_buf restart_idec_jump;
	SH4_DebugState dbg_state;
	int dbg_steps;
} SH4Cpu;

extern SH4Cpu gcpu_sh4;

static inline void
SH4_UpdateSignals(void)
{
	gcpu_sh4.signals = gcpu_sh4.signals_raw & gcpu_sh4.signals_mask;
	if (gcpu_sh4.signals) {
		mainloop_event_pending = 1;
	}
}

static inline void
SH4_PostSignal(uint32_t signal)
{
	gcpu_sh4.signals_raw |= signal;
	SH4_UpdateSignals();
}

static inline void
SH4_UnpostSignal(uint32_t signal)
{
	gcpu_sh4.signals_raw &= ~signal;
	gcpu_sh4.signals = gcpu_sh4.signals_raw & gcpu_sh4.signals_mask;
}

static inline void
SH4_RestartIdecoder(void)
{
	longjmp(gcpu_sh4.restart_idec_jump, 1);
}

#define ICODE (gcpu_sh4.icode)

static inline void
SH4_SwitchBank(int new_bank)
{
	int i;
	if (new_bank) {
		for (i = 0; i < 8; i++) {
			gcpu_sh4.regRBank0[i] = gcpu_sh4.regR[i];
			gcpu_sh4.regR[i] = gcpu_sh4.regRBank1[i];
		}
	} else {
		for (i = 0; i < 8; i++) {
			gcpu_sh4.regRBank1[i] = gcpu_sh4.regR[i];
			gcpu_sh4.regR[i] = gcpu_sh4.regRBank0[i];
		}
	}
}

static inline void
SH4_SetTrue(int val)
{
	if (val) {
		gcpu_sh4.regSR |= SR_T;
	} else {
		gcpu_sh4.regSR &= ~SR_T;
	}
}

static inline int
SH4_GetTrue(void)
{
	return !!(gcpu_sh4.regSR & SR_T);
}

static inline void
SH4_SetS(int val)
{
	if (val) {
		gcpu_sh4.regSR |= SR_S;
	}
}

static inline int
SH4_GetS(void)
{
	return !!(gcpu_sh4.regSR & SR_S);
}

static inline void
SH4_SetSR(uint32_t sr)
{
	uint32_t diff = sr ^ gcpu_sh4.regSR;
	if (unlikely(diff & SR_RB)) {
		SH4_SwitchBank(!!(sr & SR_RB));
	}
	if (unlikely(diff & SR_BL)) {
		if (sr & SR_BL) {
			gcpu_sh4.signals_mask &= ~SH4_SIG_IRQ;
		} else {
			gcpu_sh4.signals_mask |= SH4_SIG_IRQ;
			SH4_UpdateSignals();
		}
	}
	gcpu_sh4.regSR = sr;
}

static inline uint32_t
SH4_GetSR(void)
{
	return gcpu_sh4.regSR;
}

static inline void
SH4_SetFPUL(uint32_t fpul)
{
	gcpu_sh4.regFPUL = fpul;
}

static inline uint32_t
SH4_GetFPUL(void)
{
	return gcpu_sh4.regFPUL;
}

static inline void
SH4_SetSGR(uint32_t sgr)
{
	gcpu_sh4.regSGR = sgr;
}

static inline uint32_t
SH4_GetSGR(void)
{
	return gcpu_sh4.regSGR;
}

static inline void
SH4_SetSPC(uint32_t spc)
{
	gcpu_sh4.regSPC = spc;
}

static inline uint32_t
SH4_GetSPC(void)
{
	return gcpu_sh4.regSPC;
}

static inline void
SH4_SetSSR(uint32_t ssr)
{
	gcpu_sh4.regSSR = ssr;
}

static inline uint32_t
SH4_GetSSR(void)
{
	return gcpu_sh4.regSSR;
}

static inline void
SH4_SetDBR(uint32_t dbr)
{
	gcpu_sh4.regDBR = dbr;
}

static inline uint32_t
SH4_GetDBR(void)
{
	return gcpu_sh4.regDBR;
}

static inline void
SH4_SetVBR(uint32_t vbr)
{
	gcpu_sh4.regVBR = vbr;
}

static inline uint32_t
SH4_GetVBR(void)
{
	return gcpu_sh4.regVBR;
}

static inline uint32_t
SH4_GetSRFlag(uint32_t flag)
{
	return !!(gcpu_sh4.regSR & flag);
}

static inline void
SH4_ModSRFlags(int value, uint32_t flags)
{
	if (value) {
		gcpu_sh4.regSR |= flags;
	} else {
		gcpu_sh4.regSR &= ~flags;
	}
}

static inline void
SH4_SetGpr(uint32_t value, int reg)
{
	gcpu_sh4.regR[reg] = value;
}

static inline uint32_t
SH4_GetGpr(int reg)
{
	return gcpu_sh4.regR[reg];
}

/*
 * Floating point registers
 */
static inline void
SH4_SetFpr(uint32_t value, int reg)
{
	gcpu_sh4.regFR[reg] = value;
}

static inline uint32_t
SH4_GetFpr(int reg)
{
	return gcpu_sh4.regFR[reg];
}

static inline void
SH4_SetDpr(uint64_t value, int reg)
{
	gcpu_sh4.regFR[reg << 1] = value >> 32;
	gcpu_sh4.regFR[(reg << 1) + 1] = value;
}

static inline uint64_t
SH4_GetDpr(int reg)
{
	uint64_t dpr;
	dpr = (uint64_t) gcpu_sh4.regFR[reg << 1] << 32 | gcpu_sh4.regFR[(reg << 1) + 1];
	return dpr;
}

/* 
 ****************************************+
 * Extended floating point registers 
 ****************************************+
 */
static inline void
SH4_SetXF(uint32_t value, int xi)
{

}

static inline uint32_t
SH4_GetXF(int xi)
{
	return 0;
}

static inline void
SH4_SetXD(uint64_t value, int xi)
{

}

static inline uint64_t
SH4_GetXD(int xi)
{
	return 0;
}

static inline void
SH4_SetFPSCR(uint32_t value)
{
	gcpu_sh4.regFPSCR = value;
}

static inline uint32_t
SH4_GetFPSCR(void)
{
	return gcpu_sh4.regFPSCR;
}

static inline SoftFloatContext *
SH4_GetSFloat(void)
{
	return gcpu_sh4.sfloat;
}

/**
 ******************************************************
 * Other Bank access: Write to the NON-current bank
 ******************************************************
 */
static inline void
SH4_SetGprBank(uint32_t value, int reg)
{
	if (gcpu_sh4.regSR & SR_RB) {
		gcpu_sh4.regRBank0[reg] = value;
	} else {
		gcpu_sh4.regRBank1[reg] = value;
	}
}

/**
 ******************************************************
 * Other Bank access: Read from NON-current bank
 ******************************************************
 */
static inline uint32_t
SH4_GetGprBank(int reg)
{
	if (gcpu_sh4.regSR & SR_RB) {
		return gcpu_sh4.regRBank0[reg];
	} else {
		return gcpu_sh4.regRBank1[reg];
	}
}

static inline uint32_t
SH4_GetGBR(void)
{
	return gcpu_sh4.regGBR;
}

static inline void
SH4_SetGBR(uint32_t gbr)
{
	gcpu_sh4.regGBR = gbr;
}

static inline void
SH4_SetPR(uint32_t pr)
{
	gcpu_sh4.regPR = pr;
}

static inline uint32_t
SH4_GetPR(void)
{
	return gcpu_sh4.regPR;
}

static inline void
SH4_SetRegPC(uint32_t value)
{
	gcpu_sh4.regPC = value;
}

/* Power PC style access to Programm counter */
#define SH4_CIA		(gcpu_sh4.regPC - 2)	/* Current instruction address */
#define SH4_NIA		(gcpu_sh4.regPC)	/* Next instruction address     */
#define SH4_NNIA	(gcpu_sh4.regPC + 2)	/* Next instruction address     */

static inline uint32_t
SH4_GetRegPC(void)
{
	return gcpu_sh4.regPC;
}

static inline void
SH4_SetMacH(uint32_t mach)
{
	gcpu_sh4.regMACH = mach;
}

static inline uint32_t
SH4_GetMacH(void)
{
	return gcpu_sh4.regMACH;
}

static inline void
SH4_SetMacL(uint32_t macl)
{
	gcpu_sh4.regMACL = macl;
}

static inline uint32_t
SH4_GetMacL(void)
{
	return gcpu_sh4.regMACL;
}

static inline uint64_t
SH4_GetMac(void)
{
	return gcpu_sh4.regMACL | ((uint64_t) gcpu_sh4.regMACH << 32);
}

static inline void
SH4_SetMac(uint64_t mac)
{
	gcpu_sh4.regMACL = (uint32_t) mac;
	gcpu_sh4.regMACH = mac >> 32;
}

/* 
 ****************************************************************
 * A branch in a Branch delay slot is an illegal instruction 
 * Check this with a recursion check 
 * Do not accept interrupts between instr. and delay slot
 ****************************************************************
 */
void SH4_ExecuteDelaySlot(void);

/*
 ********************************************************************
 * \fn static inline void SH4_AbortInstruction(void)
 * Aborts the current instruction with a longjmp. 
 * This should use a separate longjump target, but 
 * I'm to lazy and use the restart idecoder target.
 ********************************************************************
 */
static inline void __NORETURN__
SH4_AbortInstruction(void)
{
	longjmp(gcpu_sh4.restart_idec_jump, 1);
}

static inline void
SH4_Break()
{
	gcpu_sh4.dbg_state = SH4DBG_BREAK;
	SH4_SetRegPC(SH4_GetRegPC() - 2);
	SH4_PostSignal(SH4_SIG_DBG);
	SH4_RestartIdecoder();
}

void SH4_Init(const char *instancename);
void SH4_Run();
void SH4_Exception(SH4_Exception_e exindex);

#endif
