/*
 **************************************************************************************************
 *
 * Interface between Serial chip emulator device simulator backends 
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
#include "configfile.h"

#if 0
#define dbgprintf(...) { fprintf(stderr,__VA_ARGS__); }
#else
#define dbgprintf(...)
#endif

typedef struct SerialModule_ListEntry {
	char *type;
	SerialDevice_Constructor *constructor;
	struct SerialModule_ListEntry *next;
} SerialModule_ListEntry;

static SerialModule_ListEntry *modListHead = NULL;

static void
update_timing(UartPort * port)
{
	uint32_t halftxbits;
	uint32_t halfrxbits;
	halftxbits = (port->tx_csize << 1) + 2 + port->halfstopbits;
	halfrxbits = (port->rx_csize << 1) + 2 + port->halfstopbits;
	if(port->tx_baudrate) {
		port->nsPerTxChar = (UINT64_C(1000000000) >> 1) * halftxbits / port->tx_baudrate;
	}
	if(port->rx_baudrate) {
		port->nsPerRxChar = (UINT64_C(1000000000) >> 1) * halfrxbits / port->rx_baudrate;
	}
	//fprintf(stderr,"NS per char is %d, baudrate %u\n",port->nsPerRxChar,port->tx_baudrate);
}

int
SerialDevice_Cmd(UartPort * port, UartCmd * cmd)
{
	/* Track information required for timing */
	switch (cmd->opcode) {
	    case UART_OPC_SET_BAUDRATE:
		    port->rx_baudrate = port->tx_baudrate = cmd->arg;
		    update_timing(port);
		    break;

	    case UART_OPC_SET_CSIZE:
		    port->tx_csize = cmd->arg;
		    port->rx_csize = cmd->arg;
		    port->tx_csize_mask = (0xffffU >> (16 - port->tx_csize));
		    port->rx_csize_mask = (0xffffU >> (16 - port->rx_csize));
		    break;
	}
	/* Forward information to the serial backend */
	if (port->serial_device && port->serial_device->uart_cmd) {
		return port->serial_device->uart_cmd(port->serial_device, cmd);
	} else {
		return -1;
	}
}

void
SerialDevice_DoTransmit(void *eventData)
{
	UartPort *uart = eventData;
	SerialDevice *serdev = uart->serial_device;
	bool result;
	UartChar c;
	if (!uart->tx_enabled) {
		return;
	}
	result = uart->txFetchChar(uart->owner, &c);
	if (result == true) {
		c = c & uart->tx_csize_mask;
		/* Must be delayed by the output device by one byte */
		serdev->write(serdev, &c, 1);
		CycleTimer_Mod(&uart->txTimer, NanosecondsToCycles(uart->nsPerTxChar));
	} else {
        if (uart->tx_enabled) {
		    fprintf(stderr, "fetch char failed\n");
        }
	}
}

/**
 ******************************************************************
 * The frontend requests to Transmit chars. This creates a
 * timer which will write the data to the backend at a rate
 * determined by the frontends baud rate. 
 ******************************************************************
 */
void
SerialDevice_StartTx(UartPort * uart)
{
	uart->tx_enabled = true;
	if (CycleTimer_IsActive(&uart->txTimer)) {
		/* Already running */
		return;
	}
	/* Start a transmission ASAP. Therefore the timeout is 0 */
	CycleTimer_Mod(&uart->txTimer, 0);
}


/**
 **********************************************************************
 **********************************************************************
 */
void
SerialBackend_StartRx(SerialDevice *serdev)
{
	if (CycleTimer_IsActive(&serdev->rxTimer)) {
		/* Already running */
		return;
	}
	/* Start a transmission ASAP. Therefore the timeout is 0 */
	CycleTimer_Mod(&serdev->rxTimer, 0);
	serdev->rx_enabled = true;
}

void
SerialBackend_StopRx(SerialDevice *serdev)
{
    serdev->rx_enabled = false;
}

void
SerialBackend_DoReceive(void *eventData) 
{
	SerialDevice *serdev = eventData;
	UartPort *uart = serdev->uart;	
	UartChar c;
	int result;
	if(!uart) {
		return;
	}
	result = serdev->read(serdev->uart->serial_device, &c, 1);
	if (result == 1) {
		c = c & uart->rx_csize_mask;
		if(uart->rx_enabled &&  uart->rxEventProc) {
			uart->rxEventProc(uart->owner,c);
		}
		if(serdev->rx_enabled) {
			CycleTimer_Mod(&serdev->rxTimer, NanosecondsToCycles(uart->nsPerRxChar));
		}
	}
}

void
SerialBackend_PushChar(SerialDevice *serdev,UartChar c)
{
	//if(backend_disabled) 
	// print  backend is bad 
#if 0
	serdev->rxbuf[RXBUF_WP(serdev)] = c;
	serdev->rxbuf_wp++;
	if(RXBUF_ROOM(serdev) == 0) {
		// backend rx stop
	}
#endif
}
/*
 * --------------------------------------------------------
 * Register new Serial Device emulators
 * --------------------------------------------------------
 */ 
void
SerialModule_Register(const char *modname, SerialDevice_Constructor * newSerdev)
{
	SerialModule_ListEntry *le = sg_new(SerialModule_ListEntry);
	le->type = sg_strdup(modname);
	le->next = modListHead;
	le->constructor = newSerdev;
	modListHead = le;
}

UartPort *
Uart_New(const char *uart_name, UartRxEventProc * rxEventProc, UartFetchTxCharProc * txFetchProc,
	 UartStatChgProc * statproc, void *owner)
{
	const char *filename = Config_ReadVar(uart_name, "file");
	const char *type = Config_ReadVar(uart_name, "type");
	SerialDevice *serdev;
	UartPort *port = sg_new(UartPort);
	port->owner = owner;
	port->rxEventProc = rxEventProc;
	port->txFetchChar = txFetchProc;
	port->statProc = statproc;
	port->rx_baudrate = port->tx_baudrate = 115200;
	port->tx_csize = 8;
	port->rx_csize = 8;
	port->tx_csize_mask = (0xffffU >> (16 - port->tx_csize));
	port->rx_csize_mask = (0xffffU >> (16 - port->rx_csize));
	port->halfstopbits = 2;
	update_timing(port);
	CycleTimer_Init(&port->txTimer, SerialDevice_DoTransmit, port);
	/* Compatibility to old config files */
	if (!type) {
		if (filename) {
			type = "file";
		} else {
			type = "null";
		}
	}
	if (type) {
		SerialModule_ListEntry *le;
		for (le = modListHead; le; le = le->next) {
			if (!strcmp(le->type, type)) {
				serdev = port->serial_device = le->constructor(uart_name);
				break;
			}
		}
		if (!le) {
			fprintf(stderr, "No serial emulator of type \"%s\" found\n", type);
			exit(1);
		}
	}
	CycleTimer_Init(&serdev->rxTimer, SerialBackend_DoReceive, serdev);
	port->serial_device->uart = port;
	return port;
}
