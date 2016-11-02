#include <instructions_c16x.h>
#include <idecode_c16x.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sgstring.h"

C16x_Instruction **iTab;

static C16x_Instruction instrlist[] = {
	{0x00, 0xff, "add_rw_rw", 2, c16x_add_rw_rw},
	{0x01, 0xff, "addb_rb_rb", 2, c16x_addb_rb_rb},
	{0x02, 0xff, "add_reg_mem", 4, c16x_add_reg_mem},
	{0x03, 0xff, "addb_reg_mem", 4, c16x_addb_reg_mem},
	{0x04, 0xff, "add_mem_reg", 4, c16x_add_mem_reg},
	{0x05, 0xff, "addb_mem_reg", 4, c16x_addb_mem_reg},
	{0x06, 0xff, "add_reg_data16", 4, c16x_add_reg_data16},
	{0x07, 0xff, "addb_reg_data8", 4, c16x_addb_reg_data8},
	{0x08, 0xff, "add_rw_x", 2, c16x_add_rw_x},
	{0x09, 0xff, "addb_rb_x", 2, c16x_addb_rb_x},
	{0x0a, 0xff, "bfldl_boff_mask8_data8", 4, c16x_bfldl_boff_mask8_data8},
	{0x0b, 0xff, "mul_rw_rw", 2, c16x_mul_rw_rw},
	{0x0c, 0xff, "rol_rw_rw", 2, c16x_rol_rw_rw},
	{0x0d, 0x0f, "jmpr_cc_rel", 2, c16x_jmpr_cc_rel},
	{0x0e, 0x0f, "bclr", 2, c16x_bclr},
	{0x0f, 0x0f, "bset", 2, c16x_bset},
	{0x10, 0xff, "addc_rw_rw", 2, c16x_addc_rw_rw},
	{0x11, 0xff, "addcb_rb_rb", 2, c16x_addcb_rb_rb},
	{0x12, 0xff, "addc_reg_mem", 4, c16x_addc_reg_mem},
	{0x13, 0xff, "addcb_reg_mem", 4, c16x_addcb_reg_mem},
	{0x14, 0xff, "addc_mem_reg", 4, c16x_addc_mem_reg},
	{0x15, 0xff, "addcb_mem_reg", 4, c16x_addcb_mem_reg},
	{0x16, 0xff, "addc_reg_data16", 4, c16x_addc_reg_data16},
	{0x17, 0xff, "addcb_reg_data8", 4, c16x_addcb_reg_data8},
	{0x18, 0xff, "addc_rw_x", 2, c16x_addc_rw_x},
	{0x19, 0xff, "addcb_rb_x", 2, c16x_addcb_rb_x},
	{0x1a, 0xff, "bfldh_boff_mask8_data8", 4, c16x_bfldh_boff_mask8_data8},
	{0x1b, 0xff, "mulu_rw_rw", 2, c16x_mulu_rw_rw},
	{0x1c, 0xff, "rol_rw_data4", 2, c16x_rol_rw_data4},
	{0x20, 0xff, "sub_rw_rw", 2, c16x_sub_rw_rw},
	{0x21, 0xff, "subb_rb_rb", 2, c16x_subb_rb_rb},
	{0x22, 0xff, "sub_reg_mem", 4, c16x_sub_reg_mem},
	{0x23, 0xff, "subb_reg_mem", 4, c16x_subb_reg_mem},
	{0x24, 0xff, "sub_mem_reg", 4, c16x_sub_mem_reg},
	{0x25, 0xff, "subb_mem_reg", 4, c16x_subb_mem_reg},
	{0x26, 0xff, "sub_reg_data16", 4, c16x_sub_reg_data16},
	{0x27, 0xff, "subb_reg_data8", 4, c16x_subb_reg_data8},
	{0x28, 0xff, "sub_rw_x", 2, c16x_sub_rw_x},
	{0x29, 0xff, "subb_rb_x", 2, c16x_subb_rb_x},
	{0x2a, 0xff, "bcmp_bitaddr_bitaddr", 4, c16x_bcmp_bitaddr_bitaddr},
	{0x2b, 0xff, "prior_rw_rw", 2, c16x_prior_rw_rw},
	{0x2c, 0xff, "ror_rw_rw", 2, c16x_ror_rw_rw},
	{0x30, 0xff, "subc_rw_rw", 2, c16x_subc_rw_rw},
	{0x31, 0xff, "subcb_rb_rb", 2, c16x_subcb_rb_rb},
	{0x32, 0xff, "subc_reg_mem", 4, c16x_subc_reg_mem},
	{0x33, 0xff, "subcb_reg_mem", 4, c16x_subcb_reg_mem},
	{0x34, 0xff, "subc_mem_reg", 4, c16x_subc_mem_reg},
	{0x35, 0xff, "subcb_mem_reg", 4, c16x_subcb_mem_reg},
	{0x36, 0xff, "subc_reg_data16", 4, c16x_subc_reg_data16},
	{0x37, 0xff, "subcb_reg_data8", 4, c16x_subcb_reg_data8},
	{0x38, 0xff, "subc_rw_x", 2, c16x_subc_rw_x},
	{0x39, 0xff, "subcb_rb_x", 2, c16x_subcb_rb_x},
	{0x3a, 0xff, "bmovn_bitaddr_bitaddr", 4, c16x_bmovn_bitaddr_bitaddr},
	{0x3c, 0xff, "ror_rw_data4", 2, c16x_ror_rw_data4},
	{0x40, 0xff, "cmp_rw_rw", 2, c16x_cmp_rw_rw},
	{0x41, 0xff, "cmpb_rb_rb", 2, c16x_cmpb_rb_rb},
	{0x42, 0xff, "cmp_reg_mem", 4, c16x_cmp_reg_mem},
	{0x43, 0xff, "cmpb_reg_mem", 4, c16x_cmpb_reg_mem},
	{0x46, 0xff, "cmp_reg_data16", 4, c16x_cmp_reg_data16},
	{0x47, 0xff, "cmpb_reg_data8", 4, c16x_cmpb_reg_data8},
	{0x48, 0xff, "cmp_rw_x", 2, c16x_cmp_rw_x},
	{0x49, 0xff, "cmpb_rb_x", 2, c16x_cmpb_rb_x},
	{0x4a, 0xff, "bmov_bitaddr_bitaddr", 4, c16x_bmov_bitaddr_bitaddr},
	{0x4b, 0xff, "div_rw", 2, c16x_div_rw},
	{0x4c, 0xff, "shl_rw_rw", 2, c16x_shl_rw_rw},
	{0x50, 0xff, "xor_rw_rw", 2, c16x_xor_rw_rw},
	{0x51, 0xff, "xorb_rb_rb", 2, c16x_xorb_rb_rb},
	{0x52, 0xff, "xor_reg_mem", 4, c16x_xor_reg_mem},
	{0x53, 0xff, "xorb_reg_mem", 4, c16x_xorb_reg_mem},
	{0x54, 0xff, "xor_mem_reg", 4, c16x_xor_mem_reg},
	{0x55, 0xff, "xorb_mem_reg", 4, c16x_xorb_mem_reg},
	{0x56, 0xff, "xor_reg_data16", 4, c16x_xor_reg_data16},
	{0x57, 0xff, "xorb_reg_data8", 4, c16x_xorb_reg_data8},
	{0x58, 0xff, "xor_rw_x", 2, c16x_xor_rw_x},
	{0x59, 0xff, "xorb_rb_x", 2, c16x_xorb_rb_x},
	{0x5a, 0xff, "bor_bitaddr_bitaddr", 4, c16x_bor_bitaddr_bitaddr},
	{0x5b, 0xff, "divu_rw", 2, c16x_divu_rw},
	{0x5c, 0xff, "shl_rw_data4", 2, c16x_shl_rw_data4},
	{0x60, 0xff, "and_rw_rw", 2, c16x_and_rw_rw},
	{0x61, 0xff, "andb_rb_rb", 2, c16x_andb_rb_rb},
	{0x62, 0xff, "and_reg_mem", 4, c16x_and_reg_mem},
	{0x63, 0xff, "andb_reg_mem", 4, c16x_andb_reg_mem},
	{0x64, 0xff, "and_mem_reg", 4, c16x_and_mem_reg},
	{0x65, 0xff, "andb_mem_reg", 4, c16x_andb_mem_reg},
	{0x66, 0xff, "and_reg_data16", 4, c16x_and_reg_data16},
	{0x67, 0xff, "andb_reg_data8", 4, c16x_andb_reg_data8},
	{0x68, 0xff, "and_rw_x", 2, c16x_and_rw_x},
	{0x69, 0xff, "andb_rb_x", 2, c16x_andb_rb_x},
	{0x6a, 0xff, "band_bitaddr_bitaddr", 4, c16x_band_bitaddr_bitaddr},
	{0x6b, 0xff, "divl_rw", 2, c16x_divl_rw},
	{0x6c, 0xff, "shr_rw_rw", 2, c16x_shr_rw_rw},
	{0x70, 0xff, "or_rw_rw", 2, c16x_or_rw_rw},
	{0x71, 0xff, "orb_rb_rb", 2, c16x_orb_rb_rb},
	{0x72, 0xff, "or_reg_mem", 4, c16x_or_reg_mem},
	{0x73, 0xff, "orb_reg_mem", 4, c16x_orb_reg_mem},
	{0x74, 0xff, "or_mem_reg", 4, c16x_or_mem_reg},
	{0x75, 0xff, "orb_mem_reg", 4, c16x_orb_mem_reg},
	{0x76, 0xff, "or_reg_data16", 4, c16x_or_reg_data16},
	{0x77, 0xff, "orb_reg_data8", 4, c16x_orb_reg_data8},
	{0x78, 0xff, "or_rw_x", 2, c16x_or_rw_x},
	{0x79, 0xff, "orb_rb_x", 2, c16x_orb_rb_x},
	{0x7a, 0xff, "bxor_bitaddr_bitaddr", 4, c16x_bxor_bitaddr_bitaddr},
	{0x7b, 0xff, "divlu_rw", 2, c16x_divlu_rw},
	{0x7c, 0xff, "shr_rw_data4", 2, c16x_shr_rw_data4},
	{0x80, 0xff, "cmpi1_rw_data4", 2, c16x_cmpi1_rw_data4},
	{0x81, 0xff, "neg_rw", 2, c16x_neg_rw},
	{0x82, 0xff, "cmpi1_rw_mem", 4, c16x_cmpi1_rw_mem},
	{0x84, 0xff, "mov__rw__mem", 4, c16x_mov__rw__mem},
	{0x86, 0xff, "cmpi1_rw_data16", 4, c16x_cmpi1_rw_data16},
	{0x87, 0xff, "idle", 4, c16x_idle},
	{0x88, 0xff, "mov__mrw__rw", 2, c16x_mov__mrw__rw},
	{0x89, 0xff, "movb__mrw__rb", 2, c16x_movb__mrw__rb},
	{0x8a, 0xff, "jb_bitaddr_rel", 4, c16x_jb_bitaddr_rel},
	{0x90, 0xff, "cmpi2_rw_data4", 2, c16x_cmpi2_rw_data4},
	{0x91, 0xff, "cpl_rw", 2, c16x_cpl_rw},
	{0x92, 0xff, "cmpi2_rw_mem", 4, c16x_cmpi2_rw_mem},
	{0x94, 0xff, "mov_mem__rw_", 4, c16x_mov_mem__rw_},
	{0x96, 0xff, "cmpi2_rw_data16", 4, c16x_cmpi2_rw_data16},
	{0x97, 0xff, "pwrdn", 4, c16x_pwrdn},
	{0x98, 0xff, "mov_rw__rwp_", 2, c16x_mov_rw__rwp_},
	{0x99, 0xff, "movb_rb__rwp_", 2, c16x_movb_rb__rwp_},
	{0x9a, 0xff, "jnb_bitaddr_rel", 4, c16x_jnb_bitaddr_rel},
	{0x9b, 0xff, "trap_ntrap7", 2, c16x_trap_ntrap7},
	{0x9c, 0xff, "jmpi_cc__rw_", 2, c16x_jmpi_cc__rw_},
	{0xa0, 0xff, "cmpd1_rw_data4", 2, c16x_cmpd1_rw_data4},
	{0xa1, 0xff, "negb_rb", 2, c16x_negb_rb},
	{0xa2, 0xff, "cmpd1_rw_mem", 4, c16x_cmpd1_rw_mem},
	{0xa4, 0xff, "movb__rw__mem", 4, c16x_movb__rw__mem},
	{0xa5, 0xff, "diswdt", 4, c16x_diswdt},
	{0xa6, 0xff, "cmpd1_rw_data16", 4, c16x_cmpd1_rw_data16},
	{0xa7, 0xff, "srvwdt", 4, c16x_srvwdt},
	{0xa8, 0xff, "mov_rw__rw_", 2, c16x_mov_rw__rw_},
	{0xa9, 0xff, "movb_rb__rw_", 2, c16x_movb_rb__rw_},
	{0xaa, 0xff, "jbc_bitaddr_rel", 4, c16x_jbc_bitaddr_rel},
	{0xab, 0xff, "calli_cc__rw_", 2, c16x_calli_cc__rw_},
	{0xac, 0xff, "ashr_rw_rw", 2, c16x_ashr_rw_rw},
	{0xb0, 0xff, "cmpd2_rw_data4", 2, c16x_cmpd2_rw_data4},
	{0xb1, 0xff, "cplb_rb", 2, c16x_cplb_rb},
	{0xb2, 0xff, "cmpd2_rw_mem", 4, c16x_cmpd2_rw_mem},
	{0xb4, 0xff, "movb_mem__rw_", 4, c16x_movb_mem__rw_},
	{0xb5, 0xff, "einit", 4, c16x_einit},
	{0xb6, 0xff, "cmpd2_rw_data16", 4, c16x_cmpd2_rw_data16},
	{0xb7, 0xff, "srst", 4, c16x_srst},
	{0xb8, 0xff, "mov__rw__rw", 2, c16x_mov__rw__rw},
	{0xb9, 0xff, "movb__rw__rb", 2, c16x_movb__rw__rb},
	{0xba, 0xff, "jnbs_bitaddr_rel", 4, c16x_jnbs_bitaddr_rel},
	{0xbb, 0xff, "callr_rel", 2, c16x_callr_rel},
	{0xbc, 0xff, "ashr_rw_data4", 2, c16x_ashr_rw_data4},
	{0xc0, 0xff, "movbz_rw_rb", 2, c16x_movbz_rw_rb},
	{0xc2, 0xff, "movbz_reg_mem", 4, c16x_movbz_reg_mem},
	{0xc4, 0xff, "mov__rwpdata16__rw", 4, c16x_mov__rwpdata16__rw},
	{0xc5, 0xff, "movbz_mem_reg", 4, c16x_movbz_mem_reg},
	{0xc6, 0xff, "scxt_reg_data16", 4, c16x_scxt_reg_data16},
	{0xc8, 0xff, "mov__rw___rw_", 2, c16x_mov__rw___rw_},
	{0xc9, 0xff, "movb__rw___rw_", 2, c16x_movb__rw___rw_},
	{0xca, 0xff, "calla_cc_addr", 4, c16x_calla_cc_addr},
	{0xcb, 0xff, "ret", 2, c16x_ret},
	{0xcc, 0xff, "nop", 2, c16x_nop},
	{0xd0, 0xff, "movbs_rw_rb", 2, c16x_movbs_rw_rb},
	{0xd1, 0xff, "atomic_extr_irang2", 2, c16x_atomic_extr_irang2},
	{0xd2, 0xff, "movbs_reg_mem", 4, c16x_movbs_reg_mem},
	{0xd4, 0xff, "mov_rw__rwpdata16_", 4, c16x_mov_rw__rwpdata16_},
	{0xd5, 0xff, "movbs_mem_reg", 4, c16x_movbs_reg_mem},
	{0xd6, 0xff, "scxt_reg_mem", 4, c16x_scxt_reg_mem},
	{0xd7, 0xff, "extp_exts_p10", 4, c16x_extp_exts_p10},
	{0xd8, 0xff, "mov__rwp___rw_", 2, c16x_mov__rwp___rw_},
	{0xd9, 0xff, "movb__rwp___rw_", 2, c16x_movb__rwp___rw_},
	{0xda, 0xff, "calls_seg_caddr", 4, c16x_calls_seg_caddr},
	{0xdb, 0xff, "rets", 2, c16x_rets},
	{0xdc, 0xff, "extp_exts_rwirang", 2, c16x_extp_exts_rwirang},
	{0xe0, 0xff, "mov_rw_data4", 2, c16x_mov_rw_data4},
	{0xe1, 0xff, "mov_rb_data4", 2, c16x_mov_rb_data4},
	{0xe2, 0xff, "pcall_reg_caddr", 4, c16x_pcall_reg_caddr},
	{0xe4, 0xff, "movb__rwpdata16__rb", 4, c16x_movb__rwpdata16__rb},
	{0xe6, 0xff, "mov_reg_data16", 4, c16x_mov_reg_data16},
	{0xe7, 0xff, "movb_reg_data8", 4, c16x_movb_reg_data8},
	{0xe8, 0xff, "mov__rw___rwp_", 4, c16x_mov__rw___rwp_},
	{0xe9, 0xff, "movb__rw___rwp_", 4, c16x_movb__rw___rwp_},
	{0xea, 0xff, "jmpa_cc_caddr", 4, c16x_jmpa_cc_caddr},
	{0xeb, 0xff, "retp_reg", 2, c16x_retp_reg},
	{0xec, 0xff, "push_reg", 2, c16x_push_reg},
	{0xf0, 0xff, "mov_rw_rw", 2, c16x_mov_rw_rw},
	{0xf1, 0xff, "movb_rb_rb", 2, c16x_movb_rb_rb},
	{0xf2, 0xff, "mov_reg_mem", 4, c16x_mov_reg_mem},
	{0xf3, 0xff, "movb_reg_mem", 4, c16x_movb_reg_mem},
	{0xf4, 0xff, "movb_rb__rwpdata16_", 4, c16x_movb_rb__rwpdata16_},
	{0xf6, 0xff, "mov_mem_reg", 4, c16x_mov_mem_reg},
	{0xf7, 0xff, "movb_mem_reg", 4, c16x_movb_mem_reg},
	{0xfa, 0xff, "jmps_seg_caddr", 4, c16x_jmps_seg_caddr},
	{0xfb, 0xff, "reti", 2, c16x_reti},
	{0xfc, 0xff, "pop_reg", 2, c16x_pop_reg},
	{0, 0, NULL, 0, NULL},
};

C16x_Instruction illegal_opcode = { 0x00, 0x00, "illegal opcode", 2, c16x_illegal_opcode };

void
C16x_IDecoderNew()
{
	int icode;
	fprintf(stderr, "Initialize C16x Instruction decoder\n");
	iTab = (C16x_Instruction **) sg_calloc(0x100 * sizeof(C16x_Instruction *));
	for (icode = 0; icode < 256; icode++) {
		int j;
		C16x_Instruction *instr;
		for (j = 0; (instr = &instrlist[j])->name; j++) {
			if ((icode & instr->mask) == instr->opcode) {
				if (iTab[icode]) {
					fprintf(stderr,
						"Instruction already exists for icode 0x%02x, instr->name %s\n",
						icode, instr->name);
				} else {
					iTab[icode] = instr;
				}
			}
		}
		if (!iTab[icode]) {
			//fprintf(stderr,"Warning, no instruction for icode 0x%02x\n",icode);
			iTab[icode] = &illegal_opcode;
		}

	}
}

#ifdef BLA
void
print_header(C16x_Instruction * instr)
{
	fprintf(stdout, "void ");
	fprintf(stdout, "c16x_%s(uint16_t icode);\n", instr->name);
}

void
print_proto(C16x_Instruction * instr)
{
	fprintf(stdout, "void\n");
	fprintf(stdout, "c16x_%s(uint16_t icode)\n{\n}\n\n", instr->name);
}

print_instrlistentry(C16x_Instruction * instr)
{
	fprintf(stdout, "{0x%02x,0x01,\"%s\",%d,c16x_%s},\n", instr->opcode, instr->name,
		instr->len, instr->name);
	//{0x00,"add_rw_rw",2,NULL},
}

int
main()
{
	C16x_Instruction *instr;
	int i;
	for (i = 0; instrlist[i].name; i++) {
		instr = &instrlist[i];
		//print_instrlistentry(instr);
		print_proto(instr);
		//print_header(instr);
		//fprintf(stderr,"instr %s\n",instr->name);
	}
}
#endif
