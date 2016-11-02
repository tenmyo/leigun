/*
 *************************************************************************************************
 *
 * Simulation of AVR8 AD-Converter 
 * ATMega644 manual was used.
 *
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
#include "clock.h"
#include "avr8_io.h"
#include "avr8_cpu.h"
#include "sgstring.h"
#include "signode.h"
#include "cycletimer.h"
#include "avr8_adc.h"

#define REG_ADC(base) 		((base) + 0x00)
#define REG_ADCL(base) 		((base) + 0x00)
#define REG_ADCH(base) 		((base) + 0x01)
#define REG_ADCSRA(base)	((base) + 0x02)
#define 	ADCSRA_ADEN    (1 << 7)
#define 	ADCSRA_ADSC    (1 << 6)
#define 	ADCSRA_ADATE   (1 << 5)
#define 	ADCSRA_ADIF    (1 << 4)
#define 	ADCSRA_ADIE    (1 << 3)
#define 	ADCSRA_ADPS2   (1 << 2)
#define 	ADCSRA_ADPS1   (1 << 1)
#define 	ADCSRA_ADPS0   (1 << 0)

#define	REG_ADCSRB(base)	((base) + 0x03)
#define 	ADCSRB_ACME    (1 << 6)
#define 	ADCSRB_ADTS2   (1 << 2)
#define 	ADCSRB_ADTS1   (1 << 1)
#define 	ADCSRB_ADTS0   (1 << 0)

#define REG_ADMUX(Base)		((base) + 0x04)
#define 	ADMUX_REFS1   (1 << 7)
#define 	ADMUX_REFS0   (1 << 6)
#define 	ADMUX_ADLAR   (1 << 5)
#define 	ADMUX_MUX4    (1 << 4)
#define 	ADMUX_MUX3    (1 << 3)
#define 	ADMUX_MUX2    (1 << 2)
#define 	ADMUX_MUX1    (1 << 1)
#define 	ADMUX_MUX0    (1 << 0)
#define		REFS_AREF	(0)
#define		REFS_AVCC	(1)
#define		REFS_1100	(2)
#define		REFS_2560	(3)

#define REG_DIDR0   		((base) + 0x7E)
#define 	DIDR0_ADC7D   (1 << 7)
#define 	DIDR0_ADC6D   (1 << 6)
#define 	DIDR0_ADC5D   (1 << 5)
#define 	DIDR0_ADC4D   (1 << 4)
#define 	DIDR0_ADC3D   (1 << 3)
#define 	DIDR0_ADC2D   (1 << 2)
#define 	DIDR0_ADC1D   (1 << 1)
#define 	DIDR0_ADC0D   (1 << 0)

struct AVR8_Adc {
	uint16_t reg_adc;
	uint8_t reg_adcsra;
	uint8_t reg_adcsrb;
	uint8_t reg_admux;
	SigNode *irqNode;
	SigNode *irqAckNode;
	CycleTimer convTimer;
	CycleTimer shTimer;
	Clock_t *in_clk;
	Clock_t *adc_clk;
	 uint32_t(*adcReadProc[8]) (void *clientData);
	void *readProcClientData[8];
};

static void
update_interrupt(AVR8_Adc * adc)
{
	if ((adc->reg_adcsra & ADCSRA_ADIE) && (adc->reg_adcsra & ADCSRA_ADIF)) {
		SigNode_Set(adc->irqNode, SIG_LOW);
	} else {
		SigNode_Set(adc->irqNode, SIG_OPEN);
	}
}

static void
update_clock(AVR8_Adc * adc)
{
	uint8_t divider;
	uint8_t adps = adc->reg_adcsra & (ADCSRA_ADPS0 | ADCSRA_ADPS1 | ADCSRA_ADPS2);
	if (adps == 0) {
		divider = 2;
	} else {
		divider = 1 << adps;
	}
	Clock_MakeDerived(adc->adc_clk, adc->in_clk, 1, divider);
}

/*
 ************************************************
 * adc_measure
 * 	Do the meassurement
 ************************************************
 */
static void
adc_sample_hold(void *clientData)
{
	AVR8_Adc *adc = (AVR8_Adc *) clientData;
	/* differential is missing here */
	int chan = adc->reg_admux & 0x1f;
	int refs = (adc->reg_admux >> 6) & 3;
	uint32_t uvolt = 0;
	uint32_t refvolt = ~0;
	uint32_t tmp;
	if (chan < 7) {
		if (adc->adcReadProc[chan]) {
			uvolt = adc->adcReadProc[chan] (adc->readProcClientData[chan]);
		} else {
			uvolt = 0;
		}
	}
	switch (refs) {
	    case REFS_AREF:
		    /* Me having external 2.50 volt reference */
		    refvolt = 2500000;
		    break;
	    case REFS_AVCC:
		    refvolt = 5000000;
		    break;
	    case REFS_1100:
		    refvolt = 1100000;
		    break;
	    case REFS_2560:
		    refvolt = 2560000;
		    break;
	}
	tmp = (512 * uvolt) / (refvolt / 2);
	if (tmp > 1023) {
		tmp = 1023;
	}
	if (adc->reg_admux & ADMUX_ADLAR) {
		adc->reg_adc = tmp << 6;
	} else {
		adc->reg_adc = tmp;
	}
}

static uint8_t
adcl_read(void *clientData, uint32_t address)
{
	AVR8_Adc *adc = (AVR8_Adc *) clientData;
	return adc->reg_adc & 0xff;
}

static void
adcl_write(void *clientData, uint8_t value, uint32_t address)
{

}

static uint8_t
adch_read(void *clientData, uint32_t address)
{
	AVR8_Adc *adc = (AVR8_Adc *) clientData;
	return (adc->reg_adc >> 8) & 0xff;
}

static void
adch_write(void *clientData, uint8_t value, uint32_t address)
{

}

/*
 *********************************************************************
 * ADCSRA Register
 * 	ADEN    (Bit 7):	A/D converter enable
 *	ADSC    (Bit 6):	Start conversion
 *	ADATE   (Bit 5):	Auto Trigger enable
 *	ADIF    (Bit 4):	A/D Interrupt flag
 *	ADIE    (Bit 3):	A/D Interrupt enable
 * 	ADPS2-0 (Bit 2 - 0)	Prescaler (Divide by 2 - 128)
 *********************************************************************
 */
static uint8_t
adcsra_read(void *clientData, uint32_t address)
{
	AVR8_Adc *adc = (AVR8_Adc *) clientData;
	return adc->reg_adcsra;
}

static void
adcsra_write(void *clientData, uint8_t value, uint32_t address)
{
	AVR8_Adc *adc = (AVR8_Adc *) clientData;
	uint8_t clear = (value & ADCSRA_ADIF);
	uint8_t diff = adc->reg_adcsra ^ value;
	adc->reg_adcsra = value;
	update_clock(adc);
	if (clear) {
		adc->reg_adcsra &= ~clear;
		update_interrupt(adc);
	}
	if (value & ADCSRA_ADEN) {
		if (value & ADCSRA_ADSC & diff) {
			CycleCounter_t shold_delay;
			uint32_t freq = Clock_Freq(adc->adc_clk);
			if (freq > 0) {
				shold_delay = 1.5 * CycleTimerRate_Get() / freq;
				CycleTimer_Mod(&adc->shTimer, shold_delay);
				CycleTimer_Mod(&adc->convTimer, 13 * CycleTimerRate_Get() / freq);
			}
		}
	} else {
		adc->reg_adcsra &= ~ADCSRA_ADSC;
	}
}

/*
 ******************************************************************
 * ADCSRB Register
 *	ACME      (Bit 6):  ACME: Analog Comparator Multiplexer Enable
 *			    (see comparator block diagram)
 *	ADTS2-0   (Bit 2 - 0): Auto Trigger source selection 
 ******************************************************************
 */
static uint8_t
adcsrb_read(void *clientData, uint32_t address)
{
	AVR8_Adc *adc = (AVR8_Adc *) clientData;
	return adc->reg_adcsrb;
}

static void
adcsrb_write(void *clientData, uint8_t value, uint32_t address)
{
	AVR8_Adc *adc = (AVR8_Adc *) clientData;
	adc->reg_adcsrb = value;
	fprintf(stderr, "ADCSRB write not implemented\n");
}

static uint8_t
admux_read(void *clientData, uint32_t address)
{
	AVR8_Adc *adc = (AVR8_Adc *) clientData;
	return adc->reg_admux;
}

static void
admux_write(void *clientData, uint8_t value, uint32_t address)
{
	AVR8_Adc *adc = (AVR8_Adc *) clientData;
	adc->reg_admux = value;
}

/*
 **************************************************************
 * Conversion done timer function
 *	Called by a timer when the A/D Conversion is done
 **************************************************************
 */
static void
adc_conversion_done(void *clientData)
{
	AVR8_Adc *adc = (AVR8_Adc *) clientData;
	adc->reg_adcsra |= ADCSRA_ADIF;
	adc->reg_adcsra &= ~ADCSRA_ADSC;
	update_interrupt(adc);
}

/*
 *************************************************************
 * Auto acknowledge when CPU executes Interrupt
 *************************************************************
 */
static void
adc_irq_ack(SigNode * node, int value, void *clientData)
{
	AVR8_Adc *adc = (AVR8_Adc *) clientData;
	if (value == SIG_LOW) {
		adc->reg_adcsra &= ~ADCSRA_ADIF;
		update_interrupt(adc);
	}
}

void
AVR8_AdcRegisterSource(AVR8_Adc * adc, unsigned int channel, AVR8_AdcReadProc * proc,
		       void *clientData)
{
	if (channel > 8) {
		fprintf(stderr, "Illegal channel\n");
		return;
	}
	adc->adcReadProc[channel] = proc;
	adc->readProcClientData[channel] = clientData;
}

AVR8_Adc *
AVR8_AdcNew(const char *name, uint16_t base)
{
	AVR8_Adc *adc = sg_new(AVR8_Adc);
	AVR8_RegisterIOHandler(REG_ADCL(base), adcl_read, adcl_write, adc);
	AVR8_RegisterIOHandler(REG_ADCH(base), adch_read, adch_write, adc);
	AVR8_RegisterIOHandler(REG_ADCSRA(base), adcsra_read, adcsra_write, adc);
	AVR8_RegisterIOHandler(REG_ADCSRB(base), adcsrb_read, adcsrb_write, adc);
	AVR8_RegisterIOHandler(REG_ADMUX(base), admux_read, admux_write, adc);
	adc->irqNode = SigNode_New("%s.irq", name);
	adc->in_clk = Clock_New("%s.clk", name);
	adc->adc_clk = Clock_New("%s.adc_clk", name);
	adc->irqAckNode = SigNode_New("%s.irqAck", name);
	adc->reg_adcsra = 0;
	if (!adc->irqNode || !adc->irqAckNode) {
		fprintf(stderr, "Can not create irq line for \"%s\"\n", name);
		exit(1);
	}
	SigNode_Trace(adc->irqAckNode, adc_irq_ack, adc);
	CycleTimer_Init(&adc->convTimer, adc_conversion_done, adc);
	CycleTimer_Init(&adc->shTimer, adc_sample_hold, adc);
	update_clock(adc);
	fprintf(stderr, "Created AVR A/D converter \"%s\"\n", name);
	return adc;
}
