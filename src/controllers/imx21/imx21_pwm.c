/*
 *************************************************************************************************
 * Emulation of Freescale iMX21 Pulse width modulator 
 *
 * state: nothing works
 *
 * Copyright 2006 Jochen Karrer. All rights reserved.
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

#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/fcntl.h>
#include "bus.h"
#include "fio.h"
#include "imx21_pwm.h"
#include "signode.h"
#include "configfile.h"
#include "clock.h"
#include "cycletimer.h"
#include "sgstring.h"

#define PWMC(base) ((base)+0x0)
#define		PWMC_HCTR	(1<<18)
#define		PWMC_BCTR	(1<<17)
#define		PWMC_SWR	(1<<16)
#define		PWMC_CLKSRC	(1<<15)
#define		PWMC_PRESCALER_MASK	(0x7f<<8)
#define		PWMC_PRESCALER_SHIFT	(8)
#define		PWMC_IRQ	(1<<7)
#define		PWMC_IRQEN	(1<<6)
#define		PWMC_FIFOAV	(1<<5)
#define		PWMC_EN		(1<<4)
#define		PWMC_REPEAT_MASK	(3<<2)
#define		PWMC_REPEAT_SHIFT	(2)
#define		PWMC_CLKSEL_MASK	(3)
#define		PWMC_CLKSEL_SHIFT	(0)
#define PWMS(base) ((base)+0x4)
#define PWMP(base) ((base)+0x8)
#define PWMCNT(base) ((base)+0xc)

#define FIFO_COUNT(pwm) (pwm->pwms_wp - pwm->pwms_rp)
#define FIFO_SIZE	(4)

typedef struct IMX21Pwm {
	BusDevice bdev;
	Clock_t *clk;
	Clock_t *clk32;
	SigNode *irqNode;
	uint32_t pwmc;
	uint16_t pwms[FIFO_SIZE];
	uint16_t pwmp;
	uint16_t pwmcnt;
	uint64_t pwms_wp;
	uint64_t pwms_rp;
} IMX21Pwm;

static void
update_interrupt(IMX21Pwm * pwm)
{
	if ((pwm->pwmc & PWMC_IRQ) && (pwm->pwmc & PWMC_IRQEN)) {
		SigNode_Set(pwm->irqNode, SIG_LOW);
	} else {
		SigNode_Set(pwm->irqNode, SIG_LOW);
	}
}

static uint32_t
pwmc_read(void *clientData, uint32_t address, int rqlen)
{
	IMX21Pwm *pwm = (IMX21Pwm *) clientData;
	if (pwm->pwmc & PWMC_IRQ) {
		pwm->pwmc &= ~PWMC_IRQ;
		update_interrupt(pwm);
	}
	return pwm->pwmc;
}

static void
pwmc_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	static int count = 0;
	IMX21Pwm *pwm = (IMX21Pwm *) clientData;
	pwm->pwmc = (pwm->pwmc & 0xfff900a0) | (value & 0x0006ff5f);
	update_interrupt(pwm);
	if (count < 10) {
		fprintf(stderr, "PWMC write not implemented\n");
		count++;
	}
}

static uint32_t
pwms_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "PWMS read not implemented\n");
	return 0;
}

static void
pwms_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	IMX21Pwm *pwm = (IMX21Pwm *) clientData;
	if (FIFO_COUNT(pwm) < FIFO_SIZE) {
		/* Check for swap words missing here */
		pwm->pwms[pwm->pwms_wp % FIFO_SIZE] = value;
		pwm->pwms_wp++;
	} else {
		fprintf(stderr, "PWMS fifo overflow\n");
	}
	fprintf(stderr, "PWMS write not implemented\n");
}

static uint32_t
pwmp_read(void *clientData, uint32_t address, int rqlen)
{
	IMX21Pwm *pwm = (IMX21Pwm *) clientData;
	return pwm->pwmp;
}

static void
pwmp_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	IMX21Pwm *pwm = (IMX21Pwm *) clientData;
	pwm->pwmp = value;
	fprintf(stderr, "PWMP write not implemented\n");
}

static uint32_t
pwmcnt_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "PWMCNT read not implemented\n");
	return 0;
}

static void
pwmcnt_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "PWMCNT not writable\n");
}

static void
IMXPwm_UnMap(void *owner, uint32_t base, uint32_t mask)
{
	IOH_Delete32(PWMC(base));
	IOH_Delete32(PWMS(base));
	IOH_Delete32(PWMP(base));
	IOH_Delete32(PWMCNT(base));
}

static void
IMXPwm_Map(void *owner, uint32_t base, uint32_t mask, uint32_t flags)
{
	IMX21Pwm *pwm = (IMX21Pwm *) owner;
	IOH_New32(PWMC(base), pwmc_read, pwmc_write, pwm);
	IOH_New32(PWMS(base), pwms_read, pwms_write, pwm);
	IOH_New32(PWMP(base), pwmp_read, pwmp_write, pwm);
	IOH_New32(PWMCNT(base), pwmcnt_read, pwmcnt_write, pwm);

}

BusDevice *
IMX21_PwmNew(const char *name)
{
	IMX21Pwm *pwm;
	pwm = sg_new(IMX21Pwm);
	pwm->pwmc = 0x000000a0;
	pwm->pwmp = 0xfffe;
	pwm->pwmcnt = 0;
	pwm->irqNode = SigNode_New("%s.irq", name);
	if (!pwm->irqNode) {
		fprintf(stderr, "IMX21-PWM: Creation of interrupt line failed\n");
		exit(1);
	}
	pwm->clk = Clock_New("%s.clk", name);
	pwm->clk32 = Clock_New("%s.clk32", name);
	update_interrupt(pwm);
	pwm->bdev.first_mapping = NULL;
	pwm->bdev.Map = IMXPwm_Map;
	pwm->bdev.UnMap = IMXPwm_UnMap;
	pwm->bdev.owner = pwm;
	pwm->bdev.hw_flags = MEM_FLAG_WRITABLE | MEM_FLAG_READABLE;
	return &pwm->bdev;
}
