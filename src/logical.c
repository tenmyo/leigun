/*
 *************************************************************************************************
 *
 * Emulation on Logical Signal Level
 *
 * Basic processing elements for digital signals: Inverter, And, Or
 *
 * Status:
 *      Not useful 
 *
 * Copyright 2007 Jochen Karrer. All rights reserved.
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
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "logical.h"
#include "signode.h"
#include "sgstring.h"
#include "cycletimer.h"

typedef struct SigNot {
	SigNode *in;
	SigNode *out;
	SigTrace *trace;
} SigNot;

typedef struct SigNand {
	SigNode *sigIn1;
	SigNode *sigIn2;
	SigNode *sigOut;
	SigTrace *trace1;
	SigTrace *trace2;
} SigNand;

typedef struct SigAnd {
	SigNode *sigIn1;
	SigNode *sigIn2;
	SigNode *sigOut;
	SigTrace *trace1;
	SigTrace *trace2;
} SigAnd;

typedef struct SigDelay {
	CycleCounter_t delayNegEdge;
	CycleCounter_t delayPosEdge;
	CycleTimer delayTimer;
	SigNode *sigIn;
	SigNode *sigOut;
} SigDelay;

static void
do_invert(SigNode * node, int value, void *clientData)
{
	SigNot *inv = (SigNot *) clientData;
	if (value == SIG_HIGH) {
		SigNode_Set(inv->out, SIG_LOW);
	} else if (value == SIG_LOW) {
		SigNode_Set(inv->out, SIG_HIGH);
	}
}

/**
 ***********************************************************************
 * Create a Pull node (Up down .. depending on argument) and link
 * it.
 ***********************************************************************
 */
SigNode *
Sig_AddPull(const char *name, int direction)
{
	SigNode *pull;
	SigNode *sig = SigNode_Find("%s", name);
	if (!sig) {
		return NULL;
	}
	switch (direction) {
	    case SIG_PULLUP:
		    pull = SigNode_New("%s.pullup", name);
		    break;
	    case SIG_PULLDOWN:
		    pull = SigNode_New("%s.pulldown", name);
		    break;
	    case SIG_WEAK_PULLDOWN:
		    pull = SigNode_New("%s.wpulldown", name);
		    break;
	    case SIG_WEAK_PULLUP:
		    pull = SigNode_New("%s.wpullup", name);
		    break;
	    case SIG_HIGH:
		    pull = SigNode_New("%s.high", name);
		    break;
	    case SIG_LOW:
		    pull = SigNode_New("%s.low", name);
		    break;
	    default:
		    pull = SigNode_New("%s.pull", name);
		    break;
	}
	if (!pull) {
		return NULL;
	}
	SigNode_Set(pull, direction);
	SigNode_Link(sig, pull);
	return pull;
}

const char *
SigNot_New(const char *outname, const char *inname)
{
	SigNot *inv;
	SigNode *out;
	SigNode *in;
	in = SigNode_Find("%s", inname);
	if (!in) {
		return NULL;
	}
	if (outname) {
		out = SigNode_New("%s", outname);
	} else {
		out = SigNode_New("n%s", inname);
	}
	if (!out) {
		fprintf(stderr, "Can not create signal %s\n", outname);
		exit(1);
	}
	inv = sg_new(SigNot);
	inv->out = out;
	inv->in = in;
	inv->trace = SigNode_Trace(in, do_invert, inv);
	// trace unsets  is missing here
	return outname;
}

static void
do_nand(SigNode * node, int value, void *clientData)
{
	SigNand *nand = (SigNand *) clientData;
	if ((SigNode_Val(nand->sigIn1) == SIG_HIGH) && (SigNode_Val(nand->sigIn2) == SIG_HIGH)) {
		SigNode_Set(nand->sigOut, SIG_LOW);
	} else {
		SigNode_Set(nand->sigOut, SIG_HIGH);
	}
}

/**
 ************************************************************************
 * \fn void SigNand_New(const char *out,const char *in1, const char *in2) 
 ************************************************************************
 */
void
SigNand_New(const char *out, const char *in1, const char *in2)
{
	SigNand *nand = sg_new(SigNand);
	nand->sigIn1 = SigNode_Find("%s", in1);
	nand->sigIn2 = SigNode_Find("%s", in2);
	nand->sigOut = SigNode_New("%s", out);
	if (!nand->sigIn1 || !nand->sigIn2 || !nand->sigOut) {
		fprintf(stderr, "Can not create signals for NAND gate\n");
		exit(1);
	}
	nand->trace1 = SigNode_Trace(nand->sigIn1, do_nand, nand);
	nand->trace2 = SigNode_Trace(nand->sigIn2, do_nand, nand);
}

static void
do_and(SigNode * node, int value, void *clientData)
{
	SigNand *nand = (SigNand *) clientData;
	if ((SigNode_Val(nand->sigIn1) == SIG_HIGH) && (SigNode_Val(nand->sigIn2) == SIG_HIGH)) {
		SigNode_Set(nand->sigOut, SIG_HIGH);
	} else {
		SigNode_Set(nand->sigOut, SIG_LOW);
	}
}

void
SigAnd_New(const char *out, const char *in1, const char *in2)
{
	SigAnd *and = sg_new(SigAnd);
	and->sigIn1 = SigNode_Find("%s", in1);
	and->sigIn2 = SigNode_Find("%s", in2);
	and->sigOut = SigNode_New("%s", out);
	if (!and->sigIn1) {
		fprintf(stderr, "Can not find signal %s for AND gate\n", in1);
		exit(1);
	}
	if (!and->sigIn2) {
		fprintf(stderr, "Can not find signal %s for AND gate\n", in2);
		exit(1);
	}
	if (!and->sigOut) {
		fprintf(stderr, "Can not create signal %s for AND gate\n", out);
		exit(1);
	}
	and->trace1 = SigNode_Trace(and->sigIn1, do_and, and);
	and->trace2 = SigNode_Trace(and->sigIn2, do_and, and);
}

static void
do_delay(SigNode * node, int value, void *clientData)
{
	SigDelay *del = clientData;
	CycleCounter_t delay;
	int64_t remaining;
	remaining = CycleTimer_GetRemaining(&del->delayTimer);
	if (value == SIG_LOW) {
		if (del->delayPosEdge) {
			delay = del->delayNegEdge -
			    del->delayNegEdge * remaining / del->delayPosEdge;
		} else {
			delay = del->delayNegEdge;
		}
		//fprintf(stderr,"delay %lld\n",delay);
		CycleTimer_Mod(&del->delayTimer, delay);
	} else if (value == SIG_HIGH) {
		if (del->delayNegEdge) {
			delay = del->delayPosEdge -
			    del->delayPosEdge * remaining / del->delayNegEdge;
		} else {
			delay = del->delayPosEdge;
		}
		CycleTimer_Mod(&del->delayTimer, delay);
		//fprintf(stderr,"delay %lld\n",delay);
	}
}

static void
set_delayed(void *eventData)
{
	SigDelay *del = eventData;
	SigNode_Set(del->sigOut, SigNode_Val(del->sigIn));
}

SigNode *
SigDelay_New(const char *out, const char *in, CycleCounter_t negDel, CycleCounter_t posDel)
{
	SigDelay *del = sg_new(SigDelay);
	del->sigIn = SigNode_Find("%s", in);
	del->sigOut = SigNode_New("%s", out);
	if (!del->sigIn || !del->sigOut) {
		fprintf(stderr, "Can not create delay element\n");
		exit(1);
	}
	del->delayNegEdge = negDel;
	del->delayPosEdge = posDel;
	SigNode_Trace(del->sigIn, do_delay, del);
	SigNode_Set(del->sigOut, SigNode_Val(del->sigIn));
	CycleTimer_Init(&del->delayTimer, set_delayed, del);
	return del->sigOut;
}
