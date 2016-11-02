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
#include "fio.h"
#include "configfile.h"

#if 0
#define dbgprintf(x...) { fprintf(stderr,x); }
#else
#define dbgprintf(x...)
#endif

typedef struct SerialModule_ListEntry {
	char *type;
	SerialDevice_Constructor *constructor;
	struct SerialModule_ListEntry *next;
} SerialModule_ListEntry;

static SerialModule_ListEntry *modListHead = NULL;
/*
 * --------------------------------------------------------
 * Register new Serial Device emulators
 * --------------------------------------------------------
 */
void
SerialModule_Register(const char *modname,SerialDevice_Constructor *newSerdev) 
{	
	SerialModule_ListEntry *le = sg_new(SerialModule_ListEntry);
	le->type = sg_strdup(modname);
	le->next = modListHead;
	le->constructor = newSerdev;
	modListHead = le;	
}

UartPort *
Uart_New(const char *uart_name,UartRxEventProc *rxEventProc,UartTxEventProc *txEventProc,UartStatChgProc *statproc,void *owner) 
{
	const char *filename = Config_ReadVar(uart_name,"file");
	const char *type = Config_ReadVar(uart_name,"type");
	UartPort *port = sg_new(UartPort);
	port->owner = owner;
	port->rxEventProc = rxEventProc;
        port->txEventProc = txEventProc;
        port->statProc = statproc;
	/* Compatibility to old config files */
	if(!type) {
		if(filename) {
			type = "file";
		} else {
			type = "null";
		}
	}
	if(type) {
		SerialModule_ListEntry *le;
		for(le=modListHead;le;le=le->next) {
			if(!strcmp(le->type,type)) {
				port->serial_device = le->constructor(uart_name);
				break;
			}
		}
		if(!le) {
			fprintf(stderr,"No serial emulator of type \"%s\" found\n",type);
			exit(1);
		}
	}
	port->serial_device->uart = port;
	return port;		
}
