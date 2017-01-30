/*
 *************************************************************************************************
 * Simulation of AVR8 IO-Port
 * ATMega644 manual was used.
 *
 * state: Basic IO is working, overrides partially working 
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
#include "avr8_io.h"
#include "avr8_cpu.h"
#include "sgstring.h"
#include "signode.h"

#define REG_PIN(base) 	((base) + 0x00)
#define REG_DDR(base)	((base) + 0x01)
#define REG_PORT(base)	((base) + 0x02)

typedef struct AVR8_Port AVR8_Port;

typedef struct Pin {
	int pin_idx;
	AVR8_Port *port;
	SigNode *ddoe;		/* Data direction override enable */
	SigNode *ddov;		/* Data direction override value */
	SigTrace *ddovTrace;
	SigNode *pvoe;		/* Port value override enable */
	SigNode *pvov;		/* Port value override value */
	SigNode *dieoe;		/* Digital input enable override enable */
	SigNode *dieov;		/* Digital input enable override value */
	SigNode *sleep;
	SigNode *puov;		/* Pullup override value */
	SigNode *puoe;		/* Pullup override enable */
	SigNode *pud;		/* Pullup disable */
	SigNode *px;		/* The output/input pin         */
} Pin;

struct AVR8_Port {
	Pin pin[8];
	uint8_t reg_portx;	/* The portx register     */
	uint8_t pvoe;
	uint8_t reg_ddrx;	/* The direction register */
	uint8_t ddoe;
	uint8_t ddov;
	uint8_t direction;	/* result of all the above direction registers */
	uint8_t reg_pin;
	uint8_t reg_pcmsk;
	SigNode *pcint;
};

static inline void
update_px_status(AVR8_Port * port, Pin * pin)
{
	int i = pin->pin_idx;
	if (!(port->direction & (1 << i))) {
		/* Input, for pullup portx is responsible */
		if (port->reg_portx & (1 << i)) {
			SigNode_Set(pin->px, SIG_PULLUP);
		} else {
			SigNode_Set(pin->px, SIG_OPEN);
		}
	} else {
		/* Output, the combined signal of portx and pvov */
		if (port->pvoe & (1 << i)) {
			/* do nothing, we are overwritten */
		} else if (port->reg_portx & (1 << i)) {
			SigNode_Set(pin->px, SIG_HIGH);
		} else {
			SigNode_Set(pin->px, SIG_LOW);
		}
	}
}

static void
update_port_status(AVR8_Port * port, uint8_t diff)
{
	int i;
	for (i = 0; i < 8; i++) {
		if (!(diff & (1 << i))) {
			continue;
		}
		update_px_status(port, &port->pin[i]);
	}
}

static inline uint8_t
recalculate_direction(AVR8_Port * port)
{
	return (port->reg_ddrx & ~(port->ddoe)) | (port->ddov & port->ddoe);
}

static uint8_t
pin_read(void *clientData, uint32_t address)
{
	AVR8_Port *port = (AVR8_Port *) clientData;
	return port->reg_pin;
}

static void
pin_write(void *clientData, uint8_t value, uint32_t address)
{
	AVR8_Port *port = (AVR8_Port *) clientData;
	port->reg_portx ^= value;
	update_port_status(port, value);
	return;
}

static uint8_t
ddr_read(void *clientData, uint32_t address)
{
	AVR8_Port *port = (AVR8_Port *) clientData;
	return port->reg_ddrx;
}

static void
ddr_write(void *clientData, uint8_t value, uint32_t address)
{
	AVR8_Port *port = (AVR8_Port *) clientData;
	uint8_t diff = value ^ port->reg_ddrx;
	port->reg_ddrx = value;
	port->direction = recalculate_direction(port);
	update_port_status(port, diff);
	return;
}

static uint8_t
port_read(void *clientData, uint32_t address)
{
	AVR8_Port *port = (AVR8_Port *) clientData;
	return port->reg_portx;
}

static void
port_write(void *clientData, uint8_t value, uint32_t address)
{
	AVR8_Port *port = (AVR8_Port *) clientData;
	uint8_t diff = value ^ port->reg_portx;
	port->reg_portx = value;
	update_port_status(port, diff);
	return;
}

static uint8_t
pcmsk_read(void *clientData, uint32_t address)
{
	AVR8_Port *port = (AVR8_Port *) clientData;
	return port->reg_pcmsk;
}

static void
pcmsk_write(void *clientData, uint8_t value, uint32_t address)
{
	AVR8_Port *port = (AVR8_Port *) clientData;
	port->reg_pcmsk = value;
	return;
}

/*
 ***********************************************************
 * Trace of port pin
 ***********************************************************
 */

static void
pin_change_event(SigNode * node, int value, void *clientData)
{
	Pin *pin = (Pin *) clientData;
	AVR8_Port *port = pin->port;
	uint8_t mask;
	mask = (1 << pin->pin_idx);
	if (value == SIG_LOW) {
		port->reg_pin &= ~mask;
	} else if (value == SIG_HIGH) {
		port->reg_pin |= mask;
	}
	if (port->reg_pcmsk & mask) {
		if (SigNode_Val(port->pcint) == SIG_LOW) {
			SigNode_Set(port->pcint, SIG_HIGH);
		} else {
			SigNode_Set(port->pcint, SIG_LOW);
		}
	}
}

/*
 **********************************************************
 * Data direction value change event
 **********************************************************
 */
static void
ddov_change_event(SigNode * node, int value, void *clientData)
{
	Pin *pin = (Pin *) clientData;
	AVR8_Port *port = pin->port;
	if (value == SIG_LOW) {
		port->ddov &= ~(1 << pin->pin_idx);
	} else {
		port->ddov |= (1 << pin->pin_idx);
	}
	port->direction = recalculate_direction(port);
	update_px_status(port, pin);
}

/*
 **********************************************************
 * ddoe_change_event
 * Data direction override enable event
 **********************************************************
 */
static void
ddoe_change_event(SigNode * node, int value, void *clientData)
{
	Pin *pin = (Pin *) clientData;
	AVR8_Port *port = pin->port;
	if (value == SIG_HIGH) {
		port->ddoe |= (1 << pin->pin_idx);
		if (!pin->ddovTrace) {
			pin->ddovTrace = SigNode_Trace(pin->ddov, ddov_change_event, pin);
		}
		/* Update the ddov also because it was not traced */
		if (SigNode_Val(pin->ddov) == SIG_LOW) {
			port->ddov &= ~(1 << pin->pin_idx);
		} else {
			port->ddov |= (1 << pin->pin_idx);
		}
	} else {
		port->ddoe &= ~(1 << pin->pin_idx);
		if (pin->ddovTrace) {
			SigNode_Untrace(pin->ddov, pin->ddovTrace);
		}
	}
	port->direction = recalculate_direction(port);
	update_px_status(port, pin);
}

static void
pvoe_change_event(SigNode * node, int value, void *clientData)
{
	Pin *pin = (Pin *) clientData;
	AVR8_Port *port = pin->port;
	int mask = (1 << pin->pin_idx);
	if (value == SIG_HIGH) {
		if (!(port->pvoe & mask)) {
			port->pvoe |= mask;
			/* Shit, this can generate a glitch */
			SigNode_Set(pin->px, SIG_OPEN);
			SigNode_Link(pin->pvov, pin->px);
		}
	} else {
		if (port->pvoe & mask) {
			port->pvoe &= ~mask;
			/* Shit, this can generate a glitch */
			SigNode_RemoveLink(pin->pvov, pin->px);
			update_px_status(port, pin);
		}
	}
}

void
AVR8_PortNew(const char *name, uint16_t base, uint16_t pcmask_addr)
{
	int i;
	AVR8_Port *port = sg_new(AVR8_Port);
	AVR8_RegisterIOHandler(REG_PIN(base), pin_read, pin_write, port);
	AVR8_RegisterIOHandler(REG_DDR(base), ddr_read, ddr_write, port);
	AVR8_RegisterIOHandler(REG_PORT(base), port_read, port_write, port);
	AVR8_RegisterIOHandler(pcmask_addr, pcmsk_read, pcmsk_write, port);
	port->pcint = SigNode_New("%s.pcint", name);
	if (!port->pcint) {
		fprintf(stderr, "failed to create PCINT line\n");
		exit(1);
	}
	for (i = 0; i < 8; i++) {
		Pin *pin = &port->pin[i];
		pin->pin_idx = i;
		pin->port = port;
		pin->px = SigNode_New("%s.P%d", name, i);
		pin->ddoe = SigNode_New("%s.ddoe%d", name, i);
		pin->ddov = SigNode_New("%s.ddov%d", name, i);
		pin->pvoe = SigNode_New("%s.pvoe%d", name, i);
		pin->pvov = SigNode_New("%s.pvov%d", name, i);
		pin->dieoe = SigNode_New("%s.dieoe%d", name, i);
		pin->dieov = SigNode_New("%s.dieov%d", name, i);
		pin->sleep = SigNode_New("%s.sleep%d", name, i);
		pin->puov = SigNode_New("%s.puov%d", name, i);
		pin->puoe = SigNode_New("%s.puoe%d", name, i);
		pin->pud = SigNode_New("%s.pud%d", name, i);
		if (!pin->px || !pin->ddoe || !pin->ddov
		    || !pin->pvoe || !pin->pvov || !pin->puoe || !pin->pud) {
			fprintf(stderr, "Failed to create port pin %d\n", i);
			exit(1);
		}
		SigNode_Set(pin->px, SIG_OPEN);
		/* Pulldown all overrides, there is no need to connect them */
		SigNode_Set(pin->ddoe, SIG_PULLDOWN);
		SigNode_Set(pin->pvoe, SIG_PULLDOWN);
		SigNode_Set(pin->dieoe, SIG_PULLDOWN);
		SigNode_Set(pin->puoe, SIG_PULLDOWN);
		SigNode_Trace(pin->px, pin_change_event, pin);
		SigNode_Trace(pin->ddoe, ddoe_change_event, pin);
		SigNode_Trace(pin->pvoe, pvoe_change_event, pin);
		//SigNode_Set(pin->ddov,SIG_PULLUP); // Test only jk
	}
	update_port_status(port, 0xff);
	fprintf(stderr, "Created AVR IO port \"%s\"\n", name);
}
