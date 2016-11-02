/*
 *************************************************************************************************** 
 * ST Asynchronous serial controller 
 *
 * Copyright 2009 Jochen Karrer. All rights reserved.
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

#include "bus.h"
#include "sgstring.h"
#include "fio.h"
#include "signode.h"
#include "serial.h"
#include "clock.h"
#include "st_asc.h"
#include "senseless.h"

/* Register offsets taken from the stasc.h linux kernel file */

#define ASC_BAUDRATE(base)	((base) + 0x00)
#define ASC_TXBUF(base)		((base) + 0x04)
#define 	TXBUF_MSK	0x01FF
#define ASC_RXBUF(base)		((base) + 0x08)
#define 	RXBUF_MSK	0x03FF
#define 	RXBUF_PE	0x100
#define 	RXBUF_FE	0x200
#define ASC_CTL(base)		((base) + 0x0C)
#define 	CTL_MODE_MSK		0x0007
#define 	CTL_MODE_8BIT		0x0001
#define 	CTL_MODE_7BIT_PAR	0x0003
#define 	CTL_MODE_9BIT		0x0004
#define 	CTL_MODE_8BIT_WKUP	0x0005
#define 	CTL_MODE_8BIT_PAR	0x0007
#define 	CTL_STOP_MSK		0x0018
#define 	CTL_STOP_HALFBIT	0x0000
#define 	CTL_STOP_1BIT		0x0008
#define 	CTL_STOP_1_HALFBIT	0x0010
#define 	CTL_STOP_2BIT		0x0018
#define 	CTL_PARITYODD		0x0020
#define 	CTL_LOOPBACK		0x0040
#define 	CTL_RUN			0x0080
#define 	CTL_RXENABLE		0x0100
#define 	CTL_SCENABLE		0x0200
#define 	CTL_FIFOENABLE		0x0400
#define 	CTL_CTSENABLE		0x0800
#define 	CTL_BAUDMODE		0x1000
#define ASC_INTEN(base)		((base) + 0x10)
#define 	INTEN_RBF		0x0001
#define 	INTEN_TE		0x0002
#define 	INTEN_THE		0x0004
#define 	INTEN_PE		0x0008
#define 	INTEN_FE		0x0010
#define 	INTEN_OE		0x0020
#define 	INTEN_TNE		0x0040
#define 	INTEN_TOI		0x0080
#define 	INTEN_RHF		0x0100
#define ASC_STA(base)		((base) + 0x14)
#define 	STA_RBF		0x0001
#define 	STA_TE		0x0002
#define 	STA_THE		0x0004
#define 	STA_PE		0x0008
#define 	STA_FE		0x0010
#define 	STA_OE		0x0020
#define 	STA_TNE		0x0040
#define 	STA_TOI		0x0080
#define 	STA_RHF		0x0100
#define 	STA_TF		0x0200
#define 	STA_NKD		0x0400
#define ASC_GUARDTIME(base)	((base) + 0x18)
#define 	GUARDTIME_MSK	0x00FF
#define ASC_TIMEOUT(base)	((base) + 0x1C)
#define 	TIMEOUT_MSK     0x00FF
#define ASC_TXRESET(base)	((base) + 0x20)
#define ASC_RXRESET(base)	((base) + 0x24)
#define ASC_RETRIES(base)	((base) + 0x28)
#define 	RETRIES_MSK	0x00FF

#define RX_FIFO_SIZE_MAX (16)
#define RX_FIFO_SIZE(ser) (16)
#define RX_FIFO_MASK(ser) (RX_FIFO_SIZE(ser) - 1)
#define RX_FIFO_COUNT(ser) ((ser)->rxfifo_wp - (ser)->rxfifo_rp)
#define RX_FIFO_ROOM(ser) (RX_FIFO_SIZE(ser) - RX_FIFO_COUNT(ser))
#define RX_FIFO_WP(ser)	(ser->rxfifo_wp & RX_FIFO_MASK(ser))
#define RX_FIFO_RP(ser)	(ser->rxfifo_rp & RX_FIFO_MASK(ser))

#define TX_FIFO_SIZE_MAX (16)
#define TX_FIFO_SIZE(ser) (16)
#define TX_FIFO_MASK(ser) (TX_FIFO_SIZE(ser) - 1)
#define TX_FIFO_COUNT(ser) ((ser)->txfifo_wp - (ser)->txfifo_rp)
#define TX_FIFO_ROOM(ser) (TX_FIFO_SIZE(ser) - TX_FIFO_COUNT(ser))
#define TX_FIFO_WP(ser)	(ser->txfifo_wp & TX_FIFO_MASK(ser))
#define TX_FIFO_RP(ser)	(ser->txfifo_rp & TX_FIFO_MASK(ser))

typedef struct StAsc {
	BusDevice bdev;
	UartPort *backend;
	Clock_t *clk_in;
	Clock_t *clk_baud;

	int baudrate;

	uint32_t reg_BAUDRATE;
	uint32_t reg_TXBUF;
	uint32_t reg_RXBUF;
	uint32_t reg_CTL;
	uint32_t reg_INTEN;
	uint32_t reg_STA;
	uint32_t reg_GUARDTIME;
	uint32_t reg_TIMEOUT;
	uint32_t reg_TXRESET;
	uint32_t reg_RXRESET;
	uint32_t reg_RETRIES;

	UartChar rx_fifo[RX_FIFO_SIZE_MAX];
	uint64_t rxfifo_wp;
	uint64_t rxfifo_rp;
	uint32_t rx_fifo_size;

	UartChar tx_fifo[TX_FIFO_SIZE_MAX];
	uint64_t txfifo_wp;
	uint64_t txfifo_rp;
	uint32_t tx_fifo_size;

	int interrupt_posted;
	SigNode *sigIrq;
} StAsc;

static void
update_interrupts(StAsc * asc)
{
	if (asc->reg_STA & asc->reg_INTEN) {
		if (!asc->interrupt_posted) {
			SigNode_Set(asc->sigIrq, SIG_LOW);
			asc->interrupt_posted = 1;
		}
	} else {
		if (asc->interrupt_posted) {
			SigNode_Set(asc->sigIrq, SIG_HIGH);
			asc->interrupt_posted = 0;
		}
	}
}

static void
update_clock(StAsc * asc)
{
	uint32_t bauddiv;
	bauddiv = asc->reg_BAUDRATE & 0xffff;
	if (asc->reg_CTL & CTL_BAUDMODE) {
		Clock_MakeDerived(asc->clk_baud, asc->clk_in, bauddiv, 1024 * 1024);
	} else {
		Clock_MakeDerived(asc->clk_baud, asc->clk_in, 1, 16 * bauddiv);
	}
}

/*
 **********************************************************************
 * baud_clock_trace
 * Eventhandler called whenever the baud rate changes 
 **********************************************************************
 */

static void
baud_clock_trace(Clock_t * clock, void *clientData)
{
	StAsc *asc = (StAsc *) clientData;
	UartCmd cmd;
	cmd.opcode = UART_OPC_SET_BAUDRATE;
	cmd.arg = Clock_Freq(clock);
	/* Dont forget to update the baudrate when uart is enabled */
	SerialDevice_Cmd(asc->backend, &cmd);
}

/*
 ***************************************************************
 * Filehandler for TX-Fifo
 *      Write chars from TX-Fifo to backend. 
 ***************************************************************
 */
static bool
serial_output(void *cd, UartChar * c)
{
	StAsc *asc = cd;
	if (TX_FIFO_COUNT(asc) > 0) {
		*c = asc->tx_fifo[TX_FIFO_RP(asc)];
		asc->txfifo_rp++;
	} else {
		fprintf(stderr, "Bug in %s %s\n", __FILE__, __func__);
	}
	if (TX_FIFO_COUNT(asc) == 0) {
		asc->reg_STA |= STA_TE;
		SerialDevice_StopTx(asc->backend);
	}
	if (TX_FIFO_COUNT(asc) <= 8) {
		asc->reg_STA |= STA_THE;
	}
	if (TX_FIFO_ROOM(asc) > 0) {
		/* No longer full */
		asc->reg_STA &= ~STA_TF;
	}
	update_interrupts(asc);
	return true;
}

/*
 ************************************************************
 * Put one byte to the rxfifo
 ************************************************************
 */
static inline int
serial_put_rx_fifo(StAsc * asc, uint32_t c)
{
	int room = RX_FIFO_ROOM(asc);
	if (room < 1) {
		return -1;
	}
	asc->rx_fifo[RX_FIFO_WP(asc)] = c;
	asc->rxfifo_wp++;
	if (room == 1) {
		SerialDevice_StopRx(asc->backend);
		return 0;
	}
	return 1;
}

static void
serial_input(void *cd, UartChar c)
{
	StAsc *asc = cd;
	int fifocount;
	serial_put_rx_fifo(asc, c);
	fifocount = RX_FIFO_COUNT(asc);
	if (fifocount) {
		if (fifocount >= 8) {
			asc->reg_STA |= STA_RHF;
		}
		asc->reg_STA |= STA_RBF;
		update_interrupts(asc);
	}
}

static void
reset_rx_fifo(StAsc * asc)
{
	asc->rxfifo_rp = asc->rxfifo_wp = 0;
	asc->reg_STA &= ~(STA_RHF | STA_RBF);
	update_interrupts(asc);
}

static void
reset_tx_fifo(StAsc * asc)
{
	asc->txfifo_rp = asc->txfifo_wp = 0;
	asc->reg_STA |= STA_TE | STA_THE;
	asc->reg_STA &= ~(STA_TF);
	update_interrupts(asc);
}

static void
update_serconfig(StAsc * asc)
{
	UartCmd cmd;
	tcflag_t bits;
	tcflag_t parodd;
	tcflag_t parenb;
	tcflag_t crtscts;
	uint32_t mode;
	crtscts = 0;
	mode = asc->reg_CTL & CTL_MODE_MSK;
	if (asc->reg_CTL & CTL_PARITYODD) {
		parodd = 1;
	} else {
		parodd = 0;
	}
	if ((mode == CTL_MODE_7BIT_PAR) || (mode == CTL_MODE_8BIT_PAR)) {
		parenb = 1;
	} else {
		parenb = 0;
	}
	switch (mode) {
	    case CTL_MODE_7BIT_PAR:
		    bits = 7;
		    break;

	    case CTL_MODE_8BIT:
	    case CTL_MODE_8BIT_WKUP:
	    case CTL_MODE_8BIT_PAR:
		    bits = 8;
		    break;

	    case CTL_MODE_9BIT:
		    bits = 9;
		    break;
	    default:
		    bits = 8;
		    break;
	}
	if (asc->reg_CTL & CTL_CTSENABLE) {
		crtscts = 1;
	}
	if (crtscts) {
		cmd.opcode = UART_OPC_CRTSCTS;
		cmd.arg = 1;
		SerialDevice_Cmd(asc->backend, &cmd);
	} else {
		cmd.opcode = UART_OPC_CRTSCTS;
		cmd.arg = 0;
		SerialDevice_Cmd(asc->backend, &cmd);

		cmd.opcode = UART_OPC_SET_RTS;
		/* Set the initial state of RTS */
#if 0
		if (iuart->ucr2 & UCR2_CTS) {
			cmd.arg = UART_RTS_ACT;
		} else {
			cmd.arg = UART_RTS_INACT;
		}
		SerialDevice_Cmd(asc->backend, &cmd);
#endif
	}
	cmd.opcode = UART_OPC_PAREN;
	cmd.arg = parenb;
	SerialDevice_Cmd(asc->backend, &cmd);

	cmd.opcode = UART_OPC_PARODD;
	cmd.arg = parodd;
	SerialDevice_Cmd(asc->backend, &cmd);

	cmd.opcode = UART_OPC_SET_CSIZE;
	cmd.arg = bits;
	SerialDevice_Cmd(asc->backend, &cmd);

}

/* 
 **************************************************
 * Baudrate
 **************************************************
 */
static inline void
baudrate_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	StAsc *asc = (StAsc *) clientData;
	asc->reg_BAUDRATE = value;
	update_clock(asc);
}

static uint32_t
baudrate_read(void *clientData, uint32_t address, int rqlen)
{
	StAsc *asc = (StAsc *) clientData;
	return asc->reg_BAUDRATE;
}

/**
 ************************************************************
 * \fn dr_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
 * Write one character to the TX-Fifo
 ************************************************************
 */
static void
txbuf_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	StAsc *asc = (StAsc *) clientData;
	int room;
	uint8_t old_STA;
	if (TX_FIFO_ROOM(asc) > 0) {
		asc->tx_fifo[TX_FIFO_WP(asc)] = value & 0x1ff;
		asc->txfifo_wp++;
	}
	old_STA = asc->reg_STA;
	room = TX_FIFO_ROOM(asc);

	asc->reg_STA &= ~STA_TE;
	if (room < 8) {
		asc->reg_STA &= ~STA_THE;
	}
	if (room > 0) {
		asc->reg_STA &= ~STA_TF;
	} else {
		asc->reg_STA |= STA_TF;
	}
	if (asc->reg_STA != old_STA) {
		update_interrupts(asc);
	}
	SerialDevice_StartTx(asc->backend);
}

static uint32_t
txbuf_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "StAsc: Warning: Writing readonly register txbuf\n");
	return 0;
}

static void
rxbuf_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "StAsc: Warning: Writing readonly register rxbuf\n");
}

/*
 ***************************************************************
 * Fetch a byte from RX-Fifo
 ***************************************************************
 */
static uint32_t
rxbuf_read(void *clientData, uint32_t address, int rqlen)
{
	StAsc *asc = (StAsc *) clientData;
	uint32_t data = 0;
	if (RX_FIFO_COUNT(asc) > 0) {
		data = asc->rx_fifo[RX_FIFO_RP(asc)];
		asc->rxfifo_rp++;
	}
	if (RX_FIFO_COUNT(asc) == 0) {
		asc->reg_STA &= ~STA_RBF;
	}
	if (RX_FIFO_COUNT(asc) >= 8) {
		asc->reg_STA |= STA_RHF;
	}
	update_interrupts(asc);
	if (asc->reg_CTL & CTL_RXENABLE) {
		SerialDevice_StartRx(asc->backend);
	}
	return data;
}

/* 
 ***************************************************************************
 * Control Register
 ***************************************************************************
 */
static uint32_t
ctl_read(void *clientData, uint32_t address, int rqlen)
{
	StAsc *asc = (StAsc *) clientData;
	return asc->reg_CTL;
}

static void
ctl_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	StAsc *asc = (StAsc *) clientData;
	uint32_t diff = value ^ asc->reg_CTL;
	asc->reg_CTL = value;
	update_serconfig(asc);
	if (value & CTL_RXENABLE) {
		SerialDevice_StartRx(asc->backend);
	}
	if (diff & CTL_BAUDMODE) {
		update_clock(asc);
	}
	return;
}

static uint32_t
inten_read(void *clientData, uint32_t address, int rqlen)
{
	StAsc *asc = (StAsc *) clientData;
	return asc->reg_INTEN;
}

static void
inten_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	StAsc *asc = (StAsc *) clientData;
	asc->reg_INTEN = value;
	update_interrupts(asc);
	return;
}

/*
 */
static uint32_t
sta_read(void *clientData, uint32_t address, int rqlen)
{
	StAsc *asc = (StAsc *) clientData;
	Senseless_Report(150);
	return asc->reg_STA;
}

static void
sta_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	StAsc *asc = (StAsc *) clientData;
	asc->reg_STA = value;
}

static uint32_t
guardtime_read(void *clientData, uint32_t address, int rqlen)
{
	StAsc *asc = (StAsc *) clientData;
	return asc->reg_GUARDTIME;
}

static void
guardtime_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	StAsc *asc = (StAsc *) clientData;
	fprintf(stderr, "StaAsc Warning: Guardtime register not implemented\n");
	asc->reg_GUARDTIME = value & 0x1ff;
}

static uint32_t
timeout_read(void *clientData, uint32_t address, int rqlen)
{
	StAsc *asc = (StAsc *) clientData;
	return asc->reg_TIMEOUT;
}

static void
timeout_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	StAsc *asc = (StAsc *) clientData;
	fprintf(stderr, "StaAsc Warning: timeout register not implemented\n");
	asc->reg_TIMEOUT = value & 0x1ff;
}

static void
txreset_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	StAsc *asc = (StAsc *) clientData;
	reset_tx_fifo(asc);
}

static void
rxreset_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	StAsc *asc = (StAsc *) clientData;
	reset_rx_fifo(asc);
}

static void
retries_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	StAsc *asc = (StAsc *) clientData;
	asc->reg_RETRIES = value & 0xff;
}

static uint32_t
retries_read(void *clientData, uint32_t address, int rqlen)
{
	StAsc *asc = (StAsc *) clientData;
	return asc->reg_RETRIES;
}

static void
StAsc_Map(void *owner, uint32_t base, uint32_t mask, uint32_t flags)
{
	StAsc *asc = (StAsc *) owner;
	IOH_New32(ASC_BAUDRATE(base), baudrate_read, baudrate_write, asc);
	IOH_New32(ASC_TXBUF(base), txbuf_read, txbuf_write, asc);
	IOH_New32(ASC_RXBUF(base), rxbuf_read, rxbuf_write, asc);
	IOH_New32(ASC_CTL(base), ctl_read, ctl_write, asc);
	IOH_New32(ASC_INTEN(base), inten_read, inten_write, asc);
	IOH_New32(ASC_STA(base), sta_read, sta_write, asc);
	IOH_New32(ASC_GUARDTIME(base), guardtime_read, guardtime_write, asc);
	IOH_New32(ASC_TIMEOUT(base), timeout_read, timeout_write, asc);
	IOH_New32(ASC_TXRESET(base), NULL, txreset_write, asc);
	IOH_New32(ASC_RXRESET(base), NULL, rxreset_write, asc);
	IOH_New32(ASC_RETRIES(base), retries_read, retries_write, asc);

}

static void
StAsc_UnMap(void *owner, uint32_t base, uint32_t mask)
{
	IOH_Delete32(ASC_BAUDRATE(base));
	IOH_Delete32(ASC_TXBUF(base));
	IOH_Delete32(ASC_RXBUF(base));
	IOH_Delete32(ASC_CTL(base));
	IOH_Delete32(ASC_INTEN(base));
	IOH_Delete32(ASC_STA(base));
	IOH_Delete32(ASC_GUARDTIME(base));
	IOH_Delete32(ASC_TIMEOUT(base));
	IOH_Delete32(ASC_TXRESET(base));
	IOH_Delete32(ASC_RXRESET(base));
	IOH_Delete32(ASC_RETRIES(base));
}

/*
 **********************************************************
 * StAsc_New
 * 	Create a new ST asynchronous serial controller. 
 **********************************************************
 */
BusDevice *
StAsc_New(const char *devname)
{
	StAsc *asc = (StAsc *) sg_new(StAsc);
	asc->bdev.first_mapping = NULL;
	asc->bdev.Map = StAsc_Map;
	asc->bdev.UnMap = StAsc_UnMap;
	asc->bdev.owner = asc;
	asc->bdev.hw_flags = MEM_FLAG_WRITABLE | MEM_FLAG_READABLE;
	asc->backend = Uart_New(devname, serial_input, serial_output, NULL, asc);
	asc->rx_fifo_size = asc->tx_fifo_size = 1;
	asc->clk_in = Clock_New("%s.clk", devname);
	asc->clk_baud = Clock_New("%s.baud_clk", devname);
	if (!asc->clk_in || !asc->clk_baud) {
		fprintf(stderr, "Can not create baud clocks for %s\n", devname);
		exit(1);
	}
	asc->sigIrq = SigNode_New("%s.irq", devname);
	if (!asc->sigIrq) {
		fprintf(stderr, "Can not create interrupt signal for %s\n", devname);
	}
	SigNode_Set(asc->sigIrq, SIG_HIGH);	/* No request on startup */
	asc->interrupt_posted = 0;
	Clock_Trace(asc->clk_baud, baud_clock_trace, asc);
	fprintf(stderr, "Created ST Asynchronous serial controller (ASC) \"%s\"\n", devname);
	return &asc->bdev;
}
