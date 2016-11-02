/*
 * ------------------------------------------------------------------------------
 *  Compose a MCF5282LITE Board 
 *
 * (C) 2008 Jochen Karrer
 *   Author: Jochen Karrer
 *
 * -----------------------------------------------------------------------------
 */

#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <termios.h>
#include <sys/ioctl.h>

#include <idecode_cf.h>
#include <cpu_cf.h>
#include <signode.h>
#include <bus.h>
#include <configfile.h>
#include <loader.h>
#include <boards.h>
#include <i2c_serdes.h>
#include <clock.h>
#include <sram.h>
#include <cfm.h>
#include <mcf5282_scm.h>
#include <amdflash.h>

static void
create_clock_links()
{
//	Clock_Link("st.slck","pmc.slck");	
}

static void
create_signal_links()
{
}

static void
create_i2c_devices()
{

}

static int
board_m5282lite_create()
{
        BusDevice *dev;
	MCF5282ScmCsm *scmcsm;
	Bus_Init(NULL,4*1024);
	scmcsm = MCF5282_ScmCsmNew("scmcsm");
	dev=AMDFlashBank_New("flash0");
	MCF5282Csm_RegisterDevice(scmcsm,dev,CSM_CS0);
	//dev = CFM_New("cfm");
#if 0
	if(dev) {
                Mem_AreaAddMapping(dev,0xffe00000,2*1024*1024,MEM_FLAG_WRITABLE | MEM_FLAG_READABLE);
        }
#endif
	dev = SRam_New("sram0");
	if(dev) {
                Mem_AreaAddMapping(dev,0x04000000,64*1024,MEM_FLAG_WRITABLE | MEM_FLAG_READABLE);
        }
	CF_CpuInit();
	create_i2c_devices();
	create_signal_links();
	create_clock_links();
	return 0;
}

static void
board_m5282lite_run(Board *bd) {
        CF_CpuRun();
}

#define DEFAULTCONFIG \
"[global]\n" \
"start_address: 0x00000000\n"\
"cpu_clock: 66000000\n"\
"\n"\
"[sram0]\n" \
"size: 64k"\
"\n"\
"[flash0]\n" \
"type: AM29LV160BB\n"\
"chips: 1\n"\
"\n"\

static Board board_m5282lite = {
        .name = "M5282LITE",
        .description =  "M5282LITE",
        .createBoard =  board_m5282lite_create,
        .runBoard =     board_m5282lite_run,
        .defaultconfig = DEFAULTCONFIG
};

#ifdef _SHARED_
void
_init() {
        fprintf(stderr,"Loading Freescale M5282LITE development Board module\n");
        Board_Register(&board_m5282lite);
}
#endif
