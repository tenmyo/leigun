#include <stdint.h>

typedef void InstructionProc(void);
extern InstructionProc **iProcTab;

typedef struct Instruction {
	uint16_t mask;
	uint16_t icode;
	char *name;
	InstructionProc *proc;
} Instruction;

void CF_IDecoderNew();

static inline InstructionProc *
InststructionProcFind(uint16_t icode)
{
	return iProcTab[icode];
}

Instruction *CF_InstructionFind(uint16_t icode);
