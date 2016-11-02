/*
 *************************************************************************************************
 *
 * Emulation of National Semiconductor LM75 I2C-Temperature Sensor 
 *
 * State: Temperature reading is possible (fixed value). 
 * 	  Everything else doesn't work 
 *
 *	You can crash real LM75 by creating a stop/start condition during
 *	acknowledge of read operation. It will pull SDA to low forever.
 *	This bug is a missing feature of the emulator.
 *
 * Copyright 2004 Jochen Karrer. All rights reserved.
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
#include <sys/time.h>
#include <time.h>
#include "i2c.h"
#include "lm75.h"
#include "sgstring.h"

#if 0
#define dbgprintf(x...) { fprintf(stderr,x); }
#else
#define dbgprintf(x...)
#endif

#define LM75_STATE_POINTER  	(0)
#define LM75_STATE_DATA1  	(1)
#define LM75_STATE_DATA2  	(2)
#define LM75_STATE_PAST_END  	(3)

#define LM75_REG_TEMP		(0)
#define LM75_REG_CONFIG		(1)
#define LM75_REG_THYST		(2)
#define LM75_REG_TOS		(3)

struct LM75 {
	I2C_Slave i2c_slave;
	uint8_t pointer_reg;
	int state;
	uint16_t data;
	uint8_t conf;
	int16_t temp;
	int16_t thyst;
	int16_t tos;
};

/*
 * ------------------------------------
 * LM75 Write state machine 
 * ------------------------------------
 */
static int 
lm75_write(void *dev,uint8_t data) {
	LM75 *lm = dev;
	if(lm->state==LM75_STATE_POINTER) {
		dbgprintf("LM75 Addr 0x%02x\n",data);
		lm->pointer_reg = data & 0x3;
		lm->state = LM75_STATE_DATA1;
	} else if(lm->state==LM75_STATE_DATA1) {
		dbgprintf("LM75 Write 0x%02x to %04x\n",data,lm->pointer_reg);
		if(lm->pointer_reg == LM75_REG_CONFIG) {
			lm->conf = data;
		} else {
			lm->data=data<<8;
			lm->state = LM75_STATE_DATA2;
		}
	} else if(lm->state==LM75_STATE_DATA2) {
		lm->data |= data;
		switch(lm->pointer_reg) {
			case LM75_REG_TEMP:
				return I2C_NACK;
			case LM75_REG_THYST:
				lm->thyst=data;
				break;
			case LM75_REG_TOS:
				lm->tos=data;
				break;
		}
	}
	return I2C_ACK;
};

static int 
lm75_read(void *dev,uint8_t *data) 
{
	LM75 *lm = dev;
	dbgprintf("LM75 read 0x%02x from %04x\n",*data,lm->pointer_reg);
	if(lm->state==LM75_STATE_DATA1) {
		switch(lm->pointer_reg) {
			case LM75_REG_CONFIG:
				*data=lm->conf;
				return I2C_DONE;
			case LM75_REG_TEMP:
				lm->data = lm->temp;
				break;
			case LM75_REG_THYST:
				lm->data = lm->thyst;
				break;
			case LM75_REG_TOS:
				lm->data = lm->tos;
				break;
		}
		lm->state = LM75_STATE_DATA2;
		*data = lm->data >> 8;
	} else if(lm->state==LM75_STATE_DATA2) {
		*data = lm->data; 
		lm->state = LM75_STATE_PAST_END;
	} else if(lm->state==LM75_STATE_PAST_END) {
		*data = 0xff;
	}
	return I2C_DONE;
};

static int
lm75_start(void *dev,int i2c_addr,int operation) {
	LM75 *lm = dev;
	dbgprintf("LM75 start\n");
	if(operation == I2C_READ) {
		lm->state = LM75_STATE_DATA1;
	} else if (operation == I2C_WRITE) {
		lm->state = LM75_STATE_POINTER;
	}
	return I2C_ACK;
}

static void 
lm75_stop(void *dev) {
	LM75 *lm = dev;
	dbgprintf("LM75 stop\n");
	lm->state =  LM75_STATE_POINTER; 
}


static I2C_SlaveOps lm75_ops = {
	.start = lm75_start,
	.stop =  lm75_stop,
	.read =  lm75_read,	
	.write = lm75_write	
};

I2C_Slave *
LM75_New(char *name) {
	LM75 *lm = sg_new(LM75); 
	I2C_Slave *i2c_slave;
	lm->temp = 20 << 8;
	i2c_slave = &lm->i2c_slave;
	i2c_slave->devops = &lm75_ops; 
	i2c_slave->dev = lm;
	i2c_slave->speed = I2C_SPEED_FAST;
	fprintf(stderr,"LM75 Temperature Sensor \"%s\" created\n",name);
	return i2c_slave;
}
