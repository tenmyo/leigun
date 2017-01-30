#include <stdint.h>

typedef void MOS6510_InstructionProc(void);
extern MOS6510_InstructionProc **iProcTab;

typedef struct MOS6510_Instruction {
	uint8_t mask;
	uint8_t icode;
	char *name;
	MOS6510_InstructionProc *proc;
} MOS6510_Instruction;

void M6510_IDecoderNew();

static inline MOS6510_InstructionProc *
MOS_InststructionProcFind(uint16_t icode)
{
	return iProcTab[icode];
}
