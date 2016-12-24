/* 
 ************************************************************************************************ 
 *
 * M32C Instruction Decoder
 * State: Working
 *
 * Copyright 2009/2010 Jochen Karrer. All rights reserved.
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
 ************************************************************************************************ 
 */
#include <idecode_m32c.h>
#include "instructions_m32c.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "sgstring.h"
#include "sglib.h"
#include "compiler_extensions.h"

#if 0
#define dbgprintf(...) { fprintf(stderr,__VA_ARGS__); }
#else
#define dbgprintf(...)
#endif

M32C_Instruction **m32c_iTab;

/**
 ***********************************************************************
 * \fn bool M32C_CheckGAMHigh(M32C_Instruction *instr,uint32_t icode)
 * Check if addressing mode in upper bits of the icode  exists 
 ***********************************************************************
 */
static bool
M32C_CheckGAMHigh(M32C_Instruction * instr, uint32_t icode, uint8_t * nrMemAcc)
{
	uint32_t gam;
	if (instr->len == 2) {
		icode = icode >> 8;
	} else if (instr->len == 1) {
		fprintf(stderr, "Bug: GAMH check for one byte instruction %s\n", instr->name);
		exit(1);
	}
	gam = ((icode >> 4) & 3) | ((icode >> 10) & 0x1c);
	switch (gam) {
	    case 0x12:
	    case 0x13:
	    case 0x10:
	    case 0x11:		/* r1h/R3 */
	    case 2:
	    case 3:
		    *nrMemAcc = 0;
		    break;
	    case 0:
	    case 1:
	    case 4:
	    case 5:
	    case 6:
	    case 7:
	    case 8:
	    case 9:
	    case 0xa:
	    case 0xb:
	    case 0xc:
	    case 0xd:
	    case 0xe:
	    case 0xf:
		    *nrMemAcc = 1;
	}
	if (gam < 20) {
		dbgprintf("GAMH %d exists  for %s(%06x)\n", gam, instr->name, icode);
		return true;
	} else {
		dbgprintf("GAMH %d does not exist for %s\n", gam, instr->name);
		return false;
	}
}

/**
 ***********************************************************************
 * \fn bool M32C_CheckGAMLow(M32C_Instruction *instr,uint32_t icode)
 * Check if addressing mode in lower bits of the icode  exists 
 ***********************************************************************
 */
static bool
M32C_CheckGAMLow(M32C_Instruction * instr, uint32_t icode, uint8_t * nrMemAcc)
{
	uint32_t gam;
	if (instr->len == 2) {
		icode = icode >> 8;
	} else if (instr->len == 1) {
		fprintf(stderr, "Bug: GAML check for one byte instruction %s\n", instr->name);
		exit(1);
	}
	gam = ((icode >> 6) & 3) | ((icode >> 7) & 0x1c);
	switch (gam) {
	    case 0x12:
	    case 0x13:
	    case 0x10:
	    case 0x11:		/* r1h/R3 */
	    case 2:
	    case 3:
		    *nrMemAcc = 0;
		    break;
	    case 0:
	    case 1:
	    case 4:
	    case 5:
	    case 6:
	    case 7:
	    case 8:
	    case 9:
	    case 0xa:
	    case 0xb:
	    case 0xc:
	    case 0xd:
	    case 0xe:
	    case 0xf:
		    *nrMemAcc = 1;
	}
	if (gam < 20) {
		dbgprintf("GAML %d exists  for %s(%06x)\n", gam, instr->name, icode);
		return true;
	} else {
		dbgprintf(stderr, "GAML %d does not exist for %s\n", gam, instr->name);
		return false;
	}
}

static inline bool
M32C_CheckBitaddrLow(M32C_Instruction * instr, uint32_t icode, uint8_t * nrMemAcc)
{
	return M32C_CheckGAMLow(instr, icode, nrMemAcc);
}

static bool
M32C_CheckGAMBoth(M32C_Instruction * instr, uint32_t icode, uint8_t * nrMemAcc)
{
	*nrMemAcc = 0;
	uint8_t hiMemAcc = 0;
	uint8_t loMemAcc = 0;
	if (M32C_CheckGAMHigh(instr, icode, &hiMemAcc) == false) {
		return false;
	} else if (M32C_CheckGAMLow(instr, icode, &loMemAcc) == false) {
		return false;
	} else {
		*nrMemAcc = hiMemAcc + loMemAcc;
		return true;
	}

}

/*
 *******************************************************************
 * The instruction list
 * v0
 *******************************************************************
 */
static M32C_Instruction instrlist[] = {
	{
	 .mask = 0x00f03f,
	 .icode = 0x00a01f,
	 .name = "abs.size_dst",
	 .len = 2,
	 .proc = m32c_setup_abs_size_dst,
	 .existCheckProc = M32C_CheckGAMLow,
	 .cycles = 1,
	 },
	{
	 .mask = 0xfff03f,
	 .icode = 0x09a01f,
	 .name = "abs.size_idst",
	 .len = 3,
	 .proc = m32c_setup_abs_size_idst,
	 .existCheckProc = M32C_CheckGAMLow,
	 .cycles = 4,
	 },
	{
	 .mask = 0xfff03f,
	 .icode = 0x01802e,
	 .name = "adc.size_immdst",
	 .len = 3,
	 .proc = m32c_setup_adc_size_immdst,
	 .existCheckProc = M32C_CheckGAMLow,
	 .cycles = 1,
	 },
	{
	 .mask = 0xff800f,
	 .icode = 0x018004,
	 .name = "adc.size_srcdst",
	 .len = 3,
	 .proc = m32c_setup_adc_size_srcdst,
	 .existCheckProc = M32C_CheckGAMBoth,
	 .cycles = 1,
	 },
	{
	 .mask = 0x00f03f,
	 .icode = 0x00b01e,
	 .name = "adcf.size_dst",
	 .len = 2,
	 .proc = m32c_setup_adcf_size_dst,
	 .existCheckProc = M32C_CheckGAMLow,
	 .cycles = 1,
	 },
	{
	 .mask = 0xfff03f,
	 .icode = 0x09b01e,
	 .name = "adcf.size_idst",
	 .len = 3,
	 .proc = m32c_setup_adcf_size_idst,
	 .existCheckProc = M32C_CheckGAMLow,
	 .cycles = 4,
	 },
	{
	 .mask = 0x00f03f,
	 .icode = 0x00802e,
	 .name = "add.size:g_immdst",
	 .len = 2,
	 .proc = m32c_setup_add_size_g_immdst,
	 .existCheckProc = M32C_CheckGAMLow,
	 .cycles = 1,
	 },
	{
	 .mask = 0xfff03f,
	 .icode = 0x09802e,
	 .name = "add.size:g_immidst",
	 .len = 3,
	 .proc = m32c_setup_add_size_g_immidst,
	 .existCheckProc = M32C_CheckGAMLow,
	 .cycles = 4,
	 },
	{
	 .mask = 0x00f13f,
	 .icode = 0x008031,
	 .name = "add.l:g_immdst",
	 .len = 2,
	 .proc = m32c_setup_add_l_g_immdst,
	 .existCheckProc = M32C_CheckGAMLow,
	 .cycles = 2,
	 },
	{
	 .mask = 0xfff13f,
	 .icode = 0x098031,
	 .name = "add.l:g_immidst",
	 .len = 3,
	 .proc = m32c_setup_add_l_g_immidst,
	 .existCheckProc = M32C_CheckGAMLow,
	 .cycles = 5,
	 },
	{
	 .mask = 0x00f030,
	 .icode = 0x00e030,
	 .name = "add.size:q_immdst",
	 .len = 2,
	 .proc = m32c_setup_add_size_q_immdst,
	 .existCheckProc = M32C_CheckGAMLow,
	 .cycles = 1,
	 },
	{
	 .mask = 0x00f030,
	 .icode = 0x00f030,
	 .name = "add.size:q_immdst",
	 .len = 2,
	 .proc = m32c_setup_add_size_q_immdst,
	 .existCheckProc = M32C_CheckGAMLow,
	 .cycles = 1,
	 },
	{
	 .mask = 0xfff030,
	 .icode = 0x09e030,
	 .name = "add.size:q_immidst",
	 .len = 3,
	 .proc = m32c_setup_add_size_q_immidst,
	 .existCheckProc = M32C_CheckGAMLow,
	 .cycles = 4,
	 },
	{
	 .mask = 0xfff030,
	 .icode = 0x09f030,
	 .name = "add.size:q_immidst",
	 .len = 3,
	 .proc = m32c_setup_add_size_q_immidst,
	 .existCheckProc = M32C_CheckGAMLow,
	 .cycles = 4,
	 },
	{
	 .mask = 0x0000fe,
	 .icode = 0x000006,
	 .name = "add.size:s_immdst",
	 .len = 1,
	 .proc = m32c_setup_add_size_s_immdst,
	 .cycles = 1,
	 },
	{
	 .mask = 0x0000fe,
	 .icode = 0x000016,
	 .name = "add.size:s_immdst",
	 .len = 1,
	 .proc = m32c_setup_add_size_s_immdst,
	 .cycles = 3,
	 },
	{
	 .mask = 0x0000fe,
	 .icode = 0x000026,
	 .name = "add.size:s_immdst",
	 .len = 1,
	 .proc = m32c_setup_add_size_s_immdst,
	 .cycles = 3,
	 },
	{
	 .mask = 0x0000fe,
	 .icode = 0x000036,
	 .name = "add.size:s_immdst",
	 .len = 1,
	 .proc = m32c_setup_add_size_s_immdst,
	 .cycles = 3,
	 },
	{
	 .mask = 0x00fffe,
	 .icode = 0x000906,
	 .name = "add.size:s_immidst",
	 .len = 2,
	 .proc = m32c_setup_add_size_s_immidst,
	 .cycles = 4,
	 },
	{
	 .mask = 0x00fffe,
	 .icode = 0x000916,
	 .name = "add.size:s_immidst",
	 .len = 2,
	 .proc = m32c_setup_add_size_s_immidst,
	 .cycles = 6,
	 },
	{
	 .mask = 0x00fffe,
	 .icode = 0x000926,
	 .name = "add.size:s_immidst",
	 .len = 2,
	 .proc = m32c_setup_add_size_s_immidst,
	 .cycles = 6,
	 },
	{
	 .mask = 0x00fffe,
	 .icode = 0x000936,
	 .name = "add.size:s_immidst",
	 .len = 2,
	 .proc = m32c_setup_add_size_s_immidst,
	 .cycles = 6,
	 },
	{
	 .mask = 0x0000de,
	 .icode = 0x00008c,
	 .name = "add.l:s_imm_a0a1",
	 .len = 1,
	 .proc = m32c_setup_add_l_s_imm_a0a1,
	 .cycles = 2,
	 },
	{
	 .mask = 0x00800f,
	 .icode = 0x008008,
	 .name = "add.size:g_srcdst",
	 .len = 2,
	 .proc = m32c_setup_add_size_g_srcdst,
	 .existCheckProc = M32C_CheckGAMBoth,
	 .cycles = 1,
	 },
	{
	 .mask = 0xff800f,
	 .icode = 0x418008,
	 .name = "add.size:g_isrcdst",
	 .len = 3,
	 .proc = m32c_setup_add_size_g_isrcdst,
	 .existCheckProc = M32C_CheckGAMBoth,
	 .cycles = 4,
	 },
	{
	 .mask = 0xff800f,
	 .icode = 0x098008,
	 .name = "add.size:g_srcidst",
	 .len = 3,
	 .proc = m32c_setup_add_size_g_srcidst,
	 .existCheckProc = M32C_CheckGAMBoth,
	 .cycles = 4,
	 },
	{
	 .mask = 0xff800f,
	 .icode = 0x498008,
	 .name = "add.size:g_isrcidst",
	 .len = 3,
	 .proc = m32c_setup_add_size_g_isrcidst,
	 .cycles = 7,
	 },
	{
	 .mask = 0x00810f,
	 .icode = 0x008102,
	 .name = "add.l:g:srcdst",
	 .len = 2,
	 .proc = m32c_setup_add_l_g_srcdst,
	 .existCheckProc = M32C_CheckGAMBoth,
	 .cycles = 2,
	 },
	{
	 .mask = 0xff810f,
	 .icode = 0x418102,
	 .name = "add.l:g:isrcdst",
	 .len = 3,
	 .proc = m32c_setup_add_l_g_isrcdst,
	 .existCheckProc = M32C_CheckGAMBoth,
	 .cycles = 5,
	 },
	{
	 .mask = 0xff810f,
	 .icode = 0x098102,
	 .name = "add.l:g:srcidst",
	 .len = 3,
	 .proc = m32c_setup_add_l_g_srcidst,
	 .existCheckProc = M32C_CheckGAMBoth,
	 .cycles = 5,
	 },
	{
	 .mask = 0xff810f,
	 .icode = 0x498102,
	 .name = "add.l:g:isrcidst",
	 .len = 3,
	 .proc = m32c_setup_add_l_g_isrcidst,
	 .existCheckProc = M32C_CheckGAMBoth,
	 .cycles = 8,
	 },
	{
	 .mask = 0x00ffff,
	 .icode = 0x00b613,
	 .name = "add.l:g:imm16sp",
	 .len = 2,
	 .proc = m32c_setup_add_l_g_imm16sp,
	 .cycles = 2,
	 },
	{.mask = 0x0000ce,
	 .icode = 0x000042,
	 .name = "add.l:q:imm3sp",
	 .len = 1,
	 .proc = m32c_setup_add_l_q_imm3sp,
	 .cycles = 1,
	 },
	{
	 .mask = 0x00ffff,
	 .icode = 0x00b603,
	 .name = "add.l:s:imm8sp",
	 .len = 2,
	 .proc = m32c_setup_add_l_s_imm8sp,
	 .cycles = 2,
	 },
	{
	 .mask = 0x00f13f,
	 .icode = 0x008011,
	 .name = "addx:immdst",
	 .len = 2,
	 .proc = m32c_setup_addx_immdst,
	 .existCheckProc = M32C_CheckGAMLow,
	 .cycles = 2,
	 },
	{
	 .mask = 0xfff13f,
	 .icode = 0x098011,
	 .name = "addx:immidst",
	 .len = 3,
	 .proc = m32c_setup_addx_immidst,
	 .existCheckProc = M32C_CheckGAMLow,
	 .cycles = 5,
	 },
	{
	 .mask = 0x00810f,
	 .icode = 0x008002,
	 .name = "addx:srcdst",
	 .len = 2,
	 .proc = m32c_setup_addx_srcdst,
	 .existCheckProc = M32C_CheckGAMBoth,
	 .cycles = 2,
	 },
	{
	 .mask = 0xff810f,
	 .icode = 0x418002,
	 .name = "addx:isrcdst",
	 .len = 3,
	 .proc = m32c_setup_addx_isrcdst,
	 .existCheckProc = M32C_CheckGAMBoth,
	 .cycles = 5,
	 },
	{
	 .mask = 0xff810f,
	 .icode = 0x098002,
	 .name = "addx:srcidst",
	 .len = 3,
	 .proc = m32c_setup_addx_srcidst,
	 .existCheckProc = M32C_CheckGAMBoth,
	 .cycles = 5,
	 },
	{
	 .mask = 0xff810f,
	 .icode = 0x498002,
	 .name = "addx:isrcidst",
	 .len = 3,
	 .proc = m32c_setup_addx_isrcidst,
	 .existCheckProc = M32C_CheckGAMBoth,
	 .cycles = 8,
	 },
	{
	 .mask = 0x00f030,
	 .icode = 0x00f010,
	 .name = "adjnz.size:immdstlbl",
	 .len = 2,
	 .proc = m32c_setup_adjnz_size_immdstlbl,
	 .existCheckProc = M32C_CheckGAMLow,
	 .cycles = 2,
	 },
	{
	 .mask = 0x00f03f,
	 .icode = 0x00803f,
	 .name = "and.size_immdst",
	 .len = 2,
	 .proc = m32c_setup_and_size_immdst,
	 .existCheckProc = M32C_CheckGAMLow,
	 .cycles = 1,
	 },
	{
	 .mask = 0xfff03f,
	 .icode = 0x09803f,
	 .name = "and.size_immidst",
	 .len = 3,
	 .proc = m32c_setup_and_size_immidst,
	 .existCheckProc = M32C_CheckGAMLow,
	 .cycles = 4,
	 },
	{
	 .mask = 0x0000fe,
	 .icode = 0x00004c,
	 .name = "and.size:s_immdst",
	 .len = 1,
	 .proc = m32c_setup_and_size_s_immdst,
	 .cycles = 1,
	 },
	{
	 .mask = 0x0000fe,
	 .icode = 0x00005c,
	 .name = "and.size:s_immdst",
	 .len = 1,
	 .proc = m32c_setup_and_size_s_immdst,
	 .cycles = 3,
	 },
	{
	 .mask = 0x0000fe,
	 .icode = 0x00006c,
	 .name = "and.size:s_immdst",
	 .len = 1,
	 .proc = m32c_setup_and_size_s_immdst,
	 .cycles = 3,
	 },
	{
	 .mask = 0x0000fe,
	 .icode = 0x00007c,
	 .name = "and.size:s_immdst",
	 .len = 1,
	 .proc = m32c_setup_and_size_s_immdst,
	 .cycles = 3,
	 },
	{
	 .mask = 0x00fffe,
	 .icode = 0x00094c,
	 .name = "and.size:s_immidst",
	 .len = 2,
	 .proc = m32c_setup_and_size_s_immidst,
	 .cycles = 4,
	 },
	{
	 .mask = 0x00fffe,
	 .icode = 0x00095c,
	 .name = "and.size:s_immidst",
	 .len = 2,
	 .proc = m32c_setup_and_size_s_immidst,
	 .cycles = 6,
	 },
	{
	 .mask = 0x00fffe,
	 .icode = 0x00096c,
	 .name = "and.size:s_immidst",
	 .len = 2,
	 .proc = m32c_setup_and_size_s_immidst,
	 .cycles = 6,
	 },
	{
	 .mask = 0x00fffe,
	 .icode = 0x00097c,
	 .name = "and.size:s_immidst",
	 .len = 2,
	 .proc = m32c_setup_and_size_s_immidst,
	 .cycles = 6,
	 },
	{
	 .mask = 0x00800f,
	 .icode = 0x00800d,
	 .name = "and.size:g:srcdst",
	 .len = 2,
	 .proc = m32c_setup_and_size_g_srcdst,
	 .existCheckProc = M32C_CheckGAMBoth,
	 .cycles = 1,
	 },
	{
	 .mask = 0xff800f,
	 .icode = 0x41800d,
	 .name = "and.size:g:isrcdst",
	 .len = 3,
	 .proc = m32c_setup_and_size_g_isrcdst,
	 .existCheckProc = M32C_CheckGAMBoth,
	 .cycles = 4,
	 },
	{
	 .mask = 0xff800f,
	 .icode = 0x09800d,
	 .name = "and.size:g:srcidst",
	 .len = 3,
	 .proc = m32c_setup_and_size_g_srcidst,
	 .existCheckProc = M32C_CheckGAMBoth,
	 .cycles = 4,
	 },
	{
	 .mask = 0xff800f,
	 .icode = 0x49800d,
	 .name = "and.size:g:isrcidst",
	 .len = 3,
	 .proc = m32c_setup_and_size_g_isrcidst,
	 .cycles = 7,
	 },
	{
	 .mask = 0xfff138,
	 .icode = 0x01d008,
	 .name = "band_src",
	 .len = 3,
	 .proc = m32c_setup_band_src,
	 .existCheckProc = M32C_CheckBitaddrLow,
	 .cycles = 2,
	 },
	{
	 .mask = 0x00f138,
	 .icode = 0x00d030,
	 .name = "bclr_dst",
	 .len = 2,
	 .proc = m32c_setup_bclr,
	 .existCheckProc = M32C_CheckBitaddrLow,
	 .cycles = 1,
	 },
	{
	 .mask = 0x00f03f,
	 .icode = 0x00c02e,
	 .name = "bitindex.src",
	 .len = 2,
	 .proc = m32c_setup_bitindex,
	 .existCheckProc = M32C_CheckGAMLow,
	 .cycles = 4,
	 },
	{
	 .mask = 0x00f138,
	 .icode = 0x00d010,
	 .name = "bmcnd_dst",
	 .len = 2,
	 .proc = m32c_setup_bmcnd_dst,
	 .existCheckProc = M32C_CheckBitaddrLow,
	 .cycles = 3,
	 },
	{
	 .mask = 0x00ffb8,
	 .icode = 0x00d928,
	 .name = "bmcnd_c",
	 .len = 2,
	 .proc = m32c_setup_bmcnd_c,
	 .cycles = 2,
	 },
	{
	 .mask = 0xfff138,
	 .icode = 0x01d018,
	 .name = "bnand_src",
	 .len = 3,
	 .proc = m32c_setup_bnand_src,
	 .existCheckProc = M32C_CheckBitaddrLow,
	 .cycles = 2,
	 },
	{
	 .mask = 0xfff138,
	 .icode = 0x01d030,
	 .name = "bnor_src",
	 .len = 3,
	 .proc = m32c_setup_bnor_src0,
	 .existCheckProc = M32C_CheckBitaddrLow,
	 .cycles = 2,
	 },
	{
	 .mask = 0x00f138,
	 .icode = 0x00d018,
	 .name = "bnot_dst",
	 .len = 2,
	 .proc = m32c_setup_bnot_dst,
	 .existCheckProc = M32C_CheckBitaddrLow,
	 .cycles = 1,
	 },
	{
	 .mask = 0xfff138,
	 .icode = 0x01d000,
	 .name = "bntst_src",
	 .len = 3,
	 .proc = m32c_setup_bntst_src,
	 .existCheckProc = M32C_CheckBitaddrLow,
	 .cycles = 1,
	 },
	{
	 .mask = 0xfff138,
	 .icode = 0x01d038,
	 .name = "bnxor_src",
	 .len = 3,
	 .proc = m32c_setup_bnxor_src,
	 .existCheckProc = M32C_CheckBitaddrLow,
	 .cycles = 2,
	 },
	{
	 .mask = 0xfff138,
	 .icode = 0x01d020,
	 .name = "bor_src",
	 .len = 3,
	 .proc = m32c_setup_bor_src,
	 .existCheckProc = M32C_CheckBitaddrLow,
	 .cycles = 2,
	 },
	{
	 .mask = 0x0000ff,
	 .icode = 0x000000,
	 .name = "brk",
	 .len = 1,
	 .proc = m32c_brk,
	 .cycles = 17,
	 },
	{
	 .mask = 0x0000ff,
	 .icode = 0x000008,
	 .name = "brk2",
	 .len = 1,
	 .proc = m32c_brk2,
	 .cycles = 19,
	 },
	{
	 .mask = 0x00f138,
	 .icode = 0x00d038,
	 .name = "bset_dst",
	 .len = 2,
	 .proc = m32c_setup_bset_dst,
	 .existCheckProc = M32C_CheckGAMLow,
	 .cycles = 1,
	 },
	{
	 .mask = 0x00f138,
	 .icode = 0x00d000,
	 .name = "btst:g:src",
	 .len = 2,
	 .proc = m32c_setup_btst_g_src,
	 .existCheckProc = M32C_CheckGAMLow,
	 .cycles = 1,
	 },
	{
	 .mask = 0x0000ce,
	 .icode = 0x00000a,
	 .name = "btst:s:src",
	 .len = 1,
	 .proc = m32c_setup_btst_s_src,
	 .cycles = 3,
	 },
	{
	 .mask = 0x00f138,
	 .icode = 0x00d020,
	 .name = "btstc_dst",
	 .len = 2,
	 .proc = m32c_setup_btstc_dst,
	 .existCheckProc = M32C_CheckBitaddrLow,
	 .cycles = 2,
	 },
	{
	 .mask = 0x00f138,
	 .icode = 0x00d028,
	 .name = "btsts_dst",
	 .len = 2,
	 .proc = m32c_setup_btsts_dst,
	 .existCheckProc = M32C_CheckBitaddrLow,
	 .cycles = 2,
	 },
	{
	 .mask = 0xfff138,
	 .icode = 0x01d028,
	 .name = "bxor_src",
	 .len = 3,
	 .proc = m32c_setup_bxor_src,
	 .existCheckProc = M32C_CheckBitaddrLow,
	 .cycles = 2,
	 },
	{
	 .mask = 0xfff03f,
	 .icode = 0x01803e,
	 .name = "clip_size",
	 .len = 3,
	 .proc = m32c_setup_clip,
	 .existCheckProc = M32C_CheckGAMLow,
	 .cycles = 6,
	 },
	{
	 .mask = 0x00f03f,
	 .icode = 0x00902e,
	 .name = "cmp_size:g_immdst",
	 .len = 2,
	 .proc = m32c_setup_cmp_size_g_immdst,
	 .existCheckProc = M32C_CheckGAMLow,
	 .cycles = 1,
	 },
	{
	 .mask = 0xfff03f,
	 .icode = 0x09902e,
	 .name = "cmp_size:g_immidst",
	 .len = 3,
	 .proc = m32c_setup_cmp_size_g_immidst,
	 .existCheckProc = M32C_CheckGAMLow,
	 .cycles = 4,
	 },
	{
	 .mask = 0x00f13f,
	 .icode = 0x00a031,
	 .name = "cmp_l_g_imm32dst",
	 .len = 2,
	 .proc = m32c_setup_cmp_l_g_imm32dst,
	 .existCheckProc = M32C_CheckGAMLow,
	 .cycles = 2,
	 },
	{
	 .mask = 0xfff13f,
	 .icode = 0x09a031,
	 .name = "cmp_l_g_imm32idst",
	 .len = 3,
	 .proc = m32c_setup_cmp_l_g_imm32idst,
	 .existCheckProc = M32C_CheckGAMLow,
	 .cycles = 5,
	 },
	{
	 .mask = 0x00f030,
	 .icode = 0x00e010,
	 .name = "cmp_size_q_immdst",
	 .len = 2,
	 .proc = m32c_setup_cmp_size_q_immdst,
	 .existCheckProc = M32C_CheckGAMLow,
	 .cycles = 1,
	 },
	{
	 .mask = 0xfff030,
	 .icode = 0x09e010,
	 .name = "cmp_size_q_immidst",
	 .len = 3,
	 .proc = m32c_setup_cmp_size_q_immidst,
	 .existCheckProc = M32C_CheckGAMLow,
	 .cycles = 4,
	 },
	{
	 .mask = 0x0000fe,
	 .icode = 0x000046,
	 .name = "cmp_size_s_immdst",
	 .len = 1,
	 .proc = m32c_setup_cmp_size_s_immdst,
	 .cycles = 1,
	 },
	{
	 .mask = 0x0000fe,
	 .icode = 0x000056,
	 .name = "cmp_size_s_immdst",
	 .len = 1,
	 .proc = m32c_setup_cmp_size_s_immdst,
	 .cycles = 3,
	 },
	{
	 .mask = 0x0000fe,
	 .icode = 0x000066,
	 .name = "cmp_size_s_immdst",
	 .len = 1,
	 .proc = m32c_setup_cmp_size_s_immdst,
	 .cycles = 3,
	 },
	{
	 .mask = 0x0000fe,
	 .icode = 0x000076,
	 .name = "cmp_size_s_immdst",
	 .len = 1,
	 .proc = m32c_setup_cmp_size_s_immdst,
	 .cycles = 3,
	 },
	{
	 .mask = 0x00fffe,
	 .icode = 0x000946,
	 .name = "cmp_size_s_immidst",
	 .len = 2,
	 .proc = m32c_setup_cmp_size_s_immidst,
	 .cycles = 4,
	 },
	{
	 .mask = 0x00fffe,
	 .icode = 0x000956,
	 .name = "cmp_size_s_immidst",
	 .len = 2,
	 .proc = m32c_setup_cmp_size_s_immidst,
	 .cycles = 6,
	 },
	{
	 .mask = 0x00fffe,
	 .icode = 0x000966,
	 .name = "cmp_size_s_immidst",
	 .len = 2,
	 .proc = m32c_setup_cmp_size_s_immidst,
	 .cycles = 6,
	 },
	{
	 .mask = 0x00fffe,
	 .icode = 0x000976,
	 .name = "cmp_size_s_immidst",
	 .len = 2,
	 .proc = m32c_setup_cmp_size_s_immidst,
	 .cycles = 6,
	 },
	{
	 .mask = 0x00800f,
	 .icode = 0x008006,
	 .name = "cmp_size_g_srcdst",
	 .len = 2,
	 .proc = m32c_setup_cmp_size_g_srcdst,
	 .existCheckProc = M32C_CheckGAMBoth,
	 .cycles = 1,
	 },
	{
	 .mask = 0xff800f,
	 .icode = 0x418006,
	 .name = "cmp_size_g_isrcdst",
	 .len = 3,
	 .proc = m32c_setup_cmp_size_g_isrcdst,
	 .existCheckProc = M32C_CheckGAMBoth,
	 .cycles = 4,
	 },
	{
	 .mask = 0xff800f,
	 .icode = 0x098006,
	 .name = "cmp_size_g_srcidst",
	 .len = 3,
	 .proc = m32c_setup_cmp_size_g_srcidst,
	 .existCheckProc = M32C_CheckGAMBoth,
	 .cycles = 4,
	 },
	{
	 .mask = 0xff800f,
	 .icode = 0x498006,
	 .name = "cmp_size_g_isrcidst",
	 .len = 3,
	 .proc = m32c_setup_cmp_size_g_isrcidst,
	 .existCheckProc = M32C_CheckGAMBoth,
	 .cycles = 7,
	 },
	{
	 .mask = 0x00810f,
	 .icode = 0x008101,
	 .name = "cmp_l_g_srcdst",
	 .len = 2,
	 .proc = m32c_setup_cmp_l_g_srcdst,
	 .existCheckProc = M32C_CheckGAMBoth,
	 .cycles = 2,
	 },
	{
	 .mask = 0xff810f,
	 .icode = 0x418101,
	 .name = "cmp_l_g_isrcdst",
	 .len = 3,
	 .proc = m32c_setup_cmp_l_g_isrcdst,
	 .existCheckProc = M32C_CheckGAMBoth,
	 .cycles = 5,
	 },
	{
	 .mask = 0xff810f,
	 .icode = 0x098101,
	 .name = "cmp_l_g_srcidst",
	 .len = 3,
	 .proc = m32c_setup_cmp_l_g_srcidst,
	 .existCheckProc = M32C_CheckGAMBoth,
	 .cycles = 5,
	 },
	{
	 .mask = 0xff810f,
	 .icode = 0x498101,
	 .name = "cmp_l_g_isrcidst",
	 .len = 3,
	 .proc = m32c_setup_cmp_l_g_isrcidst,
	 .existCheckProc = M32C_CheckGAMBoth,
	 .cycles = 8,
	 },
	{
	 .mask = 0x0000fe,
	 .icode = 0x000050,
	 .name = "cmp_size_s_srcr0",
	 .len = 1,
	 .proc = m32c_setup_cmp_size_s_srcr0,
	 .cycles = 3,
	 },
	{
	 .mask = 0x0000fe,
	 .icode = 0x000060,
	 .name = "cmp_size_s_srcr0",
	 .len = 1,
	 .proc = m32c_setup_cmp_size_s_srcr0,
	 .cycles = 3,
	 },
	{
	 .mask = 0x0000fe,
	 .icode = 0x000070,
	 .name = "cmp_size_s_srcr0",
	 .len = 1,
	 .proc = m32c_setup_cmp_size_s_srcr0,
	 .cycles = 3,
	 },
	{
	 .mask = 0x00fffe,
	 .icode = 0x000950,
	 .name = "cmp_size_s_isrcr0",
	 .len = 2,
	 .proc = m32c_setup_cmp_size_s_isrcr0,
	 .cycles = 6,
	 },
	{
	 .mask = 0x00fffe,
	 .icode = 0x000960,
	 .name = "cmp_size_s_isrcr0",
	 .len = 2,
	 .proc = m32c_setup_cmp_size_s_isrcr0,
	 .cycles = 6,
	 },
	{
	 .mask = 0x00fffe,
	 .icode = 0x000970,
	 .name = "cmp_size_s_isrcr0",
	 .len = 2,
	 .proc = m32c_setup_cmp_size_s_isrcr0,
	 .cycles = 6,
	 },
	{
	 .mask = 0x00f13f,
	 .icode = 0x00a011,
	 .name = "cmpx_immdst",
	 .len = 2,
	 .proc = m32c_setup_cmpx_immdst,
	 .existCheckProc = M32C_CheckGAMLow,
	 .cycles = 2,
	 },
	{
	 .mask = 0xfff13f,
	 .icode = 0x09a011,
	 .name = "cmpx_immidst",
	 .len = 3,
	 .proc = m32c_setup_cmpx_immidst,
	 .existCheckProc = M32C_CheckGAMLow,
	 .cycles = 5,
	 },
	{
	 .mask = 0xfff03f,
	 .icode = 0x01800e,
	 .name = "dadc_size_immdst",
	 .len = 3,
	 .proc = m32c_setup_dadc_size_immdst,
	 .existCheckProc = M32C_CheckGAMLow,
	 .cycles = 4,
	 },
	{
	 .mask = 0xff800f,
	 .icode = 0x018008,
	 .name = "dadc_size_srcdst",
	 .len = 3,
	 .proc = m32c_setup_dadc_size_srcdst,
	 .existCheckProc = M32C_CheckGAMBoth,
	 .cycles = 4,
	 },
	{
	 .mask = 0xfff03f,
	 .icode = 0x01801e,
	 .name = "dadd_size_immdst",
	 .len = 3,
	 .proc = m32c_setup_dadd_size_immdst,
	 .existCheckProc = M32C_CheckGAMLow,
	 .cycles = 4,
	 },
	{
	 .mask = 0xff800f,
	 .icode = 0x018000,
	 .name = "dadd_size_srcdst",
	 .len = 3,
	 .proc = m32c_setup_dadd_size_srcdst,
	 .existCheckProc = M32C_CheckGAMBoth,
	 .cycles = 4,
	 },
	{
	 .mask = 0x00f03f,
	 .icode = 0x00b00e,
	 .name = "dec_size_dst",
	 .len = 2,
	 .proc = m32c_setup_dec_size_dst,
	 .existCheckProc = M32C_CheckGAMLow,
	 .cycles = 1,
	 },
	{
	 .mask = 0xfff03f,
	 .icode = 0x09b00e,
	 .name = "dec_size_idst",
	 .len = 3,
	 .proc = m32c_setup_dec_size_idst,
	 .existCheckProc = M32C_CheckGAMLow,
	 .cycles = 4,
	 },
	{
	 .mask = 0x00ffef,
	 .icode = 0x00b043,
	 .name = "div_size_imm",
	 .len = 2,
	 .proc = m32c_setup_div_size_imm,
	 .cycles = 18,
	 },
	{
	 .mask = 0x00f03f,
	 .icode = 0x00801e,
	 .name = "div_size_src",
	 .len = 2,
	 .proc = m32c_setup_div_size_src,
	 .existCheckProc = M32C_CheckGAMLow,
	 .cycles = 18,
	 },
	{
	 .mask = 0xfff03f,
	 .icode = 0x09801e,
	 .name = "div_size_isrc",
	 .len = 3,
	 .proc = m32c_setup_div_size_isrc,
	 .existCheckProc = M32C_CheckGAMLow,
	 .cycles = 31,
	 },
	{
	 .mask = 0xfff13f,
	 .icode = 0x01a11f,
	 .name = "div_l_src",
	 .len = 3,
	 .proc = m32c_setup_div_l_src,
	 .existCheckProc = M32C_CheckGAMLow,
	 .cycles = 40,
	 },
	{
	 .mask = 0x00ffef,
	 .icode = 0x00b003,
	 .name = "divu_size_imm",
	 .len = 2,
	 .proc = m32c_setup_divu_size_imm,
	 .cycles = 18,
	 },
	{
	 .mask = 0x00f03f,
	 .icode = 0x00800e,
	 .name = "divu_size_src",
	 .len = 2,
	 .proc = m32c_setup_divu_size_src,
	 .existCheckProc = M32C_CheckGAMLow,
	 .cycles = 18,
	 },
	{
	 .mask = 0xfff03f,
	 .icode = 0x09800e,
	 .name = "divu_size_isrc",
	 .len = 3,
	 .proc = m32c_setup_divu_size_isrc,
	 .existCheckProc = M32C_CheckGAMLow,
	 .cycles = 21,
	 },
	{
	 .mask = 0xfff13f,
	 .icode = 0x01a10f,
	 .name = "divu_l_src",
	 .len = 3,
	 .proc = m32c_setup_divu_l_src,
	 .existCheckProc = M32C_CheckGAMLow,
	 .cycles = 40,
	 },
	{
	 .mask = 0x00ffef,
	 .icode = 0x00b243,
	 .name = "divx_size_imm",
	 .len = 2,
	 .proc = m32c_setup_divx_size_imm,
	 .cycles = 18,
	 },
	{
	 .mask = 0x00f03f,
	 .icode = 0x00901e,
	 .name = "divx_size_src",
	 .len = 2,
	 .proc = m32c_setup_divx_size_src,
	 .existCheckProc = M32C_CheckGAMLow,
	 .cycles = 18,
	 },
	{
	 .mask = 0xfff03f,
	 .icode = 0x09901e,
	 .name = "divx_size_isrc",
	 .len = 3,
	 .proc = m32c_setup_divx_size_isrc,
	 .existCheckProc = M32C_CheckGAMLow,
	 .cycles = 21,
	 },
	{
	 .mask = 0xfff13f,
	 .icode = 0x01a12f,
	 .name = "divx_l_src",
	 .len = 3,
	 .proc = m32c_setup_divx_l_src,
	 .existCheckProc = M32C_CheckGAMLow,
	 .cycles = 40,
	 },
	{
	 .mask = 0xfff03f,
	 .icode = 0x01900e,
	 .name = "dsbb_size_immdst",
	 .len = 3,
	 .proc = m32c_setup_dsbb_size_immdst,
	 .existCheckProc = M32C_CheckGAMLow,
	 .cycles = 2,
	 },
	{
	 .mask = 0xff800f,
	 .icode = 0x01800a,
	 .name = "dsbb_size_srcdst",
	 .len = 3,
	 .proc = m32c_setup_dsbb_size_srcdst,
	 .existCheckProc = M32C_CheckGAMBoth,
	 .cycles = 2,
	 },
	{
	 .mask = 0xfff03f,
	 .icode = 0x01901e,
	 .name = "dsub_size_immdst",
	 .len = 3,
	 .proc = m32c_setup_dsub_size_immdst,
	 .existCheckProc = M32C_CheckGAMLow,
	 .cycles = 2,
	 },
	{
	 .mask = 0xff800f,
	 .icode = 0x018002,
	 .name = "dsub_size_srcdst",
	 .len = 3,
	 .proc = m32c_setup_dsub_size_srcdst,
	 .existCheckProc = M32C_CheckGAMBoth,
	 .cycles = 2,
	 },
	{
	 .mask = 0x0000ff,
	 .icode = 0x0000ec,
	 .name = "enter_imm",
	 .len = 1,
	 .proc = m32c_enter_imm,
	 .cycles = 4,
	 },
	{
	 .mask = 0x0000ff,
	 .icode = 0x0000fc,
	 .name = "exitd",
	 .len = 1,
	 .proc = m32c_exitd,
	 .cycles = 8,
	 },
	{
	 .mask = 0x00f03f,
	 .icode = 0x00c01e,
	 .name = "exts_size_dst",
	 .len = 2,
	 .proc = m32c_setup_exts_size_dst,
	 .existCheckProc = M32C_CheckGAMLow,
	 .cycles = 1,
	 },
	{
	 .mask = 0xff810f,
	 .icode = 0x018007,
	 .name = "exts_b_src_dst",
	 .len = 3,
	 .proc = m32c_setup_exts_b_src_dst,
	 .existCheckProc = M32C_CheckGAMBoth,
	 .cycles = 1,
	 },
	{
	 .mask = 0xff810f,
	 .icode = 0x01800b,
	 .name = "extz_src_dst",
	 .len = 3,
	 .proc = m32c_setup_extz_src_dst,
	 .existCheckProc = M32C_CheckGAMBoth,
	 .cycles = 1,
	 },
	{
	 .mask = 0x00fff8,
	 .icode = 0x00d3e8,
	 .name = "fclr_dst",
	 .len = 2,
	 .proc = m32c_setup_fclr,
	 .cycles = 1,
	 },
	{
	 .mask = 0x0000ff,
	 .icode = 0x00009f,
	 .name = "freit",
	 .len = 1,
	 .proc = m32c_freit,
	 .cycles = 3,
	 },
	{
	 .mask = 0x00fff8,
	 .icode = 0x00d1e8,
	 .name = "fset",
	 .len = 2,
	 .proc = m32c_setup_fset,
	 .cycles = 1,
	 },
	{
	 .mask = 0x00f03f,
	 .icode = 0x00a00e,
	 .name = "inc_size_dst",
	 .len = 2,
	 .proc = m32c_setup_inc_size_dst,
	 .existCheckProc = M32C_CheckGAMLow,
	 .cycles = 1,
	 },
	{
	 .mask = 0xfff03f,
	 .icode = 0x09a00e,
	 .name = "inc_size_idst",
	 .len = 3,
	 .proc = m32c_setup_inc_size_idst,
	 .existCheckProc = M32C_CheckGAMLow,
	 .cycles = 4,
	 },
	{
	 .mask = 0x00f13f,
	 .icode = 0x008003,
	 .name = "indexb_size_src",
	 .len = 2,
	 .proc = m32c_setup_indexb_size_src,
	 .existCheckProc = M32C_CheckGAMLow,
	 .cycles = 2,
	 },
	{
	 .mask = 0x00f13f,
	 .icode = 0x008013,
	 .name = "indexb_size_src",
	 .len = 2,
	 .proc = m32c_setup_indexb_size_src,
	 .existCheckProc = M32C_CheckGAMLow,
	 .cycles = 4,
	 },
	{
	 .mask = 0x00f13f,
	 .icode = 0x00a003,
	 .name = "indexbd_size_src",
	 .len = 2,
	 .proc = m32c_setup_indexbd_size_src,
	 .existCheckProc = M32C_CheckGAMLow,
	 .cycles = 1,
	 },
	{
	 .mask = 0x00f13f,
	 .icode = 0x00a013,
	 .name = "indexbd_size_src",
	 .len = 2,
	 .proc = m32c_setup_indexbd_size_src,
	 .existCheckProc = M32C_CheckGAMLow,
	 .cycles = 2,
	 },
	{
	 .mask = 0x00f13f,
	 .icode = 0x00c003,
	 .name = "indexbs_size_src",
	 .len = 2,
	 .proc = m32c_setup_indexbs_size_src,
	 .existCheckProc = M32C_CheckGAMLow,
	 .cycles = 1,
	 },
	{
	 .mask = 0x00f13f,
	 .icode = 0x00c013,
	 .name = "indexbs_size_src",
	 .len = 2,
	 .proc = m32c_setup_indexbs_size_src,
	 .existCheckProc = M32C_CheckGAMLow,
	 .cycles = 2,
	 },
	{
	 .mask = 0x00f13f,
	 .icode = 0x009023,
	 .name = "indexl_size_src",
	 .len = 2,
	 .proc = m32c_setup_indexl_size_src,
	 .existCheckProc = M32C_CheckGAMLow,
	 .cycles = 2,
	 },
	{
	 .mask = 0x00f13f,
	 .icode = 0x009033,
	 .name = "indexl_size_src",
	 .len = 2,
	 .proc = m32c_setup_indexl_size_src,
	 .existCheckProc = M32C_CheckGAMLow,
	 .cycles = 4,
	 },
	{
	 .mask = 0x00f13f,
	 .icode = 0x00b023,
	 .name = "indexld_size_src",
	 .len = 2,
	 .proc = m32c_setup_indexld_size_src,
	 .existCheckProc = M32C_CheckGAMLow,
	 .cycles = 2,
	 },
	{
	 .mask = 0x00f13f,
	 .icode = 0x00b033,
	 .name = "indexld_size_src",
	 .len = 2,
	 .proc = m32c_setup_indexld_size_src,
	 .existCheckProc = M32C_CheckGAMLow,
	 .cycles = 3,
	 },
	{
	 .mask = 0x00f13f,
	 .icode = 0x009003,
	 .name = "indexls_size_src",
	 .len = 2,
	 .proc = m32c_setup_indexls_size_src,
	 .existCheckProc = M32C_CheckGAMLow,
	 .cycles = 2,
	 },
	{
	 .mask = 0x00f13f,
	 .icode = 0x009013,
	 .name = "indexls_size_src",
	 .len = 2,
	 .proc = m32c_setup_indexls_size_src,
	 .existCheckProc = M32C_CheckGAMLow,
	 .cycles = 3,
	 },
	{
	 .mask = 0x00f13f,
	 .icode = 0x008023,
	 .name = "indexw_size_src",
	 .len = 2,
	 .proc = m32c_setup_indexw_size_src,
	 .existCheckProc = M32C_CheckGAMLow,
	 .cycles = 2,
	 },
	{
	 .mask = 0x00f13f,
	 .icode = 0x008033,
	 .name = "indexw_size_src",
	 .len = 2,
	 .proc = m32c_setup_indexw_size_src,
	 .existCheckProc = M32C_CheckGAMLow,
	 .cycles = 4,
	 },
	{
	 .mask = 0x00f13f,
	 .icode = 0x00a023,
	 .name = "indexwd_size_src",
	 .len = 2,
	 .proc = m32c_setup_indexwd_size_src,
	 .existCheckProc = M32C_CheckGAMLow,
	 .cycles = 1,
	 },
	{
	 .mask = 0x00f13f,
	 .icode = 0x00a033,
	 .name = "indexwd_size_src",
	 .len = 2,
	 .proc = m32c_setup_indexwd_size_src,
	 .existCheckProc = M32C_CheckGAMLow,
	 .cycles = 2,
	 },
	{
	 .mask = 0x00f13f,
	 .icode = 0x00c023,
	 .name = "indexws_size_src",
	 .len = 2,
	 .proc = m32c_setup_indexws_size_src,
	 .existCheckProc = M32C_CheckGAMLow,
	 .cycles = 1,
	 },
	{
	 .mask = 0x00f13f,
	 .icode = 0x00c033,
	 .name = "indexws_size_src",
	 .len = 2,
	 .proc = m32c_setup_indexws_size_src,
	 .existCheckProc = M32C_CheckGAMLow,
	 .cycles = 2,
	 },
	{
	 .mask = 0x0000ff,
	 .icode = 0x0000be,
	 .name = "int_imm",
	 .len = 1,
	 .proc = m32c_int_imm,
	 .cycles = 12,
	 },
	{
	 .mask = 0x0000ff,
	 .icode = 0x0000bf,
	 .name = "into",
	 .len = 1,
	 .proc = m32c_into,
	 .cycles = 1,
	 },
	{
	 .mask = 0x00008e,
	 .icode = 0x00008a,
	 .name = "jcnd",
	 .len = 1,
	 .proc = m32c_setup_jcnd,
	 .cycles = 1,
	 },
	{
	 .mask = 0x0000ce,
	 .icode = 0x00004a,
	 .name = "jmp_s_label",
	 .len = 1,
	 .proc = m32c_setup_jmp_s,
	 .cycles = 3,
	 },
	{
	 .mask = 0x0000ff,
	 .icode = 0x0000ce,
	 .name = "jmp_w_label",
	 .len = 1,
	 .proc = m32c_jmp_w,
	 .cycles = 3,
	 },
	{
	 .mask = 0x0000ff,
	 .icode = 0x0000cc,
	 .name = "jmp_a_label",
	 .len = 1,
	 .proc = m32c_jmp_a,
	 .cycles = 3,
	 },
	{
	 .mask = 0x00f13f,
	 .icode = 0x00c10f,
	 .name = "jmpi_w_src",
	 .len = 2,
	 .proc = m32c_setup_jmpi_w_src,
	 .existCheckProc = M32C_CheckGAMLow,
	 .cycles = 7,
	 },
	{
	 .mask = 0x00f13f,
	 .icode = 0x008001,
	 .name = "jmpi_a_src",
	 .len = 2,
	 .proc = m32c_setup_jmpi_a_src,
	 .existCheckProc = M32C_CheckGAMLow,
	 .cycles = 5,
	 },
	{
	 .mask = 0x0000ff,
	 .icode = 0x0000dc,
	 .name = "jmps_imm8",
	 .len = 1,
	 .proc = m32c_jmps_imm8,
	 .cycles = 8,
	 },
	{
	 .mask = 0x0000ff,
	 .icode = 0x0000cf,
	 .name = "jsr_w_label",
	 .len = 1,
	 .proc = m32c_jsr_w_label,
	 .cycles = 3,
	 },
	{
	 .mask = 0x0000ff,
	 .icode = 0x0000cd,
	 .name = "jsr_a_label",
	 .len = 1,
	 .proc = m32c_jsr_a_label,
	 .cycles = 3,
	 },
	{
	 .mask = 0x00f13f,
	 .icode = 0x00c11f,
	 .name = "jsri_w_src",
	 .len = 2,
	 .proc = m32c_setup_jsri_w_src,
	 .existCheckProc = M32C_CheckGAMLow,
	 .cycles = 7,
	 },
	{
	 .mask = 0x00f13f,
	 .icode = 0x009001,
	 .name = "jsri_a_src",
	 .len = 2,
	 .proc = m32c_setup_jsri_a_src,
	 .existCheckProc = M32C_CheckGAMLow,
	 .cycles = 5,
	 },
	{
	 .mask = 0x0000ff,
	 .icode = 0x0000dd,
	 .name = "jsrs_imm8",
	 .len = 1,
	 .proc = m32c_jsrs_imm8,
	 .cycles = 2,
	 },
	{
	 .mask = 0x00fff8,
	 .icode = 0x00d5a8,
	 .name = "ldc_imm16_dst",
	 .len = 2,
	 .proc = m32c_setup_ldc_imm16_dst,
	 .cycles = 1,
	 },
	{
	 .mask = 0x00fff8,
	 .icode = 0x00d528,
	 .name = "ldc_imm24_dst",
	 .len = 2,
	 .proc = m32c_setup_ldc_imm24_dst,
	 .cycles = 2,
	 },
	{
	 .mask = 0x00fff8,
	 .icode = 0x00d568,
	 .name = "ldc_imm24_dst2",
	 .len = 2,
	 .proc = m32c_setup_ldc_imm24_dst2,
	 .cycles = 2,
	 },
	{
	 .mask = 0xfff138,
	 .icode = 0x01d108,
	 .name = "ldc_src_dst",
	 .len = 3,
	 .proc = m32c_setup_ldc_src_dst,
	 .existCheckProc = M32C_CheckGAMLow,
	 .cycles = 2,
	 },
	{
	 .mask = 0x00f138,
	 .icode = 0x00d100,
	 .name = "ldc_src_dst2",
	 .len = 2,
	 .proc = m32c_setup_ldc_src_dst2,
	 .existCheckProc = M32C_CheckGAMLow,
	 .cycles = 2,
	 },
	{
	 .mask = 0xfff138,
	 .icode = 0x01d100,
	 .name = "ldc_src_dst3",
	 .len = 3,
	 .proc = m32c_setup_ldc_src_dst3,
	 .existCheckProc = M32C_CheckGAMLow,
	 .cycles = 2,
	 },
	{
	 .mask = 0x00ffff,
	 .icode = 0x00b6c3,
	 .name = "ldctx_abs16_abs24",
	 .len = 2,
	 .proc = m32c_ldctx,
	 .cycles = 10,
	 },
	{
	 .mask = 0x00fff8,
	 .icode = 0x00d5e8,
	 .name = "ldipl_imm",
	 .len = 2,
	 .proc = m32c_setup_ldipl_imm,
	 .cycles = 2,
	 },
	{
	 .mask = 0xfff03f,
	 .icode = 0x01803f,
	 .name = "max_size_immdst",
	 .len = 3,
	 .proc = m32c_setup_max_size_immdst,
	 .existCheckProc = M32C_CheckGAMLow,
	 .cycles = 3,
	 },
	{
	 .mask = 0xff800f,
	 .icode = 0x01800d,
	 .name = "max_size_srcdst",
	 .len = 3,
	 .proc = m32c_setup_max_size_srcdst,
	 .existCheckProc = M32C_CheckGAMBoth,
	 .cycles = 2,
	 },
	{
	 .mask = 0xfff03f,
	 .icode = 0x01802f,
	 .name = "min_size_immdst",
	 .len = 3,
	 .proc = m32c_setup_min_size_immdst,
	 .existCheckProc = M32C_CheckGAMLow,
	 .cycles = 3,
	 },
	{
	 .mask = 0xff800f,
	 .icode = 0x01800c,
	 .name = "min_size_srcdst",
	 .len = 3,
	 .proc = m32c_setup_min_size_srcdst,
	 .existCheckProc = M32C_CheckGAMBoth,
	 .cycles = 2,
	 },
	{
	 .mask = 0x00f03f,
	 .icode = 0x00902f,
	 .name = "mov_size_g_immdst",
	 .len = 2,
	 .proc = m32c_setup_mov_size_g_immdst,
	 .existCheckProc = M32C_CheckGAMLow,
	 .cycles = 1,
	 },
	{
	 .mask = 0xfff03f,
	 .icode = 0x09902f,
	 .name = "mov_size_g_immidst",
	 .len = 3,
	 .proc = m32c_setup_mov_size_g_immidst,
	 .existCheckProc = M32C_CheckGAMLow,
	 .cycles = 4,
	 },
	{
	 .mask = 0x00f13f,
	 .icode = 0x00b031,
	 .name = "mov_l_g_immdst",
	 .len = 2,
	 .proc = m32c_setup_mov_l_g_immdst,
	 .existCheckProc = M32C_CheckGAMLow,
	 .cycles = 2,
	 },
	{
	 .mask = 0xfff13f,
	 .icode = 0x09b031,
	 .name = "mov_l_g_immidst",
	 .len = 3,
	 .proc = m32c_setup_mov_l_g_immidst,
	 .existCheckProc = M32C_CheckGAMLow,
	 .cycles = 5,
	 },
	{
	 .mask = 0x00f030,
	 .icode = 0x00f020,
	 .name = "mov_size_q_imm4dst",
	 .len = 2,
	 .proc = m32c_setup_mov_size_q_imm4dst,
	 .existCheckProc = M32C_CheckGAMLow,
	 .cycles = 1,
	 },
	{
	 .mask = 0xfff030,
	 .icode = 0x09f020,
	 .name = "mov_size_q_imm4idst",
	 .len = 3,
	 .proc = m32c_setup_mov_size_q_imm4idst,
	 .existCheckProc = M32C_CheckGAMLow,
	 .cycles = 4,
	 },
	{
	 .mask = 0x0000fe,
	 .icode = 0x000004,
	 .name = "mov_size_s_immdst",
	 .len = 1,
	 .proc = m32c_setup_mov_size_s_immdst,
	 .cycles = 1,
	 },
	{
	 .mask = 0x0000fe,
	 .icode = 0x000014,
	 .name = "mov_size_s_immdst",
	 .len = 1,
	 .proc = m32c_setup_mov_size_s_immdst,
	 .cycles = 2,
	 },
	{
	 .mask = 0x0000fe,
	 .icode = 0x000024,
	 .name = "mov_size_s_immdst",
	 .len = 1,
	 .proc = m32c_setup_mov_size_s_immdst,
	 .cycles = 2,
	 },
	{
	 .mask = 0x0000fe,
	 .icode = 0x000034,
	 .name = "mov_size_s_immdst",
	 .len = 1,
	 .proc = m32c_setup_mov_size_s_immdst,
	 .cycles = 2,
	 },
	{
	 .mask = 0x00fffe,
	 .icode = 0x000904,
	 .name = "mov_size_s_immidst",
	 .len = 2,
	 .proc = m32c_setup_mov_size_s_immidst,
	 .cycles = 3,
	 },
	{
	 .mask = 0x00fffe,
	 .icode = 0x000914,
	 .name = "mov_size_s_immidst",
	 .len = 2,
	 .proc = m32c_setup_mov_size_s_immidst,
	 .cycles = 5,
	 },
	{
	 .mask = 0x00fffe,
	 .icode = 0x000924,
	 .name = "mov_size_s_immidst",
	 .len = 2,
	 .proc = m32c_setup_mov_size_s_immidst,
	 .cycles = 5,
	 },
	{
	 .mask = 0x00fffe,
	 .icode = 0x000934,
	 .name = "mov_size_s_immidst",
	 .len = 2,
	 .proc = m32c_setup_mov_size_s_immidst,
	 .cycles = 5,
	 },
	{
	 .mask = 0x0000de,
	 .icode = 0x00009c,
	 .name = "mov_size_s_imma0a1",
	 .len = 1,
	 .proc = m32c_setup_mov_size_s_imma0a1,
	 .cycles = 1,		/* fixed in setup */
	 },
	{
	 .mask = 0x0000ce,
	 .icode = 0x000002,
	 .name = "mov_size_z_0dst",
	 .len = 1,
	 .proc = m32c_setup_mov_size_z_0dst,
	 .cycles = 1,
	 },
	{
	 .mask = 0x00ffce,
	 .icode = 0x000902,
	 .name = "mov_size_z_0idst",
	 .len = 2,
	 .proc = m32c_setup_mov_size_z_0idst,
	 .cycles = 4,
	 },
	{
	 .mask = 0x00800f,
	 .icode = 0x00800b,
	 .name = "mov_size_g_srcdst",
	 .len = 2,
	 .proc = m32c_setup_mov_size_g_srcdst,
	 .existCheckProc = M32C_CheckGAMBoth,
	 .cycles = 2,
	 },
	{
	 .mask = 0xff800f,
	 .icode = 0x41800b,
	 .name = "mov_size_g_isrcdst",
	 .len = 3,
	 .proc = m32c_setup_mov_size_g_isrcdst,
	 .existCheckProc = M32C_CheckGAMBoth,
	 .cycles = 5,
	 },
	{
	 .mask = 0xff800f,
	 .icode = 0x09800b,
	 .name = "mov_size_g_srcidst",
	 .len = 3,
	 .proc = m32c_setup_mov_size_g_srcidst,
	 .existCheckProc = M32C_CheckGAMBoth,
	 .cycles = 5,
	 },
	{
	 .mask = 0xff800f,
	 .icode = 0x49800b,
	 .name = "mov_size_g_isrcidst",
	 .len = 3,
	 .proc = m32c_setup_mov_size_g_isrcidst,
	 .existCheckProc = M32C_CheckGAMBoth,
	 .cycles = 8,
	 },
	{
	 .mask = 0x00810f,
	 .icode = 0x008103,
	 .name = "mov_l_g_srcdst",
	 .len = 2,
	 .proc = m32c_setup_mov_l_g_srcdst,
	 .existCheckProc = M32C_CheckGAMBoth,
	 .cycles = 2,
	 },
	{
	 .mask = 0xff810f,
	 .icode = 0x418103,
	 .name = "mov_l_g_isrcdst",
	 .len = 3,
	 .proc = m32c_setup_mov_l_g_isrcdst,
	 .existCheckProc = M32C_CheckGAMBoth,
	 .cycles = 5,
	 },
	{
	 .mask = 0xff810f,
	 .icode = 0x098103,
	 .name = "mov_l_g_srcidst",
	 .len = 3,
	 .proc = m32c_setup_mov_l_g_srcidst,
	 .existCheckProc = M32C_CheckGAMBoth,
	 .cycles = 5,
	 },
	{
	 .mask = 0xff810f,
	 .icode = 0x498103,
	 .name = "mov_l_g_isrcidst",
	 .len = 3,
	 .proc = m32c_setup_mov_l_g_isrcidst,
	 .existCheckProc = M32C_CheckGAMBoth,
	 .cycles = 8,
	 },
	{
	 .mask = 0x0000fe,
	 .icode = 0x000018,
	 .name = "mov_size_s_srcr0",
	 .len = 1,
	 .proc = m32c_setup_mov_size_src_r0,
	 .cycles = 2,
	 },
	{
	 .mask = 0x0000fe,
	 .icode = 0x000028,
	 .name = "mov_size_s_srcr0",
	 .len = 1,
	 .proc = m32c_setup_mov_size_src_r0,
	 .cycles = 2,
	 },
	{
	 .mask = 0x0000fe,
	 .icode = 0x000038,
	 .name = "mov_size_s_srcr0",
	 .len = 1,
	 .proc = m32c_setup_mov_size_src_r0,
	 .cycles = 2,
	 },
	{
	 .mask = 0x00fffe,
	 .icode = 0x000918,
	 .name = "mov_size_s_isrcr0",
	 .len = 2,
	 .proc = m32c_setup_mov_size_isrc_r0,
	 .cycles = 5,
	 },
	{
	 .mask = 0x00fffe,
	 .icode = 0x000928,
	 .name = "mov_size_s_isrcr0",
	 .len = 2,
	 .proc = m32c_setup_mov_size_isrc_r0,
	 .cycles = 2,
	 },
	{
	 .mask = 0x00fffe,
	 .icode = 0x000938,
	 .name = "mov_size_s_isrcr0",
	 .len = 2,
	 .proc = m32c_setup_mov_size_isrc_r0,
	 .cycles = 2,
	 },
	{
	 .mask = 0x0000ce,
	 .icode = 0x00004e,
	 .name = "mov_size_s_srcr1",
	 .len = 1,
	 .proc = m32c_setup_mov_size_src_r1,
	 .cycles = 2,
	 },
	{
	 .mask = 0x00ffce,
	 .icode = 0x00094e,
	 .name = "mov_size_s_isrcr1",
	 .len = 2,
	 .proc = m32c_setup_mov_size_isrc_r1,
	 .cycles = 5,
	 },
	{
	 .mask = 0x0000fe,
	 .icode = 0x000010,
	 .name = "mov_size_s_r0dst",
	 .len = 1,
	 .proc = m32c_setup_mov_size_r0dst,
	 .cycles = 1,
	 },
	{
	 .mask = 0x0000fe,
	 .icode = 0x000020,
	 .name = "mov_size_s_r0dst",
	 .len = 1,
	 .proc = m32c_setup_mov_size_r0dst,
	 .cycles = 1,
	 },
	{
	 .mask = 0x0000fe,
	 .icode = 0x000030,
	 .name = "mov_size_s_r0dst",
	 .len = 1,
	 .proc = m32c_setup_mov_size_r0dst,
	 .cycles = 1,
	 },
	{
	 .mask = 0x00fffe,
	 .icode = 0x000910,
	 .name = "mov_size_s_r0idst",
	 .len = 2,
	 .proc = m32c_setup_mov_size_r0idst,
	 .cycles = 4,
	 },
	{
	 .mask = 0x00fffe,
	 .icode = 0x000920,
	 .name = "mov_size_s_r0idst",
	 .len = 2,
	 .proc = m32c_setup_mov_size_r0idst,
	 .cycles = 4,
	 },
	{
	 .mask = 0x00fffe,
	 .icode = 0x000930,
	 .name = "mov_size_s_r0idst",
	 .len = 2,
	 .proc = m32c_setup_mov_size_r0idst,
	 .cycles = 4,
	 },
	{
	 .mask = 0x0000fe,
	 .icode = 0x000058,
	 .name = "mov_l_s_srca0a1",
	 .len = 1,
	 .proc = m32c_setup_mov_l_s_srca0a1,
	 .cycles = 3,
	 },
	{
	 .mask = 0x0000fe,
	 .icode = 0x000068,
	 .name = "mov_l_s_srca0a1",
	 .len = 1,
	 .proc = m32c_setup_mov_l_s_srca0a1,
	 .cycles = 3,
	 },
	{
	 .mask = 0x0000fe,
	 .icode = 0x000078,
	 .name = "mov_l_s_srca0a1",
	 .len = 1,
	 .proc = m32c_setup_mov_l_s_srca0a1,
	 .cycles = 3,
	 },
	{
	 .mask = 0x00fffe,
	 .icode = 0x000958,
	 .name = "mov_l_s_isrca0a1",
	 .len = 2,
	 .proc = m32c_setup_mov_l_s_isrca0a1,
	 .cycles = 6,
	 },
	{
	 .mask = 0x00fffe,
	 .icode = 0x000968,
	 .name = "mov_l_s_isrca0a1",
	 .len = 2,
	 .proc = m32c_setup_mov_l_s_isrca0a1,
	 .cycles = 6,
	 },
	{
	 .mask = 0x00fffe,
	 .icode = 0x000978,
	 .name = "mov_l_s_isrca0a1",
	 .len = 2,
	 .proc = m32c_setup_mov_l_s_isrca0a1,
	 .cycles = 6,
	 },
	{
	 .mask = 0x00f03f,
	 .icode = 0x00b00f,
	 .name = "mov_size_g_dsp8spdst",
	 .len = 2,
	 .proc = m32c_setup_mov_size_g_dsp8spdst,
	 .existCheckProc = M32C_CheckGAMLow,
	 .cycles = 3,
	 },
	{
	 .mask = 0x00f03f,
	 .icode = 0x00a00f,
	 .name = "mov_size_g_srcdsp8sp",
	 .len = 2,
	 .proc = m32c_setup_mov_size_g_srcdsp8sp,
	 .existCheckProc = M32C_CheckGAMLow,
	 .cycles = 3,
	 },
	{
	 .mask = 0x00f138,
	 .icode = 0x00d118,
	 .name = "mova_srcdst",
	 .len = 2,
	 .proc = m32c_setup_mova_srcdst,
	 .existCheckProc = M32C_CheckGAMLow,
	 .cycles = 2,
	 },
	{
	 .mask = 0xfff10e,
	 .icode = 0x01b00e,
	 .name = "movdir_r0ldst",
	 .len = 3,
	 .proc = m32c_setup_movdir_r0ldst,
	 .existCheckProc = M32C_CheckGAMLow,
	 .cycles = 3,
	 },
	{
	 .mask = 0xfff10e,
	 .icode = 0x01a00e,
	 .name = "movdir_srcr0l",
	 .len = 3,
	 .proc = m32c_setup_movdir_srcr0l,
	 .existCheckProc = M32C_CheckGAMLow,
	 .cycles = 3,
	 },
	{
	 .mask = 0x00f13f,
	 .icode = 0x00b011,
	 .name = "movx_immdst",
	 .len = 2,
	 .proc = m32c_setup_movx_immdst,
	 .existCheckProc = M32C_CheckGAMLow,
	 .cycles = 2,
	 },
	{
	 .mask = 0xfff13f,
	 .icode = 0x09b011,
	 .name = "movx_immidst",
	 .len = 3,
	 .proc = m32c_setup_movx_immidst,
	 .existCheckProc = M32C_CheckGAMLow,
	 .cycles = 5,
	 },
	{
	 .mask = 0x00f03f,
	 .icode = 0x00801f,
	 .name = "mul_size_immdst",
	 .len = 2,
	 .proc = m32c_setup_mul_size_immdst,
	 .existCheckProc = M32C_CheckGAMLow,
	 .cycles = 3,
	 },
	{
	 .mask = 0xfff03f,
	 .icode = 0x09801f,
	 .name = "mul_size_immidst",
	 .len = 3,
	 .proc = m32c_setup_mul_size_immidst,
	 .existCheckProc = M32C_CheckGAMLow,
	 .cycles = 6,
	 },
	{
	 .mask = 0x00800f,
	 .icode = 0x00800c,
	 .name = "mul_size_srcdst",
	 .len = 2,
	 .proc = m32c_setup_mul_size_srcdst,
	 .existCheckProc = M32C_CheckGAMBoth,
	 .cycles = 3,
	 },
	{
	 .mask = 0xff800f,
	 .icode = 0x41800c,
	 .name = "mul_size_isrcdst",
	 .len = 3,
	 .proc = m32c_setup_mul_size_isrcdst,
	 .existCheckProc = M32C_CheckGAMBoth,
	 .cycles = 6,
	 },
	{
	 .mask = 0xff800f,
	 .icode = 0x09800c,
	 .name = "mul_size_srcidst",
	 .len = 3,
	 .proc = m32c_setup_mul_size_srcidst,
	 .existCheckProc = M32C_CheckGAMBoth,
	 .cycles = 6,
	 },
	{
	 .mask = 0xff800f,
	 .icode = 0x49800c,
	 .name = "mul_size_isrcidst",
	 .len = 3,
	 .proc = m32c_setup_mul_size_isrcidst,
	 .cycles = 9,
	 },
	{
	 .mask = 0xfff13f,
	 .icode = 0x01811f,
	 .name = "mul_l_srcr2r0",
	 .len = 3,
	 .proc = m32c_setup_mul_l_srcr2r0,
	 .existCheckProc = M32C_CheckGAMLow,
	 .cycles = 8,
	 },
	{
	 .mask = 0x00f13f,
	 .icode = 0x00c13e,
	 .name = "mulex_src",
	 .len = 2,
	 .proc = m32c_setup_mulex_src,
	 .existCheckProc = M32C_CheckGAMLow,
	 .cycles = 8,
	 },
	{
	 .mask = 0xfff13f,
	 .icode = 0x09c13e,
	 .name = "mulex_isrc",
	 .len = 3,
	 .proc = m32c_setup_mulex_isrc,
	 .existCheckProc = M32C_CheckGAMLow,
	 .cycles = 11,
	 },
	{
	 .mask = 0x00f03f,
	 .icode = 0x00800f,
	 .name = "mulu_size_immdst",
	 .len = 2,
	 .proc = m32c_setup_mulu_size_immdst,
	 .existCheckProc = M32C_CheckGAMLow,
	 .cycles = 3,
	 },
	{
	 .mask = 0xfff03f,
	 .icode = 0x09800f,
	 .name = "mulu_size_immidst",
	 .len = 3,
	 .proc = m32c_setup_mulu_size_immidst,
	 .existCheckProc = M32C_CheckGAMLow,
	 .cycles = 3,
	 },
	{
	 .mask = 0x00800f,
	 .icode = 0x008004,
	 .name = "mulu_size_srcdst",
	 .len = 2,
	 .proc = m32c_setup_mulu_size_srcdst,
	 .existCheckProc = M32C_CheckGAMBoth,
	 .cycles = 3,
	 },
	{
	 .mask = 0xff800f,
	 .icode = 0x418004,
	 .name = "mulu_size_isrcdst",
	 .len = 3,
	 .proc = m32c_setup_mulu_size_isrcdst,
	 .existCheckProc = M32C_CheckGAMBoth,
	 .cycles = 6,
	 },
	{
	 .mask = 0xff800f,
	 .icode = 0x098004,
	 .name = "mulu_size_srcidst",
	 .len = 3,
	 .proc = m32c_setup_mulu_size_srcidst,
	 .existCheckProc = M32C_CheckGAMBoth,
	 .cycles = 6,
	 },
	{
	 .mask = 0xff800f,
	 .icode = 0x498004,
	 .name = "mulu_size_isrcidst",
	 .len = 3,
	 .proc = m32c_setup_mulu_size_isrcidst,
	 .existCheckProc = M32C_CheckGAMBoth,
	 .cycles = 9,
	 },
	{
	 .mask = 0xfff13f,
	 .icode = 0x01810f,
	 .name = "mulu_l_srcr2r0",
	 .len = 3,
	 .proc = m32c_setup_mulu_l_srcr2r0,
	 .existCheckProc = M32C_CheckGAMLow,
	 .cycles = 8,
	 },
	{
	 .mask = 0x00f03f,
	 .icode = 0x00a02f,
	 .name = "neg_size_dst",
	 .len = 2,
	 .proc = m32c_setup_neg_size_dst,
	 .existCheckProc = M32C_CheckGAMLow,
	 .cycles = 1,
	 },
	{
	 .mask = 0xfff03f,
	 .icode = 0x09a02f,
	 .name = "neg_size_idst",
	 .len = 3,
	 .proc = m32c_setup_neg_size_idst,
	 .existCheckProc = M32C_CheckGAMLow,
	 .cycles = 4,
	 },
	{
	 .mask = 0x0000ff,
	 .icode = 0x0000de,
	 .name = "nop",
	 .len = 1,
	 .proc = m32c_nop,
	 .cycles = 1,
	 },
	{
	 .mask = 0x00f03f,
	 .icode = 0x00a01e,
	 .name = "not_size_dst",
	 .len = 2,
	 .proc = m32c_setup_not_size_dst,
	 .existCheckProc = M32C_CheckGAMLow,
	 .cycles = 1,
	 },
	{
	 .mask = 0xfff03f,
	 .icode = 0x09a01e,
	 .name = "not_size_idst",
	 .len = 3,
	 .proc = m32c_setup_not_size_idst,
	 .existCheckProc = M32C_CheckGAMLow,
	 .cycles = 4,
	 },
	{
	 .mask = 0x00f03f,
	 .icode = 0x00802f,
	 .name = "or_size_g_immdst",
	 .len = 2,
	 .proc = m32c_setup_or_size_g_immdst,
	 .existCheckProc = M32C_CheckGAMLow,
	 .cycles = 1,
	 },
	{
	 .mask = 0xfff03f,
	 .icode = 0x09802f,
	 .name = "or_size_g_immidst",
	 .len = 3,
	 .proc = m32c_setup_or_size_g_immidst,
	 .existCheckProc = M32C_CheckGAMLow,
	 .cycles = 4,
	 },
	{
	 .mask = 0x0000ce,
	 .icode = 0x000044,
	 .name = "or_size_s_immdst",
	 .len = 1,
	 .proc = m32c_setup_or_size_s_immdst,
	 .cycles = 1,
	 },
	{
	 .mask = 0x00ffce,
	 .icode = 0x000944,
	 .name = "or_size_s_immidst",
	 .len = 2,
	 .proc = m32c_setup_or_size_s_immidst,
	 .cycles = 4,
	 },
	{
	 .mask = 0x00800f,
	 .icode = 0x008005,
	 .name = "or_size_g_srcdst",
	 .len = 2,
	 .proc = m32c_setup_or_size_srcdst,
	 .existCheckProc = M32C_CheckGAMBoth,
	 .cycles = 1,
	 },
	{
	 .mask = 0xff800f,
	 .icode = 0x418005,
	 .name = "or_size_g_isrcdst",
	 .len = 3,
	 .proc = m32c_setup_or_size_isrcdst,
	 .existCheckProc = M32C_CheckGAMBoth,
	 .cycles = 4,
	 },
	{
	 .mask = 0xff800f,
	 .icode = 0x098005,
	 .name = "or_size_g_srcidst",
	 .len = 3,
	 .proc = m32c_setup_or_size_srcidst,
	 .existCheckProc = M32C_CheckGAMBoth,
	 .cycles = 4,
	 },
	{
	 .mask = 0xff800f,
	 .icode = 0x498005,
	 .name = "or_size_g_isrcidst",
	 .len = 3,
	 .proc = m32c_setup_or_size_isrcidst,
	 .existCheckProc = M32C_CheckGAMBoth,
	 .cycles = 7,
	 },
	{
	 .mask = 0x00f03f,
	 .icode = 0x00b02f,
	 .name = "pop_size_dst",
	 .len = 2,
	 .proc = m32c_setup_pop_size_dst,
	 .existCheckProc = M32C_CheckGAMLow,
	 .cycles = 3,
	 },
	{
	 .mask = 0xfff03f,
	 .icode = 0x09b02f,
	 .name = "pop_size_idst",
	 .len = 3,
	 .proc = m32c_setup_pop_size_idst,
	 .existCheckProc = M32C_CheckGAMLow,
	 .cycles = 6,
	 },
	{
	 .mask = 0x00fff8,
	 .icode = 0x00d3a8,
	 .name = "popc_dst1",
	 .len = 2,
	 .proc = m32c_setup_popc_dst1,
	 .cycles = 3,
	 },
	{
	 .mask = 0x00fff8,
	 .icode = 0x00d328,
	 .name = "popc_dst2",
	 .len = 2,
	 .proc = m32c_setup_popc_dst2,
	 .cycles = 4,
	 },
	{
	 .mask = 0x0000ff,
	 .icode = 0x00008e,
	 .name = "popm_dst",
	 .len = 1,
	 .proc = m32c_popm_dst,
	 .cycles = 1,
	 },
	{
	 .mask = 0x0000fe,
	 .icode = 0x0000ae,
	 .name = "push_size_imm",
	 .len = 1,
	 .proc = m32c_setup_push_size_imm,
	 .cycles = 1,
	 },
	{
	 .mask = 0x00f03f,
	 .icode = 0x00c00e,
	 .name = "push_size_src",
	 .len = 2,
	 .proc = m32c_setup_push_size_src,
	 .existCheckProc = M32C_CheckGAMLow,
	 .cycles = 1,
	 },
	{
	 .mask = 0xfff03f,
	 .icode = 0x09c00e,
	 .name = "push_size_isrc",
	 .len = 3,
	 .proc = m32c_setup_push_size_isrc,
	 .existCheckProc = M32C_CheckGAMLow,
	 .cycles = 4,
	 },
	{
	 .mask = 0x00ffff,
	 .icode = 0x00b653,
	 .name = "push_l_imm32",
	 .len = 2,
	 .proc = m32c_push_l_imm32,
	 .cycles = 3,
	 },
	{
	 .mask = 0x00f13f,
	 .icode = 0x00a001,
	 .name = "push_l_src",
	 .len = 2,
	 .proc = m32c_setup_push_l_src,
	 .existCheckProc = M32C_CheckGAMLow,
	 .cycles = 2,
	 },
	{
	 .mask = 0xfff13f,
	 .icode = 0x09a001,
	 .name = "push_l_isrc",
	 .len = 3,
	 .proc = m32c_setup_push_l_isrc,
	 .existCheckProc = M32C_CheckGAMLow,
	 .cycles = 5,
	 },
	{
	 .mask = 0x00f13f,
	 .icode = 0x00b001,
	 .name = "pusha_src",
	 .len = 2,
	 .proc = m32c_setup_pusha_src,
	 .existCheckProc = M32C_CheckGAMLow,
	 .cycles = 3,
	 },
	{
	 .mask = 0x00fff8,
	 .icode = 0x00d1a8,
	 .name = "pushc_src1",
	 .len = 2,
	 .proc = m32c_setup_pushc_src1,
	 .cycles = 1,
	 },
	{
	 .mask = 0x00fff8,
	 .icode = 0x00d128,
	 .name = "pushc_src2",
	 .len = 2,
	 .proc = m32c_setup_pushc_src2,
	 .cycles = 4,
	 },
	{
	 .mask = 0x0000ff,
	 .icode = 0x00008f,
	 .name = "pushm_src",
	 .len = 1,
	 .proc = m32c_pushm_src,
	 .cycles = 1,		/* 0 cycles with no register saved according to manual */
	 },
	{
	 .mask = 0x0000ff,
	 .icode = 0x00009e,
	 .name = "reit",
	 .len = 1,
	 .proc = m32c_reit,
	 .cycles = 6,
	 },
	{
	 .mask = 0x00ffef,
	 .icode = 0x00b843,
	 .name = "rmpa_size",
	 .len = 2,
	 .proc = m32c_setup_rmpa_size,
	 .cycles = 2,		/* One time value of 7 is done in the operation */
	 },
	{
	 .mask = 0x00f03f,
	 .icode = 0x00b02e,
	 .name = "rolc_size_dst",
	 .len = 2,
	 .proc = m32c_setup_rolc_size_dst,
	 .existCheckProc = M32C_CheckGAMLow,
	 .cycles = 1,
	 },
	{
	 .mask = 0xfff03f,
	 .icode = 0x09b02e,
	 .name = "rolc_size_idst",
	 .len = 3,
	 .proc = m32c_setup_rolc_size_idst,
	 .existCheckProc = M32C_CheckGAMLow,
	 .cycles = 4,
	 },
	{
	 .mask = 0x00f03f,
	 .icode = 0x00a02e,
	 .name = "rorc_size_dst",
	 .len = 2,
	 .proc = m32c_setup_rorc_size_dst,
	 .existCheckProc = M32C_CheckGAMLow,
	 .cycles = 1,
	 },
	{
	 .mask = 0xfff03f,
	 .icode = 0x09a02e,
	 .name = "rorc_size_idst",
	 .len = 3,
	 .proc = m32c_setup_rorc_size_idst,
	 .existCheckProc = M32C_CheckGAMLow,
	 .cycles = 4,
	 },
	{
	 .mask = 0x00f030,
	 .icode = 0x00e020,
	 .name = "rot_size_immdst",
	 .len = 2,
	 .proc = m32c_setup_rot_size_immdst,
	 .existCheckProc = M32C_CheckGAMLow,
	 .cycles = 2,
	 },
	{
	 .mask = 0xfff030,
	 .icode = 0x09e020,
	 .name = "rot_size_immidst",
	 .len = 3,
	 .proc = m32c_setup_rot_size_immidst,
	 .existCheckProc = M32C_CheckGAMLow,
	 .cycles = 5,
	 },
	{
	 .mask = 0x00f03f,
	 .icode = 0x00a03f,
	 .name = "rot_size_r1hdst",
	 .len = 2,
	 .proc = m32c_setup_rot_size_r1hdst,
	 .existCheckProc = M32C_CheckGAMLow,
	 .cycles = 3,
	 },
	{
	 .mask = 0xfff03f,
	 .icode = 0x09a03f,
	 .name = "rot_size_r1hidst",
	 .len = 3,
	 .proc = m32c_setup_rot_size_r1hidst,
	 .existCheckProc = M32C_CheckGAMLow,
	 .cycles = 6,
	 },
	{
	 .mask = 0x0000ff,
	 .icode = 0x0000df,
	 .name = "rts",
	 .len = 1,
	 .proc = m32c_rts,
	 .cycles = 6,
	 },
	{
	 .mask = 0xfff03f,
	 .icode = 0x01902e,
	 .name = "sbb_size_immdst",
	 .len = 3,
	 .proc = m32c_setup_sbb_size_immdst,
	 .existCheckProc = M32C_CheckGAMLow,
	 .cycles = 1,
	 },
	{
	 .mask = 0xff800f,
	 .icode = 0x018006,
	 .name = "sbb_size_srcdst",
	 .len = 3,
	 .proc = m32c_setup_sbb_size_srcdst,
	 .existCheckProc = M32C_CheckGAMBoth,
	 .cycles = 1,
	 },
	{
	 .mask = 0x00f130,
	 .icode = 0x00d130,
	 .name = "sccnd_dst",
	 .len = 2,
	 .proc = m32c_setup_sccnd_dst,
	 .existCheckProc = M32C_CheckGAMLow,
	 .cycles = 1,
	 },
	{
	 .mask = 0xfff130,
	 .icode = 0x09d130,
	 .name = "sccnd_idst",
	 .len = 3,
	 .proc = m32c_setup_sccnd_idst,
	 .existCheckProc = M32C_CheckGAMLow,
	 .cycles = 4,
	 },
	{
	 .mask = 0x00ffef,
	 .icode = 0x00b8c3,
	 .name = "scmpu_size",
	 .len = 2,
	 .proc = m32c_setup_scmpu_size,
	 .cycles = 3,		/* should be fixed somehow in instruction */
	 },
	{
	 .mask = 0x00f030,
	 .icode = 0x00f000,
	 .name = "sha_size_immdst",
	 .len = 2,
	 .proc = m32c_setup_sha_size_immdst,
	 .existCheckProc = M32C_CheckGAMLow,
	 .cycles = 2,
	 },
	{
	 .mask = 0xfff030,
	 .icode = 0x09f000,
	 .name = "sha_size_immidst",
	 .len = 3,
	 .proc = m32c_setup_sha_size_immidst,
	 .existCheckProc = M32C_CheckGAMLow,
	 .cycles = 5,
	 },
	{
	 .mask = 0x00f13f,
	 .icode = 0x00a021,
	 .name = "sha_l_immdst",
	 .len = 2,
	 .proc = m32c_setup_sha_l_immdst,
	 .existCheckProc = M32C_CheckGAMLow,
	 .cycles = 6,
	 },
	{
	 .mask = 0xfff13f,
	 .icode = 0x09a021,
	 .name = "sha_l_immidst",
	 .len = 3,
	 .proc = m32c_setup_sha_l_immidst,
	 .existCheckProc = M32C_CheckGAMLow,
	 .cycles = 3,
	 },
	{
	 .mask = 0x00f03f,
	 .icode = 0x00b03e,
	 .name = "sha_size_r1hdst",
	 .len = 2,
	 .proc = m32c_setup_sha_size_r1hdst,
	 .existCheckProc = M32C_CheckGAMLow,
	 .cycles = 3,
	 },
	{
	 .mask = 0xfff03f,
	 .icode = 0x09b03e,
	 .name = "sha_size_r1hidst",
	 .len = 3,
	 .proc = m32c_setup_sha_size_r1hidst,
	 .existCheckProc = M32C_CheckGAMLow,
	 .cycles = 6,
	 },
	{
	 .mask = 0x00f13f,
	 .icode = 0x00c011,
	 .name = "sha_l_r1hdst",
	 .len = 2,
	 .proc = m32c_setup_sha_l_r1hdst,
	 .existCheckProc = M32C_CheckGAMLow,
	 .cycles = 6,
	 },
	{
	 .mask = 0xfff13f,
	 .icode = 0x09c011,
	 .name = "sha_l_r1hidst",
	 .len = 3,
	 .proc = m32c_setup_sha_l_r1hidst,
	 .existCheckProc = M32C_CheckGAMLow,
	 .cycles = 9,
	 },
	{
	 .mask = 0x00f13f,
	 .icode = 0x00c021,
	 .name = "shanc_l_immdst",
	 .len = 2,
	 .proc = m32c_setup_shanc_l_immdst,
	 .existCheckProc = M32C_CheckGAMLow,
	 .cycles = 4,
	 },
	{
	 .mask = 0xfff13f,
	 .icode = 0x09c021,
	 .name = "shanc_l_immidst",
	 .len = 3,
	 .proc = m32c_setup_shanc_l_immidst,
	 .existCheckProc = M32C_CheckGAMLow,
	 .cycles = 7,
	 },
	{
	 .mask = 0x00f030,
	 .icode = 0x00e000,
	 .name = "shl_size_immdst",
	 .len = 2,
	 .proc = m32c_setup_shl_size_immdst,
	 .existCheckProc = M32C_CheckGAMLow,
	 .cycles = 2,
	 },
	{
	 .mask = 0xfff030,
	 .icode = 0x09e000,
	 .name = "shl_size_immidst",
	 .len = 3,
	 .proc = m32c_setup_shl_size_immidst,
	 .existCheckProc = M32C_CheckGAMLow,
	 .cycles = 5,
	 },
	{
	 .mask = 0x00f13f,
	 .icode = 0x009021,
	 .name = "shl_l_immdst",
	 .len = 2,
	 .proc = m32c_setup_shl_l_immdst,
	 .existCheckProc = M32C_CheckGAMLow,
	 .cycles = 6,
	 },
	{
	 .mask = 0xfff13f,
	 .icode = 0x099021,
	 .name = "shl_l_immidst",
	 .len = 3,
	 .proc = m32c_setup_shl_l_immidst,
	 .existCheckProc = M32C_CheckGAMLow,
	 .cycles = 6,
	 },
	{
	 .mask = 0x00f03f,
	 .icode = 0x00a03e,
	 .name = "shl_size_r1hdst",
	 .len = 2,
	 .proc = m32c_setup_shl_size_r1hdst,
	 .existCheckProc = M32C_CheckGAMLow,
	 .cycles = 3,
	 },
	{
	 .mask = 0xfff03f,
	 .icode = 0x09a03e,
	 .name = "shl_size_r1hidst",
	 .len = 3,
	 .proc = m32c_setup_shl_size_r1hidst,
	 .existCheckProc = M32C_CheckGAMLow,
	 .cycles = 6,
	 },
	{
	 .mask = 0x00f13f,
	 .icode = 0x00c001,
	 .name = "shl_l_r1hdst",
	 .len = 2,
	 .proc = m32c_setup_shl_l_r1hdst,
	 .existCheckProc = M32C_CheckGAMLow,
	 .cycles = 6,
	 },
	{
	 .mask = 0xfff13f,
	 .icode = 0x09c001,
	 .name = "shl_l_r1hidst",
	 .len = 3,
	 .proc = m32c_setup_shl_l_r1hidst,
	 .existCheckProc = M32C_CheckGAMLow,
	 .cycles = 9,
	 },
	{
	 .mask = 0x00f13f,
	 .icode = 0x008021,
	 .name = "shlnc_l_immdst",
	 .len = 2,
	 .proc = m32c_setup_shlnc_l_immdst,
	 .existCheckProc = M32C_CheckGAMLow,
	 .cycles = 4,
	 },
	{
	 .mask = 0xfff13f,
	 .icode = 0x098021,
	 .name = "shlnc_l_immidst",
	 .len = 3,
	 .proc = m32c_setup_shlnc_l_immidst,
	 .existCheckProc = M32C_CheckGAMLow,
	 .cycles = 7,
	 },
	{
	 .mask = 0x00ffef,
	 .icode = 0x00b283,
	 .name = "sin_size",
	 .len = 2,
	 .proc = m32c_setup_sin_size,
	 .cycles = 2,		/* 1 + 2m */
	 },
	{
	 .mask = 0x00ffef,
	 .icode = 0x00b683,
	 .name = "smovb_size",
	 .len = 2,
	 .proc = m32c_setup_smovb_size,
	 .cycles = 2,		/* 1 + 2m */
	 },
	{
	 .mask = 0x00ffef,
	 .icode = 0x00b083,
	 .name = "smovf_size",
	 .len = 2,
	 .proc = m32c_setup_smovf_size,
	 .cycles = 2,		/* 1 + 2m */
	 },
	{
	 .mask = 0x00ffef,
	 .icode = 0x00b883,
	 .name = "smovu_size",
	 .len = 2,
	 .proc = m32c_setup_smovu_size,
	 .cycles = 2,		/* 1 + 2m */
	 },
	{
	 .mask = 0x00ffef,
	 .icode = 0x00b483,
	 .name = "sout_size",
	 .len = 2,
	 .proc = m32c_setup_sout_size,
	 .cycles = 2,		/* 1 + 2m */
	 },
	{
	 .mask = 0x00ffef,
	 .icode = 0x00b803,
	 .name = "sstr_size",
	 .len = 2,
	 .proc = m32c_setup_sstr_size,
	 .cycles = 2,		/* 2 + 2m */
	 },
	{
	 .mask = 0xfff138,
	 .icode = 0x01d110,
	 .name = "stc_srcdst1",
	 .len = 3,
	 .proc = m32c_setup_stc_srcdst1,
	 .existCheckProc = M32C_CheckGAMLow,
	 .cycles = 3,
	 },
	{
	 .mask = 0xfff138,
	 .icode = 0x01d118,
	 .name = "stc_srcdst2",
	 .len = 3,
	 .proc = m32c_setup_stc_srcdst2,
	 .existCheckProc = M32C_CheckGAMLow,
	 .cycles = 2,
	 },
	{
	 .mask = 0x00f138,
	 .icode = 0x00d110,
	 .name = "stc_srcdst3",
	 .len = 2,
	 .proc = m32c_setup_stc_srcdst3,
	 .existCheckProc = M32C_CheckGAMLow,
	 .cycles = 3,
	 },
	{
	 .mask = 0x00ffff,
	 .icode = 0x00b6d3,
	 .name = "stctx_abs16abs24",
	 .len = 2,
	 .proc = m32c_stctx_abs16abs24,
	 .cycles = 10,
	 },
	{
	 .mask = 0x00f03f,
	 .icode = 0x00901f,
	 .name = "stnz_size_immdst",
	 .len = 2,
	 .proc = m32c_setup_stnz_size_immdst,
	 .existCheckProc = M32C_CheckGAMLow,
	 .cycles = 2,
	 },
	{
	 .mask = 0xfff03f,
	 .icode = 0x09901f,
	 .name = "stnz_size_immidst",
	 .len = 3,
	 .proc = m32c_setup_stnz_size_immidst,
	 .existCheckProc = M32C_CheckGAMLow,
	 .cycles = 5,
	 },
	{
	 .mask = 0x00f03f,
	 .icode = 0x00900f,
	 .name = "stz_size_immdst",
	 .len = 2,
	 .proc = m32c_setup_stz_size_immdst,
	 .existCheckProc = M32C_CheckGAMLow,
	 .cycles = 2,
	 },
	{
	 .mask = 0xfff03f,
	 .icode = 0x09900f,
	 .name = "stz_size_immidst",
	 .len = 3,
	 .proc = m32c_setup_stz_size_immidst,
	 .existCheckProc = M32C_CheckGAMLow,
	 .cycles = 5,
	 },
	{
	 .mask = 0x00f03f,
	 .icode = 0x00903f,
	 .name = "stzx_size_imm1imm2dst",
	 .len = 2,
	 .proc = m32c_setup_stzx_size_imm1imm2dst,
	 .existCheckProc = M32C_CheckGAMLow,
	 .cycles = 3,
	 },
	{
	 .mask = 0xfff03f,
	 .icode = 0x09903f,
	 .name = "stzx_size_imm1imm2idst",
	 .len = 3,
	 .proc = m32c_setup_stzx_size_imm1imm2idst,
	 .existCheckProc = M32C_CheckGAMLow,
	 .cycles = 7,
	 },
	{
	 .mask = 0x00f03f,
	 .icode = 0x00803e,
	 .name = "sub_size_g_immdst",
	 .len = 2,
	 .proc = m32c_setup_sub_size_g_immdst,
	 .existCheckProc = M32C_CheckGAMLow,
	 .cycles = 1,
	 },
	{
	 .mask = 0xfff03f,
	 .icode = 0x09803e,
	 .name = "sub_size_g_immidst",
	 .len = 3,
	 .proc = m32c_setup_sub_size_g_immidst,
	 .existCheckProc = M32C_CheckGAMLow,
	 .cycles = 4,
	 },
	{
	 .mask = 0x00f13f,
	 .icode = 0x009031,
	 .name = "sub_l_g_immdst",
	 .len = 2,
	 .proc = m32c_setup_sub_l_g_immdst,
	 .existCheckProc = M32C_CheckGAMLow,
	 .cycles = 2,
	 },
	{
	 .mask = 0xfff13f,
	 .icode = 0x099031,
	 .name = "sub_l_g_immidst",
	 .len = 3,
	 .proc = m32c_setup_sub_l_g_immidst,
	 .existCheckProc = M32C_CheckGAMLow,
	 .cycles = 5,
	 },
	{
	 .mask = 0x0000fe,
	 .icode = 0x00000e,
	 .name = "sub_size_s_immdst",
	 .len = 1,
	 .proc = m32c_setup_sub_size_s_immdst,
	 .cycles = 1,
	 },
	{
	 .mask = 0x0000fe,
	 .icode = 0x00001e,
	 .name = "sub_size_s_immdst",
	 .len = 1,
	 .proc = m32c_setup_sub_size_s_immdst,
	 .cycles = 3,
	 },
	{
	 .mask = 0x0000fe,
	 .icode = 0x00002e,
	 .name = "sub_size_s_immdst",
	 .len = 1,
	 .proc = m32c_setup_sub_size_s_immdst,
	 .cycles = 3,
	 },
	{
	 .mask = 0x0000fe,
	 .icode = 0x00003e,
	 .name = "sub_size_s_immdst",
	 .len = 1,
	 .proc = m32c_setup_sub_size_s_immdst,
	 .cycles = 3,
	 },
	{
	 .mask = 0x00fffe,
	 .icode = 0x00090e,
	 .name = "sub_size_s_immidst",
	 .len = 2,
	 .proc = m32c_setup_sub_size_s_immidst,
	 .cycles = 4,
	 },
	{
	 .mask = 0x00fffe,
	 .icode = 0x00091e,
	 .name = "sub_size_s_immidst",
	 .len = 2,
	 .proc = m32c_setup_sub_size_s_immidst,
	 .cycles = 6,
	 },
	{
	 .mask = 0x00fffe,
	 .icode = 0x00092e,
	 .name = "sub_size_s_immidst",
	 .len = 2,
	 .proc = m32c_setup_sub_size_s_immidst,
	 .cycles = 6,
	 },
	{
	 .mask = 0x00fffe,
	 .icode = 0x00093e,
	 .name = "sub_size_s_immidst",
	 .len = 2,
	 .proc = m32c_setup_sub_size_s_immidst,
	 .cycles = 6,
	 },
	{
	 .mask = 0x00800f,
	 .icode = 0x00800a,
	 .name = "sub_size_g_srcdst",
	 .len = 2,
	 .proc = m32c_setup_sub_size_g_srcdst,
	 .existCheckProc = M32C_CheckGAMBoth,
	 .cycles = 1,
	 },
	{
	 .mask = 0xff800f,
	 .icode = 0x41800a,
	 .name = "sub_size_g_isrcdst",
	 .len = 3,
	 .proc = m32c_setup_sub_size_g_isrcdst,
	 .existCheckProc = M32C_CheckGAMBoth,
	 .cycles = 4,
	 },
	{
	 .mask = 0xff800f,
	 .icode = 0x09800a,
	 .name = "sub_size_g_srcidst",
	 .len = 3,
	 .proc = m32c_setup_sub_size_g_srcidst,
	 .existCheckProc = M32C_CheckGAMBoth,
	 .cycles = 4,
	 },
	{
	 .mask = 0xff800f,
	 .icode = 0x49800a,
	 .name = "sub_size_g_isrcidst",
	 .len = 3,
	 .proc = m32c_setup_sub_size_g_isrcidst,
	 .existCheckProc = M32C_CheckGAMBoth,
	 .cycles = 7,
	 },
	{
	 .mask = 0x00810f,
	 .icode = 0x008100,
	 .name = "sub_l_g_srcdst",
	 .len = 2,
	 .proc = m32c_setup_sub_l_g_srcdst,
	 .existCheckProc = M32C_CheckGAMBoth,
	 .cycles = 2,
	 },
	{
	 .mask = 0xff810f,
	 .icode = 0x418100,
	 .name = "sub_l_g_isrcdst",
	 .len = 3,
	 .proc = m32c_setup_sub_l_g_isrcdst,
	 .existCheckProc = M32C_CheckGAMBoth,
	 .cycles = 5,
	 },
	{
	 .mask = 0xff810f,
	 .icode = 0x098100,
	 .name = "sub_l_g_srcidst",
	 .len = 3,
	 .proc = m32c_setup_sub_l_g_srcidst,
	 .existCheckProc = M32C_CheckGAMBoth,
	 .cycles = 5,
	 },
	{
	 .mask = 0xff810f,
	 .icode = 0x498100,
	 .name = "sub_l_g_isrcidst",
	 .len = 3,
	 .proc = m32c_setup_sub_l_g_isrcidst,
	 .cycles = 8,
	 },
	{
	 .mask = 0x00f13f,
	 .icode = 0x009011,
	 .name = "subx_immdst",
	 .len = 2,
	 .proc = m32c_setup_subx_immdst,
	 .existCheckProc = M32C_CheckGAMLow,
	 .cycles = 2,
	 },
	{
	 .mask = 0xfff13f,
	 .icode = 0x099011,
	 .name = "subx_immidst",
	 .len = 3,
	 .proc = m32c_setup_subx_immidst,
	 .existCheckProc = M32C_CheckGAMLow,
	 .cycles = 5,
	 },
	{
	 .mask = 0x00810f,
	 .icode = 0x008000,
	 .name = "subx_srcdst",
	 .len = 2,
	 .proc = m32c_setup_subx_srcdst,
	 .existCheckProc = M32C_CheckGAMBoth,
	 .cycles = 2,
	 },
	{
	 .mask = 0xff810f,
	 .icode = 0x418000,
	 .name = "subx_isrcdst",
	 .len = 3,
	 .proc = m32c_setup_subx_isrcdst,
	 .existCheckProc = M32C_CheckGAMBoth,
	 .cycles = 5,
	 },
	{
	 .mask = 0xff810f,
	 .icode = 0x098000,
	 .name = "subx_srcidst",
	 .len = 3,
	 .proc = m32c_setup_subx_srcidst,
	 .existCheckProc = M32C_CheckGAMBoth,
	 .cycles = 5,
	 },
	{
	 .mask = 0xff810f,
	 .icode = 0x498000,
	 .name = "subx_isrcidst",
	 .len = 3,
	 .proc = m32c_setup_subx_isrcidst,
	 .existCheckProc = M32C_CheckGAMBoth,
	 .cycles = 6,
	 },
	{
	 .mask = 0x00f03f,
	 .icode = 0x00903e,
	 .name = "tst_size_g_immdst",
	 .len = 2,
	 .proc = m32c_setup_tst_size_g_immdst,
	 .existCheckProc = M32C_CheckGAMLow,
	 .cycles = 1,
	 },
	{
	 .mask = 0x0000fe,
	 .icode = 0x00000c,
	 .name = "tst_size_s_immdst",
	 .len = 1,
	 .proc = m32c_setup_tst_size_s_immdst,
	 .cycles = 1,
	 },
	{
	 .mask = 0x0000fe,
	 .icode = 0x00001c,
	 .name = "tst_size_s_immdst",
	 .len = 1,
	 .proc = m32c_setup_tst_size_s_immdst,
	 .cycles = 3,
	 },
	{
	 .mask = 0x0000fe,
	 .icode = 0x00002c,
	 .name = "tst_size_s_immdst",
	 .len = 1,
	 .proc = m32c_setup_tst_size_s_immdst,
	 .cycles = 3,
	 },
	{
	 .mask = 0x0000fe,
	 .icode = 0x00003c,
	 .name = "tst_size_s_immdst",
	 .len = 1,
	 .proc = m32c_setup_tst_size_s_immdst,
	 .cycles = 3,
	 },
	{
	 .mask = 0xff800f,
	 .icode = 0x018009,
	 .name = "tst_size_g_srcdst",
	 .len = 3,
	 .proc = m32c_setup_tst_size_g_srcdst,
	 .existCheckProc = M32C_CheckGAMBoth,
	 .cycles = 1,
	 },
	{
	 .mask = 0x0000ff,
	 .icode = 0x0000ff,
	 .name = "und",
	 .len = 1,
	 .proc = m32c_und,
	 .cycles = 13,
	 },
	{
	 .mask = 0x00ffff,
	 .icode = 0x00b203,
	 .name = "wait",
	 .len = 2,
	 .proc = m32c_wait,
	 .cycles = 2,		/* Should be fixed in instruction */
	 },
	{
	 .mask = 0x00f038,
	 .icode = 0x00d008,
	 .name = "xchg_srcdst",
	 .len = 2,
	 .proc = m32c_setup_xchg_srcdst,
	 .existCheckProc = M32C_CheckGAMLow,
	 .cycles = 3,
	 },
	{
	 .mask = 0xfff038,
	 .icode = 0x09d008,
	 .name = "xchg_srcidst",
	 .len = 3,
	 .proc = m32c_setup_xchg_srcidst,
	 .existCheckProc = M32C_CheckGAMLow,
	 .cycles = 6,
	 },
	{
	 .mask = 0x00f03f,
	 .icode = 0x00900e,
	 .name = "xor_size_immdst",
	 .len = 2,
	 .proc = m32c_setup_xor_immdst,
	 .existCheckProc = M32C_CheckGAMLow,
	 .cycles = 1,
	 },
	{
	 .mask = 0x00800f,
	 .icode = 0x008009,
	 .name = "xor_size_srcdst",
	 .len = 2,
	 .proc = m32c_setup_xor_srcdst,
	 .existCheckProc = M32C_CheckGAMBoth,
	 .cycles = 1,
	 },
	{
	 .mask = 0xff800f,
	 .icode = 0x418009,
	 .name = "xor_size_isrcdst",
	 .len = 3,
	 .proc = m32c_setup_xor_isrcdst,
	 .existCheckProc = M32C_CheckGAMBoth,
	 .cycles = 4,
	 },
	{
	 .mask = 0xff800f,
	 .icode = 0x098009,
	 .name = "xor_size_srcidst",
	 .len = 3,
	 .proc = m32c_setup_xor_srcidst,
	 .existCheckProc = M32C_CheckGAMBoth,
	 .cycles = 4,
	 },
	{
	 .mask = 0xff800f,
	 .icode = 0x498009,
	 .name = "xor_size_isrcidst",
	 .len = 3,
	 .proc = m32c_setup_xor_isrcidst,
	 .existCheckProc = M32C_CheckGAMBoth,
	 .cycles = 7,
	 }
};

static M32C_Instruction instr_und = { 0x0000ff, 0x0000ff, "und", 1, m32c_und };

/**
 *************************************************************************
 * \fn static void Verify_InstrList(void) 
 * Do some consistency checks on instruction list
 *************************************************************************
 */
static void
Verify_InstrList(void)
{
	M32C_Instruction *instr;
	M32C_Instruction *instr2;
	int instrcnt;
	int instrlen;
	int i, j;
	instrcnt = array_size(instrlist);
	for (i = 0; i < instrcnt; i++) {
		instr = &instrlist[i];
		if ((instr->icode & instr->mask) != instr->icode) {
			fprintf(stderr, "Bad instruction in list: %s\n", instr->name);
			exit(1);
		}
		instrlen = 0;
		if (instr->mask & 0xff) {
			instrlen++;
		}
		if (instr->mask & 0xff00) {
			instrlen++;
		}
		if (instr->mask & 0xff0000) {
			instrlen++;
		}
		if (instr->len != instrlen) {
			fprintf(stderr, "Bad Instruction len in list: %s\n", instr->name);
			fprintf(stderr, "bad %s, %d %d\n", instr->name, instr->len, instrlen);
			exit(1);
		}

	}
	/* Cross instruction check */
	for (i = 0; i < instrcnt; i++) {
		instr = &instrlist[i];
		for (j = 0; j < instrcnt; j++) {
			if (i == j) {
				continue;
			}
			instr2 = &instrlist[j];
			if ((instr->icode & instr2->mask) == instr2->icode) {
				if ((instr2->icode & instr->mask) == instr->icode) {
					fprintf(stderr, "Cross match %s(%06x) and %s(%06x)\n",
						instr->name, instr->icode, instr2->name,
						instr2->icode);
					exit(1);
				}
			}
		}
	}
	fprintf(stderr, "Verification of Instruction list successful\n");
}

/**
 **************************************************************************
 * Do a instruction decoding by walking through the complete table
 **************************************************************************
 */
M32C_Instruction *
idecode_slow(uint32_t icode)
{
	M32C_Instruction *cursor, *instr = NULL;
	int debug = 0;
	int i;
	uint8_t nrMemAcc;
	int instrcnt = array_size(instrlist);
	for (i = 0; i < instrcnt; i++) {
		cursor = &instrlist[i];
		if ((icode & cursor->mask) == cursor->icode) {
			if (debug) {
				fprintf(stderr, "Match %s\n", cursor->name);
			}
			if (cursor->existCheckProc) {
				bool exists = cursor->existCheckProc(cursor, icode, &nrMemAcc);
				if (!exists) {
					continue;
				}
			}
			if (instr) {
				fprintf(stderr,
					"Instruction already exists for icode 0x%06x %s, %s\n",
					icode, instr->name, cursor->name);
				exit(1);
			}
			instr = cursor;
		}
	}
	if (!instr) {
		return &instr_und;
	}
	return instr;
}

static __UNUSED__ void
Find_Dont_MatterBits(void)
{
	int i, j;
	M32C_Instruction *instr1, *instr2;
	for (j = 0; j < 24; j++) {
		int matter = 0;
		for (i = 0; i < 16777216; i++) {
			instr1 = idecode_slow(i);
			instr2 = idecode_slow(i ^ (1 << j));
			if (instr1 != instr2) {
				matter++;
				//break;
			}
		}
		if (matter) {
			fprintf(stderr, "%d matters because of %d\n", j, matter);
		} else {
			fprintf(stderr, "%d doesn't matter\n", j);
		}
	}
}

static __UNUSED__ void
TestIdecoder(void)
{
	int i;
	M32C_Instruction *instr;
	for (i = 0; i < 16777216; i++) {
		instr = M32C_InstructionFind(i);
		if (idecode_slow(i)->name != instr->name) {
			fprintf(stderr, "Shit at %s, %s, icode %06x\n", instr->name,
				idecode_slow(i)->name, i);
			exit(1);
		}
	}
	fprintf(stderr, "Idecoder good\n");
}

void
M32C_IDecoderNew(void)
{
	int i, j;
	uint8_t nrMemAcc;
	int instrcnt = array_size(instrlist);
	uint32_t subtabs = 0;
	M32C_Instruction *instr;
	M32C_Instruction *cursor;
	Verify_InstrList();
	for (i = 0; i < instrcnt; i++) {
		instr = &instrlist[i];
		if (instr->len == 1) {
			instr->icode <<= 16;
			instr->mask <<= 16;
		}
		if (instr->len == 2) {
			instr->icode <<= 8;
			instr->mask <<= 8;
		}
	}
	m32c_iTab = sg_calloc(0x10000 * sizeof(M32C_Instruction *));
	for (i = 0; i < 65536; i++) {
		uint32_t icode = i << 8;
		bool twolevel;
		instr = NULL;
		twolevel = false;
		for (j = 0; j < instrcnt; j++) {
			cursor = &instrlist[j];
			if ((icode & cursor->mask) == (cursor->icode & 0xFFFF00)) {
				if (cursor->existCheckProc) {
					bool exists =
					    cursor->existCheckProc(cursor, icode, &nrMemAcc);
					if (!exists) {
						continue;
					}
				}
				if (cursor->len > 2) {
					twolevel = true;
					break;
				}
				if (instr) {
					fprintf(stderr, "Already exists for %08x, %s %s\n", icode,
						instr->name, cursor->name);
					exit(1);
				}
				instr = cursor;
			}
		}

		if (!twolevel) {
			if (!instr) {
				m32c_iTab[i] = &instr_und;
			} else {
				M32C_Instruction *dup = sg_new(M32C_Instruction);
				*dup = *instr;
				if (nrMemAcc == 1) {
					dup->cycles += 2;
				} else if (nrMemAcc == 2) {
					dup->cycles += 3;
				}
				dup->nrMemAcc = nrMemAcc;
				m32c_iTab[i] = dup;
			}
		} else {
			m32c_iTab[i] = NULL;
			m32c_iTab[i] = sg_new(M32C_Instruction);
			m32c_iTab[i]->iTab = sg_calloc(256 * sizeof(M32C_Instruction *));
			for (j = 0; j < 256; j++) {
				M32C_Instruction *dup = sg_new(M32C_Instruction);
				icode = (i << 8) | j;
				instr = idecode_slow(icode);
				*dup = *instr;
				if (nrMemAcc == 1) {
					dup->cycles += 2;
				} else if (nrMemAcc == 2) {
					dup->cycles += 3;
				}
				dup->nrMemAcc = nrMemAcc;
				m32c_iTab[i]->iTab[j] = dup;
			}
			subtabs++;
		}
	}
	fprintf(stderr, "M32C Instruction decoder initialized, %u subtables\n", subtabs);
#if 0
	TestIdecoder();
	exit(1);
#endif
}
