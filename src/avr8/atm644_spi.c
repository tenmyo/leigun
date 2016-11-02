/*
 *************************************************************************************************
 * Emulation of ATMega644 SPI interface
 *
 * state: Master tested with Uzebox SD-Card
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
#include "sgstring.h"
#include "signode.h"
#include "cycletimer.h"
#include "avr8_io.h"
#include "avr8_cpu.h"
#include "clock.h"
#include "configfile.h"
#include "atm644_spi.h"

#define	SPI_SPCR(base)	((base) + 0x0)
#define		SPCR_SPIE	(1 << 7)
#define		SPCR_SPE	(1 << 6)
#define		SPCR_DORD	(1 << 5)
#define		SPCR_MSTR	(1 << 4)
#define		SPCR_CPOL	(1 << 3)
#define		SPCR_CPHA	(1 << 2)
#define		SPCR_SPR1	(1 << 1)
#define		SPCR_SPR0	(1 << 0)

#define SPI_SPSR(base)	((base) + 0x1)
#define		SPSR_SPIF	(1 << 7)
#define		SPSR_WCOL	(1 << 5)
#define		SPSR_SPI2X	(1 << 0)
#define SPI_SPDR(base) 	((base) + 0x2)

typedef struct ATM644_Spi {
	SigNode *miso;	
	SigNode *mosi;
	SigNode *sck;
	SigNode *ss;
	SigNode *irqNode;
	SigNode *irqAckNode;
	uint8_t spcr;
	uint8_t spsr;
	uint8_t spsr_read; /* needed as flag to clear wcol and SPIF */
	uint8_t spdr;
	Clock_t *clk_in;
	Clock_t *spi_clk;
	CycleTimer delayTimer;
	CycleTimer byteDelayTimer;
	uint32_t half_clock_delay;
	CycleCounter_t next_timeout;
	Spi_ByteExchangeProc *byteExchangeProc;
	void *exchg_clientData;

	/* configuration options */
	uint32_t zerodelay;
	uint32_t parallel;
	/* The master transmitter state */
	int state;
	int shiftoutcnt;
	int shiftincnt;
        uint8_t shiftreg;
	/* 
 	 * In master mode ddoe miso is used, in slave
 	 * mode all ddoe are used except miso
 	 */
	SigNode *ddoe_miso;
	SigNode *pvoe_miso;
	SigNode *ddov_miso;
	SigNode *ddoe_mosi;
	SigNode *pvoe_mosi;
	SigNode *ddov_mosi;
	SigNode *ddoe_sck;
	SigNode *pvoe_sck;
	SigNode *ddov_sck;
	SigNode *ddoe_ss;
	SigNode *pvoe_ss;
	SigNode *ddov_ss;
} ATM644_Spi;

static void
update_clock(ATM644_Spi *spi) 
{
	uint32_t multiplier = 1;
	uint32_t divider = 2;
	switch(spi->spcr & (SPCR_SPR1 | SPCR_SPR0)) {
		case 0:
			divider = 4;
			break;
		case 1:
			divider = 16;
			break;
		case 2:
			divider = 64;
			break;
		case 3:
			divider = 128;
			break;
	}
	if(spi->spsr & SPSR_SPI2X) {
		divider = divider >> 1;	
	}
	Clock_MakeDerived(spi->spi_clk,spi->clk_in,multiplier,divider);
}

static void
update_delay(Clock_t *clock,void *clientData)
{
	ATM644_Spi *spi = (ATM644_Spi *) clientData;
	if(Clock_Freq(spi->spi_clk) != 0) {
		spi->half_clock_delay = CycleTimerRate_Get() / Clock_Freq(spi->spi_clk) / 2;
	} else {
		fprintf(stderr,"SPI clock is 0, This should not happen\n");
		spi->half_clock_delay = 1000;
	}
}


static void
update_interrupt(ATM644_Spi *spi)
{
	if((spi->spsr & SPSR_SPIF) && (spi->spcr & SPCR_SPIE)) {
		SigNode_Set(spi->irqNode,SIG_LOW);
	} else {
		SigNode_Set(spi->irqNode,SIG_OPEN);
	}
}

#define STATE_DONE		(0)
#define STATE_STARTED		(1)
#define STATE_LEADING_EDGE	(2)
#define STATE_TRAILING_EDGE	(3)
#define STATE_RELEASE		(4)

static void
sample_miso(ATM644_Spi *spi)
{
	int miso = SigNode_Val(spi->miso);
	int bit;
	if(spi->spcr & SPCR_DORD) {
		bit = spi->shiftincnt;
	} else {
		bit = (7 - spi->shiftincnt);
	}
	if(spi->shiftincnt >= spi->shiftoutcnt) {
		fprintf(stderr,"SPI Bug, shiftin of bit before shiftout\n");
	} 
	if((bit < 0) || (bit > 7)) {
		fprintf(stderr,"SPI Bug, Shift more than 8 Bits\n");
	}
	if(miso == SIG_HIGH) {
		spi->shiftreg |= (1 << bit); 
	} else if(miso == SIG_LOW) {
		spi->shiftreg &= ~(1 << bit); 
	}
	spi->shiftincnt++;
}

static void
shiftout_mosi(ATM644_Spi *spi)
{
	int bit;
	if(spi->spcr & SPCR_DORD) {
		bit = spi->shiftincnt;
	} else {
		bit = (7 - spi->shiftincnt);
	}
	if((bit < 0) || (bit > 7)) {
		fprintf(stderr,"SPI Bug, Shift more than 8 Bits\n");
	}
	if(spi->shiftreg & (1 << bit)) {
		SigNode_Set(spi->mosi,SIG_HIGH);
	} else {
		SigNode_Set(spi->mosi,SIG_LOW);
	}
	spi->shiftoutcnt++;
}

static inline void spi_delay(ATM644_Spi *spi);

static inline void 
trigger_interrupt(ATM644_Spi *spi)
{
#if 0
	if((spi->shiftincnt != 8) || (spi->shiftoutcnt != 8)) {
		uint8_t cpol = !!(spi->spcr & SPCR_CPOL);
		uint8_t cpha = !!(spi->spcr & SPCR_CPHA);
		fprintf(stderr,"SPI-Shiftcnt not 8: %d %d, cpol %d, cpha %d\n"
			,spi->shiftincnt,spi->shiftoutcnt,cpol,cpha);
	}
#endif
	//fprintf(stderr,"needed %lld Cycles\n",spi->next_timeout-spi->trans_start);
	spi->spsr_read &= ~SPSR_SPIF;
	spi->spsr |= SPSR_SPIF;
	update_interrupt(spi);
}

static void
SpiMaster_OneStep(ATM644_Spi *spi) 
{
	uint8_t cpol = !!(spi->spcr & SPCR_CPOL);
	uint8_t cpha = !!(spi->spcr & SPCR_CPHA);
	switch(spi->state) {
		case STATE_RELEASE:
			SigNode_Set(spi->ss,SIG_HIGH);
			spi->state = STATE_DONE;
			trigger_interrupt(spi);
			break;
			
		case STATE_STARTED:
		case STATE_TRAILING_EDGE:
			if((cpha == 1) && (spi->shiftoutcnt == 8)) {
				SigNode_Set(spi->mosi,SIG_HIGH);
				SigNode_Set(spi->ss,SIG_HIGH);
				spi->state = STATE_DONE;
				trigger_interrupt(spi);
				break;
			}
			spi->state = STATE_LEADING_EDGE;
			if(cpha == 0) {
				sample_miso(spi);
				if(spi->shiftincnt == 8) {
					spi->spdr = spi->shiftreg;
				}
			} else {
				shiftout_mosi(spi);
			}
			if(cpol == 0) {
				SigNode_Set(spi->sck,SIG_HIGH);
			} else {
				SigNode_Set(spi->sck,SIG_LOW);
			}
			spi_delay(spi);
			break;
		case STATE_LEADING_EDGE:
			if((cpha == 0) && (spi->shiftoutcnt == 8)) {
				SigNode_Set(spi->mosi,SIG_HIGH);
				spi->state = STATE_RELEASE;
				if(cpol == 0) {
					SigNode_Set(spi->sck,SIG_LOW);
				} else {
					SigNode_Set(spi->sck,SIG_HIGH);
				}
				spi_delay(spi);
				break;
				
			} 
			spi->state = STATE_TRAILING_EDGE;	
			if(cpha == 0) {
				shiftout_mosi(spi);
			} else {
				sample_miso(spi);
				if(spi->shiftincnt == 8) {
					spi->spdr = spi->shiftreg;
					trigger_interrupt(spi);
				}
			}
			if(cpol == 0) {
				SigNode_Set(spi->sck,SIG_LOW);
			} else {
				SigNode_Set(spi->sck,SIG_HIGH);
			}
			spi_delay(spi);
			break;
	}	
}

static void
clock_event(void *clientData) {
	ATM644_Spi *spi = (ATM644_Spi *) clientData;
	if(spi->state == STATE_DONE) {
		fprintf(stderr,"SPI: Clock event should not happen when transfer complete\n");
		return;
	}
	SpiMaster_OneStep(spi);
}

static inline void
spi_delay(ATM644_Spi *spi) 
{
	if(!spi->zerodelay) {
                spi->next_timeout += spi->half_clock_delay;
                if(CycleCounter_Get() >= spi->next_timeout) {
                        clock_event(spi);
                } else {
                        CycleTimer_Mod(&spi->delayTimer,spi->next_timeout - CycleCounter_Get());
                }
        }
}

static void
byte_timer_event(void *clientData) {
	ATM644_Spi *spi = (ATM644_Spi *) clientData;
	/* SigNode_Set(spi->ss,SIG_HIGH); */
	trigger_interrupt(spi);
}

static void
SpiMaster_Start(ATM644_Spi *spi) 
{
	uint8_t cpha;
	if(spi->byteExchangeProc) {
		/* SigNode_Set(spi->ss,SIG_LOW); */
		spi->spdr = spi->byteExchangeProc(spi->exchg_clientData,spi->spdr);
		/* SigNode_Set(spi->ss,SIG_HIGH); */
		if(spi->zerodelay) {
			trigger_interrupt(spi);
		} else {
			/* I should verify the seventeen with a real device */
			CycleTimer_Mod(&spi->byteDelayTimer,17 * spi->half_clock_delay);
		}
		return;
	}	
	cpha = !!(spi->spcr & SPCR_CPHA);
	spi->shiftoutcnt = 0;
	spi->shiftincnt = 0;
	spi->shiftreg = spi->spdr;
	if(cpha == 0) {
		shiftout_mosi(spi);
	} 
	spi->state =  STATE_STARTED;
	SigNode_Set(spi->ss,SIG_LOW);
	if(spi->zerodelay) {
		while(spi->state != STATE_DONE) {
			SpiMaster_OneStep(spi) ;
		}
	} else {
		spi->next_timeout = CycleCounter_Get();
		spi_delay(spi);	
	}
}

static uint8_t
spcr_read(void *clientData,uint32_t address)
{
        ATM644_Spi *spi = (ATM644_Spi *) clientData;
        return spi->spcr;
}

static void
spcr_write(void *clientData,uint8_t value,uint32_t address)
{
        ATM644_Spi *spi = (ATM644_Spi *) clientData;
	uint8_t diff = spi->spcr ^ value;
	spi->spcr = value;
	if(diff & (SPCR_MSTR | SPCR_SPE)) {
		if((value & SPCR_MSTR) && (value & SPCR_SPE)) {
			/* Setup the DDOV before enabling ddoe ! */
			SigNode_Set(spi->ddov_miso,SIG_LOW);
			SigNode_Set(spi->ddoe_miso,SIG_HIGH);
			SigNode_Set(spi->pvoe_mosi,SIG_HIGH);
			SigNode_Set(spi->pvoe_sck,SIG_HIGH);
			SigNode_Set(spi->pvoe_ss,SIG_HIGH);

		} else {
			/* Take away all signla overrides */
			SigNode_Set(spi->ddoe_miso,SIG_LOW);
			SigNode_Set(spi->pvoe_mosi,SIG_LOW);
			SigNode_Set(spi->pvoe_sck,SIG_LOW);
			SigNode_Set(spi->pvoe_ss,SIG_LOW);

			SigNode_Set(spi->ddov_miso,SIG_OPEN);
			spi->state = STATE_DONE;
			CycleTimer_Remove(&spi->delayTimer);
			CycleTimer_Remove(&spi->byteDelayTimer);
			SigNode_Set(spi->mosi,SIG_OPEN);
			SigNode_Set(spi->sck,SIG_OPEN);
		}
	}
	if(diff & (SPCR_SPR0 | SPCR_SPR1)) {
		update_clock(spi);
	}
}

static uint8_t
spsr_read(void *clientData,uint32_t address)
{
        ATM644_Spi *spi = (ATM644_Spi *) clientData;
	spi->spsr_read = spi->spsr & (SPSR_WCOL | SPSR_SPIF);
        return spi->spsr;
}

static void
spsr_write(void *clientData,uint8_t value,uint32_t address)
{
        ATM644_Spi *spi = (ATM644_Spi *) clientData;
	uint8_t diff = spi->spsr ^ value;
	spi->spsr = value;
	if(diff & SPSR_SPI2X) {
		update_clock(spi);
	}

}

static uint8_t
spdr_read(void *clientData,uint32_t address)
{
        ATM644_Spi *spi = (ATM644_Spi *) clientData;
#if 0
	fprintf(stderr,"ining %02x at %lld\n",spi->spdr,CycleCounter_Get());
	usleep(30000);
#endif
	if(spi->spsr_read) {
		spi->spsr &= ~(spi->spsr_read);
		spi->spsr_read = 0;
		update_interrupt(spi);
	}
        return spi->spdr;
}

static void
spdr_write(void *clientData,uint8_t value,uint32_t address)
{
        ATM644_Spi *spi = (ATM644_Spi *) clientData;
	spi->spdr = value;
	//fprintf(stderr,"Outing %02x\n",value);
	//usleep(20000);
	if((spi->spcr & SPCR_MSTR) && (spi->spcr & SPCR_SPE)) {
		if(spi->state != STATE_DONE) {
			spi->spsr |= SPSR_WCOL;
			spi->spsr_read &= ~SPSR_WCOL;
		}
		SpiMaster_Start(spi);
	}
	if(spi->spsr_read) {
		spi->spsr &= ~(spi->spsr_read);
		spi->spsr_read = 0;
		update_interrupt(spi);
	}
}

static void spi_ack_irq(SigNode *node,int value,void *clientData)
{
        ATM644_Spi *spi = (ATM644_Spi *) clientData;
	if(value == SIG_LOW) {
		spi->spsr &= ~SPSR_SPIF;
		update_interrupt(spi);
	}
}

void
ATM644_SpiNew(const char *name,uint32_t base,Spi_ByteExchangeProc *proc,void *clientData)
{
	ATM644_Spi *spi = sg_new(ATM644_Spi);
	spi->miso = SigNode_New("%s.miso",name);
	spi->mosi = SigNode_New("%s.mosi",name);
	spi->sck = SigNode_New("%s.sck",name);
	spi->ss = SigNode_New("%s.ss",name);
	if(!spi->miso || ! spi->mosi || !spi->sck || !spi->ss) {
		fprintf(stderr,"Can not create signal lines for SPI\n");
		exit(1);
	}
	spi->irqNode = SigNode_New("%s.irq",name);
	spi->irqAckNode = SigNode_New("%s.irqAck",name);
	if(!spi->irqNode || !spi->irqAckNode) {
		fprintf(stderr,"Can not create interrupt lines for SPI\n");
		exit(1);
	}
	SigNode_Trace(spi->irqAckNode,spi_ack_irq,spi);
	spi->ddoe_miso = SigNode_New("%s.ddoe_miso",name);
	spi->pvoe_miso = SigNode_New("%s.pvoe_miso",name);
	spi->ddov_miso = SigNode_New("%s.ddov_miso",name);
	spi->ddoe_mosi = SigNode_New("%s.ddoe_mosi",name);
	spi->pvoe_mosi = SigNode_New("%s.pvoe_mosi",name);
	spi->ddov_mosi = SigNode_New("%s.ddov_mosi",name);
	spi->ddoe_sck = SigNode_New("%s.ddoe_sck",name);
	spi->pvoe_sck = SigNode_New("%s.pvoe_sck",name);
	spi->ddov_sck = SigNode_New("%s.ddov_sck",name);
	spi->ddoe_ss = SigNode_New("%s.ddoe_ss",name);
	spi->pvoe_ss = SigNode_New("%s.pvoe_ss",name);
	spi->ddov_ss = SigNode_New("%s.ddov_ss",name);
	if(!spi->ddoe_miso || !(spi->ddoe_mosi) || !(spi->ddoe_sck) || !(spi->ddoe_ss)) {
		fprintf(stderr,"Can not create SPI direction override enable outputs\n");
		exit(1);
	}
	if(!spi->ddov_miso || !(spi->ddov_mosi) || !(spi->ddov_sck) || !(spi->ddov_ss)) {
		fprintf(stderr,"Can not create SPI direction override value outputs\n");
		exit(1);
	}
	if(!spi->pvoe_miso || !(spi->pvoe_mosi) || !(spi->pvoe_sck) || !(spi->pvoe_ss)) {
		fprintf(stderr,"Can not create SPI direction override enable outputs\n");
		exit(1);
	}
	spi->zerodelay = 0;
	Config_ReadUInt32(&spi->zerodelay,name,"zerodelay");
	Config_ReadUInt32(&spi->parallel,name,"parallel");
	spi->spcr = 0;
	spi->spsr = 0;
	spi->half_clock_delay = 1;
	AVR8_RegisterIOHandler(SPI_SPCR(base),spcr_read,spcr_write,spi);
	AVR8_RegisterIOHandler(SPI_SPSR(base),spsr_read,spsr_write,spi);
	AVR8_RegisterIOHandler(SPI_SPDR(base),spdr_read,spdr_write,spi);
	spi->clk_in = Clock_New("%s.clk",name);
	spi->spi_clk = Clock_New("%s.spiclk",name);
	CycleTimer_Init(&spi->delayTimer,clock_event,spi);
	CycleTimer_Init(&spi->byteDelayTimer,byte_timer_event,spi);
	Clock_Trace(spi->spi_clk,update_delay,spi);
	update_interrupt(spi);
	update_clock(spi);
	if(spi->parallel) {
		spi->byteExchangeProc = proc;
		spi->exchg_clientData = clientData;
	}
	fprintf(stderr,"Created ATM644 SPI emulation");
	if(spi->zerodelay) {
		fprintf(stderr,", option zerodelay");
	}
	if(spi->parallel && proc) {
		fprintf(stderr,", option parallel interface");
	}
	fprintf(stderr,"\n");
}
