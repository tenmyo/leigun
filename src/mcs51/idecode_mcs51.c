/*
 * Used atmel doc0509.pdf for instruction set
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "idecode_mcs51.h"
#include "instructions_mcs51.h"
#include "sgstring.h"

MCS51_InstructionProc **mcs51_iProcTab = NULL;
MCS51_Instruction **mcs51_instrTab = NULL;

static MCS51_Instruction instrlist[] = {
        {0x28,0xf8,"mcs51_add",mcs51_add,1,1},
	{0x25,0xff,"mcs51_adddir",mcs51_adddir,2,1},
	{0x26,0xfe,"mcs51_addari",mcs51_addari,1,1},
	{0x24,0xff,"mcs51_addadata",mcs51_addadata,2,1},
	{0x38,0xf8,"mcs51_addc",mcs51_addc,1,1},
	{0x35,0xff,"mcs51_addcdir",mcs51_addcdir,2,1},
	{0x36,0xfe,"mcs51_addcari",mcs51_addcari,1,1},
	{0x34,0xff,"mcs51_addcadata",mcs51_addcadata,2,1},
	{0x58,0xf8,"mcs51_anlrn",mcs51_anlrn,1,1},	
	{0x55,0xff,"mcs51_anldir",mcs51_anldir,2,1},
	{0x56,0xfe,"mcs51_anlari",mcs51_anlari,1,1},
	{0x54,0xff,"mcs51_anladata",mcs51_anladata,2,1},
	{0x52,0xff,"mcs51_anldira",mcs51_anldira,2,1},
	{0x53,0xff,"mcs51_anldirdata",mcs51_anldirdata,3,2},
	{0x82,0xff,"mcs51_anlcbit",mcs51_anlcbit,2,2},
	{0xb0,0xff,"mcs51_anlcnbit",mcs51_anlcnbit,2,2},
	{0xb5,0xff,"mcs51_cjneadirrel",mcs51_cjneadirrel,3,2},
	{0xb4,0xff,"mcs51_cjneadatarel",mcs51_cjneadataresl,3,2},
	{0xb8,0xf8,"mcs51_cjnerdatarel",mcs51_cjnerdatarel,3,2},
	{0xb6,0xfe,"mcs51_cjneardatarel",mcs51_cjneardatarel,3,2},
	{0xe4,0xff,"mcs51_clra",mcs51_clra,1,1},	
	{0xc3,0xff,"mcs51_clrc",mcs51_clrc,1,1},
	{0xc2,0xff,"mcs51_clrbit",mcs51_clrbit,2,1},
	{0xf4,0xff,"mcs51_cpla",mcs51_cpla,1,1},
	{0xb3,0xff,"mcs51_cplc",mcs51_cplc,1,1},
	{0xb2,0xff,"mcs51_cplbit",mcs51_cplbit,2,1},
	{0xd4,0xff,"mcs51_da",mcs51_da,1,1},	
	{0x14,0xff,"mcs51_deca",mcs51_deca,1,1},
	{0x18,0xf8,"mcs51_decr",mcs51_decr,1,1},
	{0x15,0xff,"mcs51_decdir",mcs51_decdir,2,1},
	{0x16,0xfe,"mcs51_decari",mcs51_decari,1,1},
	{0x84,0xff,"mcs51_divab",mcs51_divab,1,4},
	{0xd8,0xf8,"mcs51_djnzrrel",mcs51_djnzrrel,2,2},	
	{0xb5,0xff,"mcs51_djnzdirrel",mcs51_djnzdirrel,3,2},
	{0x04,0xff,"mcs51_inca",mcs51_inca,1,1},
	{0x08,0xf8,"mcs51_incr",mcs51_incr,1,1},
	{0x05,0xff,"mcs51_incdir",mcs51_incdir,2,1},
	{0x06,0xfe,"mcs51_incari",mcs51_incari,1,1},	
	{0xa3,0xff,"mcs51_incdptr",mcs51_incdptr,1,2},
	{0x20,0xff,"mcs51_jbbitrel",mcs51_jbbitrel,3,2},
	{0x10,0xff,"mcs51_jbcbitrel",mcs51_jbcbitrel,3,2},
	{0x40,0xff,"mcs51_jcrel",mcs51_jcrel,2,2},
	{0x73,0xff,"mcs51_jmpaadptr",mcs51_jmpaadptr,1,2},
	{0x30,0xff,"mcs51_jnbbitrel",mcs51_jnbbitrel,3,2},
	{0x50,0xff,"mcs51_jncrel",mcs51_jncrel,2,2},
	{0x70,0xff,"mcs51_jnzrel",mcs51_jnzrel,2,2},
	{0x60,0xff,"mcs51_jzrel",mcs51_jzrel,2,2},
	{0x12,0xff,"mcs51_lcall",mcs51_lcall,3,2},
	{0x02,0xff,"mcs51_ljmp",mcs51_ljmp,3,2},
	{0xe8,0xf8,"mcs51_movarn",mcs51_movarn,1,1},	
	{0xe5,0xff,"mcs51_movadir",mcs51_movadir,2,1},
	{0xe6,0xfe,"mcs51_movaari",mcs51_movaari,1,1},
	{0x74,0xff,"mcs51_movadata",mcs51_movadata,2,1},
	{0xf8,0xf8,"mcs51_movra",mcs51_movra,1,1},
	{0xa8,0xf8,"mcs51_movrdir",mcs51_movrdir,2,2},
	{0x78,0xf8,"mcs51_movrdata",mcs51_movrdata,2,1},
	{0xf5,0xff,"mcs51_movdira",mcs51_movdira,2,1},
	{0x88,0xf8,"mcs51_movdirr",mcs51_movdirr,2,2},
	{0x85,0xff,"mcs51_movdirdir",mcs51_movdirdir,3,2},
	{0x86,0xfe,"mcs51_movdirari",mcs51_movdirari,2,2},
	{0x75,0xff,"mcs51_movdirdata",mcs51_movdirdata,3,2},
	{0xf6,0xfe,"mcs51_movaria",mcs51_movaria,1,1},
	{0xa6,0xfe,"mcs51_movaridir",mcs51_movaridir,2,2},
	{0x76,0xfe,"mcs51_movaridata",mcs51_movaridata,2,1},
	{0xa2,0xff,"mcs51_movcbit",mcs51_movcbit,2,1},
	{0x92,0xff,"mcs51_movbitc",mcs51_movbitc,2,2},
	{0x90,0xff,"mcs51_movdptrdata",mcs51_movdptrdata,3,2},
	{0x93,0xff,"mcs51_movcaadptr",mcs51_movcaadptr,1,2},
	{0x83,0xff,"mcs51_movaapc",mcs51_movaapc,1,2},
	{0xe2,0xfe,"mcs51_movxaari",mcs51_movxaari,1,2},
	{0xe0,0xff,"mcs51_movxaadptr",mcs51_movxaadptr,1,2},
	{0xf2,0xfe,"mcs51_movxara",mcs51_movxara,1,2},
	{0xf0,0xff,"mcs51_movxadptra",mcs51_movxadptra,1,2},
	{0xa4,0xff,"mcs51_mulab",mcs51_mulab,1,4},
	{0x00,0xff,"mcs51_nop",mcs51_nop,1,1},
	{0x48,0xf8,"mcs51_orlar",mcs51_orlar,1,1},
	{0x45,0xff,"mcs51_orladir",mcs51_orladir,2,1},
	{0x46,0xfe,"mcs51_orlaari",mcs51_orlaari,1,1},
	{0x44,0xff,"mcs51_orladata",mcs51_orladata,2,1},
	{0x42,0xff,"mcs51_orldira",mcs51_orldira,2,1},
	{0x43,0xff,"mcs51_orldirdata",mcs51_orldirdata,3,2},
	{0x72,0xff,"mcs51_orlcbit",mcs51_orlcbit,2,2},
	{0xa0,0xff,"mcs51_orlcnbit",mcs51_orlcnbit,2,2},
	{0xb0,0xff,"mcs51_popdir",mcs51_popdir,2,2},
	{0xc0,0xff,"mcs51_pusdir",mcs51_pushdir,2,2},
	{0x22,0xff,"mcs51_ret",mcs51_ret,1,2},
	{0x32,0xff,"mcs51_reti",mcs51_reti,1,2},
	{0x23,0xff,"mcs51_rla",mcs51_rla,1,1},
	{0x33,0xff,"mcs51_rlca",mcs51_rlca,1,1},
	{0x03,0xff,"mcs51_rra",mcs51_rra,1,1},
	{0x13,0xff,"mcs51_rrca",mcs51_rrca,1,1},
	{0xd3,0xff,"mcs51_setbc",mcs51_setbc,1,1},
	{0xd2,0xff,"mcs51_setbbit",mcs51_setbbit,2,1},
	{0x80,0xff,"mcs51_sjmprel",mcs51_sjmprel,2,2},
	{0x98,0xf8,"mcs51_subbar",mcs51_subbar,1,1},
	{0x95,0xff,"mcs51_subbadir",mcs51_subbadir,2,1},
	{0x96,0xfe,"mcs51_subbaari",mcs51_subbaari,1,1},
	{0x94,0xff,"mcs51_subbadata",mcs51_subbadata,2,1},
	{0xc4,0xff,"mcs51_swapa",mcs51_swapa,1,1},
	{0xc8,0xf8,"mcs51_xchar",mcs51_xchar,1,1},
	{0xc5,0xff,"mcs51_xchadir",mcs51_xchadir,2,1},
	{0xc6,0xfe,"mcs51_xchaari",mcs51_xchaari,1,1},
	{0xd6,0xfe,"mcs51_xchdaari",mcs51_xchdaari,1,1},
	{0x68,0xf8,"mcs51_xrlar",mcs51_xrlar,1,1}
};

void
MCS51_IDecoderNew()
{
        uint32_t icode;
        int j;
        int num_instr = sizeof(instrlist) / sizeof(MCS51_Instruction);
        mcs51_iProcTab = sg_calloc(sizeof(MCS51_InstructionProc *) * 0x100);
        mcs51_instrTab =  sg_calloc(sizeof(MCS51_Instruction *) * 0x100);
        for(icode=0;icode<256;icode++) {
                for(j=num_instr-1;j>=0;j--) {
                        MCS51_Instruction *instr = &instrlist[j];
                        if((icode & instr->mask) == instr->opcode) {
                                if(mcs51_iProcTab[icode]) {
                                        fprintf(stdout,"conflict at %04x %s\n",icode,instr->name);
                                } else {
					//fprintf(stderr,"icode %d: %s\n",icode,instr->name);	
                                        mcs51_iProcTab[icode] = instr->iproc;
                                        mcs51_instrTab[icode] = instr;
                                }
                        }
                }
                if(mcs51_iProcTab[icode] == NULL) {
                        mcs51_iProcTab[icode] =  mcs51_undef;
                }
        }
        fprintf(stderr,"MCS51 instruction decoder with %d Instructions created\n",num_instr);
}

