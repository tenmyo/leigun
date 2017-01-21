/*
 **************************************************************************************************
 *
 * Interface between Serial chip emulator and a file (device file, stdout, real file...) 
 *
 * State: working, not well tested 
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
// include self header
#include "compiler_extensions.h"
#include "serial.h"

// include system header
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <termios.h>
#include <sys/ioctl.h>

// include library header

// include user header
#include "initializer.h"
#include "sgstring.h"
#include "cycletimer.h"
#include "configfile.h"
#include "sglib.h"
#include "core/asyncmanager.h"

#if 0
#define dbgprintf(...) { fprintf(stderr,__VA_ARGS__); }
#else
#define dbgprintf(...)
#endif

#define RXBUF_SIZE 32

#define RXBUF_RP(pua) ((pua)->rxbuf_rp % RXBUF_SIZE)
#define RXBUF_WP(pua) ((pua)->rxbuf_wp % RXBUF_SIZE)
#define RXBUF_LVL(pua) ((pua)->rxbuf_wp - (pua)->rxbuf_rp)
#define RXBUF_ROOM(pua) (RXBUF_SIZE - RXBUF_LVL(pua)) 

typedef struct FileUart {
	SerialDevice serdev;
	Utf8ToUnicodeCtxt utf8ToUnicodeCtxt;
	int infd;
	int outfd;
	FILE *logfile;
  PollHandle_t *input_fh;
	int ifh_is_active;
	uint32_t baudrate;
	struct termios termios;
	int tx_enabled;
	CycleTimer rxBaudTimer;
	uint32_t usecs_per_char;
	uint32_t force_utf8;
	uint32_t charsize;
	uint8_t utf8_charbuf[4];
	UartChar rxChar;
	bool rxCharPresent;
	bool rxEnabled;
	uint32_t rxbuf_wp;
	uint32_t rxbuf_rp;
	uint8_t rxbuf[RXBUF_SIZE];
} FileUart;

typedef struct NullUart {
	SerialDevice serdev;
	int tx_enabled;
} NullUart;

static void
file_disable_rx(SerialDevice * serial_device)
{
	FileUart *fuart = serial_device->owner;
	if (fuart->ifh_is_active) {
		AsyncManager_PollStop(fuart->input_fh);
		fuart->ifh_is_active = 0;
	}
	fuart->rxEnabled = false;
}

static void
file_close(SerialDevice * serial_device)
{
	FileUart *fuart = serial_device->owner;
	file_disable_rx(serial_device);
	AsyncManager_Close(AsyncManager_Poll2Handle(fuart->input_fh), NULL, NULL);
	close(fuart->infd);
	fuart->infd = -1;
}

/*
 *****************************************************************
 * File descriptor based serial emulator backend 
 *****************************************************************
 */

static void
file_input(PollHandle_t *handle, int status, int events, void *clientdata)
{
	SerialDevice *serdev = clientdata;
	FileUart *fuart = serdev->owner;
    int fifo_room = RXBUF_ROOM(fuart);
	int result;
	if (fifo_room == 0) {
		/* Stop input event handler if fifo is full */
		if (fuart->ifh_is_active) {
			AsyncManager_PollStop(fuart->input_fh);
			fuart->ifh_is_active = 0;
		} else {
			fprintf(stderr, "Nothing removed\n");
		}
		dbgprintf("Fifo full\n");
		return;
	}
    while(fifo_room > 0) {
	    int max_bytes = RXBUF_SIZE - RXBUF_WP(fuart);
        if (max_bytes > fifo_room) {
            max_bytes = fifo_room;
        }
        result = read(fuart->infd, fuart->rxbuf + RXBUF_WP(fuart), max_bytes);
        //fprintf(stderr, "Read %u: %02x\n", result, *(fuart->rxbuf + RXBUF_WP(fuart)));
        //fprintf(stderr, "Read %d bytes of max %d\n", result, max_bytes);
        if ((result < 0) && (errno != EAGAIN)) {
            fprintf(stderr, "Read error\n");
            file_close(serdev);
            return;
        } else if (result > 0) {
            fuart->rxbuf_wp += result;
            fifo_room -= result;
            if (!CycleTimer_IsActive(&fuart->rxBaudTimer)) {
                /* First char is immediate, delay is after the last char ! */
                CycleTimer_Mod(&fuart->rxBaudTimer, 0);
            }
        } else {
            break;
        }
    }
	return;
}

static void
file_enable_rx(SerialDevice * serial_device)
{
	FileUart *fuart = serial_device->owner;
#if 0
    if (RXBUF_LVL(fuart) && !fuart->rxEnabled) {    
		CycleTimer_Mod(&fuart->rxBaudTimer, MicrosecondsToCycles(fuart->usecs_per_char));
    }
#endif
	fuart->rxEnabled = true;
	if ((fuart->infd >= 0) && !(fuart->ifh_is_active)) {
		AsyncManager_PollStart(fuart->input_fh, ASYNCMANAGER_EVENT_READABLE, &file_input, serial_device);
		fuart->ifh_is_active = 1;
    }
}

static void
Fuart_TriggerRxEvent(void *eventData)
{
	FileUart *fuart = eventData;
	if (RXBUF_LVL(fuart) > 0) {
		/* Do restart before Triggering an Rx Event */
        //fprintf(stderr, "Mod lvl %u %lu\n", RXBUF_LVL(fuart), fuart->usecs_per_char);
		CycleTimer_Mod(&fuart->rxBaudTimer, MicrosecondsToCycles(fuart->usecs_per_char));
	}
	while (RXBUF_LVL(fuart) > 0) {
		if (fuart->force_utf8 || (fuart->charsize > 8)) {
			uint32_t rxWord;
			bool result;
			result = utf8_to_unicode(&fuart->utf8ToUnicodeCtxt, &rxWord,
						 fuart->rxbuf[RXBUF_RP(fuart)]);
			fuart->rxbuf_rp++;
			if (result) {
				fuart->rxChar = rxWord;
				fuart->rxCharPresent = true;
				// BIG SHIT, should be repaired
				if (fuart->rxEnabled) {
					Uart_RxEvent(&fuart->serdev);
				}
				break;
			}
		} else {
		//	if (fuart->rxEnabled) {
                fuart->rxChar = fuart->rxbuf[RXBUF_RP(fuart)];
                fuart->rxbuf_rp++;
                fuart->rxCharPresent = true;
//                fprintf(stderr, "Event lvl %u\n", RXBUF_LVL(fuart));
				Uart_RxEvent(&fuart->serdev);
         //   }
			break;
		}
	}
	if ((!fuart->ifh_is_active) && fuart->rxEnabled) {
		dbgprintf("Reactivate inputfh\n");
		AsyncManager_PollStart(fuart->input_fh, ASYNCMANAGER_EVENT_READABLE, &file_input, &fuart->serdev);
		fuart->ifh_is_active = true;
	}
}

static int
file_uart_read(SerialDevice * serial_device, UartChar * buf, int maxlen)
{
	FileUart *fuart = serial_device->owner;
	if (maxlen > 0) {
		if (fuart->rxCharPresent) {
			buf[0] = fuart->rxChar;
            //fprintf(stderr,"Fetch %02x\n", buf[0]);
			fuart->rxCharPresent = false;
			return 1;
		}
	}
	return 0;
}

/**
 ******************************************************************************
 * Write to file
 ******************************************************************************
 */
static int
file_uart_write(SerialDevice * serial_device, const UartChar * buf, int len)
{
	FileUart *fuart = serial_device->owner;
	int char_count = 0;
	uint8_t *data = alloca(len << 2);
	int i;
	if (fuart->infd < 0) {
		return 0;
	}
	if (fuart->force_utf8 || (fuart->charsize > 8)) {
		int cnt;
		for (cnt = 0, i = 0; i < len; i++) {
			cnt = unicode_to_utf8(buf[i],fuart->utf8_charbuf);
			cnt = write(fuart->outfd, fuart->utf8_charbuf, cnt);
#warning needs a buffer or blocking io
			if(cnt > 0) {
				char_count++;
			} else {
				return char_count;
			}
			if ((cnt <= 0) && (errno != EAGAIN)) {
				fprintf(stderr, "Write error\n");
				file_close(serial_device);
				return -1;
			}
		}
	} else {
		for (i = 0; i < len; i++) {
			data[i] = buf[i];
		}
		char_count = write(fuart->outfd, data, 1);
		if ((char_count <= 0) && (errno != EAGAIN)) {
			fprintf(stderr, "Write error\n");
			file_close(serial_device);
			return -1;
		}
	}
#if 0
	if (count > 0) {
		if (fuart->logfile) {
			if (fwrite(buf, sizeof(UartChar), count, fuart->logfile) != count) {
				fprintf(stderr, "Writing to logfile failed\n");
			}
		}
		return count;
	}
	if ((count <= 0) && (errno != EAGAIN)) {
		fprintf(stderr, "Write error\n");
		file_close(serial_device);
		return -1;
	}
#endif
	return 0;
}

static void
file_uart_flush_termios(FileUart * fuart)
{
	/* Shit: TCSADRAIN would be better but tcsettattr seems to block */
	if (tcsetattr(fuart->infd, TCSANOW, &fuart->termios) < 0) {
		//perror("Can not  set terminal attributes");
		return;
	}

}

static void
file_uart_set_baudrate(FileUart * fuart, int rx_baudrate)
{
	speed_t rate;
	fuart->baudrate = rx_baudrate;
	if ((rx_baudrate > 440000) && (rx_baudrate < 480000)) {
		rate = B460800;
	} else if ((rx_baudrate > 220000) && (rx_baudrate < 240000)) {
		rate = B230400;
	} else if ((rx_baudrate > 110000) && (rx_baudrate < 120000)) {
		rate = B115200;
	} else if ((rx_baudrate > 55000) && (rx_baudrate < 60000)) {
		rate = B57600;
	} else if ((rx_baudrate > 36571) && (rx_baudrate < 40320)) {
		rate = B38400;
	} else if ((rx_baudrate > 18285) && (rx_baudrate < 20160)) {
		rate = B19200;
	} else if ((rx_baudrate > 9142) && (rx_baudrate < 10080)) {
		rate = B9600;
	} else if ((rx_baudrate > 4571) && (rx_baudrate < 5040)) {
		rate = B4800;
	} else if ((rx_baudrate > 2285) && (rx_baudrate < 2520)) {
		rate = B2400;
	} else if ((rx_baudrate > 1714) && (rx_baudrate < 1890)) {
		rate = B1800;
	} else if ((rx_baudrate > 1142) && (rx_baudrate < 1260)) {
		rate = B1200;
	} else if ((rx_baudrate > 570) && (rx_baudrate < 630)) {
		rate = B600;
	} else if ((rx_baudrate > 285) && (rx_baudrate < 315)) {
		rate = B300;
	} else if ((rx_baudrate > 190) && (rx_baudrate < 210)) {
		rate = B200;
	} else if ((rx_baudrate > 142) && (rx_baudrate < 158)) {
		rate = B150;
	} else if ((rx_baudrate > 128) && (rx_baudrate < 141)) {
		rate = B134;
	} else if ((rx_baudrate > 105) && (rx_baudrate < 116)) {
		rate = B110;
	} else if ((rx_baudrate > 71) && (rx_baudrate < 79)) {
		rate = B75;
	} else if ((rx_baudrate > 47) && (rx_baudrate < 53)) {
		rate = B50;
	} else {
		fprintf(stderr, "Serial emulator: Can not handle Baudrate %d\n", rx_baudrate);
		rate = B0;
		rx_baudrate = 115200;
	}
	if (cfsetispeed(&fuart->termios, rate) < 0) {
		fprintf(stderr, "Can not change Baudrate\n");
	} else {
		dbgprintf("Changed speed to %d, rate %d\n", rx_baudrate, rate);
	}
	if (cfsetospeed(&fuart->termios, rate) < 0) {
		fprintf(stderr, "Can not change Baudrate\n");
	}
	file_uart_flush_termios(fuart);
	fuart->baudrate = rx_baudrate;
	fuart->usecs_per_char = 1000000 * 10 / fuart->baudrate;
	dbgprintf("USECS per CHAR %u\n", fuart->usecs_per_char);
}

static void
file_uart_set_csize(FileUart * fuart, int csize)
{
	tcflag_t bits;
	fuart->charsize = csize;
	switch (csize) {
	    case 5:
		    bits = CS5;
		    break;
	    case 6:
		    bits = CS6;
		    break;
	    case 7:
		    bits = CS7;
		    break;
	    case 8:
		    bits = CS8;
		    break;
	    case 9:
		    bits = CS8;	/* We are sending UTF-8 */
		    break;
	    default:
		    fprintf(stderr, "Unexpected word length of %d\n", csize);
		    bits = CS8;
		    break;
	}
	fuart->termios.c_cflag &= ~(CSIZE);
	fuart->termios.c_cflag |= bits;
	file_uart_flush_termios(fuart);
}

/**
 **************************************************************************
 * \fn static int file_uart_cmd(SerialDevice *serial_device,UartCmd *cmd) 
 **************************************************************************
 */
static int
file_uart_cmd(SerialDevice * serial_device, UartCmd * cmd)
{
	FileUart *fuart = serial_device->owner;
	uint32_t arg = cmd->arg;
	unsigned int host_status;
	cmd->retval = 0;
	switch (cmd->opcode) {
	    case UART_OPC_GET_DCD:
		    if (isatty(fuart->infd) && (ioctl(fuart->infd, TIOCMGET, &host_status) >= 0)) {
			    cmd->retval = ! !(host_status & TIOCM_CAR);
		    } else {
			    cmd->retval = 0;
		    }
		    break;

	    case UART_OPC_GET_RI:
		    if (isatty(fuart->infd) && (ioctl(fuart->infd, TIOCMGET, &host_status) >= 0)) {
			    cmd->retval = ! !(host_status & TIOCM_RNG);
		    } else {
			    cmd->retval = 0;
		    }
		    break;

	    case UART_OPC_GET_DSR:
		    if (isatty(fuart->infd) && (ioctl(fuart->infd, TIOCMGET, &host_status) >= 0)) {
			    cmd->retval = ! !(host_status & TIOCM_DSR);
		    } else {
			    cmd->retval = 0;
		    }
		    break;

	    case UART_OPC_GET_CTS:
		    if (isatty(fuart->infd) && (ioctl(fuart->infd, TIOCMGET, &host_status) >= 0)) {
			    cmd->retval = ! !(host_status & TIOCM_CTS);
		    } else {
			    cmd->retval = 1;
		    }
		    break;

	    case UART_OPC_SET_RTS:
		    if (!isatty(fuart->infd)) {
			    return 0;
		    }
		    if (arg) {
			    ioctl(fuart->infd, TIOCMBIS, TIOCM_RTS);
		    } else {
			    ioctl(fuart->infd, TIOCMBIC, TIOCM_RTS);
		    }
		    break;
	    case UART_OPC_SET_DTR:
		    if (!isatty(fuart->infd)) {
			    return 0;
		    }
		    if (arg) {
			    ioctl(fuart->infd, TIOCMBIS, TIOCM_DTR);
		    } else {
			    ioctl(fuart->infd, TIOCMBIC, TIOCM_DTR);
		    }
		    break;

	    case UART_OPC_SET_BAUDRATE:
		    if (!isatty(fuart->infd)) {
			    return 0;
		    }
		    file_uart_set_baudrate(fuart, arg);
		    break;

	    case UART_OPC_SET_CSIZE:
		    if (!isatty(fuart->infd)) {
			    return 0;
		    }
		    file_uart_set_csize(fuart, arg);
		    break;

	    case UART_OPC_CRTSCTS:

		    if (!isatty(fuart->infd)) {
			    return 0;
		    }
		    if (arg) {
			    fuart->termios.c_cflag |= CRTSCTS;
		    } else {
			    fuart->termios.c_cflag &= ~(CRTSCTS);
		    }
		    fuart->termios.c_cflag &= ~(CRTSCTS);
		    file_uart_flush_termios(fuart);
		    break;

	    case UART_OPC_PAREN:
		    if (!isatty(fuart->infd)) {
			    return 0;
		    }
		    if (arg) {
			    fuart->termios.c_cflag |= PARENB;
		    } else {
			    fuart->termios.c_cflag &= ~(PARENB);
		    }
		    file_uart_flush_termios(fuart);
		    break;

	    case UART_OPC_PARODD:
		    if (!isatty(fuart->infd)) {
			    return 0;
		    }
		    if (arg) {
			    fuart->termios.c_cflag |= PARODD;
		    } else {
			    fuart->termios.c_cflag &= ~(PARODD);
		    }
		    file_uart_flush_termios(fuart);
		    break;

	    default:
		    break;
	}
	return 0;
}

static void
TerminalInit(FileUart * fuart)
{
	if (tcgetattr(fuart->infd, &fuart->termios) < 0) {
		fprintf(stderr, "Can not  get terminal attributes\n");
		return;
	}
	if (fuart->infd > 0) {
		cfmakeraw(&fuart->termios);
	} else if (fuart->infd == 0) {
		fuart->termios.c_lflag &= ~(ECHO | ICANON | ISIG | IEXTEN);
	}

	if (tcsetattr(fuart->infd, TCSAFLUSH, &fuart->termios) < 0) {
		perror("can't set terminal settings");
		return;
	}
}

#define ANSI_replacementmode "\033[4l"
static void
TerminalRestore(int fd)
{
	struct termios term;
	int result;
	if (tcgetattr(fd, &term) < 0) {
		perror("can't restore terminal settings\n");
		return;
	}
	term.c_lflag |= (ECHO | ECHONL | ICANON | ISIG | IEXTEN);
	if (tcsetattr(fd, TCSAFLUSH, &term) < 0) {
		perror("can't restore terminal settings");
		return;
	} else {
		fprintf(stderr, "Restored terminal settings\n");
	}
	fcntl(fd, F_SETFL, 0);
	fprintf(stdout, "\033[4l");	/* replacement mode */
	result = write(fd, ANSI_replacementmode, strlen(ANSI_replacementmode));
}

static void
TerminalExit(void)
{
	TerminalRestore(0);
}

/*
 * --------------------------------------------
 * The Template for a File serial device
 * --------------------------------------------
 */
static SerialDevice file_uart = {
	.stop_rx = file_disable_rx,
	.start_rx = file_enable_rx,
	.uart_cmd = file_uart_cmd,
	.write = file_uart_write,
	.read = file_uart_read,
};

SerialDevice *
FileUart_New(const char *uart_name)
{
	FileUart *fiua;
	char *logfilename;
	const char *filename = Config_ReadVar(uart_name, "file");
	if (!filename) {
		fprintf(stderr, "Missing filename (file: ) for uart \"%s\"\n", uart_name);
		exit(1);
	}
	fiua = sg_new(FileUart);
	fiua->serdev = file_uart;	/* Copy from template */
	fiua->serdev.owner = fiua;
	fiua->logfile = NULL;
	fiua->force_utf8 = 0;
	Config_ReadUInt32(&fiua->force_utf8, uart_name, "utf8");

	logfilename = Config_ReadVar(uart_name, "logfile");
	if (logfilename) {
		fiua->logfile = fopen(logfilename, "w+");
	}
	if (!strcmp(filename, "stdin")) {
		fiua->infd = 0;
		fiua->outfd = 0;
	} else if (!strcmp(filename, "stdio")) {
		fiua->infd = 0;
		fiua->outfd = 1;
	} else {
		fiua->outfd = fiua->infd = open(filename, O_RDWR);
	}
	if (fiua->infd < 0) {
		fprintf(stderr, "%s: Cannot open %s\n", uart_name, filename);
		sleep(3);
		return &fiua->serdev;
	} else {
		fiua->input_fh = AsyncManager_PollInit(fiua->infd);
		fprintf(stderr, "Uart \"%s\" Connected to %s\n", uart_name, filename);
	}
	fiua->baudrate = 115200;
	fiua->usecs_per_char = 1000000 * 10 / fiua->baudrate;
	CycleTimer_Init(&fiua->rxBaudTimer, Fuart_TriggerRxEvent, fiua);
	TerminalInit(fiua);
	atexit(TerminalExit);
	return &fiua->serdev;
}

/*
 *******************************************************************************
 * void FileUart_Init(void)
 *      It registers a SerialDevice emulator module of type "file"
 *******************************************************************************
 */
INITIALIZER(FileUart_Init)
{
	SerialModule_Register("file", FileUart_New);
	fprintf(stderr, "Registered File UART Emulator module\n");
}
