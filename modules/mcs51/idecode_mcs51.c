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
	{
		.icode = 0x11, 
		.mask = 0x1f, 
		.name = "mcs51_acall", 
		.iproc = mcs51_acall, 
		.len = 2, 
		.cycles = 2,
	},
	{
		.icode = 0x28, 
		.mask = 0xf8, 
		.name = "mcs51_adda", 
		.iproc = mcs51_adda, 
		.len = 1, 
		.cycles = 1,
	},
	{
		.icode = 0x25, 
		.mask = 0xff, 
		.name = "mcs51_addadir", 
		.iproc = mcs51_addadir, 
		.len = 2, 
		.cycles = 1
	},
	{
		.icode = 0x26, 
		.mask = 0xfe, 
		.name = "mcs51_addaari", 
		.iproc = mcs51_addaari, 
		.len = 1, 
		.cycles = 1
	},
	{
		.icode = 0x24, 
		.mask = 0xff, 
		.name = "mcs51_addadata", 
		.iproc = mcs51_addadata, 
		.len = 2, 
		.cycles = 1
	},
	{
		.icode = 0x38, 
		.mask = 0xf8, 
		.name = "mcs51_addcar", 
		.iproc = mcs51_addcar, 
		.len = 1, 
		.cycles = 1
	},
	{
		.icode = 0x35, 
		.mask = 0xff, 
		.name = "mcs51_addcadir", 
		.iproc = mcs51_addcadir, 
		.len = 2, 
		.cycles = 1
	},
	{
		.icode = 0x36, 
		.mask = 0xfe, 
		.name = "mcs51_addcaari", 
		.iproc = mcs51_addcaari, 
		.len = 1, 
		.cycles = 1
	},
	{
		.icode = 0x34, 
		.mask = 0xff, 
		.name= "mcs51_addcadata", 
		.iproc = mcs51_addcadata, 
		.len = 2, 
		.cycles = 1
	},
	{
		.icode = 0x01, 
		.mask = 0x1f, 
		.name = "mcs51_ajmp", 
		.iproc = mcs51_ajmp, 
		.len = 2, 
		.cycles = 2
	},
	{
		.icode = 0x58, 
		.mask = 0xf8, 
		.name = "mcs51_anlarn", 
		.iproc = mcs51_anlarn, 
		.len = 1, 
		.cycles = 1
	},
	{
		.icode = 0x55, 
		.mask = 0xff, 
		.name = "mcs51_anladir", 
		.iproc = mcs51_anladir, 
		.len = 2, 
		.cycles = 1
	},
	{
		.icode = 0x56, 
		.mask = 0xfe, 
		.name = "mcs51_anlaari", 
		.iproc = mcs51_anlaari, 
		.len = 1, 
		.cycles = 1
	},
	{
		.icode = 0x54, 
		.mask = 0xff, 
		.name = "mcs51_anladata", 
		.iproc = mcs51_anladata, 
		.len = 2, 
		.cycles = 1
	},
	{
		.icode = 0x52, 
		.mask = 0xff, 
		.name = "mcs51_anldira", 
		.iproc = mcs51_anldira, 
		.len = 2, 
		.cycles = 1
	},
	{
		.icode = 0x53, 
		.mask = 0xff, 
		.name = "mcs51_anldirdata", 
		.iproc = mcs51_anldirdata, 
		.len = 3, 
		.cycles = 2
	},
	{
		.icode = 0x82, 
		.mask = 0xff, 
		.name ="mcs51_anlcbit", 
		.iproc = mcs51_anlcbit, 
		.len = 2, 
		.cycles = 2
	},
	{
		.icode = 0xb0, 
		.mask = 0xff, 
		.name = "mcs51_anlcnbit", 
		.iproc = mcs51_anlcnbit, 
		.len = 2, 
		.cycles = 2
	},
	{
		.icode = 0xb5, 
		.mask = 0xff, 
		.name = "mcs51_cjneadirrel", 
		.iproc = mcs51_cjneadirrel, 
		.len = 3, 
		.cycles = 2
	},
	{
		.icode = 0xb4, 
		.mask = 0xff, 
		.name = "mcs51_cjneadatarel", 
		.iproc = mcs51_cjneadatarel, 
		.len = 3, 
		.cycles = 2
	},
	{
		.icode = 0xb8, 
		.mask = 0xf8, 
		.name = "mcs51_cjnerdatarel", 
		.iproc = mcs51_cjnerdatarel, 
		.len = 3, 
		.cycles = 2
	},
	{
		.icode = 0xb6, 
		.mask = 0xfe, 
		.name = "mcs51_cjneardatarel", 
		.iproc = mcs51_cjneardatarel, 
		.len = 3, 
		.cycles = 2
	},
	{
		.icode = 0xe4, 
		.mask = 0xff, 
		.name = "mcs51_clra", 
		.iproc = mcs51_clra, 
		.len = 1, 
		.cycles = 1
	},
	{
		.icode = 0xc3, 
		.mask = 0xff, 
		.name = "mcs51_clrc", 
		.iproc = mcs51_clrc, 
		.len = 1, 
		.cycles = 1
	},
	{
		.icode = 0xc2, 
		.mask = 0xff, 
		.name = "mcs51_clrbit", 
		.iproc = mcs51_clrbit, 
		.len = 2, 
		.cycles = 1
	},
	{
		.icode = 0xf4, 
		.mask = 0xff, 
		.name = "mcs51_cpla", 
		.iproc = mcs51_cpla, 
		.len = 1, 
		.cycles = 1
	},
	{
		.icode = 0xb3, 
		.mask = 0xff, 
		.name = "mcs51_cplc", 
		.iproc = mcs51_cplc, 
		.len = 1, 
		.cycles = 1
	},
	{
		.icode = 0xb2, 
		.mask = 0xff, 
		.name = "mcs51_cplbit", 
		.iproc = mcs51_cplbit, 
		.len = 2, 
		.cycles = 1
	},
	{
		.icode = 0xd4, 
		.mask = 0xff, 
		.name = "mcs51_da", 
		.iproc = mcs51_da, 
		.len = 1, 
		.cycles = 1
	},
	{
		.icode = 0x14, 
		.mask = 0xff, 
		.name = "mcs51_deca", 
		.iproc = mcs51_deca, 
		.len = 1, 
		.cycles = 1
	},
	{
		.icode = 0x18, 
		.mask = 0xf8, 
		.name = "mcs51_decr", 
		.iproc = mcs51_decr, 
		.len = 1, 
		.cycles = 1
	},
	{
		.icode = 0x15, 
		.mask = 0xff, 
		.name = "mcs51_decdir", 
		.iproc = mcs51_decdir, 
		.len = 2, 
		.cycles = 1
	},
	{
		.icode = 0x16, 
		.mask = 0xfe, 
		.name = "mcs51_decari", 
		.iproc = mcs51_decari, 
		.len = 1, 
		.cycles = 1
	},
	{
		.icode = 0x84, 
		.mask = 0xff, 
		.name = "mcs51_divab", 
		.iproc = mcs51_divab, 
		.len = 1, 
		.cycles = 4
	},
	{
		.icode = 0xd8, 
		.mask = 0xf8, 
		.name = "mcs51_djnzrrel", 
		.iproc = mcs51_djnzrrel, 
		.len = 2, 
		.cycles = 2
	},
	{
		.icode = 0xd5, 
		.mask = 0xff, 
		.name = "mcs51_djnzdirrel", 
		.iproc = mcs51_djnzdirrel, 
		.len = 3, 
		.cycles = 2
	},
	{
		.icode = 0x04, 
		.mask = 0xff, 
		.name = "mcs51_inca", 
		.iproc = mcs51_inca, 
		.len = 1, 
		.cycles = 1
	},
	{
		.icode = 0x08, 
		.mask = 0xf8, 
		.name = "mcs51_incr", 
		.iproc = mcs51_incr, 
		.len = 1, 
		.cycles = 1
	},
	{
		.icode = 0x05, 
		.mask = 0xff, 
		.name = "mcs51_incdir", 
		.iproc = mcs51_incdir, 
		.len = 2, 
		.cycles = 1
	},
	{
		.icode = 0x06, 
		.mask = 0xfe, 
		.name = "mcs51_incari", 
		.iproc = mcs51_incari, 
		.len = 1, 
		.cycles = 1
	},
	{
		.icode = 0xa3, 
		.mask = 0xff, 
		.name = "mcs51_incdptr", 
		.iproc = mcs51_incdptr, 
		.len = 1, 
		.cycles = 2
	},
	{
		.icode = 0x20, 
		.mask = 0xff, 
		.name = "mcs51_jbbitrel", 
		.iproc = mcs51_jbbitrel, 
		.len = 3, 
		.cycles = 2
	},
	{
		.icode = 0x10, 
		.mask = 0xff, 
		.name = "mcs51_jbcbitrel", 
		.iproc = mcs51_jbcbitrel, 
		.len = 3, 
		.cycles = 2
	},
	{
		.icode = 0x40, 
		.mask = 0xff, 
		.name = "mcs51_jcrel", 
		.iproc = mcs51_jcrel, 
		.len = 2, 
		.cycles = 2
	},
	{
		.icode = 0x73, 
		.mask = 0xff, 
		.name = "mcs51_jmpaadptr", 
		.iproc = mcs51_jmpaadptr, 
		.len = 1, 
		.cycles = 2
	},
	{
		.icode = 0x30, 
		.mask = 0xff, 
		.name = "mcs51_jnbbitrel", 
		.iproc = mcs51_jnbbitrel, 
		.len = 3, 
		.cycles = 2
	},
	{
		.icode = 0x50, 
		.mask = 0xff, 
		.name = "mcs51_jncrel", 
		.iproc = mcs51_jncrel, 
		.len = 2, 
		.cycles = 2
	},
	{
		.icode = 0x70, 
		.mask = 0xff, 
		.name = "mcs51_jnzrel", 
		.iproc = mcs51_jnzrel, 
		.len = 2, 
		.cycles = 2
	},
	{
		.icode = 0x60, 
		.mask = 0xff, 
		.name = "mcs51_jzrel", 
		.iproc = mcs51_jzrel, 
		.len = 2, 
		.cycles = 2
	},
	{
		.icode = 0x12, 
		.mask = 0xff, 
		.name = "mcs51_lcall", 
		.iproc = mcs51_lcall, 
		.len = 3, 
		.cycles = 2
	},
	{
		.icode = 0x02, 
		.mask = 0xff, 
		.name ="mcs51_ljmp", 
		.iproc = mcs51_ljmp, 
		.len = 3, 
		.cycles = 2
	},
	{
		.icode = 0xe8, 
		.mask = 0xf8, 
		.name = "mcs51_movarn", 
		.iproc = mcs51_movarn, 
		.len = 1, 
		.cycles = 1
	},
	{
		.icode = 0xe5, 
		.mask = 0xff, 
		.name = "mcs51_movadir", 
		.iproc = mcs51_movadir, 
		.len = 2, 
		.cycles = 1
	},
	{
		.icode = 0xe6, 
		.mask = 0xfe, 
		.name = "mcs51_movaari", 
		.iproc = mcs51_movaari, 
		.len = 1, 
		.cycles = 1
	},
	{
		.icode = 0x74, 
		.mask = 0xff, 
		.name = "mcs51_movadata", 
		.iproc = mcs51_movadata, 
		.len = 2, 
		.cycles = 1
	},
	{
		.icode = 0xf8, 
		.mask = 0xf8, 
		.name = "mcs51_movra", 
		.iproc = mcs51_movra, 
		.len = 1, 
		.cycles = 1
	},
	{
		.icode = 0xa8, 
		.mask = 0xf8, 
		.name = "mcs51_movrdir", 
		.iproc = mcs51_movrdir, 
		.len = 2, 
		.cycles = 2
	},
	{
		.icode = 0x78, 
		.mask = 0xf8, 
		.name = "mcs51_movrdata", 
		.iproc = mcs51_movrdata, 
		.len = 2, 
		.cycles = 1
	},
	{
		.icode = 0xf5, 
		.mask = 0xff, 
		.name = "mcs51_movdira", 
		.iproc = mcs51_movdira, 
		.len = 2, 
		.cycles = 1
	},
	{
		.icode = 0x88, 
		.mask = 0xf8, 
		.name = "mcs51_movdirr", 
		.iproc = mcs51_movdirr, 
		.len = 2, 
		.cycles = 2
	},
	{
		.icode = 0x85, 
		.mask = 0xff, 
		.name = "mcs51_movdirdir", 
		.iproc = mcs51_movdirdir, 
		.len = 3, 
		.cycles = 2
	},
	{
		.icode = 0x86, 
		.mask = 0xfe, 
		.name = "mcs51_movdirari", 
		.iproc = mcs51_movdirari, 
		.len = 2, 
		.cycles = 2
	},
	{
		.icode = 0x75, 
		.mask = 0xff, 
		.name = "mcs51_movdirdata", 
		.iproc = mcs51_movdirdata, 
		.len = 3, 
		.cycles = 2
	},
	{
		.icode = 0xf6, 
		.mask = 0xfe, 
		.name = "mcs51_movaria", 
		.iproc = mcs51_movaria, 
		.len = 1, 
		.cycles = 1
	},
	{
		.icode = 0xa6, 
		.mask = 0xfe, 
		.name = "mcs51_movaridir", 
		.iproc = mcs51_movaridir, 
		.len = 2, 
		.cycles = 2
	},
	{
		.icode = 0x76, 
		.mask = 0xfe, 
		.name = "mcs51_movaridata", 
		.iproc = mcs51_movaridata, 
		.len = 2, 
		.cycles = 1
	},
	{
		.icode = 0xa2, 
		.mask = 0xff, 
		.name = "mcs51_movcbit", 
		.iproc = mcs51_movcbit, 
		.len = 2, 
		.cycles = 1
	},
	{
		.icode = 0x92, 
		.mask = 0xff, 
		.name = "mcs51_movbitc", 
		.iproc = mcs51_movbitc, 
		.len = 2, 
		.cycles = 2
	},
	{
		.icode = 0x90, 
		.mask = 0xff, 
		.name = "mcs51_movdptrdata", 
		.iproc = mcs51_movdptrdata, 
		.len = 3, 
		.cycles = 2
	},
	{
		.icode = 0x93, 
		.mask = 0xff, 
		.name = "mcs51_movcaadptr", 
		.iproc = mcs51_movcaadptr, 
		.len = 1, 
		.cycles = 2
	},
	{
		.icode = 0x83, 
		.mask = 0xff, 
		.name = "mcs51_movaapc", 
		.iproc = mcs51_movaapc, 
		.len = 1, 
		.cycles = 2
	},
	{
		.icode = 0xe2, 
		.mask = 0xfe, 
		.name = "mcs51_movxaari", 
		.iproc = mcs51_movxaari, 
		.len = 1, 
		.cycles = 2
	},
	{
		.icode = 0xe0, 
		.mask = 0xff, 
		.name = "mcs51_movxaadptr", 
		.iproc = mcs51_movxaadptr, 
		.len = 1, 
		.cycles = 2
	},
	{
		.icode = 0xf2, 
		.mask = 0xfe, 
		.name = "mcs51_movxara", 
		.iproc = mcs51_movxara, 
		.len = 1, 
		.cycles = 2
	},
	{
		.icode = 0xf0, 
		.mask = 0xff, 
		.name = "mcs51_movxadptra", 
		.iproc = mcs51_movxadptra, 
		.len = 1, 
		.cycles = 2
	},
	{
		.icode = 0xa4, 
		.mask = 0xff, 
		.name = "mcs51_mulab", 
		.iproc = mcs51_mulab, 
		.len = 1, 
		.cycles = 4
	},
	{
		.icode = 0x00, 
		.mask = 0xff, 
		.name = "mcs51_nop", 
		.iproc = mcs51_nop, 
		.len = 1, 
		.cycles = 1
	},
	{
		.icode = 0x48, 
		.mask = 0xf8, 
		.name = "mcs51_orlar", 
		.iproc = mcs51_orlar, 
		.len = 1, 
		.cycles = 1
	},
	{
		.icode = 0x45, 
		.mask = 0xff, 
		.name = "mcs51_orladir", 
		.iproc = mcs51_orladir, 
		.len = 2, 
		.cycles = 1
	},
	{
		.icode = 0x46, 
		.mask = 0xfe, 
		.name = "mcs51_orlaari", 
		.iproc = mcs51_orlaari, 
		.len = 1, 
		.cycles = 1
	},
	{
		.icode = 0x44, 
		.mask = 0xff, 
		.name = "mcs51_orladata", 
		.iproc = mcs51_orladata, 
		.len = 2, 
		.cycles = 1
	},
	{
		.icode = 0x42, 
		.mask = 0xff, 
		.name = "mcs51_orldira", 
		.iproc = mcs51_orldira, 
		.len = 2, 
		.cycles = 1
	},
	{
		.icode = 0x43, 
		.mask = 0xff, 
		.name = "mcs51_orldirdata", 
		.iproc = mcs51_orldirdata, 
		.len = 3, 
		.cycles = 2
	},
	{
		.icode = 0x72, 
		.mask = 0xff, 
		.name = "mcs51_orlcbit", 
		.iproc = mcs51_orlcbit, 
		.len = 2, 
		.cycles = 2
	},
	{
		.icode = 0xa0, 
		.mask = 0xff, 
		.name = "mcs51_orlcnbit", 
		.iproc = mcs51_orlcnbit, 
		.len = 2, 
		.cycles = 2
	},
	{
		.icode = 0xd0, 
		.mask = 0xff, 
		.name = "mcs51_popdir", 
		.iproc = mcs51_popdir, 
		.len = 2, 
		.cycles = 2
	},
	{
		.icode = 0xc0, 
		.mask = 0xff, 
		.name = "mcs51_pusdir", 
		.iproc = mcs51_pushdir, 
		.len = 2, 
		.cycles = 2
	},
	{
		.icode = 0x22, 
		.mask = 0xff, 
		.name = "mcs51_ret", 
		.iproc = mcs51_ret, 
		.len = 1, 
		.cycles = 2
	},
	{
		.icode = 0x32, 
		.mask = 0xff, 
		.name = "mcs51_reti", 
		.iproc = mcs51_reti, 
		.len = 1, 
		.cycles = 2
	},
	{
		.icode = 0x23, 
		.mask = 0xff, 
		.name = "mcs51_rla", 
		.iproc = mcs51_rla, 
		.len = 1, 
		.cycles = 1
	},
	{
		.icode = 0x33, 
		.mask = 0xff, 
		.name = "mcs51_rlca", 
		.iproc = mcs51_rlca, 
		.len = 1, 
		.cycles = 1
	},
	{
		.icode = 0x03, 
		.mask = 0xff, 
		.name = "mcs51_rra", 
		.iproc = mcs51_rra, 
		.len = 1, 
		.cycles = 1
	},
	{
		.icode = 0x13, 
		.mask = 0xff, 
		.name = "mcs51_rrca", 
		.iproc = mcs51_rrca, 
		.len = 1, 
		.cycles = 1
	},
	{
		.icode = 0xd3, 
		.mask = 0xff, 
		.name = "mcs51_setbc", 
		.iproc = mcs51_setbc, 
		.len = 1, 
		.cycles = 1
	},
	{
		.icode = 0xd2, 
		.mask = 0xff, 
		.name = "mcs51_setbbit", 
		.iproc = mcs51_setbbit, 
		.len = 2, 
		.cycles = 1
	},
	{
		.icode = 0x80, 
		.mask = 0xff, 
		.name = "mcs51_sjmprel", 
		.iproc = mcs51_sjmprel, 
		.len = 2, 
		.cycles = 2
	},
	{
		.icode = 0x98, 
		.mask = 0xf8, 
		.name = "mcs51_subbar", 
		.iproc = mcs51_subbar, 
		.len = 1, 
		.cycles = 1
	},
	{
		.icode = 0x95, 
		.mask = 0xff, 
		.name = "mcs51_subbadir", 
		.iproc = mcs51_subbadir, 
		.len = 2, 
		.cycles = 1
	},
	{
		.icode = 0x96, 
		.mask = 0xfe, 
		.name = "mcs51_subbaari", 
		.iproc = mcs51_subbaari, 
		.len = 1, 
		.cycles = 1
	},
	{
		.icode = 0x94, 
		.mask = 0xff, 
		.name = "mcs51_subbadata", 
		.iproc = mcs51_subbadata, 
		.len = 2, 
		.cycles = 1
	},
	{
		.icode = 0xc4, 
		.mask = 0xff, 
		.name = "mcs51_swapa", 
		.iproc = mcs51_swapa, 
		.len = 1, 
		.cycles = 1
	},
	{
		.icode = 0xc8, 
		.mask = 0xf8, 
		.name = "mcs51_xchar", 
		.iproc = mcs51_xchar, 
		.len = 1, 
		.cycles = 1
	},
	{
		.icode = 0xc5, 
		.mask = 0xff, 
		.name = "mcs51_xchadir", 
		.iproc = mcs51_xchadir, 
		.len = 2, 
		.cycles = 1
	},
	{
		.icode = 0xc6, 
		.mask = 0xfe, 
		.name = "mcs51_xchaari", 
		.iproc = mcs51_xchaari, 
		.len = 1, 
		.cycles = 1
	},
	{
		.icode = 0xd6, 
		.mask = 0xfe, 
		.name = "mcs51_xchdaari",
		.iproc = mcs51_xchdaari, 
		.len = 1, 
		.cycles = 1
	},
	{
		.icode = 0x68, 
		.mask = 0xf8, 
		.name = "mcs51_xrlar", 
		.iproc = mcs51_xrlar, 
		.len = 1, 
		.cycles = 1
	},
	{
		.icode = 0x65, 
		.mask = 0xff, 
		.name ="mcs51_xrladir", 
		.iproc = mcs51_xrladir, 
		.len = 2, 
		.cycles = 1
	},
	{
		.icode = 0x66, 
		.mask = 0xfe, 
		.name = "mcs51_xrlaari", 
		.iproc = mcs51_xrlaari, 
		.len = 1, 
		.cycles = 1
	},
	{
		.icode = 0x64, 
		.mask = 0xff, 
		.name = "mcs51_xrladata", 
		.iproc = mcs51_xrladata, 
		.len = 2, 
		.cycles = 1
	},
	{
		.icode = 0x62, 
		.mask = 0xff, 
		.name = "mcs51_xrldira", 
		.iproc = mcs51_xrldira, 
		.len = 2, 
		.cycles = 1
	},
	{
		.icode = 0x63, 
		.mask = 0xff, 
		.name = "mcs51_xrldirdata", 
		.iproc = mcs51_xrldirdata, 
		.len = 3, 
		.cycles = 1
	},
};

void
MCS51_IDecoderNew(unsigned int cycles_multiplicator)
{
	uint32_t icode;
	int j;
	int num_instr = array_size(instrlist); 
	mcs51_iProcTab = sg_calloc(sizeof(MCS51_InstructionProc *) * 0x100);
	mcs51_instrTab = sg_calloc(sizeof(MCS51_Instruction *) * 0x100);
	for (icode = 0; icode < 256; icode++) {
		for (j = num_instr - 1; j >= 0; j--) {
			MCS51_Instruction *instr = &instrlist[j];
			if ((icode & instr->mask) == instr->icode) {
				if (mcs51_iProcTab[icode]) {
					fprintf(stdout, "conflict at %04x %s %s\n", icode,
						instr->name, mcs51_instrTab[icode]->name);
					exit(1);
				} else {
					//fprintf(stderr,"icode %d: %s\n",icode,instr->name);   
					mcs51_iProcTab[icode] = instr->iproc;
					mcs51_instrTab[icode] = instr;
				}
			}
		}
		if (mcs51_iProcTab[icode] == NULL) {
			mcs51_iProcTab[icode] = mcs51_undef;
		}
	}
	for (j = num_instr - 1; j >= 0; j--) {
		MCS51_Instruction *instr = &instrlist[j];
		instr->cycles *= cycles_multiplicator;
	}
	fprintf(stderr, "MCS51 instruction decoder with %d Instructions created\n", num_instr);
}
