/*
 * ----------------------------------------------------
 *
 * Definitions for the the M16C CPU
 *
 * (C) 2005 2010  Jochen Karrer 
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

#ifndef _M16C_CPU_H
#define _M16C_CPU_H
#include <stdint.h>
#include "bus.h"
#include "throttle.h"
#include "mainloop_events.h"

#define M16C_FLG_CARRY          (1<<0)
#define M16C_FLG_D		(1<<1) /* DEBUG */
#define M16C_FLG_ZERO           (1<<2)
#define M16C_FLG_SIGN           (1<<3)
#define M16C_FLG_BANK           (1<<4)
#define M16C_FLG_OVERFLOW       (1<<5)
#define M16C_FLG_I            	(1<<6) /* IEN */
#define M16C_FLG_U          	(1<<7) /* STKPS */
#define M16C_FLG_IPL_SHIFT      (12)
#define M16C_FLG_IPL_MSK	(0x7000)

#if 0
/* Moved to CPU variant handling */
#define M16C_VECTOR_UND                         (0xFFFDC)
#define M16C_VECTOR_OVERFLOW                    (0xFFFE0)
#define M16C_VECTOR_BRK                         (0xFFFE4)
#define M16C_VECTOR_ADDRESS_MATCH               (0xFFFE8)
#define M16C_VECTOR_SINGLE_STEP                 (0xFFFEC)
#define M16C_VECTOR_WATCHDOG                    (0xFFFF0)
#define M16C_VECTOR_DBC                         (0xFFFF4)
#define M16C_VECTOR_NMI                         (0xFFFF8)
#define M16C_VECTOR_RESET                       (0xFFFFC)
#endif

#define M16C_SIG_IRQ             (1)
#define M16C_SIG_DBG             (8)
#define M16C_SIG_RESTART_IDEC    (0x10)

typedef struct M16C_RegBank {
        uint16_t  r0,r2;
        uint16_t  r1,r3;
        uint16_t  a0,a1;
        uint16_t  fb;
        uint32_t  pc :20; // :20;
        uint32_t  intb :20; //:20;
        uint16_t  usp;
        uint16_t  isp;
	uint16_t  sp; // copy of current working
        uint16_t  sb;
        uint16_t  flg;
} M16C_RegBank;

typedef void M16C_AckIrqProc(BusDevice *intco,uint32_t pending_intno);

typedef struct M16C_Cpu {

	M16C_RegBank bank[2];
	M16C_RegBank regs;
	uint16_t icode;

	uint32_t signals;
        uint32_t signals_raw;
        uint32_t signals_mask;
	Throttle *throttle;

	int pending_ilvl;
	int pending_intno;
	M16C_AckIrqProc *ackIrq;
	BusDevice *intco;
	/** The fixed vectors */
	uint32_t vector_base;
	uint32_t vector_reset;
} M16C_Cpu;

M16C_Cpu gm16c;

#if __BYTE_ORDER == __BIG_ENDIAN
#define M16C_REG_R0H    (*((uint8_t *)(&gm16c.regs.r0)))
#define M16C_REG_R0L    (*(((uint8_t *)(&gm16c.regs.r0))+1))
#define M16C_REG_R1H    (*((uint8_t *)(&gm16c.regs.r1)))
#define M16C_REG_R1L    (*(((uint8_t *)(&gm16c.regs.r1))+1))
#else
#define M16C_REG_R0L    (*((uint8_t *)(&gm16c.regs.r0)))
#define M16C_REG_R0H    (*(((uint8_t *)(&gm16c.regs.r0))+1))
#define M16C_REG_R1L    (*((uint8_t *)(&gm16c.regs.r1)))
#define M16C_REG_R1H    (*(((uint8_t *)(&gm16c.regs.r1))+1))
#endif

#define M16C_REG_R0	(gm16c.regs.r0)
#define M16C_REG_R1	(gm16c.regs.r1)
#define M16C_REG_R2	(gm16c.regs.r2)
#define M16C_REG_R3	(gm16c.regs.r3)
#define M16C_REG_A0	(gm16c.regs.a0)
#define M16C_REG_A1	(gm16c.regs.a1)
#define M16C_REG_FB	(gm16c.regs.fb)
#define M16C_REG_PC	(gm16c.regs.pc)
#define M16C_REG_INTB	(gm16c.regs.intb)
#define M16C_REG_SP	(gm16c.regs.sp)
#define M16C_REG_ISP	(gm16c.regs.isp)
#define M16C_REG_USP	(gm16c.regs.usp)
#define M16C_SET_REG_ISP(value) { \
        if(M16C_REG_FLG & M16C_FLG_U) {  \
                gm16c.regs.isp = (value); \
        } else { \
                gm16c.regs.sp = (value); \
        } \
}

#define M16C_SET_REG_USP(value) { \
        if(M16C_REG_FLG & M16C_FLG_U) {  \
                gm16c.regs.sp = (value); \
        } else { \
                gm16c.regs.usp = (value); \
        } \
}

#define M16C_REG_SB	(gm16c.regs.sb)
#define M16C_REG_FLG	(gm16c.regs.flg)
#define M16C_SET_REG_FLG(value) M16C_SetRegFlg(value);	
#define ICODE()		(gm16c.icode)


static inline uint32_t 
ICODE16(void) {
	return gm16c.icode;
}

static inline uint32_t 
ICODE8(void) {
	return gm16c.icode >> 8;
}

static inline uint8_t
M16C_Read8(uint32_t addr) 
{
	return Bus_Read8(addr);	
}
static inline uint16_t
M16C_Read16(uint32_t addr) 
{
	return Bus_Read16(addr);	
}

static inline uint32_t
M16C_Read24(uint32_t addr) 
{
        uint32_t data = Bus_Read8(addr);
        data |= Bus_Read8(addr + 1) << 8;
        data |= Bus_Read8(addr + 2) << 16;
        return data;
}

static inline uint16_t
M16C_IFetch(uint32_t addr) 
{
        uint16_t data = Bus_Read8(addr) << 8;;
        data |= Bus_Read8(addr + 1);
        return data;
}

static inline void
M16C_Write8(uint8_t value,uint32_t addr) 
{
	return Bus_Write8(value,addr);
}

static inline void
M16C_Write16(uint16_t value,uint32_t addr)
{
	return Bus_Write16(value,addr);
}

static inline void
M16C_Write24(uint32_t value,uint32_t addr) 
{
	Bus_Write8(value,addr);
	Bus_Write8((value >> 8),addr + 1);
	Bus_Write8((value >> 16),addr + 2);
}

static inline void
M16C_UpdateSignals(void) {
        gm16c.signals = gm16c.signals_raw & gm16c.signals_mask;
        if(gm16c.signals) {
                mainloop_event_pending = 1;
        }
}

void M16C_SetRegFlg(uint16_t flg);
void M16C_SyncRegSets(void);

static inline void
M16C_PostSignal(uint32_t signal) {
        gm16c.signals_raw |= signal;
        M16C_UpdateSignals();
}

static inline void
M16C_UnpostSignal(uint32_t signal) {
        gm16c.signals_raw &= ~signal;
        gm16c.signals = gm16c.signals_raw & gm16c.signals_mask;
}

void M16C_PostILevel(int ilvl,int int_no);

M16C_Cpu * M16C_CpuNew(const char *instancename,M16C_AckIrqProc *,BusDevice *intco);
void M16C_Run(void);
#endif
