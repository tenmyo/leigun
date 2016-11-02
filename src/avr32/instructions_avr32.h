#ifndef _AVR_INSTRUCTIONS_H
#define _AVR_INSTRUCTIONS_H
typedef void AVR32_InstructionProc(void);

typedef struct AVR32_Instruction {
	uint32_t opcode;
	uint32_t mask;
	const char *name;
	AVR32_InstructionProc *iproc;
	//int cycles;
} AVR32_Instruction;

#endif
