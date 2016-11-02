/*
 *************************************************************************************************
 *
 * Emulation of the NS9750 Serial Interfaces 
 *
 *  State: working in non DMA mode. Timing of Modem status signals 
 *  	   not correct.
 *
 * Copyright 2004 2013 Jochen Karrer. All rights reserved.
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

#include <string.h>
#include <fcntl.h>
#include <stdint.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <termios.h>
#include <sys/ioctl.h>
#include "bus.h"
#include "cycletimer.h"
#include "ns9750_serial.h"
#include "ns9750_timer.h"
#include "ns9750_bbus.h"
#include "ns9750_bbdma.h"
#include "fio.h"
#include "configfile.h"
#include "signode.h"
#include "sgstring.h"
#include "serial.h"
#include "bus.h"

#if 0
#define dbgprintf(x...) { fprintf(stderr,x); }
#else
#define dbgprintf(x...)
#endif

#define RX_FIFO_SIZE (32)
#define RX_FIFO_MASK (RX_FIFO_SIZE-1)
#define RX_FIFO_COUNT(ser) (((RX_FIFO_SIZE + (ser)->rxfifo_wp - (ser)->rxfifo_rp)) & RX_FIFO_MASK)
#define RX_FIFO_ROOM(ser) (RX_FIFO_SIZE - RX_FIFO_COUNT(ser) - 1)

#define TX_FIFO_SIZE (32)
#define TX_FIFO_MASK (TX_FIFO_SIZE-1)
#define TX_FIFO_COUNT(ser) ((((ser)->txfifo_wp-(ser)->txfifo_rp)+TX_FIFO_SIZE)&TX_FIFO_MASK)
#define TX_FIFO_ROOM(ser) (TX_FIFO_SIZE-TX_FIFO_COUNT(ser)-1)

typedef struct Serial {
	BusDevice bdev;
	char *name;
	uint32_t txcount;
	/* registers */
	uint32_t ctrlA;
	uint32_t ctrlB;
	uint32_t statA;
	uint32_t rate;
	uint32_t fifo;
	uint32_t rxbuf_gap;
	uint32_t rxchar_gap;
	uint32_t rxmatch;
	uint32_t rxmatch_mask;
	uint32_t flow_ctrl;
	uint32_t flow_force;
	uint8_t rx_fifo[RX_FIFO_SIZE];
	int rxfifo_wp;
	int rxfifo_rp;

	uint8_t tx_fifo[TX_FIFO_SIZE];
	unsigned int txfifo_wp;
	unsigned int txfifo_rp;
	SigNode *RxDmaGnt;
	SigNode *TxDmaReq;
	int tx_dma_request;	// local copy 

	int rx_interrupt_posted;
	int tx_interrupt_posted;

	UartPort *backend;

	BBusDMA_Channel *rx_dmachan;
	BBusDMA_Channel *tx_dmachan;
	SigNode *endianNode;
	SigTrace *endianTrace;
	int endian;
	SigNode *sigRxIrq;
	SigNode *sigTxIrq;
} Serial;

struct SerialInfo {
	char *name;
	uint32_t base;
	int rxdma_chan;
	int txdma_chan;
};

#if 0
static struct SerialInfo serials[] = {
	{
 name:	 "serialA",
 base:	 SER_BASE_A,
 rxdma_chan:BBDMA_CHAN_1,
 txdma_chan:BBDMA_CHAN_2},
	{
 name:	 "serialB",
 base:	 SER_BASE_B,
 rxdma_chan:BBDMA_CHAN_3,
 txdma_chan:BBDMA_CHAN_4},
	{
 name:	 "serialC",
 base:	 SER_BASE_C,
 rxdma_chan:BBDMA_CHAN_5,
 txdma_chan:BBDMA_CHAN_6},
	{
 name:	 "serialD",
 base:	 SER_BASE_D,
 rxdma_chan:BBDMA_CHAN_7,
 txdma_chan:BBDMA_CHAN_8},
};
#endif

/*
 * -------------------------------------------------------
 * change_endian
 * 	Invoked when the Endian Signal Line changes
 * -------------------------------------------------------
 */
static void
change_endian(SigNode * node, int value, void *clientData)
{
	Serial *ser = clientData;
	if (value == SIG_HIGH) {
		fprintf(stderr, "Serial now big endian\n");
		ser->endian = en_BIG_ENDIAN;
	} else if (value == SIG_LOW) {
		ser->endian = en_LITTLE_ENDIAN;
	} else {
		fprintf(stderr, "NS9750 Serial: Endian is neither Little nor Big\n");
		exit(3424);
	}
}

static void
update_rx_interrupt(Serial * ser)
{
	uint32_t rints = SER_RRDY | SER_RHALF | SER_RBC | SER_DCDI | SER_RII | SER_DSRI;
	if (ser->statA & ser->ctrlA & rints) {
		if (!ser->rx_interrupt_posted) {
			SigNode_Set(ser->sigRxIrq, SIG_LOW);
			ser->rx_interrupt_posted = 1;
		}
	} else {
		if (ser->rx_interrupt_posted) {
			SigNode_Set(ser->sigRxIrq, SIG_PULLUP);
			ser->rx_interrupt_posted = 0;
		}
	}
}

static void
update_tx_interrupt(Serial * ser)
{
	uint32_t tints = SER_CTSI | SER_TRDY | SER_THALF | SER_TEMPTY;
	if (ser->statA & ser->ctrlA & tints) {
		if (!ser->tx_interrupt_posted) {
			SigNode_Set(ser->sigTxIrq, SIG_LOW);
			ser->tx_interrupt_posted = 1;
		}
	} else {
		if (ser->tx_interrupt_posted) {
			ser->tx_interrupt_posted = 0;
			SigNode_Set(ser->sigTxIrq, SIG_PULLUP);
		}
	}
}

/*
 * ----------------------------------------------------
 * Set TX-DMA request when there is room in tx-fifo
 * and clear when tx-fifo is full
 * ----------------------------------------------------
 */
static inline void
set_txdma_request(Serial * ser)
{
	if (!ser->tx_dma_request) {
		SigNode_Set(ser->TxDmaReq, SIG_HIGH);
		ser->tx_dma_request = 1;
	}
}

static inline void
clear_txdma_request(Serial * ser)
{
	if (ser->tx_dma_request) {
		SigNode_Set(ser->TxDmaReq, SIG_LOW);
		ser->tx_dma_request = 0;
	}
}

static inline void
update_txdma_request(Serial * ser)
{
	int room = TX_FIFO_ROOM(ser);
	if ((room >= 4) && (ser->ctrlA & SER_ETXDMA)) {
		set_txdma_request(ser);
	} else {
		clear_txdma_request(ser);
	}
}

/*
 * --------------------------------------------------------
 * Filehandler for TX-Fifo
 * 	Write chars from TX-Fifo to file as long
 *	as TX-Fifo is not empty and write would not block
 * --------------------------------------------------------
 */
static bool
serial_output(void *cd, UartChar * c)
{
	Serial *ser = cd;
	int room;
	if (ser->txfifo_rp != ser->txfifo_wp) {
		*c = ser->tx_fifo[ser->txfifo_rp];
		ser->txfifo_rp = (ser->txfifo_rp + 1) % TX_FIFO_SIZE;
	} else {
		fprintf(stderr, "Bug in %s %s\n", __FILE__, __func__);
	}
	room = TX_FIFO_ROOM(ser);
	if (room >= (TX_FIFO_SIZE / 2)) {
		if (!(ser->statA & SER_THALF)) {
			ser->statA |= SER_THALF | SER_TRDY;
		}
		update_tx_interrupt(ser);
		if (ser->ctrlA & SER_ETXDMA) {
			set_txdma_request(ser);
		}
	} else if (room >= 4) {
		if (!(ser->statA & SER_TRDY)) {
			ser->statA |= SER_TRDY;
			update_tx_interrupt(ser);
		}
		if (ser->ctrlA & SER_ETXDMA) {
			set_txdma_request(ser);
		}
	}
	if (TX_FIFO_COUNT(ser) == 0) {
		SerialDevice_StopTx(ser->backend);
		ser->statA |= SER_TEMPTY;
	}
	return true;
}

/*
 * --------------------------------------------
 * Write one character to the TX-Fifo
 * --------------------------------------------
 */
static void
serial_tx_fifo_put(Serial * ser, uint8_t c)
{
	int room;
	ser->tx_fifo[ser->txfifo_wp] = c;
	ser->txfifo_wp = (ser->txfifo_wp + 1) % TX_FIFO_SIZE;
	ser->statA &= ~SER_TEMPTY;
	room = TX_FIFO_ROOM(ser);
	if ((room < (TX_FIFO_SIZE / 2)) && (ser->statA & SER_THALF)) {
		ser->statA &= ~SER_THALF;
		update_tx_interrupt(ser);
	}
	if ((room < 4)) {
		if (ser->statA & SER_TRDY) {
			ser->statA = ser->statA & ~(SER_TRDY | SER_THALF);
			update_tx_interrupt(ser);
		}
		clear_txdma_request(ser);
	}
	SerialDevice_StartTx(ser->backend);
}

/*
 * ---------------------------------------------------------------
 * serial_txdma_sink
 * 	This is the callback handler invoked by the
 *	DMA Controller when data is available and TX-DMA request 
 *	is set. It returns the number of bytes written to
 *	the output fifo
 * ---------------------------------------------------------------
 */
#if 0
static int
serial_txdma_sink(BBusDMA_Channel * chan, uint8_t * buf, int len, void *clientData)
{
	Serial *ser = clientData;
	int count;
	for (count = 0; count < len; count++) {
		if (!ser->tx_dma_request) {
			break;
		}
		serial_tx_fifo_put(ser, buf[count]);
		count++;
	}
	return count;
}
#endif
/*
 * --------------------------------------------------------------------
 * Update terminal settings whenever a register is changed which
 * affects speed or parameters 
 * --------------------------------------------------------------------
 */
static void
update_serconfig(Serial * ser)
{
	int rx_baudrate;
	int tx_baudrate;
	int clksource;
	uint32_t clk_rate = 0;
	int rdiv = 1, tdiv = 1;
	int N, divisor;
	tcflag_t bits;
	tcflag_t parodd;
	tcflag_t parenb;
	tcflag_t crtscts;
	UartCmd cmd;

	if (ser->ctrlA & SER_CTSTX) {
		crtscts = 1;
	} else {
		crtscts = 0;
	}
	cmd.opcode = UART_OPC_CRTSCTS;
	cmd.arg = crtscts;
	SerialDevice_Cmd(ser->backend, &cmd);

	if (ser->ctrlA & SER_EPS) {
		parodd = 0;
	} else {
		parodd = 1;
	}
	cmd.opcode = UART_OPC_PARODD;
	cmd.arg = parodd;
	SerialDevice_Cmd(ser->backend, &cmd);

	if (ser->ctrlA & SER_PE) {
		parenb = 1;
	} else {
		parenb = 0;
	}
	cmd.opcode = UART_OPC_PAREN;
	cmd.arg = parenb;
	SerialDevice_Cmd(ser->backend, &cmd);

	switch (ser->ctrlA & SER_WLS_MASK) {
	    case SER_WLS_5:
		    bits = 5;
		    break;
	    case SER_WLS_6:
		    bits = 6;
		    break;
	    case SER_WLS_7:
		    bits = 7;
		    break;
	    case SER_WLS_8:
		    bits = 8;
		    break;
		    /* Can not be reached */
	    default:
		    bits = 8;
		    break;
	}
	cmd.opcode = UART_OPC_SET_CSIZE;
	cmd.arg = bits;
	SerialDevice_Cmd(ser->backend, &cmd);

	clksource = (ser->rate & SER_CLKMUX_MASK) >> SER_CLKMUX_SHIFT;
	switch (clksource) {
	    case 0:
		    // doesn't work in revision 0 of CPU
		    break;

	    case 1:		/* Source is BCLK */
		    clk_rate = CycleTimerRate_Get() / 2;
		    break;

	    case 2:
		    // not implemented
	    case 3:
		    // not implemented
		    break;
	}
	if (!clk_rate) {
		return;
	}
	switch (ser->rate & SER_RDCR_MASK) {
	    case SER_RDCR_1X:
		    rdiv = 1;
		    break;
	    case SER_RDCR_8X:
		    rdiv = 8;
		    break;
	    case SER_RDCR_16X:
		    rdiv = 16;
		    break;
	    case SER_RDCR_32X:
		    rdiv = 32;
		    break;
	}
	switch (ser->rate & SER_TDCR_MASK) {
	    case SER_TDCR_1X:
		    tdiv = 1;
		    break;
	    case SER_TDCR_8X:
		    tdiv = 8;
		    break;
	    case SER_TDCR_16X:
		    tdiv = 16;
		    break;
	    case SER_TDCR_32X:
		    tdiv = 32;
		    break;
	}
	N = ser->rate & SER_N_MASK;
	divisor = N + 1;
	rx_baudrate = clk_rate / (rdiv * divisor);
	tx_baudrate = clk_rate / (tdiv * divisor);
	if (rx_baudrate != tx_baudrate) {
		fprintf(stderr, "Rx baudrate %d Tx baudrate %d not the same !\n", rx_baudrate,
			tx_baudrate);
	} else {
//              fprintf(stderr,"Baudrate %d\n",rx_baudrate);
	}
	cmd.opcode = UART_OPC_SET_BAUDRATE;
	cmd.arg = rx_baudrate;
	SerialDevice_Cmd(ser->backend, &cmd);
	dbgprintf("NS9750 %s: baudrate %d\n", ser->name, rx_baudrate);
	return;
}

static void
ser_ctrla_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	Serial *ser = clientData;
	UartCmd cmd;
	uint32_t diff = value ^ ser->ctrlA;
	if ((value & SER_CE) && !(ser->ctrlA & SER_CE)) {
		ser->rxfifo_rp = ser->rxfifo_wp = 0;
		ser->statA = SER_TRDY | SER_THALF | SER_CTS;
	}
	if (!(value & SER_CE)) {
		SerialDevice_StopRx(ser->backend);
		SerialDevice_StopTx(ser->backend);
		return;
	}
	ser->ctrlA = value;
	if (diff & (SER_PE | SER_STP | SER_WLS_MASK | SER_CTSTX | SER_RTSRX)) {
		update_serconfig(ser);
	}
	if (diff & (SER_DTR | SER_RTS)) {
		cmd.opcode = UART_OPC_SET_RTS;
		if (value & SER_DTR) {
			cmd.arg = 1;
		} else {
			cmd.arg = 0;
		}
		SerialDevice_Cmd(ser->backend, &cmd);

		cmd.opcode = UART_OPC_SET_RTS;
		if (value & SER_RTS) {
			cmd.arg = UART_RTS_ACT;
		} else {
			cmd.arg = UART_RTS_INACT;
		}
		SerialDevice_Cmd(ser->backend, &cmd);
	}
	if (value & SER_ETXDMA) {
		fprintf(stderr, "NS9750 %s: TX-DMA enabled\n", ser->name);
	}
	if (value & SER_ERXDMA) {
		fprintf(stderr, "NS9750 Serial: RX-DMA mode not implemented\n");
	}
	if (diff & SER_ETXDMA) {
		update_txdma_request(ser);
	}
	SerialDevice_StartRx(ser->backend);
	update_tx_interrupt(ser);
	dbgprintf("New config %08x\n", value);
	return;
}

static uint32_t
ser_ctrla_read(void *clientData, uint32_t address, int rqlen)
{
	Serial *ser = clientData;
	return ser->ctrlA;
}

static void
ser_ctrlb_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	Serial *ser = clientData;
	uint32_t diff = value ^ ser->ctrlB;
	ser->ctrlB = value;
	if (diff & (SER_RTSTX)) {
		update_serconfig(ser);
	}
	return;
}

static uint32_t
ser_ctrlb_read(void *clientData, uint32_t address, int rqlen)
{
	Serial *ser = clientData;
	return ser->ctrlB;
}

static uint32_t
ser_rate_read(void *clientData, uint32_t address, int rqlen)
{
	Serial *ser = clientData;
	return ser->rate;
}

static void
ser_rate_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	Serial *ser = clientData;
	ser->rate = value;
	update_serconfig(ser);
	//fprintf(stderr,"Ser rate reg %08x\n",value);
	return;
}

static uint32_t
ser_rxfifo_read(void *clientData, uint32_t address, int rqlen)
{
	Serial *ser = clientData;
	int bytes;
	int i;
	int fifocount;
	unsigned long data = 0;
	//fprintf(stderr,"Read from Rx-Fifo\n");
	if (!(ser->statA & SER_RRDY)) {
		fprintf(stderr, "Reading from empty RX-fifo\n");
		update_rx_interrupt(ser);
		return 0;
	}
	bytes = (ser->statA & SER_RXFDB) >> SER_RXFDB_SHIFT;
	if (!bytes) {
		bytes = 4;
	}
	//fprintf(stderr,"bytes %d, fifobytes %d stata %08x\n",bytes,ser->rxfifo_bytes,ser->statA);
	if (bytes > RX_FIFO_COUNT(ser)) {
		update_rx_interrupt(ser);
		fprintf(stderr, "Emulator Bug in line %d file %s\n", __LINE__, __FILE__);
		return 0;
	}
	for (i = 0; i < bytes; i++) {
		if (ser->endian == en_LITTLE_ENDIAN) {
			data = data | (ser->rx_fifo[ser->rxfifo_rp] << (8 * i));
		} else if (ser->endian == en_BIG_ENDIAN) {
			data = data | (ser->rx_fifo[ser->rxfifo_rp] << (8 * (3 - i)));
		}
		ser->rxfifo_rp = (ser->rxfifo_rp + 1) & RX_FIFO_MASK;
	}

	fifocount = RX_FIFO_COUNT(ser);
	if (fifocount >= 4) {
		ser->statA = (ser->statA & ~(SER_RBC | SER_RXFDB)) | SER_RRDY;
	} else if (fifocount > 0) {
		ser->statA = (ser->statA & ~(SER_RRDY | SER_RXFDB)) | SER_RBC;
	} else {
		ser->statA = ser->statA & ~(SER_RXFDB | SER_RRDY | SER_RBC);
	}
	if (fifocount < 20) {
		ser->statA = ser->statA & ~(SER_RHALF);
	}
	if (RX_FIFO_ROOM(ser) < 4) {
		ser->statA = ser->statA & ~(SER_RFS);
	}
	SerialDevice_StartRx(ser->backend);
	update_rx_interrupt(ser);
	return data;
}

static void
ser_txfifo_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	int i;
	Serial *ser = clientData;
	for (i = 0; i < rqlen; i++) {
		if (ser->endian == en_LITTLE_ENDIAN) {
			serial_tx_fifo_put(ser, (value >> (i << 3)) & 0xff);
		} else if (ser->endian == en_BIG_ENDIAN) {
			serial_tx_fifo_put(ser, (value >> ((rqlen - 1 - i) << 3)) & 0xff);
		}
	}
	return;
}

static uint32_t
ser_stata_read(void *clientData, uint32_t address, int rqlen)
{
	Serial *ser = clientData;
	uint32_t stata = ser->statA;
	uint32_t diff;
	uint32_t dcd, ri, dsr, cts;
	UartCmd cmd;
	if (stata & SER_RRDY) {
		int bytes = RX_FIFO_COUNT(ser);
		if (bytes >= 4) {
			bytes = 4;
		}
		if (!bytes) {
			stata = stata & ~SER_RRDY;
		} else {
			stata = (stata & ~SER_RXFDB) | ((bytes & 3) << SER_RXFDB_SHIFT);
		}
	}
	//fprintf(stderr,"stata read RRDY %08x\n",stata);
	cmd.opcode = UART_OPC_GET_DCD;
	SerialDevice_Cmd(ser->backend, &cmd);
	dcd = cmd.retval;

	cmd.opcode = UART_OPC_GET_RI;
	SerialDevice_Cmd(ser->backend, &cmd);
	ri = cmd.retval;

	cmd.opcode = UART_OPC_GET_DSR;
	SerialDevice_Cmd(ser->backend, &cmd);
	dsr = cmd.retval;

	cmd.opcode = UART_OPC_GET_CTS;
	SerialDevice_Cmd(ser->backend, &cmd);
	cts = cmd.retval;

	stata = stata & ~(SER_DCD | SER_RI | SER_DSR | SER_CTS);
	if (dcd) {
		stata |= SER_DCD;
	}
	if (ri) {
		stata |= SER_RI;
	}
	if (dsr) {
		stata |= SER_DSR;
	}
	if (cts) {
		stata |= SER_CTS;
	}
	diff = stata ^ ser->statA;
	stata = stata | ((diff & (SER_DCD | SER_RI | SER_DSR | SER_CTS)) >> 12);
	ser->statA = stata;
	return ser->statA;
}

/* 
 * -----------------------------------------
 * acknowledging RBC sets RRDY
 * -----------------------------------------
 */
static void
ser_stata_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	Serial *ser = clientData;
	uint32_t clear_ints;
	if (ser->statA & SER_RBC) {
		if (value & SER_RBC) {
			ser->statA = (ser->statA & ~SER_RBC) | SER_RRDY;
		}
	}
	clear_ints = value & (SER_DCDI | SER_RII | SER_DSRI | SER_CTSI);
	ser->statA ^= clear_ints;
	return;
}

/*
 * ------------------------------------
 * Put one byte to the rxfifo
 * ------------------------------------
 */
static inline int
serial_rx_char(Serial * ser, uint8_t c)
{
	int room = RX_FIFO_ROOM(ser);
	if (room < 1) {
		return -1;
	}
	ser->rx_fifo[ser->rxfifo_wp] = c;
	ser->rxfifo_wp = (ser->rxfifo_wp + 1) & RX_FIFO_MASK;
	if (room == 1) {
		SerialDevice_StopRx(ser->backend);
		return 0;
	}
	return 1;
}

static void
serial_input(void *cd, UartChar c)
{
	Serial *ser = cd;
	int fifocount;
	serial_rx_char(ser, c);
	if (!(ser->ctrlA & SER_CE)) {
		fprintf(stderr, "Serial line not yet enabled\n");
		return;
	}
	fifocount = RX_FIFO_COUNT(ser);
	if (fifocount) {
		if (fifocount >= 4) {
			ser->statA = (ser->statA & ~SER_RBC) | SER_RRDY;
		} else {
			ser->statA |= SER_RBC;
		}
		if (fifocount > 20) {
			ser->statA |= SER_RHALF;
		}
		if (RX_FIFO_ROOM(ser) < 4) {
			ser->statA |= SER_RFS;
		}
		update_rx_interrupt(ser);
	}
	return;
}

static void
ser_rxbuf_gap_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	Serial *ser = clientData;
	ser->rxbuf_gap = value;
	if (value & 0x7fff0000) {
		fprintf(stderr, "Serial: Illegal value in rxbuf_gap: %08x\n", value);
	}
}

static uint32_t
ser_rxbuf_gap_read(void *clientData, uint32_t address, int rqlen)
{
	Serial *ser = clientData;
	return ser->rxbuf_gap;
}

static void
ser_rxchar_gap_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	Serial *ser = clientData;
	ser->rxchar_gap = value;
	if (value & 0x7ff00000) {
		fprintf(stderr, "Serial: Illegal value in rxchar_gap: %08x\n", value);
	}

}

static uint32_t
ser_rxchar_gap_read(void *clientData, uint32_t address, int rqlen)
{
	Serial *ser = clientData;
	return ser->rxchar_gap;
}

static void
ser_rxmatch_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	Serial *ser = clientData;
	ser->rxmatch = value;
	fprintf(stderr, "Serial: Warning, RX-Match emulation not implemented: value 0x%08x\n",
		value);

}

static uint32_t
ser_rxmatch_read(void *clientData, uint32_t address, int rqlen)
{
	Serial *ser = clientData;
	return ser->rxmatch;

}

static void
ser_rxmatch_mask_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	Serial *ser = clientData;
	ser->rxmatch_mask = value;
	fprintf(stderr, "Serial: Warning, RX-Match emulation not implemented\n");

}

static uint32_t
ser_rxmatch_mask_read(void *clientData, uint32_t address, int rqlen)
{
	Serial *ser = clientData;
	return ser->rxmatch_mask;
}

static void
ser_flow_ctrl_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	Serial *ser = clientData;
	ser->flow_ctrl = value;
	fprintf(stderr, "Serial: Warning, Flow Control register not implemented\n");
	return;
}

static uint32_t
ser_flow_ctrl_read(void *clientData, uint32_t address, int rqlen)
{
	Serial *ser = clientData;
	return ser->flow_ctrl;

}

static void
ser_flow_force_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{

	Serial *ser = clientData;
	ser->flow_force = value;
	fprintf(stderr, "Serial: Warning, Flow Force register not implemented\n");
}

static uint32_t
ser_flow_force_read(void *clientData, uint32_t address, int rqlen)
{
	Serial *ser = clientData;
	return ser->flow_force;
}

static void
NS9750Serial_Map(void *owner, uint32_t base, uint32_t mask, uint32_t flags)
{
	Serial *ser = owner;
	IOH_New32(base + SER_CTRLA, ser_ctrla_read, ser_ctrla_write, ser);
	IOH_New32(base + SER_CTRLB, ser_ctrlb_read, ser_ctrlb_write, ser);
	IOH_New32(base + SER_STATA, ser_stata_read, ser_stata_write, ser);
	IOH_New32(base + SER_RATE, ser_rate_read, ser_rate_write, ser);
	IOH_New32f(base + SER_FIFO, ser_rxfifo_read, ser_txfifo_write, ser,
		   IOH_FLG_PA_CBSE | IOH_FLG_HOST_ENDIAN);
	IOH_New32(base + SER_RXBUF_GAP, ser_rxbuf_gap_read, ser_rxbuf_gap_write, ser);
	IOH_New32(base + SER_RXCHAR_GAP, ser_rxchar_gap_read, ser_rxchar_gap_write, ser);
	IOH_New32(base + SER_RXMATCH, ser_rxmatch_read, ser_rxmatch_write, ser);
	IOH_New32(base + SER_RXMATCH_MASK, ser_rxmatch_mask_read, ser_rxmatch_mask_write, ser);
	IOH_New32(base + SER_FLOW_CTRL, ser_flow_ctrl_read, ser_flow_ctrl_write, ser);
	IOH_New32(base + SER_FLOW_FORCE, ser_flow_force_read, ser_flow_force_write, ser);
}

static void
NS9750Serial_UnMap(void *owner, uint32_t base, uint32_t mask)
{
	IOH_Delete32(base + SER_CTRLA);
	IOH_Delete32(base + SER_CTRLB);
	IOH_Delete32(base + SER_STATA);
	IOH_Delete32(base + SER_RATE);
	IOH_Delete32(base + SER_FIFO);
	IOH_Delete32(base + SER_RXBUF_GAP);
	IOH_Delete32(base + SER_RXCHAR_GAP);
	IOH_Delete32(base + SER_RXMATCH);
	IOH_Delete32(base + SER_RXMATCH_MASK);
	IOH_Delete32(base + SER_FLOW_CTRL);
	IOH_Delete32(base + SER_FLOW_FORCE);
}

BusDevice *
NS9750Serial_New(const char *serial_name, BBusDMACtrl * bbdma)
{
	Serial *ser = sg_new(Serial);
#if 0
	ser->rx_dmachan = BBDMA_Connect(bbdma, rxdma_chan);
	ser->tx_dmachan = BBDMA_Connect(bbdma, txdma_chan);
#endif
	ser->name = sg_strdup(serial_name);

	ser->RxDmaGnt = SigNode_New("%s.RxDmaGnt", serial_name);
	if (!ser->RxDmaGnt) {
		fprintf(stderr, "Can not create Ser. RxDmaGntNode\n");
		exit(1);
	}
	SigNode_Set(ser->RxDmaGnt, SIG_PULLUP);	/* controller sets RxDMAGnt  */

	ser->TxDmaReq = SigNode_New("%s.TxDmaReq", serial_name);
	if (!ser->TxDmaReq) {
		fprintf(stderr, "Can not create Ser. TxDmaReqNode\n");
		exit(1);
	}
	SigNode_Set(ser->TxDmaReq, SIG_LOW);	/* no request from me */

	ser->sigRxIrq = SigNode_New("%s.rx_irq", serial_name);
	ser->sigTxIrq = SigNode_New("%s.tx_irq", serial_name);
	if (!ser->sigRxIrq || !ser->sigTxIrq) {
		fprintf(stderr, "Can not create Ser. Interrupt signal\n");
		exit(1);
	}
	SigNode_Set(ser->sigTxIrq, SIG_PULLUP);
	SigNode_Set(ser->sigRxIrq, SIG_PULLUP);

	ser->endianNode = SigNode_New("%s.endian", serial_name);
	if (!ser->endianNode) {
		fprintf(stderr, "Can not create Ser. EndianNode\n");
		exit(1);
	}
	ser->endianTrace = SigNode_Trace(ser->endianNode, change_endian, ser);

	ser->backend = Uart_New(serial_name, serial_input, serial_output, NULL, ser);

	//BBDMA_SetDataSink(ser->tx_dmachan,serial_txdma_sink,ser);

	ser->statA = SER_TRDY | SER_CTS;

	ser->bdev.first_mapping = NULL;
	ser->bdev.Map = NS9750Serial_Map;
	ser->bdev.UnMap = NS9750Serial_UnMap;
	ser->bdev.owner = ser;
	ser->bdev.hw_flags = MEM_FLAG_WRITABLE | MEM_FLAG_READABLE;
	return &ser->bdev;
}
