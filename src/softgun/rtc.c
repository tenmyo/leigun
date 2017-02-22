/*
 * Library with support functions for implementing Realtime clock simulators
 */

#include <stdint.h>
#include <stdlib.h>
#include <sys/time.h>
#include <time.h>
#include <stdio.h>
#include "rtc.h"

static const uint16_t daysSinceJan1st[2][13] = {
    {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334, 365},   // 365 days, non-leap
    {0, 31, 60, 91, 121, 152, 182, 213, 244, 274, 305, 335, 366}    // 366 days, leap
};

/**
 **********************************************************************
 * \fn static uint32_t RTime_ToSecSinceEpoch(RTime *rtime); 
 * This is the conversion from the official Unix specification.
 **********************************************************************
 */
static uint32_t
RTime_ToSecSinceEpoch(const RTime * rtime)
{
    int leap = !(rtime->year % 4) && (rtime->year % 100 || !(rtime->year % 400));
    uint16_t yday;      /* 0 - 365 */
    yday = daysSinceJan1st[leap][(rtime->month - 1) % 13] + rtime->mday - 1;    /* 0 - 365 */
    return rtime->sec + rtime->min * UINT32_C(60) + rtime->hour * UINT32_C(3600)
        + yday * UINT32_C(86400)
        + (rtime->year - 1970) * UINT32_C(31536000)
        + ((rtime->year - 1969) / 4) * UINT32_C(86400)
        - ((rtime->year - 1901) / 100) * UINT32_C(86400)
        + ((rtime->year - 1601) / 400) * UINT32_C(86400);
}

uint8_t
RTime_CalcWDay(const RTime *rtime) 
{
    uint32_t sec = RTime_ToSecSinceEpoch(rtime);
    uint8_t wday;
    wday = ((sec / UINT32_C(86400) + 4) % 7);    /* day of week with sun = 0 */
    return wday;
}

/*
 *******************************************************************************************
 * \fn RTimeHostOfsUs RTime_DiffToHostTime(const RTime *rtime)
 * Calculate the difference to the system time of the host in microseconds
 *******************************************************************************************
 */
RTimeHostOfsUs
RTime_DiffToHostTime(const RTime *rtime)
{
    struct timeval now;
    char *zone;
    int64_t offset;
    time_t rtc_time;
    time_t sys_utc_time;
    struct tm tm;
    tm.tm_isdst = -1;
    tm.tm_mon = rtime->month - 1;
    tm.tm_mday = rtime->mday;
    tm.tm_year = rtime->year - 1900;
	tm.tm_hour = rtime->hour;
    tm.tm_min = rtime->min;
    tm.tm_sec = rtime->sec;
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
    zone = getenv("TZ");    /* remember original */
    setenv("TZ", "UTC", 1); /* "UTC" */
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
    offset += rtime->usec;
    offset -= now.tv_usec;
    //fprintf(stdout,"rtc %ld, sys %ld, calculated offset %ld\n",rtc_time,sys_utc_time,offset);
    return offset;
}

/*
 *******************************************************************************************
 * \fn void RTime_ReadWithOffset(RTime *rtime, int64_t timeOffsetUs);
 * Add an offset to the host time, and convert it to an RTime structure before returning 
 *******************************************************************************************
 */
void
RTime_ReadWithOffset(RTime *rtime, RTimeHostOfsUs timeOffsetUs)
{
    struct timeval host_tv;
    time_t time;
    struct tm tm;
    int32_t usec;
    gettimeofday(&host_tv, NULL);
    time = host_tv.tv_sec;
    time += (timeOffsetUs / (int64_t) 1000000);
    usec = host_tv.tv_usec + (timeOffsetUs % 1000000);
    if (usec > 1000000) {
        time++;
        usec -= 1000000;
    } else if (usec < 0) {
        fprintf(stderr, "Modulo was negative Happened\n");
        time--;
        usec += 1000000;
    }
    rtime->usec = usec;
    gmtime_r(&time, &tm);
    if ((tm.tm_year < 100) || (tm.tm_year >= 200)) {
        fprintf(stderr, "Time: Illegal year %d\n", tm.tm_year + 1900);
    }
	rtime->year = tm.tm_year + 1900;
	rtime->month = tm.tm_mon + 1;
	rtime->wday = (tm.tm_wday % 7); /*  Sunday = 0 */

    rtime->month = tm.tm_mon + 1;
    rtime->mday = tm.tm_mday;
  	rtime->hour = tm.tm_hour;
    rtime->min = tm.tm_min;
    rtime->sec = tm.tm_sec;
    //fprintf(stderr,"UDT: Year %d, hour %d minute %d\n",tm.tm_year+1900,tm.tm_hour,tm.tm_min);
    return;
}
