/**
 *********************************************************************************
 * Create a M32C fantasy board.
 *********************************************************************************
 */
#if 0
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include "boards.h"
#include "signode.h"
#include "sram.h"
#include "cpu_m32c.h"
#include "flash_m32c.h"
#include "irq_m32c87.h"
#include "uart_m32c.h"
#include "uart56_m32c.h"
#include "iio_m32c.h"
#include "sysco_m32c.h"
#include "timer_m32c.h"
#include "can_m32c.h"
#include "adc_m32c.h"
#include "pio_m32c.h"
#include "i2c_serdes.h"
#include "m24cxx.h"
#include "mmcard.h"
#include "sd_spi.h"
#include "ds1388.h"
#include "m25p16.h"
#include "m32c_crc.h"
#include "compiler_extensions.h"

static void
create_i2c_devices()
{
	I2C_Slave *i2c_slave;
	I2C_SerDes *i2c_serdes;
	i2c_serdes = I2C_SerDesNew("i2c0");

	/* Configuration EEPRom with 64kBit */
	i2c_slave = M24Cxx_New("M24C64", "i2c0.cfg_eeprom");
	I2C_SerDesAddSlave(i2c_serdes, i2c_slave, 0x54);

	/* Realtime clock */
	i2c_slave = DS1388_New("i2c0.rtc");
	I2C_SerDesAddSlave(i2c_serdes, i2c_slave, 0xd0 >> 1);

}

static void
link_signals(void)
{
	SigName_Link("uart0.txirq", "intco.irq17");
	SigName_Link("uart0.rxirq", "intco.irq18");
	SigName_Link("uart1.txirq", "intco.irq19");
	SigName_Link("uart1.rxirq", "intco.irq20");
	SigName_Link("uart2.txirq", "intco.irq33");
	SigName_Link("uart2.rxirq", "intco.irq34");
	SigName_Link("uart3.txirq", "intco.irq35");
	SigName_Link("uart3.rxirq", "intco.irq36");
	SigName_Link("uart4.txirq", "intco.irq37");
	SigName_Link("uart4.rxirq", "intco.irq38");
	SigName_Link("uart5.txirq", "iio.irqU5t");
	SigName_Link("uart5.rxirq", "iio.irqU5r");
	SigName_Link("uart6.txirq", "iio.irqU6t");
	SigName_Link("uart6.rxirq", "iio.irqU6r");

	SigName_Link("timerA0.irq", "intco.irq12");
	SigName_Link("timerA1.irq", "intco.irq13");
	SigName_Link("timerA2.irq", "intco.irq14");
	SigName_Link("timerA3.irq", "intco.irq15");
	SigName_Link("timerA4.irq", "intco.irq16");

	SigName_Link("timerB0.irq", "intco.irq21");
	SigName_Link("timerB1.irq", "intco.irq22");
	SigName_Link("timerB2.irq", "intco.irq23");
	SigName_Link("timerB3.irq", "intco.irq24");
	SigName_Link("timerB4.irq", "intco.irq25");

	SigName_Link("adc0.irq", "intco.irq42");
	/* Link the Intelligent io controller with the main intco */
	SigName_Link("iio.irqIIO0", "intco.irq44");
	SigName_Link("iio.irqIIO1", "intco.irq45");
	SigName_Link("iio.irqIIO2", "intco.irq46");
	SigName_Link("iio.irqIIO3", "intco.irq47");
	SigName_Link("iio.irqIIO4", "intco.irq48");
	SigName_Link("iio.irqIIO5", "intco.irq49");
	SigName_Link("iio.irqIIO6", "intco.irq50");
	SigName_Link("iio.irqIIO7", "intco.irq51");
	SigName_Link("iio.irqIIO8", "intco.irq52");
	SigName_Link("iio.irqIIO9", "intco.irq53");
	SigName_Link("iio.irqIIO10", "intco.irq54");
	SigName_Link("iio.irqIIO11", "intco.irq57");

	/* Connect the CAN bus controllers to the CPU */
	SigName_Link("iio.irqCan00", "can0.rxirq");
	SigName_Link("iio.irqCan01", "can0.txirq");

	SigName_Link("iio.irqCan10", "can1.rxirq");
	SigName_Link("iio.irqCan11", "can1.txirq");

	/* Link the I2C bus to the CPU */
	SigName_Link("pio.P6.0", "i2c0.sda");
	SigName_Link("pio.P7.2", "i2c0.scl");
	SigName_Link("i2c0.cfg_eeprom.wc", "GND");

}

static int
board_m32cfant_create(void)
{

	BusDevice *dev, *intco;
	M32C_Adc *adc;

	Bus_Init(NULL, 1 * 1024);
	intco = M32C87IntCo_New("intco");
	M32C_CpuNew("m32c", intco);
	Mem_AreaAddMapping(intco, 0x000000, 0x00400, MEM_FLAG_WRITABLE | MEM_FLAG_READABLE);
	dev = M32CFlash_New("iflash");
	if (dev) {
		Mem_AreaAddMapping(dev, 0xf00000, 0x100000, MEM_FLAG_WRITABLE | MEM_FLAG_READABLE);
	}
	dev = SRam_New("iram0");
	if (dev) {
		Mem_AreaAddMapping(dev, 0x000400, 0x0C000, MEM_FLAG_WRITABLE | MEM_FLAG_READABLE);
	} else {
		fprintf(stderr, "IRAM missing\n");
		exit(1);
	}
	dev = M32CIIO_New("iio");
	Mem_AreaAddMapping(dev, 0x000000, 0x00400, MEM_FLAG_WRITABLE | MEM_FLAG_READABLE);
	dev = M32C_SyscoNew("cgen");
	Mem_AreaAddMapping(dev, 0x000000, 0x00400, MEM_FLAG_WRITABLE | MEM_FLAG_READABLE);

	dev = M32CUart_New("uart0", 0);
	Mem_AreaAddMapping(dev, 0x000000, 0x00400, MEM_FLAG_WRITABLE | MEM_FLAG_READABLE);
	dev = M32CUart_New("uart1", 1);
	Mem_AreaAddMapping(dev, 0x000000, 0x00400, MEM_FLAG_WRITABLE | MEM_FLAG_READABLE);
	dev = M32CUart_New("uart2", 2);
	Mem_AreaAddMapping(dev, 0x000000, 0x00400, MEM_FLAG_WRITABLE | MEM_FLAG_READABLE);
	dev = M32CUart_New("uart3", 3);
	Mem_AreaAddMapping(dev, 0x000000, 0x00400, MEM_FLAG_WRITABLE | MEM_FLAG_READABLE);
	dev = M32CUart_New("uart4", 4);
	Mem_AreaAddMapping(dev, 0x000000, 0x00400, MEM_FLAG_WRITABLE | MEM_FLAG_READABLE);
	dev = M32CUart56_New("uart5", 0);
	Mem_AreaAddMapping(dev, 0x000000, 0x00400, MEM_FLAG_WRITABLE | MEM_FLAG_READABLE);
	dev = M32CUart56_New("uart6", 1);
	Mem_AreaAddMapping(dev, 0x000000, 0x00400, MEM_FLAG_WRITABLE | MEM_FLAG_READABLE);

	dev = M32CTimerBlock_New("timer");
	Mem_AreaAddMapping(dev, 0x000000, 0x00400, MEM_FLAG_WRITABLE | MEM_FLAG_READABLE);

	dev = M32C_CanNew("can0", 0);
	Mem_AreaAddMapping(dev, 0x000000, 0x00400, MEM_FLAG_WRITABLE | MEM_FLAG_READABLE);
	dev = M32C_CanNew("can1", 1);
	Mem_AreaAddMapping(dev, 0x000000, 0x00400, MEM_FLAG_WRITABLE | MEM_FLAG_READABLE);
	adc = M32C_AdcNew("adc0");
	dev = (BusDevice *) adc;
	Mem_AreaAddMapping(dev, 0x000000, 0x00400, MEM_FLAG_WRITABLE | MEM_FLAG_READABLE);

	dev = M32C_PioNew("pio");
	Mem_AreaAddMapping(dev, 0x000000, 0x00400, MEM_FLAG_WRITABLE | MEM_FLAG_READABLE);

	dev = M32C_CRCNew("crc", M32C87_REGSET_CRCGEN);
	Mem_AreaAddMapping(dev, 0x000000, 0x00400, MEM_FLAG_WRITABLE | MEM_FLAG_READABLE);

	create_i2c_devices();
	link_signals();
	return 0;
}

static void
board_m32cfant_run(Board * bd)
{
	M32C_Run();
}

#define DEFAULTCONFIG \
"[global]\n" \
"cpu_clock: 24000000\n" \
"\n" \
"[iflash]\n" \
"size: 1024k\n" \
"\n" \
"[iram0]\n" \
"size: 48k\n" \
"\n"

static Board board_m32cfant = {
	.name = "M32CFANT",
	.description = "M32C Fantasy board",
	.createBoard = board_m32cfant_create,
	.runBoard = board_m32cfant_run,
	.defaultconfig = DEFAULTCONFIG
};

__CONSTRUCTOR__ static void
m32cfant_init()
{
	fprintf(stderr, "Loading M32C Fantasy module\n");
	Board_Register(&board_m32cfant);
}

#endif
