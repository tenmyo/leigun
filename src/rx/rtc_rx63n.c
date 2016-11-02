/*
 **********************************************************************************************
 * Renesas RX Realtime clock simulation  (RTCa) from RX63 manual 
 *
 * State: Basic setting/reading of time is working. Alarms are not implemented
 *
 * Copyright 2012 Jochen Karrer. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification, are
 * permitted provided that the following conditions are met:
 *
 *   1. Redistributions of source code must retain the above copyright notice, this list of
 *       conditions and the following disclaimer.
 *
 *    2. Redistributions in binary form must reproduce the above copyright notice, this list
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
 **********************************************************************************************
 */

#include "bus.h"
#include "sgstring.h"
#include "clock.h"
#include "diskimage.h"
#include "configfile.h"
#include "rtc.h"
#include "sglib.h"
#include "cycletimer.h"

#define REG_R64CNT(base)	((base) + 0x00)
#define REG_RSECCNT(base)	((base) + 0x02)
#define REG_RMINCNT(base)	((base) + 0x04)
#define REG_RHRCNT(base)	((base) + 0x06)
#define     RHRCNT_PM       (1 << 6)
#define REG_RWKCNT(base)	((base) + 0x08)
#define REG_RDAYCNT(base)	((base) + 0x0a)
#define REG_RMONCNT(base)	((base) + 0x0c)
#define REG_RYRCNT(base)	((base) + 0x0e)
#define REG_RSECAR(base)	((base) + 0x10)
#define REG_RMINAR(base)	((base) + 0x12)
#define REG_RHRAR(base)		((base) + 0x14)
#define REG_RWKAR(base)		((base) + 0x16)
#define REG_RDAYAR(base)	((base) + 0x18)
#define REG_RMONAR(base)	((base) + 0x1a)
#define REG_RYRAR(base)		((base) + 0x1c)
#define REG_RYRAREN(base)	((base) + 0x1e)
#define REG_RCR1(base)		((base) + 0x22)
#define     RCR1_AIE        (1 << 0)
#define     RCR1_CIE        (1 << 1)
#define     RCR1_PIE        (1 << 2)
#define     RCR1_PES_MSK    (0xf << 4)
#define REG_RCR2(base)		((base) + 0x24)
#define     RCR2_HR24       (1 << 6)
#define     RCR2_AADJP      (1 << 5)
#define     RCR2_AADJE      (1 << 4)
#define     RCR2_RTCOE      (1 << 3)
#define     RCR2_ADJ30      (1 << 2)
#define     RCR2_RESET      (1 << 1)
#define     RCR2_START      (1 << 0)
#define REG_RCR3(base)		((base) + 0x26)
#define     RCR3_RTCEN      (1 << 0)
#define     RCR3_RTCDV_MSK  (7 << 1)
#define REG_RCR4(base)		((base) + 0x28)
#define     RCR4_RCKSEL     (1 << 0) /* 1 = main clock */
#define REG_RFRH(base)		((base) + 0x2a)
#define REG_RFRL(base)		((base) + 0x2c)
#define REG_RADJ(base)		((base) + 0x2e)
#define REG_RTCCR0(base)	((base) + 0x40)
#define REG_RTCCR1(base)	((base) + 0x42)
#define REG_RTCCR2(base)	((base) + 0x44)
#define REG_RSECCP0(base)	((base) + 0x52)
#define REG_RSECCP1(base)	((base) + 0x62)
#define REG_RSECCP2(base)	((base) + 0x72)
#define REG_RMINCP0(base)	((base) + 0x54)
#define REG_RMINCP1(base)	((base) + 0x64)
#define REG_RMINCP2(base)	((base) + 0x74)
#define REG_RHRCP0(base)	((base) + 0x56)
#define REG_RHRCP1(base)	((base) + 0x66)
#define REG_RHRCP2(base)	((base) + 0x76)
#define REG_RDAYCP0(base)	((base) + 0x5a)
#define REG_RDAYCP1(base)	((base) + 0x6a)
#define REG_RDAYCP2(base)	((base) + 0x7a)
#define REG_RMONCP0(base)	((base) + 0x5c)
#define REG_RMONCP1(base)	((base) + 0x6c)
#define REG_RMONCP2(base)	((base) + 0x7c)

typedef struct RxRtc {
    BusDevice bdev;
    DiskImage *diskImage;
    RTimeHostOfsUs timeOffset; /* Difference to the host system time */
    uint32_t dayOffset;        /* difference of set wday to real week day belonging to the time, NOT WORKING CURRENTLY */ 
    CycleCounter_t busyUntil;  /* Some register changes require time */ 
    Clock_t *clkSubIn;
    Clock_t *clkMainIn;	
    Clock_t *clk128;
    RTime rtime;
    uint8_t regR64CNT;
    uint8_t regRSECCNT;
    uint8_t regRMINCNT;
    uint8_t regRHRCNT;
    uint8_t regRWKCNT;
    uint8_t regRDAYCNT;
    uint8_t regRMONCNT;
    uint16_t regRYRCNT;
    uint8_t regRSECAR;
    uint8_t regRMINAR;
    uint8_t regRHRAR;
    uint8_t regRWKAR;
    uint8_t regRDAYAR;
    uint8_t regRMONAR;
    uint16_t regRYRAR;
    uint8_t regRYRAREN;
    uint8_t regjRCR1;
    uint8_t regRCR2;
    uint8_t regRCR3;
    uint8_t regRCR4;
    uint32_t regRFR;
    uint8_t regRADJ;
    uint8_t regRTCCR0;
    uint8_t regRTCCR1;
    uint8_t regRTCCR2;
    uint8_t regRSECCP0;
    uint8_t regRSECCP1;
    uint8_t regRSECCP2;
    uint8_t regRMINCP0;
    uint8_t regRMINCP1;
    uint8_t regRMINCP2;
    uint8_t regRHRCP0;
    uint8_t regRHRCP1;
    uint8_t regRHRCP2;
    uint8_t regRDAYCP0;
    uint8_t regRDAYCP1;
    uint8_t regRDAYCP2;
    uint8_t regRMONCP0;
    uint8_t regRMONCP1;
    uint8_t regRMONCP2;
} RxRtc;

#define RXRTC_IMAGESIZE  128

#define BUSY_TIME_REGWRITE (150)
/*
 ***************************************************************************
 * Registers for Time are not updated for about 150ns after write,
 * so readback gives the old value.  
 ***************************************************************************
 */
static void
make_busy(RxRtc * rtc, uint32_t nseconds)
{
    rtc->busyUntil = CycleCounter_Get() + NanosecondsToCycles(nseconds);
}

static bool
check_busy(RxRtc * rtc)
{
    if (CycleCounter_Get() < rtc->busyUntil) {
        return true;
    } else {
        return false;
    }
}

static void
update_clock(RxRtc *rtc) {
    if (rtc->regRCR4 & RCR4_RCKSEL) {
        uint32_t div = (rtc->regRFR + 1);
        Clock_MakeDerived(rtc->clk128, rtc->clkMainIn, 1, div);

    } else {
        Clock_MakeDerived(rtc->clk128, rtc->clkSubIn, 1, 256);
    }
}

static void
rtc_save_to_diskimage(RxRtc *rtc)
{
    uint8_t data[RXRTC_IMAGESIZE];
    uint8_t *buf = data;
    int i;
    *buf++ = 0xae;
    *buf++ = 0x3f;
    for (i = 0; i < sizeof(rtc->timeOffset); i++) {
            *(buf++) = (rtc->timeOffset >> (i * 8)) & 0xff;
    }
    *(buf++) = rtc->dayOffset;
    *(buf++) = rtc->regR64CNT;
    *(buf++) = rtc->regRSECCNT;
    *(buf++) = rtc->regRMINCNT;
    *(buf++) = rtc->regRHRCNT;
    *(buf++) = rtc->regRWKCNT;
    *(buf++) = rtc->regRDAYCNT;
    *(buf++) = rtc->regRMONCNT;
    *(buf++) = rtc->regRYRCNT & 0xff;
    *(buf++) = (rtc->regRYRCNT >> 8) & 0xff;
    *(buf++) = rtc->regRSECAR;
    *(buf++) = rtc->regRMINAR;
    *(buf++) = rtc->regRHRAR;
    *(buf++) = rtc->regRWKAR;
    *(buf++) = rtc->regRDAYAR;
    *(buf++) = rtc->regRMONAR;
    *(buf++) = rtc->regRYRAR & 0xff;
    *(buf++) = (rtc->regRYRAR >> 8) & 0xff;
    *(buf++) = rtc->regRCR2;
    if (!rtc->diskImage) {
        fprintf(stderr, "Warning, no diskimage for RX63 RTC\n");
        return;
    }
    if (DiskImage_Write(rtc->diskImage, 0, data, buf - data) < (buf - data)) {
        fprintf(stderr, "Error writing to RX63 disk image\n");
        return;
    }
}

static int
rtc_load_from_diskimage(RxRtc *rtc)
{
    uint8_t data[RXRTC_IMAGESIZE];
    uint8_t *buf = data;
    int i;
    int count = RXRTC_IMAGESIZE;
    if (!rtc->diskImage) {
            fprintf(stderr, "Warning, no diskimage for RX RTC\n");
            return -1;
    }
    if (DiskImage_Read(rtc->diskImage, 0, buf, count) < count) {
            fprintf(stderr, "Error reading from RX Rtc disk image\n");
            return -1;
    }
    if ((*buf++ != 0xae) || (*buf++ != 0x3f)) {
            fprintf(stderr, "RX Rtc invalid magic, starting with defaults\n");
            rtc->timeOffset = 0;
            rtc->dayOffset = 0;
            return -1;
    }
    rtc->timeOffset = 0;
    for (i = 0; i < sizeof(rtc->timeOffset); i++) {
            rtc->timeOffset |= ((uint64_t) buf[0]) << (i * 8);
            buf++;
    }
    rtc->dayOffset = *(buf++);
    rtc->regR64CNT = *(buf++);
    rtc->regRSECCNT = *(buf++);
    rtc->regRMINCNT = *(buf++);
    rtc->regRHRCNT = *(buf++);
    rtc->regRWKCNT = *(buf++);
    rtc->regRDAYCNT = *(buf++);
    rtc->regRMONCNT = *(buf++);
    rtc->regRYRCNT = *(buf++);;
    rtc->regRYRCNT |= (uint16_t)*(buf++) << 8;
    rtc->regRSECAR = *(buf++);
    rtc->regRMINAR = *(buf++);
    rtc->regRHRAR = *(buf++);
    rtc->regRWKAR = *(buf++);
    rtc->regRDAYAR = *(buf++);
    rtc->regRMONAR = *(buf++);
    rtc->regRYRAR = *(buf++);
    rtc->regRYRAR |= (uint16_t)*(buf++) << 8;
    rtc->regRCR2 = *(buf++);
	return 0;
}

static void RxRtc_UpdateRegHour(RxRtc *rtc) 
{
    RTime *rtime = &rtc->rtime; 
    if (rtc->regRCR2 & RCR2_HR24) {
        rtc->regRHRCNT = Uint8ToBcd(rtime->hour);
    } else {
        int hour;
        bool pm = false;
        if (rtime->hour == 0) {
            hour = 12;
            pm = false;
        } else if (rtime->hour < 12) {
            hour = rtime->hour;
            pm = false;
        } else if (rtime->hour < 13) {
            hour = 12;
            pm = true;
        } else {
            hour = rtime->hour - 12;
            pm = true;
        }
        rtc->regRHRCNT = Uint8ToBcd(hour);
        if (pm) {
            rtc->regRHRCNT |= RHRCNT_PM; 
        }
    }
}

static void
RxRtc_UpdateTime(RxRtc *rtc) 
{
    RTime *rtime = &rtc->rtime;
    /* ReRead from host when running */
    if ((rtc->regRCR2 & RCR2_START) && (rtc->regRCR3 & RCR3_RTCEN)) {
        RTime_ReadWithOffset(rtime, rtc->timeOffset);
        rtc->regRSECCNT = Uint8ToBcd(rtime->sec);
        rtc->regRMINCNT = Uint8ToBcd(rtime->min);
        RxRtc_UpdateRegHour(rtc);
        rtc->regRWKCNT = (rtime->wday + rtc->dayOffset + 7) % 7;
        rtc->regRDAYCNT = Uint8ToBcd(rtime->mday);
        rtc->regRMONCNT = Uint8ToBcd(rtime->month);
        rtc->regRYRCNT = Uint16ToBcd(rtime->year) & 0xff;
        rtc->regR64CNT = rtime->usec * 64 / 1000000;
    } 
}

static void
RxRtc_SetTime(RxRtc *rtc) 
{
    RTime *rtime = &rtc->rtime;
    /*
     **************************************************
     * First calculate the RTime from the RX-Time
     **************************************************
     */
    rtime->sec = BcdToUint8(rtc->regRSECCNT);
    rtime->min = BcdToUint8(rtc->regRMINCNT);
    if (rtc->regRCR2 & RCR2_HR24) {
        rtime->hour = BcdToUint8(rtc->regRHRCNT & 0x3f);
    } else {
        uint8_t hour = BcdToUint8(rtc->regRHRCNT & 0x1f) % 12;
        if (rtc->regRHRCNT & RHRCNT_PM) {
            rtime->hour = hour + 12;
        } else {
            rtime->hour = hour;
        }
    }

    //rtime->wday = rtc->regRWKCNT; /* Redundant information */
    rtime->mday = BcdToUint8(rtc->regRDAYCNT);
    rtime->month = BcdToUint8(rtc->regRMONCNT);
    rtime->year = BcdToUint16(rtc->regRYRCNT) + 2000;

    rtime->wday = rtc->regRWKCNT % 7;
    rtc->dayOffset = (rtime->wday - RTime_CalcWDay(rtime) + 7) % 7;
    rtc->timeOffset = RTime_DiffToHostTime(&rtc->rtime);
    //fprintf(stdout, "Set Time year is %u, new offset %ld\n", rtime->year, rtc->timeOffset / 1000000);
    rtc_save_to_diskimage(rtc);
}

static uint32_t
r64cnt_read(void *clientData, uint32_t address, int rqlen)
{
    RxRtc *rtc = clientData;
    RxRtc_UpdateTime(rtc);

    //fprintf(stdout, "---Usec %d, count %u\n", rtime->usec, count);
    if (check_busy(rtc) == true) {
        fprintf(stderr, "Warning: %s while busy\n", __func__);
        return 0;
    } else {
        return rtc->regR64CNT; 
    }
}

static void
r64cnt_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
    fprintf(stderr,"Writing to readonly register %s\n", __func__);
}

static uint32_t
rseccnt_read(void *clientData, uint32_t address, int rqlen)
{
    RxRtc *rtc = clientData;
    RxRtc_UpdateTime(rtc);
    if (check_busy(rtc) == true) {
        fprintf(stderr, "Warning: %s while busy\n", __func__);
        return 5;
    } else {
        return rtc->regRSECCNT;
    }
}

/**
 **********************************************************************
 * RTC must be stopped bei START bit in RCR3  when writing.
 **********************************************************************
 */
static void
rseccnt_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
    RxRtc *rtc = clientData;
    RxRtc_UpdateTime(rtc); /* for the case it is not stopped */
    rtc->regRSECCNT = value & 0x7f;
    RxRtc_SetTime(rtc);
    make_busy(rtc, BUSY_TIME_REGWRITE);
}

static uint32_t
rmincnt_read(void *clientData, uint32_t address, int rqlen)
{
    RxRtc *rtc = clientData;
    RxRtc_UpdateTime(rtc);
    if (check_busy(rtc) == true) {
        fprintf(stderr, "Warning: %s while busy\n", __func__);
        return 5;
    } else {
        return rtc->regRMINCNT;
    }
}

static void
rmincnt_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
    RxRtc *rtc = clientData;
    RxRtc_UpdateTime(rtc); /* for the case it is not stopped */
    rtc->regRMINCNT = value & 0x7f;
    RxRtc_SetTime(rtc);
    make_busy(rtc, BUSY_TIME_REGWRITE);
}

static uint32_t
rhrcnt_read(void *clientData, uint32_t address, int rqlen)
{
    RxRtc *rtc = clientData;
    RxRtc_UpdateTime(rtc);
    if (check_busy(rtc) == true) {
        fprintf(stderr, "Warning: %s while busy\n", __func__);
        return 5;
    } else {
	    return rtc->regRHRCNT;
    }
}

static void
rhrcnt_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
    RxRtc *rtc = clientData;
    RxRtc_UpdateTime(rtc); /* for the case it is not stopped */
    rtc->regRHRCNT = value & 0x7f;
    RxRtc_SetTime(rtc);
    make_busy(rtc, BUSY_TIME_REGWRITE);
}

static uint32_t
rwkcnt_read(void *clientData, uint32_t address, int rqlen)
{
    RxRtc *rtc = clientData;
    RxRtc_UpdateTime(rtc);
    if (check_busy(rtc) == true) {
        fprintf(stderr, "Warning: %s while busy\n", __func__);
        return 5;
    } else {
	    return rtc->regRWKCNT;
    }
}

static void
rwkcnt_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
    RxRtc *rtc = clientData;
    RxRtc_UpdateTime(rtc); /* for the case it is not stopped */
    rtc->regRWKCNT = value & 0x7;
    RxRtc_SetTime(rtc);
    make_busy(rtc, BUSY_TIME_REGWRITE);
}

static uint32_t
rdaycnt_read(void *clientData, uint32_t address, int rqlen)
{
    RxRtc *rtc = clientData;
    RxRtc_UpdateTime(rtc);
    if (check_busy(rtc) == true) {
        fprintf(stderr, "Warning: %s while busy\n", __func__);
        return 5;
    } else {
	    return rtc->regRDAYCNT;
    }
}

static void
rdaycnt_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
    RxRtc *rtc = clientData;
    RxRtc_UpdateTime(rtc); /* for the case it is not stopped */
    rtc->regRDAYCNT = value & 0x3f;
    RxRtc_SetTime(rtc);
    make_busy(rtc, BUSY_TIME_REGWRITE);
}

static uint32_t
rmoncnt_read(void *clientData, uint32_t address, int rqlen)
{
    RxRtc *rtc = clientData;
    RxRtc_UpdateTime(rtc);
    if (check_busy(rtc) == true) {
        fprintf(stderr, "Warning: %s while busy\n", __func__);
        return 5;
    } else {
        return rtc->regRMONCNT;
    }
}

static void
rmoncnt_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
    RxRtc *rtc = clientData;
    RxRtc_UpdateTime(rtc); /* for the case it is not stopped */
    rtc->regRMONCNT = value & 0x1f;
    RxRtc_SetTime(rtc);
    make_busy(rtc, BUSY_TIME_REGWRITE);
}

static uint32_t
ryrcnt_read(void *clientData, uint32_t address, int rqlen)
{
    RxRtc *rtc = clientData;
    RxRtc_UpdateTime(rtc);
    if (check_busy(rtc) == true) {
        fprintf(stderr, "Warning: %s while busy\n", __func__);
        return 5;
    } else {
	    return rtc->regRYRCNT;
    }
}

static void
ryrcnt_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
    RxRtc *rtc = clientData;
    RxRtc_UpdateTime(rtc); /* for the case it is not stopped */
    rtc->regRYRCNT = value & 0xff;
    //fprintf(stdout, "Write year to %04x\n", value);
    RxRtc_SetTime(rtc);
    make_busy(rtc, BUSY_TIME_REGWRITE);
}

static uint32_t
rsecar_read(void *clientData, uint32_t address, int rqlen)
{
	return 0;
}

static void
rsecar_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
}

static uint32_t
rminar_read(void *clientData, uint32_t address, int rqlen)
{
	return 0;
}

static void
rminar_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
}

static uint32_t
rhrar_read(void *clientData, uint32_t address, int rqlen)
{
	return 0;
}

static void
rhrar_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
}

static uint32_t
rwkar_read(void *clientData, uint32_t address, int rqlen)
{
	return 0;
}

static void
rwkar_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
}

static uint32_t
rdayar_read(void *clientData, uint32_t address, int rqlen)
{
	return 0;
}

static void
rdayar_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
}

static uint32_t
rmonar_read(void *clientData, uint32_t address, int rqlen)
{
	return 0;
}

static void
rmonar_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
}

static uint32_t
ryrar_read(void *clientData, uint32_t address, int rqlen)
{
	return 0;
}

static void
ryrar_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
}

static uint32_t
ryraren_read(void *clientData, uint32_t address, int rqlen)
{
	return 0;
}

static void
ryraren_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
}

static uint32_t
rcr1_read(void *clientData, uint32_t address, int rqlen)
{
	return 0;
}

static void
rcr1_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
}

static uint32_t
rcr2_read(void *clientData, uint32_t address, int rqlen)
{
    RxRtc *rtc = clientData;
    rtc->regRCR2 &= ~RCR2_RESET;
    return rtc->regRCR2;
}

static void
rcr2_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
    RxRtc *rtc = clientData;
    uint8_t diff = rtc->regRCR2 ^ value;
    rtc->regRCR2 = value & 0x7f;
    if (diff & RCR2_START) {
        if (value & RCR2_START) {
            /* The time currently in the rtc registers is declared current */
            RxRtc_SetTime(rtc); 
        } else {
            /* Actualize the RTC registers a last time */
            RxRtc_UpdateTime(rtc); 
        }
    }
    rtc_save_to_diskimage(rtc);
}

static uint32_t
rcr3_read(void *clientData, uint32_t address, int rqlen)
{
    RxRtc *rtc = clientData;
    return rtc->regRCR3;
}

static void
rcr3_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
    RxRtc *rtc = clientData;
    rtc->regRCR3 = value & 0xf;
}

static uint32_t
rcr4_read(void *clientData, uint32_t address, int rqlen)
{
    RxRtc *rtc = clientData;
    return rtc->regRCR4;
}

static void
rcr4_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
    RxRtc *rtc = clientData;
    rtc->regRCR4 = value & RCR4_RCKSEL;
    update_clock(rtc); 
}

static uint32_t
rfrl_read(void *clientData, uint32_t address, int rqlen)
{
    RxRtc *rtc = clientData;
	return rtc->regRFR & 0xffff;
}

static void
rfrl_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
    RxRtc *rtc = clientData;
    rtc->regRFR =  (rtc->regRFR & 0xffff0000) | (value & 0xffff) | (rtc->regRFR & 0xffff0000); 
    update_clock(rtc); 
}

static uint32_t
rfrh_read(void *clientData, uint32_t address, int rqlen)
{
    RxRtc *rtc = clientData;
    rtc->regRFR =  (rtc->regRFR >> 16); 
    update_clock(rtc); 
    return 0;
}

static void
rfrh_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
    RxRtc *rtc = clientData;
    rtc->regRFR =  (rtc->regRFR & 0xffff) | ((value & 0x0001) << 16); 
    update_clock(rtc); 
}

static uint32_t
radj_read(void *clientData, uint32_t address, int rqlen)
{
	return 0;
}

static void
radj_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
}

static uint32_t
rtccr0_read(void *clientData, uint32_t address, int rqlen)
{
	return 0;
}

static void
rtccr0_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
}

static uint32_t
rtccr1_read(void *clientData, uint32_t address, int rqlen)
{
	return 0;
}

static void
rtccr1_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
}

static uint32_t
rtccr2_read(void *clientData, uint32_t address, int rqlen)
{
	return 0;
}

static void
rtccr2_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
}

static uint32_t
rseccp0_read(void *clientData, uint32_t address, int rqlen)
{
	return 0;
}

static void
rseccp0_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
}

static uint32_t
rseccp1_read(void *clientData, uint32_t address, int rqlen)
{
	return 0;
}

static void
rseccp1_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
}

static uint32_t
rseccp2_read(void *clientData, uint32_t address, int rqlen)
{
	return 0;
}

static void
rseccp2_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
}

static uint32_t
rmincp0_read(void *clientData, uint32_t address, int rqlen)
{
	return 0;
}

static void
rmincp0_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
}

static uint32_t
rmincp1_read(void *clientData, uint32_t address, int rqlen)
{
	return 0;
}

static void
rmincp1_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
}

static uint32_t
rmincp2_read(void *clientData, uint32_t address, int rqlen)
{
	return 0;
}

static void
rmincp2_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
}

static uint32_t
rhrcp0_read(void *clientData, uint32_t address, int rqlen)
{
	return 0;
}

static void
rhrcp0_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
}

static uint32_t
rhrcp1_read(void *clientData, uint32_t address, int rqlen)
{
	return 0;
}

static void
rhrcp1_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
}

static uint32_t
rhrcp2_read(void *clientData, uint32_t address, int rqlen)
{
	return 0;
}

static void
rhrcp2_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
}

static uint32_t
rdaycp0_read(void *clientData, uint32_t address, int rqlen)
{
	return 0;
}

static void
rdaycp0_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
}

static uint32_t
rdaycp1_read(void *clientData, uint32_t address, int rqlen)
{
	return 0;
}

static void
rdaycp1_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
}

static uint32_t
rdaycp2_read(void *clientData, uint32_t address, int rqlen)
{
	return 0;
}

static void
rdaycp2_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
}

static uint32_t
rmoncp0_read(void *clientData, uint32_t address, int rqlen)
{
	return 0;
}

static void
rmoncp0_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
}

static uint32_t
rmoncp1_read(void *clientData, uint32_t address, int rqlen)
{
	return 0;
}

static void
rmoncp1_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
}

static uint32_t
rmoncp2_read(void *clientData, uint32_t address, int rqlen)
{
	return 0;
}

static void
rmoncp2_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
}

static void
RxRtc_Unmap(void *owner, uint32_t base, uint32_t mask)
{
	//IOH_Delete8(REG_ODR(base, i));
	IOH_Delete8(REG_R64CNT(base));
	IOH_Delete8(REG_RSECCNT(base));
	IOH_Delete8(REG_RMINCNT(base));
	IOH_Delete8(REG_RHRCNT(base));
	IOH_Delete8(REG_RWKCNT(base));
	IOH_Delete8(REG_RDAYCNT(base));
	IOH_Delete8(REG_RMONCNT(base));
	IOH_Delete16(REG_RYRCNT(base));
	IOH_Delete8(REG_RSECAR(base));
	IOH_Delete8(REG_RMINAR(base));
	IOH_Delete8(REG_RHRAR(base));
	IOH_Delete8(REG_RWKAR(base));
	IOH_Delete8(REG_RDAYAR(base));
	IOH_Delete8(REG_RMONAR(base));
	IOH_Delete16(REG_RYRAR(base));
	IOH_Delete8(REG_RYRAREN(base));
	IOH_Delete8(REG_RCR1(base));
	IOH_Delete8(REG_RCR2(base));
	IOH_Delete8(REG_RCR3(base));
	IOH_Delete8(REG_RCR4(base));
	IOH_Delete16(REG_RFRH(base));
	IOH_Delete16(REG_RFRL(base));
	IOH_Delete8(REG_RADJ(base));
	IOH_Delete8(REG_RTCCR0(base));
	IOH_Delete8(REG_RTCCR1(base));
	IOH_Delete8(REG_RTCCR2(base));
	IOH_Delete8(REG_RSECCP0(base));
	IOH_Delete8(REG_RSECCP1(base));
	IOH_Delete8(REG_RSECCP2(base));
	IOH_Delete8(REG_RMINCP0(base));
	IOH_Delete8(REG_RMINCP1(base));
	IOH_Delete8(REG_RMINCP2(base));
	IOH_Delete8(REG_RHRCP0(base));
	IOH_Delete8(REG_RHRCP1(base));
	IOH_Delete8(REG_RHRCP2(base));
	IOH_Delete8(REG_RDAYCP0(base));
	IOH_Delete8(REG_RDAYCP1(base));
	IOH_Delete8(REG_RDAYCP2(base));
	IOH_Delete8(REG_RMONCP0(base));
	IOH_Delete8(REG_RMONCP1(base));
	IOH_Delete8(REG_RMONCP2(base));
}

static void
RxRtc_Map(void *owner, uint32_t base, uint32_t mask, uint32_t mapflags)
{
	RxRtc *rtc = owner;
	IOH_New8(REG_R64CNT(base), r64cnt_read, r64cnt_write, rtc);
	IOH_New8(REG_RSECCNT(base), rseccnt_read, rseccnt_write, rtc);
	IOH_New8(REG_RMINCNT(base), rmincnt_read, rmincnt_write, rtc);
	IOH_New8(REG_RHRCNT(base), rhrcnt_read, rhrcnt_write, rtc);
	IOH_New8(REG_RWKCNT(base), rwkcnt_read, rwkcnt_write, rtc);
	IOH_New8(REG_RDAYCNT(base), rdaycnt_read, rdaycnt_write, rtc);
	IOH_New8(REG_RMONCNT(base), rmoncnt_read, rmoncnt_write, rtc);
	IOH_New16(REG_RYRCNT(base), ryrcnt_read, ryrcnt_write, rtc);
	IOH_New8(REG_RSECAR(base), rsecar_read, rsecar_write, rtc);
	IOH_New8(REG_RMINAR(base), rminar_read, rminar_write, rtc);
	IOH_New8(REG_RHRAR(base), rhrar_read, rhrar_write, rtc);
	IOH_New8(REG_RWKAR(base), rwkar_read, rwkar_write, rtc);
	IOH_New8(REG_RDAYAR(base), rdayar_read, rdayar_write, rtc);
	IOH_New8(REG_RMONAR(base), rmonar_read, rmonar_write, rtc);
	IOH_New16(REG_RYRAR(base), ryrar_read, ryrar_write, rtc);
	IOH_New8(REG_RYRAREN(base), ryraren_read, ryraren_write, rtc);
	IOH_New8(REG_RCR1(base), rcr1_read, rcr1_write, rtc);
	IOH_New8(REG_RCR2(base), rcr2_read, rcr2_write, rtc);
	IOH_New8(REG_RCR3(base), rcr3_read, rcr3_write, rtc);
	IOH_New8(REG_RCR4(base), rcr4_read, rcr4_write, rtc);
	IOH_New16(REG_RFRH(base), rfrh_read, rfrh_write, rtc);
	IOH_New16(REG_RFRL(base), rfrl_read, rfrl_write, rtc);
	IOH_New8(REG_RADJ(base), radj_read, radj_write, rtc);
	IOH_New8(REG_RTCCR0(base), rtccr0_read, rtccr0_write, rtc);
	IOH_New8(REG_RTCCR1(base), rtccr1_read, rtccr1_write, rtc);
	IOH_New8(REG_RTCCR2(base), rtccr2_read, rtccr2_write, rtc);
	IOH_New8(REG_RSECCP0(base), rseccp0_read, rseccp0_write, rtc);
	IOH_New8(REG_RSECCP1(base), rseccp1_read, rseccp1_write, rtc);
	IOH_New8(REG_RSECCP2(base), rseccp2_read, rseccp2_write, rtc);
	IOH_New8(REG_RMINCP0(base), rmincp0_read, rmincp0_write, rtc);
	IOH_New8(REG_RMINCP1(base), rmincp1_read, rmincp1_write, rtc);
	IOH_New8(REG_RMINCP2(base), rmincp2_read, rmincp2_write, rtc);
	IOH_New8(REG_RHRCP0(base), rhrcp0_read, rhrcp0_write, rtc);
	IOH_New8(REG_RHRCP1(base), rhrcp1_read, rhrcp1_write, rtc);
	IOH_New8(REG_RHRCP2(base), rhrcp2_read, rhrcp2_write, rtc);
	IOH_New8(REG_RDAYCP0(base), rdaycp0_read, rdaycp0_write, rtc);
	IOH_New8(REG_RDAYCP1(base), rdaycp1_read, rdaycp1_write, rtc);
	IOH_New8(REG_RDAYCP2(base), rdaycp2_read, rdaycp2_write, rtc);
	IOH_New8(REG_RMONCP0(base), rmoncp0_read, rmoncp0_write, rtc);
	IOH_New8(REG_RMONCP1(base), rmoncp1_read, rmoncp1_write, rtc);
	IOH_New8(REG_RMONCP2(base), rmoncp2_read, rmoncp2_write, rtc);
}


BusDevice *
Rx63nRtc_New(const char *name)
{
	RxRtc *rtc = sg_new(RxRtc);
	const char *dirname;
	char *imagename;
	rtc->bdev.first_mapping = NULL;
	rtc->bdev.Map = RxRtc_Map;
	rtc->bdev.UnMap = RxRtc_Unmap;
	rtc->bdev.owner = rtc;
	rtc->bdev.hw_flags = MEM_FLAG_WRITABLE | MEM_FLAG_READABLE;
	rtc->clkSubIn = Clock_New("%s.subClkIn",name);
	rtc->clkMainIn = Clock_New("%s.mainClkIn",name);	
	rtc->clk128 = Clock_New("%s.clk128",name);	
    rtc->regRCR4 = 0;
    update_clock(rtc);

	dirname = Config_ReadVar("global", "imagedir");
        if (dirname) {
                imagename = alloca(strlen(dirname) + strlen(name) + 20);
                sprintf(imagename, "%s/%s.img", dirname, name);
                rtc->diskImage = DiskImage_Open(imagename, RXRTC_IMAGESIZE, DI_RDWR | DI_CREAT_00);
                if (!rtc->diskImage) {
                        fprintf(stderr, "Failed to open RX Rtc time offset file, using offset 0\n");
                        rtc->timeOffset = 0;
                } else {
                        if (rtc_load_from_diskimage(rtc) < 0) {
                               // rtc->status_reg |= STATUS_OSF;
                            rtc->timeOffset = 0;
                        } else {
                            RxRtc_UpdateTime(rtc);
                        }
                }
        } else {
                rtc->timeOffset = 0;
        }
	return &rtc->bdev;
}
