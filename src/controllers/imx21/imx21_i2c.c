/*
 *************************************************************************************************
 *
 * Emulation of the i.MX21 I2C controller 
 *
 * State:
 *	Nothing is working
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

#if 0
#define dbgprintf(x...) { fprintf(stderr,x); }
#else
#define dbgprintf(x...)
#endif

#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <sys/fcntl.h>
#include "bus.h"
#include "fio.h"
#include "signode.h"
#include "imx21_i2c.h"
#include "configfile.h"
#include "cycletimer.h"
#include "sgstring.h"

#define IADR(base) ((base)+0)
#define IFDR(base) ((base)+4)
#define I2CR(base) ((base)+8)
#define		I2CR_IEN	(1<<7)
#define		I2CR_IIEN	(1<<6)
#define		I2CR_MSTA	(1<<5)
#define		I2CR_MTX	(1<<4)
#define		I2CR_TXAK	(1<<3)
#define		I2CR_RSTA	(1<<2)
#define I2SR(base) ((base)+0xc)
#define		I2SR_ICF	(1<<7)
#define		I2SR_IAAS	(1<<6)
#define		I2SR_IBB	(1<<5)
#define		I2SR_IAL	(1<<4)
#define		I2SR_SRW	(1<<2)
#define		I2SR_IIF	(1<<1)
#define		I2SR_RXAK	(1<<0)
#define I2DR(base) ((base)+0x10)

/* The microengine definitions */

#define CODE_MEM_SIZE (512)

#define INSTR_SDA_L             (0x01000000)
#define INSTR_SDA_H             (0x02000000)
#define INSTR_SCL_L             (0x03000000)
#define INSTR_SCL_H             (0x04000000)
#define INSTR_CHECK_ARB         (0x05000000)
#define INSTR_NDELAY            (0x06000000)
#define INSTR_SYNC              (0x07000000)
#define INSTR_ENDSCRIPT         (0x08000000)
#define INSTR_INTERRUPT         (0x09000000)
#define INSTR_CHECK_ACK         (0x0a000000)
#define INSTR_READ_SDA          (0x0b000000)
#define INSTR_READ_ACK          (0x0c000000)
#define INSTR_RXDATA_AVAIL      (0x0d000000)
#define INSTR_WAIT_BUS_FREE     (0x0e000000)

/* The timings */
#define T_HDSTA(i2c) ((i2c)->i2c_timing.t_hdsta)
#define T_LOW(i2c)  ((i2c)->i2c_timing.t_low)
#define T_HIGH(i2c) ((i2c)->i2c_timing.t_high)
#define T_SUSTA(i2c) ((i2c)->i2c_timing.t_susta)
#define T_HDDAT_MAX(i2c) ((i2c)->i2c_timing.t_hddat_max)
#define T_HDDAT(i2c) (T_HDDAT_MAX(i2c)>>1)
#define T_SUDAT(i2c) ((i2c)->i2c_timing.t_sudat)
#define T_SUSTO(i2c) ((i2c)->i2c_timing.t_susto)
#define T_BUF(i2c) ((i2c)->i2c_timing.t_buf)

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

#define MSTATE_IDLE (0)
#define MSTATE_READ (1)
#define MSTATE_WRITE (2)

typedef struct IMX21I2c {
	BusDevice bdev;
	I2C_Timing i2c_timing;
	uint8_t iadr;
	uint8_t ifdr;
	uint8_t i2cr;
	uint8_t i2sr;
	uint8_t i2dr;
	SigNode *irqNode;
	SigNode *sdaNode;
	SigNode *sclNode;
	SigTrace *sdaTrace;
	int mstate;

	/* The microengine interpreter */
	SigTrace *sclStretchTrace;
	CycleTimer ndelayTimer;
	uint8_t rxdata;		/* The input register */
	int ack;
	uint16_t ip;
	uint16_t icount;
	uint32_t code[CODE_MEM_SIZE];
	int wait_bus_free;
} IMX21I2c;

#define RET_DONE		(0)
#define RET_DO_NEXT		(1)
#define RET_INTERP_ERROR	(-3)

#define ACK (1)
#define NACK (0)

/* check if bus is free */

static inline int
bus_is_free(IMX21I2c * i2c)
{
	return !(i2c->i2sr & I2SR_IBB);
}

static void
update_interrupt(IMX21I2c * i2c)
{
	if (i2c->i2sr & I2SR_IIF) {
		SigNode_Set(i2c->irqNode, SIG_LOW);
	} else {
		SigNode_Set(i2c->irqNode, SIG_HIGH);
	}
}

static void run_interpreter(void *clientData);

/*
 * -----------------------------------------------
 * When SCL line is blocked forever call this
 * -----------------------------------------------
 */
static void
scl_timeout(void *clientData)
{
	IMX21I2c *i2c = (IMX21I2c *) clientData;
	fprintf(stderr, "IMX21I2c: SCL line seems to be blocked\n");
	SigNode_Untrace(i2c->sclNode, i2c->sclStretchTrace);
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
	IMX21I2c *i2c = (IMX21I2c *) clientData;
	if ((value == SIG_PULLUP) || (value == SIG_HIGH)) {
		SigNode_Untrace(i2c->sclNode, i2c->sclStretchTrace);
		CycleTimer_Remove(&i2c->ndelayTimer);
		run_interpreter(clientData);
	}
}

/*
 * --------------------------------------------------------------------
 * The Interpreter
 * --------------------------------------------------------------------
 */
static int
execute_instruction(IMX21I2c * i2c)
{
	uint32_t icode;
	if (i2c->ip >= CODE_MEM_SIZE) {
		fprintf(stderr, "i.MX21 I2C: corrupt I2C script\n");
		return RET_INTERP_ERROR;
	}
	if (i2c->ip == i2c->icount) {
		return RET_DONE;
	}
	icode = i2c->code[i2c->ip++];
	switch (icode & 0xff000000) {
	    case INSTR_SDA_H:
		    dbgprintf("SDA_H %08x\n", icode);
		    SigNode_Set(i2c->sdaNode, SIG_OPEN);
		    break;
	    case INSTR_SDA_L:
		    dbgprintf("SDA_L %08x\n", icode);
		    SigNode_Set(i2c->sdaNode, SIG_LOW);
		    break;
	    case INSTR_SCL_H:
		    dbgprintf("SCL_H %08x\n", icode);
		    SigNode_Set(i2c->sclNode, SIG_OPEN);
		    break;
	    case INSTR_SCL_L:
		    dbgprintf("SCL_L %08x\n", icode);
		    SigNode_Set(i2c->sclNode, SIG_LOW);
		    break;

	    case INSTR_CHECK_ARB:
		    dbgprintf("CHECK_ARB %08x\n", icode);
		    if (SigNode_Val(i2c->sdaNode) == SIG_LOW) {
			    SigNode *node;
			    /* do lost action */
			    node = SigNode_FindDominant(i2c->sdaNode);
			    if (node) {
				    fprintf(stderr, "Arbitration lost because of \"%s\"\n",
					    SigName(node));
			    }
			    i2c->i2sr |= I2SR_IAL;
			    /* maybe go to slave mode ? */
			    i2c->mstate = MSTATE_IDLE;
			    return RET_DONE;
		    }
		    break;

	    case INSTR_NDELAY:
		    {
			    uint32_t nsecs = icode & 0xffffff;
			    int64_t cycles = NanosecondsToCycles(nsecs);
			    CycleTimer_Add(&i2c->ndelayTimer, cycles, run_interpreter, i2c);
			    dbgprintf("NDELAY %08x\n", icode);
		    }
		    return RET_DONE;
		    break;

	    case INSTR_SYNC:
		    dbgprintf("SYNC %08x\n", icode);
		    if (SigNode_Val(i2c->sclNode) == SIG_LOW) {
			    uint32_t msecs = 200;
			    int64_t cycles = MillisecondsToCycles(msecs);
			    i2c->sclStretchTrace = SigNode_Trace(i2c->sclNode, scl_released, i2c);
			    CycleTimer_Add(&i2c->ndelayTimer, cycles, scl_timeout, i2c);
			    return RET_DONE;
		    }
		    break;

	    case INSTR_READ_SDA:
		    if (SigNode_Val(i2c->sdaNode) == SIG_LOW) {
			    i2c->rxdata = (i2c->rxdata << 1);
		    } else {
			    i2c->rxdata = (i2c->rxdata << 1) | 1;
		    }
		    dbgprintf("READ_SDA %02x %08x\n", i2c->rxdata, icode);
		    break;

	    case INSTR_READ_ACK:
		    dbgprintf("READ_ACK %08x\n", icode);
		    if (SigNode_Val(i2c->sdaNode) == SIG_LOW) {
			    i2c->ack = ACK;
		    } else {
			    i2c->ack = NACK;
		    }
		    break;

	    case INSTR_ENDSCRIPT:
		    dbgprintf("ENDSCRIPT %08x\n", icode);
		    return RET_DONE;
		    break;

	    case INSTR_CHECK_ACK:
		    dbgprintf("CHECK_ACK %08x\n", icode);
		    if (i2c->ack == NACK) {
			    i2c->i2sr |= I2SR_RXAK;
			    return RET_DONE;
		    } else {
			    i2c->i2sr &= ~I2SR_RXAK;
		    }
		    break;

	    case INSTR_INTERRUPT:
		    dbgprintf("INTERRUPT %08x\n", icode);
		    {
			    i2c->i2sr |= I2SR_IIF;
			    update_interrupt(i2c);
			    return RET_DONE;
		    }
		    break;

	    case INSTR_RXDATA_AVAIL:
		    dbgprintf("RXDATA_AVAIL %02x %08x\n", i2c->rxdata, icode);
		    /* Set some data ready flag in a register */
		    i2c->i2dr = i2c->rxdata;;
		    break;

	    case INSTR_WAIT_BUS_FREE:
		    if (!bus_is_free(i2c)) {
			    dbgprintf("Bus not free, waiting !\n");
			    i2c->wait_bus_free = 1;
			    return RET_DONE;
		    }
		    break;

	    default:
		    fprintf(stderr, "i.MX21 I2C: Unknode instruction code %08x\n", icode);
		    return RET_INTERP_ERROR;
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
	IMX21I2c *i2c = (IMX21I2c *) clientData;
	int retval;
	do {
		retval = execute_instruction(i2c);
	} while (retval == RET_DO_NEXT);
}

/*
 * ------------------------------------------------------------------
 * reset_interpreter
 *      reset the I2C-master micro instruction interpreter.
 *      called before assembling a script or on abort of script.
 * ------------------------------------------------------------------
 */
static void
reset_interpreter(IMX21I2c * i2c)
{
	i2c->wait_bus_free = 0;
	i2c->code[0] = INSTR_ENDSCRIPT;
	i2c->ip = 0;
	i2c->icount = 0;
	i2c->rxdata = 0;
	i2c->ack = 0;
	CycleTimer_Remove(&i2c->ndelayTimer);
}

/*
 * --------------------------------------------------------
 * mscript_do_ack
 *      Assemble a micro operation script which sends
 *      an acknowledge on I2C bus
 *      Enter with SCL low for at least T_HDDAT
 * --------------------------------------------------------
 */
static void
mscript_do_ack(IMX21I2c * i2c, int ack)
{
	if (ack == ACK) {
		i2c->code[i2c->icount++] = INSTR_SDA_L;
	} else {
		/* should already be in this state because do ack is done after reading only */
		i2c->code[i2c->icount++] = INSTR_SDA_H;
	}
	i2c->code[i2c->icount++] = INSTR_NDELAY | (T_LOW(i2c) - T_HDDAT(i2c));
	i2c->code[i2c->icount++] = INSTR_SCL_H;
	i2c->code[i2c->icount++] = INSTR_SYNC;
	i2c->code[i2c->icount++] = INSTR_NDELAY | T_HIGH(i2c);
	if (ack == NACK) {
		i2c->code[i2c->icount++] = INSTR_CHECK_ARB;
	}
	i2c->code[i2c->icount++] = INSTR_SCL_L;
	i2c->code[i2c->icount++] = INSTR_NDELAY | T_HDDAT(i2c);
}

void
mscript_start(IMX21I2c * i2c)
{
	/* Repstart */
	if (i2c->mstate != MSTATE_IDLE) {
		i2c->code[i2c->icount++] = INSTR_SDA_H;
		i2c->code[i2c->icount++] = INSTR_NDELAY | (T_LOW(i2c) - T_HDDAT(i2c));

		i2c->code[i2c->icount++] = INSTR_SCL_H;
		i2c->code[i2c->icount++] = INSTR_SYNC;
		i2c->code[i2c->icount++] = INSTR_NDELAY | T_HIGH(i2c);
	} else {
		i2c->code[i2c->icount++] = INSTR_WAIT_BUS_FREE;
		/* instead of t_buf */
		i2c->code[i2c->icount++] = INSTR_NDELAY | 50;
	}

	/* Generate a start condition */
	i2c->code[i2c->icount++] = INSTR_CHECK_ARB;
	i2c->code[i2c->icount++] = INSTR_SDA_L;
	i2c->code[i2c->icount++] = INSTR_NDELAY | T_HDSTA(i2c);
	i2c->code[i2c->icount++] = INSTR_SCL_L;
	i2c->code[i2c->icount++] = INSTR_NDELAY | T_HDDAT(i2c);
}

/*
 * ----------------------------------------------------------
 * mscript_stop
 *      Assemble a micro operation script which generates a
 *      stop condition on I2C bus.
 *      Enter with SCL low for at least T_HDDAT
 * ----------------------------------------------------------
 */
static void
mscript_stop(IMX21I2c * i2c)
{
	i2c->code[i2c->icount++] = INSTR_SDA_L;
	i2c->code[i2c->icount++] = INSTR_NDELAY | (T_LOW(i2c) - T_HDDAT(i2c));
	i2c->code[i2c->icount++] = INSTR_SCL_H;
	i2c->code[i2c->icount++] = INSTR_SYNC;
	i2c->code[i2c->icount++] = INSTR_NDELAY | T_SUSTO(i2c);
	i2c->code[i2c->icount++] = INSTR_SDA_H;
	i2c->code[i2c->icount++] = INSTR_NDELAY | T_BUF(i2c);
}

/*
 * -----------------------------------------------------------
 * mscript_check_ack
 *      Assemble a micro operation script which checks
 *      for acknowledge.
 *      has to be entered with SCL low for at least T_HDDAT
 * -----------------------------------------------------------
 */
static void
mscript_check_ack(IMX21I2c * i2c)
{
	/* check ack of previous */
	i2c->code[i2c->icount++] = INSTR_SDA_H;
	i2c->code[i2c->icount++] = INSTR_NDELAY | (T_LOW(i2c) - T_HDDAT(i2c));
	i2c->code[i2c->icount++] = INSTR_SCL_H;
	i2c->code[i2c->icount++] = INSTR_SYNC;
	i2c->code[i2c->icount++] = INSTR_NDELAY | T_HIGH(i2c);
	i2c->code[i2c->icount++] = INSTR_READ_ACK;
	i2c->code[i2c->icount++] = INSTR_SCL_L;
	i2c->code[i2c->icount++] = INSTR_NDELAY | T_HDDAT(i2c);
	i2c->code[i2c->icount++] = INSTR_CHECK_ACK;
}

/*
 * -----------------------------------------------------------------
 * mscript_read_byte
 *      Assemble a micro operation script which reads a byte from
 *      I2C bus
 *      Enter with SCL low and T_HDDAT waited
 * -----------------------------------------------------------------
 */
static void
mscript_read_byte(IMX21I2c * i2c)
{
	int i;
	i2c->code[i2c->icount++] = INSTR_SDA_H;
	for (i = 7; i >= 0; i--) {
		i2c->code[i2c->icount++] = INSTR_NDELAY | (T_LOW(i2c) - T_HDDAT(i2c));
		i2c->code[i2c->icount++] = INSTR_SCL_H;
		i2c->code[i2c->icount++] = INSTR_SYNC;
		i2c->code[i2c->icount++] = INSTR_NDELAY | T_HIGH(i2c);
		i2c->code[i2c->icount++] = INSTR_READ_SDA;
		i2c->code[i2c->icount++] = INSTR_SCL_L;
		i2c->code[i2c->icount++] = INSTR_NDELAY | (T_HDDAT(i2c));
	}
	i2c->code[i2c->icount++] = INSTR_RXDATA_AVAIL;
	i2c->code[i2c->icount++] = INSTR_INTERRUPT;
}

/*
 * --------------------------------------------------------------
 * mscript_write_byte
 *      Assemble a micro operation script which writes a
 *      byte to the I2C-Bus
 *      Has to be entered with SCL low for at least T_HDDAT
 * --------------------------------------------------------------
 */
static void
mscript_write_byte(IMX21I2c * i2c, uint8_t data)
{
	int i;
	for (i = 7; i >= 0; i--) {
		int bit = (data >> i) & 1;
		if (bit) {
			i2c->code[i2c->icount++] = INSTR_SDA_H;
		} else {
			i2c->code[i2c->icount++] = INSTR_SDA_L;
		}
		i2c->code[i2c->icount++] = INSTR_NDELAY | (T_LOW(i2c) - T_HDDAT(i2c));
		i2c->code[i2c->icount++] = INSTR_SCL_H;
		i2c->code[i2c->icount++] = INSTR_SYNC;
		i2c->code[i2c->icount++] = INSTR_NDELAY | T_HIGH(i2c);
		if (bit) {
			i2c->code[i2c->icount++] = INSTR_CHECK_ARB;
		}
		i2c->code[i2c->icount++] = INSTR_SCL_L;
		i2c->code[i2c->icount++] = INSTR_NDELAY | T_HDDAT(i2c);
	}
	mscript_check_ack(i2c);
}

/*
 * -----------------------------------------------------------------------------
 * start script
 *      Start execution of micro operation I2C scripts
 * -----------------------------------------------------------------------------
 */
static void
start_script(IMX21I2c * i2c)
{
	if (CycleTimer_IsActive(&i2c->ndelayTimer)) {
		fprintf(stderr, "imx i2c emulator bug: Starting script which already runs\n");
		return;
	}
	CycleTimer_Add(&i2c->ndelayTimer, 0, run_interpreter, i2c);
}

/*
 * --------------------------------------------------------
 * Monitor bus Status (start/stop conditions)
 * --------------------------------------------------------
 */
static void
sda_trace_proc(struct SigNode *node, int value, void *clientData)
{
	IMX21I2c *i2c = (IMX21I2c *) clientData;
	int sda = value;
	int scl = SigNode_Val(i2c->sclNode);
	/* detect start condition */
	if ((sda == SIG_LOW) && (scl != SIG_LOW)) {
		/* Set bus busy bit */
		i2c->i2sr |= I2SR_IBB;
	} else if ((sda == SIG_HIGH) && (scl != SIG_LOW)) {
		/* Clear bus busy */
		i2c->i2sr |= ~I2SR_IBB;
		/* Check if microengine start required */
		if (i2c->wait_bus_free) {
			uint32_t nsec = T_BUF(i2c);
			int64_t cycles = NanosecondsToCycles(nsec);
			i2c->wait_bus_free = 0;
			CycleTimer_Add(&i2c->ndelayTimer, cycles, run_interpreter, i2c);
		}
	}
}

static uint32_t
iadr_read(void *clientData, uint32_t address, int rqlen)
{
	IMX21I2c *i2c = (IMX21I2c *) clientData;
	return i2c->iadr;;
}

static void
iadr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	IMX21I2c *i2c = (IMX21I2c *) clientData;
	i2c->iadr = value & 0xfe;
	/* should be registered with changed slave address now */
//      fprintf(stderr,"I2c register 0x%08x not implemented\n",address);
}

static uint32_t
ifdr_read(void *clientData, uint32_t address, int rqlen)
{
	IMX21I2c *i2c = (IMX21I2c *) clientData;
	return i2c->ifdr;
}

static void
ifdr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	IMX21I2c *i2c = (IMX21I2c *) clientData;
	i2c->ifdr = value & 0x3f;
}

static uint32_t
i2cr_read(void *clientData, uint32_t address, int rqlen)
{
	IMX21I2c *i2c = (IMX21I2c *) clientData;
	return i2c->i2cr;
}

static void
i2cr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	IMX21I2c *i2c = (IMX21I2c *) clientData;
	uint32_t diff = value ^ i2c->i2cr;
	if (!(i2c->i2cr & I2CR_IEN)) {
		i2c->i2cr = value & 0xfc;
		return;
	}
	if (diff & value & I2CR_MSTA) {
		reset_interpreter(i2c);
		mscript_start(i2c);
	} else if (diff & ~value & I2CR_MSTA) {
		/* Where do I know from that transfer is done ? */
		reset_interpreter(i2c);
		mscript_stop(i2c);
		start_script(i2c);
	}
	i2c->i2cr = value & 0xfc;
}

static uint32_t
i2sr_read(void *clientData, uint32_t address, int rqlen)
{
	IMX21I2c *i2c = (IMX21I2c *) clientData;
	return i2c->i2sr;
}

static void
i2sr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	IMX21I2c *i2c = (IMX21I2c *) clientData;
	uint8_t clearbits = ~value & (I2SR_IAL | I2SR_IIF);
	i2c->i2sr = i2c->i2sr & ~clearbits;
	update_interrupt(i2c);
}

static uint32_t
i2dr_read(void *clientData, uint32_t address, int rqlen)
{
	IMX21I2c *i2c = (IMX21I2c *) clientData;
	if (i2c->mstate == MSTATE_READ) {
		reset_interpreter(i2c);
		if (i2c->i2cr & I2CR_TXAK) {
			mscript_do_ack(i2c, NACK);
		} else {
			mscript_do_ack(i2c, ACK);
		}
		mscript_read_byte(i2c);
	}
	return i2c->i2dr;
}

static void
i2dr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	IMX21I2c *i2c = (IMX21I2c *) clientData;
	if ((i2c->i2cr & I2CR_MSTA) && (i2c->mstate == MSTATE_IDLE)) {
		mscript_write_byte(i2c, value & 0xff);
		if (value & 1) {
			i2c->mstate = MSTATE_READ;
			mscript_read_byte(i2c);
		} else {
			i2c->mstate = MSTATE_WRITE;
		}
		start_script(i2c);
	} else if (i2c->mstate == MSTATE_WRITE) {
		/* Check if still running. If yes print "shit" */
		reset_interpreter(i2c);
		mscript_write_byte(i2c, value & 0xff);
		start_script(i2c);
	} else {
		fprintf(stderr, "I2DR: trying to write in non write state\n");
	}
}

static void
IMXI2c_Unmap(void *owner, uint32_t base, uint32_t mask)
{
	IOH_Delete16(IADR(base));
	IOH_Delete16(IFDR(base));
	IOH_Delete16(I2CR(base));
	IOH_Delete16(I2SR(base));
	IOH_Delete16(I2DR(base));
}

static void
IMXI2c_Map(void *owner, uint32_t base, uint32_t mask, uint32_t mapflags)
{

	IMX21I2c *i2c = (IMX21I2c *) owner;
	IOH_New16(IADR(base), iadr_read, iadr_write, i2c);
	IOH_New16(IFDR(base), ifdr_read, ifdr_write, i2c);
	IOH_New16(I2CR(base), i2cr_read, i2cr_write, i2c);
	IOH_New16(I2SR(base), i2sr_read, i2sr_write, i2c);
	IOH_New16(I2DR(base), i2dr_read, i2dr_write, i2c);
}

BusDevice *
IMX21_I2cNew(const char *name)
{
	I2C_Timing *timing;
	IMX21I2c *i2c = sg_new(IMX21I2c);
	timing = &i2c->i2c_timing;
	i2c->irqNode = SigNode_New("%s.irq", name);
	if (!i2c->irqNode) {
		fprintf(stderr, "i.MX21 I2C: can not create interrupt line\n");
		exit(1);
	}
	i2c->sdaNode = SigNode_New("%s.sda", name);
	i2c->sclNode = SigNode_New("%s.scl", name);
	if (!i2c->sclNode || !i2c->sdaNode) {
		fprintf(stderr, "Can not create I2C signal nodes\n");
		exit(342);
	}
	i2c->sdaTrace = SigNode_Trace(i2c->sdaNode, sda_trace_proc, i2c);
	/* Currently fixed to low speed */
	timing->t_hdsta = 4000;
	timing->t_low = 4700;
	timing->t_high = 4000;
	timing->t_susta = 4700;
	timing->t_hddat_max = 3450;
	timing->t_sudat = 250;
	timing->t_susto = 4000;
	timing->t_buf = 4700;

	i2c->bdev.first_mapping = NULL;
	i2c->bdev.Map = IMXI2c_Map;
	i2c->bdev.UnMap = IMXI2c_Unmap;
	i2c->bdev.owner = i2c;
	i2c->bdev.hw_flags = MEM_FLAG_WRITABLE | MEM_FLAG_READABLE;
	fprintf(stderr, "i.MX21 I2C-Controller \"%s\" created\n", name);
	return &i2c->bdev;
}
