/*
 **************************************************************************************************
 *
 * Throttle a CPU to its real speed by pausing if the cycle counter is ahead of the real time
 *
 * Status: working
 *
 * Copyright 2012 Jochen Karrer. All rights reserved.
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

#include <time.h>
#include <stdint.h>
#include "cycletimer.h"
#include "signode.h"
#include "sglib.h"
#include "sgstring.h"
#include "throttle.h"
#include "configfile.h"

struct Throttle {
	struct timespec tv_last_throttle;
	CycleCounter_t last_throttle_cycles;
	CycleTimer throttle_timer;
	int64_t cycles_ahead;	/* Number of cycles ahead of real cpu */
	uint32_t sleepsPerSecond;
	/* Control loop for the sound */
	SigNode *sigSpeedUp;
	SigNode *sigSpeedDown;
};

/**
 ******************************************************************
 * \fn static void throttle_proc(void *clientData)
 * Timer handler checking if the cpu cycles is ahead of the
 * excpected cycle counter. If yes sleep for some time. 
 * The CPU speed can be varied by some percent using a "Speed Up"
 * and "Speed Down" signal from outside. This is used by the
 * sound backend for adjusting the CPU speed exactly to the
 * sampling frequency of the sound device. 
 ******************************************************************
 */
static void
throttle_proc(void *clientData)
{
	Throttle *th = (Throttle *) clientData;
	struct timespec tv_now;
	uint32_t nsecs;
	uint32_t sleepCnt = 0;
	int64_t exp_cpu_cycles, done_cpu_cycles;
	done_cpu_cycles = CycleCounter_Get() - th->last_throttle_cycles;
	th->cycles_ahead += done_cpu_cycles;
	do {
		struct timespec tout;
		tout.tv_nsec = 200 * 1000; /* 0.2 ms */
		tout.tv_sec = 0;

#if defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 201112L)
		timespec_get(&tv_now, TIME_UTC);
#else
		clock_gettime(CLOCK_MONOTONIC, &tv_now);
#endif
		nsecs = (tv_now.tv_nsec - th->tv_last_throttle.tv_nsec) +
		    (int64_t) 1000000000 * (tv_now.tv_sec - th->tv_last_throttle.tv_sec);
		exp_cpu_cycles = NanosecondsToCycles(nsecs);
		if (SigNode_Val(th->sigSpeedUp) == SIG_HIGH) {
			exp_cpu_cycles += exp_cpu_cycles >> 4;
		} else if (SigNode_Val(th->sigSpeedDown) == SIG_HIGH) {
			exp_cpu_cycles -= exp_cpu_cycles >> 4;
		}
		if (th->cycles_ahead > exp_cpu_cycles) {
			//FIXME: FIO_WaitEventTimeout(&tout);
			sleepCnt++;
		}
	} while (th->cycles_ahead > exp_cpu_cycles);
	th->cycles_ahead -= exp_cpu_cycles;
	th->last_throttle_cycles = CycleCounter_Get();
	th->tv_last_throttle = tv_now;
	/*  
	 **********************************************************
	 * Forget about catch up if CPU is more than on second 
	 * behind to avoid a longer phase of overspeed. Sound
	 * doesn't like it !
	 **********************************************************
	 */
	if (-th->cycles_ahead > (CycleTimerRate_Get() >> 2)) {
		th->cycles_ahead = 0;
	}
	if(sleepCnt > 1) {
		th->sleepsPerSecond = th->sleepsPerSecond + 1 + (th->sleepsPerSecond >> 8);
	//	fprintf(stderr,"SleepCnt is %u, sps %u\n",sleepCnt,th->sleepsPerSecond);
	} else if((sleepCnt == 0) && (th->sleepsPerSecond > 40)) {
		th->sleepsPerSecond--;
	//fprintf(stderr,"SleepCnt is %u, sps %u\n",sleepCnt,th->sleepsPerSecond);
	}
	CycleTimer_Mod(&th->throttle_timer, CycleTimerRate_Get() / th->sleepsPerSecond);
	return;
}

Throttle *
Throttle_New(const char *name)
{
	uint32_t throttle_enable = 1;
	Throttle *th = sg_new(Throttle);
	th->sigSpeedUp = SigNode_New("%s.throttle.speedUp", name);
	th->sigSpeedDown = SigNode_New("%s.throttle.speedDown", name);
	if (!th->sigSpeedUp || !th->sigSpeedDown) {
		fprintf(stderr, "Can not create throttleControl signal lines\n");
		exit(1);
	}
	SigNode_Set(th->sigSpeedUp, SIG_PULLDOWN);
	SigNode_Set(th->sigSpeedDown, SIG_PULLDOWN);
#if defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 201112L)
	timespec_get(&th->tv_last_throttle, TIME_UTC);
#else
	clock_gettime(CLOCK_MONOTONIC, &th->tv_last_throttle);
#endif
	th->last_throttle_cycles = 0;
	th->sleepsPerSecond = 100; /* Start Value, Sleep 100 times per second */
	Config_ReadUInt32(&throttle_enable, name, "throttle");
	if (throttle_enable) {
		CycleTimer_Add(&th->throttle_timer, CycleTimerRate_Get() / 40, throttle_proc, th);
	}
	return th;
}
