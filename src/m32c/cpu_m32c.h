/*
 * ----------------------------------------------------
 *
 * Definitions for the the M32C CPU
 *
 * (C) 2009 2010  Jochen Karrer 
 *   Author: Jochen Karrer
 *
 *
 *  This program is free software; you can distribute it and/or modify it
 *  under the terms of the GNU General Public License (Version 2) as
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope it will be useful, but WITHOUT
 *  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 *  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 *  for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  59 Temple Place - Suite 330, Boston MA 02111-1307, USA.
 *
 * ----------------------------------------------------
 */

#ifndef _CPU_M32C_H
#define _CPU_M32C_H
#include <stdint.h>
#include <setjmp.h>
#include "bus.h"
#include "mainloop_events.h"
#include "debugger.h"
#include "throttle.h"
#include "signode.h"
#include "idecode_m32c.h"

#define M32C_FLG_CARRY          (1<<0)
#define M32C_FLG_D		(1<<1) /* DEBUG */
#define M32C_FLG_ZERO           (1<<2)
#define M32C_FLG_SIGN           (1<<3)
#define M32C_FLG_BANK           (1<<4)
#define M32C_FLG_OVERFLOW       (1<<5)
#define M32C_FLG_I            	(1<<6) /* IEN */
#define M32C_FLG_U          	(1<<7) /* STKPS */
#define M32C_FLG_IPL_MSK	(0x7 << 12)
#define M32C_FLG_IPL_SHIFT	(12)

#define M32C_VECTOR_UND                         (0xFFFFDC)
#define M32C_VECTOR_OVERFLOW                    (0xFFFFE0)
#define M32C_VECTOR_BRK                         (0xFFFFE4)
#define M32C_VECTOR_ADDRESS_MATCH               (0xFFFFE8)
#define M32C_VECTOR_SINGLE_STEP                 (0xFFFFEC)
#define M32C_VECTOR_WATCHDOG                    (0xFFFFF0)
#define M32C_VECTOR_DBC                         (0xFFFFF4)
#define M32C_VECTOR_NMI                         (0xFFFFF8)
#define M32C_VECTOR_RESET                       (0xFFFFFC)

typedef enum M32C_DebugState {
        M32CDBG_RUNNING = 0,
        M32CDBG_STOP = 1,
        M32CDBG_STOPPED = 2,
        M32CDBG_STEP = 3,
        M32CDBG_BREAK = 4
} M32C_DebugState;

typedef struct M32C_RegBank {
        uint16_t  reg_r0,reg_r2;
        uint16_t  reg_r1,reg_r3;
        uint32_t  reg_a0,reg_a1;
        uint32_t  reg_fb;
        uint32_t  reg_sb;
} M32C_RegBank;

typedef struct M32C_Cpu {
	uint32_t icode;
	M32C_Instruction *instr;
	M32C_RegBank banks[2];
	/** 
 	  *********************************************************
          * This is a work copy of the current register set 
 	  * Must be synced before switching the regset.
 	  *********************************************************
	  */
	M32C_RegBank regs; 
        uint32_t  reg_usp;
        uint32_t  reg_isp;
	/*
	 **********************************************************
 	 * This is a work copy of the current stack pointer. 
	 * Must be synced before switching the mode.
	 **********************************************************
	 */
	uint32_t  reg_sp;
        uint16_t  reg_flg;
        uint32_t  reg_intb;
        uint32_t  reg_pc; 
	/* High speed interrupt register */
	uint16_t reg_svf;
	uint32_t reg_svp;
	uint32_t reg_vct;

	/* DMAC related registers */
	uint16_t reg_dmd0; /* Only lower 8 bits are used */
	uint16_t reg_dmd1; /* Only lower 8 bist are used */
	uint16_t reg_dct0;
	uint16_t reg_dct1;
	uint16_t reg_drc0;
	uint16_t reg_drc1;
	uint32_t reg_dma0;
	uint32_t reg_dma1;
	uint32_t reg_dsa0;
	uint32_t reg_dsa1;
	uint32_t reg_dra0;
	uint32_t reg_dra1;

	/* Registers for index storage */
	uint32_t index_dst; 
	uint32_t index_src; 
	int32_t bitindex; /* -1 = invalid */

	uint32_t signals;
	uint32_t signals_raw;
	uint32_t signals_mask;
	BusDevice *intco;
	int pending_ilvl;
	int pending_intno;

	Throttle *throttle;

	Debugger *debugger;
        DebugBackendOps dbgops;
        jmp_buf restart_idec_jump;
        M32C_DebugState dbg_state;
        int dbg_steps;
	/* IO registers */
	uint8_t regRLVL;
	uint8_t regPRCR;
	/* The Hva cache for read operations */
	uint8_t *readEntryHva;
	uint32_t readEntryPa;

	uint8_t *ifetchEntryHva;
	uint32_t ifetchEntryPa;

	uint8_t *writeEntryHva;
	uint32_t writeEntryPa;

	SigNode *sigPRC[4];
} M32C_Cpu;

M32C_Cpu gm32c;

#define M32C_REG(name) ((gm32c).reg_##name)
#define M32C_BANKREG(name) ((gm32c).regs.reg_##name)
/*
 * Use this only if regsets are syncronized
 */
#define M32C_REG_BANK0(name) ((gm32c).banks[0].reg_##name)
#define M32C_REG_BANK1(name) ((gm32c).banks[1].reg_##name)

#define M32C_REG_R0	(gm32c.regs.reg_r0)

/* Byte access depens on host byteorder */
#if __BYTE_ORDER == __BIG_ENDIAN
#define M32C_REG_R0H	(*((uint8_t *)(&gm32c.regs.reg_r0)))
#define M32C_REG_R0L	(*(((uint8_t *)(&gm32c.regs.reg_r0))+1))
#define M32C_REG_R1H	(*((uint8_t *)(&gm32c.regs.reg_r1)))
#define M32C_REG_R1L	(*(((uint8_t *)(&gm32c.regs.reg_r1))+1))
#else
#define M32C_REG_R0L	(*((uint8_t *)(&gm32c.regs.reg_r0)))
#define M32C_REG_R0H	(*(((uint8_t *)(&gm32c.regs.reg_r0))+1))
#define M32C_REG_R1L	(*((uint8_t *)(&gm32c.regs.reg_r1)))
#define M32C_REG_R1H	(*(((uint8_t *)(&gm32c.regs.reg_r1))+1))
#endif

#define M32C_REG_R1	(gm32c.regs.reg_r1)
#define M32C_REG_R2	(gm32c.regs.reg_r2)
#define M32C_REG_R3	(gm32c.regs.reg_r3)
#define M32C_REG_A0	(gm32c.regs.reg_a0)
#define M32C_REG_A1	(gm32c.regs.reg_a1)
#define M32C_REG_FB	(gm32c.regs.reg_fb)
#define M32C_REG_PC	(gm32c.reg_pc)
#define M32C_REG_INTB	(gm32c.reg_intb)
#define M32C_REG_SP	(gm32c.reg_sp)
#define M32C_SET_REG_ISP(value)	{ \
	if(M32C_REG_FLG & M32C_FLG_U) {  \
		gm32c.reg_isp = (value); \
	} else { \
		gm32c.reg_sp = (value); \
	} \
}
//#define M32C_REG_USP	(gm32c.reg_usp)
#define M32C_REG_SB	(gm32c.regs.reg_sb)
#define M32C_REG_FLG	(gm32c.reg_flg)
#define M32C_SET_REG_FLG(value)	(M32C_SetRegFlg(value))
#define ICODE	(gm32c.icode)
#define INSTR	(gm32c.instr)

static inline uint32_t 
ICODE24() {
	return gm32c.icode;
}

static inline uint32_t 
ICODE16() {
	return gm32c.icode >> 8;
}

static inline uint32_t 
ICODE8() {
	return gm32c.icode >> 16;
}

#define M32C_SIG_IRQ             (1)
#define M32C_SIG_INHIBIT_IRQ	 (2)
#define M32C_SIG_DELETE_INDEX	 (4)
#define M32C_SIG_DBG             (8)
#define M32C_SIG_RESTART_IDEC    (0x10)

static inline uint32_t 
M32C_INDEXBS(void) {
	//gm32c.index_src_used = 1;
	return gm32c.index_src;
}

static inline uint32_t 
M32C_INDEXWS(void) {
	//gm32c.index_src_used = 1;
	return gm32c.index_src;
}

static inline uint32_t 
M32C_INDEXLS(void) {
	//gm32c.index_src_used = 1;
	return gm32c.index_src;
}

static inline uint32_t 
M32C_INDEXBD(void) {
	//gm32c.index_dst_used = 1;
	return gm32c.index_dst;
}
static inline uint32_t 
M32C_INDEXWD(void) {
	//gm32c.index_dst_used = 1;
	return gm32c.index_dst;
}
static inline uint32_t 
M32C_INDEXLD(void) {
	//gm32c.index_dst_used = 1;
	return gm32c.index_dst;
}

static inline uint32_t 
M32C_INDEXSS(void) {	
	//gm32c.index_src_used = 1;
	return gm32c.index_src;
}

static inline uint32_t 
M32C_INDEXSD(void) {
	//gm32c.index_dst_used = 1;
	return gm32c.index_dst;
}

static inline int32_t 
M32C_BITINDEX(void) {
	return gm32c.bitindex;
}

void M32C_PostILevel(int ilvl,int int_no);
void M32C_AckIrq(BusDevice *intco,unsigned int intno);

static inline void
M32C_UpdateSignals(void) {
        gm32c.signals = gm32c.signals_raw & gm32c.signals_mask;
        if(gm32c.signals) {
                mainloop_event_pending = 1;
        }
}

void _M32C_SetRegFlg(uint16_t flg,uint16_t diff);

static inline void
M32C_SetRegFlg(uint16_t flg)
{
        uint16_t diff = flg ^ M32C_REG_FLG;
        M32C_REG_FLG = flg;
        if(!(diff & (M32C_FLG_U | M32C_FLG_BANK | M32C_FLG_I | M32C_FLG_IPL_MSK))) {
                return;
        } else {
		_M32C_SetRegFlg(flg,diff);
	}
}

void M32C_SyncRegSets(void);

static inline void
M32C_PostSignal(uint32_t signal) {
        gm32c.signals_raw |= signal;
        M32C_UpdateSignals();
}

static inline void
M32C_UnpostSignal(uint32_t signal) {
        gm32c.signals_raw &= ~signal;
        gm32c.signals = gm32c.signals_raw & gm32c.signals_mask;
}

uint32_t _M32C_Read32(uint32_t addr); 

static inline uint32_t
M32C_Read24(uint32_t addr) {
	uint8_t *hva;
        if(likely((addr & ~0x3ff) == gm32c.readEntryPa)) {
                hva = gm32c.readEntryHva + (addr & 0x3ff);
                return HMemRead32(hva) & 0x00ffffff;
        } else {
		return _M32C_Read32(addr) & 0x00ffffff;
        }
}

static inline uint32_t
M32C_IFetch(uint32_t addr) {
	uint8_t *hva;
	if(likely((addr & ~0x3ff) == gm32c.ifetchEntryPa)) {
		hva = gm32c.ifetchEntryHva + (addr & 0x3ff);
		return be32_to_host(HMemRead32(hva)) >> 8;
	} else {
		hva = Bus_GetHVARead(addr);
		if(unlikely(hva == NULL)) {
			return be32_to_host(IO_Read32(addr)) >> 8;
		}
		gm32c.ifetchEntryHva = hva - (addr & 0x3ff);
		gm32c.ifetchEntryPa = addr & 0xfffffc00;
		return be32_to_host(HMemRead32(hva)) >> 8;
	}
}


static inline uint32_t
M32C_Read32(uint32_t addr) {
	uint8_t *hva;
        if(likely((addr & ~0x3ff) == gm32c.readEntryPa)) {
                hva = gm32c.readEntryHva + (addr & 0x3ff);
                return HMemRead32(hva);
        } else {
		return _M32C_Read32(addr);
        }
}

uint16_t _M32C_Read16(uint32_t addr);

static inline uint16_t
M32C_Read16(uint32_t addr) 
{
	return Bus_Read16(addr);
	uint8_t *hva;
        if(likely((addr & ~0x3ff) == gm32c.readEntryPa)) {
                hva = gm32c.readEntryHva + (addr & 0x3ff);
                return HMemRead16(hva);
        } else {
		return _M32C_Read16(addr);
        }
}

uint8_t _M32C_Read8(uint32_t addr);

static inline uint8_t
M32C_Read8(uint32_t addr) 
{
	uint8_t *hva;
        if(likely((addr & ~0x3ff) == gm32c.readEntryPa)) {
                hva = gm32c.readEntryHva + (addr & 0x3ff);
                return HMemRead8(hva);
        } else {
		return _M32C_Read8(addr);
        }
}

void _M32C_Write8(uint8_t value,uint32_t addr);

static inline void
M32C_Write8(uint8_t value,uint32_t addr) 
{
	uint8_t *hva;
        if(likely((addr & ~0x3ff) == gm32c.writeEntryPa)) {
                hva = gm32c.writeEntryHva + (addr & 0x3ff);
                return HMemWrite8(value,hva);
        } else {
		return _M32C_Write8(value,addr);
        }
	return Bus_Write8(value,addr);
}

void _M32C_Write16(uint16_t value,uint32_t addr);

static inline void
M32C_Write16(uint16_t value,uint32_t addr)
{
	uint8_t *hva;
        if(likely((addr & ~0x3ff) == gm32c.writeEntryPa)) {
                hva = gm32c.writeEntryHva + (addr & 0x3ff);
                return HMemWrite16(value,hva);
        } else {
		return _M32C_Write16(value,addr);
        }
	return Bus_Write16(value,addr);
}

void _M32C_Write32(uint32_t value,uint32_t addr);

static inline void
M32C_Write32(uint32_t value,uint32_t addr)
{
	uint8_t *hva;
        if(likely((addr & ~0x3ff) == gm32c.writeEntryPa)) {
                hva = gm32c.writeEntryHva + (addr & 0x3ff);
                return HMemWrite32(value,hva);
        } else {
		return _M32C_Write32(value,addr);
        }
	return Bus_Write32(value,addr);
}

static inline void
M32C_RestartIdecoder(void) {
        longjmp(gm32c.restart_idec_jump,1);
}

void M32C_InvalidateHvaCache(void);

#include "cycletimer.h"

void M32C_Break(void); 

M32C_Cpu * M32C_CpuNew(const char *instancename,BusDevice *intco);
void M32C_Run(void);
#endif
