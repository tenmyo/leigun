#include <mips64_idecode.h>
#include <mips64_instructions.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "sgstring.h"

MIPS64InstructionProc **MIPS64iProcTab;

static MIPS64Instruction instrlist[] = {
	//{0xFC000000,0x00000000,"SPECIAL"/*,mips64_*/},
		{0xFC00003F,0x00000000,"SLL",mips64_SLL},
		{0xFC00003F,0x00000001,"MOVCI",mips64_MOVCI},
		{0xFC00003F,0x00000002,"SRL",mips64_SRL},
		{0xFC00003F,0x00000003,"SRA",mips64_SRA},
		{0xFC00003F,0x00000004,"SLLV",mips64_SLLV},
		{0xFC00003F,0x00000006,"SRLV",mips64_SRLV},
		{0xFC00003F,0x00000007,"SRAV",mips64_SRAV},
		{0xFC00003F,0x00000008,"JR",mips64_JR},
		{0xFC00003F,0x00000009,"JALR",mips64_JALR},
		{0xFC00003F,0x0000000A,"MOVZ",mips64_MOVZ},
		{0xFC00003F,0x0000000B,"MOVN",mips64_MOVN},
		{0xFC00003F,0x0000000C,"SYSCALL",mips64_SYSCALL},
		{0xFC00003F,0x0000000D,"BREAK",mips64_BREAK },
		{0xFC00003F,0x0000000F,"SYNC",mips64_SYNC },
		{0xFC00003F,0x00000010,"MFHI",mips64_MFHI },
		{0xFC00003F,0x00000011,"MTHI",mips64_MTHI },
		{0xFC00003F,0x00000012,"MFLO",mips64_MFLO },
		{0xFC00003F,0x00000013,"MTLO",mips64_MTLO },
		{0xFC00003F,0x00000014,"DSLLV",mips64_DSLLV },
		{0xFC00003F,0x00000016,"DSRLV",mips64_DSRLV },
		{0xFC00003F,0x00000017,"DSRAV",mips64_DSRAV },
		{0xFC00003F,0x00000018,"MULT",mips64_MULT },
		{0xFC00003F,0x00000019,"MULTU",mips64_MULTU },
		{0xFC00003F,0x0000001A,"DIV",mips64_DIV },
		{0xFC00003F,0x0000001B,"DIVU",mips64_DIVU },
		{0xFC00003F,0x0000001C,"DMULT",mips64_DMULT },
		{0xFC00003F,0x0000001D,"DMULTU",mips64_DMULTU },
		{0xFC00003F,0x0000001E,"DDIV",mips64_DMULTU },
		{0xFC00003F,0x0000001F,"DDIVU",mips64_DMULTU },
		{0xFC00003F,0x00000020,"ADD",mips64_ADD },
		{0xFC00003F,0x00000021,"ADDU",mips64_ADDU },
		{0xFC00003F,0x00000022,"SUB",mips64_SUB },
		{0xFC00003F,0x00000023,"SUBU",mips64_SUBU },
		{0xFC00003F,0x00000024,"AND",mips64_AND },
		{0xFC00003F,0x00000025,"OR",mips64_OR },
		{0xFC00003F,0x00000026,"XOR",mips64_XOR },
		{0xFC00003F,0x00000027,"NOR",mips64_NOR },
		{0xFC00003F,0x0000002A,"SLT",mips64_SLT },
		{0xFC00003F,0x0000002B,"SLTU",mips64_SLTU },
		{0xFC00003F,0x0000002C,"DADD",mips64_DADD },
		{0xFC00003F,0x0000002D,"DADDU",mips64_DADDU },
		{0xFC00003F,0x0000002E,"DSUB",mips64_DSUB },
		{0xFC00003F,0x0000002F,"DSUBU",mips64_DSUBU },
		{0xFC00003F,0x00000030,"TGE",mips64_TGE },
		{0xFC00003F,0x00000031,"TGEU",mips64_TGEU },
		{0xFC00003F,0x00000032,"TLT",mips64_TLT },
		{0xFC00003F,0x00000033,"TLTU",mips64_TLTU },
		{0xFC00003F,0x00000034,"TEQ",mips64_TEQ },
		{0xFC00003F,0x00000036,"TNE",mips64_TNE },
		{0xFC00003F,0x00000038,"DSLL",mips64_DSLL },
		{0xFC00003F,0x0000003A,"DSRL",mips64_DSRL },
		{0xFC00003F,0x0000003B,"DSRA",mips64_DSRA },
		{0xFC00003F,0x0000003C,"DSLL32",mips64_DSLL32 },
		{0xFC00003F,0x0000003E,"DSRL32",mips64_DSRL32 },
		{0xFC00003F,0x0000003F,"DSRA32",mips64_DSRA32 },

	{0xFC000000,0x04000000,"REGIMM",mips64_regimm}, /* For further local decode */
	{0xFC000000,0x08000000,"J",mips64_J},
	{0xFC000000,0x0C000000,"JAL",mips64_JAL},
	{0xFC000000,0x10000000,"BEQ",mips64_BEQ},
	{0xFC000000,0x14000000,"BNE",mips64_BNE},
	{0xFC000000,0x18000000,"BLEZ",mips64_BLEZ},
	{0xFC000000,0x1C000000,"BGTZ",mips64_BGTZ},
	{0xFC000000,0x20000000,"ADDI",mips64_ADDI},
	{0xFC000000,0x24000000,"ADDIU",mips64_ADDIU},
	{0xFC000000,0x28000000,"SLTI",mips64_SLTI},
	{0xFC000000,0x2C000000,"SLTIU",mips64_SLTIU},
	{0xFC000000,0x30000000,"ANDI",mips64_ANDI},
	{0xFC000000,0x34000000,"ORI",mips64_ORI},
	{0xFC000000,0x38000000,"XORI",mips64_XORI},
	{0xFC000000,0x3C000000,"LUI",mips64_LUI},

	{0xFC000000,0x40000000,"COP0",mips64_COP0},
	{0xFC000000,0x44000000,"COP1",mips64_COP1},
	{0xFC000000,0x48000000,"COP2",mips64_COP2},
	{0xFC000000,0x4C000000,"COP1X",mips64_COP1X},

	{0xFC000000,0x50000000,"BEQL",mips64_BEQL},
	{0xFC000000,0x54000000,"BNEL",mips64_BNEL},
	{0xFC000000,0x58000000,"BLEZL",mips64_BLEZL},
	{0xFC000000,0x5C000000,"BGTZL",mips64_BGTZL},
	{0xFC000000,0x60000000,"DADDI",mips64_DADDI},
	{0xFC000000,0x64000000,"DADDIU",mips64_DADDIU},
	{0xFC000000,0x68000000,"LDL",mips64_LDL},
	{0xFC000000,0x6c000000,"LDR",mips64_LDR},
	//{0xFC000000,0x70000000,"SPECIAL2",mips64_LDR},
		{0xFC00003F,0x70000000,"MADD",mips64_MADD},
		{0xFC00003F,0x70000001,"MADDU",mips64_MADDU},
		{0xFC00003F,0x70000002,"MUL",mips64_MUL},
		{0xFC00003F,0x70000004,"MSUB",mips64_MSUB },
		{0xFC00003F,0x70000005,"MSUBU",mips64_MSUBU},
		{0xFC00003F,0x70000020,"CLZ",mips64_CLZ},
		{0xFC00003F,0x70000021,"CLO",mips64_CLO},
		{0xFC00003F,0x70000024,"DCLZ",mips64_DCLZ},
		{0xFC00003F,0x70000025,"DCLO",mips64_DCLO},
		{0xFC00003F,0x7000003f,"SDBBP",mips64_SDBBP},
	

	{0xFC000000,0x74000000,"JALX",mips64_JALX},
	{0xFC000000,0x78000000,"MDMX",mips64_MDMX},
	//{0xFC000000,0x7C000000,"SPECIAL3",mips64_MDMX},
		{0xFC00003F,0x7C000000,"EXT",mips64_EXT},
		{0xFC00003F,0x7C000001,"DEXTM",mips64_DEXTM},
		{0xFC00003F,0x7C000002,"DEXTU",mips64_DEXTU},
		{0xFC00003F,0x7C000003,"DEXT",mips64_DEXT},
		{0xFC00003F,0x7C000004,"INS",mips64_INS},
		{0xFC00003F,0x7C000005,"DINSM",mips64_DINSM},
		{0xFC00003F,0x7C000006,"DINSU",mips64_DINSU},
		{0xFC00003F,0x7C000007,"DINS",mips64_DINS},
		{0xFC00003F,0x7C000020,"BSHFL",mips64_BSHFL},
		{0xFC00003F,0x7C000024,"DBSHFL",mips64_DBSHFL},
		{0xFC00003F,0x7C00003B,"RDHWR",mips64_RDHWR},
	
	{0xFC000000,0x80000000,"LB",mips64_LB },
	{0xFC000000,0x84000000,"LH",mips64_LH },
	{0xFC000000,0x88000000,"LWL",mips64_LWL },
	{0xFC000000,0x8C000000,"LW",mips64_LW },
	{0xFC000000,0x90000000,"LBU",mips64_LBU },
	{0xFC000000,0x94000000,"LHU",mips64_LHU },
	{0xFC000000,0x98000000,"LWR",mips64_LWR },
	{0xFC000000,0x9C000000,"LWU",mips64_LWU },
	{0xFC000000,0xA0000000,"SB",mips64_SB },
	{0xFC000000,0xA4000000,"SH",mips64_SH },
	{0xFC000000,0xA8000000,"SWL",mips64_SWL },
	{0xFC000000,0xAC000000,"SW",mips64_SW },
	{0xFC000000,0xB0000000,"SDL",mips64_SDL },
	{0xFC000000,0xB4000000,"SDR",mips64_SDR },
	{0xFC000000,0xB8000000,"SWR",mips64_SWR },
	{0xFC000000,0xBC000000,"CACHE",mips64_SWR },
	{0xFC000000,0xC0000000,"LL",mips64_LL },
	{0xFC000000,0xC4000000,"LWC1",mips64_LWC1 },
	{0xFC000000,0xC8000000,"LWC2",mips64_LWC2 },
	{0xFC000000,0xCC000000,"PREF",mips64_PREF },
	{0xFC000000,0xD0000000,"LLD",mips64_LLD },
	{0xFC000000,0xD4000000,"LDC1",mips64_LDC1 },
	{0xFC000000,0xD8000000,"LDC2",mips64_LDC2 },
	{0xFC000000,0xDC000000,"LD",mips64_LD },
	{0xFC000000,0xE0000000,"SC",mips64_SC },
	{0xFC000000,0xE4000000,"SWC1",mips64_SWC1 },
	{0xFC000000,0xE8000000,"SWC2",mips64_SWC2 },
	//{0xFC000000,0xEC000000,"???",mips64_??? },
	{0xFC000000,0xF0000000,"SCD",mips64_SCD }, 
	{0xFC000000,0xF4000000,"SDC1",mips64_SDC1 },
	{0xFC000000,0xF8000000,"SDC2",mips64_SDC2 },
	{0xFC000000,0xFC000000,"SD",mips64_SD },
	
};

void
MIPS64_IDecoderNew(int variant) 
{
	int i,j;
	MIPS64Instruction *instr;
	int tabsize = sizeof(MIPS64InstructionProc *)*(INSTR_INDEX_MAX+1);
	int nr_instructions = sizeof(instrlist) / sizeof(MIPS64Instruction);
	MIPS64iProcTab = sg_calloc(tabsize);
	for(i=0;i<=INSTR_INDEX_MAX;i++) {
		uint32_t icode = INSTR_UNINDEX(i);
		for(j=0;j<nr_instructions;j++) {
			instr = &instrlist[j];
			if(instr->value == (icode & instr->mask)) {
				if(MIPS64iProcTab[i]) {
					fprintf(stderr,"conflict\n");	
				}
				MIPS64iProcTab[i] = instr->proc;	
			}
		}

	}
	fprintf(stderr,"MIPS instruction decoder initialized\n");
	
}
#ifdef TEST
int 
main() {
	int i;
	int nr_instructions = sizeof(instrlist) / sizeof(MIPS64Instruction);
	MIPS64_IDecoderNew(0);
	for(i=0;i<nr_instructions;i++) {
		MIPS64Instruction *instr = &instrlist[i];
		fprintf(stdout,"void\nmips64_%s(void) {\n\tfprintf(stderr,\"instruction %s not implemented\\n\");\n}\n\n",instr->name,instr->name);
		//fprintf(stdout,"void mips64_%s(void);\n",instr->name);
	}
}	
#endif
