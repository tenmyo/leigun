/*
 *************************************************************************************************
 *
 * Senseless poll detector used to lower cpu load when guest OS or bootloader
 * polls some IO registers like UART receive buffer or timer to fast.  
 * When some the rate to high this detector jumps over some cpu cycles 
 * and sleeps if enough cpu cycles are on the account.
 *
 *  State: implementation working with u-boot for 
 *  Atmel AT91RM9200 and Freescale i.MX21
 *
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
#include <string.h>
#include <stdint.h>

#include "configfile.h"
#include "cycletimer.h"
#include "senseless.h"
#include "time.h"
#include "sgstring.h"

typedef struct SenslessMonitor {
	CycleCounter_t last_senseless;
	CycleCounter_t saved_cycles;
	uint32_t nonsense_threshold;
	uint32_t jump_width;
	int64_t overjumped_nanoseconds;
	uint32_t sensivity;
} SenselessMonitor;

static SenselessMonitor *smon;

/*
 * --------------------------------------------------------------------
 * jump 
 * 	Jump over jump_width cycles. Put the saved cycles onto a
 *	nanosecond account. If it is enough to sleep 10 ms
 *	then sleep and subtract the time from the account
 * --------------------------------------------------------------------
 */
static void
jump(SenselessMonitor * smon)
{
	struct timespec tvv, tvn;
	uint32_t jump_width = smon->jump_width;
	CycleCounter_t cnow = CycleCounter_Get();
	if ((cnow + jump_width) >= firstCycleTimerTimeout) {
		jump_width = firstCycleTimerTimeout - cnow;
		CycleCounter = firstCycleTimerTimeout;
	} else {
		CycleCounter += jump_width;
	}
	/* fprintf(stderr,"jump\n"); */
	smon->overjumped_nanoseconds += CyclesToNanoseconds(jump_width);
	smon->saved_cycles -= jump_width;
	while (smon->overjumped_nanoseconds > 11000000) {
		struct timespec tout;
		uint64_t nsecs;
		tout.tv_nsec = 10000000;	/* 10 ms */
		tout.tv_sec = 0;
		clock_gettime(CLOCK_MONOTONIC, &tvv);
		FIO_WaitEventTimeout(&tout);
		clock_gettime(CLOCK_MONOTONIC, &tvn);
		nsecs = tvn.tv_nsec - tvv.tv_nsec + (int64_t) 1000000000 *(tvn.tv_sec - tvv.tv_sec);
		smon->overjumped_nanoseconds -= nsecs * 1.1;
	}
}

/*
 * ---------------------------------------------------------------------
 * Report a possibly senseless poll operation. A high
 * weight means that the operation needs to be executed less often
 * to be detected as senseless. The default weight is 100
 * ---------------------------------------------------------------------
 */
void
Senseless_Report(int weight)
{
	uint64_t diffcycles = CycleCounter_Get() - smon->last_senseless;
	uint64_t consumed_cycles = 2 * diffcycles;
	smon->last_senseless = CycleCounter_Get();
	smon->saved_cycles += NanosecondsToCycles(smon->sensivity * weight);
	if (consumed_cycles > smon->saved_cycles) {
		smon->saved_cycles = 0;
		return;
	}
	smon->saved_cycles -= consumed_cycles;
	if (smon->saved_cycles > smon->nonsense_threshold) {
		jump(smon);
		if (smon->saved_cycles > (smon->nonsense_threshold << 1)) {
			smon->saved_cycles = 0;
		}
	}
}

void
Senseless_Init()
{
	smon = sg_new(SenselessMonitor);
	smon->nonsense_threshold = CycleTimerRate_Get() / 1000;
	smon->jump_width = smon->nonsense_threshold = CycleTimerRate_Get() / 20000;
	smon->sensivity = 10;
	Config_ReadUInt32(&smon->sensivity, "poll_detector", "sensivity");
	Config_ReadUInt32(&smon->jump_width, "poll_detector", "jump_width");
	Config_ReadUInt32(&smon->nonsense_threshold, "poll_detector", "threshold");
	fprintf(stderr, "Poll detector Sensivity %d jump_width %d, jump threshold %d\n",
		smon->sensivity, smon->jump_width, smon->nonsense_threshold);
}
