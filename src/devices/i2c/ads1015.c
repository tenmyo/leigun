/*
 *************************************************************************************************
 *
 * Emulation of ADS1015 I2C  A/D Converter
 *
 *  State:
 *	
 *
 * Copyright 2004 2011 Jochen Karrer. All rights reserved.
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
#include "ads1015.h"
#include "sgstring.h"
#include "cycletimer.h"

#if 0
#define dbgprintf(x...) { fprintf(stderr,x); }
#else
#define dbgprintf(x...)
#endif

#define CONF_COMP_QUE0	(1 << 0)
#define CONF_COMP_QUE1	(1 << 1)
#define CONF_COMP_LAT	(1 << 2)
#define CONF_COMP_POL	(1 << 3)
#define CONF_COMP_MODE	(1 << 4)
#define CONF_DR0	(1 << 5)
#define CONF_DR1	(1 << 6)
#define CONF_DR2	(1 << 7)
#define CONF_DR_MSK	(7 << 5)
#define   DR_128SPS	(0 << 5)
#define	  DR_250SPS	(1 << 5)
#define   DR_490SPS	(2 << 5)
#define	  DR_920SPS	(3 << 5)
#define   DR_1600SPS	(4 << 5)
#define   DR_2400SPS	(5 << 5)
#define   DR_3300SPS	(6 << 5)
#define   DR_3300SPS_2  (7 << 5)
#define CONF_MODE	(1 << 8)
#define CONF_PGA0	(1 << 9)
#define CONF_PGA1	(1 << 10)
#define CONF_PGA2	(1 << 11)
#define CONF_PGA_MSK	(7 << 9)
#define		PGA_6144	(0 << 9)
#define		PGA_4096	(1 << 9)
#define		PGA_2048	(2 << 9)
#define		PGA_1024	(3 << 9)
#define		PGA_512		(4 << 9)
#define		PGA_256_1	(5 << 9)
#define		PGA_256_2	(6 << 9)
#define		PGA_256_3	(7 << 9)
#define CONF_MUX_SHIFT	(12)
#define CONF_MUX_MSK	(7 << 12)
#define CONF_MUX0	(1 << 12)
#define CONF_MUX1	(1 << 13)
#define CONF_MUX2	(1 << 14)
#define CONF_OS		(1 << 15)

#define ADS_STATE_DATA0		(0)
#define ADS_STATE_DATA1		(1)
#define ADS_STATE_POINTER	(2)

#define REG_CONV	(0)
#define REG_CONFIG	(1)
#define REG_LOTHRESH	(2)
#define REG_HITHRESH	(3)

struct ADS1015 {
	I2C_Slave i2c_slave;
	CycleTimer conversionTimer;
	int state;
	uint8_t regPointer;
	uint16_t regConv;
	uint16_t regLoThresh;
	uint16_t regHiThresh;
	uint16_t regConf;
	uint16_t assemblyBuf;
	float ain[4];
};

/**
 **************************************************************************
 * \fn static float ads1015_get_full_scale(ADS1015 *ads); 
 * Read the full scale voltage from config register. 
 **************************************************************************
 */
static float
ads1015_get_full_scale(ADS1015 * ads)
{
	uint16_t pga = ads->regConf & CONF_PGA_MSK;
	float volt;
	switch (pga) {
	    case PGA_6144:
		    volt = 6.144;
		    break;
	    case PGA_4096:
		    volt = 4.096;
		    break;
	    case PGA_2048:
		    volt = 2.048;
		    break;
	    case PGA_1024:
		    volt = 1.024;
		    break;
	    case PGA_512:
		    volt = 0.512;
		    break;
	    case PGA_256_1:
	    case PGA_256_2:
	    case PGA_256_3:
	    default:
		    volt = 0.256;
		    break;
	}
	return volt;
}

/**
 ********************************************************
 * \fn CycleCounter_t ads1015_conv_time(ADS1015 *ads) 
 * get the conversion time from Config register.
 ********************************************************
 */
CycleCounter_t
ads1015_conv_time(ADS1015 * ads)
{
	uint16_t dr = ads->regConf & CONF_DR_MSK;
	uint32_t sps;
	uint32_t microseconds;
	switch (dr) {
	    case DR_128SPS:
		    sps = 128;
		    break;
	    case DR_250SPS:
		    sps = 250;
		    break;
	    case DR_490SPS:
		    sps = 490;
		    break;
	    case DR_920SPS:
		    sps = 920;
		    break;
	    case DR_1600SPS:
		    sps = 1600;
		    break;
	    case DR_2400SPS:
		    sps = 2400;
		    break;
	    case DR_3300SPS:
	    case DR_3300SPS_2:
	    default:
		    sps = 3300;
		    break;
		    break;
	}
	microseconds = 1000000 / sps;
	return MicrosecondsToCycles(microseconds);
}

/*
 *********************************************************
 * ADS1015 Write state machine 
 *********************************************************
 */
static int
ads1015_write(void *dev, uint8_t data)
{
	ADS1015 *ads = dev;
	dbgprintf("ADS1015 Addr 0x%02x\n", data);
	switch (ads->state) {
	    case ADS_STATE_POINTER:
		    //fprintf(stderr,"Pointer %d\n",data);
		    ads->regPointer = data;
		    ads->state = ADS_STATE_DATA0;
		    break;
	    case ADS_STATE_DATA0:
		    //fprintf(stderr,"data0 0x%02x\n",data);
		    ads->assemblyBuf = (uint16_t) data << 8;
		    ads->state = ADS_STATE_DATA1;
		    break;
	    case ADS_STATE_DATA1:
		    //fprintf(stderr,"data1 0x%02x\n",data);
		    ads->assemblyBuf |= (uint16_t) data;
		    switch (ads->regPointer) {
			case REG_CONV:
				break;
			case REG_CONFIG:
				ads->regConf = ads->assemblyBuf;
				if (ads->assemblyBuf & CONF_OS) {
					/* Begin conversion */
					CycleTimer_Mod(&ads->conversionTimer,
						       ads1015_conv_time(ads));
					ads->regConf = ads->assemblyBuf & ~CONF_OS;
				}
				break;
			case REG_LOTHRESH:
				ads->regLoThresh = ads->assemblyBuf;
				break;
			case REG_HITHRESH:
				ads->regHiThresh = ads->assemblyBuf;
				break;
			default:
				break;
		    }
		    ads->state = ADS_STATE_DATA0;
	}
	return I2C_ACK;
};

/**
 ********************************************************
 * \fn static uint16_t ADC_Read(ADS1015 *ads) 
 * Read from conversion register
 ********************************************************
 */
static int16_t
ADC_Read(ADS1015 * ads)
{
	struct timeval tv;
	float random;
	int16_t adval;
	float full_scale = ads1015_get_full_scale(ads);
	uint8_t channel = (ads->regConf & CONF_MUX_MSK) >> CONF_MUX_SHIFT;
	gettimeofday(&tv, NULL);
	random = ((tv.tv_usec + (tv.tv_usec >> 8)) & 0x1f) / 1000.;
	float volt;
	//fprintf(stderr,"Read channel %d\n",channel);
	switch (channel) {
	    case 0:
		    volt = ads->ain[0] - ads->ain[1] + random;
		    break;
	    case 1:
		    volt = ads->ain[0] - ads->ain[3] + random;
		    break;
	    case 2:
		    volt = ads->ain[1] - ads->ain[3] + random;
		    break;
	    case 3:
		    volt = ads->ain[2] - ads->ain[3] + random;
		    break;
	    case 4:
		    volt = ads->ain[0] + random;
		    break;
	    case 5:
		    volt = ads->ain[1] + random;
		    break;
	    case 6:
		    volt = ads->ain[2] + random;
		    break;
	    case 7:
		    volt = ads->ain[3] + random;
		    break;
	    default:
		    volt = 0;
		    break;
	}
	/* Check for saturation is missing here */
	if (volt >= full_scale) {
		adval = 0x7ff;
	} else if (volt < -full_scale) {
		adval = 0x800;
	} else {
		adval = volt * 2048.5 / full_scale;
	}
	return adval;
}

/**
 *******************************************************************
 * \fn static void ADS_CompleteConversion(void *eventData);
 * Complete the conversion. Called some time after the 
 * conversion was started by the ads->conversionTimer
 *******************************************************************
 */
static void
ADS_CompleteConversion(void *eventData)
{
	ADS1015 *ads = eventData;
	ads->regConv = ADC_Read(ads) << 4;
	ads->regConf |= CONF_OS;
}

/**
 ************************************************************
 * Read from the ADS1015 I2C slave
 ************************************************************
 */
static int
ads1015_read(void *dev, uint8_t * data)
{
	ADS1015 *ads = dev;
	if (ads->state == ADS_STATE_DATA0) {
		switch (ads->regPointer) {
		    case REG_CONV:
			    //fprintf(stdout,"Reg conv read %lld\n",CycleCounter_Get());
			    ads->assemblyBuf = ads->regConv;
			    break;
		    case REG_CONFIG:
			    ads->assemblyBuf = ads->regConf;
			    break;
		    case REG_LOTHRESH:
			    ads->assemblyBuf = ads->regLoThresh;
			    break;
		    case REG_HITHRESH:
			    ads->assemblyBuf = ads->regHiThresh;
			    break;
		    default:
			    break;
		}
		*data = ads->assemblyBuf >> 8;
		ads->state = ADS_STATE_DATA1;
	} else if (ads->state == ADS_STATE_DATA1) {
		*data = ads->assemblyBuf & 0xff;
		ads->state = ADS_STATE_DATA0;
	}
	dbgprintf("ADS1015 read 0x%02x\n", *data);
	return I2C_DONE;
};

static int
ads1015_start(void *dev, int i2c_addr, int operation)
{
	ADS1015 *ads = dev;
	dbgprintf("ads1015 start\n");
	if (operation == I2C_WRITE) {
		ads->state = ADS_STATE_POINTER;
	} else {
		ads->state = ADS_STATE_DATA0;
	}
	return I2C_ACK;
}

static void
ads1015_stop(void *dev)
{
	dbgprintf("ads1015 stop\n");
}

static I2C_SlaveOps ads1015_ops = {
	.start = ads1015_start,
	.stop = ads1015_stop,
	.read = ads1015_read,
	.write = ads1015_write
};

/**
 ******************************************************************
 * \fn I2C_Slave * ADS1015_New(char *name); 
 * Create a new ADS1015 A/D converter with I2C Interface
 ******************************************************************
 */
I2C_Slave *
ADS1015_New(char *name)
{
	ADS1015 *ads = sg_new(ADS1015);
	I2C_Slave *i2c_slave;
	ads->ain[0] = 2.160;
	ads->ain[1] = 2.092;
	ads->ain[2] = 2.106;
	ads->ain[3] = 2.494;
	i2c_slave = &ads->i2c_slave;
	i2c_slave->devops = &ads1015_ops;
	i2c_slave->dev = ads;
	i2c_slave->speed = I2C_SPEED_FAST;
	CycleTimer_Init(&ads->conversionTimer, ADS_CompleteConversion, ads);
	fprintf(stderr, "ADS1015 12Bit 4-Channel A/D Converter \"%s\" created\n", name);
	return i2c_slave;
}
