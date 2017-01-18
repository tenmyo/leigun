/*
 *************************************************************************************************
 * Emulation of Sharp LH79520 reset clock and power control module
 *
 * state: Not implemented
 *
 * Copyright 2007 Jochen Karrer. All rights reserved.
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
#include <bus.h>
#include <signode.h>
#include <lh79520_rcpc.h>
#include <configfile.h>
#include <clock.h>
#include <cycletimer.h>
#include <sgstring.h>

#define RCPC_CTRL(base) 		((base)+0x00)
#define RCPC_IDSRING(base) 		((base)+0x04)
#define RCPC_REMAP_CTRL(base) 		((base)+0x08)
#define RCPC_SOFTRESET(base)		((base)+0x0c)
#define RCPC_RESET_STATUS(base)		((base)+0x10)
#define RCPC_RESET_STATUS_CLR(base)	((base)+0x14)
#define RCPC_HCLK_PRESCALE(base)	((base)+0x18)
#define RCPC_CPU_CLK_PRESCALE(base)	((base)+0x1c)
#define RCPC_PERIPH_CLK_CTRL(base)	((base)+0x24)
#define RCPC_PERIPH_CLK_CTRL2(base)	((base)+0x28)
#define RCPC_AHB_CLK_CTRL(base)		((base)+0x2c)
#define RCPC_PERIPH_CLK_SEL(base)	((base)+0x30)
#define RCPC_PERIPH_CLK_SEL2(base)	((base)+0x34)
#define RCPC_PWM0_PRESCALE(base)	((base)+0x38)
#define RCPC_PWM1_PRESCALE(base)	((base)+0x3c)
#define RCPC_LCDCLK_PRESCALE(base)	((base)+0x40)
#define RCPC_SSPCLK_PRESCALE(base)	((base)+0x44)
#define RCPC_INT_CONFIG(base)		((base)+0x80)
#define RCPC_INT_CLEAR(base)		((base)+0x84)
#define RCPC_CORE_CLK_CONFIG(base)	((base)+0x88)

typedef struct Rcpc {
	BusDevice bdev;
} Rcpc;
static uint32_t
ctrl_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "Register %08x not implemented\n", address);
	return 0;
}

static void
ctrl_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "Register %08x not implemented\n", address);
}

static uint32_t
idstring_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "Register %08x not implemented\n", address);
	return 0;
}

static void
idstring_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "Register %08x not implemented\n", address);
}

static uint32_t
remap_ctrl_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "Register %08x not implemented\n", address);
	return 0;
}

static void
remap_ctrl_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "Register %08x not implemented\n", address);
}

static uint32_t
softreset_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "Register %08x not implemented\n", address);
	return 0;
}

static void
softreset_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "Register %08x not implemented\n", address);
}

static uint32_t
reset_status_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "Register %08x not implemented\n", address);
	return 0;
}

static void
reset_status_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "Register %08x not implemented\n", address);
}

static uint32_t
reset_status_clr_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "Register %08x not implemented\n", address);
	return 0;
}

static void
reset_status_clr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "Register %08x not implemented\n", address);
}

static uint32_t
hclk_prescale_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "Register %08x not implemented\n", address);
	return 0;
}

static void
hclk_prescale_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "Register %08x not implemented\n", address);
}

static uint32_t
cpu_clk_prescale_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "Register %08x not implemented\n", address);
	return 0;
}

static void
cpu_clk_prescale_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "Register %08x not implemented\n", address);
}

static uint32_t
periph_clk_ctrl_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "Register %08x not implemented\n", address);
	return 0;
}

static void
periph_clk_ctrl_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "Register %08x not implemented\n", address);
}

static uint32_t
periph_clk_ctrl2_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "Register %08x not implemented\n", address);
	return 0;
}

static void
periph_clk_ctrl2_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "Register %08x not implemented\n", address);
}

static uint32_t
ahb_clk_ctrl_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "Register %08x not implemented\n", address);
	return 0;
}

static void
ahb_clk_ctrl_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "Register %08x not implemented\n", address);
}

static uint32_t
periph_clk_sel_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "Register %08x not implemented\n", address);
	return 0;
}

static void
periph_clk_sel_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "Register %08x not implemented\n", address);
}

static uint32_t
periph_clk_sel2_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "Register %08x not implemented\n", address);
	return 0;
}

static void
periph_clk_sel2_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "Register %08x not implemented\n", address);
}

static uint32_t
pwm0_prescale_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "Register %08x not implemented\n", address);
	return 0;
}

static void
pwm0_prescale_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "Register %08x not implemented\n", address);
}

static uint32_t
pwm1_prescale_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "Register %08x not implemented\n", address);
	return 0;
}

static void
pwm1_prescale_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "Register %08x not implemented\n", address);
}

static uint32_t
lcdclk_prescale_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "Register %08x not implemented\n", address);
	return 0;
}

static void
lcdclk_prescale_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "Register %08x not implemented\n", address);
}

static uint32_t
sspclk_prescale_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "Register %08x not implemented\n", address);
	return 0;
}

static void
sspclk_prescale_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "Register %08x not implemented\n", address);
}

static uint32_t
int_config_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "Register %08x not implemented\n", address);
	return 0;
}

static void
int_config_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "Register %08x not implemented\n", address);
}

static uint32_t
int_clear_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "Register %08x not implemented\n", address);
	return 0;
}

static void
int_clear_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "Register %08x not implemented\n", address);
}

static uint32_t
core_clk_config_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "Register %08x not implemented\n", address);
	return 0;
}

static void
core_clk_config_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "Register %08x not implemented\n", address);
}

static void
Rcpc_Map(void *owner, uint32_t base, uint32_t mask, uint32_t flags)
{
	Rcpc *rcpc = (Rcpc *) owner;

	IOH_New32(RCPC_CTRL(base), ctrl_read, ctrl_write, rcpc);
	IOH_New32(RCPC_IDSRING(base), idstring_read, idstring_write, rcpc);
	IOH_New32(RCPC_REMAP_CTRL(base), remap_ctrl_read, remap_ctrl_write, rcpc);
	IOH_New32(RCPC_SOFTRESET(base), softreset_read, softreset_write, rcpc);
	IOH_New32(RCPC_RESET_STATUS(base), reset_status_read, reset_status_write, rcpc);
	IOH_New32(RCPC_RESET_STATUS_CLR(base), reset_status_clr_read, reset_status_clr_write, rcpc);
	IOH_New32(RCPC_HCLK_PRESCALE(base), hclk_prescale_read, hclk_prescale_write, rcpc);
	IOH_New32(RCPC_CPU_CLK_PRESCALE(base), cpu_clk_prescale_read, cpu_clk_prescale_write, rcpc);
	IOH_New32(RCPC_PERIPH_CLK_CTRL(base), periph_clk_ctrl_read, periph_clk_ctrl_write, rcpc);
	IOH_New32(RCPC_PERIPH_CLK_CTRL2(base), periph_clk_ctrl2_read, periph_clk_ctrl2_write, rcpc);
	IOH_New32(RCPC_AHB_CLK_CTRL(base), ahb_clk_ctrl_read, ahb_clk_ctrl_write, rcpc);
	IOH_New32(RCPC_PERIPH_CLK_SEL(base), periph_clk_sel_read, periph_clk_sel_write, rcpc);
	IOH_New32(RCPC_PERIPH_CLK_SEL2(base), periph_clk_sel2_read, periph_clk_sel2_write, rcpc);
	IOH_New32(RCPC_PWM0_PRESCALE(base), pwm0_prescale_read, pwm0_prescale_write, rcpc);
	IOH_New32(RCPC_PWM1_PRESCALE(base), pwm1_prescale_read, pwm1_prescale_write, rcpc);
	IOH_New32(RCPC_LCDCLK_PRESCALE(base), lcdclk_prescale_read, lcdclk_prescale_write, rcpc);
	IOH_New32(RCPC_SSPCLK_PRESCALE(base), sspclk_prescale_read, sspclk_prescale_write, rcpc);
	IOH_New32(RCPC_INT_CONFIG(base), int_config_read, int_config_write, rcpc);
	IOH_New32(RCPC_INT_CLEAR(base), int_clear_read, int_clear_write, rcpc);
	IOH_New32(RCPC_CORE_CLK_CONFIG(base), core_clk_config_read, core_clk_config_write, rcpc);
}

static void
Rcpc_UnMap(void *owner, uint32_t base, uint32_t mask)
{

	IOH_Delete32(RCPC_CTRL(base));
	IOH_Delete32(RCPC_IDSRING(base));
	IOH_Delete32(RCPC_REMAP_CTRL(base));
	IOH_Delete32(RCPC_SOFTRESET(base));
	IOH_Delete32(RCPC_RESET_STATUS(base));
	IOH_Delete32(RCPC_RESET_STATUS_CLR(base));
	IOH_Delete32(RCPC_HCLK_PRESCALE(base));
	IOH_Delete32(RCPC_CPU_CLK_PRESCALE(base));
	IOH_Delete32(RCPC_PERIPH_CLK_CTRL(base));
	IOH_Delete32(RCPC_PERIPH_CLK_CTRL2(base));
	IOH_Delete32(RCPC_AHB_CLK_CTRL(base));
	IOH_Delete32(RCPC_PERIPH_CLK_SEL(base));
	IOH_Delete32(RCPC_PERIPH_CLK_SEL2(base));
	IOH_Delete32(RCPC_PWM0_PRESCALE(base));
	IOH_Delete32(RCPC_PWM1_PRESCALE(base));
	IOH_Delete32(RCPC_LCDCLK_PRESCALE(base));
	IOH_Delete32(RCPC_SSPCLK_PRESCALE(base));
	IOH_Delete32(RCPC_INT_CONFIG(base));
	IOH_Delete32(RCPC_INT_CLEAR(base));
	IOH_Delete32(RCPC_CORE_CLK_CONFIG(base));
}

BusDevice *
LH79520Rcpc_New(const char *name)
{

	Rcpc *rcpc = sg_new(Rcpc);
	rcpc->bdev.first_mapping = NULL;
	rcpc->bdev.Map = Rcpc_Map;
	rcpc->bdev.UnMap = Rcpc_UnMap;
	rcpc->bdev.owner = rcpc;
	rcpc->bdev.hw_flags = MEM_FLAG_WRITABLE | MEM_FLAG_READABLE;
	return &rcpc->bdev;
}
