/*
 *************************************************************************************************
 *
 * Emulation of PCF8575 16 Bit IO-Expander 
 *
 * State: Untested, might work
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
#include <unistd.h>
#include "i2c.h"
#include "signode.h"
#include "sgstring.h"
#include "pcf8575.h"

#if 0
#define dbgprintf(x...) { fprintf(stderr,x); }
#else
#define dbgprintf(x...)
#endif

#define PCF_STATE_DATA1  (0)
#define PCF_STATE_DATA2  (1)

struct PCF8575 {
	I2C_Slave i2c_slave;
	int state;
	uint16_t outval; /* buffer for write */
	SigNode *port[16];
};

static void 
set_ports(PCF8575 *pcf)  {
	int i;
	for(i=0;i<16;i++) {
		if(!pcf->port[i]) {
			continue;
		}
		if(pcf->outval&(1<<i)) {
			SigNode_Set(pcf->port[i],SIG_PULLUP);	
		} else {
			SigNode_Set(pcf->port[i],SIG_LOW);	
		}
	}
}

static uint8_t 
get_ports(PCF8575 *pcf,int byte_nr)  {
	int i,first_node;
	uint8_t value = 0;
	if(byte_nr==0) {
		first_node = 0;
	} else {
		first_node = 8;
	}
	for(i=0;i<8;i++) {
		int sigval;
		if(!pcf->port[i+first_node]) {
			continue;
		}
		sigval = SigNode_Val(pcf->port[i+first_node]);	
		if((sigval == SIG_LOW) || (sigval == SIG_PULLDOWN)) {
				
		} else {
			value = value | (1<<i);
		}
	}
	return value;
}
/*
 * ------------------------------------
 * PCF8575 Write state machine 
 * ------------------------------------
 */
static int 
pcf8575_write(void *dev,uint8_t data) {
	PCF8575 *pcf = dev;
	switch(pcf->state) {
		case PCF_STATE_DATA1:
			dbgprintf("PCF8575 Write 0x%02x to %04x\n",data,pcf->reg_address);
			pcf->outval = data;
			pcf->state = PCF_STATE_DATA2;
			break;
		case PCF_STATE_DATA2:
			pcf->outval |= (data<<8);
			set_ports(pcf);
			pcf->state = PCF_STATE_DATA1;
			break;
	}
	return I2C_ACK;
};

static int 
pcf8575_read(void *dev,uint8_t *data) 
{
	PCF8575 *pcf = dev;
	switch(pcf->state) {
		case PCF_STATE_DATA1:
			*data = get_ports(pcf,0);
			pcf->state = PCF_STATE_DATA2;
			break;
		case PCF_STATE_DATA2:
			*data=get_ports(pcf,1);
			pcf->state = PCF_STATE_DATA1;
			break;
	}
	return I2C_DONE;
};

static int 
pcf8575_start(void *dev,int i2c_addr,int operation) {
	PCF8575 *pcf = dev;
	dbgprintf("pcf8575 start\n");
	pcf->state = PCF_STATE_DATA1;
	return I2C_ACK;
}

static void 
pcf8575_stop(void *dev) {
	PCF8575 *pcf = dev;
	dbgprintf("pcf8575 stop\n");
	pcf->state =  PCF_STATE_DATA1; 
}

static I2C_SlaveOps pcf8575_ops = {
	.start = pcf8575_start,
	.stop =  pcf8575_stop,
	.read =  pcf8575_read,	
	.write = pcf8575_write	
};

I2C_Slave *
PCF8575_New(char *name) {
	char *nodename = alloca(strlen(name)+20);
	PCF8575 *pcf = sg_new(PCF8575); 
	I2C_Slave *i2c_slave;
	int i;
	i2c_slave = &pcf->i2c_slave;
	i2c_slave->devops = &pcf8575_ops; 
	i2c_slave->dev = pcf;
	i2c_slave->speed = I2C_SPEED_FAST;
	for(i=0;i<16;i++) {
		if(i>=8) {
			sprintf(nodename,"%s.P%02d",name,i+2);
		} else {
			sprintf(nodename,"%s.P%02d",name,i);
		}
		pcf->port[i]=SigNode_New("%s",nodename);
		if(!pcf->port[i]) {
			fprintf(stderr,"PCF8575: Can not create Signal node %s",nodename);
			sleep(1);
		}
	}
	pcf->outval = 0xffff;
	set_ports(pcf);
	fprintf(stderr,"PCF8575 16 Bit IO-Expander \"%s\" created\n",name);
	return i2c_slave;
}
