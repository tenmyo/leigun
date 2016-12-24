#ifndef _AVR8_CPU_H
#define _AVR8_CPU_H
#include <stdint.h>
#include <time.h>
#include <setjmp.h>
#include <sys/types.h>
#include <errno.h>
#include "mainloop_events.h"
#include "diskimage.h"
#include "compiler_extensions.h"
#include "signode.h"
#include "avr8/avr8_io.h"
#include "avr8/idecode_avr8.h"
#include "cycletimer.h"
#include "clock.h"
#include "byteorder.h"
#ifndef NO_DEBUGGER
#include "debugger.h"
#endif
#include "throttle.h"

#define FLG_C	(1<<0)
#define	FLG_C_SH	(0)
#define FLG_Z	(1<<1)
#define	FLG_Z_SH	(1)
#define FLG_N	(1<<2)
#define	FLG_N_SH	(2)
#define FLG_V	(1<<3)
#define	FLG_V_SH	(3)
#define FLG_S	(1<<4)
#define	FLG_S_SH	(4)
#define FLG_H	(1<<5)
#define FLG_H_SH	(5)
#define FLG_T	(1<<6)
#define	FLG_T_SH	(6)
#define FLG_I	(1<<7)
#define	FLG_I_SH	(7)

#define AVR8_SIG_IRQ             (1 << 0)	/* Normal Interrupt */
#define	AVR8_SIG_IRQENABLE	 (1 << 1)	/* Enable IRQ delayed ("sei" instruction) */
#define AVR8_SIG_RESTART_IDEC    (1 << 2)	/* Restart the Idecoder at end of Check signals. */
#define AVR8_SIG_DBG             (1 << 3)	/* Communicates in check signal with debugger. */

#define GET_REG_PC (gavr8.pc)
#define RAMPZ	(gavr8.rampz)
#define GET_REG_SP (gavr8.sp)
#define SET_REG_PC(val) (gavr8.pc = (val))
#define SET_REG_SP(val) (gavr8.sp = (val))
#define ICODE	(gavr8.icode)
#define GET_SREG  (gavr8.sreg)
#define SET_SREG(val) 	(gavr8.sreg = (val))

/* Two byte register number */
#define NR_REG_X (26)
#define NR_REG_Y (28)
#define NR_REG_Z (30)
#define IOA_RAMPD	(0x58)
#define IOA_EIND	(0x5c)
#define IOA_RAMPX	(0x59)
#define IOA_RAMPY	(0x5a)
#define IOA_RAMPZ	(0x5b)
#define IOA_SPL		(0x5d)
#define IOA_SPH		(0x5e)
#define IOA_SREG	(0x5f)

typedef enum AVR_DebugState {
	AVRDBG_RUNNING = 0,
	AVRDBG_STOP = 1,
	AVRDBG_STOPPED = 2,
	AVRDBG_STEP = 3,
	AVRDBG_BREAK = 4
} AVR_DebugState;

typedef struct AVR8_Cpu {
	uint8_t gpr[32];	/* Be the first. This is aligned */
	AVR8_Iohandler **mmioHandler;
	uint8_t sreg;
	uint16_t sp;
	uint32_t pc;
	int pc24bit;
	uint8_t rampz;
	uint8_t regEIND;
	/* Tables for faster flag calculation */
	uint8_t *add_flags;
	uint8_t *sub_flags;

	/* The memory */
	uint8_t *sram;
	uint16_t *appmem;
	uint8_t *appmem_byte;
	uint32_t appmem_words;
	uint32_t appmem_word_mask;
	uint32_t appmem_byte_mask;
	uint32_t io_registers;
	uint32_t sram_start;
	uint32_t sram_bytes;
	uint32_t sram_end;
	uint32_t cpu_signals_raw;
	uint32_t cpu_signals;
	uint32_t cpu_signal_mask;
	SigNode *wdResetNode;

	DiskImage *flash_di;

	/* The current instruction */
	uint16_t icode;
	int nr_intvects;
	SigNode **irqNode;
	SigNode **irqAckNode;
	Clock_t *clk;

	/* Throttle the CPU to its real speed */
	Throttle *throttle;

#ifndef NO_DEBUGGER
	Debugger *debugger;
	DebugBackendOps dbgops;
#endif
	jmp_buf restart_idec_jump;
	AVR_DebugState dbg_state;
	int dbg_steps;
	void (*avrAckIrq) (void *);
	void (*avrReti) (void *);
	void *avrIrqData;
} AVR8_Cpu;

void AVR8_DumpPcBuf(void);
void AVR8_DumpRegisters(void);

extern AVR8_Cpu gavr8;
static inline uint8_t
AVR8_ReadReg(unsigned int reg)
{
	return gavr8.gpr[reg];
}

static inline uint16_t 
read_uint16_t(void *ptr) {
	return (uint16_t) (*(uint16_t *) (ptr));
}

static inline void 
write_uint16_t(void *ptr, uint16_t val) {
	*(uint16_t *) (ptr) = (uint16_t) (val);
}

static inline uint16_t
AVR8_ReadReg16(unsigned int reg)
{
	/* Byteorder Low, High  */
#if __BYTE_ORDER == __BIG_ENDIAN
	return gavr8.gpr[reg] | (gavr8.gpr[reg + 1] << 8);
#else
	return read_uint16_t(&gavr8.gpr[reg]);
#endif
}

static inline void
AVR8_WriteReg(uint8_t val, unsigned int reg)
{
	gavr8.gpr[reg] = val;
}

static inline void
AVR8_WriteReg16(uint16_t val, unsigned int reg)
{
#if __BYTE_ORDER == __BIG_ENDIAN
	gavr8.gpr[reg + 1] = val >> 8;
	gavr8.gpr[reg] = val & 0xff;
#else
	write_uint16_t(&gavr8.gpr[reg],val); 
#endif
}

static inline void
AVR8_WriteMem8(uint8_t val, uint32_t addr)
{
	if (addr < gavr8.io_registers) {
		AVR8_Iohandler *ioh;
		ioh = gavr8.mmioHandler[addr];
		ioh->ioWriteProc(ioh->clientData, val, addr);
#if 0
	This is only required on XMega because of EEPROM} else if (addr < gavr8.sram_start) {
		fprintf(stderr, "Write to nonexistent %04x\n", addr);
#endif
	} else if (addr < gavr8.sram_end) {
		gavr8.sram[addr - gavr8.sram_start] = val;
	}
}

static inline uint8_t
AVR8_ReadMem8(uint32_t addr)
{
	if (addr < gavr8.io_registers) {
		AVR8_Iohandler *ioh;
		ioh = gavr8.mmioHandler[addr];
		return ioh->ioReadProc(ioh->clientData, addr);
#if 0
	This is only required on XMega because of EEPROM} else if (addr < gavr8.sram_start) {
		return 0;
#endif
	} else if (addr < gavr8.sram_end) {
		return gavr8.sram[addr - gavr8.sram_start];
	} else {
		return 0;
	}
}

static inline void
AVR8_WriteIO8(uint8_t val, uint32_t addr)
{
	AVR8_Iohandler *ioh;
	addr += 0x20;
	ioh = gavr8.mmioHandler[addr];
	ioh->ioWriteProc(ioh->clientData, val, addr);
}

static inline uint8_t
AVR8_ReadIO8(uint32_t addr)
{
	AVR8_Iohandler *ioh;
	addr += 0x20;
	ioh = gavr8.mmioHandler[addr];
	return ioh->ioReadProc(ioh->clientData, addr);
}

/*
 *****************************************
 * Keep the order, high first then low
 *****************************************
 */
static inline void
AVR8_WriteMem16(uint16_t value, int addr)
{
	AVR8_WriteMem8(value << 8, addr + 1);
	AVR8_WriteMem8(value & 0xff, addr);
}

/*
 * ------------------------------------------------------------------------
 * Access to the Application memory (Memory where the instructions are
 * located). The AVR is a Havard architecture
 * ------------------------------------------------------------------------
 */
static inline uint16_t
AVR8_ReadAppMem(uint32_t word_addr)
{
	return gavr8.appmem[word_addr & gavr8.appmem_word_mask];
}

static inline uint8_t
AVR8_ReadAppMem8(uint32_t byte_addr)
{
#if __BYTE_ORDER == __BIG_ENDIAN
	return gavr8.appmem_byte[(byte_addr ^ 1) & gavr8.appmem_byte_mask];
#else
	return gavr8.appmem_byte[byte_addr & gavr8.appmem_byte_mask];
#endif
}

/*
 ************************************************************
 * \fn AVR8_WriteAppMem8(uint8_t value,uint32_t byteaddr);
 * Only usefull for the debugger, the device itself
 * can not write single bytes. 
 ************************************************************
 */
static inline void
AVR8_WriteAppMem8(uint8_t value, uint32_t byte_addr)
{
#if __BYTE_ORDER == __BIG_ENDIAN
	gavr8.appmem_byte[(byte_addr ^ 1) & gavr8.appmem_byte_mask] = value;
#else
	gavr8.appmem_byte[byte_addr & gavr8.appmem_byte_mask] = value;
#endif
}

void AVR8_RegisterIOHandler(uint32_t addr, AVR8_IoReadProc *, AVR8_IoWriteProc *, void *clientData);

void AVR8_Run();
void AVR8_Init(const char *instancename);
void AVR8_UpdateCpuSignals(void);
static inline void
AVR8_SkipInstruction(void)
{
	AVR8_Instruction *instr;
	ICODE = AVR8_ReadAppMem(GET_REG_PC);
	instr = AVR8_InstructionFind(ICODE);
	SET_REG_PC(GET_REG_PC + instr->length);
	CycleCounter += instr->length;
}

static inline void
AVR8_PostSignal(uint32_t sig)
{
	gavr8.cpu_signals_raw |= sig;
	gavr8.cpu_signals = gavr8.cpu_signals_raw & gavr8.cpu_signal_mask;
	if (gavr8.cpu_signals) {
		mainloop_event_pending = 1;
	}
}

static inline void
AVR8_UnpostSignal(uint32_t sig)
{
	gavr8.cpu_signals_raw &= ~sig;
	gavr8.cpu_signals = gavr8.cpu_signals_raw & gavr8.cpu_signal_mask;
}

static inline void
AVR8_RestartIdecoder(void)
{
	longjmp(gavr8.restart_idec_jump, 1);
}

static inline void
AVR8_Break(void)
{
	gavr8.dbg_state = AVRDBG_BREAK;
	SET_REG_PC(GET_REG_PC - 1);
	AVR8_PostSignal(AVR8_SIG_DBG);
	AVR8_RestartIdecoder();
}

void AVR8_RegisterIntco(void (*ackProc) (void *), void (*retiProc) (void *), void *eventData);
#endif
