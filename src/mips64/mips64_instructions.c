#include <stdio.h>
#include <mips64_cpu.h>
#include <mips64_mmu.h>
#include <mips64_instructions.h>

static inline int  
add32_overflow(int32_t op1,int32_t op2,int32_t result) {
        if ((ISNEG32 (op1) && ISNEG32 (op2) && ISNOTNEG32 (result))
          || (ISNOTNEG32 (op1) && ISNOTNEG32 (op2) && ISNEG32 (result))) {
                return 1;
        } else {
                return 0;
        }
}

static inline int  
add64_overflow(int64_t op1,int64_t op2,int64_t result) {
        if ((ISNEG64 (op1) && ISNEG64 (op2) && ISNOTNEG64 (result))
          || (ISNOTNEG64 (op1) && ISNOTNEG64 (op2) && ISNEG64 (result))) {
                return 1;
        } else {
                return 0;
        }
}

static inline uint32_t
sub32_overflow(int32_t op1,int32_t op2,int32_t result) {
        if ((ISNEG32(op1) && ISNOTNEG32(op2) && ISNOTNEG32 (result))
          || (ISNOTNEG32 (op1) && ISNEG32 (op2) && ISNEG32 (result))) {
                return 1;
        } else {
                return 0;
        }
}

static inline uint32_t
sub64_overflow(int64_t op1,int64_t op2,int64_t result) {
        if ((ISNEG64(op1) && ISNOTNEG64(op2) && ISNOTNEG64 (result))
          || (ISNOTNEG64 (op1) && ISNEG64 (op2) && ISNEG64 (result))) {
                return 1;
        } else {
                return 0;
        }
}

static inline void
mips64_BLTZ(void) {
	int rs = (ICODE >> 21) & 0x1f;	
	uint64_t Rs = MIPS64_ReadGpr(rs);
	if(Rs < 0) {
		int16_t off16 = ICODE & 0xffff;
		uint64_t pc = GET_NIA;
		pc = pc + ((int32_t)off16 << 2);
		MIPS64_ExecuteDelaySlot();
		MIPS64_SetRegPC(pc);
	}
}

static inline void
mips64_BGEZ(void) {
	int rs = (ICODE >> 21) & 0x1f;	
	uint64_t Rs = MIPS64_ReadGpr(rs);
	uint64_t pc = GET_NIA;
	if(Rs >= 0) {
		int16_t off16 = ICODE & 0xffff;
		pc = pc +  ((int32_t)off16 << 2);
		MIPS64_ExecuteDelaySlot();
		MIPS64_SetRegPC(pc);
	}

}
/*
 * ----------------------------------------------------------
 * BLTZL
 * 	Branch on Less Than Zero Likely
 * 	This is a deprecated instruction because it does not 
 * 	execute the instruction in branch delay slot
 * ----------------------------------------------------------
 */
static inline void
mips64_BLTZL(void) {
	int rs = (ICODE >> 21) & 0x1f;	
	uint64_t Rs = MIPS64_ReadGpr(rs);
	if(Rs < 0) {
		int16_t off16 = ICODE & 0xffff;
		uint64_t pc = GET_NIA;
		pc = pc + ((int32_t)off16 << 2);
		MIPS64_ExecuteDelaySlot();
		MIPS64_SetRegPC(pc);
	} else {
		MIPS64_NullifyDelaySlot();
	}
}

/*
 * ----------------------------------------------------------
 * BGEZL
 * 	Branch on Greater Than or Equal to Zero Likely
 * 	This is a deprecated instruction because it does not 
 * 	execute the instruction in branch delay slot
 * ----------------------------------------------------------
 */
static inline void
mips64_BGEZL(void) {
	int rs = (ICODE >> 21) & 0x1f;	
	uint64_t Rs = MIPS64_ReadGpr(rs);
	uint64_t pc = GET_NIA;
	MIPS64_WriteGpr(31,pc+4);
	if(Rs >= 0) {
		int16_t off16 = ICODE & 0xffff;
		pc = pc + ((int32_t)off16 << 2);
		MIPS64_ExecuteDelaySlot();
		MIPS64_SetRegPC(pc);
	} else {
		MIPS64_NullifyDelaySlot();
	}

}

static inline void
mips64_TGEI(void) 
{
	int rs = (ICODE >> 21) & 0x1f;
	int16_t immediate = ICODE & 0xffff;
	int64_t Rs = MIPS64_ReadGpr(rs);
	if(Rs >= (int64_t)immediate) {
		// trap exception
	}

}

static inline void
mips64_TGEIU(void) 
{
	int rs = (ICODE >> 21) & 0x1f;
	uint64_t Rs = MIPS64_ReadGpr(rs);
	uint64_t immediate = (int64_t)(int16_t)(ICODE & 0xffff);
	if(Rs >= immediate)  {
		//trap exception
	}		 
}

static inline void
mips64_TLTI(void) 
{
	int rs = (ICODE >> 21) & 0x1f;
	int64_t Rs = MIPS64_ReadGpr(rs);
	int16_t immediate = (ICODE & 0xffff);
	if(Rs < (int64_t)immediate)  {
		//trap exception
	}		 

}

static inline void
mips64_TLTIU(void) 
{
	int rs = (ICODE >> 21) & 0x1f;
	uint64_t Rs = MIPS64_ReadGpr(rs);
	int16_t immediate = (ICODE & 0xffff);
	if(Rs < (int64_t)immediate)  {
		//trap exception
	}		 
	
}

static inline void
mips64_TEQI(void) 
{
	int rs = (ICODE >> 21) & 0x1f;
	int16_t imm = ICODE & 0xffff;
	int64_t Rs = MIPS64_ReadGpr(rs);
	if(Rs == imm) {
		// trap exception
	}	
	
}

static inline void
mips64_TNEI(void) 
{
	int rs = (ICODE >> 21) & 0x1f;
	int16_t imm = ICODE & 0xffff;
	int64_t Rs = MIPS64_ReadGpr(rs);
	if(Rs != imm) {
		// trap exception
	}	
}

static inline void
mips64_BLTZAL(void) {
	int rs = (ICODE >> 21) & 0x1f;	
	uint64_t pc = GET_NIA;
	uint64_t Rs = MIPS64_ReadGpr(rs);
	MIPS64_WriteGpr(31,pc+4);
	if(Rs < 0) {
		int16_t off16 = ICODE & 0xffff;
		pc = pc + ((int32_t)off16 << 2);
		MIPS64_ExecuteDelaySlot();
		MIPS64_SetRegPC(pc);
	}

}
static inline void
mips64_BGEZAL(void) {
	int rs = (ICODE >> 21) & 0x1f;	
	uint64_t Rs = MIPS64_ReadGpr(rs);
	uint64_t pc = GET_NIA;
	MIPS64_WriteGpr(31,pc+4);
	/* PC really changes during next instruction */
	if(Rs >= 0) {
		int16_t off16 = ICODE & 0xffff;
		pc = pc + ((int32_t)off16 << 2);
		MIPS64_ExecuteDelaySlot();
		MIPS64_SetRegPC(pc);
	}

}
/*
 * --------------------------------------------------------------
 * BLTZALL
 * 	Branch on Less Tahn Zero and Link Likely
 * 	This is a deprecated instruction which requires a hack
 * 	because it does not execute the delay slot
 * --------------------------------------------------------------
 */
static inline void
mips64_BLTZALL(void) {
	int rs = (ICODE >> 21) & 0x1f;	
	uint64_t pc = GET_NIA;
	uint64_t Rs = MIPS64_ReadGpr(rs);
	MIPS64_WriteGpr(31,pc+4);
	if(Rs < 0) {
		int16_t off16 = ICODE & 0xffff;
		pc = pc + ((int32_t)off16 << 2);
		MIPS64_ExecuteDelaySlot();
		MIPS64_SetRegPC(pc);
	} else {
		MIPS64_NullifyDelaySlot();
	}
}
/*
 * --------------------------------------------------------------
 * BGEZALL
 * 	Branch on Greater Than or Equal to Zero and Link Likely
 * 	This is a deprecated instruction which requires a hack
 * 	because it does not execute the delay slot
 * --------------------------------------------------------------
 */
static inline void
mips64_BGEZALL(void) {
	int rs = (ICODE >> 21) & 0x1f;	
	uint64_t Rs = MIPS64_ReadGpr(rs);
	uint64_t pc = GET_NIA;
	MIPS64_WriteGpr(31,pc+4);
	/* PC really changes during next instruction */
	if(Rs >= 0) {
		int16_t off16 = ICODE & 0xffff;
		pc = pc + 4 + ((int32_t)off16 << 2);
		MIPS64_ExecuteDelaySlot();
		MIPS64_SetRegPC(pc);
	} else {
		MIPS64_NullifyDelaySlot();
	}	
}

static inline void
mips64_SYNCI(void) 
{
/* TLB Refill and TLB invalid exception with cause TLBL can be triggered
   can be triggered here */ 
}

void
mips64_regimm(void) {
	int sub = (ICODE & 0x1f0000) >> 16; 
	switch(sub) {
		case 0:
			mips64_BLTZ();
			break;
		case 1:
			mips64_BGEZ();
			break;
		case 2:
			mips64_BLTZL();
			break;
		case 3:
			mips64_BGEZL();
			break;
		case 8:
			mips64_TGEI();
			break;
		case 9:
			mips64_TGEIU();
			break;
		case 10:
			mips64_TLTI();
			break;
		case 11:
			mips64_TLTIU();
			break;
		case 12:
			mips64_TEQI();
			break;
		case 14:
			mips64_TNEI();
			break;
		case 16:
			mips64_BLTZAL();
			break;
		case 17:
			mips64_BGEZAL();
			break;
		case 18:
			mips64_BLTZALL();
			break;
		case 19:
			mips64_BGEZALL();
			break;
		case 31:
			mips64_SYNCI();
			break;
		default: 
			mips64_UNDEFINED();	
			break;
		
	}
}

void
mips64_SLL(void) 
{
	int rt = (ICODE >> 16) & 0x1f;	
	int rd = (ICODE >> 11) & 0x1f;	
	int sa = (ICODE >> 6) & 0x1f;	
	uint64_t Rt = MIPS64_ReadGpr(rt);
	int32_t temp;
	temp = Rt << sa;	
	MIPS64_WriteGpr(rd,(int64_t)temp); 
}

void
mips64_MOVCI(void) 
{
	/* Floating point move conditional */	
	fprintf(stderr,"instruction MOVCI not implemented\n");
}

void
mips64_SRL(void) 
{
	int rt = (ICODE >> 16) & 0x1f;	
	int rd = (ICODE >> 11) & 0x1f;	
	int sa = (ICODE >> 6) & 0x1f;	
	uint32_t Rt = MIPS64_ReadGpr(rt);
	int32_t temp;
	temp = Rt >> sa;	
	MIPS64_WriteGpr(rd,(int64_t)temp); 
}

void
mips64_SRA(void) {
	int rt = (ICODE >> 16) & 0x1f;	
	int rd = (ICODE >> 11) & 0x1f;	
	int sa = (ICODE >> 6) & 0x1f;	
	int32_t Rt = MIPS64_ReadGpr(rt);
	int32_t temp;
	temp = Rt >> sa;	
	MIPS64_WriteGpr(rd,(int64_t)temp); 
}

void
mips64_SLLV(void) 
{
	int rs = (ICODE >> 21) & 0x1f;	
	int rt = (ICODE >> 16) & 0x1f;	
	int rd = (ICODE >> 11) & 0x1f;	
	uint64_t Rt = MIPS64_ReadGpr(rt);
	uint64_t Rs = MIPS64_ReadGpr(rs);
	int32_t temp;
	temp = Rt << (Rs & 0x1f);	
	MIPS64_WriteGpr(rd,(int64_t)temp); 
}

void
mips64_SRLV(void) {
	int rs = (ICODE >> 21) & 0x1f;	
	int rt = (ICODE >> 16) & 0x1f;	
	int rd = (ICODE >> 11) & 0x1f;	
	uint32_t Rt = MIPS64_ReadGpr(rt);
	uint32_t Rs = MIPS64_ReadGpr(rs);
	int32_t temp;
	temp = Rt >> (Rs & 0x1f);	
	MIPS64_WriteGpr(rd,(int64_t)temp); 
}

void
mips64_SRAV(void) {
	int rs = (ICODE >> 21) & 0x1f;	
	int rt = (ICODE >> 16) & 0x1f;	
	int rd = (ICODE >> 11) & 0x1f;	
	uint32_t Rt = MIPS64_ReadGpr(rt);
	int32_t Rs = MIPS64_ReadGpr(rs);
	int32_t temp;
	temp = Rt >> (Rs & 0x1f);	
	MIPS64_WriteGpr(rd,(int64_t)temp); 
}

void
mips64_JR(void) {
	int rs = (ICODE >> 21) & 0x1f;
	uint64_t Rs = MIPS64_ReadGpr(rs);	
	MIPS64_ExecuteDelaySlot();
	MIPS64_SetRegPC(Rs);
}

/*
 ****************************************************************************
 * Can be rexecuted if there is an exception in Branch delay slot
 ****************************************************************************
 */
void
mips64_JALR(void) {
	int rs = (ICODE >> 21) & 0x1f;
	int rd = (ICODE >> 11) & 0x1f;	
	uint64_t return_addr;		
	uint64_t Rs = MIPS64_ReadGpr(rs);	
	return_addr = GET_NIA + 4;	
	MIPS64_WriteGpr(rd,return_addr);
	MIPS64_ExecuteDelaySlot();
	MIPS64_SetRegPC(Rs);
}

void
mips64_MOVZ(void) {
	int rs = (ICODE >> 21) & 0x1f;
	int rt = (ICODE >> 16) & 0x1f;	
	int rd = (ICODE >> 11) & 0x1f;	
	uint64_t Rt,Rs;	
	Rt = MIPS64_ReadGpr(rt);
	if(Rt == 0) {
		Rs = MIPS64_ReadGpr(rs);
		MIPS64_WriteGpr(rd,Rs);	
	}	
}

void
mips64_MOVN(void) 
{
	int rs = (ICODE >> 21) & 0x1f;
	int rt = (ICODE >> 16) & 0x1f;	
	int rd = (ICODE >> 11) & 0x1f;	
	uint64_t Rt,Rs;	
	Rt = MIPS64_ReadGpr(rt);
	if(Rt != 0) {
		Rs = MIPS64_ReadGpr(rs);
		MIPS64_WriteGpr(rd,Rs);	
	}	
}

void
mips64_SYSCALL(void) {
	/* Trigger a system call exception here */
	fprintf(stderr,"instruction SYSCALL not implemented\n");
}

void
mips64_BREAK(void) {
	/* trigger a breakpoint exception here */ 
	fprintf(stderr,"instruction BREAK not implemented\n");
}

void
mips64_SYNC(void) {
	fprintf(stderr,"instruction SYNC not implemented\n");
}

void
mips64_MFHI(void) {
	int rd = (ICODE	>> 11) & 0x1f;
	uint64_t Hi;
	Hi = MIPS64_GetHi();	
	MIPS64_WriteGpr(rd,Hi);	
}

void
mips64_MTHI(void) {
	int rs = (ICODE	>> 21) & 0x1f;
	uint64_t Rs;
	Rs = MIPS64_ReadGpr(rs);	
	MIPS64_SetHi(Rs);	
}

void
mips64_MFLO(void) {
	int rd = (ICODE	>> 11) & 0x1f;
	uint64_t Lo;
	Lo = MIPS64_GetLo();	
	MIPS64_WriteGpr(rd,Lo);	
}

void
mips64_MTLO(void) {
	int rs = (ICODE	>> 21) & 0x1f;
	uint64_t Rs;
	Rs = MIPS64_ReadGpr(rs);	
	MIPS64_SetLo(Rs);	
}

void
mips64_DSLLV(void) {
	int rs = (ICODE >> 21) & 0x1f;	
	int rt = (ICODE >> 16) & 0x1f;	
	int rd = (ICODE >> 11) & 0x1f;	
	uint64_t Rt = MIPS64_ReadGpr(rt);
	uint64_t Rs = MIPS64_ReadGpr(rs);
	MIPS64_WriteGpr(rd,Rt << (Rs & 0x3f)); 
}

void
mips64_DSRLV(void) 
{
	int rs = (ICODE >> 21) & 0x1f;	
	int rt = (ICODE >> 16) & 0x1f;	
	int rd = (ICODE >> 11) & 0x1f;	
	uint64_t Rt = MIPS64_ReadGpr(rt);
	uint32_t Rs = MIPS64_ReadGpr(rs);
	MIPS64_WriteGpr(rd,Rt >> (Rs & 0x3f)); 
}

void
mips64_DSRAV(void) 
{
	int rs = (ICODE >> 21) & 0x1f;	
	int rt = (ICODE >> 16) & 0x1f;	
	int rd = (ICODE >> 11) & 0x1f;	
	int64_t Rt = MIPS64_ReadGpr(rt);
	uint32_t Rs = MIPS64_ReadGpr(rs);
	MIPS64_WriteGpr(rd,Rt >> (Rs & 0x3f)); 
}

void
mips64_MULT(void) {
	int rs = (ICODE	 >> 21) & 0x1f;
	int rt = (ICODE >> 16) & 0x1f;	
	int64_t Rs, Rt;	
	int64_t result;
	int64_t hi,lo;
	Rs = MIPS64_ReadGpr(rs);	
	Rt = MIPS64_ReadGpr(rt);	
	result = Rs*Rt;
	hi = (int64_t)(int32_t)(result >> 32);
	lo = (int64_t)(int32_t)(result);
	MIPS64_SetHi(hi);
	MIPS64_SetLo(lo);	
}

void
mips64_MULTU(void) {
	int rs = (ICODE	 >> 21) & 0x1f;
	int rt = (ICODE >> 16) & 0x1f;	
	uint64_t Rs, Rt;	
	uint64_t result;
	int64_t hi,lo;
	Rs = MIPS64_ReadGpr(rs);	
	Rt = MIPS64_ReadGpr(rt);	
	result = Rs*Rt;
	hi = (int64_t)(int32_t)(result >> 32);
	lo = (int64_t)(int32_t)(result);
	MIPS64_SetHi(hi);
	MIPS64_SetLo(lo);	
}

void
mips64_DIV(void) {
	int rs = (ICODE >> 21) & 0x1f;	
	int rt = (ICODE >> 16) & 0x1f;
	int32_t Rs,Rt;
	int64_t Hi,Lo;
	Rs = MIPS64_ReadGpr(rs);
	Rt = MIPS64_ReadGpr(rt);
	if(Rt == 0) {
		/* Result is unpredictable */
		return;
	}
	Lo = Rs / Rt;
	Hi = Rs - (Rt * Lo);
	
	MIPS64_SetLo((int64_t)Lo); 
	MIPS64_SetHi((int64_t)Hi); 
}

void
mips64_DIVU(void) {
	int rs = (ICODE >> 21) & 0x1f;	
	int rt = (ICODE >> 16) & 0x1f;
	uint32_t Rs,Rt;
	uint64_t Hi,Lo;
	Rs = MIPS64_ReadGpr(rs);
	Rt = MIPS64_ReadGpr(rt);
	if(Rt == 0) {
		/* Result is unpredictable */
		return;
	}
	Lo = Rs / Rt;
	Hi = Rs - (Rt * Lo);
	
	MIPS64_SetLo((int64_t)(int32_t)Lo); 
	MIPS64_SetHi((int64_t)(int32_t)Hi); 
		
	fprintf(stderr,"instruction DIVU not implemented\n");
}

void
mips64_DMULT(void) {
	int rs = (ICODE >> 21) & 0x1f;
	int rt = (ICODE >> 16) & 0x1f;	
	uint64_t Rs,Rt;	
	uint64_t Hi,Lo;
	int neg = 0;	
	Rs = MIPS64_ReadGpr(rs);	
	Rt = MIPS64_ReadGpr(rt);	
	if((int64_t)Rs < 0) {
		Rs = -Rs;	
		neg = 1 - neg;
	}	
	if((int64_t)Rt < 0) {
		Rt = -Rt;	
		neg = 1 - neg;
	}	
	Lo = Rs * Rt;	
	Hi = (Rs >> 32) * (Rt >> 32) + (((Rs >> 32) * Rt) >> 32) + ((Rs * (Rt >> 32)) >> 32); 	
	if(neg) {
		Lo = (~Lo + 1);
		Hi = ~Hi;	
		if(Lo == 0) {
			Hi++;
		}	
	}	
	MIPS64_SetLo(Lo);
	MIPS64_SetHi(Hi);
}

void
mips64_DMULTU(void) {
	int rs = (ICODE >> 21) & 0x1f;
	int rt = (ICODE >> 16) & 0x1f;	
	
	uint64_t Rs,Rt;	
	uint64_t Hi,Lo;
	Rs = MIPS64_ReadGpr(rs);	
	Rt = MIPS64_ReadGpr(rt);	
	Lo = Rs * Rt;	
	Hi = (Rs >> 32) * (Rt >> 32) + (((Rs >> 32) * Rt) >> 32) + ((Rs * (Rt >> 32)) >> 32); 	
	MIPS64_SetLo(Lo);
	MIPS64_SetHi(Hi);

}

void
mips64_DDIV(void) {
	int rs = (ICODE >> 21) & 0x1f;
	int rt = (ICODE >> 16) & 0x1f;
	int64_t Rs,Rt,result;
	Rs = MIPS64_ReadGpr(rs);
	Rt = MIPS64_ReadGpr(rt);
	if(Rt) {
		result = Rs / Rt;
		MIPS64_SetLo(result);
		MIPS64_SetHi(Rs - (result * Rt));
	}
}

void
mips64_DDIVU(void) {
	int rs = (ICODE >> 21) & 0x1f;
	int rt = (ICODE >> 16) & 0x1f;
	uint64_t Rs,Rt,result;
	Rs = MIPS64_ReadGpr(rs);
	Rt = MIPS64_ReadGpr(rt);
	if(Rt) {
		result = Rs / Rt;
		MIPS64_SetLo(result);
		MIPS64_SetHi(Rs - (result * Rt));
	}
}

void
mips64_ADD(void) {
	int32_t result;
	int rs = (ICODE >> 21) & 0x1f;
	int rt = (ICODE >> 16) & 0x1f;
	int rd = (ICODE >> 11) & 0x1f;
	int32_t op1,op2;
	op1 = MIPS64_ReadGpr(rs);
	op2 = MIPS64_ReadGpr(rt);
	result = op1 + op2; 
	if(add32_overflow(op1,op2,result)) {
		// exception
		//MIPS64_SignalException(IntegerOverflow);
	}  else {
		MIPS64_WriteGpr(rd,(int64_t)result); 
	}
}

void
mips64_ADDU(void) {
	int32_t result;
	int rs = (ICODE >> 21) & 0x1f;
	int rt = (ICODE >> 16) & 0x1f;
	int rd = (ICODE >> 11) & 0x1f;
	int32_t op1,op2;
	op1 = MIPS64_ReadGpr(rs);
	op2 = MIPS64_ReadGpr(rt);
	result = op1 + op2; 
	MIPS64_WriteGpr(rd,(int64_t)result);
	fprintf(stderr,"instruction ADDU not implemented\n");
}

void
mips64_SUB(void) {
	int rs = (ICODE >> 21) & 0x1f;
	int rt = (ICODE >> 16) & 0x1f;
	int rd = (ICODE >> 11) & 0x1f;
	int32_t Rs,Rt,Rd; 
	Rs = MIPS64_ReadGpr(rs);
	Rt = MIPS64_ReadGpr(rt);	
	Rd = Rs - Rt;
	if(sub32_overflow(Rs,Rt,Rd)) {
		// exception
	} else {
		MIPS64_WriteGpr(rd,(int64_t)Rd);
	}
}

void
mips64_SUBU(void) {
	int rs = (ICODE >> 21) & 0x1f;
	int rt = (ICODE >> 16) & 0x1f;
	int rd = (ICODE >> 11) & 0x1f;
	int32_t Rs,Rt,Rd; 
	Rs = MIPS64_ReadGpr(rs);
	Rt = MIPS64_ReadGpr(rt);	
	Rd = Rs - Rt;
	MIPS64_WriteGpr(rd,(int64_t)Rd);
}

void
mips64_AND(void) {
	uint64_t result;
	int rs = (ICODE >> 21) & 0x1f;
	int rt = (ICODE >> 16) & 0x1f;
	int rd = (ICODE >> 11) & 0x1f;
	uint64_t op1,op2;
	op1 = MIPS64_ReadGpr(rs);
	op2 = MIPS64_ReadGpr(rt);
	result = op1  & op2; 
	MIPS64_WriteGpr(rd,result);
}

void
mips64_OR(void) {
	uint64_t result;
	int rs = (ICODE >> 21) & 0x1f;
	int rt = (ICODE >> 16) & 0x1f;
	int rd = (ICODE >> 11) & 0x1f;
	uint64_t op1,op2;
	op1 = MIPS64_ReadGpr(rs);
	op2 = MIPS64_ReadGpr(rt);
	result = op1  | op2; 
	MIPS64_WriteGpr(rd,result);
}

void
mips64_XOR(void) {
	uint64_t result;
	int rs = (ICODE >> 21) & 0x1f;
	int rt = (ICODE >> 16) & 0x1f;
	int rd = (ICODE >> 11) & 0x1f;
	uint64_t op1,op2;
	op1 = MIPS64_ReadGpr(rs);
	op2 = MIPS64_ReadGpr(rt);
	result = op1 ^ op2; 
	MIPS64_WriteGpr(rd,result);
}

void
mips64_NOR(void) {
	uint64_t result;
	int rs = (ICODE >> 21) & 0x1f;
	int rt = (ICODE >> 16) & 0x1f;
	int rd = (ICODE >> 11) & 0x1f;
	uint64_t op1,op2;
	op1 = MIPS64_ReadGpr(rs);
	op2 = MIPS64_ReadGpr(rt);
	result = op1  | ~op2; 
	MIPS64_WriteGpr(rd,result);
}

void
mips64_SLT(void) {
	int rs = (ICODE >> 21) & 0x1f;
	int rt = (ICODE >> 16) & 0x1f;
	int rd = (ICODE >> 11) & 0x1f;
	int64_t Rs,Rt;
	int result = 0;
	Rs = MIPS64_ReadGpr(rs);
	Rt = MIPS64_ReadGpr(rt);
	if(Rs < Rt) {
		result = 1;
	} else {
		result = 0;
	}
	MIPS64_WriteGpr(rd,result);
}

void
mips64_SLTU(void) {
	int rs = (ICODE >> 21) & 0x1f;
	int rt = (ICODE >> 16) & 0x1f;
	int rd = (ICODE >> 11) & 0x1f;
	uint64_t Rs,Rt;
	int result = 0;
	Rs = MIPS64_ReadGpr(rs);
	Rt = MIPS64_ReadGpr(rt);
	if(Rs < Rt) {
		result = 1;
	} else {
		result = 0;
	}
	MIPS64_WriteGpr(rd,result);
}

void
mips64_DADD(void) {
	uint64_t result;
	int rs = (ICODE >> 21) & 0x1f;
	int rt = (ICODE >> 16) & 0x1f;
	int rd = (ICODE >> 11) & 0x1f;
	uint64_t op1,op2;
	op1 = MIPS64_ReadGpr(rs);
	op2 = MIPS64_ReadGpr(rt);
	result = op1 + op2; 
	if(add64_overflow(op1,op2,result)) {
		// exception
		//MIPS64_SignalException(IntegerOverflow);
	}  else {
		MIPS64_WriteGpr(rd,result); 
	}
}

void
mips64_DADDU(void) {
	uint64_t result;
	int rs = (ICODE >> 21) & 0x1f;
	int rt = (ICODE >> 16) & 0x1f;
	int rd = (ICODE >> 11) & 0x1f;
	uint64_t op1,op2;
	op1 = MIPS64_ReadGpr(rs);
	op2 = MIPS64_ReadGpr(rt);
	result = op1 + op2; 
	MIPS64_WriteGpr(rd,result); 
}

void
mips64_DSUB(void) {
	int rs = (ICODE >> 21) & 0x1f;
	int rt = (ICODE >> 16) & 0x1f;
	int rd = (ICODE >> 11) & 0x1f;
	int64_t Rs,Rt,Rd; 
	Rs = MIPS64_ReadGpr(rs);
	Rt = MIPS64_ReadGpr(rt);	
	Rd = Rs - Rt;
	if(sub64_overflow(Rs,Rt,Rd)) {
		// exception
	} else {
		MIPS64_WriteGpr(rd,(int64_t)Rd);
	}
}

void
mips64_DSUBU(void) {
	int rs = (ICODE >> 21) & 0x1f;
	int rt = (ICODE >> 16) & 0x1f;
	int rd = (ICODE >> 11) & 0x1f;
	int64_t Rs,Rt,Rd; 
	Rs = MIPS64_ReadGpr(rs);
	Rt = MIPS64_ReadGpr(rt);	
	Rd = Rs - Rt;
	MIPS64_WriteGpr(rd,(int64_t)Rd);
}

void
mips64_TGE(void) {
	int rs = (ICODE >> 21) & 0x1f;
	int rt = (ICODE >> 16) & 0x1f;
	int64_t Rs,Rt; 	
	Rs = MIPS64_ReadGpr(rs);
	Rt = MIPS64_ReadGpr(rt);	
	if(Rs>=Rt) {
		fprintf(stderr,"Trap for TGE not implemented\n");
	}
}

void
mips64_TGEU(void) {
	int rs = (ICODE >> 21) & 0x1f;
	int rt = (ICODE >> 16) & 0x1f;
	uint64_t Rs,Rt; 	
	Rs = MIPS64_ReadGpr(rs);
	Rt = MIPS64_ReadGpr(rt);	
	if(Rs>=Rt) {
		fprintf(stderr,"Trap for TGEU not implemented\n");
	}
}

void
mips64_TLT(void) {
	int rs = (ICODE >> 21) & 0x1f;
	int rt = (ICODE >> 16) & 0x1f;
	int64_t Rs,Rt; 	
	Rs = MIPS64_ReadGpr(rs);
	Rt = MIPS64_ReadGpr(rt);	
	if(Rs<Rt) {
		fprintf(stderr,"Trap for TLT not implemented\n");
	}
}

void
mips64_TLTU(void) {
	int rs = (ICODE >> 21) & 0x1f;
	int rt = (ICODE >> 16) & 0x1f;
	uint64_t Rs,Rt; 	
	Rs = MIPS64_ReadGpr(rs);
	Rt = MIPS64_ReadGpr(rt);	
	if(Rs<Rt) {
		fprintf(stderr,"Trap for TLTU not implemented\n");
	}
}

void
mips64_TEQ(void) {
	int rs = (ICODE >> 21) & 0x1f;
	int rt = (ICODE >> 16) & 0x1f;
	uint64_t Rs,Rt; 	
	Rs = MIPS64_ReadGpr(rs);
	Rt = MIPS64_ReadGpr(rt);	
	if(Rs==Rt) {
		fprintf(stderr,"Trap for TEQ not implemented\n");
	}
	
}

void
mips64_TNE(void) {
	int rs = (ICODE >> 21) & 0x1f;
	int rt = (ICODE >> 16) & 0x1f;
	uint64_t Rs,Rt; 	
	Rs = MIPS64_ReadGpr(rs);
	Rt = MIPS64_ReadGpr(rt);	
	if(Rs!=Rt) {
		fprintf(stderr,"Trap for TNE not implemented\n");
	}
}

void
mips64_DSLL(void) 
{
	int rt = (ICODE >> 16) & 0x1f;	
	int rd = (ICODE >> 11) & 0x1f;	
	int sa = (ICODE >> 6) & 0x1f;	
	uint64_t Rt = MIPS64_ReadGpr(rt);
	MIPS64_WriteGpr(rd,Rt << sa); 
}

void
mips64_DSRL(void) 
{
	int rt = (ICODE >> 16) & 0x1f;	
	int rd = (ICODE >> 11) & 0x1f;	
	int sa = (ICODE >> 6) & 0x1f;	
	uint64_t Rt = MIPS64_ReadGpr(rt);
	MIPS64_WriteGpr(rd,Rt>>sa); 
}

void
mips64_DSRA(void) 
{
	int rt = (ICODE >> 16) & 0x1f;	
	int rd = (ICODE >> 11) & 0x1f;	
	int sa = (ICODE >> 6) & 0x1f;	
	int64_t Rt = MIPS64_ReadGpr(rt);
	MIPS64_WriteGpr(rd,Rt >> sa); 
}

void
mips64_DSLL32(void) {
	int rt = (ICODE >> 16) & 0x1f;	
	int rd = (ICODE >> 11) & 0x1f;	
	int sa = (ICODE >> 6) & 0x1f;	
	uint64_t Rt = MIPS64_ReadGpr(rt);
	MIPS64_WriteGpr(rd,Rt << (sa+32)); 
}

void
mips64_DSRL32(void) {
	int rt = (ICODE >> 16) & 0x1f;	
	int rd = (ICODE >> 11) & 0x1f;	
	int sa = (ICODE >> 6) & 0x1f;	
	uint64_t Rt = MIPS64_ReadGpr(rt);
	MIPS64_WriteGpr(rd,Rt >> (sa+32)); 
}

void
mips64_DSRA32(void) 
{
	int rt = (ICODE >> 16) & 0x1f;	
	int rd = (ICODE >> 11) & 0x1f;	
	int sa = (ICODE >> 6) & 0x1f;	
	int64_t Rt = MIPS64_ReadGpr(rt);
	MIPS64_WriteGpr(rd,Rt >> (sa+32)); 
}

void
mips64_J(void) 
{
	uint64_t instr_index = ICODE & 0x03ffFFFF;
	uint64_t pc = GET_CIA;
	MIPS64_ExecuteDelaySlot();
	pc = pc & 0xFFFFffffF0000000ULL;
	pc |= instr_index << 2;
	MIPS64_SetRegPC(pc);
}

void
mips64_JAL(void) {
	uint64_t instr_index = ICODE & 0x03ffFFFF;
	uint64_t pc = GET_CIA;
	MIPS64_ExecuteDelaySlot();
	MIPS64_WriteGpr(31,pc+8);
	pc = pc & 0xFFFFffffF0000000ULL;
	pc |= instr_index << 2;
	MIPS64_SetRegPC(pc);
}

void
mips64_BEQ(void) {
	int rs = (ICODE >> 21) & 0x1f;
	int rt = (ICODE >> 16) & 0x1f;
	int16_t off16 = (ICODE & 0xffff);
	uint64_t op1 = MIPS64_ReadGpr(rs);
	uint64_t op2 = MIPS64_ReadGpr(rt);
	if(op1 == op2) {
		uint64_t pc = GET_NIA; 
		pc = pc + ((int32_t)off16 << 2);
		MIPS64_ExecuteDelaySlot();
		MIPS64_SetRegPC(pc);
	}
}

void
mips64_BNE(void) {
	int rs = (ICODE >> 21) & 0x1f;	
	uint64_t Rs = MIPS64_ReadGpr(rs);
	if(Rs != 0) {
		int16_t off16 = ICODE & 0xffff;
		uint64_t pc = GET_NIA;
		pc = pc + ((int32_t)off16 << 2);
		MIPS64_ExecuteDelaySlot();
		MIPS64_SetRegPC(pc);
	}
}

void
mips64_BLEZ(void) {
	int rs = (ICODE >> 21) & 0x1f;	
	uint64_t Rs = MIPS64_ReadGpr(rs);
	if(Rs <= 0) {
		int16_t off16 = ICODE & 0xffff;
		uint64_t pc = GET_NIA;
		pc = pc + ((int32_t)off16 << 2);
		MIPS64_ExecuteDelaySlot();
		MIPS64_SetRegPC(pc);
	}
}

void
mips64_BGTZ(void) {
	int rs = (ICODE >> 21) & 0x1f;	
	uint64_t Rs = MIPS64_ReadGpr(rs);
	if(Rs > 0) {
		int16_t off16 = ICODE & 0xffff;
		uint64_t pc = GET_NIA;
		pc = pc + ((int32_t)off16 << 2);
		MIPS64_ExecuteDelaySlot();
		MIPS64_SetRegPC(pc);
	}
}

void
mips64_ADDI(void) {
	int32_t result;
	int rs = (ICODE >> 21) & 0x1f;
	int rt = (ICODE >> 16) & 0x1f;
	int32_t op1;
	int16_t op2;
	op1 = MIPS64_ReadGpr(rs);
	op2 = ICODE & 0xffff;
	result = op1 + op2; 
	if(add32_overflow(op1,op2,result)) {
		// MIPS64_SignalException(IntegerOverflow);
		// exception
	} else {
	 	MIPS64_WriteGpr(rt,(int64_t)result);
	}
}

void
mips64_ADDIU(void) {
	int32_t result;
	int rs = (ICODE >> 21) & 0x1f;
	int rt = (ICODE >> 16) & 0x1f;
	int32_t op1;
	int16_t op2;
	op1 = MIPS64_ReadGpr(rs);
	op2 = ICODE & 0xffff;
	result = op1 + op2; 
	MIPS64_WriteGpr(rt,(int64_t)result);
}

void
mips64_SLTI(void) {
	int rs = (ICODE >> 21) & 0x1f;
	int rt = (ICODE >> 16) & 0x1f;
	int rd = (ICODE >> 11) & 0x1f;
	int64_t Rs,Rt;
	int result = 0;
	Rs = MIPS64_ReadGpr(rs);
	Rt = MIPS64_ReadGpr(rt);
	if(Rs < Rt) {
		result = 1;
	} else {
		result = 0;
	}
	MIPS64_WriteGpr(rd,result);
}

void
mips64_SLTIU(void) {
	int rs = (ICODE >> 21) & 0x1f;
	int rt = (ICODE >> 16) & 0x1f;
	int rd = (ICODE >> 11) & 0x1f;
	uint64_t Rs,Rt;
	int result = 0;
	Rs = MIPS64_ReadGpr(rs);
	Rt = MIPS64_ReadGpr(rt);
	if(Rs < Rt) {
		result = 1;
	} else {
		result = 0;
	}
	MIPS64_WriteGpr(rd,result);
}

void
mips64_ANDI(void) {
	uint64_t result;
	int rs = (ICODE >> 21) & 0x1f;
	int rt = (ICODE >> 16) & 0x1f;
	uint16_t imm = (ICODE & 0xffff);
	uint64_t op1;
	op1 = MIPS64_ReadGpr(rs);
	result = op1  & imm; 
	MIPS64_WriteGpr(rt,result);
}

void
mips64_ORI(void) {
	int rs = (ICODE >> 21) & 0x1f;
	int rt = (ICODE >> 16) & 0x1f;
	uint16_t imm = (ICODE & 0xffff);
	uint64_t Rs = MIPS64_ReadGpr(rs);
	uint64_t Rt = Rs | imm; 
	MIPS64_WriteGpr(rt,Rt);
}

void
mips64_XORI(void) {
	int rs = (ICODE >> 21) & 0x1f;
	int rt = (ICODE >> 16) & 0x1f;
	uint16_t imm = (ICODE & 0xffff);
	uint64_t Rs = MIPS64_ReadGpr(rs);
	uint64_t Rt = Rs ^ imm; 
	MIPS64_WriteGpr(rt,Rt);
}

void
mips64_LUI(void) {
	int rt = (ICODE >> 16) & 0x1f;
	int64_t upper_imm = (int64_t)(int32_t)((ICODE & 0xffff) << 16);
	MIPS64_WriteGpr(rt,upper_imm);
}

static inline void
mips64_COP0_MFC0(void) 
{
	fprintf(stderr,"instruction MFC0 not implemented\n");
}

static inline void
mips64_COP0_DMFC0(void) 
{
	fprintf(stderr,"instruction DMFC0 not implemented\n");
}

static inline void
mips64_COP0_MTC0(void) 
{
	fprintf(stderr,"instruction MTC0 not implemented\n");
}

static inline void
mips64_COP0_DMTC0(void) 
{
	fprintf(stderr,"instruction DMTC0 not implemented\n");
}

static inline void
mips64_COP0_RDPGPR(void) 
{
	fprintf(stderr,"instruction RDPGPR not implemented\n");
}

static inline void
mips64_COP0_MFMC0_EI() {
	// status update
}
static inline void
mips64_COP0_MFMC0_DI() {
	// status update:
}
static inline void
mips64_COP0_MFMC0(void) 
{
	if((ICODE & 0xffe0ffff) == 0x41606020) {
		mips64_COP0_MFMC0_EI();
	} else if((ICODE & 0xffe0ffff) == 0x41606000) {
		mips64_COP0_MFMC0_DI();
	} else {
		fprintf(stderr,"instruction MFMC0 can not be decoded\n");
	}
}

static inline void
mips64_COP0_WRPGPR(void) 
{
	fprintf(stderr,"instruction WRPGPR not implemented\n");
}

static inline void
mips64_COP0_C0_TLBR()
{
	fprintf(stderr,"instruction TLBR not implemented\n");
}

static inline void
mips64_COP0_C0_TLBWI()
{
	fprintf(stderr,"instruction TLWI not implemented\n");
}

static inline void
mips64_COP0_C0_TLBWR()
{
	fprintf(stderr,"instruction TLBWR not implemented\n");
}

static inline void
mips64_COP0_C0_TLBP()
{
	fprintf(stderr,"instruction TLBP not implemented\n");
}

/*
 * Return from exception. Clears the Atomic access LLBit 
 */
static inline void
mips64_COP0_C0_ERET()
{
	/* fail pending atomic access  */
	MIPS64_SetLLBit(0);
	fprintf(stderr,"instruction ERET not implemented\n");
}

static inline void
mips64_COP0_C0_DERET()
{
	fprintf(stderr,"instruction DERET not implemented\n");
}

static inline void
mips64_COP0_C0_WAIT()
{
	fprintf(stderr,"instruction WAIT not implemented\n");
}

static inline void
mips64_COP0_C0(void) 
{
	int func = ICODE & 0x3f;
	switch(func) {
		case 1: 
			mips64_COP0_C0_TLBR();
			break;
		case 2:
			mips64_COP0_C0_TLBWI();
			break;
		case 6:
			mips64_COP0_C0_TLBWR();
			break;
		case 8:
			mips64_COP0_C0_TLBP();
			break;
		case 24:
			mips64_COP0_C0_ERET();
			break;
		case 31:
			mips64_COP0_C0_DERET();
			break;

		case 32:
			mips64_COP0_C0_WAIT();
			break;

		default:
			mips64_UNDEFINED();
			break;
	}
	fprintf(stderr,"instruction WRPGPR not implemented\n");
}

void
mips64_COP0(void) 
{
	int rs = (ICODE >> 21) & 0x1f;
	switch(rs) {

		case 0:
			mips64_COP0_MFC0();
			break;
		case 1:
			mips64_COP0_DMFC0();
			break;
		case 4:
			mips64_COP0_MTC0();
			break;
		case 5:
			mips64_COP0_DMTC0();
			break;
		case 10:
			mips64_COP0_RDPGPR();
			break;
		case 11:
			mips64_COP0_MFMC0();
			break;
		case 14:
			mips64_COP0_WRPGPR();
			break;
		case 2:
		case 3:
		case 6:
		case 7:
		case 8:
		case 9:
		case 12:
		case 13:
		case 15:
			mips64_UNDEFINED();
			break;
		default:
			mips64_COP0_C0();
			break;

	}
}

void
mips64_COP1(void) 
{
	fprintf(stderr,"instruction COP1 not implemented\n");
}

void
mips64_COP2(void) 
{
	fprintf(stderr,"instruction COP2 not implemented\n");
}

void
mips64_COP1X(void) 
{
	fprintf(stderr,"instruction COP1X not implemented\n");
}

/*
 * ----------------------------------------------------------
 * BEQL
 * 	Branch on Equal Likely
 * 	This is a deprecated instruction because it does not 
 * 	execute the instruction in branch delay slot
 * ----------------------------------------------------------
 */
void
mips64_BEQL(void) {
	int16_t off16 = (ICODE & 0xffff);
	int rs = (ICODE >> 21) & 0x1f;
	int rt = (ICODE >> 16) & 0x1f;
	uint64_t op1 = MIPS64_ReadGpr(rs);
	uint64_t op2 = MIPS64_ReadGpr(rt);
	if(op1 == op2) {
		uint64_t pc = GET_NIA;
		pc = pc + ((int32_t)off16 << 2);
		MIPS64_ExecuteDelaySlot();
		MIPS64_SetRegPC(pc);
	} else {
		MIPS64_NullifyDelaySlot();
	}
}

/*
 * ----------------------------------------------------------
 * BNEL
 * 	Branch on not Equal Likely
 * 	This is a deprecated instruction because it does not 
 * 	execute the instruction in branch delay slot
 * ----------------------------------------------------------
 */

void
mips64_BNEL(void) {
	int rs = (ICODE >> 21) & 0x1f;	
	uint64_t Rs = MIPS64_ReadGpr(rs);
	if(Rs != 0) {
		int16_t off16 = ICODE & 0xffff;
		uint64_t pc = GET_NIA;
		pc = pc + ((int32_t)off16 << 2);
		MIPS64_ExecuteDelaySlot();
		MIPS64_SetRegPC(pc);
	} else {
		MIPS64_NullifyDelaySlot();
	}
}


/*
 * ----------------------------------------------------------
 * BLEZL
 * Branch on Less Than or Equal to Zero Likely 
 * This is a deprecated instruction because it does not 
 * execute the instruction in branch delay slot
 * ----------------------------------------------------------
 */
void
mips64_BLEZL(void) {
	int rs = (ICODE >> 21) & 0x1f;	
	uint64_t Rs = MIPS64_ReadGpr(rs);
	if(Rs <= 0) {
		int16_t off16 = ICODE & 0xffff;
		uint64_t pc = GET_NIA;
		pc = pc + ((int32_t)off16 << 2);
		MIPS64_ExecuteDelaySlot();
		MIPS64_SetRegPC(pc);
	} else {
		MIPS64_NullifyDelaySlot();
	}
}

/*
 * ----------------------------------------------------------
 * BGTZL
 * Branch on Greater Than Zero Likely
 * This is a deprecated instruction because it does not 
 * execute the instruction in branch delay slot
 * ----------------------------------------------------------
 */
void
mips64_BGTZL(void) {
	int rs = (ICODE >> 21) & 0x1f;	
	uint64_t Rs = MIPS64_ReadGpr(rs);
	if(Rs > 0) {
		int16_t off16 = ICODE & 0xffff;
		uint64_t pc = GET_NIA;
		pc = pc + ((int32_t)off16 << 2);
		MIPS64_ExecuteDelaySlot();
		MIPS64_SetRegPC(pc);
	} else {
		MIPS64_NullifyDelaySlot();
	}
}

void
mips64_DADDI(void) {
	uint64_t result;
	int rs = (ICODE >> 21) & 0x1f;
	int rt = (ICODE >> 16) & 0x1f;
	uint64_t op1;
	int16_t op2;
	op1 = MIPS64_ReadGpr(rs);
	op2 = ICODE & 0xffff;
	result = op1 + op2; 
	if(add32_overflow(op1,op2,result)) {
		// MIPS64_SignalException(IntegerOverflow);
		// exception
	} else {
	 	MIPS64_WriteGpr(rt,(int64_t)result);
	}
}

void
mips64_DADDIU(void) {
	uint64_t result;
	int rs = (ICODE >> 21) & 0x1f;
	int rt = (ICODE >> 16) & 0x1f;
	uint64_t op1;
	int16_t op2;
	op1 = MIPS64_ReadGpr(rs);
	op2 = ICODE & 0xffff;
	result = op1 + op2; 
	MIPS64_WriteGpr(rt,(int64_t)result);
}

void
mips64_LDL(void) {
	int big_endian = 1;
        int shift;
	int base = (ICODE >> 21) & 0x1f;
	int rt = (ICODE >> 16) & 0x1f;
	int16_t offset = ICODE & 0xffff;
        uint64_t addr = MIPS64_ReadGpr(base) + offset;
	uint64_t Rt = MIPS64_ReadGpr(rt);
        uint64_t waddr;
        uint64_t mask;
        uint64_t value;
        if(big_endian) {
                waddr = addr & ~7LL;
                shift = (addr & 7LL) * 8;
                mask = 0xffffffffFFFFFFFFULL << (shift);
        } else {
                waddr = (addr + 7LL) & ~7LL;
                shift = 8*((4-(addr & 7LL)) & 7LL);
                mask = 0xffffffffFFFFFFFFULL << (shift);
        }
	value = MIPS64MMU_Read64(waddr);
        value = (value << shift) & mask;
	Rt = (Rt & ~mask) | value;
	MIPS64_WriteGpr(rt,Rt);
}

void
mips64_LDR(void) {
	int base = (ICODE >> 21) & 0x1f;
	int rt = (ICODE >> 16) & 0x1f;
	int16_t offset = ICODE & 0xffff;
	int big_endian = 1;
        int shift;
        uint64_t addr = MIPS64_ReadGpr(base) + offset;
        uint64_t waddr;
        uint64_t mask;
        uint64_t value;
	uint64_t Rt = MIPS64_ReadGpr(rt);
        if(big_endian) {
                waddr = (addr + 7LL) & ~7LL;
                shift = 8*((4-(addr & 7LL)) & 7LL);
                mask = 0xffffffffFFFFFFFFULL >> (shift);
        } else {
                waddr = addr & ~7LL;
                shift = (addr & 7LL) * 8;
                mask = 0xffffffffFFFFFFFFULL >> shift;
        }
	value = MIPS64MMU_Read64(waddr);
        value = (value >> shift) & mask;
	Rt = (Rt & ~mask) | value;
	MIPS64_WriteGpr(rt,Rt);
}

void
mips64_MADD(void) {
	int rt = (ICODE >> 16) & 0x1f;
	int rs = (ICODE >> 21) & 0x1f;
	int64_t product;
	int64_t temp;
	int64_t hi,lo;
	int32_t Rs = MIPS64_ReadGpr(rs);
	int32_t Rt = MIPS64_ReadGpr(rt);
	product = (int64_t)Rs * (int64_t) Rt;
	temp =  (MIPS64_GetHi() << 32) | (uint32_t)MIPS64_GetLo();
	temp += product;
	lo = (int64_t)(int32_t)temp;
	hi = (int64_t)(int32_t)(temp >> 32);
	MIPS64_SetHi(hi);	
	MIPS64_SetLo(lo);
}

void
mips64_MADDU(void) {
	int rt = (ICODE >> 16) & 0x1f;
	int rs = (ICODE >> 21) & 0x1f;
	uint64_t product;
	uint64_t temp;
	uint64_t hi,lo;
	uint32_t Rs = MIPS64_ReadGpr(rs);
	uint32_t Rt = MIPS64_ReadGpr(rt);
	product = (uint64_t)Rs * (uint64_t) Rt;
	temp =  (MIPS64_GetHi() << 32) | (uint32_t)MIPS64_GetLo();
	temp += product;
	lo = (int64_t)(int32_t)temp;
	hi = (int64_t)(int32_t)(temp >> 32);
	MIPS64_SetHi(hi);	
	MIPS64_SetLo(lo);
}

void
mips64_MUL(void) {
	int rt = (ICODE >> 16) & 0x1f;
	int rs = (ICODE >> 21) & 0x1f;
	int rd = (ICODE >> 11) & 0x1f;
	uint64_t Rs = MIPS64_ReadGpr(rs);
	uint64_t Rt = MIPS64_ReadGpr(rt);
	uint64_t Rd = (int64_t)(int32_t)(Rs * Rt);
	MIPS64_WriteGpr(rd,Rd);
}

void
mips64_MSUB(void) {
	int rt = (ICODE >> 16) & 0x1f;
	int rs = (ICODE >> 21) & 0x1f;
	int64_t product;
	int64_t temp;
	int64_t hi,lo;
	int32_t Rs = MIPS64_ReadGpr(rs);
	int32_t Rt = MIPS64_ReadGpr(rt);
	product = (int64_t)Rs * (int64_t) Rt;
	temp =  (MIPS64_GetHi() << 32) | (uint32_t)MIPS64_GetLo();
	temp -= product;
	lo = (int64_t)(int32_t)temp;
	hi = (int64_t)(int32_t)(temp >> 32);
	MIPS64_SetHi(hi);	
	MIPS64_SetLo(lo);
}

void
mips64_MSUBU(void) {
	int rt = (ICODE >> 16) & 0x1f;
	int rs = (ICODE >> 21) & 0x1f;
	uint64_t product;
	uint64_t temp;
	uint64_t hi,lo;
	uint32_t Rs = MIPS64_ReadGpr(rs);
	uint32_t Rt = MIPS64_ReadGpr(rt);
	product = (uint64_t)Rs * (uint64_t) Rt;
	temp =  (MIPS64_GetHi() << 32) | (uint32_t)MIPS64_GetLo();
	temp -= product;
	lo = (int64_t)(int32_t)temp;
	hi = (int64_t)(int32_t)(temp >> 32);
	MIPS64_SetHi(hi);	
	MIPS64_SetLo(lo);
}

void
mips64_CLZ(void) {
	int rs = (ICODE >> 21) & 0x1f;
	//int rt = (ICODE >> 16) & 0x1f;
	int rd = (ICODE >> 11) & 0x1f;
	uint32_t Rs;
	Rs = MIPS64_ReadGpr(rs);
	int count = 0;
	uint64_t Rd; 
	if((Rs & 0xffff0000) == 0) {
		count = 16;
		Rs <<= 16;
	}
	if((Rs & 0xff000000) == 0) {
		count += 8; 	
		Rs <<= 8;
	} 
	if((Rs & 0xf0000000) == 0) {
		count += 4; 	
		Rs <<= 4;
	}
	if((Rs & 0xc0000000) == 0) {
		count += 2; 	
		Rs <<= 2;
	}
	if((Rs & 0x80000000) == 0) {
		count += 1; 	
		Rs <<= 1;
	}
	Rd = count;
	MIPS64_WriteGpr(rd,count);
	fprintf(stderr,"instruction CLZ not implemented\n");
}

void
mips64_CLO(void) {
	int rs = (ICODE >> 21) & 0x1f;
	//int rt = (ICODE >> 16) & 0x1f;
	int rd = (ICODE >> 11) & 0x1f;
	uint32_t Rs;
	Rs = MIPS64_ReadGpr(rs);
	int count = 0;
	uint64_t Rd; 
	if((Rs & 0xffff0000) == 0xffff0000) {
		count = 16;
		Rs <<= 16;
	}
	if((Rs & 0xff000000) == 0xff000000) {
		count += 8; 	
		Rs <<= 8;
	} 
	if((Rs & 0xf0000000) == 0xf0000000) {
		count += 4; 	
		Rs <<= 4;
	}
	if((Rs & 0xc0000000) == 0xc0000000) {
		count += 2; 	
		Rs <<= 2;
	}
	if((Rs & 0x80000000) == 0x80000000) {
		count += 1; 	
		Rs <<= 1;
	}
	Rd = count;
	MIPS64_WriteGpr(rd,count);
}

void
mips64_DCLZ(void) {
	int rs = (ICODE >> 21) & 0x1f;
	//int rt = (ICODE >> 16) & 0x1f;
	int rd = (ICODE >> 11) & 0x1f;
	uint64_t Rs;
	Rs = MIPS64_ReadGpr(rs);
	int count = 0;
	uint64_t Rd; 
	if((Rs & 0xffffffff00000000LL) == 0) {
		count = 16;
		Rs <<= 16;
	}
	if((Rs & 0xffff000000000000LL) == 0) {
		count = 16;
		Rs <<= 16;
	}
	if((Rs & 0xff00000000000000LL) == 0) {
		count += 8; 	
		Rs <<= 8;
	} 
	if((Rs & 0xf000000000000000LL) == 0) {
		count += 4; 	
		Rs <<= 4;
	}
	if((Rs & 0xc000000000000000LL) == 0) {
		count += 2; 	
		Rs <<= 2;
	}
	if((Rs & 0x8000000000000000LL) == 0) {
		count += 1; 	
		Rs <<= 1;
	}
	Rd = count;
	MIPS64_WriteGpr(rd,count);
}

void
mips64_DCLO(void) {
	int rs = (ICODE >> 21) & 0x1f;
	//int rt = (ICODE >> 16) & 0x1f;
	int rd = (ICODE >> 11) & 0x1f;
	uint64_t Rs;
	Rs = MIPS64_ReadGpr(rs);
	int count = 0;
	uint64_t Rd; 
	if((Rs & 0xffffffff00000000LL) == 0xffffffff00000000LL) {
		count = 16;
		Rs <<= 16;
	}
	if((Rs & 0xffff000000000000LL) == 0xffff000000000000LL) {
		count = 16;
		Rs <<= 16;
	}
	if((Rs & 0xff00000000000000LL) == 0xff00000000000000LL) {
		count += 8; 	
		Rs <<= 8;
	} 
	if((Rs & 0xf000000000000000LL) == 0xf000000000000000LL) {
		count += 4; 	
		Rs <<= 4;
	}
	if((Rs & 0xc000000000000000LL) == 0xc000000000000000LL) {
		count += 2; 	
		Rs <<= 2;
	}
	if((Rs & 0x8000000000000000LL) == 0x8000000000000000LL) {
		count += 1; 	
		Rs <<= 1;
	}
	Rd = count;
	MIPS64_WriteGpr(rd,count);
}

void
mips64_SDBBP(void) {
	fprintf(stderr,"instruction SDBBP not implemented\n");
}

/*
 * Only MIPS16e
 */
void
mips64_JALX(void) {
	fprintf(stderr,"instruction JALX not implemented\n");
}

void
mips64_MDMX(void) {
	fprintf(stderr,"instruction MDMX not implemented\n");
}

/*
 * Extract Bitfield
 */
void
mips64_EXT(void) 
{
	int rs = (ICODE >> 21) & 0x1f;
	int rt = (ICODE >> 16) & 0x1f;
	int msbd = (ICODE >> 11) & 0x1f;
	int lsb = (ICODE >> 6) & 0x1f;
	int size = msbd+1;
	uint32_t Rs = MIPS64_ReadGpr(rs);	
	uint32_t Rt = MIPS64_ReadGpr(rt);
	Rt = (Rs >> lsb) & ((1<<size)-1);	
	MIPS64_WriteGpr(rt,(int64_t)(int32_t)Rt);
}

void
mips64_DEXTM(void) {
	uint64_t mask;	
	uint64_t result;
	uint64_t Rs;
	int rs = (ICODE >> 21) & 0x1f;
	int rt = (ICODE >> 16) & 0x1f;
	int msbdminus32 = (ICODE >> 11) & 0x1f;
	int lsb = (ICODE >> 6) & 0x1f; 
	int size = msbdminus32 + 1 + 32;	
	Rs = MIPS64_ReadGpr(rs);
	mask = ~((uint64_t)0) >> (64 - size);
	mask = mask << lsb;	
	result = (Rs & mask) >> lsb;
	MIPS64_WriteGpr(rt,result);
}

void
mips64_DEXTU(void) {
	uint64_t mask;	
	uint64_t result;
	uint64_t Rs;
	int rs = (ICODE >> 21) & 0x1f;
	int rt = (ICODE >> 16) & 0x1f;
	int msbd = (ICODE >> 11) & 0x1f;
	int lsbminus32 = (ICODE >> 6) & 0x1f; 
	int lsb = lsbminus32 + 32;
	int size = msbd + 1;	
	Rs = MIPS64_ReadGpr(rs);
	mask = ~((uint64_t)0) >> (64 - size);
	mask = mask << lsb;	
	result = (Rs & mask) >> lsb;
	MIPS64_WriteGpr(rt,result);
}

void
mips64_DEXT(void) {
	uint64_t mask;	
	uint64_t result;
	uint64_t Rs;
	int rs = (ICODE >> 21) & 0x1f;
	int rt = (ICODE >> 16) & 0x1f;
	int msbd = (ICODE >> 11) & 0x1f;
	int lsb = (ICODE >> 6) & 0x1f; 
	int size = msbd + 1;	
	Rs = MIPS64_ReadGpr(rs);
	mask = ~((uint64_t)0) >> (64 - size);
	mask = mask << lsb;	
	result = (Rs & mask) >> lsb;
	MIPS64_WriteGpr(rt,result);
}

/*
 * Insert Bitfield
 */
void
mips64_INS(void) 
{
	int rs = (ICODE >> 21) & 0x1f;
	int rt = (ICODE >> 16) & 0x1f;
	int msb = (ICODE >> 11) & 0x1f;
	int pos = (ICODE >> 6) & 0x1f;
	int size = msb + 1 - pos;
	uint32_t Rs = MIPS64_ReadGpr(rs);
	uint32_t Rt = MIPS64_ReadGpr(rt);
	uint32_t mask = (1<<size) - 1;
	uint32_t value = Rs & mask;
	mask = mask << pos;	
	Rt = (Rt & ~mask) | (value << pos);
	MIPS64_WriteGpr(rt,(int64_t)(int32_t)Rt);
}

void
mips64_DINSM(void) {
	int rs = (ICODE >> 21) & 0x1f;
	int rt = (ICODE >> 16) & 0x1f;
	int msbminus32 = (ICODE >> 11) & 0x1f;		
	int lsb = (ICODE >> 6) & 0x1f;		
	int pos;
	int size;
	uint64_t Rs,Rt,mask;
	Rs = MIPS64_ReadGpr(rs);
	Rt = MIPS64_ReadGpr(rt);
	pos = lsb;	
	size = msbminus32 + 33 - pos;
	if(((pos + size) <= 32) || ((pos+size) > 64))  {
		fprintf(stderr,"Bad dins instruction\n");
		return;
	}	
	if(size == 64) {
		mask = ~(uint64_t)0;
	} else {
		mask = ((1<<size) - 1) << pos;
	}
	Rt = (Rt & ~mask) | ((Rs << pos) & mask);
	MIPS64_WriteGpr(rt,Rt);
	fprintf(stderr,"instruction DINSM not implemented\n");
}

void
mips64_DINSU(void) {
	int rs = (ICODE >> 21) & 0x1f;
	int rt = (ICODE >> 16) & 0x1f;
	int msbminus32 = (ICODE >> 11) & 0x1f;		
	int lsbminus32 = (ICODE >> 6) & 0x1f;		
	int pos;
	int size;
	uint64_t Rs,Rt,mask;
	Rs = MIPS64_ReadGpr(rs);
	Rt = MIPS64_ReadGpr(rt);
	pos = lsbminus32 + 32;	
	size = msbminus32 + 33 - pos;
	if(((pos + size) <= 32) || ((pos+size) > 64))  {
		fprintf(stderr,"Bad dins instruction\n");
		return;
	}	
	mask = ((1<<size) - 1) << pos;
	Rt = (Rt & ~mask) | ((Rs << pos) & mask);
	MIPS64_WriteGpr(rt,Rt);
	fprintf(stderr,"instruction DINSU not implemented\n");
}

void
mips64_DINS(void) {
	int rs = (ICODE >> 21) & 0x1f;
	int rt = (ICODE >> 16) & 0x1f;
	int msb = (ICODE >> 11) & 0x1f;		
	int lsb = (ICODE >> 6) & 0x1f;		
	int pos;
	int size;
	uint64_t Rs,Rt,mask;
	Rs = MIPS64_ReadGpr(rs);
	Rt = MIPS64_ReadGpr(rt);
	pos = lsb;	
	size = msb + 1 - pos;
	if(((pos + size) <= 0) || ((pos+size) > 32))  {
		fprintf(stderr,"Bad dins instruction\n");
		return;
	}	
	mask = ((1<<size) - 1) << pos;
	Rt = (Rt & ~mask) | ((Rs << pos) & mask);
	MIPS64_WriteGpr(rt,Rt);
}

/*
 * -------------------------------------------------------
 * SEB, SEH, WSBH, DSBH, DSHD 
 * -------------------------------------------------------
 */
void
mips64_BSHFL(void) {
	int rt = (ICODE >> 16) & 0x1f;
	int rd = (ICODE >> 11) & 0x1f;
	int sub = (ICODE >> 6) & 0x1f;
	uint32_t Rt;	
	int32_t Rd;	
	Rt = MIPS64_ReadGpr(rt);
	if(sub == 2) {
		/* WBSH	*/
		Rt = MIPS64_ReadGpr(rt);
		Rd = ((Rt & 0xff00ff) << 8) | ((Rt & 0xff00ff00) >> 8);	
		MIPS64_WriteGpr(rd,(int64_t)(int32_t)Rd);
	} else if(sub == 5) {
		// unknown instruction
		// dshd	
	} else if(sub == 0x10) {
		/* SEB */
		Rt = MIPS64_ReadGpr(rt);	
		MIPS64_WriteGpr(rd,(int64_t)(int8_t)Rt);	
	} else if(sub == 0x18) {
		/* SEH	*/
		Rt = MIPS64_ReadGpr(rt);	
		MIPS64_WriteGpr(rd,(int64_t)(int16_t)Rt);	
	}
	fprintf(stderr,"instruction BSHFL not implemented\n");
}

/*
 *
 */
void
mips64_DBSHFL(void) {
	int rt = (ICODE >> 16) & 0x1f;
	int rd = (ICODE >> 11) & 0x1f;
	int sub = (ICODE >> 6) & 0x1f;
	uint64_t Rt;	
	uint64_t Rd;	
	Rt = MIPS64_ReadGpr(rt);
	if(sub == 2) {
		/* DBSH	*/
		Rt = MIPS64_ReadGpr(rt);
		Rd = ((Rt & 0xff00ff00ff00ffLL) << 8) | ((Rt & 0xff00ff00ff00ff00LL) >> 8);	
		MIPS64_WriteGpr(rd,Rd);
	} else if(sub == 5) {
		/* dshd	 */
		Rt = MIPS64_ReadGpr(rt);
		Rd = ((Rt & 0xffffLL) << 48) | ((Rt & 0xffff0000LL) << 16) 
		  | ((Rt & 0xffff00000000LL) >> 16) 
		  | ((Rt & 0xffff000000000000LL) >> 48); 
		MIPS64_WriteGpr(rd,Rd);
	} else {
		// exception illegal
		fprintf(stderr,"illegal\n");
	}
}

/*
 ****************************************************************
 * Read hardware register
 ****************************************************************
 */
void
mips64_RDHWR(void) 
{
	fprintf(stderr,"Instruction RDHWR not implemented\n");
}

void
mips64_LB(void) {
	int rt = (ICODE >> 16) & 0x1f;
	int base = (ICODE >> 21) & 0x1f;
	int16_t offset = ICODE & 0xffff;
	uint64_t Rt;
	Rt = (int64_t)(int8_t)MIPS64MMU_Read8(MIPS64_ReadGpr(base)+offset);	
	MIPS64_WriteGpr(rt,Rt);
}

void
mips64_LH(void) {
	int rt = (ICODE >> 16) & 0x1f;
	int base = (ICODE >> 21) & 0x1f;
	int16_t offset = ICODE & 0xffff;
	uint64_t Rt;
	uint64_t vaddr = MIPS64_ReadGpr(base)+offset;
	if(vaddr & 1) {
		// exception
	}	
	Rt = (int64_t)(int16_t)MIPS64MMU_Read16(vaddr);	
	MIPS64_WriteGpr(rt,Rt);
}

void
mips64_LWL(void) {
	int big_endian = 1;
        int shift;
	int base = (ICODE >> 21) & 0x1f;
	int rt = (ICODE >> 16) & 0x1f;
	int16_t offset = ICODE & 0xffff;
        uint64_t addr = MIPS64_ReadGpr(base) + offset;
	uint32_t Rt = MIPS64_ReadGpr(rt);
        uint64_t waddr;
        uint32_t mask;
        uint32_t value;
        if(big_endian) {
                waddr = addr & ~3LL;
                shift = (addr & 3LL) * 8;
                mask = 0xffffffffU << (shift);
        } else {
                waddr = (addr + 3LL) & ~3LL;
                shift = 8*((4-(addr & 3LL)) & 3LL);
                mask = 0xffffffffU << (shift);
        }
	value = MIPS64MMU_Read32(waddr);
        //fprintf(stderr,"value %08x: 0x%08x\n",value,(value << shift) & mask);
        value = (value << shift) & mask;
	Rt = (Rt & ~mask) | value;
	MIPS64_WriteGpr(rt,(int64_t)(int32_t)Rt);
}

void
mips64_LW(void) {
	int rt = (ICODE >> 16) & 0x1f;
	int base = (ICODE >> 21) & 0x1f;
	int16_t offset = ICODE & 0xffff;
	uint64_t addr = MIPS64_ReadGpr(base) + offset;
	uint32_t Rt = MIPS64MMU_Read32(addr);
	MIPS64_WriteGpr(rt,Rt);
}

void
mips64_LBU(void) {
	int rt = (ICODE >> 16) & 0x1f;
	int base = (ICODE >> 21) & 0x1f;
	int16_t offset = ICODE & 0xffff;
	uint64_t Rt;
	Rt = MIPS64MMU_Read8(MIPS64_ReadGpr(base)+offset);	
	MIPS64_WriteGpr(rt,Rt);
}

void
mips64_LHU(void) {
	int rt = (ICODE >> 16) & 0x1f;
	int base = (ICODE >> 21) & 0x1f;
	int16_t offset = ICODE & 0xffff;
	uint64_t Rt;
	uint64_t vaddr = MIPS64_ReadGpr(base)+offset;
	if(vaddr & 1) {
		// exception
	}	
	Rt = MIPS64MMU_Read16(vaddr);	
	MIPS64_WriteGpr(rt,Rt);
}

void
mips64_LWR(void) {
	int base = (ICODE >> 21) & 0x1f;
	int rt = (ICODE >> 16) & 0x1f;
	int16_t offset = ICODE & 0xffff;
	int big_endian = 1;
        int shift;
        uint64_t addr = MIPS64_ReadGpr(base) + offset;
        uint64_t waddr;
        uint32_t mask;
        uint32_t value;
	uint32_t Rt = MIPS64_ReadGpr(rt);
        if(big_endian) {
                waddr = (addr + 3LL) & ~3LL;
                shift = 8*((4-(addr & 3LL)) & 3LL);
                mask = 0xffffffffU >> (shift);
        } else {
                waddr = addr & ~3LL;
                shift = (addr & 3LL) * 8;
                mask = 0xffffffffU >> shift;
        }
	value = MIPS64MMU_Read32(waddr);
        //fprintf(stderr,"value %08x: 0x%08x\n",value,(value >> shift) & mask);
        value = (value >> shift) & mask;
	Rt = (Rt & ~mask) | value;
	MIPS64_WriteGpr(rt,(int64_t)(int32_t)Rt);
}

void
mips64_LWU(void) {
#if 0
	int base = (ICODE  >> 21) & 0x1f;
	int16_t offset = (ICODE & 0xffff);
	int rt = (ICODE >> 16) & 0x1f;
	uint64_t vaddr = MIPS64_ReadGpr(base) + offset;
	if(vaddr & 3) {
		// exception
	}
	MIPS64MMU_Read32(vaddr);
#endif
	fprintf(stderr,"instruction LWU not implemented\n");
}

/*
 * Store Byte
 */
void
mips64_SB(void) {
	int base = (ICODE >> 21) & 0x1f;
	int rt = (ICODE >> 16) & 0x1f;
	int16_t offset = ICODE & 0xffff;
	uint64_t Base;
	uint64_t Rt = MIPS64_ReadGpr(rt);
	Base = MIPS64_ReadGpr(base);
	MIPS64MMU_Write8(Rt,Base+offset);
}

void
mips64_SH(void) {
	int base = (ICODE >> 21) & 0x1f;
	int rt = (ICODE >> 16) & 0x1f;
	int16_t offset = ICODE & 0xffff;
	uint64_t Base;
	uint64_t eaddr;
	uint64_t Rt = MIPS64_ReadGpr(rt);
	Base = MIPS64_ReadGpr(base);
	eaddr = Base+offset;
	if(eaddr & 1) {
		// Address error exception
	}
	MIPS64MMU_Write16(Rt,eaddr);
}

void
mips64_SWL(void) {
	int big_endian = 1;
        int shift;
	int base = (ICODE >> 21) & 0x1f;
	int rt = (ICODE >> 16) & 0x1f;
	int16_t offset = ICODE & 0xffff;
        uint64_t addr = MIPS64_ReadGpr(base) + offset;
	uint32_t Rt = MIPS64_ReadGpr(rt);
        uint64_t waddr;
        uint32_t mask;
        uint32_t value;
        waddr = addr & ~3LL;
        if(big_endian) {
                shift = (addr & 3LL) * 8;
		Rt = Rt >> shift;
                mask = 0xffffffffU >> (shift);
        } else {
                shift = 8*((3-(addr & 3LL)) & 3LL);
		Rt = Rt >> shift;
                mask = 0xffffffffU >> (shift);
        }
	value = MIPS64MMU_Read32(waddr);
	value = (value & ~mask) | (Rt & mask);
	MIPS64MMU_Write32(value,waddr);
}

void
mips64_SW(void) {
	int base = (ICODE >> 21) & 0x1f;
	int rt = (ICODE >> 16) & 0x1f;
	int16_t offset = ICODE & 0xffff;
	uint64_t Base;
	uint64_t eaddr;
	uint64_t Rt = MIPS64_ReadGpr(rt);
	Base = MIPS64_ReadGpr(base);
	eaddr = Base+offset;
	if(eaddr & 1) {
		// Address error exception
	}
	MIPS64MMU_Write32(Rt,eaddr);
}

void
mips64_SDL(void) {
	int big_endian = 1;
        int shift;
	int base = (ICODE >> 21) & 0x1f;
	int rt = (ICODE >> 16) & 0x1f;
	int16_t offset = ICODE & 0xffff;
        uint64_t addr = MIPS64_ReadGpr(base) + offset;
	uint64_t Rt = MIPS64_ReadGpr(rt);
        uint64_t waddr;
        uint64_t mask;
        uint64_t value;
        waddr = addr & ~7LL;
        if(big_endian) {
                shift = (addr & 7LL) * 8;
		Rt = Rt >> shift;
                mask = 0xFFFFFFFFffffffffULL >> (shift);
        } else {
                shift = 8*((7-(addr & 7LL)) & 7LL);
		Rt = Rt >> shift;
                mask = 0xFFFFFFFFffffffffULL >> (shift);
        }
	value = MIPS64MMU_Read64(waddr);
	value = (value & ~mask) | (Rt & mask);
	MIPS64MMU_Write64(value,waddr);
}

void
mips64_SDR(void) {
	int big_endian = 1;
        int shift;
	int base = (ICODE >> 21) & 0x1f;
	int rt = (ICODE >> 16) & 0x1f;
	int16_t offset = ICODE & 0xffff;
        uint64_t addr = MIPS64_ReadGpr(base) + offset;
	uint64_t Rt = MIPS64_ReadGpr(rt);
        uint64_t waddr;
        uint64_t mask;
        uint64_t value;
        waddr = addr & ~7LL;
        if(big_endian) {
                shift = 8*((7-(addr & 7LL)) & 7LL);
		Rt = Rt << shift;
                mask = 0xFFFFFFFFffffffffULL << (shift);
        } else {
                shift = (addr & 7LL) * 8;
		Rt = Rt << shift;
                mask = 0xFFFFFFFFffffffffULL << (shift);
        }
	value = MIPS64MMU_Read32(waddr);
	value = (value & ~mask) | (Rt & mask);
	MIPS64MMU_Write32(value,waddr);
}

void
mips64_SWR(void) {
	int big_endian = 1;
        int shift;
	int base = (ICODE >> 21) & 0x1f;
	int rt = (ICODE >> 16) & 0x1f;
	int16_t offset = ICODE & 0xffff;
        uint64_t addr = MIPS64_ReadGpr(base) + offset;
	uint32_t Rt = MIPS64_ReadGpr(rt);
        uint64_t waddr;
        uint32_t mask;
        uint32_t value;
        waddr = addr & ~3LL;
        if(big_endian) {
                shift = 8*((3-(addr & 3LL)) & 3LL);
		Rt = Rt << shift;
                mask = 0xffffffffU << (shift);
        } else {
                shift = (addr & 3LL) * 8;
		Rt = Rt << shift;
                mask = 0xffffffffU << (shift);
        }
	value = MIPS64MMU_Read32(waddr);
	value = (value & ~mask) | (Rt & mask);
	MIPS64MMU_Write32(value,waddr);
}

void
mips64_CACHE(void) {
	fprintf(stderr,"instruction CACHE not implemented\n");
}

void
mips64_LL(void) {
	int rt = (ICODE >> 16) & 0x1f;
	int base = (ICODE >> 21) & 0x1f;
	int16_t offset = ICODE & 0xffff;
	uint64_t addr = MIPS64_ReadGpr(base) + offset;
	uint32_t Rt = MIPS64MMU_Read32(addr);
	/*
 	 * Begin a RMW sequence here by setting a flags
 	 */
	MIPS64_SetLLBit(1);

	MIPS64_WriteGpr(rt,Rt);
	fprintf(stderr,"instruction LL not implemented\n");
}

/*
 * Load Word to Floating Point
 */
void
mips64_LWC1(void) {
	fprintf(stderr,"instruction LWC1 not implemented\n");
}

/*
 *******************************************************+
 * Load Word to Coprocessor 2
 *******************************************************+
 */
void
mips64_LWC2(void) {
	fprintf(stderr,"instruction LWC2 not implemented\n");
}

/*
 * Ignore the prefetch instruction
 */
void
mips64_PREF(void) 
{

}

/*
 ***********************************************************
 * Load Linked doubleword
 * Start an atomic RMW sequence
 ***********************************************************
 */
void
mips64_LLD(void) 
{
	int rt = (ICODE >> 16) & 0x1f;
	int base = (ICODE >> 21) & 0x1f;
	int16_t offset = ICODE & 0xffff;
	uint64_t addr = MIPS64_ReadGpr(base) + offset;
	uint64_t Rt = MIPS64MMU_Read64(addr);
	/*
 	 * Begin a RMW sequence here by setting a flags
	 * missing here
 	 */
	MIPS64_WriteGpr(rt,Rt);
}

void
mips64_LDC1(void) {
	fprintf(stderr,"instruction LDC1 not implemented\n");
}

void
mips64_LDC2(void) {
	fprintf(stderr,"instruction LDC2 not implemented\n");
}

void
mips64_LD(void) {
	int rt = (ICODE >> 16) & 0x1f;
	int base = (ICODE >> 21) & 0x1f;
	int16_t offset = ICODE & 0xffff;
	uint64_t addr = MIPS64_ReadGpr(base) + offset;
	uint64_t Rt = MIPS64MMU_Read64(addr);
	MIPS64_WriteGpr(rt,Rt);
}

/*
 ***********************************************************
 * Store Conditional Word. Completes an atomic RMW
 * Exceptions clear the atomic flag
 ***********************************************************
 */
void
mips64_SC(void) {
	int base = (ICODE >> 21) & 0x1f;
	int rt = (ICODE >> 16) & 0x1f;
	int16_t offset = ICODE & 0xffff;
	uint64_t Base;
	uint64_t eaddr;
	uint64_t Rt = MIPS64_ReadGpr(rt);
	Base = MIPS64_ReadGpr(base);
	eaddr = Base+offset;
	if(eaddr & 1) {
		// Address error exception
	}
	if(MIPS64_GetLLBit()) {
		MIPS64MMU_Write32(Rt,eaddr);
	}
	MIPS64_WriteGpr(rt,MIPS64_GetLLBit());	
}

/*
 * Store Word from Floating point
 */
void
mips64_SWC1(void) {
	fprintf(stderr,"instruction SWC1 not implemented\n");
}

/*
 * Store Word from Coprocessor 2
 */
void
mips64_SWC2(void) {
	fprintf(stderr,"instruction SWC2 not implemented\n");
}

/*
 ****************************************************************
 * Store Conditional doubleword
 ****************************************************************
 */
void
mips64_SCD(void) {
	int base = (ICODE >> 21) & 0x1f;
	int rt = (ICODE >> 16) & 0x1f;
	int16_t offset = ICODE & 0xffff;
	uint64_t Base;
	uint64_t eaddr;
	uint64_t Rt = MIPS64_ReadGpr(rt);
	Base = MIPS64_ReadGpr(base);
	eaddr = Base+offset;
	if(eaddr & 1) {
		// Address error exception
	}
	if(MIPS64_GetLLBit()) {
		MIPS64MMU_Write64(Rt,eaddr);
	}
	MIPS64_WriteGpr(rt,MIPS64_GetLLBit());	
}

/*
 * Store doubleword from Floating point
 */
void
mips64_SDC1(void) {
	fprintf(stderr,"instruction SCD1 not implemented\n");
}

/*
 **********************************************************************
 * Store doubleword from Coprocessor 2
 **********************************************************************
 */
void
mips64_SDC2(void) {
	fprintf(stderr,"instruction SCD2 not implemented\n");
}

void
mips64_SD(void) {
	int base = (ICODE >> 21) & 0x1f;
	int rt = (ICODE >> 16) & 0x1f;
	int16_t offset = ICODE & 0xffff;
	uint64_t Base;
	uint64_t eaddr;
	uint64_t Rt = MIPS64_ReadGpr(rt);
	Base = MIPS64_ReadGpr(base);
	eaddr = Base+offset;
	if(eaddr & 7) {
		// Address error exception
	}
	MIPS64MMU_Write64(Rt,eaddr);
}

void
mips64_UNDEFINED(void) {
	fprintf(stderr,"instruction UNDEFINED not implemented\n");
}

