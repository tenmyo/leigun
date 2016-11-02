/*
 *************************************************************************************************
 *
 * Emulation of Philips ISP1105 USB transceiver 
 *
 * state:
 *	Not implemented
 *
 * Copyright 2008 Jochen Karrer. All rights reserved.
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
#include <signode.h>
#include <string.h>
#include "isp1105.h"
#include "sgstring.h"

typedef struct ISP1105 {
	SigNode *softcon;
	SigNode *nOE;
	SigNode *speed;
	SigNode *vmo_fse0;
	SigNode *vpo_vo;
	SigNode *mode;
	SigNode *suspnd;
	SigNode *rcv;
	SigNode *vp;
	SigNode *vm;
	SigNode *dp;
	SigNode *dm;
} ISP1105;

static void
update_dp_dm(ISP1105 * isp)
{
	int mode = SigNode_Val(isp->mode);
	int vmo_fse0 = SigNode_Val(isp->vmo_fse0);
	int nOE = SigNode_Val(isp->nOE);
	if (nOE == SIG_HIGH) {
		SigNode_Set(isp->dm, SIG_OPEN);
		SigNode_Set(isp->dp, SIG_OPEN);
		return;
	}
	if (mode == SIG_LOW) {
		/* Output SE0 condition when FSE0 is high */
		if (vmo_fse0 == SIG_HIGH) {
			SigNode_Set(isp->dm, SIG_LOW);
			SigNode_Set(isp->dp, SIG_LOW);
		} else {
			int output = SigNode_Val(isp->vpo_vo);
			if (output == SIG_LOW) {
				SigNode_Set(isp->dp, SIG_LOW);
				SigNode_Set(isp->dm, SIG_HIGH);
			} else {
				SigNode_Set(isp->dp, SIG_HIGH);
				SigNode_Set(isp->dm, SIG_LOW);
			}
		}
	} else {
		SigNode_Set(isp->dp, SigNode_Val(isp->vpo_vo));
		SigNode_Set(isp->dm, SigNode_Val(isp->vmo_fse0));
	}
}

/*
 *--------------------------------------------------------------
 * Switch the pullup resistor between open and 3.3 Volt
 * Current hardware has Vpu unconnected. So do nothing here
 *--------------------------------------------------------------
 */
static void
trace_softcon(SigNode * node, int value, void *clientData)
{
	return;
}

static void
trace_nOE(SigNode * node, int value, void *clientData)
{
	ISP1105 *isp = (ISP1105 *) clientData;
	update_dp_dm(isp);
}

static void
trace_speed(SigNode * node, int value, void *clientData)
{
	return;
}

/*
 * ----------------------------------------------------------------------
 * FSE0 when mode == LOW (Single ended) 
 * FSE0 is an input pin which sets D+,D- to SE0 (both low) when high
 * VMO when mode == LOW (differential)
 * ----------------------------------------------------------------------
 */
static void
trace_vmo_fse0(SigNode * node, int value, void *clientData)
{
	ISP1105 *isp = (ISP1105 *) clientData;
	update_dp_dm(isp);
}

static void
trace_vpo_vo(SigNode * node, int value, void *clientData)
{
	ISP1105 *isp = (ISP1105 *) clientData;
	update_dp_dm(isp);
}

/*
 *******************************************************
 * Mode input Pin:
 * 	Switch between differential  and single ended mode
 * 	LOW = Single ended mode
 * 	HIGH = differential mode
 *******************************************************
 */
static void
trace_mode(SigNode * node, int value, void *clientData)
{
	ISP1105 *isp = (ISP1105 *) clientData;
	update_dp_dm(isp);
}

/*
 * Suspend pin: disables / enables the RCV pin
 */
static void
trace_suspnd(SigNode * node, int value, void *clientData)
{
	return;
}

/*
 * RCV pin is an output. So this trace is not needed
 */
static void
trace_rcv(SigNode * node, int value, void *clientData)
{
	return;
}

/*
 ****************************************************
 * VP is an output connected over an driver to D+
 ****************************************************
 */
static void
trace_vp(SigNode * node, int value, void *clientData)
{
	return;
}

/*
 ****************************************************
 * VM is an output connected over an driver to D-
 ****************************************************
 */
static void
trace_vm(SigNode * node, int value, void *clientData)
{
	return;
}

static void
trace_dp(SigNode * node, int value, void *clientData)
{
	ISP1105 *isp = (ISP1105 *) clientData;
	SigNode_Set(isp->vp, value);
}

static void
trace_dm(SigNode * node, int value, void *clientData)
{
	ISP1105 *isp = (ISP1105 *) clientData;
	SigNode_Set(isp->vm, value);
	return;
}

void
ISP1105_New(const char *name)
{
	ISP1105 *isp = sg_new(ISP1105);
	isp->softcon = SigNode_New("%s.SOFTCON", name);
	isp->nOE = SigNode_New("%s.nOE", name);
	isp->speed = SigNode_New("%s.SPEED", name);
	isp->vmo_fse0 = SigNode_New("%s.VMO_FSE0", name);
	isp->vpo_vo = SigNode_New("%s.VPO_VO", name);
	isp->mode = SigNode_New("%s.MODE", name);
	isp->suspnd = SigNode_New("%s.SUSPND", name);
	isp->rcv = SigNode_New("%s.RCV", name);
	isp->vp = SigNode_New("%s.VP", name);
	isp->vm = SigNode_New("%s.VM", name);
	isp->dp = SigNode_New("%s.DP", name);
	isp->dm = SigNode_New("%s.DM", name);
	SigNode_Trace(isp->softcon, trace_softcon, isp);
	SigNode_Trace(isp->nOE, trace_nOE, isp);
	SigNode_Trace(isp->speed, trace_speed, isp);
	SigNode_Trace(isp->vmo_fse0, trace_vmo_fse0, isp);
	SigNode_Trace(isp->vpo_vo, trace_vpo_vo, isp);
	SigNode_Trace(isp->mode, trace_mode, isp);
	SigNode_Trace(isp->suspnd, trace_suspnd, isp);
	SigNode_Trace(isp->rcv, trace_rcv, isp);
	SigNode_Trace(isp->vp, trace_vp, isp);
	SigNode_Trace(isp->vm, trace_vm, isp);
	SigNode_Trace(isp->dp, trace_dp, isp);
	SigNode_Trace(isp->dm, trace_dm, isp);
}
