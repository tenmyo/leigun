#ifndef _IDECODE_AVR8_H
#define _IDECODE_AVR8_H
//include <instructions_avr8.h>

#define AVR8_VARIANT_PC16   (1)
#define AVR8_VARIANT_PC24   (2)
#define AVR8_VARIANT_ALL    (AVR8_VARIANT_PC16 | AVR8_VARIANT_PC24)

typedef void AVR8_InstructionProc(void);
typedef struct AVR8_Instruction {
	uint16_t opcode;
	uint16_t mask;
	const char *name;
	AVR8_InstructionProc *iproc;
	int cycles;
	int length;		/* number of words */
    uint32_t cpuVariant;
} AVR8_Instruction;

extern AVR8_InstructionProc **avr8_iProcTab;
extern AVR8_Instruction **avr8_instrTab;
void AVR8_IDecoderNew(uint32_t cpuVariant);

static inline AVR8_InstructionProc *
AVR8_InstructionProcFind(uint16_t icode)
{
	return avr8_iProcTab[icode];
}

static inline AVR8_Instruction *
AVR8_InstructionFind(uint16_t icode)
{
	return avr8_instrTab[icode];
}
#endif
