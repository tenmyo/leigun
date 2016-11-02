/**
 ************************************************************************************
 *
 * Serial simulation using /dev/ptmx
 *
 * Copyright 2010 Jochen Karrer. All rights reserved.
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

#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <termios.h>
#include "serial.h"
#include "sgstring.h"
#include "cycletimer.h"
#include "fio.h"
#include "configfile.h"
#include "sglib.h"
#include "compiler_extensions.h"

#define RXBUF_SIZE 128 

#define RXBUF_RP(pua) ((pua)->rxbuf_rp % RXBUF_SIZE)
#define RXBUF_WP(pua) ((pua)->rxbuf_wp % RXBUF_SIZE)
#define RXBUF_LVL(pua) ((pua)->rxbuf_wp - (pua)->rxbuf_rp)
#define RXBUF_ROOM(pua) (RXBUF_SIZE - RXBUF_LVL(pua))

#define TXBUF_SIZE 128 
#define TXBUF_RP(pua) ((pua)->txbuf_rp % TXBUF_SIZE)
#define TXBUF_WP(pua) ((pua)->txbuf_wp % TXBUF_SIZE)
#define TXBUF_LVL(pua) ((pua)->txbuf_wp - (pua)->txbuf_rp)
#define TXBUF_ROOM(pua) (TXBUF_SIZE - TXBUF_LVL(pua))

typedef struct PtmxUart {
        SerialDevice serdev;
	Utf8ToUnicodeCtxt utf8ToUnicodeCtxt;
	int fd;
	char *linkname;
        int tx_enabled;
        int rx_enabled;

	FIO_FileHandler rfh;
        int rfh_active;
        FIO_FileHandler wfh;
        int wfh_active;

        UartChar rxChar;
        int rxchar_present;
	uint8_t charsize;
        uint8_t rxbuf[RXBUF_SIZE];
	uint32_t force_utf8;

        unsigned int rxbuf_wp;
        unsigned int rxbuf_rp;

        uint8_t txbuf[TXBUF_SIZE]; /* UTF8 format if charsize > 8 */
        unsigned int txbuf_wp;
        unsigned int txbuf_rp;

        CycleTimer rxBaudTimer;
        CycleTimer txBaudTimer;
	int txchar_present;
        uint32_t usecs_per_char;
} PtmxUart;

static void Ptmx_Reopen(PtmxUart *pua); 

/**
 ****************************************************************
 * \fn static void Ptmx_RefillRxChar(PtmxUart *pua); 
 ****************************************************************
 */

static void
Ptmx_RefillRxChar(PtmxUart *pua) {
	if(pua->rxchar_present) {
		return;
	}
	if((pua->charsize > 8) || pua->force_utf8) {
		while(RXBUF_LVL(pua) > 0) {
			uint32_t rxWord;
			int cnt;
			cnt = utf8_to_unicode(&pua->utf8ToUnicodeCtxt,&rxWord,pua->rxbuf[RXBUF_RP(pua)]);
			pua->rxbuf_rp++;
			if(cnt) {
				pua->rxChar = rxWord;
				pua->rxchar_present = 1;
				if(pua->rx_enabled) {
					Uart_RxEvent(&pua->serdev);
				}
				break;
			}
		}
	} else {
		pua->rxChar = pua->rxbuf[RXBUF_RP(pua)];
		pua->rxbuf_rp++;
		pua->rxchar_present = 1;
		if(pua->rx_enabled) {
			Uart_RxEvent(&pua->serdev);
		}
	}
}

/**
 *******************************************************************************
 * Event handler for reading from the ptmx device 
 *******************************************************************************
 */
static void 
Ptmx_Input(void *eventData,int mask) {
	PtmxUart *pua = eventData;
	int max_bytes = RXBUF_SIZE - RXBUF_WP(pua);	
	int result;
	if(max_bytes == 0) {
		fprintf(stderr,"Bug in %s\n",__func__);
		exit(1);
	}
	result = read(pua->fd,pua->rxbuf + RXBUF_WP(pua),max_bytes);
	if((result < 0) && (errno != EAGAIN)) {
		Ptmx_Reopen(pua);			
		return;
	} else if(result > 0) {
		pua->rxbuf_wp += result;
		if(!pua->rxchar_present) {
			Ptmx_RefillRxChar(pua);
		}
	}
	return;
}

/**
 ******************************************************************************
 * \fn static int Ptmx_Read(SerialDevice *sd,UartChar *buf,int maxlen)
 * The Uart simulator reads a Character from the ptmx.  
 ******************************************************************************
 */
static int
Ptmx_Read(SerialDevice *sd,UartChar *buf,int maxlen)
{
	PtmxUart *pua = sd->owner;
        if(maxlen > 0) {
                if(pua->rxchar_present) {
                        buf[0] = pua->rxChar;
                        pua->rxchar_present = 0;
                        return 1;
		}
        }
        return 0;
}

/**
 ********************************************************************************
 * \fn static void Ptxm_StopTx(SerialDevice *sd)
 ********************************************************************************
 */
static void
Ptmx_StopTx(SerialDevice *sd)
{
        PtmxUart *pua = sd->owner;
        pua->tx_enabled = 0;
}

/**
 *******************************************************************************
 * \fn static void Ptxm_StartTx(SerialDevice *sd)
 *******************************************************************************
 */
static void
Ptmx_StartTx(SerialDevice *sd)
{
        PtmxUart *pua = sd->owner;
        pua->tx_enabled = 1;
	if(!CycleTimer_IsActive(&pua->txBaudTimer)) {
		CycleTimer_Mod(&pua->txBaudTimer,MicrosecondsToCycles(pua->usecs_per_char));
	}
}

/**
 ****************************************************************************
 * Tell the serial device that the PTMX device can send data now 
 ****************************************************************************
 */
static void
Ptmx_StartRx(SerialDevice *sd)
{
        PtmxUart *pua = sd->owner;
        pua->rx_enabled = 1;
        if(pua->rxchar_present) {
                Uart_RxEvent(&pua->serdev);
        }
        if(RXBUF_LVL(pua) > 0) {
                if(!CycleTimer_IsActive(&pua->rxBaudTimer)) {
                        CycleTimer_Mod(&pua->rxBaudTimer,MicrosecondsToCycles(pua->usecs_per_char));
                }
        }
}

static void
Ptmx_StopRx(SerialDevice *sd) {
        PtmxUart *pua = sd->owner;
        pua->rx_enabled = 0;
}

static int
Ptmx_SerialCmd(SerialDevice *sd,UartCmd *cmd)
{
	PtmxUart *pua = sd->owner;
	switch(cmd->opcode) {
                case UART_OPC_SET_BAUDRATE:
			if(cmd->arg) {
				pua->usecs_per_char = 100000 / cmd->arg;
				fprintf(stdout,"%d usecs per char\n",pua->usecs_per_char);
			}
                        break;
		case UART_OPC_SET_CSIZE:
			pua->charsize = cmd->arg;	
			break;

	}
        return 0;
}

/**
 *****************************************************************
 * \fn static int Ptmx_Writehandler(void *eventData,int flags)
 * Event handler called when ptmx device is ready for writing
 *****************************************************************
 */
static void 
Ptmx_Writehandler(void *eventData,int flags)
{
        int count;
        PtmxUart *pua = eventData;
        while(pua->txbuf_rp != pua->txbuf_wp) {
                count = write(pua->fd,&pua->txbuf[TXBUF_RP(pua)],1);
                if(count < 0) {
                        if(errno == EAGAIN) {
                                return;
                        } else {
                                Ptmx_Reopen(pua);
                                return;
                        }
                } else if(count == 0) {
                        fprintf(stderr,"EOF on pty\n");
                } else {
			//fprintf(stderr,"Write was\n");
                        pua->txbuf_rp++;
                }
        }
        FIO_RemoveFileHandler(&pua->wfh);
        pua->wfh_active = 0;
        return;
}

static int
Ptmx_Write(SerialDevice *sd,const UartChar *buf,int count)
{
        PtmxUart *pua = sd->owner;
        if((count == 0) || (pua->txchar_present == 0)) {
                return 0;
        }
       	pua->txchar_present = 0;
	if(TXBUF_ROOM(pua) < 3) {
		return 0;
	}
	/* Force UTF 8 for > 8 bit */
	if((pua->charsize > 8) || pua->force_utf8) {
		uint8_t data[3];
		int cnt = unicode_to_utf8((uint16_t)buf[0],data);	
		int i;
		for(i = 0; i < cnt; i++) {
			pua->txbuf[TXBUF_WP(pua)] = data[i];
			pua->txbuf_wp++;
		}
	} else {
		pua->txbuf[TXBUF_WP(pua)] = buf[0];
		pua->txbuf_wp++;
	}
	if(!pua->wfh_active) {
		FIO_AddFileHandler(&pua->wfh,pua->fd,FIO_WRITABLE,Ptmx_Writehandler,pua);
                pua->wfh_active = 1;
	}
	return 1;
}

/**
 ***************************************************************
 * Uart RX = Ptmx module Tx
 ***************************************************************
 */
static void
Ptmx_RxChar(void *clientData)
{
        PtmxUart *pua = (PtmxUart *) clientData;
        if(RXBUF_LVL(pua) > 0) {
		Ptmx_RefillRxChar(pua);
                if(pua->rx_enabled) {
                        Uart_RxEvent(&pua->serdev);
                	CycleTimer_Mod(&pua->rxBaudTimer,MicrosecondsToCycles(pua->usecs_per_char));
                }
        }
}

/**
 **********************************************************************************
 * Uart Tx = Ptmx Rx
 **********************************************************************************
 */
static void
Ptmx_TxChar(void *clientData)
{
	PtmxUart *pua = clientData;
	pua->txchar_present = 1;
	if(pua->tx_enabled) {
		CycleTimer_Mod(&pua->txBaudTimer,MicrosecondsToCycles(pua->usecs_per_char));
		Uart_TxEvent(&pua->serdev);
	} else {
	}
}

static void
Ptmx_Reopen(PtmxUart *pua) {
	struct termios termios;
        if(pua->rfh_active) {
                FIO_RemoveFileHandler(&pua->rfh);
                pua->rfh_active = 0;
        }
        if(pua->wfh_active) {
                FIO_RemoveFileHandler(&pua->wfh);
                pua->wfh_active = 0;
        }
        if(pua->fd >= 0) {
                close(pua->fd);
        }
        pua->fd = open("/dev/ptmx",O_RDWR | O_NOCTTY);
        if(pua->fd < 0) {
                perror("Failed to open ptmx");
		return;
        }
        unlockpt(pua->fd);
        //fprintf(stdout,"ptsname: %s\n",ptsname(pua->fd));
	if(pua->linkname) {
        	unlink(pua->linkname);
		if(symlink(ptsname(pua->fd),pua->linkname) < 0) {
			perror("can not create symbolic link to ptmx device");
		}
	}
	if(tcgetattr(pua->fd,&termios)<0) {
                fprintf(stderr,"Can not  get terminal attributes\n");
                return;
        }
        cfmakeraw(&termios);
        if(tcsetattr(pua->fd,TCSAFLUSH,&termios)<0) {
                perror("can't set terminal settings");
                return;
        }
        fcntl(pua->fd,F_SETFL,O_NONBLOCK);
        FIO_AddFileHandler(&pua->rfh,pua->fd,FIO_READABLE,Ptmx_Input,pua);
        pua->rfh_active = 1;
}

/**
 ***********************************************************************************
 * \fn static SerialDevice PtmxUart_New(const char *name)
 * Create a new instance of the ptmx UART backend.
 ***********************************************************************************
 */
static SerialDevice *
PtmxUart_New(const char *name)
{
        PtmxUart *pua = sg_new(PtmxUart);
        SerialDevice *sd = &pua->serdev;
        sd->owner = pua;
        sd->uart_cmd = Ptmx_SerialCmd;
        sd->stop_tx = Ptmx_StopTx;
        sd->start_tx = Ptmx_StartTx;
        sd->stop_rx = Ptmx_StopRx;
        sd->start_rx = Ptmx_StartRx;
        sd->write = Ptmx_Write;
        sd->read =  Ptmx_Read;
	pua->fd = -1; 
	pua->charsize = 8;
	pua->force_utf8 = 0;
	Config_ReadUInt32(&pua->force_utf8,name,"utf8");
	pua->linkname = Config_ReadVar(name,"link");
	if(!pua->linkname) {
		pua->linkname = alloca(strlen(name) + 20); 
		sprintf(pua->linkname,"/tmp/pty_%s",name);
	}
        pua->usecs_per_char = 330;
        CycleTimer_Init(&pua->rxBaudTimer,Ptmx_RxChar,pua);
        CycleTimer_Init(&pua->txBaudTimer,Ptmx_TxChar,pua);
	fprintf(stderr,"PTMX pseudo Terminal Uart backend for \"%s\" at \"%s\"\n",name,pua->linkname);
	Ptmx_Reopen(pua); 
        return sd;
}

/*
 *******************************************************************************
 * void Ptmx_Init(void)
 *      It registers a SerialDevice emulator module of type "ptmx"
 *******************************************************************************
 */
__CONSTRUCTOR__ static void
Ptmx_Init(void)
{
        SerialModule_Register("ptmx",PtmxUart_New);
        fprintf(stderr,"Registered /dev/ptmx UART Emulator module\n");
}
