#include <stdint.h>

typedef void ThumbInstructionProc(void);
typedef struct ThumbInstruction ThumbInstruction;
extern ThumbInstructionProc **thumbIProcTab;
extern ThumbInstruction **thumbInstructionTab;

#define THUMB_INSTR_INDEX(icode) ((uint16_t)(icode))

static inline ThumbInstructionProc *
ThumbInstructionProc_Find(uint16_t icode) {
        ThumbInstructionProc *proc=thumbIProcTab[THUMB_INSTR_INDEX(icode)];
        return proc;
}

static inline ThumbInstruction *
ThumbInstruction_Find(uint16_t icode) {
        ThumbInstruction *instr = thumbInstructionTab[THUMB_INSTR_INDEX(icode)];
        return instr;
}

struct ThumbInstruction {
        uint16_t mask;
        uint16_t value;
        char *name;
        ThumbInstructionProc *proc;
        struct ThumbInstruction *next;
};

void ThumbDecoder_New();
