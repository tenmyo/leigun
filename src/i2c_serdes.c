/*
 *************************************************************************************************
 *
 * I2C-Serializer/Deserializer
 *	Convert Pin Level changes into I2C-Device operations
 *	and convert I2C-Device results back to Pin Level changes
 *
 *  State: working
 *
 * Copyright 2004 2005 Jochen Karrer. All rights reserved.
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
#include <unistd.h>
#include <string.h>
#include "signode.h"
#include "i2c_serdes.h"
#include "i2c.h"
#include "compiler_extensions.h"
#include "cycletimer.h"
#include "sgstring.h"

#if 0
#define dbgprintf(x...) { fprintf(stderr,x);fflush(stderr);}
#else
#define dbgprintf(x...)
#endif

#define I2C_TIME_INFINITE (~0)
#define SAT_U32(x) ((x) > ~((uint32_t)0) ? ~((uint32_t)0) : x)

/* 
 * ------------------------------------
 * Unit for times is nanoseconds
 * ------------------------------------
 */
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

/*
 * -------------------------------------
 * Meassured timings are classified by
 * this table.
 * Timings have to be sorted by speed
 * -------------------------------------
 */
static 
I2C_Timing i2c_timings[] = {
	{
		.speed = I2C_SPEED_STD,
		.t_hdsta = 4000,
		.t_low = 4700,	
		.t_high = 4000,
		.t_susta = 4700,
		.t_hddat_max = 3450,
		.t_sudat = 250,
		.t_susto = 4000,
		.t_buf	 = 4700
		
	},
	{
		.speed = I2C_SPEED_FAST,
		.t_hdsta = 600,
		.t_low = 1300,
		.t_high = 600,
		.t_susta = 600,
		.t_hddat_max = 900,
		.t_sudat = 100,
		.t_susto = 600,
		.t_buf = 1300
	},	
	{
		-1,
	}
};


#define I2C_SDA (1)
#define I2C_SCL (2)

#define I2C_STATE_IDLE (0)
#define I2C_STATE_ADDR (1)
#define I2C_STATE_ACK_READ  	 (2)
#define I2C_STATE_ACK_READ_ADDR  (3)
#define I2C_STATE_ACK_WRITE 	 (4)
#define I2C_STATE_NACK_WRITE 	 (5)
#define I2C_STATE_READ  (6)
#define I2C_STATE_WRITE (7)
#define I2C_STATE_WAIT  (8)

struct I2C_SerDes {
	char *name;
	SigNode *scl;
	SigNode *scl_pullup;
	SigTrace *sclTrace;
	SigNode *sda;
	SigNode *sda_pullup;
	SigTrace *sdaTrace;
	
	I2C_Slave *slave_list;
	I2C_Slave *active_slave; /* Todo: multiple simultanously active devices */
	int slave_was_accessed;
	int address;
	I2C_Timing timing;	/* This is the meassured timing */
	uint8_t inbuf;
	uint8_t outbuf;
	int bitcount;
	int state;
	int oldpinstate;
	int outpinstate;
	int stretch_scl;
	int (*scl_delayed_proc)(struct I2C_SerDes *);
	CycleCounter_t sda_change_time;
	CycleCounter_t scl_change_time;
	CycleTimer sdaDelayTimer;
	CycleTimer sclDelayTimer;
};

static void 
invalidate_timing(I2C_Timing *timing) {
	timing->t_hdsta = I2C_TIME_INFINITE;
	timing->t_low = I2C_TIME_INFINITE;
	timing->t_high = I2C_TIME_INFINITE;
	timing->t_susta = I2C_TIME_INFINITE;
	timing->t_hddat_max = 0;
	timing->t_sudat = I2C_TIME_INFINITE;
	timing->t_susto = I2C_TIME_INFINITE;
	timing->t_buf = I2C_TIME_INFINITE;
}

static void 
dump_timing(I2C_Timing *timing) {
	fprintf(stderr,"T_HDSTA %u\n",timing->t_hdsta);
	fprintf(stderr,"T_LOW   %u\n",timing->t_low);
	fprintf(stderr,"T_HIGH  %u\n",timing->t_high);
	fprintf(stderr,"T_SUSTA %u\n",timing->t_susta);
	fprintf(stderr,"T_SUDAT %u\n",timing->t_sudat);
	fprintf(stderr,"T_SUSTO %u\n",timing->t_susto);
	fprintf(stderr,"T_BUF   %u\n",timing->t_buf);
}
/*
 * -----------------------------------------------------------------
 * Check if a meassured timing set matches a allowed set of timing
 * -----------------------------------------------------------------
 */
static int 
compare_timings(I2C_Timing *meas,I2C_Timing *allowed) {
	if(meas->t_hdsta < allowed->t_hdsta) {
		return 0;
	} 
	if(meas->t_low < allowed->t_low) {
		return 0;
	}
	if(meas->t_high <  allowed->t_high) {
		return 0;
	}
	if(meas->t_susta < allowed->t_susta) {
		return 0;
	}
	if(meas->t_hddat_max > allowed->t_hddat_max) {
		return 0;
	}	
	if(meas->t_sudat < allowed->t_sudat) {
		return 0;
	}
	if(meas->t_susto < allowed->t_susto) {
		return 0;
	}
	if(meas->t_buf < allowed->t_buf) {
		return 0;
	}
	return 1;

}
/*
 * ------------------------------------------------------
 * returns a speed calculated from a meassured timing
 * ------------------------------------------------------
 */
static int 
check_speed(I2C_Timing *meas) 
{
	I2C_Timing *compare;
	for(compare = i2c_timings;compare->speed >= 0; compare++) {
		if(compare_timings(meas,compare)) {
			break;
		}		
	}
	if(compare->speed < 0) {
		fprintf(stderr,"Illegal I2C Bus timing\n"); 
		dump_timing(meas);
	}
	return compare->speed;
}


static inline void 
update_t_hdsta(I2C_SerDes *serdes,int line) 
{
	CycleCounter_t now = CycleCounter_Get();	
	int64_t nsecs = CyclesToNanoseconds(now -  serdes->sda_change_time);
	nsecs = SAT_U32(nsecs);
	if(nsecs < serdes->timing.t_hdsta) {
		serdes->timing.t_hdsta = nsecs;
	}
}
static inline void 
update_t_low(I2C_SerDes *serdes,int line) 
{
	CycleCounter_t now = CycleCounter_Get();	
	int64_t nsecs = CyclesToNanoseconds(now -  serdes->scl_change_time);
	nsecs = SAT_U32(nsecs);
	if(nsecs < serdes->timing.t_low) {
		serdes->timing.t_low = nsecs;
	}
}

static inline void 
update_t_high(I2C_SerDes *serdes,int line) 
{
	CycleCounter_t now = CycleCounter_Get();	
	int64_t nsecs = CyclesToNanoseconds(now -  serdes->scl_change_time);
	nsecs = SAT_U32(nsecs);
	if(nsecs < serdes->timing.t_high) {
		serdes->timing.t_high = nsecs;
	}
#if 0
	if(nsecs < 600) {
		fprintf(stderr,"Timing violation in %d pc %08x\n",line,REG_PC);
	}
#endif
}
static inline void 
update_t_susta(I2C_SerDes *serdes,int line) 
{
	CycleCounter_t now = CycleCounter_Get();	
	int64_t nsecs = CyclesToNanoseconds(now -  serdes->scl_change_time);
	nsecs = SAT_U32(nsecs);
	if(nsecs < serdes->timing.t_susta) {
		serdes->timing.t_susta = nsecs;
	}
}

__UNUSED__ static void 
update_t_hddat_max(I2C_SerDes *serdes,int line) 
{
	CycleCounter_t now = CycleCounter_Get();	
	int64_t nsecs = CyclesToNanoseconds(now -  serdes->scl_change_time);
	nsecs = SAT_U32(nsecs);
	if(nsecs > serdes->timing.t_hddat_max) {
		serdes->timing.t_hddat_max = nsecs;
	}
}

static inline void 
update_t_sudat(I2C_SerDes *serdes,int line) 
{
	CycleCounter_t now = CycleCounter_Get();	
	int64_t nsecs = CyclesToNanoseconds(now -  serdes->sda_change_time);
	nsecs = SAT_U32(nsecs);
	if(nsecs < serdes->timing.t_sudat) {
		serdes->timing.t_sudat = nsecs;
	}
}
static inline void 
update_t_susto(I2C_SerDes *serdes,int line) 
{
	CycleCounter_t now = CycleCounter_Get();	
	int64_t nsecs = CyclesToNanoseconds(now -  serdes->scl_change_time);
	nsecs = SAT_U32(nsecs);
	if(nsecs < serdes->timing.t_susto) {
		serdes->timing.t_susto = nsecs;
	}
}
static inline void 
update_t_buf(I2C_SerDes *serdes,int line) 
{
	CycleCounter_t now = CycleCounter_Get();	
	int64_t nsecs = CyclesToNanoseconds(now -  serdes->sda_change_time);
	nsecs = SAT_U32(nsecs);
	if(nsecs < serdes->timing.t_buf) {
		serdes->timing.t_buf = nsecs;
	}
}

/* return 1 if t-buf was ok else 0 */
static inline int 
check_t_buf(I2C_SerDes *serdes,I2C_Slave *slave)
{
	I2C_Timing *timing;
	for(timing = i2c_timings;timing->speed>=0;timing++) {
		if(timing->speed == slave->tolerated_speed) {
			break;
		}
	}
	if(timing->speed < 0) {
		fprintf(stderr,"Emulator bug: I2C slave has no speed grade\n");
		return 0;
	}
	if(serdes->timing.t_buf < (timing->t_buf>>1)) {
		fprintf(stderr,"I2C-serdes: bad t_buf < %d nsec instead of %d ns\n",serdes->timing.t_buf,timing->t_buf);
		return 0;
	}
	return 1;
}

static void
reset_serdes(I2C_SerDes *serdes) {
	invalidate_timing(&serdes->timing);
	serdes->state=I2C_STATE_IDLE;
	serdes->bitcount=0;
	serdes->address = 0;
	serdes->active_slave = NULL;
	serdes->inbuf=0;
	serdes->outbuf=0;
	serdes->stretch_scl = 0;
	serdes->slave_was_accessed=0;
	serdes->outpinstate=I2C_SDA | I2C_SCL;
}

void
SerDes_Decouple(I2C_SerDes *serdes) 
{
	serdes->state = I2C_STATE_WAIT;
	serdes->outpinstate=I2C_SDA | I2C_SCL;
	serdes->stretch_scl = 0;
	serdes->scl_delayed_proc = NULL;
	SigNode_Set(serdes->sda,SIG_OPEN);
	SigNode_Set(serdes->scl,SIG_OPEN);
}

/*
 * -----------------------------------------------------
 * delayed_read
 * 	Read from slave delayed by scl stretching
 * -----------------------------------------------------
 */

static int
delayed_read(I2C_SerDes *serdes) {
	I2C_Slave *slave=serdes->active_slave;
	int result;
	fprintf(stderr,"Do delayed read, bitcount %d\n",serdes->bitcount);
	result = slave->devops->read(slave->dev,&serdes->outbuf);
	serdes->bitcount--;
	serdes->outpinstate = I2C_SCL; 
	if((serdes->outbuf&(1<<serdes->bitcount))!=0) {
		serdes->outpinstate |= I2C_SDA;
	}
	return  serdes->outpinstate;
}

/*
 * -----------------------------------------------------
 * delayed_write
 * 	Read from slave delayed by scl stretching
 * -----------------------------------------------------
 */
static int
delayed_write(I2C_SerDes *serdes) {
	I2C_Slave *slave=serdes->active_slave;
	int result;
	fprintf(stderr,"Do delayed write\n");
	result = slave->devops->write(slave->dev,serdes->inbuf);	
	if(result==I2C_ACK) {
		serdes->state = I2C_STATE_ACK_WRITE;
		serdes->outpinstate |= I2C_SCL;	
	} else if(result == I2C_NACK) {
		serdes->state = I2C_STATE_NACK_WRITE;
		serdes->outpinstate |= I2C_SCL;	
	} else if(result == I2C_STRETCH_SCL) {
		serdes->stretch_scl++;
		serdes->outpinstate |= I2C_SDA;	
		fprintf(stderr,"Stretching again should never happen\n");
	} else {
		fprintf(stderr,"Bug: Unknown I2C-Result %d\n",result);
	}
	return  serdes->outpinstate;
}
/*
 * ------------------------------------------------------------------
 * I2C_SerDesFeed
 *
 *	This is the main state machine.
 * 	Feed with logical Levels, calls I2C-Slave Operations nad 
 * 	returns the Output driver state of the
 * 	connected I2C-Chips (0==driver active low) (1==driver inactive)
 * ------------------------------------------------------------------
 */
static void 
I2C_SerDesFeed(I2C_SerDes *serdes,int pinstate) 
{
	int scl=pinstate&I2C_SCL;
	int sda=pinstate&I2C_SDA;
	int oldscl=serdes->oldpinstate&I2C_SCL;
	int oldsda=serdes->oldpinstate&I2C_SDA;

	dbgprintf("feed  %02x, prev %02x, state %d n %s\n",pinstate,serdes->oldpinstate,serdes->state,serdes->name);
	if(oldscl && !scl && serdes->stretch_scl) {
		serdes->outpinstate = I2C_SDA;
		return;
	}
	/* Detect Start/Repeated condition */
	if(scl && oldsda  && !sda) {
		reset_serdes(serdes);
		update_t_buf(serdes,__LINE__);
		update_t_susta(serdes,__LINE__);
		dbgprintf("Start Condition\n");
		serdes->state=I2C_STATE_ADDR;
		return;
	}
	/* Stop condition */
	if(scl && !oldsda  && sda) {
		I2C_Slave *slave;
		slave = serdes->active_slave;
		update_t_susto(serdes,__LINE__);
		if(slave) {
			int speed=check_speed(&serdes->timing); 
			int max_speed;
			if(serdes->slave_was_accessed) {
				max_speed = slave->speed;
			} else {
				max_speed = slave->tolerated_speed;
			}
			if( speed > max_speed) {
				static int count=0;
				if(count<10) {
					count++;
					fprintf(stderr,"Wrong timing %d for I2C-Slave %02x\n",speed,serdes->address);
					dump_timing(&serdes->timing);
				}
			} else if(speed!=slave->speed) {
				//fprintf(stderr,"Not exact match: timing %d for I2C-Slave %02x\n",speed,serdes->address);
			}
			slave->devops->stop(slave->dev);
		}
		reset_serdes(serdes);
		dbgprintf("Stop Condition\n");
		return;
	}
	switch(serdes->state) {
		case I2C_STATE_IDLE: 
			serdes->outpinstate = I2C_SDA | I2C_SCL;
			break;

		case I2C_STATE_ADDR:
			if(oldscl && !scl) {
				update_t_high(serdes,__LINE__);
				if(serdes->bitcount == 0) {
					update_t_hdsta(serdes,__LINE__);
				}
				serdes->outpinstate = I2C_SDA | I2C_SCL;
			} else if(!oldscl && scl) {
				update_t_low(serdes,__LINE__);
				update_t_sudat(serdes,__LINE__);
				serdes->bitcount++;
				serdes->inbuf<<=1;		
				if(sda) {
					serdes->inbuf|=1;
				}			
				dbgprintf("inbuf 0x%02x\n",serdes->inbuf);
				if(serdes->bitcount==8) {
					I2C_Slave *slave;
					serdes->address = serdes->inbuf >> 1;
					for(slave = serdes->slave_list;slave; slave=slave->next) 
					{
						if((serdes->address & slave->addr_mask) == slave->address) {
							break;
						}
					}
					dbgprintf("I2C-Address %02x slave %p\n",serdes->inbuf>>1,slave);	
					if(slave) {
						int result;
						if(!check_t_buf(serdes,slave)) {
							fprintf(stderr,"Device did not correctly detect start/stop condition because of bad t_buf\n");
							reset_serdes(serdes);
							break;
						}
						if(serdes->inbuf&1) {
							serdes->state=I2C_STATE_ACK_READ_ADDR;
							result = slave->devops->start(slave->dev,serdes->address,I2C_READ);
						} else {
							serdes->state=I2C_STATE_ACK_WRITE;
							result = slave->devops->start(slave->dev,serdes->address,I2C_WRITE);
						}
						if(result == I2C_ACK) {
							serdes->active_slave = slave;
							serdes->outpinstate = I2C_SDA | I2C_SCL;
						} else 	if(result == I2C_NACK){
							serdes->outpinstate = I2C_SDA | I2C_SCL;
							serdes->state = I2C_STATE_WAIT;
						} else if(result == I2C_STRETCH_SCL) {
							serdes->active_slave = slave;
							serdes->stretch_scl++;
						}
					} else {
						reset_serdes(serdes);
					}
				}
			}	
			break;

		case I2C_STATE_ACK_READ_ADDR:
			if(oldscl && !scl) {
				update_t_high(serdes,__LINE__);
				serdes->outpinstate = I2C_SCL; // ack, should be delayed
			} else if(!oldscl && scl) {
				update_t_low(serdes,__LINE__);
				serdes->state=I2C_STATE_READ;
				serdes->bitcount=8;
				serdes->slave_was_accessed=1;
			}
			break;

		case I2C_STATE_ACK_READ:
			if(oldscl && !scl) {
				update_t_high(serdes,__LINE__);
				/* release the lines */
				//dbgprintf(stderr,"Release the lines for the getting ACK/NACK\n");
				serdes->outpinstate = I2C_SCL | I2C_SDA; 
			} else if(!oldscl && scl) {
				I2C_Slave *slave=serdes->active_slave;
				update_t_low(serdes,__LINE__);
				update_t_sudat(serdes,__LINE__);
				if(!slave) {
					fprintf(stderr,"I2C-SerDes Bug: no slave in I2C-read\n");
					exit(5324);
				}
				if(sda) {
					/* Last was not acknowledged, so read nothing  */
					dbgprintf("Not acked\n");
					serdes->state=I2C_STATE_WAIT;
					/* 
 					 * -----------------------------------------------------------
					 * forward nack to device for example 
					 * to trigger a NACK interrupt
 					 * -----------------------------------------------------------
 					 */
					if(slave->devops->read_ack) {
						slave->devops->read_ack(slave->dev,I2C_NACK);
					}
					if(!(serdes->outpinstate & I2C_SDA)) {
						fprintf(stderr,"Emulator Bug in %s line %d\n",__FILE__,__LINE__);
					}
				} else {
					/* 
 					 * -----------------------------------------------------------
					 * forward ack to device because it  
 					 * can start preparing next data byte for read
 					 * -----------------------------------------------------------
 					 */
					if(slave->devops->read_ack) {
						slave->devops->read_ack(slave->dev,I2C_ACK);
					}
					serdes->state=I2C_STATE_READ;
					serdes->bitcount=8;
					serdes->slave_was_accessed=1;
				}
			}
			break;
			
		case I2C_STATE_ACK_WRITE:
		case I2C_STATE_NACK_WRITE:
			if(oldscl && !scl) {
				update_t_high(serdes,__LINE__);
				serdes->outpinstate = I2C_SCL; 
				if(serdes->state==I2C_STATE_NACK_WRITE) {
					serdes->outpinstate |= I2C_SDA; 
				}
				break;
			} else if(!oldscl && scl) {
				update_t_low(serdes,__LINE__);
				dbgprintf("goto write state addr %02x\n",serdes->address);
				serdes->state=I2C_STATE_WRITE;
				serdes->bitcount=0;
				serdes->inbuf=0;
				break;
			}
	//		dbgprintf("I2C ack write addr %02x input %02x return %02x\n",serdes->address,pinstate,serdes->outpinstate);
			break;
		
		case I2C_STATE_READ:
			// We change output after falling edge of scl
			//dbgprintf(stderr,"read: bitcount %d, scl %d\n",serdes->bitcount,scl);
			if(oldscl && !scl) {
				I2C_Slave *slave=serdes->active_slave;
				update_t_high(serdes,__LINE__);
				if(serdes->bitcount == 8) {
					int result;
					if(serdes->stretch_scl) {

					}
					result = slave->devops->read(slave->dev,&serdes->outbuf);
					if(result == I2C_STRETCH_SCL) {
						serdes->scl_delayed_proc = delayed_read;
						serdes->stretch_scl++;
						serdes->outpinstate = I2C_SDA;
						break;
					}
				}
				if(serdes->bitcount > 0) {
					serdes->bitcount--;
					serdes->outpinstate=I2C_SCL; // should be delayed
					if((serdes->outbuf&(1<<serdes->bitcount))!=0) {
						serdes->outpinstate|=I2C_SDA;
					} 
				} else {
					fprintf(stderr,"I2C_SerDes Bug: bitcount<=0 should not happen\n");
				}
			} else if(!oldscl && scl) {
				update_t_low(serdes,__LINE__);
				if(serdes->bitcount==0) {
					//dbgprintf(stderr,"goto ack receive\n");
					serdes->state=I2C_STATE_ACK_READ;
				}
			}
			break;

		case I2C_STATE_WRITE:
			if(oldscl && !scl) {
				update_t_high(serdes,__LINE__);
				serdes->outpinstate =I2C_SCL | I2C_SDA;
			} else if(!oldscl && scl) {
				update_t_low(serdes,__LINE__);
				update_t_sudat(serdes,__LINE__);
				serdes->bitcount++;
				serdes->inbuf<<=1;		
				if(sda) {
					serdes->inbuf|=1;
				}			
				if(serdes->bitcount==8) {
					I2C_Slave *slave;
					int result;
					slave = serdes->active_slave;
					if(!slave) {
						fprintf(stderr,"I2C-SerDes Emulator Bug: no slave in write\n");
						exit(3245);
					}
					result = slave->devops->write(slave->dev,serdes->inbuf);	
					serdes->slave_was_accessed=1;
					if(result==I2C_ACK) {
						serdes->state=I2C_STATE_ACK_WRITE;
						serdes->outpinstate |= I2C_SCL;	
					} else if(result == I2C_NACK) {
						serdes->state=I2C_STATE_NACK_WRITE;
						serdes->outpinstate |= I2C_SCL;	
					} else if(result == I2C_STRETCH_SCL) {
						serdes->state=I2C_STATE_ACK_WRITE;
						serdes->stretch_scl++;
						serdes->scl_delayed_proc = delayed_write;
						serdes->outpinstate &= ~I2C_SCL;
						fprintf(stderr,"SCL stretching Not implemented\n");
					} else {
						fprintf(stderr,"Bug: Unknown I2C-Result %d\n",result);
					}
				}
			}	
			break;

		case I2C_STATE_WAIT:
			break;
		default:
			fprintf(stderr,"I2C_SerDes: Bug, no matching handler for state %d\n",serdes->state);
			serdes->outpinstate = I2C_SDA | I2C_SCL;
	}
}

/*
 * -------------------------------------------
 * Update of output signal nodes
 * delayed by raise/fall time
 * -------------------------------------------
 */
static void
update_sda(void *clientData)
{
	I2C_SerDes *serdes = (I2C_SerDes *)clientData;
	if(serdes->outpinstate & I2C_SDA) {
		SigNode_Set(serdes->sda,SIG_OPEN);	
	} else {
		SigNode_Set(serdes->sda,SIG_LOW);	
	}	
}

static void
update_scl(void *clientData)
{
	I2C_SerDes *serdes = (I2C_SerDes *)clientData;
	if(serdes->outpinstate & I2C_SCL) {
		SigNode_Set(serdes->scl,SIG_OPEN);	
	} else {
		SigNode_Set(serdes->scl,SIG_LOW);	
	}	
}

/*
 * -----------------------------------------------
 * Trigger the timer for rise/fall time delayed
 * SDA change
 * -----------------------------------------------
 */
static void
schedule_sda(I2C_SerDes *serdes,int sigval) 
{
	uint32_t nsecs;
	int64_t cycles;
	if(!CycleTimer_IsActive(&serdes->sdaDelayTimer)) {
		if(sigval == SIG_LOW) {
			nsecs = 50;
			cycles = NanosecondsToCycles(nsecs);
		} else {
			nsecs = 100;
			cycles = NanosecondsToCycles(nsecs);
		}
		CycleTimer_Add(&serdes->sdaDelayTimer,cycles,update_sda,serdes);
	}
}

/*
 * -------------------------------------------------------------
 *  Trigger the timer for rise/fall time delayed SCL change
 * -------------------------------------------------------------
 */
static void
schedule_scl(I2C_SerDes *serdes,int sigval) 
{
	uint32_t nsecs;
	int64_t cycles;
	if(!CycleTimer_IsActive(&serdes->sclDelayTimer)) {
		if(sigval == SIG_LOW) {
			nsecs = 50;
			cycles = NanosecondsToCycles(nsecs);
		} else {
			nsecs = 100;
			cycles = NanosecondsToCycles(nsecs);
		}
		CycleTimer_Add(&serdes->sclDelayTimer,cycles,update_scl,serdes);
	}
}

/*
 * ----------------------------------------------------------------------
 * Update the outputs of the SerDes by scheduling SDA and SCL change
 * for change after rise/fall time
 * ----------------------------------------------------------------------
 */
static void
schedule_outputs(I2C_SerDes *serdes) 
{
	int newsda,newscl;
	int outpinstate = serdes->outpinstate;
	if(!(outpinstate & I2C_SDA)) {
		newsda = SIG_LOW;
	} else {
		newsda = SIG_OPEN;
	}
	if(newsda != SigNode_State(serdes->sda)) {
		schedule_sda(serdes,newsda);
	}
	if(!(outpinstate & I2C_SCL)) {
		newscl = SIG_LOW;
	} else {
		newscl = SIG_OPEN;
	}
	if(newscl != SigNode_State(serdes->scl)) {
		schedule_scl(serdes,newscl);
	}

}
/*
 * -------------------------------------------------------------------
 * This trace of the signal node is invoked whenever SDA line changes 
 * -------------------------------------------------------------------
 */
static void 
I2C_SdaChange(SigNode *node,int value,void *clientData) 
{
	I2C_SerDes *serdes = clientData;
	int pinstate = serdes->oldpinstate;
	if((value == SIG_HIGH)) {
		dbgprintf("feed SDA_H on %s\n",SigName(node));
		pinstate = pinstate | I2C_SDA;	
	} else if (value == SIG_LOW) {
		pinstate = pinstate & ~I2C_SDA;	
	} else {
		fprintf(stderr,"I2C-SDA state %d is illegal\n",value);
	}
	I2C_SerDesFeed(serdes,pinstate); 
	serdes->sda_change_time = CycleCounter_Get();
	schedule_outputs(serdes);
	serdes->oldpinstate = pinstate;
}

static void 
I2C_SclChange(SigNode *node,int value,void *clientData) 
{
	I2C_SerDes *serdes = clientData;
	int pinstate = serdes->oldpinstate;
	if((value == SIG_HIGH)) {
		pinstate = pinstate | I2C_SCL;	
		dbgprintf("feed SCL_H on %s\n",SigName(node));
	} else if (value == SIG_LOW) {
		pinstate = pinstate & ~I2C_SCL;	
	} else {
		fprintf(stderr,"I2C-SCL state %d is illegal\n",value);
	}
	I2C_SerDesFeed(serdes,pinstate); 
	serdes->scl_change_time = CycleCounter_Get();
	schedule_outputs(serdes);
	serdes->oldpinstate = pinstate;
}

void
SerDes_UnstretchScl(I2C_SerDes *serdes) 
{
	if(!serdes->stretch_scl) {
		fprintf(stderr,"I2C Bug:Trying to unstretch nonstretched SCL\n");
		return;
	}
	fprintf(stderr,"Unstretch\n");
	serdes->stretch_scl = 0;
	if(serdes->scl_delayed_proc) {
		serdes->scl_delayed_proc(serdes);
		serdes->scl_delayed_proc = NULL;
	} else {
		serdes->outpinstate |= I2C_SCL;
	}
	schedule_outputs(serdes);
}

/*
 * -------------------------------------------------------------
 * Create a new Serializer/Deserializer with Pullup resistors
 * And trace the SDA/SCL lines
 * -------------------------------------------------------------
 */
I2C_SerDes *
I2C_SerDesNew(const char *name) 
{
	I2C_SerDes *serdes = sg_new(I2C_SerDes);
	serdes->scl = SigNode_New("%s.scl",name);
	if(!serdes->scl) {
		exit(2763);
	}
	serdes->sclTrace = SigNode_Trace(serdes->scl,I2C_SclChange,serdes);

	serdes->scl_pullup = SigNode_New("%s.scl_pullup",name);
	if(!serdes->scl_pullup) {
		exit(4763);
	}
	SigNode_Set(serdes->scl_pullup,SIG_PULLUP);

	serdes->sda = SigNode_New("%s.sda",name);
	if(!serdes->sda) {
		exit(23);
	}
	serdes->sdaTrace = SigNode_Trace(serdes->sda,I2C_SdaChange,serdes); 

	serdes->sda_pullup = SigNode_New("%s.sda_pullup",name);
	if(!serdes->sda_pullup) {
		exit(4763);
	}
	SigNode_Set(serdes->sda_pullup,SIG_PULLUP);

	/* Create the connections to the Pullup Resistors */
	SigName_Link(SigName(serdes->scl),SigName(serdes->scl_pullup));
	SigName_Link(SigName(serdes->sda),SigName(serdes->sda_pullup));
	serdes->oldpinstate = I2C_SDA | I2C_SCL;
	serdes->name = sg_strdup(name);

	fprintf(stderr,"I2C Serializer/Deserializer \"%s\" created\n",name);
	return serdes;
}

void 
I2C_SerDesAddSlave(I2C_SerDes * serdes,I2C_Slave *slave,int addr) 
{
	slave->address=addr;
	slave->addr_mask = ~0;
	slave->next = serdes->slave_list;
	if(slave->speed > slave->tolerated_speed) {
		slave->tolerated_speed=slave->speed;
	}
	serdes->slave_list = slave;	
}

int 
I2C_SerDesDetachSlave(I2C_SerDes *serdes,I2C_Slave *slave) 
{
	I2C_Slave *cursor,*prev;
	if(serdes->active_slave == slave) {
		reset_serdes(serdes);
	}
	for(prev = NULL,cursor = serdes->slave_list;cursor;cursor = cursor->next) {
		if(cursor == slave) {
			if(prev) {
				prev->next = cursor->next;
			} else {
				serdes->slave_list = cursor->next;
			}
			return 1;
		}
	}
	return -1;
}
