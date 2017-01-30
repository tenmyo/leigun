/*
 *************************************************************************************************
 *
 * Emulation of Dallas DS1302 realtime clock with an interface similar to SPI
 *
 * State: Not working
 *
 * Copyright 2010 Jochen Karrer. All rights reserved.
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
#include "ds1302.h"
#include "cycletimer.h"
#include "sgstring.h"

#define REG_SECONDS	(0x80)
#define	REG_MINUTES	(0x82)
#define REG_HOURS	(0x84)
#define		HOURS_12H	(1<<7)
#define		HOURS_PM	(1<<5)
#define REG_DATE	(0x86)
#define REG_MONTH	(0x88)
#define	REG_DAY		(0x8a)
#define REG_YEAR	(0x8c)
#define REG_WP		(0x8e)
#define		WP_WP	(0x80)
#define REG_TRICKLE	(0x90)
#define REG_CLOCK_BURST	(0xbe)
#define REG_RAM_START	(0xC0)
#define REG_RAM_END	(0xFC)
#define REG_RAM_BURST	(0xFE)

#define DS1302_TIMEOFSSIZE (8)
#define DS1302_RAMSIZE (31)
#define DS1302_MAGICSIZE (2)
#define DS1302_IMAGESIZE (DS1302_TIMEOFSSIZE + DS1302_RAMSIZE + DS1302_MAGICSIZE)

#define IODIR_IN		(2)
#define IODIR_OUT		(1)
#define IODIR_NONE		(0)

typedef struct DS1302 {
	int spi_bits;
	SigNode *sigSclk;
	int ioDir;
	SigNode *sigIo;
	SigNode *sigCe;
	SigTrace *sclkTrace;
	SigTrace *ceTrace;
	/* state machine variables */
	int reg_addr;
	int out_bitcount;
	int burstlen;
	uint8_t outval;
	int in_bitcount;
	uint8_t inval;

	/* 
	 * Time must be written completely within one second
	 * after writing to the second register 
	 */
	CycleCounter_t last_seconds_update;

	int time_changed_flag;	/* update required when write done ? */

	uint32_t useconds;	/* internal only */
	/* The registers */
	uint8_t regSeconds;
	uint8_t regMinutes;
	uint8_t regHours;
	uint8_t regDay;
	uint8_t regDate;
	uint8_t regMonth;
	uint8_t regYear;
	uint8_t regWp;
	uint8_t regTrickleCharger;

	DiskImage *disk_image;
	int64_t time_offset;	/* time difference to host clock */
	uint8_t ram[DS1302_RAMSIZE];
} DS1302;

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
ds_load_from_diskimage(DS1302 * ds)
{
	uint8_t buf[DS1302_IMAGESIZE];
	int i;
	int count = DS1302_IMAGESIZE;
	if (!ds->disk_image) {
		return -1;
	}
	if (DiskImage_Read(ds->disk_image, 0, buf, count) < count) {
		fprintf(stderr, "Error reading from DS1302 disk image\n");
		return -1;
	}
	if ((buf[0] != 0xe5) || (buf[1] != 0x3a)) {
		fprintf(stderr, "DS1302 invalid magic, starting with defaults\n");
		ds->time_offset = 0;
		return -1;
	}
	ds->time_offset = 0;
	for (i = 0; i < DS1302_TIMEOFSSIZE; i++) {
		ds->time_offset |= ((uint64_t) buf[i + DS1302_MAGICSIZE]) << (i * 8);
	}
	for (i = 0; i < DS1302_RAMSIZE; i++) {
		ds->ram[i] = buf[DS1302_TIMEOFSSIZE + DS1302_MAGICSIZE + i];
	}
	return 0;
}

static void
ds_save_to_diskimage(DS1302 * ds)
{
	uint8_t buf[DS1302_IMAGESIZE];
	int i;
	int count = DS1302_IMAGESIZE;
	buf[0] = 0xe5;
	buf[1] = 0x3a;
	for (i = 0; i < 8; i++) {
		buf[i + DS1302_MAGICSIZE] = (ds->time_offset >> (i * 8)) & 0xff;
	}
	for (i = 0; i < DS1302_RAMSIZE; i++) {
		buf[i + DS1302_MAGICSIZE + DS1302_TIMEOFSSIZE] = ds->ram[i];
	}
	if (DiskImage_Write(ds->disk_image, 0, buf, count) < count) {
		fprintf(stderr, "Error writing to DS1302 disk image\n");
		return;
	}
	return;
}

static void
update_time(DS1302 * ds)
{
	struct timeval host_tv;
	time_t time;
	struct tm tm;
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
		fprintf(stderr, "DS1302: Illegal year %d\n", tm.tm_year + 1900);
	}
	ds->regYear = i_to_bcd((tm.tm_year - 100) % 100);
	ds->regMonth = i_to_bcd(tm.tm_mon + 1);
	ds->regDay = i_to_bcd(tm.tm_wday + 1);
	ds->regDate = i_to_bcd(tm.tm_mday);
	if (ds->regHours & HOURS_12H) {
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
		ds->regHours = (ds->regHours & 0xe0) | i_to_bcd(hour);
		if (pm) {
			ds->regHours |= HOURS_PM;
		}
	} else {
		ds->regHours = i_to_bcd(tm.tm_hour);
	}
	ds->regMinutes = i_to_bcd(tm.tm_min);
	ds->regSeconds = i_to_bcd(tm.tm_sec);
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
diff_systemtime(DS1302 * ds)
{
	struct timeval now;
	char *zone;
	int64_t offset;
	time_t rtc_time;
	time_t sys_utc_time;
	struct tm tm;
	tm.tm_isdst = -1;
	tm.tm_mon = bcd_to_i(ds->regMonth) - 1;
	tm.tm_mday = bcd_to_i(ds->regDate);
	tm.tm_year = bcd_to_i(ds->regYear) + 100;
	if (ds->regHours & HOURS_12H) {
		uint8_t hour = bcd_to_i(ds->regHours & 0x1f) % 12;
		if (ds->regHours & HOURS_PM) {
			tm.tm_hour = hour + 12;
		} else {
			tm.tm_hour = hour;
		}
	} else {
		tm.tm_hour = bcd_to_i(ds->regHours & 0x3f);
	}
	tm.tm_min = bcd_to_i(ds->regMinutes);
	tm.tm_sec = bcd_to_i(ds->regSeconds);
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
seconds_read(DS1302 * ds)
{
	return ds->regSeconds;
}

static void
seconds_write(DS1302 * ds, uint8_t data)
{
	update_time(ds);
	ds->regSeconds = data;
	ds->useconds = 0;
	ds->last_seconds_update = CycleCounter_Get();
	/* write time missing here */
	ds->time_changed_flag = 1;
	ds->time_offset = diff_systemtime(ds);
	ds_save_to_diskimage(ds);
}

static uint8_t
minutes_read(DS1302 * ds)
{
	return ds->regMinutes;
}

static void
minutes_write(DS1302 * ds, uint8_t data)
{
	int64_t us;
	update_time(ds);
	ds->regMinutes = data;
	ds->time_changed_flag = 1;
	us = CyclesToMicroseconds(CycleCounter_Get() - ds->last_seconds_update);
	if (us > 1000000) {
		fprintf(stderr,
			"DS1302 Warning: Write of minutes not within a second after writing seconds\n");
	}
	ds->time_offset = diff_systemtime(ds);
	ds_save_to_diskimage(ds);
}

static uint8_t
hours_read(DS1302 * ds)
{
	return ds->regHours;
}

static void
hours_write(DS1302 * ds, uint8_t data)
{
	int64_t us;
	update_time(ds);
	ds->regHours = data;
	ds->time_changed_flag = 1;
	us = CyclesToMicroseconds(CycleCounter_Get() - ds->last_seconds_update);
	if (us > 1000000) {
		fprintf(stderr,
			"DS1302 Warning: Write of hours not within a second after writing seconds\n");
	}
	ds->time_offset = diff_systemtime(ds);
	ds_save_to_diskimage(ds);
}

static uint8_t
day_read(DS1302 * ds)
{
	return ds->regDay;
}

static void
day_write(DS1302 * ds, uint8_t data)
{
	int64_t us;
	update_time(ds);
	ds->regDay = data;
	ds->time_changed_flag = 1;
	us = CyclesToMicroseconds(CycleCounter_Get() - ds->last_seconds_update);
	if (us > 1000000) {
		fprintf(stderr,
			"DS1302 Warning: Write of day not within a second after writing seconds\n");
	}
	ds->time_offset = diff_systemtime(ds);
	ds_save_to_diskimage(ds);
}

static uint8_t
date_read(DS1302 * ds)
{
	return ds->regDate;
}

static void
date_write(DS1302 * ds, uint8_t data)
{
	int64_t us;
	update_time(ds);
	ds->regDate = data;
	ds->time_changed_flag = 1;
	us = CyclesToMicroseconds(CycleCounter_Get() - ds->last_seconds_update);
	if (us > 1000000) {
		fprintf(stderr,
			"DS1302 Warning: Write of date not within a second after writing seconds\n");
	}
	ds->time_offset = diff_systemtime(ds);
	ds_save_to_diskimage(ds);
}

static uint8_t
month_read(DS1302 * ds)
{
	return ds->regMonth;
}

static void
month_write(DS1302 * ds, uint8_t data)
{
	int64_t us;
	update_time(ds);
	ds->regMonth = data;
	ds->time_changed_flag = 1;
	us = CyclesToMicroseconds(CycleCounter_Get() - ds->last_seconds_update);
	if (us > 1000000) {
		fprintf(stderr,
			"DS1302 Warning: Write of month not within a second after writing seconds\n");
	}
	ds->time_offset = diff_systemtime(ds);
	ds_save_to_diskimage(ds);
}

static uint8_t
year_read(DS1302 * ds)
{
	return ds->regYear;
}

static void
year_write(DS1302 * ds, uint8_t data)
{
	int64_t us;
	update_time(ds);
	ds->regYear = data;
	ds->time_changed_flag = 1;
	us = CyclesToMicroseconds(CycleCounter_Get() - ds->last_seconds_update);
	if (us > 1000000) {
		fprintf(stderr,
			"DS1302 Warning: Write of year not within a second after writing seconds\n");
	}
	ds->time_offset = diff_systemtime(ds);
	ds_save_to_diskimage(ds);
}

static uint8_t
reg_wp_read(DS1302 * ds)
{
	return ds->regWp;
}

static void
reg_wp_write(DS1302 * ds, uint8_t data)
{
	ds->regWp = data & WP_WP;
	update_time(ds);
	ds_save_to_diskimage(ds);
}

static uint8_t
trickle_charger_read(DS1302 * ds)
{
	fprintf(stderr, "trickle charger register not implemented\n");
	return 0;
}

static void
trickle_charger_write(DS1302 * ds, uint8_t data)
{
	fprintf(stderr, "trickle charger register not implemented\n");
}

static uint8_t
user_ram_read(DS1302 * ds, int addr)
{
	return ds->ram[addr];
}

static void
user_ram_write(DS1302 * ds, uint8_t data, int addr)
{
	ds->ram[addr] = data;
	ds_save_to_diskimage(ds);
}

static void
write_reg(DS1302 * ds, uint8_t value, uint8_t addr)
{
	addr &= 0x7f;
	if ((addr != REG_WP) && (ds->regWp & WP_WP)) {
		fprintf(stderr, "DS1302 write: Writeprotect is set\n");
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

	    case REG_WP:
		    reg_wp_write(ds, value);
		    break;

	    case REG_TRICKLE:
		    trickle_charger_write(ds, value);
		    break;

	    default:
		    if ((addr >= REG_RAM_START) && (addr <= REG_RAM_END)) {
			    user_ram_write(ds, value, (addr - REG_RAM_START) >> 1);
		    } else {
			    fprintf(stderr, "DS1302 emulator bug: Illegal register number 0x%02x\n",
				    addr);
		    }
	}
}

static uint8_t
read_reg(DS1302 * ds, uint8_t addr)
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
	    case REG_WP:
		    value = reg_wp_read(ds);
		    break;
	    case REG_TRICKLE:
		    value = trickle_charger_read(ds);
		    break;
	    default:
		    if ((addr >= REG_RAM_START) && (addr <= REG_RAM_END)) {
			    value = user_ram_read(ds, (addr - REG_RAM_START) >> 1);
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
sdi_latch(DS1302 * ds)
{
	int val = SigNode_Val(ds->sigIo);
	int bit = (val == SIG_HIGH) ? 0x80 : 0;
	ds->inval = (ds->inval >> 1) | bit;
	ds->in_bitcount++;
	//fprintf(stderr,"DS1302: Latch a %d, count %d\n",bit,ds->in_bitcount );
	if (ds->in_bitcount == 8) {
		if ((ds->inval & 0xfe) == REG_CLOCK_BURST) {
			ds->reg_addr = REG_RAM_START;
			ds->burstlen = 31;
		} else if ((ds->inval & 0xfe) == REG_RAM_BURST) {
			ds->reg_addr = REG_SECONDS;
			ds->burstlen = 8;
		} else {
			ds->reg_addr = ds->inval & 0xfe;
			ds->burstlen = 1;
		}
		fprintf(stderr, "DS1302 register %02x\n", ds->reg_addr);
		if (ds->inval & 0x1) {
			ds->ioDir = IODIR_IN;
		} else {
			update_time(ds);
			ds->ioDir = IODIR_OUT;
		}
		ds->inval = 0;
	} else if ((ds->in_bitcount & 7) == 0) {
		if (ds->ioDir == IODIR_IN) {
			fprintf(stderr, "DS1302 write register value %02x to %02x\n", ds->inval,
				ds->reg_addr);
			if (ds->burstlen) {
				ds->burstlen--;
				write_reg(ds, ds->inval, ds->reg_addr);
				ds->reg_addr++;
			} else {
				fprintf(stderr, "DS1302 Write more than Burstlen\n");
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
sdo_shiftout(DS1302 * ds)
{
	if (ds->ioDir == IODIR_OUT) {
		if (ds->out_bitcount == 0) {
			if (ds->burstlen) {
				ds->burstlen--;
				ds->outval = read_reg(ds, ds->reg_addr);
				fprintf(stderr, "Start shiftout of 0x%02x\n", ds->outval);
				ds->out_bitcount = 8;
				ds->reg_addr++;
			} else {
				SigNode_Set(ds->sigIo, SIG_OPEN);
				return;
			}
		}
		if (ds->outval & 1) {
			SigNode_Set(ds->sigIo, SIG_HIGH);
		} else {
			SigNode_Set(ds->sigIo, SIG_LOW);
		}
		ds->outval >>= 1;
		ds->out_bitcount--;
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
	DS1302 *ds = (DS1302 *) clientData;
	if (value == SIG_HIGH) {
		if (ds->ioDir == IODIR_IN) {
			sdi_latch(ds);
		}
	} else if (value == SIG_LOW) {
		if (ds->ioDir == IODIR_OUT) {
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
	DS1302 *ds = (DS1302 *) clientData;
	if (value == SIG_HIGH) {
		fprintf(stderr, "DS1302: GOT Chip enable\n");
		if (!ds->sclkTrace) {
			ds->sclkTrace = SigNode_Trace(ds->sigSclk, SpiClk, ds);
		}
		ds->reg_addr = 0;
		ds->in_bitcount = 0;
		ds->out_bitcount = 0;
		ds->ioDir = IODIR_IN;
		SigNode_Set(ds->sigIo, SIG_OPEN);
	} else if (value == SIG_LOW) {
		fprintf(stderr, "DS1302: GOT Chip disable\n");
		ds->ioDir = IODIR_NONE;
		SigNode_Set(ds->sigIo, SIG_OPEN);
		if (ds->sclkTrace) {
			SigNode_Untrace(ds->sigSclk, ds->sclkTrace);
			ds->sclkTrace = NULL;
		}
	}
}

/*
 * -------------------------------------------------------------------------------
 * DS1302_New
 * Constructor for DS1302 RTC
 * Creates also a Pullup resistor because I'm to lazy to do it elsewhere
 * -------------------------------------------------------------------------------
 */
void
DS1302_New(const char *name)
{
	char *dirname;
	char *imagename;
	DS1302 *ds = sg_new(DS1302);
	ds->sigSclk = SigNode_New("%s.sclk", name);
	ds->sigIo = SigNode_New("%s.io", name);
	ds->sigCe = SigNode_New("%s.ce", name);
	if (!ds->sigSclk || !ds->sigIo || !ds->sigCe) {
		fprintf(stderr, "CSPI \"%s\": Signal line creation failed\n", name);
		exit(1);
	}
	SigNode_Set(ds->sigSclk, SIG_WEAK_PULLDOWN);
	ds->ceTrace = SigNode_Trace(ds->sigCe, SpiCe, ds);
	ds->regWp = WP_WP;

	dirname = Config_ReadVar("global", "imagedir");
	if (dirname) {
		imagename = alloca(strlen(dirname) + strlen(name) + 20);
		sprintf(imagename, "%s/%s.img", dirname, name);
		ds->disk_image =
		    DiskImage_Open(imagename, 8 + DS1302_RAMSIZE, DI_RDWR | DI_CREAT_00);
		if (!ds->disk_image) {
			fprintf(stderr, "Failed to open DS1302 time offset file, using offset 0\n");
			ds->time_offset = 0;
		} else {
			if (ds_load_from_diskimage(ds) < 0) {

			}
		}
	} else {
		ds->time_offset = 0;
	}
	fprintf(stderr, "DS1302 created with time offset %lld\n", (long long)ds->time_offset);
	update_time(ds);
}
