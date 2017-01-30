/*
 **************************************************************************************************
 *
 * Standard  ARM Vector Floating Point unit 
 * ARM document: VFP9-S Vector Floating point coprocessor r0p2 
 *
 *  State: Nothing implemented
 *
 * Copyright 2005 Jochen Karrer. All rights reserved.
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
#include <stdint.h>
#include <string.h>
#include "coprocessor.h"
#include "arm9cpu.h"

#define FPSCR_IOC		(1<<0)
#define FPSCR_DZC		(1<<1)
#define FPSCR_OFC		(1<<2)
#define FPSCR_UFC		(1<<3)
#define FPSCR_IXC		(1<<4)
#define FPSCR_IDC		(1<<7)
#define FPSCR_IOE		(1<<8)
#define FPSCR_DZE		(1<<9)
#define FPSCR_OFE		(1<<10)
#define FPSCR_UFE		(1<<11)
#define FPSCR_IXE		(1<<12)
#define FPSCR_IDE		(1<<15)
#define FPSCR_LEN_SHIFT		(16)
#define FPSCR_LEN_MASK		(7<<16)
#define FPSCR_STRIDE_SHIFT 	(20)
#define FPSCR_STRIDE_MASK 	(3<<20)
#define FPSCR_RMODE_SHIFT	(22)
#define FPSCR_RMODE_MASK	(3<<22)
#define FPSCR_FZ		(1<<24)
#define FPSCR_DN		(1<<25)
#define FPSCR_V			(1<<28)
#define FPSCR_C			(1<<29)
#define FPSCR_Z			(1<<30)
#define FPSCR_N			(1<<31)

typedef struct ArmVfp {
	uint32_t fpsid;
	uint32_t fpcsr;
	uint32_t fpexc;
	uint32_t fpinst;
	uint32_t fpinst2;
	union {
		uint32_t s[32];
		uint64_t d[16];
	} regs;
	ArmCoprocessor copro10;
	ArmCoprocessor copro11;
} ArmVfp;

/*
 * ------------------------------------
 * The instance of the ArmVfp 
 * ------------------------------------
 */
static ArmVfp arm_vfp = {
	.copro10 = {
		    .mrc = NULL,
		    .mcr = NULL,
		    .cdp = NULL,
		    .ldc = NULL,
		    .stc = NULL,
		    },
	.copro11 = {
		    .mrc = NULL,
		    .mcr = NULL,
		    .cdp = NULL,
		    .ldc = NULL,
		    .stc = NULL}

};

void
ArmVfp_Init(char *vfpname)
{
	ArmVfp *vfp = &arm_vfp;
	ARM9_RegisterCoprocessor(&vfp->copro10, 10);
	ARM9_RegisterCoprocessor(&vfp->copro11, 11);
	vfp->fpsid = 0x410101A0;	/* VFP 9 */

	fprintf(stderr, "Registered Vector Floating Point coprocessor 10 and 11\n");
};
