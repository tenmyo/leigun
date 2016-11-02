/*
 *********************************************************************************************
 *
 * Intel 8051 instruction set emulation
 *
 * State: not implemented
 *
 * Copyright 2009 Jochen Karrer. All rights reserved.
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
#include "cpu_mcs51.h"
#include "instructions_mcs51.h"

#define ISNEG(x) ((x) & (1 << 7))
#define ISNOTNEG(x) (!((x) & (1 << 7)))
#define ISHALFNEG(x) ((x) & (1 << 3))
#define ISNOTHALFNEG(x) (!((x) & (1 << 3)))

static inline uint8_t 
add8_overflow(uint8_t op1,uint8_t op2,uint8_t result) 
{
	if(ISNEG(op1) && ISNEG(op2) && ISNOTNEG(result)) {
		return PSW_OV;
	} else if(ISNOTNEG(op1) && ISNOTNEG(op2) && ISNEG(result)) {
		return PSW_OV;
	}
	return 0;
}

static inline uint8_t
add8_carry(uint8_t op1,uint8_t op2,uint8_t result) {
        if( ((ISNEG(op1) && ISNEG(op2))
          || (ISNEG(op1) && ISNOTNEG(result))
          || (ISNEG(op2) && ISNOTNEG(result)))) {
                        return PSW_CY;
        } else {
                return 0;
        }
}

static inline uint8_t
add4_carry(uint8_t op1,uint8_t op2,uint8_t result) {
        if( ((ISHALFNEG(op1) && ISHALFNEG(op2))
          || (ISHALFNEG(op1) && ISNOTHALFNEG(result))
          || (ISHALFNEG(op2) && ISNOTHALFNEG(result)))) {
                        return PSW_AC;
        } else {
                return 0;
        }
}

/*
 ****************************************************
 * Borrow style carry. Sub sets carry if a borrow is
 * needed 
 ****************************************************
 */ 
static inline uint8_t
sub8_carry(uint8_t op1,uint8_t op2,uint8_t result) 
{
	if( ((ISNEG(op1) && ISNOTNEG(op2))
          || (ISNEG(op1) && ISNOTNEG(result))
          || (ISNOTNEG(op2) && ISNOTNEG(result)))) {
                        return 0;
        } else {
                        return PSW_CY;
        }
}


static inline uint8_t
sub4_carry(uint8_t op1,uint8_t op2,uint8_t result) {
	if( ((ISHALFNEG(op1) && ISNOTHALFNEG(op2))
          || (ISHALFNEG(op1) && ISNOTHALFNEG(result))
          || (ISNOTHALFNEG(op2) && ISNOTHALFNEG(result)))) {
                        return 0;
        } else {
                        return PSW_AC;
        }
}

static inline uint8_t 
sub8_overflow(uint8_t op1,uint8_t op2,uint8_t result) 
{
	if ((ISNEG(op1) && ISNOTNEG (op2) && ISNOTNEG (result))
		|| (ISNOTNEG (op1) && ISNEG (op2) && ISNEG (result))) {
		return PSW_OV;
	} else {
		return 0;
	}
}

void
mcs51_add(void)
{
	int reg = ICODE & 7;
	uint8_t A = MCS51_GetAcc();
	uint8_t R = MCS51_GetRegR(reg);
	uint8_t result = A+R;
	MCS51_SetAcc(result);
	PSW = PSW & ~(PSW_CY | PSW_AC | PSW_OV);
	PSW |= 	add8_carry(A,R,result) | 
	       	add4_carry(A,R,result) |
		add8_overflow(A,R,result);

	fprintf(stderr,"mcs51_add not tested\n");
}

void
mcs51_adddir(void)
{
	uint8_t data; 
	uint8_t addr;
	uint8_t result;
	uint8_t A;
	addr = MCS51_ReadPgmMem(GET_REG_PC);
	data = MCS51_ReadMem(addr);
        SET_REG_PC(GET_REG_PC + 1);
	A = MCS51_GetAcc();
	result = A + data;
	PSW = PSW & ~(PSW_CY | PSW_AC | PSW_OV);
	PSW |= 	add8_carry(A,data,result) | 
	       	add4_carry(A,data,result) |
		add8_overflow(A,data,result);
	MCS51_SetAcc(result);
	fprintf(stderr,"mcs51_adddir not tested\n");
}

void
mcs51_addari(void)
{
	uint8_t A;	
	uint8_t R;
	uint8_t op2;
	uint8_t result;
	uint8_t reg = ICODE & 1;
	A = MCS51_GetAcc();
	R = MCS51_GetRegR(reg);
	op2 = MCS51_ReadMem(R);
	result = A + op2;
	PSW = PSW & ~(PSW_CY | PSW_AC | PSW_OV);
	PSW |= 	add8_carry(A,op2,result) | 
	       	add4_carry(A,op2,result) |
		add8_overflow(A,op2,result);
	MCS51_SetAcc(result);
	fprintf(stderr,"mcs51_addari not tested\n");
}

void
mcs51_addadata(void)
{
	uint8_t A;	
	uint8_t data;
	uint8_t result;
	data = MCS51_ReadPgmMem(GET_REG_PC);
        SET_REG_PC(GET_REG_PC + 1);
	A = MCS51_GetAcc();
	result = A + data;		
	PSW = PSW & ~(PSW_CY | PSW_AC | PSW_OV);
	PSW |= 	add8_carry(A,data,result) | 
	       	add4_carry(A,data,result) |
		add8_overflow(A,data,result);
	MCS51_SetAcc(result);
	fprintf(stderr,"mcs51_addadata not implemented\n");
}

void
mcs51_addc(void)
{
	int reg = ICODE & 7;
	uint8_t A = MCS51_GetAcc();
	uint8_t R = MCS51_GetRegR(reg);
	uint8_t C = !!(PSW & PSW_CY);
	uint8_t result = A + R + C;
	PSW = PSW & ~(PSW_CY | PSW_AC | PSW_OV);
	PSW |= 	add8_carry(A,R,result) | 
	       	add4_carry(A,R,result) |
		add8_overflow(A,R,result);
	MCS51_SetAcc(result);

	fprintf(stderr,"mcs51_addc not implemented\n");
}

void
mcs51_addcdir(void)
{
	uint8_t data; 
	uint8_t addr;
	uint8_t result;
	uint8_t A;
	uint8_t C = !!(PSW & PSW_CY);
	addr = MCS51_ReadPgmMem(GET_REG_PC);
	data = MCS51_ReadMem(addr);
        SET_REG_PC(GET_REG_PC + 1);
	A = MCS51_GetAcc();
	result = A + data + C;
	PSW = PSW & ~(PSW_CY | PSW_AC | PSW_OV);
	PSW |= 	add8_carry(A,data,result) | 
	       	add4_carry(A,data,result) |
		add8_overflow(A,data,result);
	MCS51_SetAcc(result);
	fprintf(stderr,"mcs51_addcdir not implemented\n");
}

void
mcs51_addcari(void)
{
	uint8_t A;	
	uint8_t R;
	uint8_t C;
	uint8_t op2;
	uint8_t result;
	uint8_t reg = ICODE & 1;
	C = !!(PSW & PSW_CY);
	A = MCS51_GetAcc();
	R = MCS51_GetRegR(reg);
	op2 = MCS51_ReadMem(R);
	result = A + op2 + C;
	PSW = PSW & ~(PSW_CY | PSW_AC | PSW_OV);
	PSW |= 	add8_carry(A,op2,result) | 
	       	add4_carry(A,op2,result) |
		add8_overflow(A,op2,result);
	MCS51_SetAcc(result);
	fprintf(stderr,"mcs51_addcari not implemented\n");
}

void
mcs51_addcadata(void)
{
	uint8_t A;	
	uint8_t data;
	uint8_t result;
	uint8_t C;
	C = !!(PSW & PSW_CY);
	data = MCS51_ReadPgmMem(GET_REG_PC);
        SET_REG_PC(GET_REG_PC + 1);
	A = MCS51_GetAcc();
	result = A + data + C;
	PSW = PSW & ~(PSW_CY | PSW_AC | PSW_OV);
	PSW |= 	add8_carry(A,data,result) | 
	       	add4_carry(A,data,result) |
		add8_overflow(A,data,result);
	MCS51_SetAcc(result);
	fprintf(stderr,"mcs51_addcadata not implemented\n");
}

void
mcs51_anlrn(void)
{
	int reg = ICODE & 7;
	uint8_t A = MCS51_GetAcc();
	uint8_t R = MCS51_GetRegR(reg);
	uint8_t result = A & R;
	MCS51_SetAcc(result);
	fprintf(stderr,"mcs51_anlrn not tested\n");
}

void
mcs51_anldir(void)
{
	uint8_t addr;
	uint8_t data; 
	uint8_t result;
	uint8_t A;
	addr = MCS51_ReadPgmMem(GET_REG_PC);
	data = MCS51_ReadMem(addr);
        SET_REG_PC(GET_REG_PC + 1);
	A = MCS51_GetAcc();
	result = A  & data;
	MCS51_SetAcc(result);
	fprintf(stderr,"mcs51_anldir not tested\n");
}

void
mcs51_anlari(void)
{
	uint8_t A;	
	uint8_t R;
	uint8_t op2;
	uint8_t result;
	uint8_t reg = ICODE & 1;
	A = MCS51_GetAcc();
	R = MCS51_GetRegR(reg);
	op2 = MCS51_ReadPgmMem(R);
	result = A & op2;
	MCS51_SetAcc(result);
}

void
mcs51_anladata(void)
{
	uint8_t A;	
	uint8_t data;
	uint8_t result;
	data = MCS51_ReadPgmMem(GET_REG_PC);
        SET_REG_PC(GET_REG_PC + 1);
	A = MCS51_GetAcc();
	result = A & data;
	MCS51_SetAcc(result);
}

void
mcs51_anldira(void)
{
	uint8_t data; 
	uint8_t result;
	uint8_t A;
	uint8_t addr;
	addr = MCS51_ReadPgmMem(GET_REG_PC);
	data = MCS51_ReadMem(addr);
	A = MCS51_GetAcc();
	result = A  & data;
	MCS51_WriteMem(result,GET_REG_PC);
        SET_REG_PC(GET_REG_PC + 1);
	fprintf(stderr,"mcs51_anldira not implemented\n");
}

void
mcs51_anldirdata(void)
{
	uint8_t addr = MCS51_ReadPgmMem(GET_REG_PC); 
	uint8_t imm = MCS51_ReadPgmMem(GET_REG_PC+1);
	uint8_t data = MCS51_ReadMem(addr);	
	uint8_t result;
	result = data & imm;	
	MCS51_WriteMem(result,GET_REG_PC);
        SET_REG_PC(GET_REG_PC + 2);
	fprintf(stderr,"mcs51_anldirdata not tested\n");
}

void
mcs51_anlcbit(void)
{
	uint8_t bitaddr = MCS51_ReadPgmMem(GET_REG_PC);
	uint8_t data = MCS51_ReadBit(bitaddr);
	uint8_t C; 
	C = !!(PSW & PSW_CY);
	if(data & C) {
		PSW |= PSW_CY;
	} else {
		PSW &= ~PSW_CY;
	}
        SET_REG_PC(GET_REG_PC + 1);
	fprintf(stderr,"mcs51_anlcbit not implemented\n");
}

void
mcs51_anlcnbit(void)
{
	uint8_t bitaddr = MCS51_ReadPgmMem(GET_REG_PC);
	uint8_t data = MCS51_ReadBit(bitaddr);
	uint8_t C; 
	C = !!(PSW & PSW_CY);
	if(C & !data) {
		PSW |= PSW_CY;
	} else {
		PSW &= ~PSW_CY;
	}
        SET_REG_PC(GET_REG_PC + 1);
	fprintf(stderr,"mcs51_anlcnbit not implemented\n");
}

void
mcs51_cjneadirrel(void)
{
	uint8_t dira;
	uint8_t dir;
	int8_t rel;
	uint8_t A;
	A = MCS51_GetAcc();
	dira = MCS51_ReadPgmMem(GET_REG_PC);
	rel = MCS51_ReadMem(GET_REG_PC + 1);
        SET_REG_PC(GET_REG_PC + 2);
	dir = MCS51_ReadMem(dira);
	if(A != dir) {
		SET_REG_PC(GET_REG_PC + rel);
	}
	if(A < dir) {
		PSW |= PSW_CY;
	} else {
		PSW &= ~PSW_CY;
	}
	fprintf(stderr,"mcs51_cjneadirrel not tested\n");
}

void
mcs51_cjneadataresl(void)
{
	uint8_t A;
	uint8_t imm;
	uint8_t rel;
	A = MCS51_GetAcc();
	imm = MCS51_ReadPgmMem(GET_REG_PC);
	rel = MCS51_ReadMem(GET_REG_PC + 1);
        SET_REG_PC(GET_REG_PC + 2);
	if(A != imm) {
		SET_REG_PC(GET_REG_PC + rel);
	}
	if(A < imm) {
		PSW |= PSW_CY;
	} else {
		PSW &= ~PSW_CY;
	}
		
	fprintf(stderr,"mcs51_cjneadataresl not implemented\n");
}

void
mcs51_cjnerdatarel(void)
{
	uint8_t R;
	uint8_t imm;
	uint8_t rel;
	R = MCS51_GetRegR(ICODE & 7);
	imm = MCS51_ReadPgmMem(GET_REG_PC);
	rel = MCS51_ReadMem(GET_REG_PC + 1);
        SET_REG_PC(GET_REG_PC + 2);
	if(R != imm) {
		SET_REG_PC(GET_REG_PC + rel);
	}
	if(R < imm) {
		PSW |= PSW_CY;
	} else {
		PSW &= ~PSW_CY;
	}
	fprintf(stderr,"mcs51_cjnerdatarel not implemented\n");
}

void
mcs51_cjneardatarel(void)
{
	uint8_t R;
	uint8_t imm;
	uint8_t rel;
	uint8_t val;
	R = MCS51_GetRegR(ICODE & 7);
	imm = MCS51_ReadPgmMem(GET_REG_PC);
	rel = MCS51_ReadPgmMem(GET_REG_PC + 1);
	val = MCS51_ReadMem(R);
        SET_REG_PC(GET_REG_PC + 2);
	if(val != imm) {
		SET_REG_PC(GET_REG_PC + rel);
	}
	if(val < imm) {
		PSW |= PSW_CY;
	} else {
		PSW &= ~PSW_CY;
	}
	fprintf(stderr,"mcs51_cjneardatarel not tested\n");
}

void
mcs51_clra(void)
{
	MCS51_SetAcc(0);
	fprintf(stderr,"mcs51_clra not tested\n");
}

void
mcs51_clrc(void)
{
	PSW &= ~PSW_CY;
	fprintf(stderr,"mcs51_clrc not tested\n");
}

void
mcs51_clrbit(void)
{
	uint8_t bitaddr;
	bitaddr = MCS51_ReadPgmMem(GET_REG_PC);
	MCS51_WriteBit(0,bitaddr);
        SET_REG_PC(GET_REG_PC + 1);
	fprintf(stderr,"mcs51_clrbit not tested\n");
}

void
mcs51_cpla(void)
{
	MCS51_SetAcc(~MCS51_GetAcc());
	fprintf(stderr,"mcs51_cpla not implemented\n");
}

void
mcs51_cplc(void)
{
	PSW ^= PSW_CY;
	fprintf(stderr,"mcs51_cplc not implemented\n");
}

void
mcs51_cplbit(void)
{
	uint8_t bitaddr;
	uint8_t data;
	bitaddr = MCS51_ReadPgmMem(GET_REG_PC);
	data = MCS51_ReadBit(bitaddr);
	MCS51_WriteBit(!data,bitaddr);
        SET_REG_PC(GET_REG_PC + 1);
	fprintf(stderr,"mcs51_cplbit not tested\n");
}

void
mcs51_da(void)
{
	uint16_t A = MCS51_GetAcc();
	uint8_t AC = !!(PSW & PSW_AC);
	uint8_t C = !!(PSW & PSW_CY);
	if(((A & 0xf) > 9) || AC) {
		A = A + 6;
		if(A >= 0x100) {
		 	A = A & 0xff;
			C = 1;
		}
	}
	if(((A & 0xf0) > 90) || C) {
		A = A + 0x60;
		if(A >= 0x100) {
		 	A = A & 0xff;
			C = 1;
		}
	}
	MCS51_SetAcc(A);
	if(C) {
		PSW  |= PSW_CY;
	} else {
		PSW  &= ~PSW_CY;
	}
	fprintf(stderr,"mcs51_da not implemented\n");
}

void
mcs51_deca(void)
{
	MCS51_SetAcc(MCS51_GetAcc() - 1);
	fprintf(stderr,"mcs51_deca not implemented\n");
}

void
mcs51_decr(void)
{
	uint8_t reg = ICODE & 7;
	MCS51_SetRegR(MCS51_GetRegR(reg) - 1,reg);
	fprintf(stderr,"mcs51_decr not tested\n");
}

void
mcs51_decdir(void)
{
	uint8_t addr = MCS51_ReadPgmMem(GET_REG_PC);
	uint8_t dir;
        SET_REG_PC(GET_REG_PC + 1);
	dir = MCS51_ReadMem(addr);	
	MCS51_WriteMem(dir - 1,addr);	
	fprintf(stderr,"mcs51_decdir not tested\n");
}

void
mcs51_decari(void)
{
	uint8_t reg = ICODE & 1;
	uint8_t addr = MCS51_GetRegR(reg);
	uint8_t val = MCS51_ReadMem(addr);
	MCS51_WriteMem(val - 1,addr);	
	fprintf(stderr,"mcs51_decari not tested\n");
}

void
mcs51_divab(void)
{
	uint8_t A;
	uint8_t B;	
	uint16_t ab;
	A = MCS51_GetAcc();
	B = MCS51_GetRegB();
	if(B) {
		ab = A/B;
		A = ab >> 8;
		B = ab & 0xff;
	} else {
		PSW |= PSW_OV;
	}
	PSW &= ~PSW_CY;
	MCS51_SetAcc(A);
	MCS51_SetRegB(B);
	fprintf(stderr,"mcs51_divab not implemented\n");
}

void
mcs51_djnzrrel(void)
{
	int8_t rela = MCS51_ReadPgmMem(GET_REG_PC);
	uint8_t r = ICODE & 0x7;
	uint8_t R;
        SET_REG_PC(GET_REG_PC + 1);
	R = MCS51_GetRegR(r);
	R -= 1;
	MCS51_SetRegR(R,r);
	if(R != 0) {
		SET_REG_PC(GET_REG_PC + rela);
	}	
	fprintf(stderr,"mcs51_djnzrrel not implemented\n");
}

void
mcs51_djnzdirrel(void)
{
	uint8_t dira = MCS51_ReadPgmMem(GET_REG_PC);
	uint8_t value;
	int8_t rela = MCS51_ReadPgmMem(GET_REG_PC + 1);
        SET_REG_PC(GET_REG_PC + 2);
	value = MCS51_ReadMem(dira);	
	value -= 1;
	MCS51_WriteMem(value,dira);
	if(value != 0) {
		SET_REG_PC(GET_REG_PC +rela);
	}
	fprintf(stderr,"mcs51_djnzdirrel not tested\n");
}

void
mcs51_inca(void)
{
	MCS51_SetAcc(MCS51_GetAcc() + 1);
	fprintf(stderr,"mcs51_inca not implemented\n");
}

void
mcs51_incr(void)
{
	uint8_t reg = ICODE & 7;
	MCS51_SetRegR(MCS51_GetRegR(reg) + 1,reg);
	fprintf(stderr,"mcs51_incr not implemented\n");
}

void
mcs51_incdir(void)
{
	uint8_t addr = MCS51_ReadPgmMem(GET_REG_PC);
	uint8_t dir;
        SET_REG_PC(GET_REG_PC + 1);
	dir = MCS51_ReadMem(addr);	
	MCS51_WriteMem(dir + 1,addr);	
	fprintf(stderr,"mcs51_incdir not implemented\n");
}

void
mcs51_incari(void)
{
	uint8_t reg = ICODE & 1;
	uint8_t addr = MCS51_GetRegR(reg);
	uint8_t val = MCS51_ReadMem(addr);
	MCS51_WriteMem(val + 1,addr);	
	fprintf(stderr,"mcs51_incari not implemented\n");
}

void
mcs51_incdptr(void)
{
	MCS51_SetRegDptr(MCS51_GetRegDptr() + 1);
	fprintf(stderr,"mcs51_incdptr not tested\n");
}

void
mcs51_jbbitrel(void)
{
	uint8_t bitaddr = MCS51_ReadPgmMem(GET_REG_PC);
	uint8_t data;	
	int8_t rela = MCS51_ReadPgmMem(GET_REG_PC + 1);
        SET_REG_PC(GET_REG_PC + 2);
	data = MCS51_ReadBit(bitaddr);
	if(data) {
		SET_REG_PC(GET_REG_PC + rela);
	}
	fprintf(stderr,"mcs51_jbbitrel not tested\n");
}

void
mcs51_jbcbitrel(void)
{
	uint8_t bitaddr = MCS51_ReadPgmMem(GET_REG_PC);
	uint8_t data;	
	int8_t rela = MCS51_ReadPgmMem(GET_REG_PC + 1);
        SET_REG_PC(GET_REG_PC + 2);
	data = MCS51_ReadBit(bitaddr);
	MCS51_WriteBit(0,bitaddr);
	if(data) {
		SET_REG_PC(GET_REG_PC + rela);
	}
}

void
mcs51_jcrel(void)
{
	uint8_t C;
	int8_t rela = MCS51_ReadPgmMem(GET_REG_PC);
	C = !!(PSW & PSW_CY);
        SET_REG_PC(GET_REG_PC + 1);
	if(C) {
		SET_REG_PC(GET_REG_PC + rela);
	}
	fprintf(stderr,"mcs51_jcrel not implemented\n");
}

void
mcs51_jmpaadptr(void)
{
	uint16_t dptr = MCS51_GetRegDptr();
	uint8_t A = MCS51_GetAcc();
	SET_REG_PC(dptr + A);
	fprintf(stderr,"mcs51_jmpaadptr not tested\n");
}

void
mcs51_jnbbitrel(void)
{
	uint8_t bitaddr = MCS51_ReadPgmMem(GET_REG_PC);
	uint8_t data;	
	int8_t rela = MCS51_ReadPgmMem(GET_REG_PC + 1);
        SET_REG_PC(GET_REG_PC + 2);
	data = MCS51_ReadBit(bitaddr);
	if(!data) {
		SET_REG_PC(GET_REG_PC + rela);
	}
	fprintf(stderr,"mcs51_jnbbitrel not implemented\n");
}

void
mcs51_jncrel(void)
{
	uint8_t C;
	int8_t rela = MCS51_ReadPgmMem(GET_REG_PC);
	C = !!(PSW & PSW_CY);
        SET_REG_PC(GET_REG_PC + 1);
	if(!C) {
		SET_REG_PC(GET_REG_PC + rela);
	}
	fprintf(stderr,"mcs51_jncrel not implemented\n");
}

void
mcs51_jnzrel(void)
{
	uint8_t A;
	int8_t rela = MCS51_ReadPgmMem(GET_REG_PC);
	A = MCS51_GetAcc();	
        SET_REG_PC(GET_REG_PC + 1);
	if(A != 0) {
		SET_REG_PC(GET_REG_PC + rela);
	}
	fprintf(stderr,"mcs51_jnzrel not tested\n");
}

void
mcs51_jzrel(void)
{
	uint8_t A;
	int8_t rela = MCS51_ReadPgmMem(GET_REG_PC);
	A = MCS51_GetAcc();	
        SET_REG_PC(GET_REG_PC + 1);
	if(A == 0) {
		SET_REG_PC(GET_REG_PC + rela);
	}
	fprintf(stderr,"mcs51_jzrel not tested\n");
}

void
mcs51_lcall(void)
{
	uint16_t addr;
	uint16_t sp;
	addr = (MCS51_ReadPgmMem(GET_REG_PC+1) << 8) | (MCS51_ReadPgmMem(GET_REG_PC + 2));
        SET_REG_PC(GET_REG_PC + 2);
	sp = MCS51_GetRegSP();
	sp++;
	MCS51_WriteMem(GET_REG_PC & 0xff,sp);
	sp++;
	MCS51_WriteMem((GET_REG_PC >> 8) & 0xff,sp);
	MCS51_SetRegSP(sp);
	SET_REG_PC(addr);
	fprintf(stderr,"mcs51_lcall tested\n");
}

void
mcs51_ljmp(void)
{
	uint16_t addr;
	addr = (MCS51_ReadPgmMem(GET_REG_PC+1) << 8) | (MCS51_ReadPgmMem(GET_REG_PC + 2));
        SET_REG_PC(GET_REG_PC + 2);
	SET_REG_PC(addr);
	fprintf(stderr,"mcs51_ljmp not tested\n");
}

void
mcs51_movarn(void)
{
	uint8_t A;
	uint8_t reg = ICODE & 7;
	A = MCS51_GetAcc();	
	MCS51_SetRegR(A,reg);
	fprintf(stderr,"mcs51_movarn not tested\n");
}

void
mcs51_movadir(void)
{
	uint8_t dira = MCS51_ReadPgmMem(GET_REG_PC);
	uint8_t data;
        SET_REG_PC(GET_REG_PC + 1);
	data = MCS51_ReadMem(dira);
	MCS51_SetAcc(data);
	fprintf(stderr,"mcs51_movadir not tested\n");
}

void
mcs51_movaari(void)
{
	uint8_t reg = ICODE & 1;
	uint8_t R = MCS51_GetRegR(reg);
	uint8_t data = MCS51_ReadMem(R);
	MCS51_SetAcc(data);
	fprintf(stderr,"mcs51_movaari not implemented\n");
}

void
mcs51_movadata(void)
{
	uint8_t imm8 = MCS51_ReadPgmMem(GET_REG_PC);
        SET_REG_PC(GET_REG_PC + 1);
	MCS51_SetAcc(imm8);
	fprintf(stderr,"mcs51_movadata not tested\n");
}

void
mcs51_movra(void)
{
	uint8_t R;
	uint8_t reg = ICODE & 7;
	R = MCS51_GetRegR(reg);	
	MCS51_SetAcc(R);
	fprintf(stderr,"mcs51_movra not tested\n");
}

void
mcs51_movrdir(void)
{
	uint8_t reg = ICODE & 7;
	uint8_t dira = MCS51_ReadPgmMem(GET_REG_PC);
	uint8_t data;
        SET_REG_PC(GET_REG_PC + 1);
	data = MCS51_ReadMem(dira);
	MCS51_SetRegR(data,reg);
	fprintf(stderr,"mcs51_movrdir not tested\n");
}

void
mcs51_movrdata(void)
{
	uint8_t reg = ICODE & 7;
	uint8_t imm8 = MCS51_ReadPgmMem(GET_REG_PC);
        SET_REG_PC(GET_REG_PC + 1);
	MCS51_SetRegR(imm8,reg);
	fprintf(stderr,"mcs51_movrdata not implemented\n");
}

void
mcs51_movdira(void)
{
	uint8_t dira = MCS51_ReadPgmMem(GET_REG_PC);
	uint8_t data;
        SET_REG_PC(GET_REG_PC + 1);
	data = MCS51_GetAcc();
	MCS51_WriteMem(data,dira);
	fprintf(stderr,"mcs51_movdira not tested\n");
}

void
mcs51_movdirr(void)
{
	uint8_t reg = ICODE & 7;
	uint8_t dira = MCS51_ReadPgmMem(GET_REG_PC);
	uint8_t data;
        SET_REG_PC(GET_REG_PC + 1);
	data = MCS51_GetRegR(reg);
	MCS51_WriteMem(data,dira);
	fprintf(stderr,"mcs51_movdirr not implemented\n");
}

void
mcs51_movdirdir(void)
{
	uint8_t srca,dsta;
	uint8_t data;
	dsta = MCS51_ReadPgmMem(GET_REG_PC);
	srca = MCS51_ReadPgmMem(GET_REG_PC + 1);
        SET_REG_PC(GET_REG_PC + 2);
	data = MCS51_ReadMem(srca);	
	MCS51_WriteMem(data,dsta);
	fprintf(stderr,"mcs51_movdirdir not tested\n");
}

void
mcs51_movdirari(void)
{
	uint8_t dira = MCS51_ReadPgmMem(GET_REG_PC);	
	uint8_t reg = ICODE & 1;
	uint8_t R = MCS51_GetRegR(reg);
	uint8_t data = MCS51_ReadMem(R);
	SET_REG_PC(GET_REG_PC + 1);
	MCS51_WriteMem(data,dira);
	fprintf(stderr,"mcs51_movdirari not implemented\n");
}

void
mcs51_movdirdata(void)
{
	uint8_t dira = MCS51_ReadPgmMem(GET_REG_PC);
	uint8_t imm = MCS51_ReadPgmMem(GET_REG_PC + 1); 
	MCS51_WriteMem(imm,dira);	
	SET_REG_PC(GET_REG_PC + 1);
	fprintf(stderr,"mcs51_movdirdata not implemented\n");
}

void
mcs51_movaria(void)
{
	uint8_t reg = ICODE & 1;
	uint8_t R = MCS51_GetRegR(reg);
	uint8_t A = MCS51_GetAcc();
	MCS51_WriteMem(A,R);
	fprintf(stderr,"mcs51_movaria not tested\n");
}

void
mcs51_movaridir(void)
{
	uint8_t reg = ICODE & 1;
	uint8_t dira = MCS51_ReadPgmMem(GET_REG_PC);
	uint8_t R = MCS51_GetRegR(reg);
	uint8_t data;
	SET_REG_PC(GET_REG_PC + 1);
	data = MCS51_ReadMem(dira);	
	MCS51_WriteMem(data,R);
	fprintf(stderr,"mcs51_movaridir not implemented\n");
}

void
mcs51_movaridata(void)
{
	uint8_t reg = ICODE & 1;
	uint8_t imm = MCS51_ReadPgmMem(GET_REG_PC);
	uint8_t R = MCS51_GetRegR(reg);
	SET_REG_PC(GET_REG_PC + 1);
	MCS51_WriteMem(imm,R);
	fprintf(stderr,"mcs51_movaridata not implemented\n");
}

void
mcs51_movcbit(void)
{
	uint8_t bitaddr;	
	uint8_t data;
	bitaddr = MCS51_ReadPgmMem(GET_REG_PC);	
	SET_REG_PC(GET_REG_PC + 1);
	data = MCS51_ReadBit(bitaddr);
	if(data) {
		PSW = PSW | PSW_CY;
	} else {
		PSW = PSW & ~PSW_CY;
	}

	fprintf(stderr,"mcs51_movcbit not tested\n");
}

void
mcs51_movbitc(void)
{
	uint8_t bitaddr;	
	uint8_t C = !!(PSW & PSW_CY);
	bitaddr = MCS51_ReadPgmMem(GET_REG_PC);	
	SET_REG_PC(GET_REG_PC + 1);
	if(C) {
		MCS51_WriteBit(1,bitaddr);
	} else {
		MCS51_WriteBit(0,bitaddr);
	}
	fprintf(stderr,"mcs51_movbitc not tested\n");
}

void
mcs51_movdptrdata(void)
{
	uint16_t data = (MCS51_ReadPgmMem(GET_REG_PC) << 8) | 
		(MCS51_ReadPgmMem(GET_REG_PC + 1));
	SET_REG_PC(GET_REG_PC + 1);
	MCS51_SetRegDptr(data);
	fprintf(stderr,"mcs51_movdptrdata not tested\n");
}

void
mcs51_movcaadptr(void)
{
	uint16_t dptr = MCS51_GetRegDptr();
	uint8_t A = MCS51_GetAcc();
	uint8_t data = MCS51_ReadMem(dptr + A);
	MCS51_SetAcc(data);
	fprintf(stderr,"mcs51_movcaadptr tested\n");
}

void
mcs51_movaapc(void)
{
	/* Check if this really reads from Programm mem */
	uint8_t data = MCS51_ReadPgmMem(GET_REG_PC + MCS51_GetAcc());
	MCS51_SetAcc(data);
	fprintf(stderr,"mcs51_movaapc not implemented\n");
}

void
mcs51_movxaari(void)
{
	uint8_t reg = ICODE & 1;
	uint8_t R = MCS51_GetRegR(reg);
	uint8_t data = MCS51_ReadExmem(R);
	MCS51_SetAcc(data);
	fprintf(stderr,"mcs51_movxaari not implemented\n");
}

void
mcs51_movxaadptr(void)
{
	uint16_t dptr = MCS51_GetRegDptr();
	uint8_t data = MCS51_ReadExmem(dptr);
	MCS51_SetAcc(data);
	fprintf(stderr,"mcs51_movxaadptr not tested\n");
}

void
mcs51_movxara(void)
{
	uint8_t A = MCS51_GetAcc();
	uint8_t reg = ICODE & 1;
	uint8_t R = MCS51_GetRegR(reg);
	MCS51_WriteExmem(A,R);
	fprintf(stderr,"mcs51_movxara not tested\n");
}

void
mcs51_movxadptra(void)
{
	uint8_t A = MCS51_GetAcc();
	uint16_t dptr = MCS51_GetRegDptr();
	MCS51_WriteExmem(A,dptr);
	fprintf(stderr,"mcs51_movxadptra not implemented\n");
}

void
mcs51_mulab(void)
{
	uint16_t A,B;
	uint16_t ab;
	A = MCS51_GetAcc();
	B = MCS51_GetRegB();
	ab = A * B;
	MCS51_SetAcc(ab & 0xff);
	MCS51_SetRegB(ab >> 8);
	fprintf(stderr,"mcs51_mulab not tested\n");
}

void
mcs51_nop(void)
{
	fprintf(stderr,"mcs51_nop not implemented\n");
}

void
mcs51_orlar(void)
{
	int reg = ICODE & 7;
	uint8_t A = MCS51_GetAcc();
	uint8_t R = MCS51_GetRegR(reg);
	uint8_t result = A | R;
	MCS51_SetAcc(result);
	fprintf(stderr,"mcs51_orlar not implemented\n");
}

void
mcs51_orladir(void)
{
	uint8_t addr;
	uint8_t data; 
	uint8_t result;
	uint8_t A;
	addr = MCS51_ReadPgmMem(GET_REG_PC);
	data = MCS51_ReadMem(addr);
        SET_REG_PC(GET_REG_PC + 1);
	A = MCS51_GetAcc();
	result = A  | data;
	MCS51_SetAcc(result);
	fprintf(stderr,"mcs51_orladir not tested\n");
}

void
mcs51_orlaari(void)
{
	uint8_t A;	
	uint8_t R;
	uint8_t op2;
	uint8_t result;
	uint8_t reg = ICODE & 1;
	A = MCS51_GetAcc();
	R = MCS51_GetRegR(reg);
	op2 = MCS51_ReadMem(R);
	result = A | op2;
	MCS51_SetAcc(result);
}


void
mcs51_orladata(void)
{
	uint8_t A;	
	uint8_t data;
	uint8_t result;
	data = MCS51_ReadPgmMem(GET_REG_PC);
        SET_REG_PC(GET_REG_PC + 1);
	A = MCS51_GetAcc();
	result = A | data;
	MCS51_SetAcc(result);
	fprintf(stderr,"mcs51_orladata not implemented\n");
}

void
mcs51_orldira(void)
{
	uint8_t data; 
	uint8_t result;
	uint8_t A;
	uint8_t addr;
	addr = MCS51_ReadPgmMem(GET_REG_PC);
	data = MCS51_ReadMem(addr);
	A = MCS51_GetAcc();
	result = A  | data;
	MCS51_WriteMem(result,GET_REG_PC);
        SET_REG_PC(GET_REG_PC + 1);
	fprintf(stderr,"mcs51_orldira not implemented\n");
}

void
mcs51_orldirdata(void)
{
	uint8_t addr = MCS51_ReadPgmMem(GET_REG_PC); 
	uint8_t imm = MCS51_ReadPgmMem(GET_REG_PC+1);
	uint8_t data = MCS51_ReadMem(addr);	
	uint8_t result;
	result = data | imm;	
	MCS51_WriteMem(result,GET_REG_PC);
        SET_REG_PC(GET_REG_PC + 2);
	fprintf(stderr,"mcs51_orldirdata not implemented\n");
}

void
mcs51_orlcbit(void)
{
	uint8_t bitaddr = MCS51_ReadPgmMem(GET_REG_PC);
	uint8_t data = MCS51_ReadBit(bitaddr);
	uint8_t C; 
	C = !!(PSW & PSW_CY);
	if(data | C) {
		PSW |= PSW_CY;
	} else {
		PSW &= ~PSW_CY;
	}
        SET_REG_PC(GET_REG_PC + 1);
	fprintf(stderr,"mcs51_orlcbit not tested\n");
}

void
mcs51_orlcnbit(void)
{

	uint8_t bitaddr = MCS51_ReadPgmMem(GET_REG_PC);
	uint8_t data = MCS51_ReadBit(bitaddr);
	uint8_t C; 
	C = !!(PSW & PSW_CY);
	if(C | !data) {
		PSW |= PSW_CY;
	} else {
		PSW &= ~PSW_CY;
	}
        SET_REG_PC(GET_REG_PC + 1);
	fprintf(stderr,"mcs51_orlcnbit not tested\n");
}

void
mcs51_popdir(void)
{
	uint8_t dira = MCS51_ReadPgmMem(GET_REG_PC);
	uint16_t sp;
	uint8_t data;
        SET_REG_PC(GET_REG_PC + 1);
	sp = MCS51_GetRegSP();
	data = MCS51_ReadMem(sp);
	MCS51_WriteMem(data,dira);
	MCS51_SetRegSP(sp - 1);
	fprintf(stderr,"mcs51_popdir not tested\n");
}

void
mcs51_pushdir(void)
{
	uint8_t dira = MCS51_ReadPgmMem(GET_REG_PC);
	uint16_t sp;
	uint8_t data;
        SET_REG_PC(GET_REG_PC + 1);
	sp = MCS51_GetRegSP();
	sp++;	
	MCS51_SetRegSP(sp);
	data = MCS51_ReadMem(dira);
	MCS51_WriteMem(data,sp);
	fprintf(stderr,"mcs51_pushdir not implemented\n");
}

void
mcs51_ret(void)
{
	uint16_t pc;	
	uint16_t sp;
	sp = MCS51_GetRegSP();
	pc = MCS51_ReadMem(sp) << 8;	
	sp--;
	pc |= MCS51_ReadMem(sp);
	sp--;
	MCS51_SetRegSP(sp);
	SET_REG_PC(pc);
	fprintf(stderr,"mcs51_ret not tested\n");
}

void
mcs51_reti(void)
{
	/* Restore interrupt priority is missing here */
	uint16_t pc;	
	uint16_t sp;
	sp = MCS51_GetRegSP();
	pc = MCS51_ReadMem(sp) << 8;	
	sp--;
	pc |= MCS51_ReadMem(sp);
	sp--;
	MCS51_SetRegSP(sp);
	SET_REG_PC(pc);
	fprintf(stderr,"mcs51_reti not tested\n");
}

void
mcs51_rla(void)
{
	uint8_t A;
	A = MCS51_GetAcc();	
	if(A & 0x80) {
		A = (A << 1) | 1;
	} else {
		A = (A << 1);
	}
	MCS51_SetAcc(A);
	fprintf(stderr,"mcs51_rla not implemented\n");
}

void
mcs51_rlca(void)
{
	uint8_t A;
	uint8_t Cnew;
	A = MCS51_GetAcc();	
	Cnew = !!(A & 0x80);	
	if(PSW & PSW_CY) {
		A = (A << 1) | 1;
	} else {
		A = (A << 1);
	}
	MCS51_SetAcc(A);
	if(Cnew) {
		PSW |= PSW_CY;
	} else {
		PSW &= ~PSW_CY;
	}
	fprintf(stderr,"mcs51_rlca not tested\n");
}

void
mcs51_rra(void)
{
	uint8_t A;
	A = MCS51_GetAcc();	
	if(A & 0x1) {
		A = (A >> 1) | 0x80;
	} else {
		A = (A >> 1);
	}
	MCS51_SetAcc(A);
	fprintf(stderr,"mcs51_rra not implemented\n");
}

void
mcs51_rrca(void)
{
	uint8_t A;
	uint8_t Cnew;
	A = MCS51_GetAcc();	
	Cnew = !!(A & 0x1);	
	if(PSW & PSW_CY) {
		A = (A >> 1) | 0x80;
	} else {
		A = (A >> 1);
	}
	MCS51_SetAcc(A);
	if(Cnew) {
		PSW |= PSW_CY;
	} else {
		PSW &= ~PSW_CY;
	}
	fprintf(stderr,"mcs51_rrca not tested\n");
}

void
mcs51_setbc(void)
{
	PSW |= PSW_CY;
	fprintf(stderr,"mcs51_setbc not tested\n");
}

void
mcs51_setbbit(void)
{
	uint8_t bitaddr = MCS51_ReadPgmMem(GET_REG_PC);
	SET_REG_PC(GET_REG_PC + 1);
	MCS51_WriteBit(1,bitaddr);
	fprintf(stderr,"mcs51_setbbit not tested\n");
}

void
mcs51_sjmprel(void)
{
	int8_t rel = MCS51_ReadPgmMem(GET_REG_PC); 
	SET_REG_PC(GET_REG_PC + 1 + rel);
	fprintf(stderr,"mcs51_sjmprel not tested\n");
}

void
mcs51_subbar(void)
{
	int reg = ICODE & 7;
	uint8_t A = MCS51_GetAcc();
	uint8_t R = MCS51_GetRegR(reg);
	uint8_t C = !!(PSW & PSW_CY);
	uint8_t result = A - C - R;
	MCS51_SetAcc(result);
	PSW = PSW & ~(PSW_CY | PSW_AC | PSW_OV);
	PSW |= 	sub8_carry(A,R,result) | 
	       	sub4_carry(A,R,result) |
		sub8_overflow(A,R,result);
	fprintf(stderr,"mcs51_subbar not implemented\n");
}

void
mcs51_subbadir(void)
{
	uint8_t data; 
	uint8_t addr;
	uint8_t result;
	uint8_t A;
	uint8_t C = !!(PSW & PSW_CY);
	addr = MCS51_ReadPgmMem(GET_REG_PC);
	data = MCS51_ReadMem(addr);
        SET_REG_PC(GET_REG_PC + 1);
	A = MCS51_GetAcc();
	result = A - C - data;
	PSW = PSW & ~(PSW_CY | PSW_AC | PSW_OV);
	PSW |= 	sub8_carry(A,data,result) | 
	       	sub4_carry(A,data,result) |
		sub8_overflow(A,data,result);
	MCS51_SetAcc(result);
	fprintf(stderr,"mcs51_subbadir not implemented\n");
}

void
mcs51_subbaari(void)
{
	uint8_t A;	
	uint8_t R;
	uint8_t op2;
	uint8_t result;
	uint8_t C = !!(PSW & PSW_CY);
	uint8_t reg = ICODE & 1;
	A = MCS51_GetAcc();
	R = MCS51_GetRegR(reg);
	op2 = MCS51_ReadMem(R);
	result = A - C - op2;
	PSW = PSW & ~(PSW_CY | PSW_AC | PSW_OV);
	PSW |= 	sub8_carry(A,op2,result) | 
	       	sub4_carry(A,op2,result) |
		sub8_overflow(A,op2,result);
	MCS51_SetAcc(result);
	fprintf(stderr,"mcs51_subbaari not implemented\n");
}

void
mcs51_subbadata(void)
{
	uint8_t A;	
	uint8_t data;
	uint8_t result;
	uint8_t C = !!(PSW & PSW_CY);
	data = MCS51_ReadPgmMem(GET_REG_PC);
        SET_REG_PC(GET_REG_PC + 1);
	A = MCS51_GetAcc();
	result = A - C - data;		
	PSW = PSW & ~(PSW_CY | PSW_AC | PSW_OV);
	PSW |= 	sub8_carry(A,data,result) | 
	       	sub4_carry(A,data,result) |
		sub8_overflow(A,data,result);
	MCS51_SetAcc(result);
	fprintf(stderr,"mcs51_subbadata not implemented\n");
}

void
mcs51_swapa(void)
{
	uint8_t A = MCS51_GetAcc();
	A = (A >> 4) | (A << 4);
	MCS51_SetAcc(A);
	fprintf(stderr,"mcs51_swapa not tested\n");
}

void
mcs51_xchar(void)
{
	uint8_t reg = ICODE & 7;
	uint8_t A,R;
	A = MCS51_GetAcc();
	R = MCS51_GetRegR(reg);
	MCS51_SetAcc(R);
	MCS51_SetRegR(A,reg);
	
	fprintf(stderr,"mcs51_xchar not implemented\n");
}

void
mcs51_xchadir(void)
{
	uint8_t dira = MCS51_ReadPgmMem(GET_REG_PC);
	uint8_t A = MCS51_GetAcc();
	SET_REG_PC(GET_REG_PC + 1);
	MCS51_SetAcc(MCS51_ReadMem(dira));
	MCS51_WriteMem(A,dira);
	fprintf(stderr,"mcs51_xchadir not tested\n");
}

void
mcs51_xchaari(void)
{
	uint8_t A = MCS51_GetAcc();
	uint8_t reg = ICODE & 1;
	uint8_t R = MCS51_GetRegR(reg);	
	uint8_t data = MCS51_ReadMem(R);
	MCS51_WriteMem(A,R);
	MCS51_SetAcc(data);
	fprintf(stderr,"mcs51_xchaari not tested\n");
}

void
mcs51_xchdaari(void)
{
	uint8_t reg = ICODE & 1;
	uint8_t A = MCS51_GetAcc();
	uint8_t R = MCS51_GetRegR(reg);
	MCS51_SetAcc((A & 0xf0) | (R & 0xf));
	MCS51_SetRegR((R & 0xf0) | (A & 0xf),R);
	fprintf(stderr,"mcs51_xchdaari not tested\n");
}

void
mcs51_xrlar(void)
{
	int reg = ICODE & 7;
	uint8_t A = MCS51_GetAcc();
	uint8_t R = MCS51_GetRegR(reg);
	uint8_t result = A ^ R;
	MCS51_SetAcc(result);
	fprintf(stderr,"mcs51_xrlar not implemented\n");
}

void
mcs51_undef(void)
{
	fprintf(stderr,"Undefined MCS51 instruction 0x%02x\n",ICODE);
	exit(1);
}

