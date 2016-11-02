/*
 *************************************************************************************************
 *
 * Emulation of Coldfire Instruction Set 
 *
 * state:  Not working 
 * 
 * Copyright 2008 Jochen Karrer. All rights reserved.
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
#include <stdint.h>
#include <cpu_cf.h>
#include <mem_cf.h>
typedef enum {
	Eat_Invalid = -1,
	Eat_Addr,	
	Eat_Reg,
	Eat_Imm
} EaType;
typedef struct {
	EaType ea_type;
	int reg;
	int size;
	uint32_t addr;	
	uint32_t value;
} EAddress;

#define ISNEG(x) ((x)&(1<<31))
#define ISNOTNEG(x) (!((x)&(1<<31)))

static inline uint16_t
add_carry(uint32_t op1,uint32_t op2,uint32_t result) {
        if( ((ISNEG(op1) && ISNEG(op2))
          || (ISNEG(op1) && ISNOTNEG(result))
          || (ISNEG(op2) && ISNOTNEG(result)))) {
                        return CCR_C;
        } else {
                return 0;
        }
}
/*
 * Coldfire has borrow style carry
 * (Add complement and invert carry) 
 */
static inline uint32_t
sub_carry(uint32_t op1,uint32_t op2,uint32_t result) {
        if( ((ISNEG(op1) && ISNOTNEG(op2))
          || (ISNEG(op1) && ISNOTNEG(result))
          || (ISNOTNEG(op2) && ISNOTNEG(result)))) {
                        return 0;
        } else {
                        return CCR_C;
        }
}

static inline uint32_t
sub_overflow(uint32_t op1,uint32_t op2,uint32_t result) {
        if ((ISNEG (op1) && ISNOTNEG (op2) && ISNOTNEG (result))
          || (ISNOTNEG (op1) && ISNEG (op2) && ISNEG (result))) {
                return CCR_V;
        } else {
                return 0;
        }
}


static inline uint16_t
add_overflow(uint32_t op1,uint32_t op2,uint32_t result) {
        if ((ISNEG (op1) && ISNEG (op2) && ISNOTNEG (result))
          || (ISNOTNEG (op1) && ISNOTNEG (op2) && ISNEG (result))) {
                return CCR_V;
        } else {
                return 0;
        }
}


/*
 ****************************************************************
 * Get the value at an effective Address:
 * Read from memory or from register or return the Immediate.
 * Influence of size for reading registers ?????
 * v0
 ****************************************************************
 */

uint32_t
Ea_GetValue(EAddress *ea) {
	switch(ea->ea_type) {
		case Eat_Reg:
			return CF_GetReg(ea->reg);
		case Eat_Addr:
			switch(ea->size) {
				case 8: 
					return CF_MemRead8(ea->addr);
					break;
				case 16: 
					return CF_MemRead16(ea->addr);
					break;
				case 32: 
					return CF_MemRead32(ea->addr);
					break;
				default:
					fprintf(stderr,"Shit, bad data size %d\n",ea->size);
					exit(1);
			}	
			break;
		case Eat_Imm:
			return ea->value;
			
		default:
			fprintf(stderr,"Badly initialized ea_type\n");
			exit(1);
			return 0;
	}
}

/*
 *********************************************************************
 * Get the memory address where an Effective address points to.
 * Not possible for immediates and registers.
 * v0
 *********************************************************************
 */
uint32_t
Ea_GetAddr(EAddress *ea) {
	switch(ea->ea_type) {
		case Eat_Reg:
		case Eat_Imm:
			fprintf(stderr,"A register/immediate is not an address, can not get\n");
			/* some exception ? */
			return 0; 

		case Eat_Addr:
			return ea->addr;
			
		default:
			fprintf(stderr,"Badly initialized ea_type\n");
			exit(1);
			return 0;
	}
}

/*
 ****************************************************************************
 * Ea_SetValue
 *  Set a value to an effective address.
 *  Only possible if memory or register is addressed. 
 *  Not possible for an immediate value
 ****************************************************************************
 */
void
Ea_SetValue(uint32_t value,EAddress *ea) {
	switch(ea->ea_type) {
		case Eat_Reg:
			switch(ea->size) {
				uint32_t oldval;
				case 8:
					oldval = CF_GetReg(ea->reg);
					return CF_SetReg((value & 0xff) | (oldval & 0xffffff00),ea->reg);
				case 16:	
					oldval = CF_GetReg(ea->reg);
					return CF_SetReg((value & 0xffff) | (oldval & 0xffff0000),ea->reg);
					
				case 32:
					return CF_SetReg(value,ea->reg);
				default:
					fprintf(stderr,"Bad size in effective address\n");
			}
			break;
		case Eat_Addr:
			switch(ea->size) {
				case 8: 
					CF_MemWrite8(value,ea->addr);
					break;
				case 16: 
					CF_MemWrite16(value,ea->addr);
					break;
				case 32: 
					CF_MemWrite32(value,ea->addr);
					break;
				default:
					fprintf(stderr,"Shit, bad data size %d\n",ea->size);
			}	
			break;
		case Eat_Imm:
			fprintf(stderr,"Shit, can not write to an immediate value\n");
			break;
			
		default:
			fprintf(stderr,"EA-Type is invalid\n");
			break;
			
	}
}

/*
 **************************************************************************************
 * Ea_Get
 *   Fill data into an effective address structure
 *   Operand Sizes are specified in Ch. 3.2 (Instruction Set Summary) of CFPRM.pdf 
 * v0
 **************************************************************************************
 */

void
Ea_Get(EAddress *ea,uint8_t mode,uint8_t reg,int size) 
{
	uint16_t eword1;
	int xi;
	int shift;
	uint16_t d16;
	ea->ea_type = Eat_Invalid;
	//ea->reg = 0;
	ea->size = size;
	switch(mode) {
		/* Data register direct mode */
		case 0:
			ea->ea_type = Eat_Reg;
			ea->reg = reg;
			return;

		/* Address register direct mode */
		case 1:
			ea->ea_type = Eat_Reg;
			ea->reg = reg+8;
			return;

		/* Address register indirect mode */
		case 2:
			ea->ea_type = Eat_Addr;
			ea->addr = CF_GetRegA(reg);
			return;

		/* Address register indirect mode with postincrement */
		case 3:
			ea->ea_type = Eat_Addr;
			ea->addr = CF_GetRegA(reg);
			CF_SetRegA(ea->addr+(size>>3),reg);
			return;

		/* Address register indirect mode with predecrement */	
		case 4:
			ea->ea_type = Eat_Addr;
			ea->addr = CF_GetRegA(reg) - (size>>3);
			CF_SetRegA(ea->addr,reg);
			return;	

		/*  Address register indirect with signed 16 Bit displacement */
		case 5:
			eword1 = CF_MemRead16(CF_GetRegPC());	
			CF_SetRegPC(CF_GetRegPC()+2);
			ea->ea_type = Eat_Addr;
			ea->addr = (int16_t)eword1 + CF_GetRegA(reg);
			return;

		/* Address register indirect with scaled index and 8 Bit displacement */
		case 6:
			eword1 = CF_MemRead16(CF_GetRegPC());	
			CF_SetRegPC(CF_GetRegPC()+2);
			ea->ea_type = Eat_Addr;
			if((eword1 & (1 << 11)) == 0) {
				fprintf(stderr,"Address error exception not implemented\n");
			}
			xi = (eword1 & 0xf000) >> 12; 
			shift = ((eword1 & 0x0600) >> 9);
			ea->addr = CF_GetRegA(reg) + (CF_GetReg(xi) << shift) + (int8_t)(eword1 & 0xff);
			return;

		case 7:
			switch(reg) {
				/* Absolute short addressing mode */
				case 0:
					ea->addr = (int16_t)CF_MemRead16(CF_GetRegPC());	
					ea->ea_type = Eat_Addr;
					CF_SetRegPC(CF_GetRegPC()+2);
					return;

				/* Absoulute Long addressing mode */	
				case 1:
					ea->addr = CF_MemRead32(CF_GetRegPC());
					ea->ea_type = Eat_Addr;
					CF_SetRegPC(CF_GetRegPC()+4);
					break;

				/* Programm counter indirect with 16 Bit displacement */
				case 2:
					d16 = CF_MemRead16(CF_GetRegPC());	
					ea->ea_type = Eat_Addr;
					ea->addr = (int16_t)d16 + CF_GetRegPC();
					CF_SetRegPC(CF_GetRegPC()+2);
					break;

				/* Programm counter indirect with scaled index and 8 Bit displacement */
				case 3:
					eword1 = CF_MemRead16(CF_GetRegPC());	
					shift = ((eword1 & 0x0600) >> 9);
					xi = (eword1 & 0xf000) >> 12; 
					if((eword1 & (1 << 11)) == 0) {
						fprintf(stderr,"Address error exception not implemented\n");
					}
					ea->ea_type = Eat_Addr;
					ea->addr = CF_GetRegPC() + (CF_GetReg(xi) << shift) + (int8_t)(eword1 & 0xff);
					CF_SetRegPC(CF_GetRegPC()+2);
					break;

				/* Immediate Data */
				case 4:
					ea->ea_type = Eat_Imm;
					switch(size) {
						case 8:
							ea->value = CF_MemRead16(CF_GetRegPC()) & 0xff;
							CF_SetRegPC(CF_GetRegPC()+2);
							break;
						case 16:
							ea->value = CF_MemRead16(CF_GetRegPC());
							CF_SetRegPC(CF_GetRegPC()+2);
							break;
						case 32:
							ea->value = CF_MemRead32(CF_GetRegPC());
							CF_SetRegPC(CF_GetRegPC()+4);
							break;
					}
					break;
				default:
					fprintf(stderr,"Unimplemented ea_reg == %d for mode 7\n",reg);
					// illegal instr ?
					exit(1);
				
			}
			break;

			default:
				fprintf(stderr,"Unimplemented ea_mode == %d\n",mode);
				// illegal instr ?
				exit(1);
				
		
	}

}

/*
 *************************************************************************
 * add_flags
 * calculate flags common to all add operations
 * v0
 *************************************************************************
 */
static inline uint16_t
add_flags(uint32_t op1,uint32_t op2,uint32_t result) {
	
	uint16_t flags = add_carry(op1,op2,result);
	if(flags & CCR_C) {
		flags |= CCR_X;
	}
	flags |= add_overflow(op1,op2,result);
	if(ISNEG(result)) {
		flags |= CCR_N;
	} else if(result == 0) {
		flags |= CCR_Z;
	}
	return flags;
}

/*
 ********************************************************************
 * sub_flags
 *   calculate flags common for all subtract operations
 * v0
 ********************************************************************
 */
static inline uint16_t
sub_flags(uint32_t op1,uint32_t op2,uint32_t result) {
	
	uint16_t flags = sub_carry(op1,op2,result);
	if(flags & CCR_C) {
		flags |= CCR_X;
	}
	flags |= sub_overflow(op1,op2,result);
	if(ISNEG(result)) {
		flags |= CCR_N;
	} else if(result == 0) {
		flags |= CCR_Z;
	}
	return flags;
}
/*
 ***********************************************************
 * Coldfire ADD instruction
 * v0
 ***********************************************************
 */
void
cf_add(void)
{
	uint16_t opmode = (ICODE & 0x1c0) >> 6;
	EAddress ea;	
	int ea_mode = (ICODE & 0x38) >> 3;
	int ea_reg = (ICODE & 7); 
	int reg = (ICODE & 0xe00) >> 9;
	uint32_t op1,op2,result;
	Ea_Get(&ea,ea_mode,ea_reg,32);
	op1 = Ea_GetValue(&ea); 
	op2 = CF_GetRegD(reg);
	result = op1 + op2;
	if(opmode == 2) {
		/* <ea>y + Dx -> Dx */
		CF_SetRegD(result,reg);
	} else if(opmode == 6) {
		/* Dy + <ea>x -> <ea>x */
		Ea_SetValue(result,&ea);	
	} else {
		fprintf(stderr,"Illegal opmode for add instruction\n");
		return;
	}	
	CF_REG_CCR = CF_REG_CCR & ~(CCR_C | CCR_V | CCR_Z | CCR_N | CCR_X);
	CF_REG_CCR |= add_flags(op1,op2,result); 
}

/*
 ****************************************************************
 * Coldfire ADD Address 
 * v0
 ****************************************************************
 */
void
cf_adda(void)
{
	EAddress ea;	
	int ea_mode = (ICODE & 0x38) >> 3;
	int ea_reg = (ICODE & 7); 
	int reg = (ICODE & 0xe00) >> 9;
	uint32_t op1,op2,result;
	Ea_Get(&ea,ea_mode,ea_reg,32);
	op1 = Ea_GetValue(&ea); 
	op2 = CF_GetRegA(reg);
	result = op1 + op2;
	CF_SetRegA(result,reg);
	CF_REG_CCR = CF_REG_CCR & ~(CCR_C | CCR_V | CCR_Z | CCR_N | CCR_X);
	CF_REG_CCR |= add_flags(op1,op2,result); 
}

/*
 ***********************************************************
 * Coldfire ADD Immediate
 * v0
 ***********************************************************
 */
void
cf_addi(void)
{
	int dreg = (ICODE & 0x7);
	uint32_t op1,op2,result;
	op1 = CF_MemRead32(CF_GetRegPC());
	op2 = CF_GetRegD(dreg);	
	result = op1 + op2;
	CF_SetRegPC(CF_GetRegPC()+4);
	CF_SetRegD(result,dreg);
	CF_REG_CCR = CF_REG_CCR & ~(CCR_C | CCR_V | CCR_Z | CCR_N | CCR_X);
	CF_REG_CCR |= add_flags(op1,op2,result); 
}

/*
 ****************************************************************
 * Coldfire ADD quick
 * Immediate Data + Destination -> Destination
 * v0
 ****************************************************************
 */
void
cf_addq(void)
{
	EAddress ea;	
	uint32_t op2,result;
	uint32_t op1 = (ICODE & 0x0e00) >> 9;
	int ea_mode = (ICODE & 0x38) >> 3;
	int ea_reg = (ICODE & 7); 
	if(op1 == 0) {
		op1 = 8;
	}
	Ea_Get(&ea,ea_mode,ea_reg,32);
	op2 = Ea_GetValue(&ea); 
	result = op1 + op2;	
	Ea_SetValue(result,&ea); 
	CF_REG_CCR = CF_REG_CCR & ~(CCR_C | CCR_V | CCR_Z | CCR_N | CCR_X);
	CF_REG_CCR |= add_flags(op1,op2,result); 
}

/*
 ***************************************************************
 * Coldfire ADD Extended
 * v0
 ***************************************************************
 */
void
cf_addx(void)
{
	int dy = ICODE & 0x7;
	int dx = (ICODE & 0x0e00) >> 9;
	uint32_t op1,op2,result;
	uint16_t flags;
	op1 = CF_GetRegD(dy);
	op2 = CF_GetRegD(dx);
	result = op1 + op2;
	if(CF_REG_CCR & CCR_X) {
		result += 1;
	}
	CF_SetRegD(result,dx);
	flags = add_carry(op1,op2,result);
	if(flags & CCR_C) {
		flags |= CCR_X;
	}
	flags |= add_overflow(op1,op2,result);
	if(ISNEG(result)) {
		flags |= CCR_N;
	} 
	CF_REG_CCR = CF_REG_CCR & ~(CCR_C | CCR_V |  CCR_N | CCR_X);
	CF_REG_CCR |= flags;
	if(result != 0) {
		CF_REG_CCR &= ~CCR_Z;
	}
}

/*
 **********************************************************
 * Coldfire AND
 * v0
 **********************************************************
 */
void
cf_and(void)
{
	uint16_t opmode = (ICODE & 0x1c0) >> 6;
	EAddress ea;	
	int ea_mode = (ICODE & 0x38) >> 3;
	int ea_reg = (ICODE & 7); 
	int reg = (ICODE & 0xe00) >> 9;
	uint32_t op1,op2,result;
	Ea_Get(&ea,ea_mode,ea_reg,32);
	op1 = Ea_GetValue(&ea); 
	op2 = CF_GetRegD(reg);
	result = op1 & op2;
	if(opmode == 2) {
		/* <ea>y + Dx -> Dx */
		CF_SetRegD(result,reg);
	} else if(opmode == 6) {
		/* Dy + <ea>x -> <ea>x */
		Ea_SetValue(result,&ea);	
	} else {
		// illegal opmode 
	}	
	CF_REG_CCR = CF_REG_CCR & ~(CCR_C | CCR_V | CCR_Z | CCR_N );
	if(ISNEG(result))  {
		CF_REG_CCR |=  CCR_N;
	} else if(result == 0) {
		CF_REG_CCR |=  CCR_Z;
	}
}

/*
 *******************************************************************
 * Coldfire ANDI
 * v0
 *******************************************************************
 */
void
cf_andi(void)
{
	int dreg = (ICODE & 0x7);
	uint32_t op1,op2,result;
	op1 = CF_MemRead32(CF_GetRegPC());
	op2 = CF_GetRegD(dreg);	
	result = op1 & op2;
	CF_SetRegPC(CF_GetRegPC()+4);
	CF_SetRegD(result,dreg);
	CF_REG_CCR = CF_REG_CCR & ~(CCR_C | CCR_V | CCR_Z | CCR_N);
	if(ISNEG(result)) {
		CF_REG_CCR |=  CCR_N;
	} else if(result == 0) {
		CF_REG_CCR |=  CCR_Z;
	}
}

/*
 **********************************************************************
 * cf_asri
 * 	Aritmethic Shift Right Immediate (1-8)
 * v0
 **********************************************************************
 */
void
cf_asri(void)
{
	int dx = ICODE & 7;
	int count = (ICODE & 0x0e00) >> 9;
	int32_t Dx;
	int shiftout;
	if(count == 0) {
		count = 8;
	}
	Dx = CF_GetReg(dx);
	shiftout = (Dx >> (count - 1)) & 1;
	Dx = Dx >> count;
	CF_REG_CCR = CF_REG_CCR & ~(CCR_X | CCR_C | CCR_V | CCR_Z | CCR_N);
	if(shiftout) {
		CF_REG_CCR |= CCR_X | CCR_C;
	}	
	if(ISNEG(Dx)) {
		CF_REG_CCR |= CCR_N;
	} else if(Dx == 0) {
		CF_REG_CCR |= CCR_Z;
	}
	CF_SetRegD(Dx,dx);
}

/*
 ************************************************************************
 * cf_asrr
 *     Coldfire Arithmethic shift right by a register value.
 * v0
 ************************************************************************
 */
void
cf_asrr(void)
{
	int shiftout;
	int dx = ICODE & 7;
	int dy = (ICODE & 0x0e00) >> 9;
	unsigned int shift;
	int32_t Dx;
	uint32_t Dy;
	Dx = CF_GetReg(dx);
	Dy = CF_GetReg(dy);
	shift = Dy & 0x3f;
	if(shift < 32) {
		shiftout = (Dx >> (shift-1)) & 1;
		Dx = Dx >> shift;
	} else if(shift >= 32) {
		/* I'm not totally sure for shift count > 32 */
		if(ISNEG(Dx)) {
			shiftout = 1;
			Dx = 0xffffffff;
		} else {
			shiftout = 0;
			Dx = 0;
		}	
	}
	if(shift == 0) {
		CF_REG_CCR = CF_REG_CCR & ~(CCR_C | CCR_V | CCR_Z | CCR_N);
	} else {
		CF_REG_CCR = CF_REG_CCR & ~(CCR_X | CCR_C | CCR_V | CCR_Z | CCR_N);
		if(shiftout) {
			CF_REG_CCR |= CCR_X | CCR_C;
		}	
	}
	if(ISNEG(Dx)) {
		CF_REG_CCR |= CCR_N;
	} else if(Dx == 0) {
		CF_REG_CCR |= CCR_Z;
	}
	CF_SetRegD(Dx,dx);
}

/*
 **********************************************************
 * cf_asli
 * 	Arithmetic shift left by an immediate (1-8)
 * v0
 **********************************************************
 */
void
cf_asli(void)
{
	int dx = ICODE & 7;
	uint32_t Dx;
	int count = (ICODE & 0x0e00) >> 9;
	uint32_t shiftout;
	if(count == 0) {
		count = 8;
	}
	Dx = CF_GetRegD(dx);
	shiftout = (Dx >> (32-count)) & 1;
	Dx = Dx << count;	
	CF_REG_CCR = CF_REG_CCR & ~(CCR_X | CCR_C | CCR_V | CCR_Z | CCR_N);
	if(shiftout) {
		CF_REG_CCR = CCR_X | CCR_C;
	}
	if(ISNEG(Dx)) {
		CF_REG_CCR |= CCR_N;
	} else if(Dx == 0) {
		CF_REG_CCR |= CCR_Z;
	}
	CF_SetRegD(Dx,dx);
}

/*
 *************************************************************
 * cf_aslr
 * v0
 *************************************************************
 */
void
cf_aslr(void)
{
	int shiftout;
	int dx = ICODE & 7;
	int dy = (ICODE & 0x0e00) >> 9;
	unsigned int shift;
	int32_t Dx;
	uint32_t Dy;
	Dx = CF_GetReg(dx);
	Dy = CF_GetReg(dy);
	shift = Dy & 0x3f;
	if(shift < 32) {
		shiftout = (Dx >> (32-shift)) & 1;
		Dx = Dx << shift;	
	} else if(shift == 32) {
		shiftout = (Dx & 1);
		Dx = 0;	
	} else {
		shiftout = 0;
		Dx = 0;
	}
	if(shift == 0) {
		CF_REG_CCR = CF_REG_CCR & ~(CCR_C | CCR_V | CCR_Z | CCR_N);
	} else {
		CF_REG_CCR = CF_REG_CCR & ~(CCR_X | CCR_C | CCR_V | CCR_Z | CCR_N);
		if(shiftout) {
			CF_REG_CCR |= CCR_X | CCR_C;
		}	
	}
	if(ISNEG(Dx)) {
		CF_REG_CCR |= CCR_N;
	} else if(Dx == 0) {
		CF_REG_CCR |= CCR_Z;
	}
	CF_SetRegD(Dx,dx);
}

#define CC_TRUE	 (0x0)
#define CC_FALSE (0x1)
#define CC_CC	 (0x4)
#define CC_CS	 (0x5)
#define CC_EQ	 (0x7)
#define CC_GE	 (0xc)
#define CC_GT	 (0xe)
#define CC_HI	 (0x2)
#define CC_LE	 (0xf)
#define CC_LS	 (0x3)
#define CC_LT	 (0xd)
#define CC_MI	 (0xb)
#define CC_NE	 (0x6)
#define CC_PL	 (0xa)
#define CC_VC	 (0x8)
#define CC_VS	 (0x9)

static uint8_t condition_tab[256];

/*
 ************************************************************************************
 * Init condition tab
 * The condition code is in the lower 4 bits and the flags of the status register 
 * are in the upper 4 index of the index 
 * v0
 ************************************************************************************
 */
void
cf_init_condition_tab(void) 
{
	int i;
	for(i=0;i<256;i++) {
		int result;
		int ccode = i & 0xf;
		int ccr = (i>>4) & 0xf;
		int ccr_C = !!(ccr & CCR_C);
		int ccr_N = !!(ccr & CCR_N);
		int ccr_V = !!(ccr & CCR_V);
		int ccr_Z = !!(ccr & CCR_Z);
		switch(ccode) {
			case CC_TRUE:
				result = 1;
				break;
			case CC_FALSE:
				result = 0;
				break;
			case CC_CC:
				result = !ccr_C;
				break;
			case CC_CS:
				result = ccr_C;
				break;
			case CC_EQ:
				result = ccr_Z;
				break;
			case CC_GE:
				result = (ccr_N && ccr_V) || (!ccr_N && !ccr_V);
				break;

			case CC_GT:
				result = (ccr_N && ccr_V && !ccr_Z) ||
					 (!ccr_N && !ccr_V && !ccr_Z);
				break;

			case CC_HI:
				result = !ccr_C && !ccr_Z;
				break;

			case CC_LE:
				result = ccr_Z || (ccr_N && !ccr_V) || (!ccr_N && ccr_V);
				break;

			case CC_LS:
				result = ccr_C || ccr_Z;
				break;

			case CC_LT:
				result = (ccr_N && !ccr_V) || (!ccr_N && ccr_V);
				break;

			case CC_MI:
				result = ccr_N;
				break;

			case CC_NE:
				result = !ccr_Z;
				break;

			case CC_PL:
				result = !ccr_N;	
				break;

			case CC_VC:
				result = !ccr_V;
				break;

			case CC_VS:
				result = ccr_V;
				break;
			default:
				fprintf(stderr,"shit, should never happen\n");
				exit(1);
			
		}
		condition_tab[i] = result;
	}
}

#define CHECK_CONDITION(cond,ccr) condition_tab[((cond) & 0xf) | (((ccr) & 0xf) << 4)]

/*
 *****************************************************************************************
 * Branch if condition is true
 * continues at PC + displacement (after PC is incremented by size of extension words)
 * v0 
 *****************************************************************************************
 */
void
cf_bcc(void)
{
	int ccode = (ICODE & 0x0f00) >> 8;
	int8_t disp8 = ICODE & 0xff;	
	int32_t disp32;
	uint32_t saved_pc = CF_GetRegPC();
	uint32_t nia = saved_pc;
	if(disp8 == 0) {
		disp32 = (int16_t) CF_MemRead16(CF_GetRegPC());
		nia += 2;
	} else if(disp8 == (int8_t)0xff) {
		disp32 = CF_MemRead32(CF_GetRegPC());
		nia += 4;
	} else {
		disp32 = disp8; /* sign extend */
	}
	if(CHECK_CONDITION(ccode,CF_REG_CCR)) {
		CF_SetRegPC(saved_pc+disp32);
	} else {
		CF_SetRegPC(nia);
	}
}

/*
 **************************************************************************************
 * BCHG Test a Bit and Change
 * v0
 **************************************************************************************
 */
void
cf_bchgi(void)
{
	EAddress ea;	
	int size;
	int ea_mode = (ICODE & 0x38) >> 3;
	int ea_reg = (ICODE & 7); 
	uint32_t val;
	uint8_t bit = (CF_MemRead16(CF_GetRegPC()));
	if(ea_mode == 0) {
		bit = bit & 31;
		size = 32;
	} else {
		bit = bit & 7;
		size = 8;
	}
	CF_SetRegPC(CF_GetRegPC()+2);
	Ea_Get(&ea,ea_mode,ea_reg,size);
	val = Ea_GetValue(&ea); 
	if(val & (1<<bit)) {
		CF_REG_CCR = CF_REG_CCR &~ CCR_Z;
	} else {
		CF_REG_CCR = CF_REG_CCR | CCR_Z;
	}
	val = val ^ (1<<bit);
	Ea_SetValue(val,&ea);
}

/* 
 ***************************************************************
 * BCHG register version (Bit number is in a register)
 * v0
 ***************************************************************
 */
void
cf_bchgr(void)
{
	EAddress ea;	
	int ea_mode = (ICODE & 0x38) >> 3;
	int ea_reg = (ICODE & 7); 
	int reg = (ICODE & 0x0e00) >> 9;
	int size;
	uint32_t val;
	uint32_t bit = CF_GetRegD(reg); 
	if(ea_mode == 0) {
		bit = bit & 31;
		size = 32;
	} else {
		bit = bit & 7;
		size = 8;
	}
	Ea_Get(&ea,ea_mode,ea_reg,size);
	val = Ea_GetValue(&ea); 
	if(val & (1<<bit)) {
		CF_REG_CCR = CF_REG_CCR &~ CCR_Z;
	} else {
		CF_REG_CCR = CF_REG_CCR | CCR_Z;
	}
	val = val ^ (1<<bit);
	Ea_SetValue(val,&ea);
}

/*
 ******************************************************************************
 * BCLR Test a Bit and Clear
 * v0
 ******************************************************************************
 */
void
cf_bclri(void)
{
	EAddress ea;	
	int size;
	int ea_mode = (ICODE & 0x38) >> 3;
	int ea_reg = (ICODE & 7); 
	uint32_t val;
	uint16_t bit = (CF_MemRead16(CF_GetRegPC()));
	if(ea_mode == 0) {
		bit = bit & 31;
		size = 32;
	} else {
		bit = bit & 7;
		size = 8;
	}
	CF_SetRegPC(CF_GetRegPC()+2);
	Ea_Get(&ea,ea_mode,ea_reg,size);
	val = Ea_GetValue(&ea); 
	if(val & (1<<bit)) {
		CF_REG_CCR = CF_REG_CCR &~ CCR_Z;
	} else {
		CF_REG_CCR = CF_REG_CCR | CCR_Z;
	}
	val = val &~ (1<<bit);
	Ea_SetValue(val,&ea);
}
/*
 ***********************************************************************
 * BCLR Test a Bit and Clear. Bit number is in a register 
 * v0
 ***********************************************************************
 */
void
cf_bclrr(void)
{
	EAddress ea;	
	int ea_mode = (ICODE & 0x38) >> 3;
	int ea_reg = (ICODE & 7); 
	int reg = (ICODE & 0x0e00) >> 9;
	int size;
	uint32_t val;
	uint32_t bit = CF_GetRegD(reg); 
	if(ea_mode == 0) {
		bit = bit & 31;
		size = 32;
	} else {
		bit = bit & 7;
		size = 8;
	}
	Ea_Get(&ea,ea_mode,ea_reg,size);
	val = Ea_GetValue(&ea); 
	if(val & (1<<bit)) {
		CF_REG_CCR = CF_REG_CCR &~ CCR_Z;
	} else {
		CF_REG_CCR = CF_REG_CCR | CCR_Z;
	}
	val = val & ~(1<<bit);
	Ea_SetValue(val,&ea);
}

/*
 *******************************************************************
 * bitreverse 32
 * v0
 *******************************************************************
 */
static inline uint32_t
bitreverse32(uint32_t x)
{
        x = (x >> 16) | (x << 16);
        x = ((x >> 8) & 0x00ff00ff) | ((x << 8) & 0xff00ff00);
        x = ((x >> 4) & 0x0f0f0f0f) | ((x << 4) & 0xf0f0f0f0);
        x = ((x >> 2) & 0x33333333) | ((x << 2) & 0xcccccccc);
        x = ((x >> 1) & 0x55555555) | ((x << 1) & 0xaaaaaaaa);
        return x;
}

/*
 *****************************************************************
 * Bit-Reverse the contents of a 32 Bit Data register
 * v0
 *****************************************************************
 */
void
cf_bitrev(void)
{
	int reg = ICODE & 7;
	uint32_t val = CF_GetRegD(reg);
	val = bitreverse32(val);
	CF_SetRegD(val,reg);
}

/*
 *********************************************************************
 * BSET Test a Bit and Set
 * v0
 *********************************************************************
 */
void
cf_bseti(void)
{
	EAddress ea;	
	int size;
	int ea_mode = (ICODE & 0x38) >> 3;
	int ea_reg = (ICODE & 7); 
	uint32_t val;
	uint16_t bit = (CF_MemRead16(CF_GetRegPC()));
	if(ea_mode == 0) {
		bit = bit & 31;
		size = 32;
	} else {
		bit = bit & 7;
		size = 8;
	}
	CF_SetRegPC(CF_GetRegPC()+2);
	Ea_Get(&ea,ea_mode,ea_reg,size);
	val = Ea_GetValue(&ea); 
	if(val & (1<<bit)) {
		CF_REG_CCR = CF_REG_CCR &~ CCR_Z;
	} else {
		CF_REG_CCR = CF_REG_CCR | CCR_Z;
	}
	val = val | (1<<bit);
	Ea_SetValue(val,&ea);
}

/*
 *********************************************************************
 * BSET set a bit specified in a data register
 * v0
 *********************************************************************
 */
void
cf_bsetr(void)
{
	EAddress ea;	
	int ea_mode = (ICODE & 0x38) >> 3;
	int ea_reg = (ICODE & 7); 
	int reg = (ICODE & 0x0e00) >> 9;
	int size;
	uint32_t val;
	uint32_t bit = CF_GetRegD(reg); 
	if(ea_mode == 0) {
		bit = bit & 31;
		size = 32;
	} else {
		bit = bit & 7;
		size = 8;
	}
	Ea_Get(&ea,ea_mode,ea_reg,size);
	val = Ea_GetValue(&ea); 
	if(val & (1<<bit)) {
		CF_REG_CCR = CF_REG_CCR &~ CCR_Z;
	} else {
		CF_REG_CCR = CF_REG_CCR | CCR_Z;
	}
	val = val | (1<<bit);
	Ea_SetValue(val,&ea);
}

/*
 *****************************************************************
 * BSR Branch to subroutine 
 * v0
 *****************************************************************
 */
void
cf_bsr(void)
{
	int8_t disp8 = ICODE & 0xff;	
	int32_t disp32;
	uint32_t nia = CF_GetRegPC();
	uint32_t pc = nia;
	if(disp8 == 0) {
		disp32 = (int16_t)CF_MemRead16(CF_GetRegPC());
		nia+=2;
	}  else if(disp8 == (int8_t)0xff) {
		disp32 = CF_MemRead32(CF_GetRegPC());
		nia+=4;
	} else {
		disp32 = disp8; /* sign extend */
	}
	Push4(nia); 
	CF_SetRegPC(pc+disp32);
}

/*
 ***************************************************************************
 * BTST Test a Bit. Bit number is an immediate in an extension word
 * v0
 ***************************************************************************
 */
void
cf_btsti(void)
{
	EAddress ea;	
	int size;
	int ea_mode = (ICODE & 0x38) >> 3;
	int ea_reg = (ICODE & 7); 
	uint32_t val;
	uint16_t bit = (CF_MemRead16(CF_GetRegPC()));
	if(ea_mode == 0) {
		bit = bit & 31;
		size = 32;
	} else {
		bit = bit & 7;
		size = 8;
	}
	CF_SetRegPC(CF_GetRegPC()+2);
	Ea_Get(&ea,ea_mode,ea_reg,size);
	val = Ea_GetValue(&ea); 
	if(val & (1<<bit)) {
		CF_REG_CCR = CF_REG_CCR &~ CCR_Z;
	} else {
		CF_REG_CCR = CF_REG_CCR | CCR_Z;
	}
}

/*
 *****************************************************************
 * Bit test. Bit number is in a data register
 * v0
 *****************************************************************
 */
void
cf_btstr(void)
{
	EAddress ea;	
	int ea_mode = (ICODE & 0x38) >> 3;
	int ea_reg = (ICODE & 7); 
	int reg = (ICODE & 0x0e00) >> 9;
	int size;
	uint8_t val;
	uint32_t bit = CF_GetRegD(reg); 
	if(ea_mode == 0) {
		bit = bit & 31;
		size = 32;
	} else {
		bit = bit & 7;
		size = 8;
	}
	Ea_Get(&ea,ea_mode,ea_reg,size);
	val = Ea_GetValue(&ea); 
	if(val & (1<<bit)) {
		CF_REG_CCR = CF_REG_CCR &~ CCR_Z;
	} else {
		CF_REG_CCR = CF_REG_CCR | CCR_Z;
	}
}

/*
 * bytereverse. Swap the bytes in a 32 Bit word
 */
static inline uint32_t
bytereverse32(uint32_t x)
{
        x = (x >> 16) | (x << 16);
        x = ((x >> 8) & 0x00ff00ff) | ((x << 8) & 0xff00ff00);
        return x;
}

/* 
 **************************************************************
 * Bytereverse instruction from ISA_C 
 * v0
 **************************************************************
 */
void
cf_byterev(void)
{
	int reg = ICODE & 7;
	uint32_t val = CF_GetRegD(reg);
	val = bytereverse32(val);
	CF_SetRegD(val,reg);
}

/*
 *************************************************************
 * Clear an operand (0->EA)
 * v0
 *************************************************************
 */
void
cf_clr(void)
{
	EAddress ea;	
	int ea_mode = (ICODE & 0x38) >> 3;
	int ea_reg = (ICODE & 7); 
	int size = 8 << ((ICODE & 0xc0) >> 6);
	Ea_Get(&ea,ea_mode,ea_reg,size);
	Ea_SetValue(0,&ea);
	CF_REG_CCR &= ~(CCR_N | CCR_V | CCR_C);
	CF_REG_CCR |= CCR_Z; 
}

/*
 ******************************************************************************************
 * cf_cmp
 * 	Shity Coldfire manual is unprecise about the word and byte wide versions of cmp
 *	so I omitted them totally.
 * v0
 ******************************************************************************************
 */
void
cf_cmp(void)
{
	EAddress ea;	
	uint32_t src;
	uint32_t Dx;
	uint32_t result;
	uint16_t flags;
	int ea_mode = (ICODE & 0x38) >> 3;
	int ea_reg = (ICODE & 7); 
	int reg = (ICODE & 0x0e00) >> 9;	
	Ea_Get(&ea,ea_mode,ea_reg,32);
	src = Ea_GetValue(&ea); 
	Dx = CF_GetRegD(reg);
	result = Dx - src;
	
	flags = sub_carry(Dx,src,result);
	flags |= sub_overflow(Dx,src,result);
	if(ISNEG(result)) {
		flags |= CCR_N;
	} else if(result == 0) {
		flags |= CCR_Z;
	}
	CF_REG_CCR = CF_REG_CCR & ~(CCR_C | CCR_V | CCR_Z | CCR_N);
	CF_REG_CCR |= flags;		
}

/*
 ****************************************************************
 * CMPA  Compare Address
 ****************************************************************
 */
void
cf_cmpa(void)
{
	EAddress ea;	
	uint32_t src;
	uint32_t Ax;
	uint32_t result;
	uint16_t flags;
	int ea_mode = (ICODE & 0x38) >> 3;
	int ea_reg = (ICODE & 7); 
	int reg = (ICODE & 0x0e00) >> 9;	
	Ea_Get(&ea,ea_mode,ea_reg,32);
	src = Ea_GetValue(&ea); 
	Ax = CF_GetRegA(reg);
	result = Ax - src;
	
	flags = sub_carry(Ax,src,result);
	flags |= sub_overflow(Ax,src,result);
	if(ISNEG(result)) {
		flags |= CCR_N;
	} else if(result == 0) {
		flags |= CCR_Z;
	}
	CF_REG_CCR = CF_REG_CCR & ~(CCR_C | CCR_V | CCR_Z | CCR_N);
	CF_REG_CCR |= flags;		
}
/*
 *****************************************************************
 * CMP Compare Immediate
 * v0 
 *****************************************************************
 */
void
cf_cmpi(void)
{
	int size = (ICODE & 0xC0) >> 6;
	int dx = ICODE & 0x7;
	uint16_t flags;
	uint32_t Dx;
	uint32_t imm,result;
	Dx = CF_GetRegD(dx);
	if(size == 0) { /* Byte */
		fprintf(stderr,"cmpi.b not implemented\n");
		return;
	} else if(size == 1) { /* word */
		fprintf(stderr,"cmpi.w not implemented\n");
		return;
	} else if(size == 2) { /* longword */
		imm = CF_MemRead32(CF_GetRegPC());
		CF_SetRegPC(CF_GetRegPC()+4);
	} else {
		fprintf(stderr,"Illegal size in cmpi instruction\n");
		// undefined instruction
		return;
	}
	result = Dx - imm;
	
	flags = sub_carry(Dx,imm,result);
	flags |= sub_overflow(Dx,imm,result);
	if(ISNEG(result)) {
		flags |= CCR_N;
	} else if(result == 0) {
		flags |= CCR_Z;
	}
	CF_REG_CCR = CF_REG_CCR & ~(CCR_C | CCR_V | CCR_Z | CCR_N);
	CF_REG_CCR |= flags;		
}

/*
 **********************************************************************
 * cf_divs_w
 * v0	
 **********************************************************************
 */
void
cf_divs_w(void)
{
	EAddress ea;	
	int16_t src;
	int32_t Dx;
	int32_t quot;
	int32_t remainder;
	uint32_t result;
	int ea_mode = (ICODE & 0x38) >> 3;
	int ea_reg = (ICODE & 7); 
	int dx = (ICODE & 0x0e00) >> 9;	
	Ea_Get(&ea,ea_mode,ea_reg,16);
	src = Ea_GetValue(&ea); 
	Dx = CF_GetRegD(dx);	
	if(src == 0) {
		/* do some exception is missing here */
		return;
	} else {
		quot = Dx / src;
		remainder = Dx % src;
	}
	if((quot > 0x7fff) | (quot < -0x8000)) {
		/* overflow */
		CF_REG_CCR = CF_REG_CCR & ~(CCR_N | CCR_Z | CCR_C); 
		CF_REG_CCR |= CCR_V;
		/* If an overflow is detected the destination register is untouched ! */
		return;
	} else {
		result = (int16_t)quot | ((remainder & 0xffff) << 16);
		CF_REG_CCR &= ~(CCR_V | CCR_C | CCR_Z | CCR_N);
		if(quot == 0) {
			CF_REG_CCR |= CCR_Z;
		} else if(quot < 0) {
			CF_REG_CCR |= CCR_N;
		}
		CF_SetRegD(result,dx);
	}
}

/*
 * Signed Divide Long
 * v0
 */
void
cf_divs_l(uint16_t secword)
{
	EAddress ea;	
	int32_t src;
	int32_t Dx;
	int32_t quot;
	int32_t remainder;
	uint32_t result;
	int ea_mode = (ICODE & 0x38) >> 3;
	int ea_reg = (ICODE & 7); 
	int dx = (secword & 0x7000) >> 12;
	int dw = (secword & 7);
	Ea_Get(&ea,ea_mode,ea_reg,32);
	src = Ea_GetValue(&ea); 
	Dx = CF_GetRegD(dx);	
	if(src == 0) {
		/* do some exception is missing here */
		return;
	} else {
		quot = Dx / src;
		remainder = Dx % src;
	}
	result = quot;
	CF_REG_CCR &= ~(CCR_V | CCR_C | CCR_Z | CCR_N);
	if(result == 0) {
		CF_REG_CCR |= CCR_Z;
	} else if(ISNEG(result)) {
		CF_REG_CCR |= CCR_N;
	} else if(result == 0x80000000) {
		CF_REG_CCR |= CCR_V;
	}
	if(dw == dx) {
		CF_SetRegD(quot,dx);
	} else {
		CF_SetRegD(remainder,dw);
	}
}

/*
 * divide unsigned
 */
void
cf_divu_w(void)
{
	EAddress ea;	
	uint16_t src;
	uint32_t Dx;
	uint32_t quot;
	uint32_t remainder;
	uint32_t result;
	int ea_mode = (ICODE & 0x38) >> 3;
	int ea_reg = (ICODE & 7); 
	int dx = (ICODE & 0x0e00) >> 9;	
	Ea_Get(&ea,ea_mode,ea_reg,16);
	src = Ea_GetValue(&ea); 
	Dx = CF_GetRegD(dx);	
	if(src == 0) {
		/* do some exception is missing here */
		return;
	} else {
		quot = Dx / src;
		remainder = Dx % src;
	}
	if((quot > 0xffff)) {
		/* overflow */
		CF_REG_CCR = CF_REG_CCR & ~(CCR_N | CCR_Z | CCR_C); 
		CF_REG_CCR |= CCR_V;
		/* Do not touch Dx in case of overflow */
		return;
	} else {
		result = (uint16_t)quot | ((remainder & 0xffff) << 16);
		CF_REG_CCR &= ~(CCR_V | CCR_C | CCR_Z | CCR_N);
		if(quot == 0) {
			CF_REG_CCR |= CCR_Z;
		} else if(quot & 0x8000) {
			CF_REG_CCR |= CCR_N;
		}
		CF_SetRegD(result,dx);
	}
}

/*
 * divide unsigned long
 */
void
cf_divu_l(uint16_t secword)
{
	EAddress ea;	
	uint32_t src;
	uint32_t Dx;
	uint32_t quot;
	uint32_t remainder;
	int ea_mode = (ICODE & 0x38) >> 3;
	int ea_reg = (ICODE & 7); 
	int dx = (secword & 0x7000) >> 12; 
	int dw = secword & 7;
	Ea_Get(&ea,ea_mode,ea_reg,32);
	src = Ea_GetValue(&ea); 
	Dx = CF_GetRegD(dx);	
	if(src == 0) {
		/* do some exception is missing here */
		return;
	} else {
		quot = Dx / src;
		remainder = Dx % src;
	}
	CF_REG_CCR &= ~(CCR_V | CCR_C | CCR_Z | CCR_N);
	if(quot == 0) {
		CF_REG_CCR |= CCR_Z;
	} else if(ISNEG(quot)) {
		CF_REG_CCR |= CCR_N;
	}
	if(dx == dw) {
		CF_SetRegD(quot,dx);
	} else {
		CF_SetRegD(remainder,dw);
	}
}

/*
 * Long version needs second word to decide if signed or unsigned
 */
void 
cf_div_l(void)
{
	uint16_t secword = CF_MemRead16(CF_GetRegPC());
	if(secword & (1<<11)) {
		cf_divs_l(secword);
	} else {
		cf_divu_l(secword);
	}
	/* Increment PC here because div may throw an exception with CIA */
	CF_SetRegPC(CF_GetRegPC()+2);
}

/*
 ****************************************************************
 * EOR
 * v0
 ****************************************************************
 */
void
cf_eor(void)
{
	EAddress ea;	
	int ea_mode = (ICODE & 0x38) >> 3;
	int ea_reg = (ICODE & 7); 
	int reg = (ICODE & 0xe00) >> 9;
	uint32_t op1,op2,result;
	Ea_Get(&ea,ea_mode,ea_reg,32);
	op1 = Ea_GetValue(&ea); 
	op2 = CF_GetRegD(reg);
	result = op1 ^ op2;
	Ea_SetValue(result,&ea);	
	CF_REG_CCR = CF_REG_CCR & ~(CCR_C | CCR_V | CCR_Z | CCR_N );
	if(ISNEG(result))  {
		CF_REG_CCR |=  CCR_N;
	} else if(result == 0) {
		CF_REG_CCR |=  CCR_Z;
	}
}

/*
 * EOR immediate (longword)
 * v0
 */
void
cf_eori(void)
{
	int dreg = (ICODE & 0x7);
	uint32_t op1,op2,result;
	op1 = CF_MemRead32(CF_GetRegPC());
	op2 = CF_GetRegD(dreg);	
	result = op1 ^ op2;
	CF_SetRegPC(CF_GetRegPC()+4);
	CF_SetRegD(result,dreg);
	CF_REG_CCR = CF_REG_CCR & ~(CCR_C | CCR_V | CCR_Z | CCR_N);
	if(ISNEG(result))  {
		CF_REG_CCR |=  CCR_N;
	} else if(result == 0) {
		CF_REG_CCR |=  CCR_Z;
	}
}

/*
 *******************************************************************
 * EXT Sign Extend
 * v0
 *******************************************************************
 */
void
cf_ext(void)
{
	int opmode = (ICODE & 0x1c0) >> 6;
	int dx = (ICODE & 7);
	uint32_t Dx = CF_GetRegD(dx);
	uint32_t result;
	if(opmode == 2) { /* byte to word */
		Dx = (Dx & 0xffff0000) | (uint32_t)(int16_t)(int8_t)Dx;
	} else if (opmode == 3) { /* word to longword */
		Dx = (int32_t)(int16_t)Dx; 
	} else if (opmode == 7) { /* byte to longword */
		Dx = (int32_t)(int8_t)Dx; 
	} else {
		fprintf(stderr,"cf_ext illegal opmode\n");
		return;
	}	
	result = Dx; /* is this true or are only the lower 16 bit used for opmode 2 ??? */	
	CF_REG_CCR = CF_REG_CCR & ~(CCR_C | CCR_V | CCR_Z | CCR_N);
	if(ISNEG(result))  {
		CF_REG_CCR |=  CCR_N;
	} else if(result == 0) {
		CF_REG_CCR |=  CCR_Z;
	}
}

/*
 * FF1 Find first One in Register
 * v0
 */
void
cf_ff1(void)
{
	int dx = ICODE & 7;
	uint32_t Dx = CF_GetRegD(dx);
	int i;
	for(i=0;i<32;i++,Dx = Dx>>1) {
		if(Dx & 0x80000000) {
			break;
		}
	}
	CF_SetRegD(i,dx);
	CF_REG_CCR = CF_REG_CCR & ~(CCR_C | CCR_V | CCR_Z | CCR_N);
	if(ISNEG(Dx))  {
		CF_REG_CCR |=  CCR_N;
	} else if(Dx == 0) {
		CF_REG_CCR |=  CCR_Z;
	}
}

void
cf_illegal(void)
{
	fprintf(stderr,"cf_illegal not implemented\n");
}

/*
 **********************************************************
 * JMP 
 * v0
 **********************************************************
 */
void
cf_jmp(void)
{
	EAddress ea;	
	uint32_t addr;
	int ea_mode = (ICODE & 0x38) >> 3;
	int ea_reg = (ICODE & 7); 
	Ea_Get(&ea,ea_mode,ea_reg,32);
	addr = Ea_GetAddr(&ea); 
	CF_SetRegPC(addr);
}

/*
 *********************************************************
 * JSR
 * v0
 *********************************************************
 */
void
cf_jsr(void)
{
	EAddress ea;	
	uint32_t addr;
	int ea_mode = (ICODE & 0x38) >> 3;
	int ea_reg = (ICODE & 7); 
	Ea_Get(&ea,ea_mode,ea_reg,32);
	addr = Ea_GetAddr(&ea); 
	Push4(CF_GetRegPC()); 
	CF_SetRegPC(addr);
}

/*
 ********************************************************
 * LEA Load effective address
 * v0
 ********************************************************
 */
void
cf_lea(void)
{
	EAddress ea;	
	uint32_t addr;
	int ax = (ICODE & 0xe00) >> 9;
	int ea_mode = (ICODE & 0x38) >> 3;
	int ea_reg = (ICODE & 7); 
	Ea_Get(&ea,ea_mode,ea_reg,32);
	addr = Ea_GetAddr(&ea); 
	CF_SetRegA(addr,ax);
}

/*
 *******************************************************
 * LINK and Allocate
 * v0 
 *******************************************************
 */
void
cf_link(void)
{
	int ay = ICODE & 7;	
	uint32_t Ay = CF_GetRegA(ay);
	uint32_t Sp;
	int16_t Dn;
	Push4(Ay);
	Sp = CF_GetRegA(7);
	CF_SetRegA(Sp,ay);
	Dn = CF_MemRead16(CF_GetRegPC());
	CF_SetRegPC(CF_GetRegPC()+2);
	Sp += Dn;
	CF_SetRegA(Sp,7);
}

/*
 ******************************************************
 * LSR immediate
 * v0
 ******************************************************
 */
void
cf_lsri(void)
{
	int dx = ICODE & 7;
	int count = (ICODE & 0x0e00) >> 9;
	uint32_t Dx;
	int shiftout;
	if(count == 0) {
		count = 8;
	}
	Dx = CF_GetReg(dx);
	shiftout = (Dx >> (count - 1)) & 1;
	Dx = Dx >> count;
	CF_REG_CCR = CF_REG_CCR & ~(CCR_X | CCR_C | CCR_V | CCR_Z | CCR_N);
	if(shiftout) {
		CF_REG_CCR |= CCR_X | CCR_C;
	}	
	if(ISNEG(Dx)) {
		CF_REG_CCR |= CCR_N;
	} else if(Dx == 0) {
		CF_REG_CCR |= CCR_Z;
	}
	CF_SetRegD(Dx,dx);
}

/*
 ***********************************************************************
 * LSR shift right by register value
 * v0
 ***********************************************************************
 */
void
cf_lsrr(void)
{
	int shiftout;
	int dx = ICODE & 7;
	int dy = (ICODE & 0x0e00) >> 9;
	unsigned int shift;
	uint32_t Dx;
	uint32_t Dy;
	Dx = CF_GetReg(dx);
	Dy = CF_GetReg(dy);
	shift = Dy & 0x3f;
	if(shift < 32) {
		shiftout = (Dx >> (shift - 1)) & 1;
		Dx = Dx >> shift;
	} else if(shift >= 32) {
		shiftout = 0;
		Dx = 0;
	}
	if(shift == 0) {
		CF_REG_CCR = CF_REG_CCR & ~(CCR_C | CCR_V | CCR_Z | CCR_N);
	} else {
		CF_REG_CCR = CF_REG_CCR & ~(CCR_X | CCR_C | CCR_V | CCR_Z | CCR_N);
		if(shiftout) {
			CF_REG_CCR |= CCR_X | CCR_C;
		}	
	}
	if(ISNEG(Dx)) {
		CF_REG_CCR |= CCR_N;
	} else if(Dx == 0) {
		CF_REG_CCR |= CCR_Z;
	}
	CF_SetRegD(Dx,dx);

}

/*
 **************************************************************
 * What is the difference to asl
 * LSL immediate
 * v0
 **************************************************************
 */
void
cf_lsli(void)
{
	int dx = ICODE & 7;
	uint32_t Dx;
	int count = (ICODE & 0x0e00) >> 9;
	uint32_t shiftout;
	if(count == 0) {
		count = 8;
	}
	Dx = CF_GetRegD(dx);
	shiftout = (Dx >> (32 - count)) & 1;
	Dx = Dx << count;	
	CF_REG_CCR = CF_REG_CCR & ~(CCR_X | CCR_C | CCR_V | CCR_Z | CCR_N);
	if(shiftout) {
		CF_REG_CCR = CCR_X | CCR_C;
	}
	if(ISNEG(Dx)) {
		CF_REG_CCR |= CCR_N;
	} else if(Dx == 0) {
		CF_REG_CCR |= CCR_Z;
	}
	CF_SetRegD(Dx,dx);
	
}
/*
 **********************************************************************
 * LSL by register
 * v0
 **********************************************************************
 */
void
cf_lslr(void)
{
	int shiftout;
	int dx = ICODE & 7;
	int dy = (ICODE & 0x0e00) >> 9;
	unsigned int shift;
	uint32_t Dx;
	uint32_t Dy;
	Dx = CF_GetReg(dx);
	Dy = CF_GetReg(dy);
	shift = Dy & 0x3f;
	if(shift < 32) {
		shiftout = (Dx >> (32 - shift)) & 1;
		Dx = Dx << shift;	
	} else if(shift == 32) {
		shiftout = (Dx & 1);
		Dx = 0;	
	} else {
		shiftout = 0;
		Dx = 0;
	}
	if(shift == 0) {
		CF_REG_CCR = CF_REG_CCR & ~(CCR_C | CCR_V | CCR_Z | CCR_N);
	} else {
		CF_REG_CCR = CF_REG_CCR & ~(CCR_X | CCR_C | CCR_V | CCR_Z | CCR_N);
		if(shiftout) {
			CF_REG_CCR |= CCR_X | CCR_C;
		}	
	}
	if(ISNEG(Dx)) {
		CF_REG_CCR |= CCR_N;
	} else if(Dx == 0) {
		CF_REG_CCR |= CCR_Z;
	}
	CF_SetRegD(Dx,dx);
}

/*
 ****************************************************************
 * MOV3Q
 * v0
 ****************************************************************
 */
void
cf_mov3q(void)
{
	EAddress ea;	
	int ea_mode = (ICODE & 0x38) >> 3;
	int32_t imm = (ICODE & 0x0e00) >> 9;
	int ea_reg = (ICODE & 7); 
	Ea_Get(&ea,ea_mode,ea_reg,32);
	CF_REG_CCR = CF_REG_CCR & ~(CCR_C | CCR_V | CCR_Z | CCR_N);
	if(imm == 0) {
		imm = -1;
		CF_REG_CCR |= CCR_N;
	} 
	Ea_SetValue(imm,&ea);
}

/*
 *********************************************************************
 * MOVE
 *********************************************************************
 */
void
cf_move(void)
{
	EAddress eas;	
	EAddress ead;	
	int eas_mode = (ICODE & 0x38) >> 3;
	int ead_mode = (ICODE & 0x1c0) >> 6;
	int eas_reg = (ICODE & 7); 
	int ead_reg = (ICODE & 0xe00) >> 9; 
	int size = (ICODE & 0x3000) >> 12;
	uint32_t result;
	if(size == 1) {
		size = 8;
	} else if(size == 3) {
		size = 16;
	} else if(size == 2) {
		size = 32;
	}
	Ea_Get(&eas,eas_mode,eas_reg,size);
	Ea_Get(&ead,ead_mode,ead_reg,size);
	result = Ea_GetValue(&eas);
	Ea_SetValue(result,&ead);
	CF_REG_CCR = CF_REG_CCR & ~(CCR_C | CCR_V | CCR_Z | CCR_N);
	/* I'm not sure if it shouldn't be ISNEG(result) (32 Bit) */
	//if(result & (1<<(size - 1))) 
	if(ISNEG(result)) {
		CF_REG_CCR |= CCR_N;
	} else if(result == 0) {
		CF_REG_CCR |= CCR_Z;
	}
}

/*
 ************************************************************************
 * MOVEA Move Address from Source to Destination
 * v0
 ************************************************************************
 */

void
cf_movea(void)
{
	EAddress eas;	
	int eas_mode = (ICODE & 0x38) >> 3;
	int eas_reg = (ICODE & 7); 
	int size = (ICODE & 0x3000) >> 12;
	int ax = (ICODE & 0xe00) >> 9;
	uint32_t result;
	if(size == 3) {
		size = 16;
	} else if(size == 2) {
		size = 32;
	} else {
		fprintf(stderr,"reserved\n");
		/* should trigger some exception ? */
		return;

	}
	Ea_Get(&eas,eas_mode,eas_reg,size);
	result = Ea_GetValue(&eas);
	if(size==16) {
		result = (int32_t)(int16_t)result;
	}
	CF_SetRegA(result,ax);	
}

/*
 **************************************************************************
 * Move Multiple Registers
 * v0
 **************************************************************************
 */
void
cf_movem(void)
{
	EAddress ea;
	uint16_t list;
	int i;
	int ea_mode = (ICODE & 0x38) >> 3;
	int ea_reg = (ICODE & 7); 
	uint32_t addr;
	Ea_Get(&ea,ea_mode,ea_reg,32);
	list = CF_MemRead16(CF_GetRegPC());
	CF_SetRegPC(CF_GetRegPC() + 2);
	addr = Ea_GetAddr(&ea);
	
	if(ICODE & (1<<10)) {  /* memory to register */
		for(i=0;i<16;i++) {
			if(list & (1<<i)) {
				uint32_t val = CF_MemRead32(addr);
				CF_SetReg(val,i);
				addr+=4;
			}
		}
	} else { /* register to memory */
		for(i=0;i<16;i++) {
			if(list & (1<<i)) {
				uint32_t val = CF_GetReg(i);
				CF_MemWrite32(val,addr);
				addr+=4;
			}
		}
	}	
}

/*
 ***************************************************************
 * MOVEQ Move Quick
 * v0
 ***************************************************************
 */
void
cf_moveq(void)
{
	int8_t imm = (ICODE & 0xff);
	int dx = (ICODE & 0xe00) >> 9;
	uint32_t Dx;
	Dx = (int32_t)imm;		
	CF_SetRegD(Dx,dx);
	CF_REG_CCR = CF_REG_CCR & ~(CCR_C | CCR_V | CCR_Z | CCR_N);
	if(ISNEG(Dx)) {
		CF_REG_CCR |=  CCR_N;
	} else if(Dx == 0) {
		CF_REG_CCR |=  CCR_Z;
	}
}
/*
 ***************************************************************
 * MOVE from CCR
 * v0
 ***************************************************************
 */
void
cf_movefccr(void)
{
	int dx = ICODE & 7;
	uint32_t Dx = CF_GetRegD(dx);
	Dx = (Dx & 0xffff0000) | 
		(CF_REG_CCR & (CCR_C | CCR_V | CCR_Z | CCR_N | CCR_X)); 
	CF_SetRegD(Dx,dx);
}

/*
 **************************************************************
 * MOVE to CCR
 * v0
 **************************************************************
 */
void
cf_movetccr(void)
{
	EAddress eas;	
	int eas_mode = (ICODE & 0x38) >> 3;
	int eas_reg = (ICODE & 7); 
	uint8_t value;
	Ea_Get(&eas,eas_mode,eas_reg,8);
	value = Ea_GetValue(&eas);
	CF_REG_CCR = (CF_REG_CCR & 0xff00) | (value & 0xff);
	return;
}

/*
 *****************************************************************
 * MULS Signed Multiply Word
 * v0
 *****************************************************************
 */
void
cf_mulsw(void)
{
	EAddress eas;	
	int16_t Dx;
	int16_t eaval;
	int32_t result;
	int dx = (ICODE & 0xe00) >> 9;
	int eas_mode = (ICODE & 0x38) >> 3;
	int eas_reg = (ICODE & 7); 
	Ea_Get(&eas,eas_mode,eas_reg,16);
	Dx = CF_GetRegD(dx);
	eaval = Ea_GetValue(&eas);
	result = eaval * Dx;
	CF_REG_CCR = CF_REG_CCR & ~(CCR_C | CCR_V | CCR_Z | CCR_N);
	if(result == 0) {
		CF_REG_CCR |= CCR_Z;
	} else if(ISNEG(result)) {
		CF_REG_CCR |= CCR_N;
	}
	CF_SetRegD(result,dx);
}
/*
 **********************************************************
 * MULS.L + MULU.L
 * v0
 **********************************************************
 */
void
cf_mull(void)
{
	EAddress eas;	
	uint16_t sword;
	uint32_t Dx;
	uint32_t eaval;
	uint32_t result;
	int eas_mode = (ICODE & 0x38) >> 3;
	int eas_reg = (ICODE & 7); 
	int dx;
	int is_signed;
	sword = CF_MemRead16(CF_GetRegPC());
	CF_SetRegPC(CF_GetRegPC()+2);
	is_signed = !!(sword >> 11);
	dx = (sword & 0x7000) >> 12;
	Ea_Get(&eas,eas_mode,eas_reg,32);
	Dx = CF_GetRegD(dx);
	eaval = Ea_GetValue(&eas);
	if(is_signed) {
		result = (int32_t)eaval * (int32_t)Dx;
	} else {
		result = eaval * Dx;
	}
	CF_REG_CCR = CF_REG_CCR & ~(CCR_C | CCR_V | CCR_Z | CCR_N);
	if(result == 0) {
		CF_REG_CCR |= CCR_Z;
	} else if(ISNEG(result)) {
		CF_REG_CCR |= CCR_N;
	}
	CF_SetRegD(result,dx);
}

/*
 *****************************************************************
 * MULU.W Unsigned multiply word
 * v0
 *****************************************************************
 */
void
cf_muluw(void)
{
	EAddress eas;	
	uint16_t Dx;
	uint16_t eaval;
	uint32_t result;
	int dx = (ICODE & 0xe00) >> 9;
	int eas_mode = (ICODE & 0x38) >> 3;
	int eas_reg = (ICODE & 7); 
	Ea_Get(&eas,eas_mode,eas_reg,16);
	Dx = CF_GetRegD(dx);
	eaval = Ea_GetValue(&eas);
	result = eaval * Dx;
	CF_REG_CCR = CF_REG_CCR & ~(CCR_C | CCR_V | CCR_Z | CCR_N);
	if(result == 0) {
		CF_REG_CCR |= CCR_Z;
	} else if(ISNEG(result)) {
		CF_REG_CCR |= CCR_N;
	}
	CF_SetRegD(result,dx);
}

/*
 **************************************************
 * MVS Move with Sign Extend
 * v0
 **************************************************
 */

void
cf_mvs(void)
{
	EAddress eas;	
	int eas_mode = (ICODE & 0x38) >> 3;
	int eas_reg = (ICODE & 7); 
	int size = (ICODE & 0x40) ? 16 : 8;
	int dx = (ICODE & 0xe00) >> 9;
	uint32_t value;
	uint32_t result;
	Ea_Get(&eas,eas_mode,eas_reg,size);
	value = Ea_GetValue(&eas);
	if(size == 8) {
		result = (int32_t)(int8_t) value;
	} else {
		result = (int32_t)(int16_t) value;
	}
	CF_REG_CCR = CF_REG_CCR & ~(CCR_C | CCR_V | CCR_Z | CCR_N);
	if(result == 0) {
		CF_REG_CCR |= CCR_Z;
	} else if(ISNEG(result)) {
		CF_REG_CCR |= CCR_N;
	}
	CF_SetRegD(result,dx);

}

/*
 ************************************************************
 * MVZ
 * Move with Zero Fill
 * v0
 ************************************************************
 */
void
cf_mvz(void)
{
	EAddress eas;	
	int eas_mode = (ICODE & 0x38) >> 3;
	int eas_reg = (ICODE & 7); 
	int size = (ICODE & 0x40) ? 16 : 8;
	int dx = (ICODE & 0xe00) >> 9;
	uint32_t value;
	uint32_t result;
	Ea_Get(&eas,eas_mode,eas_reg,size);
	value = Ea_GetValue(&eas);
	if(size == 8) {
		result = (uint32_t)(uint8_t) value;
	} else {
		result = (uint32_t)(uint16_t) value;
	}
	CF_REG_CCR = CF_REG_CCR & ~(CCR_C | CCR_V | CCR_Z | CCR_N);
	/* Negative can not happen because of zero fill */
	if(result == 0) {
		CF_REG_CCR |= CCR_Z;
	}
	CF_SetRegD(result,dx);
}
/*
 ***************************************************************
 * NEG 0 - Destination -> Destination
 * v0
 ***************************************************************
 */
void
cf_neg(void)
{
	int dx = (ICODE & 7);
	int32_t Dx = CF_GetRegD(dx);
	CF_REG_CCR = CF_REG_CCR & ~(CCR_C | CCR_V | CCR_Z | CCR_N | CCR_X);
	if(Dx == 0) {
		CF_REG_CCR |= CCR_Z;
	} else {
		CF_REG_CCR |= CCR_C | CCR_X;
	}
	if(ISNEG(-Dx)) {
		CF_REG_CCR |= CCR_N;
	}
	if(sub_overflow(0,Dx,-Dx)) {
		CF_REG_CCR |= CCR_V;
	}
	CF_SetRegD(-Dx,dx);
}

/*
 ****************************************************
 * NEGX negated with extend
 * v0
 ****************************************************
 */
void
cf_negx(void)
{
	int dx = (ICODE & 7);
	int32_t Dx = CF_GetRegD(dx);
	int32_t result;
	result = -Dx;
	if(CF_REG_CCR & CCR_X) {
		result -= 1;
	}
	CF_REG_CCR = CF_REG_CCR & ~(CCR_C | CCR_V | CCR_Z | CCR_N | CCR_X);
	CF_REG_CCR |= sub_flags(0,Dx,result); 
	CF_SetRegD(result,dx);
}

/*
 ***********************************************************
 * NOP Do nothing
 * v0
 ***********************************************************
 */
void
cf_nop(void)
{
	return;
}

/*
 ***********************************************************
 * NOT
 * v0
 ***********************************************************
 */
void
cf_not(void)
{
	int dx = (ICODE & 7);
	uint32_t Dx = CF_GetRegD(dx);
	uint32_t result;
	result = ~Dx;
	CF_REG_CCR = CF_REG_CCR & ~(CCR_C | CCR_V | CCR_Z | CCR_N);
	if(ISNEG(result)) {
		CF_REG_CCR |= CCR_N;
	} else if(result == 0) {
		CF_REG_CCR |= CCR_Z;
	}
	CF_SetRegD(result,dx);
}

/*
 **************************************************************
 * OR
 * v0
 **************************************************************
 */
void
cf_or(void)
{
	uint16_t opmode = (ICODE & 0x1c0) >> 6;
	EAddress ea;	
	int ea_mode = (ICODE & 0x38) >> 3;
	int ea_reg = (ICODE & 7); 
	int reg = (ICODE & 0xe00) >> 9;
	uint32_t op1,op2,result;
	Ea_Get(&ea,ea_mode,ea_reg,32);
	op1 = Ea_GetValue(&ea); 
	op2 = CF_GetRegD(reg);
	result = op1 | op2;
	if(opmode == 2) {
		/* <ea>y | Dx -> Dx */
		CF_SetRegD(result,reg);
	} else if(opmode == 6) {
		/* Dy | <ea>x -> <ea>x */
		Ea_SetValue(result,&ea);	
	} else {
		// illegal opmode 
	}	
	CF_REG_CCR = CF_REG_CCR & ~(CCR_C | CCR_V | CCR_Z | CCR_N );
	if(ISNEG(result))  {
		CF_REG_CCR |=  CCR_N;
	} else if(result == 0) {
		CF_REG_CCR |=  CCR_Z;
	}
}

/*
 *************************************************************
 * ORI Inclusive OR
 * v0
 *************************************************************
 */
void
cf_ori(void)
{
	int dreg = (ICODE & 0x7);
	uint32_t op1,op2,result;
	op1 = CF_MemRead32(CF_GetRegPC());
	op2 = CF_GetRegD(dreg);	
	result = op1 | op2;
	CF_SetRegPC(CF_GetRegPC()+4);
	CF_SetRegD(result,dreg);
	CF_REG_CCR = CF_REG_CCR & ~(CCR_C | CCR_V | CCR_Z | CCR_N);
	if(ISNEG(result))  {
		CF_REG_CCR |=  CCR_N;
	} else if(result == 0) {
		CF_REG_CCR |=  CCR_Z;
	}
}

/*
 **********************************************************
 * PEA Push Effective Address onto the stack
 * v0
 **********************************************************
 */
void
cf_pea(void)
{
	EAddress ea;	
	int ea_mode = (ICODE & 0x38) >> 3;
	int ea_reg = (ICODE & 7); 
	uint32_t addr;
	Ea_Get(&ea,ea_mode,ea_reg,32);
	addr = Ea_GetAddr(&ea);
	Push4(addr);
}

void
cf_pulse(void)
{
	fprintf(stderr,"cf_pulse not implemented\n");
}

/*
 ***********************************************
 * Return from Subroutine
 * v0
 ***********************************************
 */
void
cf_rts(void)
{
	CF_SetRegPC(Pop4());
}

/*
 *********************************************************************
 * Signed Saturate
 * Set Dx to the maximum negative number if positive or 0 and Overflow
 * Set Dx to the maximum positive number if negative and Overflow
 *********************************************************************
 */
void
cf_sats(void)
{
	int dx = (ICODE & 7);
	uint32_t Dx = CF_GetRegD(dx);
	if(CF_REG_CCR & CCR_V) {
		if(ISNEG(Dx)) {
			Dx = 0x7fffffff;
		} else {
			Dx = 0x80000000;
		}
		CF_SetRegD(Dx,dx);
	}
	CF_REG_CCR = CF_REG_CCR & ~(CCR_C | CCR_V | CCR_Z | CCR_N);
	if(ISNEG(Dx)) {
		CF_REG_CCR |= CCR_N;
	} else if(Dx == 0) {
		CF_REG_CCR |= CCR_Z;
	}
}

/*
 ******************************************************
 * Scc
 * Set According to Condition
 * v0
 ******************************************************
 */
void
cf_scc(void)
{
	int dx = ICODE & 7;
	uint32_t Dx = CF_GetRegD(dx);
	int ccode = (ICODE & 0x0f00) >> 8;
	if(CHECK_CONDITION(ccode,CF_REG_CCR)) {
		Dx |= 0xff;
	} else {
		Dx &= ~UINT32_C(0xff);
	}
	CF_SetRegD(Dx,dx);
}

/*
 ******************************************************
 * SUB
 *   Subtract
 *   v0
 ******************************************************
 */
void
cf_sub(void)
{
	uint16_t opmode = (ICODE & 0x1c0) >> 6;
	EAddress ea;	
	int ea_mode = (ICODE & 0x38) >> 3;
	int ea_reg = (ICODE & 7); 
	int reg = (ICODE & 0xe00) >> 9;
	uint32_t op1,op2,result;
	Ea_Get(&ea,ea_mode,ea_reg,32);
	if(opmode == 2) {
		/* Dx - <ea>y -> Dx */
		op1 = CF_GetRegD(reg);
		op2 = Ea_GetValue(&ea); 
		result = op1 - op2;
		CF_SetRegD(result,reg);
	} else if(opmode == 6) {
		/* <ea> - Dy -> <ea>x */
		op1 = Ea_GetValue(&ea); 
		op2 = CF_GetRegD(reg);
		result = op1 - op2;
		Ea_SetValue(result,&ea);	
	} else {
		fprintf(stderr,"sub illegal opmode\n");
		// illegal opmode 
		return;
	}	
	CF_REG_CCR = CF_REG_CCR & ~(CCR_C | CCR_V | CCR_Z | CCR_N | CCR_X);
	CF_REG_CCR |= sub_flags(op1,op2,result); 
}

/*
 *****************************************************************
 * SUBA Subtract Address
 * v0
 *****************************************************************
 */
void
cf_suba(void)
{
	EAddress ea;	
	int ea_mode = (ICODE & 0x38) >> 3;
	int ea_reg = (ICODE & 7); 
	int reg = (ICODE & 0xe00) >> 9;
	uint32_t op1,op2,result;
	Ea_Get(&ea,ea_mode,ea_reg,32);
	op1 = CF_GetRegA(reg);
	op2 = Ea_GetValue(&ea); 
	result = op1 - op2;
	CF_SetRegA(result,reg);
	CF_REG_CCR = CF_REG_CCR & ~(CCR_C | CCR_V | CCR_Z | CCR_N | CCR_X);
	CF_REG_CCR |= sub_flags(op1,op2,result); 
}

/*
 *******************************************************************
 * SUBI Subtract Immediate
 * v0
 *******************************************************************
 */
void
cf_subi(void)
{
	int dreg = (ICODE & 0x7);
	uint32_t op1,op2,result;
	op1 = CF_GetRegD(dreg);	
	op2 = CF_MemRead32(CF_GetRegPC());
	result = op1 - op2;
	CF_SetRegPC(CF_GetRegPC()+4);
	CF_SetRegD(result,dreg);
	CF_REG_CCR = CF_REG_CCR & ~(CCR_C | CCR_V | CCR_Z | CCR_N | CCR_X);
	CF_REG_CCR |= sub_flags(op1,op2,result); 
}

/*
 ***************************************************************
 * SUBQ Subtract Quick
 * Destination - 3 Bit immediate (1-8) -> Destination
 * v0
 ***************************************************************
 */
void
cf_subq(void)
{
	EAddress ea;	
	uint32_t op1,result;
	uint32_t op2 = (ICODE & 0x0e00) >> 9;
	int ea_mode = (ICODE & 0x38) >> 3;
	int ea_reg = (ICODE & 7); 
	if(op2 == 0) {
		op2 = 8;
	}
	Ea_Get(&ea,ea_mode,ea_reg,32);
	op1 = Ea_GetValue(&ea); 
	result = op1-op2;	
	Ea_SetValue(result,&ea); 
	CF_REG_CCR = CF_REG_CCR & ~(CCR_C | CCR_V | CCR_Z | CCR_N | CCR_X);
	CF_REG_CCR |= sub_flags(op1,op2,result); 
}
/*
 ************************************************************
 * SUBX Subtract Extended
 * v0
 ************************************************************
 */
void
cf_subx(void)
{
	int dy = ICODE & 0x7;
	int dx = (ICODE & 0x0e00) >> 9;
	uint32_t op1,op2,result;
	uint16_t flags;
	op1 = CF_GetRegD(dy);
	op2 = CF_GetRegD(dx);
	result = op1 - op2;
	if(CF_REG_CCR & CCR_X) {
		result -= 1;
	}
	CF_SetRegD(result,dx);
	flags = sub_carry(op1,op2,result);
	if(flags & CCR_C) {
		flags |= CCR_X;
	}
	flags |= sub_overflow(op1,op2,result);
	if(ISNEG(result)) {
		flags |= CCR_N;
	} 
	CF_REG_CCR = CF_REG_CCR & ~(CCR_C | CCR_V |  CCR_N | CCR_X);
	CF_REG_CCR |= flags;
	if(result != 0) {
		CF_REG_CCR &= ~CCR_Z;
	}
}

/*
 ******************************************************
 * SWAP Swap Register Halves
 * v0
 ******************************************************
 */
void
cf_swap(void)
{
	int dx = (ICODE & 7);
	uint32_t Dx = CF_GetRegD(dx);
	Dx = (Dx >> 16) | (Dx << 16);
	CF_SetRegD(Dx,dx);
	CF_REG_CCR = CF_REG_CCR & ~(CCR_C | CCR_V |  CCR_Z | CCR_N);
	if(ISNEG(Dx)) {
		CF_REG_CCR |= CCR_N;
	} else if(Dx == 0) {
		CF_REG_CCR |= CCR_Z;
	}
}

/*
 * Test and Set an Operand
 * ?????
 */
void
cf_tas(void)
{
	EAddress ea;	
	int ea_mode = (ICODE & 0x38) >> 3;
	int ea_reg = (ICODE & 7); 
	uint8_t val;
	Ea_Get(&ea,ea_mode,ea_reg,8);
	val = Ea_GetValue(&ea); 
	Ea_SetValue(val | 0x80,&ea);
	CF_REG_CCR = CF_REG_CCR & ~(CCR_C | CCR_V |  CCR_Z | CCR_N);
	if(val & 0x80)   {
		CF_REG_CCR |= CCR_N;
	} else if(val == 0) {
		CF_REG_CCR |= CCR_Z;
	}
}

/*
 ******************************************************************
 * TPF Trap False
 * Multi word NOP without pipeline synchronization
 * void
 ******************************************************************
 */
void
cf_tpf(void)
{
	int opmode = (ICODE & 7);
	if(opmode == 2) {
		CF_SetRegPC(CF_GetRegPC()+2);
	} else if(opmode == 3) {
		CF_SetRegPC(CF_GetRegPC()+4);
	} else if(opmode == 4) {
		/* nothing */
	} else {
		fprintf(stderr,"tpf: illegal opmode\n");
		// some exception ? 
		return;
	}
}

/*
 * exceptions are not yet implemented
 */
void
cf_trap(void)
{
	fprintf(stderr,"cf_trap not implemented\n");
}

/*
 ******************************************************************
 * Test an Operand
 * v0
 ******************************************************************
 */
void
cf_tst(void)
{
	int sz = (ICODE & 0xc0) >> 6;
	int size;
	EAddress ea;	
	int ea_mode = (ICODE & 0x38) >> 3;
	int ea_reg = (ICODE & 7); 
	uint32_t value;
	if(sz < 3) {
		size = 8 << sz;
	} else {
		size = 16;
	}
	Ea_Get(&ea,ea_mode,ea_reg,size);
	value = Ea_GetValue(&ea);
	CF_REG_CCR = CF_REG_CCR & ~(CCR_C | CCR_V |  CCR_Z | CCR_N);
	if(ISNEG(value))   {
		CF_REG_CCR |= CCR_N;
	} else if(value == 0) {
		CF_REG_CCR |= CCR_Z;
	}
}

/*
 ****************************************************
 * Unlink
 * v0
 ****************************************************
 */
void
cf_unlk(void)
{
	int ax = ICODE & 7;
	uint32_t Ax = CF_GetRegA(ax);
	CF_SetRegA(Ax,7);
	CF_SetRegA(Pop4(),ax);	
}

void
cf_wddata(void)
{
	fprintf(stderr,"cf_wddata not implemented\n");
}

/*
 ***************************************************************
 * MAC and MSAC 
 * Multiply and accumulate and Multiply Subtract
 * Manual doesn't tell if operation is signed. 
 * This makes a difference even for 32 Bit because of shift 
 ***************************************************************
 */
void
cf_macsac(void)
{
	uint16_t secword;
	int rx,ry;
	int ulx,uly;
	uint32_t Rx,Ry; 
	uint32_t Acc;
	uint32_t Op2;
	uint32_t Result;
	int sz,scale_factor;
	int subtract;
	secword = CF_MemRead16(CF_GetRegPC());
	CF_SetRegPC(CF_GetRegPC() + 2);
	subtract = (secword >> 8) & 1;
	sz = (secword >> 11) & 1;
	scale_factor = (secword >> 9) & 3;
	rx = ((ICODE >> 9) & 7) | ((ICODE & 0x40) >> 3);
	ry = (ICODE & 0xf);
	ulx = (secword >> 7) & 1;
	uly = (secword >> 6) & 1;
	Rx = CF_GetReg(rx);
	Ry = CF_GetReg(ry);
	Acc = CF_GetRegMacAcc();
	if(sz == 0) {
		if(ulx) {
			Rx = Rx >> 16;
		} else {
			Rx = Rx & 0xffff;
		}
		if(uly) {
			Ry = Ry >> 16;
		} else {
			Ry = Ry & 0xffff;
		}
	}
	if(scale_factor == 0) {
		Op2 = (Ry * Rx);
	} else if (scale_factor == 1) {
		Op2 = (Ry * Rx) << 1;
	} else if (scale_factor == 3) {
		Op2 = (Ry * Rx) >> 1;
	} else {
		fprintf(stderr,"illegal scale factor\n");
		// exception
		return;
	}
	CF_REG_MACSR = CF_REG_MACSR & ~(CCR_V |  CCR_Z | CCR_N);
	if(subtract) {
		Result = Acc - Op2;
		CF_REG_MACSR |= sub_overflow(Acc,Op2,Result); 
	} else {
		Result = Acc + Op2;
		CF_REG_MACSR |= add_overflow(Acc,Op2,Result); 
	}
	if(ISNEG(Result)) {
		CF_REG_MACSR |= CCR_N;
	} else if(Result == 0) {
		CF_REG_MACSR |= CCR_Z;
	}
	CF_SetRegMacAcc(Result);
}
/*
 ******************************************************************
 * MAC with load
 * Manual does not tell if  operation is signed
 ******************************************************************
 */
void
cf_macsacl(void)
{
	uint16_t secword;
	int rx,ry,rw;
	int ulx,uly;
	uint32_t Rx,Ry,Rw; 
	uint32_t Acc;
	uint32_t Op2;
	uint32_t Result;
	int sz,scale_factor,mask;
	EAddress ea;	
	int ea_mode = (ICODE & 0x38) >> 3;
	int ea_reg = (ICODE & 7); 
	int subtract;
	secword = CF_MemRead16(CF_GetRegPC());
	CF_SetRegPC(CF_GetRegPC()+2);
	subtract = (secword >> 8) & 1;
	sz = (secword >> 11) & 1;
	if(sz) {
		Ea_Get(&ea,ea_mode,ea_reg,32);
	} else {
		Ea_Get(&ea,ea_mode,ea_reg,16);
	}
	scale_factor = (secword >> 9) & 3;
	rx = ((secword >> 12) & 0xf);
	ry = (secword & 0xf);
	rw = (ICODE >> 9) & 0x7;
	rw |= (ICODE & 0x40) >> 3;
	ulx = (secword >> 7) & 1;
	uly = (secword >> 6) & 1;
	mask = (secword >> 5) & 1;
	Rx = CF_GetReg(rx);
	Ry = CF_GetReg(ry);
	Acc = CF_GetRegMacAcc();
	if(scale_factor == 0) {
		Op2 = (Ry * Rx);
	} else if (scale_factor == 1) {
		Op2 = (Ry * Rx) << 1;
	} else if (scale_factor == 3) {
		Op2 = (Ry * Rx) >> 1;
	} else {
		fprintf(stderr,"Illegal scale factor\n");
		// undefined instruction exception
		return;
	}
	CF_REG_MACSR = CF_REG_MACSR & ~(CCR_V |  CCR_Z | CCR_N);
	if(subtract) {
		Result = Acc - Op2;
		CF_REG_MACSR |= sub_overflow(Acc,Op2,Result); 
	} else {
		Result = Acc + Op2;
		CF_REG_MACSR |= add_overflow(Acc,Op2,Result); 
	}
	if(ISNEG(Result))   {
		CF_REG_MACSR |= CCR_N;
	} else if(Result == 0) {
		CF_REG_MACSR |= CCR_Z;
	}
	Rw = Ea_GetValue(&ea);
	if(mask) {
		Rw = Rw & CF_GetRegMacMask();
	}	
	CF_SetReg(Rw,rw);
	CF_SetRegMacAcc(Result);
}

/*
 *********************************************************
 * MOVE from ACC
 * Move Accumulator to a General Purpose Register
 * v0
 *********************************************************
 */

void
cf_movfromacc(void)
{
	int rx = ICODE & 0xf;
	CF_SetReg(rx,CF_GetRegMacAcc());
}

/*
 **************************************************************
 * MOVE from MACSR
 * Move MAC Status regsiter to General Purpose Register
 * v0
 **************************************************************
 */
void
cf_movfrommacsr(void)
{
	int rx = ICODE & 0xf;
	CF_SetReg(rx,CF_GetRegMacSr());
}

/*
 ******************************************************************
 * Move from Mask
 * Move MAC Mask Register to a general purpose register
 ******************************************************************
 */
void
cf_movfrommask(void)
{
	int rx = ICODE & 0xf;
	CF_SetReg(rx,CF_GetRegMacMask());
}

/*
 **********************************************************
 * Move MAC Status register to CCR
 * v0
 **********************************************************
 */
void
cf_movmacsrtoccr(void)
{
	CF_REG_CCR = CF_REG_CCR & ~(CCR_X | CCR_N | CCR_Z | CCR_V | CCR_C);
	CF_REG_CCR |= CF_REG_MACSR & (CCR_V | CCR_Z | CCR_N);
}

/*
 ************************************************
 * Move a 32 Bit value to the MAC Accumulator
 * v0
 ************************************************
 */
void
cf_movtoacc(void)
{
	EAddress ea;	
	uint32_t value;
	int ea_mode = (ICODE & 0x38) >> 3;
	int ea_reg = (ICODE & 7); 
	Ea_Get(&ea,ea_mode,ea_reg,32);
	value = Ea_GetValue(&ea);
	CF_SetRegMacAcc(value);
	CF_REG_MACSR &= ~(CCR_V | CCR_Z | CCR_N);
	if(ISNEG(value)) {
		CF_REG_MACSR |=  CCR_N;
	} else if(value == 0) {
		CF_REG_MACSR |=  CCR_Z;
	}
}
/*
 **************************************************************
 * MOVE to MACSR
 * v0
 **************************************************************
 */
void
cf_movtomacsr(void)
{
	EAddress ea;	
	uint32_t value;
	int ea_mode = (ICODE & 0x38) >> 3;
	int ea_reg = (ICODE & 7); 
	Ea_Get(&ea,ea_mode,ea_reg,32);
	value = Ea_GetValue(&ea);
	CF_REG_MACSR = (value & 0xfe) | (CF_REG_MACSR & ~0xfe);
}

/*
 ******************************************
 * MOVE to MASK
 * v0
 ******************************************
 */
void
cf_movtomask(void)
{
	EAddress ea;	
	uint32_t value;
	int ea_mode = (ICODE & 0x38) >> 3;
	int ea_reg = (ICODE & 7); 
	Ea_Get(&ea,ea_mode,ea_reg,32);
	value = Ea_GetValue(&ea);
	CF_SetRegMacMask(value);
}

void
cf_maaac(void)
{
	fprintf(stderr,"cf_maaac not implemented\n");
}

void
cf_cpushl(void)
{
	if(!CF_IsSupervisor()) {
		fprintf(stderr,"Privilege exception missing %04x\n",CF_REG_CCR);
		return;
	}
	fprintf(stderr,"cf_cpushl not implemented\n");
}

void
cf_frestore(void)
{
	if(!CF_IsSupervisor()) {
		fprintf(stderr,"Privilege exception missing %04x\n",CF_REG_CCR);
		return;
	}
	fprintf(stderr,"cf_frestore not implemented\n");
}

void
cf_fsave(void)
{
	if(!CF_IsSupervisor()) {
		fprintf(stderr,"Privilege exception missing %04x\n",CF_REG_CCR);
		return;
	}
	fprintf(stderr,"cf_fsave not implemented\n");
}

void
cf_halt(void)
{
#if 0
	if(!CF_IsSupervisor() && !CSRUHE) {
		fprintf(stderr,"Privilege exception missing\n");
		return;
	}
#endif
	fprintf(stderr,"cf_halt not implemented\n");
}

/* 
 * instruction prefetch operation, currently a nop because
 * cache is not emulated 
 */
void
cf_intouch(void)
{
	if(!CF_IsSupervisor()) {
		fprintf(stderr,"Privilege exception missing\n");
		return;
	}
	/* Do nothing, instruction cache not emulated */
}

/*
 ****************************************************************
 * Move from Status register to general purpose register
 * v0
 ****************************************************************
 */
void
cf_movefromsr(void)
{
	int dx = (ICODE & 0x7);
	uint32_t Dx;
	if(!CF_IsSupervisor()) {
		fprintf(stderr,"Privilege exception missing %04x\n",CF_REG_CCR);
		return;
	}
	Dx = CF_GetRegD(dx);
	Dx = (Dx & 0xffff0000) |  CF_REG_CCR;
	CF_SetRegD(Dx,dx);
}

/*
 **************************************************************
 * MOVE from USP
 * Move User stack pointer to destination
 * v0
 **************************************************************
 */
void
cf_movfromusp(void)
{
	int ax;
	if(!CF_IsSupervisor()) {
		fprintf(stderr,"Privilege exception missing %04x\n",CF_REG_CCR);
		return;
	}
	ax = ICODE & 7;
	CF_SetRegA(CF_GetRegOtherA7(),ax);
}

/*
 **********************************************
 * MOVE to SR
 * v0
 **********************************************
 */
void
cf_movetosr(void)
{
	EAddress ea;	
	uint32_t value;
	int ea_mode;
	int ea_reg;
	if(!CF_IsSupervisor()) {
		fprintf(stderr,"Privilege exception missing %04x\n",CF_REG_CCR);
		return;
	}
	ea_mode = (ICODE & 0x38) >> 3;
	ea_reg = (ICODE & 7); 
	Ea_Get(&ea,ea_mode,ea_reg,16);
	value = Ea_GetValue(&ea);
	CF_SetRegSR(value);
}

/*
 ***************************************************
 * MOVE to USP
 * Move to User Stack Pointer
 * v0
 ***************************************************
 */
void
cf_movetousp(void)
{
	int ay = ICODE & 7;
	uint32_t Ay;
	if(!CF_IsSupervisor()) {
		fprintf(stderr,"Privilege exception missing %04x\n",CF_REG_CCR);
		return;
	}
	Ay = CF_GetRegA(ay);
	CF_SetRegOtherA7(Ay);
}

/*
 ******************************************************
 * MOVEC Move to Control Register
 * v0
 ******************************************************
 */
void
cf_movec(void)
{
	uint32_t Rx;
	int rx;
	int cr;
	uint16_t eword1;	
	uint32_t pc;
	if(!CF_IsSupervisor()) {
		fprintf(stderr,"Privilege exception missing %04x\n",CF_REG_CCR);
		return;
	}
	pc = CF_GetRegPC();
	eword1 = CF_MemRead16(pc);	
	rx = (eword1 & 0xf000) >> 12;
	cr = (eword1 & 0xfff);
	CF_SetRegPC(pc+2);
	Rx = CF_GetReg(rx);
	CF_SetRegCR(Rx,cr);
}

/*
 ******************************************************************
 * The instruction description is not understandable,
 * use the Exception processing chapter instead.
 * v0
 ******************************************************************
 */
void
cf_rte(void)
{
	/* Exception stack frames are on system stack */
	uint32_t formvec;
	uint32_t pc;
	int format;
	if(!CF_IsSupervisor()) {
		fprintf(stderr,"Privilege exception missing %04x\n",CF_REG_CCR);
		return;
	}
	formvec = Pop4(); 
	pc = Pop4();
	format = (formvec >> 28) & 0xf;
	if((format > 7) || (format < 4)) {
		fprintf(stderr,"Bad format field in stack frame\n");
	} else {
		uint32_t sp = CF_GetRegA(7);	
		sp+=(format-4);
		CF_SetRegA(sp,7);	
	}
	CF_SetRegPC(pc);
	CF_SetRegSR(formvec & 0xffff);	
}

/*
 *****************************************************
 * Store/Load Status Register
 * v0 
 *****************************************************
 */
void
cf_strldsr(void)
{
	uint32_t pc;
	uint16_t eword1;
	uint16_t sr;
	if(!CF_IsSupervisor()) {
		fprintf(stderr,"Privilege exception missing %04x\n",CF_REG_CCR);
		return;
	}
	pc = CF_GetRegPC();
	eword1 = CF_MemRead16(pc);	
	if(eword1 != 0x46fc) {
		fprintf(stderr,"Not a STRLDSR instruction\n");
		// unknown instruction exception	
	}
	sr = CF_MemRead16(pc+2);	
	Push4(CF_REG_CCR);
	CF_SetRegSR(sr);
	CF_SetRegPC(pc+4);
}

void
cf_stop(void)
{
	uint32_t pc;
	uint16_t sr;
	if(!CF_IsSupervisor()) {
		fprintf(stderr,"Privilege exception missing %04x\n",CF_REG_CCR);
		return;
	}
	pc = CF_GetRegPC();
	sr = CF_MemRead16(pc);	
	CF_SetRegSR(sr);
	CF_SetRegPC(pc+2);
	fprintf(stderr,"Stop not implemented\n");
}

void
cf_wdebug(void)
{
	EAddress ea;	
	uint32_t value;
	int ea_mode;
	int ea_reg;
	if(!CF_IsSupervisor()) {
		fprintf(stderr,"Privilege exception missing %04x\n",CF_REG_CCR);
		return;
	}
	ea_mode = (ICODE & 0x38) >> 3;
	ea_reg = (ICODE & 7); 
	Ea_Get(&ea,ea_mode,ea_reg,32);
	value = Ea_GetValue(&ea);
	fprintf(stderr,"WDEBUG not implemented: %08x\n",value);
	//CF_WriteDebug(value);
}
void
cf_wdebug_ctrl(void)
{
	fprintf(stderr,"WDEBUG control not implemented\n");
}

void
cf_undefined(void) 
{
	fprintf(stderr,"Undefined instruction 0x%04x\n",ICODE);
}

