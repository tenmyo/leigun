typedef void InstructionProc(uint32_t icode);
extern InstructionProc **iProcTab;

#define INSTR_INDEX(icode) ((((icode)&0xfc000000)>>16) | ((icode & 0x7fe)>>1))
#define INSTR_UNINDEX(i) ((i&0xfc00)<<16 | (((i)&0x3ff)<<1) )
#define INSTR_INDEX_MAX (0xffff)

typedef struct Instruction {
	uint32_t mask;
	uint32_t value;
	char *name;
	InstructionProc *proc;
} Instruction;

void PPCIDecoder_New(int cpu_type);
static inline InstructionProc *
InstructionProcFind(uint32_t icode)
{
	int index = INSTR_INDEX(icode);
	InstructionProc *proc = iProcTab[index];
	return proc;
}
