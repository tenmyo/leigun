/*
 **************************************************************************************************
 *
 * ARM9 Instruction decoder  
 *
 * Status: working
 *
 * Copyright 2004 Jochen Karrer. All rights reserved.
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "arm9cpu.h"
#include "idecode_arm.h"
#include "instructions_arm.h"
#include "sgstring.h"

#define MAX_INSTRUCTIONS (200000)

typedef struct IDecoder {
        Instruction *instr[0x10000];
} IDecoder;

static Instruction *imem;
InstructionProc **iProcTab;
static IDecoder *idecoder;

static int alloc_pointer=0;
Instruction *alloc_instruction() {
	if(alloc_pointer<MAX_INSTRUCTIONS) {
		return imem+alloc_pointer++;
	} else{
		return sg_new(Instruction);
	}
}

static Instruction instrlist[] = { 
{
	.mask =  0x0fe00090,
	.icode = 0x00a00000,
	.name = "adc",
	.proc = armv5_adc,
	.arch = ARM_ARCH_V5,
}, 
{
	.mask =  0x0fe00090,
	.icode = 0x00a00010,
	.name = "adc",
	.proc = armv5_adc,
	.arch = ARM_ARCH_V5,
}, 
{
	.mask =  0x0fe00090,
	.icode = 0x00a00080,
	.name = "adc",
	.proc = armv5_adc,
	.arch = ARM_ARCH_V5,
}, 
{
	.mask =  0x0fe00090,
	.icode = 0x02a00000,
	.name = "adc",
	.proc = armv5_adc,
	.arch = ARM_ARCH_V5,
}, 
{
	.mask =  0x0fe00090,
	.icode = 0x02a00010,
	.name = "adc",
	.proc = armv5_adc,
	.arch = ARM_ARCH_V5,
}, 
{
	.mask =  0x0fe00090,
	.icode = 0x02a00080,
	.name = "adc",
	.proc = armv5_adc,
	.arch = ARM_ARCH_V5,
}, 
{
	.mask =  0x0fe00090,
	.icode = 0x02a00090,
	.name = "adc",
	.proc = armv5_adc,
	.arch = ARM_ARCH_V5,
}, 
{
	.mask =  0x0fe00090,
	.icode = 0x00800000,
	.name = "add",
	.proc = armv5_add,
	.arch = ARM_ARCH_V5,
},
{
	.mask =  0x0fe00090,
	.icode = 0x00800010,
	.name = "add",
	.proc = armv5_add,
	.arch = ARM_ARCH_V5,
},
{
	.mask =  0x0fe00090,
	.icode = 0x00800080,
	.name = "add",
	.proc = armv5_add,
	.arch = ARM_ARCH_V5,
},
{
	.mask =  0x0fe00090,
	.icode = 0x02800000,
	.name = "add",
	.proc = armv5_add,
	.arch = ARM_ARCH_V5,
},
{
	.mask =  0x0fe00090,
	.icode = 0x02800010,
	.name = "add",
	.proc = armv5_add,
	.arch = ARM_ARCH_V5,
},
{
	.mask =  0x0fe00090,
	.icode = 0x02800080,
	.name = "add",
	.proc = armv5_add,
	.arch = ARM_ARCH_V5,
},
{
	.mask =  0x0fe00090,
	.icode = 0x02800090,
	.name = "add",
	.proc = armv5_add,
	.arch = ARM_ARCH_V5,
},
{
	.mask =  0x0fe00090,
	.icode = 0x00000000,
	.name = "and",
	.proc = armv5_and,
	.arch = ARM_ARCH_V5,
},
{
	.mask =  0x0fe00090,
	.icode = 0x00000010,
	.name = "and",
	.proc = armv5_and,
	.arch = ARM_ARCH_V5,
},
{
	.mask =  0x0fe00090,
	.icode = 0x00000080,
	.name = "and",
	.proc = armv5_and,
	.arch = ARM_ARCH_V5,
},
{
	.mask =  0x0fe00090,
	.icode = 0x02000000,
	.name = "and",
	.proc = armv5_and,
	.arch = ARM_ARCH_V5,
},
{
	.mask =  0x0fe00090,
	.icode = 0x02000010,
	.name = "and",
	.proc = armv5_and,
	.arch = ARM_ARCH_V5,
},
{
	.mask =  0x0fe00090,
	.icode = 0x02000080,
	.name = "and",
	.proc = armv5_and,
	.arch = ARM_ARCH_V5,
},
{
	.mask =  0x0fe00090,
	.icode = 0x02000090,
	.name = "and",
	.proc = armv5_and,
	.arch = ARM_ARCH_V5,
},
//{0x0f000000,0x0a000000,"b"      ,armv5_b},
//{0x0f000000,0x0b000000,"bl"     ,armv5_bl},
{
	.mask = 0xfe000000,
	.icode = 0x0a000000,
	.name = "bbl",
	.proc = armv5_bbl,
	.arch = ARM_ARCH_V5,
},
{
	.mask = 0xfe000000,
	.icode = 0x1a000000,
	.name = "bbl",
	.proc = armv5_bbl,
	.arch = ARM_ARCH_V5,
},
{
	.mask = 0xfe000000,
	.icode = 0x2a000000,
	.name = "bbl",
	.proc = armv5_bbl,
	.arch = ARM_ARCH_V5,
},
{
	.mask = 0xfe000000,
	.icode = 0x3a000000,
	.name = "bbl",
	.proc = armv5_bbl,
	.arch = ARM_ARCH_V5,
},
{
	.mask = 0xfe000000,
	.icode = 0x4a000000,
	.name = "bbl",
	.proc = armv5_bbl,
	.arch = ARM_ARCH_V5,
},
{
	.mask = 0xfe000000,
	.icode = 0x5a000000,
	.name = "bbl",
	.proc = armv5_bbl,
	.arch = ARM_ARCH_V5,
},
{
	.mask = 0xfe000000,
	.icode = 0x6a000000,
	.name = "bbl",
	.proc = armv5_bbl,
	.arch = ARM_ARCH_V5,
},
{
	.mask = 0xfe000000,
	.icode = 0x7a000000,
	.name = "bbl",
	.proc = armv5_bbl,
	.arch = ARM_ARCH_V5,
},
{
	.mask = 0xfe000000,
	.icode = 0x8a000000,
	.name = "bbl",
	.proc = armv5_bbl,
	.arch = ARM_ARCH_V5,
},
{
	.mask = 0xfe000000,
	.icode = 0x9a000000,
	.name = "bbl",
	.proc = armv5_bbl,
	.arch = ARM_ARCH_V5,
},
{
	.mask = 0xfe000000,
	.icode = 0xaa000000,
	.name = "bbl",
	.proc = armv5_bbl,
	.arch = ARM_ARCH_V5,
},
{
	.mask = 0xfe000000,
	.icode = 0xba000000,
	.name = "bbl",
	.proc = armv5_bbl,
	.arch = ARM_ARCH_V5,
},
{
	.mask = 0xfe000000,
	.icode = 0xca000000,
	.name = "bbl",
	.proc = armv5_bbl,
	.arch = ARM_ARCH_V5,
},
{
	.mask = 0xfe000000,
	.icode = 0xda000000,
	.name = "bbl",
	.proc = armv5_bbl,
	.arch = ARM_ARCH_V5,
},
{
	.mask = 0xfe000000,
	.icode = 0xea000000,
	.name = "bbl",
	.proc = armv5_bbl,
	.arch = ARM_ARCH_V5,
},
{
	.mask =  0x0fe00090,
	.icode = 0x01c00000,
	.name = "bic"    ,
	.proc = armv5_bic,
	.arch = ARM_ARCH_V5,
}, 
{
	.mask =  0x0fe00090,
	.icode = 0x01c00010,
	.name = "bic"    ,
	.proc = armv5_bic,
	.arch = ARM_ARCH_V5,
}, 
{
	.mask =  0x0fe00090,
	.icode = 0x01c00080,
	.name = "bic"    ,
	.proc = armv5_bic,
	.arch = ARM_ARCH_V5,
}, 
{
	.mask =  0x0fe00090,
	.icode = 0x03c00000,
	.name = "bic"    ,
	.proc = armv5_bic,
	.arch = ARM_ARCH_V5,
}, 
{
	.mask =  0x0fe00090,
	.icode = 0x03c00010,
	.name = "bic"    ,
	.proc = armv5_bic,
	.arch = ARM_ARCH_V5,
}, 
{
	.mask =  0x0fe00090,
	.icode = 0x03c00080,
	.name = "bic"    ,
	.proc = armv5_bic,
	.arch = ARM_ARCH_V5,
}, 
{
	.mask =  0x0fe00090,
	.icode = 0x03c00090,
	.name = "bic"    ,
	.proc = armv5_bic,
	.arch = ARM_ARCH_V5,
}, 
{
	.mask = 0xfff000f0,
	.icode = 0xe1200070,
	.name = "bkpt",
	.proc = armv5_bkpt,
	.arch = ARM_ARCH_V5,
},
{
	.mask = 0xfe000000,
	.icode = 0xfa000000,
	.name = "blx1",
	.proc = armv5_blx1,
	.arch = ARM_ARCH_V5,
},
{
	.mask = 0x0ff000f0,
	.icode = 0x01200030,
	.name = "blx2",
	.proc = armv5_blx2bx,
	.arch = ARM_ARCH_V5,
},
{
	.mask =  0x0ff000f0,
	.icode = 0x01200010,
	.name = "bx",
	.proc = armv5_blx2bx,
	.arch = ARM_ARCH_V5,
},
{
	.mask =  0x0ff000f0,
	.icode = 0x01200020,
	.name = "bxj",
	.proc = armv5_bxj,
	.arch = ARM_ARCH_V5,
},
{
	.mask = 0x0f000010,
	.icode = 0x0e000000,
	.name = "cdp",
	.proc = armv5_cdp,
	.arch = ARM_ARCH_V5,
},
{
	.mask = 0x0ff000f0,
	.icode = 0x01600010,
	.name = "clz",
	.proc = armv5_clz,
	.arch = ARM_ARCH_V5,
},
{
	.mask =  0x0ff00090,
	.icode = 0x01700000,
	.name = "cmn",
	.proc = armv5_cmn,
	.arch = ARM_ARCH_V5,
},
{
	.mask =  0x0ff00090,
	.icode = 0x01700010,
	.name = "cmn",
	.proc = armv5_cmn,
	.arch = ARM_ARCH_V5,
},
{
	.mask =  0x0ff00090,
	.icode = 0x01700080,
	.name = "cmn",
	.proc = armv5_cmn,
	.arch = ARM_ARCH_V5,
},
{
	.mask =  0x0ff00090,
	.icode = 0x03700000,
	.name = "cmn",
	.proc = armv5_cmn,
	.arch = ARM_ARCH_V5,
},
{
	.mask =  0x0ff00090,
	.icode = 0x03700010,
	.name = "cmn",
	.proc = armv5_cmn,
	.arch = ARM_ARCH_V5,
},
{
	.mask =  0x0ff00090,
	.icode = 0x03700080,
	.name = "cmn",
	.proc = armv5_cmn,
	.arch = ARM_ARCH_V5,
},
{
	.mask =  0x0ff00090,
	.icode = 0x03700090,
	.name = "cmn",
	.proc = armv5_cmn,
	.arch = ARM_ARCH_V5,
},
{
	.mask =  0x0ff00090,
	.icode = 0x01500000,
	.name = "cmp",
	.proc = armv5_cmp,
	.arch = ARM_ARCH_V5,
},
{
	.mask =  0x0ff00090,
	.icode = 0x01500010,
	.name = "cmp",
	.proc = armv5_cmp,
	.arch = ARM_ARCH_V5,
},
{
	.mask =  0x0ff00090,
	.icode = 0x01500080,
	.name = "cmp",
	.proc = armv5_cmp,
	.arch = ARM_ARCH_V5,
},
{
	.mask =  0x0ff00090,
	.icode = 0x03500000,
	.name = "cmp",
	.proc = armv5_cmp,
	.arch = ARM_ARCH_V5,
},
{
	.mask =  0x0ff00090,
	.icode = 0x03500010,
	.name = "cmp",
	.proc = armv5_cmp,
	.arch = ARM_ARCH_V5,
},
{
	.mask =  0x0ff00090,
	.icode = 0x03500080,
	.name = "cmp",
	.proc = armv5_cmp,
	.arch = ARM_ARCH_V5,
},
{
	.mask =  0x0ff00090,
	.icode = 0x03500090,
	.name = "cmp",
	.proc = armv5_cmp,
	.arch = ARM_ARCH_V5,
},
{
	.mask =  0x0fe00090,
	.icode = 0x00200000,
	.name = "eor",
	.proc = armv5_eor,
	.arch = ARM_ARCH_V5,
},
{
	.mask =  0x0fe00090,
	.icode = 0x00200010,
	.name = "eor",
	.proc = armv5_eor,
	.arch = ARM_ARCH_V5,
},
{
	.mask =  0x0fe00090,
	.icode = 0x00200080,
	.name = "eor",
	.proc = armv5_eor,
	.arch = ARM_ARCH_V5,
},
{
	.mask =  0x0fe00090,
	.icode = 0x02200000,
	.name = "eor",
	.proc = armv5_eor,
	.arch = ARM_ARCH_V5,
},
{
	.mask =  0x0fe00090,
	.icode = 0x02200010,
	.name = "eor",
	.proc = armv5_eor,
	.arch = ARM_ARCH_V5,
},
{
	.mask =  0x0fe00090,
	.icode = 0x02200080,
	.name = "eor",
	.proc = armv5_eor,
	.arch = ARM_ARCH_V5,
},
{
	.mask =  0x0fe00090,
	.icode = 0x02200090,
	.name = "eor",
	.proc = armv5_eor,
	.arch = ARM_ARCH_V5,
},
{
	.mask = 0x0ff00000,
	.icode = 0x0c100000,
	.name = "ldc",
	.proc = armv5_ldc,
	.arch = ARM_ARCH_V5,
},
{
	.mask = 0x0ff00000,
	.icode = 0x0c300000,
	.name = "ldc",
	.proc = armv5_ldc,
	.arch = ARM_ARCH_V5,
},
{
	.mask = 0x0ff00000,
	.icode = 0x0c700000,
	.name = "ldc",
	.proc = armv5_ldc,
	.arch = ARM_ARCH_V5,
},
{
	.mask = 0x0ff00000,
	.icode = 0x0c900000,
	.name = "ldc",
	.proc = armv5_ldc,
	.arch = ARM_ARCH_V5,
},
{
	.mask = 0x0ff00000,
	.icode = 0x0cb00000,
	.name = "ldc",
	.proc = armv5_ldc,
	.arch = ARM_ARCH_V5,
},
{
	.mask = 0x0ff00000,
	.icode = 0x0cd00000,
	.name = "ldc",
	.proc = armv5_ldc,
	.arch = ARM_ARCH_V5,
},
{
	.mask = 0x0ff00000,
	.icode = 0x0cf00000,
	.name = "ldc",
	.proc = armv5_ldc,
	.arch = ARM_ARCH_V5,
},
{
	.mask = 0x0ff00000,
	.icode = 0x0d100000,
	.name = "ldc",
	.proc = armv5_ldc,
	.arch = ARM_ARCH_V5,
},
{
	.mask = 0x0ff00000,
	.icode = 0x0d300000,
	.name = "ldc",
	.proc = armv5_ldc,
	.arch = ARM_ARCH_V5,
},
{
	.mask = 0x0ff00000,
	.icode = 0x0d500000,
	.name = "ldc",
	.proc = armv5_ldc,
	.arch = ARM_ARCH_V5,
},
{
	.mask = 0x0ff00000,
	.icode = 0x0d700000,
	.name = "ldc",
	.proc = armv5_ldc,
	.arch = ARM_ARCH_V5,
},
{
	.mask = 0x0ff00000,
	.icode = 0x0d900000,
	.name = "ldc",
	.proc = armv5_ldc,
	.arch = ARM_ARCH_V5,
},
{
	.mask = 0x0ff00000,
	.icode = 0x0db00000,
	.name = "ldc",
	.proc = armv5_ldc,
	.arch = ARM_ARCH_V5,
},
{
	.mask = 0x0ff00000,
	.icode = 0x0dd00000,
	.name = "ldc",
	.proc = armv5_ldc,
	.arch = ARM_ARCH_V5,
},
{
	.mask = 0x0ff00000,
	.icode = 0x0df00000,
	.name = "ldc",
	.proc = armv5_ldc,
	.arch = ARM_ARCH_V5,
},
{
	.mask = 0x0e500000,
	.icode = 0x08100000,
	.name = "ldm1",
	.proc = armv5_lsm,
	.arch = ARM_ARCH_V5,
},
{
	.mask =  0x0e500000,
	.icode = 0x08500000,
	.name = "ldm2/3",
	.proc = armv5_lsm,
	.arch = ARM_ARCH_V5,
},
{
	.mask = 0x0d700000,
	.icode = 0x04100000,
	.name = "ldr",
	.proc = armv5_ldr,
	.arch = ARM_ARCH_V5,
},
{
	.mask = 0x0d700000,
	.icode = 0x05100000,
	.name = "ldr",
	.proc = armv5_ldr,
	.arch = ARM_ARCH_V5,
},
{
	.mask = 0x0d700000,
	.icode = 0x05300000,
	.name = "ldr",
	.proc = armv5_ldr,
	.arch = ARM_ARCH_V5,
},
{
	.mask = 0x0d700000,
	.icode = 0x04500000,
	.name = "ldrb",
	.proc = armv5_ldrb,
	.arch = ARM_ARCH_V5,
},
{
	.mask = 0x0d700000,
	.icode = 0x05500000,
	.name = "ldrb",
	.proc = armv5_ldrb,
	.arch = ARM_ARCH_V5,
},
{
	.mask = 0x0d700000,
	.icode = 0x05700000,
	.name = "ldrb",
	.proc = armv5_ldrb,
	.arch = ARM_ARCH_V5,
},
{
	.mask = 0x0d700000,
	.icode = 0x04700000,
	.name = "ldrbt",
	.proc = armv5_ldrbt,
	.arch = ARM_ARCH_V5,
},
{
	.mask = 0x0e1000f0,
	.icode = 0x001000b0,
	.name = "ldrh",
	.proc = armv5_ldrh,
	.arch = ARM_ARCH_V5,
},
{
	.mask =  0x0e1000f0,
	.icode = 0x001000d0,
	.name = "ldrsb",
	.proc = armv5_ldrsb,
	.arch = ARM_ARCH_V5,
},
{
	.mask = 0x0e1000f0,
	.icode = 0x001000f0,
	.name = "ldrsh",
	.proc = armv5_ldrsh,
	.arch = ARM_ARCH_V5,
}, 
{
	.mask = 0x0d700000,
	.icode = 0x04300000,
	.name = "ldrt",
	.proc = armv5_ldrt,
	.arch = ARM_ARCH_V5,
},
{
	.mask = 0x0f100010,
	.icode = 0x0e000010,
	.name = "mcr",
	.proc = armv5_mcr,
	.arch = ARM_ARCH_V5,
},
{
	.mask = 0x0fe000f0,
	.icode = 0x00200090,
	.name = "mla",
	.proc = armv5_mla,
	.arch = ARM_ARCH_V5,
},
{
	.mask =  0x0fe00090,
	.icode = 0x01a00000,
	.name = "mov",
	.proc = armv5_mov,
	.arch = ARM_ARCH_V5,
}, 
{
	.mask =  0x0fe00090,
	.icode = 0x01a00010,
	.name = "mov",
	.proc = armv5_mov,
	.arch = ARM_ARCH_V5,
}, 
{
	.mask =  0x0fe00090,
	.icode = 0x01a00080,
	.name = "mov",
	.proc = armv5_mov,
	.arch = ARM_ARCH_V5,
}, 
{
	.mask =  0x0fe00090,
	.icode = 0x03a00000,
	.name = "mov",
	.proc = armv5_mov,
	.arch = ARM_ARCH_V5,
}, 
{
	.mask =  0x0fe00090,
	.icode = 0x03a00010,
	.name = "mov",
	.proc = armv5_mov,
	.arch = ARM_ARCH_V5,
}, 
{
	.mask =  0x0fe00090,
	.icode = 0x03a00080,
	.name = "mov",
	.proc = armv5_mov,
	.arch = ARM_ARCH_V5,
}, 
{
	.mask =  0x0fe00090,
	.icode = 0x03a00090,
	.name = "mov",
	.proc = armv5_mov,
	.arch = ARM_ARCH_V5,
}, 
{
	.mask =  0xff100010,
	.icode = 0x0e100010,
	.name = "mrc",
	.proc = armv5_mrc,
	.arch = ARM_ARCH_V5,
},
{
	.mask =  0xff100010,
	.icode = 0x1e100010,
	.name = "mrc",
	.proc = armv5_mrc,
	.arch = ARM_ARCH_V5,
},
{
	.mask =  0xff100010,
	.icode = 0x2e100010,
	.name = "mrc",
	.proc = armv5_mrc,
	.arch = ARM_ARCH_V5,
},
{
	.mask =  0xff100010,
	.icode = 0x3e100010,
	.name = "mrc",
	.proc = armv5_mrc,
	.arch = ARM_ARCH_V5,
},
{
	.mask =  0xff100010,
	.icode = 0x4e100010,
	.name = "mrc",
	.proc = armv5_mrc,
	.arch = ARM_ARCH_V5,
},
{
	.mask =  0xff100010,
	.icode = 0x5e100010,
	.name = "mrc",
	.proc = armv5_mrc,
	.arch = ARM_ARCH_V5,
},
{
	.mask =  0xff100010,
	.icode = 0x6e100010,
	.name = "mrc",
	.proc = armv5_mrc,
	.arch = ARM_ARCH_V5,
},
{
	.mask =  0xff100010,
	.icode = 0x7e100010,
	.name = "mrc",
	.proc = armv5_mrc,
	.arch = ARM_ARCH_V5,
},
{
	.mask =  0xff100010,
	.icode = 0x8e100010,
	.name = "mrc",
	.proc = armv5_mrc,
	.arch = ARM_ARCH_V5,
},
{
	.mask =  0xff100010,
	.icode = 0x9e100010,
	.name = "mrc",
	.proc = armv5_mrc,
	.arch = ARM_ARCH_V5,
},
{
	.mask =  0xff100010,
	.icode = 0xae100010,
	.name = "mrc",
	.proc = armv5_mrc,
	.arch = ARM_ARCH_V5,
},
{
	.mask =  0xff100010,
	.icode = 0xbe100010,
	.name = "mrc",
	.proc = armv5_mrc,
	.arch = ARM_ARCH_V5,
},
{
	.mask =  0xff100010,
	.icode = 0xce100010,
	.name = "mrc",
	.proc = armv5_mrc,
	.arch = ARM_ARCH_V5,
},
{
	.mask =  0xff100010,
	.icode = 0xde100010,
	.name = "mrc",
	.proc = armv5_mrc,
	.arch = ARM_ARCH_V5,
},
{
	.mask =  0xff100010,
	.icode = 0xee100010,
	.name = "mrc",
	.proc = armv5_mrc,
	.arch = ARM_ARCH_V5,
},
{
	.mask =  0xff100010,
	.icode = 0xfe100010,
	.name = "mrc2",
	.proc = armv5_mrc2,
	.arch = ARM_ARCH_V5,
},
#if 0
shit
{
	.mask =  0x0fbf0000,
	.icode = 0x010f0000,
	.name = "mrs",
	.proc = armv5_mrs
},
#endif
{
	.mask =  0x0fb000f0,
	.icode = 0x01000000,
	.name = "mrs",
	.proc = armv5_mrs,
	.arch = ARM_ARCH_V5,
},
{
	.mask = 0x0fb00000,
	.icode = 0x03200000,
	.name = "msri",
	.proc = armv5_msri,
	.arch = ARM_ARCH_V5,
},
{
	.mask = 0x0fb000f0,
	.icode = 0x01200000,
	.name = "msrr",
	.proc = armv5_msrr,
	.arch = ARM_ARCH_V5,
},
{
	.mask = 0x0fe000f0,
	.icode = 0x00000090,
	.name = "mul",
	.proc = armv5_mul,
	.arch = ARM_ARCH_V5,
}, 
{
	.mask =  0x0fe00090,
	.icode = 0x01e00000,
	.name = "mvn",
	.proc = armv5_mvn,
	.arch = ARM_ARCH_V5,
},
{
	.mask =  0x0fe00090,
	.icode = 0x01e00010,
	.name = "mvn",
	.proc = armv5_mvn,
	.arch = ARM_ARCH_V5,
},
{
	.mask =  0x0fe00090,
	.icode = 0x01e00080,
	.name = "mvn",
	.proc = armv5_mvn,
	.arch = ARM_ARCH_V5,
},
{
	.mask =  0x0fe00090,
	.icode = 0x03e00000,
	.name = "mvn",
	.proc = armv5_mvn,
	.arch = ARM_ARCH_V5,
},
{
	.mask =  0x0fe00090,
	.icode = 0x03e00010,
	.name = "mvn",
	.proc = armv5_mvn,
	.arch = ARM_ARCH_V5,
},
{
	.mask =  0x0fe00090,
	.icode = 0x03e00080,
	.name = "mvn",
	.proc = armv5_mvn,
	.arch = ARM_ARCH_V5,
},
{
	.mask =  0x0fe00090,
	.icode = 0x03e00090,
	.name = "mvn",
	.proc = armv5_mvn,
	.arch = ARM_ARCH_V5,
},
{
	.mask =  0x0fe00090,
	.icode = 0x01800000,
	.name = "orr",
	.proc = armv5_orr,
	.arch = ARM_ARCH_V5,
},
{
	.mask =  0x0fe00090,
	.icode = 0x01800010,
	.name = "orr",
	.proc = armv5_orr,
	.arch = ARM_ARCH_V5,
},
{
	.mask =  0x0fe00090,
	.icode = 0x01800080,
	.name = "orr",
	.proc = armv5_orr,
	.arch = ARM_ARCH_V5,
},
{
	.mask =  0x0fe00090,
	.icode = 0x03800000,
	.name = "orr",
	.proc = armv5_orr,
	.arch = ARM_ARCH_V5,
},
{
	.mask =  0x0fe00090,
	.icode = 0x03800010,
	.name = "orr",
	.proc = armv5_orr,
	.arch = ARM_ARCH_V5,
},
{
	.mask =  0x0fe00090,
	.icode = 0x03800080,
	.name = "orr",
	.proc = armv5_orr,
	.arch = ARM_ARCH_V5,
},
{
	.mask =  0x0fe00090,
	.icode = 0x03800090,
	.name = "orr",
	.proc = armv5_orr,
	.arch = ARM_ARCH_V5,
},
{
	.mask =  0x0fe00090,
	.icode = 0x00600000,
	.name = "rsb",
	.proc = armv5_rsb,
	.arch = ARM_ARCH_V5,
},
{
	.mask =  0x0fe00090,
	.icode = 0x00600010,
	.name = "rsb",
	.proc = armv5_rsb,
	.arch = ARM_ARCH_V5,
},
{
	.mask =  0x0fe00090,
	.icode = 0x00600080,
	.name = "rsb",
	.proc = armv5_rsb,
	.arch = ARM_ARCH_V5,
},
{
	.mask =  0x0fe00090,
	.icode = 0x02600000,
	.name = "rsb",
	.proc = armv5_rsb,
	.arch = ARM_ARCH_V5,
},
{
	.mask =  0x0fe00090,
	.icode = 0x02600010,
	.name = "rsb",
	.proc = armv5_rsb,
	.arch = ARM_ARCH_V5,
},
{
	.mask =  0x0fe00090,
	.icode = 0x02600080,
	.name = "rsb",
	.proc = armv5_rsb,
	.arch = ARM_ARCH_V5,
},
{
	.mask =  0x0fe00090,
	.icode = 0x02600090,
	.name = "rsb",
	.proc = armv5_rsb,
	.arch = ARM_ARCH_V5,
},
{
	.mask =  0x0fe00090,
	.icode = 0x00e00000,
	.name = "rsc",
	.proc = armv5_rsc,
	.arch = ARM_ARCH_V5,
},
{
	.mask =  0x0fe00090,
	.icode = 0x00e00010,
	.name = "rsc",
	.proc = armv5_rsc,
	.arch = ARM_ARCH_V5,
},
{
	.mask =  0x0fe00090,
	.icode = 0x00e00080,
	.name = "rsc",
	.proc = armv5_rsc,
	.arch = ARM_ARCH_V5,
},
{
	.mask =  0x0fe00090,
	.icode = 0x02e00000,
	.name = "rsc",
	.proc = armv5_rsc,
	.arch = ARM_ARCH_V5,
},
{
	.mask =  0x0fe00090,
	.icode = 0x02e00010,
	.name = "rsc",
	.proc = armv5_rsc,
	.arch = ARM_ARCH_V5,
},
{
	.mask =  0x0fe00090,
	.icode = 0x02e00080,
	.name = "rsc",
	.proc = armv5_rsc,
	.arch = ARM_ARCH_V5,
},
{
	.mask =  0x0fe00090,
	.icode = 0x02e00090,
	.name = "rsc",
	.proc = armv5_rsc,
	.arch = ARM_ARCH_V5,
},
{
	.mask =  0x0fe00090,
	.icode = 0x00c00000,
	.name = "sbc",
	.proc = armv5_sbc,
	.arch = ARM_ARCH_V5,
},
{
	.mask =  0x0fe00090,
	.icode = 0x00c00010,
	.name = "sbc",
	.proc = armv5_sbc,
	.arch = ARM_ARCH_V5,
},
{
	.mask =  0x0fe00090,
	.icode = 0x00c00080,
	.name = "sbc",
	.proc = armv5_sbc,
	.arch = ARM_ARCH_V5,
},
{
	.mask =  0x0fe00090,
	.icode = 0x02c00000,
	.name = "sbc",
	.proc = armv5_sbc,
	.arch = ARM_ARCH_V5,
},
{
	.mask =  0x0fe00090,
	.icode = 0x02c00010,
	.name = "sbc",
	.proc = armv5_sbc,
	.arch = ARM_ARCH_V5,
},
{
	.mask =  0x0fe00090,
	.icode = 0x02c00080,
	.name = "sbc",
	.proc = armv5_sbc,
	.arch = ARM_ARCH_V5,
},
{
	.mask =  0x0fe00090,
	.icode = 0x02c00090,
	.name = "sbc",
	.proc = armv5_sbc,
	.arch = ARM_ARCH_V5,
},
{
	.mask = 0x0fe000f0,
	.icode = 0x00e00090,
	.name = "smlal",
	.proc = armv5_smlal,
	.arch = ARM_ARCH_V5,
},
{
	.mask =  0x0fe000f0,
	.icode = 0x00c00090,
	.name = "smull",
	.proc = armv5_smull,
	.arch = ARM_ARCH_V5,
},
{
	.mask =  0x0ff00000,
	.icode = 0x0c000000,
	.name = "stc",
	.proc = armv5_stc,
	.arch = ARM_ARCH_V5,
},
{
	.mask =  0x0ff00000,
	.icode = 0x0c200000,
	.name = "stc",
	.proc = armv5_stc,
	.arch = ARM_ARCH_V5,
},
{
	.mask =  0x0ff00000,
	.icode = 0x0c600000,
	.name = "stc",
	.proc = armv5_stc,
	.arch = ARM_ARCH_V5,
},
{
	.mask =  0x0ff00000,
	.icode = 0x0c800000,
	.name = "stc",
	.proc = armv5_stc,
	.arch = ARM_ARCH_V5,
},
{
	.mask =  0x0ff00000,
	.icode = 0x0ca00000,
	.name = "stc",
	.proc = armv5_stc,
	.arch = ARM_ARCH_V5,
},
{
	.mask =  0x0ff00000,
	.icode = 0x0cc00000,
	.name = "stc",
	.proc = armv5_stc,
	.arch = ARM_ARCH_V5,
},
{
	.mask =  0x0ff00000,
	.icode = 0x0ce00000,
	.name = "stc",
	.proc = armv5_stc,
	.arch = ARM_ARCH_V5,
},
{
	.mask =  0x0ff00000,
	.icode = 0x0d000000,
	.name = "stc",
	.proc = armv5_stc,
	.arch = ARM_ARCH_V5,
},
{
	.mask =  0x0ff00000,
	.icode = 0x0d200000,
	.name = "stc",
	.proc = armv5_stc,
	.arch = ARM_ARCH_V5,
},
{
	.mask =  0x0ff00000,
	.icode = 0x0d400000,
	.name = "stc",
	.proc = armv5_stc,
	.arch = ARM_ARCH_V5,
},
{
	.mask =  0x0ff00000,
	.icode = 0x0d600000,
	.name = "stc",
	.proc = armv5_stc,
	.arch = ARM_ARCH_V5,
},
{
	.mask =  0x0ff00000,
	.icode = 0x0d800000,
	.name = "stc",
	.proc = armv5_stc,
	.arch = ARM_ARCH_V5,
},
{
	.mask =  0x0ff00000,
	.icode = 0x0da00000,
	.name = "stc",
	.proc = armv5_stc,
	.arch = ARM_ARCH_V5,
},
{
	.mask =  0x0ff00000,
	.icode = 0x0dc00000,
	.name = "stc",
	.proc = armv5_stc,
	.arch = ARM_ARCH_V5,
},
{
	.mask =  0x0ff00000,
	.icode = 0x0de00000,
	.name = "stc",
	.proc = armv5_stc,
	.arch = ARM_ARCH_V5,
},
{
	.mask = 0x0e500000,
	.icode = 0x08000000,
	.name = "stm1",
	.proc = armv5_lsm,
	.arch = ARM_ARCH_V5,
},
{
	.mask = 0x0e700000,
	.icode = 0x08400000,
	.name = "stm2",
	.proc = armv5_lsm,
	.arch = ARM_ARCH_V5,
},
{
	.mask =  0x0d700000,
	.icode = 0x04000000,
	.name = "str",
	.proc = armv5_str,
	.arch = ARM_ARCH_V5,
},
{
	.mask =  0x0d700000,
	.icode = 0x05000000,
	.name = "str",
	.proc = armv5_str,
	.arch = ARM_ARCH_V5,
},
{
	.mask =  0x0d700000,
	.icode = 0x05200000,
	.name = "str",
	.proc = armv5_str,
	.arch = ARM_ARCH_V5,
},
{
	.mask = 0x0d700000,
	.icode = 0x04400000,
	.name = "strb",
	.proc = armv5_strb,
	.arch = ARM_ARCH_V5,
},
{
	.mask = 0x0d700000,
	.icode = 0x05400000,
	.name = "strb",
	.proc = armv5_strb,
	.arch = ARM_ARCH_V5,
},
{
	.mask = 0x0d700000,
	.icode = 0x05600000,
	.name = "strb",
	.proc = armv5_strb,
	.arch = ARM_ARCH_V5,
},
{
	.mask = 0x0d700000,
	.icode = 0x04600000,
	.name = "strbt",
	.proc = armv5_strbt,
	.arch = ARM_ARCH_V5,
}, 
{
	.mask =  0x0e1000f0,
	.icode = 0x000000b0,
	.name = "strh",
	.proc = armv5_strh,
	.arch = ARM_ARCH_V5,
},
{
	.mask = 0x0d700000,
	.icode = 0x04200000,
	.name = "strt",
	.proc = armv5_strt,
	.arch = ARM_ARCH_V5,
},
{
	.mask =  0x0fe00090,
	.icode = 0x00400000,
	.name = "sub",
	.proc = armv5_sub,
	.arch = ARM_ARCH_V5,
}, 
{
	.mask =  0x0fe00090,
	.icode = 0x00400010,
	.name = "sub",
	.proc = armv5_sub,
	.arch = ARM_ARCH_V5,
}, 
{
	.mask =  0x0fe00090,
	.icode = 0x00400080,
	.name = "sub",
	.proc = armv5_sub,
	.arch = ARM_ARCH_V5,
}, 
{
	.mask =  0x0fe00090,
	.icode = 0x02400000,
	.name = "sub",
	.proc = armv5_sub,
	.arch = ARM_ARCH_V5,
}, 
{
	.mask =  0x0fe00090,
	.icode = 0x02400010,
	.name = "sub",
	.proc = armv5_sub,
	.arch = ARM_ARCH_V5,
}, 
{
	.mask =  0x0fe00090,
	.icode = 0x02400080,
	.name = "sub",
	.proc = armv5_sub,
	.arch = ARM_ARCH_V5,
}, 
{
	.mask =  0x0fe00090,
	.icode = 0x02400090,
	.name = "sub",
	.proc = armv5_sub,
	.arch = ARM_ARCH_V5,
}, 
{
	.mask = 0x0f000000,
	.icode = 0x0f000000,
	.name = "swi",
	.proc = armv5_swi,
	.arch = ARM_ARCH_V5,
},
{
	.mask = 0x0ff000f0,
	.icode = 0x01000090,
	.name = "swp",
	.proc = armv5_swp,
	.arch = ARM_ARCH_V5,
},
{
	.mask = 0x0ff000f0,
	.icode = 0x01400090,
	.name = "swpb",
	.proc = armv5_swpb,
	.arch = ARM_ARCH_V5,
},
{
	.mask = 0x0ff00090,
	.icode = 0x01300000,
	.name = "teq",
	.proc = armv5_teq,
	.arch = ARM_ARCH_V5,
},
{
	.mask = 0x0ff00090,
	.icode = 0x01300010,
	.name = "teq",
	.proc = armv5_teq,
	.arch = ARM_ARCH_V5,
},
{
	.mask = 0x0ff00090,
	.icode = 0x01300080,
	.name = "teq",
	.proc = armv5_teq,
	.arch = ARM_ARCH_V5,
},
{
	.mask = 0x0ff00090,
	.icode = 0x03300000,
	.name = "teq",
	.proc = armv5_teq,
	.arch = ARM_ARCH_V5,
},
{
	.mask = 0x0ff00090,
	.icode = 0x03300010,
	.name = "teq",
	.proc = armv5_teq,
	.arch = ARM_ARCH_V5,
},
{
	.mask = 0x0ff00090,
	.icode = 0x03300080,
	.name = "teq",
	.proc = armv5_teq,
	.arch = ARM_ARCH_V5,
},
{
	.mask = 0x0ff00090,
	.icode = 0x03300090,
	.name = "teq",
	.proc = armv5_teq,
	.arch = ARM_ARCH_V5,
},
{
	.mask = 0x0ff00090,
	.icode = 0x01100000,
	.name = "tst",
	.proc = armv5_tst,
	.arch = ARM_ARCH_V5,
}, 
{
	.mask = 0x0ff00090,
	.icode = 0x01100010,
	.name = "tst",
	.proc = armv5_tst,
	.arch = ARM_ARCH_V5,
}, 
{
	.mask = 0x0ff00090,
	.icode = 0x01100080,
	.name = "tst",
	.proc = armv5_tst,
	.arch = ARM_ARCH_V5,
}, 
{
	.mask = 0x0ff00090,
	.icode = 0x03100000,
	.name = "tst",
	.proc = armv5_tst,
	.arch = ARM_ARCH_V5,
}, 
{
	.mask = 0x0ff00090,
	.icode = 0x03100010,
	.name = "tst",
	.proc = armv5_tst,
	.arch = ARM_ARCH_V5,
}, 
{
	.mask = 0x0ff00090,
	.icode = 0x03100080,
	.name = "tst",
	.proc = armv5_tst,
	.arch = ARM_ARCH_V5,
}, 
{
	.mask = 0x0ff00090,
	.icode = 0x03100090,
	.name = "tst",
	.proc = armv5_tst,
	.arch = ARM_ARCH_V5,
}, 
{
	.mask = 0x0fe000f0,
	.icode = 0x00a00090,
	.name = "umlal",
	.proc = armv5_umlal,
	.arch = ARM_ARCH_V5,
},
{
	.mask = 0x0fe000f0,
	.icode = 0x00800090,
	.name = "umull",
	.proc = armv5_umull,
	.arch = ARM_ARCH_V5,
},
{
	.mask = 0x0ff00000,
	.icode = 0x0c500000,
	.name = "mrrc",
	.proc = armv5_mrrc,
	.arch = ARM_ARCH_V5,
},
/* Enhanced DSP Instructions */
{
	.mask = 0x0e1000f0,
	.icode = 0x000000d0,
	.name = "ldrd",
	.proc = armv5_ldrd,
	.arch = ARM_ARCH_V5_E,
},
{
	.mask = 0x0ff00000,
	.icode = 0x0c400000,
	.name = "mcrr",
	.proc = armv5_mcrr,
	.arch = ARM_ARCH_V5_E,
},
#if 0
/* ICODE shared with ldbr, differs only in rd = 15 */
{
	.mask = 0xfd70f000,
	.icode = 0xf550f000,
	.name = "pld",
	.proc = armv5_pld
},
#endif
{
	.mask = 0x0ff000f0,
	.icode = 0x01000050,
	.name = "qadd",
	.proc = armv5_qadd,
	.arch = ARM_ARCH_V5_E,
},
{
	.mask = 0x0ff000f0,
	.icode = 0x01400050,
	.name = "qdadd",
	.proc = armv5_qdadd,
	.arch = ARM_ARCH_V5_E,
},
{
	.mask = 0x0ff000f0,
	.icode = 0x01600050,
	.name = "qdsub",
	.proc = armv5_qdsub,
	.arch = ARM_ARCH_V5_E,
},
{
	.mask =  0x0ff000f0,
	.icode = 0x01200050,
	.name = "qsub",
	.proc = armv5_qsub,
	.arch = ARM_ARCH_V5_E,
},
{
	.mask = 0x0ff00090,
	.icode = 0x01000080,
	.name = "smlaxy" ,
	.proc = armv5_smlaxy,
	.arch = ARM_ARCH_V5_E,
},
{	.mask = 0x0ff00090,
	.icode = 0x01400080,
	.name = "smlalxy",
	.proc = armv5_smlalxy,
	.arch = ARM_ARCH_V5_E,
},
{
	.mask = 0x0ff000b0,
	.icode = 0x01200080,
	.name = "smlawy" ,
	.proc = armv5_smlawy,
	.arch = ARM_ARCH_V5_E,
},
{
	.mask = 0x0ff00090,
	.icode = 0x01600080,
	.name = "smulxy",
	.proc = armv5_smulxy,
	.arch = ARM_ARCH_V5_E,
},
{
	.mask = 0x0ff000b0,
	.icode = 0x012000a0,
	.name = "smulwy",
	.proc = armv5_smulwy,
	.arch = ARM_ARCH_V5_E,
},
{
	.mask =  0x0e1000f0,
	.icode = 0x000000f0,
	.name = "strd",
	.proc = armv5_strd,
	.arch = ARM_ARCH_V5_E,
},
{	.mask = 0x0,
	.icode = 0x0,
	.name = "end",
	.proc = NULL
}
};

Instruction undefined = { 
	.mask = 0x0,
	.icode = 0x0,
	.name = "und",
	.proc = armv5_und
};

static void
decoder_add_instruction(IDecoder *dec,Instruction *new_inst,int index) {
	Instruction *inst = alloc_instruction();
	*inst = *new_inst;	// create copy !!!! because of the linked list
	if(dec->instr[index]==NULL) {
		dec->instr[index]=inst;
	} else {
		fprintf(stderr,"More than one for %s %s\n",inst->name,dec->instr[index]->name);
		exit(1);
	}
}

void
IDecoder_New() {
	int i;
	Instruction *cursor;
	IDecoder *dec;
	idecoder = dec = sg_new(IDecoder);
	imem = sg_calloc(sizeof(Instruction)*MAX_INSTRUCTIONS);
	memset(dec,0,sizeof(IDecoder));
	for(i=0;i<=INSTR_INDEX_MAX;i++) {
		for(cursor=instrlist;cursor->mask;cursor++) {
			if((INSTR_UNINDEX(i)&cursor->mask)==(cursor->icode & INSTR_INDEX_MASK)) { 
				decoder_add_instruction(dec,cursor,i);
			}
		}
	}
	fprintf(stderr,"- Instruction decoder Initialized: ");
	iProcTab = sg_calloc(sizeof(InstructionProc *)*(INSTR_INDEX_MAX+1));
	if(!iProcTab) {
		fprintf(stderr,"Out of Memory");
		exit(378);
	}
	for(i=0;i<=INSTR_INDEX_MAX;i++) {
		Instruction *instr = dec->instr[i];
		if(instr == NULL) {
			instr = &undefined;
		}
		iProcTab[i] = instr->proc;
	}
	fprintf(stderr,"\n");
//	fprintf(stderr,"\nMedium Nr of Instructions %f\n",(float)sum/validcount);
}

Instruction *
InstructionFind(uint32_t icode) {
        int index=INSTR_INDEX(icode);
        Instruction *inst=idecoder->instr[index];
        return inst;
}
