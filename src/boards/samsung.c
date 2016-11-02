/*
 * ------------------------------------------------------------------------------
 *  Compose a SAMSUNG S3C2410 based board
 *
 * (C) 2007 Jochen Karrer
 *   Author: Jochen Karrer
 *
 *  This program is free software; you can distribute it and/or modify it
 *  under the terms of the GNU General Public License (Version 2) as
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope it will be useful, but WITHOUT
 *  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 *  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 *  for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  59 Temple Place - Suite 330, Boston MA 02111-1307, USA.
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

#include "dram.h"
#include "sram.h"
#include "signode.h"
#include "mmu_arm9.h"
#include "fio.h"
#include "bus.h"
#include "amdflash.h"
#include "nand.h"
#include "configfile.h"
#include "phy.h"
#include "loader.h"
#include "boards.h"
#include "lxt971a.h"
#include "i2c_serdes.h"
#include "m24cxx.h"
#include "ds1337.h"
#include "usb_ohci.h"
#include "clock.h"

static int
board_samsung_create()
{
	ArmCoprocessor *copro;
	BusDevice *dev;
//      BusDevice *mc;
	BusDevice *dram0, *dram1;
	NandFlash *nand_dev;

	Bus_Init(MMU_InvalidateTlb, 4 * 1024);
	ARM9_New();
	copro = MMU9_Create("mmu", en_LITTLE_ENDIAN, MMU_ARM920T);
	ARM9_RegisterCoprocessor(copro, 15);
	dram0 = dev = DRam_New("dram0");
	if (dev) {
		Mem_AreaAddMapping(dev, 0x30000000, 0x08000000,
				   MEM_FLAG_WRITABLE | MEM_FLAG_READABLE);
	} else {
		fprintf(stderr, "DRAM bank 0 is missing\n");
		exit(1);
	}
	dram1 = dev = DRam_New("dram1");
	if (dev) {
		Mem_AreaAddMapping(dev, 0x38000000, 0x08000000,
				   MEM_FLAG_WRITABLE | MEM_FLAG_READABLE);
	}
	dev = SRam_New("bootsram");
	nand_dev = NandFlash_New("nand0");
	// Connect the Boot sram to the memory controller

	return 0;
}

static void
board_samsung_run(Board * bd)
{
	ARM9_Run();
}

#define DEFAULTCONFIG \
"[global]\n" \
"start_address: 0x00000000\n"\
"cpu_clock: 266000000\n"\
"\n"\
"[nand0]\n" \
"type: K9F1208\n"\
"\n" \
"[bootsram]\n" \
"size: 4096\n" \
"\n" \
"[dram0]\n"\
"size: 128M\n"\
"\n"\

static Board board_samsung = {
	.name = "SAMSUNG",
	.description = "SAMSUNG S3C2410 based Board",
	.createBoard = board_samsung_create,
	.runBoard = board_samsung_run,
	.defaultconfig = DEFAULTCONFIG
};

__CONSTRUCTOR__ void
board_samsung_register()
{
	fprintf(stderr, "Loading SAMSUNG Board module\n");
	Board_Register(&board_samsung);
}
