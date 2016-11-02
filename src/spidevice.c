/*
 ***********************************************************************************************
 * Emulation of a SPI Master or slave 
 *
 * (C) 2009 Jochen Karrer
 *
 * state: Master mode is working, Used as SPI Master in R8C/M16C/M32C emulation. 
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
#include "clock.h"
#include "configfile.h"
#include "spidevice.h"

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

struct Spi_Device {
	SpiDev_XmitEventProc *xmitCallback;
	void *xmitCallbackData;
	SigNode *miso;
	SigNode *mosi;
	SigNode *sck;
	SigNode *ss;
	SigNode *irqNode;
	SigNode *irqAckNode;
	uint32_t spi_config;
	uint32_t spi_shiftlen;
	uint8_t spdr;
	Clock_t *spi_clk;
	CycleTimer delayTimer;
	CycleTimer byteDelayTimer;
	uint32_t half_clock_delay;
	CycleCounter_t next_timeout;
	//Spi_ByteExchangeProc *byteExchangeProc;
	void *exchg_clientData;

	/* configuration options */
	uint32_t zerodelay;
	uint32_t parallel;
	/* The master transmitter state */
	int state;
	int shiftoutcnt;
	int shiftincnt;
	uint8_t shiftreg;
};

static void
update_delay(Clock_t * clock, void *clientData)
{
	Spi_Device *spi = (Spi_Device *) clientData;
	if (Clock_Freq(spi->spi_clk) != 0) {
		spi->half_clock_delay = CycleTimerRate_Get() / Clock_Freq(spi->spi_clk) / 2;
		//fprintf(stderr,"SPI clock is now %d, freq %lld\n",spi->half_clock_delay,Clock_Freq(spi->spi_clk) );
		//sleep(1);
	} else {
		fprintf(stderr, "SPI clock is 0, This should not happen\n");
		spi->half_clock_delay = 1000;
	}
}

#define STATE_DONE		(0)
#define STATE_STARTED		(1)
#define STATE_LEADING_EDGE	(2)
#define STATE_TRAILING_EDGE	(3)
#define STATE_RELEASE		(4)

static int
sample_miso(Spi_Device * spi)
{
	int miso = SigNode_Val(spi->miso);
	int bit = 0;
	if (spi->spi_config & SPIDEV_LSBFIRST) {
		bit = spi->shiftincnt;
	} else {
		bit = (spi->spi_shiftlen - 1 - spi->shiftincnt);
	}
	if (spi->shiftincnt >= spi->shiftoutcnt) {
		fprintf(stderr, "SPI Bug, shiftin of bit before shiftout\n");
	}
	if ((bit < 0) || (bit >= spi->spi_shiftlen)) {
		fprintf(stderr, "SPI Bug, Shift more than shiftlen Bits\n");
	}
	if (miso == SIG_HIGH) {
		spi->shiftreg |= (1 << bit);
	} else if (miso == SIG_LOW) {
		spi->shiftreg &= ~(1 << bit);
	}
	spi->shiftincnt++;
	return miso;
}

static void
shiftout_mosi(Spi_Device * spi)
{
	int bit = 0;
	if (spi->spi_config & SPIDEV_LSBFIRST) {
		bit = spi->shiftincnt;
	} else {
		bit = (spi->spi_shiftlen - 1 - spi->shiftincnt);
	}
	if ((bit < 0) || (bit >= spi->spi_shiftlen)) {
		fprintf(stderr, "SPI Bug, Shift more than 8 Bits\n");
	}
	if (spi->shiftreg & (1 << bit)) {
		SigNode_Set(spi->mosi, SIG_HIGH);
	} else {
		SigNode_Set(spi->mosi, SIG_LOW);
	}
	spi->shiftoutcnt++;
}

static inline void spi_delay(Spi_Device * spi);

static void
SpiMaster_OneStep(Spi_Device * spi)
{
	uint8_t cpol = !!(spi->spi_config & SPIDEV_CPOL1);
	uint8_t cpha = !!(spi->spi_config & SPIDEV_CPHA1);
	int miso = 0;
	switch (spi->state) {
	    case STATE_RELEASE:
		    SigNode_Set(spi->ss, SIG_HIGH);
		    spi->state = STATE_DONE;
		    //trigger_interrupt(spi);
		    break;

	    case STATE_STARTED:
	    case STATE_TRAILING_EDGE:
		    if ((cpha == 1) && (spi->shiftoutcnt == 8)) {
			    SigNode_Set(spi->mosi, SIG_HIGH);
			    SigNode_Set(spi->ss, SIG_HIGH);
			    spi->state = STATE_DONE;
			    //trigger_interrupt(spi);
			    break;
		    }
		    spi->state = STATE_LEADING_EDGE;
		    if (cpha == 0) {
			    miso = sample_miso(spi);
			    if (spi->shiftincnt == 8) {
				    spi->spdr = spi->shiftreg;
			    }
		    } else {
			    shiftout_mosi(spi);
		    }
		    if (cpol == 0) {
			    SigNode_Set(spi->sck, SIG_HIGH);
		    } else {
			    SigNode_Set(spi->sck, SIG_LOW);
		    }
		    if (cpha == 0) {
			    if (miso != SigNode_Val(spi->miso)) {
				    fprintf(stderr,
					    "Possible wrong sampling time of MISO cpol %d, LINE %d, cnt %d %d\n",
					    cpol, __LINE__, spi->shiftincnt, spi->shiftoutcnt);
			    }
		    }
		    spi_delay(spi);
		    break;
	    case STATE_LEADING_EDGE:
		    if ((cpha == 0) && (spi->shiftoutcnt == 8)) {
			    SigNode_Set(spi->mosi, SIG_HIGH);
			    spi->state = STATE_RELEASE;
			    if (cpol == 0) {
				    SigNode_Set(spi->sck, SIG_LOW);
			    } else {
				    SigNode_Set(spi->sck, SIG_HIGH);
			    }
			    spi_delay(spi);
			    break;

		    }
		    spi->state = STATE_TRAILING_EDGE;
		    if (cpha == 0) {
			    shiftout_mosi(spi);
		    } else {
			    miso = sample_miso(spi);
			    //fprintf(stderr,"Shiftin %02x, cnt %d ocnt %d\n",
			    //      spi->shiftreg,spi->shiftincnt,spi->shiftoutcnt);
			    if (spi->shiftincnt == 8) {
				    spi->spdr = spi->shiftreg;
				    //trigger_interrupt(spi);
			    }
		    }
		    if (cpol == 0) {
			    SigNode_Set(spi->sck, SIG_LOW);
		    } else {
			    SigNode_Set(spi->sck, SIG_HIGH);
		    }
		    if (cpha != 0) {
			    if (miso != SigNode_Val(spi->miso)) {
				    fprintf(stderr,
					    "Possible wrong sampling time of MISO, cpol %d\n",
					    cpol);
			    }
		    }
		    spi_delay(spi);
		    break;
	}
	if (spi->state == STATE_DONE) {
		if (spi->xmitCallback) {
			//fprintf(stdout,"CB with %02x\n",spi->spdr);
			spi->xmitCallback(spi->xmitCallbackData, &spi->spdr, 8);
		}
	}
}

static void
clock_event(void *clientData)
{
	Spi_Device *spi = (Spi_Device *) clientData;
	if (spi->state == STATE_DONE) {
		fprintf(stderr, "SPI: Clock event should not happen when transfer complete\n");
		return;
	}
	//fprintf(stderr,"Clock event\n");
	SpiMaster_OneStep(spi);
}

static inline void
spi_delay(Spi_Device * spi)
{
	if (!spi->zerodelay) {
		spi->next_timeout += spi->half_clock_delay;
		if (CycleCounter_Get() >= spi->next_timeout) {
			clock_event(spi);
		} else {
			CycleTimer_Mod(&spi->delayTimer, spi->next_timeout - CycleCounter_Get());
		}
	}
}

static void
byte_timer_event(void *clientData)
{
	//Spi_Device *spi = (Spi_Device *) clientData;
	/* SigNode_Set(spi->ss,SIG_HIGH); */
	//trigger_interrupt(spi);
}

void
SpiDev_StartXmit(Spi_Device * spi, uint8_t * firstdata, int bits)
{
	uint8_t cpha;
	if (bits != 8) {
		fprintf(stderr, "Currently only 8 Bit mode spi supported\n");
		return;
	}
	//fprintf(stdout,"xmit %02x\n",*firstdata);
	if ((spi->spi_config & SPIDEV_MS_MSK) == SPIDEV_DISA) {
		return;
	}
	cpha = !!(spi->spi_config & SPIDEV_CPHA1);
	spi->shiftoutcnt = 0;
	spi->shiftincnt = 0;
	spi->shiftreg = *firstdata;
	if (cpha == 0) {
		shiftout_mosi(spi);
	}
	spi->state = STATE_STARTED;
	SigNode_Set(spi->ss, SIG_LOW);
	//fprintf(stderr,"Initial sck is %d\n",SigNode_Val(spi->sck));
	if (spi->zerodelay) {
		while (spi->state != STATE_DONE) {
			SpiMaster_OneStep(spi);
		}
	} else {
		spi->next_timeout = CycleCounter_Get();
		spi_delay(spi);
	}
}

/**
 ******************************************************************
 * \fn void SpiDev_Configure(Spi_Device *spidev,uint32_t config)
 ******************************************************************
 */

void
SpiDev_Configure(Spi_Device * spidev, uint32_t config)
{
	spidev->spi_config = config;
	spidev->spi_shiftlen = config & 0xff;
	uint8_t cpol = !!(spidev->spi_config & SPIDEV_CPOL1);
	switch (spidev->spi_config & SPIDEV_MS_MSK) {
	    case SPIDEV_DISA:
		    SigNode_Set(spidev->miso, SIG_OPEN);
		    SigNode_Set(spidev->mosi, SIG_OPEN);
		    SigNode_Set(spidev->sck, SIG_OPEN);
		    SigNode_Set(spidev->ss, SIG_OPEN);
		    break;
	    case SPIDEV_MASTER:
		    /* Set the initial state of the clock on mode change */
		    /* M32C doesn't do it */
		    if ((config & SPIDEV_SET_IDLE_STATE) && (spidev->state == STATE_DONE)) {
			    if (cpol) {
				    SigNode_Set(spidev->sck, SIG_HIGH);
			    } else {
				    SigNode_Set(spidev->sck, SIG_LOW);
			    }
		    }
		    break;

	}
}

Spi_Device *
SpiDev_New(const char *name, SpiDev_XmitEventProc * proc, void *owner)
{
	Spi_Device *spi = sg_new(Spi_Device);
	spi->xmitCallback = proc;
	spi->xmitCallbackData = owner;
	spi->miso = SigNode_New("%s.miso", name);
	spi->mosi = SigNode_New("%s.mosi", name);
	spi->sck = SigNode_New("%s.sck", name);
	spi->ss = SigNode_New("%s.ss", name);
	if (!spi->miso || !spi->mosi || !spi->sck || !spi->ss) {
		fprintf(stderr, "Can not create signal lines for SPI\n");
		exit(1);
	}
	spi->zerodelay = 0;
	Config_ReadUInt32(&spi->zerodelay, name, "zerodelay");
	Config_ReadUInt32(&spi->parallel, name, "parallel");
	spi->spi_config = 0;
	/* just some random initial value */
	spi->half_clock_delay = 1000;
	spi->spi_clk = Clock_New("%s.clk", name);
	CycleTimer_Init(&spi->delayTimer, clock_event, spi);
	CycleTimer_Init(&spi->byteDelayTimer, byte_timer_event, spi);
	Clock_Trace(spi->spi_clk, update_delay, spi);
	fprintf(stderr, "Created SPI-Device emulation \"%s\"\n", name);
	return spi;
}
