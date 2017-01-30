/*
 * ----------------------------------------------------
 *
 * PowerPC Instruction decoder
 * (C) 2004  Lightmaze Solutions AG
 *   Author: Jochen Karrer
 *
 * Status
 *      working
 *
 * ----------------------------------------------------
 */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "idecode_ppc.h"
#include "instructions_ppc.h"
#include "sgstring.h"

Instruction instrlist[] = {
	{0xfc000000, 2 << 26, "tdi", ppc_tdi}
	,
	{0xfc000000, 3 << 26, "twi", ppc_twi}
	,
	{0xfc000000, 7 << 26, "mulli", ppc_mulli}
	,
	{0xfc000000, 8 << 26, "subfic", ppc_subfic}
	,
	{0xfc000000, 10 << 26, "cmpli", ppc_cmpli}
	,
	{0xfc000000, 11 << 26, "cmpi", ppc_cmpi}
	,
	{0xfc000000, 12 << 26, "addic", ppc_addic}
	,
	{0xfc000000, 13 << 26, "addic_", ppc_addic_}
	,
	{0xfc000000, 14 << 26, "addi", ppc_addi}
	,
	{0xfc000000, 15 << 26, "addis", ppc_addis}
	,
	{0xfc000000, 16 << 26, "bcx", ppc_bcx}
	,
	{0xfc000000, 17 << 26, "sc", ppc_sc}
	,
	{0xfc000000, 18 << 26, "bx", ppc_bx}
	,
	{0xfc0007fe, 0x4c000000, "mcrf", ppc_mcrf}
	,
	{0xfc0007fe, 0x4c000020, "bclrx", ppc_bclrx}
	,
	{0xfc0007fe, 0x4c000042, "crnor", ppc_crnor}
	,
	{0xfc0007fe, 0x4c000064, "rfi", ppc_rfi}
	,
	{0xfc0007fe, 0x4c000102, "crandc", ppc_crandc}
	,
	{0xfc0007fe, 0x4c00012c, "isync", ppc_isync}
	,
	{0xfc0007fe, 0x4c000182, "crxor", ppc_crxor}
	,
	{0xfc0007fe, 0x4c0001c2, "crnand", ppc_crnand}
	,
	{0xfc0007fe, 0x4c000202, "crand", ppc_crand}
	,
	{0xfc0007fe, 0x4c000242, "creqv", ppc_creqv}
	,
	{0xfc0007fe, 0x4c000342, "crorc", ppc_crorc}
	,
	{0xfc0007fe, 0x4c000382, "cror", ppc_cror}
	,
	{0xfc0007fe, 0x4c000420, "bcctrx", ppc_bcctrx}
	,
	{0xfc000000, 20 << 26, "rlwimix", ppc_rlwimix}
	,
	{0xfc000000, 21 << 26, "rlwinmx", ppc_rlwinmx}
	,
	{0xfc000000, 23 << 26, "rlwnmx", ppc_rlwnmx}
	,
	{0xfc000000, 24 << 26, "ori", ppc_ori}
	,
	{0xfc000000, 25 << 26, "oris", ppc_oris}
	,
	{0xfc000000, 26 << 26, "xori", ppc_xori}
	,
	{0xfc000000, 27 << 26, "xoris", ppc_xoris}
	,
	{0xfc000000, 28 << 26, "andi.", ppc_andppc_}
	,
	{0xfc000000, 29 << 26, "andis.", ppc_andis_}
	,
	{0xfc00001c, 0x78000000, "rldiclx", ppc_rldiclx}
	,
	{0xfc00001c, 0x78000004, "rldicrx", ppc_rldicrx}
	,
	{0xfc00001c, 0x78000008, "rldicx", ppc_rldicx}
	,
	{0xfc00001c, 0x7800000c, "rldimix", ppc_rldimix}
	,
	{0xfc00003e, 0x78000080, "rldclx", ppc_rldclx}
	,
	{0xfc00003e, 0x78000082, "rldcrx", ppc_rldcrx}
	,
	{0xfc0007fe, 0x7c000000, "cmp", ppc_cmp}
	,
	{0xfc0007fe, 0x7c000008, "tw", ppc_tw}
	,
	{0xfc0003fe, 0x7c000010, "subfcx", ppc_subfcx}
	,
	{0xfc0007fe, 0x7c000012, "mulhdux", ppc_mulhdux}
	,
	{0xfc0003fe, 0x7c000014, "addcx", ppc_addcx}
	,
	{0xfc0007fe, 0x7c000016, "mulhwux", ppc_mulhwux}
	,
	{0xfc0007fe, 0x7c000026, "mfcr", ppc_mfcr}
	,
	{0xfc0007fe, 0x7c000028, "lwarx", ppc_lwarx}
	,
	{0xfc0007fe, 0x7c00002a, "ldx", ppc_ldx}
	,
	{0xfc0007fe, 0x7c00002e, "lwzx", ppc_lwzx}
	,
	{0xfc0007fe, 0x7c000030, "slwx", ppc_slwx}
	,
	{0xfc0007fe, 0x7c000034, "cntlzwx", ppc_cntlzwx}
	,
	{0xfc0007fe, 0x7c000036, "sldx", ppc_sldx}
	,
	{0xfc0007fe, 0x7c000038, "andx", ppc_andx}
	,
	{0xfc0007fe, 0x7c000040, "cmpl", ppc_cmpl}
	,
	{0xfc0003fe, 0x7c000050, "subfx", ppc_subfx}
	,
	{0xfc0007fe, 0x7c00006a, "ldux", ppc_ldux}
	,
	{0xfc0007fe, 0x7c00006c, "dcbst", ppc_dcbst}
	,
	{0xfc0007fe, 0x7c00006e, "lwzux", ppc_lwzux}
	,
	{0xfc0007fe, 0x7c000074, "zntlzdx", ppc_zntlzdx}
	,
	{0xfc0007fe, 0x7c000078, "andcx", ppc_andcx}
	,
	{0xfc0007fe, 0x7c000088, "td", ppc_td}
	,
	{0xfc0007fe, 0x7c000092, "mulhdx", ppc_mulhdx}
	,
	{0xfc0007fe, 0x7c000096, "mulhwx", ppc_mulhwx}
	,
	{0xfc0007fe, 0x7c0000a6, "mfmsr", ppc_mfmsr}
	,
	{0xfc0007fe, 0x7c0000a8, "ldarx", ppc_ldarx}
	,
	{0xfc0007fe, 0x7c0000ac, "dcbf", ppc_dcbf}
	,
	{0xfc0007fe, 0x7c0000ae, "lbzx", ppc_lbzx}
	,
	{0xfc0003fe, 0x7c0000d0, "negx", ppc_negx}
	,
	{0xfc0007fe, 0x7c0000ee, "lbzux", ppc_lbzux}
	,
	{0xfc0007fe, 0x7c0000f8, "norx", ppc_norx}
	,
	{0xfc0003fe, 0x7c000110, "subfex", ppc_subfex}
	,
	{0xfc0003fe, 0x7c000114, "addex", ppc_addex}
	,
	{0xfc0007fe, 0x7c000120, "mtcrf", ppc_mtcrf}
	,
	{0xfc0007fe, 0x7c000124, "mtmsr", ppc_mtmsr}
	,
	{0xfc0007fe, 0x7c00012a, "stdx", ppc_stdx}
	,
	{0xfc0007fe, 0x7c00012c, "stwcx.", ppc_stwcx_}
	,
	{0xfc0007fe, 0x7c00012e, "stwx", ppc_stwx}
	,
	{0xfc0007fe, 0x7c00016a, "stdux", ppc_stdux}
	,
	{0xfc0007fe, 0x7c00016e, "stwux", ppc_stwux}
	,
	{0xfc0003fe, 0x7c000190, "subfzex", ppc_subfzex}
	,
	{0xfc0003fe, 0x7c000194, "addzex", ppc_addzex}
	,
	{0xfc0007fe, 0x7c0001a4, "mtsr", ppc_mtsr}
	,
	{0xfc0007fe, 0x7c0001ac, "stdcx", ppc_stdcx}
	,
	{0xfc0007fe, 0x7c0001ae, "stbx", ppc_stbx}
	,
	{0xfc0003fe, 0x7c0001d0, "subfmex", ppc_subfmex}
	,
	{0xfc0003fe, 0x7c0001d2, "mulld", ppc_mulld}
	,
	{0xfc0003fe, 0x7c0001d4, "addmex", ppc_addmex}
	,
	{0xfc0003fe, 0x7c0001d6, "mullwx", ppc_mullwx}
	,
	{0xfc0007fe, 0x7c0001e4, "mtsrin", ppc_mtsrin}
	,
	{0xfc0007fe, 0x7c0001ec, "dcbtst", ppc_dcbtst}
	,
	{0xfc0007fe, 0x7c0001ee, "stbux", ppc_stbux}
	,
	{0xfc0003fe, 0x7c000214, "addx", ppc_addx}
	,
	{0xfc0007fe, 0x7c00022c, "dcbt", ppc_dcbt}
	,

	{0xfc0007fe, 0x7c00022e, "lhzx", ppc_lhzx}
	,
	{0xfc0007fe, 0x7c000238, "eqvx", ppc_eqvx}
	,
	{0xfc0007fe, 0x7c000264, "tlbie", ppc_tlbie}
	,
	{0xfc0007fe, 0x7c00026c, "eciwx", ppc_eciwx}
	,
	{0xfc0007fe, 0x7c00026e, "lhzux", ppc_lhzux}
	,
	{0xfc0007fe, 0x7c000278, "xorx", ppc_xorx}
	,
	{0xfc0007fe, 0x7c0002a6, "mfspr", ppc_mfspr}
	,
	{0xfc0007fe, 0x7c0002aa, "lwax", ppc_lwax}
	,
	{0xfc0007fe, 0x7c0002ae, "lhax", ppc_lhax}
	,
	{0xfc0007fe, 0x7c0002e4, "tlbia", ppc_tlbia}
	,
	{0xfc0007fe, 0x7c0002e6, "mftb", ppc_mftb}
	,
	{0xfc0007fe, 0x7c0002ea, "lwaux", ppc_lwaux}
	,
	{0xfc0007fe, 0x7c0002ee, "lhaux", ppc_lhaux}
	,
	{0xfc0007fe, 0x7c00032e, "sthx", ppc_sthx}
	,
	{0xfc0007fe, 0x7c000338, "orcx", ppc_orcx}
	,
//{0xfc0007fe,0x7c000,}, // sradix stimmt was net in doku
	{0xfc0007fe, 0x7c000364, "slbie", ppc_slbie}
	,
	{0xfc0007fe, 0x7c00036c, "ecowx", ppc_ecowx}
	,
	{0xfc0007fe, 0x7c00036e, "sthux", ppc_sthux}
	,
	{0xfc0007fe, 0x7c000378, "orx", ppc_orx}
	,
	{0xfc0003fe, 0x7c000392, "divdux", ppc_divdux}
	,
	{0xfc0003fe, 0x7c000396, "divwux", ppc_divwux}
	,
	{0xfc0007fe, 0x7c0003a6, "mtspr", ppc_mtspr}
	,
	{0xfc0007fe, 0x7c0003ac, "dcbi", ppc_dcbi}
	,
	{0xfc0007fe, 0x7c0003b8, "nandx", ppc_nandx}
	,
	{0xfc0003fe, 0x7c0003d2, "divdx", ppc_divdx}
	,
	{0xfc0003fe, 0x7c0003d6, "divwx", ppc_divwx}
	,
	{0xfc0007fe, 0x7c0003e4, "slbia", ppc_slbia}
	,
	{0xfc0007fe, 0x7c000400, "mcrxr", ppc_mcrxr}
	,
	{0xfc0007fe, 0x7c00042a, "lswx", ppc_lswx}
	,
	{0xfc0007fe, 0x7c00042c, "lwbrx", ppc_lwbrx}
	,
	{0xfc0007fe, 0x7c00042e, "lfsx", ppc_lfsx}
	,
	{0xfc0007fe, 0x7c000430, "srwx", ppc_srwx}
	,
	{0xfc0007fe, 0x7c000436, "srdx", ppc_srdx}
	,
	{0xfc0007fe, 0x7c00046c, "tlbsync", ppc_tlbsync}
	,
	{0xfc0007fe, 0x7c00046e, "lfsux", ppc_lfsux}
	,
	{0xfc0007fe, 0x7c0004a6, "mfsr", ppc_mfsr}
	,
	{0xfc0007fe, 0x7c0004aa, "lswi", ppc_lswi}
	,
	{0xfc0007fe, 0x7c0004ac, "sync", ppc_sync}
	,
	{0xfc0007fe, 0x7c0004ae, "lfdx", ppc_lfdx}
	,
	{0xfc0007fe, 0x7c0004ee, "lfdux", ppc_lfdux}
	,
	{0xfc0007fe, 0x7c000526, "mfsrin", ppc_mfsrin}
	,
	{0xfc0007fe, 0x7c00052a, "stswx", ppc_stswx}
	,
	{0xfc0007fe, 0x7c00052c, "stwbrx", ppc_stwbrx}
	,
	{0xfc0007fe, 0x7c00052e, "stfsx", ppc_stfsx}
	,
	{0xfc0007fe, 0x7c00056e, "stfsux", ppc_stfsux}
	,
	{0xfc0007fe, 0x7c0005aa, "stswi", ppc_stswi}
	,
	{0xfc0007fe, 0x7c0005ae, "stfdx", ppc_stfdx}
	,
	{0xfc0007fe, 0x7c0005ec, "dcba", ppc_dcba}
	,
	{0xfc0007fe, 0x7c0005ee, "stfdux", ppc_stfdux}
	,
	{0xfc0007fe, 0x7c00062e, "lhbrx", ppc_lhbrx}
	,
	{0xfc0007fe, 0x7c000630, "srawx", ppc_srawx}
	,
	{0xfc0007fe, 0x7c000634, "sradx", ppc_sradx}
	,
	{0xfc0007fe, 0x7c000670, "srawix", ppc_srawix}
	,
	{0xfc0007fe, 0x7c0006ac, "eieio", ppc_eieio}
	,
	{0xfc0007fe, 0x7c000726, "sthbrx", ppc_sthbrx}
	,
	{0xfc0007fe, 0x7c000734, "extshx", ppc_extshx}
	,
	{0xfc0007fe, 0x7c000774, "extsbx", ppc_extsbx}
	,
	{0xfc0007fe, 0x7c0007ac, "icbi", ppc_icbi}
	,
	{0xfc0007fe, 0x7c0007ae, "stfiwx", ppc_stfiwx}
	,
	{0xfc0007fe, 0x7c0007b4, "extsw", ppc_extsw}
	,
	{0xfc0007fe, 0x7c0007ec, "dcbz", ppc_dcbz}
	,

	{0xfc000000, 32 << 26, "lwz", ppc_lwz}
	,
	{0xfc000000, 33 << 26, "lwzu", ppc_lwzu}
	,
	{0xfc000000, 34 << 26, "lbz", ppc_lbz}
	,
	{0xfc000000, 35 << 26, "lbzu", ppc_lbzu}
	,
	{0xfc000000, 36 << 26, "stw", ppc_stw}
	,
	{0xfc000000, 37 << 26, "stwu", ppc_stwu}
	,
	{0xfc000000, 38 << 26, "stb", ppc_stb}
	,
	{0xfc000000, 39 << 26, "stbu", ppc_stbu}
	,
	{0xfc000000, 40 << 26, "lhz", ppc_lhz}
	,
	{0xfc000000, 41 << 26, "lhzu", ppc_lhzu}
	,
	{0xfc000000, 42 << 26, "lha", ppc_lha}
	,
	{0xfc000000, 43 << 26, "lhau", ppc_lhau}
	,
	{0xfc000000, 44 << 26, "sth", ppc_sth}
	,
	{0xfc000000, 45 << 26, "sthu", ppc_sthu}
	,
	{0xfc000000, 46 << 26, "lmw", ppc_lmw}
	,
	{0xfc000000, 47 << 26, "stmw", ppc_stmw}
	,
	{0xfc000000, 48 << 26, "lfs", ppc_lfs}
	,
	{0xfc000000, 49 << 26, "lfsu", ppc_lfsu}
	,
	{0xfc000000, 50 << 26, "lfd", ppc_lfd}
	,
	{0xfc000000, 51 << 26, "lfdu", ppc_lfdu}
	,
	{0xfc000000, 52 << 26, "stfs", ppc_stfs}
	,
	{0xfc000000, 53 << 26, "stfsu", ppc_stfsu}
	,
	{0xfc000000, 54 << 26, "stfd", ppc_stfd}
	,
	{0xfc000000, 55 << 26, "stfdu", ppc_stfdu}
	,
	{0xfc000003, 0xe8000000, "ld", ppc_ld}
	,
	{0xfc000003, 0xe8000001, "ldu", ppc_ldu}
	,
	{0xfc000003, 0xe8000002, "lwa", ppc_lwa}
	,
	{0xfc0007fe, 0xec000024, "fdivsx", ppc_fdivsx}
	,
	{0xfc0007fe, 0xec000028, "fsubsx", ppc_fsubsx}
	,
	{0xfc0007fe, 0xec00002a, "faddsx", ppc_faddsx}
	,
	{0xfc0007fe, 0xec00002c, "fsqrtsx", ppc_fsqrtsx}
	,
	{0xfc0007fe, 0xec000030, "fresx", ppc_fsresx}
	,
	{0xfc00003e, 0xec000032, "fmulsx", ppc_fmulsx}
	,
	{0xfc00003e, 0xec000038, "fmsubsx", ppc_fmsubsx}
	,
	{0xfc00003e, 0xec00003a, "fmaddsx", ppc_fmaddsx}
	,
	{0xfc00003e, 0xec00003c, "fnmsubsx", ppc_fnmsubsx}
	,
	{0xfc00003e, 0xec00003e, "fnmaddsx", ppc_fnmaddsx}
	,
	{0xfc000003, 0xf8000000, "std", ppc_std}
	,
	{0xfc000003, 0xf8000001, "stdu", ppc_stdu}
	,
	{0xfc0007fe, 0xfc000000, "fcmpu", ppc_fcmpu}
	,
	{0xfc0007fe, 0xfc000018, "frspx", ppc_frspx}
	,
	{0xfc0007fe, 0xfc00001c, "fctiwx", ppc_fctiwx}
	,
	{0xfc0007fe, 0xfc00001e, "fctiwzx", ppc_fctiwzx}
	,
	{0xfc0007fe, 0xfc000024, "fdivx", ppc_fdivx}
	,
	{0xfc0007fe, 0xfc000028, "fsubx", ppc_fsubx}
	,
	{0xfc0007fe, 0xfc00002a, "faddx", ppc_faddx}
	,
	{0xfc0007fe, 0xfc00002c, "fsqrtx", ppc_fsqrtx}
	,
	{0xfc00003e, 0xfc00002e, "fselx", ppc_fselx}
	,
	{0xfc00003e, 0xfc000032, "fmulx", ppc_fmulx}
	,
	{0xfc0007fe, 0xfc000034, "fsqrtex", ppc_fsqrtex}
	,
	{0xfc00003e, 0xfc000038, "fmsubx", ppc_fmsubx}
	,
	{0xfc00003e, 0xfc00003a, "fmaddx", ppc_fmaddx}
	,
	{0xfc00003e, 0xfc00003c, "fnmsubx", ppc_fnmsubx}
	,
	{0xfc00003e, 0xfc00003e, "fnmaddx", ppc_fnmaddx}
	,
	{0xfc0007fe, 0xfc000040, "fcmpo", ppc_fcmpo}
	,
	{0xfc0007fe, 0xfc00004c, "mtfsb1x", ppc_mtfsb1x}
	,
	{0xfc0007fe, 0xfc000050, "fnegx", ppc_fnegx}
	,
	{0xfc0007fe, 0xfc000080, "mcrfs", ppc_mcrfs}
	,
	{0xfc0007fe, 0xfc00008c, "mtfsb0x", ppc_mtfsb0x}
	,
	{0xfc0007fe, 0xfc000090, "fmrx", ppc_fmrx}
	,
	{0xfc0007fe, 0xfc00010c, "mtfsfix", ppc_mtfsfix}
	,
	{0xfc0007fe, 0xfc000110, "fnabsx", ppc_fnabsx}
	,
	{0xfc0007fe, 0xfc000210, "fabsx", ppc_fabsx}
	,
	{0xfc0007fe, 0xfc00048e, "mffsx", ppc_mffsx}
	,
	{0xfc0007fe, 0xfc00058e, "mtfsfx", ppc_mtfsfx}
	,
	{0xfc0007fe, 0xfc00065e, "fctdix", ppc_fctdix}
	,
	{0xfc0007fe, 0xfc00069e, "fcfidx", ppc_fcfidx}
	,
	{0x0, 0x0, "", NULL}
};

Instruction instr_undefined = { 0x00000000, 0xffffffff, "und", ppc_und };

/* 
 * ---------------------------------------------------------
 * Unfortunately Global Variables are faster than structs
 * ---------------------------------------------------------
 */

InstructionProc **iProcTab;
Instruction **instructions;

void
PPCIDecoder_New(int cpu_type)
{
	int i;
	iProcTab =
	    (InstructionProc **) sg_calloc(sizeof(InstructionProc *) * (INSTR_INDEX_MAX + 1));
	instructions = (Instruction **) sg_calloc(sizeof(Instruction *) * (INSTR_INDEX_MAX + 1));
	for (i = 0; i <= INSTR_INDEX_MAX; i++) {
		Instruction *inst;
		uint32_t icode = INSTR_UNINDEX(i);
		for (inst = instrlist; inst->proc; inst++) {
			if ((icode & inst->mask) == inst->value) {
				if (iProcTab[i]) {
					fprintf(stderr, "Busy icode %08x, index %d\n", icode, i);
				} else {
					iProcTab[i] = inst->proc;
					instructions[i] = inst;
				}
			}
		}
		if (iProcTab[i] == NULL) {
			iProcTab[i] = ppc_und;
			instructions[i] = &instr_undefined;
		}
	}
	fprintf(stderr, "PPC Instruction decoder Initialized\n");
}

#ifdef TEST
int
main()
{
	PPCIDecoder_New();
	exit(0);
}
#endif
