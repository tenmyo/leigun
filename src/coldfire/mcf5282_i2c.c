/*
 *************************************************************************************************
 *
 * Emulation of Coldfire MCF5282 I2C Controller 
 *
 * state: Not implemented 
 *
 * Copyright 2008 Jochen Karrer. All rights reserved.
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

#include <bus.h>
#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <bus.h>
#include <fio.h>
#include <signode.h>
#include <configfile.h>
#include <cycletimer.h>
#include <clock.h>
#include <sgstring.h>
#include <mcf5282_i2c.h>
#include "compiler_extensions.h"

#define I2ADR(x) 	((x)+0x300)
#define I2FDR(x)	((x)+0x304)
#define I2CR(x)		((x)+0x308)
#define		I2CR_IEN	(1<<7)
#define		I2CR_IIEN	(1<<6)
#define 	I2CR_MSTA	(1<<5)
#define		I2CR_MTX	(1<<4)
#define		I2CR_TXAK	(1<<3)
#define 	I2CR_RSTA	(1<<2)
#define I2SR(x)		((x)+0x30c)
#define		I2SR_ICF	(1<<7)
#define		I2SR_IAAS	(1<<6)
#define		I2SR_IBB	(1<<5)
#define		I2SR_IAL	(1<<4)
#define		I2SR_SRW	(1<<2)
#define		I2SR_IIF	(1<<1)
#define		I2SR_RXAK	(1<<0)
#define I2DR(x)		((x)+0x310)

/* Definitions for the sequencer */
#define INSTR_SDA_L             (0x01000000)
#define INSTR_SDA_H             (0x02000000)
#define INSTR_SCL_L             (0x03000000)
#define INSTR_SCL_H             (0x04000000)
#define INSTR_CHECK_ARB         (0x05000000)
#define INSTR_NDELAY            (0x06000000)
#define INSTR_SYNC              (0x07000000)
#define INSTR_END               (0x08000000)
#define INSTR_INTERRUPT         (0x09000000)
#define INSTR_CHECK_ACK         (0x0a000000)
#define INSTR_READ_SDA          (0x0b000000)
#define INSTR_READ_ACK          (0x0c000000)
#define INSTR_RXDATA_AVAIL      (0x0d000000)
#define INSTR_WAIT_BUS_FREE     (0x0e000000)

#define RET_DONE                (0)
#define RET_DO_NEXT             (1)
#define RET_EMU_ERROR           (-3)

#define ACK     (1)
#define NACK    (0)
#define CODE_MEM_SIZE 512

#define ADD_CODE(i2c,x) ((i2c)->code[(i2c->icount++) % CODE_MEM_SIZE] = (x))

typedef struct I2C_Timing {
	int speed;
	uint32_t t_hdsta;
	uint32_t t_low;
	uint32_t t_high;
	uint32_t t_susta;
	uint32_t t_hddat_max;
	uint32_t t_sudat;
	uint32_t t_susto;
	uint32_t t_buf;
} I2C_Timing;

#define T_HDSTA(i2c) ((i2c)->timing.t_hdsta)
#define T_LOW(i2c)  ((i2c)->timing.t_low)
#define T_HIGH(i2c) ((i2c)->timing.t_high)
#define T_SUSTA(i2c) ((i2c)->timing.t_susta)
#define T_HDDAT_MAX(i2c) ((i2c)->timing.t_hddat_max)
#define T_HDDAT(i2c) (T_HDDAT_MAX(i2c)>>1)
#define T_SUDAT(i2c) ((i2c)->timing.t_sudat)
#define T_SUSTO(i2c) ((i2c)->timing.t_susto)
#define T_BUF(i2c) ((i2c)->timing.t_buf)

typedef struct I2C {
	BusDevice bdev;
	Clock_t *clockIn;
	Clock_t *clockI2C;
	SigNode *sda;
	SigNode *scl;
	SigNode *irqNode;
	ClockTrace_t *clockTrace;
	I2C_Timing timing;
	uint8_t i2adr;
	uint8_t i2fdr;
	uint8_t i2cr;
	uint8_t i2sr;
	uint8_t i2dr;

	/* The Sequencer */
	SigTrace *sclStretchTrace;
	CycleTimer ndelayTimer;

//        uint8_t rxdata;
	int ack;
	uint16_t ip;
	uint32_t icount;
	uint32_t code[CODE_MEM_SIZE];
} I2C;

static inline int
i2c_divider(unsigned int IC)
{
	uint16_t div[] = {
		28, 30, 34, 40, 44, 48, 56, 68, 80, 88, 104, 128, 144, 160, 192, 240,
		288, 320, 384, 480, 576, 640, 768, 960, 1152, 1280, 1536, 1920, 2304, 2560, 3072,
		    3840,
		20, 22, 24, 26, 28, 32, 36, 40, 48, 56, 64, 72, 80, 96, 112, 128,
		160, 192, 224, 256, 320, 384, 448, 512, 640, 768, 896, 1024, 1280, 1536, 1792, 2048
	};
	if (IC < 0x40) {
		return div[IC];
	} else {
		fprintf(stderr, "CF_I2C Illegal divider selected\n");
		return 2048;
	}
}

/*
 * -----------------------------------------------------------
 * mscript_check_ack
 *      Assemble a micro operation script which checks
 *      for acknowledge.
 *      has to be entered with SCL low for at least T_HDDAT
 * -----------------------------------------------------------
 */
static void __UNUSED__
mscript_check_ack(I2C * i2c)
{
	/* check ack of previous */
	ADD_CODE(i2c, INSTR_SDA_H);
	ADD_CODE(i2c, INSTR_NDELAY | (T_LOW(i2c) - T_HDDAT(i2c)));
	ADD_CODE(i2c, INSTR_SCL_H);
	ADD_CODE(i2c, INSTR_SYNC);
	ADD_CODE(i2c, INSTR_NDELAY | T_HIGH(i2c));
	ADD_CODE(i2c, INSTR_READ_ACK);
	ADD_CODE(i2c, INSTR_SCL_L);
	ADD_CODE(i2c, INSTR_NDELAY | T_HDDAT(i2c));
	ADD_CODE(i2c, INSTR_CHECK_ACK);
}

/*
 * --------------------------------------------------------------
 * mscript_write_byte
 *      Assemble a micro operation script which writes a
 *      byte to the I2C-Bus
 *      Has to be entered with SCL low for at least T_HDDAT
 * --------------------------------------------------------------
 */
static void __UNUSED__
mscript_write_byte(I2C * i2c, uint8_t data)
{
	int i;
	for (i = 7; i >= 0; i--) {
		int bit = (data >> i) & 1;
		if (bit) {
			ADD_CODE(i2c, INSTR_SDA_H);
		} else {
			ADD_CODE(i2c, INSTR_SDA_L);
		}
		ADD_CODE(i2c, INSTR_NDELAY | (T_LOW(i2c) - T_HDDAT(i2c)));
		ADD_CODE(i2c, INSTR_SCL_H);
		ADD_CODE(i2c, INSTR_SYNC);
		ADD_CODE(i2c, INSTR_NDELAY | T_HIGH(i2c));
		if (bit) {
			ADD_CODE(i2c, INSTR_CHECK_ARB);
		}
		ADD_CODE(i2c, INSTR_SCL_L);
		ADD_CODE(i2c, INSTR_NDELAY | T_HDDAT(i2c));
	}
	mscript_check_ack(i2c);
	ADD_CODE(i2c, INSTR_NDELAY | (T_LOW(i2c) - T_HDDAT(i2c)));
}

/*
 * --------------------------------------------------------
 * mscript_do_ack
 *      Assemble a micro operation script which sends
 *      an acknowledge on I2C bus
 *      Enter with SCL low for at least T_HDDAT
 * --------------------------------------------------------
 */
static void __UNUSED__
mscript_do_ack(I2C * i2c, int ack)
{
	if (ack == ACK) {
		ADD_CODE(i2c, INSTR_SDA_L);
	} else {
		/* should already be in this state because do ack is done after reading only */
		ADD_CODE(i2c, INSTR_SDA_H);
	}
	ADD_CODE(i2c, INSTR_NDELAY | (T_LOW(i2c) - T_HDDAT(i2c)));
	ADD_CODE(i2c, INSTR_SCL_H);
	ADD_CODE(i2c, INSTR_SYNC);
	ADD_CODE(i2c, INSTR_NDELAY | T_HIGH(i2c));
	if (ack == NACK) {
		ADD_CODE(i2c, INSTR_CHECK_ARB);
	}
	ADD_CODE(i2c, INSTR_SCL_L);
	ADD_CODE(i2c, INSTR_NDELAY | T_HDDAT(i2c));
}

/*
 * -----------------------------------------------------------------
 * mscript_read_byte
 *      Assemble a micro operation script which reads a byte from
 *      I2C bus
 *      Enter with SCL low and T_HDDAT waited
 * -----------------------------------------------------------------
 */
static void __UNUSED__
mscript_read_byte(I2C * i2c)
{
	int i;
	ADD_CODE(i2c, INSTR_SDA_H);
	for (i = 7; i >= 0; i--) {
		ADD_CODE(i2c, INSTR_NDELAY | (T_LOW(i2c) - T_HDDAT(i2c)));
		ADD_CODE(i2c, INSTR_SCL_H);
		ADD_CODE(i2c, INSTR_SYNC);
		ADD_CODE(i2c, INSTR_NDELAY | T_HIGH(i2c));
		ADD_CODE(i2c, INSTR_READ_SDA);
		ADD_CODE(i2c, INSTR_SCL_L);
		ADD_CODE(i2c, INSTR_NDELAY | (T_HDDAT(i2c)));
	}
	ADD_CODE(i2c, INSTR_RXDATA_AVAIL);
}

/*
 * ----------------------------------------------------------
 * mscript_stop
 *      Assemble a micro operation script which generates a
 *      stop condition on I2C bus.
 *      Enter with SCL low for at least T_HDDAT
 * ----------------------------------------------------------
 */
static void __UNUSED__
mscript_stop(I2C * i2c)
{
	ADD_CODE(i2c, INSTR_SDA_L);
	ADD_CODE(i2c, INSTR_NDELAY | (T_LOW(i2c) - T_HDDAT(i2c)));
	ADD_CODE(i2c, INSTR_SCL_H);
	ADD_CODE(i2c, INSTR_SYNC);
	ADD_CODE(i2c, INSTR_NDELAY | T_SUSTO(i2c));
	ADD_CODE(i2c, INSTR_SDA_H);
	ADD_CODE(i2c, INSTR_NDELAY | T_BUF(i2c));
	ADD_CODE(i2c, INSTR_INTERRUPT);
}

/*
 * ------------------------------------------------------------
 * mscript_start
 *      Assemble a micro operation script which generates
 *      a start/repeated start condition on I2C bus.
 *      If it is a repeated start condition  enter with
 *      SCL low for at least T_HDDAT
 *      else with SDA and SCL high.
 * ------------------------------------------------------------
 */
#define STARTMODE_REPSTART (1)
#define STARTMODE_START (2)
static void __UNUSED__
mscript_start(I2C * i2c, int startmode)
{
	/* For repeated start do not assume SDA and SCL state */
	if (startmode == STARTMODE_REPSTART) {
		ADD_CODE(i2c, INSTR_SDA_H);
		ADD_CODE(i2c, INSTR_NDELAY | (T_LOW(i2c) - T_HDDAT(i2c)));

		ADD_CODE(i2c, INSTR_SCL_H);
		ADD_CODE(i2c, INSTR_SYNC);
		ADD_CODE(i2c, INSTR_NDELAY | T_HIGH(i2c));
	} else {
		ADD_CODE(i2c, INSTR_WAIT_BUS_FREE);
	}

	/* Generate a start condition */
	ADD_CODE(i2c, INSTR_CHECK_ARB);
	ADD_CODE(i2c, INSTR_SDA_L);
	ADD_CODE(i2c, INSTR_NDELAY | T_HDSTA(i2c));
	ADD_CODE(i2c, INSTR_SCL_L);
	ADD_CODE(i2c, INSTR_NDELAY | T_HDDAT(i2c));
}

static void run_interpreter(void *clientData);

static void
scl_timeout(void *clientData)
{
	I2C *i2c = (I2C *) clientData;
	SigNode *dom = SigNode_FindDominant(i2c->scl);
	if (dom) {
		fprintf(stderr, "CF-I2C: I2C SCL seems to be blocked by %s\n", SigName(dom));
	} else {
		fprintf(stderr, "CF-I2C: I2C SCL seems to be blocked\n");
	}
	SigNode_Untrace(i2c->scl, i2c->sclStretchTrace);
}

/*
 * -------------------------------------------------------------------
 * scl_released
 *      The SCL trace proc for clock stretching
 *      If it is called before timeout it will
 *      remove the timer and continue running the interpreter
 * -------------------------------------------------------------------
 */
static void
scl_released(SigNode * node, int value, void *clientData)
{
	I2C *i2c = (I2C *) clientData;
	if ((value == SIG_PULLUP) || (value == SIG_HIGH)) {
		SigNode_Untrace(i2c->scl, i2c->sclStretchTrace);
		CycleTimer_Remove(&i2c->ndelayTimer);
		run_interpreter(clientData);
	}
}

static int
execute_instruction(I2C * i2c)
{
	uint32_t icode;
	if (i2c->ip >= CODE_MEM_SIZE) {
		fprintf(stderr, "CF-I2C: corrupt I2C script\n");
		return RET_EMU_ERROR;
	}
	if (i2c->ip == (i2c->icount % CODE_MEM_SIZE)) {
		return RET_DONE;
	}
	icode = i2c->code[i2c->ip];
	i2c->ip = (i2c->ip + 1) % CODE_MEM_SIZE;
	switch (icode & 0xff000000) {
	    case INSTR_SDA_H:
		    SigNode_Set(i2c->sda, SIG_OPEN);
		    break;
	    case INSTR_SDA_L:
		    SigNode_Set(i2c->sda, SIG_LOW);
		    break;
	    case INSTR_SCL_H:
		    SigNode_Set(i2c->scl, SIG_OPEN);
		    break;
	    case INSTR_SCL_L:
		    SigNode_Set(i2c->scl, SIG_LOW);
		    break;

	    case INSTR_CHECK_ARB:
		    /* This core has no arbitration check */
		    break;

	    case INSTR_NDELAY:
		    {
			    uint32_t nsecs = icode & 0xffffff;
			    int64_t cycles = NanosecondsToCycles(nsecs);
			    CycleTimer_Add(&i2c->ndelayTimer, cycles, run_interpreter, i2c);
		    }
		    return RET_DONE;
		    break;

	    case INSTR_SYNC:
		    if (SigNode_Val(i2c->scl) == SIG_LOW) {
			    uint32_t msecs = 200;
			    int64_t cycles = MillisecondsToCycles(msecs);
			    i2c->sclStretchTrace = SigNode_Trace(i2c->scl, scl_released, i2c);
			    CycleTimer_Add(&i2c->ndelayTimer, cycles, scl_timeout, i2c);
			    return RET_DONE;
		    }
		    break;

	    case INSTR_READ_SDA:
		    if (SigNode_Val(i2c->sda) == SIG_LOW) {
			    //i2c->rxdata = (i2c->rxdata<<1);
		    } else {
			    //i2c->rxdata = (i2c->rxdata<<1) | 1;
		    }
		    break;

	    case INSTR_READ_ACK:
		    if (SigNode_Val(i2c->sda) == SIG_LOW) {
			    //i2c->ack = ACK;
		    } else {
			    //i2c->ack = NACK;
		    }
		    break;

	    case INSTR_END:
		    //reset_interpreter(i2c);
		    return RET_DONE;
		    break;

	    case INSTR_CHECK_ACK:
		    if (i2c->ack == NACK) {
			    //i2c->sr |= SR_NACK | SR_TXRDY | SR_TXCOMP;
			    //update_interrupt(i2c);
			    return RET_DONE;
		    }
		    break;

	    case INSTR_INTERRUPT:
		    //i2c->sr |= icode & 0xffff;
		    //update_interrupt(i2c);
		    break;

	    case INSTR_RXDATA_AVAIL:
		    //i2c->rhr = i2c->rxdata;
		    break;

	    case INSTR_WAIT_BUS_FREE:
		    /* Wait bus free currently not implemented */
		    break;

	    default:
		    fprintf(stderr, "CF-I2C: I2C: Unknown icode %08x\n", icode);
		    return RET_EMU_ERROR;
		    break;

	}
	return RET_DO_NEXT;
}

/*
 * ----------------------------------------------------------------
 * run_interpreter
 *      The I2C-master micro instruction interpreter main loop
 *      executes instructions until the script is done ore
 *      waits for some event
 * ----------------------------------------------------------------
 */
static void
run_interpreter(void *clientData)
{
	I2C *i2c = (I2C *) clientData;
	int retval;
	do {
		retval = execute_instruction(i2c);
	} while (retval == RET_DO_NEXT);
}

/*
 ********************************************************************************
 * All timing is in nanoseconds
 ********************************************************************************
 */
static void
clock_trace(Clock_t * clock, void *clientData)
{
	I2C *i2c = (I2C *) clientData;
	I2C_Timing *timing = &i2c->timing;
	uint32_t freq = Clock_Freq(clock);
	uint32_t period;
	if (freq < 10) {
		fprintf(stderr, "Bad I2C frequency\n");
		return;
	}
	period = 1000000000 / freq;
	timing->t_hdsta = period * 4000 / 10000;
	timing->t_low = period * 4700 / 10000;
	timing->t_high = period * 4000 / 10000;
	timing->t_susta = period * 4700 / 10000;
	timing->t_hddat_max = period * 3450 / 10000;
	timing->t_sudat = period * 250 / 10000;
	timing->t_susto = period * 4000 / 10000;
	timing->t_buf = period * 4700 / 10000;
}

static void
update_clock(I2C * i2c)
{
	int ic = i2c->i2fdr & 0x3f;
	int divider = i2c_divider(ic);
	Clock_MakeDerived(i2c->clockI2C, i2c->clockIn, 1, divider);
}

/*
 ***********************************************************
 *  I2C Slave address 
 ***********************************************************
 */
static uint32_t
i2adr_read(void *clientData, uint32_t address, int rqlen)
{
	I2C *i2c = (I2C *) clientData;
	return i2c->i2adr;
}

static void
i2adr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "I2ADR not implented\n");
	// reregister Slave
}

static uint32_t
i2fdr_read(void *clientData, uint32_t address, int rqlen)
{
	I2C *i2c = (I2C *) clientData;
	return i2c->i2fdr;
}

static void
i2fdr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	I2C *i2c = (I2C *) clientData;
	i2c->i2fdr = value & 0x3f;
	update_clock(i2c);
}

static uint32_t
i2cr_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "I2CR not implented\n");
	return 0;
}

static void
i2cr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "I2CR not implented\n");
}

static uint32_t
i2sr_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "I2SR not implented\n");
	return 0;
}

static void
i2sr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "I2SR not implented\n");
}

static uint32_t
i2dr_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "I2DR not implented\n");
	return 0;
}

static void
i2dr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "I2DR not implented\n");
}

static void
I2c_Unmap(void *owner, uint32_t base, uint32_t mask)
{
	IOH_Delete8(I2ADR(base));
	IOH_Delete8(I2FDR(base));
	IOH_Delete8(I2CR(base));
	IOH_Delete8(I2SR(base));
	IOH_Delete8(I2DR(base));
}

static void
I2c_Map(void *owner, uint32_t base, uint32_t mask, uint32_t mapflags)
{
	I2C *i2c = (I2C *) owner;
	IOH_New8(I2ADR(base), i2adr_read, i2adr_write, i2c);
	IOH_New8(I2FDR(base), i2fdr_read, i2fdr_write, i2c);
	IOH_New8(I2CR(base), i2cr_read, i2cr_write, i2c);
	IOH_New8(I2SR(base), i2sr_read, i2sr_write, i2c);
	IOH_New8(I2DR(base), i2dr_read, i2dr_write, i2c);
}

BusDevice *
MFC5282_I2cNew(const char *name)
{
	I2C *i2c = sg_calloc(sizeof(I2C));
	i2c->bdev.first_mapping = NULL;
	i2c->bdev.Map = I2c_Map;
	i2c->bdev.UnMap = I2c_Unmap;
	i2c->bdev.owner = i2c;
	i2c->bdev.hw_flags = MEM_FLAG_WRITABLE | MEM_FLAG_READABLE;
	i2c->i2adr = 0;
	i2c->i2fdr = 0;
	i2c->i2cr = 0;
	i2c->i2sr = 0x81;
	i2c->i2dr = 0;
	i2c->clockIn = Clock_New("%s.inclk", name);
	i2c->clockI2C = Clock_New("%s.i2cclk", name);
	i2c->clockTrace = Clock_Trace(i2c->clockI2C, clock_trace, i2c);
	i2c->irqNode = SigNode_New("%s.irq", name);
	i2c->sda = SigNode_New("%s.sda", name);
	i2c->scl = SigNode_New("%s.scl", name);
	if (!i2c->irqNode || !i2c->sda || !i2c->scl) {
		fprintf(stderr, "CF-I2C: Can not create signal lines\n");
		exit(1);
	}

	update_clock(i2c);
	return &i2c->bdev;
}
