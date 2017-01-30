/*
 *************************************************************************************************
 *
 * Emulation of ATXMega A USART
 *
 * State: nothing implemented
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

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "sgstring.h"
#include "avr8_cpu.h"
#include "serial.h"
#include "clock.h"
#include "cycletimer.h"
#include "signode.h"
#include "xmegaA_usart.h"

#define REG_DATA(base)		((base) + 0)
#define REG_STATUS(base)	((base) + 1)
#define		STATUS_RXCIF	(1 << 7)
#define		STATUS_TXCIF	(1 << 6)
#define		STATUS_DREIF	(1 << 5)
#define 	STATUS_FERR	(1 << 4)
#define		STATUS_BUFOV	(1 << 3)
#define		STATUS_PERR	(1 << 2)
#define		STATUS_RXB8	(1 << 1)

#define REG_CTRLA(base)		((base) + 3)
#define		CTRLA_RXINTLVL_MASK	(3 << 4)
#define		CTRLA_RXINTLVL_SHIFT	(4)
#define		CTRLA_TXINTLVL_MASK	(3 << 2)
#define		CTRLA_TXINTLVL_SHIFT	(2)
#define		CTRLA_DREINTLVL_MASK	(3 << 0)
#define		CTRLA_DREINTLVL_SHIFT	(0)
#define REG_CTRLB(base)		((base) + 4)
#define		CTRLB_RXEN	(1 << 4)
#define		CTRLB_TXEN	(1 << 3)
#define		CTRLB_CLK2X	(1 << 2)
#define		CTRLB_MPCM	(1 << 1)
#define		CTRLB_TXB8	(1 << 0)

#define REG_CTRLC(base)		((base) + 5)
#define		CTRLC_CMODE_ASYNC	(0 << 6)
#define		CTRLC_CMODE_SYNC	(1 << 6)
#define		CTRLC_CMODE_IRCOM	(2 << 6)
#define		CTRLC_CMODE_MSPI	(3 << 6)
#define		CTRLC_PMODE_DISABLED	(0 << 4)
#define		CTRLC_PMODE_EVEN	(2 << 4)
#define		CTRLC_PMODE_ODD		(3 << 4)
#define		CTRLC_SBMODE		(1 << 3)
#define		CTRLC_SBMODE_1BIT	(0 << 3)
#define		CTRLC_SBMODE_2BIT	(1 << 3)
#define		CTRLC_CHSIZE_5BIT	(0 << 0)
#define		CTRLC_CHSIZE_6BIT	(1 << 0)
#define		CTRLC_CHSIZE_7BIT	(2 << 0)
#define		CTRLC_CHSIZE_8BIT	(3 << 0)
#define		CTRLC_CHSIZE_9BIT	(7 << 0)
#define REG_BAUDCTRLA(base)	((base) + 6)
#define REG_BAUDCTRLB(base)	((base) + 7)

typedef struct XMegaUsart {
	UartPort *backend;
	SigNode *txcIrq;	/* TX-Complete Interrupt */
	SigNode *rxcIrq;	/* RX-Complete Interrupt */
	SigNode *dreIrq;	/* Data Register Empty Flag */
	uint8_t regTxb;
	UartChar txShiftReg;
	uint8_t regRxb;		/* Read upper bit from status reg */
	uint8_t regStatus;
	uint8_t regCtrlA;
	uint8_t regCtrlB;
	uint8_t regCtrlC;
	uint8_t regBaudCtrlA;
	uint8_t regBaudCtrlB;
	CycleTimer tx_baud_timer;
	CycleTimer rx_baud_timer;
	CycleCounter_t byte_time;
} XMegaUsart;

static void
update_txc_interrupt(XMegaUsart * usart)
{
#if 0
#define		CTRLA_RXINTLVL_MASK	(3 << 4)
#define		CTRLA_RXINTLVL_SHIFT	(4)
#define		CTRLA_TXINTLVL_MASK	(3 << 2)
#define		CTRLA_TXINTLVL_SHIFT	(2)
#define		CTRLA_DREINTLVL_MASK	(3 << 0)
#define		CTRLA_DREINTLVL_SHIFT	(0)
#endif
	if (usart->regStatus & STATUS_TXCIF) {
		SigNode_Set(usart->txcIrq, SIG_LOW);
	} else {
		SigNode_Set(usart->txcIrq, SIG_OPEN);
	}
}

static void
update_rxc_interrupt(XMegaUsart * usart)
{
	if (usart->regStatus & STATUS_RXCIF) {
		SigNode_Set(usart->rxcIrq, SIG_LOW);
	} else {
		SigNode_Set(usart->rxcIrq, SIG_OPEN);
	}
}

static void
update_dre_interrupt(XMegaUsart * usart)
{
	if (usart->regStatus & STATUS_DREIF) {
		SigNode_Set(usart->dreIrq, SIG_LOW);
	} else {
		SigNode_Set(usart->dreIrq, SIG_OPEN);
	}

}

static void
tx_done(void *clientData)
{
	XMegaUsart *usart = clientData;
	SerialDevice_StartTx(usart->backend);
}

static bool
serial_output(void *cd, UartChar * c)
{
	XMegaUsart *usart = cd;
	*c = usart->txShiftReg;
	if (!(usart->regStatus & STATUS_DREIF)) {
		usart->txShiftReg = usart->regTxb | ((usart->regCtrlB & 1) << 8);
		usart->regStatus |= STATUS_DREIF;
		update_dre_interrupt(usart);
		CycleTimer_Mod(&usart->tx_baud_timer, usart->byte_time);
	} else {
		usart->regStatus |= STATUS_TXCIF;
		update_txc_interrupt(usart);
	}
	SerialDevice_StopTx(usart->backend);
	return true;
}

static void
serial_input(void *cd, UartChar c)
{
	XMegaUsart *usart = cd;
	usart->regRxb = c;
	usart->regStatus |= STATUS_RXCIF;
	if (c & (1 << 8)) {
		usart->regStatus |= STATUS_RXB8;
	} else {
		usart->regStatus &= ~STATUS_RXB8;
	}
	update_rxc_interrupt(usart);
	CycleTimer_Mod(&usart->rx_baud_timer, usart->byte_time);
	SerialDevice_StopRx(usart->backend);
}

static uint8_t
data_read(void *clientData, uint32_t address)
{
	XMegaUsart *usart = clientData;
	uint8_t value = usart->regRxb;

	usart->regStatus &= ~STATUS_RXCIF;
	update_rxc_interrupt(usart);
	return value;
}

static void
data_write(void *clientData, uint8_t value, uint32_t address)
{

	XMegaUsart *usart = clientData;
	usart->regTxb = value;
	if (!CycleTimer_IsActive(&usart->tx_baud_timer)) {
		usart->txShiftReg = usart->regTxb | ((usart->regCtrlB & 1) << 8);
		CycleTimer_Mod(&usart->tx_baud_timer, usart->byte_time);
	} else {
		usart->regStatus &= ~STATUS_DREIF;
	}
	//fprintf(stderr,"XMega UART: %s \"%c\" not implemented\n",__func__,value);
}

static uint8_t
status_read(void *clientData, uint32_t address)
{
	XMegaUsart *usart = clientData;
	return usart->regStatus;
}

static void
status_write(void *clientData, uint8_t value, uint32_t address)
{
	fprintf(stderr, "XMega UART: %s not implemented\n", __func__);
}

/*
 */
static uint8_t
ctrla_read(void *clientData, uint32_t address)
{
	fprintf(stderr, "XMega UART: %s not implemented\n", __func__);
	return 0;
}

static void
ctrla_write(void *clientData, uint8_t value, uint32_t address)
{
	fprintf(stderr, "XMega UART: %s not implemented\n", __func__);
}

static uint8_t
ctrlb_read(void *clientData, uint32_t address)
{
	fprintf(stderr, "XMega UART: %s not implemented\n", __func__);
	return 0;
}

static void
ctrlb_write(void *clientData, uint8_t value, uint32_t address)
{
	fprintf(stderr, "XMega UART: %s not implemented\n", __func__);
}

static uint8_t
ctrlc_read(void *clientData, uint32_t address)
{
	fprintf(stderr, "XMega UART: %s not implemented\n", __func__);
	return 0;
}

static void
ctrlc_write(void *clientData, uint8_t value, uint32_t address)
{
	fprintf(stderr, "XMega UART: %s not implemented\n", __func__);
}

static uint8_t
baudctrla_read(void *clientData, uint32_t address)
{
	fprintf(stderr, "XMega UART: %s not implemented\n", __func__);
	return 0;
}

static void
baudctrla_write(void *clientData, uint8_t value, uint32_t address)
{
	fprintf(stderr, "XMega UART: %s not implemented\n", __func__);
}

static uint8_t
baudctrlb_read(void *clientData, uint32_t address)
{
	fprintf(stderr, "XMega UART: %s not implemented\n", __func__);
	return 0;
}

static void
baudctrlb_write(void *clientData, uint8_t value, uint32_t address)
{
	fprintf(stderr, "XMega UART: %s not implemented\n", __func__);
}

void
XMegaA_UsartNew(const char *name, uint32_t base, PMic * pmic)
{
	XMegaUsart *usart = sg_new(XMegaUsart);
	//usart->udr_in = 0;
	usart->regStatus = STATUS_DREIF;
	//AVR8_RegisterIOHandler(UCSRA(base),ucsra_read,ucsra_write,usart);
	usart->backend = Uart_New(name, serial_input, serial_output, NULL, usart);
	usart->txcIrq = SigNode_New("%s.txcIrq", name);
	usart->rxcIrq = SigNode_New("%s.rxcIrq", name);
	usart->dreIrq = SigNode_New("%s.dreIrq", name);
	usart->byte_time = 200;
	CycleTimer_Init(&usart->tx_baud_timer, tx_done, usart);
	AVR8_RegisterIOHandler(REG_DATA(base), data_read, data_write, usart);
	AVR8_RegisterIOHandler(REG_STATUS(base), status_read, status_write, usart);
	AVR8_RegisterIOHandler(REG_CTRLA(base), ctrla_read, ctrla_write, usart);
	AVR8_RegisterIOHandler(REG_CTRLB(base), ctrlb_read, ctrlb_write, usart);
	AVR8_RegisterIOHandler(REG_CTRLC(base), ctrlc_read, ctrlc_write, usart);
	AVR8_RegisterIOHandler(REG_BAUDCTRLA(base), baudctrla_read, baudctrla_write, usart);
	AVR8_RegisterIOHandler(REG_BAUDCTRLB(base), baudctrlb_read, baudctrlb_write, usart);
	update_dre_interrupt(usart);
	update_txc_interrupt(usart);
	update_rxc_interrupt(usart);
}
