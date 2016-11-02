#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include "instructions_m32c.h"

typedef struct M32C_Instruction M32C_Instruction;
typedef void M32C_InstructionProc(void);
typedef bool M32C_CheckAMExistence(M32C_Instruction * instr, uint32_t icode, uint8_t * nrMemAcc);

typedef void GAM_SetProc(uint32_t value, uint32_t index);
typedef void GAM_GetProc(uint32_t * value, uint32_t index);
typedef void AM2Bit_GetProc(uint32_t * value, uint32_t index);
typedef void AM2Bit_SetProc(uint32_t value, uint32_t index);
typedef uint32_t GetBit_Proc(int32_t bitindex);
typedef void SetBit_Proc(uint8_t value, int32_t bitindex);

struct M32C_Instruction {
	uint32_t mask;
	uint32_t icode;
	char *name;
	int len;
	M32C_InstructionProc *proc;
	M32C_CheckAMExistence *existCheckProc;
	uint8_t cycles;
	uint8_t nrMemAcc;
	struct M32C_Instruction **iTab;
	union {
		GetBit_Proc *getbit;
		AM2Bit_GetProc *getam2bit;
		GAM_GetProc *getdst;
		GAM_GetProc *getdstp;
	};
	union {
		SetBit_Proc *setbit;
		AM2Bit_SetProc *setam2bit;
		GAM_SetProc *setdst;
		GAM_SetProc *setdstp;
	};
	union {
		GAM_GetProc *getsrc;
		GAM_GetProc *getsrcp;
	};
	uint8_t opsize;
	uint8_t srcsize;
	uint8_t codelen_dst;
	uint8_t codelen_src;
	union {
		uint32_t Arg1;
		uint32_t Imm32;
	};
	union {
		uint32_t Arg2;
	};
#if 0
	uint8_t offsetImm1;
	uint8_t offsetImm2;
	uint8_t pcInc;
#endif
};

extern M32C_Instruction **m32c_iTab;

void M32C_IDecoderNew(void);

static inline M32C_Instruction *
M32C_InstructionFind(uint32_t icode)
{
	M32C_Instruction *instr;
	instr = m32c_iTab[icode >> 8];
	if (instr->proc) {
		return instr;
	} else {
		return instr->iTab[icode & 0xff];
	}
}

#if 0
static inline M32C_InstructionProc *
M32C_InstructionProcFind(uint16_t icode)
{
	return m32c_und;
	//return iProcTab[icode];
}
#endif

static inline int
M32C_InstructionLen(icode)
{
	M32C_Instruction *instr = M32C_InstructionFind(icode);
	return instr->len;
}
