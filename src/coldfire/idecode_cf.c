/*
 * ----------------------------------------------------
 *
 * Coldfire Instruction decoder
 * (C) 2008 Jochen Karrer 
 *   Author: Jochen Karrer
 *
 * Status
 *      not implemented 
 *
 * ----------------------------------------------------
 */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "idecode_cf.h"
#include "instructions_cf.h"
#include "sgstring.h"

InstructionProc **iProcTab;
typedef struct IDecoder {
        Instruction *instr[0x10000];
} IDecoder;

static IDecoder *s_idec;

static Instruction instrlist[] = {
{0xf1c0,0xd080,"add",cf_add},
{0xf1c0,0xd180,"add",cf_add},
{0xf1c0,0xd1c0,"adda",cf_adda},
{0xfff8,0x0680,"addi",cf_addi},
{0xf1c0,0x5080,"addq",cf_addq},
{0xf1f8,0xd180,"addx",cf_addx},
{0xf000,0xc000,"and",cf_and},
{0xfff8,0x0280,"andi",cf_andi},
{0xf1f8,0xe080,"asri",cf_asri},
{0xf1f8,0xe0a0,"asrr",cf_asrr},
{0xf1f8,0xe180,"asl",cf_asli},
{0xf1f8,0xe1a0,"asl",cf_aslr},
{0xf000,0x6000,"bcc",cf_bcc},
{0xffc0,0x0840,"bchgi",cf_bchgi}, /* 2 Word instruction */
{0xf1c0,0x0140,"bchgr",cf_bchgi},
{0xffc0,0x0880,"bclr",cf_bclri}, /* 2 Word instruction */
{0xf1c0,0x0180,"bclr",cf_bclrr},
{0xfff8,0x00c0,"bitrev",cf_bitrev},
{0xffc0,0x08c0,"bset",cf_bseti}, /* 2 Word instruction */
{0xf1c0,0x01c0,"bset",cf_bsetr},
{0xff00,0x6100,"bsr",cf_bsr}, /* special case of bcc */
{0xffc0,0x0800,"btst",cf_btsti}, /* 2 Word instruction */
{0xf1c0,0x0100,"btst",cf_btstr},
{0xfff8,0x02c0,"byterev",cf_byterev},
{0xff00,0x4200,"clr",cf_clr},
{0xf1c0,0xb080,"cmp",cf_cmp},
{0xf1c0,0xb1c0,"cmpa",cf_cmpa},
{0xff38,0x0c00,"cmpi",cf_cmpi}, /* might be a 2 Word or 3 Word instruction */
{0xf1c0,0x81c0,"divs.w",cf_divs_w},
{0xffc0,0x4c40,"div.l",cf_div_l}, /* is a 2 word instruction, signed/unsigned is decided in 2nd word */
{0xf1c0,0x80c0,"divu.w",cf_divu_w},
{0xf1c0,0xb180,"eor",cf_eor},
{0xfff8,0x0a80,"eori",cf_eori}, /* might be a word or 3 word instruction */
{0xfff8,0x4880,"ext.w",cf_ext},
{0xfff8,0x48c0,"ext.l",cf_ext},
{0xfff8,0x49c0,"extb.l",cf_ext},
{0xfff8,0x04c0,"ff1",cf_ff1},
{0xffff,0x4afc,"illegal",cf_illegal},
{0xffc0,0x4ec0,"jmp",cf_jmp},
{0xffc0,0x4e80,"jsr",cf_jsr},
{0xf1c0,0x41c0,"lea",cf_lea},
{0xfff8,0x4e50,"link",cf_link},
{0xf1f8,0xe088,"lsri",cf_lsri},
{0xf1f8,0xe0a8,"lsrr",cf_lsrr},
{0xf1f8,0xe188,"lsli",cf_lsli},
{0xf1f8,0xe1a8,"lslr",cf_lslr},
{0xf1c0,0xa140,"mov3q",cf_mov3q},
{0xf000,0x1000,"move",cf_move},
{0xf000,0x3000,"move",cf_move},
{0xf000,0x2000,"move",cf_move},
{0xf1c0,0x3040,"movea",cf_movea},
{0xf1c0,0x2040,"movea",cf_movea},
{0xfbc0,0x48c0,"movem",cf_movem},
{0xf100,0x7000,"moveq",cf_moveq},
{0xfff8,0x42c0,"movefccr",cf_movefccr},
{0xffc0,0x44c0,"movetccr",cf_movetccr},
{0xf1c0,0xc1c0,"mulsw",cf_mulsw},
{0xffc0,0x4c00,"mull",cf_mull}, /* 2 word instruction */
{0xf1c0,0xc0c0,"muluw",cf_muluw},
{0xf180,0x7100,"mvs",cf_mvs},
{0xf180,0x7180,"mvz",cf_mvz},
{0xfff8,0x4480,"neg",cf_neg},
{0xfff8,0x4080,"negx",cf_negx},
{0xffff,0x4e71,"nop",cf_nop},
{0xfff8,0x4680,"not",cf_not},
{0xf1c0,0x8080,"or",cf_or},
{0xf1c0,0x8180,"or",cf_or},
{0xfff8,0x0080,"ori",cf_ori}, /* 3 word instruction */
{0xffc0,0x4840,"pea",cf_pea},
{0xffff,0x4acc,"pulse",cf_pulse},
{0xffff,0x4e75,"rts",cf_rts},
{0xfff8,0x4c80,"sats",cf_sats},
{0xf0f8,0x50c0,"scc",cf_scc},
{0xf1c0,0x9080,"sub",cf_sub},
{0xf1c0,0x9180,"sub",cf_sub},
{0xf1c0,0x91c0,"suba",cf_suba},
{0xfff8,0x0480,"subi",cf_subi},
{0xf1c0,0x5180,"subq",cf_subq},
{0xf1f8,0x9180,"subx",cf_subx},
{0xfff8,0x4840,"swap",cf_swap},
{0xffc0,0x4ac0,"tas",cf_tas},
{0xfff8,0x51f8,"tpf",cf_tpf},
{0xfff0,0x4e40,"trap",cf_trap},
{0xff00,0x4a00,"tst",cf_tst},
{0xfff8,0x4e58,"unlk",cf_unlk},
{0xff00,0xfb00,"wddata",cf_wddata},
#if 1
/* MAC instructions */
{0xf1b0,0xa000,"macsac",cf_macsac}, /* no difference in first word between mac and msac */
{0xf180,0xa080,"macsacl",cf_macsacl},
{0xfff0,0xa180,"movfromacc",cf_movfromacc},
{0xfff0,0xa980,"movfrommacsr",cf_movfrommacsr},
{0xfff0,0xad80,"movfrommask",cf_movfrommask},
{0xffff,0xa9c0,"movmacsrtoccr",cf_movmacsrtoccr},
{0xffc0,0xa100,"movtoacc",cf_movtoacc},
{0xffc0,0xa900,"movtomacsr",cf_movtomacsr},
{0xffc0,0xad00,"movtomask",cf_movtomask},
#endif 
/* Privileged  instructions */
{0xff38,0xf428,"cpushl",cf_cpushl},
{0xffc0,0xf340,"frestore",cf_frestore},
{0xffc0,0xf300,"fsave",cf_fsave},
{0xffff,0x4ac8,"halt",cf_halt},
{0xfff8,0xf428,"intouch",cf_intouch},
{0xfff8,0x40c0,"movefromsr",cf_movefromsr},
{0xfff8,0x4e68,"movefromusp",cf_movfromusp},
{0xffc0,0x46c0,"movetosr",cf_movetosr},
{0xfff8,0x4e60,"movetousp",cf_movetousp},
{0xffff,0x4e7b,"movec",cf_movec},
{0xffff,0x4e73,"rte",cf_rte},
{0xffff,0x40e7,"strldsr",cf_strldsr},
{0xffff,0x4e72,"stop",cf_stop},
{0xffc0,0xfbc0,"wdebug",cf_wdebug},
{0xffe0,0x2c80,"wdebug_ctrl",cf_wdebug_ctrl},

/* EMAC instructions */
//{0xf130,0xa000,"maaac",cf_maaac},
};

static Instruction instr_undefined = {0x0,0x0,"undefined",cf_undefined}; 

void
CF_IDecoderNew() {
	int i,j;
	int nr_instructions = sizeof(instrlist) / sizeof(Instruction);
	IDecoder *idec = sg_new(IDecoder);
	s_idec = idec;
	iProcTab = sg_calloc(0x10000 * sizeof(InstructionProc *));
	for(i=0;i<0x10000;i++) {
		for(j=0;j<nr_instructions;j++) {	
			Instruction *instr = &instrlist[j]; 
			if((i & instr->mask) == instr->icode) {
				if(!idec->instr[i]) {
					idec->instr[i] = instr;
					iProcTab[i] = instr->proc;
				} else {
					uint16_t mask = idec->instr[i]->mask & instr->mask;
					if(idec->instr[i]->mask == instr->mask) {
						fprintf(stderr,"Can not decide %s(%04x) %s(%04x) \n",instr->name,instr->mask,idec->instr[i]->name,idec->instr[i]->mask);
					}
					if(mask == instr->mask) {
						/* 
						 * do nothing because the already registered one
						 * is more specific
						 */
					} else if(mask == idec->instr[i]->mask) {
						idec->instr[i] = instr;
						iProcTab[i] = instr->proc;
					} else {
					fprintf(stderr,"Can not decide %s(%04x) %s(%04x) \n",instr->name,instr->mask,idec->instr[i]->name,idec->instr[i]->mask);
					}
				}
			}	
		}
		if(idec->instr[i] == NULL) {
			idec->instr[i] = &instr_undefined;
			iProcTab[i] = cf_undefined; 
		}
	}	
	fprintf(stderr,"Coldfire Instruction decoder created\n");
}

Instruction *
CF_InstructionFind(uint16_t icode)
{
	return s_idec->instr[icode];
}

