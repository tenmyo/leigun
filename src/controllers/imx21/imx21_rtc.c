/*
 *************************************************************************************************
 * Emulation of Freescale iMX21 real time clock module 
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

#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/ioctl.h>
#include <sys/fcntl.h>
#include "bus.h"
#include "fio.h"
#include "signode.h"
#include "imx21_rtc.h"
#include "configfile.h"
#include "clock.h"
#include "cycletimer.h"
#include "diskimage.h"
#include "sgstring.h"

#define RTC_HOURMIN(base) 	((base) + 0x00)
#define RTC_SECONDS(base)	((base) + 0x04)
#define RTC_ALRM_HM(base)	((base) + 0x08)
#define RTC_ALRM_SEC(base)	((base) + 0x0c)
#define RTC_RCCTL(base)		((base) + 0x10)
#define RTC_ISR(base)		((base) + 0x14)
#define RTC_IENR(base)		((base) + 0x18)
#define RTC_STPWCH(base)	((base) + 0x1c)
#define RTC_DAYR(base) 		((base) + 0x20)
#define RTC_DAYALARM(base)	((base) + 0x24)

typedef struct IMX21Rtc {
	BusDevice bdev;
	uint8_t *time_offset;	/* 4 Byte little endian array with offset to system time */
	DiskImage *disk_image;
	uint16_t days;
	uint8_t hours;
	uint8_t minutes;
	uint8_t seconds;
	uint16_t alrm_days;
	uint8_t alrm_hours;
	uint8_t alrm_minutes;
	uint8_t alrm_seconds;
	uint32_t ienr;
	uint32_t isr;
	CycleCounter_t last_timer_update;
	SigNode *irqNode;
} IMX21Rtc;

static void
update_interrupt(IMX21Rtc * rtc)
{
	if (rtc->isr & rtc->ienr) {
		SigNode_Set(rtc->irqNode, SIG_LOW);
	} else {
		SigNode_Set(rtc->irqNode, SIG_HIGH);
	}
}

static void
actualize_timers(IMX21Rtc * rtc)
{

}

/*
 * calculate diff to systemtime
 */
static int32_t
diff_systime(IMX21Rtc * rtc)
{
	struct timeval tv;
	char *zone;
	int32_t offset;
	time_t rtc_time;
	time_t sys_utc_time;
	struct tm tm;

	rtc_time = rtc->days * 86400 + rtc->hours * 3600 + rtc->minutes * 60 + rtc->seconds;

	/* Shit, there is no mktime which does GMT */
	zone = getenv("TZ");	/* remember original */
	setenv("TZ", "", 1);	/* ??? "UTC" ??? */
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
	return offset;
}

static void
write_time(IMX21Rtc * rtc)
{
	int32_t offset = diff_systime(rtc);
	rtc->time_offset[0] = offset & 0xff;
	rtc->time_offset[1] = (offset >> 8) & 0xff;
	rtc->time_offset[2] = (offset >> 16) & 0xff;
	rtc->time_offset[3] = (offset >> 24) & 0xff;
}

/*
 * ----------------------------------------------------------------
 * read_systemtime
 *      Read new time from system and write to the rtc structure 
 * ----------------------------------------------------------------
 */
static void
read_systemtime(IMX21Rtc * rtc)
{
	struct timeval tv;
	int32_t offset;
	time_t time;
	//struct tm tm;
	gettimeofday(&tv, NULL);
	time = tv.tv_sec;
	offset = rtc->time_offset[0] | (rtc->time_offset[1] << 8)
	    | (rtc->time_offset[2] << 16) | (rtc->time_offset[3] << 24);
	time += offset;
	// gmtime_r(&time,&tm);
	//fprintf(stderr,"host: tm.hour %d tm.tm_isdst %d\n",tm->tm_hour,tm->tm_isdst);
	rtc->days = time / 86400;
	time -= rtc->days * 86400;
	rtc->hours = time / 3600;
	time -= rtc->hours * 3600;
	rtc->minutes = time / 60;
	time -= rtc->minutes * 60;
	rtc->seconds = time;
	fprintf(stderr, "Got time %d days %d hours %d minutes %d seconds\n",
		rtc->days, rtc->hours, rtc->minutes, rtc->seconds);
}

static uint32_t
hourmin_read(void *clientData, uint32_t address, int rqlen)
{
	IMX21Rtc *rtc = (IMX21Rtc *) clientData;;
	//fprintf(stderr,"RTC: register 0x%08x not implemented\n",address);
	read_systemtime(rtc);
	return (rtc->hours << 8) | (rtc->minutes);
}

static void
hourmin_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	IMX21Rtc *rtc = (IMX21Rtc *) clientData;;
	read_systemtime(rtc);
	rtc->hours = (value >> 8) & 0x1f;
	rtc->minutes = (value & 0x3f);
	write_time(rtc);
	//fprintf(stderr,"RTC: register 0x%08x not implemented\n",address);
	return;
}

static uint32_t
seconds_read(void *clientData, uint32_t address, int rqlen)
{
	IMX21Rtc *rtc = (IMX21Rtc *) clientData;;
	read_systemtime(rtc);
	//fprintf(stderr,"RTC: register 0x%08x not implemented\n",address);
	return rtc->seconds;
}

static void
seconds_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	IMX21Rtc *rtc = (IMX21Rtc *) clientData;;
	read_systemtime(rtc);
	rtc->seconds = value & 0x3f;
	write_time(rtc);
	return;
}

static uint32_t
alrm_hm_read(void *clientData, uint32_t address, int rqlen)
{
	IMX21Rtc *rtc = (IMX21Rtc *) clientData;;
	return (rtc->alrm_hours << 8) | (rtc->alrm_minutes);
}

static void
alrm_hm_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "RTC: register 0x%08x not implemented\n", address);
	return;
}

static uint32_t
alrm_sec_read(void *clientData, uint32_t address, int rqlen)
{
	IMX21Rtc *rtc = (IMX21Rtc *) clientData;;
	return rtc->alrm_seconds;
}

static void
alrm_sec_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "RTC: register 0x%08x not implemented\n", address);
	return;
}

static uint32_t
rcctl_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "RTC: register 0x%08x not implemented\n", address);
	return 0;
}

static void
rcctl_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "RTC: register 0x%08x not implemented\n", address);
	return;
}

static uint32_t
isr_read(void *clientData, uint32_t address, int rqlen)
{
	IMX21Rtc *rtc = clientData;
	actualize_timers(rtc);
	return 0;
}

static void
isr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	IMX21Rtc *rtc = clientData;
	uint32_t clearbits = rtc->isr & value & 0xffbf;
	rtc->isr ^= clearbits;
	update_interrupt(rtc);
	return;
}

static uint32_t
ienr_read(void *clientData, uint32_t address, int rqlen)
{
	IMX21Rtc *rtc = clientData;
	return rtc->ienr;
}

static void
ienr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	IMX21Rtc *rtc = clientData;
	rtc->ienr = value & 0xffbf;
	update_interrupt(rtc);
	return;
}

static uint32_t
stpwch_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "RTC: register 0x%08x not implemented\n", address);
	return 0;
}

static void
stpwch_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "RTC: register 0x%08x not implemented\n", address);
	return;
}

static uint32_t
dayr_read(void *clientData, uint32_t address, int rqlen)
{
	IMX21Rtc *rtc = (IMX21Rtc *) clientData;;
	read_systemtime(rtc);
	//fprintf(stderr,"RTC: register 0x%08x not implemented\n",address);
	return rtc->days;
}

static void
dayr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	IMX21Rtc *rtc = (IMX21Rtc *) clientData;;
	read_systemtime(rtc);
	rtc->days = value & 0xffff;
	write_time(rtc);
	return;
}

static uint32_t
dayalarm_read(void *clientData, uint32_t address, int rqlen)
{
	IMX21Rtc *rtc = (IMX21Rtc *) clientData;;
	return rtc->alrm_days;
}

static void
dayalarm_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "RTC: register 0x%08x not implemented\n", address);
	return;
}

static uint32_t
undefined_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "WARNING !: read from undefined location 0x%08x in RTC module\n", address);
	return 0;
}

static void
undefined_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "WARNING !: write to undefined location 0x%08x in RTC module\n", address);
	return;
}

static void
IMXRtc_Map(void *owner, uint32_t base, uint32_t mask, uint32_t flags)
{
	IMX21Rtc *rtc = (IMX21Rtc *) owner;
	IOH_New32(RTC_HOURMIN(base), hourmin_read, hourmin_write, rtc);
	IOH_New32(RTC_SECONDS(base), seconds_read, seconds_write, rtc);
	IOH_New32(RTC_ALRM_HM(base), alrm_hm_read, alrm_hm_write, rtc);
	IOH_New32(RTC_ALRM_SEC(base), alrm_sec_read, alrm_sec_write, rtc);
	IOH_New32(RTC_RCCTL(base), rcctl_read, rcctl_write, rtc);
	IOH_New32(RTC_ISR(base), isr_read, isr_write, rtc);
	IOH_New32(RTC_IENR(base), ienr_read, ienr_write, rtc);
	IOH_New32(RTC_STPWCH(base), stpwch_read, stpwch_write, rtc);
	IOH_New32(RTC_DAYR(base), dayr_read, dayr_write, rtc);
	IOH_New32(RTC_DAYALARM(base), dayalarm_read, dayalarm_write, rtc);
	IOH_NewRegion(base, 0x1000, undefined_read, undefined_write, IOH_FLG_HOST_ENDIAN, rtc);
}

static void
IMXRtc_UnMap(void *owner, uint32_t base, uint32_t mask)
{
	IOH_Delete32(RTC_HOURMIN(base));
	IOH_Delete32(RTC_SECONDS(base));
	IOH_Delete32(RTC_ALRM_HM(base));
	IOH_Delete32(RTC_ALRM_SEC(base));
	IOH_Delete32(RTC_RCCTL(base));
	IOH_Delete32(RTC_ISR(base));
	IOH_Delete32(RTC_IENR(base));
	IOH_Delete32(RTC_STPWCH(base));
	IOH_Delete32(RTC_DAYR(base));
	IOH_Delete32(RTC_DAYALARM(base));
	IOH_DeleteRegion(base, 0x1000);
}

BusDevice *
IMX21_RtcNew(const char *name)
{
	char *dirname, *imagename;
	IMX21Rtc *rtc = sg_new(IMX21Rtc);
	rtc->irqNode = SigNode_New("%s.irq", name);
	if (!rtc->irqNode) {
		fprintf(stderr, "i.MX21 RTC: can not create interrupt line\n");
		exit(1);
	}
	dirname = Config_ReadVar("global", "imagedir");
	if (dirname) {
		imagename = alloca(strlen(dirname) + strlen(name) + 20);
		sprintf(imagename, "%s/%s.img", dirname, name);
		rtc->disk_image = DiskImage_Open(imagename, 4, DI_RDWR | DI_CREAT_00);
		if (!rtc->disk_image) {
			fprintf(stderr, "Failed to open PCF8563 time offset file\n");
			rtc->time_offset = sg_calloc(4);
		} else {
			rtc->time_offset = DiskImage_Mmap(rtc->disk_image);
		}
	} else {
		rtc->time_offset = sg_calloc(4);
	}
	rtc->bdev.first_mapping = NULL;
	rtc->bdev.Map = IMXRtc_Map;
	rtc->bdev.UnMap = IMXRtc_UnMap;
	rtc->bdev.owner = rtc;
	rtc->bdev.hw_flags = MEM_FLAG_WRITABLE | MEM_FLAG_READABLE;
	return &rtc->bdev;
}
