#include <stdint.h>

typedef void M16C_InstructionProc(void);

typedef struct M16C_Instruction {
	uint16_t mask;
	uint16_t icode;
	char *name;
	int len;
	M16C_InstructionProc *proc;
} M16C_Instruction;

extern M16C_InstructionProc **iProcTab;
extern M16C_Instruction **iTab;

void M16C_IDecoderNew(void);

static inline M16C_Instruction *
M16C_InstructionFind(uint16_t icode)
{
	return iTab[icode];
}

static inline M16C_InstructionProc *
M16C_InstructionProcFind(uint16_t icode)
{
	return iProcTab[icode];
}
