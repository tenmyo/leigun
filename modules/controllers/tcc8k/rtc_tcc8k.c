/*
 *************************************************************************************************
 *
 * Emulation of TCC8000 Realtime Clock 
 *
 * State: not working
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

#include <unistd.h>
#include <stdint.h>
#include <sys/time.h>
#include <time.h>
#include "bus.h"
#include "rtc_tcc8k.h"
#include "sgstring.h"
#include "signode.h"
#include "clock.h"
#include "cycletimer.h"
#include "configfile.h"
#include "diskimage.h"

#define RTC_IMAGESIZE	(128)

#define REG_RTCCON(base)      ((base) + 0)
#define         RTCCON_WUOUTEN  (1 << 7)
#define         RTCCON_AIOUTEN  (1 << 6)
#define         RTCCON_OSCEN    (1 << 5)
#define         RTCCON_CLKRST   (1 << 4)
#define         RTCCON_CNTSEL   (1 << 3)
#define         RTCCON_CLKSEL   (1 << 2)
#define         RTCCON_RTCWEN   (1 << 1)
#define         RTCCON_STARTB   (1 << 0)
#define REG_INTCON(base)	((base) + 0x04)
#define         INTCON_PROT             (1 << 15)
#define         INTCON_XDRV_MSK         (3 << 12)
#define         INTCON_XDRV_32768       (0 << 12)
#define         INTCON_XDRV_4194304     (3 << 12)
#define         INTCON_FSEL_MSK         (7 << 8)
#define         INTCON_FSEL_32768       (0 << 8)
#define         INTCON_FSEL_4194304     (7 << 8)
#define         INTCON_INTWREN          (1 << 0)
#define REG_RTCALM(base)	((base) + 0x08)
#define         RTCALM_ALMEN    (1 << 7)
#define         RTCALM_YEAREN   (1 << 6)
#define         RTCALM_MONEN    (1 << 5)
#define         RTCALM_DAYEN    (1 << 4)
#define         RTCALM_DATEEN   (1 << 3)
#define         RTCALM_HOUREN 	(1 << 2)
#define         RTCALM_MINEN	(1 << 1)
#define         RTCALM_SECEN	(1 << 0)
#define REG_ALMSEC(base)	((base) + 0x0c)
#define REG_ALMMIN(base)	((base) + 0x10)
#define REG_ALMHOUR(base)	((base) + 0x14)
#define REG_ALMDATE(base)	((base) + 0x18)
#define REG_ALMDAY(base)	((base) + 0x1c)
#define REG_ALMMON(base)	((base) + 0x20)
#define REG_ALMYEAR(base)	((base) + 0x24)
#define REG_BCDSEC(base)	((base) + 0x28)
#define REG_BCDMIN(base)	((base) + 0x2c)
#define REG_BCDHOUR(base)	((base) + 0x30)
#define REG_BCDDATE(base)	((base) + 0x34)
#define REG_BCDDAY(base)	((base) + 0x38)
#define REG_BCDMON(base)	((base) + 0x3C)
#define REG_BCDYEAR(base)	((base) + 0x40)
#define REG_RTCIM(base)		((base) + 0x44)
#define REG_RTCPEND(base)	((base) + 0x48)
#define REG_RTCAPB(base)	((base) + 0x100)

typedef struct TccRtc {
	BusDevice bdev;
	DiskImage *disk_image;
	int64_t time_offset;	/* time difference to host clock */
	//int time_changed_flag;

	uint32_t regRtccon;
	uint32_t regIntcon;
	uint32_t regRtcalm;
	uint32_t regAlmsec;
	uint32_t regAlmmin;
	uint32_t regAlmhour;
	uint32_t regAlmdate;
	uint32_t regAlmday;
	uint32_t regAlmmon;
	uint32_t regAlmyear;
	uint32_t regBcdsec;
	uint32_t regBcdmin;
	uint32_t regBcdhour;
	uint32_t regBcddate;
	uint32_t regBcdday;
	uint32_t regBcdmon;
	uint32_t regBcdyear;
	uint32_t regRtcim;
	uint32_t regRtcpend;
	uint32_t regRtcapb;
	SigNode *sigIrq;
} TccRtc;

/*
 * -------------------------------------------------
 * Conversion between BCD and 8Bit Integer
 * -------------------------------------------------
 */
static inline uint8_t
bcd_to_u8(uint8_t b)
{
	return (b & 0xf) + 10 * ((b >> 4) & 0xf);
}

static inline uint16_t
bcd_to_u16(uint16_t b)
{
	uint16_t result = bcd_to_u8(b & 0xff) + ((uint16_t) bcd_to_u8(b >> 8)) * 100;
	return result;
}

static inline uint8_t
u8_to_bcd(uint8_t b)
{
	return (b / 10) * 16 + (b - 10 * (b / 10));
}

static inline uint16_t
u16_to_bcd(uint16_t b)
{
	return u8_to_bcd(b % 100) | ((u8_to_bcd(b / 100)) << 8);
}

static void
get_tm_now(TccRtc * rtc, struct tm *tm)
{
	tm->tm_isdst = -1;
	tm->tm_mon = bcd_to_u8(rtc->regBcdmon) - 1;
	tm->tm_mday = bcd_to_u8(rtc->regBcddate);
	if ((rtc->regBcdyear >= 0x1900) & (rtc->regBcdyear < 0x2100)) {
		tm->tm_year = bcd_to_u16(rtc->regBcdyear) - 1900;
	} else {
		tm->tm_year = 0;
	}
	tm->tm_hour = bcd_to_u8(rtc->regBcdhour & 0x3f);

	tm->tm_min = bcd_to_u8(rtc->regBcdmin);
	tm->tm_sec = bcd_to_u8(rtc->regBcdsec);
}

/*
 *****************************************************************************************
 * Calculate the difference to the host system time.
 *****************************************************************************************
 */
static int64_t
diff_systime(TccRtc * rtc)
{
	struct timeval now;
	char *zone;
	int64_t offset;
	time_t rtc_time;
	time_t sys_utc_time;
	struct tm tm;
	get_tm_now(rtc, &tm);
#if 0
	tm.tm_isdst = -1;
	tm.tm_mon = bcd_to_u8(rtc->regBcdmon) - 1;
	tm.tm_mday = bcd_to_u8(rtc->regBcddate);
	if ((rtc->regBcdyear >= 0x1900) & (rtc->regBcdyear < 0x2100)) {
		tm.tm_year = bcd_to_u16(rtc->regBcdyear) - 1900;
	} else {
		tm.tm_year = 0;
	}
	tm.tm_hour = bcd_to_u8(rtc->regBcdhour & 0x3f);

	tm.tm_min = bcd_to_u8(rtc->regBcdmin);
	tm.tm_sec = bcd_to_u8(rtc->regBcdsec);
#endif
#if 0
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
#endif
	//fprintf(stderr,"Using year %d, month, %d, day %d hour %d minute %d,sec %d\n",tm.tm_year,tm.tm_mon,tm.tm_mday,tm.tm_min,tm.tm_hour,tm.tm_sec);
	/* Shit, there is no mktime which does GMT */
	zone = getenv("TZ");	/* remember original */
	setenv("TZ", "", 1);	/* "UTC" */
	tzset();
	rtc_time = mktime(&tm);
	if (zone) {
		setenv("TZ", zone, 1);
	} else {
		unsetenv("TZ");
	}
	tzset();
	if (rtc_time == -1) {
		fprintf(stderr, "mktime failed\n");
		return 0;
	}
	gettimeofday(&now, NULL);
	sys_utc_time = now.tv_sec;

	offset = ((int64_t) rtc_time - (int64_t) sys_utc_time) * (int64_t) 1000000;
	//offset += rtc->useconds;
	offset -= now.tv_usec;
	//fprintf(stderr,"rtc %ld, sys %ld, calculated offset %lld\n",rtc_time,sys_utc_time,offset);
	return offset;
}

static int
rtc_load_from_diskimage(TccRtc * rtc)
{
	uint8_t data[RTC_IMAGESIZE];
	uint8_t *buf = data;
	int i;
	int count = RTC_IMAGESIZE;
	if (!rtc->disk_image) {
		fprintf(stderr, "Warning, no diskimage for TCC8000 RTC\n");
		return -1;
	}
	if (DiskImage_Read(rtc->disk_image, 0, buf, count) < count) {
		fprintf(stderr, "Error reading from TCC8000 RTC disk image\n");
		return -1;
	}
	if ((*buf++ != 0xef) || (*buf++ != 0xca)) {
		fprintf(stderr, "TCC8000 RTC invalid magic, starting with defaults\n");
		rtc->time_offset = 0;
		return -1;
	}
	rtc->time_offset = 0;
	for (i = 0; i < 8; i++) {
		rtc->time_offset |= ((uint64_t) buf[0]) << (i * 8);
		buf++;
	}
	/* Now read the registers in the correct sequence from the file */
	rtc->regRtccon = *buf++;
	rtc->regIntcon = *buf++;
	rtc->regIntcon |= (*buf++) << 8;
	rtc->regRtcalm = *buf++;
	rtc->regAlmsec = *buf++;
	rtc->regAlmmin = *buf++;
	rtc->regAlmhour = *buf++;
	rtc->regAlmdate = *buf++;
	rtc->regAlmday = *buf++;
	rtc->regAlmmon = *buf++;
	rtc->regAlmyear = *buf++;

	/* 
	 * --------------------------------------------------------------
	 * Time is read back from file also because clock might be in
	 * stopped state
	 * --------------------------------------------------------------
	 */
	rtc->regBcdsec = *buf++;
	rtc->regBcdmin = *buf++;
	rtc->regBcdhour = *buf++;
	rtc->regBcddate = *buf++;
	rtc->regBcdday = *buf++;
	rtc->regBcdmon = *buf++;
	rtc->regBcdyear = *buf++;
	rtc->regBcdyear |= (*buf++) << 8;
	/* 
	 * -------------------------------------------------------
	 * do not use the stored time offset when the oscillator
	 * is not enabled
	 * -------------------------------------------------------
	 */
	if (rtc->regRtccon & RTCCON_STARTB) {
		rtc->time_offset = diff_systime(rtc);
	}
	return 0;
}

static void
rtc_save_to_diskimage(TccRtc * rtc)
{
	uint8_t data[RTC_IMAGESIZE];
	uint8_t *buf = data;
	int i;
	*buf++ = 0xef;
	*buf++ = 0xca;
	for (i = 0; i < 8; i++) {
		*(buf++) = (rtc->time_offset >> (i * 8)) & 0xff;
	}
	*(buf++) = rtc->regRtccon;
	*(buf++) = rtc->regIntcon & 0xff;
	*(buf++) = rtc->regIntcon >> 8;
	*(buf++) = rtc->regRtcalm;
	*(buf++) = rtc->regAlmsec;
	*(buf++) = rtc->regAlmmin;
	*(buf++) = rtc->regAlmhour;
	*(buf++) = rtc->regAlmdate;
	*(buf++) = rtc->regAlmday;
	*(buf++) = rtc->regAlmmon;
	*(buf++) = rtc->regAlmyear;

	/* Save time also for case of stopped clock */
	*(buf++) = rtc->regBcdsec;
	*(buf++) = rtc->regBcdmin;
	*(buf++) = rtc->regBcdhour;
	*(buf++) = rtc->regBcddate;
	*(buf++) = rtc->regBcdday;
	*(buf++) = rtc->regBcdmon;
	*(buf++) = rtc->regBcdyear & 0xff;
	*(buf++) = rtc->regBcdyear >> 8;
	if (!rtc->disk_image) {
		fprintf(stderr, "Warning, no diskimage for TCC8000 RTC\n");
		return;
	}
	if (DiskImage_Write(rtc->disk_image, 0, data, buf - data) < (buf - data)) {
		fprintf(stderr, "Error writing to TCC8000 RTC disk image\n");
		return;
	}
	return;
}

/*
 ***************************************************************************
 * update_time 
 *      Read new time from system, correct it with the offset 
 *      and put it to the TCC8000 rtc registers
 ***************************************************************************
 */
static void
update_time(TccRtc * rtc)
{
	struct timeval host_tv;
	time_t time;
	struct tm tm;
	uint32_t useconds;
	if (rtc->regRtccon & RTCCON_STARTB) {
		return;
	}
	gettimeofday(&host_tv, NULL);
	time = host_tv.tv_sec;
	time += (rtc->time_offset / (int64_t) 1000000);
	useconds = host_tv.tv_usec + (rtc->time_offset % 1000000);
	if (useconds > 1000000) {
		time++;
	}
	//fprintf(stderr,"Gettimeofday %lu seconds\n",time);
	gmtime_r(&time, &tm);
	if (tm.tm_year < 200) {
		rtc->regBcdyear = u16_to_bcd((tm.tm_year + 1900));
	} else {
		fprintf(stderr, "TCC8000: Illegal year %d\n", tm.tm_year + 1900);
	}
	rtc->regBcdmon = u8_to_bcd(tm.tm_mon + 1);
	rtc->regBcdday = u8_to_bcd(tm.tm_wday);
	rtc->regBcddate = u8_to_bcd(tm.tm_mday);
	rtc->regBcdhour = u8_to_bcd(tm.tm_hour);
	rtc->regBcdmin = u8_to_bcd(tm.tm_min);
	rtc->regBcdsec = u8_to_bcd(tm.tm_sec);
	//fprintf(stderr,"UDT: Year %d, hour %d minute %d sec %d\n",tm.tm_year+1900,tm.tm_hour,tm.tm_min,tm.tm_sec);
	//fprintf(stderr,"bcdyear %04x, mon %02x date %02x hour %02x min %02x sec %02x\n",
	//      rtc->regBcdyear,rtc->regBcdmon,rtc->regBcddate,rtc->regBcdhour,rtc->regBcdmin,rtc->regBcdsec);
	return;
}

/*
 */
static void
tm_init(struct tm *tm)
{
	tm->tm_isdst = -1;
	tm->tm_mon = 1;
	tm->tm_mday = 1;
	tm->tm_year = 0;
	tm->tm_hour = 0;
	tm->tm_min = 0;
	tm->tm_sec = 0;
}

static void
update_timeout(TccRtc * rtc)
{
	//int64_t timeout_usec = 0;
	struct tm tmout;
	if (!(rtc->regRtcalm & RTCALM_ALMEN)) {
		// Cancel the timer;
		return;
	}
	// if clock not running
	// cancel the timeout
	tm_init(&tmout);
	if (rtc->regRtcalm & RTCALM_YEAREN) {
		if ((rtc->regAlmyear >= 0x1900) & (rtc->regAlmyear < 0x2100)) {
			tmout.tm_year = bcd_to_u16(rtc->regAlmyear) - 1900;
		} else {
			tmout.tm_year = 0;
		}
		if (rtc->regAlmyear > rtc->regBcdyear) {
			// goto sleep waiting for the year only
			return;
		} else if (rtc->regAlmyear < rtc->regBcdyear) {
			// cancel the timeout
		} else {
			// continue checking the month
		}
	} else {
		/* If it doesn't matter take this year */
		tmout.tm_year = bcd_to_u8(rtc->regBcdyear);
	}
	if (rtc->regRtcalm & RTCALM_MONEN) {
		if (rtc->regAlmmon > rtc->regBcdmon) {
			tmout.tm_mon = bcd_to_u8(rtc->regAlmmon) - 1;
			// goto sleep waiting for the Month
		} else if (rtc->regAlmmon < rtc->regBcdmon) {
			int year = bcd_to_u16(rtc->regBcdyear) + 1;
			if ((year > 1900) & (year < 2100)) {
				tmout.tm_year = year - 1900;
			} else {
				// cancel timeout
			}
			// goto sleep waiting for the next year
		} else {
			tmout.tm_mon = bcd_to_u8(rtc->regAlmmon) - 1;
			// continue checking the date
		}
	} else {
		/* If it doesn't matter take this month */
		tmout.tm_mon = bcd_to_u8(rtc->regBcdmon);
	}
	if (rtc->regRtcalm & RTCALM_DATEEN) {
		tmout.tm_mday = bcd_to_u8(rtc->regAlmdate);
		if (rtc->regAlmdate > rtc->regBcddate) {
			// goto sleep waiting for the date
		} else if (rtc->regAlmdate < rtc->regBcddate) {
			// goto sleep waiting for the next month
		} else {
			// continue checking the hour
		}
	} else {
		/* if it doesn't matter take this day */
		tmout.tm_mday = bcd_to_u8(rtc->regBcddate);
	}
	if (rtc->regRtcalm & RTCALM_HOUREN) {
		tmout.tm_hour = bcd_to_u8(rtc->regAlmhour);
		if (rtc->regAlmhour > rtc->regBcdhour) {
			// goto sleep waiting for the hour
		} else if (rtc->regAlmhour < rtc->regBcdhour) {
			// goto sleep waiting for the next day
		} else {
			// continue checking the minute
		}
	} else {
		/* If it doesn't matter take this hour */
		tmout.tm_hour = bcd_to_u8(rtc->regBcdhour);
	}
	if (rtc->regRtcalm & RTCALM_MINEN) {
		tmout.tm_sec = bcd_to_u8(rtc->regAlmmin);
		if (rtc->regAlmmin > rtc->regBcdmin) {
			// goto sleep waiting for the minute
		} else if (rtc->regAlmmin < rtc->regBcdmin) {
			// goto sleep waiting for the next hour 
		} else {
			// continue checking the second
		}
	} else {
		/* If it doesn't matter take this minute */
		tmout.tm_sec = bcd_to_u8(rtc->regBcdmin);
	}
	if (rtc->regRtcalm & RTCALM_SECEN) {
		tmout.tm_sec = bcd_to_u8(rtc->regAlmsec);
		if (rtc->regAlmsec > rtc->regBcdsec) {
			// goto sleep waiting for the second 
		} else if (rtc->regAlmsec < rtc->regBcdsec) {
			// goto sleep waiting for the next minute 
		} else {
			// reached, trigger event
		}
	} else {
		tmout.tm_sec = 0;
	}
#if 0
	if ((rtc->regRtcalm & RTCALM_DAYEN)) {
		tm.tm_hour = bcd_to_u8(rtc->regBcdhour);
	}
#endif

#define REG_RTCALM(base)	((base) + 0x08)
}

static int
time_is_writable(TccRtc * rtc)
{

	if (!(rtc->regIntcon & INTCON_PROT) && (rtc->regRtccon & RTCCON_STARTB)
	    && (rtc->regRtccon & RTCCON_RTCWEN) && (rtc->regIntcon & INTCON_INTWREN)) {
		return 1;
	}
	return 0;
}

static int
alm_is_writable(TccRtc * rtc)
{

	if ((rtc->regRtccon & RTCCON_RTCWEN) && (rtc->regIntcon & INTCON_INTWREN)) {
		return 1;
	}
	return 0;
}

static int
reg_is_readable(TccRtc * rtc)
{

	if (rtc->regIntcon & INTCON_INTWREN) {
		return 1;
	}
	return 0;
}

/*
 ***********************************************************************************************
 * \fn rtccon_read(void *clientData,uint32_t address,int rqlen)
 * Bit 7: WUOUTEN Wake up interrupt output enable.
 * Bit 6: AIOUTEN Alarm Interrupt output enable.
 * Bit 5: OSCEN	  Oszillator and divider ciruit Test enable.
 * Bit 4: CLKRST  RTC clock count (divider) reset.
 * Bit 3: CNTSEL  BCD count test type select.
 * Bit 2: CLKSEL  Clock Select: 0: 1Hz clock (Normal operation) 1: XTI / 2
 * Bit 1: RTCWEN  RTC write enable 1 == enable
 * Bit 0: STARTB  0 = Run, 1 = Stop 
 ***********************************************************************************************
 */
static uint32_t
rtccon_read(void *clientData, uint32_t address, int rqlen)
{
	TccRtc *rtc = clientData;
	return rtc->regRtccon;
}

static void
rtccon_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	TccRtc *rtc = clientData;
	uint32_t diff = value ^ rtc->regRtccon;
	/* When clock will be stopped the current time is required for saving */
	update_time(rtc);
	rtc->regRtccon = value & 0xff;
	if (diff & RTCCON_STARTB) {
		if (!(value & RTCCON_STARTB)) {
			rtc->time_offset = diff_systime(rtc);
		}
		rtc_save_to_diskimage(rtc);
	}
}

/**
 ***********************************************************************
 * Bit 15: PROT             (1 << 15)
 * Bit 12 - 13: XDRV	0 = 32768Hz, 3 = 4194304Hz
 * Bit 8 - 10:  FSEL    0 = 32767Hz, 7 = 4194304Hz
 * Bit 0: INTWREN 
 ***********************************************************************
 */
static uint32_t
intcon_read(void *clientData, uint32_t address, int rqlen)
{
	TccRtc *rtc = clientData;
	return rtc->regIntcon;
}

static void
intcon_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	TccRtc *rtc = clientData;
	uint32_t xdrv = value & INTCON_XDRV_MSK;
	uint32_t fsel = value & INTCON_FSEL_MSK;
	if (!(rtc->regRtccon & RTCCON_RTCWEN)) {
		fprintf(stderr, "Can not write intcon when not RTCWEN\n");
		return;
	}
	if (value & INTCON_INTWREN) {
		if (!(rtc->regIntcon & INTCON_INTWREN)) {
			rtc->regIntcon |= INTCON_INTWREN;
			/* Does not change the rest if not yet enabled */
			return;
		}
	} else {
		rtc->regIntcon &= ~INTCON_INTWREN;
	}
	if ((value & INTCON_PROT)) {
		if (rtc->regRtccon & RTCCON_STARTB) {
			rtc->regIntcon |= INTCON_PROT;
			return;
		} else if (!(rtc->regIntcon & INTCON_PROT)) {
			/* Warn only if PROT is not already set */
			fprintf(stderr, "Can not enable protection while RTC is running\n");
		}
	} else {
		if ((rtc->regRtccon & RTCCON_STARTB)
		    && (rtc->regRtccon & RTCCON_RTCWEN)
		    && (rtc->regIntcon & INTCON_INTWREN)) {
			rtc->regIntcon &= ~INTCON_PROT;
		} else {
			fprintf(stderr,
				"Can not disable protection, RTCCON 0x%02x, INTCON 0x%02x\n",
				rtc->regRtccon, rtc->regIntcon);
		}

	}
	if (!(rtc->regIntcon & INTCON_PROT) && (rtc->regRtccon & RTCCON_STARTB)
	    && (rtc->regRtccon & RTCCON_RTCWEN) && (rtc->regIntcon & INTCON_INTWREN)) {
		rtc->regIntcon =
		    (rtc->regIntcon & ~(INTCON_XDRV_MSK | INTCON_FSEL_MSK)) | xdrv | fsel;
	} else if ((xdrv != (rtc->regIntcon & INTCON_XDRV_MSK))
		   || (fsel != (rtc->regIntcon & INTCON_FSEL_MSK))) {
		fprintf(stderr, "Can not change XDRV/FSEL because of protection\n");
	}
}

static uint32_t
rtcalm_read(void *clientData, uint32_t address, int rqlen)
{
	TccRtc *rtc = clientData;
	if (!reg_is_readable(rtc)) {
		fprintf(stderr, "Register read not enabled for %s\n", __func__);
		return intcon_read(clientData, address, rqlen);
	}
	fprintf(stderr, "TCC8K RTC: %s: Register not implemented\n", __func__);
	return 0;
}

static void
rtcalm_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{

	TccRtc *rtc = clientData;
	if (!alm_is_writable(rtc)) {
		fprintf(stderr, "Register write not enabled for %s\n", __func__);
		rtc->regRtcalm = 0;	/* Yes real device does this */
		update_timeout(rtc);
		return;
	}
	rtc->regRtcalm = value & 0xff;
	update_timeout(rtc);
	return;
}

static uint32_t
almsec_read(void *clientData, uint32_t address, int rqlen)
{
	TccRtc *rtc = clientData;
	if (!reg_is_readable(rtc)) {
		fprintf(stderr, "Register read not enabled for %s\n", __func__);
		return intcon_read(clientData, address, rqlen);
	}
	return rtc->regAlmsec;
}

static void
almsec_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	TccRtc *rtc = clientData;
	if (!alm_is_writable(rtc)) {
		fprintf(stderr, "Register write not enabled for %s\n", __func__);
		return;
	}
	rtc->regAlmsec = value & 0xff;
	update_timeout(rtc);
}

static uint32_t
almmin_read(void *clientData, uint32_t address, int rqlen)
{
	TccRtc *rtc = clientData;
	if (!reg_is_readable(rtc)) {
		fprintf(stderr, "Register read not enabled for %s\n", __func__);
		return intcon_read(clientData, address, rqlen);
	}
	return rtc->regAlmmin;
}

static void
almmin_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	TccRtc *rtc = clientData;
	if (!alm_is_writable(rtc)) {
		fprintf(stderr, "Register write not enabled for %s\n", __func__);
		return;
	}
	rtc->regAlmmin = value & 0xff;
	update_timeout(rtc);
}

static uint32_t
almhour_read(void *clientData, uint32_t address, int rqlen)
{
	TccRtc *rtc = clientData;
	if (!reg_is_readable(rtc)) {
		fprintf(stderr, "Register read not enabled for %s\n", __func__);
		return intcon_read(clientData, address, rqlen);
	}
	return rtc->regAlmhour;
}

static void
almhour_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	TccRtc *rtc = clientData;
	if (!alm_is_writable(rtc)) {
		fprintf(stderr, "Register write not enabled for %s\n", __func__);
		return;
	}
	rtc->regAlmhour = value & 0xff;
	update_timeout(rtc);
}

static uint32_t
almdate_read(void *clientData, uint32_t address, int rqlen)
{
	TccRtc *rtc = clientData;
	if (!reg_is_readable(rtc)) {
		fprintf(stderr, "Register read not enabled for %s\n", __func__);
		return intcon_read(clientData, address, rqlen);
	}
	return rtc->regAlmdate;
}

static void
almdate_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	TccRtc *rtc = clientData;
	if (!alm_is_writable(rtc)) {
		fprintf(stderr, "Register write not enabled for %s\n", __func__);
		return;
	}
	rtc->regAlmdate = value & 0xff;
	update_timeout(rtc);
}

static uint32_t
almday_read(void *clientData, uint32_t address, int rqlen)
{
	TccRtc *rtc = clientData;
	if (!reg_is_readable(rtc)) {
		fprintf(stderr, "Register read not enabled for %s\n", __func__);
		return intcon_read(clientData, address, rqlen);
	}
	return rtc->regAlmday;
}

static void
almday_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	TccRtc *rtc = clientData;
	if (!alm_is_writable(rtc)) {
		fprintf(stderr, "Register write not enabled for %s\n", __func__);
		return;
	}
	rtc->regAlmday = value & 0xff;
	update_timeout(rtc);
}

static uint32_t
almmon_read(void *clientData, uint32_t address, int rqlen)
{
	TccRtc *rtc = clientData;
	if (!reg_is_readable(rtc)) {
		fprintf(stderr, "Register read not enabled for %s\n", __func__);
		return intcon_read(clientData, address, rqlen);
	}
	return rtc->regAlmmon;
}

static void
almmon_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	TccRtc *rtc = clientData;
	if (!alm_is_writable(rtc)) {
		fprintf(stderr, "Register write not enabled for %s\n", __func__);
		return;
	}
	rtc->regAlmmon = value & 0xff;
	update_timeout(rtc);
}

static uint32_t
almyear_read(void *clientData, uint32_t address, int rqlen)
{
	TccRtc *rtc = clientData;
	if (!reg_is_readable(rtc)) {
		fprintf(stderr, "Register read not enabled for %s\n", __func__);
		return intcon_read(clientData, address, rqlen);
	}
	return rtc->regAlmyear;
}

static void
almyear_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	TccRtc *rtc = clientData;
	if (!alm_is_writable(rtc)) {
		fprintf(stderr, "Register write not enabled for %s\n", __func__);
		return;
	}
	rtc->regAlmyear = value & 0xffff;
	update_timeout(rtc);
}

static uint32_t
bcdsec_read(void *clientData, uint32_t address, int rqlen)
{
	TccRtc *rtc = clientData;
	if (!reg_is_readable(rtc)) {
		fprintf(stderr, "Register read not enabled for %s\n", __func__);
		return intcon_read(clientData, address, rqlen);
	}
	update_time(rtc);
	return rtc->regBcdsec;
}

static void
bcdsec_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	TccRtc *rtc = clientData;
	if (!time_is_writable(rtc)) {
		fprintf(stderr, "TCC8K RTC: %s: Time is not writable\n", __func__);
		return;
	}
	rtc->regBcdsec = value;
	rtc_save_to_diskimage(rtc);
}

static uint32_t
bcdmin_read(void *clientData, uint32_t address, int rqlen)
{
	TccRtc *rtc = clientData;
	if (!reg_is_readable(rtc)) {
		fprintf(stderr, "Register read not enabled for %s\n", __func__);
		return intcon_read(clientData, address, rqlen);
	}
	update_time(rtc);
	return rtc->regBcdmin;
}

static void
bcdmin_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	TccRtc *rtc = clientData;
	if (!time_is_writable(rtc)) {
		fprintf(stderr, "TCC8K RTC: %s: Time is not writable\n", __func__);
		return;
	}
	rtc->regBcdmin = value;
	rtc_save_to_diskimage(rtc);
}

/**
 *********************************************************
 * BCD hour is from 0 to 23. 
 * The manual is wrong.
 *********************************************************
 */
static uint32_t
bcdhour_read(void *clientData, uint32_t address, int rqlen)
{
	TccRtc *rtc = clientData;
	if (!reg_is_readable(rtc)) {
		fprintf(stderr, "Register read not enabled for %s\n", __func__);
		return intcon_read(clientData, address, rqlen);
	}
	update_time(rtc);
	return rtc->regBcdhour;
}

static void
bcdhour_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	TccRtc *rtc = clientData;
	if (!time_is_writable(rtc)) {
		fprintf(stderr, "TCC8K RTC: %s: Time is not writable\n", __func__);
		return;
	}
	rtc->regBcdhour = value;
	rtc_save_to_diskimage(rtc);
}

static uint32_t
bcddate_read(void *clientData, uint32_t address, int rqlen)
{
	TccRtc *rtc = clientData;
	if (!reg_is_readable(rtc)) {
		fprintf(stderr, "Register read not enabled for %s\n", __func__);
		return intcon_read(clientData, address, rqlen);
	}
	update_time(rtc);
	return rtc->regBcddate;
}

/**
 *********************************************************
 * BCD date is from 1 to 31. 
 * The manual is wrong.
 *********************************************************
 */
static void
bcddate_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	TccRtc *rtc = clientData;
	if (!time_is_writable(rtc)) {
		fprintf(stderr, "TCC8K RTC: %s: Time is not writable\n", __func__);
		return;
	}
	rtc->regBcddate = value;
	rtc_save_to_diskimage(rtc);
}

static uint32_t
bcdday_read(void *clientData, uint32_t address, int rqlen)
{
	TccRtc *rtc = clientData;
	if (!reg_is_readable(rtc)) {
		fprintf(stderr, "Register read not enabled for %s\n", __func__);
		return intcon_read(clientData, address, rqlen);
	}
	update_time(rtc);
	return rtc->regBcdday;
}

static void
bcdday_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	TccRtc *rtc = clientData;
	if (!time_is_writable(rtc)) {
		fprintf(stderr, "TCC8K RTC: %s: Time is not writable\n", __func__);
		return;
	}
	rtc->regBcdday = value;
	rtc_save_to_diskimage(rtc);
}

/**
 ********************************************************************
 * The month is in the range from 0x01 to 0x12
 * The manual is right.
 ********************************************************************
 */
static uint32_t
bcdmon_read(void *clientData, uint32_t address, int rqlen)
{
	TccRtc *rtc = clientData;
	if (!reg_is_readable(rtc)) {
		fprintf(stderr, "Register read not enabled for %s\n", __func__);
		return intcon_read(clientData, address, rqlen);
	}
	update_time(rtc);
	return rtc->regBcdmon;
}

static void
bcdmon_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	TccRtc *rtc = clientData;
	if (!time_is_writable(rtc)) {
		fprintf(stderr, "TCC8K RTC: %s: Time is not writable\n", __func__);
		return;
	}
	rtc->regBcdmon = value;
	rtc_save_to_diskimage(rtc);
}

static uint32_t
bcdyear_read(void *clientData, uint32_t address, int rqlen)
{
	TccRtc *rtc = clientData;
	if (!reg_is_readable(rtc)) {
		fprintf(stderr, "Register read not enabled for %s\n", __func__);
		return intcon_read(clientData, address, rqlen);
	}
	update_time(rtc);
	return rtc->regBcdyear;
}

static void
bcdyear_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	TccRtc *rtc = clientData;
	if (!time_is_writable(rtc)) {
		fprintf(stderr, "TCC8K RTC: %s: Time is not writable\n", __func__);
		return;
	}
	rtc->regBcdyear = value;
	rtc_save_to_diskimage(rtc);
}

static uint32_t
rtcim_read(void *clientData, uint32_t address, int rqlen)
{
	TccRtc *rtc = clientData;
	if (!reg_is_readable(rtc)) {
		fprintf(stderr, "Register read not enabled for %s\n", __func__);
		return intcon_read(clientData, address, rqlen);
	}
	return rtc->regRtcim;
}

static void
rtcim_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	TccRtc *rtc = clientData;
	if ((rtc->regRtccon & RTCCON_RTCWEN) && (rtc->regIntcon & INTCON_INTWREN)) {
		fprintf(stderr, "Register write not enabled for %s\n", __func__);
		return;
	}
	rtc->regRtcim = value & 0xf;
}

static uint32_t
rtcpend_read(void *clientData, uint32_t address, int rqlen)
{
	TccRtc *rtc = clientData;
	if (!reg_is_readable(rtc)) {
		fprintf(stderr, "Register read not enabled for %s\n", __func__);
		return intcon_read(clientData, address, rqlen);
	}
	fprintf(stderr, "TCC8K RTC: %s: Register not implemented\n", __func__);
	return 0;
}

static void
rtcpend_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "TCC8K RTC: %s: Register not implemented\n", __func__);
}

static uint32_t
rtcapb_read(void *clientData, uint32_t address, int rqlen)
{
	TccRtc *rtc = clientData;
	return rtc->regRtcapb;
}

static void
rtcapb_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	TccRtc *rtc = clientData;
	rtc->regRtcapb = value & 0x3333;
}

static void
TccRtc_Map(void *owner, uint32_t base, uint32_t mask, uint32_t _flags)
{
	TccRtc *rtc = owner;
	IOH_New32(REG_RTCCON(base), rtccon_read, rtccon_write, rtc);
	IOH_New32(REG_INTCON(base), intcon_read, intcon_write, rtc);
	IOH_New32(REG_RTCALM(base), rtcalm_read, rtcalm_write, rtc);
	IOH_New32(REG_ALMSEC(base), almsec_read, almsec_write, rtc);
	IOH_New32(REG_ALMMIN(base), almmin_read, almmin_write, rtc);
	IOH_New32(REG_ALMHOUR(base), almhour_read, almhour_write, rtc);
	IOH_New32(REG_ALMDATE(base), almdate_read, almdate_write, rtc);
	IOH_New32(REG_ALMDAY(base), almday_read, almday_write, rtc);
	IOH_New32(REG_ALMMON(base), almmon_read, almmon_write, rtc);
	IOH_New32(REG_ALMYEAR(base), almyear_read, almyear_write, rtc);
	IOH_New32(REG_BCDSEC(base), bcdsec_read, bcdsec_write, rtc);
	IOH_New32(REG_BCDMIN(base), bcdmin_read, bcdmin_write, rtc);
	IOH_New32(REG_BCDHOUR(base), bcdhour_read, bcdhour_write, rtc);
	IOH_New32(REG_BCDDATE(base), bcddate_read, bcddate_write, rtc);
	IOH_New32(REG_BCDDAY(base), bcdday_read, bcdday_write, rtc);
	IOH_New32(REG_BCDMON(base), bcdmon_read, bcdmon_write, rtc);
	IOH_New32(REG_BCDYEAR(base), bcdyear_read, bcdyear_write, rtc);
	IOH_New32(REG_RTCIM(base), rtcim_read, rtcim_write, rtc);
	IOH_New32(REG_RTCPEND(base), rtcpend_read, rtcpend_write, rtc);
	IOH_New32(REG_RTCAPB(base), rtcapb_read, rtcapb_write, rtc);
}

static void
TccRtc_UnMap(void *owner, uint32_t base, uint32_t mask)
{
	IOH_Delete32(REG_RTCCON(base));
	IOH_Delete32(REG_INTCON(base));
	IOH_Delete32(REG_RTCALM(base));
	IOH_Delete32(REG_ALMSEC(base));
	IOH_Delete32(REG_ALMMIN(base));
	IOH_Delete32(REG_ALMHOUR(base));
	IOH_Delete32(REG_ALMDATE(base));
	IOH_Delete32(REG_ALMDAY(base));
	IOH_Delete32(REG_ALMMON(base));
	IOH_Delete32(REG_ALMYEAR(base));
	IOH_Delete32(REG_BCDSEC(base));
	IOH_Delete32(REG_BCDMIN(base));
	IOH_Delete32(REG_BCDHOUR(base));
	IOH_Delete32(REG_BCDDATE(base));
	IOH_Delete32(REG_BCDDAY(base));
	IOH_Delete32(REG_BCDMON(base));
	IOH_Delete32(REG_BCDYEAR(base));
	IOH_Delete32(REG_RTCIM(base));
	IOH_Delete32(REG_RTCPEND(base));
	IOH_Delete32(REG_RTCAPB(base));
}

BusDevice *
TCC8K_RtcNew(const char *name)
{
	TccRtc *rtc = sg_new(TccRtc);
	char *dirname;
	char *imagename;
	rtc->bdev.first_mapping = NULL;
	rtc->bdev.Map = TccRtc_Map;
	rtc->bdev.UnMap = TccRtc_UnMap;
	rtc->bdev.owner = rtc;
	rtc->bdev.hw_flags = MEM_FLAG_WRITABLE | MEM_FLAG_READABLE;
	rtc->sigIrq = SigNode_New("%s.irq", name);
	if (!rtc->sigIrq) {
		fprintf(stderr, "Can not create Interrupt line for SD-Card controller\n");
		exit(1);
	}
	rtc->regRtccon = 0;
	rtc->regRtcapb = 0x1001;
	dirname = Config_ReadVar("global", "imagedir");
	if (dirname) {
		imagename = alloca(strlen(dirname) + strlen(name) + 20);
		sprintf(imagename, "%s/%s.img", dirname, name);
		rtc->disk_image = DiskImage_Open(imagename, RTC_IMAGESIZE, DI_RDWR | DI_CREAT_00);
		if (!rtc->disk_image) {
			fprintf(stderr,
				"Failed to open TCC RTC time offset file, using offset 0\n");
			rtc->time_offset = 0;
		} else {
			if (rtc_load_from_diskimage(rtc) < 0) {

			}
		}
	} else {
		rtc->time_offset = 0;
	}

#if 0
	sdc->clk_in = Clock_New("%s.clk", name);
	sdc->sdclk = Clock_New("%s.sdclk", name);
	if (!sdc->clk_in || !sdc->sdclk) {
		fprintf(stderr, "Can not create clocks for SD-Host \"%s\"\n", name);
		exit(1);
	}
	update_clock(sdc);
	CycleTimer_Init(&sdc->cmd_delay_timer, do_cmd_delayed, sdc);
	//CycleTimer_Init(&sdc->data_delay_timer,do_data_delayed,sdc);
	CycleTimer_Init(&sdc->write_delay_timer, do_transfer_write, sdc);
#endif
	fprintf(stderr, "Created TCC8K RTC \"%s\"\n", name);
	return &rtc->bdev;

}
