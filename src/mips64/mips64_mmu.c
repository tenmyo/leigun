
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "mips64_mmu.h"
#include "sgstring.h"

uint64_t
Mips64MMU_Translate(uint64_t addr)
{
	return addr;	
}

/*
 * From PRA page 70 and following and from TX49/L3 Core Architecture
 * manual
 */
#define INDEX_P		(1<<31)

#define ENTRY_LO_WCE_MASK	(3<<30)
#define ENTRY_LO_WCE_SHIFT	(30)
#define ENTRY_LO_C_MASK		(7<<3)
#define ENTRY_LO_C_SHIFT	(3)
#define ENTRY_LO_D		(1<<2)
#define ENTRY_LO_V		(1<<1)
#define ENTRY_LO_G		(1<<0)


#define STATUS_CU3	(1<<31)
#define STATUS_CU2	(1<<30)
#define STATUS_CU1	(1<<29)
#define STATUS_CU0	(1<<28)
#define STATUS_RP	(1<<27)
#define STATUS_FR	(1<<26)
#define STATUS_RE	(1<<25)
#define STATUS_MX	(1<<24)
#define STATUS_PX	(1<<23)
#define STATUS_BEV	(1<<22)
#define STATUS_TS	(1<<21)
#define STATUS_SR	(1<<20)
#define STATUS_NMI	(1<<19)
#define STATUS_IM7	(1<<15)
#define STATUS_IM6	(1<<14)
#define STATUS_IM5	(1<<13)
#define STATUS_IM4	(1<<12)
#define STATUS_IM3	(1<<11)
#define STATUS_IM2	(1<<10)
#define STATUS_IPL_MASK		(0x3f<<10)
#define STATUS_IPL_SHIFT	(10)
#define STATUS_IM1	(1<<9)
#define STATUS_IM0	(1<<8)
#define STATUS_KX	(1<<7)
#define STATUS_SX	(1<<6)
#define STATUS_UX	(1<<5)
#define STATUS_KSU_MASK		(3<<3)
#define STATUS_KSU_SHIFT	(3)
#define STATUS_UM	(1<<4)
#define STATUS_R0	(1<<3)
#define STATUS_ERL	(1<<2)
#define STATUS_EXL	(1<<1)
#define STATUS_IE	(1<<0)

typedef void   MtcProc(void *clientData,uint32_t icode,uint64_t value);
typedef uint64_t MfcProc(void *clientData,uint32_t icode);

typedef struct MIPS_SysCopro {
	MtcProc *mtcproc[256];
	MfcProc *mfcproc[256];
	uint32_t r_Index; /* cp0 r=0, s=0 */
	uint32_t r_Random; /* cp0, r=1, s=0 */
	uint64_t r_EntryLo0;
	uint64_t r_EntryLo1;
	uint64_t r_Context;
	uint32_t r_PageMask;
	uint32_t r_PageGrain;
	uint32_t r_Wired;
	uint32_t r_HWREna;
	uint64_t r_BadVAddr; /* 32 or 64 bit */
	uint32_t r_Count;
	uint64_t r_EntryHi;
	uint32_t r_Compare;
	uint32_t r_Status;
	uint32_t r_IntCtl;
	uint32_t r_SRSCtl;
	uint32_t r_SRSMap;
	uint32_t r_Cause;
	uint64_t r_ExceptionPC;
	uint32_t r_ProcessorID;
	uint32_t r_EBase;
	uint32_t r_Configuration;
	uint32_t r_Configuration1;
	uint32_t r_Configuration2;
	uint32_t r_Configuration3;
	/* Reg16 s=6,7 reserved for implementations */
} MIPS_SysCopro;

uint32_t
CP0_MFC(void *clientData,uint32_t icode) 
{
	MIPS_SysCopro *cop0 = (MIPS_SysCopro *) clientData;
	int rd = (icode >> 11)	& 0x1f;
	int sel = icode & 0x7;
	int index = (rd << 3) || sel;
	MfcProc *proc = cop0->mfcproc[index];
	return proc(cop0,icode);
}

uint64_t
CP0_DMFC(void *clientData,uint32_t icode) 
{
	MIPS_SysCopro *cop0 = (MIPS_SysCopro *) clientData;
	int rd = (icode >> 11)	& 0x1f;
	int sel = icode & 0x7;
	int index = (rd << 3) || sel;
	MfcProc *proc = cop0->mfcproc[index];
	return proc(cop0,icode);
}


void
CP0_MTC(void *clientData,uint32_t icode,uint64_t value) 
{
	MIPS_SysCopro *cop0 = (MIPS_SysCopro *) clientData;
	int rd = (icode >> 11)	& 0x1f;
	int sel = icode & 0x7;
	int index = (rd << 3) || sel;
	MtcProc *proc = cop0->mtcproc[index];
	proc(cop0,icode,value);
}

MIPS_SysCopro *
MIPS_SysCoproNew() 
{
	MIPS_SysCopro *cop0 = sg_new(MIPS_SysCopro);
	return cop0;
}
