/*
 *************************************************************************************************
 *
 * Emulation of PCF8574 8 Bit IO-Expander 
 *
 * State: Untested, might work
 *       
 * Copyright 2005 Jochen Karrer. All rights reserved.
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
#include "pcf8574.h"
#include "sgstring.h"

#if 0
#define dbgprintf(...) { fprintf(stderr,__VA_ARGS__); }
#else
#define dbgprintf(...)
#endif

#define PCF_STATE_DATA1  (0)

struct PCF8574 {
	I2C_Slave i2c_slave;
	int state;
	uint8_t outval;		/* buffer for write */
	SigNode *port[8];
};

static void
set_ports(PCF8574 * pcf)
{
	int i;
	for (i = 0; i < 8; i++) {
		if (!pcf->port[i]) {
			continue;
		}
		if (pcf->outval & (1 << i)) {
			SigNode_Set(pcf->port[i], SIG_PULLUP);
		} else {
			SigNode_Set(pcf->port[i], SIG_LOW);
		}
	}
}

static uint8_t
get_ports(PCF8574 * pcf)
{
	int i;
	uint8_t value = 0;
	for (i = 0; i < 8; i++) {
		int sigval;
		if (!pcf->port[i]) {
			continue;
		}
		sigval = SigNode_Val(pcf->port[i]);
		if ((sigval == SIG_LOW) || (sigval == SIG_PULLDOWN)) {

		} else {
			value = value | (1 << i);
		}
	}
	return value;
}

/*
 * ------------------------------------
 * PCF8574 Write state machine 
 * ------------------------------------
 */
static int
pcf8574_write(void *dev, uint8_t data)
{
	PCF8574 *pcf = dev;
	switch (pcf->state) {
	    case PCF_STATE_DATA1:
		    dbgprintf("PCF8574 Write 0x%02x to %04x\n", data, pcf->reg_address);
		    pcf->outval = data;
		    pcf->state = PCF_STATE_DATA1;
		    set_ports(pcf);
		    break;
	}
	return I2C_ACK;
};

static int
pcf8574_read(void *dev, uint8_t * data)
{
	PCF8574 *pcf = dev;
	switch (pcf->state) {
	    case PCF_STATE_DATA1:
		    *data = get_ports(pcf);
		    pcf->state = PCF_STATE_DATA1;
		    break;
	}
	return I2C_DONE;
};

static int
pcf8574_start(void *dev, int i2c_addr, int operation)
{
	PCF8574 *pcf = dev;
	dbgprintf("pcf8574 start\n");
	pcf->state = PCF_STATE_DATA1;
	return I2C_ACK;
}

static void
pcf8574_stop(void *dev)
{
	PCF8574 *pcf = dev;
	dbgprintf("pcf8574 stop\n");
	pcf->state = PCF_STATE_DATA1;
}

static I2C_SlaveOps pcf8574_ops = {
	.start = pcf8574_start,
	.stop = pcf8574_stop,
	.read = pcf8574_read,
	.write = pcf8574_write
};

I2C_Slave *
PCF8574_New(char *name)
{
	PCF8574 *pcf = sg_new(PCF8574);
	I2C_Slave *i2c_slave;
	int i;
	i2c_slave = &pcf->i2c_slave;
	i2c_slave->devops = &pcf8574_ops;
	i2c_slave->dev = pcf;
	i2c_slave->speed = I2C_SPEED_FAST;
	for (i = 0; i < 8; i++) {
		pcf->port[i] = SigNode_New("%s.P%02d", name, i);
		if (!pcf->port[i]) {
			fprintf(stderr, "PCF8574: Can not create Signal node \n");
			sleep(1);
		}
	}
	pcf->outval = 0xff;
	set_ports(pcf);
	fprintf(stderr, "PCF8574 8 Bit IO-Expander \"%s\" created\n", name);
	return i2c_slave;
}
