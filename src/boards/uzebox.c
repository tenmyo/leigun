#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <termios.h>
#include <sys/ioctl.h>
#include "avr8_cpu.h"
#include "atm644_usart.h"
#include "atm644_timer02.h"
#include "atm644_timer1.h"
#include "avr8_eeprom.h"
#include "avr8_port.h"
#include "boards.h"
#include "i2c_serdes.h"
#include "avr8_adc.h"
#include "atm644_sysreset.h"
#include "atm644_twi.h"
#include "avr8_gpio.h"
#include "uze_video.h"
#include "fbdisplay.h"
#include "keyboard.h"
#include "rfbserver.h"
#include "uze_snes.h"
#include "sound.h"
#include "uze_timer2.h"
#include "atm644_spi.h"
#include "mmcdev.h"
#include "sd_spi.h"
#include "atm644_extint.h"
#include "sgstring.h"
#include "compiler_extensions.h"

typedef struct Uzebox {
	AVR8_Adc *adc;
	FbDisplay *display;
        Keyboard *keyboard;
	SoundDevice *sounddevice;
	MMCDev *mmcard;
	SD_Spi *sdspi;
} Uzebox;

#define DEFAULTCONFIG \
"[global]\n" \
"cpu_clock: 28618180\n"\
"\n"

static void
link_signals(Uzebox *box) {

	SigName_Link("extint.extint_out0","avr.irq1");
	SigName_Link("extint.extintAck0","avr.irqAck1");
	SigName_Link("extint.extint_out1","avr.irq2");
	SigName_Link("extint.extintAck1","avr.irqAck2");
        SigName_Link("extint.extint_out2","avr.irq3");
        SigName_Link("extint.extintAck2","avr.irqAck3");
        SigName_Link("extint.pcint_out0","avr.irq4");
        SigName_Link("extint.pcintAck0","avr.irqAck4");
        SigName_Link("extint.pcint_out1","avr.irq5");
        SigName_Link("extint.pcintAck1","avr.irqAck5");
        SigName_Link("extint.pcint_out2","avr.irq6");
        SigName_Link("extint.pcintAck2","avr.irqAck6");
        SigName_Link("extint.pcint_out3","avr.irq7");
        SigName_Link("extint.pcintAck3","avr.irqAck7");

        SigName_Link("usart0.rxIrq","avr.irq20");
	SigName_Link("usart0.udreIrq","avr.irq21");
        SigName_Link("usart0.txIrq","avr.irq22");
        SigName_Link("usart0.txIrqAck","avr.irqAck22");

        SigName_Link("usart1.rxIrq","avr.irq28");
	SigName_Link("usart1.udreIrq","avr.irq29");
        SigName_Link("usart1.txIrq","avr.irq30");
        SigName_Link("usart1.txIrqAck","avr.irqAck30");


        SigName_Link("timer0.compaIrq","avr.irq16");
        SigName_Link("timer0.compaAckIrq","avr.irqAck16");
	SigName_Link("timer0.compbIrq","avr.irq17");
	SigName_Link("timer0.compbAckIrq","avr.irqAck17");
	SigName_Link("timer0.ovfIrq","avr.irq18");
	SigName_Link("timer0.ovfAckIrq","avr.irqAck18");

        SigName_Link("timer1.captIrq","avr.irq12");
        SigName_Link("timer1.captAckIrq","avr.irqAck12");
        SigName_Link("timer1.compaIrq","avr.irq13");
        SigName_Link("timer1.compaAckIrq","avr.irqAck13");
	SigName_Link("timer1.compbIrq","avr.irq14");
	SigName_Link("timer1.compbAckIrq","avr.irqAck14");
	SigName_Link("timer1.ovfIrq","avr.irq15");
	SigName_Link("timer1.ovfAckIrq","avr.irqAck15");

        SigName_Link("timer2.compaIrq","avr.irq9");
        SigName_Link("timer2.compaAckIrq","avr.irqAck9");
	SigName_Link("timer2.compbIrq","avr.irq10");
	SigName_Link("timer2.compbAckIrq","avr.irqAck10");
	SigName_Link("timer2.ovfIrq","avr.irq11");
	SigName_Link("timer2.ovfAckIrq","avr.irqAck11");

	SigName_Link("spi0.irq","avr.irq19");
	SigName_Link("spi0.irqAck","avr.irqAck19"); 

	SigName_Link("twi.irq","avr.irq26");
	SigName_Link("adc.irq","avr.irq24"); 
	SigName_Link("adc.irqAck","avr.irqAck24");
	

	SigName_Link("sr.wdReset","avr.wdReset");

#if 0
	/* TWI slave using port C */
        SigName_Link("twi.scl","portC.pvov0");
        SigName_Link("twi.ddoe_scl","portC.ddoe0");
        SigName_Link("twi.pvoe_scl","portC.pvoe0");
        SigName_Link("twi.sda","portC.pvov1");
        SigName_Link("twi.ddoe_sda","portC.ddoe1");
        SigName_Link("twi.pvoe_sda","portC.pvoe1");
#endif

	/* HSync */
	SigName_Link("portB.P0","portC.hsync");

	/* Link the SNES adapters */
	SigName_Link("portA.P0","snes0.data");
	SigName_Link("portA.P1","snes1.data");
	SigName_Link("portA.P2","snes0.latch");
	SigName_Link("portA.P2","snes1.latch");
	SigName_Link("portA.P3","snes0.clock");
	SigName_Link("portA.P3","snes1.clock");

	/* Link the SPI interface to the IO port */
	SigName_Link("spi0.ss","portB.pvov4");
	SigName_Link("spi0.ddoe_ss","portB.ddoe4");
	SigName_Link("spi0.ddov_ss","portB.ddov4");
	SigName_Link("spi0.mosi","portB.pvov5");
	SigName_Link("spi0.ddoe_mosi","portB.ddoe5");
	SigName_Link("spi0.ddov_mosi","portB.ddov5");
	SigName_Link("spi0.sck","portB.pvov7");
	SigName_Link("spi0.ddoe_sck","portB.ddoe7");
	SigName_Link("spi0.ddov_sck","portB.ddov7");

	SigName_Link("spi0.miso","portB.P6");
	SigName_Link("spi0.ddoe_miso","portB.ddoe6");
	SigName_Link("spi0.ddov_miso","portB.ddov6");

	SigName_Link("spi0.pvoe_ss","portB.pvoe4");
	SigName_Link("spi0.pvoe_mosi","portB.pvoe5");
	SigName_Link("spi0.pvoe_miso","portB.pvoe6");
	SigName_Link("spi0.pvoe_sck","portB.pvoe7");


	if(box->sdspi) {
		/* Link the SPI interface to the SD-Card */
		SigName_Link("portB.P5","sdspi0.cmd"); 	/* MOSI */
		SigName_Link("portB.P7","sdspi0.clk"); 	/* SCK */
		SigName_Link("portD.P6","sdspi0.dat3");	/* CS */
		SigName_Link("portB.P6","sdspi0.dat0");	/* MISO */
	}

	/* Connecting the PCINTS to the Port module  */
	SigName_Link("portA.pcint","extint.pcint_in0");
        SigName_Link("portB.pcint","extint.pcint_in1");
        /* SigName_Link("portC.pcint","extint.pcint_in2"); */
        SigName_Link("portD.pcint","extint.pcint_in3");

	/* Link the extint interrupts to the PINs */	
        SigName_Link("portD.P2","extint.extint_in0");
        SigName_Link("portD.P3","extint.extint_in1");
        SigName_Link("portB.P2","extint.extint_in2");

	/* Speed control loop for sound device */
	SigName_Link("avr.throttle.speedUp","sound0.speedUp");
	SigName_Link("avr.throttle.speedDown","sound0.speedDown");

	Clock_Link("usart0.clk","avr.clk");
	Clock_Link("usart1.clk","avr.clk");
	Clock_Link("timer0.clk","avr.clk");
	Clock_Link("timer1.clk","avr.clk");
	Clock_Link("timer2.clk","avr.clk");
	Clock_Link("spi0.clk","avr.clk");
	Clock_Link("twi.clk","avr.clk");
	Clock_Link("adc.clk","avr.clk");
}

static int
board_uzebox_create()
{
	Uzebox *box = sg_new(Uzebox);

	FbDisplay_New("display0",&box->display,&box->keyboard,NULL,&box->sounddevice);
        if(!box->keyboard) {
                fprintf(stderr,"Keyboard creation failed\n");
                sleep(3);
        }

	AVR8_Init("avr");
	ATM644_UsartNew("usart0",0xc0);
	ATM644_UsartNew("usart1",0xc8);
	AVR8_EEPromNew("eeprom","3f4041",2048);
	ATM644_Timer02New("timer0",0x44,0);
	ATM644_Timer1New("timer1");
	if(box->sounddevice == NULL) {
		box->sounddevice = SoundDevice_New("sound0");
	}
	/* ATM644_Timer02New("timer2",0xb0,2); */
	Uze_Timer2New("timer2",box->sounddevice);
	ATM644_TwiNew("twi");

	AVR8_PortNew("portA",0 + 0x20,0x6b);
        AVR8_PortNew("portB",3 + 0x20,0x6c);
	/* AVR8_PortNew("portC",6 + 0x20); */
	Uze_VidPortNew("portC",6 + 0x20,box->display);
        AVR8_PortNew("portD",9 + 0x20,0x73);


	box->adc = AVR8_AdcNew("adc",0x78);
	ATM644_SRNew("sr");

        AVR8_GpioNew("gpior0",0x3e);
        AVR8_GpioNew("gpior1",0x4a);
        AVR8_GpioNew("gpior2",0x4b);

	box->mmcard = MMCard_New("card0");
	if(box->mmcard) {
		box->sdspi = SDSpi_New("sdspi0",box->mmcard);
	}
	if(box->sdspi) {
		ATM644_SpiNew("spi0",0x4c,SDSpi_ByteExchange,box->sdspi);
	} else {
		ATM644_SpiNew("spi0",0x4c,NULL,NULL);
	}
	ATM644_ExtIntNew("extint");

	Uze_SnesNew("snes0",box->keyboard);
	Uze_SnesNew("snes1",box->keyboard);

	link_signals(box);
	return 0;
}

static void
board_uzebox_run(Board *bd) {
        AVR8_Run();
}

static Board board_uzebox = {
        .name = "Uzebox",
        .description =  "Uzebox",
        .createBoard =  board_uzebox_create,
        .runBoard =     board_uzebox_run,
        .defaultconfig = DEFAULTCONFIG
};

__CONSTRUCTOR__ static void
uzebox_init() {
        fprintf(stderr,"Loading Uzebox emulator module\n");
        Board_Register(&board_uzebox);
}
