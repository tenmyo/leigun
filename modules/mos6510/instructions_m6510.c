#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "instructions_m6510.h"
#include "cpu_m6510.h"
#include "mem_m6510.h"

uint8_t
addmode_Read()
{
	uint8_t addmode = (ICODE >> 2) & 7;
	uint16_t addr;
	uint16_t eaddr;
	uint16_t newaddr;
	uint8_t result;
	switch (addmode) {
	    case 2:
		    /* 
		     **********************************************
		     * It does the same in ACC based RMW but 
		     * forgets the result 
		     **********************************************
		     */
		    result = Mem_Read8(MOS_GetPC());
		    MOS_NextCycle();
		    MOS_IncPC();
		    break;

	    case 1:
		    addr = Mem_Read8(MOS_GetPC());
		    MOS_NextCycle();
		    MOS_IncPC();
		    result = Mem_Read8(addr);
		    MOS_NextCycle();
		    break;

	    case 5:
		    addr = Mem_Read8(MOS_GetPC());
		    MOS_NextCycle();
		    MOS_IncPC();
		    Mem_Read8(addr);	/* Forget the result */
		    addr = (addr + MOS_GetX()) & 0xff;	/* Stay in zeropage */
		    MOS_NextCycle();
		    MOS_IncPC();
		    result = Mem_Read8(addr);
		    MOS_NextCycle();
		    break;

	    case 3:
		    addr = Mem_Read8(MOS_GetPC());
		    MOS_IncPC();
		    MOS_NextCycle();
		    addr |= (uint16_t) Mem_Read8(MOS_GetPC()) << 8;
		    MOS_IncPC();
		    MOS_NextCycle();
		    result = Mem_Read8(addr);
		    MOS_NextCycle();
		    break;

	    case 7:
		    addr = Mem_Read8(MOS_GetPC());
		    MOS_NextCycle();
		    MOS_IncPC();
		    addr |= (uint16_t) Mem_Read8(MOS_GetPC()) << 8;
		    newaddr = addr + MOS_GetX();
		    MOS_NextCycle();
		    MOS_IncPC();
		    result = Mem_Read8((newaddr & 0xff) | (addr & 0xff00));
		    MOS_NextCycle();
		    if ((newaddr & 0xff00) != (addr & 0xff00)) {
			    result = Mem_Read8(newaddr);
			    MOS_NextCycle();
		    }
		    break;

	    case 6:
		    addr = Mem_Read8(MOS_GetPC());
		    MOS_NextCycle();
		    MOS_IncPC();
		    addr |= (uint16_t) Mem_Read8(MOS_GetPC()) << 8;
		    newaddr = addr + MOS_GetY();
		    MOS_NextCycle();
		    MOS_IncPC();
		    result = Mem_Read8((newaddr & 0xff) | (addr & 0xff00));
		    MOS_NextCycle();
		    if ((newaddr & 0xff00) != (addr & 0xff00)) {
			    result = Mem_Read8(newaddr);
			    MOS_NextCycle();
		    }
		    break;

	    case 0:
		    addr = Mem_Read8(MOS_GetPC());
		    MOS_IncPC();
		    MOS_NextCycle();
		    Mem_Read8(addr);
		    addr = (addr + MOS_GetX()) & 0xff;
		    MOS_NextCycle();
		    eaddr = Mem_Read8(addr);
		    MOS_NextCycle();
		    addr = (addr + 1) & 0xff;
		    eaddr |= (uint16_t) Mem_Read8(addr) << 8;
		    result = Mem_Read8(eaddr);
		    MOS_NextCycle();
		    break;

	    case 4:
		    addr = Mem_Read8(MOS_GetPC());
		    MOS_IncPC();
		    MOS_NextCycle();
		    eaddr = Mem_Read8(addr);
		    MOS_NextCycle();
		    addr = (addr + 1) & 0xff;
		    eaddr |= (uint16_t) Mem_Read8(addr);
		    MOS_NextCycle();
		    newaddr = eaddr + MOS_GetY();
		    result = Mem_Read8((newaddr & 0xff) | (eaddr & 0xff00));
		    MOS_NextCycle();
		    if ((newaddr & 0xff00) != (eaddr & 0xff00)) {
			    result = Mem_Read8(newaddr);
			    MOS_NextCycle();
		    }
		    break;

	}
	return result;
}

void
addmode_Write(uint8_t value)
{
	uint8_t addmode = (ICODE >> 2) & 7;
	uint16_t addr;
	uint16_t eaddr;
	uint16_t newaddr;
	uint8_t result;
	switch (addmode) {
	    case 2:
		    /* 
		     *************************************
		     * Immediate addressing #num makes 
		     * no sense for write instructions  
		     * it is a nop
		     *************************************
		     */
		    break;

	    case 1:		/* Zeropage absolute */
		    addr = Mem_Read8(MOS_GetPC());
		    MOS_NextCycle();
		    MOS_IncPC();
		    Mem_Write8(value, addr);
		    MOS_NextCycle();
		    break;

	    case 5:		/* Zero page , x */
		    addr = Mem_Read8(MOS_GetPC());
		    MOS_NextCycle();
		    MOS_IncPC();
		    Mem_Read8(addr);	/* Forget the result */
		    addr = (addr + MOS_GetX()) & 0xff;	/* Stay in zeropage */
		    MOS_NextCycle();
		    MOS_IncPC();
		    Mem_Write8(value, addr);
		    MOS_NextCycle();
		    break;

	    case 3:		/* Absolute 16 Bit address */
		    addr = Mem_Read8(MOS_GetPC());
		    MOS_IncPC();
		    MOS_NextCycle();
		    addr |= (uint16_t) Mem_Read8(MOS_GetPC()) << 8;
		    MOS_IncPC();
		    MOS_NextCycle();
		    Mem_Write8(value, addr);
		    MOS_NextCycle();
		    break;

	    case 7:		/* indexed absolute */
		    addr = Mem_Read8(MOS_GetPC());
		    MOS_NextCycle();
		    MOS_IncPC();
		    addr |= (uint16_t) Mem_Read8(MOS_GetPC()) << 8;
		    newaddr = addr + MOS_GetX();
		    MOS_NextCycle();
		    MOS_IncPC();
		    result = Mem_Read8((newaddr & 0xff) | (addr & 0xff00));
		    MOS_NextCycle();

		    Mem_Write8(value, newaddr);
		    MOS_NextCycle();
		    break;

	    case 6:
		    addr = Mem_Read8(MOS_GetPC());
		    MOS_NextCycle();
		    MOS_IncPC();
		    addr |= (uint16_t) Mem_Read8(MOS_GetPC()) << 8;
		    newaddr = addr + MOS_GetY();
		    MOS_NextCycle();
		    MOS_IncPC();
		    result = Mem_Read8((newaddr & 0xff) | (addr & 0xff00));
		    MOS_NextCycle();

		    Mem_Write8(value, newaddr);
		    MOS_NextCycle();
		    break;

	    case 0:
		    addr = Mem_Read8(MOS_GetPC());
		    MOS_IncPC();
		    MOS_NextCycle();
		    Mem_Read8(addr);
		    addr = (addr + MOS_GetX()) & 0xff;
		    MOS_NextCycle();
		    eaddr = Mem_Read8(addr);
		    MOS_NextCycle();
		    addr = (addr + 1) & 0xff;
		    eaddr |= (uint16_t) Mem_Read8(addr) << 8;
		    Mem_Write8(value, eaddr);
		    MOS_NextCycle();
		    break;

	    case 4:
		    addr = Mem_Read8(MOS_GetPC());
		    MOS_IncPC();
		    MOS_NextCycle();
		    eaddr = Mem_Read8(addr);
		    MOS_NextCycle();
		    addr = (addr + 1) & 0xff;
		    eaddr |= (uint16_t) Mem_Read8(addr);
		    MOS_NextCycle();
		    newaddr = eaddr + MOS_GetY();
		    result = Mem_Read8((newaddr & 0xff) | (eaddr & 0xff00));
		    MOS_NextCycle();
		    Mem_Write8(value, newaddr);
		    MOS_NextCycle();
		    break;

	}
}

/* 
 ******************************************************************
 * Read part of a Read modify write sequence
 * Writeback of value before modification by instruction
 * is already done here.
 ******************************************************************
 */

uint8_t
addmode_RMW_Read(uint16_t * retAddr)
{
	uint8_t addmode = (ICODE >> 2) & 7;
	uint16_t addr;
	uint16_t newaddr;
	uint8_t result;
	switch (addmode) {
	    case 2:
		    /* 
		     **********************************************
		     * forgets the result 
		     **********************************************
		     */
		    Mem_Read8(MOS_GetPC());
		    MOS_NextCycle();
		    MOS_IncPC();
		    result = MOS_GetAcc();
		    break;

	    case 1:		/* Zero page with writeback of unmodified */
		    addr = Mem_Read8(MOS_GetPC());
		    MOS_NextCycle();
		    MOS_IncPC();
		    result = Mem_Read8(addr);
		    MOS_NextCycle();
		    Mem_Write8(result, addr);
		    MOS_NextCycle();
		    *retAddr = addr;
		    break;

	    case 5:		/* Zeropage indexed with writeback */
		    addr = Mem_Read8(MOS_GetPC());
		    MOS_NextCycle();
		    MOS_IncPC();
		    Mem_Read8(addr);	/* Forget the result */
		    addr = (addr + MOS_GetX()) & 0xff;	/* Stay in zeropage */
		    MOS_NextCycle();
		    MOS_IncPC();
		    result = Mem_Read8(addr);
		    *retAddr = addr;
		    break;

	    case 3:		/* Absolute with writeback */
		    addr = Mem_Read8(MOS_GetPC());
		    MOS_IncPC();
		    MOS_NextCycle();
		    addr |= (uint16_t) Mem_Read8(MOS_GetPC()) << 8;
		    MOS_IncPC();
		    MOS_NextCycle();
		    result = Mem_Read8(addr);
		    MOS_NextCycle();
		    Mem_Write8(result, addr);
		    *retAddr = addr;
		    break;

	    case 7:
		    addr = Mem_Read8(MOS_GetPC());
		    MOS_NextCycle();
		    MOS_IncPC();
		    addr |= (uint16_t) Mem_Read8(MOS_GetPC()) << 8;
		    newaddr = addr + MOS_GetX();
		    MOS_NextCycle();
		    MOS_IncPC();
		    result = Mem_Read8((newaddr & 0xff) | (addr & 0xff00));
		    MOS_NextCycle();
		    result = Mem_Read8(newaddr);
		    MOS_NextCycle();
		    Mem_Write8(result, newaddr);
		    MOS_NextCycle();
		    *retAddr = newaddr;
		    break;

	    case 6:		/* NOP */
#if 0
		    addr = Mem_Read8(MOS_GetPC());
		    MOS_NextCycle();
		    MOS_IncPC();
		    addr |= (uint16_t) Mem_Read8(MOS_GetPC()) << 8;
		    newaddr = addr + MOS_GetY();
		    MOS_NextCycle();
		    MOS_IncPC();
		    result = Mem_Read8((newaddr & 0xff) | (addr & 0xff00));
		    MOS_NextCycle();
		    if ((newaddr & 0xff00) != (addr & 0xff00)) {
			    result = Mem_Read8(newaddr);
			    MOS_NextCycle();
		    }
#endif
		    result = 0;
		    break;

	    case 0:		/* JAM */
		    result = 0;
		    break;

	    case 4:		/* JAM */
		    result = 0;
		    break;
		    /* Unreachable, make the compiler happy */
	    default:
		    result = 0;

	}
	return result;
}

/**
 ************************************************************
 * Writeback of the new value part of the Read modify
 * write instructions.
 ************************************************************
 */
void
addmode_RMW_Write(uint8_t value, uint16_t addr)
{
	uint8_t addmode = (ICODE >> 2) & 7;
	switch (addmode) {
	    case 2:
		    /* The cpu cycles are already used up in Read */
		    MOS_SetAcc(value);
		    break;

	    case 1:		/* Zeropage absolute */
		    Mem_Write8(value, addr);
		    MOS_NextCycle();
		    break;

	    case 5:		/* Zero page , x */
		    Mem_Write8(value, addr);
		    MOS_NextCycle();
		    break;

	    case 3:		/* Absolute 16 Bit address */
		    Mem_Write8(value, addr);
		    MOS_NextCycle();
		    break;

	    case 7:		/* indexed absolute */
		    Mem_Write8(value, addr);
		    MOS_NextCycle();
		    break;

	    case 6:		/* NOP */
#if 0
		    Mem_Write8(value, addr);
		    MOS_NextCycle();
#endif
		    break;

	    case 0:		/* JAM */
#if 0
		    Mem_Write8(value, addr);
		    MOS_NextCycle();
#endif
		    break;

	    case 4:		/* JAM */
#if 0
		    Mem_Write8(value, newaddr);
		    MOS_NextCycle();
#endif
		    break;

	}
}

void
m6510_brk(void)
{
	fprintf(stderr, "m6510_brk not implemented\n");
}

void
m6510_ora(void)
{
	fprintf(stderr, "m6510_ora not implemented\n");
}

void
m6510_asl(void)
{

}

void
m6510_jam(void)
{
	fprintf(stderr, "m6510_jam not implemented\n");
}

void
m6510_slo(void)
{
	fprintf(stderr, "m6510_slo not implemented\n");
}

void
m6510_nop(void)
{
	fprintf(stderr, "m6510_nop not implemented\n");
}

void
m6510_asl_a(void)
{
	uint8_t acc = MOS_GetAcc();
	if (acc & 0x80) {
		REG_FLAGS |= FLG_C;
	} else {
		REG_FLAGS &= ~FLG_C;
	}
	acc = acc << 1;
	if (acc & 80) {
		REG_FLAGS |= FLG_N;
	} else {
		REG_FLAGS &= ~FLG_N;
	}
	if (acc == 0) {
		REG_FLAGS |= FLG_Z;
	} else {
		REG_FLAGS &= ~FLG_Z;
	}
	MOS_SetAcc(acc);
}

void
m6510_php(void)
{
	fprintf(stderr, "m6510_php not implemented\n");
}

void
m6510_anc(void)
{
	fprintf(stderr, "m6510_anc not implemented\n");
}

void
m6510_bpl(void)
{
	fprintf(stderr, "m6510_bpl not implemented\n");
}

void
m6510_clc(void)
{
	fprintf(stderr, "m6510_clc not implemented\n");
}

void
m6510_jsr(void)
{
	fprintf(stderr, "m6510_jsr not implemented\n");
}

void
m6510_and(void)
{
	fprintf(stderr, "m6510_and not implemented\n");
}

void
m6510_rla(void)
{
	fprintf(stderr, "m6510_rla not implemented\n");
}

void
m6510_bit(void)
{
	fprintf(stderr, "m6510_bit not implemented\n");
}

void
m6510_plp(void)
{
	fprintf(stderr, "m6510_plp not implemented\n");
}

void
m6510_rol(void)
{
	fprintf(stderr, "m6510_rol not implemented\n");
}

void
m6510_bmi(void)
{
	fprintf(stderr, "m6510_bmi not implemented\n");
}

void
m6510_sec(void)
{
	fprintf(stderr, "m6510_sec not implemented\n");
}

void
m6510_rti(void)
{
	fprintf(stderr, "m6510_rti not implemented\n");
}

void
m6510_eor(void)
{
	fprintf(stderr, "m6510_eor not implemented\n");
}

void
m6510_sre(void)
{
	fprintf(stderr, "m6510_sre not implemented\n");
}

void
m6510_lsr(void)
{
	fprintf(stderr, "m6510_lsr not implemented\n");
}

void
m6510_pha(void)
{
	fprintf(stderr, "m6510_pha not implemented\n");
}

void
m6510_asr(void)
{
	fprintf(stderr, "m6510_asr not implemented\n");
}

void
m6510_jmp(void)
{
	fprintf(stderr, "m6510_jmp not implemented\n");
}

void
m6510_bvc(void)
{
	fprintf(stderr, "m6510_bvc not implemented\n");
}

void
m6510_cli(void)
{
	fprintf(stderr, "m6510_cli not implemented\n");
}

void
m6510_rts(void)
{
	fprintf(stderr, "m6510_rts not implemented\n");
}

void
m6510_adc(void)
{
	fprintf(stderr, "m6510_adc not implemented\n");
}

void
m6510_rra(void)
{
	fprintf(stderr, "m6510_rra not implemented\n");
}

void
m6510_ror(void)
{
	fprintf(stderr, "m6510_ror not implemented\n");
}

void
m6510_pla(void)
{
	fprintf(stderr, "m6510_pla not implemented\n");
}

void
m6510_arr(void)
{
	fprintf(stderr, "m6510_arr not implemented\n");
}

void
m6510_bvs(void)
{
	fprintf(stderr, "m6510_bvs not implemented\n");
}

void
m6510_sei(void)
{
	fprintf(stderr, "m6510_sei not implemented\n");
}

void
m6510_sta(void)
{
	fprintf(stderr, "m6510_sta not implemented\n");
}

void
m6510_sax(void)
{
	fprintf(stderr, "m6510_sax not implemented\n");
}

void
m6510_sty(void)
{
	fprintf(stderr, "m6510_sty not implemented\n");
}

void
m6510_stx(void)
{
	fprintf(stderr, "m6510_stx not implemented\n");
}

void
m6510_dey(void)
{
	fprintf(stderr, "m6510_dey not implemented\n");
}

void
m6510_txa(void)
{
	fprintf(stderr, "m6510_txa not implemented\n");
}

void
m6510_ane(void)
{
	fprintf(stderr, "m6510_ane not implemented\n");
}

void
m6510_bcc(void)
{
	fprintf(stderr, "m6510_bcc not implemented\n");
}

void
m6510_sha(void)
{
	fprintf(stderr, "m6510_sha not implemented\n");
}

void
m6510_tya(void)
{
	fprintf(stderr, "m6510_tya not implemented\n");
}

void
m6510_txs(void)
{
	fprintf(stderr, "m6510_txs not implemented\n");
}

void
m6510_shs(void)
{
	fprintf(stderr, "m6510_shs not implemented\n");
}

void
m6510_shy(void)
{
	fprintf(stderr, "m6510_shy not implemented\n");
}

void
m6510_shx(void)
{
	fprintf(stderr, "m6510_shx not implemented\n");
}

void
m6510_ldy(void)
{
	fprintf(stderr, "m6510_ldy not implemented\n");
}

void
m6510_lda(void)
{
	fprintf(stderr, "m6510_lda not implemented\n");
}

void
m6510_ldx(void)
{
	fprintf(stderr, "m6510_ldx not implemented\n");
}

void
m6510_lax(void)
{
	fprintf(stderr, "m6510_lax not implemented\n");
}

void
m6510_tay(void)
{
	fprintf(stderr, "m6510_tay not implemented\n");
}

void
m6510_tax(void)
{
	fprintf(stderr, "m6510_tax not implemented\n");
}

void
m6510_lxa(void)
{
	fprintf(stderr, "m6510_lxa not implemented\n");
}

void
m6510_bcs(void)
{
	fprintf(stderr, "m6510_bcs not implemented\n");
}

void
m6510_clv(void)
{
	fprintf(stderr, "m6510_clv not implemented\n");
}

void
m6510_tsx(void)
{
	fprintf(stderr, "m6510_tsx not implemented\n");
}

void
m6510_las(void)
{
	fprintf(stderr, "m6510_las not implemented\n");
}

void
m6510_cpy(void)
{
	fprintf(stderr, "m6510_cpy not implemented\n");
}

void
m6510_cmp(void)
{
	fprintf(stderr, "m6510_cmp not implemented\n");
}

void
m6510_dcp(void)
{
	fprintf(stderr, "m6510_dcp not implemented\n");
}

void
m6510_dec(void)
{
	fprintf(stderr, "m6510_dec not implemented\n");
}

void
m6510_iny(void)
{
	fprintf(stderr, "m6510_iny not implemented\n");
}

void
m6510_dex(void)
{
	fprintf(stderr, "m6510_dex not implemented\n");
}

void
m6510_sbx(void)
{
	fprintf(stderr, "m6510_sbx not implemented\n");
}

void
m6510_bne(void)
{
	fprintf(stderr, "m6510_bne not implemented\n");
}

void
m6510_cld(void)
{
	fprintf(stderr, "m6510_cld not implemented\n");
}

void
m6510_cpx(void)
{
	fprintf(stderr, "m6510_cpx not implemented\n");
}

void
m6510_sbc(void)
{
	fprintf(stderr, "m6510_sbc not implemented\n");
}

void
m6510_isb(void)
{
	fprintf(stderr, "m6510_isb not implemented\n");
}

void
m6510_inc(void)
{
	fprintf(stderr, "m6510_inc not implemented\n");
}

void
m6510_inx(void)
{
	fprintf(stderr, "m6510_inx not implemented\n");
}

void
m6510_beq(void)
{
	fprintf(stderr, "m6510_beq not implemented\n");
}

void
m6510_sed(void)
{
	fprintf(stderr, "m6510_sed not implemented\n");
}
