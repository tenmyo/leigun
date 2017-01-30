/*
 *************************************************************************************************
 * Emulation of Hilscher NetX Serial UART 
 *
 * state: Working with u-boot and linux-2.6.30.7 
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
#include "signode.h"
#include "serial.h"
#include "clock.h"
#include "netx_uart.h"
#include "senseless.h"

/* Uart register definitions are taken from the linux kernel driver */

enum uart_regs {
	UART_DR = 0x00,
	UART_SR = 0x04,
	UART_LINE_CR = 0x08,
	UART_BAUDDIV_MSB = 0x0c,
	UART_BAUDDIV_LSB = 0x10,
	UART_CR = 0x14,
	UART_FR = 0x18,
	UART_IIR = 0x1c,
	UART_ILPR = 0x20,
	UART_RTS_CR = 0x24,
	UART_RTS_LEAD = 0x28,
	UART_RTS_TRAIL = 0x2c,
	UART_DRV_ENABLE = 0x30,
	UART_BRM_CR = 0x34,
	UART_RXFIFO_IRQLEVEL = 0x38,
	UART_TXFIFO_IRQLEVEL = 0x3c,
};

#define SR_FE (1<<0)
#define SR_PE (1<<1)
#define SR_BE (1<<2)
#define SR_OE (1<<3)

#define LINE_CR_BRK       (1<<0)
#define LINE_CR_PEN       (1<<1)
#define LINE_CR_EPS       (1<<2)
#define LINE_CR_STP2      (1<<3)
#define LINE_CR_FEN       (1<<4)
#define LINE_CR_5BIT      (0<<5)
#define LINE_CR_6BIT      (1<<5)
#define LINE_CR_7BIT      (2<<5)
#define LINE_CR_8BIT      (3<<5)
#define LINE_CR_BITS_MASK (3<<5)

#define CR_UART_EN (1<<0)
#define CR_SIREN   (1<<1)
#define CR_SIRLP   (1<<2)
#define CR_MSIE    (1<<3)
#define CR_RIE     (1<<4)
#define CR_TIE     (1<<5)
#define CR_RTIE    (1<<6)
#define CR_LBE     (1<<7)

#define FR_CTS  (1<<0)
#define FR_DSR  (1<<1)
#define FR_DCD  (1<<2)
#define FR_BUSY (1<<3)
#define FR_RXFE (1<<4)
#define FR_TXFF (1<<5)
#define FR_RXFF (1<<6)
#define FR_TXFE (1<<7)

#define IIR_MIS		(1<<0)
#define IIR_RIS		(1<<1)
#define IIR_TIS		(1<<2)
#define IIR_RTIS	(1<<3)
#define IIR_MASK	0xf

#define RTS_CR_AUTO	(1<<0)
#define RTS_CR_RTS	(1<<1)
#define RTS_CR_COUNT	(1<<2)
#define RTS_CR_MOD2	(1<<3)
#define RTS_CR_RTS_POL	(1<<4)
#define RTS_CR_CTS_CTR	(1<<5)
#define RTS_CR_CTS_POL	(1<<6)
#define RTS_CR_STICK	(1<<7)

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

typedef struct NetXUart {
	BusDevice bdev;
	UartPort *backend;
	Clock_t *clk_in;
	Clock_t *clk_baud;

	int baudrate;
	uint32_t reg_DR;
	uint32_t reg_SR;
	uint32_t reg_LINE_CR;
	uint32_t reg_BAUDDIV_MSB;
	uint32_t reg_BAUDDIV_LSB;
	uint32_t reg_CR;
	uint32_t reg_FR;
	uint32_t reg_ISR;
	uint32_t reg_ILPR;
	uint32_t reg_RTS_CR;
	uint32_t reg_RTS_LEAD;
	uint32_t reg_RTS_TRAIL;
	uint32_t reg_DRV_ENABLE;
	uint32_t reg_BRM_CR;
	uint32_t reg_RXFIFO_IRQLEVEL;
	uint32_t reg_TXFIFO_IRQLEVEL;

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
} NetXUart;

static void
update_interrupts(NetXUart * nxua)
{
	if (nxua->reg_ISR & (nxua->reg_CR >> 3) & IIR_MASK) {
		if (!nxua->interrupt_posted) {
			//fprintf(stderr,"Post int CR %08x, iir %08x\n",nxua->reg_CR,nxua->reg_ISR);
			SigNode_Set(nxua->irqNode, SIG_LOW);
			nxua->interrupt_posted = 1;
		}
	} else {
		if (nxua->interrupt_posted) {
			//fprintf(stderr,"UnPost int CR %08x, iir %08x\n",nxua->reg_CR,nxua->reg_ISR);
			SigNode_Set(nxua->irqNode, SIG_HIGH);
			nxua->interrupt_posted = 0;
		}
	}
}

static void
update_clock(NetXUart * nxua)
{
	uint32_t bauddiv;
	bauddiv = nxua->reg_BAUDDIV_LSB + (nxua->reg_BAUDDIV_MSB << 8);
	if (nxua->reg_BRM_CR & 1) {
		Clock_MakeDerived(nxua->clk_baud, nxua->clk_in, bauddiv, 1024 * 1024);
	} else {
		Clock_MakeDerived(nxua->clk_baud, nxua->clk_in, 1, 16 * (bauddiv + 1));
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
	NetXUart *nxua = (NetXUart *) clientData;
	UartCmd cmd;
	cmd.opcode = UART_OPC_SET_BAUDRATE;
	cmd.arg = Clock_Freq(clock);
	/* Dont forget to update the baudrate when uart is enabled */
	if (nxua->reg_CR & CR_UART_EN) {
		SerialDevice_Cmd(nxua->backend, &cmd);
	}
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
	NetXUart *nxua = cd;
	if (TX_FIFO_COUNT(nxua) > 0) {
		*c = nxua->tx_fifo[TX_FIFO_RP(nxua)];
		nxua->txfifo_rp++;
	} else {
		fprintf(stderr, "Bug in %s %s\n", __FILE__, __func__);
	}
	if (TX_FIFO_COUNT(nxua) == 0) {
		nxua->reg_FR |= FR_TXFE;
		nxua->reg_FR &= ~(FR_BUSY | FR_TXFF);
		nxua->reg_ISR |= IIR_TIS;
		update_interrupts(nxua);
		SerialDevice_StopTx(nxua->backend);
	} else if (TX_FIFO_ROOM(nxua) > 0) {
		/* No longer full */
		if ((TX_FIFO_SIZE(nxua) == 1) || (TX_FIFO_ROOM(nxua) <= nxua->reg_TXFIFO_IRQLEVEL)) {
			nxua->reg_ISR |= IIR_TIS;
		}
		nxua->reg_FR &= ~FR_TXFF;
		update_interrupts(nxua);
	}
	return true;
}

/*
 ************************************************************
 * Put one byte to the rxfifo
 ************************************************************
 */
static inline int
serial_put_rx_fifo(NetXUart * nxua, uint8_t c)
{
	int room = RX_FIFO_ROOM(nxua);
	if (room < 1) {
		return -1;
	}
	nxua->rx_fifo[RX_FIFO_WP(nxua)] = c;
	nxua->rxfifo_wp++;
	if (room == 1) {
		SerialDevice_StopRx(nxua->backend);
		return 0;
	}
	return 1;
}

static void
serial_input(void *cd, UartChar c)
{
	NetXUart *nxua = cd;
	int fifocount;
	serial_put_rx_fifo(nxua, c);
	fifocount = RX_FIFO_COUNT(nxua);
	if (fifocount) {
		nxua->reg_FR &= ~FR_RXFE;
		/* Timer for receive timeout missing */
		nxua->reg_ISR |= IIR_RTIS;
		if (RX_FIFO_SIZE(nxua) == 1) {
			nxua->reg_ISR |= IIR_RIS;
		} else if (fifocount >= nxua->reg_RXFIFO_IRQLEVEL) {
			nxua->reg_ISR |= IIR_RIS;
		}
		update_interrupts(nxua);
	}
}

static void
reset_rx_fifo(NetXUart * nxua)
{
	nxua->rxfifo_rp = nxua->rxfifo_wp = 0;
	nxua->reg_FR |= FR_RXFE;
	nxua->reg_FR &= ~FR_RXFF;
	nxua->reg_ISR &= ~IIR_RIS;
	update_interrupts(nxua);
}

static void
reset_tx_fifo(NetXUart * nxua)
{
	nxua->txfifo_rp = nxua->txfifo_wp = 0;
	nxua->reg_FR |= FR_TXFE;
	nxua->reg_FR &= ~(FR_TXFF | FR_BUSY);
	nxua->reg_ISR &= ~IIR_TIS;
	update_interrupts(nxua);
}

static void
update_serconfig(NetXUart * nxua)
{
	UartCmd cmd;
	tcflag_t bits;
	tcflag_t parodd;
	tcflag_t parenb;
	tcflag_t crtscts;
	crtscts = 0;
	if (nxua->reg_LINE_CR & LINE_CR_EPS) {
		parodd = 0;
	} else {
		parodd = 1;
	}
	if (nxua->reg_LINE_CR & LINE_CR_PEN) {
		parenb = 1;
	} else {
		parenb = 0;
	}
	switch (nxua->reg_LINE_CR & LINE_CR_BITS_MASK) {
	    case LINE_CR_5BIT:
		    bits = 5;
		    break;
	    case LINE_CR_6BIT:
		    bits = 6;
		    break;
	    case LINE_CR_7BIT:
		    bits = 7;
		    break;
	    case LINE_CR_8BIT:
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
		SerialDevice_Cmd(nxua->backend, &cmd);
	} else {
		cmd.opcode = UART_OPC_CRTSCTS;
		cmd.arg = 0;
		SerialDevice_Cmd(nxua->backend, &cmd);

		cmd.opcode = UART_OPC_SET_RTS;
		/* Set the initial state of RTS */
#if 0
		if (iuart->ucr2 & UCR2_CTS) {
			cmd.arg = UART_RTS_ACT;
		} else {
			cmd.arg = UART_RTS_INACT;
		}
		SerialDevice_Cmd(nxua->backend, &cmd);
#endif
	}
	cmd.opcode = UART_OPC_PAREN;
	cmd.arg = parenb;
	SerialDevice_Cmd(nxua->backend, &cmd);

	cmd.opcode = UART_OPC_PARODD;
	cmd.arg = parodd;
	SerialDevice_Cmd(nxua->backend, &cmd);

	cmd.opcode = UART_OPC_SET_CSIZE;
	cmd.arg = bits;
	SerialDevice_Cmd(nxua->backend, &cmd);
}

/**
 ************************************************************
 * \fn dr_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
 * Write one character to the TX-Fifo
 ************************************************************
 */
static void
dr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	NetXUart *nxua = (NetXUart *) clientData;
	int room;
	uint8_t old_fr;
	if (TX_FIFO_ROOM(nxua) > 0) {
		nxua->tx_fifo[TX_FIFO_WP(nxua)] = value;
		nxua->txfifo_wp++;
	}
	old_fr = nxua->reg_FR;
	nxua->reg_FR &= ~FR_TXFE;
	nxua->reg_FR |= FR_BUSY;
	room = TX_FIFO_ROOM(nxua);
	if (room > 0) {
		nxua->reg_FR &= ~FR_TXFF;
		if ((TX_FIFO_SIZE(nxua) == 1) || (room >= nxua->reg_TXFIFO_IRQLEVEL)) {
			nxua->reg_ISR |= IIR_TIS;
		}
	} else {
		nxua->reg_ISR |= IIR_TIS;
		nxua->reg_FR |= FR_TXFF;
	}
	update_interrupts(nxua);
	SerialDevice_StartTx(nxua->backend);
}

/*
 * ----------------------------------------
 * Fetch a byte from RX-Fifo
 * ----------------------------------------
 */
static uint32_t
dr_read(void *clientData, uint32_t address, int rqlen)
{
	NetXUart *nxua = (NetXUart *) clientData;
	uint32_t data = 0;
	if (!(nxua->reg_CR & CR_UART_EN)) {
		return 0;
	}
	if (RX_FIFO_COUNT(nxua) > 0) {
		data = nxua->rx_fifo[RX_FIFO_RP(nxua)];
		nxua->rxfifo_rp++;
		if (RX_FIFO_COUNT(nxua) < nxua->reg_RXFIFO_IRQLEVEL) {
			nxua->reg_ISR &= ~IIR_RIS;
			update_interrupts(nxua);
		}
	}
	if (RX_FIFO_COUNT(nxua) == 0) {
		nxua->reg_ISR &= ~IIR_RIS;
		nxua->reg_ISR &= ~IIR_RTIS;
		nxua->reg_FR |= FR_RXFE;
		update_interrupts(nxua);
	}
	SerialDevice_StartRx(nxua->backend);
	return data;
}

/*
 */
static uint32_t
sr_read(void *clientData, uint32_t address, int rqlen)
{
	NetXUart *nxua = clientData;
	return nxua->reg_SR;
}

static void
sr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	NetXUart *nxua = clientData;
	fprintf(stderr, "NetXUart: Status register write\n");
	nxua->reg_SR = value;
}

/* 
 ***************************************************************************
 * Line Control Register
 ***************************************************************************
 */
static uint32_t
line_cr_read(void *clientData, uint32_t address, int rqlen)
{
	NetXUart *nxua = clientData;
	return nxua->reg_LINE_CR;
}

static void
line_cr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	NetXUart *nxua = clientData;
	nxua->reg_LINE_CR = value;
	update_serconfig(nxua);
	return;
}

/* 
 **************************************************
 * Divisor latch 
 **************************************************
 */
static inline void
bauddiv_lsb_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	NetXUart *nxua = clientData;
	nxua->reg_BAUDDIV_LSB = value;
	update_clock(nxua);
}

static uint32_t
bauddiv_lsb_read(void *clientData, uint32_t address, int rqlen)
{
	NetXUart *nxua = clientData;
	return nxua->reg_BAUDDIV_LSB;
}

static inline void
bauddiv_msb_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	NetXUart *nxua = clientData;
	nxua->reg_BAUDDIV_MSB = value;
	update_clock(nxua);

}

static uint32_t
bauddiv_msb_read(void *clientData, uint32_t address, int rqlen)
{
	NetXUart *nxua = clientData;
	return nxua->reg_BAUDDIV_MSB;
}

/*
 **************************************************************************
 * Control Register 
 *	Bit 0: 	Uart-Enable
 * 	Bit 1:  SIR-Enable
 *	Bit 2: 	SIRLP
 *	Bit 3: 	Receive Data Interrupt and Timeout	
 *	Bit 4:  Transmit hold register Empty interrupt enable
 *	Bit 5:  Receive Line status interrupt enable
 *	Bit 6: 	Modem status change interrupt enable
 * 	Bit 7:  Loop back enable
 **************************************************************************
 */
static uint32_t
cr_read(void *clientData, uint32_t address, int rqlen)
{
	NetXUart *nxua = clientData;
	return nxua->reg_CR;
}

static void
cr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	NetXUart *nxua = clientData;
	uint32_t diff = nxua->reg_CR ^ value;

	nxua->reg_CR = value;
	update_interrupts(nxua);
	if (value & diff & CR_UART_EN) {
		reset_rx_fifo(nxua);
		reset_tx_fifo(nxua);
		baud_clock_trace(nxua->clk_baud, nxua);
		SerialDevice_StartRx(nxua->backend);
	}
	return;
}

/*
 ********************************************************************* 
 *  Uart flag register
 * 	Bit 7: TXFE  TX-Fifo empty 
 *	Bit 6: RXFF  RX-Fifo full
 *	Bit 5: TXFF  TX-Fifo full
 * 	Bit 4: RXFE  RX-Fifo empty 
 *	Bit 3: Busy
 * 	Bit 2: Complement of DCD intput (Not supported)
 *	Bit 1: Complement of DSR input (Not supported)
 *	Bit 0: Complement of CTS input
 ********************************************************************* 
 */
static uint32_t
fr_read(void *clientData, uint32_t address, int rqlen)
{
	NetXUart *nxua = clientData;
	uint32_t fr;
	UartCmd cmd;
	fr = nxua->reg_FR;
	/* Ask the backend */
	cmd.opcode = UART_OPC_GET_CTS;
	SerialDevice_Cmd(nxua->backend, &cmd);
	if (cmd.retval) {
		fr |= FR_CTS;
	} else {
		fr = fr & ~FR_CTS;
	}
	Senseless_Report(150);
	return fr;
}

static void
fr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "Uart Flag register is not writable\n");
	return;
}

/*
 ****************************************************************
 * Interrupt Identification Register 
 *	Bit 0: MIS Mode Status Interrupt	
 *	Bit 1: RIS Receive interrupt status
 *	Bit 2: TIS Transmit interrupt status	
 * 	Bit 3: Receive timeout interrupt status
 ***************************************************************
 */
static uint32_t
iir_read(void *clientData, uint32_t address, int rqlen)
{
	NetXUart *nxua = clientData;
	uint32_t iir = (nxua->reg_ISR & (nxua->reg_CR >> 3) & IIR_MASK);
	return iir;
}

static void
iir_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	NetXUart *nxua = clientData;
	if (nxua->reg_ISR & IIR_MIS) {
		nxua->reg_ISR &= ~IIR_MIS;
		update_interrupts(nxua);
	}
	return;
}

/*
 ************************************************************
 * IrDA low power counter register
 * not yet implemented
 ************************************************************
 */
static uint32_t
ilpr_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "IrDA low power counter not implemented\n");
	return 0;
}

static void
ilpr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "IrDA low power counter not implemented\n");
}

/*
 ***********************************************************************
 * Uart RTS control register
 * Bit 7: STICK Parity works as stick
 * Bit 6: CTS_POL CTS polarity
 * Bit 5: CTS_CTR CTS control
 * Bit 4: RTS_POL RTS polarity
 * Bit 3: MOD2 go into Trail state after every char or after firo is empty
 * Bit 2: COUNT select the RTS counter timer base 
 * Bit 1: RTS When auto is 0 rts is set by this bit
 * Bit 0: AUTO RTS output done automatically/manually
 ***********************************************************************
 */
static uint32_t
rts_cr_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "RTS CR read not implemented\n");
	return 0;
}

static void
rts_cr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "RTS CR write not implemented\n");
}

/*
 *****************************************************
 * RTS leading cycles register
 *****************************************************
 */
static uint32_t
rts_lead_read(void *clientData, uint32_t address, int rqlen)
{
	NetXUart *nxua = clientData;
	return nxua->reg_RTS_LEAD;
}

static void
rts_lead_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	NetXUart *nxua = clientData;
	nxua->reg_RTS_LEAD = value & 0xff;
}

/*
 *****************************************************
 * RTS trailing cycles register
 *****************************************************
 */
static uint32_t
rts_trail_read(void *clientData, uint32_t address, int rqlen)
{
	NetXUart *nxua = clientData;
	return nxua->reg_RTS_TRAIL;
}

static void
rts_trail_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	NetXUart *nxua = clientData;
	nxua->reg_RTS_TRAIL = value & 0xff;
}

/*
 **********************************************************************
 * DRV_EN
 * Bit 1: DRVRTS Enables the driver for the UARTi_RTS output pin
 * Bit 0: DRVTX Enables the driver for the UARTi_TXD output pin
 **********************************************************************
 */
static uint32_t
drv_enable_read(void *clientData, uint32_t address, int rqlen)
{
	NetXUart *nxua = clientData;
	fprintf(stderr, "NetXUart drive enable is not implemented\n");
	return nxua->reg_DRV_ENABLE;
}

static void
drv_enable_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	NetXUart *nxua = clientData;
	fprintf(stderr, "NetXUart drive enable is not implemented\n");
	nxua->reg_DRV_ENABLE = value & 3;
}

/*
 * Baud rate mode control
 */
static uint32_t
brm_cr_read(void *clientData, uint32_t address, int rqlen)
{
	NetXUart *nxua = clientData;
	return nxua->reg_BRM_CR;
}

static void
brm_cr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	NetXUart *nxua = clientData;
	nxua->reg_BRM_CR = value & 1;
	update_clock(nxua);
}

/*
 ***********************************************************************
 * Set the trigger level for the RX irq
 ***********************************************************************
 */
static uint32_t
rxfifo_irqlevel_read(void *clientData, uint32_t address, int rqlen)
{
	NetXUart *nxua = clientData;
	return nxua->reg_RXFIFO_IRQLEVEL;
}

static void
rxfifo_irqlevel_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	NetXUart *nxua = clientData;
	nxua->reg_RXFIFO_IRQLEVEL = value & 0x1f;
	// update the status  of the interrupts
}

/**
 ******************************************************************
 * Set the trigger level for the tx irq
 ******************************************************************
 */
static uint32_t
txfifo_irqlevel_read(void *clientData, uint32_t address, int rqlen)
{
	NetXUart *nxua = clientData;
	return nxua->reg_TXFIFO_IRQLEVEL;
}

static void
txfifo_irqlevel_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	NetXUart *nxua = clientData;
	nxua->reg_TXFIFO_IRQLEVEL = value & 0x1f;
	// update the status  of the interrupts
}

#define ABS_ADDR(base,reg) ((base) + (reg))

static void
NetXUart_Map(void *owner, uint32_t base, uint32_t mask, uint32_t flags)
{
	NetXUart *nxua = (NetXUart *) owner;
	IOH_New32(ABS_ADDR(base, UART_DR), dr_read, dr_write, nxua);
	IOH_New32(ABS_ADDR(base, UART_SR), sr_read, sr_write, nxua);
	IOH_New32(ABS_ADDR(base, UART_LINE_CR), line_cr_read, line_cr_write, nxua);
	IOH_New32(ABS_ADDR(base, UART_BAUDDIV_MSB), bauddiv_msb_read, bauddiv_msb_write, nxua);
	IOH_New32(ABS_ADDR(base, UART_BAUDDIV_LSB), bauddiv_lsb_read, bauddiv_lsb_write, nxua);
	IOH_New32(ABS_ADDR(base, UART_CR), cr_read, cr_write, nxua);
	IOH_New32(ABS_ADDR(base, UART_FR), fr_read, fr_write, nxua);
	IOH_New32(ABS_ADDR(base, UART_IIR), iir_read, iir_write, nxua);
	IOH_New32(ABS_ADDR(base, UART_ILPR), ilpr_read, ilpr_write, nxua);
	IOH_New32(ABS_ADDR(base, UART_RTS_CR), rts_cr_read, rts_cr_write, nxua);
	IOH_New32(ABS_ADDR(base, UART_RTS_LEAD), rts_lead_read, rts_lead_write, nxua);
	IOH_New32(ABS_ADDR(base, UART_RTS_TRAIL), rts_trail_read, rts_trail_write, nxua);
	IOH_New32(ABS_ADDR(base, UART_DRV_ENABLE), drv_enable_read, drv_enable_write, nxua);
	IOH_New32(ABS_ADDR(base, UART_BRM_CR), brm_cr_read, brm_cr_write, nxua);
	IOH_New32(ABS_ADDR(base, UART_RXFIFO_IRQLEVEL), rxfifo_irqlevel_read, rxfifo_irqlevel_write,
		  nxua);
	IOH_New32(ABS_ADDR(base, UART_TXFIFO_IRQLEVEL), txfifo_irqlevel_read, txfifo_irqlevel_write,
		  nxua);

}

static void
NetXUart_UnMap(void *owner, uint32_t base, uint32_t mask)
{
	int i;
	for (i = 0; i < 16; i++) {
		IOH_Delete32(ABS_ADDR(base, i));
	}
}

/*
 **********************************************************
 * NetXUart_New
 * 	Create a new NetX UART. 
 **********************************************************
 */
BusDevice *
NetXUart_New(const char *devname)
{
	NetXUart *nxua = sg_new(NetXUart);
	nxua->bdev.first_mapping = NULL;
	nxua->bdev.Map = NetXUart_Map;
	nxua->bdev.UnMap = NetXUart_UnMap;
	nxua->bdev.owner = nxua;
	nxua->bdev.hw_flags = MEM_FLAG_WRITABLE | MEM_FLAG_READABLE;
	nxua->backend = Uart_New(devname, serial_input, serial_output, NULL, nxua);
	nxua->rx_fifo_size = nxua->tx_fifo_size = 1;
	nxua->reg_DR = 0;
	nxua->reg_SR = 0;
	nxua->reg_LINE_CR = 0;
	nxua->reg_BAUDDIV_MSB = 0;
	nxua->reg_BAUDDIV_LSB = 0;
	nxua->reg_CR = 0;
	nxua->reg_FR = 0;
	nxua->reg_ISR = 0;
	nxua->reg_ILPR = 0;
	nxua->reg_RTS_CR = 0;
	nxua->reg_RTS_LEAD = 0;
	nxua->reg_RTS_TRAIL = 0;
	nxua->reg_DRV_ENABLE = 0;
	nxua->reg_BRM_CR = 0;
	nxua->reg_RXFIFO_IRQLEVEL = 8;
	nxua->reg_TXFIFO_IRQLEVEL = 8;
	nxua->clk_in = Clock_New("%s.clk", devname);
	nxua->clk_baud = Clock_New("%s.baud_clk", devname);
	if (!nxua->clk_in || !nxua->clk_baud) {
		fprintf(stderr, "Can not create baud clocks for %s\n", devname);
		exit(1);
	}
	nxua->irqNode = SigNode_New("%s.irq", devname);
	if (!nxua->irqNode) {
		fprintf(stderr, "Can not create interrupt signal for %s\n", devname);
	}
	SigNode_Set(nxua->irqNode, SIG_HIGH);	/* No request on startup */
	nxua->interrupt_posted = 0;
	Clock_Trace(nxua->clk_baud, baud_clock_trace, nxua);
	fprintf(stderr, "Created NetX UART \"%s\"\n", devname);
	Clock_SetFreq(nxua->clk_in, 100000000);
	return &nxua->bdev;
}
