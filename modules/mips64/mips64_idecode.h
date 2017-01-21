#include <stdint.h>

#define INSTR_INDEX(icode)  ((((icode)&0xFC000000)>>20) | (icode & 0x3f))
#define INSTR_UNINDEX(i) (((i&0xfc0)<<20) | (i&0x3f))
#define INSTR_INDEX_MASK (0xFC00003f)
#define INSTR_INDEX_MAX (0xfff)

typedef void MIPS64InstructionProc(void);
extern MIPS64InstructionProc **Mips64iProcTab;

typedef struct MIPS64Instruction {
	uint32_t mask;
	uint32_t value;
	char *name;
	MIPS64InstructionProc *proc;
	struct Instruction *next;
} MIPS64Instruction;

void MIPS64_IDecoderNew(int variant);
