/*
 ****************************************************************************************************
 *
 * Emulation of the AT91RM9200 Two wire interface (TWI) 
 *
 *  State: implementation working with u-boot and linux, interrupts untested 
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
#include "sgstring.h"
#include "i2c.h"
#include "i2c_serdes.h"
#include "bus.h"
#include "signode.h"
#include "cycletimer.h"
#include "clock.h"
#include "at91_twi.h"
#include "senseless.h"


#if 0
#define dbgprintf(x...) { fprintf(stderr,x); }
#else
#define dbgprintf(x...)
#endif


#define TWI_CR(base)	((base) + 0x00)
#define		CR_SWRST	(1<<7)
#define		CR_SVDIS	(1<<5) /* Only Versions with Slave */
#define		CR_SVEN		(1<<4) /* Only Versions with Slave */
#define		CR_MSDIS	(1<<3)
#define 	CR_MSEN		(1<<2)
#define		CR_STOP		(1<<1)
#define		CR_START	(1<<0)
#define	TWI_MMR(base)	((base) + 0x04)
#define		MMR_DADR_MASK 		(0x7f<<16)
#define		MMR_DADR_SHIFT		(16)
#define		MMR_MREAD		(1<<12)
#define 	MMR_IADRSZ_MASK		(3<<8)
#define 	MMR_IADRSZ_SHIFT	(8)
#define		IADRSZ_0		(0<<8)
#define		IADRSZ_1		(1<<8)
#define		IADRSZ_2		(2<<8)
#define		IADRSZ_3		(3<<8)
#define TWI_SMR(base)	((base) + 0x08)
#define		SMR_SADR_SHIFT		(16)
#define		SMR_SADR_MASK		(0x7f << 16)
#define TWI_IADR(base)	((base) + 0x0c)
#define	TWI_CWGR(base)	((base) + 0x10)
#define 	CWGR_CKDIV_MASK		(7<<16)
#define		CWGR_CKDIV_SHIFT	(16)
#define		CWGR_CHDIV_MASK		(0xff<<8)
#define		CWGR_CHDIV_SHIFT	(8)
#define		CWGR_CLDIV_MASK		(0xff)
#define		CWGR_CLDIV_SHIFT	(0)
#define TWI_SR(base)	((base) + 0x20)
#define		SR_EOSACC	(1<<11) /* Only Version with slave */
#define		SR_SCLWS	(1<<10)	/* Only Version with slave */
#define		SR_ARBLST	(1<<9)	/* Only in some masters (SAM7SE) */
#define		SR_NACK		(1<<8)
#define		SR_UNRE		(1<<7)
#define		SR_OVRE		(1<<6)
#define		SR_GACC		(1<<5)	/* Only Version with slave */
#define		SR_SVACC	(1<<4)	/* Only Version with slave */
#define		SR_SVREAD	(1<<3)	/* Only Version with slave */
#define		SR_TXRDY	(1<<2)
#define		SR_RXRDY	(1<<1)
#define		SR_TXCOMP	(1<<0)
#define TWI_IER(base)	((base) + 0x24)
#define		IER_EOSACC	(1<<11)	/* Only Version with slave */
#define 	IER_SCLWS	(1<<10)	/* Only Version with slave */
#define		IER_ARBLST	(1<<9)	/* Only SAM7SE	*/
#define		IER_NACK	(1<<8)
#define		IER_UNRE	(1<<7)
#define		IER_OVRE	(1<<6)
#define		IER_GACC	(1<<5)	/* Only Version with slave */
#define		IER_SVACC	(1<<4)	/* Only Version with slave */
#define		IER_TXRDY	(1<<2)
#define		IER_RXRDY	(1<<1)
#define		IER_TXCOMP	(1<<0)

#define TWI_IDR(base)	((base) + 0x28)
#define		IDR_EOSACC	(1<<11)	/* Only Version with slave */
#define 	IDR_SCLWS	(1<<10)	/* Only Version with slave */
#define		IDR_ARBLST	(1<<9)	/* Only SAM7SE	*/
#define		IDR_NACK	(1<<8)
#define		IDR_UNRE	(1<<7)
#define		IDR_OVRE	(1<<6)
#define		IDR_GACC	(1<<5)	/* Only Version with slave */
#define		IDR_SVACC	(1<<4)	/* Only Version with slave */
#define		IDR_TXRDY	(1<<2)
#define		IDR_RXRDY	(1<<1)
#define		IDR_TXCOMP	(1<<0)

#define TWI_IMR(base)	((base) + 0x2c)
#define		IMR_EOSACC	(1<<11)	/* Only Version with slave */
#define 	IMR_SCLWS	(1<<10)	/* Only Version with slave */
#define		IMR_ARBLST	(1<<9)	/* Only SAM7SE	*/
#define		IMR_NACK	(1<<8)
#define		IMR_UNRE	(1<<7)
#define		IMR_OVRE	(1<<6)
#define		IMR_GACC	(1<<5)	/* Only Version with slave */
#define		IMR_SVACC	(1<<4)	/* Only Version with slave */
#define		IMR_TXRDY	(1<<2)
#define		IMR_RXRDY	(1<<1)
#define		IMR_TXCOMP	(1<<0)
#define TWI_RHR(base)	((base) + 0x30)
#define TWI_THR(base) 	((base) + 0x34)

#define CODE_MEM_SIZE (512) 

#define INSTR_SDA_L             (0x01000000)
#define INSTR_SDA_H             (0x02000000)
#define INSTR_SCL_L             (0x03000000)
#define INSTR_SCL_H             (0x04000000)
#define INSTR_CHECK_ARB         (0x05000000)
#define INSTR_NDELAY            (0x06000000)
#define INSTR_SYNC              (0x07000000)
#define INSTR_END		(0x08000000)
#define INSTR_INTERRUPT         (0x09000000)
#define INSTR_CHECK_ACK         (0x0a000000)
#define INSTR_READ_SDA          (0x0b000000)
#define INSTR_READ_ACK          (0x0c000000)
#define INSTR_RXDATA_AVAIL      (0x0d000000)
#define INSTR_WAIT_BUS_FREE     (0x0e000000)

#define RET_DONE		(0)
#define RET_DO_NEXT		(1)
#define RET_EMU_ERROR		(-3)

#define ACK	(1)
#define NACK	(0)

#define ADD_CODE(twi,x) ((twi)->code[((twi)->icount++) % CODE_MEM_SIZE] = (x))
#define FEAT_SLAVE(twi) ((twi)->features & TWI_FEATURE_SLAVE)

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


typedef struct AT91Twi  {
	BusDevice bdev;
	uint32_t features;
	const char *name;
	SigNode *irqNode;
	SigNode *sda;
	SigNode *scl;
	Clock_t *clk;
	ClockTrace_t *clkTrace;
	I2C_Timing i2c_timing;

	uint32_t cr;
	uint32_t mmr;
	uint32_t smr;
	uint32_t iadr;
	uint32_t cwgr;
	uint32_t sr;
	uint32_t imr;
	uint32_t rhr;
	uint32_t thr;

	/* The Processor */
        SigTrace *sclStretchTrace;
        CycleTimer ndelayTimer;

	uint8_t rxdata;
	int ack;
        uint16_t ip;
        uint32_t icount;
        uint32_t code[CODE_MEM_SIZE];

	
        /* Slave functionality */
        I2C_SerDes *serdes;
        I2C_Slave i2c_slave;
//	int sstate;
} AT91Twi;

#define T_HDSTA(twi) ((twi)->i2c_timing.t_hdsta)
#define T_LOW(twi)  ((twi)->i2c_timing.t_low)
#define T_HIGH(twi) ((twi)->i2c_timing.t_high)
#define T_SUSTA(twi) ((twi)->i2c_timing.t_susta)
#define T_HDDAT_MAX(twi) ((twi)->i2c_timing.t_hddat_max)
#define T_HDDAT(twi) (T_HDDAT_MAX(twi)>>1)
#define T_SUDAT(twi) ((twi)->i2c_timing.t_sudat)
#define T_SUSTO(twi) ((twi)->i2c_timing.t_susto)
#define T_BUF(twi) ((twi)->i2c_timing.t_buf)

static void 
update_interrupt(AT91Twi *twi) 
{
	if(twi->sr & twi->imr) {
		SigNode_Set(twi->irqNode,SIG_HIGH);
	} else {
		SigNode_Set(twi->irqNode,SIG_PULLDOWN);
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
static void
mscript_check_ack(AT91Twi *twi) {
        /* check ack of previous */
        ADD_CODE(twi,INSTR_SDA_H);
        ADD_CODE(twi,INSTR_NDELAY | (T_LOW(twi)-T_HDDAT(twi)));
        ADD_CODE(twi,INSTR_SCL_H);
        ADD_CODE(twi,INSTR_SYNC);
        ADD_CODE(twi,INSTR_NDELAY | T_HIGH(twi));
        ADD_CODE(twi,INSTR_READ_ACK);
        ADD_CODE(twi,INSTR_SCL_L);
        ADD_CODE(twi,INSTR_NDELAY | T_HDDAT(twi));
        ADD_CODE(twi,INSTR_CHECK_ACK);
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
mscript_write_byte(AT91Twi * twi,uint8_t data)
{
        int i;
        for(i=7;i>=0;i--) {
                int bit = (data>>i) & 1;
                if(bit) {
                        ADD_CODE(twi,INSTR_SDA_H);
		} else {
			ADD_CODE(twi,INSTR_SDA_L);
		}
		ADD_CODE(twi,INSTR_NDELAY | (T_LOW(twi)-T_HDDAT(twi)));
		ADD_CODE(twi,INSTR_SCL_H);
		ADD_CODE(twi,INSTR_SYNC);
		ADD_CODE(twi,INSTR_NDELAY | T_HIGH(twi));
		if(bit) {
			ADD_CODE(twi,INSTR_CHECK_ARB);
		}
		ADD_CODE(twi,INSTR_SCL_L);
		ADD_CODE(twi,INSTR_NDELAY | T_HDDAT(twi));
	}
	mscript_check_ack(twi);
	ADD_CODE(twi,INSTR_NDELAY | (T_LOW(twi)-T_HDDAT(twi)));
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
mscript_do_ack(AT91Twi *twi, int ack)
{
	if(ack == ACK) {
		ADD_CODE(twi,INSTR_SDA_L);
	} else {
		/* should already be in this state because do ack is done after reading only */
		ADD_CODE(twi,INSTR_SDA_H);
	}
	ADD_CODE(twi,INSTR_NDELAY | (T_LOW(twi)-T_HDDAT(twi)));
	ADD_CODE(twi,INSTR_SCL_H);
	ADD_CODE(twi,INSTR_SYNC);
	ADD_CODE(twi,INSTR_NDELAY | T_HIGH(twi));
	if(ack == NACK) {
		ADD_CODE(twi,INSTR_CHECK_ARB);
	}
	ADD_CODE(twi,INSTR_SCL_L);
	ADD_CODE(twi,INSTR_NDELAY | T_HDDAT(twi));
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
mscript_read_byte(AT91Twi *twi)
{
	int i;
	ADD_CODE(twi,INSTR_SDA_H);
	for(i=7;i>=0;i--) {
		ADD_CODE(twi,INSTR_NDELAY | (T_LOW(twi)-T_HDDAT(twi)));
		ADD_CODE(twi,INSTR_SCL_H);
		ADD_CODE(twi,INSTR_SYNC);
		ADD_CODE(twi,INSTR_NDELAY | T_HIGH(twi));
		ADD_CODE(twi,INSTR_READ_SDA);
		ADD_CODE(twi,INSTR_SCL_L);
		ADD_CODE(twi,INSTR_NDELAY | (T_HDDAT(twi)));
	}
	ADD_CODE(twi,INSTR_RXDATA_AVAIL);
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
mscript_stop(AT91Twi *twi)
{
	dbgprintf("Append stop\n");
	ADD_CODE(twi,INSTR_SDA_L);
	ADD_CODE(twi,INSTR_NDELAY | (T_LOW(twi)-T_HDDAT(twi)));
	ADD_CODE(twi,INSTR_SCL_H);
	ADD_CODE(twi,INSTR_SYNC);
	ADD_CODE(twi,INSTR_NDELAY | T_SUSTO(twi));
	ADD_CODE(twi,INSTR_SDA_H);
	ADD_CODE(twi,INSTR_NDELAY | T_BUF(twi));
	ADD_CODE(twi,INSTR_INTERRUPT | SR_TXCOMP);
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
static void
mscript_start(AT91Twi *twi,int startmode) {
	/* For repeated start do not assume SDA and SCL state */
	if(startmode == STARTMODE_REPSTART) {
		ADD_CODE(twi,INSTR_SDA_H);
		ADD_CODE(twi,INSTR_NDELAY | (T_LOW(twi)-T_HDDAT(twi)));

		ADD_CODE(twi,INSTR_SCL_H);
		ADD_CODE(twi,INSTR_SYNC);
		ADD_CODE(twi,INSTR_NDELAY | T_HIGH(twi));
	} else {
		ADD_CODE(twi,INSTR_WAIT_BUS_FREE);
	}

	/* Generate a start condition */
	ADD_CODE(twi,INSTR_CHECK_ARB);
	ADD_CODE(twi,INSTR_SDA_L);
	ADD_CODE(twi,INSTR_NDELAY | T_HDSTA(twi));
	ADD_CODE(twi,INSTR_SCL_L);
	ADD_CODE(twi,INSTR_NDELAY | T_HDDAT(twi));
}

/*
 * ------------------------------------------------------------------
 * reset_interpreter
 *      reset the I2C-master micro instruction interpreter.
 *      called before assembling a script or on abort of script.
 * ------------------------------------------------------------------
 */
static void
reset_interpreter(AT91Twi *twi) {
        twi->code[0] = INSTR_END;
        twi->ip = 0;
        twi->icount=0;
        twi->rxdata=0;
        twi->ack=0;
        CycleTimer_Remove(&twi->ndelayTimer);
}

static void run_interpreter(void *clientData);

static void
scl_timeout(void *clientData)
{
        AT91Twi *twi = (AT91Twi *)clientData;
	SigNode * dom = SigNode_FindDominant(twi->scl);
	if(dom) {
        	fprintf(stderr,"AT91Twi: I2C SCL seems to be blocked by %s\n",SigName(dom));
	} else {
        	fprintf(stderr,"AT91Twi: I2C SCL seems to be blocked\n");
	}
        SigNode_Untrace(twi->scl,twi->sclStretchTrace);
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
scl_released(SigNode *node,int value,void *clientData)
{
        AT91Twi *twi = (AT91Twi *)clientData;
        if((value == SIG_PULLUP) || (value == SIG_HIGH)) {
                SigNode_Untrace(twi->scl,twi->sclStretchTrace);
                CycleTimer_Remove(&twi->ndelayTimer);
                run_interpreter(clientData);
        }
}



static int
execute_instruction(AT91Twi *twi) {
        uint32_t icode;
        if(twi->ip >= CODE_MEM_SIZE) {
                fprintf(stderr,"AT91Twi: corrupt I2C script\n");
                return RET_EMU_ERROR;
        }
	if(twi->ip == (twi->icount % CODE_MEM_SIZE)) {
		return RET_DONE;
	}
        icode = twi->code[twi->ip];
	twi->ip = (twi->ip + 1) % CODE_MEM_SIZE;
        switch(icode&0xff000000) {
                case INSTR_SDA_H:
                        dbgprintf("SDA_H %08x\n",icode);
                        SigNode_Set(twi->sda,SIG_OPEN);
                        break;
                case INSTR_SDA_L:
                        dbgprintf("SDA_L %08x\n",icode);
                        SigNode_Set(twi->sda,SIG_LOW);
                        break;
                case INSTR_SCL_H:
                        dbgprintf("SCL_H %08x\n",icode);
                        SigNode_Set(twi->scl,SIG_OPEN);
                        break;
                case INSTR_SCL_L:
                        dbgprintf("SCL_L %08x\n",icode);
                        SigNode_Set(twi->scl,SIG_LOW);
                        break;

                case INSTR_CHECK_ARB:
			/* This core has no arbitration check */
                        break;

                case INSTR_NDELAY:
                        {
                                uint32_t nsecs = icode & 0xffffff;
                                int64_t cycles = NanosecondsToCycles(nsecs);
                                CycleTimer_Add(&twi->ndelayTimer,cycles,run_interpreter,twi);
                                dbgprintf("NDELAY %08x\n",icode);
                        }
                        return RET_DONE;
                        break;
		
                case INSTR_SYNC:
                        dbgprintf("SYNC %08x\n",icode);
                        if(SigNode_Val(twi->scl)==SIG_LOW) {
                                uint32_t msecs = 200;
                                int64_t cycles =  MillisecondsToCycles(msecs);
                                twi->sclStretchTrace = SigNode_Trace(twi->scl,scl_released,twi);
                                CycleTimer_Add(&twi->ndelayTimer,cycles,scl_timeout,twi);
                                return RET_DONE;
                        }
                        break;

                case INSTR_READ_SDA:
                        if(SigNode_Val(twi->sda) == SIG_LOW) {
                                twi->rxdata = (twi->rxdata<<1);
                        } else {
                                twi->rxdata = (twi->rxdata<<1) | 1;
                        }
                        break;

                case INSTR_READ_ACK:
                        dbgprintf("READ_ACK %08x\n",icode);
                        if(SigNode_Val(twi->sda) == SIG_LOW) {
                                twi->ack = ACK;
                        } else {
                                twi->ack = NACK;
                        }
                        break;

                case  INSTR_END:
                        dbgprintf("ENDSCRIPT %08x\n",icode);
			reset_interpreter(twi);
                        return RET_DONE;
                        break;

                case  INSTR_CHECK_ACK:
                        dbgprintf("CHECK_ACK %08x\n",icode);
                        if(twi->ack == NACK) {
				/* I should urgently check if TXRDY is really set on NACK */
				twi->sr |= SR_NACK | SR_TXRDY | SR_TXCOMP;
				update_interrupt(twi);
				return RET_DONE;
                        }
                        break;

                case  INSTR_INTERRUPT:
                        dbgprintf("INTERRUPT %08x\n",icode);
			twi->sr |= icode & 0xffff;
			update_interrupt(twi);
                        break;

                case  INSTR_RXDATA_AVAIL:
                        dbgprintf("RXDATA_AVAIL %02x %08x\n",twi->rxdata,icode);
			twi->rhr = twi->rxdata;
                        break;

                case  INSTR_WAIT_BUS_FREE:
			/* Wait bus free currently not implemented */
			break;


                default:
                        fprintf(stderr,"AT91Twi: I2C: Unknode icode %08x\n",icode);
                        return RET_EMU_ERROR;
                        break;

        }
        return RET_DO_NEXT;
}

/*
 * ----------------------------------------------------------------
 * run_interpreter
 *      The I2C-master micro instruction interpreter main loop
 *      executes instructions until the script is done or
 *      waits for some event
 * ----------------------------------------------------------------
 */
static void
run_interpreter(void *clientData)
{
        AT91Twi *twi = (AT91Twi *) clientData;
        int retval;
        do {
                retval = execute_instruction(twi);
        } while(retval == RET_DO_NEXT);
}

/*
 * -----------------------------------------------------------------------------
 * start interpreter 
 *      Start execution of micro operation I2C scripts
 * -----------------------------------------------------------------------------
 */
static void
start_interpreter(AT91Twi *twi) {
        if(CycleTimer_IsActive(&twi->ndelayTimer)) {
                dbgprintf("AT91Twi: Starting already running interp.\n");
                return;
        }
        CycleTimer_Add(&twi->ndelayTimer,0,run_interpreter,twi);
}

static uint32_t
cr_read(void *clientData,uint32_t address,int rqlen)
{
        fprintf(stderr,"AT91Twi: Reading from write only Control register\n");
        return 0;
}

static void
cr_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	AT91Twi *twi = (AT91Twi *) clientData;
	int dir_read = !!(twi->mmr & MMR_MREAD);
	int run = 0;
	if(value & CR_MSEN) {
		twi->sr |= SR_TXCOMP | SR_TXRDY;
		twi->cr |= CR_MSEN;
		twi->cr &= ~CR_MSDIS;
	}
	if(value & CR_MSDIS) {
		twi->cr &= ~CR_MSEN;
		twi->cr |= CR_MSDIS;
	}
	if(twi->cr & CR_MSEN) {
		if(value & CR_START) {
			uint16_t addr = (twi->mmr & MMR_DADR_MASK) >> MMR_DADR_SHIFT;
			int iadrsz = (twi->mmr & MMR_IADRSZ_MASK) >> MMR_IADRSZ_SHIFT;
			int i;
			update_interrupt(twi);
			reset_interpreter(twi);
			// fprintf(stderr,"Device addr is %02x addrsz %d, iaddr %08x read %d\n",addr,iadrsz,twi->iadr,dir_read); // jk

			if(iadrsz) {
				mscript_start(twi,STARTMODE_START);
				mscript_write_byte(twi,(addr<<1));
				for(i=iadrsz-1;i>=0;i--) {	
					mscript_write_byte(twi,twi->iadr >> (i*8));
				}
			}
			if(dir_read) {
				twi->sr &= ~(SR_TXCOMP | SR_RXRDY);
				update_interrupt(twi);
				dbgprintf("Read: append repstart\n");
				if(iadrsz) {
					mscript_start(twi,STARTMODE_REPSTART);
				} else {
					mscript_start(twi,STARTMODE_START);
				}
				mscript_write_byte(twi,(addr<<1) | dir_read);
				mscript_read_byte(twi);
			} else {
				if(iadrsz == 0) {
					mscript_start(twi,STARTMODE_START);
					mscript_write_byte(twi,(addr<<1));
				}
				ADD_CODE(twi,INSTR_INTERRUPT | SR_TXRDY);
			}
			run = 1;
		}
		if(value & CR_STOP) {
			if(dir_read) {
				mscript_do_ack(twi,NACK);
				ADD_CODE(twi,INSTR_INTERRUPT | SR_RXRDY);
			} 
			mscript_stop(twi);
			run = 1;
			twi->cr |= CR_STOP;
		} else {
			twi->cr &= ~CR_STOP;
			if(dir_read) {
				ADD_CODE(twi,INSTR_INTERRUPT | SR_RXRDY);
			}
		}
	}
	if(run) {
		start_interpreter(twi);
	}
	if(FEAT_SLAVE(twi)) {
		uint32_t i2caddr = (twi->smr & SMR_SADR_MASK) >> SMR_SADR_SHIFT;
		if((value & CR_SVEN) && !(twi->cr & CR_SVEN)) {
                	I2C_SerDesAddSlave(twi->serdes,&twi->i2c_slave,i2caddr);
			twi->cr |= CR_SVEN;
			twi->cr &= ~CR_SVDIS;
		}
		if((value & CR_SVDIS) && (twi->cr & CR_SVEN)) {
	 		I2C_SerDesDetachSlave(twi->serdes,&twi->i2c_slave);
			twi->cr &= ~CR_SVEN;
			twi->cr |= CR_SVDIS;
		}
	}
}

static uint32_t
mmr_read(void *clientData,uint32_t address,int rqlen)
{
	AT91Twi *twi = (AT91Twi *) clientData;
        return twi->mmr;
}

static void
mmr_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	AT91Twi *twi = (AT91Twi *) clientData;
	twi->mmr = value & 0x007f1300;
}

static uint32_t
smr_read(void *clientData,uint32_t address,int rqlen)
{
	AT91Twi *twi = (AT91Twi *) clientData;
        return twi->smr;
}

static void
smr_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	AT91Twi *twi = (AT91Twi *) clientData;
	/* Must be programmed before Salve mode is enabled. Write at other
	   times has no effect */
	if(twi->cr & CR_SVEN) {
		fprintf(stderr,"SMR Warning: Programm slave address after Slave is enabled\n");
	} else {
		twi->smr = value & 0x007f00;
	}
}

static uint32_t
iadr_read(void *clientData,uint32_t address,int rqlen)
{
	AT91Twi *twi = (AT91Twi *) clientData;
        return twi->iadr;
}

static void
iadr_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	AT91Twi *twi = (AT91Twi *) clientData;
	twi->iadr = value & 0x00ffffff;
}

static uint32_t
cwgr_read(void *clientData,uint32_t address,int rqlen)
{
	AT91Twi *twi = (AT91Twi *) clientData;
        return twi->cwgr;
}

static void
update_timings(AT91Twi *twi) 
{
	I2C_Timing *timing = &twi->i2c_timing;
	int tmc;
	int ckdiv = (twi->cwgr & CWGR_CKDIV_MASK) >> CWGR_CKDIV_SHIFT;
	int chdiv = (twi->cwgr & CWGR_CHDIV_MASK) >> CWGR_CHDIV_SHIFT;
	int cldiv = (twi->cwgr & CWGR_CLDIV_MASK) >> CWGR_CLDIV_SHIFT;
	int lowtime;
	int hightime;
	/* Should also be updated on clock change */
	if(Clock_Freq(twi->clk) >= 1) {
		tmc = 1000000000 / Clock_Freq(twi->clk);
	} else {
		tmc = 0xffff;
	}
	lowtime =  ((cldiv << ckdiv) + 3) * tmc;
	hightime = ((chdiv << ckdiv) + 3) * tmc;
	if(ckdiv > 5) {
		fprintf(stderr,"AT91Twi: CKDIV must be <= 5 (see errata)\n");
	}
	
	timing->t_hdsta = hightime * 600 / 700;
	timing->t_high = hightime * 600 / 700;
	timing->t_susta = hightime * 600 / 700;
	timing->t_susto = hightime * 600 / 700;
	timing->t_sudat = hightime * 100 / 700;

	timing->t_low = lowtime * 1300 / 1300;
	timing->t_hddat_max = lowtime * 900 / 1300;
	timing->t_buf = lowtime * 1300 / 1300;
        fprintf(stderr,"AT91Twi: cwgr %08x high_time %d, lowtime %d t_high %d\n",twi->cwgr,hightime,lowtime,timing->t_high);
#if 0
	if(Clock_Find("pmc.main_clk")) {
		Clock_DumpTree(Clock_Find("pmc.main_clk"));
		//*(char*) 0 = 0;
	}
#endif
}

/*
 ********************************************************************************
 * Update the timing parameters when the clock changes its frequency
 ********************************************************************************
 */
static void
ClockTrace(struct Clock *clock,void *clientData)
{
        AT91Twi *twi = (AT91Twi *) clientData;
	dbgprintf(stderr,"New TWI clock is %f\n",freq);
        update_timings(twi);
}


static void
cwgr_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	AT91Twi *twi = (AT91Twi *) clientData;
	twi->cwgr = value & 0x0007ffff;
	update_timings(twi);
}

static uint32_t
sr_read(void *clientData,uint32_t address,int rqlen)
{
	AT91Twi *twi = (AT91Twi *) clientData;
	uint32_t sr = twi->sr;
	if(!(sr & twi->imr) && !(sr & SR_RXRDY) && !(~sr & SR_TXRDY)) {
		Senseless_Report(200);
	}
	twi->sr &= ~SR_EOSACC;
        return sr;
}

static void
sr_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
        fprintf(stderr,"AT91Twi: SR is readonly\n");
}

static uint32_t
ier_read(void *clientData,uint32_t address,int rqlen)
{
        fprintf(stderr,"AT91Twi: IER is write only");
        return 0;
}
static void
ier_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	AT91Twi *twi = (AT91Twi*) clientData;
	twi->imr |= value & 0x1c7;
	update_interrupt(twi);
}

static uint32_t
idr_read(void *clientData,uint32_t address,int rqlen)
{
        fprintf(stderr,"AT91Twi: IDR is writeonly\n");
        return 0;
}

static void
idr_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	AT91Twi *twi = (AT91Twi*) clientData;
	twi->imr &= ~(value & 0x1c7);
	update_interrupt(twi);
}

static uint32_t
imr_read(void *clientData,uint32_t address,int rqlen)
{
	AT91Twi *twi = (AT91Twi*) clientData;
        return twi->imr;
}

static void
imr_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
        fprintf(stderr,"AT91Twi: IMR is readonly\n");
}

static uint32_t
rhr_read(void *clientData,uint32_t address,int rqlen)
{
	AT91Twi *twi = (AT91Twi*) clientData;
	uint32_t rhr = twi->rhr;
	if(twi->sr & SR_RXRDY) {
		twi->sr &= ~SR_RXRDY;
		if(twi->sr & SR_SCLWS) {
			twi->sr &= ~SR_SCLWS;
                	SerDes_UnstretchScl(twi->serdes);
        	}
		// if(state == READ) 
		/* Should be some state check instead */
		if(twi->cr & CR_MSEN) {
			if(!(twi->cr & CR_STOP)) {
				mscript_do_ack(twi,ACK);
				mscript_read_byte(twi);
				ADD_CODE(twi,INSTR_INTERRUPT | SR_RXRDY);
				start_interpreter(twi);
			}
		}
		// 
		update_interrupt(twi);
	}
	dbgprintf("AT91Twi: read byte %02x\n",twi->rhr);
        return rhr;
}

static void
rhr_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
        fprintf(stderr,"AT91Twi: Writing to readonly Receive Holding Register\n");
}

static uint32_t
thr_read(void *clientData,uint32_t address,int rqlen)
{
	AT91Twi *twi = (AT91Twi *)clientData;
	fprintf(stderr,"AT91Twi: Reading from write only Transmit Holding Register\n");
        return twi->thr;
}

static void
thr_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	AT91Twi *twi = (AT91Twi *)clientData;
	/* Should only be done if in master mode */
	if(twi->cr & CR_MSEN) {
		if(twi->sr & SR_TXRDY) {
			twi->thr = value;
			dbgprintf("AT91Twi: write byte %02x\n",value);
			mscript_write_byte(twi,twi->thr);
			ADD_CODE(twi,INSTR_INTERRUPT | SR_TXRDY);
			start_interpreter(twi);
		}
	}
	/* In write mode TXCOMP goes low on write thr  and high on stop condition */
	twi->sr &= ~(SR_TXCOMP | SR_TXRDY);
	/* State check if really in slave read mode missing here ?? */
	if(twi->sr & SR_SCLWS) {
		twi->sr &= ~SR_SCLWS;
		SerDes_UnstretchScl(twi->serdes);
	}
	update_interrupt(twi);
}

static void
AT91Twi_Map(void *owner,uint32_t base,uint32_t mask,uint32_t flags)
{
        AT91Twi *twi = (AT91Twi*) owner;
	IOH_New32(TWI_CR(base),cr_read,cr_write,twi);
	IOH_New32(TWI_MMR(base),mmr_read,mmr_write,twi);
	if(FEAT_SLAVE(twi)) {	
		IOH_New32(TWI_SMR(base),smr_read,smr_write,twi);
	}
	IOH_New32(TWI_IADR(base),iadr_read,iadr_write,twi);
	IOH_New32(TWI_CWGR(base),cwgr_read,cwgr_write,twi);
	IOH_New32(TWI_SR(base),sr_read,sr_write,twi);
	IOH_New32(TWI_IER(base),ier_read,ier_write,twi);
	IOH_New32(TWI_IDR(base),idr_read,idr_write,twi);
	IOH_New32(TWI_IMR(base),imr_read,imr_write,twi);
	IOH_New32(TWI_RHR(base),rhr_read,rhr_write,twi);
	IOH_New32(TWI_THR(base),thr_read,thr_write,twi);
}

static void
AT91Twi_UnMap(void *owner,uint32_t base,uint32_t mask)
{
        AT91Twi *twi = (AT91Twi*) owner;
	IOH_Delete32(TWI_CR(base));
	IOH_Delete32(TWI_MMR(base));
	if(FEAT_SLAVE(twi)) {	
		IOH_Delete32(TWI_SMR(base));
	}
	IOH_Delete32(TWI_IADR(base));
	IOH_Delete32(TWI_CWGR(base));
	IOH_Delete32(TWI_SR(base));
	IOH_Delete32(TWI_IER(base));
	IOH_Delete32(TWI_IDR(base));
	IOH_Delete32(TWI_IMR(base));
	IOH_Delete32(TWI_RHR(base));
	IOH_Delete32(TWI_THR(base));

}


/*
 * -----------------------------------------------------------
 * slave_write
 *      Accept a byte as slave if ready. If not ready
 *      advise the slave module to stretch SCL and call
 *      again when stretching is finished
 * -----------------------------------------------------------
 */
static int
twislave_write(void *dev,uint8_t data)
{
        AT91Twi *twi = (AT91Twi*) dev;
	if(twi->sr  & SR_RXRDY) {
		twi->sr |= SR_SCLWS;
		return I2C_STRETCH_SCL;
	} else {
		twi->rhr = data;
		twi->sr |= SR_RXRDY;
		return I2C_ACK;
	}
}

/*
 * -----------------------------------------------------------------------
 * slave_read
 *      return the next data byte which is sent over I2C.
 *      if not available SCL stretching stops the slave state machine
 *      until CTDR_TXVAL is set.
 * -----------------------------------------------------------------------
 */

static int
twislave_read(void *dev,uint8_t *data)
{
        AT91Twi *twi = (AT91Twi*) dev;
	if(twi->sr & SR_TXRDY) {
		*data = twi->thr;
		return I2C_DONE;	
	} else {
		twi->sr |= SR_SCLWS;
		return I2C_STRETCH_SCL;
	}
}


/*
 * --------------------------------------
 * slave_start
 *      Start a slave transaction
 * --------------------------------------
 */
static int
twislave_start(void *dev,int i2c_addr,int operation)
{
        AT91Twi *twi = (AT91Twi*) dev;
	twi->sr |= SR_SVACC;
	if(operation == I2C_READ) {
		twi->sr |= SR_SVREAD;
	} else if (operation == I2C_READ) {
		twi->sr &= ~SR_SVREAD;
	} else {
                return I2C_NACK;
	}
	update_interrupt(twi);
	return I2C_ACK;
}

static void
twislave_stop(void *dev)
{
        AT91Twi *twi = (AT91Twi*) dev;
	twi->sr &= ~SR_SVACC;
	twi->sr |= SR_EOSACC;
	update_interrupt(twi);
	return;
}


/*
 * --------------------------------------------------------
 * read_ack
 *      When the master acknowleges the byte sent by
 *      the slave the next byte is requested by posting
 *      a TX_DATA_IRQ. If the master doesn't ack a
 *      NO_ACK_IRQ is posted.
 * --------------------------------------------------------
 */
static void
twislave_read_ack(void *dev, int ack)
{
	AT91Twi *twi = (AT91Twi *) dev;
	twi->sr |= SR_TXRDY;
	if(ack == I2C_ACK) {
		
	} else if (ack == I2C_NACK) {
		twi->sr |= SR_NACK;
		update_interrupt(twi);

	}
}

static I2C_SlaveOps twislave_ops = {
	.start = twislave_start,
	.stop =  twislave_stop,
	.read =  twislave_read,
	.write = twislave_write,
	.read_ack =  twislave_read_ack
};

BusDevice *
AT91Twi_New(const char *name,uint32_t features)
{
	char *sdaname = (char*)alloca(strlen(name)+50);
        char *sclname = (char*)alloca(strlen(name)+50);
	I2C_Slave *i2c_slave;
        AT91Twi *twi = sg_new(AT91Twi);

        twi->name = sg_strdup(name);
        twi->irqNode = SigNode_New("%s.irq",name);
	sprintf(sdaname,"%s.sda",name);
	sprintf(sclname,"%s.scl",name);
        twi->sda = SigNode_New("%s",sdaname);
        twi->scl = SigNode_New("%s",sclname);
        if(!twi->irqNode || !twi->sda || !twi->scl) {
                fprintf(stderr,"AT91Twi: Can not create signal lines\n");
                exit(1);
        }
	
	if(features & TWI_FEATURE_SLAVE) {
		/* Create the slave */
		char *slvname = (char*)alloca(strlen(name)+50);
		char *slvsdaname = (char*)alloca(strlen(name)+50);
		char *slvsclname = (char*)alloca(strlen(name)+50);
		i2c_slave = &twi->i2c_slave;
		i2c_slave->devops = &twislave_ops;
		i2c_slave->dev = (void *)twi;
		i2c_slave->speed = I2C_SPEED_FAST;

		sprintf(slvname,"%s.slave",name);
		twi->serdes = I2C_SerDesNew(slvname);

		/* Connect the scl and sda from slave with master */
		sprintf(slvsclname,"%s.slave.scl",name);
		SigName_Link(sclname,slvsclname);

		sprintf(slvsdaname,"%s.slave.sda",name);
		SigName_Link(sdaname,slvsclname);
	}

        SigNode_Set(twi->irqNode,SIG_PULLDOWN);
	twi->clk = Clock_New("%s.clk",name);
	/* Should be connected to mclk instead of be set here */
	twi->clkTrace = Clock_Trace(twi->clk,ClockTrace,twi);
        twi->bdev.first_mapping=NULL;
        twi->bdev.Map=AT91Twi_Map;
        twi->bdev.UnMap=AT91Twi_UnMap;
        twi->bdev.owner=twi;
        twi->bdev.hw_flags=MEM_FLAG_WRITABLE|MEM_FLAG_READABLE;
	if(features & TWI_FEATURE_SLAVE) {
		twi->sr = 0xf009; 
	} else {
		twi->sr = 8; 
	}
	twi->features = features;
	reset_interpreter(twi);
	update_timings(twi);
        update_interrupt(twi);
        fprintf(stderr,"AT91RM9200 TWI \"%s\" created\n",name);
        return &twi->bdev;
}

