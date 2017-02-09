/*
 ***************************************************************************************************
 *
 *  Compose the ARMee board from Electronic magazine elektor
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

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>

#include "signode.h"
#include "mmu_arm9.h"
#include "bus.h"
#include "configfile.h"
#include "loader.h"
#include "i2c_serdes.h"
#include "m24cxx.h"
#include "boards.h"
#include "lpcflash.h"
#include "sram.h"
#include "pc16550.h"
#include "pl190_irq.h"
#include "clock.h"
#include "lpc2106_timer.h"
#include "lpc2106_gpio.h"
#include "lpc2106_scb.h"
#include "lcd_hd44780.h"
#include "compiler_extensions.h"
#include "initializer.h"

/*
 * ---------------------------------------------------------------
 * Link some Electrical signals in the same way as in schematics
 * This has to be done when all devices are created, because
 * you can not link nonexisting Signals
 * ---------------------------------------------------------------
 */
static void
create_signal_links(void)
{
	SigName_Link("arm.irq", "vic.irq");
	SigName_Link("arm.fiq", "vic.fiq");
	SigName_Link("timer0.irq", "vic.nVICINTSOURCE4");
	SigName_Link("timer1.irq", "vic.nVICINTSOURCE5");
	SigName_Link("uart0.irq", "vic.nVICINTSOURCE6");
	SigName_Link("uart1.irq", "vic.nVICINTSOURCE7");
	/* Connect the LCD */
	SigName_Link("gpio.P0.4", "lcd0.D4");
	SigName_Link("gpio.P0.5", "lcd0.D5");
	SigName_Link("gpio.P0.6", "lcd0.D6");
	SigName_Link("gpio.P0.7", "lcd0.D7");

	SigName_Link("gpio.P0.8", "lcd0.RS");
	SigName_Link("gpio.P0.9", "lcd0.RW");
	SigName_Link("gpio.P0.10", "lcd0.E");

}

static void
create_clock_links(void)
{
	Clock_Link("arm.clk", "scb.cclk");
	Clock_Link("uart0.clk", "scb.pclk");
	Clock_Link("uart1.clk", "scb.pclk");
	Clock_Link("timer0.pclk", "scb.pclk");
	Clock_Link("timer1.pclk", "scb.pclk");
}

static int
board_armee_create(void)
{
	ArmCoprocessor *copro;
	BusDevice *dev;
	FbDisplay *display = NULL;
	Keyboard *keyboard = NULL;

	FbDisplay_New("lcd0", &display, &keyboard, NULL, NULL);
	if (!display) {
		fprintf(stderr, "LCD creation failed\n");
		exit(1);
	}

	Bus_Init(MMU_InvalidateTlb, 4 * 1024);
	ARM9_New();
	/* Copro is created but not registered (1:1 translation is bootup default) */
	copro = MMU9_Create("mmu", BYTE_ORDER_LITTLE, MMU_ARM926EJS | MMUV_NS9750);

	dev = PL190_New("vic");
	Mem_AreaAddMapping(dev, 0xfffff000, 0x1000, MEM_FLAG_WRITABLE | MEM_FLAG_READABLE);

	dev = SRam_New("iram");
	Mem_AreaAddMapping(dev, 0x40000000, 256 * 1024, MEM_FLAG_WRITABLE | MEM_FLAG_READABLE);

	dev = LPCFlash_New("iflash");
	Mem_AreaAddMapping(dev, 0x00000000, 128 * 1024, MEM_FLAG_WRITABLE | MEM_FLAG_READABLE);

	dev = LPC2106Timer_New("timer0");
	Mem_AreaAddMapping(dev, 0xE0004000, 0x20, MEM_FLAG_WRITABLE | MEM_FLAG_READABLE);

	dev = LPC2106Timer_New("timer1");
	Mem_AreaAddMapping(dev, 0xE0008000, 0x20, MEM_FLAG_WRITABLE | MEM_FLAG_READABLE);

	dev = PC16550_New("uart0", 2);
	Mem_AreaAddMapping(dev, 0xE000C000, 0x40, MEM_FLAG_WRITABLE | MEM_FLAG_READABLE);

	dev = PC16550_New("uart1", 2);
	Mem_AreaAddMapping(dev, 0xE0010000, 0x40, MEM_FLAG_WRITABLE | MEM_FLAG_READABLE);

	LPC2106_GpioNew("gpio.P0");

	dev = LPC2106_ScbNew("scb");
	Mem_AreaAddMapping(dev, 0xE01FC000, 0x200, MEM_FLAG_WRITABLE | MEM_FLAG_READABLE);

	HD44780_LcdNew("lcd0", display);

	create_signal_links();
	create_clock_links();
	return 0;
}

static void
board_armee_run(Board * bd)
{
	ARM9_Run();
}

#define DEFAULTCONFIG \
"[global]\n" \
"start_address: 0\n"\
"cpu_clock: 58982400\n"\
"oscillator: 14745600\n" \
"\n" \
"[iram]\n" \
"size: 64k\n" \
"\n" \
"[lcd0]\n" \
"backend: rfbserver\n" \
"host: 127.0.0.1\n" \
"port: 5901\n" \
"width: 310\n" \
"height: 75\n" \
"start: vncviewer localhost:5901\n" \
"exit_on_close: 1\n" \
"\n" \
"[loader]\n" \
"load_address: 0x0\n"\
"\n" \
"[iflash]\n"\
"size: 128k\n"\
"\n"

static Board board_armee = {
	.name = "ARMee",
	.description = "Elektor ARMee Board",
	.createBoard = board_armee_create,
	.runBoard = board_armee_run,
	.defaultconfig = DEFAULTCONFIG
};

INITIALIZER(armee_init)
{
	fprintf(stderr, "Loading Elektor ARMee Board module\n");
	Board_Register(&board_armee);
}
