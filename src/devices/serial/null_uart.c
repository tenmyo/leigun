/*
 **************************************************************************************************
 *
 * This is the null device among the serial device simulators. It eats up everything and 
 * emits nothing. 
 *
 * State: working
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

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <termios.h>
#include <sys/ioctl.h>

#include "sgstring.h"
#include "serial.h"
#include "fio.h"
#include "configfile.h"
#include "compiler_extensions.h"

#if 0
#define dbgprintf(...) { fprintf(stderr,__VA_ARGS__); }
#else
#define dbgprintf(...)
#endif

typedef struct NullUart {
	SerialDevice serdev;
	int tx_enabled;
} NullUart;

static int
null_write(SerialDevice * serial_device, const UartChar * buf, int len)
{
	return len;
}

static int
null_read(SerialDevice * serial_device, UartChar * buf, int len)
{
	return 0;
}

static SerialDevice null_uart = {
	.stop_rx = NULL,
	.start_rx = NULL,
	.write = null_write,
	.read = null_read,
};

static SerialDevice *
NullUart_New(const char *uart_name)
{
	NullUart *nua = sg_new(NullUart);
	nua->serdev = null_uart;	/* copy from template */
	nua->serdev.owner = nua;
	nua->tx_enabled = 0;
	return &nua->serdev;
}

/*
 *******************************************************************************
 * void NullUart_Init(void)
 *      It registers a SerialDevice emulator module of type "null"
 *******************************************************************************
 */

__CONSTRUCTOR__ static void
NullUart_Init(void)
{
	SerialModule_Register("null", NullUart_New);
	fprintf(stderr, "Registered Null UART Emulator module\n");
}
