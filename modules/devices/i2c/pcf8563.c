/*
 *************************************************************************************************
 *
 * Emulation of PCF8563 I2C-Realtime Clock 
 *
 * State: Time is readable, and writable, many registers are missing 
 * 	  The difference between the rtc-clock time and the
 *	  seconds since 1970 are stored in a 4 byte file
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
#include "pcf8563.h"
#include "configfile.h"
#include "diskimage.h"
#include "sgstring.h"

#if 0
#define dbgprintf(...) { fprintf(stderr,__VA_ARGS__); }
#else
#define dbgprintf(...)
#endif

#define PCF_STATE_ADDR (0)
#define PCF_STATE_DATA  (2)

struct PCF8563 {
	I2C_Slave i2c_slave;
	uint16_t reg_address;
	int state;
	int direction;		// copy of I2C operation at start condition
	DiskImage *disk_image;
	uint8_t *time_offset;	/* 4 Byte little endian array with offset to system time */
	uint8_t data[16];
};

/*
 * ------------------------------------
 * PCF8563 Write state machine 
 * ------------------------------------
 */
static int
pcf8563_write(void *dev, uint8_t data)
{
	PCF8563 *pcf = dev;
	if (pcf->state == PCF_STATE_ADDR) {
		dbgprintf("PCF8563 Addr 0x%02x\n", data);
		pcf->reg_address = data & 0xf;
		pcf->state = PCF_STATE_DATA;
	} else if (pcf->state == PCF_STATE_DATA) {
		dbgprintf("PCF8563 Write 0x%02x to %04x\n", data, pcf->reg_address);
		pcf->data[pcf->reg_address] = data;
		pcf->reg_address = ((pcf->reg_address + 1) & 0xf) | (pcf->reg_address & ~0xf);
	}
	return I2C_ACK;
};

static int
pcf8563_read(void *dev, uint8_t * data)
{
	PCF8563 *pcf = dev;
	*data = pcf->data[pcf->reg_address];
	dbgprintf("PCF8563 read 0x%02x from %04x\n", *data, pcf->reg_address);
	pcf->reg_address = (pcf->reg_address + 1) & 0xf;
	return I2C_DONE;
};

static inline unsigned char
bcd_to_i(unsigned char b)
{
	return (b & 0xf) + 10 * ((b >> 4) & 0xf);
}

static inline unsigned char
i_to_bcd(unsigned char b)
{
	return (b / 10) * 16 + (b - 10 * (b / 10));
}

#define RTC_SECONDS  2
#define RTC_MINUTES  3
#define RTC_HOURS    4
#define RTC_DAYS     5
#define RTC_WEEKDAYS 6
#define RTC_MONTHS   7
#define RTC_YEARS    8
#define RTC_MONTHS_FLAG_1900 (1<<7)

/*
 * ----------------------------------------------------------------
 * read_systemtime
 * 	Read new time from system and convert to PCF8563 format 
 *	Called on I2C-Start with direction READ	
 * ----------------------------------------------------------------
 */
static void
read_systemtime(PCF8563 * pcf)
{
	struct timeval tv;
	int32_t offset;
	time_t time;
	struct tm tm;
	gettimeofday(&tv, NULL);
	time = tv.tv_sec;
	offset = pcf->time_offset[0] | (pcf->time_offset[1] << 8)
	    | (pcf->time_offset[2] << 16) | (pcf->time_offset[3] << 24);
	time += offset;
	gmtime_r(&time, &tm);
	//fprintf(stderr,"host: tm.hour %d tm.tm_isdst %d\n",tm.tm_hour,tm.tm_isdst);
	pcf->data[RTC_MONTHS] = i_to_bcd(tm.tm_mon + 1);
	pcf->data[RTC_DAYS] = i_to_bcd(tm.tm_mday);
	if (tm.tm_year > 100) {
		pcf->data[RTC_YEARS] = i_to_bcd(tm.tm_year - 100);
	} else {
		pcf->data[RTC_YEARS] = i_to_bcd(tm.tm_year);
		pcf->data[RTC_MONTHS] |= RTC_MONTHS_FLAG_1900;
	}
	pcf->data[RTC_HOURS] = i_to_bcd(tm.tm_hour);
	pcf->data[RTC_MINUTES] = i_to_bcd(tm.tm_min);
	pcf->data[RTC_SECONDS] = i_to_bcd(tm.tm_sec);
}

/*
 */
static int32_t
diff_systime(PCF8563 * pcf)
{
	struct timeval tv;
	char *zone;
	int32_t offset;
	time_t rtc_time;
	time_t sys_utc_time;
	struct tm tm;

	tm.tm_isdst = -1;
	tm.tm_mon = bcd_to_i(pcf->data[RTC_MONTHS] & 0x1f) - 1;
	tm.tm_mday = bcd_to_i(pcf->data[RTC_DAYS] & 0x3f);
	if (pcf->data[RTC_MONTHS] & RTC_MONTHS_FLAG_1900) {
		tm.tm_year = bcd_to_i(pcf->data[RTC_YEARS] & 0xff);
	} else {
		tm.tm_year = bcd_to_i(pcf->data[RTC_YEARS] & 0xff) + 100;
	}
	tm.tm_hour = bcd_to_i(pcf->data[RTC_HOURS] & 0x3f);
	tm.tm_min = bcd_to_i(pcf->data[RTC_MINUTES] & 0x7f);
	tm.tm_sec = bcd_to_i(pcf->data[RTC_SECONDS] & 0x7f);

	/* Shit, there is no mktime which does GMT */
	zone = getenv("TZ");	/* remember original */
	setenv("TZ", "UTC", 1);	/* No timezone is "UTC" */
	tzset();
	rtc_time = mktime(&tm);
	if (zone) {
		setenv("TZ", zone, 1);
	} else {
		unsetenv("TZ");
	}
	tzset();
	if (rtc_time == -1) {
		return 0;
	}

	gettimeofday(&tv, NULL);
	sys_utc_time = tv.tv_sec;

	offset = rtc_time - sys_utc_time;
	dbgprintf("diff %d, sys %d, rtc %d\n", offset, sys_utc_time, rtc_time);
	return offset;
}

static int
pcf8563_start(void *dev, int i2c_addr, int operation)
{
	PCF8563 *pcf = dev;
	dbgprintf("pcf8563 start\n");
	pcf->state = PCF_STATE_ADDR;
	pcf->direction = operation;
	if (operation == I2C_READ) {
		read_systemtime(pcf);
	} else {
		// hack ! should be only written if some registers are touched
		read_systemtime(pcf);
	}
	return I2C_ACK;
}

static void
pcf8563_stop(void *dev)
{
	PCF8563 *pcf = dev;
	dbgprintf("pcf8563 stop\n");
	if (pcf->direction == I2C_WRITE) {
		// hack ! should be only written if some registers are touched
		int32_t offset = diff_systime(pcf);
		pcf->time_offset[0] = offset & 0xff;
		pcf->time_offset[1] = (offset >> 8) & 0xff;
		pcf->time_offset[2] = (offset >> 16) & 0xff;
		pcf->time_offset[3] = (offset >> 24) & 0xff;
	}
	pcf->state = PCF_STATE_ADDR;
}

/*
 * -----------------------------------------------
 * pcf8563_ops 
 *	I2C-Operations provided by the PCF8563
 * -----------------------------------------------
 */

static I2C_SlaveOps pcf8563_ops = {
	.start = pcf8563_start,
	.stop = pcf8563_stop,
	.read = pcf8563_read,
	.write = pcf8563_write
};

I2C_Slave *
PCF8563_New(char *name)
{
	PCF8563 *pcf = sg_new(PCF8563);
	char *dirname, *imagename;

	I2C_Slave *i2c_slave;

	dirname = Config_ReadVar("global", "imagedir");
	if (dirname) {
		imagename = alloca(strlen(dirname) + strlen(name) + 20);
		sprintf(imagename, "%s/%s.img", dirname, name);
		pcf->disk_image = DiskImage_Open(imagename, 4, DI_RDWR | DI_CREAT_00);
		if (!pcf->disk_image) {
			fprintf(stderr, "Failed to open PCF8563 time offset file\n");
			pcf->time_offset = sg_calloc(4);
		} else {
			pcf->time_offset = DiskImage_Mmap(pcf->disk_image);
		}
	} else {
		pcf->time_offset = sg_calloc(4);
	}
	i2c_slave = &pcf->i2c_slave;
	i2c_slave->devops = &pcf8563_ops;
	i2c_slave->dev = pcf;
	i2c_slave->speed = I2C_SPEED_FAST;
	read_systemtime(pcf);
	fprintf(stderr, "PCF8563 Real Time Clock \"%s\" timeoffset %d sec\n", name,
		diff_systime(pcf));
	return i2c_slave;
}
