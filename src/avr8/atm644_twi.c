/*
 *************************************************************************************************
 *
 * Emulation of the AVR ATMega644 TWI controller 
 *
 *  State: Statemachine working, General call not implemented 
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
#include <stdlib.h>
#include "avr8_io.h"
#include "avr8_cpu.h"
#include "atm644_twi.h"
#include "clock.h"
#include "cycletimer.h"
#include "signode.h"
#include "sgstring.h"

#if 0
#define dbgprintf(x...) fprintf(stderr,x)
#else
#define dbgprintf(x...)
#endif

/* States for the twi state machine from ATMEL manual */
#define TW_START		(0x08)
#define TW_REP_START		(0x10)
#define TW_MT_SLA_ACK		(0x18)
#define TW_MT_SLA_NACK		(0x20)
#define TW_MT_DATA_ACK		(0x28)
#define TW_MT_DATA_NACK         (0x30)
#define TW_MT_ARB_LOST		(0x38)
#define TW_MR_ARB_LOST		(0x38)
#define TW_MR_SLA_ACK		(0x40)
#define TW_MR_SLA_NACK		(0x48)
#define TW_MR_DATA_ACK		(0x50)
#define TW_MR_DATA_NACK		(0x58)
#define TW_ST_SLA_ACK		(0xa8)
#define TW_ST_ARB_LOST_SLA_ACK	(0xb0)
#define TW_ST_DATA_ACK		(0xb8)
#define TW_ST_DATA_NACK		(0xc0)
#define TW_ST_LAST_DATA		(0xc8)
#define TW_SR_SLA_ACK		(0x60)
#define TW_SR_ARB_LOST_SLA_ACK	(0x68)
#define TW_SR_GCALL_ACK		(0x70)
#define TW_SR_ARB_LOST_GCALL_ACK 	(0x78)
#define TW_SR_DATA_ACK		(0x80)
#define TW_SR_DATA_NACK		(0x88)
#define TW_SR_GCALL_DATA_ACK	(0x90)
#define TW_SR_GCALL_DATA_NACK	(0x98)
#define TW_SR_STOP		(0xa0)
#define TW_NO_INFO		(0xf8)
#define TW_BUS_ERROR		(0x00)

#define STARTMODE_REPSTART (1)
#define STARTMODE_START (2)

#define TWBR(base)  ((base) + 0xb8)
#define TWSR(base)  ((base) + 0xb9)
#define 	TWSR_TWS7	(1 << 7)
#define 	TWSR_TWS6    	(1 << 6)
#define 	TWSR_TWS5	(1 << 5)
#define 	TWSR_TWS4    	(1 << 4)
#define 	TWSR_TWS3    	(1 << 3)
#define 	TWSR_TWPS1   	(1 << 1)
#define 	TWSR_TWPS0   	(1 << 0)

#define TWAR(base)  ((base) + 0xba)
#define 	TWAR_TWA6    (1 << 7)
#define 	TWAR_TWA5    (1 << 6)
#define 	TWAR_TWA4    (1 << 5)
#define 	TWAR_TWA3    (1 << 4)
#define 	TWAR_TWA2    (1 << 3)
#define 	TWAR_TWA1    (1 << 2)
#define 	TWAR_TWA0    (1 << 1)
#define 	TWAR_TWGCE   (1 << 0)

#define TWDR(base)  ((base) + 0xbb)
#define TWCR(base)  ((base) + 0xbc)
#define 	TWCR_TWINT   (1 << 7)
#define 	TWCR_TWEA    (1 << 6)
#define 	TWCR_TWSTA   (1 << 5)
#define 	TWCR_TWSTO   (1 << 4)
#define 	TWCR_TWWC    (1 << 3)
#define 	TWCR_TWEN    (1 << 2)
#define 	TWCR_TWIE    (1 << 0)

#define TWAMR(base) ((base) + 0xbd)
#define 	TWAMR_TWAM6   (1 << 7)
#define 	TWAMR_TWAM5   (1 << 6)
#define 	TWAMR_TWAM4   (1 << 5)
#define 	TWAMR_TWAM3   (1 << 4)
#define 	TWAMR_TWAM2   (1 << 3)
#define 	TWAMR_TWAM1   (1 << 2)
#define 	TWAMR_TWAM0   (1 << 1)

#define TWI_STATE_SLV	(0)
#define TWI_STATE_MSTR	(1)

#define TWISL_STATE_IDLE		(0)
#define TWISL_STATE_ADDR		(1)
#define TWISL_STATE_READ		(2)
#define TWISL_STATE_WRITE		(3)
#define TWISL_STATE_SILENT		(5)
#define TWISL_STATE_REPSTARTED		(10)

#define TWIM_STATE_IDLE			(0x40)
#define TWIM_STATE_START		(0x41)
#define TWIM_STATE_ADDR			(0x42)
#define TWIM_STATE_NACK			(0x43)	/* Issue stop asap */
#define TWIM_STATE_RECEIVE		(0x44)
#define TWIM_STATE_TRANSMIT		(0x45)
#define TWIM_STATE_STOP			(0x46)
#define TWIM_STATE_ARB_LOST_ADDR	(0x47)
#define TWIM_STATE_ARB_LOST		(0x48)

/* Definitions for the master state machine */
#define CODE_MEM_SIZE (512)

#define INSTR_SDA_L             (0x01000000)
#define INSTR_SDA_H             (0x02000000)
#define INSTR_SCL_L             (0x03000000)
#define INSTR_SCL_H             (0x04000000)
#define INSTR_CHECK_BUSFREE     (0x05000000)
#define INSTR_NDELAY            (0x06000000)
#define INSTR_SYNC              (0x07000000)
#define INSTR_END               (0x08000000)
#define INSTR_INTERRUPT         (0x09000000)
#define INSTR_MSTR_STATE        (0x0f000000)

#define MSTR_ACK     (1)
#define MSTR_NACK    (0)

#define ADD_CODE(twi,x) ((twi)->mstr_code[((twi)->mstr_icount++) % CODE_MEM_SIZE] = (x))
#define T_HDSTA(twi) 	((twi)->timing.t_hdsta)
#define T_LOW(twi) 	((twi)->timing.t_low)
#define T_HIGH(twi)	((twi)->timing.t_high)
#define T_SUSTA(twi) 	((twi)->timing.t_susta)
#define T_HDDAT_MAX(twi) 	((twi)->timing.t_hddat_max)
#define T_HDDAT(twi) 	(T_HDDAT_MAX(twi) >> 1)
#define T_SUDAT(twi)	((twi)->timing.t_sudat)
#define T_SUSTO(twi)	((twi)->timing.t_susto)
#define T_BUF(twi)	((twi)->timing.t_buf)

typedef struct TWI_Timing {
	int speed;
	uint32_t t_hdsta;
	uint32_t t_low;
	uint32_t t_high;
	uint32_t t_susta;
	uint32_t t_hddat_max;
	uint32_t t_sudat;
	uint32_t t_susto;
	uint32_t t_buf;
} TWI_Timing;

typedef struct ATM644_Twi {
	int twi_state;
	int shiftcnt;
	int bus_free;
	int clock_stopped;
	uint64_t timeout_of_stopped;
	TWI_Timing timing;

	Clock_t *clk_in;
	Clock_t *twi_clk;

	SigNode *irqNode;

	SigNode *ddoe_sda;
	SigNode *ddov_sda;
	SigNode *pvoe_sda;
	SigNode *sdaNode;

	SigNode *ddoe_scl;
	SigNode *ddov_scl;
	SigNode *pvoe_scl;
	SigNode *sclNode;

	SigTrace *sdaTrace;
	SigTrace *sclTrace;
	int old_sda;
	int old_scl;
	int expected_bit;
	uint8_t reg_twbr;
	uint8_t reg_twsr;
	uint8_t reg_twar;
	uint16_t reg_twdr;
	uint8_t reg_twcr;
	uint8_t reg_twamr;

	/* Variables for the master engine */
	SigTrace *mstr_sclStretchTrace;
	CycleTimer mstr_delay_timer;
	CycleTimer mstr_scl_timer;
	CycleTimer slv_release_scl_timer;
	uint16_t mstr_ip;	/* The Instruction pointer in codemem */
	uint32_t mstr_code[CODE_MEM_SIZE];
	uint32_t mstr_icount;	/* The number of instructions in code memory */
} ATM644_Twi;

static void
update_interrupt(ATM644_Twi * twi)
{
	if ((twi->reg_twcr & TWCR_TWINT) && (twi->reg_twcr & TWCR_TWIE)) {
		SigNode_Set(twi->irqNode, SIG_LOW);
	} else {
		SigNode_Set(twi->irqNode, SIG_OPEN);
	}
}

static void
update_timing(Clock_t * clock, void *clientData)
{
	ATM644_Twi *twi = (ATM644_Twi *) clientData;
	uint32_t hightime, lowtime;
	uint32_t freq;
	TWI_Timing *t = &twi->timing;
	freq = Clock_Freq(twi->twi_clk);
	if (!freq) {
		fprintf(stderr, "ATM644_TWI has 0 HZ clock\n");
		freq = 1;
	}
	hightime = lowtime = (CycleTimerRate_Get() / 2 / freq) + 1;

	t->t_hdsta = (hightime * 600 + 699) / 700;
	t->t_high = (hightime * 600 + 699) / 700;
	t->t_susta = (hightime * 600 + 699) / 700;
	t->t_susto = (hightime * 600 + 699) / 700;
	t->t_sudat = (hightime * 100 + 699) / 700;
	t->t_low = (lowtime * 1300 + 1299) / 1300;
	t->t_hddat_max = (lowtime * 900 + 1299) / 1300;
	t->t_buf = (lowtime * 1300 + 1299) / 1300;
}

static void
update_clock(ATM644_Twi * twi)
{
	uint8_t twbr = twi->reg_twbr;
	uint8_t twps = twi->reg_twsr & (TWSR_TWPS0 | TWSR_TWPS1);
	int divider = 16 + 2 * twbr * (1 << (twps * 2));
	Clock_MakeDerived(twi->twi_clk, twi->clk_in, 1, divider);
}

static void
stop_clock(ATM644_Twi * twi)
{
	if (!twi->clock_stopped) {
		twi->clock_stopped = 1;
		twi->timeout_of_stopped = ~(uint64_t) 0;
	}
}

static void
start_clock(ATM644_Twi * twi)
{
	if (twi->clock_stopped) {
		twi->clock_stopped = 0;
		if (twi->timeout_of_stopped != ~(uint64_t) 0) {
			dbgprintf("****************** Restarte clock with %lld\n",
				  twi->timeout_of_stopped);
			CycleTimer_Mod(&twi->mstr_delay_timer, twi->timeout_of_stopped);
		}
	}
}

/*
 **********************************************************************
 * Post the state to the status register and trigger an interrupt
 **********************************************************************
 */
static void
post_state_irq(ATM644_Twi * twi, int state)
{
	twi->reg_twsr = (twi->reg_twsr & 0x7) | state;
	twi->reg_twcr |= TWCR_TWINT;
	update_interrupt(twi);
	stop_clock(twi);
}

/*
 ****************************************************************
 * assemble_script_start
 *      Assemble a micro operation script which generates
 *      a start/repeated start condition on I2C bus.
 *      If it is a repeated start condition  enter with
 *      SCL low for at least T_HDDAT
 *      else with SDA and SCL high.
 ****************************************************************
 */

static void
assemble_script_start(ATM644_Twi * twi, int startmode)
{
	/* For repeated start do not assume SDA and SCL state */
	ADD_CODE(twi, INSTR_MSTR_STATE | TWIM_STATE_START);
	if (startmode == STARTMODE_REPSTART) {
		ADD_CODE(twi, INSTR_SDA_H);
		ADD_CODE(twi, INSTR_NDELAY | (T_LOW(twi) - T_HDDAT(twi)));

		ADD_CODE(twi, INSTR_SCL_H);
		ADD_CODE(twi, INSTR_SYNC);
		ADD_CODE(twi, INSTR_NDELAY | T_HIGH(twi));
	} else {
		ADD_CODE(twi, INSTR_NDELAY | T_BUF(twi));
	}

	/* Generate a start condition */
	/* Try again if bus not free somehow ???????????????? */
	if (startmode == STARTMODE_START) {
		ADD_CODE(twi, INSTR_CHECK_BUSFREE | TWISL_STATE_ADDR);
	}
	ADD_CODE(twi, INSTR_SDA_L);
	ADD_CODE(twi, INSTR_NDELAY | T_HDSTA(twi));
	/* Enter the interrupt with clock low and T_HDDAT waited */
	ADD_CODE(twi, INSTR_SCL_L);
	ADD_CODE(twi, INSTR_NDELAY | T_HDDAT(twi));
	if (startmode == STARTMODE_REPSTART) {
		ADD_CODE(twi, INSTR_INTERRUPT | TW_REP_START);
	} else {
		ADD_CODE(twi, INSTR_INTERRUPT | TW_START);
	}
}

/*
 ****************************************************************
 * assemble_script_stop
 *      Assemble a micro operation script which generates a
 *      stop condition on I2C bus.
 *      Enter with SCL low for at least T_HDDAT
 ****************************************************************
 */
static void
assemble_script_stop(ATM644_Twi * twi)
{
	ADD_CODE(twi, INSTR_SDA_L);
	ADD_CODE(twi, INSTR_NDELAY | (T_LOW(twi) - T_HDDAT(twi)));
	ADD_CODE(twi, INSTR_SCL_H);
	ADD_CODE(twi, INSTR_SYNC);
	ADD_CODE(twi, INSTR_NDELAY | T_SUSTO(twi));
	ADD_CODE(twi, INSTR_SDA_H);
	ADD_CODE(twi, INSTR_NDELAY | T_BUF(twi));
}

/*
 ****************************************************************
 * assemble_script_write
 *      Assemble a micro operation script which genarates 
 *	9 cycles on the bus
 *      Has to be entered with SCL low for at least T_HDDAT
 ****************************************************************
 */
static void
assemble_script_9cycles(ATM644_Twi * twi)
{
	int i;
	for (i = 8; i >= 0; i--) {
		ADD_CODE(twi, INSTR_NDELAY | (T_LOW(twi) - T_HDDAT(twi)));
		ADD_CODE(twi, INSTR_SCL_H);
		ADD_CODE(twi, INSTR_SYNC);
		ADD_CODE(twi, INSTR_NDELAY | T_HIGH(twi));
		ADD_CODE(twi, INSTR_SCL_L);
		ADD_CODE(twi, INSTR_NDELAY | T_HDDAT(twi));
	}
}

static void inline
assemble_script_write(ATM644_Twi * twi)
{
	assemble_script_9cycles(twi);
}

static void inline
assemble_script_read(ATM644_Twi * twi)
{
	assemble_script_9cycles(twi);
}

/*
 *********************************************************************
 * reset_interpreter
 *      reset the I2C-master micro instruction interpreter.
 *      called before assembling a script or on abort of script.
 *********************************************************************
 */
static void
reset_interpreter(ATM644_Twi * twi)
{
	twi->mstr_code[0] = INSTR_END;
	twi->mstr_ip = 0;
	twi->mstr_icount = 0;
	CycleTimer_Remove(&twi->mstr_delay_timer);
	CycleTimer_Remove(&twi->mstr_scl_timer);
	if (twi->mstr_sclStretchTrace) {
		SigNode_Untrace(twi->sclNode, twi->mstr_sclStretchTrace);
		twi->mstr_sclStretchTrace = NULL;
	}
}

#define SDA_H(twi) (sda_out = 1)
#define SDA_L(twi) (sda_out = 0)
#define SCL_H(twi) (scl_out = 1)
#define SCL_L(twi) (scl_out = 0)
static void start_interpreter(ATM644_Twi * twi);

static void
feed_slave(ATM644_Twi * twi, int sda, int scl)
{
	int old_sda = twi->old_sda;
	int old_scl = twi->old_scl;
	int sda_out = -1;
	int scl_out = -1;
	int arbloss = 0;
	if ((old_scl == SIG_LOW) && (scl == SIG_HIGH)) {
		/* Sample data immediately after positive clock edge */
		/* Shiftregister runs always ! */
		/* real device always shifts on clk pos edge */
		/* first bit of shiftout is already set up on neg edge  */
		dbgprintf("posedge in 0x%02x, twdr %04x, shiftin %d\n", twi->twi_state,
			  twi->reg_twdr, sda);
		twi->reg_twdr = (twi->reg_twdr << 1) | sda;
		if (twi->shiftcnt < 9) {
			twi->shiftcnt++;
		}
	} else if (old_scl == SIG_HIGH) {
		if (scl == SIG_LOW) {
			dbgprintf("entering low state %02x cnt %d\n", twi->twi_state,
				  twi->shiftcnt);
			if (twi->expected_bit >= 0) {
				if (twi->expected_bit == 0) {
					if (((twi->reg_twdr & 1) == 1)) {
						dbgprintf("BUS ERROR exp %d got %d\n",
							  twi->expected_bit, twi->reg_twdr & 1);
						post_state_irq(twi, TW_BUS_ERROR);
						/* should be error */
						twi->twi_state = TWISL_STATE_IDLE;
					}
				} else {
					if (((twi->reg_twdr & 1) == 0)) {
						arbloss = 1;
					}
				}
				twi->expected_bit = -1;
			}
			if (arbloss) {
				fprintf(stderr, "ARB_LOSS state %d, shiftcnt %d\n",
					twi->twi_state, twi->shiftcnt);
				switch (twi->twi_state) {
				    case TWIM_STATE_ADDR:
					    SDA_H(twi);
					    SCL_H(twi);
					    twi->twi_state = TWIM_STATE_ARB_LOST_ADDR;
					    reset_interpreter(twi);
					    break;
				    case TWIM_STATE_TRANSMIT:
				    case TWIM_STATE_RECEIVE:
					    SDA_H(twi);
					    SCL_H(twi);
					    twi->twi_state = TWIM_STATE_ARB_LOST;
					    reset_interpreter(twi);
					    break;
				}
			}
			/* Send out data on negative clock edge */
			switch (twi->twi_state) {

			    case TWISL_STATE_WRITE:
				    /* Bus turnaround ! on first cycle of write */
				    if (twi->shiftcnt == 0) {
					    SCL_H(twi);
					    SDA_H(twi);
				    } else if (twi->shiftcnt == 8) {
					    /* Send ack or nack ackording to twea */
					    if (twi->reg_twcr & TWCR_TWEA) {
						    SDA_L(twi);
						    twi->expected_bit = 0;
					    } else {
						    twi->expected_bit = 1;
						    SDA_H(twi);
					    }
				    } else if (twi->shiftcnt == 9) {
					    twi->twi_state = TWISL_STATE_WRITE;
					    twi->shiftcnt = 0;
					    SCL_L(twi);
					    SDA_H(twi);
					    if (twi->reg_twdr & 1) {
						    post_state_irq(twi, TW_SR_DATA_NACK);
					    } else {
						    post_state_irq(twi, TW_SR_DATA_ACK);
					    }
					    break;
				    }
				    break;

			    case TWISL_STATE_ADDR:
			    case TWIM_STATE_ARB_LOST_ADDR:
				    if (twi->shiftcnt == 8) {
					    if ((twi->reg_twdr & 0xfe & ~twi->reg_twamr)
						!= (twi->reg_twar & 0xfe)) {
						    //fprintf(stderr,"No address match\n");
						    if (twi->twi_state == TWIM_STATE_ARB_LOST_ADDR) {
							    /* irqack handler goes to IDLE ! */
							    post_state_irq(twi, TW_MT_ARB_LOST);
						    } else {
							    twi->twi_state = TWISL_STATE_IDLE;
						    }
						    break;
					    }
					    if (!(twi->reg_twcr & TWCR_TWEA)) {
						    dbgprintf("Ack disabled\n");
						    twi->twi_state = TWISL_STATE_IDLE;
						    break;
					    }
					    SDA_L(twi);
					    twi->expected_bit = 0;
				    } else if (twi->shiftcnt == 9) {
					    twi->shiftcnt = 0;
					    SCL_L(twi);
					    SDA_H(twi);
					    if (twi->twi_state == TWIM_STATE_ARB_LOST_ADDR) {
						    if (twi->reg_twdr & 2) {
							    twi->twi_state = TWISL_STATE_READ;
							    post_state_irq(twi,
									   TW_ST_ARB_LOST_SLA_ACK);
						    } else {
							    twi->twi_state = TWISL_STATE_WRITE;
							    post_state_irq(twi,
									   TW_SR_ARB_LOST_SLA_ACK);
						    }
					    } else {
						    if (twi->reg_twdr & 2) {
							    twi->twi_state = TWISL_STATE_READ;
							    post_state_irq(twi, TW_ST_SLA_ACK);
						    } else {
							    twi->twi_state = TWISL_STATE_WRITE;
							    post_state_irq(twi, TW_SR_SLA_ACK);
						    }
					    }
				    }
				    break;

			    case TWISL_STATE_REPSTARTED:
				    twi->shiftcnt = 0;
				    twi->twi_state = TWISL_STATE_ADDR;
				    SCL_L(twi);
				    SDA_H(twi);
				    post_state_irq(twi, TW_SR_STOP);
				    break;

			    case TWIM_STATE_ADDR:
				    if (twi->shiftcnt == 0) {
					    /* Have to wait for the ack_irq first */
					    /* SCL_L(twi);  is already low from script */
					    dbgprintf("Wait for the ack irq\n");
					    SDA_H(twi);
				    } else if (twi->shiftcnt == 8) {
					    SDA_H(twi);
					    break;
				    } else if (twi->shiftcnt == 9) {
					    if (twi->reg_twdr & 0x400) {
						    twi->shiftcnt = 0;
						    if (twi->reg_twdr & 1) {
							    twi->twi_state = TWIM_STATE_NACK;
							    post_state_irq(twi, TW_MR_SLA_NACK);
						    } else {
							    twi->twi_state = TWIM_STATE_RECEIVE;
							    post_state_irq(twi, TW_MR_SLA_ACK);
						    }
					    } else {
						    twi->shiftcnt = 0;
						    if (twi->reg_twdr & 1) {
							    dbgprintf("Got MT SLA_NACK\n");
							    twi->twi_state = TWIM_STATE_NACK;
							    post_state_irq(twi, TW_MT_SLA_NACK);
						    } else {
							    dbgprintf("Got MT SLA_ACK\n");
							    twi->twi_state = TWIM_STATE_TRANSMIT;
							    post_state_irq(twi, TW_MT_SLA_ACK);
						    }
						    break;
					    }
				    } else {
					    dbgprintf("Shiftout %d\n", !!(twi->reg_twdr & 0x100));
					    if (twi->reg_twdr & 0x100) {
						    SDA_H(twi);
						    twi->expected_bit = 1;
					    } else {
						    SDA_L(twi);
						    twi->expected_bit = 0;
					    }
				    }
				    break;

			    case TWIM_STATE_TRANSMIT:
				    if (twi->shiftcnt == 0) {
					    /* Have to wait for the ack_irq first */
					    //SCL_L(twi);  /* is already low from script */
					    dbgprintf("Wait for the ack irq\n");
					    SDA_H(twi);
				    } else if (twi->shiftcnt == 9) {
					    twi->shiftcnt = 0;
					    if (twi->reg_twdr & 1) {
						    twi->twi_state = TWIM_STATE_NACK;
						    post_state_irq(twi, TW_MT_DATA_NACK);
					    } else {
						    twi->twi_state = TWIM_STATE_TRANSMIT;
						    post_state_irq(twi, TW_MT_DATA_ACK);
					    }
					    break;
				    } else {
					    dbgprintf("Shiftout %d\n", !!(twi->reg_twdr & 0x100));
					    if (twi->reg_twdr & 0x100) {
						    SDA_H(twi);
						    twi->expected_bit = 1;
					    } else {
						    SDA_L(twi);
						    twi->expected_bit = 0;
					    }
				    }
				    break;

			    case TWIM_STATE_ARB_LOST:
				    if (twi->shiftcnt == 9) {
					    twi->shiftcnt = 0;
					    /* ack irq has to go to idle silently */
					    post_state_irq(twi, TW_MT_ARB_LOST);
				    }
				    break;

			    case TWIM_STATE_RECEIVE:
				    /* Bus turnaround done by script */
				    if (twi->shiftcnt == 8) {
					    /* Send ack or nack ackording to twea */
					    if (twi->reg_twcr & TWCR_TWEA) {
						    SDA_L(twi);
						    twi->expected_bit = 0;
					    } else {
						    twi->expected_bit = 1;
						    SDA_H(twi);
					    }
				    } else if (twi->shiftcnt == 9) {
					    twi->twi_state = TWIM_STATE_RECEIVE;
					    twi->shiftcnt = 0;
					    SCL_L(twi);
					    SDA_H(twi);
					    if (twi->reg_twdr & 1) {
						    post_state_irq(twi, TW_MR_DATA_NACK);
					    } else {
						    twi->twi_state = TWIM_STATE_RECEIVE;
						    post_state_irq(twi, TW_MR_DATA_ACK);
					    }
				    }
				    break;

			    case TWISL_STATE_READ:
				    if (twi->shiftcnt == 8) {
					    SDA_H(twi);
					    break;
				    } else if (twi->shiftcnt == 9) {
					    SCL_L(twi);
					    twi->shiftcnt = 0;
					    if (twi->reg_twdr & 1) {
						    twi->twi_state = TWISL_STATE_SILENT;
						    post_state_irq(twi, TW_ST_DATA_NACK);
					    } else {
						    /* stay in state TWISL_STATE_READ */
						    if (twi->reg_twcr & TWCR_TWEA) {
							    post_state_irq(twi, TW_ST_DATA_ACK);
						    } else {
							    post_state_irq(twi, TW_ST_LAST_DATA);
						    }
					    }
					    break;
				    }
				    if (twi->reg_twdr & 0x100) {
					    SDA_H(twi);
					    //twi->expected_bit = 1;
				    } else {
					    SDA_L(twi);
					    twi->expected_bit = 0;
				    }
				    break;
			}
		} else if ((old_sda == SIG_LOW) && (sda == SIG_HIGH)) {
			/* Stop condition */
			twi->bus_free = 1;
			dbgprintf("Stop condition detected\n");
			twi->reg_twcr &= ~(TWCR_TWSTO);
			switch (twi->twi_state) {
			    default:
				    dbgprintf("Stop in state 0x%02x\n", twi->twi_state);
				    post_state_irq(twi, TW_SR_STOP);
				    /* In silent we need no stop IRQ, master has to know because he nacked */
			    case TWISL_STATE_ADDR:
			    case TWIM_STATE_STOP:
			    case TWISL_STATE_SILENT:
				    twi->twi_state = TWISL_STATE_IDLE;
				    break;
			    case TWISL_STATE_IDLE:
				    break;
			}
			if (twi->reg_twcr & TWCR_TWSTA) {
				reset_interpreter(twi);
				assemble_script_start(twi, STARTMODE_START);
			}
		} else if ((old_sda == SIG_HIGH) && (sda == SIG_LOW)) {
			/* Start condition */
			twi->bus_free = 0;
			dbgprintf("Start condition detected\n");
			switch (twi->twi_state) {
			    default:
				    dbgprintf("TWI: Unexpected start in state 0x%02x\n",
					      twi->twi_state);
			    case TWISL_STATE_WRITE:
			    case TWISL_STATE_SILENT:
				    twi->shiftcnt = 0;
				    twi->twi_state = TWISL_STATE_REPSTARTED;
				    break;

			    case TWISL_STATE_IDLE:
				    twi->shiftcnt = 0;
				    twi->twi_state = TWISL_STATE_ADDR;
				    break;

			    case TWIM_STATE_START:
				    twi->shiftcnt = 0;
				    twi->twi_state = TWIM_STATE_ADDR;
				    break;

			}
		}
	}
	dbgprintf("Leaving in state %02x\n", twi->twi_state);
	twi->old_sda = sda;
	twi->old_scl = scl;
	if (scl_out == 0) {
		SigNode_Set(twi->sclNode, SIG_LOW);
	} else if (scl_out == 1) {
		SigNode_Set(twi->sclNode, SIG_OPEN);
	}
	if (sda_out == 0) {
		SigNode_Set(twi->sdaNode, SIG_LOW);
	} else if (sda_out == 1) {
		SigNode_Set(twi->sdaNode, SIG_OPEN);
	}
}

/*
 ****************************************************************
 * sda_change/scl_change
 * The trace procs for the SDA/SCL line, Invoked whenever
 * one of the lines changes its level
 ****************************************************************
 */
static void
sda_change(SigNode * node, int value, void *clientData)
{
	ATM644_Twi *twi = (ATM644_Twi *) clientData;
	int sda = value;
	int scl = SigNode_Val(twi->sclNode);
	feed_slave(twi, sda, scl);
}

static void
scl_change(SigNode * node, int value, void *clientData)
{
	ATM644_Twi *twi = (ATM644_Twi *) clientData;
	int scl = value;
	int sda = SigNode_Val(twi->sdaNode);
	feed_slave(twi, sda, scl);
}

#define MSTR_RET_DONE		(0)
#define MSTR_RET_DO_NEXT	(1)
#define MSTR_RET_EMU_ERROR	(-3)

static void scl_released(SigNode * node, int value, void *clientData);
static void scl_timeout(void *clientData);

/*
 ******************************************
 * Master statemachine
 ******************************************
 */
static int
mstr_execute_instruction(ATM644_Twi * twi)
{
	uint32_t icode;
	uint32_t arg;
	if (twi->mstr_ip >= CODE_MEM_SIZE) {
		fprintf(stderr, "AT91Twi: corrupt I2C script\n");
		return MSTR_RET_EMU_ERROR;
	}
	if (twi->mstr_ip == (twi->mstr_icount % CODE_MEM_SIZE)) {
		return MSTR_RET_DONE;
	}
	icode = twi->mstr_code[twi->mstr_ip] & 0xff000000;
	arg = twi->mstr_code[twi->mstr_ip] & 0xffffff;
	twi->mstr_ip = (twi->mstr_ip + 1) % CODE_MEM_SIZE;

	//fprintf(stderr,"Instr %08x\n",icode | arg);
	switch (icode) {
	    case INSTR_SDA_H:
		    dbgprintf("SDA_H %08x\n", icode);
		    SigNode_Set(twi->sdaNode, SIG_OPEN);
		    break;
	    case INSTR_SDA_L:
		    dbgprintf("SDA_L %08x\n", icode);
		    SigNode_Set(twi->sdaNode, SIG_LOW);
		    break;
	    case INSTR_SCL_H:
		    dbgprintf("SCL_H %08x\n", icode);
		    SigNode_Set(twi->sclNode, SIG_OPEN);
		    break;
	    case INSTR_SCL_L:
		    dbgprintf("SCL_L %08x\n", icode);
		    SigNode_Set(twi->sclNode, SIG_LOW);
		    break;

	    case INSTR_CHECK_BUSFREE:
		    if (twi->bus_free == 0) {
			    fprintf(stderr, "Bus not free\n");
			    reset_interpreter(twi);
			    twi->twi_state = arg;
			    return MSTR_RET_DONE;
		    }
		    break;

	    case INSTR_NDELAY:
		    {
			    int64_t cycles = arg;
			    if (twi->clock_stopped) {
				    twi->timeout_of_stopped = cycles;
			    } else {
				    CycleTimer_Mod(&twi->mstr_delay_timer, cycles);
			    }
			    dbgprintf("NDELAY %08x\n", icode);
		    }
		    return MSTR_RET_DONE;
		    break;

	    case INSTR_SYNC:
		    dbgprintf("SYNC %08x\n", icode);
		    if (SigNode_Val(twi->sclNode) == SIG_LOW) {
			    uint32_t msecs = 200;
			    int64_t cycles = MillisecondsToCycles(msecs);
			    if (!twi->mstr_sclStretchTrace) {
				    twi->mstr_sclStretchTrace =
					SigNode_Trace(twi->sclNode, scl_released, twi);
			    }
			    CycleTimer_Mod(&twi->mstr_scl_timer, cycles);
			    return MSTR_RET_DONE;
		    }
		    break;

	    case INSTR_END:
		    dbgprintf("ENDSCRIPT %08x\n", icode);
		    reset_interpreter(twi);
		    return MSTR_RET_DONE;
		    break;

	    case INSTR_INTERRUPT:
		    dbgprintf("INTERRUPT %08x\n", icode);
		    post_state_irq(twi, arg);
		    break;

	    case INSTR_MSTR_STATE:
		    twi->twi_state = arg;
		    break;

	    default:
		    fprintf(stderr, "ATMega Twi: I2C: Unknode icode %08x\n", icode);
		    return MSTR_RET_EMU_ERROR;
		    break;

	}
	return MSTR_RET_DO_NEXT;
}

/*
 ******************************************************************
 * mstr_run_interpreter
 *      The I2C-master micro instruction interpreter main loop
 *      executes instructions until the script is done or
 *      waits for some event
 ******************************************************************
 */
static void
mstr_run_interpreter(void *clientData)
{
	ATM644_Twi *twi = (ATM644_Twi *) clientData;
	int retval;
	do {
		retval = mstr_execute_instruction(twi);
	} while (retval == MSTR_RET_DO_NEXT);
}

static int
interpreter_running(ATM644_Twi * twi)
{
	if (CycleTimer_IsActive(&twi->mstr_delay_timer)) {
		return 1;
	}
	if (twi->clock_stopped && (twi->timeout_of_stopped != ~(uint64_t) 0)) {
		return 1;
	}
	return 0;
}

/*
 * -----------------------------------------------------------------------------
 * start interpreter
 *      Start execution of micro operation I2C scripts
 * -----------------------------------------------------------------------------
 */
static void
start_interpreter(ATM644_Twi * twi)
{
	if (CycleTimer_IsActive(&twi->mstr_delay_timer)) {
		dbgprintf("ATM644_Twi: Starting already running interp.\n");
		//fprintf(stderr,"ATM644_Twi: Starting already running interp.\n");
	} else {
		CycleTimer_Mod(&twi->mstr_delay_timer, 0);
	}
}

/*
 **********************************************************************
 * scl_released
 *      The SCL trace proc for clock stretching
 *      If it is called before timeout it will
 *      remove the timer and continue running the interpreter
 **********************************************************************
 */
static void
scl_released(SigNode * node, int value, void *clientData)
{
	ATM644_Twi *twi = (ATM644_Twi *) clientData;
	if ((value == SIG_PULLUP) || (value == SIG_HIGH)) {
		if (twi->mstr_sclStretchTrace) {
			SigNode_Untrace(twi->sclNode, twi->mstr_sclStretchTrace);
			twi->mstr_sclStretchTrace = NULL;
		}
		CycleTimer_Remove(&twi->mstr_delay_timer);
		mstr_run_interpreter(clientData);
	}
}

static void
scl_timeout(void *clientData)
{
	ATM644_Twi *twi = (ATM644_Twi *) clientData;
	SigNode *dom = SigNode_FindDominant(twi->sclNode);
	if (dom) {
		fprintf(stderr, "ATM644_Twi: I2C SCL seems to be blocked by %s\n", SigName(dom));
	} else {
		fprintf(stderr, "ATM644_Twi: I2C SCL seems to be blocked\n");
	}
	if (twi->mstr_sclStretchTrace) {
		SigNode_Untrace(twi->sclNode, twi->mstr_sclStretchTrace);
		twi->mstr_sclStretchTrace = NULL;
	}
	/* Do something ? Post an interrupt ? */
}

/*
 ***************************************************************************
 * Enable/Disable the two wire slave. Connected to the TWEN bit in the
 * control register
 ***************************************************************************
 */
static void
tw_enable(ATM644_Twi * twi)
{
	twi->twi_state = TWISL_STATE_IDLE;;

	SigNode_Set(twi->ddov_sda, SIG_LOW);
	SigNode_Set(twi->ddov_scl, SIG_LOW);

	SigNode_Set(twi->ddoe_sda, SIG_HIGH);
	SigNode_Set(twi->pvoe_sda, SIG_HIGH);
	SigNode_Set(twi->ddoe_scl, SIG_HIGH);
	SigNode_Set(twi->pvoe_scl, SIG_HIGH);

	twi->old_sda = SigNode_Val(twi->sdaNode);
	twi->old_scl = SigNode_Val(twi->sclNode);
	twi->bus_free = 1;

	if (!twi->sdaTrace) {
		twi->sdaTrace = SigNode_Trace(twi->sdaNode, sda_change, twi);
	}
	if (!twi->sclTrace) {
		twi->sclTrace = SigNode_Trace(twi->sclNode, scl_change, twi);
	}
}

static void
tw_disable(ATM644_Twi * twi)
{
	twi->twi_state = TWISL_STATE_IDLE;;
	if (twi->sdaTrace) {
		SigNode_Untrace(twi->sdaNode, twi->sdaTrace);
		twi->sdaTrace = NULL;
	}
	if (twi->sclTrace) {
		SigNode_Untrace(twi->sclNode, twi->sclTrace);
		twi->sclTrace = NULL;
	}
	twi->reg_twcr &= ~TWCR_TWINT;
	update_interrupt(twi);

	SigNode_Set(twi->ddoe_sda, SIG_LOW);
	SigNode_Set(twi->pvoe_sda, SIG_LOW);
	SigNode_Set(twi->ddoe_scl, SIG_LOW);
	SigNode_Set(twi->pvoe_scl, SIG_LOW);

	/* Should not be required */
	SigNode_Set(twi->ddov_sda, SIG_OPEN);
	SigNode_Set(twi->ddov_scl, SIG_OPEN);

	SigNode_Set(twi->sdaNode, SIG_OPEN);
	SigNode_Set(twi->sclNode, SIG_OPEN);
}

static void
release_scl(void *clientData)
{
	ATM644_Twi *twi = (ATM644_Twi *) clientData;
	SigNode_Set(twi->sclNode, SIG_OPEN);

}

static void
ack_irq(ATM644_Twi * twi)
{
	int sda_changed = 0;
	start_clock(twi);
	update_interrupt(twi);
	dbgprintf("ack irq in state 0x%02x\n", twi->twi_state);
	if (twi->reg_twcr & TWCR_TWSTA) {
		twi->twi_state = TWIM_STATE_START;	/* double, shit */
		assemble_script_start(twi, STARTMODE_REPSTART);
		start_interpreter(twi);
		return;
	} else if (twi->reg_twcr & TWCR_TWSTO) {
		twi->twi_state = TWIM_STATE_STOP;
		assemble_script_stop(twi);
		start_interpreter(twi);
		return;
	}
	switch (twi->twi_state) {
		    /* Go silently to idle */
	    case TWIM_STATE_ARB_LOST_ADDR:
	    case TWIM_STATE_ARB_LOST:
		    twi->twi_state = TWISL_STATE_IDLE;
		    return;

	    case TWIM_STATE_TRANSMIT:	/* Same as in state addr */
	    case TWIM_STATE_ADDR:
		    assemble_script_write(twi);
		    start_interpreter(twi);
	    case TWISL_STATE_READ:
		    /* Set the first bit before releasing SCL */
		    //fprintf(stderr,"Stretch end with twdr %04x\n",twi->reg_twdr);
		    if (twi->reg_twdr & 0x100) {
			    twi->expected_bit = 1;
			    if (SigNode_State(twi->sdaNode) != SIG_OPEN) {
				    sda_changed = 1;
				    SigNode_Set(twi->sdaNode, SIG_OPEN);
			    }
		    } else {
			    twi->expected_bit = 0;
			    if (SigNode_State(twi->sdaNode) != SIG_LOW) {
				    sda_changed = 1;
				    SigNode_Set(twi->sdaNode, SIG_LOW);
			    }
		    }
		    break;
#if 1
	    case TWIM_STATE_RECEIVE:
		    assemble_script_read(twi);
		    start_interpreter(twi);
#endif
	}
	/* This needs a delay timer with T_SUDAT if SDA has changed !!!!! */
	if (!interpreter_running(twi)) {
		if (!sda_changed) {
			SigNode_Set(twi->sclNode, SIG_OPEN);
		} else {
			int64_t cycles = T_SUDAT(twi);
			CycleTimer_Mod(&twi->slv_release_scl_timer, cycles);
		}
	}
}

static uint8_t
twbr_read(void *clientData, uint32_t address)
{
	ATM644_Twi *twi = (ATM644_Twi *) clientData;
	return twi->reg_twbr;
}

static void
twbr_write(void *clientData, uint8_t value, uint32_t address)
{
	ATM644_Twi *twi = (ATM644_Twi *) clientData;
	twi->reg_twbr = value;
	update_clock(twi);
}

static uint8_t
twsr_read(void *clientData, uint32_t address)
{
	ATM644_Twi *twi = (ATM644_Twi *) clientData;
	uint8_t retval = twi->reg_twsr;
	twi->reg_twsr = (twi->reg_twsr & 0x7) | TW_NO_INFO;
	return retval;
}

static void
twsr_write(void *clientData, uint8_t value, uint32_t address)
{
	ATM644_Twi *twi = (ATM644_Twi *) clientData;
	uint8_t diff = twi->reg_twsr ^ value;
	twi->reg_twsr = (value & 0x7) | (twi->reg_twsr & 0xf8);
	if (diff & 3) {
		update_clock(twi);
	}
}

static uint8_t
twar_read(void *clientData, uint32_t address)
{
	ATM644_Twi *twi = (ATM644_Twi *) clientData;
	return twi->reg_twar;
}

static void
twar_write(void *clientData, uint8_t value, uint32_t address)
{
	ATM644_Twi *twi = (ATM644_Twi *) clientData;
	twi->reg_twar = value;
}

static uint8_t
twdr_read(void *clientData, uint32_t address)
{
	ATM644_Twi *twi = (ATM644_Twi *) clientData;
	return twi->reg_twdr >> 1;
}

static void
twdr_write(void *clientData, uint8_t value, uint32_t address)
{
	ATM644_Twi *twi = (ATM644_Twi *) clientData;
	twi->reg_twdr = ((uint16_t) value << 1);
	if (!(twi->reg_twcr & TWCR_TWINT)) {
		twi->reg_twcr |= TWCR_TWWC;
		fprintf(stderr, "Possible driver bug: Writing TWDR while not TWINT\n");
	} else {
		twi->reg_twcr &= ~TWCR_TWWC;
	}
}

static uint8_t
twcr_read(void *clientData, uint32_t address)
{
	ATM644_Twi *twi = (ATM644_Twi *) clientData;
	return twi->reg_twcr;
}

static void
twcr_write(void *clientData, uint8_t value, uint32_t address)
{
	ATM644_Twi *twi = (ATM644_Twi *) clientData;
	uint8_t diff = twi->reg_twcr ^ value;
	uint8_t keep = TWCR_TWWC | TWCR_TWINT;
	twi->reg_twcr = (value & ~keep) | (twi->reg_twcr & keep);
	if (diff & twi->reg_twcr & TWCR_TWEN) {
		tw_enable(twi);
	} else if (diff & ~twi->reg_twcr & TWCR_TWEN) {
		tw_disable(twi);
	}
	if (diff & TWCR_TWIE) {
		update_interrupt(twi);
	}
	if (diff & value & TWCR_TWSTA) {
		update_interrupt(twi);
		if (twi->bus_free) {
			reset_interpreter(twi);
			assemble_script_start(twi, STARTMODE_START);
			start_interpreter(twi);
		}
	} else if (diff & ~value & TWCR_TWSTA) {
		/* abort the script  if still running ??? */
	}
	/* What to do if start and stop are set at the same time ? */
	if (diff & value & TWCR_TWSTO) {
#if 0
		reset_interpreter(twi);
		assemble_script_stop(twi);
		start_interpreter(twi);
#endif
		//ADD_CODE(twi,INSTR_INTERRUPT | SR_TXRDY);
		/* Stop bit is cleared automatically when stop is done */
		/* Missing: Reset the slave here also !!!!!!!!!!!!!!!! */
	}
	if (value & twi->reg_twcr & TWCR_TWINT) {
		twi->reg_twcr &= ~TWCR_TWINT;
		ack_irq(twi);
	}
}

static uint8_t
twamr_read(void *clientData, uint32_t address)
{
	ATM644_Twi *twi = (ATM644_Twi *) clientData;
	return twi->reg_twamr;
}

static void
twamr_write(void *clientData, uint8_t value, uint32_t address)
{
	ATM644_Twi *twi = (ATM644_Twi *) clientData;
	twi->reg_twamr = value;
}

void
ATM644_TwiNew(const char *name)
{
	uint32_t base = 0;
	ATM644_Twi *twi = sg_new(ATM644_Twi);
	twi->irqNode = SigNode_New("%s.irq", name);
	if (!twi->irqNode) {
		fprintf(stderr, "Can not create signal lines for ATMega644 TWI module\n");
		exit(1);
	}
	twi->reg_twbr = 0;
	twi->reg_twsr = 0;
	twi->clk_in = Clock_New("%s.clk", name);
	twi->twi_clk = Clock_New("%s.twi_clk", name);
	twi->sdaNode = SigNode_New("%s.sda", name);
	twi->ddoe_sda = SigNode_New("%s.ddoe_sda", name);
	twi->ddov_sda = SigNode_New("%s.ddov_sda", name);
	twi->pvoe_sda = SigNode_New("%s.pvoe_sda", name);

	twi->sclNode = SigNode_New("%s.scl", name);
	twi->ddoe_scl = SigNode_New("%s.ddoe_scl", name);
	twi->ddov_scl = SigNode_New("%s.ddov_scl", name);
	twi->pvoe_scl = SigNode_New("%s.pvoe_scl", name);
	twi->expected_bit = -1;
	if (!twi->sdaNode || !twi->ddoe_sda || !twi->ddov_sda || !twi->pvoe_sda) {
		fprintf(stderr, "Can not create SDA lines for ATMega644 TWI module\n");
		exit(1);
	}
	if (!twi->sclNode || !twi->ddoe_scl || !twi->ddov_scl || !twi->pvoe_scl) {
		fprintf(stderr, "Can not create SCL lines for ATMega644 TWI module\n");
		exit(1);
	}

	SigNode_Set(twi->sdaNode, SIG_OPEN);
	SigNode_Set(twi->sclNode, SIG_OPEN);
	twi->old_sda = twi->old_scl = SIG_HIGH;
	AVR8_RegisterIOHandler(TWBR(base), twbr_read, twbr_write, twi);
	AVR8_RegisterIOHandler(TWSR(base), twsr_read, twsr_write, twi);
	AVR8_RegisterIOHandler(TWAR(base), twar_read, twar_write, twi);
	AVR8_RegisterIOHandler(TWDR(base), twdr_read, twdr_write, twi);
	AVR8_RegisterIOHandler(TWCR(base), twcr_read, twcr_write, twi);
	AVR8_RegisterIOHandler(TWAMR(base), twamr_read, twamr_write, twi);
	CycleTimer_Init(&twi->mstr_delay_timer, mstr_run_interpreter, twi);
	CycleTimer_Init(&twi->mstr_scl_timer, scl_timeout, twi);
	CycleTimer_Init(&twi->slv_release_scl_timer, release_scl, twi);

	Clock_Trace(twi->twi_clk, update_timing, twi);
	update_clock(twi);
	fprintf(stderr, "Created ATMegaXX4 Two wire interface \"%s\"\n", name);
}
