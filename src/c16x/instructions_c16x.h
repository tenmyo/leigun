#include <stdint.h>
void C16x_InitInstructions(void);
void c16x_add_rw_rw(uint8_t *icodeP);
void c16x_addb_rb_rb(uint8_t *icodeP);
void c16x_add_reg_mem(uint8_t *icodeP);
void c16x_addb_reg_mem(uint8_t *icodeP);
void c16x_add_mem_reg(uint8_t *icodeP);
void c16x_addb_mem_reg(uint8_t *icodeP);
void c16x_add_reg_data16(uint8_t *icodeP);
void c16x_addb_reg_data8(uint8_t *icodeP);
void c16x_add_rw_x(uint8_t *icodeP);
void c16x_addb_rb_x(uint8_t *icodeP);
void c16x_bfldl_boff_mask8_data8(uint8_t *icodeP);
void c16x_mul_rw_rw(uint8_t *icodeP);
void c16x_rol_rw_rw(uint8_t *icodeP);
void c16x_jmpr_cc_rel(uint8_t *icodeP);
void c16x_bclr(uint8_t *icodeP);
void c16x_bset(uint8_t *icodeP);
void c16x_addc_rw_rw(uint8_t *icodeP);
void c16x_addcb_rb_rb(uint8_t *icodeP);
void c16x_addc_reg_mem(uint8_t *icodeP);
void c16x_addcb_reg_mem(uint8_t *icodeP);
void c16x_addc_mem_reg(uint8_t *icodeP);
void c16x_addcb_mem_reg(uint8_t *icodeP);
void c16x_addc_reg_data16(uint8_t *icodeP);
void c16x_addcb_reg_data8(uint8_t *icodeP);
void c16x_addc_rw_x(uint8_t *icodeP);
void c16x_addcb_rb_x(uint8_t *icodeP);
void c16x_bfldh_boff_mask8_data8(uint8_t *icodeP);
void c16x_mulu_rw_rw(uint8_t *icodeP);
void c16x_rol_rw_data4(uint8_t *icodeP);
void c16x_sub_rw_rw(uint8_t *icodeP);
void c16x_subb_rb_rb(uint8_t *icodeP);
void c16x_sub_reg_mem(uint8_t *icodeP);
void c16x_subb_reg_mem(uint8_t *icodeP);
void c16x_sub_mem_reg(uint8_t *icodeP);
void c16x_subb_mem_reg(uint8_t *icodeP);
void c16x_sub_reg_data16(uint8_t *icodeP);
void c16x_subb_reg_data8(uint8_t *icodeP);
void c16x_sub_rw_x(uint8_t *icodeP);
void c16x_subb_rb_x(uint8_t *icodeP);
void c16x_bcmp_bitaddr_bitaddr(uint8_t *icodeP);
void c16x_prior_rw_rw(uint8_t *icodeP);
void c16x_ror_rw_rw(uint8_t *icodeP);
void c16x_subc_rw_rw(uint8_t *icodeP);
void c16x_subcb_rb_rb(uint8_t *icodeP);
void c16x_subc_reg_mem(uint8_t *icodeP);
void c16x_subcb_reg_mem(uint8_t *icodeP);
void c16x_subc_mem_reg(uint8_t *icodeP);
void c16x_subcb_mem_reg(uint8_t *icodeP);
void c16x_subc_reg_data16(uint8_t *icodeP);
void c16x_subcb_reg_data8(uint8_t *icodeP);
void c16x_subc_rw_x(uint8_t *icodeP);
void c16x_subcb_rb_x(uint8_t *icodeP);
void c16x_bmovn_bitaddr_bitaddr(uint8_t *icodeP);
void c16x_ror_rw_data4(uint8_t *icodeP);
void c16x_cmp_rw_rw(uint8_t *icodeP);
void c16x_cmpb_rb_rb(uint8_t *icodeP);
void c16x_cmp_reg_mem(uint8_t *icodeP);
void c16x_cmpb_reg_mem(uint8_t *icodeP);
void c16x_cmp_reg_data16(uint8_t *icodeP);
void c16x_cmpb_reg_data8(uint8_t *icodeP);
void c16x_cmp_rw_x(uint8_t *icodeP);
void c16x_cmpb_rb_x(uint8_t *icodeP);
void c16x_bmov_bitaddr_bitaddr(uint8_t *icodeP);
void c16x_div_rw(uint8_t *icodeP);
void c16x_shl_rw_rw(uint8_t *icodeP);
void c16x_xor_rw_rw(uint8_t *icodeP);
void c16x_xorb_rb_rb(uint8_t *icodeP);
void c16x_xor_reg_mem(uint8_t *icodeP);
void c16x_xorb_reg_mem(uint8_t *icodeP);
void c16x_xor_mem_reg(uint8_t *icodeP);
void c16x_xorb_mem_reg(uint8_t *icodeP);
void c16x_xor_reg_data16(uint8_t *icodeP);
void c16x_xorb_reg_data8(uint8_t *icodeP);
void c16x_xor_rw_x(uint8_t *icodeP);
void c16x_xorb_rb_x(uint8_t *icodeP);
void c16x_bor_bitaddr_bitaddr(uint8_t *icodeP);
void c16x_divu_rw(uint8_t *icodeP);
void c16x_shl_rw_data4(uint8_t *icodeP);
void c16x_and_rw_rw(uint8_t *icodeP);
void c16x_andb_rb_rb(uint8_t *icodeP);
void c16x_and_reg_mem(uint8_t *icodeP);
void c16x_andb_reg_mem(uint8_t *icodeP);
void c16x_and_mem_reg(uint8_t *icodeP);
void c16x_andb_mem_reg(uint8_t *icodeP);
void c16x_and_reg_data16(uint8_t *icodeP);
void c16x_andb_reg_data8(uint8_t *icodeP);
void c16x_and_rw_x(uint8_t *icodeP);
void c16x_andb_rb_x(uint8_t *icodeP);
void c16x_band_bitaddr_bitaddr(uint8_t *icodeP);
void c16x_divl_rw(uint8_t *icodeP);
void c16x_shr_rw_rw(uint8_t *icodeP);
void c16x_or_rw_rw(uint8_t *icodeP);
void c16x_orb_rb_rb(uint8_t *icodeP);
void c16x_or_reg_mem(uint8_t *icodeP);
void c16x_orb_reg_mem(uint8_t *icodeP);
void c16x_or_mem_reg(uint8_t *icodeP);
void c16x_orb_mem_reg(uint8_t *icodeP);
void c16x_or_reg_data16(uint8_t *icodeP);
void c16x_orb_reg_data8(uint8_t *icodeP);
void c16x_or_rw_x(uint8_t *icodeP);
void c16x_orb_rb_x(uint8_t *icodeP);
void c16x_bxor_bitaddr_bitaddr(uint8_t *icodeP);
void c16x_divlu_rw(uint8_t *icodeP);
void c16x_shr_rw_data4(uint8_t *icodeP);
void c16x_cmpi1_rw_data4(uint8_t *icodeP);
void c16x_neg_rw(uint8_t *icodeP);
void c16x_cmpi1_rw_mem(uint8_t *icodeP);
void c16x_mov__rw__mem(uint8_t *icodeP);
void c16x_cmpi1_rw_data16(uint8_t *icodeP);
void c16x_idle(uint8_t *icodeP);
void c16x_mov__mrw__rw(uint8_t *icodeP);
void c16x_movb__mrw__rb(uint8_t *icodeP);
void c16x_jb_bitaddr_rel(uint8_t *icodeP);
void c16x_cmpi2_rw_data4(uint8_t *icodeP);
void c16x_cpl_rw(uint8_t *icodeP);
void c16x_cmpi2_rw_mem(uint8_t *icodeP);
void c16x_mov_mem__rw_(uint8_t *icodeP);
void c16x_cmpi2_rw_data16(uint8_t *icodeP);
void c16x_pwrdn(uint8_t *icodeP);
void c16x_mov_rw__rwp_(uint8_t *icodeP);
void c16x_movb_rb__rwp_(uint8_t *icodeP);
void c16x_jnb_bitaddr_rel(uint8_t *icodeP);
void c16x_trap_ntrap7(uint8_t *icodeP);
void c16x_jmpi_cc__rw_(uint8_t *icodeP);
void c16x_cmpd1_rw_data4(uint8_t *icodeP);
void c16x_negb_rb(uint8_t *icodeP);
void c16x_cmpd1_rw_mem(uint8_t *icodeP);
void c16x_movb__rw__mem(uint8_t *icodeP);
void c16x_diswdt(uint8_t *icodeP);
void c16x_cmpd1_rw_data16(uint8_t *icodeP);
void c16x_srvwdt(uint8_t *icodeP);
void c16x_mov_rw__rw_(uint8_t *icodeP);
void c16x_movb_rb__rw_(uint8_t *icodeP);
void c16x_jbc_bitaddr_rel(uint8_t *icodeP);
void c16x_calli_cc__rw_(uint8_t *icodeP);
void c16x_ashr_rw_rw(uint8_t *icodeP);
void c16x_cmpd2_rw_data4(uint8_t *icodeP);
void c16x_cplb_rb(uint8_t *icodeP);
void c16x_cmpd2_rw_mem(uint8_t *icodeP);
void c16x_movb_mem__rw_(uint8_t *icodeP);
void c16x_einit(uint8_t *icodeP);
void c16x_cmpd2_rw_data16(uint8_t *icodeP);
void c16x_srst(uint8_t *icodeP);
void c16x_mov__rw__rw(uint8_t *icodeP);
void c16x_movb__rw__rb(uint8_t *icodeP);
void c16x_jnbs_bitaddr_rel(uint8_t *icodeP);
void c16x_callr_rel(uint8_t *icodeP);
void c16x_ashr_rw_data4(uint8_t *icodeP);
void c16x_movbz_rw_rb(uint8_t *icodeP);
void c16x_movbz_reg_mem(uint8_t *icodeP);
void c16x_mov__rwpdata16__rw(uint8_t *icodeP);
void c16x_movbz_mem_reg(uint8_t *icodeP);
void c16x_scxt_reg_data16(uint8_t *icodeP);
void c16x_mov__rw___rw_(uint8_t *icodeP);
void c16x_movb__rw___rw_(uint8_t *icodeP);
void c16x_calla_cc_addr(uint8_t *icodeP);
void c16x_ret(uint8_t *icodeP);
void c16x_nop(uint8_t *icodeP);
void c16x_movbs_rw_rb(uint8_t *icodeP);
void c16x_atomic_extr_irang2(uint8_t *icodeP);
void c16x_movbs_reg_mem(uint8_t *icodeP);
void c16x_mov_rw__rwpdata16_(uint8_t *icodeP);
void c16x_movbs_mem_reg(uint8_t *icodeP);
void c16x_scxt_reg_mem(uint8_t *icodeP);
void c16x_extp_exts_p10(uint8_t *icodeP);
void c16x_mov__rwp___rw_(uint8_t *icodeP);
void c16x_movb__rwp___rw_(uint8_t *icodeP);
void c16x_calls_seg_caddr(uint8_t *icodeP);
void c16x_rets(uint8_t *icodeP);
void c16x_extp_exts_rwirang(uint8_t *icodeP);
void c16x_mov_rw_data4(uint8_t *icodeP);
void c16x_mov_rb_data4(uint8_t *icodeP);
void c16x_pcall_reg_caddr(uint8_t *icodeP);
void c16x_movb__rwpdata16__rb(uint8_t *icodeP);
void c16x_mov_reg_data16(uint8_t *icodeP);
void c16x_movb_reg_data8(uint8_t *icodeP);
void c16x_mov__rw___rwp_(uint8_t *icodeP);
void c16x_movb__rw___rwp_(uint8_t *icodeP);
void c16x_jmpa_cc_caddr(uint8_t *icodeP);
void c16x_retp_reg(uint8_t *icodeP);
void c16x_push_reg(uint8_t *icodeP);
void c16x_mov_rw_rw(uint8_t *icodeP);
void c16x_movb_rb_rb(uint8_t *icodeP);
void c16x_mov_reg_mem(uint8_t *icodeP);
void c16x_movb_reg_mem(uint8_t *icodeP);
void c16x_movb_rb__rwpdata16_(uint8_t *icodeP);
void c16x_mov_mem_reg(uint8_t *icodeP);
void c16x_movb_mem_reg(uint8_t *icodeP);
void c16x_jmps_seg_caddr(uint8_t *icodeP);
void c16x_reti(uint8_t *icodeP);
void c16x_pop_reg(uint8_t *icodeP);
void c16x_illegal_opcode(uint8_t *icodeP);
