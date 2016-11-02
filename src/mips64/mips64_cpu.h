#ifndef MIPS64_CPU_H
#define MIPS64_CPU_H
#include <stdint.h>
#include <setjmp.h>

#define ISNEG32(x) ((x)&(1<<31))
#define ISNOTNEG32(x) (!((x)&(1<<31)))
#define ISPOS32(x) (-(x) & (1<<31)

#define ISNEG64(x) ((x)&(1<<31))
#define ISNOTNEG64(x) (!((x)&(1<<31)))
#define ISPOS64(x) (-(x) & (1<<31)

typedef struct MIPS64Cpu {
	uint64_t gpr[32];
	uint64_t hi;
	uint64_t lo;
	uint64_t pc;		/* location of currently interpreted instruction, Not result from PC read */
	uint32_t icode;
	uint32_t next_icode;
	int LLBit;		/* Atomic instruction flag */
	jmp_buf restart_pipeline;
} MIPS64Cpu;

extern MIPS64Cpu gcpu_mips64;

#define ICODE (gcpu_mips64.icode)
#define MIPS64_ReadGpr(i) (gcpu_mips64.gpr[(i)])

#define PC_OFFSET (4)
/*
 * Get Current/Next Instruction Address
 */
#define GET_CIA (gcpu_mips64.pc-PC_OFFSET)
#define GET_NIA (gcpu_mips64.pc)

/*
 * The instruction manual uses PC in the sense of the Current
 * instruction address.
 */
static inline uint64_t
MIPS64_GetRegPC()
{
	return GET_CIA;
}

static inline void __attribute__ ((noreturn))
    MIPS64_SetRegPC(uint64_t value)
{
	gcpu_mips64.pc = value;
	longjmp(gcpu_mips64.restart_pipeline, 1);
}

static inline void
MIPS64_WriteGpr(int i, uint64_t value)
{
	if (i) {
		(gcpu_mips64.gpr[(i)]) = value;
	}
}

/*
 * -----------------------------------------
 * Access to HI and LO special registers
 * -----------------------------------------
 */
static inline uint64_t
MIPS64_GetLo()
{
	return gcpu_mips64.lo;
}

static inline uint64_t
MIPS64_GetHi()
{
	return gcpu_mips64.hi;
}

static inline void
MIPS64_SetLo(uint64_t lo)
{
	gcpu_mips64.lo = lo;
}

static inline void
MIPS64_SetHi(uint64_t hi)
{
	gcpu_mips64.hi = hi;
}

void MIPS64_ExecuteDelaySlot();

static inline void
MIPS64_NullifyDelaySlot(void)
{
	gcpu_mips64.next_icode = 0;
}

static inline int
MIPS64_GetLLBit(void)
{
	return gcpu_mips64.LLBit;
}

static inline void
MIPS64_SetLLBit(int value)
{
	gcpu_mips64.LLBit = value;
}
#endif
