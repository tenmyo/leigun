/*
 **************************************************************************************************
 * SPI Interface to MMC/SD cards 
 *
 * (C) 2009 Jochen Karrer
 *   Author: Jochen Karrer
 *
 * state: Basically working, write multiple blocks is untested 
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
#include <stdlib.h>
#include "compiler_extensions.h"
#include "sgstring.h"
#include "signode.h"
#include "mmcdev.h"
#include "mmcard.h"
#include "sd_spi.h"
#include "cycletimer.h"
#include "mmc_crc.h"

#if 0
#define dbgprintf(x...) fprintf(stderr,x)
#else
#define dbgprintf(x...)
#endif

struct SD_Spi {
	SigNode *cmd;		/* connected to MOSI */
	SigNode *clk;		/* connected to SCK */
	SigTrace *clkTrace;
	SigNode *dat3;		/* connected to SPI CS */
	SigNode *dat0;		/* connected to SPI MISO */
	SigNode *sd_det;	/* The switch opened ?? when card is inserted */
	uint8_t shift_in;
	uint8_t shift_out;
	int shiftin_cnt;
	int shiftout_cnt;
	uint8_t inbuf[2048];
	uint8_t outbuf[2048];
	uint8_t delay_fifo[2];	/* Fifo for one byte delay */
	unsigned int delay_fifo_wp;
	/* SPI input state machine variables */
	int istate;
	int ostate;
	unsigned int inbuf_wp;
	CycleCounter_t busy_until;
	//int eight_pause;      /* eight clock cycles pause after every command */

	/* SPI output state machine variables */
	uint64_t outbuf_rp;
	uint64_t outbuf_wp;
	int dataread;
	int cs;
	MMCDev *card;
};

#define OUT_WP(sds) ((sds)->outbuf_wp % sizeof(sds->outbuf))
#define OUT_RP(sds) ((sds)->outbuf_rp % sizeof(sds->outbuf))

#define TOKEN_START_BLOCK	(0xfe)
#define TOKEN_MBW_START_BLOCK	(0xfc)
#define TOKEN_STOP_TRAN		(0xfd)

#define DRTOKEN_ACCEPTED	(0x5)
#define DRTOKEN_REJECT_CRC	(0xB)
#define DRTOKEN_REJECT_WRITE	(0xD)

/* SPI input state machine states */
#define ISTATE_IDLE  		(0)
#define ISTATE_CMD   		(1)
#define ISTATE_DATABLOCK   	(2)
#define ISTATE_DATABLOCKS   	(3)
#define ISTATE_IGNORE   	(4)
#define ISTATE_EIGHT_PAUSE	(5)

#define OSTATE_IDLE  		(0x10)
#define OSTATE_CMD_REPLY 	(0x11)
#define OSTATE_DATA_REPLY 	(0x12)
#define OSTATE_MULTDATA_REPLY 	(0x12)

/* 
 ***************************************************************
 * Set the card to busy for some time.
 ***************************************************************
 */
static inline void
make_busy(SD_Spi * sds, uint32_t useconds)
{
	sds->busy_until = CycleCounter_Get() + MicrosecondsToCycles(useconds);
}

/**
 ****************************************************
 * Put one byte to the SPI output fifo.
 ****************************************************
 */
static inline void
put_outbuf(SD_Spi * sds, uint8_t value)
{
	sds->outbuf[OUT_WP(sds)] = value;
	sds->outbuf_wp++;
}

/**
 ****************************************************
 * Put a datablock to the SPI output fifo. 
 * A start token is prepended.
 ****************************************************
 */
static void
put_block(SD_Spi * sds, uint8_t * data, int len)
{
	int i;
	dbgprintf("Got transmission of data block len %d\n", len);
	put_outbuf(sds, TOKEN_START_BLOCK);
	for (i = 0; i < len; i++) {
		put_outbuf(sds, data[i]);
	}
//      put_outbuf(sds,0); put_outbuf(sds,0); /* Should be crc */
}

/**
 ***************************************************************************
 * \fn static void send_command(SD_Spi *sds) 
 * When a command is assembled from MOSI it is forwarded to the SD-Card
 * simulator. The response is written to the SPI output fifo.
 ***************************************************************************
 */
static void
send_command(SD_Spi * sds)
{
	int i;
	uint32_t cmd;
	uint32_t arg;
	int len;
	uint8_t buf[512];
	MMCResponse resp;
	uint8_t *data = sds->inbuf;

	/* Hack for stop */
	if (sds->dataread) {
		sds->dataread = 0;
		sds->outbuf_wp = sds->outbuf_rp = 0;
	}

	arg = (data[1] << 24) | (data[2] << 16) | (data[3] << 8) | (data[4] << 0);
	cmd = data[0] & 0x3f;

//	fprintf(stderr,"%02x %02x %02x %02x %02x %02x ",
//		  data[0], data[1], data[2], data[3], data[4], data[5]);
	MMCDev_DoCmd(sds->card, cmd, arg, &resp);
	dbgprintf("%02x %02x %02x %02x %02x %02x",
		  data[0], data[1], data[2], data[3], data[4], data[5]);
	dbgprintf(", CMD %d, arg 0x%08x, resp len %d d0 0x%02x\n", cmd, arg, resp.len,
		  resp.data[0]);
	put_outbuf(sds, 0xff);
#if 0
	/* Maximum allowed non response count is 8 */
	put_outbuf(sds, 0xff);
	put_outbuf(sds, 0xff);
	put_outbuf(sds, 0xff);
	put_outbuf(sds, 0xff);
	put_outbuf(sds, 0xff);
	put_outbuf(sds, 0xff);
	put_outbuf(sds, 0xff);
#endif
	for (i = 0; i < resp.len; i++) {
		put_outbuf(sds, resp.data[i]);
	}
	/* 
	 ******************************************************
	 * Simply try if cmd has a data phase
	 ******************************************************
	 */
	if ((len = MMCDev_Read(sds->card, buf, 512)) > 0) {
		uint16_t crc;
		put_block(sds, buf, len);
		MMC_CRC16(&crc, buf, 512);
		put_outbuf(sds, crc >> 8);
		put_outbuf(sds, crc & 0xff);
		sds->dataread = 1;
		sds->ostate = OSTATE_DATA_REPLY;
	} else {
		sds->ostate = OSTATE_CMD_REPLY;
	}

}

/*
 ********************************************
 * Send a datablock to the card
 * currently fixed to 512 Byte blocks
 ********************************************
 */
static void
send_datablock(SD_Spi * sds)
{
	int rc;
	rc = MMCDev_Write(sds->card, sds->inbuf, 512);
	if (rc == 512) {
		put_outbuf(sds, DRTOKEN_ACCEPTED);
		//fprintf(stdout,"ACC 0x%02x\n",DRTOKEN_ACCEPTED);
	} else {
		put_outbuf(sds, DRTOKEN_REJECT_WRITE);
		fprintf(stdout, "Not-ACC 0x%02x\n", DRTOKEN_REJECT_WRITE);
	}
}

/*
 ************************************************************
 * Statemachine for bytes comming from SPI
 ************************************************************
 */
static void
spi_byte_in(SD_Spi * sds, uint8_t data)
{
	if (sds->inbuf_wp >= sizeof(sds->inbuf)) {
		return;
	}
	switch (sds->istate) {
	    case ISTATE_IDLE:
		    if (CycleCounter_Get() < sds->busy_until) {
			    return;
		    }
		    if ((data & 0xc0) == 0x40) {
			    sds->istate = ISTATE_CMD;
			    sds->inbuf[sds->inbuf_wp++] = data;
		    } else if (data == TOKEN_START_BLOCK) {
			    /* Forget the byte itself, its only a token */
			    sds->istate = ISTATE_DATABLOCK;
		    } else if (data == TOKEN_MBW_START_BLOCK) {
			    /* Forget the byte itself, its only a token */
			    sds->istate = ISTATE_DATABLOCKS;
		    } else if (data == TOKEN_STOP_TRAN) {
			    /* Don't know for what i should need this token */
		    }
		    break;

	    case ISTATE_CMD:
		    sds->inbuf[sds->inbuf_wp++] = data;
		    if (sds->inbuf_wp == 6) {
			    send_command(sds);
			    sds->inbuf_wp = 0;
			    sds->istate = ISTATE_IDLE;
		    }
		    break;

	    case ISTATE_DATABLOCK:
		    sds->inbuf[sds->inbuf_wp++] = data;
		    /* Fixed block size of 512 + 2 byte CRC for now */
			//fprintf(stdout,"Data 0x%02x, wp %u\n",data,sds->inbuf_wp);
		    if (sds->inbuf_wp == 514) {
			    send_datablock(sds);
			    sds->inbuf_wp = 0;
			    sds->istate = ISTATE_IGNORE;
			    make_busy(sds, 1000);
		    }
		    break;

	    case ISTATE_DATABLOCKS:
		    sds->inbuf[sds->inbuf_wp++] = data;
		    /* Fixed block size of 512 + 2 byte CRC for now */
		    if (sds->inbuf_wp == 514) {
			    send_datablock(sds);
			    sds->inbuf_wp = 0;
			    sds->istate = ISTATE_IGNORE;
		    }
		    break;

	    case ISTATE_EIGHT_PAUSE:
		    break;

	    case ISTATE_IGNORE:
		    break;

	}
}

static inline uint8_t
spi_fetch_next_byte(SD_Spi * sds)
{
	uint8_t next;
	uint8_t buf[512];
	if (likely(sds->outbuf_rp < sds->outbuf_wp)) {
		next = sds->outbuf[OUT_RP(sds)];
		sds->outbuf_rp++;
		//dbgprintf("%02x ",next);
		/* Check immediately because Eight pause works with CS disabled */
	} else {
		if (CycleCounter_Get() < sds->busy_until) {
			next = 0x00;
		} else {
			next = 0xff;
		}
		if (sds->dataread) {
			int len;
			if ((len = MMCDev_Read(sds->card, buf, 512)) > 0) {
				uint16_t crc = 0;
				dbgprintf("multi-Read success %d\n", len);
				put_block(sds, buf, len);
				MMC_CRC16(&crc, buf, len);
				put_outbuf(sds, crc >> 8);
				put_outbuf(sds, crc & 0xff);
			} else {
				dbgprintf("multi-Read fail %d\n", len);
				sds->dataread = 0;
			}
		}
	}
	return next;
}

/*
 ********************************************************************
 * spi_clk_change
 * 	Signal Trace of the SPI Clock line
 * 	Shifts the bits.	
 ********************************************************************
 */
static void
spi_clk_change(SigNode * node, int value, void *clientData)
{
	SD_Spi *sds = (SD_Spi *) clientData;
#if 0
	/* eight clocks pause also works with chip select disabled */
	if (sds->istate == ISTATE_EIGHT_PAUSE) {
		if (value == SIG_LOW) {
			sds->eight_pause--;
			if (sds->eight_pause < 1) {
				sds->istate = ISTATE_IDLE;
			}
			//fprintf(stderr,"Eight %d\n",sds->eight_pause);
		}
		return;
	}
#endif
	//fprintf(stderr,"+L%d+",value == SIG_HIGH);
	if (sds->cs) {
		if (sds->clkTrace) {
			SigNode_Untrace(sds->clk, sds->clkTrace);
			sds->clkTrace = NULL;
		}
		return;
	}
	if (value == SIG_HIGH) {
		if (SigNode_Val(sds->cmd) == SIG_HIGH) {
			sds->shift_in = (sds->shift_in << 1) | 1;
		} else {
			sds->shift_in = (sds->shift_in << 1);
		}
		sds->shiftin_cnt++;
		if (sds->shiftin_cnt == 8) {
			spi_byte_in(sds, sds->shift_in);
			sds->shiftin_cnt = 0;
		}
	} else if (value == SIG_LOW) {
		if (sds->shiftout_cnt == 0) {
			sds->shift_out = spi_fetch_next_byte(sds);
		}
		if (sds->shift_out & 0x80) {
			SigNode_Set(sds->dat0, SIG_HIGH);
		} else {
			SigNode_Set(sds->dat0, SIG_LOW);
		}
		sds->shift_out <<= 1;
		sds->shiftout_cnt++;
		if (sds->shiftout_cnt == 8) {
			sds->shiftout_cnt = 0;
			//fprintf(stderr,"Fetched %02x\n",sds->shift_out);
		}
	}
	return;
}

/*
 *******************************************************************
 * spi_cs_change
 * 	Signal trace of the chip select line
 *******************************************************************
 */

static void
spi_cs_change(SigNode * node, int value, void *clientData)
{
	SD_Spi *sds = (SD_Spi *) clientData;
	sds->cs = (value != SIG_LOW);
	if (value == SIG_LOW) {
		sds->shift_in = 0;
		sds->shiftin_cnt = 0;
		sds->inbuf_wp = 0;
		if (sds->istate != ISTATE_EIGHT_PAUSE) {
			sds->istate = ISTATE_IDLE;
			sds->ostate = OSTATE_IDLE;
		}
	//	sds->outbuf_wp = sds->outbuf_rp = 0;

		sds->shift_out = 0xff;
		/*
		 **********************************************************
		 * If clock is already low shiftout first bit immediately 
		 * This is required for compatibility with SPI mode 0
		 **********************************************************
		 */
		if (SigNode_Val(sds->clk) == SIG_LOW) {
			sds->shift_out = spi_fetch_next_byte(sds);
			if (sds->shift_out & 0x80) {
				SigNode_Set(sds->dat0, SIG_HIGH);
			} else {
				SigNode_Set(sds->dat0, SIG_LOW);
			}
			sds->shiftout_cnt = 1;
			sds->shift_out <<= 1;
		} else {
			sds->shiftout_cnt = 0;
		}
		if (!sds->clkTrace) {
			sds->clkTrace = SigNode_Trace(sds->clk, spi_clk_change, sds);
		}
	} else {
		SigNode_Set(sds->dat0, SIG_OPEN);
	}
	dbgprintf("CS %d\n", value);
	//fprintf(stdout,"CS %d\n", value);
	return;
}

uint8_t
SDSpi_ByteExchange(void *clientData, uint8_t data)
{
	SD_Spi *sds = (SD_Spi *) clientData;
	uint8_t retval;
	if (unlikely(!sds)) {
		fprintf(stderr, "Bug: SDCard Spi interface called with NULL pointer\n");
		exit(1);
	}
	spi_byte_in(sds, data);
	/*
	 *************************************************************************
	 * In serial spi the reply can not come in the same xmit byte than
	 * the query. To simualate this in parallel mode delay the
	 * reply by one byte.
	 *************************************************************************
	 */
	sds->delay_fifo[sds->delay_fifo_wp] = spi_fetch_next_byte(sds);
	sds->delay_fifo_wp = (sds->delay_fifo_wp + 1) % 2;
	retval = sds->delay_fifo[sds->delay_fifo_wp];
//      fprintf(stderr,"exch %02x, %02x\n",data,retval);
//      usleep(100000);
	return retval;
}

SD_Spi *
SDSpi_New(const char *name, MMCDev * card)
{
	SD_Spi *sds;
	if (!card) {
		fprintf(stderr, "No mmcard given for SPI-SD Interface\n");
		return NULL;
	}
	sds = sg_new(SD_Spi);
	sds->cmd = SigNode_New("%s.cmd", name);
	sds->clk = SigNode_New("%s.clk", name);
	sds->dat3 = SigNode_New("%s.dat3", name);
	sds->dat0 = SigNode_New("%s.dat0", name);
	sds->sd_det = SigNode_New("%s.sd_det", name);
	if (!sds->cmd || !sds->clk || !sds->dat3 || !sds->dat0 || !sds->sd_det) {
		fprintf(stderr, "Can not create Signal lines for SD-Card SPI interface\n");
		exit(1);
	}
	sds->card = card;
	sds->delay_fifo[0] = sds->delay_fifo[1] = 0xff;
	sds->delay_fifo_wp = 0;
	sds->cs = 1;
	SigNode_Set(sds->dat0, SIG_OPEN);
	//sds->clkTrace = SigNode_Trace(sds->clk,spi_clk_change,sds);
	SigNode_Trace(sds->dat3, spi_cs_change, sds);
	MMCDev_GotoSpi(card);
	fprintf(stderr, "Created SPI interface \"%s\" to MMCard\n", name);
	return sds;
}
