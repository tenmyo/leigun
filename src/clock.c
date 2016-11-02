/*
 ************************************************************************************************ 
 * clock.c
 *      Emulation of clock trees 
 *
 * Copyright 2006 Jochen Karrer. All rights reserved.
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
 ************************************************************************************************ 
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include "clock.h"
#include "strhash.h"
#include "sgstring.h"
#include "interpreter.h"

static Clock_t *systemMasterClock = NULL;
static uint64_t systemMasterClock_Version = 1;
static SHashTable clock_hash;
ClockTrace_t *systemMasterClockTrace = NULL;


/**
 *****************************************************************
 * \fn void master_clock_trace(struct Clock *clock,void *clientData)
 * Increment the versions number of the system master clock 
 * whenever the clock frequency changes
 * to force recalulation of ratios to master clock.
 *****************************************************************
 */
void 
master_clock_trace(struct Clock *clock,void *clientData)
{
	systemMasterClock_Version++;
}

/**
 ********************************************************************
 * \fn void Clock_MakeSystemMaster(Clock_t *clock) 
 * Declare a clock to be the timing source of the
 * complete system. Typically this is the CPU cycle counter.
 ********************************************************************
 */
void 
Clock_MakeSystemMaster(Clock_t *clock) 
{
	if(systemMasterClockTrace) {
		Clock_Untrace(systemMasterClock,systemMasterClockTrace);
	}
	systemMasterClock = clock;
	systemMasterClockTrace 
		= Clock_Trace(clock,master_clock_trace,NULL);
	systemMasterClock_Version++;	
}

/*
 ***************************************+
 * find gcd euklid used to reduce 
 * clock multiplier/divider pairs
 ***************************************+
 */
static inline uint64_t 
find_gcd_mod(uint64_t u, uint64_t v){
        uint64_t tmp;
	while( u > 0) {
            tmp = u;
	    u = v % u;
	    v = tmp;
        }
        return v;
}


/*
 ********************************************
 * reduce a fraction
 ********************************************
 */
static inline void 
reduce_fraction(uint64_t *pu,uint64_t *pv) 
{
	uint64_t gcd = find_gcd_mod(*pu,*pv);
	if(gcd > 1) {
		*pu = *pu / gcd;
		*pv = *pv / gcd;
	}
}

/*
 **************************************************************************
 * Returns ratio between clock and Master Clock (ClockFreq/MasterFreq)
 **************************************************************************
 */
FractionU64_t
Clock_MasterRatio(Clock_t *clock) 
{
	FractionU64_t frac;
	Clock_t *sm;
	if(clock->systemMasterClock_Version != systemMasterClock_Version) {
		sm = systemMasterClock;
		if(!sm) {
			fprintf(stderr,"Bug: System master clock is not existing\n");
			exit(1);
		}
		clock->ratio_nom = clock->acc_nom * sm->acc_denom;
		clock->ratio_denom = clock->acc_denom * sm->acc_nom;
		reduce_fraction(&clock->ratio_nom,&clock->ratio_denom);
		clock->systemMasterClock_Version = systemMasterClock_Version;
	}	
	frac.nom = clock->ratio_nom;
	frac.denom = clock->ratio_denom;
	return frac;
}
/*
 *********************************************************
 * Set the frequency of a clock. Invoke the traces 
 * Internal variant called by dependency tree.
 *********************************************************
 */
static void 
Clock_UpdateChild(Clock_t *clock) 
{
	ClockTrace_t *trace;
	ClockTrace_t *next;
	uint64_t nom,denom;
	uint64_t acc_nom,acc_denom;
	Clock_t *child, *next_sibling;
	if(!clock->parent) {
		fprintf(stderr,"Update child clock without parent: %s\n",clock->name);
		exit(1);
	}
	nom = clock->derivation_nom;
	denom = clock->derivation_denom;
	acc_denom = clock->parent->acc_denom * denom;
	acc_nom = clock->parent->acc_nom * nom;
	reduce_fraction(&acc_nom,&acc_denom);	
	if((clock->acc_nom == acc_nom) && (clock->acc_denom == acc_denom)) {
		return;
	}
	clock->acc_nom = acc_nom;
	clock->acc_denom = acc_denom;
	clock->systemMasterClock_Version = 0;
	for(child = clock->first_child; child; child = next_sibling) {
		/* a clock might remove itself from the parent */
		next_sibling = child->next_sibling;
		Clock_UpdateChild(child);
	}
	for(trace = clock->traceHead;trace;trace = next) {
		next = trace->next; 
		if(trace->proc) {
			trace->proc(clock,trace->clientData);
		}	
	}
}

/*
 *************************************************************
 * Clock_SetFreq
 * Variant called from outside, makes a clock
 * to a top level clock
 *************************************************************
 */
void 
Clock_SetFreq(Clock_t *clock,uint64_t hz) 
{
	ClockTrace_t *trace;
	ClockTrace_t *next;
	Clock_t *child, *next_sibling;
	if(clock->parent) {
		fprintf(stderr,"Can not set frequency of a child clock: %s\n",clock->name);
	}
	if(clock->acc_nom == hz) {
		return;
	}
	clock->acc_nom = hz;
	clock->acc_denom = 1;
	clock->derivation_nom = 1;
	clock->derivation_denom = 1;
	clock->systemMasterClock_Version = 0;
	/* Now notify all children */
	for(child = clock->first_child; child; child = next_sibling) {
		next_sibling = child->next_sibling;
		Clock_UpdateChild(child);
	}
	for(trace = clock->traceHead;trace;trace = next) {
		next = trace->next; 
		if(trace->proc) {
			trace->proc(clock,trace->clientData);
		}	
	}
}

/*
 ****************************************************************
 * ClockTrace
 * 	Set a handler which will be invoked when the clock changes
 * 	its frequency.
 ****************************************************************
 */
ClockTrace_t *
Clock_Trace(Clock_t *clock,ClockTraceProc *proc,void *traceData) 
{
	ClockTrace_t *trace = sg_new(ClockTrace_t);	
	trace->proc = proc;
	trace->clientData = traceData;
	trace->next = clock->traceHead;
	clock->traceHead = trace;
	return trace;
};

void
Clock_Untrace(Clock_t *clock,ClockTrace_t *trace) 
{
	ClockTrace_t *cursor,*prev;
	for(prev=NULL,cursor = clock->traceHead; cursor;prev=cursor,cursor=cursor->next) {
		if(trace == cursor) {
			break;
		}
	}
	if(!cursor) {
		fprintf(stderr,"Warning: can not remove nonexisting clock trace\n");
		return;
	}
	if(prev) {
		prev->next=cursor->next;
		return;
	} else {
		clock->traceHead = cursor->next;
	}
}

/*
 * ---------------------------------------------------
 * Registration in namespace is currently missing
 * ---------------------------------------------------
 */
Clock_t * 
Clock_New(const char *format,...)
{
        char name[512];
        va_list ap;
        Clock_t *clock;
        va_start (ap, format);
        vsnprintf(name,512,format,ap);
        va_end(ap);
	clock = sg_new(Clock_t);

	clock->hash_entry=SHash_CreateEntry(&clock_hash,name);
        if(!clock->hash_entry) {
                fprintf(stderr,"Clock: Can not create hash entry for %s\n",name);
                free(clock);
                return NULL;
        }
        SHash_SetValue(clock->hash_entry,clock);
	clock->name = sg_strdup(name);
	clock->acc_denom = clock->derivation_denom = 1;
	return clock;
}


/*
 */
#if 0
void
Clock_Delete(Clock_t *clock) 
{
	for(trace=clock->traceHead;trace;trace=trace->next) {
		if(!trace->isactive && trace->proc) {
			trace->proc(clock,hz,trace->clientData);
		}	
	}
        SHash_DeleteEntry(&clock_hash,clock->hash_entry);
        free(clock->name);
        free(clock);

}
#endif

/*
 *************************************************************
 * Remove a clock from its parents linked child list
 *************************************************************
 */
static void
detach_from_parent(Clock_t *clock) {
	if(clock->parent) {
		if(clock->prev_sibling) {
			if(clock->next_sibling) {
				clock->next_sibling->prev_sibling = clock->prev_sibling;
			}
			clock->prev_sibling->next_sibling = clock->next_sibling;
		} else {
			if(clock->next_sibling) {
				clock->next_sibling->prev_sibling = NULL;
			}
			clock->parent->first_child = clock->next_sibling;
		}
	}
	clock->parent = NULL;
	clock->next_sibling = clock->prev_sibling = NULL;
}
/*
 ************************************************************************
 * Add a clock to the linked list of children in the parent 
 ************************************************************************
 */
static void
attach_to_parent(Clock_t *clock, Clock_t *parent) {
	clock->parent = parent;
	clock->prev_sibling = NULL;
	clock->next_sibling = parent->first_child;
	if(parent->first_child) {
		parent->first_child->prev_sibling = clock;
	}
	parent->first_child = clock;
}

/*
 ************************************************************************
 * Decouple derived clock from its parent 
 ************************************************************************
 */
void
Clock_Decouple(Clock_t *child)  
{
	if(!child->parent)
		return;
	detach_from_parent(child);
	child->derivation_nom = 0;
	child->acc_nom = 0;
	child->systemMasterClock_Version = 0;
}

/*
 * -------------------------------------------------------------------
 * Make a clock to be derived from another clock by a fraction
 * -------------------------------------------------------------------
 */
void
Clock_MakeDerived(Clock_t *child,Clock_t *parent,uint64_t nom,uint64_t denom) 
{	
	if(!denom) {
		fprintf(stderr,"Clock \"%s\": denominator is 0\n",SHash_GetKey(child->hash_entry));
		exit(1);
	}
	reduce_fraction(&nom,&denom);
	/* Clear already existing derivation */
	if(!child) {
		fprintf(stderr,"Emulator bug: access to nonexisting clock\n");
		exit(1);
	}
	/* Remove the child from the old parents queue */
	if(parent != child->parent) {
		detach_from_parent(child);
		attach_to_parent(child,parent); 
	}
	child->derivation_nom = nom;
	child->derivation_denom = denom;
	Clock_UpdateChild(child);
}

Clock_t *
Clock_Find(const char *format,...) 
{
        char name[512];
        va_list ap;
        SHashEntry *entryPtr;
	Clock_t *clock;
        va_start (ap, format);
        vsnprintf(name,sizeof(name),format,ap);
        va_end(ap);
        entryPtr = SHash_FindEntry(&clock_hash,name);
        if(!entryPtr) {
                return NULL;
        }
        clock = SHash_GetValue(entryPtr);
        return clock;

}

void
Clock_Link(const char *child,const char *parent) 
{
	Clock_t *sclock = Clock_Find(child);	
	Clock_t *mclock = Clock_Find(parent);	
	if(!sclock || !mclock) {
		fprintf(stderr,"Trying to link nonexsitent clocks \"%s\" and \"%s\"\n",child,parent); 
		sleep(2);
		return;
	}
	Clock_MakeDerived(sclock,mclock,1,1);
}

void
Clock_Unlink(const char *child) 
{
	Clock_t *sclock = Clock_Find(child);	
	if(!sclock) {
		sleep(2);
		fprintf(stderr,"Trying to unlink nonexsitent clock \"%s\"\n",child); 
		return;
	}
	Clock_Decouple(sclock);
}

static void
_Clock_DumpTree(Clock_t *clock,int indent) 
{
	Clock_t *child;
	int i;
	for(i=0;i<indent;i++) {
		fprintf(stderr,"|  ");
	}
	if(indent) {
		fprintf(stderr,"|-");
	}
	Clock_MasterRatio(clock);
	fprintf(stderr,"%s: %fkHz %"PRId64"/%"PRId64"->%"PRId64"/%"PRId64" MR %"PRId64"/%"PRId64"\n",SHash_GetKey(clock->hash_entry),
		Clock_DFreq(clock)/1000,clock->derivation_nom,clock->derivation_denom,clock->acc_nom,clock->acc_denom,clock->ratio_nom,clock->ratio_denom);

	for(child=clock->first_child;child;child=child->next_sibling) 
	{
		_Clock_DumpTree(child,indent+1);
	}
}

void
Clock_DumpTree(Clock_t *top) 
{
	fprintf(stderr,"\n");
	_Clock_DumpTree(top,0);	
}

int
cmd_clktree(Interp *interp,void *clientData,int argc,char *argv[])
{
	Clock_t *clk;
        if(argc < 2) {
                return CMD_RESULT_BADARGS;
        }
	clk = Clock_Find("%s",argv[1]);
	if(clk) {
		Clock_DumpTree(clk);
	}
        //Interp_AppendResult(interp,"%s\r\n",str);
	//exit(1);
        return CMD_RESULT_OK;
}


void 
ClocksInit(void) {
	SHash_InitTable(&clock_hash);
        if(Cmd_Register("clktree",cmd_clktree,NULL) < 0) {
                fprintf(stderr,"Can not register clock command\n");
                exit(1);
        }
}
