/*
 *************************************************************************************************
 *
 * Emulation of the Atmel AVR 8 Bit CPU core
 *
 * State: nothing implemented
 *
 * Copyright 2009 Jochen Karrer. All rights reserved.
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
#include "compiler_extensions.h"
#include "avr8_cpu.h"

#include <cycletimer.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <setjmp.h>
#include <sys/types.h>
#include <errno.h>

#include "avr8_io.h"
#include "sgstring.h"
#include "instructions_avr8.h"
#include "idecode_avr8.h"
#include "signode.h"
#include "fio.h"
#include "diskimage.h"
#include "cycletimer.h"
#include "configfile.h"
#include "mainloop_events.h"
#include "loader.h"

typedef struct AVR8_Variant {
	char *name;
	uint32_t flashwords;
	uint32_t srambytes;
	uint32_t io_registers;
	uint32_t sram_start;
	uint32_t eeprombytes;
	uint32_t pc_width;
	uint32_t nr_intvects;
} AVR8_Variant;

AVR8_Variant avr8_variants[] = {
	{
	 .name = "AT90S2313",
	 .flashwords = 1024,
	 .srambytes = 128,
	 .io_registers = 0x60,
	 .sram_start = 0x60,
	 .eeprombytes = 128,
	 .pc_width = 16,
	 },
	{
	 .name = "ATMega8",
	 .flashwords = 4096,
	 .srambytes = 1024,
	 .io_registers = 0x60,
	 .sram_start = 0x60,
	 .pc_width = 16,
	 },
	{
	 .name = "ATMega16",
	 .flashwords = 8192,
	 .srambytes = 1024,
	 .io_registers = 0x60,
	 .sram_start = 0x60,
	 .eeprombytes = 512,
	 .pc_width = 16,
	 },
	{
	 .name = "ATMega128",
	 .flashwords = 64 * 1024, /* 128kB */
	 .srambytes = 4096,
	 .io_registers = 0x100, /* ATMega103 compatibility mode not supported */
	 .sram_start = 0x100,
	 .eeprombytes = 4096,
	 .pc_width = 16,
	 .nr_intvects = 36,
	 },
	{
	 .name = "ATMega1280",
	 .flashwords = 64 * 1024,
	 .srambytes = 8192,
	 .io_registers = 0x200,
	 .sram_start = 0x200,
	 .eeprombytes = 4096,
	 .pc_width = 16,
	 },
	{
	 .name = "ATMega328",
	 .flashwords = 16 * 1024,
	 .srambytes = 2048,
	 .nr_intvects = 26,
     .io_registers = 0x100,
     .sram_start = 0x100,
     .eeprombytes = 1024,
     .pc_width = 16,
    },
	{
	 .name = "ATMega644",
	 .flashwords = 32 * 1024,
	 .srambytes = 4096,
	 .io_registers = 0x100,
	 .sram_start = 0x100,
	 .eeprombytes = 2048,
	 .pc_width = 16,
	 .nr_intvects = 31,
	 },
	{
	 .name = "ATMega2560",
	 .flashwords = 128 * 1024,
	 .srambytes = 8192,
	 .io_registers = 0x200,
	 .sram_start = 0x200,
	 .eeprombytes = 4096,
	 .pc_width = 24,
	 .nr_intvects = 57,
	 },
	{
	 .name = "ATXMega32a4",
	 .flashwords = 16 * 1024,
	 .srambytes = 4096,
	 .io_registers = 0x1000,
	 .sram_start = 0x2000,
	 .eeprombytes = 1024,
	 .pc_width = 16,
	 .nr_intvects = 94,
	 }
};

AVR8_Cpu gavr8;

#ifndef NO_DEBUGGER
/*
 * 32 Regs + SREG + SP + PC
 */
static void
debugger_setreg(void *clientData, const uint8_t * data, uint32_t index, int len)
{
	if (index < 32) {
		if (len != 1) {
			return;
		}
		AVR8_WriteReg(*data, index);
	} else if (index == 32) {
		/* SREG */
		if (len != 1) {
			return;
		}
		SET_SREG(*data);
		return;
	} else if (index == 33) {
		/* SP */
		uint16_t sp;
		if (len != 2) {
			return;
		}
		sp = data[0] | (data[1] << 8);
		SET_REG_SP(sp);
		return;
	} else if (index == 34) {
		/* PC */
		uint32_t pc;
		if (len != 4) {
			return;
		}
		pc = data[0] | (data[1] << 8) | (data[2] << 16) | (data[3] << 24);
		SET_REG_PC(pc >> 1);
	} else {
		return;
	}
	return;
}

static int
debugger_getreg(void *clientData, uint8_t * data, uint32_t index, int maxlen)
{
	int retval = 0;
	if (index < 32) {
		if (maxlen < 1) {
			return -EINVAL;
		}
		*data = AVR8_ReadReg(index);
		//fprintf(stderr,"Reg%d is 0x%02x\n",index,*data);
		retval = 1;
	} else if (index == 32) {
		/* SREG */
		if (maxlen < 1) {
			return -EINVAL;
		}
		*data = GET_SREG;
		//fprintf(stderr,"SReg%d is 0x%02x\n",index,*data);
		retval = 1;
	} else if (index == 33) {
		/* SP */
		if (maxlen < 3) {
			return -EINVAL;
		}
		data[0] = GET_REG_SP & 0xff;
		data[1] = (GET_REG_SP >> 8) & 0xff;
		retval = 2;
		//fprintf(stderr,"SP%d is 0x%04x or 0x%04x, \n",index,*(uint16_t*)data,GET_REG_SP);
	} else if (index == 34) {
		/* PC */
		uint32_t pc = GET_REG_PC << 1;
		if (maxlen < 4) {
			return -EINVAL;
		}
		data[0] = pc & 0xff;
		data[1] = (pc >> 8) & 0xff;
		data[2] = (pc >> 16) & 0xff;
		data[3] = (pc >> 24) & 0xff;
		retval = 4;
		//fprintf(stderr,"PC%d is 0x%02x\n",index,*(uint32_t*)data);
	} else {
		return 0;
	}
	return retval;
}

/* These are stolen from gdb */
#define DBG_AVR_IMEM_START   0x00000000	/* INSN memory */
#define DBG_AVR_SMEM_START   0x00800000	/* SRAM memory */
#define DBG_AVR_MEM_MASK     0x00f00000	/* mask to determine memory space */
#define DBG_AVR_EMEM_START   0x00810000	/* EEPROM memory */
#define DBG_AVR_EMEM_MASK     0x00ff0000	/* mask to determine memory space */

static ssize_t
debugger_getmem(void *clientData, uint8_t * data, uint64_t addr, uint32_t count)
{
	uint32_t i;
	/* catch exceptions from MMU */
	if ((addr & DBG_AVR_MEM_MASK) == DBG_AVR_IMEM_START) {
		for (i = 0; i < count; i++) {
			data[i] = AVR8_ReadAppMem8(addr + i);
		}
	} else if ((addr & DBG_AVR_MEM_MASK) == DBG_AVR_SMEM_START) {
		for (i = 0; i < count; i++) {
			data[i] = AVR8_ReadMem8(addr + i);
		}
	}
	return count;
}

static ssize_t
debugger_setmem(void *clientData, const uint8_t * data, uint64_t addr, uint32_t count)
{
	uint32_t i;
	/* catch exceptions from MMU */
	if ((addr & DBG_AVR_MEM_MASK) == DBG_AVR_IMEM_START) {
		for (i = 0; i < count; i++) {
			AVR8_WriteAppMem8(data[i], addr + i);
		}
	} else if ((addr & DBG_AVR_MEM_MASK) == DBG_AVR_SMEM_START) {
		for (i = 0; i < count; i++) {
			AVR8_WriteMem8(data[i], addr + i);
		}
	}
	return count;
}

static int
debugger_stop(void *clientData)
{

	gavr8.dbg_state = AVRDBG_STOP;
	AVR8_PostSignal(AVR8_SIG_DBG);
	return -1;
}

static int
debugger_cont(void *clientData)
{
	gavr8.dbg_state = AVRDBG_RUNNING;
	/* Should only be called if there are no breakpoints */
	AVR8_UnpostSignal(AVR8_SIG_DBG);
	return 0;
}

static int
debugger_step(void *clientData, uint64_t addr, int use_addr)
{
	if (use_addr) {
		SET_REG_PC(addr >> 1);
	}
	gavr8.dbg_steps = 1;
	gavr8.dbg_state = AVRDBG_STEP;
	return -1;
}

static Dbg_TargetStat
debugger_get_status(void *clientData)
{
	if (gavr8.dbg_state == AVRDBG_STOPPED) {
		return DbgStat_SIGINT;
	} else if (gavr8.dbg_state == AVRDBG_RUNNING) {
		return DbgStat_RUNNING;
	} else {
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
	if (len == 2) {
		/* AVR break is 0x9598 */
		ins[0] = 0x98;
		ins[1] = 0x95;
	}
}

static void
Do_Debug(void)
{
	if (gavr8.dbg_state == AVRDBG_RUNNING) {
		fprintf(stderr, "Debug mode is off, should not be called\n");
	} else if (gavr8.dbg_state == AVRDBG_STEP) {
		if (gavr8.dbg_steps == 0) {
			gavr8.dbg_state = AVRDBG_STOPPED;
			if (gavr8.debugger) {
				Debugger_Notify(gavr8.debugger, DbgStat_SIGTRAP);
			}
			AVR8_RestartIdecoder();
		} else {
			gavr8.dbg_steps--;
			/* Requeue event */
			mainloop_event_pending = 1;
		}
	} else if (gavr8.dbg_state == AVRDBG_STOP) {
		gavr8.dbg_state = AVRDBG_STOPPED;
		if (gavr8.debugger) {
			Debugger_Notify(gavr8.debugger, DbgStat_SIGINT);
		}
		AVR8_RestartIdecoder();
	} else if (gavr8.dbg_state == AVRDBG_BREAK) {
		if (gavr8.debugger) {
			if (Debugger_Notify(gavr8.debugger, DbgStat_SIGTRAP) > 0) {
				gavr8.dbg_state = AVRDBG_STOPPED;
				AVR8_RestartIdecoder();
			}	/* Else no debugger session open */
		} else {
			//AVR Exception break
			gavr8.dbg_state = AVRDBG_RUNNING;
		}
	} else {
		fprintf(stderr, "Unknown restart signal reason %d\n", gavr8.dbg_state);
	}
}
#endif

void
AVR8_UpdateCpuSignals(void)
{
	if (gavr8.sreg & FLG_I) {
		gavr8.cpu_signal_mask |= AVR8_SIG_IRQ;
	} else {
		gavr8.cpu_signal_mask &= ~AVR8_SIG_IRQ;
	}
	gavr8.cpu_signals = gavr8.cpu_signals_raw & gavr8.cpu_signal_mask;
	if (gavr8.cpu_signals) {
		mainloop_event_pending = 1;
	}
}

static void
AVR8_Interrupt(void *irqData)
{
	uint16_t sp = GET_REG_SP;
	uint32_t pc = GET_REG_PC;
	uint8_t sreg = GET_SREG;
	int irqvect;
	for (irqvect = 0; irqvect < gavr8.nr_intvects; irqvect++) {
		if (SigNode_Val(gavr8.irqNode[irqvect]) == SIG_LOW) {
			break;
		}
	}
	if (irqvect == gavr8.nr_intvects) {
		gavr8.cpu_signals_raw &= ~AVR8_SIG_IRQ;
		AVR8_UpdateCpuSignals();
		return;
	}
	SigNode_Set(gavr8.irqAckNode[irqvect], SIG_LOW);
	SigNode_Set(gavr8.irqAckNode[irqvect], SIG_HIGH);

	AVR8_WriteMem8(pc & 0xff, sp--);
	AVR8_WriteMem8((pc >> 8) & 0xff, sp--);
	if (gavr8.pc24bit) {
		AVR8_WriteMem8((pc >> 16) & 0xff, sp--);
		CycleCounter += 1;
	}
	sreg &= ~FLG_I;
	SET_SREG(sreg);		// SET SREG should be a proc with influence on cpu_signals
	AVR8_UpdateCpuSignals();
	SET_REG_SP(sp);
	CycleCounter += 4;
	SET_REG_PC(irqvect << 1);
}

static inline void
CheckSignals(void)
{
	if (unlikely(mainloop_event_pending)) {
		mainloop_event_pending = 0;
		if (mainloop_event_io) {
			FIO_HandleInput();
		}
		if (likely(gavr8.cpu_signals & AVR8_SIG_IRQ)) {
			gavr8.avrAckIrq(gavr8.avrIrqData);
		} else if (gavr8.cpu_signals & AVR8_SIG_IRQENABLE) {
			gavr8.cpu_signals_raw &= ~AVR8_SIG_IRQENABLE;
			AVR8_UpdateCpuSignals();
		}
#ifndef NO_DEBUGGER
		if (unlikely(gavr8.cpu_signals & AVR8_SIG_DBG)) {
			Do_Debug();
		}
#endif
		if (unlikely(gavr8.cpu_signals & AVR8_SIG_RESTART_IDEC)) {
			AVR8_RestartIdecoder();
		}
	}
}

static uint16_t pcbuf[1024];
static int pcbuf_wp = 0;
static int pcbuf_rp = 0;

static inline void
logPC(void)
{
	pcbuf[pcbuf_wp] = GET_REG_PC;
	pcbuf_wp = (pcbuf_wp + 1) & ((sizeof(pcbuf) / 2) - 1);
}

void
AVR8_DumpRegisters(void)
{
	int i;
	fprintf(stderr, "Register Dump, PC: %04x\n", GET_REG_PC << 1);
	for (i = 0; i < 32; i++) {
		fprintf(stderr, "R%02d: 0x%02x ", i, AVR8_ReadReg(i));
		if ((i & 7) == 7) {
			fprintf(stderr, "\n");
		}
	}
}

void
AVR8_DumpPcBuf(void)
{
	int i;
	pcbuf_rp = (pcbuf_wp - 200) & ((sizeof(pcbuf) / 2) - 1);
	for (i = 0; i < 200; i++) {
		fprintf(stderr, "PC: %04x\n", pcbuf[pcbuf_rp] << 1);
		pcbuf_rp = (pcbuf_rp + 1) & ((sizeof(pcbuf) / 2) - 1);
	}
	AVR8_DumpRegisters();
}

/*
 *******************************************************************
 * AVR8 CPU main loop
 *******************************************************************
 */
void
AVR8_Run()
{
	AVR8_Cpu *avr = &gavr8;
	uint32_t addr = 0;
	AVR8_InstructionProc *iproc;
	if (Config_ReadUInt32(&addr, "global", "start_address") < 0) {
		addr = 0;
	}
	SET_REG_PC(addr);
	setjmp(avr->restart_idec_jump);
	while (avr->dbg_state == AVRDBG_STOPPED) {
		struct timespec tout;
		tout.tv_nsec = 0;
		tout.tv_sec = 10000;
		FIO_WaitEventTimeout(&tout);
	}
	while (1) {
		CheckSignals();
		CycleTimers_Check();
		ICODE = AVR8_ReadAppMem(GET_REG_PC);
		//logPC();
		SET_REG_PC(GET_REG_PC + 1);
		iproc = AVR8_InstructionProcFind(ICODE);
		iproc();
	}
}

static uint8_t
avr8_read_unknown(void *clientData, uint32_t address)
{
	fprintf(stderr, "Unhandled read from IO %04x, PC 0x%04x\n", address, (GET_REG_PC << 1) - 2);
	return 0;
}

static void
avr8_write_unknown(void *clientData, uint8_t value, uint32_t address)
{
	fprintf(stderr, "Unhandled read from IO %04x, PC 0x%04x\n", address, (GET_REG_PC << 1) - 2);
}

static uint8_t
avr8_read_rampz(void *clientData, uint32_t address)
{
	return gavr8.rampz;
}

static void
avr8_write_rampz(void *clientData, uint8_t value, uint32_t address)
{
	gavr8.rampz = value;
}

static uint8_t
avr8_read_eind(void *clientData, uint32_t address)
{
	return gavr8.regEIND;
}

static void
avr8_write_eind(void *clientData, uint8_t value, uint32_t address)
{
	gavr8.regEIND = value;
}

void
AVR8_RegisterIOHandler(uint32_t addr, AVR8_IoReadProc * readproc, AVR8_IoWriteProc * writeproc,
		       void *clientData)
{

	AVR8_Iohandler *ioh;
	AVR8_Cpu *avr = &gavr8;
	if (addr >= avr->io_registers) {
		fprintf(stderr, "Bug: registering IO-Handler outside of IO address space of CPU: %u\n", 
            addr);
		exit(1);
	}
	if (readproc && (avr->mmioHandler[addr]->ioReadProc != avr8_read_unknown)) {
		fprintf(stderr, "Bug: IO-Handler for address 0x%04x already exists\n", addr);
		exit(1);
	}
	if (writeproc && (avr->mmioHandler[addr]->ioWriteProc != avr8_write_unknown)) {
		fprintf(stderr, "Bug: IO-Handler for address 0x%04x already exists\n", addr);
		exit(1);
	}
	ioh = avr->mmioHandler[addr];
	if (readproc) {
		ioh->ioReadProc = readproc;
	}
	if (writeproc) {
		ioh->ioWriteProc = writeproc;
	}
	ioh->clientData = clientData;
}

/*
 **************************************************************
 * Assume a low for an Interrupt and an high value for no
 * interrupt
 **************************************************************
 */
static void
AVR8_IrqTrace(SigNode * sig, int value, void *clientData)
{
	if (value == SIG_LOW) {
		AVR8_PostSignal(AVR8_SIG_IRQ);
	}
}

static uint8_t
avr8_read_reg(void *clientData, uint32_t address)
{
	return AVR8_ReadReg(address);
}

static void
avr8_write_reg(void *clientData, uint8_t value, uint32_t address)
{
	return AVR8_WriteReg(value, address);
}

static uint8_t
avr8_read_flags(void *clientData, uint32_t address)
{
	return GET_SREG;
}

static void
avr8_write_flags(void *clientData, uint8_t value, uint32_t address)
{
	SET_SREG(value);
	AVR8_UpdateCpuSignals();
}

static uint8_t
avr8_read_spl(void *clientData, uint32_t address)
{
	uint8_t spl;
	spl = GET_REG_SP & 0xff;
	return spl;
}

static void
avr8_write_spl(void *clientData, uint8_t value, uint32_t address)
{
	SET_REG_SP((GET_REG_SP & 0xffff00) | value);
}

static uint8_t
avr8_read_sph(void *clientData, uint32_t address)
{
	uint8_t sph = (GET_REG_SP >> 8) & 0xff;
	return sph;
}

static void
avr8_write_sph(void *clientData, uint8_t value, uint32_t address)
{
	SET_REG_SP((GET_REG_SP & 0xff00ff) | ((uint16_t) value << 8));
}

/*
 * -----------------------------------------------------
 * The interface to the loader
 * -----------------------------------------------------
 */
static int
load_to_bus(void *clientData, uint32_t addr, uint8_t * buf, unsigned int count, int flags)
{
	AVR8_Cpu *avr = (AVR8_Cpu *) clientData;
	uint32_t i;
	for (i = 0; i < count; i++) {
		uint32_t word;
		word = (addr + i) >> 1;
		if (word >= avr->appmem_words) {
			fprintf(stderr, "Loading past end of application memory\n");
			exit(1);
		}
		if ((addr + i) & 1) {
			avr->appmem[word] = (avr->appmem[word] & 0xff) | (buf[i] << 8);
		} else {
			avr->appmem[word] = (avr->appmem[word] & 0xff00) | buf[i];
		}
	}
	return 0;
}

CycleTimer exit_timer;

static void
avr_exit(void *clientData)
{
//      fprintf(stderr,"Timeout\n");
//      exit(0);
}

static void
AVR8_SignalLevelConflict(const char *msg)
{
	fprintf(stderr, "PC 0x%04x: %s\n", ((GET_REG_PC - 1) << 1), msg);
}

void
AVR8_RegisterIntco(void (*ackProc) (void *), void (*retiProc) (void *), void *eventData)
{
	gavr8.avrAckIrq = ackProc;
	gavr8.avrReti = retiProc;
	gavr8.avrIrqData = eventData;

}

static void
AVR8_Reti(void *eventData)
{
	SET_SREG(GET_SREG | FLG_I);
}

/*
 * ----------------------------------------------------------
 * AVR8_Init
 *      Initialize the CPU.
 * ----------------------------------------------------------
 */
void
AVR8_Init(const char *instancename)
{
	AVR8_Cpu *avr = &gavr8;
	AVR8_Variant *var;
	char *variantname;
	char *flashname;
	char *imagedir;
	uint32_t cpu_clock = 20000000;
	int nr_variants = sizeof(avr8_variants) / sizeof(AVR8_Variant);
	int i;
	variantname = Config_ReadVar(instancename, "variant");
	if (!variantname) {
		fprintf(stderr, "No CPU variant selected\n");
		exit(1);
	}
	fprintf(stderr, "%d variants\n", nr_variants);
	for (i = 0; i < nr_variants; i++) {
		var = &avr8_variants[i];
		if (strcmp(var->name, variantname) == 0) {
			break;
		}
	}
	if (i == nr_variants) {
		fprintf(stderr, "Unknown AVR8 CPU \"%s\"\n", variantname);
		exit(1);
	}
	memset(avr, 0, sizeof(*avr));
	imagedir = Config_ReadVar("global", "imagedir");
	if (!imagedir) {
		fprintf(stderr, "No directory given for AVR8 flash diskimage\n");
		exit(1);
	}
	flashname = alloca(strlen(instancename) + strlen(imagedir) + 20);
	sprintf(flashname, "%s/%s.flash", imagedir, instancename);
	avr->flash_di = DiskImage_Open(flashname, var->flashwords << 1, DI_RDWR | DI_CREAT_FF);
	if (!avr->flash_di) {
		fprintf(stderr, "Can not create or open the AVR internal flash image \"%s\"\n",
			flashname);
		exit(1);
	}
	avr->appmem = DiskImage_Mmap(avr->flash_di);
	avr->appmem_byte = (uint8_t *) avr->appmem;
	avr->appmem_words = var->flashwords;
	avr->appmem_word_mask = var->flashwords - 1;
	avr->appmem_byte_mask = (2 * var->flashwords) - 1;
	avr->sram_start = var->sram_start;
	avr->sram_end = var->sram_start + var->srambytes;
	avr->sram = sg_calloc(var->srambytes);
	avr->io_registers = var->io_registers;
	avr->pc24bit = (var->pc_width > 16) ? 1 : 0;
	avr->mmioHandler = (AVR8_Iohandler **)
	    sg_calloc(var->io_registers * sizeof(AVR8_Iohandler *));
	for (i = 0; i < var->io_registers; i++) {
		AVR8_Iohandler *ioh;
		ioh = avr->mmioHandler[i] = sg_new(AVR8_Iohandler);
		ioh->ioReadProc = avr8_read_unknown;
		ioh->ioWriteProc = avr8_write_unknown;
		ioh->clientData = avr;
	}
	avr->cpu_signals_raw = 0;
	avr->cpu_signal_mask = ~0;
	avr->cpu_signals = 0;
	avr->wdResetNode = SigNode_New("%s.wdReset", instancename);
	if (!avr->wdResetNode) {
		fprintf(stderr, "Can not create cpu signal lines node\n");
		exit(1);
	}
	SigNode_Set(avr->wdResetNode, SIG_HIGH);
	avr->nr_intvects = var->nr_intvects;
	avr->irqNode = (SigNode **) sg_calloc(var->nr_intvects * sizeof(SigNode *));
	avr->irqAckNode = (SigNode **) sg_calloc(var->nr_intvects * sizeof(SigNode *));
	for (i = 0; i < var->nr_intvects; i++) {
		avr->irqNode[i] = SigNode_New("%s.irq%d", instancename, i);
		avr->irqAckNode[i] = SigNode_New("%s.irqAck%d", instancename, i);
		if (!avr->irqNode[i] || !avr->irqAckNode[i]) {
			fprintf(stderr, "Can not create CPU interrupt lines\n");
			exit(1);
		}
		SigNode_Set(avr->irqNode[i], SIG_PULLUP);
		SigNode_Set(avr->irqAckNode[i], SIG_PULLUP);
		SigNode_Trace(avr->irqNode[i], AVR8_IrqTrace, avr);
	}
	if (var->pc_width > 16) { 
	    AVR8_IDecoderNew(AVR8_VARIANT_PC24);
    } else {
	    AVR8_IDecoderNew(AVR8_VARIANT_PC16);
    }
	AVR8_InitInstructions(avr);
	Config_ReadUInt32(&cpu_clock, "global", "cpu_clock");
	CycleTimers_Init(instancename, cpu_clock);
	for (i = 0; i < 32; i++) {
		AVR8_RegisterIOHandler(i, avr8_read_reg, avr8_write_reg, avr);
	}
	AVR8_RegisterIOHandler(IOA_SREG, avr8_read_flags, avr8_write_flags, avr);
	AVR8_RegisterIOHandler(IOA_SPL, avr8_read_spl, avr8_write_spl, avr);
	AVR8_RegisterIOHandler(IOA_SPH, avr8_read_sph, avr8_write_sph, avr);
	AVR8_RegisterIOHandler(IOA_RAMPZ, avr8_read_rampz, avr8_write_rampz, avr);
	AVR8_RegisterIOHandler(IOA_EIND, avr8_read_eind, avr8_write_eind, avr);
	Loader_RegisterBus("bus", load_to_bus, avr);
	SET_REG_SP(avr->sram_end - 1);
	SET_SREG(0);		/* ??? */
	avr->throttle = Throttle_New(instancename);
	CycleTimer_Add(&exit_timer, CycleTimerRate_Get() * 30, avr_exit, avr);
	Signodes_SetConflictProc(AVR8_SignalLevelConflict);
	avr->avrAckIrq = AVR8_Interrupt;
	avr->avrReti = AVR8_Reti;
	avr->avrIrqData = avr;
	avr->dbg_state = AVRDBG_RUNNING;
#ifndef NO_DEBUGGER
	avr->dbgops.getreg = debugger_getreg;
	avr->dbgops.setreg = debugger_setreg;
	avr->dbgops.stop = debugger_stop;
	avr->dbgops.cont = debugger_cont;
	avr->dbgops.get_status = debugger_get_status;
	avr->dbgops.getmem = debugger_getmem;
	avr->dbgops.setmem = debugger_setmem;
	avr->dbgops.step = debugger_step;
	avr->dbgops.get_bkpt_ins = debugger_get_bkpt_ins;
	avr->debugger = Debugger_New(&avr->dbgops, avr);
#endif
}
