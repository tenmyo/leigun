/*
 *************************************************************************************************
 *
 * Emulation of ATMegaXX4 System control and reset module 
 *
 * state: Very incomplete, only enabling System watchdog reset works 
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

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include "sgstring.h"
#include "clock.h"
#include "signode.h"
#include "avr8_io.h"
#include "avr8_cpu.h"
#include "atm644_sysreset.h"

#define REG_MCUSR(base)		(0x54)
#define 	MCUSR_JTRF    (1 << 4)
#define 	MCUSR_WDRF    (1 << 3)
#define 	MCUSR_BORF    (1 << 2)
#define 	MCUSR_EXTRF   (1 << 1)
#define 	MCUSR_PORF    (1 << 0)

#define REG_WDTCSR(base)	(0x60)
#define 	WDTCSR_WDIF    (1 << 7)
#define 	WDTCSR_WDIE    (1 << 6)
#define 	WDTCSR_WDP3    (1 << 5)
#define 	WDTCSR_WDCE    (1 << 4)
#define 	WDTCSR_WDE     (1 << 3)
#define 	WDTCSR_WDP2    (1 << 2)
#define 	WDTCSR_WDP1    (1 << 1)
#define 	WDTCSR_WDP0    (1 << 0)

typedef struct ATM644_SR {
	Clock_t *wdOsc;
	CycleTimer wd_timer;
	SigNode *wdIrqNode;
	SigNode *wdResetNode;
	uint8_t reg_mcusr;
	uint8_t reg_wdtcsr;
} ATM644_SR;

static void
restart_watchdog(ATM644_SR * sr)
{
	uint64_t timeout;
	int wdp;
	uint32_t prescaler;
	if (!(sr->reg_wdtcsr & WDTCSR_WDE)) {
		CycleTimer_Remove(&sr->wd_timer);
		return;
	}
	wdp = (sr->reg_wdtcsr & 7) | ((sr->reg_wdtcsr >> 2) & 8);
	switch (wdp) {
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
		    prescaler = 2048 << wdp;
		    break;
	    default:
		    fprintf(stderr, "Illegal prescaler value for watchdog\n");
		    prescaler = 1;
	}
	timeout = prescaler * (uint64_t) CycleTimerRate_Get() / Clock_DFreq(sr->wdOsc);
	CycleTimer_Mod(&sr->wd_timer, timeout);
}

static uint8_t
mcusr_read(void *clientData, uint32_t address)
{
	ATM644_SR *sr = (ATM644_SR *) clientData;
	return sr->reg_mcusr;
}

static void
mcusr_write(void *clientData, uint8_t value, uint32_t address)
{
	ATM644_SR *sr = (ATM644_SR *) clientData;
	sr->reg_mcusr = value;
	return;
}

static uint8_t
wdtcsr_read(void *clientData, uint32_t address)
{
	ATM644_SR *sr = (ATM644_SR *) clientData;
	return sr->reg_wdtcsr;
}

static void
wdtcsr_write(void *clientData, uint8_t value, uint32_t address)
{
	ATM644_SR *sr = (ATM644_SR *) clientData;
	sr->reg_wdtcsr = value;
	if (value & WDTCSR_WDE) {
		if (!CycleTimer_IsActive(&sr->wd_timer)) {
			restart_watchdog(sr);
		}
	}
	return;
}

static void
wd_timeout(void *clientData)
{
	ATM644_SR *sr = (ATM644_SR *) clientData;
	if (sr->reg_wdtcsr & WDTCSR_WDE) {
		fprintf(stderr, "Watchdog expired: reset\n");
		exit(1);
	}

}

static void
wd_reset(SigNode * node, int value, void *clientData)
{

	ATM644_SR *sr = (ATM644_SR *) clientData;
	if (value != SIG_LOW) {
		return;
	}
	restart_watchdog(sr);
}

void
ATM644_SRNew(const char *name)
{
	ATM644_SR *sr = sg_new(ATM644_SR);
	AVR8_RegisterIOHandler(REG_MCUSR(base), mcusr_read, mcusr_write, sr);
	AVR8_RegisterIOHandler(REG_WDTCSR(base), wdtcsr_read, wdtcsr_write, sr);
	sr->wdIrqNode = SigNode_New("%s.wdIrq", name);
	sr->wdResetNode = SigNode_New("%s.wdReset", name);
	sr->reg_mcusr = MCUSR_PORF;
	if (!sr->wdIrqNode || !sr->wdResetNode) {
		fprintf(stderr, "Can not create System/Reset module control lines\n");
		exit(1);
	}
	SigNode_Set(sr->wdIrqNode, SIG_PULLUP);
	SigNode_Set(sr->wdResetNode, SIG_OPEN);
	sr->wdOsc = Clock_New("%s.wdClock", name);
	if (!sr->wdOsc) {
		fprintf(stderr, "Creating the Watchdog clock failed\n");
		exit(1);
	}
	Clock_SetFreq(sr->wdOsc, 128000);
	SigNode_Trace(sr->wdResetNode, wd_reset, sr);
	CycleTimer_Init(&sr->wd_timer, wd_timeout, sr);

}
