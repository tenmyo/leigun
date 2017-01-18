#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <avr8_cpu.h>
#include <boards.h>
#include "compiler_extensions.h"
#include "initializer.h"

#define DEFAULTCONFIG \
"[global]\n" \
"cpu_clock: 20000000\n"\
"\n"

static int
board_stk500_create()
{
	AVR8_Init("avr");
	return 0;
}

static void
board_stk500_run(Board * bd)
{
	AVR8_Run();
}

static Board board_stk500 = {
	.name = "STK500",
	.description = "STK500 AVR8 development Board",
	.createBoard = board_stk500_create,
	.runBoard = board_stk500_run,
	.defaultconfig = DEFAULTCONFIG
};

INITIALIZER(stk500_init)
{
	fprintf(stderr, "Loading STK500 Board module\n");
	Board_Register(&board_stk500);
}
