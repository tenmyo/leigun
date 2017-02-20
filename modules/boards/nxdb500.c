//===-- boards/nxdb500.c ------------------------------------------*- C -*-===//
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
///
///
//===----------------------------------------------------------------------===//

// clang-format off
/*
 ****************************************************************************************************
 *
 *  State: Boots linux 
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
// clang-format on

//==============================================================================
//= Dependencies
//==============================================================================
// Main Module Header

// Local/Private Headers
#include "amdflash.h"
#include "arm/mmu_arm9.h"
#include "devices/netx/netx_gpio.h"
#include "devices/netx/netx_sysco.h"
#include "devices/netx/netx_uart.h"
#include "devices/netx/netx_xmac.h"
#include "devices/netx/netx_xpec.h"
#include "pl190_irq.h"

// Leigun Core Headers
#include "bus.h"
#include "core/device.h"
#include "core/logging.h"
#include "dram.h"
#include "signode.h"
#include "sram.h"

// External headers

// System headers


//==============================================================================
//= Constants(also Enumerations)
//==============================================================================
#define BOARD_NAME "NXDB500"
#define BOARD_DESCRIPTION "NXDB500 ARM Controller Card"
#define BOARD_DEFAULTCONFIG                                                    \
    "[global]\n"                                                               \
    "start_address: 0\n"                                                       \
    "cpu_clock: 200000000\n"                                                   \
    "\n"                                                                       \
    "[dram0]\n"                                                                \
    "size: 64M\n"                                                              \
    "\n"                                                                       \
    "[loader]\n"                                                               \
    "load_address: 0x50000000\n"                                               \
    "\n"                                                                       \
    "[sram0]\n"                                                                \
    "size: 32k\n"                                                              \
    "\n"                                                                       \
    "[sram1]\n"                                                                \
    "size: 32k\n"                                                              \
    "\n"                                                                       \
    "[sram2]\n"                                                                \
    "size: 32k\n"                                                              \
    "\n"                                                                       \
    "[sram3]\n"                                                                \
    "size: 32k\n"                                                              \
    "\n"                                                                       \
    "[tcdm]\n"                                                                 \
    "size: 8k\n"                                                               \
    "\n"                                                                       \
    "[backupram]\n"                                                            \
    "size: 16k\n"                                                              \
    "\n"                                                                       \
    "[dram1]\n"                                                                \
    "size: 32M\n"                                                              \
    "\n"                                                                       \
    "[flash0]\n"                                                               \
    "type: AM29LV256ML\n"                                                      \
    "chips: 1\n"


//==============================================================================
//= Types
//==============================================================================
typedef struct board_s {
    Device_Board_t base;
    Device_MPU_t *mpu;
} board_t;


//==============================================================================
//= Variables
//==============================================================================


//==============================================================================
//= Function declarations(static)
//==============================================================================
static void create_signal_links(void);
static Device_Board_t *create(void);


//==============================================================================
//= Function definitions(static)
//==============================================================================

static void create_signal_links(void) {
    SigName_Link("arm.irq", "vic.irq");
    SigName_Link("arm.fiq", "vic.fiq");
    SigName_Link("gpio.timer0.irq", "vic.nVICINTSOURCE1");
    SigName_Link("gpio.timer1.irq", "vic.nVICINTSOURCE2");
    SigName_Link("gpio.timer2.irq", "vic.nVICINTSOURCE3");
#if 0
	SigName_Link("systime_ns.irq", "vic.nVICINTSOURCE4");
	SigName_Link("systime_s.irq", "vic.nVICINTSOURCE5");
	SigName_Link("gpio.15.irq", "vic.nVICINTSOURCE6");
#endif
    SigName_Link("uart0.irq", "vic.nVICINTSOURCE8");
    SigName_Link("uart1.irq", "vic.nVICINTSOURCE9");
    SigName_Link("uart2.irq", "vic.nVICINTSOURCE10");
#if 0
	SigName_Link("usb.irq", "vic.nVICINTSOURCE11");
	SigName_Link("spi.irq", "vic.nVICINTSOURCE12");
	SigName_Link("i2c.irq", "vic.nVICINTSOURCE13");
	SigName_Link("lcd.irq", "vic.nVICINTSOURCE14");
	SigName_Link("hif.irq", "vic.nVICINTSOURCE15");
#endif
    SigName_Link("gpio.irq", "vic.nVICINTSOURCE16");
#if 0
	SigName_Link("com0.irq", "vic.nVICINTSOURCE17");
	SigName_Link("com1.irq", "vic.nVICINTSOURCE18");
	SigName_Link("com2.irq", "vic.nVICINTSOURCE19");
	SigName_Link("com3.irq", "vic.nVICINTSOURCE20");
	SigName_Link("msync0.irq", "vic.nVICINTSOURCE21");
	SigName_Link("msync1.irq", "vic.nVICINTSOURCE22");
	SigName_Link("msync2.irq", "vic.nVICINTSOURCE23");
	SigName_Link("msync3.irq", "vic.nVICINTSOURCE24");
	SigName_Link("intphy.irq", "vic.nVICINTSOURCE25");
	SigName_Link("isoarea.irq", "vic.nVICINTSOURCE26");
#endif
    SigName_Link("gpio.timer3.irq", "vic.nVICINTSOURCE29");
    SigName_Link("gpio.timer4.irq", "vic.nVICINTSOURCE30");
}

static Device_Board_t *create(void) {
    ArmCoprocessor *copro;
    BusDevice *dev;
    board_t *board = calloc(1, sizeof(*board));
    board->base.base.self = board;

    Bus_Init(MMU_InvalidateTlb, 4 * 1024);
    board->mpu = Device_CreateMPU("ARM9");
    copro = MMU9_Create("mmu", BYTE_ORDER_LITTLE, MMU_ARM926EJS);
    ARM9_RegisterCoprocessor(copro, 15);

    dev = NetXSysco_New("sysco");
    Mem_AreaAddMapping(dev, 0x00100000, 0x300,
                       MEM_FLAG_WRITABLE | MEM_FLAG_READABLE);

    dev = NetXUart_New("uart0");
    Mem_AreaAddMapping(dev, 0x00100a00, 0x40,
                       MEM_FLAG_WRITABLE | MEM_FLAG_READABLE);
    dev = NetXUart_New("uart1");
    Mem_AreaAddMapping(dev, 0x00100a40, 0x40,
                       MEM_FLAG_WRITABLE | MEM_FLAG_READABLE);
    dev = NetXUart_New("uart2");
    Mem_AreaAddMapping(dev, 0x00100a80, 0x40,
                       MEM_FLAG_WRITABLE | MEM_FLAG_READABLE);

    dev = NetXGpio_New("gpio");
    Mem_AreaAddMapping(dev, 0x00100800, 0xff,
                       MEM_FLAG_WRITABLE | MEM_FLAG_READABLE);

    dev = XMac_New("xmac0");
    Mem_AreaAddMapping(dev, 0x00160000, 0x1000,
                       MEM_FLAG_WRITABLE | MEM_FLAG_READABLE);
    dev = XMac_New("xmac1");
    Mem_AreaAddMapping(dev, 0x00161000, 0x1000,
                       MEM_FLAG_WRITABLE | MEM_FLAG_READABLE);
    dev = XMac_New("xmac2");
    Mem_AreaAddMapping(dev, 0x00162000, 0x1000,
                       MEM_FLAG_WRITABLE | MEM_FLAG_READABLE);
    dev = XMac_New("xmac3");
    Mem_AreaAddMapping(dev, 0x00163000, 0x1000,
                       MEM_FLAG_WRITABLE | MEM_FLAG_READABLE);

    dev = XPec_New("xpec0");
    Mem_AreaAddMapping(dev, 0x00170000, 0x4000,
                       MEM_FLAG_WRITABLE | MEM_FLAG_READABLE);
    dev = XPec_New("xpec1");
    Mem_AreaAddMapping(dev, 0x00174000, 0x4000,
                       MEM_FLAG_WRITABLE | MEM_FLAG_READABLE);
    dev = XPec_New("xpec2");
    Mem_AreaAddMapping(dev, 0x00178000, 0x4000,
                       MEM_FLAG_WRITABLE | MEM_FLAG_READABLE);
    dev = XPec_New("xpec3");
    Mem_AreaAddMapping(dev, 0x0017c000, 0x4000,
                       MEM_FLAG_WRITABLE | MEM_FLAG_READABLE);
    dev = PL190_New("vic");
    Mem_AreaAddMapping(dev, 0x001ff000, 0x400,
                       MEM_FLAG_WRITABLE | MEM_FLAG_READABLE);

    dev = DRam_New("dram0");
    if (dev) {
        Mem_AreaAddMapping(dev, 0x80000000, 0x40000000,
                           MEM_FLAG_WRITABLE | MEM_FLAG_READABLE);
    }

    dev = SRam_New("sram0");
    Mem_AreaAddMapping(dev, 0x00000000, 0x00008000,
                       MEM_FLAG_WRITABLE | MEM_FLAG_READABLE);
    dev = SRam_New("sram1");
    Mem_AreaAddMapping(dev, 0x00008000, 0x00008000,
                       MEM_FLAG_WRITABLE | MEM_FLAG_READABLE);
    dev = SRam_New("sram2");
    Mem_AreaAddMapping(dev, 0x00010000, 0x00008000,
                       MEM_FLAG_WRITABLE | MEM_FLAG_READABLE);
    dev = SRam_New("sram3");
    Mem_AreaAddMapping(dev, 0x00008000, 0x00008000,
                       MEM_FLAG_WRITABLE | MEM_FLAG_READABLE);

    dev = SRam_New("backupram");
    Mem_AreaAddMapping(dev, 0x00300000, 0x00004000,
                       MEM_FLAG_WRITABLE | MEM_FLAG_READABLE);

    dev = SRam_New("tcdm");
    Mem_AreaAddMapping(dev, 0x10000000, 0x00002000,
                       MEM_FLAG_WRITABLE | MEM_FLAG_READABLE);

    dev = AMDFlashBank_New("flash0");
    if (dev) {
        Mem_AreaAddMapping(dev, 0xC0000000, 0x40000000,
                           MEM_FLAG_WRITABLE | MEM_FLAG_READABLE);
    }
    // create_i2c_devices();
    create_signal_links();
    return &board->base;
}


//==============================================================================
//= Function definitions(global)
//==============================================================================
DEVICE_REGISTER_BOARD(BOARD_NAME, BOARD_DESCRIPTION, &create,
                      BOARD_DEFAULTCONFIG);
