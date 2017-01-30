/*
 *************************************************************************************************
 *
 * Emulation of DS1337 I2C-Realtime Clock (I2C address 0x68)
 *
 * State: untested, alarms and eosc missing
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
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include "i2c.h"
#include "ds1337.h"
#include "configfile.h"
#include "diskimage.h"
#include "sgstring.h"

#if 0
#define dbgprintf(...) { fprintf(stderr,__VA_ARGS__); }
#else
#define dbgprintf(...)
#endif

#define DS_STATE_ADDR (0)
#define DS_STATE_DATA  (2)

#define REG_SECONDS		(0)
#define REG_MINUTES		(1)
#define REG_HOUR		(2)
#define         HOURS_12H       (1<<6)
#define         HOURS_PM        (1<<5)
#define REG_DAY			(3)
#define	REG_DATE		(4)
#define REG_MONTH		(5)
#define REG_YEAR		(6)
#define REG_ALARM1_SECONDS	(7)
#define REG_ALARM1_MINUTES	(8)
#define REG_ALARM1_HOUR		(9)
#define REG_ALARM1_DAYDATE	(10)
#define REG_ALARM2_MINUTES	(11)
#define REG_ALARM2_HOUR		(12)
#define REG_ALARM2_DAYDATE	(13)
#define REG_CONTROL		(14)
#define		CONTROL_NEOSC	(1<<7)
#define		CONTROL_RS2	(1<<4)
#define		CONTROL_RS1	(1<<3)
#define		CONTROL_INTCN	(1<<2)
#define		CONTROL_A2IE	(1<<1)
#define		CONTROL_A1IE	(1<<0)
#define REG_STATUS		(15)

#define		STATUS_A1F	(1<<0)
#define		STATUS_A2F	(1<<1)
#define		STATUS_OSF	(1<<7)

#define DS1337_IMAGESIZE (128)

struct DS1337 {
	I2C_Slave i2c_slave;
	uint16_t reg_address;
	int state;
	int direction;		// copy of I2C operation at start condition

	uint32_t useconds;	/* internal only */
	/* The registers */
	uint8_t seconds;
	uint8_t minutes;
	uint8_t hours;
	uint8_t day;
	uint8_t date;
	uint8_t month;
	uint8_t year;
	uint8_t seconds_alarm[2];
	uint8_t minutes_alarm[2];
	uint8_t hours_alarm[2];
	uint8_t daydate_alarm[2];
	uint8_t control_reg;
	uint8_t status_reg;

	DiskImage *disk_image;
	int64_t time_offset;	/* time difference to host clock */
	int time_changed_flag;
};

/*
 * -------------------------------------------------
 * Conversion between BCD and 8Bit Integer
 * -------------------------------------------------
 */
static inline uint8_t
bcd_to_i(uint8_t b)
{
	return (b & 0xf) + 10 * ((b >> 4) & 0xf);
}

static inline uint8_t
i_to_bcd(uint8_t b)
{
	return (b / 10) * 16 + (b - 10 * (b / 10));
}

/*
 * ---------------------------------------------------------
 * return the difference to system time in microseconds
 * ---------------------------------------------------------
 */
static int64_t
diff_systime(DS1337 * ds)
{
	struct timeval now;
	char *zone;
	int64_t offset;
	time_t rtc_time;
	time_t sys_utc_time;
	struct tm tm;
	tm.tm_isdst = -1;
	tm.tm_mon = bcd_to_i(ds->month) - 1;
	tm.tm_mday = bcd_to_i(ds->date);
	tm.tm_year = bcd_to_i(ds->year) + 100;
	if (ds->hours & HOURS_12H) {
		uint8_t hour = bcd_to_i(ds->hours & 0x1f) % 12;
		if (ds->hours & HOURS_PM) {
			tm.tm_hour = hour + 12;
		} else {
			tm.tm_hour = hour;
		}
	} else {
		tm.tm_hour = bcd_to_i(ds->hours & 0x3f);
	}
	tm.tm_min = bcd_to_i(ds->minutes);
	tm.tm_sec = bcd_to_i(ds->seconds);
	if (tm.tm_sec >= 60) {
		tm.tm_sec = 0;
	}
	if (tm.tm_min >= 60) {
		tm.tm_min = 0;
	}
	if (tm.tm_mon >= 12) {
		tm.tm_mon = 0;
	}
	if ((tm.tm_mday >= 32) || (tm.tm_mday < 1)) {
		tm.tm_mday = 1;
	}
	if ((tm.tm_year >= 200) || (tm.tm_year < 100)) {
		tm.tm_year = 100;
	}
	/* Shit, there is no mktime which does GMT */
	zone = getenv("TZ");	/* remember original */
	setenv("TZ", "UTC", 1);	/* "UTC" */
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

	gettimeofday(&now, NULL);
	sys_utc_time = now.tv_sec;

	offset = ((int64_t) rtc_time - (int64_t) sys_utc_time) * (int64_t) 1000000;
	offset += ds->useconds;
	offset -= now.tv_usec;
	//fprintf(stderr,"rtc %ld, sys %ld, calculated offset %lld\n",rtc_time,sys_utc_time,offset);
	return offset;
}

/*
 * ------------------------------------------------------------------
 * update_time 
 * 	Read new time from system, correct it with the offset 
 *	and put it to the DS1337 registers
 *	Called on I2C-Start with direction READ	
 * ------------------------------------------------------------------
 */
static void
update_time(DS1337 * ds)
{
	struct timeval host_tv;
	time_t time;
	struct tm tm;
	if (ds->control_reg & CONTROL_NEOSC) {
		return;
	}
	gettimeofday(&host_tv, NULL);
	time = host_tv.tv_sec;
	time += (ds->time_offset / (int64_t) 1000000);
	ds->useconds = host_tv.tv_usec + (ds->time_offset % 1000000);
	if (ds->useconds > 1000000) {
		time++;
		ds->useconds -= 1000000;
	} else if (ds->useconds < 0) {
		fprintf(stderr, "Modulo was negative Happened\n");
		time--;
		ds->useconds += 1000000;
	}
	gmtime_r(&time, &tm);
	if ((tm.tm_year < 100) || (tm.tm_year >= 200)) {
		fprintf(stderr, "DS1337: Illegal year %d\n", tm.tm_year + 1900);
	}
	ds->year = i_to_bcd((tm.tm_year - 100) % 100);
	ds->month = i_to_bcd(tm.tm_mon + 1);
	ds->day = i_to_bcd(tm.tm_wday + 1);
	ds->date = i_to_bcd(tm.tm_mday);
	if (ds->hours & HOURS_12H) {
		int hour;
		int pm = 0;
		if (tm.tm_hour == 0) {
			hour = 12;
			pm = 0;
		} else if (tm.tm_hour < 12) {
			hour = tm.tm_hour;
			pm = 0;
		} else if (tm.tm_hour < 13) {
			hour = 12;
			pm = 1;
		} else {
			hour = tm.tm_hour - 12;
			pm = 1;
		}
		ds->hours = (ds->hours & 0xe0) | i_to_bcd(hour);
		if (pm) {
			ds->hours |= HOURS_PM;
		}
	} else {
		ds->hours = i_to_bcd(tm.tm_hour);
	}
	ds->minutes = i_to_bcd(tm.tm_min);
	ds->seconds = i_to_bcd(tm.tm_sec);
	//fprintf(stderr,"UDT: Year %d, hour %d minute %d\n",tm.tm_year+1900,tm.tm_hour,tm.tm_min);
	return;
}

static int
ds_load_from_diskimage(DS1337 * ds)
{
	uint8_t data[DS1337_IMAGESIZE];
	uint8_t *buf = data;
	int i;
	int count = DS1337_IMAGESIZE;
	if (!ds->disk_image) {
		fprintf(stderr, "Warning, no diskimage for DS1337 RTC\n");
		return -1;
	}
	if (DiskImage_Read(ds->disk_image, 0, buf, count) < count) {
		fprintf(stderr, "Error reading from DS1337 disk image\n");
		return -1;
	}
	if ((*buf++ != 0x78) || (*buf++ != 0x21)) {
		fprintf(stderr, "DS1337 invalid magic, starting with defaults\n");
		ds->time_offset = 0;
		return -1;
	}
	ds->time_offset = 0;
	for (i = 0; i < 8; i++) {
		ds->time_offset |= ((uint64_t) buf[0]) << (i * 8);
		buf++;
	}
	/* 
	 * --------------------------------------------------------------
	 * Time is read back from file also because clock might be in
	 * stopped state
	 * --------------------------------------------------------------
	 */
	ds->seconds = *buf++;
	ds->minutes = *buf++;
	ds->hours = *buf++;
	ds->day = *buf++;
	ds->date = *buf++;
	ds->month = *buf++;
	ds->year = *buf++;
	ds->seconds_alarm[0] = *buf++;
	ds->minutes_alarm[0] = *buf++;
	ds->hours_alarm[0] = *buf++;
	ds->daydate_alarm[0] = *buf++;
	ds->minutes_alarm[1] = *buf++;
	ds->hours_alarm[1] = *buf++;
	ds->daydate_alarm[1] = *buf++;
	ds->control_reg = *buf++;
	ds->status_reg = *buf++;
	/* 
	 * -------------------------------------------------------
	 * do not use the stored time offset when the oscillator
	 * is not enabled
	 * -------------------------------------------------------
	 */
	if (ds->control_reg & CONTROL_NEOSC) {
		ds->time_offset = diff_systime(ds);
	}
	return 0;
}

static void
ds_save_to_diskimage(DS1337 * ds)
{
	uint8_t data[DS1337_IMAGESIZE];
	uint8_t *buf = data;
	int i;
	*buf++ = 0x78;
	*buf++ = 0x21;
	for (i = 0; i < 8; i++) {
		*(buf++) = (ds->time_offset >> (i * 8)) & 0xff;
	}
	*(buf++) = ds->seconds;
	*(buf++) = ds->minutes;
	*(buf++) = ds->hours;
	*(buf++) = ds->day;
	*(buf++) = ds->date;
	*(buf++) = ds->month;
	*(buf++) = ds->year;
	*(buf++) = ds->seconds_alarm[0];
	*(buf++) = ds->minutes_alarm[0];
	*(buf++) = ds->hours_alarm[0];
	*(buf++) = ds->daydate_alarm[0];
	*(buf++) = ds->minutes_alarm[1];
	*(buf++) = ds->hours_alarm[1];
	*(buf++) = ds->daydate_alarm[1];
	*(buf++) = ds->control_reg;
	*(buf++) = ds->status_reg;
	if (!ds->disk_image) {
		fprintf(stderr, "Warning, no diskimage for DS1337 RTC\n");
		return;
	}
	if (DiskImage_Write(ds->disk_image, 0, data, buf - data) < (buf - data)) {
		fprintf(stderr, "Error writing to DS1337 disk image\n");
		return;
	}
	return;
}

static uint8_t
seconds_read(DS1337 * ds)
{
	return ds->seconds;
}

static void
seconds_write(DS1337 * ds, uint8_t data)
{
	ds->seconds = data;
	ds->useconds = 0;
	ds->time_changed_flag = 1;
}

static uint8_t
minutes_read(DS1337 * ds)
{
	return ds->minutes;
}

static void
minutes_write(DS1337 * ds, uint8_t data)
{
	ds->minutes = data;
	ds->time_changed_flag = 1;
}

static uint8_t
hour_read(DS1337 * ds)
{
	return ds->hours;
}

static void
hour_write(DS1337 * ds, uint8_t data)
{
	ds->hours = data;
	ds->time_changed_flag = 1;
}

static uint8_t
day_read(DS1337 * ds)
{
	return ds->day;
}

static void
day_write(DS1337 * ds, uint8_t data)
{
	ds->day = data;
	ds->time_changed_flag = 1;
}

static uint8_t
date_read(DS1337 * ds)
{
	return ds->date;
}

static void
date_write(DS1337 * ds, uint8_t data)
{
	ds->date = data;
	ds->time_changed_flag = 1;
}

static uint8_t
month_read(DS1337 * ds)
{
	return ds->month;
}

static void
month_write(DS1337 * ds, uint8_t data)
{
	ds->month = data;
	ds->time_changed_flag = 1;
}

static uint8_t
year_read(DS1337 * ds)
{
	return ds->year;
}

static void
year_write(DS1337 * ds, uint8_t data)
{
	ds->year = data;
	ds->time_changed_flag = 1;
}

static uint8_t
seconds_alarm1_read(DS1337 * ds)
{
	return ds->seconds_alarm[0];
}

static void
seconds_alarm1_write(DS1337 * ds, uint8_t data)
{
	ds->seconds_alarm[0] = data;
}

static uint8_t
minutes_alarm1_read(DS1337 * ds)
{
	return ds->minutes_alarm[0];
}

static void
minutes_alarm1_write(DS1337 * ds, uint8_t data)
{
	ds->minutes_alarm[0] = data;
}

static uint8_t
hour_alarm1_read(DS1337 * ds)
{
	return ds->hours_alarm[0];
}

static void
hour_alarm1_write(DS1337 * ds, uint8_t data)
{
	ds->hours_alarm[0] = data;
}

static uint8_t
daydate_alarm1_read(DS1337 * ds)
{
	return ds->daydate_alarm[0];
}

static void
daydate_alarm1_write(DS1337 * ds, uint8_t data)
{
	ds->daydate_alarm[0] = data;
}

static uint8_t
minutes_alarm2_read(DS1337 * ds)
{
	return ds->minutes_alarm[1];
}

static void
minutes_alarm2_write(DS1337 * ds, uint8_t data)
{
	ds->minutes_alarm[1] = data;
}

static uint8_t
hour_alarm2_read(DS1337 * ds)
{
	return ds->hours_alarm[1];
}

static void
hour_alarm2_write(DS1337 * ds, uint8_t data)
{
	ds->hours_alarm[1] = data;
}

static uint8_t
daydate_alarm2_read(DS1337 * ds)
{
	return ds->daydate_alarm[1];
}

static void
daydate_alarm2_write(DS1337 * ds, uint8_t data)
{
	ds->daydate_alarm[1] = data;
}

static uint8_t
control_read(DS1337 * ds)
{
	return ds->control_reg;
}

/*
 * --------------------------------------------------------------------
 * Control register
 * --------------------------------------------------------------------
 */
static void
control_write(DS1337 * ds, uint8_t data)
{
	uint8_t diff = data ^ ds->control_reg;
	update_time(ds);
	ds->control_reg = data & 0x9f;
	if ((diff & CONTROL_NEOSC) && !(data & CONTROL_NEOSC)) {
		ds->time_offset = diff_systime(ds);
		ds_save_to_diskimage(ds);
	} else if ((diff & CONTROL_NEOSC) && (data & CONTROL_NEOSC)) {
		/* OSCILLATOR is only stopped on power VCC loss ! */
		ds_save_to_diskimage(ds);
	}
}

/*
 * -------------------------------------------------------------------------
 * Status register
 * ------------------------------------------------------------------------
 */
static uint8_t
status_read(DS1337 * ds)
{
	return ds->status_reg;
}

static void
status_write(DS1337 * ds, uint8_t data)
{
	uint8_t clearmask = (STATUS_OSF | STATUS_A2F | STATUS_A1F);
	ds->status_reg = (ds->status_reg & ~clearmask) & (data & clearmask);
}

static void
write_reg(DS1337 * ds, uint8_t value, uint8_t addr)
{
	addr &= 0x7f;
	switch (addr) {
	    case REG_SECONDS:
		    seconds_write(ds, value);
		    break;

	    case REG_MINUTES:
		    minutes_write(ds, value);
		    break;

	    case REG_HOUR:
		    hour_write(ds, value);
		    break;

	    case REG_DAY:
		    day_write(ds, value);
		    break;

	    case REG_DATE:
		    date_write(ds, value);
		    break;

	    case REG_MONTH:
		    month_write(ds, value);
		    break;

	    case REG_YEAR:
		    year_write(ds, value);
		    break;

	    case REG_ALARM1_SECONDS:
		    seconds_alarm1_write(ds, value);
		    break;

	    case REG_ALARM1_MINUTES:
		    minutes_alarm1_write(ds, value);
		    break;

	    case REG_ALARM1_HOUR:
		    hour_alarm1_write(ds, value);
		    break;

	    case REG_ALARM1_DAYDATE:
		    daydate_alarm1_write(ds, value);
		    break;
	    case REG_ALARM2_MINUTES:
		    minutes_alarm2_write(ds, value);
		    break;

	    case REG_ALARM2_HOUR:
		    hour_alarm2_write(ds, value);
		    break;

	    case REG_ALARM2_DAYDATE:
		    daydate_alarm2_write(ds, value);
		    break;

	    case REG_CONTROL:
		    control_write(ds, value);
		    break;

	    case REG_STATUS:
		    status_write(ds, value);
		    break;

	    default:
		    fprintf(stderr, "DS1337: Illegal register number 0x%02x\n", addr);
	}
}

static uint8_t
read_reg(DS1337 * ds, uint8_t addr)
{
	addr &= 0x7f;
	uint8_t value;
	switch (addr) {
	    case REG_SECONDS:
		    value = seconds_read(ds);
		    break;
	    case REG_MINUTES:
		    value = minutes_read(ds);
		    break;
	    case REG_HOUR:
		    value = hour_read(ds);
		    break;
	    case REG_DAY:
		    value = day_read(ds);
		    break;
	    case REG_DATE:
		    value = date_read(ds);
		    break;
	    case REG_MONTH:
		    value = month_read(ds);
		    break;
	    case REG_YEAR:
		    value = year_read(ds);
		    break;
	    case REG_ALARM1_SECONDS:
		    value = seconds_alarm1_read(ds);
		    break;
	    case REG_ALARM1_MINUTES:
		    value = minutes_alarm1_read(ds);
		    break;
	    case REG_ALARM1_HOUR:
		    value = hour_alarm1_read(ds);
		    break;
	    case REG_ALARM1_DAYDATE:
		    value = daydate_alarm1_read(ds);
		    break;

	    case REG_ALARM2_MINUTES:
		    value = minutes_alarm2_read(ds);
		    break;
	    case REG_ALARM2_HOUR:
		    value = hour_alarm2_read(ds);
		    break;
	    case REG_ALARM2_DAYDATE:
		    value = daydate_alarm2_read(ds);
		    break;
	    case REG_CONTROL:
		    value = control_read(ds);
		    break;
	    case REG_STATUS:
		    value = status_read(ds);
		    break;
	    default:
		    value = 0;
		    fprintf(stderr, "DS1337: access to nonexisting register\n");
		    break;
	}
	return value;
}

static int
ds1337_read(void *dev, uint8_t * data)
{
	DS1337 *ds = dev;
	*data = read_reg(ds, ds->reg_address);
	dbgprintf("DS1337 read 0x%02x from %04x\n", *data, ds->reg_address);
	ds->reg_address = (ds->reg_address + 1) & 0xf;
	if (ds->reg_address == 0) {
		update_time(ds);
	}
	return I2C_DONE;
};

/*
 * ------------------------------------
 * DS1337 Write state machine 
 * ------------------------------------
 */
static int
ds1337_write(void *dev, uint8_t data)
{
	DS1337 *ds = dev;
	if (ds->state == DS_STATE_ADDR) {
		dbgprintf("DS1337 Addr 0x%02x\n", data);
		ds->reg_address = data & 0xf;
		ds->state = DS_STATE_DATA;
	} else if (ds->state == DS_STATE_DATA) {
		dbgprintf("DS1337 Write 0x%02x to %04x\n", data, ds->reg_address);
		write_reg(ds, data, ds->reg_address);
		ds->reg_address = ((ds->reg_address + 1) & 0xf) | (ds->reg_address & ~0xf);
	}
	return I2C_ACK;
};

static int
ds1337_start(void *dev, int i2c_addr, int operation)
{
	DS1337 *ds = dev;
	dbgprintf("ds1337 start\n");
	ds->state = DS_STATE_ADDR;
	ds->direction = operation;
	update_time(ds);
	ds->time_changed_flag = 0;
	return I2C_ACK;
}

static void
ds1337_stop(void *dev)
{
	DS1337 *ds = dev;
	dbgprintf("ds1337 stop\n");
	if (ds->direction == I2C_WRITE) {
		if (ds->time_changed_flag) {
			ds->time_offset = diff_systime(ds);
		}
		ds_save_to_diskimage(ds);
	}
	ds->state = DS_STATE_ADDR;
}

/*
 * -----------------------------------------------
 * ds1337_ops 
 *	I2C-Operations provided by the DS1337 
 * -----------------------------------------------
 */

static I2C_SlaveOps ds1337_ops = {
	.start = ds1337_start,
	.stop = ds1337_stop,
	.read = ds1337_read,
	.write = ds1337_write
};

I2C_Slave *
DS1337_New(char *name)
{
	DS1337 *ds = sg_new(DS1337);
	char *dirname, *imagename;

	I2C_Slave *i2c_slave;
	dirname = Config_ReadVar("global", "imagedir");
	if (dirname) {
		imagename = alloca(strlen(dirname) + strlen(name) + 20);
		sprintf(imagename, "%s/%s.img", dirname, name);
		ds->disk_image = DiskImage_Open(imagename, DS1337_IMAGESIZE, DI_RDWR | DI_CREAT_00);
		if (!ds->disk_image) {
			fprintf(stderr, "Failed to open DS1337 time offset file, using offset 0\n");
			ds->time_offset = 0;
		} else {
			if (ds_load_from_diskimage(ds) < 0) {
				ds->status_reg |= STATUS_OSF;
			}
		}
	} else {
		ds->time_offset = 0;
	}
	if (ds->control_reg & CONTROL_NEOSC) {
		ds->status_reg |= STATUS_OSF;
	}
	i2c_slave = &ds->i2c_slave;
	i2c_slave->devops = &ds1337_ops;
	i2c_slave->dev = ds;
	i2c_slave->speed = I2C_SPEED_FAST;
	update_time(ds);
	fprintf(stderr, "DS1337 Real Time Clock \"%s\" timeoffset %lld usec\n", name,
		(unsigned long long)diff_systime(ds));
	return i2c_slave;
}
