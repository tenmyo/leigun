/*
 *************************************************************************************************
 *
 * Emulation of the Infineon C16x CPU 
 *
 * State:
 *      Untested, Interrupts are missing 
 *
 * Copyright 2005 Jochen Karrer. All rights reserved.
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
#include <stdint.h>
#include "cycletimer.h"
#include "instructions_c16x.h"
#include "idecode_c16x.h"
#include "c16x/c16x_cpu.h"
#include "configfile.h"
#include "mainloop_events.h"
#include "fio.h"

C16x gc16x;

uint16_t c16x_signals_raw = 0;
uint16_t c16x_signal_mask = 0;
uint16_t c16x_signals = 0;

static void
C16x_Reset(C16x * c16x)
{
	int i;
	REG_CP = 0xfc00;
	c16x->stkun = 0xfc00;
	c16x->stkov = 0xfa00;
	REG_SP = 0xfc00;
	c16x->wdtcon = 0x00;	// depends on reset config
	for (i = 0; i < 4; i++) {
		REG_DPP(i) = i;
	}
	//c16x->s0rbuf = undefined;
	//c16x->sscrb = undefined;
	//c16x->syscon = reset config
	//c16x->buscon0 = reset config
	//c16x->rp0h = reset config
}

/*
 * -------------------------------------------
 * default read handler for sfr
 * 	some of them are overwritten by a more
 *	specific handler
 * -------------------------------------------
 */

static uint32_t
c16x_sfr_read(void *clientData, uint32_t address, int rqlen)
{
	//C16x *c16x = clientData;
	//uint16_t reg = (address & 0x1ff);
	fprintf(stderr, "Unhandled SFR_Read from %08x\n", address);
	return 0;
}

static void
c16x_sfr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	//C16x *c16x = clientData;
	//uint16_t reg = (address & 0x1ff);
	fprintf(stderr, "Unhandled SFR_WRITE to %08x\n", address);

}

static uint32_t
c16x_esfr_read(void *clientData, uint32_t address, int rqlen)
{
	//C16x *c16x = clientData;
	//uint16_t reg = (address & 0x1ff);
	fprintf(stderr, "Unhandled ESFR Read from %08x\n", address);
	return 0;
}

static void
c16x_esfr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	//C16x *c16x = clientData;
	//uint16_t reg = (address & 0x1ff);
	fprintf(stderr, "Unhandled ESFR_Write to %08x\n", address);

}

static inline void
modify_masked(uint16_t * reg, uint32_t value, uint32_t address, int rqlen)
{
	if (unlikely(rqlen == 1)) {
		if (address & 1) {
			*reg = (*reg & ~0x00ff) | (value << 8);
		} else {
			*reg = (*reg & ~0x00ff) | (value << 8);
		}
	} else {
		if (unlikely(address & 1)) {
			fprintf(stderr, "misaligned register write, address %08x\n", address);
		} else {
			*reg = value;
		}
	}
}

static inline uint32_t
read_masked(uint16_t * reg, uint32_t address, int rqlen)
{
	if (unlikely(rqlen == 1)) {
		if (address & 1) {
			return *reg >> 1;
		} else {
			return *reg & 0xff;
		}
	} else {
		if (unlikely(address & 1)) {
			fprintf(stderr, "misaligned register read address %08x\n", address);
			return 0;
		} else {
			return *reg;
		}
	}
}

static uint32_t
dpp_read(void *clientData, uint32_t address, int rqlen)
{
	C16x *c16x = (C16x *) clientData;
	int dpp_nr = (address >> 1) & 3;
	return read_masked(&c16x->dpp[dpp_nr], address, rqlen);
}

static void
dpp_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	C16x *c16x = (C16x *) clientData;
	int dpp_nr = (address >> 1) & 3;
	modify_masked(&c16x->dpp[dpp_nr], value, address, rqlen);
}

static uint32_t
cp_read(void *clientData, uint32_t address, int rqlen)
{
	C16x *c16x = (C16x *) clientData;
	return read_masked(&c16x->cp, address, rqlen);
}

static void
cp_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	C16x *c16x = (C16x *) clientData;
	modify_masked(&c16x->cp, value, address, rqlen);
}

static uint32_t
psw_read(void *clientData, uint32_t address, int rqlen)
{
	C16x *c16x = (C16x *) clientData;
	return read_masked(&c16x->psw, address, rqlen);
}

static void
psw_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	C16x *c16x = (C16x *) clientData;
	modify_masked(&c16x->psw, value, address, rqlen);
}

static uint32_t
sp_read(void *clientData, uint32_t address, int rqlen)
{
	C16x *c16x = (C16x *) clientData;
	return read_masked(&c16x->sp, address, rqlen);
}

static void
sp_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	C16x *c16x = (C16x *) clientData;
	modify_masked(&c16x->sp, value, address, rqlen);
}

static uint32_t
csp_read(void *clientData, uint32_t address, int rqlen)
{
	C16x *c16x = (C16x *) clientData;
	return read_masked(&c16x->csp, address, rqlen);
}

static void
csp_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	C16x *c16x = (C16x *) clientData;
	modify_masked(&c16x->csp, value, address, rqlen);
}

static uint32_t
mdl_read(void *clientData, uint32_t address, int rqlen)
{
	C16x *c16x = (C16x *) clientData;
	return read_masked(&c16x->mdl, address, rqlen);
}

static void
mdl_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	C16x *c16x = (C16x *) clientData;
	modify_masked(&c16x->mdl, value, address, rqlen);
}

static uint32_t
mdh_read(void *clientData, uint32_t address, int rqlen)
{
	C16x *c16x = (C16x *) clientData;
	return read_masked(&c16x->mdh, address, rqlen);
}

static void
mdh_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	C16x *c16x = (C16x *) clientData;
	modify_masked(&c16x->mdh, value, address, rqlen);
}

static uint32_t
syscon_read(void *clientData, uint32_t address, int rqlen)
{
	C16x *c16x = (C16x *) clientData;
	return read_masked(&c16x->syscon, address, rqlen);
}

static void
syscon_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	C16x *c16x = (C16x *) clientData;
	modify_masked(&c16x->syscon, value, address, rqlen);
}

static uint32_t
stkun_read(void *clientData, uint32_t address, int rqlen)
{
	C16x *c16x = (C16x *) clientData;
	return read_masked(&c16x->stkun, address, rqlen);
}

static void
stkun_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	C16x *c16x = (C16x *) clientData;
	modify_masked(&c16x->stkun, value, address, rqlen);
	// check if underflow and trigger trap
}

static uint32_t
stkov_read(void *clientData, uint32_t address, int rqlen)
{
	C16x *c16x = (C16x *) clientData;
	return read_masked(&c16x->stkov, address, rqlen);
}

static void
stkov_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	C16x *c16x = (C16x *) clientData;
	modify_masked(&c16x->stkov, value, address, rqlen);
	// check if overflow and trigger trap
}

static uint32_t
wdtcon_read(void *clientData, uint32_t address, int rqlen)
{
	C16x *c16x = (C16x *) clientData;
	return read_masked(&c16x->wdtcon, address, rqlen);
}

static void
wdtcon_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	C16x *c16x = (C16x *) clientData;
	modify_masked(&c16x->wdtcon, value, address, rqlen);
	// update wdt
}

/*
 * -------------------------------------------
 * User Manual Chapter 22 System reset
 * -------------------------------------------
 */
C16x *
C16x_New()
{
	C16x *c16x = &gc16x;
	memset(c16x, 0, sizeof(C16x));
	//REG_SFR(SFR_ONES) = 0xffff; 
	C16x_IDecoderNew();
	C16x_InitInstructions();
	IOH_NewRegion(SFR_BASE, 512, c16x_sfr_read, c16x_sfr_write, TARGET_BYTEORDER, c16x);
	IOH_NewRegion(ESFR_BASE, 512, c16x_esfr_read, c16x_esfr_write, TARGET_BYTEORDER, c16x);

	IOH_New16(SFR_ADDR(SFR_DPP0), dpp_read, dpp_write, c16x);
	IOH_New16(SFR_ADDR(SFR_DPP1), dpp_read, dpp_write, c16x);
	IOH_New16(SFR_ADDR(SFR_DPP2), dpp_read, dpp_write, c16x);
	IOH_New16(SFR_ADDR(SFR_DPP3), dpp_read, dpp_write, c16x);
	IOH_New16(SFR_ADDR(SFR_CP), cp_read, cp_write, c16x);
	IOH_New16(SFR_ADDR(SFR_PSW), psw_read, psw_write, c16x);
	//IOH_New16(SFR_ADDR(SFR_IP),ip_read,ip_write,c16x);
	IOH_New16(SFR_ADDR(SFR_SP), sp_read, sp_write, c16x);
	IOH_New16(SFR_ADDR(SFR_CSP), csp_read, csp_write, c16x);
	IOH_New16(SFR_ADDR(SFR_MDL), mdl_read, mdl_write, c16x);
	IOH_New16(SFR_ADDR(SFR_MDH), mdh_read, mdh_write, c16x);
	IOH_New16(SFR_ADDR(SFR_SYSCON), syscon_read, syscon_write, c16x);
	IOH_New16(SFR_ADDR(SFR_STKUN), stkun_read, stkun_write, c16x);
	IOH_New16(SFR_ADDR(SFR_STKOV), stkov_read, stkov_write, c16x);
	IOH_New16(SFR_ADDR(SFR_WDTCON), wdtcon_read, wdtcon_write, c16x);

	C16x_Reset(c16x);
	C16x_MemRead16(0xff80);
	return c16x;
}

/*
 * ----------------------------------------
 * Fetch an instruction from IP/CSP
 * and decode it
 * ----------------------------------------
 */
static inline C16x_Instruction *
ifetch_and_decode(uint8_t * icodeP)
{
	C16x_Instruction *instr;
	uint32_t addr = REG_IP | (REG_CSP << 16);
	uint16_t ip = addr & 0xffff;
	uint16_t seg = addr >> 16;
	icodeP[0] = Bus_Read8(addr);
	icodeP[1] = Bus_Read8(((ip + 1) & 0xffff) | (seg << 16));
	instr = C16x_FindInstruction(icodeP[0]);
	if (instr->len == 4) {
		icodeP[2] = Bus_Read8(((ip + 2) & 0xffff) | (seg << 16));
		icodeP[3] = Bus_Read8(((ip + 3) & 0xffff) | (seg << 16));
	}
	return instr;
}

void
c16x_update_interrupts()
{

}

static inline void
CheckSignals()
{
	if (unlikely(mainloop_event_pending)) {
		mainloop_event_pending = 0;
		if (mainloop_event_io) {
			FIO_HandleInput();
		}
	}
}

/*
 *
 */
void
C16x_PostIPL(int ipl)
{
	C16x *c16x = &gc16x;
	if (ipl > c16x->ipl) {

	}
//      cpu->posted_ipl = ipl;
}

/*
 * --------------------------------------
 * The CPU main loop
 * --------------------------------------
 */
void
C16x_Run()
{
	uint32_t start_address;
	uint8_t icodeP[4];
	C16x_Instruction *instr;
	C16x *c16x = &gc16x;
	if (Config_ReadUInt32(&start_address, "global", "start_address") < 0) {
		start_address = 0;
	}
	fprintf(stderr, "Starting Infineon C16x CPU at %08x\n", start_address);

	c16x->ip = start_address;
	while (1) {
		instr = ifetch_and_decode(icodeP);
		fprintf(stderr, "Doing instruction %s at %04x-%04x\n", instr->name, REG_CSP,
			REG_IP);
		REG_IP = REG_IP + instr->len;
		instr->proc(icodeP);
		CycleCounter += 3;
		CycleTimers_Check();
		if (c16x->lock_counter) {
			c16x->lock_counter--;
		} else {
			c16x->extmode = 0;
			CheckSignals();
		}
	}
}

#if 0
void
_init()
{
	fprintf(stderr, "Infineon C16x emulation module loaded\n");
}
#endif
