#include <idecode_m16c.h>
#include "instructions_m16c.h"
#include <xy_hash.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "sgstring.h"
#include "sglib.h"

M16C_InstructionProc **iProcTab;
M16C_Instruction **iTab;

static M16C_Instruction instrlist[] = {
	{0xfef0, 0x76f0, "abs.size_dst", 2, m16c_abs_size_dst},
	{0xfef0, 0x7660, "adc.size_immdst", 2, m16c_adc_size_immdst},
	{0xfe00, 0xb000, "adc.size_srcdst", 2, m16c_adc_size_srcdst},
	{0xfef0, 0x76e0, "adcf.size_dst", 2, m16c_adcf_size_dst},
	{0xfef0, 0x7640, "add.size:g", 2, m16c_add_size_g_immdst},
	{0xfe00, 0xc800, "add.size:q", 2, m16c_add_size_q},
	{0x00ff, 0x0083, "add.b:s_immdst", 1, m16c_add_b_s_immdst},
	{0x00ff, 0x0084, "add.b:s_immdst", 1, m16c_add_b_s_immdst},
	{0x00ff, 0x0085, "add.b:s_immdst", 1, m16c_add_b_s_immdst},
	{0x00ff, 0x0086, "add.b:s_immdst", 1, m16c_add_b_s_immdst},
	{0x00ff, 0x0087, "add.b:s_immdst", 1, m16c_add_b_s_immdst},
	{0xfe00, 0xa000, "add.size:g", 2, m16c_add_size_g_srcdst},
	{0x00f8, 0x0020, "add.b:s_srcr0l", 1, m16c_add_b_s_srcr0l},
	{0xfeff, 0x7ceb, "add.size:g:imm_sp", 2, m16c_add_size_g_imm_sp},
	{0xfff0, 0x7db0, "add_size:q:imm_sp", 2, m16c_add_size_q_imm_sp},
	{0xfe00, 0xf800, "addjnz_size_immdst", 2, m16c_adjnz_size_immdst},
	{0xfef0, 0x7620, "and.size:g_immdst", 2, m16c_and_size_g_immdst},
	{0x00ff, 0x0093, "and.b:s_immdst", 1, m16c_and_b_s_immdst},
	{0x00ff, 0x0094, "and.b:s_immdst", 1, m16c_and_b_s_immdst},
	{0x00ff, 0x0095, "and.b:s_immdst", 1, m16c_and_b_s_immdst},
	{0x00ff, 0x0096, "and.b:s_immdst", 1, m16c_and_b_s_immdst},
	{0x00ff, 0x0097, "and.b:s_immdst", 1, m16c_and_b_s_immdst},
	{0xfe00, 0x9000, "and.size:g_srcdst", 2, m16c_and_size_g_srcdst},
	{0x00f8, 0x0010, "and.b:s_srcr0l", 1, m16c_and_b_s_srcr0l},
	{0xfff0, 0x7e40, "band_src", 2, m16c_band_src},
	{0xfff0, 0x7e80, "bclr:g_dst", 2, m16c_bclr_g_dst},
	{0x00f8, 0x0040, "bclr:s_bit_base", 1, m16c_bclr_s_bit_base},
	{0xfff0, 0x7e20, "bmcnd_dst", 2, m16c_bmcnd_dst},
	{0xfff0, 0x7dd0, "bmcnd_c", 2, m16c_bmcnd_c},
	{0xfff0, 0x7e50, "bnand_src", 2, m16c_bnand_src},
	{0xfff0, 0x7e70, "bnor_src", 2, m16c_bnor_src},
	{0xfff0, 0x7ea0, "bnot:g_dst", 2, m16c_bnot_g_dst},
	{0x00f8, 0x0050, "bnot:s_bit_base", 1, m16c_bnot_s_bit_base},
	{0xfff0, 0x7e30, "bntst_src", 2, m16c_bntst_src},
	{0xfff0, 0x7ed0, "bnxor_src", 2, m16c_bnxor_src},
	{0xfff0, 0x7e60, "bor_src", 2, m16c_bor_src},
	{0x00ff, 0x0000, "brk", 1, m16c_brk},
	{0xfff0, 0x7e90, "bset:g_dst", 2, m16c_bset_g_dst},
	{0x00f8, 0x0048, "bset:s_bit_base", 1, m16c_bset_s_bit_base},
	{0xfff0, 0x7eb0, "btst:g_src", 2, m16c_btst_src},
	{0x00f8, 0x0058, "btst:s:bit_base", 1, m16c_btst_s_bit_base},
	{0xfff0, 0x7e00, "btstc_dst", 2, m16c_btstc_dst},
	{0xfff0, 0x7e10, "btsts_dst", 2, m16c_btsts_dst},
	{0xfff0, 0x7ec0, "bxor_src", 2, m16c_bxor_src},
	{0xfef0, 0x7680, "cmp.size:g_immdst", 2, m16c_cmp_size_g_immdst},
	{0xfe00, 0xd000, "cmp.size:q_immdst", 2, m16c_cmp_size_q_immdst},
	{0x00ff, 0x00e3, "cmp.b:s_immdst", 1, m16c_cmp_b_s_immdst},
	{0x00ff, 0x00e4, "cmp.b:s_immdst", 1, m16c_cmp_b_s_immdst},
	{0x00ff, 0x00e5, "cmp.b:s_immdst", 1, m16c_cmp_b_s_immdst},
	{0x00ff, 0x00e6, "cmp.b:s_immdst", 1, m16c_cmp_b_s_immdst},
	{0x00ff, 0x00e7, "cmp.b:s_immdst", 1, m16c_cmp_b_s_immdst},
	{0xfe00, 0xc000, "cmp.size:g_srcdst", 2, m16c_cmp_size_g_srcdst},
	{0x00f8, 0x0038, "cmp.b:s_srcr0l", 1, m16c_cmp_b_s_srcr0l},
	{0xffff, 0x7cee, "dadc.b_imm8_r0l", 2, m16c_dadc_b_imm8_r0l},
	{0xffff, 0x7dee, "dadc.w_imm16_r0", 2, m16c_dadc_w_imm16_r0},
	{0xffff, 0x7ce6, "dadc.b_r0h_r0l", 2, m16c_dadc_b_r0h_r0l},
	{0xffff, 0x7de6, "dadc.w_r0_r1", 2, m16c_dadc_w_r0_r1},
	{0xffff, 0x7cec, "dadd.b_imm8_r0l", 2, m16c_dadd_b_imm8_r0l},
	{0xffff, 0x7dec, "dadd.w_imm16_r0", 2, m16c_dadd_w_imm16_r0},
	{0xffff, 0x7ce4, "dadd.b_r0h_r0l", 2, m16c_dadd_b_r0h_r0l},
	{0xffff, 0x7de4, "dadd.w_r0_r1", 2, m16c_dadd_w_r0_r1},
	{0x00ff, 0x00ab, "dec.b_dst", 1, m16c_dec_b_dst},
	{0x00ff, 0x00ac, "dec.b_dst", 1, m16c_dec_b_dst},
	{0x00ff, 0x00ad, "dec.b_dst", 1, m16c_dec_b_dst},
	{0x00ff, 0x00ae, "dec.b_dst", 1, m16c_dec_b_dst},
	{0x00ff, 0x00af, "dec.b_dst", 1, m16c_dec_b_dst},
	{0x00f7, 0x00f2, "dec.w_dst", 1, m16c_dec_w_dst},
	{0xffff, 0x7ce1, "div.b_imm", 2, m16c_div_b_imm},
	{0xffff, 0x7de1, "div.w_imm", 2, m16c_div_w_imm},
	{0xfff0, 0x76d0, "div.b_src", 2, m16c_div_b_src},
	{0xfff0, 0x77d0, "div.w_src", 2, m16c_div_w_src},
	{0xffff, 0x7ce0, "divu.b_imm", 2, m16c_divu_b_imm},
	{0xffff, 0x7de0, "divu.w_imm", 2, m16c_divu_w_imm},
	{0xfff0, 0x76c0, "divu.b_src", 2, m16c_divu_b_src},
	{0xfff0, 0x77c0, "divu.w_src", 2, m16c_divu_w_src},
	{0xffff, 0x7ce3, "divx.b_imm", 2, m16c_divx_b_imm},
	{0xffff, 0x7de3, "divx.w_imm", 2, m16c_divx_b_imm},
	{0xfff0, 0x7690, "divx.b_src", 2, m16c_divx_b_src},
	{0xfff0, 0x7790, "divx.w_src", 2, m16c_divx_w_src},
	{0xffff, 0x7cef, "dsbb.b_imm8_r0l", 2, m16c_dsbb_b_imm8_r0l},
	{0xffff, 0x7def, "dsbb.w_imm16_r0", 2, m16c_dsbb_w_imm16_r0},
	{0xffff, 0x7ce7, "dsbb.b_r0h_r0l", 2, m16c_dsbb_b_r0h_r0l},
	{0xffff, 0x7de7, "dsbb.w_r1_r0", 2, m16c_dsbb_w_r1_r0},
	{0xffff, 0x7ced, "dsub.b_imm8_r0l", 2, m16c_dsub_b_imm8_r0l},
	{0xffff, 0x7ded, "dsub.w_imm16_r0", 2, m16c_dsub_w_imm16_r0},
	{0xffff, 0x7ce5, "dsub.b_r0h_r0l", 2, m16c_dsub_b_r0h_r0l},
	{0xffff, 0x7de5, "dsub.w_r1_r0", 2, m16c_dsub_w_r1_r0},
	{0xffff, 0x7cf2, "enter", 2, m16c_enter},
	{0xffff, 0x7df2, "exitd", 2, m16c_exitd},
	{0xfff0, 0x7c60, "exts.b_dst", 2, m16c_exts_b_dst},
	{0xffff, 0x7cf3, "exts.w_r0", 2, m16c_exts_w_r0},
	{0xffff, 0xeb05, "fclr_dst", 2, m16c_fclr_dst},
	{0xffff, 0xeb15, "fclr_dst", 2, m16c_fclr_dst},
	{0xffff, 0xeb25, "fclr_dst", 2, m16c_fclr_dst},
	{0xffff, 0xeb35, "fclr_dst", 2, m16c_fclr_dst},
	{0xffff, 0xeb45, "fclr_dst", 2, m16c_fclr_dst},
	{0xffff, 0xeb55, "fclr_dst", 2, m16c_fclr_dst},
	{0xffff, 0xeb65, "fclr_dst", 2, m16c_fclr_dst},
	{0xffff, 0xeb75, "fclr_dst", 2, m16c_fclr_dst},
	{0xffff, 0xeb04, "fset_dst", 2, m16c_fset_dst},
	{0xffff, 0xeb14, "fset_dst", 2, m16c_fset_dst},
	{0xffff, 0xeb24, "fset_dst", 2, m16c_fset_dst},
	{0xffff, 0xeb34, "fset_dst", 2, m16c_fset_dst},
	{0xffff, 0xeb44, "fset_dst", 2, m16c_fset_dst},
	{0xffff, 0xeb54, "fset_dst", 2, m16c_fset_dst},
	{0xffff, 0xeb64, "fset_dst", 2, m16c_fset_dst},
	{0xffff, 0xeb74, "fset_dst", 2, m16c_fset_dst},
	{0x00ff, 0x00a3, "inc.b_dst", 1, m16c_inc_b_dst},
	{0x00ff, 0x00a4, "inc.b_dst", 1, m16c_inc_b_dst},
	{0x00ff, 0x00a5, "inc.b_dst", 1, m16c_inc_b_dst},
	{0x00ff, 0x00a6, "inc.b_dst", 1, m16c_inc_b_dst},
	{0x00ff, 0x00a7, "inc.b_dst", 1, m16c_inc_b_dst},
	{0x00ff, 0x00b2, "inc.w_dst", 1, m16c_inc_w_dst},
	{0x00ff, 0x00ba, "inc.w_dst", 1, m16c_inc_w_dst},
	{0xffc0, 0xebc0, "int_imm", 2, m16c_int_imm},
	{0x00ff, 0x00f6, "into", 1, m16c_into},
	{0x00f8, 0x0068, "jcnd1", 1, m16c_jcnd1},
	{0xfff0, 0x7dc0, "jcnd2", 2, m16c_jcnd2},
	{0x00f8, 0x0060, "jmp.s", 1, m16c_jmp_s},
	{0x00ff, 0x00fe, "jmp.b", 1, m16c_jmp_b},
	{0x00ff, 0x00f4, "jmp.w", 1, m16c_jmp_w},
	{0x00ff, 0x00fc, "jmp.a", 1, m16c_jmp_a},
	{0xfff0, 0x7d20, "jmpi.w_src", 2, m16c_jmpi_w_src},
	{0xfff0, 0x7d00, "jmpi.a", 2, m16c_jmpi_a},
	{0x00ff, 0x00ee, "jmps_imm8", 1, m16c_jmps_imm8},
	{0x00ff, 0x00f5, "jsr.w", 1, m16c_jsr_w},
	{0x00ff, 0x00fd, "jsr.a", 1, m16c_jsr_a},
	{0xfff0, 0x7d30, "jsri.w", 2, m16c_jsri_w},
	{0xfff0, 0x7d10, "jsri.a", 2, m16c_jsri_a},
	{0x00ff, 0x00ef, "jsrs_imm8", 1, m16c_jsrs_imm8},
	{0xffff, 0xeb00, "ldc_imm16_dst", 2, m16c_ldc_imm16_dst},
	{0xffff, 0xeb10, "ldc_imm16_dst", 2, m16c_ldc_imm16_dst},
	{0xffff, 0xeb20, "ldc_imm16_dst", 2, m16c_ldc_imm16_dst},
	{0xffff, 0xeb30, "ldc_imm16_dst", 2, m16c_ldc_imm16_dst},
	{0xffff, 0xeb40, "ldc_imm16_dst", 2, m16c_ldc_imm16_dst},
	{0xffff, 0xeb50, "ldc_imm16_dst", 2, m16c_ldc_imm16_dst},
	{0xffff, 0xeb60, "ldc_imm16_dst", 2, m16c_ldc_imm16_dst},
	{0xffff, 0xeb70, "ldc_imm16_dst", 2, m16c_ldc_imm16_dst},
	{0xff80, 0x7a80, "ldc_srcdst", 2, m16c_ldc_srcdst},
	{0xffff, 0x7cf0, "ldctx", 2, m16c_ldctx},
	{0xfef0, 0x7480, "lde.size_abs20_dst", 2, m16c_lde_size_abs20_dst},
	{0xfef0, 0x7490, "lde.size_dsp_dst", 2, m16c_lde_size_dsp_dst},
	{0xfef0, 0x74a0, "lde.size_a1a0_dst", 2, m16c_lde_size_a1a0_dst},
	{0xfff8, 0x7da0, "ldipl_imm", 2, m16c_ldipl_imm},
	{0xfef0, 0x74c0, "mov.size:g_immdst", 2, m16c_mov_size_g_immdst},
	{0xfe00, 0xd800, "mov.size:q_immdst", 2, m16c_mov_size_q_immdst},
	{0x00ff, 0x00c3, "mov.b:s_imm8_dst", 1, m16c_mov_b_s_imm8_dst},
	{0x00ff, 0x00c4, "mov.b:s_imm8_dst", 1, m16c_mov_b_s_imm8_dst},
	{0x00ff, 0x00c5, "mov.b:s_imm8_dst", 1, m16c_mov_b_s_imm8_dst},
	{0x00ff, 0x00c6, "mov.b:s_imm8_dst", 1, m16c_mov_b_s_imm8_dst},
	{0x00ff, 0x00c7, "mov.b:s_imm8_dst", 1, m16c_mov_b_s_imm8_dst},
	{0x00ff, 0x00a2, "mov.size:s_immdst", 1, m16c_mov_size_s_immdst},
	{0x00ff, 0x00e2, "mov.size:s_immdst", 1, m16c_mov_size_s_immdst},
	{0x00ff, 0x00aa, "mov.size:s_immdst", 1, m16c_mov_size_s_immdst},
	{0x00ff, 0x00ea, "mov.size:s_immdst", 1, m16c_mov_size_s_immdst},
	{0x00ff, 0x00b3, "mov.b:z_0_dst", 1, m16c_mov_b_z_0_dst},
	{0x00ff, 0x00b4, "mov.b:z_0_dst", 1, m16c_mov_b_z_0_dst},
	{0x00ff, 0x00b5, "mov.b:z_0_dst", 1, m16c_mov_b_z_0_dst},
	{0x00ff, 0x00b6, "mov.b:z_0_dst", 1, m16c_mov_b_z_0_dst},
	{0x00ff, 0x00b7, "mov.b:z_0_dst", 1, m16c_mov_b_z_0_dst},
	{0xfe00, 0x7200, "mov.size:g_srcdst", 2, m16c_mov_size_g_srcdst},
	{0x00f8, 0x0030, "mov.b:s_srcdst", 1, m16c_mov_b_s_srcdst},
	{0x00ff, 0x0001, "mov.b:s_r0dst", 1, m16c_mov_b_r0dst},
	{0x00ff, 0x0002, "mov.b:s_r0dst", 1, m16c_mov_b_r0dst},
	{0x00ff, 0x0003, "mov.b:s_r0dst", 1, m16c_mov_b_r0dst},
	{0x00ff, 0x0005, "mov.b:s_r0dst", 1, m16c_mov_b_r0dst},
	{0x00ff, 0x0006, "mov.b:s_r0dst", 1, m16c_mov_b_r0dst},
	{0x00ff, 0x0007, "mov.b:s_r0dst", 1, m16c_mov_b_r0dst},
	{0x00f8, 0x0008, "mov.b:s_src_r0", 1, m16c_mov_b_s_r0},
	{0xfef0, 0x74b0, "mov.size:g_dspdst", 2, m16c_mov_size_g_dspdst},
	{0xfef0, 0x7430, "mov.size:g_srcdsp", 2, m16c_mov_size_g_srcdsp},
	{0xff88, 0xeb08, "mova_srcdst", 2, m16c_mova_srcdst},
	{0xffc0, 0x7c80, "movdir_r0dst", 2, m16c_movdir_r0dst},
	{0xffc0, 0x7c00, "movdir_srcr0l", 2, m16c_movdir_srcr0l},
	{0xfef0, 0x7c50, "mul.size_immdst", 2, m16c_mul_size_immdst},
	{0xfe00, 0x7800, "mul.size_srcdst", 2, m16c_mul_size_srcdst},
	{0xfef0, 0x7c40, "mulu.size_immdst", 2, m16c_mulu_size_immdst},
	{0xfe00, 0x7000, "mulu.size_srcdst", 2, m16c_mulu_size_srcdst},
	{0xfef0, 0x7450, "neg.size_dst", 2, m16c_neg_size_dst},
	{0x00ff, 0x0004, "nop", 1, m16c_nop},
	{0xfef0, 0x7470, "not.size:g_dst", 2, m16c_not_size_g_dst},
	{0x00ff, 0x00bb, "not.b:s_dst", 1, m16c_not_b_s_dst},
	{0x00ff, 0x00bc, "not.b:s_dst", 1, m16c_not_b_s_dst},
	{0x00ff, 0x00bd, "not.b:s_dst", 1, m16c_not_b_s_dst},
	{0x00ff, 0x00be, "not.b:s_dst", 1, m16c_not_b_s_dst},
	{0x00ff, 0x00bf, "not.b:s_dst", 1, m16c_not_b_s_dst},
	{0xfef0, 0x7630, "or.size:g_immdst", 2, m16c_or_size_g_immdst},
	{0x00ff, 0x009b, "or.b:s_immdst", 1, m16c_or_b_s_immdst},
	{0x00ff, 0x009c, "or.b:s_immdst", 1, m16c_or_b_s_immdst},
	{0x00ff, 0x009d, "or.b:s_immdst", 1, m16c_or_b_s_immdst},
	{0x00ff, 0x009e, "or.b:s_immdst", 1, m16c_or_b_s_immdst},
	{0x00ff, 0x009f, "or.b:s_immdst", 1, m16c_or_b_s_immdst},
	{0xfe00, 0x9800, "or.size:g_srcdst", 2, m16c_or_size_g_srcdst},
	{0x00f8, 0x0018, "or.b:s_srcr0", 1, m16c_or_b_s_srcr0},
	{0xfef0, 0x74d0, "pop.size:g_dst", 2, m16c_pop_size_g_dst},
	{0x00ff, 0x0092, "pop.b:s_dst", 1, m16c_pop_b_s_dst},
	{0x00ff, 0x009a, "pop.b:s_dst", 1, m16c_pop_b_s_dst},
	{0x00f7, 0x00d2, "pop.w:s_dst", 1, m16c_pop_w_s_dst},
	{0xffff, 0xeb03, "popc_dst", 2, m16c_popc_dst},
	{0xffff, 0xeb13, "popc_dst", 2, m16c_popc_dst},
	{0xffff, 0xeb23, "popc_dst", 2, m16c_popc_dst},
	{0xffff, 0xeb33, "popc_dst", 2, m16c_popc_dst},
	{0xffff, 0xeb43, "popc_dst", 2, m16c_popc_dst},
	{0xffff, 0xeb53, "popc_dst", 2, m16c_popc_dst},
	{0xffff, 0xeb63, "popc_dst", 2, m16c_popc_dst},
	{0xffff, 0xeb73, "popc_dst", 2, m16c_popc_dst},
	{0x00ff, 0x00ed, "popm_dst", 1, m16c_popm_dst},
	{0xfeff, 0x7ce2, "push.size:g_imm", 2, m16c_push_size_g_imm},
	{0xfef0, 0x7440, "push.size:g_src", 2, m16c_push_size_g_src},
	{0x00ff, 0x0082, "push.b:s_src", 1, m16c_pushb_s_src},
	{0x00ff, 0x008a, "push.b:s_src", 1, m16c_pushb_s_src},
	{0x00ff, 0x00c2, "push.w:s_src", 1, m16c_push_w_src},
	{0x00ff, 0x00ca, "push.w:s_src", 1, m16c_push_w_src},
	{0xfff0, 0x7d90, "pusha_src", 2, m16c_pusha_src},
	{0xffff, 0xeb02, "pushc_src", 2, m16c_pushc_src},
	{0xffff, 0xeb12, "pushc_src", 2, m16c_pushc_src},
	{0xffff, 0xeb22, "pushc_src", 2, m16c_pushc_src},
	{0xffff, 0xeb32, "pushc_src", 2, m16c_pushc_src},
	{0xffff, 0xeb42, "pushc_src", 2, m16c_pushc_src},
	{0xffff, 0xeb52, "pushc_src", 2, m16c_pushc_src},
	{0xffff, 0xeb62, "pushc_src", 2, m16c_pushc_src},
	{0xffff, 0xeb72, "pushc_src", 2, m16c_pushc_src},
	{0x00ff, 0x00ec, "pushm_src", 1, m16c_pushm_src},
	{0x00ff, 0x00fb, "reit", 1, m16c_reit},
	{0xffff, 0x7cf1, "rmpa_b", 2, m16c_rmpa_b},
	{0xffff, 0x7df1, "rmpa_w", 2, m16c_rmpa_w},
	{0xfef0, 0x76a0, "rolc.size_dst", 2, m16c_rolc_size_dst},
	{0xfef0, 0x76b0, "rorc.size_dst", 2, m16c_rorc_size_dst},
	{0xfe00, 0xe000, "rot.size_immdst", 2, m16c_rot_size_immdst},
	{0xfef0, 0x7460, "rot.size_r1hdst", 2, m16c_rot_size_r1hdst},
	{0x00ff, 0x00f3, "rts", 1, m16c_rts},
	{0xfef0, 0x7670, "sbb.size_immdst", 2, m16c_sbb_size_immdst},
	{0xfe00, 0xb800, "sbb.size_srcdst", 2, m16c_sbb_size_srcdst},
	{0xfe00, 0xf000, "sha.size_immdst", 2, m16c_sha_size_immdst},
	{0xfef0, 0x74f0, "sha.size_r1hdst", 2, m16c_sha_size_r1hdst},
	{0xffe0, 0xeba0, "sha.l_immdst", 2, m16c_sha_l_immdst},
	{0xffff, 0xeb21, "sha.l_r1hdst", 2, m16c_sha_l_r1hdst},
	{0xffff, 0xeb31, "sha.l_r1hdst", 2, m16c_sha_l_r1hdst},
	{0xfe00, 0xe800, "shl.size_immdst", 2, m16c_shl_size_immdst},
	{0xfff0, 0x74e0, "shl.size_r1hdst", 2, m16c_shl_size_r1hdst},
	{0xfff0, 0x75e0, "shl.size_r1hdst", 2, m16c_shl_size_r1hdst},
	{0xffe0, 0xeb80, "shl.l_immdst", 2, m16c_shl_l_immdst},
	{0xffff, 0xeb01, "shl.l_r1hdst", 2, m16c_shl_l_r1hdst},
	{0xffff, 0xeb11, "shl.l_r1hdst", 2, m16c_shl_l_r1hdst},
	{0xfeff, 0x7ce9, "smovb.size", 2, m16c_smovb_size},
	{0xfeff, 0x7ce8, "smovf.size", 2, m16c_smovf_size},
	{0xfeff, 0x7cea, "sstr.size", 2, m16c_sstr_size},
	{0xff80, 0x7b80, "stc_srcdst", 2, m16c_stc_srcdst},
	{0xfff0, 0x7cc0, "stc_pcdst", 2, m16c_stc_pcdst},
	{0xffff, 0x7df0, "stctx_abs16abs20", 2, m16c_stctx_abs16abs20},
	{0xfef0, 0x7400, "ste.size_srcabs20", 2, m16c_ste_size_srcabs20},
	{0xfef0, 0x7410, "ste.size_srcdsp20", 2, m16c_ste_size_srcdsp20},
	{0xfef0, 0x7420, "ste.size_srca1a0", 2, m16c_ste_size_srca1a0},
	{0x00ff, 0x00d3, "stnz_immdst", 1, m16c_stnz_immdst},
	{0x00ff, 0x00d4, "stnz_immdst", 1, m16c_stnz_immdst},
	{0x00ff, 0x00d5, "stnz_immdst", 1, m16c_stnz_immdst},
	{0x00ff, 0x00d6, "stnz_immdst", 1, m16c_stnz_immdst},
	{0x00ff, 0x00d7, "stnz_immdst", 1, m16c_stnz_immdst},
	{0x00ff, 0x00cb, "stz_immdst", 1, m16c_stz_immdst},
	{0x00ff, 0x00cc, "stz_immdst", 1, m16c_stz_immdst},
	{0x00ff, 0x00cd, "stz_immdst", 1, m16c_stz_immdst},
	{0x00ff, 0x00ce, "stz_immdst", 1, m16c_stz_immdst},
	{0x00ff, 0x00cf, "stz_immdst", 1, m16c_stz_immdst},
	{0x00ff, 0x00db, "stzx_immimmdst", 1, m16c_stzx_immimmdst},
	{0x00ff, 0x00dc, "stzx_immimmdst", 1, m16c_stzx_immimmdst},
	{0x00ff, 0x00dd, "stzx_immimmdst", 1, m16c_stzx_immimmdst},
	{0x00ff, 0x00de, "stzx_immimmdst", 1, m16c_stzx_immimmdst},
	{0x00ff, 0x00df, "stzx_immimmdst", 1, m16c_stzx_immimmdst},
	{0xfef0, 0x7650, "sub.size:g_immdst", 2, m16c_sub_size_g_immdst},
	{0x00ff, 0x008b, "sub.b:s_immdst", 1, m16c_sub_b_s_immdst},
	{0x00ff, 0x008c, "sub.b:s_immdst", 1, m16c_sub_b_s_immdst},
	{0x00ff, 0x008d, "sub.b:s_immdst", 1, m16c_sub_b_s_immdst},
	{0x00ff, 0x008e, "sub.b:s_immdst", 1, m16c_sub_b_s_immdst},
	{0x00ff, 0x008f, "sub.b:s_immdst", 1, m16c_sub_b_s_immdst},
	{0xfe00, 0xa800, "sub.size:g_srcdst", 2, m16c_sub_size_g_srcdst},
	{0x00f8, 0x0028, "sub.b:s_srcr0lr0h", 1, m16c_sub_b_srcr0lr0h},
	{0xfef0, 0x7600, "tst.size_immdst", 2, m16c_tst_size_immdst},
	{0xfe00, 0x8000, "tst.size_srcdst", 2, m16c_tst_size_srcdst},
	{0x00ff, 0x00ff, "und", 1, m16c_und},
	{0xffff, 0x7df3, "wait", 2, m16c_wait},
	{0xfec0, 0x7a00, "xchg.size_srcdst", 2, m16c_xchg_size_srcdst},
	{0xfef0, 0x7610, "xor.size_immdst", 2, m16c_xor_size_immdst},
	{0xfe00, 0x8800, "xor.size_srcdst", 2, m16c_xor_size_srcdst},
	{0, 0, NULL, 0, NULL}
};

static M16C_Instruction undefined_instr = {
	0x0, 0, "undefined", 1 /* ? */ , m16c_und
};

void
M16C_IDecoderNew()
{
	int i, j;
	int onecount1, onecount2;
	iProcTab = (M16C_InstructionProc **) sg_calloc(0x10000 * sizeof(M16C_InstructionProc *));
	iTab = sg_calloc(0x10000 * sizeof(M16C_Instruction *));
	fprintf(stderr, "Allocated M16C Instruction decoder table\n");
	for (j = 0; instrlist[j].proc; j++) {
		M16C_Instruction *instr = &instrlist[j];
		if (instr->len == 1) {
			instr->icode <<= 8;
			instr->mask <<= 8;
		}
	}
	for (i = 0; i < 65536; i++) {
		for (j = 0; instrlist[j].proc; j++) {
			M16C_Instruction *instr = &instrlist[j];
			uint16_t specmask1, specmask2;
#if 0
			if ((instr->icode & instr->mask) != instr->icode) {
				fprintf(stderr, "Error in instr %s\n", instr->name);
			}
#endif
			if ((i & instr->mask) == instr->icode) {
				if (iTab[i]) {
					M16C_Instruction *instr2 = iTab[i];
					specmask1 = instr->mask;
					specmask2 = instr2->mask;
					onecount1 = SGLib_OnecountU32(instr->mask);
					onecount2 = SGLib_OnecountU32(instr2->mask);
					fprintf(stderr, "Collission %s, %s\n", instr->name,
						instr2->name);
#if 0
					if (instr->len > instr2->len) {
						iTab[i] = instr;
						iProcTab[i] = instr->proc;
					} else if (instr2->len > instr->len) {
						iTab[i] = instr2;
						iProcTab[i] = instr2->proc;
					} else
#endif
					if (onecount1 > onecount2) {
						iTab[i] = instr;
						iProcTab[i] = instr->proc;
					} else if (onecount2 > onecount1) {
						iTab[i] = instr2;
						iProcTab[i] = instr2->proc;
					} else {
						fprintf(stderr, "Can not decide %s, %s\n",
							instr->name, instr2->name);
					}
#if 0
					if (instr->len == 1) {
						specmask1 |= 0xff00;
					}
					if (instr2->len == 1) {
						specmask2 |= 0xff00;
					}
					if ((specmask2 & specmask1) == specmask1) {
						iTab[i] = instr2;
						iProcTab[i] = instr2->proc;
					} else if ((specmask2 & specmask1) == specmask2) {
						iTab[i] = instr;
						iProcTab[i] = instr->proc;
					} else {
						fprintf(stdout,
							"%04x: no instruction is more specific %s %s %04x %04x %d %d\n",
							i, instr->name, instr2->name, instr->icode,
							instr2->icode, instr->len, instr2->len);
						exit(18);
					}
#endif
				} else {
					iTab[i] = instr;
					iProcTab[i] = instr->proc;
				}
			}
		}
		if (iTab[i] == NULL) {
			iTab[i] = &undefined_instr;
			iProcTab[i] = (&undefined_instr)->proc;
		}
	}
}
