/*
 **************************************************************************************************
 *
 * Emulation of logical signals. 
 *
 * Status:
 *	Basically working, needs cleanup
 *
 * Copyright 2004 2009 Jochen Karrer. All rights reserved.
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

#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdarg.h>
#include "sgstring.h"
#include "signode.h"
#include "xy_hash.h"
#include "compiler_extensions.h"
#include "interpreter.h"

static SHashTable signode_hash;
static SigStamp g_stamp = 0;
static int active_trace_deleted_flag = 0;
static SigConflictProc *g_conflictProc = NULL;

static char *
SigVal_String(int sigval)
{
	switch (sigval) {
	    case SIG_LOW:
		    return "Low";
	    case SIG_HIGH:
		    return "High";
	    case SIG_FORCE_LOW:
		    return "ForceLow";
	    case SIG_FORCE_HIGH:
		    return "ForceHigh";
	    case SIG_OPEN:
		    return "Open";
	    case SIG_PULLDOWN:
		    return "Pulldown";
	    case SIG_PULLUP:
		    return "Pullup";
	    case SIG_WEAK_PULLDOWN:
		    return "Weak Pulldown";
	    case SIG_WEAK_PULLUP:
		    return "Weak Pullup";
	    default:
		    return "Undefined";
	}
};

/*
 * ----------------------------------------------------
 * SigNode_Find
 * 	Find a signal by its name in the hash table
 * ----------------------------------------------------
 */

SigNode *
SigNode_Find(const char *format, ...)
{
	char name[512];
	va_list ap;
	SHashEntry *entryPtr;
	SigNode *signode;
	va_start(ap, format);
	vsnprintf(name, sizeof(name), format, ap);
	va_end(ap);
	entryPtr = SHash_FindEntry(&signode_hash, name);
	if (!entryPtr) {
		return NULL;
	}
	signode = SHash_GetValue(entryPtr);
	return signode;
}

/*
 * -------------------------------------------------------------
 * Create a new Signal and enter its name into the Hash Table
 * The default value is SIG_OPEN
 * -------------------------------------------------------------
 */
SigNode *
SigNode_New(const char *format, ...)
{
	char name[512];
	va_list ap;
	SigNode *signode;
	va_start(ap, format);
	vsnprintf(name, 512, format, ap);
	va_end(ap);
	if ((signode = SigNode_Find("%s", name))) {
		fprintf(stderr, "Node \"%s\" already exists\n", name);
		return NULL;
	}
	signode = sg_new(SigNode);
	signode->hash_entry = SHash_CreateEntry(&signode_hash, name);
	if (!signode->hash_entry) {
		fprintf(stderr, "Can not create hash entry for %s\n", name);
		sg_free(signode);
		return NULL;
	}
	SHash_SetValue(signode->hash_entry, signode);
	signode->propval = signode->selfval = SIG_OPEN;
	//signode->propval = SIG_HIGH;
	//signode->propval = SIG_LOW;
	return signode;
}

/*
 * ---------------------------------------------
 * Calculate one Value from two signal values
 * ---------------------------------------------
 */
static uint32_t
calculate_sigval(int val1, int val2)
{
	if (val1 == SIG_OPEN) {
		return val2;
	} else if (val2 == SIG_OPEN) {
		return val1;
	} else if (val1 == SIG_FORCE_LOW) {
		if ((val2 == SIG_HIGH) || (val2 == SIG_FORCE_HIGH)) {
			return SIG_FORCE_LOW | SIG_ILLEGAL;
		} else {
			return SIG_FORCE_LOW;
		}
	} else if (val1 == SIG_FORCE_HIGH) {
		if ((val2 == SIG_LOW) || (val2 == SIG_FORCE_LOW)) {
			return SIG_FORCE_HIGH | SIG_ILLEGAL;
		} else {
			return SIG_FORCE_HIGH;
		}
	} else if (val1 == SIG_LOW) {
		if (val2 == SIG_FORCE_HIGH) {
			return SIG_FORCE_HIGH | SIG_ILLEGAL;
		} else if (val2 == SIG_HIGH) {
			return SIG_LOW | SIG_ILLEGAL;
		}
		return SIG_LOW;
	} else if (val1 == SIG_HIGH) {
		if (val2 == SIG_FORCE_LOW) {
			return SIG_FORCE_LOW | SIG_ILLEGAL;
		} else if (val2 == SIG_LOW) {
			return SIG_HIGH | SIG_ILLEGAL;
		}
		return SIG_HIGH;
	} else if (val1 == SIG_PULLUP) {
		if (val2 == SIG_LOW) {
			return val2;
		} else if (val2 == SIG_FORCE_LOW) {
			return val2;
		} else if (val2 == SIG_FORCE_HIGH) {
			return val2;
		} else if (val2 == SIG_HIGH) {
			return val2;
		} else if (val2 == SIG_PULLDOWN) {
			return SIG_PULLDOWN;
		} else if (val2 == SIG_PULLUP) {
			return SIG_PULLUP;
		} else if (val2 == SIG_WEAK_PULLDOWN) {
			return SIG_PULLUP;
		} else if (val2 == SIG_WEAK_PULLUP) {
			return SIG_PULLUP;
		}
	} else if (val1 == SIG_PULLDOWN) {
		if (val2 == SIG_LOW) {
			return val2;
		} else if (val2 == SIG_FORCE_HIGH) {
			return val2;
		} else if (val2 == SIG_FORCE_LOW) {
			return val2;
		} else if (val2 == SIG_HIGH) {
			return val2;
		} else if (val2 == SIG_PULLDOWN) {
			return SIG_PULLDOWN;
		} else if (val2 == SIG_PULLUP) {
			return SIG_PULLUP;
		} else if (val2 == SIG_WEAK_PULLDOWN) {
			return SIG_PULLDOWN;
		} else if (val2 == SIG_WEAK_PULLUP) {
			return SIG_PULLDOWN;
		}
	} else if (val1 == SIG_WEAK_PULLUP) {
		if (val2 == SIG_LOW) {
			return SIG_LOW;
		} else if (val2 == SIG_FORCE_LOW) {
			return val2;
		} else if (val2 == SIG_FORCE_HIGH) {
			return val2;
		} else if (val2 == SIG_HIGH) {
			return val2;
		} else if (val2 == SIG_PULLDOWN) {
			return SIG_PULLDOWN;
		} else if (val2 == SIG_PULLUP) {
			return SIG_PULLUP;
		} else if (val2 == SIG_WEAK_PULLDOWN) {
			return SIG_WEAK_PULLDOWN;
		} else if (val2 == SIG_WEAK_PULLUP) {
			return SIG_WEAK_PULLUP;
		}
	} else if (val1 == SIG_WEAK_PULLDOWN) {
		if (val2 == SIG_LOW) {
			return val2;
		} else if (val2 == SIG_FORCE_LOW) {
			return val2;
		} else if (val2 == SIG_FORCE_HIGH) {
			return val2;
		} else if (val2 == SIG_HIGH) {
			return val2;
		} else if (val2 == SIG_PULLDOWN) {
			return SIG_PULLDOWN;
		} else if (val2 == SIG_PULLUP) {
			return SIG_PULLUP;
		} else if (val2 == SIG_WEAK_PULLDOWN) {
			return SIG_WEAK_PULLDOWN;
		} else if (val2 == SIG_WEAK_PULLUP) {
			return SIG_WEAK_PULLUP;
		}
	}
	return SIG_OPEN;
}

static uint32_t sigval_tab[256];
static uint32_t sigmeassure_tab[16];

static void
init_sigval_tab(void)
{
	int i;
	for (i = 0; i < 256; i++) {
		int val1 = i & 0xf;
		int val2 = (i >> 4) & 0xf;
		sigval_tab[i] = calculate_sigval(val1, val2);
	}
	sigmeassure_tab[SIG_LOW] = SIG_LOW;
	sigmeassure_tab[SIG_HIGH] = SIG_HIGH;
	sigmeassure_tab[SIG_FORCE_LOW] = SIG_LOW;
	sigmeassure_tab[SIG_FORCE_HIGH] = SIG_HIGH;
	sigmeassure_tab[SIG_OPEN] = SIG_OPEN;	/* Should be made random */
	sigmeassure_tab[SIG_PULLDOWN] = SIG_LOW;
	sigmeassure_tab[SIG_PULLUP] = SIG_HIGH;
	sigmeassure_tab[SIG_WEAK_PULLDOWN] = SIG_LOW;
	sigmeassure_tab[SIG_WEAK_PULLUP] = SIG_HIGH;
}

static inline int
lookup_sigval(int val1, int val2)
{
	uint8_t index = (val1 & 0xf) | (val2 << 4);
	uint32_t result = sigval_tab[index];
	return result;
}

/*
 * -----------------------------------------------
 * Recursive
 * -----------------------------------------------
 */

/* avoid alloca */
static char conflict_msg[100];

static int
_SigMeassure(SigNode * sig, SigStamp stamp, int sigval)
{
	SigLink *cursor;
	sig->stamp = stamp;
	for (cursor = sig->linkList; cursor; cursor = cursor->next) {
		SigNode *partner = cursor->partner;
		if (likely(partner->stamp != stamp)) {
			int oldsigval = sigval;
			sigval = lookup_sigval(sigval, partner->selfval);
			if ((sigval == SIG_LOW) || (sigval == SIG_HIGH)) {
				return sigval;
			} else if (unlikely(sigval & SIG_ILLEGAL)) {
				snprintf(conflict_msg, sizeof(conflict_msg),
					 "************ Short circuit between %s:(%s) and %s:(%s) ",
					 SHash_GetKey(sig->hash_entry), SigVal_String(oldsigval),
					 SHash_GetKey(partner->hash_entry),
					 SigVal_String(partner->selfval));
				if (g_conflictProc) {
					g_conflictProc(conflict_msg);
				} else {
					fprintf(stderr, "%s\n", conflict_msg);
				}
				return sigval;
			}
			sigval = _SigMeassure(partner, stamp, sigval);
		}
	}
	return sigval;
}

static inline int
SigMeassure(SigNode * sig)
{
	int propval = _SigMeassure(sig, ++g_stamp, sig->selfval);
	int measval = sigmeassure_tab[propval & 0xf];
	return measval;
}

/*
 * -------------------------------------------------
 * Todo: Traces can only delete itself
 * from TraceProcs 
 * -------------------------------------------------
 */
static void
InvokeTraces(SigNode * signode)
{
	SigTrace *trace;
	SigTrace *next;
	for (trace = signode->sigTraceList; trace; trace = next) {
		next = trace->next;
		/* Avoid recursion */
		if (!trace->isactive) {
			trace->isactive++;
			trace->proc(signode, signode->propval, trace->clientData);
			if (active_trace_deleted_flag) {
				active_trace_deleted_flag = 0;
			} else {
				trace->isactive--;
			}
		} else {
			// fprintf(stderr,"Warning: Signal trace recursion\n");
		}
	}
}

/*
 * -------------------------------------------------
 * Propagate a meassured value to 
 * all connected nodes
 * -------------------------------------------------
 */
static int
SigPropagate(SigNode * sig, SigStamp stamp, int sigval)
{
	SigLink *cursor;
	int invoke_traces = 0;
	sig->stamp = stamp;
	if (sig->propval != sigval) {
		sig->propval = sigval;
		invoke_traces = 1;
	}
	for (cursor = sig->linkList; cursor; cursor = cursor->next) {
		SigNode *partner = cursor->partner;
		if (partner->stamp != stamp) {
			sigval = SigPropagate(partner, stamp, sigval);
		}
	}
	if (invoke_traces) {
		InvokeTraces(sig);
	}
	return sigval;
}

static int
update_sigval(SigNode * signode)
{
	int propval = SigMeassure(signode);
	/* Don't propagate open. If it is open keep old value */
	if (propval != SIG_OPEN) {
		propval = SigPropagate(signode, ++g_stamp, propval);
	}
	return propval;
}

/*
 * ---------------------------------------------------
 * Set a Signal 
 * ---------------------------------------------------
 */
int
SigNode_Set(SigNode * signode, int sigval)
{
	//fprintf(stderr,"Set node %s to %u\n",signode->hash_entry->key,sigval);
	if (unlikely((sigval == signode->selfval) && !(signode->propval & SIG_ILLEGAL))) {
		return signode->propval;
	}
	//fprintf(stderr,"Propagate new %d, old %d ",sigval,signode->selfval); //jk
	signode->selfval = sigval;
	update_sigval(signode);
	return signode->propval;
}

/*
 * ---------------------------------
 * Link two signals (bidirectional
 * ---------------------------------
 */
int
SigNode_Link(SigNode * sig1, SigNode * sig2)
{
	SigLink *link1, *link2;
	link1 = sg_new(SigLink);
	link2 = sg_new(SigLink);

	link1->partner = sig2;
	link1->next = sig1->linkList;
	sig1->linkList = link1;

	link2->partner = sig1;
	link2->next = sig2->linkList;
	sig2->linkList = link2;
	update_sigval(sig1);
	return 0;
}

int
SigName_Link(const char *name1, const char *name2)
{
	SigNode *sig1, *sig2;
	sig1 = SigNode_Find("%s", name1);
	sig2 = SigNode_Find("%s", name2);
	if (!sig1) {
		fprintf(stderr, "Node not found: \"%s\"\n", name1);
		sleep(2);
		return -1;
	}
	if (!sig2) {
		fprintf(stderr, "Node not found: \"%s\"\n", name2);
		sleep(2);
		return -2;
	}
	return SigNode_Link(sig1, sig2);
}

/*
 * ---------------------------------
 * Remove pair of Links 
 * ----------------------------------
 */
static int
Sig_RemoveLink(SigNode * sig, SigLink * link)
{
	SigNode *partner = link->partner;
	SigLink *cursor, *prev = NULL;
	for (cursor = sig->linkList; cursor; prev = cursor, cursor = cursor->next) {
		if (cursor == link) {
			break;
		}
	}
	if (!cursor) {
		fprintf(stderr, "Bug: Link not found\n");
		return 0;
	}
	if (prev) {
		prev->next = cursor->next;
	} else {
		sig->linkList = cursor->next;
	}
	sg_free(cursor);
	prev = NULL;
	for (cursor = partner->linkList; cursor; prev = cursor, cursor = cursor->next) {
		if (cursor->partner == sig) {
			break;
		}
	}
	if (!cursor) {
		fprintf(stderr, "Bug: Backlink not found\n");
		return -1;
	}
	if (prev) {
		prev->next = cursor->next;
	} else {
		partner->linkList = cursor->next;
	}
	update_sigval(sig);
	update_sigval(partner);
	sg_free(cursor);
	return 1;
}

int
SigName_RemoveLink(const char *name1, const char *name2)
{
	SigNode *sig1, *sig2;
	sig1 = SigNode_Find("%s", name1);
	sig2 = SigNode_Find("%s", name2);
	if (!sig1) {
		fprintf(stderr, "Node not found: \"%s\"\n", name1);
		sleep(2);
		return -1;
	}
	if (!sig2) {
		fprintf(stderr, "Node not found: \"%s\"\n", name2);
		sleep(2);
		return -2;
	}
	return SigNode_RemoveLink(sig1, sig2);
}

int
SigNode_Linked(SigNode * sig1, SigNode * sig2)
{
	SigLink *cursor;
	for (cursor = sig1->linkList; cursor; cursor = cursor->next) {
		if (cursor->partner == sig2) {
			break;
		}
	}
	if (!cursor) {
		return 0;
	} else {
		return 1;
	}
}

/*
 * --------------------------------------------------------------------
 * RemoveLink
 *
 * Return Value: 1 if a link was removed
 *		 0 if no link was removed 
 *		-1 when consistency check failed
 * --------------------------------------------------------------------
 */
int
SigNode_RemoveLink(SigNode * sig1, SigNode * sig2)
{
	SigLink *cursor, *prev = NULL;
	for (cursor = sig1->linkList; cursor; prev = cursor, cursor = cursor->next) {
		if (cursor->partner == sig2) {
			break;
		}
	}
	if (!cursor) {
		return 0;
	}
	if (prev) {
		prev->next = cursor->next;
	} else {
		sig1->linkList = cursor->next;
	}
	prev = NULL;
	for (cursor = sig2->linkList; cursor; prev = cursor, cursor = cursor->next) {
		if (cursor->partner == sig1) {
			break;
		}
	}
	if (!cursor) {
		fprintf(stderr, "Bug: Unidirectional signal link\n");
		return -1;
	}
	if (prev) {
		prev->next = cursor->next;
	} else {
		sig2->linkList = cursor->next;
	}
	update_sigval(sig1);
	update_sigval(sig2);
	sg_free(cursor);
	return 1;
}

/*
 * ---------------------------------------
 * Remove all links from a Signode
 * ---------------------------------------
 */
static void
SigNode_UnLink(SigNode * sig)
{
	while (sig->linkList) {
		Sig_RemoveLink(sig, sig->linkList);
	}
}

void
SigNode_Delete(SigNode * signode)
{

	SigNode_UnLink(signode);
	SHash_DeleteEntry(&signode_hash, signode->hash_entry);
	sg_free(signode);
}

SigTrace *
SigNode_Trace(SigNode * node, SigTraceProc * proc, void *clientData)
{
	SigTrace *trace;
	trace = sg_new(SigTrace);
	memset(trace, 0, sizeof(SigTrace));
	trace->clientData = clientData;
	trace->proc = proc;
	trace->next = node->sigTraceList;
	node->sigTraceList = trace;
	trace->isactive = 0;
	return trace;
}

int
SigNode_Untrace(SigNode * node, SigTrace * trace)
{
	SigTrace *cursor, *prev;
	for (prev = 0, cursor = node->sigTraceList; cursor; prev = cursor, cursor = cursor->next) {
		if (cursor == trace) {
			break;
		}
	}
	if (cursor) {
		if (prev) {
			prev->next = cursor->next;
		} else {
			node->sigTraceList = cursor->next;
		}
		if (cursor->isactive) {
			active_trace_deleted_flag = 1;
		}
		sg_free(cursor);
		return 0;
	} else {
		fprintf(stderr, "Bug: Deleting non existing trace\n");
		return -1;
	}
}

/**
 **************************************************************************
 * Check is a signal node is traced. Required to check if it is
 * necessary to update it.
 **************************************************************************
 */
static bool
_SigNode_IsTraced(SigNode * sig, SigStamp stamp)
{
	SigLink *cursor;
	sig->stamp = stamp;
	bool result;
	if (sig->sigTraceList) {
		return true;
	}
	for (cursor = sig->linkList; cursor; cursor = cursor->next) {
		SigNode *partner = cursor->partner;
		if (partner->stamp != stamp) {
			result = _SigNode_IsTraced(partner, stamp);
			if (result == true) {
				return result;
			}
		}
	}
	return false;
}

bool
SigNode_IsTraced(SigNode * sig)
{
	return _SigNode_IsTraced(sig, ++g_stamp);
}

/*
 * -----------------------------------------------
 * Return the dominant signal node. 
 * Mainly used for debugging purposes
 * -----------------------------------------------
 */
static SigNode *
_SigNode_FindDominant(SigNode * sig, SigStamp stamp)
{
	SigLink *cursor;
	sig->stamp = stamp;
	if ((sig->selfval == SIG_HIGH) || (sig->selfval == SIG_LOW)) {
		return sig;
	}
	for (cursor = sig->linkList; cursor; cursor = cursor->next) {
		SigNode *partner = cursor->partner;
		if (partner->stamp != stamp) {
			return _SigNode_FindDominant(partner, stamp);
		}
	}
	return NULL;
}

SigNode *
SigNode_FindDominant(SigNode * sig)
{
	return _SigNode_FindDominant(sig, ++g_stamp);
}

static void
_SigNode_Dump(SigNode * sig, SigStamp stamp)
{
	SigLink *cursor;
	sig->stamp = stamp;
	fprintf(stderr, "node %s self %d, prop %d\n", SigName(sig), sig->selfval, sig->propval);
	for (cursor = sig->linkList; cursor; cursor = cursor->next) {
		SigNode *partner = cursor->partner;
		if (partner->stamp != stamp) {
			_SigNode_Dump(partner, stamp);
		} else {
			return;
		}
	}
}

void
SigNode_Dump(SigNode * sig)
{
	_SigNode_Dump(sig, ++g_stamp);
}

void
Signodes_SetConflictProc(SigConflictProc * proc)
{
	g_conflictProc = proc;
}

/**
 *****************************************************************
 * Queury or set Elektrical signals
 *****************************************************************
 */
int
cmd_sig(Interp * interp, void *clientData, int argc, char *argv[])
{
	SigNode *sig;
	if (argc < 2) {
		return CMD_RESULT_BADARGS;
	}
	sig = SigNode_Find("%s", argv[1]);
	if (sig) {
		int val;
		if (argc == 2) {
			val = SigNode_Val(sig);
			Interp_AppendResult(interp, "%u\r\n", val);
		} else if (argc == 3) {
			val = atoi(argv[2]);
			SigNode_Set(sig, val);
		} else {
			return CMD_RESULT_BADARGS;
		}
	}
	return CMD_RESULT_OK;
}

/*
 * --------------------------------------------
 * Setup hash tables for the signal table 
 * and create GND and VCC signals
 * --------------------------------------------
 */
void
SignodesInit()
{
	SigNode *node;
	SHash_InitTable(&signode_hash);

	init_sigval_tab();
	node = SigNode_New("GND");
	if (!node) {
		fprintf(stderr, "Can not create GND\n");
		exit(43275);
	}
	SigNode_Set(node, SIG_FORCE_LOW);
	node = SigNode_New("VCC");
	if (!node) {
		fprintf(stderr, "Can not create VCC\n");
		exit(43275);
	}
	SigNode_Set(node, SIG_FORCE_HIGH);
	if (Cmd_Register("sig", cmd_sig, NULL) < 0) {
		fprintf(stderr, "Can not register sig command\n");
		exit(1);
	}
	return;
}
