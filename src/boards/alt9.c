#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <termios.h>
#include <sys/ioctl.h>
#include "boards.h"
#include "alt9.h"
#include "cpu_mcs51.h"
#include "compiler_extensions.h"

#define DEFAULTCONFIG \
"[global]\n" \
"cpu_clock: 8000000\n"\
"\n"

static int
board_alt9_create()
{
	MCS51_Init("mcs51");
	return 0;
}

static void
board_alt9_run(Board * bd)
{
	MCS51_Run();
}

Board board_alt9 = {
	.name = "ALT9",
	.description = "Alt-9 8051 example board",
	.createBoard = board_alt9_create,
	.runBoard = board_alt9_run,
	.defaultconfig = DEFAULTCONFIG
};

__CONSTRUCTOR__ static void
alt9_init()
{
	fprintf(stderr, "Loading ALT-9 Board module\n");
	Board_Register(&board_alt9);
}
