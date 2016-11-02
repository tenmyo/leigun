/*
 *************************************************************************************************
 *
 * SH4 Instruction Set and Addressing Modes
 *
 * State: Starting u-boot, Exceptions and mmu not working
 *
 * Used Renesas SH-4 Software Manual REJ09B0318-0600
 *
 * Copyright 2006 Jochen Karrer. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification, are
 * permitted provided that the following conditions are met:
 *
 *   1. Redistributions of source code must retain the above copyright notice, this list of
 *       conditions and the following disclaimer.
 *
 *   2. Redistributions in binary form must reproduce the above copyright notice, this list
 *       of conditions and the following disclaimer in the documentation and/or other materials
 *       provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY Jochen Karrer ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * The views and conclusions contained in the software and documentation are those of the
 * authors and should not be interpreted as representing official policies, either expressed
 * or implied, of Jochen Karrer.
 *
 *************************************************************************************************
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <cpu_sh4.h>
#include <mmu_sh4.h>
#include <cycletimer.h>
#include <softfloat.h>

#define ISNEG(x) ((x)&(1<<31))
#define ISNOTNEG(x) (!((x)&(1<<31)))

static inline uint32_t
add_carry(uint32_t op1,uint32_t op2,uint32_t result) {

	if(((op1 + op2) < op1) || ((op1 + op2) > result)) {
                return SR_T;
        } else {
                return 0;
        }
}

static inline uint32_t
add_overflow(uint32_t op1,uint32_t op2,uint32_t result) {
        return (((op1 & op2 & ~result) | (~op1 & ~op2 & result)) >> 31);
}


static inline uint32_t 
sub_carry(uint32_t Rn,uint32_t Rm, uint32_t R)
{
	if((Rn < (Rn - Rm)) || ((Rn - Rm) < R)) {
		return SR_T;
	} else {
		return 0;
	}	
}

static inline uint32_t
sub_overflow(uint32_t op1,uint32_t op2,uint32_t result) {
        return (((op1 & ~op2 & ~result) | (~op1 & op2 & result)) >> 31);
}


/**
 ********************************************************
 * \fn sh4_add(void)
 * Add two Registers.
 * v1
 ********************************************************
 */
void
sh4_add(void)
{
	int rn = (ICODE >> 8) & 0xf;
	int rm = (ICODE >> 4) & 0xf;
	uint32_t Rm,Rn;
	Rn = SH4_GetGpr(rn);	
	Rm = SH4_GetGpr(rm);	
	SH4_SetGpr(Rn + Rm,rn);
}

/**
 ***********************************************************
 * \fn sh4_add_imm(void)
 * Add a sign extended 8 Bit immediate to the 
 * Rn register.
 * v1 
 ***********************************************************
 */
void
sh4_add_imm(void)
{
	int rn = (ICODE >> 8) & 0xf;
	int32_t Rn;
	int8_t imm = ICODE & 0xff;
	Rn = SH4_GetGpr(rn);	
	SH4_SetGpr(Rn + imm,rn);
}

/**
 *********************************************************
 * \fn sh4_addc(void)
 * Add two registers with carry.
 *********************************************************
 */

void
sh4_addc(void)
{
	int rn = (ICODE >> 8) & 0xf;
	int rm = (ICODE >> 4) & 0xf;
	uint32_t Rn,Rm;
	Rn = SH4_GetGpr(rn);	
	Rm = SH4_GetGpr(rm);	
	uint32_t result;
	if(SH4_GetTrue()) {
		result = Rm + Rn + 1;
	} else {
		result = Rm + Rn;
	}
	SH4_SetGpr(result,rn);	
	if(add_carry(Rn,Rm,result)) {
		SH4_SetTrue(1);
	} else {
		SH4_SetTrue(0);
	}
}

/**
 *************************************************
 * \fn sh4_addv(void)
 * Add with overflow check
 * v1
 *************************************************
 */
void
sh4_addv(void)
{
	int rn = (ICODE >> 8) & 0xf;
	int rm = (ICODE >> 4) & 0xf;
	uint32_t Rn,Rm;
	Rn = SH4_GetGpr(rn);	
	Rm = SH4_GetGpr(rm);	
	uint32_t result;
	result = Rm + Rn;
	SH4_SetGpr(result,rn);
	if(add_overflow(Rn,Rm,result)) {
		SH4_SetTrue(1);
	} else {
		SH4_SetTrue(0);
	}
}

/**
 ***************************************************
 * \fn void sh_and(void)
 * Logical and of two registers.
 * v1
 ***************************************************
 */
void
sh4_and(void)
{
	int rn = (ICODE >> 8) & 0xf;
	int rm = (ICODE >> 4) & 0xf;
	uint32_t Rn,Rm;
	Rn = SH4_GetGpr(rn);	
	Rm = SH4_GetGpr(rm);	
	Rn = Rn & Rm;
	SH4_SetGpr(Rn,rn);
}

/**
 *******************************************************
 * \fn void sh4_and_imm(void)
 *  Logical and of register R0 with an immediate
 *******************************************************
 */
void
sh4_and_imm(void)
{
	uint32_t R0;
	uint32_t imm = (ICODE & 0xff);	
	R0 = SH4_GetGpr(0);
	R0 = R0 & imm;	
	SH4_SetGpr(R0,0);
}

/**
 ******************************************
 * \fn void sh4_andb(void)
 * And of (Gbr + R0) with and immediate
 * v1
 ******************************************
 */
void
sh4_andb(void)
{
	uint32_t temp;
	uint32_t imm = (ICODE & 0xff);	
	uint32_t addr = SH4_GetGBR() + SH4_GetGpr(0);
	temp = SH4_MMURead8(addr);
	temp = temp & imm;
	SH4_MMUWrite8(temp,addr);
}

/*
 ********************************************************
 * \fn void sh4_bf(void)
 * Branch if false. Does not execute a branch delay slot. 
 * v1
 ********************************************************
 */
void
sh4_bf(void)
{
	int32_t disp = (int32_t)(int8_t)(ICODE & 0xff);
	int T = SH4_GetTrue();	
	if(T == 0) {
		SH4_SetRegPC(SH4_NNIA + (disp << 1)); 
	}
}

/**
 ***********************************************************
 * \fn void sh4_bfs(void)
 * Branch if false with delay Slot.
 * v1 
 ***********************************************************
 */
void
sh4_bfs(void)
{
	int32_t disp = (int32_t)(int8_t)(ICODE & 0xff);
	int T = SH4_GetTrue();	
	if(T == 0) {
		SH4_ExecuteDelaySlot();
		SH4_SetRegPC(SH4_NNIA + (disp << 1)); 
	}
	
}

/**
 ********************************************************
 * \fn void sh4_bra(void)
 * Unconditional branch with delay slot execution.  
 * v1
 ********************************************************
 */
void
sh4_bra(void)
{
	int32_t disp;
	disp = ((int32_t)(ICODE << 20)) >> 19;
	SH4_ExecuteDelaySlot();
	SH4_SetRegPC(SH4_NNIA + disp); 
}

/**
 *********************************************************
 * \fn void sh4_braf(void)
 * Branch Far adds a register value to the pc + 4
 * ST40 Manual says pc = target & ~1.
 * v1
 *********************************************************
 */
void
sh4_braf(void)
{
	int rn = (ICODE >> 8) & 0xf;
	uint32_t Rn = SH4_GetGpr(rn);
	SH4_ExecuteDelaySlot();
	SH4_SetRegPC((SH4_NNIA + Rn) & ~UINT32_C(1)); 
}

/**
 *****************************************************************************
 * \fn void sh4_brk(void)
 * Break instruction only mentioned in ST40 manual. Not in Renesas.
 *****************************************************************************
 */
void
sh4_brk(void)
{
	fprintf(stderr,"sh4_brk not implemented\n");
	SH4_Break();
}

/**
 *********************************************************
 * \fn void sh4_bsr(void)
 * Branch to subroutine with delay slot execution
 * v1
 *********************************************************
 */
void
sh4_bsr(void)
{
	int32_t disp = ((int32_t)(ICODE << 20)) >> 19;
	SH4_SetPR(SH4_NNIA);
	SH4_ExecuteDelaySlot();
	SH4_SetRegPC(SH4_NNIA + disp);
}

/**
 *********************************************************
 * \fn void sh4_bsrf(void)
 * Branch to subroutine far with delay slot execution.
 * ST40 manual says pc = target & ~1;
 * v1
 *********************************************************
 */
void
sh4_bsrf(void)
{
	int rn = (ICODE >> 8) & 0xf;
	uint32_t Rn = SH4_GetGpr(rn);
	SH4_SetPR(SH4_NNIA);
	SH4_ExecuteDelaySlot();
	SH4_SetRegPC((SH4_NNIA + Rn) & ~UINT32_C(1)); 
}

/**
 ************************************************************ 
 * \fn void sh4_bt(void)
 * Branch if true.
 ************************************************************ 
 */
void
sh4_bt(void)
{
	int32_t disp = (int32_t)(int8_t)(ICODE & 0xff);
	int T = SH4_GetTrue();	
	if(T != 0) {
		SH4_SetRegPC(SH4_NNIA + (disp << 1)); 
	}
}

/**
 **************************************************************
 * \fn void sh4_bts(void)
 * Branch if true with delay slot.
 * v1
 **************************************************************
 */
void
sh4_bts(void)
{
	int32_t disp = (int32_t)(int8_t)(ICODE & 0xff);
	int T = SH4_GetTrue();	
	if(T != 0) {
		SH4_ExecuteDelaySlot();
		SH4_SetRegPC(SH4_NNIA + (disp << 1)); /* does not return */
	}
}

/**
 ******************************************************
 * \fn void sh4_clrmac(void)
 * Clear MAC register.
 * v1;
 ******************************************************
 */
void
sh4_clrmac(void)
{
	SH4_SetMacH(0);
	SH4_SetMacL(0);
}

/**
 *****************************************************
 * \fn void sh4_clrs(void)
 * Clear S-Bit.
 * v1
 *****************************************************
 */
void
sh4_clrs(void)
{
	SH4_SetS(0);
}

/**
 *************************************************
 * \fn void sh4_clrt(void)
 * Clear T-Bit.
 * v1
 *************************************************
 */
void
sh4_clrt(void)
{
	SH4_SetTrue(0);
}

/**
 *************************************************
 * \fn void sh4_cmpeq(void)
 * Compare Equal. Set T Bit if Rn == Rm. 
 * v1
 *************************************************
 */
void
sh4_cmpeq(void)
{
	int rn = (ICODE >> 8) & 0xf;
	int rm = (ICODE >> 4) & 0xf;
	uint32_t Rn = SH4_GetGpr(rn);
	uint32_t Rm = SH4_GetGpr(rm);
	if(Rm == Rn) {
		SH4_SetTrue(1);
	} else {
		SH4_SetTrue(0);
	}
}

/**
 **************************************************************
 * \fn void sh4_cmpge(void)
 * Compare signed greater or equal. Set T Flag if Rn >= Rm.
 * v1
 **************************************************************
 */
void
sh4_cmpge(void)
{
	int rn = (ICODE >> 8) & 0xf;
	int rm = (ICODE >> 4) & 0xf;
	int32_t Rn = SH4_GetGpr(rn);
	int32_t Rm = SH4_GetGpr(rm);
	if(Rn >= Rm) {
		SH4_SetTrue(1);
	} else {
		SH4_SetTrue(0);
	}
}

/**
 ****************************************************************
 * \fn void sh4_cmpgt(void)
 * Compare signed greater. Set T Flag if Rn > Rm. 
 * v1
 ****************************************************************
 */
void
sh4_cmpgt(void)
{
	int rn = (ICODE >> 8) & 0xf;
	int rm = (ICODE >> 4) & 0xf;
	int32_t Rn = SH4_GetGpr(rn);
	int32_t Rm = SH4_GetGpr(rm);
	if(Rn > Rm) {
		SH4_SetTrue(1);
	} else {
		SH4_SetTrue(0);
	}
}

/**
 **************************************************************
 * \fn void sh4_cmphi(void)
 * Compare unsigned if higher. Set T Flag if Rn > Rm. 
 **************************************************************
 */
void
sh4_cmphi(void)
{
	int rn = (ICODE >> 8) & 0xf;
	int rm = (ICODE >> 4) & 0xf;
	uint32_t Rn = SH4_GetGpr(rn);
	uint32_t Rm = SH4_GetGpr(rm);
	if(Rn > Rm) {
		SH4_SetTrue(1);
	} else {
		SH4_SetTrue(0);
	}
}

/**
 *************************************************************
 * \fn void sh4_cmphs(void)
 * Compare if higher or same. Set T Flag if Rn >= Rm.
 *************************************************************
 */
void
sh4_cmphs(void)
{
	int rn = (ICODE >> 8) & 0xf;
	int rm = (ICODE >> 4) & 0xf;
	uint32_t Rn = SH4_GetGpr(rn);
	uint32_t Rm = SH4_GetGpr(rm);
	if(Rn >= Rm) {
		SH4_SetTrue(1);
	} else {
		SH4_SetTrue(0);
	}
}

/**
 ************************************************************
 * \fn void sh4_cmppl(void)
 * Compare of Plus. Set T flag if Rn > 0.
 * v1
 ************************************************************
 */
void
sh4_cmppl(void)
{
	int rn = (ICODE >> 8) & 0xf;
	int32_t Rn = SH4_GetGpr(rn);
	if(Rn > 0) {
		SH4_SetTrue(1);
	} else {
		SH4_SetTrue(0);
	}
}

/**
 ***********************************************************
 * \fn void sh4_cmppz(void)
 * Compare if Plus or Zero. Set T Flag if Rn >= 0.
 * v1
 ***********************************************************
 */
void
sh4_cmppz(void)
{
	int rn = (ICODE >> 8) & 0xf;
	int32_t Rn = SH4_GetGpr(rn);
	if(Rn >= 0) {
		SH4_SetTrue(1);
	} else {
		SH4_SetTrue(0);
	}
	
}

/**
 ************************************************
 * \fn void sh4_cmpstr(void);
 * Page 233/236
 * Compare if any bytes are equal.
 * v1
 ************************************************
 */
void
sh4_cmpstr(void)
{
	int rn = (ICODE >> 8) & 0xf;
	int rm = (ICODE >> 4) & 0xf;
	uint32_t Rn,Rm,temp;
	uint32_t hh,hl,lh,ll;
	Rn = SH4_GetGpr(rn);
	Rm = SH4_GetGpr(rm);
	temp = Rn ^ Rm;
	hh = temp >> 24;
	hl = (temp >> 16) & 0xff;
	lh = (temp >> 8) & 0xff;
	ll = temp & 0xff;
	hh = hh && hl && lh && ll;
	if(hh == 0) {
		SH4_SetTrue(1);
	} else {
		SH4_SetTrue(0);
	}	
}

/**
 *********************************************************************
 * \fn void sh4_cmpeq_imm_r0(void)
 * Compare if equal. Set T flag if R0 is equal a signed immediate.
 * v1
 *********************************************************************
 */
void
sh4_cmpeq_imm_r0(void)
{
	int32_t R0; 
	int32_t imm = (int8_t)(ICODE & 0xff);
	R0 = SH4_GetGpr(0);
	if(R0 == imm) {
		SH4_SetTrue(1);
	} else {
		SH4_SetTrue(0);
	}
}

/**
 *********************************************************************
 * \fn void sh4_div0s(void)
 * Divide (step 0) as signed.  
 * v1
 *********************************************************************
 */
void
sh4_div0s(void)
{
	int rn = (ICODE >> 8) & 0xf;
	int rm = (ICODE >> 4) & 0xf;
	int32_t Rn,Rm;
	int Q,M;
	Rn = SH4_GetGpr(rn);	
	Rm = SH4_GetGpr(rm);	
	if(Rn >= 0) {
		Q = 0;
	}  else {
		Q = 1;
	}
	if(Rm >= 0) {
		M = 0;
	} else {
		M = 1;
	}
	SH4_ModSRFlags(Q,SR_Q);
	SH4_ModSRFlags(M,SR_M);
	SH4_ModSRFlags(M ^ Q,SR_T);
}

/**
 ******************************************************
 * \fn void sh4_div0u(void)
 * Divide (step 0) as unsigned
 * v1
 ******************************************************
 */
void
sh4_div0u(void)
{
	SH4_ModSRFlags(0,SR_Q | SR_M | SR_T);
}

/**
 **********************************************************************
 * \fn void sh4_div1(void)
 * Divide (step 1).
 * I think I should use the version from ST40 manual. It
 * is easier to read. 
 * v1
 **********************************************************************
 */
void
sh4_div1(void)
{
	int old_q;
	int Q,M,T;
	//uint32_t tmp0;
	uint32_t tmp0,tmp1,tmp2;
	int rn = (ICODE >> 8) & 0xf;
	int rm = (ICODE >> 4) & 0xf;
	uint32_t Rn,Rm;
	Rn = SH4_GetGpr(rn);	
	Rm = SH4_GetGpr(rm);	
	old_q = SH4_GetSRFlag(SR_Q);	
	Q = ((Rn & (1 << 31)) != 0);
	tmp2 = Rm;
	Rn <<= 1;
	Rn |= SH4_GetTrue();
	M = SH4_GetSRFlag(SR_M);
	tmp0 = Rn;
	switch(old_q) {
		case 0:
		    if(M == 0) {
			Rn -= tmp2;
			tmp1 = (Rn > tmp0);
			if(Q == 0) {
				Q = !!tmp1;
				break;
			} else {
				Q = !tmp1;
			}

		    } else {
			Rn += tmp2;
			tmp1 = (Rn < tmp0);
			if(Q == 0) {
				Q = !tmp1;
			} else {
				Q = !!tmp1;
			}

		    }
		    break;
		case 1:
		    if(M == 0) {
			Rn += tmp2;
			tmp1 = (Rn < tmp0);
			if(Q == 0) {
				Q = !!tmp1;
			} else {
				Q = !tmp1;
			}
		    } else {
			Rn -= tmp2;
			tmp1 = (Rn > tmp0);
			if(Q == 0) {
				Q = !tmp1;
			} else {
				Q = !!tmp1;
			}
		    }
			break;
	}
	SH4_SetGpr(Rn,rn);
	SH4_ModSRFlags(Q,SR_Q);
	T = (Q == M);
	SH4_ModSRFlags(T,SR_T);
	fprintf(stderr,"sh4_div1 not tested\n");
}

/**
 ***********************************************************
 * \fn void sh4_dmulsl(void)
 * double length multiply as signed.
 * v1
 ***********************************************************
 */
void
sh4_dmulsl(void)
{
	int32_t Rn,Rm;
	int64_t result;
	int rn = (ICODE >> 8) & 0xf;
	int rm = (ICODE >> 4) & 0xf;
	Rn = SH4_GetGpr(rn);	
	Rm = SH4_GetGpr(rm);	
	result = (int64_t)Rn * (int64_t)Rm;
	SH4_SetMacH(result >> 32); 
	SH4_SetMacL(result); 
}

/**
 *********************************************************
 * \fn void sh4_dmulul(void)
 * double length multiply as unsigned. 
 * v1
 *********************************************************
 */
void
sh4_dmulul(void)
{
	uint32_t Rn,Rm;
	uint64_t result;
	int rn = (ICODE >> 8) & 0xf;
	int rm = (ICODE >> 4) & 0xf;
	Rn = SH4_GetGpr(rn);	
	Rm = SH4_GetGpr(rm);	
	result = (uint64_t)Rn * (uint64_t)Rm;
	SH4_SetMacH(result >> 32); 
	SH4_SetMacL(result); 
}

/**
 ********************************************************************
 * \fn void sh4_dt(void)
 * Decrement and Test. 
 * v1
 ********************************************************************
 */
void
sh4_dt(void)
{
	int rn = (ICODE >> 8) & 0xf;
	uint32_t Rn = SH4_GetGpr(rn);	
	Rn--;	
	SH4_SetGpr(Rn,rn);
	if(Rn == 0) {
		SH4_SetTrue(1);
	} else {
		SH4_SetTrue(0);
	}
}

/**
 ******************************************************************
 * \fn void sh4_extsb(void)
 * sign extend byte to 32 Bit.
 * v1
 ******************************************************************
 */
void
sh4_extsb(void)
{
	int32_t Rn;
	int rn = (ICODE >> 8) & 0xf;
	int rm = (ICODE >> 4) & 0xf;
	Rn = (int32_t)(int8_t)SH4_GetGpr(rm);	
	SH4_SetGpr(Rn,rn);
}

/**
 ******************************************************************
 * \fn void sh4_extsw(void)
 * sign extend word to 32 Bit.
 * v1
 ******************************************************************
 */
void
sh4_extsw(void)
{
	int32_t Rn;
	int rn = (ICODE >> 8) & 0xf;
	int rm = (ICODE >> 4) & 0xf;
	Rn = (int32_t)(int16_t)SH4_GetGpr(rm);	
	SH4_SetGpr(Rn,rn);
}

/**
 ******************************************************************
 * \fn void sh4_extub(void)
 * extend byte as unsigned to 32 Bit.
 * v1
 ******************************************************************
 */
void
sh4_extub(void)
{
	uint32_t Rn;
	int rn = (ICODE >> 8) & 0xf;
	int rm = (ICODE >> 4) & 0xf;
	Rn = (uint32_t)(uint8_t)SH4_GetGpr(rm);	
	SH4_SetGpr(Rn,rn);
}

/**
 ******************************************************************
 * \fn void sh4_extuw(void)
 * extend unsigned word to 32 Bit.
 * v1
 ******************************************************************
 */
void
sh4_extuw(void)
{
	uint32_t Rn;
	int rn = (ICODE >> 8) & 0xf;
	int rm = (ICODE >> 4) & 0xf;
	Rn = (uint32_t)(uint16_t)SH4_GetGpr(rm);	
	SH4_SetGpr(Rn,rn);
}

void
sh4_fabs_frdr(void)
{
	uint32_t sr = SH4_GetSR();
	uint32_t fpscr = SH4_GetFPSCR();
	if(unlikely(sr & SR_FD)) {
		SH4_Exception(EX_FPUDIS);
	}
	if((fpscr & FPSCR_PR) == 0) {
		/* 32 Bit FRn version */ 
		int rn = (ICODE >> 8) & 0xf;
		Float32_t Frn;
		Frn = SH4_GetFpr(rn);
		Frn &= ~(UINT32_C(1) << 31);
		SH4_SetFpr(Frn,rn);
	}  else if((fpscr & FPSCR_SZ) == 0) {
		/* 64 Bit version */	
		int rn = (ICODE >> 9) & 0x7;
		Float64_t Drn;
		Drn = SH4_GetDpr(rn);
		Drn &= ~(UINT64_C(1) << 63);
		SH4_SetDpr(Drn,rn);
	} else {
		fprintf(stderr,"sh4_fabs_fr illegal configuration register value\n");
	}
}

void
sh4_fadd_frdr(void)
{
	uint32_t sr = SH4_GetSR();
	SoftFloatContext *sf = SH4_GetSFloat();
	uint32_t fpscr = SH4_GetFPSCR();
	if(unlikely(sr & SR_FD)) {
		SH4_Exception(EX_FPUDIS);
	}
	if((fpscr & FPSCR_PR) == 0) {
		/* 32 Bit FRn version */ 
		int frn = (ICODE >> 8) & 0xf;
		int frm = (ICODE >> 4) & 0xf;
		Float32_t Frn,Frm,result;
		Frn = SH4_GetFpr(frn);
		Frm = SH4_GetFpr(frm);
		result = Float32_Add(sf,Frn,Frm);
		SH4_SetFpr(result,frn);
	}  else if((fpscr & FPSCR_SZ) == 0) {
		/* 64 Bit version */	
		int drn = (ICODE >> 9) & 0x7;
		int drm = (ICODE >> 5) & 0x7;
		Float64_t Drn,Drm,result;
		Drn = SH4_GetDpr(drn);
		Drm = SH4_GetDpr(drm);
		result = Float64_Add(sf,Drn,Drm);
		SH4_SetDpr(result,drn);
	} else {
		fprintf(stderr,"sh4_fabs_fr illegal configuration register value\n");
	}
}

void
sh4_fcmpeq_frdr(void)
{
	uint32_t sr = SH4_GetSR();
	SoftFloatContext *sf = SH4_GetSFloat();
	uint32_t fpscr = SH4_GetFPSCR();
	if(unlikely(sr & SR_FD)) {
		SH4_Exception(EX_FPUDIS);
	}
	if((fpscr & FPSCR_PR) == 0) {
		int frn = (ICODE >> 8) & 0xf;
		int frm = (ICODE >> 4) & 0xf;
		Float32_t Frn,Frm;
		Frn = SH4_GetFpr(frn);
		Frm = SH4_GetFpr(frm);
		if(Float32_Cmp(sf,Frn,Frm) == SFC_EQUAL) {
			SH4_SetTrue(1);
		} else {
			SH4_SetTrue(0);
		}

	}  else {
		int drn = (ICODE >> 9) & 0x7;
		int drm = (ICODE >> 5) & 0x7;
		Float64_t Drn,Drm;
		Drn = SH4_GetDpr(drn);
		Drm = SH4_GetDpr(drm);
		if(Float64_Cmp(sf,Drn,Drm) == SFC_EQUAL) {
			SH4_SetTrue(1);
		} else {
			SH4_SetTrue(0);
		}
	}
	fprintf(stderr,"sh4_fcmpeq_fr not tested\n");
}

void
sh4_fcmpgt_frdr(void)
{
	uint32_t sr = SH4_GetSR();
	SoftFloatContext *sf = SH4_GetSFloat();
	uint32_t fpscr = SH4_GetFPSCR();
	if(unlikely(sr & SR_FD)) {
		SH4_Exception(EX_FPUDIS);
	}
	if((fpscr & FPSCR_PR) == 0) {
		int frn = (ICODE >> 8) & 0xf;
		int frm = (ICODE >> 4) & 0xf;
		Float32_t Frn,Frm;
		Frn = SH4_GetFpr(frn);
		Frm = SH4_GetFpr(frm);
		if(Float32_Cmp(sf,Frn,Frm) == SFC_GREATER) {
			SH4_SetTrue(1);
		} else {
			SH4_SetTrue(0);
		}
	}  else {
		int drn = (ICODE >> 9) & 0x7;
		int drm = (ICODE >> 5) & 0x7;
		Float64_t Drn,Drm;
		Drn = SH4_GetDpr(drn);
		Drm = SH4_GetDpr(drm);
		if(Float64_Cmp(sf,Drn,Drm) == SFC_GREATER) {
			SH4_SetTrue(1);
		} else {
			SH4_SetTrue(0);
		}
	}
	fprintf(stderr,"sh4_fcmpgt_fr not tested\n");
}

/**
 * \fn void sh4_fcnvds(void)
 * Floating point convert double to single precision
 */
void
sh4_fcnvds(void)
{
	SoftFloatContext *sf = SH4_GetSFloat();
	int drm = (ICODE >> 9) & 7;
	Float64_t Drm;
	Float32_t fpul;
	Drm = SH4_GetDpr(drm);
	fpul = Float32_FromFloat64(sf,Drm);
	SH4_SetFPUL(fpul);
	fprintf(stderr,"sh4_fcnvds not tested\n");
}

/**
 ************************************************************
 * \fn void sh4_fcnvsd(void)
 * Floating point convert single to double precision.
 ************************************************************
 */
void
sh4_fcnvsd(void)
{
	SoftFloatContext *sf = SH4_GetSFloat();
	int drm = (ICODE >> 9) & 7;
	Float64_t Drm;
	Float32_t fpul;
	fpul = SH4_GetFPUL();
	Drm = Float64_FromFloat32(sf,fpul);
	SH4_SetDpr(Drm,drm);
	fprintf(stderr,"sh4_fcnvsd not tested\n");
}

void
sh4_fdiv_frdr(void)
{
	uint32_t sr = SH4_GetSR();
	SoftFloatContext *sf = SH4_GetSFloat();
	uint32_t fpscr = SH4_GetFPSCR();
	if(unlikely(sr & SR_FD)) {
		SH4_Exception(EX_FPUDIS);
	}
	if((fpscr & FPSCR_PR) == 0) {
		/* 32 Bit FRn version */ 
		int frn = (ICODE >> 8) & 0xf;
		int frm = (ICODE >> 4) & 0xf;
		Float32_t Frn,Frm,result;
		Frn = SH4_GetFpr(frn);
		Frm = SH4_GetFpr(frm);
		result = Float32_Div(sf,Frn,Frm);
		SH4_SetFpr(result,frn);
	}  else  {
		/* 64 Bit version */	
		int drn = (ICODE >> 9) & 0x7;
		int drm = (ICODE >> 5) & 0x7;
		Float64_t Drn,Drm,result;
		Drn = SH4_GetDpr(drn);
		Drm = SH4_GetDpr(drm);
		result = Float64_Div(sf,Drn,Drm);
		SH4_SetDpr(result,drn);
	} 
	fprintf(stderr,"sh4_fdiv_fr not tested\n");
}

/**
 ************************************************************
 * \fn void sh4_fipr(void)
 * Does not implement correct rounding and exceptions.
 ************************************************************
 */
void
sh4_fipr(void)
{
	SoftFloatContext *sf = SH4_GetSFloat();
	int i;
	int fvm = (ICODE >> 8) & 3;
	int fvn = (ICODE >> 10) & 3;
	int frn = fvn << 2;
	int frm = fvm << 2;
	Float32_t Frn,Frm;
	Float32_t sum = 0;
	Float32_t prod;
	for(i=0;i<4;i++) {
		Frn = SH4_GetFpr(frn);
		Frm = SH4_GetFpr(frm);
		prod = Float32_Mul(sf,Frn,Frm);
		sum = Float32_Add(sf,sum,prod);
	}
	SH4_SetFpr(sum,fvn + 3);
	fprintf(stderr,"sh4_fipr not tested\n");
}

void
sh4_fldi0(void)
{
	int frn = (ICODE >> 8) & 0xf;
	SH4_SetFpr(0,frn);
	fprintf(stderr,"sh4_fldi0 not tested\n");
}

void
sh4_fldi1(void)
{
	int frn = (ICODE >> 8) & 0xf;
	SH4_SetFpr(0x3f800000,frn);
	fprintf(stderr,"sh4_fldi1 not tested\n");
}

void
sh4_flds(void)
{
	int frn = (ICODE >> 8) & 0xf;
	uint32_t Frn;
	Frn = SH4_GetFpr(frn);
	SH4_SetFPUL(Frn);
	fprintf(stderr,"sh4_flds not tested\n");
}

/**
 * Exceptions are missing here
 */
void
sh4_float_fpul_frdr(void)
{
	uint32_t fpscr = SH4_GetFPSCR();
	SoftFloatContext *sf = SH4_GetSFloat();
	if((fpscr & FPSCR_PR) == 0) {
		int frn = (ICODE >> 8) & 0xf;
		Float32_t Frn;
		int32_t fpul = SH4_GetFPUL();
		Frn = Float32_FromInt32(sf,fpul,SFM_ROUND_ZERO);
		SH4_SetFpr(Frn,frn);
	} else {
		int drn = (ICODE >> 9) & 0x7;
		Float64_t Drn;
		int32_t fpul = SH4_GetFPUL();
		Drn = Float64_FromInt32(sf,fpul);
		SH4_SetDpr(Drn,drn);
	}
	fprintf(stderr,"sh4_fpul_frdr not tested\n");
}

/**
 **************************************************************
 * void sh4_fmac(void)
 * Floating point multiply and accumulate.
 **************************************************************
 */
void
sh4_fmac(void)
{
	SoftFloatContext *sf = SH4_GetSFloat();
	int frn = (ICODE >> 8) & 0xf;
	int frm = (ICODE >> 4) & 0xf;
	uint32_t fpscr = SH4_GetFPSCR();
	Float32_t Fr0,Frn,Frm,prod;
	if((fpscr & FPSCR_PR) == 0) {
		Fr0 = SH4_GetFpr(0);	
		Frn = SH4_GetFpr(frn);	
		Frm = SH4_GetFpr(frm);	
		prod = Float32_Mul(sf,Fr0,Frm);
		Frn = Float32_Add(sf,Frn,prod);
		SH4_SetFpr(Frn,frn);
	}
	fprintf(stderr,"sh4_fmac not tested\n");
}

/**
 * Mov from one floating point register to another
 */
void
sh4_fmov_frdr(void)
{
	uint32_t fpscr = SH4_GetFPSCR();
	if((fpscr & FPSCR_SZ) == 0) {
		int frn = (ICODE >> 8) & 0xf;
		int frm = (ICODE >> 4) & 0xf;
		Float32_t Frm;
		Frm = SH4_GetFpr(frm);
		SH4_SetFpr(Frm,frn);
	} else {
		int drn = (ICODE >> 9) & 0x7;
		int drm = (ICODE >> 5) & 0x7;
		Float64_t Drm;
		Drm = SH4_GetDpr(drm);
		SH4_SetDpr(Drm,drn);
	}
	fprintf(stderr,"sh4_fmov_frdr not tested\n");
}

void
sh4_fmovs_frdrarn(void)
{
	uint32_t fpscr = SH4_GetFPSCR();
	if((fpscr & FPSCR_PR) == 0) {
		if((fpscr & FPSCR_SZ) == 0) {
			int rn = (ICODE >> 8) & 0xf;
			int frm = (ICODE >> 4) & 0xf;
			uint32_t Frm,Rn;
			Frm = SH4_GetFpr(frm);
			Rn = SH4_GetGpr(rn);
			SH4_MMUWrite32(Frm,Rn);
		} else {
			int rn = (ICODE >> 8) & 0xf;
			int drm = (ICODE >> 5) & 0x7;
			uint32_t Drm,Rn;
			Drm = SH4_GetDpr(drm);
			Rn = SH4_GetGpr(rn);
			SH4_MMUWrite64(Drm,Rn);
		}
	} else {
		int rn = (ICODE >> 8) & 0xf;
		int xdm = (ICODE >> 5) & 7;
		uint32_t Rn;
		uint64_t Xdm = SH4_GetXD(xdm);
		Rn = SH4_GetGpr(rn);
		SH4_MMUWrite64(Xdm,Rn);
	}
	fprintf(stderr,"sh4_fmovs_frarn not tested\n");
}

void
sh4_fmovs_armfrdr(void)
{
	uint32_t fpscr = SH4_GetFPSCR();
	if((fpscr & FPSCR_PR) == 0) {
		if((fpscr & FPSCR_SZ) == 0) {
			int rm = (ICODE >> 4) & 0xf;
			int frn = (ICODE >> 8) & 0xf;
			uint32_t Frn,Rm;
			Rm = SH4_GetGpr(rm);
			Frn = SH4_MMURead32(Rm);
			SH4_SetFpr(Frn,frn);
		} else {
			int rm = (ICODE >> 9) & 0x7;
			int drn = (ICODE >> 4) & 0xf;
			uint32_t Drn,Rm;
			Rm = SH4_GetGpr(rm);
			Drn = SH4_MMURead64(Rm);
			SH4_SetDpr(Drn,drn);
		}
	} else {
		int rm = (ICODE >> 4) & 0xf;
		int xdn = (ICODE >> 9) & 7;
		uint32_t Rm;
		uint64_t Xdn;
		Rm = SH4_GetGpr(rm);
		Xdn = SH4_MMURead64(Rm);
		SH4_SetXD(Xdn,xdn);
	}
	fprintf(stderr,"sh4_fmovs_rmfr not tested\n");
}

void
sh4_fmovs_armpfrdr(void)
{
	uint32_t fpscr = SH4_GetFPSCR();
	if((fpscr & FPSCR_PR) == 0) {
		if((fpscr & FPSCR_SZ) == 0) {
			int rm = (ICODE >> 4) & 0xf;
			int frn = (ICODE >> 8) & 0xf;
			uint32_t Frn,Rm;
			Rm = SH4_GetGpr(rm);
			Frn = SH4_MMURead32(Rm);
			SH4_SetFpr(Frn,frn);
			Rm += 4;
			SH4_SetGpr(Rm,rm);
		} else {
			int rm = (ICODE >> 9) & 0x7;
			int drn = (ICODE >> 4) & 0xf;
			uint32_t Drn,Rm;
			Rm = SH4_GetGpr(rm);
			Drn = SH4_MMURead64(Rm);
			SH4_SetDpr(Drn,drn);
			Rm += 8;
			SH4_SetGpr(Rm,rm);
		}
	} else {
		int rm = (ICODE >> 4) & 0xf;
		int xdn = (ICODE >> 9) & 7;
		uint32_t Rm;
		uint64_t Xdn;
		Rm = SH4_GetGpr(rm);
		Xdn = SH4_MMURead64(Rm);
		SH4_SetXD(Xdn,xdn);
		Rm += 8;
		SH4_SetGpr(Rm,rm);
	}
	fprintf(stderr,"sh4_fmovs_rmpfr not implemented\n");
}

void
sh4_fmov_frdramrn(void)
{
	uint32_t fpscr = SH4_GetFPSCR();
	if((fpscr & FPSCR_PR) == 0) {
		if((fpscr & FPSCR_SZ) == 0) {
			int rn = (ICODE >> 8) & 0xf;
			int frm = (ICODE >> 4) & 0xf;
			uint32_t Frm,Rn;
			Frm = SH4_GetFpr(frm);
			Rn = SH4_GetGpr(rn);
			Rn -= 4;
			SH4_MMUWrite32(Frm,Rn);
			SH4_SetGpr(Rn,rn);
		} else {
			int rn = (ICODE >> 8) & 0xf;
			int drm = (ICODE >> 5) & 0x7;
			uint32_t Drm,Rn;
			Drm = SH4_GetDpr(drm);
			Rn = SH4_GetGpr(rn);
			Rn -= 8;
			SH4_MMUWrite64(Drm,Rn);
			SH4_SetGpr(Rn,rn);
		}
	} else {
		int rn = (ICODE >> 8) & 0xf;
		int xdm = (ICODE >> 5) & 7;
		uint32_t Rn;
		uint64_t Xdm = SH4_GetXD(xdm);
		Rn = SH4_GetGpr(rn);
		Rn -= 8;
		SH4_MMUWrite64(Xdm,Rn);
		SH4_SetGpr(Rn,rn);
	}
	fprintf(stderr,"sh4_fmov_frmrn not tested\n");
}

void
sh4_fmovs_ar0rmfrdr(void)
{
	uint32_t fpscr = SH4_GetFPSCR();
	if((fpscr & FPSCR_PR) == 0) {
		if((fpscr & FPSCR_SZ) == 0) {
			int rm = (ICODE >> 4) & 0xf;
			int frn = (ICODE >> 8) & 0xf;
			uint32_t Frn,Rm,R0;
			Rm = SH4_GetGpr(rm);
			R0 = SH4_GetGpr(0);
			Frn = SH4_MMURead32(Rm + R0);
			SH4_SetFpr(Frn,frn);
		} else {
			int rm = (ICODE >> 9) & 0x7;
			int drn = (ICODE >> 4) & 0xf;
			uint32_t Drn,Rm,R0;
			Rm = SH4_GetGpr(rm);
			R0 = SH4_GetGpr(0);
			Drn = SH4_MMURead64(Rm + R0);
			SH4_SetDpr(Drn,drn);
		}
	} else {
		int rm = (ICODE >> 4) & 0xf;
		int xdn = (ICODE >> 9) & 7;
		uint32_t Rm,R0;
		uint64_t Xdn;
		Rm = SH4_GetGpr(rm);
		R0 = SH4_GetGpr(0);
		Xdn = SH4_MMURead64(Rm + R0);
		SH4_SetXD(Xdn,xdn);
	}
	fprintf(stderr,"sh4_fmovs_r0rmfr not tested\n");
}

void
sh4_fmovs_frdrar0rn(void)
{
	uint32_t fpscr = SH4_GetFPSCR();
	if((fpscr & FPSCR_PR) == 0) {
		if((fpscr & FPSCR_SZ) == 0) {
			int rn = (ICODE >> 8) & 0xf;
			int frm = (ICODE >> 4) & 0xf;
			uint32_t Frm,Rn,R0;
			Frm = SH4_GetFpr(frm);
			Rn = SH4_GetGpr(rn);
			R0 = SH4_GetGpr(0);
			SH4_MMUWrite32(Frm,Rn + R0);
		} else {
			int rn = (ICODE >> 8) & 0xf;
			int drm = (ICODE >> 5) & 0x7;
			uint32_t Drm,Rn,R0;
			Drm = SH4_GetDpr(drm);
			Rn = SH4_GetGpr(rn);
			R0 = SH4_GetGpr(0);
			SH4_MMUWrite64(Drm,Rn + R0);
		}
	} else {
		int rn = (ICODE >> 8) & 0xf;
		int xdm = (ICODE >> 5) & 7;
		uint32_t Rn,R0;
		uint64_t Xdm = SH4_GetXD(xdm);
		Rn = SH4_GetGpr(rn);
		R0 = SH4_GetGpr(0);
		SH4_MMUWrite64(Xdm,Rn + R0);
	}
	fprintf(stderr,"sh4_fmovs_frdrar0rn not tested\n");
}

void
sh4_fmul_frdr(void)
{
	uint32_t sr = SH4_GetSR();
	SoftFloatContext *sf = SH4_GetSFloat();
	uint32_t fpscr = SH4_GetFPSCR();
	if(unlikely(sr & SR_FD)) {
		SH4_Exception(EX_FPUDIS);
	}
	if((fpscr & FPSCR_PR) == 0) {
		/* 32 Bit FRn version */ 
		int frn = (ICODE >> 8) & 0xf;
		int frm = (ICODE >> 4) & 0xf;
		Float32_t Frn,Frm,result;
		Frn = SH4_GetFpr(frn);
		Frm = SH4_GetFpr(frm);
		result = Float32_Mul(sf,Frn,Frm);
		SH4_SetFpr(result,frn);
	}  else  {
		/* 64 Bit version */	
		int drn = (ICODE >> 9) & 0x7;
		int drm = (ICODE >> 5) & 0x7;
		Float64_t Drn,Drm,result;
		Drn = SH4_GetDpr(drn);
		Drm = SH4_GetDpr(drm);
		result = Float64_Mul(sf,Drn,Drm);
		SH4_SetDpr(result,drn);
	} 
	fprintf(stderr,"sh4_fmul_frdr not tested\n");
}

void
sh4_fneg_frdr(void)
{
	int frn = (ICODE >> 8) & 0xf;
	Float32_t Frn = SH4_GetFpr(frn);
	Frn ^= (UINT32_C(1) << 31);
	SH4_SetFpr(Frn,frn);
	fprintf(stderr,"sh4_fneg_fr not tested\n");
}

void
sh4_frchg(void)
{
	uint32_t fpscr = SH4_GetFPSCR();
	if((fpscr & FPSCR_PR) == 0) {
		fpscr ^= FPSCR_FR;
		SH4_SetFPSCR(fpscr);
	}
	fprintf(stderr,"sh4_frchg not tested\n");
}

void
sh4_fschg(void)
{
	uint32_t fpscr = SH4_GetFPSCR();
	if((fpscr & FPSCR_PR) == 0) {
		fpscr ^= FPSCR_SZ;
		SH4_SetFPSCR(fpscr);
	}
	fprintf(stderr,"sh4_fschg not tested\n");
}

void
sh4_fsqrt_frdr(void)
{
	uint32_t sr = SH4_GetSR();
	SoftFloatContext *sf = SH4_GetSFloat();
	uint32_t fpscr = SH4_GetFPSCR();
	if(unlikely(sr & SR_FD)) {
		SH4_Exception(EX_FPUDIS);
	}
	if((fpscr & FPSCR_PR) == 0) {
		/* 32 Bit FRn version */ 
		int frn = (ICODE >> 8) & 0xf;
		Float32_t Frn,result;
		Frn = SH4_GetFpr(frn);
		result = Float32_Sqrt(sf,Frn);
		SH4_SetFpr(result,frn);
	}  else  {
		/* 64 Bit version */	
		int drn = (ICODE >> 9) & 0x7;
		Float64_t Drn,result;
		Drn = SH4_GetDpr(drn);
		result = Float64_Sqrt(sf,Drn);
		SH4_SetDpr(result,drn);
	} 
	fprintf(stderr,"sh4_fsqrt_fr not tested\n");
}

void
sh4_fsts(void)
{
	int frn = (ICODE >> 8) & 0xf;
	SH4_SetFpr(SH4_GetFPUL(),frn);
	fprintf(stderr,"sh4_fsts not tested\n");
}

void
sh4_fsub_frdr(void)
{
	uint32_t sr = SH4_GetSR();
	SoftFloatContext *sf = SH4_GetSFloat();
	uint32_t fpscr = SH4_GetFPSCR();
	if(unlikely(sr & SR_FD)) {
		SH4_Exception(EX_FPUDIS);
	}
	if((fpscr & FPSCR_PR) == 0) {
		/* 32 Bit FRn version */ 
		int frn = (ICODE >> 8) & 0xf;
		int frm = (ICODE >> 4) & 0xf;
		Float32_t Frn,Frm,result;
		Frn = SH4_GetFpr(frn);
		Frm = SH4_GetFpr(frm);
		result = Float32_Sub(sf,Frn,Frm);
		SH4_SetFpr(result,frn);
	}  else  {
		/* 64 Bit version */	
		int drn = (ICODE >> 9) & 0x7;
		int drm = (ICODE >> 5) & 0x7;
		Float64_t Drn,Drm,result;
		Drn = SH4_GetDpr(drn);
		Drm = SH4_GetDpr(drm);
		result = Float64_Sub(sf,Drn,Drm);
		SH4_SetDpr(result,drn);
	} 
	fprintf(stderr,"sh4_fsub_frdr not tested\n");
}

/*
 * Convert a floating point number into an integer
 */
void
sh4_ftrc_frdr(void)
{
	SoftFloatContext *sf = SH4_GetSFloat();
	uint32_t fpscr = SH4_GetFPSCR();
	int32_t fpul;
	if((fpscr & FPSCR_PR) == 0) {
		int frm = (ICODE >> 8) & 0xf;
		Float32_t Frm = SH4_GetFpr(frm);
		fpul = Float32_ToInt32(sf,Frm,SFM_ROUND_ZERO);
		SH4_SetFPUL(fpul);
	} else {
		int drm = (ICODE >> 9) & 0x7;
		Float64_t Drm = SH4_GetDpr(drm);
		fpul = Float64_ToInt32(sf,Drm,SFM_ROUND_ZERO);
		SH4_SetFPUL(fpul);
	}
	fprintf(stderr,"sh4_ftrc_fr not tested\n");
}

void
sh4_ftrv(void)
{
	fprintf(stderr,"sh4_ftrv not implemented\n");
}

/**
 *******************************************************************
 * \fn void sh4_jmp(void)
 * Jump ( With a delay slot )
 * Unconditional branch to an address read from a register.
 * The ST40 manual says pc = Rn & ~1
 * v1
 *******************************************************************
 */
void
sh4_jmp(void)
{
	
	int rn = (ICODE >> 8) & 0xf;
	uint32_t Rn = SH4_GetGpr(rn);	
	SH4_ExecuteDelaySlot();
	SH4_SetRegPC(Rn & ~UINT32_C(1)); 
}

/**
 ******************************************************************
 * \fn void sh4_jsr(void)
 * Jump to Subroutine. Store the return address in register "PR"
 * The ST40 manual says pc = Rn & ~1
 * v1
 ******************************************************************
 */
void
sh4_jsr(void)
{
	int rn = (ICODE >> 8) & 0xf;
	uint32_t Rn = SH4_GetGpr(rn);	
	SH4_SetPR(SH4_NNIA);	
	SH4_ExecuteDelaySlot();
	SH4_SetRegPC(Rn & ~UINT32_C(1)); 
}

/**
 ******************************************************************
 * \fn void sh4_ldc_rmsr(void)
 *  Load control register: Load Status Register from Rm.
 * v1
 ******************************************************************
 */
void
sh4_ldc_rmsr(void)
{
	int rm = (ICODE >> 8) & 0xf;
	uint32_t Rm = SH4_GetGpr(rm);
	if(SH4_GetSR() & SR_MD) {
		SH4_SetSR(Rm);
	} else {
		SH4_Exception(EX_RESINST);
	}
}

/**
 ****************************************************************** 
 * \fn sh4_ldc_rmgbr(void)
 * Load control register: Load GBR from Rm.
 * v1
 ****************************************************************** 
 */
void
sh4_ldc_rmgbr(void)
{
	int rm = (ICODE >> 8) & 0xf;
	uint32_t Rm = SH4_GetGpr(rm);
	SH4_SetGBR(Rm);
}

/**
 ******************************************************************
 * \fn void sh4_ldc_rmvbr(void)
 * Load control register: Load VBR from Rm
 * v1
 ******************************************************************
 */
void
sh4_ldc_rmvbr(void)
{
	int rm = (ICODE >> 8) & 0xf;
	uint32_t Rm = SH4_GetGpr(rm);	
	if(SH4_GetSR() & SR_MD) {
		SH4_SetVBR(Rm);	
	} else {
		SH4_Exception(EX_RESINST);
	}
}

/**
 *****************************************************************
 * \fn void sh4_ldc_rmssr(void)
 * Load control register: Load SSR from Rm.
 * v1
 *****************************************************************
 */
void
sh4_ldc_rmssr(void)
{
	int rm = (ICODE >> 8) & 0xf;
	uint32_t Rm = SH4_GetGpr(rm);	
	if(SH4_GetSR() & SR_MD) {
		SH4_SetSSR(Rm);	
	} else {
		SH4_Exception(EX_RESINST);
	}
}

/**
 **********************************************************
 * \fn void sh4_ldc_rmspc
 * Load control register: Load SPC from Rm.
 * v1
 **********************************************************
 */
void
sh4_ldc_rmspc(void)
{
	int rm = (ICODE >> 8) & 0xf;
	uint32_t Rm = SH4_GetGpr(rm);	
	if(SH4_GetSR() & SR_MD) {
		SH4_SetSPC(Rm);	
	} else {
		SH4_Exception(EX_RESINST);
	}
}

/**
 *********************************************************
 * \fn void sh4_ldc_rmdbr
 * Load control register: Load DBR from Rm.
 *********************************************************
 */
void
sh4_ldc_rmdbr(void)
{
	int rm = (ICODE >> 8) & 0xf;
	uint32_t Rm = SH4_GetGpr(rm);	
	if(SH4_GetSR() & SR_MD) {
		SH4_SetDBR(Rm);	
	} else {
		SH4_Exception(EX_RESINST);
	}
}

/** 
 ********************************************************
 * \fn void sh4_ldc_rmr0b(void)
 * Load register Rb of other bank From Rm
 ********************************************************
 */
void sh4_ldc_rmrb(void) 
{
	int rm = (ICODE >> 8) & 0xf;
	int rb = (ICODE >> 4) & 7;
	uint32_t Rm = SH4_GetGpr(rm);	
	if(SH4_GetSR() & SR_MD) {
		SH4_SetGprBank(Rm,rb);	
	} else {
		SH4_Exception(EX_RESINST);
	}
	
}

/**
 *******************************************************************
 * \fn void sh4_ldcl_atrmpsr(void)
 * Load status register from Address in Rm. Post increment Rm.
 * v1
 *******************************************************************
 */
void
sh4_ldcl_atrmpsr(void)
{
	int rm = (ICODE >> 8) & 0xf;
	uint32_t Rm = SH4_GetGpr(rm);	
	uint32_t value;
	if(SH4_GetSR() & SR_MD) {
		value = SH4_MMURead32(Rm);
		SH4_SetSR(value);	
		Rm += 4;
		SH4_SetGpr(Rm,rm);	
	} else {
		SH4_Exception(EX_RESINST);
	}
}

/**
 ******************************************************************
 * void sh4_ldcl_atrmpgbr(void)
 * Load GBR register from memory address in Rm. Post increment Rm.
 * v1
 ******************************************************************
 */
void
sh4_ldcl_atrmpgbr(void)
{
	int rm = (ICODE >> 8) & 0xf;
	uint32_t Rm = SH4_GetGpr(rm);	
	uint32_t value = SH4_MMURead32(Rm);
	SH4_SetGBR(value);	
	Rm += 4;
	SH4_SetGpr(Rm,rm);	
}

/**
 ******************************************************************
 * void sh4_ldcl_atrmpvbr(void)
 * Load VBR register from memory address in Rm. Post increment Rm.
 * v1
 ******************************************************************
 */
void
sh4_ldcl_atrmpvbr(void)
{
	int rm = (ICODE >> 8) & 0xf;
	uint32_t Rm = SH4_GetGpr(rm);	
	uint32_t value;
	if(SH4_GetSR() & SR_MD) {
		value = SH4_MMURead32(Rm);
		SH4_SetVBR(value);	
		Rm += 4;
		SH4_SetGpr(Rm,rm);	
	} else {
		SH4_Exception(EX_RESINST);
	}
}

/**
 ******************************************************************
 * void sh4_ldcl_atrmpssr(void)
 * Load SSR register from memory address in Rm. Post increment Rm. 
 * v1
 ******************************************************************
 */
void
sh4_ldcl_atrmpssr(void)
{
	int rm = (ICODE >> 8) & 0xf;
	uint32_t Rm = SH4_GetGpr(rm);	
	uint32_t value;
	if(SH4_GetSR() & SR_MD) {
		value = SH4_MMURead32(Rm);
		SH4_SetSSR(value);	
		Rm += 4;
		SH4_SetGpr(Rm,rm);	
	} else {
		SH4_Exception(EX_RESINST);
	}
}

/**
 ******************************************************************
 * void sh4_ldcl_atrmpspc(void)
 * Load SPC register from memory address in Rm. Post increment Rm. 
 * v1
 ******************************************************************
 */
void
sh4_ldcl_atrmpspc(void)
{
	int rm = (ICODE >> 8) & 0xf;
	uint32_t Rm = SH4_GetGpr(rm);	
	uint32_t value;
	if(SH4_GetSR() & SR_MD) {
		value = SH4_MMURead32(Rm);
		SH4_SetSPC(value);	
		Rm += 4;
		SH4_SetGpr(Rm,rm);	
	} else {
		SH4_Exception(EX_RESINST);
	}
}

/**
 ******************************************************************
 * void sh4_ldcl_atrmpdbr(void)
 * Load DBR register from memory address in Rm. Post increment Rm. 
 * v1
 ******************************************************************
 */
void
sh4_ldcl_atrmpdbr(void)
{
	int rm = (ICODE >> 8) & 0xf;
	uint32_t Rm = SH4_GetGpr(rm);	
	uint32_t value;
	if(SH4_GetSR() & SR_MD) {
		value = SH4_MMURead32(Rm);
		SH4_SetDBR(value);	
		Rm += 4;
		SH4_SetGpr(Rm,rm);	
	} else {
		SH4_Exception(EX_RESINST);
	}
}

/**
 ******************************************************************
 * void sh4_ldcl_atrmprb(void)
 * Load Other Bank register from memory address in Rm. 
 * Post increment Rm. 
 * v1
 ******************************************************************
 */

void
sh4_ldcl_atrmprb(void)
{
	int rm = (ICODE >> 8) & 0xf;
	int rb = (ICODE >> 4) & 7;
	uint32_t Rm = SH4_GetGpr(rm);	
	uint32_t value;
	if(SH4_GetSR() & SR_MD) {
		value = SH4_MMURead32(Rm);
		SH4_SetGprBank(value,rb);	
		Rm += 4;
		SH4_SetGpr(Rm,rm);	
	} else {
		SH4_Exception(EX_RESINST);
	}
}


/*
 ******************************************
 * Rm to Floating point
 ******************************************
 */
void
sh4_lds_rmfpul(void)
{
	int rm = (ICODE >> 8) & 0xf;
	uint32_t Rm = SH4_GetGpr(rm);
	SH4_SetFPUL(Rm);
	fprintf(stderr,"sh4_lds_rmfpul not tested\n");
}

void
sh4_ldsl_armpfpul(void)
{
	int rm = (ICODE >> 8) & 0xf;
	uint32_t Rm = SH4_GetGpr(rm);
	SH4_SetFPUL(SH4_MMURead32(Rm));
	Rm += 4;
	SH4_SetGpr(Rm,rm);
	fprintf(stderr,"sh4_ldsl_rmpfpul not tested\n");
}

void
sh4_lds_rmfpscr(void)
{
	int rm = (ICODE >> 8) & 0xf;
	uint32_t Rm = SH4_GetGpr(rm);
	SH4_SetFPSCR(Rm);
	fprintf(stderr,"sh4_lds_rmfpscr not tested\n");
}

void
sh4_lds_armpfpscr(void)
{
	int rm = (ICODE >> 8) & 0xf;
	uint32_t Rm = SH4_GetGpr(rm);
	SH4_SetFPSCR(SH4_MMURead32(Rm));
	Rm += 4;
	SH4_SetGpr(Rm,rm);
	fprintf(stderr,"sh4_lds_rmpfpscr not tested\n");
}

/**
 *******************************************************************
 * \fn void sh4_lds_rmmach(void)
 * Load Rm to System Register MacH.
 * v1
 *******************************************************************
 */
void
sh4_lds_rmmach(void)
{
	int rm = (ICODE >> 8) & 0xf;
	uint32_t Rm = SH4_GetGpr(rm);	
	SH4_SetMacH(Rm);	
}

/**
 *************************************************************
 * \fn void sh4_lds_rmmacl(void)
 * Load Rm to System Register MacL
 * v1
 *************************************************************
 */
void
sh4_lds_rmmacl(void)
{
	int rm = (ICODE >> 8) & 0xf;
	uint32_t Rm = SH4_GetGpr(rm);	
	SH4_SetMacL(Rm);	
}

/**
 *******************************************************
 * \fn void sh4_lds_rmpr(void)
 * Load Register Rm to System Register PR
 * v1
 *******************************************************
 */
void
sh4_lds_rmpr(void)
{
	int rm = (ICODE >> 8) & 0xf;
	uint32_t Rm = SH4_GetGpr(rm);	
	SH4_SetPR(Rm);	
}

/**
 ********************************************************
 * \fn void sh4_ldsatrmpmach(void)
 * Load value at memory address Rm to register MacH.
 * Post increment Rm.
 * v1 
 ********************************************************
 */
void
sh4_lds_atrmpmach(void)
{
	int rm = (ICODE >> 8) & 0xf;
	uint32_t Rm = SH4_GetGpr(rm);	
	uint32_t value = SH4_MMURead32(Rm);
	SH4_SetMacH(value);	
	Rm += 4;
	SH4_SetGpr(Rm,rm);	
}

/**
 ********************************************************
 * \fn void sh4_ldsarmpmacl(void)
 * Load value at memory address Rm to register MacL.
 * Post increment Rm.
 * v1
 ********************************************************
 */
void
sh4_lds_atrmpmacl(void)
{
	int rm = (ICODE >> 8) & 0xf;
	uint32_t Rm = SH4_GetGpr(rm);	
	uint32_t value = SH4_MMURead32(Rm);
	SH4_SetMacL(value);	
	Rm += 4;
	SH4_SetGpr(Rm,rm);	
}

/**
 ********************************************************
 * \fn void sh4_ldsarmppr(void)
 * Load value at memory address Rm to register PR.
 * Post increment Rm.
 * v1
 ********************************************************
 */
void
sh4_lds_atrmppr(void)
{
	int rm = (ICODE >> 8) & 0xf;
	uint32_t Rm = SH4_GetGpr(rm);	
	uint32_t value = SH4_MMURead32(Rm);
	SH4_SetPR(value);	
	Rm += 4;
	SH4_SetGpr(Rm,rm);	
}

/*
 *********************************************************************
 * Load the TLB register
 *********************************************************************
 */
void
sh4_ldtlb(void)
{
	if(SH4_GetSR() & SR_MD) {

	} else {
		SH4_Exception(EX_RESINST);
	}
	fprintf(stderr,"sh4_ldtlb not implemented\n");
}

/*
 ****************************************************************+
 * \fn void sh4_macl(void)
 * Signed multiply and accumulate long.
 ****************************************************************+
 */
void
sh4_macl(void)
{
	int rn,rm;
	uint32_t Rn,Rm;
	int32_t M1,M2; 
	int64_t mac = SH4_GetMac();
	int64_t plus;
	int64_t result;
	int S = SH4_GetS();
	rn = (ICODE >> 8) & 0xf;
	rm = (ICODE >> 4) & 0xf;
	Rn = SH4_GetGpr(rn);	
	Rm = SH4_GetGpr(rm);	
	M1 = SH4_MMURead32(Rn);
	M2 = SH4_MMURead32(Rm);
	plus = (int64_t)M1 * (int64_t)M2;
	if(S) {
		int64_t max = UINT64_C(0x00007fffFFFFffff);
		int64_t min = UINT64_C(0xFFFF800000000000); 
		if(mac < 0) {
			result = plus + (mac | min);
		} else {
			result = plus + (mac & max);
		}
		if(result < min) {
			result = min;
		} else if(result > max) {
			result = max;
		}
	} else {
		result = mac + plus;
	}
	SH4_SetMac(mac);
}

/**
 ***********************************************************
 * \fn void sh4_macw(void)
 * Multiply and accumulate signed word.
 ***********************************************************
 */
void
sh4_macw(void)
{
	int rm,rn;
	uint32_t S = SH4_GetS();
	uint32_t Rn,Rm;
	int16_t M1,M2; 
	rn = (ICODE >> 8) & 0xf;
	rm = (ICODE >> 4) & 0xf;
	Rn = SH4_GetGpr(rn);	
	Rm = SH4_GetGpr(rm);	
	M1 = SH4_MMURead16(Rn);
	M2 = SH4_MMURead16(Rm);
	SH4_SetGpr(Rn + 2, rn); 
	SH4_SetGpr(Rm + 2, rm);	
	if(S == 0) {
		int64_t mac = SH4_GetMac();
		mac += M1 * M2;
		SH4_SetMac(mac);
	} else {
		int64_t mac = (int64_t)(int32_t)SH4_GetMacL();
		uint32_t mach;
		mac += M1 * M2;	
		if(mac > (int64_t)0x7FffFFff) {
			mach = SH4_GetMacH();
			SH4_SetMacH(mach | 1);
			mac = 0x7FffFFff;
		} else if (mac < (int64_t)(int32_t)0x80000000) {
			mach = SH4_GetMacH();
			SH4_SetMacH(mach | 1);
			mac = 0x80000000;
		}
		SH4_SetMacL(mac);
	}
}

/**
 ***************************************************
 * \fn void sh4_mov_rmrn(void)
 * Copy register value Rm to register rn.
 * v1
 ***************************************************
 */
void
sh4_mov_rmrn(void)
{
	int rm,rn;
	uint32_t Rm;
	rn = (ICODE >> 8) & 0xf;
	rm = (ICODE >> 4) & 0xf;
	Rm = SH4_GetGpr(rm);	
	SH4_SetGpr(Rm,rn);	
}

/**
 ********************************************************
 * \fn void sh4_movb_rmarn(void)
 * Move lowest byte of Register Rm into memory address
 * given by register Rn. 
 * v1
 ********************************************************
 */
void
sh4_movb_rmarn(void)
{
	int rm,rn;
	uint32_t Rn,Rm;
	rn = (ICODE >> 8) & 0xf;
	rm = (ICODE >> 4) & 0xf;
	Rm = SH4_GetGpr(rm);	
	Rn = SH4_GetGpr(rn);
	SH4_MMUWrite8(Rm,Rn);
}

/**
 ****************************************************************
 * \fn void sh4_movw_rmarn(void)
 * Move lowest 16 Bit in Register Rm into memory. The memory
 * address is given by register Rn.  
 * v1
 ****************************************************************
 */
void
sh4_movw_rmarn(void)
{
	int rm,rn;
	uint32_t Rn,Rm;
	rn = (ICODE >> 8) & 0xf;
	rm = (ICODE >> 4) & 0xf;
	Rm = SH4_GetGpr(rm);	
	Rn = SH4_GetGpr(rn);
	SH4_MMUWrite16(Rm,Rn);
}

/**
 ***************************************************************
 * \fn void sh4_movl_rmarn(void)
 * Move the value in Register Rm into memory. The memory
 * address is given by register Rn.  
 * v1
 ***************************************************************
 */
void
sh4_movl_rmarn(void)
{
	int rm,rn;
	uint32_t Rn,Rm;
	rn = (ICODE >> 8) & 0xf;
	rm = (ICODE >> 4) & 0xf;
	Rm = SH4_GetGpr(rm);	
	Rn = SH4_GetGpr(rn);
	SH4_MMUWrite32(Rm,Rn);
}

/**
 ******************************************************************
 * void sh4_movb_armrn(void)
 * Move a byte from memory address given by Rm into register Rn
 * v1
 ******************************************************************
 */
void
sh4_movb_armrn(void)
{
	int rm,rn;
	uint32_t Rm;
	rn = (ICODE >> 8) & 0xf;
	rm = (ICODE >> 4) & 0xf;
	Rm = SH4_GetGpr(rm);	
	SH4_SetGpr((int32_t)(int8_t)SH4_MMURead8(Rm),rn);
}

/**
 ***************************************************************
 * \fn void sh4_movw_armrn(void)
 * Move a word from memory address given by Rm into register Rn 
 * v1
 ***************************************************************
 */
void
sh4_movw_armrn(void)
{
	int rm,rn;
	uint32_t Rm;
	rn = (ICODE >> 8) & 0xf;
	rm = (ICODE >> 4) & 0xf;
	Rm = SH4_GetGpr(rm);	
	SH4_SetGpr((int32_t)(int16_t)SH4_MMURead16(Rm),rn);
}

/**
 *************************************************************
 * \fn void sh4_movl_armrn(void)
 * Move a 32 Bit value from memory address given by Rm into
 * register Rn.
 * v1
 *************************************************************
 */
void
sh4_movl_armrn(void)
{
	int rm,rn;
	uint32_t Rm;
	rn = (ICODE >> 8) & 0xf;
	rm = (ICODE >> 4) & 0xf;
	Rm = SH4_GetGpr(rm);	
	SH4_SetGpr(SH4_MMURead32(Rm),rn);
}

/**
 *****************************************************************
 * \fn void sh4_movb_rmamrn(void)
 * Move a Byte from Register Rm to the address given by register Rn.
 * The Rn register is pre-decremented by one.
 * v0
 *****************************************************************
 */
void
sh4_movb_rmamrn(void)
{
	int rm,rn;
	uint32_t Rn,Rm;
	rn = (ICODE >> 8) & 0xf;
	rm = (ICODE >> 4) & 0xf;
	Rn = SH4_GetGpr(rn);	
	Rm = SH4_GetGpr(rm);
	Rn--;
	/*********************************************************** 
	 * This order gives a base restored exception model.
	 * I have not verified that real CPU uses base restored. 
	 ***********************************************************/
	SH4_MMUWrite8(Rm,Rn);
	SH4_SetGpr(Rn,rn);	
}

/**
 ******************************************************************
 * \fn void sh4_movw_rmamrn(void)
 * Move a word from Register Rm to the address given by register Rn.
 * The Rn register is pre-decremented by two.
 * v0
 ******************************************************************
 */
void
sh4_movw_rmamrn(void)
{
	int rm,rn;
	uint32_t Rn,Rm;
	rn = (ICODE >> 8) & 0xf;
	rm = (ICODE >> 4) & 0xf;
	Rn = SH4_GetGpr(rn);	
	Rm = SH4_GetGpr(rm);
	Rn -= 2;
	/*********************************************************** 
	 * This order gives a base restored exception model.
	 * I have not verified that real CPU uses base restored. 
	 ***********************************************************/
	SH4_MMUWrite16(Rm,Rn);
	SH4_SetGpr(Rn,rn);	
}

/**
 *******************************************************************
 * \fn void sh4_movl_rmamrn(void)
 * Move a 32 Bit value from Register Rm to the address 
 * given by register Rn. The Rn register is pre-decremented by four.
 * v0
 *******************************************************************
 */
void
sh4_movl_rmamrn(void)
{
	int rm,rn;
	uint32_t Rn,Rm;
	rn = (ICODE >> 8) & 0xf;
	rm = (ICODE >> 4) & 0xf;
	Rn = SH4_GetGpr(rn);	
	Rm = SH4_GetGpr(rm);
	Rn -= 4;
	/*********************************************************** 
	 * This order gives a base restored exception model.
	 * I have not verified that real CPU uses base restored. 
	 ***********************************************************/
	SH4_MMUWrite32(Rm,Rn);
	SH4_SetGpr(Rn,rn);	
}

/**
 *******************************************************************
 * \fn void sh4_movb_armprn(void)
 * Move a byte from the address given by Rm to register Rn.
 * Use sign extension and post increment Rm.
 * v0
 *******************************************************************
 */
void
sh4_movb_armprn(void)
{
	int rm,rn;
	uint32_t Rn,Rm;
	rn = (ICODE >> 8) & 0xf;
	rm = (ICODE >> 4) & 0xf;
	Rm = SH4_GetGpr(rm);
	/* I hope real cpu uses base restored exception model */
	Rn = (int32_t)(int8_t)SH4_MMURead8(Rm); 
	SH4_SetGpr(Rm + 1,rm);	
	/* Write Rn past Rm because of possible rm == rn */
	SH4_SetGpr(Rn,rn);	
}

/**
 *******************************************************************
 * \fn void sh4_movw_armprn(void)
 * Move a word from the address given by Rm to register Rn.
 * Use sign extension and post increment Rm by two.
 * v0
 *******************************************************************
 */
void
sh4_movw_armprn(void)
{
	int rm,rn;
	uint32_t Rn,Rm;
	rn = (ICODE >> 8) & 0xf;
	rm = (ICODE >> 4) & 0xf;
	Rm = SH4_GetGpr(rm);
	/* I hope real cpu uses base restored exception model */
	Rn = (int32_t)(int16_t)SH4_MMURead16(Rm); 
	SH4_SetGpr(Rm + 2,rm);	
	/* Write Rn past Rm because of possible rm == rn */
	SH4_SetGpr(Rn,rn);	
}

/**
 *******************************************************************
 * \fn void sh4_movl_armprn(void)
 * Move a 32 bit value from the address given by Rm to register Rn.
 * Use sign extension and post increment Rm by four.
 * v0
 *******************************************************************
 */
void
sh4_movl_armprn(void)
{
	int rm,rn;
	uint32_t Rn,Rm;
	rn = (ICODE >> 8) & 0xf;
	rm = (ICODE >> 4) & 0xf;
	Rm = SH4_GetGpr(rm);
	/* I hope real cpu uses base restored exception model */
	Rn = SH4_MMURead32(Rm); 
	SH4_SetGpr(Rm + 4,rm);	
	/* Write Rn past Rm because of possible rm == rn */
	SH4_SetGpr(Rn,rn);	
}

/**
 *******************************************************************
 * \fn void sh4_movb_rmar0rn(void)
 * Move low byte in register Rm to address calculated as sum
 * of R0 and Rn.
 * v1
 *******************************************************************
 */
void
sh4_movb_rmar0rn(void)
{
	int rm,rn;
	uint32_t Rn,Rm,R0;
	rn = (ICODE >> 8) & 0xf;
	rm = (ICODE >> 4) & 0xf;
	Rm = SH4_GetGpr(rm);
	Rn = SH4_GetGpr(rn); 
	R0 = SH4_GetGpr(0);
	SH4_MMUWrite8(Rm,R0 + Rn);
}

/**
 *********************************************************************
 * \fn void sh4_movw_rmar0rn(void)
 * Move low word in register Rm to address calculated as sum
 * of R0 and Rn.
 * v1
 *********************************************************************
 */
void
sh4_movw_rmar0rn(void)
{
	int rm,rn;
	uint32_t Rn,Rm,R0;
	rn = (ICODE >> 8) & 0xf;
	rm = (ICODE >> 4) & 0xf;
	Rm = SH4_GetGpr(rm);
	Rn = SH4_GetGpr(rn); 
	R0 = SH4_GetGpr(0);
	SH4_MMUWrite16(Rm,R0 + Rn);
}

/**
 **************************************************************
 * \fn void sh4_movl_rmar0rn(void)
 * Move value in register Rm to address calculated as sum
 * of R0 and Rn.
 * v1
 **************************************************************
 */
void
sh4_movl_rmar0rn(void)
{
	int rm,rn;
	uint32_t Rn,Rm,R0;
	rn = (ICODE >> 8) & 0xf;
	rm = (ICODE >> 4) & 0xf;
	Rm = SH4_GetGpr(rm);
	Rn = SH4_GetGpr(rn); 
	R0 = SH4_GetGpr(0);
	SH4_MMUWrite32(Rm,R0 + Rn);
}

/**
 *******************************************************************
 * \fn void sh4_movb_ar0rmrn(void)
 * Move byte from address calculated by R0+Rm to Register Rn.
 * Use sign extension.
 * v1
 *******************************************************************
 */
void
sh4_movb_ar0rmrn(void)
{
	int rm,rn;
	uint32_t Rn,Rm,R0;
	rn = (ICODE >> 8) & 0xf;
	rm = (ICODE >> 4) & 0xf;
	Rm = SH4_GetGpr(rm);
	R0 = SH4_GetGpr(0);
	Rn = (int32_t)(int8_t)SH4_MMURead8(R0 + Rm);
	SH4_SetGpr(Rn,rn); 
}

/**
 ******************************************************************
 * \fn void sh4_movw_ar0rmrn(void)
 * Move word from address calculated by R0+Rm to Register Rn.
 * Use sign extension.
 * v1
 ******************************************************************
 */
void
sh4_movw_ar0rmrn(void)
{
	int rm,rn;
	uint32_t Rn,Rm,R0;
	rn = (ICODE >> 8) & 0xf;
	rm = (ICODE >> 4) & 0xf;
	Rm = SH4_GetGpr(rm);
	R0 = SH4_GetGpr(0);
	Rn = (int32_t)(int16_t)SH4_MMURead16(R0 + Rm);
	SH4_SetGpr(Rn,rn); 
}

/**
 *****************************************************************
 * \fn void sh4_movl_ar0rmrn(void)
 * Move value at address calculated by R0+Rm to register Rn.
 * v1
 *****************************************************************
 */
void
sh4_movl_ar0rmrn(void)
{
	int rm,rn;
	uint32_t Rn,Rm,R0;
	rn = (ICODE >> 8) & 0xf;
	rm = (ICODE >> 4) & 0xf;
	Rm = SH4_GetGpr(rm);
	R0 = SH4_GetGpr(0);
	Rn = SH4_MMURead32(R0 + Rm);
	SH4_SetGpr(Rn,rn); 
}

/**
 ***************************************************************
 * \fn void sh4_mov_immrn(void)
 * Move an immediate value to a register with sign extension.
 * v1
 ***************************************************************
 */
void
sh4_mov_immrn(void)
{
	int rn = (ICODE >> 8) & 0xf;
	int8_t imm = ICODE & 0xff;
	SH4_SetGpr((int32_t)(int8_t)imm,rn);
}

/**
 *******************************************************************
 * \fn void void sh4_movw_adisppcrn(void)
 * Move a 16 Bit value from a memory address relative to PC
 * into a register. The 16 Bit value is sign extended. The
 * displacement is zero extended.
 *******************************************************************
 */
void
sh4_movw_adisppcrn(void)
{
	int rn = (ICODE >> 8) & 0xf;
	uint32_t disp = ICODE & 0xff;
	uint32_t addr = SH4_NNIA + (disp << 1);
	uint32_t Rn = (int32_t)(int16_t)SH4_MMURead16(addr);

	SH4_SetGpr(Rn,rn);
}

/**
 ****************************************************************
 * \fn void sh4_movl_adisppcrn(void)
 * Move a 32 Bit value from a memory address relative to PC
 * into a register. The displacement is zero extend.
 ****************************************************************
 */
void
sh4_movl_adisppcrn(void)
{
	int rn = (ICODE >> 8) & 0xf;
	uint32_t disp = ICODE & 0xff;
	uint32_t addr = (SH4_NNIA & ~UINT32_C(3)) + (disp << 2);
	uint32_t Rn = SH4_MMURead32(addr);
	SH4_SetGpr(Rn,rn);
#if 0
	fprintf(stderr,"movl_adisppcrn disp %02x,nnia %08x,addr %08x,Rn %08x\n",
		disp,SH4_NNIA,addr,Rn);
#endif
}

/**
 **************************************************************
 * \fn void sh4_movb_adispgbrr0(void)
 * Move a byte from a memory address relative to GBR 
 * into Register R0. The displacement is zero extended.
 * The byte from memory is sign extended.
 **************************************************************
 */
void
sh4_movb_adispgbrr0(void)
{
	uint32_t Gbr = SH4_GetGBR();
	uint32_t disp = ICODE & 0xff;
	uint32_t R0;
	R0 = (int32_t)(int8_t)SH4_MMURead8(Gbr + disp);
	SH4_SetGpr(R0,0);
}

/**
 **************************************************************
 * \fn void sh4_movw_adispgbrr0(void)
 * Move a word from a memory address relative to GBR 
 * into Register R0. The displacement is zero extended.
 * The word from memory is sign extended.
 * v1
 **************************************************************
 */
void
sh4_movw_adispgbrr0(void)
{
	uint32_t Gbr = SH4_GetGBR();
	uint32_t disp = ICODE & 0xff;
	uint32_t R0;
	R0 = (int32_t)(int16_t)SH4_MMURead16(Gbr + (disp << 1));
	SH4_SetGpr(R0,0);
}

/**
 **************************************************************
 * \fn void sh4_movl_adispgbrr0(void)
 * Move a 32 Bit value from a memory address relative to GBR 
 * into Register R0. The displacement is zero extended.
 **************************************************************
 */
void
sh4_movl_adispgbrr0(void)
{
	uint32_t Gbr = SH4_GetGBR();
	uint32_t disp = ICODE & 0xff;
	uint32_t R0;
	R0 = SH4_MMURead32(Gbr + (disp << 2));
	SH4_SetGpr(R0,0);
}

/**
 *************************************************************
 * \fn void sh4_movb_r0adispgbr(void)
 * Move the low byte from Register R0 to a memory address
 * with a displacement relative to GBR. 
 *************************************************************
 */
void
sh4_movb_r0adispgbr(void)
{
	uint32_t Gbr = SH4_GetGBR();
	uint32_t R0 = SH4_GetGpr(0);
	uint32_t disp = ICODE & 0xff;
	SH4_MMUWrite8(R0,Gbr + disp);
}

/**
 **************************************************************
 * \fn void sh4_movw_r0adispgbr(void)
 * Move the low word from Register R0 to a memory address
 * with a displacement relative to GBR.
 **************************************************************
 */
void
sh4_movw_r0adispgbr(void)
{
	uint32_t Gbr = SH4_GetGBR();
	uint32_t R0 = SH4_GetGpr(0);
	uint32_t disp = ICODE & 0xff;
	SH4_MMUWrite16(R0,Gbr + (disp << 1));
}

/**
 ************************************************************
 * \fn void sh4_movl_r0adispgbr(void)
 * Move the value of  Register R0 to a memory address
 * with a displacement relative to GBR.
 * v1
 ************************************************************
 */
void
sh4_movl_r0adispgbr(void)
{
	uint32_t Gbr = SH4_GetGBR();
	uint32_t R0 = SH4_GetGpr(0);
	uint32_t disp = ICODE & 0xff;
	SH4_MMUWrite32(R0,Gbr + (disp << 2));
}

/**
 *************************************************************
 * \fn void sh4_movb_r0adisprn(void)
 * Move low byte in Register R0 to the address calculated
 * by R0 + displacement. 
 * v1
 *************************************************************
 */
void
sh4_movb_r0adisprn(void)
{
	uint32_t R0 = SH4_GetGpr(0);
	uint32_t disp = ICODE & 0xf;
	int rn = (ICODE >> 4) & 0xf;
	uint32_t Rn = SH4_GetGpr(rn);
	SH4_MMUWrite8(R0,Rn + disp);	
}

/*
 **************************************************************
 * \fn void sh4_movw_r0adisprn(void)
 * Move low word of Register R0 to the address calculated
 * by R0 + displacement. 
 * v1
 *************************************************************
 */
void
sh4_movw_r0adisprn(void)
{
	uint32_t R0 = SH4_GetGpr(0);
	uint32_t disp = ICODE & 0xf;
	int rn = (ICODE >> 4) & 0xf;
	uint32_t Rn = SH4_GetGpr(rn);
	SH4_MMUWrite16(R0,Rn + (disp << 1));	
}

/**
 *************************************************************
 * \fn void sh4_movl_rmadisprn(void)
 * Move the value in Register Rm to the address calculated
 * by Rm + displacement. 
 * v1
 *************************************************************
 */
void
sh4_movl_rmadisprn(void)
{
	uint32_t disp = ICODE & 0xf;
	int rn = (ICODE >> 8) & 0xf;
	int rm = (ICODE >> 4) & 0xf;
	uint32_t Rn;
	uint32_t Rm;
	Rn = SH4_GetGpr(rn);
	Rm = SH4_GetGpr(rm);
	SH4_MMUWrite32(Rm,Rn + (disp << 2));	
}

/**
 ***********************************************************************
 * \fn void sh4_movb_adisprmr0(void)
 * Move a byte from an address calculated as sum of a displacement and
 * the register value Rm to the Register R0. Do a sign extension 
 * of the byte from memory.
 * v1
 ***********************************************************************
 */
void
sh4_movb_adisprmr0(void)
{
	uint32_t disp = ICODE & 0xf;
	int rm = (ICODE >> 4) & 0xf;
	uint32_t Rm = SH4_GetGpr(rm);
	uint32_t R0;
	R0 = (int32_t)(int8_t)SH4_MMURead8(Rm + disp);
	SH4_SetGpr(R0,0);	
}

/**
 ****************************************************************************
 * \fn void sh4_movw_adisprmr0(void)
 * Move a word from an address calculated as sum of a displacement and
 * the register value Rm to the register R0. Do a sign extension
 * of the word from memory.
 * v1
 ****************************************************************************
 */

void
sh4_movw_adisprmr0(void)
{
	uint32_t disp = ICODE & 0xf;
	int rm = (ICODE >> 4) & 0xf;
	uint32_t Rm = SH4_GetGpr(rm);
	uint32_t R0;
	R0 = (int32_t)(int16_t)SH4_MMURead16(Rm + (disp << 1));
	SH4_SetGpr(R0,0);	
}

/**
 ****************************************************************************
 * \fn void sh4_movl_adisprmrn(void)
 * Move a long from an address calculated as the sum of a displacement
 * and the register value RM to the Register Rn.
 * v1
 ****************************************************************************
 */
void
sh4_movl_adisprmrn(void)
{
	uint32_t disp = ICODE & 0xf;
	int rm = (ICODE >> 4) & 0xf;
	int rn = (ICODE >> 8) & 0xf;
	uint32_t Rm = SH4_GetGpr(rm);
	uint32_t Rn;
	Rn = SH4_MMURead32(Rm + (disp << 2));
	SH4_SetGpr(Rn,rn);	
}

/**
 **************************************************************************
 * \fn void sh4_mova_adisppcr0(void)
 * Calculate an address by adding the programm counter and a displacement. 
 * Store the result in register R0.
 * v1
 **************************************************************************
 */
void
sh4_mova_adisppcr0(void)
{
	uint32_t disp = ICODE & 0xff;	
	uint32_t addr;
	addr = (SH4_NNIA & ~UINT32_C(3)) + (disp << 2);
	SH4_SetGpr(addr,0);	
}

/** 
 ***********************************************************
 * \fn void sh4_movcal(void)
 * Move with cache block allocation.
 * Write the value of register R0 to a memory location
 * indicated by Register Rn.
 * Should zero some (cache-) memory to be more realistic.
 * v1
 ***********************************************************
 */
void
sh4_movcal(void)
{
	int rn;
	uint32_t R0,Rn;
	rn = (ICODE >> 8) & 0xf;
	R0 = SH4_GetGpr(0);
	Rn = SH4_GetGpr(rn);
	SH4_MMUWrite32(R0,Rn);
}

/**
 *******************************************************
 * \fn void sh4_movt(void)
 * Move the T Bit into Gpr Rn.
 * v1
 *******************************************************
 */
void
sh4_movt(void)
{
	int rn = (ICODE >> 8) & 0xf;
	SH4_SetGpr(SH4_GetTrue(),rn);
}

/**
 *******************************************************
 * \fn void sh4_mull(void)
 * Double precission multiplication.
 * v1
 *******************************************************
 */
void
sh4_mull(void)
{
	int rn = (ICODE >> 8) & 0xf;
	int rm = (ICODE >> 4) & 0xf;
	uint32_t Rm,Rn;
	uint32_t macl;
	Rm = SH4_GetGpr(rm);
	Rn = SH4_GetGpr(rn);
	macl = Rm * Rn;
	SH4_SetMacL(macl);	
}

/**
 **************************************************************
 * \fn void sh4_mulsw(void)
 * Multiply signed words and store the result in 32 Bit MacL.
 * v1
 **************************************************************
 */
void
sh4_mulsw(void)
{
	int rn = (ICODE >> 8) & 0xf;
	int rm = (ICODE >> 4) & 0xf;
	uint32_t Rm,Rn;
	int32_t macl;
	Rm = SH4_GetGpr(rm);
	Rn = SH4_GetGpr(rn);
	macl = (int32_t)(int16_t)Rm * (int32_t)(int16_t)Rn;
	SH4_SetMacL(macl);	
}

/**
 ***************************************************************
 * \fn void sh4_muluw(void)
 * Multiply unsigned words and store the result in 32 Bit MacL
 * v1
 ***************************************************************
 */
void
sh4_muluw(void)
{
	int rn = (ICODE >> 8) & 0xf;
	int rm = (ICODE >> 4) & 0xf;
	uint32_t Rm,Rn;
	uint32_t macl;
	Rm = SH4_GetGpr(rm);
	Rn = SH4_GetGpr(rn);
	macl = (uint32_t)(uint16_t)Rm * (uint32_t)(uint16_t)Rn;
	SH4_SetMacL(macl);	
}

/**
 ******************************************************************
 * \fn void sh4_neg(void)
 * Mov the sign inverted value of register Rm to register Rn.  
 * v1
 ******************************************************************
 */
void
sh4_neg(void)
{
	int rn = (ICODE >> 8) & 0xf;
	int rm = (ICODE >> 4) & 0xf;
	uint32_t Rm,Rn;
	Rm = SH4_GetGpr(rm);
	Rn = 0 - Rm;
	SH4_SetGpr(Rn,rn);	
}

/**
 *****************************************************************
 * \fn void sh4_negc(void)
 * Negate with carry.
 *****************************************************************
 */
void
sh4_negc(void)
{
	int rn = (ICODE >> 8) & 0xf;
	int rm = (ICODE >> 4) & 0xf;
	int32_t Rm,Rn;
	uint32_t temp;
	Rm = SH4_GetGpr(rm);
	temp = 0 - Rm;
	Rn = temp - SH4_GetTrue();
	if((temp > 0) || (temp < Rn)) {
		SH4_SetTrue(1);
	} else {
		SH4_SetTrue(0);
	}
	SH4_SetGpr(Rn,rn);
}

/**
 ****************************************************
 * \fn void sh4_nop(void)
 * Eat up time.
 * v2
 ****************************************************
 */ 
void
sh4_nop(void)
{
}

/**
 *********************************************************+
 * \fn void sh4_not(void)
 * Calculate one's complement of register Rm and store
 * the result in Rn.
 * v1
 *********************************************************+
 */
void
sh4_not(void)
{
	int rn = (ICODE >> 8) & 0xf;
	int rm = (ICODE >> 4) & 0xf;
	uint32_t Rm,Rn;
	Rm = SH4_GetGpr(rm);
	Rn = ~Rm;
	SH4_SetGpr(Rn,rn);	
}

/*
 **************************************************************
 * Cache block operations
 **************************************************************
 */
void
sh4_ocbi(void)
{
	fprintf(stderr,"sh4_ocbi not implemented\n");
}

void
sh4_ocbp(void)
{
	fprintf(stderr,"sh4_ocbp not implemented\n");
}

void
sh4_ocbwb(void)
{
	fprintf(stderr,"sh4_ocbwb not implemented\n");
}

/**
 *********************************************************
 * \fn void sh4_or_rmrn(void)
 * Calculate the logical "OR" of Rm and Rn and store
 * the result in Rn.
 *********************************************************
 */
void
sh4_or_rmrn(void)
{
	int rn = (ICODE >> 8) & 0xf;
	int rm = (ICODE >> 4) & 0xf;
	uint32_t Rm,Rn;
	Rm = SH4_GetGpr(rm);
	Rn = SH4_GetGpr(rn);
	Rn = Rm | Rn;
	SH4_SetGpr(Rn,rn);
}

/**
 *********************************************************
 * \fn void sh4_or_immr0(void)
 * Calculate the logical "OR" of register R0 and an
 * immediate and store the result in register R0. 
 *********************************************************
 */
void
sh4_or_immr0(void)
{
	uint32_t imm = ICODE & 0xff;
	uint32_t R0 = SH4_GetGpr(0);
	SH4_SetGpr(R0 | imm,0);
}

/**
 *******************************************************
 * \fn void sh4_or_immar0gbr(void)
 * Calculate the logical or of an immediate and a  
 * byte in memory and write back the result.
 * v1
 *******************************************************
 */

void
sh4_or_immar0gbr(void)
{
	uint32_t imm = ICODE & 0xff;
	uint32_t R0 = SH4_GetGpr(0);
	uint32_t Gbr = SH4_GetGBR();
	uint32_t addr = Gbr + R0;
	SH4_MMUWrite8(SH4_MMURead8(addr) | imm,addr);
}

/*
 * Prefetch does nothing because cache is currently not
 * emulated
 */
void
sh4_pref(void)
{
	fprintf(stderr,"sh4_pref not implemented\n");
}

/** 
 *************************************************************
 * \fn void sh4_rotcl(void)
 * Rotate with carry left.
 * v1
 *************************************************************
 */
void
sh4_rotcl(void)
{
	int rn = (ICODE >> 8) & 0xf;
	uint32_t Rn = SH4_GetGpr(rn);
	int T = SH4_GetTrue();
	SH4_SetTrue(Rn >> 31);
	SH4_SetGpr((Rn << 1) | T, rn);
}

/**
 **************************************************************
 * \fn void sh4_rotcr(void)
 * Rotate with carry right.
 **************************************************************
 */
void
sh4_rotcr(void)
{
	int rn = (ICODE >> 8) & 0xf;
	uint32_t Rn = SH4_GetGpr(rn);
	uint32_t T = SH4_GetTrue();
	SH4_SetTrue(Rn & 1);
	SH4_SetGpr((Rn >> 1) | (T << 31), rn);
}

/**
 ***************************************************************
 * \fn void sh4_rotl(void)
 * Rotate left without carry. The bit reinserted at the 
 * end is copied int the T flag.
 * v1
 ***************************************************************
 */
void
sh4_rotl(void)
{
	int rn = (ICODE >> 8) & 0xf;
	uint32_t Rn = SH4_GetGpr(rn);
	uint32_t T = Rn >> 31;
	SH4_SetGpr((Rn << 1) | T, rn);
	SH4_SetTrue(T);
}

/**
 **************************************************************
 * \fn void sh4_rotr(void)
 * Rotate right without carry. The bit reinserted at the
 * left side is copied to the T flag.
 * v1
 **************************************************************
 */
void
sh4_rotr(void)
{
	int rn = (ICODE >> 8) & 0xf;
	uint32_t Rn = SH4_GetGpr(rn);
	uint32_t T = Rn & 1;
	SH4_SetGpr((Rn >> 1) | (T << 31), rn);
	SH4_SetTrue(T);
}

/**
 ************************************************************
 * \fn void sh4_rte(void)
 * Returns from an exception by restoring th pc and SR
 * by restoring the values from SPC and SSR.
 * What happens if ILLSLOT and RESINST exception happen
 * at the same time ?
 * Instruction fetch is done with old SR values !, 
 * Special variant of execution of delay slot !
 * v1
 ************************************************************
 */
void
sh4_rte(void)
{
        SH4_InstructionProc *iproc;

	if(SH4_GetSR() & SR_MD) {
		CycleCounter+=2;
		CycleTimers_Check();
		ICODE = SH4_MMURead16(SH4_GetRegPC());

		SH4_SetSR(SH4_GetSSR());	
		SH4_SetRegPC(SH4_GetSPC());

		iproc = SH4_InstructionProcFind(ICODE);
		iproc();

	} else {
		SH4_Exception(EX_RESINST);
	}
}

/**
 *************************************************************
 * \fn void sh4_rts(void);
 * Return from subroutine by copying the PR register into 
 * the PC.
 * v1
 *************************************************************
 */
void
sh4_rts(void)
{
	SH4_ExecuteDelaySlot();
	SH4_SetRegPC(SH4_GetPR());
}

/**
 *************************************************************
 * \fn void sh4_sets(void)
 * Set S-Flag
 * v1
 *************************************************************
 */
void
sh4_sets(void)
{
	SH4_SetS(1);
}

/**
 *************************************************************
 * \fn void sh4_sett(void)
 * Set T bit
 * v1
 *************************************************************
 */
void
sh4_sett(void)
{
	SH4_SetTrue(1);
}

/*
 *************************************************************
 * \fn void sh4_shad(void)
 * Shift Arithmetic dynamically. (Signed).
 * v1
 *************************************************************
 */
void
sh4_shad(void)
{
	int rm = (ICODE >> 4) & 0xf;
	int rn = (ICODE >> 8) & 0xf;
	int shift;
	int32_t Rm = SH4_GetGpr(rm);
	int32_t Rn = SH4_GetGpr(rn);;
	if(Rm >= 0) {
		shift = Rm & 0x1f;
		Rn <<= shift;
	} else {
		Rm = Rm | ~UINT32_C(0x1f);
		shift = - Rm;
		/* Shift > width of type special case */
		if(shift == 32) {
			if(Rn < 0) {
				Rn = ~UINT32_C(0);
			} else {
				Rn = 0;
			}
		} else {
			Rn >>= shift;
		}
	}
	SH4_SetGpr(Rn,rn);
}

/**
 *******************************************************************
 * \fn void sh4_shal(void)
 * Shift arithmetic left by one. 
 * v1
 *******************************************************************
 */
void
sh4_shal(void)
{
	int rn = (ICODE >> 8) & 0xf;
	int32_t Rn = SH4_GetGpr(rn);
	SH4_SetTrue(Rn >> 31);
	SH4_SetGpr(Rn << 1, rn);
}

/**
 ****************************************************************
 * \fn void sh4_shar(void)
 * Shift arithmetic right. 
 * v1
 ****************************************************************
 */
void
sh4_shar(void)
{
	int rn = (ICODE >> 8) & 0xf;
	int32_t Rn = SH4_GetGpr(rn);
	SH4_SetTrue(Rn & 1);
	SH4_SetGpr((Rn >> 1), rn);
}

/**
 *************************************************
 * \fn void sh4_shld(void)
 * Shift logically dynamically.
 * v1
 *************************************************
 */
void
sh4_shld(void)
{
	int shift;
	int rm = (ICODE >> 4) & 0xf;
	int rn = (ICODE >> 8) & 0xf;
	int32_t Rm = SH4_GetGpr(rm);
	uint32_t Rn = SH4_GetGpr(rn);;
	if(Rm >= 0) {
		shift = Rm & 0x1f;
		Rn <<= shift;
	} else {
		Rm = Rm | 0xFFFFffe0;
		shift = - Rm;
		/* Shift > width of type special case */
		if(shift == 32) {
			Rn = 0;
		} else {
			Rn >>= shift;
		}
	}
	SH4_SetGpr(Rn,rn);
}

/**
 ***************************************************************
 * \fn void sh4_shll(void)
 * Shift logical left.
 * v1
 ***************************************************************
 */
void
sh4_shll(void)
{
	int rn = (ICODE >> 8) & 0xf;
	uint32_t Rn = SH4_GetGpr(rn);
	SH4_SetTrue(Rn >> 31);
	SH4_SetGpr(Rn << 1, rn);
}

/**
 ***********************************************************
 * \fn void sh4_shll2(void)
 * Shift logical left by two
 * v1 
 ***********************************************************
 */
void
sh4_shll2(void)
{
	int rn = (ICODE >> 8) & 0xf;
	uint32_t Rn = SH4_GetGpr(rn);
	SH4_SetGpr(Rn << 2, rn);
}

/**
 *********************************************************
 * \fn void sh4_shll8(void)
 * Shift logical left by 8.
 * v1
 *********************************************************
 */
void
sh4_shll8(void)
{
	int rn = (ICODE >> 8) & 0xf;
	uint32_t Rn = SH4_GetGpr(rn);
	SH4_SetGpr(Rn << 8, rn);
}

/**
 **********************************************************
 * \fn void sh4_shll16(void)
 * Shift logical left by 16 .
 * v1
 **********************************************************
 */
void
sh4_shll16(void)
{
	int rn = (ICODE >> 8) & 0xf;
	uint32_t Rn = SH4_GetGpr(rn);
	SH4_SetGpr(Rn << 16, rn);
}

/**
 **************************************************************
 * \fn void sh4_shlr(void)
 * Shift logical right by one: Bit is shifted into T register.
 * v1
 **************************************************************
 */
void
sh4_shlr(void)
{
	int rn = (ICODE >> 8) & 0xf;
	uint32_t Rn = SH4_GetGpr(rn);
	SH4_SetTrue(Rn & 1);
	SH4_SetGpr(Rn >> 1, rn);
}

/**
 ************************************************************
 * \fn void sh4_shlr2(void)
 * Shift logical right by two.
 * v1
 ************************************************************
 */
void
sh4_shlr2(void)
{
	int rn = (ICODE >> 8) & 0xf;
	uint32_t Rn = SH4_GetGpr(rn);
	SH4_SetGpr(Rn >> 2, rn);
}

/**
 ************************************************************
 * \fn void sh4_shlr8(void)
 * Shift logical right by 8.
 * v1
 ************************************************************
 */
void
sh4_shlr8(void)
{
	int rn = (ICODE >> 8) & 0xf;
	uint32_t Rn = SH4_GetGpr(rn);
	SH4_SetGpr(Rn >> 8, rn);
}

/**
 ************************************************************
 * \fn void sh4_shlr16(void)
 * Shift logical right by 16.
 * v1
 ************************************************************
 */
void
sh4_shlr16(void)
{
	int rn = (ICODE >> 8) & 0xf;
	uint32_t Rn = SH4_GetGpr(rn);
	SH4_SetGpr(Rn >> 16, rn);
}

void
sh4_sleep(void)
{
	if(SH4_GetSR() & SR_MD) {

	} else {
		SH4_Exception(EX_RESINST);
	}
	fprintf(stderr,"sh4_sleep not implemented\n");
}

/**
 ********************************************************
 * \fn void sh4_stc_sr_rn(void)
 * Store from Control Register. Copy SR into Rn
 * v1 
 ********************************************************
 */
void
sh4_stc_sr_rn(void)
{
	int rn = (ICODE >> 8) & 0xf;
	uint32_t Sr;
	if(SH4_GetSR() & SR_MD) {
		Sr = SH4_GetSR();
		SH4_SetGpr(Sr,rn);
	} else {
		SH4_Exception(EX_RESINST);
	}
}

/**
 ********************************************************
 * \fn void sh4_stc_gbr_rn(void)
 * Store Gbr in Rn.
 * v1
 ********************************************************
 */
void
sh4_stc_gbr_rn(void)
{
	int rn = (ICODE >> 8) & 0xf;
	uint32_t Gbr = SH4_GetGBR();
	SH4_SetGpr(Gbr,rn);
}

/**
 *******************************************************
 * \fn void sh4_stc_vbr_rn(void)
 * Store VBR in Rn.
 * v1
 *******************************************************
 */
void
sh4_stc_vbr_rn(void)
{
	int rn = (ICODE >> 8) & 0xf;
	uint32_t Vbr;
	if(SH4_GetSR() & SR_MD) {
		Vbr = SH4_GetVBR();
		SH4_SetGpr(Vbr,rn);
	} else {
		SH4_Exception(EX_RESINST);
	}
}

/**
 *******************************************************
 * \fn void sh4_stc_ssr_rn(void)
 * Store SSR in Rn.
 * v1
 *******************************************************
 */
void
sh4_stc_ssr_rn(void)
{
	int rn = (ICODE >> 8) & 0xf;
	uint32_t Ssr;
	if(SH4_GetSR() & SR_MD) {
		Ssr = SH4_GetSSR();
		SH4_SetGpr(Ssr,rn);
	} else {
		SH4_Exception(EX_RESINST);
	}
}

/** 
 ********************************************************
 * \fn void sh4_stc_spc_rn(void)
 * Store SPC in Register Rn
 ********************************************************
 */
void
sh4_stc_spc_rn(void)
{
	int rn = (ICODE >> 8) & 0xf;
	uint32_t Spc;
	if(SH4_GetSR() & SR_MD) {
		Spc = SH4_GetSPC();
		SH4_SetGpr(Spc,rn);
	} else {
		SH4_Exception(EX_RESINST);
	}
}

/**
 ********************************************************
 * \fn void sh4_stc_sgr_rn(void)
 * Store SGR in Rn.
 * v1
 ********************************************************
 */
void
sh4_stc_sgr_rn(void)
{
	int rn = (ICODE >> 8) & 0xf;
	uint32_t Sgr;
	if(SH4_GetSR() & SR_MD) {
		Sgr = SH4_GetSGR();
		SH4_SetGpr(Sgr,rn);
	} else {
		SH4_Exception(EX_RESINST);
	}
}

/**
 *******************************************************
 * \fn void sh4_stc_dbr_rn(void)
 * Store DBR in Rn.
 * v1
 *******************************************************
 */
void
sh4_stc_dbr_rn(void)
{
	int rn = (ICODE >> 8) & 0xf;
	uint32_t Dbr;
	if(SH4_GetSR() & SR_MD) {
		Dbr = SH4_GetDBR();
		SH4_SetGpr(Dbr,rn);
	} else {
		SH4_Exception(EX_RESINST);
	}
}

/**
 *******************************************************
 * \fn void sh4_stc_dbr_rn(void)
 * Store banked Register into Rn.
 * v0
 *******************************************************
 */
void
sh4_stc_rb_rn(void)
{
	int rn = (ICODE >> 8) & 0xf;
	int rb = (ICODE >> 4) & 7;
	uint32_t R;
	if(SH4_GetSR() & SR_MD) {
		R = SH4_GetGprBank(rb);
		SH4_SetGpr(R,rn);
	} else {
		SH4_Exception(EX_RESINST);
	}
}

/**
 ********************************************************
 * \fn void sh4_stcl_sr_amrn(void)
 * Store Status register into address in Rn.
 * Rn is pre-decremented by four.
 * v0
 ********************************************************
 */
void
sh4_stcl_sr_amrn(void)
{
	int rn = (ICODE >> 8) & 0xf;
	uint32_t Sr;
	uint32_t Rn;
	Sr = SH4_GetSR();
	if(Sr & SR_MD) {
		Rn = SH4_GetGpr(rn);
		Rn -= 4;
		/* Base restored exception model */
		SH4_MMUWrite32(Sr,Rn);
		SH4_SetGpr(Rn,rn);
	} else {
		SH4_Exception(EX_RESINST);
	}
}

/**
 *******************************************************
 * \fn void sh4_stcl_gbr_amrn(void)
 * Store Gbr at adress in Rn register.
 * Rn is pre-decremented by four.
 * v0
 *******************************************************
 */
void
sh4_stcl_gbr_amrn(void)
{
	int rn = (ICODE >> 8) & 0xf;
	uint32_t Gbr = SH4_GetGBR();
	uint32_t Rn = SH4_GetGpr(rn);
	Rn -= 4;
	/* Base restored exception model */
	SH4_MMUWrite32(Gbr,Rn);
	SH4_SetGpr(Rn,rn);
}

/**
 *******************************************************
 * \fn void sh4_stcl_vbr_amrn(void)
 * store VBR at the address in the Rn register.
 * Rn is predecremented by four.
 * v0
 *******************************************************
 */
void
sh4_stcl_vbr_amrn(void)
{
	int rn = (ICODE >> 8) & 0xf;
	uint32_t Vbr;
	uint32_t Rn;
	if(SH4_GetSR() & SR_MD) {
		Vbr = SH4_GetVBR();
		Rn = SH4_GetGpr(rn);
		Rn -= 4;
		/* Base restored exception model ? */
		SH4_MMUWrite32(Vbr,Rn);
		SH4_SetGpr(Rn,rn);
	} else {
		SH4_Exception(EX_RESINST);
	}
}

/**
 ********************************************************
 * \fn void sh4_stcl_ssr_amrn(void)
 * store SSR at the address in register Rn
 * Rn is pre-decremented by four.
 * v0
 ********************************************************
 */
void
sh4_stcl_ssr_amrn(void)
{
	int rn = (ICODE >> 8) & 0xf;
	uint32_t Ssr;
	uint32_t Rn;
	if(SH4_GetSR() & SR_MD) {
		Ssr = SH4_GetSSR();
		Rn = SH4_GetGpr(rn);
		Rn -= 4;
		/* Base restored exception model */
		SH4_MMUWrite32(Ssr,Rn);
		SH4_SetGpr(Rn,rn);
	} else {
		SH4_Exception(EX_RESINST);
	}
}

/**
 *********************************************************
 * \fn void sh4_stcl_spc_amrn(void)
 * Store SPC at the memory address in register Rn.
 * Rn is pre-decremented by four.
 * v0
 *********************************************************
 */
void
sh4_stcl_spc_amrn(void)
{
	int rn = (ICODE >> 8) & 0xf;
	uint32_t Spc;
	uint32_t Rn;
	if(SH4_GetSR() & SR_MD) {
		Spc = SH4_GetSPC();
		Rn = SH4_GetGpr(rn);
		Rn -= 4;
		/* Base restored exception model ??? */
		SH4_MMUWrite32(Spc,Rn);
		SH4_SetGpr(Rn,rn);
	} else {
		SH4_Exception(EX_RESINST);
	}
}

/**
 ***********************************************************
 * \fn void sh4_stcl_sgr_amrn(void)
 * Store SGR at the memory address in register Rn.
 * Rn is predecremented by four.
 ***********************************************************
 */
void
sh4_stcl_sgr_amrn(void)
{
	int rn = (ICODE >> 8) & 0xf;
	uint32_t Sgr;
	uint32_t Rn;
	if(SH4_GetSR() & SR_MD) {
		Sgr = SH4_GetSGR();
		Rn = SH4_GetGpr(rn);
		Rn -= 4;
		/* Base restored exception model */
		SH4_MMUWrite32(Sgr,Rn);
		SH4_SetGpr(Rn,rn);
	} else {
		SH4_Exception(EX_RESINST);
	}
}

/**
 ***********************************************************
 * \fn void sh4_stcl_dbr_amrn(void)
 * Store DBR at the memory address in register Rn.
 * Rn is predecremented by four.
 * v0
 ***********************************************************
 */
void
sh4_stcl_dbr_amrn(void)
{
	int rn = (ICODE >> 8) & 0xf;
	uint32_t Dbr;
	uint32_t Rn;
	if(SH4_GetSR() & SR_MD) {
		Dbr = SH4_GetDBR();
		Rn = SH4_GetGpr(rn);
		Rn -= 4;
		/* Base restored exception model ??? */
		SH4_MMUWrite32(Dbr,Rn);
		SH4_SetGpr(Rn,rn);
	} else {
		SH4_Exception(EX_RESINST);
	}
}

/** 
 ************************************************************
 * \fn void sh4_stcl_rb_amrn(void)
 * Store Banked Register Rb at the memory address in
 * register Rn. Rn is predecremented by 4.
 * v0
 ************************************************************
 */
void
sh4_stcl_rb_amrn(void)
{
	int rn = (ICODE >> 8) & 0xf;
	int rb = (ICODE >> 4) & 7;
	uint32_t Rb;
	uint32_t Rn;
	if(SH4_GetSR() & SR_MD) {
		Rb = SH4_GetGprBank(rb);
		Rn = SH4_GetGpr(rn);
		Rn -= 4;
		/* Base restored exception model */
		SH4_MMUWrite32(Rb,Rn);
		SH4_SetGpr(Rn,rn);
	} else {
		SH4_Exception(EX_RESINST);
	}
}

/*
 ********************************************************************
 * \fn void sh4_sts_mach_rn(void)
 * Store System register MacH -> Rn.
 * v1
 ********************************************************************
 */
void
sh4_sts_mach_rn(void)
{
	int rn = (ICODE >> 8) & 0xf;
	uint32_t Mach = SH4_GetMacH();
	SH4_SetGpr(Mach,rn);
}

/**
 *******************************************************************
 * \fn void sh4_sts_macl_rn(void)
 * Store system register MacH -> Rn.
 * v1
 *******************************************************************
 */
void
sh4_sts_macl_rn(void)
{
	int rn = (ICODE >> 8) & 0xf;
	uint32_t Macl = SH4_GetMacL();
	SH4_SetGpr(Macl,rn);
}

/*
 ********************************************************************
 * \fn void sh4_sts_pr_rn(void)
 * Store PR register in Rn.
 * v1
 ********************************************************************
 */
void
sh4_sts_pr_rn(void)
{
	int rn = (ICODE >> 8) & 0xf;
	uint32_t Pr = SH4_GetPR();
	SH4_SetGpr(Pr,rn);
}

/**
 ******************************************************************
 * \fn void sh4_stsl_mach_amrn(void)
 * Store MacH register in the address at Rn.
 * Rn is pre-decremented by four.
 * v1
 ******************************************************************
 */
void
sh4_stsl_mach_amrn(void)
{
	int rn = (ICODE >> 8) & 0xf;
	uint32_t Mach = SH4_GetMacH();
	uint32_t Rn = SH4_GetGpr(rn);
	Rn -= 4;
	/* Base restored exception model */
	SH4_MMUWrite32(Mach,Rn);
	SH4_SetGpr(Rn,rn);
}

/**
 ***************************************************************
 * \fn void sh4_stsl_macl_amrn(void)
 * Store MacL register in the address at Rn. 
 * Rn is pre-decremented by four.
 * v1
 ***************************************************************
 */
void
sh4_stsl_macl_amrn(void)
{
	int rn = (ICODE >> 8) & 0xf;
	uint32_t Macl = SH4_GetMacL();
	uint32_t Rn = SH4_GetGpr(rn);
	Rn -= 4;
	/* Base restored exception model */
	SH4_MMUWrite32(Macl,Rn);
	SH4_SetGpr(Rn,rn);
}

/**
 **********************************************************
 * \fn void sh4_stsl_pr_amrn(void)
 * Store value of PR register in the address in Rn. 
 * Rn is pre-decremented by four.
 * v1
 **********************************************************
 */
void
sh4_stsl_pr_amrn(void)
{
	int rn = (ICODE >> 8) & 0xf;
	uint32_t Pr = SH4_GetPR();
	uint32_t Rn = SH4_GetGpr(rn);
	Rn -= 4;
	/* Base restored exception model */
	SH4_MMUWrite32(Pr,Rn);
	SH4_SetGpr(Rn,rn);
}

void
sh4_sts_fpul_rn(void)
{
	int rn = (ICODE >> 8) & 0xf;
	uint32_t Fpul;
	Fpul = SH4_GetFPUL();
	SH4_SetGpr(Fpul,rn);
	fprintf(stderr,"sh4_sts_fpul_rn not tested\n");
}

void
sh4_fpscr_rn(void)
{
	int rn = (ICODE >> 8) & 0xf;
	uint32_t Fpscr;
	Fpscr = SH4_GetFPSCR();
	SH4_SetGpr(Fpscr,rn);
	fprintf(stderr,"sh4_fpscr_rn not tested\n");
}

void
sh4_sts_fpul_amrn(void)
{
	int rn = (ICODE >> 8) & 0xf;
	uint32_t Rn;
	uint32_t Fpul;
	Fpul = SH4_GetFPUL();
	Rn = SH4_GetGpr(rn);
	Rn -= 4;
	/* I think that the Renesas manual is wrong and that
 	 * base address is restored in case of exception */
	SH4_MMUWrite32(Fpul,Rn);
	SH4_SetGpr(Rn,rn);
	fprintf(stderr,"sh4_sts_fpul_amrn not tested\n");
}

void
sh4_sts_fpscr_amrn(void)
{
	int rn = (ICODE >> 8) & 0xf;
	uint32_t Rn;
	uint32_t Fpscr;
	Fpscr = SH4_GetFPSCR();
	Rn = SH4_GetGpr(rn);
	Rn -= 4;
	/* I think that the Renesas manual is wrong and that
 	 * base address is restored in case of exception */
	SH4_MMUWrite32(Fpscr,Rn);
	SH4_SetGpr(Rn,rn);
	fprintf(stderr,"sh4_sts_fpscr_amrn not tested\n");
}

/*
 ********************************************************
 * \fn void sh4_sub_rm_rn(void)
 * Subtract Rn - Rm and store the result in Rn.
 * v1
 ********************************************************
 */
void
sh4_sub_rm_rn(void)
{
	int rn,rm;
	int32_t Rn,Rm,R;
	rn = (ICODE >> 8) & 0xf;
	rm = (ICODE >> 4) & 0xf;
	Rm = SH4_GetGpr(rm);
	Rn = SH4_GetGpr(rn);
	R = Rn - Rm; 
	SH4_SetGpr(R,rn);
}

/**
 *********************************************************************************** 
 * \fn void sh4_subc_rm_rn(void)
 * Subtract with carry.
 * v1
 *********************************************************************************** 
 */
void
sh4_subc_rm_rn(void)
{
	int rn,rm;
	int32_t Rn,Rm,R;
	rn = (ICODE >> 8) & 0xf;
	rm = (ICODE >> 4) & 0xf;
	Rm = SH4_GetGpr(rm);
	Rn = SH4_GetGpr(rn);
	R = Rn - Rm - SH4_GetTrue(); 
	SH4_SetGpr(R,rn);
	if(sub_carry(Rm,Rn,R)) {
		SH4_SetTrue(1);
	} else {
		SH4_SetTrue(0);
	}
}

/**
 **********************************************************************************
 * \fn void sh4_subv_rm_rn(void)
 * Subtract with underflow check.
 * v1
 **********************************************************************************
 */
void
sh4_subv_rm_rn(void)
{
	int rn,rm;
	uint32_t Rn,Rm,R;
	rn = (ICODE >> 8) & 0xf;
	rm = (ICODE >> 4) & 0xf;
	Rm = SH4_GetGpr(rm);
	Rn = SH4_GetGpr(rn);
	R = Rn - Rm; 
	SH4_SetGpr(R,rn);
	if(sub_overflow(Rm,Rn,R)) {
		SH4_SetTrue(1);
	} else {
		SH4_SetTrue(0);
	}
}

/** 
 *********************************************************************************
 * \fn void sh4_swapb_rm_rn(void)
 * Swap lower two bytes from Register Rm and store the result in register Rn.
 *********************************************************************************
 */
void
sh4_swapb_rm_rn(void)
{
	int rn,rm;
	uint32_t Rn,Rm;
	rn = (ICODE >> 8) & 0xf;
	rm = (ICODE >> 4) & 0xf;
	Rm = SH4_GetGpr(rm);
	Rn = (Rm & 0xFFFF0000) | ((Rm >> 8) & 0xff) | ((Rm << 8) & 0xff00);
	SH4_SetGpr(Rn,rn);
}

/**
 ******************************************************************************
 * \fn void sh4_swapw_rm_rn(void)
 * Swap upper word of Rm with lower word and store the result in Rn.
 * v1
 ******************************************************************************
 */
void
sh4_swapw_rm_rn(void)
{
	int rn,rm;
	uint32_t Rn,Rm;
	rn = (ICODE >> 8) & 0xf;
	rm = (ICODE >> 4) & 0xf;
	Rm = SH4_GetGpr(rm);
	Rn = ((Rm >> 16) & 0xFFFF) | ((Rm & 0xffff) << 16);
	SH4_SetGpr(Rn,rn);
}

/**
 *****************************************************************************
 * \fn void sh4_tas(void)
 * Test and set bit.
 * v1
 *****************************************************************************
 */
void
sh4_tas(void)
{
	int rn;
	uint32_t Rn;
	uint8_t data;
	rn = (ICODE >> 8) & 0xf;
	Rn = SH4_GetGpr(rn);	
	data = SH4_MMURead8(Rn);
	SH4_MMUWrite8(data | 0x80, Rn);
	if(data == 0) {
		SH4_SetTrue(1);
	} else {
		SH4_SetTrue(0);
	}
}

void
sh4_trapa(void)
{
	fprintf(stderr,"sh4_trapa not implemented\n");
	SH4_Break();
}

/**
 **************************************************************************
 * \fn void sh4_tst_rm_rn(void)
 * Test instruction. Does an "AND" of two registers and 
 * modifies only the T flag.
 **************************************************************************
 */
void
sh4_tst_rm_rn(void)
{
	int rn,rm;
	uint32_t Rn,Rm;
	rn = (ICODE >> 8) & 0xf;
	rm = (ICODE >> 4) & 0xf;
	Rn = SH4_GetGpr(rn);
	Rm = SH4_GetGpr(rm);
	if((Rm & Rn) == 0) {
		SH4_SetTrue(1);
	} else {
		SH4_SetTrue(0);
	}
}

/**
 ****************************************************************************
 * \fn void sh4_tst_imm_r0(void)
 * Test instruction. Does an "AND" of a register with an immediate and 
 * modifies only the T flag.
 * v1
 ****************************************************************************
 */
void
sh4_tst_imm_r0(void)
{
	uint32_t imm;
	uint32_t R0;
	R0 = SH4_GetGpr(0);
	imm = ICODE & 0xff;
	if((R0 & imm) == 0) {
		SH4_SetTrue(1);
	} else {
		SH4_SetTrue(0);
	}
}

/**
 ****************************************************************************
 * \fn void sh4_tstb_imm_ar0gbr(void)
 * Calculate "AND" of a byte from memory at address (R0 + GBR) and an
 * immediate.
 * v1
 ****************************************************************************
 */
void
sh4_tstb_imm_ar0gbr(void)
{
	uint32_t imm;
	uint32_t data;
	uint32_t R0,Gbr;
	R0 = SH4_GetGpr(0);
	Gbr = SH4_GetGBR();
	imm = ICODE & 0xff;
	data = SH4_MMURead8(R0 + Gbr);
	if((data & imm) == 0) {
		SH4_SetTrue(1);
	} else {
		SH4_SetTrue(0);
	}
}

/**
 *****************************************************************************
 * \fn void sh4_xor_rm_rn(void)
 * Calculate the exclusive OR off two  registers. Store the result in Rn.
 * v1
 *****************************************************************************
 */
void
sh4_xor_rm_rn(void)
{
	int rn,rm;
	uint32_t Rn,Rm;
	rn = (ICODE >> 8) & 0xf;
	rm = (ICODE >> 4) & 0xf;
	Rn = SH4_GetGpr(rn);
	Rm = SH4_GetGpr(rm);
	Rn = Rn ^ Rm;
	SH4_SetGpr(Rn,rn);
}

/**
 *******************************************************************************
 * \fn void sh4_xor_imm_r0(void)
 * Calculate the exclusive OR of register R0 and an immediate. 
 * The result is stored in register R0. 
 * v1
 *******************************************************************************
 */
void
sh4_xor_imm_r0(void)
{
	uint32_t R0,imm;
	imm = ICODE & 0xff;
	R0 = SH4_GetGpr(0);
	R0 = R0 ^ imm;
	SH4_SetGpr(R0,0);
}

/**
 ****************************************************************************
 * \fn void sh4_xorb_ar0gbr(void)
 * Calculate the exclusive OR of an immediate and a byte stored in memory
 * at address (R0 + GBR).
 * v1
 ****************************************************************************
 */
void
sh4_xorb_ar0gbr(void)
{
	uint32_t imm;
	uint32_t data;
	uint32_t R0,Gbr;
	R0 = SH4_GetGpr(0);
	Gbr = SH4_GetGBR();
	imm = ICODE & 0xff;
	data = SH4_MMURead8(R0 + Gbr);
	data = data ^ imm;
	SH4_MMUWrite8(data,R0 + Gbr);
}

/**
 ***********************************************************************************
 * \fn void sh4_xtrct(void)
 * Middle extract from two linked register. Concatenates lower 16 Bit of Rm with
 * upper 16 Bit of Rn and stores the result in Rn.
 ***********************************************************************************
 */
void
sh4_xtrct(void)
{
	int rn,rm;
	uint32_t Rn,Rm;
	rn = (ICODE >> 8) & 0xf;
	rm = (ICODE >> 4) & 0xf;
	Rn = SH4_GetGpr(rn);
	Rm = SH4_GetGpr(rm);
	Rn = ((Rm & 0xffff) << 16) | (Rn >> 16);
	SH4_SetGpr(Rn,rn);
}

void
sh4_undef(void)
{
	fprintf(stderr,"SH4 undefined instruction code %04x at %08x\n",ICODE,SH4_CIA);
	SH4_Break();
	//exit(1);
}
