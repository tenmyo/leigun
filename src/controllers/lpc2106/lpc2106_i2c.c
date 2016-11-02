/*
 *************************************************************************************************
 *
 * LPC2106 I2C 
 *      Emulation of the I2C Controller of the Phillips LPC2106
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

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <bus.h>
#include "i2c.h"
#include "i2c_serdes.h"
#include "signode.h"
#include "clock.h"
#include "cycletimer.h"
#include "sgstring.h"

#if 0
#define dbgprintf(x...) { fprintf(stderr,x); }
#else
#define dbgprintf(x...)
#endif


#define REG_I2CONSET (0x00)
#define		I2CON_AA	(1<<2)
#define		I2CON_SI	(1<<3)
#define		I2CON_STO	(1<<4)
#define		I2CON_STA	(1<<5)
#define		I2CON_I2EN	(1<<6)
#define REG_I2CSTAT  (0x04)
#define REG_I2CDAT   (0x08)
#define REG_I2CADR   (0x0c)
#define	REG_SCLH     (0x10)
#define REG_SCLL     (0x14)
#define REG_I2CONCLR (0x18)


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
#define INSTR_SET_STATUS        (0x0f000000)

#define ADD_CODE(i2c,x) ((i2c)->code[(i2c->icount++) % CODE_MEM_SIZE] = (x))

#define RET_DONE                (0)
#define RET_DO_NEXT             (1)
#define RET_INTERP_ERROR        (-3)

#define ACK (1)
#define NACK (0)

/* The timings */
#define T_HDSTA(i2c) ((i2c)->timing.t_hdsta)
#define T_LOW(i2c)  ((i2c)->timing.t_low)
#define T_HIGH(i2c) ((i2c)->timing.t_high)
#define T_SUSTA(i2c) ((i2c)->timing.t_susta)
#define T_HDDAT_MAX(i2c) ((i2c)->timing.t_hddat_max)
#define T_HDDAT(i2c) (T_HDDAT_MAX(i2c)>>1)
#define T_SUDAT(i2c) ((i2c)->timing.t_sudat)
#define T_SUSTO(i2c) ((i2c)->timing.t_susto)
#define T_BUF(i2c) ((i2c)->timing.t_buf)


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

typedef struct LPC_I2C {
	BusDevice bdev;
	I2C_Timing timing;
	I2C_Slave i2c_slave;
	I2C_SerDes *serdes;
	Clock_t *clk;
	ClockTrace_t *clkTrace;
	uint8_t i2con;
	uint8_t i2cstat;
	uint8_t i2cdat;
	uint8_t i2cadr;
	uint16_t sclh;
	uint16_t scll;
	SigNode *irqNode;
        SigNode *sdaNode;
        SigNode *sclNode;
        SigTrace *sdaTrace;

	/* The interpreter */
        SigTrace *sclStretchTrace;
        CycleTimer ndelayTimer;
        uint8_t rxdata; /* The input register */
        int ack;
        uint16_t ip;
        uint16_t icount;
        uint32_t code[CODE_MEM_SIZE];
        int wait_bus_free;

} LPC_I2C;

static void
update_timing(LPC_I2C *i2c) {
	I2C_Timing *timing = &i2c->timing;
	uint32_t freq;
	uint32_t low_period;
	uint32_t high_period;
	freq = Clock_Freq(i2c->clk);
	if(freq == 0) {
		freq = 1;
	}
	low_period = i2c->scll *  (1000000000 / freq);
	high_period = i2c->sclh * (1000000000 / freq);
	timing->t_hdsta = low_period * 600 / 2500;
	timing->t_low = low_period * 1300 / 2500;
	timing->t_high = high_period * 600 / 2500;
	timing->t_susta = high_period * 600 / 2500;
	timing->t_hddat_max = low_period * 900 / 2500;
	timing->t_sudat = low_period * 100 / 2500;
	timing->t_susto = high_period * 600 / 2500;
	timing->t_buf = high_period * 1300 / 2500;
}

static void
update_interrupt(LPC_I2C *i2c) 
{
	if(i2c->i2con & I2CON_SI) {
		SigNode_Set(i2c->irqNode,SIG_LOW);
	} else {
		SigNode_Set(i2c->irqNode,SIG_OPEN);
	}
}

static void run_interpreter(void *clientData);

/*
 * -------------------------------------------------------------------
 * scl_released
 *      The SCL trace proc for clock stretching
 *      If it is called before timeout it will
 *      remove the timer and continue running the interpreter
 * -------------------------------------------------------------------
 */
static void 
scl_released(SigNode *node,int value,void *clientData)
{
        LPC_I2C *i2c = (LPC_I2C *)clientData;
        if((value == SIG_PULLUP) || (value == SIG_HIGH)) {
                SigNode_Untrace(i2c->sclNode,i2c->sclStretchTrace);
                CycleTimer_Remove(&i2c->ndelayTimer);
                run_interpreter(clientData);
        }
}

/*
 * -----------------------------------------------
 * When SCL line is blocked forever call this
 * -----------------------------------------------
 */
static void
scl_timeout(void *clientData)
{
        LPC_I2C *i2c = (LPC_I2C *)clientData;
        fprintf(stderr,"LPC_I2C: SCL line seems to be blocked\n");
        SigNode_Untrace(i2c->sclNode,i2c->sclStretchTrace);
}

static inline int
bus_is_free(LPC_I2C *i2c) {
//        return !(i2c->i2sr & I2SR_IBB);
	return 0;
}

/*
 * Assemble the code for a start condition
 */
void
assemble_start(LPC_I2C *i2c,int repstart) {
        /* Repstart */
        if(repstart) {
                ADD_CODE(i2c,INSTR_SDA_H);
                ADD_CODE(i2c,INSTR_NDELAY | (T_LOW(i2c)-T_HDDAT(i2c)));

                ADD_CODE(i2c,INSTR_SCL_H);
                ADD_CODE(i2c,INSTR_SYNC);
                ADD_CODE(i2c,INSTR_NDELAY | T_HIGH(i2c));
        } else {
                ADD_CODE(i2c,INSTR_WAIT_BUS_FREE);
                /* instead of t_buf */
                ADD_CODE(i2c,INSTR_NDELAY | 50);
        }

        /* Generate a start condition */
        ADD_CODE(i2c,INSTR_CHECK_ARB);
        ADD_CODE(i2c,INSTR_SDA_L);
        ADD_CODE(i2c,INSTR_NDELAY | T_HDSTA(i2c));
        ADD_CODE(i2c,INSTR_SCL_L);
        ADD_CODE(i2c,INSTR_NDELAY | T_HDDAT(i2c));
}

/*
 * ----------------------------------------------------------
 * assemble_stop
 *      Assemble a micro operation script which generates a
 *      stop condition on I2C bus.
 *      Enter with SCL low for at least T_HDDAT
 * ----------------------------------------------------------
 */
static void
assemble_stop(LPC_I2C *i2c)
{
        dbgprintf("Append stop\n");
        ADD_CODE(i2c,INSTR_SDA_L);
        ADD_CODE(i2c,INSTR_NDELAY | (T_LOW(i2c)-T_HDDAT(i2c)));
        ADD_CODE(i2c,INSTR_SCL_H);
        ADD_CODE(i2c,INSTR_SYNC);
        ADD_CODE(i2c,INSTR_NDELAY | T_SUSTO(i2c));
        ADD_CODE(i2c,INSTR_SDA_H);
        ADD_CODE(i2c,INSTR_NDELAY | T_BUF(i2c));
}

/*
 * -----------------------------------------------------------
 * assemble_check_ack
 *      Assemble a micro operation script which checks
 *      for acknowledge.
 *      has to be entered with SCL low for at least T_HDDAT
 * -----------------------------------------------------------
 */
static void
assemble_check_ack(LPC_I2C *i2c) {
        /* check ack of previous */
        ADD_CODE(i2c,INSTR_SDA_H);
        ADD_CODE(i2c,INSTR_NDELAY | (T_LOW(i2c)-T_HDDAT(i2c)));
        ADD_CODE(i2c,INSTR_SCL_H);
        ADD_CODE(i2c,INSTR_SYNC);
        ADD_CODE(i2c,INSTR_NDELAY | T_HIGH(i2c));
        ADD_CODE(i2c,INSTR_READ_ACK);
        ADD_CODE(i2c,INSTR_SCL_L);
        ADD_CODE(i2c,INSTR_NDELAY | T_HDDAT(i2c));
        ADD_CODE(i2c,INSTR_CHECK_ACK);
}

/*
 * --------------------------------------------------------------
 * assemble_write_byte
 *      Assemble a micro operation script which writes a
 *      byte to the I2C-Bus
 *      Has to be entered with SCL low for at least T_HDDAT
 * --------------------------------------------------------------
 */
static void
assemble_write_byte(LPC_I2C * i2c,uint8_t data)
{
        int i;
        for(i=7;i>=0;i--) {
                int bit = (data>>i) & 1;
                if(bit) {
                        ADD_CODE(i2c,INSTR_SDA_H);
                } else {
                        ADD_CODE(i2c,INSTR_SDA_L);
                }
                ADD_CODE(i2c,INSTR_NDELAY | (T_LOW(i2c)-T_HDDAT(i2c)));
                ADD_CODE(i2c,INSTR_SCL_H);
                ADD_CODE(i2c,INSTR_SYNC);
                ADD_CODE(i2c,INSTR_NDELAY | T_HIGH(i2c));
                if(bit) {
                        ADD_CODE(i2c,INSTR_CHECK_ARB);
                }
                ADD_CODE(i2c,INSTR_SCL_L);
                ADD_CODE(i2c,INSTR_NDELAY | T_HDDAT(i2c));
        }
        assemble_check_ack(i2c);
        ADD_CODE(i2c,INSTR_INTERRUPT);
        ADD_CODE(i2c,INSTR_NDELAY | (T_LOW(i2c)-T_HDDAT(i2c)));
}

/*
 * ------------------------------------------------------------------
 * reset_interpreter
 *      reset the I2C-master micro instruction interpreter.
 *      called before assembling a script or on abort of script.
 * ------------------------------------------------------------------
 */
static void
reset_interpreter(LPC_I2C *i2c) {
        i2c->wait_bus_free = 0;
        i2c->code[0] = INSTR_ENDSCRIPT;
        i2c->ip = 0;
        i2c->icount=0;
        i2c->rxdata=0;
        i2c->ack=0;
        CycleTimer_Remove(&i2c->ndelayTimer);
}


/*
 * --------------------------------------------------------------------
 * The Interpreter
 * --------------------------------------------------------------------
 */
static int
execute_instruction(LPC_I2C *i2c) {
        uint32_t icode;
        if(i2c->ip >= CODE_MEM_SIZE) {
                fprintf(stderr,"LPC I2C: corrupt I2C script\n");
                return RET_INTERP_ERROR;
        }
        if(i2c->ip == i2c->icount) {
                return RET_DONE;
        }
        icode = i2c->code[i2c->ip++];
        switch(icode&0xff000000) {
                case INSTR_SDA_H:
                        dbgprintf("SDA_H %08x\n",icode);
                        SigNode_Set(i2c->sdaNode,SIG_OPEN);
                        break;
                case INSTR_SDA_L:
                        dbgprintf("SDA_L %08x\n",icode);
                        SigNode_Set(i2c->sdaNode,SIG_LOW);
                        break;
                case INSTR_SCL_H:
                        dbgprintf("SCL_H %08x\n",icode);
                        SigNode_Set(i2c->sclNode,SIG_OPEN);
                        break;
                case INSTR_SCL_L:
                        dbgprintf("SCL_L %08x\n",icode);
                        SigNode_Set(i2c->sclNode,SIG_LOW);
                        break;

                case INSTR_CHECK_ARB:
                        dbgprintf("CHECK_ARB %08x\n",icode);
                        if(SigNode_Val(i2c->sdaNode)==SIG_LOW) {
                                i2c->i2con |= I2CON_SI;
				/* ARB_LOST state */
				i2c->i2cstat = 0x38; 
				update_interrupt(i2c);
                                return RET_DONE;
                        }
                        break;

                case INSTR_NDELAY:
                        {
                                uint32_t nsecs = icode & 0xffffff;
                                int64_t cycles = NanosecondsToCycles(nsecs);
                                CycleTimer_Add(&i2c->ndelayTimer,cycles,run_interpreter,i2c);
                                dbgprintf("NDELAY %08x\n",icode);
                        }
                        return RET_DONE;

		
                case INSTR_SYNC:
                        dbgprintf("SYNC %08x\n",icode);
                        if(SigNode_Val(i2c->sclNode)==SIG_LOW) {
                                uint32_t msecs = 200;
                                int64_t cycles =  MillisecondsToCycles(msecs);
                                i2c->sclStretchTrace = SigNode_Trace(i2c->sclNode,scl_released,i2c);
                                CycleTimer_Add(&i2c->ndelayTimer,cycles,scl_timeout,i2c);
                                return RET_DONE;
                        }
                        break;

                case INSTR_READ_SDA:
                        if(SigNode_Val(i2c->sdaNode) == SIG_LOW) {
                                i2c->rxdata = (i2c->rxdata<<1);
                        } else {
                                i2c->rxdata = (i2c->rxdata<<1) | 1;
                        }
                        dbgprintf("READ_SDA %02x %08x\n",i2c->rxdata,icode);
                        break;

                case INSTR_READ_ACK:
                        dbgprintf("READ_ACK %08x\n",icode);
                        if(SigNode_Val(i2c->sdaNode) == SIG_LOW) {
                                i2c->ack = ACK;
				/* Start/Repstart */
				if((i2c->i2cstat == 0x8) || (i2c->i2cstat == 0x10)) {
					i2c->i2cstat = 0x18; /* SLA acked */	
				} else if ((i2c->i2cstat == 0x18) || (i2c->i2cstat == 0x28)) {
					i2c->i2cstat = 0x28; /* Data acked */	
				}
                        } else {
                                i2c->ack = NACK;
				if((i2c->i2cstat == 0x8) || (i2c->i2cstat == 0x10)) {
					i2c->i2cstat = 0x20; /* SLA nacked */	
				} else if ((i2c->i2cstat == 0x18) || (i2c->i2cstat == 0x28)) {
					i2c->i2cstat = 0x30;
				}
                        }
                        break;

                case  INSTR_ENDSCRIPT:
                        dbgprintf("ENDSCRIPT %08x\n",icode);
                        return RET_DONE;
                        break;

                case  INSTR_CHECK_ACK:
                        dbgprintf("CHECK_ACK %08x\n",icode);
                        if(i2c->ack == NACK) {
                               // i2c->i2sr |= I2SR_RXAK;
                     //           return RET_DONE;
                        } else {
                                //i2c->i2sr &= ~I2SR_RXAK;
                        }
                        break;

                case  INSTR_SET_STATUS:
                        {
				i2c->i2cstat = icode & 0xff;
				if((i2c->i2cstat == 0x08) || (i2c->i2cstat = 0x10)) {
					i2c->i2con &= ~I2CON_STA;
				} else if((i2c->i2cstat == 0xf8)) {
					i2c->i2con &= ~I2CON_STO;
				}
                               	break; 
                        }

                case  INSTR_INTERRUPT:
                        dbgprintf("INTERRUPT %08x\n",icode);
                        {
				i2c->i2con |= I2CON_SI;
                                update_interrupt(i2c);
                                return RET_DONE;
                        }
                        break;

                case  INSTR_RXDATA_AVAIL:
                        dbgprintf("RXDATA_AVAIL %02x %08x\n",i2c->rxdata,icode);
                        /* Set some data ready flag in a register */
                        i2c->i2cdat = i2c->rxdata; // or does it shift directly in i2cdat ?
                                                      
		case INSTR_WAIT_BUS_FREE:
                        if(!bus_is_free(i2c)) {
                                dbgprintf("Bus not free, waiting !\n");
                                i2c->wait_bus_free = 1;
                                return RET_DONE;
                        }
                        break;

                default:
                        fprintf(stderr,"LPC I2C: Unknode instruction code %08x\n",icode);
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
        LPC_I2C *i2c = (LPC_I2C *) clientData;
        int retval;
        do {
                retval = execute_instruction(i2c);
        } while(retval == RET_DO_NEXT);
}

/*
 * -----------------------------------------------------------------------------
 * start script
 *      Start execution of micro operation I2C scripts
 * -----------------------------------------------------------------------------
 */
static void
start_script(LPC_I2C *i2c) {
        if(CycleTimer_IsActive(&i2c->ndelayTimer)) {
                fprintf(stderr,"I2C Emulator bug: Starting script which already runs\n");
                return;
        }
        CycleTimer_Add(&i2c->ndelayTimer,0,run_interpreter,i2c);
}


static void 
ClockTrace(struct Clock *clock,void *clientData)
{
	LPC_I2C *i2c = (LPC_I2C *) clientData;
	update_timing(i2c);
}


static uint32_t
i2conset_read(void *clientData,uint32_t address,int rqlen)
{
	LPC_I2C *i2c = (LPC_I2C*) clientData;
        return i2c->i2con;
}

static void
i2conset_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	LPC_I2C *i2c = (LPC_I2C*) clientData;
	uint8_t diff;
	int script_empty = 1;
	diff = i2c->i2con ^ (i2c->i2con | value); 
	i2c->i2con |= value;
	if(diff & I2CON_I2EN) {
		I2C_SerDesAddSlave(i2c->serdes,&i2c->i2c_slave,i2c->i2cadr);
	}
	if(i2c->i2con & I2CON_STO) {
		uint8_t st = i2c->i2cstat;
		/* This resets the slave (simulates a stop condition) if in slave mode */
		/* Else it generates a stop condition and then a start condition */
		if((st == 0x08) || (st == 0x10) || (st == 0x18) || (st == 0x20) || (st == 0x28) 
			|| (st == 0x30) /* || (st == 0x38) */) {
			reset_interpreter(i2c);
			script_empty = 0;
			assemble_stop(i2c);
			ADD_CODE(i2c,INSTR_SET_STATUS | 0xf8);
			start_script(i2c);
		}
	
	} 
        if(i2c->i2con & I2CON_STA) {
		uint8_t st = i2c->i2cstat;
		if(script_empty == 1) {
			script_empty = 0;
			reset_interpreter(i2c);
		}
		if((st == 0x08) || (st == 0x10) || (st == 0x18) || (st == 0x20) || (st == 0x28) 
		    || (st == 0x30) /* || (st == 0x38) */) {
			assemble_start(i2c,1);
			ADD_CODE(i2c,INSTR_SET_STATUS | 0xf8);
		} else {
			assemble_start(i2c,0);
			ADD_CODE(i2c,INSTR_SET_STATUS | 0x08);
			ADD_CODE(i2c,INSTR_INTERRUPT);
		}
	}
	if(!script_empty) {
		start_script(i2c);
	}
	update_interrupt(i2c);
}

static uint32_t
i2cstat_read(void *clientData,uint32_t address,int rqlen)
{
	LPC_I2C *i2c = (LPC_I2C*) clientData;
        return i2c->i2cstat;
}

static void
i2cstat_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	fprintf(stderr,"Writing to readonly register I2C-Stat\n");
}

static uint32_t
i2cdat_read(void *clientData,uint32_t address,int rqlen)
{
	LPC_I2C *i2c = (LPC_I2C*) clientData;
        return i2c->i2cdat; 
}

static void
i2cdat_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	LPC_I2C *i2c = (LPC_I2C*) clientData;
	i2c->i2cdat = value;
}

static uint32_t
i2cadr_read(void *clientData,uint32_t address,int rqlen)
{
	LPC_I2C *i2c = (LPC_I2C*) clientData;
        return i2c->i2cadr;
}

static void
i2cadr_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	LPC_I2C *i2c = (LPC_I2C*) clientData;
	if((i2c->i2con & I2CON_I2EN) && (i2c->i2cadr != value)) {
		I2C_SerDesDetachSlave(i2c->serdes,&i2c->i2c_slave);
		I2C_SerDesAddSlave(i2c->serdes,&i2c->i2c_slave,i2c->i2cadr);
	}
	i2c->i2cadr = value;
}

static uint32_t
sclh_read(void *clientData,uint32_t address,int rqlen)
{
	LPC_I2C *i2c = (LPC_I2C*) clientData;
        return i2c->sclh;
}

static void
sclh_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	LPC_I2C *i2c = (LPC_I2C*) clientData;
	i2c->sclh = value;
	update_timing(i2c);
}

static uint32_t
scll_read(void *clientData,uint32_t address,int rqlen)
{
	LPC_I2C *i2c = (LPC_I2C*) clientData;
        return i2c->scll;
}

static void
scll_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	LPC_I2C *i2c = (LPC_I2C*) clientData;
	i2c->scll = value;
	update_timing(i2c);
}

static uint32_t
i2conclr_read(void *clientData,uint32_t address,int rqlen)
{
	LPC_I2C *i2c = (LPC_I2C*) clientData;
	fprintf(stderr,"I2CONCLR is not readable\n");
        return i2c->i2con; /* Not checked with real device */
}

static void
i2conclr_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	LPC_I2C *i2c = (LPC_I2C*) clientData;
	uint8_t diff;
	diff = i2c->i2con ^ (i2c->i2con & ~value); 
	i2c->i2con &= ~value;
	if(diff & I2CON_SI) {
		update_interrupt(i2c);
		if((i2c->i2cstat == 0x08) || (i2c->i2cstat == 0x10)) {
			/* Start condition finished, send Address as master */
			reset_interpreter(i2c);
			assemble_write_byte(i2c,i2c->i2cdat);
			start_script(i2c);
		} else if(i2c->i2cstat == 0x18) {
			reset_interpreter(i2c);
			assemble_write_byte(i2c,i2c->i2cdat);
			start_script(i2c);
		} else if(i2c->i2cstat == 0x28) {
			reset_interpreter(i2c);
			assemble_write_byte(i2c,i2c->i2cdat);
			start_script(i2c);
		}	
	}
}

static int
nxp_slave_start(void *dev,int i2c_addr,int operation)
{
        LPC_I2C * i2c = (LPC_I2C *) dev;
	if(!(i2c->i2con & I2CON_AA)) {
		if(i2c_addr == 0) {
			i2c->i2cstat = 0x78;
		} else {
			i2c->i2cstat = 0x68;
		}
		i2c->i2con |= I2CON_SI; // ?
		update_interrupt(i2c);
		return I2C_NACK;	
	}
        if(operation == I2C_WRITE) {
		if(i2c_addr == 0) {
			i2c->i2cstat = 0x70;
		} else {
			i2c->i2cstat = 0x60;
		}
		i2c->i2con |= I2CON_SI;
		update_interrupt(i2c);
		return I2C_ACK;
        } else if(operation == I2C_READ) {
		if(i2c_addr == 0) {
                	return I2C_NACK;
		} else {
			i2c->i2cstat = 0xa8;
		}
		i2c->i2con |= I2CON_SI;
		update_interrupt(i2c);
                return I2C_ACK;
	} else {
                fprintf(stderr,"illegal operation %d for i2c slave\n",operation);
                return I2C_NACK;
	}
}

static void
nxp_slave_stop(void *dev)
{
	LPC_I2C *i2c = (LPC_I2C *) dev;
	i2c->i2cstat = 0xa0; 
	i2c->i2con |= I2CON_SI;
	update_interrupt(i2c);
}

static void
nxp_slave_read_ack(void *dev, int ack)
{
	LPC_I2C *i2c = (LPC_I2C *) dev;
	i2c->i2con |= I2CON_SI;
	if(ack == I2C_ACK) {
		i2c->i2con |= I2CON_AA;
		i2c->i2cstat = 0xB8; 
        } else if (ack == I2C_NACK) {
		i2c->i2cstat = 0xC0; 
		i2c->i2con &= ~I2CON_AA;
	} 
	update_interrupt(i2c);
}

static int
nxp_slave_write(void *dev,uint8_t data)
{
	LPC_I2C *i2c = (LPC_I2C *) dev;
	i2c->i2cdat = data;
	if(i2c->i2con & I2CON_AA) {
		if((i2c->i2cstat == 0x70) || (i2c->i2cstat == 0x78)) 
		{
			i2c->i2cstat = 0x90;
		} else {
			i2c->i2cstat = 0x80;
		}
		i2c->i2con |= I2CON_SI;
		update_interrupt(i2c);
		return I2C_ACK;
	} else {
		if((i2c->i2cstat == 0x90) || (i2c->i2cstat == 0x70) 
		  || (i2c->i2cstat == 0x78)) 
		{
			i2c->i2cstat = 0x98;
		} else {
			i2c->i2cstat = 0x88;
		}
		i2c->i2con |= I2CON_SI;
		update_interrupt(i2c);
		return I2C_NACK;
	}
	
}

static int
nxp_slave_read(void *dev,uint8_t *data)
{
	LPC_I2C *i2c = (LPC_I2C *) dev;
	*data = i2c->i2cdat;
	// Any stretchng if not ready ?
	return I2C_DONE;
}


static I2C_SlaveOps nxp_slave_ops = {
        .start = nxp_slave_start,
        .stop =  nxp_slave_stop,
        .read =  nxp_slave_read,
        .write = nxp_slave_write,
        .read_ack =  nxp_slave_read_ack
};


static void
I2C_Map(void *owner,uint32_t base,uint32_t mask,uint32_t flags)
{
	LPC_I2C *i2c = (LPC_I2C*) owner;
	IOH_New32(REG_I2CONSET,i2conset_read,i2conset_write,i2c);
	IOH_New32(REG_I2CSTAT,i2cstat_read,i2cstat_write,i2c);
	IOH_New32(REG_I2CDAT,i2cdat_read,i2cdat_write,i2c);
	IOH_New32(REG_I2CADR,i2cadr_read,i2cadr_write,i2c);
	IOH_New32(REG_SCLH,sclh_read,sclh_write,i2c);
	IOH_New32(REG_SCLL,scll_read,scll_write,i2c);
	IOH_New32(REG_I2CONCLR,i2conclr_read,i2conclr_write,i2c);
}

static void
I2C_UnMap(void *owner,uint32_t base,uint32_t mask) 
{
	IOH_Delete32(REG_I2CONSET);
	IOH_Delete32(REG_I2CSTAT);
	IOH_Delete32(REG_I2CDAT);
	IOH_Delete32(REG_I2CADR);
	IOH_Delete32(REG_SCLH);
	IOH_Delete32(REG_SCLL);
	IOH_Delete32(REG_I2CONCLR);
}

BusDevice *
LPC_I2CNew(const char *name) 
{
	I2C_Slave *i2c_slave;
	char *serdesname = alloca(strlen(name)+30);
	LPC_I2C *i2c = sg_new(LPC_I2C);

	i2c_slave = &i2c->i2c_slave;
        i2c_slave->devops = &nxp_slave_ops;
        i2c_slave->dev = (void *)i2c;
        i2c_slave->speed = I2C_SPEED_FAST;

	i2c->irqNode = SigNode_New("%s.irq",name);
	if(!i2c->irqNode) {
		fprintf(stderr,"Can not create interrupt line for %s\n",name);
		exit(1);
	}
	i2c->sdaNode = SigNode_New("%s.sda",name);
        i2c->sclNode = SigNode_New("%s.scl",name);
	if(!i2c->sclNode || !i2c->sdaNode) {
		fprintf(stderr,"Can not create I2C signal nodes\n");
		exit(1);
	}
//	i2c->sdaTrace = SigNode_Trace(i2c->sdaNode,sda_trace_proc,i2c);

	sprintf(serdesname,"%s.slave",name);
        i2c->serdes = I2C_SerDesNew(serdesname);
	i2c->clk = Clock_New("%s.clk",name);
	/* Set some initial value before it is connected */
	Clock_SetFreq(i2c->clk,4000000);
	i2c->clkTrace = Clock_Trace(i2c->clk,ClockTrace,i2c);

	i2c->bdev.first_mapping=NULL;
       	i2c->bdev.Map=I2C_Map;
        i2c->bdev.UnMap=I2C_UnMap;
        i2c->bdev.owner=i2c;
        i2c->bdev.hw_flags=MEM_FLAG_WRITABLE|MEM_FLAG_READABLE;
	//i2c->i2con;
	i2c->i2cstat = 0xf8; /* No valid state */
	//i2c->i2cdat;
	//i2c->i2cadr;
	//i2c->sclh;
	//i2c->scll;

	update_interrupt(i2c);
	update_timing(i2c);

	return &i2c->bdev;
}
