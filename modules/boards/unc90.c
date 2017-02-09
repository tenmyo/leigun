//===-- boards/unc90.c --------------------------------------------*- C -*-===//
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
/// Compose a FS-Forth UNC90 module with development board
///
//===----------------------------------------------------------------------===//

// clang-format off
/*
 ************************************************************************************************* 
 * Compose a FS-Forth UNC90 module with development board 
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
#include "controllers/at91/at91_usart.h"
#include "controllers/at91/at91_tc.h"
#include "controllers/at91/at91_twi.h"
#include "controllers/at91/at91_emac.h"
#include "controllers/at91/at91_aic.h"
#include "controllers/at91/at91_ebi.h"
#include "controllers/at91/at91_mc.h"
#include "controllers/at91/at91_pio.h"
#include "controllers/at91/at91_pmc.h"
#include "controllers/at91/at91_st.h"
#include "devices/i2c/ds1337.h"
#include "devices/i2c/m24cxx.h"
#include "devices/phy/lxt971a.h"
#include "devices/phy/phy.h"
#include "i2c_serdes.h"
#include "usb_ohci.h"

// Leigun Core Headers
#include "bus.h"
#include "clock.h"
#include "core/device.h"
#include "dram.h"
#include "initializer.h"
#include "signode.h"
#include "sram.h"

// External headers

// System headers


//==============================================================================
//= Constants(also Enumerations)
//==============================================================================
static const char *BOARD_NAME = "UNC90";
static const char *BOARD_DESCRIPTION = "FS-Forth UNC90";
static const char *BOARD_DEFAULTCONFIG = "[global]\n"
                                         "start_address: 0x00000000\n"
                                         "cpu_clock: 158515200\n"
                                         "\n"
                                         "[flash0]\n"
                                         "type: AM29LV128ML\n"
                                         "chips: 1\n"
                                         "\n"
                                         "[dram0]\n"
                                         "size: 32M\n"
                                         "\n"
                                         "[sram0]\n"
                                         "size: 16k\n"
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
static void create_clock_links(void);
static void create_signal_links(void);
static void create_i2c_devices(void);
static Device_Board_t *create(void);
static int run(Device_Board_t *board);


//==============================================================================
//= Function definitions(static)
//==============================================================================

static void create_clock_links(void) {
    /* This is wrong ! does not use the clock enables */
    Clock_Link("st.slck", "pmc.slck");
    Clock_Link("usart0.mck", "pmc.pc_usart0");
    Clock_Link("usart1.mck", "pmc.pc_usart1");
    Clock_Link("usart2.mck", "pmc.pc_usart2");
    Clock_Link("usart3.mck", "pmc.pc_usart3");
    Clock_Link("twi.clk", "pmc.pc_twi");
    Clock_Link("timer0.ch0.mck", "pmc.pc_tc0");
    Clock_Link("timer0.ch0.slck", "pmc.slck");
    Clock_Link("timer0.ch1.mck", "pmc.pc_tc1");
    Clock_Link("timer0.ch1.slck", "pmc.slck");
    Clock_Link("timer0.ch2.mck", "pmc.pc_tc2");
    Clock_Link("timer0.ch2.slck", "pmc.slck");
    Clock_Link("timer1.ch0.mck", "pmc.pc_tc3");
    Clock_Link("timer1.ch0.slck", "pmc.slck");
    Clock_Link("timer1.ch1.mck", "pmc.pc_tc4");
    Clock_Link("timer1.ch1.slck", "pmc.slck");
    Clock_Link("timer1.ch2.mck", "pmc.pc_tc5");
    Clock_Link("timer1.ch2.slck", "pmc.slck");
}

static void create_signal_links(void) {
    int i;
    /* Connect the interrupt controller to the CPU */
    SigName_Link("arm.irq", "aic.irq");
    SigName_Link("arm.fiq", "aic.fiq");

    // SigName_Link("bla","aic.irq0");
    SigName_Link("st.irq", "aic.irq1");
    SigName_Link("pioa.irq", "aic.irq2");
    SigName_Link("piob.irq", "aic.irq3");
    SigName_Link("pioc.irq", "aic.irq4");
    SigName_Link("piod.irq", "aic.irq5");
    SigName_Link("usart0.irq", "aic.irq6");
    SigName_Link("usart1.irq", "aic.irq7");
    SigName_Link("usart2.irq", "aic.irq8");
    SigName_Link("usart3.irq", "aic.irq9");
    // SigName_Link("msi.irq","aic.irq10");
    // SigName_Link("udp.irq","aic.irq11");
    SigName_Link("twi.irq", "aic.irq12");
    // SigName_Link("spi.irq","aic.irq13");
    // SigName_Link("ssc0.irq","aic.irq14");
    // SigName_Link("ssc1.irq","aic.irq15");
    // SigName_Link("ssc2.irq","aic.irq16");
    // SigName_Link("ssc2.irq","aic.irq16");
    SigName_Link("timer0.ch0.irq", "aic.irq17");
    SigName_Link("timer0.ch1.irq", "aic.irq18");
    SigName_Link("timer0.ch2.irq", "aic.irq19");
    SigName_Link("timer1.ch0.irq", "aic.irq20");
    SigName_Link("timer1.ch1.irq", "aic.irq21");
    SigName_Link("timer1.ch2.irq", "aic.irq22");
    SigName_Link("uhp.irq", "aic.irq23");
    SigName_Link("emac.irq", "aic.irq24");

    /* UNC90 links multiple IO Ports to one Pin */
    for (i = 0; i < 8; i++) {
        SigNode_New("UNC90.PORTA%d", i);
        SigNode_New("UNC90.PORTC%d", i);
    }
    SigName_Link("UNC90.PORTA0", "piob.pad0");
    SigName_Link("UNC90.PORTA0", "piob.pad23");
    SigName_Link("UNC90.PORTA1", "piob.pad24");
    SigName_Link("UNC90.PORTA2", "piob.pad25");
    SigName_Link("UNC90.PORTA3", "piob.pad21");
    SigName_Link("UNC90.PORTA3", "piob.pad3");
    SigName_Link("UNC90.PORTA4", "piob.pad1");
    SigName_Link("UNC90.PORTA5", "piob.pad26");
    SigName_Link("UNC90.PORTA6", "piob.pad25");
    SigName_Link("UNC90.PORTA6", "piob.pad27");
    SigName_Link("UNC90.PORTA7", "piob.pad20");
    SigName_Link("UNC90.PORTA7", "piob.pad2");
    SigName_Link("UNC90.PORTC0", "piob.pad6");
    SigName_Link("UNC90.PORTC0", "piob.pad29");
    SigName_Link("UNC90.PORTC0", "pioa.pad27");
    SigName_Link("UNC90.PORTC1", "pioa.pad30");
    SigName_Link("UNC90.PORTC1", "piob.pad28");
    SigName_Link("UNC90.PORTC2", "pioa.pad25");
    SigName_Link("UNC90.PORTC3", "pioa.pad22");
    SigName_Link("UNC90.PORTC3", "piob.pad9");
    SigName_Link("UNC90.PORTC3", "pioa.pad2");
    SigName_Link("UNC90.PORTC4", "pioa.pad24");
    SigName_Link("UNC90.PORTC4", "piob.pad7");
    SigName_Link("UNC90.PORTC5", "pioa.pad31");
    SigName_Link("UNC90.PORTC6", "pioa.pad26");
    SigName_Link("UNC90.PORTC7", "pioa.pad23");
    SigName_Link("UNC90.PORTC7", "piob.pad8");

    /* Link the TWI (I2C) Bus */
    SigName_Link("i2cbus.sda", "twi.sda");
    SigName_Link("i2cbus.scl", "twi.scl");
    SigName_Link("i2cbus.cfg_eeprom.wc", "GND");
}

static void create_i2c_devices(void) {
    I2C_Slave *i2c_slave;
    I2C_SerDes *i2c_serdes;
    i2c_serdes = I2C_SerDesNew("i2cbus");

    /* Configuration EEPRom with 64kBit */
    i2c_slave = M24Cxx_New("M24C64", "i2cbus.cfg_eeprom");
    I2C_SerDesAddSlave(i2c_serdes, i2c_slave, 0x50);
    i2c_slave = DS1337_New("i2cbus.ds1337");
    I2C_SerDesAddSlave(i2c_serdes, i2c_slave, 0x68);
}

static Device_Board_t *create(void) {
    ArmCoprocessor *copro;
    BusDevice *dev;
    BusDevice *mc;
    BusDevice *dram0 = NULL;
    PHY_Device *phy;
    Device_Board_t *board;
    board = malloc(sizeof(*board));
    board->run = &run;

    Bus_Init(MMU_InvalidateTlb, 4 * 1024);
    ARM9_New();
    copro = MMU9_Create("mmu", BYTE_ORDER_LITTLE, MMU_ARM920T);
    ARM9_RegisterCoprocessor(copro, 15);

    /*
     * First create the memory controller. It is required by other devices
     */
    mc = AT91Mc_New("mc");
    Mem_AreaAddMapping(mc, 0xffffff00, 0x10,
                       MEM_FLAG_WRITABLE | MEM_FLAG_READABLE);

    dev = OhciHC_New("uhp", MainBus);
    Mem_AreaAddMapping(dev, 0x00300000, 0x00100000,
                       MEM_FLAG_WRITABLE | MEM_FLAG_READABLE);
    OhciHC_Enable(dev);

    dev = AMDFlashBank_New("flash0");
    if (dev) {
        AT91Mc_RegisterDevice(mc, dev, AT91_AREA_EXMEM0);
    }

    dram0 = dev = DRam_New("dram0");
    if (dev) {
        AT91Mc_RegisterDevice(mc, dev, AT91_AREA_CS1);
    }

    dev = AT91Tc_New("timer0");
    Mem_AreaAddMapping(dev, 0xfffa0000, 0x00004000,
                       MEM_FLAG_WRITABLE | MEM_FLAG_READABLE);
    dev = AT91Tc_New("timer1");
    Mem_AreaAddMapping(dev, 0xfffa4000, 0x00004000,
                       MEM_FLAG_WRITABLE | MEM_FLAG_READABLE);

    dev = AT91Twi_New("twi", 0);
    Mem_AreaAddMapping(dev, 0xfffb8000, 0x00004000,
                       MEM_FLAG_WRITABLE | MEM_FLAG_READABLE);

    dev = AT91Emac_New("emac");
    Mem_AreaAddMapping(dev, 0xfffbc000, 0x00004000,
                       MEM_FLAG_WRITABLE | MEM_FLAG_READABLE);
    phy = Lxt971a_New("phy");
    AT91Emac_RegisterPhy(dev, phy, 1);

    dev = AT91Usart_New("usart0");
    Mem_AreaAddMapping(dev, 0xfffc0000, 0x00004000,
                       MEM_FLAG_WRITABLE | MEM_FLAG_READABLE);
    dev = AT91Usart_New("usart1");
    Mem_AreaAddMapping(dev, 0xfffc4000, 0x00004000,
                       MEM_FLAG_WRITABLE | MEM_FLAG_READABLE);
    dev = AT91Usart_New("usart2");
    Mem_AreaAddMapping(dev, 0xfffc8000, 0x00004000,
                       MEM_FLAG_WRITABLE | MEM_FLAG_READABLE);
    dev = AT91Usart_New("usart3");
    Mem_AreaAddMapping(dev, 0xfffcc000, 0x00004000,
                       MEM_FLAG_WRITABLE | MEM_FLAG_READABLE);
    dev = AT91Aic_New("aic");
    Mem_AreaAddMapping(dev, 0xfffff000, 0x00000200,
                       MEM_FLAG_WRITABLE | MEM_FLAG_READABLE);

    dev = AT91Pio_New("pioa");
    Mem_AreaAddMapping(dev, 0xfffff400, 0x00000200,
                       MEM_FLAG_WRITABLE | MEM_FLAG_READABLE);
    dev = AT91Pio_New("piob");
    Mem_AreaAddMapping(dev, 0xfffff600, 0x00000200,
                       MEM_FLAG_WRITABLE | MEM_FLAG_READABLE);
    dev = AT91Pio_New("pioc");
    Mem_AreaAddMapping(dev, 0xfffff800, 0x00000200,
                       MEM_FLAG_WRITABLE | MEM_FLAG_READABLE);
    dev = AT91Pio_New("piod");
    Mem_AreaAddMapping(dev, 0xfffffa00, 0x00000200,
                       MEM_FLAG_WRITABLE | MEM_FLAG_READABLE);

    dev = AT91Pmc_New("pmc", AT91_PID_TABLE_RM9200);
    Mem_AreaAddMapping(dev, 0xfffffc00, 0x00000100,
                       MEM_FLAG_WRITABLE | MEM_FLAG_READABLE);
    dev = AT91St_New("st");
    Mem_AreaAddMapping(dev, 0xfffffd00, 0x00000100,
                       MEM_FLAG_WRITABLE | MEM_FLAG_READABLE);
    dev = AT91Ebi_New("ebi");
    Mem_AreaAddMapping(dev, 0xffffff60, 0x00000080,
                       MEM_FLAG_WRITABLE | MEM_FLAG_READABLE);

    dev = SRam_New("sram0");
    if (dev) {
        AT91Mc_RegisterDevice(mc, dev, AT91_AREA_SRAM);
    }
    create_i2c_devices();
    create_signal_links();
    create_clock_links();
    return board;
}

static int run(Device_Board_t *board) {
    ARM9_Run();
    return 0;
}


//==============================================================================
//= Function definitions(global)
//==============================================================================
INITIALIZER(init) {
    Device_RegisterBoard(BOARD_NAME, BOARD_DESCRIPTION, &create,
                         BOARD_DEFAULTCONFIG);
}
