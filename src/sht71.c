/*
 **************************************************************************************************
 *
 * SHT71 temperature sensor emulation
 *
 *  State: working but constant humidity and temperature 
 *	   Timing checks are missing. CRC generator is untested.
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

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <unistd.h>
#include <string.h>
#include "signode.h"
#include "cycletimer.h"
#include "sht71.h"
#include "sgstring.h"
#include "sglib.h"

#if 0
#define dbgprintf(...) { fprintf(stderr,__VA_ARGS__); }
#else
#define dbgprintf(...)
#endif

typedef struct SHT71 {
	int state;
	CycleTimer measDelayTimer;
	SigNode *sckNode;
	SigNode *dataNode;
	SigNode *data_pullup;
	uint8_t statreg;
	uint8_t cmd;
	uint16_t inval;
	int bitcnt;
	int bytecnt;
	uint8_t crc;
	uint8_t reply[3];
	int replylen;
	float temperature;
	float humidity;
} SHT71;

#define STATE_IDLE 		(0)
#define STATE_TRANS0 		(1)
#define STATE_TRANS1 		(2)
#define STATE_CMD 		(3)
#define STATE_CMDACK 		(4)
#define STATE_DELAY 		(5)
#define STATE_CMDACK_DONE 	(6)
#define STATE_REPLY 		(7)
#define STATE_CHKACK 		(8)
#define STATE_CRC 		(10)
#define STATE_WRITEBYTE		(11)
#define STATE_DATAACK 		(12)
#define STATE_DATAACKDONE	(13)

#define CMD_MEASTEMP	(3)
#define	CMD_MEASHUM	(5)
#define	CMD_READSTAT	(7)
#define CMD_WRITESTAT	(6)
#define CMD_SOFTRESET	(0x1e)

/*
 * ----------------------------------------------
 * Initialize CRC8 with reversed lower 4 bits
 * of status register
 * ----------------------------------------------
 */
static inline void
CRC8_Init(uint8_t initval, uint8_t * crc)
{
	*crc = initval;
};

static inline void
CRC8(uint8_t val, uint8_t * crc)
{
	int i;
	for (i = 7; i >= 0; i--) {
		int carry = *crc & 0x80;
		int inbit = !!(val & (1 << i));
		*crc = *crc << 1;
		if (carry)
			inbit = !inbit;
		*crc = *crc | inbit;
		if (inbit) {
			*crc = *crc ^ ((1 << 4) | (1 << 5));
		}
	}
}

/*
 * ---------------------------------------------------------------------------
 * Add "len" bytes from the reply to the CRC8 
 * ---------------------------------------------------------------------------
 */
static inline void
sht_add_reply_to_crc(SHT71 * sht, int len)
{
	int i;
	for (i = 0; i < len; i++) {
		CRC8(sht->reply[i], &sht->crc);
	}
}

/*
 * -----------------------------------------------------
 * Initialize the CRC generator with the lower for
 * bits forom the status register
 * -----------------------------------------------------
 */
static void
sht_init_crc(SHT71 * sht, uint8_t first)
{
	int initval = 0;
	int i;
	for (i = 0; i < 4; i++) {
		if (sht->statreg & (1 << i)) {
			initval |= (1 << (7 - i));
		}
	}
	CRC8_Init(initval, &sht->crc);
	CRC8(first, &sht->crc);
}

static void
decode_cmd(SHT71 * sht)
{
	switch (sht->cmd) {
	    case CMD_MEASTEMP:
	    case CMD_MEASHUM:
		    {
			    CycleTimer_Mod(&sht->measDelayTimer, MillisecondsToCycles(2));
			    sht->state = STATE_DELAY;
		    }
		    break;

	    case CMD_READSTAT:
		    sht->reply[0] = sht->statreg;
		    sht_add_reply_to_crc(sht, 1);
		    sht->reply[1] = Bitreverse8(sht->crc);
		    sht->replylen = 2;
		    sht->bitcnt = 1;
		    sht->bytecnt = 0;
		    sht->state = STATE_REPLY;
		    if (sht->reply[0] & 0x80) {
			    SigNode_Set(sht->dataNode, SIG_HIGH);
		    } else {
			    SigNode_Set(sht->dataNode, SIG_LOW);
		    }
		    break;

	    case CMD_WRITESTAT:
		    sht->state = STATE_WRITEBYTE;
		    sht->bitcnt = 0;
		    sht->inval = 0;
		    break;

	    case CMD_SOFTRESET:
		    // reset_sht is missing here
		    break;
	}
}

static void
set_statreg(SHT71 * sht, int inval)
{
	sht->statreg = inval;
}

static void
meas_done(void *clientData)
{
	SHT71 *sht = (SHT71 *) clientData;
	switch (sht->cmd) {
	    case CMD_MEASTEMP:
		    {
			    float d2;
			    uint16_t temp;
			    if (sht->statreg & 1) {
				    d2 = 0.04;	/* 12 Bit */
			    } else {
				    d2 = 0.01;	/* 14 Bit */
			    }
			    temp = (sht->temperature + 39.63) / d2;
			    dbgprintf("Reply with temperature %d\n", temp);
			    sht->reply[0] = temp >> 8;
			    sht->reply[1] = temp & 0xff;
			    sht_add_reply_to_crc(sht, 2);
			    sht->reply[2] = Bitreverse8(sht->crc);
			    sht->state = STATE_REPLY;

			    sht->replylen = 3;
			    sht->bytecnt = 0;
			    sht->bitcnt = 1;	/* next sig_low is already the first bit */
			    SigNode_Set(sht->dataNode, SIG_LOW);
			    break;
		    }

	    case CMD_MEASHUM:
		    {
			    float c1, c2, c3, sq, rh;
			    float ct1, ct2, ct3;
			    float t1 = 0.01;
			    float t2;
			    int hum;
			    if (sht->statreg & 1) {
				    /* 8 Bit */
				    c2 = 0.648;
				    c3 = -7.2 * 10e-4;
				    t2 = 0.00128;
			    } else {
				    /* 12 Bit */
				    c2 = 0.0405;
				    c3 = -2.8e-6;
				    t2 = 0.00008;
			    }
			    c1 = -4;
			    ct1 = c1 + (sht->temperature - 25) * t1;
			    ct2 = c2 + (sht->temperature - 25) * t2;
			    ct3 = c3;
			    rh = sht->humidity;
			    sq = ct2 * ct2 - 4 * ct3 * (ct1 - rh);
			    if (sq > 0) {
				    sq = sqrt(sq);
			    } else {
				    sq = 0;
			    }
			    hum = (-ct2 + sq) / (2 * ct3);
			    if (hum < 0) {
				    hum = (-ct2 - sq) / (2 * ct3);
			    }
			    dbgprintf("Reply with humidity %d\n", hum);
			    sht->reply[0] = hum >> 8;
			    sht->reply[1] = hum & 0xff;
			    sht_add_reply_to_crc(sht, 2);
			    sht->reply[2] = Bitreverse8(sht->crc);
			    sht->state = STATE_REPLY;

			    sht->replylen = 3;
			    sht->bytecnt = 0;
			    sht->bitcnt = 1;	/* next sig_low is already the first bit */
			    SigNode_Set(sht->dataNode, SIG_LOW);
			    break;
		    }

	    default:
		    fprintf(stderr, "SHT71 bug: Delay timer with wrong cmd\n");
		    break;
	}
}

static void
SHT_SckChange(SigNode * node, int value, void *clientData)
{
	SHT71 *sht = (SHT71 *) clientData;
	int data = SigNode_Val(sht->dataNode);
	int sck = value;
	dbgprintf("SHT(%d): Clock line changed to state %d\n", sht->state, value);
	switch (sht->state) {
		    /* 
		     * ------------------------------------------------------------------------
		     * STATE_TRANS0 is entered on a DATA to low transition during clk high.
		     * on negative clock edge state goes to TRANS1 which waits for the
		     * low to high transition of DATA line during clock high.
		     * ------------------------------------------------------------------------
		     */
	    case STATE_TRANS0:
		    if ((sck == SIG_LOW) && (data == SIG_LOW)) {
			    sht->state = STATE_TRANS1;
			    break;
		    } else {
			    fprintf(stderr, "SHT71: Protokoll error in transmission start\n");
			    break;
		    }

		    /* 
		     * ---------------------------------------------------------------------------------------
		     * STATE_CMD is entered after a succesful transmission start sequence from STATE_TRANS1 
		     * it is left to STATE_CMDACK when 8 Bits are received. 
		     * ---------------------------------------------------------------------------------------
		     */
	    case STATE_CMD:
		    /* Positive edge: sample */
		    if (sck == SIG_HIGH) {
			    if (data == SIG_HIGH) {
				    sht->cmd = (sht->cmd << 1) | 1;
			    } else {
				    sht->cmd = (sht->cmd << 1) | 0;
			    }
			    sht->bitcnt++;
			    if (sht->bitcnt == 8) {
				    if (sht->cmd & 0xe0) {
					    fprintf(stderr,
						    "SHT71: Address is not 0, goto IDLE state\n");
					    sht->state = STATE_IDLE;
					    break;
				    }
				    sht->cmd = sht->cmd & 0x1f;
				    sht->state = STATE_CMDACK;
			    }
		    }
		    break;

		    /* 
		     * ---------------------------------------------------------------------
		     * STATE_CMDACK is entered from STATE_CMD when 8 Bits are received  
		     * it is left to CMDACK done when the ACK written to the DATA line 
		     * ---------------------------------------------------------------------
		     */
	    case STATE_CMDACK:
		    /* Set ack on negative edge of sck */
		    if (sck == SIG_LOW) {
			    SigNode_Set(sht->dataNode, SIG_LOW);
			    sht->state = STATE_CMDACK_DONE;
		    }
		    break;

		    /* 
		     * --------------------------------------------------------------------
		     * STATE_CMDACK_DONE is entered from STATE_CMDACK when ack is written
		     * to the data line. It remove the acknowledge on the neg. clk edge 
		     * following the ack.  
		     * The command decoder decides about next state
		     * --------------------------------------------------------------------
		     */
	    case STATE_CMDACK_DONE:
		    /* Remove ack on negative clock edge */
		    if (sck == SIG_LOW) {
			    sht_init_crc(sht, sht->cmd);
			    SigNode_Set(sht->dataNode, SIG_OPEN);
			    decode_cmd(sht);
		    }
		    break;

		    /*
		     * --------------------------------------------------------------------------
		     * STATE_DELAY: state during command execution. The command interpreter
		     * will cause the sht71 to leave this state when the command is done.
		     * No change of clock is allowed until command is done. so 
		     * reaching this is a driver bug.
		     * --------------------------------------------------------------------------
		     */
	    case STATE_DELAY:
		    fprintf(stderr,
			    "SHT71: Driver bug, clock is not allowed to change during meassurement\n");
		    break;

		    /*
		     * --------------------------------------------------------------------------------
		     * STATE_REPLY is entered by the command interpreter when the response fields
		     * are filled, and whenever there is a next byte to send.  
		     * STATE_REPLY is left to STATE_CHKACK when 8 Bits are sent
		     * --------------------------------------------------------------------------------
		     */
	    case STATE_REPLY:
		    if (sck == SIG_LOW) {
			    int index = sht->bytecnt;
			    if (index > 2) {
				    fprintf(stderr, "SHT71: Bug, reply index is %d\n", index);
				    sht->state = STATE_IDLE;
				    break;
			    }
			    if (sht->bitcnt < 8) {
				    if (sht->reply[index] & (1 << (7 - sht->bitcnt))) {
					    SigNode_Set(sht->dataNode, SIG_OPEN);
				    } else {
					    SigNode_Set(sht->dataNode, SIG_LOW);
				    }
				    sht->bitcnt++;
			    } else {
				    /* Prepare for checkack state */
				    SigNode_Set(sht->dataNode, SIG_OPEN);
			    }
		    } else {
			    if (sht->bitcnt == 8) {
				    sht->state = STATE_CHKACK;
			    }
		    }
		    break;

		    /*
		     * ------------------------------------------------------------------------ 
		     * STATE_CHKACK is entered from STATE_REPLY when a byte is sent complete
		     * It goes to STATE_IDLE if not acknowledged. If byte is acknowledged
		     * STATE_REPLY is entered and next byte is sent. If there is no
		     * next byte the next meassurement is triggered by calling decode_cmd
		     * ------------------------------------------------------------------------ 
		     */
	    case STATE_CHKACK:
		    if (sck == SIG_HIGH) {
			    if (data == SIG_LOW) {
				    sht->bytecnt++;
				    if (sht->bytecnt < sht->replylen) {
					    sht->bitcnt = 0;
					    sht->state = STATE_REPLY;
					    break;
				    } else {
					    fprintf(stderr,
						    "SHT71: do next meassurement because acked\n");
					    decode_cmd(sht);
				    }
			    } else {
				    /* not acked means finished  */
				    sht->state = STATE_IDLE;
			    }
		    }
		    break;

		    /*
		     * --------------------------------------------------------------
		     * STATE_WRITEBYTE is entered from the command interpreter when
		     * a status write command is detected
		     * It is left to STATE_DATAACK when 8 Bits are received
		     * --------------------------------------------------------------
		     */
	    case STATE_WRITEBYTE:
		    if (sck == SIG_HIGH) {
			    if (data == SIG_HIGH) {
				    sht->inval = (sht->inval << 1) | 1;
			    } else {
				    sht->inval = (sht->inval << 1) | 0;
			    }
			    sht->bitcnt++;
			    if (sht->bitcnt == 8) {
				    sht->state = STATE_DATAACK;
				    sht->statreg = sht->inval & 0x7;
			    }
		    }
		    break;

		    /*
		     * ---------------------------------------------------------------
		     * STATE_DATACK is entered when a data byte receive is complete.
		     * It sets the date line to low on negative clock edge (ACK)
		     * and leaves to DATAACKDONE
		     * ---------------------------------------------------------------
		     */
	    case STATE_DATAACK:
		    if (sck == SIG_LOW) {
			    SigNode_Set(sht->dataNode, SIG_LOW);
			    sht->state = STATE_DATAACKDONE;
		    }
		    break;

		    /*
		     * ---------------------------------------------------------------
		     *  STATE_DATAACKDONE is entered after the data line is set to
		     *  low by the outgoing acknowledge.
		     *  It releases the DATA line and goes to STATE_IDLE 
		     *  on neg. clk edge
		     * ---------------------------------------------------------------
		     */
	    case STATE_DATAACKDONE:
		    if (sck == SIG_LOW) {
			    SigNode_Set(sht->dataNode, SIG_OPEN);
			    set_statreg(sht, sht->inval);
			    sht->state = STATE_IDLE;
		    }
		    break;
	}
}

static void
SHT_DataChange(SigNode * node, int value, void *clientData)
{
	SHT71 *sht = (SHT71 *) clientData;
	int data = value;
	int sck = SigNode_Val(sht->sckNode);
	dbgprintf("SHT(%d): Data line changed to state %d\n", sht->state, value);
	switch (sht->state) {
		    /*
		     * -----------------------------------------------------
		     * STATE_IDLE is the initial state
		     * If data goes low while clock is high this might be
		     * a transmision start. It leaves to 
		     * TRANS0 which checks for negative clock edge 
		     * -----------------------------------------------------
		     */
	    case STATE_IDLE:
		    if ((data == SIG_LOW) && (sck == SIG_HIGH)) {
			    sht->state = STATE_TRANS0;
			    break;
		    } else {
			    break;
		    }
		    /*
		     * -----------------------------------------------------------------
		     * STATE_TRANS1 is entered from state TRANS0 when a negative clock
		     * edge was detected. It checks for positive edge of data while
		     * the clock is high. The TRANSMISSION start sequence is then
		     * finished and we are waiting for the command.
		     * -----------------------------------------------------------------
		     */
	    case STATE_TRANS1:
		    if ((data == SIG_HIGH) && (sck == SIG_HIGH)) {
			    sht->state = STATE_CMD;
			    sht->bitcnt = 0;
		    }
		    break;

	    default:
		    /* 
		     * -----------------------------------------------------------
		     * A transmit start condition at any other place means
		     * reset the chip. Abort measurements 
		     * -----------------------------------------------------------
		     */
		    if ((data == SIG_LOW) && (sck == SIG_HIGH)) {
			    fprintf(stderr, "SHT71: Chip reset by Transmission Start\n");
			    CycleTimer_Remove(&sht->measDelayTimer);
			    sht->state = STATE_TRANS0;
			    break;
		    } else {
			    break;
		    }
		    break;
	}
}

/*
 * -------------------------------------------------------------
 * SHT71_New
 *	Create a new instance of a SHT71 with the two
 *	signal lines "sck" and "data" as interface
 * -------------------------------------------------------------
 */
void
SHT71_New(const char *name)
{
	SHT71 *sht;
	sht = sg_new(SHT71);
	if (!sht) {
		fprintf(stderr, "Out of memory allocating SHT71\n");
		exit(1);
	}
	memset(sht, 0, sizeof(*sht));
	sht->state = STATE_IDLE;
	sht->sckNode = SigNode_New("%s.sck", name);
	sht->dataNode = SigNode_New("%s.data", name);
	if (!sht->sckNode || !sht->dataNode) {
		fprintf(stderr, "SHT71: can not create signal lines\n");
		exit(1);
	}
	SigNode_Trace(sht->sckNode, SHT_SckChange, sht);
	SigNode_Trace(sht->dataNode, SHT_DataChange, sht);

	/* Create and connect the pullup resistors */
	sht->data_pullup = SigNode_New("%s.sck_pullup", name);
	if (!sht->data_pullup) {
		fprintf(stderr, "DS1305: can not create pullup resistors\n");
		exit(1);
	}
	SigNode_Set(sht->data_pullup, SIG_PULLUP);
	SigNode_Link(sht->dataNode, sht->data_pullup);

	CycleTimer_Init(&sht->measDelayTimer, meas_done, sht);
	sht->temperature = 23.45;
	sht->humidity = 75.19;
	fprintf(stderr, "SHT71 \"%s\" created\n", name);
	return;
}
