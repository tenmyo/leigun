/*
 **********************************************************************************************
 * Renesas RX instruction decoder simulator 
 *
 * Copyright 2012 Jochen Karrer. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification, are
 * permitted provided that the following conditions are met:
 *
 *   1. Redistributions of source code must retain the above copyright notice, this list of
 *       conditions and the following disclaimer.
 *
 *    2. Redistributions in binary form must reproduce the above copyright notice, this list
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
 **********************************************************************************************
 */

#include <stdbool.h>
#include <stdint.h>
#include "sgstring.h"
#include "idecode_rx.h"
#include "byteorder.h"
#include "cpu_rx.h"

RX_Instruction **rxITab;

/*
 *******************************************************************
 * The instruction list
 *******************************************************************
 */
static RX_Instruction instrlist[] = {
	{
	 /* (1) ABS dest */
	 .mask = 0x0000fff0,
	 .icode = 0x00007e20,
	 .name = "abs_dst",
	 .len = 2,
	 .proc = rx_setup_abs_dst},
	{
	 /* (2) ABS src,dest */
	 .mask = 0x00ffff00,
	 .icode = 0x00fc0f00,
	 .name = "abs_src_dst",
	 .len = 3,
	 .proc = rx_setup_abs_src_dst},
	{
	 /* (1) ADC src,dest */
	 .mask = 0x00fff3f0,
	 .icode = 0x00fd7020,
	 .name = "adc_imm_dst",
	 .len = 3,
	 .proc = rx_setup_adc_imm_dst},
	{
	 /* (2) ADC src,dest */
	 .mask = 0x00ffff00,
	 .icode = 0x00fc0b00,
	 .name = "adc_src_dst",
	 .len = 3,
	 .proc = rx_setup_adc_src_dst},
	{
	 /* (3) ADC src dest */
	 .mask = 0xffffff00,
	 .icode = 0x06a00200,
	 .name = "adc_isrc_dst",
	 .len = 4,
	 .proc = rx_adc_isrc_dst},
	{
	 /* (3) ADC src dest */
	 .mask = 0xffffff00,
	 .icode = 0x06a10200,
	 .name = "adc_isrc_dst",
	 .len = 4,
	 .proc = rx_adc_isrc_dst},
	{
	 /* (3) ADC src dest */
	 .mask = 0xffffff00,
	 .icode = 0x06a20200,
	 .name = "adc_isrc_dst",
	 .len = 4,
	 .proc = rx_adc_isrc_dst},
	{
	 /* (1) ADD src dest */
	 .mask = 0x0000ff00,
	 .icode = 0x00006200,
	 .name = "add_uimm_dst",
	 .len = 2,
	 .proc = rx_setup_add_uimm_dst},
/* Signed variants are using the 3 arg opcodes with src = dst */
	{
	 /* (2) ADD src,dest UB */
	 .mask = 0x0000fc00,
	 .icode = 0x00004800,
	 .name = "add_src_dst_ub",
	 .len = 2,
	 .proc = rx_setup_add_src_dst_ub},
	{
	 /* (2) ADD src,dest */
	 .mask = 0x00ff3c00,
	 .icode = 0x00060800,
	 .name = "add_src_dst_nub",
	 .len = 3,
	 .proc = rx_setup_add_src_dst_nub},
	{
	 /* (3) ADD src,src2,dest */
	 .mask = 0x0000fc00,
	 .icode = 0x00007000,
	 .name = "add_simm_src2_dst",
	 .len = 2,
	 .proc = rx_setup_add_simm_src2_dst},
	{
	 /* (4) ADD src,src2,dest */
	 .mask = 0x00fff000,
	 .icode = 0x00ff2000,
	 .name = "add_src_src2_dst",
	 .len = 3,
	 .proc = rx_setup_add_src_src2_dst},
	{
	 /* (1) AND src,dest */
	 .mask = 0x0000ff00,
	 .icode = 0x00006400,
	 .name = "and_uimm_dst",
	 .len = 2,
	 .proc = rx_setup_and_uimm_dst},
/* Signed variants are using the 3 arg opcodes with src = dst */
	{
	 /* (2) AND src,dest */
	 .mask = 0x0000fcf0,
	 .icode = 0x00007420,
	 .name = "and_imm_dst",
	 .len = 2,
	 .proc = rx_setup_and_imm_dst},
	{
	 /* (3) AND src,dest UB */
	 .mask = 0x0000fc00,
	 .icode = 0x00005000,
	 .name = "and_src_dst_ub",
	 .len = 2,
	 .proc = rx_setup_and_src_dst_ub},
	{
	 /* (3) AND src,dest */
	 .mask = 0x00ff3c00,
	 .icode = 0x00061000,
	 .name = "and_src_dst_nub",
	 .len = 3,
	 .proc = rx_setup_and_src_dst_nub},
	{
	 /* (4) AND src,src2,dest */
	 .mask = 0x00fff000,
	 .icode = 0x00ff4000,
	 .name = "and_src_src2_dst",
	 .len = 3,
	 .proc = rx_setup_and_src_src2_dst},
	{
	 /* (1) BCLR src,dest */
	 .mask = 0x0000ff08,
	 .icode = 0x0000f008,
	 .name = "bclr_imm_dst",
	 .len = 2,
	 .proc = rx_setup_bclr_imm_dst},
	{
	 /* (1) BCLR src,dest */
	 .mask = 0x0000ff08,
	 .icode = 0x0000f108,
	 .name = "bclr_imm_dst",
	 .len = 2,
	 .proc = rx_setup_bclr_imm_dst},
	{
	 /* (1) BCLR src,dest */
	 .mask = 0x0000ff08,
	 .icode = 0x0000f208,
	 .name = "bclr_imm_dst",
	 .len = 2,
	 .proc = rx_setup_bclr_imm_dst},
	{
	 /* (2) BCLR src,dest */
	 .mask = 0x00ffff00,
	 .icode = 0x00fc6400,
	 .name = "bclr_rs_dst",
	 .len = 3,
	 .proc = rx_setup_bclr_rs_dst},
	{
	 /* (2) BCLR src,dest */
	 .mask = 0x00ffff00,
	 .icode = 0x00fc6500,
	 .name = "bclr_rs_dst",
	 .len = 3,
	 .proc = rx_setup_bclr_rs_dst},
	{
	 /* (2) BCLR src,dest */
	 .mask = 0x00ffff00,
	 .icode = 0x00fc6600,
	 .name = "bclr_rs_dst",
	 .len = 3,
	 .proc = rx_setup_bclr_rs_dst},
	{
	 /* (3)  BCLR src,dest */
	 .mask = 0x0000fe00,
	 .icode = 0x00007a00,
	 .name = "bclr_imm5_dst",
	 .len = 2,
	 .proc = rx_setup_bclr_imm5_dst},
	{
	 /* (4) BCLR src,dest */
	 .mask = 0x00ffff00,
	 .icode = 0x00fc6700,
	 .name = "bclr_src_dst",
	 .len = 3,
	 .proc = rx_setup_bclr_src_dst},
	{
	 /* (1) BCND */
	 .mask = 0x000000f8,
	 .icode = 0x00000010,
	 .name = "beq_s_src",
	 .len = 1,
	 .proc = rx_setup_beq_s_src},
	{
	 /* (1) BCND */
	 .mask = 0x000000f8,
	 .icode = 0x00000018,
	 .name = "bne_s_src",
	 .len = 1,
	 .proc = rx_setup_bne_s_src},
	{
	 /* (2) BCND BEQ,BZ */
	 .mask = 0x000000ff,
	 .icode = 0x00000020,
	 .name = "bcnd_b_src",
	 .len = 1,
	 .proc = rx_beq_b_src},
	{
	 /* (2) BCND BNE,BNZ */
	 .mask = 0x000000ff,
	 .icode = 0x00000021,
	 .name = "bcnd_b_src",
	 .len = 1,
	 .proc = rx_bne_b_src},
	{
	 /* (2) BGEU, BC */
	 .mask = 0x000000ff,
	 .icode = 0x00000022,
	 .name = "bcnd_b_src",
	 .len = 1,
	 .proc = rx_bgeu_b_src},
	{
	 /* (2) BLTU,BNC */
	 .mask = 0x000000ff,
	 .icode = 0x00000023,
	 .name = "bcnd_b_src",
	 .len = 1,
	 .proc = rx_bltu_b_src},
	{
	 /* (2) BGTU */
	 .mask = 0x000000ff,
	 .icode = 0x00000024,
	 .name = "bcnd_b_src",
	 .len = 1,
	 .proc = rx_bgtu_b_src},
	{
	 /* (2) BLEU */
	 .mask = 0x000000ff,
	 .icode = 0x00000025,
	 .name = "bcnd_b_src",
	 .len = 1,
	 .proc = rx_bleu_b_src},
	{
	 /* (2) BPZ */
	 .mask = 0x000000ff,
	 .icode = 0x00000026,
	 .name = "bcnd_b_src",
	 .len = 1,
	 .proc = rx_bpz_b_src},
	{
	 /* (2) BN */
	 .mask = 0x000000ff,
	 .icode = 0x00000027,
	 .name = "bcnd_b_src",
	 .len = 1,
	 .proc = rx_bn_b_src},
	{
	 /* (2) BGE */
	 .mask = 0x000000ff,
	 .icode = 0x00000028,
	 .name = "bcnd_b_src",
	 .len = 1,
	 .proc = rx_bge_b_src},
	{
	 /* (2) BLT */
	 .mask = 0x000000ff,
	 .icode = 0x00000029,
	 .name = "bcnd_b_src",
	 .len = 1,
	 .proc = rx_blt_b_src},
	{
	 /* (2) BGT */
	 .mask = 0x000000ff,
	 .icode = 0x0000002a,
	 .name = "bcnd_b_src",
	 .len = 1,
	 .proc = rx_bgt_b_src},
	{
	 /* (2) BLE */
	 .mask = 0x000000ff,
	 .icode = 0x0000002b,
	 .name = "bcnd_b_src",
	 .len = 1,
	 .proc = rx_ble_b_src},
	{
	 /* (2) BO */
	 .mask = 0x000000ff,
	 .icode = 0x0000002c,
	 .name = "bcnd_b_src",
	 .len = 1,
	 .proc = rx_bo_b_src},
	{
	 /* (2) BNO */
	 .mask = 0x000000ff,
	 .icode = 0x0000002d,
	 .name = "bcnd_b_src",
	 .len = 1,
	 .proc = rx_bno_b_src},
	{
	 /* (3) BCND.W src  */
	 .mask = 0x000000ff,
	 .icode = 0x0000003a,
	 .name = "bcnd_w_src",
	 .len = 1,
	 .proc = rx_beq_w_src},
	{
	 /* (3) BCND.W src  */
	 .mask = 0x000000ff,
	 .icode = 0x0000003b,
	 .name = "bcnd_w_src",
	 .len = 1,
	 .proc = rx_bne_w_src},
	{
	 /* (1) BMEQ,BMZ src,dest */
	 .mask = 0x00ffe00f,
	 .icode = 0x00fce000,
	 .name = "bmcnd_im3_dst",
	 .len = 3,
	 .proc = rx_setup_bmcnd_imm3_dst},
	{
	 /* (1) BMNE,BMZ */
	 .mask = 0x00ffe00f,
	 .icode = 0x00fce001,
	 .name = "bmcnd_im3_dst",
	 .len = 3,
	 .proc = rx_setup_bmcnd_imm3_dst},
	{
	 /* (1) BMGEU,BMC */
	 .mask = 0x00ffe00f,
	 .icode = 0x00fce002,
	 .name = "bmcnd_im3_dst",
	 .len = 3,
	 .proc = rx_setup_bmcnd_imm3_dst},
	{
	 /* (1) BMLTU,BMNC */
	 .mask = 0x00ffe00f,
	 .icode = 0x00fce003,
	 .name = "bmcnd_im3_dst",
	 .len = 3,
	 .proc = rx_setup_bmcnd_imm3_dst},
	{
	 /* (1) BMGTU */
	 .mask = 0x00ffe00f,
	 .icode = 0x00fce004,
	 .name = "bmcnd_im3_dst",
	 .len = 3,
	 .proc = rx_setup_bmcnd_imm3_dst},
	{
	 /* (1) BMLEU */
	 .mask = 0x00ffe00f,
	 .icode = 0x00fce005,
	 .name = "bmcnd_im3_dst",
	 .len = 3,
	 .proc = rx_setup_bmcnd_imm3_dst},
	{
	 /* (1) BMPZ */
	 .mask = 0x00ffe00f,
	 .icode = 0x00fce006,
	 .name = "bmcnd_im3_dst",
	 .len = 3,
	 .proc = rx_setup_bmcnd_imm3_dst},
	{
	 /* (1) BMN */
	 .mask = 0x00ffe00f,
	 .icode = 0x00fce007,
	 .name = "bmcnd_im3_dst",
	 .len = 3,
	 .proc = rx_setup_bmcnd_imm3_dst},
	{
	 /* (1) BMGE     */
	 .mask = 0x00ffe00f,
	 .icode = 0x00fce008,
	 .name = "bmcnd_im3_dst",
	 .len = 3,
	 .proc = rx_setup_bmcnd_imm3_dst},
	{
	 /* (1) BMLT     */
	 .mask = 0x00ffe00f,
	 .icode = 0x00fce009,
	 .name = "bmcnd_im3_dst",
	 .len = 3,
	 .proc = rx_setup_bmcnd_imm3_dst},
	{
	 /* (1) BMGT     */
	 .mask = 0x00ffe00f,
	 .icode = 0x00fce00a,
	 .name = "bmcnd_im3_dst",
	 .len = 3,
	 .proc = rx_setup_bmcnd_imm3_dst},
	{
	 /* BMLE */
	 .mask = 0x00ffe00f,
	 .icode = 0x00fce00b,
	 .name = "bmcnd_im3_dst",
	 .len = 3,
	 .proc = rx_setup_bmcnd_imm3_dst},
	{
	 /* BMO */
	 .mask = 0x00ffe00f,
	 .icode = 0x00fce00c,
	 .name = "bmcnd_im3_dst",
	 .len = 3,
	 .proc = rx_setup_bmcnd_imm3_dst},
	{
	 /* BMNO */
	 .mask = 0x00ffe00f,
	 .icode = 0x00fce00d,
	 .name = "bmcnd_im3_dst",
	 .len = 3,
	 .proc = rx_setup_bmcnd_imm3_dst},
	{
	 /* (2)  BMEQ,BMZ */
	 .mask = 0x00ffe0f0,
	 .icode = 0x00fde000,
	 .name = "bmcnd_imm5_dst",
	 .len = 3,
	 .proc = rx_setup_bmcnd_src_dst},
	{
	 /* (2) BMNE,BMNZ */
	 .mask = 0x00ffe0f0,
	 .icode = 0x00fde010,
	 .name = "bmcnd_imm5_dst",
	 .len = 3,
	 .proc = rx_setup_bmcnd_src_dst},
	{
	 /* (2) BMGEU,BMC */
	 .mask = 0x00ffe0f0,
	 .icode = 0x00fde020,
	 .name = "bmcnd_imm5_dst",
	 .len = 3,
	 .proc = rx_setup_bmcnd_src_dst},
	{
	 /* (2) BMLTU,BMNC */
	 .mask = 0x00ffe0f0,
	 .icode = 0x00fde030,
	 .name = "bmcnd_imm5_dst",
	 .len = 3,
	 .proc = rx_setup_bmcnd_src_dst},
	{
	 /* (2) BMGTU */
	 .mask = 0x00ffe0f0,
	 .icode = 0x00fde040,
	 .name = "bmcnd_imm5_dst",
	 .len = 3,
	 .proc = rx_setup_bmcnd_src_dst},
	{
	 /* (2) BMLEU */
	 .mask = 0x00ffe0f0,
	 .icode = 0x00fde050,
	 .name = "bmcnd_imm5_dst",
	 .len = 3,
	 .proc = rx_setup_bmcnd_src_dst},
	{
	 /* (2) BMPZ */
	 .mask = 0x00ffe0f0,
	 .icode = 0x00fde060,
	 .name = "bmcnd_imm5_dst",
	 .len = 3,
	 .proc = rx_setup_bmcnd_src_dst},
	{
	 /* (2) BMN */
	 .mask = 0x00ffe0f0,
	 .icode = 0x00fde070,
	 .name = "bmcnd_imm5_dst",
	 .len = 3,
	 .proc = rx_setup_bmcnd_src_dst},
	{
	 /* (2) BMGE */
	 .mask = 0x00ffe0f0,
	 .icode = 0x00fde080,
	 .name = "bmcnd_imm5_dst",
	 .len = 3,
	 .proc = rx_setup_bmcnd_src_dst},
	{
	 /* BMLT */
	 .mask = 0x00ffe0f0,
	 .icode = 0x00fde090,
	 .name = "bmcnd_imm5_dst",
	 .len = 3,
	 .proc = rx_setup_bmcnd_src_dst},
	{
	 /* BMGT */
	 .mask = 0x00ffe0f0,
	 .icode = 0x00fde0a0,
	 .name = "bmcnd_imm5_dst",
	 .len = 3,
	 .proc = rx_setup_bmcnd_src_dst},
	{
	 /* BMLE */
	 .mask = 0x00ffe0f0,
	 .icode = 0x00fde0b0,
	 .name = "bmcnd_imm5_dst",
	 .len = 3,
	 .proc = rx_setup_bmcnd_src_dst},
	{
	 /* BMO */
	 .mask = 0x00ffe0f0,
	 .icode = 0x00fde0c0,
	 .name = "bmcnd_imm5_dst",
	 .len = 3,
	 .proc = rx_setup_bmcnd_src_dst},
	{
	 /* BMNO */
	 .mask = 0x00ffe0f0,
	 .icode = 0x00fde0d0,
	 .name = "bmcnd_imm5_dst",
	 .len = 3,
	 .proc = rx_setup_bmcnd_src_dst},
	{
	 /* (1) BNOT src,dest */
	 .mask = 0x00ffe30f,
	 .icode = 0x00fce00f,
	 .name = "bnot_im3_dst",
	 .len = 3,
	 .proc = rx_setup_bnot_im3_dst},
	{
	 /* (1) BNOT src,dest */
	 .mask = 0x00ffe30f,
	 .icode = 0x00fce10f,
	 .name = "bnot_im3_dst",
	 .len = 3,
	 .proc = rx_setup_bnot_im3_dst},
	{
	 /* (1) BNOT src,dest */
	 .mask = 0x00ffe30f,
	 .icode = 0x00fce20f,
	 .name = "bnot_im3_dst",
	 .len = 3,
	 .proc = rx_setup_bnot_im3_dst},
	{
	 /* (2) BNOT src,dest */
	 .mask = 0x00ffff00,
	 .icode = 0x00fc6c00,
	 .name = "bnot_rs_dst",
	 .len = 3,
	 .proc = rx_setup_bnot_rs_dst},
	{
	 /* (2) BNOT src,dest */
	 .mask = 0x00ffff00,
	 .icode = 0x00fc6d00,
	 .name = "bnot_rs_dst",
	 .len = 3,
	 .proc = rx_setup_bnot_rs_dst},
	{
	 /* (2) BNOT src,dest */
	 .mask = 0x00ffff00,
	 .icode = 0x00fc6e00,
	 .name = "bnot_rs_dst",
	 .len = 3,
	 .proc = rx_setup_bnot_rs_dst},
	{
	 /* (3) BNOT src,dest */
	 .mask = 0x00ffe0f0,
	 .icode = 0x00fde0f0,
	 .name = "bnot_imm5_rd",
	 .len = 3,
	 .proc = rx_setup_bnot_imm5_rd},
	{
	 /* (4) BNOT src,dest */
	 .mask = 0x00ffff00,
	 .icode = 0x00fc6f00,
	 .name = "bnot_rs_rd",
	 .len = 3,
	 .proc = rx_setup_bnot_rs_rd},
	{
	 /* (1) BRA.S src */
	 .mask = 0x000000f8,
	 .icode = 0x00000008,
	 .name = "bra_s_src",
	 .len = 1,
	 .proc = rx_setup_bra_s_src},
	{
	 /* (2) BRA.B src */
	 .mask = 0x000000ff,
	 .icode = 0x0000002e,
	 .name = "bra_b_src",
	 .len = 1,
	 .proc = rx_bra_b_src},
	{
	 /* (3) BRA.W src */
	 .mask = 0x000000ff,
	 .icode = 0x00000038,
	 .name = "bra_w_src",
	 .len = 1,
	 .proc = rx_bra_w_src},
	{
	 /* (4) BRA.A src */
	 .mask = 0x000000ff,
	 .icode = 0x00000004,
	 .name = "bra_a_src",
	 .len = 1,
	 .proc = rx_bra_a_src},
	{
	 /* (5) BRA.L src */
	 .mask = 0x0000fff0,
	 .icode = 0x00007f40,
	 .name = "bra_l_src",
	 .len = 2,
	 .proc = rx_bra_l_src},
	{
	 /* BRK */
	 .mask = 0x000000ff,
	 .icode = 0x00000000,
	 .name = "brk",
	 .len = 1,
	 .proc = rx_brk},
	{
	 /* (1) BSET src,dest */
	 .mask = 0x0000ff08,
	 .icode = 0x0000f000,
	 .name = "bset_imm3_dst",
	 .len = 2,
	 .proc = rx_setup_bset_imm3_dst},
	{
	 /* (1) BSET src,dest */
	 .mask = 0x0000ff08,
	 .icode = 0x0000f100,
	 .name = "bset_imm3_dst",
	 .len = 2,
	 .proc = rx_setup_bset_imm3_dst},
	{
	 /* (1) BSET src,dest */
	 .mask = 0x0000ff08,
	 .icode = 0x0000f200,
	 .name = "bset_imm3_dst",
	 .len = 2,
	 .proc = rx_setup_bset_imm3_dst},
	{
	 /* (2) BSET src,dest */
	 .mask = 0x00ffff00,
	 .icode = 0x00fc6000,
	 .name = "bset_rs_dst",
	 .len = 3,
	 .proc = rx_setup_bset_rs_dst},
	{
	 /* (2) BSET src,dest */
	 .mask = 0x00ffff00,
	 .icode = 0x00fc6100,
	 .name = "bset_rs_dst",
	 .len = 3,
	 .proc = rx_setup_bset_rs_dst},
	{
	 /* (2) BSET src,dest */
	 .mask = 0x00ffff00,
	 .icode = 0x00fc6200,
	 .name = "bset_rs_dst",
	 .len = 3,
	 .proc = rx_setup_bset_rs_dst},
	{
	 /* (3) BSET src,dest */
	 .mask = 0x0000fe00,
	 .icode = 0x00007800,
	 .name = "bset_imm5_dst",
	 .len = 2,
	 .proc = rx_setup_bset_imm5_dst},
	{
	 /* (4) BSET src,dest */
	 .mask = 0x00ffff00,
	 .icode = 0x00fc6300,
	 .name = "bset_rs_rd",
	 .len = 3,
	 .proc = rx_setup_bset_rs_rd},
	{
	 /* (1) BSR.W */
	 .mask = 0x000000ff,
	 .icode = 0x00000039,
	 .name = "bsr_w_src",
	 .len = 1,
	 .proc = rx_bsr_w_src},
	{
	 /* (2) BSR.A src */
	 .mask = 0x000000ff,
	 .icode = 0x00000005,
	 .name = "bsr_a_src",
	 .len = 1,
	 .proc = rx_bsr_a_src},
	{
	 /* (3) BSR.L src */
	 .mask = 0x0000fff0,
	 .icode = 0x00007f50,
	 .name = "bsr_l_src",
	 .len = 2,
	 .proc = rx_setup_bsr_l_src},
	{
	 /* (1) BTST src,src2 */
	 .mask = 0x0000ff08,
	 .icode = 0x0000f400,
	 .name = "btst_imm3_src2",
	 .len = 2,
	 .proc = rx_setup_btst_imm3_src2},
	{
	 /* (1) BTST src,src2 */
	 .mask = 0x0000ff08,
	 .icode = 0x0000f500,
	 .name = "btst_imm3_src2",
	 .len = 2,
	 .proc = rx_setup_btst_imm3_src2},
	{
	 /* (1) BTST src,src2 */
	 .mask = 0x0000ff08,
	 .icode = 0x0000f600,
	 .name = "btst_imm3_src2",
	 .len = 2,
	 .proc = rx_setup_btst_imm3_src2},
	{
	 /* (2) BTST src,src2 */
	 .mask = 0x00ffff00,
	 .icode = 0x00fc6800,
	 .name = "btst_rs_src2",
	 .len = 3,
	 .proc = rx_setup_btst_rs_src2},
	{
	 /* (2) BTST src,src2 */
	 .mask = 0x00ffff00,
	 .icode = 0x00fc6900,
	 .name = "btst_rs_src2",
	 .len = 3,
	 .proc = rx_setup_btst_rs_src2},
	{
	 /* (2) BTST src,src2 */
	 .mask = 0x00ffff00,
	 .icode = 0x00fc6a00,
	 .name = "btst_rs_src2",
	 .len = 3,
	 .proc = rx_setup_btst_rs_src2},
	{
	 /* (3) BTST src,src2 */
	 .mask = 0x0000fe00,
	 .icode = 0x00007c00,
	 .name = "btst_imm5_src2",
	 .len = 2,
	 .proc = rx_setup_btst_imm5_src2},
	{
	 /* (4) BTST src,src2 */
	 .mask = 0x00ffff00,
	 .icode = 0x00fc6b00,
	 .name = "btst_rs_rs2",
	 .len = 3,
	 .proc = rx_setup_btst_rs_rs2},
	{
	 /* (1) CLRPSW dest */
	 .mask = 0x0000fff0,
	 .icode = 0x00007fb0,
	 .name = "clrpsw",
	 .len = 2,
	 .proc = rx_setup_clrpsw},
	{
	 /* (1) CMP src,src2 */
	 .mask = 0x0000ff00,
	 .icode = 0x00006100,
	 .name = "cmp_uimm4_rs",
	 .len = 2,
	 .proc = rx_setup_cmp_uimm4_rs},
	{
	 /* (2) CMP src,src2 */
	 .mask = 0x0000fff0,
	 .icode = 0x00007550,
	 .name = "cmp_uimm8_rs",
	 .len = 2,
	 .proc = rx_setup_cmp_uimm8_rs},
	{
	 /* (3) CMP src,src2 */
	 .mask = 0x0000fff0,
	 .icode = 0x00007400,
	 .name = "cmp_imm32_rs",
	 .len = 2,
	 .proc = rx_setup_cmp_imm32_rs},
	{
	 /* (3) CMP src,src2 */
	 .mask = 0x0000fff0,
	 .icode = 0x00007500,
	 .name = "cmp_imm32_rs",
	 .len = 2,
	 .proc = rx_setup_cmp_simm8_rs},
	{
	 /* (3) CMP src,src2 */
	 .mask = 0x0000fff0,
	 .icode = 0x00007600,
	 .name = "cmp_simm16_rs",
	 .len = 2,
	 .proc = rx_setup_cmp_simm16_rs},
	{
	 /* (3) CMP src,src2 */
	 .mask = 0x0000fff0,
	 .icode = 0x00007700,
	 .name = "cmp_simm24_rs",
	 .len = 2,
	 .proc = rx_setup_cmp_simm24_rs},
	{
	 /* (4) CMP src,src2 */
	 .mask = 0x0000fc00,
	 .icode = 0x00004400,
	 .name = "cmp_ubrs_src2",
	 .len = 2,
	 .proc = rx_setup_cmp_ubrs_src2},
	{
	 /* (4) CMP src,src2 */
	 .mask = 0x00ff3c00,
	 .icode = 0x00060400,
	 .name = "cmp_memex_src2",
	 .len = 3,
	 .proc = rx_setup_cmp_memex_src2},
	{
	 /* (1) DIV src,dest */
	 .mask = 0x00fff3f0,
	 .icode = 0x00fd7080,
	 .name = "div_imm_dst",
	 .len = 3,
	 .proc = rx_setup_div_imm_dst},
	{
	 /* (2) DIV src,dest */
	 .mask = 0x00fffc00,
	 .icode = 0x00fc2000,
	 .name = "div_ubrs_dst",
	 .len = 3,
	 .proc = rx_setup_div_ubrs_dst},
	{
	 /* (2) DIV src,dest */
	 .mask = 0xff3cff00,
	 .icode = 0x06200800,
	 .name = "div_memex_dst",
	 .len = 4,
	 .proc = rx_div_memex_dst},
	{
	 /* (1) DIVU src,dest */
	 .mask = 0x00fff3f0,
	 .icode = 0x00fd7090,
	 .name = "divu_imm_dst",
	 .len = 3,
	 .proc = rx_setup_divu_imm_dst},
	{
	 /* (2) DIVU src,dest */
	 .mask = 0x00fffc00,
	 .icode = 0x00fc2400,
	 .name = "divu_ubrs_dst",
	 .len = 3,
	 .proc = rx_setup_divu_ubrs_dst},
	{
	 /* (2) DIVU src,dest */
	 .mask = 0xff3cff00,
	 .icode = 0x06200900,
	 .name = "divu_memex_dst",
	 .len = 4,
	 .proc = rx_divu_memex_dst},
	{
	 /* (1) EMUL src,dest */
	 .mask = 0x00fff3f0,
	 .icode = 0x00fd7060,
	 .name = "emul_imm_dst",
	 .len = 3,
	 .proc = rx_setup_emul_imm_dst},
	{
	 /* (2) EMUL src dest */
	 .mask = 0x00fffc00,
	 .icode = 0x00fc1800,
	 .name = "emul_ubrs_dst",
	 .len = 3,
	 .proc = rx_setup_emul_ubrs_dst},
	{
	 /* (2) EMUL src,dest */
	 .mask = 0xff3cff00,
	 .icode = 0x06200600,
	 .name = "emul_memex_dst",
	 .len = 4,
	 .proc = rx_emul_memex_dst},
	{
	 /* (1) EMULU src,dest */
	 .mask = 0x00fff3f0,
	 .icode = 0x00fd7070,
	 .name = "emulu_imm_dst",
	 .len = 3,
	 .proc = rx_setup_emulu_imm_dst},
	{
	 /* (2) EMULU src,dest */
	 .mask = 0x00fffc00,
	 .icode = 0x00fc1c00,
	 .name = "emulu_ubrs_dst",
	 .len = 3,
	 .proc = rx_setup_emulu_ubrs_dst},
	{
	 /* (2) EMULU src,dest */
	 .mask = 0xff3cff00,
	 .icode = 0x06200700,
	 .name = "emulu_memex_dst",
	 .len = 4,
	 .proc = rx_emulu_memex_dst},
	{
	 /* (1) FADD src,dest */
	 .mask = 0x00fffff0,
	 .icode = 0x00fd7220,
	 .name = "fadd_imm32_rd",
	 .len = 3,
	 .proc = rx_setup_fadd_imm32_rd},
	{
	 /* (2) FADD src,dest */
	 .mask = 0x00fffc00,
	 .icode = 0x00fc8800,
	 .name = "fadd_src_dst",
	 .len = 3,
	 .proc = rx_setup_fadd_src_dst},
	{
	 /* (1) FCMP src,src2 */
	 .mask = 0x00fffff0,
	 .icode = 0x00fd7210,
	 .name = "fcmp_imm32_rs2",
	 .len = 3,
	 .proc = rx_setup_fcmp_imm32_rs2},
	{
	 /* (2) FCMP src,src2 */
	 .mask = 0x00fffc00,
	 .icode = 0x00fc8400,
	 .name = "fcmp_src_src2",
	 .len = 3,
	 .proc = rx_setup_fcmp_src_src2},
	{
	 /* (1) FDIV */
	 .mask = 0x00fffff0,
	 .icode = 0x00fd7240,
	 .name = "fdiv_imm32_rd",
	 .len = 3,
	 .proc = rx_setup_fdiv_imm32_rd},
	{
	 /* (2) FDIV */
	 .mask = 0x00fffc00,
	 .icode = 0x00fc9000,
	 .name = "fdiv_src_dst",
	 .len = 3,
	 .proc = rx_setup_fdiv_src_dst},
	{
	 /* (1) FMUL */
	 .mask = 0x00fffff0,
	 .icode = 0x00fd7230,
	 .name = "fmul_imm32_rd",
	 .len = 3,
	 .proc = rx_setup_fmul_imm32_rd},
	{
	 /* (2) FMUL */
	 .mask = 0x00fffc00,
	 .icode = 0x00fc8c00,
	 .name = "fmul_src_dst",
	 .len = 3,
	 .proc = rx_setup_fmul_src_dst},
	{
	 /* (1) FSUB src,dest */
	 .mask = 0x00fffff0,
	 .icode = 0x00fd7200,
	 .name = "fsub_imm32_dst",
	 .len = 3,
	 .proc = rx_setup_fsub_imm32_dst},
	{
	 /* (2) FSUB src,dest */
	 .mask = 0x00fffc00,
	 .icode = 0x00fc8000,
	 .name = "fsub_src_dst",
	 .len = 3,
	 .proc = rx_setup_fsub_src_dst},
	{
	 /* (1) FTOI src,dest */
	 .mask = 0x00fffc00,
	 .icode = 0x00fc9400,
	 .name = "ftoi_src_dst",
	 .len = 3,
	 .proc = rx_setup_ftoi_src_dst},
	{
	 /* (1) INT */
	 .mask = 0x0000ffff,
	 .icode = 0x00007560,
	 .name = "int",
	 .len = 2,
	 .proc = rx_int},
	{
	 /* (1) ITOF */
	 .mask = 0x00fffc00,
	 .icode = 0x00fc4400,
	 .name = "itof_ubrs_rd",
	 .len = 3,
	 .proc = rx_setup_itof_ubrs_rd},
	{
	 /* (2) ITOF */
	 .mask = 0xff3cff00,
	 .icode = 0x06201100,
	 .name = "itof_src_dst",
	 .len = 4,
	 .proc = rx_itof_src_dst},
	{
	 /* (1) JMP */
	 .mask = 0x0000fff0,
	 .icode = 0x00007f00,
	 .name = "jmp_rs",
	 .len = 2,
	 .proc = rx_setup_jmp_rs},
	{
	 /* (1) JSR */
	 .mask = 0x0000fff0,
	 .icode = 0x00007f10,
	 .name = "jsr_rs",
	 .len = 2,
	 .proc = rx_setup_jsr_rs},
	{
	 /* (1) MACHI src,src2 */
	 .mask = 0x00ffff00,
	 .icode = 0x00fd0400,
	 .name = "machi",
	 .len = 3,
	 .proc = rx_setup_machi},
	{
	 /* (1) MACLO src,src2 */
	 .mask = 0x00ffff00,
	 .icode = 0x00fd0500,
	 .name = "maclo",
	 .len = 3,
	 .proc = rx_setup_maclo},
	{
	 /* (1) MAX src,dest */
	 .mask = 0x00fff3f0,
	 .icode = 0x00fd7040,
	 .name = "max_simm_rd",
	 .len = 3,
	 .proc = rx_setup_max_simm_rd},
	{
	 /* (2) MAX src,dest */
	 .mask = 0x00fffc00,
	 .icode = 0x00fc1000,
	 .name = "max_srcub_dst",
	 .len = 3,
	 .proc = rx_setup_max_srcub_dst},
	{
	 /* (2) MAX src,dest */
	 .mask = 0xff3cff00,
	 .icode = 0x06200400,
	 .name = "max_src_dst",
	 .len = 4,
	 .proc = rx_max_src_dst},
	{
	 /* (1) MIN src,dest */
	 .mask = 0x00fff3f0,
	 .icode = 0x00fd7050,
	 .name = "min_simm_rd",
	 .len = 3,
	 .proc = rx_setup_min_simm_rd},
	{
	 /* (2) MIN src,dest */
	 .mask = 0x00fffc00,
	 .icode = 0x00fc1400,
	 .name = "min_srcub_dst",
	 .len = 3,
	 .proc = rx_setup_min_srcub_rd},
	{
	 /* (2) MIN src,dest */
	 .mask = 0xff3cff00,
	 .icode = 0x06200500,
	 .name = "min_src_dst",
	 .len = 4,
	 .proc = rx_min_src_dst},
	{
	 /* (1) MOV src,dst */
	 .mask = 0x0000f800,
	 .icode = 0x00008000,
	 .name = "mov_rs_dsp5_ird",
	 .len = 2,
	 .proc = rx_setup_mov_rs_dsp5_ird},
	{
	 /* (1) MOV src,dst */
	 .mask = 0x0000f800,
	 .icode = 0x00009000,
	 .name = "mov_irs_dsp5_rd",
	 .len = 2,
	 .proc = rx_setup_mov_rs_dsp5_ird},
	{
	 /* (1) MOV src,dst */
	 .mask = 0x0000f800,
	 .icode = 0x0000a000,
	 .name = "mov_rs_dsp5_rd",
	 .len = 2,
	 //.proc = rx_mov_rs_dsp5_rd
	 .proc = rx_setup_mov_rs_dsp5_ird},
	{
	 /* (2) MOV src,dst */
	 .mask = 0x0000f800,
	 .icode = 0x00008800,
	 .name = "mov_dsp5_rs_rd",
	 .len = 2,
	 //.proc = rx_mov_dsp5_rs_rd
	 .proc = rx_setup_mov_dsp5_irs_rd},
	{
	 /* (2) MOV src,dst */
	 .mask = 0x0000f800,
	 .icode = 0x00009800,
	 .name = "mov_dsp5_rs_rd",
	 .len = 2,
	 //.proc = rx_mov_dsp5_rs_rd
	 .proc = rx_setup_mov_dsp5_irs_rd},
	{
	 /* (2) MOV src,dst */
	 .mask = 0x0000f800,
	 .icode = 0x0000a800,
	 .name = "mov_dsp5_rs_rd",
	 .len = 2,
	 //.proc = rx_mov_dsp5_rs_rd
	 .proc = rx_setup_mov_dsp5_irs_rd},
	{
	 /* (3) MOV src,dest */
	 .mask = 0x0000ff00,
	 .icode = 0x00006600,
	 .name = "mov_uimm4_rd",
	 .len = 2,
	 //.proc = rx_mov_uimm4_rd
	 .proc = rx_setup_mov_uimm4_rd},
	{
	 /* (4) MOV src,dest */
	 .mask = 0x0000ff00,
	 .icode = 0x00003c00,
	 .name = "mov_imm8_dsp5_rd",
	 .len = 2,
	 //.proc = rx_mov_imm8_dsp5_rd
	 .proc = rx_setup_mov_imm8_dsp5_rd},
	{
	 /* (4) MOV src,dest */
	 .mask = 0x0000ff00,
	 .icode = 0x00003d00,
	 .name = "mov_imm8_dsp5_rd",
	 .len = 2,
	 //.proc = rx_mov_imm8_dsp5_rd
	 .proc = rx_setup_mov_imm8_dsp5_rd},
	{
	 /* (4) MOV src,dest */
	 .mask = 0x0000ff00,
	 .icode = 0x00003e00,
	 .name = "mov_imm8_dsp5_rd",
	 .len = 2,
	 //.proc = rx_mov_imm8_dsp5_rd
	 .proc = rx_setup_mov_imm8_dsp5_rd},
	{
	 /* (5) MOV src,dest */
	 .mask = 0x0000fff0,
	 .icode = 0x00007540,
	 .name = "mov_uimm8_rd",
	 .len = 2,
	 .proc = rx_setup_mov_uimm8_rd},
	{
	 /* (6) MOV src,dest */
	 .mask = 0x0000ff03,
	 .icode = 0x0000fb02,
	 .name = "mov_simm_rd",
	 .len = 2,
	 .proc = rx_setup_mov_simm_rd},
	{
	 /* (7) MOV src,dest */
	 .mask = 0x0000ff00,
	 .icode = 0x0000cf00,
	 .name = "mov_rs_rd",
	 .len = 2,
	 .proc = rx_setup_mov_rs_rd},
	{
	 /* (7) MOV src,dest */
	 .mask = 0x0000ff00,
	 .icode = 0x0000df00,
	 .name = "mov_rs_rd",
	 .len = 2,
	 .proc = rx_setup_mov_rs_rd},
	{
	 /* (7) MOV src,dest */
	 .mask = 0x0000ff00,
	 .icode = 0x0000ef00,
	 .name = "mov_rs_rd",
	 .len = 2,
	 .proc = rx_setup_mov_rs_rd},
	{
	 /* (8) MOV src,dest */
	 .mask = 0x0000ff03,
	 .icode = 0x0000f800,
	 .name = "mov_simm_dsp_ird",
	 .len = 2,
	 .proc = rx_setup_mov_simm_dsp_ird},
	{
	 /* (8) MOV src,dest */
	 .mask = 0x0000ff03,
	 .icode = 0x0000f900,
	 .name = "mov_simm_dsp_ird",
	 .len = 2,
	 .proc = rx_setup_mov_simm_dsp_ird},
	{
	 /* (8) MOV src,dest */
	 .mask = 0x0000ff03,
	 .icode = 0x0000fa00,
	 .name = "mov_simm_dsp_ird",
	 .len = 2,
	 .proc = rx_setup_mov_simm_dsp_ird},
	{
	 /* (8) MOV src,dest */
	 .mask = 0x0000ff03,
	 .icode = 0x0000f801,
	 .name = "mov_simm_dsp_ird",
	 .len = 2,
	 .proc = rx_setup_mov_simm_dsp_ird},
	{
	 /* (8) MOV src,dest */
	 .mask = 0x0000ff03,
	 .icode = 0x0000f901,
	 .name = "mov_simm_dsp_ird",
	 .len = 2,
	 .proc = rx_setup_mov_simm_dsp_ird},
	{
	 /* (8) MOV src,dest */
	 .mask = 0x0000ff03,
	 .icode = 0x0000fa01,
	 .name = "mov_simm_dsp_ird",
	 .len = 2,
	 .proc = rx_setup_mov_simm_dsp_ird},
	{
	 /* (8) MOV src,dest */
	 .mask = 0x0000ff03,
	 .icode = 0x0000f802,
	 .name = "mov_simm_dsp_ird",
	 .len = 2,
	 .proc = rx_setup_mov_simm_dsp_ird},
	{
	 /* (8) MOV src,dest */
	 .mask = 0x0000ff03,
	 .icode = 0x0000f902,
	 .name = "mov_simm_dsp_ird",
	 .len = 2,
	 .proc = rx_setup_mov_simm_dsp_ird},
	{
	 /* (8) MOV src,dest */
	 .mask = 0x0000ff03,
	 .icode = 0x0000fa02,
	 .name = "mov_simm_dsp_ird",
	 .len = 2,
	 .proc = rx_setup_mov_simm_dsp_ird},
	{
	 /* (9) MOV src,dest */
	 .mask = 0x0000ff00,
	 .icode = 0x0000cc00,
	 .name = "mov_dsp_irs_rd",
	 .len = 2,
	 .proc = rx_setup_mov_dsp_irs_rd},
	{
	 /* (9) MOV src,dest */
	 .mask = 0x0000ff00,
	 .icode = 0x0000cd00,
	 .name = "mov_dsp_irs_rd",
	 .len = 2,
	 .proc = rx_setup_mov_dsp_irs_rd},
	{
	 /* (9) MOV src,dest */
	 .mask = 0x0000ff00,
	 .icode = 0x0000ce00,
	 .name = "mov_dsp_irs_rd",
	 .len = 2,
	 .proc = rx_setup_mov_dsp_irs_rd},
	{
	 /* (9) MOV src,dest */
	 .mask = 0x0000ff00,
	 .icode = 0x0000dc00,
	 .name = "mov_dsp_irs_rd",
	 .len = 2,
	 .proc = rx_setup_mov_dsp_irs_rd},
	{
	 /* (9) MOV src,dest */
	 .mask = 0x0000ff00,
	 .icode = 0x0000dd00,
	 .name = "mov_dsp_irs_rd",
	 .len = 2,
	 .proc = rx_setup_mov_dsp_irs_rd},
	{
	 /* (9) MOV src,dest */
	 .mask = 0x0000ff00,
	 .icode = 0x0000de00,
	 .name = "mov_dsp_irs_rd",
	 .len = 2,
	 .proc = rx_setup_mov_dsp_irs_rd},
	{
	 /* (9) MOV src,dest */
	 .mask = 0x0000ff00,
	 .icode = 0x0000ec00,
	 .name = "mov_dsp_irs_rd",
	 .len = 2,
	 .proc = rx_setup_mov_dsp_irs_rd},
	{
	 /* (9) MOV src,dest */
	 .mask = 0x0000ff00,
	 .icode = 0x0000ed00,
	 .name = "mov_dsp_irs_rd",
	 .len = 2,
	 .proc = rx_setup_mov_dsp_irs_rd},
	{
	 /* (9) MOV src,dest */
	 .mask = 0x0000ff00,
	 .icode = 0x0000ee00,
	 .name = "mov_dsp_irs_rd",
	 .len = 2,
	 .proc = rx_setup_mov_dsp_irs_rd},
	{
	 /* (10) MOV src,dest */
	 .mask = 0x00fff000,
	 .icode = 0x00fe4000,
	 .name = "mov_irirb_rd",
	 .len = 3,
	 .proc = rx_setup_mov_irirb_rd},
	{
	 /* (10) MOV src,dest */
	 .mask = 0x00fff000,
	 .icode = 0x00fe5000,
	 .name = "mov_irirb_rd",
	 .len = 3,
	 .proc = rx_setup_mov_irirb_rd},
	{
	 /* (10) MOV src,dest */
	 .mask = 0x00fff000,
	 .icode = 0x00fe6000,
	 .name = "mov_irirb_rd",
	 .len = 3,
	 .proc = rx_setup_mov_irirb_rd},
	{
	 /* (11) MOV src,dest */
	 .mask = 0x0000ff00,
	 .icode = 0x0000c300,
	 .name = "mov_rs_dsp_ird",
	 .len = 2,
	 .proc = rx_setup_mov_rs_dsp_ird},
	{
	 /* (11) MOV src,dest */
	 .mask = 0x0000ff00,
	 .icode = 0x0000c700,
	 .name = "mov_rs_dsp_ird",
	 .len = 2,
	 .proc = rx_setup_mov_rs_dsp_ird},
	{
	 /* (11) MOV src,dest */
	 .mask = 0x0000ff00,
	 .icode = 0x0000cb00,
	 .name = "mov_rs_dsp_ird",
	 .len = 2,
	 .proc = rx_setup_mov_rs_dsp_ird},
	{
	 /* (11) MOV src,dest */
	 .mask = 0x0000ff00,
	 .icode = 0x0000d300,
	 .name = "mov_rs_dsp_ird",
	 .len = 2,
	 .proc = rx_setup_mov_rs_dsp_ird},
	{
	 /* (11) MOV src,dest */
	 .mask = 0x0000ff00,
	 .icode = 0x0000d700,
	 .name = "mov_rs_dsp_ird",
	 .len = 2,
	 .proc = rx_setup_mov_rs_dsp_ird},
	{
	 /* (11) MOV src,dest */
	 .mask = 0x0000ff00,
	 .icode = 0x0000db00,
	 .name = "mov_rs_dsp_ird",
	 .len = 2,
	 .proc = rx_setup_mov_rs_dsp_ird},
	{
	 /* (11) MOV src,dest */
	 .mask = 0x0000ff00,
	 .icode = 0x0000e300,
	 .name = "mov_rs_dsp_ird",
	 .len = 2,
	 .proc = rx_setup_mov_rs_dsp_ird},
	{
	 /* (11) MOV src,dest */
	 .mask = 0x0000ff00,
	 .icode = 0x0000e700,
	 .name = "mov_rs_dsp_ird",
	 .len = 2,
	 .proc = rx_setup_mov_rs_dsp_ird},
	{
	 /* (11) MOV src,dest */
	 .mask = 0x0000ff00,
	 .icode = 0x0000eb00,
	 .name = "mov_rs_dsp_ird",
	 .len = 2,
	 .proc = rx_setup_mov_rs_dsp_ird},

	{
	 /* (12) MOV src,dest */
	 .mask = 0x00fff000,
	 .icode = 0x00fe0000,
	 .name = "mov_rs_irird",
	 .len = 3,
	 .proc = rx_setup_mov_rs_irird},
	{
	 /* (12) MOV src,dest */
	 .mask = 0x00fff000,
	 .icode = 0x00fe1000,
	 .name = "mov_rs_irird",
	 .len = 3,
	 .proc = rx_setup_mov_rs_irird},
	{
	 /* (12) MOV src,dest */
	 .mask = 0x00fff000,
	 .icode = 0x00fe2000,
	 .name = "mov_rs_irird",
	 .len = 3,
	 .proc = rx_setup_mov_rs_irird},
	{
	 /* (13) MOV src,dst */
	 .mask = 0x0000ff00,
	 .icode = 0x0000c000,
	 .name = "mov_dsp_irs_dsp_ird",
	 .len = 2,
	 .proc = rx_setup_mov_dsp_irs_dsp_ird},
	{
	 /* (13) MOV src,dst */
	 .mask = 0x0000ff00,
	 .icode = 0x0000c100,
	 .name = "mov_dsp_irs_dsp_ird",
	 .len = 2,
	 .proc = rx_setup_mov_dsp_irs_dsp_ird},
	{
	 /* (13) MOV src,dst */
	 .mask = 0x0000ff00,
	 .icode = 0x0000c200,
	 .name = "mov_dsp_irs_dsp_ird",
	 .len = 2,
	 .proc = rx_setup_mov_dsp_irs_dsp_ird},
	{
	 /* (13) MOV src,dst */
	 .mask = 0x0000ff00,
	 .icode = 0x0000c400,
	 .name = "mov_dsp_irs_dsp_ird",
	 .len = 2,
	 .proc = rx_setup_mov_dsp_irs_dsp_ird},
	{
	 /* (13) MOV src,dst */
	 .mask = 0x0000ff00,
	 .icode = 0x0000c500,
	 .name = "mov_dsp_irs_dsp_ird",
	 .len = 2,
	 .proc = rx_setup_mov_dsp_irs_dsp_ird},
	{
	 /* (13) MOV src,dst */
	 .mask = 0x0000ff00,
	 .icode = 0x0000c600,
	 .name = "mov_dsp_irs_dsp_ird",
	 .len = 2,
	 .proc = rx_setup_mov_dsp_irs_dsp_ird},
	{
	 /* (13) MOV src,dst */
	 .mask = 0x0000ff00,
	 .icode = 0x0000c800,
	 .name = "mov_dsp_irs_dsp_ird",
	 .len = 2,
	 .proc = rx_setup_mov_dsp_irs_dsp_ird},
	{
	 /* (13) MOV src,dst */
	 .mask = 0x0000ff00,
	 .icode = 0x0000c900,
	 .name = "mov_dsp_irs_dsp_ird",
	 .len = 2,
	 .proc = rx_setup_mov_dsp_irs_dsp_ird},
	{
	 /* (13) MOV src,dst */
	 .mask = 0x0000ff00,
	 .icode = 0x0000ca00,
	 .name = "mov_dsp_irs_dsp_ird",
	 .len = 2,
	 .proc = rx_setup_mov_dsp_irs_dsp_ird},

	{
	 /* (13) MOV src,dst */
	 .mask = 0x0000ff00,
	 .icode = 0x0000d000,
	 .name = "mov_dsp_irs_dsp_ird",
	 .len = 2,
	 .proc = rx_setup_mov_dsp_irs_dsp_ird},
	{
	 /* (13) MOV src,dst */
	 .mask = 0x0000ff00,
	 .icode = 0x0000d100,
	 .name = "mov_dsp_irs_dsp_ird",
	 .len = 2,
	 .proc = rx_setup_mov_dsp_irs_dsp_ird},
	{
	 /* (13) MOV src,dst */
	 .mask = 0x0000ff00,
	 .icode = 0x0000d200,
	 .name = "mov_dsp_irs_dsp_ird",
	 .len = 2,
	 .proc = rx_setup_mov_dsp_irs_dsp_ird},
	{
	 /* (13) MOV src,dst */
	 .mask = 0x0000ff00,
	 .icode = 0x0000d400,
	 .name = "mov_dsp_irs_dsp_ird",
	 .len = 2,
	 .proc = rx_setup_mov_dsp_irs_dsp_ird},
	{
	 /* (13) MOV src,dst */
	 .mask = 0x0000ff00,
	 .icode = 0x0000d500,
	 .name = "mov_dsp_irs_dsp_ird",
	 .len = 2,
	 .proc = rx_setup_mov_dsp_irs_dsp_ird},
	{
	 /* (13) MOV src,dst */
	 .mask = 0x0000ff00,
	 .icode = 0x0000d600,
	 .name = "mov_dsp_irs_dsp_ird",
	 .len = 2,
	 .proc = rx_setup_mov_dsp_irs_dsp_ird},
	{
	 /* (13) MOV src,dst */
	 .mask = 0x0000ff00,
	 .icode = 0x0000d800,
	 .name = "mov_dsp_irs_dsp_ird",
	 .len = 2,
	 .proc = rx_setup_mov_dsp_irs_dsp_ird},
	{
	 /* (13) MOV src,dst */
	 .mask = 0x0000ff00,
	 .icode = 0x0000d900,
	 .name = "mov_dsp_irs_dsp_ird",
	 .len = 2,
	 .proc = rx_setup_mov_dsp_irs_dsp_ird},
	{
	 /* (13) MOV src,dst */
	 .mask = 0x0000ff00,
	 .icode = 0x0000da00,
	 .name = "mov_dsp_irs_dsp_ird",
	 .len = 2,
	 .proc = rx_setup_mov_dsp_irs_dsp_ird},

	{
	 /* (13) MOV src,dst */
	 .mask = 0x0000ff00,
	 .icode = 0x0000e000,
	 .name = "mov_dsp_irs_dsp_ird",
	 .len = 2,
	 .proc = rx_setup_mov_dsp_irs_dsp_ird},
	{
	 /* (13) MOV src,dst */
	 .mask = 0x0000ff00,
	 .icode = 0x0000e100,
	 .name = "mov_dsp_irs_dsp_ird",
	 .len = 2,
	 .proc = rx_setup_mov_dsp_irs_dsp_ird},
	{
	 /* (13) MOV src,dst */
	 .mask = 0x0000ff00,
	 .icode = 0x0000e200,
	 .name = "mov_dsp_irs_dsp_ird",
	 .len = 2,
	 .proc = rx_setup_mov_dsp_irs_dsp_ird},
	{
	 /* (13) MOV src,dst */
	 .mask = 0x0000ff00,
	 .icode = 0x0000e400,
	 .name = "mov_dsp_irs_dsp_ird",
	 .len = 2,
	 .proc = rx_setup_mov_dsp_irs_dsp_ird},
	{
	 /* (13) MOV src,dst */
	 .mask = 0x0000ff00,
	 .icode = 0x0000e500,
	 .name = "mov_dsp_irs_dsp_ird",
	 .len = 2,
	 .proc = rx_setup_mov_dsp_irs_dsp_ird},
	{
	 /* (13) MOV src,dst */
	 .mask = 0x0000ff00,
	 .icode = 0x0000e600,
	 .name = "mov_dsp_irs_dsp_ird",
	 .len = 2,
	 .proc = rx_setup_mov_dsp_irs_dsp_ird},
	{
	 /* (13) MOV src,dst */
	 .mask = 0x0000ff00,
	 .icode = 0x0000e800,
	 .name = "mov_dsp_irs_dsp_ird",
	 .len = 2,
	 .proc = rx_setup_mov_dsp_irs_dsp_ird},
	{
	 /* (13) MOV src,dst */
	 .mask = 0x0000ff00,
	 .icode = 0x0000e900,
	 .name = "mov_dsp_irs_dsp_ird",
	 .len = 2,
	 .proc = rx_setup_mov_dsp_irs_dsp_ird},
	{
	 /* (13) MOV src,dst */
	 .mask = 0x0000ff00,
	 .icode = 0x0000ea00,
	 .name = "mov_dsp_irs_dsp_ird",
	 .len = 2,
	 .proc = rx_setup_mov_dsp_irs_dsp_ird},

	{
	 /* (14) MOV src,dst */
	 .mask = 0x00fffb00,
	 .icode = 0x00fd2000,
	 .name = "mov_rs_irdpm",
	 .len = 3,
	 .proc = rx_setup_mov_rs_irdpm},
	{
	 /* (14) MOV src,dst */
	 .mask = 0x00fffb00,
	 .icode = 0x00fd2100,
	 .name = "mov_rs_irdpm",
	 .len = 3,
	 .proc = rx_setup_mov_rs_irdpm},
	{
	 /* (14) MOV src,dst */
	 .mask = 0x00fffb00,
	 .icode = 0x00fd2200,
	 .name = "mov_rs_irdpm",
	 .len = 3,
	 .proc = rx_setup_mov_rs_irdpm},
	{
	 /* (15) MOV src,dst */
	 .mask = 0x00fffb00,
	 .icode = 0x00fd2800,
	 .name = "mov_irspm_rd",
	 .len = 3,
	 .proc = rx_setup_mov_irspm_rd},
	{
	 /* (15) MOV src,dst */
	 .mask = 0x00fffb00,
	 .icode = 0x00fd2900,
	 .name = "mov_irspm_rd",
	 .len = 3,
	 .proc = rx_setup_mov_irspm_rd},
	{
	 /* (15) MOV src,dst */
	 .mask = 0x00fffb00,
	 .icode = 0x00fd2a00,
	 .name = "mov_irspm_rd",
	 .len = 3,
	 .proc = rx_setup_mov_irspm_rd},
	{
	 /* (1) MOVU src,dest */
	 .mask = 0x0000f000,
	 .icode = 0x0000b000,
	 .name = "movu_dsp5_rs_rd",
	 .len = 2,
	 .proc = rx_setup_movu_dsp5_irs_rd},
	{
	 /* (2) MOVU src,dest */
	 .mask = 0x0000f800,
	 .icode = 0x00005800,
	 .name = "movu_src_rd",
	 .len = 2,
	 .proc = rx_setup_movu_src_rd},
	{
	 /* (3) MOVU src,dest */
	 .mask = 0x00ffe000,
	 .icode = 0x00fec000,
	 .name = "movu_irirb_rd",
	 .len = 3,
	 .proc = rx_setup_movu_irirb_rd},
	{
	 /* (4) MOVU src,dest */
	 .mask = 0x00fff200,
	 .icode = 0x00fd3000,
	 .name = "movu_rspm_rd",
	 .len = 3,
	 .proc = rx_setup_movu_rspm_rd},
	{
	 /* (1) MUL src,dest */
	 .mask = 0x0000ff00,
	 .icode = 0x00006300,
	 .name = "mul_uimm4_rd",
	 .len = 2,
	 .proc = rx_setup_mul_uimm4_rd},
	{
	 /* (2) MUL src,dest */
	 .mask = 0x0000fcf0,
	 .icode = 0x00007410,
	 .name = "mul_simm_rd",
	 .len = 2,
	 .proc = rx_setup_mul_simm_rd},
	{
	 /* (3) MUL src,dest */
	 .mask = 0x0000fc00,
	 .icode = 0x00004c00,
	 .name = "mul_srcub_rd",
	 .len = 2,
	 .proc = rx_setup_mul_srcub_rd},
	{
	 /* (3) MUL src,dest */
	 .mask = 0x00ff3c00,
	 .icode = 0x00060c00,
	 .name = "mul_src_rd",
	 .len = 3,
	 .proc = rx_setup_mul_src_rd},
	{
	 /* (4) MUL src,src2,dest */
	 .mask = 0x00fff000,
	 .icode = 0x00ff3000,
	 .name = "mul_rsrs2_rd",
	 .len = 3,
	 .proc = rx_setup_mul_rsrs2_rd},
	{
	 /* (1) MULHI src,src2 */
	 .mask = 0x00ffff00,
	 .icode = 0x00fd0000,
	 .name = "mulhi_rs_rs2",
	 .len = 3,
	 .proc = rx_setup_mulhi_rs_rs2},
	{
	 /* (1) MULLO src,src2 */
	 .mask = 0x00ffff00,
	 .icode = 0x00fd0100,
	 .name = "mullo_rs_rs2",
	 .len = 3,
	 .proc = rx_setup_mullo_rs_rs2},
	{
	 /* (1) MVFACHI dest */
	 .mask = 0x00fffff0,
	 .icode = 0x00fd1f00,
	 .name = "mvfachi_rd",
	 .len = 3,
	 .proc = rx_setup_mvfachi},
	{
	 /* (1) MVFACMI dest */
	 .mask = 0x00fffff0,
	 .icode = 0x00fd1f20,
	 .name = "mvfacmi_rd",
	 .len = 3,
	 .proc = rx_setup_mvfacmi},
	{
	 /* (1) MVFC src,dest */
	 .mask = 0x00ffff00,
	 .icode = 0x00fd6a00,
	 .name = "mvfc_src_dst",
	 .len = 3,
	 .proc = rx_setup_mvfc},
	{
	 /* (1) MVTACHI src */
	 .mask = 0x00fffff0,
	 .icode = 0x00fd1700,
	 .name = "mvtachi",
	 .len = 3,
	 .proc = rx_setup_mvtachi},
	{
	 /* (1) MVTACLO src */
	 .mask = 0x00fffff0,
	 .icode = 0x00fd1710,
	 .name = "mvtaclo",
	 .len = 3,
	 .proc = rx_setup_mvtaclo},
	{
	 /* (1) MVTC src,dest */
	 .mask = 0x00fff3f0,
	 .icode = 0x00fd7300,
	 .name = "mvtc_imm_dst",
	 .len = 3,
	 .proc = rx_setup_mvtc_imm_dst},
	{
	 /* (2) MVTC src,dest */
	 .mask = 0x00ffff00,
	 .icode = 0x00fd6800,
	 .name = "mvtc_src_dst",
	 .len = 3,
	 .proc = rx_setup_mvtc_src_dst},
	{
#if 0
	 /* 
	  **************************************************************************
	  * (3) MVTIPL src described as 3 byte instruction in manual, but code fits
	  * better as a 2 byte instruction because it doesn't have the usual prefix.
	  **************************************************************************
	  */
	 .mask = 0x0000ffff,
	 .icode = 0x00007570,
	 .name = "mvtipl_src",
	 .len = 2,
	 .proc = rx_setup_mvtipl_src},
#endif
     .mask = 0x00fffff0,
     .icode = 0x00757000,
     .name = "mvtipl_src",
     .len = 3,
     .proc = rx_setup_mvtipl_src},
	{
	 /* (1) NEG src,dest */
	 .mask = 0x0000fff0,
	 .icode = 0x00007e10,
	 .name = "neg_dst",
	 .len = 2,
	 .proc = rx_setup_neg_dst},
	{
	 /* (2) NEG src,dest */
	 .mask = 0x00ffff00,
	 .icode = 0x00fc0700,
	 .name = "neg_src_dst",
	 .len = 3,
	 .proc = rx_setup_neg_src_dst},
	{
	 /* (1) NOP */
	 .mask = 0x000000ff,
	 .icode = 0x00000003,
	 .name = "nop",
	 .len = 1,
	 .proc = rx_nop},
	{
	 /* (1) NOT dest */
	 .mask = 0x0000fff0,
	 .icode = 0x00007e00,
	 .name = "not_dst",
	 .len = 2,
	 .proc = rx_setup_not_dst},
	{
	 /* (2) NOT src,dest */
	 .mask = 0x00ffff00,
	 .icode = 0x00fc3b00,
	 .name = "not_src_dst",
	 .len = 3,
	 .proc = rx_setup_not_src_dst},
	{
	 /* (1) OR src,dest */
	 .mask = 0x0000ff00,
	 .icode = 0x00006500,
	 .name = "or_uimm4_dst",
	 .len = 2,
	 .proc = rx_setup_or_uimm4_dst},
	{
	 /* (2) OR src,dest */
	 .mask = 0x0000fcf0,
	 .icode = 0x00007430,
	 .name = "or_imm_dst",
	 .len = 2,
	 .proc = rx_setup_or_imm_dst},
	{
	 /* (3) OR src,dest */
	 .mask = 0x0000fc00,
	 .icode = 0x00005400,
	 .name = "or_src_dst_ub",
	 .len = 2,
	 .proc = rx_setup_or_src_dst_ub},
	{
	 /* (3) OR src,dest */
	 .mask = 0x00ff3c00,
	 .icode = 0x00061400,
	 .name = "or_src_dst_nub",
	 .len = 3,
	 .proc = rx_setup_or_src_dst_nub},
	{
	 /* (4) OR src,src2,dest */
	 .mask = 0x00fff000,
	 .icode = 0x00ff5000,
	 .name = "or_src_src2_dst",
	 .len = 3,
	 .proc = rx_setup_or_src_src2_dst},
	{
	 /* (1) POP dest */
	 .mask = 0x0000fff0,
	 .icode = 0x00007eb0,
	 .name = "pop_dst",
	 .len = 2,
	 .proc = rx_setup_pop_dst},
	{
	 /* (2) POPC dest */
	 .mask = 0x0000fff0,
	 .icode = 0x00007ee0,
	 .name = "popc_dst",
	 .len = 2,
	 .proc = rx_setup_popc_dst},
	{
	 /* (1) POPM dest-dest2 */
	 .mask = 0x0000ff00,
	 .icode = 0x00006f00,
	 .name = "popm_dst_dst2",
	 .len = 2,
	 .proc = rx_setup_popm_dst_dst2},
	{
	 /* (1) PUSH */
	 .mask = 0x0000fff0,
	 .icode = 0x00007e80,
	 .name = "push_rs",
	 .len = 2,
	 .proc = rx_setup_push_rs},
	{
	 /* (1) PUSH */
	 .mask = 0x0000fff0,
	 .icode = 0x00007e90,
	 .name = "push_rs",
	 .len = 2,
	 .proc = rx_setup_push_rs},
	{
	 /* (1) PUSH */
	 .mask = 0x0000fff0,
	 .icode = 0x00007ea0,
	 .name = "push_rs",
	 .len = 2,
	 .proc = rx_setup_push_rs},
	{
	 /* (2) PUSH.size src */
	 .mask = 0x0000ff0f,
	 .icode = 0x0000f408,
	 .name = "push_isrc",
	 .len = 2,
	 .proc = rx_setup_push_isrc},
	{
	 /* (2) PUSH.size src */
	 .mask = 0x0000ff0f,
	 .icode = 0x0000f409,
	 .name = "push_isrc",
	 .len = 2,
	 .proc = rx_setup_push_isrc},
	{
	 /* (2) PUSH.size src */
	 .mask = 0x0000ff0f,
	 .icode = 0x0000f40a,
	 .name = "push_isrc",
	 .len = 2,
	 .proc = rx_setup_push_isrc},
	{
	 /* (2) PUSH.size src */
	 .mask = 0x0000ff0f,
	 .icode = 0x0000f508,
	 .name = "push_isrc",
	 .len = 2,
	 .proc = rx_setup_push_isrc},
	{
	 /* (2) PUSH.size src */
	 .mask = 0x0000ff0f,
	 .icode = 0x0000f509,
	 .name = "push_isrc",
	 .len = 2,
	 .proc = rx_setup_push_isrc},
	{
	 /* (2) PUSH.size src */
	 .mask = 0x0000ff0f,
	 .icode = 0x0000f50a,
	 .name = "push_isrc",
	 .len = 2,
	 .proc = rx_setup_push_isrc},
	{
	 /* (2) PUSH.size src */
	 .mask = 0x0000ff0f,
	 .icode = 0x0000f608,
	 .name = "push_isrc",
	 .len = 2,
	 .proc = rx_setup_push_isrc},
	{
	 /* (2) PUSH.size src */
	 .mask = 0x0000ff0f,
	 .icode = 0x0000f609,
	 .name = "push_isrc",
	 .len = 2,
	 .proc = rx_setup_push_isrc},
	{
	 /* (2) PUSH.size src */
	 .mask = 0x0000ff0f,
	 .icode = 0x0000f60a,
	 .name = "push_isrc",
	 .len = 2,
	 .proc = rx_setup_push_isrc},
	{
	 /* (1) PUSHC src */
	 .mask = 0x0000fff0,
	 .icode = 0x00007ec0,
	 .name = "pushc_src",
	 .len = 2,
	 .proc = rx_setup_pushc_src},
	{
	 /* (1) PUSHM src-src2 */
	 .mask = 0x0000ff00,
	 .icode = 0x00006e00,
	 .name = "pushm_src_src2",
	 .len = 2,
	 .proc = rx_setup_pushm_src_src2},
	{
	 /* (1) RACW src */
	 .mask = 0x00ffffef,
	 .icode = 0x00fd1800,
	 .name = "racw_src",
	 .len = 3,
	 .proc = rx_setup_racw_src},
	{
	 /* (1) REVL */
	 .mask = 0x00ffff00,
	 .icode = 0x00fd6700,
	 .name = "revl_src_dst",
	 .len = 3,
	 .proc = rx_setup_revl_src_dst},
	{
	 /* (1) REVW src,dest */
	 .mask = 0x00ffff00,
	 .icode = 0x00fd6500,
	 .name = "revw_src_dst",
	 .len = 3,
	 .proc = rx_setup_revw_src_dst},
	{
	 /* (1) RMPA.size */
	 .mask = 0x0000ffff,
	 .icode = 0x00007f8c,
	 .name = "rmpa",
	 .len = 2,
	 .proc = rx_setup_rmpa},
	{
	 /* (1) RMPA.size */
	 .mask = 0x0000ffff,
	 .icode = 0x00007f8d,
	 .name = "rmpa",
	 .len = 2,
	 .proc = rx_setup_rmpa},
	{
	 /* (1) RMPA.size */
	 .mask = 0x0000ffff,
	 .icode = 0x00007f8e,
	 .name = "rmpa",
	 .len = 2,
	 .proc = rx_setup_rmpa},
	{
	 /* (1) ROLC dest */
	 .mask = 0x0000fff0,
	 .icode = 0x00007e50,
	 .name = "rolc_dst",
	 .len = 2,
	 .proc = rx_setup_rolc_dst},
	{
	 /* (1) RORC dest */
	 .mask = 0x0000fff0,
	 .icode = 0x00007e40,
	 .name = "rorc_dst",
	 .len = 2,
	 .proc = rx_setup_rorc_dst},
	{
	 /* (1) ROTL src,dest */
	 .mask = 0x00fffe00,
	 .icode = 0x00fd6e00,
	 .name = "rotl_imm5_dst",
	 .len = 3,
	 .proc = rx_setup_rotl_imm5_dst},
	{
	 /* (2) ROTL src,dest */
	 .mask = 0x00ffff00,
	 .icode = 0x00fd6600,
	 .name = "rotl_src_dst",
	 .len = 3,
	 .proc = rx_setup_rotl_src_dst},
	{
	 /* (1) ROTR src,dest */
	 .mask = 0x00fffe00,
	 .icode = 0x00fd6c00,
	 .name = "rotr_imm5_dst",
	 .len = 3,
	 .proc = rx_setup_rotr_imm5_dst},
	{
	 /* (2) ROTR src,dest */
	 .mask = 0x00ffff00,
	 .icode = 0x00fd6400,
	 .name = "rotr_src_dst",
	 .len = 3,
	 .proc = rx_setup_rortr_src_dst},
	{
	 /* (1) ROUND */
	 .mask = 0x00fffc00,
	 .icode = 0x00fc9800,
	 .name = "round_src_dst",
	 .len = 3,
	 .proc = rx_setup_round_src_dst},
	{
	 /* (1) RTE */
	 .mask = 0x0000ffff,
	 .icode = 0x00007f95,
	 .name = "rte",
	 .len = 2,
	 .proc = rx_rte},
	{
	 /* (1) RTFI */
	 .mask = 0x0000ffff,
	 .icode = 0x00007f94,
	 .name = "rtfi",
	 .len = 2,
	 .proc = rx_rtfi},
	{
	 /* (1) RTS */
	 .mask = 0x000000ff,
	 .icode = 0x00000002,
	 .name = "rts",
	 .len = 1,
	 .proc = rx_rts},
	{
	 /* (1) RTSD */
	 .mask = 0x000000ff,
	 .icode = 0x00000067,
	 .name = "rtsd_src",
	 .len = 1,
	 .proc = rx_rtsd_src},
	{
	 /* (2) RTSD src,dest-dest2 */
	 .mask = 0x0000ff00,
	 .icode = 0x00003f00,
	 .name = "rtsd_src_dst_dst2",
	 .len = 2,
	 .proc = rx_setup_rtsd_src_dst_dst2},
	{
	 /* (1) SAT */
	 .mask = 0x0000fff0,
	 .icode = 0x00007e30,
	 .name = "sat_dst",
	 .len = 2,
	 .proc = rx_sat_dst},
	{
	 /* (1) SATR */
	 .mask = 0x0000ffff,
	 .icode = 0x00007f93,
	 .name = "satr",
	 .len = 2,
	 .proc = rx_satr},
	{
	 /* (1) SBB src,dest */
	 .mask = 0x00ffff00,
	 .icode = 0x00fc0300,
	 .name = "sbb_src_dst",
	 .len = 3,
	 .proc = rx_setup_sbb_src_dst},
	{
	 /* (2) SBB src,dest */
	 .mask = 0xffffff00,
	 .icode = 0x06a00000,
	 .name = "sbb_isrc_dst",
	 .len = 4,
	 .proc = rx_sbb_isrc_dst},
	{
	 /* (2) SBB src,dest */
	 .mask = 0xffffff00,
	 .icode = 0x06a10000,
	 .name = "sbb_isrc_dst",
	 .len = 4,
	 .proc = rx_sbb_isrc_dst},
	{
	 /* (2) SBB src,dest */
	 .mask = 0xffffff00,
	 .icode = 0x06a20000,
	 .name = "sbb_isrc_dst",
	 .len = 4,
	 .proc = rx_sbb_isrc_dst},
	{
	 /* (1) SCCnd.size dest */
	 .mask = 0x00fffc00,
	 .icode = 0x00fcd000,
	 .name = "sccnd_dst",
	 .len = 3,
	 .proc = rx_setup_sccnd_dst},
	{
	 /* (1) SCCnd.size dest */
	 .mask = 0x00fffc00,
	 .icode = 0x00fcd400,
	 .name = "sccnd_dst",
	 .len = 3,
	 .proc = rx_setup_sccnd_dst},
	{
	 /* (1) SCCnd.size dest */
	 .mask = 0x00fffc00,
	 .icode = 0x00fcd800,
	 .name = "sccnd_dst",
	 .len = 3,
	 .proc = rx_setup_sccnd_dst},
	{
	 /* (1) SCMPU */
	 .mask = 0x0000ffff,
	 .icode = 0x00007f83,
	 .name = "scmpu",
	 .len = 2,
	 .proc = rx_scmpu},
	{
	 /* SETPSW */
	 .mask = 0x0000fff0,
	 .icode = 0x00007fa0,
	 .name = "setpsw",
	 .len = 2,
	 .proc = rx_setup_setpsw},
	{
	 /* (1) SHAR src,dest */
	 .mask = 0x0000fe00,
	 .icode = 0x00006a00,
	 .name = "shar_imm5_dst",
	 .len = 2,
	 .proc = rx_setup_shar_imm5_dst},
	{
	 /* (2) SHAR src,dest */
	 .mask = 0x00ffff00,
	 .icode = 0x00fd6100,
	 .name = "shar_src_dst",
	 .len = 3,
	 .proc = rx_setup_shar_src_dst},
	{
	 /* (3) SHAR src,src2,dest */
	 .mask = 0x00ffe000,
	 .icode = 0x00fda000,
	 .name = "shar_imm5_src2_dst",
	 .len = 3,
	 .proc = rx_setup_shar_imm5_src2_dst},
	{
	 /* (1) SHLL src,dest */
	 .mask = 0x0000fe00,
	 .icode = 0x00006c00,
	 .name = "shll_imm5_dst",
	 .len = 2,
	 .proc = rx_setup_shll_imm5_dst},
	{
	 /* (2) SHLL src,dest */
	 .mask = 0x00ffff00,
	 .icode = 0x00fd6200,
	 .name = "shll_src_dst",
	 .len = 3,
	 .proc = rx_setup_shll_src_dst},
	{
	 /* (3) SHLL src,src2,dest */
	 .mask = 0x00ffe000,
	 .icode = 0x00fdc000,
	 .name = "shll_imm5_src2_dst",
	 .len = 3,
	 .proc = rx_setup_shll_imm5_src2_dst},
	{
	 /* (1) SHLR  */
	 .mask = 0x0000fe00,
	 .icode = 0x00006800,
	 .name = "shlr_imm5_dst",
	 .len = 2,
	 .proc = rx_setup_shlr_imm5_dst},
	{
	 /* (2) SHLR src,dest */
	 .mask = 0x00ffff00,
	 .icode = 0x00fd6000,
	 .name = "shlr_src_dst",
	 .len = 3,
	 .proc = rx_setup_shlr_src_dst},
	{
	 /* (3) SHLR src,src2,dest */
	 .mask = 0x00ffe000,
	 .icode = 0x00fd8000,
	 .name = "shlr_imm5_src2_dst",
	 .len = 3,
	 .proc = rx_setup_shlr_imm5_src2_dst},
	{
	 /* (1) SMOVB */
	 .mask = 0x0000ffff,
	 .icode = 0x00007f8b,
	 .name = "smovb",
	 .len = 2,
	 .proc = rx_smovb},
	{
	 /* (1) SMOVF */
	 .mask = 0x0000ffff,
	 .icode = 0x00007f8f,
	 .name = "smovf",
	 .len = 2,
	 .proc = rx_smovf},
	{
	 /* (1) SMOVU */
	 .mask = 0x0000ffff,
	 .icode = 0x00007f87,
	 .name = "smovu",
	 .len = 2,
	 .proc = rx_smovu},
	{
	 /* (1) SSTR */
	 .mask = 0x0000ffff,
	 .icode = 0x00007f88,
	 .name = "sstr",
	 .len = 2,
	 .proc = rx_setup_sstr},
	{
	 /* (1) SSTR */
	 .mask = 0x0000ffff,
	 .icode = 0x00007f89,
	 .name = "sstr",
	 .len = 2,
	 .proc = rx_setup_sstr},
	{
	 /* (1) SSTR */
	 .mask = 0x0000ffff,
	 .icode = 0x00007f8a,
	 .name = "sstr",
	 .len = 2,
	 .proc = rx_setup_sstr},
	{
	 /* (1) STNZ src,dest */
	 .mask = 0x00fff3f0,
	 .icode = 0x00fd70f0,
	 .name = "stnz_src_dst",
	 .len = 3,
	 .proc = rx_setup_stnz_src_dst},
	{
	 /* (1) STZ src,dest */
	 .mask = 0x00fff3f0,
	 .icode = 0x00fd70e0,
	 .name = "stz_src_dst",
	 .len = 3,
	 .proc = rx_setup_stz_src_dst},
	{
	 /* (1) SUB src,dest */
	 .mask = 0x0000ff00,
	 .icode = 0x00006000,
	 .name = "sub_uimm4_dst",
	 .len = 2,
	 .proc = rx_setup_sub_uimm4_dst},
	{
	 /* (2) SUB src,dest */
	 .mask = 0x0000fc00,
	 .icode = 0x00004000,
	 .name = "sub_src_dst_ub",
	 .len = 2,
	 .proc = rx_setup_sub_src_dst_ub},
	{
	 /* (2) SUB src,dest */
	 .mask = 0x00ff3c00,
	 .icode = 0x00060000,
	 .name = "sub_src_dst_nub",
	 .len = 3,
	 .proc = rx_setup_sub_src_dst_nub},
	{
	 /* (3) SUB src,src2,dest */
	 .mask = 0x00fff000,
	 .icode = 0x00ff0000,
	 .name = "sub_src_src2_dst",
	 .len = 3,
	 .proc = rx_setup_sub_src_src2_dst},
	{
	 /* (1) SUNTIL */
	 .mask = 0x0000ffff,
	 .icode = 0x00007f80,
	 .name = "suntil",
	 .len = 2,
	 .proc = rx_setup_suntil},
	{
	 /* (1) SUNTIL */
	 .mask = 0x0000ffff,
	 .icode = 0x00007f81,
	 .name = "suntil",
	 .len = 2,
	 .proc = rx_setup_suntil},
	{
	 /* (1) SUNTIL */
	 .mask = 0x0000ffff,
	 .icode = 0x00007f82,
	 .name = "suntil",
	 .len = 2,
	 .proc = rx_setup_suntil},
	{
	 /* (1) SWHILE */
	 .mask = 0x0000ffff,
	 .icode = 0x00007f84,
	 .name = "swhile",
	 .len = 2,
	 .proc = rx_setup_swhile},
	{
	 /* (1) SWHILE */
	 .mask = 0x0000ffff,
	 .icode = 0x00007f85,
	 .name = "swhile",
	 .len = 2,
	 .proc = rx_setup_swhile},
	{
	 /* (1) SWHILE */
	 .mask = 0x0000ffff,
	 .icode = 0x00007f86,
	 .name = "swhile",
	 .len = 2,
	 .proc = rx_setup_swhile},
	{
	 /* (1) TST src,src2 */
	 .mask = 0x00fff3f0,
	 .icode = 0x00fd70c0,
	 .name = "tst_imm_rs",
	 .len = 3,
	 .proc = rx_setup_tst_imm_rs},
	{
	 /* (2) TST src,src2 */
	 .mask = 0x00fffc00,
	 .icode = 0x00fc3000,
	 .name = "tst_src_src2_ub",
	 .len = 3,
	 .proc = rx_setup_tst_src_src2_ub},
	{
	 /* (2) TST src,src2 */
	 .mask = 0xff3cff00,
	 .icode = 0x06200c00,
	 .name = "tst_src_src2_nub",
	 .len = 4,
	 .proc = rx_tst_src_src2_nub},
	{
	 /* (1) WAIT */
	 .mask = 0x0000ffff,
	 .icode = 0x00007f96,
	 .name = "wait",
	 .len = 2,
	 .proc = rx_wait},
	{
	 /* (1) XCHG src,dest */
	 .mask = 0x00fffc00,
	 .icode = 0x00fc4000,
	 .name = "xchg_src_dst_ub",
	 .len = 3,
	 .proc = rx_setup_xchg_src_dst_ub},
	{
	 /* (1) XCHG src,dest */
	 .mask = 0xff3cff00,
	 .icode = 0x06201000,
	 .name = "xchg_src_dst_nub",
	 .len = 4,
	 .proc = rx_xchg_src_dst_nub},
	{
	 /* (1) XOR src,dest */
	 .mask = 0x00fff3f0,
	 .icode = 0x00fd70d0,
	 .name = "xor_imm_dst",
	 .len = 3,
	 .proc = rx_setup_xor_imm_dst},
	{
	 /* (2) XOR src,dest */
	 .mask = 0x00fffc00,
	 .icode = 0x00fc3400,
	 .name = "xor_src_dst_ub",
	 .len = 3,
	 .proc = rx_setup_xor_src_dst_ub},
	{
	 /* (2) XOR src,dest */
	 .mask = 0xff3cff00,
	 .icode = 0x06200d00,
	 .name = "xor_src_dst_nub",
	 .len = 4,
	 .proc = rx_xor_src_dst_nub},
};

static RX_Instruction instr_und = {
	.mask = 0x000000,
	.icode = 0x000000,
	.name = "und",
	.len = 1,
	.proc = rx_und,
};

/**
 **************************************************************************************
 * \fn static void Verify_InstrList(void); 
 * Make some plausibility checks for every instruction from the instruction list
 **************************************************************************************
 */
static void
Verify_InstrList(void)
{
	RX_Instruction *instr, *instr2;
	int instrcnt;
	int i, j;
	instrcnt = array_size(instrlist);
	for (i = 0; i < instrcnt; i++) {
		instr = &instrlist[i];
		if ((instr->icode & instr->mask) != instr->icode) {
			fprintf(stderr, "Bad instruction in list: %s\n", instr->name);
			exit(1);
		}
	}
	for (i = 0; i < instrcnt; i++) {
		for (j = 0; j < instrcnt; j++) {
			if (i == j) {
				continue;
			}
			instr = &instrlist[i];
			instr2 = &instrlist[j];
			if ((instr->icode & instr2->mask) == instr2->icode) {
				fprintf(stderr, "Cross match at %s(%08x), %s(%08x)\n",
					instr->name, instr->icode, instr2->name, instr2->icode);
			}
		}
	}

	for (i = 0; i < instrcnt; i++) {
		for (j = 0; j < instrcnt; j++) {
			if (i == j) {
				continue;
			}
			instr = &instrlist[i];
			instr2 = &instrlist[j];
			if ((instr->icode & instr2->mask) == instr2->icode) {
				fprintf(stderr, "Cross match at %s(%08x), %s(%08x)\n",
					instr->name, instr->icode, instr2->name, instr2->icode);
			}
		}
	}
}

static void
LeftAlign_InstrList(void)
{
	RX_Instruction *instr;
	int instrcnt;
	int i;
	instrcnt = array_size(instrlist);
	for (i = 0; i < instrcnt; i++) {
		instr = &instrlist[i];
		int shift = (4 - instr->len) * 8;
		instr->mask <<= shift;
		instr->icode <<= shift;
//              fprintf(stderr,"Shift %s by %d %08x %08x\n",instr->name,shift,instr->icode,instr->mask);
	}
}

/**
 *****************************************************************************************************
 * Full Idecoder test by walking through the complete instruction table for every
 * of the 2^24 instruction codes and comparing it with the result of the instruction decoder.
 *****************************************************************************************************
 */
__attribute__ ((unused))
static void
IDecoderTest(void)
{
	uint32_t i, j;
	uint32_t instrcnt;
	RX_Instruction *instr, *instr2;
	instrcnt = array_size(instrlist);
	for (i = 0; i < 65536 * 256; i++) {
		ICODE = i << 8;
		instr2 = &instr_und;
		for (j = 0; j < instrcnt; j++) {
			instr = &instrlist[j];
			if ((ICODE & instr->mask) == instr->icode) {
				instr2 = instr;
			}
		}
		instr = RX_InstructionFind();
		if (memcmp(instr, instr2, sizeof(*instr)) != 0) {
			fprintf(stderr,
				"Shit at find %s real %s icode %08x,iicode %08x mask %08x\n",
				instr->name, instr2->name, ICODE, instr->icode, instr->mask);
			exit(1);
		}
		if (instr->name == NULL) {
			fprintf(stderr, "Idecode test failed at %08x\n", ICODE);
		}
	}
}

/**
 **************************************************
 * Only decode second level if necessary
 **************************************************
 */
static void
Level2Execute(void)
{
	INSTR = INSTR->subTab[(ICODE >> 16) & 0xff];
	return INSTR->proc();
}

/**
 *******************************************************************
 * \fn void RX_IDecoderNew(void)
 * Create a new instruction decoder for the Renesas RX.
 *******************************************************************
 */
#define SWAP 1

void
RX_IDecoderNew(void)
{
	uint32_t i, j, k;
	uint32_t instrcnt;
	uint32_t subtabs = 0;
	bool verbose = false;
	instrcnt = array_size(instrlist);
	RX_Instruction *instr;
	rxITab = sg_calloc(0x10000 * sizeof(RX_Instruction *));
	LeftAlign_InstrList();
	Verify_InstrList();
#if SWAP
	for (j = 0; j < instrcnt; j++) {
		instrlist[j].icode = swap32(instrlist[j].icode);
		instrlist[j].mask = swap32(instrlist[j].mask);
	}
#endif
	for (i = 0; i < 65536; i++) {
#if SWAP
		uint32_t icode = i;
#else
		uint32_t icode = i << 16;
#endif
		bool twolevel = false;
		for (j = 0; j < instrcnt; j++) {
			instr = &instrlist[j];
#if SWAP
			if ((icode & instr->mask) == (instr->icode & 0x0000ffff)) {
#else
			if ((icode & instr->mask) == (instr->icode & 0xffff0000)) {
#endif
				if (verbose) {
					fprintf(stderr, "match at icode %08x, instr %s\n", icode,
						instr->name);
				}
				if (instr->len > 2) {
					twolevel = true;
				} else if (rxITab[i]) {
					fprintf(stderr,
						"Instruction already exists for icode 0x%08x at %s %s\n",
						icode, instr->name, rxITab[i]->name);
					exit(1);
				} else {
					RX_Instruction *dup = sg_new(RX_Instruction);
					*dup = *instr;
					rxITab[i] = dup;
				}
			}
		}
		if (twolevel) {
			if (rxITab[i]) {
				fprintf(stderr, "Twolevel subtab slot already busy: %s\n",
					rxITab[i]->name);
				exit(1);
			} else {
				rxITab[i] = sg_new(RX_Instruction);
				rxITab[i]->subTab = sg_calloc(256 * sizeof(RX_Instruction *));
				rxITab[i]->proc = Level2Execute;
				rxITab[i]->name = "level2 table";
			}
			++subtabs;
			if (verbose) {
				fprintf(stderr, "SubTab%d for %04x\n", subtabs, i);
			}
			for (k = 0; k < 256; k++) {
#if SWAP
				icode = i | (k << 16);
#else
				icode = (i << 16) | (k << 8);
#endif
				for (j = 0; j < instrcnt; j++) {
					instr = &instrlist[j];
					if ((icode & instr->mask) == (instr->icode)) {
						if (instr->len < 3) {
							fprintf(stderr,
								"Short instruction in 3 byte instr. subtab\n");
							exit(1);
						} else {
							RX_Instruction *dup =
							    sg_new(RX_Instruction);
							*dup = *instr;
							rxITab[i]->subTab[k] = dup;
						}
					}
				}
				if (rxITab[i]->subTab[k] == NULL) {
					rxITab[i]->subTab[k] = &instr_und;
				}
			}

		}
		if (rxITab[i] == NULL) {
			rxITab[i] = &instr_und;
		}
	}
#ifdef TEST
	IDecoderTest();
#endif
	fprintf(stderr, "RX Instruction decoder initialzed with %d second level tabs\n", subtabs);
}
