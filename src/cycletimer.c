/*
 *************************************************************************************************
 * Cycle Timer Handler Implementation 
 *
 * Modified version  from Jochen Karrers
 * xy_tools Event-IO System. It uses
 * cycles of emulated CPU as timing source instead of the 
 * system time.  
 * Timers are periodically checked instead of using select 
 *
 * Author: Jochen Karrer
 *
 * Copyright 2002 2004 Jochen Karrer. All rights reserved.
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

#include <cycletimer.h>
#include "clock.h"
#include <xy_tree.h>

/*
 * ----------------------------------------------
 * We only have one global Cycle timer Tree
 * If we emulate more than one CPU they should 
 * have the same frequency 
 * ----------------------------------------------
 */

CycleTimer *firstCycleTimer=0;
xy_node *firstCycleTimerNode=0;
XY_Tree CycleTimerTree;
uint64_t firstCycleTimerTimeout=~(uint64_t)0;
uint64_t CycleCounter=0;
uint32_t CycleTimerRate;
static Clock_t *ct_CpuClk;

/*
 * -----------------------------------------------
 * returns true if time1 is later then time2
 * -----------------------------------------------
 */
static int
is_later(const void *t1,const void *t2) {
        uint64_t *time1 = (uint64_t *) t1;
        uint64_t *time2 = (uint64_t *) t2;
        if(*time2 < *time1) {
                return 1;
        } else  {
                return 0;
        }
}

/*
 * ----------------------------------------------------------
 * Remove Timer from linked list
 * ----------------------------------------------------------
 */

void
CycleTimer_Remove(CycleTimer *timer)
{
        if(unlikely(!timer->isactive))
                return;
        XY_DeleteTreeNode(&CycleTimerTree,&timer->node);
	timer->isactive=0;
        if(timer==XY_NodeValue(firstCycleTimerNode)) {
                CycleTimer *timer;
                firstCycleTimerNode=XY_NextTreeNode(&CycleTimerTree,firstCycleTimerNode);
                if(firstCycleTimerNode) {
                        timer=XY_NodeValue(firstCycleTimerNode);
                        firstCycleTimerTimeout = timer->timeout;
                } else {
                        firstCycleTimerTimeout = ~(uint64_t)0;
                }
        }
}

/*
 ***************************************************
 * Insert timer into the tree
 ***************************************************
 */

void
CycleTimer_Add(CycleTimer *timer,uint64_t cycles,CycleTimer_Proc *proc,void *clientData)
{
        if(unlikely(!proc))
                return;

        timer->proc=proc;
	timer->isactive=1;
        timer->clientData=clientData;
       	timer->timeout = CycleCounter + cycles;
        XY_AddTreeNode(&CycleTimerTree,&timer->node,&timer->timeout,timer);
        if(firstCycleTimerNode) {
                CycleTimer *first_timer = XY_NodeValue(firstCycleTimerNode);
                if(timer->timeout < first_timer->timeout) {
                        firstCycleTimerNode = &timer->node;
                        firstCycleTimerTimeout=timer->timeout;
                }
        } else {
                firstCycleTimerNode=&timer->node;
                firstCycleTimerTimeout=timer->timeout;
        }
}

/*
 *****************************************************************************
 * Trace changes of the CPU clock
 *****************************************************************************
 */
static void
CpuClock_Trace(struct Clock *clock,void *clientData) 
{
	CycleTimerRate = Clock_Freq(clock);
}

void 
CycleTimers_Init(const char *cpu_name,uint32_t freq_hz) {
	CycleTimerRate = freq_hz;
	ct_CpuClk = Clock_New("%s.clk",cpu_name);
	Clock_SetFreq(ct_CpuClk,freq_hz);
	XY_InitTree(&CycleTimerTree,is_later,NULL,NULL,NULL);
	Clock_Trace(ct_CpuClk,CpuClock_Trace,NULL);
	Clock_MakeSystemMaster(ct_CpuClk);
}
