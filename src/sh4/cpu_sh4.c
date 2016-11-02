/*
 *************************************************************************************************
 *
 * Emulation of the SH4 CPU, Initialization and
 * main loop
 *
 * Copyright 2009 Jochen Karrer. All rights reserved.
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

#include "compiler_extensions.h"
#include "cycletimer.h"
#include "idecode_sh4.h"
#include "mainloop_events.h"
#include "mmu_sh4.h"
#include "configfile.h"
#include "fio.h"
#include "signode.h"
#include "debugger.h"
#include "cpu_sh4.h"
#include "clock.h"
#include "cycletimer.h"
#include "sglib.h"
#include "sgstring.h"

#define REG_TRAPA(base) 	((base) + 0x20)
#define REG_EXPEVT(base) 	((base) + 0x24)
#define REG_INTEVT(base) 	((base) + 0x28)

SH4Cpu gcpu_sh4;

typedef enum ExVecBase {
	VBASE_0,
	VBASE_VBR,
	VBASE_DBR,
} ExVecBase;

typedef struct SH4_Exception_t {
	int index;
	uint32_t prio_level;
	uint32_t prio_order;
	ExVecBase vec_base;
	uint32_t vec_offset;
	uint32_t ex_code;
} SH4_Exception_t;


static SH4_Exception_t extable[] = {
	{
		.index = EX_POWERON,
		.prio_level = 1,
		.prio_order = 1,
		.vec_base = VBASE_0,
		.vec_offset = 0xA0000000,
		.ex_code = 0x000
	}, 
	{
		.index = EX_MANRESET,
		.prio_level = 1,
		.prio_order = 2,
		.vec_base = VBASE_0,
		.vec_offset = 0xA0000000,
		.ex_code = 0x020
	},
	{
		.index = EX_HUDIRESET,
		.prio_level = 1,
		.prio_order = 1,
		.vec_base = VBASE_0,
		.vec_offset = 0xA0000000,
		.ex_code = 0x000
	},
	{
		.index = EX_ITLBMULTIHIT,
		.prio_level = 1,
		.prio_order = 3,
		.vec_base = VBASE_0,
		.vec_offset = 0xA0000000,
		.ex_code = 0x140,	
	},
	{
		.index = EX_DTLBMULTIHIT,
		.prio_level = 1,
		.prio_order = 4,
		.vec_base = VBASE_0,
		.vec_offset = 0xA0000000,
		.ex_code = 0x140
	},
	{
		.index = EX_UBRKBEFORE,
		.prio_level = 2,
		.prio_order = 0,
		.vec_base = VBASE_VBR,
		.vec_offset = 0x100,
		.ex_code = 0x1E0
	},
	{
		.index = EX_IADDERR,
		.prio_level = 2,
		.prio_order = 1,
		.vec_base = VBASE_VBR,
		.vec_offset = 0x100,
		.ex_code = 0x0E0
	},
	{
		.index = EX_ITLBMISS,
		.prio_level = 2,
		.prio_order = 2,
		.vec_base = VBASE_VBR,
		.vec_offset = 0x400,
		.ex_code = 0x040
	},
	{
		.index = EX_EXECPROT,
		.prio_level = 2,
		.prio_order = 3,
		.vec_base = VBASE_VBR,
		.vec_offset = 0x100,
		.ex_code = 0x0A0
	},
	{
		.index = EX_RESINST,
		.prio_level = 2,
		.prio_order = 4,
		.vec_base = VBASE_VBR,
		.vec_offset = 0x100,
		.ex_code = 0x180
	},
	{
		.index = EX_ILLSLOT,
		.prio_level = 2,
		.prio_order = 4,
		.vec_base = VBASE_VBR,
		.vec_offset = 0x100,
		.ex_code = 0x1A0
	},
	{
		.index = EX_FPUDIS,
		.prio_level = 2,
		.prio_order = 4,
		.vec_base = VBASE_VBR,
		.vec_offset = 0x100,
		.ex_code = 0x800
	},
	{
		.index = EX_SLOTFPUDIS,
		.prio_level = 2,
		.prio_order = 4,
		.vec_base = VBASE_VBR,
		.vec_offset = 0x100,
		.ex_code = 0x820
	}, 
	{
		.index = EX_RADDERR,
		.prio_level = 2,
		.prio_order = 5,
		.vec_base = VBASE_VBR,
		.vec_offset = 0x100,
		.ex_code = 0x0E0
	},
	{
		.index = EX_WADDERR,
		.prio_level = 5,
		.vec_base = VBASE_VBR,
		.vec_offset = 0x100,
		.ex_code = 0x100
	},
	{
		.index = EX_RTLBMISS,
		.prio_level = 2,
		.prio_order = 6,
		.vec_base = VBASE_VBR,
		.vec_offset = 0x400,
		.ex_code = 0x040
	},
	{
		.index = EX_WTLBMISS,
		.prio_level = 2,
		.prio_order = 6,
		.vec_base = VBASE_VBR,
		.vec_offset = 0x400,
		.ex_code = 0x060
	}, 
	{
		.index = EX_READPROT,
		.prio_level = 2,
		.prio_order = 7,
		.vec_base = VBASE_VBR,
		.vec_offset = 0x100,
		.ex_code = 0x0a0
	},
	{
		.index = EX_WRITEPROT,
		.prio_level = 2,
		.prio_order = 7,
		.vec_base = VBASE_VBR,
		.vec_offset = 0x100,
		.ex_code = 0x0c0
	},
	{
		.index = EX_FPUEXC,
		.prio_level = 2,
		.prio_order = 8,
		.vec_base = VBASE_VBR,
		.vec_offset = 0x100,
		.ex_code = 0x120
	},
	{
		.index = EX_FIRSTWRITE,
		.prio_level = 2,
		.prio_order = 9,
		.vec_base = VBASE_VBR,
		.vec_offset = 0x100,
		.ex_code = 0x080
	},
	{
		.index = EX_TRAP,
		.prio_level = 2,
		.prio_order = 4,
		.vec_base = VBASE_VBR,
		.vec_offset = 0x100,
		.ex_code = 0x160,
	},
	{
		.index = EX_UBRKAFTER,
		.prio_level = 2,
		.prio_order = 10,
		.vec_base = VBASE_VBR,
		.vec_offset = 0x100,
		.ex_code = 0x1E0
	},
	{
		.index = EX_NMI,
		.prio_level = 3,
		.prio_order = 0,
		.vec_base = VBASE_VBR,
		.vec_offset = 0x600,
		.ex_code = 0x1C0
	},
	/* Maybe the following are system dependent,
	   These are from the Renesas manual */
	{
		.index = EX_IRL_INT_0,
		.prio_level = 4,
		.prio_order = -1,
		.vec_base = VBASE_VBR,
		.vec_offset = 0x600,
		.ex_code = 0x200
	},
	{
		.index = EX_IRL_INT_1,
		.prio_level = 4,
		.prio_order = -1,
		.vec_base = VBASE_VBR,
		.vec_offset = 0x600,
		.ex_code = 0x220
	},
	{
		.index = EX_IRL_INT_2,
		.prio_level = 4,
		.prio_order = -1,
		.vec_base = VBASE_VBR,
		.vec_offset = 0x600,
		.ex_code = 0x240
	},
	{
		.index = EX_IRL_INT_3,
		.prio_level = 4,
		.prio_order = -1,
		.vec_base = VBASE_VBR,
		.vec_offset = 0x600,
		.ex_code = 0x260
	},
	{
		.index = EX_IRL_INT_4,
		.prio_level = 4,
		.prio_order = -1,
		.vec_base = VBASE_VBR,
		.vec_offset = 0x600,
		.ex_code = 0x280
	},
	{
		.index = EX_IRL_INT_5,
		.prio_level = 4,
		.prio_order = -1,
		.vec_base = VBASE_VBR,
		.vec_offset = 0x600,
		.ex_code = 0x2A0
	},
	{
		.index = EX_IRL_INT_6,
		.prio_level = 4,
		.prio_order = -1,
		.vec_base = VBASE_VBR,
		.vec_offset = 0x600,
		.ex_code = 0x2C0
	},
	{
		.index = EX_IRL_INT_7,
		.prio_level = 4,
		.prio_order = -1,
		.vec_base = VBASE_VBR,
		.vec_offset = 0x600,
		.ex_code = 0x2E0
	},
	{
		.index = EX_IRL_INT_8,
		.prio_level = 4,
		.prio_order = -1,
		.vec_base = VBASE_VBR,
		.vec_offset = 0x600,
		.ex_code = 0x300
	},
	{
		.index = EX_IRL_INT_9,
		.prio_level = 4,
		.prio_order = -1,
		.vec_base = VBASE_VBR,
		.vec_offset = 0x600,
		.ex_code = 0x320
	},
	{
		.index = EX_IRL_INT_A,
		.prio_level = 4,
		.prio_order = -1,
		.vec_base = VBASE_VBR,
		.vec_offset = 0x600,
		.ex_code = 0x340
	},
	{
		.index = EX_IRL_INT_B,
		.prio_level = 4,
		.prio_order = -1,
		.vec_base = VBASE_VBR,
		.vec_offset = 0x600,
		.ex_code = 0x360
	},
	{
		.index = EX_IRL_INT_C,
		.prio_level = 4,
		.prio_order = -1,
		.vec_base = VBASE_VBR,
		.vec_offset = 0x600,
		.ex_code = 0x380
	},
	{
		.index = EX_IRL_INT_D,
		.prio_level = 4,
		.prio_order = -1,
		.vec_base = VBASE_VBR,
		.vec_offset = 0x600,
		.ex_code = 0x3A0
	},
	{
		.index = EX_IRL_INT_E,
		.prio_level = 4,
		.prio_order = -1,
		.vec_base = VBASE_VBR,
		.vec_offset = 0x600,
		.ex_code = 0x3E0
	},
	{
		.index = EX_PERINT_TMU0,
		.prio_level = 4,
		.prio_order = -1,
		.vec_base = VBASE_VBR,
		.vec_offset = 0x600,
		.ex_code = 0x400
	},
	{
		.index = EX_PERINT_TMU1,
		.prio_level = 4,
		.prio_order = -1,
		.vec_base = VBASE_VBR,
		.vec_offset = 0x600,
		.ex_code = 0x420
	},
	{
		.index = EX_PERINT_TMU2,
		.prio_level = 4,
		.prio_order = -1,
		.vec_base = VBASE_VBR,
		.vec_offset = 0x600,
		.ex_code = 0x440
	},
	{	.index = EX_PERINT_TMU2_TICPI2,
		.prio_level = 4,
		.prio_order = -1,
		.vec_base = VBASE_VBR,
		.vec_offset = 0x600,
		.ex_code = 0x460
	},
	{
		.index = EX_PERINT_TMU3,
		.prio_level = 4,
		.prio_order = -1,
		.vec_base = VBASE_VBR,
		.vec_offset = 0x600,
		.ex_code = 0xb00
	},
	{
		.index = EX_PERINT_TMU4,
		.prio_level = 4,
		.prio_order = -1,
		.vec_base = VBASE_VBR,
		.vec_offset = 0x600,
		.ex_code = 0xb80
	},
	{
		.index = EX_PERINT_RTC_ATI,
		.prio_level = 4,
		.prio_order = -1,
		.vec_base = VBASE_VBR,
		.vec_offset = 0x600,
		.ex_code = 0x480
	},
	{ 
		.index = EX_PERINT_RTC_PRI,
		.prio_level = 4,
		.prio_order = -1,
		.vec_base = VBASE_VBR,
		.vec_offset = 0x600,
		.ex_code = 0x4A0
	},	
	{
		.index = EX_PERINT_RTC_CUI,
		.prio_level = 4,
		.prio_order = -1,
		.vec_base = VBASE_VBR,
		.vec_offset = 0x600,
		.ex_code = 0x4C0
	},
	{
		.index = EX_PERINT_SCI_ERI,
		.prio_level = 4,
		.prio_order = -1,
		.vec_base = VBASE_VBR,
		.vec_offset = 0x600,
		.ex_code = 0x4E0
	},
	{
		.index = EX_PERINT_SCI_RXI,
		.prio_level = 4,
		.prio_order = -1,
		.vec_base = VBASE_VBR,
		.vec_offset = 0x600,
		.ex_code = 0x500
	},
	{
		.index = EX_PERINT_SCI_TXI,
		.prio_level = 4,
		.prio_order = -1,
		.vec_base = VBASE_VBR,
		.vec_offset = 0x600,
		.ex_code = 0x520
	},
	{
		.index = EX_PERINT_SCI_TEI,
		.prio_level = 4,
		.prio_order = -1,
		.vec_base = VBASE_VBR,
		.vec_offset = 0x600,
		.ex_code = 0x540
	},
	{
		.index = EX_PERINT_WDT_ITI,
		.prio_level = 4,
		.prio_order = -1,
		.vec_base = VBASE_VBR,
		.vec_offset = 0x600,
		.ex_code = 0x560
	},
	{
		.index = EX_PERINT_REF_RCMI,
		.prio_level = 4,
		.prio_order = -1,
		.vec_base = VBASE_VBR,
		.vec_offset = 0x600,
		.ex_code = 0x580
	},
	{
		.index = EX_PERINT_REF_ROVI,
		.prio_level = 4,
		.prio_order = -1,
		.vec_base = VBASE_VBR,
		.vec_offset = 0x600,
		.ex_code = 0x5A0
	},
	{
		.index = EX_PERINT_HUDI,
		.prio_level = 4,
		.prio_order = -1,
		.vec_base = VBASE_VBR,
		.vec_offset = 0x600,
		.ex_code = 0x600
	},
	{
		.index = EX_PERINT_GPIO,
		.prio_level = 4,
		.prio_order = -1,
		.vec_base = VBASE_VBR,
		.vec_offset = 0x600,
		.ex_code = 0x620
	},
	{
		.index = EX_PERINT_DMAC_DMTE0,
		.prio_level = 4,
		.prio_order = -1,
		.vec_base = VBASE_VBR,
		.vec_offset = 0x600,
		.ex_code = 0x640
	},
	{
		.index = EX_PERINT_DMAC_DMTE1,
		.prio_level = 4,
		.prio_order = -1,
		.vec_base = VBASE_VBR,
		.vec_offset = 0x600,
		.ex_code = 0x660
	},
	{
		.index = EX_PERINT_DMAC_DMTE2,
		.prio_level = 4,
		.prio_order = -1,
		.vec_base = VBASE_VBR,
		.vec_offset = 0x600,
		.ex_code = 0x680
	},
	{
		.index = EX_PERINT_DMAC_DMTE3,
		.prio_level = 4,
		.prio_order = -1,
		.vec_base = VBASE_VBR,
		.vec_offset = 0x600,
		.ex_code = 0x6A0
	},
	{
		.index = EX_PERINT_DMAC_DMAE,
		.prio_level = 4,
		.prio_order = -1,
		.vec_base = VBASE_VBR,
		.vec_offset = 0x600,
		.ex_code = 0x6C0
	},
	{
		.index = EX_PERINT_SCIF_ERI,
		.prio_level = 4,
		.prio_order = -1,
		.vec_base = VBASE_VBR,
		.vec_offset = 0x600,
		.ex_code = 0x700,
	},
	{
		.index = EX_PERINT_SCIF_RXI,
		.prio_level = 4,
		.prio_order = -1,
		.vec_base = VBASE_VBR,
		.vec_offset = 0x600,
		.ex_code = 0x720,
	},
	{
		.index = EX_PERINT_SCIF_BRI,
		.prio_level = 4,
		.prio_order = -1,
		.vec_base = VBASE_VBR,
		.vec_offset = 0x600,
		.ex_code = 0x740,
	},
	{
		.index = EX_PERINT_SCIF_TXI,
		.prio_level = 4,
		.prio_order = -1,
		.vec_base = VBASE_VBR,
		.vec_offset = 0x600,
		.ex_code = 0x760,
	},
	{
		.index = EX_PERINT_PCIC_PCISERR,
		.prio_level = 4,
		.prio_order = -1,
		.vec_base = VBASE_VBR,
		.vec_offset = 0x600,
		.ex_code = 0xA00,
	},
	{
		.index = EX_PERINT_PCIC_PCIERR,
		.prio_level = 4,
		.prio_order = -1,
		.vec_base = VBASE_VBR,
		.vec_offset = 0x600,
		.ex_code = 0xAE0,
	},
	{
		.index = EX_PERINT_PCIC_PCIPWDWN,
		.prio_level = 4,
		.prio_order = -1,
		.vec_base = VBASE_VBR,
		.vec_offset = 0x600,
		.ex_code = 0xAC0,
	},
	{
		.index = EX_PERINT_PCIC_PCIPWON,
		.prio_level = 4,
		.prio_order = -1,
		.vec_base = VBASE_VBR,
		.vec_offset = 0x600,
		.ex_code = 0xAA0,
	},
	{
		.index = EX_PERINT_PCIC_PCIDMA0,
		.prio_level = 4,
		.prio_order = -1,
		.vec_base = VBASE_VBR,
		.vec_offset = 0x600,
		.ex_code = 0xA80,
	},
	{
		.index = EX_PERINT_PCIC_PCIDMA1,
		.prio_level = 4,
		.prio_order = -1,
		.vec_base = VBASE_VBR,
		.vec_offset = 0x600,
		.ex_code = 0xA60,
	},
	{
		.index = EX_PERINT_PCIC_PCIDMA2,
		.prio_level = 4,
		.prio_order = -1,
		.vec_base = VBASE_VBR,
		.vec_offset = 0x600,
		.ex_code = 0xA40,
	},
	{
		.index = EX_PERINT_PCIC_PCIDMA3,
		.prio_level = 4,
		.prio_order = -1,
		.vec_base = VBASE_VBR,
		.vec_offset = 0x600,
		.ex_code = 0xA20,
	}
};

#if 0
    From gdb
    /* general registers 0-15 */
    "r0", "r1", "r2", "r3", "r4", "r5", "r6", "r7",
    "r8", "r9", "r10", "r11", "r12", "r13", "r14", "r15",
    /* 16 - 22 */
    "pc", "pr", "gbr", "vbr", "mach", "macl", "sr",
    /* 23, 24 */
    "fpul", "fpscr",
    /* floating point registers 25 - 40 */
    "fr0", "fr1", "fr2", "fr3", "fr4", "fr5", "fr6", "fr7",
    "fr8", "fr9", "fr10", "fr11", "fr12", "fr13", "fr14", "fr15",
    /* 41, 42 */
    "ssr", "spc",
    /* bank 0 43 - 50 */
    "r0b0", "r1b0", "r2b0", "r3b0", "r4b0", "r5b0", "r6b0", "r7b0",
    /* bank 1 51 - 58 */
    "r0b1", "r1b1", "r2b1", "r3b1", "r4b1", "r5b1", "r6b1", "r7b1",
    "", "", "", "", "", "", "", "",
    /* pseudo bank register. */
    "",
    /* double precision (pseudo) 59 - 66 */
    "dr0", "dr2", "dr4", "dr6", "dr8", "dr10", "dr12", "dr14",
    /* vectors (pseudo) 67 - 70 */
    "fv0", "fv4", "fv8", "fv12",
    /* FIXME: missing XF 71 - 86 */
    /* FIXME: missing XD 87 - 94 */
#endif

static void
debugger_setreg(void *clientData,const uint8_t *data,uint32_t index,int len)
{
	uint32_t value;
	if(len < 4) {
		return;
	}
	value = target32_to_host(*((uint32_t*)data));
        if(index < 16) {
		SH4_SetGpr(value,index);		
	} else if (index == 16) {
		SH4_SetRegPC(value);
	} else if(index == 17) {
		SH4_SetPR(value);
	} else if(index == 18) {
		SH4_SetGBR(value);
	} else if(index == 19) {
		SH4_SetVBR(value);
	} else if(index == 20) {
		SH4_SetMacH(value);
	} else if(index == 21) {
		SH4_SetMacL(value);
	} else if(index == 22) {
		SH4_SetSR(value);
	} else if (index >= 23 && (index <= 42)) {
		/* Ignore floating point for now */
	} else if((index >= 43) && (index <= 50)) {
		if(SH4_GetSR() & SR_RB) {
			SH4_SetGprBank(value,index - 43);
		} else {
			SH4_SetGpr(value,index - 43);
		}
	} else if((index >= 51) && (index <= 58)) {
		if(SH4_GetSR() & SR_RB) {
			SH4_SetGpr(value,index - 51);
		} else {
			SH4_SetGprBank(value,index - 51);
		}
	}
}

static int
debugger_getreg(void *clientData,uint8_t *data,uint32_t index,int maxlen)
{
	int len = 0;
	uint32_t value;
	if(maxlen < 4) {
		return 0;
	}
	if(index < 16) {
		value = SH4_GetGpr(index);
		len = 4;
	} else if(index == 16) {
		value = SH4_NIA;
		len = 4;
	} else if (index == 17) {
		value = SH4_GetPR();
		len = 4;
	} else if (index == 18) {
		value = SH4_GetGBR();
		len = 4;
	} else if (index == 19) {
		value = SH4_GetVBR();
		len = 4;
	} else if (index == 20) {
		value = SH4_GetMacH();
		len = 4;
	} else if (index == 21) {
		value = SH4_GetMacL();
		len = 4;
	} else if (index == 22) {
		value = SH4_GetSR();
		len = 4;
	} else if (index >= 23 && (index <= 42)) {
		value = 0;
		len = 4;
	} else if((index >= 43) && (index <= 50)) {
		if(SH4_GetSR() & SR_RB) {
			value = SH4_GetGprBank(index - 43);
		} else {
			value = SH4_GetGpr(index - 43);
		}
		len = 4;
	} else if((index >= 51) && (index <= 58)) {
		if(SH4_GetSR() & SR_RB) {
			value = SH4_GetGpr(index - 51);
		} else {
			value = SH4_GetGprBank(index - 51);
		}
		len = 4;
	}
	if(len == 4) {
		//fprintf(stderr,"Reg%d: %08x\n",index,host32_to_target(value));
		*((uint32_t*)data) = host32_to_target(value);
	}
	return len;
}

static ssize_t
debugger_getmem(void *clientData,uint8_t *data,uint64_t addr,uint32_t count)
{
	uint32_t i;
	for(i = 0;(i + 3) < count; i+=4) {
		uint32_t *datap = (uint32_t *)(data + i);
                *datap = host32_to_target(SH4_MMURead32(addr + i));
        }
	if((i + 1) < count) {
		uint16_t *datap = (uint16_t *)(data + i);
		*datap = host16_to_target(SH4_MMURead16(addr + i));
		i+=2;	
	}
	if(i < count) {
		*data = SH4_MMURead8(addr + i);
	}
        return count;
}

static ssize_t
debugger_setmem(void *clientData,const uint8_t *data,uint64_t addr,uint32_t count)
{
	uint32_t i;
	for(i = 0;(i + 3) < count; i+=4) {
		uint32_t *datap = (uint32_t *)(data + i);
		SH4_MMUWrite32(target32_to_host(*datap),addr + i);
        }
	if((i + 1) < count) {
		uint16_t *datap = (uint16_t *)(data + i);
		SH4_MMUWrite16(target16_to_host(*datap),addr + i);
		i+=2;	
	}
	if(i < count) {
		SH4_MMUWrite8(*data,addr + i);
	}
        return count;
}

static int
debugger_stop(void *clientData)
{
	SH4Cpu *sh4 = (SH4Cpu *) clientData;
	sh4->dbg_state = SH4DBG_STOP;
        SH4_PostSignal(SH4_SIG_DBG);
	fprintf(stderr,"Posted stop\n");
        return -1;
}


static int
debugger_cont(void *clientData)
{
	SH4Cpu *sh4 = (SH4Cpu *) clientData;
        sh4->dbg_state = SH4DBG_RUNNING;
        /* Should only be called if there are no breakpoints */
        SH4_UnpostSignal(SH4_SIG_DBG);
        return 0;
}

static int
debugger_step(void *clientData,uint64_t addr,int use_addr)
{
	SH4Cpu *sh4 = (SH4Cpu *) clientData;
        if(use_addr)  {
                SH4_SetRegPC(addr);
        }
        sh4->dbg_steps = 1;
        sh4->dbg_state = SH4DBG_STEP;
        return -1;
}

static Dbg_TargetStat
debugger_get_status(void *clientData)
{
	SH4Cpu *sh4 = (SH4Cpu *) clientData;
        if(sh4->dbg_state == SH4DBG_STOPPED) {
                return DbgStat_SIGINT;
        } else if(sh4->dbg_state == SH4DBG_RUNNING) {
                return DbgStat_RUNNING;
	}
	return -1;
}

/*
 ****************************************************************************
 * get_bkpt_ins
 *      returns the operation code of the breakpoint instruction.
 *      Needed by the debugger to insert breakpoints
 ****************************************************************************
 */
static void
debugger_get_bkpt_ins(void *clientData,uint8_t *ins,uint64_t addr,int len)
{
        if(len == 2) {
                /* trapa instruction is 0xc3XX */
                ins[0] = 0xc3;
                ins[1] = 0xc3;
        } else {
		fprintf(stderr,"Unexpected length in get_bkpt_ins\n");
	}
}


static void
SH4_IrqTrace(SigNode *sig,int value,void *clientData)
{
        if(value == SIG_LOW) {
                SH4_PostSignal(SH4_SIG_IRQ);
        } else {
                SH4_UnpostSignal(SH4_SIG_IRQ);
	}
}

/*
 */

static void
Do_Debug(void) {
        if(gcpu_sh4.dbg_state == SH4DBG_RUNNING) {
                fprintf(stderr,"Debug mode is off, should not be called\n");
        }  else if(gcpu_sh4.dbg_state == SH4DBG_STEP) {
                if(gcpu_sh4.dbg_steps == 0) {
                        gcpu_sh4.dbg_state = SH4DBG_STOPPED;
                        if(gcpu_sh4.debugger) {
                                Debugger_Notify(gcpu_sh4.debugger,DbgStat_SIGTRAP);
                        }
                        SH4_RestartIdecoder();
                } else {
                        gcpu_sh4.dbg_steps--;
                        /* Requeue event */
                        mainloop_event_pending = 1;
                }
        } else if(gcpu_sh4.dbg_state == SH4DBG_STOP) {
                gcpu_sh4.dbg_state = SH4DBG_STOPPED;
                if(gcpu_sh4.debugger) {
                        Debugger_Notify(gcpu_sh4.debugger,DbgStat_SIGINT);
                }
                SH4_RestartIdecoder();
        } else if(gcpu_sh4.dbg_state == SH4DBG_BREAK) {
                if(gcpu_sh4.debugger) {
                        if(Debugger_Notify(gcpu_sh4.debugger,DbgStat_SIGTRAP) > 0) {
                                gcpu_sh4.dbg_state = SH4DBG_STOPPED;
                                SH4_RestartIdecoder();
                        }       /* Else no debugger session open */
                } else {
                        // Exception break
                        gcpu_sh4.dbg_state = SH4DBG_RUNNING;
                }
        } else {
                fprintf(stderr,"Unknown restart signal reason %d\n",
				gcpu_sh4.dbg_state);
        }
}

void 
SH4_Exception(uint32_t ex_index) 
{
	SH4Cpu *sh4 = &gcpu_sh4;
	uint32_t sr;
	uint32_t vec_addr;
	SH4_Exception_t *ex = &extable[ex_index];
	sr = SH4_GetSR();
	fprintf(stderr,"Exception %d at 0x%08x not tested\n",ex_index,SH4_GetRegPC());
	if(sr & SR_BL) {
		fprintf(stderr,"Exception while SR_BL = 0\n");
		exit(1);
	}
	SH4_SetSSR(SH4_GetSR());
	SH4_SetSPC(SH4_GetRegPC());
	SH4_SetSGR(SH4_GetGpr(15));
	SH4_SetSR(SH4_GetSR() | SR_BL | SR_RB | SR_MD);
	if(ex_index >= EX_IRL_INT_0) {
		sh4->io_intevt = ex->ex_code;
	} else {
		sh4->io_expevt = ex->ex_code;
	}
	switch(ex->vec_base) {
		case VBASE_VBR:
			vec_addr = SH4_GetVBR() + ex->vec_offset;
			break;	
		default:
		case VBASE_0:
			vec_addr = ex->vec_offset;
	}
	SH4_SetRegPC(vec_addr);
}

static inline void
CheckSignals(void)
{
        if(unlikely(mainloop_event_pending)) {
                mainloop_event_pending = 0;
                if(mainloop_event_io) {
                        FIO_HandleInput();
                }
		if(gcpu_sh4.signals & SH4_SIG_IRQ) {

		} 
		if(gcpu_sh4.signals & SH4_SIG_DBG) {
                        Do_Debug();
		} 
		if(gcpu_sh4.signals & SH4_SIG_RESTART_IDEC) {
                        SH4_RestartIdecoder();
		}
        }
}

/*
 *********************************************************************
 * I should check what happens with the PC if an exception happens
 *********************************************************************
 */
void
SH4_ExecuteDelaySlot(void)
{
        SH4_InstructionProc *iproc;
	CycleCounter+=2;
//	CheckSignals(); /* first check if interrupts are possible here */
	CycleTimers_Check();
        ICODE = SH4_MMURead16(SH4_GetRegPC());
        iproc = SH4_InstructionProcFind(ICODE);
    //    fprintf(stderr,"Branch delay Slot at %08x\n",SH4_GetRegPC());
        iproc();
}


void
SH4_Run() 
{
	SH4Cpu *sh4 = &gcpu_sh4;
	uint32_t dbgwait;
	SH4_InstructionProc *iproc;
	uint32_t addr;
        if(Config_ReadUInt32(&addr,"global","start_address")<0) {
                addr=0;
        }
	if(Config_ReadUInt32(&dbgwait,"global","dbgwait") < 0) {
                dbgwait=0;
        }
        if(dbgwait) {
                fprintf(stderr,
			"CPU is waiting for debugger connection at %08x\n",addr);
                sh4->dbg_state = SH4DBG_STOPPED;
        	SH4_PostSignal(SH4_SIG_DBG);
	}
        SH4_SetRegPC(addr);
	setjmp(sh4->restart_idec_jump);
        while(sh4->dbg_state == SH4DBG_STOPPED) {
                struct timespec tout;
                tout.tv_nsec=0;
                tout.tv_sec=10000;
                FIO_WaitEventTimeout(&tout);
        }
	while(1) {
//		SH4_Instruction *instr;
		CycleCounter+=2;
		CheckSignals();
		CycleTimers_Check();
		ICODE = SH4_MMUIFetch(SH4_GetRegPC());
		iproc = SH4_InstructionProcFind(ICODE);	
#if 0
		instr = SH4_InstructionFind(ICODE);
		fprintf(stderr,"PC %08x, instr %s\n",SH4_GetRegPC(),instr->name);
#endif
		SH4_SetRegPC(SH4_GetRegPC() + 2);
#if 0
		fprintf(stderr,"Inc to %08x\n",SH4_GetRegPC());
#endif
		iproc();
	}	
}

static uint32_t
trapa_read(void *clientData,uint32_t address,int rqlen)
{
	fprintf(stderr,"Trapa register not implemented\n");
        return 0; 
}

static void
trapa_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	fprintf(stderr,"Trapa register not implemented\n");
}

static uint32_t
expevt_read(void *clientData,uint32_t address,int rqlen)
{
	fprintf(stderr,"Expevt register not implemented\n");
        return 0; 
}

static void
expevt_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	fprintf(stderr,"Expevt register not implemented\n");
}

static uint32_t
intevt_read(void *clientData,uint32_t address,int rqlen)
{
	fprintf(stderr,"Intevt register not implemented\n");
        return 0; 
}

static void
intevt_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	fprintf(stderr,"Intevt register not implemented\n");
}


CycleTimer htimer;
static void
hello_proc(void *clientData) 
{
	Clock_DumpTree(Clock_Find("osc"));
}

void
SH4_Init(const char *instancename) {

	SH4Cpu *sh4 = &gcpu_sh4;
	int i;
	uint32_t cpu_clock = 265500000;
	sh4->sigIrq = SigNode_New("%s.irq",instancename);
	if(!sh4->sigIrq) {
		fprintf(stderr,"Can not create interrupt line for sh4 cpu\n");
		exit(1);
	}
	Config_ReadUInt32(&cpu_clock,"global","cpu_clock");
	SH4_IDecoderNew();
	SigNode_Trace(sh4->sigIrq,SH4_IrqTrace,sh4);
	sh4->signals_mask |= SH4_SIG_DBG | SH4_SIG_RESTART_IDEC; 
	sh4->dbg_state = SH4DBG_RUNNING;
        sh4->dbgops.getreg = debugger_getreg;
        sh4->dbgops.setreg = debugger_setreg;
        sh4->dbgops.stop = debugger_stop;
        sh4->dbgops.cont = debugger_cont;
        sh4->dbgops.get_status = debugger_get_status;
        sh4->dbgops.getmem = debugger_getmem;
        sh4->dbgops.setmem = debugger_setmem;
        sh4->dbgops.step = debugger_step;
        sh4->dbgops.get_bkpt_ins = debugger_get_bkpt_ins;
	sh4->debugger = Debugger_New(&sh4->dbgops,sh4);
	SH4_SetSR(SR_MD | SR_RB | SR_BL | (0xf << SR_IMASK_SHIFT));
	SH4_SetVBR(0);
	SH4_SetGpr(0x123,7); /* just for test */
	SH4_SetFPSCR(0x00040001);
	CycleTimers_Init(instancename,cpu_clock);

	CycleTimer_Add(&htimer,100000000,hello_proc,NULL);
	for(i = 0; i < array_size(extable); i++) {
		if(extable[i].index != i) {
			fprintf(stderr,"Exception table is bad at index %d\n",i);
			exit(1);
		}
	}
	IOH_New32(REG_TRAPA(0xff000000),trapa_read,trapa_write,sh4);
	IOH_New32(REG_TRAPA(0x1f000000),trapa_read,trapa_write,sh4);
	IOH_New32(REG_EXPEVT(0xff000000),expevt_read,expevt_write,sh4);
	IOH_New32(REG_EXPEVT(0x1f000000),expevt_read,expevt_write,sh4);
	IOH_New32(REG_INTEVT(0xff000000),intevt_read,intevt_write,sh4);
	IOH_New32(REG_INTEVT(0x1f000000),intevt_read,intevt_write,sh4);
	sh4->io_trapa = sh4->io_expevt = sh4->io_intevt = 0;
	sh4->sfloat = SFloat_New();
}

