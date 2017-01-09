/*
 ************************************************************************************************** 
 * Emulation of National semiconductor PC16550D Universal asynchronous
 * Receiver/Transmitter with Fifo
 *
 * state: never used, totally untested
 *
 * Copyright 2005 2009 Jochen Karrer. All rights reserved.
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
#include "signode.h"
#include "serial.h"
#include "clock.h"
#include "pc16550.h"

#define RBR(pc)	(0)		/* Read  */
#define THR(pc)	(0)		/* Write */
#define IER(pc)	(1)
#define		IER_ERBFI	(1<<0)
#define		IER_ETBEI	(1<<1)
#define		IER_ELSI	(1<<2)
#define		IER_EDSSI	(1<<3)
#define IIR(pc)	(2)		/* Read */
#define		IIR_NPENDING	(1<<0)
#define		IIR_INTID_MASK	(7<<1)
#define		IIR_INTID_SHIFT (1)
#define			IIR_TYPE_RLS 	(3)
#define			IIR_TYPE_RDA 	(2)
#define			IIR_TYPE_CTI 	(6)
#define			IIR_TYPE_THRE 	(1)
#define			IIR_TYPE_MST	(0)
#define FCR(pc)	(2)		/* Write */
#define FCR_FIFO_ENABLE		(1<<0)
#define FCR_RCVR_FIFO_RESET	(1<<1)
#define FCR_XMIT_FIFO_RESET	(1<<2)
#define FCR_DMA_MODE_SEL	(1<<3)
#define FCR_RCVR_TRIGGER_SHIFT	(6)
#define FCR_RCVR_TRIGGER_MASK	(3<<6)

#define LCR(pc)	(3)
#define		LCR_BITS_MASK (3)
#define		LCR_5BITS (0)
#define		LCR_6BITS (1)
#define		LCR_7BITS (2)
#define 	LCR_8BITS (3)
#define		LCR_STB  (4)
#define 	LCR_PEN	 (8)
#define 	LCR_EPS	 (0x10)
#define		LCR_SPAR (0x20)
#define		LCR_SBRK (0x40)
#define		LCR_DLAB (0x80)
#define	MCR(pc)	(4)
#define		MCR_DTR		(1<<0)
#define		MCR_RTS		(1<<1)
#define		MCR_OUT1	(1<<2)
#define		MCR_OUT2	(1<<3)
#define		MCR_LOOP	(1<<4)
#define LSR(pc)	(5)
#define		LSR_DR	(1)
#define 	LSR_OE  (1<<1)
#define		LSR_PE  (1<<2)
#define 	LSR_FE	(1<<3)
#define		LSR_BI	(1<<4)
#define		LSR_THRE (1<<5)
#define		LSR_TEMT (1<<6)
#define		LSR_ERR	(1<<7)

#define MSR(pc)	(6)
#define		MSR_DCTS	(1<<0)
#define		MSR_DDSR	(1<<1)
#define		MSR_TERI	(1<<2)
#define		MSR_DDCD	(1<<3)
#define		MSR_CTS		(1<<4)
#define		MSR_DSR		(1<<5)
#define		MSR_RI		(1<<6)
#define		MSR_DCD		(1<<7)
#define SCR(pc)	(7)
#define DLL(pc)	(0)		/* DLAB=1 */
#define DLM(pc)	(1)		/* DLAB=1 */

#define DLAB(pc) (LCR(pc) & (1<<7))

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

typedef struct PC16550 {
	BusDevice bdev;
	UartPort *backend;
	Clock_t *clk_in;
	Clock_t *clk_baud;

	int register_shift;

	int baudrate;
	uint8_t lcr;
	uint8_t mcr;
	uint8_t lsr;
	uint8_t msr;
	uint8_t dll, dlm;
	uint8_t ier;
	uint8_t iir;
	uint8_t fcr;
	uint8_t scr;

	UartChar rx_fifo[RX_FIFO_SIZE_MAX];
	uint64_t rxfifo_wp;
	uint64_t rxfifo_rp;
	uint32_t rx_fifo_size;

	UartChar tx_fifo[TX_FIFO_SIZE_MAX];
	uint64_t txfifo_wp;
	uint64_t txfifo_rp;
	uint32_t tx_fifo_size;

	int interrupt_posted;
	SigNode *irqNode;
} PC16550;

static void
update_interrupts(PC16550 * pc)
{
	/* do Priority encoding */
	int interrupt;
	uint8_t ier = pc->ier;
	uint8_t int_id = 0;
	if ((ier & IER_ELSI) && (pc->lsr & (LSR_OE | LSR_PE | LSR_FE | LSR_BI))) {

		int_id = IIR_TYPE_RLS;
		interrupt = 1;

	} else if ((ier & IER_ERBFI) && (pc->lsr & LSR_DR)) {
		// Fifo trigger level should be checked here also

		int_id = IIR_TYPE_RDA;
		interrupt = 1;

		/* 
		 * Character Timeout indication (IIR_TYPE_CTI) ommited here because 
		 * trigger level IRQ mode is  not implemented 
		 */
	} else if ((ier & IER_ETBEI) && (pc->lsr & LSR_THRE)) {

		int_id = IIR_TYPE_THRE;
		interrupt = 1;

	} else if ((ier & IER_EDSSI) && pc->msr & (MSR_DCTS | MSR_TERI | MSR_DDSR | MSR_DDCD)) {
		int_id = IIR_TYPE_MST;
		interrupt = 1;

	} else {
		int_id = 0;
		interrupt = 0;
	}
	pc->iir = (pc->iir & ~IIR_INTID_MASK) | (int_id << IIR_INTID_SHIFT);
	if (interrupt) {
		pc->iir &= ~IIR_NPENDING;
		if (!pc->interrupt_posted) {
			SigNode_Set(pc->irqNode, SIG_LOW);
			pc->interrupt_posted = 1;
		}
	} else {
		pc->iir |= IIR_NPENDING;
		if (pc->interrupt_posted) {
			SigNode_Set(pc->irqNode, SIG_HIGH);
			pc->interrupt_posted = 0;
		}
	}

}

static void
update_clock(PC16550 * pc)
{
	uint16_t divisor;
	divisor = pc->dll + (pc->dlm << 8);
	if (divisor) {
		Clock_MakeDerived(pc->clk_baud, pc->clk_in, 1, divisor);
	} else {
		Clock_MakeDerived(pc->clk_baud, pc->clk_in, 0, 1);
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
	PC16550 *pc = (PC16550 *) clientData;
	UartCmd cmd;
	cmd.opcode = UART_OPC_SET_BAUDRATE;
	cmd.arg = Clock_Freq(clock);
	SerialDevice_Cmd(pc->backend, &cmd);
	fprintf(stderr, "****************The Baudrate is now %d\n", (uint32_t) Clock_Freq(clock));
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
	PC16550 *pc = cd;
	if (TX_FIFO_COUNT(pc) > 0) {
		*c = pc->tx_fifo[TX_FIFO_RP(pc)];
		pc->txfifo_rp++;
	} else {
		fprintf(stderr, "Fifo empty Bug in 16550 serial_output\n");
	}
	if (TX_FIFO_COUNT(pc) == 0) {
		pc->lsr |= LSR_THRE | LSR_TEMT;
		update_interrupts(pc);
		SerialDevice_StopTx(pc->backend);
	} else if (TX_FIFO_ROOM(pc) > 0) {
		pc->lsr |= LSR_THRE;
		update_interrupts(pc);
	}
	return true;
}

/*
 ************************************************************
 * Put one byte to the rxfifo
 ************************************************************
 */
static inline int
serial_put_rx_fifo(PC16550 * pc, uint8_t c)
{
	int room = RX_FIFO_ROOM(pc);
	if (room < 1) {
		return -1;
	}
	pc->rx_fifo[RX_FIFO_WP(pc)] = c;
	pc->rxfifo_wp++;
	if (room == 1) {
		SerialDevice_StopRx(pc->backend);
		return 0;
	}
	return 1;
}

static void
serial_input(void *cd, UartChar c)
{
	PC16550 *pc = cd;
	int fifocount;
	if (serial_put_rx_fifo(pc, c) < 1) {
		return;
	}
	fifocount = RX_FIFO_COUNT(pc);
	if (fifocount) {
		pc->lsr |= LSR_DR;
		update_interrupts(pc);
	}
}

static void
reset_rx_fifo(PC16550 * pc)
{
	pc->rxfifo_rp = pc->rxfifo_wp = 0;
	pc->lsr &= ~LSR_DR;
	update_interrupts(pc);
}

static void
reset_tx_fifo(PC16550 * pc)
{
	pc->txfifo_rp = pc->txfifo_wp = 0;
	pc->lsr |= LSR_THRE;
	/* 
	 ****************************************************
	 * TEMT is not really emptied by reset but 
	 * a separeate shift register is not implemented
	 ****************************************************
	 */
	pc->lsr |= LSR_TEMT;
	update_interrupts(pc);
}

static void
update_serconfig(PC16550 * pc)
{
	UartCmd cmd;
	tcflag_t bits;
	tcflag_t parodd;
	tcflag_t parenb;
	tcflag_t crtscts;
	/* Does 16550 really have no automatic rtscts handshaking ? */
	crtscts = 0;
	if (pc->lcr & LCR_EPS) {
		parodd = 0;
	} else {
		parodd = 1;
	}
	if (pc->lcr & LCR_PEN) {
		parenb = 1;
	} else {
		parenb = 0;
	}
	switch (pc->lcr & LCR_BITS_MASK) {
	    case LCR_5BITS:
		    bits = 5;
		    break;
	    case LCR_6BITS:
		    bits = 6;
		    break;
	    case LCR_7BITS:
		    bits = 7;
		    break;
	    case LCR_8BITS:
		    bits = 8;
		    break;
		    /* Can not be reached */
	    default:
		    bits = 8;
		    break;
	}

	if (crtscts) {
		cmd.opcode = UART_OPC_CRTSCTS;
		cmd.arg = 1;
		SerialDevice_Cmd(pc->backend, &cmd);
	} else {
		cmd.opcode = UART_OPC_CRTSCTS;
		cmd.arg = 0;
		SerialDevice_Cmd(pc->backend, &cmd);

		cmd.opcode = UART_OPC_SET_RTS;
		/* Set the initial state of RTS */
#if 0
		if (iuart->ucr2 & UCR2_CTS) {
			cmd.arg = UART_RTS_ACT;
		} else {
			cmd.arg = UART_RTS_INACT;
		}
		SerialDevice_Cmd(pc->backend, &cmd);
#endif
	}
	cmd.opcode = UART_OPC_PAREN;
	cmd.arg = parenb;
	SerialDevice_Cmd(pc->backend, &cmd);

	cmd.opcode = UART_OPC_PARODD;
	cmd.arg = parodd;
	SerialDevice_Cmd(pc->backend, &cmd);

	cmd.opcode = UART_OPC_SET_CSIZE;
	cmd.arg = bits;
	SerialDevice_Cmd(pc->backend, &cmd);

}

/*
 * --------------------------------------------
 * Write one character to the TX-Fifo
 * --------------------------------------------
 */
static inline void
thr_write(PC16550 * pc, uint8_t c)
{
	int room;
	uint8_t old_lsr;
	if (TX_FIFO_ROOM(pc) > 0) {
		pc->tx_fifo[TX_FIFO_WP(pc)] = c;
		pc->txfifo_wp++;
	}
	old_lsr = pc->lsr;
	pc->lsr &= ~LSR_TEMT;
	room = TX_FIFO_ROOM(pc);
	if (room > 0) {
		pc->lsr |= LSR_THRE;
	} else {
		pc->lsr &= ~LSR_THRE;
	}
	if (pc->lsr != old_lsr) {
		update_interrupts(pc);
	}
	SerialDevice_StartTx(pc->backend);
}

/*
 * ----------------------------------------
 * Fetch a byte from RX-Fifo
 * ----------------------------------------
 */
static inline uint8_t
rbr_read(PC16550 * pc)
{
	uint8_t data = 0;
	if (RX_FIFO_COUNT(pc) > 0) {
		data = pc->rx_fifo[RX_FIFO_RP(pc)];
		pc->rxfifo_rp++;
	}
	if (RX_FIFO_COUNT(pc) == 0) {
		pc->lsr &= ~LSR_DR;
		update_interrupts(pc);
	}
	return data;
}

/* 
 **************************************************
 * Divisor latch 
 **************************************************
 */
static inline void
dll_write(PC16550 * pc, uint8_t value)
{
	pc->dll = value;
	update_clock(pc);
}

static inline void
dlm_write(PC16550 * pc, uint8_t value)
{
	pc->dlm = value;
	update_clock(pc);

}

static uint32_t
reg0_read(void *clientData, uint32_t address, int rqlen)
{
	PC16550 *pc = clientData;
	if (DLAB(pc)) {
		return pc->dll;	/* Divisor latch last significant */
	} else {
		return rbr_read(pc);
	}
}

static void
reg0_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	PC16550 *pc = clientData;
	if (DLAB(pc)) {
		dll_write(pc, value);
	} else {
		thr_write(pc, value);
	}
}

/*
 * ---------------------------------------------------------------------
 * Interrupt Enable register 
 *	Bit 0: 	Receive Data Interrupt and Timeout	
 *	Bit 1:  Transmit hold register Empty interrupt enable
 *	Bit 2:  Receive Line status interrupt enable
 *	Bit 3: 	Modem status change interrupt enable
 *	Bit 4-7: 0
 * ---------------------------------------------------------------------
 */
static uint32_t
ier_read(PC16550 * pc)
{
	return pc->ier;
}

static void
ier_write(PC16550 * pc, uint8_t value)
{
	pc->ier = value;
	update_interrupts(pc);
	return;
}

static uint32_t
reg1_read(void *clientData, uint32_t address, int rqlen)
{
	PC16550 *pc = clientData;
	if (DLAB(pc)) {
		return pc->dlm;	/* Divisor latch most significant */
	} else {
		return ier_read(pc);
	}
	return 0;
}

static void
reg1_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	PC16550 *pc = clientData;
	if (DLAB(pc)) {
		dlm_write(pc, value);
	} else {
		ier_write(pc, value);
	}
	return;
}

/*
 * -------------------------------------------------------------
 * Interrupt Identification Register (Read only) 
 *	Bit 0:	Summary Interrupt
 *	Bit 1,2 Identify highest priority pending Interrupt
 *	Bit 3: 	Timeout interrupt in 16450 mode
 *	Bit 4,5 always 0
 *	Bit 6,7 set when fcr0 = 1
 * -------------------------------------------------------------
 */
static uint32_t
iir_read(void *clientData, uint32_t address, int rqlen)
{
	return 0;
}

/* 
 * ----------------------------------------------------------------------------------
 * Fifo Control Register (Write only)
 *	Bit 0:	enable RCVR and XMIT Fifos, resetting clears them
 *      Bit 1:	Clear RX-Fifo and reset counters
 * 	Bit 2:	Reset XMIT-Fifo and reset counters
 *	Bit 3:  Change RXRDY and TXRDY Pins from mode 0 to mode 1
 * 	Bit 4,5: Reserved
 * 	Bit 6,7: Trigger level for RX-Fifo Interrupt {00,01,10,11} 1,4,8,14 Bytes
 * ---------------------------------------------------------------------------------
 */
static void
fcr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	PC16550 *pc = (PC16550 *) clientData;
	uint8_t diff = value ^ pc->fcr;
	if (diff & FCR_FIFO_ENABLE) {
		reset_rx_fifo(pc);
		reset_tx_fifo(pc);
		if (value & FCR_FIFO_ENABLE) {
			pc->tx_fifo_size = 16;
			pc->rx_fifo_size = 16;
		} else {
			pc->tx_fifo_size = 1;
			pc->rx_fifo_size = 1;
		}
	}
	/* If fifo enable not 1 the other bits will not be programmed */
	if (!(value & FCR_FIFO_ENABLE)) {
		return;
	}
	if (value & FCR_RCVR_FIFO_RESET) {
		pc->rxfifo_wp = pc->rxfifo_rp = 0;
	} else if (value & FCR_XMIT_FIFO_RESET) {
		pc->txfifo_wp = pc->txfifo_rp = 0;
	}
	pc->fcr = value;
	return;
}

/* 
 * --------------------------------------------------------------------------
 * Line Control Register
 * 	Bits 0 and 1: 00: 5Bit 01: 6Bit 10: 7Bit 11: 8Bit
 *	Bit  2:	   0 = 1 Stop Bit 1 = 1.5-2 Stop Bits
 *      Bit  3: PE 1 = Parity Enable
 * 	Bit  4: EPS 1 = Even Parity
 *	Bit  5: Stick parity
 *	Bit  6: Break control
 *	Bit  7: DLAB Divisor Latch Access Bit (bank switch for registers)
 * ---------------------------------------------------------------------------
 */
static uint32_t
lcr_read(void *clientData, uint32_t address, int rqlen)
{
	PC16550 *pc = clientData;
	return pc->lcr;
}

static void
lcr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	PC16550 *pc = clientData;
	pc->lcr = value;
	update_serconfig(pc);
	return;
}

/*
 * ------------------------------------------------------------------------------
 * Modem Control Register
 *	Bit 0: Controls the #DTR output
 *	Bit 1: Controls the #RTS output 
 *	Bit 2: Controls the #OUT1 signal (Auxilliary)
 *	Bit 3: Controls the #OUT2 signal (Auxilliary)
 *	Bit 4: Local Loopback feature	
 *	Bit 5-7: always 0
 * ------------------------------------------------------------------------------
 */
static uint32_t
mcr_read(void *clientData, uint32_t address, int rqlen)
{
	return 0;
}

static void
mcr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	PC16550 *pc = clientData;
	uint8_t diff = pc->mcr ^ value;
	UartCmd cmd;

	if (diff & (MCR_DTR | MCR_RTS)) {

		cmd.opcode = UART_OPC_SET_DTR;
		if (value & MCR_DTR) {
			cmd.arg = 1;
		} else {
			cmd.arg = 0;
		}
		SerialDevice_Cmd(pc->backend, &cmd);

		cmd.opcode = UART_OPC_SET_RTS;
		if (value & MCR_RTS) {
			cmd.arg = 1;
		} else {
			cmd.arg = 0;
		}
		SerialDevice_Cmd(pc->backend, &cmd);

	}
	pc->mcr = value;
	return;
}

/*
 * -------------------------------------------------------------
 * Line Status Register
 * 	Bit 0: DR Data Ready indicator 
 *	Bit 1: OE Overrun Error indicator
 *	Bit 2: PE Parity Error indicator
 * 	Bit 3: FE Framing Error indicator
 *	Bit 4: BI Break Interrupt indicator
 *      Bit 5: THRE Transmitter Holding Register Empty
 *	Bit 6: TEMT Transmitter Empty indicator 
 *	Bit 7: Error Summary (not in 16450 mode)
 * -------------------------------------------------------------
 */
static uint32_t
lsr_read(void *clientData, uint32_t address, int rqlen)
{
	PC16550 *pc = clientData;
	return pc->lsr;
}

static void
lsr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	return;
}

/*
 * -----------------------------------------------------------------
 * Modem Status register
 *	Bit 0: Delta CTS indicator
 *	Bit 1: Delta DSR indicator
 *	Bit 2: TERI Trailing edge of Ring Indicator (low to high)
 *	Bit 3: DCD Delta carrier detect
 *	Bit 4: Complement of RTS input
 *	Bit 5: Complement of DSR input
 * 	Bit 6: Complement of RI input
 * 	Bit 7: Complement of DCD intput
 * -----------------------------------------------------------------
 */
static uint32_t
msr_read(void *clientData, uint32_t address, int rqlen)
{
	PC16550 *pc = clientData;
	uint8_t msr;
	UartCmd cmd;
	msr = pc->msr;
	msr = msr & ~(MSR_DCD | MSR_RI | MSR_DSR | MSR_CTS);

	/* Ask the backend */
	cmd.arg = 0;
	cmd.opcode = UART_OPC_GET_DCD;
	SerialDevice_Cmd(pc->backend, &cmd);
	if (cmd.retval) {
		msr |= MSR_DCD;
	}

	cmd.opcode = UART_OPC_GET_RI;
	SerialDevice_Cmd(pc->backend, &cmd);
	if (cmd.retval) {
		msr |= MSR_RI;
	}

	cmd.opcode = UART_OPC_GET_DSR;
	SerialDevice_Cmd(pc->backend, &cmd);
	if (cmd.retval) {
		msr |= MSR_DSR;
	}

	cmd.opcode = UART_OPC_GET_CTS;
	SerialDevice_Cmd(pc->backend, &cmd);
	if (cmd.retval) {
		msr |= MSR_CTS;
	}

	/* Now determine the deltas */
	if ((pc->msr ^ msr) & MSR_DCD) {
		msr |= MSR_DDCD;
	}
	if ((pc->msr ^ msr) & MSR_RI) {
		msr |= MSR_TERI;
	}
	if ((pc->msr ^ msr) & MSR_DSR) {
		msr |= MSR_DDSR;
	}
	if ((pc->msr ^ msr) & MSR_CTS) {
		msr |= MSR_DCTS;
	}
	pc->msr = msr & ~(MSR_DCTS | MSR_DDSR | MSR_TERI | MSR_DDCD);
	if ((pc->ier & IER_EDSSI) && (pc->msr != msr)) {
		update_interrupts(pc);
	}
	return msr;
}

static void
msr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	return;
}

/*
 * ------------------------------------------------
 * Scratchpad register
 * ------------------------------------------------
 */
static uint32_t
scr_read(void *clientData, uint32_t address, int rqlen)
{
	PC16550 *pc = clientData;
	return pc->scr;
}

static void
scr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	PC16550 *pc = clientData;
	pc->scr = value;
	return;
}

#define ABS_ADDR(pc,base,reg) ((base) + ((reg)<<pc->register_shift))

static void
PC16550_Map(void *owner, uint32_t base, uint32_t mask, uint32_t flags)
{
	PC16550 *pc = owner;
	IOH_New8(ABS_ADDR(pc, base, RBR(pc)), reg0_read, reg0_write, pc);
	IOH_New8(ABS_ADDR(pc, base, IER(pc)), reg1_read, reg1_write, pc);
	IOH_New8(ABS_ADDR(pc, base, IIR(pc)), iir_read, fcr_write, pc);
	IOH_New8(ABS_ADDR(pc, base, LCR(pc)), lcr_read, lcr_write, pc);
	IOH_New8(ABS_ADDR(pc, base, MCR(pc)), mcr_read, mcr_write, pc);
	IOH_New8(ABS_ADDR(pc, base, LSR(pc)), lsr_read, lsr_write, pc);
	IOH_New8(ABS_ADDR(pc, base, MSR(pc)), msr_read, msr_write, pc);
	IOH_New8(ABS_ADDR(pc, base, SCR(pc)), scr_read, scr_write, pc);

}

static void
PC16550_UnMap(void *owner, uint32_t base, uint32_t mask)
{
	PC16550 *pc = owner;
	int i;
	for (i = 0; i < 8; i++) {
		IOH_Delete8(ABS_ADDR(pc, base, i));
	}
}

/*
 * ---------------------------------------------------
 * PC16550_New
 * 	Create a new National semicondcutor PC16650
 *      register_shift defines the space between
 *	to registers of 16550 in powers of two.
 * ---------------------------------------------------
 */
BusDevice *
PC16550_New(char *devname, int register_shift)
{
	PC16550 *pc = sg_new(PC16550);
	pc->register_shift = register_shift;
	pc->bdev.first_mapping = NULL;
	pc->bdev.Map = PC16550_Map;
	pc->bdev.UnMap = PC16550_UnMap;
	pc->bdev.owner = pc;
	pc->bdev.hw_flags = MEM_FLAG_WRITABLE | MEM_FLAG_READABLE;
	pc->backend = Uart_New(devname, serial_input, serial_output, NULL, pc);
	pc->rx_fifo_size = pc->tx_fifo_size = 1;
	pc->clk_in = Clock_New("%s.clk", devname);
	pc->clk_baud = Clock_New("%s.baud_clk", devname);
	if (!pc->clk_in || !pc->clk_baud) {
		fprintf(stderr, "Can not create baud clocks for \"%s\"\n", devname);
		exit(1);
	}
	pc->irqNode = SigNode_New("%s.irq", devname);
	if (!pc->irqNode) {
		fprintf(stderr, "Can not create interrupt signal for \"%s\"\n", devname);
	}
	SigNode_Set(pc->irqNode, SIG_HIGH);	/* No request on startup */
	pc->interrupt_posted = 0;
	Clock_Trace(pc->clk_baud, baud_clock_trace, pc);
	fprintf(stderr, "Created 16550 UART \"%s\"\n", devname);
	return &pc->bdev;
}
