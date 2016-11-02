/*
 *************************************************************************************************
 *
 * Atmel XMega-A programmable interrupt controller (PMIC)
 *
 * State: Minimal implementation that allows Software startup. 
 *
 * Copyright 2010 Jochen Karrer. All rights reserved.
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
#include <stdio.h>
#include <string.h>
#include "sgstring.h"
#include "serial.h"
#include "clock.h"
#include "cycletimer.h"
#include "signode.h"
#include "xmegaA_clock.h"
#include "avr8_cpu.h"

#define CLOCK_CTRL(base) 	((base) + 0x00)
#define CLOCK_PSCTRL(base)	((base) + 0x01)
#define CLOCK_LOCK(base)	((base) + 0x02)
#define CLOCK_RTCCTRL(base)	((base) + 0x03)

#define OSC_CTRL(base)		((base) + 0x00)
#define OSC_STATUS(base)	((base) + 0x01)
#define OSC_XOSCCTRL(base)	((base) + 0x02)
#define OSC_XOSCFAIL(base)	((base) + 0x03)
#define OSC_RC32KCAL(base)	((base) + 0x04)
#define OSC_PLLCTRL(base)	((base) + 0x05)
#define OSC_DFLLCTRL(base) 	((base) + 0x06)

#define DFLL_CTRL(base)		((base) + 0x00)
#define DFLL_CALA(base)		((base) + 0x02)
#define DFLL_CALB(base)		((base) + 0x03)
#define DFLL_COMP0(base)	((base) + 0x04)
#define DFLL_COMP1(base)	((base) + 0x05)
#define DFLL_COMP2(base)	((base) + 0x06)
#define DFLL_COMP2H(base)	((base) + 0x07)

typedef struct BaseAddr {
	int baClk;
	int baOsc;
	int baDfll32m;
	int baDfll2m;
} BaseAddr;

typedef struct ClockMod {
	uint8_t regOscCtrl;
	uint8_t regOscStatus;
} ClockMod;

BaseAddr base_addrs[] =  {
	{
		.baClk = 0x40,
		.baOsc = 0x50,
		.baDfll32m = 0x60,
		.baDfll2m = 0x68
	}
};

static uint8_t
clock_ctrl_read(void *clientData,uint32_t address)
{
	fprintf(stderr,"%s not implemented\n",__func__);
        return 0;
}

static void
clock_ctrl_write(void *clientData,uint8_t value,uint32_t address)
{
	fprintf(stderr,"%s not implemented\n",__func__);
}

static uint8_t
clock_psctrl_read(void *clientData,uint32_t address)
{
	fprintf(stderr,"%s not implemented\n",__func__);
        return 0;
}

static void
clock_psctrl_write(void *clientData,uint8_t value,uint32_t address)
{
	fprintf(stderr,"%s not implemented\n",__func__);
}

static uint8_t
clock_lock_read(void *clientData,uint32_t address)
{
	fprintf(stderr,"%s not implemented\n",__func__);
        return 0;
}

static void
clock_lock_write(void *clientData,uint8_t value,uint32_t address)
{
	fprintf(stderr,"%s not implemented\n",__func__);
}

static uint8_t
clock_rtcctrl_read(void *clientData,uint32_t address)
{
	fprintf(stderr,"%s not implemented\n",__func__);
        return 0;
}

static void
clock_rtcctrl_write(void *clientData,uint8_t value,uint32_t address)
{
	fprintf(stderr,"%s not implemented\n",__func__);
}

static uint8_t
osc_ctrl_read(void *clientData,uint32_t address)
{
	ClockMod *ck = clientData;	
	return ck->regOscCtrl;
}

static void
osc_ctrl_write(void *clientData,uint8_t value,uint32_t address)
{
	ClockMod *ck = clientData;	
	ck->regOscCtrl = ck->regOscStatus = value & 0x1f;
}

static uint8_t
osc_status_read(void *clientData,uint32_t address)
{
	ClockMod *ck = clientData;	
	return ck->regOscStatus;
}

static void
osc_status_write(void *clientData,uint8_t value,uint32_t address)
{
	fprintf(stderr,"%s not implemented\n",__func__);
}

static uint8_t
osc_xoscctrl_read(void *clientData,uint32_t address)
{
	fprintf(stderr,"%s not implemented\n",__func__);
        return 0;
}

static void
osc_xoscctrl_write(void *clientData,uint8_t value,uint32_t address)
{
	fprintf(stderr,"%s not implemented\n",__func__);
}

static uint8_t
osc_xoscfail_read(void *clientData,uint32_t address)
{
	fprintf(stderr,"%s not implemented\n",__func__);
        return 0;
}

static void
osc_xoscfail_write(void *clientData,uint8_t value,uint32_t address)
{
	fprintf(stderr,"%s not implemented\n",__func__);
}

static uint8_t
osc_rc32kcal_read(void *clientData,uint32_t address)
{
	fprintf(stderr,"%s not implemented\n",__func__);
        return 0;
}

static void
osc_rc32kcal_write(void *clientData,uint8_t value,uint32_t address)
{
	fprintf(stderr,"%s not implemented\n",__func__);
}

static uint8_t
osc_pllctrl_read(void *clientData,uint32_t address)
{
	fprintf(stderr,"%s not implemented\n",__func__);
        return 0;
}

static void
osc_pllctrl_write(void *clientData,uint8_t value,uint32_t address)
{
	fprintf(stderr,"%s not implemented\n",__func__);
}

static uint8_t
osc_dfllctrl_read(void *clientData,uint32_t address)
{
	fprintf(stderr,"%s not implemented\n",__func__);
        return 0;
}

static void
osc_dfllctrl_write(void *clientData,uint8_t value,uint32_t address)
{
	fprintf(stderr,"%s not implemented\n",__func__);
}


static uint8_t
dfll32m_ctrl_read(void *clientData,uint32_t address)
{
	fprintf(stderr,"%s not implemented\n",__func__);
        return 0;
}

static void
dfll32m_ctrl_write(void *clientData,uint8_t value,uint32_t address)
{
	fprintf(stderr,"%s not implemented\n",__func__);
}

static uint8_t
dfll32m_cala_read(void *clientData,uint32_t address)
{
	fprintf(stderr,"%s not implemented\n",__func__);
        return 0;
}

static void
dfll32m_cala_write(void *clientData,uint8_t value,uint32_t address)
{
	fprintf(stderr,"%s not implemented\n",__func__);
}

static uint8_t
dfll32m_calb_read(void *clientData,uint32_t address)
{
	fprintf(stderr,"%s not implemented\n",__func__);
        return 0;
}

static void
dfll32m_calb_write(void *clientData,uint8_t value,uint32_t address)
{
	fprintf(stderr,"%s not implemented\n",__func__);
}

static uint8_t
dfll32m_comp0_read(void *clientData,uint32_t address)
{
	fprintf(stderr,"%s not implemented\n",__func__);
        return 0;
}

static void
dfll32m_comp0_write(void *clientData,uint8_t value,uint32_t address)
{
	fprintf(stderr,"%s not implemented\n",__func__);
}

static uint8_t
dfll32m_comp1_read(void *clientData,uint32_t address)
{
	fprintf(stderr,"%s not implemented\n",__func__);
        return 0;
}

static void
dfll32m_comp1_write(void *clientData,uint8_t value,uint32_t address)
{
	fprintf(stderr,"%s not implemented\n",__func__);
}

static uint8_t
dfll32m_comp2_read(void *clientData,uint32_t address)
{
	fprintf(stderr,"%s not implemented\n",__func__);
        return 0;
}

static void
dfll32m_comp2_write(void *clientData,uint8_t value,uint32_t address)
{
	fprintf(stderr,"%s not implemented\n",__func__);
}

static uint8_t
dfll32m_comp2h_read(void *clientData,uint32_t address)
{
	fprintf(stderr,"%s not implemented\n",__func__);
        return 0;
}

static void
dfll32m_comp2h_write(void *clientData,uint8_t value,uint32_t address)
{
	fprintf(stderr,"%s not implemented\n",__func__);
}

static uint8_t
dfll2m_ctrl_read(void *clientData,uint32_t address)
{
	fprintf(stderr,"%s not implemented\n",__func__);
        return 0;
}

static void
dfll2m_ctrl_write(void *clientData,uint8_t value,uint32_t address)
{
	fprintf(stderr,"%s not implemented\n",__func__);
}

static uint8_t
dfll2m_cala_read(void *clientData,uint32_t address)
{
	fprintf(stderr,"%s not implemented\n",__func__);
        return 0;
}

static void
dfll2m_cala_write(void *clientData,uint8_t value,uint32_t address)
{
	fprintf(stderr,"%s not implemented\n",__func__);
}

static uint8_t
dfll2m_calb_read(void *clientData,uint32_t address)
{
	fprintf(stderr,"%s not implemented\n",__func__);
        return 0;
}

static void
dfll2m_calb_write(void *clientData,uint8_t value,uint32_t address)
{
	fprintf(stderr,"%s not implemented\n",__func__);
}

static uint8_t
dfll2m_comp0_read(void *clientData,uint32_t address)
{
	fprintf(stderr,"%s not implemented\n",__func__);
        return 0;
}

static void
dfll2m_comp0_write(void *clientData,uint8_t value,uint32_t address)
{
	fprintf(stderr,"%s not implemented\n",__func__);
}

static uint8_t
dfll2m_comp1_read(void *clientData,uint32_t address)
{
	fprintf(stderr,"%s not implemented\n",__func__);
        return 0;
}

static void
dfll2m_comp1_write(void *clientData,uint8_t value,uint32_t address)
{
	fprintf(stderr,"%s not implemented\n",__func__);
}
static uint8_t
dfll2m_comp2_read(void *clientData,uint32_t address)
{
	fprintf(stderr,"%s not implemented\n",__func__);
        return 0;
}

static void
dfll2m_comp2_write(void *clientData,uint8_t value,uint32_t address)
{
	fprintf(stderr,"%s not implemented\n",__func__);
}

static uint8_t
dfll2m_comp2h_read(void *clientData,uint32_t address)
{
	fprintf(stderr,"%s not implemented\n",__func__);
        return 0;
}

static void
dfll2m_comp2h_write(void *clientData,uint8_t value,uint32_t address)
{
	fprintf(stderr,"%s not implemented\n",__func__);
}

void
XMegaA_ClockNew(const char *name,unsigned int variant)
{
	BaseAddr *addrs;
	ClockMod *mod;
	if(variant >= array_size(base_addrs)) {
		fprintf(stderr,"Illegal variant %d for Xmega clock module variant\n",variant);
		exit(1);
	}
	mod = sg_new(ClockMod);
	addrs = &base_addrs[variant];
        AVR8_RegisterIOHandler(CLOCK_CTRL(addrs->baClk),clock_ctrl_read,clock_ctrl_write,mod);
        AVR8_RegisterIOHandler(CLOCK_PSCTRL(addrs->baClk),clock_psctrl_read,clock_psctrl_write,mod);
        AVR8_RegisterIOHandler(CLOCK_LOCK(addrs->baClk),clock_lock_read,clock_lock_write,mod);
        AVR8_RegisterIOHandler(CLOCK_RTCCTRL(addrs->baClk),clock_rtcctrl_read,clock_rtcctrl_write,mod);

        AVR8_RegisterIOHandler(OSC_CTRL(addrs->baOsc),osc_ctrl_read,osc_ctrl_write,mod);
        AVR8_RegisterIOHandler(OSC_STATUS(addrs->baOsc),osc_status_read,osc_status_write,mod);
        AVR8_RegisterIOHandler(OSC_XOSCCTRL(addrs->baOsc),osc_xoscctrl_read,osc_xoscctrl_write,mod);
        AVR8_RegisterIOHandler(OSC_XOSCFAIL(addrs->baOsc),osc_xoscfail_read,osc_xoscfail_write,mod);
        AVR8_RegisterIOHandler(OSC_RC32KCAL(addrs->baOsc),osc_rc32kcal_read,osc_rc32kcal_write,mod);
        AVR8_RegisterIOHandler(OSC_PLLCTRL(addrs->baOsc),osc_pllctrl_read,osc_pllctrl_write,mod);
        AVR8_RegisterIOHandler(OSC_DFLLCTRL(addrs->baOsc),osc_dfllctrl_read,osc_dfllctrl_write,mod);
	
        AVR8_RegisterIOHandler(DFLL_CTRL(addrs->baDfll32m),dfll32m_ctrl_read,dfll32m_ctrl_write,mod);
        AVR8_RegisterIOHandler(DFLL_CALA(addrs->baDfll32m),dfll32m_cala_read,dfll32m_cala_write,mod);
        AVR8_RegisterIOHandler(DFLL_CALB(addrs->baDfll32m),dfll32m_calb_read,dfll32m_calb_write,mod);
        AVR8_RegisterIOHandler(DFLL_COMP0(addrs->baDfll32m),dfll32m_comp0_read,dfll32m_comp0_write,mod);
        AVR8_RegisterIOHandler(DFLL_COMP1(addrs->baDfll32m),dfll32m_comp1_read,dfll32m_comp1_write,mod);
        AVR8_RegisterIOHandler(DFLL_COMP2(addrs->baDfll32m),dfll32m_comp2_read,dfll32m_comp2_write,mod);
        AVR8_RegisterIOHandler(DFLL_COMP2H(addrs->baDfll32m),dfll32m_comp2h_read,dfll32m_comp2h_write,mod);

        AVR8_RegisterIOHandler(DFLL_CTRL(addrs->baDfll2m),dfll2m_ctrl_read,dfll2m_ctrl_write,mod);
        AVR8_RegisterIOHandler(DFLL_CALA(addrs->baDfll2m),dfll2m_cala_read,dfll2m_cala_write,mod);
        AVR8_RegisterIOHandler(DFLL_CALB(addrs->baDfll2m),dfll2m_calb_read,dfll2m_calb_write,mod);
        AVR8_RegisterIOHandler(DFLL_COMP0(addrs->baDfll2m),dfll2m_comp0_read,dfll2m_comp0_write,mod);
        AVR8_RegisterIOHandler(DFLL_COMP1(addrs->baDfll2m),dfll2m_comp1_read,dfll2m_comp1_write,mod);
        AVR8_RegisterIOHandler(DFLL_COMP2(addrs->baDfll2m),dfll2m_comp2_read,dfll2m_comp2_write,mod);
        AVR8_RegisterIOHandler(DFLL_COMP2H(addrs->baDfll2m),dfll2m_comp2h_read,dfll2m_comp2h_write,mod);
	
}

