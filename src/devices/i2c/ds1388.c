/*
 *************************************************************************************************
 *
 * Emulation of DS1388 I2C-Realtime Clock (I2C address 0x68)
 *
 * State: watchdog missing, eeprom missing, recommended to use 2
 * 	  separate eeprom with 8 Byte write buffer.
 *
 * Copyright 2006 2010 Jochen Karrer. All rights reserved.
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
#include "ds1388.h"
#include "configfile.h"
#include "diskimage.h"
#include "sgstring.h"

#if 0
#define dbgprintf(x...) { fprintf(stderr,x); }
#else
#define dbgprintf(x...)
#endif

#define DS_STATE_ADDR (0)
#define DS_STATE_DATA  (2)

#define REG_HUNSEC		(0)
#define REG_SECOND		(1)
#define REG_MINUTE		(2)
#define REG_HOUR		(3)
#define         HOURS_12H       (1<<6)
#define         HOURS_PM        (1<<5)
#define REG_DAY			(4)
#define	REG_DATE		(5)
#define REG_MONTH		(6)
#define REG_YEAR		(7)
#define REG_WDG_HUNSEC		(8)
#define REG_WDG_SEC		(9)
#define REG_TRICKLE_CHARGE	(10)
#define		TCS_MAGIC_MSK	(0xf0)
#define		TCS_MAGIC	(0xa0)
#define		ROUT_250	(1)
#define		ROUT_2K		(2)
#define		ROUT_4K		(3)
#define		DS_NONE		(1 << 2)
#define		DS_ON		(1 << 3)
#define REG_FLAG		(11)
#define		FLAG_OSF	(1 << 7)
#define		FLAG_WF		(1 << 6)
#define REG_CONTROL		(12)
#define		CONTROL_NEOSC	(1<<7)
#define		CONTROL_INTCN	(1<<2)
#define		CONTROL_WDE	(1<<1)
#define		CONTROL_WDnRST	(1<<0)

#define DS1388_IMAGESIZE (128)

struct DS1388 {
	I2C_Slave i2c_slave;
	const char *name;
	uint16_t reg_address;
	int state;
	int direction; // copy of I2C operation at start condition

	uint32_t useconds; /* internal only */
	/* The registers */
	uint8_t hunsec;
	uint8_t second;
	uint8_t minute;
	uint8_t hour;
	uint8_t day;
	uint8_t day_offset; /* difference between real day belonging to this date and day reg */
	uint8_t date;
	uint8_t month;
	uint8_t year;
	uint8_t control_reg;
	uint8_t trcharge_reg;
	uint8_t flag_reg;

	DiskImage *disk_image;
	int64_t time_offset; /* time difference to host clock */
	int time_changed_flag;
	uint32_t chargeable;
};

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

/*
 * ---------------------------------------------------------
 * return the difference to system time in microseconds
 * ---------------------------------------------------------
 */
static int64_t
diff_systime(DS1388 *ds) {
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
        if(ds->hour & HOURS_12H) {
                uint8_t hour = bcd_to_i(ds->hour & 0x1f) % 12;
                if(ds->hour & HOURS_PM) {
                        tm.tm_hour = hour + 12;
                } else {
                        tm.tm_hour = hour;
                }
        } else {
                tm.tm_hour = bcd_to_i(ds->hour & 0x3f);
        }
        tm.tm_min = bcd_to_i(ds->minute);
        tm.tm_sec = bcd_to_i(ds->second);
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
        //fprintf(stderr,"rtc %ld, sys %ld, calculated offset %lld\n",rtc_time,sys_utc_time,offset);
        return offset;
}

/*
 * ------------------------------------------------------------------
 * update_time 
 * 	Read new time from system, correct it with the offset 
 *	and put it to the DS1388 registers
 *	Called on I2C-Start with direction READ	and on
 * 	address wrap when reading.
 * ------------------------------------------------------------------
 */
static void
update_time(DS1388 *ds) {
	struct timeval host_tv;
	time_t time;
	struct tm tm;
	/* 
	 *******************************************************************
	 * With real device the Oscillator is stopped only on VCC loss !
	 * So this is wrong !
	 *******************************************************************
	 */
	if(ds->control_reg & CONTROL_NEOSC) {
		return;
	} 
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
			fprintf(stderr,"DS1388: Illegal year %d\n",tm.tm_year+1900);
	}
	ds->year = i_to_bcd((tm.tm_year-100) % 100);
	ds->month = i_to_bcd(tm.tm_mon+1);
	ds->day = i_to_bcd(((tm.tm_wday + 6) % 7) + 1);
	ds->day = (((ds->day - 1) + ds->day_offset) % 7) + 1;
	ds->date = i_to_bcd(tm.tm_mday);
  	if(ds->hour & HOURS_12H) {
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
			ds->hour = (ds->hour & 0xe0) | i_to_bcd(hour);
			if(pm) {
					ds->hour |= HOURS_PM;
			}
	} else {
			ds->hour = i_to_bcd(tm.tm_hour);
	}
	ds->minute = i_to_bcd(tm.tm_min);
	ds->second = i_to_bcd(tm.tm_sec);
	ds->hunsec = i_to_bcd(ds->useconds / 10000);
	//fprintf(stderr,"UDT: Year %d, hour %d minute %d\n",tm.tm_year+1900,tm.tm_hour,tm.tm_min);
	return;
}

static int
ds_load_from_diskimage(DS1388 *ds)
{
	uint8_t data[DS1388_IMAGESIZE];
	uint8_t *buf = data;
	int i;
	int count = DS1388_IMAGESIZE;
	if(!ds->disk_image) {
	fprintf(stderr,"Warning, no diskimage for DS1388 RTC\n");
			return -1;
	}
	if(DiskImage_Read(ds->disk_image,0,buf,count)<count) {
			fprintf(stderr,"Error reading from DS1388 disk image\n");
			return -1;
	}
	if((*buf++ != 0x71) || (*buf++ != 0x2d)) {
			fprintf(stderr,"DS1388 invalid magic, starting with defaults\n");
			ds->time_offset = 0;
	ds->day_offset = 0;
			return -1;
	}
	ds->time_offset = 0;
	for(i=0;i<8;i++) {
		ds->time_offset |= ((uint64_t)buf[0])  << (i*8);
		buf++;
	}
	/* 
 	 * --------------------------------------------------------------
	 * Time is read back from file also because clock might be in
 	 * stopped state
 	 * --------------------------------------------------------------
 	 */
	ds->second = *buf++;
	ds->minute = *buf++;
	ds->hour = *buf++;
	ds->day_offset = *buf++;
	ds->date = *buf++;
	ds->month = *buf++;
	ds->year = *buf++;
	ds->control_reg = *buf++;
	ds->flag_reg = *buf++;
	ds->trcharge_reg = *buf++;
	/* 
 	 * -------------------------------------------------------
	 * do not use the stored time offset when the oscillator
	 * is not enabled
 	 * -------------------------------------------------------
	 */ 
	if(ds->control_reg & CONTROL_NEOSC) {
		ds->time_offset = diff_systime(ds);
	}
        return 0;
}


static void
ds_save_to_diskimage(DS1388 *ds)
{
	uint8_t data[DS1388_IMAGESIZE];
	uint8_t *buf = data;
	int i;
	dbgprintf("DS1388 save to diskimage\n");
	*buf++ = 0x71;
	*buf++ = 0x2d;
	for(i=0;i<8;i++) {
			*(buf++) = (ds->time_offset >> (i*8)) & 0xff;
	}
	*(buf++) = ds->second;
	*(buf++) = ds->minute;
	*(buf++) = ds->hour;
	*(buf++) = ds->day_offset;
	*(buf++) = ds->date;
	*(buf++) = ds->month;
	*(buf++) = ds->year;
	*(buf++) = ds->control_reg;
	*(buf++) = ds->flag_reg;
	*(buf++) = ds->trcharge_reg;
	if(!ds->disk_image) {
		fprintf(stderr,"Warning, no diskimage for DS1388 RTC\n");
		return;
	}
	if(DiskImage_Write(ds->disk_image,0,data,buf-data)<(buf-data)) {
			fprintf(stderr,"Error writing to DS1388 disk image\n");
			return;
	}
	return;
}

static uint8_t
hunsec_read(DS1388 *ds)
{
        return ds->hunsec;
}

static void
hunsec_write(DS1388 *ds,uint8_t data)
{
        ds->hunsec = data;
        ds->useconds = 0;
        ds->time_changed_flag = 1;
}
static uint8_t
second_read(DS1388 *ds)
{
        return ds->second;
}

static void
second_write(DS1388 *ds,uint8_t data)
{
        ds->second = data;
        ds->useconds = 0;
	dbgprintf("DS1388 setting seconds to %d\n",data);
        ds->time_changed_flag = 1;
}

static uint8_t
minute_read(DS1388 *ds)
{
        return ds->minute;
}

static void
minute_write(DS1388 *ds,uint8_t data)
{
        ds->minute = data;
        ds->time_changed_flag = 1;
}

static uint8_t
hour_read(DS1388 *ds)
{
        return ds->hour;
}

static void
hour_write(DS1388 *ds,uint8_t data)
{
        ds->hour = data;
        ds->time_changed_flag = 1;
}

static uint8_t
day_read(DS1388 *ds)
{
        return ds->day;
}

static void
day_write(DS1388 *ds,uint8_t data)
{
        ds->day = data;
	ds->day_offset = (7 + (data - 1) - (ds->day - 1)) % 7;
        ds->time_changed_flag = 1;
}

static uint8_t
date_read(DS1388 *ds)
{
        return ds->date;
}

static void
date_write(DS1388 *ds,uint8_t data)
{
        ds->date = data;
        ds->time_changed_flag = 1;
}

static uint8_t
month_read(DS1388 *ds)
{
        return ds->month;
}

static void
month_write(DS1388 *ds,uint8_t data)
{
        ds->month = data;
        ds->time_changed_flag = 1;
}

static uint8_t
year_read(DS1388 *ds)
{
        return ds->year;
}

static void
year_write(DS1388 *ds,uint8_t data)
{
        ds->year = data;
        ds->time_changed_flag = 1;
}


static uint8_t
control_read(DS1388 *ds)
{
        return ds->control_reg;
}

/*
 * --------------------------------------------------------------------
 * Control register
 * --------------------------------------------------------------------
 */
static void
control_write(DS1388 *ds,uint8_t data)
{
        uint8_t diff = data ^ ds->control_reg;
        update_time(ds);
        ds->control_reg = data & 0x9f;
        if((diff & CONTROL_NEOSC) && !(data & CONTROL_NEOSC)) {
			ds->time_offset = diff_systime(ds);
			ds_save_to_diskimage(ds);
        } else if((diff & CONTROL_NEOSC) && (data & CONTROL_NEOSC)) {
			/* 
			 *******************************************************************
			 * With real device the Oscillator is stopped only on VCC loss !
			 *******************************************************************
			 */
		}
}

static uint8_t
trcharge_read(DS1388 *ds)
{
	return ds->trcharge_reg;
}
static void
trcharge_write(DS1388 *ds,uint8_t data)
{
	uint8_t diode_select = (data >> 2) & 3;
	uint8_t rout = data & 3;
	uint8_t disabled = 0;
	if((diode_select == 0) || (diode_select == 3)) {
		disabled = 1;
	}
	if(rout == 0) {
		disabled = 1;
	}
	ds->trcharge_reg = data;
	if(!disabled & !ds->chargeable) {
		fprintf(stderr,"Bug: \"%s\": Charging non Chargeable Battery\nTrickle Charge 0x%02x\n",ds->name,data);
		sleep(1);
		exit(1);
	} 
}

/*
 * -------------------------------------------------------------------------
 * Status register
 * ------------------------------------------------------------------------
 */
static uint8_t
flag_read(DS1388 *ds)
{
        return ds->flag_reg;
}

static void
flag_write(DS1388 *ds,uint8_t data)
{
	uint8_t clearmask  = (FLAG_OSF | FLAG_WF);
	ds->flag_reg = (ds->flag_reg & ~clearmask) & (data & clearmask);
}


static void
write_reg(DS1388 *ds,uint8_t value,uint8_t addr)
{
        addr &= 0x7f;
        switch(addr) {
                case REG_HUNSEC:
                        hunsec_write(ds,value);
                        break;
                case REG_SECOND:
                        second_write(ds,value);
                        break;

                case REG_MINUTE:
                        minute_write(ds,value);
                        break;

                case REG_HOUR:
                        hour_write(ds,value);
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

                case REG_CONTROL:
                        control_write(ds,value);
                        break;

                case REG_TRICKLE_CHARGE:
                        trcharge_write(ds,value);
                        break;

                case REG_FLAG:
                        flag_write(ds,value);
                        break;

                default:
                        fprintf(stderr,"DS1388: Illegal register number 0x%02x\n",addr);
        }
}


static uint8_t
read_reg(DS1388 *ds,uint8_t addr)
{
        addr &= 0x7f;
        uint8_t value;
        switch(addr) {
                case REG_HUNSEC:
                        value = hunsec_read(ds);
			break;
                case REG_SECOND:
                        value = second_read(ds);
                        break;
                case REG_MINUTE:
                        value = minute_read(ds);
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
		case REG_CONTROL:
                        value = control_read(ds);
                        break;
                case REG_TRICKLE_CHARGE:
                        value = trcharge_read(ds);
                        break;
                case REG_FLAG:
                        value = flag_read(ds);
                        break;
                default:
                        value = 0;
                        //fprintf(stderr,"DS1388: access to nonexisting register\n");
                        break;
        }
        return value;
}

static int 
ds1388_read(void *dev,uint8_t *data) 
{
	DS1388 *ds = dev;
	*data = read_reg(ds,ds->reg_address);
	dbgprintf("DS1388 read 0x%02x from %04x\n",*data,ds->reg_address);
	ds->reg_address = (ds->reg_address + 1);
	if(ds->reg_address > 0xc) {
		ds->reg_address = 0;
		update_time(ds);
	}
	return I2C_DONE;
};

/*
 **************************************************************
 * DS1388 Write state machine 
 * Real device doesn't ACK addresses > 0xc.
 **************************************************************
 */
static int 
ds1388_write(void *dev,uint8_t data) {
	DS1388 *ds = dev;
	if(ds->state==DS_STATE_ADDR) {
		dbgprintf("DS1388 Addr 0x%02x\n",data);
		if(data > 0xc) {
			return I2C_NACK;
		}
		ds->reg_address = data;
		ds->state = DS_STATE_DATA;
	} else if(ds->state==DS_STATE_DATA) {
		dbgprintf("DS1388 Write 0x%02x to reg 0x%04x\n",data,ds->reg_address);
		write_reg(ds,data,ds->reg_address);	
		ds->reg_address++;
		/* Page 10 in manual says it wraps to 0 after 0xc */
		if(ds->reg_address > 0xc) {
			ds->reg_address = 0;
			//update_time(ds);
		}
	}
	return I2C_ACK;
};

static int
ds1388_start(void *dev,int i2c_addr,int operation) {
	DS1388 *ds = dev;
	dbgprintf("DS1388 start\n");
	ds->state = DS_STATE_ADDR;
	ds->direction = operation;
	update_time(ds); 
	ds->time_changed_flag = 0;
	return I2C_ACK;
}

static void 
ds1388_stop(void *dev) {
	DS1388 *ds = dev;
	dbgprintf("DS1388 stop\n");
	if(ds->direction == I2C_WRITE) {
		if(ds->time_changed_flag) {
			ds->time_offset = diff_systime(ds);
		}
		ds_save_to_diskimage(ds);
	}
	ds->state =  DS_STATE_ADDR; 
}

/*
 ***********************************************************
 * ds1388_ops 
 *	I2C-Operations provided by the DS1388 
 ***********************************************************
 */

static I2C_SlaveOps ds1388_ops = {
	.start = ds1388_start,
	.stop =  ds1388_stop,
	.read =  ds1388_read,	
	.write = ds1388_write	
};

/**
 *************************************************************************
 * \fn I2C_Slave * DS1388_New(char *name); 
 * Create a new DS1388 realtime clock
 *************************************************************************
 */
I2C_Slave *
DS1388_New(char *name) {
	DS1388 *ds = sg_new(DS1388); 
	char *dirname,*imagename;
	I2C_Slave *i2c_slave;
	ds->name = sg_strdup(name);
	ds->chargeable = 0;
	Config_ReadUInt32(&ds->chargeable,name,"chargeable");	
	dirname=Config_ReadVar("global","imagedir");
	if(dirname) {
		imagename = alloca(strlen(dirname) + strlen(name) + 20);
		sprintf(imagename,"%s/%s.img",dirname,name);
		ds->disk_image = DiskImage_Open(imagename,DS1388_IMAGESIZE,DI_RDWR | DI_CREAT_00);
		if(!ds->disk_image) {
			fprintf(stderr,"Failed to open DS1388 time offset file, using offset 0\n");
			ds->time_offset = 0;
		} else {
			if(ds_load_from_diskimage(ds) < 0) {
				ds->flag_reg |= FLAG_OSF;
			}
		}
	} else {
		ds->time_offset = 0;
	}
	if(ds->control_reg & CONTROL_NEOSC) {
		ds->flag_reg |= FLAG_OSF;
	}
	i2c_slave = &ds->i2c_slave;
	i2c_slave->devops = &ds1388_ops; 
	i2c_slave->dev = ds;
	i2c_slave->speed = I2C_SPEED_FAST;
	update_time(ds); 
	fprintf(stderr,"DS1388 Real Time Clock \"%s\" timeoffset %lld usec\n",name,(unsigned long long)diff_systime(ds));
	return i2c_slave;
}
