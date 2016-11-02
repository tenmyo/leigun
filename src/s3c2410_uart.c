/*
 *************************************************************************************************
 * Emulation of S3C2410 UART 
 *
 * state: not implemented 
 *
 * Copyright 2007 Jochen Karrer. All rights reserved.
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

#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/fcntl.h>
#include <bus.h>
#include <fio.h>
#include <signode.h>
#include <serial.h>
#include <configfile.h>
#include <sgstring.h>
#include <s3c2410_uart.h>
#include <clock.h>
#include <senseless.h>

#define UART_ULCON	((base)+0x00)
#define		ULCON_INFRARED		(1<<6)
#define		ULCON_PARITY_MODE_MASK	(7<<3)
#define		ULCON_PARITY_MODE_SHIFT	(3)
#define		ULCON_STOP_BITS		(1<<2)
#define		ULCON_WORDLENGTH_MASK	(3<<0)
#define		ULCON_WORDLENGTH_SHIFT	(0)
#define UART_UCON	((base)+0x04)
#define		UCON_CLOCKSEL		(1<<10)
#define		UCON_TX_INTTYPE		(1<<9)
#define		UCON_RX_INTTYPE		(1<<8)
#define		UCON_RX_TOUT_EN		(1<<7)
#define		UCON_RX_ERRSTATIE	(1<<6)
#define		UCON_LOOPBACK		(1<<5)
#define		UCON_TXMODE_MASK	(3<<2)
#define		UCON_RXMODE_MASK	(3)
#define		UCON_RXMODE_SHIFT	(0)
#define UART_UFCON	((base)+0x08)
#define 	UFCON_TXTL_MASK		(3<<6)
#define		UFCON_TXTL_SHIFT	(6)
#define 	UFCON_RXTL_MASK		(3<<4)
#define		UFCON_RXTL_SHIFT	(4)
#define		UFCON_TXFIFO_RESET	(1<<2)
#define		UFCON_RXFIFO_RESET	(1<<1)
#define		UFCON_FIFO_ENABLE	(1<<0)
#define UART_UMCON	((base)+0x0c)
#define		UMCON_AFC	(1<<4)
#define		UMCON_RQTS	(1<<0)
#define UART_UTRSTAT	((base)+0x10)
#define		UTRSTAT_TXEMPTY		(1<<2)
#define		UTRSTAT_TXBEMPTY	(1<<1)
#define		UTRSTAT_RXDREADY	(1<<0)
#define UART_UERSTAT	((base)+0x14)
#define UART_UFSTAT	((base)+0x18)
#define	UART_UMSTAT	((base)+0x1c)
#define	UART_UTXH	((base)+0x20)
#define	UART_URXH	((base)+0x24)
#define	UART_UBRDIV0	((base)+0x28)

typedef struct Uart {
	BusDevice bdev;	
	UartPort *port; /* Connection to SerialDevice */
	uint8_t txfifo[16];
	unsigned int txfifo_rp;
	unsigned int txfifo_wp;

	uint8_t rxfifo[16];
	unsigned int rxfifo_rp;
	unsigned int rxfifo_wp;
} Uart;

/*
 * --------------------------------------------------------
 * ULCON Line control register
 * --------------------------------------------------------
 */
static uint32_t
ulcon_read(void *clientData,uint32_t address,int rqlen)
{
        return 0;
}

static void
ulcon_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
}


static uint32_t
ucon_read(void *clientData,uint32_t address,int rqlen)
{
        return 0;
}

static void
ucon_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
}

static uint32_t
ufcon_read(void *clientData,uint32_t address,int rqlen)
{
        return 0;
}

static void
ufcon_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
}

static uint32_t
umcon_read(void *clientData,uint32_t address,int rqlen)
{
        return 0;
}

static void
umcon_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
}

static uint32_t
utrstat_read(void *clientData,uint32_t address,int rqlen)
{
        return 0;
}

static void
utrstat_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
}

static uint32_t
uerstat_read(void *clientData,uint32_t address,int rqlen)
{
        return 0;
}

static void
uerstat_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
}

static uint32_t
ufstat_read(void *clientData,uint32_t address,int rqlen)
{
        return 0;
}

static void
ufstat_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
}

static uint32_t
umstat_read(void *clientData,uint32_t address,int rqlen)
{
        return 0;
}

static void
umstat_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
}

static uint32_t
utxh_read(void *clientData,uint32_t address,int rqlen)
{
        return 0;
}

static void
utxh_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
}

static uint32_t
urxh_read(void *clientData,uint32_t address,int rqlen)
{
        return 0;
}

static void
urxh_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
}

static uint32_t
ubrdiv0_read(void *clientData,uint32_t address,int rqlen)
{
        return 0;
}

static void
ubrdiv0_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
}

static void
S3C2410Uart_Map(void *owner,uint32_t base,uint32_t mask,uint32_t flags)
{
	Uart *uart = (Uart*) owner;
	IOH_New32(UART_ULCON,ulcon_read,ulcon_write,uart);
	IOH_New32(UART_UCON,ucon_read,ucon_write,uart);
	IOH_New32(UART_UFCON,ufcon_read,ufcon_write,uart);
	IOH_New32(UART_UMCON,umcon_read,umcon_write,uart);
	IOH_New32(UART_UTRSTAT,utrstat_read,utrstat_write,uart);
	IOH_New32(UART_UERSTAT,uerstat_read,uerstat_write,uart);
	IOH_New32(UART_UFSTAT,ufstat_read,ufstat_write,uart);
	IOH_New32(UART_UMSTAT,umstat_read,umstat_write,uart);
	IOH_New32(UART_UTXH,utxh_read,utxh_write,uart);
	IOH_New32(UART_URXH,urxh_read,urxh_write,uart);
	IOH_New32(UART_UBRDIV0,ubrdiv0_read,ubrdiv0_write,uart);
}

static void
S3C2410Uart_UnMap(void *owner,uint32_t base,uint32_t mask)
{
	IOH_Delete32(UART_ULCON);
	IOH_Delete32(UART_UCON);
	IOH_Delete32(UART_UFCON);
	IOH_Delete32(UART_UMCON);
	IOH_Delete32(UART_UTRSTAT);
	IOH_Delete32(UART_UERSTAT);
	IOH_Delete32(UART_UFSTAT);
	IOH_Delete32(UART_UMSTAT);
	IOH_Delete32(UART_UTXH);
	IOH_Delete32(UART_URXH);
	IOH_Delete32(UART_UBRDIV0);
}

BusDevice *
S3C2410_UartNew(const char *name) 
{
	Uart *uart = sg_new(Uart);
	uart->bdev.first_mapping=NULL;
        uart->bdev.Map=S3C2410Uart_Map;
        uart->bdev.UnMap=S3C2410Uart_UnMap;
        uart->bdev.owner=uart;
        uart->bdev.hw_flags=MEM_FLAG_WRITABLE|MEM_FLAG_READABLE;
        //update_interrupt(usart);
        //uart->port = Uart_New(name,serial_input,serial_output,NULL,usart);
        return &uart->bdev;
}


