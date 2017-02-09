//===-- boards/armee.c --------------------------------------------*- C -*-===//
//
//              The Leigun Embedded System Simulator Platform : modules
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
//===----------------------------------------------------------------------===//
///
/// @file
/// Compose the ARMee board from Electronic magazine elektor
///
//===----------------------------------------------------------------------===//

// clang-format off
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
// clang-format on

//==============================================================================
//= Dependencies
//==============================================================================
// Main Module Header

// Local/Private Headers
#include "arm/mmu_arm9.h"
#include "controllers/lpc2106/lpc2106_gpio.h"
#include "controllers/lpc2106/lpc2106_scb.h"
#include "controllers/lpc2106/lpc2106_timer.h"
#include "controllers/lpc2106/lpcflash.h"
#include "devices/i2c/m24cxx.h"
#include "devices/lcd/lcd_hd44780.h"
#include "pc16550.h"
#include "pl190_irq.h"

// Leigun Core Headers
#include "bus.h"
#include "clock.h"
#include "configfile.h"
#include "core/device.h"
#include "core/logging.h"
#include "i2c_serdes.h"
#include "initializer.h"
#include "signode.h"
#include "sram.h"

// External headers

// System headers
#include <stdlib.h>


//==============================================================================
//= Constants(also Enumerations)
//==============================================================================
static const char *BOARD_NAME = "ARMee";
static const char *BOARD_DESCRIPTION = "Elektor ARMee Board";
static const char *BOARD_DEFAULTCONFIG = 
"[global]\n"
"start_address: 0\n"
"cpu_clock: 58982400\n"
"oscillator: 14745600\n"
"\n"
"[iram]\n"
"size: 64k\n"
"\n"
"[lcd0]\n"
"backend: rfbserver\n"
"host: 127.0.0.1\n"
"port: 5901\n"
"width: 310\n"
"height: 75\n"
"start: vncviewer localhost:5901\n"
"exit_on_close: 1\n"
"\n"
"[loader]\n"
"load_address: 0x0\n"
"\n"
"[iflash]\n"
"size: 128k\n"
"\n";


//==============================================================================
//= Types
//==============================================================================


//==============================================================================
//= Variables
//==============================================================================


//==============================================================================
//= Function declarations(static)
//==============================================================================
static void create_signal_links(void);
static void create_clock_links(void);
static Device_Board_t *create(void);
static int run(Device_Board_t *board);


//==============================================================================
//= Function definitions(static)
//==============================================================================


/**
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

static Device_Board_t *
create(void)
{
	ArmCoprocessor *copro;
	BusDevice *dev;
	FbDisplay *display = NULL;
	Keyboard *keyboard = NULL;
	Device_Board_t *board;
	board = malloc(sizeof(*board));
	board->run = &run;

	FbDisplay_New("lcd0", &display, &keyboard, NULL, NULL);
	if (!display) {
		LOG_Error(BOARD_NAME, "LCD creation failed");
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
	return board;
}

static int
run(Device_Board_t *board)
{
	ARM9_Run();
	return 0;
}


//==============================================================================
//= Function definitions(global)
//==============================================================================
INITIALIZER(init) {
    Device_RegisterBoard(BOARD_NAME, BOARD_DESCRIPTION, &create, BOARD_DEFAULTCONFIG);
}
