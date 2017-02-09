//===-- boards/eth_m32.c ------------------------------------------*- C -*-===//
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
/// Compose ETH-M32 board from Ulrich Radig
///
//===----------------------------------------------------------------------===//

// clang-format off
/*
 ************************************************************************************************* 
 * Compose ETH-M32 board from Ulrich Radig
 *
 * Copyright 2012 Jochen Karrer. All rights reserved.
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
#include "avr8/atm644_extint.h"
#include "avr8/atm644_spi.h"
#include "avr8/atm644_sysreset.h"
#include "avr8/atm644_timer02.h"
#include "avr8/atm644_timer1.h"
#include "avr8/atm644_twi.h"
#include "avr8/atm644_usart.h"
#include "avr8/avr8_adc.h"
#include "avr8/avr8_cpu.h"
#include "avr8/avr8_eeprom.h"
#include "avr8/avr8_gpio.h"
#include "avr8/avr8_port.h"
#include "devices/enc28j60/enc28j60.h"
#include "devices/i2c/ds1337.h"
#include "devices/lcd/lcd_hd44780.h"
#include "devices/sdcard/sd_spi.h"

// Leigun Core Headers
#include "bus.h"
#include "clock.h"
#include "core/device.h"
#include "fbdisplay.h"
#include "i2c_serdes.h"
#include "initializer.h"
#include "mmcdev.h"
#include "sgstring.h"
#include "signode.h"

// External headers

// System headers


//==============================================================================
//= Constants(also Enumerations)
//==============================================================================
static const char *BOARD_NAME = "ETH-M32";
static const char *BOARD_DESCRIPTION = "ETH-M32";
static const char *BOARD_DEFAULTCONFIG = "[global]\n"
                                         "cpu_clock: 16000000\n"
                                         "\n"
                                         "[lcd0]\n"
                                         "rows: 2\n"
                                         "cols: 16\n"
                                         "colorset: 1\n"
                                         "backend: rfbserver\n"
                                         "host: 127.0.0.1\n"
                                         "port: 5903\n"
                                         "width: 310\n"
                                         "height: 75\n"
                                         "startupdelay: 0\n"
                                         "exit_on_close: 1\n"
                                         "\n";


//==============================================================================
//= Types
//==============================================================================
typedef struct EthM32 {
    AVR8_Adc *adc;
    FbDisplay *display;
    Keyboard *keyboard;
    MMCDev *mmcard;
    SD_Spi *sdspi;
} EthM32;


//==============================================================================
//= Variables
//==============================================================================


//==============================================================================
//= Function declarations(static)
//==============================================================================
static void link_signals(EthM32 *em32);
static void create_i2c_devices();
static Device_Board_t *create(void);
static int run(Device_Board_t *board);


//==============================================================================
//= Function definitions(static)
//==============================================================================


static void link_signals(EthM32 *em32) {
    SigName_Link("extint.extint_out0", "avr.irq1");
    SigName_Link("extint.extintAck0", "avr.irqAck1");
    SigName_Link("extint.extint_out1", "avr.irq2");
    SigName_Link("extint.extintAck1", "avr.irqAck2");
    SigName_Link("extint.extint_out2", "avr.irq3");
    SigName_Link("extint.extintAck2", "avr.irqAck3");
    SigName_Link("extint.pcint_out0", "avr.irq4");
    SigName_Link("extint.pcintAck0", "avr.irqAck4");
    SigName_Link("extint.pcint_out1", "avr.irq5");
    SigName_Link("extint.pcintAck1", "avr.irqAck5");
    SigName_Link("extint.pcint_out2", "avr.irq6");
    SigName_Link("extint.pcintAck2", "avr.irqAck6");
    SigName_Link("extint.pcint_out3", "avr.irq7");
    SigName_Link("extint.pcintAck3", "avr.irqAck7");

    SigName_Link("usart0.rxIrq", "avr.irq20");
    SigName_Link("usart0.udreIrq", "avr.irq21");
    SigName_Link("usart0.txIrq", "avr.irq22");
    SigName_Link("usart0.txIrqAck", "avr.irqAck22");

    SigName_Link("usart1.rxIrq", "avr.irq28");
    SigName_Link("usart1.udreIrq", "avr.irq29");
    SigName_Link("usart1.txIrq", "avr.irq30");
    SigName_Link("usart1.txIrqAck", "avr.irqAck30");


    SigName_Link("timer0.compaIrq", "avr.irq16");
    SigName_Link("timer0.compaAckIrq", "avr.irqAck16");
    SigName_Link("timer0.compbIrq", "avr.irq17");
    SigName_Link("timer0.compbAckIrq", "avr.irqAck17");
    SigName_Link("timer0.ovfIrq", "avr.irq18");
    SigName_Link("timer0.ovfAckIrq", "avr.irqAck18");

    SigName_Link("timer1.captIrq", "avr.irq12");
    SigName_Link("timer1.captAckIrq", "avr.irqAck12");
    SigName_Link("timer1.compaIrq", "avr.irq13");
    SigName_Link("timer1.compaAckIrq", "avr.irqAck13");
    SigName_Link("timer1.compbIrq", "avr.irq14");
    SigName_Link("timer1.compbAckIrq", "avr.irqAck14");
    SigName_Link("timer1.ovfIrq", "avr.irq15");
    SigName_Link("timer1.ovfAckIrq", "avr.irqAck15");

    SigName_Link("timer2.compaIrq", "avr.irq9");
    SigName_Link("timer2.compaAckIrq", "avr.irqAck9");
    SigName_Link("timer2.compbIrq", "avr.irq10");
    SigName_Link("timer2.compbAckIrq", "avr.irqAck10");
    SigName_Link("timer2.ovfIrq", "avr.irq11");
    SigName_Link("timer2.ovfAckIrq", "avr.irqAck11");

    SigName_Link("spi0.irq", "avr.irq19");
    SigName_Link("spi0.irqAck", "avr.irqAck19");

    SigName_Link("twi.irq", "avr.irq26");
    SigName_Link("adc.irq", "avr.irq24");
    SigName_Link("adc.irqAck", "avr.irqAck24");


    SigName_Link("sr.wdReset", "avr.wdReset");


    /* TWI slave using port C */
    SigName_Link("twi.scl", "portC.pvov0");
    SigName_Link("twi.ddoe_scl", "portC.ddoe0");
    SigName_Link("twi.pvoe_scl", "portC.pvoe0");
    SigName_Link("twi.sda", "portC.pvov1");
    SigName_Link("twi.ddoe_sda", "portC.ddoe1");
    SigName_Link("twi.pvoe_sda", "portC.pvoe1");

    /* HSync */
    /* Link the SPI interface to the IO port */


    SigName_Link("spi0.ss", "portB.pvov4");
    SigName_Link("spi0.ddoe_ss", "portB.ddoe4");
    SigName_Link("spi0.ddov_ss", "portB.ddov4");
    SigName_Link("spi0.mosi", "portB.pvov5");
    SigName_Link("spi0.ddoe_mosi", "portB.ddoe5");
    SigName_Link("spi0.ddov_mosi", "portB.ddov5");
    SigName_Link("spi0.sck", "portB.pvov7");
    SigName_Link("spi0.ddoe_sck", "portB.ddoe7");
    SigName_Link("spi0.ddov_sck", "portB.ddov7");

    SigName_Link("spi0.miso", "portB.P6");
    SigName_Link("spi0.ddoe_miso", "portB.ddoe6");
    SigName_Link("spi0.ddov_miso", "portB.ddov6");

    SigName_Link("spi0.pvoe_ss", "portB.pvoe4");
    SigName_Link("spi0.pvoe_mosi", "portB.pvoe5");
    SigName_Link("spi0.pvoe_miso", "portB.pvoe6");
    SigName_Link("spi0.pvoe_sck", "portB.pvoe7");

#if 0
	if(em32->sdspi) {
		/* Link the SPI interface to the SD-Card */
		SigName_Link("portB.P5","sdspi0.cmd"); 	/* MOSI */
		SigName_Link("portB.P7","sdspi0.clk"); 	/* SCK */
		SigName_Link("portD.P6","sdspi0.dat3");	/* CS */
		SigName_Link("portB.P6","sdspi0.dat0");	/* MISO */
	}
#endif

    /* Connecting the PCINTS to the Port module  */
    SigName_Link("portA.pcint", "extint.pcint_in0");
    SigName_Link("portB.pcint", "extint.pcint_in1");
    /* SigName_Link("portC.pcint","extint.pcint_in2"); */
    SigName_Link("portD.pcint", "extint.pcint_in3");

    /* Connecting the PCINTS to the port modules */
    SigName_Link("portA.pcint", "extint.pcint_in0");
    SigName_Link("portB.pcint", "extint.pcint_in1");
    SigName_Link("portC.pcint", "extint.pcint_in2");
    SigName_Link("portD.pcint", "extint.pcint_in3");

    SigName_Link("portD.P2", "extint.extint_in0");
    SigName_Link("portD.P3", "extint.extint_in1");
    SigName_Link("portB.P2", "extint.extint_in2");


    /* Speed control loop for sound device */
    // SigName_Link("avr.speed_up","sound0.speed_up");
    // SigName_Link("avr.speed_down","sound0.speed_down");

    Clock_Link("usart0.clk", "avr.clk");
    Clock_Link("usart1.clk", "avr.clk");
    Clock_Link("timer0.clk", "avr.clk");
    Clock_Link("timer1.clk", "avr.clk");
    Clock_Link("timer2.clk", "avr.clk");
    Clock_Link("spi0.clk", "avr.clk");
    Clock_Link("twi.clk", "avr.clk");
    Clock_Link("adc.clk", "avr.clk");

    /* Link the network chip */
    SigName_Link("portB.P7", "enc28j60.sck");
    SigName_Link("portB.P6", "enc28j60.miso");
    SigName_Link("portB.P5", "enc28j60.mosi");
    SigName_Link("portB.P2", "enc28j60.irq");
    SigName_Link("portB.P3", "enc28j60.ncs");

    /* Connect the LCD display in 4 bit mode */
    SigName_Link("portC.P0", "lcd0.D4");
    SigName_Link("portC.P1", "lcd0.D5");
    SigName_Link("portC.P2", "lcd0.D6");
    SigName_Link("portC.P3", "lcd0.D7");

    SigName_Link("portC.P4", "lcd0.RS");
    SigName_Link("portC.P5", "lcd0.RW");
    SigName_Link("portC.P6", "lcd0.E");

    // SigName_Link("i2cbus.sda","twi.sda");
    // SigName_Link("i2cbus.scl","twi.scl");

    SigName_Link("portC.P0", "i2cbus.scl");
    SigName_Link("portC.P1", "i2cbus.sda");
}

static void create_i2c_devices() {
    I2C_Slave *i2c_slave;
    I2C_SerDes *i2c_serdes;
    i2c_serdes = I2C_SerDesNew("i2cbus");

    i2c_slave = DS1337_New("i2cbus.ds1337");
    I2C_SerDesAddSlave(i2c_serdes, i2c_slave, 0x68);
}

static Device_Board_t *create(void) {
    EthM32 *em32 = sg_new(EthM32);
    ATMUsartRegisterMap usartRegMap = {
        .addrUCSRA = 0,
        .addrUCSRB = 1,
        .addrUCSRC = 2,
        .addrUBRRL = 4,
        .addrUBRRH = 5,
        .addrUDR = 6,
    };
    Device_Board_t *board;
    board = malloc(sizeof(*board));
    board->run = &run;

    FbDisplay_New("lcd0", &em32->display, &em32->keyboard, NULL, NULL);
    if (!em32->display) {
        fprintf(stderr, "LCD creation failed\n");
    }

    // FbDisplay_New("display0",&em32->display,&em32->keyboard,NULL,NULL);
    // if(!em32->keyboard) {
    //       fprintf(stderr,"Keyboard creation failed\n");
    //       sleep(3);
    //}

    AVR8_Init("avr");
    usartRegMap.addrBase = 0xc0;
    ATM644_UsartNew("usart0", &usartRegMap);
    usartRegMap.addrBase = 0xc8;
    ATM644_UsartNew("usart1", &usartRegMap);
    AVR8_EEPromNew("eeprom", "3f4041", 2048);
    ATM644_Timer02New("timer0", 0x44, 0);
    ATM644_Timer1New("timer1");
    ATM644_Timer02New("timer2", 0xb0, 2);
    ATM644_TwiNew("twi");

    AVR8_PortNew("portA", 0 + 0x20, 0x6b);
    AVR8_PortNew("portB", 3 + 0x20, 0x6c);
    AVR8_PortNew("portC", 6 + 0x20, 0x6d);
    AVR8_PortNew("portD", 9 + 0x20, 0x73);

    em32->adc = AVR8_AdcNew("adc", 0x78);
    ATM644_SRNew("sr");

    AVR8_GpioNew("gpior0", 0x3e);
    AVR8_GpioNew("gpior1", 0x4a);
    AVR8_GpioNew("gpior2", 0x4b);

    em32->mmcard = MMCard_New("card0");
    if (em32->mmcard) {
        em32->sdspi = SDSpi_New("sdspi0", em32->mmcard);
    }
    if (em32->sdspi) {
        ATM644_SpiNew("spi0", 0x4c, SDSpi_ByteExchange, em32->sdspi);
    } else {
        ATM644_SpiNew("spi0", 0x4c, NULL, NULL);
    }
    ATM644_ExtIntNew("extint");
    Enc28j60_New("enc28j60");
    if (em32->display) {
        HD44780_LcdNew("lcd0", em32->display);
    }
    create_i2c_devices();
    link_signals(em32);
    return board;
}

static int run(Device_Board_t *board) {
    AVR8_Run();
    return 0;
}


//==============================================================================
//= Function definitions(global)
//==============================================================================
INITIALIZER(init) {
    Device_RegisterBoard(BOARD_NAME, BOARD_DESCRIPTION, &create,
                         BOARD_DEFAULTCONFIG);
}
