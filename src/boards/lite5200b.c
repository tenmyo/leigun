/*
 * ------------------------------------------------------------------------------
 *  Compose a LITE5200B Board
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
#include <dram.h>
#include <cfm.h>
#include <ppc/cpu_ppc.h>
//#include <mpc5200_psc.h>
#include <amdflash.h>

static void
board_lite5200b_run(Board *bd) {
	PpcCpu_Run();
}

static int
board_lite5200b_create()
{
	// PpcCpu *cpu = PpcCpu_New(CPU_MPC866P,0);
        Bus_Init(NULL,4*1024);
        BusDevice *dev;
	/* CS0 */
	dev=AMDFlashBank_New("flash0");
	/* CS1 */
	dev=AMDFlashBank_New("flash1");
	/* SDRAM_CS0 */
	dev=DRam_New("dram0");
	/* SDRAM_CS1 */
	dev=DRam_New("dram1");
	return 0;
}

#define DEFAULTCONFIG \
"[global]\n" \
"start_address: 0x00000000\n"\
"cpu_clock: 400000000\n"\
"\n"\
"[flash0]\n" \
"type: S29GL128NR2\n"\
"chips: 1\n"\
"\n" \
"[flash1]\n" \
"type: S29GL128NR2\n"\
"chips: 1\n"\
"\n" \
"[dram0]\n" \
"size: 128M\n" \
"\n" \
"[dram1]\n" \
"size: 128M\n" \
"\n"

static Board board_lite5200b = {
        .name = "LITE5200B",
        .description =  "LITE5200B",
        .createBoard =  board_lite5200b_create,
        .runBoard =     board_lite5200b_run,
        .defaultconfig = DEFAULTCONFIG
};

#ifdef _SHARED_
void
_init() {
        fprintf(stderr,"Loading Freescale LITE5200B development Board module\n");
        Board_Register(&board_lite5200b);
}
#endif

