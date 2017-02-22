/*
 * ----------------------------------------------------
 *
 * Definitions for the CP15 Memory Management Unit
 * (C) 2004  Lightmaze Solutions AG
 *   Author: Jochen Karrer
 *
 * ----------------------------------------------------
 */

#include <bus.h>
#include "arm9cpu.h"
#include <cycletimer.h>
#include <sys/time.h>
#include <time.h>
#include "signode.h"
#include "mmu_arm.h"

#define SYSCPR_ID 	(0)
#define SYSCPR_CTRL 	(1)
#define SYSCPR_MTBASE 	(2)
#define SYSCPR_MDAC 	(3)
#define SYSCPR_MFSTAT 	(5)
#define SYSCPR_MFADDR	(6)
#define SYSCPR_MCCTRL	(7)
#define SYSCPR_MTLBCTRL	(8)
#define SYSCPR_MCLCK	(9)
#define SYSCPR_MTLBLCK	(10)
#define SYSCPR_MPID	(13)

#define MCTRL_MMUEN	(1<<0)
#define MCTRL_ALGNCHK	(1<<1)
#define MCTRL_CACHEEN	(1<<2)
#define MCTRL_WBEN	(1<<3)
#define MCTRL_P32	(1<<4)
#define MCTRL_D32	(1<<5)
#define MCTRL_LABT	(1<<6)
#define MCTRL_BE	(1<<7)
#define MCTRL_SPB	(1<<8)
#define MCTRL_RPR	(1<<9)
#define MCTRL_F		(1<<10)
#define MCTRL_ZBRANCH	(1<<11)
#define MCTRL_ICACHEEN	(1<<12)
#define MCTRL_V		(1<<13)
#define MCTRL_RR	(1<<14)
#define MCTRL_L4	(1<<15)

/*
 * ------------------------------------------------
 * Access types required for priviledge calculation
 * ------------------------------------------------
 */
#define MMU_ACCESS_IFETCH (0x1)
#define MMU_ACCESS_DATA_READ  (0)
#define MMU_ACCESS_DATA_WRITE (32)

/*
 * ---------------------------------------------------------
 * Translation Lookaside buffer
 *      caches Physical address for IO
 *      accesses and Host Virtual Address for faster
 *      memory access
 * ---------------------------------------------------------
 */

extern uint32_t mmu_enabled;

#define TLBE_IS_HVA(tlbe) ((tlbe).hva!=NULL)
#define TLBE_IS_PA(tlbe) ((tlbe).hva==NULL)

#define TLB_MATCH(tlbe,addr) ((((addr)&0xfffffc00)==(tlbe).va) && ((tlbe).cpu_mode==ARM_SIGNALING_MODE))
#define TLB_MATCH_HVA(tlbe,addr) ((((addr)&0xfffffc00)==(tlbe).va) && ((tlbe).cpu_mode==ARM_SIGNALING_MODE) && TLBE_IS_HVA(tlbe))

#define MMU_ARM926EJS	(0xa0310000)
#define MMU_ARM920T	(0xa0320000)
#define MMUV_NS9750	(0x2)
#define MMUV_IMX21	(0x3)

ArmCoprocessor *MMU9_Create(const char *name, int endian, uint32_t type);
uint32_t MMU9_TranslateAddress(uint32_t addr, uint32_t access_type);

void MMU_AlignmentException(uint32_t far);
void MMU_InvalidateTlb();
void MMU_SetDebugMode(int val);
int MMU_Byteorder();
