#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "mips64_mmu.h"
#include "mips64_cpu.h"
#include "mips64_idecode.h"
#include <setjmp.h>

MIPS64Cpu gcpu_mips64;
/*
 * -----------------------------
 * Constructor for the CPU
 * -----------------------------
 */
MIPS64Cpu *
MIPS64Cpu_New(int cpu_variant)
{
	MIPS64Cpu *cpu = &gcpu_mips64;
	if (!cpu) {
		fprintf(stderr, "Out of memory allocating PowerPC CPU\n");
		exit(345);
	}
	memset(cpu, 0, sizeof(MIPS64Cpu));
	MIPS64_IDecoderNew(cpu_variant);
	return cpu;
}

static inline void
execute_instruction()
{

}

/*
 * This executes the instruction in the delay slot without incrementing
 * the PC. The Pipeline has to be refilled after this. Best restart pipeline 
 * with New PC
 */
void
MIPS64_ExecuteDelaySlot()
{
	gcpu_mips64.icode = gcpu_mips64.next_icode;
	gcpu_mips64.next_icode = 0;	/* avoid endless recursion if jmp in delay slot */
	execute_instruction();
}

void
MIPS64Cpu_Run(uint64_t start_address)
{
	setjmp(gcpu_mips64.restart_pipeline);
	gcpu_mips64.icode = MIPS64MMU_FetchIcode(GET_NIA);
	GET_NIA += 4;
	gcpu_mips64.next_icode = MIPS64MMU_FetchIcode(GET_NIA);
	while (1) {
		execute_instruction();
		gcpu_mips64.icode = gcpu_mips64.next_icode;
		GET_NIA += 4;
		gcpu_mips64.next_icode = MIPS64MMU_FetchIcode(GET_NIA);
	}

}
