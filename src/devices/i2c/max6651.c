/*
 *************************************************************************************************
 *
 * Emulation of MAX6651 Fan Controller 
 *
 * State: basically working, but FAN-Speeds are constants. GPIO 
 *        emulation is not complete
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
#include "max6651.h"
#include "sgstring.h"

#if 0
#define dbgprintf(x...) { fprintf(stderr,x); }
#else
#define dbgprintf(x...)
#endif

#define MAX_STATE_ADDR (1)
#define MAX_STATE_DATA (2)
#define	REG_SPEED	(0)
#define REG_CONFIG	(2)
#define REG_GPIO_DEF	(4)
#define REG_DAC		(6)
#define REG_ALARM_ENA	(8)
#define		ALARM_ENA_GPIO2		(1<<4)
#define		ALARM_ENA_GPIO1		(1<<3)
#define		ALARM_ENA_TACH		(1<<2)
#define		ALARM_ENA_MIN		(1<<1)
#define		ALARM_ENA_MAX		(1<<0)
#define REG_ALARM_STAT	(10)
#define		ALARM_STAT_GPIO2	(1<<4)
#define		ALARM_STAT_GPIO1	(1<<3)
#define		ALARM_STAT_TACH		(1<<2)
#define		ALARM_STAT_MIN		(1<<1)
#define		ALARM_STAT_MAX		(1<<0)
#define REG_TACH0	(12)
#define REG_TACH1	(14)
#define REG_TACH2	(16)
#define REG_TACH3	(18)
#define REG_GPIO_STAT	(20)
#define REG_COUNT	(22)

struct MAX6651 {
	I2C_Slave i2c_slave;
	int state;
	int reg_addr;

	uint8_t speed;
	uint8_t config;
	uint8_t gpio_def;
	uint8_t dac;
	uint8_t alarm_enable;
	uint8_t alarm;
	uint32_t tach[4];
	uint8_t gpio_stat;
	uint8_t count;
};

/*
 * ------------------------------------
 * MAX6651 Write state machine 
 * ------------------------------------
 */
static int 
max6651_write(void *dev,uint8_t data) {
	MAX6651 *max = dev;
	if(max->state==MAX_STATE_ADDR) {
		dbgprintf("MAX6651 Addr 0x%02x\n",data);
		max->reg_addr= data;
		max->state = MAX_STATE_DATA;
	} else if(max->state==MAX_STATE_DATA) {
		dbgprintf("MAX6651 Write 0x%02x to %04x\n",data,max->reg_addr);
		switch(max->reg_addr) {
			case REG_SPEED:
				max->speed=data;
				break;
			case REG_CONFIG:
				max->config = data;
				break;
			case REG_GPIO_DEF:
				max->gpio_def = data;
				break;
			case REG_DAC:
				max->dac = data;
				break;
			case REG_ALARM_ENA:
				max->alarm_enable=data;
				break;
			case REG_COUNT:
				max->count = data;
				break;
			default:
				fprintf(stderr,"MAX6651 write to nonexisting register\n");
				return I2C_NACK; /* does the real device the same thing ? */
		} 
	}
	return I2C_ACK;
};

static void
update_fanspeed(MAX6651 *max,unsigned int fan) 
{
	struct timeval tv;
	if(fan>3) {
		return;
	}
	gettimeofday(&tv,NULL);
	max->tach[fan] = 170 + (tv.tv_usec & 0xf); 	
	
}
static int 
max6651_read(void *dev,uint8_t *data) 
{
	MAX6651 *max = dev;
	switch(max->reg_addr) {
			case REG_SPEED:
				*data=max->speed;
				break;
			case REG_CONFIG:
				*data=max->config;
				break;
			case REG_GPIO_DEF:
				*data=max->gpio_def;
				break;
			case REG_DAC:
				*data=max->dac;
				break;

			case REG_ALARM_ENA:
				*data=max->alarm_enable;
				break;

			case REG_ALARM_STAT:
				*data=max->alarm;
				break;

			case REG_TACH0:
				update_fanspeed(max,0);
				*data=(max->tach[0]<<1)>>(3-max->count);
				break;

			case REG_TACH1:
				update_fanspeed(max,1);
				*data=(max->tach[1]<<1)>>(3-max->count);
				break;

			case REG_TACH2:
				update_fanspeed(max,2);
				*data=(max->tach[2]<<1)>>(3-max->count);
				break;

			case REG_TACH3:
				update_fanspeed(max,3);
				*data=(max->tach[3]<<1)>>(3-max->count);
				break;

			case REG_GPIO_STAT:
				*data=max->gpio_stat;
				break;

			case REG_COUNT:
				*data=max->count;
				break;
			default:
				*data=0;
	}
	dbgprintf("MAX6651 read 0x%02x from %04x\n",*data,max->reg_addr);
	return I2C_DONE;
};

static int
max6651_start(void *dev,int i2c_addr,int operation) {
	MAX6651 *max = dev;
	dbgprintf("MAX6651 start\n");
	max->state = MAX_STATE_ADDR;
	return I2C_ACK;
}

static void 
max6651_stop(void *dev) {
	MAX6651 *max = dev;
	dbgprintf("MAX6651 stop\n");
	max->state =  MAX_STATE_ADDR; 
}


static I2C_SlaveOps max6651_ops = {
	.start = max6651_start,
	.stop =  max6651_stop,
	.read =  max6651_read,	
	.write = max6651_write	
};

I2C_Slave *
MAX6651_New(char *name) {
	MAX6651 *max = sg_new(MAX6651); 
	I2C_Slave *i2c_slave;
	max->config=0xa;
	max->gpio_def=0xff;
	max->gpio_stat=0x1f;
	max->tach[0]=0;
	max->tach[1]=0;
	max->tach[2]=0;
	max->tach[3]=0;
	max->count=0x02;
	i2c_slave = &max->i2c_slave;
	i2c_slave->devops = &max6651_ops; 
	i2c_slave->dev = max;
	i2c_slave->speed = I2C_SPEED_FAST;
	fprintf(stderr,"MAX6652 FAN-Controller \"%s\" created\n",name);
	return i2c_slave;
}
