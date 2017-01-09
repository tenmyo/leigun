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
#include "configfile.h"
#include "sglib.h"
#include "compiler_extensions.h"
#include "cleanup.h"

#define RXBUF_SIZE 128

#define RXBUF_RP(pua) ((pua)->rxbuf_rp % RXBUF_SIZE)
#define RXBUF_WP(pua) ((pua)->rxbuf_wp % RXBUF_SIZE)
#define RXBUF_LVL(pua) ((pua)->rxbuf_wp - (pua)->rxbuf_rp)
#define RXBUF_ROOM(pua) (RXBUF_SIZE - RXBUF_LVL(pua))

#define TXBUF_SIZE 256
#define TXBUF_RP(pua) ((pua)->txbuf_rp % TXBUF_SIZE)
#define TXBUF_WP(pua) ((pua)->txbuf_wp % TXBUF_SIZE)
#define TXBUF_LVL(pua) ((pua)->txbuf_wp - (pua)->txbuf_rp)
#define TXBUF_ROOM(pua) (TXBUF_SIZE - TXBUF_LVL(pua))

typedef struct PtmxUart {
    SerialDevice serdev;
    Utf8ToUnicodeCtxt utf8ToUnicodeCtxt;
    const char *group;
    const char *owner;
    const char *mode;
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
    /* MDB Translator */
    uint32_t force_mdbtrans;
    uint16_t mdbTransAssBuf;
    uint16_t mdbRxBytes;
    uint8_t mdbPktChksum;

    unsigned int rxbuf_wp;
    unsigned int rxbuf_rp;

    uint8_t txbuf[TXBUF_SIZE];  /* UTF8 format if charsize > 8 */
    unsigned int txbuf_wp;
    unsigned int txbuf_rp;

    CycleTimer rxBaudTimer;
//      int txchar_present;
    uint32_t usecs_per_char;
} PtmxUart;

static void Ptmx_Reopen(PtmxUart * pua);
static void Ptmx_RefillRxChar(PtmxUart * pua);
static void Ptmx_Writehandler(void *eventData, int flags);

/**
 **********************************************************************
 * Write to the internal (RX-Buffer) 
 **********************************************************************
 */
static void
MdbWriteToRxBuf(PtmxUart * pua, uint8_t * pc2x8, int cnt)
{
    int i;
    for (i = 0; i < cnt; i++) {
        pua->rxbuf[RXBUF_WP(pua)] = pc2x8[i];
        pua->rxbuf_wp++;
    }
    if (!pua->rxchar_present) {
        Ptmx_RefillRxChar(pua);
    }
}

/**
 * static void PCCmd(PtmxUart *pua, uint16_t cmd); 
 */
static void
PCCmd(PtmxUart * pua, uint16_t cmd)
{
    char *str = "WeichGewehr " __DATE__ " " __TIME__;
    int i;
    fprintf(stderr, "Got cmd %04x\n", cmd);
    switch (cmd) {
        case 0xc301:
            pua->txbuf[TXBUF_WP(pua)] = cmd >> 8;
            pua->txbuf_wp++;
            pua->txbuf[TXBUF_WP(pua)] = 0x40;
            pua->txbuf_wp++;
            for (i = 0; i < strlen(str) + 1; i++) {
                pua->txbuf[TXBUF_WP(pua)] = str[i];
                pua->txbuf_wp++;
            }
            if (!pua->wfh_active) {
                FIO_AddFileHandler(&pua->wfh, pua->fd, FIO_WRITABLE, Ptmx_Writehandler, pua);
                pua->wfh_active = 1;
            }
            break;

        case 0xcb00:           // CMD_MDB_BUS_RESET:
            pua->txbuf[TXBUF_WP(pua)] = cmd >> 8;
            pua->txbuf_wp++;
            pua->txbuf[TXBUF_WP(pua)] = 0x40;
            pua->txbuf_wp++;
            if (!pua->wfh_active) {
                FIO_AddFileHandler(&pua->wfh, pua->fd, FIO_WRITABLE, Ptmx_Writehandler, pua);
                pua->wfh_active = 1;
            }
            break;

        default:
            break;
    }
}

/**
 *******************************************************
 * MDB: Translate 9 bit words to 2x8 bits
 ******************************************************
 */
static int
mdb9_to_pc2x8(PtmxUart * pua, uint16_t mdb9, uint8_t * pc2x8)
{
    pc2x8[0] = ((mdb9 >> 4) & 0x1f) | 0x80;
    pc2x8[1] = mdb9 & 0xf;
    fprintf(stderr, "T -> 0x%04x\n", mdb9);
    pua->rxbuf_wp = pua->rxbuf_rp = 0;
    pua->mdbRxBytes++;
    if (mdb9 & 0x100) {
        if (pua->mdbRxBytes == 1) {
            //fprintf(stderr,"Don't ack ack\n");
            /* Do not ack an ack/nak only */
        } else if (pua->mdbPktChksum == (mdb9 & 0xff)) {
            uint8_t data[2] = { 0x80, 0 };
            MdbWriteToRxBuf(pua, data, 2);
        } else {
            uint8_t data[2] = { 0x9f, 0xf };
            MdbWriteToRxBuf(pua, data, 2);
        }
        pua->mdbRxBytes = 0;
        pua->mdbPktChksum = 0;
    } else {
        pua->mdbPktChksum += (mdb9 & 0xff);
    }
    return 2;
}

/**
 *******************************************************
 * MDB: Translate 2x8 bit to 9 Bit words
 ******************************************************
 */
static int
pc2x8_to_mdb9(PtmxUart * pua, uint32_t * mdbWord, uint8_t pcbyte)
{
    //fprintf(stderr,"PCByte 0x%02x\n", pcbyte);
    if (pcbyte & 0x80) {
        pua->mdbTransAssBuf = (uint16_t) pcbyte << 8;
        return 0;
    } else {
        uint16_t w;
        pua->mdbTransAssBuf |= pcbyte;
        w = pua->mdbTransAssBuf;
        if ((w & 0xc000) == 0xc000) {
            /* Do not output commands on MDB */
            PCCmd(pua, w);
            return 0;
        }
        *mdbWord = ((w & 0x1f00) >> 4) | (w & 0xf);
        pua->mdbPktChksum = 0;
        pua->txbuf_wp = pua->txbuf_rp = 0;
//        fprintf(stderr,"R <- %04x\n", *mdbWord);
        return 1;
    }
}

/**
 ****************************************************************
 * \fn static void Ptmx_RefillRxChar(PtmxUart *pua); 
 ****************************************************************
 */

/**
 *******************************************************************************
 * Event handler for reading from the ptmx device 
 *******************************************************************************
 */
static void
Ptmx_Input(void *eventData, int mask)
{
    PtmxUart *pua = eventData;
    int max_bytes = RXBUF_SIZE - RXBUF_WP(pua);
    int read_size;
    int result;
    if (max_bytes == 0) {
        fprintf(stderr, "Bug in %s\n", __func__);
        exit(1);
    }
    read_size = RXBUF_ROOM(pua);
    if (read_size == 0) {
        if (pua->rfh_active) {
            FIO_RemoveFileHandler(&pua->rfh);
            pua->rfh_active = 0;
        }
        return;
    }
    if (read_size > max_bytes) {
        read_size = max_bytes;
    }
    result = read(pua->fd, pua->rxbuf + RXBUF_WP(pua), read_size);
    //fprintf(stderr, "Read %u: %02x, wp %u, rp %u\n", result, *(pua->rxbuf + RXBUF_WP(pua)), pua->rxbuf_wp, pua->rxbuf_rp);
    if ((result == 0)) {
        fprintf(stderr, "Reopen EOF\n");
        Ptmx_Reopen(pua);
    } else if ((result < 0) && (errno != EAGAIN)) {
        fprintf(stderr, "Reopen error\n");
        Ptmx_Reopen(pua);
        return;
    } else if (result > 0) {
        pua->rxbuf_wp += result;
        if (!CycleTimer_IsActive(&pua->rxBaudTimer)) {
            /* First char is immediate, delay is after the last char ! */
            CycleTimer_Mod(&pua->rxBaudTimer, 0);
        }
    }
    return;
}

static void
Ptmx_RefillRxChar(PtmxUart * pua)
{
    if (pua->rxchar_present) {
        return;
    }
    if ((pua->charsize > 8) || pua->force_utf8 || pua->force_mdbtrans) {
        while (RXBUF_LVL(pua) > 0) {
            uint32_t rxWord;
            int cnt;
            if (pua->force_mdbtrans) {
                cnt = pc2x8_to_mdb9(pua, &rxWord, pua->rxbuf[RXBUF_RP(pua)]);
            } else {
                cnt = utf8_to_unicode(&pua->utf8ToUnicodeCtxt, &rxWord, pua->rxbuf[RXBUF_RP(pua)]);
            }
            pua->rxbuf_rp++;
            if (cnt) {
                pua->rxChar = rxWord;
                pua->rxchar_present = 1;
                if (pua->rx_enabled) {
                    Uart_RxEvent(&pua->serdev);
                }
                break;
            }
        }
    } else {
        pua->rxChar = pua->rxbuf[RXBUF_RP(pua)];
        pua->rxbuf_rp++;
        pua->rxchar_present = 1;
        if (pua->rx_enabled) {
            Uart_RxEvent(&pua->serdev);
        }
    }
    if (pua->rfh_active == 0) {
        FIO_AddFileHandler(&pua->rfh, pua->fd, FIO_READABLE, Ptmx_Input, pua);
        pua->rfh_active = 1;
    }
    //fprintf(stderr,"Refilled with %02x at %llu\n", pua->rxChar,CycleCounter_Get());
}

/**
 ******************************************************************************
 * \fn static int Ptmx_Read(SerialDevice *sd,UartChar *buf,int maxlen)
 * The Uart simulator reads a Character from the ptmx.  
 ******************************************************************************
 */
static int
Ptmx_Read(SerialDevice * sd, UartChar * buf, int maxlen)
{
    PtmxUart *pua = sd->owner;
    if (maxlen > 0) {
        if (pua->rxchar_present) {
            buf[0] = pua->rxChar;
            pua->rxchar_present = 0;
            return 1;
        }
    }
    return 0;
}

/**
 ****************************************************************************
 * Tell the serial device that the PTMX device can send data now 
 ****************************************************************************
 */
static void
Ptmx_StartRx(SerialDevice * sd)
{
    PtmxUart *pua = sd->owner;
    pua->rx_enabled = 1;
    if (pua->rxchar_present) {
        Uart_RxEvent(&pua->serdev);
    }
    if (RXBUF_LVL(pua) > 0) {
        if (!CycleTimer_IsActive(&pua->rxBaudTimer)) {
            CycleTimer_Mod(&pua->rxBaudTimer, MicrosecondsToCycles(pua->usecs_per_char));
        }
    }
}

static void
Ptmx_StopRx(SerialDevice * sd)
{
    PtmxUart *pua = sd->owner;
    pua->rx_enabled = 0;
}

static int
Ptmx_SerialCmd(SerialDevice * sd, UartCmd * cmd)
{
    PtmxUart *pua = sd->owner;
    switch (cmd->opcode) {
        case UART_OPC_SET_BAUDRATE:
            if (cmd->arg) {
                pua->usecs_per_char = (1000000 * 10) / cmd->arg;
                fprintf(stderr, "%d usecs per char, baud %u\n", pua->usecs_per_char, cmd->arg);
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
Ptmx_Writehandler(void *eventData, int flags)
{
    int count;
    PtmxUart *pua = eventData;
    while (pua->txbuf_rp != pua->txbuf_wp) {
        unsigned int cnt = TXBUF_LVL(pua);
        if ((TXBUF_RP(pua) + cnt) > TXBUF_SIZE) {
            cnt = TXBUF_SIZE - TXBUF_RP(pua);
        }
        count = write(pua->fd, &pua->txbuf[TXBUF_RP(pua)], cnt);
        if (count < 0) {
            if (errno == EAGAIN) {
                return;
            } else {
                Ptmx_Reopen(pua);
                return;
            }
        } else if (count == 0) {
            fprintf(stderr, "Write of %u bytes to pty failed\n", cnt);
            pua->txbuf_rp = pua->txbuf_wp;
        } else {
            //fprintf(stderr,"Write %u\n", cnt);
            pua->txbuf_rp += cnt;
        }
    }
    FIO_RemoveFileHandler(&pua->wfh);
    pua->wfh_active = 0;
    return;
}

static int
Ptmx_Write(SerialDevice * sd, const UartChar * buf, int count)
{
    PtmxUart *pua = sd->owner;
    if (count == 0) {
        return 0;
    }
    //pua->txchar_present = 0;
    if (TXBUF_ROOM(pua) < 3) {
        return 0;
    }
    /* Force UTF 8 for > 8 bit */
    if ((pua->charsize > 8) || pua->force_utf8 || pua->force_mdbtrans) {
        uint8_t data[3];
        int cnt;
        int i;
        if (pua->force_mdbtrans) {
            cnt = mdb9_to_pc2x8(pua, (uint16_t) buf[0], data);
        } else {
            cnt = unicode_to_utf8((uint16_t) buf[0], data);
        }
        for (i = 0; i < cnt; i++) {
            pua->txbuf[TXBUF_WP(pua)] = data[i];
            pua->txbuf_wp++;
        }
    } else {
        pua->txbuf[TXBUF_WP(pua)] = buf[0];
        pua->txbuf_wp++;
    }
    if (!pua->wfh_active) {
        FIO_AddFileHandler(&pua->wfh, pua->fd, FIO_WRITABLE, Ptmx_Writehandler, pua);
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
    if (RXBUF_LVL(pua) > 0) {
        Ptmx_RefillRxChar(pua);
        if (pua->rx_enabled) {
            Uart_RxEvent(&pua->serdev);
            CycleTimer_Mod(&pua->rxBaudTimer, MicrosecondsToCycles(pua->usecs_per_char));
        }
    }
}

/**
 ********************************************************************
 * \fn static void Ptmx_CleanupLink(void *eventData); 
 ********************************************************************
 */
static void
Ptmx_CleanupLink(void *eventData)
{
    PtmxUart *pua = eventData;
    unlink(pua->linkname);
}

static void
Ptmx_Reopen(PtmxUart * pua)
{
    struct termios termios;
    if (pua->rfh_active) {
        FIO_RemoveFileHandler(&pua->rfh);
        pua->rfh_active = 0;
    }
    if (pua->wfh_active) {
        FIO_RemoveFileHandler(&pua->wfh);
        pua->wfh_active = 0;
    }
    if (pua->fd >= 0) {
        close(pua->fd);
    }
    pua->fd = open("/dev/ptmx", O_RDWR | O_NOCTTY);
    if (pua->fd < 0) {
        perror("Failed to open ptmx");
        return;
    }
    unlockpt(pua->fd);
    //fprintf(stdout,"ptsname: %s\n",ptsname(pua->fd));
    if (pua->linkname) {
        ExitHandler_Unregister(Ptmx_CleanupLink, pua);
        unlink(pua->linkname);
        if (symlink(ptsname(pua->fd), pua->linkname) < 0) {
            perror("can not create symbolic link to ptmx device");
        } else {
            ExitHandler_Register(Ptmx_CleanupLink, pua);
        }
    }
    if (pua->group) {
        char *cmd = alloca(strlen(pua->group) + 500);
        sprintf(cmd, "chgrp %s %s", pua->group, ptsname(pua->fd));
        if (system(cmd) != 0) {
            fprintf(stderr, "Changing group of \"%s\" to \"%s\" failed\n",
                    ptsname(pua->fd), pua->group);
            sleep(1);
        }
    }
    if (pua->owner) {
        char *cmd = alloca(strlen(pua->owner) + 500);
        sprintf(cmd, "chown %s %s", pua->owner, ptsname(pua->fd));
        if (system(cmd) != 0) {
            fprintf(stderr, "Changing owner of \"%s\" to \"%s\" failed\n",
                    ptsname(pua->fd), pua->owner);
            sleep(1);
        }
    }
    if (pua->mode) {
        char *cmd = alloca(strlen(pua->mode) + 500);
        sprintf(cmd, "chmod %s %s", pua->mode, ptsname(pua->fd));
        if (system(cmd) != 0) {
            fprintf(stderr, "Changing mode of \"%s\" to \"%s\"failed\n",
                    ptsname(pua->fd), pua->mode);
            sleep(1);
        }
    }
    if (tcgetattr(pua->fd, &termios) < 0) {
        fprintf(stderr, "Can not  get terminal attributes\n");
        return;
    }
    cfmakeraw(&termios);
    if (tcsetattr(pua->fd, TCSAFLUSH, &termios) < 0) {
        perror("can't set terminal settings");
        return;
    }
    fcntl(pua->fd, F_SETFL, O_NONBLOCK);
    FIO_AddFileHandler(&pua->rfh, pua->fd, FIO_READABLE, Ptmx_Input, pua);
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
    sd->stop_rx = Ptmx_StopRx;
    sd->start_rx = Ptmx_StartRx;
    sd->write = Ptmx_Write;
    sd->read = Ptmx_Read;
    pua->fd = -1;
    pua->charsize = 8;
    pua->force_utf8 = 0;
    Config_ReadUInt32(&pua->force_utf8, name, "utf8");
    Config_ReadUInt32(&pua->force_mdbtrans, name, "mdbtranslator");
    if (pua->force_utf8 && pua->force_mdbtrans) {
        fprintf(stderr, "PTMX: mdbtranslator mode and UTF-8 mode can not be enabled both\n");
        sleep(1);
        exit(1);
    }
    pua->linkname = Config_ReadVar(name, "link");
    if (!pua->linkname) {
        pua->linkname = alloca(strlen(name) + 20);
        sprintf(pua->linkname, "/tmp/pty_%s", name);
    }
    pua->group = Config_ReadVar(name, "group");
    pua->owner = Config_ReadVar(name, "owner");
    pua->mode = Config_ReadVar(name, "mode");
    pua->usecs_per_char = 330;
    CycleTimer_Init(&pua->rxBaudTimer, Ptmx_RxChar, pua);
    fprintf(stderr, "PTMX pseudo Terminal Uart backend for \"%s\" at \"%s\"\n", name,
            pua->linkname);
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
    SerialModule_Register("ptmx", PtmxUart_New);
    fprintf(stderr, "Registered /dev/ptmx UART Emulator module\n");
}
