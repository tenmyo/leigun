#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "instructions_sh4.h"
#include "idecode_sh4.h"
#include "sgstring.h"

SH4_InstructionProc **sh4_iProcTab = NULL;
SH4_Instruction **sh4_instrTab = NULL;

/**
 *****************************************************************
 * \var SH4_Instruction instrlist[]
 * v1
 *****************************************************************
 */
static SH4_Instruction instrlist[] = {
{0xf00f,0x300c,"add",sh4_add},
{0xf000,0x7000,"add_imm",sh4_add_imm},
{0xf00f,0x300e,"addc",sh4_addc},
{0xf00f,0x300f,"addv",sh4_addv},
{0xf00f,0x2009,"and",sh4_and},
{0xff00,0xc900,"and_imm",sh4_and_imm},
{0xff00,0xcd00,"and.b",sh4_andb},
{0xff00,0x8b00,"bf",sh4_bf,	SH4INST_ILLSLOT},
{0xff00,0x8f00,"bf_s",sh4_bfs,	SH4INST_ILLSLOT},
{0xf000,0xa000,"bra",sh4_bra,	SH4INST_ILLSLOT},
{0xf0ff,0x0023,"braf",sh4_braf, SH4INST_ILLSLOT},
{0xffff,0x003b,"brk",sh4_brk },
{0xf000,0xb000,"bsr",sh4_bsr,	SH4INST_ILLSLOT},
{0xf0ff,0x0003,"bsrf",sh4_bsrf, SH4INST_ILLSLOT},
{0xff00,0x8900,"bt",sh4_bt,	SH4INST_ILLSLOT},
{0xff00,0x8d00,"bts",sh4_bts,	SH4INST_ILLSLOT},
{0xffff,0x0028,"clrmac",sh4_clrmac},
{0xffff,0x0048,"clrs",sh4_clrs},
{0xffff,0x0008,"clrt",sh4_clrt},
{0xf00f,0x3000,"cpmeq",sh4_cmpeq},
{0xf00f,0x3003,"cmpge",sh4_cmpge},
{0xf00f,0x3007,"cmpgt",sh4_cmpgt},
{0xf00f,0x3006,"cmphi",sh4_cmphi},
{0xf00f,0x3002,"cmphs",sh4_cmphs},
{0xf0ff,0x4015,"cmppl",sh4_cmppl},
{0xf0ff,0x4011,"cmppz",sh4_cmppz},
{0xf00f,0x200c,"cmpstr",sh4_cmpstr},
{0xff00,0x8800,"cmpeq_imm_r0",sh4_cmpeq_imm_r0},
{0xf00f,0x2007,"div0.s",sh4_div0s},
{0xffff,0x0019,"div0.u",sh4_div0u},
{0xf00f,0x3004,"div1",sh4_div1},
{0xf00f,0x300d,"dmulsl",sh4_dmulsl},
{0xf00f,0x3005,"dmulul",sh4_dmulul},
{0xf0ff,0x4010,"dt",sh4_dt},
{0xf00f,0x600e,"exts.b",sh4_extsb},
{0xf00f,0x600f,"exts.w",sh4_extsw},
{0xf00f,0x600c,"extu.b",sh4_extub},
{0xf00f,0x600d,"extu.w",sh4_extuw},
{0xf0ff,0xf05d,"fabs_frdr",sh4_fabs_frdr,SH4INST_SLOTFPUDIS},
{0xf00f,0xf000,"fadd_frdr",sh4_fadd_frdr},
{0xf00f,0xf004,"fcmpeq_frdr",sh4_fcmpeq_frdr},
{0xf00f,0xf005,"fcmpgt_frdr",sh4_fcmpgt_frdr},
{0xf1ff,0xf0bd,"fcnvds",sh4_fcnvds},
{0xf1ff,0xf0ad,"fcnvsd",sh4_fcnvsd},
{0xf00f,0xf003,"fdiv_frdr",sh4_fdiv_frdr},
{0xf0ff,0xf0ed,"fipr",sh4_fipr},
{0xf0ff,0xf08d,"flidi0",sh4_fldi0},
{0xf0ff,0xf09d,"fldi1",sh4_fldi1},
{0xf0ff,0xf01d,"flds",sh4_flds},
{0xf0ff,0xf02d,"float_fpul_frdr",sh4_float_fpul_frdr},
{0xf00f,0xf00e,"fmac",sh4_fmac},
{0xf00f,0xf00c,"fmov_frdr",sh4_fmov_frdr},

/* FMOVe extension integrated into the following opcodes */
{0xf00f,0xf00a,"fmov.s_frdrarn",sh4_fmovs_frdrarn},
{0xf00f,0xf008,"fmov.s_armfrdr",sh4_fmovs_armfrdr},
{0xf00f,0xf009,"fmov.s_armpfrdr",sh4_fmovs_armpfrdr},
{0xf00f,0xf00b,"fmov.s_frdramrn",sh4_fmov_frdramrn},
{0xf00f,0xf006,"fmov.s_r0rmfrdr",sh4_fmovs_ar0rmfrdr},
{0xf00f,0xf007,"fmov.s_frdrr0rn",sh4_fmovs_frdrar0rn},

{0xf00f,0xf002,"fmul_frdr",sh4_fmul_frdr},
{0xf0ff,0xf04d,"fneg_frdr",sh4_fneg_frdr},
{0xffff,0xfbfd,"frchg",sh4_frchg},
{0xffff,0xf3fd,"fschg",sh4_fschg},
{0xf0ff,0xf06d,"fsqrt_frdr",sh4_fsqrt_frdr},
{0xf0ff,0xf00d,"fsts",sh4_fsts},
{0xf00f,0xf001,"fsub_frdr",sh4_fsub_frdr},
{0xf0ff,0xf03d,"ftrc_frdr",sh4_ftrc_frdr},
{0xf3ff,0xf1fd,"ftrv",sh4_ftrv},
{0xf0ff,0x402b,"jmp",sh4_jmp,	SH4INST_ILLSLOT},
{0xf0ff,0x400b,"jsr",sh4_jsr,	SH4INST_ILLSLOT},
{0xf0ff,0x400e,"ldc_rmsr",sh4_ldc_rmsr},
{0xf0ff,0x401e,"ldc_rmgbr",sh4_ldc_rmgbr},
{0xf0ff,0x402e,"ldc_rmvbr",sh4_ldc_rmvbr},
{0xf0ff,0x403e,"ldc_rmssr",sh4_ldc_rmssr},
{0xf0ff,0x404e,"ldc_rmspc",sh4_ldc_rmspc},
{0xf0ff,0x40fa,"ldc_rmdbr",sh4_ldc_rmdbr},
{0xf08f,0x408e,"ldc_rmrb",sh4_ldc_rmrb},
{0xf0ff,0x4007,"ldc.l_atrmpsr",sh4_ldcl_atrmpsr},
{0xf0ff,0x4017,"ldc.l_atrmpgbr",sh4_ldcl_atrmpgbr},
{0xf0ff,0x4027,"ldc.l_atrmpvbr",sh4_ldcl_atrmpvbr},
{0xf0ff,0x4037,"ldc.l_atrmpssr",sh4_ldcl_atrmpssr},
{0xf0ff,0x4047,"ldc.l_atrmpspc",sh4_ldcl_atrmpspc},
{0xf0ff,0x40f6,"ldc.l_atrmpdbr",sh4_ldcl_atrmpdbr},
{0xf08f,0x4087,"ldc.l_atrmprb",sh4_ldcl_atrmprb},
{0xf0ff,0x405a,"lds_rmfpul",sh4_lds_rmfpul},
{0xf0ff,0x4056,"lds.l_armpfpul",sh4_ldsl_armpfpul},
{0xf0ff,0x406a,"lds_rmfpscr",sh4_lds_rmfpscr},
{0xf0ff,0x4066,"lds_armpfpscr",sh4_lds_armpfpscr},
{0xf0ff,0x400a,"lds_rmmach",sh4_lds_rmmach},
{0xf0ff,0x401a,"lds_rmmacl",sh4_lds_rmmacl},
{0xf0ff,0x402a,"lds_rmpr",sh4_lds_rmpr},
{0xf0ff,0x4006,"lds_atrmpmach",sh4_lds_atrmpmach},
{0xf0ff,0x4016,"lds_atrmpmacl",sh4_lds_atrmpmacl},
{0xf0ff,0x4026,"lds_atrmppr",sh4_lds_atrmppr},
{0xffff,0x0038,"ldtlb",sh4_ldtlb},
{0xf00f,0x000f,"mac.l",sh4_macl},
{0xf00f,0x400f,"mac.w",sh4_macw},
{0xf00f,0x6003,"mov_rmrn",sh4_mov_rmrn},
{0xf00f,0x2000,"mov.b_rmarn",sh4_movb_rmarn},
{0xf00f,0x2001,"mov.w_rmarn",sh4_movw_rmarn},
{0xf00f,0x2002,"mov.l_rmarn",sh4_movl_rmarn},
{0xf00f,0x6000,"mov.b_armrn",sh4_movb_armrn},
{0xf00f,0x6001,"mov.w_armrn",sh4_movw_armrn},
{0xf00f,0x6002,"mov.l_armrn",sh4_movl_armrn},
{0xf00f,0x2004,"mov.b_rmamrn",sh4_movb_rmamrn},
{0xf00f,0x2005,"mov.w_rmamrn",sh4_movw_rmamrn},
{0xf00f,0x2006,"mov.l_rmamrn",sh4_movl_rmamrn},
{0xf00f,0x6004,"mov.b_armprn",sh4_movb_armprn},
{0xf00f,0x6005,"mov.w_armprn",sh4_movw_armprn},
{0xf00f,0x6006,"mov.l_armprn",sh4_movl_armprn},
{0xf00f,0x0004,"mov.b_rmar0rn",sh4_movb_rmar0rn},
{0xf00f,0x0005,"mov.w_rmar0rn",sh4_movw_rmar0rn},
{0xf00f,0x0006,"mov.l_rmar0rn",sh4_movl_rmar0rn},
{0xf00f,0x000c,"mov.b_ar0rmrn",sh4_movb_ar0rmrn},
{0xf00f,0x000d,"mov.w_ar0rmrn",sh4_movw_ar0rmrn},
{0xf00f,0x000e,"mov.l_ar0rmrn",sh4_movl_ar0rmrn},
{0xf000,0xe000,"mov_immrn",sh4_mov_immrn},
{0xf000,0x9000,"mov.w_adisppcrn",sh4_movw_adisppcrn, SH4INST_ILLSLOT},
{0xf000,0xd000,"mov.l_adisppcrn",sh4_movl_adisppcrn, SH4INST_ILLSLOT},
{0xff00,0xc400,"mov.b_adispgbrr0",sh4_movb_adispgbrr0},
{0xff00,0xc500,"mov.w_adispgbrr0",sh4_movw_adispgbrr0},
{0xff00,0xc600,"mov.l_adispgbrr0",sh4_movl_adispgbrr0},
{0xff00,0xc000,"mov.b_r0adispgbr",sh4_movb_r0adispgbr},
{0xff00,0xc100,"mov.w_r0adispgbr",sh4_movw_r0adispgbr},
{0xff00,0xc200,"mov.l_r0adsipgbr",sh4_movl_r0adispgbr},
{0xff00,0x8000,"mov.b_r0adisprn",sh4_movb_r0adisprn},
{0xff00,0x8100,"mov.w_r0adisprn",sh4_movw_r0adisprn},
{0xf000,0x1000,"mov.l_rmadsiprn",sh4_movl_rmadisprn},
{0xff00,0x8400,"mov.b_adsiprmr0",sh4_movb_adisprmr0},
{0xff00,0x8500,"mov.w_adisprmr0",sh4_movw_adisprmr0},
{0xf000,0x5000,"mov.l_adisprmrn",sh4_movl_adisprmrn},
{0xff00,0xc700,"mova_adisppcr0",sh4_mova_adisppcr0, SH4INST_ILLSLOT},
{0xf0ff,0x00c3,"movca.l",sh4_movcal},
{0xf0ff,0x0029,"movt",sh4_movt},
{0xf00f,0x0007,"mul.l",sh4_mull},
{0xf00f,0x200f,"muls.w",sh4_mulsw},
{0xf00f,0x200e,"mulu.w",sh4_muluw},
{0xf00f,0x600b,"neg",sh4_neg},
{0xf00f,0x600a,"negc",sh4_negc},
{0xffff,0x0009,"nop",sh4_nop},
{0xf00f,0x6007,"not",sh4_not},
{0xf0ff,0x0093,"ocbi",sh4_ocbi},
{0xf0ff,0x00a3,"ocbp",sh4_ocbp},
{0xf0ff,0x00b3,"ocbwb",sh4_ocbwb},
{0xf00f,0x200b,"or_rmrn",sh4_or_rmrn},
{0xff00,0xcb00,"or_immr0",sh4_or_immr0},
{0xff00,0xcf00,"or_immar0gbr",sh4_or_immar0gbr},
{0xf0ff,0x0083,"pref",sh4_pref},
{0xf0ff,0x4024,"rotcl",sh4_rotcl},
{0xf0ff,0x4025,"rotcr",sh4_rotcr},
{0xf0ff,0x4004,"rotl",sh4_rotl},
{0xf0ff,0x4005,"rtor",sh4_rotr},
{0xffff,0x002b,"rte",sh4_rte,	SH4INST_ILLSLOT},
{0xffff,0x000b,"rts",sh4_rts,	SH4INST_ILLSLOT},
{0xffff,0x0058,"sets",sh4_sets},
{0xffff,0x0018,"sett",sh4_sett},
{0xf00f,0x400c,"shad",sh4_shad},
{0xf0ff,0x4020,"shal",sh4_shal},
{0xf0ff,0x4021,"shar",sh4_shar},
{0xf00f,0x400d,"shld",sh4_shld},
{0xf0ff,0x4000,"shll",sh4_shll},
{0xf0ff,0x4008,"shll2",sh4_shll2},
{0xf0ff,0x4018,"shll8",sh4_shll8},
{0xf0ff,0x4028,"shll16",sh4_shll16},
{0xf0ff,0x4001,"shlr",sh4_shlr},
{0xf0ff,0x4009,"shlr2",sh4_shlr2},
{0xf0ff,0x4019,"shlr8",sh4_shlr8},
{0xf0ff,0x4029,"shlr16",sh4_shlr16},
{0xffff,0x001b,"sleep",sh4_sleep},
{0xf0ff,0x0002,"stc_sr_rn",sh4_stc_sr_rn},
{0xf0ff,0x0012,"stc_gbr_rn",sh4_stc_gbr_rn},
{0xf0ff,0x0022,"stc_vbr_rn",sh4_stc_vbr_rn},
{0xf0ff,0x0032,"stc_ssr_rn",sh4_stc_ssr_rn},
{0xf0ff,0x0042,"stc_spc_rn",sh4_stc_spc_rn},
{0xf0ff,0x003a,"stc_sgr_rn",sh4_stc_sgr_rn},
{0xf0ff,0x00fa,"stc_dbr_rn",sh4_stc_dbr_rn},
{0xf08f,0x0082,"stc_rb_rn",sh4_stc_rb_rn},
{0xf0ff,0x4003,"stc.l_sr_amrn",sh4_stcl_sr_amrn},
{0xf0ff,0x4013,"stc.l_gbr_amrn",sh4_stcl_gbr_amrn},
{0xf0ff,0x4023,"stc.l_vbr_amrn",sh4_stcl_vbr_amrn},
{0xf0ff,0x4033,"stc.l_ssr_amrn",sh4_stcl_ssr_amrn},
{0xf0ff,0x4043,"stc.l_spc_amrn",sh4_stcl_spc_amrn},
{0xf0ff,0x4032,"stc.l_sgr_amrn",sh4_stcl_sgr_amrn},
{0xf0ff,0x40f2,"stc.l_dbr_amrn",sh4_stcl_dbr_amrn},
{0xf08f,0x4083,"stc.l_rb_amrn",sh4_stcl_rb_amrn},
{0xf0ff,0x000a,"sts_mach_rn",sh4_sts_mach_rn},
{0xf0ff,0x001a,"sts_macl_rn",sh4_sts_macl_rn},
{0xf0ff,0x002a,"sts_pr_rn",sh4_sts_pr_rn},
{0xf0ff,0x4002,"sts.l_mach_amrn",sh4_stsl_mach_amrn},
{0xf0ff,0x4012,"sts.l_macl_amrn",sh4_stsl_macl_amrn},
{0xf0ff,0x4022,"sts.l_pr_amrn",sh4_stsl_pr_amrn},
{0xf0ff,0x005a,"sts_fpul_rn",sh4_sts_fpul_rn},
{0xf0ff,0x006a,"sts_fpscr_rn",sh4_fpscr_rn},
{0xf0ff,0x4052,"sts_fpul_amrn",sh4_sts_fpul_amrn},
{0xf0ff,0x4062,"sts_fpscr_amrn",sh4_sts_fpscr_amrn},
{0xf00f,0x3008,"sub_rm_rn",sh4_sub_rm_rn},
{0xf00f,0x300a,"subc_rm_rn",sh4_subc_rm_rn},
{0xf00f,0x300b,"subv_rm_rn",sh4_subv_rm_rn},
{0xf00f,0x6008,"swap.b_rm_rn",sh4_swapb_rm_rn},
{0xf00f,0x6009,"swap.w_rm_rn",sh4_swapw_rm_rn},
{0xf0ff,0x401b,"tas",sh4_tas},
{0xff00,0xc300,"trapa",sh4_trapa, SH4INST_ILLSLOT},
{0xf00f,0x2008,"tst_rm_rn",sh4_tst_rm_rn},
{0xff00,0xc800,"tst_imm_r0",sh4_tst_imm_r0},
{0xff00,0xcc00,"tst.b_imm_ar0gbr",sh4_tstb_imm_ar0gbr},
{0xf00f,0x200a,"xor_rm_rn",sh4_xor_rm_rn},
{0xff00,0xca00,"xor_imm_r0",sh4_xor_imm_r0},
{0xff00,0xce00,"xor.b_imm_ar0gbr",sh4_xorb_ar0gbr},
{0xf00f,0x200d,"xtrct",sh4_xtrct},
};

void
SH4_IDecoderNew()
{
        uint32_t icode;
        int j;
        int num_instr = sizeof(instrlist) / sizeof(SH4_Instruction);
        sh4_iProcTab = sg_calloc(sizeof(SH4_InstructionProc *) * 0x10000);
        sh4_instrTab =  sg_calloc(sizeof(SH4_Instruction *) * 0x10000);
        if(!sh4_iProcTab) {
                fprintf(stderr,"Out of memory for allocating SH4 Instruction decoder\n");
                exit(1);
        }
        for(icode=0;icode<65536;icode++) {
                for(j=num_instr-1;j>=0;j--) {
                        SH4_Instruction *instr = &instrlist[j];
                        if((icode & instr->mask) == instr->opcode) {
                                if(sh4_iProcTab[icode]) {
                                        fprintf(stdout,"conflict at %04x %s, %s\n",icode,instr->name,sh4_instrTab[icode]->name);
                                } else {
                                        sh4_iProcTab[icode] = instr->iproc;
                                        sh4_instrTab[icode] = instr;
                                }
                        }
                }
                if(sh4_iProcTab[icode] == NULL) {
                        sh4_iProcTab[icode] =  sh4_undef;
                }
        }
        fprintf(stderr,"SH4 instruction decoder with %d Instructions created\n",num_instr);
}

