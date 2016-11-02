#ifndef _SH4_IDECODE_H
#define _SH4_IDECODE_H
#include <stdint.h>
typedef void SH4_InstructionProc(void);

typedef struct SH4_Instruction {
        uint16_t mask;
        uint16_t opcode;
        char *name;
        SH4_InstructionProc *iproc;
	uint32_t flags;
} SH4_Instruction;
#define SH4INST_ILLSLOT		(1)
#define SH4INST_SLOTFPUDIS	(2)

extern SH4_InstructionProc **sh4_iProcTab;
extern SH4_Instruction **sh4_instrTab;


static inline SH4_InstructionProc *
SH4_InstructionProcFind(uint16_t icode) {
        return sh4_iProcTab[icode];
}

static inline SH4_Instruction *
SH4_InstructionFind(uint16_t icode) {
        return sh4_instrTab[icode];
}

void SH4_IDecoderNew();

#endif
