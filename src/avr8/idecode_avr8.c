/*
 *************************************************************************************************
 *
 * Atmel AVR8 instruction decoder 
 *
 * State: working 
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

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "instructions_avr8.h"
#include "idecode_avr8.h"
#include "sgstring.h"

AVR8_InstructionProc **avr8_iProcTab = NULL;
AVR8_Instruction **avr8_instrTab = NULL;

/*
 **********************************************************
 * Instruction List with opcodes and masks
 * v1
 **********************************************************
 */
static AVR8_Instruction instrlist[] = {
	{0x1c00,0xfc00,"avr8_adc",avr8_adc,1,1},
	{0x0c00,0xfc00,"avr8_add",avr8_add,1,1},
	{0x9600,0xff00,"avr8_adiw",avr8_adiw,2,1},
	{0x2000,0xfc00,"avr8_and",avr8_and,1,1},
	{0x7000,0xf000,"avr8_andi",avr8_andi,1,1},
	{0x9405,0xfe0f,"avr8_asr",avr8_asr,1,1},
	{0x9488,0xff8f,"avr8_bclr",avr8_bclr,1,1},
	{0xf800,0xfe08,"avr8_bld",avr8_bld,1,1},

	{0xf400,0xfc00,"avr8_brbc",avr8_brbc,1,1},
	{0xf000,0xfc00,"avr8_brbs",avr8_brbs,1,1},

	{0x9598,0xffff,"avr8_break",avr8_break,1,1},


	{0x9408,0xff8f,"avr8_bset",avr8_bset,1,1},
	{0xfa00,0xfe08,"avr8_bst",avr8_bst,1,1},
	{0x940e,0xfe0e,"avr8_call",avr8_call,4,2},
	{0x9800,0xff00,"avr8_cbi",avr8_cbi,2,1},
	{0x9400,0xfe0f,"avr8_com",avr8_com,1,1},
	{0x1400,0xfc00,"avr8_cp",avr8_cp,1,1},
	{0x0400,0xfc00,"avr8_cpc",avr8_cpc,1,1},
	{0x3000,0xf000,"avr8_cpi",avr8_cpi,1,1},	
	{0x1000,0xfc00,"avr8_cpse",avr8_cpse,1,1},
	{0x940a,0xfe0f,"avr8_dec",avr8_dec,1,1},
	{0x9519,0xffff,"avr8_eicall",avr8_eicall,4,1},
	{0x9419,0xffff,"avr8_eijmp",avr8_eijmp,2,1},
	{0x95d8,0xffff,"avr8_elpm1",avr8_elpm1,3,1},
	{0x9006,0xfe0f,"avr8_elpm2",avr8_elpm2,3,1},
	{0x9007,0xfe0f,"avr8_elpm3",avr8_elpm3,3,1},
	{0x2400,0xfc00,"avr8_eor",avr8_eor,1,1},	
	{0x0308,0xff88,"avr8_fmul",avr8_fmul,2,1},
	{0x0380,0xff88,"avr8_fmuls",avr8_fmuls,2,1},
	{0x0388,0xff88,"avr8_fmulsu",avr8_fmulsu,2,1},
	{0x9509,0xffff,"avr8_icall",avr8_icall,3,1},	
	{0x9409,0xffff,"avr8_ijmp",avr8_ijmp,2,1},
	{0xb000,0xf800,"avr8_in",avr8_in,1,1},
	{0x9403,0xfe0f,"avr8_inc",avr8_inc,1,1},
	{0x940c,0xfe0e,"avr8_jmp",avr8_jmp,3,2},

	{0x900c,0xfe0f,"avr8_ld1",avr8_ld1,2,1},	
	{0x900d,0xfe0f,"avr8_ld2",avr8_ld2,2,1},	
	{0x900e,0xfe0f,"avr8_ld3",avr8_ld3,2,1},
	{0x9009,0xfe0f,"avr8_ldy2",avr8_ldy2,2,1},
	{0x900a,0xfe0f,"avr8_ldy3",avr8_ldy3,2,1},
	{0x8008,0xd208,"avr8_ldy4",avr8_ldy4,2,1},
	{0x9001,0xfe0f,"avr8_ldz2",avr8_ldz2,2,1},
	{0x9002,0xfe0f,"avr8_ldz3",avr8_ldz3,2,1},
	{0x8000,0xd208,"avr8_ldz4",avr8_ldz4,2,1},
	{0xe000,0xf000,"avr8_ldi",avr8_ldi,1,1},
	{0x9000,0xfe0f,"avr8_lds",avr8_lds,2,2},

	{0x95c8,0xffff,"avr8_lpm1",avr8_lpm1,3,1},
	{0x9004,0xfe0f,"avr8_lpm2",avr8_lpm2,3,1},
	{0x9005,0xfe0f,"avr8_lpm3",avr8_lpm3,3,1},
	{0x9406,0xfe0f,"avr8_lsr",avr8_lsr,1,1},
	{0x2c00,0xfc00,"avr8_mov",avr8_mov,1,1},
	{0x0100,0xff00,"avr8_movw",avr8_movw,1,1},	
	{0x9c00,0xfc00,"avr8_mul",avr8_mul,2,1},
	{0x0200,0xff00,"avr8_muls",avr8_muls,2,1},
	{0x0300,0xff88,"avr8_mulsu",avr8_mulsu,2,1},
	{0x9401,0xfe0f,"avr8_neg",avr8_neg,1,1},
	{0x0000,0xffff,"avr8_nop",avr8_nop,1,1},
	{0x2800,0xfc00,"avr8_or",avr8_or,1,1},
	{0x6000,0xf000,"avr8_ori",avr8_ori,1,1},
	{0xb800,0xf800,"avr8_out",avr8_out,1,1},
	{0x900f,0xfe0f,"avr8_pop",avr8_pop,2,1},
	{0x920f,0xfe0f,"avr8_push",avr8_push,2,1},
	{0xd000,0xf000,"avr8_rcall",avr8_rcall,3,1},
	{0x9508,0xffff,"avr8_ret",avr8_ret,4,1},
	{0x9518,0xffff,"avr8_reti",avr8_reti,4,1},
	{0xc000,0xf000,"avr8_rjmp",avr8_rjmp,2,1},
	{0x9407,0xfe0f,"avr8_ror",avr8_ror,1,1},
	{0x0800,0xfc00,"avr8_sbc",avr8_sbc,1,1},
	{0x4000,0xf000,"avr8_sbci",avr8_sbci,1,1},	
	{0x9a00,0xff00,"avr8_sbi",avr8_sbi,2,1},
	{0x9900,0xff00,"avr8_sbic",avr8_sbic,1,1},
	{0x9b00,0xff00,"avr8_sbis",avr8_sbis,1,1},
	{0x9700,0xff00,"avr8_sbiw",avr8_sbiw,2,1},	
	{0xfc00,0xfe08,"avr8_sbrc",avr8_sbrc,1,1},
	{0xfe00,0xfe08,"avr8_sbrs",avr8_sbrs,1,1},
	{0x9588,0xffff,"avr8_sleep",avr8_sleep,1,1},
	{0x95e8,0xffff,"avr8_spm",avr8_spm,1,1},
	{0x920c,0xfe0f,"avr8_st1",avr8_st1,2,1},
	{0x920d,0xfe0f,"avr8_st2",avr8_st2,2,1},
	{0x920e,0xfe0f,"avr8_st3",avr8_st3,2,1},
	{0x9209,0xfe0f,"avr8_sty2",avr8_sty2,2,1},
	{0x920a,0xfe0f,"avr8_sty3",avr8_sty3,2,1},
	{0x8208,0xd208,"avr8_sty4",avr8_sty4,2,1},
	{0x9201,0xfe0f,"avr8_stz2",avr8_stz2,2,1},
	{0x9202,0xfe0f,"avr8_stz3",avr8_stz3,2,1},
	{0x8200,0xd208,"avr8_stz4",avr8_stz4,2,1},
	{0x9200,0xfe0f,"avr8_sts",avr8_sts,2,2},
	{0x1800,0xfc00,"avr8_sub",avr8_sub,1,1},
	{0x5000,0xf000,"avr8_subi",avr8_subi,1,1},
	{0x9402,0xfe0f,"avr8_swap",avr8_swap,1,1},
	{0x95a8,0xffff,"avr8_wdr",avr8_wdr,1,1},
};

void
AVR8_IDecoderNew() 
{
	uint32_t icode;
	int j;
	int num_instr = sizeof(instrlist) / sizeof(AVR8_Instruction);
	avr8_iProcTab = sg_calloc(sizeof(AVR8_InstructionProc *) * 0x10000);
	avr8_instrTab =  sg_calloc(sizeof(AVR8_Instruction *) * 0x10000);
	for(icode=0;icode<65536;icode++) {
		for(j=num_instr-1;j>=0;j--) {
			AVR8_Instruction *instr = &instrlist[j];
			if((icode & instr->mask) == instr->opcode) {
				if(avr8_iProcTab[icode]) {
					fprintf(stdout,"conflict at %04x %s\n",icode,instr->name);
				} else {
					avr8_iProcTab[icode] = instr->iproc;
					avr8_instrTab[icode] = instr;
				}
			}
		}
		if(avr8_iProcTab[icode] == NULL) {
			avr8_iProcTab[icode] = 	avr8_undef;
		}
	}
	fprintf(stderr,"AVR8 instruction decoder with %d Instructions created\n",num_instr);
}
#ifdef TEST
int
main() {
	int i;
	AVR8_IDecoderNew();
	int num_instr = sizeof(instrlist) / sizeof(AVR8_Instruction);
	for(i=0;i<num_instr;i++) {
		AVR8_Instruction *instr = &instrlist[i];
		fprintf(stdout,"void\n%s(void) {\n\tfprintf(stderr,\"%s not implemented\\n\");\n}\n\n",instr->name,instr->name);
	}

}
#endif
