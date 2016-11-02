/*
 *************************************************************************************************
 * Emulation of Freescale iMX21 Watchdog module 
 *
 * state: minimal implementation allowing reboot by watchdog. 
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
#include <sys/ioctl.h>
#include <sys/fcntl.h>
#include "bus.h"
#include "signode.h"
#include "cycletimer.h"
#include "sgstring.h"

#define WDOG_WCR(base) 	((base)+0)
#define		WCR_WT_MASK (0xff<<8)
#define		WCR_WT_SHIFT 	(8)
#define		WCR_WDA		(1<<5)
#define		WCR_SRS		(1<<4)
#define		WCR_WRE		(1<<3)
#define		WCR_WDE		(1<<2)
#define		WCR_WDBG	(1<<1)
#define		WCR_WDZST	(1<<0)
#define WDOG_WSR(base)	((base)+0x2)
#define WDOG_WRSR(base)	((base)+0x4)
#define		WRSR_PWR	(1<<4)
#define		WRSR_EXT	(1<<3)
#define		WRSR_TOUT	(1<<2)
#define		WRSR_SFTW	(1<<1)

#define	WATCHDOG_CLOCK_HZ	(2) 
typedef struct IMX21Wdog {
	BusDevice bdev;
	uint16_t wcr;
	int wcr_once_written;
	uint16_t wsr;
	uint16_t wrsr;

	int count;
	SigNode *nWdog_Node;
        CycleCounter_t last_timer_update;
        CycleCounter_t saved_cpu_cycles;
        CycleTimer event_timer;
} IMX21Wdog;


static void
actualize_count(IMX21Wdog *wdog) 
{
	int divider;
	int count;
	CycleCounter_t timeout;
	if((wdog->wcr & WCR_WDE)
		&& !(wdog->wcr & WCR_WDBG)) {
		wdog->saved_cpu_cycles += CycleCounter_Get() - wdog->last_timer_update;
		divider = CycleTimerRate_Get() / (32768/16384);
		count = wdog->saved_cpu_cycles / divider;
		wdog->saved_cpu_cycles -= count * divider;
		wdog->count -= count;
		/* timeout when when count reaches 0 */
		if(wdog->count <= 0) {
			if(wdog->wcr & WCR_WRE) {
				SigNode_Set(wdog->nWdog_Node,SIG_LOW);
			} else {
				fprintf(stderr,"Watchdog timeout\n");
				exit(0);
			}
		} else {
			timeout = wdog->count * divider - wdog->saved_cpu_cycles; 
			CycleTimer_Mod(&wdog->event_timer,timeout);
		}
	}
}

static void
do_timeout(void *clientData) 
{
	IMX21Wdog *wdog = (IMX21Wdog*) clientData;
	actualize_count(wdog);
}

/*
 * ---------------------------------------------------------------------------
 * WCR:
 * 	Watchdog control register
 *
 * Bit 0 WDZST: 0 = continue during low power, 1 = suspend watchdog
 * Bit 1  WDBG: 0 continue wdog during debug mode 1 = suspend watchdog
 * Bit 2   WDE: Watchdog enable 0 = disable watchdog, 1 enable watchdog
 * Bit 3   WRE: 0 = generate reset on timeout, 1 = generate wdog
 * Bit 4   SRS: 0 Assert system reset signal, 1 = no effect
 * Bit 5   WDA: 0 Assert nWDOG output, 1 = no effect
 * Bit 8-15 WT: Watchdog timeout: load value for counter
 * ---------------------------------------------------------------------------
 */
static uint32_t
wcr_read(void *clientData,uint32_t address,int rqlen)
{
	IMX21Wdog *wdog = (IMX21Wdog *) clientData;
        return wdog->wcr;
}

static void
wcr_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	IMX21Wdog *wdog = (IMX21Wdog *) clientData;
	uint16_t diff;
	actualize_count(wdog);
	if(wdog->wcr_once_written) {
		wdog->wcr = (value & 0xff34) |  (wdog->wcr & 0xf);	
	} else {
		wdog->wcr_once_written = 1;
		wdog->wcr = value & 0xff3f;
	}
	diff = wdog->wcr ^ value;
	if(diff & WCR_WDE) {
		wdog->count = (wdog->wcr >> 8) & 0xff;
	}
	/* should be update_timeout only */
	if(!(wdog->wcr & WCR_WRE) && ((wdog->wcr & WCR_WT_MASK) == 0)) {
		fprintf(stderr,"\nWatchdog module: software reset\n");
		exit(0);
	}
	if(wdog->wcr & WCR_WDA) {
		SigNode_Set(wdog->nWdog_Node,SIG_LOW);
	} else {
		SigNode_Set(wdog->nWdog_Node,SIG_HIGH);
	}
	actualize_count(wdog);
}

/*
 * --------------------------------------------------------------------
 * WSR
 * 	Watchdog service register
 *	write 0x5555, 0xaaaa to this register  
 * --------------------------------------------------------------------
 */
static uint32_t
wsr_read(void *clientData,uint32_t address,int rqlen)
{
	IMX21Wdog *wdog = (IMX21Wdog *) clientData;
        return wdog->wsr;
}

static void
wsr_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	IMX21Wdog *wdog = (IMX21Wdog *) clientData;
	if((value = 0xaaaa) && (wdog->wsr == 0x5555)) {
		actualize_count(wdog);
		wdog->count = (wdog->wcr >> 8) & 0xff;
		// update_timeout() would be better
		actualize_count(wdog);
	}
	wdog->wsr = value;
}

/*
 * --------------------------------------------------------------------
 * WRSR
 * 	Watchdog reset status register
 * --------------------------------------------------------------------
 */
static uint32_t
wrsr_read(void *clientData,uint32_t address,int rqlen)
{
        fprintf(stderr,"Watchdog reset status register read not implemented\n");
        return WRSR_PWR;
}

static void
wrsr_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
        fprintf(stderr,"Watchdog: WRSR register is readonly ! Bus error\n");
}


static void
IMXWdog_Unmap(void *owner,uint32_t base,uint32_t mask)
{
        IOH_Delete16(WDOG_WCR(base));
        IOH_Delete16(WDOG_WSR(base));
        IOH_Delete16(WDOG_WRSR(base));
}

static void
IMXWdog_Map(void *owner,uint32_t base,uint32_t mask,uint32_t mapflags)
{

        IMX21Wdog *wdog = (IMX21Wdog *) owner;
        IOH_New16(WDOG_WCR(base),wcr_read,wcr_write,wdog);
        IOH_New16(WDOG_WSR(base),wsr_read,wsr_write,wdog);
        IOH_New16(WDOG_WRSR(base),wrsr_read,wrsr_write,wdog);
}
BusDevice * 
IMX21_WdogNew(const char *name) 
{
	IMX21Wdog *wdog = sg_new(IMX21Wdog);
	CycleTimer_Init(&wdog->event_timer,do_timeout,wdog);
	wdog->nWdog_Node = SigNode_New("%s.nWDOG",name);
	if(!wdog->nWdog_Node) {
		fprintf(stderr,"Can not create watchdog signal node\n");
		exit(1);
	}
	wdog->wcr = 0x0030;
	wdog->bdev.first_mapping=NULL;
        wdog->bdev.Map=IMXWdog_Map;
        wdog->bdev.UnMap=IMXWdog_Unmap;
        wdog->bdev.owner=wdog;
        wdog->bdev.hw_flags=MEM_FLAG_WRITABLE|MEM_FLAG_READABLE;
	fprintf(stderr,"IMX21 Watchdog module created\n");
	return &wdog->bdev;
}
