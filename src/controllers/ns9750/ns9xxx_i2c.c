/*
 *************************************************************************************************
 *
 * Emulation of the NS9xxx I2C Controller
 *
 * State:
 *	Master and slave working
 *	10 Bit mode and General call address not implemented, 
 * 	multimaster is not tested, spike_filter is only emulated
 *	for t_buf
 *	
 *  	Warning: Multimaster in real device is broken (t_buf is not kept,
 *	Arbitration loss blocks SCL, repeated start is 6.5536 ms delayed)
 *
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

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "i2c.h"
#include "i2c_serdes.h"
#include "bus.h"
#include "signode.h"
#include "cycletimer.h"
#include "ns9xxx_i2c.h"
#include "ns9750_timer.h"	/* should be removed, required for irq */
#include "sgstring.h"

#if 0
#define dbgprintf(x...) { fprintf(stderr,x); }
#else
#define dbgprintf(x...)
#endif

#define NS9xxx_I2C_BASE 0x90500000

#define NS_I2C_CTDR	(0x0)
#define		CTDR_PIPE 	(1<<15)
#define		CTDR_DLEN	(1<<14)
#define		CTDR_TXVAL	(1<<13)
#define		CTDR_CMD_MASK	(0x1f<<8)
#define		CTDR_CMD_SHIFT		(8)
#define 	CTDR_TXDATA_MASK	(0xff)
#define		CTDR_TXDATA_SHIFT	(0)
#define NS_I2C_STRDR	(0x0)
#define		STRDR_BSTS	(1<<15)
#define		STRDR_RDE	(1<<14)
#define		STRDR_SCMDL	(1<<13)
#define		STRDR_MCMDL	(1<<12)
#define		STRDR_IRQCD_MASK 	(0xf<<8)
#define		STRDR_IRQCD_SHIFT	(0x8)
#define		STRDR_RXDATA_MASK	(0xff)
#define		STRDR_RXDATA_SHIFT	(0)
#define		NO_IRQ			(0)
#define		M_ARBIT_LOST_IRQ	(1)
#define		M_NO_ACK_IRQ		(2)
#define		M_TX_DATA_IRQ		(3)
#define		M_RX_DATA_IRQ		(4)
#define		M_CMD_ACK_IRQ		(5)
#define		S_RX_ABORT_IRQ		(8)
#define		S_CMD_REQ_IRQ		(9)
#define		S_NO_ACK_IRQ		(0xa)
#define		S_TX_DATA_1ST_IRQ	(0xb)
#define		S_RX_DATA_1ST_IRQ	(0xc)
#define		S_TX_DATA_IRQ		(0xd)
#define		S_RX_DATA_IRQ		(0xe)
#define		S_GCA			(0xf)
#define NS_I2C_MAR	(0x4)
#define 	MAR_MAM		(1<<0)
#define NS_I2C_SAR	(0x8)
#define NS_I2C_CFG	(0xc)
#define		CFG_IRQD	(1<<15)
#define		CFG_TMDE	(1<<14)
#define		CFG_VSCD	(1<<13)
#define		CFG_SFW_MASK	(0xf<<9)
#define		CFG_SFW_SHIFT	(9)
#define 	CFG_CLKREF_MASK	(0x1ff)
#define		CFG_CLKREF_SHIFT (0)

#define CMD_M_NOP 	(0)
#define CMD_M_READ	(4)
#define CMD_M_WRITE	(5)
#define CMD_M_STOP	(6)
#define CMD_S_NOP	(0x10)
#define CMD_S_STOP	(0x16)

#define CODE_MEM_SIZE (512)

#define INSTR_SDA_L		(0x01000000)
#define INSTR_SDA_H		(0x02000000)
#define INSTR_SCL_L		(0x03000000)
#define INSTR_SCL_H		(0x04000000)
#define INSTR_CHECK_ARB		(0x05000000)
#define INSTR_NDELAY		(0x06000000)
#define INSTR_SYNC		(0x07000000)
#define INSTR_ENDSCRIPT		(0x08000000)
#define INSTR_INTERRUPT		(0x09000000)
#define INSTR_CHECK_ACK		(0x0a000000)
#define INSTR_READ_SDA		(0x0b000000)
#define INSTR_READ_ACK		(0x0c000000)
#define INSTR_RXDATA_AVAIL	(0x0d000000)
#define INSTR_WAIT_BUS_FREE	(0x0e000000)

#define RET_DONE			(0)
#define RET_DO_NEXT		(1)
#define RET_EMU_ERROR		(-3)

#define T_HDSTA(i2c) ((i2c)->i2c_timing.t_hdsta)
#define T_LOW(i2c)  ((i2c)->i2c_timing.t_low)
#define T_HIGH(i2c) ((i2c)->i2c_timing.t_high)
#define T_SUSTA(i2c) ((i2c)->i2c_timing.t_susta)
#define T_HDDAT_MAX(i2c) ((i2c)->i2c_timing.t_hddat_max)
#define T_HDDAT(i2c) (T_HDDAT_MAX(i2c)>>1)
#define T_SUDAT(i2c) ((i2c)->i2c_timing.t_sudat)
#define T_SUSTO(i2c) ((i2c)->i2c_timing.t_susto)
#define T_BUF(i2c) ((i2c)->i2c_timing.t_buf)
#ifdef DONT_EMULATE_BUGS
#define T_BUF_BAD(i2c)  T_BUF(i2c)
#else
#define T_BUF_BAD(i2c) 100
#endif

#define MSTATE_IDLE (0)
#define MSTATE_READ (1)
#define MSTATE_WRITE (2)

#define SSTATE_IDLE 	(0)
#define SSTATE_RX_FIRST (1)
#define SSTATE_RX	(2)
#define SSTATE_TX	(3)

#define ACK (1)
#define NACK (0)

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

typedef struct NS_I2C {
	BusDevice bdev;
	int interrupt_posted;
	SigNode *irqNode;
	SigNode *resetNode;
	SigNode *sdaNode;
	SigNode *sclNode;
	SigTrace *sdaTrace;
	SigTrace *sclTrace;
	SigTrace *resetTrace;
	uint32_t ctdr;
	uint32_t strdr;
	uint32_t queued_irqcd;
	CycleTimer updateInterruptsTimer;
	uint32_t mar;
	uint32_t sar;
	uint32_t cfg;
	uint32_t sfw_nsec;

	/* Slave functionality */
	I2C_SerDes *serdes;
	I2C_Slave i2c_slave;
	int sstate;
	int scl_stretched;

	int mstate;
	uint32_t clk_period;	/* Nanoseconds */
	I2C_Timing i2c_timing;

	/* The Processor */
	SigTrace *sclStretchTrace;
	CycleTimer ndelayTimer;
	/* Script can wait for bus_free */
	int wait_bus_free;

	uint8_t rxdata;
	int ack;
	uint16_t ip;
	uint16_t icount;
	uint32_t code[CODE_MEM_SIZE];
} NS_I2C;

/*
 * -------------------------------------------------------------
 * update_clock
 * 	Calculate a new set of I2C timings depending on
 *	CLREF and Timing characteristics (TMDE) from 
 *	configuration register
 * -------------------------------------------------------------
 */
static void
update_clock(NS_I2C * i2c, unsigned int clref, int speed)
{
	uint32_t clk = CycleTimerRate_Get() / 4;
	uint32_t i2c_bus_clock;
	uint32_t period;
	I2C_Timing *timing = &i2c->i2c_timing;
	int scl_delay = (int)(clk * 50e-9);
	if (i2c->cfg & CFG_VSCD) {
		clk = clk / 8;
	}
	if (speed == I2C_SPEED_STD) {
		i2c_bus_clock = clk / ((2 * clref) + 4 + scl_delay);
	} else if (speed == I2C_SPEED_FAST) {
		i2c_bus_clock = 3 * (clk / ((clref * 2) + 4 + scl_delay)) / 4;
		i2c_bus_clock *= 2;	/* Suspect bug in manual */
	} else {
		return;
	}
	if (!i2c_bus_clock) {
		fprintf(stderr, "I2C Bus clock is 0 Hz\n");
		return;
	}
	period = 1000000000 / i2c_bus_clock;
	dbgprintf("New period: %d ns, clock %d, scl_delay %d\n", period, i2c_bus_clock, scl_delay);
	timing->speed = speed;
	/* 
	 * ----------------------------------------------------------------------
	 * Calculate relative values derived from the absolute values from
	 * I2C Standard version 2.1
	 * ----------------------------------------------------------------------
	 */
	if (speed == I2C_SPEED_STD) {
		timing->t_hdsta = period * 4000 / 10000;
		timing->t_low = period * 4700 / 10000;
		timing->t_high = period * 4000 / 10000;
		timing->t_susta = period * 4700 / 10000;
		timing->t_hddat_max = period * 3450 / 10000;
		timing->t_sudat = period * 250 / 10000;
		timing->t_susto = period * 4000 / 10000;
		timing->t_buf = period * 4700 / 10000;
	} else if (speed == I2C_SPEED_FAST) {
		/* 
		 * ----------------------------------------------------
		 * Better use the controller in STD with 
		 * faster clock if you want to have high speed 
		 * Netsilicon has shortened high time because it
		 * is shorter in the spec. But the shorter high
		 * time in spec is because of longer rise times.
		 * ----------------------------------------------------
		 */
		timing->t_hdsta = period * 600 / 2500;
		timing->t_low = period * 1300 / 2500;
		timing->t_high = period * 600 / 2500;
		timing->t_susta = period * 600 / 2500;
		timing->t_hddat_max = period * 900 / 2500;
		timing->t_sudat = period * 100 / 2500;
		timing->t_susto = period * 600 / 2500;
		timing->t_buf = period * 1300 / 2500;
	}
	dbgprintf("t_hdsta %d\n", timing->t_hdsta);
	dbgprintf("t_low %d\n", timing->t_low);
	dbgprintf("t_high %d\n", timing->t_high);
	dbgprintf("t_susta %d\n", timing->t_susta);
	dbgprintf("t_sudat %d\n", timing->t_sudat);
	dbgprintf("t_susto %d\n", timing->t_susto);
	dbgprintf("t_buf %d\n", timing->t_buf);
}

/*
 * ----------------------------------------------------
 * update_interrupts
 *	Update the Interrupt signal node whenever
 *	any register changes which has influence 
 * 	on interrupts
 * ----------------------------------------------------
 */
static void
update_interrupts(NS_I2C * i2c)
{
	int irqcd = (i2c->strdr & STRDR_IRQCD_MASK) >> STRDR_IRQCD_SHIFT;
	uint32_t irq_dis = i2c->cfg & CFG_IRQD;

	if (irq_dis || (irqcd == NO_IRQ)) {
		if (i2c->interrupt_posted) {
			dbgprintf("UnPost irq\n");
			SigNode_Set(i2c->irqNode, SIG_HIGH);
			i2c->interrupt_posted = 0;
		}
	} else {
		if (!i2c->interrupt_posted) {
			dbgprintf("Post irq (irqcd %d)\n", irqcd);
			SigNode_Set(i2c->irqNode, SIG_LOW);
			i2c->interrupt_posted = 1;
		}
	}
}

/*
 * -----------------------------------------------------------------
 * queue_irqcd
 *	Queue an interrupt code. If nothing
 *	is in Interrupt register trigger interrupt immediately
 * ------------------------------------------------------------------
 */
static void
queue_irqcd(NS_I2C * i2c, int irqcd)
{
	if (i2c->strdr & STRDR_IRQCD_MASK) {
		if (i2c->queued_irqcd != NO_IRQ) {
			fprintf(stderr, "NS-I2C Bug: More than one irqcd queued\n");
		}
		i2c->queued_irqcd = irqcd;
	} else {
		i2c->strdr = (i2c->strdr & ~(STRDR_IRQCD_MASK)) | (irqcd << STRDR_IRQCD_SHIFT);
		update_interrupts(i2c);
	}
}

/*
 * ----------------------------------------------------------------
 * release_master_lock
 * 	Release master command lock when script is done
 *	real device seems to lock only when transmitting data
 * ----------------------------------------------------------------
 */
static void
release_master_lock(NS_I2C * i2c)
{
	i2c->strdr = i2c->strdr & ~(STRDR_MCMDL);
}

#if 0
/* No clue when slave is locked */
static void
release_slave_lock(NS_I2C * i2c)
{
	i2c->strdr = i2c->strdr & ~(STRDR_SCMDL);
}
#endif

static void
scl_timeout(void *clientData)
{
	NS_I2C *i2c = (NS_I2C *) clientData;
	fprintf(stderr, "NS9xxx I2C SCL seems to be blocked\n");
	SigNode_Untrace(i2c->sclNode, i2c->sclStretchTrace);
}

static void run_interpreter(void *clientData);

/*
 * -------------------------------------------------------------------
 * scl_released
 * 	The SCL trace proc for clock stretching 
 * 	If it is called before timeout it will
 * 	remove the timer and continue running the interpreter
 * -------------------------------------------------------------------
 */
static void
scl_released(SigNode * node, int value, void *clientData)
{
	NS_I2C *i2c = (NS_I2C *) clientData;
	if (value == SIG_HIGH) {
		SigNode_Untrace(i2c->sclNode, i2c->sclStretchTrace);
		CycleTimer_Remove(&i2c->ndelayTimer);
		run_interpreter(clientData);
	}
}

static inline int
bus_is_free(NS_I2C * i2c)
{
	return !(i2c->strdr & STRDR_BSTS);
}

/*
 * --------------------------------------------------------------
 * execute_instruction
 * 	The I2C-master microscript instruction executor. 
 *	returns REG_DO_NEXT when next instruction can
 *	be done immediately, RET_DONE when script is finished	
 *	or next instruction can not be done before an event 
 *	happens.
 * --------------------------------------------------------------
 */
static int
execute_instruction(NS_I2C * i2c)
{
	uint32_t icode;
	if (i2c->ip >= CODE_MEM_SIZE) {
		fprintf(stderr, "NS9xxx I2C: corrupt I2C script\n");
		return RET_EMU_ERROR;
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
			    uint32_t irqcd = icode & 0xffffff;
			    /* do lost action */
			    node = SigNode_FindDominant(i2c->sdaNode);
			    if (node) {
				    fprintf(stderr, "Arbitration lost because of \"%s\"\n",
					    SigName(node));
			    }
			    queue_irqcd(i2c, irqcd);
			    release_master_lock(i2c);
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
			    int irqcd = (icode & 0xf);
			    queue_irqcd(i2c, irqcd);
			    release_master_lock(i2c);
			    return RET_DONE;
		    }
		    break;

	    case INSTR_INTERRUPT:
		    dbgprintf("INTERRUPT %08x\n", icode);
		    {
			    int irqcd = icode & 0xf;
			    queue_irqcd(i2c, irqcd);
			    release_master_lock(i2c);
			    return RET_DONE;
		    }
		    break;

	    case INSTR_RXDATA_AVAIL:
		    dbgprintf("RXDATA_AVAIL %02x %08x\n", i2c->rxdata, icode);
		    i2c->strdr = (i2c->strdr & ~0xff) | STRDR_RDE | i2c->rxdata;;
		    break;

	    case INSTR_WAIT_BUS_FREE:
		    if (!bus_is_free(i2c)) {
			    dbgprintf("Bus not free, waiting !\n");
			    i2c->wait_bus_free = 1;
			    return RET_DONE;
		    }
		    break;

	    default:
		    fprintf(stderr, "NS9xxx I2C: Unknode instruction code %08x\n", icode);
		    return RET_EMU_ERROR;
		    break;

	}
	return RET_DO_NEXT;
}

/*
 * ----------------------------------------------------------------
 * run_interpreter
 * 	The I2C-master micro instruction interpreter main loop
 * 	executes instructions until the script is done ore	
 * 	waits for some event
 * ----------------------------------------------------------------
 */
static void
run_interpreter(void *clientData)
{
	NS_I2C *i2c = (NS_I2C *) clientData;
	int retval;
	do {
		retval = execute_instruction(i2c);
	} while (retval == RET_DO_NEXT);
}

/*
 * ------------------------------------------------------------------
 * reset_interpreter
 * 	reset the I2C-master micro instruction interpreter.
 *	called before assembling a script or on abort of script. 
 * ------------------------------------------------------------------
 */
static void
reset_interpreter(NS_I2C * i2c)
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
 * -----------------------------------------------------------
 * mscript_check_ack
 *      Assemble a micro operation script which checks
 *	for acknowledge. 
 * 	has to be entered with SCL low for at least T_HDDAT 
 * -----------------------------------------------------------
 */
static void
mscript_check_ack(NS_I2C * i2c)
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
	i2c->code[i2c->icount++] = INSTR_CHECK_ACK | M_NO_ACK_IRQ;
}

/*
 * --------------------------------------------------------------
 * mscript_write_byte
 *      Assemble a micro operation script which writes a
 * 	byte to the I2C-Bus
 * 	Has to be entered with SCL low for at least T_HDDAT 
 * --------------------------------------------------------------
 */
static void
mscript_write_byte(NS_I2C * i2c, uint8_t data)
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
			i2c->code[i2c->icount++] = INSTR_CHECK_ARB | M_ARBIT_LOST_IRQ;
		}
		i2c->code[i2c->icount++] = INSTR_SCL_L;
		i2c->code[i2c->icount++] = INSTR_NDELAY | T_HDDAT(i2c);
	}
	mscript_check_ack(i2c);
}

/*
 * --------------------------------------------------------
 * mscript_do_ack
 *	Assemble a micro operation script which sends
 *	an acknowledge on I2C bus
 * 	Enter with SCL low for at least T_HDDAT 
 * --------------------------------------------------------
 */
static void
mscript_do_ack(NS_I2C * i2c, int ack)
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
		i2c->code[i2c->icount++] = INSTR_CHECK_ARB | M_ARBIT_LOST_IRQ;
	}
	i2c->code[i2c->icount++] = INSTR_SCL_L;
	i2c->code[i2c->icount++] = INSTR_NDELAY | T_HDDAT(i2c);
}

/*
 * -----------------------------------------------------------------
 * mscript_read_byte
 *	Assemble a micro operation script which reads a byte from
 *	I2C bus
 * 	Enter with SCL low and T_HDDAT waited
 * -----------------------------------------------------------------
 */
static void
mscript_read_byte(NS_I2C * i2c)
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
	i2c->code[i2c->icount++] = INSTR_INTERRUPT | M_RX_DATA_IRQ;
}

/*
 * ----------------------------------------------------------
 * mscript_stop
 * 	Assemble a micro operation script which generates a
 * 	stop condition on I2C bus.
 * 	Enter with SCL low for at least T_HDDAT 
 * ----------------------------------------------------------
 */
static void
mscript_stop(NS_I2C * i2c)
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
 * ------------------------------------------------------------
 * mscript_start
 *	Assemble a micro operation script which generates
 *	a start/repeated start condition on I2C bus.
 * 	If it is a repeated start condition  enter with 
 *	SCL low for at least T_HDDAT
 *	else with SDA and SCL high.
 * ------------------------------------------------------------
 */
#define STARTMODE_REPSTART (1)
#define STARTMODE_START (2)
static void
mscript_start(NS_I2C * i2c, int startmode)
{
	/* For repeated start do not assume SDA and SCL state */
	if (startmode == STARTMODE_REPSTART) {
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
	i2c->code[i2c->icount++] = INSTR_CHECK_ARB | M_ARBIT_LOST_IRQ;
	i2c->code[i2c->icount++] = INSTR_SDA_L;
	i2c->code[i2c->icount++] = INSTR_NDELAY | T_HDSTA(i2c);
	i2c->code[i2c->icount++] = INSTR_SCL_L;
	i2c->code[i2c->icount++] = INSTR_NDELAY | T_HDDAT(i2c);
}

/*
 * -----------------------------------------------------------------------------
 * start script 
 *	Start execution of micro operation I2C scripts
 * -----------------------------------------------------------------------------
 */
static void
start_script(NS_I2C * i2c)
{
	if (CycleTimer_IsActive(&i2c->ndelayTimer)) {
		fprintf(stderr, "NS-I2C Emulator bug: Starting script which already runs\n");
		return;
	}
	CycleTimer_Add(&i2c->ndelayTimer, 0, run_interpreter, i2c);
}

/*
 * ---------------------------------------------------------------------
 * master_cmd_write 
 *	Assemble and execute a microoperation script
 *	when CMD_M_WRITE is written to the command register.
 *
 * 	Create a start condition and write address and at least 
 * 	1 byte of data.
 * 	If no data is given then ignore the command.
 * 	The state diagramm in NS9750 manual seems to be wrong.
 * 	writing with no data is not possible.
 * ---------------------------------------------------------------------
 */
static void
master_cmd_write(NS_I2C * i2c, uint8_t first_tx_data, int do_tx)
{
	uint8_t data;
	int startmode;
	if (!do_tx) {
		return;
	}
	reset_interpreter(i2c);
	if (i2c->mstate != MSTATE_IDLE) {
		startmode = STARTMODE_REPSTART;
	} else {
		startmode = STARTMODE_START;
	}
	mscript_start(i2c, startmode);
	if (i2c->mar & MAR_MAM) {
		data = 0xf0 | ((i2c->mar & 0x600) >> 8);
		mscript_write_byte(i2c, data);
		data = ((i2c->mar & 0x1fe) >> 1);
		mscript_write_byte(i2c, data);
	} else {
		data = i2c->mar & 0xfe;
		mscript_write_byte(i2c, data);
	}
	mscript_write_byte(i2c, first_tx_data);
	i2c->code[i2c->icount++] = INSTR_NDELAY | (T_LOW(i2c) - T_HDDAT(i2c));
	/* 
	 * -------------------------------------------------------------------------
	 * If do_tx is false
	 * real device writes a byte, triggers never an interrupt and can not be
	 * stopped because it doesn't know that it is in state  write
	 * -------------------------------------------------------------------------
	 */
	if (do_tx) {
		i2c->code[i2c->icount++] = INSTR_INTERRUPT | M_TX_DATA_IRQ;
		i2c->mstate = MSTATE_WRITE;
		i2c->strdr |= STRDR_MCMDL;
	}
	i2c->code[i2c->icount++] = INSTR_ENDSCRIPT;
	start_script(i2c);
	return;
}

/*
 * --------------------------------------------------------------
 * master_cmd_read
 *	Assemble and execute a micro operation script
 *	when CMD_M_READ is written to the command register
 * --------------------------------------------------------------
 */
static void
master_cmd_read(NS_I2C * i2c)
{
	uint8_t data;
	int startmode;
	reset_interpreter(i2c);
	if (i2c->mstate == MSTATE_IDLE) {
		startmode = STARTMODE_START;
	} else {
		startmode = STARTMODE_REPSTART;
	}
	mscript_start(i2c, startmode);
	if (i2c->mar & MAR_MAM) {
		data = 0xf0 | ((i2c->mar & 0x600) >> 8);
		mscript_write_byte(i2c, data);
		data = ((i2c->mar & 0x1fe) >> 1);
		mscript_write_byte(i2c, data);
	} else {
		data = (i2c->mar & 0xfe) | 1;
		mscript_write_byte(i2c, data);
	}
	mscript_read_byte(i2c);
	i2c->mstate = MSTATE_READ;
	start_script(i2c);
}

/*
 * ----------------------------------------------------
 * master_cmd_stop
 *	Assemble and execute a micro operation script
 *	which is done when CMD_M_STOP is written
 *	to the command register  
 * ----------------------------------------------------
 */
static void
master_cmd_stop(NS_I2C * i2c)
{
	reset_interpreter(i2c);
	if (i2c->mstate == MSTATE_READ) {
		mscript_do_ack(i2c, NACK);
	}
	if (i2c->mstate == MSTATE_IDLE) {
		/* Real device does a cmd-ack irq in this case ! */
		fprintf(stderr, "NS-I2C: Stopping while idle\n");
	} else {
		mscript_stop(i2c);
	}
	i2c->code[i2c->icount++] = INSTR_INTERRUPT | M_CMD_ACK_IRQ;
	i2c->code[i2c->icount++] = INSTR_ENDSCRIPT;

	start_script(i2c);
	i2c->mstate = MSTATE_IDLE;
}

/*
 * ---------------------------------------------------------
 * master_cmd_nop
 *	Assemble and execute a micro operation script
 *	when CMD_M_NOP is written to the command register.
 *	The action depends on state from previous commands.
 *	When IDLE do nothing, when in write, write a byte
 *	When in read ack previous and read next byte
 * ---------------------------------------------------------
 */
static void
master_cmd_nop(NS_I2C * i2c, uint8_t txdata, int do_tx)
{
	switch (i2c->mstate) {
	    case MSTATE_IDLE:
		    return;
	    case MSTATE_WRITE:
		    if (!do_tx) {
			    return;
		    }
		    reset_interpreter(i2c);
		    mscript_write_byte(i2c, txdata);
		    i2c->code[i2c->icount++] = INSTR_NDELAY | (T_LOW(i2c) - T_HDDAT(i2c));
		    i2c->code[i2c->icount++] = INSTR_INTERRUPT | M_TX_DATA_IRQ;
		    i2c->code[i2c->icount++] = INSTR_ENDSCRIPT;
		    i2c->strdr |= STRDR_MCMDL;
		    start_script(i2c);
		    break;

	    case MSTATE_READ:
		    reset_interpreter(i2c);
		    mscript_do_ack(i2c, ACK);
		    mscript_read_byte(i2c);
		    start_script(i2c);
		    break;
	}
}

/*
 * ---------------------------------------------------------------------------
 * update_interrupts_timer_handler
 * 	NS9750 leaves IRQ pin high for at least one clock cycle when
 * 	more than one interrupt was queued. So when previous interrupt is acked
 * 	the Interrupt is cleared and this timer handler is installed which will
 * 	send the next interrupt request to cpu later. 
 * ---------------------------------------------------------------------------
 */
static void
update_interrupts_timer_handler(void *clientData)
{
	NS_I2C *i2c = (NS_I2C *) clientData;
	update_interrupts(i2c);
}

/*
 * -------------------------------------------------------------------
 * strdr_read
 * 	The Status/Data register implementation 
 * -------------------------------------------------------------------
 */
static uint32_t
strdr_read(void *clientData, uint32_t address, int rqlen)
{
	NS_I2C *i2c = (NS_I2C *) clientData;
	uint32_t value = i2c->strdr;
	i2c->strdr = i2c->strdr & ~(STRDR_IRQCD_MASK | STRDR_RDE);
	update_interrupts(i2c);
	if (i2c->queued_irqcd != NO_IRQ) {
		i2c->strdr |= (i2c->queued_irqcd << STRDR_IRQCD_SHIFT);
		i2c->queued_irqcd = NO_IRQ;
		CycleTimer_Add(&i2c->updateInterruptsTimer, 1, update_interrupts_timer_handler,
			       i2c);
	}
	dbgprintf("strdr read\n");
	return value;
}

/*
 * -----------------------------------------------------------
 * slave_cmd_nop 
 *	Tell the slave state machine to continue
 *	when it was waiting for data.
 *	Executed when CMD_S_NOP is written to the command
 * -----------------------------------------------------------
 */
static void
slave_cmd_nop(NS_I2C * i2c, uint8_t txdata, int do_tx)
{

	dbgprintf("Slave cmd NOP in state %d, scl_stretched %d\n", i2c->sstate, i2c->scl_stretched);
	if (i2c->sstate == SSTATE_IDLE) {
		fprintf(stderr, "Slave cmd NOP in IDLE state !!\n");
		return;
	}
	if (i2c->scl_stretched) {
		i2c->scl_stretched = 0;
		SerDes_UnstretchScl(i2c->serdes);
		if (do_tx) {
			if (i2c->sstate != SSTATE_TX) {
				fprintf(stderr, "NS i2cslave Got Tx NO but not in TX mode\n");
				return;
			}
		} else {
			if (i2c->sstate != SSTATE_RX && i2c->sstate != SSTATE_RX_FIRST) {
				fprintf(stderr, "NS i2cslave Got rx NO but not in RX mode\n");
				return;
			}
		}
	}
}

/*
 * -------------------------------------------------------------
 * slave_cmd_stop
 *	Early abort of transaction in slave mode.
 *	simply decouples the slave from bus.
 *
 * -------------------------------------------------------------
 */
static void
slave_cmd_stop(NS_I2C * i2c)
{
	if (i2c->sstate != SSTATE_IDLE) {
		i2c->sstate = SSTATE_IDLE;
		i2c->scl_stretched = 0;
		SerDes_Decouple(i2c->serdes);
	} else {
		fprintf(stderr, "Got Slave stop but already idle\n");
	}
}

/*
 * ------------------------------------------------------------------
 * ctdr_write
 * 	The Control/Data register Implementation
 * ------------------------------------------------------------------
 */
static void
ctdr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	NS_I2C *i2c = (NS_I2C *) clientData;
	int txval = value & CTDR_TXVAL;
	uint32_t cmd = (value & CTDR_CMD_MASK) >> CTDR_CMD_SHIFT;
	uint8_t txdata = (value & CTDR_TXDATA_MASK) >> CTDR_TXDATA_SHIFT;
	if (value & (CTDR_PIPE | CTDR_DLEN)) {
		fprintf(stderr, "NS9xxx I2C Pipe and dlen bit in CTDR must be 0\n");
		return;
	}
	i2c->ctdr = value;
	switch (cmd) {
	    case CMD_M_NOP:
		    if (i2c->strdr & STRDR_MCMDL) {
			    fprintf(stderr, "NS-I2C Driver bug: Master Cmd locked\n");
			    return;
		    }
		    master_cmd_nop(i2c, txdata, txval);
		    break;
	    case CMD_M_READ:
		    if (i2c->strdr & STRDR_MCMDL) {
			    fprintf(stderr, "NS-I2C Driver bug: Master Cmd locked\n");
			    return;
		    }
		    master_cmd_read(i2c);
		    break;
	    case CMD_M_WRITE:
		    if (i2c->strdr & STRDR_MCMDL) {
			    fprintf(stderr, "NS-I2C Driver bug: Master Cmd locked\n");
			    return;
		    }
		    master_cmd_write(i2c, txdata, txval);
		    break;
	    case CMD_M_STOP:
		    if (i2c->strdr & STRDR_MCMDL) {
			    fprintf(stderr, "NS-I2C Driver bug: Master Cmd locked\n");
			    return;
		    }
		    master_cmd_stop(i2c);
		    break;

	    case CMD_S_NOP:
		    if (i2c->strdr & STRDR_SCMDL) {
			    fprintf(stderr,
				    "NS-I2C Driver bug: writing to Command register while locked\n");
			    return;
		    }
		    slave_cmd_nop(i2c, txdata, txval);
		    break;
	    case CMD_S_STOP:
		    if (i2c->strdr & STRDR_SCMDL) {
			    fprintf(stderr,
				    "NS-I2C Driver bug: writing to Command register while locked\n");
			    return;
		    }
		    slave_cmd_stop(i2c);
		    break;
	    default:
		    fprintf(stderr, "NS9xxx I2C: illegal command %03x\n", cmd);
	}
}

/*
 * --------------------------------------------------------------
 * mar_read/mar_write 
 *	Master address register implementation.
 *	Simply stores/returns the master address 
 * --------------------------------------------------------------
 */
static uint32_t
mar_read(void *clientData, uint32_t address, int rqlen)
{
	NS_I2C *i2c = (NS_I2C *) clientData;
	dbgprintf("mar read\n");
	return i2c->mar;
}

static void
mar_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	NS_I2C *i2c = (NS_I2C *) clientData;
	i2c->mar = value;
}

/*
 * -------------------------------------------------------
 * sar_read/sar_write
 * 	Slave address register implementation
 *	Simply stores/returs the slave address
 * -------------------------------------------------------
 */

static uint32_t
sar_read(void *clientData, uint32_t address, int rqlen)
{
	NS_I2C *i2c = (NS_I2C *) clientData;
	dbgprintf("sar read\n");
	return i2c->sar;
}

static void
sar_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	NS_I2C *i2c = (NS_I2C *) clientData;
	i2c->sar = value;
}

static uint32_t
cfg_read(void *clientData, uint32_t address, int rqlen)
{
	NS_I2C *i2c = (NS_I2C *) clientData;
	dbgprintf("cfg read\n");
	return i2c->cfg;
}

static void
cfg_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	NS_I2C *i2c = (NS_I2C *) clientData;
	uint32_t clref = value & 0x1ff;
	uint32_t tmde = value & CFG_TMDE;
	uint32_t sfw;
	int speed;
	if (tmde) {
		speed = I2C_SPEED_FAST;
	} else {
		speed = I2C_SPEED_STD;
	}
	i2c->cfg = value;
	sfw = (value & CFG_SFW_MASK) >> CFG_SFW_SHIFT;
	if (value & CFG_VSCD) {
		i2c->sfw_nsec = CyclesToNanoseconds(32) * sfw;
	} else {
		i2c->sfw_nsec = CyclesToNanoseconds(4) * sfw;
	}
	update_clock(i2c, clref, speed);
	update_interrupts(i2c);
}

static void
scl_trace_proc(struct SigNode *node, int value, void *clientData)
{
	return;
}

/*
 * --------------------------------------------------------
 * Monitor bus Status (start/stop conditions)
 * --------------------------------------------------------
 */
static void
sda_trace_proc(struct SigNode *node, int value, void *clientData)
{
	NS_I2C *i2c = (NS_I2C *) clientData;
	int sda = value;
	int scl = SigNode_Val(i2c->sclNode);
	/* detect start condition */
	if ((sda == SIG_LOW) && (scl != SIG_LOW)) {
		i2c->strdr |= STRDR_BSTS;
	} else if ((sda == SIG_HIGH) && (scl != SIG_LOW)) {
		i2c->strdr &= ~STRDR_BSTS;
		if (i2c->wait_bus_free) {
			/* Real device does not care about speed. It has a to small T_BUF always */
			uint32_t nsec = T_BUF_BAD(i2c) - 50 + i2c->sfw_nsec;
			uint32_t good_nsec = T_BUF(i2c);
			int64_t cycles = NanosecondsToCycles(nsec);
			i2c->wait_bus_free = 0;
			CycleTimer_Add(&i2c->ndelayTimer, cycles, run_interpreter, i2c);
			if (nsec < good_nsec) {
				//fprintf(stderr,"Warning: Real NS9750 does not keep T_BUF from I2C-Spec.\nEmulating the bug\n");
			}
		}
	}
	return;
}

/*
 * ----------------------------------------------------
 * reset_trace_proc
 * 	The reset signal has to be connected to the
 * 	master reset register in the BBus Utility and
 * 	is active hight
 * ----------------------------------------------------
 */
static void
reset_trace_proc(struct SigNode *node, int value, void *clientData)
{
	NS_I2C *i2c = (NS_I2C *) clientData;
	if (value == SIG_HIGH) {
		dbgprintf("NS9xxx I2C reset\n");
		I2C_SerDesDetachSlave(i2c->serdes, &i2c->i2c_slave);
		Mem_AreaDeleteMappings(&i2c->bdev);
		i2c->ctdr = 0;
		i2c->strdr = 0;
		i2c->mar = 0;
		i2c->sar = 0x7fe;
		i2c->mstate = MSTATE_IDLE;
		i2c->cfg = (0xf << CFG_SFW_SHIFT) | CFG_VSCD | CFG_TMDE;
		// delete timers
		CycleTimer_Remove(&i2c->ndelayTimer);
		CycleTimer_Remove(&i2c->updateInterruptsTimer);
		SigNode_Set(i2c->sdaNode, SIG_OPEN);
		SigNode_Set(i2c->sclNode, SIG_OPEN);
	} else if (value == SIG_LOW) {
		int addr = (i2c->sar >> 1) & 0x7f;
		I2C_SerDesAddSlave(i2c->serdes, &i2c->i2c_slave, addr);
		Mem_AreaAddMapping(&i2c->bdev, NS9xxx_I2C_BASE, 0x10,
				   MEM_FLAG_WRITABLE | MEM_FLAG_READABLE);
	}
	return;
}

/* 
 * ------------------------------------------------------
 * Map/Unmap the I2C controller to memory space
 * ------------------------------------------------------
 */

#define BUS_ADDR(base,reg) ((base)+(reg))
static void
NSI2C_UnMap(void *owner, uint32_t base, uint32_t mapsize)
{
	IOH_Delete32(BUS_ADDR(base, NS_I2C_CTDR));
	IOH_Delete32(BUS_ADDR(base, NS_I2C_MAR));
	IOH_Delete32(BUS_ADDR(base, NS_I2C_SAR));
	IOH_Delete32(BUS_ADDR(base, NS_I2C_CFG));
}

static void
NSI2C_Map(void *owner, uint32_t base, uint32_t mapsize, uint32_t flags)
{
	NS_I2C *i2c = (NS_I2C *) owner;
	IOH_New32(BUS_ADDR(base, NS_I2C_CTDR), strdr_read, ctdr_write, i2c);
	IOH_New32(BUS_ADDR(base, NS_I2C_MAR), mar_read, mar_write, i2c);
	IOH_New32(BUS_ADDR(base, NS_I2C_SAR), sar_read, sar_write, i2c);
	IOH_New32(BUS_ADDR(base, NS_I2C_CFG), cfg_read, cfg_write, i2c);
}

/*
 * -----------------------------------------------------------
 * nsslave_write
 * 	Accept a byte as slave if ready. If not ready 
 *	advise the slave module to stretch SCL and call
 *	again when stretching is finished 
 * -----------------------------------------------------------
 */
static int
nsslave_write(void *dev, uint8_t data)
{
	NS_I2C *i2c = (NS_I2C *) dev;
	if (i2c->sstate == SSTATE_RX_FIRST) {
		i2c->strdr = (i2c->strdr & ~0xff) | data | STRDR_RDE;
		dbgprintf("Slave RX_DATA_1ST_IRQ\n");
		queue_irqcd(i2c, S_RX_DATA_1ST_IRQ);
		i2c->sstate = SSTATE_RX;
		return I2C_ACK;
	} else if (i2c->sstate == SSTATE_RX) {
		if (i2c->strdr & STRDR_RDE) {
			i2c->scl_stretched = 1;
			return I2C_STRETCH_SCL;
		} else {
			i2c->strdr = (i2c->strdr & ~0xff) | data | STRDR_RDE;
			queue_irqcd(i2c, S_RX_DATA_IRQ);
			fprintf(stderr, "Slave RX_DATA_IRQ\n");
			return I2C_ACK;
		}
	} else {
		fprintf(stderr, "Illegal slave state in nsslave_write\n");
		return I2C_NACK;
	}
}

/* 
 * -----------------------------------------------------------------------
 * nsslave_read
 *	return the next data byte which is sent over I2C.
 *	if not available SCL stretching stops the slave state machine 
 *	until CTDR_TXVAL is set. If not first byte TX_DATA_IRQ was already 
 *      sent by	previous read_acknowledge.
 * -----------------------------------------------------------------------
 */
static int
nsslave_read(void *dev, uint8_t * data)
{
	NS_I2C *i2c = (NS_I2C *) dev;
	uint32_t ctdr = i2c->ctdr;
	if (!(ctdr & CTDR_TXVAL)) {
		i2c->scl_stretched = 1;
		return I2C_STRETCH_SCL;
	} else {
		*data = ctdr & 0xff;
		dbgprintf("slave read commit value 0x%02x\n", *data);
		i2c->ctdr &= ~CTDR_TXVAL;
		return I2C_DONE;
	}
}

/*
 * --------------------------------------
 * nsslave_start
 * 	Start a slave transaction
 * --------------------------------------
 */
static int
nsslave_start(void *dev, int i2c_addr, int operation)
{
	NS_I2C *i2c = (NS_I2C *) dev;
	if (operation == I2C_WRITE) {
		/* do nothing until data available */
		//fprintf(stderr,"Slave start WRITE operation\n");
		i2c->sstate = SSTATE_RX_FIRST;
		return I2C_ACK;
	} else if (operation == I2C_READ) {
		i2c->sstate = SSTATE_TX;
		dbgprintf("Slave start READ operation: TX_DATA_IRQ\n");
		queue_irqcd(i2c, S_TX_DATA_1ST_IRQ);
		return I2C_ACK;
	} else {
		fprintf(stderr, "illegal operation %d for i2c slave\n", operation);
		return I2C_NACK;
	}
}

static void
nsslave_stop(void *dev)
{
	NS_I2C *i2c = (NS_I2C *) dev;
	i2c->sstate = SSTATE_IDLE;
	i2c->scl_stretched = 0;
	return;
}

/*
 * --------------------------------------------------------
 * read_ack
 *	When the master acknowleges the byte sent by
 *	the slave the next byte is requested by posting
 *	a TX_DATA_IRQ. If the master doesn't ack a
 *	NO_ACK_IRQ is posted.
 * --------------------------------------------------------
 */
static void
nsslave_read_ack(void *dev, int ack)
{
	NS_I2C *i2c = (NS_I2C *) dev;
	if (ack == I2C_ACK) {
		dbgprintf("Slave TX_DATA_IRQ\n");
		queue_irqcd(i2c, S_TX_DATA_IRQ);
	} else if (ack == I2C_NACK) {
		dbgprintf("Slave NO_ACK_IRQ\n");
		queue_irqcd(i2c, S_NO_ACK_IRQ);
	}
}

static I2C_SlaveOps nsslave_ops = {
	.start = nsslave_start,
	.stop = nsslave_stop,
	.read = nsslave_read,
	.write = nsslave_write,
	.read_ack = nsslave_read_ack
};

/*
 * -----------------------------------------------
 * Create a new I2C controller
 * -----------------------------------------------
 */
BusDevice *
NS9xxx_I2CNew(const char *name)
{
	char *nodename1 = (char *)alloca(strlen(name) + 50);
	char *nodename2 = (char *)alloca(strlen(name) + 50);
	I2C_Slave *i2c_slave;
	NS_I2C *i2c = sg_new(NS_I2C);
	i2c->ctdr = 0;
	i2c->strdr = 0;
	i2c->mar = 0;
	i2c->sar = 0x7fe;
	i2c->mstate = MSTATE_IDLE;
	i2c->cfg = (0xf << CFG_SFW_SHIFT) | CFG_VSCD | CFG_TMDE;

	/* Create the slave */
	i2c_slave = &i2c->i2c_slave;
	i2c_slave->devops = &nsslave_ops;
	i2c_slave->dev = (void *)i2c;
	i2c_slave->speed = I2C_SPEED_FAST;

	sprintf(nodename1, "%s.slave", name);
	i2c->serdes = I2C_SerDesNew(nodename1);

	/* Create the SDA/SCL nodes and connect with slave */
	sprintf(nodename1, "%s.scl", name);
	sprintf(nodename2, "%s.slave.scl", name);
	i2c->sclNode = SigNode_New("%s", nodename1);
	SigName_Link(nodename2, nodename1);

	sprintf(nodename1, "%s.sda", name);
	sprintf(nodename2, "%s.slave.sda", name);
	i2c->sdaNode = SigNode_New("%s", nodename1);
	SigName_Link(nodename2, nodename1);

	if (!i2c->sclNode || !i2c->sdaNode) {
		fprintf(stderr, "Can not create I2C signal nodes\n");
		exit(342);
	}
	i2c->resetNode = SigNode_New("%s.reset", name);
	i2c->irqNode = SigNode_New("%s.irq", name);

	if (!i2c->irqNode || !i2c->resetNode) {
		fprintf(stderr, "Can not create I2C irq or reset node\n");
		exit(343);
	}
	i2c->sclTrace = SigNode_Trace(i2c->sclNode, scl_trace_proc, i2c);
	i2c->sdaTrace = SigNode_Trace(i2c->sdaNode, sda_trace_proc, i2c);
	i2c->resetTrace = SigNode_Trace(i2c->resetNode, reset_trace_proc, i2c);
	i2c->bdev.first_mapping = NULL;
	i2c->bdev.Map = NSI2C_Map;
	i2c->bdev.UnMap = NSI2C_UnMap;
	i2c->bdev.owner = i2c;
	i2c->bdev.hw_flags = MEM_FLAG_WRITABLE | MEM_FLAG_READABLE;
	fprintf(stderr, "Netsilicon I2C Controller created\n");
	return &i2c->bdev;
}
