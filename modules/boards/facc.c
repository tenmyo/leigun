//===-- boards/facc.c ---------------------------------------------*- C -*-===//
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
/// NS9360 Fantasy ARM Controller Card
///     Compose a NS9360 Fantasy Board
///
//===----------------------------------------------------------------------===//

// clang-format off
/*
 *************************************************************************************************
 * NS9360 Fantasy ARM Controller Card 
 *      Compose a NS9360 Fantasy Board
 *
 * Copyright 2004 2005 Jochen Karrer. All rights reserved.
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
#include "controllers/ns9750/ns9750_bbdma.h"
#include "controllers/ns9750/ns9750_bbus.h"
#include "controllers/ns9750/ns9750_eth.h"
#include "controllers/ns9750/ns9750_mem.h"
#include "controllers/ns9750/ns9750_pci.h"
#include "controllers/ns9750/ns9750_serial.h"
#include "controllers/ns9750/ns9750_timer.h"
#include "controllers/ns9750/ns9750_usb.h"
#include "devices/can/lacc_can.h"
#include "devices/dm9000/dm9000.h"
#include "devices/i2c/m24cxx.h"
#include "devices/i2c/pcf8563.h"
#include "devices/phy/lxt971a.h"
#include "devices/phy/phy.h"

// Leigun Core Headers
#include "bus.h"
#include "core/device.h"
#include "core/logging.h"
#include "dram.h"
#include "i2c_serdes.h"
#include "initializer.h"
#include "signode.h"

// External headers

// System headers


//==============================================================================
//= Constants(also Enumerations)
//==============================================================================
static const char *BOARD_NAME = "FACC";
static const char *BOARD_DESCRIPTION = "Fantasy NS9360 ARM Controller Card";
static const char *BOARD_DEFAULTCONFIG = "[global]\n"
                                         "start_address: 0\n"
                                         "\n"
                                         "[loader]\n"
                                         "load_address: 0x50000000\n"
                                         "\n"
                                         "[dram0]\n"
                                         "size: 32M\n"
                                         "\n"
                                         "[flash1]\n"
                                         "type: AM29LV640ML\n"
                                         "chips: 1\n"
                                         "\n";


//==============================================================================
//= Types
//==============================================================================
typedef struct board_s {
    Device_Board_t board;
    Device_MPU_t *mpu;
} board_t;


//==============================================================================
//= Variables
//==============================================================================


//==============================================================================
//= Function declarations(static)
//==============================================================================
static void create_signal_links(void);
static void create_i2c_devices(void);
static Device_Board_t *create(void);
static int run(Device_Board_t *board);


//==============================================================================
//= Function definitions(static)
//==============================================================================

/*
 * ---------------------------------------------------------------
 * Link some Electrical signals in the same way as in schematics
 * This has to be done when all devices are created, because
 * you can not link nonexisting Signals
 * ---------------------------------------------------------------
 */
static void create_signal_links(void) {
    SigName_Link("i2cbus0000.sda", "bbutil.gpio.13");
    SigName_Link("i2cbus0000.scl", "bbutil.gpio.14");
    SigName_Link("i2cbus0000.cfg_eeprom.wc", "bbutil.gpio.15");

    SigName_Link("serialA.TxDmaReq", "bbdma.1.FbrDmaReq");
    SigName_Link("serialB.TxDmaReq", "bbdma.3.FbrDmaReq");
    SigName_Link("serialC.TxDmaReq", "bbdma.5.FbrDmaReq");
    SigName_Link("serialD.TxDmaReq", "bbdma.7.FbrDmaReq");

    SigName_Link("serialA.tx_irq", "bbus.irq_satx");
    SigName_Link("serialA.rx_irq", "bbus.irq_sarx");
    SigName_Link("serialB.tx_irq", "bbus.irq_sbtx");
    SigName_Link("serialB.rx_irq", "bbus.irq_sbrx");
    SigName_Link("serialC.tx_irq", "bbus.irq_sctx");
    SigName_Link("serialC.rx_irq", "bbus.irq_scrx");
    SigName_Link("serialD.tx_irq", "bbus.irq_sdtx");
    SigName_Link("serialD.rx_irq", "bbus.irq_sdrx");

    /* Endian */
    SigName_Link("bbutil.endian_serA", "serialA.endian");
    SigName_Link("bbutil.endian_serB", "serialB.endian");
    SigName_Link("bbutil.endian_serC", "serialC.endian");
    SigName_Link("bbutil.endian_serD", "serialD.endian");
    SigName_Link("bbutil.endian_usb", "ns9750_ohci.endian");
    SigName_Link("mmu.endian", "ns9750_eth.dataendian");
    SigName_Link("flash1.big_endian", "memco.big_endian");

    /* DM9000 network chip */
    SigName_Link("dm9000.irq", "ns9750sysco.extirq0");
}

/*
 * ----------------------------------------------------
 * Create The I2C-Serializers/Deserializers and add
 * devices to the busses
 * ----------------------------------------------------
 */
static void create_i2c_devices(void) {
    I2C_Slave *i2c_slave;
    I2C_SerDes *i2c_serdes0000;
    i2c_serdes0000 = I2C_SerDesNew("i2cbus0000");

    /* Configuration EEPRom with 64kBit */
    i2c_slave = M24Cxx_New("M24C64", "i2cbus0000.cfg_eeprom");
    I2C_SerDesAddSlave(i2c_serdes0000, i2c_slave, 0x50);

    /* Real Time Clock */
    i2c_slave = PCF8563_New("i2cbus0000.rtc");
    I2C_SerDesAddSlave(i2c_serdes0000, i2c_slave, 0x51);
}

/*
 * -----------------------------
 * board_facc_create
 * 	Create a fantasy bord
 * -----------------------------
 */
static Device_Board_t *create(void) {
    BusDevice *dev;
    BusDevice *bbus;
    BBusDMACtrl *bbdma;
    NS9750_MemController *memco;
    ArmCoprocessor *copro;
    PHY_Device *phy;
    board_t *board = malloc(sizeof(*board));
    board->board.run = &run;

    Bus_Init(MMU_InvalidateTlb, 4 * 1024);
    board->mpu = Device_CreateMPU("ARM9");
    copro = MMU9_Create("mmu", BYTE_ORDER_LITTLE, MMU_ARM926EJS | MMUV_NS9750);
    ARM9_RegisterCoprocessor(copro, 15);
    bbus = NS9xxx_BBusNew("NS9360", "bbus");
    bbdma = NS9750_BBusDMA_New("bbdma");

    dev = NS9750Serial_New("serialA", bbdma);
    Mem_AreaAddMapping(dev, 0x90200040, 0x40,
                       MEM_FLAG_WRITABLE | MEM_FLAG_READABLE);
    dev = NS9750Serial_New("serialB", bbdma);
    Mem_AreaAddMapping(dev, 0x90200000, 0x40,
                       MEM_FLAG_WRITABLE | MEM_FLAG_READABLE);
    dev = NS9750Serial_New("serialC", bbdma);
    Mem_AreaAddMapping(dev, 0x90300000, 0x40,
                       MEM_FLAG_WRITABLE | MEM_FLAG_READABLE);
    dev = NS9750Serial_New("serialD", bbdma);
    Mem_AreaAddMapping(dev, 0x90300040, 0x40,
                       MEM_FLAG_WRITABLE | MEM_FLAG_READABLE);

    NS9750_TimerInit("sysco");
    memco = NS9750_MemCoInit("memco");
    dev = NS9750_EthInit("ns9750_eth");

    phy = Lxt971a_New("phy");
    NS9750_EthRegisterPhy(dev, phy, 0);

    NS9750Usb_New("ns9360_usb");

    /* Now Create and Register the devices */
    dev = DRam_New("dram0");
    if (dev) {
        NS9750_RegisterDevice(memco, dev, NS9750_CS4);
    }
    dev = DRam_New("dram1");
    if (dev) {
        NS9750_RegisterDevice(memco, dev, NS9750_CS5);
    }
    dev = DRam_New("dram2");
    if (dev) {
        NS9750_RegisterDevice(memco, dev, NS9750_CS6);
    }
    dev = DRam_New("dram3");
    if (dev) {
        NS9750_RegisterDevice(memco, dev, NS9750_CS7);
    }

    dev = AMDFlashBank_New("flash0");
    if (dev) {
        NS9750_RegisterDevice(memco, dev, NS9750_CS0);
    }
    dev = AMDFlashBank_New("flash1");
    if (dev) {
        NS9750_RegisterDevice(memco, dev, NS9750_CS1);
    } else {
        LOG_Warn(BOARD_NAME, "Warning ! no boot Flash available !");
    }
    dev = DM9000_New("dm9000", 4);
    if (dev) {
        NS9750_RegisterDevice(memco, dev, NS9750_CS2);
    }

    dev = LaccCAN_New();
    NS9750_RegisterDevice(memco, dev, NS9750_CS3);

    create_i2c_devices();
    create_signal_links();
    return &board->board;
}

static int run(Device_Board_t *board) {
    return ((board_t *)board)->mpu->run(((board_t *)board)->mpu);
    return 0;
}


//==============================================================================
//= Function definitions(global)
//==============================================================================
INITIALIZER(init) {
    Device_RegisterBoard(BOARD_NAME, BOARD_DESCRIPTION, &create,
                         BOARD_DEFAULTCONFIG);
}
