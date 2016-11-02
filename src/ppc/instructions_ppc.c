/*
 * ----------------------------------------------------
 *
 * PowerPC Instruction Set 
 * (C) 2005 Jochen Karrer 
 *   Author: Jochen Karrer
 *
 * With some code from PearPC
 *
 * ----------------------------------------------------
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <instructions_ppc.h>
#include <byteorder.h>
#include <cpu_ppc.h>
#include <mmu_ppc.h>
#include <cycletimer.h>

#ifdef DEBUG
#define dbgprintf(x...) { if(unlikely(debugflags&DEBUG_INSTRUCTIONS)) { fprintf(stderr,x); fflush(stderr); } }
#else
#define dbgprintf(x...)
#endif

static inline int
count_leading_zeros(uint32_t value)
{
	int i;
	for (i = 0; i < 32; i++) {
		if (value & 0x80000000) {
			return i;
		}
		value = value << 1;
	}
	return 32;
}

static inline void
update_cr0(uint32_t result)
{
	CR = CR & ~(CR_LT | CR_GT | CR_EQ | CR_SO);
	if (!result) {
		CR = CR | CR_EQ;
	} else if (ISNEG(result)) {
		CR = CR | CR_LT;
	} else {
		CR = CR | CR_GT;
	}
	if (XER & XER_SO) {
		CR |= CR_SO;
	}
}

/*
 * ---------------------------------------
 * Carry and overflow Flag Helper 
 * functions for add and sub
 * ---------------------------------------
 */
static inline uint32_t
sub_carry(uint32_t op1, uint32_t op2, uint32_t result)
{
	if (((ISNEG(op1) && ISNOTNEG(op2))
	     || (ISNEG(op1) && ISNOTNEG(result))
	     || (ISNOTNEG(op2) && ISNOTNEG(result)))) {
		return 1;
	} else {
		return 0;
	}
}

static inline uint32_t
add_carry(uint32_t op1, uint32_t op2, uint32_t result)
{
	if (((ISNEG(op1) && ISNEG(op2))
	     || (ISNEG(op1) && ISNOTNEG(result))
	     || (ISNEG(op2) && ISNOTNEG(result)))) {
		return 1;
	} else {
		return 0;
	}
}

static inline uint32_t
sub_overflow(uint32_t op1, uint32_t op2, uint32_t result)
{
	if ((ISNEG(op1) && ISNOTNEG(op2) && ISNOTNEG(result))
	    || (ISNOTNEG(op1) && ISNEG(op2) && ISNEG(result))) {
		return 1;
	} else {
		return 0;
	}
}

static inline uint32_t
add_overflow(uint32_t op1, uint32_t op2, uint32_t result)
{
	if ((ISNEG(op1) && ISNEG(op2) && ISNOTNEG(result))
	    || (ISNOTNEG(op1) && ISNOTNEG(op2) && ISNEG(result))) {
		return 1;
	} else {
		return 0;
	}
}

void
ppc_tdi(uint32_t icode)
{
	fprintf(stderr, "instr ppc_tdi(%08x) not implemented\n", icode);
}

/*
 * ------------------------------------------------------------
 * twi UISA Form D 
 *	Trap word Immediate
 * ------------------------------------------------------------
 */
void
ppc_twi(uint32_t icode)
{
	int to = (icode >> 21) & 0x1f;
	int a = (icode >> 16) & 0x1f;
	int16_t imm = icode & 0xffff;
	int32_t eimm = imm;
	int32_t A = GPR(a);
	if (((int32_t) A < (int32_t) eimm) && (to & (1 << 0))) {
		// Exception
	}
	if (((int32_t) A > (int32_t) eimm) && (to & (1 << 1))) {
		// Exception
	}
	if ((A == eimm) && (to & (1 << 2))) {
		// Exception
	}
	if (((uint32_t) A < (uint32_t) eimm) && (to & (1 << 3))) {
		// Exception
	}
	if (((uint32_t) A > (uint32_t) eimm) && (to & (1 << 4))) {
		// Exception
	}
	fprintf(stderr, "instr ppc_twi(%08x) not implemented\n", icode);
}

/* 
 * -------------------------------------
 * mulli UISA Form D 
 *	Multiply Low Immediate
 * -------------------------------------
 */
void
ppc_mulli(uint32_t icode)
{
	int d = (icode >> 21) & 0x1f;
	int a = (icode >> 16) & 0x1f;
	int rc = icode & 1;
	uint32_t result;
	int16_t simm = (icode & 0xffff);
	result = GPR(d) = (int64_t) GPR(a) * (int64_t) simm;
	if (rc) {
		update_cr0(result);
	}
	dbgprintf("instr ppc_mulli(%08x)\n", icode);
}

/*
 * ------------------------------------------------
 * subfic UISA Form D
 * ------------------------------------------------
 */
void
ppc_subfic(uint32_t icode)
{
	int d = (icode >> 21) & 0x1f;
	int a = (icode >> 16) & 0x1f;
	int16_t imm = icode & 0xffff;
	uint32_t result;
	uint32_t op2 = GPR(a);
	result = GPR(d) = imm - op2;
	if (sub_carry(imm, op2, result)) {
		XER = XER | XER_CA;
	} else {
		XER = XER & ~XER_CA;
	}
	dbgprintf("instr ppc_subfic(%08x)\n", icode);
}

/*
 * ------------------
 * cmpli UISA Form D
 * v1
 * ------------------
 */
void
ppc_cmpli(uint32_t icode)
{
	uint32_t crfd = 7 - ((icode >> 23) & 7);
	uint32_t a = (icode >> 16) & 0x1f;
	uint16_t uimm = icode & 0xffff;
	uint32_t Ra = GPR(a);
	int L = (icode >> 21) & 1;
	uint32_t c;
	if (L) {
		fprintf(stderr, "Invalid instruction format for cmpli\n");
		return;
	}
	if (Ra < uimm) {
		c = 8;
	} else if (Ra > uimm) {
		c = 4;
	} else {
		c = 2;
	}
	if (XER & XER_SO) {
		c |= 1;
	}
	CR &= 0xffffffff ^ (0xf << (crfd << 2));
	CR |= c << (crfd << 2);
	dbgprintf("instr ppc_cmpli(%08x)\n", icode);
}

/*
 * ----------------------
 * cmpi UISA Form D 
 * v1
 * ----------------------
 */

void
ppc_cmpi(uint32_t icode)
{
	uint32_t crfd = 7 - ((icode >> 23) & 7);
	uint32_t a = (icode >> 16) & 0x1f;
	int16_t simm = icode & 0xffff;
	int32_t Ra = GPR(a);
	int L = (icode >> 21) & 1;
	uint32_t c;
	if (L) {
		fprintf(stderr, "invalid instruction format\n");
	}
	if (Ra < simm) {
		c = 8;
	} else if (Ra > simm) {
		c = 4;
	} else {
		c = 2;
	}
	if (XER & XER_SO) {
		c |= 1;
	}
	CR &= 0xffffffff ^ (0xf << (crfd << 2));
	CR |= c << (crfd << 2);
	dbgprintf("instr ppc_cmpi(%08x)\n", icode);
}

/*
 * -----------------------------
 * addic UISA form D
 * v1
 * -----------------------------
 */
void
ppc_addic(uint32_t icode)
{
	int d = (icode >> 21) & 0x1f;
	int a = (icode >> 16) & 0x1f;
	int16_t simm = (icode & 0xffff);
	uint32_t op1;
	uint32_t result;
	op1 = GPR(a);
	GPR(d) = result = op1 + simm;
	if (add_carry(op1, simm, result)) {
		XER = XER | XER_CA;
	} else {
		XER = XER & ~XER_CA;
	}
	dbgprintf("instr ppc_addic(%08x)\n", icode);
}

/* 
 * --------------------------------
 * addic. UISA Form D 
 * v1
 * --------------------------------
 */
void
ppc_addic_(uint32_t icode)
{
	int d = (icode >> 21) & 0x1f;
	int a = (icode >> 16) & 0x1f;
	int16_t simm = (icode & 0xffff);
	uint32_t op1;
	uint32_t result;
	op1 = GPR(a);
	GPR(d) = result = op1 + simm;
	if (add_carry(op1, simm, result)) {
		XER = XER | XER_CA;
	} else {
		XER = XER & ~XER_CA;
	}
	update_cr0(result);
	dbgprintf("instr ppc_addic_(%08x)\n", icode);
}

/*
 * -------------------------
 * addi UISA form D
 * v1
 * -------------------------
 */
void
ppc_addi(uint32_t icode)
{
	int d = (icode >> 21) & 0x1f;
	int a = (icode >> 16) & 0x1f;
	int16_t simm = (icode & 0xffff);
	if (a == 0) {
		GPR(d) = simm;
	} else {
		GPR(d) = GPR(a) + simm;
	}
	dbgprintf("instr ppc_addi(%08x)\n", icode);
}

/*
 * --------------------------------
 * addis UISA Form D
 * v1
 * --------------------------------
 */
void
ppc_addis(uint32_t icode)
{
	int d = (icode >> 21) & 0x1f;
	int a = (icode >> 16) & 0x1f;
	int32_t simm = (icode & 0xffff) << 16;
	if (a == 0) {
		GPR(d) = simm;
	} else {
		GPR(d) = GPR(a) + simm;
	}
	dbgprintf("instr ppc_addis(%08x)\n", icode);
}

/*
 * -------------------------------------
 * bcx UISA Form D
 * v1
 * -------------------------------------
 */
void
ppc_bcx(uint32_t icode)
{
	uint32_t bo = (icode >> 21) & 0x1f;
	uint32_t bi = (icode >> 16) & 0x1f;
	int16_t bd = (icode) & 0xfffc;
	int aa = (icode & 2);
	int lk = (icode & 1);
	int ctr_ok;
	uint32_t cond_ok;
	if (!(bo & (1 << (4 - 2)))) {
		CTR = CTR - 1;
	}
	ctr_ok = ((bo >> 2) & 1) | ((CTR != 0) != ((bo >> (4 - 3)) & 1));
	cond_ok = (bo & (1 << (4 - 0))) | (((CR >> (31 - bi)) & 1) == ((bo >> (4 - 1)) & 1));
	if (ctr_ok && cond_ok) {
		if (lk) {
			LR = CIA + 4;
		}
		if (aa) {
			NIA = bd;
		} else {
			NIA = CIA + bd;
		}
	}
	dbgprintf("instr ppc_bcx(%08x)\n", icode);
}

/*
 * ----------------------------------------
 * sc
 * Systemcall 
 * not complete
 * ----------------------------------------
 */

void
ppc_sc(uint32_t icode)
{
	uint32_t mask = 0x87c0ff73;
	SRR0 = CIA + 4;
	SRR1 = SRR1 & ~0x783f0000;
	SRR1 = (SRR1 & ~mask) | (MSR & mask);
//      MSR = bla;
//      NIA = bla;
//      PpcCpu_Exception(EX_SYSCALL,NIA,MSR);
	fprintf(stderr, "instr ppc_sc(%08x) not implemented\n", icode);
}

/*
 * ----------------------------------
 * bx  UISA Form I
 * ----------------------------------
 */
void
ppc_bx(uint32_t icode)
{
	int32_t li;
	int aa = icode & (1 << 1);
	int lk = icode & 1;
	if (icode & 0x02000000) {
		li = (icode & 0x03fffffc) | 0xfc000000;
	} else {
		li = icode & 0x03fffffc;
	}
	if (lk) {
		LR = CIA + 4;
	}
	if (aa) {
		NIA = li;
	} else {
		NIA = CIA + li;
	}
	dbgprintf("instr ppc_bx(%08x)\n", icode);
}

/*
 * ---------------------------------------
 * mcrf UISA Form XL
 * Move condition register field
 * v1
 * ---------------------------------------
 */
void
ppc_mcrf(uint32_t icode)
{
	uint32_t mask;
	int crfd = 7 - ((icode >> 23) & 7);	// shit PPC bit counting 
	int crfs = 7 - ((icode >> 18) & 7);
	uint32_t setbits;
	setbits = ((CR >> (4 * crfs)) & 0xf) << (4 * crfd);
	mask = ~(0xf << (4 * crfd));
	CR = (CR & mask) | setbits;
	dbgprintf("instr ppc_mcrf(%08x)\n", icode);
}

/*
 * ------------------------------------------
 * bclrx UISA Form XL
 * v1
 * ------------------------------------------
 */

void
ppc_bclrx(uint32_t icode)
{
	uint32_t bo = (icode >> 21) & 0x1f;
	uint32_t bi = (icode >> 16) & 0x1f;
	int lk = icode & 1;
	int ctr_ok;
	int cond_ok;
	if (!((bo >> (4 - 2)) & 1)) {
		CTR -= 1;
	}
	ctr_ok = ((bo >> (4 - 2)) & 1) | ((CTR != 0) ^ ((bo >> (4 - 3)) & 1));
	cond_ok = (bo & (1 << (4 - 0))) | (((CR >> (31 - bi)) & 1) == ((bo >> (4 - 1)) & 1));
	fprintf(stderr, "from CIA %08x \n", CIA);
	if (ctr_ok & cond_ok) {
		uint32_t tmp_lr = LR;
		if (lk) {
			LR = CIA + 4;
		}
		NIA = tmp_lr & 0xfffffffc;
	}
	fprintf(stderr, "instr ppc_bclrx(%08x)  to NIA %08x\n", icode, NIA);
}

/*
 * -------------------------------------------
 * crnor  UISA Form XL 
 * v1
 * -------------------------------------------
 */
void
ppc_crnor(uint32_t icode)
{
	uint32_t crbD = 31 - ((icode >> 21) & 0x1f);
	uint32_t crbA = 31 - ((icode >> 16) & 0x1f);
	uint32_t crbB = 31 - ((icode >> 11) & 0x1f);
	if ((CR & (1 << crbA)) || (CR & (1 << crbB))) {
		CR = CR & ~(1 << crbD);
	} else {
		CR = CR | (1 << crbD);
	}
	dbgprintf("instr ppc_crnor(%08x)\n", icode);
}

/* 
 * -----------------------------------
 * rfi OEA Supervisor Form XL 
 * return from interrupt
 * incomplete v1
 * -----------------------------------
 */
void
ppc_rfi(uint32_t icode)
{
	uint32_t mask = 0x87c0ff73;
#if 0
	if (!supervisor) {
		fprintf(stderr, "Mist\n");
		Exception();
	}
#endif
	PpcSetMsr((MSR & ~(mask | (1 << 18))) | (SRR1 & mask));
	NIA = SRR0 & 0xfffffffc;
	fprintf(stderr, "instr ppc_rfi(%08x) incomplete\n", icode);
}

/*
 * ------------------------------
 * crandc UISA Form XL
 * v1
 * ------------------------------
 */
void
ppc_crandc(uint32_t icode)
{
	uint32_t crbD = 31 - ((icode >> 21) & 0x1f);
	uint32_t crbA = 31 - ((icode >> 16) & 0x1f);
	uint32_t crbB = 31 - ((icode >> 11) & 0x1f);
	if ((CR & (1 << crbA)) && !(CR & (1 << crbB))) {
		CR = CR | (1 << crbD);
	} else {
		CR = CR & ~(1 << crbD);
	}
	dbgprintf("instr ppc_crandc(%08x)\n", icode);
}

/* 
 * ------------------------------------------------------------------------------
 * isync VEA Form XL
 * 	Instruction Synchronize
 *	Currently does nothing because Instruction pipeline is not implemented
 * ------------------------------------------------------------------------------
 */
void
ppc_isync(uint32_t icode)
{
	dbgprintf("instr ppc_isync(%08x) ignored\n", icode);
}

/*
 * ----------------------------------------
 * crxor UISA Form XL
 * v1
 * ----------------------------------------
 */
void
ppc_crxor(uint32_t icode)
{
	uint32_t crbD = 31 - ((icode >> 21) & 0x1f);
	uint32_t crbA = 31 - ((icode >> 16) & 0x1f);
	uint32_t crbB = 31 - ((icode >> 11) & 0x1f);
	if ((((CR >> crbA) & 1) != ((CR >> crbB) & 1))) {
		CR = CR | (1 << crbD);
	} else {
		CR = CR & ~(1 << crbD);
	}
	dbgprintf("instr ppc_crxor(%08x)\n", icode);
}

/*
 * ------------------------------
 * crnand UISA Form XL
 * v1
 * ------------------------------
 */
void
ppc_crnand(uint32_t icode)
{
	uint32_t crbD = 31 - ((icode >> 21) & 0x1f);
	uint32_t crbA = 31 - ((icode >> 16) & 0x1f);
	uint32_t crbB = 31 - ((icode >> 11) & 0x1f);
	if ((CR & (1 << crbA)) && (CR & (1 << crbB))) {
		CR = CR & ~(1 << crbD);
	} else {
		CR = CR | (1 << crbD);
	}
	dbgprintf("instr ppc_crnand(%08x)\n", icode);
}

/*
 * ---------------------------
 * crand UISA Form XL
 * v1
 * ---------------------------
 */
void
ppc_crand(uint32_t icode)
{
	uint32_t crbD = 31 - ((icode >> 21) & 0x1f);
	uint32_t crbA = 31 - ((icode >> 16) & 0x1f);
	uint32_t crbB = 31 - ((icode >> 11) & 0x1f);
	if ((CR & (1 << crbA)) && (CR & (1 << crbB))) {
		CR = CR | (1 << crbD);
	} else {
		CR = CR & ~(1 << crbD);
	}
	dbgprintf("instr ppc_crand(%08x)\n", icode);
}

/*
 * ---------------------------------------
 * creqv UISA Form XL
 * v1
 * ---------------------------------------
 */
void
ppc_creqv(uint32_t icode)
{
	uint32_t crbD = 31 - ((icode >> 21) & 0x1f);
	uint32_t crbA = 31 - ((icode >> 16) & 0x1f);
	uint32_t crbB = 31 - ((icode >> 11) & 0x1f);
	if (((CR >> crbA) & 1) == ((CR >> crbB) & 1)) {
		CR = CR | (1 << crbD);
	} else {
		CR = CR & ~(1 << crbD);
	}
	dbgprintf("instr ppc_creqv(%08x)\n", icode);
}

/*
 * --------------------
 * crorc UISA Form XL
 * v1
 * --------------------
 */
void
ppc_crorc(uint32_t icode)
{
	uint32_t crbD = 31 - ((icode >> 21) & 0x1f);
	uint32_t crbA = 31 - ((icode >> 16) & 0x1f);
	uint32_t crbB = 31 - ((icode >> 11) & 0x1f);
	if ((CR & (1 << crbA)) || !(CR & (1 << crbB))) {
		CR = CR | (1 << crbD);
	} else {
		CR = CR & ~(1 << crbD);
	}
	dbgprintf("instr ppc_crorc(%08x)\n", icode);
}

/*
 * --------------------------
 * cror UISA Form XL
 * v1
 * --------------------------
 */
void
ppc_cror(uint32_t icode)
{
	uint32_t crbD = 31 - ((icode >> 21) & 0x1f);
	uint32_t crbA = 31 - ((icode >> 16) & 0x1f);
	uint32_t crbB = 31 - ((icode >> 11) & 0x1f);
	if ((CR & (1 << crbA)) || (CR & (1 << crbB))) {
		CR = CR | (1 << crbD);
	} else {
		CR = CR & ~(1 << crbD);
	}
	dbgprintf("instr ppc_cror(%08x)\n", icode);
}

/*
 *--------------------------------
 * bcctrx UISA Form XL
 * 	Jump to count register
 * v1
 *--------------------------------
 */
void
ppc_bcctrx(uint32_t icode)
{
	uint32_t bo = (icode >> 21) & 0x1f;
	uint32_t bi = (icode >> 16) & 0x1f;
	int lk = icode & 1;
	int cond_ok;
	cond_ok = (bo & (1 << 4)) | (((CR >> (31 - bi)) & 1) == ((bo >> (4 - 1)) & 1));
	if (cond_ok) {
		if (lk) {
			LR = CIA + 4;
		}
		NIA = CTR & 0xfffffffc;
	}
	dbgprintf("instr ppc_bcctrx(%08x)\n", icode);
}

/* stolen from pearpc */
static inline uint32_t
mix_mask(int32_t mb, int32_t me)
{
	uint32_t mask;
	if (mb <= me) {
		if (me - mb == 31) {
			mask = 0xffffffff;
		} else {
			mask = ((1 << (me - mb + 1)) - 1) << (31 - me);
		}
	} else {
		int rot = 31 - me;
		uint32_t w = (1 << (32 - mb + me + 1)) - 1;
		if (rot) {
			mask = (w >> rot) | (w << (32 - rot));
		} else {
			mask = w;
		}
	}
	return mask;
}

/*
 * -----------------------------------------------
 * rlwimix UISA Form M
 * 	Rotate left immediate then mask insert
 * mix_mask not verified
 * -----------------------------------------------
 */
void
ppc_rlwimix(uint32_t icode)
{
	int s = (icode >> 21) & 0x1f;
	int a = (icode >> 16) & 0x1f;
	int sl = (icode >> 11) & 0x1f;
	int mb = (icode >> 6) & 0x1f;
	int me = (icode >> 1) & 0x1f;
	int rc = icode & 1;
	uint32_t r;
	uint32_t mask;
	uint32_t result;
	r = (GPR(s) << sl) | (GPR(s) >> (32 - sl));
	mask = mix_mask(mb, me);
	result = GPR(a) = (r & mask) | (GPR(a) & ~mask);
	if (rc) {
		update_cr0(result);
	}
	fprintf(stderr, "instr ppc_rlwimix(%08x) not tested\n", icode);
}

/*
 * -------------------------------------------------------
 * rlwinmx 
 * 	Rotate left word immediate then and with mask
 * mix_mask not verified
 * -------------------------------------------------------
 */
void
ppc_rlwinmx(uint32_t icode)
{
	int s = (icode >> 21) & 0x1f;
	int a = (icode >> 16) & 0x1f;
	int sl = (icode >> 11) & 0x1f;
	int mb = (icode >> 6) & 0x1f;
	int me = (icode >> 1) & 0x1f;
	int rc = icode & 1;
	uint32_t r;
	uint32_t mask;
	uint32_t result;
	r = (GPR(s) << sl) | (GPR(s) >> (32 - sl));
	mask = mix_mask(mb, me);
	result = GPR(a) = (r & mask);
	if (rc) {
		update_cr0(result);
	}
	dbgprintf("instr ppc_rlwinmx(%08x)\n", icode);
}

void
ppc_rlwnmx(uint32_t icode)
{
	int s = (icode >> 21) & 0x1f;
	int a = (icode >> 16) & 0x1f;
	int b = (icode >> 11) & 0x1f;
	int sl;
	int mb = (icode >> 6) & 0x1f;
	int me = (icode >> 1) & 0x1f;
	int rc = icode & 1;
	uint32_t r;
	uint32_t mask;
	uint32_t result;
	sl = GPR(b) & 0x1f;
	r = (GPR(s) << sl) | (GPR(s) >> (32 - sl));
	mask = mix_mask(mb, me);
	result = GPR(a) = (r & mask);
	if (rc) {
		update_cr0(result);
	}
	fprintf(stderr, "instr ppc_rlwnmx(%08x) not tested\n", icode);
}

/* 
 * -----------------------------
 * ori UISA Form D 
 * OR immediate
 * v1	
 * -----------------------------
 */
void
ppc_ori(uint32_t icode)
{
	int s = (icode >> 21) & 0x1f;
	int a = (icode >> 16) & 0x1f;
	uint16_t uimm = icode;
	GPR(a) = GPR(s) | uimm;
	/* no registers else are changed */
}

/*
 * ------------------------------------------
 * oris UISA Form D
 * OR immediate shifted
 * v1
 * ------------------------------------------
 */
void
ppc_oris(uint32_t icode)
{
	int s = (icode >> 21) & 0x1f;
	int a = (icode >> 16) & 0x1f;
	uint32_t uimm = (icode & 0xffff) << 16;
	GPR(a) = GPR(s) | uimm;
	/* no registers else are changed */
}

/*
 * ---------------------------------------------------------------------
 * xori  UISA Form D
 * 	XOR immediate
 * v1
 * ---------------------------------------------------------------------
 */
void
ppc_xori(uint32_t icode)
{
	int s = (icode >> 21) & 0x1f;
	int a = (icode >> 16) & 0x1f;
	uint16_t uimm = (icode & 0xffff);
	GPR(a) = GPR(s) ^ uimm;
	dbgprintf("instr ppc_xori(%08x)\n", icode);
}

/* 
 * -------------------------------------------------------------------
 * xoris UISA Form D
 *	XOR Immediate shifted
 * v1
 * -------------------------------------------------------------------
 */
void
ppc_xoris(uint32_t icode)
{
	int s = (icode >> 21) & 0x1f;
	int a = (icode >> 16) & 0x1f;
	uint32_t uimm = (icode & 0xffff) << 16;
	GPR(a) = GPR(s) ^ uimm;
	/* no registers else are changed */
	dbgprintf("instr ppc_xoris(%08x)\n", icode);
}

/*
 * -----------------------
 * andi. UISA Form D 
 * v1
 * -----------------------
 */
void
ppc_andppc_(uint32_t icode)
{
	uint32_t result;
	int s = (icode >> 21) & 0x1f;
	int a = (icode >> 16) & 0x1f;
	uint16_t uimm = icode & 0xffff;
	result = GPR(s) & uimm;
	update_cr0(result);
	GPR(a) = result;
	dbgprintf("instr ppc_andi(%08x)\n", icode);
}

/*
 * ----------------------------
 * andis. UISA Form D
 * v1
 * ----------------------------
 */
void
ppc_andis_(uint32_t icode)
{
	uint32_t result;
	int s = (icode >> 21) & 0x1f;
	int a = (icode >> 16) & 0x1f;
	uint32_t uimm = (icode & 0xffff) << 16;
	result = GPR(s) & uimm;
	GPR(a) = result;
	update_cr0(result);
	dbgprintf("instr ppc_andis(%08x)\n", icode);
}

void
ppc_rldiclx(uint32_t icode)
{
	fprintf(stderr, "instr ppc_rldclx(%08x) not implemented\n", icode);
}

void
ppc_rldicrx(uint32_t icode)
{
	fprintf(stderr, "instr ppc_rldicrx(%08x) not implemented\n", icode);
}

void
ppc_rldicx(uint32_t icode)
{
	fprintf(stderr, "instr ppc_rldicx(%08x) not implemented\n", icode);
}

void
ppc_rldimix(uint32_t icode)
{
	fprintf(stderr, "instr ppc_rldimix(%08x) not implemented\n", icode);
}

void
ppc_rldclx(uint32_t icode)
{
	fprintf(stderr, "instr ppc_rldclx(%08x) not implemented\n", icode);
}

void
ppc_rldcrx(uint32_t icode)
{
	fprintf(stderr, "instr ppc_rldcrx(%08x) not implemented\n", icode);
}

/* 
 * -----------------------------------------------------------
 * cmp UISA Form X
 * 866UM says on page 127 that the L bit is ignored
 * v1
 * -----------------------------------------------------------
 */
void
ppc_cmp(uint32_t icode)
{
	uint32_t crfd = 7 - ((icode >> 23) & 7);
	uint32_t a = (icode >> 16) & 0x1f;
	uint32_t b = (icode >> 11) & 0x1f;
	int32_t Ra = GPR(a);
	int32_t Rb = GPR(b);
	uint32_t c;
#if 0
	int L = (icode >> 21) & 1;
	if (L) {
		fprintf(stderr, "Invalid instruction format for cmp icode %08x at %08x\n", icode,
			CIA);
	}
#endif
	if (Ra < Rb) {
		c = 8;
	} else if (Ra > Rb) {
		c = 4;
	} else {
		c = 2;
	}
	if (XER & XER_SO) {
		c |= 1;
	}
	CR &= 0xffffffff ^ (0xf << (crfd << 2));
	CR |= c << (crfd << 2);
	dbgprintf("instr ppc_cmp(%08x)\n", icode);
}

/*
 * --------------------------------------------------------
 * tw UISA Form X
 * 	Trap Word
 * --------------------------------------------------------
 */
void
ppc_tw(uint32_t icode)
{
	int to = (icode >> 21) & 0x1f;
	int a = (icode >> 16) & 0x1f;
	int b = (icode >> 11) & 0x1f;
	int32_t A = GPR(a);
	int32_t B = GPR(b);
	if (((int32_t) A < (int32_t) B) && (to & (1 << 0))) {
		// Exception
	}
	if (((int32_t) A > (int32_t) B) && (to & (1 << 1))) {
		// Exception
	}
	if ((A == B) && (to & (1 << 2))) {
		// Exception
	}
	if (((uint32_t) A < (uint32_t) B) && (to & (1 << 3))) {
		// Exception
	}
	if (((uint32_t) A > (uint32_t) B) && (to & (1 << 4))) {
		// Exception
	}
	fprintf(stderr, "instr ppc_tw(%08x) not implemented\n", icode);
}

/*
 * ------------------------------------------------------------
 * subfcx
 * 	Subtract from Carrying
 * ------------------------------------------------------------
 */
void
ppc_subfcx(uint32_t icode)
{
	int d = (icode >> 21) & 0x1f;
	int a = (icode >> 16) & 0x1f;
	int b = (icode >> 11) & 0x1f;
	int rc = icode & 1;
	int oe = icode & (1 << 10);
	uint32_t result;
	uint32_t op1 = GPR(b), op2 = GPR(a);
	GPR(d) = result = op1 - op2;
	if (sub_carry(op1, op2, result)) {
		XER = XER | XER_CA;
	} else {
		XER = XER & ~XER_CA;
	}
	if (oe) {
		if (sub_overflow(op1, op2, result)) {
			XER = XER | XER_OV | XER_SO;
		} else {
			XER = XER & ~XER_OV;
		}
	}
	if (rc) {
		update_cr0(result);
	}
	dbgprintf("instr ppc_subfcx(%08x)\n", icode);
}

void
ppc_mulhdux(uint32_t icode)
{
	fprintf(stderr, "instr ppc_mulhdux(%08x) not implemented\n", icode);
}

/*
 * --------------------------------------
 * ADDCx UISA Form XO
 * v1
 * --------------------------------------
 */
void
ppc_addcx(uint32_t icode)
{
	int d = (icode >> 21) & 0x1f;
	int a = (icode >> 16) & 0x1f;
	int b = (icode >> 11) & 0x1f;
	int oe = icode & (1 << 10);
	int rc = icode & 1;
	uint32_t result, op1, op2;
	op1 = GPR(a);
	op2 = GPR(b);
	GPR(d) = result = op1 + op2;
	if (add_carry(op1, op2, result)) {
		XER = XER | XER_CA;
	} else {
		XER = XER & ~XER_CA;
	}
	if (oe) {
		if (add_overflow(op1, op2, result)) {
			XER |= XER_SO | XER_OV;
		} else {
			XER &= ~XER_OV;
		}
	}
	if (rc) {
		update_cr0(result);
	}
	dbgprintf("instr ppc_addcx(%08x)\n", icode);
}

/*
 * ---------------------------------------------------
 * mulhwux UISA Form XO
 * 	Multiply high word unsigned 
 * v1
 * ---------------------------------------------------
 */
void
ppc_mulhwux(uint32_t icode)
{
	int d = (icode >> 21) & 0x1f;
	int a = (icode >> 16) & 0x1f;
	int b = (icode >> 11) & 0x1f;
	int rc = icode & 1;
	uint32_t result;
	uint64_t prod = (uint64_t) GPR(a) * (uint64_t) GPR(b);
	result = GPR(d) = (prod >> 32);
	if (rc) {
		update_cr0(result);
	}
	dbgprintf("instr ppc_mulhwux(%08x)\n", icode);
}

/*
 * ---------------------------------------------------------------
 * mfcr  UISA Form X
 * Move from Condition register
 * v1 
 * ---------------------------------------------------------------
 */
void
ppc_mfcr(uint32_t icode)
{
	int d = (icode >> 21) & 0x1f;
#if 0
	check_illegal icode
#endif
	 GPR(d) = CR;
	dbgprintf("instr ppc_mfcr(%08x)\n", icode);
}

/*
 * -------------------------------------------
 * lwarx UISA Form X
 * 	Load word and reserve indexed
 * v1 
 * -------------------------------------------
 */
void
ppc_lwarx(uint32_t icode)
{
	int d = (icode >> 21) & 0x1f;
	int a = (icode >> 16) & 0x1f;
	int b = (icode >> 11) & 0x1f;
	uint32_t ea;
	if (a == 0) {
		ea = GPR(b);
	} else {
		ea = GPR(a) + GPR(b);
	}
	if (!(ea & 3)) {
		fprintf(stderr, "DSI exception 0x00300 missing here\n");
		return;
		// Alignment exception
	}
	gppc.reservation_valid = 1;
	gppc.reservation = ea;
	GPR(d) = PPCMMU_Read32(ea);
	dbgprintf("instr ppc_lwarx(%08x)\n", icode);
}

void
ppc_ldx(uint32_t icode)
{
	fprintf(stderr, "instr ppc_ldx(%08x) not implemented\n", icode);
}

/*
 * ----------------------------------------------------
 * lwzx UISA Form X 
 *   Load Word and zero indexed
 * v1	
 * ----------------------------------------------------
 */
void
ppc_lwzx(uint32_t icode)
{
	int d = (icode >> 21) & 0x1f;
	int a = (icode >> 16) & 0x1f;
	int b = (icode >> 11) & 0x1f;
	uint32_t ea;
	if (a == 0) {
		ea = GPR(b);
	} else {
		ea = GPR(a) + GPR(b);
	}
	GPR(d) = PPCMMU_Read32(ea);
	dbgprintf("instr ppc_lwzx(%08x)\n", icode);
}

/*
 * --------------------------------------------------------
 * slwx
 * Shift left word
 * --------------------------------------------------------
 */
void
ppc_slwx(uint32_t icode)
{
	int s = (icode >> 21);
	int a = (icode >> 16);
	int b = (icode >> 11);
	int rc = icode & 1;
	int sh = GPR(b) & 0x3f;
	uint32_t result;
	if (sh > 31) {
		result = GPR(a) = 0;
	} else {
		result = GPR(a) = GPR(s) << sh;
	}
	if (rc) {
		update_cr0(result);
	}
	dbgprintf("instr ppc_slwx(%08x)\n", icode);
}

/*
 * ----------------------------
 * cntlzw UISA Form X
 * v1
 * ----------------------------
 */
void
ppc_cntlzwx(uint32_t icode)
{
	int s = (icode >> 21) & 0x1f;
	int a = (icode >> 16) & 0x1f;
	int b = (icode >> 11) & 0x1f;
	uint32_t result;
	int rc = icode & 1;
	if (b) {
		fprintf(stderr, "Illegal instruction format\n");
		return;
	}
	result = GPR(a) = count_leading_zeros(GPR(s));
	if (rc) {
		update_cr0(result);
	}
	dbgprintf("instr ppc_cntlzwx(%08x)\n", icode);
}

void
ppc_sldx(uint32_t icode)
{
	fprintf(stderr, "instr ppc_sldx(%08x) not implemented\n", icode);
}

/*
 * ----------------------------
 * andx UISA Form X
 * v1
 * ----------------------------
 */
void
ppc_andx(uint32_t icode)
{
	uint32_t result;
	int s = (icode >> 21) & 0x1f;
	int a = (icode >> 16) & 0x1f;
	int b = (icode >> 11) & 0x1f;
	int rc = icode & 1;
	result = GPR(s) & GPR(b);
	GPR(a) = result;
	if (rc) {
		update_cr0(result);
	}
	dbgprintf("instr ppc_andx(%08x) not implemented\n", icode);
}

/*
 * --------------------------------------
 * cmpl UISA Form X 
 * v1
 * --------------------------------------
 */
void
ppc_cmpl(uint32_t icode)
{
	uint32_t crfd = 7 - ((icode >> 23) & 7);
	uint32_t a = (icode >> 16) & 0x1f;
	uint32_t b = (icode >> 11) & 0x1f;
	uint32_t Ra = GPR(a);
	uint32_t Rb = GPR(b);
	int L = (icode >> 21) & 1;
	uint32_t c;
	if (L) {
		fprintf(stderr, "Invalid instruction for cmpl\n");
		return;
	}
	if (Ra < Rb) {
		c = 8;
	} else if (Ra > Rb) {
		c = 4;
	} else {
		c = 2;
	}
	if (XER & XER_SO) {
		c |= 1;
	}
	CR &= 0xffffffff ^ (0xf << (crfd << 2));
	CR |= c << (crfd << 2);
	fprintf(stderr, "instr ppc_cmpl(%08x)\n", icode);
}

/*
 * -------------------------------------------------------
 * subfx UISA Form XO
 * 	Subtract from
 * -------------------------------------------------------
 */
void
ppc_subfx(uint32_t icode)
{
	int d = (icode >> 21) & 0x1f;
	int a = (icode >> 16) & 0x1f;
	int b = (icode >> 11) & 0x1f;
	int rc = icode & 1;
	int oe = icode & (1 << 10);
	uint32_t op1 = GPR(b), op2 = GPR(a);
	uint32_t result;
	result = GPR(d) = op1 - op2;
	if (oe) {
		if (sub_overflow(op1, op2, result)) {
			XER = XER | XER_OV | XER_SO;
		} else {
			XER = XER & ~XER_OV;
		}
	}
	if (rc) {
		update_cr0(result);
	}
	dbgprintf("instr ppc_subfx(%08x)\n", icode);
}

/* Load doubleword with update index: 64 Bit impl. only */
void
ppc_ldux(uint32_t icode)
{
	fprintf(stderr, "instr ppc_ldux(%08x) not implemented\n", icode);
}

/*
 * ------------------------------------------------
 * dcbst VEA Form X
 * 	Data cache block store
 * 	Currently does nothing because no cache is
 *	emulated
 * v1
 * ------------------------------------------------
 */
void
ppc_dcbst(uint32_t icode)
{
	fprintf(stderr, "ignore ppc_dcbst(%08x)\n", icode);
}

/*
 * ------------------------------------------
 * lwzux UISA Form X
 * Load word and zero with update indexed
 * v1
 * ------------------------------------------
 */
void
ppc_lwzux(uint32_t icode)
{
	int d = (icode >> 21) & 0x1f;
	int a = (icode >> 16) & 0x1f;
	int b = (icode >> 11) & 0x1f;
	uint32_t ea;
	ea = GPR(a) + GPR(b);
	GPR(d) = PPCMMU_Read32(ea);
	GPR(a) = ea;
	fprintf(stderr, "instr ppc_lwzux(%08x)\n", icode);
}

void
ppc_zntlzdx(uint32_t icode)
{
	fprintf(stderr, "instr ppc_zntlzdx(%08x) not implemented\n", icode);
}

/*
 * ----------------------
 * andcx UISA Form X
 * v1
 * ----------------------
 */
void
ppc_andcx(uint32_t icode)
{
	uint32_t result;
	int s = (icode >> 21) & 0x1f;
	int a = (icode >> 16) & 0x1f;
	int b = (icode >> 11) & 0x1f;
	int rc = icode & 1;
	GPR(a) = result = GPR(s) & ~GPR(b);
	if (rc) {
		update_cr0(result);
	}
	fprintf(stderr, "instr ppc_andcx(%08x) not implemented\n", icode);
}

void
ppc_td(uint32_t icode)
{
	fprintf(stderr, "instr ppc_td(%08x) not implemented\n", icode);
}

void
ppc_mulhdx(uint32_t icode)
{
	fprintf(stderr, "instr ppc_mulhdx(%08x) not implemented\n", icode);
}

/*
 * -------------------------------------
 * mulhwx UISA Form XO
 *	Multiply high word
 * -------------------------------------
 */
void
ppc_mulhwx(uint32_t icode)
{
	int d = (icode >> 21) & 0x1f;
	int a = (icode >> 16) & 0x1f;
	int b = (icode >> 11) & 0x1f;
	int rc = icode & 1;
	uint32_t result;
	int64_t prod = (int64_t) GPR(a) * (int64_t) GPR(b);
	result = GPR(d) = (prod >> 32);
	if (rc) {
		update_cr0(result);
	}
	fprintf(stderr, "instr ppc_mulhwx(%08x) not implemented\n", icode);
}

/*
 * --------------------------------------------
 * mfmsr UISA supervisor Form X
 *	Move from Machine status register
 * --------------------------------------------
 */
void
ppc_mfmsr(uint32_t icode)
{
	int d = (icode >> 21) & 0x1f;
#if 0
	if (!supervisor) {
		exception();
	}
#endif
	GPR(d) = MSR;
//      fprintf(stderr,"instr ppc_mfmsr(%08x) not implemented\n",icode);
}

void
ppc_ldarx(uint32_t icode)
{
	fprintf(stderr, "instr ppc_ldarx(%08x) not implemented\n", icode);
}

/*
 * ---------------------------------------------------------
 * dcbf VEA Form X
 * 	Data Cache Block flush
 * 	Do nothing because currently no cache is emulated 
 * v1
 * ---------------------------------------------------------
 */
void
ppc_dcbf(uint32_t icode)
{
	fprintf(stderr, "ignore ppc_dcbf(%08x)\n", icode);
}

/*
 * -----------------------------------------------
 * lbzx  UISA Form X
 * Load Byte and zero indexed 
 * -----------------------------------------------
 */
void
ppc_lbzx(uint32_t icode)
{
	int d = (icode >> 21) & 0x1f;
	int a = (icode >> 16) & 0x1f;
	int b = (icode >> 11) & 0x1f;
	uint32_t ea;
	if (a == 0) {
		ea = GPR(b);
	} else {
		ea = GPR(a) + GPR(b);
	}
	GPR(d) = PPCMMU_Read8(ea);
	fprintf(stderr, "instr ppc_lbzx(%08x) not implemented\n", icode);
}

/*
 * ----------------------------------------------------
 * negx UISA Form XO
 * 	two's complement
 * v1
 * ----------------------------------------------------
 */
void
ppc_negx(uint32_t icode)
{
	int d = (icode >> 21) & 0x1f;
	int a = (icode >> 16) & 0x1f;
	int oe = (icode >> 10) & 1;
	int rc = icode & 1;
	uint32_t result;
	result = GPR(d) = ~GPR(a) + 1;
	if (oe) {
		if (result == 0x80000000) {
			XER = XER | XER_SO | XER_OV;
		} else {
			XER = XER & ~XER_OV;
		}
	}
	if (rc) {
		update_cr0(result);
	}
	fprintf(stderr, "instr ppc_negx(%08x)\n", icode);
}

/*
 * ----------------------------------------------
 * lbzux UISA Form X
 * Load Byte and Zero with update indexed
 * v1
 * ----------------------------------------------
 */
void
ppc_lbzux(uint32_t icode)
{
	int d = (icode >> 21) & 0x1f;
	int a = (icode >> 16) & 0x1f;
	int b = (icode >> 11) & 0x1f;
	uint32_t ea;
	if ((a == 0) || (a == d)) {
		/* Trigger exception here */
		fprintf(stderr, "illegal instruction format\n");
		return;
	}
	ea = GPR(a) + GPR(b);
	GPR(d) = PPCMMU_Read8(ea);
	GPR(a) = ea;
	fprintf(stderr, "instr ppc_lbzux(%08x)\n", icode);
}

/*
 * ----------------------------------------------
 * norx UISA form X
 * v1
 * ----------------------------------------------
 */
void
ppc_norx(uint32_t icode)
{
	int s = (icode >> 21) & 0x1f;
	int a = (icode >> 16) & 0x1f;
	int b = (icode >> 11) & 0x1f;
	int rc = icode & 1;
	uint32_t result;
	result = GPR(a) = ~(GPR(s) | GPR(b));
	if (rc) {
		update_cr0(result);
	}
	fprintf(stderr, "instr ppc_norx(%08x) not implemented\n", icode);
}

/*
 * -----------------------------------------------------------------
 * subfex UISA Form XO
 * 	Subtract from extended
 * -----------------------------------------------------------------
 */
void
ppc_subfex(uint32_t icode)
{
	int d = (icode >> 21) & 0x1f;
	int a = (icode >> 16) & 0x1f;
	int b = (icode >> 11) & 0x1f;
	int rc = icode & 1;
	int oe = icode & (1 << 10);
	uint32_t result;
	uint32_t op1 = GPR(b), op2 = GPR(a);
	if (XER & XER_CA) {
		GPR(d) = result = op1 - op2;
	} else {
		GPR(d) = result = op1 - op2 - 1;
	}
	if (sub_carry(op1, op2, result)) {
		XER = XER | XER_CA;
	} else {
		XER = XER & ~XER_CA;
	}
	if (oe) {
		if (sub_overflow(op1, op2, result)) {
			XER = XER | XER_OV | XER_SO;
		} else {
			XER = XER & ~XER_OV;
		}
	}
	if (rc) {
		update_cr0(result);
	}
//      fprintf(stderr,"instr ppc_subfex(%08x)\n",icode);
}

/*
 * ----------------------------------------
 * addex UISA form XO 
 * v1
 * ----------------------------------------
 */
void
ppc_addex(uint32_t icode)
{
	int d = (icode >> 21) & 0x1f;
	int a = (icode >> 16) & 0x1f;
	int b = (icode >> 11) & 0x1f;
	int oe = icode & (1 << 10);
	int rc = icode & 1;
	uint32_t result, op1, op2;
	op1 = GPR(a);
	op2 = GPR(b);
	result = op1 + op2;
	if (XER & XER_CA) {
		result++;
	}
	GPR(d) = result;
	if (add_carry(op1, op2, result)) {
		XER |= XER_CA;
	} else {
		XER &= ~XER_CA;
	}
	if (oe) {
		if (add_overflow(op1, op2, result)) {
			XER |= XER_SO | XER_OV;
		} else {
			XER &= ~XER_OV;
		}
	}
	if (rc) {
		update_cr0(result);
	}
	fprintf(stderr, "instr ppc_addex(%08x)\n", icode);
}

/*
 * ---------------------------------------------------------
 * mtcrf  UISA Form XFX
 *	Move to condition register fields
 * ---------------------------------------------------------
 */
void
ppc_mtcrf(uint32_t icode)
{
	int s = (icode >> 21) & 0x1f;
	int crm = (icode >> 12) & 0xff;
	uint32_t mask = 0;
	int i;
	for (i = 0; i < 8; i++) {
		if (crm & (1 << i)) {
			mask |= (0xf << (i * 4));
		}
	}
	CR = (GPR(s) & mask) | (CR & ~mask);
	fprintf(stderr, "instr ppc_mtcrf(%08x)\n", icode);
}

/* 
 * --------------------------
 * OEA Supervisor Form X 
 * --------------------------
 */
void
ppc_mtmsr(uint32_t icode)
{
	int s = (icode >> 21) & 0x1f;
#if 0
	if (icode & bla) {
		fprintf(stderr, "Illegal icode %08x\n", icode);
		Exception();
	}
	if (!oea supervisor) {
		Exception();
	}
#endif
	PpcSetMsr(GPR(s));
	fprintf(stderr, "instr ppc_mtmsr(%08x)\n", icode);
}

void
ppc_stdx(uint32_t icode)
{
	fprintf(stderr, "instr ppc_stdx(%08x) not implemented\n", icode);
}

/* 
 * --------------------------------------------
 * stwcx UISA Form x
 * 	Store Word Conditional indexed
 * v1
 * --------------------------------------------
 */
void
ppc_stwcx_(uint32_t icode)
{
	int s = (icode >> 21) & 0x1f;
	int a = (icode >> 16) & 0x1f;
	int b = (icode >> 11) & 0x1f;
	uint32_t ea;
	if (a) {
		ea = GPR(a) + GPR(b);
	} else {
		ea = GPR(b);
	}
	if (gppc.reservation_valid) {
		gppc.reservation_valid = 0;
		if (ea != gppc.reservation) {
			fprintf(stderr, "reservation for wrong address\n");
		}
		PPCMMU_Write32(GPR(s), ea);
		CR = (CR & ~(CR_LT | CR_GT | CR_SO)) | CR_EQ;
		if (XER & XER_SO) {
			CR |= CR_SO;
		}
	} else {
		CR = (CR & ~(CR_LT | CR_GT | CR_EQ | CR_SO));
		if (XER & XER_SO) {
			CR |= CR_SO;
		}
	}
	fprintf(stderr, "instr ppc_stwcx(%08x)\n", icode);
}

/*
 * -------------------------------------------------------------
 * stwx UISA Form X
 *	Store Word indexed
 * v1
 * -------------------------------------------------------------
 */
void
ppc_stwx(uint32_t icode)
{
	int s = (icode >> 21) & 0x1f;
	int a = (icode >> 16) & 0x1f;
	int b = (icode >> 11) & 0x1f;
	uint32_t ea;
	if (a) {
		ea = GPR(a) + GPR(b);
	} else {
		ea = GPR(b);
	}
	PPCMMU_Write32(GPR(s), ea);
	fprintf(stderr, "instr ppc_stwx(%08x)\n", icode);
}

void
ppc_stdux(uint32_t icode)
{
	fprintf(stderr, "instr ppc_stdux(%08x) not implemented\n", icode);
}

/*
 * -----------------------------------------------------------
 * stwux
 *	Store Word with update Indexed
 * -----------------------------------------------------------
 */
void
ppc_stwux(uint32_t icode)
{
	int s = (icode >> 21) & 0x1f;
	int a = (icode >> 16) & 0x1f;
	int b = (icode >> 11) & 0x1f;
	uint32_t ea;
	ea = GPR(a) + GPR(b);
	PPCMMU_Write32(GPR(s), ea);
	GPR(a) = ea;
	fprintf(stderr, "instr ppc_stwux(%08x)\n", icode);
}

/*
 * --------------------------------------------------
 * subfzex UISA Form XO
 *	Subtract from Zero extended
 * --------------------------------------------------
 */
void
ppc_subfzex(uint32_t icode)
{
	int d = (icode >> 21) & 0x1f;
	int a = (icode >> 21) & 0x1f;
	int oe = icode & (1 << 10);
	int rc = icode & 1;
	uint32_t result;
	if (XER & XER_CA) {
		result = 0 - GPR(a);
	} else {
		result = 0 - GPR(a) - 1;
	}
	if (sub_carry(0, GPR(a), result)) {
		XER = XER | XER_CA;
	} else {
		XER = XER & ~XER_CA;
	}
	if (oe) {
		if (sub_overflow(0, GPR(a), result)) {
			XER = XER | XER_OV | XER_SO;
		} else {
			XER = XER & ~XER_OV;
		}
	}
	if (rc) {
		update_cr0(result);
	}
	GPR(d) = result;
	fprintf(stderr, "instr ppc_subfzex(%08x)\n", icode);
}

/*
 * -----------------------------
 * addzex UISA Form XO
 * -----------------------------
 */
void
ppc_addzex(uint32_t icode)
{
	int d = (icode >> 21) & 0x1f;
	int a = (icode >> 16) & 0x1f;
	int oe = icode & (1 << 10);
	int rc = icode & 1;
	uint32_t result, op1;
	op1 = GPR(a);
	result = op1;
	if (XER & XER_CA) {
		result++;
	}
	if (add_carry(op1, 0, result)) {
		XER |= XER_CA;
	} else {
		XER &= ~XER_CA;
	}
	GPR(d) = result;
	if (oe) {
		if (add_overflow(op1, 0, result)) {
			XER |= XER_SO | XER_OV;
		} else {
			XER &= ~XER_OV;
		}
	}
	if (rc) {
		update_cr0(result);
	}
}

/*
 * -------------------------------------
 * mtsr OEA Supervisor Form X
 * 	move to Segment Register
 * incomplete v1
 * -------------------------------------
 */
void
ppc_mtsr(uint32_t icode)
{
	// OEA check missing //
	int s = (icode >> 21) & 0x1f;
	int sr = (icode >> 16) & 0xf;
#if 0
	if (!supervisor(x)) {
	Exception}
#endif
	SR(sr) = GPR(s);
	fprintf(stderr, "instr ppc_mtsr(%08x) not implemented\n", icode);
}

void
ppc_stdcx(uint32_t icode)
{
	fprintf(stderr, "instr ppc_stdcx(%08x) not implemented\n", icode);
}

/*
 * -----------------------------------------------------
 * Store Byte indexed
 * -----------------------------------------------------
 */
void
ppc_stbx(uint32_t icode)
{
	int s = (icode >> 21) & 0x1f;
	int a = (icode >> 16) & 0x1f;
	int b = (icode >> 11) & 0x1f;
	uint32_t ea;
	if (a) {
		ea = GPR(a) + GPR(b);
	} else {
		ea = GPR(b);
	}
	PPCMMU_Write8(GPR(s) & 0xff, ea);
	fprintf(stderr, "instr ppc_stbx(%08x)\n", icode);
}

/*
 * -------------------------------------------------------------------
 * subfmex UISA Form XO
 * 	Subtract from Minus one extended
 * -------------------------------------------------------------------
 */
void
ppc_subfmex(uint32_t icode)
{
	int d = (icode >> 21) & 0x1f;
	int a = (icode >> 21) & 0x1f;
	int oe = icode & (1 << 10);
	int rc = icode & 1;
	uint32_t result;
	if (XER & XER_CA) {
		result = -1 - GPR(a);
	} else {
		result = -1 - GPR(a) - 1;
	}
	if (sub_carry((uint32_t) - 1, GPR(a), result)) {
		XER = XER | XER_CA;
	} else {
		XER = XER & ~XER_CA;
	}
	if (oe) {
		if (sub_overflow((uint32_t) - 1, GPR(a), result)) {
			XER = XER | XER_OV | XER_SO;
		} else {
			XER = XER & ~XER_OV;
		}
	}
	if (rc) {
		update_cr0(result);
	}
	GPR(d) = result;
	fprintf(stderr, "instr ppc_subfmex(%08x)\n", icode);
}

void
ppc_mulld(uint32_t icode)
{
	fprintf(stderr, "instr ppc_mulld(%08x) not implemented\n", icode);
}

/*
 * ------------------------------------
 * addmex UISA Form XO
 * v1
 * ------------------------------------
 */
void
ppc_addmex(uint32_t icode)
{
	int d = (icode >> 21) & 0x1f;
	int a = (icode >> 16) & 0x1f;
	int oe = icode & (1 << 10);
	int rc = icode & 1;
	uint32_t result, op1, op2;
	op1 = GPR(a);
	op2 = (uint32_t) - 1;
	result = op1 + op2;
	if (XER & XER_CA) {
		result++;
	}
	GPR(d) = result;
	if (add_carry(op1, op2, result)) {
		XER |= XER_CA;
	} else {
		XER &= ~XER_CA;
	}
	if (oe) {
		if (add_overflow(op1, op2, result)) {
			XER |= XER_SO | XER_OV;
		} else {
			XER &= ~XER_OV;
		}
	}
	if (rc) {
		update_cr0(result);
	}
	fprintf(stderr, "instr ppc_addmex(%08x)\n", icode);
}

/*
 * -----------------------------------------------------------------
 * mullwx UISA Form XO
 * 
 * -----------------------------------------------------------------
 */
void
ppc_mullwx(uint32_t icode)
{
	int d = (icode >> 21) & 0x1f;
	int a = (icode >> 16) & 0x1f;
	int b = (icode >> 11) & 0x1f;
	int rc = icode & 1;
	int oe = (icode >> 10) & 1;
	uint32_t low, high;
	uint64_t prod;
	low = GPR(d) = prod = (int64_t) GPR(a) * (int64_t) GPR(b);
	if (oe) {
		high = prod >> 32;
		if ((high == 0) || (high == 0xffffffff)) {
			XER = XER & ~XER_OV;
		} else {
			XER = XER | XER_OV | XER_SO;
		}
	}
	if (rc) {
		update_cr0(low);
	}
	fprintf(stderr, "instr ppc_mullwx(%08x)\n", icode);
}

/*
 * ------------------------------------------
 * mtsrin OEA supervisor Form X
 * 	Move to segment register indirect
 * incomplete v1
 * ------------------------------------------
 */
void
ppc_mtsrin(uint32_t icode)
{
	/*  OEA Supervisor */
	int s = (icode >> 21) & 0x1f;
	int b = (icode >> 11) & 0x1f;
	SR(GPR(b) >> 28) = GPR(s);
	fprintf(stderr, "instr ppc_mtsrin(%08x) not implemented\n", icode);
}

/*
 *-----------------------------------------------------------
 * dcbtst VEA Form X
 * 	Data Block touch for store
 *	Currently ignored because no cache is emulated
 * v1
 *-----------------------------------------------------------
 */
void
ppc_dcbtst(uint32_t icode)
{
	int a = (icode >> 16) & 0x1f;
	int b = (icode >> 21) & 0x1f;
	uint32_t ea;
	if (a) {
		ea = GPR(a) + GPR(b);
	} else {
		ea = GPR(b);
	}
	fprintf(stderr, "ignore ppc_dcbtst(%08x)\n", icode);
}

/*
 * ----------------------------------
 * stbux UISA Form X
 *  	Store byte with update indexed
 * v1
 * ----------------------------------
 */
void
ppc_stbux(uint32_t icode)
{
	int s = (icode >> 21) & 0x1f;
	int a = (icode >> 16) & 0x1f;
	int b = (icode >> 11) & 0x1f;
	uint32_t ea;
	ea = GPR(a) + GPR(b);
	PPCMMU_Write8(GPR(s) & 0xff, ea);
	GPR(a) = ea;
	fprintf(stderr, "instr ppc_stbux(%08x)\n", icode);
}

/*
 * ----------------------------------
 * ADDx UISA Form XO 
 * v1
 * ----------------------------------
 */
void
ppc_addx(uint32_t icode)
{
	int d = (icode >> 21) & 0x1f;
	int a = (icode >> 16) & 0x1f;
	int b = (icode >> 11) & 0x1f;
	int oe = icode & (1 << 10);
	int rc = icode & 1;
	uint32_t result, op1, op2;
	op1 = GPR(a);
	op2 = GPR(b);
	GPR(d) = result = op1 + op2;
	if (oe) {
		if (add_overflow(op1, op2, result)) {
			XER |= XER_SO | XER_OV;
		} else {
			XER &= ~XER_OV;
		}
	}
	if (rc) {
		update_cr0(result);
	}
	dbgprintf("instr ppc_addx(%08x)\n", icode);
}

/*
 * ---------------------------------------
 * dcbt VEA Form X
 * 	Data block touch
 * v1
 * ---------------------------------------
 */
void
ppc_dcbt(uint32_t icode)
{
	int a = (icode >> 16) & 0x1f;
	int b = (icode >> 21) & 0x1f;
	uint32_t ea;
	if (a) {
		ea = GPR(a) + GPR(b);
	} else {
		ea = GPR(b);
	}
	fprintf(stderr, "ignore ppc_dcbt(%08x) not implemented\n", icode);
}

/*
 * --------------------------------------
 * lhzx UISA Form X
 * 	Load half word and Zero indexed
 * v1
 * --------------------------------------
 */
void
ppc_lhzx(uint32_t icode)
{
	int d = (icode >> 21) & 0x1f;
	int a = (icode >> 16) & 0x1f;
	int b = (icode >> 11) & 0x1f;
	uint32_t ea;
	if (a == 0) {
		ea = GPR(b);
	} else {
		ea = GPR(a) + GPR(b);
	}
	GPR(d) = PPCMMU_Read16(ea);
	fprintf(stderr, "instr ppc_lhzx(%08x)\n", icode);
}

/*
 * ---------------------------------
 * eqvx  UISA Form X
 * v1
 * ---------------------------------
 */
void
ppc_eqvx(uint32_t icode)
{
	int s = (icode >> 21) & 0x1f;
	int a = (icode >> 16) & 0x1f;
	int b = (icode >> 10) & 0x1f;
	int rc = icode & 1;
	uint32_t result;
	result = GPR(a) = ~(GPR(s) ^ GPR(b));
	if (rc) {
		update_cr0(result);
	}
	fprintf(stderr, "instr ppc_eqvx(%08x)\n", icode);
}

/*
 * ------------------------------------------------------
 * tlbie OEA supervisor optional Form X
 * Translation Lookaside Buffer invalidate Entry
 * Currently invalidates all
 * ------------------------------------------------------
 */
void
ppc_tlbie(uint32_t icode)
{
	PPCMMU_InvalidateTlb();
	fprintf(stderr, "instr ppc_tlbie(%08x)\n", icode);
}

/*
 * ------------------------------------------
 * eciwx
 * incomplete v1
 * ------------------------------------------
 */
void
ppc_eciwx(uint32_t icode)
{
	int d = (icode >> 21) & 0x1f;
	int a = (icode >> 16) & 0x1f;
	int b = (icode >> 11) & 0x1f;
	uint32_t ea;
	if (!(EAR & (1 << 31))) {
		/* Exception */
		fprintf(stderr, "DSI Exception missing here\n");
		return;
	}
	if (a == 0) {
		ea = GPR(b);
	} else {
		ea = GPR(a) + GPR(b);
	}
	GPR(d) = PPCMMU_Read32(ea);	/* Nocache */
	fprintf(stderr, "instr ppc_eciwx(%08x) not implemented\n", icode);
}

/*
 * ------------------------------------------------
 * lhzux UISA Form X
 * Load half word and zero with update indexed
 * ------------------------------------------------
 */
void
ppc_lhzux(uint32_t icode)
{
	int d = (icode >> 21) & 0x1f;
	int a = (icode >> 16) & 0x1f;
	int b = (icode >> 11) & 0x1f;
	uint32_t ea;
	ea = GPR(a) + GPR(b);
	GPR(d) = PPCMMU_Read16(ea);
	GPR(a) = ea;
	fprintf(stderr, "instr ppc_lhzux(%08x) not implemented\n", icode);
}

/*
 * --------------------------------------------------
 * xorx  UISA Form X
 * 	XOR
 * --------------------------------------------------
 */
void
ppc_xorx(uint32_t icode)
{
	int s = (icode >> 21) & 0x1f;
	int a = (icode >> 16) & 0x1f;
	int b = (icode >> 11) & 0x1f;
	int rc = icode & 1;
	uint32_t result;
	result = GPR(a) = GPR(s) ^ GPR(b);
	if (rc) {
		update_cr0(result);
	}
	dbgprintf("instr ppc_xorx(%08x)\n", icode);
}

/*
 * ------------------------------------------------------------------
 * mfspr
 * incomplete v1
 * ------------------------------------------------------------------
 */
void
ppc_mfspr(uint32_t icode)
{
	int d = (icode >> 21) & 0x1f;
	int n = (((icode >> 16) & 0x1f)) | (((icode >> 11) & 0x1f) << 5);
	/* Check for Supervisor here ! */
	int oea = 1;
	if (oea || (n == 1) || (n == 8) || (n == 9)) {
		if (HAS_SPR(n)) {
			GPR(d) = SPR(n);
		} else if (HAS_SPR_READ(n)) {
			GPR(d) = SPR_READ(n);
		} else {
			/* Illegal instruction type or undefined */
			fprintf(stderr, "Mist, nonexisting SPR %d\n", n);
		}
	} else {
		fprintf(stderr, "Mist, illegal mfspr %d icode %d\n", n, icode);
	}
	fprintf(stderr, "instr ppc_mfspr(%08x)\n", icode);
}

void
ppc_lwax(uint32_t icode)
{
	fprintf(stderr, "instr ppc_lwax(%08x) not implemented\n", icode);
}

/*
 * ------------------------------------------------
 * lhax UISA Form X
 *	Load Half word algebraic indexed	
 * v1
 * ------------------------------------------------
 */
void
ppc_lhax(uint32_t icode)
{
	int d = (icode >> 21) & 0x1f;
	int a = (icode >> 16) & 0x1f;
	int b = (icode >> 11) & 0x1f;
	uint32_t ea;
	uint32_t result;
	if (a == 0) {
		ea = GPR(b);
	} else {
		ea = GPR(a) + GPR(b);
	}
	result = PPCMMU_Read16(ea);
	if (result & 0x8000) {
		GPR(d) = result | 0xffff0000;
	} else {
		GPR(d) = result;
	}
	fprintf(stderr, "instr ppc_lhax(%08x)\n", icode);
}

/*
 * ----------------------------------------------------
 * tlbia OEA supervisor optional Form X
 * 	Translation Lookaside Buffer invalidate all
 * ----------------------------------------------------
 */
void
ppc_tlbia(uint32_t icode)
{
	PPCMMU_InvalidateTlb();
	fprintf(stderr, "instr ppc_tlbia(%08x)\n", icode);
}

static void
actualize_tb(void)
{
	uint64_t timer_cycles;
	uint64_t tb;
	gppc.tb_saved_cycles += CycleCounter_Get() - gppc.last_tb_update;
	gppc.last_tb_update = CycleCounter_Get();
	timer_cycles = gppc.tb_saved_cycles * Clock_Freq(gppc.tmbclk) / Clock_Freq(gppc.cpuclk);
	tb = gppc.tbl + ((uint64_t) gppc.tbu << 32);
	tb += timer_cycles;
	gppc.tbl = tb & 0xffffffff;
	gppc.tbu = tb >> 32;
	gppc.tb_saved_cycles -= timer_cycles * Clock_Freq(gppc.cpuclk) / Clock_Freq(gppc.tmbclk);

	//fprintf(stderr,"TB update to %lld\n",tb);
	//sleep(1);
}

/*
 * ----------------------------------------------------------------------
 * mftb
 * move from timebase
 *
 * ----------------------------------------------------------------------
 */
void
ppc_mftb(uint32_t icode)
{
	/* VEA */
	int d = (icode >> 21) & 0x1f;
	int n = (((icode >> 16) & 0x1f)) | (((icode >> 11) & 0x1f) << 5);
	actualize_tb();
	if (n == 268) {
		GPR(d) = TBL;
	} else if (n == 269) {
		GPR(d) = TBU;
	} else {
		fprintf(stderr, "Illegal time base register\n");
		// Exception illegal instruction
	}
//      fprintf(stderr,"instr ppc_mftb(%08x)\n",icode);
}

void
ppc_lwaux(uint32_t icode)
{
	fprintf(stderr, "instr ppc_lwaux(%08x) not implemented\n", icode);
}

/*
 * -------------------------------------------------------
 * lhaux UISA Form X
 * 	Load half word algebraic with update indexed
 * v1
 * -------------------------------------------------------
 */
void
ppc_lhaux(uint32_t icode)
{
	int d = (icode >> 21) & 0x1f;
	int a = (icode >> 16) & 0x1f;
	int b = (icode >> 11) & 0x1f;
	uint32_t ea;
	uint32_t result;
	if ((a == 0) || (a == d)) {
		fprintf(stderr, "Illegal instruction format\n");
		return;
	}
	ea = GPR(a) + GPR(b);
	result = PPCMMU_Read16(ea);
	if (result & 0x8000) {
		GPR(d) = result | 0xffff0000;
	} else {
		GPR(d) = result;
	}
	GPR(a) = ea;
	fprintf(stderr, "instr ppc_lhaux(%08x) not implemented\n", icode);
}

/*
 * --------------------------------
 * sthx UISA Form X
 * 	Store Half Word Indexed
 * v1
 * --------------------------------
 */
void
ppc_sthx(uint32_t icode)
{
	int s = (icode >> 21) & 0x1f;
	int a = (icode >> 16) & 0x1f;
	int b = (icode >> 11) & 0x1f;
	uint32_t ea;
	if (a) {
		ea = GPR(a) + GPR(b);
	} else {
		ea = GPR(b);
	}
	PPCMMU_Write16(GPR(s) & 0xffff, ea);
	fprintf(stderr, "instr ppc_sthx(%08x)\n", icode);
}

/*
 * ----------------------------------------
 * orcx UISA Form X
 * OR with complement
 * ----------------------------------------
 */
void
ppc_orcx(uint32_t icode)
{
	int s = (icode >> 21) & 0x1f;
	int a = (icode >> 16) & 0x1f;
	int b = (icode >> 11) & 0x1f;
	int rc = icode & 1;
	uint32_t result;
	result = GPR(a) = GPR(s) | ~GPR(b);
	if (rc) {
		update_cr0(result);
	}
	fprintf(stderr, "instr ppc_orcx(%08x)\n", icode);
}

void
ppc_slbie(uint32_t icode)
{
	fprintf(stderr, "instr ppc_slbie(%08x) not implemented\n", icode);
}

/*
 * ----------------------------------------------
 * ecowx 
 * External Control word out indexed
 * incomplete v1
 * ----------------------------------------------
 */
void
ppc_ecowx(uint32_t icode)
{
	int s = (icode >> 21) & 0x1f;
	int a = (icode >> 16) & 0x1f;
	int b = (icode >> 11) & 0x1f;
	uint32_t ea;
	if (!(EAR & (1 << 31))) {
		fprintf(stderr, "exception missing here\n");
		return;
	}
	if (a == 0) {
		ea = GPR(b);
	} else {
		ea = GPR(a) + GPR(b);
	}
	if (ea & 3) {
		fprintf(stderr, "Alignment exception missing here\n");
		return;
	}
	PPCMMU_Write32(GPR(s), ea);	/* nochache */
	fprintf(stderr, "instr ppc_ecowx(%08x)\n", icode);
}

/*
 * ---------------------------------------------------------
 * sthux  UISA Form X
 * Store Half word with Update Indexed
 * v1
 * ---------------------------------------------------------
 */
void
ppc_sthux(uint32_t icode)
{
	int s = (icode >> 21) & 0x1f;
	int a = (icode >> 16) & 0x1f;
	int b = (icode >> 11) & 0x1f;
	uint32_t ea;
	ea = GPR(a) + GPR(b);
	PPCMMU_Write16(GPR(s) & 0xffff, ea);
	GPR(a) = ea;
	fprintf(stderr, "instr ppc_sthux(%08x)\n", icode);
}

/*
 * --------------------------------
 * orx UISA Form X 
 * v1
 * --------------------------------
 */
void
ppc_orx(uint32_t icode)
{
	uint32_t result;
	int s = (icode >> 21) & 0x1f;
	int a = (icode >> 16) & 0x1f;
	int b = (icode >> 11) & 0x1f;
	int rc = icode & 1;
	result = GPR(a) = GPR(s) | GPR(b);
	if (rc) {
		update_cr0(result);
	}
	fprintf(stderr, "instr ppc_orx(%08x) at %08x\n", icode, CIA);
}

void
ppc_divdux(uint32_t icode)
{
	fprintf(stderr, "instr ppc_divdux(%08x) not implemented\n", icode);
}

/*
 * --------------------------------
 * divwux UISA Form XO
 * v1
 * --------------------------------
 */
void
ppc_divwux(uint32_t icode)
{
	int d = (icode >> 21) & 0x1f;
	int a = (icode >> 16) & 0x1f;
	int b = (icode >> 11) & 0x1f;
	int oe = (icode >> 10) & 1;
	int rc = icode & 1;
	uint32_t result;
	if (GPR(b)) {
		result = GPR(a) / GPR(b);
	} else {
		fprintf(stderr, "Warning undefined result of division\n");
		result = 47110815;	/* undefined */
	}
	if (oe) {
		XER = XER & ~XER_OV;
		if (GPR(b) == 0) {
			XER |= XER_OV | XER_SO;
		}
	}
	if (rc) {
		update_cr0(result);
	}
	GPR(d) = result;
	fprintf(stderr, "instr ppc_divwux(%08x)\n", icode);
}

/*
 * ----------------------------------------------
 * mtspr UISA/OEA sometimes supervisor form XFX 
 * 	Move to special purpose register
 * incomplete v1
 * ----------------------------------------------
 */
void
ppc_mtspr(uint32_t icode)
{
	int s = (icode >> 21) & 0x1f;
	int n = (((icode >> 16) & 0x1f)) | (((icode >> 11) & 0x1f) << 5);
	/* Check for OEA here ! */
	int supervisor = 1;
	if (supervisor || (n == 1) || (n == 8) || (n == 9)) {
		if (HAS_SPR(n)) {
			SPR(n) = GPR(s);
			fprintf(stderr, "mtspr: SPR %d new value %08x from R%d\n", n, GPR(s), s);
		} else if (HAS_SPR_WRITE(n)) {
			SPR_WRITE(GPR(s), n);
		} else {
			fprintf(stderr, "mtspr: Mist, SPR %d does not exist, icode %08x\n", n,
				icode);
		}
		if (n == 9) {
			fprintf(stderr, "Load spr(9) with %08x\n", GPR(s));
		}
	} else {
		fprintf(stderr, "Mist, mtspr not allowed %08x\n", icode);
#if 0
		Exception();
#endif
		return;
	}
	dbgprintf("instr ppc_mtspr(%08x) not implemented\n", icode);
}

/*
 * -------------------------------------------------------
 * dcbi VEA Form X 
 *	Data Cache Block Invalidate
 *	Is ignored because no data cache is emulated
 * v1
 * --------------------------------------------------------
 */
void
ppc_dcbi(uint32_t icode)
{
	int a = (icode >> 16) & 0x1f;
	int b = (icode >> 11) & 0x1f;
	uint32_t ea;
	if (a) {
		ea = GPR(a) + GPR(b);
	} else {
		ea = GPR(b);
	}
#if 0
	if (!translate_address(ea)) {
		PPC_Exception(DSI);
	}
#endif
	fprintf(stderr, "ignore ppc_dcbi(%08x)\n", icode);
}

/*
 * ---------------------------------------------------
 * nandx UISA Form X
 * 	NAND
 * v1
 * ---------------------------------------------------
 */
void
ppc_nandx(uint32_t icode)
{
	int s = (icode >> 21) & 0x1f;
	int a = (icode >> 16) & 0x1f;
	int b = (icode >> 11) & 0x1f;
	int rc = icode & 1;
	uint32_t result;
	result = GPR(a) = ~(GPR(s) & GPR(b));
	if (rc) {
		update_cr0(result);
	}
	fprintf(stderr, "instr ppc_nandx(%08x) not implemented\n", icode);
}

void
ppc_divdx(uint32_t icode)
{
	fprintf(stderr, "instr ppc_divdx(%08x) not implemented\n", icode);
}

/*
 * ------------------------------------
 * divwx UISA Form XO
 * v1
 * ------------------------------------
 */
void
ppc_divwx(uint32_t icode)
{
	int d = (icode >> 21) & 0x1f;
	int a = (icode >> 16) & 0x1f;
	int b = (icode >> 11) & 0x1f;
	int oe = (icode >> 10) & 1;
	int rc = icode & 1;
	int32_t result;
	if (GPR(b)) {
		result = (int32_t) GPR(a) / (int32_t) GPR(b);
	} else {
		fprintf(stderr, "Warning undefined result of division\n");
		result = 0x47110815;	/* Manual says undefined */
	}
	if (oe) {
		XER = XER & ~XER_OV;
		if ((GPR(a) == 0x80000000) && (GPR(b) == 0xffffffff)) {
			XER |= XER_OV | XER_SO;
		}
		if (GPR(b) == 0) {
			XER |= XER_OV | XER_SO;
		}
	}
	if (rc) {
		update_cr0(result);
	}
	GPR(d) = result;
	fprintf(stderr, "instr ppc_divwx(%08x) not implemented\n", icode);
}

void
ppc_slbia(uint32_t icode)
{
	fprintf(stderr, "instr ppc_slbia(%08x) not implemented\n", icode);
}

/*
 * ---------------------------------------
 * mcrxr UISA Form X
 * Move to condition register from XER
 * ---------------------------------------
 */
void
ppc_mcrxr(uint32_t icode)
{
	int crfd = 7 - ((icode >> 23) & 7);
	uint32_t mask = ~(0xf << (4 * crfd));
#if 0
	check_illegal icode
#endif
	 CR = (CR & mask) | (((XER & 0xf0000000) >> 28) << (4 * crfd));
	XER = XER & 0x0fffffff;
	fprintf(stderr, "instr ppc_mcrxr(%08x)\n", icode);
}

/* 
 * --------------------------------------------------------------
 * lswx  UISA Form X
 * Load string word indexed
 * v1
 * --------------------------------------------------------------
 */
void
ppc_lswx(uint32_t icode)
{
	int d = (icode >> 21) & 0x1f;
	int a = (icode >> 16) & 0x1f;
	int b = (icode >> 11) & 0x1f;
	int n = XER & 0x7f;
	int r;
	int i;
	uint32_t ea;
	if (a == 0) {
		ea = GPR(b);
	} else {
		ea = GPR(b) + GPR(a);
	}
	r = GPR(d) - 1;
	i = 0;
	while (n > 0) {
		if (i == 0) {
			r = (r + 1) & 31;
			GPR(r) = 0;
		}
		/* Create Exception on segment Boudary is missing here */
		GPR(r) = GPR(r) | (PPCMMU_Read8(ea) << (24 - i));
		i = (i + 8) & 31;
		ea++;
		n--;
	}
	fprintf(stderr, "instr ppc_lswx(%08x)\n", icode);
}

/* 
 * ---------------------------------------------
 * lwbrx UISA Form X
 * Load  word byte reversed indexed
 * v1
 * ---------------------------------------------
 */
void
ppc_lwbrx(uint32_t icode)
{
	int d = (icode >> 21) & 0x1f;
	int a = (icode >> 16) & 0x1f;
	int b = (icode >> 11) & 0x1f;
	uint32_t ea;
	if (a == 0) {
		ea = GPR(b);
	} else {
		ea = GPR(b) + GPR(a);
	}
	GPR(d) = swap32(PPCMMU_Read32(ea));
	fprintf(stderr, "instr ppc_lwbrx(%08x)\n", icode);
}

void
ppc_lfsx(uint32_t icode)
{
	fprintf(stderr, "instr ppc_lfsx(%08x) not implemented\n", icode);
}

/*
 * --------------------------------------
 * srwx
 * Shift Right Word
 * --------------------------------------
 */

void
ppc_srwx(uint32_t icode)
{
	int s = (icode >> 21) & 0x1f;
	int a = (icode >> 16) & 0x1f;
	int b = (icode >> 11) & 0x1f;
	int rc = icode & 1;
	int sh = GPR(b) & 0x3f;
	uint32_t result;
	if (sh > 31) {
		result = GPR(a) = 0;
	} else {
		result = GPR(a) = GPR(s) >> sh;
	}
	if (rc) {
		update_cr0(result);
	}
	fprintf(stderr, "instr ppc_srwx(%08x) not implemented\n", icode);
}

void
ppc_srdx(uint32_t icode)
{
	fprintf(stderr, "instr ppc_srdx(%08x) not implemented\n", icode);
}

/*
 * -----------------------------------------------------------------------------
 * tlbsync
 *	Wait until all processors have invalidated the outstanding TLB-Entries
 * -----------------------------------------------------------------------------
 */
void
ppc_tlbsync(uint32_t icode)
{
	fprintf(stderr, "instr ppc_tlbsync(%08x) ignored\n", icode);
}

void
ppc_lfsux(uint32_t icode)
{
	fprintf(stderr, "instr ppc_lfsux(%08x) not implemented\n", icode);
}

void
ppc_mfsr(uint32_t icode)
{
	// OEA check missing //
	int d = (icode >> 21) & 0x1f;
	int sr = (icode >> 16) & 0xf;
	GPR(d) = SR(sr);
	fprintf(stderr, "instr ppc_mfsr(%08x) not implemented\n", icode);
}

/*
 * -------------------------------------------------------
 * lswi UISA Form X
 * 	Load string word immediate
 * v1
 * -------------------------------------------------------
 */
void
ppc_lswi(uint32_t icode)
{
	int d = (icode >> 21) & 0x1f;
	int a = (icode >> 16) & 0x1f;
	int nb = (icode >> 11) & 0x1f;
	int i;
	int n;
	int r;
	uint32_t ea;
	if (a == 0) {
		ea = 0;
	} else {
		ea = GPR(a);
	}
	if (nb == 0) {
		n = 32;
	} else {
		n = nb;
	}
	r = GPR(d) - 1;
	i = 0;
	while (n > 0) {
		if (i == 0) {
			r = (r + 1) & 31;
			GPR(r) = 0;
		}
		GPR(r) = GPR(r) | PPCMMU_Read8(ea) << (24 - i);
		i = (i + 8) & 31;
		ea++;
		n--;
	}
	fprintf(stderr, "instr ppc_lswi(%08x) not implemented\n", icode);
}

/*
 * ------------------------------------------------
 * sync
 *	Synchronize
 * ------------------------------------------------
 */
void
ppc_sync(uint32_t icode)
{
	fprintf(stderr, "instr ppc_sync(%08x) currently does nothing\n", icode);
}

void
ppc_lfdx(uint32_t icode)
{
	fprintf(stderr, "instr ppc_lfdx(%08x) not implemented\n", icode);
}

void
ppc_lfdux(uint32_t icode)
{
	fprintf(stderr, "instr ppc_lfdux(%08x) not implemented\n", icode);
}

/*
 * -----------------------------------------------
 * mfsrin Move from Segment register indirect
 * v1
 * -----------------------------------------------
 */
void
ppc_mfsrin(uint32_t icode)
{
	/*  OEA Supervisor */
	int d = (icode >> 21) & 0x1f;
	int b = (icode >> 11) & 0x1f;
	GPR(d) = SR(GPR(b) >> 28);
	fprintf(stderr, "instr ppc_mfsrin(%08x) not implemented\n", icode);
}

/*
 * -----------------------------------------------
 * stswx UISA Form X
 * 	Store String Word Indexed
 * v1
 * -----------------------------------------------
 */
void
ppc_stswx(uint32_t icode)
{
	int s = (icode >> 21) & 0x1f;
	int a = (icode >> 16) & 0x1f;
	int b = (icode >> 11) & 0x1f;
	int n = XER & 0x7f;
	int r;
	int i;
	uint32_t ea;
	if (a == 0) {
		ea = GPR(b);
	} else {
		ea = GPR(b) + GPR(a);
	}
	r = GPR(s) - 1;
	i = 0;
	while (n > 0) {
		if (i == 0) {
			r = (r + 1) & 31;
		}
		PPCMMU_Write8(GPR(r) >> (24 - i), ea);
		i = (i + 8) & 31;
		ea++;
		n--;
	}
	fprintf(stderr, "instr ppc_stswx(%08x)\n", icode);
}

/*
 * ------------------------------------------------------------------
 * stwbrx UISA Form X
 * 	Store Word Byte Reversed Indexed
 * v1
 * ------------------------------------------------------------------
 */
void
ppc_stwbrx(uint32_t icode)
{
	int s = (icode >> 21) & 0x1f;
	int a = (icode >> 16) & 0x1f;
	int b = (icode >> 11) & 0x1f;
	uint32_t ea;
	if (a) {
		ea = GPR(a) + GPR(b);
	} else {
		ea = GPR(b);
	}
	PPCMMU_Write32(swap32(GPR(s)), ea);
	fprintf(stderr, "instr ppc_stwbrx(%08x)\n", icode);
}

void
ppc_stfsx(uint32_t icode)
{
	fprintf(stderr, "instr ppc_stfsx(%08x) not implemented\n", icode);
}

void
ppc_stfsux(uint32_t icode)
{
	fprintf(stderr, "instr ppc_stfsux(%08x) not implemented\n", icode);
}

/*
 * -------------------------------------------
 * stswi UISA Form X
 * 	Store String Word Immediate
 * v1
 * -------------------------------------------
 */
void
ppc_stswi(uint32_t icode)
{
	int s = (icode >> 21) & 0x1f;
	int a = (icode >> 16) & 0x1f;
	int nb = (icode >> 11) & 0x1f;
	int i;
	int n;
	int r;
	uint32_t ea;
	if (a == 0) {
		ea = 0;
	} else {
		ea = GPR(a);
	}
	if (nb == 0) {
		n = 32;
	} else {
		n = nb;
	}
	r = GPR(s) - 1;
	i = 0;
	while (n > 0) {
		if (i == 0) {
			r = (r + 1) & 31;
		}
		PPCMMU_Write8((GPR(r) >> (24 - i)) & 0xff, ea);
		i = (i + 8) & 31;
		ea++;
		n--;
	}
	fprintf(stderr, "instr ppc_stswi(%08x)\n", icode);
}

/*
 *
 */
void
ppc_stfdx(uint32_t icode)
{
	int s = (icode >> 21) & 0x1f;
	int a = (icode >> 16) & 0x1f;
	int b = (icode >> 11) & 0x1f;
	uint32_t ea;
	if (a) {
		ea = GPR(a) + GPR(b);
	} else {
		ea = GPR(b);
	}
	PPCMMU_Write64(FPR(s), ea);
	fprintf(stderr, "instr ppc_stfdx(%08x) not implemented\n", icode);
}

/*
 * -------------------------
 * dcba VEA Optional Form X
 * Currently does nothing
 * v1
 * -------------------------
 */
void
ppc_dcba(uint32_t icode)
{
	int a = (icode >> 16) & 0x1f;
	int b = (icode >> 11) & 0x1f;
	uint32_t ea;
	if (a) {
		ea = GPR(a) + GPR(b);
	} else {
		ea = GPR(b);
	}
	/* Write some random numbers to the memory block */
	fprintf(stderr, "instr ppc_dcba(%08x) ignored\n", icode);
}

void
ppc_stfdux(uint32_t icode)
{
	int s = (icode >> 21) & 0x1f;
	int a = (icode >> 16) & 0x1f;
	int b = (icode >> 11) & 0x1f;
	uint32_t ea;
	ea = GPR(a) + GPR(b);
	PPCMMU_Write64(FPR(s), ea);
	GPR(a) = ea;
	fprintf(stderr, "instr ppc_stfdux(%08x) not implemented\n", icode);
}

/*
 * ------------------------------------------
 * lhbrx
 * 	Load half word byte reversed indexed
 * v1
 * ------------------------------------------
 */
void
ppc_lhbrx(uint32_t icode)
{
	int d = (icode >> 21) & 0x1f;
	int a = (icode >> 16) & 0x1f;
	int b = (icode >> 11) & 0x1f;
	uint32_t ea;
	if (a == 0) {
		ea = GPR(b);
	} else {
		ea = GPR(a) + GPR(b);
	}
	GPR(d) = swap16(PPCMMU_Read16(ea));
	fprintf(stderr, "instr ppc_lhbrx(%08x)\n", icode);
}

/*
 * -------------------------------------------
 * srawx
 * 	Shift right algebraic word
 * carry untested
 * -------------------------------------------
 */
void
ppc_srawx(uint32_t icode)
{
	int s = (icode >> 21);
	int a = (icode >> 16);
	int b = (icode >> 11);
	int rc = icode & 1;
	int sh = GPR(b) & 0x3f;
	uint32_t result;
	XER = XER & ~XER_CA;
	if (sh > 31) {
		result = GPR(a) = 0;
	} else {
		if (GPR(s) & 0x80000000) {
			if (((GPR(s) >> sh) << sh) != GPR(s)) {
				XER |= XER_CA;
			}
		}
		result = GPR(a) = ((int32_t) GPR(s)) >> sh;
	}
	if (rc) {
		update_cr0(result);
	}
	fprintf(stderr, "instr ppc_srawx(%08x)\n", icode);
}

void
ppc_sradx(uint32_t icode)
{
	fprintf(stderr, "instr ppc_sradx(%08x) not implemented\n", icode);
}

/*
 * ---------------------------------------------
 * srawix
 * 	Shift right algebraic word immediate
 * ---------------------------------------------
 */
void
ppc_srawix(uint32_t icode)
{
	int s = (icode >> 21) & 0x1f;
	int a = (icode >> 16) & 0x1f;
	int rc = icode & 1;
	int sh = (icode >> 11) & 0x1f;
	uint32_t result;
	XER = XER & ~XER_CA;
	if (sh > 31) {
		result = GPR(a) = 0;
	} else {
		if (GPR(s) & 0x80000000) {
			if (((GPR(s) >> sh) << sh) != GPR(s)) {
				XER |= XER_CA;
			}
		}
		result = GPR(a) = ((int32_t) GPR(s)) >> sh;
	}
	if (rc) {
		update_cr0(result);
	}
	dbgprintf("instr ppc_srawix(%08x)\n", icode);
}

/*
 * ------------------------------------------
 * eieio Enforce in order execution of IO
 * ------------------------------------------
 */
void
ppc_eieio(uint32_t icode)
{
	fprintf(stderr, "instr ppc_eieio(%08x) not implemented\n", icode);
}

/*
 * --------------------------------------
 * sthbrx UISA Form X
 * 	Store Half Word reverse Indexed
 * v1
 * --------------------------------------
 */
void
ppc_sthbrx(uint32_t icode)
{
	int s = (icode >> 21) & 0x1f;
	int a = (icode >> 16) & 0x1f;
	int b = (icode >> 11) & 0x1f;
	uint32_t ea;
	if (a) {
		ea = GPR(a) + GPR(b);
	} else {
		ea = GPR(b);
	}
	PPCMMU_Write16(swap16(GPR(s) & 0xffff), ea);
	fprintf(stderr, "instr ppc_sthbrx(%08x)\n", icode);
}

/*
 * ----------------------------
 * extshx UISA Form X
 * v1
 * ----------------------------
 */
void
ppc_extshx(uint32_t icode)
{
	int s = (icode >> 21) & 0x1f;
	int a = (icode >> 16) & 0x1f;
	int b = (icode >> 11) & 0x1f;
	int rc = icode & 1;
	int16_t imm;
	if (b != 0) {
		fprintf(stderr, "Illegal instruction format\n");
		return;
	}
	imm = GPR(s);
	GPR(a) = imm;
	if (rc) {
		update_cr0(GPR(a));
	}
	fprintf(stderr, "instr ppc_extshx(%08x) not implemented\n", icode);
}

/*
 * ----------------------------
 * extsbx UISA Form X
 * Extend Sign Byte
 * v1
 * ----------------------------
 */
void
ppc_extsbx(uint32_t icode)
{
	int s = (icode >> 21) & 0x1f;
	int a = (icode >> 16) & 0x1f;
	int b = (icode >> 11) & 0x1f;
	int rc = icode & 1;
	int8_t imm;
	if (b != 0) {
		fprintf(stderr, "Illegal instruction format\n");
		return;
	}
	imm = GPR(s) & 0xff;
	GPR(a) = imm;
	if (rc) {
		update_cr0(GPR(a));
	}
	fprintf(stderr, "instr ppc_extsbx(%08x)\n", icode);
}

/*
 * ------------------------------------------------------------
 * icbi VEA Form X
 * Instruction Cache block invalidate
 * Currently does nothing because cache is not emulated
 * ------------------------------------------------------------
 */
void
ppc_icbi(uint32_t icode)
{
	fprintf(stderr, "instr ppc_icbi(%08x) ignored\n", icode);
}

void
ppc_stfiwx(uint32_t icode)
{
	int s = (icode >> 21) & 0x1f;
	int a = (icode >> 16) & 0x1f;
	int b = (icode >> 11) & 0x1f;
	uint32_t ea;
	if (a) {
		ea = GPR(a) + GPR(b);
	} else {
		ea = GPR(b);
	}
	PPCMMU_Write32(FPR(s) & 0xffffffff, ea);

	fprintf(stderr, "instr ppc_stfiwx(%08x) not implemented\n", icode);
}

void
ppc_extsw(uint32_t icode)
{
	fprintf(stderr, "instr ppc_extsw(%08x) not implemented\n", icode);
}

/*
 * -------------------------------------
 * dcbz
 * 	Data block clear to zero
 * v1
 * -------------------------------------
 */
void
ppc_dcbz(uint32_t icode)
{
	int i;
	int a = (icode >> 16) & 0x1f;
	int b = (icode >> 11) & 0x1f;
	uint32_t ea = GPR(b);
	if (a) {
		ea += GPR(a);
	}
	ea = ea & ~0x1f;
	/* Exception Check Granularity ? */
	for (i = 0; i < 4; i++) {
		PPCMMU_Write64(0, ea);
		ea += 8;
	}
	fprintf(stderr, "instr ppc_dcbz(%08x) not implemented\n", icode);
}

/*
 * ---------------------------------------------------------
 * lwz UISA Form D
 * 	Load word and zero
 * v1
 * ---------------------------------------------------------
 */
void
ppc_lwz(uint32_t icode)
{
	int d = (icode >> 21) & 0x1f;
	int a = (icode >> 16) & 0x1f;
	int16_t offs = icode & 0xffff;
	uint32_t ea;
	if (a == 0) {
		ea = offs;
	} else {
		ea = GPR(a) + offs;
	}
	GPR(d) = PPCMMU_Read32(ea);
	dbgprintf("instr ppc_lwz(%08x)\n", icode);
}

/*
 * ---------------------------------------------------------
 * lwzu UISA Form D
 * 	Load Word and Zero with Update
 * v1
 * ---------------------------------------------------------
 */
void
ppc_lwzu(uint32_t icode)
{
	int d = (icode >> 21) & 0x1f;
	int a = (icode >> 16) & 0x1f;
	int16_t offs = icode & 0xffff;
	uint32_t ea;
	ea = GPR(a) + offs;
	GPR(d) = PPCMMU_Read32(ea);
	GPR(a) = ea;
	fprintf(stderr, "instr ppc_lwzu(%08x) not implemented\n", icode);
}

/*
 * -----------------------------
 * lbz UISA Form D
 * Load Byte and zero
 * -----------------------------
 */
void
ppc_lbz(uint32_t icode)
{
	int d = (icode >> 21) & 0x1f;
	int a = (icode >> 16) & 0x1f;
	uint32_t ea;
	uint32_t offs;
	if (icode & 0x8000) {
		offs = icode | 0xffff0000;
	} else {
		offs = icode & 0xffff;
	}
	if (a == 0) {
		ea = offs;
	} else {
		ea = GPR(a) + offs;
	}
	GPR(d) = PPCMMU_Read8(ea);
	fprintf(stderr, "instr ppc_lbz(%08x)\n", icode);
}

/*
 * -----------------------------------------------------------
 * lbzu UISA Form D
 * 	Load Byte and Zero with update
 * v1 
 * -----------------------------------------------------------
 */
void
ppc_lbzu(uint32_t icode)
{
	int d = (icode >> 21) & 0x1f;
	int a = (icode >> 16) & 0x1f;
	uint32_t ea;
	uint32_t offs;
	if (icode & 0x8000) {
		offs = icode | 0xffff0000;
	} else {
		offs = icode & 0xffff;
	}
	if ((a == 0) || (a == d)) {
		fprintf(stderr, "illegal instruction format\n");
		return;
	}
	ea = GPR(a) + offs;
	GPR(d) = PPCMMU_Read8(ea);
	GPR(a) = ea;
	fprintf(stderr, "instr ppc_lbzu(%08x) not implemented\n", icode);
}

/* 
 * -----------------------------------------
 * stw UISA Form D
 *	Store Word
 * -----------------------------------------
 */
void
ppc_stw(uint32_t icode)
{
	int s = (icode >> 21) & 0x1f;
	int a = (icode >> 16) & 0x1f;
	int16_t imm = icode & 0xffff;
	uint32_t ea;
	if (a) {
		ea = GPR(a) + imm;
	} else {
		ea = imm;
	}
	PPCMMU_Write32(GPR(s), ea);
	dbgprintf("instr ppc_stw(%08x) not implemented\n", icode);
}

/*
 * ----------------------------------------------
 * stwu
 *	Store Word with update
 * v1
 * ----------------------------------------------
 */
void
ppc_stwu(uint32_t icode)
{
	int s = (icode >> 21) & 0x1f;
	int a = (icode >> 16) & 0x1f;
	int16_t imm = 0xffff;
	uint32_t ea;
	ea = GPR(a) + imm;
	PPCMMU_Write32(GPR(s), ea);
	fprintf(stderr, "instr ppc_stwu(%08x), val %08x\n", icode, GPR(s));
	GPR(a) = ea;
}

/*
 * ---------------------------------
 * stb UISA Form D
 * v1
 * ---------------------------------
 */
void
ppc_stb(uint32_t icode)
{
	int s = (icode >> 21) & 0x1f;
	int a = (icode >> 16) & 0x1f;
	int16_t imm = icode & 0xffff;
	uint32_t ea;
	if (a) {
		ea = GPR(a) + imm;
	} else {
		ea = imm;
	}
	PPCMMU_Write8(GPR(s) & 0xff, ea);
	fprintf(stderr, "instr ppc_stb(%08x)\n", icode);
}

/*
 * ---------------------------------
 * stbu UISA Form D
 *	Store Byte with update
 * v1
 * ---------------------------------
 */
void
ppc_stbu(uint32_t icode)
{
	int s = (icode >> 21) & 0x1f;
	int a = (icode >> 16) & 0x1f;
	int16_t imm = icode & 0xffff;
	uint32_t ea;
	ea = GPR(a) + imm;
	PPCMMU_Write8(GPR(s) & 0xff, ea);
	GPR(a) = ea;
	fprintf(stderr, "instr ppc_stbu(%08x)\n", icode);
}

/*
 * ---------------------------------
 * lhz UISA Form D
 * Load half word and zero
 * v1
 * ---------------------------------
 */
void
ppc_lhz(uint32_t icode)
{
	int d = (icode >> 21) & 0x1f;
	int a = (icode >> 16) & 0x1f;
	int16_t ofs = icode & 0xffff;
	uint32_t ea;
	if (a == 0) {
		ea = ofs;
	} else {
		ea = GPR(a) + ofs;
	}
	GPR(d) = PPCMMU_Read16(ea);
}

/*
 * -----------------------------------------
 * lhzu UISA Form D
 *  Load half word and zero with update
 * -----------------------------------------
 */
void
ppc_lhzu(uint32_t icode)
{
	int d = (icode >> 21) & 0x1f;
	int a = (icode >> 16) & 0x1f;
	int16_t ofs = icode & 0xffff;
	uint32_t ea;
	ea = GPR(a) + ofs;
	GPR(d) = PPCMMU_Read16(ea);
	GPR(a) = ea;
	fprintf(stderr, "instr ppc_lhzu(%08x) not implemented\n", icode);
}

/*
 * ------------------------------------------------------------
 * lha UISA Form D  
 *	Load Half word algebraic	
 * ------------------------------------------------------------
 */
void
ppc_lha(uint32_t icode)
{
	int d = (icode >> 21) & 0x1f;
	int a = (icode >> 16) & 0x1f;
	uint32_t ea;
	uint32_t offs = icode & 0x8000 ? (icode | 0xffff0000) : (icode & 0xffff);
	uint32_t result = 0xAffe;
	if (a == 0) {
		ea = offs;
	} else {
		ea = GPR(a) + offs;
	}
	result = PPCMMU_Read16(ea);
	if (result & 0x8000) {
		GPR(d) = result | 0xffff0000;
	} else {
		GPR(d) = result;
	}
	fprintf(stderr, "instr ppc_lha(%08x)\n", icode);
}

/*
 * --------------------------------------------
 * lhau UISA Form D
 * 	Load Half word algebraic with update
 * v1
 * --------------------------------------------
 */
void
ppc_lhau(uint32_t icode)
{
	int d = (icode >> 21) & 0x1f;
	int a = (icode >> 16) & 0x1f;
	uint32_t ea;
	uint32_t offs = icode & 0x8000 ? (icode | 0xffff0000) : (icode & 0xffff);
	uint32_t result = 0xAffe;
	if ((a == 0) || (a == d)) {
		fprintf(stderr, "Illegal instruction format\n");
		return;
	} else {
		ea = GPR(a) + offs;
	}
	result = PPCMMU_Read16(ea);
	if (result & 0x8000) {
		GPR(d) = result | 0xffff0000;
	} else {
		GPR(d) = result;
	}
	GPR(a) = ea;
	fprintf(stderr, "instr ppc_lhau(%08x)\n", icode);
}

/*
 * ----------------------------
 * sth UISA Form D
 * Store Half Word
 * v1
 * ----------------------------
 */
void
ppc_sth(uint32_t icode)
{
	int s = (icode >> 21) & 0x1f;
	int a = (icode >> 16) & 0x1f;
	int16_t imm = icode & 0xffff;
	uint32_t ea;
	if (a) {
		ea = GPR(a) + imm;
	} else {
		ea = imm;
	}
	PPCMMU_Write16(GPR(s) & 0xffff, ea);
	fprintf(stderr, "instr ppc_sth(%08x)\n", icode);
}

/*
 * ------------------------------------------------
 * sthu
 * Store Half Word with Update
 * v1
 * ------------------------------------------------
 */
void
ppc_sthu(uint32_t icode)
{
	int s = (icode >> 21) & 0x1f;
	int a = (icode >> 16) & 0x1f;
	int16_t imm = icode & 0xffff;
	uint32_t ea;
	ea = GPR(a) + imm;
	PPCMMU_Write16(GPR(s) & 0xffff, ea);
	GPR(a) = ea;
	fprintf(stderr, "instr ppc_sthu(%08x)\n", icode);
}

/*
 * ---------------------------------
 * lmw UISA Form D
 * Load Multiple Word
 * v1
 * ---------------------------------
 */
void
ppc_lmw(uint32_t icode)
{
	int d = (icode >> 21) & 0x1f;
	int a = (icode >> 16) & 0x1f;
	int16_t ofs = icode & 0xffff;
	uint32_t ea;
	uint32_t r;
	if (a == 0) {
		ea = ofs;
	} else {
		ea = GPR(a) + ofs;
	}
	r = GPR(d);
	while (r <= 31) {
		GPR(r) = PPCMMU_Read32(ea);
		ea += 4;
		r++;
	}
	fprintf(stderr, "instr ppc_lmw(%08x)\n", icode);
}

/*
 * ---------------------------------------------
 * stmw UISA Form D
 * 	Store Multiple Word
 * v1
 * ---------------------------------------------
 */
void
ppc_stmw(uint32_t icode)
{
	int s = (icode >> 21) & 0x1f;
	int a = (icode >> 16) & 0x1f;
	int16_t ofs = icode & 0xffff;
	uint32_t ea;
	uint32_t r;
	if (a == 0) {
		ea = ofs;
	} else {
		ea = GPR(a) + ofs;
	}
	r = GPR(s);
	while (r <= 31) {
		PPCMMU_Write32(GPR(r), ea);
		r = r + 1;
		ea = ea + 4;
	}
	fprintf(stderr, "instr ppc_stmw(%08x)\n", icode);
}

void
ppc_lfs(uint32_t icode)
{
	fprintf(stderr, "instr ppc_lfs(%08x) not implemented\n", icode);
}

void
ppc_lfsu(uint32_t icode)
{
	fprintf(stderr, "instr ppc_lfsu(%08x) not implemented\n", icode);
}

void
ppc_lfd(uint32_t icode)
{
	fprintf(stderr, "instr ppc_lfd(%08x) not implemented\n", icode);
}

void
ppc_lfdu(uint32_t icode)
{
	fprintf(stderr, "instr ppc_lfdu(%08x) not implemented\n", icode);
}

void
ppc_stfs(uint32_t icode)
{
	fprintf(stderr, "instr ppc_stfs(%08x) not implemented\n", icode);
}

void
ppc_stfsu(uint32_t icode)
{
	fprintf(stderr, "instr ppc_stfsu(%08x) not implemented\n", icode);
}

void
ppc_stfd(uint32_t icode)
{
	int s = (icode >> 21) & 0x1f;
	int a = (icode >> 16) & 0x1f;
	int16_t imm = icode & 0xffff;
	uint32_t ea;
	if (a) {
		ea = GPR(a) + imm;
	} else {
		ea = imm;
	}
	PPCMMU_Write64(FPR(s), ea);
	fprintf(stderr, "instr ppc_stfd(%08x) not implemented\n", icode);
}

void
ppc_stfdu(uint32_t icode)
{
	int s = (icode >> 21) & 0x1f;
	int a = (icode >> 16) & 0x1f;
	int16_t imm = icode & 0xffff;
	uint32_t ea;
	if (a) {
		ea = GPR(a) + imm;
	} else {
		ea = imm;
	}
	PPCMMU_Write64(FPR(s), ea);
	GPR(a) = ea;
	fprintf(stderr, "instr ppc_stfdu(%08x) at %08x not implemented\n", icode, CIA);
}

/* Load Double word: 64 Bit impl. only */
void
ppc_ld(uint32_t icode)
{
	fprintf(stderr, "instr ppc_ld(%08x) not implemented\n", icode);
}

/* Load Double word with update: 64 Bit impl. only */
void
ppc_ldu(uint32_t icode)
{
	fprintf(stderr, "instr ppc_ldu(%08x) not implemented\n", icode);
}

void
ppc_lwa(uint32_t icode)
{
	fprintf(stderr, "instr ppc_lwa(%08x) not implemented\n", icode);
}

void
ppc_fdivsx(uint32_t icode)
{
	fprintf(stderr, "instr ppc_fdivsx(%08x) not implemented\n", icode);
}

void
ppc_fsubsx(uint32_t icode)
{
	fprintf(stderr, "instr ppc_fsubsx(%08x) not implemented\n", icode);
}

void
ppc_faddsx(uint32_t icode)
{
	fprintf(stderr, "instr ppc_faddsx(%08x) not implemented\n", icode);
}

void
ppc_fsqrtsx(uint32_t icode)
{
	fprintf(stderr, "instr ppc_fsqrtsx(%08x) not implemented\n", icode);
}

void
ppc_fsresx(uint32_t icode)
{
	fprintf(stderr, "instr ppc_fsresx(%08x) not implemented\n", icode);
}

void
ppc_fmulsx(uint32_t icode)
{
	fprintf(stderr, "instr ppc_fmulsx(%08x) not implemented\n", icode);
}

void
ppc_fmsubsx(uint32_t icode)
{
	fprintf(stderr, "instr ppc_fmsubsx(%08x) not implemented\n", icode);
}

void
ppc_fmaddsx(uint32_t icode)
{
	fprintf(stderr, "instr ppc_fmaddsx(%08x) not implemented\n", icode);
}

void
ppc_fnmsubsx(uint32_t icode)
{
	fprintf(stderr, "instr ppc_fnmsubsx(%08x) not implemented\n", icode);
}

void
ppc_fnmaddsx(uint32_t icode)
{
	fprintf(stderr, "instr ppc_fnmaddsx(%08x) not implemented\n", icode);
}

void
ppc_std(uint32_t icode)
{
	fprintf(stderr, "instr ppc_std(%08x) not implemented\n", icode);
}

void
ppc_stdu(uint32_t icode)
{
	fprintf(stderr, "instr ppc_stdu(%08x) not implemented\n", icode);
}

void
ppc_fcmpu(uint32_t icode)
{
	fprintf(stderr, "instr ppc_fcmpu(%08x) not implemented\n", icode);
}

void
ppc_frspx(uint32_t icode)
{
	fprintf(stderr, "instr ppc_frspx(%08x) not implemented\n", icode);
}

void
ppc_fctiwx(uint32_t icode)
{
	fprintf(stderr, "instr ppc_fctiwx(%08x) not implemented\n", icode);
}

void
ppc_fctiwzx(uint32_t icode)
{
	fprintf(stderr, "instr ppc_fctiwzx(%08x) not implemented\n", icode);
}

void
ppc_fdivx(uint32_t icode)
{
	fprintf(stderr, "instr ppc_fdivx(%08x) not implemented\n", icode);
}

void
ppc_fsubx(uint32_t icode)
{
	fprintf(stderr, "instr ppc_fsubx(%08x) not implemented\n", icode);
}

void
ppc_faddx(uint32_t icode)
{
	fprintf(stderr, "instr ppc_faddx(%08x) not implemented\n", icode);
}

void
ppc_fsqrtx(uint32_t icode)
{
	fprintf(stderr, "instr ppc_fsqrtx(%08x) not implemented\n", icode);
}

void
ppc_fselx(uint32_t icode)
{
	fprintf(stderr, "instr ppc_fselx(%08x) not implemented\n", icode);
}

void
ppc_fmulx(uint32_t icode)
{
	fprintf(stderr, "instr ppc_fmulx(%08x) not implemented\n", icode);
}

void
ppc_fsqrtex(uint32_t icode)
{
	fprintf(stderr, "instr ppc_fsqrtex(%08x) not implemented\n", icode);
}

void
ppc_fmsubx(uint32_t icode)
{
	fprintf(stderr, "instr ppc_fmsubx(%08x) not implemented\n", icode);
}

void
ppc_fmaddx(uint32_t icode)
{
	fprintf(stderr, "instr ppc_fmaddx(%08x) not implemented\n", icode);
}

void
ppc_fnmsubx(uint32_t icode)
{
	fprintf(stderr, "instr ppc_fnmsubx(%08x) not implemented\n", icode);
}

void
ppc_fnmaddx(uint32_t icode)
{
	fprintf(stderr, "instr ppc_fnmaddx(%08x) not implemented\n", icode);
}

void
ppc_fcmpo(uint32_t icode)
{
	fprintf(stderr, "instr ppc_fcmpo(%08x) not implemented\n", icode);
}

/*
 * ----------------------------------------------
 * mtfsb1x UISA Form X
 * Move to FPSCR Bit 1
 * ----------------------------------------------
 */
void
ppc_mtfsb1x(uint32_t icode)
{
	int crbd = 31 - ((icode >> 21) & 0x1f);
	int rc = icode & 1;
	if ((crbd == 29) || (crbd == 30)) {
		fprintf(stderr, "mtfsb1x geht net\n");
		return;
	}
	FPSCR = FPSCR | (1 << crbd);
	if (rc) {
		CR = (CR & 0xf0ffffff) | ((FPSCR >> 4) & 0x0f000000);
	}
	fprintf(stderr, "instr ppc_mtfsb1x(%08x) not implemented\n", icode);
}

void
ppc_fnegx(uint32_t icode)
{
	fprintf(stderr, "instr ppc_fnegx(%08x) not implemented\n", icode);
}

#define FPSCR_EXCEPTIONS (FPSCR_VXCVI | FPSCR_VXSQRT |FPSCR_VXSOFT | FPSCR_VXVC | FPSCR_VXIMZ \
		| FPSCR_VXZDZ | FPSCR_VXIDI | FPSCR_VXISI | FPSCR_VXSNAN \
		| FPSCR_XX | FPSCR_ZX | FPSCR_UX  | FPSCR_OX | FPSCR_FX)

/*
 * --------------------------------------------------------
 * mcrfs UISA Form X
 * Move to Condition register from FPSCR		
 * --------------------------------------------------------
 */
void
ppc_mcrfs(uint32_t icode)
{
	uint32_t mask;
	uint32_t clear;
	uint32_t setbits;
	int crfd = 7 - ((icode >> 23) & 7);
	int crfs = 7 - ((icode >> 18) & 7);
	clear = FPSCR & (0xf << (4 * crfs)) & FPSCR_EXCEPTIONS;
	setbits = ((FPSCR >> (4 * crfs)) & 0xf) << (4 * crfd);
	mask = ~(0xf << (4 * crfd));
	CR = (CR & mask) | setbits;
	/* Clear the exception bits except FEX and VX */
	FPSCR = FPSCR ^ clear;
	//update_fex_vx(); /* Summary bits */
	fprintf(stderr, "instr ppc_mcrfs(%08x) not implemented\n", icode);
}

/*
 * ------------------------------------------------------------------
 * mtfsb0x Move to FPSCR Bit 0 
 * ------------------------------------------------------------------
 */
void
ppc_mtfsb0x(uint32_t icode)
{
	int crbd = 31 - ((icode >> 21) & 0x1f);
	int rc = icode & 1;
	if ((crbd == 29) || (crbd == 30)) {
		fprintf(stderr, "mtfsb0x geht net\n");
		return;
	}
	FPSCR = FPSCR & ~(1 << crbd);
	if (rc) {
		CR = (CR & 0xf0ffffff) | ((FPSCR >> 4) & 0x0f000000);
	}
	fprintf(stderr, "instr ppc_mtfsb0x(%08x) not implemented\n", icode);
}

void
ppc_fmrx(uint32_t icode)
{
	fprintf(stderr, "instr ppc_fmrx(%08x) not implemented\n", icode);
}

/*
 * ----------------------------------------------
 * mtfsfix UISA Form X
 * 	Move to FPSCR field immediate
 * ----------------------------------------------
 */
void
ppc_mtfsfix(uint32_t icode)
{
	unsigned int crfd = 7 - ((icode >> 23) & 0x7);
	unsigned int imm = (icode >> 12) & 0xf;
	int rc = icode & 1;
	uint32_t mask = ~(0x0000000fU << (crfd * 4));
	FPSCR = (FPSCR & mask) | (imm << (crfd * 4));
	if (rc) {
		CR = (CR & 0xf0ffffff) | ((FPSCR >> 4) & 0x0f000000);
	}
	fprintf(stderr, "instr ppc_mtfsfix(%08x)\n", icode);
}

void
ppc_fnabsx(uint32_t icode)
{
	fprintf(stderr, "instr ppc_fnabsx(%08x) not implemented\n", icode);
}

void
ppc_fabsx(uint32_t icode)
{
#if 0
	int frd = (icode >> 21) & 0xf;
	int frb = (icode >> 11) & 0xf;
	/* */
	fprintf(stderr, "instr ppc_fabsx(%08x) not implemented\n", icode);
#endif
}

void
ppc_mffsx(uint32_t icode)
{
	int d = (icode >> 21) & 0x1f;
	FPR(d) = (FPR(d) & 0xffffffff00000000ULL) | FPSCR;
	fprintf(stderr, "instr ppc_mffsx(%08x) not implemented\n", icode);
}

/*
 * ------------------------------------------------------------------
 * mtfsfx UISA Form XFL 
 * Move to FPSCR fields 
 * ------------------------------------------------------------------
 */
void
ppc_mtfsfx(uint32_t icode)
{
	int fm = (icode >> 17) & 0xff;
	int b = (icode >> 11) & 0x1f;
	int rc = icode & 1;
	uint32_t mask = 0;
	unsigned int i;
	for (i = 0; i < 8; i++) {
		if (fm & (1 << i)) {
			mask |= (0xf << (4 * i));
		}
	}
	FPSCR = (FPR(b) & mask) | (FPSCR & ~mask);
	if (rc) {
		CR = (CR & 0xf0ffffff) | ((FPSCR >> 4) & 0x0f000000);
	}
	fprintf(stderr, "instr ppc_mtfsfx(%08x)\n", icode);
}

void
ppc_fctdix(uint32_t icode)
{
	fprintf(stderr, "instr ppc_fctdix(%08x) not implemented\n", icode);
}

void
ppc_fcfidx(uint32_t icode)
{
	fprintf(stderr, "instr ppc_fcfidx(%08x) not implemented\n", icode);
}

void
ppc_und(uint32_t icode)
{
	fprintf(stderr, "Instruction not found for icode %08x at pc %08x\n", icode, CIA);
	//PPC_Exception(EX_UNDEFINED);
}
