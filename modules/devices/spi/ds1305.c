/*
 *************************************************************************************************
 *
 * Emulation of Dallas DS1305 realtime clock with SPI Interface
 *
 * State: clock is basically working, readable and writable 
 *	  Alarms are missing
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
#include "signode.h"
#include "configfile.h"
#include "diskimage.h"
#include "ds1305.h"
#include "cycletimer.h"
#include "sgstring.h"

#define DIR_NONE (1)
#define DIR_IN (2)
#define DIR_OUT (3)

#define REG_SECONDS	(0)
#define	REG_MINUTES	(1)
#define REG_HOURS	(2)
#define		HOURS_12H	(1<<6)
#define		HOURS_PM	(1<<5)
#define	REG_DAY		(3)
#define REG_DATE	(4)
#define REG_MONTH	(5)
#define REG_YEAR	(6)
#define REG_ALARM0_SECONDS	(7)
#define REG_ALARM0_MINUTES	(8)
#define REG_ALARM0_HOUR		(9)
#define REG_ALARM0_DAY		(0xa)
#define REG_ALARM1_SECONDS	(0xb)
#define	REG_ALARM1_MINUTES	(0xc)
#define REG_ALARM1_HOUR		(0xd)
#define	REG_ALARM1_DAY		(0xe)
#define REG_CONTROL		(0xf)
#define		CONTROL_NEOSC	(1<<7)
#define		CONTROL_WP	(1<<6)
#define		CONTROL_INTCN	(1<<2)
#define		CONTROL_AIE1	(1<<1)
#define		CONTROL_AIE0	(1<<0)

#define REG_STATUS		(0x10)
#define REG_TRICKLE_CHRG	(0x11)
#define REG_RAM_START		(0x20)
#define REG_RAM_END		(0x7f)

#define DS1305_TIMEOFSSIZE (8)
#define DS1305_RAMSIZE (128-32)
#define DS1305_MAGICSIZE (2)
#define DS1305_IMAGESIZE (DS1305_TIMEOFSSIZE + DS1305_RAMSIZE + DS1305_MAGICSIZE)

typedef struct DS1305 {
	int cpol;		/* clock polarity */
	int spi_bits;
	SigNode *sclkNode;
	SigNode *sdiNode;
	SigNode *sdoNode;
	SigNode *ceNode;
	SigTrace *sclkTrace;
	SigTrace *ceTrace;
	/* state machine variables */
	int reg_addr;
	int out_bitcount;
	uint8_t outval;
	int in_bitcount;
	uint8_t inval;
	int direction;

	/* 
	 * Time must be written completely within one second
	 * after writing to the second register 
	 */
	CycleCounter_t last_seconds_update;

	int time_changed_flag;	/* update required when write done ? */

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
	uint8_t day_alarm[2];
	uint8_t control_reg;
	uint8_t status_reg;
	uint8_t trickle_charger_reg;

	DiskImage *disk_image;
	int64_t time_offset;	/* time difference to host clock */
	uint8_t ram[DS1305_RAMSIZE];
} DS1305;

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

static int
ds_load_from_diskimage(DS1305 * ds)
{
	uint8_t buf[DS1305_IMAGESIZE];
	int i;
	int count = DS1305_IMAGESIZE;
	if (!ds->disk_image) {
		return -1;
	}
	if (DiskImage_Read(ds->disk_image, 0, buf, count) < count) {
		fprintf(stderr, "Error reading from DS1305 disk image\n");
		return -1;
	}
	if ((buf[0] != 0x55) || (buf[1] != 0xaa)) {
		fprintf(stderr, "DS1305 invalid magic, starting with defaults\n");
		ds->time_offset = 0;
		return -1;
	}
	ds->time_offset = 0;
	for (i = 0; i < DS1305_TIMEOFSSIZE; i++) {
		ds->time_offset |= ((uint64_t) buf[i + DS1305_MAGICSIZE]) << (i * 8);
	}
	for (i = 0; i < DS1305_RAMSIZE; i++) {
		ds->ram[i] = buf[DS1305_TIMEOFSSIZE + DS1305_MAGICSIZE + i];
	}
	return 0;
}

static void
ds_save_to_diskimage(DS1305 * ds)
{
	uint8_t buf[DS1305_IMAGESIZE];
	int i;
	int count = DS1305_IMAGESIZE;
	buf[0] = 0x55;
	buf[1] = 0xaa;
	for (i = 0; i < 8; i++) {
		buf[i + DS1305_MAGICSIZE] = (ds->time_offset >> (i * 8)) & 0xff;
	}
	for (i = 0; i < DS1305_RAMSIZE; i++) {
		buf[i + DS1305_MAGICSIZE + DS1305_TIMEOFSSIZE] = ds->ram[i];
	}
	if (DiskImage_Write(ds->disk_image, 0, buf, count) < count) {
		fprintf(stderr, "Error writing to DS1305 disk image\n");
		return;
	}
	return;
}

static void
update_time(DS1305 * ds)
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
	//fprintf(stderr,"seconds %ld\n",time);
	gmtime_r(&time, &tm);
	//fprintf(stderr,"host: tm.hour %d tm.tm_isdst %d\n",tm.tm_hour,tm.tm_isdst);
	if ((tm.tm_year < 100) || (tm.tm_year >= 200)) {
		fprintf(stderr, "DS1305: Illegal year %d\n", tm.tm_year + 1900);
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
	fprintf(stderr, "UDT: Year %d, hour %d minute %d\n", tm.tm_year + 1900, tm.tm_hour,
		tm.tm_min);
	return;
}

/*
 * ---------------------------------------------------------
 * return the difference to system time in microseconds
 * ---------------------------------------------------------
 */
static int64_t
diff_systemtime(DS1305 * ds)
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
	setenv("TZ", "UTC", 1);	/* ??? "UTC" ??? */
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

static uint8_t
seconds_read(DS1305 * ds)
{
	return ds->seconds;
}

static void
seconds_write(DS1305 * ds, uint8_t data)
{
	update_time(ds);
	ds->seconds = data;
	ds->useconds = 0;
	ds->last_seconds_update = CycleCounter_Get();
	/* write time missing here */
	ds->time_changed_flag = 1;
	ds->time_offset = diff_systemtime(ds);
	ds_save_to_diskimage(ds);
}

static uint8_t
minutes_read(DS1305 * ds)
{
	return ds->minutes;
}

static void
minutes_write(DS1305 * ds, uint8_t data)
{
	int64_t us;
	update_time(ds);
	ds->minutes = data;
	ds->time_changed_flag = 1;
	us = CyclesToMicroseconds(CycleCounter_Get() - ds->last_seconds_update);
	if (us > 1000000) {
		fprintf(stderr,
			"DS1305 Warning: Write of minutes not within a second after writing seconds\n");
	}
	ds->time_offset = diff_systemtime(ds);
	ds_save_to_diskimage(ds);
}

static uint8_t
hours_read(DS1305 * ds)
{
	return ds->hours;
}

static void
hours_write(DS1305 * ds, uint8_t data)
{
	int64_t us;
	update_time(ds);
	ds->hours = data;
	ds->time_changed_flag = 1;
	us = CyclesToMicroseconds(CycleCounter_Get() - ds->last_seconds_update);
	if (us > 1000000) {
		fprintf(stderr,
			"DS1305 Warning: Write of hours not within a second after writing seconds\n");
	}
	ds->time_offset = diff_systemtime(ds);
	ds_save_to_diskimage(ds);
}

static uint8_t
day_read(DS1305 * ds)
{
	return ds->day;
}

static void
day_write(DS1305 * ds, uint8_t data)
{
	int64_t us;
	update_time(ds);
	ds->day = data;
	ds->time_changed_flag = 1;
	us = CyclesToMicroseconds(CycleCounter_Get() - ds->last_seconds_update);
	if (us > 1000000) {
		fprintf(stderr,
			"DS1305 Warning: Write of day not within a second after writing seconds\n");
	}
	ds->time_offset = diff_systemtime(ds);
	ds_save_to_diskimage(ds);
}

static uint8_t
date_read(DS1305 * ds)
{
	return ds->date;
}

static void
date_write(DS1305 * ds, uint8_t data)
{
	int64_t us;
	update_time(ds);
	ds->date = data;
	ds->time_changed_flag = 1;
	us = CyclesToMicroseconds(CycleCounter_Get() - ds->last_seconds_update);
	if (us > 1000000) {
		fprintf(stderr,
			"DS1305 Warning: Write of date not within a second after writing seconds\n");
	}
	ds->time_offset = diff_systemtime(ds);
	ds_save_to_diskimage(ds);
}

static uint8_t
month_read(DS1305 * ds)
{
	return ds->month;
}

static void
month_write(DS1305 * ds, uint8_t data)
{
	int64_t us;
	update_time(ds);
	ds->month = data;
	ds->time_changed_flag = 1;
	us = CyclesToMicroseconds(CycleCounter_Get() - ds->last_seconds_update);
	if (us > 1000000) {
		fprintf(stderr,
			"DS1305 Warning: Write of month not within a second after writing seconds\n");
	}
	ds->time_offset = diff_systemtime(ds);
	ds_save_to_diskimage(ds);
}

static uint8_t
year_read(DS1305 * ds)
{
	return ds->year;
}

static void
year_write(DS1305 * ds, uint8_t data)
{
	int64_t us;
	update_time(ds);
	ds->year = data;
	ds->time_changed_flag = 1;
	us = CyclesToMicroseconds(CycleCounter_Get() - ds->last_seconds_update);
	if (us > 1000000) {
		fprintf(stderr,
			"DS1305 Warning: Write of year not within a second after writing seconds\n");
	}
	ds->time_offset = diff_systemtime(ds);
	ds_save_to_diskimage(ds);
}

static uint8_t
seconds_alarm0_read(DS1305 * ds)
{
	fprintf(stderr, "seconds alarm0 register not implemented\n");
	return 0;
}

static void
seconds_alarm0_write(DS1305 * ds, uint8_t data)
{
	fprintf(stderr, "seconds alarm0 register not implemented\n");
}

static uint8_t
minutes_alarm0_read(DS1305 * ds)
{
	fprintf(stderr, "minutes alarm0 register not implemented\n");
	return 0;
}

static void
minutes_alarm0_write(DS1305 * ds, uint8_t data)
{
	fprintf(stderr, "minutes alarm0 register not implemented\n");
}

static uint8_t
hour_alarm0_read(DS1305 * ds)
{
	fprintf(stderr, "hour alarm0 register not implemented\n");
	return 0;
}

static void
hour_alarm0_write(DS1305 * ds, uint8_t data)
{
	fprintf(stderr, "hour alarm0 register not implemented\n");
}

static uint8_t
day_alarm0_read(DS1305 * ds)
{
	fprintf(stderr, "day alarm0 register not implemented\n");
	return 0;
}

static void
day_alarm0_write(DS1305 * ds, uint8_t data)
{
	fprintf(stderr, "day alarm0 register not implemented\n");
}

static uint8_t
seconds_alarm1_read(DS1305 * ds)
{
	fprintf(stderr, "seconds alarm1 register not implemented\n");
	return 0;
}

static void
seconds_alarm1_write(DS1305 * ds, uint8_t data)
{
	fprintf(stderr, "seconds alarm1 register not implemented\n");
}

static uint8_t
minutes_alarm1_read(DS1305 * ds)
{
	fprintf(stderr, "minutes alarm1 register not implemented\n");
	return 0;
}

static void
minutes_alarm1_write(DS1305 * ds, uint8_t data)
{
	fprintf(stderr, "minutes alarm1 register not implemented\n");
}

static uint8_t
hour_alarm1_read(DS1305 * ds)
{
	fprintf(stderr, "hour alarm1 register not implemented\n");
	return 0;
}

static void
hour_alarm1_write(DS1305 * ds, uint8_t data)
{
	fprintf(stderr, "hour alarm1 register not implemented\n");
}

static uint8_t
day_alarm1_read(DS1305 * ds)
{
	fprintf(stderr, "day alarm1 register not implemented\n");
	return 0;
}

static void
day_alarm1_write(DS1305 * ds, uint8_t data)
{
	fprintf(stderr, "day alarm1 register not implemented\n");
}

static uint8_t
control_read(DS1305 * ds)
{
	return ds->control_reg;
}

static void
control_write(DS1305 * ds, uint8_t data)
{
	uint8_t diff = data ^ ds->control_reg;
	if (ds->control_reg & data & CONTROL_WP) {
		/* Check if any non WP bit changed */
		if (diff) {
			fprintf(stderr, "DS1305 control register is write protected\n");
		}
		return;
	}
	ds->control_reg = data & 0xc7;
	if (!(data & CONTROL_NEOSC) && (diff & CONTROL_NEOSC)) {
		update_time(ds);
		ds_save_to_diskimage(ds);
	}
}

static uint8_t
status_read(DS1305 * ds)
{
	return ds->status_reg;
}

static void
status_write(DS1305 * ds, uint8_t data)
{
	fprintf(stderr, "status register not writable ??\n");
}

static uint8_t
trickle_charger_read(DS1305 * ds)
{
	fprintf(stderr, "trickle charger register not implemented\n");
	return 0;
}

static void
trickle_charger_write(DS1305 * ds, uint8_t data)
{
	fprintf(stderr, "trickle charger register not implemented\n");
}

static uint8_t
user_ram_read(DS1305 * ds, int addr)
{
	unsigned int ofs = (addr & 0x7f);
	if (ofs < 0x20) {
		return 0;
	}
	return ds->ram[ofs - 0x20];
}

static void
user_ram_write(DS1305 * ds, uint8_t data, int addr)
{
	unsigned int ofs = (addr & 0x7f);
	if (ofs < 0x20) {
		return;
	}
	ds->ram[ofs - 0x20] = data;
	ds_save_to_diskimage(ds);
}

static void
write_reg(DS1305 * ds, uint8_t value, uint8_t addr)
{
	addr &= 0x7f;
	if ((addr != REG_CONTROL) && (ds->control_reg & CONTROL_WP)) {
		fprintf(stderr, "DS1305 write: Writeprotect is set\n");
		return;
	}
	switch (addr) {
	    case REG_SECONDS:
		    seconds_write(ds, value);
		    break;

	    case REG_MINUTES:
		    minutes_write(ds, value);
		    break;

	    case REG_HOURS:
		    hours_write(ds, value);
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

	    case REG_ALARM0_SECONDS:
		    seconds_alarm0_write(ds, value);
		    break;

	    case REG_ALARM0_MINUTES:
		    minutes_alarm0_write(ds, value);
		    break;

	    case REG_ALARM0_HOUR:
		    hour_alarm0_write(ds, value);
		    break;

	    case REG_ALARM0_DAY:
		    day_alarm0_write(ds, value);
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

	    case REG_ALARM1_DAY:
		    day_alarm1_write(ds, value);
		    break;

	    case REG_CONTROL:
		    control_write(ds, value);
		    break;

	    case REG_STATUS:
		    status_write(ds, value);
		    break;

	    case REG_TRICKLE_CHRG:
		    trickle_charger_write(ds, value);
		    break;

	    default:
		    if ((addr >= REG_RAM_START) && (addr <= REG_RAM_END)) {
			    user_ram_write(ds, value, addr);
		    } else {
			    fprintf(stderr, "DS1305 emulator bug: Illegal register number 0x%02x\n",
				    addr);
		    }
	}
}

static uint8_t
read_reg(DS1305 * ds, uint8_t addr)
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
	    case REG_HOURS:
		    value = hours_read(ds);
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
	    case REG_ALARM0_SECONDS:
		    value = seconds_alarm0_read(ds);
		    break;
	    case REG_ALARM0_MINUTES:
		    value = minutes_alarm0_read(ds);
		    break;
	    case REG_ALARM0_HOUR:
		    value = hour_alarm0_read(ds);
		    break;
	    case REG_ALARM0_DAY:
		    value = day_alarm0_read(ds);
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
	    case REG_ALARM1_DAY:
		    value = day_alarm1_read(ds);
		    break;
	    case REG_CONTROL:
		    value = control_read(ds);
		    break;
	    case REG_STATUS:
		    value = status_read(ds);
		    break;
	    case REG_TRICKLE_CHRG:
		    value = trickle_charger_read(ds);
		    break;
	    default:
		    if ((addr >= REG_RAM_START) && (addr <= REG_RAM_END)) {
			    value = user_ram_read(ds, addr);
		    } else {
			    value = 0;
		    }
	}
	return value;
}

/*
 * -----------------------------------------------------------------------------------------
 * sdi_latch
 * 	Shift in the bit which is currently on the SDI line into the input shift register
 * -----------------------------------------------------------------------------------------
 */
static void
sdi_latch(DS1305 * ds)
{
	int val = SigNode_Val(ds->sdiNode);
	int bit = (val == SIG_HIGH) ? 1 : 0;
	ds->inval = (ds->inval << 1) | bit;
	ds->in_bitcount++;
	//fprintf(stderr,"DS1305: Latch a %d, count %d\n",bit,ds->in_bitcount );
	if (ds->in_bitcount == 8) {
		ds->reg_addr = ds->inval;
		fprintf(stderr, "DS1305 register %02x\n", ds->reg_addr);
		if (ds->reg_addr & 0x80) {
			ds->direction = DIR_IN;
		} else {
			update_time(ds);
			ds->direction = DIR_OUT;
		}
		ds->inval = 0;
	} else if ((ds->in_bitcount & 7) == 0) {
		if (ds->direction == DIR_IN) {
			fprintf(stderr, "DS1305 write register value %02x to %02x\n", ds->inval,
				ds->reg_addr);
			write_reg(ds, ds->inval, ds->reg_addr);
			if (ds->reg_addr < 0x9f) {
				ds->reg_addr++;
			} else if (ds->reg_addr == 0x9f) {
				ds->reg_addr = 0x80;
			} else if (ds->reg_addr < 0xff) {
				ds->reg_addr++;
			} else if (ds->reg_addr == 0xff) {
				ds->reg_addr = 0xa0;
			}
		}
		ds->inval = 0;
	}
}

/*
 * ---------------------------------------------------------------------------
 * sdo_shiftout
 * 	Shift out one bit from the output shift register to the SDO line
 * ---------------------------------------------------------------------------
 */
static void
sdo_shiftout(DS1305 * ds)
{
	if (ds->direction == DIR_OUT) {
		if (ds->out_bitcount == 0) {
			ds->outval = read_reg(ds, ds->reg_addr);
			fprintf(stderr, "Start shiftout of 0x%02x\n", ds->outval);
			ds->out_bitcount = 8;
			if (ds->reg_addr < 0x1f) {
				ds->reg_addr++;
			} else if (ds->reg_addr == 0x1f) {
				ds->reg_addr = 0;
			} else if (ds->reg_addr < 0x7f) {
				ds->reg_addr++;
			} else if (ds->reg_addr == 0x7f) {
				ds->reg_addr = 0x20;
			}
		}
		if (ds->outval & (1 << 7)) {
			SigNode_Set(ds->sdoNode, SIG_HIGH);
		} else {
			SigNode_Set(ds->sdoNode, SIG_LOW);
		}
		ds->outval <<= 1;
		ds->out_bitcount--;
	} else {
		SigNode_Set(ds->sdoNode, SIG_OPEN);
	}
}

/*
 * --------------------------------------------------------------------------
 * SpiClk
 * 	The signal trace procedure invoked when the SPI-Clock line changes 
 * --------------------------------------------------------------------------
 */
static void
SpiClk(SigNode * node, int value, void *clientData)
{
	DS1305 *ds = (DS1305 *) clientData;
	if (value == SIG_HIGH) {
		if (ds->cpol == 0) {
			sdo_shiftout(ds);
		} else {
			sdi_latch(ds);
		}
	} else if (value == SIG_LOW) {
		if (ds->cpol == 0) {
			sdi_latch(ds);
		} else {
			sdo_shiftout(ds);
		}
	}
	return;
}

/*
 * -----------------------------------------------------------
 * State change of Chip enable line
 * -----------------------------------------------------------
 */
static void
SpiCe(SigNode * node, int value, void *clientData)
{
	DS1305 *ds = (DS1305 *) clientData;
	if (value == SIG_HIGH) {
		fprintf(stderr, "DS1305: GOT Chip enable\n");
		if (!ds->sclkTrace) {
			ds->sclkTrace = SigNode_Trace(ds->sclkNode, SpiClk, ds);
		}
		ds->reg_addr = 0;
		ds->in_bitcount = 0;
		ds->out_bitcount = 0;
		ds->direction = DIR_NONE;
		SigNode_Set(ds->sdoNode, SIG_OPEN);
		if (SigNode_Val(ds->sclkNode) == SIG_HIGH) {
			/* shift out on negedge, latch on posedge */
			ds->cpol = 1;
		} else {
			/* Shift out on posedge, latch on negedge */
			ds->cpol = 0;
		}
	} else if (value == SIG_LOW) {
		fprintf(stderr, "DS1305: GOT Chip disable\n");
		ds->direction = DIR_NONE;
		if (ds->sclkTrace) {
			SigNode_Untrace(ds->sclkNode, ds->sclkTrace);
			ds->sclkTrace = NULL;
		}
	}
}

/*
 * -------------------------------------------------------------------------------
 * DS1305_New
 * Constructor for DS1305 RTC
 * Creates also a Pullup resistor because I'm to lazy to do it elsewhere
 * -------------------------------------------------------------------------------
 */
void
DS1305_New(const char *name)
{
	char *dirname;
	char *imagename;
	DS1305 *ds = sg_new(DS1305);
	ds->sclkNode = SigNode_New("%s.sclk", name);
	ds->sdiNode = SigNode_New("%s.sdi", name);
	ds->sdoNode = SigNode_New("%s.sdo", name);
	ds->ceNode = SigNode_New("%s.ce", name);
	if (!ds->sclkNode || !ds->sdiNode || !ds->sdoNode || !ds->ceNode) {
		fprintf(stderr, "CSPI \"%s\": Signal line creation failed\n", name);
		exit(1);
	}

	ds->ceTrace = SigNode_Trace(ds->ceNode, SpiCe, ds);
	ds->control_reg = CONTROL_WP;

	dirname = Config_ReadVar("global", "imagedir");
	if (dirname) {
		imagename = alloca(strlen(dirname) + strlen(name) + 20);
		sprintf(imagename, "%s/%s.img", dirname, name);
		ds->disk_image =
		    DiskImage_Open(imagename, 8 + DS1305_RAMSIZE, DI_RDWR | DI_CREAT_00);
		if (!ds->disk_image) {
			fprintf(stderr, "Failed to open DS1305 time offset file, using offset 0\n");
			ds->time_offset = 0;
		} else {
			if (ds_load_from_diskimage(ds) < 0) {
				ds->control_reg |= CONTROL_NEOSC;
			}
		}
	} else {
		ds->time_offset = 0;
	}
	fprintf(stderr, "DS1305 created with time offset %lld\n", (long long)ds->time_offset);
	update_time(ds);
}
