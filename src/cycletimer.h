/*
 * -------------------------------------------------------------------------
 * Cycle Timer Handler Implementation
 *
 * Modified version  from Jochen Karrers
 * xy_tools Event-IO System. It uses
 * cycles of emulated CPU instead of the system time and
 * is periodically checked instead of using select
 *
 * (C) 2002 Jochen Karrer
 * (C) 2004 Lightmaze Solutions AG
 * Author: Jochen Karrer
 * ----------------------------------------------------------------
 */

#ifndef CYCLETIMER_H
#define CYCLETIMER_H
#include <stdint.h>
#include <xy_tree.h>
#include <compiler_extensions.h>

typedef void CycleTimer_Proc(void *clientData);
typedef uint64_t CycleCounter_t;

// All fields of CycleTimer are private !
typedef struct CycleTimer {
        xy_node node;
        uint64_t timeout; // absolute cycles of timeout
        CycleTimer_Proc *proc;
        void *clientData;
	int isactive;
} CycleTimer;

/*
 * ----------------------------------------------
 * We only have one global Cycle timer Tree
 * If we emulate more than one CPU they should be
 * in sync
 * ----------------------------------------------
 */
extern CycleTimer *firstCycleTimer;
extern xy_node *firstCycleTimerNode;
extern XY_Tree CycleTimerTree;
extern uint64_t firstCycleTimerTimeout;
extern uint64_t CycleCounter;
extern uint32_t CycleTimerRate;

/*
 * -------------------------------------------------
 * This function is called from the CPU main loop
 * after every instruction. So Its inline
 * -------------------------------------------------
 */

static inline
void CycleTimers_Check() {
        if(unlikely(CycleCounter >= firstCycleTimerTimeout)) {
                xy_node *node=firstCycleTimerNode;
                if(node) {
                        CycleTimer *timer=(CycleTimer *)XY_NodeValue(node);
                        CycleTimer_Proc *proc;
                        firstCycleTimerNode = XY_NextTreeNode(&CycleTimerTree,firstCycleTimerNode);
                        if(firstCycleTimerNode) {
                                CycleTimer *timer=(CycleTimer *)XY_NodeValue(firstCycleTimerNode);
                                firstCycleTimerTimeout = timer->timeout;
                        } else {
				// Never
                                firstCycleTimerTimeout = ~0ULL;
                        }
                        XY_DeleteTreeNode(&CycleTimerTree,node);
                        proc = timer->proc;
                        timer->isactive = 0;
			if(likely(proc))
                        	proc(timer->clientData);
                } else {
                        fprintf(stderr,"Bug in timertree\n");
                }
        }
}
static inline int
CycleTimer_IsActive(CycleTimer *timer) {
        return timer->isactive;
}
static inline uint64_t CycleCounter_Get() {
        return CycleCounter;
}

static inline int64_t
CyclesToMilliseconds(int64_t cycles) {
	return (cycles)/(CycleTimerRate/1000);
}

static inline int64_t
CyclesToMicroseconds(int64_t cycles) {
	return (1000*cycles)/(CycleTimerRate/1000);
}
static inline int64_t
CyclesToNanoseconds(int64_t cycles) {
	if((uint64_t)abs(cycles) < (100*CycleTimerRate)) {
		return (10000 * cycles)/(CycleTimerRate/100000); 
	} else {
		return (cycles / CycleTimerRate) * 1000000000; 
	}
}

static inline int64_t
MillisecondsToCycles(int64_t msec) {
	return (msec * (int64_t)CycleTimerRate)/1000;
}

static inline int64_t
MicrosecondsToCycles(int64_t usec) {
	return (usec * (int64_t)CycleTimerRate)/1000000;
}

static inline int64_t
NanosecondsToCycles(int64_t nsec) {
	return (nsec * (int64_t)(CycleTimerRate/1000))/1000000;
}

static inline void 
CycleTimer_Init(CycleTimer *timer,CycleTimer_Proc *proc,void *clientData) {
	timer->isactive=0;
	timer->proc=proc;
	timer->clientData=clientData;
}

void
CycleTimer_Add(CycleTimer *,uint64_t cycles,CycleTimer_Proc *,void *clientData);
void CycleTimer_Remove(CycleTimer *);

static inline void
CycleTimer_Mod(CycleTimer *timer,uint64_t cycles) {
	if(timer->isactive) {
		CycleTimer_Remove(timer);
	}
	CycleTimer_Add(timer,cycles,timer->proc,timer->clientData);
}

static inline uint32_t 
CycleTimerRate_Get() {
	return CycleTimerRate;
}

static inline int64_t 
CycleTimer_GetRemaining(CycleTimer *timer) 
{
	if(!timer->isactive) {
		return 0;
	} else {
		return timer->timeout - CycleCounter_Get();
	}
}

void CycleTimers_Init(const char *cpu_name,uint32_t cpu_clock);

#endif
