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

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>

#include "signode.h"
#include "mmu_arm9.h"
#include "ste10_100.h"
#include "fio.h"
#include "bus.h"
#include "amdflash.h"
#include "configfile.h"
#include "phy.h"
#include "loader.h"
#include "i2c_serdes.h"
#include "boards.h"
#include "netx_uart.h"
#include "netx_gpio.h"
#include "netx_xpec.h"
#include "netx_xmac.h"
#include "netx_sysco.h"
#include "dram.h"
#include "sram.h"
#include "pl190_irq.h"
#include "compiler_extensions.h"

static void
create_signal_links(void)
{
        SigName_Link("arm.irq","vic.irq");
        SigName_Link("arm.fiq","vic.fiq");
        SigName_Link("gpio.timer0.irq","vic.nVICINTSOURCE1");
        SigName_Link("gpio.timer1.irq","vic.nVICINTSOURCE2");
        SigName_Link("gpio.timer2.irq","vic.nVICINTSOURCE3");
#if 0
        SigName_Link("systime_ns.irq","vic.nVICINTSOURCE4");
        SigName_Link("systime_s.irq","vic.nVICINTSOURCE5");
        SigName_Link("gpio.15.irq","vic.nVICINTSOURCE6");
#endif
        SigName_Link("uart0.irq","vic.nVICINTSOURCE8");
        SigName_Link("uart1.irq","vic.nVICINTSOURCE9");
        SigName_Link("uart2.irq","vic.nVICINTSOURCE10");
#if 0
        SigName_Link("usb.irq","vic.nVICINTSOURCE11");
        SigName_Link("spi.irq","vic.nVICINTSOURCE12");
        SigName_Link("i2c.irq","vic.nVICINTSOURCE13");
        SigName_Link("lcd.irq","vic.nVICINTSOURCE14");
        SigName_Link("hif.irq","vic.nVICINTSOURCE15");
#endif
        SigName_Link("gpio.irq","vic.nVICINTSOURCE16");
#if 0
        SigName_Link("com0.irq","vic.nVICINTSOURCE17");
        SigName_Link("com1.irq","vic.nVICINTSOURCE18");
        SigName_Link("com2.irq","vic.nVICINTSOURCE19");
        SigName_Link("com3.irq","vic.nVICINTSOURCE20");
        SigName_Link("msync0.irq","vic.nVICINTSOURCE21");
        SigName_Link("msync1.irq","vic.nVICINTSOURCE22");
        SigName_Link("msync2.irq","vic.nVICINTSOURCE23");
        SigName_Link("msync3.irq","vic.nVICINTSOURCE24");
        SigName_Link("intphy.irq","vic.nVICINTSOURCE25");
        SigName_Link("isoarea.irq","vic.nVICINTSOURCE26");
#endif
        SigName_Link("gpio.timer3.irq","vic.nVICINTSOURCE29");
        SigName_Link("gpio.timer4.irq","vic.nVICINTSOURCE30");
}


static int
board_nxdb500_create()
{
        ArmCoprocessor *copro;
        BusDevice *dev;

        Bus_Init(MMU_InvalidateTlb,4*1024);
        ARM9_New();
        copro = MMU9_Create("mmu",TARGET_BYTEORDER,MMU_ARM926EJS);
        ARM9_RegisterCoprocessor(copro,15);

	dev = NetXSysco_New("sysco");
        Mem_AreaAddMapping(dev,0x00100000,0x300,MEM_FLAG_WRITABLE | MEM_FLAG_READABLE);

	dev = NetXUart_New("uart0");
        Mem_AreaAddMapping(dev,0x00100a00,0x40,MEM_FLAG_WRITABLE | MEM_FLAG_READABLE);
	dev = NetXUart_New("uart1");
        Mem_AreaAddMapping(dev,0x00100a40,0x40,MEM_FLAG_WRITABLE | MEM_FLAG_READABLE);
	dev = NetXUart_New("uart2");
        Mem_AreaAddMapping(dev,0x00100a80,0x40,MEM_FLAG_WRITABLE | MEM_FLAG_READABLE);

	dev = NetXGpio_New("gpio");
        Mem_AreaAddMapping(dev,0x00100800,0xff,MEM_FLAG_WRITABLE | MEM_FLAG_READABLE);

	dev = XMac_New("xmac0");
        Mem_AreaAddMapping(dev,0x00160000,0x1000,MEM_FLAG_WRITABLE | MEM_FLAG_READABLE);
	dev = XMac_New("xmac1");
        Mem_AreaAddMapping(dev,0x00161000,0x1000,MEM_FLAG_WRITABLE | MEM_FLAG_READABLE);
	dev = XMac_New("xmac2");
        Mem_AreaAddMapping(dev,0x00162000,0x1000,MEM_FLAG_WRITABLE | MEM_FLAG_READABLE);
	dev = XMac_New("xmac3");
        Mem_AreaAddMapping(dev,0x00163000,0x1000,MEM_FLAG_WRITABLE | MEM_FLAG_READABLE);

	dev = XPec_New("xpec0");
        Mem_AreaAddMapping(dev,0x00170000,0x4000,MEM_FLAG_WRITABLE | MEM_FLAG_READABLE);
	dev = XPec_New("xpec1");
        Mem_AreaAddMapping(dev,0x00174000,0x4000,MEM_FLAG_WRITABLE | MEM_FLAG_READABLE);
	dev = XPec_New("xpec2");
        Mem_AreaAddMapping(dev,0x00178000,0x4000,MEM_FLAG_WRITABLE | MEM_FLAG_READABLE);
	dev = XPec_New("xpec3");
        Mem_AreaAddMapping(dev,0x0017c000,0x4000,MEM_FLAG_WRITABLE | MEM_FLAG_READABLE);
	dev = PL190_New("vic");
        Mem_AreaAddMapping(dev,0x001ff000,0x400,MEM_FLAG_WRITABLE | MEM_FLAG_READABLE);
	
	dev = DRam_New("dram0");
        if(dev) {
                Mem_AreaAddMapping(dev,0x80000000,0x40000000,MEM_FLAG_WRITABLE | MEM_FLAG_READABLE);
        }

	dev = SRam_New("sram0");
        Mem_AreaAddMapping(dev,0x00000000,0x00008000,MEM_FLAG_WRITABLE | MEM_FLAG_READABLE);
	dev = SRam_New("sram1");
        Mem_AreaAddMapping(dev,0x00008000,0x00008000,MEM_FLAG_WRITABLE | MEM_FLAG_READABLE);
	dev = SRam_New("sram2");
        Mem_AreaAddMapping(dev,0x00010000,0x00008000,MEM_FLAG_WRITABLE | MEM_FLAG_READABLE);
	dev = SRam_New("sram3");
        Mem_AreaAddMapping(dev,0x00008000,0x00008000,MEM_FLAG_WRITABLE | MEM_FLAG_READABLE);

	dev = SRam_New("backupram");
        Mem_AreaAddMapping(dev,0x00300000,0x00004000,MEM_FLAG_WRITABLE | MEM_FLAG_READABLE);

	dev = SRam_New("tcdm");
        Mem_AreaAddMapping(dev,0x10000000,0x00002000,MEM_FLAG_WRITABLE | MEM_FLAG_READABLE);

	dev=AMDFlashBank_New("flash0");
        if(dev) {
                Mem_AreaAddMapping(dev,0xC0000000,0x40000000,MEM_FLAG_WRITABLE | MEM_FLAG_READABLE);
        }

        //create_i2c_devices();
        create_signal_links();
        return 0;
}


static void
board_nxdb500_run(Board *bd) {
        ARM9_Run();
}

#define DEFAULTCONFIG \
"[global]\n" \
"start_address: 0\n" \
"cpu_clock: 200000000\n" \
"\n" \
"[dram0]\n" \
"size: 64M\n" \
"\n" \
"[loader]\n" \
"load_address: 0x50000000\n"\
"\n"\
"[sram0]\n" \
"size: 32k\n" \
"\n" \
"[sram1]\n" \
"size: 32k\n" \
"\n" \
"[sram2]\n" \
"size: 32k\n" \
"\n" \
"[sram3]\n" \
"size: 32k\n" \
"\n" \
"[tcdm]\n" \
"size: 8k\n" \
"\n" \
"[backupram]\n" \
"size: 16k\n" \
"\n" \
"[dram1]\n"\
"size: 32M\n"\
"\n"\
"[flash0]\n"\
"type: AM29LV256ML\n"\
"chips: 1\n"\
"\n"
 
static Board board_nxdb500 = {
        .name = "NXDB500",
        .description =  "NXDB500 ARM Controller Card",
        .createBoard =  board_nxdb500_create,
        .runBoard =     board_nxdb500_run,
        .defaultconfig = DEFAULTCONFIG
};

__CONSTRUCTOR__ static void
nxbd500_init() {
        fprintf(stderr,"Loading NXDB500 Board module\n");
        Board_Register(&board_nxdb500);
}
