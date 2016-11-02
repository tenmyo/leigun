/*
 **************************************************************************************************
 *
 * ARM Thumb Instruction decoder
 * Status: Working 
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

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "thumb_decode.h"
#include "thumb_instructions.h"
#include "sgstring.h"

ThumbInstructionProc **thumbIProcTab = NULL;
ThumbInstruction **thumbInstructionTab = NULL;

static ThumbInstruction instrlist[] = {
	{0xffc0, 0x4140, "adc", th_adc},
	{0xfe00, 0x1c00, "add_1", th_add_1},
	{0xf800, 0x3000, "add_2", th_add_2},
	{0xfe00, 0x1800, "add_3", th_add_3},
	{0xff00, 0x4400, "add_4", th_add_4},
	{0xf800, 0xa000, "add_5", th_add_5},
	{0xf800, 0xa800, "add_6", th_add_6},
	{0xff80, 0xb000, "add_7", th_add_7},
	{0xffc0, 0x4000, "and", th_and},
	{0xf800, 0x1000, "asr_1", th_asr_1},
	{0xffc0, 0x4100, "asr_2", th_asr_2},

	{0xff00, 0xd000, "b_1", th_b_1},
	{0xff00, 0xd100, "b_1", th_b_1},
	{0xff00, 0xd200, "b_1", th_b_1},
	{0xff00, 0xd300, "b_1", th_b_1},
	{0xff00, 0xd400, "b_1", th_b_1},
	{0xff00, 0xd500, "b_1", th_b_1},
	{0xff00, 0xd600, "b_1", th_b_1},
	{0xff00, 0xd700, "b_1", th_b_1},
	{0xff00, 0xd800, "b_1", th_b_1},
	{0xff00, 0xd900, "b_1", th_b_1},
	{0xff00, 0xda00, "b_1", th_b_1},
	{0xff00, 0xdb00, "b_1", th_b_1},
	{0xff00, 0xdc00, "b_1", th_b_1},
	{0xff00, 0xdd00, "b_1", th_b_1},
	{0xff00, 0xde00, "b_1", th_b_1},

	{0xf800, 0xe000, "b_2", th_b_2},
	{0xffc0, 0x4380, "bic", th_bic},
	{0xff00, 0xbe00, "bkpt", th_bkpt},

	{0xf800, 0xe800, "bl_blx", th_bl_blx},
	{0xf800, 0xf000, "bl_blx", th_bl_blx},
	{0xf800, 0xf800, "bl_blx", th_bl_blx},

	{0xff80, 0x4780, "blx_2", th_blx_2},
	{0xff80, 0x4700, "bx", th_bx},
	{0xffc0, 0x42c0, "cmn", th_cmn},
	{0xf800, 0x2800, "cmp_1", th_cmp_1},
	{0xffc0, 0x4280, "cmp_2", th_cmp_2},
	{0xff00, 0x4500, "cmp_3", th_cmp_3},
	{0xffc0, 0x4040, "eor", th_eor},
	{0xf800, 0xc800, "ldmia", th_ldmia_base_restored},
	{0xf800, 0x6800, "ldr_1", th_ldr_1},
	{0xfe00, 0x5800, "ldr_2", th_ldr_2},
	{0xf800, 0x4800, "ldr_3", th_ldr_3},
	{0xf800, 0x9800, "ldr_4", th_ldr_4},
	{0xf800, 0x7800, "ldrb_1", th_ldrb_1},
	{0xfe00, 0x5c00, "ldrb_2", th_ldrb_2},
	{0xf800, 0x8800, "ldrh_1", th_ldrh_1},
	{0xfe00, 0x5a00, "ldrh_2", th_ldrh_2},
	{0xfe00, 0x5600, "ldrsb", th_ldrsb},
	{0xfe00, 0x5e00, "ldrsh", th_ldrsh},
	{0xf800, 0x0000, "lsl_1", th_lsl_1},
	{0xffc0, 0x4080, "lsl_2", th_lsl_2},
	{0xf800, 0x0100, "lsr_1", th_lsr_1},
	{0xffc0, 0x40c0, "lsr_2", th_lsr_2},
	{0xf800, 0x2000, "mov_1", th_mov_1},
	//{0xffc0,0x1c00,"mov_2",th_mov_2}, /* Special version of add_1 */
	{0xff00, 0x4600, "mov_3", th_mov_3},
	{0xffc0, 0x4340, "mul", th_mul},
	{0xffc0, 0x43c0, "mvn", th_mvn},
	{0xffc0, 0x4240, "neg", th_neg},
	{0xffc0, 0x4300, "orr", th_orr},
	{0xfe00, 0xbc00, "pop", th_pop_archv5},
	{0xfe00, 0xb400, "push", th_push},
	{0xffc0, 0x41c0, "ror", th_ror},
	{0xff00, 0x4180, "sbc", th_sbc},
	{0xf800, 0xc000, "stmia", th_stmia_base_restored},
	{0xf800, 0x6000, "str_1", th_str_1},
	{0xfe00, 0x5000, "str_2", th_str_2},
	{0xf800, 0x9000, "str_3", th_str_3},
	{0xf800, 0x7000, "strb_1", th_strb_1},
	{0xfe00, 0x5400, "strb_2", th_strb_2},
	{0xf800, 0x8000, "strh_1", th_strh_1},
	{0xfe00, 0x5200, "strh_2", th_strh_2},
	{0xfe00, 0x1e00, "sub_1", th_sub_1},
	{0xf800, 0x3800, "sub_2", th_sub_2},
	{0xfe00, 0x1a00, "sub_3", th_sub_3},
	{0xff80, 0xb080, "sub_4", th_sub_4},
	{0xff00, 0xdf00, "swi", th_swi},
	{0xffc0, 0x4200, "tst", th_tst},
	{0x0, 0, "undefined", th_undefined}
};

void
ThumbDecoder_New()
{
	int icode;
	ThumbInstruction *cursor;
	if (thumbIProcTab) {
		fprintf(stderr, "Warning: Thumb Instruction decoder is already initialized\n");
		return;
	}
	thumbIProcTab = sg_calloc(sizeof(ThumbInstructionProc *) * 65536);
	thumbInstructionTab = sg_calloc(sizeof(ThumbInstruction *) * 65536);
	memset(thumbIProcTab, 0, sizeof(ThumbInstructionProc *) * 65536);
	memset(thumbInstructionTab, 0, sizeof(ThumbInstruction *) * 65536);
	for (icode = 0; icode < 65536; icode++) {
		for (cursor = &instrlist[0]; cursor->mask != 0; cursor++) {
			if ((icode & cursor->mask) == cursor->value) {
				if (thumbIProcTab[icode]) {
					fprintf(stderr, "instruction code coll %s, %s\n",
						cursor->name, thumbInstructionTab[icode]->name);
				}
				thumbIProcTab[icode] = cursor->proc;
				thumbInstructionTab[icode] = cursor;
			}
		}
		if (thumbIProcTab[icode] == NULL) {
			thumbIProcTab[icode] = th_undefined;
			thumbInstructionTab[icode] = cursor;
		}
	}
}

#ifdef TEST
int
main()
{
	ThumbInstruction *cursor;
	for (cursor = instrlist; cursor->mask; cursor++) {
		fprintf(stdout,
			"\nvoid th_%s() {\n  fprintf(stderr,\"Thumb %s not implemented\\n\");\n}\n",
			cursor->name, cursor->name);
	}
	exit(0);
}

#endif
#ifdef TEST2
int
main()
{
	ThumbInstruction *cursor;
	for (cursor = instrlist; cursor->mask; cursor++) {
		fprintf(stdout, "void th_%s();\n", cursor->name);
	}
	exit(0);
}
#endif
#ifdef TEST3
int
main()
{
	ThumbDecoder_New();
	exit(0);
}
#endif
