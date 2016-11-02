#ifndef IDECODE_H
#define IDECODE_H
#include <stdint.h>
#include <compiler_extensions.h>

#define INSTR_INDEX(icode) ( (((icode)&0xfff00000)>>20) | ((icode & 0xf0)<<8) )
#define INSTR_UNINDEX(i) ((i&0xfff)<<20 | (((i>>12)&0xf)<<4) )
#define INSTR_INDEX_MASK (0xfff000f0)
#define INSTR_INDEX_MAX (0xffff)

#define ARM_ARCH_V5	(1 << 0)
#define ARM_ARCH_V5_E	(1 << 1) /**< Enhanced DSP instructions */
#define ARM_ARCH_V7	(1 << 2)

struct ARM9;
typedef void InstructionProc(void);
extern InstructionProc **iProcTab;

typedef struct Instruction {
        uint32_t mask;
        uint32_t icode;
	uint32_t arch;
        char *name;
        InstructionProc *proc;
} Instruction;

void IDecoder_New();
Instruction * InstructionFind(uint32_t icode); 

static inline InstructionProc *
InstructionProcFind(uint32_t icode) {
        int index=INSTR_INDEX(icode);
	InstructionProc *proc=iProcTab[index];
	return proc;
}
#endif
