/*
 *************************************************************************************************
 * Emulation of Freescale iMX21 UART
 *
 * state: working 
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

#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/fcntl.h>
#include "bus.h"
#include "fio.h"
#include "signode.h"
#include "clock.h"
#include "imx21_uart.h"
#include "serial.h"
#include "configfile.h"
#include "senseless.h"
#include "cycletimer.h"
#include "sgstring.h"

#define URR(base)	((base) + 0x0)
#define		URR_CHARRDY	(1<<15)
#define		URR_ERR		(1<<14)
#define		URR_OVRRUN	(1<<13)
#define		URR_FRMERR	(1<<12)
#define 	URR_BRK		(1<<11)
#define		URR_PRERR	(1<<10)
#define		URR_DATA_MASK	(0xff)

#define UTR(base)	((base) + 0x40)
#define UCR1(base)	((base) + 0x80)
#define		UCR1_ADEN	(1<<15)
#define		UCR1_ADBR	(1<<14)
#define		UCR1_TRDYEN	(1<<13)
#define		UCR1_IDEN	(1<<12)
#define		UCR1_ICD_MASK	(3<<10)
#define		UCR1_ICD_SHIFT	(10)
#define		UCR1_RRDYEN	(1<<9)
#define		UCR1_RXDMAEN	(1<<8)
#define		UCR1_IREN	(1<<7)
#define		UCR1_TXMPTYEN	(1<<6)
#define		UCR1_RTSDEN	(1<<5)
#define		UCR1_SNDBRK	(1<<4)
#define		UCR1_TXDMAEN	(1<<3)
#define		UCR1_DOZE	(1<<2)
#define		UCR1_UARTEN	(1<<1)
#define UCR2(base)	((base) + 0x84)
#define		UCR2_ESCI	(1<<15)
#define		UCR2_IRTS	(1<<14)
#define		UCR2_CTSC	(1<<13)
#define		UCR2_CTS	(1<<12)
#define		UCR2_ESCEN	(1<<11)
#define		UCR2_RTEC_MASK	(3<<9)
#define		UCR2_RTEC_SHIFT (9)
#define		UCR2_PREN	(1<<8)
#define		UCR2_PROE	(1<<7)
#define		UCR2_STPB	(1<<6)
#define		UCR2_WS		(1<<5)
#define		UCR2_RTSEN	(1<<4)
#define		UCR2_ATEN	(1<<3)
#define		UCR2_TXEN	(1<<2)
#define		UCR2_RXEN	(1<<1)
#define		UCR2_SRST	(1<<0)

#define UCR3(base)	((base) + 0x88)
#define		UCR3_DPEC_MASK	(3<<14)
#define 	UCR3_DPEC_SHIFT	(14)
#define 	UCR3_DTREN	(1<<13)
#define		UCR3_PARERREN	(1<<12)
#define		UCR3_FRAERREN	(1<<11)
#define		UCR3_DSR	(1<<10)
#define		UCR3_DCD	(1<<9)
#define		UCR3_RI		(1<<8)
#define		UCR3_ADNIMP	(1<<7)
#define		UCR3_RXDSEN	(1<<6)
#define		UCR3_AIRINTEN	(1<<5)
#define		UCR3_AWAKEN	(1<<4)
#define		UCR3_RXDMUXSEL	(1<<2)
#define		UCR3_INVT	(1<<1)
#define		UCR3_ACIEN	(1<<0)

#define UCR4(base)	((base) + 0x8c)
#define		UCR4_CTSTL_MASK		(0x3f << 10)
#define		UCR4_CTSTL_SHIFT 	(10)
#define 	UCR4_INVR		(1<<9)
#define		UCR4_ENIRI		(1<<8)
#define		UCR4_WKEN		(1<<7)
#define		UCR4_IRSC		(1<<5)
#define		UCR4_LPBYP		(1<<4)
#define		UCR4_TCEN		(1<<3)
#define		UCR4_BKEN		(1<<2)
#define		UCR4_OREN		(1<<1)
#define		UCR4_DREN		(1<<0)

#define UFCR(base)	((base) + 0x90)
#define		UFCR_TXTL_SHIFT		(10)
#define		UFCR_TXTL_MASK		(0x3f<<10)
#define		UFCR_RFDIV_MASK		(7<<7)
#define		UFCR_RFDIV_SHIFT 	(7)
#define		UFCR_DCEDTE		(1<<6)
#define		UFCR_RXTL_MASK		(0x3f)
#define		UFCR_RXTL_SHIFT		(0)

#define USR1(base)	((base) + 0x94)
#define		USR1_PARITYERR	(1<<15)
#define		USR1_RTSS	(1<<14)
#define		USR1_TRDY	(1<<13)
#define		USR1_RTSD	(1<<12)
#define		USR1_ESCF	(1<<11)
#define		USR1_FRAMERR	(1<<10)
#define		USR1_RRDY	(1<<9)
#define		USR1_AGTIM	(1<<8)
#define		USR1_RXDS	(1<<6)
#define		USR1_AIRINT	(1<<5)
#define		USR1_AWAKE	(1<<4)

#define USR2(base)	((base) + 0x98)
#define		USR2_ADET	(1<<15)
#define		USR2_TXFE	(1<<14)
#define		USR2_DTRF	(1<<13)
#define		USR2_IDLE	(1<<12)
#define		USR2_ACST	(1<<11)
#define		USR2_RIDELT	(1<<10)
#define		USR2_RIIN	(1<<9)
#define		USR2_IRINT	(1<<8)
#define		USR2_WAKE	(1<<7)
#define		USR2_DCDDELT	(1<<6)
#define		USR2_DCDIN	(1<<5)
#define		USR2_RTSF	(1<<4)
#define		USR2_TXDC	(1<<3)
#define		USR2_BRCD	(1<<2)
#define		USR2_ORE	(1<<1)
#define		USR2_RDR	(1<<0)

#define UESC(base)	((base) + 0x9c)
#define UTIM(base)	((base) + 0xa0)
#define UBIR(base)	((base) + 0xa4)
#define UBMR(base)	((base) + 0xa8)
#define UBRC(base)	((base) + 0xac)
#define ONEMS(base)	((base) + 0xb0)
#define UTS(base)	((base) + 0xb4)
#define		UTS_FRCPER	(1<<13)
#define		UTS_LOOP	(1<<12)
#define 	UTS_DBGEN	(1<<11)
#define		UTS_LOOPIR	(1<<10)
#define		UTS_RXDBG	(1<<9)
#define		UTS_TXEMPTY	(1<<6)
#define		UTS_RXEMPTY	(1<<5)
#define		UTS_TXFULL	(1<<4)
#define		UTS_RXFULL	(1<<3)
#define		UTS_SOFTRST	(1<<0)

#define RX_FIFO_SIZE (32)
#define RX_FIFO_MASK (RX_FIFO_SIZE-1)
#define RX_FIFO_COUNT(ser) ((ser)->rxfifo_wp - (ser)->rxfifo_rp)
#define RX_FIFO_ROOM(ser) (RX_FIFO_SIZE - RX_FIFO_COUNT(ser))
#define RX_FIFO_WIDX(iuart) ((iuart)->rxfifo_wp & RX_FIFO_MASK)
#define RX_FIFO_RIDX(iuart) ((iuart)->rxfifo_rp & RX_FIFO_MASK)

#define TX_FIFO_SIZE (32)
#define TX_FIFO_MASK (TX_FIFO_SIZE-1)
#define TX_FIFO_COUNT(ser) ((ser)->txfifo_wp-(ser)->txfifo_rp)
#define TX_FIFO_RELEASED(ser) ((ser)->txfifo_releasep-(ser)->txfifo_rp)
#define TX_FIFO_ROOM(ser) (TX_FIFO_SIZE-TX_FIFO_COUNT(ser))
#define TX_FIFO_WIDX(iuart) ((iuart)->txfifo_wp & TX_FIFO_MASK)
#define TX_FIFO_RIDX(iuart) ((iuart)->txfifo_rp & TX_FIFO_MASK)

typedef struct IMX_Uart {
	BusDevice bdev;
	Clock_t *in_clk;	/* from crm module */
	Clock_t *refclk;
	char *name;
	UartPort *port;
	uint16_t ucr1, ucr2, ucr3, ucr4;
	uint16_t ufcr;
	uint16_t usr1;
	uint16_t usr2;
	uint16_t uesc;
	uint16_t utim;
	uint16_t ubir;
	uint16_t ubmr;
	uint16_t ubrc;
	uint16_t onems;
	uint16_t uts;

	UartChar txfifo[TX_FIFO_SIZE];
	uint64_t txfifo_wp;
	uint64_t txfifo_releasep;
	uint64_t txfifo_rp;
	int nsecs_per_char;

	uint16_t rxfifo[RX_FIFO_SIZE];
	uint64_t rxfifo_wp;
	uint64_t rxfifo_rp;

	int interrupt_posted;
	SigNode *irqNode;
	SigNode *rxDmaReqNode;
	SigNode *txDmaReqNode;
	CycleTimer baudTimer;
} IMX_Uart;

static void
update_interrupts(IMX_Uart * iuart)
{
	uint16_t rrdyen = iuart->ucr1 & UCR1_RRDYEN;
	uint16_t trdyen = iuart->ucr1 & UCR1_TRDYEN;
	uint16_t txmptyen = iuart->ucr1 & UCR1_TXMPTYEN;
	int interrupt = 0;
	int tx_room = TX_FIFO_ROOM(iuart);
	int rx_fill = RX_FIFO_COUNT(iuart);
	int txtl = (iuart->ufcr & UFCR_TXTL_MASK) >> UFCR_TXTL_SHIFT;
	int rxtl = (iuart->ufcr & UFCR_RXTL_MASK) >> UFCR_RXTL_SHIFT;
	if (trdyen && (tx_room >= txtl)) {
		interrupt = 1;
	}
	/* does it also trigger an interrupt when 0 characters are received  ? */
	if (rrdyen && (rx_fill >= rxtl)) {
		interrupt = 1;
	}
	if (txmptyen && (TX_FIFO_COUNT(iuart) == 0)) {
		interrupt = 1;
	}
	if (interrupt) {
		if (!iuart->interrupt_posted) {
			SigNode_Set(iuart->irqNode, SIG_LOW);
			iuart->interrupt_posted = 1;
		}
	} else {
		if (iuart->interrupt_posted) {
			SigNode_Set(iuart->irqNode, SIG_HIGH);
			iuart->interrupt_posted = 0;
		}
	}
}

static void
update_refclk(IMX_Uart * iuart)
{
	int rfdiv = (iuart->ufcr & UFCR_RFDIV_MASK) >> UFCR_RFDIV_SHIFT;
	int divider = 1;
	if (rfdiv < 6) {
		divider = (rfdiv ^ 7) - 1;
	} else if (rfdiv == 5) {
		divider = 7;
	} else {
		fprintf(stderr, "IMXUart: Illegal rfdiv value in UFCR register\n");
	}
	Clock_MakeDerived(iuart->refclk, iuart->in_clk, 1, divider);
	//fprintf(stderr,"Clock: %f, ref %f\n",Clock_Freq(iuart->in_clk),Clock_Freq(iuart->refclk));

}

/*
 * ------------------------------------------------------------------------
 * DMA request lines
 * ------------------------------------------------------------------------
 */

static void
update_txdma(IMX_Uart * iuart)
{
	int txdmaen = !!(iuart->ucr1 & UCR1_TXDMAEN);
	int txtl = (iuart->ufcr & UFCR_TXTL_MASK) >> UFCR_TXTL_SHIFT;
	if (txdmaen && (TX_FIFO_ROOM(iuart) >= txtl)) {
		SigNode_Set(iuart->txDmaReqNode, SIG_LOW);
	} else {
		SigNode_Set(iuart->txDmaReqNode, SIG_HIGH);
	}
}

static void
update_rxdma(IMX_Uart * iuart)
{
	int rxdmaen = !!(iuart->ucr1 & UCR1_RXDMAEN);
	int rxtl = (iuart->ufcr & UFCR_RXTL_MASK) >> UFCR_RXTL_SHIFT;
	if (rxtl && rxdmaen && (RX_FIFO_COUNT(iuart) >= rxtl)) {
		SigNode_Set(iuart->rxDmaReqNode, SIG_LOW);
	} else {
		SigNode_Set(iuart->rxDmaReqNode, SIG_HIGH);
	}
}

/*
 * ------------------------------------
 * Put one byte to the rxfifo
 * ------------------------------------
 */
static inline void
serial_rx_char(IMX_Uart * iuart, UartChar c)
{
	iuart->rxfifo[RX_FIFO_WIDX(iuart)] = c | URR_CHARRDY;
	iuart->rxfifo_wp++;
	return;
}

/*
 * --------------------------------------------------------------------------------
 * Serial Input has to accept the chars received from the underlying UART.
 * It has the ability to stop the data when the input fifo is full. This is
 * not very realistic because only UARTS with flow control enabled wait when
 * fifo is full. But timing isn't good enough currently to guarantee that
 * the cpu runs enough Cycles to empty the RX-Fifo. 
 * --------------------------------------------------------------------------------
 */

static void
serial_input(void *cd, UartChar c)
{
	IMX_Uart *iuart = cd;
	int fifocount;
	int room;
	serial_rx_char(iuart, c);
	room = RX_FIFO_ROOM(iuart);
	if (room < 1) {
		SerialDevice_StopRx(iuart->port);
	}
	update_rxdma(iuart);
	fifocount = RX_FIFO_COUNT(iuart);
	if (fifocount) {
		update_interrupts(iuart);
	}
	return;
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
serial_output(void *cd, UartChar * c)
{
	IMX_Uart *iuart = cd;
	if (TX_FIFO_RELEASED(iuart) > 0) {
		*c = iuart->txfifo[TX_FIFO_RIDX(iuart)];
		iuart->txfifo_rp = iuart->txfifo_rp + 1;
		update_txdma(iuart);
	} else {
		fprintf(stderr, "Bug in %s %s\n", __FILE__, __func__);
	}
	if (TX_FIFO_RELEASED(iuart) == 0) {
		SerialDevice_StopTx(iuart->port);
	}
	update_interrupts(iuart);
	return true;
}

static void
update_rx(IMX_Uart * iuart)
{
	if (iuart->ucr2 & UCR2_RXEN) {
		int room = RX_FIFO_ROOM(iuart);
		if (room > 0) {
			if (!(iuart->ucr3 & UCR3_RXDMUXSEL)) {
				fprintf(stderr, "Driver bug, starting rx with DMUXSEL 0\n");
			} else {
				SerialDevice_StartRx(iuart->port);
			}
		}
	} else {
		SerialDevice_StopRx(iuart->port);
	}
}

/*
 * ------------------------------------------------------------------
 * This releases a char in the txfifo for beeing sent
 * ------------------------------------------------------------------
 */
static void
IUart_ReleaseChar(void *clientData)
{
	IMX_Uart *iuart = (IMX_Uart *) clientData;
	if (iuart->txfifo_releasep < iuart->txfifo_wp) {
		iuart->txfifo_releasep++;
		SerialDevice_StartTx(iuart->port);
	}
	if (iuart->txfifo_releasep < iuart->txfifo_wp) {
		CycleTimer_Mod(&iuart->baudTimer, NanosecondsToCycles(iuart->nsecs_per_char));
	}
}

static uint32_t
urr_read(void *clientData, uint32_t address, int rqlen)
{
	IMX_Uart *iuart = (IMX_Uart *) clientData;
	uint16_t urr = iuart->rxfifo[RX_FIFO_RIDX(iuart)];
	int count = RX_FIFO_COUNT(iuart);
	//fprintf(stderr,"URR rxfifo count %lld\n",RX_FIFO_COUNT(iuart));
	if (count > 0) {
		iuart->rxfifo_rp++;
		update_interrupts(iuart);
		update_rxdma(iuart);
		/* Maybe rx was stopped because fifo was full, so reenable it */
		if ((count == RX_FIFO_SIZE)) {
			update_rx(iuart);
		}
	} else {
		urr = urr & ~URR_CHARRDY;
	}
	return urr;
}

static void
urr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "IMX: Uart receiver register not writable\n");
	return;
}

/*
 * --------------------------------------------------
 * Reading Uart Transmitter register returns 0 
 * --------------------------------------------------
 */
static uint32_t
utr_read(void *clientData, uint32_t address, int rqlen)
{
	return 0;
}

static void
utr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	IMX_Uart *iuart = (IMX_Uart *) clientData;
	if (TX_FIFO_ROOM(iuart) < 1) {
		fprintf(stderr, "IMX21-Uart TX-Fifo overflow\n");
		return;
	}
	iuart->txfifo[TX_FIFO_WIDX(iuart)] = value;
	iuart->txfifo_wp += 1;
	if (iuart->ucr2 & UCR2_TXEN) {
		if (!CycleTimer_IsActive(&iuart->baudTimer)) {
			CycleTimer_Mod(&iuart->baudTimer,
				       NanosecondsToCycles(iuart->nsecs_per_char));
		}
	} else {
		fprintf(stderr, "i.MX21 UART: Writing to TX register of %s while disabled\n",
			iuart->name);
	}
	update_interrupts(iuart);
	update_txdma(iuart);
	return;
}

static uint32_t
ucr1_read(void *clientData, uint32_t address, int rqlen)
{
	IMX_Uart *iuart = (IMX_Uart *) clientData;
	return iuart->ucr1;
}

static void
ucr1_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	IMX_Uart *iuart = (IMX_Uart *) clientData;
	iuart->ucr1 = value & 0xfffd;
	update_interrupts(iuart);
	return;
}

static uint32_t
ucr2_read(void *clientData, uint32_t address, int rqlen)
{
	IMX_Uart *iuart = (IMX_Uart *) clientData;
	return iuart->ucr2;
}

static void
ucr2_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	IMX_Uart *iuart = (IMX_Uart *) clientData;
	uint16_t diff = iuart->ucr2 ^ value;
	UartCmd cmd;
	iuart->ucr2 = value | UCR2_SRST;
	if (diff & UCR2_TXEN) {
		if (value & UCR2_TXEN) {
			if (!CycleTimer_IsActive(&iuart->baudTimer)) {
				CycleTimer_Mod(&iuart->baudTimer,
					       NanosecondsToCycles(iuart->nsecs_per_char));
			}
		} else {
			CycleTimer_Remove(&iuart->baudTimer);
		}
	}
	if (diff & UCR2_RXEN) {
		update_rx(iuart);
	}
	if (!(value & UCR2_SRST)) {
		iuart->txfifo_wp = iuart->txfifo_rp = 0;
		iuart->txfifo_releasep = 0;
		iuart->rxfifo_wp = iuart->rxfifo_rp = 0;
		iuart->usr1 = USR1_RXDS | USR1_TRDY;
		iuart->usr2 = USR2_TXFE | USR2_TXDC;
	}
	if (iuart->ucr2 & UCR2_CTSC) {
		cmd.opcode = UART_OPC_CRTSCTS;
		cmd.arg = 1;
		SerialDevice_Cmd(iuart->port, &cmd);
	} else {
		cmd.opcode = UART_OPC_CRTSCTS;
		cmd.arg = 0;
		SerialDevice_Cmd(iuart->port, &cmd);

		cmd.opcode = UART_OPC_SET_RTS;
		if (iuart->ucr2 & UCR2_CTS) {
			cmd.arg = UART_RTS_ACT;
		} else {
			cmd.arg = UART_RTS_INACT;
		}
		SerialDevice_Cmd(iuart->port, &cmd);
	}
	cmd.opcode = UART_OPC_PAREN;
	if (iuart->ucr2 & UCR2_PREN) {
		cmd.arg = 1;
	} else {
		cmd.arg = 0;
	}
	SerialDevice_Cmd(iuart->port, &cmd);

	cmd.opcode = UART_OPC_PARODD;
	if (iuart->ucr2 & UCR2_PROE) {
		cmd.arg = 1;
	} else {
		cmd.arg = 0;
	}
	SerialDevice_Cmd(iuart->port, &cmd);

	cmd.opcode = UART_OPC_SET_CSIZE;
	if (iuart->ucr2 & UCR2_WS) {
		cmd.arg = 8;
	} else {
		cmd.arg = 7;
	}
	SerialDevice_Cmd(iuart->port, &cmd);
	return;
}

static uint32_t
ucr3_read(void *clientData, uint32_t address, int rqlen)
{
	IMX_Uart *iuart = (IMX_Uart *) clientData;
	return iuart->ucr3;
}

static void
ucr3_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	IMX_Uart *iuart = (IMX_Uart *) clientData;
	uint32_t diff = value ^ iuart->ucr3;
	iuart->ucr3 = value;
	if ((diff & UCR3_RXDMUXSEL)) {
		update_rx(iuart);
	}
	return;
}

static uint32_t
ucr4_read(void *clientData, uint32_t address, int rqlen)
{
	IMX_Uart *iuart = (IMX_Uart *) clientData;
	return iuart->ucr4;
}

static void
ucr4_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	IMX_Uart *iuart = (IMX_Uart *) clientData;
	iuart->ucr4 = value;
	return;
}

static uint32_t
ufcr_read(void *clientData, uint32_t address, int rqlen)
{
	IMX_Uart *iuart = (IMX_Uart *) clientData;
	return iuart->ufcr;
}

static void
ufcr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	IMX_Uart *iuart = (IMX_Uart *) clientData;
	iuart->ufcr = value & 0xffff;
	update_interrupts(iuart);
	update_refclk(iuart);
	return;
}

static uint32_t
usr1_read(void *clientData, uint32_t address, int rqlen)
{
	IMX_Uart *iuart = (IMX_Uart *) clientData;
	UartCmd cmd;
	int tx_room = TX_FIFO_ROOM(iuart);
	int rx_fill = RX_FIFO_COUNT(iuart);
	int txtl = (iuart->ufcr & UFCR_TXTL_MASK) >> UFCR_TXTL_SHIFT;
	int rxtl = (iuart->ufcr & UFCR_RXTL_MASK) >> UFCR_RXTL_SHIFT;
	uint16_t usr1 = iuart->usr1;
	uint16_t diff;

	if (tx_room >= txtl) {
		usr1 |= USR1_TRDY;
	} else {
		usr1 &= ~USR1_TRDY;
	}
	if (rx_fill >= rxtl) {
		usr1 &= ~USR1_RRDY;
	} else {
		usr1 |= USR1_RRDY;
	}
	cmd.opcode = UART_OPC_GET_CTS;
	SerialDevice_Cmd(iuart->port, &cmd);
	if (cmd.retval) {
		usr1 |= USR1_RTSS;
	}
	diff = usr1 ^ iuart->usr1;
	if (diff & USR1_RTSS) {
		usr1 |= USR1_RTSD;
	}
	iuart->usr1 = usr1;
	return usr1;
}

static void
usr1_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	IMX_Uart *iuart = (IMX_Uart *) clientData;
	uint16_t clearmask = value & 0x9d30;
	iuart->usr1 &= ~clearmask;
	return;
}

static uint32_t
usr2_read(void *clientData, uint32_t address, int rqlen)
{
	IMX_Uart *iuart = (IMX_Uart *) clientData;
	uint16_t diff;
	uint16_t usr2 = iuart->usr2;
	int rx_fill = RX_FIFO_COUNT(iuart);
	int tx_fill = TX_FIFO_COUNT(iuart);
	UartCmd cmd;

	if (rx_fill > 0) {
		usr2 |= USR2_RDR;
	} else {
		usr2 &= ~USR2_RDR;
	}
	if (tx_fill > 0) {
		//fprintf(stderr,"Not empty\n");
		usr2 &= ~(USR2_TXFE | USR2_TXDC);
	} else {
		usr2 |= USR2_TXFE | USR2_TXDC;
	}

	cmd.opcode = UART_OPC_GET_RI;
	SerialDevice_Cmd(iuart->port, &cmd);
	if (cmd.retval) {
		usr2 |= USR2_RIIN;
	}
	cmd.opcode = UART_OPC_GET_DCD;
	SerialDevice_Cmd(iuart->port, &cmd);
	if (cmd.retval) {
		usr2 |= USR2_DCDIN;
	}

	diff = usr2 ^ iuart->usr2;
	if (diff & USR2_RIIN) {
		usr2 |= USR2_RIDELT;
	}
	if (diff & USR2_DCDIN) {
		usr2 |= USR2_DCDDELT;
	}
	iuart->usr2 = usr2;
	if (!(usr2 & USR2_TXFE) || (~usr2 & USR2_RDR)) {
		Senseless_Report(150);
	}
	return iuart->usr2;
}

static void
usr2_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	IMX_Uart *iuart = (IMX_Uart *) clientData;
	int clearmask = value & 0xbdf6;	// Is ring indicator writable  ????
	iuart->usr2 &= ~clearmask;
	return;
}

/*
 * -------------------------------
 * Escape: No functionality
 * -------------------------------
 */
static uint32_t
uesc_read(void *clientData, uint32_t address, int rqlen)
{
	IMX_Uart *iuart = (IMX_Uart *) clientData;
	return iuart->uesc;
}

static void
uesc_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	IMX_Uart *iuart = (IMX_Uart *) clientData;
	iuart->uesc = value & 0xff;
	return;
}

static uint32_t
utim_read(void *clientData, uint32_t address, int rqlen)
{
	IMX_Uart *iuart = (IMX_Uart *) clientData;
	return iuart->utim;
}

static void
utim_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	IMX_Uart *iuart = (IMX_Uart *) clientData;
	iuart->utim = value & 0xfff;
	return;
}

static uint32_t
ubir_read(void *clientData, uint32_t address, int rqlen)
{
	IMX_Uart *iuart = (IMX_Uart *) clientData;
	return iuart->ubir;
}

static void
ubir_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	IMX_Uart *iuart = (IMX_Uart *) clientData;
	//fprintf(stderr,"UBIR write %d\n",iuart->ubir);
	iuart->ubir = value & 0xffff;
	return;
}

static uint32_t
ubmr_read(void *clientData, uint32_t address, int rqlen)
{
	IMX_Uart *iuart = (IMX_Uart *) clientData;
	return iuart->ubmr;
}

static void
ubmr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	IMX_Uart *iuart = (IMX_Uart *) clientData;
	int baudrate;
	UartCmd cmd;
	iuart->ubmr = value & 0xffff;
	baudrate = 1.0 * (iuart->ubir + 1)
	    * Clock_Freq(iuart->refclk) / (iuart->ubmr + 1) / 16;
	cmd.opcode = UART_OPC_SET_BAUDRATE;
	cmd.arg = baudrate;
	SerialDevice_Cmd(iuart->port, &cmd);
	if (baudrate) {
		int nsecs = 10000000000LL / baudrate;
		if ((nsecs > 5000) && (iuart->nsecs_per_char != nsecs)) {
			iuart->nsecs_per_char = nsecs;
			if (CycleTimer_IsActive(&iuart->baudTimer)) {
				CycleTimer_Mod(&iuart->baudTimer,
					       NanosecondsToCycles(iuart->nsecs_per_char));
			}
		}
	}
	return;
}

static uint32_t
ubrc_read(void *clientData, uint32_t address, int rqlen)
{
	IMX_Uart *iuart = (IMX_Uart *) clientData;
	return iuart->ubrc;
}

static void
ubrc_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	IMX_Uart *iuart = (IMX_Uart *) clientData;
	iuart->ubrc = value;
	return;
}

static uint32_t
onems_read(void *clientData, uint32_t address, int rqlen)
{
	IMX_Uart *iuart = (IMX_Uart *) clientData;
	return iuart->onems;
}

static void
onems_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	IMX_Uart *iuart = (IMX_Uart *) clientData;
	iuart->onems = value;
	return;
}

static uint32_t
uts_read(void *clientData, uint32_t address, int rqlen)
{
	IMX_Uart *iuart = (IMX_Uart *) clientData;
	int rx_fill = RX_FIFO_COUNT(iuart);
	int tx_fill = TX_FIFO_COUNT(iuart);
	int rx_room = RX_FIFO_ROOM(iuart);
	int tx_room = TX_FIFO_ROOM(iuart);
	if (rx_fill > 0) {
		iuart->uts &= ~UTS_RXEMPTY;
	} else {
		iuart->uts |= UTS_RXEMPTY;
	}
	if (tx_fill > 0) {
		iuart->uts &= ~UTS_TXEMPTY;
	} else {
		iuart->uts |= UTS_TXEMPTY;
	}
	if (rx_room > 0) {
		iuart->uts &= ~UTS_RXFULL;
	} else {
		iuart->uts |= UTS_RXFULL;
	}
	if (tx_room > 0) {
		iuart->uts &= ~UTS_TXFULL;
	} else {
		iuart->uts |= UTS_TXFULL;
	}
	//fprintf(stderr,"rx_room %d tx_room %d,rx_fill %d tx_fill %d uts %08x\n",rx_room,tx_room,rx_fill,tx_fill,iuart->uts);
	if (!(iuart->uts & UTS_TXEMPTY) || (iuart->uts & UTS_RXEMPTY)) {
		Senseless_Report(100);
	}
	return iuart->uts;
}

static void
uts_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	IMX_Uart *iuart = (IMX_Uart *) clientData;
	iuart->uts = (iuart->uts & ~0x3e00) | (value & 0x3e00);
	return;

}

static uint32_t
undefined_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "WARNING !: read from undefined location 0x%08x in i.MX21 UART module\n",
		address);
	return 0;
}

static void
undefined_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "WARNING !: write to undefined location 0x%08x in i.MX21 UART module\n",
		address);
	return;
}

static void
IMXUart_Map(void *owner, uint32_t base, uint32_t mask, uint32_t flags)
{
	IMX_Uart *iuart = owner;
	/* Partial read: Read all return part, Partial Write: Read modify write */
	uint32_t mapflags = IOH_FLG_PRD_RARP | IOH_FLG_PWR_RMW | IOH_FLG_HOST_ENDIAN;
	IOH_New32f(URR(base), urr_read, urr_write, iuart, mapflags);
	IOH_New32f(UTR(base), utr_read, utr_write, iuart, mapflags);
	IOH_New32f(UCR1(base), ucr1_read, ucr1_write, iuart, mapflags);
	IOH_New32f(UCR2(base), ucr2_read, ucr2_write, iuart, mapflags);
	IOH_New32f(UCR3(base), ucr3_read, ucr3_write, iuart, mapflags);
	IOH_New32f(UCR4(base), ucr4_read, ucr4_write, iuart, mapflags);
	IOH_New32f(UFCR(base), ufcr_read, ufcr_write, iuart, mapflags);
	IOH_New32f(USR1(base), usr1_read, usr1_write, iuart, mapflags);
	IOH_New32f(USR2(base), usr2_read, usr2_write, iuart, mapflags);
	IOH_New32f(UESC(base), uesc_read, uesc_write, iuart, mapflags);
	IOH_New32f(UTIM(base), utim_read, utim_write, iuart, mapflags);
	IOH_New32f(UBIR(base), ubir_read, ubir_write, iuart, mapflags);
	IOH_New32f(UBMR(base), ubmr_read, ubmr_write, iuart, mapflags);
	IOH_New32f(UBRC(base), ubrc_read, ubrc_write, iuart, mapflags);
	IOH_New32f(ONEMS(base), onems_read, onems_write, iuart, mapflags);
	IOH_New32f(UTS(base), uts_read, uts_write, iuart, mapflags);
	IOH_NewRegion(base, 0x1000, undefined_read, undefined_write, IOH_FLG_HOST_ENDIAN, iuart);
}

static void
IMXUart_UnMap(void *owner, uint32_t base, uint32_t mask)
{
	IOH_Delete32(URR(base));
	IOH_Delete32(UTR(base));
	IOH_Delete32(UCR1(base));
	IOH_Delete32(UCR2(base));
	IOH_Delete32(UCR3(base));
	IOH_Delete32(UCR4(base));
	IOH_Delete32(UFCR(base));
	IOH_Delete32(USR1(base));
	IOH_Delete32(USR2(base));
	IOH_Delete32(UESC(base));
	IOH_Delete32(UTIM(base));
	IOH_Delete32(UBIR(base));
	IOH_Delete32(UBMR(base));
	IOH_Delete32(UBRC(base));
	IOH_Delete32(ONEMS(base));
	IOH_Delete32(UTS(base));
	IOH_DeleteRegion(base, 0x1000);
}

BusDevice *
IMXUart_New(const char *name)
{
	IMX_Uart *iuart = sg_new(IMX_Uart);
	iuart->irqNode = SigNode_New("%s.irq", name);
	iuart->rxDmaReqNode = SigNode_New("%s.rx_dmareq", name);
	iuart->txDmaReqNode = SigNode_New("%s.tx_dmareq", name);
	if (!iuart->irqNode || !iuart->rxDmaReqNode || !iuart->txDmaReqNode) {
		fprintf(stderr, "IMX21Uart: Can not create uart signal lines\n");
	}
	iuart->usr1 = USR1_RXDS | USR1_TRDY;
	iuart->usr2 = USR2_TXFE | USR2_TXDC;
	iuart->ucr2 = UCR2_SRST;
	/* Make it nonworking when user forgets to reset the fifo */
	iuart->rxfifo_rp = 0x1000000;
	iuart->ufcr = 0x00000801;
	iuart->bdev.first_mapping = NULL;
	iuart->bdev.Map = IMXUart_Map;
	iuart->bdev.UnMap = IMXUart_UnMap;
	iuart->bdev.owner = iuart;
	iuart->bdev.hw_flags = MEM_FLAG_WRITABLE | MEM_FLAG_READABLE;
	iuart->name = sg_strdup(name);
	iuart->port = Uart_New(name, serial_input, serial_output, NULL, iuart);
	iuart->in_clk = Clock_New("%s.clk", name);
	iuart->nsecs_per_char = 1000000;
	CycleTimer_Init(&iuart->baudTimer, IUart_ReleaseChar, iuart);

	/* Clock should come from perclk1 of CRM module */

	iuart->refclk = Clock_New("%s.refclk", name);
	update_refclk(iuart);
	update_interrupts(iuart);
	fprintf(stderr, "IMX21 Uart \"%s\" created\n", name);
	return &iuart->bdev;
}
