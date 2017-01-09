/*
 *************************************************************************************************
 *
 * Emulation of ATMegaXX4 USART
 *
 * state: minimal implementation, working
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

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "sgstring.h"
#include "avr8_cpu.h"
#include "serial.h"
#include "clock.h"
#include "cycletimer.h"
#include "atm644_usart.h"

#define UCSRA(base)	((base) + 0)
#define 	UCSRA_RXC    (1 << 7)
#define 	UCSRA_TXC    (1 << 6)
#define 	UCSRA_UDRE   (1 << 5)
#define 	UCSRA_FE     (1 << 4)
#define 	UCSRA_DOR    (1 << 3)
#define 	UCSRA_UPE    (1 << 2)
#define 	UCSRA_U2X    (1 << 1)
#define 	UCSRA_MPCM   (1 << 0)

#define UCSRB(base)	((base) + 1)
#define 	UCSRB_RXCIE  (1 << 7)
#define 	UCSRB_TXCIE  (1 << 6)
#define 	UCSRB_UDRIE  (1 << 5)
#define 	UCSRB_RXEN   (1 << 4)
#define 	UCSRB_TXEN   (1 << 3)
#define 	UCSRB_UCSZ2  (1 << 2)
#define 	UCSRB_RXB8   (1 << 1)
#define 	UCSRB_TXB8   (1 << 0)

#define UCSRC(base)	((base) + 2)
#define 	UCSRB_UMSEL1 (1<< 7)
#define 	UCSRB_UMSEL0 (1<< 6)
#define 	UCSRB_UPM1   (1<< 5)
#define 	UCSRB_UPM0   (1<< 4)
#define 	UCSRB_USBS   (1<< 3)
#define 	UCSRB_UCSZ1  (1<< 2)
#define 	UCSRB_UCSZ0  (1<< 1)
#define 	UCSRB_UCPHA  (1<< 1)
#define 	UCPOL  (1<< 0)

#define UBRRL(base)	((base) + 4)
#define UBRRH(base)	((base) + 5)
#define UDR(base)	((base) + 6)

#define TXFIFO_SIZE 32
#define TXFIFO_WP(usart) ((usart->txfifo_wp) & (TXFIFO_SIZE - 1))
#define TXFIFO_RP(usart) ((usart->txfifo_rp) & (TXFIFO_SIZE - 1))

typedef struct ATM644_Usart {
	UartPort *port;

	SigNode *udreIrqNode;
	SigNode *txIrqNode;
	SigNode *rxIrqNode;

	/* TXC flag is auto cleared when entering Interrupt */
	SigNode *txIrqAckNode;

	uint8_t ucsra;
	uint8_t ucsrb;
	uint8_t ucsrc;
	uint8_t ubrrl;
	uint8_t ubrrh;
	uint8_t udr_in;
	uint8_t udr_out;
	Clock_t *clk_baud;
	Clock_t *clk_in;
	UartChar txfifo[TXFIFO_SIZE];	/* This Not the fifo of the chip */
	uint64_t txfifo_wp;
	uint64_t txfifo_rp;
	CycleTimer tx_baud_timer;
	CycleTimer rx_baud_timer;
	CycleCounter_t byte_time;
} ATM644_Usart;

static void
update_interrupts(ATM644_Usart * usart)
{
	if (usart->ucsra & usart->ucsrb & UCSRB_RXCIE) {
		//fprintf(stderr,"Post Rx int\n");
		SigNode_Set(usart->rxIrqNode, SIG_LOW);
	} else {
		//fprintf(stderr,"unPost Rx int\n");
		SigNode_Set(usart->rxIrqNode, SIG_OPEN);
	}
	if (usart->ucsra & usart->ucsrb & UCSRB_TXCIE) {
		SigNode_Set(usart->txIrqNode, SIG_LOW);
	} else {
		SigNode_Set(usart->txIrqNode, SIG_OPEN);
	}
	if (usart->ucsra & usart->ucsrb & UCSRB_UDRIE) {
		SigNode_Set(usart->udreIrqNode, SIG_LOW);
	} else {
		SigNode_Set(usart->udreIrqNode, SIG_OPEN);
	}
}

static void
update_baudrate(Clock_t * clock, void *clientData)
{
	ATM644_Usart *usart = (ATM644_Usart *) clientData;
	if (Clock_Freq(clock) > 0) {
		usart->byte_time = 10 * CycleTimerRate_Get() / Clock_Freq(clock);
	} else {
		usart->byte_time = 5;
	}
}

static void
update_clockdivider(ATM644_Usart * usart)
{
	uint8_t u2x = !!(usart->ucsra & UCSRA_U2X);
	uint32_t div;
	if (u2x == 0) {
		div = 16;
	} else {
		div = 8;
	}
	div = div * ((((uint32_t) usart->ubrrl + (((uint32_t) usart->ubrrh) << 8))) + 1);
	Clock_MakeDerived(usart->clk_baud, usart->clk_in, 1, div);
}

static void
tx_irq_ack(SigNode * node, int value, void *clientData)
{
	ATM644_Usart *usart = (ATM644_Usart *) clientData;
	if (value == SIG_LOW) {
		usart->ucsra &= ~UCSRA_TXC;
		update_interrupts(usart);
	}
}

static uint8_t
ucsra_read(void *clientData, uint32_t address)
{
	ATM644_Usart *usart = (ATM644_Usart *) clientData;
	return usart->ucsra;
}

static void
ucsra_write(void *clientData, uint8_t value, uint32_t address)
{
	ATM644_Usart *usart = (ATM644_Usart *) clientData;
	uint8_t diff = usart->ucsra ^ value;
	uint8_t clear = (value & (UCSRA_RXC | UCSRA_TXC | UCSRA_UDRE));
	usart->ucsra = value & ~clear;
	update_interrupts(usart);
	if (diff & UCSRA_U2X) {
		update_clockdivider(usart);
	}
}

static uint8_t
ucsrb_read(void *clientData, uint32_t address)
{
	ATM644_Usart *usart = (ATM644_Usart *) clientData;
	return usart->ucsrb;
}

static void
ucsrb_write(void *clientData, uint8_t value, uint32_t address)
{
	ATM644_Usart *usart = (ATM644_Usart *) clientData;
	uint8_t diff = usart->ucsrb ^ value;
	usart->ucsrb = value;
	if ((value & UCSRB_TXEN) && (diff & UCSRB_TXEN)) {
		usart->ucsra |= UCSRA_UDRE;
		update_interrupts(usart);
	}
	if ((value & UCSRB_RXEN)) {
		if (diff & UCSRB_RXEN) {
			SerialDevice_StartRx(usart->port);
		}
	} else {
		if (diff & UCSRB_RXEN) {
			SerialDevice_StopRx(usart->port);
		}
	}

}

static uint8_t
ucsrc_read(void *clientData, uint32_t address)
{
	ATM644_Usart *usart = (ATM644_Usart *) clientData;
	return usart->ucsrc;
}

static void
ucsrc_write(void *clientData, uint8_t value, uint32_t address)
{
	ATM644_Usart *usart = (ATM644_Usart *) clientData;
	usart->ucsrc = value;
}

static uint8_t
ubrrl_read(void *clientData, uint32_t address)
{
	ATM644_Usart *usart = (ATM644_Usart *) clientData;
	return usart->ubrrl;
}

static void
ubrrl_write(void *clientData, uint8_t value, uint32_t address)
{
	ATM644_Usart *usart = (ATM644_Usart *) clientData;
	usart->ubrrl = value;
	update_clockdivider(usart);
}

static uint8_t
ubrrh_read(void *clientData, uint32_t address)
{
	ATM644_Usart *usart = (ATM644_Usart *) clientData;
	return usart->ubrrh;
}

static void
ubrrh_write(void *clientData, uint8_t value, uint32_t address)
{
	ATM644_Usart *usart = (ATM644_Usart *) clientData;
	usart->ubrrh = value;
	update_clockdivider(usart);
}

static uint8_t
udr_read(void *clientData, uint32_t address)
{
	ATM644_Usart *usart = (ATM644_Usart *) clientData;
	uint8_t result = usart->udr_in;
	usart->ucsra &= ~UCSRA_RXC;
	if (usart->ucsrb & UCSRB_RXEN) {
		SerialDevice_StartRx(usart->port);
	}
	update_interrupts(usart);
	return result;
}

static void
udr_write(void *clientData, uint8_t value, uint32_t address)
{
	ATM644_Usart *usart = (ATM644_Usart *) clientData;
	if (!(usart->ucsra & UCSRA_UDRE)) {
		return;
	}
	usart->ucsra &= ~UCSRA_TXC;
	usart->ucsra &= ~UCSRA_UDRE;
//      usart->tx_start = CycleCounter_Get();
	SerialDevice_StartTx(usart->port);
	usart->txfifo[TXFIFO_WP(usart)] = value;
	usart->txfifo_wp++;
	update_interrupts(usart);
	usart->udr_out = value;
	CycleTimer_Mod(&usart->tx_baud_timer, usart->byte_time);
}

static void
rx_next(void *clientData)
{
	ATM644_Usart *usart = (ATM644_Usart *) clientData;
	if (usart->ucsrb & UCSRB_RXEN) {
		SerialDevice_StartRx(usart->port);
	}
}

static void
serial_input(void *cd, UartChar c)
{
	ATM644_Usart *usart = cd;
	if (usart->ucsra & UCSRA_RXC) {
		fprintf(stderr, "Usart RX Overrun\n");
		usart->ucsra |= UCSRA_DOR;
	} else {
		usart->udr_in = c;
		usart->ucsra |= UCSRA_RXC;
		update_interrupts(usart);
	}
	CycleTimer_Mod(&usart->rx_baud_timer, usart->byte_time);
	SerialDevice_StopRx(usart->port);
}

static void
tx_done(void *clientData)
{
	ATM644_Usart *usart = (ATM644_Usart *) clientData;
	/* Force the emulator not to be faster than the output channel */
	if ((usart->txfifo_wp - usart->txfifo_rp) == TXFIFO_SIZE) {
		CycleTimer_Mod(&usart->tx_baud_timer, 0);
		return;
	}
	usart->ucsra |= UCSRA_UDRE;
	usart->ucsra |= UCSRA_TXC;
	update_interrupts(usart);
}

static bool
serial_output(void *cd, UartChar * c)
{
	ATM644_Usart *usart = cd;
	if (usart->txfifo_rp < usart->txfifo_wp) {
		*c = usart->txfifo[TXFIFO_RP(usart)];
		usart->txfifo_rp++;
	} else {
		fprintf(stderr, "Bug in %s %s\n", __FILE__, __func__);
	}
	if (usart->txfifo_rp == usart->txfifo_wp) {
		SerialDevice_StopTx(usart->port);
	}
	return true;
}

void
ATM644_UsartNew(const char *name, ATMUsartRegisterMap *regMap)
{
	ATM644_Usart *usart = sg_calloc(sizeof(ATM644_Usart));
	uint32_t base;
	ATMUsartRegisterMap defaultRegMap = {
                .addrUCSRA = 0,
                .addrUCSRB = 1,
                .addrUCSRC = 2,
                .addrUBRRL = 4,
                .addrUBRRH = 5,
                .addrUDR = 6,
        };
	if(!regMap) {
		regMap = &defaultRegMap;
	}
	base = regMap->addrBase;

	usart->udr_in = 0;
	usart->ucsra = 0;
	usart->ucsrb = 0;
	usart->ucsrc = 6;
	usart->ubrrl = 0;
	usart->ubrrh = 0;
	if(regMap->addrUCSRA >= 0) {
		AVR8_RegisterIOHandler(base + regMap->addrUCSRA, ucsra_read, ucsra_write, usart);
	}
	if(regMap->addrUCSRB >= 0) {
		AVR8_RegisterIOHandler(base + regMap->addrUCSRB, ucsrb_read, ucsrb_write, usart);
	}
	if(regMap->addrUCSRC >= 0) {
		AVR8_RegisterIOHandler(base + regMap->addrUCSRC, ucsrc_read, ucsrc_write, usart);
	}
	if(regMap->addrUBRRL >= 0) {
		AVR8_RegisterIOHandler(base + regMap->addrUBRRL, ubrrl_read, ubrrl_write, usart);
	}
	if(regMap->addrUBRRH >= 0) {
		AVR8_RegisterIOHandler(base + regMap->addrUBRRH, ubrrh_read, ubrrh_write, usart);
	}
	if(regMap->addrUDR >= 0) {
		AVR8_RegisterIOHandler(base + regMap->addrUDR, udr_read, udr_write, usart);
	}
	usart->port = Uart_New(name, serial_input, serial_output, NULL, usart);

	usart->udreIrqNode = SigNode_New("%s.udreIrq", name);
	usart->txIrqNode = SigNode_New("%s.txIrq", name);
	usart->txIrqAckNode = SigNode_New("%s.txIrqAck", name);
	usart->rxIrqNode = SigNode_New("%s.rxIrq", name);
	if (!usart->udreIrqNode || !usart->txIrqNode || !usart->rxIrqNode || !usart->txIrqAckNode) {
		fprintf(stderr, "Can not create IRQ lines for USART\n");
		exit(1);
	}
	SigNode_Trace(usart->txIrqAckNode, tx_irq_ack, usart);
	usart->clk_baud = Clock_New("%s.clk_baud", name);
	usart->clk_in = Clock_New("%s.clk", name);
	Clock_Trace(usart->clk_baud, update_baudrate, usart);
	update_clockdivider(usart);
	CycleTimer_Init(&usart->tx_baud_timer, tx_done, usart);
	CycleTimer_Init(&usart->rx_baud_timer, rx_next, usart);
	fprintf(stderr, "Created ATMegaXX4 USART \"%s\"\n", name);
}
