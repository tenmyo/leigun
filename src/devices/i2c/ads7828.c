/*
 *************************************************************************************************
 *
 * Emulation of ADS7828 I2C  A/D Converter
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
#include "ads7828.h"
#include "sgstring.h"

#if 0
#define dbgprintf(...) { fprintf(stderr,__VA_ARGS__); }
#else
#define dbgprintf(...)
#endif

struct ADS7828 {
	I2C_Slave i2c_slave;
	uint16_t reg_address;
	int state;
	uint8_t cmd;
	uint16_t ad;
	uint16_t ain[8];
};

#define ADS_STATE_DATA0 (0)
#define ADS_STATE_DATA1 (1)
/*
 * ------------------------------------
 * ADS7828 Write state machine 
 * ------------------------------------
 */
static int
ads7828_write(void *dev, uint8_t data)
{
	ADS7828 *ads = dev;
	dbgprintf("ADS7828 Addr 0x%02x\n", data);
	ads->cmd = data;
	return I2C_ACK;
};

#define PCF_AUTO_INC (8)
#define PCF_AIN_MODE 	     (0x30)
#define PCF_AIN_SINGLE_ENDED 	(0)
#define PCF_AIN_THREE_DIFF 	(0x10)
#define PCF_AIN_MIXED 		(0x20)
#define PCF_AIN_TWO_DIFF 	(0x30)
static int
ads7828_read(void *dev, uint8_t * data)
{
	ADS7828 *ads = dev;
	struct timeval tv;
	int chsel = (ads->cmd & 0xf0) >> 4;
	int pd = (ads->cmd & 0xc) >> 2;
	int random;
	if (ads->state == ADS_STATE_DATA0) {
		if (pd != 3) {
			fprintf(stderr, "ADS7828 Power Down mode %d not implemented\n", pd);
		}
		gettimeofday(&tv, NULL);
		random = tv.tv_usec & 0xff;
		switch (chsel) {
		    case 0:
			    ads->ad = ads->ain[0] - ads->ain[1] + random;
			    break;
		    case 1:
			    ads->ad = ads->ain[2] - ads->ain[3] + random;
			    break;
		    case 2:
			    ads->ad = ads->ain[4] - ads->ain[5] + random;
			    break;
		    case 3:
			    ads->ad = ads->ain[6] - ads->ain[7] + random;
			    break;
		    case 4:
			    ads->ad = ads->ain[1] - ads->ain[0] + random;
			    break;
		    case 5:
			    ads->ad = ads->ain[3] - ads->ain[2] + random;
			    break;
		    case 6:
			    ads->ad = ads->ain[5] - ads->ain[4] + random;
			    break;
		    case 7:
			    ads->ad = ads->ain[7] - ads->ain[6] + random;
			    break;
		    case 8:
			    ads->ad = ads->ain[0] + random;
			    break;
		    case 9:
			    ads->ad = ads->ain[2] + random;
			    break;
		    case 10:
			    ads->ad = ads->ain[4] + random;
			    break;
		    case 11:
			    ads->ad = ads->ain[6] + random;
			    break;
		    case 12:
			    ads->ad = ads->ain[1] + random;
			    break;
		    case 13:
			    ads->ad = ads->ain[3] + random;
			    break;
		    case 14:
			    ads->ad = ads->ain[5] + random;
			    break;
		    case 15:
			    ads->ad = ads->ain[7] + random;
			    break;
		}
		*data = (ads->ad >> 8) & 0xf;
		ads->state = ADS_STATE_DATA1;
	} else if (ads->state == ADS_STATE_DATA1) {
		*data = ads->ad & 0xff;
		ads->state = ADS_STATE_DATA0;
	}
	dbgprintf("ADS7828 read 0x%02x\n", *data);
	return I2C_DONE;
};

static int
ads7828_start(void *dev, int i2c_addr, int operation)
{
	ADS7828 *ads = dev;
	dbgprintf("ads7828 start\n");
	ads->state = ADS_STATE_DATA0;
	return I2C_ACK;
}

static void
ads7828_stop(void *dev)
{
	ADS7828 *ads = dev;
	dbgprintf("ads7828 stop\n");
	ads->state = ADS_STATE_DATA0;
}

static I2C_SlaveOps ads7828_ops = {
	.start = ads7828_start,
	.stop = ads7828_stop,
	.read = ads7828_read,
	.write = ads7828_write
};

I2C_Slave *
ADS7828_New(char *name)
{
	ADS7828 *ads = sg_new(ADS7828);
	I2C_Slave *i2c_slave;
	ads->ain[0] = 3277;	// 5
	ads->ain[1] = 1650;
	ads->ain[2] = 789;
	ads->ain[3] = 1234;
	ads->ain[4] = 3604;	// 3v3
	ads->ain[5] = 2334;
	ads->ain[6] = 2534;
	ads->ain[7] = 2500;
	i2c_slave = &ads->i2c_slave;
	i2c_slave->devops = &ads7828_ops;
	i2c_slave->dev = ads;
	i2c_slave->speed = I2C_SPEED_FAST;
	fprintf(stderr, "ADS7828 12Bit 8-Channel A/D Converter \"%s\" created\n", name);
	return i2c_slave;
}
