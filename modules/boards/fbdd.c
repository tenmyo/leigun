//===-- boards/fbdd.c ---------------------------------------------*- C -*-===//
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
/// Compose an fbdd ARM controller card
///
//===----------------------------------------------------------------------===//

// clang-format off
/*
 *************************************************************************************************
 *  Compose an fbdd ARM controller card
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
#include "controllers/ns9750/ns9750_bbdma.h"
#include "controllers/ns9750/ns9750_bbus.h"
#include "controllers/ns9750/ns9750_eth.h"
#include "controllers/ns9750/ns9750_mem.h"
#include "controllers/ns9750/ns9750_pci.h"
#include "controllers/ns9750/ns9750_serial.h"
#include "controllers/ns9750/ns9750_timer.h"
#include "controllers/ns9750/ns9750_usb.h"
#include "controllers/ns9750/ns9xxx_i2c.h"
#include "devices/can/lacc_can.h"
#include "devices/fbdd_02_02/fbdd_02_02.h"
#include "devices/i2c/ads7828.h"
#include "devices/i2c/lm75.h"
#include "devices/i2c/m24cxx.h"
#include "devices/i2c/max6651.h"
#include "devices/i2c/pca9544.h"
#include "devices/i2c/pcf8563.h"
#include "devices/i2c/pcf8575.h"
#include "devices/i2c/pcf8591.h"
#include "devices/phy/lxt971a.h"
#include "devices/phy/phy.h"
#include "ml6652.h"
#include "ste10_100.h"

// Leigun Core Headers
#include "bus.h"
#include "core/device.h"
#include "core/logging.h"
#include "dram.h"
#include "i2c_serdes.h"
#include "signode.h"

// External headers

// System headers


//==============================================================================
//= Constants(also Enumerations)
//==============================================================================
#define BOARD_NAME "FBDD"
#define BOARD_DESCRIPTION "FBDD NS9750 ARM Controller Card"
#define BOARD_DEFAULTCONFIG                                                    \
    "[global]\n"                                                               \
    "start_address: 0\n"                                                       \
    "[dram0]\n"                                                                \
    "size: 32M\n"                                                              \
    "\n"                                                                       \
    "[loader]\n"                                                               \
    "load_address: 0x50000000\n"                                               \
    "\n"                                                                       \
    "[dram1]\n"                                                                \
    "size: 32M\n"                                                              \
    "\n"                                                                       \
    "[flash1]\n"                                                               \
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
static void create_i2c_devices(void);
static Device_Board_t *create(void);


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
    SigName_Link("i2cbus1000.sda", "bbutil.gpio.18");
    SigName_Link("i2cbus1000.scl", "bbutil.gpio.29");

    SigName_Link("i2cbus1000.mux.sda", "i2cbus1000.sda");
    SigName_Link("i2cbus1000.mux.scl", "i2cbus1000.scl");

    SigName_Link("i2cbus1400.eeprom.wc", "bbutil.gpio.11");
    SigName_Link("i2cbus1500.eeprom.wc", "bbutil.gpio.11");
    SigName_Link("i2cbus1500.eeprom_l.wc", "bbutil.gpio.11");
    SigName_Link("i2cbus1600.eeprom.wc", "bbutil.gpio.11");
    SigName_Link("i2cbus1600.eeprom_l.wc", "bbutil.gpio.11");
    SigName_Link("i2cbus1700.fan_eeprom.wc", "bbutil.gpio.11");
    SigName_Link("i2cbus1700.ps1_eeprom.wc", "bbutil.gpio.11");
    SigName_Link("i2cbus1700.ps2_eeprom.wc", "bbutil.gpio.11");
    SigName_Link("i2cbus1700.bpl_eeprom.wc", "bbutil.gpio.11");

    SigName_Link("i2cbus1400.sda", "i2cbus1000.mux.sd0"); /* DZBB */
    SigName_Link("i2cbus1400.scl", "i2cbus1000.mux.sc0");
    SigName_Link("i2cbus1500.sda", "i2cbus1000.mux.sd1"); /* RB0 */
    SigName_Link("i2cbus1500.scl", "i2cbus1000.mux.sc1");
    SigName_Link("i2cbus1600.sda", "i2cbus1000.mux.sd2"); /* RB1 */
    SigName_Link("i2cbus1600.scl", "i2cbus1000.mux.sc2");
    SigName_Link("i2cbus1700.sda", "i2cbus1000.mux.sd3"); /* AO */
    SigName_Link("i2cbus1700.scl", "i2cbus1000.mux.sc3");

    /* Netsilicon CPU-I2C */
    SigName_Link("ns9750_i2c.sda", "i2cbus1000.sda");
    SigName_Link("ns9750_i2c.scl", "i2cbus1000.scl");
    SigName_Link("ns9750_i2c.reset", "bbutil.rst_i2c");
    SigName_Link("ns9750_i2c.irq", "bbus.irq_i2c");
    // SigName_Link("ns9750_i2c.irq","sysco.irq_i2c");
    SigName_Link("ns9750sysco.irq14", "ns9750_i2c.irq");
    SigName_Link("serialA.tx_irq", "bbus.irq_satx");
    SigName_Link("serialA.rx_irq", "bbus.irq_sarx");
    SigName_Link("serialB.tx_irq", "bbus.irq_sbtx");
    SigName_Link("serialB.rx_irq", "bbus.irq_sbrx");
    SigName_Link("serialC.tx_irq", "bbus.irq_sctx");
    SigName_Link("serialC.rx_irq", "bbus.irq_scrx");
    SigName_Link("serialD.tx_irq", "bbus.irq_sdtx");
    SigName_Link("serialD.rx_irq", "bbus.irq_sdrx");
    SigName_Link("ns9750_eth.rx_irq", "ns9750sysco.irq4");
    SigName_Link("ns9750_eth.tx_irq", "ns9750sysco.irq5");

    /* CPU Internal DMA request links */
    SigName_Link("serialA.TxDmaReq", "bbdma.1.FbrDmaReq");
    SigName_Link("serialB.TxDmaReq", "bbdma.3.FbrDmaReq");
    SigName_Link("serialC.TxDmaReq", "bbdma.5.FbrDmaReq");
    SigName_Link("serialD.TxDmaReq", "bbdma.7.FbrDmaReq");

    /* Serial Interfaces Endian */
    SigName_Link("bbutil.endian_serA", "serialA.endian");
    SigName_Link("bbutil.endian_serB", "serialB.endian");
    SigName_Link("bbutil.endian_serC", "serialC.endian");
    SigName_Link("bbutil.endian_serD", "serialD.endian");
    SigName_Link("bbutil.endian_usb", "ns9750_ohci.endian");
    SigName_Link("mmu.endian", "ns9750_eth.dataendian");
    SigName_Link("mmu.endian", "ns9750_pci.cpu_endian");
    SigName_Link("flash1.big_endian", "memco.big_endian");

    /* Link the CAN-Bus Interrupts */
    SigName_Link("acc_can0.irq", "ns9750sysco.extirq2");
    SigName_Link("acc_can1.irq", "ns9750sysco.extirq1");

    /* Fbdd cpld links */
    SigName_Link("FCpld.addat0", "bbutil.gpio.33");
    SigName_Link("FCpld.addat1", "bbutil.gpio.34");
    SigName_Link("FCpld.addat2", "bbutil.gpio.35");
    SigName_Link("FCpld.addat3", "bbutil.gpio.36");
    SigName_Link("FCpld.dir", "bbutil.gpio.37");
    SigName_Link("FCpld.clk", "bbutil.gpio.38");
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
    I2C_SerDes *i2c_serdes1000;
    I2C_SerDes *i2c_serdes1400;
    I2C_SerDes *i2c_serdes1500;
    I2C_SerDes *i2c_serdes1600;
    I2C_SerDes *i2c_serdes1700;
    i2c_serdes0000 = I2C_SerDesNew("i2cbus0000");
    /* second bus */
    i2c_serdes1000 = I2C_SerDesNew("i2cbus1000");
    /* Sub-Busses behind multiplexer */
    i2c_serdes1400 = I2C_SerDesNew("i2cbus1400"); /* DZBB */
    i2c_serdes1500 = I2C_SerDesNew("i2cbus1500"); /* RB0  */
    i2c_serdes1600 = I2C_SerDesNew("i2cbus1600"); /* RB1  */
    i2c_serdes1700 = I2C_SerDesNew("i2cbus1700"); /* AO   */

    /* Configuration EEPRom with 64kBit */
    i2c_slave = M24Cxx_New("M24C64", "i2cbus0000.cfg_eeprom");
    I2C_SerDesAddSlave(i2c_serdes0000, i2c_slave, 0x50);

    /* Real Time Clock */
    i2c_slave = PCF8563_New("i2cbus0000.rtc");
    I2C_SerDesAddSlave(i2c_serdes0000, i2c_slave, 0x51);

    /* Second I2C-Bus */

    /* I2C-Multiplexer */
    i2c_slave = PCA9544_New("i2cbus1000.mux");
    I2C_SerDesAddSlave(i2c_serdes1000, i2c_slave, 0x70);

    /* Card */
    i2c_slave = PCF8575_New("i2cbus1400.plugged");
    I2C_SerDesAddSlave(i2c_serdes1400, i2c_slave, 0x20);
    i2c_slave = PCF8575_New("i2cbus1400.slot_id");
    I2C_SerDesAddSlave(i2c_serdes1400, i2c_slave, 0x22);
    i2c_slave = PCF8575_New("i2cbus1400.signals");
    I2C_SerDesAddSlave(i2c_serdes1400, i2c_slave, 0x24);
    i2c_slave = M24Cxx_New("M24C02", "i2cbus1400.eeprom");
    I2C_SerDesAddSlave(i2c_serdes1400, i2c_slave, 0x52);
    i2c_slave = LM75_New("i2cbus1400.card_temp");
    I2C_SerDesAddSlave(i2c_serdes1400, i2c_slave, 0x48);
    i2c_slave = PCF8591_New("i2cbus1400.card_voltages");
    I2C_SerDesAddSlave(i2c_serdes1400, i2c_slave, 0x4c);

    /* Module 1 */
    i2c_slave = M24Cxx_New("M24C02", "i2cbus1500.eeprom");
    I2C_SerDesAddSlave(i2c_serdes1500, i2c_slave, 0x50);
    i2c_slave = M24Cxx_New("M24C64", "i2cbus1500.eeprom_l");
    I2C_SerDesAddSlave(i2c_serdes1500, i2c_slave, 0x54);
    i2c_slave = ADS7828_New("i2cbus1500.ad");
    I2C_SerDesAddSlave(i2c_serdes1500, i2c_slave, 0x48);

    /* Module 2 */
    i2c_slave = M24Cxx_New("M24C02", "i2cbus1600.eeprom");
    I2C_SerDesAddSlave(i2c_serdes1600, i2c_slave, 0x50);
    i2c_slave = M24Cxx_New("M24C64", "i2cbus1600.eeprom_l");
    I2C_SerDesAddSlave(i2c_serdes1600, i2c_slave, 0x54);
    i2c_slave = ADS7828_New("i2cbus1600.ad");
    I2C_SerDesAddSlave(i2c_serdes1600, i2c_slave, 0x48);

    /* Fan Controllers */
    i2c_slave = MAX6651_New("i2cbus1700.fan0");
    I2C_SerDesAddSlave(i2c_serdes1700, i2c_slave, 0x1b);
    i2c_slave = MAX6651_New("i2cbus1700.fan1");
    I2C_SerDesAddSlave(i2c_serdes1700, i2c_slave, 0x4b);
    i2c_slave = M24Cxx_New("M24C02", "i2cbus1700.fan_eeprom");
    I2C_SerDesAddSlave(i2c_serdes1700, i2c_slave, 0x52);
    i2c_slave = M24Cxx_New("M24C02", "i2cbus1700.ps1_eeprom");
    I2C_SerDesAddSlave(i2c_serdes1700, i2c_slave, 0x54);
    i2c_slave = M24Cxx_New("M24C02", "i2cbus1700.ps2_eeprom");
    I2C_SerDesAddSlave(i2c_serdes1700, i2c_slave, 0x55);
    i2c_slave = M24Cxx_New("M24C02", "i2cbus1700.bpl_eeprom");
    I2C_SerDesAddSlave(i2c_serdes1700, i2c_slave, 0x57);
    i2c_slave = LM75_New("i2cbus1700.fan_temp");
    I2C_SerDesAddSlave(i2c_serdes1700, i2c_slave, 0x4a);
    i2c_slave = LM75_New("i2cbus1700.ps1_temp");
    I2C_SerDesAddSlave(i2c_serdes1700, i2c_slave, 0x4c);
    i2c_slave = LM75_New("i2cbus1700.ps2_temp");
    I2C_SerDesAddSlave(i2c_serdes1700, i2c_slave, 0x4d);
}

/*
 * -------------------------------------------------------------------------------
 *
 * -------------------------------------------------------------------------------
 */
static Device_Board_t *create(void) {
    BusDevice *dev;
    BusDevice *bbus;
    ArmCoprocessor *copro;
    NS9750_MemController *memco;
    BBusDMACtrl *bbdma;
    PHY_Device *phy;
    PCI_Function *bridge;
    board_t *board = calloc(1, sizeof(*board));
    board->base.base.self = board;

    Bus_Init(MMU_InvalidateTlb, 4 * 1024);
    board->mpu = Device_CreateMPU("ARM9");
    copro = MMU9_Create("mmu", BYTE_ORDER_LITTLE, MMU_ARM926EJS | MMUV_NS9750);
    ARM9_RegisterCoprocessor(copro, 15);

    bbus = NS9xxx_BBusNew("NS9750", "bbus");
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
    phy = ML6652_New();
    NS9750_EthRegisterPhy(dev, phy, 1);
    phy = ML6652_New();
    NS9750_EthRegisterPhy(dev, phy, 17);
    NS9750Usb_New("ns9750_usb");
    NS9xxx_I2CNew("ns9750_i2c");

    bridge = NS9750_PciInit("ns9750_pci", PCI_DEVICE(0));
    STE_New("ste_0", bridge, PCI_DEVICE(1), PCI_INTA);
    STE_New("ste_1", bridge, PCI_DEVICE(2), PCI_INTB);
    STE_New("ste_2", bridge, PCI_DEVICE(3), PCI_INTC);

    FbddCpld_New("FCpld");

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

    dev = LaccCAN_New();
    NS9750_RegisterDevice(memco, dev, NS9750_CS3);

    create_i2c_devices();
    create_signal_links();
    return &board->base;
}


//==============================================================================
//= Function definitions(global)
//==============================================================================
DEVICE_REGISTER_BOARD(BOARD_NAME, BOARD_DESCRIPTION, &create,
                      BOARD_DEFAULTCONFIG);
