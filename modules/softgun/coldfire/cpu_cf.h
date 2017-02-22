#include <stdint.h>
#include "coldfire/mem_cf.h"

#define CR_REG_CACR	(2)
#define CR_REG_ASID	(3)
#define CR_REG_ACR0	(4)
#define CR_REG_ACR1	(5)
#define CR_REG_ACR2	(6)
#define CR_REG_ACR3	(7)
#define CR_REG_MMUBAR	(8)
#define CR_REG_VBR	(0x801)
#define CR_REG_PC	(0x80f)
//#define CR_REG_ROMBAR0        (0xc00)
//#define CR_REG_ROMBAR1        (0xc01)
#define CR_REG_FLASHBAR	(0xc04)
#define CR_REG_RAMBAR	(0xc05)
#define CR_REG_MPCR	(0xc0c)
#define CR_REG_EDRAMBAR (0xc0d)
#define CR_REG_SECMBAR	(0xc0e)
#define CR_REG_MBAR	(0xc0f)
#define CR_REG_PCR1U0	(0xd02)
#define CR_REG_PCR1L0	(0xd03)
#define CR_REG_PCR2U0	(0xd04)
#define CR_REG_PCR2L0	(0xd05)
#define CR_REG_PCR3U0	(0xd06)
#define CR_REG_PCR3L0	(0xd07)
#define CR_REG_PCR1U1	(0xd0a)
#define CR_REG_PCR1L1	(0xd0b)
#define CR_REG_PCR2U1	(0xd0c)
#define CR_REG_PCR2L1	(0xd0d)
#define CR_REG_PCR3U1	(0xd0e)
#define CR_REG_PCR3L1	(0xd0f)

#define EX_ACCESS_ERROR			(2)
#define EX_ADDRESS_ERROR		(3)
#define EX_ILLEGAL_INSTR		(4)
#define EX_DIVIDE_BY_ZERO		(5)
#define EX_PRIVILEGE_VIOLATION		(8)
#define EX_TRACE			(9)
#define EX_UNIMPL_LINE_A		(10)
#define EX_UNIMPL_LINE_F		(11)
#define EX_NON_PC_BRKPT_DEBUG		(12)
#define EX_PC_BRKPT_DEBUG		(13)
#define EX_FORMAT_ERROR			(14)
#define EX_UNITIALIZED_INT		(15)
#define EX_SPURIOUS_INT			(24)
#define EX_LEVEL_AUTO_INT(lev)		(25 + ((lev)-1))
#define EX_TRAP(x)			(32+(x))
#define EX_FP_BRANCH_ON_UNORDERED	(48)
#define EX_FP_INEXACT			(49)
#define EX_FP_DIVIDE_BY_ZERO		(50)
#define EX_FP_UNDERFLOW			(51)
#define EX_FP_OPERAND_ERROR		(52)
#define EX_FP_OVERFLOW			(53)
#define EX_FP_NAN			(54)
#define EX_FP_DENORMALIZED_NUM		(55)
#define EX_UNSUPP_INSTR			(61)
#define EX_USER_DEF_INT(x)		(64+(x))

typedef struct {
	uint32_t reg_GP[16];
	uint32_t *reg_D;
	uint32_t *reg_A;
	uint32_t reg_PC;
	uint16_t reg_CCR;
	uint16_t icode;		/* currently executed instruction */
	/* MAC registers */
	uint32_t reg_macSR;
	uint32_t reg_macACC;
	uint32_t reg_macMASK;

	/* EMAC registers */
	uint32_t reg_emacACC[4];
	uint32_t reg_emacACCext01;
	uint32_t reg_emacAccext23;
	uint32_t reg_emacMASK;
	/* Supervisor Registers */
	uint16_t reg_SR;
	uint32_t reg_OTHER_A7;
	uint32_t reg_VBR;
	uint32_t reg_CACR;
	uint32_t reg_ASID;	// not in v2
	uint32_t reg_ACR[4];	// only 2 in v2
	uint32_t reg_MMUBAR;	// not in v2
	uint32_t reg_FLASHBAR;	// device specific
	uint32_t reg_RAMBAR;	// device specific
	uint32_t reg_MBAR;	// device specific
} CFCpu;

/* CCR Register Bitfield */
#define CCR_C	(1<<0)
#define CCR_V	(1<<1)
#define CCR_Z	(1<<2)
#define CCR_N	(1<<3)
#define CCR_X	(1<<4)
#define CCR_P	(1<<7)
#define CCRS_T	(1<<15)		/* Trace Bit        */
#define CCRS_S	(1<<13)		/* Supervisor State */
#define CCRS_M	(1<<12)		/* Master Interrupt State */
#define CCRS_I	(7<<8)		/* Interrupt Priority Mask */

/* floating point control register */
#define FPCR_BSUN	(1<<15)
#define FPCR_INAN	(1<<14)
#define FPCR_OPERR	(1<<13)
#define FPCR_OVFL	(1<<12)
#define FPCR_UNFL	(1<<11)
#define FPCR_DZ		(1<<10)
#define FPCR_INEX	(1<<9)
#define FPCR_IDE	(1<<8)
#define FPCR_PREC	(1<<6)
#define FPCR_RND_SHIFT	(4)
#define FPCR_RND_MASK	(3<<4)

/* FPSR Floating Point Status Register */
#define FPSR_N		(1<<27)
#define FPSR_Z		(1<<26)
#define FPSR_I		(1<<25)
#define FPSR_NAN	(1<<24)
#define FPSR_EXC_BSUN	(1<<15)
#define FPSR_EXC_INAN	(1<<14)
#define FPSR_EXC_OPERR	(1<<13)
#define FPSR_EXC_OVFL	(1<<12)
#define FPSR_EXC_UNFL	(1<<11)
#define FPSR_EXC_DZ	(1<<10)
#define FPSR_EXC_INEX	(1<<9)
#define FPSR_EXC_IDE	(1<<8)
#define FPSR_AEXC_IOP	(1<<7)
#define FPSR_AEXC_OVFL	(1<<6)
#define FPSR_AEXC_UNFL	(1<<5)
#define FPSR_AEXC_DZ	(1<<4)
#define FPSR_AEXC_INEX	(1<<3)

#define MACSR_OMC 	(1<<7)
#define MACSR_SU	(1<<6)
#define MACSR_FI	(1<<5)
#define MACSR_RT	(1<<4)
#define MACSR_N		(1<<3)
#define MACSR_Z		(1<<2)
#define MACSR_V		(1<<1)
#define MACSR_C		(1<<0)

#define EMACSR_PAV3	(1<<11)
#define EMACSR_PAV2	(1<<10)
#define EMACSR_PAV1	(1<<9)
#define EMACSR_PAV0	(1<<8)
#define EMACSR_OMC	(1<<7)
#define EMACSR_SU	(1<<6)
#define EMACSR_FI	(1<<5)
#define EMACSR_RT	(1<<4)
#define EMACSR_N	(1<<3)
#define EMACSR_Z	(1<<2)
#define EMACSR_V	(1<<1)
#define EMACSR_EV	(1<<0)

#define MBAR_BA_MASK 	0xfffff000
#define MBAR_BA_SHIFT	(12)
#define MBAR_WP		(1<<8)
#define MBAR_AM		(1<<6)
#define MBAR_CI		(1<<5)
#define MBAR_SC		(1<<4)
#define MBAR_SD		(1<<3)
#define MBAR_UC		(1<<2)
#define MBAR_UD		(1<<1)
#define MBAR_V		(1<<0)

/* Initial State of D0/D1 Registers */
#define HWCONFIG_D0_MFC5282	(0xcf206080)
#define HWCONFIG_D1_MFC5282	(0x13b01080)

extern CFCpu g_CFCpu;

#define CF_REG_CCR (g_CFCpu.reg_CCR)
#define CF_REG_MACSR (g_CFCpu.reg_macSR)

#define ICODE g_CFCpu.icode

static inline int
CF_IsSupervisor(void)
{
	return !!(g_CFCpu.reg_CCR & CCRS_S);
}

static inline uint32_t
CF_GetRegOtherA7(void)
{
	return g_CFCpu.reg_OTHER_A7;
}

static inline void
CF_SetRegOtherA7(uint32_t value)
{
	g_CFCpu.reg_OTHER_A7 = value;
}

static inline void
CF_SetRegSR(uint16_t value)
{
	uint32_t diff = g_CFCpu.reg_CCR ^ value;
	if (diff & CCRS_S) {
		uint32_t tmp = g_CFCpu.reg_OTHER_A7;
		g_CFCpu.reg_OTHER_A7 = g_CFCpu.reg_A[7];
		g_CFCpu.reg_A[7] = tmp;
	}
	g_CFCpu.reg_CCR = value;
}

static inline uint32_t
CF_GetReg(int reg)
{
	return g_CFCpu.reg_GP[reg];
}

static inline uint32_t
CF_GetRegA(int reg)
{
	return g_CFCpu.reg_A[reg];
}

static inline uint32_t
CF_GetRegD(int reg)
{
	return g_CFCpu.reg_D[reg];
}

static inline void
CF_SetReg(uint32_t value, int reg)
{
	g_CFCpu.reg_GP[reg] = value;
}

static inline void
CF_SetRegA(uint32_t value, int reg)
{
	g_CFCpu.reg_A[reg] = value;
}

static inline void
CF_SetRegD(uint32_t value, int reg)
{
	g_CFCpu.reg_D[reg] = value;
}

static inline void
CF_SetRegPC(uint32_t value)
{
	g_CFCpu.reg_PC = value;
}

static inline uint32_t
CF_GetRegPC(void)
{
	return g_CFCpu.reg_PC;
}

static inline void
CF_SetRegMacAcc(uint32_t value)
{
	g_CFCpu.reg_macACC = value;
}

static inline uint32_t
CF_GetRegMacAcc(void)
{
	return g_CFCpu.reg_macACC;
}

static inline void
CF_SetRegMacSr(uint32_t value)
{
	g_CFCpu.reg_macSR = value;
}

static inline uint32_t
CF_GetRegMacSr(void)
{
	return g_CFCpu.reg_macSR;
}

static inline void
CF_SetRegMacMask(uint32_t value)
{
	g_CFCpu.reg_macMASK = value;
}

static inline uint32_t
CF_GetRegMacMask(void)
{
	return g_CFCpu.reg_macMASK;
}

void CF_SetRegCR(uint32_t value, int reg);
uint32_t CF_GetRegCR(int reg);

static inline void
Push4(uint32_t val)
{
	uint32_t sp = CF_GetRegA(7);
	sp -= 4;
	CF_MemWrite32(val, sp);
	CF_SetRegA(sp, 7);
}

static inline uint32_t
Pop4(void)
{
	uint32_t val;
	uint32_t sp = CF_GetRegA(7);
	val = CF_MemRead32(sp);
	sp += 4;
	CF_SetRegA(sp, 7);
	return val;
}
