#include <stdint.h>

typedef void C16x_InstructionProc(uint8_t *icodeP);

void C16x_IDecoderNew(void);

typedef struct C16x_Instruction {
        uint8_t opcode;
	uint8_t mask;
        char *name;
	int len;
        C16x_InstructionProc *proc;
        struct Instruction *next;
} C16x_Instruction;

extern C16x_Instruction **iTab;
static inline C16x_Instruction *
C16x_FindInstruction(uint8_t opcode) 
{
	return iTab[opcode];
}
