/*
 * ------------------------------------------------------------------------------
 *  Compose a AT91_SAM7A3-EK Development Board
 *
 * (C) 2009 Jochen Karrer
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

#include <signode.h>
#include <mmu_arm9.h>
#include <bus.h>
#include <configfile.h>
#include <loader.h>
#include <boards.h>
#include <i2c_serdes.h>
#include <clock.h>
#include <sram.h>
#include <at91sam_efc.h>

static void
board_sam7a3_ek_run(Board * bd)
{
	ARM9_Run();
}

static int
board_sam7a3_ek_create()
{
//      ArmCoprocessor *copro;
	BusDevice *dev, *flash, *efc;

	Bus_Init(MMU_InvalidateTlb, 4 * 1024);
	ARM9_New();
	dev = SRam_New("iram");
	Mem_AreaAddMapping(dev, 0x00200000, 1024 * 1024, MEM_FLAG_WRITABLE | MEM_FLAG_READABLE);
	//Mem_AreaAddMapping(dev,0x00000000,1024*1024,MEM_FLAG_WRITABLE | MEM_FLAG_READABLE);
	AT91SAM7_EfcNew(&flash, &efc, "efc", "iflash");

	/* Before remap only */
	Mem_AreaAddMapping(flash, 0x00000000, 1024 * 1024, MEM_FLAG_WRITABLE | MEM_FLAG_READABLE);
	Mem_AreaAddMapping(flash, 0x00100000, 1024 * 1024, MEM_FLAG_WRITABLE | MEM_FLAG_READABLE);

	return 0;
}

#define DEFAULTCONFIG \
"[global]\n" \
"start_address: 0\n"\
"\n" \
"[iram]\n" \
"size: 32k\n" \
"\n" \
"[loader]\n" \
"load_address: 0x0\n"\
"\n" \
"[iflash]\n"\
"size: 256k\n"\
"\n"

Board board_sam7a3_ek = {
	.name = "AT91SAM7A3_EK",
	.description = "Atmel SAM7A3 Evaluation Kit",
	.createBoard = board_sam7a3_ek_create,
	.runBoard = board_sam7a3_ek_run,
	.defaultconfig = DEFAULTCONFIG
};

#ifdef _SHARED_
void
_init()
{
	fprintf(stderr, "Loading SAM7A3 Board module\n");
	Board_Register(&board_sam7a3_ek);
}
#endif
