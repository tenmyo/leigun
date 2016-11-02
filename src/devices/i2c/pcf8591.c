/*
 *************************************************************************************************
 *
 * Emulation of PCF8591 I2C  A/D Converter
 *
 *  State:
 *	Working, but shows constant values
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
#include "pcf8591.h"
#include "sgstring.h"

#if 0
#define dbgprintf(x...) { fprintf(stderr,x); }
#else
#define dbgprintf(x...)
#endif

#define PCF_STATE_CONTROL (0)
#define PCF_STATE_DA      (2)

struct PCF8591 {
	I2C_Slave i2c_slave;
	uint16_t reg_address;
	int state;
	uint8_t control;
	uint8_t da;
	uint8_t ad;
	uint8_t ain[4];
};

/*
 * ------------------------------------
 * PCF8591 Write state machine 
 * ------------------------------------
 */
static int
pcf8591_write(void *dev, uint8_t data)
{
	PCF8591 *pcf = dev;
	if (pcf->state == PCF_STATE_CONTROL) {
		dbgprintf("PCF8591 Addr 0x%02x\n", data);
		pcf->control = data;
		pcf->state = PCF_STATE_DA;
	} else if (pcf->state == PCF_STATE_DA) {
		dbgprintf("PCF8591 Write 0x%02x to %04x\n", data, pcf->reg_address);
		pcf->da = data;
	}
	return I2C_ACK;
};

#define PCF_AUTO_INC (8)
#define PCF_AIN_MODE 	     (0x30)
#define PCF_AIN_SINGLE_ENDED 	(0)
#define PCF_AIN_THREE_DIFF 	(0x10)
#define PCF_AIN_MIXED 		(0x20)
#define PCF_AIN_TWO_DIFF 	(0x30)
static int
pcf8591_read(void *dev, uint8_t * data)
{
	PCF8591 *pcf = dev;
	*data = pcf->ad;
	if (pcf->control & PCF_AIN_MODE) {
		fprintf(stderr, "Warning. AIN mode %02x not implemented\n", pcf->control);
	}
	pcf->ad = pcf->ain[pcf->control & 3];
	if (pcf->control & PCF_AUTO_INC) {
		pcf->control = (pcf->control & ~3) | ((pcf->control + 1) & 3);
	}
	dbgprintf("PCF8591 read 0x%02x\n", *data);
	return I2C_DONE;
};

static int
pcf8591_start(void *dev, int i2c_addr, int operation)
{
	PCF8591 *pcf = dev;
	dbgprintf("pcf8591 start\n");
	pcf->state = PCF_STATE_CONTROL;
	return I2C_ACK;
}

static void
pcf8591_stop(void *dev)
{
	PCF8591 *pcf = dev;
	dbgprintf("pcf8591 stop\n");
	pcf->state = PCF_STATE_CONTROL;
}

static I2C_SlaveOps pcf8591_ops = {
	.start = pcf8591_start,
	.stop = pcf8591_stop,
	.read = pcf8591_read,
	.write = pcf8591_write
};

I2C_Slave *
PCF8591_New(char *name)
{
	PCF8591 *pcf = sg_new(PCF8591);
	I2C_Slave *i2c_slave;
	pcf->ain[0] = 5000 / 32;
	pcf->ain[1] = 5100 / 32;
	pcf->ain[2] = 5150 / 32;
	pcf->ain[3] = 3300 / 16;
	i2c_slave = &pcf->i2c_slave;
	i2c_slave->devops = &pcf8591_ops;
	i2c_slave->dev = pcf;
	i2c_slave->speed = I2C_SPEED_STD;
	i2c_slave->tolerated_speed = I2C_SPEED_FAST;
	fprintf(stderr, "PCF8591 A/D D/A Converter \"%s\" created\n", name);
	return i2c_slave;
}
