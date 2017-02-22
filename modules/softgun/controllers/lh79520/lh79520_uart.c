/*
 *************************************************************************************************
 * Emulation of LH79520 UART 
 *
 * state: Not implemented 
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
#include "bus.h"
#include "signode.h"
#include "clock.h"
#include "lh79520_uart.h"
#include "serial.h"
#include "configfile.h"
#include "senseless.h"
#include "sgstring.h"

#define LH_UARTDR(base)	 	((base)+0x000)
#define LH_UARTRSR(base) 	((base)+0x004)
#define LH_UARTECR(base)	((base)+0x004)
#define LH_UARTFR(base)		((base)+0x018)
#define LH_UARTILPR(base)	((base)+0x020)
#define LH_UARTIBRD(base)	((base)+0x024)
#define LH_UARTFBRD(base)	((base)+0x028)
#define LH_UARTLCR_H(base) 	((base)+0x02c)
#define LH_UARTCR(base)		((base)+0x030)
#define LH_UARTIFLS(base)	((base)+0x034)
#define LH_UARTIMSC(base)	((base)+0x038)
#define LH_UARTRIS(base)	((base)+0x03c)
#define LH_UARTMIS(base)	((base)+0x040)
#define LH_UARTICR(base)	((base)+0x044)
#define LH_UARTTCR(base)	((base)+0x080)

typedef struct LH_Uart {
	BusDevice bdev;
	UartPort *port;
	char *name;
	int interrupt_posted;
	SigNode *irqNode;
	Clock_t *in_clk;
	uint32_t dr;
	uint32_t rsr;
	uint32_t ecr;
	uint32_t fr;
	uint32_t ilpr;
	uint32_t ibrd;
	uint32_t fbrd;
	uint32_t lcr_h;
	uint32_t cr;
	uint32_t ifls;
	uint32_t imsc;
	uint32_t ris;
	uint32_t mis;
	uint32_t icr;
	uint32_t tcr;
//      SigNode *rxDmaReqNode;
//      SigNode *txDmaReqNode;
} LH_Uart;

static void
serial_input(void *cd, UartChar c)
{
#if 0
	IMX_Uart *iuart = cd;
	int fifocount;
	while (1) {
		uint8_t c;
		int count = Uart_Read(iuart->port, &c, 1);
		int room;
		if (count == 1) {
			serial_rx_char(iuart, c);
			//fprintf(stdout,"Console got %c\n",c);
		} else {
			break;
		}
		room = RX_FIFO_ROOM(iuart);
		if (room < 1) {
			Uart_StopRx(iuart->port);
			break;
		}
	}
	update_rxdma(iuart);
	fifocount = RX_FIFO_COUNT(iuart);
	if (fifocount) {
		update_interrupts(iuart);
	}
	return;
#endif
}

/*
 * --------------------------------------------------------------------------------
 * Serial output is called whenever the underlying UART backend is able
 * to accept one or more chars. If serial output has no data for sending
 * (TX fifo is empty). Then the backend has to be stopped because the
 * Event handler for fetching more data should not be invoked anymore.
 * --------------------------------------------------------------------------------
 */
static bool 
serial_output(void *cd,UartChar *c)
{
#if 0
	LH_Uart *lhuart = cd;
	IMX_Uart *iuart = cd;
	int fill;
	while (TX_FIFO_COUNT(iuart) > 0) {
		int count, len;
		fill = TX_FIFO_COUNT(iuart);
		len = fill;
		if ((TX_FIFO_RIDX(iuart) + fill) > TX_FIFO_SIZE) {
			len = TX_FIFO_SIZE - TX_FIFO_RIDX(iuart);
		}
		count = Uart_Write(iuart->port, &iuart->txfifo[TX_FIFO_RIDX(iuart)], len);
		if (count > 0) {
			iuart->txfifo_rp = iuart->txfifo_rp + count;
			update_txdma(iuart);
		}
	}
	Uart_StopTx(iuart->port);
	update_interrupts(iuart);
	return;
#endif
	return true;
}

static uint32_t
uartdr_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "LH-Uart: Reading from 0x%08x not implemented\n", address);
	return 0;
}

static void
uartdr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "LH-Uart: Writing to %08x not implemented\n", address);
	return;
}

static uint32_t
uartrsr_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "LH-Uart: Reading from 0x%08x not implemented\n", address);
	return 0;
}

static void
uartecr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "LH-Uart: Writing to %08x not implemented\n", address);
	return;
}

static uint32_t
uartfr_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "LH-Uart: Reading from 0x%08x not implemented\n", address);
	return 0;
}

static void
uartfr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "LH-Uart: Writing to %08x not implemented\n", address);
	return;
}

static uint32_t
uartilpr_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "LH-Uart: Reading from 0x%08x not implemented\n", address);
	return 0;
}

static void
uartilpr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "LH-Uart: Writing to %08x not implemented\n", address);
	return;
}

static uint32_t
uartibrd_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "LH-Uart: Reading from 0x%08x not implemented\n", address);
	return 0;
}

static void
uartibrd_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "LH-Uart: Writing to %08x not implemented\n", address);
	return;
}

static uint32_t
uartfbrd_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "LH-Uart: Reading from 0x%08x not implemented\n", address);
	return 0;
}

static void
uartfbrd_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "LH-Uart: Writing to %08x not implemented\n", address);
	return;
}

static uint32_t
uartlcr_h_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "LH-Uart: Reading from 0x%08x not implemented\n", address);
	return 0;
}

static void
uartlcr_h_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "LH-Uart: Writing to %08x not implemented\n", address);
	return;
}

static uint32_t
uartcr_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "LH-Uart: Reading from 0x%08x not implemented\n", address);
	return 0;
}

static void
uartcr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "LH-Uart: Writing to %08x not implemented\n", address);
	return;
}

static uint32_t
uartifls_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "LH-Uart: Reading from 0x%08x not implemented\n", address);
	return 0;
}

static void
uartifls_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "LH-Uart: Writing to %08x not implemented\n", address);
	return;
}

static uint32_t
uartimsc_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "LH-Uart: Reading from 0x%08x not implemented\n", address);
	return 0;
}

static void
uartimsc_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "LH-Uart: Writing to %08x not implemented\n", address);
	return;
}

static uint32_t
uartris_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "LH-Uart: Reading from 0x%08x not implemented\n", address);
	return 0;
}

static void
uartris_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "LH-Uart: Writing to %08x not implemented\n", address);
	return;
}

static uint32_t
uartmis_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "LH-Uart: Reading from 0x%08x not implemented\n", address);
	return 0;
}

static void
uartmis_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "LH-Uart: Writing to %08x not implemented\n", address);
	return;
}

static uint32_t
uarticr_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "LH-Uart: Reading from 0x%08x not implemented\n", address);
	return 0;
}

static void
uarticr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "LH-Uart: Writing to %08x not implemented\n", address);
	return;
}

static uint32_t
uarttcr_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "LH-Uart: Reading from 0x%08x not implemented\n", address);
	return 0;
}

static void
uarttcr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "LH-Uart: Writing to %08x not implemented\n", address);
	return;
}

static void
LHUart_Map(void *owner, uint32_t base, uint32_t mask, uint32_t flags)
{
	LH_Uart *lhuart = owner;
	IOH_New32(LH_UARTDR(base), uartdr_read, uartdr_write, lhuart);
	IOH_New32(LH_UARTRSR(base), uartrsr_read, uartecr_write, lhuart);
	IOH_New32(LH_UARTFR(base), uartfr_read, uartfr_write, lhuart);
	IOH_New32(LH_UARTILPR(base), uartilpr_read, uartilpr_write, lhuart);
	IOH_New32(LH_UARTIBRD(base), uartibrd_read, uartibrd_write, lhuart);
	IOH_New32(LH_UARTFBRD(base), uartfbrd_read, uartfbrd_write, lhuart);
	IOH_New32(LH_UARTLCR_H(base), uartlcr_h_read, uartlcr_h_write, lhuart);
	IOH_New32(LH_UARTCR(base), uartcr_read, uartcr_write, lhuart);
	IOH_New32(LH_UARTIFLS(base), uartifls_read, uartifls_write, lhuart);
	IOH_New32(LH_UARTIMSC(base), uartimsc_read, uartimsc_write, lhuart);
	IOH_New32(LH_UARTRIS(base), uartris_read, uartris_write, lhuart);
	IOH_New32(LH_UARTMIS(base), uartmis_read, uartmis_write, lhuart);
	IOH_New32(LH_UARTICR(base), uarticr_read, uarticr_write, lhuart);
	IOH_New32(LH_UARTTCR(base), uarttcr_read, uarttcr_write, lhuart);
}

static void
LHUart_UnMap(void *owner, uint32_t base, uint32_t mask)
{
	IOH_Delete32(LH_UARTDR(base));
	IOH_Delete32(LH_UARTRSR(base));
	IOH_Delete32(LH_UARTFR(base));
	IOH_Delete32(LH_UARTILPR(base));
	IOH_Delete32(LH_UARTIBRD(base));
	IOH_Delete32(LH_UARTFBRD(base));
	IOH_Delete32(LH_UARTLCR_H(base));
	IOH_Delete32(LH_UARTCR(base));
	IOH_Delete32(LH_UARTIFLS(base));
	IOH_Delete32(LH_UARTIMSC(base));
	IOH_Delete32(LH_UARTRIS(base));
	IOH_Delete32(LH_UARTMIS(base));
	IOH_Delete32(LH_UARTICR(base));
	IOH_Delete32(LH_UARTTCR(base));
}

BusDevice *
LH79520Uart_New(const char *name)
{
	LH_Uart *lhuart = sg_new(LH_Uart);
	lhuart->bdev.first_mapping = NULL;
	lhuart->bdev.Map = LHUart_Map;
	lhuart->bdev.UnMap = LHUart_UnMap;
	lhuart->bdev.owner = lhuart;
	lhuart->bdev.hw_flags = MEM_FLAG_WRITABLE | MEM_FLAG_READABLE;
	lhuart->name = sg_strdup(name);
	lhuart->port = Uart_New(name, serial_input, serial_output, NULL, lhuart);
	lhuart->in_clk = Clock_New("%s.clk", name);
	/* Clock should come from the clock_module */
	//Clock_SetFreq(iuart->in_clk,44333342.280);

	//update_interrupts(lhuart);
	fprintf(stderr, "IMX21 Uart \"%s\" created\n", name);
	return &lhuart->bdev;
}
