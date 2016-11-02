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
#include "cycletimer.h"
#include "compiler_extensions.h"

#if 0
#define dbgprintf(x...) { fprintf(stderr,x); }
#else
#define dbgprintf(x...)
#endif

typedef struct FileUart {
	SerialDevice serdev;
	int fd;
	FILE *logfile;
        FIO_FileHandler input_fh;
        FIO_FileHandler output_fh;
        int ifh_is_active;
        int ofh_is_active;
	uint32_t baudrate;
	struct termios termios;

	int tx_enabled;
	CycleTimer rxBaudTimer;
        CycleTimer txBaudTimer;
	uint32_t usecs_per_char;
        //int txchar_present;
} FileUart;

typedef struct NullUart {
	SerialDevice serdev;
	int tx_enabled;
} NullUart;

/*
 *****************************************************************
 * File descriptor based serial emulator backend 
 *****************************************************************
 */

static void
file_input(void *cd,int mask) {
	SerialDevice *serdev = (SerialDevice*) cd;
	Uart_RxEvent(serdev);
	return;
}


static void 
FileUart_TxChar(void *cd) {
	FileUart *fuart = cd; 
	Uart_TxEvent(&fuart->serdev);
	if(fuart->tx_enabled) {
                CycleTimer_Mod(&fuart->txBaudTimer,MicrosecondsToCycles(fuart->usecs_per_char));
        }
	return;
}

static void
file_enable_rx(SerialDevice *serial_device) {
	FileUart *fuart = serial_device->owner;
        if((fuart->fd >= 0) && !(fuart->ifh_is_active)) {
                FIO_AddFileHandler(&fuart->input_fh,fuart->fd,FIO_READABLE,file_input,serial_device);
        	fuart->ifh_is_active=1;
        }
}

static void
file_disable_rx(SerialDevice *serial_device) {
	FileUart *fuart = serial_device->owner;
        if(fuart->ifh_is_active) {
                FIO_RemoveFileHandler(&fuart->input_fh);
        	fuart->ifh_is_active=0;
        }
}

static void
file_enable_tx(SerialDevice *serial_device) {
	FileUart *fuart = serial_device->owner;
	fuart->tx_enabled = 1;
	if(!CycleTimer_IsActive(&fuart->txBaudTimer)) {
                CycleTimer_Mod(&fuart->txBaudTimer,MicrosecondsToCycles(fuart->usecs_per_char));
        }
}

static void
file_disable_tx(SerialDevice *serial_device) {
	FileUart *fuart = serial_device->owner;
	fuart->tx_enabled = 0;
}

static void
file_close(SerialDevice *serial_device) {
	FileUart *fuart = serial_device->owner;	
	file_disable_tx(serial_device);
	file_disable_rx(serial_device);
	close(fuart->fd);
	fuart->fd=-1;
}

static int 
file_uart_read(SerialDevice *serial_device,UartChar *buf,int maxlen) 
{
	FileUart *fuart = serial_device->owner;	
	int count;
	if(fuart->fd < 0) {
		return 0;
	}
	if(sizeof(UartChar) != 1) {
		int i;
		uint8_t *data = alloca(maxlen);
		count = read(fuart->fd,data,maxlen);
		for(i = 0; i < count; i++) {
			buf[i] = data[i];	
		}
	} else {
		count = read(fuart->fd,buf,maxlen);
	}
	if(count > 0) {
		return count;
	}
	if((count <= 0) && (errno != EAGAIN)) {
		fprintf(stderr,"Read error\n");
		file_close(serial_device);
		return -1;
	}
	return 0;
}

/**
 ******************************************************************************
 * Write to file
 ******************************************************************************
 */
static int 
file_uart_write(SerialDevice *serial_device,const UartChar *buf,int len) 
{
	FileUart *fuart = serial_device->owner;	
	int count;
	if(fuart->fd<0) {
		return 0;
	}
	if(sizeof(UartChar) == 1) {
		count = write(fuart->fd,buf,1);
	} else {
		uint8_t *data = alloca(len);
		int i;
		for(i = 0; i < len; i++) {
			data[i] = buf[i];	
		}
		count = write(fuart->fd,data,1);
	}
	if(count > 0) {
		if(fuart->logfile) {
			if(fwrite(buf,sizeof(UartChar),count,fuart->logfile)!=count) {
				fprintf(stderr,"Writing to logfile failed\n");
			}
		}
		return count;
	}
	if((count <= 0) && (errno != EAGAIN)) {
		fprintf(stderr,"Write error\n");
		file_close(serial_device);
		return -1;
	}
	return 0;
}

static void 
file_uart_flush_termios(FileUart *fuart) 
{
        /* Shit: TCSADRAIN would be better but tcsettattr seems to block */
        if(tcsetattr(fuart->fd,TCSANOW,&fuart->termios)<0) {
                //perror("Can not  set terminal attributes");
                return;
        }

}

static void
file_uart_set_baudrate(FileUart *fuart,int rx_baudrate) 
{
	speed_t rate;
	fuart->baudrate = rx_baudrate;
	if((rx_baudrate > 440000) && (rx_baudrate < 480000)) {
                rate=B460800;
        } else if((rx_baudrate > 220000) && (rx_baudrate < 240000)) {
                rate=B230400;
        } else if((rx_baudrate > 110000) && (rx_baudrate < 120000)) {
                rate=B115200;
        } else if((rx_baudrate > 55000) && (rx_baudrate < 60000)) {
                rate=B57600;
        } else if((rx_baudrate > 36571 ) && (rx_baudrate < 40320)) {
                rate=B38400;
        } else if((rx_baudrate > 18285 ) && (rx_baudrate < 20160)) {
                rate=B19200;
        } else if((rx_baudrate > 9142 ) && (rx_baudrate < 10080)) {
                rate=B9600;
        } else if((rx_baudrate > 4571 ) && (rx_baudrate < 5040)) {
                rate=B4800;
        } else if((rx_baudrate > 2285 ) && (rx_baudrate < 2520)) {
                rate=B2400;
        } else if((rx_baudrate > 1890 ) && (rx_baudrate < 1714)) {
                rate=B1800;
        } else if((rx_baudrate > 1142 ) && (rx_baudrate < 1260)) {
                rate=B1200;
        } else if((rx_baudrate > 570 ) && (rx_baudrate < 630)) {
                rate=B600;
        } else if((rx_baudrate > 285 ) && (rx_baudrate < 315)) {
                rate=B300;
        } else if((rx_baudrate > 190 ) && (rx_baudrate < 210)) {
                rate=B200;
        } else if((rx_baudrate > 142 ) && (rx_baudrate < 158)) {
                rate=B150;
        } else if((rx_baudrate > 128 ) && (rx_baudrate < 141)) {
                rate=B134;
        } else if((rx_baudrate > 105 ) && (rx_baudrate < 116)) {
                rate=B110;
        } else if((rx_baudrate > 71 ) && (rx_baudrate < 79)) {
                rate=B75;
        } else if((rx_baudrate > 47 ) && (rx_baudrate < 53)) {
                rate=B50;
        } else {
                fprintf(stderr,"Serial emulator: Can not handle Baudrate %d\n",rx_baudrate);
                rate=B0;
		rx_baudrate = 115200;
        }
	if(cfsetispeed ( &fuart->termios, rate)<0) {
                fprintf(stderr,"Can not change Baudrate\n");
        } else {
		dbgprintf("Changed speed to %d, rate %d\n",rx_baudrate,rate);
	}
        if(cfsetospeed ( &fuart->termios, rate)<0) {
                fprintf(stderr,"Can not change Baudrate\n");
        }
	file_uart_flush_termios(fuart);
	fuart->baudrate = rx_baudrate;
	fuart->usecs_per_char = 1000000 * 10 / fuart->baudrate;
}

static void
file_uart_set_csize(FileUart *fuart,int csize) 
{
	tcflag_t bits;
	switch(csize) {
                case 5:
                        bits=CS5; break;
                case 6:
                        bits=CS6; break;
                case  7:
                        bits=CS7; break;
                case  8:
                        bits=CS8; break;
                default:
			fprintf(stderr,"Illegal word length of %d\n",csize);
                        bits=CS8; break;
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
file_uart_cmd(SerialDevice *serial_device,UartCmd *cmd) 
{
	FileUart *fuart = serial_device->owner;	
	uint32_t arg = cmd->arg;
	unsigned int host_status;
	cmd->retval = 0;
	switch(cmd->opcode) {
		case UART_OPC_GET_DCD:
			if(isatty(fuart->fd) && (ioctl(fuart->fd,TIOCMGET,&host_status)>=0)) {
				cmd->retval = !!(host_status & TIOCM_CAR);
			} else {
				cmd->retval = 0;
			}
			break;

		case UART_OPC_GET_RI:
			if(isatty(fuart->fd) && (ioctl(fuart->fd,TIOCMGET,&host_status)>=0)) {
				cmd->retval = !!(host_status & TIOCM_RNG);
			} else {
				cmd->retval = 0;
			}
			break;

		case UART_OPC_GET_DSR:
			if(isatty(fuart->fd) && (ioctl(fuart->fd,TIOCMGET,&host_status)>=0)) {
				cmd->retval = !!(host_status & TIOCM_DSR);
			} else {
				cmd->retval = 0;
			}
			break;

		case UART_OPC_GET_CTS:
			if(isatty(fuart->fd) && (ioctl(fuart->fd,TIOCMGET,&host_status)>=0)) {
				cmd->retval = !!(host_status & TIOCM_CTS);
			} else {
				cmd->retval = 1;
			}
			break;

		case UART_OPC_SET_RTS:
			if(!isatty(fuart->fd)) {
				return 0;
			}
			if(arg) {
				ioctl(fuart->fd,TIOCMSET,TIOCM_RTS);
			} else {
				ioctl(fuart->fd,TIOCMBIC,TIOCM_RTS);
			}
			break;
		case UART_OPC_SET_DTR:
			if(!isatty(fuart->fd)) {
				return 0;
			}
			if(arg) {
				ioctl(fuart->fd,TIOCMSET,TIOCM_DTR);
			} else {
				ioctl(fuart->fd,TIOCMBIC,TIOCM_DTR);
			}
			break;

		case UART_OPC_SET_BAUDRATE:
			if(!isatty(fuart->fd)) {
                		return 0;
        		}
			file_uart_set_baudrate(fuart,arg);
			break;		
			
		case UART_OPC_SET_CSIZE: 
			if(!isatty(fuart->fd)) {
                		return 0;
        		}
			file_uart_set_csize(fuart,arg);	
			break;

		case UART_OPC_CRTSCTS: 
			
			if(!isatty(fuart->fd)) {
                		return 0;
        		}
			if(arg) {
        			fuart->termios.c_cflag |= CRTSCTS;
			} else {
				fuart->termios.c_cflag &= ~(CRTSCTS);
			}
			fuart->termios.c_cflag &= ~(CRTSCTS);
			file_uart_flush_termios(fuart);
			break;

		case UART_OPC_PAREN: 
			if(!isatty(fuart->fd)) {
                		return 0;
        		}
			if(arg) {
        			fuart->termios.c_cflag |= PARENB;
			} else {
				fuart->termios.c_cflag &= ~(PARENB);
			}
			file_uart_flush_termios(fuart);
			break;

		case UART_OPC_PARODD:
			if(!isatty(fuart->fd)) {
                		return 0;
        		}
			if(arg) {
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
TerminalInit(FileUart *fuart) {
	if(tcgetattr(fuart->fd,&fuart->termios)<0) {
                fprintf(stderr,"Can not  get terminal attributes\n");
                return;
        }
        if(fuart->fd > 0) {
                cfmakeraw(&fuart->termios);
        } else if(fuart->fd == 0){
		fuart->termios.c_lflag &= ~(ECHO | ICANON | ISIG | IEXTEN);
        }

        if(tcsetattr(fuart->fd,TCSAFLUSH,&fuart->termios)<0) {
                perror("can't set terminal settings");
                return;
        }
}

#define ANSI_replacementmode "\033[4l"
static void
TerminalRestore(int fd) {
        struct termios term;
	int result;
        if(tcgetattr(fd,&term)<0) {
                perror("can't restore terminal settings\n");
                return;
        }
        term.c_lflag |= (ECHO|ECHONL|ICANON|ISIG|IEXTEN);
        if(tcsetattr(fd,TCSAFLUSH,&term)<0) {
                perror("can't restore terminal settings");
                return;
        } else {
		fprintf(stderr,"Restored terminal settings\n");
	}
	fcntl(fd,F_SETFL,0);
	fprintf(stdout,"\033[4l"); /* replacement mode */
	result = write(fd,ANSI_replacementmode,strlen(ANSI_replacementmode));
}

static void
TerminalExit(void) {
        TerminalRestore(0);
}

/*
 * --------------------------------------------
 * The Template for a File serial device
 * --------------------------------------------
 */
static SerialDevice file_uart = {
	.stop_tx = file_disable_tx,
	.start_tx = file_enable_tx,
	.stop_rx = file_disable_rx,
	.start_rx = file_enable_rx,
	.uart_cmd = file_uart_cmd,
	.write = file_uart_write,
	.read = file_uart_read,
};

SerialDevice *
FileUart_New(const char *uart_name)  {
	FileUart *fiua;
	char *logfilename;
	const char *filename = Config_ReadVar(uart_name,"file");
	if(!filename) {
		fprintf(stderr,"Missing filename (file: ) for uart \"%s\"\n",uart_name);
		exit(1);
	}
	fiua = sg_new(FileUart);
	fiua->serdev = file_uart; /* Copy from template */
	fiua->serdev.owner = fiua;
	fiua->logfile = NULL;	
	logfilename = Config_ReadVar(uart_name,"logfile");
	if(logfilename) {
		fiua->logfile = fopen(logfilename,"w+");
	}
	if(!strcmp(filename,"stdin")) {
		fiua->fd = 0;
	} else {
		fiua->fd = open(filename,O_RDWR);
	}
	if(fiua->fd<0) {
		fprintf(stderr,"%s: Cannot open %s\n",uart_name,filename);
		sleep(3);
		return &fiua->serdev;
	} else {
		fcntl(fiua->fd,F_SETFL,O_NONBLOCK);
		fprintf(stderr,"Uart \"%s\" Connected to %s\n",uart_name,filename);
	}
	fiua->baudrate = 115200;
	fiua->usecs_per_char = 1000000 * 10 / fiua->baudrate;
//	CycleTimer_Init(&pua->rxBaudTimer,Ptmx_RxChar,pua);
        CycleTimer_Init(&fiua->txBaudTimer,FileUart_TxChar,fiua);

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

__CONSTRUCTOR__ static void
FileUart_Init(void)
{
        SerialModule_Register("file",FileUart_New);
        fprintf(stderr,"Registered File UART Emulator module\n");
}
