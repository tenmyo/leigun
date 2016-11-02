/*
 *************************************************************************************************
 *
 * Emulation of Dallas DS3234 realtime clock with SPI Interface
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

#if 0
#define dbgprintf(x...) { fprintf(stderr,x); }
#else
#define dbgprintf(x...) 
#endif

#define RTC_MAGIC_HEADER 0x5732

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
#define REG_ALARM1_SECONDS	(7)
#define REG_ALARM1_MINUTES	(8)
#define REG_ALARM1_HOUR		(9)
#define REG_ALARM1_DAY		(0xa)
#define	REG_ALARM2_MINUTES	(0xb)
#define REG_ALARM2_HOUR		(0xc)
#define	REG_ALARM2_DAY		(0xd)
#define REG_CONTROL		(0xe)
#define		CONTROL_NEOSC	(1<<7)
#define		CONTROL_BBSQW	(1<<6)
#define		CONTROL_CONV	(1<<5)
#define		CONTROL_RS2	(1<<4)
#define		CONTROL_RS1	(1<<3)
#define		CONTROL_INTCN	(1<<2)
#define		CONTROL_A2IE	(1<<1)
#define		CONTROL_A1IE	(1<<0)

#define REG_STATUS		(0x0f)
#define		STATUS_OSF	(1<<7)
#define		STATUS_BB32KHZ	(1<<6)
#define		STATUS_CRATE_MASK	(3<<4)
#define		STATUS_CRATE_SHIFT	(4)
#define			CRATE_64	(0<<4)
#define			CRATE_128	(1<<4)
#define			CRATE_256	(2<<4)
#define			CRATE_512	(3<<4)
#define		STATUS_EN32KHZ	(1<<3)
#define		STATUS_BSY	(1<<2)
#define		STATUS_A2F	(1<<1)
#define		STATUS_A1F	(1<<0)
#define	REG_AGING		(0x10)
#define	REG_TEMPM		(0x11)
#define	REG_TEMPL		(0x12)
#define	REG_DISCONV		(0x13)
#define REG_SRAM_ADDR		(0x18)
#define REG_SRAM_DATA		(0x19)

#define DS3234_TIMEOFSSIZE (8)
#define DS3234_RAMSIZE (256)
#define DS3234_MAGICSIZE (2)
#define DS3234_IMAGESIZE (512)

typedef struct DS3234 {
	int cpol; /* clock polarity */
	int spi_bits;
	SigNode *sclkNode;
	SigNode *sdiNode;
	SigNode *sdoNode;
	SigNode *ceNode;
	SigTrace *sclkTrace;
	SigTrace *csTrace;
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
	CycleCounter_t 	last_seconds_update; 
	CycleTimer TemperatureUpdateTimer;

	int time_changed_flag; /* update required when write done ? */

	uint32_t useconds; /* internal only */
	/* The registers */
	uint8_t seconds;
	uint8_t minutes;
	uint8_t hours;
	uint8_t day;
	uint8_t date;
	uint8_t month;
	uint8_t year;
	uint8_t seconds_alarm1;
	uint8_t minutes_alarm[2];
	uint8_t hours_alarm[2];
	uint8_t day_alarm[2];
	uint8_t control_reg;
	uint8_t status_reg;
	uint8_t aging;
	uint8_t tempm;
	uint8_t templ;
	uint8_t disconv;

	DiskImage *disk_image;
	int64_t time_offset; /* time difference to host clock */
	uint8_t sram_addr;
	uint8_t sram[DS3234_RAMSIZE];
} DS3234;


static void
update_temperature(DS3234 *ds) 
{
	int temperature = (16 << 2) + (rand() & 0x1f);
	ds->tempm = temperature >> 2;
	ds->templ = (temperature & 0x3) << 6;
	/* fprintf(stderr,"Temperature %f\n",0.25 *temperature); */
}

static void
temp_meass_timer(void *cd)
{
	DS3234 *ds = (DS3234*)cd;
	int rate = ds->status_reg & STATUS_CRATE_MASK >> STATUS_CRATE_SHIFT;
	int millisec = 64000 << rate;	
	update_temperature(ds);
	CycleTimer_Mod(&ds->TemperatureUpdateTimer,MillisecondsToCycles(millisec));
}

/*
 * -------------------------------------------------
 * Conversion between BCD and 8Bit Integer
 * -------------------------------------------------
 */
static inline uint8_t
bcd_to_i(uint8_t b) {
        return (b&0xf)+10*((b>>4)&0xf);
}
static inline uint8_t 
i_to_bcd(uint8_t b) {
        return (b/10)*16+(b-10*(b/10));
}


static int
ds_load_from_diskimage(DS3234 *ds) 
{
	uint8_t buf[DS3234_IMAGESIZE];
	int i;
	int count = 2;
	if(!ds->disk_image) {
		return -1;
	}
	if(DiskImage_Read(ds->disk_image,0,buf,DS3234_IMAGESIZE) < DS3234_IMAGESIZE) {
		fprintf(stderr,"Error reading from DS3234 disk image\n");
		return -1;
	}
	if((buf[0] != (RTC_MAGIC_HEADER & 0xff)) || (buf[1] != (RTC_MAGIC_HEADER >> 8))) {
		fprintf(stderr,"DS3234 invalid magic, starting with defaults\n");
		ds->time_offset = 0;
		return -1;	
	}
	ds->time_offset = 0;
	for(i=0;i<DS3234_TIMEOFSSIZE;i++) {
		ds->time_offset |= ((uint64_t)buf[count++])  << (i*8);
	}
	for(i=0;i<DS3234_RAMSIZE;i++) {
		ds->sram[i] = buf[count++];
	}
	ds->seconds_alarm1 = buf[count++];
	for(i=0;i<2;i++) {
		ds->minutes_alarm[i] = buf[count++];
		ds->hours_alarm[i] = buf[count++];
		ds->day_alarm[i] = buf[count++];
	}
	ds->control_reg = buf[count++];
	ds->status_reg = buf[count++];
	ds->aging = buf[count++];
	ds->tempm = buf[count++];
	ds->templ = buf[count++];
	ds->disconv = buf[count++];
	if((buf[count++] != (RTC_MAGIC_HEADER & 0xff)) || (buf[count++] != (RTC_MAGIC_HEADER >> 8))) {
		fprintf(stderr,"DS3234 invalid magic tail, starting with defaults\n");
		ds->time_offset = 0;
		return -1;	
	}
	return 0;
}

static void
ds_save_to_diskimage(DS3234 *ds) 
{
	uint8_t buf[DS3234_IMAGESIZE];
	int i;
	int count = 2;
	memset(buf,0,sizeof(buf));
	buf[0] = RTC_MAGIC_HEADER & 0xff;
	buf[1] = RTC_MAGIC_HEADER >> 8; 
	for(i=0;i<8;i++) {
		buf[count++] = (ds->time_offset >> (i*8)) & 0xff;
	}
	for(i=0;i<DS3234_RAMSIZE;i++) {
		buf[count++] = ds->sram[i];
	}
	buf[count++] = ds->seconds_alarm1;
	for(i=0;i<2;i++) {
		buf[count++] = ds->minutes_alarm[i];
		buf[count++] = ds->hours_alarm[i];
		buf[count++] = ds->day_alarm[i];
	}
	buf[count++] = ds->control_reg;
	buf[count++] = ds->status_reg;
	buf[count++] = ds->aging;
	buf[count++] = ds->tempm;
	buf[count++] = ds->templ;
	buf[count++] = ds->disconv;
	buf[count++] = RTC_MAGIC_HEADER & 0xff;
	buf[count++] = RTC_MAGIC_HEADER >> 8; 
	if(DiskImage_Write(ds->disk_image,0,buf,DS3234_IMAGESIZE) < DS3234_IMAGESIZE) {
		fprintf(stderr,"Error writing to DS3234 disk image\n");
		return;
	}
	return;	
}

static void
update_time(DS3234 *ds) {
	struct timeval host_tv; 
        time_t time;
        struct tm tm;
        gettimeofday(&host_tv,NULL);
        time = host_tv.tv_sec;
        time += (ds->time_offset / (int64_t)1000000);
	ds->useconds = host_tv.tv_usec + (ds->time_offset % 1000000);
	if(ds->useconds > 1000000) {
		time++;
		ds->useconds -= 1000000;
	} else if(ds->useconds < 0) {
		fprintf(stderr,"Modulo was negative Happened\n");
		time--;
		ds->useconds += 1000000;
	}
        gmtime_r(&time,&tm);
	if((tm.tm_year < 100) || (tm.tm_year >= 200)) {
		fprintf(stderr,"DS3234: Illegal year %d\n",tm.tm_year+1900); 
	}
	ds->year = i_to_bcd((tm.tm_year-100) % 100);
	ds->month = i_to_bcd(tm.tm_mon+1);
	ds->day = i_to_bcd(tm.tm_wday+1);
	ds->date = i_to_bcd(tm.tm_mday);
	if(ds->hours & HOURS_12H) {
		int hour;
		int pm = 0;
		if(tm.tm_hour == 0) {
			hour = 12;
			pm = 0;
		} else if(tm.tm_hour < 12) {
			hour = tm.tm_hour;
			pm = 0;
		} else if(tm.tm_hour < 13) {
			hour = 12;
			pm = 1;	
		} else {
			hour = tm.tm_hour - 12; 
			pm = 1;
		}
		ds->hours = (ds->hours & 0xe0) | i_to_bcd(hour);
		if(pm) {
			ds->hours |= HOURS_PM;
		}
	} else {
		ds->hours = i_to_bcd(tm.tm_hour);
	}
	ds->minutes = i_to_bcd(tm.tm_min);
	ds->seconds = i_to_bcd(tm.tm_sec);
	dbgprintf("UDT: Year %d, hour %d minute %d\n",tm.tm_year+1900,tm.tm_hour,tm.tm_min);
	return;
}

/*
 * ---------------------------------------------------------
 * return the difference to system time in microseconds
 * ---------------------------------------------------------
 */
static int64_t
diff_systemtime(DS3234 *ds) {
	struct timeval now;
        char *zone;
        int64_t offset;
        time_t rtc_time;
        time_t sys_utc_time;
        struct tm tm;
        tm.tm_isdst=-1;
        tm.tm_mon= bcd_to_i(ds->month)-1;
        tm.tm_mday= bcd_to_i(ds->date);
        tm.tm_year=bcd_to_i(ds->year)+100;
	if(ds->hours & HOURS_12H) { 
		uint8_t hour = bcd_to_i(ds->hours & 0x1f) % 12;
		if(ds->hours & HOURS_PM) {
			tm.tm_hour = hour + 12;
		} else {
			tm.tm_hour = hour;
		}
	} else {
		tm.tm_hour = bcd_to_i(ds->hours & 0x3f);
	}
	tm.tm_min = bcd_to_i(ds->minutes);
	tm.tm_sec = bcd_to_i(ds->seconds);
	if(tm.tm_sec >= 60)  {
		tm.tm_sec = 0;
	}
	if(tm.tm_min >= 60)  {
		tm.tm_min = 0;
	}
	if(tm.tm_mon >= 12)  {
		tm.tm_mon = 0;
	}
	if((tm.tm_mday >= 32) || (tm.tm_mday < 1)) {
		tm.tm_mday=1;
	}
	if((tm.tm_year >= 200) || (tm.tm_year < 100)) {
		tm.tm_year = 100;
	}
        /* Shit, there is no mktime which does GMT */
        zone = getenv("TZ"); /* remember original */
        setenv("TZ", "UTC", 1); /* "UTC" */
        tzset();
        rtc_time = mktime(&tm);
        if (zone) {
                setenv("TZ", zone, 1);
        } else {
                unsetenv("TZ");
        }
        tzset();
        if(rtc_time == -1) {
                return 0;
        }

        gettimeofday(&now,NULL);
        sys_utc_time = now.tv_sec;

        offset = ((int64_t)rtc_time-(int64_t)sys_utc_time) * (int64_t)1000000;
	offset+=ds->useconds;
	offset-=now.tv_usec;
	dbgprintf("rtc %ld, sys %ld, calculated offset %lld\n",rtc_time,sys_utc_time,offset);
        return offset;
}

static uint8_t 
seconds_read(DS3234 *ds) 
{
	return ds->seconds;
}

static void 
seconds_write(DS3234 *ds,uint8_t data) 
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
minutes_read(DS3234 *ds) 
{
	return ds->minutes;
}

static void 
minutes_write(DS3234 *ds,uint8_t data) 
{
	int64_t us;
	update_time(ds);
	ds->minutes = data;
	ds->time_changed_flag = 1;
	us = CyclesToMicroseconds(CycleCounter_Get() - ds->last_seconds_update);	
	if(us>1000000) {
		fprintf(stderr,"DS3234 Warning: Write of minutes not within a second after writing seconds\n");
	}
	ds->time_offset = diff_systemtime(ds);
	ds_save_to_diskimage(ds);
}

static uint8_t 
hours_read(DS3234 *ds) 
{
	return ds->hours;
}

static void 
hours_write(DS3234 *ds,uint8_t data) 
{
	int64_t us;
	update_time(ds);
	ds->hours = data;
	ds->time_changed_flag = 1;
	us = CyclesToMicroseconds(CycleCounter_Get() - ds->last_seconds_update);	
	if(us>1000000) {
		fprintf(stderr,"DS3234 Warning: Write of hours not within a second after writing seconds\n");
	}
	ds->time_offset = diff_systemtime(ds);
	ds_save_to_diskimage(ds);
}

static uint8_t 
day_read(DS3234 *ds) 
{
	return ds->day;
}

static void 
day_write(DS3234 *ds,uint8_t data) 
{
	int64_t us;
	update_time(ds);
	ds->day = data;
	ds->time_changed_flag = 1;
	us = CyclesToMicroseconds(CycleCounter_Get() - ds->last_seconds_update);	
	if(us>1000000) {
		fprintf(stderr,"DS3234 Warning: Write of day not within a second after writing seconds\n");
	}
	ds->time_offset = diff_systemtime(ds);
	ds_save_to_diskimage(ds);
}

static uint8_t 
date_read(DS3234 *ds) 
{
	return ds->date;
}

static void 
date_write(DS3234 *ds,uint8_t data) 
{
	int64_t us;
	update_time(ds);
	ds->date = data;
	ds->time_changed_flag = 1;
	us = CyclesToMicroseconds(CycleCounter_Get() - ds->last_seconds_update);	
	if(us>1000000) {
		fprintf(stderr,"DS3234 Warning: Write of date not within a second after writing seconds\n");
	}
	ds->time_offset = diff_systemtime(ds);
	ds_save_to_diskimage(ds);
}

static uint8_t 
month_read(DS3234 *ds) 
{
	return ds->month;
}

static void 
month_write(DS3234 *ds,uint8_t data) 
{
	int64_t us;
	update_time(ds);
	ds->month = data;
	ds->time_changed_flag = 1;
	us = CyclesToMicroseconds(CycleCounter_Get() - ds->last_seconds_update);	
	if(us>1000000) {
		fprintf(stderr,"DS3234 Warning: Write of month not within a second after writing seconds\n");
	}
	ds->time_offset = diff_systemtime(ds);
	ds_save_to_diskimage(ds);
}

static uint8_t 
year_read(DS3234 *ds) 
{
	return ds->year;
}

static void 
year_write(DS3234 *ds,uint8_t data) 
{
	int64_t us;
	update_time(ds);
	ds->year = data;
	ds->time_changed_flag = 1;
	us = CyclesToMicroseconds(CycleCounter_Get() - ds->last_seconds_update);	
	if(us>1000000) {
		fprintf(stderr,"DS3234 Warning: Write of year not within a second after writing seconds\n");
	}
	ds->time_offset = diff_systemtime(ds);
	ds_save_to_diskimage(ds);
}

static uint8_t 
seconds_alarm1_read(DS3234 *ds) 
{
	return ds->seconds_alarm1;
}

static void 
seconds_alarm1_write(DS3234 *ds,uint8_t data) 
{
	ds->seconds_alarm1 = data;
	// update_alarm()
}

static uint8_t
minutes_alarm1_read(DS3234 *ds) 
{
	return ds->minutes_alarm[0];
}

static void 
minutes_alarm1_write(DS3234 *ds,uint8_t data) 
{
	ds->minutes_alarm[0] = data;
	//update_alarm();
}

static uint8_t
hour_alarm1_read(DS3234 *ds) 
{
	return ds->hours_alarm[0];
}

static void 
hour_alarm1_write(DS3234 *ds,uint8_t data) 
{
	ds->hours_alarm[0] = data;
	//update_alarm();
}

static uint8_t
day_alarm1_read(DS3234 *ds) 
{
	return ds->day_alarm[0];
}

static void 
day_alarm1_write(DS3234 *ds,uint8_t data) 
{
	
	ds->day_alarm[0] = data;
	//update_alarm();
}


static uint8_t
minutes_alarm2_read(DS3234 *ds) 
{
	return ds->minutes_alarm[1];
}

static void 
minutes_alarm2_write(DS3234 *ds,uint8_t data) 
{
	ds->minutes_alarm[1] = data;
	//update_alarm();
}

static uint8_t
hour_alarm2_read(DS3234 *ds) 
{
	return ds->hours_alarm[1];
}

static void 
hour_alarm2_write(DS3234 *ds,uint8_t data) 
{
	ds->hours_alarm[1] = data;
	// update_alarm;
}

static uint8_t
day_alarm2_read(DS3234 *ds) 
{
	return ds->day_alarm[1];
}

static void 
day_alarm2_write(DS3234 *ds,uint8_t data) 
{
	ds->day_alarm[1] = data;
	//update_alarm();
}

static uint8_t
control_read(DS3234 *ds) 
{
	return ds->control_reg;
}

/*
 * --------------------------------------------------------------------
 * Control register
 * 	Bit 7: nEOSC Enable oscillator when bat powered
 *	Bit 6: BBSQ enable interrupt/square wave when VCC absent
 *	Bit 5: Convert Temperature: Trigger meassurement
 *	Bit 3-4: Rate select for Square wave
 *	Bit 2:	Interrupt control
 *	Bit 1:	Alarm 2 Interrupt Enable
 *	Bit 0:	Alarm 1 Interrupt Enable
 * --------------------------------------------------------------------
 */
static void 
control_write(DS3234 *ds,uint8_t data) 
{
	uint8_t diff = data ^ ds->control_reg;
	ds->control_reg = data & ~(CONTROL_CONV);
	if(!(data & CONTROL_NEOSC) && (diff & CONTROL_NEOSC)) {
		update_time(ds);
	}
	if(diff & ~CONTROL_CONV) {
		ds_save_to_diskimage(ds);
	}
	if(data & CONTROL_CONV) {
		update_temperature(ds);
	}
}

/*
 * -------------------------------------------------------------------------
 * Status register
 * Bit 7: OSF flag Stop flag indicates that the oscillator was stoped
 * Bit 6: Battery backet 32kHz output (should be disabled);
 * Bit 4-5: Rate for temperature update 64s-512s
 * Bit 3: EN32KHZ: enable the 32khz output
 * Bit 1: A2F flag set when alarm 2 matched 
 * Bit 0: A1F flag set when alarm 1 matched 
 * ------------------------------------------------------------------------
 */
static uint8_t
status_read(DS3234 *ds) 
{
	return ds->status_reg;
}

static void 
status_write(DS3234 *ds,uint8_t data) 
{
	uint8_t clearmask  = data  & (STATUS_A2F | STATUS_A1F | STATUS_OSF);
	ds->status_reg = (data & ~(clearmask)) & (ds->status_reg & clearmask);
	ds_save_to_diskimage(ds);
}

static uint8_t
aging_read(DS3234 *ds) 
{
	fprintf(stderr,"aging register not implemented\n");
	return 0;
}

static void 
aging_write(DS3234 *ds,uint8_t data) 
{
	fprintf(stderr,"aging register not implemented\n");
}

static uint8_t
tempm_read(DS3234 *ds) 
{
	fprintf(stderr,"tempm register not implemented\n");
	return 0;
}

static void 
tempm_write(DS3234 *ds,uint8_t data) 
{
	fprintf(stderr,"DS3234: Temperature is not writable\n");
}

static uint8_t
templ_read(DS3234 *ds) 
{
	fprintf(stderr,"templ register not implemented\n");
	return 0;
}

static void 
templ_write(DS3234 *ds,uint8_t data) 
{
	fprintf(stderr,"DS3234: Temperature is not writable\n");
}

static uint8_t
disconv_read(DS3234 *ds) 
{
	fprintf(stderr,"disconv register not implemented\n");
	return 0;
}

static void 
disconv_write(DS3234 *ds,uint8_t data) 
{
	fprintf(stderr,"disconv register not implemented\n");
}

static uint8_t
sram_addr_read(DS3234 *ds) 
{
	return ds->sram_addr;
}

static void 
sram_addr_write(DS3234 *ds,uint8_t data) 
{
	ds->sram_addr = data;
}

static uint8_t
sram_data_read(DS3234 *ds) 
{
	return ds->sram[ds->sram_addr];
}

static void 
sram_data_write(DS3234 *ds,uint8_t data) 
{
	ds->sram[ds->sram_addr] = data;
}


static void 
write_reg(DS3234 *ds,uint8_t value,uint8_t addr) 
{
	addr &= 0x7f;
	switch(addr) {
		case REG_SECONDS:
			seconds_write(ds,value);
			break;

		case REG_MINUTES:
			minutes_write(ds,value);
			break;

		case REG_HOURS:
			hours_write(ds,value);
			break;

		case REG_DAY:
			day_write(ds,value);
			break;

		case REG_DATE:
			date_write(ds,value);
			break;

		case REG_MONTH:
			month_write(ds,value);
			break;

		case REG_YEAR:
			year_write(ds,value);
			break;

		case REG_ALARM1_SECONDS:
			seconds_alarm1_write(ds,value);
			break;

		case REG_ALARM1_MINUTES:
			minutes_alarm1_write(ds,value);
			break;

		case REG_ALARM1_HOUR:
			hour_alarm1_write(ds,value);
			break;

		case REG_ALARM1_DAY:
			day_alarm1_write(ds,value);
			break;

		case REG_ALARM2_MINUTES:
			minutes_alarm2_write(ds,value);
			break;

		case REG_ALARM2_HOUR:
			hour_alarm2_write(ds,value);
			break;

		case REG_ALARM2_DAY:
			day_alarm2_write(ds,value);
			break;

		case REG_CONTROL:
			control_write(ds,value);
			break;

		case REG_STATUS:
			status_write(ds,value);
			break;
		
		case REG_AGING:
			aging_write(ds,value);
			break;

		case REG_TEMPM:
			tempm_write(ds,value);
			break;

		case REG_TEMPL:
			templ_write(ds,value);
			break;

		case REG_DISCONV:
			disconv_write(ds,value);
			break;

		case REG_SRAM_ADDR:
			sram_addr_write(ds,value);
			break;

		case REG_SRAM_DATA:
			sram_data_write(ds,value);
			break;

		default:
			fprintf(stderr,"DS3234: Illegal register number 0x%02x\n",addr);
	}
}

static uint8_t 
read_reg(DS3234 *ds,uint8_t addr) 
{
	addr &= 0x7f;
	uint8_t value;
	switch(addr) {
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

		case REG_ALARM2_MINUTES:
			value = minutes_alarm2_read(ds);
			break;
		case REG_ALARM2_HOUR:
			value = hour_alarm2_read(ds);
			break;
		case REG_ALARM2_DAY:
			value = day_alarm2_read(ds);
			break;
		case REG_CONTROL:
			value = control_read(ds);
			break;
		case REG_STATUS:
			value = status_read(ds);
			break;
		case REG_AGING:
			value = aging_read(ds);
			break;
		case REG_TEMPM:
			value = tempm_read(ds);
			break;
		case REG_TEMPL:
			value = templ_read(ds);
			break;
		case REG_DISCONV:
			value = disconv_read(ds);
			break;
		case REG_SRAM_ADDR:
			value = sram_addr_read(ds);
			break;
		case REG_SRAM_DATA:
			value = sram_data_read(ds);
			break;
		default:
			value = 0;
			fprintf(stderr,"DS3234: access to nonexisting register\n");
			break;
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
sdi_latch(DS3234 *ds) 
{
	int val = SigNode_Val(ds->sdiNode);
	int bit = (val == SIG_HIGH) ? 1:0;
	ds->inval = (ds->inval<<1) | bit;
	ds->in_bitcount++;
	if(ds->in_bitcount == 8) {
		ds->reg_addr = ds->inval;
		dbgprintf("DS3234 register %02x\n",ds->reg_addr);
		if(ds->reg_addr & 0x80) {
			ds->direction = DIR_IN;
		} else {
			update_time(ds);
			ds->direction = DIR_OUT;
		}
		ds->inval = 0;
	} else if((ds->in_bitcount & 7) == 0) {
		if(ds->direction == DIR_IN) {
			dbgprintf("DS3234 write register value %02x to %02x\n",ds->inval,ds->reg_addr);
			write_reg(ds,ds->inval,ds->reg_addr);
			if(ds->reg_addr < 0x9f) {
				ds->reg_addr++;
			} else if(ds->reg_addr == 0x9f) {
				ds->reg_addr = 0x80;
			} else if(ds->reg_addr < 0xff) {
				ds->reg_addr++;
			} else if(ds->reg_addr == 0xff) {
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
sdo_shiftout(DS3234 *ds) 
{
	if(ds->direction == DIR_OUT) {
		if(ds->out_bitcount == 0) {
			ds->outval = read_reg(ds,ds->reg_addr);
			dbgprintf("Start shiftout of 0x%02x\n",ds->outval);
			ds->out_bitcount = 8;
			if(ds->reg_addr < 0x1f) {
				ds->reg_addr++;
			} else if(ds->reg_addr == 0x1f) {
				ds->reg_addr = 0;
			} else if(ds->reg_addr < 0x7f) {
				ds->reg_addr++;
			} else if(ds->reg_addr == 0x7f) {
				ds->reg_addr = 0x20;
			}
		}
		if(ds->outval & (1<<7)) {
			SigNode_Set(ds->sdoNode,SIG_HIGH);
		} else {
			SigNode_Set(ds->sdoNode,SIG_LOW);
		}
		ds->outval <<=1;
		ds->out_bitcount--;
	}  else {
		SigNode_Set(ds->sdoNode,SIG_OPEN);
	}
}

/*
 * --------------------------------------------------------------------------
 * SpiClk
 * 	The signal trace procedure invoked when the SPI-Clock line changes 
 * --------------------------------------------------------------------------
 */
static void 
SpiClk(SigNode *node,int value,void *clientData) 
{
	DS3234 *ds = (DS3234*) clientData;
	if(value == SIG_HIGH) {
		if(ds->cpol == 0) {
			sdo_shiftout(ds);
		} else {
			sdi_latch(ds);
		}
	} else if(value == SIG_LOW) {
		if(ds->cpol == 0) {
			sdi_latch(ds);
		} else {
			sdo_shiftout(ds);
		}
	}
}


/*
 * -----------------------------------------------------------
 * State change of Chip enable line
 * -----------------------------------------------------------
 */
static void 
SpiCs(SigNode *node,int value,void *clientData) 
{
	DS3234 *ds = (DS3234*) clientData;
	if(value == SIG_LOW) {
		dbgprintf("DS3234: GOT Chip enable\n");
		if(!ds->sclkTrace) {
			ds->sclkTrace = SigNode_Trace(ds->sclkNode,SpiClk,ds);
		}
		ds->reg_addr = 0;
		ds->in_bitcount = 0;
		ds->out_bitcount = 0;
		ds->direction =  DIR_NONE;
		SigNode_Set(ds->sdoNode,SIG_OPEN);
		if(SigNode_Val(ds->sclkNode) == SIG_HIGH) {	
			/* shift out on negedge, latch on posedge */
			ds->cpol =  1;
		} else {
			/* Shift out on posedge, latch on negedge */
			ds->cpol =  0;
		}
	} else if(value == SIG_HIGH) {
		dbgprintf("DS3234: GOT Chip disable\n");
		ds->direction =  DIR_NONE;
		if(ds->sclkTrace) {
			SigNode_Untrace(ds->sclkNode,ds->sclkTrace);
			ds->sclkTrace = NULL;
		}
	}
}
 /*
  * ---------------------------------------------------------------------------------
  * POR is defined as first application of power to the device either VBAT or VCC 
  * ---------------------------------------------------------------------------------
  */
static void
DS3234_SetPorValues(DS3234 *ds) {
	fprintf(stderr,"First Power application to DS3234 realtime clock\n");
	ds->status_reg  = STATUS_OSF | STATUS_EN32KHZ;
	ds->control_reg = CONTROL_RS2 | CONTROL_RS1 | CONTROL_INTCN;
	ds->aging = 0;
	ds->tempm = 0;
	ds->templ = 0;
	ds->disconv = 0;
	ds_save_to_diskimage(ds);
}
/*
 * -------------------------------------------------------------------------------
 * DS3234_New
 * Constructor for DS3234 RTC
 * Creates also a Pullup resistor because I'm to lazy to do it elsewhere
 * -------------------------------------------------------------------------------
 */
void
DS3234_New(const char *name) 
{
	char *dirname;
	char *imagename;
	DS3234 *ds = sg_new(DS3234);
	ds->sclkNode = SigNode_New("%s.sclk",name);
	ds->sdiNode = SigNode_New("%s.sdi",name);
	ds->sdoNode = SigNode_New("%s.sdo",name);
	ds->ceNode = SigNode_New("%s.ce",name);
	if(!ds->sclkNode || !ds->sdiNode || !ds->sdoNode || !ds->ceNode) {
		fprintf(stderr,"CSPI \"%s\": Signal line creation failed\n",name);
		exit(1);
	}
	ds->csTrace = SigNode_Trace(ds->ceNode,SpiCs,ds);
	dirname=Config_ReadVar("global","imagedir");
        if(dirname) {
                imagename = alloca(strlen(dirname) + strlen(name) + 20);
                sprintf(imagename,"%s/%s.img",dirname,name);
                ds->disk_image = DiskImage_Open(imagename,8+DS3234_RAMSIZE,DI_RDWR | DI_CREAT_00);
                if(!ds->disk_image) {
                        fprintf(stderr,"Failed to open DS3234 time offset file, using offset 0\n");
			ds->time_offset = 0;
                } else {
			if(ds_load_from_diskimage(ds)<0) {
				DS3234_SetPorValues(ds);
			}
			if(ds->control_reg &  CONTROL_NEOSC) {
				ds->status_reg |= STATUS_OSF;
			}
                }
        } else {
		ds->time_offset = 0;
        }
	CycleTimer_Add(&ds->TemperatureUpdateTimer,MillisecondsToCycles(5000),temp_meass_timer,ds);
	fprintf(stderr,"DS3234 created with time offset %lld\n",(unsigned long long)ds->time_offset);
	update_time(ds);
}
