#ifndef ARM9CPU_H
#define ARM9CPU_H
#include <stdint.h>
#include <setjmp.h>
#include <coprocessor.h>
#include <idecode_arm.h>
#include <xy_tree.h>
#include <sys/time.h>
#include <debugger.h>
#include "mainloop_events.h"
#include "signode.h"
#include "cycletimer.h"
/*
 * ------------------------------------------------------
 * ARM9_RegPointerSet
 *   ARM has banked registers. The bank selection is
 *   done with the mode from CPSR. The CPU has one 
 *   pointer set to the register storage for every mode
 * -------------------------------------------------------
 */
typedef struct ARM9_RegPointerSet {
	uint32_t *r0;
	uint32_t *r1;
	uint32_t *r2;
	uint32_t *r3;
	uint32_t *r4;
	uint32_t *r5;
	uint32_t *r6;
	uint32_t *r7;
	uint32_t *r8;
	uint32_t *r9;
	uint32_t *r10;
	uint32_t *r11;
	uint32_t *r12;
	uint32_t *r13;
	uint32_t *r14;
	uint32_t *pc;	
	uint32_t *spsr;
} ARM9_RegPointerSet;

extern uint64_t cpu_cyclecounter;
extern uint32_t mmu_vector_base;
extern uint32_t do_alignment_check;


#define MODE_USER 	(0x10)
#define MODE_FIQ  	(0x11)
#define MODE_IRQ  	(0x12)
#define MODE_SVC	(0x13)
#define MODE_ABORT	(0x17)
#define MODE_UNDEFINED	(0x1B)
#define MODE_SYSTEM	(0x1F)
#define MODE_SECMON	(0x16)
#define MODE_MASK	(0x1f)

/**
 ***********************************************************************************************
 * The exception ID consists of the mode to be entered in the Lower 5 bits of the ID 
 * and the Programmcounter offset of the exception handler.
 ***********************************************************************************************
 */
typedef enum ARM_ExceptionID {
	EX_RESET =	(MODE_SVC | 0),
	EX_UNDEFINED =	(MODE_UNDEFINED | (0x00000004 << 8)),
	EX_SWI =	(MODE_SVC | (0x00000008 << 8)),
	EX_PABT =	(MODE_ABORT|(0x0000000c << 8)),
	EX_DABT =	(MODE_ABORT|(0x00000010 << 8)),
	EX_IRQ =	(MODE_IRQ | (0x00000018 << 8)),
	EX_FIQ =	(MODE_FIQ | (0x0000001c << 8)),
} ARM_ExceptionID;

#define EX_TO_MODE(ex) ((ex)&0x1f)
#define EX_TO_PC(ex)   ((ex)>>8)

#define COND_EQ		(0)
#define COND_NE 	(1)
#define COND_CSHS 	(2)
#define COND_CCLO	(3)
#define COND_MI		(4)
#define COND_PL		(5)
#define COND_VS		(6)
#define COND_VC		(7)
#define COND_HI		(8)
#define COND_LS		(9)
#define COND_GE		(10)
#define COND_LT		(11)
#define COND_GT		(12)
#define COND_LE		(13)
#define COND_AL		(14)
#define COND_ILLEGAL	(15)

#define FLAG_N		(1UL<<31)
#define FLAG_Z		(1UL<<30)
#define FLAG_C		(1UL<<29)
#define FLAG_C_SHIFT	(29)
#define FLAG_V		(1UL<<28)
#define FLAG_Q		(1UL<<27)
#define FLAG_T		(1UL<<5)
#define FLAG_F		(1UL<<6)
#define FLAG_I		(1UL<<7)

typedef struct ARM9 {
	/* Scratch registers for avoiding arguments to functions  */
	uint32_t icode;
	uint32_t am_scratch1, am_scratch2,am_scratch3;

	uint32_t registers[17];
 	uint32_t reg_cpsr;
	uint32_t reg_bank; /* duplicate of lower 5 Bits of cpsr for fast access */
	uint32_t signaling_mode; /* most time the same like bits 0-4 of cpsr */ 
	/* 
	 *  The Values of the following registers are only valid
         *  if they are not in the current active register set
 	 *  because registers[] contains the valid copy of 
         *  the active registers
	 */
	uint32_t r0, r1, r2, r3, r4, r5, r6, r7; /* never valid */
	uint32_t r8, r9, r10, r11, r12, r13, r14;
	uint32_t r13_svc, r14_svc;
	uint32_t r8_fiq, r9_fiq, r10_fiq, r11_fiq, r12_fiq;	
	uint32_t r13_fiq, r14_fiq;
	uint32_t r13_abt, r14_abt; 
	uint32_t r13_irq, r14_irq;
	uint32_t r13_und, r14_und;
	uint32_t r13_mon, r14_mon;
	uint32_t spsr_fiq, spsr_svc, spsr_abt, spsr_irq, spsr_und, spsr_mon;
	uint32_t reg_dummy;

	ARM9_RegPointerSet regSet[32];

	/* 
         * --------------------------------------------------------------
         * signaling_mode:
         *   The mode which is used for permission checking.
 	 *     
  	 * -------------------------------------------------------------
	 */  

	/* Signals from outside (IRQ, FIQ) */
	uint32_t signals_raw;
	uint32_t signal_mask;
	uint32_t signals;

	SigNode *irqNode;
	SigNode *fiqNode;
	SigTrace *irqTrace;
	SigTrace *fiqTrace;

	ArmCoprocessor *copro[16];
	/* Timer Handlers and statistics */
	struct timeval starttime;
	jmp_buf abort_jump;
	jmp_buf restart_idec_jump;
	
	/* The GDB Operations */
	int dbg_state;
	int dbg_steps;
	Debugger *debugger;
	DebugBackendOps dbgops;

	/* Throttling cpu to real speed */
        struct timespec tv_last_throttle;
        CycleCounter_t last_throttle_cycles;
        CycleTimer throttle_timer;
        int64_t cycles_ahead; /* Number of cycles ahead of real cpu */

	uint32_t cpuArchitecture;
} ARM9;

#define ARCH_ARMV5		(0)
#define ARCH_ARMV6		(1)
#define ARCH_ARMV7		(2)

#define DBG_STATE_RUNNING     	(0)
#define DBG_STATE_STOP		(1)
#define DBG_STATE_STOPPED	(2)
#define DBG_STATE_STEP		(3)
#define DBG_STATE_BREAK		(4)
extern ARM9 gcpu;

/*
 * Bit in field cpu_signals
 */
#define ARM_SIG_IRQ		(1<<0)  /* Normal Interrupt */
#define ARM_SIG_FIQ		(1<<1)  /* Fast Interrupt */
#define ARM_SIG_RESTART_IDEC	(1<<2)  /* Something changed in CPU or debugmode */
#define ARM_SIG_DEBUGMODE	(1<<3)
#define CPU_REGS (gcpu.regSet)

void ARM_set_reg_cpsr(uint32_t val); 

#define PC_OFFSET (4)
#define THUMB_PC_OFFSET (2)
#define REG_LR	 ((gcpu.registers[14]))
/*
 **********************************************************
 * NIA is power PC style: Next instruction address
 **********************************************************
 */
#define ARM_NIA  		(gcpu.registers[15])
#define ARM_GET_CIA  		(gcpu.registers[15] - PC_OFFSET)
#define ARM_GET_NNIA  		(gcpu.registers[15] + PC_OFFSET)
#define THUMB_GET_CIA 		(gcpu.registers[15] - PC_OFFSET)
#define THUMB_NIA 		(gcpu.registers[15])
#define THUMB_GET_NNIA 		(gcpu.registers[15] + THUMB_PC_OFFSET)

#define ARM_SET_NIA(val)	({gcpu.registers[15]=(val);})
#define REG_CPSR      (gcpu.reg_cpsr)

#define SET_REG_CPSR(val) ARM_set_reg_cpsr(val); 
#define ARM_BANK     	(gcpu.reg_bank)
#define ARM_SIGNALING_MODE     	(gcpu.signaling_mode)
#define REG_SPSR	 (gcpu.registers[16])
#define MODE_HAS_SPSR (gcpu.regSet[gcpu.reg_bank].spsr)
#define REG_NR_SPSR  (16)

#define AM_SCRATCH1 (gcpu.am_scratch1)
#define AM3_NEW_RN (gcpu.am_scratch2)
#define AM3_UPDATE_RN (gcpu.am_scratch3)
#define ICODE (gcpu.icode)

static inline void 
ARM9_RegisterCoprocessor(ArmCoprocessor *copro,unsigned int nr) {
	if(nr>15) {
		exit(2);
	} else { 
		gcpu.copro[nr]=copro;
	}
}

ARM9* ARM9_New();
void ARM9_Run();

static inline uint32_t
Thumb_ReadReg(int nr) {
        if(likely(nr!=15)) {
                return gcpu.registers[nr];
        } else {
                return gcpu.registers[15]+THUMB_PC_OFFSET;
        }
}

static inline uint32_t
Thumb_ReadHighReg(int nr) {
        if(likely(nr!=15)) {
                return gcpu.registers[nr];
        } else {
                return gcpu.registers[15]+THUMB_PC_OFFSET;
        }
}

static inline void 
Thumb_WriteReg(uint32_t val,int nr) {
	gcpu.registers[nr] = val;
}

/*
 * ------------------------------------------------------
 * Register access for "normal" instructions  
 * which use the currently active register set
 * ------------------------------------------------------
 */
static inline uint32_t 
ARM9_ReadReg(int nr) {
        if(likely(nr!=15)) {
                return gcpu.registers[nr];
        } else {
                return gcpu.registers[15]+PC_OFFSET;
        }
}
static inline uint32_t 
ARM9_ReadRegNot15(int nr) {
	return gcpu.registers[nr];
}

static inline void 
ARM9_WriteReg(uint32_t val,int nr) {
	gcpu.registers[nr] = val;
}

/*
 * ------------------------------------------------------
 * Register access for instructions which don't use
 * the current register set but explicitely specify
 * the bank.
 * Do not call 
 * ------------------------------------------------------
 */
static inline uint32_t
ARM9_ReadRegBank(int nr,int bank) 
{
        uint32_t **regpp;
	regpp=(&CPU_REGS[bank].r0+nr);
	if(*regpp == *(&CPU_REGS[gcpu.reg_bank].r0+nr)) {
		return ARM9_ReadReg(nr);
	} else {
		if(unlikely(nr==15)) {
			return **regpp+PC_OFFSET;
		} else {
			return **regpp;
		}
	}
}

static inline void
ARM9_WriteRegBank(uint32_t val,int nr,int bank) 
{
        uint32_t **regpp;
	regpp=(&CPU_REGS[bank].r0+nr);
	if(*regpp == *(&CPU_REGS[gcpu.reg_bank].r0+nr)) {
		return ARM9_WriteReg(val,nr);
	} else {
		**regpp=val;
	}
}

/*
 * -------------------------------------
 * Interrupt handling 
 * -------------------------------------
 */
static inline void 
ARM_PostIrq() {
	gcpu.signals_raw |= ARM_SIG_IRQ;	
	gcpu.signals = gcpu.signals_raw & gcpu.signal_mask;
	if(gcpu.signals)
		mainloop_event_pending = 1;
}

static inline void 
ARM_UnPostIrq() {
	gcpu.signals_raw &= ~ARM_SIG_IRQ;	
	gcpu.signals = gcpu.signals_raw & gcpu.signal_mask;
}

static inline void 
ARM_PostFiq() {
	gcpu.signals_raw |= ARM_SIG_FIQ;	
	gcpu.signals = gcpu.signals_raw & gcpu.signal_mask;
	if(gcpu.signals)
		mainloop_event_pending = 1;
}
static inline void 
ARM_UnPostFiq() {
	gcpu.signals_raw &= ~ARM_SIG_FIQ;	
	gcpu.signals = gcpu.signals_raw & gcpu.signal_mask;
}


static inline void 
ARM_PostRestartIdecoder() {
	gcpu.signals_raw |= ARM_SIG_RESTART_IDEC;
        gcpu.signals = gcpu.signals_raw & gcpu.signal_mask;
	if(gcpu.signals)
		mainloop_event_pending = 1;
}

static inline void 
ARM_SigDebugMode(value) {
	if(value) {
		gcpu.signals_raw |= ARM_SIG_DEBUGMODE;
	} else {
		gcpu.signals_raw &= ~ARM_SIG_DEBUGMODE;
	}
        gcpu.signals = gcpu.signals_raw & gcpu.signal_mask;
	if(gcpu.signals)
		mainloop_event_pending = 1;
}

static inline void 
ARM_Break() {
	gcpu.dbg_state = DBG_STATE_BREAK;
	ARM_SigDebugMode(1); 
	ARM_PostRestartIdecoder(); 
}

void ARM_Exception(ARM_ExceptionID exception,int nia_offset);

static inline void
ARM_RestartIdecoder() {
	longjmp(gcpu.restart_idec_jump,1);
}

#endif
